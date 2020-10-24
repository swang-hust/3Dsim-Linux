#include <stdlib.h>
#include <crtdbg.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <crtdbg.h>  


#include "ssd.h"
#include "initialize.h"
#include "flash.h"
#include "buffer.h"
#include "interface.h"
#include "ftl.h"
#include "fcl.h"

int secno_num_per_page, secno_num_sub_page;


//parameters路径名
char *parameters_file[3] =
{ "page.parameters",
"page_OSA.parameters",
"page_TSA.parameters"
};

//trace 路径名
char *trace_file[7] =
{
	 "fiu_web.ascii","src0_0.001.ascii", "ts0_0.001.ascii",  "hm0_0.001.ascii", "vps_0.001.ascii","wdev0_0.001.ascii", "src1_0.001.ascii"
};


char *result_file_statistic[7][10] =
{
	{ "fiu_web_0.001.dat", "fiu_web_0.01.dat", "fiu_web_0.05.dat", "fiu_web_0.1.dat", "fiu_web_0.15.dat", "fiu_web_0.2.dat", "fiu_web_0.25.dat", "fiu_web_0.3.dat", "fiu_web_0.35.dat", "fiu_web_1.dat" },
	{ "src0_0.001.dat", "src0_0.01.dat", "src0_0.05.dat", "src0_0.1.dat", "src0_0.15.dat", "src0_0.2.dat", "src0_0.25.dat", "src0_0.3.dat", "src0_0.35.dat", "src0_1.dat" },
	{ "ts0_0.001.dat", "ts0_0.01.dat", "ts0_0.05.dat", "ts0_0.1.dat", "ts0_0.15.dat", "ts0_0.2.dat", "ts0_0.25.dat", "ts0_0.3.dat", "ts0_0.35.dat", "ts0_1.dat" },
	{ "hm0_0.001.dat", "hm0_0.01.dat", "hm0_0.05.dat", "hm0_0.1.dat", "hm0_0.15.dat", "hm0_0.2.dat", "hm0_0.25.dat", "hm0_0.3.dat", "hm0_0.35.dat", "hm0_1.dat" },
	{ "vps_0.001.dat", "vps_0.01.dat", "vps_0.05.dat", "vps_0.1.dat", "vps_0.15.dat", "vps_0.2.dat", "vps_0.25.dat", "vps_0.3.dat", "vps_0.35.dat", "vps_1.dat" },
	{ "wdev0_0.001.dat", "wdev0_0.01.dat", "wdev0_0.05.dat", "wdev0_0.1.dat", "wdev0_0.15.dat", "wdev0_0.2.dat", "wdev0_0.25.dat", "wdev0_0.3.dat", "wdev0_0.35.dat", "wdev0_1.dat" },
	{ "src1_0.001.dat", "src1_0.01.dat", "src1_0.05.dat", "src1_0.1.dat", "src1_0.15.dat", "src1_0.2.dat", "src1_0.25.dat", "src1_0.3.dat", "src1_0.35.dat", "src1_0.dat" },
};


char *result_file_ex[7][10] =
{
	{ "fiu_web_0.001.date", "fiu_web_0.01.date", "fiu_web_0.05.date", "fiu_web_0.1.date", "fiu_web_0.15.date", "fiu_web_0.2.date", "fiu_web_0.25.date", "fiu_web_0.3.date", "fiu_web_0.35.date", "fiu_web_1.date" },
	{ "src0_0.001.date", "src0_0.01.date", "src0_0.05.date", "src0_0.1.date", "src0_0.15.date", "src0_0.2.date", "src0_0.25.date", "src0_0.3.date", "src0_0.35.date", "src0_1.date" },
	{ "ts0_0.001.date", "ts0_0.01.date", "ts0_0.05.date", "ts0_0.1.date", "ts0_0.15.date", "ts0_0.2.date", "ts0_0.25.date", "ts0_0.3.date", "ts0_0.35.date", "ts0_1.date" },
	{ "hm0_0.001.date", "hm0_0.01.date", "hm0_0.05.date", "hm0_0.1.date", "hm0_0.15.date", "hm0_0.2.date", "hm0_0.25.date", "hm0_0.3.date", "hm0_0.35.date", "hm0_1.date" },
	{ "vps_0.001.date", "vps_0.01.date", "vps_0.05.date", "vps_0.1.date", "vps_0.15.date", "vps_0.2.date", "vps_0.25.date", "vps_0.3.date", "vps_0.35.date", "vps_1.date" },
	{ "wdev0_0.001.date", "wdev0_0.01.date", "wdev0_0.05.date", "wdev0_0.1.date", "wdev0_0.15.date", "wdev0_0.2.date", "wdev0_0.25.date", "wdev0_0.3.date", "wdev0_0.35.date", "wdev0_1.date" },
	{ "src1_0.001.date", "src1_0.01.date", "src1_0.05.date", "src1_0.1.date", "src1_0.15.date", "src1_0.2.date", "src1_0.25.date", "src1_0.3.date", "src1_0.35.date", "src1_0.date" },
};

char *die_read_cnt[7][10] =
{
	{ "fiu_web_0.001.txt", "fiu_web_0.01.txt", "fiu_web_0.05.txt", "fiu_web_0.1.txt", "fiu_web_0.15.txt", "fiu_web_0.2.txt", "fiu_web_0.25.txt", "fiu_web_0.3.txt", "fiu_web_0.35.txt", "fiu_web_1.txt" },
	{ "src0_0.001.txt", "src0_0.01.txt", "src0_0.05.txt", "src0_0.1.txt", "src0_0.15.txt", "src0_0.2.txt", "src0_0.25.txt", "src0_0.3.txt", "src0_0.35.txt", "src0_1.txt" },
	{ "ts0_0.001.txt", "ts0_0.01.txt", "ts0_0.05.txt", "ts0_0.1.txt", "ts0_0.15.txt", "ts0_0.2.txt", "ts0_0.25.txt", "ts0_0.3.txt", "ts0_0.35.txt", "ts0_1.txt" },
	{ "hm0_0.001.txt", "hm0_0.01.txt", "hm0_0.05.txt", "hm0_0.1.txt", "hm0_0.15.txt", "hm0_0.2.txt", "hm0_0.25.txt", "hm0_0.3.txt", "hm0_0.35.txt", "hm0_1.txt" },
	{ "vps_0.001.txt", "vps_0.01.txt", "vps_0.05.txt", "vps_0.1.txt", "vps_0.15.txt", "vps_0.2.txt", "vps_0.25.txt", "vps_0.3.txt", "vps_0.35.txt", "vps_1.txt" },
	{ "wdev0_0.001.txt", "wdev0_0.01.txt", "wdev0_0.05.txt", "wdev0_0.1.txt", "wdev0_0.15.txt", "wdev0_0.2.txt", "wdev0_0.25.txt", "wdev0_0.3.txt", "wdev0_0.35.txt", "wdev0_1.txt" },
	{ "src1_0.001.txt", "src1_0.01.txt", "src1_0.05.txt", "src1_0.1.txt", "src1_0.15.txt", "src1_0.2.txt", "src1_0.25.txt", "src1_0.3.txt", "src1_0.35.txt", "src1_0.txt" },
};


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

