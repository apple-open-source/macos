/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*!
 * @header SMBAuth
 */

#ifndef __SMBAUTH_H__
#define	__SMBAUTH_H__		1

// utility functions prototypes
#ifdef __cplusplus
extern "C" {
#endif

	void CalculateSMBNTHash(const char *utf8Password, unsigned char outHash[16]);
	void CalculateSMBLANManagerHash(const char *password, unsigned char outHash[16]);
	void CalculateP24(unsigned char *P21, unsigned char *C8, unsigned char *P24);
	void DESEncode(void *str, void *data);

#ifdef __cplusplus
}
#endif

#endif