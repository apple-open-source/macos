/*
 * Copyright (c) 2011 Apple Inc. All rights reserved.
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

#include <TargetConditionals.h>

#define INFO_FILE 1
#if !TARGET_OS_EMBEDDED
#define INFO_NIS 2
#define INFO_OPEN_DIRECTORY 3
#define INFO_PAM 4
#endif

extern int file_passwd(char *, char *);
extern int nis_passwd(char *, char *);
#ifdef INFO_OPEN_DIRECTORY
extern int od_passwd(char *, char *, char*);
#endif
#ifdef INFO_PAM
extern int pam_passwd(char *);
#endif

void
getpasswd(char *name, int isroot, int minlen, int mixcase, int nonalpha,
          char *old_pw, char **new_pw, char **old_clear, char **new_clear);
