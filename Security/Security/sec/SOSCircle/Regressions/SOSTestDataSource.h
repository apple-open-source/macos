/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
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


#ifndef _SEC_SOSTestDataSource_H_
#define _SEC_SOSTestDataSource_H_

#include <SecureObjectSync/SOSDataSource.h>

extern CFStringRef sSOSDataSourceErrorDomain;

enum {
    kSOSDataSourceObjectMallocFailed = 1,
    kSOSDataSourceAddDuplicateEntry,
    kSOSDataSourceObjectNotFoundError,
    kSOSDataSourceAccountCreationFailed,
};

//
// MARK: Data Source Functions
//
SOSDataSourceRef SOSTestDataSourceCreate(void);

CFMutableDictionaryRef SOSTestDataSourceGetDatabase(SOSDataSourceRef data_source);

SOSMergeResult SOSTestDataSourceAddObject(SOSDataSourceRef data_source, SOSObjectRef object, CFErrorRef *error);
bool SOSTestDataSourceDeleteObject(SOSDataSourceRef data_source, CFDataRef key, CFErrorRef *error);

//
// MARK: Data Source Factory Functions
//

SOSDataSourceFactoryRef SOSTestDataSourceFactoryCreate(void);
void SOSTestDataSourceFactoryAddDataSource(SOSDataSourceFactoryRef factory, CFStringRef name, SOSDataSourceRef ds);

SOSObjectRef SOSDataSourceCreateGenericItemWithData(SOSDataSourceRef ds, CFStringRef account, CFStringRef service, bool is_tomb, CFDataRef data);
SOSObjectRef SOSDataSourceCreateGenericItem(SOSDataSourceRef ds, CFStringRef account, CFStringRef service);

SOSObjectRef SOSDataSourceCopyObject(SOSDataSourceRef ds, SOSObjectRef match, CFErrorRef *error);

#endif /* _SEC_SOSTestDataSource_H_ */
