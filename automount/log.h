/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#include <syslog.h>
#include "nfs_prot.h"

#define DEBUG_NONE 0
#define DEBUG_STDERR 1
#define DEBUG_SYSLOG 2
#define DEBUG_ALL (DEBUG_STDERR & DEBUG_SYSLOG)

/*
 * Minimum size of the buffer that "formattimevalue()" fills in.
 * The string is:
 *
 *	         11111111112222222
 *	12345678901234567890123456
 *	DOW MMM DD HH:MM:SS.FFFFFF
 *
 * for a total of 26 characters, plus one more for a terminating '\0'.
 */
#define FORMATTED_TIME_LEN	(26+1)

char *formattimevalue(struct nfstime *t, char *str, size_t stringlength);
void sys_openlog(char *, int, int);
void sys_msg(int, int, char *, ...);
