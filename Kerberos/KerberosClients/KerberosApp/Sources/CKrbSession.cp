/* $Copyright:
 *
 * Copyright 1998-2002 by the Massachusetts Institute of Technology.
 * 
 * All rights reserved.
 * 
 * Export of this software from the United States of America may require a
 * specific license from the United States Government.  It is the
 * responsibility of any person or organization contemplating export to
 * obtain such a license before exporting.
 * 
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and distribute
 * this software and its documentation for any purpose and without fee is
 * hereby granted, provided that the above copyright notice appear in all
 * copies and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of M.I.T. not be
 * used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  Furthermore if you
 * modify this software you must label your software as modified software
 * and not distribute it in such a fashion that it might be confused with
 * the original MIT software. M.I.T. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * Individual source code files are copyright MIT, Cygnus Support,
 * OpenVision, Oracle, Sun Soft, FundsXpress, and others.
 * 
 * Project Athena, Athena, Athena MUSE, Discuss, Hesiod, Kerberos, Moira,
 * and Zephyr are trademarks of the Massachusetts Institute of Technology
 * (MIT).  No commercial use of these trademarks may be made without prior
 * written permission of MIT.
 * 
 * "Commercial use" means use of a name in a product or other for-profit
 * manner.  It does NOT prevent a commercial firm from referring to the MIT
 * trademarks in order to convey information (although in doing so,
 * recognition of their trademark status should be given).
 * $
 */

/* $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/CKrbSession.cp,v 1.124 2003/06/27 00:50:41 smcguire Exp $ */

// ===========================================================================
//	CKrbSession.cp
// ===========================================================================

#include <string>

#if TARGET_RT_MAC_CFM
	extern "C" {
	#include <CredentialsCache/CredentialsCache.h>
	}

	#include <KerberosWrappers/UPrincipal.h> // from KrbWrappersLib
	#include <KerberosWrappers/UCCache.h>	// from KrbWrappersLib

	#include <KerberosLogin/KerberosLogin.h>
	#include <KerberosSupport/Utilities.h>

#else
	#include <Kerberos/Kerberos.h>
	#include <sys/time.h>
	#include "UCCache.h"
#endif

//#include <PointerValidation.h>

#include <utime.h>

#include "CKerberosManagerApp.h"
#include "CKrbErrorAlert.h"
#include "CKrbSession.h"
#include "CTicketListPrincipalItem.h"
#include "CTicketListSubPrincipalItem.h"
#include "CTicketListCredentialsItem.h"
#include "UModalDialogs.h"
#include "UAMModalDialogs.h"

#include "string.h"

PP_Using_Namespace_PowerPlant

// ---------------------------------------------------------------------------
//		¥ CKrbSession constructor
// ---------------------------------------------------------------------------
CKrbSession::CKrbSession() : LPeriodical() {
	cc_int32 cacheErr = ccNoError;

	mCacheDirty = true; 
	mCCContext = NULL;
	
	//get a pointer to the preferences
	CKerberosManagerApp *theApp = static_cast<CKerberosManagerApp *>(LCommander::GetTopCommander());
	Assert_(theApp != NULL);
	mKrbPrefsRef = theApp->GetKrbPreferencesRef();
	Assert_(mKrbPrefsRef != NULL);

	//init our CCache context
	cc_int32 apiSupported;
	cc_int32 apiRequested = ccapi_version_4;
	
	cacheErr = cc_initialize(&mCCContext, apiRequested, &apiSupported, NULL);
	if (cacheErr != ccNoError) {
		char errStr1[256], errStr2[256];
		
		if (cacheErr == ccErrBadAPIVersion) {
			sprintf(errStr1, "Credentials Cache library version is not the expected version.");
			sprintf(errStr2, "Requested version %ld, Library supports version %ld of the CCache API.  The Kerberos application cannot continue.", (long)apiRequested, (long)apiSupported);
		} else {
			sprintf(errStr1, "Error initializing Credentials Cache library.");
			sprintf(errStr2, "Error number %ld.", (long)cacheErr);
		}
		CKrbErrorAlert *alrt = new CKrbErrorAlert(errStr1, errStr2, "OK", NULL, true);
		alrt->DisplayErrorAlert();
		delete alrt;
		return;
	}
	
	mLastChangeTime = 0;
	
	struct timeval currentTime;
	
	gettimeofday(&currentTime, nil);
	mActiveUserExpTime = (UInt32) currentTime.tv_sec;
	
	mForwardable = false;

	LString::CopyPStr("\p", mActiveUserPrincipal);
	LString::CopyPStr("\p", mActiveUserRealm);
	
	this->SetActiveUserFromCache();
	
	this->StartIdling(0.5);
}

// ---------------------------------------------------------------------------
//		¥ SetActiveUserFromCache
// ---------------------------------------------------------------------------
/* check for existing tickets on startup, and sets the active user info
   appropriately if they're found */
