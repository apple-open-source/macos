/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 *  keychain_utilities.h
 *  security
 *
 *  Created by Michael Brouwer on Tue May 07 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 */

#ifndef _KEYCHAIN_UTILITIES_H_
#define _KEYCHAIN_UTILITIES_H_ 1

#include <Security/SecKeychain.h>

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Open a given named name. */
extern SecKeychainRef keychain_open(const char *name);

/* Return either NULL if argc is 0, or a SecKeychainRef if argc is 1 or a CFArrayRef of SecKeychainRefs if argc is greater than 1. */
extern CFTypeRef keychain_create_array(int argc, char * const *argv);

extern int print_keychain_name(FILE *stream, SecKeychainRef keychain);

extern int print_keychain_item_attributes(FILE *stream, SecKeychainItemRef item, Boolean show_data, Boolean show_raw_data);

extern void print_buffer(FILE *stream, UInt32 length, void *data);

extern void print_buffer_pem(FILE *stream, const char *headerString, UInt32 length, void *data);

#ifdef __cplusplus
}
#endif

#endif /*  _KEYCHAIN_UTILITIES_H_ */
