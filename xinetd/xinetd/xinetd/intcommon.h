#ifndef INTCOMMON_H
#define INTCOMMON_H

#include "config.h"
#include <sys/types.h>
#include "int.h"

void int_fail(const struct intercept_s *ip,const char *lsyscall);
int int_select(int max,fd_set *read_mask);
#ifdef __GNUC__
__attribute__ ((noreturn))
#endif
void int_exit(struct intercept_s *ip);
void int_init(struct intercept_s *ip,struct server *serp);

channel_s *int_newconn( struct intercept_s *ip, union xsockaddr *sinp, 
                        int remote_socket );
channel_s *int_lookupconn( struct intercept_s *ip, union xsockaddr *sinp, 
                           bool_int *addr_checked );
#endif