void CKrbSession::SetActiveUserFromCache()
{
	UCCacheContext contextWrapper;
	UPrincipal::EVersion credCacheVersion;
	std::string princName, displayName;
	UInt32 cacheExpTime = 0;
	KLPrincipal tempPrinc = nil;
	KLStatus klErr = klNoErr;
	short cacheValidity = kTicketInvalidUnknown; // may as well start with this assumption
	
	try {
		UCCache defaultCCache = contextWrapper.OpenDefaultCCache();
	
		credCacheVersion = defaultCCache.GetCredentialsVersion();
		
		if (credCacheVersion == UPrincipal::kerberosV4And5)
			credCacheVersion = UPrincipal::kerberosV5;
		
		princName = defaultCCache.GetPrincipal(credCacheVersion).GetString(credCacheVersion);
		displayName = defaultCCache.GetPrincipal(credCacheVersion).GetDisplayString(credCacheVersion);
				
		klErr = KLCreatePrincipalFromString(princName.c_str(), (credCacheVersion==UPrincipal::kerberosV5)?kerberosVersion_V5:kerberosVersion_V4, &tempPrinc);
		if (klErr == klNoErr) {
			klErr = KLTicketExpirationTime(tempPrinc, kerberosVersion_All, (KLTime *)&cacheExpTime);
			// check for srvtab case - there might be only one kind of tix
			if (klErr == klNoCredentialsErr) {
				klErr = KLTicketExpirationTime(tempPrinc, kerberosVersion_Any, (KLTime *)&cacheExpTime);
			}
			cacheValidity = kTicketValid;
			
			if (klErr != klNoErr) {
				cacheExpTime = 0;
				if (klErr == klCredentialsBadAddressErr) {
					cacheValidity = kTicketInvalidBadAddress;
				} else if (klErr == klCredentialsNeedValidationErr ) {
					cacheValidity = kTicketInvalidNeedsValidation;
				} else if (klErr == klPrincipalDoesNotExistErr) {
					throw klErr;
				} else if (klErr != klCredentialsExpiredErr) {
					cacheValidity = kTicketInvalidUnknown;
					
					char *klErrorString;
					KLGetErrorString(klErr, &klErrorString);
					SignalString_(klErrorString);
					KLDisposeString(klErrorString);
				}		
			}
			KLDisposePrincipal(tempPrinc);
			tempPrinc = nil;
		}
		
		SetActiveUserInfo(displayName.c_str(), cacheExpTime, cacheValidity);
	}
	
	catch (...) {
            if (tempPrinc != nil)
                KLDisposePrincipal(tempPrinc);
                
            SetActiveUserInfo(NULL, 0, kTicketInvalidUnknown);
	}
}

// ---------------------------------------------------------------------------
//		¥ ~CKrbSession
// ---------------------------------------------------------------------------
CKrbSession::~CKrbSession() {

	this->StopIdling();
	
	// dispose of CCache context
	// used to check for errors here but there's not much point...
	if (mCCContext != NULL)
		cc_context_release(mCCContext);

}

// ---------------------------------------------------------------------------
//		¥ ListenToMessage
// ---------------------------------------------------------------------------
// Listen to messgaes 
void CKrbSession::ListenToMessage(MessageT inMessage, void *ioParam) {
#pragma unused(inMessage, ioParam)
}

// ---------------------------------------------------------------------------
//		¥ DoLogin
// ---------------------------------------------------------------------------
// start the process of logging a new user, this is the function most of
// KTM should interface with
void CKrbSession::DoLogin()
{
	KLStatus loginLibErr = KLAcquireNewInitialTickets(nil, nil, nil, nil);
	
	if ((loginLibErr != klNoErr) && (loginLibErr != klUserCanceledErr)) {
		char *klErrorString;
		
		KLGetErrorString(loginLibErr, &klErrorString);
		CKrbErrorAlert *alrt = new CKrbErrorAlert("An error occurred while logging in.",
			klErrorString,
			"OK", NULL, false);
		alrt->DisplayErrorAlert();
		delete alrt;
		
		KLDisposeString(klErrorString);
	}

	// we're not setting the application default (which only applies to KM) here,
	// but nothing in KM depends on this being set, so that should be okay
}

// ---------------------------------------------------------------------------
//		¥ DoRenew
// ---------------------------------------------------------------------------
// set up to do a renew of a user
// we need a version to do the bigstring princ conversion for login library
void CKrbSession::DoRenew(Str255 inPrinc, cc_int32 inPrincVersion) {

	KLPrincipal renewPrinc = nil;
	KLStatus klErr = klNoErr;
	
	LString::PToCStr(inPrinc);
	
	// convert cc kerb version to right kll kerb version, it is in v5 format
	if ( (inPrincVersion == cc_credentials_v4_v5) || (inPrincVersion == cc_credentials_v5))
		inPrincVersion = kerberosVersion_V5;
	else
		inPrincVersion = kerberosVersion_V4;
		
	klErr = KLCreatePrincipalFromString((char *)inPrinc, (UInt32)inPrincVersion, &renewPrinc);
	if (klErr == klNoErr) {
		// first try to renew tickets without an interface; if this doesn't work
		// fall back on old method
		klErr = KLRenewInitialTickets(renewPrinc, nil, nil, nil);
		
		if (klErr != klNoErr) {
			klErr = KLAcquireNewInitialTickets(renewPrinc, nil, nil, nil);

			if ((klErr != klNoErr) && (klErr != klUserCanceledErr)) {
				char *klErrorString;
				
				KLGetErrorString(klErr, &klErrorString);
				CKrbErrorAlert *alrt = new CKrbErrorAlert("An error occurred while logging in.",
					klErrorString,
					"OK", NULL, false);
				alrt->DisplayErrorAlert();
				delete alrt;
				
				KLDisposeString(klErrorString);
			}
		}
		
		// we're not setting the application default (which only applies to KM) here,
		// but nothing in KM depends on this being set, so that should be okay

		KLDisposePrincipal(renewPrinc);
	} else {
		Assert_(klErr == klNoErr);
	}
}

