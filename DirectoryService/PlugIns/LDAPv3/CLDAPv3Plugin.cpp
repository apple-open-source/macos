/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 * @header CLDAPv3Plugin
 * LDAP v3 plugin implementation to interface with Directory Services.
 */

#include <string.h>		//used for strcpy, etc.
#include <stdlib.h>		//used for malloc
#include <ctype.h>		//use for isprint
#include <openssl/md5.h>
#include <syslog.h>

#include <Security/Authorization.h>
#include <SystemConfiguration/SCDynamicStoreCopyDHCPInfo.h>

#include "DirServices.h"
#include "DirServicesUtils.h"
#include "DirServicesConst.h"
extern "C" {
	#include "saslutil.h"
};

#include "CLDAPv3Plugin.h"

using namespace std;

#include "ServerModuleLib.h"
#include "CRCCalc.h"
#include "CPlugInRef.h"
#include "CContinue.h"
#include "DSCThread.h"
#include "DSEventSemaphore.h"
#include "CSharedData.h"
#include "DSUtils.h"
#include "PrivateTypes.h"
#include "CLog.h"


#define LDAPURLOPT95SEPCHAR			" "
#define LDAPCOMEXPSPECIALCHARS		"()|&"

static void DoLDAPPINetworkChange(CFRunLoopTimerRef timer, void *info);
void DoLDAPPINetworkChange(CFRunLoopTimerRef timer, void *info)
{
	if ( info != nil )
	{
		((CLDAPv3Plugin *)info)->ReInitForNetworkTransition();
	}
}// DoLDAPPINetworkChange


CFStringRef NetworkChangeLDAPPICopyStringCallback( const void *item );
CFStringRef NetworkChangeLDAPPICopyStringCallback( const void *item )
{
	return CFSTR("NetworkChangeLDAPPlugin");
}

//TODO KW the AuthAuthority definitions need to come from DirectoryServiceCore
typedef struct LDAPv3AuthAuthorityHandler {
	char* fTag;
	LDAPv3AuthAuthorityHandlerProc fHandler;
} LDAPv3AuthAuthorityHandler;

#define		kLDAPv3AuthAuthorityHandlerProcs		2

static LDAPv3AuthAuthorityHandler sLDAPv3AuthAuthorityHandlerProcs[ kLDAPv3AuthAuthorityHandlerProcs ] =
{
	{ kDSTagAuthAuthorityBasic,				(LDAPv3AuthAuthorityHandlerProc)CLDAPv3Plugin::DoBasicAuth },
	{ kDSTagAuthAuthorityPasswordServer,	(LDAPv3AuthAuthorityHandlerProc)CLDAPv3Plugin::DoPasswordServerAuth }
};


// --------------------------------------------------------------------------------
//	Globals

CContinue				   *gLDAPContinueTable	= nil;
CPlugInRef				   *gLDAPContextTable	= nil;
CPlugInRef				   *gLDAPConfigTable	= nil; //TODO need STL type for config table instead
uInt32						gLDAPConfigTableLen	= 0;
static DSEventSemaphore	   *gKickLDAPv3Requests	= nil;
CLDAPv3Plugin			   *gLDAPv3Plugin		= nil;

void DSDoSearch (	char			   *inRecType,
					char			   *inNativeRecType,
					CAttributeList	   *inAttrTypeList,
					sLDAPContextData   *inContext,
					sLDAPContinueData  *inContinue,
					CLDAPv3Configs	   *inConfigFromXML,
					ber_int_t			inScope,
					CLDAPNode		   &inLDAPSessionMgr,
					char			   *inQueryFilter,
					int				   &outLDAPReturnCode,
					int					inSearchTO,
					LDAPMessage		  *&outResult);

sInt32 DSDoRetrieval (	char			   *inNativeRecType,
						char			  **inAttrs,
						sLDAPContextData   *inContext,
						ber_int_t			inScope,
						CLDAPNode		   &inLDAPSessionMgr,
						char			   *inQueryFilter,
						int				   &outLDAPReturnCode,
						int					inSearchTO,
						LDAPMessage		  *&outResult,
						bool			   &outResultFound,
						CLDAPv3Configs     *inConfigFromXML);

sInt32 DSSearchLDAP (	CLDAPNode		   &inLDAPSessionMgr,
						sLDAPContextData   *inContext,
						char			   *inNativeRecType,
						int					scope,
						char			   *queryFilter,
						char			  **attrs,
						int				   &ldapMsgId,
                                                int				   &ldapReturnCode,
                                                int                         recLimit);
sInt32 DSSearchLDAP (	CLDAPNode		   &inLDAPSessionMgr,
						sLDAPContextData   *inContext,
						char			   *inNativeRecType,
						int					scope,
						char			   *queryFilter,
						char			  **attrs,
						int				   &ldapMsgId,
						int				   &ldapReturnCode,
                                                int                         recLimit )
{
	sInt32			siResult 			= eDSNoErr;
	LDAPControl	  **serverctrls			= nil;
	LDAPControl	  **clientctrls			= nil;
	LDAP		   *aHost				= nil;
	
	aHost = inLDAPSessionMgr.LockSession(inContext);
	if (aHost != nil)
    {
		ldapReturnCode = ldap_search_ext(	aHost,
											inNativeRecType,
											scope,
											queryFilter,
											attrs,
											0,
											serverctrls,
											clientctrls,
											0, recLimit, 
											&ldapMsgId );
    }
    else
    {
        ldapReturnCode = LDAP_LOCAL_ERROR;
    }
	switch(ldapReturnCode)
	{
		case LDAP_SUCCESS:
			break;
		case LDAP_UNAVAILABLE:
		case LDAP_SERVER_DOWN:
		case LDAP_BUSY:
		case LDAP_LOCAL_ERROR:
		//case LDAP_TIMEOUT:
			siResult = eDSCannotAccessSession;
			break;
		default:
			//nothing found? - continue data msgId not set
			siResult = eDSRecordNotFound;
			break;
	}
	
	// CheckFailed thread requires that we not flag the connection as bad, otherwise we will not
	// reconnect if necessary now.  If the reconnect fails, it will get flagged as bad then.
	inLDAPSessionMgr.UnLockSession(inContext, false, false);
	
	if (serverctrls)  ldap_controls_free( serverctrls );
	if (clientctrls)  ldap_controls_free( clientctrls );

	return(siResult);
} // DSSearchLDAP

sInt32 DSGetSearchLDAPResult (	CLDAPNode		   &inLDAPSessionMgr,
								sLDAPContextData   *inContext,
								int					inSearchTO,
								LDAPMessage		  *&inResult,
								int					all,
								int				   &inLDAPMsgId,
								int				   &inLDAPReturnCode,
								bool				bErrorOnNoEntry);
sInt32 DSGetSearchLDAPResult (	CLDAPNode		   &inLDAPSessionMgr,
								sLDAPContextData   *inContext,
								int					inSearchTO,
								LDAPMessage		  *&inResult,
								int					all,
								int				   &inLDAPMsgId,
								int				   &inLDAPReturnCode,
								bool				bErrorOnNoEntry)
{
	sInt32			siResult 			= eDSNoErr;
	LDAP		   *aHost				= nil;

	aHost = inLDAPSessionMgr.LockSession(inContext);
	if (aHost != nil)
	{
		struct	timeval	tv;
		tv.tv_usec	= 0;
		if (inSearchTO == 0)
		{
			tv.tv_sec	= kLDAPDefaultSearchTimeoutInSeconds;
		}
		else
		{
			tv.tv_sec	= inSearchTO;
		}
		inLDAPReturnCode = ldap_result(aHost, inLDAPMsgId, all, &tv, &inResult);
		
		if ( inLDAPReturnCode == 0 )
		{
			// timed out, let's forget it
			ldap_abandon(aHost, inLDAPMsgId);
			inLDAPReturnCode = -1;
		}
	}
	else
	{
		inLDAPReturnCode = LDAP_LOCAL_ERROR;
	}
	
	// CheckFailed thread requires that we not flag the connection as bad, otherwise we will not
	// reconnect if necessary now.  If the reconnect fails, it will get flagged as bad then.
	inLDAPSessionMgr.UnLockSession(inContext, false, false);
	
	switch(inLDAPReturnCode)
	{
		case LDAP_RES_SEARCH_ENTRY:
			break;
		case LDAP_RES_SEARCH_RESULT:
			{
				int				ldapReturnCode2 = 0;
				int 			err				= 0;
				char		   *matcheddn		= nil;
				char		   *text			= nil;
				char		  **refs			= nil;
				LDAPControl	  **ctrls			= nil;
				aHost = inLDAPSessionMgr.LockSession(inContext);
				if (aHost != nil)
				{
					ldapReturnCode2 = ldap_parse_result(aHost, inResult, &err, &matcheddn, &text, &refs, &ctrls, 0);
				}
				else
				{
					ldapReturnCode2 = LDAP_LOCAL_ERROR;
				}
				inLDAPSessionMgr.UnLockSession(inContext);
				if (ldapReturnCode2 == LDAP_LOCAL_ERROR)
				{
					break;
				}
				else
				{
					if ( (ldapReturnCode2 != LDAP_SUCCESS) && (ldapReturnCode2 != LDAP_MORE_RESULTS_TO_RETURN) )
					{
						aHost = inLDAPSessionMgr.LockSession(inContext);
						ldap_abandon(aHost, inLDAPMsgId);
						inLDAPSessionMgr.UnLockSession(inContext);
					}
					else
					{
						if (ldapReturnCode2 == LDAP_SUCCESS) inLDAPReturnCode = LDAP_SUCCESS;
						if (text)  ber_memfree( text );
						if (matcheddn)  ber_memfree( matcheddn );
						if (refs)  ber_memvfree( (void **) refs );
						if (ctrls)  ldap_controls_free( ctrls );
					}
				}
				//no need to set siResult unless bErrorOnNoEntry
				//since some calls can't handle the return of LDAP_RES_SEARCH_RESULT
				if (bErrorOnNoEntry)
				{
					siResult = eDSRecordNotFound;
				}
				break;
			}
		case LDAP_TIMEOUT:
			siResult = eDSServerTimeout;
			break;
		case -1:
			siResult = eDSCannotAccessSession;
			break;
		default:
			//nothing found? even with LDAP_SUCCESS
			siResult = eDSRecordNotFound;
			break;
	} //switch(ldapReturnCode)

	return (siResult);
}

void DSGetExtendedLDAPResult (	LDAP			   *inHost,
								int					inSearchTO,
								int				   &inLDAPMsgId,
								int				   &inLDAPReturnCode);
void DSGetExtendedLDAPResult (	LDAP			   *inHost,
								int					inSearchTO,
								int				   &inLDAPMsgId,
								int				   &inLDAPReturnCode)
{
    LDAPMessage *res = NULL;
	struct	timeval	tv;
	tv.tv_usec	= 0;
	if (inSearchTO == 0)
	{
		tv.tv_sec	= kLDAPDefaultSearchTimeoutInSeconds;
	}
	else
	{
		tv.tv_sec	= inSearchTO;
	}
	inLDAPReturnCode = ldap_result(inHost, inLDAPMsgId, LDAP_MSG_ALL, &tv, &res);
	
	switch(inLDAPReturnCode)
	{
		case LDAP_RES_SEARCH_ENTRY:
			break;
		case LDAP_RES_EXTENDED:
		case LDAP_RES_SEARCH_RESULT:
			{
				int				ldapReturnCode2 = 0;
				int 			err				= 0;
				char		   *matcheddn		= nil;
				char		   *text			= nil;
				char		  **refs			= nil;
				LDAPControl	  **ctrls			= nil;
				ldapReturnCode2 = ldap_parse_result(inHost, res, &err, &matcheddn, &text, &refs, &ctrls, 0);
				if ( (ldapReturnCode2 != LDAP_SUCCESS) && (ldapReturnCode2 != LDAP_MORE_RESULTS_TO_RETURN) )
				{
					ldap_abandon(inHost, inLDAPMsgId);
                    inLDAPReturnCode = ldapReturnCode2;
				}
				else
				{
					inLDAPReturnCode = err;
					if (text && *text)  ber_memfree( text );
					if (matcheddn && *matcheddn)  ber_memfree( matcheddn );
					if (refs)  ber_memvfree( (void **) refs );
					if (ctrls)  ldap_controls_free( ctrls );
				}
			}
			break;
		case LDAP_TIMEOUT:
		case LDAP_SUCCESS:
		case -1:
		default:
            // the inReturnCode is passed back to the caller in this case
            break;
	} //switch(ldapReturnCode)
}

static int standard_password_create( LDAP *ld, char *dn, char *newPwd );
static int standard_password_create( LDAP *ld, char *dn, char *newPwd )
{
		// this modification removes all existing passwords for the user, we have to
		// do this because we have no idea which old password to replace and we cannot
		// do a LDAP_MOD_REPLACE for the same reason.
		LDAPMod modRemove = {};
		modRemove.mod_op = LDAP_MOD_DELETE;
		modRemove.mod_type = "userPassword";
		modRemove.mod_values = NULL;

		char *pwdCreate[2] = {};
		pwdCreate[0] = newPwd;
		pwdCreate[1] = NULL;
		LDAPMod modAdd = {};
		modAdd.mod_op = LDAP_MOD_ADD;
		modAdd.mod_type = "userPassword";
		modAdd.mod_values = pwdCreate;
		
		LDAPMod *mods[3] = {};
		mods[0] = &modRemove;
		mods[1] = &modAdd;
		mods[2] = NULL;

		return ldap_modify_s( ld, dn, mods );
}

static int exop_password_create( LDAP *ld, char *dn, char *newPwd );
static int exop_password_create( LDAP *ld, char *dn, char *newPwd )
{
		BerElement *ber = ber_alloc_t( LBER_USE_DER );
		if( ber == NULL ) throw( (sInt32) eMemoryError );
		ber_printf( ber, "{" /*}*/ );
        ber_printf( ber, "ts", LDAP_TAG_EXOP_MODIFY_PASSWD_ID, dn );
        ber_printf( ber, "ts", LDAP_TAG_EXOP_MODIFY_PASSWD_NEW, newPwd );
 		ber_printf( ber, /*{*/ "N}" );

		struct berval *bv = NULL;
		int rc = ber_flatten( ber, &bv );

		if( rc < 0 ) throw( (sInt32) eMemoryError );

		ber_free( ber, 1 );

        int id;
        rc = ldap_extended_operation( ld, LDAP_EXOP_MODIFY_PASSWD, bv, NULL, NULL, &id );
    
        ber_bvfree( bv );

		if ( rc == LDAP_SUCCESS ) {
				DSGetExtendedLDAPResult( ld, 0, id, rc);
		}

		return rc;
}


// --------------------------------------------------------------------------------
//	* CLDAPv3Plugin ()
// --------------------------------------------------------------------------------

CLDAPv3Plugin::CLDAPv3Plugin ( FourCharCode inSig, const char *inName ) : CServerPlugin(inSig, inName)
{
	
	fState					= kUnknownState;
	fState += kInactive;
	bDoNotInitTwiceAtStartUp= true;
    //ensure that the configXML is nil before initialization
    pConfigFromXML			= nil;
	fDHCPLDAPServersString	= NULL;
	fServerRunLoop			= nil;  //could be obtained directly now since a module not plugin
	fTimeToHandleNetworkTransition = 0;
	fInitFlag				= false;
    
	gLDAPv3Plugin = this;

    if ( gLDAPContinueTable == nil )
    {
        gLDAPContinueTable = new CContinue( CLDAPv3Plugin::ContinueDeallocProc );
    }

    //KW need to pass in a DeleteConfigData method instead of nil
    if ( gLDAPConfigTable == nil )
    {
        gLDAPConfigTable = new CPlugInRef( nil ); //TODO KW make this a STL map class instead
    }
    
    if ( gLDAPContextTable == nil )
    {
        gLDAPContextTable = new CPlugInRef( CLDAPv3Plugin::ContextDeallocProc );
    }

	if ( gKickLDAPv3Requests == nil )
	{
		gKickLDAPv3Requests = new DSEventSemaphore();
	}

} // CLDAPv3Plugin


// --------------------------------------------------------------------------------
//	* ~CLDAPv3Plugin ()
// --------------------------------------------------------------------------------

CLDAPv3Plugin::~CLDAPv3Plugin ( void )
{
/*TODO
    sLDAPContextData	   *pContext	= nil;
    uInt32				iTableIndex	= 0;

    // clean up the gLDAPContextTable here
    // not only the table but any dangling references in the table as well
    //KW this will not get the linked pointer entries
    //KW need to have the CPlugInRef class provide a destructor that cleans up everything
    //KW set up a DeleteContextData method that calls the three things below before RemoveItem
    for (iTableIndex=0; iTableIndex<CPlugInRef::kTableSize; iTableIndex++)
    {
        pContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( iTableIndex );
        gLDAPContextTable->RemoveItem( iTableIndex );
    }
    if ( gLDAPContextTable != nil)
    {
        delete ( gLDAPContextTable );
        gLDAPContextTable = nil;
    }
*/
	//cleanup the mappings and config data here
	//this will clean up the following:
	// 1) gLDAPConfigTable
	// 2) gLDAPConfigTableLen
    if ( pConfigFromXML != nil)
    {
        delete ( pConfigFromXML );
        pConfigFromXML			= nil;
        gLDAPConfigTable			= nil;
        gLDAPConfigTableLen			= 0;
    }

    //no need to clean this up since only one and it exists for the life of this process
    //ie. don't want a race condition on shutdown where it may be in use on another thread
    //if ( gLDAPContinueTable != nil)
    //{
        //delete ( gLDAPContinueTable );
        //gLDAPContinueTable = nil;
    //}
    
    //KW ensure the release of all LDAP session handles eventually
    //but probably NOT through CleanContextData since multiple contexts will
    //have the same session handle
	//TODO is this true?
    
} // ~CLDAPv3Plugin


// --------------------------------------------------------------------------------
//	* Validate ()
// --------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::Validate ( const char *inVersionStr, const uInt32 inSignature )
{
	fPlugInSignature = inSignature;

	return( noErr );

} // Validate


// --------------------------------------------------------------------------------
//	* Initialize ()
// --------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::Initialize ( void )
{
    int					countNodes	= 0;
    sInt32				siResult	= eDSNoErr;
    tDataList		   *pldapName	= nil;
    sLDAPConfigData	   *pConfig		= nil;
    uInt32				iTableIndex	= 0;
	char			   *server		= nil;
	char			   *searchBase	= nil;
	int					portNumber	= 389;
	bool				bIsSSL		= false;
   
	fCheckInitFlag.Wait();
	fInitFlag = true;
	fCheckInitFlag.Signal();
   
    try
	{
	    if ( pConfigFromXML == nil )
	    {
	        pConfigFromXML = new CLDAPv3Configs();
            if ( pConfigFromXML  == nil ) throw( (sInt32)eDSOpenNodeFailed ); //KW need an eDSPlugInConfigFileError
	    }
		
	    siResult = pConfigFromXML->Init( gLDAPConfigTable, gLDAPConfigTableLen );
	    if ( siResult != eDSNoErr ) throw( siResult );
		
		//duplicate node in local config file overrides any DHCP obtained config
		
		//our DHCP obtained node has its config as part of the gLDAPConfigTable
		//we register it as the node it really is
		//check if DHCP has changed - if so then we re-discover the nodes else we simply mark the bUpdated flag true
		//go get the DHCP nodes
		CFDictionaryRef		ourDHCPInfo			= NULL;
		CFStringRef			ourDHCPServers		= NULL;
		CFDataRef			ourDHCPServersData	= NULL;
		UInt8			   *byteData			= NULL;
		bool				bNewDHCPConfig		= false;
		
		ourDHCPInfo = SCDynamicStoreCopyDHCPInfo(NULL, NULL);
		if (ourDHCPInfo != NULL)
		{
			ourDHCPServersData = (CFDataRef) DHCPInfoGetOptionData(ourDHCPInfo, 95);
			if (ourDHCPServersData != NULL)
			{
				byteData = (UInt8 *) calloc(1, CFDataGetLength(ourDHCPServersData));
				if (byteData != NULL)
				{
					CFDataGetBytes(ourDHCPServersData, CFRangeMake(	0,
																	CFDataGetLength(ourDHCPServersData)),
																	byteData);
					if (byteData[0] != '\0')
					{
						ourDHCPServers = CFStringCreateWithBytes(	kCFAllocatorDefault,
																	byteData,
																	CFDataGetLength(ourDHCPServersData),
																	kCFStringEncodingUTF8,
																	false);
					}
					free(byteData);
				} // byteData not NULL
				//CFRelease(ourDHCPServersData); don't release since retrieved with Get
			}
			if (ourDHCPServers != NULL)
			{
				if ( CFGetTypeID( ourDHCPServers ) == CFStringGetTypeID() )
				{
					if (( fDHCPLDAPServersString == NULL) || (CFStringCompare(fDHCPLDAPServersString, ourDHCPServers, 0) != kCFCompareEqualTo))
					{
						if (fDHCPLDAPServersString != NULL)
						{
							CFRelease(fDHCPLDAPServersString);
						}
						fDHCPLDAPServersString = ourDHCPServers;
						bNewDHCPConfig = true;  // set this so we can do a System Config change notification...
					} // if (CFStringCompare(fDHCPLDAPServersString, ourDHCPServers, 0) != kCFCompareEqualTo)
					else
					{
						CFRelease(ourDHCPServers);
						ourDHCPServers = nil;
					}

					//always update the config table
					//need to parse the fDHCPLDAPServersString string
					int serverIndex = 1;
					while( ParseNextDHCPLDAPServerString(&server, &searchBase, &portNumber, &bIsSSL, serverIndex) )
					{
						sInt32 resultAddingDHCPLDAP = pConfigFromXML->MakeServerBasedMappingsLDAPConfig( server, searchBase, 15, 2, 120, 120, portNumber, bIsSSL, true);

						if (resultAddingDHCPLDAP == eDSNoErr)
						{
							gLDAPConfigTableLen++; //only other place this counter is synced up ie. other place is call to pConfigFromXML->Init
						} 

						if (server != nil)
						{
							free(server);
							server = nil;
						}
						if (searchBase != nil)
						{
							free(searchBase);
							searchBase = nil;
						}
						portNumber	= 389;
						bIsSSL		= false;
						serverIndex++;
					}
				} // if ( CFGetTypeID( ourDHCPServers ) == CFStringGetTypeID() )
			} // if (ourDHCPServers != NULL)
			else
			{
				if (fDHCPLDAPServersString != nil)
				{
					CFRelease(fDHCPLDAPServersString);
					fDHCPLDAPServersString = nil;
				}
			}
			CFRelease(ourDHCPInfo);
		} // if (ourDHCPInfo != NULL)
		else
		{
			if (fDHCPLDAPServersString != nil)
			{
				CFRelease(fDHCPLDAPServersString);
				fDHCPLDAPServersString = nil;
			}
		}
		
	    //Cycle through the gLDAPConfigTable
	    //skip the first "generic unknown" configuration ie. nothing to register so start at 1 not 0
	    for (iTableIndex=1; iTableIndex<gLDAPConfigTableLen; iTableIndex++)
	    {
	        pConfig = (sLDAPConfigData *)gLDAPConfigTable->GetItemData( iTableIndex );
	        if (pConfig != nil)
	        {
				if (pConfig->fServerName != nil)
				{
					if (pConfig->bUpdated)
					{
						//allow register of nodes that have NOT been verified by ldap_init calls
						countNodes++;
						pConfig->bAvail = true;
						//add standard LDAPv3 prefix to the registered node names here
						pldapName = dsBuildListFromStringsPriv((char *)"LDAPv3", pConfig->fServerName, nil);
						if (pldapName != nil)
						{
							//same node does not get registered twice
							CServerPlugin::_RegisterNode( fPlugInSignature, pldapName, kDirNodeType );
							dsDataListDeallocatePriv( pldapName);
							free(pldapName);
							pldapName = nil;
						}
						if (pConfig->bServerMappings)
						{
							//reset to get server mappings again if there was update here
							pConfig->bGetServerMappings = true;
						}
						pConfig->bBuildReplicaList = true; //always reset this
					} // Config has been updated OR is new so register the node
					else
					{
						//UN register the node
						//and remove it from the config table
						//but DO NOT decrement the gLDAPConfigTableLen counter since we allow empty entries
						
		                //add standard LDAPv3 prefix to the registered node names here
		                pldapName = dsBuildListFromStringsPriv((char *)"LDAPv3", pConfig->fServerName, nil);
		                if (pldapName != nil)
		                {
							//KW what happens when the same node is unregistered twice???
	                        CServerPlugin::_UnregisterNode( fPlugInSignature, pldapName );
	                    	dsDataListDeallocatePriv( pldapName);
							free(pldapName);
	                    	pldapName = nil;
		                }
						//ensure here that we close out all existing connections for this config
						fLDAPSessionMgr.ForcedSafeClose(pConfig->fServerName);
						pConfigFromXML->CleanLDAPConfigData( pConfig );
						// delete the sLDAPConfigData itself
						delete( pConfig );
						pConfig = nil;
						// remove the table entry
						gLDAPConfigTable->RemoveItem( iTableIndex );
					}
				} //if servername defined
//KW don't throw anything here since we want to go on and get the others
//				if (pConfig->fServerName == nil ) throw( (sInt32)eDSNullParameter);  //KW might want a specific err
	        } // pConfig != nil
	    } // loop over the LDAP config entries

        //need to wait for all nodes to be registered before setting dummy DHCP since
        //don't know which nodes may be DHCP either from DHCP directly or as a configured facade
        //make sure that above in initialization that server mappings are not yet retrieved
        //KW can we optimize this to be called earlier?
        //simply register a dummy DHCP node to let the daemon know that DHCP has been checked
        //note this really doesn't get registered
        pldapName = dsBuildListFromStringsPriv((char *)"LDAPv3", "DHCPChecked", nil);
        if (pldapName != nil)
        {
            CServerPlugin::_RegisterNode( fPlugInSignature, pldapName, kDHCPLDAPv3NodeType );
            dsDataListDeallocatePriv( pldapName);
            free(pldapName);
            pldapName = nil;
        }
        
		// set the initialization flags
		if (!(fState & kInitialized))
		{
			fState += kInitialized;
		}
		if (fState & kFailedToInit)
		{
			fState -= kFailedToInit;
		}
        
        WakeUpRequests();

		// now that we are done, let's notify the SearchPlugin or anyone else that we have updated based on DHCP
		if( bNewDHCPConfig )
		{
			CFStringRef service = CFStringCreateWithCString( NULL, "DirectoryService", kCFStringEncodingUTF8 );
			
			if( service )
			{
				SCDynamicStoreRef   store = SCDynamicStoreCreate(NULL, service, NULL, NULL);
				
				if (store != NULL)
				{   // we don't have to change it we can just cause a notify....
					CFStringRef notify = CFStringCreateWithCString( NULL, "com.apple.DirectoryService.NotifyTypeStandard:DHCP_LDAP_CHANGE", kCFStringEncodingUTF8 );
					
					if( notify ) {
						SCDynamicStoreNotifyValue( store, notify );
						CFRelease( notify );
					}
					CFRelease( store );
				}
				CFRelease( service );
			}
		}
		
	} // try
	catch ( sInt32 err )
	{
		siResult = err;
		// set the inactive and failedtoinit flags
		fState = kUnknownState;
		fState += kFailedToInit;
		fState += kInactive;
	}

	fCheckInitFlag.Wait();
	fInitFlag = false;
	fCheckInitFlag.Signal();
   
	return( siResult );

} // Initialize


//--------------------------------------------------------------------------------------------------
//	* ParseNextDHCPLDAPServerString()
//
//--------------------------------------------------------------------------------------------------

bool CLDAPv3Plugin::ParseNextDHCPLDAPServerString ( char **inServer, char **inSearchBase, int *inPortNumber, bool *inIsSSL, int inServerIndex )
{
	bool	foundNext		= false;
	char   *tmpBuff			= nil;
	char   *aString			= nil;
	uInt32	callocLength	= 0;
	char   *aToken			= nil;
	int		aIndex			= 1;
	uInt32	strLength		= 0;
	char   *ptr				= nil;
	
	if (fDHCPLDAPServersString == NULL)
	{
		return(foundNext);
	}
	
	//parse the fDHCPLDAPServersString string
	callocLength = (uInt32) CFStringGetMaximumSizeForEncoding(CFStringGetLength(fDHCPLDAPServersString), kCFStringEncodingUTF8) + 1;
	tmpBuff = (char *) calloc(1, callocLength);
	CFStringGetCString( fDHCPLDAPServersString, tmpBuff, callocLength, kCFStringEncodingUTF8 );
	
	//KW might want to add code to skip leading white space?
	aString = tmpBuff;
	aToken = strsep(&aString,LDAPURLOPT95SEPCHAR);
	if (inServerIndex > 1)
	{
		aIndex = inServerIndex;
		while (aIndex != 1)
		{
			aToken = strsep(&aString, LDAPURLOPT95SEPCHAR);
			aIndex--;
		}
	}
	
	if (aToken != nil)
	{
		*inServer		= nil;
		*inSearchBase	= nil;
		
		if ( strncmp(aToken,"ldap://",7) == 0 )
		{
			*inPortNumber	= 389;
			*inIsSSL		= false;
			aToken			= aToken + 7;
		}
		else if ( strncmp(aToken,"ldaps://",8) == 0 )
		{
			*inPortNumber	= 636;
			*inIsSSL		= true;
			aToken			= aToken + 8;
		}
		else
		{
			syslog(LOG_INFO,"DSLDAPv3PlugIn: DHCP option 95 error since obtained [%s] LDAP server prefix is not of the correct format.", aToken);
			if ( tmpBuff != nil )
			{
				free(tmpBuff);
				tmpBuff = nil;
			}
			return(foundNext);
		}
		
		ptr				= aToken;
		strLength		= 0;
		while ( (*ptr != '\0') && (*ptr != ':') && (*ptr != '/') )
		{
			strLength++;
			ptr++;
		}
		if (strLength != 0)
		{
			*inServer	= (char *) calloc(1, strLength+1);
			strncpy(*inServer, aToken, strLength);
			aToken		= aToken + strLength;
			foundNext	= true;
		}
		else
		{
			syslog(LOG_INFO,"DSLDAPv3PlugIn: DHCP option 95 error since can't extract LDAP server name from URL.");
			if ( tmpBuff != nil )
			{
				free(tmpBuff);
				tmpBuff = nil;
			}
			return(foundNext);
		}
		
		if (*ptr == ':')
		{
			ptr++;
			aToken++;
			strLength	= 0;
			while ( (*ptr != '\0') && (*ptr != '/') )
			{
				strLength++;
				ptr++;
			}
			if (strLength != 0)
			{
				char *portStr	= nil;
				portStr			= (char *) calloc(1, strLength+1);
				strncpy(portStr, aToken, strLength);
				*inPortNumber	= atoi(portStr);
				free(portStr);
				portStr			= nil;
				aToken			= aToken + strLength;
			}
		}
			
		if (*ptr == '/')
		{
			ptr++;
			aToken++;
			strLength	= 0;
			while ( (*ptr != '\0') && (*ptr != '?') )
			{
				strLength++;
				ptr++;
			}
			if (strLength != 0)
			{
				*inSearchBase	= (char *) calloc(1, strLength+1);
				strncpy(*inSearchBase, aToken, strLength);
				//let's deal with embedded spaces here ie. %20 versus ' '
				ptr = *inSearchBase;
				char *BasePtr = ptr;
				while (*ptr !=  '\0')
				{
					if (*ptr == '%')
					{
						ptr++;
						if (*ptr == '2')
						{
							ptr++;
							if (*ptr == '0')
							{
								*ptr = ' ';
							}
						}
					}
					*BasePtr = *ptr;
					ptr++;
					BasePtr++;
				}
				*BasePtr = *ptr; //add NULL to end of string with replaced spaces
			}
		}
	} // if (aToken != nil)

	if ( tmpBuff != nil )
	{
		free(tmpBuff);
		tmpBuff = nil;
	}
	
	return(foundNext);

} // ParseNextDHCPLDAPServerString


// --------------------------------------------------------------------------------
//	* SetPluginState ()
// --------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::SetPluginState ( const uInt32 inState )
{

    tDataList		   *pldapName	= nil;
    sLDAPConfigData	   *pConfig		= nil;
    uInt32				iTableIndex	= 0;

// don't allow any changes other than active / in-active

	if (inState & kActive) //want to set to active
	{
		if ( (fState & kInactive) && (!(fState & kActive)) )
		{
			//call to initialize again since we need to re-register the nodes and discover DHCP LDAP
			if (bDoNotInitTwiceAtStartUp)
			{
				bDoNotInitTwiceAtStartUp = false;
			}
			else
			{
				Initialize();
			}
			fState += kActive;
			fState -= kInactive;
		}
	}
	if (inState & kInactive) //want to set to in-active
	{
		if ( (fState & kActive) && (!(fState & kInactive)) )
		{
			//Cycle through the gLDAPConfigTable
			//skip the first "generic unknown" configuration ie. nothing to register so start at 1 not 0
			for (iTableIndex=1; iTableIndex<gLDAPConfigTableLen; iTableIndex++)
			{
				pConfig = (sLDAPConfigData *)gLDAPConfigTable->GetItemData( iTableIndex );
				if (pConfig != nil)
				{
					if (pConfig->fServerName != nil)
					{
						//UN register the node
						//but don't remove it from the config table
						
						//add standard LDAPv3 prefix to the registered node names here
						pldapName = dsBuildListFromStringsPriv((char *)"LDAPv3", pConfig->fServerName, nil);
						if (pldapName != nil)
						{
							//KW what happens when the same node is unregistered twice???
							CServerPlugin::_UnregisterNode( fPlugInSignature, pldapName );
							dsDataListDeallocatePriv( pldapName);
							free(pldapName);
							pldapName = nil;
						}
					} //if servername defined
	//KW don't throw anything here since we want to go on and get the others
	//				if (pConfig->fServerName == nil ) throw( (sInt32)eDSNullParameter);  //KW might want a specific err
				} // pConfig != nil
			} // loop over the LDAP config entries

            fState += kInactive;
            fState -= kActive;
		}
    }

	return( eDSNoErr );

} // SetPluginState


//--------------------------------------------------------------------------------------------------
//	* WakeUpRequests() (static)
//
//--------------------------------------------------------------------------------------------------

void CLDAPv3Plugin::WakeUpRequests ( void )
{
	gKickLDAPv3Requests->Signal();
} // WakeUpRequests


// ---------------------------------------------------------------------------
//	* WaitForInit
//
// ---------------------------------------------------------------------------

void CLDAPv3Plugin::WaitForInit ( void )
{
	volatile	uInt32		uiAttempts	= 0;

	if (!(fState & kActive))
	{
		while ( !(fState & kInitialized) &&
				!(fState & kFailedToInit) )
		{
			// Try for 2 minutes before giving up
			if ( uiAttempts++ >= 240 )
			{
				return;
			}
	
			// Now wait until we are told that there is work to do or
			//	we wake up on our own and we will look for ourselves
	
			gKickLDAPv3Requests->Wait( (uInt32)(.5 * kMilliSecsPerSec) );
	
			gKickLDAPv3Requests->Reset();
		}
	}//NOT already Active
} // WaitForInit


// ---------------------------------------------------------------------------
//	* ProcessRequest
//
// ---------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::ProcessRequest ( void *inData )
{
	sInt32		siResult	= 0;
	char	   *pathStr		= nil;

	if ( inData == nil )
	{
		return( ePlugInDataError );
	}
    
	if (((sHeader *)inData)->fType == kOpenDirNode)
	{
		if (((sOpenDirNode *)inData)->fInDirNodeName != nil)
		{
			pathStr = ::dsGetPathFromListPriv( ((sOpenDirNode *)inData)->fInDirNodeName, "/" );
			if (pathStr != nil)
			{
				if (strncmp(pathStr,"/LDAPv3",7) != 0)
				{
					free(pathStr);
					pathStr = nil;
					return(eDSOpenNodeFailed);
				}
				free(pathStr);
				pathStr = nil;
			}
		}
	}
	
	if ( ((sHeader *)inData)->fType == kServerRunLoop )
	{
		if ( (((sHeader *)inData)->fContextData) != nil )
		{
			fServerRunLoop = (CFRunLoopRef)(((sHeader *)inData)->fContextData);
			return (siResult);
		}
	}

    WaitForInit();

	if (fState == kUnknownState)
	{
		return( (sInt32)ePlugInCallTimedOut );
	}

	if ( (fState & kFailedToInit) || !(fState & kInitialized) )
	{
		return( (sInt32)ePlugInFailedToInitialize );
	}

	if ( ((fState & kInactive) || !(fState & kActive))
		  && (((sHeader *)inData)->fType != kDoPlugInCustomCall)
		  && (((sHeader *)inData)->fType != kOpenDirNode) )
	{
        return( ePlugInNotActive );
	}
    
	if ( ((sHeader *)inData)->fType == kHandleNetworkTransition )
	{
		HandleMultipleNetworkTransitionsForLDAP();
	}
	else
	{
		siResult = HandleRequest( inData );
	}

	return( siResult );

} // ProcessRequest



// ---------------------------------------------------------------------------
//	* HandleRequest
//
// ---------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::HandleRequest ( void *inData )
{
	sInt32				siResult	= 0;
	sHeader			   *pMsgHdr		= nil;
	sLDAPContextData   *pContext	= nil;
	char			   *recName		= nil; //needed for close/delete record idle connection counting since context removed after call completed
	bool				bWaitForInit= true;
	DSSemaphore			timedWait;

	//block new requests while a re-initialization is on-going
	while(bWaitForInit)
	{
		fCheckInitFlag.Wait();
		if (fInitFlag)
		{
			fCheckInitFlag.Signal();
			//wait one tenth of a second to check again
			timedWait.Wait( (uInt32)(.1 * kMilliSecsPerSec) );
		}
		else
		{
			fCheckInitFlag.Signal();
			bWaitForInit = false;
		}
	}

	if ( inData == nil )
	{
		return( -8088 );
	}

	pMsgHdr = (sHeader *)inData;

	//need to keep track of use of LDAP connections
	switch ( pMsgHdr->fType )
	{
		//first group all has the data structure start with
		//typedef struct {
		// uInt32				fType;
		// sInt32				fResult;
		// tDirNodeReference	fInNodeRef;
		//so using sOpenRecord cast
		case kGetRecordList:
		//case kGetDirNodeInfo: //not needed since no LDAP connection yet for this API call
		case kOpenRecord:
		case kCreateRecord:
		case kCreateRecordAndOpen:
		case kDoDirNodeAuth:
		case kDoDirNodeAuthOnRecordType:
		case kDoAttributeValueSearch:
		case kDoAttributeValueSearchWithData:
			if ( ((sOpenRecord *)inData)->fInNodeRef  != nil )
			{
				pContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( ((sOpenRecord *)inData)->fInNodeRef );
				if ( pContext  != nil )
				{
					if ( ( pContext->fName != nil ) && !(pContext->authCallActive) )
					{
						DBGLOG1( kLogPlugin, "CLDAPv3PlugIn::HandleRequest-ActiveConn-%d", pMsgHdr->fType );
						fLDAPSessionMgr.ActiveConnection(pContext->fName);
					}
				}
			}
			break;

		//second group all has the data structure start with
		//typedef struct {
		// uInt32				fType;
		// sInt32				fResult;
		// tDirNodeReference	fInRecRef;
		//so using sCloseRecord cast
		case kGetRecordAttributeInfo:
		case kGetRecordAttributeValueByIndex:
		case kGetRecordAttributeValueByID:
		case kSetRecordName:
		case kAddAttribute:
		case kRemoveAttribute:
		case kAddAttributeValue:
		case kRemoveAttributeValue:
		case kSetAttributeValue:
			if ( ((sCloseRecord *)inData)->fInRecRef  != nil )
			{
				pContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( ((sCloseRecord *)inData)->fInRecRef );
				if ( pContext  != nil )
				{
					if ( ( pContext->fName != nil ) && !(pContext->authCallActive) )
					{
						DBGLOG1( kLogPlugin, "CLDAPv3PlugIn::HandleRequest-ActiveConn-%d", pMsgHdr->fType );
						fLDAPSessionMgr.ActiveConnection(pContext->fName);
					}
				}
			}
			break;

		case kCloseRecord:
		case kDeleteRecord:
			if ( ((sCloseRecord *)inData)->fInRecRef  != nil )
			{
				pContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( ((sCloseRecord *)inData)->fInRecRef );
				if ( pContext  != nil )
				{
					if ( ( pContext->fName != nil ) && !(pContext->authCallActive) )
					{
						recName = strdup(pContext->fName);
						DBGLOG1( kLogPlugin, "CLDAPv3PlugIn::HandleRequest-ActiveConn-%d", pMsgHdr->fType );
						fLDAPSessionMgr.ActiveConnection(pContext->fName);
					}
				}
			}
			break;

		default:
			break;
	}

	switch ( pMsgHdr->fType )
	{
		case kOpenDirNode:
			siResult = OpenDirNode( (sOpenDirNode *)inData );
			break;
			
		case kCloseDirNode:
			siResult = CloseDirNode( (sCloseDirNode *)inData );
			break;
			
		case kGetDirNodeInfo:
			siResult = GetDirNodeInfo( (sGetDirNodeInfo *)inData );
			break;
			
		case kGetRecordList:
			siResult = GetRecordList( (sGetRecordList *)inData );
			break;
			
		case kGetRecordEntry:
            siResult = GetRecordEntry( (sGetRecordEntry *)inData );
			break;
			
		case kGetAttributeEntry:
			siResult = GetAttributeEntry( (sGetAttributeEntry *)inData );
			break;
			
		case kGetAttributeValue:
			siResult = GetAttributeValue( (sGetAttributeValue *)inData );
			break;
			
		case kOpenRecord:
			siResult = OpenRecord( (sOpenRecord *)inData );
			break;
			
		case kGetRecordReferenceInfo:
			siResult = GetRecRefInfo( (sGetRecRefInfo *)inData );
			break;
			
		case kGetRecordAttributeInfo:
			siResult = GetRecAttribInfo( (sGetRecAttribInfo *)inData );
			break;
			
		case kGetRecordAttributeValueByIndex:
			siResult = GetRecAttrValueByIndex( (sGetRecordAttributeValueByIndex *)inData );
			break;
			
		case kGetRecordAttributeValueByID:
			siResult = GetRecordAttributeValueByID( (sGetRecordAttributeValueByID *)inData );
			break;

		case kFlushRecord:
			siResult = FlushRecord( (sFlushRecord *)inData );
			break;
			
		case kCloseAttributeList:
			siResult = CloseAttributeList( (sCloseAttributeList *)inData );
			break;

		case kCloseAttributeValueList:
			siResult = CloseAttributeValueList( (sCloseAttributeValueList *)inData );
			break;

		case kCloseRecord:
			siResult = CloseRecord( (sCloseRecord *)inData );
			break;
			
		case kReleaseContinueData:
			siResult = ReleaseContinueData( (sReleaseContinueData *)inData );
			break;

		case kSetRecordName:
			siResult = SetRecordName( (sSetRecordName *)inData );
			break;
			
		case kSetRecordType:
			siResult = eNotYetImplemented; //KW not to be implemented
			break;
			
		case kDeleteRecord:
			siResult = DeleteRecord( (sDeleteRecord *)inData );
			break;

		case kCreateRecord:
		case kCreateRecordAndOpen:
			siResult = CreateRecord( (sCreateRecord *)inData );
			break;

		case kAddAttribute:
			siResult = AddAttribute( (sAddAttribute *)inData );
			break;

		case kRemoveAttribute:
			siResult = RemoveAttribute( (sRemoveAttribute *)inData );
			break;
			
		case kAddAttributeValue:
			siResult = AddAttributeValue( (sAddAttributeValue *)inData );
			break;
			
		case kRemoveAttributeValue:
			siResult = RemoveAttributeValue( (sRemoveAttributeValue *)inData );
			break;
			
		case kSetAttributeValue:
			siResult = SetAttributeValue( (sSetAttributeValue *)inData );
			break;
			
		case kDoDirNodeAuth:
			siResult = DoAuthentication( (sDoDirNodeAuth *)inData );
			break;
			
		case kDoDirNodeAuthOnRecordType:
			siResult = DoAuthenticationOnRecordType( (sDoDirNodeAuthOnRecordType *)inData );
			break;
			
		case kDoAttributeValueSearch:
		case kDoAttributeValueSearchWithData:
			siResult = DoAttributeValueSearch( (sDoAttrValueSearchWithData *)inData );
			break;

		case kDoPlugInCustomCall:
			siResult = DoPlugInCustomCall( (sDoPlugInCustomCall *)inData );
			break;
			
		default:
			siResult = eNotHandledByThisNode;
			break;
	}

	//need to keep track of use of LDAP connections
	switch ( pMsgHdr->fType )
	{
		case kGetRecordList:
		//case kGetDirNodeInfo: //not needed since no LDAP connection yet for this API call
		case kOpenRecord:
		case kCreateRecord:
		case kCreateRecordAndOpen:
		case kDoDirNodeAuth:
		case kDoDirNodeAuthOnRecordType:
		case kDoAttributeValueSearch:
		case kDoAttributeValueSearchWithData:
		case kGetRecordAttributeInfo:
		case kGetRecordAttributeValueByIndex:
		case kGetRecordAttributeValueByID:
		case kSetRecordName:
		case kAddAttribute:
		case kRemoveAttribute:
		case kAddAttributeValue:
		case kRemoveAttributeValue:
		case kSetAttributeValue:
			if ( pContext  != nil ) //should be garnered from above connection case statement
			{
				if ( ( pContext->fName != nil ) && !(pContext->authCallActive) )
				{
					DBGLOG1( kLogPlugin, "CLDAPv3PlugIn::HandleRequest-IdleConn-%d", pMsgHdr->fType );
					fLDAPSessionMgr.IdleConnection(pContext->fName);
				}
			}
			break;

		case kCloseRecord:
		case kDeleteRecord:
			if ( recName  != nil ) //should be garnered from above connection case statement
			{
				DBGLOG1( kLogPlugin, "CLDAPv3PlugIn::HandleRequest-IdleConn-%d", pMsgHdr->fType );
				fLDAPSessionMgr.IdleConnection(recName);
				free(recName);
				recName = nil;
			}
			break;

		default:
			break;
	}

	pMsgHdr->fResult = siResult;
	return( siResult );

} // HandleRequest

//------------------------------------------------------------------------------------
//	* OpenDirNode
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::OpenDirNode ( sOpenDirNode *inData )
{
    char			   *ldapName		= nil;
	char			   *pathStr			= nil;
    char			   *subStr			= nil;
	sLDAPContextData	   *pContext		= nil;
	sInt32				siResult		= eDSNoErr;
    tDataListPtr		pNodeList		= nil;

    pNodeList	=	inData->fInDirNodeName;
    
	try
	{
            if ( inData != nil )
            {
                pathStr = dsGetPathFromListPriv( pNodeList, (char *)"/" );
                if ( pathStr == nil ) throw( (sInt32)eDSNullNodeName );

				//special case for the configure LDAPv3 node
				if (::strcmp(pathStr,"/LDAPv3") == 0)
				{
					// set up the context data now with the relevant parameters for the configure LDAPv3 node
                	// DS API reference number is used to access the reference table
                	pContext = MakeContextData();
                	pContext->fHost = nil;
                	pContext->fName = new char[1+::strlen("LDAPv3 Configure")];
                	::strcpy(pContext->fName,"LDAPv3 Configure");
                	//generic hash index
                	pContext->fConfigTableIndex = 0;
                	// add the item to the reference table
					gLDAPContextTable->AddItem( inData->fOutNodeRef, pContext );
				}
                // check that there is something after the delimiter or prefix
                // strip off the LDAPv3 prefix here
                else if ( (strlen(pathStr) > 8) && (::strncmp(pathStr,"/LDAPv3/",8) == 0) )
                {
					subStr = pathStr + 8;

                    ldapName = (char *) calloc( 1, strlen(subStr) + 1 );
                    if ( ldapName == nil ) throw( (sInt32)eDSNullNodeName );
                    
                    ::strcpy(ldapName,subStr);
                	pContext = MakeContextData();
					pContext->fName = ldapName;
					pContext->fUID = inData->fInUID;
					pContext->fEffectiveUID = inData->fInEffectiveUID;
					
					siResult = fLDAPSessionMgr.SafeOpen( (char *)ldapName, &(pContext->fHost), &(pContext->fConfigTableIndex), pConfigFromXML );
					if ( siResult != eDSNoErr)
					{
						CleanContextData(pContext);
						free(pContext);
						pContext = nil;
						throw( (sInt32)eDSOpenNodeFailed );
					}
					
					//wipe out the LDAP handle from the context since it is retained in the node table
					pContext->fHost = nil;
					
                	// add the item to the reference table
					gLDAPContextTable->AddItem( inData->fOutNodeRef, pContext );
                } // there was some name passed in here ie. length > 1
                else
                {
                    siResult = eDSOpenNodeFailed;
                }

            } // inData != nil
	} // try
	catch ( sInt32 err )
	{
		siResult = err;
		if (pContext != nil)
		{
			gLDAPContextTable->RemoveItem( inData->fOutNodeRef );
		}
	}
	
	if (pathStr != nil)
	{
		delete( pathStr );
		pathStr = nil;
	}

	return( siResult );

} // OpenDirNode

//------------------------------------------------------------------------------------
//	* CloseDirNode
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::CloseDirNode ( sCloseDirNode *inData )
{
	sInt32				siResult	= eDSNoErr;
	sLDAPContextData   *pContext	= nil;

	try
	{
		pContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInNodeRef );
		if ( pContext  == nil ) throw( (sInt32)eDSBadContextData );

		if ( pContext->authCallActive )
		{
			siResult = fLDAPSessionMgr.SafeClose(pContext->fName, pContext->fHost);
		}
		else
		{
			siResult = fLDAPSessionMgr.SafeClose(pContext->fName, nil);
		}
		
		gLDAPContextTable->RemoveItem( inData->fInNodeRef );
		// our last chance to clean up anything we missed for that node 
		gLDAPContinueTable->RemoveItems( inData->fInNodeRef );

	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // CloseDirNode


// ---------------------------------------------------------------------------
//	* MakeContextData
// ---------------------------------------------------------------------------

sLDAPContextData* CLDAPv3Plugin::MakeContextData ( void )
{
    sLDAPContextData   *pOut		= nil;
    sInt32				siResult	= eDSNoErr;

    pOut = (sLDAPContextData *) calloc(1, sizeof(sLDAPContextData));
    if ( pOut != nil )
    {
        //do nothing with return here since we know this is new
        //and we did a memset above
        siResult = CleanContextData(pOut);
    }

    return( pOut );

} // MakeContextData

// ---------------------------------------------------------------------------
//	* CleanContextData
// ---------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::CleanContextData ( sLDAPContextData *inContext )
{
    sInt32	siResult = eDSNoErr;
    
    if ( inContext == nil )
    {
        siResult = eDSBadContextData;
	}
    else
    {
        //LDAP specific
        //SafeClose will release the LDAP servers
        //since there are more than one context with the same fHost
        inContext->fHost			= nil;
        if (inContext->fName != nil)
        {
            free( inContext->fName );
        }
        inContext->fName			= nil;
        inContext->fPort			= 389;
        inContext->fConfigTableIndex= 0;
        inContext->fType			= 0;	//TODO KW using 1 for Nodes and 2 for Records - why?
        inContext->authCallActive	= false;

        // data buffer handling parameters
        inContext->offset			= 0;
        inContext->index			= 1;
        if (inContext->fOpenRecordType != nil)
        {
            free( inContext->fOpenRecordType );
			inContext->fOpenRecordType	= nil;
        }
        if (inContext->fOpenRecordName != nil)
        {
            free( inContext->fOpenRecordName );
			inContext->fOpenRecordName	= nil;
        }
        if (inContext->fOpenRecordDN != nil)
        {
            free( inContext->fOpenRecordDN );
			inContext->fOpenRecordDN	= nil;
        }
        if (inContext->fUserName != nil)
        {
            free( inContext->fUserName );
			inContext->fUserName	= nil;
        }
        if (inContext->fAuthCredential != nil)
        {
            free( inContext->fAuthCredential );
			inContext->fAuthCredential	= nil;
        }
        if (inContext->fAuthType != nil)
        {
            free( inContext->fAuthType );
			inContext->fAuthType	= nil;
        }

        if ( inContext->fPWSNodeRef != 0 )
            dsCloseDirNode(inContext->fPWSNodeRef);
        if ( inContext->fPWSRef != 0 )
            dsCloseDirService(inContext->fPWSRef);
   }

    return( siResult );

} // CleanContextData

//------------------------------------------------------------------------------------
//	* GetRecordList
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::GetRecordList ( sGetRecordList *inData )
{
    sInt32					siResult			= eDSNoErr;
    uInt32					uiTotal				= 0;
    uInt32					uiCount				= 0;
    char				   *pRecName			= nil;
    char				   *pRecType			= nil;
    char				   *pLDAPRecType		= nil;
    bool					bAttribOnly			= false;
    tDirPatternMatch		pattMatch			= eDSNoMatch1;
    CAttributeList		   *cpRecNameList		= nil;
    CAttributeList		   *cpRecTypeList		= nil;
    CAttributeList		   *cpAttrTypeList 		= nil;
    sLDAPContextData	   *pContext			= nil;
    sLDAPContinueData	   *pLDAPContinue		= nil;
    CBuff				   *outBuff				= nil;
    int						numRecTypes			= 1;
    bool					bBuffFull			= false;
    bool					separateRecTypes	= false;
    uInt32					countDownRecTypes	= 0;
	bool					bOCANDGroup			= false;
	CFArrayRef				OCSearchList		= nil;
	ber_int_t				scope				= LDAP_SCOPE_SUBTREE;

    try
    {
        // Verify all the parameters
        if ( inData  == nil ) throw( (sInt32) eMemoryError );
        if ( inData->fInDataBuff  == nil ) throw( (sInt32)eDSEmptyBuffer );
        if (inData->fInDataBuff->fBufferSize == 0) throw( (sInt32)eDSEmptyBuffer );

        if ( inData->fInRecNameList  == nil ) throw( (sInt32)eDSEmptyRecordNameList );
        if ( inData->fInRecTypeList  == nil ) throw( (sInt32)eDSEmptyRecordTypeList );
        if ( inData->fInAttribTypeList  == nil ) throw( (sInt32)eDSEmptyAttributeTypeList );

        // Node context data
        pContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInNodeRef );
        if ( pContext  == nil ) throw( (sInt32)eDSBadContextData );

		if ( inData->fIOContinueData == nil )
		{
			pLDAPContinue = (sLDAPContinueData *)::calloc( 1, sizeof( sLDAPContinueData ) );

			gLDAPContinueTable->AddItem( pLDAPContinue, inData->fInNodeRef );

            //parameters used for data buffering
            pLDAPContinue->fRecNameIndex = 1;
            pLDAPContinue->fRecTypeIndex = 1;
            pLDAPContinue->fTotalRecCount = 0;
            pLDAPContinue->fLimitRecSearch = 0;
			
			//check if the client has requested a limit on the number of records to return
			//we only do this the first call into this context for pLDAPContinue
			if (inData->fOutRecEntryCount >= 0)
			{
				pLDAPContinue->fLimitRecSearch = inData->fOutRecEntryCount;
			}
		}
		else
		{
			pLDAPContinue = (sLDAPContinueData *)inData->fIOContinueData;
			if ( gLDAPContinueTable->VerifyItem( pLDAPContinue ) == false )
			{
				throw( (sInt32)eDSInvalidContinueData );
			}
		}

        // start with the continue set to nil until buffer gets full and there is more data
        //OR we have more record types to look through
        inData->fIOContinueData		= nil;
		//return zero if nothing found here
		inData->fOutRecEntryCount	= 0;

        // copy the buffer data into a more manageable form
        outBuff = new CBuff();
        if ( outBuff  == nil ) throw( (sInt32) eMemoryError );

        siResult = outBuff->Initialize( inData->fInDataBuff, true );
        if ( siResult != eDSNoErr ) throw( siResult );

        siResult = outBuff->GetBuffStatus();
        if ( siResult != eDSNoErr ) throw( siResult );

        siResult = outBuff->SetBuffType( 'StdA' );
        if ( siResult != eDSNoErr ) throw( siResult );

        // Get the record name list for pattern matching
        cpRecNameList = new CAttributeList( inData->fInRecNameList );
        if ( cpRecNameList  == nil ) throw( (sInt32)eDSEmptyRecordNameList );
        if (cpRecNameList->GetCount() == 0) throw( (sInt32)eDSEmptyRecordNameList );

        // Get the record name pattern match type
        pattMatch = inData->fInPatternMatch;

        // Get the record type list
        // Record type mapping for LDAP to DS API is dealt with below
        cpRecTypeList = new CAttributeList( inData->fInRecTypeList );
        if ( cpRecTypeList  == nil ) throw( (sInt32)eDSEmptyRecordTypeList );
        //save the number of rec types here to use in separating the buffer data
        countDownRecTypes = cpRecTypeList->GetCount() - pLDAPContinue->fRecTypeIndex + 1;
        if (cpRecTypeList->GetCount() == 0) throw( (sInt32)eDSEmptyRecordTypeList );

        // Get the attribute list
        //KW? at this time would like to simply dump all attributes
        //would expect to do this always since this is where the databuffer is built
        cpAttrTypeList = new CAttributeList( inData->fInAttribTypeList );
        if ( cpAttrTypeList  == nil ) throw( (sInt32)eDSEmptyAttributeTypeList );
        if (cpAttrTypeList->GetCount() == 0) throw( (sInt32)eDSEmptyAttributeTypeList );

        // Get the attribute info only flag
        bAttribOnly = inData->fInAttribInfoOnly;

        // get records of these types
        while ((( cpRecTypeList->GetAttribute( pLDAPContinue->fRecTypeIndex, &pRecType ) == eDSNoErr ) && (!bBuffFull)) && (!separateRecTypes))
        {
        	//mapping rec types - if std to native
        	numRecTypes = 1;
			bOCANDGroup = false;
			if (OCSearchList != nil)
			{
				CFRelease(OCSearchList);
				OCSearchList = nil;
			}
            pLDAPRecType = MapRecToLDAPType( (const char *)pRecType, pContext->fConfigTableIndex, numRecTypes, &bOCANDGroup, &OCSearchList, &scope,  pConfigFromXML );
            //throw on first nil
            if ( pLDAPRecType == nil ) throw( (sInt32)eDSInvalidRecordType );
            while (( pLDAPRecType != nil ) && (!bBuffFull))
            {
				
                // Get the records of this type and these names
                while (( (cpRecNameList->GetAttribute( pLDAPContinue->fRecNameIndex, &pRecName ) == eDSNoErr) && ( siResult == eDSNoErr) ) && (!bBuffFull))
                 {
                	bBuffFull = false;
                    if ( ::strcmp( pRecName, kDSRecordsAll ) == 0 )
                    {
                        siResult = GetAllRecords( pRecType, pLDAPRecType, cpAttrTypeList, pContext, pLDAPContinue, bAttribOnly, outBuff, uiCount, bOCANDGroup, OCSearchList, scope );
                    }
                    else
                    {
                        siResult = GetTheseRecords( pRecName, pRecType, pLDAPRecType, pattMatch, cpAttrTypeList, pContext, pLDAPContinue, bAttribOnly, outBuff, uiCount, bOCANDGroup, OCSearchList, scope );
                    }

                    //outBuff->GetDataBlockCount( &uiCount );
					//cannot use this as there may be records added from different record names
					//at which point the first name will fill the buffer with n records and
					//uiCount is reported as n but as the second name fills the buffer with m MORE records
					//the statement above will return the total of n+m and add it to the previous n
					//so that the total erroneously becomes 2n+m and not what it should be as n+m
					//therefore uiCount is extracted directly out of the GetxxxRecord(s) calls

                    if ( siResult == CBuff::kBuffFull )
                    {
                    	bBuffFull = true;
                        //set continue if there is more data available
                        inData->fIOContinueData = pLDAPContinue;
                        
                        // check to see if buffer is empty and no entries added
                        // which implies that the buffer is too small
						if ( ( uiCount == 0 ) && ( uiTotal == 0 ) )
						{
							throw( (sInt32)eDSBufferTooSmall );
						}

                        uiTotal += uiCount;
                        inData->fOutRecEntryCount = uiTotal;
                        outBuff->SetLengthToSize();
                        siResult = eDSNoErr;
                    }
                    else if ( siResult == eDSNoErr )
                    {
                        uiTotal += uiCount;
	                    pLDAPContinue->fRecNameIndex++;
						pLDAPContinue->msgId = 0;
                    }

                } // while loop over record names
                
                if (pLDAPRecType != nil)
                {
                    delete( pLDAPRecType );
                    pLDAPRecType = nil;
                }
                
                if (!bBuffFull)
                {
	                numRecTypes++;
					bOCANDGroup = false;
					if (OCSearchList != nil)
					{
						CFRelease(OCSearchList);
						OCSearchList = nil;
					}
	                //get the next mapping
	                pLDAPRecType = MapRecToLDAPType( (const char *)pRecType, pContext->fConfigTableIndex, numRecTypes, &bOCANDGroup, &OCSearchList, &scope, pConfigFromXML );
                }
                
            } // while mapped Rec Type != nil
            
            if (!bBuffFull)
            {
	            pRecType = nil;
	            pLDAPContinue->fRecTypeIndex++;
	            pLDAPContinue->fRecNameIndex = 1;
	            //reset the LDAP message ID to zero since now going to go after a new type
	            pLDAPContinue->msgId = 0;
	            
	            //KW? here we decide to exit with data full of the current type of records
	            // and force a good exit with the data we have so we can come back for the next rec type
            	separateRecTypes = true;
                //set continue since there may be more data available
                inData->fIOContinueData = pLDAPContinue;
				siResult = eDSNoErr;
				
				//however if this was the last rec type then there will be no more data
	            // check the number of rec types left
	            countDownRecTypes--;
	            if (countDownRecTypes == 0)
	            {
	                inData->fIOContinueData = nil;
				}
            }
            
        } // while loop over record types

        if (( siResult == eDSNoErr ) & (!bBuffFull))
        {
            if ( uiTotal == 0 )
            {
                outBuff->ClearBuff();
            }
            else
            {
                outBuff->SetLengthToSize();
            }

            inData->fOutRecEntryCount = uiTotal;
        }
    } // try
    
    catch ( sInt32 err )
    {
		siResult = err;
    }

	if (OCSearchList != nil)
	{
		CFRelease(OCSearchList);
		OCSearchList = nil;
	}
	
	if ( (inData->fIOContinueData == nil) && (pLDAPContinue != nil) )
	{
		// we've decided not to return continue data, so we should clean up
		gLDAPContinueTable->RemoveItem( pLDAPContinue );
		pLDAPContinue = nil;
	}

	if (pLDAPRecType != nil)
	{
		delete( pLDAPRecType );
		pLDAPRecType = nil;
	}

	if ( cpRecNameList != nil )
	{
		delete( cpRecNameList );
		cpRecNameList = nil;
	}

	if ( cpRecTypeList != nil )
	{
		delete( cpRecTypeList );
		cpRecTypeList = nil;
	}

	if ( cpAttrTypeList != nil )
	{
		delete( cpAttrTypeList );
		cpAttrTypeList = nil;
	}

	if ( outBuff != nil )
	{
		delete( outBuff );
		outBuff = nil;
	}

    return( siResult );

} // GetRecordList


// ---------------------------------------------------------------------------
//	* MapRecToLDAPType
// ---------------------------------------------------------------------------

char* CLDAPv3Plugin::MapRecToLDAPType ( const char *inRecType, uInt32 inConfigTableIndex, int inIndex, bool *outOCGroup, CFArrayRef *outOCListCFArray, ber_int_t *outScope, CLDAPv3Configs *inConfigFromXML )
{
	char				   *outResult	= nil;
	uInt32					uiStrLen	= 0;
	uInt32					uiNativeLen	= ::strlen( kDSNativeRecordTypePrefix );
	uInt32					uiStdLen	= ::strlen( kDSStdRecordTypePrefix );
	sLDAPConfigData		   *pConfig		= nil;
	
	//idea here is to use the inIndex to request a specific map
	//if inIndex is 1 then the first map will be returned
	//if inIndex is >= 1 and <= totalCount then that map will be returned
	//if inIndex is <= 0 then nil will be returned
	//if inIndex is > totalCount nil will be returned
	//note the inIndex will reference the inIndexth entry ie. start at 1
	//caller can increment the inIndex starting at one and get maps until nil is returned

	if ( ( inRecType != nil ) && (inIndex > 0) )
	{
		uiStrLen = ::strlen( inRecType );

		// First look for native record type
		if ( ::strncmp( inRecType, kDSNativeRecordTypePrefix, uiNativeLen ) == 0 )
		{
			// Make sure we have data past the prefix
			if ( ( uiStrLen > uiNativeLen ) && (inIndex == 1) )
			{
				uiStrLen = uiStrLen - uiNativeLen;
				outResult = new char[ uiStrLen + 2 ];
				::strcpy( outResult, inRecType + uiNativeLen );
			}
		}//native maps
		//now deal with the standard mappings
		else if ( ::strncmp( inRecType, kDSStdRecordTypePrefix, uiStdLen ) == 0 )
		{
			//find the rec map that we need using inConfigTableIndex
			if (( inConfigTableIndex < gLDAPConfigTableLen) && ( inConfigTableIndex >= 0 ))
			{
				pConfig = (sLDAPConfigData *)gLDAPConfigTable->GetItemData( inConfigTableIndex );
				if (pConfig != nil)
				{
					if (inConfigFromXML != nil)
					{
						//TODO need to "try" to get a default here if no mappings
						if (pConfig->fRecordTypeMapCFArray == 0)
						{
						}
						else if ( CFArrayGetTypeID() == CFGetTypeID( pConfig->fRecordTypeMapCFArray ) )
						{
							outResult = inConfigFromXML->ExtractRecMap(inRecType, pConfig->fRecordTypeMapCFArray, inIndex, outOCGroup, outOCListCFArray, outScope );
						}
					}
				}
			}
		}//standard maps
		//passthrough since we don't know what to do with it
		//and we assume the DS API client knows that the LDAP server
		//can handle this record type
		else
		{
			if ( inIndex == 1 )
			{
				outResult = new char[ 1 + ::strlen( inRecType ) ];
				::strcpy( outResult, inRecType );
			}
		}//passthrough map
		
	}// ( ( inRecType != nil ) && (inIndex > 0) )

	return( outResult );

} // MapRecToLDAPType

//------------------------------------------------------------------------------------
//	* GetAllRecords
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::GetAllRecords (	char			   *inRecType,
										char			   *inNativeRecType,
										CAttributeList	   *inAttrTypeList,
										sLDAPContextData   *inContext,
										sLDAPContinueData  *inContinue,
										bool				inAttrOnly,
										CBuff			   *inBuff,
										uInt32			   &outRecCount,
										bool				inbOCANDGroup,
										CFArrayRef			inOCSearchList,
										ber_int_t			inScope )
{
    sInt32				siResult		= eDSNoErr;
    sInt32				siValCnt		= 0;
    int					ldapReturnCode 	= 0;
    bool				bufferFull		= false;
    LDAPMessage		   *result			= nil;
    char			   *recName			= nil;
    sLDAPConfigData	   *pConfig			= nil;
    int					searchTO		= 0;
	CDataBuff		   *aRecData		= nil;
	CDataBuff		   *aAttrData		= nil;
    char			   *queryFilter		= nil;
	char			  **attrs			= nil;


	//TODO why not optimize and save the queryFilter in the continue data for the next call in?
	
	//build the record query string
	queryFilter = BuildLDAPQueryFilter(	nil,
										nil,
										eDSAnyMatch,
										inContext->fConfigTableIndex,
										false,
										(const char *)inRecType,
										inNativeRecType,
										inbOCANDGroup,
										inOCSearchList,
                                        pConfigFromXML );
	    
	outRecCount = 0; //need to track how many records were found by this call to GetAllRecords
	
    try
    {
    	if (inContext == nil ) throw( (sInt32)eDSInvalidContext);
    	if (inContinue == nil ) throw( (sInt32)eDSInvalidContinueData);
    	
    	// check to make sure the queryFilter is not nil
    	if ( queryFilter == nil ) throw( (sInt32)eDSNullParameter );
    	
		aRecData = new CDataBuff();
		if ( aRecData  == nil ) throw( (sInt32) eMemoryError );
		aAttrData = new CDataBuff();
		if ( aAttrData  == nil ) throw( (sInt32) eMemoryError );
		
		//retrieve the config data
		//don't need to retrieve for the case of "generic unknown" so don't check index 0
		if (( inContext->fConfigTableIndex < gLDAPConfigTableLen) && ( inContext->fConfigTableIndex >= 1 ))
		{
			pConfig = (sLDAPConfigData *)gLDAPConfigTable->GetItemData( inContext->fConfigTableIndex );
			if (pConfig != nil)
			{
				searchTO	= pConfig->fSearchTimeout;
			}
		}
		
		//LDAP search with rebind capability
		DSDoSearch (	inRecType,
						inNativeRecType,
						inAttrTypeList,
						inContext,
						inContinue,
						pConfigFromXML,
						inScope,
						fLDAPSessionMgr,
						queryFilter,
						ldapReturnCode,
						searchTO,
						result);

		while ( 	( (ldapReturnCode == LDAP_RES_SEARCH_ENTRY) || (ldapReturnCode == LDAP_RES_SEARCH_RESULT) )
					&& !(bufferFull)
					&& (	(inContinue->fTotalRecCount < inContinue->fLimitRecSearch) ||
							(inContinue->fLimitRecSearch == 0) ) )
        {
			if (ldapReturnCode == LDAP_RES_SEARCH_ENTRY)
			{
				// package the record into the DS format into the buffer
				// steps to add an entry record to the buffer
				// build the fRecData header
				// build the fAttrData
				// append the fAttrData to the fRecData
				// add the fRecData to the buffer inBuff
	
				aAttrData->Clear();
				aRecData->Clear();
	
				if ( inRecType != nil )
				{
					aRecData->AppendShort( ::strlen( inRecType ) );
					aRecData->AppendString( inRecType );
				} // what to do if the inRecType is nil? - never get here then
				else
				{
					aRecData->AppendShort( ::strlen( "Record Type Unknown" ) );
					aRecData->AppendString( "Record Type Unknown" );
				}
	
				// need to get the record name
				recName = GetRecordName( inRecType, result, inContext, siResult );
				if ( siResult != eDSNoErr ) throw( siResult );
				if ( recName != nil )
				{
					aRecData->AppendShort( ::strlen( recName ) );
					aRecData->AppendString( recName );
	
					delete ( recName );
					recName = nil;
				}
				else
				{
					aRecData->AppendShort( ::strlen( "Record Name Unknown" ) );
					aRecData->AppendString( "Record Name Unknown" );
				}
	
				// need to calculate the number of attribute types ie. siValCnt
				// also need to extract the attributes and place them into fAttrData
				//siValCnt = 0;
	
				aAttrData->Clear();
				siResult = GetTheseAttributes( inRecType, inAttrTypeList, result, inAttrOnly, inContext, siValCnt, aAttrData );
				if ( siResult != eDSNoErr ) throw( siResult );
				
				//add the attribute info to the fRecData
				if ( siValCnt == 0 )
				{
					// Attribute count
					aRecData->AppendShort( 0 );
				}
				else
				{
					// Attribute count
					aRecData->AppendShort( siValCnt );
					aRecData->AppendBlock( aAttrData->GetData(), aAttrData->GetLength() );
				}
	
				// add the fRecData now to the inBuff
				siResult = inBuff->AddData( aRecData->GetData(), aRecData->GetLength() );
			}
            // need to check if the buffer is full
			// need to handle full buffer and keep the result alive for the next call in
            if (siResult == CBuff::kBuffFull)
            {
                bufferFull = true;
                
                //save the result if buffer is full
                inContinue->pResult = result;
            }
            else if ( siResult == eDSNoErr )
            {
                ldap_msgfree( result );
				result = nil;
                
				if (ldapReturnCode == LDAP_RES_SEARCH_ENTRY)
				{
					outRecCount++; //another record added
					inContinue->fTotalRecCount++;
				}
				
                //make sure no result is carried in the context
                inContinue->pResult = nil;

				//only get next result if buffer is not full
				DSGetSearchLDAPResult (	fLDAPSessionMgr,
										inContext,
										searchTO,
										result,
										LDAP_MSG_ONE,
										inContinue->msgId,
										ldapReturnCode,
										false);
            }
            else
            {
                //make sure no result is carried in the context
                inContinue->pResult = nil;
                throw( (sInt32)eDSInvalidBuffFormat );
            }

        } // while loop over entries

		if ( (result != inContinue->pResult) && (result != nil) )
		{
			ldap_msgfree( result );
			result = nil;
		}

    } // try block

    catch ( sInt32 err )
    {
        siResult = err;
    }

	if (attrs != nil)
	{
		int aIndex = 0;
		while (attrs[aIndex] != nil)
		{
			free(attrs[aIndex]);
			attrs[aIndex] = nil;
			aIndex++;
		}
		free(attrs);
		attrs = nil;
	}
	
	if ( queryFilter != nil )
	{
		delete( queryFilter );
		queryFilter = nil;
	}

	if (aRecData != nil)
	{
		delete (aRecData);
		aRecData = nil;
	}
	if (aAttrData != nil)
	{
		delete (aAttrData);
		aAttrData = nil;
	}

    return( siResult );

} // GetAllRecords


//------------------------------------------------------------------------------------
//	* GetRecordEntry
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::GetRecordEntry ( sGetRecordEntry *inData )
{
    sInt32					siResult		= eDSNoErr;
    uInt32					uiIndex			= 0;
    uInt32					uiCount			= 0;
    uInt32					uiOffset		= 0;
	uInt32					uberOffset		= 0;
    char 				   *pData			= nil;
    tRecordEntryPtr			pRecEntry		= nil;
    sLDAPContextData		   *pContext		= nil;
    CBuff					inBuff;
	uInt32					offset			= 0;
	uInt16					usTypeLen		= 0;
	char				   *pRecType		= nil;
	uInt16					usNameLen		= 0;
	char				   *pRecName		= nil;
	uInt16					usAttrCnt		= 0;
	uInt32					buffLen			= 0;

    try
    {
        if ( inData  == nil ) throw( (sInt32) eMemoryError );
        if ( inData->fInOutDataBuff  == nil ) throw( (sInt32)eDSEmptyBuffer );
        if (inData->fInOutDataBuff->fBufferSize == 0) throw( (sInt32)eDSEmptyBuffer );

        siResult = inBuff.Initialize( inData->fInOutDataBuff );
        if ( siResult != eDSNoErr ) throw( siResult );

        siResult = inBuff.GetDataBlockCount( &uiCount );
        if ( siResult != eDSNoErr ) throw( siResult );

        uiIndex = inData->fInRecEntryIndex;
        if ((uiIndex > uiCount) || (uiIndex == 0)) throw( (sInt32)eDSInvalidIndex );

        pData = inBuff.GetDataBlock( uiIndex, &uberOffset );
        if ( pData  == nil ) throw( (sInt32)eDSCorruptBuffer );

		//assume that the length retrieved is valid
		buffLen = inBuff.GetDataBlockLength( uiIndex );
		
		// Skip past this same record length obtained from GetDataBlockLength
		pData	+= 4;
		offset	= 0; //buffLen does not include first four bytes

		// Do record check, verify that offset is not past end of buffer, etc.
		if (offset + 2 > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the length for the record type
		::memcpy( &usTypeLen, pData, 2 );
		
		pData	+= 2;
		offset	+= 2;

		pRecType = pData;
		
		pData	+= usTypeLen;
		offset	+= usTypeLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (offset + 2 > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the length for the record name
		::memcpy( &usNameLen, pData, 2 );
		
		pData	+= 2;
		offset	+= 2;

		pRecName = pData;
		
		pData	+= usNameLen;
		offset	+= usNameLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (offset + 2 > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the attribute count
		::memcpy( &usAttrCnt, pData, 2 );

		pRecEntry = (tRecordEntry *)::calloc( 1, sizeof( tRecordEntry ) + usNameLen + usTypeLen + 4 + kBuffPad );

		pRecEntry->fRecordNameAndType.fBufferSize	= usNameLen + usTypeLen + 4 + kBuffPad;
		pRecEntry->fRecordNameAndType.fBufferLength	= usNameLen + usTypeLen + 4;

		// Add the record name length
		::memcpy( pRecEntry->fRecordNameAndType.fBufferData, &usNameLen, 2 );
		uiOffset += 2;

		// Add the record name
		::memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, pRecName, usNameLen );
		uiOffset += usNameLen;

		// Add the record type length
		::memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, &usTypeLen, 2 );

		// Add the record type
		uiOffset += 2;
		::memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, pRecType, usTypeLen );

		pRecEntry->fRecordAttributeCount = usAttrCnt;

        pContext = MakeContextData();
        if ( pContext  == nil ) throw( (sInt32) eMemoryAllocError );

        pContext->offset = uberOffset + offset + 4; // context used by next calls of GetAttributeEntry
													// include the four bytes of the buffLen

        gLDAPContextTable->AddItem( inData->fOutAttrListRef, pContext );

        inData->fOutRecEntryPtr = pRecEntry;
    }

    catch ( sInt32 err )
    {
        siResult = err;
    }

    return( siResult );

} // GetRecordEntry


//------------------------------------------------------------------------------------
//	* GetTheseAttributes
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::GetTheseAttributes (	char			   *inRecType,
											CAttributeList	   *inAttrTypeList,
											LDAPMessage		   *inResult,
											bool				inAttrOnly,
											sLDAPContextData   *inContext,
											sInt32			   &outCount,
											CDataBuff		   *inDataBuff )
{
	sInt32				siResult				= eDSNoErr;
	sInt32				attrTypeIndex			= 1;
	char			   *pLDAPAttrType			= nil;
	char			   *pAttrType				= nil;
	char			   *pAttr					= nil;
	BerElement		   *ber						= nil;
	struct berval	  **bValues;
	int					numAttributes			= 1;
	int					numStdAttributes		= 1;
	bool				bTypeFound				= false;
	int					valCount				= 0;
	char			   *pStdAttrType			= nil;
	bool				bAtLeastOneTypeValid	= false;
	CDataBuff		   *aTmpData				= nil;
	CDataBuff		   *aTmp2Data				= nil;
	bool				bStripCryptPrefix		= false;
	bool				bUsePlus				= true;
	char			   *pLDAPPasswdAttrType		= nil;
	uInt32				literalLength			= 0;
	LDAP			   *aHost					= nil;

	try
	{
		if ( inContext  == nil ) throw( (sInt32)eDSBadContextData );
		
		outCount = 0;
		aTmpData = new CDataBuff();
		if ( aTmpData  == nil ) throw( (sInt32) eMemoryError );
		aTmp2Data = new CDataBuff();
		if ( aTmp2Data  == nil ) throw( (sInt32) eMemoryError );
		inDataBuff->Clear();

		// Get the record attributes with/without the values
		while ( inAttrTypeList->GetAttribute( attrTypeIndex++, &pAttrType ) == eDSNoErr )
		{
			siResult = eDSNoErr;
			if ( ::strcmp( kDSNAttrMetaNodeLocation, pAttrType ) == 0 )
			{
			// we look at kDSNAttrMetaNodeLocation with NO mapping
			// since we have special code to deal with it and we always place the
			// node name into it
				aTmpData->Clear();
				
				// Append the attribute name
				aTmpData->AppendShort( ::strlen( pAttrType ) );
				aTmpData->AppendString( pAttrType );

				if ( inAttrOnly == false )
				{
					// Append the attribute value count
					aTmpData->AppendShort( 1 );

					char *tmpStr = nil;

					//extract name from the context data
					//need to add prefix of /LDAPv3/ here since the fName does not contain it in the context
					if ( inContext->fName != nil )
					{
		        		tmpStr = new char[1+8+::strlen(inContext->fName)];
		        		::strcpy( tmpStr, "/LDAPv3/" );
		        		::strcat( tmpStr, inContext->fName );
					}
					else
					{
						tmpStr = new char[1+::strlen("Unknown Node Location")];
						::strcpy( tmpStr, "Unknown Node Location" );
					}

					// Append attribute value
					aTmpData->AppendLong( ::strlen( tmpStr ) );
					aTmpData->AppendString( tmpStr );

					delete( tmpStr );
				}// if ( inAttrOnly == false )
				else
				{
					aTmpData->AppendShort( 0 );
				}

				// Add the attribute length
				outCount++;
				inDataBuff->AppendLong( aTmpData->GetLength() );
				inDataBuff->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );

				// Clear the temp block
				aTmpData->Clear();

			} // if ( ::strcmp( kDSNAttrMetaNodeLocation, pAttrType ) == 0 )
			else if ((::strcmp(pAttrType,kDSAttributesAll) == 0) || (::strcmp(pAttrType,kDSAttributesStandardAll) == 0) || (::strcmp(pAttrType,kDSAttributesNativeAll) == 0))
			{
				if ((::strcmp(pAttrType,kDSAttributesAll) == 0) || (::strcmp(pAttrType,kDSAttributesStandardAll) == 0))
				{
					// we look at kDSNAttrMetaNodeLocation with NO mapping
					// since we have special code to deal with it and we always place the
					// node name into it AND we output it here since ALL or ALL Std was asked for
					aTmpData->Clear();

					// Append the attribute name
					aTmpData->AppendShort( ::strlen( kDSNAttrMetaNodeLocation ) );
					aTmpData->AppendString( kDSNAttrMetaNodeLocation );

					if ( inAttrOnly == false )
					{
						// Append the attribute value count
						aTmpData->AppendShort( 1 );

						char *tmpStr = nil;

						//extract name from the context data
						//need to add prefix of /LDAPv3/ here since the fName does not contain it in the context
						if ( inContext->fName != nil )
						{
							tmpStr = new char[1+8+::strlen(inContext->fName)];
							::strcpy( tmpStr, "/LDAPv3/" );
							::strcat( tmpStr, inContext->fName );
						}
						else
						{
							tmpStr = new char[1+::strlen("Unknown Node Location")];
							::strcpy( tmpStr, "Unknown Node Location" );
						}

						// Append attribute value
						aTmpData->AppendLong( ::strlen( tmpStr ) );
						aTmpData->AppendString( tmpStr );

						delete( tmpStr );
					}
					else
					{
						aTmpData->AppendShort( 0 );
					}
	
					// Add the attribute length
					outCount++;
					inDataBuff->AppendLong( aTmpData->GetLength() );
					inDataBuff->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
	
					// Clear the temp block
					aTmpData->Clear();
						
					//Get the mapping for kDS1AttrPassword
					//If it exists AND it is mapped to something that exists IN LDAP then we will use it
					//otherwise we set bUsePlus and use the kDS1AttrPasswordPlus value of "*******"
					//Don't forget to strip off the {crypt} prefix from kDS1AttrPassword as well
					pLDAPPasswdAttrType = MapAttrToLDAPType( (const char *)inRecType, kDS1AttrPassword, inContext->fConfigTableIndex, 1, pConfigFromXML, true );
					
					//plan is to output both standard and native attributes if request ALL ie. kDSAttributesAll
					// ie. all the attributes even if they are duplicated
					
					// std attributes go first
					numStdAttributes = 1;
					pStdAttrType = GetNextStdAttrType( inRecType, inContext->fConfigTableIndex, numStdAttributes );
					while ( pStdAttrType != nil )
					{
						//get the first mapping
						numAttributes = 1;
						pLDAPAttrType = MapAttrToLDAPType( (const char *)inRecType, pStdAttrType, inContext->fConfigTableIndex, numAttributes, pConfigFromXML );
						//throw if first nil since no more will be found otherwise proceed until nil
											//can't throw since that wipes out retrieval of all the following requested attributes
						//if (pLDAPAttrType == nil ) throw( (sInt32)eDSInvalidAttrListRef); //KW would like a eDSNoMappingAvailable
						
						//set the indicator to check whether at least one of the native maps
						//form given standard type is correct
						//KW need to provide client with info on failed mappings??
						bAtLeastOneTypeValid = false;
						//set indicator of multiple native to standard mappings
						bTypeFound = false;
						while ( pLDAPAttrType != nil )
						{
							if (pLDAPAttrType[0] == '#') //special case where attr is mapped to a literal string
							{
								if (!bTypeFound)
								{
									aTmp2Data->Clear();
		
									//use given type in the output NOT mapped to type
									aTmp2Data->AppendShort( ::strlen( pStdAttrType ) );
									aTmp2Data->AppendString( pStdAttrType );
									
									//set indicator so that multiple values from multiple mapped to types
									//can be added to the given type
									bTypeFound = true;
									
									//set attribute value count to zero
									valCount = 0;
		
									// Clear the temp block
									aTmpData->Clear();
								}
								
								//set the flag indicating that we got a match at least once
								bAtLeastOneTypeValid = true;
	
								literalLength = strlen(pLDAPAttrType) - 1;
	
								if ( (inAttrOnly == false) && (literalLength > 0) )
								{
									valCount++;
										
									// Append attribute value
									aTmpData->AppendLong( literalLength );
									aTmpData->AppendBlock( pLDAPAttrType + 1, literalLength );
								} // if ( (inAttrOnly == false) && (strlen(pLDAPAttrType) > 1) ) 
							}
							else
							{
								aHost = fLDAPSessionMgr.LockSession(inContext);
								if (aHost != nil)
								{
									for (	pAttr = ldap_first_attribute (aHost, inResult, &ber );
											pAttr != NULL; pAttr = ldap_next_attribute(aHost, inResult, ber ) )
									{
										//TODO can likely optimize use of ldap_get_values_len() below for Std types
										bStripCryptPrefix = false;
										if (::strcasecmp( pAttr, pLDAPAttrType ) == 0)
										{
											if (pLDAPPasswdAttrType != nil )
											{
												if ( ( ::strcasecmp( pAttr, pLDAPPasswdAttrType ) == 0 ) &&
													( ::strcmp( pStdAttrType, kDS1AttrPassword ) == 0 ) )
												{
													//want to remove leading "{crypt}" prefix from password if it exists
													bStripCryptPrefix = true;
													//don't need to use the "********" passwdplus
													bUsePlus = false;
													//cleanup
													free(pLDAPPasswdAttrType);
													pLDAPPasswdAttrType = nil;
												}
											}
											//set the flag indicating that we got a match at least once
											bAtLeastOneTypeValid = true;
											//note that if standard type is incorrectly mapped ie. not found here
											//then the output will not contain any data on that std type
											if (!bTypeFound)
											{
												aTmp2Data->Clear();
			
												//use given type in the output NOT mapped to type
												aTmp2Data->AppendShort( ::strlen( pStdAttrType ) );
												aTmp2Data->AppendString( pStdAttrType );
												//set indicator so that multiple values from multiple mapped to types
												//can be added to the given type
												bTypeFound = true;
												
												//set attribute value count to zero
												valCount = 0;
												
												// Clear the temp block
												aTmpData->Clear();
											}
			
											if (( inAttrOnly == false ) &&
												(( bValues = ldap_get_values_len (aHost, inResult, pAttr )) != NULL) )
											{
												if (bStripCryptPrefix)
												{
													bStripCryptPrefix = false; //only attempted once here
													// add to the number of values for this attribute
													for (int ii = 0; bValues[ii] != NULL; ii++ )
													valCount++;
													
													// for each value of the attribute
													for (int i = 0; bValues[i] != NULL; i++ )
													{
														//case insensitive compare with "crypt" string
														if ( ( bValues[i]->bv_len > 7) &&
															(strncasecmp(bValues[i]->bv_val,"{crypt}",7) == 0) )
														{
															// Append attribute value without "{crypt}" prefix
															aTmpData->AppendLong( bValues[i]->bv_len - 7 );
															aTmpData->AppendBlock( (bValues[i]->bv_val) + 7, bValues[i]->bv_len - 7 );
														}
														else
														{
															// Append attribute value
															aTmpData->AppendLong( bValues[i]->bv_len );
															aTmpData->AppendBlock( bValues[i]->bv_val, bValues[i]->bv_len );
														}
													} // for each bValues[i]
													ldap_value_free_len(bValues);
													bValues = NULL;
												}
												else
												{
													// add to the number of values for this attribute
													for (int ii = 0; bValues[ii] != NULL; ii++ )
													valCount++;
													
													// for each value of the attribute
													for (int i = 0; bValues[i] != NULL; i++ )
													{
														// Append attribute value
														aTmpData->AppendLong( bValues[i]->bv_len );
														aTmpData->AppendBlock( bValues[i]->bv_val, bValues[i]->bv_len );
													} // for each bValues[i]
													ldap_value_free_len(bValues);
													bValues = NULL;
												}
											} // if ( inAttrOnly == false ) && bValues = ldap_get_values_len ...
											
											if (pAttr != nil)
											{
												ldap_memfree( pAttr );
											}
											//found the right attr so go to the next one
											break;
										} // if (::strcmp( pAttr, pLDAPAttrType ) == 0) || 
										if (pAttr != nil)
										{
											ldap_memfree( pAttr );
										}
									} // for ( loop over ldap_next_attribute )
									
									if (ber != nil)
									{
										ber_free( ber, 0 );
										ber = nil;
									}
								} //aHost != nil
								fLDAPSessionMgr.UnLockSession(inContext);
							}
							
							//cleanup pLDAPAttrType if needed
							if (pLDAPAttrType != nil)
							{
								delete (pLDAPAttrType);
								pLDAPAttrType = nil;
							}
							numAttributes++;
							//get the next mapping
							pLDAPAttrType = MapAttrToLDAPType( (const char *)inRecType, pStdAttrType, inContext->fConfigTableIndex, numAttributes, pConfigFromXML );				
						} // while ( pLDAPAttrType != nil )
						
						if (bAtLeastOneTypeValid)
						{
							// Append the attribute value count
							aTmp2Data->AppendShort( valCount );
							
							if (valCount > 0)
							{
								// Add the attribute values to the attribute type
								aTmp2Data->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
								valCount = 0;
							}
	
							// Add the attribute data to the attribute data buffer
							outCount++;
							inDataBuff->AppendLong( aTmp2Data->GetLength() );
							inDataBuff->AppendBlock( aTmp2Data->GetData(), aTmp2Data->GetLength() );
						}
						
						//cleanup pStdAttrType if needed
						if (pStdAttrType != nil)
						{
							delete (pStdAttrType);
							pStdAttrType = nil;
						}
						numStdAttributes++;
						//get the next std attribute
						pStdAttrType = GetNextStdAttrType( inRecType, inContext->fConfigTableIndex, numStdAttributes );
					}// while ( pStdAttrType != nil )
					
					if (bUsePlus)
					{
						// we add kDS1AttrPasswordPlus here
						aTmpData->Clear();
		
						// Append the attribute name
						aTmpData->AppendShort( ::strlen( kDS1AttrPasswordPlus ) );
						aTmpData->AppendString( kDS1AttrPasswordPlus );
	
						if ( inAttrOnly == false )
						{
							// Append the attribute value count
							aTmpData->AppendShort( 1 );
	
							// Append attribute value
							aTmpData->AppendLong( 8 );
							aTmpData->AppendString( "********" );
						}
						else
						{
							aTmpData->AppendShort( 0 );
						}
	
						// Add the attribute length
						outCount++;
						inDataBuff->AppendLong( aTmpData->GetLength() );
						inDataBuff->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );

						// Clear the temp block
						aTmpData->Clear();
					} //if (bUsePlus)
					
				}// Std and all
				if ((::strcmp(pAttrType,kDSAttributesAll) == 0) || (::strcmp(pAttrType,kDSAttributesNativeAll) == 0))
				{
					aHost = fLDAPSessionMgr.LockSession(inContext);
					if (aHost != nil)
					{
						//now we output the native attributes
						for (	pAttr = ldap_first_attribute (aHost, inResult, &ber );
								pAttr != NULL; pAttr = ldap_next_attribute(aHost, inResult, ber ) )
						{
							aTmpData->Clear();
									
							if ( pAttr != nil )
							{
								aTmpData->AppendShort( ::strlen( pAttr ) + ::strlen( kDSNativeAttrTypePrefix ) );
								aTmpData->AppendString( (char *)kDSNativeAttrTypePrefix );
								aTmpData->AppendString( pAttr );
		
								if (( inAttrOnly == false ) &&
									(( bValues = ldap_get_values_len(aHost, inResult, pAttr )) != NULL) )
								{
								
									// calculate the number of values for this attribute
									valCount = 0;
									for (int ii = 0; bValues[ii] != NULL; ii++ )
									valCount++;
									
									// Append the attribute value count
									aTmpData->AppendShort( valCount );
			
									// for each value of the attribute
									for (int i = 0; bValues[i] != NULL; i++ )
									{
										// Append attribute value
										aTmpData->AppendLong( bValues[i]->bv_len );
										aTmpData->AppendBlock( bValues[i]->bv_val, bValues[i]->bv_len );
									} // for each bValues[i]
									ldap_value_free_len(bValues);
									bValues = NULL;
								} // if ( inAttrOnly == false ) && bValues = ldap_get_values_len ...
								else
								{
									aTmpData->AppendShort( 0 );
								}
							
							}
							// Add the attribute length
							outCount++;
							inDataBuff->AppendLong( aTmpData->GetLength() );
							inDataBuff->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
		
							// Clear the temp block
							aTmpData->Clear();
							
							if (pAttr != nil)
							{
								ldap_memfree( pAttr );
							}
						} // for ( loop over ldap_next_attribute )
					
						if (ber != nil)
						{
							ber_free( ber, 0 );
							ber = nil;
						}
					} //if aHost != nil
					fLDAPSessionMgr.UnLockSession(inContext);
				}//Native and all
			}
			else //we have a specific attribute in mind
			{
				bStripCryptPrefix = false;
				//here we first check for the request for kDS1AttrPasswordPlus
				//ie. we see if kDS1AttrPassword is mapped and return that value if it is -- otherwise
				//we return the special value of "********" which is apparently never a valid crypt password
				//but this signals the requestor of kDS1AttrPasswordPlus to attempt to auth against the LDAP
				//server through doAuthentication via dsDoDirNodeAuth
				//get the first mapping
				numAttributes = 1;
				if (::strcmp( kDS1AttrPasswordPlus, pAttrType ) == 0)
				{
					//want to remove leading "{crypt}" prefix from password if it exists
					bStripCryptPrefix = true;
					pLDAPAttrType = MapAttrToLDAPType( (const char *)inRecType, kDS1AttrPassword, inContext->fConfigTableIndex, numAttributes, pConfigFromXML );
					//set the indicator to check whether at least one of the native maps
					//form given standard type is correct
					//KW need to provide client with info on failed mappings??
					bAtLeastOneTypeValid = false;
					//set indicator of multiple native to standard mappings
					bTypeFound = false;
					if (pLDAPAttrType == nil)
					{
						bAtLeastOneTypeValid = true;
						
						//here we fill the value with "*******"
						aTmp2Data->Clear();
						//use given type in the output NOT mapped to type
						aTmp2Data->AppendShort( ::strlen( pAttrType ) );
						aTmp2Data->AppendString( pAttrType );
						//set indicator so that multiple values from multiple mapped to types
						//can be added to the given type
						bTypeFound = true;
						
						if (inAttrOnly == false)
						{
							//set attribute value count to one
							valCount = 1;
									
							// Clear the temp block
							aTmpData->Clear();
							// Append attribute value
							aTmpData->AppendLong( 8 );
							aTmpData->AppendString( "********" );
						}
					}
				}
				else
				{
					pLDAPAttrType = MapAttrToLDAPType( (const char *)inRecType, pAttrType, inContext->fConfigTableIndex, numAttributes, pConfigFromXML );
					//throw if first nil since no more will be found otherwise proceed until nil
					// can't throw since that wipes out retrieval of all the following requested attributes
					//if (pLDAPAttrType == nil ) throw( (sInt32)eDSInvalidAttrListRef); //KW would like a eDSNoMappingAvailable

					//set the indicator to check whether at least one of the native maps
					//form given standard type is correct
					//KW need to provide client with info on failed mappings??
					bAtLeastOneTypeValid = false;
					//set indicator of multiple native to standard mappings
					bTypeFound = false;
				}
				
				while ( pLDAPAttrType != nil )
				{
					//note that if standard type is incorrectly mapped ie. not found here
					//then the output will not contain any data on that std type
					if (!bTypeFound)
					{
						aTmp2Data->Clear();

						//use given type in the output NOT mapped to type
						aTmp2Data->AppendShort( ::strlen( pAttrType ) );
						aTmp2Data->AppendString( pAttrType );
						//set indicator so that multiple values from multiple mapped to types
						//can be added to the given type
						bTypeFound = true;
						
						//set attribute value count to zero
						valCount = 0;
						
						// Clear the temp block
						aTmpData->Clear();
					}

					if (pLDAPAttrType[0] == '#') //special case where attr is mapped to a literal string
					{
						bAtLeastOneTypeValid = true;
						literalLength = strlen(pLDAPAttrType) - 1;

						if ( (inAttrOnly == false) && (literalLength > 0) )
						{
							valCount++;
								
							// Append attribute value
							aTmpData->AppendLong( literalLength );
							aTmpData->AppendBlock( pLDAPAttrType + 1, literalLength );
						} // if ( (inAttrOnly == false) && (strlen(pLDAPAttrType) > 1) ) 
					}
					else
					{
						aHost = fLDAPSessionMgr.LockSession(inContext);
						if (aHost != nil)
						{
							bValues = ldap_get_values_len (aHost, inResult, pLDAPAttrType );
							if (bValues != NULL && bValues[0] != NULL)
							{
								bAtLeastOneTypeValid = true;
							}
							if ( ( inAttrOnly == false ) && (bValues != NULL) )
							{
							
								if (bStripCryptPrefix)
								{
									// add to the number of values for this attribute
									for (int ii = 0; bValues[ii] != NULL; ii++ )
									valCount++;
									
									if (valCount == 0)
									{
										valCount = 1;
										//no value found or returned for the mapped password attr
										// Append attribute value
										aTmpData->AppendLong( 8 );
										aTmpData->AppendString( "********" );
									}
									
									// for each value of the attribute
									for (int i = 0; bValues[i] != NULL; i++ )
									{
										//case insensitive compare with "crypt" string
										if ( ( bValues[i]->bv_len > 7) &&
											(strncasecmp(bValues[i]->bv_val,"{crypt}",7) == 0) )
										{
											// Append attribute value without "{crypt}" prefix
											aTmpData->AppendLong( bValues[i]->bv_len - 7 );
											aTmpData->AppendBlock( (bValues[i]->bv_val) + 7, bValues[i]->bv_len - 7 );
										}
										else
										{
											// Append attribute value
											aTmpData->AppendLong( bValues[i]->bv_len );
											aTmpData->AppendBlock( bValues[i]->bv_val, bValues[i]->bv_len );
										}
									} // for each bValues[i]
									ldap_value_free_len(bValues);
									bValues = NULL;
								}
								else
								{
									// add to the number of values for this attribute
									for (int ii = 0; bValues[ii] != NULL; ii++ )
									valCount++;
									
									// for each value of the attribute
									for (int i = 0; bValues[i] != NULL; i++ )
									{
										// Append attribute value
										aTmpData->AppendLong( bValues[i]->bv_len );
										aTmpData->AppendBlock( bValues[i]->bv_val, bValues[i]->bv_len );
									} // for each bValues[i]
									ldap_value_free_len(bValues);
									bValues = NULL;
								}
							} // if (aHost != nil) && ( inAttrOnly == false ) && bValues = ldap_get_values_len ...
							else if ( (valCount == 0) && (bValues == NULL) && bStripCryptPrefix)
							{
								valCount = 1;
								//no value found or returned for the mapped password attr
								// Append attribute value
								aTmpData->AppendLong( 8 );
								aTmpData->AppendString( "********" );
							}
							if (bValues != NULL)
							{
								ldap_value_free_len(bValues);
								bValues = NULL;
							}
						}
						fLDAPSessionMgr.UnLockSession(inContext);
					}
							
					//cleanup pLDAPAttrType if needed
					if (pLDAPAttrType != nil)
					{
						delete (pLDAPAttrType);
						pLDAPAttrType = nil;
					}
					numAttributes++;
					//get the next mapping
					pLDAPAttrType = MapAttrToLDAPType( (const char *)inRecType, pAttrType, inContext->fConfigTableIndex, numAttributes, pConfigFromXML );				
				} // while ( pLDAPAttrType != nil )
				
				if ((bAtLeastOneTypeValid && inAttrOnly) || (valCount > 0))
				{
					// Append the attribute value count
					aTmp2Data->AppendShort( valCount );
					
					if (valCount > 0) 
					{
						// Add the attribute values to the attribute type
						aTmp2Data->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
						valCount = 0;
					}

					// Add the attribute data to the attribute data buffer
					outCount++;
					inDataBuff->AppendLong( aTmp2Data->GetLength() );
					inDataBuff->AppendBlock( aTmp2Data->GetData(), aTmp2Data->GetLength() );
				}
			} // else specific attr in mind
			
		} // while ( inAttrTypeList->GetAttribute( attrTypeIndex++, &pAttrType ) == eDSNoErr )
		
	} // try

	catch ( sInt32 err )
	{
		siResult = err;
	}
	
	if ( pLDAPPasswdAttrType != nil )
	{
		free(pLDAPPasswdAttrType);
		pLDAPPasswdAttrType = nil;
	}
	if ( aTmpData != nil )
	{
		delete( aTmpData );
		aTmpData = nil;
	}
	if ( aTmp2Data != nil )
	{
		delete( aTmp2Data );
		aTmp2Data = nil;
	}

	return( siResult );

} // GetTheseAttributes


//------------------------------------------------------------------------------------
//	* GetRecordName
//------------------------------------------------------------------------------------

char *CLDAPv3Plugin::GetRecordName (	char			   *inRecType,
										LDAPMessage		   *inResult,
                                        sLDAPContextData   *inContext,
                                        sInt32			   &errResult )
{
	char		       *recName			= nil;
	char		       *pLDAPAttrType	= nil;
	struct berval	  **bValues;
	int					numAttributes	= 1;
	bool				bTypeFound		= false;
	LDAP			   *aHost			= nil;

	try
	{
		if ( inContext  == nil ) throw( (sInt32)eDSBadContextData );

		errResult = eDSNoErr;
            
		//get the first mapping
		numAttributes = 1;
		pLDAPAttrType = MapAttrToLDAPType( (const char *)inRecType, kDSNAttrRecordName, inContext->fConfigTableIndex, numAttributes, pConfigFromXML, true );
		//throw if first nil since no more will be found otherwise proceed until nil
		if (pLDAPAttrType == nil ) throw( (sInt32)eDSInvalidAttrListRef); //KW would like a eDSNoMappingAvailable
            
		//set indicator of multiple native to standard mappings
		bTypeFound = false;
		aHost = fLDAPSessionMgr.LockSession(inContext);
		while ( (aHost != nil) && ( pLDAPAttrType != nil ) && (!bTypeFound) )
		{
			if ( ( bValues = ldap_get_values_len(aHost, inResult, pLDAPAttrType )) != NULL )
			{
				// for first value of the attribute
				recName = new char[1 + bValues[0]->bv_len];
				::strcpy( recName, bValues[0]->bv_val );
				//we found a value so stop looking
				bTypeFound = true;
				ldap_value_free_len(bValues);
				bValues = NULL;
			} // if ( bValues = ldap_get_values_len ...)
						
			//cleanup pLDAPAttrType if needed
			if (pLDAPAttrType != nil)
			{
					delete (pLDAPAttrType);
					pLDAPAttrType = nil;
			}
			numAttributes++;
			//get the next mapping
			pLDAPAttrType = MapAttrToLDAPType( (const char *)inRecType, kDSNAttrRecordName, inContext->fConfigTableIndex, numAttributes, pConfigFromXML, true );				
		} // while ( pLDAPAttrType != nil )
		fLDAPSessionMgr.UnLockSession(inContext);
		//cleanup pLDAPAttrType if needed
		if (pLDAPAttrType != nil)
		{
			delete (pLDAPAttrType);
			pLDAPAttrType = nil;
		}
           
	} // try

	catch ( sInt32 err )
	{
		errResult = err;
	}

	return( recName );

} // GetRecordName


// ---------------------------------------------------------------------------
//	* MapAttrToLDAPType
// ---------------------------------------------------------------------------

char* CLDAPv3Plugin::MapAttrToLDAPType ( const char *inRecType, const char *inAttrType, uInt32 inConfigTableIndex, int inIndex, CLDAPv3Configs *inConfigFromXML, bool bSkipLiteralMappings )
{
	char				   *outResult	= nil;
	uInt32					uiStrLen	= 0;
	uInt32					uiNativeLen	= ::strlen( kDSNativeAttrTypePrefix );
	uInt32					uiStdLen	= ::strlen( kDSStdAttrTypePrefix );
	sLDAPConfigData		   *pConfig		= nil;
	int						aIndex		= inIndex;

	//idea here is to use the inIndex to request a specific map
	//if inIndex is 1 then the first map will be returned
	//if inIndex is >= 1 and <= totalCount then that map will be returned
	//if inIndex is <= 0 then nil will be returned
	//if inIndex is > totalCount nil will be returned
	//note the inIndex will reference the inIndexth entry ie. start at 1
	//caller can increment the inIndex starting at one and get maps until nil is returned

	if (( inAttrType != nil ) && (inIndex > 0))
	{
		uiStrLen = ::strlen( inAttrType );

		// First look for native attribute type
		if ( ::strncmp( inAttrType, kDSNativeAttrTypePrefix, uiNativeLen ) == 0 )
		{
			// Make sure we have data past the prefix
			if (( uiStrLen > uiNativeLen ) && (inIndex == 1))
			{
				uiStrLen = uiStrLen - uiNativeLen;
				outResult = new char[ uiStrLen + 1 ];
				::strcpy( outResult, inAttrType + uiNativeLen );
			}
		} // native maps
		else if ( ::strncmp( inAttrType, kDSStdAttrTypePrefix, uiStdLen ) == 0 )
		{
			//find the attr map that we need using inConfigTableIndex
			if (( inConfigTableIndex < gLDAPConfigTableLen) && ( inConfigTableIndex >= 0 ))
			{
		        pConfig = (sLDAPConfigData *)gLDAPConfigTable->GetItemData( inConfigTableIndex );
		        if (pConfig != nil)
		        {
					if (inConfigFromXML != nil)
					{
						//TODO need to "try" to get a default here if no mappings
						//KW maybe NOT ie. directed open can work with native types
						if ( (pConfig->fRecordTypeMapCFArray == 0) || (pConfig->fAttrTypeMapCFArray == 0) )
						{
						}
						else if (	( CFArrayGetTypeID() == CFGetTypeID( pConfig->fRecordTypeMapCFArray ) ) &&
								( CFArrayGetTypeID() == CFGetTypeID( pConfig->fAttrTypeMapCFArray ) ) )
						{
							if (bSkipLiteralMappings)
							{
								bool bKeepLooking = false;
								do
								{
									bKeepLooking = false;
									outResult = inConfigFromXML->ExtractAttrMap(inRecType, inAttrType, pConfig->fRecordTypeMapCFArray, pConfig->fAttrTypeMapCFArray, aIndex );
									//if not used, ie. static, then free outResult
									if ( (outResult != nil) && (outResult[0] == '#') )
									{
										free(outResult);
										outResult = nil;
										bKeepLooking = true;
									}
									aIndex++;
								}
								while ( bKeepLooking );
							}
							else
							{
								outResult = inConfigFromXML->ExtractAttrMap(inRecType, inAttrType, pConfig->fRecordTypeMapCFArray, pConfig->fAttrTypeMapCFArray, inIndex );
							}
						}
					}
	        	}
			}
		}//standard maps
        //passthrough since we don't know what to do with it
        //and we assume the DS API client knows that the LDAP server
        //can handle this attribute type
		else
		{
			if ( inIndex == 1 )
			{
				outResult = new char[ 1 + ::strlen( inAttrType ) ];
				::strcpy( outResult, inAttrType );
			}
		}// passthrough map
	}// if (( inAttrType != nil ) && (inIndex > 0))

	return( outResult );

} // MapAttrToLDAPType


// ---------------------------------------------------------------------------
//	* MapAttrToLDAPTypeArray
// ---------------------------------------------------------------------------

char** CLDAPv3Plugin::MapAttrToLDAPTypeArray ( const char *inRecType, const char *inAttrType, uInt32 inConfigTableIndex, CLDAPv3Configs *inConfigFromXML )
{
	char				  **outResult	= nil;
	uInt32					uiStrLen	= 0;
	uInt32					uiNativeLen	= ::strlen( kDSNativeAttrTypePrefix );
	uInt32					uiStdLen	= ::strlen( kDSStdAttrTypePrefix );
	sLDAPConfigData		   *pConfig		= nil;
	int						countNative	= 0;

	if ( inAttrType != nil )
	{
		uiStrLen = ::strlen( inAttrType );

		// First look for native attribute type
		if ( ::strncmp( inAttrType, kDSNativeAttrTypePrefix, uiNativeLen ) == 0 )
		{
			// Make sure we have data past the prefix
			if ( uiStrLen > uiNativeLen )
			{
				uiStrLen = uiStrLen - uiNativeLen;
				outResult = (char **)::calloc( 2, sizeof( char * ) );
				*outResult = new char[ uiStrLen + 1 ];
				::strcpy( *outResult, inAttrType + uiNativeLen );
			}
		} // native maps
		else if ( ::strncmp( inAttrType, kDSStdAttrTypePrefix, uiStdLen ) == 0 )
		{
			//find the attr map that we need using inConfigTableIndex
			if (( inConfigTableIndex < gLDAPConfigTableLen) && ( inConfigTableIndex >= 0 ))
			{
		        pConfig = (sLDAPConfigData *)gLDAPConfigTable->GetItemData( inConfigTableIndex );
		        if (pConfig != nil)
		        {
					if (inConfigFromXML != nil)
					{
						//TODO need to "try" to get a default here if no mappings
						//KW maybe NOT ie. directed open can work with native types
						
						char   *singleMap	= nil;
						int		aIndex		= 1;
						int		usedIndex	= 0;

						if ( (pConfig->fRecordTypeMapCFArray == 0) || (pConfig->fAttrTypeMapCFArray == 0) )
						{
						}
						else if (	( CFArrayGetTypeID() == CFGetTypeID( pConfig->fRecordTypeMapCFArray ) ) &&
								( CFArrayGetTypeID() == CFGetTypeID( pConfig->fAttrTypeMapCFArray ) ) )
						{
							//need to know number of native maps first to alloc the outResult
							countNative = inConfigFromXML->AttrMapsCount(inRecType, inAttrType, pConfig->fRecordTypeMapCFArray, pConfig->fAttrTypeMapCFArray);
							if (countNative > 0)
							{
								outResult = (char **)::calloc( countNative + 1, sizeof(char *) );
								do
								{
									singleMap = inConfigFromXML->ExtractAttrMap(inRecType, inAttrType, pConfig->fRecordTypeMapCFArray, pConfig->fAttrTypeMapCFArray, aIndex );
									if (singleMap != nil)
									{
										if (singleMap[0] != '#')
										{
											outResult[usedIndex] = singleMap; //usedIndex is zero based
											usedIndex++;
										}
										else //don't use the literal mapping
										{
											free(singleMap);
											//singleMap = nil; //don't reset since needed for while condition below
										}
									}
									aIndex++;
								}
								while (singleMap != nil);
							}
						}
					}
	        	}
			}
			
		}//standard maps
        //passthrough since we don't know what to do with it
        //and we assume the DS API client knows that the LDAP server
        //can handle this attribute type
		else
		{
			outResult = (char **)::calloc( 2, sizeof(char *) );
			*outResult = new char[ 1 + ::strlen( inAttrType ) ];
			::strcpy( *outResult, inAttrType );
		}// passthrough map
	}// if ( inAttrType != nil )

	return( outResult );

} // MapAttrToLDAPTypeArray


// ---------------------------------------------------------------------------
//	* MapAttrListToLDAPTypeArray
// ---------------------------------------------------------------------------

char** CLDAPv3Plugin::MapAttrListToLDAPTypeArray ( char *inRecType, CAttributeList *inAttrTypeList, uInt32 inConfigTableIndex, CLDAPv3Configs *inConfigFromXML )
{
	char				  **outResult		= nil;
	char				  **mvResult		= nil;
	uInt32					uiStrLen		= 0;
	uInt32					uiNativeLen		= ::strlen( kDSNativeAttrTypePrefix );
	uInt32					uiStdLen		= ::strlen( kDSStdAttrTypePrefix );
	sLDAPConfigData		   *pConfig			= nil;
	int						countNative		= 0;
	char				   *pAttrType		= nil;
	sInt32					attrTypeIndex	= 1;

	//TODO can we optimize allocs using a STL set and then creating the char** at the end
	while ( inAttrTypeList->GetAttribute( attrTypeIndex++, &pAttrType ) == eDSNoErr )
	{
		//deal with the special requests for all attrs here
		//if any of kDSAttributesAll, kDSAttributesNativeAll, or kDSAttributesStandardAll
		//are found anywhere in the list then we retrieve everything in the ldap_search call
		if (	( strcmp(pAttrType,kDSAttributesAll) == 0 ) ||
				( strcmp(pAttrType,kDSAttributesNativeAll) == 0 ) ||
				( strcmp(pAttrType,kDSAttributesStandardAll) == 0 ) )
		{
			if (outResult != nil)
			{
				for (int ourIndex=0; ourIndex < countNative; ourIndex++) //remove existing
				{
					free(outResult[ourIndex]);
					outResult[ourIndex] = nil;
				}
				free(outResult);
			}
			return(nil);
		}
		
		uiStrLen = ::strlen( pAttrType );

		// First look for native attribute type
		if ( ::strncmp( pAttrType, kDSNativeAttrTypePrefix, uiNativeLen ) == 0 )
		{
			// Make sure we have data past the prefix
			if ( uiStrLen > uiNativeLen )
			{
				uiStrLen = uiStrLen - uiNativeLen;
				if (outResult != nil)
				{
					mvResult = outResult;
					outResult = (char **)::calloc( countNative + 2, sizeof( char * ) );
					for (int oldIndex=0; oldIndex < countNative; oldIndex++) //transfer existing
					{
						outResult[oldIndex] = mvResult[oldIndex];
					}
				}
				else
				{
					outResult = (char **)::calloc( 2, sizeof( char * ) );
				}
				outResult[countNative] = new char[ uiStrLen + 1 ];
				::strcpy( outResult[countNative], pAttrType + uiNativeLen );
				countNative++;
				if (mvResult != nil)
				{
					free(mvResult);
					mvResult = nil;
				}
			}
		} // native maps
		else if ( ::strncmp( pAttrType, kDSStdAttrTypePrefix, uiStdLen ) == 0 )
		{
			//find the attr map that we need using inConfigTableIndex
			if (( inConfigTableIndex < gLDAPConfigTableLen) && ( inConfigTableIndex >= 0 ))
			{
		        pConfig = (sLDAPConfigData *)gLDAPConfigTable->GetItemData( inConfigTableIndex );
		        if (pConfig != nil)
		        {
					if (inConfigFromXML != nil)
					{
						//TODO need to "try" to get a default here if no mappings
						//KW maybe NOT ie. directed open can work with native types
						
						char   *singleMap	= nil;
						int		aIndex		= 1;
						int		mapCount	= 0;
						int		usedMapCount= 0;

						if ( (pConfig->fRecordTypeMapCFArray == 0) || (pConfig->fAttrTypeMapCFArray == 0) )
						{
						}
						else if (	( CFArrayGetTypeID() == CFGetTypeID( pConfig->fRecordTypeMapCFArray ) ) &&
								( CFArrayGetTypeID() == CFGetTypeID( pConfig->fAttrTypeMapCFArray ) ) )
						{
							//need to know number of native maps first to alloc the outResult
							mapCount = inConfigFromXML->AttrMapsCount(inRecType, pAttrType, pConfig->fRecordTypeMapCFArray, pConfig->fAttrTypeMapCFArray);
							if (mapCount > 0)
							{
								if (outResult != nil)
								{
									mvResult = outResult;
									outResult = (char **)::calloc( countNative + mapCount + 1, sizeof( char * ) );
									for (int oldIndex=0; oldIndex < countNative; oldIndex++) //transfer existing
									{
										outResult[oldIndex] = mvResult[oldIndex];
									}
								}
								else
								{
									outResult = (char **)::calloc( mapCount + 1, sizeof( char * ) );
								}
								do
								{
									singleMap = inConfigFromXML->ExtractAttrMap(inRecType, pAttrType, pConfig->fRecordTypeMapCFArray, pConfig->fAttrTypeMapCFArray, aIndex );
									if (singleMap != nil)
									{
										if (singleMap[0] != '#')
										{
											outResult[countNative + usedMapCount] = singleMap; //usedMapCount is zero based
											usedMapCount++;
										}
										else //don't use the literal mapping
										{
											free(singleMap);
											//singleMap = nil; //don't reset since needed for while condition below
										}
									}
									aIndex++;
								}
								while (singleMap != nil);
								countNative += usedMapCount;
								if (mvResult != nil)
								{
									free(mvResult);
									mvResult = nil;
								}
							}
						}
					}
	        	}
			}
			
		}//standard maps
        //passthrough since we don't know what to do with it
        //and we assume the DS API client knows that the LDAP server
        //can handle this attribute type
		else
		{
			if (outResult != nil)
			{
				mvResult = outResult;
				outResult = (char **)::calloc( countNative + 2, sizeof( char * ) );
				for (int oldIndex=0; oldIndex < countNative; oldIndex++) //transfer existing
				{
					outResult[oldIndex] = mvResult[oldIndex];
				}
			}
			else
			{
				outResult = (char **)::calloc( 2, sizeof( char * ) );
			}
			outResult[countNative] = new char[ strlen( pAttrType ) + 1 ];
			::strcpy( outResult[countNative], pAttrType );
			countNative++;
			if (mvResult != nil)
			{
				free(mvResult);
				mvResult = nil;
			}
		}// passthrough map
	}// while ( inAttrTypeList->GetAttribute( attrTypeIndex++, &pAttrType ) == eDSNoErr )

	return( outResult );

} // MapAttrListToLDAPTypeArray


// ---------------------------------------------------------------------------
//	* MapLDAPWriteErrorToDS
//
//		- convert LDAP error to DS error
// ---------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::MapLDAPWriteErrorToDS ( sInt32 inLDAPError, sInt32 inDefaultError )
{
	sInt32		siOutError	= 0;

	switch ( inLDAPError )
	{
		case LDAP_SUCCESS:
			siOutError = eDSNoErr;
			break;
		case LDAP_AUTH_UNKNOWN:
		case LDAP_AUTH_METHOD_NOT_SUPPORTED:
			siOutError = eDSAuthMethodNotSupported;
			break;
		case LDAP_INAPPROPRIATE_AUTH:
		case LDAP_INVALID_CREDENTIALS:
		case LDAP_INSUFFICIENT_ACCESS:
		case LDAP_STRONG_AUTH_REQUIRED:
			siOutError = eDSPermissionError;
			break;
		case LDAP_NO_SUCH_ATTRIBUTE:
			siOutError = eDSAttributeNotFound;
			break;
		case LDAP_NO_SUCH_OBJECT:
			siOutError = eDSRecordNotFound;
			break;
		case LDAP_NO_MEMORY:
			siOutError = eMemoryError;
			break;
		case LDAP_TIMEOUT:
			siOutError = eDSServerTimeout;
			break;
		case LDAP_NAMING_VIOLATION:
		case LDAP_OBJECT_CLASS_VIOLATION:
		case LDAP_CONSTRAINT_VIOLATION:
			siOutError = eDSSchemaError;
			break;
		case LDAP_SERVER_DOWN:
			siOutError = eDSCannotAccessSession;
			break;
		case LDAP_UNDEFINED_TYPE:
			siOutError = eDSInvalidAttributeType;
			break;
			
		default:
		/*
		Remaining errors not yet mapped
		case LDAP_INAPPROPRIATE_MATCHING:
		case LDAP_TYPE_OR_VALUE_EXISTS:
		case LDAP_INVALID_SYNTAX:
		case LDAP_OPERATIONS_ERROR:
		case LDAP_PROTOCOL_ERROR:
		case LDAP_TIMELIMIT_EXCEEDED:
		case LDAP_SIZELIMIT_EXCEEDED:
		case LDAP_COMPARE_FALSE:
		case LDAP_COMPARE_TRUE:
		case LDAP_PARTIAL_RESULTS:
		case LDAP_REFERRAL:
		case LDAP_ADMINLIMIT_EXCEEDED
		case LDAP_UNAVAILABLE_CRITICAL_EXTENSION
		case LDAP_CONFIDENTIALITY_REQUIRED
		case LDAP_SASL_BIND_IN_PROGRESS
		case LDAP_ALIAS_PROBLEM
		case LDAP_INVALID_DN_SYNTAX
		case LDAP_IS_LEAF
		case LDAP_ALIAS_DEREF_PROBLEM

		case LDAP_BUSY
		case LDAP_UNAVAILABLE
		case LDAP_UNWILLING_TO_PERFORM
		case LDAP_LOOP_DETECT

		case LDAP_NOT_ALLOWED_ON_NONLEAF
		case LDAP_NOT_ALLOWED_ON_RDN
		case LDAP_ALREADY_EXISTS
		case LDAP_NO_OBJECT_CLASS_MODS
		case LDAP_RESULTS_TOO_LARGE
		case LDAP_AFFECTS_MULTIPLE_DSAS

		case LDAP_OTHER

		case LDAP_LOCAL_ERROR
		case LDAP_ENCODING_ERROR
		case LDAP_DECODING_ERROR
		case LDAP_FILTER_ERROR
		case LDAP_USER_CANCELLED
		case LDAP_PARAM_ERROR

		case LDAP_CONNECT_ERROR
		case LDAP_NOT_SUPPORTED
		case LDAP_CONTROL_NOT_FOUND
		case LDAP_NO_RESULTS_RETURNED
		case LDAP_MORE_RESULTS_TO_RETURN
		case LDAP_CLIENT_LOOP
		case LDAP_REFERRAL_LIMIT_EXCEEDED*/
			siOutError = inDefaultError;
			break;
	}
	DBGLOG1( kLogPlugin, "LDAP PlugIn: error code %d returned by LDAP framework", inLDAPError );

	return( siOutError );

} // MapNetInfoErrors


//------------------------------------------------------------------------------------
//	* GetAttributeEntry
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::GetAttributeEntry ( sGetAttributeEntry *inData )
{
	sInt32					siResult			= eDSNoErr;
	uInt16					usAttrTypeLen		= 0;
	uInt16					usAttrCnt			= 0;
	uInt32					usAttrLen			= 0;
	uInt16					usValueCnt			= 0;
	uInt32					usValueLen			= 0;
	uInt32					i					= 0;
	uInt32					uiIndex				= 0;
	uInt32					uiAttrEntrySize		= 0;
	uInt32					uiOffset			= 0;
	uInt32					uiTotalValueSize	= 0;
	uInt32					offset				= 4;
	uInt32					buffSize			= 0;
	uInt32					buffLen				= 0;
	char				   *p			   		= nil;
	char				   *pAttrType	   		= nil;
	tDataBufferPtr			pDataBuff			= nil;
	tAttributeEntryPtr		pAttribInfo			= nil;
	sLDAPContextData		   *pAttrContext		= nil;
	sLDAPContextData		   *pValueContext		= nil;

	try
	{
		if ( inData  == nil ) throw( (sInt32) eMemoryError );

		pAttrContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInAttrListRef );
		if ( pAttrContext  == nil ) throw( (sInt32)eDSBadContextData );

		uiIndex = inData->fInAttrInfoIndex;
		if (uiIndex == 0) throw( (sInt32)eDSInvalidIndex );
				
		pDataBuff = inData->fInOutDataBuff;
		if ( pDataBuff  == nil ) throw( (sInt32)eDSNullDataBuff );
		
		buffSize	= pDataBuff->fBufferSize;
		//buffLen		= pDataBuff->fBufferLength;
		//here we can't use fBufferLength for the buffLen SINCE the buffer is packed at the END of the data block
		//and the fBufferLength is the overall length of the data for all blocks at the end of the data block
		//the value ALSO includes the bookkeeping data at the start of the data block
		//so we need to read it here

		p		= pDataBuff->fBufferData + pAttrContext->offset;
		offset	= pAttrContext->offset;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (offset + 2 > buffSize)  throw( (sInt32)eDSInvalidBuffFormat );
				
		// Get the attribute count
		::memcpy( &usAttrCnt, p, 2 );
		if (uiIndex > usAttrCnt) throw( (sInt32)eDSInvalidIndex );

		// Move 2 bytes
		p		+= 2;
		offset	+= 2;

		// Skip to the attribute that we want
		for ( i = 1; i < uiIndex; i++ )
		{
			// Do record check, verify that offset is not past end of buffer, etc.
			if (offset + 4 > buffSize)  throw( (sInt32)eDSInvalidBuffFormat );
		
			// Get the length for the attribute
			::memcpy( &usAttrLen, p, 4 );

			// Move the offset past the length word and the length of the data
			p		+= 4 + usAttrLen;
			offset	+= 4 + usAttrLen;
		}

		// Get the attribute offset
		uiOffset = offset;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (offset + 4 > buffSize)  throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the length for the attribute block
		::memcpy( &usAttrLen, p, 4 );

		// Skip past the attribute length
		p		+= 4;
		offset	+= 4;

		//set the bufLen to stricter range
		buffLen = offset + usAttrLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (offset + 2 > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the length for the attribute type
		::memcpy( &usAttrTypeLen, p, 2 );
		
		pAttrType = p + 2;
		p		+= 2 + usAttrTypeLen;
		offset	+= 2 + usAttrTypeLen;
		
		// Do record check, verify that offset is not past end of buffer, etc.
		if (offset + 2 > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get number of values for this attribute
		::memcpy( &usValueCnt, p, 2 );
		
		p		+= 2;
		offset	+= 2;
		
		for ( i = 0; i < usValueCnt; i++ )
		{
			// Do record check, verify that offset is not past end of buffer, etc.
			if (offset + 4 > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
			// Get the length for the value
			::memcpy( &usValueLen, p, 4 );
			
			p		+= 4 + usValueLen;
			offset	+= 4 + usValueLen;
			
			uiTotalValueSize += usValueLen;
		}

		uiAttrEntrySize = sizeof( tAttributeEntry ) + usAttrTypeLen + kBuffPad;
		pAttribInfo = (tAttributeEntry *)::calloc( 1, uiAttrEntrySize );

		pAttribInfo->fAttributeValueCount				= usValueCnt;
		pAttribInfo->fAttributeDataSize					= uiTotalValueSize;
		pAttribInfo->fAttributeValueMaxSize				= 512;				// KW this is not used anywhere
		pAttribInfo->fAttributeSignature.fBufferSize	= usAttrTypeLen + kBuffPad;
		pAttribInfo->fAttributeSignature.fBufferLength	= usAttrTypeLen;
		::memcpy( pAttribInfo->fAttributeSignature.fBufferData, pAttrType, usAttrTypeLen );

		pValueContext = MakeContextData();
		if ( pValueContext  == nil ) throw( (sInt32) eMemoryAllocError );

		pValueContext->offset = uiOffset;

		gLDAPContextTable->AddItem( inData->fOutAttrValueListRef, pValueContext );

		inData->fOutAttrInfoPtr = pAttribInfo;
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // GetAttributeEntry


//------------------------------------------------------------------------------------
//	* GetAttributeValue
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::GetAttributeValue ( sGetAttributeValue *inData )
{
	sInt32						siResult		= eDSNoErr;
	uInt16						usValueCnt		= 0;
	uInt32						usValueLen		= 0;
	uInt16						usAttrNameLen	= 0;
	uInt32						i				= 0;
	uInt32						uiIndex			= 0;
	uInt32						offset			= 0;
	char					   *p				= nil;
	tDataBuffer				   *pDataBuff		= nil;
	tAttributeValueEntry	   *pAttrValue		= nil;
	sLDAPContextData		   *pValueContext	= nil;
	uInt32						buffSize		= 0;
	uInt32						buffLen			= 0;
	uInt32						attrLen			= 0;

	try
	{
		pValueContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInAttrValueListRef );
		if ( pValueContext  == nil ) throw( (sInt32)eDSBadContextData );

		uiIndex = inData->fInAttrValueIndex;
		if (uiIndex == 0) throw( (sInt32)eDSInvalidIndex );
		
		pDataBuff = inData->fInOutDataBuff;
		if ( pDataBuff  == nil ) throw( (sInt32)eDSNullDataBuff );

		buffSize	= pDataBuff->fBufferSize;
		//buffLen		= pDataBuff->fBufferLength;
		//here we can't use fBufferLength for the buffLen SINCE the buffer is packed at the END of the data block
		//and the fBufferLength is the overall length of the data for all blocks at the end of the data block
		//the value ALSO includes the bookkeeping data at the start of the data block
		//so we need to read it here

		p		= pDataBuff->fBufferData + pValueContext->offset;
		offset	= pValueContext->offset;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (offset + 4 > buffSize)  throw( (sInt32)eDSInvalidBuffFormat );
				
		// Get the buffer length
		::memcpy( &attrLen, p, 4 );

		//now add the offset to the attr length for the value of buffLen to be used to check for buffer overruns
		//AND add the length of the buffer length var as stored ie. 4 bytes
		buffLen		= attrLen + pValueContext->offset + 4;
		if (buffLen > buffSize)  throw( (sInt32)eDSInvalidBuffFormat );

		// Skip past the attribute length
		p		+= 4;
		offset	+= 4;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (offset + 2 > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the attribute name length
		::memcpy( &usAttrNameLen, p, 2 );
		
		p		+= 2 + usAttrNameLen;
		offset	+= 2 + usAttrNameLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (offset + 2 > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the value count
		::memcpy( &usValueCnt, p, 2 );
		
		p		+= 2;
		offset	+= 2;

		if (uiIndex > usValueCnt)  throw( (sInt32)eDSInvalidIndex );

		// Skip to the value that we want
		for ( i = 1; i < uiIndex; i++ )
		{
			// Do record check, verify that offset is not past end of buffer, etc.
			if (offset + 4 > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
			// Get the length for the value
			::memcpy( &usValueLen, p, 4 );
			
			p		+= 4 + usValueLen;
			offset	+= 4 + usValueLen;
		}

		// Do record check, verify that offset is not past end of buffer, etc.
		if (offset + 4 > buffLen)  throw( (sInt32)eDSInvalidBuffFormat );
		
		::memcpy( &usValueLen, p, 4 );
		
		p		+= 4;
		offset	+= 4;

		//if (usValueLen == 0)  throw( (sInt32)eDSInvalidBuffFormat ); //if zero is it okay?

		pAttrValue = (tAttributeValueEntry *)::calloc( 1, sizeof( tAttributeValueEntry ) + usValueLen + kBuffPad );

		pAttrValue->fAttributeValueData.fBufferSize		= usValueLen + kBuffPad;
		pAttrValue->fAttributeValueData.fBufferLength	= usValueLen;
		
		// Do record check, verify that offset is not past end of buffer, etc.
		if ( offset + usValueLen > buffLen )throw( (sInt32)eDSInvalidBuffFormat );
		
		::memcpy( pAttrValue->fAttributeValueData.fBufferData, p, usValueLen );

		// Set the attribute value ID
		pAttrValue->fAttributeValueID = CalcCRC( pAttrValue->fAttributeValueData.fBufferData );

		inData->fOutAttrValue = pAttrValue;
			
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // GetAttributeValue



// ---------------------------------------------------------------------------
//	* CalcCRC
// ---------------------------------------------------------------------------

uInt32 CLDAPv3Plugin::CalcCRC ( char *inStr )
{
	char		   *p			= inStr;
	sInt32			siI			= 0;
	sInt32			siStrLen	= 0;
	uInt32			uiCRC		= 0xFFFFFFFF;
	CRCCalc			aCRCCalc;

	if ( inStr != nil )
	{
		siStrLen = ::strlen( inStr );

		for ( siI = 0; siI < siStrLen; ++siI )
		{
			uiCRC = aCRCCalc.UPDC32( *p, uiCRC );
			p++;
		}
	}

	return( uiCRC );

} // CalcCRC


//------------------------------------------------------------------------------------
//	* GetTheseRecords
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::GetTheseRecords (	char			   *inConstRecName,
										char			   *inRecType,
										char			   *inNativeRecType,
										tDirPatternMatch	patternMatch,
										CAttributeList	   *inAttrTypeList,
										sLDAPContextData   *inContext,
										sLDAPContinueData  *inContinue,
										bool				inAttrOnly,
										CBuff			   *inBuff,
										uInt32			   &outRecCount,
										bool				inbOCANDGroup,
										CFArrayRef			inOCSearchList,
										ber_int_t			inScope )
{
	sInt32					siResult		= eDSNoErr;
    sInt32					siValCnt		= 0;
    int						ldapReturnCode 	= 0;
    bool					bufferFull		= false;
    LDAPMessage			   *result			= nil;
    char				   *recName			= nil;
    char				   *queryFilter		= nil;
    sLDAPConfigData		   *pConfig			= nil;
    int						searchTO		= 0;
	CDataBuff			   *aAttrData		= nil;
	CDataBuff			   *aRecData		= nil;
	char				  **attrs			= nil;

	//TODO why not optimize and save the queryFilter in the continue data for the next call in?
	
	//build the record query string
	queryFilter = BuildLDAPQueryFilter(	(char *)kDSNAttrRecordName,
										inConstRecName,
										patternMatch,
										inContext->fConfigTableIndex,
										false,
										(const char *)inRecType,
										inNativeRecType,
										inbOCANDGroup,
										inOCSearchList,
                                        pConfigFromXML );
	    
	outRecCount = 0; //need to track how many records were found by this call to GetTheseRecords
	
    try
    {
    
    	if (inContext == nil ) throw( (sInt32)eDSInvalidContext);
    	if (inContinue == nil ) throw( (sInt32)eDSInvalidContinueData);
    	
    	// check to make sure the queryFilter is not nil
    	if ( queryFilter == nil ) throw( (sInt32)eDSNullParameter );
    	
		aAttrData = new CDataBuff();
		if ( aAttrData  == nil ) throw( (sInt32) eMemoryError );
		aRecData = new CDataBuff();
		if ( aRecData  == nil ) throw( (sInt32) eMemoryError );
		
		//retrieve the config data
		//don't need to retrieve for the case of "generic unknown" so don't check index 0
		if (( inContext->fConfigTableIndex < gLDAPConfigTableLen) && ( inContext->fConfigTableIndex >= 1 ))
		{
			pConfig = (sLDAPConfigData *)gLDAPConfigTable->GetItemData( inContext->fConfigTableIndex );
			if (pConfig != nil)
			{
				searchTO	= pConfig->fSearchTimeout;
			}
		}
		
		//LDAP search with rebind capability
		DSDoSearch (	inRecType,
						inNativeRecType,
						inAttrTypeList,
						inContext,
						inContinue,
						pConfigFromXML,
						inScope,
						fLDAPSessionMgr,
						queryFilter,
						ldapReturnCode,
						searchTO,
						result);

		while ( 	( (ldapReturnCode == LDAP_RES_SEARCH_ENTRY) || (ldapReturnCode == LDAP_RES_SEARCH_RESULT) )
					&& !(bufferFull)
					&& (	(inContinue->fTotalRecCount < inContinue->fLimitRecSearch) ||
							(inContinue->fLimitRecSearch == 0) ) )
        {
            // package the record into the DS format into the buffer
            // steps to add an entry record to the buffer
            // build the aRecData header
            // build the aAttrData
            // append the aAttrData to the aRecData
            // add the aRecData to the buffer inBuff

			if (ldapReturnCode == LDAP_RES_SEARCH_ENTRY)
			{
				aRecData->Clear();
	
				if ( inRecType != nil )
				{
					aRecData->AppendShort( ::strlen( inRecType ) );
					aRecData->AppendString( inRecType );
				}
				else
				{
					aRecData->AppendShort( ::strlen( "Record Type Unknown" ) );
					aRecData->AppendString( "Record Type Unknown" );
				}
	
				// need to get the record name
				recName = GetRecordName( inRecType, result, inContext, siResult );
				if ( siResult != eDSNoErr ) throw( siResult );
				if ( recName != nil )
				{
					aRecData->AppendShort( ::strlen( recName ) );
					aRecData->AppendString( recName );
	
					delete ( recName );
					recName = nil;
				}
				else
				{
					aRecData->AppendShort( ::strlen( "Record Name Unknown" ) );
					aRecData->AppendString( "Record Name Unknown" );
				}
	
				// need to calculate the number of attribute types ie. siValCnt
				// also need to extract the attributes and place them into fAttrData
				//siValCnt = 0;
	
				aAttrData->Clear();
				siResult = GetTheseAttributes( inRecType, inAttrTypeList, result, inAttrOnly, inContext, siValCnt, aAttrData );
				if ( siResult != eDSNoErr ) throw( siResult );
				
				//add the attribute info to the aRecData
				if ( siValCnt == 0 )
				{
					// Attribute count
					aRecData->AppendShort( 0 );
				}
				else
				{
					// Attribute count
					aRecData->AppendShort( siValCnt );
					aRecData->AppendBlock( aAttrData->GetData(), aAttrData->GetLength() );
				}
	
				// add the aRecData now to the inBuff
				siResult = inBuff->AddData( aRecData->GetData(), aRecData->GetLength() );
			}
            // need to check if the buffer is full
			// need to handle full buffer and keep the result alive for the next call in
            if (siResult == CBuff::kBuffFull)
            {
                bufferFull = true;
                
                //save the result if buffer is full
                inContinue->pResult = result;
            }
            else if ( siResult == eDSNoErr )
            {
				//only get next result if buffer is not full
                ldap_msgfree( result );
				result = nil;
                
				if (ldapReturnCode == LDAP_RES_SEARCH_ENTRY)
				{
					outRecCount++; //another record added
					inContinue->fTotalRecCount++;
				}
				
                //make sure no result is carried in the context
                inContinue->pResult = nil;

				DSGetSearchLDAPResult (	fLDAPSessionMgr,
										inContext,
										searchTO,
										result,
										LDAP_MSG_ONE,
										inContinue->msgId,
										ldapReturnCode,
										false);
            }
            else
            {
                //make sure no result is carried in the context
                inContinue->pResult = nil;
                throw( (sInt32)eDSInvalidBuffFormat );
            }
            
        } // while loop over entries

		if ( (result != inContinue->pResult) && (result != nil) )
		{
			ldap_msgfree( result );
			result = nil;
		}

    } // try block

    catch ( sInt32 err )
    {
        siResult = err;
    }

	if (attrs != nil)
	{
		int aIndex = 0;
		while (attrs[aIndex] != nil)
		{
			free(attrs[aIndex]);
			attrs[aIndex] = nil;
			aIndex++;
		}
		free(attrs);
		attrs = nil;
	}
	
	if ( queryFilter != nil )
	{
		delete( queryFilter );
		queryFilter = nil;
	}

	if (aAttrData != nil)
	{
		delete (aAttrData);
		aAttrData = nil;
	}
	if (aRecData != nil)
	{
		delete (aRecData);
		aRecData = nil;
	}
			
    return( siResult );

} // GetTheseRecords


//------------------------------------------------------------------------------------
//	* BuildLDAPQueryFilter
//------------------------------------------------------------------------------------

char *CLDAPv3Plugin::BuildLDAPQueryFilter (	char			   *inConstAttrType,
											const char		   *inConstAttrName,
											tDirPatternMatch	patternMatch,
											uInt32				inConfigTableIndex,
											bool				useWellKnownRecType,
											const char		   *inRecType,
											char			   *inNativeRecType,
											bool				inbOCANDGroup,
											CFArrayRef			inOCSearchList,
                                            CLDAPv3Configs	   *inConfigFromXML )
{
    char				   *queryFilter			= nil;
	unsigned long			matchType			= eDSExact;
	char				   *nativeAttrType		= nil;
	uInt32					recNameLen			= 0;
	int						numAttributes		= 1;
	CFMutableStringRef		cfStringRef			= nil;
	CFMutableStringRef		cfQueryStringRef	= nil;
	char				   *escapedName			= nil;
	uInt32					escapedIndex		= 0;
	uInt32					originalIndex		= 0;
	bool					bOnceThru			= false;
	uInt32					offset				= 3;
	uInt32					callocLength		= 0;
	bool					objClassAdded		= false;
	bool					bGetAllDueToLiteralMapping = false;
	int						aOCSearchListCount	= 0;
	
	cfQueryStringRef = CFStringCreateMutable(kCFAllocatorDefault, 0);
	
	//build the objectclass filter prefix condition if available and then set objClassAdded
	//before the original filter on name and native attr types
	//use the inConfigTableIndex to access the mapping config

	//check for nil and then check if this is a standard type so we have a chance there is an objectclass mapping
	if ( (inRecType != nil) && (inNativeRecType != nil) )
	{
		if (strncmp(inRecType,kDSStdRecordTypePrefix,strlen(kDSStdRecordTypePrefix)) == 0)
		{
			//determine here whether or not there are any objectclass mappings to include
			if (inOCSearchList != nil)
			{
				//here we extract the object class strings
				//combine using "&" if inbOCANDGroup otherwise use "|"
				aOCSearchListCount = CFArrayGetCount(inOCSearchList);
				for ( int iOCIndex = 0; iOCIndex < aOCSearchListCount; iOCIndex++ )
				{
					CFStringRef	ocString;
					ocString = (CFStringRef)::CFArrayGetValueAtIndex( inOCSearchList, iOCIndex );
					if (ocString != nil)
					{
						if (!objClassAdded)
						{
							objClassAdded = true;
							if (inbOCANDGroup)
							{
								CFStringAppendCString(cfQueryStringRef,"(&", kCFStringEncodingUTF8);
							}
							else
							{
								CFStringAppendCString(cfQueryStringRef,"(&(|", kCFStringEncodingUTF8);
							}
						}
						
						CFStringAppendCString(cfQueryStringRef, "(objectclass=", kCFStringEncodingUTF8);

						// do we need to escape any of the characters internal to the CFString??? like before
						// NO since "*, "(", and ")" are not legal characters for objectclass names
						CFStringAppend(cfQueryStringRef, ocString);
						
						CFStringAppendCString(cfQueryStringRef, ")", kCFStringEncodingUTF8);
					}
				}// loop over the objectclasses CFArray
				if (CFStringGetLength(cfQueryStringRef) != 0)
				{
					if (!inbOCANDGroup)
					{
						//( so PB bracket completion works
						CFStringAppendCString(cfQueryStringRef,")", kCFStringEncodingUTF8);
					}
				}
			}
		}// if (strncmp(inRecType,kDSStdRecordTypePrefix,strlen(kDSStdRecordTypePrefix)) == 0)
	}// if ( (inRecType != nil) && (inNativeRecType != nil) )
	
	//check for case of NO objectclass mapping BUT also no inConstAttrName meaning we want to have
	//the result of (objectclass=*)
	if ( (CFStringGetLength(cfQueryStringRef) == 0) && (inConstAttrName == nil) )
	{
		CFStringAppendCString(cfQueryStringRef,"(&(objectclass=*)", kCFStringEncodingUTF8);
		objClassAdded = true;
	}
	
	//here we decide if this is eDSCompoundExpression or eDSiCompoundExpression so that we special case this
	if (	(patternMatch == eDSCompoundExpression) ||
			(patternMatch == eDSiCompoundExpression) )
	{ //KW right now it is always case insensitive
		cfStringRef = ParseCompoundExpression(inConstAttrName, inRecType, inConfigTableIndex, inConfigFromXML);

		if (cfStringRef != nil)
		{
			CFStringAppend(cfQueryStringRef, cfStringRef);
			CFRelease(cfStringRef);
			cfStringRef = nil;
		}
			
	}
	else
	{
		//first check to see if input not nil
		if (inConstAttrName != nil)
		{
			recNameLen = strlen(inConstAttrName);
			escapedName = (char *)::calloc(1, 2 * recNameLen + 1);
			// assume at most all characters will be escaped
			while (originalIndex < recNameLen)
			{
				switch (inConstAttrName[originalIndex])
				{
					case '*':
					case '(':
					case ')':
						escapedName[escapedIndex] = '\\';
						++escapedIndex;
						// add \ escape character then fall through and pick up the original character
					default:
						escapedName[escapedIndex] = inConstAttrName[originalIndex];
						++escapedIndex;
						break;
				}
				++originalIndex;
			}
			
			//assume that the query is "OR" based ie. meet any of the criteria
			cfStringRef = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, CFSTR("(|"));
			
			//get the first mapping
			numAttributes = 1;
			//KW Note that kDS1RecordName is single valued so we are using kDSNAttrRecordName
			//as a multi-mapped std type which will easily lead to multiple values
			nativeAttrType = MapAttrToLDAPType( inRecType, inConstAttrType, inConfigTableIndex, numAttributes, inConfigFromXML, false );
			//would throw if first nil since no more will be found otherwise proceed until nil
			//however simply set to default LDAP native in this case
			//ie. we are trying regardless if kDSNAttrRecordName is mapped or not
			//whether or not "cn" is a good choice is a different story
			if (nativeAttrType == nil) //only for first mapping
			{
				nativeAttrType = new char[ 3 ];
				::strcpy(nativeAttrType, "cn");
			}
	
			while ( nativeAttrType != nil )
			{
				if (nativeAttrType[0] == '#') //literal mapping
				{
					if (strlen(nativeAttrType) > 1)
					{
						if (DoesThisMatch(escapedName, nativeAttrType+1, patternMatch))
						{
							if (CFStringGetLength(cfQueryStringRef) == 0)
							{
								CFStringAppendCString(cfQueryStringRef,"(&(objectclass=*)", kCFStringEncodingUTF8);
								objClassAdded = true;
							}
							bGetAllDueToLiteralMapping = true;
							free(nativeAttrType);
							nativeAttrType = nil;
							continue;
						}
					}
				}
				else
				{
					if (bOnceThru)
					{
						if (useWellKnownRecType)
						{
							//need to allow for condition that we want only a single query
							//that uses only a single native type - use the first one - 
							//for perhaps an open record or a write to a specific record
							bOnceThru = false;
							break;
						}
						offset = 0;
					}
					matchType = (unsigned long) (patternMatch);
					switch (matchType)
					{
				//		case eDSAnyMatch:
				//			cout << "Pattern match type of <eDSAnyMatch>" << endl;
				//			break;
						case eDSStartsWith:
						case eDSiStartsWith:
							CFStringAppendCString(cfStringRef,"(", kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef, nativeAttrType, kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef,"=", kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef, escapedName, kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef,"*)", kCFStringEncodingUTF8);
							bOnceThru = true;
							break;
						case eDSEndsWith:
						case eDSiEndsWith:
							CFStringAppendCString(cfStringRef,"(", kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef, nativeAttrType, kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef,"=*", kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef, escapedName, kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef,")", kCFStringEncodingUTF8);
							bOnceThru = true;
							break;
						case eDSContains:
						case eDSiContains:
							CFStringAppendCString(cfStringRef,"(", kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef, nativeAttrType, kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef,"=*", kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef, escapedName, kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef,"*)", kCFStringEncodingUTF8);
							bOnceThru = true;
							break;
						case eDSWildCardPattern:
						case eDSiWildCardPattern:
							//assume the inConstAttrName is wild
							CFStringAppendCString(cfStringRef,"(", kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef, nativeAttrType, kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef,"=", kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef, inConstAttrName, kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef,")", kCFStringEncodingUTF8);
							bOnceThru = true;
							break;
						case eDSRegularExpression:
						case eDSiRegularExpression:
							//assume inConstAttrName replaces entire wild expression
							CFStringAppendCString(cfStringRef, inConstAttrName, kCFStringEncodingUTF8);
							bOnceThru = true;
							break;
						case eDSExact:
						case eDSiExact:
						default:
							CFStringAppendCString(cfStringRef,"(", kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef, nativeAttrType, kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef,"=", kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef, escapedName, kCFStringEncodingUTF8);
							CFStringAppendCString(cfStringRef,")", kCFStringEncodingUTF8);
							bOnceThru = true;
							break;
					} // switch on matchType
				}
				//cleanup nativeAttrType if needed
				if (nativeAttrType != nil)
				{
					free(nativeAttrType);
					nativeAttrType = nil;
				}
				numAttributes++;
				//get the next mapping
				nativeAttrType = MapAttrToLDAPType( inRecType, inConstAttrType, inConfigTableIndex, numAttributes, inConfigFromXML, false );
			} // while ( nativeAttrType != nil )
	
			if (!bGetAllDueToLiteralMapping)
			{
				if (cfStringRef != nil)
				{
					// building search like "sn=name"
					if (offset == 3)
					{
						CFRange	aRangeToDelete;
						aRangeToDelete.location = 1;
						aRangeToDelete.length = 2;			
						CFStringDelete(cfStringRef, aRangeToDelete);
					}
					//building search like "(|(sn=name)(cn=name))"
					else
					{
						CFStringAppendCString(cfStringRef,")", kCFStringEncodingUTF8);
					}
					
					CFStringAppend(cfQueryStringRef, cfStringRef);
				}
			}
			
			if (cfStringRef != nil)
			{
				CFRelease(cfStringRef);
				cfStringRef = nil;
			}

			if (escapedName != nil)
			{
				free(escapedName);
				escapedName = nil;
			}
	
		} // if inConstAttrName not nil
	}
	if (objClassAdded)
	{
		CFStringAppendCString(cfQueryStringRef,")", kCFStringEncodingUTF8);
	}
	
	//here we make the char * output in queryfilter
	if (CFStringGetLength(cfQueryStringRef) != 0)
	{
		callocLength = (uInt32) CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfQueryStringRef), kCFStringEncodingUTF8) + 1;
		queryFilter = (char *) calloc(1, callocLength);
		CFStringGetCString( cfQueryStringRef, queryFilter, callocLength, kCFStringEncodingUTF8 );
	}

	if (cfQueryStringRef != nil)
	{
		CFRelease(cfQueryStringRef);
		cfQueryStringRef = nil;
	}

	return (queryFilter);
	
} // BuildLDAPQueryFilter


// ---------------------------------------------------------------------------
//	* ParseCompoundExpression
// ---------------------------------------------------------------------------

CFMutableStringRef CLDAPv3Plugin::ParseCompoundExpression ( const char *inConstAttrName, const char *inRecType, uInt32 inConfigTableIndex, CLDAPv3Configs *inConfigFromXML )
{
	CFMutableStringRef	outExpression	= NULL;
	char			   *ourExpressionPtr= nil;
	char			   *ourExp			= nil;
	char			   *attrExpression	= nil;
	uInt32				numChars		= 0;
	bool				bNotDone		= true;
	char			   *attrType		= nil;
	char			   *pattMatch		= nil;
	bool				bMultiMap		= false;
	
	//TODO if there is a search on a single attr type with a single literal mapping - how do we find anything?
	//transfer the special chars "(", ")", "|", "&" directly to the outExpression
	//extract the attr type and replace with the correct mappings
	//if std attr type use ALL native attr maps; if native type use alone; if not prefixed assume native attr type
	//extract the pattern match and use it with all the native maps used for a standard attr type
	
	//NOTE many comments with ( or ) have been added solely to get block completion in PB to work

	if ( ( inConstAttrName != nil ) && ( inRecType != nil ) )
	{
		ourExpressionPtr = (char *) calloc(1, strlen(inConstAttrName) + 1);
		if (ourExpressionPtr != nil)
		{
			ourExp = ourExpressionPtr;
			strcpy(ourExpressionPtr, inConstAttrName);
			outExpression = CFStringCreateMutable(kCFAllocatorDefault, 0);
			//get special chars to start
			numChars = strspn(ourExpressionPtr,LDAPCOMEXPSPECIALCHARS); //number of special chars at start
			if (numChars > 0)
			{
				attrExpression = (char *) calloc(1, numChars + 1);
				if (attrExpression != nil)
				{
					strncpy(attrExpression, ourExpressionPtr, numChars);
					CFStringAppendCString(outExpression, attrExpression, kCFStringEncodingUTF8);
					ourExpressionPtr = ourExpressionPtr + numChars;
					free(attrExpression);
					attrExpression = nil;
				}
			}
			
			//loop over the attr types with their pattern match
			while (bNotDone)
			{
				bMultiMap = false;
				numChars = 0;
				numChars = strcspn(ourExpressionPtr,LDAPCOMEXPSPECIALCHARS); //number of chars in "attrtype=pattmatch"
				if (numChars > 0) //another attr Expression found
				{
					attrExpression = (char *) calloc(1, numChars + 1);
					if (attrExpression != nil)
					{
						strncpy(attrExpression, ourExpressionPtr, numChars);
						ourExpressionPtr = ourExpressionPtr + numChars;
						attrType = strsep(&attrExpression,"=");
						pattMatch= strsep(&attrExpression,"=");
						if ((attrType != nil) && (pattMatch != nil) ) //we have something here
						{
							char *pLDAPAttrType = nil;
							int numAttributes = 1;
							pLDAPAttrType = MapAttrToLDAPType( inRecType, attrType, inConfigTableIndex, numAttributes, inConfigFromXML, true );
							if ( pLDAPAttrType != nil)
							{
								if ( (strcasecmp(attrType, pLDAPAttrType) == 0) //no mappings really so reuse input
									|| (strncmp(attrType, kDSNativeAttrTypePrefix, strlen(kDSNativeAttrTypePrefix)) == 0) ) //native type input
								{
									CFStringAppendCString(outExpression, pLDAPAttrType, kCFStringEncodingUTF8);
									CFStringAppendCString(outExpression, "=", kCFStringEncodingUTF8);
									CFStringAppendCString(outExpression, pattMatch, kCFStringEncodingUTF8);
									free(pLDAPAttrType);
									pLDAPAttrType = nil;
								}
								else
								{
									CFStringAppendCString(outExpression, "|", kCFStringEncodingUTF8); //)
									bMultiMap = true; //possible multi map that needs to be accounted for - not always the case ie. could be a single map
								}
							}
							while(pLDAPAttrType != nil)
							{
								CFStringAppendCString(outExpression, "(", kCFStringEncodingUTF8); //)
								CFStringAppendCString(outExpression, pLDAPAttrType, kCFStringEncodingUTF8);
								CFStringAppendCString(outExpression, "=", kCFStringEncodingUTF8);
								CFStringAppendCString(outExpression, pattMatch, kCFStringEncodingUTF8); //(
								numAttributes++;
								free(pLDAPAttrType);
								pLDAPAttrType = nil;
								pLDAPAttrType = MapAttrToLDAPType( inRecType, attrType, inConfigTableIndex, numAttributes, inConfigFromXML, true );
								if (pLDAPAttrType != nil)
								{
									CFStringAppendCString(outExpression, ")", kCFStringEncodingUTF8);
								}
							}
							if (bMultiMap)
							{
								//(
								CFStringAppendCString(outExpression, ")", kCFStringEncodingUTF8);
							}
						} // if ((attrType != nil) && (pattMatch != nil) )
						free(attrExpression);
						attrExpression = nil;
					} //attrExpression != nil
				} //another attr Expression found
				else
				{
					bNotDone = false;
				}
				//get more following special chars
				numChars = 0;
				numChars = strspn(ourExpressionPtr,LDAPCOMEXPSPECIALCHARS); //number of special chars following
				if (numChars > 0)
				{
					attrExpression = (char *) calloc(1, numChars + 1);
					if (attrExpression != nil)
					{
						strncpy(attrExpression, ourExpressionPtr, numChars);
						CFStringAppendCString(outExpression, attrExpression, kCFStringEncodingUTF8);
						ourExpressionPtr = ourExpressionPtr + numChars;
						free(attrExpression);
						attrExpression = nil;
					}
				}
			} // while (bNotDone)
			
			free(ourExp);
			ourExp = nil;
		} //ourExpressionPtr != nil
	}

	if (outExpression != NULL)
	{
		if (CFStringGetLength(outExpression) < 3) //if only "()" then don't return anything
		{
			CFRelease(outExpression);
			outExpression = NULL;
		}
	}

	return( outExpression );

} // ParseCompoundExpression


//------------------------------------------------------------------------------------
//	* GetDirNodeInfo
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::GetDirNodeInfo ( sGetDirNodeInfo *inData )
{
	sInt32				siResult		= eDSNoErr;
	uInt32				uiOffset		= 0;
	uInt32				uiCntr			= 1;
	uInt32				uiAttrCnt		= 0;
	CAttributeList	   *inAttrList		= nil;
	char			   *pAttrName		= nil;
	char			   *pData			= nil;
	sLDAPContextData	   *pContext		= nil;
	sLDAPContextData	   *pAttrContext	= nil;
	CBuff				outBuff;
	CDataBuff		   *aRecData		= nil;
	CDataBuff		   *aAttrData		= nil;
	CDataBuff		   *aTmpData		= nil;
	sLDAPConfigData	   *pConfig			= nil;
    uInt32				iTableIndex		= 0;

// Can extract here the following:
// kDSAttributesAll
// kDSNAttrNodePath
// kDS1AttrReadOnlyNode
// kDSNAttrAuthMethod
// dsAttrTypeStandard:AcountName
// kDSNAttrDefaultLDAPPaths
// kDS1AttrDistinguishedName
//KW need to add mappings info next

	try
	{
		if ( inData  == nil ) throw( (sInt32) eMemoryError );

		pContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInNodeRef );
		if ( pContext  == nil ) throw( (sInt32)eDSBadContextData );

		inAttrList = new CAttributeList( inData->fInDirNodeInfoTypeList );
		if ( inAttrList == nil ) throw( (sInt32)eDSNullNodeInfoTypeList );
		if (inAttrList->GetCount() == 0) throw( (sInt32)eDSEmptyNodeInfoTypeList );

		siResult = outBuff.Initialize( inData->fOutDataBuff, true );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = outBuff.SetBuffType( 'Gdni' );  //can't use 'StdB' since a tRecordEntry is not returned
		if ( siResult != eDSNoErr ) throw( siResult );

		aRecData = new CDataBuff();
		if ( aRecData  == nil ) throw( (sInt32) eMemoryError );
		aAttrData = new CDataBuff();
		if ( aAttrData  == nil ) throw( (sInt32) eMemoryError );
		aTmpData = new CDataBuff();
		if ( aTmpData  == nil ) throw( (sInt32) eMemoryError );

		// Set the record name and type
		aRecData->AppendShort( ::strlen( "dsAttrTypeStandard:DirectoryNodeInfo" ) );
		aRecData->AppendString( (char *)"dsAttrTypeStandard:DirectoryNodeInfo" );
		aRecData->AppendShort( ::strlen( "DirectoryNodeInfo" ) );
		aRecData->AppendString( (char *)"DirectoryNodeInfo" );

		while ( inAttrList->GetAttribute( uiCntr++, &pAttrName ) == eDSNoErr )
		{
			//package up all the dir node attributes dependant upon what was asked for
			if ((::strcmp( pAttrName, kDSAttributesAll ) == 0) ||
				(::strcmp( pAttrName, kDSNAttrNodePath ) == 0) )
			{
				aTmpData->Clear();
				
				uiAttrCnt++;
			
				// Append the attribute name
				aTmpData->AppendShort( ::strlen( kDSNAttrNodePath ) );
				aTmpData->AppendString( kDSNAttrNodePath );

				if ( inData->fInAttrInfoOnly == false )
				{
					// Attribute value count always two
					aTmpData->AppendShort( 2 );
					
					// Append attribute value
					aTmpData->AppendLong( ::strlen( "LDAPv3" ) );
					aTmpData->AppendString( (char *)"LDAPv3" );

					char *tmpStr = nil;
					//don't need to retrieve for the case of "generic unknown" so don't check index 0
					// simply always use the pContext->fName since case of registered it is identical to
					// pConfig->fServerName and in the case of generic it will be correct for what was actually opened
					if (pContext->fName != nil)
					{
						tmpStr = new char[1+::strlen(pContext->fName)];
						::strcpy( tmpStr, pContext->fName );
					}
					else
					{
						tmpStr = new char[1+::strlen("Unknown Node Location")];
						::strcpy( tmpStr, "Unknown Node Location" );
					}
					
					// Append attribute value
					aTmpData->AppendLong( ::strlen( tmpStr ) );
					aTmpData->AppendString( tmpStr );

					delete( tmpStr );

				} // fInAttrInfoOnly is false
				else
				{
					aTmpData->AppendShort( 0 );
				}
				
				// Add the attribute length
				aAttrData->AppendLong( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
                aTmpData->Clear();
			} // kDSAttributesAll or kDSNAttrNodePath
			
			if ( (::strcmp( pAttrName, kDSAttributesAll ) == 0) || 
				 (::strcmp( pAttrName, kDS1AttrReadOnlyNode ) == 0) )
			{
				aTmpData->Clear();

				uiAttrCnt++;

				// Append the attribute name
				aTmpData->AppendShort( ::strlen( kDS1AttrReadOnlyNode ) );
				aTmpData->AppendString( kDS1AttrReadOnlyNode );

				if ( inData->fInAttrInfoOnly == false )
				{
					// Attribute value count
					aTmpData->AppendShort( 1 );

					//possible for a node to be ReadOnly, ReadWrite, WriteOnly
					//note that ReadWrite does not imply fully readable or writable
					
					// Add the root node as an attribute value
					aTmpData->AppendLong( ::strlen( "ReadWrite" ) );
					aTmpData->AppendString( "ReadWrite" );

				}
				else
				{
					aTmpData->AppendShort( 0 );
				}

				// Add the attribute length and data
				aAttrData->AppendLong( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );

				// Clear the temp block
				aTmpData->Clear();
			}
				 
			if ((::strcmp( pAttrName, kDSAttributesAll ) == 0) ||
				(::strcmp( pAttrName, kDSNAttrAuthMethod ) == 0) )
			{
				aTmpData->Clear();
				
				uiAttrCnt++;
			
				//KW at some point need to retrieve SASL auth methods from LDAP server if they are available
			
				// Append the attribute name
				aTmpData->AppendShort( ::strlen( kDSNAttrAuthMethod ) );
				aTmpData->AppendString( kDSNAttrAuthMethod );

				if ( inData->fInAttrInfoOnly == false )
				{
					char *tmpStr = nil;
					
					// Attribute value count
					aTmpData->AppendShort( 4 );
					
					tmpStr = new char[1+::strlen( kDSStdAuthCrypt )];
					::strcpy( tmpStr, kDSStdAuthCrypt );
					
					// Append first attribute value
					aTmpData->AppendLong( ::strlen( tmpStr ) );
					aTmpData->AppendString( tmpStr );

					delete( tmpStr );
					tmpStr = nil;

					tmpStr = new char[1+::strlen( kDSStdAuthClearText )];
					::strcpy( tmpStr, kDSStdAuthClearText );
					
					// Append second attribute value
					aTmpData->AppendLong( ::strlen( tmpStr ) );
					aTmpData->AppendString( tmpStr );

					delete( tmpStr );
					tmpStr = nil;
                                        
					tmpStr = new char[1+::strlen( kDSStdAuthNodeNativeClearTextOK )];
					::strcpy( tmpStr, kDSStdAuthNodeNativeClearTextOK );
					
					// Append third attribute value
					aTmpData->AppendLong( ::strlen( tmpStr ) );
					aTmpData->AppendString( tmpStr );

					delete( tmpStr );
					tmpStr = nil;
					
					tmpStr = new char[1+::strlen( kDSStdAuthNodeNativeNoClearText )];
					::strcpy( tmpStr, kDSStdAuthNodeNativeNoClearText );
					
					// Append fourth attribute value
					aTmpData->AppendLong( ::strlen( tmpStr ) );
					aTmpData->AppendString( tmpStr );

					delete( tmpStr );
					tmpStr = nil;
				} // fInAttrInfoOnly is false
				else
				{
					aTmpData->AppendShort( 0 );
				}

				// Add the attribute length
				aAttrData->AppendLong( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
                aTmpData->Clear();
			
			} // kDSAttributesAll or kDSNAttrAuthMethod

			if ((::strcmp( pAttrName, kDSAttributesAll ) == 0) ||
				(::strcmp( pAttrName, "dsAttrTypeStandard:AccountName" ) == 0) )
			{
				aTmpData->Clear();
				
				uiAttrCnt++;
			
				// Append the attribute name
				aTmpData->AppendShort( ::strlen( "dsAttrTypeStandard:AccountName" ) );
				aTmpData->AppendString( (char *)"dsAttrTypeStandard:AccountName" );

				if ( inData->fInAttrInfoOnly == false )
				{
					char *tmpStr = nil;
		        	if ( (pContext->authCallActive) && (pContext->fUserName != nil) )
		        	{
		        		tmpStr = new char[1+::strlen(pContext->fUserName)];
		        		::strcpy( tmpStr, pContext->fUserName );
	        		}

					if (tmpStr == nil)
					{
						tmpStr = new char[1+::strlen("No Account Name")];
						::strcpy( tmpStr, "No Account Name" );
					}
					
					// Attribute value count
					aTmpData->AppendShort( 1 );
					
					// Append attribute value
					aTmpData->AppendLong( ::strlen( tmpStr ) );
					aTmpData->AppendString( tmpStr );

					delete( tmpStr );

				} // fInAttrInfoOnly is false
				else
				{
					aTmpData->AppendShort( 0 );
				}
				
				// Add the attribute length
				aAttrData->AppendLong( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
                aTmpData->Clear();
			
			} // kDSAttributesAll or dsAttrTypeStandard:AccountName

			if ( ::strcmp( pAttrName, kDSNAttrDefaultLDAPPaths ) == 0 )
			{
				aTmpData->Clear();
				
				uiAttrCnt++;
			
				// Append the attribute name
				aTmpData->AppendShort( ::strlen( kDSNAttrDefaultLDAPPaths ) );
				aTmpData->AppendString( (char *)kDSNAttrDefaultLDAPPaths );

				if ( inData->fInAttrInfoOnly == false )
				{
					uInt32 aDefaultLDAPNodeCount = 0;
					for (iTableIndex=1; iTableIndex<gLDAPConfigTableLen; iTableIndex++)
					{
						pConfig = (sLDAPConfigData *)gLDAPConfigTable->GetItemData( iTableIndex );
						if (pConfig != nil)
						{
							if (pConfig->bUseAsDefaultLDAP)
							{
								if (pConfig->fServerName != nil)
								{
									aDefaultLDAPNodeCount++;
								}
							}
						}
					}

					// Attribute value count
					aTmpData->AppendShort( aDefaultLDAPNodeCount );

					if (aDefaultLDAPNodeCount > 0)
					{
						for (iTableIndex=1; iTableIndex<gLDAPConfigTableLen; iTableIndex++)
						{
							pConfig = (sLDAPConfigData *)gLDAPConfigTable->GetItemData( iTableIndex );
							if (pConfig != nil)
							{
								if (pConfig->bUseAsDefaultLDAP)
								{
									if (pConfig->fServerName != nil)
									{
										char *theDHCPNodeName = (char *) calloc(1, 1+8+strlen(pConfig->fServerName));
										strcpy(theDHCPNodeName, "/LDAPv3/");
										strcat(theDHCPNodeName, pConfig->fServerName);
										// Append attribute value
										aTmpData->AppendLong( strlen( theDHCPNodeName ) );
										aTmpData->AppendString( theDHCPNodeName );
										free(theDHCPNodeName);
										theDHCPNodeName = nil;
									}
								}
							}
						}
					}
				} // fInAttrInfoOnly is false
				else
				{
					aTmpData->AppendShort( 0 );
				}
				
				// Add the attribute length
				aAttrData->AppendLong( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
                aTmpData->Clear();
			
			} // kDSNAttrDefaultLDAPPaths

			if ((::strcmp( pAttrName, kDSAttributesAll ) == 0) ||
				(::strcmp( pAttrName, "kDS1AttrDistinguishedName" ) == 0) )
			{
				aTmpData->Clear();
				
				uiAttrCnt++;
			
				// Append the attribute name
				aTmpData->AppendShort( ::strlen( "kDS1AttrDistinguishedName" ) );
				aTmpData->AppendString( (char *)"kDS1AttrDistinguishedName" );

				if ( inData->fInAttrInfoOnly == false )
				{
					char *tmpStr = nil;
					//have chosen not to carry this UI name around in the context and
					//don't need to retrieve for the case of "generic unknown" so don't check index 0
					if (( pContext->fConfigTableIndex < gLDAPConfigTableLen) && ( pContext->fConfigTableIndex >= 1 ))
					{
				        pConfig = (sLDAPConfigData *)gLDAPConfigTable->GetItemData( pContext->fConfigTableIndex );
				        if (pConfig != nil)
				        {
				        	if (pConfig->fName != nil)
				        	{
				        		tmpStr = new char[1+::strlen(pConfig->fName)];
				        		::strcpy( tmpStr, pConfig->fName );
			        		}
		        		}
	        		}

					if (tmpStr == nil)
					{
						tmpStr = new char[1+::strlen("No Display Name")];
						::strcpy( tmpStr, "No Display Name" );
					}
					
					// Attribute value count
					aTmpData->AppendShort( 1 );
					
					// Append attribute value
					aTmpData->AppendLong( ::strlen( tmpStr ) );
					aTmpData->AppendString( tmpStr );

					delete( tmpStr );

				} // fInAttrInfoOnly is false
				else
				{
					aTmpData->AppendShort( 0 );
				}
				
				// Add the attribute length
				aAttrData->AppendLong( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
                aTmpData->Clear();
			
			} // kDSAttributesAll or kDS1AttrDistinguishedName

		} // while

		aRecData->AppendShort( uiAttrCnt );
		if (uiAttrCnt > 0)
		{
			aRecData->AppendBlock( aAttrData->GetData(), aAttrData->GetLength() );
		}

		outBuff.AddData( aRecData->GetData(), aRecData->GetLength() );
		inData->fOutAttrInfoCount = uiAttrCnt;

		pData = outBuff.GetDataBlock( 1, &uiOffset );
		if ( pData != nil )
		{
			pAttrContext = MakeContextData();
			if ( pAttrContext  == nil ) throw( (sInt32) eMemoryAllocError );
			
			pAttrContext->fConfigTableIndex = pContext->fConfigTableIndex;

		//add to the offset for the attr list the length of the GetDirNodeInfo fixed record labels
//		record length = 4
//		aRecData->AppendShort( ::strlen( "dsAttrTypeStandard:DirectoryNodeInfo" ) ); = 2
//		aRecData->AppendString( "dsAttrTypeStandard:DirectoryNodeInfo" ); = 36
//		aRecData->AppendShort( ::strlen( "DirectoryNodeInfo" ) ); = 2
//		aRecData->AppendString( "DirectoryNodeInfo" ); = 17
//		total adjustment = 4 + 2 + 36 + 2 + 17 = 61

			pAttrContext->offset = uiOffset + 61;

			gLDAPContextTable->AddItem( inData->fOutAttrListRef, pAttrContext );
		}
        else
        {
            siResult = eDSBufferTooSmall;
        }
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	if ( inAttrList != nil )
	{
		delete( inAttrList );
		inAttrList = nil;
	}
	if ( aRecData != nil )
	{
		delete( aRecData );
		aRecData = nil;
	}
	if ( aAttrData != nil )
	{
		delete( aAttrData );
		aAttrData = nil;
	}
	if ( aTmpData != nil )
	{
		delete( aTmpData );
		aTmpData = nil;
	}

	return( siResult );

} // GetDirNodeInfo

//------------------------------------------------------------------------------------
//	* OpenRecord
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::OpenRecord ( sOpenRecord *inData )
{
	sInt32				siResult		= eDSNoErr;
	tDataNodePtr		pRecName		= nil;
	tDataNodePtr		pRecType		= nil;
	char			   *pLDAPRecType	= nil;
	sLDAPContextData   *pContext		= nil;
	sLDAPContextData   *pRecContext		= nil;
    char			   *queryFilter		= nil;
	LDAPMessage		   *result			= nil;
	int					ldapReturnCode	= 0;
	int					numRecTypes		= 1;
	bool				bResultFound	= false;
    sLDAPConfigData	   *pConfig			= nil;
    int					searchTO   		= 0;
	bool				bOCANDGroup		= false;
	CFArrayRef			OCSearchList	= nil;
	ber_int_t			scope			= LDAP_SCOPE_SUBTREE;
	sInt32				searchResult	= eDSNoErr;


	try
	{
		pContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInNodeRef );
		if ( pContext  == nil ) throw( (sInt32)eDSBadContextData );

		pRecType = inData->fInRecType;
		if ( pRecType  == nil ) throw( (sInt32)eDSNullRecType );

		pRecName = inData->fInRecName;
		if ( pRecName  == nil ) throw( (sInt32)eDSNullRecName );

		if (pRecName->fBufferLength == 0) throw( (sInt32)eDSEmptyRecordNameList );
		if (pRecType->fBufferLength == 0) throw( (sInt32)eDSEmptyRecordTypeList );

		//search for the specific LDAP record now
		
		//retrieve the config data
		//don't need to retrieve for the case of "generic unknown" so don't check index 0
		if (( pContext->fConfigTableIndex < gLDAPConfigTableLen) && ( pContext->fConfigTableIndex >= 1 ))
		{
			pConfig = (sLDAPConfigData *)gLDAPConfigTable->GetItemData( pContext->fConfigTableIndex );
			if (pConfig != nil)
			{
				searchTO	= pConfig->fSearchTimeout;
			}
		}
		
        // we will search over all the rectype mappings until we find the first
        // result for the search criteria in the queryfilter
        numRecTypes = 1;
		bOCANDGroup = false;
		if (OCSearchList != nil)
		{
			CFRelease(OCSearchList);
			OCSearchList = nil;
		}
		pLDAPRecType = MapRecToLDAPType( (const char *)(pRecType->fBufferData), pContext->fConfigTableIndex, numRecTypes, &bOCANDGroup, &OCSearchList, &scope, pConfigFromXML );
		//only throw this for first time since we need at least one map
		if ( pLDAPRecType == nil ) throw( (sInt32)eDSInvalidRecordType );
		
		while ( (pLDAPRecType != nil) && (!bResultFound) )
		{

			//build the record query string
			//removed the use well known map only condition ie. true to false
			queryFilter = BuildLDAPQueryFilter(	(char *)kDSNAttrRecordName,
												pRecName->fBufferData,
												eDSExact,
												pContext->fConfigTableIndex,
												false,
												(const char *)(pRecType->fBufferData),
												pLDAPRecType,
												bOCANDGroup,
												OCSearchList,
                                                pConfigFromXML );
			if ( queryFilter == nil ) throw( (sInt32)eDSNullParameter );

			searchResult = DSDoRetrieval (	pLDAPRecType,
											NULL,
											pContext,
											scope,
											fLDAPSessionMgr,
											queryFilter,
											ldapReturnCode,
											searchTO,
											result,
											bResultFound,
											pConfigFromXML);

			if ( queryFilter != nil )
			{
				delete( queryFilter );
				queryFilter = nil;
			}

			if (pLDAPRecType != nil)
			{
				delete (pLDAPRecType);
				pLDAPRecType = nil;
			}
			numRecTypes++;
			bOCANDGroup = false;
			if (OCSearchList != nil)
			{
				CFRelease(OCSearchList);
				OCSearchList = nil;
			}
			pLDAPRecType = MapRecToLDAPType( (const char *)(pRecType->fBufferData), pContext->fConfigTableIndex, numRecTypes, &bOCANDGroup, &OCSearchList, &scope, pConfigFromXML );
		} // while ( (pLDAPRecType != nil) && (!bResultFound) )

		if (	(bResultFound) &&
				( ldapReturnCode == LDAP_RES_SEARCH_ENTRY ) )
		{
		
			pRecContext = MakeContextData();
			if ( pRecContext  == nil ) throw( (sInt32) eMemoryAllocError );
	        
			if (pContext->fName != nil)
			{
				pRecContext->fName = (char *)calloc(1, 1+::strlen(pContext->fName));
				::strcpy( pRecContext->fName, pContext->fName );
			}
			pRecContext->fType = 2;
	        pRecContext->fHost = pContext->fHost;
	        pRecContext->fConfigTableIndex = pContext->fConfigTableIndex;
			if (pRecType->fBufferData != nil)
			{
				pRecContext->fOpenRecordType = (char *)calloc(1, 1+::strlen(pRecType->fBufferData));
				::strcpy( pRecContext->fOpenRecordType, pRecType->fBufferData );
			}
			if (pRecName->fBufferData != nil)
			{
				pRecContext->fOpenRecordName = (char *)calloc(1, 1+::strlen(pRecName->fBufferData));
				::strcpy( pRecContext->fOpenRecordName, pRecName->fBufferData );
			}
			
			if (pContext->authCallActive)
			{
				pRecContext->authCallActive = true;
				if (pContext->fUserName != nil)
				{
					pRecContext->fUserName = (char *)calloc(1, 1+::strlen(pContext->fUserName));
					::strcpy( pRecContext->fUserName, pContext->fUserName );
				}
				if (pContext->fAuthCredential != nil)
				{
					if ( (pContext->fAuthType == nil) || (strcmp(pContext->fAuthType,kDSStdAuthClearText) == 0) )
					{
						char *aPassword = nil;
						aPassword = (char *)calloc(1, 1+::strlen((char *)(pContext->fAuthCredential)));
						::strcpy( aPassword, (char *)(pContext->fAuthCredential) );
						pRecContext->fAuthCredential = (void *)aPassword;
					}
				}
				if (pContext->fAuthType != nil)
				{
					pRecContext->fAuthType = (char *)calloc(1, 1+::strlen(pContext->fAuthType));
					::strcpy( pRecContext->fAuthType, pContext->fAuthType );
				}
			}
	        
			LDAP *aHost = fLDAPSessionMgr.LockSession(pRecContext);
			if (aHost != nil)
			{
				//get the ldapDN here
				pRecContext->fOpenRecordDN = ldap_get_dn(aHost, result);
			}
			fLDAPSessionMgr.UnLockSession(pRecContext);
		
			gLDAPContextTable->AddItem( inData->fOutRecRef, pRecContext );
		} // if bResultFound and ldapReturnCode okay
		else
		{
	     	siResult = searchResult;
		}
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	if (OCSearchList != nil)
	{
		CFRelease(OCSearchList);
		OCSearchList = nil;
	}
	
	if ( result != nil )
	{
		ldap_msgfree( result );
		result = nil;
	}
	
	if ( pLDAPRecType != nil )
	{
		delete( pLDAPRecType );
		pLDAPRecType = nil;
	}

	if ( queryFilter != nil )
	{
		delete( queryFilter );
		queryFilter = nil;
	}

	return( siResult );

} // OpenRecord


//------------------------------------------------------------------------------------
//	* CloseRecord
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::CloseRecord ( sCloseRecord *inData )
{
	sInt32				siResult	=	eDSNoErr;
	sLDAPContextData   *pContext	=	nil;

	try
	{
		pContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInRecRef );
		if ( pContext  == nil ) throw( (sInt32)eDSBadContextData );
		
		gLDAPContextTable->RemoveItem( inData->fInRecRef );
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // CloseRecord


//------------------------------------------------------------------------------------
//	* FlushRecord
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::FlushRecord ( sFlushRecord *inData )
{
	sInt32				siResult	=	eDSNoErr;
	sLDAPContextData   *pContext	=	nil;

	try
	{
		pContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInRecRef );
		if ( pContext  == nil ) throw( (sInt32)eDSBadContextData );
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // FlushRecord


//------------------------------------------------------------------------------------
//	* DeleteRecord
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::DeleteRecord ( sDeleteRecord *inData )
{
	sInt32					siResult		= eDSNoErr;
	sLDAPContextData	   *pRecContext		= nil;

	try
	{
		pRecContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInRecRef );
		if ( pRecContext  == nil ) throw( (sInt32)eDSBadContextData );

		if ( pRecContext->fOpenRecordDN  == nil ) throw( (sInt32)eDSNullRecName );

		//KW revisit for what degree of error return we need to provide
		//if LDAP_NOT_ALLOWED_ON_NONLEAF then this is not a leaf in the hierarchy ie. leaves need to go first
		//if LDAP_INVALID_CREDENTIALS or ??? then don't have authority so use eDSPermissionError
		//if LDAP_NO_SUCH_OBJECT then eDSRecordNotFound
		//so for now we return simply  eDSPermissionError if ANY error
		LDAP *aHost = fLDAPSessionMgr.LockSession(pRecContext);
		if ( (aHost != nil) && ( ldap_delete_s( aHost, pRecContext->fOpenRecordDN) != LDAP_SUCCESS ) )
		{
			siResult = eDSPermissionError;
		}
		fLDAPSessionMgr.UnLockSession(pRecContext);
		
		gLDAPContextTable->RemoveItem( inData->fInRecRef );
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}
		
	return( siResult );
} // DeleteRecord


//------------------------------------------------------------------------------------
//	* AddAttribute
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::AddAttribute( sAddAttribute *inData )
{
	sInt32					siResult		= eDSNoErr;

	siResult = AddValue( inData->fInRecRef, inData->fInNewAttr, inData->fInFirstAttrValue );

	return( siResult );

} // AddAttribute


//------------------------------------------------------------------------------------
//	* AddAttributeValue
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::AddAttributeValue( sAddAttributeValue *inData )
{
	sInt32					siResult		= eDSNoErr;

	siResult = AddValue( inData->fInRecRef, inData->fInAttrType, inData->fInAttrValue );

	return( siResult );

} // AddAttributeValue


//------------------------------------------------------------------------------------
//	* RemoveAttribute
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::RemoveAttribute ( sRemoveAttribute *inData )
{
	sInt32					siResult		= eDSNoErr;
	sLDAPContextData	   *pRecContext		= nil;
	int						numAttributes	= 1;
	char				   *pLDAPAttrType	= nil;
	LDAPMod				   *mods[2];
	int						ldapReturnCode	= 0;

	try
	{
		pRecContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInRecRef );
		if ( pRecContext  == nil ) throw( (sInt32)eDSBadContextData );

		if ( pRecContext->fOpenRecordDN == nil ) throw( (sInt32)eDSNullRecName );
		if ( inData->fInAttribute->fBufferData == nil ) throw( (sInt32)eDSNullAttributeType );

		//get the first mapping
		numAttributes = 1;
		pLDAPAttrType = MapAttrToLDAPType( (const char *)(pRecContext->fOpenRecordType), inData->fInAttribute->fBufferData, pRecContext->fConfigTableIndex, numAttributes, pConfigFromXML, true );
		//throw if first nil since no more will be found otherwise proceed until nil
		if ( pLDAPAttrType == nil ) throw( (sInt32)eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable

		mods[1] = NULL;
		//do all the mapped native types and simply collect the last error if there is one
		while ( pLDAPAttrType != nil )
		{
			//create this mods entry
			{
				LDAPMod	mod;
				mod.mod_op		= LDAP_MOD_DELETE;
				mod.mod_type	= pLDAPAttrType;
				mod.mod_values	= NULL;
				mods[0]			= &mod;
				
				LDAP *aHost = fLDAPSessionMgr.LockSession(pRecContext);
				if (aHost != nil)
				{
					ldapReturnCode = ldap_modify_s( aHost, pRecContext->fOpenRecordDN, mods);
				}
				else
				{
					ldapReturnCode = LDAP_LOCAL_ERROR;
				}
				fLDAPSessionMgr.UnLockSession(pRecContext);
				if ( ( ldapReturnCode == LDAP_NO_SUCH_ATTRIBUTE ) || ( ldapReturnCode == LDAP_UNDEFINED_TYPE ) )
				{
					siResult = eDSNoErr;
				}
				else if ( ldapReturnCode != LDAP_SUCCESS )
				{
					siResult = MapLDAPWriteErrorToDS( ldapReturnCode, eDSAttributeNotFound );
				}
			}

			//cleanup pLDAPAttrType if needed
			if (pLDAPAttrType != nil)
			{
				delete (pLDAPAttrType);
				pLDAPAttrType = nil;
			}
			numAttributes++;
			//get the next mapping
			pLDAPAttrType = MapAttrToLDAPType( (const char *)(pRecContext->fOpenRecordType), inData->fInAttribute->fBufferData, pRecContext->fConfigTableIndex, numAttributes, pConfigFromXML, true );
		} // while ( pLDAPAttrType != nil )

	}

	catch ( sInt32 err )
	{
		siResult = err;
	}
	
	return( siResult );

} // RemoveAttribute


//------------------------------------------------------------------------------------
//	* AddValue
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::AddValue ( uInt32 inRecRef, tDataNodePtr inAttrType, tDataNodePtr inAttrValue )
{
	sInt32					siResult		= eDSNoErr;
	sLDAPContextData	   *pRecContext		= nil;
	char				   *attrValue		= nil;
	uInt32					attrLength		= 0;
	char				   *emptyValue		= (char *)"";
	struct berval			bval;
	struct berval			*bvals[2];
	int						numAttributes	= 1;
	char				   *pLDAPAttrType	= nil;
	LDAPMod				   *mods[2];
	int						ldapReturnCode	= 0;

	try
	{
		pRecContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inRecRef );
		if ( pRecContext  == nil ) throw( (sInt32)eDSBadContextData );

		if ( pRecContext->fOpenRecordDN == nil ) throw( (sInt32)eDSNullRecName );
		if ( inAttrType->fBufferData == nil ) throw( (sInt32)eDSNullAttributeType );

		//build the mods entry to pass into ldap_modify_s
		if (inAttrValue == nil)
		{
			//set up with empty value
			attrValue	= emptyValue;
			attrLength	= 1; //TODO ????
		}
		else
		{
			attrLength = inAttrValue->fBufferLength;
			attrValue = (char *) calloc(1, 1 + attrLength);
			memcpy(attrValue, inAttrValue->fBufferData, attrLength);
			
		}
		bval.bv_val = attrValue;
		bval.bv_len = attrLength;
		bvals[0]	= &bval;
		bvals[1]	= NULL;

		//get the first mapping
		numAttributes = 1;
		pLDAPAttrType = MapAttrToLDAPType( (const char *)(pRecContext->fOpenRecordType), inAttrType->fBufferData, pRecContext->fConfigTableIndex, numAttributes, pConfigFromXML, true );
		//throw if first nil since no more will be found otherwise proceed until nil
		if ( pLDAPAttrType == nil ) throw( (sInt32)eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable

		mods[1] = NULL;
		//ONLY add to the FIRST mapped native type
		if ( pLDAPAttrType != nil )
		{
			//create this mods entry
			{
				LDAPMod	mod;
				mod.mod_op		= LDAP_MOD_ADD | LDAP_MOD_BVALUES;
				mod.mod_type	= pLDAPAttrType;
				mod.mod_bvalues	= bvals;
				mods[0]			= &mod;
				
				LDAP *aHost = fLDAPSessionMgr.LockSession(pRecContext);
				if (aHost != nil)
				{
					ldapReturnCode = ldap_modify_s( aHost, pRecContext->fOpenRecordDN, mods);
				}
				else
				{
					ldapReturnCode = LDAP_LOCAL_ERROR;
				}
				fLDAPSessionMgr.UnLockSession(pRecContext);
				if ( ldapReturnCode != LDAP_SUCCESS )
				{
					siResult = MapLDAPWriteErrorToDS( ldapReturnCode, eDSAttributeNotFound );
				}
			}

			//cleanup pLDAPAttrType if needed
			if (pLDAPAttrType != nil)
			{
				delete (pLDAPAttrType);
				pLDAPAttrType = nil;
			}
		} // if ( pLDAPAttrType != nil )
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}
	
	if ((attrValue != emptyValue) && (attrValue != nil))
	{
		free(attrValue);
		attrValue = nil;
	}
		
	return( siResult );

} // AddValue


//------------------------------------------------------------------------------------
//	* SetRecordName
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::SetRecordName ( sSetRecordName *inData )
{
	sInt32					siResult		= eDSNoErr;
	sLDAPContextData	   *pRecContext		= nil;
	char				   *pLDAPAttrType	= nil;
	int						ldapReturnCode	= 0;
	tDataNodePtr			pRecName		= nil;
	char				   *ldapRDNString	= nil;
	uInt32					ldapRDNLength	= 0;

	try
	{
		pRecContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInRecRef );
		if ( pRecContext  == nil ) throw( (sInt32)eDSBadContextData );

		//here we are going to assume that the name given to create this record
		//will fill out the DN with the first recordname native type
		pRecName = inData->fInNewRecName;
		if ( pRecName  == nil ) throw( (sInt32)eDSNullRecName );

		if (pRecName->fBufferLength == 0) throw( (sInt32)eDSEmptyRecordNameList );

		//get ONLY the first record name mapping
		pLDAPAttrType = MapAttrToLDAPType( (const char *)(pRecContext->fOpenRecordType), kDSNAttrRecordName, pRecContext->fConfigTableIndex, 1, pConfigFromXML, true );
		//throw if first nil since no more will be found otherwise proceed until nil
		if ( pLDAPAttrType == nil ) throw( (sInt32)eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable
		
		
		//if first native map for kDSNAttrRecordName is "cn" then the DN will be built as follows:
		//"cn = pRecName->fBufferData, pLDAPRecType"
		ldapRDNLength = strlen(pLDAPAttrType) + 1 + pRecName->fBufferLength;
		ldapRDNString = (char *)calloc(1, ldapRDNLength + 1);
		strcpy(ldapRDNString,pLDAPAttrType);
		strcat(ldapRDNString,"=");
		strcat(ldapRDNString,pRecName->fBufferData);
		
		//KW looks like for v3 we must use ldap_rename API instead of ldap_modrdn2_s for v2

//		ldapReturnCode = ldap_modrdn2_s( pRecContext->fHost, pRecContext->fOpenRecordDN, ldapRDNString, 1);
		LDAP *aHost = fLDAPSessionMgr.LockSession(pRecContext);
		if (aHost != nil)
		{
			ldapReturnCode = ldap_rename_s( aHost, pRecContext->fOpenRecordDN, ldapRDNString, NULL, 1, NULL, NULL);
		}
		else
		{
			ldapReturnCode = LDAP_LOCAL_ERROR;
		}
		fLDAPSessionMgr.UnLockSession(pRecContext);
		if ( ldapReturnCode == LDAP_ALREADY_EXISTS )
		{
			siResult = eDSNoErr; // already has this name
		}
		else if ( ldapReturnCode != LDAP_SUCCESS )
		{
			siResult = MapLDAPWriteErrorToDS( ldapReturnCode, eDSRecordNotFound );
		}
		else //let's update our context data since we succeeded
		{
			if (pRecContext->fOpenRecordName != nil)
			{
				free(pRecContext->fOpenRecordName);
			}
			pRecContext->fOpenRecordName = (char *)calloc(1, 1+::strlen(pRecName->fBufferData));
			::strcpy( pRecContext->fOpenRecordName, pRecName->fBufferData );
			
			char *newldapDN		= nil;
			char *pLDAPRecType	= nil;
			pLDAPRecType = MapRecToLDAPType( (const char *)(pRecContext->fOpenRecordType), pRecContext->fConfigTableIndex, 1, nil, nil, nil, pConfigFromXML );
			if (pLDAPRecType != nil)
			{
				newldapDN = (char *) calloc(1, 1 + strlen(ldapRDNString) + 2 + strlen(pLDAPRecType));
				strcpy(newldapDN,ldapRDNString);
				strcat(newldapDN,", ");
				strcat(newldapDN,pLDAPRecType);
				if (pRecContext->fOpenRecordDN != nil)
				{
					free(pRecContext->fOpenRecordDN);
				}
				pRecContext->fOpenRecordDN = newldapDN;
			}
		}

	}

	catch ( sInt32 err )
	{
		siResult = err;
	}
	
	//cleanup pLDAPAttrType if needed
	if (pLDAPAttrType != nil)
	{
		free(pLDAPAttrType);
		pLDAPAttrType = nil;
	}

	//cleanup ldapRDNString if needed
	if (ldapRDNString != nil)
	{
		free(ldapRDNString);
		ldapRDNString = nil;
	}

	return( siResult );

} // SetRecordName


//------------------------------------------------------------------------------------
//	* CreateRecord
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::CreateRecord ( sCreateRecord *inData )
{
	sInt32					siResult		= eDSNoErr;
	sLDAPContextData	   *pContext		= nil;
	sLDAPContextData	   *pRecContext		= nil;
	char				   *pLDAPAttrType	= nil;
	LDAPMod				   *mods[128];				//pick 128 since it seems unlikely we will ever get to 126 required attrs
	uInt32					modIndex		= 0;
	int						ldapReturnCode	= 0;
    sLDAPConfigData		   *pConfig			= nil;
    int						searchTO   		= 0;
	tDataNodePtr			pRecType		= nil;
	tDataNodePtr			pRecName		= nil;
	char				   *pLDAPRecType	= nil;
	char				   *ldapDNString	= nil;
	uInt32					ldapDNLength	= 0;
	uInt32					ocCount			= 0;
	uInt32					raCount			= 0;
	LDAPMod					ocmod;
	LDAPMod					rnmod;
	char				  **ocvals			= nil;
	char				   *rnvals[2];
	char				   *ocString		= nil;
	listOfStrings			objectClassList;
	listOfStrings			reqAttrsList;
	char				  **needsValueMarker= nil;
	bool					bOCANDGroup		= false;
	CFArrayRef				OCSearchList	= nil;
	char				   *tmpBuff			= nil;
	CFIndex					cfBuffSize		= 1024;
	int						aOCSearchListCount	= 0;


	try
	{
		pContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInNodeRef );
		if ( pContext  == nil ) throw( (sInt32)eDSBadContextData );

		pRecType = inData->fInRecType;
		if ( pRecType  == nil ) throw( (sInt32)eDSNullRecType );

		//here we are going to assume that the name given to create this record
		//will fill out the DN with the first recordname native type
		pRecName = inData->fInRecName;
		if ( pRecName  == nil ) throw( (sInt32)eDSNullRecName );

		if (pRecName->fBufferLength == 0) throw( (sInt32)eDSEmptyRecordNameList );
		if (pRecType->fBufferLength == 0) throw( (sInt32)eDSEmptyRecordTypeList );

		//get ONLY the first record name mapping
		pLDAPAttrType = MapAttrToLDAPType( (const char *)(pRecType->fBufferData), kDSNAttrRecordName, pContext->fConfigTableIndex, 1, pConfigFromXML, true );
		//throw if first nil since no more will be found otherwise proceed until nil
		if ( pLDAPAttrType == nil ) throw( (sInt32)eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable
		
		//get ONLY the first record type mapping
		pLDAPRecType = MapRecToLDAPType( (const char *)(pRecType->fBufferData), pContext->fConfigTableIndex, 1, &bOCANDGroup, &OCSearchList, nil, pConfigFromXML );
		//throw if first nil since no more will be found otherwise proceed until nil
		if ( pLDAPAttrType == nil ) throw( (sInt32)eDSInvalidRecordType );  //KW would like a eDSNoMappingAvailable
		
		//if first native map for kDSNAttrRecordName is "cn" then the DN will be built as follows:
		//"cn = pRecName->fBufferData, pLDAPRecType"
		ldapDNLength = strlen(pLDAPAttrType) + 1 + pRecName->fBufferLength + 2 + strlen(pLDAPRecType);
		ldapDNString = (char *)calloc(1, ldapDNLength + 1);
		strcpy(ldapDNString,pLDAPAttrType);
		strcat(ldapDNString,"=");
		strcat(ldapDNString,pRecName->fBufferData);
		strcat(ldapDNString,", ");
		strcat(ldapDNString,pLDAPRecType);
		
		rnvals[0] = pRecName->fBufferData;
		rnvals[1] = NULL;
		rnmod.mod_op = 0;
		rnmod.mod_type = pLDAPAttrType;
		rnmod.mod_values = rnvals;

		if ( (pRecType->fBufferData != nil) && (pLDAPRecType != nil) )
		{
			if (strncmp(pRecType->fBufferData,kDSStdRecordTypePrefix,strlen(kDSStdRecordTypePrefix)) == 0)
			{
				//find the rec map that we need using pContext->fConfigTableIndex
				if (( pContext->fConfigTableIndex < gLDAPConfigTableLen) && ( pContext->fConfigTableIndex >= 0 ))
				{
					pConfig = (sLDAPConfigData *)gLDAPConfigTable->GetItemData( pContext->fConfigTableIndex );
					if (pConfig != nil)
					{
						searchTO	= pConfig->fSearchTimeout;
					}
				}
				
				if (OCSearchList != nil)
				{
					CFStringRef	ocString = nil;
					// assume that the extracted objectclass strings will be significantly less than 1024 characters
					tmpBuff = (char *)::calloc(1, 1024);
					
					// here we extract the object class strings
					// do we need to escape any of the characters internal to the CFString??? like before
					// NO since "*, "(", and ")" are not legal characters for objectclass names
					
					// if OR then we only use the first one
					if (!bOCANDGroup)
					{
						if (CFArrayGetCount(OCSearchList) >= 1)
						{
							ocString = (CFStringRef)::CFArrayGetValueAtIndex( OCSearchList, 0 );
						}
						if (ocString != nil)
						{
							CFStringGetCString(ocString, tmpBuff, cfBuffSize, kCFStringEncodingUTF8);
							string ourString(tmpBuff);
							objectClassList.push_back(ourString);
						}
					}
					else
					{
						aOCSearchListCount = CFArrayGetCount(OCSearchList);
						for ( int iOCIndex = 0; iOCIndex < aOCSearchListCount; iOCIndex++ )
						{
							::memset(tmpBuff,0,1024);
							ocString = (CFStringRef)::CFArrayGetValueAtIndex( OCSearchList, iOCIndex );
							if (ocString != nil)
							{		
								CFStringGetCString(ocString, tmpBuff, cfBuffSize, kCFStringEncodingUTF8);
								string ourString(tmpBuff);
								objectClassList.push_back(ourString);
							}
						}// loop over the objectclasses CFArray
					}
				}//OCSearchList != nil
			}// if (strncmp(pRecType->fBufferData,kDSStdRecordTypePrefix,strlen(kDSStdRecordTypePrefix)) == 0)
		}// if ( (pRecType->fBufferData != nil) && (pLDAPRecType != nil) )

		mods[0] = &rnmod;
//		mods[1] = &snmod;
		for (uInt32 modsIndex=1; modsIndex < 128; modsIndex++)
		{
			mods[modsIndex] = NULL;
		}
		modIndex = 1;
		
		//always assuming here that pConfig != nil
		//here we check if we have the object class schema -- otherwise we try to retrieve it
		//ie. don't have it and haven't already tried to retrieve it
		if ( (pConfig->fObjectClassSchema == nil) && (!pConfig->bOCBuilt) )
		{
			fLDAPSessionMgr.GetSchema( pContext );
		}
		
		if (OCSearchList != nil)
		{
			if (pConfig->fObjectClassSchema != nil) //if there is a hierarchy to compare to then do it
			{
				//now we look at the objectclass list provided by the user and expand it fully with the hierarchy info
				for (listOfStringsCI iter = objectClassList.begin(); iter != objectClassList.end(); ++iter)
				{
					//const char *aString = (*iter).c_str();
					if (pConfig->fObjectClassSchema->count(*iter) != 0)
					{
						ObjectClassMap::iterator mapIter = pConfig->fObjectClassSchema->find(*iter);
						for (AttrSetCI parentIter = mapIter->second->fParentOCs.begin(); parentIter != mapIter->second->fParentOCs.end(); ++parentIter)
						{
							bool addObjectClassName = true;
							for (listOfStringsCI dupIter = objectClassList.begin(); dupIter != objectClassList.end(); ++dupIter)
							{
								if (*dupIter == *parentIter) //already in the list
								{
									addObjectClassName = false;
									break;
								}
							}
							if (addObjectClassName)
							{
								objectClassList.push_back(*parentIter);
							}
						}
					}
				}
				
				//now that we have the objectclass list we can build a similar list of the required creation attributes
				for (listOfStringsCI iter = objectClassList.begin(); iter != objectClassList.end(); ++iter)
				{
					if (pConfig->fObjectClassSchema->count(*iter) != 0)
					{
						ObjectClassMap::iterator mapIter = pConfig->fObjectClassSchema->find(*iter);
						for (AttrSetCI reqIter = mapIter->second->fRequiredAttrs.begin(); reqIter != mapIter->second->fRequiredAttrs.end(); ++reqIter)
						{
							if (	(*reqIter != "objectClass") &&					// explicitly added already
									(*reqIter != "nTSecurityDescriptor") &&			// exclude nTSecurityDescriptor special AD attr type
									(*reqIter != "objectCategory") &&				// exclude objectCategory special AD attr type
									(*reqIter != "instanceType") )					// exclude instanceType special AD attr type
							{
								bool addReqAttr = true;
								for (listOfStringsCI dupIter = reqAttrsList.begin(); dupIter != reqAttrsList.end(); ++dupIter)
								{
									if (*dupIter == *reqIter) //already in the list
									{
										addReqAttr = false;
										break;
									}
								}
								if (addReqAttr)
								{
									reqAttrsList.push_back(*reqIter);
								}
							}
							if (*reqIter == "nTSecurityDescriptor")		//For AD LDAP we force the addition of the sAMAccountName
							{
								string nameString("sAMAccountName");
								reqAttrsList.push_back(nameString);
							}
						}
					}
				}
			}
			raCount = reqAttrsList.size();
			ocCount = objectClassList.size();
			ocString = (char *)calloc(1,12);
			strcpy(ocString,"objectClass");
			ocmod.mod_op = 0;
			ocmod.mod_type = ocString;
			ocmod.mod_values = nil;
			ocvals = (char **)calloc(1,(ocCount+1)*sizeof(char **));
			//build the ocvals here
			uInt32 ocValIndex = 0;
			for (listOfStringsCI iter = objectClassList.begin(); iter != objectClassList.end(); ++iter)
			{
				const char *aString = (*iter).c_str();
				ocvals[ocValIndex] = (char *)aString;  //TODO recheck for leaks
				ocValIndex++;
			}
			ocvals[ocCount] = nil;
			ocmod.mod_values = ocvals;

			mods[1] = &ocmod;
			modIndex = 2;
		}
		
		needsValueMarker = (char **)calloc(1,2*sizeof(char *));
		needsValueMarker[0] = "99";
		needsValueMarker[1] = NULL;
		
		//check if we have determined what attrs need to be added
		if (raCount != 0)
		{
			for (listOfStringsCI addIter = reqAttrsList.begin(); addIter != reqAttrsList.end(); ++addIter)
			{
				if (modIndex == 127)
				{
					//unlikely to get here as noted above but nonetheless check for it and just drop the rest of the req attrs
					break;
				}
				if (	(strcasecmp((*addIter).c_str(), pLDAPAttrType) != 0) ||		//if this is not the record name then we can add a default value
						(strcasecmp("sAMAccountName", pLDAPAttrType) == 0) )
				{
					LDAPMod *aLDAPMod = (LDAPMod *)calloc(1,sizeof(LDAPMod));
					aLDAPMod->mod_op = 0;
					const char *aString = (*addIter).c_str();
					aLDAPMod->mod_type = (char *)aString;  //TODO recheck for leaks
					aLDAPMod->mod_values = needsValueMarker; //TODO KW really need syntax specific default value added here
					mods[modIndex] = aLDAPMod;
					modIndex++;
				}
			}
		}
		mods[modIndex] = NULL;

		LDAP *aHost = fLDAPSessionMgr.LockSession(pContext);
		if (aHost != nil)
		{
			ldapReturnCode = ldap_add_ext_s( aHost, ldapDNString, mods, NULL, NULL);
		}
		else
		{
			ldapReturnCode = LDAP_LOCAL_ERROR;
		}
		fLDAPSessionMgr.UnLockSession(pContext);
		if ( ldapReturnCode == LDAP_ALREADY_EXISTS )
		{
			siResult = eDSRecordAlreadyExists;
		}
		else if ( ldapReturnCode != LDAP_SUCCESS )
		{
			siResult = MapLDAPWriteErrorToDS( ldapReturnCode, eDSRecordNotFound );
		}

		if ( (inData->fInOpen == true) && (siResult == eDSNoErr) )
		{
			pRecContext = MakeContextData();
			if ( pRecContext  == nil ) throw( (sInt32) eMemoryAllocError );
	        
			if (pContext->fName != nil)
			{
				pRecContext->fName = (char *)calloc(1, 1+::strlen(pContext->fName));
				::strcpy( pRecContext->fName, pContext->fName );
			}
			pRecContext->fType = 2;
	        pRecContext->fHost = pContext->fHost;
	        pRecContext->fConfigTableIndex = pContext->fConfigTableIndex;
			if (pRecType->fBufferData != nil)
			{
				pRecContext->fOpenRecordType = (char *)calloc(1, 1+::strlen(pRecType->fBufferData));
				::strcpy( pRecContext->fOpenRecordType, pRecType->fBufferData );
			}
			if (pRecName->fBufferData != nil)
			{
				pRecContext->fOpenRecordName = (char *)calloc(1, 1+::strlen(pRecName->fBufferData));
				::strcpy( pRecContext->fOpenRecordName, pRecName->fBufferData );
			}
			
			//get the ldapDN here
			pRecContext->fOpenRecordDN = ldapDNString;
			ldapDNString = nil;
		
			if (pContext->authCallActive)
			{
				pRecContext->authCallActive = true;
				if (pContext->fUserName != nil)
				{
					pRecContext->fUserName = (char *)calloc(1, 1+::strlen(pContext->fUserName));
					::strcpy( pRecContext->fUserName, pContext->fUserName );
				}
				if (pContext->fAuthCredential != nil)
				{
					if ( (pContext->fAuthType == nil) || (strcmp(pContext->fAuthType,kDSStdAuthClearText) == 0) )
					{
						char *aPassword = nil;
						aPassword = (char *)calloc(1, 1+::strlen((char *)(pContext->fAuthCredential)));
						::strcpy( aPassword, (char *)(pContext->fAuthCredential) );
						pRecContext->fAuthCredential = (void *)aPassword;
					}
				}
				if (pContext->fAuthType != nil)
				{
					pRecContext->fAuthType = (char *)calloc(1, 1+::strlen(pContext->fAuthType));
					::strcpy( pRecContext->fAuthType, pContext->fAuthType );
				}
			}
	        
			gLDAPContextTable->AddItem( inData->fOutRecRef, pRecContext );
		}

	}

	catch ( sInt32 err )
	{
		siResult = err;
	}
	
	//cleanup pLDAPAttrType if needed
	if (pLDAPAttrType != nil)
	{
		free(pLDAPAttrType);
		pLDAPAttrType = nil;
	}

	//cleanup pLDAPRecType if needed
	if (pLDAPRecType != nil)
	{
		free(pLDAPRecType);
		pLDAPRecType = nil;
	}

	//cleanup ldapDNString if needed
	if (ldapDNString != nil)
	{
		free(ldapDNString);
		ldapDNString = nil;
	}

	//cleanup ocvals if needed
	if (needsValueMarker != nil)
	{
		free(needsValueMarker);
		needsValueMarker = nil;
	}

	//cleanup ocString if needed
	if (ocString != nil)
	{
		free(ocString);
		ocString = nil;
	}

	//cleanup ocvals if needed
	if (ocvals != nil)
	{
		free(ocvals);
		ocvals = nil;
	}
	
	uInt32 startIndex = 1;
	if (OCSearchList != nil)
	{
		startIndex = 2;
	}
	for (uInt32 anIndex = startIndex; anIndex < modIndex; anIndex++)
	{
		free(mods[anIndex]);
		mods[anIndex] = NULL;
	}
	
	if (OCSearchList != nil)
	{
		CFRelease(OCSearchList);
		OCSearchList = nil;
	}
	
	if ( tmpBuff != nil )
	{
		free(tmpBuff);
		tmpBuff = nil;
	}

	return( siResult );

} // CreateRecord


//------------------------------------------------------------------------------------
//	* RemoveAttributeValue
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::RemoveAttributeValue ( sRemoveAttributeValue *inData )
{
	sInt32					siResult		= eDSNoErr;
	tDataNodePtr			pAttrType		= nil;
	char				   *pLDAPAttrType	= nil;
	sLDAPContextData	   *pRecContext		= nil;
	LDAPMessage			   *result			= nil;
	struct berval		  **bValues			= nil;
	struct berval		  **newValues		= nil;
	unsigned long			valCount		= 0;
	bool					bFoundIt		= false;
	int						numAttributes	= 1;
	uInt32					crcVal			= 0;
	LDAPMod				   *mods[2];
	int						ldapReturnCode	= 0;
	LDAP				   *aHost			= nil;

	try
	{
		pRecContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInRecRef );
		if ( pRecContext  == nil ) throw( (sInt32)eDSBadContextData );

		if ( pRecContext->fOpenRecordDN == nil ) throw( (sInt32)eDSNullRecName );

		siResult = GetRecRefLDAPMessage( pRecContext, &result );
		if ( siResult != eDSNoErr ) throw( siResult );

		pAttrType = inData->fInAttrType;
		if ( pAttrType == nil ) throw( (sInt32)eDSEmptyAttributeType );

        if ( result != nil )
        {
        
			//get the first mapping
			numAttributes = 1;
			pLDAPAttrType = MapAttrToLDAPType( (const char *)(pRecContext->fOpenRecordType), pAttrType->fBufferData, pRecContext->fConfigTableIndex, numAttributes, pConfigFromXML, true );
			//throw if first nil since no more will be found otherwise proceed until nil
			if ( pLDAPAttrType == nil ) throw( (sInt32)eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable

			//set indicator of multiple native to standard mappings
			bFoundIt = false;
			while ( ( pLDAPAttrType != nil ) && (!bFoundIt) )
			{
				valCount = 0; //separate count for each bvalues
				
				aHost = fLDAPSessionMgr.LockSession(pRecContext);
				if ( (aHost != nil) && ( ( bValues = ldap_get_values_len (aHost, result, pLDAPAttrType )) != NULL ) )
				{
					// calc length of bvalues
					for (int i = 0; bValues[i] != NULL; i++ )
					{
						valCount++;
					}
					
					newValues = (struct berval**) calloc(1, (valCount + 1) *sizeof(struct berval *) );
					// for each value of the attribute
					uInt32 newIndex = 0;
					for (int i = 0; bValues[i] != NULL; i++ )
					{

						//use CRC here - WITF we assume string??
						crcVal = CalcCRC( bValues[i]->bv_val );
						if ( crcVal == inData->fInAttrValueID )
						{
							bFoundIt = true;
							//add the bvalues to the newValues
							newValues[newIndex] = bValues[i];
							newIndex++; 
						}
						
					} // for each bValues[i]
					
				} // if bValues = ldap_get_values_len ...
				fLDAPSessionMgr.UnLockSession(pRecContext);
						
				if (bFoundIt)
				{
					//here we set the newValues ie. remove the found one
					//create this mods entry
					{
						LDAPMod	mod;
						mod.mod_op		= LDAP_MOD_DELETE | LDAP_MOD_BVALUES;
						mod.mod_type	= pLDAPAttrType;
						mod.mod_bvalues	= newValues;
						mods[0]			= &mod;
						mods[1]			= NULL;

						//KW revisit for what degree of error return we need to provide
						//if LDAP_INVALID_CREDENTIALS or LDAP_INSUFFICIENT_ACCESS then don't have authority so use eDSPermissionError
						//if LDAP_NO_SUCH_OBJECT then eDSAttributeNotFound
						//if LDAP_TYPE_OR_VALUE_EXISTS then ???
						//so for now we return simply eDSSchemaError if ANY other error
						aHost = fLDAPSessionMgr.LockSession(pRecContext);
						if (aHost != nil)
						{
							ldapReturnCode = ldap_modify_s( aHost, pRecContext->fOpenRecordDN, mods);
						}
						else
						{
							ldapReturnCode = LDAP_LOCAL_ERROR;
						}
						fLDAPSessionMgr.UnLockSession(pRecContext);
						if ( ldapReturnCode != LDAP_SUCCESS )
						{
							siResult = MapLDAPWriteErrorToDS( ldapReturnCode, eDSAttributeNotFound );
						}
					}
				}
				
				if (bValues != NULL)
				{
					ldap_value_free_len(bValues);
					bValues = NULL;
				}
				if (newValues != NULL) 
				{
					free(newValues); //since newValues points to bValues
					newValues = NULL;
				}
				
				//KW here we decide to opt out since we have removed one value already
				//ie. we could continue on to the next native mapping to find more
				//CRC ID matches and remove them as well by resetting bFoundIt
				//and allowing the stop condition to be the number of native types
								
				//cleanup pLDAPAttrType if needed
				if (pLDAPAttrType != nil)
				{
					delete (pLDAPAttrType);
					pLDAPAttrType = nil;
				}
				numAttributes++;
				//get the next mapping
				pLDAPAttrType = MapAttrToLDAPType( (const char *)(pRecContext->fOpenRecordType), pAttrType->fBufferData, pRecContext->fConfigTableIndex, numAttributes, pConfigFromXML, true );
			} // while ( pLDAPAttrType != nil )
			
        	ldap_msgfree( result );
	        result = nil;
	
        } // retrieve the result from the LDAP server
        else
        {
        	throw( (sInt32)eDSRecordNotFound ); //KW???
        }

	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	if (pLDAPAttrType != nil)
	{
		delete( pLDAPAttrType );
		pLDAPAttrType = nil;
	}
	
	return( siResult );

} // RemoveAttributeValue


//------------------------------------------------------------------------------------
//	* SetAttributeValue
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::SetAttributeValue ( sSetAttributeValue *inData )
{
	sInt32					siResult		= eDSNoErr;
	tDataNodePtr			pAttrType		= nil;
	char				   *pLDAPAttrType	= nil;
	sLDAPContextData	   *pRecContext		= nil;
	LDAPMessage			   *result			= nil;
	unsigned long			valCount		= 0;
	struct berval		  **bValues			= nil;
	struct berval			replaceValue;
	struct berval		  **newValues		= nil;
	bool					bFoundIt		= false;
	int						numAttributes	= 1;
	uInt32					crcVal			= 0;
	LDAPMod				   *mods[2];
	int						ldapReturnCode	= 0;
	char				   *attrValue		= nil;
	uInt32					attrLength		= 0;
	char				   *emptyValue		= (char *)"";
	LDAP				   *aHost			= nil;

	try
	{
		pRecContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInRecRef );
		if ( pRecContext  == nil ) throw( (sInt32)eDSBadContextData );

		if ( pRecContext->fOpenRecordDN == nil ) throw( (sInt32)eDSNullRecName );
		if ( inData->fInAttrValueEntry == nil ) throw( (sInt32)eDSNullAttributeValue );
		
		//CRC ID is inData->fInAttrValueEntry->fAttributeValueID as used below

		//build the mods data entry to pass into replaceValue
		if (inData->fInAttrValueEntry->fAttributeValueData.fBufferData == NULL)
		{
			//set up with empty value
			attrValue	= emptyValue;
			attrLength	= 1; //TODO ????
		}
		else
		{
			attrLength = inData->fInAttrValueEntry->fAttributeValueData.fBufferLength;
			attrValue = (char *) calloc(1, 1 + attrLength);
			memcpy(attrValue, inData->fInAttrValueEntry->fAttributeValueData.fBufferData, attrLength);
			
		}
		siResult = GetRecRefLDAPMessage( pRecContext, &result );
		if ( siResult != eDSNoErr ) throw( siResult );

		pAttrType = inData->fInAttrType;
		if ( pAttrType == nil ) throw( (sInt32)eDSEmptyAttributeType );

        if ( result != nil )
        {
        
			//get the first mapping
			numAttributes = 1;
			pLDAPAttrType = MapAttrToLDAPType( (const char *)(pRecContext->fOpenRecordType), pAttrType->fBufferData, pRecContext->fConfigTableIndex, numAttributes, pConfigFromXML, true );
			//throw if first nil since no more will be found otherwise proceed until nil
			if ( pLDAPAttrType == nil ) throw( (sInt32)eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable

			//set indicator of multiple native to standard mappings
			bFoundIt = false;
			while ( ( pLDAPAttrType != nil ) && (!bFoundIt) )
			{
				valCount = 0; //separate count for each bvalues
				
				aHost = fLDAPSessionMgr.LockSession(pRecContext);
				if ( (aHost != nil) && ( ( bValues = ldap_get_values_len (aHost, result, pLDAPAttrType )) != NULL ) )
				{
				
					// calc length of bvalues
					for (int i = 0; bValues[i] != NULL; i++ )
					{
						valCount++;
					}
					
					newValues = (struct berval**) calloc(1, (valCount + 1) *sizeof(struct berval *) );
					
					for (int i = 0; bValues[i] != NULL; i++ )
					{

						//use CRC here - WITF we assume string??
						crcVal = CalcCRC( bValues[i]->bv_val );
						if ( crcVal == inData->fInAttrValueEntry->fAttributeValueID )
						{
							bFoundIt = true;
							replaceValue.bv_val = attrValue;
							replaceValue.bv_len = attrLength;
							newValues[i] = &replaceValue;

						}
						else
						{
							//add the bvalues to the newValues
							newValues[i] = bValues[i];
						}
						
					} // for each bValues[i]
					
					if (!bFoundIt) //this means that attr type was searched but current value is not present so nothing to change
					{
						if (bValues != NULL)
						{
							ldap_value_free_len(bValues);
							bValues = NULL;
						}
						if (newValues != NULL)
						{
							free(newValues); //since newValues points to bValues
							newValues = NULL;
						}
						siResult = eDSAttributeValueNotFound;
					}
				} // if bValues = ldap_get_values_len ...
				fLDAPSessionMgr.UnLockSession(pRecContext);
						
				if (bFoundIt)
				{
					//here we set the newValues ie. remove the found one
					//create this mods entry
					{
						LDAPMod	mod;
						mod.mod_op		= LDAP_MOD_REPLACE | LDAP_MOD_BVALUES;
						mod.mod_type	= pLDAPAttrType;
						mod.mod_bvalues	= newValues;
						mods[0]			= &mod;
						mods[1]			= NULL;

						aHost = fLDAPSessionMgr.LockSession(pRecContext);
						if (aHost != nil)
						{
							ldapReturnCode = ldap_modify_s( aHost, pRecContext->fOpenRecordDN, mods);
						}
						else
						{
							ldapReturnCode = LDAP_LOCAL_ERROR;
						}
						fLDAPSessionMgr.UnLockSession(pRecContext);
						if ( ldapReturnCode != LDAP_SUCCESS )
						{
							siResult = MapLDAPWriteErrorToDS( ldapReturnCode, eDSAttributeNotFound );
						}
					}
				}
				
				if (bValues != NULL)
				{
					ldap_value_free_len(bValues);
					bValues = NULL;
				}
				if (newValues != NULL)
				{
					free(newValues); //since newValues points to bValues
					newValues = NULL;
				}
				
				//KW here we decide to opt out since we have removed one value already
				//ie. we could continue on to the next native mapping to find more
				//CRC ID matches and remove them as well by resetting bFoundIt
				//and allowing the stop condition to be the number of native types
								
				//cleanup pLDAPAttrType if needed
				if (pLDAPAttrType != nil)
				{
					delete (pLDAPAttrType);
					pLDAPAttrType = nil;
				}
				numAttributes++;
				//get the next mapping
				pLDAPAttrType = MapAttrToLDAPType( (const char *)(pRecContext->fOpenRecordType), pAttrType->fBufferData, pRecContext->fConfigTableIndex, numAttributes, pConfigFromXML, true );
			} // while ( pLDAPAttrType != nil )
			
        	ldap_msgfree( result );
	        result = nil;
	
        } // retrieve the result from the LDAP server
        else
        {
        	throw( (sInt32)eDSRecordNotFound ); //KW???
        }

	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	if (pLDAPAttrType != nil)
	{
		delete( pLDAPAttrType );
		pLDAPAttrType = nil;
	}
		
	if ((attrValue != emptyValue) && (attrValue != nil))
	{
		free(attrValue);
		attrValue = nil;
	}
		
	return( siResult );

} // SetAttributeValue


//------------------------------------------------------------------------------------
//	* ReleaseContinueData
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::ReleaseContinueData ( sReleaseContinueData *inData )
{
	sInt32	siResult	= eDSNoErr;

	// RemoveItem calls our ContinueDeallocProc to clean up
	if ( gLDAPContinueTable->RemoveItem( inData->fInContinueData ) != eDSNoErr )
	{
		siResult = eDSInvalidContext;
	}

	return( siResult );

} // ReleaseContinueData


//------------------------------------------------------------------------------------
//	* CloseAttributeList
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::CloseAttributeList ( sCloseAttributeList *inData )
{
	sInt32				siResult		= eDSNoErr;
	sLDAPContextData	   *pContext		= nil;

	pContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInAttributeListRef );
	if ( pContext != nil )
	{
		//only "offset" should have been used in the Context
		gLDAPContextTable->RemoveItem( inData->fInAttributeListRef );
	}
	else
	{
		siResult = eDSInvalidAttrListRef;
	}

	return( siResult );

} // CloseAttributeList


//------------------------------------------------------------------------------------
//	* CloseAttributeValueList
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::CloseAttributeValueList ( sCloseAttributeValueList *inData )
{
	sInt32				siResult		= eDSNoErr;
	sLDAPContextData	   *pContext		= nil;

	pContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInAttributeValueListRef );
	if ( pContext != nil )
	{
		//only "offset" should have been used in the Context
		gLDAPContextTable->RemoveItem( inData->fInAttributeValueListRef );
	}
	else
	{
		siResult = eDSInvalidAttrValueRef;
	}

	return( siResult );

} // CloseAttributeValueList


//------------------------------------------------------------------------------------
//	* GetRecRefInfo
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::GetRecRefInfo ( sGetRecRefInfo *inData )
{
	sInt32			siResult	= eDSNoErr;
	uInt32			uiRecSize	= 0;
	tRecordEntry   *pRecEntry	= nil;
	sLDAPContextData   *pContext	= nil;
	char		   *refType		= nil;
    uInt32			uiOffset	= 0;
    char		   *refName		= nil;

	try
	{
		pContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInRecRef );
		if ( pContext  == nil ) throw( (sInt32)eDSBadContextData );

		//place in the record type from the context data of an OpenRecord
		if ( pContext->fOpenRecordType != nil)
		{
			refType = new char[1+::strlen(pContext->fOpenRecordType)];
			::strcpy( refType, pContext->fOpenRecordType );
		}
		else //assume Record type of "Record Type Unknown"
		{
			refType = new char[1+::strlen("Record Type Unknown")];
			::strcpy( refType, "Record Type Unknown" );
		}
		
		//place in the record name from the context data of an OpenRecord
		if ( pContext->fOpenRecordName != nil)
		{
			refName = new char[1+::strlen(pContext->fOpenRecordName)];
			::strcpy( refName, pContext->fOpenRecordName );
		}
		else //assume Record name of "Record Name Unknown"
		{
			refName = new char[1+::strlen("Record Name Unknown")];
			::strcpy( refName, "Record Name Unknown" );
		}
		
		uiRecSize = sizeof( tRecordEntry ) + ::strlen( refType ) + ::strlen( refName ) + 4 + kBuffPad;
		pRecEntry = (tRecordEntry *)::calloc( 1, uiRecSize );
		
		pRecEntry->fRecordNameAndType.fBufferSize	= ::strlen( refType ) + ::strlen( refName ) + 4 + kBuffPad;
		pRecEntry->fRecordNameAndType.fBufferLength	= ::strlen( refType ) + ::strlen( refName ) + 4;
		
		uiOffset = 0;
		uInt16 strLen = 0;
		// Add the record name length and name itself
		strLen = ::strlen( refName );
		::memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, &strLen, 2);
		uiOffset += 2;
		::memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, refName, strLen);
		uiOffset += strLen;
		
		// Add the record type length and type itself
		strLen = ::strlen( refType );
		::memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, &strLen, 2);
		uiOffset += 2;
		::memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, refType, strLen);
		uiOffset += strLen;

		inData->fOutRecInfo = pRecEntry;
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}
		
	if (refType != nil)
	{
		delete( refType );
		refType = nil;
	}
	if (refName != nil)
	{
		delete( refName );
		refName = nil;
	}

	return( siResult );

} // GetRecRefInfo


//------------------------------------------------------------------------------------
//	* GetRecAttribInfo
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::GetRecAttribInfo ( sGetRecAttribInfo *inData )
{
	sInt32					siResult		= eDSNoErr;
	uInt32					uiTypeLen		= 0;
	uInt32					uiDataLen		= 0;
	tDataNodePtr			pAttrType		= nil;
	char				   *pLDAPAttrType	= nil;
	tAttributeEntryPtr		pOutAttrEntry	= nil;
	sLDAPContextData	   *pRecContext		= nil;
	LDAPMessage			   *result			= nil;
	struct berval		  **bValues;
	int						numAttributes	= 1;
	bool					bTypeFound		= false;
	int						valCount		= 0;
	
	try
	{

		pRecContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInRecRef );
		if ( pRecContext  == nil ) throw( (sInt32)eDSBadContextData );

		siResult = GetRecRefLDAPMessage( pRecContext, &result );
		if ( siResult != eDSNoErr ) throw( siResult);

		pAttrType = inData->fInAttrType;
		if ( pAttrType == nil ) throw( (sInt32)eDSEmptyAttributeType );

        if ( result != nil )
        {

			//get the first mapping
			numAttributes = 1;
			pLDAPAttrType = MapAttrToLDAPType( (const char *)(pRecContext->fOpenRecordType), pAttrType->fBufferData, pRecContext->fConfigTableIndex, numAttributes, pConfigFromXML );
			//throw if first nil since no more will be found otherwise proceed until nil
			if ( pLDAPAttrType == nil ) throw( (sInt32)eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable

			//set indicator of multiple native to standard mappings
			bTypeFound = false;
			while ( pLDAPAttrType != nil )
			{
				if (!bTypeFound)
				{
					//set up the length of the attribute type
					uiTypeLen = ::strlen( pAttrType->fBufferData );
					pOutAttrEntry = (tAttributeEntry *)::calloc( 1, sizeof( tAttributeEntry ) + uiTypeLen + kBuffPad );

					pOutAttrEntry->fAttributeSignature.fBufferSize		= uiTypeLen;
					pOutAttrEntry->fAttributeSignature.fBufferLength	= uiTypeLen;
					::memcpy( pOutAttrEntry->fAttributeSignature.fBufferData, pAttrType->fBufferData, uiTypeLen ); 
					bTypeFound = true;
					valCount = 0;
					uiDataLen = 0;
				}
				if ( (pLDAPAttrType[0] == '#') && (strlen(pLDAPAttrType) > 1) )
				{
					valCount++;
					uiDataLen += (strlen(pLDAPAttrType) - 1);
				}
				else
				{
					LDAP *aHost = fLDAPSessionMgr.LockSession(pRecContext);
					if ( (aHost != nil) && ( ( bValues = ldap_get_values_len(aHost, result, pLDAPAttrType )) != NULL ) )
					{
					
						// calculate the number of values for this attribute
						for (int ii = 0; bValues[ii] != NULL; ii++ )
							valCount++;
							
						// for each value of the attribute
						for (int i = 0; bValues[i] != NULL; i++ )
						{
							// Append attribute value
							uiDataLen += bValues[i]->bv_len;
						} // for each bValues[i]
						if (bValues != NULL)
						{
							ldap_value_free_len(bValues);
							bValues = NULL;
						}
					} // if ( aHost != nil ) && bValues = ldap_get_values_len ...
					fLDAPSessionMgr.UnLockSession(pRecContext);
				}

				//cleanup pLDAPAttrType if needed
				if (pLDAPAttrType != nil)
				{
					delete (pLDAPAttrType);
					pLDAPAttrType = nil;
				}
				numAttributes++;
				//get the next mapping
				pLDAPAttrType = MapAttrToLDAPType( (const char *)(pRecContext->fOpenRecordType), pAttrType->fBufferData, pRecContext->fConfigTableIndex, numAttributes, pConfigFromXML );
			} // while ( pLDAPAttrType != nil )

			if ( pOutAttrEntry == nil )
			{
				inData->fOutAttrInfoPtr = nil;
				throw( (sInt32)eDSAttributeNotFound );
			}
			// Number of attribute values
			pOutAttrEntry->fAttributeValueCount = valCount;
			//KW seems arbitrary max length
			pOutAttrEntry->fAttributeValueMaxSize = 255;
			//set the total length of all the attribute data
			pOutAttrEntry->fAttributeDataSize = uiDataLen;
			//assign the result out
			inData->fOutAttrInfoPtr = pOutAttrEntry;
			
        	ldap_msgfree( result );
	        result = nil;
	
        } // retrieve the result from the LDAP server
        else
        {
        	siResult = eDSRecordNotFound;
        }
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	if (pLDAPAttrType != nil)
	{
		delete( pLDAPAttrType );
		pLDAPAttrType = nil;
	}				

	return( siResult );

} // GetRecAttribInfo


//------------------------------------------------------------------------------------
//	* GetRecAttrValueByIndex
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::GetRecAttrValueByIndex ( sGetRecordAttributeValueByIndex *inData )
{
	sInt32					siResult		= eDSNoErr;
	uInt32					uiDataLen		= 0;
	tDataNodePtr			pAttrType		= nil;
	char				   *pLDAPAttrType	= nil;
	tAttributeValueEntryPtr	pOutAttrValue	= nil;
	sLDAPContextData	   *pRecContext		= nil;
	LDAPMessage			   *result			= nil;
	struct berval		  **bValues;
	unsigned long			valCount		= 0;
	bool					bFoundIt		= false;
	int						numAttributes	= 1;
	uInt32					literalLength	= 0;
	bool					bStripCryptPrefix		= false;

	try
	{
		pRecContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInRecRef );
		if ( pRecContext  == nil ) throw( (sInt32)eDSBadContextData );

		siResult = GetRecRefLDAPMessage( pRecContext, &result );
		if ( siResult != eDSNoErr ) throw( siResult);

		pAttrType = inData->fInAttrType;
		if ( pAttrType == nil ) throw( (sInt32)eDSEmptyAttributeType );
		if ( pAttrType->fBufferData == nil ) throw( (sInt32)eDSEmptyAttributeType );

        if ( result != nil )
        {
        
			if (::strcmp( kDS1AttrPasswordPlus, pAttrType->fBufferData ) == 0)
			{
				//want to remove leading "{crypt}" prefix from password if it exists
				//iff the request came in for the DS std password type ie. not native
				bStripCryptPrefix = true;
			}
			
			//get the first mapping
			numAttributes = 1;
			pLDAPAttrType = MapAttrToLDAPType( (const char *)(pRecContext->fOpenRecordType), pAttrType->fBufferData, pRecContext->fConfigTableIndex, numAttributes, pConfigFromXML );
			//throw if first nil since no more will be found otherwise proceed until nil
			if ( pLDAPAttrType == nil ) throw( (sInt32)eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable

			//set indicator of multiple native to standard mappings
			bFoundIt = false;
			while ( ( pLDAPAttrType != nil ) && (!bFoundIt) )
			{
				if (pLDAPAttrType[0] == '#')
				{
					valCount++;
					literalLength = strlen(pLDAPAttrType + 1);
					if ( (valCount == inData->fInAttrValueIndex) && (literalLength > 0) )
					{
						// Append attribute value
						uiDataLen = literalLength;
						
						pOutAttrValue = (tAttributeValueEntry *)::calloc( 1, sizeof( tAttributeValueEntry ) + uiDataLen + kBuffPad );

						pOutAttrValue->fAttributeValueData.fBufferSize = uiDataLen + kBuffPad;
						pOutAttrValue->fAttributeValueData.fBufferLength = uiDataLen;
						pOutAttrValue->fAttributeValueID = CalcCRC( pLDAPAttrType + 1 );
						::memcpy( pOutAttrValue->fAttributeValueData.fBufferData, pLDAPAttrType + 1, uiDataLen );

						bFoundIt = true;
					}
				}
				else
				{
					LDAP *aHost = fLDAPSessionMgr.LockSession(pRecContext);
					if ( (aHost != nil) && ( ( bValues = ldap_get_values_len (aHost, result, pLDAPAttrType )) != NULL ) )
					{
					
						// for each value of the attribute
						for (int i = 0; bValues[i] != NULL; i++ )
						{

							valCount++;
							if (valCount == inData->fInAttrValueIndex)
							{
								uInt32 anOffset = 0;
								if (bStripCryptPrefix)
								{
									//case insensitive compare with "crypt" string
									if ( ( bValues[i]->bv_len > 6) &&
										(strncasecmp(bValues[i]->bv_val,"{crypt}",7) == 0) )
									{
										// use the value without "{crypt}" prefix
										anOffset = 7;
									}
								}
								// Append attribute value
								uiDataLen = bValues[i]->bv_len - anOffset;
								
								pOutAttrValue = (tAttributeValueEntry *)::calloc( 1, sizeof( tAttributeValueEntry ) + uiDataLen + kBuffPad );
	
								pOutAttrValue->fAttributeValueData.fBufferSize = uiDataLen + kBuffPad;
								pOutAttrValue->fAttributeValueData.fBufferLength = uiDataLen;
								if ( bValues[i]->bv_val != nil )
								{
									pOutAttrValue->fAttributeValueID = CalcCRC( bValues[i]->bv_val ); //no offset for CRC
									::memcpy( pOutAttrValue->fAttributeValueData.fBufferData, bValues[i]->bv_val + anOffset, uiDataLen );
								}
	
								bFoundIt = true;
								break;
							} // if valCount correct one
						} // for each bValues[i]
						if (bValues != NULL)
						{
							ldap_value_free_len(bValues);
							bValues = NULL;
						}
					} // if bValues = ldap_get_values_len ...
					fLDAPSessionMgr.UnLockSession(pRecContext);
				}
						
				if (bFoundIt)
				{
					inData->fOutEntryPtr = pOutAttrValue;				
				}
						
				//cleanup pLDAPAttrType if needed
				if (pLDAPAttrType != nil)
				{
					delete (pLDAPAttrType);
					pLDAPAttrType = nil;
				}
				numAttributes++;
				//get the next mapping
				pLDAPAttrType = MapAttrToLDAPType( (const char *)(pRecContext->fOpenRecordType), pAttrType->fBufferData, pRecContext->fConfigTableIndex, numAttributes, pConfigFromXML );
			} // while ( pLDAPAttrType != nil )
			
        	ldap_msgfree( result );
	        result = nil;
	        if (!bFoundIt)
	        {
				if (valCount < inData->fInAttrValueIndex)
				{
					siResult = eDSIndexOutOfRange;
				}
				else
				{
					siResult = eDSAttributeNotFound;
				}
	        }
        } // retrieve the result from the LDAP server
        else
        {
        	siResult = eDSRecordNotFound;
        }

		if (pLDAPAttrType != nil)
		{
			delete( pLDAPAttrType );
			pLDAPAttrType = nil;
		}				
			
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // GetRecAttrValueByIndex


//------------------------------------------------------------------------------------
//	* GetRecordAttributeValueByID
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::GetRecordAttributeValueByID ( sGetRecordAttributeValueByID *inData )
{
	sInt32					siResult		= eDSNoErr;
	uInt32					uiDataLen		= 0;
	tDataNodePtr			pAttrType		= nil;
	char				   *pLDAPAttrType	= nil;
	tAttributeValueEntryPtr	pOutAttrValue	= nil;
	sLDAPContextData	   *pRecContext		= nil;
	LDAPMessage			   *result			= nil;
	struct berval		  **bValues;
	bool					bFoundIt		= false;
	int						numAttributes	= 1;
	uInt32					crcVal			= 0;
	uInt32					literalLength	= 0;
	bool					bStripCryptPrefix		= false;

	try
	{
		pRecContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInRecRef );
		if ( pRecContext  == nil ) throw( (sInt32)eDSBadContextData );

		siResult = GetRecRefLDAPMessage( pRecContext, &result );
		if ( siResult != eDSNoErr ) throw( siResult);

		pAttrType = inData->fInAttrType;
		if ( pAttrType == nil ) throw( (sInt32)eDSEmptyAttributeType );
		if ( pAttrType->fBufferData == nil ) throw( (sInt32)eDSEmptyAttributeType );

        if ( result != nil )
        {
        
			if (::strcmp( kDS1AttrPasswordPlus, pAttrType->fBufferData ) == 0)
			{
				//want to remove leading "{crypt}" prefix from password if it exists
				//iff the request came in for the DS std password type ie. not native
				bStripCryptPrefix = true;
			}
			
			//get the first mapping
			numAttributes = 1;
			pLDAPAttrType = MapAttrToLDAPType( (const char *)(pRecContext->fOpenRecordType), pAttrType->fBufferData, pRecContext->fConfigTableIndex, numAttributes, pConfigFromXML );
			//throw if first nil since no more will be found otherwise proceed until nil
			if ( pLDAPAttrType == nil ) throw( (sInt32)eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable

			//set indicator of multiple native to standard mappings
			bFoundIt = false;
			while ( ( pLDAPAttrType != nil ) && (!bFoundIt) )
			{
				if (pLDAPAttrType[0] == '#')
				{
					literalLength = strlen(pLDAPAttrType + 1);
					if (literalLength > 0)
					{
						crcVal = CalcCRC( pLDAPAttrType + 1 );
						if ( crcVal == inData->fInValueID )
						{
							// Append attribute value
							uiDataLen = literalLength;
							
							pOutAttrValue = (tAttributeValueEntry *)::calloc( 1, sizeof( tAttributeValueEntry ) + uiDataLen + kBuffPad );
	
							pOutAttrValue->fAttributeValueData.fBufferSize = uiDataLen + kBuffPad;
							pOutAttrValue->fAttributeValueData.fBufferLength = uiDataLen;
							pOutAttrValue->fAttributeValueID = crcVal;
							::memcpy( pOutAttrValue->fAttributeValueData.fBufferData, pLDAPAttrType + 1, uiDataLen );
	
							bFoundIt = true;
						}
					}
				}
				else
				{
					LDAP *aHost = fLDAPSessionMgr.LockSession(pRecContext);
					if ( (aHost != nil) && ( ( bValues = ldap_get_values_len(aHost, result, pLDAPAttrType )) != NULL ) )
					{
					
						// for each value of the attribute
						for (int i = 0; bValues[i] != NULL; i++ )
						{
							//use CRC here - WITF we assume string??
							crcVal = CalcCRC( bValues[i]->bv_val );
							if ( crcVal == inData->fInValueID )
							{
								uInt32 anOffset = 0;
								if (bStripCryptPrefix)
								{
									//case insensitive compare with "crypt" string
									if ( ( bValues[i]->bv_len > 6) &&
										(strncasecmp(bValues[i]->bv_val,"{crypt}",7) == 0) )
									{
										// use the value without "{crypt}" prefix
										anOffset = 7;
									}
								}
								// Append attribute value
								uiDataLen = bValues[i]->bv_len - anOffset;
								
								pOutAttrValue = (tAttributeValueEntry *)::calloc( 1, sizeof( tAttributeValueEntry ) + uiDataLen + kBuffPad );
								pOutAttrValue->fAttributeValueData.fBufferSize = uiDataLen + kBuffPad;
								pOutAttrValue->fAttributeValueData.fBufferLength = uiDataLen;
								if ( bValues[i]->bv_val != nil )
								{
									pOutAttrValue->fAttributeValueID = crcVal;
									::memcpy( pOutAttrValue->fAttributeValueData.fBufferData, bValues[i]->bv_val + anOffset, uiDataLen );
								}
	
								bFoundIt = true;
								break;
							} // if ( crcVal == inData->fInValueID )
						} // for each bValues[i]
						ldap_value_free_len(bValues);
						bValues = NULL;
					} // if bValues = ldap_get_values_len ...
					fLDAPSessionMgr.UnLockSession(pRecContext);
				}
				if (bFoundIt)
				{
					inData->fOutEntryPtr = pOutAttrValue;				
				}
						
				//cleanup pLDAPAttrType if needed
				if (pLDAPAttrType != nil)
				{
					delete (pLDAPAttrType);
					pLDAPAttrType = nil;
				}
				numAttributes++;
				//get the next mapping
				pLDAPAttrType = MapAttrToLDAPType( (const char *)(pRecContext->fOpenRecordType), pAttrType->fBufferData, pRecContext->fConfigTableIndex, numAttributes, pConfigFromXML );
			} // while ( pLDAPAttrType != nil )
			
        	ldap_msgfree( result );
	        result = nil;
	
        } // retrieve the result from the LDAP server
        else
        {
        	throw( (sInt32)eDSRecordNotFound ); //KW???
        }

		if (pLDAPAttrType != nil)
		{
			delete( pLDAPAttrType );
			pLDAPAttrType = nil;
		}				
			
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // GetRecordAttributeValueByID


// ---------------------------------------------------------------------------
//	* GetNextStdAttrType
// ---------------------------------------------------------------------------

char* CLDAPv3Plugin::GetNextStdAttrType ( char *inRecType, uInt32 inConfigTableIndex, int &inputIndex )
{
	char				   *outResult	= nil;
	sLDAPConfigData		   *pConfig		= nil;

	//idea here is to use the inIndex to request a specific std Attr Type
	//if inIndex is 1 then the first std attr type will be returned
	//if inIndex is >= 1 and <= totalCount then that std attr type will be returned
	//if inIndex is <= 0 then nil will be returned
	//if inIndex is > totalCount nil will be returned
	//note the inIndex will reference the inIndexth entry ie. start at 1
	//caller can increment the inIndex starting at one and get std attr types until nil is returned

	if (inputIndex > 0)
	{
		//if no std attr type is found then NIL will be returned
		
		//find the attr map that we need using inConfigTableIndex
		if (( inConfigTableIndex < gLDAPConfigTableLen) && ( inConfigTableIndex >= 0 ))
		{
	        pConfig = (sLDAPConfigData *)gLDAPConfigTable->GetItemData( inConfigTableIndex );
	        if (pConfig != nil)
	        {
				if (pConfigFromXML != nil)
				{
					//TODO need to "try" to get a default here if no mappings
					//KW maybe NOT ie. directed open can work with native types
					if ( (pConfig->fRecordTypeMapCFArray == 0) || (pConfig->fAttrTypeMapCFArray == 0) )
					{
					}
					else if (	( CFArrayGetTypeID() == CFGetTypeID( pConfig->fRecordTypeMapCFArray ) ) &&
							( CFArrayGetTypeID() == CFGetTypeID( pConfig->fAttrTypeMapCFArray ) ) )
					{
						outResult = pConfigFromXML->ExtractStdAttr(inRecType, pConfig->fRecordTypeMapCFArray, pConfig->fAttrTypeMapCFArray, inputIndex );
					}
				}
        	}
		}
	}// if (inIndex > 0)

	return( outResult );

} // GetNextStdAttrType


//------------------------------------------------------------------------------------
//	* DoAttributeValueSearch
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::DoAttributeValueSearch ( sDoAttrValueSearchWithData *inData )
{
    sInt32					siResult			= eDSNoErr;
    bool					bAttribOnly			= false;
    uInt32					uiCount				= 0;
    uInt32					uiTotal				= 0;
    CAttributeList		   *cpRecTypeList		= nil;
    CAttributeList		   *cpAttrTypeList 		= nil;
    char				   *pSearchStr			= nil;
	char				   *pRecType			= nil;
    char				   *pAttrType			= nil;
    char				   *pLDAPRecType		= nil;
    tDirPatternMatch		pattMatch			= eDSExact;
    sLDAPContextData	   *pContext			= nil;
    sLDAPContinueData	   *pLDAPContinue		= nil;
    CBuff				   *outBuff				= nil;
	tDataList			   *pTmpDataList		= nil;
    int						numRecTypes			= 1;
    bool					bBuffFull			= false;
    bool					separateRecTypes	= false;
    uInt32					countDownRecTypes	= 0;
	bool					bOCANDGroup			= false;
	CFArrayRef				OCSearchList		= nil;
	char				   *aCompoundExp		= " "; //added fake string since pAttrType is used in strcmp below
	ber_int_t				scope				= LDAP_SCOPE_SUBTREE;

    try
    {

        // Verify all the parameters
        if ( inData  == nil ) throw( (sInt32) eMemoryError );
        if ( inData->fOutDataBuff  == nil ) throw( (sInt32)eDSEmptyBuffer );
        if (inData->fOutDataBuff->fBufferSize == 0) throw( (sInt32)eDSEmptyBuffer );

        if ( inData->fInRecTypeList  == nil ) throw( (sInt32)eDSEmptyRecordTypeList );
//depends on call in        if ( inData->fInAttribTypeList  == nil ) throw( (sInt32)eDSEmptyAttributeTypeList );

        // Node context data
        pContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInNodeRef );
        if ( pContext  == nil ) throw( (sInt32)eDSBadContextData );

		if ( inData->fIOContinueData == nil )
		{
			pLDAPContinue = (sLDAPContinueData *)::calloc( 1, sizeof( sLDAPContinueData ) );

			gLDAPContinueTable->AddItem( pLDAPContinue, inData->fInNodeRef );

            //parameters used for data buffering
            pLDAPContinue->fRecTypeIndex = 1;
            pLDAPContinue->fTotalRecCount = 0;
            pLDAPContinue->fLimitRecSearch = 0;
			
			//check if the client has requested a limit on the number of records to return
			//we only do this the first call into this context for pLDAPContinue
			if (inData->fOutMatchRecordCount >= 0)
			{
				pLDAPContinue->fLimitRecSearch = inData->fOutMatchRecordCount;
			}
		}
		else
		{
			pLDAPContinue = (sLDAPContinueData *)inData->fIOContinueData;
			if ( gLDAPContinueTable->VerifyItem( pLDAPContinue ) == false )
			{
				throw( (sInt32)eDSInvalidContinueData );
			}
		}

        // start with the continue set to nil until buffer gets full and there is more data
        //OR we have more record types to look through
        inData->fIOContinueData			= nil;
		//return zero here if nothing found
		inData->fOutMatchRecordCount	= 0;

        // copy the buffer data into a more manageable form
        outBuff = new CBuff();
        if ( outBuff  == nil ) throw( (sInt32) eMemoryError );

        siResult = outBuff->Initialize( inData->fOutDataBuff, true );
        if ( siResult != eDSNoErr ) throw( siResult );

        siResult = outBuff->GetBuffStatus();
        if ( siResult != eDSNoErr ) throw( siResult );

        siResult = outBuff->SetBuffType( 'StdA' );
        if ( siResult != eDSNoErr ) throw( siResult );

        // Get the record type list
        // Record type mapping for LDAP to DS API is dealt with below
        cpRecTypeList = new CAttributeList( inData->fInRecTypeList );
        if ( cpRecTypeList  == nil ) throw( (sInt32)eDSEmptyRecordTypeList );
        //save the number of rec types here to use in separating the buffer data
        countDownRecTypes = cpRecTypeList->GetCount() - pLDAPContinue->fRecTypeIndex + 1;
        if (cpRecTypeList->GetCount() == 0) throw( (sInt32)eDSEmptyRecordTypeList );

        // Get the attribute pattern match type
        pattMatch = inData->fInPattMatchType;

        // Get the attribute type
		pAttrType = inData->fInAttrType->fBufferData;
		if (	(pattMatch == eDSCompoundExpression) || 
				(pattMatch == eDSiCompoundExpression) )
		{
			pAttrType = aCompoundExp; //used fake string since pAttrType is used in strcmp below
		}
		else
		{
			if ( pAttrType == nil ) throw( (sInt32)eDSEmptyAttributeType );
		}

        // Get the attribute string match
		pSearchStr = inData->fInPatt2Match->fBufferData;
		if ( pSearchStr == nil ) throw( (sInt32)eDSEmptyPattern2Match );

		if ( inData->fType == kDoAttributeValueSearchWithData )
		{
			// Get the attribute list
			cpAttrTypeList = new CAttributeList( inData->fInAttrTypeRequestList );
			if ( cpAttrTypeList == nil ) throw( (sInt32)eDSEmptyAttributeTypeList );
			if (cpAttrTypeList->GetCount() == 0) throw( (sInt32)eDSEmptyAttributeTypeList );

			// Get the attribute info only flag
			bAttribOnly = inData->fInAttrInfoOnly;
		}
		else
		{
			pTmpDataList = dsBuildListFromStringsPriv( kDSAttributesAll, nil );
			if ( pTmpDataList != nil )
			{
				cpAttrTypeList = new CAttributeList( pTmpDataList );
				if ( cpAttrTypeList == nil ) throw( (sInt32)eDSEmptyAttributeTypeList );
				if (cpAttrTypeList->GetCount() == 0) throw( (sInt32)eDSEmptyAttributeTypeList );
			}
		}

        // get records of these types
        while ((( cpRecTypeList->GetAttribute( pLDAPContinue->fRecTypeIndex, &pRecType ) == eDSNoErr ) && (!bBuffFull)) && (!separateRecTypes))
        {
        	//mapping rec types - if std to native
        	numRecTypes = 1;
			bOCANDGroup = false;
			if (OCSearchList != nil)
			{
				CFRelease(OCSearchList);
				OCSearchList = nil;
			}
            pLDAPRecType = MapRecToLDAPType( (const char *)pRecType, pContext->fConfigTableIndex, numRecTypes, &bOCANDGroup, &OCSearchList, &scope, pConfigFromXML );
            //throw on first nil
            if ( pLDAPRecType == nil ) throw( (sInt32)eDSInvalidRecordType );
            while (( pLDAPRecType != nil ) && (!bBuffFull))
            {
				bBuffFull = false;
				if (	(::strcmp( pAttrType, kDSAttributesAll ) == 0 ) ||
						(::strcmp( pAttrType, kDSAttributesStandardAll ) == 0 ) ||
						(::strcmp( pAttrType, kDSAttributesNativeAll ) == 0 ) )
				{
					//go get me all records that have any attribute equal to pSearchStr with pattMatch constraint
					//KW this is a very difficult search to do
					//approach A: set up a very complex search filter to pass to the LDAP server
					//need to be able to handle all standard types that are mapped
					//CHOSE THIS approach B: get each record and parse it completely using native attr types
					//approach C: just like A but concentrate on a selected subset of attr types
					siResult = FindAllRecords( pSearchStr, pRecType, pLDAPRecType, pattMatch, cpAttrTypeList, pContext, pLDAPContinue, bAttribOnly, outBuff, uiCount, bOCANDGroup, OCSearchList, scope );
				}
				else
				{
					//go get me all records that have pAttrType equal to pSearchStr with pattMatch constraint
					siResult = FindTheseRecords( pAttrType, pSearchStr, pRecType, pLDAPRecType, pattMatch, cpAttrTypeList, pContext, pLDAPContinue, bAttribOnly, outBuff, uiCount, bOCANDGroup, OCSearchList, scope );
				}

				//outBuff->GetDataBlockCount( &uiCount );
				//cannot use this as there may be records added from different record names
				//at which point the first name will fill the buffer with n records and
				//uiCount is reported as n but as the second name fills the buffer with m MORE records
				//the statement above will return the total of n+m and add it to the previous n
				//so that the total erroneously becomes 2n+m and not what it should be as n+m
				//therefore uiCount is extracted directly out of the FindxxxRecord(s) calls

				if ( siResult == CBuff::kBuffFull )
				{
					bBuffFull = true;
					//set continue if there is more data available
					inData->fIOContinueData = pLDAPContinue;
					
					// check to see if buffer is empty and no entries added
					// which implies that the buffer is too small
					if ( ( uiCount == 0 ) && ( uiTotal == 0 ) )
					{
						throw( (sInt32)eDSBufferTooSmall );
					}

					uiTotal += uiCount;
					inData->fOutMatchRecordCount = uiTotal;
					outBuff->SetLengthToSize();
					siResult = eDSNoErr;
				}
				else if ( siResult == eDSNoErr )
				{
					uiTotal += uiCount;
//	                pContext->fRecNameIndex++;
				}

                if (pLDAPRecType != nil)
                {
                    delete( pLDAPRecType );
                    pLDAPRecType = nil;
                }
                
                if (!bBuffFull)
                {
	                numRecTypes++;
					bOCANDGroup = false;
					if (OCSearchList != nil)
					{
						CFRelease(OCSearchList);
						OCSearchList = nil;
					}
	                //get the next mapping
	                pLDAPRecType = MapRecToLDAPType( (const char *)pRecType, pContext->fConfigTableIndex, numRecTypes, &bOCANDGroup, &OCSearchList, &scope, pConfigFromXML );
                }
                
            } // while mapped Rec Type != nil
            
            if (!bBuffFull)
            {
	            pRecType = nil;
	            pLDAPContinue->fRecTypeIndex++;
//	            pContext->fRecNameIndex = 1;
	            //reset the LDAP message ID to zero since now going to go after a new type
	            pLDAPContinue->msgId = 0;
	            
	            //KW? here we decide to exit with data full of the current type of records
	            // and force a good exit with the data we have so we can come back for the next rec type
            	separateRecTypes = true;
                //set continue since there may be more data available
                inData->fIOContinueData = pLDAPContinue;
				siResult = eDSNoErr;
				
				//however if this was the last rec type then there will be no more data
	            // check the number of rec types left
	            countDownRecTypes--;
	            if (countDownRecTypes == 0)
	            {
	                inData->fIOContinueData = nil;
				}
            }
            
        } // while loop over record types

        if (( siResult == eDSNoErr ) & (!bBuffFull))
        {
            if ( uiTotal == 0 )
            {
                outBuff->ClearBuff();
            }
            else
            {
                outBuff->SetLengthToSize();
            }

            inData->fOutMatchRecordCount = uiTotal;
        }
    } // try
    
    catch ( sInt32 err )
    {
		siResult = err;
    }

	if (OCSearchList != nil)
	{
		CFRelease(OCSearchList);
		OCSearchList = nil;
	}
	
	if ( (inData->fIOContinueData == nil) && (pLDAPContinue != nil) )
	{
		// we've decided not to return continue data, so we should clean up
		gLDAPContinueTable->RemoveItem( pLDAPContinue );
		pLDAPContinue = nil;
	}

	if (pLDAPRecType != nil)
	{
		delete( pLDAPRecType );
		pLDAPRecType = nil;
	}

    if ( cpRecTypeList != nil )
    {
		delete( cpRecTypeList );
		cpRecTypeList = nil;
    }

    if ( cpAttrTypeList != nil )
    {
		delete( cpAttrTypeList );
		cpAttrTypeList = nil;
    }

	if ( pTmpDataList != nil )
	{
		dsDataListDeallocatePriv( pTmpDataList );
		free(pTmpDataList);
		pTmpDataList = nil;
	}

    if ( outBuff != nil )
    {
		delete( outBuff );
		outBuff = nil;
    }

    return( siResult );

} // DoAttributeValueSearch


//------------------------------------------------------------------------------------
//	* DoTheseAttributesMatch
//------------------------------------------------------------------------------------

bool CLDAPv3Plugin::DoTheseAttributesMatch(	sLDAPContextData	   *inContext,
											char				   *inAttrName,
											tDirPatternMatch		pattMatch,
											LDAPMessage			   *inResult)
{
	char			   *pAttr					= nil;
	char			   *pVal					= nil;
	BerElement		   *ber						= nil;
	struct berval	  **bValues;
	bool				bFoundMatch				= false;
	LDAP			   *aHost					= nil;

	//let's check all the attribute values for a match on the input name
	//with the given patt match constraint - first match found we stop and
	//then go get it all
	//TODO - room for optimization here
	aHost = fLDAPSessionMgr.LockSession(inContext);
	if (aHost != nil)
	{
		for (	pAttr = ldap_first_attribute(aHost, inResult, &ber );
				pAttr != NULL; pAttr = ldap_next_attribute(aHost, inResult, ber ) )
		{
			if (( bValues = ldap_get_values_len(aHost, inResult, pAttr )) != NULL)
			{
				// for each value of the attribute
				for (int i = 0; bValues[i] != NULL; i++ )
				{
					//need this since bValues might be binary data with no NULL terminator
					pVal = (char *) calloc(1,bValues[i]->bv_len + 1);
					memcpy(pVal, bValues[i]->bv_val, bValues[i]->bv_len);
					if (DoesThisMatch(pVal, inAttrName, pattMatch))
					{
						bFoundMatch = true;
						free(pVal);
						pVal = nil;
						break;
					}
					else
					{
						free(pVal);
						pVal = nil;
					}
				} // for each bValues[i]
				ldap_value_free_len(bValues);
				bValues = NULL;
				
				if (bFoundMatch)
				{
					if (pAttr != nil)
					{
						ldap_memfree( pAttr );
					}
					break;
				}
			} // if bValues = ldap_get_values_len ...
				
			if (pAttr != nil)
			{
				ldap_memfree( pAttr );
			}
		} // for ( loop over ldap_next_attribute )
	} // if aHost != nil
	fLDAPSessionMgr.UnLockSession(inContext);
	
	if (ber != nil)
	{
		ber_free( ber, 0 );
	}

	return( bFoundMatch );

} // DoTheseAttributesMatch


// ---------------------------------------------------------------------------
//	* DoesThisMatch
// ---------------------------------------------------------------------------

bool CLDAPv3Plugin::DoesThisMatch (	const char		   *inString,
									const char		   *inPatt,
									tDirPatternMatch	inPattMatch )
{
	const char	   *p			= nil;
	bool			bMatched	= false;
	char		   *string1;
	char		   *string2;
	uInt32			length1		= 0;
	uInt32			length2		= 0;
	uInt32			uMatch		= 0;
	uInt16			usIndex		= 0;

	if ( (inString == nil) || (inPatt == nil) )
	{
		return( false );
	}

	length1 = strlen(inString);
	length2 = strlen(inPatt);
	string1 = new char[length1 + 1];
	string2 = new char[length2 + 1];
	
	if ( (inPattMatch >= eDSExact) && (inPattMatch <= eDSRegularExpression) )
	{
		strcpy(string1,inString);
		strcpy(string2,inPatt);
	}
	else
	{
		p = inString;
		for ( usIndex = 0; usIndex < length1; usIndex++  )
		{
			string1[usIndex] = toupper( *p );
			p++;
		}

		p = inPatt;
		for ( usIndex = 0; usIndex < length2; usIndex++  )
		{
			string2[usIndex] = toupper( *p );
			p++;
		}
	}

	uMatch = (uInt32) inPattMatch;
	switch ( uMatch )
	{
		case eDSExact:
		case eDSiExact:
		{
			if ( strcmp( string1, string2 ) == 0 )
			{
				bMatched = true;
			}
		}
		break;

		case eDSStartsWith:
		case eDSiStartsWith:
		{
			if ( strncmp( string1, string2, length2 ) == 0 )
			{
				bMatched = true;
			}
		}
		break;

		case eDSEndsWith:
		case eDSiEndsWith:
		{
			if ( length1 >= length2 )
			{
				if ( strcmp( string1 + length1 - length2, string2 ) == 0 )
				{
					bMatched = true;
				}
			}
		}
		break;

		case eDSContains:
		case eDSiContains:
		{
			if ( length1 >= length2 )
			{
				if ( strstr( string1, string2 ) != nil )
				{
					bMatched = true;
				}
			}
		}
		break;

		case eDSLessThan:
		case eDSiLessThan:
		{
			if ( strcmp( string1, string2 ) < 0 )
			{
				bMatched = true;
			}
		}
		break;

		case eDSGreaterThan:
		case eDSiGreaterThan:
		{
			if ( strcmp( string1, string2 ) > 0 )
			{
				bMatched = true;
			}
		}
		break;

		case eDSLessEqual:
		case eDSiLessEqual:
		{
			if ( strcmp( string1, string2 ) <= 0 )
			{
				bMatched = true;
			}
		}
		break;

		case eDSGreaterEqual:
		case eDSiGreaterEqual:
		{
			if ( strcmp( string1, string2 ) >= 0 )
			{
				bMatched = true;
			}
		}
		break;

		case eDSAnyMatch:
		default:
			break;
	}
	
	if (string1 != nil)
	{
		delete(string1);
	}

	if (string2 != nil)
	{
		delete(string2);
	}

	return( bMatched );

} // DoesThisMatch


//------------------------------------------------------------------------------------
//	* FindAllRecords
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::FindAllRecords (	char			   *inConstAttrName,
										char			   *inRecType,
										char			   *inNativeRecType,
										tDirPatternMatch	patternMatch,
										CAttributeList	   *inAttrTypeList,
										sLDAPContextData   *inContext,
										sLDAPContinueData  *inContinue,
										bool				inAttrOnly,
										CBuff			   *inBuff,
										uInt32			   &outRecCount,
										bool				inbOCANDGroup,
										CFArrayRef			inOCSearchList,
										ber_int_t			inScope )
{
    sInt32				siResult		= eDSNoErr;
    sInt32				siValCnt		= 0;
    int					ldapReturnCode 	= 0;
    bool				bufferFull		= false;
    LDAPMessage		   *result			= nil;
    char			   *recName			= nil;
    sLDAPConfigData	   *pConfig			= nil;
    int					searchTO		= 0;
	bool				bFoundMatch		= false;
	CDataBuff		   *aRecData		= nil;
	CDataBuff		   *aAttrData		= nil;
    char			   *queryFilter		= nil;
	char			  **attrs			= nil;

	//TODO why not optimize and save the queryFilter in the continue data for the next call in?
	
	//build the record query string
	queryFilter = BuildLDAPQueryFilter(	nil,
										nil,
										eDSAnyMatch,
										inContext->fConfigTableIndex,
										false,
										(const char *)inRecType,
										inNativeRecType,
										inbOCANDGroup,
										inOCSearchList,
                                        pConfigFromXML );

	outRecCount = 0; //need to track how many records were found by this call to FindAllRecords
	
    try
    {
    	if (inContext == nil ) throw( (sInt32)eDSInvalidContext);
    	if (inContinue == nil ) throw( (sInt32)eDSInvalidContinueData);
    	
    	// check to make sure the queryFilter is not nil
    	if ( queryFilter == nil ) throw( (sInt32)eDSNullParameter );
    	
 		aRecData = new CDataBuff();
		if ( aRecData  == nil ) throw( (sInt32) eMemoryError );
		aAttrData = new CDataBuff();
		if ( aAttrData  == nil ) throw( (sInt32) eMemoryError );

		//retrieve the config data
		//don't need to retrieve for the case of "generic unknown" so don't check index 0
		if (( inContext->fConfigTableIndex < gLDAPConfigTableLen) && ( inContext->fConfigTableIndex >= 1 ))
		{
			pConfig = (sLDAPConfigData *)gLDAPConfigTable->GetItemData( inContext->fConfigTableIndex );
			if (pConfig != nil)
			{
				searchTO	= pConfig->fSearchTimeout;
			}
		}
		
		//LDAP search with rebind capability
		DSDoSearch (	inRecType,
						inNativeRecType,
						inAttrTypeList,
						inContext,
						inContinue,
						pConfigFromXML,
						inScope,
						fLDAPSessionMgr,
						queryFilter,
						ldapReturnCode,
						searchTO,
						result);

		while ( 	( (ldapReturnCode == LDAP_RES_SEARCH_ENTRY) || (ldapReturnCode == LDAP_RES_SEARCH_RESULT) )
					&& !(bufferFull)
					&& (	(inContinue->fTotalRecCount < inContinue->fLimitRecSearch) ||
							(inContinue->fLimitRecSearch == 0) ) )
        {
			// check to see if there is a match
            // package the record into the DS format into the buffer
            // steps to add an entry record to the buffer
            // build the fRecData header
            // build the fAttrData
            // append the fAttrData to the fRecData
            // add the fRecData to the buffer inBuff
			
			if (ldapReturnCode == LDAP_RES_SEARCH_ENTRY)
			{
				bFoundMatch = false;
				if ( DoTheseAttributesMatch(inContext, inConstAttrName, patternMatch, result) )
				{
					bFoundMatch = true;
	
					aRecData->Clear();
		
					if ( inRecType != nil )
					{
						aRecData->AppendShort( ::strlen( inRecType ) );
						aRecData->AppendString( inRecType );
					} // what to do if the inRecType is nil? - never get here then
					else
					{
						aRecData->AppendShort( ::strlen( "Record Type Unknown" ) );
						aRecData->AppendString( "Record Type Unknown" );
					}
		
					// need to get the record name
					recName = GetRecordName( inRecType, result, inContext, siResult );
					if ( siResult != eDSNoErr ) throw( siResult );
					if ( recName != nil )
					{
						aRecData->AppendShort( ::strlen( recName ) );
						aRecData->AppendString( recName );
		
						delete ( recName );
						recName = nil;
					}
					else
					{
						aRecData->AppendShort( ::strlen( "Record Name Unknown" ) );
						aRecData->AppendString( "Record Name Unknown" );
					}
		
					// need to calculate the number of attribute types ie. siValCnt
					// also need to extract the attributes and place them into aAttrData
					//siValCnt = 0;
		
					aAttrData->Clear();
					siResult = GetTheseAttributes( inRecType, inAttrTypeList, result, inAttrOnly, inContext, siValCnt, aAttrData );
					if ( siResult != eDSNoErr ) throw( siResult );
					
					//add the attribute info to the fRecData
					if ( siValCnt == 0 )
					{
						// Attribute count
						aRecData->AppendShort( 0 );
					}
					else
					{
						// Attribute count
						aRecData->AppendShort( siValCnt );
						aRecData->AppendBlock( aAttrData->GetData(), aAttrData->GetLength() );
					}
		
					// add the aRecData now to the inBuff
					siResult = inBuff->AddData( aRecData->GetData(), aRecData->GetLength() );
					
				} // DoTheseAttributesMatch?
			}
            // need to check if the buffer is full
			// need to handle full buffer and keep the result alive for the next call in
            if (siResult == CBuff::kBuffFull)
            {
                bufferFull = true;
                
                //save the result if buffer is full
                inContinue->pResult = result;
            }
            else if ( siResult == eDSNoErr )
            {
                ldap_msgfree( result );
				result = nil;
                
				if (bFoundMatch)
				{
					outRecCount++; //another record added
					inContinue->fTotalRecCount++;
				}
				
                //make sure no result is carried in the context
                inContinue->pResult = nil;

				//only get next result if buffer is not full
				DSGetSearchLDAPResult (	fLDAPSessionMgr,
										inContext,
										searchTO,
										result,
										LDAP_MSG_ONE,
										inContinue->msgId,
										ldapReturnCode,
										false);
            }
            else
            {
                //make sure no result is carried in the context
                inContinue->pResult = nil;
                throw( (sInt32)eDSInvalidBuffFormat );
            }

        } // while loop over entries

		if ( (result != inContinue->pResult) && (result != nil) )
		{
			ldap_msgfree( result );
			result = nil;
		}
    } // try block

    catch ( sInt32 err )
    {
        siResult = err;
    }

	if (attrs != nil)
	{
		int aIndex = 0;
		while (attrs[aIndex] != nil)
		{
			free(attrs[aIndex]);
			attrs[aIndex] = nil;
			aIndex++;
		}
		free(attrs);
		attrs = nil;
	}
	
	if ( queryFilter != nil )
	{
		delete( queryFilter );
		queryFilter = nil;
	}

	if ( aRecData != nil )
	{
		delete( aRecData );
		aRecData = nil;
	}
	if ( aAttrData != nil )
	{
		delete( aAttrData );
		aAttrData = nil;
	}

    return( siResult );

} // FindAllRecords


//------------------------------------------------------------------------------------
//	* FindTheseRecords
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::FindTheseRecords (	char			   *inConstAttrType,
										char			   *inConstAttrName,
										char			   *inRecType,
										char			   *inNativeRecType,
										tDirPatternMatch	patternMatch,
										CAttributeList	   *inAttrTypeList,
										sLDAPContextData   *inContext,
										sLDAPContinueData  *inContinue,
										bool				inAttrOnly,
										CBuff			   *inBuff,
										uInt32			   &outRecCount,
										bool				inbOCANDGroup,
										CFArrayRef			inOCSearchList,
										ber_int_t			inScope )
{
	sInt32					siResult		= eDSNoErr;
    sInt32					siValCnt		= 0;
    int						ldapReturnCode 	= 0;
    bool					bufferFull		= false;
    LDAPMessage			   *result			= nil;
    char				   *recName			= nil;
    char				   *queryFilter		= nil;
    sLDAPConfigData		   *pConfig			= nil;
    int						searchTO		= 0;
	CDataBuff			   *aRecData		= nil;
	CDataBuff			   *aAttrData		= nil;
	char				  **attrs			= nil;

	//TODO why not optimize and save the queryFilter in the continue data for the next call in?
	
	//build the record query string
	queryFilter = BuildLDAPQueryFilter(	inConstAttrType,
										inConstAttrName,
										patternMatch,
										inContext->fConfigTableIndex,
										false,
										(const char *)inRecType,
										inNativeRecType,
										inbOCANDGroup,
										inOCSearchList,
                                        pConfigFromXML );
	    
	outRecCount = 0; //need to track how many records were found by this call to FindTheseRecords
	
    try
    {
    
    	if (inContext == nil ) throw( (sInt32)eDSInvalidContext);
    	if (inContinue == nil ) throw( (sInt32)eDSInvalidContinueData);
    	
    	// check to make sure the queryFilter is not nil
    	if ( queryFilter == nil ) throw( (sInt32)eDSNullParameter );
    	
 		aRecData = new CDataBuff();
		if ( aRecData  == nil ) throw( (sInt32) eMemoryError );
		aAttrData = new CDataBuff();
		if ( aAttrData  == nil ) throw( (sInt32) eMemoryError );

		//retrieve the config data
		//don't need to retrieve for the case of "generic unknown" so don't check index 0
		if (( inContext->fConfigTableIndex < gLDAPConfigTableLen) && ( inContext->fConfigTableIndex >= 1 ))
		{
			pConfig = (sLDAPConfigData *)gLDAPConfigTable->GetItemData( inContext->fConfigTableIndex );
			if (pConfig != nil)
			{
				searchTO	= pConfig->fSearchTimeout;
			}
		}
		
		//LDAP search with rebind capability
		DSDoSearch (	inRecType,
						inNativeRecType,
						inAttrTypeList,
						inContext,
						inContinue,
						pConfigFromXML,
						inScope,
						fLDAPSessionMgr,
						queryFilter,
						ldapReturnCode,
						searchTO,
						result );

		while ( 	( (ldapReturnCode == LDAP_RES_SEARCH_ENTRY) || (ldapReturnCode == LDAP_RES_SEARCH_RESULT) )
					&& !(bufferFull)
					&& (	(inContinue->fTotalRecCount < inContinue->fLimitRecSearch) ||
							(inContinue->fLimitRecSearch == 0) ) )
        {
            // package the record into the DS format into the buffer
            // steps to add an entry record to the buffer
            // build the fRecData header
            // build the fAttrData
            // append the fAttrData to the fRecData
            // add the fRecData to the buffer inBuff
			if (ldapReturnCode == LDAP_RES_SEARCH_ENTRY)
			{
				aRecData->Clear();
	
				if ( inRecType != nil )
				{
					aRecData->AppendShort( ::strlen( inRecType ) );
					aRecData->AppendString( inRecType );
				}
				else
				{
					aRecData->AppendShort( ::strlen( "Record Type Unknown" ) );
					aRecData->AppendString( "Record Type Unknown" );
				}
	
				// need to get the record name
				recName = GetRecordName( inRecType, result, inContext, siResult );
				if ( siResult != eDSNoErr ) throw( siResult );
				if ( recName != nil )
				{
					aRecData->AppendShort( ::strlen( recName ) );
					aRecData->AppendString( recName );
	
					delete ( recName );
					recName = nil;
				}
				else
				{
					aRecData->AppendShort( ::strlen( "Record Name Unknown" ) );
					aRecData->AppendString( "Record Name Unknown" );
				}
	
				// need to calculate the number of attribute types ie. siValCnt
				// also need to extract the attributes and place them into fAttrData
				//siValCnt = 0;
	
				aAttrData->Clear();
				siResult = GetTheseAttributes( inRecType, inAttrTypeList, result, inAttrOnly, inContext, siValCnt, aAttrData );
				if ( siResult != eDSNoErr ) throw( siResult );
				
				//add the attribute info to the fRecData
				if ( siValCnt == 0 )
				{
					// Attribute count
					aRecData->AppendShort( 0 );
				}
				else
				{
					// Attribute count
					aRecData->AppendShort( siValCnt );
					aRecData->AppendBlock( aAttrData->GetData(), aAttrData->GetLength() );
				}
	
				// add the fRecData now to the inBuff
				siResult = inBuff->AddData( aRecData->GetData(), aRecData->GetLength() );
			}
            // need to check if the buffer is full
			// need to handle full buffer and keep the result alive for the next call in
            if (siResult == CBuff::kBuffFull)
            {
                bufferFull = true;
                
                //save the result if buffer is full
                inContinue->pResult = result;
            }
            else if ( siResult == eDSNoErr )
            {
                ldap_msgfree( result );
				result = nil;
                
				if (ldapReturnCode == LDAP_RES_SEARCH_ENTRY)
				{
					outRecCount++; //another record added
					inContinue->fTotalRecCount++;
				}
				
                //make sure no result is carried in the context
                inContinue->pResult = nil;

				//only get next result if buffer is not full
				DSGetSearchLDAPResult (	fLDAPSessionMgr,
										inContext,
										searchTO,
										result,
										LDAP_MSG_ONE,
										inContinue->msgId,
										ldapReturnCode,
										false);
            }
            else
            {
                //make sure no result is carried in the context
                inContinue->pResult = nil;
                throw( (sInt32)eDSInvalidBuffFormat );
            }
            
        } // while loop over entries

		if ( (result != inContinue->pResult) && (result != nil) )
		{
			ldap_msgfree( result );
			result = nil;
		}

    } // try block

    catch ( sInt32 err )
    {
        siResult = err;
    }

	if (attrs != nil)
	{
		int aIndex = 0;
		while (attrs[aIndex] != nil)
		{
			free(attrs[aIndex]);
			attrs[aIndex] = nil;
			aIndex++;
		}
		free(attrs);
		attrs = nil;
	}
	
	if ( queryFilter != nil )
	{
		delete( queryFilter );
		queryFilter = nil;
	}

	if ( aRecData != nil )
	{
		delete( aRecData );
		aRecData = nil;
	}
	if ( aAttrData != nil )
	{
		delete( aAttrData );
		aAttrData = nil;
	}

    return( siResult );

} // FindTheseRecords


//------------------------------------------------------------------------------------
//	* DoAuthentication
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::DoAuthentication ( sDoDirNodeAuth *inData )
{
	sInt32						siResult		= eDSAuthFailed;
	uInt32						uiAuthMethod	= 0;
	sLDAPContextData			*pContext		= nil;
	sLDAPContinueData   		*pContinueData	= NULL;
	char*						userName		= NULL;
	LDAPv3AuthAuthorityHandlerProc	handlerProc 	= NULL;
	
	try
	{
		pContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInNodeRef );
		if ( pContext == nil ) throw( (sInt32)eDSInvalidNodeRef );
		if ( inData->fInAuthStepData == nil ) throw( (sInt32)eDSNullAuthStepData );
		
		if ( inData->fIOContinueData != NULL )
		{
			// get info from continue
			pContinueData = (sLDAPContinueData *)inData->fIOContinueData;
			if ( gLDAPContinueTable->VerifyItem( pContinueData ) == false )
			{
				throw( (sInt32)eDSInvalidContinueData );
			}
			
			if ( eDSInvalidContinueData == nil ) throw( (sInt32)(pContinueData->fAuthHandlerProc) );
			handlerProc = (LDAPv3AuthAuthorityHandlerProc)(pContinueData->fAuthHandlerProc);
			if (handlerProc != NULL)
			{
				siResult = (handlerProc)(inData->fInNodeRef, inData->fInAuthMethod, pContext,
										 &pContinueData, inData->fInAuthStepData, 
										 inData->fOutAuthStepDataResponse, 
										 inData->fInDirNodeAuthOnlyFlag, 
										 pContinueData->fAuthAuthorityData,
                                         pConfigFromXML, fLDAPSessionMgr,
										 (const char *)kDSStdRecordTypeUsers);
			}
		}
		else
		{
			unsigned long idx = 0;
			unsigned long aaCount = 0;
			char **aaArray;
                
			// first call
			// note: if GetAuthMethod() returns eDSAuthMethodNotSupported we want to keep going
			// because password server users may still be able to use the method with PSPlugin
			siResult = GetAuthMethod( inData->fInAuthMethod, &uiAuthMethod );
			if ( ( siResult != eDSNoErr ) && ( siResult != eDSAuthMethodNotSupported ) )
				throw(siResult);

			if ( ( siResult == eDSNoErr) && ( uiAuthMethod == kAuth2WayRandom ) )
			{
				// for 2way random the first buffer is the username
				if ( inData->fInAuthStepData->fBufferLength > inData->fInAuthStepData->fBufferSize ) throw( (sInt32)eDSInvalidBuffFormat );
				userName = (char*)calloc( inData->fInAuthStepData->fBufferLength + 1, 1 );
				strncpy( userName, inData->fInAuthStepData->fBufferData, inData->fInAuthStepData->fBufferLength );
			}
			else
			{
				siResult = GetUserNameFromAuthBuffer( inData->fInAuthStepData, 1, &userName );
				if ( siResult != eDSNoErr ) throw( siResult );
			}
			
			// get the auth authority
			siResult = GetAuthAuthority( pContext,
										userName,
										pConfigFromXML,
										fLDAPSessionMgr,
										&aaCount,
										&aaArray,
										(const char *)kDSStdRecordTypeUsers );
			
			if ( siResult == eDSNoErr ) 
			{
				// loop through all possibilities for set
				// do first auth authority that supports the method for check password
				bool bLoopAll = IsWriteAuthRequest( uiAuthMethod );
				bool bIsSecondary = false;
				char* aaVersion = NULL;
				char* aaTag = NULL;
				char* aaData = NULL;
				siResult = eDSAuthMethodNotSupported;
				
				while ( idx < aaCount &&
						(siResult == eDSAuthMethodNotSupported || (bLoopAll && siResult == eDSNoErr)) )
				{
					//parse this value of auth authority
					siResult = ParseAuthAuthority( aaArray[idx], &aaVersion, &aaTag, &aaData );
					if ( aaArray[idx] )
						free( aaArray[idx] );
					
					// JT need to check version
					if (siResult != eDSNoErr) {
						siResult = eDSAuthFailed;
						break;
					}
					handlerProc = GetLDAPv3AuthAuthorityHandler( aaTag );
					if (handlerProc != NULL)
					{
						siResult = (handlerProc)(inData->fInNodeRef, inData->fInAuthMethod, pContext, 
												&pContinueData, 
												inData->fInAuthStepData, 
												inData->fOutAuthStepDataResponse,
												inData->fInDirNodeAuthOnlyFlag,aaData,
												pConfigFromXML, fLDAPSessionMgr,
												(const char *)kDSStdRecordTypeUsers);
						if ( siResult == eDSNoErr )
						{
							if ( pContinueData != NULL )
							{
								// we are supposed to return continue data
								// remember the proc we used
								pContinueData->fAuthHandlerProc = (void*)handlerProc;
								pContinueData->fAuthAuthorityData = aaData;
								aaData = NULL;
								break;
							}
							else
							{
								bIsSecondary = true;
							}
						}
					}
					else {
						siResult = eDSAuthMethodNotSupported;
					}
					if (aaVersion != NULL) {
						free(aaVersion);
						aaVersion = NULL;
					}
					if (aaTag != NULL) {
						free(aaTag);
						aaTag = NULL;
					}
					if (aaData != NULL) {
						free(aaData);
						aaData = NULL;
					}
					++idx;
				}
 
				// need to free remaining attributes
				while( idx < aaCount )
				{
					if ( aaArray[idx] )
						free( aaArray[idx] );
					idx++;
				}

				if ( bIsSecondary && siResult == eDSAuthMethodNotSupported )
					siResult = eDSNoErr;
				
				if (aaVersion != NULL) {
					free(aaVersion);
					aaVersion = NULL;
				}
				if (aaTag != NULL) {
					free(aaTag);
					aaTag = NULL;
				}
				if (aaData != NULL) {
					free(aaData);
					aaData = NULL;
				}
				
				if (aaArray != NULL) {
					free( aaArray );
					aaArray = NULL;
				}
			}
			else
			{
				//revert to basic
				siResult = DoBasicAuth(inData->fInNodeRef,inData->fInAuthMethod, pContext, 
									&pContinueData, inData->fInAuthStepData,
									inData->fOutAuthStepDataResponse,
									inData->fInDirNodeAuthOnlyFlag,NULL,
									pConfigFromXML, fLDAPSessionMgr,
									(const char *)kDSStdRecordTypeUsers);
				if (pContinueData != NULL && siResult == eDSNoErr)
				{
					// we are supposed to return continue data
					// remember the proc we used
					pContinueData->fAuthHandlerProc = (void*)CLDAPv3Plugin::DoBasicAuth;
				}
			}
		}
	}
    
	catch( sInt32 err )
	{
		siResult = err;
	}
	
	if (userName != NULL)
	{
		free(userName);
		userName = NULL;
	}

	inData->fResult = siResult;
	inData->fIOContinueData = pContinueData;

	return( siResult );

} // DoAuthentication


//------------------------------------------------------------------------------------
//	* DoAuthenticationOnRecordType
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::DoAuthenticationOnRecordType ( sDoDirNodeAuthOnRecordType *inData )
{
	sInt32						siResult		= eDSAuthFailed;
	uInt32						uiAuthMethod	= 0;
	sLDAPContextData			*pContext		= nil;
	sLDAPContinueData   		*pContinueData	= NULL;
	char*						userName		= NULL;
	LDAPv3AuthAuthorityHandlerProc	handlerProc 	= NULL;
	
	try
	{
		pContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInNodeRef );
		if ( pContext == nil ) throw( (sInt32)eDSInvalidNodeRef );
		if ( inData->fInAuthStepData == nil ) throw( (sInt32)eDSNullAuthStepData );
		if ( inData->fInRecordType == nil ) throw( (sInt32)eDSNullRecType );
		if ( inData->fInRecordType->fBufferData == nil ) throw( (sInt32)eDSNullRecType );
		
		if ( inData->fIOContinueData != NULL )
		{
			// get info from continue
			pContinueData = (sLDAPContinueData *)inData->fIOContinueData;
			if ( gLDAPContinueTable->VerifyItem( pContinueData ) == false )
			{
				throw( (sInt32)eDSInvalidContinueData );
			}
			
			if ( eDSInvalidContinueData == nil ) throw( (sInt32)(pContinueData->fAuthHandlerProc) );
			handlerProc = (LDAPv3AuthAuthorityHandlerProc)(pContinueData->fAuthHandlerProc);
			if (handlerProc != NULL)
			{
				siResult = (handlerProc)(inData->fInNodeRef, inData->fInAuthMethod, pContext,
										 &pContinueData, inData->fInAuthStepData, 
										 inData->fOutAuthStepDataResponse, 
										 inData->fInDirNodeAuthOnlyFlag, 
										 pContinueData->fAuthAuthorityData,
                                         pConfigFromXML, fLDAPSessionMgr, inData->fInRecordType->fBufferData);
			}
		}
		else
		{
			unsigned long idx = 0;
			unsigned long aaCount = 0;
			char **aaArray;
                
			// first call
			// note: if GetAuthMethod() returns eDSAuthMethodNotSupported we want to keep going
			// because password server users may still be able to use the method with PSPlugin
			siResult = GetAuthMethod( inData->fInAuthMethod, &uiAuthMethod );
			if ( ( siResult != eDSNoErr ) && ( siResult != eDSAuthMethodNotSupported ) )
				throw(siResult);

			if ( ( siResult == eDSNoErr) && ( uiAuthMethod == kAuth2WayRandom ) )
			{
				// for 2way random the first buffer is the username
				if ( inData->fInAuthStepData->fBufferLength > inData->fInAuthStepData->fBufferSize ) throw( (sInt32)eDSInvalidBuffFormat );
				userName = (char*)calloc( inData->fInAuthStepData->fBufferLength + 1, 1 );
				strncpy( userName, inData->fInAuthStepData->fBufferData, inData->fInAuthStepData->fBufferLength );
			}
			else
			{
				siResult = GetUserNameFromAuthBuffer( inData->fInAuthStepData, 1, &userName );
				if ( siResult != eDSNoErr ) throw( siResult );
			}
			// get the auth authority
			siResult = GetAuthAuthority( pContext,
										userName,
										pConfigFromXML,
										fLDAPSessionMgr,
										&aaCount,
										&aaArray, inData->fInRecordType->fBufferData );
			
			if ( siResult == eDSNoErr ) 
			{
				// loop through all possibilities for set
				// do first auth authority that supports the method for check password
				bool bLoopAll = IsWriteAuthRequest( uiAuthMethod );
				bool bIsSecondary = false;
				char* aaVersion = NULL;
				char* aaTag = NULL;
				char* aaData = NULL;
				siResult = eDSAuthMethodNotSupported;
				
				while ( idx < aaCount &&
						(siResult == eDSAuthMethodNotSupported || (bLoopAll && siResult == eDSNoErr)) )
				{
					//parse this value of auth authority
					siResult = ParseAuthAuthority( aaArray[idx], &aaVersion, &aaTag, &aaData );
					if ( aaArray[idx] )
						free( aaArray[idx] );
					
					// JT need to check version
					if (siResult != eDSNoErr) {
						siResult = eDSAuthFailed;
						break;
					}
					handlerProc = GetLDAPv3AuthAuthorityHandler( aaTag );
					if (handlerProc != NULL) {
						siResult = (handlerProc)(inData->fInNodeRef, inData->fInAuthMethod, pContext, 
												&pContinueData, 
												inData->fInAuthStepData, 
												inData->fOutAuthStepDataResponse,
												inData->fInDirNodeAuthOnlyFlag,aaData,
												pConfigFromXML, fLDAPSessionMgr, inData->fInRecordType->fBufferData);
						if ( siResult == eDSNoErr )
						{
							if ( pContinueData != NULL )
							{
								// we are supposed to return continue data
								// remember the proc we used
								pContinueData->fAuthHandlerProc = (void*)handlerProc;
								pContinueData->fAuthAuthorityData = aaData;
								aaData = NULL;
								break;
							}
							else
							{
								bIsSecondary = true;
							}
						}
					} else {
						siResult = eDSAuthMethodNotSupported;
					}
					if (aaVersion != NULL) {
						free(aaVersion);
						aaVersion = NULL;
					}
					if (aaTag != NULL) {
						free(aaTag);
						aaTag = NULL;
					}
					if (aaData != NULL) {
						free(aaData);
						aaData = NULL;
					}
					++idx;
				}
				
				if ( bIsSecondary && siResult == eDSAuthMethodNotSupported )
					siResult = eDSNoErr;
				
				if (aaVersion != NULL) {
					free(aaVersion);
					aaVersion = NULL;
				}
				if (aaTag != NULL) {
					free(aaTag);
					aaTag = NULL;
				}
				if (aaData != NULL) {
					free(aaData);
					aaData = NULL;
				}
				
				if (aaArray != NULL) {
					free( aaArray );
					aaArray = NULL;
				}
			}
			else
			{
				//revert to basic
				siResult = DoBasicAuth(inData->fInNodeRef,inData->fInAuthMethod, pContext, 
									&pContinueData, inData->fInAuthStepData,
									inData->fOutAuthStepDataResponse,
									inData->fInDirNodeAuthOnlyFlag,NULL,
									pConfigFromXML, fLDAPSessionMgr, inData->fInRecordType->fBufferData);
				if (pContinueData != NULL && siResult == eDSNoErr)
				{
					// we are supposed to return continue data
					// remember the proc we used
					pContinueData->fAuthHandlerProc = (void*)CLDAPv3Plugin::DoBasicAuth;
				}
			}
		}
	}
    
	catch( sInt32 err )
	{
		siResult = err;
	}
	
	if (userName != NULL)
	{
		free(userName);
		userName = NULL;
	}

	inData->fResult = siResult;
	inData->fIOContinueData = pContinueData;

	return( siResult );

} // DoAuthenticationOnRecordType


//------------------------------------------------------------------------------------
//	* IsWriteAuthRequest
//------------------------------------------------------------------------------------

bool CLDAPv3Plugin::IsWriteAuthRequest ( uInt32 uiAuthMethod )
{
	switch ( uiAuthMethod )
	{
		case kAuthSetPasswd:
		case kAuthSetPasswdAsRoot:
		case kAuthChangePasswd:
			return true;
			
		default:
			return false;
	}
} // IsWriteAuthRequest


//------------------------------------------------------------------------------------
//	* DoPasswordServerAuth
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::DoPasswordServerAuth(
    tDirNodeReference inNodeRef,
    tDataNodePtr inAuthMethod, 
    sLDAPContextData* inContext, 
    sLDAPContinueData** inOutContinueData, 
    tDataBufferPtr inAuthData, 
    tDataBufferPtr outAuthData, 
    bool inAuthOnly,
    char* inAuthAuthorityData,
    CLDAPv3Configs *inConfigFromXML,
    CLDAPNode& inLDAPSessionMgr,
	const char *inRecordType )
{
    sInt32 result = eDSAuthFailed;
    sInt32 error;
    uInt32 authMethod;
    char *serverAddr;
    char *uidStr = NULL;
    long uidStrLen;
    tDataBufferPtr authDataBuff = NULL;
    tDataBufferPtr authDataBuffTemp = NULL;
    char *nodeName = NULL;
    char *userName = NULL;
    char *accountId = NULL;
	char *password = NULL;
    sLDAPContinueData *pContinue = NULL;
    tContextData continueData = NULL;
	bool	bNewMutex	= false;
    unsigned long aaCount = 0;
	char **aaArray;
           
    if ( !inAuthAuthorityData || *inAuthAuthorityData == '\0' )
        return eDSAuthParameterError;
    
    try
    {        
        serverAddr = strchr( inAuthAuthorityData, ':' );
        if ( serverAddr )
        {
            uidStrLen = serverAddr - inAuthAuthorityData;
            uidStr = (char *) calloc(1, uidStrLen+1);
            if ( uidStr == nil ) throw( eMemoryError );
            strncpy( uidStr, inAuthAuthorityData, uidStrLen );
            
            // advance past the colon
            serverAddr++;
            
            error = GetAuthMethod( inAuthMethod, &authMethod );            
            switch( authMethod )
            {
                case kAuth2WayRandom:
                    if ( inOutContinueData == nil )
                        throw( (sInt32)eDSNullParameter );
                    
                    if ( *inOutContinueData == nil )
                    {
                        pContinue = (sLDAPContinueData *)::calloc( 1, sizeof( sLDAPContinueData ) );
                        gLDAPContinueTable->AddItem( pContinue, inNodeRef );
                        
                        // make a buffer for the user ID
                        authDataBuff = ::dsDataBufferAllocatePriv( uidStrLen + 1 );
                        if ( authDataBuff == nil ) throw ( eMemoryError );
                        
                        // fill
                        strcpy( authDataBuff->fBufferData, uidStr );
                        authDataBuff->fBufferLength = uidStrLen;
                    }
                    else
                    {
                        pContinue = *inOutContinueData;
                        if ( gLDAPContinueTable->VerifyItem( pContinue ) == false )
                            throw( (sInt32)eDSInvalidContinueData );
                    }
                    break;
                    
                case kAuthSetPasswd:
				case kAuthSetPolicy:
                    {
                        char* aaVersion = NULL;
                        char* aaTag = NULL;
                        char* aaData = NULL;
                        unsigned int idx;
                        sInt32 lookupResult;
                        char* endPtr = NULL;
                        
                        // lookup the user that wasn't passed to us
                       	error = GetUserNameFromAuthBuffer( inAuthData, 3, &userName );
                        if ( error != eDSNoErr ) throw( error );
                        
                        // get the auth authority
                        //error = IsValidRecordName ( userName, "/users", inContext->fDomain, niDirID );
                        if ( error != eDSNoErr ) throw( (sInt32)eDSAuthFailed );
                        
						// lookup authauthority attribute
						error = GetAuthAuthority( inContext,
										userName,
										inConfigFromXML,
										inLDAPSessionMgr,
										&aaCount,
										&aaArray,
										inRecordType );
                     	
						if ( error != eDSNoErr ) throw( (sInt32)eDSAuthFailed );
						
                        // don't break or throw to guarantee cleanup
                        lookupResult = eDSAuthFailed;
                        for ( idx = 0; idx < aaCount && lookupResult == eDSAuthFailed; idx++ )
                        {
							//parse this value of auth authority
							error = ParseAuthAuthority( aaArray[idx], &aaVersion, &aaTag, &aaData );
							if ( aaArray[idx] ) {
								free( aaArray[idx] );
								aaArray[idx] = NULL;
							}
							
							// need to check version
                            if ( error != eDSNoErr )
                                lookupResult = eParameterError;
                            
                            if ( error == eDSNoErr && strcmp(aaTag, kDSTagAuthAuthorityPasswordServer) == 0 )
                            {
                                endPtr = strchr( aaData, ':' );
                                if ( endPtr == NULL )
                                {
                                    lookupResult = eParameterError;
                                }
                                else
                                {
                                    *endPtr = '\0';
                                    lookupResult = eDSNoErr;
                                }
                            }
                            
                            if (aaVersion != NULL) {
                                free(aaVersion);
                                aaVersion = NULL;
                            }
                            if (aaTag != NULL) {
                                free(aaTag);
                                aaTag = NULL;
                            }
                            if (lookupResult != eDSNoErr && aaData != NULL) {
                                free(aaData);
                                aaData = NULL;
                            }
                        }
                        
                        if ( lookupResult != eDSNoErr ) throw( eDSAuthFailed );
                        
                        // do the usual
                        error = RepackBufferForPWServer( inAuthData, uidStr, 1, &authDataBuffTemp );
                        if ( error != eDSNoErr ) throw( error );
                        
                        // put the admin user ID in slot 3
                        error = RepackBufferForPWServer( authDataBuffTemp, aaData, 3, &authDataBuff );
                        if (aaData != NULL) {
                            free(aaData);
                            aaData = NULL;
                        }
                        if ( error != eDSNoErr ) throw( error );
                    }
                    break;
                    
				case kAuthClearText:
				case kAuthNativeClearTextOK:
				case kAuthNativeNoClearText:
					{
						tDataListPtr dataList = dsAuthBufferGetDataListAllocPriv(inAuthData);
						if (dataList != NULL)
						{
							userName = dsDataListGetNodeStringPriv(dataList, 1);
							password = dsDataListGetNodeStringPriv(dataList, 2);
							// this allocates a copy of the string
							
							dsDataListDeallocatePriv(dataList);
							free(dataList);
							dataList = NULL;
						}
					}
					//fall through
                
                default:
                    error = RepackBufferForPWServer( inAuthData, uidStr, 1, &authDataBuff );
                    if ( error != eDSNoErr ) throw( error );
            }
            
			if ( inContext->fPWSRef == 0 )
            {
                error = ::dsOpenDirService( &inContext->fPWSRef );
                if ( error != eDSNoErr ) throw( error );
            }
            
            // JT we should only use the saved reference if the operation
            // requires prior authentication
            // otherwise we should open a new ref each time
            if ( inContext->fPWSNodeRef == 0 )
            {
                nodeName = (char *)calloc(1,strlen(serverAddr)+17);
                if ( nodeName == nil ) throw ( eMemoryError );
                
                sprintf( nodeName, "/PasswordServer/%s", serverAddr );
                error = PWOpenDirNode( inContext->fPWSRef, nodeName, &inContext->fPWSNodeRef );
                if ( error != eDSNoErr ) throw( error );
                
            }
            
            if ( pContinue )
                continueData = pContinue->fPassPlugContinueData;
            
            result = dsDoDirNodeAuth( inContext->fPWSNodeRef, inAuthMethod, inAuthOnly,
                                        authDataBuff, outAuthData, &continueData );
            
			//DBGLOG1( kLogPlugin, "LDAP PlugIn: result = %l", result );
			
			if ( result == eDSAuthNoAuthServerFound || result == eDSAuthServerError )
			{
				result = PWSetReplicaData( inContext, uidStr, inConfigFromXML, inLDAPSessionMgr );
				if ( result == eDSNoErr )
					result = dsDoDirNodeAuth( inContext->fPWSNodeRef, inAuthMethod, inAuthOnly,
												authDataBuff, outAuthData, &continueData );
			}
			
            if ( pContinue )
                pContinue->fPassPlugContinueData = continueData;
                
            if ( (result == eDSNoErr) && (inAuthOnly == false) && (userName != NULL) && (password != NULL) )
			{
                accountId = GetDNForRecordName ( userName, inContext, inConfigFromXML, inLDAPSessionMgr, inRecordType );
				
				LDAP *aHost = gLDAPv3Plugin->fLDAPSessionMgr.LockSession(inContext);
				// Here is the bind to the LDAP server
				if (aHost != nil)
				{
					LDAP *aLDAPHost = nil;
					result = inLDAPSessionMgr.AuthOpen(	inContext->fName,
														aHost,
														accountId,
														password,
														kDSStdAuthClearText,
														&aLDAPHost,
														&(inContext->fConfigTableIndex),
														true);
					if (result == eDSNoErr)
					{
						if (inContext->fLDAPSessionMutex == nil)
						{
							inContext->fLDAPSessionMutex = new DSMutexSemaphore();
							bNewMutex = true;
						}
						inContext->fLDAPSessionMutex->Wait();
						inContext->fHost = aLDAPHost;
						//provision here for future different auth but now auth credential is a password only
						if (inContext->fAuthType == nil)
						{
							inContext->fAuthType = strdup(kDSStdAuthClearText);
						}
						inContext->authCallActive = true;
						if (inContext->fUserName != nil)
						{
							free(inContext->fUserName);
							inContext->fUserName = nil;
						}
						inContext->fUserName = strdup(accountId);
						if (inContext->fAuthCredential != nil)
						{
							free(inContext->fAuthCredential);
							inContext->fAuthCredential = nil;
						}
						inContext->fAuthCredential = strdup(password);
						inContext->fLDAPSessionMutex->Signal();
					}
					else
					{
						result = eDSAuthFailed;
					}
				}// if aHost != nil
				gLDAPv3Plugin->fLDAPSessionMgr.UnLockSession(inContext, false, bNewMutex);
            }
        }
    }
    
    catch(sInt32 err )
    {
        result = err;
    }
    catch( ... )
	{
		result = eDSAuthFailed;
	}
	
	if ( nodeName )
        free( nodeName );

	if ( uidStr != NULL )
	{
		free( uidStr );
		uidStr = NULL;
	}
    if ( userName != NULL )
	{
		free( userName );
		userName = NULL;
	}
    if ( password != NULL )
	{
		free( password );
		password = NULL;
	}
    if ( accountId != NULL )
	{
		free( accountId );
		accountId = NULL;
	}
  
    if ( authDataBuff )
        dsDataBufferDeallocatePriv( authDataBuff );
    if ( authDataBuffTemp )
        dsDataBufferDeallocatePriv( authDataBuffTemp );
                        
	return( result );
}

    
//------------------------------------------------------------------------------------
//	* DoBasicAuth
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::DoBasicAuth(
    tDirNodeReference inNodeRef,
    tDataNodePtr inAuthMethod,
    sLDAPContextData* inContext,
    sLDAPContinueData** inOutContinueData,
    tDataBufferPtr inAuthData,
    tDataBufferPtr outAuthData,
    bool inAuthOnly,
    char* inAuthAuthorityData,
    CLDAPv3Configs *inConfigFromXML,
    CLDAPNode& inLDAPSessionMgr,
	const char *inRecordType )
{
	sInt32					siResult		= noErr;
	UInt32					uiAuthMethod	= 0;
    
	try
	{
		siResult = GetAuthMethod( inAuthMethod, &uiAuthMethod );
		if ( siResult == noErr )
		{
			switch( uiAuthMethod )
			{
				case kAuthSetPolicy:
					siResult = eDSAuthMethodNotSupported;
					break;
				
				case kAuthCrypt:
				case kAuthNativeNoClearText:
					siResult = DoUnixCryptAuth( inContext, inAuthData, inConfigFromXML, inLDAPSessionMgr, inRecordType );
					if( siResult == eDSAuthFailedClearTextOnly )
					{
						if (( inContext->fConfigTableIndex < gLDAPConfigTableLen) && ( inContext->fConfigTableIndex >= 1 ))
						{
							sLDAPConfigData *pConfig = (sLDAPConfigData *)gLDAPConfigTable->GetItemData( inContext->fConfigTableIndex );
							// eDSAuthFailedRequiring Clear Text only, so let's do a bind check if we have SSL enabled
							if( pConfig != nil && pConfig->bIsSSL )
							{
								siResult = DoClearTextAuth( inContext, inAuthData, inAuthOnly, inConfigFromXML, inLDAPSessionMgr, inRecordType );
							}
						}
					}
					break;

				case kAuthNativeClearTextOK:
					if ( inAuthOnly == true )
					{
						// auth only
						siResult = DoUnixCryptAuth( inContext, inAuthData, inConfigFromXML, inLDAPSessionMgr, inRecordType );
						if ( siResult == eDSNoErr )
						{
							if ( outAuthData->fBufferSize > ::strlen( kDSStdAuthCrypt ) )
							{
								::strcpy( outAuthData->fBufferData, kDSStdAuthCrypt );
							}
						}
					}
					if ( (siResult != eDSNoErr) || (inAuthOnly == false) )
					{
						siResult = DoClearTextAuth( inContext, inAuthData, inAuthOnly, inConfigFromXML, inLDAPSessionMgr, inRecordType );
						if ( siResult == eDSNoErr )
						{
							if ( outAuthData->fBufferSize > ::strlen( kDSStdAuthClearText ) )
							{
								::strcpy( outAuthData->fBufferData, kDSStdAuthClearText );
							}
						}
					}
					break;

				case kAuthClearText:
					siResult = DoClearTextAuth( inContext, inAuthData, inAuthOnly, inConfigFromXML, inLDAPSessionMgr, inRecordType );
					if ( siResult == eDSNoErr )
					{
						if ( outAuthData->fBufferSize > ::strlen( kDSStdAuthClearText ) )
						{
							::strcpy( outAuthData->fBufferData, kDSStdAuthClearText );
						}
					}
					break;
                
                case kAuthSetPasswd:
					siResult = DoSetPassword( inContext, inAuthData, inConfigFromXML, inLDAPSessionMgr, inRecordType );
					break;

				case kAuthSetPasswdAsRoot:
					siResult = DoSetPasswordAsRoot( inContext, inAuthData, inConfigFromXML, inLDAPSessionMgr, inRecordType );
					break;

				case kAuthChangePasswd:
					siResult = DoChangePassword( inContext, inAuthData, inConfigFromXML, inLDAPSessionMgr, inRecordType );
					break;
                
				default:
					siResult = eDSAuthMethodNotSupported;
					break;
			}
		}
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // DoBasicAuth


//------------------------------------------------------------------------------------
//	* DoSetPassword
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::DoSetPassword ( sLDAPContextData *inContext, tDataBuffer *inAuthData, 
                                      CLDAPv3Configs *inConfigFromXML, CLDAPNode& inLDAPSessionMgr,
									  const char *inRecordType )
{
	sInt32				siResult		= eDSAuthFailed;
	char			   *pData			= nil;
	char			   *userName		= nil;
	uInt32				userNameLen		= 0;
	char			   *userPwd			= nil;
	uInt32				userPwdLen		= 0;
	char			   *rootName		= nil;
	uInt32				rootNameLen		= 0;
	char			   *rootPwd			= nil;
	uInt32				rootPwdLen		= 0;
	uInt32				offset			= 0;
	uInt32				buffSize		= 0;
	uInt32				buffLen			= 0;
	char			   *accountId		= nil;

	try
	{
		if ( inContext == nil ) throw( (sInt32)eDSAuthFailed );
		if ( inAuthData == nil ) throw( (sInt32)eDSNullAuthStepData );

		pData = inAuthData->fBufferData;
		buffSize = inAuthData->fBufferSize;
		buffLen = inAuthData->fBufferLength;

		if ( buffLen > buffSize ) throw( (sInt32)eDSInvalidBuffFormat );

		// Get the length of the first data block and verify that it
		//	is greater than 0 and doesn't go past the end of the buffer
		//	(user name)

		if ( buffLen < 4 * sizeof( unsigned long ) + 2 ) throw( (sInt32)eDSInvalidBuffFormat );
		// need length for username, password, agent username, and agent password.
		// both usernames must be at least one character
		::memcpy( &userNameLen, pData, sizeof( unsigned long ) );
		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );
		if (userNameLen == 0) throw( (sInt32)eDSInvalidBuffFormat );
		if (offset + userNameLen > buffLen) throw( (sInt32)eDSInvalidBuffFormat );

		userName = (char *)::calloc( 1, userNameLen + 1 );
		::memcpy( userName, pData, userNameLen );
		pData += userNameLen;
		offset += userNameLen;
		if (offset + sizeof( unsigned long ) > buffLen) throw( (sInt32)eDSInvalidBuffFormat );

		// Get the user's new password
		::memcpy( &userPwdLen, pData, sizeof( unsigned long ) );
  		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );
		if (offset + userPwdLen > buffLen) throw( (sInt32)eDSInvalidBuffFormat );

		userPwd = (char *)::calloc( 1, userPwdLen + 1 );
		::memcpy( userPwd, pData, userPwdLen );
		pData += userPwdLen;
		offset += userPwdLen;
		if (offset + sizeof( unsigned long ) > buffLen) throw( (sInt32)eDSInvalidBuffFormat );

		// Get the agent user's name
		::memcpy( &rootNameLen, pData, sizeof( unsigned long ) );
		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );
		if (rootNameLen == 0) throw( (sInt32)eDSInvalidBuffFormat );
		if (rootNameLen > (buffSize - offset)) throw( (sInt32)eDSInvalidBuffFormat );

		rootName = (char *)::calloc( 1, rootNameLen + 1 );
		::memcpy( rootName, pData, rootNameLen );
		pData += rootNameLen;
		offset += rootNameLen;
		if (sizeof( unsigned long ) > (buffSize - offset)) throw( (sInt32)eDSInvalidBuffFormat );

		// Get the agent user's password
		::memcpy( &rootPwdLen, pData, sizeof( unsigned long ) );
		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );
		if (rootPwdLen > (buffSize - offset)) throw( (sInt32)eDSInvalidBuffFormat );

		rootPwd = (char *)::calloc( 1, rootPwdLen + 1 );
		::memcpy( rootPwd, pData, rootPwdLen );
		pData += rootPwdLen;
		offset += rootPwdLen;

		if (rootName)
		{
			//here again we choose the first match
			accountId = GetDNForRecordName ( rootName, inContext, inConfigFromXML, inLDAPSessionMgr, inRecordType );
		}

		LDAP *aHost = gLDAPv3Plugin->fLDAPSessionMgr.LockSession(inContext);
		if (aHost != nil)
		{
			LDAP* aLDAPHost = nil;
			// Here is the bind to the LDAP server as the agent
			siResult = inLDAPSessionMgr.AuthOpen(	inContext->fName,
													aHost,
													accountId,
													rootPwd,
													kDSStdAuthClearText,
													&aLDAPHost,
													&(inContext->fConfigTableIndex),
													false);
			if( siResult == eDSNoErr ) {
				int rc = exop_password_create( aLDAPHost, accountId, userPwd );
				if( rc != LDAP_SUCCESS ) {
					rc = standard_password_create( aLDAPHost, accountId, userPwd );
				}
	
				// *** gbv not sure what error codes to check for
				if ( ( rc == LDAP_INSUFFICIENT_ACCESS ) || ( rc == LDAP_INVALID_CREDENTIALS ) ) {
					siResult = eDSPermissionError;
				}
				else if ( rc == LDAP_NOT_SUPPORTED ) {
					siResult = eDSAuthMethodNotSupported;
				}
				else if ( rc == LDAP_SUCCESS ) {
					siResult = eDSNoErr;
				}
				else
				{
					siResult = eDSAuthFailed;
				}
	
				// get rid of our new connection
				ldap_unbind(aLDAPHost);
			}
			else
			{
				siResult = eDSAuthFailed;
			}
		}// if aHost != nil
		gLDAPv3Plugin->fLDAPSessionMgr.UnLockSession(inContext);
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	if ( userName != nil )
	{
		free( userName );
		userName = nil;
	}

	if ( userPwd != nil )
	{
		free( userPwd );
		userPwd = nil;
	}

	if ( rootName != nil )
	{
		free( rootName );
		rootName = nil;
	}

	if ( rootPwd != nil )
	{
		free( rootPwd );
		rootPwd = nil;
	}

	return( siResult );

} // DoSetPassword


//------------------------------------------------------------------------------------
//	* DoSetPasswordAsRoot
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::DoSetPasswordAsRoot ( sLDAPContextData *inContext, tDataBuffer *inAuthData, 
                                      CLDAPv3Configs *inConfigFromXML, CLDAPNode& inLDAPSessionMgr,
									  const char *inRecordType )
{
	sInt32				siResult		= eDSAuthFailed;
	char			   *pData			= nil;
	char			   *userName		= nil;
	uInt32				userNameLen		= 0;
	char			   *newPasswd		= nil;
	uInt32				newPwdLen		= 0;
	uInt32				offset			= 0;
	uInt32				buffSize		= 0;
	uInt32				buffLen			= 0;
	char			   *accountId		= nil;
	int					rc				= LDAP_LOCAL_ERROR;

	try
	{
		if ( inContext == nil ) throw( (sInt32)eDSAuthFailed );
		if ( inAuthData == nil ) throw( (sInt32)eDSNullAuthStepData );

		pData = inAuthData->fBufferData;
		buffSize = inAuthData->fBufferSize;
		buffLen = inAuthData->fBufferLength;

   		if ( buffLen > buffSize ) throw( (sInt32)eDSInvalidBuffFormat );

		// Get the length of the first data block and verify that it
		//	is greater than 0 and doesn't go past the end of the buffer
		//	(user name)

		if ( buffLen < 2 * sizeof( unsigned long ) + 1 ) throw( (sInt32)eDSInvalidBuffFormat );
		// need length for both username and password, plus username must be at least one character
		::memcpy( &userNameLen, pData, sizeof( unsigned long ) );
		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );
		if (userNameLen == 0) throw( (sInt32)eDSInvalidBuffFormat );
		if (userNameLen > (buffSize - offset)) throw( (sInt32)eDSInvalidBuffFormat );

		userName = (char *)::calloc( 1, userNameLen + 1 );
		::memcpy( userName, pData, userNameLen );
		pData += userNameLen;
		offset += userNameLen;
		if (offset + sizeof( unsigned long ) > buffLen) throw( (sInt32)eDSInvalidBuffFormat );

		// Get the users new password
		::memcpy( &newPwdLen, pData, sizeof( unsigned long ) );
		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );
		if (newPwdLen > (buffSize - offset)) throw( (sInt32)eDSInvalidBuffFormat );

		newPasswd = (char *)::calloc( 1, newPwdLen + 1 );
		::memcpy( newPasswd, pData, newPwdLen );
		pData += newPwdLen;
		offset += newPwdLen;

		if (userName)
		{
			//here again we choose the first match
			accountId = GetDNForRecordName ( userName, inContext, inConfigFromXML, inLDAPSessionMgr, inRecordType );
		}

		LDAP *aHost = inLDAPSessionMgr.LockSession(inContext);
		if (aHost != nil)
		{
			rc = exop_password_create( aHost, accountId, newPasswd );
			if( rc != LDAP_SUCCESS )
					rc = standard_password_create( aHost, accountId, newPasswd );
		}
		else
		{
			rc = LDAP_LOCAL_ERROR;
		}
        inLDAPSessionMgr.UnLockSession(inContext);
    
        // *** gbv not sure what error codes to check for
        if ( ( rc == LDAP_INSUFFICIENT_ACCESS ) || ( rc == LDAP_INVALID_CREDENTIALS ) )
        {
            siResult = eDSPermissionError;
        }
        else if ( rc == LDAP_NOT_SUPPORTED )
        {
            siResult = eDSAuthMethodNotSupported;
        }
        else if ( rc == LDAP_SUCCESS )
        {
            siResult = eDSNoErr;
        }
        else
        {
            siResult = eDSAuthFailed;
        }
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	if ( userName != nil )
	{
		free( userName );
		userName = nil;
	}

	if ( newPasswd != nil )
	{
		free( newPasswd );
		newPasswd = nil;
	}

	return( siResult );

} // DoSetPasswordAsRoot


//------------------------------------------------------------------------------------
//	* DoChangePassword
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::DoChangePassword ( sLDAPContextData *inContext, tDataBuffer *inAuthData, 
                                      CLDAPv3Configs *inConfigFromXML, CLDAPNode& inLDAPSessionMgr,
									  const char *inRecordType )
{
	sInt32			siResult		= eDSAuthFailed;
	char		   *pData			= nil;
	char		   *name			= nil;
	uInt32			nameLen			= 0;
	char		   *oldPwd			= nil;
	uInt32			OldPwdLen		= 0;
	char		   *newPwd			= nil;
	uInt32			newPwdLen		= 0;
	uInt32			offset			= 0;
	uInt32			buffSize		= 0;
	uInt32			buffLen			= 0;
	char		   *pathStr			= nil;
	char		   *accountId		= nil;

	try
	{
		if ( inContext == nil ) throw( (sInt32)eDSAuthFailed );

		if ( inAuthData == nil ) throw( (sInt32)eDSNullAuthStepData );

		pData = inAuthData->fBufferData;
		buffSize = inAuthData->fBufferSize;
		buffLen = inAuthData->fBufferLength;

		if ( buffLen > buffSize ) throw( (sInt32)eDSInvalidBuffFormat );

		// Get the length of the first data block and verify that it
		//	is greater than 0 and doesn't go past the end of the buffer

		if ( buffLen < 3 * sizeof( unsigned long ) + 1 ) throw( (sInt32)eDSInvalidBuffFormat );
		// we need at least 3 x 4 bytes for lengths of three strings,
		// and username must be at least 1 long
		::memcpy( &nameLen, pData, sizeof( unsigned long ) );
		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );
		if (nameLen == 0) throw( (sInt32)eDSInvalidBuffFormat );
		if (offset + nameLen > buffLen) throw( (sInt32)eDSInvalidBuffFormat );

		name = (char *)::calloc( 1, nameLen + 1 );
		::memcpy( name, pData, nameLen );
		pData += nameLen;
		offset += nameLen;
		if (offset + sizeof( unsigned long ) > buffLen) throw( (sInt32)eDSInvalidBuffFormat );

		::memcpy( &OldPwdLen, pData, sizeof( unsigned long ) );
		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );
		if (offset + OldPwdLen > buffLen) throw( (sInt32)eDSInvalidBuffFormat );

		oldPwd = (char *)::calloc( 1, OldPwdLen + 1 );
		::memcpy( oldPwd, pData, OldPwdLen );
		pData += OldPwdLen;
		offset += OldPwdLen;
		if (offset + sizeof( unsigned long ) > buffLen) throw( (sInt32)eDSInvalidBuffFormat );

		::memcpy( &newPwdLen, pData, sizeof( unsigned long ) );
   		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );
		if (offset + newPwdLen > buffLen) throw( (sInt32)eDSInvalidBuffFormat );

		newPwd = (char *)::calloc( 1, newPwdLen + 1 );
		::memcpy( newPwd, pData, newPwdLen );
		pData += newPwdLen;
		offset += newPwdLen;

        // Set up a new connection to LDAP for this user

		if (name)
		{
			//here again we choose the first match
			accountId = GetDNForRecordName ( name, inContext, inConfigFromXML, inLDAPSessionMgr, inRecordType );
		}

		//if username did not garner an accountId then fail authentication
		if (accountId == nil)
		{
			throw( (sInt32)eDSAuthFailed );
		}

		LDAP *aHost = gLDAPv3Plugin->fLDAPSessionMgr.LockSession(inContext);
		if (aHost != nil)
		{
			LDAP *aLDAPHost = nil;
			// Here is the bind to the LDAP server as the user in question
			siResult = inLDAPSessionMgr.AuthOpen(	inContext->fName,
													aHost,
													accountId,
													oldPwd,
													kDSStdAuthClearText,
													&aLDAPHost,
													&(inContext->fConfigTableIndex),
													false);
			if( siResult == eDSNoErr ) {
				// password change algorithm: first attempt the extended operation,
				// if that fails try to change the userPassword field directly
				int rc = exop_password_create( aLDAPHost, accountId, newPwd );
				if( rc != LDAP_SUCCESS ) {
					rc = standard_password_create( aLDAPHost, accountId, newPwd );
				}
	
				// *** gbv not sure what error codes to check for
				if ( ( rc == LDAP_INSUFFICIENT_ACCESS ) || ( rc == LDAP_INVALID_CREDENTIALS ) )
				{
					siResult = eDSPermissionError;
				}
				else if ( rc == LDAP_NOT_SUPPORTED )
				{
					siResult = eDSAuthMethodNotSupported;
				}
				else if ( rc == LDAP_SUCCESS )
				{
					siResult = eDSNoErr;
				}
				else
				{
					siResult = eDSAuthFailed;
				}
				
				// get rid of our new connection
				ldap_unbind(aLDAPHost);
			}
			else
			{
				siResult = eDSAuthFailed;
			}
		}// if aHost != nil
		gLDAPv3Plugin->fLDAPSessionMgr.UnLockSession(inContext);
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	if ( pathStr != nil )
	{
		free( pathStr );
		pathStr = nil;
	}

	if ( name != nil )
	{
		free( name );
		name = nil;
	}

	if ( newPwd != nil )
	{
		free( newPwd );
		newPwd = nil;
	}

	if ( oldPwd != nil )
	{
		free( oldPwd );
		oldPwd = nil;
	}

	return( siResult );

} // DoChangePassword


// ---------------------------------------------------------------------------
//	* GetAuthMethod
// ---------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::GetAuthMethod ( tDataNode *inData, uInt32 *outAuthMethod )
{
	sInt32			siResult		= noErr;
	char		   *p				= nil;

	if ( inData == nil )
	{
		*outAuthMethod = kAuthUnknownMethod;
		return( eDSAuthParameterError );
	}

	p = (char *)inData->fBufferData;

	DBGLOG1( kLogPlugin, "LDAP PlugIn: Attempting use of authentication method %s", p );

	if ( ::strcmp( p, kDSStdAuthClearText ) == 0 )
	{
		// Clear text auth method
		*outAuthMethod = kAuthClearText;
	}
	else if ( ::strcmp( p, kDSStdAuthNodeNativeClearTextOK ) == 0 )
	{
		// Node native auth method
		*outAuthMethod = kAuthNativeClearTextOK;
	}
	else if ( ::strcmp( p, kDSStdAuthNodeNativeNoClearText ) == 0 )
	{
		// Node native auth method
		*outAuthMethod = kAuthNativeNoClearText;
	}
	else if ( ::strcmp( p, kDSStdAuthCrypt ) == 0 )
	{
		// Unix crypt auth
		*outAuthMethod = kAuthCrypt;
	}
	else if ( ::strcmp( p, kDSStdAuthSetPasswd ) == 0 )
	{
		// Admin set password
		*outAuthMethod = kAuthSetPasswd;
	}
	else if ( ::strcmp( p, kDSStdAuthSetPasswdAsRoot ) == 0 )
	{
		// Admin set password
		*outAuthMethod = kAuthSetPasswdAsRoot;
	}
	else if ( ::strcmp( p, kDSStdAuthChangePasswd ) == 0 )
	{
		// User change password
		*outAuthMethod = kAuthChangePasswd;
	}
	else if ( ::strcmp( p, kDSStdAuthSetPolicy ) == 0 )
	{
		*outAuthMethod = kAuthSetPolicy;
	}
	else
	{
		*outAuthMethod = kAuthUnknownMethod;
		siResult = eDSAuthMethodNotSupported;
	}

	return( siResult );

} // GetAuthMethod


//--------------------------------------------------------------------------------------------------
// * PWOpenDirNode ()
//
//--------------------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::PWOpenDirNode ( tDirNodeReference fDSRef, char *inNodeName, tDirNodeReference *outNodeRef )
{
	sInt32			error		= eDSNoErr;
	sInt32			error2		= eDSNoErr;
	tDataList	   *pDataList	= nil;

	pDataList = ::dsBuildFromPathPriv( inNodeName, "/" );
    if ( pDataList != nil )
    {
        error = ::dsOpenDirNode( fDSRef, pDataList, outNodeRef );
        error2 = ::dsDataListDeallocatePriv( pDataList );
        free( pDataList );
    }

    return( error );

} // PWOpenDirNode


//--------------------------------------------------------------------------------------------------
// * PWSetReplicaData ()
//
//	Note:	inAuthorityData is the UserID + RSA_key, but the IP address should be pre-stripped by
//			the calling function.
//--------------------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::PWSetReplicaData( sLDAPContextData *inContext, const char *inAuthorityData, CLDAPv3Configs *inConfigFromXML, CLDAPNode& inLDAPSessionMgr )
{
	sInt32				error					= eDSNoErr;
	bool				bFoundWithHash			= false;
	long				replicaListLen			= 0;
	char				*rsaKeyPtr				= NULL;
	tDataBufferPtr		replicaBuffer			= NULL;
    tDataBufferPtr		replyBuffer				= NULL;
	unsigned long		valueCount				= 0;
	char				**valueData				= NULL;
	char				recordName[64];
	char				hashStr[34];
	
	// get /config/passwordserver_HEXHASH
	rsaKeyPtr = strchr( inAuthorityData, ',' );
	if ( rsaKeyPtr != NULL )
	{
		MD5_CTX ctx;
		unsigned char pubKeyHash[MD5_DIGEST_LENGTH];
		
		MD5_Init( &ctx );
		rsaKeyPtr++;
		MD5_Update( &ctx, rsaKeyPtr, strlen(rsaKeyPtr) );
		MD5_Final( pubKeyHash, &ctx );
		
		BinaryToHexConversion( pubKeyHash, MD5_DIGEST_LENGTH, hashStr );
		sprintf( recordName, "passwordserver_%s", hashStr );
		
		error = LookupAttribute( inContext, kDSStdRecordTypeConfig, recordName, kDS1AttrPasswordServerList, inConfigFromXML, inLDAPSessionMgr, &valueCount, &valueData );
		if ( error == eDSNoErr && valueCount > 0 )
			bFoundWithHash = true;
	}
	
	if ( ! bFoundWithHash )
	{
		error = LookupAttribute( inContext, kDSStdRecordTypeConfig, "passwordserver", kDS1AttrPasswordServerList, inConfigFromXML, inLDAPSessionMgr, &valueCount, &valueData );
		if ( error != eDSNoErr ) 
			return error;
	}
	
	try
	{
		if ( valueCount >= 1 )
		{
			replicaListLen = strlen( valueData[0] );
			replicaBuffer = ::dsDataBufferAllocatePriv( replicaListLen + 1 );
			if ( replicaBuffer == nil ) throw( (sInt32)eMemoryError );
			
			replyBuffer = ::dsDataBufferAllocatePriv( 1 );
			if ( replyBuffer == nil ) throw( (sInt32)eMemoryError );
			
			replicaBuffer->fBufferLength = replicaListLen;
			memcpy( replicaBuffer->fBufferData, valueData[0], replicaListLen );
			
			error = dsDoPlugInCustomCall( inContext->fPWSNodeRef, 1, replicaBuffer, replyBuffer );
			
			::dsDataBufferDeallocatePriv( replicaBuffer );
			::dsDataBufferDeallocatePriv( replyBuffer );
		}
	}
	catch( sInt32 catchErr )
	{
		error = catchErr;
	}
	
	// clean up
	if ( valueData != NULL )
	{
		for ( unsigned long index = 0; index < valueCount; index++ )
			if ( valueData[index] != NULL )
				free( valueData[index] );
		
		free( valueData );
	}
	
	return error;
}


//--------------------------------------------------------------------------------------------------
//	RepackBufferForPWServer
//
//	Replace the user name with the user id.
//--------------------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::RepackBufferForPWServer ( tDataBufferPtr inBuff, const char *inUserID, unsigned long inUserIDNodeNum, tDataBufferPtr *outBuff )
{
	sInt32 result = eDSNoErr;
    tDataListPtr dataList = NULL;
    tDataNodePtr dataNode = NULL;
	unsigned long index, nodeCount;
	unsigned long uidLen;
                
    if ( !inBuff || !inUserID || !outBuff )
        return eDSAuthParameterError;
    
    try
    {	
        uidLen = strlen(inUserID);
        *outBuff = ::dsDataBufferAllocatePriv( inBuff->fBufferLength + uidLen + 1 );
        if ( *outBuff == nil ) throw( (sInt32)eMemoryError );
        
        (*outBuff)->fBufferLength = 0;
        
        dataList = dsAuthBufferGetDataListAllocPriv(inBuff);
        if ( dataList == nil ) throw( (sInt32)eDSInvalidBuffFormat );
        
        nodeCount = dsDataListGetNodeCountPriv(dataList);
        if ( nodeCount < 1 ) throw( (sInt32)eDSInvalidBuffFormat );
        
        for ( index = 1; index <= nodeCount; index++ )
        {
            if ( index == inUserIDNodeNum )
            {
                // write 4 byte length
                memcpy( (*outBuff)->fBufferData + (*outBuff)->fBufferLength, &uidLen, sizeof(unsigned long) );
                (*outBuff)->fBufferLength += sizeof(unsigned long);
                
                // write uid
                memcpy( (*outBuff)->fBufferData + (*outBuff)->fBufferLength, inUserID, uidLen );
                (*outBuff)->fBufferLength += uidLen;
            }
            else
            {
                // get a node
                result = dsDataListGetNodeAllocPriv(dataList, index, &dataNode);
                if ( result != eDSNoErr ) throw( (sInt32)eDSInvalidBuffFormat );
            
                // copy it
                memcpy((*outBuff)->fBufferData + (*outBuff)->fBufferLength, &dataNode->fBufferLength, sizeof(unsigned long));
                (*outBuff)->fBufferLength += sizeof(unsigned long);
                
                memcpy( (*outBuff)->fBufferData + (*outBuff)->fBufferLength, dataNode->fBufferData, dataNode->fBufferLength );
                (*outBuff)->fBufferLength += dataNode->fBufferLength;
                
                // clean up
                dsDataBufferDeallocatePriv(dataNode);
            }
            
        }
        
        (void)dsDataListDeallocatePriv(dataList);
        free(dataList);
    }
    
    catch( sInt32 error )
    {
        result = error;
    }
    
    return result;
}


//------------------------------------------------------------------------------------
//	* GetAuthAuthority
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::GetAuthAuthority ( sLDAPContextData *inContext, const char *userName, CLDAPv3Configs *inConfigFromXML, CLDAPNode& inLDAPSessionMgr, unsigned long *outAuthCount, char **outAuthAuthority[], const char *inRecordType )
{
	return LookupAttribute( inContext, inRecordType, userName, kDSNAttrAuthenticationAuthority, inConfigFromXML, inLDAPSessionMgr, outAuthCount, outAuthAuthority );
}


sInt32 CLDAPv3Plugin::LookupAttribute (	sLDAPContextData *inContext,
										const char *inRecordType,
										const char *inRecordName,
										const char *inAttribute,
										CLDAPv3Configs *inConfigFromXML,
										CLDAPNode& inLDAPSessionMgr,
										unsigned long *outCount,
										char **outData[] )
{
	sInt32			siResult			= eDSAuthFailed;
	char		   *valueData			= nil;
	char		   *pLDAPRecType		= nil;
    char		   *queryFilter			= nil;
	LDAPMessage	   *result				= nil;
	LDAPMessage	   *entry				= nil;
	char		   *attr				= nil;
	BerElement	   *ber					= nil;
	int				ldapReturnCode 		= 0;
	int				numRecTypes			= 1;
	bool			bResultFound		= false;
    sLDAPConfigData	   *pConfig			= nil;
	char		  **attrs				= nil;
    int				searchTO			= 0;
	bool			bOCANDGroup			= false;
	CFArrayRef		OCSearchList		= nil;
	struct berval **berVal				= nil;
	ber_int_t		scope				= LDAP_SCOPE_SUBTREE;
	sInt32			searchResult		= eDSNoErr;
	char		   *pLDAPAttrType		= nil;
    
	try
	{
		if ( inContext  == nil ) throw( (sInt32)eDSBadContextData );
		if ( inRecordName == nil ) throw( (sInt32)eDSNullDataBuff );
        if ( outData == nil ) throw( (sInt32)eDSNullParameter );
        *outCount = 0;
        *outData = nil;
        
		attrs = MapAttrToLDAPTypeArray( inRecordType, inAttribute, inContext->fConfigTableIndex, inConfigFromXML );
		if ( attrs == nil ) throw( (sInt32)eDSInvalidAttributeType );
		
		if (*attrs == nil) //no values returned so maybe this is a static mapping
		{
			pLDAPAttrType = MapAttrToLDAPType( inRecordType, inAttribute, inContext->fConfigTableIndex, 1, inConfigFromXML, false );
			if (pLDAPAttrType != nil)
			{
				*outData = (char **)calloc( 2, sizeof(char *) );
				(*outData)[0] = pLDAPAttrType;
				siResult = eDSNoErr;
			}
		}
		else //search for auth authority in LDAP server
		{
        
			DBGLOG1( kLogPlugin, "LDAP PlugIn: Attempting to get %s", inAttribute );
			
			//retrieve the config data
			//don't need to retrieve for the case of "generic unknown" so don't check index 0
			if (( inContext->fConfigTableIndex < gLDAPConfigTableLen) && ( inContext->fConfigTableIndex >= 1 ))
			{
				pConfig = (sLDAPConfigData *)gLDAPConfigTable->GetItemData( inContext->fConfigTableIndex );
				if (pConfig != nil)
				{
					searchTO = pConfig->fSearchTimeout;
				}
			}
			
			// we will search over all the rectype mappings until we find the first
			// result for the search criteria in the queryfilter
			numRecTypes = 1;
			bOCANDGroup = false;
			if (OCSearchList != nil)
			{
				CFRelease(OCSearchList);
				OCSearchList = nil;
			}
			pLDAPRecType = MapRecToLDAPType( inRecordType, inContext->fConfigTableIndex, numRecTypes, &bOCANDGroup, &OCSearchList, &scope, inConfigFromXML );
			//only throw this for first time since we need at least one map
			if ( pLDAPRecType == nil ) throw( (sInt32)eDSInvalidRecordType );
			
			while ( (pLDAPRecType != nil) && (!bResultFound) )
			{
				queryFilter = BuildLDAPQueryFilter(	(char *)kDSNAttrRecordName,
												inRecordName,
												eDSExact,
												inContext->fConfigTableIndex,
												false,
												inRecordType,
												pLDAPRecType,
												bOCANDGroup,
												OCSearchList,
                                                inConfigFromXML );
				
				if ( queryFilter == nil ) throw( (sInt32)eDSNullParameter );
	
				searchResult = DSDoRetrieval (	pLDAPRecType,
												attrs,
												inContext,
												scope,
												inLDAPSessionMgr,
												queryFilter,
												ldapReturnCode,
												searchTO,
												result,
												bResultFound,
												inConfigFromXML);
	
				if (queryFilter != nil)
				{
					delete (queryFilter);
					queryFilter = nil;
				}
	
				if (pLDAPRecType != nil)
				{
					delete (pLDAPRecType);
					pLDAPRecType = nil;
				}
				numRecTypes++;
				bOCANDGroup = false;
				if (OCSearchList != nil)
				{
					CFRelease(OCSearchList);
					OCSearchList = nil;
				}
				pLDAPRecType = MapRecToLDAPType( inRecordType, inContext->fConfigTableIndex, numRecTypes, &bOCANDGroup, &OCSearchList, &scope, inConfigFromXML );
			} // while ( (pLDAPRecType != nil) && (!bResultFound) )
	
			if (	(bResultFound) &&
					( ldapReturnCode == LDAP_RES_SEARCH_ENTRY ) )
			{
				LDAP *aHost = inLDAPSessionMgr.LockSession(inContext);
				if (aHost != nil)
				{
					//get the authAuthority attribute here
					entry = ldap_first_entry( aHost, result );
					if ( entry != nil )
					{
						attr = ldap_first_attribute( aHost, entry, &ber );
						if ( attr != nil )
						{
							int idx;
							int numValues = 0;
							
							berVal = ldap_get_values_len( aHost, entry, attr );
							if ( berVal != nil )
							{
								numValues = ldap_count_values_len( berVal );
								if ( numValues > 0 )
								{
									*outCount = numValues;
									*outData = (char **)calloc( numValues+1, sizeof(char *) );
								}
								
								for ( idx = 0; idx < numValues; idx++ )
								{
									valueData = (char *)malloc( berVal[idx]->bv_len + 1 );
									if ( valueData == nil ) throw ( eMemoryError );
									
									strncpy( valueData, berVal[idx]->bv_val, berVal[idx]->bv_len );
									valueData[berVal[idx]->bv_len] = '\0';
									
									// TODO: return the right string
									DBGLOG1( kLogPlugin, "value found %s\n", valueData ); 
									
									(*outData)[idx] = valueData;
								}
								siResult = eDSNoErr;
								
								ldap_value_free_len( berVal );
								berVal = nil;
							}
							ldap_memfree( attr );
							attr = nil;
						}
						if ( ber != nil )
						{
							ber_free( ber, 0 );
						}
						//need to be smart and not call abandon unless the search is continuing
						//ldap_abandon( aHost, ldapMsgId ); // we don't care about the other results, just the first
					}		
				}// if aHost != nil
				inLDAPSessionMgr.UnLockSession(inContext);
			} // if bResultFound and ldapReturnCode okay
			else
			{
				siResult = searchResult;
			}
		} //search for auth authority in LDAP server
	}

	catch ( sInt32 err )
	{
		DBGLOG1( kLogPlugin, "LDAP PlugIn: LookupAttribute error %l", err );
		siResult = err;
	}
	
	if (OCSearchList != nil)
	{
		CFRelease(OCSearchList);
		OCSearchList = nil;
	}
	
	if ( attrs != nil )
	{
		for ( int i = 0; attrs[i] != nil; ++i )
		{
			free( attrs[i] );
		}
		free( attrs );
		attrs = nil;
	}

	if ( queryFilter != nil )
	{
		delete( queryFilter );
		queryFilter = nil;
	}
	
	if ( result != nil )
	{
		ldap_msgfree( result );
		result = nil;
	}
	
	return( siResult );
}


// ---------------------------------------------------------------------------
//	* ParseAuthAuthority
//    retrieve version, tag, and data from authauthority
//    format is version;tag;data
// ---------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::ParseAuthAuthority ( const char	 * inAuthAuthority,
                                            char			** outVersion,
                                            char			** outAuthTag,
                                            char			** outAuthData )
{
	char* authAuthority = NULL;
	char* current = NULL;
	char* tempPtr = NULL;
	sInt32 result = eDSAuthFailed;
	if ( inAuthAuthority == NULL || outVersion == NULL 
		 || outAuthTag == NULL || outAuthData == NULL )
	{
		return eDSAuthFailed;
	}
	authAuthority = strdup(inAuthAuthority);
	if (authAuthority == NULL)
	{
		return eDSAuthFailed;
	}
	current = authAuthority;
	do {
		tempPtr = strsep(&current, ";");
		if (tempPtr == NULL) break;
		*outVersion = strdup(tempPtr);
		
		tempPtr = strsep(&current, ";");
		if (tempPtr == NULL) break;
		*outAuthTag = strdup(tempPtr);
		
		tempPtr = strsep(&current, ";");
		if (tempPtr == NULL) break;
		*outAuthData = strdup(tempPtr);
		
		result = eDSNoErr;
	} while (false);
	
	free(authAuthority);
	authAuthority = NULL;
	if (result != eDSNoErr)
	{
		if (*outVersion != NULL)
		{
			free(*outVersion);
			*outVersion = NULL;
		}
		if (*outAuthTag != NULL)
		{
			free(*outAuthTag);
			*outAuthTag = NULL;
		}
		if (*outAuthData != NULL)
		{
			free(*outAuthData);
			*outAuthData = NULL;
		}
	}
	return result;
}


//------------------------------------------------------------------------------------
//	* DoUnixCryptAuth
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::DoUnixCryptAuth ( sLDAPContextData *inContext, tDataBuffer *inAuthData, CLDAPv3Configs *inConfigFromXML, CLDAPNode& inLDAPSessionMgr, const char *inRecordType )
{
	sInt32			siResult			= eDSAuthFailed;
	char		   *pData				= nil;
	uInt32			offset				= 0;
	uInt32			buffSize			= 0;
	uInt32			buffLen				= 0;
	char		   *userName			= nil;
	sInt32			nameLen				= 0;
	char		   *pwd					= nil;
	sInt32			pwdLen				= 0;
	char		   *cryptPwd			= nil;
	char		   *pLDAPRecType		= nil;
    char		   *queryFilter			= nil;
	LDAPMessage	   *result				= nil;
	LDAPMessage	   *entry				= nil;
	char		   *attr				= nil;
	char		  **vals				= nil;
	BerElement	   *ber					= nil;
	int				ldapReturnCode 		= 0;
	int				numRecTypes			= 1;
	bool			bResultFound		= false;
    sLDAPConfigData	   *pConfig			= nil;
	char		  **attrs				= nil;
    int				searchTO			= 0;
	bool			bOCANDGroup			= false;
	CFArrayRef		OCSearchList		= nil;
	ber_int_t		scope				= LDAP_SCOPE_SUBTREE;
	sInt32			searchResult		= eDSNoErr;
	
	try
	{
		if ( inContext  == nil ) throw( (sInt32)eDSBadContextData );
		if ( inAuthData == nil ) throw( (sInt32)eDSNullDataBuff );

		pData = inAuthData->fBufferData;
		if ( pData == nil ) throw( (sInt32)eDSNullDataBuff );
		
		buffSize	= inAuthData->fBufferSize;
		buffLen		= inAuthData->fBufferLength;
		if (buffLen > buffSize) throw( (sInt32)eDSInvalidBuffFormat );
	
		if ( offset + (2 * sizeof( unsigned long ) + 1) > buffLen ) throw( (sInt32)eDSInvalidBuffFormat );
		// need username length, password length, and username must be at least 1 character

		// Get the length of the user name
		::memcpy( &nameLen, pData, sizeof( unsigned long ) );
		if (nameLen == 0) throw( (sInt32)eDSInvalidBuffFormat );
		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );

		userName = (char *)::calloc(1, nameLen + 1);
		if ( userName == nil ) throw( (sInt32)eMemoryError );

		if ( offset + nameLen > buffLen ) throw( (sInt32)eDSInvalidBuffFormat );
		// Copy the user name
		::memcpy( userName, pData, nameLen );
		
		pData += nameLen;
		offset += nameLen;

		if ( offset + sizeof( unsigned long ) > buffLen ) throw( (sInt32)eDSInvalidBuffFormat );
		// Get the length of the user password
		::memcpy( &pwdLen, pData, sizeof( unsigned long ) );
		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );

		pwd = (char *)::calloc(1, pwdLen + 1);
		if ( pwd == nil ) throw( (sInt32)eMemoryError );

		if ( offset + pwdLen > buffLen ) throw( (sInt32)eDSInvalidBuffFormat );
		// Copy the user password
		::memcpy( pwd, pData, pwdLen );

		attrs = MapAttrToLDAPTypeArray( inRecordType, kDS1AttrPassword, inContext->fConfigTableIndex, inConfigFromXML );
		
		//No Mappings, so we need to return an error that only a clear text password would work instead of no mapping/failure
		if( attrs == nil ) throw ( (sInt32)eDSAuthFailedClearTextOnly );

		DBGLOG( kLogPlugin, "LDAP PlugIn: Attempting Crypt Authentication" );

		//retrieve the config data
		//don't need to retrieve for the case of "generic unknown" so don't check index 0
		if (( inContext->fConfigTableIndex < gLDAPConfigTableLen) && ( inContext->fConfigTableIndex >= 1 ))
		{
			pConfig = (sLDAPConfigData *)gLDAPConfigTable->GetItemData( inContext->fConfigTableIndex );
			if (pConfig != nil)
			{
				searchTO	= pConfig->fSearchTimeout;
			}
		}
		
        // we will search over all the rectype mappings until we find the first
        // result for the search criteria in the queryfilter
        numRecTypes = 1;
		bOCANDGroup = false;
		if (OCSearchList != nil)
		{
			CFRelease(OCSearchList);
			OCSearchList = nil;
		}
		pLDAPRecType = MapRecToLDAPType( inRecordType, inContext->fConfigTableIndex, numRecTypes, &bOCANDGroup, &OCSearchList, &scope, inConfigFromXML );
		//only throw this for first time since we need at least one map
		if ( pLDAPRecType == nil ) throw( (sInt32)eDSInvalidRecordType );
		
		while ( (pLDAPRecType != nil) && (!bResultFound) )
		{
			queryFilter = BuildLDAPQueryFilter(	(char *)kDSNAttrRecordName,
												userName,
												eDSExact,
												inContext->fConfigTableIndex,
												false,
												inRecordType,
												pLDAPRecType,
												bOCANDGroup,
												OCSearchList,
                                                inConfigFromXML );
			if ( queryFilter == nil ) throw( (sInt32)eDSNullParameter );

			searchResult = DSDoRetrieval (	pLDAPRecType,
											attrs,
											inContext,
											scope,
											inLDAPSessionMgr,
											queryFilter,
											ldapReturnCode,
											searchTO,
											result,
											bResultFound,
											inConfigFromXML);

			if (queryFilter != nil)
			{
				delete (queryFilter);
				queryFilter = nil;
			}

			if (pLDAPRecType != nil)
			{
				delete (pLDAPRecType);
				pLDAPRecType = nil;
			}
			numRecTypes++;
			bOCANDGroup = false;
			if (OCSearchList != nil)
			{
				CFRelease(OCSearchList);
				OCSearchList = nil;
			}
			pLDAPRecType = MapRecToLDAPType( inRecordType, inContext->fConfigTableIndex, numRecTypes, &bOCANDGroup, &OCSearchList, &scope, inConfigFromXML );
		} // while ( (pLDAPRecType != nil) && (!bResultFound) )

		if (	(bResultFound) &&
				( ldapReturnCode == LDAP_RES_SEARCH_ENTRY ) )
		{
			LDAP *aHost = gLDAPv3Plugin->fLDAPSessionMgr.LockSession(inContext);
			if (aHost != nil)
			{
				//get the passwd attribute here
				//we are only going to look at the first attribute, first value
				entry = ldap_first_entry( aHost, result );
				if ( entry != nil )
				{
					attr = ldap_first_attribute( aHost, entry, &ber );
					if ( attr != nil )
					{
						vals = ldap_get_values( aHost, entry, attr );
						if ( vals != nil )
						{
							if ( vals[0] != nil )
							{
								cryptPwd = vals[0];
							}
							else
							{
								cryptPwd = (char *)""; //don't free this
							}
							if (::strncasecmp(cryptPwd,"{SMD5}",6) == 0)
							{
								cryptPwd = cryptPwd + 6;
								
								if ( *cryptPwd != '\0' )
								{
									unsigned outlen;
									int result;
									unsigned char hashResult[MD5_DIGEST_LENGTH];
									MD5_CTX ctx;
									char targetStr[128];
									
									result = sasl_decode64( cryptPwd, strlen(cryptPwd), targetStr, sizeof(targetStr), &outlen );
									if ( result == SASL_OK )
									{
										MD5_Init( &ctx );
										MD5_Update( &ctx, pwd, strlen( pwd ) );
										MD5_Update( &ctx, targetStr + MD5_DIGEST_LENGTH, outlen - MD5_DIGEST_LENGTH );
										MD5_Final( hashResult, &ctx );
										
										if ( memcmp( hashResult, targetStr, MD5_DIGEST_LENGTH ) == 0 )
										{
											siResult = eDSNoErr;
										}
									}
								}
							}
							else
							{
								//case insensitive compare with "crypt" string
								if (::strncasecmp(cryptPwd,"{crypt}",7) == 0)
								{
									// special case for OpenLDAP's crypt password attribute
									// advance past {crypt} to the actual crypt password we want to compare against
									cryptPwd = cryptPwd + 7;
								}
								//account for the case where cryptPwd == "" such that we will auth if pwdLen is 0
								if (::strcmp(cryptPwd,"") != 0)
								{
									char salt[ 9 ];
									char hashPwd[ 32 ];
									salt[ 0 ] = cryptPwd[0];
									salt[ 1 ] = cryptPwd[1];
									salt[ 2 ] = '\0';
						
									::memset( hashPwd, 0, 32 );
									::strcpy( hashPwd, ::crypt( pwd, salt ) );
						
									siResult = eDSAuthFailed;
									if ( ::strcmp( hashPwd, cryptPwd ) == 0 )
									{
										siResult = eDSNoErr;
									}
									
								}
								else // cryptPwd is == ""
								{
									if ( ::strcmp(pwd,"") == 0 )
									{
										siResult = eDSNoErr;
									}
								}
							}
							ldap_value_free( vals );
							vals = nil;
						}
						ldap_memfree( attr );
						attr = nil;
					}
					if ( ber != nil )
					{
						ber_free( ber, 0 );
					}
					//need to be smart and not call abandon unless the search is continuing
					//ldap_abandon( aHost, ldapMsgId ); // we don't care about the other results, just the first
				}
			}//if aHost != nil
			gLDAPv3Plugin->fLDAPSessionMgr.UnLockSession(inContext);
		} // if bResultFound and ldapReturnCode okay

		//no check for LDAP_TIMEOUT on ldapReturnCode since we will return nil
		if ( result != nil )
		{
			ldap_msgfree( result );
			result = nil;
		}
	}

	catch ( sInt32 err )
	{
		DBGLOG1( kLogPlugin, "LDAP PlugIn: Crypt authentication error %l", err );
		siResult = err;
	}
	
	if (OCSearchList != nil)
	{
		CFRelease(OCSearchList);
		OCSearchList = nil;
	}
	
	if ( attrs != nil )
	{
		for ( int i = 0; attrs[i] != nil; ++i )
		{
			free( attrs[i] );
		}
		free( attrs );
		attrs = nil;
	}

	if ( userName != nil )
	{
		delete( userName );
		userName = nil;
	}

	if ( pwd != nil )
	{
		delete( pwd );
		pwd = nil;
	}

	if ( queryFilter != nil )
	{
		delete( queryFilter );
		queryFilter = nil;
	}
	
	return( siResult );

} // DoUnixCryptAuth


//------------------------------------------------------------------------------------
//	* DoClearTextAuth
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::DoClearTextAuth ( sLDAPContextData *inContext, tDataBuffer *inAuthData, bool authCheckOnly, CLDAPv3Configs *inConfigFromXML, CLDAPNode& inLDAPSessionMgr, const char *inRecordType )
{
	sInt32				siResult			= eDSAuthFailed;
	char			   *pData				= nil;
	char			   *userName			= nil;
	char			   *accountId			= nil;
	sInt32				nameLen				= 0;
	char			   *pwd					= nil;
	sInt32				pwdLen				= 0;
	LDAP			   *aLDAPHost			= nil;
	bool				bNewMutex			= false;

	try
	{
	//check the authCheckOnly if true only test name and password
	//ie. need to retain current credentials
		if ( inContext  == nil ) throw( (sInt32)eDSBadContextData );
		if ( inAuthData == nil ) throw( (sInt32)eDSNullDataBuff );

		pData = inAuthData->fBufferData;
		if ( pData == nil ) throw( (sInt32)eDSNullDataBuff );
		
		// Get the length of the user name
		::memcpy( &nameLen, pData, sizeof( long ) );
		//accept the case of a NO given name and NO password
		//LDAP servers use this to reset the credentials
		//if (nameLen == 0) throw( (sInt32)eDSAuthInBuffFormatError );

		if (nameLen > 0)
		{
			userName = (char *) calloc(1, nameLen + 1);
			if ( userName == nil ) throw( (sInt32) eMemoryError );

			// Copy the user name
			pData += sizeof( long );
			::memcpy( userName, pData, nameLen );
			pData += nameLen;
		}

		// Get the length of the user password
		::memcpy( &pwdLen, pData, sizeof( long ) );
		//accept the case of a given name and NO password
		//LDAP servers use this as tracking info when no password is required
		//if (pwdLen == 0) throw( (sInt32)eDSAuthInBuffFormatError );

		if (pwdLen > 0)
		{
			pwd = (char *) calloc(1, pwdLen + 1);
			if ( pwd == nil ) throw( (sInt32) eMemoryError );

			// Copy the user password
			pData += sizeof( long );
			::memcpy( pwd, pData, pwdLen );
			pData += pwdLen;
		}

		DBGLOG( kLogPlugin, "LDAP PlugIn: Attempting Cleartext Authentication" );

		//get the correct account id
		//we assume that the DN is always used for this
		if (userName)
		{
			//here again we choose the first match
			accountId = GetDNForRecordName ( userName, inContext, inConfigFromXML, inLDAPSessionMgr, inRecordType );
		}
                
		//if username did not garner an accountId then fail authentication
		if (accountId == nil)
		{
			throw( (sInt32)eDSAuthFailed );
		}
		
		if ((pwd != NULL) && (pwd[0] != '\0') && (nameLen != 0))
		{
			if (!authCheckOnly)
			{
				LDAP *aHost = gLDAPv3Plugin->fLDAPSessionMgr.LockSession(inContext);
				if (aHost != nil)
				{
					// Here is the bind to the LDAP server
					siResult = inLDAPSessionMgr.AuthOpen(	inContext->fName,
															aHost,
															accountId,
															pwd,
															kDSStdAuthClearText,
															&aLDAPHost,
															&(inContext->fConfigTableIndex),
															true);
					if (siResult == eDSNoErr)
					{
						if (inContext->fLDAPSessionMutex == nil)
						{
							inContext->fLDAPSessionMutex = new DSMutexSemaphore();
							bNewMutex = true;
						}
						inContext->fLDAPSessionMutex->Wait();
						inContext->fHost = aLDAPHost;
						//provision here for future different auth but now auth credential is a password only
						if (inContext->fAuthType == nil)
						{
							inContext->fAuthType = strdup(kDSStdAuthClearText);
						}
						inContext->authCallActive = true;
						if (inContext->fUserName != nil)
						{
							free(inContext->fUserName);
							inContext->fUserName = nil;
						}
						inContext->fUserName = strdup(accountId);
						if (inContext->fAuthCredential != nil)
						{
							free(inContext->fAuthCredential);
							inContext->fAuthCredential = nil;
						}
						inContext->fAuthCredential = strdup(pwd);
						inContext->fLDAPSessionMutex->Signal();
					}
					else
					{
						siResult = eDSAuthFailed;
					}
				}//if aHost != nil
				gLDAPv3Plugin->fLDAPSessionMgr.UnLockSession(inContext, false, bNewMutex);
			}
			else //no session ie. authCheckOnly
			{
				siResult = inLDAPSessionMgr.SimpleAuth(	inContext->fName,
														accountId,
														pwd );
			}
			
		}

	}

	catch ( sInt32 err )
	{
		DBGLOG1( kLogPlugin, "LDAP PlugIn: Cleartext authentication error %l", err );
		siResult = err;
	}

	if ( accountId != nil )
	{
		delete( accountId );
		accountId = nil;
	}

	if ( userName != nil )
	{
		delete( userName );
		userName = nil;
	}

	if ( pwd != nil )
	{
		delete( pwd );
		pwd = nil;
	}

	return( siResult );

} // DoClearTextAuth

//------------------------------------------------------------------------------------
//	* GetDNForRecordName
//------------------------------------------------------------------------------------

char* CLDAPv3Plugin::GetDNForRecordName ( char* inRecName, sLDAPContextData *inContext, CLDAPv3Configs *inConfigFromXML, CLDAPNode& inLDAPSessionMgr, const char *inRecordType )
{
	char			   *ldapDN			= nil;	
	char			   *pLDAPRecType	= nil;
    char			   *queryFilter		= nil;
	LDAPMessage		   *result			= nil;
	int					ldapReturnCode	= 0;
	int					numRecTypes		= 1;
	bool				bResultFound	= false;
    sLDAPConfigData	   *pConfig			= nil;
    int					searchTO		= 0;
	bool				bOCANDGroup		= false;
	CFArrayRef			OCSearchList	= nil;
	ber_int_t			scope			= LDAP_SCOPE_SUBTREE;
	sInt32				searchResult	= eDSNoErr;


	try
	{
		if ( inRecName  == nil ) throw( (sInt32)eDSNullRecName );
		if ( inContext  == nil ) throw( (sInt32)eDSBadContextData );

		//search for the specific LDAP record now
		
		//retrieve the config data
		//don't need to retrieve for the case of "generic unknown" so don't check index 0
		if (( inContext->fConfigTableIndex < gLDAPConfigTableLen) && ( inContext->fConfigTableIndex >= 1 ))
		{
			pConfig = (sLDAPConfigData *)gLDAPConfigTable->GetItemData( inContext->fConfigTableIndex );
			if (pConfig != nil)
			{
				searchTO	= pConfig->fSearchTimeout;
			}
		}
		
        // we will search over all the rectype mappings until we find the first
        // result for the search criteria in the queryfilter
        numRecTypes = 1;
		bOCANDGroup = false;
		if (OCSearchList != nil)
		{
			CFRelease(OCSearchList);
			OCSearchList = nil;
		}
		pLDAPRecType = MapRecToLDAPType( inRecordType, inContext->fConfigTableIndex, numRecTypes, &bOCANDGroup, &OCSearchList, &scope, inConfigFromXML );
		//only throw this for first time since we need at least one map
		if ( pLDAPRecType == nil ) throw( (sInt32)eDSInvalidRecordType );
		
		while ( (pLDAPRecType != nil) && (!bResultFound) )
		{

			//build the record query string
			queryFilter = BuildLDAPQueryFilter(	(char *)kDSNAttrRecordName,
												inRecName,
												eDSExact,
												inContext->fConfigTableIndex,
												false,
												inRecordType,
												pLDAPRecType,
												bOCANDGroup,
												OCSearchList,
                                                inConfigFromXML );
			if ( queryFilter == nil ) throw( (sInt32)eDSNullParameter );

			searchResult = DSDoRetrieval (	pLDAPRecType,
											NULL,
											inContext,
											scope,
											inLDAPSessionMgr,
											queryFilter,
											ldapReturnCode,
											searchTO,
											result,
											bResultFound,
											inConfigFromXML);

			if (queryFilter != nil)
			{
				delete (queryFilter);
				queryFilter = nil;
			}

			if (pLDAPRecType != nil)
			{
				delete (pLDAPRecType);
				pLDAPRecType = nil;
			}
			numRecTypes++;
			bOCANDGroup = false;
			if (OCSearchList != nil)
			{
				CFRelease(OCSearchList);
				OCSearchList = nil;
			}
			pLDAPRecType = MapRecToLDAPType( inRecordType, inContext->fConfigTableIndex, numRecTypes, &bOCANDGroup, &OCSearchList, &scope, inConfigFromXML );
		} // while ( (pLDAPRecType != nil) && (!bResultFound) )

		if (	(bResultFound) &&
				( ldapReturnCode == LDAP_RES_SEARCH_ENTRY ) )
		{
			LDAP *aHost = gLDAPv3Plugin->fLDAPSessionMgr.LockSession(inContext);
			if (aHost != nil)
			{
				//get the ldapDN here
				ldapDN = ldap_get_dn(aHost, result);
			}
			gLDAPv3Plugin->fLDAPSessionMgr.UnLockSession(inContext);
		
		} // if bResultFound and ldapReturnCode okay
		
		//no check for LDAP_TIMEOUT on ldapReturnCode since we will return nil
		if ( result != nil )
		{
			ldap_msgfree( result );
			result = nil;
		}
	}

	catch ( sInt32 err )
	{
		ldapDN = nil;
	}

	if (OCSearchList != nil)
	{
		CFRelease(OCSearchList);
		OCSearchList = nil;
	}
	
	if ( pLDAPRecType != nil )
	{
		delete( pLDAPRecType );
		pLDAPRecType = nil;
	}

	if ( queryFilter != nil )
	{
		delete( queryFilter );
		queryFilter = nil;
	}

	return( ldapDN );

} // GetDNForRecordName

//------------------------------------------------------------------------------------
//      * DoPlugInCustomCall
//------------------------------------------------------------------------------------ 

sInt32 CLDAPv3Plugin::DoPlugInCustomCall ( sDoPlugInCustomCall *inData )
{
	sInt32			siResult	= eDSNoErr;
	unsigned long	aRequest	= 0;
	sLDAPContextData	   *pContext		= nil;
	sInt32				xmlDataLength	= 0;
	CFDataRef   		xmlData			= nil;
	unsigned long		bufLen			= 0;
	AuthorizationRef	authRef			= 0;
	AuthorizationItemSet   *resultRightSet = NULL;
	tDataListPtr		dataList		= NULL;
	char*				userName		= NULL;
	char*				password		= NULL;
	char*				xmlString		= NULL;
	AuthorizationExternalForm blankExtForm;
	bool				verifyAuthRef	= true;

//seems that the client needs to have a tDirNodeReference 
//to make the custom call even though it will likely be non-dirnode specific related

	try
	{
		if ( inData == nil ) throw( (sInt32)eDSNullParameter );
		if ( inData->fInRequestData == nil ) throw( (sInt32)eDSNullDataBuff );
		if ( inData->fInRequestData->fBufferData == nil ) throw( (sInt32)eDSEmptyBuffer );

		pContext = (sLDAPContextData *)gLDAPContextTable->GetItemData( inData->fInNodeRef );
		if ( pContext  == nil ) throw( (sInt32)eDSBadContextData );
		
		if ( strcmp(pContext->fName,"LDAPv3 Configure") == 0 )
		{
			aRequest = inData->fInRequestCode;
			bufLen = inData->fInRequestData->fBufferLength;
			if ( aRequest != 55 )
			{
				if ( bufLen < sizeof( AuthorizationExternalForm ) ) throw( (sInt32)eDSInvalidBuffFormat );

				if ( pContext->fEffectiveUID == 0 ) {
					bzero(&blankExtForm,sizeof(AuthorizationExternalForm));
					if (memcmp(inData->fInRequestData->fBufferData,&blankExtForm,
							   sizeof(AuthorizationExternalForm)) == 0) {
						verifyAuthRef = false;
					}
				}
				if (verifyAuthRef) {
					siResult = AuthorizationCreateFromExternalForm((AuthorizationExternalForm *)inData->fInRequestData->fBufferData,
						&authRef);
					if (siResult != errAuthorizationSuccess)
					{
						throw( (sInt32)eDSPermissionError );
					}
		
					AuthorizationItem rights[] = { {"system.services.directory.configure", 0, 0, 0} };
					AuthorizationItemSet rightSet = { sizeof(rights)/ sizeof(*rights), rights };
				
					siResult = AuthorizationCopyRights(authRef, &rightSet, NULL,
						kAuthorizationFlagExtendRights, &resultRightSet);
					if (resultRightSet != NULL)
					{
						AuthorizationFreeItemSet(resultRightSet);
						resultRightSet = NULL;
					}
					if (siResult != errAuthorizationSuccess)
					{
						throw( (sInt32)eDSPermissionError );
					}
				}
			}
			switch( aRequest )
			{
				case 55:
					// parse input buffer
					dataList = dsAuthBufferGetDataListAllocPriv(inData->fInRequestData);
					if ( dataList == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					if ( dsDataListGetNodeCountPriv(dataList) != 3 ) throw( (sInt32)eDSInvalidBuffFormat );

					// this allocates a copy of the string
					userName = dsDataListGetNodeStringPriv(dataList, 1);
					if ( userName == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					if ( strlen(userName) < 1 ) throw( (sInt32)eDSInvalidBuffFormat );

					password = dsDataListGetNodeStringPriv(dataList, 2);
					if ( password == nil ) throw( (sInt32)eDSInvalidBuffFormat );

					xmlString = dsDataListGetNodeStringPriv(dataList, 3);
					if ( xmlString == nil ) throw( (sInt32)eDSInvalidBuffFormat );
					xmlData = CFDataCreate(NULL,(UInt8*)xmlString,strlen(xmlString));
					
//must provide full distinguished name until we get Directory Access to actually open the correct node other than the LDAPv3 config node
//					siResult = eDSInvalidContext; //if for some reason we can't get the session manager
//					if (gLDAPv3Plugin != nil)
//					{
//						char *ldapDN = GetDNForRecordName( userName, pContext, pConfigFromXML, gLDAPv3Plugin->fLDAPSessionMgr, (const char *)kDSStdRecordTypeUsers );
//						if (ldapDN != nil)
//						{
					//WriteServerMappings will accept the proper XML config that needs to be written into the LDAP server
					siResult = pConfigFromXML->WriteServerMappings( userName, password, xmlData );
//							siResult = pConfigFromXML->WriteServerMappings( ldapDN, password, xmlData );
//							free(ldapDN);
//							ldapDN = nil;
//						}
//						else
//						{
//							siResult = eDSInvalidName;
//						}
//					}
					if (userName != nil)
					{
						free(userName);
						userName = nil;
					}
					if (password != nil)
					{
						free(password);
						password = nil;
					}
					if (xmlString != nil)
					{
						free(xmlString);
						xmlString = nil;
					}
					break;
					/*
					//ReadServerMappings will accept the partial XML config data that comes out of the local config file so that
					//it can return the proper XML config data that defines all the mappings
					CFDataRef pConfigFromXML->ReadServerMappings ( LDAP *serverHost = pContext->fHost, CFDataRef inMappings = xmlData );
					*/
					 
				case 66:
					// get length of XML file
						
					if ( inData->fOutRequestResponse == nil ) throw( (sInt32)eDSNullDataBuff );
					if ( inData->fOutRequestResponse->fBufferData == nil ) throw( (sInt32)eDSEmptyBuffer );
					if ( inData->fOutRequestResponse->fBufferSize < sizeof( CFIndex ) ) throw( (sInt32)eDSInvalidBuffFormat );
					if (pConfigFromXML)
					{
						pConfigFromXML->XMLConfigLock();
						// need four bytes for size
						xmlData = pConfigFromXML->CopyXMLConfig();
						if (xmlData != 0)
						{
							*(CFIndex*)(inData->fOutRequestResponse->fBufferData) = CFDataGetLength(xmlData);
							inData->fOutRequestResponse->fBufferLength = sizeof( CFIndex );
							CFRelease( xmlData );
							xmlData = 0;
						}
						pConfigFromXML->XMLConfigUnlock();
					}
					break;
					
				case 77:
					// read xml config
					CFRange	aRange;
						
					if ( inData->fOutRequestResponse == nil ) throw( (sInt32)eDSNullDataBuff );
					if ( inData->fOutRequestResponse->fBufferData == nil ) throw( (sInt32)eDSEmptyBuffer );
					if (pConfigFromXML)
					{
						pConfigFromXML->XMLConfigLock();
						xmlData = pConfigFromXML->CopyXMLConfig();
						if (xmlData != 0)
						{
							aRange.location = 0;
							aRange.length = CFDataGetLength(xmlData);
							if ( inData->fOutRequestResponse->fBufferSize < (unsigned int)aRange.length ) throw( (sInt32)eDSBufferTooSmall );
							CFDataGetBytes( xmlData, aRange, (UInt8*)(inData->fOutRequestResponse->fBufferData) );
							inData->fOutRequestResponse->fBufferLength = aRange.length;
							CFRelease(xmlData);
							xmlData = 0;
						}
						pConfigFromXML->XMLConfigUnlock();
					}
					break;
					
				case 88:
				{
					CFPropertyListRef		configPropertyList = NULL;
					CFStringRef				errorString = NULL;
					
					//here we accept an XML blob to replace the current config file
					//need to make xmlData large enough to receive the data
					xmlDataLength = (sInt32) bufLen - sizeof( AuthorizationExternalForm );
					if ( xmlDataLength <= 0 ) throw( (sInt32)eDSInvalidBuffFormat );
					xmlData = CFDataCreate(NULL,(UInt8 *)(inData->fInRequestData->fBufferData + sizeof( AuthorizationExternalForm )),
						xmlDataLength);
					
					if (xmlData != nil)
					{
						// extract the config dictionary from the XML data.
						configPropertyList = CFPropertyListCreateFromXMLData( kCFAllocatorDefault,
												xmlData,
												kCFPropertyListMutableContainers, 	// in case we have to strip out DHCP data
											&errorString);
						if (configPropertyList != nil )
						{
							//make the propertylist a dict
							if ( CFDictionaryGetTypeID() == CFGetTypeID( configPropertyList ) && CFDictionaryContainsValue( (CFDictionaryRef)configPropertyList, CFSTR(kXMLDHCPConfigArrayKey) ) )
							{
								// check to see if this has the DHCP config data.  If so strip it.
								CFDictionaryRemoveValue( (CFMutableDictionaryRef)configPropertyList, CFSTR(kXMLDHCPConfigArrayKey) );
								CFRelease( xmlData );
								xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, configPropertyList);
							}
							CFRelease(configPropertyList);
							configPropertyList = NULL;
						}
					}
					
					if (pConfigFromXML)
					{
						// refresh registered nodes
						pConfigFromXML->XMLConfigLock();
						siResult = pConfigFromXML->SetXMLConfig(xmlData);
						siResult = pConfigFromXML->WriteXMLConfig();
						pConfigFromXML->XMLConfigUnlock();
						Initialize();
					}
					CFRelease(xmlData);
					break;
				}
					
				case 99:
					Initialize();
					break;

				case 111:
					//here we accept an XML blob to add a Server Mappings LDAP node to the search policy
					//need to make xmlData large enough to accomodate the data
					xmlDataLength = (sInt32) bufLen - sizeof(AuthorizationExternalForm);
					if ( xmlDataLength <= 0 ) throw( (sInt32)eDSInvalidBuffFormat );
					xmlData = CFDataCreate(NULL, (UInt8 *)(inData->fInRequestData->fBufferData + sizeof(AuthorizationExternalForm)), xmlDataLength);
					if (pConfigFromXML)
					{
						// add to registered nodes as a "forced" DHCP type node
						pConfigFromXML->XMLConfigLock();
						siResult = pConfigFromXML->AddToConfig(xmlData);
						if (siResult == eDSNoErr)
						{
							siResult = pConfigFromXML->WriteXMLConfig();
						}
						pConfigFromXML->XMLConfigUnlock();
						if (siResult == eDSNoErr)
						{
							Initialize();
						}
					}
					CFRelease(xmlData);
					break;

				default:
					break;
			}
		}

	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	if (authRef != 0)
	{
		AuthorizationFree(authRef, 0);
		authRef = 0;
	}

	return( siResult );

} // DoPlugInCustomCall


// ---------------------------------------------------------------------------
//	* ContextDeallocProc
// ---------------------------------------------------------------------------

void CLDAPv3Plugin::ContextDeallocProc ( void* inContextData )
{
	sLDAPContextData *pContext = (sLDAPContextData *) inContextData;

	if ( pContext != nil )
	{
		CleanContextData( pContext );
		
		free( pContext );
		pContext = nil;
	}
} // ContextDeallocProc

// ---------------------------------------------------------------------------
//	* ContinueDeallocProc
// ---------------------------------------------------------------------------

void CLDAPv3Plugin::ContinueDeallocProc ( void* inContinueData )
{
	sLDAPContinueData *pLDAPContinue = (sLDAPContinueData *)inContinueData;

	if ( pLDAPContinue != nil )
	{
        // remember ldap_msgfree( pLDAPContinue->pResult ) will remove the LDAPMessage
        if (pLDAPContinue->pResult != nil)
        {
        	ldap_msgfree( pLDAPContinue->pResult );
	        pLDAPContinue->pResult = nil;
        }

		free( pLDAPContinue );
		pLDAPContinue = nil;
	}
} // ContinueDeallocProc


// ---------------------------------------------------------------------------
//	* RebindLDAPSession
// ---------------------------------------------------------------------------

sInt32 CLDAPv3Plugin:: RebindLDAPSession ( sLDAPContextData *inContext, CLDAPNode& inLDAPSessionMgr, CLDAPv3Configs *inConfigFromXML )
{

    sInt32			siResult	= eDSNoErr;
	LDAP		   *aLDAPHost	= nil;

	if (inContext != nil)
	{
		if (inContext->authCallActive)
		{
			siResult = inLDAPSessionMgr.RebindAuthSession(	inContext->fName,
															inContext->fHost,
															inContext->fUserName,
															inContext->fAuthCredential,
															inContext->fAuthType,
															inContext->fConfigTableIndex,
															&aLDAPHost );
		}
		else
		{
			siResult = inLDAPSessionMgr.RebindSession(		inContext->fName,
															inContext->fHost, //what does this mean?
															inConfigFromXML,
															&aLDAPHost );
		}
		//always reset the session even if nil
		inContext->fHost = aLDAPHost;
	}
	else
	{
		siResult = eDSNullParameter;
	}

	return (siResult);
	
}// RebindLDAPSession

//------------------------------------------------------------------------------------
//	* GetRecRefLDAPMessage
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::GetRecRefLDAPMessage ( sLDAPContextData *inRecContext, LDAPMessage **outResultMsg )
{
	sInt32				siResult		= eDSNoErr;
    sLDAPConfigData	   *pConfig			= nil;
    int					searchTO   		= 0;
	int					numRecTypes		= 1;
	char			   *pLDAPRecType	= nil;
	bool				bResultFound	= false;
    char			   *queryFilter		= nil;
	LDAPMessage		   *result			= nil;
	int					ldapReturnCode	= 0;
	bool				bOCANDGroup		= false;
	CFArrayRef			OCSearchList	= nil;
	ber_int_t			scope			= LDAP_SCOPE_SUBTREE;
	sInt32				searchResult	= eDSNoErr;

	try
	{
	
		//TODO would actually like to use the fOpenRecordDN instead here as it might reduce the effort in the search?
		//ie. we will likely already have it
		if ( inRecContext->fOpenRecordName  == nil ) throw( (sInt32)eDSNullRecName );
		if ( inRecContext->fOpenRecordType  == nil ) throw( (sInt32)eDSNullRecType );

		//search for the specific LDAP record now
		
		//retrieve the config data
		//don't need to retrieve for the case of "generic unknown" so don't check index 0
		if (( inRecContext->fConfigTableIndex < gLDAPConfigTableLen) && ( inRecContext->fConfigTableIndex >= 1 ))
		{
			pConfig = (sLDAPConfigData *)gLDAPConfigTable->GetItemData( inRecContext->fConfigTableIndex );
			if (pConfig != nil)
			{
				searchTO	= pConfig->fSearchTimeout;
			}
		}
		
        // we will search over all the rectype mappings until we find the first
        // result for the search criteria in the queryfilter
        numRecTypes = 1;
		bOCANDGroup = false;
		if (OCSearchList != nil)
		{
			CFRelease(OCSearchList);
			OCSearchList = nil;
		}
		pLDAPRecType = MapRecToLDAPType( (const char *)(inRecContext->fOpenRecordType), inRecContext->fConfigTableIndex, numRecTypes, &bOCANDGroup, &OCSearchList, &scope, pConfigFromXML );
		//only throw this for first time since we need at least one map
		if ( pLDAPRecType == nil ) throw( (sInt32)eDSInvalidRecordType );
		
		while ( (pLDAPRecType != nil) && (!bResultFound) )
		{

			//build the record query string
			//removed the use well known map only condition ie. true to false
			queryFilter = BuildLDAPQueryFilter(	(char *)kDSNAttrRecordName, //TODO can we use dn here ie native type
												inRecContext->fOpenRecordName,
												eDSExact,
												inRecContext->fConfigTableIndex,
												false,
												(const char *)(inRecContext->fOpenRecordType),
												pLDAPRecType,
												bOCANDGroup,
												OCSearchList,
                                                pConfigFromXML );
			if ( queryFilter == nil ) throw( (sInt32)eDSNullParameter );

			searchResult = DSDoRetrieval (	pLDAPRecType,
											NULL,
											inRecContext,
											scope,
											fLDAPSessionMgr,
											queryFilter,
											ldapReturnCode,
											searchTO,
											result,
											bResultFound,
											pConfigFromXML);

			if ( queryFilter != nil )
			{
				delete( queryFilter );
				queryFilter = nil;
			}

			if (pLDAPRecType != nil)
			{
				delete (pLDAPRecType);
				pLDAPRecType = nil;
			}
			numRecTypes++;
			bOCANDGroup = false;
			if (OCSearchList != nil)
			{
				CFRelease(OCSearchList);
				OCSearchList = nil;
			}
			pLDAPRecType = MapRecToLDAPType( (const char *)(inRecContext->fOpenRecordType), inRecContext->fConfigTableIndex, numRecTypes, &bOCANDGroup, &OCSearchList, &scope, pConfigFromXML );
		} // while ( (pLDAPRecType != nil) && (!bResultFound) )

		if (	(bResultFound) &&
				( ldapReturnCode == LDAP_RES_SEARCH_ENTRY ) )
		{
			siResult = eDSNoErr;
		} // if bResultFound and ldapReturnCode okay
		else
		{
	     	siResult = searchResult;
			if ( result != nil )
			{
				ldap_msgfree( result );
				result = nil;
			}
		}
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	if (OCSearchList != nil)
	{
		CFRelease(OCSearchList);
		OCSearchList = nil;
	}
	
	if ( pLDAPRecType != nil )
	{
		delete( pLDAPRecType );
		pLDAPRecType = nil;
	}

	if ( queryFilter != nil )
	{
		delete( queryFilter );
		queryFilter = nil;
	}
	
	if (result != nil)
	{
		*outResultMsg = result;
	}

	return( siResult );

} // GetRecRefLDAPMessage


// ---------------------------------------------------------------------------
//	* GetUserNameFromAuthBuffer
//    retrieve the username from a standard auth buffer
//    buffer format should be 4 byte length followed by username, then optional
//    additional data after. Buffer length must be at least 5 (length + 1 character name)
// ---------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::GetUserNameFromAuthBuffer ( tDataBufferPtr inAuthData, unsigned long inUserNameIndex, char  **outUserName )
{
	tDataListPtr dataList = dsAuthBufferGetDataListAllocPriv(inAuthData);
	if (dataList != NULL)
	{
		*outUserName = dsDataListGetNodeStringPriv(dataList, inUserNameIndex);
		// this allocates a copy of the string
		
		dsDataListDeallocatePriv(dataList);
		free(dataList);
		dataList = NULL;
		return eDSNoErr;
	}
	return eDSInvalidBuffFormat;
}


// ---------------------------------------------------------------------------
//	* GetLDAPv3AuthAuthorityHandler
// ---------------------------------------------------------------------------

LDAPv3AuthAuthorityHandlerProc CLDAPv3Plugin::GetLDAPv3AuthAuthorityHandler ( const char* inTag )
{
	if (inTag == NULL)
	{
		return NULL;
	}
	for (unsigned int i = 0; i < kLDAPv3AuthAuthorityHandlerProcs; ++i)
	{
		if (strcasecmp(inTag,sLDAPv3AuthAuthorityHandlerProcs[i].fTag) == 0)
		{
			// found it
			return sLDAPv3AuthAuthorityHandlerProcs[i].fHandler;
		}
	}
	return NULL;
}


// --------------------------------------------------------------------------------
//	* PeriodicTask ()
// --------------------------------------------------------------------------------

sInt32 CLDAPv3Plugin::PeriodicTask ( void )
{
	fLDAPSessionMgr.CheckIdles();
	
	return( eDSNoErr );
} // PeriodicTask

//------------------------------------------------------------------------------------
//	* HandleMultipleNetworkTransitionsForLDAP
//------------------------------------------------------------------------------------

void CLDAPv3Plugin::HandleMultipleNetworkTransitionsForLDAP ( void )
{
	void	   *ptInfo		= nil;
	
	//let us be smart about doing the check
	//we would like to wait a short period for the Network transitions to subside
	//since we don't want to re-init multiple times during this wait period
	//however we do go ahead and fire off timers each time
	//each call in here we update the delay time by 5 seconds
	fTimeToHandleNetworkTransition = dsTimestamp() + USEC_PER_SEC*5;

	if (fServerRunLoop != nil)
	{
		ptInfo = (void *)this;
		CFRunLoopTimerContext c = {0, (void*)ptInfo, NULL, NULL, NetworkChangeLDAPPICopyStringCallback};
	
		CFRunLoopTimerRef timer = CFRunLoopTimerCreate(	NULL,
														CFAbsoluteTimeGetCurrent() + 5,
														0,
														0,
														0,
														DoLDAPPINetworkChange,
														(CFRunLoopTimerContext*)&c);
	
		CFRunLoopAddTimer(fServerRunLoop, timer, kCFRunLoopDefaultMode);
		if (timer) CFRelease(timer);
	}
} // HandleMultipleNetworkTransitionsForLDAP


//------------------------------------------------------------------------------------
//	* ReInitForNetworkTransition
//------------------------------------------------------------------------------------

void CLDAPv3Plugin::ReInitForNetworkTransition ( void )
{
	if ( dsTimestamp() >= fTimeToHandleNetworkTransition )
	{
		Initialize();
		fLDAPSessionMgr.NetTransition();
	}

}// ReInitForNetworkTransition


//------------------------------------------------------------------------------------
//	* DSDoSearch
//------------------------------------------------------------------------------------

void DSDoSearch (	char			   *inRecType,
					char			   *inNativeRecType,
					CAttributeList	   *inAttrTypeList,
					sLDAPContextData   *inContext,
					sLDAPContinueData  *inContinue,
					CLDAPv3Configs	   *inConfigFromXML,
					ber_int_t			inScope,
					CLDAPNode		   &inLDAPSessionMgr,
					char			   *inQueryFilter,
					int				   &outLDAPReturnCode,
					int					inSearchTO,
					LDAPMessage		  *&outResult)
{
	sInt32				searchResult	= eDSNoErr;
	char			  **attrs			= nil;
	int					ldapMsgId		= 0;
	int					numRetries		= 2; //two tries to rebind
	
	try
	{
	
		// here we check if there was a LDAP message ID in the context
		// If there was we continue to query, otherwise we search anew
		if (inContinue->msgId == 0)
		{
			attrs = CLDAPv3Plugin::MapAttrListToLDAPTypeArray( inRecType, inAttrTypeList, inContext->fConfigTableIndex, inConfigFromXML );
			// here is the call to the LDAP server asynchronously which requires
			// host handle, search base, search scope(LDAP_SCOPE_SUBTREE for all), search filter,
			// attribute list (NULL for all), return attrs values flag
			// This returns us the message ID which is used to query the server for the results
			while (true)
			{
				searchResult = DSSearchLDAP(	inLDAPSessionMgr,
												inContext,
												inNativeRecType,
												inScope,
												inQueryFilter,
												attrs,
												ldapMsgId,
												outLDAPReturnCode,
                                                                                                inContinue->fLimitRecSearch );
												
				if (	(searchResult == eDSNoErr) &&
						((inContinue->fTotalRecCount < inContinue->fLimitRecSearch) || (inContinue->fLimitRecSearch == 0)) )
				{
					searchResult = DSGetSearchLDAPResult (	inLDAPSessionMgr,
															inContext,
															inSearchTO,
															outResult,
															LDAP_MSG_ONE,
															ldapMsgId,
															outLDAPReturnCode,
															false);
				}
	
				if (searchResult == eDSRecordNotFound)
				{
					throw( (sInt32)eDSRecordNotFound );
				}
				else if (searchResult == eDSCannotAccessSession)
				{
					if (numRetries == 0)
					{
						throw( (sInt32)eDSCannotAccessSession );
					}
					else
					{
						CLDAPv3Plugin::RebindLDAPSession(inContext, inLDAPSessionMgr, inConfigFromXML);
						numRetries--;
					}
				}
				else if (searchResult == eDSServerTimeout)
				{
					throw( (sInt32)eDSServerTimeout );
				}
				else
				{
					break;
				}
			}//while
			
			inContinue->msgId = ldapMsgId;
		} // msgId == 0
		else
		{
			if ( (inContinue->fTotalRecCount < inContinue->fLimitRecSearch) || (inContinue->fLimitRecSearch == 0) )
			{
				//check if there is a carried LDAP message in the context
				//with a rebind here in between context calls we still have the previous result
				//however, the next call will start right over in the whole context of the ldap_search
				if (inContinue->pResult != nil)
				{
					outResult = inContinue->pResult;
					outLDAPReturnCode = LDAP_RES_SEARCH_ENTRY;
				}
				//retrieve a new LDAP message
				else
				{
					searchResult = DSGetSearchLDAPResult (	inLDAPSessionMgr,
															inContext,
															inSearchTO,
															outResult,
															LDAP_MSG_ONE,
															inContinue->msgId,
															outLDAPReturnCode,
															false);
					if (searchResult == eDSCannotAccessSession)
					{
						throw( (sInt32)eDSCannotAccessSession );
					}
					else if (searchResult == eDSServerTimeout)
					{
						throw( (sInt32)eDSServerTimeout );
					}
				}
			}
		}
	}
	
	catch ( sInt32 err )
	{
		searchResult = err;
	}

	if (attrs != nil)
	{
		int aIndex = 0;
		while (attrs[aIndex] != nil)
		{
			free(attrs[aIndex]);
			attrs[aIndex] = nil;
			aIndex++;
		}
		free(attrs);
		attrs = nil;
	}
	
	//throw out if error and after cleanup of local variables
	if (searchResult != eDSNoErr)
	{
		throw(searchResult);
	}
	
} // DSDoSearch

//------------------------------------------------------------------------------------
//	* DSDoRetrieval
//------------------------------------------------------------------------------------

sInt32 DSDoRetrieval (	char			   *inNativeRecType,
						char			  **inAttrs,
						sLDAPContextData   *inContext,
						ber_int_t			inScope,
						CLDAPNode		   &inLDAPSessionMgr,
						char			   *inQueryFilter,
						int				   &outLDAPReturnCode,
						int					inSearchTO,
						LDAPMessage		  *&outResult,
						bool			   &outResultFound,
						CLDAPv3Configs     *inConfigFromXML)
{
	sInt32				searchResult	= eDSNoErr;
	int					ldapMsgId		= 0;
	int					numRetries		= 2; //two tries to rebind

	try
	{
		// here is the call to the LDAP server asynchronously which requires
		// host handle, search base, search scope(LDAP_SCOPE_SUBTREE for all), search filter,
		// attribute list (NULL for all), return attrs values flag
		while (true)
		{
			searchResult = DSSearchLDAP(	inLDAPSessionMgr,
											inContext,
											inNativeRecType,
											inScope,
											inQueryFilter,
											inAttrs,
											ldapMsgId,
											outLDAPReturnCode,
                                                                                        1 );
			if (searchResult == eDSNoErr)
			{
				// retrieve the actual LDAP record data for use internally
				// KW should we internally re-read the result after a write?
				searchResult = DSGetSearchLDAPResult (	inLDAPSessionMgr,
														inContext,
														inSearchTO,
														outResult,
														LDAP_MSG_ONE,
														ldapMsgId,
														outLDAPReturnCode,
														true);
			}
			outResultFound = false;
			if (searchResult == eDSRecordNotFound)
			{
				break;
			}
			else if (searchResult == eDSCannotAccessSession)
			{
				if (numRetries == 0)
				{
					throw( (sInt32)eDSCannotAccessSession );
				}
				else
				{
					CLDAPv3Plugin::RebindLDAPSession(inContext, inLDAPSessionMgr, inConfigFromXML);
					numRetries--;
				}
			}
			else if (searchResult == eDSServerTimeout)
			{
				break;
			}
			else
			{
				outResultFound = true;
				break;
			}
		} // while
	}
	
	catch ( sInt32 err )
	{
		searchResult = err;
	}

	//throw out if eDSCannotAccessSession error only after cleanup of local variables
	if (searchResult == eDSCannotAccessSession)
	{
		throw(searchResult);
	}
	
	return(searchResult);
	
} // DSDoRetrieval
