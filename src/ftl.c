#include <stdlib.h>
#include <crtdbg.h>

#include "initialize.h"
#include "ssd.h"
#include "flash.h"
#include "buffer.h"
#include "interface.h"
#include "ftl.h"
#include "fcl.h"

extern int secno_num_per_page, secno_num_sub_page;
/******************************************************************************************下面是ftl层map操作******************************************************************************************/

/***************************************************************************************************
*function is given in the channel, chip, die, plane inside find an active_block and then find a page 
*inside the block, and then use find_ppn find ppn
****************************************************************************************************/
struct ssd_info *get_ppn(struct ssd_info *ssd, unsigned int channel, unsigned int chip, unsigned int die, unsigned int plane, struct sub_request *sub)
{
	return ssd;
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
struct local *find_location(struct ssd_info *ssd, unsigned int ppn)
{
	struct local *location = NULL;
	unsigned int i = 0;
	int pn, ppn_value = ppn;
	int page_plane = 0, page_die = 0, page_chip = 0, page_channel = 0;

	pn = ppn;

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

	return location;
}

/*******************************************************************
*When executing a write request, get ppn for a normal write request
*********************************************************************/
Status get_ppn_for_normal_command(struct ssd_info * ssd, unsigned int channel, unsigned int chip, struct sub_request * sub)
{
	unsigned int die, plane,block,page;
	unsigned int lpn;
	unsigned int ppn;
	struct local *location = NULL;
	unsigned int blk_type;

	if (sub == NULL)
	{
		return ERROR;
	}
	if (ssd->parameter->allocation_scheme == DYNAMIC_ALLOCATION || ssd->parameter->allocation_scheme == HYBRID_ALLOCATION)
	{

	}
	else if (ssd->parameter->allocation_scheme == STATIC_ALLOCATION)
	{

	}
	else if (ssd->parameter->allocation_scheme == SUPERBLOCK_ALLOCATION)
	{
		
		//write the data 
		if (NAND_program(ssd, sub) == FAILURE)
		{
			printf("Error! Program Failure!\n");
			getchar();
		}
		die = sub->location->die;
		plane = sub->location->plane;
		lpn   = sub->lpn;
		block = sub->location->block;
		page  = sub->location->page;
	
		blk_type = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].block_type;

		if (blk_type == DATA_BLK)
		{
			if (ssd->dram->map->map_entry[lpn].state == 0)                                       /*this is the first logical page*/
			{
				ssd->dram->map->map_entry[lpn].pn = find_ppn(ssd, channel, chip, die, plane, block, page);
				ssd->dram->map->map_entry[lpn].state = sub->state;
			}
			else                                                                            /*This logical page has been updated, and the original page needs to be invalidated*/
			{
				ppn = ssd->dram->map->map_entry[lpn].pn;
				location = find_location(ssd, ppn);
				if (ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].lpn != lpn)
				{
					printf("\nError in get_ppn()\n");
					getchar();
				}

				ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].valid_state = 0;
				ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].free_state = 0;
				//ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].lpn = -1;
				ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].invalid_page_num++;

				free(location);
				location = NULL;
				ssd->dram->map->map_entry[lpn].pn = find_ppn(ssd, channel, chip, die, plane, block, page);
				ssd->dram->map->map_entry[lpn].state = (ssd->dram->map->map_entry[lpn].state | sub->state);
			}
			sub->ppn = ssd->dram->map->map_entry[lpn].pn;
		}
		if (blk_type == TRAN_BLK)
		{
			if (ssd->dram->tran_map->map_entry[lpn].state == 0)                                       /*this is the first logical page*/
			{
				ssd->dram->tran_map->map_entry[lpn].pn = find_ppn(ssd, channel, chip, die, plane, block, page);
				ssd->dram->tran_map->map_entry[lpn].state = sub->state;
			}
			else                                                                            /*This logical page has been updated, and the original page needs to be invalidated*/
			{
				ppn = ssd->dram->tran_map->map_entry[lpn].pn;
				location = find_location(ssd, ppn);
				if (ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].lpn != lpn)
				{
					printf("\nError in get_ppn()\n");
					getchar();
				}

				ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].valid_state = 0;
				ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].free_state = 0;
				//ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].lpn = -1;
				ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].invalid_page_num++;

				free(location);
				location = NULL;
				ssd->dram->tran_map->map_entry[lpn].pn = find_ppn(ssd, channel, chip, die, plane, block, page);
				ssd->dram->tran_map->map_entry[lpn].state = (ssd->dram->map->map_entry[lpn].state | sub->state);
			}
			sub->ppn = ssd->dram->tran_map->map_entry[lpn].pn;
		}

        /*Modify the sub number request ppn, location and other variables*/

		if (size(sub->state) > ssd->parameter->subpage_page)
		{
			getchar();
		}

		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_write_count++;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_page--;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[page].lpn = lpn;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[page].valid_state = sub->state;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[page].free_state = ssd->dram->map->map_entry[lpn].state;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[page].written_count++;
		
		//compute servew time
		compute_serve_time(ssd, channel, chip, die, &sub, 1, NORMAL);

		//judge whether GC is triggered 
		if (ssd->free_sb_cnt <= MIN_SB_RATE*ssd->sb_cnt)
		{
			SuperBlock_GC(ssd);
		}
	}

	return SUCCESS;
}

