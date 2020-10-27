#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>


#include "ssd.h"

int secno_num_per_page, secno_num_sub_page;
char *parameters_file ="page.parameters";
char *trace_file ="traces/fileserver64.trace";
char* result_file_statistic = "output/fileserver64.stat";
char* result_file_ex = "output/fileserver64.out";


/********************************************************************************************************************************
1，the initiation() used to initialize ssd;
2，the make_aged() used to handle old processing on ssd, Simulation used for some time ssd;
3，the pre_process_page() handle all read request in advance and established lpn<-->ppn of read request in advance ,in order to 
pre-processing trace to prevent the read request is not read the data;
4. the pre_process_write() ensure that up to a valid block contains free page, to meet the actual ssd mechanism after make_aged and pre-process()
5，the simulate() is the actual simulation handles the core functions
6，the statistic_output () outputs the information in the ssd structure to the output file, which outputs statistics and averaging data
7，the free_all_node() release the request for the entire main function
*********************************************************************************************************************************/

int main()
{
	struct ssd_info *ssd;

	//初始化ssd结构体
	ssd = (struct ssd_info*)malloc(sizeof(struct ssd_info));
	alloc_assert(ssd, "ssd");
	memset(ssd, 0, sizeof(struct ssd_info));

	//输入配置文件参数
	// strcpy_s(ssd->parameterfilename, 50, parameters_file);
	strcpy(ssd->parameterfilename, parameters_file);

	//输入trace文件参数，输出文件名
	// strcpy_s(ssd->tracefilename, 50, trace_file);
	// strcpy_s(ssd->outputfilename, 50, result_file_ex);
	// strcpy_s(ssd->statisticfilename, 50, result_file_statistic);
	strcpy(ssd->tracefilename, trace_file);
	strcpy(ssd->outputfilename, result_file_ex);
	strcpy(ssd->statisticfilename, result_file_statistic);

	printf("tracefile:%s begin simulate-------------------------\n", ssd->tracefilename);

	tracefile_sim(ssd);

	printf("tracefile:%s end simulate---------------------------\n\n\n", ssd->tracefilename);
	return 0;
}


void tracefile_sim(struct ssd_info *ssd)
{
	#ifdef DEBUG
	printf("enter main\n"); 
	#endif

	//initialize SSD
	ssd = initiation(ssd);
	warm_flash(ssd);

	//reset the static
	reset(ssd);

	ssd=simulate(ssd);
	statistic_output(ssd);  

	printf("\n");
	printf("the simulation is completed!\n");
}

void reset(struct ssd_info *ssd)
{
	unsigned int i, j;
	initialize_statistic(ssd);

	//reset the time 
	ssd->current_time = 0;
	for (i = 0; i < ssd->parameter->channel_number; i++)
	{
		ssd->channel_head[i].channel_busy_flag = 0;
		ssd->channel_head[i].current_time = 0;
		ssd->channel_head[i].current_state = CHANNEL_IDLE;
		ssd->channel_head[i].next_state_predict_time = 0;

		for (j = 0; j < ssd->channel_head[i].chip; j++)
		{
			ssd->channel_head[i].chip_head[j].current_state = CHIP_IDLE;
			ssd->channel_head[i].chip_head[j].current_time = 0;
			ssd->channel_head[i].chip_head[j].next_state_predict_time = 0;
		}
	}
	

	ssd->trace_over_flag = 0;
}

struct ssd_info *warm_flash(struct ssd_info *ssd)
{
	int flag = 1;

	ssd->warm_flash_cmplt = 0;

	printf("\n");
	printf("begin warm flash.......................\n");
	printf("\n");
	printf("\n");
	printf("   ^o^    OK, please wait a moment, and enjoy music and coffee   ^o^    \n");

	//if ((err = fopen_s(&(ssd->tracefile), ssd->tracefilename, "r")) != 0)
	ssd->tracefile = fopen(ssd->tracefilename, "rb");
	if (ssd->tracefile == NULL)
	{
		printf("the trace file can't open\n");
		return NULL;
	}

	while (flag != 100)
	{
		/*interface layer*/
		flag = get_requests(ssd);

		/*buffer layer*/
		if (flag == 1 || (flag == 0 && ssd->request_work != NULL))
		{
			if (ssd->parameter->data_dram_capacity != 0)
			{
				//printf("ssd->parameter->data_dram_capacity = %u\n", ssd->parameter->data_dram_capacity);
				if (ssd->buffer_full_flag == 0)				//buffer don't block,it can be handle.
				{
					buffer_management(ssd);
				}
			}
			else
			{
				no_buffer_distribute(ssd);
			}
			if (ssd->request_work->cmplt_flag == 1)
			{
				if (ssd->request_work != ssd->request_tail)
					ssd->request_work = ssd->request_work->next_node;
				else
					ssd->request_work = NULL;
			}

		}

		/*ftl+fcl+flash layer*/
		process(ssd);


		trace_output(ssd);

		if (flag == 0 && ssd->request_queue == NULL)
			flag = 100;
	}
	ssd->warm_flash_cmplt = 1;
	fclose(ssd->tracefile);
	return ssd;
}

