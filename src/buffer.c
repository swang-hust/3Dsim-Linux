#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

#include "buffer.h"

extern int secno_num_per_page, secno_num_sub_page;

/***********************************DFTL*************************************************/
/*
	mapping buffer: cache hot mapping entries in two-level lists


	flag: write insert or read translation miss insert  
	    write：create a new translation subpage read 
		 read：when read translation miss, an extra translation read can occur and is hooked into sub read request
*/
struct ssd_info* insert2map_buffer(struct ssd_info* ssd, unsigned int lpn, struct request* req,unsigned int flag)
{
	unsigned int vpn;
	unsigned int free_B, insert_B;
	struct buffer_group* buffer_node = NULL, * pt, key;
	unsigned int r_vpn = 0, entry_cnt;
	unsigned int i, tmp_lpn;

	vpn = lpn / ssd->map_entry_per_subpage;

	switch (DFTL)
	{
	case DFTL_BASE:
		insert_B = ssd->parameter->mapping_entry_size;   // unit is B
		key.group = lpn;
		break;
	case TPFTL:
		insert_B = ssd->parameter->mapping_entry_size;   // unit is B
		key.group = vpn;
		break;
	case SFTL:
		insert_B = ssd->parameter->subpage_capacity;        // manage at the granularity by translation page
		key.group = vpn;
		break;
	case FULLY_CACHED:
		insert_B = ssd->parameter->mapping_entry_size;        // manage at the granularity by translation page
		key.group = vpn;
		break;
	default:
		printf("Unidentifiable Phara\n");
		break;
	}

	//fprintf(ssd->sb_info, "%d\n",vpn);
	buffer_node = (struct buffer_group*)avlTreeFind(ssd->dram->mapping_buffer, (TREE_NODE*)& key);

	if (buffer_node == NULL)  // the mapping node is not in the mapping buffer 
	{
		ssd->write_tran_cache_miss++;
		//judge whether some mapping node is needed to be replaced
		free_B = ssd->dram->mapping_buffer->max_buffer_B - ssd->dram->mapping_buffer->buffer_B_count;
		while (free_B < insert_B) //replace
		{
			/*
			   insert to mapping command buffer
			   get tail node and invoke the insert to command buffer
			*/
			entry_cnt = ssd->dram->mapping_buffer->buffer_tail->entry_cnt;
			r_vpn = ssd->dram->mapping_buffer->buffer_tail->group;
			if (entry_cnt == 0)
			{
				printf("Look Here\n");
			}
			insert2_command_buffer(ssd, ssd->dram->mapping_command_buffer, r_vpn, entry_cnt, req, MAPPING_DATA);

			for (i = 0; i < ssd->map_entry_per_subpage; i++)
			{
				tmp_lpn = r_vpn * ssd->map_entry_per_subpage + i;
				ssd->dram->map->map_entry[tmp_lpn].cache_valid = 0;
			}

			//delete the mapping node from the mapping command buffer 
			pt = ssd->dram->mapping_buffer->buffer_tail;
			avlTreeDel(ssd->dram->mapping_buffer, (TREE_NODE*)pt);

			if (ssd->dram->mapping_buffer->buffer_head->LRU_link_next == NULL) {
				ssd->dram->mapping_buffer->buffer_head = NULL;
				ssd->dram->mapping_buffer->buffer_tail = NULL;
			}
			else {
				ssd->dram->mapping_buffer->buffer_tail = ssd->dram->mapping_buffer->buffer_tail->LRU_link_pre;
				ssd->dram->mapping_buffer->buffer_tail->LRU_link_next = NULL;
			}
			pt->LRU_link_next = NULL;
			pt->LRU_link_pre = NULL;
			AVL_TREENODE_FREE(ssd->dram->mapping_buffer, (TREE_NODE*)pt);
			pt = NULL;

			switch (DFTL)
			{
			case DFTL_BASE:
			case TPFTL:
				ssd->dram->mapping_buffer->buffer_B_count -= entry_cnt * ssd->parameter->mapping_entry_size;
				break;
			case SFTL:
				ssd->dram->mapping_buffer->buffer_B_count -= entry_cnt * (ssd->parameter->mapping_entry_size / 2);
				break;
			default:
				break;
			}

			free_B = ssd->dram->mapping_buffer->max_buffer_B - ssd->dram->mapping_buffer->buffer_B_count;

			ssd->dram->mapping_node_count--;
		}
		create_new_mapping_buffer(ssd, lpn, req,flag);
	}
	else  // there is the corresponding mapping node in the mappingb buffer 
	{
		//further judge whether the lpn mapping entry is cached in mapping buffer via bitmap
		if (ssd->dram->map->map_entry[lpn].cache_valid == 1)
			ssd->write_tran_cache_hit++;
		else
		{
			ssd->write_tran_cache_miss++;
			switch (DFTL)
			{
			case TPFTL:
				ssd->dram->map->map_entry[lpn].cache_valid = 1;
				ssd->dram->map->map_entry[lpn].dirty = 1;
				ssd->dram->mapping_buffer->buffer_B_count += ssd->parameter->mapping_entry_size;
				break;
			case DFTL_BASE:
				printf("Something cannot happen have happened!\n");
				//never happan
				getchar();
				break;
			case SFTL:
				//cache the mapping entry, new written lpn
				ssd->dram->map->map_entry[lpn].cache_valid = 1;
				break;
			default:
				break;
			}
		}
		//LRU management 
		if (ssd->dram->mapping_buffer->buffer_head != buffer_node)
		{
			if (ssd->dram->mapping_buffer->buffer_tail == buffer_node)
			{
				ssd->dram->mapping_buffer->buffer_tail = buffer_node->LRU_link_pre;
				buffer_node->LRU_link_pre->LRU_link_next = NULL;
			}
			else if (buffer_node != ssd->dram->mapping_buffer->buffer_head)
			{
				buffer_node->LRU_link_pre->LRU_link_next = buffer_node->LRU_link_next;
				buffer_node->LRU_link_next->LRU_link_pre = buffer_node->LRU_link_pre;
			}
			buffer_node->LRU_link_next = ssd->dram->mapping_buffer->buffer_head;
			ssd->dram->mapping_buffer->buffer_head->LRU_link_pre = buffer_node;
			buffer_node->LRU_link_pre = NULL;
			ssd->dram->mapping_buffer->buffer_head = buffer_node;
		}
	}
	return ssd;
}