/************************************************************************************************
*Write a request for an advanced command to get ppn
*According to different orders, in accordance with the same block in the order to write the request, 
*select the write can be done ppn, skip the ppn all set to invaild
*
*In the use of two plane operation, in order to find the same level of the page, you may need to 
*directly find two completely blank block, this time the original block is not used up, can only be 
*placed on this, waiting for the next use, while modifying the search blank Page method, will be the 
*first to find free block to change, as long as the invalid block! = 64 can.
*
*except find aim page, we should modify token and decide gc operation
*************************************************************************************************/
Status get_ppn_for_advanced_commands(struct ssd_info *ssd, unsigned int channel, unsigned int chip, struct sub_request ** subs, unsigned int subs_count, unsigned int command)
{
	unsigned int aim_die = 0, aim_plane = 0, aim_count = 0;
	unsigned int die_token = 0, plane_token = 0;
	unsigned int i = 0, j = 0, k = 0;
	unsigned int valid_subs_count = 0;
	unsigned int state ;

	struct sub_request * sub = NULL;
	struct sub_request ** mutli_subs = NULL;

	if (ssd->parameter->allocation_scheme == DYNAMIC_ALLOCATION || ssd->parameter->allocation_scheme == HYBRID_ALLOCATION)  //动态分配的目标die plane由动态令牌来决定
	{
		aim_die = ssd->channel_head[channel].chip_head[chip].token;
		aim_plane = ssd->channel_head[channel].chip_head[chip].die_head[aim_die].token;
	}
	else if (ssd->parameter->allocation_scheme == STATIC_ALLOCATION)
	{
		aim_die = subs[0]->location->die;            //静态分配只能找到对应的die
		//验证subs的有效性
		for (i = 0; i < subs_count; i++)
		{
			if (subs[i]->location->die != aim_die)
			{
				printf("Error ,aim_die match failed\n");
				getchar();
			}
		}
	}

	//如果是one shot mutli plane的情况，这里就要分superpage还是mutli plane优先
	if (command == ONE_SHOT_MUTLI_PLANE)
	{
		mutli_subs = (struct sub_request **)malloc(ssd->parameter->plane_die * sizeof(struct sub_request *));
		//plane>superpage:024/135
		if (ssd->parameter->static_allocation == PLANE_STATIC_ALLOCATION || ssd->parameter->static_allocation == CHANNEL_PLANE_STATIC_ALLOCATION )
		{
			for (i = 0; i < PAGE_INDEX; i++)
			{
				for (j = 0; j < ssd->parameter->plane_die; j++)
				{
					if (i + k > subs_count)
					{
						printf("subs_count distribute error\n");
						getchar();
					}
					mutli_subs[j] = subs[j + k];
				}
				//进行mutli plane的操作
				find_level_page(ssd, channel, chip, aim_die, mutli_subs, ssd->parameter->plane_die);
				k = k + ssd->parameter->plane_die;
			}
		}//superpage>plane;012/345
		else if (ssd->parameter->static_allocation == SUPERPAGE_STATIC_ALLOCATION || ssd->parameter->static_allocation == CHANNEL_SUPERPAGE_STATIC_ALLOCATION)
		{
			for (i = 0; i < PAGE_INDEX; i++)
			{
				k = 0;
				for (j = 0; j < ssd->parameter->plane_die; j++)
				{
					if (i + k > subs_count)
					{
						printf("subs_count distribute error\n");
						getchar();
					}
					mutli_subs[j] = subs[i + k];
					k = k + PAGE_INDEX;
				}
				//进行mutli plane的操作
				find_level_page(ssd, channel, chip, aim_die, mutli_subs, ssd->parameter->plane_die);
			}
		}

		valid_subs_count = subs_count;
		compute_serve_time(ssd, channel, chip, aim_die, subs, valid_subs_count, ONE_SHOT_MUTLI_PLANE);
		//printf("lz:mutli plane one shot\n");

		if (ssd->parameter->allocation_scheme == DYNAMIC_ALLOCATION || ssd->parameter->allocation_scheme == HYBRID_ALLOCATION)
			ssd->channel_head[channel].chip_head[chip].token = (aim_die + 1) % ssd->parameter->die_chip;

		//free mutli_subs
		for (i = 0; i < ssd->parameter->plane_die; i++)
			mutli_subs[i] = NULL;
		free(mutli_subs);
		mutli_subs = NULL;
		return SUCCESS;
	}
	else if (command == MUTLI_PLANE)
	{
		if (subs_count == ssd->parameter->plane_die)
		{
			state = find_level_page(ssd, channel, chip, aim_die, subs, subs_count);
			if (state != SUCCESS)
			{
				get_ppn_for_normal_command(ssd, channel, chip, subs[0]);		 
				printf("find_level_page failed, begin to one page program\n");
				getchar();
				return FAILURE;
			}
			else
			{
				valid_subs_count = ssd->parameter->plane_die;
				compute_serve_time(ssd, channel, chip, aim_die, subs, valid_subs_count, MUTLI_PLANE);

				if (ssd->parameter->allocation_scheme == DYNAMIC_ALLOCATION || ssd->parameter->allocation_scheme == HYBRID_ALLOCATION)
					ssd->channel_head[channel].chip_head[chip].token = (aim_die + 1) % ssd->parameter->die_chip;
				
				//printf("lz:mutli_plane\n");
				return SUCCESS;
			}
		}
		else
		{
			return ERROR;
		}
	}
	else if (command == ONE_SHOT)
	{
		for (i = 0; i < subs_count; i++)
			get_ppn(ssd, channel, chip, aim_die, aim_plane, subs[i]);

		valid_subs_count = PAGE_INDEX;
		compute_serve_time(ssd, channel, chip, aim_die, subs, valid_subs_count, ONE_SHOT);

		//更新plane die
		if (ssd->parameter->allocation_scheme == DYNAMIC_ALLOCATION || ssd->parameter->allocation_scheme == HYBRID_ALLOCATION)
		{
			ssd->channel_head[channel].chip_head[chip].die_head[aim_die].token = (aim_plane + 1) % ssd->parameter->plane_die;
			if (aim_plane == (ssd->parameter->plane_die - 1))
				ssd->channel_head[channel].chip_head[chip].token = (aim_die + 1) % ssd->parameter->die_chip;
		}

		//printf("lz:one shot\n");
		return SUCCESS;
	}
	else
	{
		return ERROR;
	}
}


