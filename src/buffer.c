#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

#include "ssd.h"
#include "initialize.h"
#include "flash.h"
#include "buffer.h"
#include "interface.h"
#include "ftl.h"
#include "fcl.h"

extern int secno_num_per_page, secno_num_sub_page;

/**********************************************************************************************************************************************
*Buff strategy:Blocking buff strategy
*1--first check the buffer is full, if dissatisfied, check whether the current request to put down the data, if so, put the current request,
*if not, then block the buffer;
*
*2--If buffer is blocked, select the replacement of the two ends of the page. If the two full page, then issued together to lift the buffer
*block; if a partial page 1 full page or 2 partial page, then issued a pre-read request, waiting for the completion of full page and then issued
*And then release the buffer block.
***********************************************************************************************************************************************/
struct ssd_info *buffer_management(struct ssd_info *ssd)
{
	struct request *new_request;

#ifdef DEBUG
	printf("enter buffer_management,  current time:%I64u\n", ssd->current_time);
#endif

	ssd->dram->current_time = ssd->current_time;
	new_request = ssd->request_work;   //取队列上工作指针的请求
	
	if (new_request->operation == READ)
	{
		//先处理写缓存，在处理读缓存
		handle_write_buffer(ssd, new_request);
		//handle_read_cache(ssd, new_request);
	}
	else if (new_request->operation == WRITE)
	{   
		//先处理读缓存，在处理写缓存
		//handle_read_cache(ssd, new_request);
		handle_write_buffer(ssd, new_request);
	}
	//完全命中，则计算延时
	if (new_request->subs == NULL)
	{
		new_request->begin_time = ssd->current_time;
		new_request->response_time = ssd->current_time + 1000;
	}

	new_request->cmplt_flag = 1;
	ssd->buffer_full_flag = 0;
	return ssd;
}

struct ssd_info *handle_write_buffer(struct ssd_info *ssd, struct request *req)
{
	unsigned int full_page, lsn, lpn, last_lpn, first_lpn,i;
	unsigned int mask;
	unsigned int state,offset1 = 0, offset2 = 0, flag = 0;                                                                                       

	lsn = req->lsn;//起始扇区号
	last_lpn = (req->lsn + req->size - 1) / ssd->parameter->subpage_page;
	first_lpn = req->lsn/ ssd->parameter->subpage_page;
	//last_lpn = (req->lsn + req->size - 1)/ssd->parameter->subpage_capacity;
	//first_lpn = req->lsn / ssd->parameter->subpage_capacity;
	lpn = first_lpn;

	while (lpn <= last_lpn)       //lpn值在递增
	{
		state = 0; 
		offset1 = 0;
		offset2 = ssd->parameter->subpage_page - 1;

		if (lpn == first_lpn)
			offset1 = lsn - lpn*ssd->parameter->subpage_page;
		if (lpn == last_lpn)
			offset2 = (lsn + req->size - 1) % ssd->parameter->subpage_page;

		for (i = offset1; i <= offset2; i++)
		{
			state = SET_VALID(state, i);
		}

		if (size(state) > ssd->parameter->subpage_page)
		{
			getchar();
		}

		//req->size = offset2 - offset1;
		if (req->operation == READ)                                                     //读写最小单位是page 
			ssd = check_w_buff(ssd, lpn, state, NULL, req);
		else if (req->operation == WRITE)
			ssd = insert2buffer(ssd, lpn, state, NULL, req);

		lpn++;
	}
	
	return ssd;
}

struct ssd_info *handle_read_cache(struct ssd_info *ssd, struct request *req)           //处理读缓存，待添加
{
	return ssd;
}

struct ssd_info * check_w_buff(struct ssd_info *ssd, unsigned int lpn, int state, struct sub_request *sub, struct request *req)
{
	unsigned int sub_req_state = 0, sub_req_size = 0, sub_req_lpn = 0;
	struct buffer_group *buffer_node, key;
	struct sub_request *sub_req = NULL;

	key.group = lpn;
	buffer_node = (struct buffer_group*)avlTreeFind(ssd->dram->buffer, (TREE_NODE *)&key);		// buffer node 

	if (buffer_node == NULL)         //未命中，去flash上读
	{
		sub_req = NULL;
		sub_req_state = state;
		sub_req_size = size(state);
		sub_req_lpn = lpn;
		sub_req = creat_sub_request(ssd, sub_req_lpn, sub_req_size, sub_req_state, req, READ,DATA_COMMAND_BUFFER);     //生成子请求，挂载在总请求上

		ssd->dram->buffer->read_miss_hit++;            //读命中失效次数+1
	}
	else
	{
		if ((state&buffer_node->stored) == state)   //完全命中
		{
			ssd->dram->buffer->read_hit++;
		}
		else    //部分命中（命中部分在buffer   ，其他去flash读）
		{
			sub_req = NULL;
			sub_req_state = (state | buffer_node->stored) ^ buffer_node->stored;       //^ 异或  相同为0 不同为1
			sub_req_size = size(sub_req_state);     //子请求需要读的子页的数目                通过查看sub_req_state中有几个1即需要读取几个子页
			sub_req_lpn = lpn;
			sub_req = creat_sub_request(ssd, sub_req_lpn, sub_req_size, sub_req_state, req, READ,DATA_COMMAND_BUFFER);

			ssd->dram->buffer->read_miss_hit++;
		}
	}
	return ssd;
}

/*******************************************************************************
*The function is to write data to the buffer,Called by buffer_management()
********************************************************************************/
struct ssd_info * insert2buffer_old (struct ssd_info *ssd, unsigned int lpn, int state, struct sub_request *sub, struct request *req)
{
	int write_back_count, flag = 0;                                                             /*flag表示为写入新数据腾空间是否完成，0表示需要进一步腾，1表示已经腾空*/
	unsigned int sector_count, active_region_flag = 0, free_sector = 0;
	struct buffer_group *buffer_node = NULL, *pt, *new_node = NULL, key;
	struct sub_request *sub_req = NULL, *update = NULL;

	unsigned int sub_req_state = 0, sub_req_size = 0, sub_req_lpn = 0;
	unsigned int add_size;

#ifdef DEBUG
	printf("enter insert2buffer,  current time:%I64u, lpn:%d, state:%d,\n", ssd->current_time, lpn, state);
#endif

	sector_count = size(state);                                                                /*需要写到buffer的sector个数*/
	key.group = lpn;
	buffer_node = (struct buffer_group*)avlTreeFind(ssd->dram->buffer, (TREE_NODE *)&key);    /*在平衡二叉树中寻找buffer node*/

	if (buffer_node == NULL)
	{
		free_sector = ssd->dram->buffer->max_buffer_sector - ssd->dram->buffer->buffer_sector_count;
		if (free_sector >= sector_count)
		{
			flag = 1;
		}
		if (flag == 0) //buffer is not enough and come out data replace
		{
			write_back_count = sector_count - free_sector;
			while (write_back_count>0)
			{
				sub_req = NULL;
				sub_req_state = ssd->dram->buffer->buffer_tail->stored;
				sub_req_size = size(ssd->dram->buffer->buffer_tail->stored);
				sub_req_lpn = ssd->dram->buffer->buffer_tail->group;
				//sub_req = creat_sub_request(ssd, sub_req_lpn, sub_req_size, sub_req_state, req, WRITE);
	//insert2_command_buffer_old(ssd, sub_req_lpn, sub_req_size, sub_req_state, req, WRITE);  //deal with tail sub-request
				ssd->dram->buffer->write_miss_hit++;


				ssd->dram->buffer->buffer_sector_count = ssd->dram->buffer->buffer_sector_count - sub_req_size;
				//ssd->dram->buffer->buffer_sector_count = ssd->dram->buffer->buffer_sector_count - sub_req->size;
				pt = ssd->dram->buffer->buffer_tail;
				avlTreeDel(ssd->dram->buffer, (TREE_NODE *)pt);
				if (ssd->dram->buffer->buffer_head->LRU_link_next == NULL){
					ssd->dram->buffer->buffer_head = NULL;
					ssd->dram->buffer->buffer_tail = NULL;
				}
				else{
					ssd->dram->buffer->buffer_tail = ssd->dram->buffer->buffer_tail->LRU_link_pre;
					ssd->dram->buffer->buffer_tail->LRU_link_next = NULL;
				}
				pt->LRU_link_next = NULL;
				pt->LRU_link_pre = NULL;
				AVL_TREENODE_FREE(ssd->dram->buffer, (TREE_NODE *)pt);
				pt = NULL;

				write_back_count = write_back_count - sub_req_size;                            /*因为产生了实时写回操作，需要将主动写回操作区域增加*/
				//write_back_count = write_back_count - sub_req->size;
			}
		}

		/******************************************************************************
		*生成一个buffer node，根据这个页的情况分别赋值个各个成员，添加到队首和二叉树中
		*******************************************************************************/
		new_node = NULL;
		new_node = (struct buffer_group *)malloc(sizeof(struct buffer_group));
		alloc_assert(new_node, "buffer_group_node");
		memset(new_node, 0, sizeof(struct buffer_group));

		new_node->group = lpn;
		new_node->stored = state;
		new_node->dirty_clean = state;
		new_node->LRU_link_pre = NULL;
		new_node->LRU_link_next = ssd->dram->buffer->buffer_head;
		if (ssd->dram->buffer->buffer_head != NULL){
			ssd->dram->buffer->buffer_head->LRU_link_pre = new_node;
		}
		else{
			ssd->dram->buffer->buffer_tail = new_node;
		}
		ssd->dram->buffer->buffer_head = new_node;
		new_node->LRU_link_pre = NULL;
		avlTreeAdd(ssd->dram->buffer, (TREE_NODE *)new_node);
		ssd->dram->buffer->buffer_sector_count += sector_count;
		ssd->dram->buffer->write_hit++;
	}
	else
	{
		ssd->dram->buffer->write_hit++;
		if ((state&buffer_node->stored) == state)   //完全命中
		{
			if (req != NULL)
			{
				if (ssd->dram->buffer->buffer_head != buffer_node)
				{
					if (ssd->dram->buffer->buffer_tail == buffer_node)
					{
						ssd->dram->buffer->buffer_tail = buffer_node->LRU_link_pre;
						buffer_node->LRU_link_pre->LRU_link_next = NULL;
					}
					else if (buffer_node != ssd->dram->buffer->buffer_head)
					{
						buffer_node->LRU_link_pre->LRU_link_next = buffer_node->LRU_link_next;
						buffer_node->LRU_link_next->LRU_link_pre = buffer_node->LRU_link_pre;
					}
					buffer_node->LRU_link_next = ssd->dram->buffer->buffer_head;
					ssd->dram->buffer->buffer_head->LRU_link_pre = buffer_node;
					buffer_node->LRU_link_pre = NULL;
					ssd->dram->buffer->buffer_head = buffer_node;
				}
				req->complete_lsn_count += size(state);
			}
		}
		else
		{
			add_size = size((state | buffer_node->stored) ^ buffer_node->stored);					 //需要额外写入缓存的
			while ((ssd->dram->buffer->buffer_sector_count + add_size) > ssd->dram->buffer->max_buffer_sector)
			{
				if (buffer_node == ssd->dram->buffer->buffer_tail)                  /*如果命中的节点是buffer中最后一个节点，交换最后两个节点*/
				{
					pt = ssd->dram->buffer->buffer_tail->LRU_link_pre;
					ssd->dram->buffer->buffer_tail->LRU_link_pre = pt->LRU_link_pre;
					ssd->dram->buffer->buffer_tail->LRU_link_pre->LRU_link_next = ssd->dram->buffer->buffer_tail;
					ssd->dram->buffer->buffer_tail->LRU_link_next = pt;
					pt->LRU_link_next = NULL;
					pt->LRU_link_pre = ssd->dram->buffer->buffer_tail;
					ssd->dram->buffer->buffer_tail = pt;

				}
				//写回尾节点
				sub_req = NULL;
				sub_req_state = ssd->dram->buffer->buffer_tail->stored;
				sub_req_size = size(ssd->dram->buffer->buffer_tail->stored);
				sub_req_lpn = ssd->dram->buffer->buffer_tail->group;
				//sub_req = creat_sub_request(ssd, sub_req_lpn, sub_req_size, sub_req_state, req, WRITE);
	//insert2_command_buffer_old(ssd, sub_req_lpn, sub_req_size, sub_req_state, req, WRITE);

				ssd->dram->buffer->write_miss_hit++;
				//删除尾节点
				pt = ssd->dram->buffer->buffer_tail;
				avlTreeDel(ssd->dram->buffer, (TREE_NODE *)pt);
				if (ssd->dram->buffer->buffer_head->LRU_link_next == NULL)
				{
					ssd->dram->buffer->buffer_head = NULL;
					ssd->dram->buffer->buffer_tail = NULL;
				}
				else{
					ssd->dram->buffer->buffer_tail = ssd->dram->buffer->buffer_tail->LRU_link_pre;
					ssd->dram->buffer->buffer_tail->LRU_link_next = NULL;
				}
				pt->LRU_link_next = NULL;
				pt->LRU_link_pre = NULL;
				AVL_TREENODE_FREE(ssd->dram->buffer, (TREE_NODE *)pt);
				pt = NULL;

				ssd->dram->buffer->buffer_sector_count = ssd->dram->buffer->buffer_sector_count - sub_req_size;
				//ssd->dram->buffer->buffer_sector_count = ssd->dram->buffer->buffer_sector_count - sub_req->size;
			}
			/*如果该buffer节点不在buffer的队首，需要将这个节点提到队首*/
			if (ssd->dram->buffer->buffer_head != buffer_node)
			{
				if (ssd->dram->buffer->buffer_tail == buffer_node)
				{
					buffer_node->LRU_link_pre->LRU_link_next = NULL;
					ssd->dram->buffer->buffer_tail = buffer_node->LRU_link_pre;
				}
				else
				{
					buffer_node->LRU_link_pre->LRU_link_next = buffer_node->LRU_link_next;
					buffer_node->LRU_link_next->LRU_link_pre = buffer_node->LRU_link_pre;
				}
				buffer_node->LRU_link_next = ssd->dram->buffer->buffer_head;
				ssd->dram->buffer->buffer_head->LRU_link_pre = buffer_node;
				buffer_node->LRU_link_pre = NULL;
				ssd->dram->buffer->buffer_head = buffer_node;
			}
			buffer_node->stored = buffer_node->stored | state;
			buffer_node->dirty_clean = buffer_node->dirty_clean | state;
			ssd->dram->buffer->buffer_sector_count += add_size;
		}
	}
	return ssd;
}