struct ssd_info* create_new_mapping_buffer(struct ssd_info* ssd, unsigned int lpn, struct request* req, unsigned int flag)
{
	unsigned int vpn;
	struct buffer_group* new_node = NULL;
	unsigned int i;
	unsigned int tmp_lpn;
	struct sub_request* tran_read;

	vpn = lpn / ssd->map_entry_per_subpage;

	//create a new mapping node 
	new_node = (struct buffer_group*)malloc(sizeof(struct buffer_group));
	alloc_assert(new_node, "buffer_group_node");

	if (new_node == NULL)
	{
		printf("Buffer Node Allocation Error!\n");
		getchar();
		return ssd;
	}

	//different DFTL schemes lead to different loading granularity
	switch (DFTL)
	{
	case DFTL_BASE:
		new_node->entry_cnt = 1;
		ssd->dram->map->map_entry[lpn].dirty = 1;
		ssd->dram->map->map_entry[lpn].cache_valid = 1;
		new_node->group = lpn;
		break;
	case TPFTL:
		new_node->entry_cnt = 1;
		ssd->dram->map->map_entry[lpn].dirty = 1;
		ssd->dram->map->map_entry[lpn].cache_valid = 1;
		new_node->group = vpn;
		break;
	case SFTL:
		//read one translation subpage
		/*
		    req == NULL  means read translation miss and create translation read and hook into sub read request
		*/
		if (ssd->dram->tran_map->map_entry[vpn].state != 0 && flag == WRITE)  //not the first entry in the cached node 
		{
			tran_read = tran_read_sub_reqeust(ssd, vpn); //generate a new translation read
			if (tran_read != NULL)  //hook the sub translation read request into the request
			{
				tran_read->next_node = NULL;
				tran_read->next_subs = NULL;
				tran_read->update_cnt = 0;
				if (req == NULL){
					assert(0);
					//req = tran_read;
				}
				else
				{
					tran_read->next_subs = req->subs;
					req->subs = tran_read;
					tran_read->total_request = req;
				}
			}
		}
		 
		new_node->entry_cnt = ssd->map_entry_per_subpage;  //including invalid mapping entries 
		for (i = 0; i < ssd->map_entry_per_subpage; i++)
		{
			tmp_lpn = vpn * ssd->map_entry_per_subpage + i;
			if(ssd->dram->map->map_entry[tmp_lpn].state!=0)
				ssd->dram->map->map_entry[tmp_lpn].cache_valid = 1;
		}
		ssd->dram->tran_map->map_entry[vpn].dirty = 1;
		new_node->group = vpn;
		break;
	default:
		printf("Unidentifiable Phara\n");
		break;
	}
	ssd->dram->tran_map->map_entry[vpn].dirty = 1;
	new_node->LRU_link_pre = NULL;
	new_node->LRU_link_next = ssd->dram->mapping_buffer->buffer_head;
	if (ssd->dram->mapping_buffer->buffer_head != NULL)
	{
		ssd->dram->mapping_buffer->buffer_head->LRU_link_pre = new_node;
	}
	else
	{
		ssd->dram->mapping_buffer->buffer_tail = new_node;
	}
	ssd->dram->mapping_buffer->buffer_head = new_node;
	new_node->LRU_link_pre = NULL;
	avlTreeAdd(ssd->dram->mapping_buffer, (TREE_NODE*)new_node);
	switch (DFTL)
	{
	case DFTL_BASE:
	case TPFTL:
		ssd->dram->mapping_buffer->buffer_B_count += new_node->entry_cnt * ssd->parameter->mapping_entry_size;
		break;
	case SFTL:
		ssd->dram->mapping_buffer->buffer_B_count += new_node->entry_cnt * (ssd->parameter->mapping_entry_size/2);
		break;
	default:
		break;
	}

