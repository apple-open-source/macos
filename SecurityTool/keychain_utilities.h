/*
 * Copyright (c) 2003-2009 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 *
 * keychain_utilities.h
 */

#ifndef _KEYCHAIN_UTILITIES_H_
#define _KEYCHAIN_UTILITIES_H_ 1

#include <Security/SecKeychain.h>

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Open a given named keychain. */
extern SecKeychainRef keychain_open(const char *name);

/* Return either NULL if argc is 0, or a SecKeychainRef if argc is 1 or a CFArrayRef of SecKeychainRefs if argc is greater than 1. */
extern CFTypeRef keychain_create_array(int argc, char * const *argv);

extern int parse_fourcharcode(const char *name, UInt32 *code);

extern int print_keychain_name(FILE *stream, SecKeychainRef keychain);

extern int print_keychain_item_attributes(FILE *stream, SecKeychainItemRef item, Boolean show_data, Boolean show_raw_data, Boolean show_acl, Boolean interactive);

extern void print_buffer(FILE *stream, UInt32 length, const void *data);

extern void print_buffer_pem(FILE *stream, const char *headerString, UInt32 length, const void *data);

extern void print_uint32(FILE* stream, uint32 n);
	
extern unsigned char hexValue(char c);

extern void fromHex(const char *hexDigits, CSSM_DATA *data);
	
extern void safe_CFRelease(void *cfTypeRefPtr);

extern void check_obsolete_keychain(const char *kcName);

#ifdef __cplusplus
}
#endif

#endif /*  _KEYCHAIN_UTILITIES_H_ */