// ---------------------------------------------------------------------------
//		¥ DoAutoRenew
// ---------------------------------------------------------------------------
// set up to do a renew of a user, but never show UI if we can't
// we need a version to do the bigstring princ conversion for login library
Boolean CKrbSession::DoAutoRenew(Str255 inPrinc, cc_int32 inPrincVersion) {

	KLPrincipal renewPrinc = nil;
	KLStatus klErr = klNoErr;
	
	LString::PToCStr(inPrinc);
	
	// convert cc kerb version to right kll kerb version, it is in v5 format
	if ( (inPrincVersion == cc_credentials_v4_v5) || (inPrincVersion == cc_credentials_v5))
		inPrincVersion = kerberosVersion_V5;
	else
		inPrincVersion = kerberosVersion_V4;
		
	klErr = KLCreatePrincipalFromString((char *)inPrinc, (UInt32)inPrincVersion, &renewPrinc);
	if (klErr == klNoErr) {
		// try to renew tickets without an interface; if this doesn't work we just give up
		// because we don't want Kerberos.app to randomly display the long in dialog
		klErr = KLRenewInitialTickets(renewPrinc, nil, nil, nil);
		
		KLDisposePrincipal(renewPrinc);
	}
	
	// in practice ought to assert that we didn't get an error from CreatePrincipalFromString
	// but again, we don't want this automatic operation to put up dialogs
	
	return (klErr == klNoErr);
}

// ---------------------------------------------------------------------------
//		¥ DoLogout
// ---------------------------------------------------------------------------
// destroy tickets and remove the principal from the cache
// applies to the active principal
void CKrbSession::DoLogout(char *inPrincName, cc_int32 inPrincVersion) {
	
	KLPrincipal destroyPrinc = nil;
	KLStatus klErr = klNoErr;
	
	Assert_(inPrincName != NULL);
	
	// convert cc kerb version to right kll kerb version, it is in v5 format
	if ( (inPrincVersion == cc_credentials_v4_v5) || (inPrincVersion == cc_credentials_v5))
		inPrincVersion = kerberosVersion_V5;
	else
		inPrincVersion = kerberosVersion_V4;
		
	klErr = KLCreatePrincipalFromString(inPrincName, (KLKerberosVersion)inPrincVersion, &destroyPrinc);
	if (klErr == klNoErr) {
		klErr = KLDestroyTickets(destroyPrinc);

		if (klErr != klNoErr) {
			char *klErrorString;
			
			KLGetErrorString(klErr, &klErrorString);
			CKrbErrorAlert *alrt = new CKrbErrorAlert("An error occurred while destroying tickets.",
				klErrorString,
				"OK", NULL, false);
			alrt->DisplayErrorAlert();
			delete alrt;
			
			KLDisposeString(klErrorString);
		}

		KLDisposePrincipal(destroyPrinc);
	} else {
		Assert_(klErr == klNoErr);
	}

	return;
}