void main()
{
	struct ssd_info *ssd;
	unsigned int i,j;

	j = 0;
	for (i = 0; i < 1; i++)
	{
		for (j = 0; j < 10; j++)
		{
			//初始化ssd结构体
			ssd = (struct ssd_info*)malloc(sizeof(struct ssd_info));
			alloc_assert(ssd, "ssd");
			memset(ssd, 0, sizeof(struct ssd_info));

			//输入配置文件参数
			strcpy_s(ssd->parameterfilename, 50, parameters_file[0]);

			//输入trace文件参数，输出文件名
			strcpy_s(ssd->tracefilename, 50, trace_file[i]);
			strcpy_s(ssd->outputfilename, 50, result_file_ex[i][j]);
			strcpy_s(ssd->statisticfilename, 50, result_file_statistic[i][j]);
			strcpy_s(ssd->die_read_req_name, 50, die_read_cnt[i][j]);

			printf("tracefile:%s begin simulate-------------------------\n", ssd->tracefilename);
			//getchar();

			//开始对当前trace进行仿真
			tracefile_sim(ssd,j);

			//仿真结束，释放所有的节点
			printf("tracefile:%s end simulate---------------------------\n\n\n", ssd->tracefilename);
			free_all_node(ssd);
		}
	}

	//所有trace跑完停止当前程序
	system("pause");
}


void tracefile_sim(struct ssd_info *ssd,unsigned int Off)
{
	unsigned  int i, j, k, p, m, n, invalid = 0;

	#ifdef DEBUG
	printf("enter main\n"); 
	#endif

	//SSD参数初始化
	ssd = initiation(ssd,Off);
	make_aged(ssd);
	warm_flash(ssd);

	//initialize the static
	reset(ssd);


	//SSD预处理建立映射表
//	pre_process_page(ssd);

	//SSD旧化方法
//	warm_flash(ssd);


#if 0
	//After the preprocessing is complete, the page offset address of each plane should be guaranteed to be consistent
	for (i=0;i<ssd->parameter->channel_number;i++)
	{
		for (m = 0; m < ssd->parameter->chip_channel[i]; m++)
		{
			for (j = 0; j < ssd->parameter->die_chip; j++)
			{
				for (k = 0; k < ssd->parameter->plane_die; k++)
				{	
					/*
					for (p = 0; p < ssd->parameter->block_plane; p++)
					{
						printf("%d,0,%d,%d,%d,%d:  %5d\n", i, m, j, k, p, ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].blk_head[p].last_write_page);
					}
					*/
					printf("free_page：%d,0,%d,%d,%d:  %5d\n", i, m, j, k, ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].free_page);
				}
			}
		}
	}

	fprintf(ssd->outputfile,"\t\t\t\t\t\t\t\t\tOUTPUT\n");
	fprintf(ssd->outputfile,"****************** TRACE INFO ******************\n");