void gc_check(struct ssd_info *ssd, unsigned int channel, unsigned int chip, unsigned int die, unsigned int old_plane)
{

}


unsigned int gc(struct ssd_info *ssd, unsigned int channel, unsigned int flag)
{
	return FAILURE;
}

//each time write operation is carried out, judge whether superblock-garbage is carried out
Status SuperBlock_GC(struct ssd_info *ssd)
{
	unsigned int sb_no;
	//find victim garbage superblock
	sb_no = find_victim_superblock(ssd);
	
/*	if (VERTICAL_DATA_DISTRIBUTION)
	{
		//migration and erase  
		if (migration(ssd, sb_no) == ERROR)
		{
			printf("GC migration Error!\n");
			getchar();
		}
	}
	else
	{
		if (migration_horizon(ssd, sb_no) == ERROR)
		{
			printf("GC migration Error!\n");
			getchar();
		}
	}*/

	if (migration_horizon(ssd, sb_no) == ERROR)
	{
		printf("GC migration Error!\n");
		getchar();
	}
	ssd->free_sb_cnt++;
	return SUCCESS;
}


//migrate data 
Status migration_horizon(struct ssd_info *ssd, unsigned int victim)
{
	int i,j;
	unsigned int chan, chip, die, plane, block, page;
	unsigned int blk_type;

	struct sub_request *gc_req = NULL, *sub_w = NULL;
	unsigned int read_cnt;
	unsigned int transer = 0;

	block = ssd->sb_pool[victim].pos[0].block;
	blk_type = ssd->channel_head[0].chip_head[0].die_head[0].plane_head[0].blk_head[block].block_type;
	if (blk_type == TRAN_BLK)
	{
		printf("Look Here\n");
	}

	for (page = 0; page < ssd->parameter->page_block; page++)
	{
		read_cnt = IS_superpage_valid(ssd, victim, page);
		if (read_cnt>0)
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
							if (ssd->channel_head[chan].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[page].valid_state > 0)
							{
								transer++;
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
								gc_req->location = (struct local*)malloc(sizeof(struct local));
								memset(gc_req->location, 0, sizeof(struct local));

								gc_req->operation = WRITE;
								gc_req->next_node = NULL;
								gc_req->next_subs = NULL;
								//gc_req->update = NULL;
								gc_req->gc_flag = 1;

								gc_req->current_state = SR_WAIT;
								gc_req->current_time = ssd->channel_head[chan].chip_head[chip].next_state_predict_time;
								gc_req->lpn = ssd->channel_head[chan].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[page].lpn;
								gc_req->size = size(ssd->channel_head[chan].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[page].valid_state);
								transer += gc_req->size;
								gc_req->state = ssd->channel_head[chan].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[page].valid_state;
								gc_req->begin_time = gc_req->current_time;

								
								//set mapping table invalid
								if (blk_type == TRAN_BLK)
								{
									ssd->dram->tran_map->map_entry[gc_req->lpn].state = 0;
									ssd->dram->tran_map->map_entry[gc_req->lpn].pn = INVALID_PPN;
								}
								else
								{
									ssd->dram->map->map_entry[gc_req->lpn].state = 0;
									ssd->dram->map->map_entry[gc_req->lpn].pn = INVALID_PPN;
								}
								//apply for free super page
								if (ssd->open_sb[blk_type]->next_wr_page >= ssd->parameter->page_block) //no free superpage in the superblock
									find_active_superblock(ssd, blk_type);

								//allocate free page 
								ssd->open_sb[blk_type]->pg_off = (ssd->open_sb[blk_type]->pg_off + 1) % ssd->sb_pool[0].blk_cnt;
								gc_req->location->channel = ssd->open_sb[blk_type]->pos[ssd->open_sb[blk_type]->pg_off].channel;
								gc_req->location->chip = ssd->open_sb[blk_type]->pos[ssd->open_sb[blk_type]->pg_off].chip;
								gc_req->location->die = ssd->open_sb[blk_type]->pos[ssd->open_sb[blk_type]->pg_off].die;
								gc_req->location->plane = ssd->open_sb[blk_type]->pos[ssd->open_sb[blk_type]->pg_off].plane;
								gc_req->location->block = ssd->open_sb[blk_type]->pos[ssd->open_sb[blk_type]->pg_off].block;
								gc_req->location->page = ssd->open_sb[blk_type]->next_wr_page;
								gc_req->ppn = find_ppn(ssd, gc_req->location->channel, gc_req->location->chip, gc_req->location->die, gc_req->location->plane, gc_req->location->block, gc_req->location->page);
								if (ssd->open_sb[blk_type]->pg_off == ssd->sb_pool[0].blk_cnt - 1)
									ssd->open_sb[blk_type]->next_wr_page++;

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
							ssd->gc_read_count++;
						}
					}
					//transfer req to buffer and hand the request
					if (transer>0)
						ssd->channel_head[chan].next_state_predict_time = gc_req->current_time + transer*SECTOR*ssd->parameter->time_characteristics.tRC;
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
	ssd->sb_pool[victim].next_wr_page = 0;
	ssd->sb_pool[victim].pg_off = -1;
	ssd->sb_pool[victim].ec++;
	ssd->sb_pool[victim].blk_type = -1;
	return SUCCESS;
}

