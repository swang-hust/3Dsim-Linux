#ifndef _FTL_H_
#define _FTL_H_

struct ssd_info *pre_process_page(struct ssd_info *ssd);
struct local *find_location(struct ssd_info *ssd, unsigned int ppn);
struct ssd_info *get_ppn(struct ssd_info *ssd, unsigned int channel, unsigned int chip, unsigned int die, unsigned int plane, struct sub_request *sub);
void gc_check(struct ssd_info *ssd, unsigned int channel, unsigned int chip, unsigned int die, unsigned int old_plane);
unsigned int gc(struct ssd_info *ssd, unsigned int channel, unsigned int flag);
unsigned int get_ppn_for_pre_process(struct ssd_info *ssd, unsigned int lpn);
unsigned int get_ppn_for_gc(struct ssd_info *ssd, unsigned int channel, unsigned int chip, unsigned int die, unsigned int plane);
unsigned int find_ppn(struct ssd_info * ssd, unsigned int channel, unsigned int chip, unsigned int die, unsigned int plane, unsigned int block, unsigned int page);

int gc_for_channel(struct ssd_info *ssd, unsigned int channel, unsigned int flag);
int delete_gc_node(struct ssd_info *ssd, unsigned int channel, struct gc_operation *gc_node);
Status get_ppn_for_normal_command(struct ssd_info * ssd, unsigned int channel, unsigned int chip, struct sub_request * sub);
int  find_active_block(struct ssd_info *ssd, unsigned int channel, unsigned int chip, unsigned int die, unsigned int plane);

int gc_direct_erase(struct ssd_info *ssd, unsigned int channel, unsigned int chip, unsigned int die);
int greedy_gc(struct ssd_info *ssd, unsigned int channel, unsigned int chip, unsigned int die);
int get_ppn_for_advanced_commands(struct ssd_info *ssd, unsigned int channel, unsigned int chip, struct sub_request * * subs, unsigned int subs_count, unsigned int command);
struct ssd_info * suspend_erase_operation(struct ssd_info * ssd, unsigned int channel, unsigned int chip, unsigned int die, unsigned int * erase_block);
int resume_erase_operation(struct ssd_info * ssd, unsigned int channel, unsigned int chip);
struct ssd_info *delete_suspend_command(struct ssd_info *ssd, unsigned int channel, unsigned int chip, struct suspend_spot * suspend_command);
struct allocation_info* pre_process_allocation(struct ssd_info *ssd, unsigned int lpn);

Status find_active_superblock(struct ssd_info *ssd, unsigned int type);

Status migration(struct ssd_info *ssd, unsigned int victim);
Status SuperBlock_GC(struct ssd_info *ssd);
int find_victim_superblock(struct ssd_info *ssd);
int Get_SB_PE(struct ssd_info *ssd, unsigned int sb_no);
int Get_SB_Invalid(struct ssd_info *ssd, unsigned int sb_no);

#endif //_FTL_H_