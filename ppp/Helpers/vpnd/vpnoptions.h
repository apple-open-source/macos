/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef __VPNOPTIONS_H__
#define __VPNOPTIONS_H__

#ifndef DAEMON_NAME
    #define DAEMON_NAME "vpnd"
#endif

#define SERVER_TYPE_PPP		1
#define SERVER_TYPE_IPSEC	2

#define OPT_STR_LEN 256
#define MAXARG 100

/* PATHS definitions for PPP */

#define DIR_KEXT		"/System/Library/Extensions/"
#define DIR_LOGS		"/var/log/ppp/"
#define PATH_PPPD 		"/usr/sbin/pppd"
#define PPPD_PRGM 		"pppd"
#define PATH_VPND		"/usr/sbin/vpnd"
#define VPND_PRGM		"vpnd"

/* Comm default values */

#define OPT_COMM_IDLETIMER_DEF 		0	// no idle timer
#define OPT_COMM_SESSIONTIMER_DEF 	0	// no session timer

/* Values for flags */
#define OPT_VALUE	0xff	/* mask for presupplied value */
#define OPT_HEX		0x100	/* int option is in hex */
#define OPT_NOARG	0x200	/* option doesn't take argument */
#define OPT_OR		0x400	/* OR in argument to value */
#define OPT_INC		0x800	/* increment value */
#define OPT_A2OR	0x800	/* for o_bool, OR arg to *(u_char *)addr2 */
#define OPT_PRIV	0x1000	/* privileged option */
#define OPT_STATIC	0x2000	/* string option goes into static array */
#define OPT_LLIMIT	0x4000	/* check value against lower limit */
#define OPT_ULIMIT	0x8000	/* check value against upper limit */
#define OPT_LIMITS	(OPT_LLIMIT|OPT_ULIMIT)
#define OPT_ZEROOK	0x10000	/* 0 value is OK even if not within limits */
#define OPT_HIDE	0x10000	/* for o_string, print value as ?????? */
#define OPT_A2LIST	0x10000 /* for o_special, keep list of values */
#define OPT_A2CLRB	0x10000 /* o_bool, clr val bits in *(u_char *)addr2 */
#define OPT_NOINCR	0x20000	/* value mustn't be increased */
#define OPT_ZEROINF	0x40000	/* with OPT_NOINCR, 0 == infinity */
#define OPT_PRIO	0x80000	/* process option priorities for this option */
#define OPT_PRIOSUB	0x100000 /* subsidiary member of priority group */
#define OPT_ALIAS	0x200000 /* option is alias for previous option */
#define OPT_A2COPY	0x400000 /* addr2 -> second location to rcv value */
#define OPT_ENABLE	0x800000 /* use *addr2 as enable for option */
#define OPT_A2CLR	0x1000000 /* clear *(bool *)addr2 */
#define OPT_PRIVFIX	0x2000000 /* user can't override if set by root */
#define OPT_INITONLY	0x4000000 /* option can only be set in init phase */
#define OPT_DEVEQUIV	0x8000000 /* equiv to device name */
#define OPT_DEVNAM	(OPT_INITONLY | OPT_DEVEQUIV)
#define OPT_A2PRINTER	0x10000000 /* *addr2 is a fn for printing option */
#define OPT_A2STRVAL	0x20000000 /* *addr2 points to current string value */
#define OPT_NOPRINT	0x40000000 /* don't print this option at all */

#define OPT_VAL(x)	((x) & OPT_VALUE)

/* Values for priority */
#define OPRIO_DEFAULT	0	/* a default value */
#define OPRIO_CFGFILE	1	/* value from a configuration file */
#define OPRIO_CMDLINE	2	/* value from the command line */
#define OPRIO_SECFILE	3	/* value from options in a secrets file */
#define OPRIO_ROOT	100	/* added to priority if OPT_PRIVFIX && root */

/* LCP default values */	

