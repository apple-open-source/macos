/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
/*
 * inetroute.h
 * - get a list of internet network routes (includes default route)
 */

/*
 * Modification History
 *
 * Dieter Siegmund (dieter@apple.com) Tue Jul 14 11:33:50 PDT 1998
 * - created
 */

typedef struct {
    struct in_addr		dest;
    struct in_addr		mask;
    union {
	struct sockaddr_dl	link;
	struct sockaddr_in	inet;
    } gateway;
} inetroute_t;

typedef struct {
    int			count;
    inetroute_t * 	list;
    int			def_index;
} inetroute_list_t;

inetroute_list_t *	inetroute_list_init();
void			inetroute_list_free(inetroute_list_t * * list);
struct in_addr *	inetroute_default(inetroute_list_t * list_p);
void			inetroute_list_print(inetroute_list_t * list_p);