	ssd->dram->mapping_node_count++;
	return ssd;
}

struct sub_request * tran_read_sub_reqeust(struct ssd_info* ssd, unsigned int vpn)
{
	struct sub_request* sub;
	struct local* tem_loc;
	unsigned int pn;

	sub = (struct sub_request*)malloc(sizeof(struct sub_request));
	memset(sub, 0, sizeof(struct sub_request));

	if (sub == NULL)
	{
		return NULL;
	}
	sub->next_node = NULL;
	sub->next_subs = NULL;
	sub->update_cnt = 0;

	pn = ssd->dram->tran_map->map_entry[vpn].pn;

	if (pn == -1) //hit in sub reqeust queues
	{
		ssd->req_read_hit_cnt++;

		tem_loc = (struct local*)malloc(sizeof(struct local));
		memset(tem_loc, 0, sizeof(struct local));
		sub->location = tem_loc;

		sub->lpn = vpn;
		sub->current_state = SR_R_DATA_TRANSFER;
		sub->current_time = ssd->current_time;
		sub->next_state = SR_COMPLETE;                                         //置为完成状态
		sub->next_state_predict_time = ssd->current_time + 1000;
		sub->complete_time = ssd->current_time + 1000;
	}
	else
	{
		sub->read_flag = REQ_READ;
		sub->state = FULL_FLAG;  // all sectors needed to be read
			sub->size = secno_num_sub_page;
		sub->ppn = pn;
		sub->location = find_location_pun(ssd, pn);
		sub->lpn = vpn;
		creat_one_read_sub_req(ssd, sub);  //insert into channel read  req queue
	}
	return sub;
}


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
	new_request = ssd->request_work; 

	handle_write_buffer(ssd, new_request);
	if (new_request->subs == NULL)  //sub requests are cached in data buffer 
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
	unsigned int lsn, lun, last_lun, first_lun,i;
	unsigned int state,offset1 = 0, offset2 = 0;                                                                                       

	lsn = req->lsn;
	last_lun = (req->lsn + req->size - 1) / secno_num_sub_page;
	first_lun = req->lsn/ secno_num_sub_page;
	lun = first_lun;

	while (lun <= last_lun)     
	{
		state = 0; 
		offset1 = 0;
		offset2 = secno_num_sub_page - 1;

		if (lun == first_lun)
			offset1 = lsn - lun* secno_num_sub_page;
		if (lun == last_lun)
			offset2 = (lsn + req->size - 1) % secno_num_sub_page;

		for (i = offset1; i <= offset2; i++)
			state = SET_VALID(state, i);

		if (req->operation == READ)                                                   
			ssd = check_w_buff(ssd, lun, state, NULL, req);
		else if (req->operation == WRITE)
			ssd = insert2buffer(ssd, lun, state, NULL, req);
		lun++;
	}
	return ssd;
}

struct ssd_info *handle_read_cache(struct ssd_info *ssd, struct request *req)           //处理读缓存，待添加
{
	return ssd;
}

struct ssd_info * check_w_buff(struct ssd_info *ssd, unsigned int lpn, int state, struct sub_request *sub, struct request *req)
{
	struct buffer_group *buffer_node, key;

	key.group = lpn;
	buffer_node = (struct buffer_group*)avlTreeFind(ssd->dram->data_buffer, (TREE_NODE *)&key);

	if (buffer_node == NULL)
	{
		read_request(ssd, lpn, req, state, USER_DATA);
		ssd->dram->data_buffer->read_miss_hit++;         
	}
	else
	{
		if ((state&buffer_node->stored) == state)   
		{
			ssd->dram->data_buffer->read_hit++;
		}
		else    
		{ 
			read_request(ssd, lpn, req, state, USER_DATA);
			ssd->dram->data_buffer->read_miss_hit++;
		}
	}
	return ssd;
}

