/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 2002 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _LU_HOST_H_
#define _LU_HOST_H_

#include <sys/cdefs.h>


#define WANT_A4_ONLY 0
#define WANT_A6_ONLY 1
#define WANT_A6_PLUS_MAPPED_A4 2
#define WANT_MAPPED_A4_ONLY 3

/* ONLY TO BE USED BY getipv6nodebyaddr */
#define WANT_A6_OR_MAPPED_A4_IF_NO_A6 -1


__BEGIN_DECLS

void free_host_data(struct hostent *h);
struct hostent * extract_host(XDR *xdr, int want, int *err);
struct hostent * fake_hostent(const char *name, struct in_addr addr);
struct hostent * fake_hostent6(const char *name, struct in6_addr addr);
int is_a4_mapped(const char *s);
int is_a4_compat(const char *s);

__END_DECLS

#endif /* ! _LU_HOST_H_ */