Status IS_superpage_valid(struct ssd_info *ssd, unsigned int sb_no, unsigned int page)
{
	unsigned int chan, chip, die, plane, block;
	unsigned i;
	unsigned valid_cnt=0;

	for (i = 0; i < ssd->sb_pool[sb_no].blk_cnt; i++)
	{
		chan = ssd->sb_pool[sb_no].pos[i].channel;
		chip = ssd->sb_pool[sb_no].pos[i].chip;
		die = ssd->sb_pool[sb_no].pos[i].die;
		plane = ssd->sb_pool[sb_no].pos[i].plane;
		block = ssd->sb_pool[sb_no].pos[i].block;
		if (ssd->channel_head[chan].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[page].valid_state > 0)
			valid_cnt++;
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
		sum_invalid += ssd->channel_head[chan].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].invalid_page_num;
	}
	return sum_invalid;
}


//return superblock pe 
int Get_SB_PE(struct ssd_info *ssd, unsigned int sb_no)
{
	unsigned int i,chan,chip,die,plane,block;
	unsigned int sum_pe = 0;

	chan   = ssd->sb_pool[sb_no].pos[0].channel;
	chip   = ssd->sb_pool[sb_no].pos[0].chip;
	die    = ssd->sb_pool[sb_no].pos[0].die;
	plane  = ssd->sb_pool[sb_no].pos[0].plane;
	block  = ssd->sb_pool[sb_no].pos[0].block;
	return ssd->channel_head[chan].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].erase_count;
}