/*******************************************************************************
*The function is to write data to the buffer,Called by buffer_management()
********************************************************************************/
struct ssd_info * insert2buffer(struct ssd_info *ssd, unsigned int lpn, int state, struct sub_request *sub, struct request *req)
{
	int write_back_count, flag = 0;                                                             /*flag表示为写入新数据腾空间是否完成，0表示需要进一步腾，1表示已经腾空*/
	unsigned int sector_count, active_region_flag = 0, free_sector = 0;
	struct buffer_group *buffer_node = NULL, *pt, *new_node = NULL, key;
	struct sub_request *sub_req = NULL, *update = NULL;

	unsigned int sub_req_state = 0, sub_req_size = 0, sub_req_lpn = 0;
	unsigned int add_size;

#ifdef DEBUG
	printf("enter insert2buffer,  current time:%I64u, lpn:%d, state:%d,\n", ssd->current_time, lpn, state);
#endif

	sector_count = size(state);                                                                /*需要写到buffer的sector个数*/
	key.group = lpn;                //group 即一个int 
	buffer_node = (struct buffer_group*)avlTreeFind(ssd->dram->buffer, (TREE_NODE *)&key);    /*在平衡二叉树中寻找buffer node*/

	if (buffer_node == NULL)
	{
		free_sector = ssd->dram->buffer->max_buffer_sector - ssd->dram->buffer->buffer_sector_count;

		if (free_sector >= sector_count)
		{
			flag = 1;
		}
		if (flag == 0)
		{	
			write_back_count = sector_count - free_sector;
			while (write_back_count>0)
			{
				sub_req = NULL;
				sub_req_state = ssd->dram->buffer->buffer_tail->stored;
				sub_req_size = size(ssd->dram->buffer->buffer_tail->stored);
				sub_req_lpn = ssd->dram->buffer->buffer_tail->group;
				
				/*
				if (ssd->parameter->allocation_scheme == DYNAMIC_ALLOCATION)
					insert2_command_buffer(ssd, sub_req_lpn, sub_req_size, sub_req_state, req, WRITE);
				else
					sub_req = creat_sub_request(ssd, sub_req_lpn, sub_req_size, sub_req_state, req, WRITE);
				*/
				/**/
				//将请求分配到command_buffer
				//distribute2_command_buffer(ssd, sub_req_lpn, sub_req_size, sub_req_state, req, WRITE);

				if (sub_req_size > ssd->parameter->subpage_page)
				{
					getchar();
				}

				insert2_command_buffer(ssd, ssd->dram->command_buffer[DATA_COMMAND_BUFFER], sub_req_lpn, sub_req_size, sub_req_state, req, WRITE, DATA_COMMAND_BUFFER);  //deal with tail sub-request
				ssd->dram->buffer->write_miss_hit++;

				ssd->dram->buffer->buffer_sector_count = ssd->dram->buffer->buffer_sector_count - sub_req_size;
				
				pt = ssd->dram->buffer->buffer_tail;
				avlTreeDel(ssd->dram->buffer, (TREE_NODE *)pt);
				if (ssd->dram->buffer->buffer_head->LRU_link_next == NULL){
					ssd->dram->buffer->buffer_head = NULL;
					ssd->dram->buffer->buffer_tail = NULL;
				}
				else{
					ssd->dram->buffer->buffer_tail = ssd->dram->buffer->buffer_tail->LRU_link_pre;
					ssd->dram->buffer->buffer_tail->LRU_link_next = NULL;
				}
				pt->LRU_link_next = NULL;
				pt->LRU_link_pre = NULL;
				AVL_TREENODE_FREE(ssd->dram->buffer, (TREE_NODE *)pt);
				pt = NULL;

				write_back_count = write_back_count - sub_req_size;                            /*因为产生了实时写回操作，需要将主动写回操作区域增加*/
				//write_back_count = write_back_count - sub_req->size;
			}
		}

		/******************************************************************************
		*生成一个buffer node，根据这个页的情况分别赋值个各个成员，添加到队首和二叉树中
		*******************************************************************************/
		new_node = NULL;
		new_node = (struct buffer_group *)malloc(sizeof(struct buffer_group));
		alloc_assert(new_node, "buffer_group_node");
		memset(new_node, 0, sizeof(struct buffer_group));

		new_node->group = lpn;
		new_node->stored = state;
		new_node->dirty_clean = state;
		new_node->LRU_link_pre = NULL;
		new_node->LRU_link_next = ssd->dram->buffer->buffer_head;
		if (ssd->dram->buffer->buffer_head != NULL){
			ssd->dram->buffer->buffer_head->LRU_link_pre = new_node;
		}
		else{
			ssd->dram->buffer->buffer_tail = new_node;
		}
		ssd->dram->buffer->buffer_head = new_node;
		new_node->LRU_link_pre = NULL;
		avlTreeAdd(ssd->dram->buffer, (TREE_NODE *)new_node);
		ssd->dram->buffer->buffer_sector_count += sector_count;
	}
	else
	{
		ssd->dram->buffer->write_hit++;
		if ((state&buffer_node->stored) == state)   //完全命中
		{
			if (req != NULL)
			{
				if (ssd->dram->buffer->buffer_head != buffer_node)
				{
					if (ssd->dram->buffer->buffer_tail == buffer_node)
					{
						ssd->dram->buffer->buffer_tail = buffer_node->LRU_link_pre;
						buffer_node->LRU_link_pre->LRU_link_next = NULL;
					}
					else if (buffer_node != ssd->dram->buffer->buffer_head)
					{
						buffer_node->LRU_link_pre->LRU_link_next = buffer_node->LRU_link_next;
						buffer_node->LRU_link_next->LRU_link_pre = buffer_node->LRU_link_pre;
					}
					buffer_node->LRU_link_next = ssd->dram->buffer->buffer_head;
					ssd->dram->buffer->buffer_head->LRU_link_pre = buffer_node;
					buffer_node->LRU_link_pre = NULL;
					ssd->dram->buffer->buffer_head = buffer_node;
				}
				req->complete_lsn_count += size(state);                                       
			}
		}
		else
		{
			add_size = size((state | buffer_node->stored) ^ buffer_node->stored);					 //需要额外写入缓存的
			while((ssd->dram->buffer->buffer_sector_count + add_size) > ssd->dram->buffer->max_buffer_sector)
			{
				if (buffer_node == ssd->dram->buffer->buffer_tail)                  /*如果命中的节点是buffer中最后一个节点，交换最后两个节点*/
				{
					pt = ssd->dram->buffer->buffer_tail->LRU_link_pre;
					ssd->dram->buffer->buffer_tail->LRU_link_pre = pt->LRU_link_pre;
					ssd->dram->buffer->buffer_tail->LRU_link_pre->LRU_link_next = ssd->dram->buffer->buffer_tail;
					ssd->dram->buffer->buffer_tail->LRU_link_next = pt;
					pt->LRU_link_next = NULL;
					pt->LRU_link_pre = ssd->dram->buffer->buffer_tail;
					ssd->dram->buffer->buffer_tail = pt;

				}
				//写回尾节点
				sub_req = NULL;
				sub_req_state = ssd->dram->buffer->buffer_tail->stored;
				sub_req_size = size(ssd->dram->buffer->buffer_tail->stored);
				sub_req_lpn = ssd->dram->buffer->buffer_tail->group;
				/*
				if (ssd->parameter->allocation_scheme == DYNAMIC_ALLOCATION)
					insert2_command_buffer(ssd, sub_req_lpn, sub_req_size, sub_req_state, req, WRITE);
				else
					sub_req = creat_sub_request(ssd, sub_req_lpn, sub_req_size, sub_req_state, req, WRITE);*/
				
				//将请求分配到command_buffer
				insert2_command_buffer(ssd, ssd->dram->command_buffer[DATA_COMMAND_BUFFER], sub_req_lpn, sub_req_size, sub_req_state, req, WRITE, DATA_COMMAND_BUFFER);

				ssd->dram->buffer->write_miss_hit++;
				//删除尾节点
				pt = ssd->dram->buffer->buffer_tail;
				avlTreeDel(ssd->dram->buffer, (TREE_NODE *)pt);
				if (ssd->dram->buffer->buffer_head->LRU_link_next == NULL)
				{
					ssd->dram->buffer->buffer_head = NULL;
					ssd->dram->buffer->buffer_tail = NULL;
				}
				else{
					ssd->dram->buffer->buffer_tail = ssd->dram->buffer->buffer_tail->LRU_link_pre;
					ssd->dram->buffer->buffer_tail->LRU_link_next = NULL;
				}
				pt->LRU_link_next = NULL;
				pt->LRU_link_pre = NULL;
				AVL_TREENODE_FREE(ssd->dram->buffer, (TREE_NODE *)pt);
				pt = NULL;

				ssd->dram->buffer->buffer_sector_count = ssd->dram->buffer->buffer_sector_count - sub_req_size;
				//ssd->dram->buffer->buffer_sector_count = ssd->dram->buffer->buffer_sector_count - sub_req->size;
			}
			/*如果该buffer节点不在buffer的队首，需要将这个节点提到队首*/
			if (ssd->dram->buffer->buffer_head != buffer_node)                   
			{
				if (ssd->dram->buffer->buffer_tail == buffer_node)
				{
					buffer_node->LRU_link_pre->LRU_link_next = NULL;
					ssd->dram->buffer->buffer_tail = buffer_node->LRU_link_pre;
				}
				else
				{
					buffer_node->LRU_link_pre->LRU_link_next = buffer_node->LRU_link_next;
					buffer_node->LRU_link_next->LRU_link_pre = buffer_node->LRU_link_pre;
				}
				buffer_node->LRU_link_next = ssd->dram->buffer->buffer_head;
				ssd->dram->buffer->buffer_head->LRU_link_pre = buffer_node;
				buffer_node->LRU_link_pre = NULL;
				ssd->dram->buffer->buffer_head = buffer_node;
			}
			buffer_node->stored = buffer_node->stored | state;
			buffer_node->dirty_clean = buffer_node->dirty_clean | state;
			ssd->dram->buffer->buffer_sector_count += add_size;
		}
	}
	return ssd;
}