#define OPT_LCP_ACCOMP_DEF 		1	// address and control fields compression activated
#define OPT_LCP_PCOMP_DEF 		1	// protocol field compression activated
#define OPT_LCP_RCACCM_DEF 		0	// default asyncmap value
#define OPT_LCP_TXACCM_DEF 		0	// default asyncmap value
#define OPT_LCP_MRU_DEF 		1500
#define OPT_LCP_MRU_PPPoE_DEF 		1492	/* use standart PPPoE MTU */
#define OPT_LCP_MRU_PPTP_DEF 		1500	/* use standart PPP MTU */
#define OPT_LCP_MRU_L2TP_DEF 		1500	/* use standart PPP MTU */
#define OPT_LCP_MTU_DEF 		1500
#define OPT_LCP_MTU_PPPoE_DEF 		1492	/* use standart PPPoE MTU */
#define OPT_LCP_MTU_PPTP_DEF 		1448	/* avoid fragmentation */
                                                /* 1500-IPHdr(20)-GRE(16)-PPP/MPPE(8)-PPPoE(8) */
#define OPT_LCP_MTU_L2TP_DEF 		1280	/* avoid fragmentation */
#define OPT_LCP_ECHOINTERVAL_DEF	10
#define OPT_LCP_ECHOFAILURE_DEF		4

/* IPCP default values */

#define OPT_IPCP_HDRCOMP_DEF 		1	// tcp vj compression activated

enum opt_type {
	o_special_noarg = 0,
	o_special = 1,
	o_bool,
	o_int,
	o_uint32,
	o_string,
	o_wild
};

typedef struct {
	char		*name;		/* name of the option */
	enum opt_type 	type;
	void		*addr;
	char		*description;
	unsigned int 	flags;
	void		*addr2;
	int		upper_limit;
	int		lower_limit;
	const char 	*source;
	short int 	priority;
	short int 	winner;
	void		*addr3;
} option_t;

/* load balancing */
//#define LB_MAX_PRIORITY 10
#define LB_DEFAULT_PORT 4112


struct vpn_params {
    int					debug;
	int					log_verbose;
    int					daemonize;
    SCDynamicStoreRef 	storeRef;
    CFStringRef			serverIDRef;
    CFPropertyListRef	serverRef;
    char				*server_id;
	u_int32_t			max_sessions;
	char				log_path[MAXPATHLEN];

	/* command line arguments used for the give type */
	u_int32_t			next_arg_index;		/* indicates end of argument array */
	char				*exec_args[MAXARG];

    int					server_type; /* PPP or IPSEC */
	
	/* parameter for type PPP */
	CFStringRef			serverSubTypeRef;
	u_int32_t			server_subtype;
	char				*plugin_path;
        
	/* parameter for type Load Balancing */
	int					lb_enable;
	//int					lb_priority;
	u_int16_t			lb_port;		// network order
	struct in_addr		lb_cluster_address;		// network order
	struct in_addr		lb_redirect_address;		// network order
	char				lb_interface[IFNAMSIZ+1];

	/* parameter for type IPSEC */


};


int process_options(struct vpn_params *options, int argc, char *argv[]);
CFArrayRef get_active_servers(struct vpn_params *params);
int check_conflicts(struct vpn_params *params);
int process_prefs(struct vpn_params *params);
int publish_state(struct vpn_params* params);
int kill_orphans(struct vpn_params* params);
void open_dynamic_store(struct vpn_params* params);
void close_dynamic_store(struct vpn_params* params);
int add_builtin_plugin(struct vpn_params* params, void *channel);
int plugin_exists(const char *inPath);
char* validate_ip_string(const char *inIPString, char *outIPString, size_t outSize);

void addparam(char **arg, u_int32_t *argi, char *param);
void addintparam(char **arg, u_int32_t *argi, char *param, u_int32_t val);
void addstrparam(char **arg, u_int32_t *argi, char *param, char *val);

#endif

