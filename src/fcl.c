#include <stdlib.h>
#include <crtdbg.h>

#include "flash.h"
#include "ssd.h"
#include "initialize.h"
#include "buffer.h"
#include "interface.h"
#include "ftl.h"
#include "fcl.h"

extern int secno_num_per_page, secno_num_sub_page;

//deal with read requests at one time
Status service_2_read(struct ssd_info *ssd, unsigned int channel)
{
	unsigned int chip, subs_count, i;
	unsigned int aim_die;
	unsigned int transfer_size = 0;
	unsigned int lpn,ppn;
	unsigned int chan0, chip0, die0, plane0, block0, page0;
	double tmp_time;
	struct loc *location;
	location = (struct loc *)malloc(sizeof(struct loc *));


	struct sub_request ** sub_r_request = NULL;
	sub_r_request = (struct sub_request **)malloc((ssd->parameter->die_chip * PAGE_INDEX) * sizeof(struct sub_request *));
	alloc_assert(sub_r_request, "sub_r_request");
	for (i = 0; i < (ssd->parameter->die_chip * PAGE_INDEX); i++)
		sub_r_request[i] = NULL;

	for (chip = 0; chip < ssd->channel_head[channel].chip; chip++)
	{
		if ((ssd->channel_head[channel].chip_head[chip].current_state == CHIP_IDLE) ||
			((ssd->channel_head[channel].chip_head[chip].next_state == CHIP_IDLE) &&
			(ssd->channel_head[channel].chip_head[chip].next_state_predict_time <= ssd->current_time)))
		{
			for (aim_die = 0; aim_die < ssd->parameter->die_chip; aim_die++)
			{			
				//find read request
				subs_count = find_read_sub_request(ssd, channel, chip, aim_die, sub_r_request, SR_WAIT, NORMAL);
				
				//complte the read requests
				for (i = 0; i < subs_count; i++)
				{
					lpn = sub_r_request[i]->lpn;
					ppn = sub_r_request[i]->ppn;

					if (lpn == 3839 && ppn == 1686710)
					{
						printf("look here\n");
					}

					//read 
					if (NAND_read(ssd, sub_r_request[i]) == FAILURE)
					{
						//printf("ERROR! read error!\n");
						//getchar();
					}

					switch (sub_r_request[i]->read_flag)
					{
					case REQ_READ:
						ssd->req_read_count++;
						break;
					case UPDATE_READ:
						ssd->update_read_count++; 
						break;
					default:
						break;
					}
					ssd->channel_head[channel].chip_head[chip].die_head[aim_die].read_cnt--;
					//sub_r_request[i]->size = size(ssd->dram->map->map_entry[lpn].state);
					sub_r_request[i]->current_state = SR_R_DATA_TRANSFER;
					sub_r_request[i]->next_state = SR_COMPLETE;



					if (sub_r_request[i]->size == 0)
					{
						printf("look here\n");
					}

					if (sub_r_request[i]->size > 8)
					{
						getchar();
					}
					transfer_size += sub_r_request[i]->size;
					sub_r_request[i]->complete_time = ssd->current_time + 7 * ssd->parameter->time_characteristics.tWC + ssd->parameter->time_characteristics.tR + (transfer_size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tRC;
					sub_r_request[i]->next_state_predict_time = sub_r_request[i]->complete_time;
					

				}
			}
			if (transfer_size > 0)
			{
				ssd->channel_head[channel].chip_head[chip].next_state_predict_time = ssd->current_time+ 7 * ssd->parameter->time_characteristics.tWC + ssd->parameter->time_characteristics.tR;
				tmp_time = ssd->channel_head[channel].chip_head[chip].next_state_predict_time + (transfer_size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tRC;
				ssd->channel_head[channel].next_state_predict_time = (tmp_time > ssd->channel_head[channel].next_state_predict_time) ? tmp_time : ssd->channel_head[channel].next_state_predict_time;
			}
			for (i = 0; i < subs_count; i++)
			{
				if (sub_r_request[i]->next_state_predict_time > ssd->channel_head[channel].next_state_predict_time && sub_r_request[i]->next_state_predict_time > ssd->channel_head[channel].chip_head[chip].next_state_predict_time)
				{
					getchar();
				}
			}
		}
	}

	//freet the malloc 
	for (i = 0; i < (ssd->parameter->plane_die * PAGE_INDEX); i++)
		sub_r_request[i] = NULL;
	free(sub_r_request);
	sub_r_request = NULL;
	free(location);
	
	return SUCCESS;
}

/**************************************************************************
*This function is also only deal with read requests, processing chip current state is CHIP_WAIT,
*Or the next state is CHIP_DATA_TRANSFER and the next state of the expected time is less than the current time of the chip
***************************************************************************/
Status services_2_r_data_trans(struct ssd_info * ssd, unsigned int channel)
{
	return SUCCESS;
}

Status services_2_r_read(struct ssd_info * ssd)
{
	return SUCCESS;
}

/**************************************************************************************
*Function function is given in the channel, chip, die above looking for reading requests
*The request for this child ppn corresponds to the ppn of the corresponding plane's register
*****************************************************************************************/
unsigned int find_read_sub_request(struct ssd_info * ssd, unsigned int channel, unsigned int chip, unsigned int die, struct sub_request ** subs, unsigned int state, unsigned int command)
{
	struct sub_request * sub = NULL;
	unsigned int add_reg, i, j = 0;

	for (i = 0; i < (ssd->parameter->plane_die * PAGE_INDEX); i++)
		subs[i] = NULL;
	
	j = 0;
	sub = ssd->channel_head[channel].subs_r_head;
	while (sub != NULL)
	{
		if (sub->location->chip == chip && sub->location->die == die)
		{
			if (sub->current_state == state)
			{
				if (command == MUTLI_PLANE)
				{
					subs[j] = sub;
					j++;
					if (j == ssd->parameter->plane_die)
						break;
				}
				else if (command == NORMAL)
				{
					if ((sub->oneshot_mutliplane_flag != 1) && (sub->oneshot_flag != 1) && (sub->mutliplane_flag != 1))
					{
						subs[j] = sub;
						j++;
					}
					if (j == 1)
						break; 
				}
			}
		}
		sub = sub->next_node;
	}
 

	if (j > (ssd->parameter->plane_die * PAGE_INDEX))
	{
		printf("error,beyong plane_die* PAGE_INDEX\n");
		getchar();
	}

	return j;
}

/*********************************************************************************************
* function that specifically serves a read request
*1，Only when the current state of the sub request is SR_R_C_A_TRANSFER
*2，The current state of the read request is SR_COMPLETE or the next state is SR_COMPLETE and 
*the next state arrives less than the current time
**********************************************************************************************/
Status services_2_r_complete(struct ssd_info * ssd)
{
	unsigned int i = 0;
	struct sub_request * sub = NULL, *p = NULL;
	
	for (i = 0; i<ssd->parameter->channel_number; i++)                                       /*This loop does not require the channel time, when the read request is completed, it will be removed from the channel queue*/
	{
		sub = ssd->channel_head[i].subs_r_head;
		p = NULL;
		while (sub != NULL)
		{
			if ((sub->current_state == SR_COMPLETE) || sub->next_state == SR_COMPLETE)
			{
				if (sub != ssd->channel_head[i].subs_r_head)                         
				{
					if (sub == ssd->channel_head[i].subs_r_tail)
					{
						ssd->channel_head[i].subs_r_tail = p;
						p->next_node = NULL;
					}
					else
					{
						p->next_node = sub->next_node;
						sub = p->next_node;
					}
				}
				else
				{
					if (ssd->channel_head[i].subs_r_head != ssd->channel_head[i].subs_r_tail)
					{
						ssd->channel_head[i].subs_r_head = sub->next_node;
						sub = sub->next_node;
						p = NULL;
					}
					else
					{
						ssd->channel_head[i].subs_r_head = NULL;
						ssd->channel_head[i].subs_r_tail = NULL;
						break;
					}
				}
			}
			else
			{
				p = sub;
				sub = sub->next_node;
			}
			
		}
	}

	return SUCCESS;
}


/*****************************************************************************************
*This function is also a service that only reads the child request, and is in a wait state
******************************************************************************************/
Status services_2_r_wait(struct ssd_info * ssd, unsigned int channel)
{
	struct sub_request ** sub_place = NULL;
	unsigned int sub_r_req_count, i ,chip;

	sub_place = (struct sub_request **)malloc(ssd->parameter->plane_die * PAGE_INDEX * sizeof(struct sub_request *));
	alloc_assert(sub_place, "sub_place");
	sub_r_req_count = 0;
	for (chip = 0; chip < ssd->parameter->chip_channel[channel]; chip++)
	{
		/*************************************************************************************************************************************/
		//判断收到的gc信号，如果是resume的信号，则进行恢复擦除操作
		if (ssd->channel_head[channel].chip_head[chip].gc_signal != SIG_NORMAL)
		{
			if ((ssd->channel_head[channel].chip_head[chip].current_state == CHIP_IDLE) || ((ssd->channel_head[channel].chip_head[chip].next_state == CHIP_IDLE) &&
				(ssd->channel_head[channel].chip_head[chip].next_state_predict_time <= ssd->current_time)))
			{
				if (ssd->current_time >= ssd->channel_head[channel].chip_head[chip].erase_cmplt_time)
				{
					if (ssd->channel_head[channel].chip_head[chip].gc_signal == SIG_ERASE_SUSPEND)
					{
						//将剩余的擦除请求，推动chip的时间线
						ssd->channel_head[channel].chip_head[chip].current_state = CHIP_ERASE_BUSY;
						ssd->channel_head[channel].chip_head[chip].next_state = CHIP_IDLE;
						ssd->channel_head[channel].chip_head[chip].next_state_predict_time += ssd->channel_head[channel].chip_head[chip].erase_rest_time;

						resume_erase_operation(ssd, channel, chip);
						
						ssd->resume_count++;
						ssd->channel_head[channel].channel_busy_flag = 1;
						ssd->channel_head[channel].chip_head[chip].gc_signal = SIG_NORMAL;
						continue;
					}
					else
					{
						resume_erase_operation(ssd, channel, chip);
						ssd->channel_head[channel].channel_busy_flag = 0;
						ssd->channel_head[channel].chip_head[chip].gc_signal = SIG_NORMAL;
					}
				}
			}
		}
		/***************************************************************************************************************************************/
		if (ssd->parameter->flash_mode == TLC_MODE)							 //只有在tlc mode 下才能进行one shot mutli plane read/one shot read
		{
			//判断是否可以用高级命令oneshot_mutliplane_read
			if ((ssd->parameter->advanced_commands&AD_ONESHOT_READ) == AD_ONESHOT_READ && (ssd->parameter->advanced_commands&AD_MUTLIPLANE) == AD_MUTLIPLANE)
			{
				sub_r_req_count = find_r_wait_sub_request(ssd, channel, chip, sub_place, ONE_SHOT_READ_MUTLI_PLANE);
				if (sub_r_req_count == (PAGE_INDEX*ssd->parameter->plane_die))
				{
					go_one_step(ssd, sub_place, sub_r_req_count, SR_R_C_A_TRANSFER, ONE_SHOT_READ_MUTLI_PLANE);
					ssd->channel_head[channel].channel_busy_flag = 1;
					continue;
				}
			}
			//判断能否用one shot read高级命令完成
			if ((ssd->parameter->advanced_commands&AD_ONESHOT_READ) == AD_ONESHOT_READ)
			{
				sub_r_req_count = find_r_wait_sub_request(ssd, channel, chip, sub_place, ONE_SHOT_READ);
				if (sub_r_req_count == PAGE_INDEX)
				{

					for (i = sub_r_req_count; i < (ssd->parameter->plane_die * PAGE_INDEX); i++)
						sub_place[i] = NULL;

					go_one_step(ssd, sub_place, sub_r_req_count, SR_R_C_A_TRANSFER, ONE_SHOT_READ);
					ssd->channel_head[channel].channel_busy_flag = 1;
					continue;
				}
			}
		}
		//判断能否用mutli plane高级命令完成
		if ((ssd->parameter->advanced_commands&AD_MUTLIPLANE) == AD_MUTLIPLANE)
		{
			sub_r_req_count = find_r_wait_sub_request(ssd, channel, chip, sub_place, MUTLI_PLANE);
			if ((sub_r_req_count >1) && (sub_r_req_count <= ssd->parameter->plane_die))
			{
				for (i = sub_r_req_count; i < (ssd->parameter->plane_die * PAGE_INDEX); i++)
					sub_place[i] = NULL;

				go_one_step(ssd, sub_place, sub_r_req_count, SR_R_C_A_TRANSFER, MUTLI_PLANE);
				ssd->channel_head[channel].channel_busy_flag = 1;
				continue;
			}
		}

		//若不能，表示所有的高级命令都不可行，则去执行普通的读请求,若普通的读请求未找到，则返回，本chip无有效读请求执行
		sub_r_req_count = find_r_wait_sub_request(ssd, channel, chip, sub_place, NORMAL);
		if (sub_r_req_count == 1)
		{
			for (i = sub_r_req_count; i < (ssd->parameter->plane_die * PAGE_INDEX); i++)
				sub_place[i] = NULL;

			go_one_step(ssd, sub_place, sub_r_req_count, SR_R_C_A_TRANSFER, NORMAL);
			ssd->channel_head[channel].channel_busy_flag = 1;
		}
		else if (sub_r_req_count == 0)
		{
			ssd->channel_head[channel].channel_busy_flag = 0;
		}
		
		/***************************************************************************************************************************************/
		//判断是否有suspend挂起引起来的读请求，如果是，将这些请求挂载在对应channel的请求上
		if (ssd->channel_head[channel].chip_head[chip].gc_signal != SIG_NORMAL)
		{
			if (sub_r_req_count != 0)
			{
				if (ssd->channel_head[channel].chip_head[chip].gc_signal == SIG_ERASE_WAIT)
				{
					ssd->channel_head[channel].chip_head[chip].erase_rest_time = ssd->channel_head[channel].chip_head[chip].erase_cmplt_time - ssd->current_time;
					ssd->suspend_count++;
				}
				ssd->channel_head[channel].chip_head[chip].gc_signal = SIG_ERASE_SUSPEND;
				ssd->suspend_read_count += sub_r_req_count;
			}
		}
		/***************************************************************************************************************************************/

	}

	for (i = 0; i < (ssd->parameter->plane_die * PAGE_INDEX); i++)
		sub_place[i] = NULL;
	free(sub_place);
	sub_place = NULL;

	return SUCCESS;
}

//判断请求是不是suspend的block
Status check_req_in_suspend(struct ssd_info * ssd, unsigned int channel, unsigned int chip, struct sub_request * sub_plane_request)
{
	unsigned  int i;
	struct suspend_location * loc = NULL;

	loc = ssd->channel_head[channel].chip_head[chip].suspend_location;
	if (sub_plane_request->location->die == loc->die)
	{
		for (i = 0; i < ssd->parameter->plane_die; i++)
		{
			if ((sub_plane_request->location->plane == i) && (sub_plane_request->location->block == loc->block[i]))
				return FAILURE;
		}
	}
	return SUCCESS;
}

//寻找能one shot read的请求个数
unsigned int find_r_wait_sub_request(struct ssd_info * ssd, unsigned int channel, unsigned int chip, struct sub_request ** sub_place, unsigned int command)
{
	unsigned int i,j,flag;
	unsigned int aim_die, aim_plane, aim_block, aim_group;
	unsigned int sub_count, plane_flag;
	struct sub_request * sub_plane_request = NULL;

	//初始化
	for (i = 0; i < (ssd->parameter->plane_die * PAGE_INDEX); i++)
		sub_place[i] = NULL;

	flag = 0;
	i = 0;
	sub_count = 0;
	sub_plane_request = ssd->channel_head[channel].subs_r_head;
	while (sub_plane_request != NULL)
	{
		if (sub_plane_request->current_state == SR_WAIT && sub_plane_request->location->chip == chip)
		{
			/*************************************************************************************/
			if (ssd->channel_head[channel].chip_head[chip].gc_signal != SIG_NORMAL)
			{
				if (check_req_in_suspend(ssd, channel, chip, sub_plane_request) == FAILURE)
				{
					sub_plane_request = sub_plane_request->next_node;
					continue;
				}
			}
			/*************************************************************************************/

		    if (command == ONE_SHOT_READ_MUTLI_PLANE)
			{
				if (sub_count % PAGE_INDEX == 0)
				{
					if (((ssd->channel_head[sub_plane_request->location->channel].chip_head[sub_plane_request->location->chip].current_state == CHIP_IDLE) ||
						((ssd->channel_head[sub_plane_request->location->channel].chip_head[sub_plane_request->location->chip].next_state == CHIP_IDLE) &&
						(ssd->channel_head[sub_plane_request->location->channel].chip_head[sub_plane_request->location->chip].next_state_predict_time <= ssd->current_time))))
					{
						if (sub_count == 0)
						{
							aim_die = sub_plane_request->location->die;
							aim_plane = sub_plane_request->location->plane;
							aim_block = sub_plane_request->location->block;
							aim_group = sub_plane_request->location->page / PAGE_INDEX;
							i = 0;
							flag = 1;
						}
						else   //当第一个plane已经寻找完了，下面开始寻找第二个的目标aimplane\aim_block
						{
							if (ssd->parameter->scheduling_algorithm == FCFS)
							{
								if ((sub_plane_request->location->plane != aim_plane) && (sub_place[i*PAGE_INDEX] != NULL))
								{
									aim_plane = sub_plane_request->location->plane;
									aim_block = sub_plane_request->location->block;
									i++;
									flag = 1;
								}
							}
						}
					}
				}
				if (flag == 1)
				{
					if (sub_plane_request->location->die == aim_die && sub_plane_request->location->plane == aim_plane && sub_plane_request->location->block == aim_block)
					{
						for (j = 0; j < PAGE_INDEX; j++)
						{
							if (sub_plane_request->location->page == (j + aim_group*PAGE_INDEX))
							{
								if (sub_place[j + (i*PAGE_INDEX)] != NULL)
								{
									printf("read the same page!\n");
									getchar();
								}
								sub_place[j + (i*PAGE_INDEX)] = sub_plane_request;
								sub_count++;
								break;
							}

						}
					}
					if (sub_count == (ssd->parameter->plane_die * PAGE_INDEX))
						break;
				}
			}
			else if (command == ONE_SHOT_READ)
			{
				if (flag == 0)
				{
					if (((ssd->channel_head[sub_plane_request->location->channel].chip_head[sub_plane_request->location->chip].current_state == CHIP_IDLE) ||
						((ssd->channel_head[sub_plane_request->location->channel].chip_head[sub_plane_request->location->chip].next_state == CHIP_IDLE) &&
						(ssd->channel_head[sub_plane_request->location->channel].chip_head[sub_plane_request->location->chip].next_state_predict_time <= ssd->current_time))))
					{
						aim_die = sub_plane_request->location->die;
						aim_plane = sub_plane_request->location->plane;
						aim_block = sub_plane_request->location->block;
						aim_group = sub_plane_request->location->page / PAGE_INDEX;
						flag = 1;
					}
				}
				if (flag == 1)
				{
					if (sub_plane_request->location->die == aim_die && sub_plane_request->location->plane == aim_plane && sub_plane_request->location->block == aim_block)
					{
						for (j = 0; j < PAGE_INDEX; j++)
						{
							if (sub_plane_request->location->page == (j + aim_group*PAGE_INDEX))
							{
								if (sub_place[j] != NULL)
								{
									printf("read the same page!\n");
									getchar();
								}
								sub_place[j] = sub_plane_request;
								sub_count++;
								break;
							}

						}
					}
				}
			}
			else if (command == MUTLI_PLANE)
			{
				if (i == 0)
				{
					if (((ssd->channel_head[sub_plane_request->location->channel].chip_head[sub_plane_request->location->chip].current_state == CHIP_IDLE) ||
						((ssd->channel_head[sub_plane_request->location->channel].chip_head[sub_plane_request->location->chip].next_state == CHIP_IDLE) &&
						(ssd->channel_head[sub_plane_request->location->channel].chip_head[sub_plane_request->location->chip].next_state_predict_time <= ssd->current_time))))
					{
						sub_place[0] = sub_plane_request;
						i++;
						sub_count++;
					}
				}
				else
				{
					if ((sub_place[0]->location->chip == sub_plane_request->location->chip) &&
						(sub_place[0]->location->die == sub_plane_request->location->die) &&
						(sub_place[0]->location->page == sub_plane_request->location->page))
					{
						plane_flag = 0;
						for (j = 0; j < i; j++)
						{
							if (sub_place[j]->location->plane == sub_plane_request->location->plane)
								plane_flag = 1;
						}
						if (plane_flag == 0)
						{
							sub_place[i] = sub_plane_request;
							i++;
							sub_count++;
						}
					}
				}
				if (i == ssd->parameter->plane_die)
					break;
			}
			else if (command == NORMAL)
			{
				if (((ssd->channel_head[sub_plane_request->location->channel].chip_head[sub_plane_request->location->chip].current_state == CHIP_IDLE) ||
					((ssd->channel_head[sub_plane_request->location->channel].chip_head[sub_plane_request->location->chip].next_state == CHIP_IDLE) &&
					(ssd->channel_head[sub_plane_request->location->channel].chip_head[sub_plane_request->location->chip].next_state_predict_time <= ssd->current_time))))
				{
					sub_place[0] = sub_plane_request;
					sub_count = 1;
					break;
				}
			}
		}
		sub_plane_request = sub_plane_request->next_node;
	}

	if (sub_count > ssd->parameter->plane_die * PAGE_INDEX)
	{
		printf("error,beyong plane_die * PAGE_INDEX\n");
		getchar();
	}
	return sub_count;
}




/***********************************************************************************************************
*1.The state transition of the child request, and the calculation of the time, are handled by this function
*2.The state of the execution of the normal command, and the calculation of the time, are handled by this function
****************************************************************************************************************/
Status go_one_step(struct ssd_info * ssd, struct sub_request ** subs, unsigned int subs_count, unsigned int aim_state, unsigned int command)
{
	return SUCCESS;
}

/****************************************
Write the request function of the request
*****************************************/
Status services_2_write(struct ssd_info * ssd, unsigned int channel)
{
	int j = 0,i = 0;
	unsigned int chip_token = 0;
	struct sub_request *sub = NULL;

	/************************************************************************************************************************
	*Because it is dynamic allocation, all write requests hanging in ssd-> subs_w_head, that is, do not know which allocation before writing on the channel
	*************************************************************************************************************************/
	if (ssd->subs_w_head != NULL || ssd->channel_head[channel].subs_w_head != NULL)
	{
		//判断tlc模式下，一次process，请求链上的请求有没有小于3个请求，不能执行one　shot
		if (ssd->channel_head[channel].subs_w_head != NULL)
		{
			sub = ssd->channel_head[channel].subs_w_head;
			while (sub != NULL)
			{
				i++;	
				sub = sub->next_node;
			}
			if (ssd->parameter->flash_mode == TLC_MODE)
			{
				if (i < PAGE_INDEX)
					printf("\n less than 3 subs \n");
			}
		}
		
		//根据flag去判断到底本次状态转变执行哪种方式
		if (ssd->parameter->allocation_scheme == DYNAMIC_ALLOCATION || ssd->parameter->allocation_scheme == HYBRID_ALLOCATION)
		{

		}
		else if (ssd->parameter->allocation_scheme == STATIC_ALLOCATION || ssd->parameter->allocation_scheme == SUPERBLOCK_ALLOCATION)
		{
			for (j = 0; j < ssd->channel_head[channel].chip; j++)
			{
				if (ssd->channel_head[channel].subs_w_head == NULL)
					continue;

				if (ssd->channel_head[channel].channel_busy_flag == 0)
				{
					if ((ssd->channel_head[channel].chip_head[j].current_state == CHIP_IDLE) || ((ssd->channel_head[channel].chip_head[j].next_state == CHIP_IDLE) && (ssd->channel_head[channel].chip_head[j].next_state_predict_time <= ssd->current_time)))
					{
						if (dynamic_advanced_process(ssd, channel, j) == NULL)
							ssd->channel_head[channel].channel_busy_flag = 0;
						else
							ssd->channel_head[channel].channel_busy_flag = 1;
					}
				}
			}
		}
	}
	else
	{
		ssd->channel_head[channel].channel_busy_flag = 0;
	}
	return SUCCESS;
}

Status Is_Update_Read_Done(struct ssd_info *ssd, struct sub_request *sub)
{
	unsigned int i;
	unsigned int chan, chip, die, plane, block,blk_type;

	for (i = 0; i < sub->update_cnt; i++)
	{
		switch (i)
		{
		case 0:
			if (sub->update_0->next_state != SR_COMPLETE)
			{
				chan = sub->location->channel;
				chip = sub->location->chip;
				die = sub->location->die;
				plane = sub->location->plane;
				block = sub->location->block;
				blk_type = ssd->channel_head[chan].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].block_type;
				return 0;
			}
			break;
		case 1:
			if (sub->update_1->next_state != SR_COMPLETE)
				return 0;
			break;
		case 2:
			if (sub->update_2->next_state != SR_COMPLETE)
				return 0;
			break;
		case 3:
			if (sub->update_3->next_state != SR_COMPLETE)
				return 0;
			break;
		case 4:
			if (sub->update_4->next_state != SR_COMPLETE)
				return 0;
			break;
		case 5:
			if (sub->update_5->next_state != SR_COMPLETE)
				return 0;
			break;
		case 6:
			if (sub->update_6->next_state != SR_COMPLETE)
				return 0;
			break;
		case 7:
			if (sub->update_7->next_state != SR_COMPLETE)
				return 0;
			break;
		case 8:
			if (sub->update_8->next_state != SR_COMPLETE)
				return 0;
			break;
		case 9:
			if (sub->update_9->next_state != SR_COMPLETE)
				return 0;
			break;
		case 10:
			if (sub->update_10->next_state != SR_COMPLETE)
				return 0;
			break;
		case 11:
			if (sub->update_11->next_state != SR_COMPLETE)
				return 0;
			break;
		case 12:
			if (sub->update_12->next_state != SR_COMPLETE)
				return 0;
			break;
		case 13:
			if (sub->update_13->next_state != SR_COMPLETE)
				return 0;
			break;
		case 14:
			if (sub->update_14->next_state != SR_COMPLETE)
				return 0;
			break;
		case 15:
			if (sub->update_15->next_state != SR_COMPLETE)
				return 0;
			break;
		case 16:
			if (sub->update_16->next_state != SR_COMPLETE)
				return 0;
			break;
		case 17:
			if (sub->update_17->next_state != SR_COMPLETE)
				return 0;
			break;
		case 18:
			if (sub->update_18->next_state != SR_COMPLETE)
				return 0;
			break;
		case 19:
			if (sub->update_19->next_state != SR_COMPLETE)
				return 0;
			break;
		case 20:
			if (sub->update_20->next_state != SR_COMPLETE)
				return 0;
			break;
		case 21:
			if (sub->update_21->next_state != SR_COMPLETE)
				return 0;
			break;
		case 22:
			if (sub->update_22->next_state != SR_COMPLETE)
				return 0;
			break;
		case 23:
			if (sub->update_23->next_state != SR_COMPLETE)
				return 0;
			break;
		case 24:
			if (sub->update_24->next_state != SR_COMPLETE)
				return 0;
			break;
		case 25:
			if (sub->update_25->next_state != SR_COMPLETE)
				return 0;
			break;
		case 26:
			if (sub->update_26->next_state != SR_COMPLETE)
				return 0;
			break;
		case 27:
			if (sub->update_27->next_state != SR_COMPLETE)
				return 0;
			break;
		case 28:
			if (sub->update_28->next_state != SR_COMPLETE)
				return 0;
			break;
		case 29:
			if (sub->update_29->next_state != SR_COMPLETE)
				return 0;
			break;
		case 30:
			if (sub->update_30->next_state != SR_COMPLETE)
				return 0;
			break;
		case 31:
			if (sub->update_31->next_state != SR_COMPLETE)
				return 0;
			break;
		default:

			break;
		}	
	}
	return 1;
}

/****************************************************************************************************************************
*When ssd supports advanced commands, the function of this function is to deal with high-level command write request
*According to the number of requests, decide which type of advanced command to choose (this function only deal with write requests, 
*read requests have been assigned to each channel, so the implementation of the election between the corresponding command)
*****************************************************************************************************************************/
struct ssd_info *dynamic_advanced_process(struct ssd_info *ssd, unsigned int channel, unsigned int chip)
{
	unsigned int subs_count = 0;
	unsigned int update_count = 0;                                                                                                                     /*record which plane has sub request in static allocation*/
	struct sub_request *sub = NULL, *p = NULL;
	struct sub_request ** subs = NULL;
	unsigned int max_sub_num = 0, aim_subs_count;
	unsigned int die_token = 0, plane_token = 0;

	unsigned int mask = 0x00000001;
	unsigned int i = 0, j = 0, k = 0;
	unsigned int aim_die;

	max_sub_num = (ssd->parameter->die_chip)*(ssd->parameter->plane_die)*PAGE_INDEX;
	subs = (struct sub_request **)malloc(max_sub_num*sizeof(struct sub_request *));
	alloc_assert(subs, "sub_request");
	update_count = 0;
	for (i = 0; i < max_sub_num; i++)
		subs[i] = NULL;  
	

	//根据是动态分配还是静态分配，选择不同的挂载点
	if (ssd->parameter->allocation_scheme == DYNAMIC_ALLOCATION || ssd->parameter->allocation_scheme == HYBRID_ALLOCATION)
		sub = ssd->subs_w_head;
	else if (ssd->parameter->allocation_scheme == STATIC_ALLOCATION || ssd->parameter->allocation_scheme ==	SUPERBLOCK_ALLOCATION)
	{
		sub = ssd->channel_head[channel].subs_w_head;
		aim_die = sub->location->die;
	}

	//1.遍历请求链上当前空闲的请求
	subs_count = 0;
	while ((sub != NULL) && (subs_count < max_sub_num))
	{
		if (sub->current_state == SR_WAIT)
		{
			if (Is_Update_Read_Done(ssd,sub))// && (sub->update->next_state_predict_time <= ssd->current_time)))))    //没有需要提前读出的页
			{
				if (ssd->parameter->allocation_scheme == STATIC_ALLOCATION || ssd->parameter->allocation_scheme == SUPERBLOCK_ALLOCATION)							//如果是静态分配，还需要保证找到的空闲请求是属于同一个channel,chip,die
				{
					if (sub->location->chip == chip && sub->location->die == aim_die)
					{
						subs[subs_count] = sub;
						subs_count++;
					}
				}
				else if (ssd->parameter->allocation_scheme == DYNAMIC_ALLOCATION)						//动态分配，即不知道逻辑页分到到哪一个固定的页，这个是不确定的
				{
					//if (sub->location->chip == chip)
					//{
					//	subs[subs_count] = sub;
					//	subs_count++;
					//}
					subs[subs_count] = sub;
					subs_count++;
				}
				else if (ssd->parameter->allocation_scheme == HYBRID_ALLOCATION)						//根据首节点的location判断是完全动态的写入还是stripe的写入
				{
					if (ssd->subs_w_head->location->channel == -1)
					{
						if (sub->location->chip == -1 && sub->location->channel == -1)
						{
							subs[subs_count] = sub;
							subs_count++;
						}
					}
					else
					{
						if (sub->location->chip == chip && sub->location->channel == channel)
						{
							subs[subs_count] = sub;
							subs_count++;
						}
					}
				}
			}
		}

		if (ssd->parameter->allocation_scheme == SUPERBLOCK_ALLOCATION && (~Is_Update_Read_Done(ssd,sub))) // ensure the program order character
		{
				break;
		}

		if (sub->update_read_flag == 1)
		{
			update_count++;
		}
			

		sub = sub->next_node;
	}

	//2.找不到空闲的情况，则返回null,表示当前没有请求执行
	if (subs_count == 0)
	{
		for (i = 0; i < max_sub_num; i++)
			subs[i] = NULL;
		free(subs);
		subs = NULL;
		return NULL;
	}

	//3.根据请求链遍历的情况，处理更新请求队列的管理	
	if (update_count > ssd->update_sub_request)
		ssd->update_sub_request = update_count;

	//超过更新队列深度，将trace文件读取阻塞
	if (update_count > ssd->parameter->update_reqeust_max)
	{
		printf("update sub request is full!\n");
		ssd->buffer_full_flag = 1;  //blcok the buffer
	}
	else
		ssd->buffer_full_flag = 0;
		
	//4.根据不同模式，选择对应的高级命令服务
	if (ssd->parameter->flash_mode == SLC_MODE)
	{ 
		if ((ssd->parameter->advanced_commands&AD_MUTLIPLANE) == AD_MUTLIPLANE)
		{
			aim_subs_count = ssd->parameter->plane_die;
			service_advance_command(ssd, channel, chip, subs, subs_count, aim_subs_count, MUTLI_PLANE);
		}
		else   //当不支持高级命令的时候，使用one page  program
		{
			for (i = 1; i<subs_count; i++)
			{
				subs[i] = NULL;
			}
			subs_count = 1;
			get_ppn_for_normal_command(ssd, channel, chip, subs[0]);
			//printf("lz:normal program\n");
			//getchar();
		}
	}
	else if (ssd->parameter->flash_mode == TLC_MODE)
	{
		if ((ssd->parameter->advanced_commands&AD_ONESHOT_PROGRAM) == AD_ONESHOT_PROGRAM)
		{
			if ((ssd->parameter->advanced_commands&AD_MUTLIPLANE) == AD_MUTLIPLANE)
			{
				aim_subs_count = ssd->parameter->plane_die * PAGE_INDEX;
				service_advance_command(ssd, channel, chip, subs, subs_count, aim_subs_count, ONE_SHOT_MUTLI_PLANE);
			}
			else
			{
				aim_subs_count = PAGE_INDEX;
				service_advance_command(ssd, channel, chip, subs, subs_count, aim_subs_count, ONE_SHOT);
			}
		}
		else
		{
			printf("Error! tlc mode match advanced commamd failed!\n");
			getchar();
		}
	}

	//5.处理完成，释放请求数组空间，并返回ssd结构体，表示请求已执行
	for (i = 0; i < max_sub_num; i++)
	{
		subs[i] = NULL;
	}
	free(subs);
	subs = NULL;
	return ssd;
}


//根据不同的高级命令去凑足请求的个数
Status service_advance_command(struct ssd_info *ssd, unsigned int channel, unsigned int chip, struct sub_request ** subs, unsigned int subs_count, unsigned int aim_subs_count, unsigned int command)
{
	unsigned int i = 0;
	unsigned int max_sub_num;
	struct sub_request *sub = NULL, *p = NULL;

	max_sub_num = (ssd->parameter->die_chip)*(ssd->parameter->plane_die)*PAGE_INDEX;

	//当最后读完了，只剩下了单个请求，这个时候可以使用普通的one page program去写完
	if ((ssd->trace_over_flag == 1 && ssd->request_work == NULL) || (ssd->parameter->warm_flash == 1 && ssd->trace_over_flag == 1 && ssd->warm_flash_cmplt == 0))
	{
		for (i = 0; i < subs_count;i++)
			get_ppn_for_normal_command(ssd, channel, chip, subs[i]);

		return SUCCESS;
	}
	
	if (subs_count >= aim_subs_count)
	{
		for (i = aim_subs_count; i < subs_count; i++)
			subs[i] = NULL;

		subs_count = aim_subs_count;
		get_ppn_for_advanced_commands(ssd, channel, chip, subs, subs_count, command);
	}
	else if (subs_count > 0)
	{
		/*
		while (subs_count < aim_subs_count)
		{
			getout2buffer(ssd, NULL, subs[0]->total_request);
			//重新遍历整个写请求链，取出对应的两个写子请求
			for (i = 0; i < max_sub_num; i++)
			{
				subs[i] = NULL;
			}
			sub = ssd->subs_w_head;
			subs_count = 0;
			while ((sub != NULL) && (subs_count < max_sub_num))
			{
				if (sub->current_state == SR_WAIT)
				{
					if ((sub->update == NULL) || ((sub->update != NULL) && ((sub->update->current_state == SR_COMPLETE) || ((sub->update->next_state == SR_COMPLETE) && (sub->update->next_state_predict_time <= ssd->current_time)))))    //没有需要提前读出的页
					{
						subs[subs_count] = sub;
						subs_count++;
					}
				}
				p = sub;
				sub = sub->next_node;
			}
		}
		if (subs_count == aim_subs_count)
			get_ppn_for_advanced_commands(ssd, channel, chip, subs, subs_count, command);

		*/
		return FAILURE;
	}
	else
	{
		printf("there is no vaild subs\n");
	}
	return SUCCESS;
}

/******************************************************************************************************
*The function of the function is to find two pages of the same horizontal position for the two plane 
*command, and modify the statistics, modify the status of the page
*******************************************************************************************************/
Status find_level_page(struct ssd_info *ssd, unsigned int channel, unsigned int chip, unsigned int die, struct sub_request **sub, unsigned int subs_count)
{
	unsigned int i,j, aim_page = 0, old_plane;
	struct gc_operation *gc_node;
	unsigned int gc_add;

	unsigned int plane,active_block, page,equal_flag;
	unsigned int *page_place;

	page_place = (unsigned int *)malloc(ssd->parameter->plane_die*sizeof(page_place));
	old_plane = ssd->channel_head[channel].chip_head[chip].die_head[die].token;

	//验证请求sub的有效性
	if (subs_count != ssd->parameter->plane_die)
	{
		printf("find level failed\n");
		getchar();
		return ERROR;
	}

	for (i = 0; i < ssd->parameter->plane_die; i++)
	{
		find_active_block(ssd, channel, chip, die, i);
		active_block = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[i].active_block;
		page = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[i].blk_head[active_block].last_write_page + 1;
		page_place[i] = page;
	}

	equal_flag = 1;
	for (i = 0; i < (ssd->parameter->plane_die - 1); i++)
	{
		if (page_place[i] != page_place[i + 1])
		{
			equal_flag = 0;
			break;
		}
	}

	//判断所有的page是否相等，如果相等，执行mutli plane，如果不相等，贪婪的使用，将所有的page向最大的page靠近
	if (equal_flag == 1)	//page偏移地址一致
	{
		for (i = 0; i < ssd->parameter->plane_die; i++)
		{
			active_block = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[i].active_block;
			flash_page_state_modify(ssd, sub[i], channel, chip, die, i, active_block, page_place[i]);
		}
	}
	else				    //page偏移地址不一致
	{
		if (ssd->parameter->greed_MPW_ad == 1)                                          
		{			
			//查看page最大的页当做aim_page
			for (i = 0; i < ssd->parameter->plane_die; i++)
			{
				if (page_place[i] > aim_page)
					aim_page = page_place[i];
			}
			/*
			for (i = 0; i < ssd->parameter->plane_die; i++)
			{
				if (page_place[i] != aim_page)
				{
					if (aim_page - page_place[i] != 1)
						getchar();
				}
			}*/

			//首先检查是否出现了0-63、63-0的跨页不均匀现象
			if ((page_place[0] == (ssd->parameter->page_block - 1) && page_place[1] == 0) || (page_place[0] == 0 && page_place[1] == (ssd->parameter->page_block - 1)))
			{
				for (i = 0; i < ssd->parameter->plane_die; i++)
				{
					if (page_place[i] == (ssd->parameter->page_block - 1))
					{
						//首先将这个63号页置无效
						active_block = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[i].active_block;
						make_same_level(ssd, channel, chip, die, i, active_block, ssd->parameter->page_block);

						//寻找新的block,由于置无效了63号页，则会寻找到后面的一个空闲块
						find_active_block(ssd, channel, chip, die, i);
						active_block = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[i].active_block;
						page_place[i] = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[i].blk_head[active_block].last_write_page + 1;

						//设置aim_page为0
						aim_page = 0;
						break;
					}
				}
			}
			
			for (i = 0; i < ssd->parameter->plane_die; i++)
			{
				active_block = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[i].active_block;
				if (page_place[i] != aim_page)
					make_same_level(ssd, channel, chip, die, i, active_block, aim_page);

				flash_page_state_modify(ssd, sub[i], channel, chip, die, i, active_block, aim_page);
			}
			ssd->channel_head[channel].chip_head[chip].die_head[die].token = old_plane;
		}
		else                                                                            
		{
			ssd->channel_head[channel].chip_head[chip].die_head[die].token = old_plane;
			for (i = 0; i < subs_count; i++)
				sub[i] = NULL;
			free(page_place);
			return FAILURE;
		}
	}

	//判断是否偏移地址free page一致
	/*
	if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[0].free_page != ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[1].free_page)
	{
		printf("free page don't equal\n");
		getchar();
	}*/


	//判断是否偏移地址free page一致
	/*
	if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[0].free_page != ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[1].free_page)
	{
	printf("free page don't equal\n");
	getchar();
	}*/


	gc_add = 0;
	ssd->channel_head[channel].gc_soft = 0;
	ssd->channel_head[channel].gc_hard = 0;
	for (i = 0; i < ssd->parameter->plane_die; i++)
	{
		if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[i].free_page <= (ssd->parameter->page_block*ssd->parameter->block_plane*ssd->parameter->gc_soft_threshold))
		{
			ssd->channel_head[channel].gc_soft = 1;
			gc_add = 1;
			if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[i].free_page <= (ssd->parameter->page_block*ssd->parameter->block_plane*ssd->parameter->gc_hard_threshold))
			{
				ssd->channel_head[channel].gc_hard = 1;
			}
		}
	}
	if (gc_add == 1)		//produce a gc reqeuest and add gc_node to the channel
	{

		gc_node = (struct gc_operation *)malloc(sizeof(struct gc_operation));
		alloc_assert(gc_node, "gc_node");
		memset(gc_node, 0, sizeof(struct gc_operation));
		if (ssd->channel_head[channel].gc_soft == 1)
		{
			gc_node->soft = 1;
		}
		else
		{
			gc_node->soft = 0;
		}
		if (ssd->channel_head[channel].gc_hard == 1)
		{
			gc_node->hard = 1;
		}
		else
		{
			gc_node->hard = 0;
		}
		gc_node->next_node = NULL;
		gc_node->channel = channel;
		gc_node->chip = chip;
		gc_node->die = die;
		gc_node->plane = old_plane;
		gc_node->block = 0xffffffff;
		gc_node->page = 0;
		gc_node->state = GC_WAIT;
		gc_node->priority = GC_UNINTERRUPT;
		gc_node->next_node = ssd->channel_head[channel].gc_command;
		ssd->channel_head[channel].gc_command = gc_node;					//inserted into the head of the gc chain
		ssd->gc_request++;
	}
	free(page_place);
	return SUCCESS;
}