struct ssd_info * getout2buffer(struct ssd_info *ssd, struct sub_request *sub, struct request *req)
{
	return ssd;
}

/*********************************************************************************************
*The no_buffer_distribute () function is processed when ssd has no dram，
*This is no need to read and write requests in the buffer inside the search, directly use the 
*creat_sub_request () function to create sub-request, and then deal with.
*********************************************************************************************/
struct ssd_info *no_buffer_distribute(struct ssd_info *ssd)
{
	return ssd;
}

/***********************************************************************************
*According to the status of each page to calculate the number of each need to deal 
*with the number of sub-pages, that is, a sub-request to deal with the number of pages
************************************************************************************/
unsigned int size(unsigned int stored)
{
	unsigned int i, total = 0, mask = 0x80000000;

#ifdef DEBUG
	printf("enter size\n");
#endif
	for (i = 1; i <= 32; i++)
	{
		if (stored & mask) total++;     
		stored <<= 1;
	}
#ifdef DEBUG
	printf("leave size\n");
#endif
	return total;
}

struct ssd_info * distribute2_command_buffer(struct ssd_info * ssd, unsigned int lpn, int size_count, unsigned int state, struct request * req, unsigned int operation)
{

	unsigned int method_flag = 1;
	struct buffer_info * aim_command_buffer = NULL;
	struct allocation_info * allocation = NULL;

	//根据不同分配方式调用函数allocation_method分配，返回结果包括channel chip die plane aim_command_buffer 
	allocation = allocation_method(ssd, lpn, ALLOCATION_BUFFER);
    
	//将请求插入到对应的缓存中
	if (allocation->aim_command_buffer != NULL)
		insert2_command_buffer(ssd, allocation->aim_command_buffer, lpn, size_count, state, req, operation, DATA_COMMAND_BUFFER);

	//free掉地址空间
	free(allocation);
	return ssd;
}

struct allocation_info * allocation_method(struct ssd_info *ssd,unsigned int lpn,unsigned int use_flag)
{
	struct allocation_info * return_info = (struct allocation_info *)malloc(sizeof(struct allocation_info));
	unsigned int channel_num, chip_num, die_num, plane_num;
	unsigned int aim_die = 0;
	unsigned int type_flag = 0, i = 0;

	int64_t return_distance = 0;
	int64_t max_distance = 0;
	//初始化返回结构体
	return_info->plane = 0;
	return_info->channel = 0;
	return_info->chip = 0;
	return_info->die = 0;

	channel_num = ssd->parameter->channel_number;
	chip_num = ssd->parameter->chip_channel[0];
	die_num = ssd->parameter->die_chip;
	plane_num = ssd->parameter->plane_die;

