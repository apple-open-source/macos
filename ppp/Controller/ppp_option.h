/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */


#ifndef __PPP_OPTIONS__
#define __PPP_OPTIONS__



/* PATHS definitions for PPP */

#define DIR_PPP			"/System/Library/PPP/"
#define DIR_EXT			"/System/Library/Extensions/"

#define DIR_MODEMS		"/Library/Modem Scripts/"
//#define DIR_CHATS		DIR_PPP"Chats/"
#define DIR_CHATS		"/etc/ppp/chats/"
//#define DIR_HELPERS		DIR_PPP"Helpers/"
#define DIR_HELPERS		"/usr/libexec/"
#define DIR_LOGS		"/var/log/"
#define DIR_TTYS		"/dev/"

#define TTY_MODEM		"modem"
#define TTY_PRINTER		"printer"
//#define TTYPPPOE_NAME		"pppoe0"
//#define DEVPPPOE		"pppoe"
#define CCL_ENGINE		"CCLEngine"
#define CHAT_ENGINE		"CCLEngine"
#define CHAT_WINDOW		"MiniTerm.app"

#define KEXT_PPP		DIR_EXT"PPP.kext"
#define KEXT_PPPSERIAL		DIR_EXT"PPPSerial.kext"
#define KEXT_PPPoE		DIR_EXT"PPPoE.kext"

#define LOG_SEPARATOR		"----------------------------------------------------------------------"

/* Device default values */

#define OPT_DEV_NAME_DEF 		TTY_MODEM
#define OPT_DEV_SPEED_DEF 		0	// use the default tty speed, CCL will set the speed
#define OPT_DEV_CONNECTSCRIPT_DEF	"Apple Internal 56K Modem (v.34)"
#define OPT_DEV_SPEAKER_DEF		1
#define OPT_DEV_DIALMODE_DEF		0 // Normal mode
#define OPT_DEV_PULSE_DEF		0
//#define OPT_DEV_CONNECTPRGM_DEF	"ChatEngine"
//#define OPT_DEV_CONNECTPRGM_DEF		CCLENGINE

/* Comm default values */

#define OPT_COMM_IDLETIMER_DEF 		0	// no idle timer
#define OPT_COMM_SESSIONTIMER_DEF 	0	// no session timer
#define OPT_COMM_CONNECTDELAY_DEF 	0 	// delay to wait after link is connected (in seconds)
//
#define OPT_COMM_TERMINALMODE_DEF	PPP_COMM_TERM_NONE

/* LCP default values */	

#define OPT_LCP_ACCOMP_DEF 		1	// address and control fields compression activated
#define OPT_LCP_PCOMP_DEF 		1	// protocol field compression activated
#define OPT_LCP_RCACCM_DEF 		PPP_LCP_ACCM_NONE	// default asyncmap value
#define OPT_LCP_TXACCM_DEF 		PPP_LCP_ACCM_NONE	// default asyncmap value
#define OPT_LCP_MRU_DEF 		1500
#define OPT_LCP_MTU_DEF 		1500
#define OPT_LCP_ECHOINTERVAL_DEF	10
#define OPT_LCP_ECHOFAILURE_DEF		4

/* IPCP default values */

#define OPT_IPCP_HDRCOMP_DEF 		PPP_IPCP_HDRCOMP_VJ	// tcp vj compression activated
#define OPT_IPCP_USESERVERDNS_DEF 	1	// acquire DNS from server
#define OPT_HOSTNAME_DEF 		"localhost"	

/* AUTH default values */

#define OPT_AUTH_PROTO_DEF 		PPP_AUTH_NONE	// no authentication

/* Misc default values */

#define OPT_VERBOSELOG_DEF	0	// quiet log by default
#define OPT_LOGFILE_DEF		""	// no logs by default (suggested name "ppp.log")
#define OPT_AUTOCONNECT_DEF 	0	// dial on demand not activated
#define OPT_DISCLOGOUT_DEF 	1	// disconnect on logout by default
#define OPT_CONNLOGOUT_DEF 	0	// don't allow connection when logged out by default
                                        // only apply to dial on demand connections



u_long set_long_opt (struct opt_long *option, u_long opt, u_long mini, u_long maxi, u_long limit);
u_long set_str_opt (struct opt_str *option, char *opt, int maxlen);

u_long ppp_setoption (u_short id, struct msg *req);
u_long ppp_getoption (u_short id, struct msg *req);

void options_init_all(SCDSessionRef session);
void options_dispose_all(SCDSessionRef session);
//void read_options(u_char *ifname, u_short ifunit, struct options *opts);

u_char isUserLoggedIn();

#endif
