#include <stdlib.h>

#include "ftl.h"

extern int secno_num_per_page, secno_num_sub_page;
/******************************************************************************************下面是ftl层map操作******************************************************************************************/

/*****************************************************************************
*The function is based on the parameters channel, chip, die, plane, block, page,
*find the physical page number
******************************************************************************/
unsigned int find_pun(struct ssd_info* ssd, unsigned int channel, unsigned int chip, unsigned int die, unsigned int plane, unsigned int block, unsigned int page, unsigned int unit)
{
	unsigned int ppn = 0, pun = 0;
	unsigned int i = 0;
	int page_plane = 0, page_die = 0, page_chip = 0;
	int page_channel[100];

#ifdef DEBUG
	printf("enter find_psn,channel:%d, chip:%d, die:%d, plane:%d, block:%d, page:%d\n", channel, chip, die, plane, block, page);
#endif

	/***************************************************************
	*Calculate the number of pages in plane, die, chip, and channel
	****************************************************************/
	page_plane = ssd->parameter->page_block * ssd->parameter->block_plane;
	page_die = page_plane * ssd->parameter->plane_die;
	page_chip = page_die * ssd->parameter->die_chip;

	while (i < ssd->parameter->channel_number)
	{
		page_channel[i] = ssd->parameter->chip_channel[i] * page_chip;
		i++;
	}

	i = 0;
	while (i < channel)
	{
		ppn = ppn + page_channel[i];
		i++;
	}
	ppn = ppn + page_chip * chip + page_die * die + page_plane * plane + block * ssd->parameter->page_block + page;
	pun = ppn * ssd->parameter->subpage_page + unit;
	return pun;
}


/*****************************************************************************
*The function is based on the parameters channel, chip, die, plane, block, page, 
*find the physical page number
******************************************************************************/
unsigned int find_ppn(struct ssd_info * ssd, unsigned int channel, unsigned int chip, unsigned int die, unsigned int plane, unsigned int block, unsigned int page)
{
	unsigned int ppn = 0;
	unsigned int i = 0;
	int page_plane = 0, page_die = 0, page_chip = 0;
	int page_channel[100];                 

#ifdef DEBUG
	printf("enter find_psn,channel:%d, chip:%d, die:%d, plane:%d, block:%d, page:%d\n", channel, chip, die, plane, block, page);
#endif

	/***************************************************************
	*Calculate the number of pages in plane, die, chip, and channel
	****************************************************************/
	page_plane = ssd->parameter->page_block*ssd->parameter->block_plane;
	page_die = page_plane*ssd->parameter->plane_die;
	page_chip = page_die*ssd->parameter->die_chip;
	while (i<ssd->parameter->channel_number)
	{
		page_channel[i] = ssd->parameter->chip_channel[i] * page_chip;
		i++;
	}

	/****************************************************************************
	*Calculate the physical page number ppn, ppn is the sum of the number of pages 
	*in channel, chip, die, plane, block, page
	*****************************************************************************/
	i = 0;
	while (i<channel)
	{
		ppn = ppn + page_channel[i];
		i++;
	}
	ppn = ppn + page_chip*chip + page_die*die + page_plane*plane + block*ssd->parameter->page_block + page;

	return ppn;
}


/************************************************************************************
*function is based on the physical page number ppn find the physical page where the 
*channel, chip, die, plane, block,In the structure location and as a return value
*************************************************************************************/
struct local *find_location(struct ssd_info *ssd, unsigned int pun)
{
	struct local *location = NULL;
	unsigned int ppn;
	int page_plane = 0, page_die = 0, page_chip = 0, page_channel = 0;

	ppn = pun/ssd->parameter->subpage_page;

#ifdef DEBUG
	printf("enter find_location\n");
#endif

	location = (struct local *)malloc(sizeof(struct local));
	alloc_assert(location, "location");
	memset(location, 0, sizeof(struct local));

	page_plane = ssd->parameter->page_block*ssd->parameter->block_plane;
	page_die = page_plane*ssd->parameter->plane_die;
	page_chip = page_die*ssd->parameter->die_chip;
	page_channel = page_chip*ssd->parameter->chip_channel[0];

	location->channel = ppn / page_channel;
	location->chip = (ppn%page_channel) / page_chip;
	location->die = ((ppn%page_channel) % page_chip) / page_die;
	location->plane = (((ppn%page_channel) % page_chip) % page_die) / page_plane;
	location->block = ((((ppn%page_channel) % page_chip) % page_die) % page_plane) / ssd->parameter->page_block;
	location->page = (((((ppn%page_channel) % page_chip) % page_die) % page_plane) % ssd->parameter->page_block) % ssd->parameter->page_block;
	location->sub_page = pun % ssd->parameter->subpage_page;
	return location;
}