/******************simulate() *********************************************************************
*Simulation () is the core processing function, the main implementation of the features include:
*1, get_requests() :Get a request from the trace file and hang to ssd-> request
*2，buffer_management()/distribute()/no_buffer_distribute() :Make read and write requests through 
the buff layer processing, resulting in read and write sub_requests,linked to ssd-> channel or ssd

*3，process() :Follow these events to handle these read and write sub_requests
*4，trace_output() :Process the completed request, and count the simulation results
**************************************************************************************************/
struct ssd_info *simulate(struct ssd_info *ssd)
{
	int flag=1;

	printf("\n");
	printf("begin simulating.......................\n");
	printf("\n");
	printf("\n");
	printf("   ^o^    OK, please wait a moment, and enjoy music and coffee   ^o^    \n");

	//if((err=fopen_s(&(ssd->tracefile),ssd->tracefilename,"r"))!=0)
	ssd->tracefile = fopen(ssd->tracefilename, "rb");
	if (ssd->tracefile == NULL)
	{  
		printf("the trace file can't open\n");
		return NULL;
	}

	while(flag!=100)      
	{        
		/*interface layer*/
		flag = get_requests(ssd); 

		/*buffer layer*/
		if (flag == 1 || (flag == 0 && ssd->request_work != NULL))
		{   
			if (ssd->parameter->data_dram_capacity !=0)
			{
				if (ssd->buffer_full_flag == 0)				//buffer don't block,it can be handle.
				{
					buffer_management(ssd);
				}
			} 
			else
			{
				no_buffer_distribute(ssd);
			}

			if (ssd->request_work->cmplt_flag == 1)
			{
				if (ssd->request_work != ssd->request_tail)
					ssd->request_work = ssd->request_work->next_node;
				else
					ssd->request_work = NULL;
			}

		}

		/*ftl+fcl+flash layer*/
		process(ssd); 

		trace_output(ssd);
	
		if (flag == 0 && ssd->request_queue == NULL)
			flag = 100;
	}

	fclose(ssd->tracefile);
	return ssd;
}


/********************************************************
*the main function :Controls the state change processing 
*of the read request and the write request
*********************************************************/
struct ssd_info *process(struct ssd_info *ssd)
{
	unsigned int i, chan;
	unsigned int flag = 0;

#ifdef DEBUG
	printf("enter process,  current time:%I64u\n", ssd->current_time);
#endif

	/*********************************************************
	*flag=0, processing read and write sub_requests
	*flag=1, processing gc request
	**********************************************************/
	for (i = 0; i<ssd->parameter->channel_number; i++)
	{
		if ((ssd->channel_head[i].subs_r_head == NULL) && (ssd->channel_head[i].subs_w_head == NULL) && (ssd->subs_w_head == NULL))
		{
			flag = 1;
		}
		else
		{
			flag = 0;
			break;
		}
	}
	if (flag == 1)
	{
		ssd->flag = 1;
		return ssd;
	}
	else
	{
		ssd->flag = 0;
	}

	/*********************************************************
	*Gc operation is completed, the read and write state changes
	**********************************************************/
	// time = ssd->current_time;
	//services_2_r_read(ssd);
	//services_2_r_complete(ssd);

	for (chan = 0; chan<ssd->parameter->channel_number; chan++)
	{
		i = chan % ssd->parameter->channel_number;																	
		ssd->channel_head[i].channel_busy_flag = 0;		
		if ((ssd->channel_head[i].current_state == CHANNEL_IDLE) || (ssd->channel_head[i].next_state == CHANNEL_IDLE&&ssd->channel_head[i].next_state_predict_time <= ssd->current_time))
		{								          
			if ((ssd->channel_head[i].channel_busy_flag == 0) && (ssd->channel_head[i].subs_r_head != NULL))					  //chg_cur_time_flag=1,current_time has changed，chg_cur_time_flag=0,current_time has not changed  			
				service_2_read(ssd, i);

			if (ssd->channel_head[i].channel_busy_flag == 0)
				services_2_write(ssd, i); 
		}
	}
	return ssd;
}