/*******************************************************************************************************************
*GC operation in a number of plane selected two offset address of the same block to erase, and in the invalid block 
*on the table where the invalid block node, erase success, calculate the mutli plane erase operation of the implementation 
*time, channel chip status Change time
*********************************************************************************************************************/
int gc_direct_erase(struct ssd_info *ssd, unsigned int channel, unsigned int chip, unsigned int die)
{
	unsigned int i,j, plane, block;		
	unsigned int * erase_block;
	struct direct_erase * direct_erase_node = NULL;

	erase_block = (unsigned int*)malloc(ssd->parameter->plane_die * sizeof(erase_block));
	for ( i = 0; i < ssd->parameter->plane_die; i++)
	{
		direct_erase_node = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[i].erase_node;
		if (direct_erase_node == NULL)
		{
			free(erase_block);
			erase_block = NULL;
			return FAILURE;
		}

		//Perform mutli plane erase operation,and delete gc_node
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[i].erase_node = direct_erase_node->next_node;
		erase_block[i] = direct_erase_node->block;

		free(direct_erase_node);
		ssd->direct_erase_count++;
		direct_erase_node = NULL;
	}

	//首先进行channel的跳转，仅是传输命令的时间
	ssd->channel_head[channel].current_state = CHANNEL_TRANSFER;
	ssd->channel_head[channel].current_time = ssd->current_time;
	ssd->channel_head[channel].next_state = CHANNEL_IDLE;
	ssd->channel_head[channel].next_state_predict_time = ssd->current_time + 7 * ssd->parameter->plane_die * ssd->parameter->time_characteristics.tWC;   //14表示的是传输命令的时间，为mutli plane


	//判断是否有suspend命令，有次命令则不能擦除，要挂起擦除操作，等待恢复
	if ((ssd->parameter->advanced_commands&AD_ERASE_SUSPEND_RESUME) == AD_ERASE_SUSPEND_RESUME)
	{
		//1.使用了suspend命令，首先更改chip上的suspend请求状态，用于检测是否有suspend请求到来
		ssd->channel_head[channel].chip_head[chip].gc_signal = SIG_ERASE_WAIT;
		ssd->channel_head[channel].chip_head[chip].erase_begin_time = ssd->channel_head[channel].next_state_predict_time;
		ssd->channel_head[channel].chip_head[chip].erase_cmplt_time = ssd->channel_head[channel].next_state_predict_time + ssd->parameter->time_characteristics.tBERS + ssd->parameter->time_characteristics.tERSL;

		//2.保留擦除操作的现场,产生一个suspend_erase_command请求挂载在chip上
		suspend_erase_operation(ssd, channel, chip, die, erase_block);
	}
	else
	{
		for (j = 0; j < ssd->parameter->plane_die; j++)
		{
			plane = j;
			block = erase_block[j];
			erase_operation(ssd, channel, chip, die, plane, block);
		}

		ssd->mplane_erase_count++;
		ssd->channel_head[channel].chip_head[chip].current_state = CHIP_ERASE_BUSY;
		ssd->channel_head[channel].chip_head[chip].current_time = ssd->current_time;
		ssd->channel_head[channel].chip_head[chip].next_state = CHIP_IDLE;
		ssd->channel_head[channel].chip_head[chip].next_state_predict_time = ssd->channel_head[channel].next_state_predict_time + ssd->parameter->time_characteristics.tBERS;
	}
	free(erase_block);
	erase_block = NULL;
	return SUCCESS;
}