struct local* find_location_pun(struct ssd_info* ssd, unsigned int pun)
{
	struct local* location = NULL;
	int unit_page, unit_block, unit_plane, unit_die, unit_chip, unit_channel;

	unit_page  = ssd->parameter->subpage_page;
	unit_block = unit_page * ssd->parameter->page_block;
	unit_plane = unit_block * ssd->parameter->block_plane;
	unit_die   = unit_plane * ssd->parameter->plane_die;
	unit_chip = unit_die * ssd->parameter->die_chip;
	unit_channel = unit_chip * ssd->parameter->chip_channel[0];  // work under the condition where all channels are armed with the same number of chips

#ifdef DEBUG
	printf("enter find_location\n");
#endif

	location = (struct local*)malloc(sizeof(struct local));
	alloc_assert(location, "location");
	memset(location, 0, sizeof(struct local));


	location->channel = pun / unit_channel;
	location->chip = (pun % unit_channel) / unit_chip;
	location->die = ((pun % unit_channel) % unit_chip) / unit_die;
	location->plane = (((pun % unit_channel) % unit_chip) % unit_die) / unit_plane;
	location->block = ((((pun % unit_channel) % unit_chip) % unit_die) % unit_plane) / unit_block;
	location->page = (((((pun % unit_channel) % unit_chip) % unit_die) % unit_plane) % unit_block) / unit_page;
	location->sub_page = pun % unit_page;
	return location;
}

//each time write operation is carried out, judge whether superblock-garbage is carried out
Status SuperBlock_GC(struct ssd_info *ssd, struct request *req)
{
	unsigned int sb_no;
	//find victim garbage superblock
	sb_no = find_victim_superblock(ssd);
	
	if (migration_horizon(ssd, req, sb_no) == ERROR)
	{
		printf("GC migration Error!\n");
		getchar();
	}
	ssd->free_sb_cnt++;
	return SUCCESS;
}

//migrate data 
Status migration_horizon(struct ssd_info *ssd, struct request * req, unsigned int victim)
{
	int i,j;
	unsigned int chan, chip, die, plane, block, page,unit;
	unsigned int blk_type;

	unsigned int read_cnt;
	unsigned int transer = 0;
	unsigned int lpn, state;
	unsigned int time;

	block = ssd->sb_pool[victim].pos[0].block;
	blk_type = ssd->channel_head[0].chip_head[0].die_head[0].plane_head[0].blk_head[block].block_type;

	for (page = 0; page < ssd->parameter->page_block; page++)
	{
		read_cnt = IS_superpage_valid(ssd, victim, page);
		if (read_cnt > 0)
		{
			for (chan = 0; chan < ssd->parameter->channel_number; chan++)
			{
				for (chip = 0; chip < ssd->parameter->chip_channel[chan]; chip++)
				{
					ssd->channel_head[chan].chip_head[chip].next_state_predict_time += ssd->parameter->time_characteristics.tR;
					transer = 0;
					for (die = 0; die < ssd->parameter->die_chip; die++)
					{
						for (plane = 0; plane < ssd->parameter->plane_die; plane++)
						{
							for (unit = 0; unit < ssd->parameter->subpage_page; unit++)
							{
								if (ssd->channel_head[chan].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[page].lun_state[unit] > 0)
								{
									transer++;

									lpn = ssd->channel_head[chan].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[page].luns[unit];
									state = ssd->channel_head[chan].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[page].lun_state[unit];
									//size = secno_num_sub_page;

									if (blk_type == USER_DATA)
									{
										//set mapping table invalid
										ssd->dram->map->map_entry[lpn].state = 0;
										ssd->dram->map->map_entry[lpn].pn = INVALID_PPN;
										insert2_command_buffer(ssd, ssd->dram->data_command_buffer, lpn, state, req, USER_DATA);
									}
									if (blk_type == MAPPING_DATA)
									{
										//insert2_mapping_command_buffer_in_order(ssd, lpn, req);  // this is for lsm tree 
										ssd->dram->tran_map->map_entry[lpn].state = 0;
										ssd->dram->tran_map->map_entry[lpn].pn = INVALID_PPN;
										insert2_command_buffer(ssd, ssd->dram->mapping_command_buffer, lpn, state, req, MAPPING_DATA);
									}
								}
							}
						}
					}
					//transfer req to buffer and hand the request
					if (transer > 0)
					{
						time = ssd->channel_head[chan].chip_head[chip].next_state_predict_time + transer * ssd->parameter->subpage_capacity * ssd->parameter->time_characteristics.tRC;
						ssd->channel_head[chan].next_state_predict_time = (ssd->channel_head[chan].next_state_predict_time > time) ? ssd->channel_head[chan].next_state_predict_time : time;
					}
			
				}
			}
		}
	}
	for ( i = 0; i < ssd->sb_pool[victim].blk_cnt; i++)
	{
		chan = ssd->sb_pool[victim].pos[i].channel;
		chip = ssd->sb_pool[victim].pos[i].chip;
		die = ssd->sb_pool[victim].pos[i].die;
		plane = ssd->sb_pool[victim].pos[i].plane;
		block = ssd->sb_pool[victim].pos[i].block;
		ssd->channel_head[chan].current_state = CHANNEL_GC;
		ssd->channel_head[chan].next_state = CHANNEL_IDLE;
		ssd->channel_head[chan].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].block_type = -1;
		erase_operation(ssd, chan, chip, die, plane, block);
	}
	for (i = 0; i < ssd->parameter->channel_number; i++)
	{
		for (j = 0; j < ssd->parameter->chip_channel[i]; j++)
		{
			ssd->channel_head[i].chip_head[j].next_state_predict_time += ssd->parameter->time_characteristics.tBERS;
		}
	}

	ssd->sb_pool[victim].next_wr_page = 0;
	ssd->sb_pool[victim].pg_off = -1;
	ssd->sb_pool[victim].ec++;
	ssd->sb_pool[victim].blk_type = -1;
	return SUCCESS;
}

