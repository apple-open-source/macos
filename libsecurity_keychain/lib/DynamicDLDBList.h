/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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
 *  DynamicDLDBList.h
 */
#ifndef _SECURITY_DYNAMICDLDBLIST_H_
#define _SECURITY_DYNAMICDLDBLIST_H_

#include <security_cdsa_client/DLDBList.h>
#include <security_cdsa_client/cssmclient.h>

namespace Security
{

namespace KeychainCore
{

class DynamicDLDBList
{
public:
    DynamicDLDBList();
    ~DynamicDLDBList();

	const vector<DLDbIdentifier> &searchList();

protected:
	Mutex mMutex;
	bool _add(const Guid &guid, uint32 subserviceID, CSSM_SERVICE_TYPE subserviceType);
	bool _add(const DLDbIdentifier &);
	bool _remove(const Guid &guid, uint32 subserviceID, CSSM_SERVICE_TYPE subserviceType);
	bool _remove(const DLDbIdentifier &);
	bool _load();
	DLDbIdentifier dlDbIdentifier(const Guid &guid, uint32 subserviceID,
		CSSM_SERVICE_TYPE subserviceType);
	void callback(const Guid &guid, uint32 subserviceID,
		CSSM_SERVICE_TYPE subserviceType, CSSM_MODULE_EVENT eventType);

private:
	static CSSM_RETURN appNotifyCallback(const CSSM_GUID *guid, void *context,
		uint32 subserviceId, CSSM_SERVICE_TYPE subserviceType, CSSM_MODULE_EVENT eventType);

	vector<CssmClient::Module> mModules;
	typedef vector<DLDbIdentifier> SearchList;
	SearchList mSearchList;
    bool mSearchListSet;
};

} // end namespace KeychainCore

} // end namespace Security

#endif /* !_SECURITY_DYNAMICDLDBLIST_H_ */
