/*
 *  lookupDSLocalKDC.c
 *  KerberosHelper
 */
/*
 * Copyright (c) 2006-2007 Apple Inc. All rights reserved.
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


#include "lookupDSLocalKDC.h"
#include <CoreFoundation/CoreFoundation.h>

#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_5

#include <Carbon/Carbon.h>
#include <OpenDirectory/OpenDirectory.h>
#include <DirectoryService/DirectoryService.h>

OSStatus DSCopyLocalKDC (CFStringRef *realm) 
{
	OSStatus err = 0;
	ODNodeRef cfNodeRef = NULL;
	ODRecordRef cfRecord = NULL;
	CFArrayRef cfLKDCName = NULL;
	CFIndex limit;
	CFTypeRef realmName;

	if (NULL == realm) { err = paramErr; goto Error; }

	*realm = NULL;
        
    cfNodeRef = ODNodeCreateWithNodeType( kCFAllocatorDefault, kODSessionDefault, kODNodeTypeAuthentication, NULL );

    if ( NULL == cfNodeRef ) { err = paramErr; goto Error; }
	
    cfRecord = ODNodeCopyRecord( cfNodeRef, CFSTR(kDSStdRecordTypeConfig), CFSTR(kKDCRecordName), NULL, NULL );

    if( NULL == cfRecord ) { err = paramErr; goto Error; }
	
	cfLKDCName = ODRecordCopyValues ( cfRecord, CFSTR(kRealmNameKey), NULL);

    if ( NULL == cfLKDCName ) { err = paramErr; goto Error; }
	
	limit = CFArrayGetCount (cfLKDCName);

	if (1 != limit) { err = paramErr; goto Error; }
	
	realmName = CFArrayGetValueAtIndex (cfLKDCName, 0);

	if (CFStringGetTypeID () != CFGetTypeID (realmName)) { err = paramErr; goto Error; }
	
	*realm = CFRetain (realmName);

Error:
	if (cfLKDCName) { CFRelease( cfLKDCName ); }
	if (cfRecord)   { CFRelease( cfRecord ); }
	if (cfNodeRef)  { CFRelease( cfNodeRef ); }

	return err;
}

#else

#warning On Mac OS X 10.4

#define kLocalKDCRealmFile	"/var/db/realm.local"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

OSStatus DSCopyLocalKDC (CFStringRef *realm) 
{
	char			*endRealm = NULL, *realmString = NULL;
	struct stat		realmFileSB;
	int				fd;
	size_t			wasRead, realmSize;
	
	if (NULL == realm) { err = paramErr; goto Error; }

	*realm = NULL;

	if (0 != stat (kLocalKDCRealmFile, &realmFileSB)) { err = paramErr; goto Error; }

	/* The LKDC: line should only be 51 characters in size */
	if (realmFileSB.st_size > 64) { err = paramErr; goto Error; }

	realmSize = realmFileSB.st_size + 1;

	realmString = malloc (realmSize);

	fd = open (kLocalKDCRealmFile, O_RDONLY);
	
	if (0 == fd || NULL == realmString) { err = paramErr; goto Error; }
		
	wasRead = read (fd, realmString, realmFileSB.st_size);

	close (fd);

	if (wasRead != realmFileSB.st_size) { err = paramErr; goto Error; }

	/* Make sure the buffer is null terminated */
	realmString [realmSize] = '\0';

	endRealm = strchr (realmString, '\n');

	/* Trim the trailing newline */
	if (NULL != endRealm) { *endRealm = '\0'; }
				
	*realm = CFStringCreateWithCString (NULL, realmString, kCFStringEncodingASCII);

Error:
	if (realmString) { free (realmString); }

	return err;
}

#endif