/*******************************************************************************************************************************************
*The target plane can not be directly deleted by the block, need to find the target erase block after the implementation of the erase operation, 
*the successful deletion of a block, returns 1, does not delete a block returns -1
********************************************************************************************************************************************/
int greedy_gc(struct ssd_info *ssd, unsigned int channel, unsigned int chip, unsigned int die)
{
	unsigned int i = 0, j = 0, p = 0 ,invalid_page = 0;
	unsigned int active_block1, active_block2, transfer_size = 0, free_page, avg_page_move = 0;                           /*Record the maximum number of blocks that are invalid*/
	struct local *  location = NULL;
	unsigned int plane , move_plane;
	int block1, block2;
	
	unsigned int active_block;
	unsigned int block;
	unsigned int page_move_count = 0;
	struct direct_erase * direct_erase_node_tmp = NULL;
	struct direct_erase * pre_erase_node_tmp = NULL;
	unsigned int * erase_block;
	unsigned int aim_page;


	erase_block = (unsigned int*)malloc( ssd->parameter->plane_die * sizeof(erase_block));
	//gets active blocks within all plane
	for ( p = 0; p < ssd->parameter->plane_die; p++)
	{
		if ( find_active_block(ssd, channel, chip, die, p) != SUCCESS )
		{
			free(erase_block);
			erase_block = NULL;
			printf("\n\n Error in uninterrupt_gc().\n");
			return ERROR;
		}
		active_block = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[p].active_block;
		
		//find the largest number of invalid pages in plane
		invalid_page = 0;
		block = -1;
		direct_erase_node_tmp = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[p].erase_node;
		for (i = 0; i<ssd->parameter->block_plane; i++)																					 /*Find the maximum number of invalid_page blocks, and the largest invalid_page_num*/
		{
			if ((active_block != i) && (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[p].blk_head[i].invalid_page_num>invalid_page)) /*Can not find the current active block*/
			{
				invalid_page = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[p].blk_head[i].invalid_page_num;
				block = i;
			}
		}
		//Check whether all is invalid page, if all is, then the current block is invalid block, need to remove this node from the erase chain
		if (invalid_page == ssd->parameter->page_block)
		{
			while (direct_erase_node_tmp != NULL)
			{
				if (block == direct_erase_node_tmp->block)
				{
					if (pre_erase_node_tmp == NULL)
						ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[p].erase_node = direct_erase_node_tmp->next_node;
					else
						pre_erase_node_tmp->next_node = direct_erase_node_tmp->next_node;

					free(direct_erase_node_tmp);
					direct_erase_node_tmp = NULL;
					break;
				}
				else
				{
					pre_erase_node_tmp = direct_erase_node_tmp;
					direct_erase_node_tmp = direct_erase_node_tmp->next_node;
				}
			}
			pre_erase_node_tmp = NULL;
			direct_erase_node_tmp = NULL;
		}

		//Found the block to be erased
		if (block == -1)
		{
			free(erase_block);
			erase_block = NULL;
			return ERROR;
		}

		//caculate sum of  vaild page_move count
		page_move_count += ssd->parameter->page_block - ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[p].blk_head[block].invalid_page_num;
		erase_block[p] = block;
	}

	//caculate the average of the sum vaild page_block,and distribute equally to all plane of die
	avg_page_move = page_move_count / (ssd->parameter->plane_die);

	//Perform a migration of valid data pages
	free_page = 0;
	page_move_count = 0;
	move_plane = 0;
	for (j = 0; j < ssd->parameter->plane_die; j++)
	{
		plane = j;
		block = erase_block[j];
		for (i = 0; i < ssd->parameter->page_block; i++)		                                                     /*Check each page one by one, if there is a valid data page need to move to other places to store*/
		{
			if ((ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].free_state&PG_SUB) == 0x0000000f)
			{
				free_page++;
			}
			if (free_page != 0)
			{
				printf("\ntoo much free page. \t %d\t .%d\t%d\t%d\t%d\t\n", free_page, channel, chip, die, plane); /*There are free pages, proved to be active blocks, blocks are not finished, can not be erased*/
				//getchar();
			}

			if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].valid_state > 0) /*The page is a valid page that requires a copyback operation*/
			{
				location = (struct local *)malloc(sizeof(struct local));
				alloc_assert(location, "location");
				memset(location, 0, sizeof(struct local));
				location->channel = channel;
				location->chip = chip;
				location->die = die;
				location->plane = plane;
				location->block = block;
				location->page = i;
				page_move_count++;

				move_page(ssd, location, move_plane, &transfer_size);                                                   /*Real move_page operation*/
				move_plane = (move_plane + 1) % ssd->parameter->plane_die;

				free(location);
				location = NULL;
			}
		}
	}

	//当move plane不等0的时候，表示此时是单数，需要磨平
	if (move_plane != 0)
	{
		find_active_block(ssd, channel, chip, die, move_plane);
		active_block = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[move_plane].active_block;
		aim_page = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[move_plane].blk_head[active_block].last_write_page + 2;
		if (aim_page == ssd->parameter->page_block + 1)
			getchar();
		make_same_level(ssd, channel, chip, die, move_plane, active_block, aim_page);
	}
	
	//判断是否偏移地址free page一致
	/*
	if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[0].free_page != ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[1].free_page)
	{
		printf("free page don't equal\n");
		getchar();
	}*/

	//迁移有效页的时间推动
	ssd->channel_head[channel].current_state = CHANNEL_GC;
	ssd->channel_head[channel].current_time = ssd->current_time;
	ssd->channel_head[channel].next_state = CHANNEL_IDLE;
	ssd->channel_head[channel].next_state_predict_time = ssd->current_time +
	page_move_count*(7 * ssd->parameter->time_characteristics.tWC + ssd->parameter->time_characteristics.tR + 7 * ssd->parameter->time_characteristics.tWC + ssd->parameter->time_characteristics.tPROG) +
	transfer_size*SECTOR*(ssd->parameter->time_characteristics.tWC + ssd->parameter->time_characteristics.tRC);

	//有效页迁移完成，开始执行擦除操作,擦除两个block
	if ((ssd->parameter->advanced_commands&AD_ERASE_SUSPEND_RESUME) == AD_ERASE_SUSPEND_RESUME)
	{
		//1.使用了suspend命令，首先更改chip上的suspend请求状态，用于检测是否有suspend请求到来
		ssd->channel_head[channel].chip_head[chip].gc_signal = SIG_ERASE_WAIT;
		ssd->channel_head[channel].chip_head[chip].erase_begin_time = ssd->channel_head[channel].next_state_predict_time;
		ssd->channel_head[channel].chip_head[chip].erase_cmplt_time = ssd->channel_head[channel].next_state_predict_time + ssd->parameter->time_characteristics.tBERS + ssd->parameter->time_characteristics.tERSL;

		//2.保留擦除操作的现场,产生一个suspend_erase_command请求挂载在chip上
		suspend_erase_operation(ssd, channel, chip, die, erase_block);
	}
	else
	{
		for (j = 0; j < ssd->parameter->plane_die; j++)
		{
			plane = j;
			block = erase_block[j];
			erase_operation(ssd, channel, chip, die, plane, block);					
		}
		ssd->mplane_erase_count++;
		ssd->channel_head[channel].chip_head[chip].current_state = CHIP_ERASE_BUSY;
		ssd->channel_head[channel].chip_head[chip].current_time = ssd->current_time;
		ssd->channel_head[channel].chip_head[chip].next_state = CHIP_IDLE;
		ssd->channel_head[channel].chip_head[chip].next_state_predict_time = ssd->channel_head[channel].next_state_predict_time + ssd->parameter->time_characteristics.tBERS;
	}
	
	free(erase_block);
	erase_block = NULL;
	return SUCCESS;
}