/**********************************************************************
*The trace_output () is executed after all the sub requests of each request 
*are processed by the process () 
*Print the output of the relevant results to the outputfile file
**********************************************************************/
void trace_output(struct ssd_info* ssd){
	int flag = 1;
	int64_t start_time, end_time;
	struct request *req, *pre_node;
	struct sub_request *sub, *tmp;

#ifdef DEBUG
	printf("enter trace_output,  current time:%I64u\n", ssd->current_time);
#endif

	pre_node = NULL;
	req = ssd->request_queue;
	start_time = 0;
	end_time = 0;

	if (req == NULL)
		return;
	while (req != NULL)
	{
		sub = req->subs;
		flag = 1;
		start_time = 0;
		end_time = 0;
		if (req->response_time != 0)
		{
			//fprintf(ssd->outputfile, "%16I64u %10u %6u %2u %16I64u %16I64u %10I64u\n", req->time, req->lsn, req->size, req->operation, req->begin_time, req->response_time, req->response_time - req->time);
			//fflush(ssd->outputfile);

			if (req->response_time - req->begin_time == 0)
			{
				printf("the response time is 0?? \n");
			}

			if (req->operation == READ)
			{
				ssd->read_request_count++;
				ssd->read_avg = ssd->read_avg + (req->response_time - req->time);
			}
			else
			{
				ssd->write_request_count++;
				ssd->write_avg = ssd->write_avg + (req->response_time - req->time);
			}

			if (pre_node == NULL)
			{
				if (req->next_node == NULL)
				{
					free(req);
					req = NULL;
					ssd->request_queue = NULL;
					ssd->request_tail = NULL;
					ssd->request_queue_length--;
				}
				else
				{
					ssd->request_queue = req->next_node;
					pre_node = req;
					req = req->next_node;
					free((void *)pre_node);
					pre_node = NULL;
					ssd->request_queue_length--;
				}
			}
			else
			{
				if (req->next_node == NULL)
				{
					pre_node->next_node = NULL;
					free(req);
					req = NULL;
					ssd->request_tail = pre_node;
					ssd->request_queue_length--;
				}
				else
				{
					pre_node->next_node = req->next_node;
					free((void *)req);
					req = pre_node->next_node;
					ssd->request_queue_length--;
				}
			}
		}
		else
		{
			flag = 1;
			while (sub != NULL)
			{
				if (start_time == 0)
					start_time = sub->begin_time;
				if (start_time > sub->begin_time)
					start_time = sub->begin_time;
				if (end_time < sub->complete_time)
					end_time = sub->complete_time;
				if ((sub->current_state == SR_COMPLETE) || ((sub->next_state == SR_COMPLETE) && (sub->next_state_predict_time <= ssd->current_time)))	// if any sub-request is not completed, the request is not completed
				{
					sub = sub->next_subs;
				}
				else
				{
					flag = 0;
					break;
				}

			}

			if (flag == 1)
			{
				//fprintf(ssd->outputfile, "%16I64u %10u %6u %2u %16I64u %16I64u %10I64u\n", req->time, req->lsn, req->size, req->operation, start_time, end_time, end_time - req->time);
				//fflush(ssd->outputfile);

				if (end_time - start_time == 0)
				{
					printf("the response time is 0?? \n");
					getchar();
				}

				if (req->operation == READ)
				{
					ssd->read_request_count++;
					ssd->read_avg = ssd->read_avg + (end_time - req->time);
				}
				else
				{
					ssd->write_request_count++;
					ssd->write_avg = ssd->write_avg + (end_time - req->time);
				}
				while (req->subs != NULL)
				{
					tmp = req->subs;
					req->subs = tmp->next_subs;
					if (tmp->tran_read != NULL)
					{
						free(tmp->tran_read->location);
						tmp->tran_read->location = NULL;
						free(tmp->tran_read);
						tmp->tran_read = NULL;
					}

					if (tmp->update_cnt != 0)
					{	
						delete_update(ssd, tmp);
					}
					free(tmp->location);
					tmp->location = NULL;
					free(tmp);
					tmp = NULL;

				}
				if (pre_node == NULL)
				{
					if (req->next_node == NULL)
					{
						free(req);
						req = NULL;
						ssd->request_queue = NULL;
						ssd->request_tail = NULL;
						ssd->request_queue_length--;
					}
					else
					{
						ssd->request_queue = req->next_node;
						pre_node = req;
						req = req->next_node;
						free(pre_node);
						pre_node = NULL;
						ssd->request_queue_length--;
					}
				}
				else
				{
					if (req->next_node == NULL)
					{
						pre_node->next_node = NULL;
						free(req);
						req = NULL;
						ssd->request_tail = pre_node;
						ssd->request_queue_length--;
					}
					else
					{
						pre_node->next_node = req->next_node;
						free(req);
						req = pre_node->next_node;
						ssd->request_queue_length--;
					}

				}
			}
			else
			{
				pre_node = req;
				req = req->next_node;
			}
		}
	}
}

void delete_update(struct ssd_info *ssd, struct sub_request *sub)
{
	unsigned int i;

	for (i=0;i<sub->update_cnt;i++)
	{
		free(sub->update[i]->location);
		sub->update[i]->location = NULL;
		free(sub->update[i]);
		sub->update[i] = NULL;
	}
	sub->update_cnt = 0;
}