Status IS_superpage_valid(struct ssd_info *ssd, unsigned int sb_no, unsigned int page)
{
	unsigned int chan, chip, die, plane, block;
	unsigned i,j;
	unsigned valid_cnt=0;

	for (i = 0; i < ssd->sb_pool[sb_no].blk_cnt; i++)
	{
		chan = ssd->sb_pool[sb_no].pos[i].channel;
		chip = ssd->sb_pool[sb_no].pos[i].chip;
		die = ssd->sb_pool[sb_no].pos[i].die;
		plane = ssd->sb_pool[sb_no].pos[i].plane;
		block = ssd->sb_pool[sb_no].pos[i].block;
		
		for (j = 0; j < ssd->parameter->subpage_page; j++)
		{
			if (ssd->channel_head[chan].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[page].lun_state[j] > 0)
			{
				valid_cnt++;
				break;
			}
		}
	}
	return valid_cnt;
}


//migrate data 
Status migration(struct ssd_info *ssd,unsigned int victim)
{
#if 0
	int i;
	unsigned int chan, chip, die, plane, block,page;
	unsigned int move_page_cnt;

	struct sub_request *gc_req = NULL,*sub_w = NULL;
	for (i = 0; i < ssd->sb_pool[victim].blk_cnt; i++)
	{ 
		move_page_cnt = 0;
		chan  = ssd->sb_pool[victim].pos[i].channel;
		chip  = ssd->sb_pool[victim].pos[i].chip;
		die   = ssd->sb_pool[victim].pos[i].die;
		plane = ssd->sb_pool[victim].pos[i].plane;
		block = ssd->sb_pool[victim].pos[i].block;
		for (page = 0; page < ssd->parameter->page_block; page++)
		{ 
			if (ssd->channel_head[chan].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[page].valid_state > 0)
			{
				move_page_cnt++;
				ssd->gc_read_count++;
				//read data  becuase the migration is doing right after write operation is done 
				ssd->channel_head[chan].chip_head[chip].next_state_predict_time += ssd->parameter->time_characteristics.tR;

				//creat a gc write request 
				// create sub request and add to command buffer
				gc_req = (struct sub_request *)malloc(sizeof(struct sub_request));
				alloc_assert(gc_req, "sub_request");
				memset(gc_req, 0, sizeof(struct sub_request));
				if (gc_req == NULL)
				{
					printf("error! can't appply for memory space for gc migration request\n");
					getchar();
				}
				gc_req->operation = WRITE;
				gc_req->next_node = NULL;
				gc_req->next_subs = NULL;
				//gc_req->update = NULL;
				gc_req->gc_flag = 1;

				gc_req->location = (struct local*)malloc(sizeof(struct local));
				memset(gc_req->location, 0, sizeof(struct local));
				gc_req->current_state = SR_WAIT;
				gc_req->current_time = ssd->channel_head[chan].chip_head[chip].next_state_predict_time;
				gc_req->lpn = ssd->channel_head[chan].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[page].lpn;
				gc_req->size = size(ssd->channel_head[chan].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[page].valid_state);
				gc_req->state = ssd->channel_head[chan].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[page].valid_state;
				gc_req->begin_time = gc_req->current_time;


				//set mapping table invalid 
				ssd->dram->map->map_entry[gc_req->lpn].state = 0;
				ssd->dram->map->map_entry[gc_req->lpn].pn = INVALID_PPN;

				//apply for free super page
				if (ssd->open_sb[0]->next_wr_page >= ssd->parameter->page_block) //no free superpage in the superblock
					find_active_superblock(ssd,0);



				//allocate free page 
				ssd->open_sb[0]->pg_off = (ssd->open_sb[0]->pg_off + 1) % ssd->sb_pool[0].blk_cnt;
				gc_req->location->channel = ssd->open_sb[0]->pos[ssd->open_sb[0]->pg_off].channel;
				gc_req->location->chip = ssd->open_sb[0]->pos[ssd->open_sb[0]->pg_off].chip;
				gc_req->location->die = ssd->open_sb[0]->pos[ssd->open_sb[0]->pg_off].die;
				gc_req->location->plane = ssd->open_sb[0]->pos[ssd->open_sb[0]->pg_off].plane;
				gc_req->location->block = ssd->open_sb[0]->pos[ssd->open_sb[0]->pg_off].block;
				gc_req->location->page = ssd->open_sb[0]->next_wr_page;
				gc_req->ppn = find_ppn(ssd, gc_req->location->channel, gc_req->location->chip, gc_req->location->die, gc_req->location->plane, gc_req->location->block, gc_req->location->page);
				if (ssd->open_sb[0]->pg_off == ssd->sb_pool[0].blk_cnt - 1)
					ssd->open_sb[0]->next_wr_page++;

				//printf("GC: channel = %d, chip = %d, die = %d, plane = %d, block = %d, page = %d\n", gc_req->location->channel, gc_req->location->chip, gc_req->location->die, gc_req->location->plane, gc_req->location->block, gc_req->location->page);

				//transfer req to buffer and hand the request
				ssd->channel_head[chan].next_state_predict_time = gc_req->current_time + gc_req->size*SECTOR*ssd->parameter->time_characteristics.tRC;
				while (sub_w != NULL)
				{
					if (sub_w->ppn == gc_req->ppn)  //no possibility to write into the same physical position
					{
						printf("error: write into the same physical address\n");
						getchar();
					}
					sub_w = sub_w->next_node;
				}

				if (ssd->channel_head[gc_req->location->channel].subs_w_tail != NULL)
				{
					ssd->channel_head[gc_req->location->channel].subs_w_tail->next_node = gc_req;
					ssd->channel_head[gc_req->location->channel].subs_w_tail = gc_req;
				}
				else
				{
					ssd->channel_head[gc_req->location->channel].subs_w_head = gc_req;
					ssd->channel_head[gc_req->location->channel].subs_w_tail = gc_req;
				}

			}
		}
		ssd->channel_head[chan].current_state = CHANNEL_GC;
		ssd->channel_head[chan].next_state = CHANNEL_IDLE;
		erase_operation(ssd, chan, chip, die, plane, block);
	}
	ssd->sb_pool[victim].next_wr_page = 0;
	ssd->sb_pool[victim].pg_off = -1;
	ssd->sb_pool[victim].ec++;
#endif
	return SUCCESS;
}

