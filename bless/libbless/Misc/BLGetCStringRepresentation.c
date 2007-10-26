/*
 * Copyright (c) 2006-2007 Apple Inc. All Rights Reserved.
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
 *  BLGetCStringRepresentation.c
 *  bless
 *
 *  Created by Shantonu Sen on 5/30/06.
 *  Copyright 2006-2007 Apple Inc. All Rights Reserved.
 *
 */

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#include <CoreFoundation/CoreFoundation.h>

#include "bless.h"
#include "bless_private.h"

/*
 * For the given CFTypeRef, get a C-string representation, backed
 * by thread-local storage
 */

#if !defined(NO_GETCSTRING) || !NO_GETCSTRING

static pthread_once_t	blcstr_once_control = PTHREAD_ONCE_INIT;
static pthread_key_t	blcstr_key = 0;

static void initkey(void);
static void releasestorage(void *addr);

struct stringer {
	size_t		size;
	char	*	string;
};

char *BLGetCStringDescription(CFTypeRef typeRef) {

	CFStringRef desc = NULL;
	int ret;
	struct stringer	*storage;
	CFIndex	strsize;
	
	if(typeRef == NULL)
		return NULL;
	
	ret = pthread_once(&blcstr_once_control, initkey);
	if(ret)
		return NULL;

	if(CFGetTypeID(typeRef) == CFStringGetTypeID()) {
		desc = CFRetain(typeRef);
	} else {
		desc = CFCopyDescription(typeRef);		
	}
	if(desc == NULL)
		return NULL;
	
	strsize = CFStringGetLength(desc);
	
	// assume encoding size of 3x as UTF-8
	strsize = 3*strsize + 1;
	
	storage = (struct stringer	*)pthread_getspecific(blcstr_key);
	if(storage == NULL) {
		storage = malloc(sizeof(*storage));
		storage->size = (size_t)strsize;
		storage->string = malloc(storage->size);

		ret = pthread_setspecific(blcstr_key, storage);
		if(ret) {
			CFRelease(desc);
			free(storage->string);
			free(storage);
			fprintf(stderr, "pthread_setspecific failed\n");
			return NULL;
		}
		
	} else if(storage->size < strsize) {
		// need more space
		storage->size = (size_t)strsize;
		free(storage->string);
		storage->string = malloc(storage->size);
	}
	
	if(!CFStringGetCString(desc, storage->string, (CFIndex)storage->size, kCFStringEncodingUTF8)) {
		CFRelease(desc);
		fprintf(stderr, "CFStringGetCString failed\n");		
		return NULL;
	}
	
	CFRelease(desc);
	
	return storage->string;
}

static void initkey(void)
{
	int ret;
	
	ret = pthread_key_create(&blcstr_key, releasestorage);
	if(ret)
		fprintf(stderr, "pthread_key_create failed\n");
//	printf("pthread_key_create: %lu\n", blcstr_key);
}

static void releasestorage(void *addr)
{
	// should be non-NULL
	struct stringer	*storage = (struct stringer	*)addr;

	free(storage->string);
	free(storage);
}

#else

char *BLGetCStringDescription(CFTypeRef typeRef) {
	return NULL;
}
#endif