	if (ssd->parameter->allocation_scheme == HYBRID_ALLOCATION)
	{	
		if (use_flag == ALLOCATION_BUFFER)
		{
			/*//使用动态的混合感知
			//1.check缓存是否命中，命中则直接分配到对应的buffer，没有命中则按照频率来分配
			switch (ssd->dram->map->map_entry[lpn].type)
			{
			case 0:
			type_flag = 1; break;
			case WRITE_MORE:
			{
			key.group = lpn;
			buffer_node = (struct buffer_group*)avlTreeFind(ssd->dram->command_buffer, (TREE_NODE *)&key);		// buffer node

			if (buffer_node != NULL)
			{
			aim_command_buffer = ssd->dram->command_buffer;
			ssd->dram->map->map_entry[lpn].type = WRITE_MORE;
			type_flag = 0;
			}
			else
			type_flag = 1;
			break;
			}
			case READ_MORE:
			{
			key.group = lpn;
			//检查所有的writeback缓存，判断是否命中
			for (i = 0; i < die_num; i++)
			{
			buffer_node = (struct buffer_group*)avlTreeFind(ssd->dram->static_die_buffer[i], (TREE_NODE *)&key);		// buffer node
			if (buffer_node != NULL)
			{
			aim_die = i;
			break;
			}
			}

			//判读是否命中，若检查所有缓存没有命中，则置1，否则置
			if (buffer_node != NULL)
			{
			aim_command_buffer = ssd->dram->static_die_buffer[aim_die];
			ssd->dram->map->map_entry[lpn].type = READ_MORE;
			type_flag = 0;
			}
			else
			type_flag = 1;
			break;
			}
			default:break;
			}

			//2.根据读写type频率的大小区分最后使用哪个分配方式
			if (type_flag == 1)
			{
			if (ssd->dram->map->map_entry[lpn].read_count >= ssd->dram->map->map_entry[lpn].write_count)
			{
			//分配到对应的plane_buffer上
			aim_die = ssd->die_token;
			aim_command_buffer = ssd->dram->static_die_buffer[aim_die];

			ssd->plane_count++;
			if (ssd->plane_count % ssd->parameter->plane_die == 0)
			{
			ssd->die_token = (ssd->die_token + 1) % DIE_NUMBER;
			ssd->plane_count = 0;
			}
			//在表中支持改lpn的类型
			ssd->dram->map->map_entry[lpn].type = READ_MORE;
			}
			else
			{
			aim_command_buffer = ssd->dram->command_buffer;
			aim_die = 4;
			ssd->dram->map->map_entry[lpn].type = WRITE_MORE;
			}
			}*/
			
			//静态的负载感知
			if (ssd->dram->map->map_entry[lpn].type == READ_MORE)
			{
				//按照poll的方式分配
				aim_die = ssd->die_token;
				return_info->aim_command_buffer = ssd->dram->static_die_buffer[aim_die];
				ssd->plane_count++;
				if (ssd->plane_count % ssd->parameter->plane_die == 0)
				{
					ssd->die_token = (ssd->die_token + 1) % DIE_NUMBER;
					ssd->plane_count = 0;
				}
				ssd->dram->map->map_entry[lpn].mount_type = aim_die;

			}
			else if (ssd->dram->map->map_entry[lpn].type == WRITE_MORE)
			{
				//按照聚集的方式分配
				return_info->aim_command_buffer = ssd->dram->command_buffer;
				ssd->dram->map->map_entry[lpn].mount_type = 4;
			}
			else
			{
				printf("aware workloads error!\n");
				getchar();
			}
		}
		else if (use_flag == ALLOCATION_MOUNT){
			//计算mount_flag
			switch (ssd->dram->map->map_entry[lpn].mount_type)
			{
				case 0:
				{
					return_info->channel = 0;
					return_info->chip = 0;
					return_info->die = 0;
					break;
				}
				case 1:
				{
					return_info->channel = 0;
					return_info->chip = 1;
					return_info->die = 0;
					break;
				}
				case 2:
				{
					return_info->channel = 1;
					return_info->chip = 0;
					return_info->die = 0;
					break;
				}
				case 3:
				{
					return_info->channel = 1;
					return_info->chip = 1;
					return_info->die = 0;
					break;
				}
				case 4:
				{
					return_info->channel = -1;
					return_info->chip = -1;
					return_info->die = -1;
					return_info->plane = -1;
					break;
				}
					default:break;
				}
			return_info->mount_flag = SSD_MOUNT;
		}
	}
	else if (ssd->parameter->allocation_scheme == STATIC_ALLOCATION)
	{
		//静态分配优先级,由于以die级为缓存，故这里只需要分配到die级即可，即只考虑die与channel/chip之间的并行情况
		if (ssd->parameter->flash_mode == TLC_MODE)
		{
			if (ssd->parameter->static_allocation == PLANE_STATIC_ALLOCATION || ssd->parameter->static_allocation == SUPERPAGE_STATIC_ALLOCATION)
			{
				return_info->channel = (lpn / (plane_num*PAGE_INDEX)) % channel_num;
				return_info->chip = (lpn / (plane_num*PAGE_INDEX*channel_num)) % chip_num;
				return_info->die = (lpn / (plane_num*PAGE_INDEX*channel_num*chip_num)) % die_num;
			}
			else if (ssd->parameter->static_allocation == CHANNEL_PLANE_STATIC_ALLOCATION || ssd->parameter->static_allocation == CHANNEL_SUPERPAGE_STATIC_ALLOCATION)
			{
				return_info->channel = lpn % channel_num;
				return_info->chip = (lpn / channel_num) % chip_num;
				return_info->die = (lpn / (plane_num*PAGE_INDEX*channel_num*chip_num)) % die_num;
			}
		}
		else if (ssd->parameter->flash_mode == SLC_MODE)
		{
			if (ssd->parameter->static_allocation == PLANE_STATIC_ALLOCATION || ssd->parameter->static_allocation == SUPERPAGE_STATIC_ALLOCATION)
			{
				return_info->plane = lpn % plane_num;
				return_info->channel = (lpn / plane_num) % channel_num;
				return_info->chip = (lpn / (plane_num*channel_num)) % chip_num;
				return_info->die = (lpn / (plane_num*channel_num*chip_num)) % die_num;

			}
			else if (ssd->parameter->static_allocation == CHANNEL_PLANE_STATIC_ALLOCATION || ssd->parameter->static_allocation == CHANNEL_SUPERPAGE_STATIC_ALLOCATION)
			{
				return_info->channel = lpn % channel_num;
				return_info->chip = (lpn / channel_num) % chip_num;
				return_info->plane = (lpn / (chip_num*channel_num)) % plane_num;
				return_info->die = (lpn / (plane_num*channel_num*chip_num)) % die_num;
			}
		}

		//分配到对应的plane_buffer上
		aim_die = return_info->channel * (die_num*chip_num) + return_info->chip * die_num + return_info->die;
		return_info->aim_command_buffer = ssd->dram->static_die_buffer[aim_die];
		return_info->mount_flag = CHANNEL_MOUNT;
	}
	else if (ssd->parameter->allocation_scheme == DYNAMIC_ALLOCATION)
	{
		if (use_flag == ALLOCATION_BUFFER)
		{
			if (ssd->parameter->dynamic_allocation == STRIPE_DYNAMIC_ALLOCATION)           //优先级
			{
				//按照替换的顺序，轮询分配到每个die_buffer上面
				aim_die = ssd->die_token;
				return_info->aim_command_buffer = ssd->dram->static_die_buffer[aim_die];

				//ssd->die_token = (ssd->die_token + 1) % DIE_NUMBER;

				ssd->plane_count++;
				if (ssd->plane_count % ssd->parameter->plane_die == 0)
				{
					ssd->die_token = (ssd->die_token + 1) % DIE_NUMBER;
					ssd->plane_count = 0;
				}
			}
			else if (ssd->parameter->dynamic_allocation == OSPA_DYNAMIC_ALLOCATION)
			{
				aim_die = 0;
				//进入这个函数之前，die_buffer里面已经有缓存的数据。
				for (i = 0; i < DIE_NUMBER; i++)
				{
					//如果当前buffer是空的，则直接写入
					if (ssd->dram->static_die_buffer[i]->buffer_head == NULL)
					{
						aim_die = i;
						break;
					}

					//遍历所有的die_buffer并计算欧式距离，返回对应die中最小的欧式距离
					return_distance = calculate_distance(ssd, ssd->dram->static_die_buffer[i], lpn);
					if (max_distance <= return_distance)
					{
						max_distance = return_distance;
						aim_die = i;
					}
				}
				return_info->aim_command_buffer = ssd->dram->static_die_buffer[aim_die];
			}
			else if (ssd->parameter->dynamic_allocation == POLL_DISTRANCE_ALLOCATION)
			{
				//首先获取轮询的分配的令牌
				aim_die = ssd->die_token;
				if (ssd->plane_count == 0)				//重新轮询到一个新的die上
				{
					if (ssd->dram->static_die_buffer[aim_die]->buffer_head != NULL)
					{
						//计算当前aim-die的距离是否等于1
						return_distance = calculate_distance(ssd, ssd->dram->static_die_buffer[aim_die], lpn);
						if (return_distance >= 1 && return_distance <= 2)	//跳过当前die
						{
							ssd->die_token = (ssd->die_token + 1) % DIE_NUMBER;
							aim_die = ssd->die_token;
						}
					}
				}

				//确定好分配的缓存
				return_info->aim_command_buffer = ssd->dram->static_die_buffer[aim_die];

				//ssd->die_token = (ssd->die_token + 1) % DIE_NUMBER;

				//连续的两个页分配到相同的die缓存上
				ssd->plane_count++;
				if (ssd->plane_count % ssd->parameter->plane_die == 0)
				{
					ssd->die_token = (ssd->die_token + 1) % DIE_NUMBER;
					ssd->plane_count = 0;
				}
			}
			else{
				return_info->aim_command_buffer = ssd->dram->command_buffer;
			}
		}
		else if (use_flag == ALLOCATION_MOUNT){
			return_info->channel = -1;
			return_info->chip = -1;
			return_info->die = -1;
			return_info->plane = -1;
			return_info->mount_flag = SSD_MOUNT;
		}
	}
	return return_info;
}

int64_t calculate_distance(struct ssd_info * ssd, struct buffer_info * die_buffer, unsigned int lpn)
{
	struct buffer_group *page_node = NULL;
	int64_t lpn_buffer;
	int64_t distance;
	int64_t min_distance = 0x7fffffffffffffff;    //最大值

	//从head开始遍历计算欧氏距离
	if (die_buffer->buffer_head == NULL)
	{
		printf("die buffer error\n");
		getchar();
	}
	else
	{
		for (page_node = die_buffer->buffer_head; page_node != NULL; page_node = page_node->LRU_link_next)
		{
			//计算每个节点lpn的距离
			lpn_buffer = page_node->group;
			if (lpn_buffer >= lpn)
				distance = lpn_buffer - lpn;
			else
				distance = lpn - lpn_buffer;

			//比较每个节点的距离，选距离最小的值
			if (min_distance > distance)
				min_distance = distance;
		}
	}
	return min_distance;
}


struct ssd_info * insert2_command_buffer(struct ssd_info * ssd, struct buffer_info * command_buffer, unsigned int lpn, int size_count, unsigned int state, struct request * req, unsigned int operation, unsigned int commond_buffer_type)
{
	unsigned int i = 0;
	unsigned int sub_req_state = 0, sub_req_size = 0, sub_req_lpn = 0;
	struct buffer_group *command_buffer_node = NULL, *pt, *new_node = NULL, key;
	struct sub_request *sub_req = NULL;
	int tmp;

	//遍历缓存的节点，判断是否有命中
	key.group = lpn;
	command_buffer_node = (struct buffer_group*)avlTreeFind(command_buffer, (TREE_NODE *)&key);

	if (command_buffer_node == NULL)
	{
		//生成 一个buff_node,根据这个页的情况分别赋值给各个成员，并且添加到队首
		new_node = NULL;
		new_node = (struct buffer_group *)malloc(sizeof(struct buffer_group));
		alloc_assert(new_node, "buffer_group_node");
		memset(new_node, 0, sizeof(struct buffer_group));

		new_node->group = lpn;
		new_node->stored = state;
		new_node->dirty_clean = state;
		new_node->LRU_link_pre = NULL;
		new_node->LRU_link_next = command_buffer->buffer_head;
		if (command_buffer->buffer_head != NULL)
		{
			command_buffer->buffer_head->LRU_link_pre = new_node;
		}
		else
		{
			command_buffer->buffer_tail = new_node;
		}
		command_buffer->buffer_head = new_node;
		new_node->LRU_link_pre = NULL;
		avlTreeAdd(command_buffer, (TREE_NODE *)new_node);
		command_buffer->command_buff_page++;

		//如果缓存已满，此时发生flush操作，将缓存的内存一次性flush到闪存上
		if (command_buffer->command_buff_page >= command_buffer->max_command_buff_page)
		{
			if (ssd->warm_flash_cmplt == 0)
				tmp = command_buffer->command_buff_page;
			else
				tmp = command_buffer->max_command_buff_page;
			//printf("begin to flush command_buffer\n");
			for (i = 0; i < tmp; i++)
			{
				sub_req = NULL;
				sub_req_state = command_buffer->buffer_tail->stored;
				sub_req_size = size(command_buffer->buffer_tail->stored);
				sub_req_lpn = command_buffer->buffer_tail->group;
				sub_req = creat_sub_request(ssd, sub_req_lpn, sub_req_size, sub_req_state, req, operation, DATA_COMMAND_BUFFER);

				//删除buff中的节点
				pt = command_buffer->buffer_tail;
				avlTreeDel(command_buffer, (TREE_NODE *)pt);
				if (command_buffer->buffer_head->LRU_link_next == NULL){
					command_buffer->buffer_head = NULL;
					command_buffer->buffer_tail = NULL;
				}
				else{
					command_buffer->buffer_tail = command_buffer->buffer_tail->LRU_link_pre;
					command_buffer->buffer_tail->LRU_link_next = NULL;
				}
				pt->LRU_link_next = NULL;
				pt->LRU_link_pre = NULL;
				AVL_TREENODE_FREE(command_buffer, (TREE_NODE *)pt);
				pt = NULL;

				command_buffer->command_buff_page--;
 			}
			if (command_buffer->command_buff_page != 0)
			{
				printf("command buff flush failed\n");
				getchar();
			}
		}
	}
	else  //命中的情况，合并扇区数
	{
		if (command_buffer->buffer_head != command_buffer_node)
		{
			if (command_buffer->buffer_tail == command_buffer_node)      //如果是最后一个节点  则交换最后两个
			{
				command_buffer_node->LRU_link_pre->LRU_link_next = NULL;
				command_buffer->buffer_tail = command_buffer_node->LRU_link_pre;
			}
			else
			{
				command_buffer_node->LRU_link_pre->LRU_link_next = command_buffer_node->LRU_link_next;     //如果是中间节点，则操作前一个和后一个节点的指针
				command_buffer_node->LRU_link_next->LRU_link_pre = command_buffer_node->LRU_link_pre;
			}
			command_buffer_node->LRU_link_next = command_buffer->buffer_head;              //提到队首
			command_buffer->buffer_head->LRU_link_pre = command_buffer_node;
			command_buffer_node->LRU_link_pre = NULL;
			command_buffer->buffer_head = command_buffer_node;
		}

		command_buffer_node->stored = command_buffer_node->stored | state;
		command_buffer_node->dirty_clean = command_buffer_node->dirty_clean | state;
	}

	return ssd;
}