//return the garbage superblock with the maximal invalid data 
int find_victim_superblock(struct ssd_info *ssd)
{
	unsigned int sb_no = 0;
	unsigned int max_sb_cnt = 0;
    
	unsigned int sb_invalid;
	int i;
	for (i = 0; i < ssd->sb_cnt; i++)
	{
		if (Is_Garbage_SBlk(ssd,i)==FALSE)
			continue;
		//sb_ec = Get_SB_PE(ssd, i);
		sb_invalid = Get_SB_Invalid(ssd, i);
		if (sb_invalid > max_sb_cnt)
		{
			max_sb_cnt = sb_invalid;
			sb_no = i;
		}
	}	
	if (max_sb_cnt < ssd->open_sb[0]->blk_cnt * ssd->parameter->page_block * 0.1)
	{
		printf("Look Here\n");
	}
	return sb_no;
}

//judge whether block is garbage block 
Status Is_Garbage_SBlk(struct ssd_info *ssd, int sb_no)
{
	unsigned int channel, chip, die, plane, block;
	int i;
	for (i = 0; i < ssd->sb_pool[sb_no].blk_cnt; i++)
	{
		channel = ssd->sb_pool[sb_no].pos[i].channel;
		chip = ssd->sb_pool[sb_no].pos[i].chip;
		die = ssd->sb_pool[sb_no].pos[i].die;
		plane = ssd->sb_pool[sb_no].pos[i].plane;
		block = ssd->sb_pool[sb_no].pos[i].block;
		if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].last_write_page != ssd->parameter->page_block-1)
		{
			return FAILURE;
		}
	}
	return SUCCESS;
}