/*******************************************************************************
*statistic_output() output processing of a request after the relevant processing information
*1，Calculate the number of erasures per plane, ie plane_erase and the total number of erasures
*2，Print min_lsn, max_lsn, read_count, program_count and other statistics to the file outputfile.
*3，Print the same information into the file statisticfile
*******************************************************************************/
void statistic_output(struct ssd_info *ssd)
{
	unsigned int i,j,k,m,p;
#ifdef DEBUG
	printf("enter statistic_output,  current time:%I64u\n",ssd->current_time);
#endif

	for(i = 0;i<ssd->parameter->channel_number;i++)
	{
		for (p = 0; p < ssd->parameter->chip_channel; p++)
		{
			for (j = 0; j < ssd->parameter->die_chip; j++)
			{
				for (k = 0; k < ssd->parameter->plane_die; k++)
				{
					for (m = 0; m < ssd->parameter->block_plane; m++)
					{
						if (ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].blk_head[m].erase_count > 0)
						{
							ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].plane_erase_count += ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].blk_head[m].erase_count;
						}

						if (ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].blk_head[m].page_read_count > 0)
						{
							ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].plane_read_count += ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].blk_head[m].page_read_count;
						}

						if (ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].blk_head[m].page_write_count > 0)
						{
							ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].plane_program_count += ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].blk_head[m].page_write_count;
						}

						if (ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].blk_head[m].pre_write_count > 0)
						{
							ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].pre_plane_write_count += ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].blk_head[m].pre_write_count;
						}
					}
					fprintf(ssd->outputfile, "the %d channel, %d chip, %d die, %d plane has : ", i, p, j, k);
					fprintf(ssd->outputfile, "%3lu erase operations,", ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].plane_erase_count);
					fprintf(ssd->outputfile, "%3lu read operations,", ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].plane_read_count);
					fprintf(ssd->outputfile, "%3lu write operations,", ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].plane_program_count);
					fprintf(ssd->outputfile, "%3lu pre_process write operations\n", ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].pre_plane_write_count);
					
					fprintf(ssd->statisticfile, "the %d channel, %d chip, %d die, %d plane has : ", i, p, j, k);
					fprintf(ssd->statisticfile, "%3lu erase operations,", ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].plane_erase_count);
					fprintf(ssd->statisticfile, "%3lu read operations,", ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].plane_read_count);
					fprintf(ssd->statisticfile, "%3lu write operations,", ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].plane_program_count);
					fprintf(ssd->statisticfile, "%3lu pre_process write operations\n", ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].pre_plane_write_count);
				}
			}
		}
	}

	fprintf(ssd->outputfile,"\n");
	fprintf(ssd->outputfile,"\n");
	fprintf(ssd->outputfile,"---------------------------statistic data---------------------------\n");	 
	fprintf(ssd->outputfile,"min lsn: %13u\n",ssd->min_lsn);	
	fprintf(ssd->outputfile,"max lsn: %13u\n",ssd->max_lsn);
	fprintf(ssd->outputfile,"sum flash read count: %13lu\n",ssd->read_count);	 
	fprintf(ssd->outputfile, "the read operation leaded by request read count: %13lu\n", ssd->req_read_count);
	fprintf(ssd->outputfile, "the request read hit count: %13lu\n", ssd->req_read_hit_cnt);
	fprintf(ssd->outputfile,"the read operation leaded by un-covered update count: %13lu\n",ssd->update_read_count);
	fprintf(ssd->outputfile, "the GC read hit count: %13lu\n", ssd->gc_read_hit_cnt);
	fprintf(ssd->outputfile, "the read operation leaded by gc read count: %13lu\n", ssd->gc_read_count);
	fprintf(ssd->outputfile, "the update read hit count: %13lu\n", ssd->update_read_hit_cnt);
	fprintf(ssd->outputfile, "\n");

	fprintf(ssd->outputfile, "program count: %13lu\n", ssd->program_count);
	fprintf(ssd->outputfile, "the write operation leaded by pre_process write count: %13lu\n", ssd->pre_all_write);
	fprintf(ssd->outputfile, "the write operation leaded by un-covered update count: %13lu\n", ssd->update_write_count);
	fprintf(ssd->outputfile, "the write operation leaded by gc read count: %13lu\n", ssd->gc_write_count);
	fprintf(ssd->outputfile, "\n");
	fprintf(ssd->outputfile,"erase count: %13lu\n",ssd->erase_count);
	fprintf(ssd->outputfile,"direct erase count: %13lu\n",ssd->direct_erase_count);
	fprintf(ssd->outputfile, "gc count: %13lu\n", ssd->gc_count);


	fprintf(ssd->outputfile, "\n");
	fprintf(ssd->outputfile, "---------------------------dftl info data---------------------------\n");
	fprintf(ssd->outputfile, "cache write hit: %13d\n", ssd->write_tran_cache_hit);
	fprintf(ssd->outputfile, "cache read hit: %13d\n", ssd->read_tran_cache_hit);
	fprintf(ssd->outputfile, "cache write miss: %13d\n", ssd->write_tran_cache_miss);
	fprintf(ssd->outputfile, "cache read miss: %13d\n", ssd->read_tran_cache_miss);

	fprintf(ssd->outputfile, "data transaction update read cnt: %13d\n", ssd->tran_update_cnt);
	fprintf(ssd->outputfile, "data req update read cnt: %13d\n", ssd->data_update_cnt);
	fprintf(ssd->outputfile, "data read cnt: %13d\n", ssd->data_read_cnt);
	fprintf(ssd->outputfile, "transaction read cnt: %13d\n", ssd->tran_read_cnt);
	fprintf(ssd->outputfile, "data program: %13d\n", ssd->data_program_cnt);
	fprintf(ssd->outputfile, "transaction proggram cnt: %13d\n", ssd->tran_program_cnt);

	fprintf(ssd->outputfile, "\n");

	fprintf(ssd->outputfile, "multi-plane program count: %13lu\n", ssd->m_plane_prog_count);
	fprintf(ssd->outputfile, "multi-plane read count: %13lu\n", ssd->m_plane_read_count);
	fprintf(ssd->outputfile, "\n");

	fprintf(ssd->outputfile, "mutli plane one shot program count : %13lu\n", ssd->mutliplane_oneshot_prog_count);
	fprintf(ssd->outputfile, "one shot program count : %13lu\n", ssd->ontshot_prog_count);
	fprintf(ssd->outputfile, "\n");

	fprintf(ssd->outputfile, "half page read count : %13lu\n", ssd->half_page_read_count);
	fprintf(ssd->outputfile, "one shot read count : %13lu\n", ssd->one_shot_read_count);
	fprintf(ssd->outputfile, "mutli plane one shot read count : %13lu\n", ssd->one_shot_mutli_plane_count);
	fprintf(ssd->outputfile, "\n");

	fprintf(ssd->outputfile, "erase suspend count : %13lu\n", ssd->suspend_count);
	fprintf(ssd->outputfile, "erase resume  count : %13lu\n", ssd->resume_count);
	fprintf(ssd->outputfile, "suspend read  count : %13lu\n", ssd->suspend_read_count);
	fprintf(ssd->outputfile, "\n");

	fprintf(ssd->outputfile, "update sub request count : %13d\n", ssd->update_sub_request);
	fprintf(ssd->outputfile,"write flash count: %13lu\n",ssd->write_flash_count);
	fprintf(ssd->outputfile, "\n");
	
	fprintf(ssd->outputfile,"read request count: %13lu\n",ssd->read_request_count);
	fprintf(ssd->outputfile,"write request count: %13lu\n",ssd->write_request_count);
	fprintf(ssd->outputfile, "\n");
	fprintf(ssd->outputfile,"read request average size: %13f\n",ssd->ave_read_size);
	fprintf(ssd->outputfile,"write request average size: %13f\n",ssd->ave_write_size);
	fprintf(ssd->outputfile, "\n");
	if (ssd->read_request_count != 0)
		fprintf(ssd->outputfile, "read request average response time: %ld\n", ssd->read_avg / ssd->read_request_count);
	if (ssd->write_request_count != 0)
		fprintf(ssd->outputfile, "write request average response time: %ld\n", ssd->write_avg / ssd->write_request_count);
	fprintf(ssd->outputfile, "\n");
	fprintf(ssd->outputfile,"buffer read hits: %13lu\n",ssd->dram->data_buffer->read_hit);
	fprintf(ssd->outputfile,"buffer read miss: %13lu\n",ssd->dram->data_buffer->read_miss_hit);
	fprintf(ssd->outputfile,"buffer write hits: %13lu\n",ssd->dram->data_buffer->write_hit);
	fprintf(ssd->outputfile,"buffer write miss: %13lu\n",ssd->dram->data_buffer->write_miss_hit);
	
	fprintf(ssd->outputfile, "update sub request count : %13u\n", ssd->update_sub_request);
	fprintf(ssd->outputfile, "half page read count : %13lu\n", ssd->half_page_read_count);
	fprintf(ssd->outputfile, "mutli plane one shot program count : %13lu\n", ssd->mutliplane_oneshot_prog_count);
	fprintf(ssd->outputfile, "one shot read count : %13lu\n", ssd->one_shot_read_count);
	fprintf(ssd->outputfile, "mutli plane one shot read count : %13lu\n", ssd->one_shot_mutli_plane_count);
	fprintf(ssd->outputfile, "erase suspend count : %13lu\n", ssd->suspend_count);
	fprintf(ssd->outputfile, "erase resume  count : %13lu\n", ssd->resume_count);
	fprintf(ssd->outputfile, "suspend read  count : %13lu\n", ssd->suspend_read_count);

	fprintf(ssd->outputfile, "\n");
	fflush(ssd->outputfile);

	//fclose(ssd->outputfile);

	fprintf(ssd->statisticfile, "\n");
	fprintf(ssd->statisticfile, "\n");
	fprintf(ssd->statisticfile, "---------------------------statistic data---------------------------\n");
	fprintf(ssd->statisticfile, "min lsn: %13d\n", ssd->min_lsn);
	fprintf(ssd->statisticfile, "max lsn: %13d\n", ssd->max_lsn);
	fprintf(ssd->statisticfile, "sum flash read count: %13lu\n", ssd->read_count);
	fprintf(ssd->statisticfile, "the read operation leaded by request read count: %13lu\n", ssd->req_read_count);
	fprintf(ssd->statisticfile, "the request read hit count: %13lu\n", ssd->req_read_hit_cnt);
	fprintf(ssd->statisticfile, "the read operation leaded by un-covered update count: %13lu\n", ssd->update_read_count);
	fprintf(ssd->statisticfile, "the GC read hit count: %13lu\n", ssd->gc_read_hit_cnt);
	fprintf(ssd->statisticfile, "the read operation leaded by gc read count: %13lu\n", ssd->gc_read_count);
	fprintf(ssd->statisticfile, "the update read hit count: %13lu\n", ssd->update_read_hit_cnt);	

	fprintf(ssd->statisticfile, "\n");
	fprintf(ssd->statisticfile, "program count: %13lu\n", ssd->program_count);
	fprintf(ssd->statisticfile, "the write operation leaded by pre_process write count: %13lu\n", ssd->pre_all_write);
	fprintf(ssd->statisticfile, "the write operation leaded by un-covered update count: %13lu\n", ssd->update_write_count);
	fprintf(ssd->statisticfile, "the write operation leaded by gc read count: %13lu\n", ssd->gc_write_count);
	fprintf(ssd->statisticfile, "\n");
	fprintf(ssd->statisticfile,"erase count: %13lu\n",ssd->erase_count);	  
	fprintf(ssd->statisticfile,"direct erase count: %13lu\n",ssd->direct_erase_count);
	fprintf(ssd->statisticfile, "gc count: %14lu\n", ssd->gc_count);
	fprintf(ssd->statisticfile, "\n");

	fprintf(ssd->statisticfile, "\n");
	fprintf(ssd->statisticfile, "---------------------------dftl info data---------------------------\n");
	fprintf(ssd->statisticfile, "cache write hit: %13d\n", ssd->write_tran_cache_hit);
	fprintf(ssd->statisticfile, "cache read hit: %13d\n", ssd->read_tran_cache_hit);
	fprintf(ssd->statisticfile, "cache write miss: %13d\n", ssd->write_tran_cache_miss);
	fprintf(ssd->statisticfile, "cache read miss: %13d\n", ssd->read_tran_cache_miss);
	fprintf(ssd->statisticfile, "data transaction update read cnt: %13d\n", ssd->tran_update_cnt);
	fprintf(ssd->statisticfile, "data req update read cnt: %13d\n", ssd->data_update_cnt);
	fprintf(ssd->statisticfile, "data read cnt: %13d\n", ssd->data_read_cnt);
	fprintf(ssd->statisticfile, "transaction read cnt: %13d\n", ssd->tran_read_cnt);
	fprintf(ssd->statisticfile, "data program: %13d\n", ssd->data_program_cnt);
	fprintf(ssd->statisticfile, "transaction proggram cnt: %13d\n", ssd->tran_program_cnt);
	fprintf(ssd->statisticfile, "\n");


	fprintf(ssd->statisticfile,"multi-plane program count: %13lu\n",ssd->m_plane_prog_count);
	fprintf(ssd->statisticfile,"multi-plane read count: %13lu\n",ssd->m_plane_read_count);
	fprintf(ssd->statisticfile, "\n");

	fprintf(ssd->statisticfile, "mutli plane one shot program count : %13lu\n", ssd->mutliplane_oneshot_prog_count);
	fprintf(ssd->statisticfile, "one shot program count : %13lu\n", ssd->ontshot_prog_count);
	fprintf(ssd->statisticfile, "\n");

	fprintf(ssd->statisticfile, "half page read count : %13lu\n", ssd->half_page_read_count);
	fprintf(ssd->statisticfile, "one shot read count : %13lu\n", ssd->one_shot_read_count);
	fprintf(ssd->statisticfile, "mutli plane one shot read count : %13lu\n", ssd->one_shot_mutli_plane_count);
	fprintf(ssd->statisticfile, "\n");

	fprintf(ssd->statisticfile, "erase suspend count : %13lu\n", ssd->suspend_count);
	fprintf(ssd->statisticfile, "erase resume  count : %13lu\n", ssd->resume_count);
	fprintf(ssd->statisticfile, "suspend read  count : %13lu\n", ssd->suspend_read_count);
	fprintf(ssd->statisticfile, "\n");

	fprintf(ssd->statisticfile, "update sub request count : %13d\n", ssd->update_sub_request);
	fprintf(ssd->statisticfile,"write flash count: %13lu\n",ssd->write_flash_count);
	fprintf(ssd->statisticfile, "\n");
	
	fprintf(ssd->statisticfile,"read request count: %13lu\n",ssd->read_request_count);
	fprintf(ssd->statisticfile, "write request count: %13lu\n", ssd->write_request_count);
	fprintf(ssd->statisticfile, "\n");
	fprintf(ssd->statisticfile,"read request average size: %13f\n",ssd->ave_read_size);
	fprintf(ssd->statisticfile,"write request average size: %13f\n",ssd->ave_write_size);
	fprintf(ssd->statisticfile, "\n");
	if (ssd->read_request_count != 0)
		fprintf(ssd->statisticfile, "read request average response time: %ld\n", ssd->read_avg / ssd->read_request_count);
	if (ssd->write_request_count != 0)
		fprintf(ssd->statisticfile, "write request average response time: %ld\n", ssd->write_avg / ssd->write_request_count);
	fprintf(ssd->statisticfile, "\n");
	fprintf(ssd->statisticfile,"buffer read hits: %13lu\n",ssd->dram->data_buffer->read_hit);
	fprintf(ssd->statisticfile,"buffer read miss: %13lu\n",ssd->dram->data_buffer->read_miss_hit);
	fprintf(ssd->statisticfile,"buffer write hits: %13lu\n",ssd->dram->data_buffer->write_hit);
	fprintf(ssd->statisticfile,"buffer write miss: %13lu\n",ssd->dram->data_buffer->write_miss_hit);
	fprintf(ssd->statisticfile, "\n");
	fflush(ssd->statisticfile);
}


