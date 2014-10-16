// ======================================================================
//	File:		KCOperationID.cpp
//
//	OperationID must be registered so that a test script knows how to
//	construct an appropriate KCOperation class
//
//	Copyright:	Copyright (c) 2000,2003,2008 Apple Inc. All Rights Reserved.
//
//	Change History (most recent first):
//
//		 <1>	2/22/00	em		Created.
// ======================================================================

//#include "SecAPI_Keychain.h"
#include "KCOperationID.h"
#include "KCAPI_Manager.h"
#include "KCAPI_Keychain.h"
#include "KCAPI_Password.h"
#include "KCAPI_Item.h"
#include "KCAPI_Cert.h"
#include "KCAPI_CString.h"




// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ Static initialization
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
bool			COpRegister::sRegistered = false;
tOperationInfo	COpRegister::sOperationInfoTbl[OpID_NumOperations];

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// 	¥ RegisterAll
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
void
COpRegister::RegisterAll()     
{
	if(sRegistered) return;

									// Getting KC manager info
									// (KCAPI_Manager)
	Register(KCGetKeychainManagerVersion);
	Register(KeychainManagerAvailable);

									// High level KC APIs
									// (KCAPI_Keychain)
//	Register(KCMakeKCRefFromFSRef);
	Register(KCMakeKCRefFromFSSpec);
	Register(KCMakeKCRefFromAlias);
	Register(KCMakeAliasFromKCRef);
	Register(KCReleaseKeychain);
	Register(KCUnlockNoUI);
		Register(KCUnlock);
		Register(KCLogin);
		Register(KCChangeLoginPassword);
		Register(KCLogout);
		Register(KCUnlockWithInfo);
		Register(KCLock);
	//Register(KCLockNoUI);
	Register(KCGetDefaultKeychain);
	Register(KCSetDefaultKeychain);
		Register(KCCreateKeychain);
	Register(KCCreateKeychainNoUI);
	Register(KCGetStatus);
	Register(KCChangeSettingsNoUI);
	Register(KCGetKeychain);
	Register(KCGetKeychainName);
		Register(KCChangeSettings);
	Register(KCCountKeychains);
	Register(KCGetIndKeychain);
	Register(KCAddCallback);
	Register(KCRemoveCallback);
	Register(KCSetInteractionAllowed);
	Register(KCIsInteractionAllowed);
	
										// Storing and retrieveng passwords
										// (KCAPI_Password)
    Register(KCAddAppleSharePassword);
    Register(KCFindAppleSharePassword);
    Register(KCAddInternetPassword);
    Register(KCAddInternetPasswordWithPath);
    Register(KCFindInternetPassword);
    Register(KCFindInternetPasswordWithPath);
    Register(KCAddGenericPassword);
    Register(KCFindGenericPassword);
		
										// Managing KC items
										// (KCAPI_Item)
	Register(KCNewItem);
	Register(KCSetAttribute);
	Register(KCGetAttribute);
	Register(KCSetData);
		Register(KCGetData);
	//Register(KCGetDataNoUI);
		Register(KCAddItem);
	Register(KCAddItemNoUI);
		Register(KCDeleteItem);
	Register(KCDeleteItemNoUI);
	Register(KCUpdateItem);
	Register(KCReleaseItem);
	Register(KCCopyItem);
	Register(KCFindFirstItem);
	Register(KCFindNextItem);
	Register(KCReleaseSearch);
	
										// Working with Certificates
										// (KCAPI_Cert)
	Register(KCFindX509Certificates);
		Register(KCChooseCertificate);
	
										// KC Manager calls that use "C" strings
										// (KCAPI_CString)
		Register(kcunlock);
		Register(kccreatekeychain);
	Register(kcgetkeychainname);
		Register(kcaddapplesharepassword);
		Register(kcfindapplesharepassword);
		Register(kcaddinternetpassword);
		Register(kcaddinternetpasswordwithpath);
		Register(kcfindinternetpassword);
		Register(kcfindinternetpasswordwithpath);
		Register(kcaddgenericpassword);
		Register(kcfindgenericpassword);
     }