//return superblock invalid data count 
int Get_SB_Invalid(struct ssd_info *ssd, unsigned int sb_no)
{
	unsigned int i, chan, chip, die, plane, block;
	unsigned int sum_invalid = 0;
	for (i = 0; i < ssd->sb_pool[sb_no].blk_cnt; i++)
	{
		chan = ssd->sb_pool[sb_no].pos[i].channel;
		chip = ssd->sb_pool[sb_no].pos[i].chip;
		die = ssd->sb_pool[sb_no].pos[i].die;
		plane = ssd->sb_pool[sb_no].pos[i].plane;
		block = ssd->sb_pool[sb_no].pos[i].block;
		sum_invalid += ssd->channel_head[chan].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].invalid_subpage_num;
	}
	return sum_invalid;
}


//return superblock pe 
int Get_SB_PE(struct ssd_info *ssd, unsigned int sb_no)
{
	unsigned int chan,chip,die,plane,block;

	chan   = ssd->sb_pool[sb_no].pos[0].channel;
	chip   = ssd->sb_pool[sb_no].pos[0].chip;
	die    = ssd->sb_pool[sb_no].pos[0].die;
	plane  = ssd->sb_pool[sb_no].pos[0].plane;
	block  = ssd->sb_pool[sb_no].pos[0].block;
	return ssd->channel_head[chan].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].erase_count;
}
/* 
    find superblock block with the minimal P/E cycles
*/
Status find_active_superblock(struct ssd_info *ssd,struct request *req, unsigned int type)
{
	int i;
	int min_ec = 9999999;
	int min_sb = -1;
	unsigned int channel, chip, die, plane, block;

	for (i = 0; i < ssd->sb_cnt; i++)
	{
		if (ssd->sb_pool[i].next_wr_page == 0 && ssd->sb_pool[i].pg_off == -1&& ssd->sb_pool[i].blk_type==-1) //free superblock
		{
			if (ssd->sb_pool[i].ec < min_ec)
			{
				min_sb = i;
				min_ec = ssd->sb_pool[i].ec;
			}
		}
	}

	if (min_sb == -1)
	{
		printf("No Free Blocks\n");
		return FALSE;
	}
	//set open superblock 
	ssd->open_sb[type] = ssd->sb_pool + min_sb;
	ssd->open_sb[type]->blk_type = type;
	for (i = 0; i < ssd->open_sb[type]->blk_cnt; i++)
	{
		channel = ssd->open_sb[type]->pos[i].channel;
		chip = ssd->open_sb[type]->pos[i].chip;
		die = ssd->open_sb[type]->pos[i].die;
		plane = ssd->open_sb[type]->pos[i].plane;
		block = ssd->open_sb[type]->pos[i].block;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].block_type = type;
	}

	ssd->free_sb_cnt--;

	/*
	judge whether GC is triggered
	note need to reduce the influence of mixture of GC data and user data
    */
	while (ssd->free_sb_cnt <= MIN_SB_RATE * ssd->sb_cnt && !ssd->gc_flag)
	{
		ssd->gc_flag = true;
		SuperBlock_GC(ssd, req);
		ssd->gc_flag = false;
	}

	return SUCCESS;
}


/**************************************************************************************
*Function function is to find active fast, there should be only one active block for 
*each plane, only the active block in order to operate
***************************************************************************************/
Status  find_active_block(struct ssd_info *ssd, unsigned int channel, unsigned int chip, unsigned int die, unsigned int plane)
{
	unsigned int active_block = 0;
	unsigned int free_page_num = 0;
	unsigned int count = 0;
	//	int i, j, k, p, t;

	active_block = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].active_block;
	free_page_num = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num;
	//last_write_page=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num;
	while ((free_page_num == 0) && (count<ssd->parameter->block_plane))
	{
		active_block = (active_block + 1) % ssd->parameter->block_plane;
		free_page_num = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num;
		count++;
	}

	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].active_block = active_block;

	if (count<ssd->parameter->block_plane)
	{
		return SUCCESS;
	}
	else
	{
		return FAILURE;
	}
}


