
#ifndef _S_GLOBALS_H
#define _S_GLOBALS_H
#include <mach/boolean.h>

extern u_short 			G_client_port;
extern boolean_t		G_dhcp_accepts_bootp;
extern boolean_t		G_dhcp_failure_configures_linklocal;
extern boolean_t		G_dhcp_success_deconfigures_linklocal;
extern u_long			G_dhcp_init_reboot_retry_count;
extern u_long			G_dhcp_select_retry_count;
extern u_long			G_dhcp_allocate_linklocal_at_retry_count;
extern u_short 			G_server_port;
extern u_long			G_gather_secs;
extern u_long			G_initial_wait_secs;
extern u_long			G_max_wait_secs;
extern u_long			G_gather_secs;
extern u_long			G_link_inactive_secs;
extern u_long			G_max_retries;
extern boolean_t 		G_must_broadcast;
extern int			G_verbose;
extern int			G_debug;

extern const unsigned char	G_rfc_magic[4];
extern const struct sockaddr	G_blank_sin;
extern const struct in_addr	G_ip_broadcast;
extern const struct in_addr	G_ip_zeroes;

extern void my_log(int priority, const char * message, ...);

#endif _S_GLOBALS_H
