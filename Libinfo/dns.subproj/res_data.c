/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
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
/*
 * This file contains global data but it is a `private_extern' in the
 * shared library so that its address and size can change.
 */

#if defined(__APPLE__)

/*
 * _res is declared to be the same size as struct __res_9_state
 * This allows both the BIND-8 library in libSystem (this one)
 * and the new BIND-9 library in libresolv to share the same
 * structure.  We ues the __res_9_state's _pad variable to store
 * a version number when _res have been initialized by the BIND-9
 * library, and take precautions to make them work together.
 */
#ifdef __LP64__
#define RES_9_STATE_SIZE 552
#else
#define RES_9_STATE_SIZE 512
#endif

char _res[RES_9_STATE_SIZE] = {0};
int _net_stayopen = 0;
#endif
