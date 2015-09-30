/*
 * Copyright (c) 2004,2011,2014 Apple Inc. All Rights Reserved.
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
    DynamicDLDBList.cpp
*/

#include "DynamicDLDBList.h"

#include "Globals.h"

#include <security_utilities/debugging.h>
#include <security_cdsa_utilities/cssmbridge.h> // For Required()
#include <security_cdsa_client/mdsclient.h>
#include <security_cdsa_client/mds_standard.h>
#include "KCEventNotifier.h"

using namespace KeychainCore;

//
// DynamicDLDBList
//
DynamicDLDBList::DynamicDLDBList()
	: mMutex(Mutex::recursive), mSearchListSet(false)
{
}

DynamicDLDBList::~DynamicDLDBList()
{
}

CSSM_RETURN
DynamicDLDBList::appNotifyCallback(const CSSM_GUID *guid, void *context,
		uint32 subserviceId, CSSM_SERVICE_TYPE subserviceType, CSSM_MODULE_EVENT eventType)
{
	CSSM_RETURN status = 0;
	try
	{
		reinterpret_cast<DynamicDLDBList *>(context)->callback(Guid::required(guid),
			subserviceId, subserviceType, eventType);
	}
	catch (const CommonError &error)
	{
		status = error.osStatus();
	}
	catch (...)
	{
	}

	return status;	
}

/* Assume mLock is locked already. Add all databases for this module. */
bool
DynamicDLDBList::_add(const Guid &guid, uint32 subserviceID, CSSM_SERVICE_TYPE subserviceType)
{
	return _add(dlDbIdentifier(guid, subserviceID, subserviceType));
}

/* Assume mLock is locked already.  Add a single database to the searchlist.  */
bool
DynamicDLDBList::_add(const DLDbIdentifier &dlDbIdentifier)
{
	StLock<Mutex>_(mMutex);
	
	if (find(mSearchList.begin(), mSearchList.end(), dlDbIdentifier) == mSearchList.end())
	{
		mSearchList.push_back(dlDbIdentifier);
		return true;
	}

	return false;
}

/* Assume mLock is locked already. Remove all databases for this module. */
bool
DynamicDLDBList::_remove(const Guid &guid, uint32 subserviceID, CSSM_SERVICE_TYPE subserviceType)
{
	return _remove(dlDbIdentifier(guid, subserviceID, subserviceType));
}

/* Assume mLock is locked already.  Remove a single database from the
   searchlist.  */
bool
DynamicDLDBList::_remove(const DLDbIdentifier &dlDbIdentifier)
{
	StLock<Mutex>_(mMutex);
	
	// search for subserviceUid but ignore the dbName, which is dynamic
	for (SearchList::iterator it = mSearchList.begin(); it != mSearchList.end(); it++)
		if (it->ssuid() == dlDbIdentifier.ssuid())
		{
			mSearchList.erase(it);

			// Remove from the storageManager cache if it was there.
			globals().storageManager.didRemoveKeychain(dlDbIdentifier);
			return true;
		}
	// not found
	return false;
}

bool
DynamicDLDBList::_load()
{
	StLock<Mutex>_(mMutex);
	
	bool list_changed = false;
	MDSClient::Directory &mds = MDSClient::mds();
	MDSClient::Table<MDSClient::Common> common(mds);
	MDSClient::Table<MDSClient::DL> dl(mds);
	MDSClient::Table<MDSClient::CSP> csp(mds);

	for (MDSClient::Table<MDSClient::Common>::iterator commonIt =
		common.find(MDSClient::Attribute("DynamicFlag") != false);
		commonIt != common.end(); ++commonIt)
	{
		CSSM_SERVICE_MASK serviceMask = (*commonIt)->serviceMask();
		if (serviceMask & CSSM_SERVICE_DL)
		{
			string moduleID = (*commonIt)->moduleID();
			secdebug("dynamic", "Loading dynamic %sDL module: %s",
				(serviceMask & CSSM_SERVICE_CSP) ? "CSP/" : "", moduleID.c_str());

			/* Register module for callbacks and load it. */
			Guid moduleGuid(moduleID);
			CssmClient::Module module(moduleGuid);
			module->appNotifyCallback(appNotifyCallback, this);
			module->load();
			mModules.push_back(module);

			/* Now that we have registered for notifications, Find all already
			   registered dl subsevices for this module. */
			for (MDSClient::Table<MDSClient::DL>::iterator dlIt =
				dl.find(MDSClient::Attribute("ModuleID") == moduleID);
				dlIt!= dl.end(); ++dlIt)
			{
				uint32 subserviceID = (*dlIt)->subserviceID();
				bool hasCSP = csp.find(MDSClient::Attribute("ModuleID") == moduleID
					&& MDSClient::Attribute("SSID") == subserviceID) != csp.end();

				secdebug("dynamic", "Adding databases from %sDL SSID %lu module: %s",
						hasCSP ? "CSP/" : "", (unsigned long)subserviceID, moduleID.c_str());
				list_changed |= _add(moduleGuid, subserviceID,
                                     hasCSP ? CSSM_SERVICE_CSP | CSSM_SERVICE_DL : CSSM_SERVICE_DL);
			}
		}
	}

	return list_changed;
}

const vector<DLDbIdentifier> &
DynamicDLDBList::searchList()
{
	StLock<Mutex>_(mMutex);
    if (!mSearchListSet)
    {
		// Load all dynamic DL's so we start receiving notifications.
		_load();

        mSearchListSet = true;
    }

	return mSearchList;
}

void
DynamicDLDBList::callback(const Guid &guid, uint32 subserviceID,
	CSSM_SERVICE_TYPE subserviceType, CSSM_MODULE_EVENT eventType)
{
	secdebug("event", "Received callback from guid: %s ssid: %lu type: %lu event: %lu",
 			guid.toString().c_str(), (unsigned long)subserviceID, (unsigned long)subserviceType, (unsigned long)eventType);

	StLock<Mutex>_(mMutex);
	
	bool list_changed = false;

	if (subserviceType & CSSM_SERVICE_DL)
	{
		if (eventType == CSSM_NOTIFY_INSERT)
		{
			/* A DL or CSP/DL was inserted. */
			secdebug("dynamic", "%sDL module: %s SSID: %lu inserted",
				(subserviceType & CSSM_SERVICE_CSP) ? "CSP/" : "", guid.toString().c_str(), (unsigned long)subserviceID);
			list_changed = _add(guid, subserviceID, subserviceType);
		}
		else if (eventType == CSSM_NOTIFY_REMOVE)
		{
			/* A DL or CSP/DL was removed. */
			secdebug("dynamic", "%sDL module: %s SSID: %lu removed",
				(subserviceType & CSSM_SERVICE_CSP) ? "CSP/" : "", guid.toString().c_str(), (unsigned long)subserviceID);
			list_changed = _remove(guid, subserviceID, subserviceType);
		}
	}

	if (list_changed)
	{
		// Make sure we are not holding mLock nor the StorageManager mLock when we post these events.
		// @@@ Rather than posting we should simulate a receive since each client will receive this
		// cssm level notification.
		KCEventNotifier::PostKeychainEvent(kSecKeychainListChangedEvent);
	}
}

DLDbIdentifier DynamicDLDBList::dlDbIdentifier(const Guid &guid,
	uint32 subserviceID, CSSM_SERVICE_TYPE subserviceType)
{
	CSSM_VERSION theVersion={};
    CssmSubserviceUid ssuid(guid, &theVersion, subserviceID, subserviceType);
	CssmNetAddress *dbLocation=NULL;

	return DLDbIdentifier(ssuid, NULL, dbLocation);
}
