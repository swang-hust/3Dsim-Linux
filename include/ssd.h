#ifndef _SSD_H_
#define _SSD_H_

#include "initialize.h"
#include "interface.h"
#include "buffer.h"
#include "fcl.h"

void trace_output(struct ssd_info* );
void statistic_output(struct ssd_info *);
void free_all_node(struct ssd_info *);

struct ssd_info *warm_flash(struct ssd_info *ssd);
struct ssd_info *make_aged(struct ssd_info *);
struct ssd_info *pre_process_write(struct ssd_info *ssd);
struct ssd_info *process(struct ssd_info *);
struct ssd_info *simulate(struct ssd_info *);
void tracefile_sim(struct ssd_info *ssd);
void delete_update(struct ssd_info *ssd, struct sub_request *sub);

struct ssd_info *warm_flash(struct ssd_info *ssd);
void reset(struct ssd_info *ssd);
void Calculate_Energy(struct ssd_info *ssd);

#endif //_SSD_H_