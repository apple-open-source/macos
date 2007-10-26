/*
 * Copyright (c) 2006 Apple Computer, Inc. All Rights Reserved.
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
 *  reader.cpp
 *  SmartCardServices
*/

#include "reader.h"
#include "eventhandler.h"
#include "pcsclite.h"
#include <security_utilities/debugging.h>

static PCSCD::Readers *mReaders;

namespace PCSCD {


Readers::Readers()
{
}

Readers::~Readers()
{
}

bool Readers::find(const char *name, XReaderContext &rc) const
{
	return false;
}

bool Readers::find(uint32_t port, const char *name, XReaderContext &rc) const
{
	return false;
}

bool Readers::find(uint32_t id, XReaderContext &rc) const
{
	return false;
}


} // end namespace PCSCD

#pragma mark ---------- C Interface ----------

LONG XRFAllocateReaderSpace(DWORD dwAllocNum)
{
	try
	{
		mReaders = new PCSCD::Readers();
	}
	catch (...)
	{
		secdebug("pcscd", "failed to allocate Readers");
		return -1;
	}
	return EHInitializeEventStructures();
}

LONG XRFReaderInfo(LPSTR lpcReader, PREADER_CONTEXT *sReader)
{
	// Find a reader given a name
	PCSCD::XReaderContext rc;	//>>>> use iterator instead
	if (!sReader)
		return SCARD_E_INVALID_PARAMETER;

	if (!mReaders->find(lpcReader, rc))
		return SCARD_E_UNKNOWN_READER;

	*sReader = &rc;	//>>>> WRONG - temporary var
	return SCARD_S_SUCCESS;
}

LONG XRFReaderInfoNamePort(DWORD dwPort, LPSTR lpcReader, PREADER_CONTEXT *sReader)
{
	// Find a reader given a name
	PCSCD::XReaderContext rc;
	if (!sReader)
		return SCARD_E_INVALID_PARAMETER;

	if (!mReaders->find(dwPort, lpcReader, rc))
		return SCARD_E_UNKNOWN_READER;

	*sReader = &rc;	//>>>> WRONG - temporary var
	return SCARD_S_SUCCESS;
}

LONG XRFReaderInfoById(DWORD dwIdentity, PREADER_CONTEXT * sReader)
{
	// Find a reader given a handle
	PCSCD::XReaderContext rc;
	if (!sReader)
		return SCARD_E_INVALID_PARAMETER;

	if (!mReaders->find(dwIdentity, rc))
		return SCARD_E_INVALID_VALUE;

	*sReader = &rc;	//>>>> WRONG - temporary var
	return SCARD_S_SUCCESS;
}

LONG XRFCheckSharing(DWORD hCard)
{
	PCSCD::XReaderContext rc;
	if (!mReaders->find(hCard, rc))
		return SCARD_E_INVALID_VALUE;

	return (rc.dwLockId == 0 || rc.dwLockId == hCard)?SCARD_S_SUCCESS:SCARD_E_SHARING_VIOLATION;
}

LONG XRFLockSharing(DWORD hCard)
{
	PCSCD::XReaderContext rc;
	if (!mReaders->find(hCard, rc))
		return SCARD_E_INVALID_VALUE;

	if (rc.dwLockId != 0 && rc.dwLockId != hCard)
	{
		secdebug("pcscd", "XRFLockSharing: Lock ID invalid: %d", rc.dwLockId);
		return SCARD_E_SHARING_VIOLATION;
	}
	
	EHSetSharingEvent(&rc, 1);
	rc.dwLockId = hCard;
	return SCARD_S_SUCCESS;
}

LONG XRFUnlockSharing(DWORD hCard)
{
	PCSCD::XReaderContext rc;
	if (!mReaders->find(hCard, rc))
		return SCARD_E_INVALID_VALUE;

	if (rc.dwLockId != 0 && rc.dwLockId != hCard)
	{
		secdebug("pcscd", "XRFUnlockSharing: Lock ID invalid: %d", rc.dwLockId);
		return SCARD_E_SHARING_VIOLATION;
	}
	
	EHSetSharingEvent(&rc, 0);
	rc.dwLockId = 0;
	return SCARD_S_SUCCESS;
}

