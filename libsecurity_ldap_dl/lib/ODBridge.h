/*
 * Copyright (c) 2009 Apple Inc. All Rights Reserved.
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

#ifndef _OD_BRIDGE_H_
#define _OD_BRIDGE_H_  1

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <OpenDirectory/OpenDirectory.h>
#include <Security/cssmapi.h>
#include <Security/cssmapple.h>
#include <Security/cssmerr.h>

// Query results are stored in this.
typedef struct ODdl_results {
	CSSM_DB_RECORDTYPE	recordid;
	ODQueryRef			query;
	CFStringRef			searchString;
	CFIndex				currentRecord;
	CFMutableArrayRef	certificates;
	dispatch_semaphore_t results_done;
} *ODdl_results_handle;


// Oh how the mighty have fallen - had to get out of Dodge with one of these ...  once.
class DirectoryServiceException
{
protected:
	long mResult;
	
public:
	DirectoryServiceException (CFErrorRef result) : mResult (CFErrorGetCode(result)) {}
	
	long GetResult () {return mResult;}
};


class DirectoryService
{
protected:
	char						*db_name;
	ODNodeRef					node;
	dispatch_queue_t			query_dispatch_queue;	// Queue to use for queries
	CFMutableArrayRef			all_open_queries;
	
public:
	DirectoryService ();
	~DirectoryService ();
	long long int getNextRecordID();
	ODdl_results_handle makeNewDSQuery();
	ODdl_results_handle translate_cssm_query_to_OD_query(const CSSM_QUERY *Query, CSSM_RETURN *error);
	CFDataRef getNextCertFromResults(ODdl_results_handle results);
};


#endif /* !_OD_BRIDGE_H_ */
