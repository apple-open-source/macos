/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

/*!
 * @header CLDAPv3Plugin
 * LDAP v3 plugin implementation to interface with Directory Services.
 */

#pragma mark Includes

#include <string.h>		//used for strcpy, etc.
#include <stdlib.h>		//used for malloc
#include <ctype.h>		//used for isprint
#include <syslog.h>
#include <arpa/inet.h>
#include <libkern/OSAtomic.h>
#include <CommonCrypto/CommonDigest.h>
#include <Security/Authorization.h>
#include <SystemConfiguration/SystemConfiguration.h>	//required for the configd kicker operation
#include <SystemConfiguration/SCDynamicStoreCopyDHCPInfo.h>
#include <CoreFoundation/CFPriv.h>

#include "DirServices.h"
#include "DirServicesUtils.h"
#include "DirServicesUtilsPriv.h"
#include "DirServicesConst.h"
#include "DirServicesPriv.h"
extern "C" {
	#include "saslutil.h"
};

#include "CLDAPv3Plugin.h"
#include <krb5.h>

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
#include "GetMACAddress.h"
#include "DSLDAPUtils.h"
#include "buffer_unpackers.h"
#include "CDSPluginUtils.h"
#include "LDAPv3SupportFunctions.h"
#include "CLDAPNodeConfig.h"
#include "CPlugInObjectRef.h"

#pragma mark -
#pragma mark Defines and Macros

#define LDAPURLOPT95SEPCHAR				" "
#define LDAPCOMEXPSPECIALCHARS			"()|&"

#define kLDAPRecordTypeUnknownStr		"Record Type Unknown"
#define kLDAPRecordNameUnknownStr		"Record Name Unknown"
#define kLDAPUnknownNodeLocationStr		"Unknown Node Location"
#define kLDAPv3NodePrefix				"/LDAPv3/"

#pragma mark -
#pragma mark Globals and Structures

// --------------------------------------------------------------------------------
//	Globals
extern	bool		gServerOS;

CContinue								*gLDAPContinueTable			= nil;
CPlugInObjectRef<sLDAPContextData *>	*gLDAPContextTable			= nil;
static DSEventSemaphore					gNetworkTransition;

// replica synchronization
int							gPWSReplicaListLoadedSeqNumber	= 0;
int							gConfigRecordChangeSeqNumber	= 1;
char						*gPWSReplicaListLastRSAStr		= NULL;
tDataBufferPtr				gPWSReplicaListXMLBuffer		= NULL;
DSMutexSemaphore			gPWSReplicaListXMLBufferMutex("::gPWSReplicaListXMLBufferMutex");

//TODO KW the AuthAuthority definitions need to come from DirectoryServiceCore
struct LDAPv3AuthAuthorityHandler
{
	char							*fTag;
	LDAPv3AuthAuthorityHandlerProc	fHandler;
};

#define		kLDAPv3AuthAuthorityHandlerProcs		3

static LDAPv3AuthAuthorityHandler sLDAPv3AuthAuthorityHandlerProcs[ kLDAPv3AuthAuthorityHandlerProcs ] =
{
	{ kDSTagAuthAuthorityBasic,				(LDAPv3AuthAuthorityHandlerProc)CLDAPv3Plugin::DoBasicAuth },
	{ kDSTagAuthAuthorityPasswordServer,	(LDAPv3AuthAuthorityHandlerProc)CLDAPv3Plugin::DoPasswordServerAuth },
	{ kDSTagAuthAuthorityKerberosv5,		(LDAPv3AuthAuthorityHandlerProc)CLDAPv3Plugin::DoKerberosAuth }
};

#pragma mark -
#pragma mark CLDAPv3Plugin Member Functions

// --------------------------------------------------------------------------------
//	* CLDAPv3Plugin ()
// --------------------------------------------------------------------------------

CLDAPv3Plugin::CLDAPv3Plugin ( FourCharCode inSig, const char *inName )
	: BaseDirectoryPlugin( inSig, inName )
{
	if ( gLDAPContinueTable == NULL )
	{
		CContinue *pContinue = new CContinue( CLDAPv3Plugin::ContinueDeallocProc, (gServerOS ? 256 : 64) );
		
		// got set by someone else, let's delete this one
		if ( OSAtomicCompareAndSwapPtrBarrier(NULL, pContinue, (void **) &gLDAPContinueTable) == true )
			DbgLog( kLogPlugin, "CLDAPv3Plugin::CLDAPv3Plugin - created the Continue Data table" );
		else
			DSDelete( pContinue );
	}

	if ( gLDAPContextTable == NULL )
	{
		CPlugInObjectRef<sLDAPContextData *> *pRefTable = new CPlugInObjectRef<sLDAPContextData *>( gServerOS ? 1024 : 256 );
		
		// got set by someone else, let's delete this one
		if ( OSAtomicCompareAndSwapPtrBarrier(NULL, pRefTable, (void **) &gLDAPContextTable) == true )
			DbgLog( kLogPlugin, "CLDAPv3Plugin::CLDAPv3Plugin - created the Ref table" );
		else
			DSDelete( pRefTable );
	}

	gNetworkTransition.PostEvent(); // post network transition, we'll get a new one right after if needed

	fConfigFromXML = new CLDAPv3Configs( fPlugInSignature );
	fLDAPConnectionMgr = new CLDAPConnectionManager( fConfigFromXML );
} // CLDAPv3Plugin


// --------------------------------------------------------------------------------
//	* ~CLDAPv3Plugin ()
// --------------------------------------------------------------------------------

CLDAPv3Plugin::~CLDAPv3Plugin ( void )
{
	DSDelete( fConfigFromXML );
	DSDelete( fLDAPConnectionMgr );

	// we don't clean up context tables because they can block shutdown of the daemon if this object
	// is deleted.
} // ~CLDAPv3Plugin


// --------------------------------------------------------------------------------
//	* Validate ()
// --------------------------------------------------------------------------------

SInt32 CLDAPv3Plugin::Validate ( const char *inVersionStr, const UInt32 inSignature )
{
	fPlugInSignature = inSignature;

	return( eDSNoErr );

} // Validate


// --------------------------------------------------------------------------------
//	* Initialize ()
// --------------------------------------------------------------------------------

SInt32 CLDAPv3Plugin::Initialize ( void )
{
	// this is no longer needed, but too many dependencies to remove
	tDataListPtr pldapName = dsBuildListFromStringsPriv( "LDAPv3", "DHCPChecked", NULL );
	if (pldapName != NULL)
	{
		CServerPlugin::_RegisterNode( fPlugInSignature, pldapName, kDHCPLDAPv3NodeType );
		dsDataListDeallocatePriv( pldapName );
		DSFree( pldapName );
	}
	
	// base directory plugin sets the flags
	return BaseDirectoryPlugin::Initialize();
} // Initialize

// --------------------------------------------------------------------------------
//	* SetPluginState ()
// --------------------------------------------------------------------------------

static CFRunLoopTimerRef	gActiveTimer		= NULL;

static void RegisterAllNodesCallback( CFRunLoopTimerRef inTimer, void *inInfo )
{
	CLDAPv3Configs *configs = (CLDAPv3Configs *)inInfo;
	if ( configs != NULL )
		configs->RegisterAllNodes();
	OSAtomicCompareAndSwapPtrBarrier( inTimer, NULL, (void **) &gActiveTimer );
}

SInt32 CLDAPv3Plugin::SetPluginState ( const UInt32 inState )
{
	BaseDirectoryPlugin::SetPluginState( inState );
	
	// add a timer to add our Nodes so we can't deadlock during init
	if ( inState & kActive != 0 && gActiveTimer == NULL )
	{
		CFRunLoopTimerContext	context = { 0, fConfigFromXML, NULL, NULL, NULL };

		// we only want one active at a time for registration, use atomic instead of a lock
		CFRunLoopTimerRef cfTimer = CFRunLoopTimerCreate( NULL, CFAbsoluteTimeGetCurrent(), 0, 0, 0, RegisterAllNodesCallback, &context );
		if ( OSAtomicCompareAndSwapPtrBarrier(NULL, cfTimer, (void **) &gActiveTimer) == true )
		{
			DbgLog( kLogPlugin, "CLDAPv3Plugin::SetPluginState - Added timer for registering our nodes" );
			CFRunLoopAddTimer( fPluginRunLoop, cfTimer, kCFRunLoopDefaultMode );
		}
		
		DSCFRelease( cfTimer );
	}
	else if ( inState & kInactive )
		fConfigFromXML->UnregisterAllNodes();

	return( eDSNoErr );

} // SetPluginState


// ---------------------------------------------------------------------------
//	* ProcessRequest
//
// ---------------------------------------------------------------------------

SInt32 CLDAPv3Plugin::ProcessRequest ( void *inData )
{
	tDirStatus	siResult	= eDSNoErr;
	char	   *pathStr		= nil;
    sHeader		*pMsgHdr	= (sHeader *) inData;

	if ( inData == nil )
	{
		return( ePlugInDataError );
	}
    
	if (pMsgHdr->fType == kOpenDirNode)
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
	
	// we allow a subset of calls if we are not initialized
	switch (pMsgHdr->fType)
	{
		case kKerberosMutex:
			siResult = (tDirStatus) BaseDirectoryPlugin::ProcessRequest( inData );
			break;
		case kServerRunLoop:
			siResult = (tDirStatus) BaseDirectoryPlugin::ProcessRequest( inData );
			break;
		default:
			siResult = eDSNoErr;
			break;
	}
	
	if (fState == kUnknownState)
	{
		return( ePlugInCallTimedOut );
	}

	if ( (fState & kFailedToInit) || !(fState & kInitialized) )
	{
		DbgLog( kLogPlugin, "CLDAPPlugin::ProcessRequest -14279 condition fState = %d", fState );
		return( ePlugInFailedToInitialize );
	}

	if ( ((fState & kInactive) || !(fState & kActive))
		  && (pMsgHdr->fType != kDoPlugInCustomCall)
		  && (pMsgHdr->fType != kOpenDirNode) )
	{
        return( ePlugInNotActive );
	}
	
	fConfigFromXML->WaitForNodeRegistration();
	
	siResult = (tDirStatus) HandleRequest( inData );
	if ( siResult == eNotHandledByThisNode )
		siResult = (tDirStatus) BaseDirectoryPlugin::ProcessRequest( inData );
	
	return( siResult );

} // ProcessRequest

#pragma mark -
#pragma mark Support functions

// ---------------------------------------------------------------------------
//	* HandleRequest
//
// ---------------------------------------------------------------------------

SInt32 CLDAPv3Plugin::HandleRequest ( void *inData )
{
	tDirStatus	siResult	= eDSNoErr;
	sHeader		*pMsgHdr	= nil;
	char		*recTypeStr	= NULL;
	
	if ( inData == nil )
	{
		return( -8088 );
	}

	pMsgHdr = (sHeader *)inData;

	//use of LDAP connections will be tracked with LockSession / UnlockSession
	
	switch ( pMsgHdr->fType )
	{
		case kSetRecordType:
			siResult = eNotYetImplemented; //KW not to be implemented
			break;
					
		case kSetAttributeValues:
			recTypeStr = GetRecordTypeFromRef( ((sAddAttribute *)inData)->fInRecRef );
			siResult = SetAttributeValues( (sSetAttributeValues *)inData, recTypeStr );
			break;
						
		case kDoDirNodeAuthOnRecordType:
			if (	( ((sDoDirNodeAuthOnRecordType *)inData)->fInRecordType != NULL) &&
					( ((sDoDirNodeAuthOnRecordType *)inData)->fInRecordType->fBufferData != NULL) )
					{
						siResult = (tDirStatus) DoAuthenticationOnRecordType(	(sDoDirNodeAuthOnRecordType *)inData,
																				((sDoDirNodeAuthOnRecordType *)inData)->fInRecordType->fBufferData );
					}
			break;
			
		case kDoAttributeValueSearch:
		case kDoAttributeValueSearchWithData:
		case kDoMultipleAttributeValueSearch:
		case kDoMultipleAttributeValueSearchWithData:
			siResult = DoAttributeValueSearchWithData( (sDoAttrValueSearchWithData *)inData );
			break;

		case kDoPlugInCustomCall:
			siResult = DoPlugInCustomCall( (sDoPlugInCustomCall *)inData );
			break;
		
		case kHandleSystemWillSleep:
			siResult = eDSNoErr;
			fLDAPConnectionMgr->SystemGoingToSleep();
			break;

		case kHandleSystemWillPowerOn:
			siResult = eDSNoErr;
			fLDAPConnectionMgr->SystemWillPowerOn();
			break;

		default:
			siResult = eNotHandledByThisNode;
			break;
	}

	pMsgHdr->fResult = siResult;
	
	DSFreeString( recTypeStr );
	
	return( siResult );
} // HandleRequest


//------------------------------------------------------------------------------------
//	* OpenDirNode
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::OpenDirNode ( sOpenDirNode *inData )
{
	char				*pathStr		= nil;
	sLDAPContextData	*pContext		= nil;
	tDirStatus			siResult		= eDSNoErr;
    tDataListPtr		pNodeList		= nil;

    pNodeList	=	inData->fInDirNodeName;
    
	try
	{
		if ( inData != nil )
		{
			pathStr = dsGetPathFromListPriv( pNodeList, (char *)"/" );
			if ( pathStr == nil ) throw( eDSNullNodeName );
			
			//special case for the configure LDAPv3 node
			if (strcmp(pathStr,"/LDAPv3") == 0)
			{
				// set up the context data now with the relevant parameters for the configure LDAPv3 node
				// DS API reference number is used to access the reference table
				pContext = new sLDAPContextData;
				pContext->fUID = inData->fInUID;
				pContext->fEffectiveUID = inData->fInEffectiveUID;
				pContext->fLDAPConnection = new CLDAPConnection( NULL );
				
				//generic hash index
				// add the item to the reference table
				gLDAPContextTable->AddObjectForRefNum( inData->fOutNodeRef, pContext );
				DSRelease( pContext );
			}
			// check that there is something after the delimiter or prefix
			// strip off the LDAPv3 prefix here
			else if ( strlen(pathStr) > (sizeof(kLDAPv3NodePrefix) - 1) && 
					  strncmp(pathStr, kLDAPv3NodePrefix, (sizeof(kLDAPv3NodePrefix) - 1)) == 0 )
			{
				char *ldapName = pathStr + 8;
				
				if ( ldap_is_ldapi_url(ldapName) )
				{
					if ( inData->fInUID != 0 || inData->fInEffectiveUID != 0 )
						throw( eDSOpenNodeFailed );
				}
				
				CLDAPConnection *pConnection = fLDAPConnectionMgr->GetConnection( ldapName );
				if ( pConnection == NULL )
					throw( eDSOpenNodeFailed );

				pContext = new sLDAPContextData( pConnection );
				pContext->fUID = inData->fInUID;
				pContext->fEffectiveUID = inData->fInEffectiveUID;
				
				pConnection->Release();
				
				// add the item to the reference table
				gLDAPContextTable->AddObjectForRefNum( inData->fOutNodeRef, pContext );
				DSRelease( pContext );
			} // there was some name passed in here ie. length > 1
			else
			{
				siResult = eDSOpenNodeFailed;
			}
		} // inData != nil
		else
		{
			siResult = eDSNullParameter;
		}
	} // try
	catch ( tDirStatus err )
	{
		siResult = err;
		if (pContext != nil)
			gLDAPContextTable->RemoveRefNum( inData->fOutNodeRef );
	}
	
	DSDelete( pathStr );

	return( siResult );

} // OpenDirNode

//------------------------------------------------------------------------------------
//	* CloseDirNode
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::CloseDirNode ( sCloseDirNode *inData )
{
	tDirStatus			siResult	= eDSNoErr;
	sLDAPContextData   *pContext	= nil;

	try
	{
		pContext = gLDAPContextTable->GetObjectForRefNum( inData->fInNodeRef );
		if ( pContext == nil )
			throw( eDSBadContextData );
		
		// our last chance to clean up anything we missed for that node 
		gLDAPContinueTable->RemoveItems( inData->fInNodeRef ); // clean up continues before we remove the first context itself..

		gLDAPContextTable->RemoveRefNum( inData->fInNodeRef );
		DSRelease( pContext );
	}

	catch ( tDirStatus err )
	{
		siResult = err;
	}

	return( siResult );

} // CloseDirNode


//------------------------------------------------------------------------------------
//	* GetRecordList
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::GetRecordList ( sGetRecordList *inData )
{
    tDirStatus				siResult			= eDSNoErr;
    UInt32					uiTotal				= 0;
    UInt32					uiCount				= 0;
    char				   *pRecType			= nil;
    char				   *pLDAPSearchBase		= nil;
    bool					bAttribOnly			= false;
    tDirPatternMatch		pattMatch			= eDSNoMatch1;
    char				  **pRecNames			= nil;
    CAttributeList		   *cpRecTypeList		= nil;
	UInt32					cpRecTypeListCount	= 0;
    CAttributeList		   *cpAttrTypeList 		= nil;
    sLDAPContextData	   *pContext			= nil;
    sLDAPContinueData	   *pLDAPContinue		= nil;
    CBuff				   *outBuff				= nil;
	bool					bOCANDGroup			= false;
	CFArrayRef				OCSearchList		= nil;
	ber_int_t				scope				= LDAP_SCOPE_SUBTREE;

	// Verify all the parameters
	if ( inData == nil ) return( eMemoryError );
	if ( inData->fInDataBuff == NULL || inData->fInDataBuff->fBufferSize == 0 ) return( eDSEmptyBuffer );
	if ( inData->fInRecNameList == nil ) return( eDSEmptyRecordNameList );
	if ( inData->fInRecTypeList == nil ) return( eDSEmptyRecordTypeList );
	if ( inData->fInAttribTypeList == nil ) return( eDSEmptyAttributeTypeList );
	
	// Node context data
	pContext = gLDAPContextTable->GetObjectForRefNum( inData->fInNodeRef );
	if ( pContext == nil ) return eDSBadContextData;
	
	if ( pContext->fLDAPConnection->ConnectionStatus() != kConnectionSafe ) return eDSCannotAccessSession;
	
    try
    { 
		// we are issuing the search for the first time
		if ( inData->fIOContinueData == nil )
		{
			pLDAPContinue = (sLDAPContinueData *) calloc( 1, sizeof(sLDAPContinueData) );

			gLDAPContinueTable->AddItem( pLDAPContinue, inData->fInNodeRef );

            //parameters used for data buffering
			pLDAPContinue->fLDAPConnection = pContext->fLDAPConnection->Retain();
			pLDAPContinue->fNodeRef = inData->fInNodeRef;
            pLDAPContinue->fRecNameIndex = 1;
            pLDAPContinue->fRecTypeIndex = 1;
            pLDAPContinue->fTotalRecCount = 0;
            pLDAPContinue->fLimitRecSearch = (inData->fOutRecEntryCount >= 0 ? inData->fOutRecEntryCount : 0);
		}
		else
		{
			pLDAPContinue = (sLDAPContinueData *)inData->fIOContinueData;
			if ( gLDAPContinueTable->VerifyItem( pLDAPContinue ) == false ) throw( eDSInvalidContinueData );
		}

        // start with the continue set to nil until buffer gets full and there is more data
        //OR we have more record types to look through
        inData->fIOContinueData		= NULL;
		//return zero if nothing found here
		inData->fOutRecEntryCount	= 0;
		
        // copy the buffer data into a more manageable form
        outBuff = new CBuff();
        if ( outBuff == nil ) throw( eMemoryError );

        siResult = (tDirStatus) outBuff->Initialize( inData->fInDataBuff, true );
        if ( siResult != eDSNoErr ) throw( siResult );

        siResult = (tDirStatus) outBuff->GetBuffStatus();
        if ( siResult != eDSNoErr ) throw( siResult );

        siResult = (tDirStatus) outBuff->SetBuffType( 'StdA' );
        if ( siResult != eDSNoErr ) throw( siResult );

        // Get the attribute strings to match
		pRecNames = dsAllocStringsFromList(0, inData->fInRecNameList);
		if ( pRecNames == nil ) throw( eDSEmptyRecordNameList );

        // Get the record name pattern match type
        pattMatch = inData->fInPatternMatch;

        // Get the record type list
        // Record type mapping for LDAP to DS API is dealt with below
        cpRecTypeList = new CAttributeList( inData->fInRecTypeList );
        if ( cpRecTypeList == nil ) throw( eDSEmptyRecordTypeList );

        if ( (cpRecTypeListCount = cpRecTypeList->GetCount()) == 0 )
			throw( eDSEmptyRecordTypeList );
		
        // Get the attribute list
        //KW? at this time would like to simply dump all attributes
        //would expect to do this always since this is where the databuffer is built
        cpAttrTypeList = new CAttributeList( inData->fInAttribTypeList );
        if ( cpAttrTypeList == nil ) throw( eDSEmptyAttributeTypeList );
        if (cpAttrTypeList->GetCount() == 0)
			throw( eDSEmptyAttributeTypeList );
		
        // Get the attribute info only flag
        bAttribOnly = inData->fInAttribInfoOnly;

        // get records of these types
        while ( siResult == eDSNoErr && cpRecTypeList->GetAttribute(pLDAPContinue->fRecTypeIndex, &pRecType) == eDSNoErr )
        {
            pLDAPSearchBase = MapRecToSearchBase( pContext, (const char *)pRecType, 1, &bOCANDGroup, &OCSearchList, &scope );
            if ( pLDAPSearchBase == NULL )
			{
				pLDAPContinue->fRecTypeIndex++;
				continue;
			}
			
			if ( strcmp(pRecNames[0], kDSRecordsAll) == 0 ) //should be only a single value of kDSRecordsAll in this case
			{
				siResult = GetAllRecords( pRecType, pLDAPSearchBase, cpAttrTypeList, pContext, pLDAPContinue, bAttribOnly, outBuff, 
										  uiCount, bOCANDGroup, OCSearchList, scope );
			}
			else
			{
				siResult = GetTheseRecords( (char *)kDSNAttrRecordName, pRecNames, pRecType, pLDAPSearchBase, pattMatch, cpAttrTypeList, 
											pContext, pLDAPContinue, bAttribOnly, outBuff, uiCount, bOCANDGroup, OCSearchList, scope );
			}

			bOCANDGroup = false;
			DSDelete( pLDAPSearchBase );
			DSCFRelease( OCSearchList );
			
			// add the found records to the total
			uiTotal += uiCount;
			
			// see if we got a buffer too small, it just means we couldn't put any from the search
			// but there may already be some in the buffer
			if ( siResult == eDSBufferTooSmall )
			{
				//set continue if there is more data available
				inData->fIOContinueData = pLDAPContinue;
				
				// if we've put some records we change the error
				if ( uiTotal != 0 )
					siResult = eDSNoErr;
				
				inData->fOutRecEntryCount = uiTotal;
				outBuff->SetLengthToSize();
				break;
			}
			else if ( siResult == eDSNoErr )
			{
				pLDAPContinue->fRecTypeIndex++;
				if ( pLDAPContinue->fRecTypeIndex > cpRecTypeListCount )
				{
					inData->fOutRecEntryCount = uiTotal;
					outBuff->SetLengthToSize();
					break;
				}
			}
        } // while loop over record types

		if ( uiTotal == 0 )
			outBuff->ClearBuff();
    } // try
    
    catch ( tDirStatus err )
    {
		siResult = err;
    }
	
	if ( inData->fIOContinueData == NULL && pLDAPContinue != NULL )
	{
		// we've decided not to return continue data, so we should clean up
		gLDAPContinueTable->RemoveItem( pLDAPContinue );
		pLDAPContinue = nil;
	}
	
	DSDelete( pLDAPSearchBase );
	DSDelete( cpRecTypeList );
	DSDelete( cpAttrTypeList );
	DSDelete( outBuff );
	DSFreeStringList( pRecNames );
	DSRelease( pContext );

    return( (tDirStatus)siResult );

} // GetRecordList


//------------------------------------------------------------------------------------
//	* GetAllRecords
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::GetAllRecords (
	char			   *inRecType,
	char			   *inNativeRecType,
	CAttributeList	   *inAttrTypeList,
	sLDAPContextData   *inContext,
	sLDAPContinueData  *inContinue,
	bool				inAttrOnly,
	CBuff			   *inBuff,
	UInt32			   &outRecCount,
	bool				inbOCANDGroup,
	CFArrayRef			inOCSearchList,
	ber_int_t			inScope )
{
    tDirStatus			siResult		= eDSNoErr;
    SInt32				siValCnt		= 0;
    LDAPMessage			*result			= nil;
    char				*recName		= nil;
	CDataBuff			*aRecData		= nil;
	CDataBuff			*aAttrData		= nil;
    char				*queryFilter	= nil;
	
	outRecCount = 0; //need to track how many records were found by this call to GetAllRecords

	if ( inContext == NULL ) return( eDSInvalidContext );
	if ( inContinue == NULL ) return( eDSInvalidContinueData );
	if ( inContext->fLDAPConnection->ConnectionStatus() != kConnectionSafe ) return eDSCannotAccessSession;
	
	//TODO why not optimize and save the queryFilter in the continue data for the next call in?
	//not that easy over continue data
	
	//build the record query string if we haven't initiated the search
	if ( inContinue->fLDAPMsgId == 0 )
	{
		queryFilter = BuildLDAPQueryFilter(	inContext,
										    nil,
										    nil,
										    eDSAnyMatch,
										    false,
										    inRecType,
										    inNativeRecType,
										    inbOCANDGroup,
										    inOCSearchList );
		
    	// check to make sure the queryFilter is not nil
		if ( queryFilter == NULL ) return eDSNullParameter;
	}

	char **attrs = MapAttrListToLDAPTypeArray( inRecType, inAttrTypeList, inContext );

    try
    {
		aRecData = new CDataBuff();
		if ( aRecData == nil ) throw( eMemoryError );
		aAttrData = new CDataBuff();
		if ( aAttrData == nil ) throw( eMemoryError );
		
		do
		{
			siResult = DSInitiateOrContinueSearch( inContext, inContinue, inNativeRecType, attrs, inScope, queryFilter, &result );
			if ( siResult == eDSNoErr && result != NULL )
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
					aRecData->AppendShort( strlen( inRecType ) );
					aRecData->AppendString( inRecType );
				} // what to do if the inRecType is nil? - never get here then
				else
				{
					aRecData->AppendShort( sizeof(kLDAPRecordTypeUnknownStr) - 1 );
					aRecData->AppendString( kLDAPRecordTypeUnknownStr );
				}
				
				// need to get the record name
				recName = GetRecordName( inRecType, result, inContext, siResult );
				if ( siResult != eDSNoErr ) break;
				if ( recName != nil )
				{
					aRecData->AppendShort( strlen( recName ) );
					aRecData->AppendString( recName );
					
					DSFree( recName );
				}
				else
				{
					aRecData->AppendShort( sizeof(kLDAPRecordNameUnknownStr) - 1 );
					aRecData->AppendString( kLDAPRecordNameUnknownStr );
				}
				
				// need to calculate the number of attribute types ie. siValCnt
				// also need to extract the attributes and place them into fAttrData
				
				aAttrData->Clear();
				siResult = GetTheseAttributes( inRecType, inAttrTypeList, result, inAttrOnly, inContext, siValCnt, aAttrData );
				if ( siResult != eDSNoErr ) break;
				
				//add the attribute info to the fRecData
				aRecData->AppendShort( siValCnt );
				
				if ( siValCnt != 0 )
					aRecData->AppendBlock( aAttrData->GetData(), aAttrData->GetLength() );
				
				// add the fRecData now to the inBuff
				if ( inBuff->AddData( aRecData->GetData(), aRecData->GetLength()) == CBuff::kBuffFull )
				{
					siResult = eDSBufferTooSmall;
					inContinue->fResult = result;
					result = NULL;
					break;
				}
				else
				{
					outRecCount++; //another record added
					inContinue->fTotalRecCount++;
					
					ldap_msgfree( result );
					result = NULL;
				}
			}
			else if ( siResult == eDSRecordNotFound )
			{
				// error signifies we're done with those results
				siResult = eDSNoErr;
				break;
			}

		} while ( siResult == eDSNoErr && 
				  (inContinue->fLimitRecSearch == 0 || inContinue->fTotalRecCount < inContinue->fLimitRecSearch) );

    } // try block
    catch ( tDirStatus err )
    {
        siResult = err;
    }
	
	// free any pointer here
	if ( result != NULL )
	{
		ldap_msgfree( result );
		result = NULL;
	}
	
	DSFreeStringList( attrs );
	DSFreeString( queryFilter );
	DSDelete( aRecData );
	DSDelete( aAttrData );

    return siResult;

} // GetAllRecords


//------------------------------------------------------------------------------------
//	* GetRecordEntry
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::GetRecordEntry ( sGetRecordEntry *inData )
{
    tDirStatus				siResult		= eDSNoErr;
    UInt32					uiIndex			= 0;
    UInt32					uiCount			= 0;
    UInt32					uiOffset		= 0;
	UInt32					uberOffset		= 0;
    char					*pData			= nil;
    tRecordEntryPtr			pRecEntry		= nil;
    sLDAPContextData		*pContext		= nil;
    CBuff					inBuff;
	UInt32					offset			= 0;
	UInt16					usTypeLen		= 0;
	char				   *pRecType		= nil;
	UInt16					usNameLen		= 0;
	char				   *pRecName		= nil;
	UInt16					usAttrCnt		= 0;
	UInt32					buffLen			= 0;

	if ( inData == nil ) return( eMemoryError );
	if ( inData->fInOutDataBuff == nil || inData->fInOutDataBuff->fBufferSize == 0 )
		return( eDSEmptyBuffer );
	
    try
    {
        siResult = (tDirStatus) inBuff.Initialize( inData->fInOutDataBuff );
        if ( siResult != eDSNoErr ) throw( siResult );

        siResult = (tDirStatus) inBuff.GetDataBlockCount( &uiCount );
        if ( siResult != eDSNoErr ) throw( siResult );

        uiIndex = inData->fInRecEntryIndex;
        if ( uiIndex == 0 ) throw( eDSInvalidIndex );

		if ( uiIndex > uiCount ) throw( eDSIndexOutOfRange );

        pData = inBuff.GetDataBlock( uiIndex, &uberOffset );
        if ( pData == nil ) throw( eDSCorruptBuffer );

		//assume that the length retrieved is valid
		buffLen = inBuff.GetDataBlockLength( uiIndex );
		
		// Skip past this same record length obtained from GetDataBlockLength
		pData	+= 4;
		offset	= 0; //buffLen does not include first four bytes

		// Do record check, verify that offset is not past end of buffer, etc.
		if (offset + 2 > buffLen)  throw( eDSInvalidBuffFormat );
		
		// Get the length for the record type
		memcpy( &usTypeLen, pData, 2 );
		
		pData	+= 2;
		offset	+= 2;

		pRecType = pData;
		
		pData	+= usTypeLen;
		offset	+= usTypeLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (offset + 2 > buffLen)  throw( eDSInvalidBuffFormat );
		
		// Get the length for the record name
		memcpy( &usNameLen, pData, 2 );
		
		pData	+= 2;
		offset	+= 2;

		pRecName = pData;
		
		pData	+= usNameLen;
		offset	+= usNameLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (offset + 2 > buffLen)  throw( eDSInvalidBuffFormat );
		
		// Get the attribute count
		memcpy( &usAttrCnt, pData, 2 );

		pRecEntry = (tRecordEntry *)calloc( 1, sizeof( tRecordEntry ) + usNameLen + usTypeLen + 4 + kBuffPad );

		pRecEntry->fRecordNameAndType.fBufferSize	= usNameLen + usTypeLen + 4 + kBuffPad;
		pRecEntry->fRecordNameAndType.fBufferLength	= usNameLen + usTypeLen + 4;

		// Add the record name length
		memcpy( pRecEntry->fRecordNameAndType.fBufferData, &usNameLen, 2 );
		uiOffset += 2;

		// Add the record name
		memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, pRecName, usNameLen );
		uiOffset += usNameLen;

		// Add the record type length
		memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, &usTypeLen, 2 );

		// Add the record type
		uiOffset += 2;
		memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, pRecType, usTypeLen );

		pRecEntry->fRecordAttributeCount = usAttrCnt;

        pContext = new sLDAPContextData;
        if ( pContext == nil ) throw( eMemoryAllocError );

        pContext->offset = uberOffset + offset + 4; // context used by next calls of GetAttributeEntry
													// include the four bytes of the buffLen

        gLDAPContextTable->AddObjectForRefNum( inData->fOutAttrListRef, pContext );
        inData->fOutRecEntryPtr = pRecEntry;
		DSRelease( pContext );
    }

    catch ( tDirStatus err )
    {
        siResult = err;
    }

    return( (tDirStatus)siResult );

} // GetRecordEntry


