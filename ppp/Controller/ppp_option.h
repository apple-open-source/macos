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


#ifndef __PPP_OPTIONS__
#define __PPP_OPTIONS__


/* PATHS definitions for PPP */

#define DIR_KEXT		"/System/Library/Extensions/"
#define DIR_LOGS		"/var/log/ppp/"
#define DIR_TTYS		"/dev/"
#define PATH_PPPD 		"/usr/sbin/pppd"
#define PPPD_PRGM 		"pppd"

/* Device default values */

#define OPT_DEV_NAME_DEF 		"modem"
#define OPT_DEV_NAME_PPPoE_DEF 		"en0"
#define OPT_DEV_SPEED_DEF 		0	// use the default tty speed, CCL will set the speed
#define OPT_DEV_CONNECTSCRIPT_DEF	"Apple Internal 56K Modem (v.34)"
#define OPT_DEV_SPEAKER_DEF		1
#define OPT_DEV_DIALMODE_DEF		0 // Normal mode
#define OPT_DEV_PULSE_DEF		0

/* Comm default values */

#define OPT_COMM_IDLETIMER_DEF 		0	// no idle timer
#define OPT_COMM_SESSIONTIMER_DEF 	0	// no session timer
#define OPT_COMM_CONNECTDELAY_DEF 	0 	// delay to wait after link is connected (in seconds)
#define OPT_COMM_REMINDERTIMER_DEF 	0	// no reminder timer

#define OPT_COMM_TERMINALMODE_DEF	PPP_COMM_TERM_NONE

/* LCP default values */	

#define OPT_LCP_ACCOMP_DEF 		1	// address and control fields compression activated
#define OPT_LCP_PCOMP_DEF 		1	// protocol field compression activated
#define OPT_LCP_RCACCM_DEF 		PPP_LCP_ACCM_NONE	// default asyncmap value
#define OPT_LCP_TXACCM_DEF 		PPP_LCP_ACCM_NONE	// default asyncmap value
#define OPT_LCP_MRU_DEF 		1500
#define OPT_LCP_MRU_PPPoE_DEF 		1492	/* use standart PPPoE MTU */
#define OPT_LCP_MRU_PPTP_DEF 		1500	/* use standart PPP MTU */
#define OPT_LCP_MRU_L2TP_DEF 		1500    /* use standart PPP MTU */
#define OPT_LCP_MTU_DEF 		1500
#define OPT_LCP_MTU_PPPoE_DEF 		1492	/* use standart PPPoE MTU */
#define OPT_LCP_MTU_PPTP_DEF 		1448	/* avoid fragmentation */
                                                /* 1500-IPHdr(20)-GRE(16)-PPP/MPPE(8)-PPPoE(8) */
#define OPT_LCP_MTU_L2TP_DEF 		1280	/* avoid fragmentation */
#define OPT_LCP_ECHOINTERVAL_DEF	10
#define OPT_LCP_ECHOFAILURE_DEF		4

/* IPCP default values */

#define OPT_IPCP_HDRCOMP_DEF 		PPP_IPCP_HDRCOMP_VJ	// tcp vj compression activated
#define OPT_IPCP_USESERVERDNS_DEF 	1	// acquire DNS from server
#define OPT_HOSTNAME_DEF 		"localhost"	
#define OPT_ALERT_DEF 			PPP_ALERT_ENABLEALL	

/* AUTH default values */

#define OPT_AUTH_PROTO_DEF 		PPP_AUTH_PAPCHAP	// do any auth proposed by server

/* Misc default values */

#define OPT_VERBOSELOG_DEF	0	// quiet log by default
#define OPT_LOGFILE_DEF		""	// no logs by default (suggested name "ppp.log")
#define OPT_AUTOCONNECT_DEF 	0	// dial on demand not activated
#define OPT_DISCLOGOUT_DEF 	1	// disconnect on logout by default

// pppd error codes (bits 0..7 of lastcause key)
// error codes are in pppd/pppd.h

// ppp serial error codes (bits 8..15 of last cause key)
#define EXIT_PPPSERIAL_NOCARRIER  	1
#define EXIT_PPPSERIAL_NONUMBER  	2
#define EXIT_PPPSERIAL_BUSY	  	3
#define EXIT_PPPSERIAL_NODIALTONE  	4
#define EXIT_PPPSERIAL_ERROR	  	5
#define EXIT_PPPSERIAL_NOANSWER	  	6
#define EXIT_PPPSERIAL_HANGUP	  	7
#define EXIT_PPPSERIAL_MODEMSCRIPTNOTFOUND  	8
#define EXIT_PPPSERIAL_BADSCRIPT  	9


#define OPT_STR_LEN 256


int ppp_getoptval(struct service *serv, CFDictionaryRef opts, CFDictionaryRef setup, 
        u_int32_t otype, void *pdata, u_int32_t pdatasiz, u_int32_t *plen);
u_long get_addr_option (struct service *serv, CFStringRef entity, CFStringRef property, 
        CFDictionaryRef optsdict, CFDictionaryRef setupdict, u_int32_t *opt, u_int32_t defaultval);
u_long get_int_option (struct service *serv, CFStringRef entity, CFStringRef property,
        CFDictionaryRef optsdict, CFDictionaryRef setupdict, u_int32_t *opt, u_int32_t defaultval);
int get_str_option (struct service *serv, CFStringRef entity, CFStringRef property,
        CFDictionaryRef optsdict, CFDictionaryRef setupdict, u_char *opt, u_int32_t optsiz, u_int32_t *outlen, u_char *defaultval);
CFTypeRef get_cf_option (CFStringRef entity, CFStringRef property, CFTypeID type, 
        CFDictionaryRef options, CFDictionaryRef setup, CFTypeRef defaultval);


#endif
