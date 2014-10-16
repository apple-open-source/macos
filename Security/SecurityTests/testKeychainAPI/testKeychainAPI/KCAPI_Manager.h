// ======================================================================
//	File:		KCAPI_Manager.h
//
//	Operation classes for KC manager APIs:
//		- KCGetKeychainManagerVersion
//		- KeychainManagerAvailable
//
//
//	Copyright:	Copyright (c) 2000,2003 Apple Computer, Inc. All Rights Reserved.
//
//	Change History (most recent first):
//
//		 <1>	2/22/00	em		Created.
// ======================================================================

#ifndef __KCAPI_MANAGER__
#define __KCAPI_MANAGER__

#include "KCOperation.h"
#include "KCOperationID.h"

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KCGetKeychainManagerVersion
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class COp_KCGetKeychainManagerVersion : public KCOperation
{
public:
OPERATION_ID(KCGetKeychainManagerVersion)

								COp_KCGetKeychainManagerVersion();
	virtual	OSStatus			Operate();

protected:
	CParamUInt32				mVersion;
};

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ COp_KeychainManagerAvailable
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class COp_KeychainManagerAvailable : public KCOperation
{
public:
OPERATION_ID(KeychainManagerAvailable)

								COp_KeychainManagerAvailable();						
	virtual	OSStatus			Operate();

protected:
	CParamBoolean				mAvailable;
};

#endif	// __KCAPI_MANAGER__
