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
/* Copyright (c) 1992, 1997  NeXT Software, Inc.  All rights reserved.
 *
 *	File:	libc/gen/ppc/remque.c
 *
 * struct qelem {
 *	struct qelem	*q_forw;
 *	struct qelem	*q_back;
 *	char		q_data[];
 * };
 *
 * void remque(struct qelem *entry);
 *
 * Removes `entry' from a queue.
 *
 * HISTORY
 *  24-Jan-1997 Umesh Vaishampayan (umeshv@NeXT.com)
 *	Ported to PPC.
 *  10-Nov-92  Derek B Clegg (dclegg@next.com)
 *	Created.
 */
#import <libc.h>

void
remque(struct qelem *elem)
{
    struct qelem *next, *prev;

    next = elem->q_forw;
    prev = elem->q_back;
    if (next != 0)
	next->q_back = prev;
    if (prev != 0)
	prev->q_forw = next;
}