// ---------------------------------------------------------------------------
//		¥ GetCacheInfo
// ---------------------------------------------------------------------------
// return an array of CTicketListItem
// items could be of princType or credType
// any cred types following a princType belong to that princType
// until another princType is reached
// we'll let the list object parse this into a heirarchical structure
LArray * CKrbSession::GetCacheInfo() {

	UCCacheContext contextWrapper;
	UCCache currentCCache;
	UCredentials currentCreds;
	UPrincipal::EVersion credCacheVersion;
	std::string v5PrincName, v4PrincName, v5DisplayName, v4DisplayName;
	UInt32 cacheExpTime, cacheStartTime;
	short cacheValidity = kTicketInvalidUnknown; // may as well start with this assumption
	KLPrincipal tempPrinc = nil;
	KLStatus klErr = klNoErr;
	cc_credentials_v5_t *v5Creds = nil;
	cc_credentials_v4_t *v4Creds = nil;
	
	LArray *princArray = NULL;
	char temp[255];
	
	//allocate array
	princArray = new LArray(sizeof(CTicketListItem *));
	if (princArray == NULL) return NULL;
	
	try {
		// get the name of the default cache so we can mark it later
		UCCacheString defaultCacheName = contextWrapper.GetDefaultCCacheName();
		
		//now iterate through whole ccache to get rest of tickets to display
		UCCacheIterator ccacheIterator = contextWrapper.NewCCacheIterator();
		
		while (ccacheIterator.Next(currentCCache)) {
			credCacheVersion = currentCCache.GetCredentialsVersion();
			
			UCCacheString currentCCacheName = currentCCache.GetName();
			
			if ((credCacheVersion == UPrincipal::kerberosV4And5) || (credCacheVersion == UPrincipal::kerberosV5)) {
				v5PrincName = currentCCache.GetPrincipal(UPrincipal::kerberosV5).GetString(UPrincipal::kerberosV5);
				v5DisplayName = currentCCache.GetPrincipal(UPrincipal::kerberosV5).GetDisplayString(UPrincipal::kerberosV5);
			}
			
			if ((credCacheVersion == UPrincipal::kerberosV4And5) || (credCacheVersion == UPrincipal::kerberosV4)) {
				v4PrincName = currentCCache.GetPrincipal(UPrincipal::kerberosV4).GetString(UPrincipal::kerberosV4);
				v4DisplayName = currentCCache.GetPrincipal(UPrincipal::kerberosV4).GetDisplayString(UPrincipal::kerberosV4);
			}
			
			if ((credCacheVersion == UPrincipal::kerberosV4And5) || (credCacheVersion == UPrincipal::kerberosV5)) {
				klErr = KLCreatePrincipalFromString(v5PrincName.c_str(), kerberosVersion_V5, &tempPrinc);
				if (klErr == klNoErr) {
					klErr = KLTicketStartTime(tempPrinc, kerberosVersion_All, (KLTime *)&cacheStartTime);
					// check for srvtab case - there might be only one kind of tix
					if (klErr == klNoCredentialsErr) {
						klErr = KLTicketStartTime(tempPrinc, kerberosVersion_Any, (KLTime *)&cacheStartTime);
					}

					klErr = KLTicketExpirationTime(tempPrinc, kerberosVersion_All, (KLTime *)&cacheExpTime);
					// check for srvtab case - there might be only one kind of tix
					if (klErr == klNoCredentialsErr) {
						klErr = KLTicketExpirationTime(tempPrinc, kerberosVersion_Any, (KLTime *)&cacheExpTime);
					}

					cacheValidity = kTicketValid;
					
					if (klErr != klNoErr) {
						cacheStartTime = 0;
						cacheExpTime = 0;
						if (klErr == klCredentialsBadAddressErr) {
							cacheValidity = kTicketInvalidBadAddress;
						} else if (klErr == klCredentialsNeedValidationErr) {
							cacheValidity = kTicketInvalidNeedsValidation;
						} else if (klErr == klPrincipalDoesNotExistErr) {
							// something bad happened to the ccache while we were reading it.  get out
							throw klPrincipalDoesNotExistErr;							
						} else if (klErr != klCredentialsExpiredErr) {
							cacheValidity = kTicketInvalidUnknown;
							
							char *klErrorString;
							KLGetErrorString(klErr, &klErrorString);
							SignalString_(klErrorString);
							KLDisposeString(klErrorString);
						}		
					}
					
					if (tempPrinc != nil) {
						KLDisposePrincipal(tempPrinc);
						tempPrinc = nil;
					}
				}
			} else {
				klErr = KLCreatePrincipalFromString(v4PrincName.c_str(), kerberosVersion_V4, &tempPrinc);
				if (klErr == klNoErr) {
					klErr = KLTicketStartTime(tempPrinc, kerberosVersion_All, (KLTime *)&cacheStartTime);
					// check for srvtab case - there might be only one kind of tix
					if (klErr == klNoCredentialsErr) {
						klErr = KLTicketStartTime(tempPrinc, kerberosVersion_Any, (KLTime *)&cacheStartTime);
					}

					klErr = KLTicketExpirationTime(tempPrinc, kerberosVersion_All, (KLTime *)&cacheExpTime);
					// check for srvtab case - there might be only one kind of tix
					if (klErr == klNoCredentialsErr) {
						klErr = KLTicketExpirationTime(tempPrinc, kerberosVersion_Any, (KLTime *)&cacheExpTime);
					}
					cacheValidity = kTicketValid;
					
					if (klErr != klNoErr) {
						cacheStartTime = 0;
						cacheExpTime = 0;
						if (klErr == klCredentialsBadAddressErr) {
							cacheValidity = kTicketInvalidBadAddress;
						} else if (klErr == klCredentialsNeedValidationErr) {
							cacheValidity = kTicketInvalidNeedsValidation;
						} else if (klErr == klPrincipalDoesNotExistErr) {
							// something bad happened to the ccache while we were reading it.  get out
							throw klPrincipalDoesNotExistErr;							
						} else if (klErr != klCredentialsExpiredErr) {
							cacheValidity = kTicketInvalidUnknown;
							
							char *klErrorString;
							KLGetErrorString(klErr, &klErrorString);
							SignalString_(klErrorString);
							KLDisposeString(klErrorString);
						}
					}
					
					if (tempPrinc != nil) {
						KLDisposePrincipal(tempPrinc);
						tempPrinc = nil;
					}
				}
			}
			
			// create new CTicketListPrincItem, mark as default if the ccache names match
			
			CTicketListPrincipalItem *princItem;
			// make a copy of the UCCache for storing in the princ; must Reset later to prevent double-dispose
			UCCache ccacheForItem = UCCache(currentCCache.Get());
			
			// try block to invalidate currentCCache if necessary
			try {
				if ((credCacheVersion == UPrincipal::kerberosV4And5) || (credCacheVersion == UPrincipal::kerberosV5)) {
					princItem = new CTicketListPrincipalItem(v5PrincName.c_str(), v5DisplayName.c_str(), cacheStartTime, cacheExpTime, credCacheVersion, ccacheForItem, cacheValidity);
				} else {
					princItem = new CTicketListPrincipalItem(v4PrincName.c_str(), v4DisplayName.c_str(), cacheStartTime, cacheExpTime, credCacheVersion, ccacheForItem, cacheValidity);
				}
				Assert_(princItem != NULL);

				if (strcmp(defaultCacheName.CString(), currentCCacheName.CString()))
					princItem->SetPrincipalAsInactive();
				else
					princItem->SetPrincipalAsActive();
				
				princArray->AddItem(&princItem);

				CTicketListSubPrincipalItem *subPrincItem;
				CTicketListCredentialsItem *credsItem;
				
				// Add Kerberos v4 principal & credential information to the list
				if ((credCacheVersion == UPrincipal::kerberosV4And5) || (credCacheVersion == UPrincipal::kerberosV4)) {
					// create sub-principal item (only if this is a mixed cache, otherwise it's redundant)
					if (credCacheVersion == UPrincipal::kerberosV4And5) {
						subPrincItem = new CTicketListSubPrincipalItem(v4PrincName.c_str(), v4DisplayName.c_str(), cacheStartTime, cacheExpTime, cc_credentials_v4, princItem);
						Assert_(subPrincItem != NULL);
						princArray->AddItem(&subPrincItem);
					}
					
					// is this cache valid?  if not, we will list tix as expired
					KLBoolean v4TixValid = true;
					cacheValidity = true;
					
					klErr = KLCreatePrincipalFromString(v4PrincName.c_str(), kerberosVersion_V4, &tempPrinc);
					if (klErr == klNoErr) {
						klErr = KLCacheHasValidTickets(tempPrinc, kerberosVersion_V4, &v4TixValid, nil, nil);
						if (klErr != klNoErr) {
							if (klErr == klCredentialsBadAddressErr) {
								cacheValidity = kTicketInvalidBadAddress;
							} else if (klErr == klCredentialsNeedValidationErr) {
								cacheValidity = kTicketInvalidNeedsValidation;
							} else if (klErr == klPrincipalDoesNotExistErr) {
								// something bad happened to the ccache while we were reading it.  get out
								throw klPrincipalDoesNotExistErr;							
							} else if (klErr != klCredentialsExpiredErr) {
								cacheValidity = kTicketInvalidUnknown;
								
								char *klErrorString;
								KLGetErrorString(klErr, &klErrorString);
								SignalString_(klErrorString);
								KLDisposeString(klErrorString);
							}
						}
					
						if (tempPrinc != nil) {
							KLDisposePrincipal(tempPrinc);
							tempPrinc = nil;
						}
					}
					
					//iterate the creds for this cache
					UCredentialsIterator credsIterator = currentCCache.NewCredentialsIterator(UPrincipal::kerberosV4);
					
					while (credsIterator.Next(currentCreds)) {
						v4Creds = currentCreds.GetV4Credentials();
						
						std::string servicePrincName = currentCreds.GetServicePrincipal().GetString(UPrincipal::kerberosV4);

						credsItem = new CTicketListCredentialsItem(servicePrincName.c_str(), servicePrincName.c_str(),
													(v4TixValid)?v4Creds->issue_date:0,
													(v4TixValid)?(v4Creds->issue_date  + (UInt32)v4Creds->lifetime):0, cc_credentials_v4, currentCreds, cacheValidity);
						Assert_(credsItem != NULL);
						
						princArray->AddItem(&credsItem);
					}

				}
				
				// Add Kerberos v5 principal & credentials information to the list
				if ((credCacheVersion == UPrincipal::kerberosV4And5) || (credCacheVersion == UPrincipal::kerberosV5)) {
					// create sub-principal item (only if this is a mixed cache, otherwise it's redundant)
					if (credCacheVersion == UPrincipal::kerberosV4And5) {
						subPrincItem = new CTicketListSubPrincipalItem(v5PrincName.c_str(), v5DisplayName.c_str(), cacheStartTime, cacheExpTime, cc_credentials_v5, princItem);
						Assert_(subPrincItem != NULL);
						princArray->AddItem(&subPrincItem);
					}
					
					// is this cache valid?  if not, we will list tix as expired
					KLBoolean v5TixValid = true;
					cacheValidity = true;
					
					klErr = KLCreatePrincipalFromString(v5PrincName.c_str(), kerberosVersion_V5, &tempPrinc);
					if (klErr == klNoErr) {
						klErr = KLCacheHasValidTickets(tempPrinc, kerberosVersion_V5, &v5TixValid, nil, nil);
						if (klErr != klNoErr) {
							if (klErr == klCredentialsBadAddressErr) {
								cacheValidity = kTicketInvalidBadAddress;
							} else if (klErr == klCredentialsNeedValidationErr) {
								cacheValidity = kTicketInvalidNeedsValidation;
							} else if (klErr == klPrincipalDoesNotExistErr) {
								// something bad happened to the ccache while we were reading it.  get out
								throw klPrincipalDoesNotExistErr;							
							} else if (klErr != klCredentialsExpiredErr) {
								cacheValidity = kTicketInvalidUnknown;
								
								char *klErrorString;
								KLGetErrorString(klErr, &klErrorString);
								SignalString_(klErrorString);
								KLDisposeString(klErrorString);
							}
						}

						if (tempPrinc != nil) {
							KLDisposePrincipal(tempPrinc);
							tempPrinc = nil;
						}
					}
					
					//iterate the creds for this cache
					UCredentialsIterator credsIterator = currentCCache.NewCredentialsIterator(UPrincipal::kerberosV5);
					Boolean princIsRenewable = false;
					
					while (credsIterator.Next(currentCreds)) {
						v5Creds = currentCreds.GetV5Credentials();
						strcpy(temp, v5Creds->server);
						
						// if the creds contain a renewable tgt, note that so we can set the principal as renewable
						if ((strncmp("krbtgt", temp, 6) == 0) && ((currentCreds.GetV5Credentials()->ticket_flags) & TKT_FLG_RENEWABLE))
							princIsRenewable = true;
						
						LString::CToPStr(temp);
						credsItem = new CTicketListCredentialsItem((unsigned char *)temp, (unsigned char *)temp, (v5TixValid)?v5Creds->starttime:0,
																	(v5TixValid)?v5Creds->endtime:0, cc_credentials_v5, currentCreds, cacheValidity);
						Assert_(credsItem != NULL);
						
						princArray->AddItem(&credsItem);
					}
					
					princItem->SetPrincipalIsRenewable(princIsRenewable);

				}
				
				// Reset currentCCache since it's now being stored elsewhere (in a list item) and we don't want to free it here
				currentCCache.Release();
			}
			// catch block to invalidate currentCCache if necessary
			catch (...) {
				// Reset currentCCache since it's now being stored elsewhere (in a list item) and we don't want to free it here
				currentCCache.Release();
				
				throw;
			}			

		} // end iterate through collection
		
		//cache is now clean
		mCacheDirty = false;
		
		return princArray;
	}
	
	catch (UCCacheLogicError &err) {
		char errString[256];
		sprintf(errString, "UCCacheLogicError %d in GetCacheInfo()",err.Error() );
		SignalString_(errString);
	}
	catch (UCCacheRuntimeError &err) {
		char errString[256];
		// if the server vanished while we were iterating, that's an expected occurence; just clean up and don't announce anything
		if ((err.Error() != ccErrCCacheNotFound) && (err.Error() != ccErrCredentialsNotFound) && (err.Error() != ccErrServerUnavailable)) {
			sprintf(errString, "UCCacheRuntimeError %d in GetCacheInfo()",err.Error() );
			SignalString_(errString);
		}
	}
	catch (KLEStatus klErr) {
		// don't do anything except clean up below
	}
	catch (...) {
		SignalStringLiteral_("Unexpected exception in GetCacheInfo()");
	}
	
	// if we got to here an error occurred and we need to clean up
	// Reset currentCCache since it's now being stored elsewhere (in a list item) and we don't want to free it here
	currentCCache.Reset(nil);

    if (tempPrinc != nil)
        KLDisposePrincipal(tempPrinc);
            
	// clean up partially formed listitem array, if necessary
	if (princArray != nil) {
	/*
            LArrayIterator deleteIterator(*princArray, 0L);
            
            CTicketListItem *listItem = nil;
                
            while (deleteIterator.Next(&listItem)) {
                princArray->Remove(&listItem);
                
                delete listItem;
            }
     */
            delete princArray;
	}

	return NULL;
}