//------------------------------------------------------------------------------------
//	* GetTheseAttributes
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::GetTheseAttributes (	char			   *inRecType,
											CAttributeList	   *inAttrTypeList,
											LDAPMessage		   *inResult,
											bool				inAttrOnly,
											sLDAPContextData   *inContext,
											SInt32			   &outCount,
											CDataBuff		   *inDataBuff )
{
	tDirStatus			siResult				= eDSNoErr;
	SInt32				attrTypeIndex			= 1;
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
	char			   *pLDAPIPv6AddressAttrType = nil;
	char			   *pLDAPIPAddressAttrType	= nil;
	UInt32				literalLength			= 0;
	LDAP			   *aHost					= nil;

	if ( inContext == nil )
		return( eDSBadContextData );
	
	try
	{
		outCount = 0;
		aTmpData = new CDataBuff();
		if ( aTmpData == nil ) throw( eMemoryError );
		aTmp2Data = new CDataBuff();
		if ( aTmp2Data == nil ) throw( eMemoryError );
		inDataBuff->Clear();

		//Get the mapping for kDS1AttrPassword
		//If it exists AND it is mapped to something that exists IN LDAP then we will use it
		//otherwise we set bUsePlus and use the kDS1AttrPasswordPlus value of "*******"
		//Don't forget to strip off the {crypt} prefix from kDS1AttrPassword as well
		pLDAPPasswdAttrType = MapAttrToLDAPType( inContext, (const char *)inRecType, kDS1AttrPassword, 1, true );
		pLDAPIPv6AddressAttrType = MapAttrToLDAPType( inContext, (const char *)inRecType, kDSNAttrIPv6Address, 1, true );
		pLDAPIPAddressAttrType = MapAttrToLDAPType( inContext, (const char *)inRecType, kDSNAttrIPAddress, 1, true );
				
		// Get the record attributes with/without the values
		while ( inAttrTypeList->GetAttribute( attrTypeIndex++, &pAttrType ) == eDSNoErr && pAttrType != NULL )
		{
			siResult = eDSNoErr;
				
			if ( strcmp( kDSNAttrMetaNodeLocation, pAttrType ) == 0 )
			{
				// we look at kDSNAttrMetaNodeLocation with NO mapping
				// since we have special code to deal with it and we always place the
				// node name into it
				aTmpData->Clear();
				
				// Append the attribute name
				aTmpData->AppendShort( strlen( pAttrType ) );
				aTmpData->AppendString( pAttrType );

				if ( inAttrOnly == false )
				{
					// Append the attribute value count
					aTmpData->AppendShort( 1 );

					char *tmpStr = nil;

					//extract name from the context data
					//need to add prefix of /LDAPv3/ here since the fName does not contain it in the context
					CLDAPNodeConfig *nodeConfig = inContext->fLDAPConnection->fNodeConfig;
					if ( nodeConfig != NULL && nodeConfig->fNodeName != nil )
					{
		        		tmpStr = (char *) calloc(1, sizeof(kLDAPv3NodePrefix) + strlen(nodeConfig->fNodeName));
		        		strcpy( tmpStr, kLDAPv3NodePrefix );
		        		strcat( tmpStr, nodeConfig->fNodeName );
					}
					else
					{
						tmpStr = strdup( kLDAPUnknownNodeLocationStr );
					}

					// Append attribute value
					aTmpData->AppendLong( strlen( tmpStr ) );
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

			} // if ( strcmp( kDSNAttrMetaNodeLocation, pAttrType ) == 0 )
			else if ( strcmp( kDSNAttrRecordType, pAttrType ) == 0 )
			{
				// we simply use the input argument inRecType
				aTmpData->Clear();
				
				// Append the attribute name
				aTmpData->AppendShort( strlen( pAttrType ) );
				aTmpData->AppendString( pAttrType );

				if ( inAttrOnly == false )
				{
					// Append the attribute value count
					aTmpData->AppendShort( 1 );

					// Append attribute value
					aTmpData->AppendLong( strlen( inRecType ) );
					aTmpData->AppendString( inRecType );

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

			} // if ( strcmp( kDSNAttrRecordType, pAttrType ) == 0 )
			else if ((strcmp(pAttrType,kDSAttributesAll) == 0) || (strcmp(pAttrType,kDSAttributesStandardAll) == 0) || (strcmp(pAttrType,kDSAttributesNativeAll) == 0))
			{
				if ((strcmp(pAttrType,kDSAttributesAll) == 0) || (strcmp(pAttrType,kDSAttributesStandardAll) == 0))
				{
					//attrs always added here
					//kDSNAttrMetaNodeLocation
					//kDSNAttrRecordType
					
					// we look at kDSNAttrMetaNodeLocation with NO mapping
					// since we have special code to deal with it and we always place the
					// node name into it AND we output it here since ALL or ALL Std was asked for
					aTmpData->Clear();

					// Append the attribute name
					aTmpData->AppendShort( sizeof( kDSNAttrMetaNodeLocation ) - 1 );
					aTmpData->AppendString( kDSNAttrMetaNodeLocation );

					if ( inAttrOnly == false )
					{
						// Append the attribute value count
						aTmpData->AppendShort( 1 );

						char *tmpStr = nil;

						//extract name from the context data
						//need to add prefix of /LDAPv3/ here since the fName does not contain it in the context
						CLDAPNodeConfig *nodeConfig = inContext->fLDAPConnection->fNodeConfig;
						if ( nodeConfig != NULL && nodeConfig->fNodeName != nil )
						{
							tmpStr = (char *) calloc(1, sizeof(kLDAPv3NodePrefix) + strlen(nodeConfig->fNodeName));
							strcpy( tmpStr, kLDAPv3NodePrefix );
							strcat( tmpStr, nodeConfig->fNodeName );
						}
						else
						{
							tmpStr = (char *) calloc(1, sizeof(kLDAPUnknownNodeLocationStr));
							strcpy( tmpStr, kLDAPUnknownNodeLocationStr );
						}

						// Append attribute value
						aTmpData->AppendLong( strlen( tmpStr ) );
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
	
					// we simply use the input argument inRecType
					aTmpData->Clear();
					
					// Append the attribute name
					aTmpData->AppendShort( sizeof(kDSNAttrRecordType) - 1 );
					aTmpData->AppendString( kDSNAttrRecordType );

					if ( inAttrOnly == false )
					{
						// Append the attribute value count
						aTmpData->AppendShort( 1 );

						// Append attribute value
						aTmpData->AppendLong( strlen( inRecType ) );
						aTmpData->AppendString( inRecType );

					}// if ( inAttrOnly == false )
					else
					{
						aTmpData->AppendShort( 0 );
					}

					// Add the attribute length
					outCount++;
					inDataBuff->AppendLong( aTmpData->GetLength() );
					inDataBuff->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );

					// now if this is an automount, we also add the kDS1AttrMetaAutomountMap
					if (strcmp(inRecType, kDSStdRecordTypeAutomount) == 0)
					{
						aTmpData->Clear();
						
						// Append the attribute name
						aTmpData->AppendShort( sizeof(kDS1AttrMetaAutomountMap) - 1 );
						aTmpData->AppendString( kDS1AttrMetaAutomountMap );
						
						if ( inAttrOnly == false )
						{
							char	*mapName = CopyAutomountMapName( inContext, inResult );
							
							if (mapName != NULL)
							{
								// Append the attribute value count
								aTmpData->AppendShort( 1 );
								
								// Append attribute value
								aTmpData->AppendLong( strlen(mapName) );
								aTmpData->AppendString( mapName );
								
								free( mapName );
								mapName = NULL;
							}
							else
							{
								aTmpData->AppendShort( 0 );
							}
							
						}// if ( inAttrOnly == false )
						else
						{
							aTmpData->AppendShort( 0 );
						}
						
						// Add the attribute length
						outCount++;
						inDataBuff->AppendLong( aTmpData->GetLength() );
						inDataBuff->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
					}

					// Clear the temp block
					aTmpData->Clear();
						
					//plan is to output both standard and native attributes if request ALL ie. kDSAttributesAll
					// ie. all the attributes even if they are duplicated
					
					// std attributes go first
					numStdAttributes = 1;
					pStdAttrType = GetNextStdAttrType( inRecType, inContext, numStdAttributes );
					while ( pStdAttrType != nil )
					{
						//get the first mapping
						numAttributes = 1;
						pLDAPAttrType = MapAttrToLDAPType( inContext, (const char *)inRecType, pStdAttrType, numAttributes );
						//throw if first nil since no more will be found otherwise proceed until nil
											//can't throw since that wipes out retrieval of all the following requested attributes
						//if (pLDAPAttrType == nil ) throw( eDSInvalidAttrListRef); //KW would like a eDSNoMappingAvailable
						
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
									aTmp2Data->AppendShort( strlen( pStdAttrType ) );
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
	
								char *vsReturnStr = nil;
								vsReturnStr = MappingNativeVariableSubstitution( pLDAPAttrType, inContext, inResult, siResult );
								
								if (vsReturnStr != nil)
								{
									// we found a wildcard so reset the literalLength
									literalLength = strlen(vsReturnStr) - 1;
								}
								else
								{
									//if parsing error returned then we throw an error
									if (siResult != eDSNoErr) throw (siResult);
									// no wildcard found so get length of pLDAPAttrType
									literalLength = strlen(pLDAPAttrType) - 1;
								}
									
								if ( (inAttrOnly == false) && (literalLength > 0) )
								{
									valCount++;
										
									if (vsReturnStr != nil)
									{
										// If we found a wildcard then copy it here
										aTmpData->AppendLong( literalLength );
										aTmpData->AppendBlock( vsReturnStr + 1, literalLength );
									}
									else
									{
										// Append attribute value
										aTmpData->AppendLong( literalLength );
										aTmpData->AppendBlock( pLDAPAttrType + 1, literalLength );
									}
								} // if ( (inAttrOnly == false) && (strlen(pLDAPAttrType) > 1) ) 
								if (vsReturnStr != nil)
								{
									free( vsReturnStr );
									vsReturnStr = nil;
								}
							}
							else
							{
								aHost = inContext->fLDAPConnection->LockLDAPSession();
								if (aHost != NULL)
								{
									if ( (bValues = ldap_get_values_len(aHost, inResult, pLDAPAttrType)) != NULL )
									{
										bStripCryptPrefix = false;
										if (pLDAPPasswdAttrType != nil )
										{
											if ( strcmp( pStdAttrType, kDS1AttrPassword ) == 0 &&
												 strcasecmp( pLDAPAttrType, pLDAPPasswdAttrType ) == 0 )
											{
												//want to remove leading "{crypt}" prefix from password if it exists
												bStripCryptPrefix = true;
												//don't need to use the "********" passwdplus
												bUsePlus = false;
											}
										}
										// we only add marker if this is a user, group or computer, no use otherwise
										else if (strcmp(inRecType, kDSStdRecordTypeUsers) != 0 &&
												 strcmp(inRecType, kDSStdRecordTypeComputers) != 0 &&
												 strcmp(inRecType, kDSStdRecordTypeGroups) != 0)
										{
											bUsePlus = false;
										}
										
										//set the flag indicating that we got a match at least once
										bAtLeastOneTypeValid = true;
										//note that if standard type is incorrectly mapped ie. not found here
										//then the output will not contain any data on that std type
										if (!bTypeFound)
										{
											
												aTmp2Data->Clear();
												//use given type in the output NOT mapped to type
												aTmp2Data->AppendShort( strlen( pStdAttrType ) );
												aTmp2Data->AppendString( pStdAttrType );
												//set indicator so that multiple values from multiple mapped to types
												//can be added to the given type
												bTypeFound = true;
												
												//set attribute value count to zero
												valCount = 0;
												
												// Clear the temp block
												aTmpData->Clear();													
										}
		
										if ( inAttrOnly == false )
										{
											if (bStripCryptPrefix)
											{
												bStripCryptPrefix = false; //only attempted once here

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
													valCount++;
												} // for each bValues[i]
											}
											else
											{
												bool bIPAttribute = false;

												if ( strcmp(inRecType, kDSStdRecordTypeComputers) == 0 )
												{
													if ( pLDAPIPAddressAttrType != NULL && strcasecmp(pLDAPIPAddressAttrType, pLDAPAttrType) == 0 )
														bIPAttribute = true;
													if ( pLDAPIPv6AddressAttrType != NULL && strcasecmp(pLDAPIPAddressAttrType, pLDAPAttrType) == 0 )
														bIPAttribute = true;
												}
												
												if ( bIPAttribute )
												{
													char buffer[16];
													if ( strcmp(pStdAttrType, kDSNAttrIPv6Address) == 0 ) {
														// for each value of the attribute
														for (int i = 0; bValues[i] != NULL; i++ )
														{
															if ( inet_pton(AF_INET6, bValues[i]->bv_val, buffer) == 1 ) {
																// Append attribute value
																aTmpData->AppendLong( bValues[i]->bv_len );
																aTmpData->AppendBlock( bValues[i]->bv_val, bValues[i]->bv_len );
																valCount++;
																DbgLog( kLogInfo, "CLDAPv3Plugin: GetTheseAttributes:: appending value %s to ipv6", bValues[i]->bv_val );
															}
														} // for each bValues[i]
													}
													else if ( strcmp(pStdAttrType, kDSNAttrIPAddress) == 0 ) {
														// for each value of the attribute
														for (int i = 0; bValues[i] != NULL; i++ )
														{
															if ( inet_pton(AF_INET, bValues[i]->bv_val, buffer) == 1 ) {
																// Append attribute value
																aTmpData->AppendLong( bValues[i]->bv_len );
																aTmpData->AppendBlock( bValues[i]->bv_val, bValues[i]->bv_len );
																valCount++;
																DbgLog( kLogInfo, "CLDAPv3Plugin: GetTheseAttributes:: appending value %s to ipv4", bValues[i]->bv_val );
															}
														} // for each bValues[i]
													}
												}
												else
												{
													// for each value of the attribute
													for (int i = 0; bValues[i] != NULL; i++ )
													{
														// Append attribute value
														aTmpData->AppendLong( bValues[i]->bv_len );
														aTmpData->AppendBlock( bValues[i]->bv_val, bValues[i]->bv_len );
														valCount++;
													} // for each bValues[i]
												}
											}
										} // if ( inAttrOnly == false )
										
										ldap_value_free_len(bValues);
										bValues = NULL;
									} // ldap_get_values_len
									
									inContext->fLDAPConnection->UnlockLDAPSession( aHost, false );
								} //aHost != NULL
							}
							
							//cleanup pLDAPAttrType if needed
							DSFreeString( pLDAPAttrType );
							numAttributes++;
							//get the next mapping
							pLDAPAttrType = MapAttrToLDAPType( inContext, (const char *)inRecType, pStdAttrType, numAttributes );				
						} // while ( pLDAPAttrType != nil )
						
						if (bAtLeastOneTypeValid)
						{
							// Append the attribute value count
							aTmp2Data->AppendShort( valCount );
							
							if (valCount > 0)
							{
								DbgLog( kLogInfo, "CLDAPv3Plugin: GetTheseAttributes:: adding %s - %d values, blocklen = %d", pStdAttrType, valCount, aTmpData->GetLength() );

								// Add the attribute values to the attribute type
								aTmp2Data->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
								valCount = 0;
							}
							else
							{
								DbgLog( kLogInfo, "CLDAPv3Plugin: GetTheseAttributes:: adding %s - 0 values", pStdAttrType );
							}
	
							// Add the attribute data to the attribute data buffer
							outCount++;
							inDataBuff->AppendLong( aTmp2Data->GetLength() );
							inDataBuff->AppendBlock( aTmp2Data->GetData(), aTmp2Data->GetLength() );
							DbgLog( kLogInfo, "CLDAPv3Plugin: GetTheseAttributes:: appending attribute block %s - length = %d", pStdAttrType, aTmp2Data->GetLength() );
						}
						
						//cleanup pStdAttrType if needed
						DSFree( pStdAttrType );
						numStdAttributes++;
						//get the next std attribute
						pStdAttrType = GetNextStdAttrType( inRecType, inContext, numStdAttributes );
					}// while ( pStdAttrType != nil )
					
					if (bUsePlus)
					{
						// we add kDS1AttrPasswordPlus here
						aTmpData->Clear();
		
						// Append the attribute name
						aTmpData->AppendShort( sizeof( kDS1AttrPasswordPlus )-1 );
						aTmpData->AppendString( kDS1AttrPasswordPlus );
	
						if ( inAttrOnly == false )
						{
							// Append the attribute value count
							aTmpData->AppendShort( 1 );
	
							// Append attribute value
							aTmpData->AppendLong( sizeof(kDSValueNonCryptPasswordMarker)-1 );
							aTmpData->AppendString( kDSValueNonCryptPasswordMarker );
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
				if ((strcmp(pAttrType,kDSAttributesAll) == 0) || (strcmp(pAttrType,kDSAttributesNativeAll) == 0))
				{
					aHost = inContext->fLDAPConnection->LockLDAPSession();
					if (aHost != NULL)
					{
						//now we output the native attributes
						for (	pAttr = ldap_first_attribute (aHost, inResult, &ber );
								pAttr != NULL; pAttr = ldap_next_attribute(aHost, inResult, ber ) )
						{
							aTmpData->Clear();
									
							aTmpData->AppendShort( (sizeof(kDSNativeAttrTypePrefix)-1) + strlen( pAttr ) );
							aTmpData->AppendString( kDSNativeAttrTypePrefix );
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
						
							// Add the attribute length
							outCount++;
							inDataBuff->AppendLong( aTmpData->GetLength() );
							inDataBuff->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
		
							// Clear the temp block
							aTmpData->Clear();
							
							ldap_memfree( pAttr );
							pAttr = NULL;
						} // for ( loop over ldap_next_attribute )
					
						if (ber != nil)
						{
							ber_free( ber, 0 );
							ber = nil;
						}
						inContext->fLDAPConnection->UnlockLDAPSession( aHost, false );
					} //if aHost != NULL
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
				if (strcmp( kDS1AttrPasswordPlus, pAttrType ) == 0)
				{
					//want to remove leading "{crypt}" prefix from password if it exists
					bStripCryptPrefix = true;
					pLDAPAttrType = MapAttrToLDAPType( inContext, (const char *)inRecType, kDS1AttrPassword, numAttributes );
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
						aTmp2Data->AppendShort( strlen( pAttrType ) );
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
							aTmpData->AppendLong( sizeof(kDSValueNonCryptPasswordMarker) - 1 );
							aTmpData->AppendString( kDSValueNonCryptPasswordMarker );
						}
					}
				}
				else if (strcmp(inRecType, kDSStdRecordTypeAutomount) == 0 &&
						 strcmp(kDS1AttrMetaAutomountMap, pAttrType) == 0)
				{
					// now if this is an automount, we also add the kDS1AttrMetaAutomountMap
					aTmpData->Clear();
					
					// Append the attribute name
					aTmpData->AppendShort( sizeof(kDS1AttrMetaAutomountMap) - 1 );
					aTmpData->AppendString( kDS1AttrMetaAutomountMap );
					
					if ( inAttrOnly == false )
					{
						char	*mapName = CopyAutomountMapName( inContext, inResult );
						
						if (mapName != NULL)
						{
							// Append the attribute value count
							aTmpData->AppendShort( 1 );
							
							// Append attribute value
							aTmpData->AppendLong( strlen(mapName) );
							aTmpData->AppendString( mapName );
							
							free( mapName );
							mapName = NULL;
						}
						else
						{
							aTmpData->AppendShort( 0 );
						}
						
					}// if ( inAttrOnly == false )
					else
					{
						aTmpData->AppendShort( 0 );
					}
					
					// Add the attribute length
					outCount++;
					inDataBuff->AppendLong( aTmpData->GetLength() );
					inDataBuff->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
				}
				else if (strcmp(inRecType, kDSStdRecordTypeComputers) == 0 &&
						 strcmp(kDSNAttrIPv6Address, pAttrType) == 0)
				{
					
					aTmpData->Clear();
					aTmp2Data->Clear();
								
					// Append the attribute name
					aTmp2Data->AppendShort( strlen( pAttrType ) );
					aTmp2Data->AppendString( pAttrType );

					valCount = 0;
					
					if ( inAttrOnly == false )
					{
						char buffer[16];
					    char **vals;
						aHost = inContext->fLDAPConnection->LockLDAPSession();
						if (aHost != NULL)
						{
							//get the first mapping
							pLDAPAttrType = MapAttrToLDAPType( inContext,kDSStdRecordTypeComputers, kDSNAttrIPv6Address, numAttributes );
							if ( pLDAPAttrType != NULL )
							{
								if((vals = ldap_get_values(aHost, inResult,pLDAPAttrType)) != NULL) 
								{
									// for each value of the attribute
									for (int i = 0; vals[i] != NULL; i++ )
									{
										if(inet_pton(AF_INET6, vals[i], buffer) == 1 ) {
											// Append attribute value
											aTmpData->AppendLong( strlen(vals[i]) );
											aTmpData->AppendString( vals[i] );
											DbgLog( kLogInfo, "CLDAPv3Plugin: GetTheseAttributes:: appending value %s to ipv6", vals[i] );
											valCount++;
										}
									} // for each bValues[i]	
								}
								if (vals != NULL)
								{
									ldap_value_free(vals);
									vals = NULL;
								}
					
								DSFree( pLDAPAttrType );
							}
							inContext->fLDAPConnection->UnlockLDAPSession( aHost, false );
						}
						
					}// if ( inAttrOnly == false )
					
				}
				else if (strcmp(inRecType, kDSStdRecordTypeComputers) == 0 &&
						 strcmp(kDSNAttrIPAddress, pAttrType) == 0)
				{
					aTmpData->Clear();
					aTmp2Data->Clear();
					// Append the attribute name
					aTmp2Data->AppendShort( strlen( pAttrType ) );
					aTmp2Data->AppendString( pAttrType );
					valCount = 0;
					
					if ( inAttrOnly == false )
					{
						char buffer[16];
						char **vals;
						aHost = inContext->fLDAPConnection->LockLDAPSession();
						if (aHost != NULL)
						{
							//get the first mapping
							pLDAPAttrType = MapAttrToLDAPType( inContext,kDSStdRecordTypeComputers, kDSNAttrIPAddress, numAttributes );
							if ( pLDAPAttrType != NULL )
							{
								if((vals = ldap_get_values(aHost, inResult,pLDAPAttrType)) != NULL) 
								{
									// for each value of the attribute
									for (int i = 0; vals[i] != NULL; i++ )
									{
										if(inet_pton(AF_INET, vals[i], buffer) == 1 ) {
											// Append attribute value
											aTmpData->AppendLong( strlen(vals[i]) );
											aTmpData->AppendString( vals[i] );
											DbgLog( kLogInfo, "CLDAPv3Plugin: GetTheseAttributes:: appending value %s to ipv4",vals[i] );
											valCount++;
										}
									} // for each bValues[i]	
								}
								if (vals != NULL)
								{
									ldap_value_free(vals);
									vals = NULL;
								}
					
								DSFree( pLDAPAttrType );
							}
							inContext->fLDAPConnection->UnlockLDAPSession( aHost, false );
						}
						
					}// if ( inAttrOnly == false )
					
				}
				else
				{
					pLDAPAttrType = MapAttrToLDAPType( inContext, (const char *)inRecType, pAttrType, numAttributes );
					//throw if first nil since no more will be found otherwise proceed until nil
					// can't throw since that wipes out retrieval of all the following requested attributes
					//if (pLDAPAttrType == nil ) throw( eDSInvalidAttrListRef); //KW would like a eDSNoMappingAvailable

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
						aTmp2Data->AppendShort( strlen( pAttrType ) );
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

						char *vsReturnStr = nil;
						vsReturnStr = MappingNativeVariableSubstitution( pLDAPAttrType, inContext, inResult, siResult );
						
						if (vsReturnStr != nil)
						{
							// If we found a wildcard then set literalLength to the tmpStr
							literalLength = strlen(vsReturnStr) - 1;
						}
						else
						{
							//if parsing error returned then we throw an error
							if (siResult != eDSNoErr) throw (siResult);
							literalLength = strlen(pLDAPAttrType) - 1;
						}

						if ( (inAttrOnly == false) && (literalLength > 0) )
						{
							valCount++;
								
							if(vsReturnStr != nil)
							{
								// we found a wildcard then copy it here
								aTmpData->AppendLong( literalLength );
								aTmpData->AppendBlock( vsReturnStr + 1, literalLength );
							}
							else
							{
								// append attribute value
								aTmpData->AppendLong( literalLength );
								aTmpData->AppendBlock( pLDAPAttrType + 1, literalLength );
							}
						} // if ( (inAttrOnly == false) && (strlen(pLDAPAttrType) > 1) ) 
						if (vsReturnStr != nil)
						{
							free( vsReturnStr );
							vsReturnStr = nil;
						}
					}
					else
					{
						aHost = inContext->fLDAPConnection->LockLDAPSession();
						if (aHost != NULL)
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
										aTmpData->AppendLong( sizeof(kDSValueNonCryptPasswordMarker)-1 );
										aTmpData->AppendString( kDSValueNonCryptPasswordMarker );
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
							} // if (aHost != NULL) && ( inAttrOnly == false ) && bValues = ldap_get_values_len ...
							else if ( (valCount == 0) && (bValues == NULL) && bStripCryptPrefix)
							{
								valCount = 1;
								//no value found or returned for the mapped password attr
								// Append attribute value
								aTmpData->AppendLong( sizeof(kDSValueNonCryptPasswordMarker)-1 );
								aTmpData->AppendString( kDSValueNonCryptPasswordMarker );
							}
							if (bValues != NULL)
							{
								ldap_value_free_len(bValues);
								bValues = NULL;
							}
							inContext->fLDAPConnection->UnlockLDAPSession( aHost, false );
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
					pLDAPAttrType = MapAttrToLDAPType( inContext, (const char *)inRecType, pAttrType, numAttributes );				
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

	catch ( tDirStatus err )
	{
		siResult = err;
	}
	
	DSFree( pLDAPIPAddressAttrType );
	DSFree( pLDAPIPv6AddressAttrType );
	DSFree( pLDAPPasswdAttrType );
	DSDelete( aTmpData );
	DSDelete( aTmp2Data );

	return( siResult );

} // GetTheseAttributes


//------------------------------------------------------------------------------------
//	* GetRecordName
//------------------------------------------------------------------------------------

char *CLDAPv3Plugin::GetRecordName (	char			   *inRecType,
										LDAPMessage		   *inResult,
                                        sLDAPContextData   *inContext,
                                        tDirStatus		   &errResult )
{
	char		       *recName			= nil;
	char		       *pLDAPAttrType	= nil;
	struct berval	  **bValues;
	int					numAttributes	= 1;
	bool				bTypeFound		= false;
	LDAP			   *aHost			= nil;

	if ( inContext == nil ) return( NULL );
	
	try
	{
		errResult = eDSNoErr;
            
		//get the first mapping
		numAttributes = 1;
		pLDAPAttrType = MapAttrToLDAPType( inContext, (const char *)inRecType, kDSNAttrRecordName, numAttributes, true );
		//throw if first nil since no more will be found otherwise proceed until nil
		if ( pLDAPAttrType == nil ) throw( eDSInvalidAttrListRef ); //KW would like a eDSNoMappingAvailable
            
		//set indicator of multiple native to standard mappings
		bTypeFound = false;
		aHost = inContext->fLDAPConnection->LockLDAPSession();
		if ( aHost != NULL )
		{
			while ( (pLDAPAttrType != nil) && (!bTypeFound) )
			{
				if ( (bValues = ldap_get_values_len(aHost, inResult, pLDAPAttrType)) != NULL )
				{
					// for first value of the attribute
					recName = (char *) calloc(1, 1 + bValues[0]->bv_len);
					strcpy( recName, bValues[0]->bv_val );
					//we found a value so stop looking
					bTypeFound = true;
					ldap_value_free_len(bValues);
					bValues = NULL;
				} // if ( bValues = ldap_get_values_len ...)
							
				//cleanup pLDAPAttrType if needed
				DSFreeString( pLDAPAttrType );
				numAttributes++;
				//get the next mapping
				pLDAPAttrType = MapAttrToLDAPType( inContext, (const char *)inRecType, kDSNAttrRecordName, numAttributes, true );				
			} // while ( pLDAPAttrType != nil )
			inContext->fLDAPConnection->UnlockLDAPSession( aHost, false );
		}
		//cleanup pLDAPAttrType if needed
		DSFreeString( pLDAPAttrType );           
	} // try
	catch ( tDirStatus err )
	{
		errResult = err;
	}

	return( recName );

} // GetRecordName


// ---------------------------------------------------------------------------
//	* MapAttrListToLDAPTypeArray
// ---------------------------------------------------------------------------

char** CLDAPv3Plugin::MapAttrListToLDAPTypeArray( char *inRecType, CAttributeList *inAttrTypeList, sLDAPContextData *inContext,
												  char *inSearchAttr )
{
	char				  **outResult		= nil;
	char				  **mvResult		= nil;
	UInt32					uiStrLen		= 0;
	UInt32					uiNativeLen		= strlen( kDSNativeAttrTypePrefix );
	UInt32					uiStdLen		= strlen( kDSStdAttrTypePrefix );
	int						countNative		= 0;
	char				   *pAttrType		= nil;
	SInt32					attrTypeIndex	= 1;
	bool					bAddRecordName  = true;
	bool					bRecordNameGiven= false;
	bool					bCleanUp		= false;

	//TODO can we optimize allocs using a STL set and then creating the char** at the end
	//add in the recordname as the last entry if not already present or ALL not requested
	inAttrTypeList->GetAttribute(attrTypeIndex++, &pAttrType);
	while ( pAttrType != NULL || bAddRecordName )
	{
		if ( pAttrType == nil )
		{
			if ( bRecordNameGiven )
			{
				break;
			}
			else
			{
				bAddRecordName  = false;
				pAttrType		= (char *)kDSNAttrRecordName;
				bCleanUp		= true;
			}
		}
		//deal with the special requests for all attrs here
		//if any of kDSAttributesAll, kDSAttributesNativeAll, or kDSAttributesStandardAll
		//are found anywhere in the list then we retrieve everything in the ldap_search call
		else if (	( strcmp(pAttrType,kDSAttributesAll) == 0 ) ||
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
		
		uiStrLen = strlen( pAttrType );

		// First look for native attribute type
		if ( strncmp( pAttrType, kDSNativeAttrTypePrefix, uiNativeLen ) == 0 )
		{
			// Make sure we have data past the prefix
			if ( uiStrLen > uiNativeLen )
			{
				uiStrLen = uiStrLen - uiNativeLen;
				if (outResult != nil)
				{
					mvResult = outResult;
					outResult = (char **)calloc( countNative + 2, sizeof( char * ) );
					for (int oldIndex=0; oldIndex < countNative; oldIndex++) //transfer existing
					{
						outResult[oldIndex] = mvResult[oldIndex];
					}
				}
				else
				{
					outResult = (char **)calloc( 2, sizeof( char * ) );
				}
				outResult[countNative] = (char *) calloc(1, uiStrLen + 1 );
				strcpy( outResult[countNative], pAttrType + uiNativeLen );
				DbgLog( kLogPlugin, "CLDAPv3Plugin: MapAttrListToLDAPTypeArray:: Warning:Native attribute type <%s> is being used", outResult[countNative] );
				countNative++;
				if (mvResult != nil)
				{
					free(mvResult);
					mvResult = nil;
				}
			}
		} // native maps
		else if ( strncmp( pAttrType, kDSStdAttrTypePrefix, uiStdLen ) == 0 )
		{
			if ( strcmp( pAttrType, (char *)kDSNAttrRecordName ) == 0 )
			{
				bRecordNameGiven = true;
			}

			//TODO need to "try" to get a default here if no mappings
			//KW maybe NOT ie. directed open can work with native types
			
			char   *singleMap	= nil;
			int		aIndex		= 1;
			int		mapCount	= 0;
			int		usedMapCount= 0;

			//need to know number of native maps first to alloc the outResult
			mapCount = AttrMapsCount( inContext, inRecType, pAttrType );
			if (mapCount > 0)
			{
				if (outResult != nil)
				{
					mvResult = outResult;
					outResult = (char **)calloc( countNative + mapCount + 1, sizeof( char * ) );
					for (int oldIndex=0; oldIndex < countNative; oldIndex++) //transfer existing
					{
						outResult[oldIndex] = mvResult[oldIndex];
					}
				}
				else
				{
					outResult = (char **)calloc( mapCount + 1, sizeof( char * ) );
				}
				do
				{
					singleMap = ExtractAttrMap( inContext, inRecType, pAttrType, aIndex );
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
			outResult[countNative] = (char *) calloc(1, strlen( pAttrType ) + 1 );
			::strcpy( outResult[countNative], pAttrType );
			DbgLog( kLogPlugin, "CLDAPv3Plugin::MapAttrListToLDAPTypeArray: Warning:Native attribute type with no "
					"provided prefix <%s> is being used", outResult[countNative] );
			countNative++;
			if (mvResult != nil)
			{
				free(mvResult);
				mvResult = nil;
			}
		}// passthrough map
		if (bCleanUp)
		{
			pAttrType = nil;
		}
		else
		{
			pAttrType = nil;
			inAttrTypeList->GetAttribute(attrTypeIndex++, &pAttrType);
		}
		
		// if we have no type, let's try the search attribute too
		if ( pAttrType == NULL && DSIsStringEmpty(inSearchAttr) == false )
		{
			pAttrType = inSearchAttr;
			inSearchAttr = NULL;
		}
	}// while ( pAttrType != NULL || bAddRecordName )
	
	return( outResult );

} // MapAttrListToLDAPTypeArray


// ---------------------------------------------------------------------------
//	* MapLDAPWriteErrorToDS
//
//		- convert LDAP error to DS error
// ---------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::MapLDAPWriteErrorToDS ( SInt32 inLDAPError, tDirStatus inDefaultError )
{
	tDirStatus	siOutError	= eDSNoErr;

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
		case LDAP_INVALID_SYNTAX:
			siOutError = eDSSchemaError;
			break;
		case LDAP_SERVER_DOWN:
			siOutError = eDSCannotAccessSession;
			break;
		case LDAP_UNDEFINED_TYPE:
			siOutError = eDSInvalidAttributeType;
			break;
		case LDAP_INVALID_DN_SYNTAX:
			siOutError = eDSInvalidName;
			break;
			
		default:
		/*
		Remaining errors not yet mapped
		case LDAP_INAPPROPRIATE_MATCHING:
		case LDAP_TYPE_OR_VALUE_EXISTS:
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
	DbgLog( kLogPlugin, "CLDAPv3Plugin: error code %d returned by LDAP framework", inLDAPError );

	return( siOutError );

} // MapLDAPWriteErrorToDS


//------------------------------------------------------------------------------------
//	* GetAttributeEntry
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::GetAttributeEntry ( sGetAttributeEntry *inData )
{
	tDirStatus				siResult			= eDSNoErr;
	UInt16					usAttrTypeLen		= 0;
	UInt16					usAttrCnt			= 0;
	UInt32					usAttrLen			= 0;
	UInt16					usValueCnt			= 0;
	UInt32					usValueLen			= 0;
	UInt32					i					= 0;
	UInt32					uiIndex				= 0;
	UInt32					uiAttrEntrySize		= 0;
	UInt32					uiOffset			= 0;
	UInt32					uiTotalValueSize	= 0;
	UInt32					offset				= 4;
	UInt32					buffSize			= 0;
	UInt32					buffLen				= 0;
	char				   *p			   		= nil;
	char				   *pAttrType	   		= nil;
	tDataBufferPtr			pDataBuff			= nil;
	tAttributeEntryPtr		pAttribInfo			= nil;
	sLDAPContextData		   *pAttrContext		= nil;
	sLDAPContextData		   *pValueContext		= nil;

	try
	{
		if ( inData == nil ) throw( eMemoryError );

		pAttrContext = gLDAPContextTable->GetObjectForRefNum( inData->fInAttrListRef );
		if ( pAttrContext == nil ) throw( eDSBadContextData );

		uiIndex = inData->fInAttrInfoIndex;
		if (uiIndex == 0) throw( eDSInvalidIndex );
				
		pDataBuff = inData->fInOutDataBuff;
		if ( pDataBuff == nil ) throw( eDSNullDataBuff );
		
		buffSize	= pDataBuff->fBufferSize;
		//buffLen		= pDataBuff->fBufferLength;
		//here we can't use fBufferLength for the buffLen SINCE the buffer is packed at the END of the data block
		//and the fBufferLength is the overall length of the data for all blocks at the end of the data block
		//the value ALSO includes the bookkeeping data at the start of the data block
		//so we need to read it here

		p		= pDataBuff->fBufferData + pAttrContext->offset;
		offset	= pAttrContext->offset;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (offset + 2 > buffSize)  throw( eDSInvalidBuffFormat );
				
		// Get the attribute count
		memcpy( &usAttrCnt, p, 2 );
		if (uiIndex > usAttrCnt) throw( eDSIndexOutOfRange );

		// Move 2 bytes
		p		+= 2;
		offset	+= 2;

		// Skip to the attribute that we want
		for ( i = 1; i < uiIndex; i++ )
		{
			// Do record check, verify that offset is not past end of buffer, etc.
			if (offset + 4 > buffSize)  throw( eDSInvalidBuffFormat );
		
			// Get the length for the attribute
			memcpy( &usAttrLen, p, 4 );

			// Move the offset past the length word and the length of the data
			p		+= 4 + usAttrLen;
			offset	+= 4 + usAttrLen;
		}

		// Get the attribute offset
		uiOffset = offset;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (offset + 4 > buffSize)  throw( eDSInvalidBuffFormat );
		
		// Get the length for the attribute block
		memcpy( &usAttrLen, p, 4 );

		// Skip past the attribute length
		p		+= 4;
		offset	+= 4;

		//set the bufLen to stricter range
		buffLen = offset + usAttrLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (offset + 2 > buffLen)  throw( eDSInvalidBuffFormat );
		
		// Get the length for the attribute type
		memcpy( &usAttrTypeLen, p, 2 );
		
		pAttrType = p + 2;
		p		+= 2 + usAttrTypeLen;
		offset	+= 2 + usAttrTypeLen;
		
		// Do record check, verify that offset is not past end of buffer, etc.
		if (offset + 2 > buffLen)  throw( eDSInvalidBuffFormat );
		
		// Get number of values for this attribute
		memcpy( &usValueCnt, p, 2 );
		
		p		+= 2;
		offset	+= 2;
		
		for ( i = 0; i < usValueCnt; i++ )
		{
			// Do record check, verify that offset is not past end of buffer, etc.
			if (offset + 4 > buffLen)  throw( eDSInvalidBuffFormat );
		
			// Get the length for the value
			memcpy( &usValueLen, p, 4 );
			
			p		+= 4 + usValueLen;
			offset	+= 4 + usValueLen;
			
			uiTotalValueSize += usValueLen;
		}

		uiAttrEntrySize = sizeof( tAttributeEntry ) + usAttrTypeLen + kBuffPad;
		pAttribInfo = (tAttributeEntry *)calloc( 1, uiAttrEntrySize );

		pAttribInfo->fAttributeValueCount				= usValueCnt;
		pAttribInfo->fAttributeDataSize					= uiTotalValueSize;
		pAttribInfo->fAttributeValueMaxSize				= 512;				// KW this is not used anywhere
		pAttribInfo->fAttributeSignature.fBufferSize	= usAttrTypeLen + kBuffPad;
		pAttribInfo->fAttributeSignature.fBufferLength	= usAttrTypeLen;
		memcpy( pAttribInfo->fAttributeSignature.fBufferData, pAttrType, usAttrTypeLen );

		pValueContext = new sLDAPContextData( *pAttrContext );
		if ( pValueContext == nil ) throw( eMemoryAllocError );

		pValueContext->offset = uiOffset;

		gLDAPContextTable->AddObjectForRefNum( inData->fOutAttrValueListRef, pValueContext );

		inData->fOutAttrInfoPtr = pAttribInfo;
		DSRelease( pValueContext );
	}

	catch ( tDirStatus err )
	{
		siResult = err;
	}
	
	DSRelease( pAttrContext );

	return( siResult );

} // GetAttributeEntry


//------------------------------------------------------------------------------------
//	* GetAttributeValue
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::GetAttributeValue ( sGetAttributeValue *inData )
{
	tDirStatus					siResult		= eDSNoErr;
	UInt16						usValueCnt		= 0;
	UInt32						usValueLen		= 0;
	UInt16						usAttrNameLen	= 0;
	UInt32						i				= 0;
	UInt32						uiIndex			= 0;
	UInt32						offset			= 0;
	char					   *p				= nil;
	tDataBuffer				   *pDataBuff		= nil;
	tAttributeValueEntry	   *pAttrValue		= nil;
	sLDAPContextData		   *pValueContext	= nil;
	UInt32						buffSize		= 0;
	UInt32						buffLen			= 0;
	UInt32						attrLen			= 0;

	try
	{
		pValueContext = gLDAPContextTable->GetObjectForRefNum( inData->fInAttrValueListRef );
		if ( pValueContext == nil ) throw( eDSBadContextData );

		uiIndex = inData->fInAttrValueIndex;
		if (uiIndex == 0) throw( eDSInvalidIndex );
		
		pDataBuff = inData->fInOutDataBuff;
		if ( pDataBuff == nil ) throw( eDSNullDataBuff );

		buffSize	= pDataBuff->fBufferSize;
		//buffLen		= pDataBuff->fBufferLength;
		//here we can't use fBufferLength for the buffLen SINCE the buffer is packed at the END of the data block
		//and the fBufferLength is the overall length of the data for all blocks at the end of the data block
		//the value ALSO includes the bookkeeping data at the start of the data block
		//so we need to read it here

		p		= pDataBuff->fBufferData + pValueContext->offset;
		offset	= pValueContext->offset;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (offset + 4 > buffSize)  throw( eDSInvalidBuffFormat );
				
		// Get the buffer length
		memcpy( &attrLen, p, 4 );

		//now add the offset to the attr length for the value of buffLen to be used to check for buffer overruns
		//AND add the length of the buffer length var as stored ie. 4 bytes
		buffLen		= attrLen + pValueContext->offset + 4;
		if (buffLen > buffSize)  throw( eDSInvalidBuffFormat );

		// Skip past the attribute length
		p		+= 4;
		offset	+= 4;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (offset + 2 > buffLen)  throw( eDSInvalidBuffFormat );
		
		// Get the attribute name length
		memcpy( &usAttrNameLen, p, 2 );
		
		p		+= 2 + usAttrNameLen;
		offset	+= 2 + usAttrNameLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (offset + 2 > buffLen)  throw( eDSInvalidBuffFormat );
		
		// Get the value count
		memcpy( &usValueCnt, p, 2 );
		
		p		+= 2;
		offset	+= 2;

		if (uiIndex > usValueCnt)  throw( eDSIndexOutOfRange );

		// Skip to the value that we want
		for ( i = 1; i < uiIndex; i++ )
		{
			// Do record check, verify that offset is not past end of buffer, etc.
			if (offset + 4 > buffLen)  throw( eDSInvalidBuffFormat );
		
			// Get the length for the value
			memcpy( &usValueLen, p, 4 );
			
			p		+= 4 + usValueLen;
			offset	+= 4 + usValueLen;
		}

		// Do record check, verify that offset is not past end of buffer, etc.
		if (offset + 4 > buffLen)  throw( eDSInvalidBuffFormat );
		
		memcpy( &usValueLen, p, 4 );
		
		p		+= 4;
		offset	+= 4;

		//if (usValueLen == 0)  throw( eDSInvalidBuffFormat ); //if zero is it okay?

		pAttrValue = (tAttributeValueEntry *)calloc( 1, sizeof( tAttributeValueEntry ) + usValueLen + kBuffPad );

		pAttrValue->fAttributeValueData.fBufferSize		= usValueLen + kBuffPad;
		pAttrValue->fAttributeValueData.fBufferLength	= usValueLen;
		
		// Do record check, verify that offset is not past end of buffer, etc.
		if ( offset + usValueLen > buffLen )throw( eDSInvalidBuffFormat );
		
		memcpy( pAttrValue->fAttributeValueData.fBufferData, p, usValueLen );

		// Set the attribute value ID
		pAttrValue->fAttributeValueID = CalcCRC( pAttrValue->fAttributeValueData.fBufferData );

		inData->fOutAttrValue = pAttrValue;
			
	}

	catch ( tDirStatus err )
	{
		siResult = err;
	}
	
	DSRelease( pValueContext );

	return( siResult );

} // GetAttributeValue

//------------------------------------------------------------------------------------
//	* CheckAutomountNames
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::CheckAutomountNames(	char		*inRecType,
												char	   **inAttrValues,
												char	  ***outValues,
												char	  ***outMaps,
												UInt32		*outCount )
{
	char	  **aMaps		= NULL;
	char	  **aValues		= NULL;
	UInt32		iCount		= 0;
	tDirStatus	dsStatus	= eDSNoErr;
	
	// lets see if this is an automount type and also contains the record name since it could be special name
	if (inRecType != NULL && strcmp(inRecType, kDSStdRecordTypeAutomount) == 0)
	{
		for (iCount = 0; inAttrValues[iCount] != NULL; iCount++);
		
		if (iCount > 0)
		{
			aMaps = (char **) calloc( iCount + 1, sizeof(char *) );
			aValues = (char **) calloc( iCount + 1, sizeof(char *) );
			
			for ( UInt32 ii = 0; ii < iCount; ii++ )
			{
				char *pValue = inAttrValues[ii];
				
				// see if we have a separator, because it means this is a specific search, need to separate them out
				// otherwise, we fail with bad recordname?
				char *separator = strcasestr( pValue, ",automountMapName=" );
				if (separator != NULL)
				{
					int length = separator - pValue;
					char *automountName = dsCStrFromCharacters( pValue, length );
					
					aMaps[ii] = strdup( separator + (sizeof(",automountMapName=") - 1) );
					aValues[ii] = automountName;
				}
				else
				{
					dsStatus = eDSInvalidRecordName;
					break;
				}
			}
		}
	}

	if (iCount != 0 && dsStatus == eDSNoErr)
	{
		(*outMaps) = aMaps;
		(*outValues) = aValues;
		(*outCount) = iCount;
	}
	else
	{
		(*outMaps) = NULL;
		(*outValues) = NULL;
		(*outCount) = 0;
		
		DSFreeStringList( aMaps );
		DSFreeStringList( aValues );
	}
	
	return dsStatus;
} // CheckAutomountNames

//------------------------------------------------------------------------------------
//	* CopyAutomountMapName
//------------------------------------------------------------------------------------

char *CLDAPv3Plugin::CopyAutomountMapName(	sLDAPContextData	*inContext,
											LDAPMessage			*result )
{
	// get the "automountMap" and then parse out the 2nd component since that should be the automountMap
	// cn=mountName, automountMapName=theName, dc=example, dc=com
	char	*mapName	= nil;
	bool	failed		= false;
	
	if (inContext != NULL)
	{
		LDAP *aHost = inContext->fLDAPConnection->LockLDAPSession();
		if ( aHost != NULL )
		{
			//get the first mapping
			char *pMapAttrName = MapAttrToLDAPType( inContext,kDSStdRecordTypeAutomountMap, kDSNAttrRecordName, 1, true );
			if ( pMapAttrName != NULL )
			{
				char	*pDNStr	= ldap_get_dn( aHost, result );
				LDAPDN	tmpDN	= NULL;
				
				if ( pDNStr != NULL )
				{
					if (ldap_str2dn(pDNStr, &tmpDN, LDAP_DN_FORMAT_LDAPV3) == LDAP_SUCCESS)
					{
						for (int iRDN = 0; tmpDN[iRDN] != NULL; iRDN++) 
						{
							// see if we are at the automountMapName
							if (tmpDN[iRDN][0] != NULL && strcmp(tmpDN[iRDN][0]->la_attr.bv_val, pMapAttrName) == 0)
							{
								mapName = strdup( tmpDN[iRDN][0]->la_value.bv_val );
								break;
							}
						}
						
						ldap_dnfree( tmpDN );
					}
					
					free( pDNStr );
					pDNStr = NULL;
				}
				else
				{
					DbgLog( kLogPlugin, "CLDAPv3Plugin::CopyAutomountMapName(): bad ldap session, no DN." );
					failed = true;
				}
				
				free( pMapAttrName );
				pMapAttrName = NULL;
			}
			
			inContext->fLDAPConnection->UnlockLDAPSession( aHost, failed );
		}
	}
	
	return( mapName );
	
} // CopyAutomountMapName

bool CLDAPv3Plugin::DoesThisMatchAutomount(		sLDAPContextData   *inContext,
												UInt32				inCountMaps,
												char			  **inMaps,
												LDAPMessage		   *inResult )
{
	bool	bMatches = true; // default to true since we only stop if it's a map
	
	// if this is a search for an automount, we do some extra checking
	if (inCountMaps > 0)
	{
		// first look what automountMap it is under
		char *mapName = CopyAutomountMapName( inContext, inResult );
		if (mapName != NULL)
		{
			bMatches = false;
			
			for (UInt32 ii = 0; ii < inCountMaps; ii++)
			{
				// if this had a specific map, let's check the names now
				if (inMaps[ii] != NULL && strcmp(inMaps[ii], mapName) == 0)
				{
					// now we let the other matching rule deal with it
					bMatches = true;
					break;
				}
			}
			
			free( mapName );
			mapName = NULL;
		}
	}
	
	return bMatches;
} //DoesThisMatchAutomount

//------------------------------------------------------------------------------------
//	* GetTheseRecords
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::GetTheseRecords (
	char			   *inConstAttrType,
	char			  **inAttrNames,
	char			   *inRecType,
	char			   *inNativeRecType,
	tDirPatternMatch	patternMatch,
	CAttributeList	   *inAttrTypeList,
	sLDAPContextData   *inContext,
	sLDAPContinueData  *inContinue,
	bool				inAttrOnly,
	CBuff			   *inBuff,
	UInt32			   &outRecCount,
	bool				inbOCANDGroup,
	CFArrayRef			inOCSearchList,
	ber_int_t			inScope )
{
	tDirStatus			siResult			= eDSNoErr;
    SInt32				siValCnt			= 0;
    LDAPMessage			*result				= nil;
    char				*recName			= nil;
    char				*queryFilter		= nil;
	CDataBuff			*aRecData			= nil;
	CDataBuff			*aAttrData			= nil;
	bool				bFoundMatch			= false;
	char				**aAutomountMaps	= NULL;
	char				**aValues			= NULL;
	char				**checkMaps			= NULL;
	char				*aAutomountBase		= NULL;
	char				*pMapAttrName		= NULL;
	UInt32				iCountMaps			= 0;
	
	if ( inContext == NULL ) return( eDSInvalidContext );
	if ( inContinue == NULL ) return( eDSInvalidContinueData );
	
	outRecCount = 0; //need to track how many records were found by this call to GetTheseRecords
	
	// serialize search & result
	if ( inContext->fLDAPConnection->ConnectionStatus() == kConnectionUnsafe )
		return( eDSCannotAccessSession );
		
	// if this is an automount search, we have some special things to do
	if (inConstAttrType != NULL && inRecType != NULL && strcmp(inRecType, kDSStdRecordTypeAutomount) == 0)
	{
		// it this is the Meta attribute, then we know that we're looking for all mounts under this map
		if ( strcmp(inConstAttrType, kDS1AttrMetaAutomountMap) == 0 )
		{
			char *value = inAttrNames[0];
			
			// should always be 1 attribute value we are searching for, we ignore all others
			if ( value == NULL )
				return eDSEmptyPatternMatch;

			pMapAttrName = MapAttrToLDAPType( inContext, kDSStdRecordTypeAutomountMap, kDSNAttrRecordName, 1, true );
			if ( pMapAttrName == NULL )
				return eDSNoStdMappingAvailable;

			// now calculate the new length of our searchbase
			int length = strlen(pMapAttrName) + sizeof("=") + strlen(value) + strlen(inNativeRecType) + 1;
			aAutomountBase = (char *) calloc( length, sizeof(char) );
			
			// now generate the new searchbase
			snprintf( aAutomountBase, length, "%s=%s,%s", pMapAttrName, value, inNativeRecType );
			
			// nil out this value because we adjusted the search base and we are going to do the equivalent of search all
			inConstAttrType = nil;

			// we override the scope
			inScope = LDAP_SCOPE_ONELEVEL;
			inNativeRecType = aAutomountBase;
			patternMatch = eDSAnyMatch;
			
			DbgLog( kLogDebug, "CLDAPv3Plugin::GetTheseRecords - initiating automount search with searchbase \"%s\"", aAutomountBase );
		}
		// if this is for the record name, let's split out the name parts
		else if ( strcmp(inConstAttrType, kDSNAttrRecordName) == 0 && 
				  (siResult = CheckAutomountNames(inRecType, inAttrNames, &aValues, &aAutomountMaps, &iCountMaps)) == eDSNoErr )
		{
			inAttrNames = aValues;
			checkMaps = aAutomountMaps;
		}
		else if (siResult != eDSNoErr)
		{
			return siResult;
		}
	}

	// if we have no message ID, then we are initiating a lookup, so let's build the filter
	if ( inContinue->fLDAPMsgId == 0 )
	{
		if (inConstAttrType == nil)
		{
			queryFilter = BuildLDAPQueryFilter(	inContext, 
											    inConstAttrType,
											    nil,
											    eDSAnyMatch,
											    false,
											    (const char *)inRecType,
											    inNativeRecType,
											    inbOCANDGroup,
											    inOCSearchList );
		}
		else
		{
			//build the record query string
			queryFilter = BuildLDAPQueryMultiFilter( inConstAttrType,
													 inAttrNames,
													 patternMatch,
													 inContext,
													 false,
													 (const char *)inRecType,
													 inNativeRecType,
													 inbOCANDGroup,
													 inOCSearchList );
		}

		if ( queryFilter == nil )
		{
			DSFreeStringList( aAutomountMaps );
			DSFreeStringList( aValues );
			DSFreeString( aAutomountBase );
			return eDSNullParameter;
		}
	}
	    
	char **attrs = MapAttrListToLDAPTypeArray( inRecType, inAttrTypeList, inContext, inConstAttrType );
	
    try
    {
    	// check to make sure the queryFilter is not nil
    	
		aRecData = new CDataBuff();
		if ( aRecData == nil ) throw( eMemoryError );
		aAttrData = new CDataBuff();
		if ( aAttrData == nil ) throw( eMemoryError );
		
		do
		{
			siResult = DSInitiateOrContinueSearch( inContext, inContinue, inNativeRecType, attrs, inScope, queryFilter, &result );
			if ( siResult == eDSNoErr && result != NULL )
			{
				bFoundMatch = (	DoesThisMatchAutomount(inContext, iCountMaps, checkMaps, result) &&
							    DoAnyOfTheseAttributesMatch(inContext, inAttrNames, patternMatch, result) );
				
				if ( bFoundMatch )
				{
					aRecData->Clear();
					
					if ( inRecType != nil )
					{
						aRecData->AppendShort( strlen( inRecType ) );
						aRecData->AppendString( inRecType );
					}
					else
					{
						aRecData->AppendShort( sizeof(kLDAPRecordTypeUnknownStr) - 1 );
						aRecData->AppendString( kLDAPRecordTypeUnknownStr );
					}
					
					// need to get the record name
					recName = GetRecordName( inRecType, result, inContext, siResult );
					if ( siResult != eDSNoErr ) throw( siResult );
					if ( recName != nil )
					{
						aRecData->AppendShort( strlen( recName ) );
						aRecData->AppendString( recName );
						
						DSDelete ( recName );
					}
					else
					{
						aRecData->AppendShort( sizeof(kLDAPRecordNameUnknownStr) - 1 );
						aRecData->AppendString( kLDAPRecordNameUnknownStr );
					}
					
					// need to calculate the number of attribute types ie. siValCnt
					// also need to extract the attributes and place them into fAttrData
					//siValCnt = 0;
					
					aAttrData->Clear();
					siResult = GetTheseAttributes( inRecType, inAttrTypeList, result, inAttrOnly, inContext, siValCnt, aAttrData );
					if ( siResult != eDSNoErr ) throw( siResult );
					
					//add the attribute info to the aRecData
					aRecData->AppendShort( siValCnt );
					if ( siValCnt != 0 )
						aRecData->AppendBlock( aAttrData->GetData(), aAttrData->GetLength() );
					
					// add the aRecData now to the inBuff
					if ( inBuff->AddData(aRecData->GetData(), aRecData->GetLength()) == CBuff::kBuffFull )
					{
						siResult = eDSBufferTooSmall;
						inContinue->fResult = result;
						result = NULL;
						break;
					}
					
				} // DoTheseAttributesMatch?
				
				if ( result != NULL )
				{
					ldap_msgfree( result );
					result = NULL;
					
					if ( bFoundMatch )
					{
						outRecCount++; //another record added
						inContinue->fTotalRecCount++;
					}
				}
			}
			else if ( siResult == eDSRecordNotFound )
			{
				// error signifies we're done with those results
				siResult = eDSNoErr;
				break;
			}
		} while ( siResult == eDSNoErr && 
				  (inContinue->fLimitRecSearch == 0 || inContinue->fTotalRecCount < inContinue->fLimitRecSearch) );
		
    } // try block
    catch ( tDirStatus err )
    {
        siResult = err;
    }
	
	DSFreeStringList( attrs );
	DSDelete( queryFilter );
	DSDelete( aRecData );
	DSDelete( aAttrData );
	DSFreeStringList( aAutomountMaps );
	DSFreeStringList( aValues );
	DSFreeString( aAutomountBase );
	DSFreeString( pMapAttrName );
	
    return( siResult );

} // GetTheseRecords


//------------------------------------------------------------------------------------
//	* BuildLDAPQueryMultiFilter
//------------------------------------------------------------------------------------

char *CLDAPv3Plugin::BuildLDAPQueryMultiFilter
										(	char			   *inConstAttrType,
											char			  **inAttrNames,
											tDirPatternMatch	patternMatch,
											sLDAPContextData   *inContext,
											bool				useWellKnownRecType,
											const char		   *inRecType,
											char			   *inNativeRecType,
											bool				inbOCANDGroup,
											CFArrayRef			inOCSearchList )
{
    char				   *queryFilter			= nil;
	UInt32					matchType			= eDSExact;
	char				   *nativeAttrType		= nil;
	UInt32					recNameLen			= 0;
	int						numAttributes		= 1;
	CFMutableStringRef		cfStringRef			= nil;
	CFMutableStringRef		cfQueryStringRef	= nil;
	char				  **escapedStrings		= nil;
	UInt32					escapedIndex		= 0;
	UInt32					originalIndex		= 0;
	bool					bOnceThru			= false;
	UInt32					offset				= 3;
	UInt32					callocLength		= 0;
	bool					objClassAdded		= false;
	bool					bGetAllDueToLiteralMapping = false;
	int						aOCSearchListCount	= 0;
	char				  **attrValues			= inAttrNames;
	char				  **nonEscapedStrings	= NULL;
	
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
	
	//check for case of NO objectclass mapping BUT also no inAttrNames meaning we want to have
	//the result of (objectclass=*)
	if ( (CFStringGetLength(cfQueryStringRef) == 0) && (inAttrNames == nil) )
	{
		CFStringAppendCString(cfQueryStringRef,"(&(objectclass=*)", kCFStringEncodingUTF8);
		objClassAdded = true;
	}
	
	//here we decide if this is eDSCompoundExpression or eDSiCompoundExpression so that we special case this
	if (	((patternMatch == eDSCompoundExpression) ||
			(patternMatch == eDSiCompoundExpression)) && inAttrNames )
	{ //KW right now it is always case insensitive
		
		//if this is a multi-search, we will assume that inAttrNames[0] is the compound expression
		//multiple inAttrNames is not supported
		CFStringRef cfTempStringRef = inContext->fLDAPConnection->fNodeConfig->ParseCompoundExpression( inAttrNames[0], inRecType );
		
		if (cfTempStringRef != nil)
		{
			CFStringAppend(cfQueryStringRef, cfTempStringRef);
			DSCFRelease(cfTempStringRef);
		}
	}
	else
	{
		//first check to see if input not nil
		if (inAttrNames != nil)
		{
			UInt32 strCount = 0; //get number of strings to search for
			while(inAttrNames[strCount] != nil)
			{
				strCount++;
			}
			
			// if this is an automount and we are looking for a name, we need to do some extra work
			if (inRecType != NULL && strcmp(inRecType, kDSStdRecordTypeAutomount) == 0 && 
				inConstAttrType != NULL && strcmp(inConstAttrType, kDSNAttrRecordName) == 0)
			{
				nonEscapedStrings = (char **) calloc( strCount + 1, sizeof (char *) );
				attrValues = nonEscapedStrings;
			}
			
			escapedStrings = (char **) calloc( strCount +1, sizeof(char *) );
			strCount = 0;
			while(inAttrNames[strCount] != nil)
			{
				// here we just split out the name so the does the right thing if we have a list of nonEscapedStrings
				if (nonEscapedStrings != NULL)
				{
					// see if we have a separator, because it means this is a specific search
					char *attrValue = inAttrNames[strCount];
					char *separator = strcasestr( attrValue, ",automountMapName=" );
					if (separator != NULL)
					{
						int length = separator - attrValue;
						nonEscapedStrings[strCount] = dsCStrFromCharacters( attrValue, length );
					}
					else
					{
						nonEscapedStrings[strCount] = strdup( attrValue );
					}
				}
				
				recNameLen = strlen(attrValues[strCount]);
				escapedStrings[strCount] = (char *)calloc(1, 3 * recNameLen + 1);
				//assume at most all characters will be escaped
				escapedIndex	= 0;
				originalIndex   = 0;
				while (originalIndex < recNameLen)
				{
					switch (attrValues[strCount][originalIndex])
					{
						// add \ escape character then hex representation of original character
						case '*':
							escapedStrings[strCount][escapedIndex] = '\\';
							++escapedIndex;
							escapedStrings[strCount][escapedIndex] = '2';
							++escapedIndex;
							escapedStrings[strCount][escapedIndex] = 'a';
							++escapedIndex;                        
							break;
						case '(':
							escapedStrings[strCount][escapedIndex] = '\\';
							++escapedIndex;
							escapedStrings[strCount][escapedIndex] = '2';
							++escapedIndex;
							escapedStrings[strCount][escapedIndex] = '8';
							++escapedIndex;                        
							break;
						case ')':
							escapedStrings[strCount][escapedIndex] = '\\';
							++escapedIndex;
							escapedStrings[strCount][escapedIndex] = '2';
							++escapedIndex;
							escapedStrings[strCount][escapedIndex] = '9';
							++escapedIndex;                        
							break;
						case '\\':
							escapedStrings[strCount][escapedIndex] = '\\';
							++escapedIndex;
							escapedStrings[strCount][escapedIndex] = '5';
							++escapedIndex;
							escapedStrings[strCount][escapedIndex] = 'c';
							++escapedIndex;                        
							break;
						default:
							escapedStrings[strCount][escapedIndex] = attrValues[strCount][originalIndex];
							++escapedIndex;
							break;
					}//switch (attrValues[strCount][originalIndex])
					++originalIndex;
				}//while (originalIndex < recNameLen)
				strCount++;
			}//while(inAttrNames[strCount] != nil)

			//assume that the query is "OR" based ie. meet any of the criteria
			cfStringRef = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, CFSTR("(|"));
			
			//get the first mapping
			numAttributes = 1;
			//KW Note that kDS1RecordName is single valued so we are using kDSNAttrRecordName
			//as a multi-mapped std type which will easily lead to multiple values
			nativeAttrType = MapAttrToLDAPType( inContext, inRecType, inConstAttrType, numAttributes, false );

			//would throw if first nil since no more will be found otherwise proceed until nil
			//however simply set to default LDAP native in this case
			//ie. we are trying regardless if kDSNAttrRecordName is mapped or not
			//whether or not "cn" is a good choice is a different story
			if (nativeAttrType == nil ) 
			{
				if ( inConstAttrType == nil ) // only if we had no attribute
					nativeAttrType = strdup("cn");
				else
					goto failed;
			}
	
			matchType = (UInt32) (patternMatch);
			while ( nativeAttrType != nil )
			{
				if (nativeAttrType[0] == '#') //literal mapping
				{
					if (strlen(nativeAttrType) > 1)
					{
						if (DoAnyMatch((const char *)(nativeAttrType+1), escapedStrings, patternMatch))
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
					//the multiple values are OR'ed in this search
					CFStringAppendCString(cfStringRef,"(|", kCFStringEncodingUTF8);
					strCount = 0;
					//for each search pattern to match to
					while(escapedStrings[strCount] != nil)
					{
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
								CFStringAppendCString(cfStringRef, escapedStrings[strCount], kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef,"*)", kCFStringEncodingUTF8);
								bOnceThru = true;
								break;
							case eDSEndsWith:
							case eDSiEndsWith:
								CFStringAppendCString(cfStringRef,"(", kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef, nativeAttrType, kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef,"=*", kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef, escapedStrings[strCount], kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef,")", kCFStringEncodingUTF8);
								bOnceThru = true;
								break;
							case eDSContains:
							case eDSiContains:
								CFStringAppendCString(cfStringRef,"(", kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef, nativeAttrType, kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef,"=*", kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef, escapedStrings[strCount], kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef,"*)", kCFStringEncodingUTF8);
								bOnceThru = true;
								break;
							case eDSWildCardPattern:
							case eDSiWildCardPattern:
								//assume the inConstAttrName is wild
								CFStringAppendCString(cfStringRef,"(", kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef, nativeAttrType, kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef,"=", kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef, attrValues[strCount], kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef,")", kCFStringEncodingUTF8);
								bOnceThru = true;
								break;
							case eDSRegularExpression:
							case eDSiRegularExpression:
								//assume inConstAttrName replaces entire wild expression
								CFStringAppendCString(cfStringRef, attrValues[strCount], kCFStringEncodingUTF8);
								bOnceThru = true;
								break;
							case eDSExact:
							case eDSiExact:
							default:
								CFStringAppendCString(cfStringRef,"(", kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef, nativeAttrType, kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef,"=", kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef, escapedStrings[strCount], kCFStringEncodingUTF8);
								CFStringAppendCString(cfStringRef,")", kCFStringEncodingUTF8);
								bOnceThru = true;
								break;
						} // switch on matchType
						strCount++;
					}//while(escapedStrings[strCount] != nil)
					//the multiple values are OR'ed in this search
					CFStringAppendCString(cfStringRef,")", kCFStringEncodingUTF8);
				}
				//cleanup nativeAttrType if needed
				if (nativeAttrType != nil)
				{
					free(nativeAttrType);
					nativeAttrType = nil;
				}
				numAttributes++;
				//get the next mapping
				nativeAttrType = MapAttrToLDAPType( inContext,inRecType, inConstAttrType, numAttributes, false );
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

			if (escapedStrings != nil)
			{
				strCount = 0;
				while(escapedStrings[strCount] != nil)
				{
					free(escapedStrings[strCount]);
					escapedStrings[strCount] = nil;
					strCount++;
				}
				free(escapedStrings);
				escapedStrings = nil;
			}
	
		} // if (inAttrNames != nil)
	}
	if (objClassAdded)
	{
		CFStringAppendCString(cfQueryStringRef,")", kCFStringEncodingUTF8);
	}
	
	//here we make the char * output in queryfilter
	if (CFStringGetLength(cfQueryStringRef) != 0)
	{
		callocLength = (UInt32) CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfQueryStringRef), kCFStringEncodingUTF8) + 1;
		queryFilter = (char *) calloc(1, callocLength);
		CFStringGetCString( cfQueryStringRef, queryFilter, callocLength, kCFStringEncodingUTF8 );
	}

failed:
	
	DSCFRelease( cfQueryStringRef );
	DSFreeStringList( nonEscapedStrings );
	
	return (queryFilter);
	
} // BuildLDAPQueryMultiFilter


//------------------------------------------------------------------------------------
//	* GetDirNodeInfo
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::GetDirNodeInfo ( sGetDirNodeInfo *inData )
{
	tDirStatus			siResult		= eDSNoErr;
	UInt32				uiOffset		= 0;
	UInt32				uiCntr			= 1;
	UInt32				uiAttrCnt		= 0;
	CAttributeList	   *inAttrList		= nil;
	char			   *pAttrName		= nil;
	char			   *pData			= nil;
	sLDAPContextData   *pContext		= nil;
	sLDAPContextData   *pAttrContext	= nil;
	CBuff				outBuff;
	CDataBuff		   *aRecData		= nil;
	CDataBuff		   *aAttrData		= nil;
	CDataBuff		   *aTmpData		= nil;

// Can extract here the following:
// kDSAttributesAll
// kDSNAttrNodePath
// kDS1AttrReadOnlyNode
// kDSNAttrAuthMethod
// dsAttrTypeStandard:AcountName
// kDSNAttrDefaultLDAPPaths
// kDS1AttrDistinguishedName
// kDS1AttrLDAPSearchBaseSuffix
//KW need to add mappings info next

	try
	{
		if ( inData == nil ) throw( eMemoryError );

		pContext = gLDAPContextTable->GetObjectForRefNum( inData->fInNodeRef );
		if ( pContext == nil ) throw( eDSBadContextData );

		inAttrList = new CAttributeList( inData->fInDirNodeInfoTypeList );
		if ( inAttrList == nil ) throw( eDSNullNodeInfoTypeList );
		if (inAttrList->GetCount() == 0) throw( eDSEmptyNodeInfoTypeList );

		siResult = (tDirStatus)outBuff.Initialize( inData->fOutDataBuff, true );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = (tDirStatus)outBuff.SetBuffType( 'StdA' );
		if ( siResult != eDSNoErr ) throw( siResult );

		aRecData = new CDataBuff();
		if ( aRecData == nil ) throw( eMemoryError );
		aAttrData = new CDataBuff();
		if ( aAttrData == nil ) throw( eMemoryError );
		aTmpData = new CDataBuff();
		if ( aTmpData == nil ) throw( eMemoryError );
		
		// Set the record name and type
		aRecData->AppendShort( sizeof(kDSStdRecordTypeDirectoryNodeInfo)-1 );
		aRecData->AppendString( kDSStdRecordTypeDirectoryNodeInfo );
		aRecData->AppendShort( sizeof( "DirectoryNodeInfo" )-1 );
		aRecData->AppendString( "DirectoryNodeInfo" );

		CLDAPNodeConfig *nodeConfig = pContext->fLDAPConnection->fNodeConfig;
		while ( inAttrList->GetAttribute( uiCntr++, &pAttrName ) == eDSNoErr )
		{
			//package up all the dir node attributes dependant upon what was asked for
			if ((strcmp( pAttrName, kDSAttributesAll ) == 0) ||
				(strcmp( pAttrName, kDSNAttrNodePath ) == 0) )
			{
				aTmpData->Clear();
				
				uiAttrCnt++;
			
				// Append the attribute name
				aTmpData->AppendShort( sizeof( kDSNAttrNodePath )-1 );
				aTmpData->AppendString( kDSNAttrNodePath );

				if ( inData->fInAttrInfoOnly == false )
				{
					// Attribute value count always two
					aTmpData->AppendShort( 2 );
					
					// Append attribute value
					aTmpData->AppendLong( sizeof( "LDAPv3" )-1 );
					aTmpData->AppendString( "LDAPv3" );

					char *tmpStr = kLDAPUnknownNodeLocationStr;
					
					//don't need to retrieve for the case of "generic unknown" so don't check index 0
					// simply always use the pContext->fNodeName since case of registered it is identical to
					if (nodeConfig == NULL )
						tmpStr = "LDAPv3 Configure";
					else if (nodeConfig->fNodeName != nil)
						tmpStr = nodeConfig->fNodeName;
					
					// Append attribute value
					aTmpData->AppendLong( strlen( tmpStr ) );
					aTmpData->AppendString( tmpStr );

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
			
			if ( (strcmp( pAttrName, kDSAttributesAll ) == 0) || 
				 (strcmp( pAttrName, kDS1AttrReadOnlyNode ) == 0) )
			{
				aTmpData->Clear();

				uiAttrCnt++;

				// Append the attribute name
				aTmpData->AppendShort( sizeof( kDS1AttrReadOnlyNode )-1 );
				aTmpData->AppendString( kDS1AttrReadOnlyNode );

				if ( inData->fInAttrInfoOnly == false )
				{
					// Attribute value count
					aTmpData->AppendShort( 1 );

					//possible for a node to be ReadOnly, ReadWrite, WriteOnly
					//note that ReadWrite does not imply fully readable or writable
					
					if (nodeConfig == NULL || nodeConfig->fLDAPv2ReadOnly)
					{
						aTmpData->AppendLong( sizeof("ReadOnly") - 1 );
						aTmpData->AppendString( "ReadOnly" );
					}
					else
					{
						aTmpData->AppendLong( sizeof("ReadWrite") - 1 );
						aTmpData->AppendString( "ReadWrite" );
					}
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
				 
			if ( strcmp(pAttrName, kDSAttributesAll) == 0 || strcmp(pAttrName, kDSNAttrAuthMethod) == 0 )
			{
				aTmpData->Clear();
				
				uiAttrCnt++;
			
				//KW at some point need to retrieve SASL auth methods from LDAP server if they are available
			
				// Append the attribute name
				aTmpData->AppendShort( sizeof( kDSNAttrAuthMethod )-1 );
				aTmpData->AppendString( kDSNAttrAuthMethod );

				if ( inData->fInAttrInfoOnly == false && nodeConfig != NULL )
				{					
					// Attribute value count
					aTmpData->AppendShort( 5 );
					
					// Append first attribute value
					aTmpData->AppendLong( sizeof(kDSStdAuthCrypt) - 1 );
					aTmpData->AppendString( kDSStdAuthCrypt );

					aTmpData->AppendLong( sizeof(kDSStdAuthClearText) - 1 );
					aTmpData->AppendString( kDSStdAuthClearText );

					aTmpData->AppendLong( sizeof(kDSStdAuthNodeNativeClearTextOK) - 1 );
					aTmpData->AppendString( kDSStdAuthNodeNativeClearTextOK );
					
					aTmpData->AppendLong( sizeof(kDSStdAuthNodeNativeNoClearText) - 1 );
					aTmpData->AppendString( kDSStdAuthNodeNativeNoClearText );
					
					aTmpData->AppendLong( sizeof(kDSStdAuthKerberosTickets) - 1 );
					aTmpData->AppendString( kDSStdAuthKerberosTickets );
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

			if ((strcmp( pAttrName, kDSAttributesAll ) == 0) ||
				(strcmp( pAttrName, "dsAttrTypeStandard:AccountName" ) == 0) )
			{
				aTmpData->Clear();
				
				uiAttrCnt++;
			
				// Append the attribute name
				aTmpData->AppendShort( sizeof( "dsAttrTypeStandard:AccountName" )-1 );
				aTmpData->AppendString( "dsAttrTypeStandard:AccountName" );

				if ( inData->fInAttrInfoOnly == false )
				{
					// Attribute value count
					aTmpData->AppendShort( 1 );

					// a context cannot change while while in a call already
					if ( DSIsStringEmpty(pContext->fLDAPConnection->fLDAPUsername) == false )
					{
						// Append attribute value
						aTmpData->AppendLong( strlen(pContext->fLDAPConnection->fLDAPUsername) );
						aTmpData->AppendString( pContext->fLDAPConnection->fLDAPUsername );
					}
					else
					{
						// Append attribute value
						aTmpData->AppendLong( sizeof("No Account Name") - 1 );
						aTmpData->AppendString( "No Account Name" );
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
			
			} // kDSAttributesAll or dsAttrTypeStandard:AccountName

			if ( strcmp( pAttrName, kDSNAttrDefaultLDAPPaths ) == 0 )
			{
				aTmpData->Clear();
				
				uiAttrCnt++;
			
				// Append the attribute name
				aTmpData->AppendShort( sizeof( kDSNAttrDefaultLDAPPaths )-1 );
				aTmpData->AppendString( kDSNAttrDefaultLDAPPaths );

				if ( inData->fInAttrInfoOnly == false )
				{
					char **defaultLDAPNodes = nil;
					UInt32 aDefaultLDAPNodeCount = 0;
					defaultLDAPNodes = fConfigFromXML->GetDHCPBasedLDAPNodes( &aDefaultLDAPNodeCount );
					
					// Attribute value count
					aTmpData->AppendShort( aDefaultLDAPNodeCount );

					if ( (aDefaultLDAPNodeCount > 0) && (defaultLDAPNodes != nil) )
					{
						int listIndex = 0;
						for (listIndex=0; defaultLDAPNodes[listIndex] != nil; listIndex++)
						{
							// Append attribute value
							aTmpData->AppendLong( strlen( defaultLDAPNodes[listIndex] ) );
							aTmpData->AppendString( defaultLDAPNodes[listIndex] );
						}
					}
					DSFreeStringList(defaultLDAPNodes);
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

			if ((strcmp( pAttrName, kDSAttributesAll ) == 0) ||
				(strcmp( pAttrName, kDS1AttrDistinguishedName ) == 0) )
			{
				aTmpData->Clear();
				
				uiAttrCnt++;
			
				// Append the attribute name
				aTmpData->AppendShort( sizeof( kDS1AttrDistinguishedName )-1 );
				aTmpData->AppendString( kDS1AttrDistinguishedName );

				if ( inData->fInAttrInfoOnly == false )
				{
					char	*tmpStr;
					
					if ( nodeConfig != NULL && (tmpStr = nodeConfig->CopyUIName()) != NULL )
					{
						// Attribute value count
						aTmpData->AppendShort( 1 );
						
						// Append attribute value
						aTmpData->AppendLong( strlen( tmpStr ) );
						aTmpData->AppendString( tmpStr );
						
						DSFree( tmpStr );
					}
					else
					{
						aTmpData->AppendShort( 0 );
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
			
			} // kDSAttributesAll or kDS1AttrDistinguishedName

			if ( nodeConfig != NULL && 
				 ((strcmp( pAttrName, kDSAttributesAll ) == 0) || (strcmp( pAttrName, kDSNAttrRecordType ) == 0)) )
			{
				CFDictionaryRef cfNormalizedMap = nodeConfig->CopyNormalizedMappings();
				if ( cfNormalizedMap != NULL )
				{
					aTmpData->Clear();
					
					uiAttrCnt++;
					
					// Append the attribute name
					aTmpData->AppendShort( sizeof( kDSNAttrRecordType )-1 );
					aTmpData->AppendString( kDSNAttrRecordType );
					
					if ( inData->fInAttrInfoOnly == false )
					{
						CFIndex valueCount = CFDictionaryGetCount( cfNormalizedMap );
						
						aTmpData->AppendShort( valueCount );
						
						if( valueCount != 0 )
						{
							CFStringRef *keys = (CFStringRef *)calloc( valueCount, sizeof(CFStringRef) );
							
							CFDictionaryGetKeysAndValues( cfNormalizedMap, (const void **)keys, NULL );
							for ( CFIndex i = 0; i < valueCount; i++ )
							{
								char	tempBuffer[1024];
								
								// just get the string in to the temp buffer and put it in the list
								CFStringGetCString( keys[i], tempBuffer, 1024, kCFStringEncodingUTF8 );
								
								aTmpData->AppendLong( strlen( tempBuffer ) );
								aTmpData->AppendString( tempBuffer );
							}
							
							DSFree( keys );
						}
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
													   
				   DSCFRelease( cfNormalizedMap );
			   }
			}
			
			if ( nodeConfig != NULL &&
				 (strcmp(pAttrName, kDSAttributesAll) == 0 || strcmp(pAttrName, "dsAttrTypeStandard:TrustInformation") == 0) )
			{
				aTmpData->Clear();
				
				uiAttrCnt++;
				
				// Append the attribute name
				aTmpData->AppendShort( sizeof( "dsAttrTypeStandard:TrustInformation" )-1 );
				aTmpData->AppendString( "dsAttrTypeStandard:TrustInformation" );
				
				if ( inData->fInAttrInfoOnly == false )
				{
					// use sets of true & false, don't rely on compiler setting 1 for comparisons
					
					bool bAuthenticated = nodeConfig->fSecureUse;
					bool bAnonymous = (bAuthenticated ? false : true); // we can't do ! because we need a 1 or 0
					bool bFullTrust = false;
					bool bPartialTrust = false;
					bool bDHCP = nodeConfig->fDHCPLDAPServer;
					bool bEncryption = (nodeConfig->fIsSSL || (nodeConfig->fSecurityLevel & kSecPacketEncryption));
					
					if( bAuthenticated )
					{
						bFullTrust = ((nodeConfig->fSecurityLevel & (kSecManInMiddle | kSecPacketSigning)) == (kSecManInMiddle | kSecPacketSigning));
						
						// let's see if we have a partial trust flags in place..
						if( bFullTrust == false )
						{
							bPartialTrust = ((nodeConfig->fSecurityLevel & kSecManInMiddle) == kSecManInMiddle);
						}
					}
					
					aTmpData->AppendShort( bAuthenticated + bAnonymous + bFullTrust + bPartialTrust + bDHCP + bEncryption );
					
					if( bAuthenticated )
					{
						aTmpData->AppendLong( sizeof( "Authenticated" )-1 );
						aTmpData->AppendString( "Authenticated" );
					}
					else if( bAnonymous )
					{
						aTmpData->AppendLong( sizeof( "Anonymous" )-1 );
						aTmpData->AppendString( "Anonymous" );
					}

					if( bFullTrust )
					{
						aTmpData->AppendLong( sizeof( "FullTrust" )-1 );
						aTmpData->AppendString( "FullTrust" );
					}
					else if( bPartialTrust )
					{
						aTmpData->AppendLong( sizeof( "PartialTrust" )-1 );
						aTmpData->AppendString( "PartialTrust" );
					}
					
					if( bEncryption )
					{
						aTmpData->AppendLong( sizeof( "Encryption" )-1 );
						aTmpData->AppendString( "Encryption" );
					}

					if( bDHCP )
					{
						aTmpData->AppendLong( sizeof( "DHCP" )-1 );
						aTmpData->AppendString( "DHCP" );
					}
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
			}// kDSAttributesAll or dsAttrTypeStandard:TrustInformation
			
			if ( nodeConfig != NULL && 
				 (strcmp(pAttrName, kDSAttributesAll) == 0 || strcmp(pAttrName, kDS1AttrLDAPSearchBaseSuffix) == 0) )
			{
				aTmpData->Clear();
				
				uiAttrCnt++;
			
				// Append the attribute name
				aTmpData->AppendShort( sizeof( kDS1AttrLDAPSearchBaseSuffix )-1 );
				aTmpData->AppendString( kDS1AttrLDAPSearchBaseSuffix );

				if ( inData->fInAttrInfoOnly == false )
				{
					char *tmpStr = nodeConfig->CopyMapSearchBase();
					if ( tmpStr != NULL )
					{
						aTmpData->AppendShort( 1 );
						
						// Append attribute value
						aTmpData->AppendLong( strlen(tmpStr) );
						aTmpData->AppendString( tmpStr );
					}
					else
					{
						aTmpData->AppendShort( 0 );
					}
					
					DSFree( tmpStr );
				} // fInAttrInfoOnly is false
				else
				{
					aTmpData->AppendShort( 0 );
				}
				
				// Add the attribute length
				aAttrData->AppendLong( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
                aTmpData->Clear();
			
			} // kDSAttributesAll or kDS1AttrLDAPSearchBaseSuffix

			if ( pContext->fLDAPConnection != NULL && 
				(strcmp(pAttrName, kDSAttributesAll) == 0 || strcmp(pAttrName, "dsAttrTypeStandard:ServerConnection") == 0) )
			{
				aTmpData->Clear();
				
				uiAttrCnt++;
				
				// Append the attribute name
				aTmpData->AppendShort( sizeof( "dsAttrTypeStandard:ServerConnection" )-1 );
				aTmpData->AppendString( "dsAttrTypeStandard:ServerConnection" );
				
				if ( inData->fInAttrInfoOnly == false )
				{
					char *tmpStr = pContext->fLDAPConnection->CopyReplicaIPAddress();
					if ( tmpStr != NULL )
					{
						aTmpData->AppendShort( 1 );
						
						// Append attribute value
						aTmpData->AppendLong( strlen(tmpStr) );
						aTmpData->AppendString( tmpStr );
					}
					else
					{
						aTmpData->AppendShort( 0 );
					}
					
					DSFree( tmpStr );
				} // fInAttrInfoOnly is false
				else
				{
					aTmpData->AppendShort( 0 );
				}
				
				// Add the attribute length
				aAttrData->AppendLong( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
                aTmpData->Clear();
				
			} // kDSAttributesAll or "dsAttrTypeStandard:ServerConnection"
			
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
			pAttrContext = new sLDAPContextData( *pContext );
			if ( pAttrContext == nil ) throw( eMemoryAllocError );
			
		//add to the offset for the attr list the length of the GetDirNodeInfo fixed record labels
//		record length = 4
//		aRecData->AppendShort( strlen( kDSStdRecordTypeDirectoryNodeInfo ) ); = 2
//		aRecData->AppendString( kDSStdRecordTypeDirectoryNodeInfo ); = 35
//		aRecData->AppendShort( strlen( "DirectoryNodeInfo" ) ); = 2
//		aRecData->AppendString( "DirectoryNodeInfo" ); = 17
//		total adjustment = 4 + 2 + 35 + 2 + 17 = 60

			pAttrContext->offset = uiOffset + 60;

			gLDAPContextTable->AddObjectForRefNum( inData->fOutAttrListRef, pAttrContext );
			DSRelease( pAttrContext );
		}
        else
        {
            siResult = eDSBufferTooSmall;
        }
	}

	catch ( tDirStatus err )
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
	
	DSRelease( pContext );

	return( siResult );

} // GetDirNodeInfo

//------------------------------------------------------------------------------------
//	* OpenRecord
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::OpenRecord ( sOpenRecord *inData )
{
	tDirStatus			siResult		= eDSNoErr;
	tDataNodePtr		pRecName		= nil;
	tDataNodePtr		pRecType		= nil;
	char			   *pLDAPSearchBase	= nil;
	sLDAPContextData   *pContext		= nil;
	sLDAPContextData   *pRecContext		= nil;
    char			   *queryFilter		= nil;
	LDAPMessage		   *result			= nil;
	int					numRecTypes		= 1;
	bool				bResultFound	= false;
	bool				bOCANDGroup		= false;
	CFArrayRef			OCSearchList	= nil;
	ber_int_t			scope			= LDAP_SCOPE_SUBTREE;
	tDirStatus			searchResult	= eDSNoErr;
	char				**attrs			= nil;
	char				*ldapDN			= NULL;
	
	pRecType = inData->fInRecType;
	if ( pRecType == nil ) return( eDSNullRecType );

	pRecName = inData->fInRecName;
	if ( pRecName == nil ) return( eDSNullRecName );
	
	if (pRecName->fBufferLength == 0) return( eDSEmptyRecordNameList );
	if (pRecType->fBufferLength == 0) return( eDSEmptyRecordTypeList );

	pContext = gLDAPContextTable->GetObjectForRefNum( inData->fInNodeRef );
	if ( pContext == nil ) return eDSBadContextData;

	if ( pContext->fLDAPConnection->ConnectionStatus() != kConnectionSafe ) return eDSCannotAccessSession;

	//only throw this for first time since we need at least one map
	pLDAPSearchBase = MapRecToSearchBase( pContext, (const char *)(pRecType->fBufferData), numRecTypes, &bOCANDGroup, &OCSearchList, &scope );
	if ( pLDAPSearchBase == nil ) return eDSInvalidRecordType;
	
	//let us only ask for the record name in this search for the record
	attrs = MapAttrToLDAPTypeArray( pContext, pRecType->fBufferData, kDSNAttrRecordName );

	try
	{
		//search for the specific LDAP record now
        // we will search over all the rectype mappings until we find the first
        // result for the search criteria in the queryfilter
		
		// serialize search & result
		while ( (pLDAPSearchBase != nil) && (!bResultFound) )
		{

			//build the record query string
			//removed the use well known map only condition ie. true to false
			queryFilter = BuildLDAPQueryFilter(	pContext,
												(char *)kDSNAttrRecordName,
												pRecName->fBufferData,
												eDSExact,
												false,
												(const char *)(pRecType->fBufferData),
												pLDAPSearchBase,
												bOCANDGroup,
												OCSearchList );
			if ( queryFilter == nil ) throw( eDSNullParameter );

			searchResult = DSRetrieveSynchronous( pLDAPSearchBase, attrs, pContext, scope, queryFilter, &result, &ldapDN );
			if ( searchResult == eDSNoErr )
				break;

			DSFreeString( queryFilter );
			DSFreeString( pLDAPSearchBase );
			DSCFRelease( OCSearchList );
			
			numRecTypes++;
			bOCANDGroup = false;

			pLDAPSearchBase = MapRecToSearchBase( pContext, (const char *)(pRecType->fBufferData), numRecTypes, &bOCANDGroup, 
												  &OCSearchList, &scope );
		} // while ( (pLDAPSearchBase != nil) && (!bResultFound) )

		if ( searchResult == eDSNoErr )
		{
			pRecContext = new sLDAPContextData( *pContext );
			if ( pRecContext == nil ) throw( eMemoryAllocError );
	        
			pRecContext->fType = 2;
			
			if (pRecType->fBufferData != nil)
				pRecContext->fOpenRecordType = strdup( pRecType->fBufferData );
			
			if (pRecName->fBufferData != nil)
				pRecContext->fOpenRecordName = strdup( pRecName->fBufferData );
			
			pRecContext->fOpenRecordDN = ldapDN;
			ldapDN = NULL;
			
			gLDAPContextTable->AddObjectForRefNum( inData->fOutRecRef, pRecContext );
			DSRelease( pRecContext );
		} // if bResultFound and ldapReturnCode okay

		siResult = searchResult;
	}

	catch ( tDirStatus err )
	{
		siResult = err;
	}
	
	if ( result != nil )
	{
		ldap_msgfree( result );
		result = nil;
	}
	
	DSCFRelease( OCSearchList );
	DSFreeString( ldapDN );
	DSFreeString( pLDAPSearchBase );
	DSFreeString( queryFilter );
	DSFreeStringList( attrs );
	DSRelease( pContext );

	return( siResult );

} // OpenRecord


//------------------------------------------------------------------------------------
//	* CloseRecord
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::CloseRecord ( sCloseRecord *inData )
{
	tDirStatus			siResult	=	eDSNoErr;
	sLDAPContextData   *pContext	=	nil;

	try
	{
		pContext = gLDAPContextTable->GetObjectForRefNum( inData->fInRecRef );
		if ( pContext == nil ) throw( eDSBadContextData );
		
		if ( pContext->fLDAPConnection )
		{
			// notify about changes people might care about
			if ( strcmp(pContext->fOpenRecordType, kDSStdRecordTypeUsers) == 0 )
				dsNotifyUpdatedRecord( "LDAPv3", pContext->fLDAPConnection->fNodeConfig->fNodeName, "users" );
			else if ( strcmp(pContext->fOpenRecordType, kDSStdRecordTypeGroups) == 0 )
				dsNotifyUpdatedRecord( "LDAPv3", pContext->fLDAPConnection->fNodeConfig->fNodeName, "groups" );
			else if ( strcmp(pContext->fOpenRecordType, kDSStdRecordTypeComputers) == 0 )
				dsNotifyUpdatedRecord( "LDAPv3", pContext->fLDAPConnection->fNodeConfig->fNodeName, "computers" );
		}
		
		gLDAPContextTable->RemoveRefNum( inData->fInRecRef );
		DSRelease( pContext );
	}

	catch ( tDirStatus err )
	{
		siResult = err;
	}

	return( siResult );

} // CloseRecord


//------------------------------------------------------------------------------------
//	* FlushRecord
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::FlushRecord ( sFlushRecord *inData )
{
	tDirStatus			siResult	=	eDSNoErr;

	sLDAPContextData *pContext = gLDAPContextTable->GetObjectForRefNum( inData->fInRecRef );
	if ( pContext == nil ) return eDSBadContextData;
	DSRelease( pContext );

	return( siResult );

} // FlushRecord


//------------------------------------------------------------------------------------
//	* DeleteRecord
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::DeleteRecord ( sDeleteRecord *inData )
{
	return DeleteRecord( inData, false );
}

tDirStatus CLDAPv3Plugin::DeleteRecord ( sDeleteRecord *inData, bool inDeleteCredentials )
{
	tDirStatus				siResult		= eDSNoErr;
	sLDAPContextData	   *pRecContext		= nil;
	char				   *pUserID			= NULL;
	char				   *pAdminID		= NULL;
	char				   *pServerAddress	= NULL;

	pRecContext = gLDAPContextTable->GetObjectForRefNum( inData->fInRecRef );
	if ( pRecContext == nil )
		return( eDSBadContextData );
	CLDAPNodeConfig *nodeConfig = pRecContext->fLDAPConnection->fNodeConfig;
	if ( nodeConfig != NULL && nodeConfig->fLDAPv2ReadOnly )
	{
		DSRelease( pRecContext );
		return( eDSReadOnly );
	}
	
	if ( pRecContext->fOpenRecordDN == nil )
	{
		DSRelease( pRecContext );
		return( eDSNullRecName );
	}
	
	// if we want to delete the credentials, we need to make sure
	// PWS will allow it before deleting the record. We always
	// need to check now because of Tiered Administration.
	if ( strcmp(pRecContext->fOpenRecordType, kDSStdRecordTypeUsers) == 0 ||
		 strcmp(pRecContext->fOpenRecordType, kDSStdRecordTypeComputers) == 0 )
		inDeleteCredentials = true;
	
	if ( inDeleteCredentials && pRecContext->fOpenRecordName != NULL && pRecContext->fOpenRecordType != NULL && 
		 pRecContext->fLDAPConnection->fbAuthenticated && pRecContext->fLDAPConnection->fLDAPUsername )
	{
		char **dn = ldap_explode_dn( pRecContext->fLDAPConnection->fLDAPUsername, 1 );

		pUserID = GetPWSIDforRecord( pRecContext, pRecContext->fOpenRecordName, pRecContext->fOpenRecordType );
		if ( pUserID != NULL )
		{
			// now lets stop at the ","
			char *pComma = strchr( pUserID, ',' );
			if ( pComma != NULL )
			{
				*pComma = '\0';
			}
		}
		pAdminID = GetPWSIDforRecord( pRecContext, dn[0], kDSStdRecordTypeUsers );
		
		if ( pAdminID != NULL )
		{
			pServerAddress = strchr( pAdminID, ':' );
			if ( pServerAddress != NULL )
			{
				pServerAddress++; // skip past the ':'
			}
		}
		
		ldap_value_free( dn );
		dn = NULL;
	}
	
	tDirStatus error = eDSNoErr;
	if ( pUserID != NULL && pAdminID != NULL && pServerAddress != NULL )
	{
		tDirReference		dsRef = 0;
		tDirNodeReference	nodeRef	= 0;

		error = dsOpenDirService( &dsRef );
		if ( error == eDSNoErr )
		{
			char *nodeName = (char *)calloc(1,strlen(pServerAddress)+sizeof("/PasswordServer/")+1);

			if( nodeName != NULL )
			{
				sprintf( nodeName, "/PasswordServer/%s", pServerAddress );
				error = (tDirStatus)PWOpenDirNode( dsRef, nodeName, &nodeRef );
				
				free( nodeName );
				nodeName = NULL;
			}
			else
			{
				error = eMemoryAllocError;
			}
		}
		
		if ( error == eDSNoErr )
		{
			int				iUserIDLen	= strlen( pUserID ) + 1; // include the NULL terminator
			int				iAdminIDLen	= strlen( pAdminID ) + 1; // include the NULL terminator
			int				iPassLen = strlen( pRecContext->fLDAPConnection->fLDAPPassword ) + 1;
			tDataBufferPtr	pAuthMethod = dsDataNodeAllocateString( nodeRef, kDSStdAuthDeleteUser );
			tDataBufferPtr	pStepData	= dsDataBufferAllocatePriv( sizeof(SInt32) + iAdminIDLen + 
																	sizeof(SInt32) + iPassLen +
																	sizeof(SInt32) + iUserIDLen );
			tDataBufferPtr	pStepDataResp = dsDataBufferAllocatePriv( 1024 ); // shouldn't be needed, but just in case
			
			siResult = dsFillAuthBuffer(
							pStepData, 3,
							iAdminIDLen, pAdminID,
							iPassLen, pRecContext->fLDAPConnection->fLDAPPassword,
							iUserIDLen, pUserID );
			if ( siResult == eDSNoErr )
			{
				// let's attempt the delete now
				siResult = dsDoDirNodeAuth( nodeRef, pAuthMethod, true, pStepData, pStepDataResp, NULL );
			}
			
			DbgLog( kLogPlugin, "CLDAPv3Plugin: Attempt to delete PWS ID associated with record returned status %d", siResult );
			
			dsDataNodeDeAllocate( nodeRef, pAuthMethod );
			dsDataNodeDeAllocate( nodeRef, pStepData );
			dsDataNodeDeAllocate( nodeRef, pStepDataResp );
		}
		
		if ( nodeRef )
			dsCloseDirNode( nodeRef );
		
		if ( dsRef )
			dsCloseDirService( dsRef );
	}
	
	// If PWS accepted the deletion, then delete the record from LDAP.
	// PWS returns eDSPermissionError for rejections, eDSAuthFailed for some
	// other error.
	if ( error == eDSNoErr && (siResult == eDSNoErr || siResult == eDSAuthFailed) )
	{
		//KW revisit for what degree of error return we need to provide
		//if LDAP_NOT_ALLOWED_ON_NONLEAF then this is not a leaf in the hierarchy ie. leaves need to go first
		//if LDAP_INVALID_CREDENTIALS or ??? then don't have authority so use eDSPermissionError
		//if LDAP_NO_SUCH_OBJECT then eDSRecordNotFound
		//so for now we return simply  eDSPermissionError if ANY error
		LDAP *aHost = pRecContext->fLDAPConnection->LockLDAPSession();
		if ( aHost != NULL )
		{
			if ( ldap_delete_s(aHost, pRecContext->fOpenRecordDN) != LDAP_SUCCESS )
				siResult = eDSPermissionError;
			pRecContext->fLDAPConnection->UnlockLDAPSession( aHost, false );
		}
		
		// notify about changes people might care about
		if ( strcmp(pRecContext->fOpenRecordType, kDSStdRecordTypeUsers) == 0 )
			dsNotifyUpdatedRecord( "LDAPv3", nodeConfig->fNodeName, "users" );
		else if ( strcmp(pRecContext->fOpenRecordType, kDSStdRecordTypeGroups) == 0 )
			dsNotifyUpdatedRecord( "LDAPv3", nodeConfig->fNodeName, "groups" );
		else if ( strcmp(pRecContext->fOpenRecordType, kDSStdRecordTypeComputers) == 0 )
			dsNotifyUpdatedRecord( "LDAPv3", nodeConfig->fNodeName, "computers" );
		
		gLDAPContextTable->RemoveRefNum( inData->fInRecRef );
	}
	
	DSRelease( pRecContext );
	
	DSFreeString( pAdminID );
	DSFreeString( pUserID );

	return( siResult );
} // DeleteRecord


//------------------------------------------------------------------------------------
//	* IsConfigRecordModify
//------------------------------------------------------------------------------------

bool CLDAPv3Plugin::IsConfigRecordModify( tRecordReference inRecRef )
{
	char			*recTypeStr		= NULL;
	bool			result			= false;
	
	recTypeStr = GetRecordTypeFromRef( inRecRef );
	if ( strcmp(recTypeStr, kDSStdRecordTypeConfig) == 0 )
		result = true;
	DSFree( recTypeStr );
	
	return result;
}


//------------------------------------------------------------------------------------
//	* IncrementChangeSeqNumber
//
//	Increments the change count and handles roll-overs
//------------------------------------------------------------------------------------
 
void CLDAPv3Plugin::IncrementChangeSeqNumber( void )
{
	struct stat sb;
	
	// stop using the out-of-date hints
	if ( lstat(kLDAPReplicaHintFilePath, &sb) == 0 )
		unlink( kLDAPReplicaHintFilePath );
	
	if ( ++gConfigRecordChangeSeqNumber < 0 )
	{
		// out-of-bounds, and we know the replica list needs to be reloaded.
		// adjust to something reasonable.		
		gPWSReplicaListLoadedSeqNumber = 0;
		gConfigRecordChangeSeqNumber = 1;
	}
}


//------------------------------------------------------------------------------------
//	* AddAttribute
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::AddAttribute( sAddAttribute *inData, const char *inRecTypeStr )
{
	tDirStatus		siResult		= eDSNoErr;

	siResult = AddValue( inData->fInRecRef, inData->fInNewAttr, inData->fInFirstAttrValue );

	if ( siResult == eDSNoErr && strcmp(inRecTypeStr, kDSStdRecordTypeConfig) == 0 )
		IncrementChangeSeqNumber();
	
	return( siResult );
} // AddAttribute


//------------------------------------------------------------------------------------
//	* AddAttributeValue
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::AddAttributeValue( sAddAttributeValue *inData, const char *inRecTypeStr )
{
	tDirStatus			siResult		= eDSNoErr;

	siResult = AddValue( inData->fInRecRef, inData->fInAttrType, inData->fInAttrValue );

	if ( siResult == eDSNoErr && strcmp(inRecTypeStr, kDSStdRecordTypeConfig) == 0 )
		IncrementChangeSeqNumber();

	return( siResult );

} // AddAttributeValue


//------------------------------------------------------------------------------------
//	* SetAttributes
//  to be used by custom plugin calls
//  caller owns the CFDictionary and needs to clean it up
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::SetAttributes ( UInt32 inRecRef, CFDictionaryRef inDict )
{
	tDirStatus				siResult		= eDSNoErr;
	char				   *attrTypeStr		= nil;
	char				   *attrTypeLDAPStr	= nil;
	sLDAPContextData	   *pRecContext		= nil;
	UInt32					attrCount		= 0;
	UInt32					attrIndex		= 0;
	UInt32					valCount		= 0;
	UInt32					valIndex		= 0;
	struct berval		  **newValues		= nil;
	LDAPMod				   *mods[128];				//pick 128 since it seems unlikely we will ever get to 126 required attrs
	int						ldapReturnCode	= 0;
	char				   *attrValue		= nil;
	UInt32					attrLength		= 0;
	LDAP				   *aHost			= nil;
	UInt32					callocLength	= 0;
	bool					bGotIt			= false;
	CFStringRef				keyCFString		= NULL;

	try
	{
		pRecContext = gLDAPContextTable->GetObjectForRefNum( inRecRef );
		if ( pRecContext == nil ) throw( eDSBadContextData );
		
		CLDAPNodeConfig *nodeConfig = pRecContext->fLDAPConnection->fNodeConfig;
		if ( nodeConfig != NULL && nodeConfig->fLDAPv2ReadOnly ) throw( eDSReadOnly);

		if ( pRecContext->fOpenRecordDN == nil ) throw( eDSNullRecName );
		if ( CFGetTypeID(inDict) != CFDictionaryGetTypeID() ) throw( eDSInvalidBuffFormat );

		if ( IsConfigRecordModify(inRecRef) )
			IncrementChangeSeqNumber();
		
		for (UInt32 modsIndex=0; modsIndex < 128; modsIndex++)
		{
			mods[modsIndex] = NULL;
		}

		//find out how many attrs need to be set
		attrCount = (UInt32) CFDictionaryGetCount(inDict);
		char** keys = (char**)calloc( attrCount + 1, sizeof( void* ) );
		//get a linked list of the attrs so we can iterate over them
		CFDictionaryGetKeysAndValues( inDict, (const void**)keys, NULL );
		
		//loop over attrs in the dictionary
		attrIndex = 0;
		for( UInt32 keyIndex = 0; keyIndex < attrCount; keyIndex++ )
		{
			keyCFString = NULL;
			if ( !(CFGetTypeID((CFStringRef)keys[keyIndex]) == CFStringGetTypeID() ) )
			{
				continue; //skip this one and do not increment attrIndex
			}
			else
			{
				keyCFString = (CFStringRef)keys[keyIndex];
			}

			attrTypeStr = (char *)CFStringGetCStringPtr( keyCFString, kCFStringEncodingUTF8 );
			if (attrTypeStr == nil)
			{
				callocLength = (UInt32) CFStringGetMaximumSizeForEncoding(CFStringGetLength(keyCFString), kCFStringEncodingUTF8) + 1;
				attrTypeStr = (char *)calloc( 1, callocLength );
				if ( attrTypeStr == nil ) throw( eMemoryError );

				// Convert it to a regular 'C' string 
				bGotIt = CFStringGetCString( keyCFString, attrTypeStr, callocLength, kCFStringEncodingUTF8 );
				if (bGotIt == false) throw( eMemoryError );
			}

			//get the first mapping
			attrTypeLDAPStr = MapAttrToLDAPType( pRecContext, (const char *)(pRecContext->fOpenRecordType), attrTypeStr, 1, true );
			//throw if first nil since we only use the first native type to write to
			//skip everything if a single one is incorrect?
			if ( attrTypeLDAPStr == nil ) throw( eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable

			CFArrayRef valuesCFArray = (CFArrayRef)CFDictionaryGetValue( inDict, keyCFString );
			if ( !(CFGetTypeID(valuesCFArray) == CFArrayGetTypeID() ) )
			{
				if (bGotIt)
				{
					DSFreeString(attrTypeStr);
				}
				continue; //skip this one and free up the attr string if required
			}
			valCount	= (UInt32) CFArrayGetCount( valuesCFArray );
			newValues	= (struct berval**) calloc(1, (valCount + 1) *sizeof(struct berval *) );
			
			valIndex = 0;
			for (UInt32 i = 0; i < valCount; i++ ) //extract the values out of the valuesCFArray
			{
				//need to determine whether the value is either string or data
				CFTypeRef valueCFType = CFArrayGetValueAtIndex( valuesCFArray, (CFIndex)i );
				
				if ( CFGetTypeID(valueCFType) == CFStringGetTypeID() )
				{
					CFStringRef valueCFString = (CFStringRef)valueCFType;
					
					callocLength = (UInt32) CFStringGetMaximumSizeForEncoding(CFStringGetLength(valueCFString), kCFStringEncodingUTF8) + 1;
					attrValue = (char *)calloc( 1, callocLength );
					if ( attrValue == nil ) throw( eMemoryError );

					// Convert it to a regular 'C' string 
					bool bGotValue = CFStringGetCString( valueCFString, attrValue, callocLength, kCFStringEncodingUTF8 );
					if (bGotValue == false) throw( eMemoryError );

					attrLength = strlen(attrValue);
					newValues[valIndex] = (struct berval*) calloc(1, sizeof(struct berval) );
					newValues[valIndex]->bv_val = attrValue;
					newValues[valIndex]->bv_len = attrLength;
					valIndex++;
					attrValue = nil;
				}
				else if ( CFGetTypeID(valueCFType) == CFDataGetTypeID() )
				{
					CFDataRef valueCFData = (CFDataRef)valueCFType;
					
					attrLength = (UInt32) CFDataGetLength(valueCFData);
					attrValue = (char *)calloc( 1, attrLength + 1 );
					if ( attrValue == nil ) throw( eMemoryError );
					
					CFDataGetBytes( valueCFData, CFRangeMake(0,attrLength), (UInt8*)attrValue );

					newValues[valIndex] = (struct berval*) calloc(1, sizeof(struct berval) );
					newValues[valIndex]->bv_val = attrValue;
					newValues[valIndex]->bv_len = attrLength;
					valIndex++;
					attrValue = nil;
				}
			} // for each value provided
			
			if (valIndex > 0) //means we actually have something to add
			{
				//create this mods entry
				mods[attrIndex] = (LDAPMod *) calloc( 1, sizeof(LDAPMod *) );
				mods[attrIndex]->mod_op		= LDAP_MOD_REPLACE | LDAP_MOD_BVALUES;
				mods[attrIndex]->mod_type	= attrTypeLDAPStr;
				mods[attrIndex]->mod_bvalues= newValues;
				attrTypeLDAPStr = nil;
				attrIndex++;
				if (attrIndex == 127) //we need terminating NULL for the list
				{
					if (bGotIt)
					{
						DSFreeString(attrTypeStr);
					}
					break; //this is all we modify ie. first 126 attrs in this set - we will never hit this
				}
			}
			if (bGotIt)
			{
				DSFreeString(attrTypeStr);
			}
		} //for( UInt32 keyIndex = 0; keyIndex < attrCount; keyIndex++ )

		aHost = pRecContext->fLDAPConnection->LockLDAPSession();
		if (aHost != NULL)
		{
			ldapReturnCode = ldap_modify_s( aHost, pRecContext->fOpenRecordDN, mods);
			pRecContext->fLDAPConnection->UnlockLDAPSession( aHost, false );
		}
		else
		{
			ldapReturnCode = LDAP_LOCAL_ERROR;
		}
		if ( ldapReturnCode != LDAP_SUCCESS )
		{
			siResult = MapLDAPWriteErrorToDS( ldapReturnCode, eDSAttributeNotFound );
		}
		
		for (UInt32 modsIndex=0; ((modsIndex < 128) && (mods[modsIndex] != NULL)); modsIndex++)
		{
			DSFreeString(mods[modsIndex]->mod_type)
			newValues = mods[modsIndex]->mod_bvalues;
			if (newValues != NULL)
			{
				ldap_value_free_len(newValues);
				newValues = NULL;
			}
			mods[modsIndex] = NULL;
		}
	}

	catch ( tDirStatus err )
	{
		siResult = err;
	}

	DSRelease( pRecContext );
	DSFreeString( attrTypeLDAPStr );
		
	return( siResult );

} // SetAttributes


//------------------------------------------------------------------------------------
//	* RemoveAttribute
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::RemoveAttribute ( sRemoveAttribute *inData, const char *inRecTypeStr )
{
	tDirStatus				siResult		= eDSNoErr;
	sLDAPContextData	   *pRecContext		= nil;
	int						numAttributes	= 1;
	char				   *pLDAPAttrType	= nil;
	LDAPMod				   *mods[2];
	int						ldapReturnCode	= 0;

	try
	{
		pRecContext = gLDAPContextTable->GetObjectForRefNum( inData->fInRecRef );
		if ( pRecContext == nil ) throw( eDSBadContextData );
		
		CLDAPNodeConfig *nodeConfig = pRecContext->fLDAPConnection->fNodeConfig;
		if ( nodeConfig != NULL && nodeConfig->fLDAPv2ReadOnly ) throw( eDSReadOnly);

		if ( pRecContext->fOpenRecordDN == nil ) throw( eDSNullRecName );
		if ( inData->fInAttribute->fBufferData == nil ) throw( eDSNullAttributeType );

		//get the first mapping
		numAttributes = 1;
		pLDAPAttrType = MapAttrToLDAPType( pRecContext, (const char *)(pRecContext->fOpenRecordType), inData->fInAttribute->fBufferData, 
										   numAttributes, true );
		//throw if first nil since no more will be found otherwise proceed until nil
		if ( pLDAPAttrType == nil ) throw( eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable

		if ( strcmp(inRecTypeStr, kDSStdRecordTypeConfig) == 0 )
			IncrementChangeSeqNumber();

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
				
				LDAP *aHost = pRecContext->fLDAPConnection->LockLDAPSession();
				if (aHost != NULL)
				{
					ldapReturnCode = ldap_modify_s( aHost, pRecContext->fOpenRecordDN, mods);
					pRecContext->fLDAPConnection->UnlockLDAPSession( aHost, false );
				}
				else
				{
					ldapReturnCode = LDAP_LOCAL_ERROR;
				}
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
			DSFreeString(pLDAPAttrType);
			numAttributes++;
			//get the next mapping
			pLDAPAttrType = MapAttrToLDAPType( pRecContext, (const char *)(pRecContext->fOpenRecordType), 
											   inData->fInAttribute->fBufferData, numAttributes, true );
		} // while ( pLDAPAttrType != nil )

	}

	catch ( tDirStatus err )
	{
		siResult = err;
	}
	
	DSRelease( pRecContext );
	
	return( siResult );

} // RemoveAttribute


//------------------------------------------------------------------------------------
//	* AddValue
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::AddValue ( UInt32 inRecRef, tDataNodePtr inAttrType, tDataNodePtr inAttrValue )
{
	tDirStatus				siResult		= eDSNoErr;
	sLDAPContextData	   *pRecContext		= nil;
	char				   *attrValue		= nil;
	UInt32					attrLength		= 0;
	struct berval			bval;
	struct berval			*bvals[2];
	int						numAttributes	= 1;
	char				   *pLDAPAttrType	= nil;
	LDAPMod				   *mods[2];
	int						ldapReturnCode	= 0;

	try
	{
		pRecContext = gLDAPContextTable->GetObjectForRefNum( inRecRef );
		if ( pRecContext == nil ) throw( eDSBadContextData );
		
		CLDAPNodeConfig *nodeConfig = pRecContext->fLDAPConnection->fNodeConfig;
		if ( nodeConfig != NULL && nodeConfig->fLDAPv2ReadOnly ) throw( eDSReadOnly);

		if ( pRecContext->fOpenRecordDN == nil ) throw( eDSNullRecName );
		if ( inAttrType->fBufferData == nil ) throw( eDSNullAttributeType );

		//build the mods entry to pass into ldap_modify_s
		if ( (inAttrValue == nil) || (inAttrValue->fBufferLength < 1) )
		{
			//don't allow empty values since the ldap call will fail
			throw( eDSEmptyAttributeValue );
		}
		else
		{
			attrLength = inAttrValue->fBufferLength;
			attrValue = (char *) calloc(1, 1 + attrLength);
			memcpy(attrValue, inAttrValue->fBufferData, attrLength);
			
		}
		
		if ( IsConfigRecordModify(inRecRef) )
			IncrementChangeSeqNumber();
		
		bval.bv_val = attrValue;
		bval.bv_len = attrLength;
		bvals[0]	= &bval;
		bvals[1]	= NULL;

		//get the first mapping
		numAttributes = 1;
		pLDAPAttrType = MapAttrToLDAPType( pRecContext, (const char *)(pRecContext->fOpenRecordType), inAttrType->fBufferData, numAttributes, true );
		//throw if first nil since no more will be found otherwise proceed until nil
		if ( pLDAPAttrType == nil ) throw( eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable

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
				
				LDAP *aHost = pRecContext->fLDAPConnection->LockLDAPSession();
				if (aHost != NULL)
				{
					ldapReturnCode = ldap_modify_s( aHost, pRecContext->fOpenRecordDN, mods);
					pRecContext->fLDAPConnection->UnlockLDAPSession( aHost, false );
				}
				else
				{
					ldapReturnCode = LDAP_LOCAL_ERROR;
				}
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

	catch ( tDirStatus err )
	{
		siResult = err;
	}
	
	DSFree(attrValue);
	DSRelease( pRecContext );
		
	return( siResult );

} // AddValue


//------------------------------------------------------------------------------------
//	* SetRecordName
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::SetRecordName ( sSetRecordName *inData )
{
	tDirStatus				siResult		= eDSNoErr;
	sLDAPContextData	   *pRecContext		= nil;
	char				   *pLDAPAttrType	= nil;
	int						ldapReturnCode	= 0;
	tDataNodePtr			pRecName		= nil;
	char				   *ldapRDNString	= nil;
	UInt32					ldapRDNLength	= 0;

	try
	{
		pRecContext = gLDAPContextTable->GetObjectForRefNum( inData->fInRecRef );
		if ( pRecContext == nil ) throw( eDSBadContextData );
		
		CLDAPNodeConfig *nodeConfig = pRecContext->fLDAPConnection->fNodeConfig;
		if ( nodeConfig != NULL && nodeConfig->fLDAPv2ReadOnly ) throw( eDSReadOnly);

		//here we are going to assume that the name given to create this record
		//will fill out the DN with the first recordname native type
		pRecName = inData->fInNewRecName;
		if ( pRecName == nil ) throw( eDSNullRecName );

		if (pRecName->fBufferLength == 0) throw( eDSEmptyRecordNameList );

		//get ONLY the first record name mapping
		pLDAPAttrType = MapAttrToLDAPType( pRecContext, (const char *)(pRecContext->fOpenRecordType), kDSNAttrRecordName, 1, true );
		//throw if first nil since no more will be found otherwise proceed until nil
		if ( pLDAPAttrType == nil ) throw( eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable
		
		
		//if first native map for kDSNAttrRecordName is "cn" then the DN will be built as follows:
		//"cn = pRecName->fBufferData, pLDAPSearchBase"
		//special cars in ldapRDNString need to be escaped
		ldapRDNLength = strlen(pLDAPAttrType) + 1 + 2*pRecName->fBufferLength;
		ldapRDNString = (char *)calloc(1, ldapRDNLength + 1);
		strcpy(ldapRDNString,pLDAPAttrType);
		strcat(ldapRDNString,"=");
		char *escapedString = BuildEscapedRDN(pRecName->fBufferData);
		strcat(ldapRDNString,escapedString);
		DSFreeString(escapedString);
		
		//KW looks like for v3 we must use ldap_rename API instead of ldap_modrdn2_s for v2

//		ldapReturnCode = ldap_modrdn2_s( pRecContext->fHost, pRecContext->fOpenRecordDN, ldapRDNString, 1);
		LDAP *aHost = pRecContext->fLDAPConnection->LockLDAPSession();
		if (aHost != NULL)
		{
			ldapReturnCode = ldap_rename_s( aHost, pRecContext->fOpenRecordDN, ldapRDNString, NULL, 1, NULL, NULL);
			pRecContext->fLDAPConnection->UnlockLDAPSession( aHost, false );
		}
		else
		{
			ldapReturnCode = LDAP_LOCAL_ERROR;
		}
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
			DSFreeString(pRecContext->fOpenRecordName);
			pRecContext->fOpenRecordName = (char *)calloc(1, 1+strlen(pRecName->fBufferData));
			strcpy( pRecContext->fOpenRecordName, pRecName->fBufferData );
			
			char *newldapDN		= nil;
			char *pLDAPSearchBase	= nil;
			pLDAPSearchBase = MapRecToSearchBase( pRecContext, (const char *)(pRecContext->fOpenRecordType), 1, nil, nil, nil );
			if (pLDAPSearchBase != nil)
			{
				newldapDN = (char *) calloc(1, 1 + strlen(ldapRDNString) + 2 + strlen(pLDAPSearchBase));
				strcpy(newldapDN,ldapRDNString);
				strcat(newldapDN,", ");
				strcat(newldapDN,pLDAPSearchBase);
				DSFreeString(pRecContext->fOpenRecordDN);
				pRecContext->fOpenRecordDN = newldapDN;
				DSFreeString(pLDAPSearchBase);
			}
		}

	}

	catch ( tDirStatus err )
	{
		siResult = err;
	}
	
	//cleanup pLDAPAttrType if needed
	DSFreeString(pLDAPAttrType);

	//cleanup ldapRDNString if needed
	DSFreeString(ldapRDNString);
	
	DSRelease( pRecContext );

	return( siResult );

} // SetRecordName


//------------------------------------------------------------------------------------
//	* CreateRecord
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::CreateRecord ( sCreateRecord *inData )
{
	tDirStatus				siResult		= eDSNoErr;
	sLDAPContextData	   *pContext		= nil;
	sLDAPContextData	   *pRecContext		= nil;
	char				   *pLDAPAttrType	= nil;
	LDAPMod				   *mods[128];				//pick 128 since it seems unlikely we will ever get to 126 required attrs
	UInt32					modIndex		= 0;
	int						ldapReturnCode	= 0;
	tDataNodePtr			pRecType		= nil;
	tDataNodePtr			pRecName		= nil;
	char				   *pLDAPSearchBase	= nil;
	char				   *ldapDNString	= nil;
	UInt32					ldapDNLength	= 0;
	UInt32					ocCount			= 0;
	UInt32					raCount			= 0;
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
	const CFIndex			cfBuffSize		= 1024;
	int						aOCSearchListCount	= 0;
	char				   *recName			= NULL;
	int						recNameLen		= 0;
	char				  **aValues			= NULL;
	char				  **aMaps			= NULL;

	try
	{
		pContext = gLDAPContextTable->GetObjectForRefNum( inData->fInNodeRef );
		if ( pContext == nil ) throw( eDSBadContextData );
		
		CLDAPNodeConfig *nodeConfig = pContext->fLDAPConnection->fNodeConfig;
		if ( nodeConfig != NULL && nodeConfig->fLDAPv2ReadOnly ) throw( eDSReadOnly);

		pRecType = inData->fInRecType;
		if ( pRecType == nil ) throw( eDSNullRecType );

		//here we are going to assume that the name given to create this record
		//will fill out the DN with the first recordname native type
		pRecName = inData->fInRecName;
		if ( pRecName == nil ) throw( eDSNullRecName );

		if (pRecName->fBufferLength == 0) throw( eDSEmptyRecordNameList );
		if (pRecType->fBufferLength == 0) throw( eDSEmptyRecordTypeList );
		
		recName = pRecName->fBufferData;
		recNameLen = pRecName->fBufferLength;

		//get ONLY the first record name mapping
		pLDAPAttrType = MapAttrToLDAPType( pContext, (const char *)(pRecType->fBufferData), kDSNAttrRecordName, 1, true );
		//throw if first nil since no more will be found otherwise proceed until nil
		if ( pLDAPAttrType == nil ) throw( eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable
		
		//get ONLY the first record type mapping
		pLDAPSearchBase = MapRecToSearchBase( pContext, (const char *)(pRecType->fBufferData), 1, &bOCANDGroup, &OCSearchList, nil );
		//throw if first nil since no more will be found otherwise proceed until nil
		if ( pLDAPSearchBase == nil ) throw( eDSInvalidRecordType );  //KW would like a eDSNoMappingAvailable
		
		if (strcmp(pRecType->fBufferData, kDSStdRecordTypeAutomount) == 0)
		{
			char	*names[2]	= { recName, NULL };
			UInt32	count		= 0;
			
			// automountMap type
			char *pMapRecType = MapRecToSearchBase( pContext, kDSStdRecordTypeAutomountMap, 1, NULL, NULL, NULL );
			if (pMapRecType == NULL) throw( eDSInvalidRecordType );
			
			char *pMapAttrType = MapAttrToLDAPType( pContext,kDSStdRecordTypeAutomountMap, kDSNAttrRecordName, 1, true );
			if (pMapAttrType == NULL) throw( eDSInvalidRecordType );
			
			siResult = CheckAutomountNames( kDSStdRecordTypeAutomount, names, &aValues, &aMaps, &count );
			if (siResult != eDSNoErr || count == 0 || aMaps[0][0] == '\0') throw( eDSInvalidRecordName );
			
			recName = aValues[0];
			recNameLen = strlen( recName );
			
			//if first native map for kDSNAttrRecordName is "cn" then the DN will be built as follows:
			//"cn = pRecName->fBufferData, automountMap=mapName, automountMapType"
			char *escRecName = BuildEscapedRDN( recName );
			char *escMapname = BuildEscapedRDN( aMaps[0] );

			ldapDNLength = strlen(pLDAPAttrType) + 1 + strlen(escRecName) + 2 + strlen(pMapAttrType) + 1 + strlen(escMapname) + 2 + strlen(pMapRecType) + 1;
			ldapDNString = (char *)calloc(1, ldapDNLength);

			snprintf( ldapDNString, ldapDNLength, "%s=%s, %s=%s, %s", pLDAPAttrType, escRecName, pMapAttrType, escMapname, pMapRecType );

			DSFreeString( escRecName );
			DSFreeString( escMapname );
			DSFreeString( pMapAttrType );
			DSFreeString( pMapRecType );
		}
		else
		{
			//if first native map for kDSNAttrRecordName is "cn" then the DN will be built as follows:
			//"cn = pRecName->fBufferData, pLDAPSearchBase"
			//might need to escape RDN chars
			ldapDNLength = strlen(pLDAPAttrType) + 1 + 2*recNameLen + 2 + strlen(pLDAPSearchBase);
			ldapDNString = (char *)calloc(1, ldapDNLength + 1);
			strcpy(ldapDNString,pLDAPAttrType);
			strcat(ldapDNString,"=");
			char *escapedString = BuildEscapedRDN(recName);
			strcat(ldapDNString,escapedString);
			DSFreeString(escapedString);
			strcat(ldapDNString,", ");
			strcat(ldapDNString,pLDAPSearchBase);
		}
		
		rnvals[0] = recName;
		rnvals[1] = NULL;
		rnmod.mod_op = 0;
		rnmod.mod_type = pLDAPAttrType;
		rnmod.mod_values = rnvals;

		if ( (pRecType->fBufferData != nil) && (pLDAPSearchBase != nil) )
		{
			if (strncmp(pRecType->fBufferData,kDSStdRecordTypePrefix,strlen(kDSStdRecordTypePrefix)) == 0)
			{
				if (OCSearchList != nil)
				{
					CFStringRef	ocCFString = nil;
					// assume that the extracted objectclass strings will be significantly less than 1024 characters
					tmpBuff = (char *)calloc(1, cfBuffSize);
					
					// here we extract the object class strings
					// do we need to escape any of the characters internal to the CFString??? like before
					// NO since "*, "(", and ")" are not legal characters for objectclass names
					
					// if OR then we only use the first one
					if (!bOCANDGroup)
					{
						if (CFArrayGetCount(OCSearchList) >= 1)
						{
							ocCFString = (CFStringRef)CFArrayGetValueAtIndex( OCSearchList, 0 );
						}
						if (ocCFString != NULL)
						{
							CFStringGetCString(ocCFString, tmpBuff, cfBuffSize, kCFStringEncodingUTF8);
							string ourString(tmpBuff);
							objectClassList.push_back(ourString);
						}
					}
					else
					{
						aOCSearchListCount = CFArrayGetCount(OCSearchList);
						for ( int iOCIndex = 0; iOCIndex < aOCSearchListCount; iOCIndex++ )
						{
							bzero(tmpBuff, cfBuffSize);
							ocCFString = (CFStringRef)CFArrayGetValueAtIndex( OCSearchList, iOCIndex );
							if (ocCFString != nil)
							{		
								CFStringGetCString(ocCFString, tmpBuff, cfBuffSize, kCFStringEncodingUTF8);
								string ourString(tmpBuff);
								objectClassList.push_back(ourString);
							}
						}// loop over the objectclasses CFArray
					}
				}//OCSearchList != nil
			}// if (strncmp(pRecType->fBufferData,kDSStdRecordTypePrefix,strlen(kDSStdRecordTypePrefix)) == 0)
		}// if ( (pRecType->fBufferData != nil) && (pLDAPSearchBase != nil) )

		mods[0] = &rnmod;
//		mods[1] = &snmod;
		for (UInt32 modsIndex=1; modsIndex < 128; modsIndex++)
		{
			mods[modsIndex] = NULL;
		}
		modIndex = 1;
		
		if (OCSearchList != nil)
		{
			if ( nodeConfig != NULL )
				nodeConfig->GetReqAttrListForObjectList( objectClassList, reqAttrsList );

			raCount = reqAttrsList.size();
			ocCount = objectClassList.size();
			ocString = strdup("objectClass");
			ocmod.mod_op = 0;
			ocmod.mod_type = ocString;
			ocmod.mod_values = nil;
			ocvals = (char **)calloc(1,(ocCount+1)*sizeof(char **));
			//build the ocvals here
			UInt32 ocValIndex = 0;
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

		LDAP *aHost = pContext->fLDAPConnection->LockLDAPSession();
		if (aHost != NULL)
		{
			ldapReturnCode = ldap_add_ext_s( aHost, ldapDNString, mods, NULL, NULL);
			pContext->fLDAPConnection->UnlockLDAPSession( aHost, false );
		}
		else
		{
			ldapReturnCode = LDAP_LOCAL_ERROR;
		}
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
			pRecContext = new sLDAPContextData( *pContext );
			if ( pRecContext == nil ) throw( eMemoryAllocError );
	        
			pRecContext->fType = 2;

			if (pRecType->fBufferData != nil)
			{
				pRecContext->fOpenRecordType = strdup( pRecType->fBufferData );
			}
			if (recName != nil)
			{
				pRecContext->fOpenRecordName = strdup( recName );
			}
			
			//get the ldapDN here
			pRecContext->fOpenRecordDN = ldapDNString;
			ldapDNString = nil;
		
			gLDAPContextTable->AddObjectForRefNum( inData->fOutRecRef, pRecContext );
			DSRelease( pRecContext );
		}

	}

	catch ( tDirStatus err )
	{
		siResult = err;
	}
	
	//cleanup if needed
	DSFreeString(pLDAPAttrType);
	DSFreeString(pLDAPSearchBase);
	DSFreeString(ldapDNString);
	DSFree(needsValueMarker);
	DSFreeString(ocString);
	DSFree(ocvals);
	DSFreeStringList( aValues );
	DSFreeStringList( aMaps );
	
	UInt32 startIndex = 1;
	if (OCSearchList != nil)
	{
		startIndex = 2;
	}
	for (UInt32 anIndex = startIndex; anIndex < modIndex; anIndex++)
	{
		free(mods[anIndex]);
		mods[anIndex] = NULL;
	}
	
	DSCFRelease(OCSearchList);
	DSFreeString(tmpBuff);
	DSRelease( pContext );

	return( siResult );

} // CreateRecord


//------------------------------------------------------------------------------------
//	* CreateRecordWithAttributes
//  to be used by custom plugin calls
//  caller owns the CFDictionary and needs to clean it up
//  Record name is ONLY singled valued ie. multiple values are ignored if provided
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::CreateRecordWithAttributes ( tDirNodeReference inNodeRef, const char* inRecType, const char* inRecName, CFDictionaryRef inDict )
{
	tDirStatus				siResult		= eDSNoErr;
	char				   *attrTypeStr		= nil;
	char				   *attrTypeLDAPStr	= nil;
	sLDAPContextData	   *pContext		= nil;
	UInt32					attrCount		= 0;
	UInt32					valCount		= 0;
	UInt32					valIndex		= 0;
	struct berval		  **newValues		= nil;
	LDAPMod				   *mods[128];				//pick 128 since it seems unlikely we will ever get to 126 required attrs
	int						ldapReturnCode	= 0;
	char				   *attrValue		= nil;
	UInt32					attrLength		= 0;
	LDAP				   *aHost			= nil;
	UInt32					callocLength	= 0;
	bool					bGotIt			= false;
	CFStringRef				keyCFString		= NULL;
	char				   *pLDAPSearchBase	= nil;
	char				   *recNameAttrType	= nil;
	char				   *ldapDNString	= nil;
	UInt32					ldapDNLength	= 0;
	UInt32					ocCount			= 0;
	UInt32					raCount			= 0;
	LDAPMod				   *ocmod;
	LDAPMod				   *rnmod;
	char				  **ocvals			= nil;
	char				   *rnvals[2];
	listOfStrings			objectClassList;
	listOfStrings			reqAttrsList;
	bool					bOCANDGroup		= false;
	CFArrayRef				OCSearchList	= nil;
	char				   *tmpBuff			= nil;
	CFIndex					cfBuffSize		= 1024;
	int						aOCSearchListCount	= 0;
	UInt32					keyIndex		= 0;
	UInt32					modIndex		= 0;
	UInt32					bValueStartIndex= 0;
	CFMutableDictionaryRef	mergedDict		= NULL;
	char				  **aValues			= NULL;
	char				  **aMaps			= NULL;

	try
	{
		pContext = gLDAPContextTable->GetObjectForRefNum( inNodeRef );
		if ( pContext == nil ) throw( eDSBadContextData );
		
		CLDAPNodeConfig *nodeConfig = pContext->fLDAPConnection->fNodeConfig;
		if ( nodeConfig != NULL && nodeConfig->fLDAPv2ReadOnly ) throw( eDSReadOnly);

		if ( CFGetTypeID(inDict) != CFDictionaryGetTypeID() ) throw( eDSInvalidBuffFormat );

		if ( inRecType == nil ) throw( eDSNullRecType );
		//here we are going to assume that the name given to create this record
		//will fill out the DN with the first recordname native type
		if ( inRecName == nil ) throw( eDSNullRecName );
		
		for (UInt32 modsIndex=0; modsIndex < 128; modsIndex++)
		{
			mods[modsIndex] = NULL;
		}

		//get ONLY the first record name mapping
		recNameAttrType = MapAttrToLDAPType( pContext, (const char *)inRecType, kDSNAttrRecordName, 1, true );
		//throw if first nil since no more will be found otherwise proceed until nil
		if ( recNameAttrType == nil ) throw( eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable
		
		//get ONLY the first record type mapping
		pLDAPSearchBase = MapRecToSearchBase( pContext, (const char *)inRecType, 1, &bOCANDGroup, &OCSearchList, nil );
		//throw if first nil since no more will be found otherwise proceed until nil
		if ( pLDAPSearchBase == nil ) throw( eDSInvalidRecordType );  //KW would like a eDSNoMappingAvailable
		
		if (strcmp(inRecType, kDSStdRecordTypeAutomount) == 0)
		{
			char	*names[2]	= { (char *) inRecName, NULL };
			UInt32	count		= 0;
			
			// automountMap type
			char *pMapRecType = MapRecToSearchBase( pContext, kDSStdRecordTypeAutomountMap, 1, NULL, NULL, NULL );
			if (pMapRecType == NULL) throw( eDSInvalidRecordType );
			
			char *pMapAttrType = MapAttrToLDAPType( pContext,kDSStdRecordTypeAutomountMap, kDSNAttrRecordName, 1, true );
			if (pMapAttrType == NULL) throw( eDSInvalidRecordType );
			
			siResult = CheckAutomountNames( kDSStdRecordTypeAutomount, names, &aValues, &aMaps, &count );
			if (siResult != eDSNoErr || count == 0 || aMaps[0][0] == '\0') throw( eDSInvalidRecordName );
			
			inRecName = aValues[0];
			
			//if first native map for kDSNAttrRecordName is "cn" then the DN will be built as follows:
			//"cn = pRecName->fBufferData, automountMap=mapName, automountMapType"
			char *escRecName = BuildEscapedRDN( inRecName );
			char *escMapname = BuildEscapedRDN( aMaps[0] );
			
			ldapDNLength = strlen(recNameAttrType) + 1 + strlen(escRecName) + 2 + strlen(pMapAttrType) + 1 + strlen(escMapname) + 2 + strlen(pMapRecType) + 1;
			ldapDNString = (char *)calloc(1, ldapDNLength);
			
			snprintf( ldapDNString, ldapDNLength, "%s=%s, %s=%s, %s", recNameAttrType, escRecName, pMapAttrType, escMapname, pMapRecType );
			
			DSFreeString( escRecName );
			DSFreeString( escMapname );
		}
		else
		{
			//if first native map for kDSNAttrRecordName is "cn" then the DN will be built as follows:
			//"cn = inRecName, pLDAPSearchBase"
			//RDN chars might need escaping
			ldapDNLength = strlen(recNameAttrType) + 1 + 2*strlen(inRecName) + 2 + strlen(pLDAPSearchBase);
			ldapDNString = (char *)calloc(1, ldapDNLength + 1);
			strcpy(ldapDNString,recNameAttrType);
			strcat(ldapDNString,"=");
			char *escapedString = BuildEscapedRDN(inRecName);
			strcat(ldapDNString,escapedString);
			DSFreeString(escapedString);
			strcat(ldapDNString,", ");
			strcat(ldapDNString,pLDAPSearchBase);
		}
		
		rnmod = (LDAPMod *) calloc( 1, sizeof(LDAPMod *) );
		rnmod->mod_type = strdup(recNameAttrType);

		CFArrayRef shortNames = (CFArrayRef)CFDictionaryGetValue( inDict, CFSTR( kDSNAttrRecordName ) );
		CFIndex numShortNames = CFArrayGetCount( shortNames );
		
		if( numShortNames == 1 )
		{
			rnvals[0] = (char *)inRecName;
			rnvals[1] = NULL;

			rnmod->mod_op = LDAP_MOD_ADD;
			rnmod->mod_values = rnvals; //not freed below
		}
		else
		{
			newValues = (struct berval**) calloc(1, (numShortNames + 1) *sizeof(struct berval *) );
			valIndex = 0;
			for( CFIndex i=0; i<numShortNames; i++ )
			{
				CFStringRef shortName  = (CFStringRef)CFArrayGetValueAtIndex( shortNames, i );

				callocLength = (UInt32) CFStringGetMaximumSizeForEncoding(
					CFStringGetLength(shortName), kCFStringEncodingUTF8) + 1;
				attrValue = (char *)calloc( 1, callocLength );
				if ( attrValue == NULL ) throw( eMemoryError );
				// Convert it to a regular 'C' string 
				bool bGotValue = CFStringGetCString( shortName, attrValue, callocLength, kCFStringEncodingUTF8 );
				if (bGotValue == false) throw( eMemoryError );
				attrLength = strlen(attrValue);

				newValues[valIndex] = (struct berval*) calloc(1, sizeof(struct berval) );
				newValues[valIndex]->bv_val = attrValue;
				newValues[valIndex]->bv_len = attrLength;
				valIndex++;
			}

			rnmod->mod_op = LDAP_MOD_ADD | LDAP_MOD_BVALUES;
			rnmod->mod_bvalues = newValues;
		}

		if ( (inRecType != nil) && (pLDAPSearchBase != nil) )
		{
			if (strncmp(inRecType,kDSStdRecordTypePrefix,strlen(kDSStdRecordTypePrefix)) == 0)
			{
				if (OCSearchList != nil)
				{
					CFStringRef	ocString = nil;
					// assume that the extracted objectclass strings will be significantly less than 1024 characters
					tmpBuff = (char *)calloc(1, 1024);
					
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
		}// if ( (pRecType->fBufferData != nil) && (pLDAPSearchBase != nil) )

		//first entry is the name of the record to create
		mods[0] = rnmod;
		modIndex = 1;
		bValueStartIndex = 1;
		
		if (OCSearchList != nil && nodeConfig != NULL)
		{
			nodeConfig->GetReqAttrListForObjectList( objectClassList, reqAttrsList );
			
			//set the count of required attrs
			raCount = reqAttrsList.size();
			
			//set the count of object classes
			ocCount = objectClassList.size();
			
			ocmod = (LDAPMod *) calloc( 1, sizeof(LDAPMod *) );
			ocmod->mod_op = LDAP_MOD_ADD;
			ocmod->mod_type = strdup("objectClass");
			ocmod->mod_values = nil;
			ocvals = (char **)calloc(1,(ocCount+1)*sizeof(char **));
			//build the ocvals here
			UInt32 ocValIndex = 0;
			for (listOfStringsCI iter = objectClassList.begin(); iter != objectClassList.end(); ++iter)
			{
				const char *aString = (*iter).c_str();
				ocvals[ocValIndex] = (char *)aString;  //TODO recheck for leaks
				ocValIndex++;
			}
			ocvals[ocCount] = nil;
			ocmod->mod_values = ocvals; // freed outside of mods below

			mods[1] = ocmod;
			modIndex = 2;
			bValueStartIndex = 2;
		}
		
		//NEED to reconcile the two separate attr lists
		//ie. one of required attrs and one of user defined attrs to set
		//build a Dict of the required attrs and then add in or replcae with the defined attrs
		
		mergedDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		CFStringRef singleDefaultValue = CFStringCreateWithCString( kCFAllocatorDefault, "99", kCFStringEncodingUTF8 ); //TODO KW really need syntax specific default value added here
		CFMutableArrayRef valueDefaultArray = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
		CFArrayAppendValue( valueDefaultArray, singleDefaultValue );
		
		//check if we have determined what attrs need to be added
		if (raCount != 0)
		{
			for (listOfStringsCI addIter = reqAttrsList.begin(); addIter != reqAttrsList.end(); ++addIter)
			{
				if (	(strcasecmp((*addIter).c_str(), recNameAttrType) != 0) ||		//if this is not the record name then we can add a default value
						(strcasecmp("sAMAccountName", recNameAttrType) == 0) )
				{
					CFStringRef aString = CFStringCreateWithCString(kCFAllocatorDefault, (*addIter).c_str(), kCFStringEncodingUTF8);
					CFDictionarySetValue( mergedDict, aString, valueDefaultArray );
					CFRelease(aString);
				}
			}
		}
		CFRelease(singleDefaultValue);
		CFRelease(valueDefaultArray);
		
		//now add in the defined attrs
		attrCount = (UInt32) CFDictionaryGetCount(inDict);
		char** keys = (char**)calloc( attrCount + 1, sizeof( void* ) );
		//get a linked list of the attrs so we can iterate over them
		CFDictionaryGetKeysAndValues( inDict, (const void**)keys, NULL );
		CFArrayRef valuesCFArray = NULL;
		for( keyIndex = 0; keyIndex < attrCount; keyIndex++ )
		{
			keyCFString = NULL;
			if ( !(CFGetTypeID((CFStringRef)keys[keyIndex]) == CFStringGetTypeID() ) )
			{
				continue; //skip this one and do not increment attrIndex
			}
			else
			{
				keyCFString = (CFStringRef)keys[keyIndex];
			}
			attrTypeStr = (char *)CFStringGetCStringPtr( keyCFString, kCFStringEncodingUTF8 );
			if (attrTypeStr == nil)
			{
				callocLength = (UInt32) CFStringGetMaximumSizeForEncoding(CFStringGetLength(keyCFString), kCFStringEncodingUTF8) + 1;
				attrTypeStr = (char *)calloc( 1, callocLength );
				if ( attrTypeStr == nil ) throw( eMemoryError );

				// Convert it to a regular 'C' string 
				bGotIt = CFStringGetCString( keyCFString, attrTypeStr, callocLength, kCFStringEncodingUTF8 );
				if (bGotIt == false) throw( eMemoryError );
			}

			//get the first mapping
			attrTypeLDAPStr = MapAttrToLDAPType( pContext, (const char *)inRecType, attrTypeStr, 1, true );
			//throw if first nil since we only use the first native type to write to
			//skip everything if a single one is incorrect?
			if ( attrTypeLDAPStr == nil ) throw( eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable

			valuesCFArray = (CFArrayRef)CFDictionaryGetValue( inDict, keyCFString );

			// record name is done in mod[0] above, so don't do it here
			if (strcasecmp(attrTypeLDAPStr, recNameAttrType) != 0)
			{	// merge in the attribute values
				CFStringRef aString = CFStringCreateWithCString(kCFAllocatorDefault, attrTypeLDAPStr, kCFStringEncodingUTF8);
				CFDictionarySetValue( mergedDict, aString, valuesCFArray );
				CFRelease(aString);
			}
			
			if (bGotIt)
			{
				DSFreeString(attrTypeStr);
			}
			DSFreeString(attrTypeLDAPStr);
		}
		
		//find out how many attrs need to be set
		attrCount = (UInt32) CFDictionaryGetCount(mergedDict);
		keys = (char**)calloc( attrCount + 1, sizeof( void* ) );
		//get a linked list of the attrs so we can iterate over them
		CFDictionaryGetKeysAndValues( mergedDict, (const void**)keys, NULL );
		
		//loop over attrs in the dictionary
		for( keyIndex = 0; keyIndex < attrCount; keyIndex++ )
		{
			keyCFString = NULL;
			if ( !(CFGetTypeID((CFStringRef)keys[keyIndex]) == CFStringGetTypeID() ) )
			{
				continue; //skip this one and do not increment modIndex
			}
			else
			{
				keyCFString = (CFStringRef)keys[keyIndex];
			}

			attrTypeStr = (char *)CFStringGetCStringPtr( keyCFString, kCFStringEncodingUTF8 );
			if (attrTypeStr == nil)
			{
				callocLength = (UInt32) CFStringGetMaximumSizeForEncoding(CFStringGetLength(keyCFString), kCFStringEncodingUTF8) + 1;
				attrTypeStr = (char *)calloc( 1, callocLength );
				if ( attrTypeStr == nil ) throw( eMemoryError );

				// Convert it to a regular 'C' string 
				bGotIt = CFStringGetCString( keyCFString, attrTypeStr, callocLength, kCFStringEncodingUTF8 );
				if (bGotIt == false) throw( eMemoryError );
			}

			valuesCFArray = (CFArrayRef)CFDictionaryGetValue( mergedDict, keyCFString );
			if ( !(CFGetTypeID(valuesCFArray) == CFArrayGetTypeID() ) )
			{
				if (bGotIt)
				{
					DSFreeString(attrTypeStr);
				}
				continue; //skip this one and free up the attr string if required
			}
			valCount	= (UInt32) CFArrayGetCount( valuesCFArray );
			newValues	= (struct berval**) calloc(1, (valCount + 1) *sizeof(struct berval *) );
			
			valIndex = 0;
			for (UInt32 i = 0; i < valCount; i++ ) //extract the values out of the valuesCFArray
			{
				//need to determine whether the value is either string or data
				CFTypeRef valueCFType = CFArrayGetValueAtIndex( valuesCFArray, (CFIndex)i );
				
				if ( CFGetTypeID(valueCFType) == CFStringGetTypeID() )
				{
					CFStringRef valueCFString = (CFStringRef)valueCFType;
					
					callocLength = (UInt32) CFStringGetMaximumSizeForEncoding(CFStringGetLength(valueCFString), kCFStringEncodingUTF8) + 1;
					attrValue = (char *)calloc( 1, callocLength );
					if ( attrValue == nil ) throw( eMemoryError );

					// Convert it to a regular 'C' string 
					bool bGotValue = CFStringGetCString( valueCFString, attrValue, callocLength, kCFStringEncodingUTF8 );
					if (bGotValue == false) throw( eMemoryError );

					attrLength = strlen(attrValue);
					newValues[valIndex] = (struct berval*) calloc(1, sizeof(struct berval) );
					newValues[valIndex]->bv_val = attrValue;
					newValues[valIndex]->bv_len = attrLength;
					valIndex++;
					attrValue = nil;
				}
				else if ( CFGetTypeID(valueCFType) == CFDataGetTypeID() )
				{
					CFDataRef valueCFData = (CFDataRef)valueCFType;
					
					attrLength = (UInt32) CFDataGetLength(valueCFData);
					attrValue = (char *)calloc( 1, attrLength + 1 );
					if ( attrValue == nil ) throw( eMemoryError );
					
					CFDataGetBytes( valueCFData, CFRangeMake(0,attrLength), (UInt8*)attrValue );

					newValues[valIndex] = (struct berval*) calloc(1, sizeof(struct berval) );
					newValues[valIndex]->bv_val = attrValue;
					newValues[valIndex]->bv_len = attrLength;
					valIndex++;
					attrValue = nil;
				}
			} // for each value provided
			
			if (valIndex > 0) //means we actually have something to add
			{
				//create this mods entry
				mods[modIndex] = (LDAPMod *) calloc( 1, sizeof(LDAPMod *) );
				mods[modIndex]->mod_op		= LDAP_MOD_ADD | LDAP_MOD_BVALUES;
				mods[modIndex]->mod_type	= strdup(attrTypeStr);
				mods[modIndex]->mod_bvalues= newValues;
				modIndex++;
				if (modIndex == 127) //we need terminating NULL for the list
				{
					if (bGotIt)
					{
						DSFreeString(attrTypeStr);
					}
					break; //this is all we modify ie. first 126 attrs in this set - we will never hit this
				}
			}
			if (bGotIt)
			{
				DSFreeString(attrTypeStr);
			}
			attrTypeStr = nil;
		} //for( UInt32 keyIndex = 0; keyIndex < attrCount; keyIndex++ )

		aHost = pContext->fLDAPConnection->LockLDAPSession();
		if (aHost != NULL)
		{
			ldapReturnCode = ldap_add_ext_s( aHost, ldapDNString, mods, NULL, NULL);
			pContext->fLDAPConnection->UnlockLDAPSession( aHost, false );
		}
		else
		{
			ldapReturnCode = LDAP_LOCAL_ERROR;
		}
		if ( ldapReturnCode == LDAP_ALREADY_EXISTS )
		{
			siResult = eDSRecordAlreadyExists;
		}
		else if ( ldapReturnCode != LDAP_SUCCESS )
		{
			siResult = MapLDAPWriteErrorToDS( ldapReturnCode, eDSRecordNotFound );
		}

		for (UInt32 modsIndex=0; ((modsIndex < 128) && (mods[modsIndex] != NULL)); modsIndex++)
		{
			DSFreeString(mods[modsIndex]->mod_type);
			if (modsIndex >= bValueStartIndex)
			{
				newValues = mods[modsIndex]->mod_bvalues;
				if (newValues != NULL)
				{
					ldap_value_free_len(newValues);
					newValues = NULL;
				}
			}
			DSFree( mods[modsIndex] );
		}
	}

	catch ( tDirStatus err )
	{
		siResult = err;
	}

	DSCFRelease(OCSearchList);
	DSFree(ocvals);
	DSFreeString( attrTypeLDAPStr );
	DSFreeString( pLDAPSearchBase );
	DSFreeString( recNameAttrType );
	DSFreeStringList( aValues );
	DSFreeStringList( aMaps );
	DSFreeString( ldapDNString );
	DSCFRelease(mergedDict);
	DSRelease( pContext );
		
	return( siResult );

} // CreateRecordWithAttributes


//------------------------------------------------------------------------------------
//	* RemoveAttributeValue
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::RemoveAttributeValue ( sRemoveAttributeValue *inData, const char *inRecTypeStr )
{
	tDirStatus				siResult		= eDSNoErr;
	tDataNodePtr			pAttrType		= nil;
	char				   *pLDAPAttrType	= nil;
	sLDAPContextData	   *pRecContext		= nil;
	LDAPMessage			   *result			= nil;
	struct berval		  **bValues			= nil;
	struct berval		  **newValues		= nil;
	UInt32					valCount		= 0;
	bool					bFoundIt		= false;
	int						numAttributes	= 1;
	UInt32					crcVal			= 0;
	LDAPMod				   *mods[2];
	int						ldapReturnCode	= 0;
	LDAP				   *aHost			= nil;
	char				  **attrs			= nil;

	try
	{
		pRecContext = gLDAPContextTable->GetObjectForRefNum( inData->fInRecRef );
		if ( pRecContext == nil ) throw( eDSBadContextData );
		
		CLDAPNodeConfig *nodeConfig = pRecContext->fLDAPConnection->fNodeConfig;
		if ( nodeConfig != NULL && nodeConfig->fLDAPv2ReadOnly ) throw( eDSReadOnly);

		if ( pRecContext->fOpenRecordDN == nil ) throw( eDSNullRecName );

		pAttrType = inData->fInAttrType;
		if ( pAttrType == nil ) throw( eDSEmptyAttributeType );
		if ( pAttrType->fBufferData == nil ) throw( eDSEmptyAttributeType );

		attrs = MapAttrToLDAPTypeArray( pRecContext, pRecContext->fOpenRecordType, pAttrType->fBufferData );
		if ( attrs == nil ) throw( eDSInvalidAttributeType );
		
		siResult = (tDirStatus)GetRecLDAPMessage( pRecContext, attrs, &result );
		if ( siResult != eDSNoErr ) throw( siResult );

		if ( strcmp(inRecTypeStr, kDSStdRecordTypeConfig) == 0 )
			IncrementChangeSeqNumber();

        if ( result != nil )
        {
        
			//get the first mapping
			numAttributes = 1;
			pLDAPAttrType = MapAttrToLDAPType( pRecContext, (const char *)(pRecContext->fOpenRecordType), pAttrType->fBufferData, numAttributes, true );
			//throw if first nil since no more will be found otherwise proceed until nil
			if ( pLDAPAttrType == nil ) throw( eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable

			//set indicator of multiple native to standard mappings
			bFoundIt = false;
			while ( ( pLDAPAttrType != nil ) && (!bFoundIt) )
			{
				valCount = 0; //separate count for each bvalues
				
				aHost = pRecContext->fLDAPConnection->LockLDAPSession();
				if ( aHost != NULL )
				{
					if ( (bValues = ldap_get_values_len(aHost, result, pLDAPAttrType)) != NULL )
					{
						// calc length of bvalues
						for (int i = 0; bValues[i] != NULL; i++ )
						{
							valCount++;
						}
						
						newValues = (struct berval**) calloc(1, (valCount + 1) *sizeof(struct berval *) );
						// for each value of the attribute
						UInt32 newIndex = 0;
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
				
					pRecContext->fLDAPConnection->UnlockLDAPSession( aHost, false );
				}
					
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
						aHost = pRecContext->fLDAPConnection->LockLDAPSession();
						if (aHost != NULL)
						{
							ldapReturnCode = ldap_modify_s( aHost, pRecContext->fOpenRecordDN, mods);
							pRecContext->fLDAPConnection->UnlockLDAPSession( aHost, false );
						}
						else
						{
							ldapReturnCode = LDAP_LOCAL_ERROR;
						}
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
				DSFree(newValues); //since newValues points to bValues
				
				//KW here we decide to opt out since we have removed one value already
				//ie. we could continue on to the next native mapping to find more
				//CRC ID matches and remove them as well by resetting bFoundIt
				//and allowing the stop condition to be the number of native types
								
				//cleanup pLDAPAttrType if needed
				DSFreeString(pLDAPAttrType);
				numAttributes++;
				//get the next mapping
				pLDAPAttrType = MapAttrToLDAPType( pRecContext, (const char *)(pRecContext->fOpenRecordType), pAttrType->fBufferData, numAttributes, true );
			} // while ( pLDAPAttrType != nil )
			
        } // retrieve the result from the LDAP server
        else
        {
        	throw( eDSRecordNotFound ); //KW???
        }

	}

	catch ( tDirStatus err )
	{
		siResult = err;
	}

	DSFreeString( pLDAPAttrType );
	DSFreeStringList( attrs );
	
	if (result != nil)
	{
		ldap_msgfree( result );
		result = nil;
	}
				
	DSRelease( pRecContext );
	
	return( siResult );

} // RemoveAttributeValue


//------------------------------------------------------------------------------------
//	* SetAttributeValues
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::SetAttributeValues ( sSetAttributeValues *inData, const char *inRecTypeStr )
{
	tDirStatus				siResult		= eDSNoErr;
	tDataNodePtr			pAttrType		= nil;
	char				   *pLDAPAttrType	= nil;
	sLDAPContextData	   *pRecContext		= nil;
	CFIndex					valCount		= 0;
	struct berval		   *replaceValue	= nil;
	struct berval		  **newValues		= nil;
	LDAPMod				   *mods[2];
	int						ldapReturnCode	= 0;
	char				   *attrValue		= nil;
	UInt32					attrLength		= 0;
	LDAP				   *aHost			= nil;

	try
	{
		pRecContext = gLDAPContextTable->GetObjectForRefNum( inData->fInRecRef );
		if ( pRecContext == nil ) throw( eDSBadContextData );
		
		CLDAPNodeConfig *nodeConfig = pRecContext->fLDAPConnection->fNodeConfig;
		if ( nodeConfig!= NULL && nodeConfig->fLDAPv2ReadOnly ) throw( eDSReadOnly);

		if ( pRecContext->fOpenRecordDN == nil ) throw( eDSNullRecName );
		if ( inData->fInAttrType == nil ) throw( eDSNullAttributeType );
		if ( inData->fInAttrValueList == nil ) throw( eDSNullAttributeValue ); //would like a plural constant for this
		if ( inData->fInAttrValueList->fDataNodeCount <= 0 ) throw( eDSNullAttributeValue ); //would like a plural constant for this
		
		pAttrType = inData->fInAttrType;
		if ( pAttrType == nil ) throw( eDSEmptyAttributeType );
		if ( pAttrType->fBufferData == nil ) throw( eDSEmptyAttributeType );

		if ( strcmp(inRecTypeStr, kDSStdRecordTypeConfig) == 0 )
			IncrementChangeSeqNumber();

		//get the first mapping
		pLDAPAttrType = MapAttrToLDAPType( pRecContext, (const char *)(pRecContext->fOpenRecordType), pAttrType->fBufferData, 1, true );
		//throw if first nil since we only use the first native type to write to
		if ( pLDAPAttrType == nil ) throw( eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable

		CFArrayRef  cfArray = dsConvertDataListToCFArray( inData->fInAttrValueList );
		UInt32      jIndex  = 0;
		
		if( NULL != cfArray )
		{
			valCount	= CFArrayGetCount( cfArray );
			newValues	= (struct berval**) calloc(1, ((UInt32)valCount + 1) *sizeof(struct berval *) );
			
			for ( CFIndex i = 0; i < valCount; i++ ) //extract the values out of the tDataList
			{
				CFDataRef   cfRef   = (CFDataRef) CFArrayGetValueAtIndex( cfArray, i );
				CFIndex     iLength = CFDataGetLength( cfRef );
				
				if( iLength > 0 )
				{
					attrLength = iLength;
					attrValue = (char *) calloc(1, 1 + attrLength);
					CFDataGetBytes( cfRef, CFRangeMake(0,iLength), (UInt8*)attrValue );
					
					replaceValue = (struct berval*) calloc(1, sizeof(struct berval) );
					replaceValue->bv_val = attrValue;
					replaceValue->bv_len = attrLength;
					newValues[jIndex] = replaceValue;
					jIndex++;
					attrValue = nil;
					replaceValue = nil;
				}
			} // for each value provided
			
			DSCFRelease( cfArray );
		}
		
		if (jIndex > 0) //means we actually have something to add
		{
			//here we set the newValues ie. remove the found one
			//create this mods entry
			LDAPMod	mod;
			mod.mod_op		= LDAP_MOD_REPLACE | LDAP_MOD_BVALUES;
			mod.mod_type	= pLDAPAttrType;
			mod.mod_bvalues	= newValues;
			mods[0]			= &mod;
			mods[1]			= NULL;

			aHost = pRecContext->fLDAPConnection->LockLDAPSession();
			if (aHost != NULL)
			{
				ldapReturnCode = ldap_modify_s( aHost, pRecContext->fOpenRecordDN, mods);
				pRecContext->fLDAPConnection->UnlockLDAPSession( aHost, false );
			}
			else
			{
				ldapReturnCode = LDAP_LOCAL_ERROR;
			}
			if ( ldapReturnCode != LDAP_SUCCESS )
			{
				siResult = MapLDAPWriteErrorToDS( ldapReturnCode, eDSAttributeNotFound );
			}
			
			if (newValues != NULL)
			{
				ldap_value_free_len(newValues);
				newValues = NULL;
			}
		}
	}

	catch ( tDirStatus err )
	{
		siResult = err;
	}

	DSFreeString( pLDAPAttrType );
	DSRelease( pRecContext );
		
	return( siResult );

} // SetAttributeValues


//------------------------------------------------------------------------------------
//	* SetAttributeValue
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::SetAttributeValue ( sSetAttributeValue *inData, const char *inRecTypeStr )
{
	tDirStatus				siResult		= eDSNoErr;
	tDataNodePtr			pAttrType		= nil;
	char				   *pLDAPAttrType	= nil;
	sLDAPContextData	   *pRecContext		= nil;
	LDAPMessage			   *result			= nil;
	UInt32					valCount		= 0;
	struct berval		  **bValues			= nil;
	struct berval			replaceValue;
	struct berval		  **newValues		= nil;
	bool					bFoundIt		= false;
	int						numAttributes	= 1;
	UInt32					crcVal			= 0;
	LDAPMod				   *mods[2];
	int						ldapReturnCode	= 0;
	char				   *attrValue		= nil;
	UInt32					attrLength		= 0;
	LDAP				   *aHost			= nil;
	char				  **attrs			= nil;

	try
	{
		pRecContext = gLDAPContextTable->GetObjectForRefNum( inData->fInRecRef );
		if ( pRecContext == nil ) throw( eDSBadContextData );
		
		CLDAPNodeConfig *nodeConfig = pRecContext->fLDAPConnection->fNodeConfig;
		if ( nodeConfig!= NULL && nodeConfig->fLDAPv2ReadOnly ) throw( eDSReadOnly);

		if ( pRecContext->fOpenRecordDN == nil ) throw( eDSNullRecName );
		if ( inData->fInAttrValueEntry == nil ) throw( eDSNullAttributeValue );
		
		//CRC ID is inData->fInAttrValueEntry->fAttributeValueID as used below

		//build the mods data entry to pass into replaceValue
		if (	(inData->fInAttrValueEntry->fAttributeValueData.fBufferData == NULL) ||
				(inData->fInAttrValueEntry->fAttributeValueData.fBufferLength < 1) )
		{
			//don't allow empty values since the ldap call will fail
			throw( eDSEmptyAttributeValue );
		}
		else
		{
			attrLength = inData->fInAttrValueEntry->fAttributeValueData.fBufferLength;
			attrValue = (char *) calloc(1, 1 + attrLength);
			memcpy(attrValue, inData->fInAttrValueEntry->fAttributeValueData.fBufferData, attrLength);
			
		}

		if ( strcmp(inRecTypeStr, kDSStdRecordTypeConfig) == 0 )
			IncrementChangeSeqNumber();

		pAttrType = inData->fInAttrType;
		if ( pAttrType == nil ) throw( eDSEmptyAttributeType );
		if ( pAttrType->fBufferData == nil ) throw( eDSEmptyAttributeType );

		attrs = MapAttrToLDAPTypeArray( pRecContext, pRecContext->fOpenRecordType, pAttrType->fBufferData );
		if ( attrs == nil ) throw( eDSInvalidAttributeType );
		
		siResult = (tDirStatus)GetRecLDAPMessage( pRecContext, attrs, &result );
		if ( siResult != eDSNoErr ) throw( siResult );

        if ( result != nil )
        {
        
			//get the first mapping
			numAttributes = 1;
			pLDAPAttrType = MapAttrToLDAPType( pRecContext, (const char *)(pRecContext->fOpenRecordType), pAttrType->fBufferData, numAttributes, true );
			//throw if first nil since no more will be found otherwise proceed until nil
			if ( pLDAPAttrType == nil ) throw( eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable

			//set indicator of multiple native to standard mappings
			bFoundIt = false;
			while ( ( pLDAPAttrType != nil ) && (!bFoundIt) )
			{
				valCount = 0; //separate count for each bvalues
				
				aHost = pRecContext->fLDAPConnection->LockLDAPSession();
				if ( aHost != NULL )
				{
					if ( (bValues = ldap_get_values_len(aHost, result, pLDAPAttrType)) != NULL )
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
				
					pRecContext->fLDAPConnection->UnlockLDAPSession( aHost, false );
				}
				
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

						aHost = pRecContext->fLDAPConnection->LockLDAPSession();
						if (aHost != NULL)
						{
							ldapReturnCode = ldap_modify_s( aHost, pRecContext->fOpenRecordDN, mods);
							pRecContext->fLDAPConnection->UnlockLDAPSession( aHost, false );
						}
						else
						{
							ldapReturnCode = LDAP_LOCAL_ERROR;
						}
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
				pLDAPAttrType = MapAttrToLDAPType( pRecContext, (const char *)(pRecContext->fOpenRecordType), pAttrType->fBufferData, numAttributes, true );
			} // while ( pLDAPAttrType != nil )
			
        } // retrieve the result from the LDAP server
        else
        {
        	throw( eDSRecordNotFound ); //KW???
        }

	}

	catch ( tDirStatus err )
	{
		siResult = err;
	}

	if ( result != nil )
	{
		ldap_msgfree( result );
		result = nil;
	}
				
	DSFreeString(pLDAPAttrType);
	DSFree(attrValue);
	DSFreeStringList(attrs);
	DSRelease( pRecContext );
	
	return( siResult );

} // SetAttributeValue


//------------------------------------------------------------------------------------
//	* ReleaseContinueData
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::ReleaseContinueData ( sReleaseContinueData *inData )
{
	tDirStatus	siResult	= eDSNoErr;
	
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

tDirStatus CLDAPv3Plugin::CloseAttributeList ( sCloseAttributeList *inData )
{
	tDirStatus				siResult		= eDSNoErr;
	sLDAPContextData	   *pContext		= nil;

	pContext = gLDAPContextTable->GetObjectForRefNum( inData->fInAttributeListRef );
	if ( pContext != nil )
	{
		//only "offset" should have been used in the Context
		gLDAPContextTable->RemoveRefNum( inData->fInAttributeListRef );
		DSRelease( pContext );
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

tDirStatus CLDAPv3Plugin::CloseAttributeValueList ( sCloseAttributeValueList *inData )
{
	tDirStatus				siResult		= eDSNoErr;
	sLDAPContextData	   *pContext		= nil;

	pContext = gLDAPContextTable->GetObjectForRefNum( inData->fInAttributeValueListRef );
	if ( pContext != nil )
	{
		//only "offset" should have been used in the Context
		gLDAPContextTable->RemoveRefNum( inData->fInAttributeValueListRef );
		DSRelease( pContext );
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

tDirStatus CLDAPv3Plugin::GetRecRefInfo ( sGetRecRefInfo *inData )
{
	tDirStatus		siResult	= eDSNoErr;
	UInt32			uiRecSize	= 0;
	tRecordEntry   *pRecEntry	= nil;
	sLDAPContextData   *pContext	= nil;
	char		   *refType		= nil;
    UInt32			uiOffset	= 0;
    char		   *refName		= nil;

	pContext = gLDAPContextTable->GetObjectForRefNum( inData->fInRecRef );
	if ( pContext == NULL )
		return( eDSBadContextData );

	// place in the record type from the context data of an OpenRecord
	if ( pContext->fOpenRecordType != nil)
	{
		refType = strdup( pContext->fOpenRecordType );
	}
	else //assume Record type of "Record Type Unknown"
	{
		refType = strdup( kLDAPRecordTypeUnknownStr );
	}
	
	//place in the record name from the context data of an OpenRecord
	if ( pContext->fOpenRecordName != nil)
	{
		refName = strdup( pContext->fOpenRecordName );
	}
	else //assume Record name of "Record Name Unknown"
	{
		refName = strdup( kLDAPRecordNameUnknownStr );
	}
	
	uiRecSize = sizeof( tRecordEntry ) + strlen( refType ) + strlen( refName ) + 4 + kBuffPad;
	pRecEntry = (tRecordEntry *)calloc( 1, uiRecSize );
	
	pRecEntry->fRecordNameAndType.fBufferSize	= strlen( refType ) + strlen( refName ) + 4 + kBuffPad;
	pRecEntry->fRecordNameAndType.fBufferLength	= strlen( refType ) + strlen( refName ) + 4;
	
	uiOffset = 0;
	UInt16 strLen = 0;
	// Add the record name length and name itself
	strLen = strlen( refName );
	memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, &strLen, 2);
	uiOffset += 2;
	memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, refName, strLen);
	uiOffset += strLen;
	
	// Add the record type length and type itself
	strLen = strlen( refType );
	memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, &strLen, 2);
	uiOffset += 2;
	memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, refType, strLen);
	uiOffset += strLen;

	inData->fOutRecInfo = pRecEntry;

	DSDelete( refType );
	DSDelete( refName );
	DSRelease( pContext );

	return( siResult );

} // GetRecRefInfo


//------------------------------------------------------------------------------------
//	* GetRecAttribInfo
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::GetRecAttribInfo ( sGetRecAttribInfo *inData )
{
	tDirStatus				siResult			= eDSNoErr;
	UInt32					uiTypeLen			= 0;
	UInt32					uiDataLen			= 0;
	tDataNodePtr			pAttrType			= nil;
	char				   *pLDAPAttrType		= nil;
	tAttributeEntryPtr		pOutAttrEntry		= nil;
	sLDAPContextData	   *pRecContext			= nil;
	LDAPMessage			   *result				= nil;
	struct berval		  **bValues;
	int						numAttributes		= 1;
	bool					bTypeFound			= false;
	int						valCount			= 0;
	LDAP			   		*aHost				= nil;
	char				  **attrs				= nil;
	
	try
	{
		pRecContext = gLDAPContextTable->GetObjectForRefNum( inData->fInRecRef );
		if ( pRecContext == nil ) throw( eDSBadContextData );

		pAttrType = inData->fInAttrType;
		if ( pAttrType == nil ) throw( eDSEmptyAttributeType );
		if ( pAttrType->fBufferData == nil ) throw( eDSEmptyAttributeType );

		attrs = MapAttrToLDAPTypeArray( pRecContext, pRecContext->fOpenRecordType, pAttrType->fBufferData );
		if ( attrs == nil ) throw( eDSInvalidAttributeType );
		
		siResult = (tDirStatus)GetRecLDAPMessage( pRecContext, attrs, &result );
		if ( siResult != eDSNoErr ) throw( siResult);

        if ( result != nil )
        {
			//get the first mapping
			numAttributes = 1;
			pLDAPAttrType = MapAttrToLDAPType( pRecContext, (const char *)(pRecContext->fOpenRecordType), pAttrType->fBufferData, numAttributes );
			//throw if first nil since no more will be found otherwise proceed until nil
			if ( pLDAPAttrType == nil ) throw( eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable

			//set indicator of multiple native to standard mappings
			bTypeFound = false;
			while ( pLDAPAttrType != nil )
			{
				if (!bTypeFound)
				{
					//set up the length of the attribute type
					uiTypeLen = strlen( pAttrType->fBufferData );
					pOutAttrEntry = (tAttributeEntry *)calloc( 1, sizeof( tAttributeEntry ) + uiTypeLen + kBuffPad );

					pOutAttrEntry->fAttributeSignature.fBufferSize		= uiTypeLen;
					pOutAttrEntry->fAttributeSignature.fBufferLength	= uiTypeLen;
					memcpy( pOutAttrEntry->fAttributeSignature.fBufferData, pAttrType->fBufferData, uiTypeLen ); 
					bTypeFound = true;
					valCount = 0;
					uiDataLen = 0;
				}
				
				if ( (pLDAPAttrType[0] == '#') && (strlen(pLDAPAttrType) > 1) )
				{
					char *vsReturnStr = nil;
					vsReturnStr = MappingNativeVariableSubstitution( pLDAPAttrType, pRecContext, result, siResult );
					
					if (vsReturnStr != nil)
					{
						valCount++;
						uiDataLen += (strlen(vsReturnStr) - 1);
						free( vsReturnStr );
						vsReturnStr = nil;
					}
					else
					{
						//if parsing error returned then we throw an error
						if (siResult != eDSNoErr) throw (siResult);
						valCount++;
						uiDataLen += (strlen(pLDAPAttrType) - 1);
					}
				}
				else
				{
					aHost = pRecContext->fLDAPConnection->LockLDAPSession();
					if ( aHost != NULL )
					{
						if ( (bValues = ldap_get_values_len(aHost, result, pLDAPAttrType)) != NULL )
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
						} // if ( aHost != NULL ) && bValues = ldap_get_values_len ...
						
						pRecContext->fLDAPConnection->UnlockLDAPSession( aHost, false );
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
				pLDAPAttrType = MapAttrToLDAPType( pRecContext, (const char *)(pRecContext->fOpenRecordType), pAttrType->fBufferData, numAttributes );
			} // while ( pLDAPAttrType != nil )

			if ( pOutAttrEntry == nil )
			{
				inData->fOutAttrInfoPtr = nil;
				throw( eDSAttributeNotFound );
			}
			// Number of attribute values
			if ( valCount > 0 )
			{
				pOutAttrEntry->fAttributeValueCount = valCount;
				//KW seems arbitrary max length
				pOutAttrEntry->fAttributeValueMaxSize = 255;
				//set the total length of all the attribute data
				pOutAttrEntry->fAttributeDataSize = uiDataLen;
				//assign the result out
				inData->fOutAttrInfoPtr = pOutAttrEntry;
			}
			else
			{
				free( pOutAttrEntry );
				pOutAttrEntry = nil;
				inData->fOutAttrInfoPtr = nil;
				siResult = eDSAttributeNotFound;
			}
			
        } // retrieve the result from the LDAP server
        else
        {
        	siResult = eDSRecordNotFound;
        }
	}

	catch ( tDirStatus err )
	{
		siResult = err;
	}

	if ( result != nil )
	{
		ldap_msgfree( result );
		result = nil;
	}
				
	DSFreeString(pLDAPAttrType);
	DSFreeStringList(attrs);
	DSRelease( pRecContext );
	
	return( siResult );

} // GetRecAttribInfo


//------------------------------------------------------------------------------------
//	* GetRecAttrValueByIndex
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::GetRecAttrValueByIndex ( sGetRecordAttributeValueByIndex *inData )
{
	tDirStatus				siResult				= eDSNoErr;
	UInt32					uiDataLen				= 0;
	tDataNodePtr			pAttrType				= nil;
	char				   *pLDAPAttrType			= nil;
	tAttributeValueEntryPtr	pOutAttrValue			= nil;
	sLDAPContextData	   *pRecContext				= nil;
	LDAPMessage			   *result					= nil;
	struct berval		  **bValues;
	UInt32					valCount				= 0;
	bool					bFoundIt				= false;
	int						numAttributes			= 1;
	UInt32					literalLength			= 0;
	bool					bStripCryptPrefix		= false;
	LDAP				   *aHost					= nil;
	char				  **attrs					= nil;

	try
	{
		if (inData->fInAttrValueIndex == 0) throw( eDSInvalidIndex );
		
		pRecContext = gLDAPContextTable->GetObjectForRefNum( inData->fInRecRef );
		if ( pRecContext == nil ) throw( eDSBadContextData );

		pAttrType = inData->fInAttrType;
		if ( pAttrType == nil ) throw( eDSEmptyAttributeType );
		if ( pAttrType->fBufferData == nil ) throw( eDSEmptyAttributeType );

		attrs = MapAttrToLDAPTypeArray( pRecContext, pRecContext->fOpenRecordType, pAttrType->fBufferData );
		if ( attrs == nil ) throw( eDSInvalidAttributeType );
		
		siResult = (tDirStatus)GetRecLDAPMessage( pRecContext, attrs, &result );
		if ( siResult != eDSNoErr ) throw( siResult);

        if ( result != nil )
        {
			if (strcmp( kDS1AttrPasswordPlus, pAttrType->fBufferData ) == 0)
			{
				//want to remove leading "{crypt}" prefix from password if it exists
				//iff the request came in for the DS std password type ie. not native
				bStripCryptPrefix = true;
			}
			
			//get the first mapping
			numAttributes = 1;
			pLDAPAttrType = MapAttrToLDAPType( pRecContext, (const char *)(pRecContext->fOpenRecordType), pAttrType->fBufferData, numAttributes );
			//throw if first nil since no more will be found otherwise proceed until nil
			if ( pLDAPAttrType == nil ) throw( eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable

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
						char *vsReturnStr = nil;
						vsReturnStr = MappingNativeVariableSubstitution( pLDAPAttrType, pRecContext, result, siResult );
					
						if (vsReturnStr != nil)
						{
							uiDataLen = strlen(vsReturnStr + 1);
							
							pOutAttrValue = (tAttributeValueEntry *)calloc( 1, sizeof( tAttributeValueEntry ) + uiDataLen + kBuffPad );

							pOutAttrValue->fAttributeValueData.fBufferSize = uiDataLen + kBuffPad;
							pOutAttrValue->fAttributeValueData.fBufferLength = uiDataLen;
							pOutAttrValue->fAttributeValueID = CalcCRC( pLDAPAttrType + 1 );
							memcpy( pOutAttrValue->fAttributeValueData.fBufferData, vsReturnStr + 1, uiDataLen );
							free(vsReturnStr);
							vsReturnStr = nil;
						}
						else
						{
							//if parsing error returned then we throw an error
							if (siResult != eDSNoErr) throw (siResult);
							// Append attribute value
							uiDataLen = literalLength;
							
							pOutAttrValue = (tAttributeValueEntry *)calloc( 1, sizeof( tAttributeValueEntry ) + uiDataLen + kBuffPad );
	
							pOutAttrValue->fAttributeValueData.fBufferSize = uiDataLen + kBuffPad;
							pOutAttrValue->fAttributeValueData.fBufferLength = uiDataLen;
							pOutAttrValue->fAttributeValueID = CalcCRC( pLDAPAttrType + 1 );
							memcpy( pOutAttrValue->fAttributeValueData.fBufferData, pLDAPAttrType + 1, uiDataLen );
						}
						
						bFoundIt = true;
					}
				}
				else
				{
					aHost = pRecContext->fLDAPConnection->LockLDAPSession();
					if ( aHost != NULL )
					{
						if ( (bValues = ldap_get_values_len(aHost, result, pLDAPAttrType)) != NULL )
						{
						
							// for each value of the attribute
							for (int i = 0; bValues[i] != NULL; i++ )
							{

								valCount++;
								if (valCount == inData->fInAttrValueIndex)
								{
									UInt32 anOffset = 0;
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
									
									pOutAttrValue = (tAttributeValueEntry *)calloc( 1, sizeof( tAttributeValueEntry ) + uiDataLen + kBuffPad );
		
									pOutAttrValue->fAttributeValueData.fBufferSize = uiDataLen + kBuffPad;
									pOutAttrValue->fAttributeValueData.fBufferLength = uiDataLen;
									if ( bValues[i]->bv_val != nil )
									{
										pOutAttrValue->fAttributeValueID = CalcCRC( bValues[i]->bv_val ); //no offset for CRC
										memcpy( pOutAttrValue->fAttributeValueData.fBufferData, bValues[i]->bv_val + anOffset, uiDataLen );
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
						
						pRecContext->fLDAPConnection->UnlockLDAPSession( aHost, false );
					}
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
				pLDAPAttrType = MapAttrToLDAPType( pRecContext, (const char *)(pRecContext->fOpenRecordType), pAttrType->fBufferData, numAttributes );
			} // while ( pLDAPAttrType != nil )
			
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

	catch ( tDirStatus err )
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
	
	if ( result != nil )
	{
		ldap_msgfree( result );
		result = nil;
	}
	
	DSRelease( pRecContext );

	return( siResult );

} // GetRecAttrValueByIndex


//------------------------------------------------------------------------------------
//	* GetRecAttrValueByValue
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::GetRecAttrValueByValue ( sGetRecordAttributeValueByValue *inData )
{
	tDirStatus				siResult				= eDSNoErr;
	UInt32					uiDataLen				= 0;
	tDataNodePtr			pAttrType				= nil;
	char				   *pLDAPAttrType			= nil;
	tAttributeValueEntryPtr	pOutAttrValue			= nil;
	sLDAPContextData	   *pRecContext				= nil;
	LDAPMessage			   *result					= nil;
	struct berval		  **bValues;
	bool					bFoundIt				= false;
	int						numAttributes			= 1;
	UInt32					literalLength			= 0;
	bool					bStripCryptPrefix		= false;
	LDAP			   		*aHost					= nil;
	char				  **attrs					= nil;
	char				   *attrValue				= nil;
	UInt32					attrLength				= 0;

	try
	{
		pRecContext = gLDAPContextTable->GetObjectForRefNum( inData->fInRecRef );
		if ( pRecContext == nil ) throw( eDSBadContextData );

		pAttrType = inData->fInAttrType;
		if ( pAttrType == nil ) throw( eDSEmptyAttributeType );
		if ( pAttrType->fBufferData == nil ) throw( eDSEmptyAttributeType );

		attrLength = inData->fInAttrValue->fBufferLength;
		attrValue = (char *) calloc(1, 1 + attrLength);
		memcpy(attrValue, inData->fInAttrValue->fBufferData, attrLength);

		attrs = MapAttrToLDAPTypeArray( pRecContext, pRecContext->fOpenRecordType, pAttrType->fBufferData );
		if ( attrs == nil ) throw( eDSInvalidAttributeType );
		
//call for each ldap attr type below and do not bother with this GetRecLDAPMessage call at all
// however, does not handle binary data
//       int ldap_compare_s(ld, dn, attr, value)
//       LDAP *ld;
//       char *dn, *attr, *value;
// success means LDAP_COMPARE_TRUE

		siResult = (tDirStatus)GetRecLDAPMessage( pRecContext, attrs, &result );
		if ( siResult != eDSNoErr ) throw( siResult);

        if ( result != nil )
        {
			if (strcmp( kDS1AttrPasswordPlus, pAttrType->fBufferData ) == 0)
			{
				//want to remove leading "{crypt}" prefix from password if it exists
				//iff the request came in for the DS std password type ie. not native
				bStripCryptPrefix = true;
			}
			
			//get the first mapping
			numAttributes = 1;
			pLDAPAttrType = MapAttrToLDAPType( pRecContext, (const char *)(pRecContext->fOpenRecordType), pAttrType->fBufferData, numAttributes );
			//throw if first nil since no more will be found otherwise proceed until nil
			if ( pLDAPAttrType == nil ) throw( eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable

			//set indicator of multiple native to standard mappings
			bFoundIt = false;
			while ( ( pLDAPAttrType != nil ) && (!bFoundIt) )
			{
				if (pLDAPAttrType[0] == '#')
				{
					literalLength = strlen(pLDAPAttrType + 1);
					if ( literalLength > 0 )
					{
						char *vsReturnStr = nil;
						vsReturnStr = MappingNativeVariableSubstitution( pLDAPAttrType, pRecContext, result, siResult );
					
						if ( (vsReturnStr != nil)  && (memcmp(vsReturnStr, attrValue, attrLength) == 0 ) )
						{
							bFoundIt = true;
							uiDataLen = strlen(vsReturnStr + 1);
							
							pOutAttrValue = (tAttributeValueEntry *)calloc( 1, sizeof( tAttributeValueEntry ) + uiDataLen + kBuffPad );

							pOutAttrValue->fAttributeValueData.fBufferSize = uiDataLen + kBuffPad;
							pOutAttrValue->fAttributeValueData.fBufferLength = uiDataLen;
							pOutAttrValue->fAttributeValueID = CalcCRC( pLDAPAttrType + 1 );
							memcpy( pOutAttrValue->fAttributeValueData.fBufferData, vsReturnStr + 1, uiDataLen );
							free(vsReturnStr);
							vsReturnStr = nil;
						}
					}
				}
				else
				{
					aHost = pRecContext->fLDAPConnection->LockLDAPSession();
					if ( aHost != NULL )
					{
						if ( (bValues = ldap_get_values_len(aHost, result, pLDAPAttrType)) != NULL )
						{
							// for each value of the attribute
							for (int i = 0; bValues[i] != NULL; i++ )
							{
								UInt32 anOffset = 0;
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
								if (memcmp((bValues[i]->bv_val)+anOffset, attrValue, attrLength-anOffset) == 0 )
								{
									// Append attribute value
									uiDataLen = bValues[i]->bv_len - anOffset;
									
									pOutAttrValue = (tAttributeValueEntry *)calloc( 1, sizeof( tAttributeValueEntry ) + uiDataLen + kBuffPad );
		
									pOutAttrValue->fAttributeValueData.fBufferSize = uiDataLen + kBuffPad;
									pOutAttrValue->fAttributeValueData.fBufferLength = uiDataLen;
									if ( bValues[i]->bv_val != nil )
									{
										pOutAttrValue->fAttributeValueID = CalcCRC( bValues[i]->bv_val ); //no offset for CRC
										memcpy( pOutAttrValue->fAttributeValueData.fBufferData, bValues[i]->bv_val + anOffset, uiDataLen );
									}
		
									bFoundIt = true;
									break;
								}
							} // for each bValues[i]
							if (bValues != NULL)
							{
								ldap_value_free_len(bValues);
								bValues = NULL;
							}
						} // if bValues = ldap_get_values_len ...
						
						pRecContext->fLDAPConnection->UnlockLDAPSession( aHost, false );
					}
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
				pLDAPAttrType = MapAttrToLDAPType( pRecContext, (const char *)(pRecContext->fOpenRecordType), pAttrType->fBufferData, numAttributes );
			} // while ( pLDAPAttrType != nil )
			
	        if (!bFoundIt)
	        {
				siResult = eDSAttributeNotFound;
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

	catch ( tDirStatus err )
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
	if (attrValue != nil)
	{
		free(attrValue);
		attrValue = nil;
	}
	
	if ( result != nil )
	{
		ldap_msgfree( result );
		result = nil;
	}
	
	DSRelease( pRecContext );

	return( siResult );

} // GetRecAttrValueByValue


//------------------------------------------------------------------------------------
//	* GetRecAttrValueByID
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::GetRecAttrValueByID ( sGetRecordAttributeValueByID *inData )
{
	tDirStatus				siResult			= eDSAttributeNotFound;
	UInt32					uiDataLen			= 0;
	tDataNodePtr			pAttrType			= nil;
	char				   *pLDAPAttrType		= nil;
	tAttributeValueEntryPtr	pOutAttrValue		= nil;
	sLDAPContextData	   *pRecContext			= nil;
	LDAPMessage			   *result				= nil;
	struct berval		  **bValues;
	bool					bFoundIt			= false;
	int						numAttributes		= 1;
	UInt32					crcVal				= 0;
	UInt32					literalLength		= 0;
	bool					bStripCryptPrefix   = false;
	LDAP			   		*aHost				= nil;
	char				  **attrs				= nil;

	try
	{
		pRecContext = gLDAPContextTable->GetObjectForRefNum( inData->fInRecRef );
		if ( pRecContext == nil ) throw( eDSBadContextData );

		pAttrType = inData->fInAttrType;
		if ( pAttrType == nil ) throw( eDSEmptyAttributeType );
		if ( pAttrType->fBufferData == nil ) throw( eDSEmptyAttributeType );

		attrs = MapAttrToLDAPTypeArray( pRecContext, pRecContext->fOpenRecordType, pAttrType->fBufferData );
		if ( attrs == nil ) throw( eDSInvalidAttributeType );
		
		siResult = (tDirStatus)GetRecLDAPMessage( pRecContext, attrs, &result );
		if ( siResult != eDSNoErr ) throw( siResult);

        if ( result != nil )
        {
			if (strcmp( kDS1AttrPasswordPlus, pAttrType->fBufferData ) == 0)
			{
				//want to remove leading "{crypt}" prefix from password if it exists
				//iff the request came in for the DS std password type ie. not native
				bStripCryptPrefix = true;
			}
			
			//get the first mapping
			numAttributes = 1;
			pLDAPAttrType = MapAttrToLDAPType( pRecContext, (const char *)(pRecContext->fOpenRecordType), pAttrType->fBufferData, numAttributes );
			//throw if first nil since no more will be found otherwise proceed until nil
			if ( pLDAPAttrType == nil ) throw( eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable

			siResult = eDSAttributeValueNotFound;
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
							char *vsReturnStr = nil;
							vsReturnStr = MappingNativeVariableSubstitution( pLDAPAttrType, pRecContext, result, siResult );
					
							if (vsReturnStr != nil)
							{
								uiDataLen = strlen(vsReturnStr + 1);
								pOutAttrValue = (tAttributeValueEntry *)calloc( 1, sizeof( tAttributeValueEntry ) + uiDataLen + kBuffPad );
		
								pOutAttrValue->fAttributeValueData.fBufferSize = uiDataLen + kBuffPad;
								pOutAttrValue->fAttributeValueData.fBufferLength = uiDataLen;
								pOutAttrValue->fAttributeValueID = crcVal;
								memcpy( pOutAttrValue->fAttributeValueData.fBufferData, vsReturnStr + 1, uiDataLen );
								free(vsReturnStr);
								vsReturnStr = nil;
							}
							else
							{
								//if parsing error returned then we throw an error
								if (siResult != eDSNoErr) throw (siResult);
								// Append attribute value
								uiDataLen = literalLength;
								
								pOutAttrValue = (tAttributeValueEntry *)calloc( 1, sizeof( tAttributeValueEntry ) + uiDataLen + kBuffPad );
		
								pOutAttrValue->fAttributeValueData.fBufferSize = uiDataLen + kBuffPad;
								pOutAttrValue->fAttributeValueData.fBufferLength = uiDataLen;
								pOutAttrValue->fAttributeValueID = crcVal;
								memcpy( pOutAttrValue->fAttributeValueData.fBufferData, pLDAPAttrType + 1, uiDataLen );
							}
							
							bFoundIt = true;
							siResult = eDSNoErr;
						}
					}
				}
				else
				{
					aHost = pRecContext->fLDAPConnection->LockLDAPSession();
					if ( aHost != NULL )
					{
						if ( (bValues = ldap_get_values_len(aHost, result, pLDAPAttrType)) != NULL )
						{
						
							// for each value of the attribute
							for (int i = 0; bValues[i] != NULL; i++ )
							{
								//use CRC here - WITF we assume string??
								crcVal = CalcCRC( bValues[i]->bv_val );
								if ( crcVal == inData->fInValueID )
								{
									UInt32 anOffset = 0;
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
									
									pOutAttrValue = (tAttributeValueEntry *)calloc( 1, sizeof( tAttributeValueEntry ) + uiDataLen + kBuffPad );
									pOutAttrValue->fAttributeValueData.fBufferSize = uiDataLen + kBuffPad;
									pOutAttrValue->fAttributeValueData.fBufferLength = uiDataLen;
									if ( bValues[i]->bv_val != nil )
									{
										pOutAttrValue->fAttributeValueID = crcVal;
										memcpy( pOutAttrValue->fAttributeValueData.fBufferData, bValues[i]->bv_val + anOffset, uiDataLen );
									}
		
									bFoundIt = true;
									siResult = eDSNoErr;
									break;
								} // if ( crcVal == inData->fInValueID )
							} // for each bValues[i]
							ldap_value_free_len(bValues);
							bValues = NULL;
						} // if bValues = ldap_get_values_len ...
						pRecContext->fLDAPConnection->UnlockLDAPSession( aHost, false );
					}
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
				pLDAPAttrType = MapAttrToLDAPType( pRecContext, (const char *)(pRecContext->fOpenRecordType), pAttrType->fBufferData, numAttributes );
			} // while ( pLDAPAttrType != nil )
			
        } // retrieve the result from the LDAP server

		if (pLDAPAttrType != nil)
		{
			delete( pLDAPAttrType );
			pLDAPAttrType = nil;
		}				
			
	}

	catch ( tDirStatus err )
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
	
	if ( result != nil )
	{
		ldap_msgfree( result );
		result = nil;
	}
	
	DSRelease( pRecContext );
	
	return( siResult );

} // GetRecordAttributeValueByID


// ---------------------------------------------------------------------------
//	* GetNextStdAttrType
// ---------------------------------------------------------------------------

char* CLDAPv3Plugin::GetNextStdAttrType ( char *inRecType, sLDAPContextData *inContext, int &inputIndex )
{
	char	*outResult	= nil;

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
		outResult = ExtractStdAttrName( inContext, inRecType, inputIndex );
	}// if (inIndex > 0)

	return( outResult );

} // GetNextStdAttrType


//------------------------------------------------------------------------------------
//	* DoAttributeValueSearch
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::DoAttributeValueSearch ( sDoAttrValueSearch *inData )
{
	return this->DoAttributeValueSearchWithData( (sDoAttrValueSearchWithData *)inData );
}


//------------------------------------------------------------------------------------
//	* DoAttributeValueSearch
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::DoAttributeValueSearchWithData( sDoAttrValueSearchWithData *inData )
{
    tDirStatus				siResult			= eDSNoErr;
	SInt32					siCBuffErr			= 0;
    bool					bAttribOnly			= false;
    UInt32					uiCount				= 0;
    UInt32					uiTotal				= 0;
    char				  **pRecTypes			= nil;
    CAttributeList		   *cpAttrTypeList 		= nil;
    char				  **pSearchStrings		= nil;
    char				   *pAttrType			= nil;
    char				   *pLDAPSearchBase		= nil;
    tDirPatternMatch		pattMatch			= eDSExact;
    sLDAPContextData	   *pContext			= nil;
    sLDAPContinueData	   *pLDAPContinue		= nil;
    CBuff				   *outBuff				= nil;
	tDataList			   *pTmpDataList		= nil;
    UInt32					countDownRecTypes	= 0;
	bool					bOCANDGroup			= false;
	CFArrayRef				OCSearchList		= nil;
	ber_int_t				scope				= LDAP_SCOPE_SUBTREE;
	UInt32					strCount			= 0;

	// Verify all the parameters
	if ( inData == nil ) return( eMemoryError );
	if ( inData->fOutDataBuff == nil ) return( eDSEmptyBuffer );
	if ( inData->fOutDataBuff->fBufferSize == 0 ) return( eDSEmptyBuffer );
	if ( inData->fInRecTypeList == nil ) return( eDSEmptyRecordTypeList );
	
	// Node context data
	pContext = gLDAPContextTable->GetObjectForRefNum( inData->fInNodeRef );
	if ( pContext == nil ) return( eDSBadContextData );
	if ( pContext->fLDAPConnection->ConnectionStatus() != kConnectionSafe ) return eDSCannotAccessSession;

    try
    {
		if ( inData->fIOContinueData == nil )
		{
			pLDAPContinue = (sLDAPContinueData *) calloc( 1, sizeof(sLDAPContinueData) );

			gLDAPContinueTable->AddItem( pLDAPContinue, inData->fInNodeRef );

            //parameters used for data buffering
			pLDAPContinue->fLDAPConnection = pContext->fLDAPConnection->Retain();
			pLDAPContinue->fNodeRef = inData->fInNodeRef;
            pLDAPContinue->fRecNameIndex = 1;
            pLDAPContinue->fRecTypeIndex = 1;
            pLDAPContinue->fTotalRecCount = 0;
            pLDAPContinue->fLimitRecSearch = (inData->fOutMatchRecordCount >= 0 ? inData->fOutMatchRecordCount : 0);
		}
		else
		{
			pLDAPContinue = (sLDAPContinueData *)inData->fIOContinueData;
			if ( gLDAPContinueTable->VerifyItem( pLDAPContinue ) == false ) throw( eDSInvalidContinueData );
		}

        // start with the continue set to nil until buffer gets full and there is more data
        //OR we have more record types to look through
        inData->fIOContinueData			= nil;
		//return zero here if nothing found
		inData->fOutMatchRecordCount	= 0;

        // copy the buffer data into a more manageable form
        outBuff = new CBuff();
        if ( outBuff == nil ) throw( eMemoryError );

        siCBuffErr = outBuff->Initialize( inData->fOutDataBuff, true );
        if ( siCBuffErr != eDSNoErr ) throw( (tDirStatus)siCBuffErr );

		if ( (siCBuffErr = outBuff->GetBuffStatus()) != eDSNoErr )
			throw( (tDirStatus)siCBuffErr );
		if ( (siCBuffErr = outBuff->SetBuffType('StdA')) != eDSNoErr )
			throw( (tDirStatus)siCBuffErr );
		
        // Get the record type list
		pRecTypes = dsAllocStringsFromList(0, inData->fInRecTypeList);
		if ( pRecTypes == nil ) throw( eDSEmptyRecordTypeList );
		bool bResetRecTypes = false;
		while(pRecTypes[strCount] != nil)
		{
			if (strcmp(pRecTypes[strCount], kDSStdRecordTypeAll) == 0)
			{
				bResetRecTypes = true;
			}
			strCount++;
		}
        if (strCount == 0) throw( eDSEmptyRecordTypeList );
		if (bResetRecTypes && pContext->fLDAPConnection->fNodeConfig != NULL )
		{
			//in this case we now do the following:
			//A:TODO try to retrieve the native record type from the search base suffix in the config
			//B:TODO if not present then we compare suffixes of the native record types for users, groups, computers and use that
			//C:otherwise we fall back on brute force search through the remaining std record types
			//TODO we only build this list of native record types once for a query and save it in the continue data
			
			//C:
			//mapping rec types - if std to native
			DSFreeStringList(pRecTypes);
			strCount = 0;
			UInt32 i = 0;
												
			CFDictionaryRef cfNormalizedMap = pContext->fLDAPConnection->fNodeConfig->CopyNormalizedMappings();
			if( cfNormalizedMap != nil )
			{
				strCount = CFDictionaryGetCount( cfNormalizedMap );
				if( strCount )
				{
					CFStringRef	*keys = (CFStringRef *) calloc( strCount, sizeof(CFStringRef) );
					pRecTypes = (char **) calloc(strCount+1, sizeof(char *));
					
					CFDictionaryGetKeysAndValues( cfNormalizedMap, (const void **)keys, NULL );
					
					for( i = 0; i < strCount; i++ )
					{
						CFIndex uiLength = CFStringGetMaximumSizeForEncoding( CFStringGetLength(keys[i]), kCFStringEncodingUTF8 );
						pRecTypes[i] = (char *) calloc( uiLength, sizeof(char) );
						CFStringGetCString( keys[i], pRecTypes[i], uiLength, kCFStringEncodingUTF8 );
					}
					
					DSFree( keys );
				}
				
				DSCFRelease( cfNormalizedMap );
			}
			// if we didn't end up with a list, then let's throw an error
			if (strCount == 0) throw( eDSEmptyRecordTypeList );
		}
		
        //save the number of rec types here to use in separating the buffer data
        countDownRecTypes = strCount - pLDAPContinue->fRecTypeIndex + 1;

        // Get the attribute pattern match type
        pattMatch = inData->fInPattMatchType;

        // Get the attribute type
		pAttrType = inData->fInAttrType->fBufferData;
		if (	(pattMatch == eDSCompoundExpression) || 
				(pattMatch == eDSiCompoundExpression) )
		{
			pAttrType = ""; //use fake string since pAttrType is used in strcmp below
		}
		else
		{
			if ( pAttrType == nil ) throw( eDSEmptyAttributeType );
		}

        // Get the attribute string match
		if ( inData->fType == kDoAttributeValueSearchWithData || inData->fType == kDoAttributeValueSearch )
		{
			pSearchStrings = (char **) calloc(2, sizeof(char *));
			pSearchStrings[0] = strdup(inData->fInPatt2Match->fBufferData);
			if ( pSearchStrings == nil ) throw( eDSEmptyPattern2Match );
		}
		else
		{
			// otherwise this is a MultiAttrValueSearch
			pSearchStrings = dsAllocStringsFromList(0, ((sDoMultiAttrValueSearchWithData *) inData)->fInPatterns2MatchList);
			if ( pSearchStrings == nil ) throw( eDSEmptyPattern2Match );
		}

		if ( inData->fType == kDoAttributeValueSearchWithData || inData->fType == kDoMultipleAttributeValueSearchWithData )
		{
			// Get the attribute list
			cpAttrTypeList = new CAttributeList( inData->fInAttrTypeRequestList );
			
			// Get the attribute info only flag
			bAttribOnly = inData->fInAttrInfoOnly;
		}
		else
		{
			pTmpDataList = dsBuildListFromStringsPriv( kDSAttributesAll, nil );
			if ( pTmpDataList != nil )
				cpAttrTypeList = new CAttributeList( pTmpDataList );
			}
		
		if ( cpAttrTypeList == nil || cpAttrTypeList->GetCount() == 0 )
			throw( eDSEmptyAttributeTypeList );
		
		// get records of these types
        while ( siResult == eDSNoErr && pRecTypes[pLDAPContinue->fRecTypeIndex-1] != NULL )
        {
            pLDAPSearchBase = MapRecToSearchBase( pContext, pRecTypes[pLDAPContinue->fRecTypeIndex-1], 1, &bOCANDGroup, &OCSearchList, &scope );
            if ( pLDAPSearchBase == NULL )
			{
				pLDAPContinue->fRecTypeIndex++;
				continue;
			}
			
			if ( (strcmp( pAttrType, kDSAttributesAll ) == 0 ) ||
				 (strcmp( pAttrType, kDSAttributesStandardAll ) == 0 ) ||
				 (strcmp( pAttrType, kDSAttributesNativeAll ) == 0 ) )
			{
				//go get me all records that have any attribute equal to pSearchStrings with pattMatch constraint
				//KW this is a very difficult search to do
				//approach A: set up a very complex search filter to pass to the LDAP server
				//need to be able to handle all standard types that are mapped
				//CHOSE THIS approach B: get each record and parse it completely using native attr types
				//approach C: just like A but concentrate on a selected subset of attr types
				siResult = GetTheseRecords( nil, pSearchStrings, pRecTypes[pLDAPContinue->fRecTypeIndex-1], pLDAPSearchBase, 
										    pattMatch, cpAttrTypeList, pContext, pLDAPContinue, bAttribOnly, outBuff, uiCount, 
										    bOCANDGroup, OCSearchList, scope );
			}
			else
			{
				//go get me all records that have pAttrType equal to pSearchStrings with pattMatch constraint
				siResult = GetTheseRecords( pAttrType, pSearchStrings, pRecTypes[pLDAPContinue->fRecTypeIndex-1], pLDAPSearchBase,
										    pattMatch, cpAttrTypeList, pContext, pLDAPContinue, bAttribOnly, outBuff, uiCount, 
										    bOCANDGroup, OCSearchList, scope );
			}
			
			bOCANDGroup = false;
			DSDelete( pLDAPSearchBase );
			DSCFRelease( OCSearchList );
			
			// add the found records to the total
			uiTotal += uiCount;
			
			// see if we got a buffer too small, it just means we couldn't put any from the search
			// but there may already be some in the buffer
			if ( siResult == eDSBufferTooSmall )
			{
				//set continue if there is more data available
				inData->fIOContinueData = pLDAPContinue;
				
				// if we've put some records we change the error
				if ( uiTotal != 0 )
					siResult = eDSNoErr;
				break;
			}
			else if ( siResult == eDSNoErr )
			{
				pLDAPContinue->fRecTypeIndex++;
			}
        } // while loop over record types
		
		if ( uiTotal == 0 )
			outBuff->ClearBuff();
		
    } // try
    
    catch ( tDirStatus err )
    {
		siResult = err;
    }
	
	if ( uiTotal > 0 )
	{
		inData->fOutMatchRecordCount = uiTotal;
		outBuff->SetLengthToSize();
	}

	if ( (inData->fIOContinueData == nil) && (pLDAPContinue != nil) )
	{
		// we've decided not to return continue data, so we should clean up
		gLDAPContinueTable->RemoveItem( pLDAPContinue );
		pLDAPContinue = nil;
	}

	DSDelete( pLDAPSearchBase );
	DSDelete( cpAttrTypeList );
    DSDelete( outBuff );
	DSFreeStringList( pSearchStrings );
	DSFreeStringList( pRecTypes );
	DSRelease( pContext );

	if ( pTmpDataList != nil )
	{
		dsDataListDeallocatePriv( pTmpDataList );
		free(pTmpDataList);
		pTmpDataList = nil;
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
	aHost = inContext->fLDAPConnection->LockLDAPSession();
	if (aHost != NULL)
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
		inContext->fLDAPConnection->UnlockLDAPSession( aHost, false );
	} // if aHost != NULL
	
	if (ber != nil)
	{
		ber_free( ber, 0 );
	}

	return( bFoundMatch );

} // DoTheseAttributesMatch


//------------------------------------------------------------------------------------
//	* DoAnyOfTheseAttributesMatch
//
//	sns: This routine gets pounded and needs good performance. Shark was
//	showing a lot of time in calloc() so I've switched to malloc() and
//	explicitly terminate the strings.
//------------------------------------------------------------------------------------

bool CLDAPv3Plugin::DoAnyOfTheseAttributesMatch(	sLDAPContextData	   *inContext,
													char				  **inAttrNames,
													tDirPatternMatch		pattMatch,
													LDAPMessage			   *inResult)
{
	char			   *pAttr					= nil;
	char			   *pVal					= nil;
	BerElement		   *ber						= nil;
	struct berval	  **bValues;
	bool				bFoundMatch				= false;
	LDAP			   *aHost					= nil;
	ber_len_t			bvlen					= 0;
	
	switch ( pattMatch )
	{
		case eDSCompoundExpression:
		case eDSWildCardPattern:
		case eDSRegularExpression:
		case eDSiCompoundExpression:
		case eDSiWildCardPattern:
		case eDSiRegularExpression:
		case eDSAnyMatch:
			return true;
		default:
			break;

	}
	
	//let's check all the attribute values for a match on the input name
	//with the given patt match constraint - first match found we stop and
	//then go get it all
	//TODO - room for optimization here
	aHost = inContext->fLDAPConnection->LockLDAPSession();
	if ( aHost != NULL )
	{
		if ( inResult != nil )
		{
			for (	pAttr = ldap_first_attribute(aHost, inResult, &ber);
					pAttr != NULL; pAttr = ldap_next_attribute(aHost, inResult, ber) )
			{
				if ( (bValues = ldap_get_values_len(aHost, inResult, pAttr)) != NULL)
				{
					// for each value of the attribute
					for ( int i = 0; bValues[i] != NULL; i++ )
					{
						//need this since bValues might be binary data with no NULL terminator
						pVal = (char *) malloc((bvlen = bValues[i]->bv_len) + 1);
						memcpy(pVal, bValues[i]->bv_val, bvlen);
						pVal[bvlen] = '\0';
						
						if (DoAnyMatch(pVal, inAttrNames, pattMatch))
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
		} // if aHost != NULL
		inContext->fLDAPConnection->UnlockLDAPSession( aHost, false );
	}
	
	if (ber != nil)
	{
		ber_free( ber, 0 );
	}

	return( bFoundMatch );

} // DoAnyOfTheseAttributesMatch


// ---------------------------------------------------------------------------
//	* DoAnyMatch
//
//	sns: This routine gets pounded and needs good performance. Shark was
//	showing a lot of time in calloc() so I've switched to malloc() and
//	explicitly terminate the strings.
// ---------------------------------------------------------------------------

bool CLDAPv3Plugin::DoAnyMatch (	const char		   *inString,
									char			  **inPatterns,
									tDirPatternMatch	inPattMatch )
{
	const char	   *p			= nil;
	bool			bMatched	= false;
	char		   *string1;
	char		   *string2;
	UInt32			length1		= 0;
	UInt32			length2		= 0;
	UInt32			uMatch		= 0;
	UInt16			usIndex		= 0;

	if ( (inString == nil) || (inPatterns == nil) )
		return( false );
	
	uMatch = (UInt32) inPattMatch;

	length1 = strlen(inString);
	if ( (inPattMatch >= eDSExact) && (inPattMatch <= eDSRegularExpression) )
	{
		string1 = strdup(inString);
	}
	else
	{
		string1 = (char *) malloc(length1 + 1);
		p = inString;
		for ( usIndex = 0; usIndex < length1; usIndex++ )
			string1[usIndex] = toupper( *p++ );
		string1[length1] = '\0';
	}
	
	UInt32 strCount = 0;
	while(inPatterns[strCount] != nil)
	{
		length2 = strlen(inPatterns[strCount]);
		if ( (inPattMatch >= eDSExact) && (inPattMatch <= eDSRegularExpression) )
		{
			string2 = strdup( inPatterns[strCount] );
		}
		else
		{
			string2 = (char *) malloc(length2 + 1);
			p = inPatterns[strCount];
			for ( usIndex = 0; usIndex < length2; usIndex++  )
				string2[usIndex] = toupper( *p++ );
			string2[length2] = '\0';
		}
		
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
		
		if (string2 != nil)
		{
			delete(string2);
		}
		
		if (bMatched)
		{
			break;
		}
		strCount++;
	} //while(inPatterns[strCount] != nil)
	
	DSFreeString(string1);

	return( bMatched );

} // DoAnyMatch


//------------------------------------------------------------------------------------
//	* SetAttributeValueForDN
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::SetAttributeValueForDN( sLDAPContextData *inContext, char *inDN, const char *inRecordType, char *inAttrType, char **inValues )
{
	tDirStatus	siResult		= eDSNoErr;
	char		*pLDAPAttrType	= NULL;
	
	// if we have values we can continue..
	if( inValues != NULL && *inValues != NULL )
	{
		//get the first mapping
		pLDAPAttrType = MapAttrToLDAPType( inContext, (const char *)inRecordType, inAttrType, 1, true );

		// if we have a mapping and we have values
		if( pLDAPAttrType != NULL )
		{
			LDAPMod		stMod			= { 0 };
			int			ldapReturnCode	= 0;
			berval		**bvals			= NULL;
			LDAPMod		*pMods[2];

			// first let's build our berEntries
			while( *inValues )
			{
				berval *bval = (berval *) calloc( sizeof(berval), 1 );
				if( bval != NULL )  // if a calloc fails we have bigger issues, we'll just stop here.. set siResult
				{
					bval->bv_val = *inValues;
					bval->bv_len = strlen( *inValues );
					ber_bvecadd( &bvals, bval );
				}
				else
				{
					siResult = eMemoryAllocError;
					break;
				}
				inValues++;
			};
			
			// if we have bvals and we didn't get a memoryallocation error
			if( bvals != NULL && siResult == eDSNoErr )
			{
				// now, create the mod entry
				stMod.mod_op		= LDAP_MOD_REPLACE | LDAP_MOD_BVALUES;
				stMod.mod_type		= pLDAPAttrType;
				stMod.mod_bvalues   = bvals;
				
				pMods[0]			= &stMod;
				pMods[1]			= NULL;
				
				// now let's get our LDAP session and do the work..
				LDAP *aHost = inContext->fLDAPConnection->LockLDAPSession();
				if( aHost != NULL )
				{
					ldapReturnCode = ldap_modify_s( aHost, inDN, pMods );
					inContext->fLDAPConnection->UnlockLDAPSession( aHost, false );
				}
				else
				{
					ldapReturnCode = LDAP_LOCAL_ERROR;
				}
				
				if( ldapReturnCode != LDAP_SUCCESS )
				{
					siResult = MapLDAPWriteErrorToDS( ldapReturnCode, eDSAttributeNotFound );
				}
			}
			
			delete( pLDAPAttrType );
			pLDAPAttrType = NULL;
			
			if( bvals )
			{
				ber_memvfree( (void **) bvals );
				bvals = NULL;
			}			
		} // if( pLDAPAttrType != NULL )
		else
		{
			siResult = eDSNoStdMappingAvailable;
		}
	}
	else
	{
		siResult = eDSNullParameter;
	}
	
	return( siResult );
} // SetAttributeValueForDN


//------------------------------------------------------------------------------------
//	* GetAuthAuthority
//------------------------------------------------------------------------------------

SInt32
CLDAPv3Plugin::GetAuthAuthority( sLDAPContextData *inContext, const char *userName, int inUserNameBufferLength,
	unsigned long *outAuthCount, char **outAuthAuthority[], const char *inRecordType )
{
	tDataBufferPtr		aaBuffer		= NULL;

	if ( userName == NULL )
		return eDSNullDataBuff;
	
	int nameLen = strlen( userName );
	if ( inUserNameBufferLength > nameLen + (int)sizeof(kDSNameAndAATag) &&
		 strncmp(userName + nameLen + 1, kDSNameAndAATag, sizeof(kDSNameAndAATag) - 1) == 0 )
	{
		aaBuffer = (tDataBufferPtr)(userName + nameLen + sizeof(kDSNameAndAATag));
		return UnpackUserWithAABuffer( aaBuffer, outAuthCount, outAuthAuthority );
	}
	
	return LookupAttribute( inContext, inRecordType, userName, kDSNAttrAuthenticationAuthority, outAuthCount, outAuthAuthority );
}


//------------------------------------------------------------------------------------
//	* LookupAttribute
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::LookupAttribute (	sLDAPContextData *inContext,
										    const char *inRecordType,
										    const char *inRecordName,
										    const char *inAttribute,
										    UInt32 *outCount,
										    char **outData[] )
{
	tDirStatus		siResult			= eDSRecordNotFound;
	char		   *valueData			= nil;
	char		   *pLDAPSearchBase		= nil;
    char		   *queryFilter			= nil;
	LDAPMessage	   *result				= nil;
	LDAPMessage	   *entry				= nil;
	char		   *attr				= nil;
	BerElement	   *ber					= nil;
	int				numRecTypes			= 1;
	char		  **attrs				= nil;
	bool			bOCANDGroup			= false;
	CFArrayRef		OCSearchList		= nil;
	struct berval **berVal				= nil;
	ber_int_t		scope				= LDAP_SCOPE_SUBTREE;
	char		   *pLDAPAttrType		= nil;
	tDirStatus		searchResult		= eDSNoErr;
    
	if ( inContext == nil ) return( eDSBadContextData );
	if ( inRecordName == nil ) return( eDSNullDataBuff );
	if ( outData == nil ) return( eDSNullParameter );
	
	try
	{
		*outCount = 0;
        *outData = nil;
        
		attrs = MapAttrToLDAPTypeArray( inContext, inRecordType, inAttribute );
		if ( attrs == nil ) throw( eDSInvalidAttributeType );
		
		if (*attrs == nil) //no values returned so maybe this is a static mapping
		{
			pLDAPAttrType = MapAttrToLDAPType( inContext,inRecordType, inAttribute, 1, false );
			if (pLDAPAttrType != nil)
			{
				*outData = (char **)calloc( 2, sizeof(char *) );
				(*outData)[0] = pLDAPAttrType;
				siResult = eDSNoErr;
			}
		}
		else //search for auth authority in LDAP server
		{
			DbgLog( kLogPlugin, "CLDAPv3Plugin: Attempting to get %s", inAttribute );
			
			// we will search over all the rectype mappings until we find the first
			// result for the search criteria in the queryfilter

			pLDAPSearchBase = MapRecToSearchBase( inContext, inRecordType, numRecTypes, &bOCANDGroup, &OCSearchList, &scope );
			//only throw this for first time since we need at least one map
			if ( pLDAPSearchBase == nil ) throw( eDSInvalidRecordType );
			
			while ( pLDAPSearchBase != nil )
			{
				queryFilter = BuildLDAPQueryFilter(
												inContext,
												(char *)kDSNAttrRecordName,
												inRecordName,
												eDSExact,
												false,
												inRecordType,
												pLDAPSearchBase,
												bOCANDGroup,
												OCSearchList );
				
				if ( queryFilter == nil ) throw( eDSNullParameter );
	
				searchResult = DSRetrieveSynchronous( pLDAPSearchBase, attrs, inContext, scope, queryFilter, &result, NULL );
				if ( searchResult == eDSNoErr || searchResult == eDSCannotAccessSession )
					break;
				
				DSDelete( queryFilter );
				DSDelete( pLDAPSearchBase );
				DSCFRelease( OCSearchList );

				numRecTypes++;
				bOCANDGroup = false;
				
				pLDAPSearchBase = MapRecToSearchBase( inContext, inRecordType, numRecTypes, &bOCANDGroup, &OCSearchList, &scope );
			} // while ( (pLDAPSearchBase != nil) && (!bResultFound) )
			
			if ( searchResult == eDSNoErr )
				siResult = eDSAttributeNotFound;
			
			if ( searchResult == eDSNoErr && result != NULL )
			{
				LDAP *aHost = inContext->fLDAPConnection->LockLDAPSession();
				if (aHost != NULL)
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
									valueData = dsCStrFromCharacters( berVal[idx]->bv_val, berVal[idx]->bv_len );
									if ( valueData == nil )
									{
										inContext->fLDAPConnection->UnlockLDAPSession( aHost, false );
										throw ( eMemoryError );
									}
									
									// TODO: return the right string
									DbgLog( kLogPlugin, "CLDAPv3Plugin: LookupAttribute value found %s\n", valueData ); 
									
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
					}		

					inContext->fLDAPConnection->UnlockLDAPSession( aHost, false );
				}// if aHost != NULL
			} // if bResultFound and ldapReturnCode okay
		} //search for auth authority in LDAP server
	}

	catch ( tDirStatus err )
	{
		DbgLog( kLogPlugin, "CLDAPv3Plugin: LookupAttribute error %l", err );
		siResult = err;
	}
	
	DSDelete( pLDAPSearchBase );
	DSCFRelease( OCSearchList );
	DSFreeStringList( attrs );
	DSDelete( queryFilter );
	
	if ( result != nil )
	{
		ldap_msgfree( result );
		result = nil;
	}
	
	return( siResult );
}


//------------------------------------------------------------------------------------
//      * DoPlugInCustomCall
//------------------------------------------------------------------------------------ 

tDirStatus CLDAPv3Plugin::DoPlugInCustomCall ( sDoPlugInCustomCall *inData )
{
	tDirStatus			siResult		= eDSNoErr;
	OSStatus			status			= 0;
	UInt32				aRequest		= 0;
	sLDAPContextData	*pContext		= nil;
	SInt32				xmlDataLength	= 0;
	CFDataRef   		xmlData			= nil;
	UInt32				bufLen			= 0;
	AuthorizationRef	authRef			= 0;
	tDataListPtr		dataList		= NULL;
	char*				userName		= NULL;
	char*				password		= NULL;
	char*				xmlString		= NULL;
	AuthorizationExternalForm blankExtForm;
	bool				verifyAuthRef	= true;
	CFRange				aRange			= { 0, 0 };
	char				*recordTypeCStr	= NULL;
	char				*recordNameCStr	= NULL;
	
//seems that the client needs to have a tDirNodeReference 
//to make the custom call even though it will likely be non-dirnode specific related

	try
	{
		aRequest = inData->fInRequestCode;
		if ( inData == nil ) throw( eDSNullParameter );
		if( aRequest != eDSCustomCallLDAPv3ReadConfigDataServerList )
		{
			if ( inData->fInRequestData == nil ) throw( eDSNullDataBuff );
			if ( inData->fInRequestData->fBufferData == nil ) throw( eDSEmptyBuffer );
		}

		pContext = gLDAPContextTable->GetObjectForRefNum( inData->fInNodeRef );
		if ( pContext == nil ) throw( eDSBadContextData );
		
		CLDAPNodeConfig *nodeConfig = pContext->fLDAPConnection->fNodeConfig;
		if ( nodeConfig == NULL || aRequest == eDSCustomCallLDAPv3Reinitialize )
		{
			bufLen = inData->fInRequestData->fBufferLength;
			if (	( aRequest != eDSCustomCallLDAPv3WriteServerMappings ) &&
					( aRequest != eDSCustomCallLDAPv3ReadConfigDataServerList ) )
			{
				if ( bufLen < sizeof( AuthorizationExternalForm ) ) throw( eDSInvalidBuffFormat );

				if ( pContext->fEffectiveUID == 0 ) {
					bzero(&blankExtForm,sizeof(AuthorizationExternalForm));
					if (memcmp(inData->fInRequestData->fBufferData,&blankExtForm,
							   sizeof(AuthorizationExternalForm)) == 0) {
						verifyAuthRef = false;
					}
				}
				if (verifyAuthRef) {
					status = AuthorizationCreateFromExternalForm((AuthorizationExternalForm *)inData->fInRequestData->fBufferData,
						&authRef);
					if (status != errAuthorizationSuccess)
					{
						DbgLog( kLogPlugin, "CLDAPv3Plugin: AuthorizationCreateFromExternalForm returned error %d", siResult );
						syslog( LOG_ALERT, "LDAP Custom Call <%d> AuthorizationCreateFromExternalForm returned error %d", aRequest, siResult );
						throw( eDSPermissionError );
					}
		
					AuthorizationItem rights[] = { {"system.services.directory.configure", 0, 0, 0} };
					AuthorizationItemSet rightSet = { sizeof(rights)/ sizeof(*rights), rights };
				
					status = AuthorizationCopyRights(authRef, &rightSet, NULL,
						kAuthorizationFlagExtendRights, NULL);

					if (status != errAuthorizationSuccess)
					{
						DbgLog( kLogPlugin, "CLDAPv3Plugin: AuthorizationCopyRights returned error %d", siResult );
						syslog( LOG_ALERT, "AuthorizationCopyRights returned error %d", siResult );
						throw( eDSPermissionError );
					}
				}
			}
			switch( aRequest )
			{
				case eDSCustomCallLDAPv3WriteServerMappings:
					// parse input buffer
					dataList = dsAuthBufferGetDataListAllocPriv(inData->fInRequestData);
					if ( dataList == nil ) throw( eDSInvalidBuffFormat );
					if ( dsDataListGetNodeCountPriv(dataList) != 3 ) throw( eDSInvalidBuffFormat );

					// this allocates a copy of the string
					userName = dsDataListGetNodeStringPriv(dataList, 1);
					if ( userName == nil ) throw( eDSInvalidBuffFormat );
					if ( strlen(userName) < 1 ) throw( eDSInvalidBuffFormat );

					password = dsDataListGetNodeStringPriv(dataList, 2);
					if ( password == nil ) throw( eDSInvalidBuffFormat );

					xmlString = dsDataListGetNodeStringPriv(dataList, 3);
					if ( xmlString == nil ) throw( eDSInvalidBuffFormat );
					xmlData = CFDataCreate(NULL,(UInt8*)xmlString,strlen(xmlString));
					
					siResult = (tDirStatus)fConfigFromXML->WriteServerMappings( userName, password, xmlData );

					DSFree( userName );
					DSFree( password );
					DSFree( xmlString );
					DSCFRelease( xmlData );

					break;
					/*
					//ReadServerMappings will accept the partial XML config data that comes out of the local config file so that
					//it can return the proper XML config data that defines all the mappings
					CFDataRef fConfigFromXML->ReadServerMappings ( LDAP *serverHost = pContext->fHost, CFDataRef inMappings = xmlData );
					*/
					 
				case eDSCustomCallLDAPv3ReadConfigSize:
					// get length of XML file
						
					if ( inData->fOutRequestResponse == nil ) throw( eDSNullDataBuff );
					if ( inData->fOutRequestResponse->fBufferData == nil ) throw( eDSEmptyBuffer );
					if ( inData->fOutRequestResponse->fBufferSize < sizeof( CFIndex ) ) throw( eDSInvalidBuffFormat );
					if (fConfigFromXML)
					{
						// need four bytes for size
						xmlData = fConfigFromXML->CopyLiveXMLConfig();
						if (xmlData != 0)
						{
							*(CFIndex*)(inData->fOutRequestResponse->fBufferData) = CFDataGetLength(xmlData);
							inData->fOutRequestResponse->fBufferLength = sizeof( CFIndex );
							DSCFRelease( xmlData );
						}
					}
					break;
					
				case eDSCustomCallLDAPv3ReadConfigData:
					// read xml config
					if ( inData->fOutRequestResponse == nil ) throw( eDSNullDataBuff );
					if ( inData->fOutRequestResponse->fBufferData == nil ) throw( eDSEmptyBuffer );
					if (fConfigFromXML)
					{
						xmlData = fConfigFromXML->CopyLiveXMLConfig();
						if (xmlData != 0)
						{
							aRange.location = 0;
							aRange.length = CFDataGetLength(xmlData);
							if ( inData->fOutRequestResponse->fBufferSize < (unsigned int)aRange.length ) throw( eDSBufferTooSmall );
							CFDataGetBytes( xmlData, aRange, (UInt8*)(inData->fOutRequestResponse->fBufferData) );
							inData->fOutRequestResponse->fBufferLength = aRange.length;
							DSCFRelease(xmlData);
						}
					}
					break;
				
				case eDSCustomCallLDAPv3ReadConfigDataServerList:
					// read xml config and build a server list with data that can be read without authentication
						
					if ( inData->fOutRequestResponse == nil ) throw( eDSNullDataBuff );
					if ( inData->fOutRequestResponse->fBufferData == nil ) throw( eDSEmptyBuffer );
					if (fConfigFromXML)
					{
						xmlData = fConfigFromXML->CopyLiveXMLConfig();
						if (xmlData != 0)
						{
							CFMutableDictionaryRef serverList = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, 
																						   &kCFTypeDictionaryValueCallBacks );
							CFDictionaryRef configDict = (CFDictionaryRef)CFPropertyListCreateFromXMLData( kCFAllocatorDefault, xmlData, 
																										   kCFPropertyListImmutable, NULL );
							if ( configDict != NULL )
							{
								CFTypeRef	arrays[2] = { CFDictionaryGetValue(configDict, CFSTR(kXMLConfigArrayKey)), 
														  CFDictionaryGetValue(configDict, CFSTR(kXMLDHCPConfigArrayKey)) };
								
								CFMutableArrayRef servers = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

								for ( int ii = 0; ii < 2; ii++ )
								{
									CFArrayRef serverConfigs = (CFArrayRef) arrays[ii];
									if ( serverConfigs != NULL && CFArrayGetCount(serverConfigs) > 0 )
									{
										for ( CFIndex iCnt = 0; iCnt < CFArrayGetCount( serverConfigs ); iCnt++ )
										{
											CFDictionaryRef serverEntry = (CFDictionaryRef)CFArrayGetValueAtIndex( serverConfigs, iCnt );
											CFMutableDictionaryRef listEntry = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, 
																										  &kCFTypeDictionaryKeyCallBacks, 
																										  &kCFTypeDictionaryValueCallBacks );
											if (serverEntry != NULL)
											{
												// if we are in our DHCP servers, flag this as DHCP
												if ( ii == 1 )
													CFDictionaryAddValue( listEntry, CFSTR(kXMLMakeDefLDAPFlagKey), kCFBooleanTrue );
												
												if ( CFDictionaryContainsKey( serverEntry, CFSTR(kXMLUserDefinedNameKey) ) )
												{
													CFDictionaryAddValue( listEntry, CFSTR(kXMLUserDefinedNameKey), CFDictionaryGetValue( serverEntry, CFSTR(kXMLUserDefinedNameKey) ) );
												}
												if ( CFDictionaryContainsKey( serverEntry, CFSTR(kXMLEnableUseFlagKey) ) )
												{
													CFDictionaryAddValue( listEntry, CFSTR(kXMLEnableUseFlagKey), CFDictionaryGetValue( serverEntry, CFSTR(kXMLEnableUseFlagKey) ) );
												}
												if ( CFDictionaryContainsKey( serverEntry, CFSTR(kXMLNodeName) ) )
												{
													CFDictionaryAddValue( listEntry, CFSTR(kXMLNodeName), CFDictionaryGetValue( serverEntry, CFSTR(kXMLNodeName) ) );
												}
												if ( CFDictionaryContainsKey( serverEntry, CFSTR(kXMLPortNumberKey) ) )
												{
													CFDictionaryAddValue( listEntry, CFSTR(kXMLPortNumberKey), CFDictionaryGetValue( serverEntry, CFSTR(kXMLPortNumberKey) ) );
												}
												if ( CFDictionaryContainsKey( serverEntry, CFSTR(kXMLSecureUseFlagKey) ) )
												{
													CFDictionaryAddValue( listEntry, CFSTR(kXMLSecureUseFlagKey), CFDictionaryGetValue( serverEntry, CFSTR(kXMLSecureUseFlagKey) ) );
												}
												if ( CFDictionaryContainsKey( serverEntry, CFSTR(kXMLServerKey) ) )
												{
													CFDictionaryAddValue( listEntry, CFSTR(kXMLServerKey), CFDictionaryGetValue( serverEntry, CFSTR(kXMLServerKey) ) );
												}
												CFArrayAppendValue( servers, listEntry );
												DSCFRelease(listEntry);
											}
										}
									}
								}
								
								CFDictionaryAddValue( serverList, CFSTR(kXMLConfigArrayKey), servers );
								DSCFRelease(servers);
							}
							
							DSCFRelease(configDict);
							DSCFRelease(xmlData);
							
							//build the response data
							xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, serverList);
							DSCFRelease(serverList);
							aRange.location = 0;
							aRange.length = CFDataGetLength(xmlData);
							if ( inData->fOutRequestResponse->fBufferSize < (unsigned int)aRange.length ) throw( eDSBufferTooSmall );
							CFDataGetBytes( xmlData, aRange, (UInt8*)(inData->fOutRequestResponse->fBufferData) );
							inData->fOutRequestResponse->fBufferLength = aRange.length;
							DSCFRelease(xmlData);
						}
					}
					break;
				
				case eDSCustomCallLDAPv3WriteConfigData:
				{
					CFPropertyListRef		configPropertyList = NULL;
					CFStringRef				errorString = NULL;
					
					//here we accept an XML blob to replace the current config file
					//need to make xmlData large enough to receive the data
					xmlDataLength = bufLen - sizeof( AuthorizationExternalForm );
					if ( xmlDataLength <= 0 ) throw( eDSInvalidBuffFormat );
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
							if ( CFDictionaryGetTypeID() == CFGetTypeID( configPropertyList ) && CFDictionaryContainsKey( (CFDictionaryRef)configPropertyList, CFSTR(kXMLDHCPConfigArrayKey) ) )
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
					
					// refresh registered nodes
					siResult = (tDirStatus) fConfigFromXML->NewXMLConfig( xmlData );
					// TODO: is this necessary anymore?
					if ( siResult == eDSNoErr )
						Initialize();
					DSCFRelease(xmlData);
					break;
				}
					
				case eDSCustomCallLDAPv3Reinitialize:
					if ( nodeConfig != NULL ) {
						nodeConfig->ReinitializeReplicaList();
						
						// this deletes all open connections from our map so they aren't re-used
						// doesn't do anything else
						fLDAPConnectionMgr->NodeDeleted( nodeConfig->fNodeName );
					}
					break;
					
				case eDSCustomCallLDAPv3AddServerConfig:
					//here we accept an XML blob to add a Server Mappings LDAP node to the search policy
					//need to make xmlData large enough to accomodate the data
					xmlDataLength =  bufLen - sizeof(AuthorizationExternalForm);
					if ( xmlDataLength <= 0 ) throw( eDSInvalidBuffFormat );
					xmlData = CFDataCreate(NULL, (UInt8 *)(inData->fInRequestData->fBufferData + sizeof(AuthorizationExternalForm)), xmlDataLength);
					if (fConfigFromXML)
					{
						// add to registered nodes as a "forced" DHCP type node
						siResult = (tDirStatus)fConfigFromXML->AddToXMLConfig( xmlData );
					}
					DSCFRelease( xmlData );
					break;
					
				case eDSCustomCallLDAPv3NewServerDiscovery:
				case eDSCustomCallLDAPv3NewServerDiscoveryNoDupes:
					// Verify server can be contacted, get basic information from RootDSE
					// this includes determining the name context and look for server mappings
					// Will return:
					//		eDSNoErr					= Everything okay, continue to next step
					//		eDSBogusServer				= Could not contact a server at that address
					//		eDSSchemaError				= Could not detect Schema Mappings from config
					//		eDSRecordAlreadyExists		= The server is already in the configuration
					siResult = (tDirStatus)DoNewServerDiscovery( inData );
					break;
					
				case eDSCustomCallLDAPv3NewServerVerifySettings:
					// Verify configuration from server or user works (i.e., mappings) or requires authentication
					// Will return:
					//		eDSNoErr					= Everything okay, continue to next step
					//		eDSBogusServer				= Could not contact a server at that address
					//		eDSInvalidNativeMapping		= Could not use mappings supplied to query directory
					//		eDSAuthParameterError		= Requires Authentication, can't query Directory
					siResult = (tDirStatus)DoNewServerVerifySettings( inData );
					break;
					
				case eDSCustomCallLDAPv3NewServerGetConfig:
					// Determine if server configuration (i.e., Directory Binding)
					// Will return:
					//		eDSNoErr					= Everything okay, continue next step
					//		eDSBogusServer				= Could not contact a server at that address
					//		eDSInvalidNativeMapping		= Could not use mappings supplied to query directory
					//		eDSAuthParameterError		= Requires Authentication, can't query Directory
					siResult = (tDirStatus)DoNewServerGetConfig( inData );
					break;
					
				case eDSCustomCallLDAPv3NewServerBind:          // Do not join, check fail with eDSRecordAlreadyExists if already there
				case eDSCustomCallLDAPv3NewServerForceBind:		// Join existing account
					// Bind to server
					// Will return:
					//		eDSNoErr					= Everything okay, configuration complete
					//		eDSBogusServer				= Could not contact a server at that address
					//		eDSInvalidNativeMapping		= Could not use mappings supplied to query directory
					//		eDSNoStdMappingAvailable	= No Computer Mapping available for this process
					//		eDSInvalidRecordName		= Missing a valid name for the Computer
					//		eDSAuthMethodNotSupported   = Method not supported, see key "Supported Security Level"
					//		eDSRecordAlreadyExists		= Computer already exists, send eDSCustomCallLDAPv3NewServerForceBind to override
					siResult = (tDirStatus)DoNewServerBind( inData );
					break;
					
				case eDSCustomCallLDAPv3NewServerAddConfig:
					// Setup and non-binded server
					// Will return:
					//		eDSNoErr					= Everything okay, configuration complete
					//		eDSBogusServer				= Could not contact a server at that address
					//		eDSInvalidNativeMapping		= Could not use mappings supplied to query directory
					siResult = (tDirStatus)DoNewServerSetup( inData );
					break;
					
				case eDSCustomCallLDAPv3UnbindServerConfig:         // normal unbind
				case eDSCustomCallLDAPv3ForceUnbindServerConfig:    // force unbind, leaving computer account if it exists
				case eDSCustomCallLDAPv3RemoveServerConfig:         // remove server from configuration
					// Unbind request
					// Will return:
					//		eDSNoErr					= Everything okay, removal complete
					//		eDSBogusServer				= No such server configured
					//		eDSAuthParameterError		= Must be bound, needs authentication to unbind
					//		eDSOpenNodeFailed			= Bound but couldn't complete for some reason, may need to force unbind
					siResult = (tDirStatus)DoRemoveServer( inData );
					break;
				
				case eDSCustomCallLDAPv3NewServerBindOther:         // do a bind, but not for ourselves
				case eDSCustomCallLDAPv3NewServerForceBindOther:    // do a force bind, but not for ourselves
					// Bind to server
					// Will return:
					//		eDSNoErr					= Everything okay, configuration complete
					//		eDSBogusServer				= Could not contact a server at that address
					//		eDSInvalidNativeMapping		= Could not use mappings supplied to query directory
					//		eDSNoStdMappingAvailable	= No Computer Mapping available for this process
					//		eDSInvalidRecordName		= Missing a valid name for the Computer
					//		eDSAuthMethodNotSupported   = Method not supported, see key "Supported Security Level"
					//		eDSRecordAlreadyExists		= Computer already exists, send eDSCustomCallLDAPv3NewServerForceBind to override
					siResult = (tDirStatus)DoNewServerBind2( inData );
					break;
					
				default:
					break;
			}
		}
		else if ( inData->fInRequestCode == eDSCustomCallLDAPv3CurrentAuthenticatedUser )
		{
			if ( pContext != NULL && pContext->fLDAPConnection != NULL && pContext->fLDAPConnection->fKerberosID != NULL )
			{
				UInt32 iLen = strlen( pContext->fLDAPConnection->fKerberosID );
				if ( inData->fOutRequestResponse->fBufferSize > iLen )
				{
					strcpy( inData->fOutRequestResponse->fBufferData, pContext->fLDAPConnection->fKerberosID );
					inData->fOutRequestResponse->fBufferLength = iLen;
					siResult = eDSNoErr;
				}
				else
				{
					siResult = eDSBufferTooSmall;
				}
			}
			else
			{
				siResult = eDSRecordNotFound;
			}
		}
		else if( inData->fInRequestCode == eDSCustomCallDeleteRecordAndCredentials )
		{
			// this is a special delete function that will use existing credentials to delete a record
			// and it's PWS information cleanly
			sDeleteRecord	tempStruct;
			
			tempStruct.fInRecRef = *((tRecordReference *)inData->fInRequestData->fBufferData);
			siResult = DeleteRecord( &tempStruct, true );
		}
		else if( inData->fInRequestCode == eDSCustomCallExtendedRecordCallsAvailable )	// lets clients see  if record blat operations are available
		{
			if( inData->fOutRequestResponse->fBufferSize < 1 )
				throw( eDSInvalidBuffFormat );
			inData->fOutRequestResponse->fBufferLength = 1;
			inData->fOutRequestResponse->fBufferData[0] = 1;
		}
		else if( inData->fInRequestCode == eDSCustomCallCreateRecordWithAttributes )	// create and set the attribute values in one op
		{
			const char* recordType = NULL;
			const char* recordName = NULL;
			CFDictionaryRef recordDict = 0;
			{	// get the record ref and desired record dict out of the buffer
				CFDataRef bufferCFData = CFDataCreateWithBytesNoCopy( kCFAllocatorDefault, (const UInt8*)inData->fInRequestData->fBufferData,
					inData->fInRequestData->fBufferLength, kCFAllocatorNull );
				if( bufferCFData == NULL )
					throw( eDSInvalidBuffFormat );
				CFStringRef errorString;
				CFDictionaryRef bufferCFDict = (CFDictionaryRef)CFPropertyListCreateFromXMLData( kCFAllocatorDefault, bufferCFData,
					kCFPropertyListImmutable, &errorString );
				CFRelease( bufferCFData );	//don't need the bufferCFData anymore
				bufferCFData = 0;

				CFStringRef recordTypeCFString = (CFStringRef)CFDictionaryGetValue( bufferCFDict, CFSTR( "Record Type" ) );
				if ( recordTypeCFString == NULL || CFGetTypeID(recordTypeCFString) != CFStringGetTypeID() )
					throw( eDSInvalidBuffFormat );

				recordType = GetCStringFromCFString( recordTypeCFString, &recordTypeCStr );
				
				recordDict = (CFDictionaryRef)CFDictionaryGetValue( bufferCFDict, CFSTR( "Attributes and Values" ) );
				if( recordDict == NULL )
					throw( eDSInvalidBuffFormat );

				CFArrayRef recordNames = (CFArrayRef)CFDictionaryGetValue( recordDict, CFSTR( kDSNAttrRecordName ) );
				if( recordNames == 0 || CFArrayGetCount( recordNames ) == 0 )
					throw( eDSInvalidBuffFormat );
				
				CFStringRef recordNameCFString = (CFStringRef)CFArrayGetValueAtIndex( recordNames, 0 );
				if( recordNameCFString == NULL || CFGetTypeID(recordNameCFString) != CFStringGetTypeID())
					throw( eDSInvalidBuffFormat );

				recordName = GetCStringFromCFString( recordNameCFString, &recordNameCStr );
				
				CFRetain( recordDict );

				CFRelease( bufferCFDict );	//don't need the bufferCFDict anymore
				bufferCFDict = NULL;
			}
			if( ( recordType != NULL ) &&( recordName != NULL ) && ( recordDict != 0 ) )
			{
				siResult = CreateRecordWithAttributes( inData->fInNodeRef, recordType, recordName, recordDict );
				if( siResult != eDSNoErr )
					throw( siResult );
			}
		}
		else if( inData->fInRequestCode == eDSCustomCallSetAttributes )	// set all of the records attr values
		{
			tRecordReference recordRef = 0;
			CFDictionaryRef recordDict = 0;
			{	// get the record ref and desired record dict out of the buffer
				CFDataRef bufferCFData = CFDataCreateWithBytesNoCopy( kCFAllocatorDefault, (const UInt8*)inData->fInRequestData->fBufferData,
					inData->fInRequestData->fBufferLength, kCFAllocatorNull );
				if( bufferCFData == NULL )
					throw( eDSInvalidBuffFormat );
				CFStringRef errorString;
				CFDictionaryRef bufferCFDict = (CFDictionaryRef)CFPropertyListCreateFromXMLData( kCFAllocatorDefault, bufferCFData,
					kCFPropertyListImmutable, &errorString );
				CFRelease( bufferCFData );	//don't need the bufferCFData anymore
				bufferCFData = 0;

				CFNumberRef recordRefNumber = (CFNumberRef)CFDictionaryGetValue( bufferCFDict, CFSTR( "Record Reference" ) );
				if( !CFNumberGetValue( recordRefNumber, kCFNumberLongType, &recordRef ) )
					throw( eDSInvalidBuffFormat );
				
				recordDict = (CFDictionaryRef)CFDictionaryGetValue( bufferCFDict, CFSTR( "Attributes and Values" ) );
				if( recordDict == NULL )
					throw( eDSInvalidBuffFormat );

				
				CFRetain( recordDict );

				CFRelease( bufferCFDict );	//don't need the bufferCFDict anymore
				bufferCFDict = NULL;
			}
			if( ( recordRef != 0 ) && ( recordDict != 0 ) )
			{
				siResult = SetAttributes( recordRef, recordDict );
				if( siResult != eDSNoErr )
					throw( siResult );
			}
			
			CFRelease( recordDict );	//done with the recordDict
		}
	}

	catch ( tDirStatus err )
	{
		siResult = err;
	}

	if (authRef != 0)
	{
		AuthorizationFree(authRef, 0);
		authRef = 0;
	}
	
	DSFree( recordTypeCStr );
	DSFree( recordNameCStr );
	DSRelease( pContext );

	return( siResult );

} // DoPlugInCustomCall

#pragma mark -
#pragma mark Other functions

// ---------------------------------------------------------------------------
//	* ContinueDeallocProc
// ---------------------------------------------------------------------------

void CLDAPv3Plugin::ContinueDeallocProc ( void* inContinueData )
{
	sLDAPContinueData *pLDAPContinue = (sLDAPContinueData *) inContinueData;

	if ( pLDAPContinue != nil )
	{
        if ( pLDAPContinue->fResult != nil )
        {
        	ldap_msgfree( pLDAPContinue->fResult );
	        pLDAPContinue->fResult = nil;
        }
		
		if ( pLDAPContinue->fLDAPMsgId > 0 )
		{
			if ( pLDAPContinue->fLDAPConnection != nil ) 
			{
				LDAP *aHost = pLDAPContinue->fLDAPConnection->LockLDAPSession();
				if ( aHost != NULL )
				{
					if ( aHost == pLDAPContinue->fRefLD )
					{
						ldap_abandon_ext( aHost, pLDAPContinue->fLDAPMsgId, NULL, NULL );
					}

					pLDAPContinue->fLDAPConnection->UnlockLDAPSession( aHost, false );
				}
			}
			
			pLDAPContinue->fLDAPMsgId = 0;
			pLDAPContinue->fRefLD = NULL;			
		}
		
		DSRelease( pLDAPContinue->fLDAPConnection );
		DSFreeString( pLDAPContinue->fAuthAuthorityData );
		DSFree( pLDAPContinue );
	}
} // ContinueDeallocProc


//------------------------------------------------------------------------------------
//	* GetRecLDAPMessage
//------------------------------------------------------------------------------------

tDirStatus CLDAPv3Plugin::GetRecLDAPMessage ( sLDAPContextData *inRecContext, char **inAttrs, LDAPMessage **outResultMsg )
{
	tDirStatus			siResult		= eDSNoErr;
	int					numRecTypes		= 1;
	char			   *pLDAPSearchBase	= nil;
	bool				bResultFound	= false;
    char			   *queryFilter		= nil;
	LDAPMessage		   *result			= nil;
	bool				bOCANDGroup		= false;
	CFArrayRef			OCSearchList	= nil;
	ber_int_t			scope			= LDAP_SCOPE_SUBTREE;

	//TODO would actually like to use the fOpenRecordDN instead here as it might reduce the effort in the search?
	//ie. we will likely already have it
	if ( inRecContext->fOpenRecordName == nil ) return eDSNullRecName;
	if ( inRecContext->fOpenRecordType == nil ) return eDSNullRecType;
	
	// we will search over all the rectype mappings until we find the first
	// result for the search criteria in the queryfilter
	pLDAPSearchBase = MapRecToSearchBase( inRecContext, (const char *)(inRecContext->fOpenRecordType), numRecTypes, &bOCANDGroup, &OCSearchList, &scope );
	if ( pLDAPSearchBase == NULL )
		siResult = eDSInvalidRecordType;

	//search for the specific LDAP record now
	while ( (pLDAPSearchBase != nil) && (!bResultFound) )
	{

		//build the record query string
		//removed the use well known map only condition ie. true to false
		queryFilter = BuildLDAPQueryFilter( inRecContext,
											(char *)kDSNAttrRecordName, //TODO can we use dn here ie native type
											inRecContext->fOpenRecordName,
											eDSExact,
											false,
											(const char *)(inRecContext->fOpenRecordType),
											pLDAPSearchBase,
											bOCANDGroup,
											OCSearchList );
		if ( queryFilter == nil )
		{
			siResult = eDSNullParameter;
			break;
		}

		siResult = DSRetrieveSynchronous( pLDAPSearchBase,
										  inAttrs,
										  inRecContext,
										  scope,
										  queryFilter,
										  &result,
										  NULL );
		
		if ( siResult == eDSNoErr || siResult == eDSCannotAccessSession )
			break;

		numRecTypes++;
		bOCANDGroup = false;

		DSDelete( queryFilter );
		DSDelete( pLDAPSearchBase );
		DSCFRelease( OCSearchList );
		
		pLDAPSearchBase = MapRecToSearchBase( inRecContext, (const char *)(inRecContext->fOpenRecordType), numRecTypes, 
										   &bOCANDGroup, &OCSearchList, &scope );
	} // while ( (pLDAPSearchBase != nil) && (!bResultFound) )

	DSCFRelease( OCSearchList );
	DSFreeString( pLDAPSearchBase );
	DSFreeString( queryFilter );
	
	*outResultMsg = result;

	return( siResult );

} // GetRecLDAPMessage


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

SInt32 CLDAPv3Plugin::PeriodicTask ( void )
{
    CLDAPv3Plugin::WaitForNetworkTransitionToFinish();
    
    fLDAPConnectionMgr->PeriodicTask();
	fConfigFromXML->PeriodicTask();
	
	return( eDSNoErr );
} // PeriodicTask

//------------------------------------------------------------------------------------
//	* WaitForNetworkTransitionToFinishWithFunctionName
//------------------------------------------------------------------------------------

void CLDAPv3Plugin::WaitForNetworkTransitionToFinishWithFunctionName( const char* callerFunction )
{
    if( gNetworkTransition.WaitForEvent( 10 * kMilliSecsPerSec ) == false )
    {
        DbgLog( kLogPlugin, "CLDAPv3Plugin::WaitForNetworkTransitionToFinish %s timed out or reached maximum wait time", callerFunction );
    }
}// WaitForNetworkTransitionToFinish

//------------------------------------------------------------------------------------
//	* MappingNativeVariableSubstitution
//------------------------------------------------------------------------------------

char* CLDAPv3Plugin::MappingNativeVariableSubstitution(	char			   *inLDAPAttrType,
														sLDAPContextData   *inContext,
														LDAPMessage		   *inResult,
														tDirStatus&			outResult)
{
	char			   *returnStr					= nil;
	char			   *tmpStr						= nil;
	char			   *vsPtr						= nil;
	char			  **tokens						= nil;
	char			  **tokenValues					= nil;
	struct berval     **vsBValues;
	int					tokenCount					= 0;
	int					returnStrCount				= 0;
	int					vsShift						= 0;
	LDAP			   *aHost						= nil;
	int					tokenStep					= 0;
	int					index						= 0;
	char			   *lastPtr						= nil;
	const char			delimiterChar				= '$';
	const char			literalChar					= '#';
	const char			nullChar					= '\0';
	int					tokensMax					= 0;
	
	
	outResult = eDSNoErr;
	
	if ( (inLDAPAttrType != nil) && (*inLDAPAttrType == literalChar) && ( strchr(inLDAPAttrType, delimiterChar) != nil) )
	{
		// found a variable substitution marker...
		
		returnStrCount = strlen(inLDAPAttrType);
		
		// copy the literal mapping since we do destructive parsing
		vsPtr = (char *)calloc(1, 1+returnStrCount);
		memcpy(vsPtr, inLDAPAttrType, returnStrCount);
		
		tmpStr = vsPtr; //needed to free the memory below
		
		//setup the max size for the tokens arrays
		while (vsPtr != nil)
		{
			tokensMax++;
			vsPtr = strchr(vsPtr, delimiterChar);
			if (vsPtr != nil)
			{
				vsPtr = vsPtr + 1;
			}
		}
		tokens		= (char **)calloc(sizeof(char *), tokensMax+1);
		tokenValues = (char **)calloc(sizeof(char *), tokensMax+1);
		
		vsPtr = tmpStr; //reset the ptr

		while ( (vsPtr != nil) && (*vsPtr != nullChar) )
		{
			if ( *vsPtr == delimiterChar )
			{
				//this should be the start of a variable substitution
				*vsPtr = nullChar;
				vsPtr = vsPtr + 1;
				if (*vsPtr != nullChar)
				{
					lastPtr = strchr(vsPtr, delimiterChar);
					//attribute value must be alphanumeric or - or ; according to RFC2252
					//also check for consecutive delimiters which represents a single delimiter
					if (lastPtr != nil)
					{
						tokens[tokenCount] = vsPtr;
						if (vsPtr != lastPtr)
						{
							//case of true variable substitution
							*lastPtr = nullChar;
						}
						else
						{
							tokenValues[tokenCount] = strdup("$");
						}
						tokenCount++;
						vsPtr = lastPtr + 1;
					}
					else
					{
						//invalid mapping format ie. no matching delimiter pair
						outResult = eDSInvalidNativeMapping;
						break;
					}
				}
				else
				{
					//invalid mapping format ie. nothing following delimiter
					outResult = eDSInvalidNativeMapping;
					break;
				}
			}
			else
			{
				//we have literal text so we leave it alone
				tokens[tokenCount] = vsPtr;
				tokenValues[tokenCount] = vsPtr;
				tokenCount++;
				while ( ( *vsPtr != nullChar ) && ( *vsPtr != delimiterChar ) )
				{
					vsPtr = vsPtr + 1;
				}
			}
		}
		
		if (outResult == eDSNoErr)
		{
			aHost = inContext->fLDAPConnection->LockLDAPSession();
			if (aHost != NULL)
			{
				for ( tokenStep = 0; tokenStep < tokenCount; tokenStep++)
				{
					if (tokenValues[tokenStep] == nil)
					{
						//choose first value only for the substitution
						vsBValues = ldap_get_values_len(aHost, inResult, tokens[tokenStep]);
						if (vsBValues != nil)
						{
							returnStrCount += vsBValues[0]->bv_len; //a little extra since variable name was already included
							tokenValues[tokenStep] = (char *) calloc(vsBValues[0]->bv_len, sizeof(char));
							memcpy(tokenValues[tokenStep], vsBValues[0]->bv_val, vsBValues[0]->bv_len);
							ldap_value_free_len(vsBValues);
							vsBValues = nil;
						}
					} //not a delimiter char
				} // for step through tokens
				inContext->fLDAPConnection->UnlockLDAPSession( aHost, false );
			}
			
			returnStr = (char *) calloc(1+returnStrCount, sizeof(char));
			
			for ( tokenStep = 0; tokenStep < tokenCount; tokenStep++)
			{
				if (tokenValues[tokenStep] != nil)
				{
					memcpy((returnStr + vsShift), tokenValues[tokenStep], (strlen(tokenValues[tokenStep])));
					vsShift += strlen(tokenValues[tokenStep]);
				}
			}
			
			for (index = 0; index < tokensMax; index++)
			{
				if ( (tokenValues[index] != nil) && ( tokenValues[index] != tokens[index] ) )
				{
					free(tokenValues[index]);
					tokenValues[index] = nil;
				}
			}
		}
		free(tmpStr);
		tmpStr = nil;
		free(tokens);
		free(tokenValues);
	}
	
	return(returnStr);
	
} // MappingNativeVariableSubstitution


//------------------------------------------------------------------------------------
//	* GetPWSIDforRecord
//------------------------------------------------------------------------------------

char *CLDAPv3Plugin::GetPWSIDforRecord( sLDAPContextData *pContext, const char *inRecName, const char *inRecType )
{
	char			**aaArray   = NULL;
	UInt32			aaCount		= 0;
	char			*pUserID	= NULL;
	
	if ( GetAuthAuthority(pContext, inRecName, 0, &aaCount, &aaArray, inRecType) == eDSNoErr )
	{
		pUserID = GetPWSAuthData( aaArray, aaCount );
		DSFree( aaArray );
	}
	
	return pUserID;
} // GetPWSIDforRecord

#pragma mark -
#pragma mark BaseDirectoryPlugin virtual functions

void CLDAPv3Plugin::NetworkTransition( void )
{
	// now we call network transition so we kick off our idle stuff
	fLDAPConnectionMgr->NetworkTransition();
	fConfigFromXML->NetworkTransition();
	
	DbgLog( kLogPlugin, "CLDAPv3Plugin::Posting Network transition event now" );
	gNetworkTransition.PostEvent();
}

// BaseDirectoryPlugin pure virtual overrides

CFDataRef CLDAPv3Plugin::CopyConfiguration( void )
{
	return NULL;
}

bool CLDAPv3Plugin::NewConfiguration( const char *inData, UInt32 inLength )
{
	return false;
}

bool CLDAPv3Plugin::CheckConfiguration( const char *inData, UInt32 inLength )
{
	return false;
}

tDirStatus CLDAPv3Plugin::HandleCustomCall( sBDPINodeContext *pContext, sDoPlugInCustomCall *inData )
{
	tDirStatus	siResult = eNotHandledByThisNode;

	return siResult;
}

bool CLDAPv3Plugin::IsConfigureNodeName( CFStringRef inNodeName )
{
	return false;
}

BDPIVirtualNode *CLDAPv3Plugin::CreateNodeForPath( CFStringRef inPath, uid_t inUID, uid_t inEffectiveUID )
{
	return NULL;
}