#endif 



	ssd=simulate(ssd);

	/*
	for (i = 0; i<ssd->parameter->channel_number; i++)
	{
		for (m = 0; m < ssd->parameter->chip_channel[i]; m++)
		{
			for (j = 0; j < ssd->parameter->die_chip; j++)
			{
				for (k = 0; k < ssd->parameter->plane_die; k++)
				{

					//for (p = 0; p < ssd->parameter->block_plane; p++)
					//{
					//	printf("last_write_page：%d,0,%d,%d,%d,%d:  %5d\n", i, m, j, k, p, ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].blk_head[p].last_write_page);
					//}
					printf("free_page：%d,0,%d,%d,%d:  %5d\n", i, m, j, k, ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].free_page);
				}
			}
		}
	}*/

	statistic_output(ssd);  
	Calculate_Energy(ssd);
	//free_all_node(ssd);

	printf("\n");
	printf("the simulation is completed!\n");

	//system("pause");
 	
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
	double output_step = 0;
	unsigned int a = 0, b = 0;
	errno_t err;
	unsigned int i, j, k, m, p;


	ssd->warm_flash_cmplt = 0;

	printf("\n");
	printf("begin warm flash.......................\n");
	printf("\n");
	printf("\n");
	printf("   ^o^    OK, please wait a moment, and enjoy music and coffee   ^o^    \n");

	if ((err = fopen_s(&(ssd->tracefile), ssd->tracefilename, "r")) != 0)
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
			if (ssd->parameter->dram_capacity != 0)
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
	double output_step=0;
	unsigned int a=0,b=0;
	errno_t err;
	unsigned int i, j, k,m,p;

	printf("\n");
	printf("begin simulating.......................\n");
	printf("\n");
	printf("\n");
	printf("   ^o^    OK, please wait a moment, and enjoy music and coffee   ^o^    \n");

	//Empty the allocation token in pre_process()
	ssd->token = 0;
	for (i = 0; i < ssd->parameter->channel_number; i++)
	{
		for (j = 0; j < ssd->parameter->chip_channel[i]; j++)
		{
			for (k = 0; k < ssd->parameter->die_chip; k++)
			{
				ssd->channel_head[i].chip_head[j].die_head[k].token = 0;
			}
			ssd->channel_head[i].chip_head[j].token = 0;
		}
		ssd->channel_head[i].token = 0;
	}


	if((err=fopen_s(&(ssd->tracefile),ssd->tracefilename,"r"))!=0)
	{  
		printf("the trace file can't open\n");
		return NULL;
	}

	/*fprintf(ssd->outputfile,"      arrive           lsn     size ope     begin time    response time    process time\n");	
	fflush(ssd->outputfile);*/

	while(flag!=100)      
	{        
		/*interface layer*/
		flag = get_requests(ssd); 

		/*buffer layer*/
		if (flag == 1 || (flag == 0 && ssd->request_work != NULL))
		{   
			if (ssd->parameter->dram_capacity!=0)
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
	int old_ppn = -1, flag_die = -1;
	unsigned int i,j,k,m,p,chan, random_num;
	unsigned int flag = 0, new_write = 0, chg_cur_time_flag = 1, flag2 = 0, flag_gc = 0;
	unsigned int count1;
	int64_t time, channel_time = 0x7fffffffffffffff;

#ifdef DEBUG
	printf("enter process,  current time:%I64u\n", ssd->current_time);
#endif

	/*
	if (ssd->channel_head[0].subs_r_head == NULL && ssd->channel_head[0].subs_r_tail != NULL)
		printf("lz\n");
	*/

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
		if (ssd->gc_request>0)                                                           
		{
			gc(ssd, 0, 1);                                                                  /*Active gc, require all channels must traverse*/
		}
		return ssd;
	}
	else
	{
		ssd->flag = 0;
	}

	/*********************************************************
	*Gc operation is completed, the read and write state changes
	**********************************************************/
	time = ssd->current_time;
	//services_2_r_read(ssd);
	services_2_r_complete(ssd);

	random_num = ssd->token;
	for (chan = 0; chan<ssd->parameter->channel_number; chan++)
	{
		i = chan % ssd->parameter->channel_number;
		flag_gc = 0;																		
		ssd->channel_head[i].channel_busy_flag = 0;		
		if ((ssd->channel_head[i].current_state == CHANNEL_IDLE) || (ssd->channel_head[i].next_state == CHANNEL_IDLE&&ssd->channel_head[i].next_state_predict_time <= ssd->current_time))
		{
			//先处理是否有挂起的gc擦除操作，再处理普通的gc操作
			if (ssd->gc_request > 0)
			{
				if (ssd->channel_head[i].gc_soft == 1 && ssd->channel_head[i].gc_hard == 1 && ssd->channel_head[i].gc_command != NULL)
				{
					flag_gc = gc(ssd, i, 0);
					if (flag_gc == 1)
					{
						//ssd->channel_head[i].gc_soft = 0;
						//ssd->channel_head[i].gc_hard = 0;
						continue;
					}
				}
			}
			
			/*********************************************************
			*1.First read the wait state of the read request, 
			*2.followed by the state of the read flash
			*3.end by processing write sub_request
			**********************************************************/

			//services_2_r_wait(ssd, i);							  //flag=1,channel is busy，else idle....                  
			if ((ssd->channel_head[i].channel_busy_flag == 0) && (ssd->channel_head[i].subs_r_head != NULL))					  //chg_cur_time_flag=1,current_time has changed，chg_cur_time_flag=0,current_time has not changed  			
				//services_2_r_data_trans(ssd, i);
				service_2_read(ssd, i);

			//Write request state jump
			if (ssd->channel_head[i].channel_busy_flag == 0)
				services_2_write(ssd, i); 
		}

		/*This section is used to see if the offset addresses in the plane are the same, thus verifying the validity of our code*/
		/*
		for (j = 0; j < ssd->parameter->die_chip; j++)
		{
			for (i = 0; i<ssd->parameter->channel_number; i++)
			{
				for (m = 0; m < ssd->parameter->chip_channel[i]; m++)
				{
					for (k = 0; k < ssd->parameter->plane_die; k++)
					{
						for (p = 0; p < ssd->parameter->block_plane; p++)
						{
							if ((ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].blk_head[p].free_page_num > 0) && (ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].blk_head[p].free_page_num < ssd->parameter->page_block))
							{
								printf("%d %d %d %d %d,%5d\n", i, m, j, k, p, ssd->channel_head[i].chip_head[m].die_head[j].plane_head[k].blk_head[p].last_write_page);
								//getchar();
							}
						}
					}
				}
			}
		}
		*/
	}
	ssd->token = (ssd->token + 1) % ssd->parameter->channel_number;

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
	struct sub_request *sub, *tmp,*tmp_update;
	unsigned int chan, chip;

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
				getchar();
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
					free(req->need_distr_flag);
					req->need_distr_flag = NULL;
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
					free(pre_node->need_distr_flag);
					pre_node->need_distr_flag = NULL;
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
					free(req->need_distr_flag);
					req->need_distr_flag = NULL;
					free(req);
					req = NULL;
					ssd->request_tail = pre_node;
					ssd->request_queue_length--;
				}
				else
				{
					pre_node->next_node = req->next_node;
					free(req->need_distr_flag);
					req->need_distr_flag = NULL;
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
					
					chan = sub->location->channel;
					chip = sub->location->chip;

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
					/*
					fprintf(ssd->read_req, "delete 前：\n");
					if (Read_cnt_4_Debug(ssd) != 1)
					{
						printf("look here\n");
					}*/
					tmp = req->subs;
					req->subs = tmp->next_subs;
					if (tmp->update_cnt >0)
					{	
						delete_update(ssd, tmp);
					}
					free(tmp->location);
					tmp->location = NULL;
					free(tmp);
					tmp = NULL;
				/*
				fprintf(ssd->read_req, "delete 后：\n");
				if (Read_cnt_4_Debug(ssd) != 1)
				{
					printf("look here\n");
				}*/

				}
				if (pre_node == NULL)
				{
					if (req->next_node == NULL)
					{
						free(req->need_distr_flag);
						req->need_distr_flag = NULL;
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
						free(pre_node->need_distr_flag);
						pre_node->need_distr_flag = NULL;
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
						free(req->need_distr_flag);
						req->need_distr_flag = NULL;
						free(req);
						req = NULL;
						ssd->request_tail = pre_node;
						ssd->request_queue_length--;
					}
					else
					{
						pre_node->next_node = req->next_node;
						free(req->need_distr_flag);
						req->need_distr_flag = NULL;
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
	for (i = 0; i < sub->update_cnt; i++)
	{
		switch (i)
		{
		case 0:
			if (sub->update_0->next_state != SR_COMPLETE)
			{
				printf("LOOK HERE!\n");
				getchar();
			}

			free(sub->update_0->location);
			sub->update_0->location = NULL;
			free(sub->update_0);
			sub->update_0 = NULL;
			break;
		case 1:
			free(sub->update_1->location);
			sub->update_1->location = NULL;
			free(sub->update_1);
			sub->update_1 = NULL;
			break;
		case 2:
			free(sub->update_2->location);
			sub->update_2->location = NULL;
			free(sub->update_2);
			sub->update_2 = NULL;
			break;
		case 3:
			free(sub->update_3->location);
			sub->update_3->location = NULL;
			free(sub->update_3);
			sub->update_3 = NULL;
			break;
		case 4:
			free(sub->update_4->location);
			sub->update_4->location = NULL;
			free(sub->update_4);
			sub->update_4 = NULL;
			break;
		case 5:
			free(sub->update_5->location);
			sub->update_5->location = NULL;
			free(sub->update_5);
			sub->update_5 = NULL;
			break;
		case 6:
			free(sub->update_6->location);
			sub->update_6->location = NULL;
			free(sub->update_6);
			sub->update_6 = NULL;
			break;
		case 7:
			free(sub->update_7->location);
			sub->update_7->location = NULL;
			free(sub->update_7);
			sub->update_7 = NULL;
			break;
		case 8:
			free(sub->update_8->location);
			sub->update_8->location = NULL;
			free(sub->update_8);
			sub->update_8 = NULL;
			break;
		case 9:
			free(sub->update_9->location);
			sub->update_9->location = NULL;
			free(sub->update_9);
			sub->update_9 = NULL;
			break;
		case 10:
			free(sub->update_10->location);
			sub->update_10->location = NULL;
			free(sub->update_10);
			sub->update_10 = NULL;
			break;
		case 11:
			free(sub->update_11->location);
			sub->update_11->location = NULL;
			free(sub->update_11);
			sub->update_11 = NULL;
			break;
		case 12:
			free(sub->update_12->location);
			sub->update_12->location = NULL;
			free(sub->update_12);
			sub->update_12 = NULL;
			break;
		case 13:
			free(sub->update_13->location);
			sub->update_13->location = NULL;
			free(sub->update_13);
			sub->update_13 = NULL;
			break;
		case 14:
			free(sub->update_14->location);
			sub->update_14->location = NULL;
			free(sub->update_14);
			sub->update_14 = NULL;
			break;
		case 15:
			free(sub->update_15->location);
			sub->update_15->location = NULL;
			free(sub->update_15);
			sub->update_15 = NULL;
			break;
		case 16:
			free(sub->update_16->location);
			sub->update_16->location = NULL;
			free(sub->update_16);
			sub->update_16 = NULL;
			break;
		case 17:
			free(sub->update_17->location);
			sub->update_17->location = NULL;
			free(sub->update_17);
			sub->update_17 = NULL;
			break;
		case 18:
			free(sub->update_18->location);
			sub->update_18->location = NULL;
			free(sub->update_18);
			sub->update_18 = NULL;
			break;
		case 19:
			free(sub->update_19->location);
			sub->update_19->location = NULL;
			free(sub->update_19);
			sub->update_19 = NULL;
			break;
		case 20:
			free(sub->update_20->location);
			sub->update_20->location = NULL;
			free(sub->update_20);
			sub->update_20 = NULL;
			break;
		case 21:
			free(sub->update_21->location);
			sub->update_21->location = NULL;
			free(sub->update_21);
			sub->update_21 = NULL;
			break;
		case 22:
			free(sub->update_22->location);
			sub->update_22->location = NULL;
			free(sub->update_22);
			sub->update_22 = NULL;
			break;
		case 23:
			free(sub->update_23->location);
			sub->update_23->location = NULL;
			free(sub->update_23);
			sub->update_23 = NULL;
			break;
		case 24:
			free(sub->update_24->location);
			sub->update_24->location = NULL;
			free(sub->update_24);
			sub->update_24 = NULL;
			break;
		case 25:
			free(sub->update_25->location);
			sub->update_25->location = NULL;
			free(sub->update_25);
			sub->update_25 = NULL;
			break;
		case 26:
			free(sub->update_26->location);
			sub->update_26->location = NULL;
			free(sub->update_26);
			sub->update_26 = NULL;
			break;
		case 27:
			free(sub->update_27->location);
			sub->update_27->location = NULL;
			free(sub->update_27);
			sub->update_27 = NULL;
			break;
		case 28:
			free(sub->update_28->location);
			sub->update_28->location = NULL;
			free(sub->update_28);
			sub->update_28 = NULL;
			break;
		case 29:
			free(sub->update_29->location);
			sub->update_29->location = NULL;
			free(sub->update_29);
			sub->update_29 = NULL;
			break;
		case 30:
			free(sub->update_30->location);
			sub->update_30->location = NULL;
			free(sub->update_30);
			sub->update_30 = NULL;
			break;
		case 31:
			free(sub->update_31->location);
			sub->update_31->location = NULL;
			free(sub->update_31);
			sub->update_31 = NULL;
			break;
		default:

			break;
		}
	}
}


/*******************************************************************************
*statistic_output() output processing of a request after the relevant processing information
*1，Calculate the number of erasures per plane, ie plane_erase and the total number of erasures
*2，Print min_lsn, max_lsn, read_count, program_count and other statistics to the file outputfile.
*3，Print the same information into the file statisticfile
*******************************************************************************/
void statistic_output(struct ssd_info *ssd)
{
	unsigned int lpn_count=0,i,j,k,m,p,erase=0,plane_erase=0;
	unsigned int blk_read = 0, plane_read = 0;
	unsigned int blk_write = 0, plane_write = 0;
	unsigned int pre_plane_write = 0;
	double gc_energy=0.0;
#ifdef DEBUG
	printf("enter statistic_output,  current time:%I64u\n",ssd->current_time);
#endif

	for(i = 0;i<ssd->parameter->channel_number;i++)
	{
		for (p = 0; p < ssd->parameter->chip_channel[i]; p++)
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
					fprintf(ssd->outputfile, "%3d erase operations,", ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].plane_erase_count);
					fprintf(ssd->outputfile, "%3d read operations,", ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].plane_read_count);
					fprintf(ssd->outputfile, "%3d write operations,", ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].plane_program_count);
					fprintf(ssd->outputfile, "%3d pre_process write operations\n", ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].pre_plane_write_count);
					
					fprintf(ssd->statisticfile, "the %d channel, %d chip, %d die, %d plane has : ", i, p, j, k);
					fprintf(ssd->statisticfile, "%3d erase operations,", ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].plane_erase_count);
					fprintf(ssd->statisticfile, "%3d read operations,", ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].plane_read_count);
					fprintf(ssd->statisticfile, "%3d write operations,", ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].plane_program_count);
					fprintf(ssd->statisticfile, "%3d pre_process write operations\n", ssd->channel_head[i].chip_head[p].die_head[j].plane_head[k].pre_plane_write_count);
				}
			}
		}
	}

	fprintf(ssd->outputfile,"\n");
	fprintf(ssd->outputfile,"\n");
	fprintf(ssd->outputfile,"---------------------------statistic data---------------------------\n");	 
	fprintf(ssd->outputfile,"min lsn: %13d\n",ssd->min_lsn);	
	fprintf(ssd->outputfile,"max lsn: %13d\n",ssd->max_lsn);
	fprintf(ssd->outputfile,"sum flash read count: %13d\n",ssd->read_count);	 
	fprintf(ssd->outputfile, "the read operation leaded by request read count: %13d\n", ssd->req_read_count);
	fprintf(ssd->outputfile, "the request read hit count: %13d\n", ssd->req_read_hit_cnt);
	fprintf(ssd->outputfile,"the read operation leaded by un-covered update count: %13d\n",ssd->update_read_count);
	fprintf(ssd->outputfile, "the GC read hit count: %13d\n", ssd->gc_read_hit_cnt);
	fprintf(ssd->outputfile, "the read operation leaded by gc read count: %13d\n", ssd->gc_read_count);
	fprintf(ssd->outputfile, "the update read hit count: %13d\n", ssd->update_read_hit_cnt);
	fprintf(ssd->outputfile, "\n");

	fprintf(ssd->outputfile, "program count: %13d\n", ssd->program_count);
	fprintf(ssd->outputfile, "the write operation leaded by pre_process write count: %13d\n", ssd->pre_all_write);
	fprintf(ssd->outputfile, "the write operation leaded by un-covered update count: %13d\n", ssd->update_write_count);
	fprintf(ssd->outputfile, "the write operation leaded by gc read count: %13d\n", ssd->gc_write_count);
	fprintf(ssd->outputfile, "\n");
	fprintf(ssd->outputfile,"erase count: %13d\n",ssd->erase_count);
	fprintf(ssd->outputfile,"direct erase count: %13d\n",ssd->direct_erase_count);
	fprintf(ssd->outputfile, "gc count: %13d\n", ssd->gc_count);


	fprintf(ssd->outputfile, "\n");
	fprintf(ssd->outputfile, "---------------------------dftl info data---------------------------\n");
	fprintf(ssd->outputfile, "cache  hit: %13d\n", ssd->cache_hit);
	fprintf(ssd->outputfile, "cache  miss: %13d\n", ssd->evict);
	fprintf(ssd->outputfile, "cache write hit: %13d\n", ssd->write_cache_hit_num);
	fprintf(ssd->outputfile, "cache read hit: %13d\n", ssd->read_cache_hit_num);
	fprintf(ssd->outputfile, "cache write miss: %13d\n", ssd->write_evict);
	fprintf(ssd->outputfile, "cache read miss: %13d\n", ssd->read_evict);
	fprintf(ssd->outputfile, "data transaction update read cnt: %13d\n", ssd->tran_update_cnt);
	fprintf(ssd->outputfile, "data req update read cnt: %13d\n", ssd->data_update_cnt);
	fprintf(ssd->outputfile, "data read cnt: %13d\n", ssd->data_read_cnt);
	fprintf(ssd->outputfile, "transaction read cnt: %13d\n", ssd->tran_read_cnt);
	fprintf(ssd->outputfile, "data program: %13d\n", ssd->data_program_cnt);
	fprintf(ssd->outputfile, "transaction proggram cnt: %13d\n", ssd->tran_program_cnt);

	fprintf(ssd->outputfile, "\n");

	fprintf(ssd->outputfile, "multi-plane program count: %13d\n", ssd->m_plane_prog_count);
	fprintf(ssd->outputfile, "multi-plane read count: %13d\n", ssd->m_plane_read_count);
	fprintf(ssd->outputfile, "\n");

	fprintf(ssd->outputfile, "mutli plane one shot program count : %13d\n", ssd->mutliplane_oneshot_prog_count);
	fprintf(ssd->outputfile, "one shot program count : %13d\n", ssd->ontshot_prog_count);
	fprintf(ssd->outputfile, "\n");

	fprintf(ssd->outputfile, "half page read count : %13d\n", ssd->half_page_read_count);
	fprintf(ssd->outputfile, "one shot read count : %13d\n", ssd->one_shot_read_count);
	fprintf(ssd->outputfile, "mutli plane one shot read count : %13d\n", ssd->one_shot_mutli_plane_count);
	fprintf(ssd->outputfile, "\n");

	fprintf(ssd->outputfile, "erase suspend count : %13d\n", ssd->suspend_count);
	fprintf(ssd->outputfile, "erase resume  count : %13d\n", ssd->resume_count);
	fprintf(ssd->outputfile, "suspend read  count : %13d\n", ssd->suspend_read_count);
	fprintf(ssd->outputfile, "\n");

	fprintf(ssd->outputfile, "update sub request count : %13d\n", ssd->update_sub_request);
	fprintf(ssd->outputfile,"write flash count: %13d\n",ssd->write_flash_count);
	fprintf(ssd->outputfile, "\n");
	
	fprintf(ssd->outputfile,"read request count: %13d\n",ssd->read_request_count);
	fprintf(ssd->outputfile,"write request count: %13d\n",ssd->write_request_count);
	fprintf(ssd->outputfile, "\n");
	fprintf(ssd->outputfile,"read request average size: %13f\n",ssd->ave_read_size);
	fprintf(ssd->outputfile,"write request average size: %13f\n",ssd->ave_write_size);
	fprintf(ssd->outputfile, "\n");
	if (ssd->read_request_count != 0)
		fprintf(ssd->outputfile, "read request average response time: %16I64u\n", ssd->read_avg / ssd->read_request_count);
	if (ssd->write_request_count != 0)
		fprintf(ssd->outputfile, "write request average response time: %16I64u\n", ssd->write_avg / ssd->write_request_count);
	fprintf(ssd->outputfile, "\n");
	fprintf(ssd->outputfile,"buffer read hits: %13d\n",ssd->dram->buffer->read_hit);
	fprintf(ssd->outputfile,"buffer read miss: %13d\n",ssd->dram->buffer->read_miss_hit);
	fprintf(ssd->outputfile,"buffer write hits: %13d\n",ssd->dram->buffer->write_hit);
	fprintf(ssd->outputfile,"buffer write miss: %13d\n",ssd->dram->buffer->write_miss_hit);
	
	fprintf(ssd->outputfile, "update sub request count : %13d\n", ssd->update_sub_request);
	fprintf(ssd->outputfile, "half page read count : %13d\n", ssd->half_page_read_count);
	fprintf(ssd->outputfile, "mutli plane one shot program count : %13d\n", ssd->mutliplane_oneshot_prog_count);
	fprintf(ssd->outputfile, "one shot read count : %13d\n", ssd->one_shot_read_count);
	fprintf(ssd->outputfile, "mutli plane one shot read count : %13d\n", ssd->one_shot_mutli_plane_count);
	fprintf(ssd->outputfile, "erase suspend count : %13d\n", ssd->suspend_count);
	fprintf(ssd->outputfile, "erase resume  count : %13d\n", ssd->resume_count);
	fprintf(ssd->outputfile, "suspend read  count : %13d\n", ssd->suspend_read_count);

	fprintf(ssd->outputfile, "\n");
	fflush(ssd->outputfile);

	//fclose(ssd->outputfile);


	fprintf(ssd->statisticfile, "\n");
	fprintf(ssd->statisticfile, "\n");
	fprintf(ssd->statisticfile, "---------------------------statistic data---------------------------\n");
	fprintf(ssd->statisticfile, "min lsn: %13d\n", ssd->min_lsn);
	fprintf(ssd->statisticfile, "max lsn: %13d\n", ssd->max_lsn);
	fprintf(ssd->statisticfile, "sum flash read count: %13d\n", ssd->read_count);
	fprintf(ssd->statisticfile, "the read operation leaded by request read count: %13d\n", ssd->req_read_count);
	fprintf(ssd->statisticfile, "the request read hit count: %13d\n", ssd->req_read_hit_cnt);
	fprintf(ssd->statisticfile, "the read operation leaded by un-covered update count: %13d\n", ssd->update_read_count);
	fprintf(ssd->statisticfile, "the GC read hit count: %13d\n", ssd->gc_read_hit_cnt);
	fprintf(ssd->statisticfile, "the read operation leaded by gc read count: %13d\n", ssd->gc_read_count);
	fprintf(ssd->statisticfile, "the update read hit count: %13d\n", ssd->update_read_hit_cnt);	

	fprintf(ssd->statisticfile, "\n");
	fprintf(ssd->statisticfile, "program count: %13d\n", ssd->program_count);
	fprintf(ssd->statisticfile, "the write operation leaded by pre_process write count: %13d\n", ssd->pre_all_write);
	fprintf(ssd->statisticfile, "the write operation leaded by un-covered update count: %13d\n", ssd->update_write_count);
	fprintf(ssd->statisticfile, "the write operation leaded by gc read count: %13d\n", ssd->gc_write_count);
	fprintf(ssd->statisticfile, "\n");
	fprintf(ssd->statisticfile,"erase count: %13d\n",ssd->erase_count);	  
	fprintf(ssd->statisticfile,"direct erase count: %13d\n",ssd->direct_erase_count);
	fprintf(ssd->statisticfile, "gc count: %13d\n", ssd->gc_count);
	fprintf(ssd->statisticfile, "\n");

	fprintf(ssd->statisticfile, "\n");
	fprintf(ssd->statisticfile, "---------------------------dftl info data---------------------------\n");
	fprintf(ssd->statisticfile, "cache  hit: %13d\n", ssd->cache_hit);
	fprintf(ssd->statisticfile, "cache  miss: %13d\n", ssd->evict);
	fprintf(ssd->statisticfile, "cache write hit: %13d\n", ssd->write_cache_hit_num);
	fprintf(ssd->statisticfile, "cache read hit: %13d\n", ssd->read_cache_hit_num);
	fprintf(ssd->statisticfile, "cache write miss: %13d\n", ssd->write_evict);
	fprintf(ssd->statisticfile, "cache read miss: %13d\n", ssd->read_evict);
	fprintf(ssd->statisticfile, "data transaction update read cnt: %13d\n", ssd->tran_update_cnt);
	fprintf(ssd->statisticfile, "data req update read cnt: %13d\n", ssd->data_update_cnt);
	fprintf(ssd->statisticfile, "data read cnt: %13d\n", ssd->data_read_cnt);
	fprintf(ssd->statisticfile, "transaction read cnt: %13d\n", ssd->tran_read_cnt);
	fprintf(ssd->statisticfile, "data program: %13d\n", ssd->data_program_cnt);
	fprintf(ssd->statisticfile, "transaction proggram cnt: %13d\n", ssd->tran_program_cnt);
	fprintf(ssd->statisticfile, "\n");


	fprintf(ssd->statisticfile,"multi-plane program count: %13d\n",ssd->m_plane_prog_count);
	fprintf(ssd->statisticfile,"multi-plane read count: %13d\n",ssd->m_plane_read_count);
	fprintf(ssd->statisticfile, "\n");

	fprintf(ssd->statisticfile, "mutli plane one shot program count : %13d\n", ssd->mutliplane_oneshot_prog_count);
	fprintf(ssd->statisticfile, "one shot program count : %13d\n", ssd->ontshot_prog_count);
	fprintf(ssd->statisticfile, "\n");

	fprintf(ssd->statisticfile, "half page read count : %13d\n", ssd->half_page_read_count);
	fprintf(ssd->statisticfile, "one shot read count : %13d\n", ssd->one_shot_read_count);
	fprintf(ssd->statisticfile, "mutli plane one shot read count : %13d\n", ssd->one_shot_mutli_plane_count);
	fprintf(ssd->statisticfile, "\n");

	fprintf(ssd->statisticfile, "erase suspend count : %13d\n", ssd->suspend_count);
	fprintf(ssd->statisticfile, "erase resume  count : %13d\n", ssd->resume_count);
	fprintf(ssd->statisticfile, "suspend read  count : %13d\n", ssd->suspend_read_count);
	fprintf(ssd->statisticfile, "\n");

	fprintf(ssd->statisticfile, "update sub request count : %13d\n", ssd->update_sub_request);
	fprintf(ssd->statisticfile,"write flash count: %13d\n",ssd->write_flash_count);
	fprintf(ssd->statisticfile, "\n");
	
	fprintf(ssd->statisticfile,"read request count: %13d\n",ssd->read_request_count);
	fprintf(ssd->statisticfile, "write request count: %13d\n", ssd->write_request_count);
	fprintf(ssd->statisticfile, "\n");
	fprintf(ssd->statisticfile,"read request average size: %13f\n",ssd->ave_read_size);
	fprintf(ssd->statisticfile,"write request average size: %13f\n",ssd->ave_write_size);
	fprintf(ssd->statisticfile, "\n");
	if (ssd->read_request_count != 0)
		fprintf(ssd->statisticfile, "read request average response time: %16I64u\n", ssd->read_avg / ssd->read_request_count);
	if (ssd->write_request_count != 0)
		fprintf(ssd->statisticfile, "write request average response time: %16I64u\n", ssd->write_avg / ssd->write_request_count);
	fprintf(ssd->statisticfile, "\n");
	fprintf(ssd->statisticfile,"buffer read hits: %13d\n",ssd->dram->buffer->read_hit);
	fprintf(ssd->statisticfile,"buffer read miss: %13d\n",ssd->dram->buffer->read_miss_hit);
	fprintf(ssd->statisticfile,"buffer write hits: %13d\n",ssd->dram->buffer->write_hit);
	fprintf(ssd->statisticfile,"buffer write miss: %13d\n",ssd->dram->buffer->write_miss_hit);
	fprintf(ssd->statisticfile, "\n");
	fflush(ssd->statisticfile);
}




