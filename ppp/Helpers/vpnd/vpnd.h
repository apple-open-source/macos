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


#include <sys/syslog.h>


/*
 * Procedures exported from sys-*.c
 */
int  ppp_available __P((void));		/* Test whether ppp kernel support exists */
CFStringRef CopyDefaultIPAddress(); 	/* Copy the IPAddress of the default interface */
int get_route_interface(struct sockaddr *src, const struct sockaddr *dst, char *if_name); /* get the interface for a given address */
int find_address(const struct sockaddr_in *address, char *interface); /* check if an interface has a given address */


/*
 * Exit status values.
 */
#define EXIT_OK			0
#define EXIT_FATAL_ERROR	1
#define EXIT_OPTION_ERROR	2
#define EXIT_NOT_ROOT		3
#define EXIT_NO_KERNEL_SUPPORT	4
#define EXIT_USER_REQUEST	5


#define	PLUGINS_DIR 	"/System/Library/Extensions/"

void vpnlog(int nSyslogPriority, char *format_str, ...);
int update_prefs(void);
void toggle_debug(void);
void set_terminate(void);

int readn(int ref, void *data, int len);
int writen(int ref, void *data, int len);
