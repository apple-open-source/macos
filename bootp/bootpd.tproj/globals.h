
#ifndef _S_GLOBALS_H
#define _S_GLOBALS_H

#import <objc/Object.h>
#include "NIDomain.h"
#include "NICache.h"

extern NICache_t	cache;
extern int		bootp_socket;
extern int		debug;
extern int		detect_other_dhcp_server;
extern NIDomain_t *	ni_local;
extern NIDomainList_t	niSearchDomains;
extern int		quiet;
extern unsigned short	server_priority;
extern u_int16_t	reply_threshold_seconds;
extern u_char		rfc_magic[4];
extern char		server_name[MAXHOSTNAMELEN + 1];
extern id		subnets;
extern char *		testing_control;
extern char		transmit_buffer[];
extern int		verbose;
#endif _S_GLOBALS_H
