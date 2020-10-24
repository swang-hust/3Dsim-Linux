#ifndef _BUFFER_H_
#define _BUFFER_H_

struct ssd_info *buffer_management(struct ssd_info *);
struct ssd_info *no_buffer_distribute(struct ssd_info *);
struct ssd_info * getout2buffer(struct ssd_info *ssd, struct sub_request *sub, struct request *req);
struct ssd_info * check_w_buff(struct ssd_info *ssd, unsigned int lpn, int state, struct sub_request *sub, struct request *req);
struct ssd_info * insert2buffer(struct ssd_info *ssd, unsigned int lpn, int state, struct sub_request *sub, struct request *req);
struct sub_request * creat_sub_request(struct ssd_info * ssd, unsigned int lpn, int size, unsigned int state, struct request * req, unsigned int operation, unsigned int command_type);
struct ssd_info * insert2_command_buffer(struct ssd_info * ssd, struct buffer_info * command_buffer, unsigned int lpn, int size_count, unsigned int state, struct request * req, unsigned int operation, unsigned int commond_buffer_type);
struct ssd_info * distribute2_command_buffer(struct ssd_info * ssd, unsigned int lpn, int size_count, unsigned int state, struct request * req, unsigned int operation);
struct allocation_info * allocation_method(struct ssd_info *ssd, unsigned int lpn, unsigned int use_flag);


unsigned int size(unsigned int);
int64_t calculate_distance(struct ssd_info * ssd, struct buffer_info * die_buffer, unsigned int lpn);
Status allocate_location(struct ssd_info * ssd, struct sub_request *sub_req);

struct ssd_info *handle_write_buffer(struct ssd_info *ssd, struct request *req);
struct ssd_info *handle_read_cache(struct ssd_info *ssd, struct request *req);
struct ssd_info *flush_all(struct ssd_info *ssd);
struct ssd_info * insert2_command_buffer_old(struct ssd_info * ssd, unsigned int lpn, int size_count, unsigned int state, struct request * req, unsigned int operation);
struct ssd_info * insert2buffer_old(struct ssd_info *ssd, unsigned int lpn, int state, struct sub_request *sub, struct request *req);

Status creat_one_read_sub_req(struct ssd_info *ssd, struct sub_request* sub);
Status update_read_request(struct ssd_info *ssd, unsigned int lpn, unsigned int state, struct sub_request *req, unsigned int commond_buffer_type);
Status Match_update(struct ssd_inof *ssd, unsigned int i, struct sub_request *req, struct sub_request *sub);
Status read_reqeust(struct ssd_info *ssd, unsigned int lpn, struct request *req, unsigned int state, unsigned int buffer_commond_type);
Status Is_exist_data(struct ssd_info *ssd, int para_level, unsigned int state, unsigned int off);

#endif //_BUFFER_H_