/***********************************************
*free_all_node(): release all applied nodes
************************************************/
void free_all_node(struct ssd_info *ssd)
{
	unsigned int i,j,k,l,n,p;
	struct direct_erase * erase_node=NULL;

//	struct gc_operation *gc_node = NULL;

	avlTreeDestroy( ssd->dram->data_buffer);
	ssd->dram->data_buffer =NULL;
	avlTreeDestroy(ssd->dram->data_command_buffer);
	ssd->dram->data_command_buffer = NULL;
	avlTreeDestroy(ssd->dram->mapping_command_buffer);
	ssd->dram->mapping_command_buffer = NULL;

	free(ssd->dram->map->map_entry);
	ssd->dram->map->map_entry=NULL;
	free(ssd->dram->map);
	ssd->dram->map=NULL;
	free(ssd->dram->tran_map->map_entry);
	ssd->dram->tran_map->map_entry = NULL;
	free(ssd->dram->tran_map);
	ssd->dram->tran_map = NULL;
	free(ssd->dram);
	ssd->dram=NULL;

	for (p = 0; p < ssd->parameter->block_plane; p++)
	{
		free(ssd->sb_pool[p].pos);
	}
	free(ssd->sb_pool);
	ssd->sb_pool = NULL;

	for (i = 0; i < ssd->parameter->channel_number; i++)
	{
		for (j = 0; j < ssd->parameter->chip_channel; j++)
		{
			for (k = 0; k < ssd->parameter->die_chip; k++)
			{
				for (l = 0; l < ssd->parameter->plane_die; l++)
				{
					for (n = 0; n < ssd->parameter->block_plane; n++)
					{
						assert(ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[n].page_head);
						free(ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[n].page_head);
						ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[n].page_head = NULL;
					}
					assert(ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head);
					free(ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head);
					ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head = NULL;
					while (ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].erase_node != NULL)
					{
						erase_node = ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].erase_node;
						ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].erase_node = erase_node->next_node;
						assert(erase_node);
						free(erase_node);
						erase_node = NULL;
					}
				}
				assert(ssd->channel_head[i].chip_head[j].die_head[k].plane_head);
				free(ssd->channel_head[i].chip_head[j].die_head[k].plane_head);
				ssd->channel_head[i].chip_head[j].die_head[k].plane_head = NULL;
			}
			assert(ssd->channel_head[i].chip_head[j].die_head);
			free(ssd->channel_head[i].chip_head[j].die_head);
			ssd->channel_head[i].chip_head[j].die_head = NULL;
		}
		assert(ssd->channel_head[i].chip_head);
		free(ssd->channel_head[i].chip_head);
		ssd->channel_head[i].chip_head = NULL;
	}
	assert(ssd->channel_head);
	free(ssd->channel_head);
	ssd->channel_head = NULL;
	free(ssd->parameter);
	ssd->parameter = NULL;
	free(ssd);
	ssd=NULL;
}