//req -> write sub request  
Status update_read_request(struct ssd_info *ssd, unsigned int lpn, unsigned int state, struct sub_request *req, unsigned int commond_buffer_type)  //in this code, the state is for sector and maximal 32 bits
{
	struct sub_request *sub_r = NULL;
	struct sub_request * sub = NULL;
	struct channel_info * p_ch = NULL;
	struct local * loc = NULL;
	struct local * tem_loc = NULL;
	unsigned int flag = 0;
	int i= 0;
	unsigned int chan, chip, die, plane;
	unsigned int update_cnt;
	unsigned int off=0;
	unsigned int read_size;

	/**********************************************************
	     determine the update request count
		 generate the updates
		 note: data needed to read is the  in horizon data layout
   ******************************************************/

	unsigned int pn,state1;

	if (commond_buffer_type == DATA_COMMAND_BUFFER)
	{
		state1 = ssd->dram->map->map_entry[lpn].state;
		pn = ssd->dram->map->map_entry[lpn].pn;
	}
	else
	{
		pn = ssd->dram->tran_map->map_entry[lpn].pn;
		state1 = ssd->dram->tran_map->map_entry[lpn].state;
	}
	if (state1 == 0) //hit in reqeust queue
	{
		ssd->req_read_hit_cnt++;
		sub = (struct local *)malloc(sizeof(struct sub_request));
		tem_loc = (struct local *)malloc(sizeof(struct local));

		sub->next_node = NULL;
		sub->next_subs = NULL;
		sub->update_cnt = 0;

		if (sub == NULL)
		{
			return NULL;
		}
		sub->location = tem_loc;
		sub->current_state = SR_R_DATA_TRANSFER;
		sub->current_time = ssd->current_time;
		sub->next_state = SR_COMPLETE;                                         //置为完成状态
		sub->next_state_predict_time = ssd->current_time + 1000;
		sub->complete_time = ssd->current_time + 1000;

		//creat_one_read_sub_req(ssd, sub); 
		Match_update(ssd, 0, req, sub);
		return SUCCESS;
	}



	loc = find_location(ssd, pn);
	req->update_cnt = 0;

	if (VERTICAL_DATA_DISTRIBUTION || commond_buffer_type== TRANSACTION_COMMAND_BUFFER)  //distribute data in vertical model
	{
		req->update_cnt = 1;
		update_cnt = 1;

		for (i = 0; i < update_cnt; i++)
		{
			sub = (struct local *)malloc(sizeof(struct sub_request));
			tem_loc = (struct local *)malloc(sizeof(struct local));

			sub->next_node = NULL;
			sub->next_subs = NULL;
			//sub->update = NULL;
			sub->read_flag = UPDATE_READ;
			tem_loc->channel = loc->channel;
			tem_loc->chip = loc->chip;
			tem_loc->die = loc->die;
			tem_loc->plane = loc->plane;
			tem_loc->block = loc->block;
			tem_loc->page = loc->page;
			sub->location = tem_loc;
			sub->lpn = ssd->channel_head[tem_loc->channel].chip_head[tem_loc->chip].die_head[tem_loc->die].plane_head[tem_loc->plane].blk_head[tem_loc->block].page_head[tem_loc->page].lpn;
			sub->ppn = find_ppn(ssd, tem_loc->channel, tem_loc->chip, tem_loc->die, tem_loc->plane, tem_loc->block,tem_loc->page);
			sub->size = size(ssd->channel_head[tem_loc->channel].chip_head[tem_loc->chip].die_head[tem_loc->die].plane_head[tem_loc->plane].blk_head[tem_loc->block].page_head[tem_loc->page].valid_state);
			
			
			
			if (sub->size == 0)
			{
				printf("look here\n");
			}
			if (sub->size > 8)
			{
				printf("look here\n");
			}

			creat_one_read_sub_req(ssd, sub);  //insert into channel read  req queue
			Match_update(ssd, i, req, sub);
		}
		return SUCCESS;
	}

	switch (SB_LEVEL)
	{
	case CHANNEL_LEVEL: //across all channels
		for (chan = 0; chan < ssd->parameter->channel_number; chan++)
		{
			read_size = Is_exist_data(ssd, CHANNEL_LEVEL, state, off);
			if (read_size == 0)
			{
				off++;
				continue;
			}
				
			off++;
			sub = (struct sub_request*)malloc(sizeof(struct sub_request));                        /*申请一个子请求的结构*/
			alloc_assert(sub, "sub_request");
			memset(sub, 0, sizeof(struct sub_request));
			//sub->location = (struct local *)malloc(sizeof(struct local*));
			tem_loc = (struct local *)malloc(sizeof(struct local));

			if (sub == NULL)
			{
				return NULL;
			}
			sub->next_node = NULL;
			sub->next_subs = NULL;
			sub->read_flag = UPDATE_READ;
			sub->size = read_size;

			tem_loc->channel = chan;
			tem_loc->chip = loc->chip;
			tem_loc->die = loc->die;
			tem_loc->plane = loc->plane;
			tem_loc->block = loc->block;
			tem_loc->page = loc->page;
			sub->location = tem_loc;
			sub->lpn = ssd->channel_head[chan].chip_head[loc->chip].die_head[loc->die].plane_head[loc->plane].blk_head[loc->block].page_head[loc->page].lpn;
			sub->ppn = find_ppn(ssd, tem_loc->channel, tem_loc->chip, tem_loc->die, tem_loc->plane, tem_loc->block, tem_loc->page);
			creat_one_read_sub_req(ssd, sub);
			Match_update(ssd, i, req, sub);
			i++;
		}
		break;
	case CHIP_LEVEL:  //across all chips
		for (chan = 0; chan < ssd->parameter->channel_number; chan++)
		{
			for (chip = 0; chip < ssd->parameter->chip_channel[chan]; chip++)
			{
				read_size = Is_exist_data(ssd, CHANNEL_LEVEL, state, off);
				if (read_size == 0)
				{
					off++;
					continue;
				}
				off++;

				sub = (struct sub_request*)malloc(sizeof(struct sub_request));                        /*申请一个子请求的结构*/
				alloc_assert(sub, "sub_request");
				memset(sub, 0, sizeof(struct sub_request));
				tem_loc = (struct local *)malloc(sizeof(struct local));
				//sub->location = (struct local *)malloc(sizeof(struct local*));

				if (sub == NULL)
				{
					return NULL;
				}
				sub->next_node = NULL;
				sub->next_subs = NULL;
				sub->read_flag = UPDATE_READ;
				sub->size = read_size;
				//sub->update = NULL;
				tem_loc->channel = chan;
				tem_loc->chip = chip;
				tem_loc->die = loc->die;
				tem_loc->plane = loc->plane;
				tem_loc->block = loc->block;
				tem_loc->page = loc->page;
				sub->location = tem_loc;
				sub->lpn = ssd->channel_head[chan].chip_head[chip].die_head[loc->die].plane_head[loc->plane].blk_head[loc->block].page_head[loc->page].lpn;
				sub->ppn = find_ppn(ssd, tem_loc->channel, tem_loc->chip, tem_loc->die, tem_loc->plane, tem_loc->block, tem_loc->page);
				creat_one_read_sub_req(ssd, sub);
				Match_update(ssd, i, req, sub);
				i++;
			}
		}
		break;
	case DIE_LEVEL:  //across all dies
		for (chan = 0; chan < ssd->parameter->channel_number; chan++)
		{
			for (chip = 0; chip < ssd->parameter->chip_channel[chan]; chip++)
			{
				for (die = 0; die < ssd->parameter->die_chip; die++)
				{
					read_size = Is_exist_data(ssd, CHANNEL_LEVEL, state, off);
					if (read_size == 0)
					{
						off++;
						continue;
					}
					off++;

					sub = (struct sub_request*)malloc(sizeof(struct sub_request));
					alloc_assert(sub, "sub_request");
					memset(sub, 0, sizeof(struct sub_request));
					//sub->location = (struct local *)malloc(sizeof(struct local*));
					tem_loc = (struct local *)malloc(sizeof(struct local));

					if (sub == NULL)
					{
						return NULL;
					}
					sub->next_node = NULL;
					sub->next_subs = NULL;
					sub->read_flag = UPDATE_READ;
					sub->size = read_size;
					//sub->update = NULL;

					tem_loc->channel = chan;
					tem_loc->chip = chip;
					tem_loc->die = die;
					tem_loc->plane = loc->plane;
					tem_loc->block = loc->block;
					tem_loc->page = loc->page;
					sub->location = tem_loc;
					sub->lpn = ssd->channel_head[chan].chip_head[chip].die_head[die].plane_head[loc->plane].blk_head[loc->block].page_head[loc->page].lpn;
					sub->ppn = find_ppn(ssd, tem_loc->channel, tem_loc->chip, tem_loc->die, tem_loc->plane, tem_loc->block, tem_loc->page);
					creat_one_read_sub_req(ssd, sub);
					Match_update(ssd, i, req, sub);
					i++;
				}
			}
		}
		break;
	case PLANE_LEVEL: //across all planes
		for (chan = 0; chan < ssd->parameter->channel_number; chan++)
		{
			for (chip = 0; chip < ssd->parameter->chip_channel[chan]; chip++)
			{
				for (die = 0; die < ssd->parameter->die_chip; die++)
				{
					for (plane = 0; plane < ssd->parameter->plane_die; plane++)
					{
						read_size = Is_exist_data(ssd, CHANNEL_LEVEL, state, off);
						if (read_size == 0)
						{
							off++;
							continue;
						}
						off++;

						sub = (struct sub_request*)malloc(sizeof(struct sub_request));
						alloc_assert(sub, "sub_request");
						memset(sub, 0, sizeof(struct sub_request));
						//sub->location = (struct local *)malloc(sizeof(struct local*));
						tem_loc = (struct local *)malloc(sizeof(struct local));
						if (sub == NULL)
						{
							return NULL;
						}
						sub->next_node = NULL;
						sub->next_subs = NULL;
						sub->read_flag = UPDATE_READ;
						sub->size = read_size;
						//sub->update = NULL;

						tem_loc->channel = chan;
						tem_loc->chip = chip;
						tem_loc->die = die;
						tem_loc->plane = plane;
						tem_loc->block = loc->block;
						tem_loc->page = loc->page;
						sub->location = tem_loc;
						sub->lpn = ssd->channel_head[chan].chip_head[chip].die_head[die].plane_head[plane].blk_head[loc->block].page_head[loc->page].lpn;
						sub->ppn = find_ppn(ssd, tem_loc->channel, tem_loc->chip, tem_loc->die, tem_loc->plane, tem_loc->block, tem_loc->page);
						creat_one_read_sub_req(ssd, sub);
						Match_update(ssd, i, req, sub);
						i++;
					}
				}
			}
		}
		break;
	default:
		printf("Warning:Data Distribution is mentioned in this simulation!\n");
		break;
	}
	req->update_cnt = i;

	free(loc);
	return SUCCESS;
}

