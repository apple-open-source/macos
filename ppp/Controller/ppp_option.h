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
#define OPT_LCP_MRU_L2TP_DEF 		1500	/* use standart PPP MTU */
#define OPT_LCP_MTU_DEF 		1500
#define OPT_LCP_MTU_PPPoE_DEF 		1492	/* use standart PPPoE MTU */
#define OPT_LCP_MTU_PPTP_DEF 		1448	/* avoid fragmentation */
                                                /* 1500-IPHdr(20)-GRE(16)-PPP/MPPE(8)-PPPoE(8) */
#define OPT_LCP_MTU_L2TP_DEF 		1448	/* avoid fragmentation */
                                                /* 1500-IPHdr(20)-UDP(20)-PPP(4)-PPPoE(8) */
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
#define EXIT_OK                  	0
#define EXIT_FATAL_ERROR 		1
#define EXIT_OPTION_ERROR        	2
#define EXIT_NOT_ROOT            	3
#define EXIT_NO_KERNEL_SUPPORT   	4
#define EXIT_USER_REQUEST        	5
#define EXIT_LOCK_FAILED 		6
#define EXIT_OPEN_FAILED 		7
#define EXIT_CONNECT_FAILED      	8
#define EXIT_PTYCMD_FAILED       	9
#define EXIT_NEGOTIATION_FAILED  	10
#define EXIT_PEER_AUTH_FAILED    	11
#define EXIT_IDLE_TIMEOUT        	12
#define EXIT_CONNECT_TIME        	13
#define EXIT_CALLBACK            	14
#define EXIT_PEER_DEAD           	15
#define EXIT_HANGUP              	16
#define EXIT_LOOPBACK            	17
#define EXIT_INIT_FAILED 		18
#define EXIT_AUTH_TOPEER_FAILED  	19
#define EXIT_TERMINAL_FAILED  		20
#define EXIT_DEVICE_ERROR  		21

// ppp serial error codes (bits 8..15 of last cause key)
#define EXIT_PPPSERIAL_NOCARRIER  	1
#define EXIT_PPPSERIAL_NONUMBER  	2
#define EXIT_PPPSERIAL_BUSY	  	3
#define EXIT_PPPSERIAL_NODIALTONE  	4
#define EXIT_PPPSERIAL_ERROR	  	5
#define EXIT_PPPSERIAL_NOANSWER	  	6
#define EXIT_PPPSERIAL_HANGUP	  	7



u_long set_long_opt (struct opt_long *option, u_long opt, u_long mini, u_long maxi, u_long limit);
u_long set_str_opt (struct opt_str *option, char *opt, int maxlen);

u_long ppp_setoption (struct client *client, struct msg *req);
u_long ppp_getoption (struct client *client, struct msg *req);

int options_init_all();

int getStringFromEntity(CFStringRef domain, CFStringRef serviceID, 
        CFStringRef entity, CFStringRef property, u_char *str, u_int16_t maxlen);
int getNumberFromEntity(CFStringRef domain, CFStringRef serviceID, 
        CFStringRef entity, CFStringRef property, u_int32_t *outval);
int getAddressFromEntity(CFStringRef domain, CFStringRef serviceID, 
        CFStringRef entity, CFStringRef property, u_int32_t *outval);
int getNumber(CFDictionaryRef service, CFStringRef property, u_int32_t *outval);
int getString(CFDictionaryRef service, CFStringRef property, u_char *str, u_int16_t maxlen);
CFTypeRef copyEntity(CFStringRef domain, CFStringRef serviceID, CFStringRef entity);
int getServiceName(CFStringRef serviceID, u_char *str, u_int16_t maxlen);

extern CFStringRef	gLoggedInUser;

#endif