/*****************************************************************************
*Make_aged () function of the role of death to simulate the real use of a period of time ssd,
******************************************************************************/
struct ssd_info *make_aged(struct ssd_info *ssd)
{
	unsigned int i,j,k,l,m,n;
	int threshould,flag=0;
    
	if (ssd->parameter->aged==1)
	{
		//Threshold indicates how many pages in a plane need to be set to invaild in advance
		threshould=(int)(ssd->parameter->block_plane*ssd->parameter->page_block*ssd->parameter->aged_ratio);  
		for (i=0;i<ssd->parameter->channel_number;i++)
			for (j=0;j<ssd->parameter->chip_channel;j++)
				for (k=0;k<ssd->parameter->die_chip;k++)
					for (l=0;l<ssd->parameter->plane_die;l++)
					{  
						flag=0;
						for (m=0;m<ssd->parameter->block_plane;m++)
						{  
							if (flag>=threshould)
							{
								break;
							}
							//Note that aged_ratio+1 here, that the final conditions to meet the old conditions will remain all the remaining free block.
							for (n=0;n<(ssd->parameter->page_block*ssd->parameter->aged_ratio+1);n++)
							{  
								ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].page_head[n].valid_state=0;        //Indicates that a page is invalid and both the valid and free states are 0
								ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].page_head[n].free_state=0;         //Indicates that a page is invalid and both the valid and free states are 0
								ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].page_head[n].lpn=0;				   //Setting valid_state free_state lpn to 0 means that the page is invalid
								ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].free_page_num--;
								ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].invalid_page_num++;
								ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].last_write_page++;
								ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].free_page--;
								flag++;

								//ppn=find_ppn(ssd,i,j,k,l,m,n);
							
							}
							ssd->make_age_free_page = ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].free_page_num;
						} 
					}	 
	}  
	else
	{
		return ssd;
	}

	//make_aged完成之后，进行补写操作，保证闪存只有一个活跃块，包含有效页无效页和空闲页
	pre_process_write(ssd);
	return ssd;
}