//执行suspend挂起操作，保留擦写操作的现场
struct ssd_info * suspend_erase_operation(struct ssd_info * ssd, unsigned int channel, unsigned int chip, unsigned int die, unsigned int * erase_block)
{
	long long erase_begin_time, erase_end_time;
	unsigned int flag = 0, j = 0;
	struct suspend_location * location = NULL;

	//1.保留现场，suspend擦除操作的开始时间、结束时间
	erase_begin_time = ssd->channel_head[channel].next_state_predict_time;
	erase_end_time = erase_begin_time + ssd->parameter->time_characteristics.tBERS + ssd->parameter->time_characteristics.tERSL;
	
	//2.保护现场，产生一个suspend的location地址，并挂载在chip上
	location = (struct suspend_location *)malloc(sizeof(struct suspend_location));
	alloc_assert(location, "location");
	memset(location, 0, sizeof(struct suspend_location));

	location->channel = channel;
	location->chip = chip;
	location->die = die;
	for (j = 0; j < ssd->parameter->plane_die; j++)
	{
		location->plane[j] = j;
		location->block[j] = erase_block[j];
	}

	//3.挂载chip上
	ssd->channel_head[channel].chip_head[chip].suspend_location = location;
	return ssd;
}

Status resume_erase_operation(struct ssd_info * ssd, unsigned int channel, unsigned int chip)
{
	unsigned int j = 0;
	struct suspend_location * resume_location = NULL;
	struct sub_request * sub = NULL ,* pre_sub = NULL;

	resume_location = ssd->channel_head[channel].chip_head[chip].suspend_location;
	if (resume_location != NULL)
	{
		//3.重置erase的状态时间线
		ssd->channel_head[resume_location->channel].chip_head[resume_location->chip].gc_signal = SIG_NORMAL;
		ssd->channel_head[resume_location->channel].chip_head[resume_location->chip].erase_begin_time = 0;
		ssd->channel_head[resume_location->channel].chip_head[resume_location->chip].erase_cmplt_time = 0;
		ssd->channel_head[resume_location->channel].chip_head[resume_location->chip].erase_rest_time = 0;

		//4.执行擦除操作
		for (j = 0; j < ssd->parameter->plane_die; j++)
			erase_operation(ssd, resume_location->channel, resume_location->chip, resume_location->die, resume_location->plane[j], resume_location->block[j]);

		ssd->mplane_erase_count++;
		free(resume_location);
		ssd->channel_head[channel].chip_head[chip].suspend_location = NULL;
		return SUCCESS;
	}
	else
	{
		printf("resume failed\n");
		getchar();
		return FAILURE;
	}
}