/*******************************************************************************
*The function is to write data to the buffer,Called by buffer_management()
********************************************************************************/
struct ssd_info * insert2buffer(struct ssd_info *ssd, unsigned int lpn, int state, struct sub_request *sub, struct request *req)
{
	int write_back_count;                                  
	unsigned int sector_count, free_sector = 0;
	struct buffer_group *buffer_node = NULL, *pt, *new_node = NULL, key;

	unsigned int sub_req_state = 0, sub_req_size = 0, sub_req_lpn = 0;

#ifdef DEBUG
	printf("enter insert2buffer,  current time:%I64u, lpn:%d, state:%d,\n", ssd->current_time, lpn, state);
#endif

	//sector_count = size(state); 
	sector_count = secno_num_sub_page;  //after 4KB aligning
	key.group = lpn;             
	buffer_node = (struct buffer_group*)avlTreeFind(ssd->dram->data_buffer, (TREE_NODE *)&key);   

	if (buffer_node == NULL)
	{
		if (ssd->dram->data_buffer->max_buffer_sector < sector_count + ssd->dram->data_buffer->buffer_sector_count)
		{	
#if WS_DEBUG
			assert(sector_count > free_sector);
#endif
			write_back_count = sector_count - free_sector;
			while (write_back_count > 0)
			{
				sub_req_state = ssd->dram->data_buffer->buffer_tail->stored;
				sub_req_size = secno_num_sub_page;   //after aligning
				sub_req_lpn = ssd->dram->data_buffer->buffer_tail->group;

				insert2_command_buffer(ssd, ssd->dram->data_command_buffer, sub_req_lpn, sub_req_state, req, USER_DATA);  //deal with tail sub-request
				ssd->dram->data_buffer->write_miss_hit++;
				ssd->dram->data_buffer->buffer_sector_count = ssd->dram->data_buffer->buffer_sector_count - sub_req_size;
				
				pt = ssd->dram->data_buffer->buffer_tail;
				avlTreeDel(ssd->dram->data_buffer, (TREE_NODE *)pt);
				if (ssd->dram->data_buffer->buffer_head->LRU_link_next == NULL){
					ssd->dram->data_buffer->buffer_head = NULL;
					ssd->dram->data_buffer->buffer_tail = NULL;
				}
				else{
					ssd->dram->data_buffer->buffer_tail = ssd->dram->data_buffer->buffer_tail->LRU_link_pre;
					ssd->dram->data_buffer->buffer_tail->LRU_link_next = NULL;
				}
				pt->LRU_link_next = NULL;
				pt->LRU_link_pre = NULL;
				AVL_TREENODE_FREE(ssd->dram->data_buffer, (TREE_NODE *)pt);
				pt = NULL;

				write_back_count = write_back_count - sub_req_size; 
			}
		}

		new_node = NULL;
		new_node = (struct buffer_group *)malloc(sizeof(struct buffer_group));
		alloc_assert(new_node, "buffer_group_node");
		memset(new_node, 0, sizeof(struct buffer_group));

		new_node->group = lpn;
		new_node->stored = state;
		new_node->LRU_link_pre = NULL;
		new_node->LRU_link_next = ssd->dram->data_buffer->buffer_head;
		if (ssd->dram->data_buffer->buffer_head != NULL){
			ssd->dram->data_buffer->buffer_head->LRU_link_pre = new_node;
		}
		else{
			ssd->dram->data_buffer->buffer_tail = new_node;
		}
		ssd->dram->data_buffer->buffer_head = new_node;
		new_node->LRU_link_pre = NULL;
		avlTreeAdd(ssd->dram->data_buffer, (TREE_NODE *)new_node);
		ssd->dram->data_buffer->buffer_sector_count += sector_count;
	}
	else
	{
		ssd->dram->data_buffer->write_hit++;
		if (req != NULL)
		{
			if (ssd->dram->data_buffer->buffer_head != buffer_node)
			{
				if (ssd->dram->data_buffer->buffer_tail == buffer_node)
				{
					ssd->dram->data_buffer->buffer_tail = buffer_node->LRU_link_pre;
					buffer_node->LRU_link_pre->LRU_link_next = NULL;
				}
				else if (buffer_node != ssd->dram->data_buffer->buffer_head)
				{
					buffer_node->LRU_link_pre->LRU_link_next = buffer_node->LRU_link_next;
					buffer_node->LRU_link_next->LRU_link_pre = buffer_node->LRU_link_pre;
				}
				buffer_node->LRU_link_next = ssd->dram->data_buffer->buffer_head;
				ssd->dram->data_buffer->buffer_head->LRU_link_pre = buffer_node;
				buffer_node->LRU_link_pre = NULL;
				ssd->dram->data_buffer->buffer_head = buffer_node;
			}
			req->complete_lsn_count += size(state);                                       
		}
	}
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

//Orderly insert to mapping command buffer 
struct ssd_info* insert2_mapping_command_buffer_in_order(struct ssd_info* ssd, unsigned int lpn, struct request* req)
{
	struct  buffer_group *front, *tmp;
	struct  buffer_group *new_node;