/*********************************************************************************************
*Make free block free_page all invalid, to ensure that up to a valid block contains free page, 
*to meet the actual ssd mechanism.Traverse all the blocks, when the valid block contains the 
*legacy free_page, all the free_page is invalid and the invalid block is placed in the gc chain.
*********************************************************************************************/
struct ssd_info *pre_process_write(struct ssd_info *ssd)
{
	unsigned  int i, j, k, p, m, n;
	struct direct_erase *direct_erase_node, *new_direct_erase;

	for (i = 0; i<ssd->parameter->channel_number; i++)
	{
		for (m = 0; m < ssd->parameter->chip_channel; m++)
		{
			for (j = 0; j < ssd->parameter->die_chip; j++)
			{
				for (k = 0; k < ssd->parameter->plane_die; k++)
				{
					//See if there are free blocks
					for (p = 0; p < ssd->parameter->block_plane; p++)
					{
						if ((ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].blk_head[p].free_page_num > 0) && (ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].blk_head[p].free_page_num < ssd->parameter->page_block))
						{
							if (ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].blk_head[p].free_page_num == ssd->make_age_free_page)			//The current block is invalid page, all set invalid current block
							{
								ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].free_page = ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].free_page - ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].blk_head[p].free_page_num;
								ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].blk_head[p].free_page_num = 0;
								ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].blk_head[p].invalid_page_num = ssd->parameter->page_block;
								ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].blk_head[p].last_write_page = ssd->parameter->page_block - 1;

								for (n = 0; n < ssd->parameter->page_block; n++)
								{
									ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].blk_head[p].page_head[n].valid_state = 0;        
									ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].blk_head[p].page_head[n].free_state = 0;        
									ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].blk_head[p].page_head[n].lpn = 0;  
								}
								//All pages are invalid pages, so this block is invalid and added to the gc chain
								new_direct_erase = (struct direct_erase *)malloc(sizeof(struct direct_erase));
								alloc_assert(new_direct_erase, "new_direct_erase");
								memset(new_direct_erase, 0, sizeof(struct direct_erase));

								new_direct_erase->block = p;  
								new_direct_erase->next_node = NULL;
								direct_erase_node = ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].erase_node;
								if (direct_erase_node == NULL)
								{
									ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].erase_node = new_direct_erase;
								}
								else
								{
									new_direct_erase->next_node = ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].erase_node;
									ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].erase_node = new_direct_erase;
								}
							}
							else   //There is an invalid page and a valid page in the current block
							{
								ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].free_page = ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].free_page - ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].blk_head[p].free_page_num;
								ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].blk_head[p].free_page_num = 0;
								ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].blk_head[p].invalid_page_num += ssd->parameter->page_block - (ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].blk_head[p].last_write_page + 1);
								ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].blk_head[p].last_write_page = ssd->parameter->page_block - 1;

								for (n = (ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].blk_head[p].last_write_page + 1); n < ssd->parameter->page_block; n++)
								{
									ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].blk_head[p].page_head[n].valid_state = 0;        
									ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].blk_head[p].page_head[n].free_state = 0;         
									ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].blk_head[p].page_head[n].lpn = 0;  
								}
							}
						}
						//printf("%d,0,%d,%d,%d:%5d\n", i, j, k, p, ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].blk_head[p].free_page_num);
					}
				}
			}
		}
	}
return ssd;
}

void Calculate_Energy(struct ssd_info *ssd)
{
	unsigned int program_cnt, read_cnt, erase_cnt;
	double flash_energy;

	//calculate flash memory energy
	program_cnt = ssd->program_count;
	read_cnt = ssd->read_count;
	erase_cnt = ssd->erase_count;
	flash_energy = program_cnt*PROGRAM_POWER_PAGE + read_cnt*READ_POWER_PAGE + erase_cnt*ERASE_POWER_BLOCK;

	fprintf(ssd->outputfile, "flash memory Energy      %13f\n", flash_energy);
	fclose(ssd->statisticfile);
	fclose(ssd->outputfile);
}