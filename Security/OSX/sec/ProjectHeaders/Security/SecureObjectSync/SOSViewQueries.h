/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
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

/*!
 @header SOSViewQueries.h - view queries
 */

#ifndef _sec_SOSViewQueries_
#define _sec_SOSViewQueries_

#include <CoreFoundation/CFString.h>

__BEGIN_DECLS

// General View Queries
extern const CFStringRef kSOSViewQueryAppleTV;
extern const CFStringRef kSOSViewQueryHomeKit;

// Synced View Queries
extern const CFStringRef kSOSViewQueryKeychainV0;
extern const CFStringRef kSOSViewQueryKeychainV2;

// PCS View Queries
extern const CFStringRef kSOSViewQueryPCSMasterKey;
extern const CFStringRef kSOSViewQueryPCSiCloudDrive;
extern const CFStringRef kSOSViewQueryPCSPhotos;
extern const CFStringRef kSOSViewQueryPCSCloudKit;
extern const CFStringRef kSOSViewQueryPCSEscrow;
extern const CFStringRef kSOSViewQueryPCSFDE;
extern const CFStringRef kSOSViewQueryPCSMailDrop;
extern const CFStringRef kSOSViewQueryPCSiCloudBackup;
extern const CFStringRef kSOSViewQueryPCSNotes;
extern const CFStringRef kSOSViewQueryPCSiMessage;
extern const CFStringRef kSOSViewQueryPCSFeldspar;

// Backup Views
// - these are not sync views - supported by backup peers
extern const CFStringRef kSOSViewiCloudBackupV0;
extern const CFStringRef kSOSViewiTunesBackupV0;

// Backup View Queries
// - these are not sync view queries -
extern const CFStringRef kSOSViewQueryiCloudBackupV0;
extern const CFStringRef kSOSViewQueryiCloudBackupV2;
extern const CFStringRef kSOSViewQueryiTunesBackupV0;
extern const CFStringRef kSOSViewQueryiTunesBackupV2;

__END_DECLS

#endif /* defined(_sec_SOSViewQueries_) */
