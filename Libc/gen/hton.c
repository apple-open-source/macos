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
/* Copyright (c) 1994 NeXT Computer, Inc.  All rights reserved.
 *
 *      File:   hton.c
 *      Author: Matt Watson, NeXT Computer, Inc.
 *
 *      Use the machine independent byte-swapping code for htonl-type functions.
 *
 * HISTORY
 * 15-Dec-94  Matt Watson (Matt_Watson@NeXT.COM)
 *      Created.
 */

#import <architecture/byte_order.h>

long ntohl(long x) {
    return NXSwapBigLongToHost(x);
}

short ntohs(short x) {
    return NXSwapBigShortToHost(x);
}

long htonl(long x) {
    return NXSwapHostLongToBig(x);
}

short htons(short x) {
    return NXSwapHostShortToBig(x);
}
