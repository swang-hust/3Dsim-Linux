#ifndef _FCL_H_
#define _FCL_H_

struct ssd_info *dynamic_advanced_process(struct ssd_info *ssd, unsigned int channel, unsigned int chip);
//struct ssd_info *delete_from_ssd(struct ssd_info *ssd, unsigned int channel, struct sub_request * sub_req);
struct ssd_info *delete_from_channel(struct ssd_info *ssd, unsigned int channel, struct sub_request * sub_req);

struct ssd_info *make_same_level(struct ssd_info *, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int);
struct ssd_info *compute_serve_time(struct ssd_info *ssd, unsigned int channel, unsigned int chip, unsigned int die, struct sub_request **subs, unsigned int subs_count, unsigned int command);

unsigned int find_mutliplane_sub_request(struct ssd_info * ssd, unsigned int channel, unsigned int chip, struct sub_request ** sub_mutliplane_place, unsigned int command);
unsigned int find_read_sub_request(struct ssd_info * ssd, unsigned int channel, unsigned int chip, unsigned int die, struct sub_request **subs, unsigned int state, unsigned int command);
unsigned int find_r_wait_sub_request(struct ssd_info * ssd, unsigned int channel, unsigned int chip, struct sub_request ** sub_place, unsigned int command);
unsigned int find_static_write_sub_request(struct ssd_info *ssd, unsigned int channel, unsigned int chip, struct sub_request ** subs, unsigned int command);

Status check_req_in_suspend(struct ssd_info * ssd, unsigned int channel, unsigned int chip, struct sub_request * sub_plane_request);

Status find_level_page(struct ssd_info *ssd, unsigned int channel, unsigned int chip, unsigned int die, struct sub_request **sub, unsigned int subs_count);
Status go_one_step(struct ssd_info * ssd, struct sub_request **subs, unsigned int subs_count, unsigned int aim_state, unsigned int command);

Status services_2_r_read(struct ssd_info * ssd);
Status services_2_r_wait(struct ssd_info * ssd, unsigned int channel);
Status services_2_r_data_trans(struct ssd_info * ssd, unsigned int channel);
Status services_2_r_complete(struct ssd_info * ssd);
Status services_2_write(struct ssd_info * ssd, unsigned int channel);
Status service_advance_command(struct ssd_info *ssd, unsigned int channel, unsigned int chip, struct sub_request ** subs, unsigned int subs_count, unsigned int aim_subs_count, unsigned int command);
Status service_2_read(struct ssd_info *ssd, unsigned int channel);
Status Is_Update_Read_Done(struct ssd_info *ssd, struct sub_request *sub);
Status migration_horizon(struct ssd_info *ssd, unsigned int victim);
Status IS_superpage_valid(struct ssd_info *ssd, unsigned int sb_no, unsigned int page);

#endif //_FCL_H_