/*****************************************************************************************
*This function is for the gc operation to find a new ppn, because in the gc operation need 
*to find a new physical block to store the original physical block data
******************************************************************************************/
unsigned int get_ppn_for_gc(struct ssd_info *ssd, unsigned int channel, unsigned int chip, unsigned int die, unsigned int plane)
{
	unsigned int ppn=0;

	return ppn;
}
/*************************************************************
*function is when dealing with a gc operation, the need to gc 
*chain gc_node deleted
**************************************************************/
int delete_gc_node(struct ssd_info *ssd, unsigned int channel, struct gc_operation *gc_node)
{
	struct gc_operation *gc_pre = NULL;
	if (gc_node == NULL)
	{
		return ERROR;
	}

	if (gc_node == ssd->channel_head[channel].gc_command)
	{
		ssd->channel_head[channel].gc_command = gc_node->next_node;
	}
	else
	{
		gc_pre = ssd->channel_head[channel].gc_command;
		while (gc_pre->next_node != NULL)
		{
			if (gc_pre->next_node == gc_node)
			{
				gc_pre->next_node = gc_node->next_node;
				break;
			}
			gc_pre = gc_pre->next_node;
		}
	}
	free(gc_node);
	gc_node = NULL;
	ssd->gc_request--;
	return SUCCESS;
}

/* 
    find superblock block with the minimal P/E cycles
*/
Status find_active_superblock(struct ssd_info *ssd,unsigned int type)
{
	int i;
	int min_ec = 9999999;
	int min_sb;
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
	//set open superblock 
	ssd->open_sb[type] = ssd->sb_pool+min_sb;
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

	/*
	ssd->open_sb->blk_cnt = ssd->sb_pool[min_sb].blk_cnt;
	ssd->open_sb->next_wr_page = ssd->sb_pool[min_sb].next_wr_page;
	for (i = 0; i < ssd->open_sb->blk_cnt; i++)
	{
		ssd->open_sb->pos[i].channel = ssd->sb_pool[min_sb].pos[i].channel;
		ssd->open_sb->pos[i].chip    = ssd->sb_pool[min_sb].pos[i].chip;
		ssd->open_sb->pos[i].die     = ssd->sb_pool[min_sb].pos[i].die;
		ssd->open_sb->pos[i].plane   = ssd->sb_pool[min_sb].pos[i].plane;
		ssd->open_sb->pos[i].block   = ssd->sb_pool[min_sb].pos[i].block;
	}
	*/
	ssd->free_sb_cnt--;
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