/*************************************************************
*the function is to have two different page positions the same
**************************************************************/
struct ssd_info *make_same_level(struct ssd_info *ssd, unsigned int channel, unsigned int chip, unsigned int die, unsigned int plane, unsigned int block, unsigned int aim_page)
{
	return ssd;
}

/****************************************************************************
*this function is to calculate the processing time and the state transition 
*of the processing when processing the write request for the advanced command
*****************************************************************************/
struct ssd_info *compute_serve_time(struct ssd_info *ssd, unsigned int channel, unsigned int chip, unsigned int die, struct sub_request **subs, unsigned int subs_count, unsigned int command)
{
	unsigned int i = 0;
	struct sub_request * last_sub = NULL;
	int prog_time = 0;

	//由于写操作中，写地址命令和写数据的传输是串行的，共用一个总线
	for (i = 0; i < subs_count; i++)
	{
		subs[i]->current_state = SR_W_TRANSFER;
		if (last_sub == NULL)
		{
			subs[i]->current_time = ssd->current_time;
		}
		else
		{
			subs[i]->current_time = last_sub->complete_time + ssd->parameter->time_characteristics.tDBSY;
		}

		subs[i]->update_read_flag = 0;
		subs[i]->next_state = SR_COMPLETE;
		subs[i]->next_state_predict_time = subs[i]->current_time + 7 * ssd->parameter->time_characteristics.tWC + (subs[i]->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
		subs[i]->complete_time = subs[i]->next_state_predict_time;
		last_sub = subs[i];

		delete_from_channel(ssd, channel, subs[i]);
	}

	ssd->channel_head[channel].current_state = CHANNEL_TRANSFER;
	ssd->channel_head[channel].current_time = ssd->current_time;
	ssd->channel_head[channel].next_state = CHANNEL_IDLE;
	ssd->channel_head[channel].next_state_predict_time = (last_sub->complete_time>ssd->channel_head[channel].next_state_predict_time) ? last_sub->complete_time : ssd->channel_head[channel].next_state_predict_time;

	//不同的高级命令在写介质的时间上区别，体现在请求的最终时间和chip的时间线上
	if (command == ONE_SHOT_MUTLI_PLANE)
	{
		prog_time = ssd->parameter->time_characteristics.tPROGO;
		ssd->mutliplane_oneshot_prog_count++;
		ssd->m_plane_prog_count += 3;
	}
	else if (command == MUTLI_PLANE)
	{
		prog_time = ssd->parameter->time_characteristics.tPROG;
		ssd->m_plane_prog_count++;
	}
	else if (command == ONE_SHOT)
	{
		prog_time = ssd->parameter->time_characteristics.tPROGO;
		ssd->ontshot_prog_count++;
	}
	else if (command == NORMAL)
	{
		prog_time = ssd->parameter->time_characteristics.tPROG;
	}
	else
	{
		printf("Error! commond error\n");
		getchar();
	}

	ssd->channel_head[channel].chip_head[chip].current_state = CHIP_WRITE_BUSY;
	ssd->channel_head[channel].chip_head[chip].current_time = ssd->current_time;
	ssd->channel_head[channel].chip_head[chip].next_state = CHIP_IDLE;
	ssd->channel_head[channel].chip_head[chip].next_state_predict_time = ssd->channel_head[channel].next_state_predict_time + prog_time;

	//给当前请求加上写介质的时间
	for (i = 0; i < subs_count; i++)
	{
		subs[i]->next_state_predict_time = subs[i]->next_state_predict_time + prog_time;
		subs[i]->complete_time = subs[i]->next_state_predict_time;
		if (subs[i]->gc_flag == 1)
		{
			free(subs[i]->location);
			subs[i]->location = NULL;
			free(subs[i]);
			subs[i] = NULL;
		}
	}

	return ssd;

}


/*****************************************************************************************
*Function is to remove the request from ssd-> subs_w_head or ssd-> channel_head [channel] .subs_w_head
******************************************************************************************/
struct ssd_info *delete_from_channel(struct ssd_info *ssd, unsigned int channel, struct sub_request * sub_req)
{
	struct sub_request *sub = NULL, *p,*del_sub;