	new_node = (struct buffer_group*)malloc(sizeof(struct buffer_group));
	alloc_assert(new_node, "buffer_group_node");
	memset(new_node, 0, sizeof(struct buffer_group));
	
	
	new_node->LRU_link_next = NULL;
	new_node->LRU_link_pre = NULL;
	new_node->group = lpn;

	tmp = ssd->dram->mapping_command_buffer->buffer_head;
	front = tmp;
	if (tmp == NULL)
	{
		ssd->dram->mapping_command_buffer->buffer_head = new_node;
		ssd->dram->mapping_command_buffer->count++;
		return ssd;
	}

	while(tmp != NULL)
	{
		if (tmp->LRU_link_next == NULL)  //insert to tail
		{
			tmp->LRU_link_next = new_node;
			new_node->LRU_link_pre = tmp;
			ssd->dram->mapping_command_buffer->count++;
			break;
		}
		if (tmp->group < lpn)
		{
			front = tmp;
			tmp = tmp->LRU_link_next;
		}
		else if (tmp->group == lpn)
		{
			//no need to insert
			free(new_node);
			new_node = NULL;
			break;
		}
		else //insert the node
		{
			if (tmp ==  ssd->dram->mapping_command_buffer->buffer_head) //insert to the head  
			{
				new_node->LRU_link_next = tmp;
				tmp->LRU_link_pre = new_node;
				ssd->dram->mapping_command_buffer->buffer_head = new_node;
			}
			else
			{
				front->LRU_link_next = new_node;
				new_node->LRU_link_pre = front;
				new_node->LRU_link_next = tmp;
				tmp->LRU_link_pre = tmp;
			}
			ssd->dram->mapping_command_buffer->count++;

			if (ssd->dram->mapping_command_buffer->count == ssd->dram->mapping_command_buffer->max_command_buff_page)
			{
				//show_mapping_command_buffer(ssd);

				//trigger SMT dump
				smt_dump(ssd,req);
			}
			break;
		}
	}
	return ssd;
}

void show_mapping_command_buffer(struct ssd_info* ssd)
{
	struct  buffer_group* node;

	fprintf(ssd->smt, "******data**********\n");
	node = ssd->dram->mapping_command_buffer->buffer_head;
	while (node)
	{
		fprintf(ssd->smt, "%d ", node->group);
		node = node->LRU_link_next;
	}
	fprintf(ssd->smt, "\n");
	fflush(ssd->smt);
}

//SMT dump
struct ssd_info* smt_dump(struct ssd_info * ssd, struct request* req)
{
	struct  buffer_group *node, *tmp;
	unsigned int i;

	fprintf(ssd->smt, "*************Sorted Mapping Table***************\n");

	for (i = 0; i < ssd->dram->mapping_command_buffer->max_command_buff_page; i++)
	{
		node = ssd->dram->mapping_command_buffer->buffer_head;

		//write the reqeust  ->  fprintf mapping entries 
		fprintf(ssd->smt, "%d ", node->group);

	    //delete mapping entries from the mapping command  buffer
		tmp = node;
		node = node->LRU_link_next;
		ssd->dram->mapping_command_buffer->buffer_head = node;
		free(tmp);
		tmp = NULL;
		ssd->dram->mapping_command_buffer->count--;
	}
	fprintf(ssd->smt, "\n");
	fflush(ssd->smt);
	return ssd;
}

/* 
   insert to data commond buffer
      data_type = USER_DATA, state records the sector bitmap
	  data_type = MAPPING_DATA, state records the replaced mapping entry
 */
struct ssd_info * insert2_command_buffer(struct ssd_info* ssd, struct buffer_info * command_buffer, unsigned int lpn, unsigned int state, struct request* req, unsigned int data_type)
{
	unsigned int i = 0, j = 0;
	struct buffer_group *command_buffer_node = NULL, *pt, *new_node = NULL, key;
	struct sub_request *sub_req = NULL;
	unsigned int loop;
	unsigned int lun_state, lun;
	//unsigned int used_size, max_size;

	key.group = lpn;
	command_buffer_node = (struct buffer_group*)avlTreeFind(command_buffer, (TREE_NODE *)&key);

	if (state == 0)
	{
		printf("Debug Look Here\n");
	}

