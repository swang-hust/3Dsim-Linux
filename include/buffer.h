#ifndef _BUFFER_H_
#define _BUFFER_H_

#include "initialize.h"
#include "ftl.h"

struct ssd_info *buffer_management(struct ssd_info *);
struct ssd_info *no_buffer_distribute(struct ssd_info *);
struct ssd_info * check_w_buff(struct ssd_info *ssd, unsigned int lpn, int state, struct sub_request *sub, struct request *req);
struct ssd_info * insert2buffer(struct ssd_info *ssd, unsigned int lpn, int state, struct sub_request *sub, struct request *req);
struct ssd_info * insert2_command_buffer(struct ssd_info * ssd, struct buffer_info * command_buffer,unsigned int lpn,unsigned int state,struct request * req,unsigned int data_type);

Status create_sub_w_req(struct ssd_info* ssd, struct sub_request* sub, struct request* req, unsigned int data_type);

unsigned int size(unsigned int);

struct ssd_info *handle_write_buffer(struct ssd_info *ssd, struct request *req);
struct ssd_info *handle_read_cache(struct ssd_info *ssd, struct request *req);

Status creat_one_read_sub_req(struct ssd_info* ssd, struct sub_request* sub);
Status update_read_request(struct ssd_info* ssd, unsigned int lpn, unsigned int state, struct sub_request* req, unsigned int commond_buffer_type);
Status read_reqeust(struct ssd_info* ssd, unsigned int lpn, struct request* req, unsigned int state, unsigned int buffer_commond_type);
struct sub_request* tran_read_sub_reqeust(struct ssd_info* ssd, unsigned int lpn);

struct ssd_info* insert2_mapping_command_buffer_in_order(struct ssd_info* ssd, unsigned int lpn, struct request* req);
struct ssd_info* smt_dump(struct ssd_info* ssd, struct request* req);
void show_mapping_command_buffer(struct ssd_info* ssd);
void insert2update_reqs(struct ssd_info* ssd, struct sub_request* req, struct sub_request* update);

struct ssd_info* insert2map_buffer(struct ssd_info* ssd, unsigned int lpn, struct request* req, unsigned int flag);
struct ssd_info* create_new_mapping_buffer(struct ssd_info* ssd, unsigned int lpn, struct request* req, unsigned int flag);

unsigned int translate(struct ssd_info* ssd, unsigned int lpn, struct sub_request* sub);

#endif //_BUFFER_H_