#ifndef _POWER_MGMT_H
#define _POWER_MGMT_H

#include <sys/types.h>

extern time_t slept_at;
extern time_t woke_at;
extern time_t swept_at;

extern int init_power_mgmt (void);
extern void cleanup_power_mgmt (void);
extern void check_power_mgmt (void);

#endif /* _POWER_MGMT_H */