	if (command_buffer_node == NULL)
	{
		new_node = NULL;
		new_node = (struct buffer_group *)malloc(sizeof(struct buffer_group));
		alloc_assert(new_node, "buffer_group_node");
		memset(new_node, 0, sizeof(struct buffer_group));

		new_node->group = lpn;
		new_node->stored = state;

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

		if (command_buffer->command_buff_page >= command_buffer->max_command_buff_page)
		{
			loop = command_buffer->command_buff_page / ssd->parameter->subpage_page;

			//printf("begin to flush command_buffer\n");
			for (i = 0; i < loop; i++)
			{
				sub_req = (struct sub_request*)malloc(sizeof(struct sub_request));
				alloc_assert(sub_req, "sub_request");
				memset(sub_req, 0, sizeof(struct sub_request));
				sub_req->lun_count = 0;
				sub_req->req_type = data_type;

				for (j = 0; j < ssd->parameter->subpage_page; j++)
				{
					lun = command_buffer->buffer_tail->group;
					lun_state = command_buffer->buffer_tail->stored;

					if (data_type == USER_DATA)
					{
						if(lun_state!= 255)
							printf("Error: state errorr\n");
					}
					if (lun_state == 0)
					{
						printf("Debug Look Here: %s\n", __FUNCTION__);
					}
					sub_req->luns[sub_req->lun_count] = lun;
					sub_req->lun_state[sub_req->lun_count++] = lun_state;	

					//delete the data node from command buffer
					pt = command_buffer->buffer_tail;
					avlTreeDel(command_buffer, (TREE_NODE*)pt);
					if (command_buffer->buffer_head->LRU_link_next == NULL) {
						command_buffer->buffer_head = NULL;
						command_buffer->buffer_tail = NULL;
					}
					else {
						command_buffer->buffer_tail = command_buffer->buffer_tail->LRU_link_pre;
						command_buffer->buffer_tail->LRU_link_next = NULL;
					}
					pt->LRU_link_next = NULL;
					pt->LRU_link_pre = NULL;
					AVL_TREENODE_FREE(command_buffer, (TREE_NODE*)pt);
					pt = NULL;

					command_buffer->command_buff_page--;
				}
				create_sub_w_req(ssd, sub_req, req, data_type);
 			}
			if (command_buffer->command_buff_page != 0)
			{
				printf("command buff flush failed\n");
				getchar();
			}
		}
	}
	else  
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
		if(data_type == USER_DATA)
			command_buffer_node->stored = command_buffer_node->stored | state;
		else
		{
			command_buffer_node->stored = ssd->map_entry_per_subpage;
		}
	}

	/*
	  apply for free superblock  for used data
	  the reason why apply for new superblock at this point is to reduce the influence of mixture of GC data and user data
    */

	if (ssd->open_sb[data_type] == NULL)
		find_active_superblock(ssd, req, data_type);

	if (ssd->open_sb[data_type]->next_wr_page == ssd->parameter->page_block) //no free superpage in the superblock
		find_active_superblock(ssd, req, data_type);

	return ssd;
}

//req -> write sub request  
Status update_read_request(struct ssd_info *ssd, unsigned int lpn, unsigned int state, struct sub_request *req, unsigned int commond_buffer_type)  //in this code, the state is for sector and maximal 32 bits
{
	struct sub_request * sub = NULL;
	struct local * tem_loc = NULL;

	unsigned int pn,state1 = 0;

	if (commond_buffer_type == MAPPING_DATA)
	{
		printf("Cannot Happen in S-FTL\n");
	}

	//create sub quest 
	sub = (struct sub_request*)malloc(sizeof(struct sub_request));
	if (sub == NULL)
	{
		return 0;
	}

	sub->next_node = NULL;
	sub->next_subs = NULL;
	sub->tran_read = NULL;
	if (commond_buffer_type == DATA_COMMAND_BUFFER)
	{
		pn = translate(ssd,lpn,sub);
		state1 = ssd->dram->map->map_entry[lpn].state;
	}
	if (state1 == 0) //hit in reqeust queue
	{
		ssd->req_read_hit_cnt++;

		tem_loc = (struct local *)malloc(sizeof(struct local));
		sub->location = tem_loc;
		sub->current_state = SR_R_DATA_TRANSFER;
		sub->current_time = ssd->current_time;
		sub->next_state = SR_COMPLETE;                                       
		sub->next_state_predict_time = ssd->current_time + 1000;
		sub->complete_time = ssd->current_time + 1000;

		insert2update_reqs(ssd, req, sub);
		return SUCCESS;
	}

	sub->location = find_location_pun(ssd, pn);
	
	sub->read_flag = UPDATE_READ;

	if (ssd->channel_head[sub->location->channel].chip_head[sub->location->chip].die_head[sub->location->die].plane_head[sub->location->plane].blk_head[sub->location->block].page_head[sub->location->page].luns[sub->location->sub_page] != lpn)
	{
		printf("Update Read Error!\n");
		getchar();
	}

	sub->lpn = lpn;
	sub->ppn = pn;
	sub->size = secno_num_sub_page;

	creat_one_read_sub_req(ssd, sub);  //insert into channel read  req queue
	insert2update_reqs(ssd, req, sub); 

	return SUCCESS;
}