	if (ssd->parameter->allocation_scheme == DYNAMIC_ALLOCATION || ssd->parameter->allocation_scheme == HYBRID_ALLOCATION)
		sub = ssd->subs_w_head;
	else if (ssd->parameter->allocation_scheme == STATIC_ALLOCATION||ssd->parameter->allocation_scheme == SUPERBLOCK_ALLOCATION)
		sub = ssd->channel_head[channel].subs_w_head; 

	p = sub;
	while (sub != NULL)
	{
		if (sub == sub_req)
		{
			if (ssd->parameter->allocation_scheme == DYNAMIC_ALLOCATION || ssd->parameter->allocation_scheme == HYBRID_ALLOCATION)
			{
				if (sub == ssd->subs_w_head)
				{
					if (ssd->subs_w_head != ssd->subs_w_tail)
					{
						ssd->subs_w_head = sub->next_node;
						sub = ssd->subs_w_head;
						continue;
					}
					else
					{
						ssd->subs_w_head = NULL;
						ssd->subs_w_tail = NULL;
						p = NULL;
						break;
					}
				}
				else
				{
					if (sub->next_node != NULL)
					{
						p->next_node = sub->next_node;
						sub = p->next_node;
						continue;
					}
					else
					{
						ssd->subs_w_tail = p;
						ssd->subs_w_tail->next_node = NULL;
						break;
					}
				}
			}
			else if (ssd->parameter->allocation_scheme == STATIC_ALLOCATION || ssd->parameter->allocation_scheme == SUPERBLOCK_ALLOCATION)
			{
				if (sub == ssd->channel_head[channel].subs_w_head)
				{
					if (ssd->channel_head[channel].subs_w_head != ssd->channel_head[channel].subs_w_tail)
					{
						ssd->channel_head[channel].subs_w_head = sub->next_node;
						sub = ssd->channel_head[channel].subs_w_head;
						if (sub->gc_flag == 1)
						{
							del_sub=sub;
						}
						continue;
					}
					else
					{
						ssd->channel_head[channel].subs_w_head = NULL;
						ssd->channel_head[channel].subs_w_tail = NULL;
						p = NULL;
						if (sub->gc_flag == 1)
						{
							del_sub = sub;
						}
						break;
					}
				}
				else
				{
					if (sub->next_node != NULL)
					{
						p->next_node = sub->next_node;
						sub = p->next_node;
						if (sub->gc_flag == 1)
						{
							del_sub = sub;
						}
						continue;
					}
					else
					{
						ssd->channel_head[channel].subs_w_tail = p;
						ssd->channel_head[channel].subs_w_tail->next_node = NULL;
						if (sub->gc_flag == 1)
						{
							del_sub = sub;
						}
						break;
					}
				}
			}
		}
		p = sub;
		sub = sub->next_node;
	}

	return ssd;
}