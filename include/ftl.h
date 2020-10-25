#ifndef _FTL_H_
#define _FTL_H_

#include "initialize.h"
#include "buffer.h"
#include "flash.h"

struct local *find_location(struct ssd_info *ssd, unsigned int ppn);
unsigned int find_ppn(struct ssd_info *ssd, unsigned int channel, unsigned int chip, unsigned int die, unsigned int plane, unsigned int block, unsigned int page);
int  find_active_block(struct ssd_info *ssd, unsigned int channel, unsigned int chip, unsigned int die, unsigned int plane);

Status find_active_superblock(struct ssd_info* ssd, struct request* req, unsigned int type);

Status migration(struct ssd_info *ssd, unsigned int victim);
Status Is_Garbage_SBlk(struct ssd_info *ssd, int sb_no);
Status SuperBlock_GC(struct ssd_info* ssd, struct request* req);
int find_victim_superblock(struct ssd_info *ssd);
int Get_SB_PE(struct ssd_info *ssd, unsigned int sb_no);
int Get_SB_Invalid(struct ssd_info *ssd, unsigned int sb_no);
Status migration_horizon(struct ssd_info* ssd, struct request* req, unsigned int victim);
Status IS_superpage_valid(struct ssd_info* ssd, unsigned int sb_no, unsigned int page);
unsigned int find_pun(struct ssd_info* ssd, unsigned int channel, unsigned int chip, unsigned int die, unsigned int plane, unsigned int block, unsigned int page, unsigned int unit);

#endif //_FTL_H_