/***********************************************
*free_all_node(): release all applied nodes
************************************************/
void free_all_node(struct ssd_info *ssd)
{
	unsigned int i,j,k,l,n,p;
	struct buffer_group *pt=NULL;
	struct direct_erase * erase_node=NULL;

//	struct gc_operation *gc_node = NULL;


	for (i=0;i<ssd->parameter->channel_number;i++)
	{
		for (j=0;j<ssd->parameter->chip_channel[0];j++)
		{
			for (k=0;k<ssd->parameter->die_chip;k++)
			{
				for (l=0;l<ssd->parameter->plane_die;l++)
				{
					for (n=0;n<ssd->parameter->block_plane;n++)
					{
						free(ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[n].page_head);
						ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[n].page_head=NULL;
					}
					free(ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head);
					ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head=NULL;
					while(ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].erase_node!=NULL)
					{
						erase_node=ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].erase_node;
						ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].erase_node=erase_node->next_node;
						free(erase_node);
						erase_node=NULL;
					}
				}
				
				free(ssd->channel_head[i].chip_head[j].die_head[k].plane_head);
				ssd->channel_head[i].chip_head[j].die_head[k].plane_head=NULL;
			}
			free(ssd->channel_head[i].chip_head[j].die_head);
			ssd->channel_head[i].chip_head[j].die_head=NULL;
		}
		free(ssd->channel_head[i].chip_head);
		ssd->channel_head[i].chip_head=NULL;

		//free掉没有执行的gc_node
		/*
		while (ssd->channel_head[i].gc_command != NULL)
		{
			gc_node = ssd->channel_head[i].gc_command;
			ssd->channel_head[i].gc_command = gc_node->next_node;
			free(gc_node);
			gc_node = NULL;
		}*/
	}
	free(ssd->channel_head);
	ssd->channel_head=NULL;

	avlTreeDestroy( ssd->dram->buffer);
	ssd->dram->buffer=NULL;
	avlTreeDestroy(ssd->dram->command_buffer[0]);
	ssd->dram->command_buffer[0] = NULL;
	avlTreeDestroy(ssd->dram->command_buffer[1]);
	ssd->dram->command_buffer[1] = NULL;
	
	for (p = 0; p < DIE_NUMBER; p++)
	{
		avlTreeDestroy(ssd->dram->static_die_buffer[p]);
		ssd->dram->static_die_buffer[p] = NULL;
	}

	free(ssd->dram->map->map_entry);
	ssd->dram->map->map_entry=NULL;
	free(ssd->dram->map);
	ssd->dram->map=NULL;
	free(ssd->dram);
	ssd->dram=NULL;
	free(ssd->parameter);
	ssd->parameter=NULL;

	free(ssd);
	ssd=NULL;
}