void insert2update_reqs(struct ssd_info* ssd, struct sub_request* req, struct sub_request* update)
{
	req->update[req->update_cnt] = update;
	req->update_cnt++;
	return;
}


unsigned int translate(struct ssd_info* ssd, unsigned int lpn, struct sub_request* sub)
{
	unsigned int ppn = INVALID_PPN;
	unsigned int vpn;
	struct sub_request* tran_read;

	sub->tran_read = NULL;
	vpn = lpn / ssd->map_entry_per_subpage;

	if (DFTL)  // dftl schemes are used 
	{
		if (ssd->dram->map->map_entry[lpn].state != 0)  //valid mapping entry
		{
			if (ssd->dram->map->map_entry[lpn].cache_valid == 1 || ssd->dram->tran_map->map_entry[vpn].state == 0)  //cached in mapping buffer or sub  request queue
			{
				ppn = ssd->dram->map->map_entry[lpn].pn;
				ssd->read_tran_cache_hit++;
			}
			else  //obtain the mapping entry from the flash
			{
				ssd->read_tran_cache_miss++;
				
				//create one read translation requets
				tran_read = tran_read_sub_reqeust(ssd,vpn);

				//hook the translation read into sub request
				sub->tran_read = tran_read;

				ppn = ssd->dram->map->map_entry[lpn].pn;
				//insert to mapping buffer 
				ssd->dram->tran_map->map_entry[vpn].state++;
				//create_new_mapping_buffer(ssd, lpn, NULL);
				insert2map_buffer(ssd, lpn, sub->total_request,READ);
			}
		}
		else
		{
			printf("Maybe in the sub requet queue \n");
			//printf("Error! read the invalid data\n");
			//getchar();
		}
	}
	else
	{
		if (ssd->dram->map->map_entry[lpn].state == 0) //
		{
			ppn = -1;
		}
		else
		{
			ppn = ssd->dram->map->map_entry[lpn].pn;
		}
	}
	return ppn;
}

int read_request(struct ssd_info *ssd, unsigned int lpn, struct request *req, unsigned int state,unsigned int data_type)
{
	struct sub_request* sub = NULL;
	struct local *tem_loc = NULL;

	unsigned int pn = 0;

	//create a sub request 
	sub = (struct sub_request*)malloc(sizeof(struct sub_request));
	alloc_assert(sub, "sub");
	sub->next_node = NULL;
	sub->next_subs = NULL;
	sub->update_cnt = 0;
	
	assert(req != NULL);

	sub->next_subs = req->subs;
	req->subs = sub;
	sub->total_request = req;

	//address translation 
	pn = translate(ssd, lpn, sub);

	if (pn == -1) //hit in sub reqeust queue
	{
		ssd->req_read_hit_cnt++;
		tem_loc = (struct local*)malloc(sizeof(struct local));
		alloc_assert(tem_loc, "FUNCTION : read_request");
		memset(tem_loc, 0, sizeof(struct local));
		sub->location = tem_loc;
		sub->lpn = lpn;
		sub->current_state = SR_R_DATA_TRANSFER;
		sub->current_time = ssd->current_time;
		sub->next_state = SR_COMPLETE;                                      
		sub->next_state_predict_time = ssd->current_time + 1000;
		sub->complete_time = ssd->current_time + 1000;

		return SUCCESS;
	}
	else
	{
		sub->read_flag = REQ_READ;
		if (data_type == USER_DATA)
		{
			sub->state = state;
			sub->size = size(state);
		}
		else  //translation data 
		{
			sub->state = FULL_FLAG;  // all sectors needed to be read
			sub->size = secno_num_sub_page;
		}
		sub->ppn = pn;
		sub->location = find_location_pun(ssd, pn);
		sub->lpn = lpn;

		creat_one_read_sub_req(ssd, sub);  //insert into channel read  req queue
		return SUCCESS;
	}
}