// ---------------------------------------------------------------------------
//		¥ SpendTime
// ---------------------------------------------------------------------------
// - use our periodic time to check to the state of the cache
// - set the fDirty flag accordingly
void CKrbSession::SpendTime(const EventRecord &inMacEvent) {

#pragma unused (inMacEvent)

/*
	cc_time_t cTime;
	cc_int32 cacheErr = ccNoError;
	
	// get last cache change time
	cacheErr = cc_context_get_change_time(mCCContext, &cTime);
	Assert_(cacheErr == ccNoError);
*/
	
	KLTime klTime;
	KLStatus klErr = klNoErr;
	
	// get last cache change time
	klErr = KLLastChangedTime(&klTime);
								
	if (klErr == klMemFullErr) {
		this->StopIdling();	// if we don't stop idling this error will come up a zillion times
		CKrbErrorAlert *alrt = new CKrbErrorAlert("Error reading configuration file, or out of memory.",
			"There was an error while trying to read the configuration file.  Either you don't have a configuration file (more likely), or you are out of memory (less likely).", "Quit", NULL, true);
		alrt->DisplayErrorAlert();
		delete alrt;
	} else if (klErr == KRB5_CONFIG_CANTOPEN) {
		this->StopIdling();
		CKrbErrorAlert *alrt = new CKrbErrorAlert("Cannot open a configuration file.",
		"Cannot open/find a Kerberos configuration file.  The Kerberos application cannot continue.", "OK", NULL, true);
		alrt->DisplayErrorAlert();
		delete alrt;
		
	} else {
		Assert_(klErr == klNoErr);
	}
	
	// do or-equals because it may have already been dirty when we came in to this procedure
	// a case of this is when default caches (active user) got swapped
	mCacheDirty |= (klTime != mLastChangeTime);
        
	mLastChangeTime = klTime;
}