Status Match_update(struct ssd_inof *ssd, unsigned int i, struct sub_request *req,struct sub_request *sub)
{
	if (i == 0)
		req->update_0 = sub;
	else if (i == 1)
		req->update_1 = sub;
	else if (i == 2)
		req->update_2 = sub;
	else if (i == 3)
		req->update_3 = sub;
	else if (i == 4)
		req->update_4 = sub;
	else if (i == 5)
		req->update_5 = sub;
	else if (i == 6)
		req->update_6 = sub;
	else if (i == 7)
		req->update_7 = sub;
	else if (i == 8)
		req->update_8 = sub;
	else if (i == 9)
		req->update_9 = sub;
	else if (i == 10)
		req->update_10 = sub;
	else if (i == 11)
		req->update_11 = sub;
	else if (i == 12)
		req->update_12 = sub;
	else if (i == 13)
		req->update_13 = sub;
	else if (i == 14)
		req->update_14 = sub;
	else if (i == 15)
		req->update_15 = sub;
	else if (i == 16)
		req->update_16 = sub;
	else if (i == 17)
		req->update_17 = sub;
	else if (i == 18)
		req->update_18 = sub;
	else if (i == 19)
		req->update_19 = sub;
	else if (i == 20)
		req->update_20 = sub;
	else if (i == 21)
		req->update_21 = sub;
	else if (i == 22)
		req->update_22 = sub;
	else if (i == 23)
		req->update_23 = sub;
	else if (i == 24)
		req->update_24 = sub;
	else if (i == 25)
		req->update_25 = sub;
	else if (i == 26)
		req->update_26 = sub;
	else if (i == 27)
		req->update_27 = sub;
	else if (i == 28)
		req->update_28 = sub;
	else if (i == 29)
		req->update_29 = sub;
	else if (i == 30)
		req->update_30 = sub;
	else if (i == 31)
		req->update_31 = sub;
	else
	{
		printf("Error: Update error\n");
		getchar();
	}
	return SUCCESS;
}


Status Is_exist_data(struct ssd_info *ssd, int para_level, unsigned int state, unsigned int off)
{
	unsigned int unit_size; //data size in each part in horizon data layout
	unsigned int sum_request_num;
	
	unsigned int i;
	unsigned int flag = 0;

	switch (para_level)
	{
	case CHANNEL_LEVEL:
		sum_request_num = ssd->parameter->channel_number;
		unit_size = (ssd->parameter->page_capacity/SECTOR) / sum_request_num;
		break;
	case CHIP_LEVEL:
		sum_request_num = ssd->parameter->chip_num;
		unit_size = (ssd->parameter->page_capacity / SECTOR) / sum_request_num;
		break;
	case DIE_LEVEL:
		sum_request_num = ssd->parameter->chip_num*ssd->parameter->die_chip;
		unit_size = (ssd->parameter->page_capacity / SECTOR) / sum_request_num;
		break;
	case PLANE_LEVEL:
		sum_request_num = ssd->parameter->chip_num*ssd->parameter->die_chip*ssd->parameter->plane_die;
		unit_size = (ssd->parameter->page_capacity / SECTOR) / sum_request_num;
		break;
	default:
		break;
	}

	for (i = off*unit_size; i < (off + 1)*unit_size; i++)
	{
		if (GET_BIT(state, i) == 1)
			flag++;
	}
	return flag;
}

Status read_reqeust(struct ssd_info *ssd, unsigned int lpn, struct request *req, unsigned int state,unsigned int buffer_commond_type)
{
	struct sub_request* sub = NULL;
	struct local * loc = NULL;
	struct local *tem_loc = NULL;
	unsigned int chan, chip, die, plane;
	unsigned int data_size;
	unsigned int off = 0;
	unsigned int read_size;

	unsigned int pn;
	
	if (buffer_commond_type == DATA_COMMAND_BUFFER)
		pn = ssd->dram->map->map_entry[lpn].pn;
	else
		pn = ssd->dram->tran_map->map_entry[lpn].pn;

	if (pn == -1) //hit in reqeust queue
	{
		ssd->req_read_hit_cnt++;
		sub = (struct local *)malloc(sizeof(struct sub_request));
		tem_loc = (struct local *)malloc(sizeof(struct local));

		sub->next_node = NULL;
		sub->next_subs = NULL;
		sub->update_cnt = 0;

		if (sub == NULL)
		{
			return NULL;
		}
		sub->location = tem_loc;
		sub->current_state = SR_R_DATA_TRANSFER;
		sub->current_time = ssd->current_time;
		sub->next_state = SR_COMPLETE;                                         //置为完成状态
		sub->next_state_predict_time = ssd->current_time + 1000;
		sub->complete_time = ssd->current_time + 1000;

		//将子请求挂载在总请求上，为请求的执行完成做准备
		if (req == NULL)  //request is NULL means update reqd in this version. sub->update
		{
			req = sub;
		}
		else
		{
			sub->next_subs = req->subs;
			req->subs = sub;
			sub->total_request = req;
		}
		return SUCCESS;
	}


	loc = find_location(ssd, pn);

	if (VERTICAL_DATA_DISTRIBUTION || buffer_commond_type==TRANSACTION_COMMAND_BUFFER)  //distribute data in vertical model
	{
		sub = (struct local *)malloc(sizeof(struct sub_request));
		tem_loc = (struct local *)malloc(sizeof(struct local));

		sub->next_node = NULL;
		sub->next_subs = NULL;
			
		if (sub == NULL)
		{
			return NULL;
		}
		sub->next_node = NULL;
		sub->next_subs = NULL;
		sub->update_cnt = 0;
		sub->read_flag = REQ_READ;
		sub->state = state;
		sub->size = size(state);

		if (sub->size == 0)
		{
			printf("look here , size = 0 \n");
		}

		if (sub->size > 8)
		{
			getchar();
		}
		//将子请求挂载在总请求上，为请求的执行完成做准备
		if (req == NULL)  //request is NULL means update reqd in this version. sub->update
		{
			req = sub;
		}
		else
		{
			sub->next_subs = req->subs;
			req->subs = sub;
			sub->total_request = req;
		}


		tem_loc->channel = loc->channel;
		tem_loc->chip = loc->chip;
		tem_loc->die = loc->die;
		tem_loc->plane = loc->plane;
		tem_loc->block = loc->block;
		tem_loc->page = loc->page;
		sub->location = tem_loc;
		sub->lpn = ssd->channel_head[tem_loc->channel].chip_head[tem_loc->chip].die_head[tem_loc->die].plane_head[tem_loc->plane].blk_head[tem_loc->block].page_head[tem_loc->page].lpn;
		sub->ppn = find_ppn(ssd, tem_loc->channel, tem_loc->chip, tem_loc->die, tem_loc->plane, tem_loc->block, tem_loc->page);
		creat_one_read_sub_req(ssd, sub);  //insert into channel read  req queue
		free(loc);
		return SUCCESS;
	}

	data_size = size(state);

	switch (SB_LEVEL)
	{
	case CHANNEL_LEVEL: //across all channels
		for (chan = 0; chan < ssd->parameter->channel_number; chan++)
		{
			//judge the data to be read is not locates the data area
            
			read_size = Is_exist_data(ssd, CHANNEL_LEVEL, state, off);
			if (read_size == 0)
			{
				off++;
				continue;
			}
			off++;

			sub = (struct sub_request*)malloc(sizeof(struct sub_request));                        /*申请一个子请求的结构*/
			alloc_assert(sub, "sub_request");
			memset(sub, 0, sizeof(struct sub_request));
			//sub->location = (struct local *)malloc(sizeof(struct local*));
			tem_loc = (struct local *)malloc(sizeof(struct local));

			sub->next_node = NULL;
			sub->next_subs = NULL;
			sub->update_cnt = 0;
			sub->read_flag = REQ_READ;
			sub->size = read_size;
			//将子请求挂载在总请求上，为请求的执行完成做准备
			if (req == NULL)  //request is NULL means update reqd in this version. sub->update
			{
				req = sub;
			}
			else
			{
				sub->next_subs = req->subs;
				req->subs = sub;
				sub->total_request = req;
			}
			tem_loc->channel = chan;
			tem_loc->chip = loc->chip;
			tem_loc->die = loc->die;
			tem_loc->plane = loc->plane;
			tem_loc->block = loc->block;
			tem_loc->page = loc->page;
			sub->location = tem_loc;
			sub->lpn = ssd->channel_head[chan].chip_head[loc->chip].die_head[loc->die].plane_head[loc->plane].blk_head[loc->block].page_head[loc->page].lpn;
			sub->ppn = find_ppn(ssd, tem_loc->channel, tem_loc->chip, tem_loc->die, tem_loc->plane, tem_loc->block, tem_loc->page);
			creat_one_read_sub_req(ssd, sub);

		}
		break;
	case CHIP_LEVEL:  //across all chips
		for (chan = 0; chan < ssd->parameter->channel_number; chan++)
		{
			for (chip = 0; chip < ssd->parameter->chip_channel[chan]; chip++)
			{
				read_size = Is_exist_data(ssd, CHANNEL_LEVEL, state, off);
				if (read_size == 0)
				{
					off++;
					continue;
				}
				off++;
				sub = (struct sub_request*)malloc(sizeof(struct sub_request));                        /*申请一个子请求的结构*/
				alloc_assert(sub, "sub_request");
				memset(sub, 0, sizeof(struct sub_request));
				tem_loc = (struct local *)malloc(sizeof(struct local));
				//sub->location = (struct local *)malloc(sizeof(struct local*));

				if (sub == NULL)
				{
					return NULL;
				}
				sub->next_node = NULL;
				sub->next_subs = NULL;
				sub->update_cnt = 0;
				sub->read_flag = REQ_READ;
				sub->size = read_size;

				//将子请求挂载在总请求上，为请求的执行完成做准备
				if (req == NULL)  //request is NULL means update reqd in this version. sub->update
				{
					req = sub;
				}
				else
				{
					sub->next_subs = req->subs;
					req->subs = sub;
					sub->total_request = req;
				}
				tem_loc->channel = chan;
				tem_loc->chip = chip;
				tem_loc->die = loc->die;
				tem_loc->plane = loc->plane;
				tem_loc->block = loc->block;
				tem_loc->page = loc->page;
				sub->location = tem_loc;
				sub->lpn = ssd->channel_head[chan].chip_head[chip].die_head[loc->die].plane_head[loc->plane].blk_head[loc->block].page_head[loc->page].lpn;
				sub->ppn = find_ppn(ssd, tem_loc->channel, tem_loc->chip, tem_loc->die, tem_loc->plane, tem_loc->block, tem_loc->page);
				creat_one_read_sub_req(ssd, sub);
			}
		}
		break;
	case DIE_LEVEL:  //across all dies
		for (chan = 0; chan < ssd->parameter->channel_number; chan++)
		{
			for (chip = 0; chip < ssd->parameter->chip_channel[chan]; chip++)
			{
				for (die = 0; die < ssd->parameter->die_chip; die++)
				{
					read_size = Is_exist_data(ssd, CHANNEL_LEVEL, state, off);
					if (read_size == 0)
					{
						off++;
						continue;
					}
					off++;
					sub = (struct sub_request*)malloc(sizeof(struct sub_request));
					alloc_assert(sub, "sub_request");
					memset(sub, 0, sizeof(struct sub_request));
					//sub->location = (struct local *)malloc(sizeof(struct local*));
					tem_loc = (struct local *)malloc(sizeof(struct local));

					if (sub == NULL)
					{
						return NULL;
					}
					sub->next_node = NULL;
					sub->next_subs = NULL;
					sub->update_cnt = 0;
					sub->read_flag = REQ_READ;
					sub->size = read_size;

					//将子请求挂载在总请求上，为请求的执行完成做准备
					if (req == NULL)  //request is NULL means update reqd in this version. sub->update
					{
						req = sub;
					}
					else
					{
						sub->next_subs = req->subs;
						req->subs = sub;
						sub->total_request = req;
					}
					tem_loc->channel = chan;
					tem_loc->chip = chip;
					tem_loc->die = die;
					tem_loc->plane = loc->plane;
					tem_loc->block = loc->block;
					tem_loc->page = loc->page;
					sub->location = tem_loc;
					sub->lpn = ssd->channel_head[chan].chip_head[chip].die_head[die].plane_head[loc->plane].blk_head[loc->block].page_head[loc->page].lpn;
					sub->ppn = find_ppn(ssd, tem_loc->channel, tem_loc->chip, tem_loc->die, tem_loc->plane, tem_loc->block, tem_loc->page);
					creat_one_read_sub_req(ssd, sub);
				}
			}
		}
		break;
	case PLANE_LEVEL: //across all planes
		for (chan = 0; chan < ssd->parameter->channel_number; chan++)
		{
			for (chip = 0; chip < ssd->parameter->chip_channel[chan]; chip++)
			{
				for (die = 0; die < ssd->parameter->die_chip; die++)
				{
					for (plane = 0; plane < ssd->parameter->plane_die; plane++)
					{
						read_size = Is_exist_data(ssd, CHANNEL_LEVEL, state, off);
						if (read_size == 0)
						{
							off++;
							continue;
						}
						off++;

						sub = (struct sub_request*)malloc(sizeof(struct sub_request));
						alloc_assert(sub, "sub_request");
						memset(sub, 0, sizeof(struct sub_request));
						//sub->location = (struct local *)malloc(sizeof(struct local*));
						tem_loc = (struct local *)malloc(sizeof(struct local));
						if (sub == NULL)
						{
							return NULL;
						}
						sub->next_node = NULL;
						sub->next_subs = NULL;
						sub->update_cnt = 0;
						sub->read_flag = REQ_READ;
						sub->size = read_size;

						//将子请求挂载在总请求上，为请求的执行完成做准备
						if (req == NULL)  //request is NULL means update reqd in this version. sub->update
						{
							req = sub;
						}
						else
						{
							sub->next_subs = req->subs;
							req->subs = sub;
							sub->total_request = req;
						}
						tem_loc->channel = chan;
						tem_loc->chip = chip;
						tem_loc->die = die;
						tem_loc->plane = plane;
						tem_loc->block = loc->block;
						tem_loc->page = loc->page;
						sub->location = tem_loc;
						sub->lpn = ssd->channel_head[chan].chip_head[chip].die_head[die].plane_head[plane].blk_head[loc->block].page_head[loc->page].lpn;
						sub->ppn = find_ppn(ssd, tem_loc->channel, tem_loc->chip, tem_loc->die, tem_loc->plane, tem_loc->block, tem_loc->page);
						creat_one_read_sub_req(ssd, sub);
					}
				}
			}
		}
		break;
	default:
		printf("Warning:Data Distribution is mentioned in this simulation!\n");
		break;
	}

	return SUCCESS;
}

