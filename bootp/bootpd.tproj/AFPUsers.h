/*
 * Copyright (c) 2003-2013 Apple Inc. All rights reserved.
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

#ifndef _S_AFPUSERS_H
#define _S_AFPUSERS_H

#include <unistd.h>
#include <stdint.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFDictionary.h>
#include <OpenDirectory/OpenDirectory.h>
#include <DirectoryService/DirectoryService.h>

#define CHARSET_SYMBOLS			"-,./[]\\;'!@#%&*()_{}:\"?"
#define CHARSET_SYMBOLS_LENGTH		(sizeof(CHARSET_SYMBOLS) - 1)

typedef CFMutableDictionaryRef	AFPUserRef;

typedef struct {
    ODNodeRef		node;
    CFMutableArrayRef	list;
} AFPUserList, *AFPUserListRef;

void		AFPUserList_free(AFPUserListRef users);
Boolean		AFPUserList_init(AFPUserListRef users);
Boolean		AFPUserList_create(AFPUserListRef users, gid_t gid,
				   uid_t start, int count);
AFPUserRef	AFPUserList_lookup(AFPUserListRef users, CFStringRef afp_user);

uid_t		AFPUser_get_uid(AFPUserRef user);
char *		AFPUser_get_user(AFPUserRef user, char *buf, size_t buf_len);
Boolean		AFPUser_set_random_password(AFPUserRef user, 
					    char * passwd, size_t passwd_len);

#endif	// _S_AFPUSERS_H
