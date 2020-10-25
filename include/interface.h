#ifndef _INTERFACE_H_
#define _INTERFACE_H_

#include "initialize.h"

int get_requests(struct ssd_info *);
int64_t find_nearest_event(struct ssd_info *);

#endif //_INTERFACE_H_