#if 0
struct ssd_info *warm_flash(struct ssd_info *ssd)
{
	int flag = 1;
	double output_step = 0;
	unsigned int a = 0, b = 0;
	errno_t err;
	unsigned int i, j, k, m, p;

	//判断配置文件是否支持warm_flash
	if (ssd->parameter->warm_flash != 1){
		ssd->warm_flash_cmplt = 1;
		return ssd;
	}
	
	ssd->warm_flash_cmplt = 0;

	printf("\n");
	printf("begin warm_flash.......................\n");
	printf("\n");

	//Empty the allocation token in pre_process()
	ssd->token = 0;
	for (i = 0; i < ssd->parameter->channel_number; i++)
	{
		for (j = 0; j < ssd->parameter->chip_channel[i]; j++)
		{
			for (k = 0; k < ssd->parameter->die_chip; k++)
			{
				ssd->channel_head[i].chip_head[j].die_head[k].token = 0;
			}
			ssd->channel_head[i].chip_head[j].token = 0;
		}
		ssd->channel_head[i].token = 0;
	}

	if ((err = fopen_s(&(ssd->tracefile), ssd->tracefilename, "r")) != 0)
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
			if (flag == 0 && ssd->request_work != NULL)
				getchar();
			if (ssd->parameter->dram_capacity != 0)
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