// ---------------------------------------------------------------------------
//		¥ CCacheChanged
// ---------------------------------------------------------------------------
//accessor for mCacheDirty
bool CKrbSession::CCacheChanged() {

	return mCacheDirty;
}

// ---------------------------------------------------------------------------
//		¥ CCacheChanged
// ---------------------------------------------------------------------------
// allow setting of dirty bit, handy if you want to force an update of the cache
void CKrbSession::SetCacheDirty(Boolean inDirty) {
	mCacheDirty = inDirty;
}

// ---------------------------------------------------------------------------
//		¥ SetPrincipalAsDefault
// ---------------------------------------------------------------------------
void CKrbSession::SetPrincipalAsDefault(char *inPrincName) {
	UCCacheContext contextWrapper;
	UPrincipal::EVersion credCacheVersion;
	std::string princName, newCacheName;
	UInt32 cacheExpTime = 0;
	KLPrincipal tempPrinc = nil;
	KLStatus klErr = klNoErr;
	short cacheValidity = kTicketInvalidUnknown;

	Assert_(inPrincName != NULL);
	
	try {
		UCCache defaultCCache = contextWrapper.OpenDefaultCCache();

		UCCacheString defaultCCacheName = defaultCCache.GetName();
		
		// did we find it?
		if (FindPrincipalInCache(inPrincName, &newCacheName)) {
			// and it's different from the current one
			if (strcmp(newCacheName.c_str(), defaultCCacheName.CString()) ) {
				//try to open the new cache
				UCCache newCCache = contextWrapper.OpenCCache(newCacheName.c_str());

				//set new one to be default
				newCCache.SetDefault();
				
				credCacheVersion = newCCache.GetCredentialsVersion();

				if (credCacheVersion == UPrincipal::kerberosV4And5)
					credCacheVersion = UPrincipal::kerberosV5;
			    
			    princName = newCCache.GetPrincipal(credCacheVersion).GetString(credCacheVersion);
			    
				klErr = KLCreatePrincipalFromString(princName.c_str(), (credCacheVersion==UPrincipal::kerberosV5)?kerberosVersion_V5:kerberosVersion_V4, &tempPrinc);
				if (klErr == klNoErr) {
					klErr = KLTicketExpirationTime(tempPrinc, kerberosVersion_All, (KLTime *)&cacheExpTime);
					// check for srvtab case - there might be only one kind of tix
					if (klErr == klNoCredentialsErr) {
						klErr = KLTicketExpirationTime(tempPrinc, kerberosVersion_Any, (KLTime *)&cacheExpTime);
					}
					cacheValidity = kTicketValid;
					
					if (klErr != klNoErr) {
						cacheExpTime = 0;
						if (klErr == klCredentialsBadAddressErr) {
							cacheValidity = kTicketInvalidBadAddress;
						} else if (klErr == klCredentialsNeedValidationErr) {
							cacheValidity = kTicketInvalidNeedsValidation;
						} else if (klErr == klPrincipalDoesNotExistErr) {
							throw klPrincipalDoesNotExistErr; // cache changed unexpected; get out
						} else if (klErr != klCredentialsExpiredErr) {
							cacheValidity = kTicketInvalidUnknown;
							
							char *klErrorString;
							KLGetErrorString(klErr, &klErrorString);
							SignalString_(klErrorString);
							KLDisposeString(klErrorString);
						}		
					}
					KLDisposePrincipal(tempPrinc);
					tempPrinc = nil;
				}
				
				SetActiveUserInfo(inPrincName, cacheExpTime, cacheValidity);
				
				//force an update
				mCacheDirty = true;
			}
		} else {
			SignalStringLiteral_("Attempted to make non-existant user the default.");
		}
	}
	catch (...) {
            if (tempPrinc != nil)
                KLDisposePrincipal(tempPrinc);
	}
	
	//done
	return;	
	
}