unsigned int find_cache_offset(struct ssd_info *ssd, unsigned int lpn)
{
	unsigned int offset;
	int i;
	for (i = 0; i < ssd->cache_size; i++)
	{
		if (ssd->dram->map->cached_lpns[i] == lpn)
		{
			offset = i;
			break;
		}
	}
	if (i == ssd->cache_size)
	{
		printf("Error! Cache Mappping Table \n");
		getchar();
	}
	return offset;
}

unsigned int find_cache_min(struct ssd_info *ssd)
{
	unsigned int min_pos;
	unsigned int min = 9999999999;
	int i, lpn;

	for (i = 0; i < ssd->dram->map->cache_num; i++)
	{
		lpn = ssd->dram->map->cached_lpns[i];
		if (min> ssd->dram->map->map_entry[lpn].cache_age)
		{
			min = ssd->dram->map->map_entry[lpn].cache_age;
			min_pos = i;
		}			
	}
	return min_pos;
}


/**************************************************************
this function is to create sub_request based on lpn, size, state
****************************************************************/
struct sub_request * creat_sub_request(struct ssd_info * ssd, unsigned int lpn, int size, unsigned int state, struct request * req, unsigned int operation,unsigned int command_type)
{
	struct sub_request* sub = NULL, *sub_r = NULL, *update = NULL, *sub_w = NULL, *tmp_sub_w = NULL;
	struct channel_info * p_ch = NULL;
	struct local * loc = NULL;
	unsigned int flag = 0;
	int i;
	unsigned int chan, chip, die, plane;
	unsigned int cache_offset,max_cache_age_lpn;
	unsigned int min_pos,cache_lpn,vpn1,vpn2;
	unsigned int map_size, map_state,off;
	unsigned int map_size2, map_state2, off2, state22;
	int t;

	unsigned int bits = 0;

	if (command_type == DATA_COMMAND_BUFFER && DFTL)
	{
		/***********************
		dftl design
		1. judge whether the lpn is cached
		1.1 yes: update the cache mapping table info
		1.2 no: go to step 2
		2. insert the mapping entry
		2.1 judge whether the mapping entry cache is full
		2.1.1 yes. remove one entry according to the LRU
		2.1.1.1 if the entry is dirty, update the mapping page
		2.1.2 no. goto the step 2.2
		2.2 insert to mapping entry
		************************/
		if (ssd->dram->map->map_entry[lpn].cache_valid == CACHE_VALID)
		{

			ssd->cache_hit++;
			if (operation == WRITE)
				ssd->write_cache_hit_num++;
			else
				ssd->read_cache_hit_num++;

			ssd->dram->map->map_entry[lpn].cache_age++;
			if (ssd->dram->map->cache_max == -1)
			{
				cache_offset = find_cache_offset(ssd, lpn);
				ssd->dram->map->cache_max = cache_offset;
			}
			max_cache_age_lpn = ssd->dram->map->cached_lpns[ssd->dram->map->cache_max];
			if (ssd->dram->map->map_entry[max_cache_age_lpn].cache_valid != CACHE_VALID)
			{
				printf("Error! Maintain cached mapping entries\n");
				getchar();
			}
			if (ssd->dram->map->map_entry[max_cache_age_lpn].cache_age <= ssd->dram->map->map_entry[lpn].cache_age)
			{
				cache_offset = find_cache_offset(ssd, lpn);
				ssd->dram->map->cache_max = cache_offset;
			}
		}
		else  // not hit in the cache
		{
			min_pos = ssd->dram->map->cache_num;
			//if the cache is full 
			if (ssd->dram->map->cache_num >= ssd->cache_size)
			{
				//find the LRU entry  
				min_pos = find_cache_min(ssd);
				ssd->evict++;

				if (operation == WRITE)
					ssd->write_evict++;
				else
					ssd->read_evict++;

				cache_lpn = ssd->dram->map->cached_lpns[min_pos];
				if (ssd->dram->map->map_entry[cache_lpn].update == 1) // write back the dirty entry
				{
					// creat a transaction update write 
					vpn1 = cache_lpn / (ssd->parameter->page_capacity / 4);
					map_size = 1;
					map_state = 0;
					off = (cache_lpn / (SECTOR / 4)) % (secno_num_per_page);
					map_state=SET_VALID(map_state, off);
					creat_sub_request(ssd, vpn1, map_size, map_state, req, WRITE, TRANSACTION_COMMAND_BUFFER);

					ssd->dram->map->map_entry[cache_lpn].update = 0;
				}
				ssd->dram->map->map_entry[cache_lpn].cache_valid = CACHE_INVALID;
				ssd->dram->map->cache_num--;
				ssd->dram->map->cached_lpns[min_pos] = -1;
			}
			//read the corresponding transaction page 
			vpn2 = lpn / (ssd->parameter->page_capacity / 4);
			map_size2 = 1;
			map_state2 = 0;
			off = (lpn / (SECTOR / 4)) % (secno_num_per_page);
			map_state2=SET_VALID(map_state2, off);
			if (ssd->dram->map->map_entry[lpn].state > 0)
			{
				creat_sub_request(ssd, vpn2, map_size2, map_state2, req, READ, TRANSACTION_COMMAND_BUFFER);
			}
			ssd->dram->map->map_entry[lpn].cache_valid = CACHE_VALID;
			if (ssd->dram->map->cache_num > 0)
			{
				max_cache_age_lpn = ssd->dram->map->cached_lpns[ssd->dram->map->cache_max];
				ssd->dram->map->map_entry[lpn].cache_age = ssd->dram->map->map_entry[max_cache_age_lpn].cache_age + 1;
			}
			ssd->dram->map->cached_lpns[min_pos] = lpn;
			ssd->dram->map->cache_max = min_pos;
			ssd->dram->map->cache_num++;
			ssd->dram->map->map_entry[lpn].update = 1;
		}
	}