Status create_sub_w_req(struct ssd_info* ssd, struct sub_request* sub, struct request* req, unsigned int data_type)
{
	unsigned int i; 
	unsigned int lun,lun_state;

	if (sub->lun_count != ssd->parameter->subpage_page)
	{
		printf("Cannot Create Sub Write Request\n");
	}

	sub->next_node = NULL;
	sub->next_subs = NULL;
	sub->operation = WRITE;
	sub->location = (struct local*)malloc(sizeof(struct local));
	alloc_assert(sub->location, "sub->location");
	memset(sub->location, 0, sizeof(struct local));
	sub->current_state = SR_WAIT;
	sub->current_time = ssd->current_time;
	sub->size = secno_num_per_page;
	sub->begin_time = ssd->current_time;
	sub->update_cnt = 0;

	if (req != NULL)
	{
		sub->next_subs = req->subs;
		req->subs = sub;
		sub->total_request = req;
	}

	//allocate free page 
	ssd->open_sb[data_type]->pg_off = (ssd->open_sb[data_type]->pg_off + 1) % ssd->sb_pool[data_type].blk_cnt;
	sub->location->channel = ssd->open_sb[data_type]->pos[ssd->open_sb[data_type]->pg_off].channel;
	sub->location->chip = ssd->open_sb[data_type]->pos[ssd->open_sb[data_type]->pg_off].chip;
	sub->location->die = ssd->open_sb[data_type]->pos[ssd->open_sb[data_type]->pg_off].die;
	sub->location->plane = ssd->open_sb[data_type]->pos[ssd->open_sb[data_type]->pg_off].plane;
	sub->location->block = ssd->open_sb[data_type]->pos[ssd->open_sb[data_type]->pg_off].block;
	sub->location->page = ssd->open_sb[data_type]->next_wr_page;

	if (ssd->open_sb[data_type]->next_wr_page == ssd->parameter->page_block)
	{
		printf("Debug LOOK Here\n");
	}

	if (ssd->open_sb[data_type]->pg_off == ssd->sb_pool[data_type].blk_cnt - 1)
		ssd->open_sb[data_type]->next_wr_page++;


	sub->ppn = find_ppn(ssd, sub->location->channel, sub->location->chip, sub->location->die, sub->location->plane, sub->location->block, sub->location->page);

	//handle update write 
	for (i = 0; i < sub->lun_count; i++)
	{
		lun = sub->luns[i];
		lun_state = sub->lun_state[i]; // data_type == 0 (user data) state is the sector state; data_type = 1 (mapping data),state is the valid mapping entry count

		if (data_type == USER_DATA)
		{
			if (ssd->dram->map->map_entry[lun].state != 0)  // maybe update read
			{
				if (size(lun_state) != secno_num_sub_page)  //patitial write
				{
					update_read_request(ssd, lun, lun_state, sub, USER_DATA);
				}
			}
		}
		else  //mapping data 
		{
			switch (DFTL)
			{
			case SFTL:
				//no update write in S-FTL
				break;
			default:
				break;
			}
		}

	}
	//insert into sub request queue
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

	//Write_cnt(ssd, sub->location->channel);
	return SUCCESS;
}

Status creat_one_read_sub_req(struct ssd_info *ssd, struct sub_request* sub)
{
	unsigned int lpn,flag;
	struct local *loc = NULL;
	struct sub_request* sub_r;
	unsigned int channel, chip, die, plane, block, page, subpage,flash_lpn;

	channel = sub->location->channel;
	chip = sub->location->chip;
	die = sub->location->die;
	plane = sub->location->plane;
	block = sub->location->block;
	page = sub->location->page;
	subpage = sub->location->sub_page;
	flash_lpn = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[page].luns[subpage];

	lpn = sub->lpn;

	if (flash_lpn != lpn)
	{
		printf("Read Request Error\n");
	}

	sub->begin_time = ssd->current_time;
	sub->current_state = SR_WAIT;
	sub->current_time = 0x7fffffffffffffff;
	sub->next_state = SR_R_C_A_TRANSFER;
	sub->next_state_predict_time = 0x7fffffffffffffff;
	sub->suspend_req_flag = NORMAL_TYPE;

	loc = sub->location;
	
	sub->operation = READ;
	sub_r = ssd->channel_head[loc->channel].subs_r_head;

	flag = 0;
	while (sub_r != NULL)
	{
		if (sub_r->ppn == sub->ppn)                          
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
	
	if (flag == 0)          
	{
		ssd->channel_head[loc->channel].chip_head[loc->chip].die_head[loc->die].read_cnt++;
		if (ssd->channel_head[loc->channel].subs_r_tail != NULL)
		{
			ssd->channel_head[loc->channel].subs_r_tail->next_node = sub;       
			ssd->channel_head[loc->channel].subs_r_tail = sub;
		}
		else
		{
#if WS_DEBUG
			assert(ssd->channel_head[loc->channel].subs_r_head == NULL);
#endif //WS_DEBUG
			ssd->channel_head[loc->channel].subs_r_head = sub;
			ssd->channel_head[loc->channel].subs_r_tail = sub;
		}
	}
	else
	{
		sub->current_state = SR_R_DATA_TRANSFER;
		sub->current_time = ssd->current_time;
		sub->next_state = SR_COMPLETE;                                        
		sub->next_state_predict_time = ssd->current_time + 1000;
		sub->complete_time = ssd->current_time + 1000;
	}
	return SUCCESS;
}