// ---------------------------------------------------------------------------
//		¥ SetActiveUserInfo
// ---------------------------------------------------------------------------
// update name and expiration time for a (presumably) new active user
// pass in NULL for newFullPrinc to indicate no active user
void CKrbSession::SetActiveUserInfo(const char *newFullPrinc, UInt32 newExpTime, short cacheValidity) {

	char *temp, temp2[256];

	Assert_((newFullPrinc == NULL) || (strlen(newFullPrinc) < 256));
	
	// this code assumes that there won't be more than one @ sign in the principal
	// this may not be the case - fix me or notate
	
	if (newFullPrinc != NULL) {
		// truncate full princ string if longer than 256 chars - this will probably
		// lead to some sort of misbehavior, but it's better than scribbling over
		// memory
		strncpy(temp2, newFullPrinc, 255);
		temp2[255] = '\0';
		temp = strrchr(temp2, '@');
		strcpy((char *)mActiveUserRealm, temp+1);
		LString::CToPStr((char *)mActiveUserRealm);
		*temp = 0; //terminate at @
		strcpy((char *)mActiveUserPrincipal, temp2);
		LString::CToPStr((char *)mActiveUserPrincipal);
	} else {
		LString::CopyPStr(kNoActiveUserUserString, mActiveUserPrincipal);
		LString::CopyPStr(kNoActiveUserRealmString, mActiveUserRealm);
	}
	
	mActiveUserExpTime = newExpTime;
	mActiveUserValidity = cacheValidity;
}