	if (ssd->dram->buffer->buffer_sector_count != 0)
		flush_all(ssd);
	while (ssd->request_queue != NULL)
	{
		ssd->current_time = find_nearest_event(ssd);
		process(ssd);
		trace_output(ssd);
	}
	ssd->warm_flash_cmplt = 1;

	//清空warm计数参数
	initialize_statistic(ssd);
	ssd->request_work = NULL;
	ssd->dram->buffer->write_hit = 0;
	ssd->dram->buffer->write_miss_hit = 0;
	for (i = 0; i < ssd->parameter->channel_number; i++)
	{
		for (j = 0; j < ssd->parameter->chip_channel[i]; j++)
		{
			ssd->channel_head[i].chip_head[j].current_time = 0;
			ssd->channel_head[i].chip_head[j].next_state_predict_time = 0;
			ssd->channel_head[i].chip_head[j].current_state = 100;
			ssd->channel_head[i].chip_head[j].next_state = 100;
			for (k = 0; k < ssd->parameter->die_chip; k++)
			{
				for (m = 0; m < ssd->parameter->plane_die; m++)
				{
					for (p = 0; p < ssd->parameter->block_plane; p++)
					{
						ssd->channel_head[i].chip_head[j].die_head[k].plane_head[m].blk_head[p].erase_count = 0;
						ssd->channel_head[i].chip_head[j].die_head[k].plane_head[m].blk_head[p].page_read_count = 0;
						if (ssd->channel_head[i].chip_head[j].die_head[k].plane_head[m].blk_head[p].page_write_count > 0)
							ssd->channel_head[i].chip_head[j].die_head[k].plane_head[m].pre_plane_write_count += ssd->channel_head[i].chip_head[j].die_head[k].plane_head[m].blk_head[p].page_write_count;
						ssd->channel_head[i].chip_head[j].die_head[k].plane_head[m].blk_head[p].page_write_count = 0;
					}
					ssd->channel_head[i].chip_head[j].die_head[k].plane_head[m].plane_erase_count = 0;
					ssd->channel_head[i].chip_head[j].die_head[k].plane_head[m].test_gc_count = 0;
				}
			}
		}
		ssd->channel_head[i].current_time = 0;
		ssd->channel_head[i].next_state_predict_time = 0;
		ssd->channel_head[i].current_state = 0;
		ssd->channel_head[i].next_state = 0;
	}

