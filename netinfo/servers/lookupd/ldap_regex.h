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

/*
 * Copyright (c) 1993 Regents of the University of Michigan.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of Michigan at Ann Arbor. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission. This software
 * is provided ``as is'' without express or implied warranty.
 */
/*
 * regex.h -- includes for regular expression matching routines
 * 13 August 1993 Mark C Smith
 */

#ifdef __cplusplus
extern "C" {
#endif

#if !defined( NEEDPROTOS ) && defined( __STDC__ )
#define NEEDPROTOS
#endif

#ifdef NEEDPROTOS
#if !defined( __NeXT__ ) && !defined(__APPLE__)
/* This is in unistd.h. */
char *re_comp( char *pat );
int re_exec( char *lp );
void re_modw( char *s );
int re_subs( char *src, char *dst );
#endif
#else /* NEEDPROTOS */
char *re_comp();
int re_exec();
void re_modw();
int re_subs();
#endif /* NEEDPROTOS */

#define re_fail( m, p )

#ifdef __cplusplus
}
#endif