	/*************************************************************************************
	*在读操作的情况下，有一点非常重要就是要预先判断读子请求队列中是否有与这个子请求相同的，
	*有的话，新子请求就不必再执行了，将新的子请求直接赋为完成
	**************************************************************************************/
	if (operation == READ)
	{
		read_reqeust(ssd, lpn, req,state,command_type);
	}
	else if (operation == WRITE)
	{
		sub = (struct sub_request*)malloc(sizeof(struct sub_request));                        /*申请一个子请求的结构*/
		alloc_assert(sub, "sub_request");
		memset(sub, 0, sizeof(struct sub_request));

		if (sub == NULL)
		{
			return NULL;
		}
		sub->location = NULL;
		sub->next_node = NULL;
		sub->next_subs = NULL;
		//sub->update = NULL;
		sub->lpn = lpn;

		//将子请求挂载在总请求上，为请求的执行完成做准备
		if (req != NULL)
		{
			sub->next_subs = req->subs;
			req->subs = sub;
			sub->total_request = req;
		}
		
		sub->ppn = 0;
		sub->operation = WRITE;
		sub->location = (struct local *)malloc(sizeof(struct local));
		alloc_assert(sub->location, "sub->location");
		memset(sub->location, 0, sizeof(struct local));

		//sub->update = (struct sub_request **)malloc(MAX_SUPERBLOCK_SISE*sizeof(struct sub_request *));

		sub->current_state = SR_WAIT;
		sub->current_time = ssd->current_time;
		sub->lpn = lpn;
		sub->size = size;
		sub->state = state;
		sub->begin_time = ssd->current_time;

		if (ssd->parameter->allocation_scheme == SUPERBLOCK_ALLOCATION)
		{
			//apply for free super page
			if (ssd->open_sb[command_type]->next_wr_page >= ssd->parameter->page_block) //no free superpage in the superblock
				find_active_superblock(ssd,command_type);

			//allocate free page 
			ssd->open_sb[command_type]->pg_off = (ssd->open_sb[command_type]->pg_off + 1) % ssd->sb_pool[command_type].blk_cnt;
			sub->location->channel = ssd->open_sb[command_type]->pos[ssd->open_sb[command_type]->pg_off].channel;
			sub->location->chip = ssd->open_sb[command_type]->pos[ssd->open_sb[command_type]->pg_off].chip;
			sub->location->die = ssd->open_sb[command_type]->pos[ssd->open_sb[command_type]->pg_off].die;
			sub->location->plane = ssd->open_sb[command_type]->pos[ssd->open_sb[command_type]->pg_off].plane;
			sub->location->block = ssd->open_sb[command_type]->pos[ssd->open_sb[command_type]->pg_off].block;
			sub->location->page = ssd->open_sb[command_type]->next_wr_page;

			if (ssd->open_sb[command_type]->pg_off == ssd->sb_pool[command_type].blk_cnt - 1)
				ssd->open_sb[command_type]->next_wr_page++;


			sub->ppn = find_ppn(ssd, sub->location->channel, sub->location->chip, sub->location->die, sub->location->plane, sub->location->block, sub->location->page);
			if (command_type == TRANSACTION_COMMAND_BUFFER)
			{
				state22 = 0;
				for (t = 0; t < ssd->parameter->subpage_page; t++)
				{
					if (GET_BIT(state, t) == 0)
					{
						state22 = SET_VALID(state22, t);
						bits++;
					}
				}

				if (bits > 8)
				{
					getchar();
				}
				update_read_request(ssd, lpn, state22, sub, TRANSACTION_COMMAND_BUFFER);
			}
			else if (ssd->dram->map->map_entry[lpn].state != 0)//update write 
			{
				if (size != secno_num_per_page)  //not full page 
				{	
					state22 = 0;
					for (t = 0; t < ssd->parameter->subpage_page; t++)
					{
						if (GET_BIT(state, t) == 0)
						{
							state22 = SET_VALID(state22, t);
							bits++;
						}
					}

					if (bits > 8)
					{
						getchar();
					}
					//create a new read operation
					update_read_request(ssd, lpn, state22, sub, DATA_COMMAND_BUFFER);
				}
			}


			sub_w = ssd->channel_head[sub->location->channel].subs_w_head;


			// can hit write request queue?? 
			while (sub_w != NULL)
			{
				if (sub_w->ppn == sub->ppn)  //no possibility to write into the same physical position
				{
					printf("error: write into the same physical address\n");
					getchar();
				}
				sub_w = sub_w->next_node;
			}

			if (ssd->channel_head[sub->location->channel].subs_w_tail != NULL)
			{
				ssd->channel_head[sub->location->channel].subs_w_tail->next_node = sub;
				ssd->channel_head[sub->location->channel].subs_w_tail = sub;
			}
			else
			{
				ssd->channel_head[sub->location->channel].subs_w_head = sub;
				ssd->channel_head[sub->location->channel].subs_w_tail = sub;
			}

		}
	}
	else
	{
		free(sub->location);
		sub->location = NULL;
		free(sub);
		sub = NULL;
		printf("\nERROR ! Unexpected command.\n");
		return NULL;
	}

	return sub;
}

Status creat_one_read_sub_req(struct ssd_info *ssd, struct sub_request* sub)
{
	unsigned int lpn,flag;
	struct channel_info * p_ch = NULL;
	struct local *loc = NULL;
	struct sub_request* sub_r;

	lpn = sub->lpn;
	sub->begin_time = ssd->current_time;
	sub->current_state = SR_WAIT;
	sub->current_time = 0x7fffffffffffffff;
	sub->next_state = SR_R_C_A_TRANSFER;
	sub->next_state_predict_time = 0x7fffffffffffffff;
	//sub->size = size;                                                               /*需要计算出该子请求的请求大小*/
	sub->update_read_flag = 0;
	sub->suspend_req_flag = NORMAL_TYPE;

	//sub->ppn = sub->ppn;
	//ssd->dram->map->map_entry[lpn].pn;
	loc = sub->location;
	p_ch = &ssd->channel_head[loc->channel];
	
	sub->operation = READ;
	sub_r = ssd->channel_head[loc->channel].subs_r_head;


	flag = 0;

	//if (SMART_DATA_ALLOCATION)  //读请求批处理
	if(1)
	{
		while (sub_r != NULL)
		{
			if (sub_r->ppn == sub->ppn)                             //判断有没有访问相同ppn的子请求    ，依次比较ppn
			{
				if (sub->read_flag == REQ_READ)
					ssd->req_read_hit_cnt++;
				if (sub->read_flag == GC_READ)
					ssd->gc_read_hit_cnt++;
				if (sub->read_flag == UPDATE_READ)
					ssd->update_read_hit_cnt++;
				flag = 1;
				break;
			}
			sub_r = sub_r->next_node;
		}
	}

	if (flag == 0)                                                //子请求队列中没有访问相同ppn的请求则将新建的sub插入到子请求队列中
	{
		ssd->channel_head[loc->channel].chip_head[loc->chip].die_head[loc->die].read_cnt++;
		if (ssd->channel_head[loc->channel].subs_r_tail != NULL)
		{
			ssd->channel_head[loc->channel].subs_r_tail->next_node = sub;          //sub挂在子请求队列最后
			ssd->channel_head[loc->channel].subs_r_tail = sub;
		}
		else
		{
			ssd->channel_head[loc->channel].subs_r_head = sub;
			ssd->channel_head[loc->channel].subs_r_tail = sub;
		}
	}
	else
	{
		sub->current_state = SR_R_DATA_TRANSFER;
		sub->current_time = ssd->current_time;
		sub->next_state = SR_COMPLETE;                                         //置为完成状态
		sub->next_state_predict_time = ssd->current_time + 1000;
		sub->complete_time = ssd->current_time + 1000;
	}

	if (sub->size > 8)
	{
		getchar();
	}

	if (sub->size == 0)
	{
		printf("look here\n");
	}

	return SUCCESS;
}


/***************************************
*Write request allocation mount
***************************************/
Status allocate_location(struct ssd_info * ssd, struct sub_request *sub_req)
{
	return SUCCESS;
}

struct ssd_info *flush_all(struct ssd_info *ssd)
{
#if 0
	struct buffer_group *pt;
	struct sub_request *sub_req = NULL;
	unsigned int sub_req_state = 0, sub_req_size = 0, sub_req_lpn = 0;

	struct request *req = (struct request*)malloc(sizeof(struct request));
	alloc_assert(req, "request");
	memset(req, 0, sizeof(struct request));

	int i, j,t;
	/*for (i = 0; i < ssd->parameter->channel_number; i++)
	{
	for (j = 0; j < ssd->parameter->chip_channel[i]; j++)
	{
	printf("channel:%d  chip:%d  current_state:%d\n", i, j, ssd->channel_head[i].chip_head[j].current_state);
	}
	printf("channel:%d  current_state:%d\n", i, ssd->channel_head[i].current_state);
	printf("channel:%d  next_state:%d\n", i, ssd->channel_head[i].next_state);
	printf("ssd->currenn_time:%I64u channel:next_predict_time:%I64u\n", ssd->current_time, ssd->channel_head[i].next_state_predict_time);
	}*/

	if (ssd->request_queue == NULL)          //The queue is empty
	{
		ssd->request_queue = req;
		ssd->request_tail = req;
		ssd->request_work = req;
		ssd->request_queue_length++;
	}
	else
	{
		(ssd->request_tail)->next_node = req;
		ssd->request_tail = req;
		if (ssd->request_work == NULL)
			ssd->request_work = req;
		ssd->request_queue_length++;
	}

	int max_command_buff_page_tmp1, max_command_buff_page_tmp2;
	if (ssd->trace_over_flag == 1 && ssd->warm_flash_cmplt == 0)
	{
		for (t = 0; t < 2; t++)
		{
			max_command_buff_page_tmp1 = ssd->dram->command_buffer[t]->max_command_buff_page;
			max_command_buff_page_tmp2 = ssd->dram->static_die_buffer[t]->max_command_buff_page;


			ssd->dram->command_buffer[t]->max_command_buff_page = 1;

			for (i = 0; i < 4; i++)
				ssd->dram->static_die_buffer[i]->max_command_buff_page = 1;
			while (ssd->dram->buffer->buffer_sector_count > 0)
			{
				//printf("%u\n", ssd->dram->buffer->buffer_sector_count);
				sub_req = NULL;
				sub_req_state = ssd->dram->buffer->buffer_tail->stored;
				sub_req_size = size(ssd->dram->buffer->buffer_tail->stored);
				sub_req_lpn = ssd->dram->buffer->buffer_tail->group;

				distribute2_command_buffer(ssd, sub_req_lpn, sub_req_size, sub_req_state, req, WRITE);

				ssd->dram->buffer->buffer_sector_count = ssd->dram->buffer->buffer_sector_count - sub_req_size;

				pt = ssd->dram->buffer->buffer_tail;
				avlTreeDel(ssd->dram->buffer, (TREE_NODE *)pt);
				if (ssd->dram->buffer->buffer_head->LRU_link_next == NULL) {
					ssd->dram->buffer->buffer_head = NULL;
					ssd->dram->buffer->buffer_tail = NULL;
				}
				else {
					ssd->dram->buffer->buffer_tail = ssd->dram->buffer->buffer_tail->LRU_link_pre;
					ssd->dram->buffer->buffer_tail->LRU_link_next = NULL;
				}
				pt->LRU_link_next = NULL;
				pt->LRU_link_pre = NULL;
				AVL_TREENODE_FREE(ssd->dram->buffer, (TREE_NODE *)pt);
				pt = NULL;
			}
			ssd->dram->command_buffer[t]->max_command_buff_page = max_command_buff_page_tmp1;
			for (i = 0; i < 4; i++)
				ssd->dram->static_die_buffer[i]->max_command_buff_page = max_command_buff_page_tmp2;
		}

	}
#endif
	return ssd;
}