	printf("warmflash is completed!\n");
	return ssd;
}
#endif

/*****************************************************************************
*Make_aged () function of the role of death to simulate the real use of a period of time ssd,
******************************************************************************/
struct ssd_info *make_aged(struct ssd_info *ssd)
{
	unsigned int i,j,k,l,m,n,ppn;
	int threshould,flag=0;
    
	if (ssd->parameter->aged==1)
	{
		//Threshold indicates how many pages in a plane need to be set to invaild in advance
		threshould=(int)(ssd->parameter->block_plane*ssd->parameter->page_block*ssd->parameter->aged_ratio);  
		for (i=0;i<ssd->parameter->channel_number;i++)
			for (j=0;j<ssd->parameter->chip_channel[i];j++)
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

								ppn=find_ppn(ssd,i,j,k,l,m,n);
							
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
		for (m = 0; m < ssd->parameter->chip_channel[i]; m++)
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
	double buffer_size; // mapping table + data buffer 
	unsigned int program_cnt, read_cnt, erase_cnt;
	double run_time;
	int map_entry_num;
	double entry_size;

	double dram_energy, flash_energy;

	entry_size = 32; //bit

	//calculate the DRAM power
	buffer_size = ssd->dram->dram_capacity;  //B
	switch (ssd->mapping_scheme)
	{
	case PAGE_MAPPING:
		map_entry_num = ssd->parameter->page_block*ssd->parameter->block_plane*ssd->parameter->plane_die*ssd->parameter->die_chip*ssd->parameter->chip_num/(1 +ssd->parameter->overprovide);
		break;
	case BLOCK_MAPPING:
		map_entry_num = ssd->parameter->block_plane*ssd->parameter->plane_die*ssd->parameter->die_chip*ssd->parameter->chip_num/(1 + ssd->parameter->overprovide);
		break;
	case HYBRID_MAPPING:
		break;
	case DFTL_MAPPING:
		break;
	default:
		printf("The mapping scheme isn't mentioned in this simulation \n");
	}
	if (SMART_DATA_ALLOCATION)
	{	
		switch (SB_LEVEL)
		{
		case CHANNEL_LEVEL:
			entry_size = 29;
			break;
		case CHIP_LEVEL:
			entry_size = 27;
			break;
		case DIE_LEVEL:
			entry_size = 24;
			break;
		case PLANE_LEVEL:
			entry_size = 22;
			break;
		default:
			break;
		}
	}
	buffer_size += map_entry_num * (entry_size/8);  //bit ->Byte
	run_time =  ssd->current_time;
	dram_energy = ((DRAM_POWER_32MB_LOW / (32 * MB))*buffer_size)*(run_time /S);
	fprintf(ssd->outputfile, "DRAM Energy in low power model      %13f\n", dram_energy);
	dram_energy = ((DRAM_POWER_32MB_MODERATE / (32 * MB))*buffer_size)*(run_time / S);
	fprintf(ssd->outputfile, "DRAM Energy in moderate power model      %13f\n", dram_energy);
	dram_energy = ((DRAM_POWER_32MB_HIGH / (32 * MB))*buffer_size)*(run_time / S);
	fprintf(ssd->outputfile, "DRAM Energy in high power model      %13f\n", dram_energy);

	//calculate flash memory energy
	program_cnt = ssd->program_count;
	read_cnt = ssd->read_count;
	erase_cnt = ssd->erase_count;
	flash_energy = (program_cnt*PROGRAM_POWER_PAGE + read_cnt*READ_POWER_PAGE + erase_cnt*ERASE_POWER_BLOCK)/J;

	fprintf(ssd->outputfile, "flash memory Energy      %13f\n", flash_energy);
	fclose(ssd->statisticfile);
	fclose(ssd->outputfile);
}


/************************************************
*Assert,open failed，printf“open ... error”
*************************************************/
void file_assert(int error, char *s)
{
	if (error == 0) return;
	printf("open %s error\n", s);
	getchar();
	exit(-1);
}

/*****************************************************
*Assert,malloc failed，printf“malloc ... error”
******************************************************/
void alloc_assert(void *p, char *s)
{
	if (p != NULL) return;
	printf("malloc %s error\n", s);
	getchar();
	exit(-1);
}

/*********************************************************************************
*Assert
*A，trace about time_t，device，lsn，size，ope <0 ，printf“trace error:.....”
*B，trace about time_t，device，lsn，size，ope =0，printf“probable read a blank line”
**********************************************************************************/
void trace_assert(_int64 time_t, int device, unsigned int lsn, int size, int ope)
{
	if (time_t <0 || device < 0 || lsn < 0 || size < 0 || ope < 0)
	{
		printf("trace error:%I64u %d %d %d %d\n", time_t, device, lsn, size, ope);
		getchar();
		exit(-1);
	}
	if (time_t == 0 && device == 0 && lsn == 0 && size == 0 && ope == 0)
	{
		printf("probable read a blank line\n");
		getchar();
	}
}