// ---------------------------------------------------------------------------
//		¥ GetActiveUserInfo
// ---------------------------------------------------------------------------
void CKrbSession::GetActiveUserInfo(Str255 outPrincipal, long &outTimeTillExp, Str255 outRealm, short &outValidity) {

 int sec;
 
 	/* if the cache has changed, the active user may have been changed by some
 	   other program (such as Login Library or Control Strip), so we should check
 	   for this and compensate. */
 	/* SetInitialActiveUser() does exactly what we want, so just call it */
 	if (mCacheDirty) {
 		this->SetActiveUserFromCache();
 	}
 	
	LString::CopyPStr(mActiveUserRealm, outRealm);	 
	LString::CopyPStr(mActiveUserPrincipal, outPrincipal);

	// we will assume mActiveUserExpTime is in local time (not kdc time), but it's up
	// to the functions that set it to make sure that this is so
	struct timeval currentTime;
	
	gettimeofday(&currentTime, nil);
	sec = currentTime.tv_sec;

	outTimeTillExp = (long)mActiveUserExpTime - sec;
	
	outValidity = mActiveUserValidity;
}

// ---------------------------------------------------------------------------
//		¥ FindPrincipalInCache
// ---------------------------------------------------------------------------
// -check the cache to see if a principal is in it
//  if it is, return true and put the name of the cache in *outCacheName if **outCacheName is non-NULL
//  (outCacheName should be freed by the caller)
//  if not found, return false and leave outCacheName unchanged
bool CKrbSession::FindPrincipalInCache(char *inPrincName, std::string *outCacheName) {
	
	UCCacheContext contextWrapper;
	UCCache currentCCache;
	UPrincipal::EVersion credCacheVersion;
	
	bool princFound = false;
	
	Assert_(inPrincName != NULL);
	
	try {
		UCCacheIterator ccacheIterator = contextWrapper.NewCCacheIterator();
		
		// iterate through the cache looking for this principal
		while (ccacheIterator.Next(currentCCache)) {
			credCacheVersion = currentCCache.GetCredentialsVersion();
			
			UCCacheString currentCCacheName = currentCCache.GetName();
				
			if (credCacheVersion == UPrincipal::kerberosV4And5)
				credCacheVersion = UPrincipal::kerberosV5;
			
			UPrincipal comparePrinc = currentCCache.GetPrincipal(credCacheVersion);
			UPrincipal inPrinc(credCacheVersion,inPrincName);
				
			// compare the two principals
			if (inPrinc == comparePrinc) {
				princFound = true;
				if (outCacheName != nil) {
					*outCacheName = currentCCacheName.CString();
				}
			} // princ compare

			if (princFound)
				break;
		} // while
	}
	catch (...) {
		// do nothing, we'll return false
	}
	
	return princFound;
}

// ---------------------------------------------------------------------------
//		¥ ChangePassword
// ---------------------------------------------------------------------------
// -prompt the user name as princ for old and new password (twice), get password changing tickets
// -change the password
void  CKrbSession::ChangePassword(Str255 inPrinc, cc_int32 inPrincVersion) {

	KLPrincipal cpPrinc = nil;
	KLStatus klErr = klNoErr;
	
	LString::PToCStr(inPrinc);
	
	// convert cc kerb version to right kll kerb version, it is in v5 format
	if ( (inPrincVersion == cc_credentials_v4_v5) || (inPrincVersion == cc_credentials_v5))
		inPrincVersion = kerberosVersion_V5;
	else
		inPrincVersion = kerberosVersion_V4;
		
	klErr = KLCreatePrincipalFromString((char *)inPrinc, (UInt32)inPrincVersion, &cpPrinc);
	if (klErr == klNoErr) {
		klErr = KLChangePassword(cpPrinc);

		if ((klErr != klNoErr) && (klErr != klUserCanceledErr)) {
			char *klErrorString;
			
			KLGetErrorString(klErr, &klErrorString);
			CKrbErrorAlert *alrt = new CKrbErrorAlert("An error occurred while changing the password.",
				klErrorString,
				"OK", NULL, false);
			alrt->DisplayErrorAlert();
			delete alrt;
			
			KLDisposeString(klErrorString);
		}
		
		KLDisposePrincipal(cpPrinc);
	} else {
		Assert_(klErr == klNoErr);
	}
}

