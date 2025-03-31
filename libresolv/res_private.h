/* $FreeBSD$ */

#ifndef res_private_h
#define res_private_h

#ifdef __APPLE__
#include <sys/time.h>

/*
 * Status codes from dns_res_xxx SPIs
 * positive numbers are ns_rcode values.
 */
#define DNS_RES_STATUS_TIMEOUT -1001
#define DNS_RES_STATUS_CANCELLED -1002
#define DNS_RES_STATUS_INVALID_QUERY -1003
#define DNS_RES_STATUS_INVALID_ARGUMENT -1004
#define DNS_RES_STATUS_INVALID_RES_STATE -1005
#define DNS_RES_STATUS_INVALID_REPLY -1006
#define DNS_RES_STATUS_CONNECTION_REFUSED -1007
#define DNS_RES_STATUS_SEND_FAILED -1008
#define DNS_RES_STATUS_CONNECTION_FAILED -1009
#define DNS_RES_STATUS_SYSTEM_ERROR -1010

#define RES_EXT_SUFFIX_LEN 64

#include <resolv.h>
#endif /* __APPLE__ */

struct __res_state_ext {
	union res_sockaddr_union nsaddrs[MAXNS];
	struct sort_list {
		int     af;
		union {
			struct in_addr  ina;
			struct in6_addr in6a;
		} addr, mask;
	} sort_list[MAXRESOLVSORT];
	char nsuffix[64];
#ifdef __APPLE__
	char bsuffix[64];
#endif	/* __APPLE__ */
	char nsuffix2[64];
	struct timespec	conf_mtim;	/* mod time of loaded resolv.conf */
	time_t		conf_stat;	/* time of last stat(resolv.conf) */
	u_short	reload_period;		/* seconds between stat(resolv.conf) */
};

extern int
res_ourserver_p(const res_state statp, const struct sockaddr *sa);

#ifdef __APPLE__

struct sockaddr * get_nsaddr(res_state statp, size_t n);

res_state res_state_new(void);

int dns_res_send(res_state statp, const u_char *buf, int buflen, u_char *ans, int *anssiz, struct sockaddr *from, int *fromlen);

int res_check_if_exit_requested(res_state statp, int notify_token);
void res_client_close(res_state res);
int res_nsend_2(res_state statp, const u_char *buf, int buflen, u_char *ans, int anssiz, struct sockaddr *from, int *fromlen);
int res_query_mDNSResponder(res_state statp, const char *name, int class, int type, u_char *answer, int anslen, struct sockaddr *from, uint32_t *fromlen);

#endif	/* __APPLE__ */

#endif

/*! \file */
