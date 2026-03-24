//
//Copyright (c) 2025 Apple Inc. All Rights Reserved.
//
// @APPLE_LICENSE_HEADER_START@
//
// This file contains Original Code and/or Modifications of Original Code
// as defined in and that are subject to the Apple Public Source License
// Version 2.0 (the 'License'). You may not use this file except in
// compliance with the License. Please obtain a copy of the License at
// http://www.opensource.apple.com/apsl/ and read it before using this
// file.
//
// The Original Code and all software distributed under the License are
// distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
// EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
// INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
// Please see the License for the specific language governing rights and
// limitations under the License.
//
// @APPLE_LICENSE_HEADER_END@
//

#pragma once

void SecDbStorageStatsReportMetrics(void); // Report keychain database storage info related metrics
#if __OBJC__

NSDictionary* _Nullable SecDbStatsGetAccessGroupItemCounts(void); // Info about keychain items per access group
NSDictionary* _Nullable SecDbStatsGetAccessGroupStorage(void); // Storage bytes per access group
NSDictionary* _Nullable SecDbStatsGetDatabaseFileSizeInfo(void); // Database file size information (main, WAL, SHM)
NSDictionary* _Nullable SecDbStatsGetMetrics(void); // Comprehensive metrics: access group stats + file sizes


#endif /* __OBCJ__ */
