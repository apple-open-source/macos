/*
 * Copyright (c) 2008 Apple Computer, Inc. All rights reserved.
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

#ifndef _IPSECCONFIGTRACER_H
#define _IPSECCONFIGTRACER_H

typedef enum ipsecConfigEventCode {
	IPSECCONFIGEVENTCODE_NONE = 0,
    IPSECCONFIGEVENTCODE_REPARSE_ERROR,
	IPSECCONFIGEVENTCODE_PARSE_ERROR,
	IPSECCONFIGEVENTCODE_SIGNAL_ERROR,
	IPSECCONFIGEVENTCODE_MAX,
} ipsecConfigEventCode_t;

const char * ipsecConfigEventCodeToString (ipsecConfigEventCode_t);
void ipsecConfigTracerEvent (const char *, ipsecConfigEventCode_t, const char *, const char *);

#endif /* _IPSECCONFIGTRACER_H */
