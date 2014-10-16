/*
 * Copyright (c) 2004-2005 Apple Computer, Inc. All Rights Reserved.
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
 */

/*
 * aclUtils.h - ACL utility functions, copied from the SecurityTool project. 
 */
 
#ifndef	_ACL_UTILS_H_
#define _ACL_UTILS_H_

#include <stdio.h>
#include <Security/Security.h>

#ifdef __cplusplus
extern "C" {
#endif

char *readline(char *buffer, int buffer_size);
void print_buffer_hex(FILE *stream, UInt32 length, const void *data);
void print_buffer_ascii(FILE *stream, UInt32 length, const void *data);
void print_buffer(FILE *stream, UInt32 length, const void *data);
void print_cfdata(FILE *stream, CFDataRef data);
void print_cfstring(FILE *stream, CFStringRef string);
int print_access(FILE *stream, SecAccessRef access, Boolean interactive);

OSStatus stickyRecordUpdateAcl(
	SecAccessRef accessRef);

#ifdef __cplusplus
}
#endif

#endif	/* _ACL_UTILS_H_ */

