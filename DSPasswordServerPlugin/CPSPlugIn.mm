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
 * @header CPSPlugIn
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <net/ethernet.h>
#include <ldap.h>

#include "CPSPlugIn.h"
#include "PSUtilities.h"
#include "CAuthFileBase.h"
#include "ReplicaFileDefs.h"
#include "CPSPlugInUtils.h"

using namespace std;


#include <DirectoryServiceCore/ServerModuleLib.h>

#include <DirectoryServiceCore/CRCCalc.h>

#include <DirectoryServiceCore/CPlugInRef.h>
#include <DirectoryServiceCore/DSCThread.h>
#include <DirectoryServiceCore/CContinue.h>
#include <DirectoryServiceCore/DSEventSemaphore.h>
#include <DirectoryServiceCore/DSMutexSemaphore.h>
#include <DirectoryServiceCore/CSharedData.h>
#include <DirectoryServiceCore/DSUtils.h>
#include <DirectoryServiceCore/PrivateTypes.h>

#include "SASLCode.h"

#define DEBUG	0

#define kPWSPlugInPrefsDirPath				"/Library/Preferences/DirectoryService"
#define kPWSPlugInPrefsFilePath				kPWSPlugInPrefsDirPath "/PasswordServerPluginPrefs.plist"
#define kPWSPlugInPrefsSASLPriorityKey		"SASL Mech Priority"
#define kPWSPlugInDigestMethodStr			",method=\""

#define kDSTempSyncFileControlStr			"/var/db/authserver/apsSyncFi%ld.%ld.gz"
#define kDSRequestNonceHashStr				"hash"

#define dsDataListDeallocFree(DSREF, NODE)		{ dsDataListDeallocate( (DSREF), (NODE) ); free( (NODE) ); }

#undef kAuthNTLMv2WithSessionKey
#define kAuthNTLMv2WithSessionKey	(1305)

// --------------------------------------------------------------------------------
//	Globals

CPlugInRef				   *gPSContextTable	= NULL;
static DSEventSemaphore	   gKickSearchRequests;
static DSMutexSemaphore	   *gSASLMutex = NULL;
static DSMutexSemaphore	   *gPWSConnMutex = NULL;
CContinue	 			   *gContinue = NULL;

extern long gOpenCount;
static long gCloseCount = 0;

static MethodMapEntry gMethodMap[] = 
{
	{ "APOP", kDSStdAuthAPOP },
	{ "CRAM-MD5", kDSStdAuthCRAM_MD5 },
	{ "CRYPT", kDSStdAuthCrypt },
	{ "MS-CHAPV2", kDSStdAuthMSCHAP2 },
	{ "PPS", "dsAuthMethodStandard:dsAuthNodePPS" },
	{ "SMB-LAN-MANAGER", kDSStdAuthSMB_LM_Key },
	{ "SMB-NT", kDSStdAuthSMB_NT_Key },
	{ "SMB-NTLMv2", kDSStdAuthNTLMv2 },
	{ "TWOWAYRANDOM", kDSStdAuth2WayRandom },
	{ "WEBDAV-DIGEST", kDSStdAuthDIGEST_MD5 },
	{ NULL, NULL }
};

// Consts ----------------------------------------------------------------------------

static const	UInt32	kBuffPad	= 16;

extern "C" {
CFUUIDRef ModuleFactoryUUID = CFUUIDGetConstantUUIDWithBytes ( NULL, \
								0xF8, 0xAC, 0xD8, 0x6B, 0x3C, 0x66, 0x11, 0xD6, \
								0x93, 0x9C, 0x00, 0x03, 0x93, 0x50, 0xEB, 0x4E );

}


static CDSServerModule* _Creator ( void )
{
	return( new CPSPlugIn );
}

CDSServerModule::tCreator CDSServerModule::sCreator = _Creator;

// --------------------------------------------------------------------------------
//	* CPSPlugIn ()
// --------------------------------------------------------------------------------

CPSPlugIn::CPSPlugIn ( void )
{        
	fState = kUnknownState;
	fOpenNodeCount = 0;
    fCalledSASLInit = false;
	
    try
    {
        if ( gPSContextTable == NULL )
        {
            gPSContextTable = new CPlugInRef( CPSPlugIn::ContextDeallocProc );
            Throw_NULL( gPSContextTable, eMemoryAllocError );
        }
        
        if ( gSASLMutex == NULL )
        {
            gSASLMutex = new DSMutexSemaphore();
            Throw_NULL( gSASLMutex, eMemoryAllocError );
        }
		
		if ( gPWSConnMutex == NULL )
        {
            gPWSConnMutex = new DSMutexSemaphore();
            Throw_NULL( gPWSConnMutex, eMemoryAllocError );
        }
		
		if ( gContinue == NULL )
        {
            gContinue = new CContinue( CPSPlugIn::ContinueDeallocProc );
			Throw_NULL( gContinue, eMemoryAllocError );
		}
		
		strlcpy( fSASLMechPriority, kAuthNative_Priority, sizeof(fSASLMechPriority) );
    }
    
    catch (SInt32 err)
    {
	    DEBUGLOG( "CPSPlugIn::CPSPlugIn failed: eMemoryAllocError");
        throw( err );
    }
} // CPSPlugIn


// --------------------------------------------------------------------------------
//	* ~CPSPlugIn ()
// --------------------------------------------------------------------------------

CPSPlugIn::~CPSPlugIn ( void )
{
} // ~CPSPlugIn


// --------------------------------------------------------------------------------
//	* Validate ()
// --------------------------------------------------------------------------------

SInt32 CPSPlugIn::Validate ( const char *inVersionStr, const UInt32 inSignature )
{
	fSignature = inSignature;

	return( eDSNoErr );

} // Validate


// --------------------------------------------------------------------------------
//	* Initialize ()
// --------------------------------------------------------------------------------

SInt32 CPSPlugIn::Initialize ( void )
{
    SInt32 siResult = eDSNoErr;
	CFMutableDictionaryRef prefsDict = NULL;
	CFStringRef priorityString = NULL;
	struct stat sb;
		
	// set the active and initted flags
	fState = kUnknownState;
	fState += kInitialized;
	fState += kActive;
	
	// read prefs file
	siResult = lstat( kPWSPlugInPrefsFilePath, &sb );
	if ( siResult == 0 && pwsf_loadxml(kPWSPlugInPrefsFilePath, &prefsDict) == 0 )
	{
		if ( CFDictionaryGetValueIfPresent(prefsDict, CFSTR(kPWSPlugInPrefsSASLPriorityKey), (const void **)&priorityString) )
		{
			CFStringGetCString( priorityString, fSASLMechPriority, sizeof(fSASLMechPriority), kCFStringEncodingUTF8);
		}
	}
	else
	{
		// create a template
		prefsDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		if ( prefsDict != NULL )
		{
			CFDictionaryAddValue( prefsDict, CFSTR(kPWSPlugInPrefsSASLPriorityKey), CFSTR(kAuthNative_Priority) );
			pwsf_savexml( kPWSPlugInPrefsFilePath, prefsDict );
		}
	}
	
	DSCFRelease( prefsDict );
	
	return( siResult );
} // Initialize


// --------------------------------------------------------------------------------
//	* SetPluginState ()
// --------------------------------------------------------------------------------

SInt32 CPSPlugIn::SetPluginState ( const UInt32 inState )
{

// don't allow any changes other than active / in-active

	if (kActive & inState) //want to set to active
	{
		if (fState & kActive) //if already active
		{
			//TODO ???
		}
		else
		{
			//call to Init so that we re-init everything that requires it
			Initialize();
		}
		WakeUpRequests();
	}

	if (kInactive & inState) //want to set to in-active
	{
		if (!(fState & kInactive))
		{
			fState += kInactive;
		}
		if (fState & kActive)
		{
			fState -= kActive;
		}
	}

	return( eDSNoErr );

} // SetPluginState


//--------------------------------------------------------------------------------------------------
//	* WakeUpRequests() (static)
//
//--------------------------------------------------------------------------------------------------

void CPSPlugIn::WakeUpRequests ( void )
{
	gKickSearchRequests.PostEvent();
} // WakeUpRequests


// ---------------------------------------------------------------------------
//	* WaitForInit
//
// ---------------------------------------------------------------------------

void CPSPlugIn::WaitForInit ( void )
{
	// we wait for 2 minutes before giving up
	gKickSearchRequests.WaitForEvent( (UInt32)(2 * 60 * kMilliSecsPerSec) );
} // WaitForInit


// ---------------------------------------------------------------------------
//	* ProcessRequest
//
// ---------------------------------------------------------------------------

SInt32 CPSPlugIn::ProcessRequest ( void *inData )
{
	SInt32		siResult	= 0;

	if ( inData == NULL )
	{
		return( ePlugInDataError );
	}

	if( ((sHeader *)inData)->fType == kKerberosMutex || ((sHeader *)inData)->fType == kServerRunLoop )
	{
		// we don't care about Kerberos mutexes here
		return eDSNoErr;
	}		

    
    WaitForInit();

	if ( (fState & kFailedToInit) )
	{
        return( ePlugInFailedToInitialize );
	}

	if ( ((fState & kInactive) || !(fState & kActive))
		  && (((sHeader *)inData)->fType != kDoPlugInCustomCall)
		  && (((sHeader *)inData)->fType != kOpenDirNode) )
	{
        return( ePlugInNotActive );
	}
    
	if ( ((sHeader *)inData)->fType == kHandleNetworkTransition )
	{
		siResult = Initialize();
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

SInt32 CPSPlugIn::HandleRequest ( void *inData )
{
	SInt32	siResult	= 0;
	sHeader	*pMsgHdr	= NULL;

	if ( inData == NULL )
	{
		return( -8088 );
	}

	pMsgHdr = (sHeader *)inData;

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
			
		case kGetAttributeEntry:
			siResult = GetAttributeEntry( (sGetAttributeEntry *)inData );
			break;
			
		case kGetAttributeValue:
			siResult = GetAttributeValue( (sGetAttributeValue *)inData );
			break;
			
		case kCloseAttributeList:
			siResult = CloseAttributeList( (sCloseAttributeList *)inData );
			break;

		case kCloseAttributeValueList:
			siResult = CloseAttributeValueList( (sCloseAttributeValueList *)inData );
			break;

		case kDoDirNodeAuth:
			siResult = DoAuthentication( (sDoDirNodeAuth *)inData );
			break;
			
		case kDoPlugInCustomCall:
			siResult = DoPlugInCustomCall( (sDoPlugInCustomCall *)inData );
			break;
			
		default:
			siResult = eNotHandledByThisNode;
			break;
	}

	pMsgHdr->fResult = siResult;

	return( siResult );

} // HandleRequest


//------------------------------------------------------------------------------------
//	* ReleaseContinueData
//------------------------------------------------------------------------------------

SInt32 CPSPlugIn::ReleaseContinueData ( sReleaseContinueData *inData )
{
	SInt32	siResult	= eDSNoErr;
    
	// RemoveItem calls our ContinueDeallocProc to clean up
	if ( gContinue->RemoveItem( (void *)inData->fInContinueData ) != eDSNoErr )
	{
		siResult = eDSInvalidContext;
	}
    
	return( siResult );

} // ReleaseContinueData


//------------------------------------------------------------------------------------
//	* OpenDirNode
//------------------------------------------------------------------------------------

SInt32 CPSPlugIn::OpenDirNode ( sOpenDirNode *inData )
{
	SInt32				siResult		= eDSNoErr;
    tDataListPtr		pNodeList		= NULL;
	char			   *pathStr			= NULL;
    char			   *subStr			= NULL;
	sPSContextData	   *pContext		= NULL;
	bool				nodeNameIsID	= false;
	unsigned int		prefixLen		= sizeof(kPasswordServerPrefixStr) - 1;
	sPSServerEntry		anEntry;
	
	try
	{
		if ( inData != NULL )
		{
			pNodeList = inData->fInDirNodeName;
			pathStr = dsGetPathFromListPriv( pNodeList, (char *)"/" );
            Throw_NULL( pathStr, eDSNullNodeName );
            
            DEBUGLOG( "CPSPlugIn::OpenDirNode path = %s", pathStr);
            
            //special case for the configure PS node?
            if (strcmp(pathStr,"/PasswordServer") == 0)
            {
                // set up the context data now with the relevant parameters for the configure PasswordServer node
                // DS API reference number is used to access the reference table
				/*
                pContext = MakeContextData();
                pContext->psName = new char[1+strlen("PasswordServer Configure")];
                ::strcpy(pContext->psName,"PasswordServer Configure");
                // add the item to the reference table
                gPSContextTable->AddItem( inData->fOutNodeRef, pContext );
				*/
				
				// currently, we do not configure anything so this is a bad node.
				siResult = eDSOpenNodeFailed;
            }
            // check that there is something after the delimiter or prefix
            // strip off the PasswordServer prefix here
            else
            if ( (strlen(pathStr) > prefixLen) && (::strncmp(pathStr,kPasswordServerPrefixStr,prefixLen) == 0) )
            {
				char *debugEnvVar = getenv("PWSDEBUG");
				if ( debugEnvVar != NULL )
				{
					if ( strcmp( debugEnvVar, "0" ) == 0 )
						psfwSetUSR1Debug( false );
					else
						psfwSetUSR1Debug( true );
				}
				
				pContext = MakeContextData();
				
                // add the item to the reference table
                gPSContextTable->AddItem( inData->fOutNodeRef, pContext );
				
                // sasl_client_init() is called when DirectoryService
				// starts. There is no longer a need to do it here.
				                
				subStr = pathStr + prefixLen;
                
				if ( strncmp( subStr, "only/", 5 ) == 0 ) {
					subStr += 5;
					pContext->providedNodeOnlyOrFail = true;
				}
				
                if ( strncmp( subStr, "ipv4/", 5 ) == 0 )
                    subStr += 5;
                else
                if ( strncmp( subStr, "ipv6/", 5 ) == 0 )
                    subStr += 5;
                else
                if ( strncmp( subStr, "dns/", 4 ) == 0 )
                    subStr += 4;
                else
				if ( strncmp( subStr, "id/", 3 ) == 0 ) {
                    subStr += 3;
					nodeNameIsID = true;
				}
				
				if ( nodeNameIsID && strlen(subStr) >= sizeof(anEntry.id) )
					throw( (SInt32)eParameterError );
				else
				if ( strlen(subStr) >= sizeof(anEntry.ip) )
					throw( (SInt32)eParameterError );
				
				bzero( &anEntry, sizeof(anEntry) );
				
				if ( nodeNameIsID )
				{
					strcpy( anEntry.id, subStr );
				}
				else
				{
					int rc;
					int error_num;
					struct in_addr inetAddr;
					struct hostent *hostEnt;
					
					// is it an IP address?
					rc = inet_aton( subStr, &inetAddr );
					if ( rc == 1 )
					{
						strcpy( anEntry.ip, subStr );
					}
					else
					{
						strlcpy( anEntry.dns, subStr, sizeof(anEntry.dns) );
						
						// resolve if possible
						hostEnt = getipnodebyname( anEntry.dns, AF_INET, AI_DEFAULT, &error_num );
						if ( hostEnt != NULL )
						{
							if ( hostEnt->h_addr_list[0] != NULL ) {
								if ( inet_ntop(AF_INET, hostEnt->h_addr_list[0], anEntry.ip, sizeof(anEntry.ip)) == NULL )
									anEntry.ip[0] = 0;
							}
							freehostent( hostEnt );
						}
						
						DEBUGLOG( "anEntry.ip = %s", anEntry.ip );
					}
					
					anEntry.ipFromNode = true;
				}
				
				if ( strcmp(anEntry.ip, "127.0.0.1") == 0 )
				{
					struct stat sb;
					
					if ( lstat(kPWFilePath, &sb) != 0 )
					{
						// no password server on this machine, fail now
						throw( (SInt32)eDSOpenNodeFailed );
					}
				}
				
				pContext->serverProvidedFromNode = anEntry;
				
				strcpy( (char *)pContext->psIV, "D5F:A24A" );
				
				fOpenNodeCount++;
                siResult = eDSNoErr;
				
            } // there was some name passed in here ie. length > 1
            else
            {
                siResult = eDSOpenNodeFailed;
            }
			
        } // inData != NULL
	} // try
	catch( SInt32 err )
	{
		siResult = err;
		if (pContext != NULL)
		{
			gPSContextTable->RemoveItem( inData->fOutNodeRef );
		}
	}
	catch (...)
	{
		siResult = eDSOpenNodeFailed;
	}

	if (pathStr != NULL)
	{
		delete( pathStr );
		pathStr = NULL;
	}

	return( siResult );

} // OpenDirNode

//------------------------------------------------------------------------------------
//	* CloseDirNode
//------------------------------------------------------------------------------------

SInt32 CPSPlugIn::CloseDirNode ( sCloseDirNode *inData )
{
	SInt32				siResult	= eDSNoErr;
	sPSContextData	   *pContext	= NULL;
    
	try
	{
		pContext = (sPSContextData *)gPSContextTable->GetItemData( inData->fInNodeRef );
        Throw_NULL( pContext, eDSBadContextData );
		
		// do whatever to close out the context
		EndServerSession( pContext, kSendQuit );
		
		if ( fOpenNodeCount > 0 )
			fOpenNodeCount--;
		
		this->CleanContextData( pContext );
		
		gPSContextTable->RemoveItem( inData->fInNodeRef );
		
		gPWSConnMutex->Wait();
		gContinue->RemoveItems( inData->fInNodeRef );
		gPWSConnMutex->Signal();
	}
    
	catch( SInt32 err )
	{
		siResult = err;
	}
	catch (...)
	{
		siResult = eDSCloseFailed;
	}
    
	return( siResult );

} // CloseDirNode


// ---------------------------------------------------------------------------
//	* HandleFirstContact
// ---------------------------------------------------------------------------

SInt32
CPSPlugIn::HandleFirstContact(
	sPSContextData *inContext,
	const char *inIP,
	const char *inUserKeyHash,
	const char *inUserKeyStr,
	bool inSecondTime )
{
	SInt32 siResult = kCPSUtilFail;
	char *psName = NULL;
	bool usingLocalCache = false;
	int sock = -1;
	sPSServerEntry anEntry = {0};
	sPSServerEntry *entrylist = NULL;
	CFIndex servIndex = 0;
	CFIndex servCount = 0;
	
	gPWSConnMutex->Wait();
	
	try
	{
		if ( ! inContext->providedNodeOnlyOrFail )
		{
			// check the config record for the current LDAP server
			if ( inContext->replicaFile != nil )
			{
				DEBUGLOG( "CPSPlugIn::HandleFirstContact: trying LDAP Hint" );
				
				siResult = kCPSUtilFail;
				
				// special-case localhost because it's not in the replica table
				CFStringRef ldapServerString = [(ReplicaFile *)inContext->replicaFile currentServerForLDAP];
				if ( ldapServerString != NULL )
				{
					if ( CFStringCompare(ldapServerString, CFSTR("127.0.0.1"), 0) == kCFCompareEqualTo )
					{
						sPSServerEntry localEntry = { 0, 0, 0, "127.0.0.1", kPasswordServerPortStr, "", "", 1 };
						siResult = IdentifyReachableReplicaByIP( &localEntry, 1, inUserKeyHash, &anEntry, &sock );
						DEBUGLOG( "CPSPlugIn::HandleFirstContact: hint is 127.0.0.1, result = %l", siResult );
						
						// retrieve the replica list for caching
						// get entry for the OD master
						if ( siResult == kCPSUtilOK ) {
							pwsf_GetServerListFromConfig( &inContext->serverList, (ReplicaFile *)inContext->replicaFile );
							GetServerFromDict( [(ReplicaFile *)inContext->replicaFile getParent], 0, &inContext->master );
						}
					}
					else
					{
						// Try current ldap IP. Translate the config record plist and extract the entry for ldap.
						siResult = pwsf_GetServerListFromConfig( &inContext->serverList, (ReplicaFile *)inContext->replicaFile );
						if ( siResult == kCPSUtilOK && inContext->serverList != NULL && CFArrayGetCount( inContext->serverList ) > 0 )
						{
							siResult = kCPSUtilFail;
							if ( ConvertCFArrayToServerArray(inContext->serverList, &entrylist, &servCount) == kCPSUtilOK )
							{
								bool currentLDAPServerUnfound = true;
									
								for ( servIndex = 0; servIndex < servCount; servIndex++ )
								{
									if ( entrylist[servIndex].currentServerForLDAP )
									{
										currentLDAPServerUnfound = false;
										siResult = IdentifyReachableReplicaByIP( &entrylist[servIndex], 1, inUserKeyHash, &anEntry, &sock );
										if ( siResult == kCPSUtilOK )
										{
											GetServerFromDict( [(ReplicaFile *)inContext->replicaFile getParent], 0, &inContext->master );
										}
										else
										{
											// if the LDAP entry isn't present, log more info
											DEBUGLOG( "CPSPlugIn::HandleFirstContact: Unable to contact the current LDAP server "
														"(ip = %s).", entrylist[servIndex].ip );
										}
										break;
									}
								}
								
								DSFree( entrylist );
								
								if ( currentLDAPServerUnfound )
								{
									char ldapServerStr[256] = {0};
									
									CFStringGetCString( ldapServerString, ldapServerStr, sizeof(ldapServerStr), kCFStringEncodingUTF8 );
									
									DEBUGLOG( "CPSPlugIn::HandleFirstContact: could not find entry for LDAP hint "
												"(ip = %s)", ldapServerStr );
								}
							}
							else
							{
								DEBUGLOG( "CPSPlugIn::HandleFirstContact: the LDAP config record is invalid." );
							}
						}
						else
						{
							DEBUGLOG( "CPSPlugIn::HandleFirstContact: the LDAP config record has no entries." );
						}
					}
					
					CFRelease( ldapServerString );
				}
				else
				{
					DEBUGLOG( "CPSPlugIn::HandleFirstContact: the LDAP config record does not contain a hint." );
				}
			}
			else
			{
				DEBUGLOG( "CPSPlugIn::HandleFirstContact: LDAP did not provide a config record." );
				siResult = kCPSUtilFail;
			}

			// if that didn't work, try the local replica list cache
			if ( siResult != kCPSUtilOK )
			{
				DEBUGLOG( "CPSPlugIn::HandleFirstContact: trying .local file" );
				DSCFRelease( inContext->serverList );
				
				siResult = GetPasswordServerList( &inContext->serverList, kPWSearchLocalFile );
				if ( siResult == kCPSUtilOK && inContext->serverList != NULL && CFArrayGetCount( inContext->serverList ) > 0 )
				{
					siResult = IdentifyReachableReplica( inContext->serverList, inUserKeyHash, &anEntry, &sock );
					usingLocalCache = (siResult == kCPSUtilOK);
				}
				else
					siResult = kCPSUtilFail;
			}
			
			// if that didn't work, revisit the provided replica list
			if ( siResult != kCPSUtilOK )
			{
				DEBUGLOG( "CPSPlugIn::HandleFirstContact: trying config record" );
				DSCFRelease( inContext->serverList );
				
				siResult = pwsf_GetServerListFromConfig( &inContext->serverList, (ReplicaFile *)inContext->replicaFile );
				if ( siResult == kCPSUtilOK && inContext->serverList != NULL && CFArrayGetCount( inContext->serverList ) > 0 )
				{
					siResult = IdentifyReachableReplica( inContext->serverList, inUserKeyHash, &anEntry, &sock );
					if ( siResult == kCPSUtilOK )
						GetServerFromDict( [(ReplicaFile *)inContext->replicaFile getParent], 0, &inContext->master );
				}
				else
					siResult = kCPSUtilFail;
			}
			
			// next, try the password server's replication database (if this is an OD master or replica)
			if ( siResult != kCPSUtilOK )
			{
				DEBUGLOG( "CPSPlugIn::HandleFirstContact: trying passwordserver's file" );
				DSCFRelease( inContext->serverList );
				
				siResult = GetPasswordServerListForKeyHash( &inContext->serverList, kPWSearchReplicaFile, inUserKeyHash );
				if ( siResult == kCPSUtilOK && inContext->serverList != NULL && CFArrayGetCount( inContext->serverList ) > 0 )
				{
					siResult = IdentifyReachableReplica( inContext->serverList, inUserKeyHash, &anEntry, &sock );
					if ( siResult == kCPSUtilOK )
						GetServerFromDict( [(ReplicaFile *)inContext->replicaFile getParent], 0, &inContext->master );
				}
				else
					siResult = kCPSUtilFail;
			}
			
			// we're desperate now, try Bonjour
			if ( siResult != kCPSUtilOK )
			{
				DEBUGLOG( "CPSPlugIn::HandleFirstContact: trying bonjour" );
				DSCFRelease( inContext->serverList );
				
				siResult = GetPasswordServerList( &inContext->serverList, kPWSearchRegisteredServices );
				if ( siResult == kCPSUtilOK && inContext->serverList != NULL && CFArrayGetCount( inContext->serverList ) > 0 )
					siResult = IdentifyReachableReplica( inContext->serverList, inUserKeyHash, &anEntry, &sock );
				else
					siResult = kCPSUtilFail;
			}
		}
		
		// try node IP
		if ( siResult != kCPSUtilOK )
		{
			DEBUGLOG( "CPSPlugIn::HandleFirstContact: trying node IP" );
			DSCFRelease( inContext->serverList );
			
			siResult = IdentifyReachableReplicaByIP( &inContext->serverProvidedFromNode, 1, inUserKeyHash, &anEntry, &sock );
			if ( siResult == kCPSUtilOK && (!inContext->providedNodeOnlyOrFail) && inUserKeyHash != NULL )
				inContext->askForReplicaList = true;
		}
		
		// try localhost
		if ( !inSecondTime && !inContext->providedNodeOnlyOrFail && siResult != kCPSUtilOK )
		{
			DEBUGLOG( "CPSPlugIn::HandleFirstContact: trying localhost" );
			DSCFRelease( inContext->serverList );
			
			sPSServerEntry localEntry = { 0, 0, 0, "127.0.0.1", kPasswordServerPortStr, "", "", 0 };
			siResult = IdentifyReachableReplicaByIP( &localEntry, 1, inUserKeyHash, &anEntry, &sock );
		}
		
		if ( siResult != kCPSUtilOK || DSIsStringEmpty(anEntry.ip) )
			throw( (SInt32)eDSAuthNoAuthServerFound );
		
		psName = (char *) calloc( 1, strlen(anEntry.ip) + 1 );
		Throw_NULL( psName, eDSNullNodeName );
		
		strcpy( psName, anEntry.ip );
		
		if ( inContext->psName != NULL )
			free( inContext->psName );
		inContext->psName = psName;
		
		strlcpy(inContext->psPort, anEntry.port, 10);
		
		// close disconnected connections before opening new ones
		// release the conn mutex so we don't get deadlocked in
		// the ref table iterator
		gPWSConnMutex->Signal();
		gPSContextTable->DoOnAllItems( CPSPlugIn::ReleaseCloseWaitConnections );
		gPWSConnMutex->Wait();
		
		siResult = BeginServerSession( inContext, sock, inUserKeyHash );
		if ( siResult == eDSNoErr )
		{
			// check for wrong server error
			if ( (inUserKeyStr != NULL) && (!RSAPublicKeysEqual( inUserKeyStr, inContext->rsaPublicKeyStr )) )
			{
				EndServerSession( inContext, kSendQuit );
				siResult = eDSAuthNoAuthServerFound;
				
				// if retrieved from local cache, try flushing the cache and do a fresh search
				if ( usingLocalCache )
				{
					struct stat sb;
					
					if ( lstat( kPWReplicaLocalFile, &sb ) == 0 )
					{
						unlink( kPWReplicaLocalFile );
						siResult = HandleFirstContact( inContext, inIP, inUserKeyHash, inUserKeyStr, true );
					}
				}
			}
			
			// save the cache iff we did a fresh lookup and were not limited to a single IP
			if ( (!usingLocalCache) && (!inContext->providedNodeOnlyOrFail) )
			{
				if ( DSIsStringEmpty(anEntry.id) )
					snprintf( anEntry.id, sizeof(anEntry.id), "%s", inContext->rsaPublicKeyHash );
				(void)SaveLocalReplicaCache( inContext->serverList, &anEntry );
			}
		}
		else
		if ( sock > 0 )
		{
			EndServerSession( inContext, false );
		}
	}
	
	catch( SInt32 error )
	{
		siResult = error;
	}
	catch (...)
	{
		siResult = eDSAuthFailed;
	}
	
	gPWSConnMutex->Signal();
		
	return siResult;
}


// ---------------------------------------------------------------------------
//	* BeginServerSession
//
//	
// ---------------------------------------------------------------------------

SInt32 CPSPlugIn::BeginServerSession( sPSContextData *inContext, int inSock, const char *inUserKeyHash )
{
	SInt32 siResult = eDSNoErr;
	char *tptr = NULL;
	char buf[4096];
	PWServerError serverResult;
	
	try
	{
		DEBUGLOG( "BeginServerSession, ip = %s, inSock = %d", inContext->psName ? inContext->psName : "(null)", inSock );
		
		if ( inSock != -1 )
		{
			EndServerSession( inContext, false );
			inContext->fd = inSock;
			inContext->serverOut = fdopen(inSock, "w");
			
			// get password server version from the greeting
			serverResult = readFromServer(inSock, buf, sizeof(buf));
			if ( serverResult.err == 0 &&
				 (tptr = strstr(buf, "ApplePasswordServer")) != NULL )
			{
				char *cur;
				int index = 0;
				
				tptr += sizeof( "ApplePasswordServer" );
				while ( (cur = strsep(&tptr, ".")) != NULL ) {
					sscanf( cur, "%d", &inContext->serverVers[index++] );
					if ( index >= 4 )
						break;
				}
			}
		}
		else
		{
			// connect to remote server
			siResult = ConnectToServer( inContext );
			if ( siResult != eDSNoErr )
				throw( siResult );
		}
		
		inContext->localaddr[0] = '\0';
		inContext->remoteaddr[0] = '\0';
		
		if ( !inContext->isUNIXDomainSocket )
		{
			// set ip addresses
			socklen_t salen;
			char hbuf[NI_MAXHOST], pbuf[NI_MAXSERV];
			struct sockaddr_storage local_ip;
			
			salen = sizeof(local_ip);
			if (getsockname(inContext->fd, (struct sockaddr *)&local_ip, &salen) < 0) {
				salen = 0;
				DEBUGLOG("getsockname");
			}
			
			if ( salen > 0 )
			{
				getnameinfo((struct sockaddr *)&local_ip, salen,
							hbuf, sizeof(hbuf), pbuf, sizeof(pbuf),
							NI_NUMERICHOST | NI_WITHSCOPEID | NI_NUMERICSERV);
				snprintf(inContext->localaddr, sizeof(inContext->localaddr), "%s;%s", hbuf, pbuf);
				snprintf(inContext->remoteaddr, sizeof(inContext->remoteaddr), "%s;%s", inContext->psName, inContext->psPort);
			}
		}
		
		// retrieve the password server's list of available auth methods
		serverResult = SendFlushReadWithMutex( inContext, "LIST RSAPUBLIC", NULL, NULL, buf, sizeof(buf) );
		if ( serverResult.err != 0 )
			throw( PWSErrToDirServiceError(serverResult) );
		
		sasl_chop(buf);
		
		siResult = GetSASLMechListFromString( buf, &inContext->mech, &inContext->mechCount );
		if ( siResult != eDSNoErr ) {
			DEBUGLOG( "GetSASLMechListFromString = %l", siResult);
			throw( siResult );
		}
		
		// did the rsa public key come too?
		if ( recvfrom( inContext->fd, buf, 1, (MSG_DONTWAIT | MSG_PEEK), NULL, NULL ) > 0 )
		{
			serverResult = readFromServer( inContext->fd, buf, sizeof(buf) );
			if ( serverResult.err != 0 )
				throw( PWSErrToDirServiceError(serverResult) );
			
			siResult = this->GetRSAPublicKey( inContext, buf );
		}
		else
		{
			// old server, don't ask for the replica list
			inContext->askForReplicaList = false;
			
			// retrieve the password server's public RSA key
			siResult = this->GetRSAPublicKey( inContext );
		}
		
		if ( siResult != eDSNoErr )
		{
			DEBUGLOG( "rsapublic = %l", siResult);
			throw( siResult );
		}
		
		if ( inContext->askForReplicaList && inUserKeyHash != NULL )
		{
			char *replicaListData;
			unsigned long replicaListDataLen;
			struct stat sb;
			char filePath[sizeof(kPWReplicaRemoteFilePrefix) + strlen(inUserKeyHash)];

			// construct the cache file name
			strcpy( filePath, kPWReplicaRemoteFilePrefix );
			strcat( filePath, inUserKeyHash );
			
			// only write a replica file if one doesn't already exist
			if ( lstat( filePath, &sb ) != 0 )
			{
				siResult = GetReplicaListFromServer( inContext, &replicaListData, &replicaListDataLen );
				if ( siResult == eDSNoErr )
				{
					ReplicaFile *replicaFile = [[ReplicaFile alloc] initWithXMLStr:replicaListData];
					[replicaFile saveXMLDataToFile:filePath];
					[replicaFile free];
					if ( replicaListData != NULL )
						free( replicaListData );
				}
			}
			inContext->askForReplicaList = false;
		}
	}
	catch( SInt32 error )
	{
		siResult = error;
	}
	catch (...)
	{
		siResult = eDSAuthFailed;
	}
	
	return siResult;
}
    

// ---------------------------------------------------------------------------
//	* EndServerSession
// ---------------------------------------------------------------------------

SInt32 CPSPlugIn::EndServerSession( sPSContextData *inContext, bool inSendQuit )
{
	gPWSConnMutex->Wait();
	
	if ( inSendQuit )
	{
		if ( Connected( inContext ) )
		{
			int result;
			PWServerError serverResult;
			struct timeval recvTimeoutVal = { 0, 150000 };
			char buf[kOneKBuffer];
			
			result = setsockopt( inContext->fd, SOL_SOCKET, SO_RCVTIMEO, &recvTimeoutVal, sizeof(recvTimeoutVal) );
			serverResult = SendFlushReadWithMutex( inContext, "QUIT", NULL, NULL, buf, sizeof(buf) );
        }
	}
	
	if ( inContext->serverOut != NULL ) {
		fpurge( inContext->serverOut );
		fclose( inContext->serverOut );
		inContext->serverOut = NULL;
	}
	if ( inContext->fd > 0 ) {
		close( inContext->fd );
		gCloseCount++;
	}
	// always set to -1
	inContext->fd = -1;
	
	inContext->castKeySet = false;
	bzero( &inContext->rc5Key, sizeof(RC5_32_KEY) );
	
	gPWSConnMutex->Signal();

if ( gOpenCount != gCloseCount )
	DEBUGLOG( "CPSPlugIn::EndServerSession opens: %l, closes %l", gOpenCount, gCloseCount );
	
	return eDSNoErr;
}


// ---------------------------------------------------------------------------
//	* GetSASLMechListFromString
// ---------------------------------------------------------------------------

SInt32 CPSPlugIn::GetSASLMechListFromString( const char *inSASLList, AuthMethName **mechList, int *mechCount )
{
	const char *tptr = NULL;
	const char *end = NULL;
	int count = 0;
	
	if ( inSASLList == NULL || mechList == NULL || mechCount == NULL )
		return eParameterError;
	
	*mechList = NULL;
	*mechCount = 0;
	
	tptr = inSASLList;
	for ( count = 0; tptr != NULL; count++ ) {
		tptr = strchr( tptr, ' ' );
		if ( tptr != NULL )
			tptr++;
	}
	
	if ( count == 0 )
		return eDSNoErr;
	
	*mechList = (AuthMethName *)calloc(count, sizeof(AuthMethName));
	if ( *mechList == NULL )
		return eMemoryAllocError;
	*mechCount = count;
	
	tptr = strstr( inSASLList, kSASLListPrefix );
	if ( tptr != NULL )
	{
		tptr += sizeof( kSASLListPrefix ) - 1;
		
		for ( ; tptr && count > 0; count-- )
		{
			if ( *tptr == '\"' )
				tptr++;
			else
				break;
			
			end = strchr( tptr, '\"' );
			if ( end != NULL )
				strlcpy( (*mechList)[count-1].method, tptr, end - tptr + 1 );
			else
				strcpy( (*mechList)[count-1].method, tptr );
			
			tptr = end;
			if ( tptr != NULL )
				tptr += 2;
		}
	}
	
	return eDSNoErr;
}


// ---------------------------------------------------------------------------
//	* GetRSAPublicKey
// ---------------------------------------------------------------------------

SInt32 CPSPlugIn::GetRSAPublicKey( sPSContextData *inContext, char *inData )
{
	SInt32				siResult			= eDSNoErr;
	PWServerError		serverResult;
    char				buf[kOneKBuffer];
    char				*keyStr;
	char				*bufPtr				= NULL;
    int					bits				= 0;
    
	try
	{
        Throw_NULL( inContext, eDSBadContextData );
        
		if ( inData == NULL )
		{
			// get string
			serverResult = SendFlushReadWithMutex( inContext, "RSAPUBLIC", NULL, NULL, buf, sizeof(buf) );
			if ( serverResult.err != 0 )
			{
				DEBUGLOG( "no public key");
				throw( (SInt32)eDSAuthServerError );
			}
			
			bufPtr = buf;
        }
		else
		{
			bufPtr = inData;
		}
		
		sasl_chop( bufPtr );
		inContext->rsaPublicKeyStr = (char *) calloc( 1, strlen(bufPtr)+1 );
        Throw_NULL( inContext->rsaPublicKeyStr, eMemoryAllocError );
		
		strcpy( inContext->rsaPublicKeyStr, bufPtr + 4 );
		
		// get as struct
        inContext->rsaPublicKey = key_new( KEY_RSA );
        Throw_NULL( inContext->rsaPublicKey, eDSAllocationFailed );
        
        keyStr = bufPtr + 4;
        bits = pwsf_key_read(inContext->rsaPublicKey, &keyStr);
        if (bits == 0) {
            DEBUGLOG( "no key bits");
            throw( (SInt32)eDSAuthServerError );
        }
		
		// calculate the hash of the ID for comparison
		pwsf_CalcServerUniqueID( inContext->rsaPublicKeyStr, inContext->rsaPublicKeyHash );
 	}
	
	catch( SInt32 err )
	{
		DEBUGLOG( "catch in GetRSAPublicKey = %l", err);
		siResult = err;
	}
	catch (...)
	{
		siResult = eDSAuthServerError;
	}

    return siResult;
}


// ---------------------------------------------------------------------------
//	* DoRSAValidation
// ---------------------------------------------------------------------------

SInt32 CPSPlugIn::DoRSAValidation ( sPSContextData *inContext, const char *inUserKey )
{
	SInt32				siResult			= eDSNoErr;
	char				*encodedStr			= NULL;
    PWServerError		serverResult;
    char				buf[2 * kOneKBuffer];
	char				*bnStr				= NULL;
	int					len;
    MD5_CTX				ctx;
	int					extra				= 384;
	unsigned char		md5Result[MD5_DIGEST_LENGTH];
	
	gPWSConnMutex->Wait();
	
	try
	{
        Throw_NULL( inContext, eDSBadContextData );
		
		// servers must have a public key
		if ( inContext->rsaPublicKey == NULL )
			throw( (SInt32)eDSAuthServerError );
		
		// make sure we are talking to the right server
		if ( ! RSAPublicKeysEqual( inContext->rsaPublicKeyStr, inUserKey ) )
			throw( (SInt32)eDSAuthServerError );
		
		siResult = GetBigNumber( inContext, &bnStr );
        switch ( siResult )
		{
			case kCPSUtilOK:
				break;
			case kCPSUtilMemoryError:
				throw( (SInt32)eMemoryError );
				break;
			case kCPSUtilParameterError:
				throw( (SInt32)eParameterError );
				break;
			default:
				throw( (SInt32)eDSAuthFailed );
		}
		
        int nonceLen = strlen(bnStr);
		
		// just to be safe
		if ( nonceLen > 256 ) {
			bnStr[256] = '\0';
			nonceLen = 256;
		}
		
		// add some scratch space for the RSA padding
		if ( inContext->rsaPublicKey->rsa != NULL && inContext->rsaPublicKey->rsa->n != NULL )
			extra = RSA_size( inContext->rsaPublicKey->rsa );
		
        encodedStr = (char *) malloc( nonceLen + sizeof(kDSRequestNonceHashStr) + extra );
        Throw_NULL( encodedStr, eMemoryError );
        
		// put the "HASH" text on the end to make a plain text attack a little more work
		// The "HASH" text must be encrypted to prevent a man-in-the-middle from stripping it off
		strcpy( buf, bnStr );
		strcat( buf, kDSRequestNonceHashStr );
		
		len = RSA_public_encrypt(nonceLen + sizeof(kDSRequestNonceHashStr),
								 (unsigned char *)buf,
								 (unsigned char *)encodedStr,
								 inContext->rsaPublicKey->rsa,
								 RSA_PKCS1_PADDING);
		
        if ( len <= 0 ) {
            DEBUGLOG( "rsa_public_encrypt() failed");
            throw( (SInt32)eDSAuthServerError );
        }
        
        if ( ConvertBinaryTo64( encodedStr, (unsigned)len, buf ) == SASL_OK )
        {
            unsigned long encodedStrLen;
            
			serverResult = SendFlush( inContext, "RSAVALIDATE", buf, NULL );
            if ( serverResult.err != 0 )
				throw( PWSErrToDirServiceError(serverResult) );
			
			// The response to RSAVALIDATE always comes back unencrypted.
			inContext->castKeySet = false;
            serverResult = readFromServer( inContext->fd, buf, sizeof(buf) );
			if ( serverResult.err != 0 )
				throw( PWSErrToDirServiceError(serverResult) );
			
			// Assume failure and clear the error on success
			siResult = eDSAuthServerError;
			
			if ( Convert64ToBinary( buf + 4, encodedStr, kOneKBuffer, &encodedStrLen ) == SASL_OK )
            {
                encodedStr[nonceLen] = '\0';
                
				MD5_Init( &ctx );
				MD5_Update( &ctx, bnStr, nonceLen );
				MD5_Final( md5Result, &ctx );
				
				if ( encodedStrLen >= MD5_DIGEST_LENGTH )
				{
					// check for both hash and plain in case we are talking to a Jaguar server
					if ( memcmp(md5Result, encodedStr, MD5_DIGEST_LENGTH) == 0 )
					{
						CAST_set_key( &inContext->castKey, nonceLen, (unsigned char *)bnStr );
						bzero( inContext->castIV, sizeof(inContext->castIV) );
						bzero( inContext->castReceiveIV, sizeof(inContext->castReceiveIV) );
						inContext->castKeySet = true;
						siResult = eDSNoErr;
					}
					else
					{
						if ( memcmp(bnStr, encodedStr, nonceLen) == 0 )
							siResult = eDSNoErr;
					}
				}
            }
		}
		else
		{
			siResult = eDSAuthFailed;
		}
 	}
    
	catch( SInt32 err )
	{
		siResult = err;
	}       
	catch (...)
	{
		siResult = eDSAuthServerError;
	}
	
	gPWSConnMutex->Signal();
	
	if ( bnStr != NULL )
		free( bnStr );
    if ( encodedStr != NULL )
        free( encodedStr );
    
    return siResult;
}


// ---------------------------------------------------------------------------
//	* SetupSecureSyncSession
// ---------------------------------------------------------------------------

SInt32 CPSPlugIn::SetupSecureSyncSession( sPSContextData *inContext )
{
	SInt32				siResult					= eDSNoErr;
	char				*encryptedStr				= NULL;
	char				*decryptedStr				= NULL;
	char				*bnStr						= NULL;
	CAuthFileBase		*authFile					= NULL;
	char				*encNonceStr				= NULL;
    PWServerError		serverResult;
    char				buf[kOneKBuffer];
	char				base64Buf[kOneKBuffer];
	int					len;
	MD5_CTX				ctx;
    unsigned char		md5Result[MD5_DIGEST_LENGTH];
	unsigned long		encryptedStrLen;
	UInt32				randomLength;
	UInt32				nonceLength;
	time_t				now;
	char				timeBuf[30];
	
	if ( inContext == NULL )
		return eDSBadContextData;
	
	// already set up?
	if ( SecureSyncSessionIsSetup(inContext) )
		return eDSNoErr;
	
	gPWSConnMutex->Wait();
	
	try
	{
		// load RSA private key from Password Server database
		authFile = new CAuthFileBase();
		if ( authFile == NULL )
			throw( (SInt32)eMemoryError );
		
		siResult = authFile->validateFiles();
		if ( siResult != 0 )
			throw( (SInt32)eDSAuthNoAuthServerFound );
		
		if ( authFile->loadRSAKeys() != 1 )
			throw( (SInt32)eDSAuthServerError );
		
		siResult = GetBigNumber( inContext, &bnStr );
        switch ( siResult )
		{
			case kCPSUtilOK:
				if ( bnStr == NULL )
					throw( (SInt32)eDSAuthFailed );
				break;
			case kCPSUtilMemoryError:
				throw( (SInt32)eMemoryError );
				break;
			case kCPSUtilParameterError:
				throw( (SInt32)eParameterError );
				break;
			default:
				throw( (SInt32)eDSAuthFailed );
		}
		
        int nonceLen = strlen(bnStr);
        encryptedStr = (char *) calloc(1, kOneKBuffer);
        Throw_NULL( encryptedStr, eMemoryError );
        
		len = RSA_public_encrypt(nonceLen + 1,
								 (unsigned char *)bnStr,
								 (unsigned char *)encryptedStr,
								 inContext->rsaPublicKey->rsa,
								 RSA_PKCS1_PADDING);
		
        if ( len <= 0 ) {
            DEBUGLOG( "rsa_public_encrypt() failed");
            throw( (SInt32)eDSAuthServerError );
        }
        
        if ( ConvertBinaryTo64( encryptedStr, (unsigned)len, buf ) != SASL_OK )
        {
			DEBUGLOG( "ConvertBinaryTo64() failed");
            throw( (SInt32)eParameterError );
		}
		
		time( &now );
		snprintf( timeBuf, sizeof(timeBuf), "%lu", (unsigned long)now );
		
		serverResult = SendFlushReadWithMutex( inContext, "SYNC SESSIONKEY", buf, timeBuf, buf, sizeof(buf) );
		if ( serverResult.err != 0 )
			throw( PWSErrToDirServiceError(serverResult) );
		
		if ( Convert64ToBinary( buf + 4, encryptedStr, kOneKBuffer, &encryptedStrLen ) != SASL_OK )
		{
			DEBUGLOG( "Convert64ToBinary() failed");
			DEBUGLOG( "value = %s", buf + 4 );
            throw( (SInt32)eParameterError );
		}
		if ( encryptedStrLen < 8 )
		{
			DEBUGLOG( "not enough data returned from SYNC SESSIONKEY");
            throw( (SInt32)eDSAuthServerError );
		}
		
		// get the RSA-encrypted random
		randomLength = ntohl( *((unsigned long *)encryptedStr) );
		DEBUGLOG( "length of random from SYNC SESSIONKEY is %l", randomLength );
		if ( randomLength > 128 )
		{
			DEBUGLOG( "length of random from SYNC SESSIONKEY is too long");
            throw( (SInt32)eDSAuthServerError );
		}
		
		decryptedStr = (char *) malloc( randomLength + RSA_PKCS1_PADDING_SIZE + 1 );
		if ( decryptedStr == NULL )
			throw( (SInt32)eMemoryError );
		
		siResult = authFile->decryptRSA( (unsigned char *)encryptedStr + 4, randomLength, (unsigned char *)decryptedStr );
		decryptedStr[randomLength] = '\0';
		if ( siResult != 0 )
			throw( (SInt32)eDSNotAuthorized );
		
		// get the rc5-encrypted nonce
		nonceLength = ntohl( *((unsigned long *)(encryptedStr + 4 + randomLength)) );
		if ( nonceLength > 1024 )
			throw( (SInt32)eDSAuthServerError );
		
		encNonceStr = (char *) malloc( nonceLength + RC5_32_BLOCK + 1 );
		if ( encNonceStr == NULL )
			throw( (SInt32)eMemoryError );
		
		memcpy( encNonceStr, encryptedStr + 4 + randomLength + 4, RC5_32_BLOCK );
		
		// make the composite session key
		MD5_Init( &ctx );
		MD5_Update( &ctx, decryptedStr, strlen(decryptedStr) );
		MD5_Update( &ctx, bnStr, nonceLen );
		MD5_Final( md5Result, &ctx );
		RC5_32_set_key( &inContext->rc5Key, MD5_DIGEST_LENGTH, md5Result, RC5_16_ROUNDS );
		
		// decrypt the server's nonce
		RC5_32_ecb_encrypt( (unsigned char *)encNonceStr, (unsigned char *)buf, &inContext->rc5Key, RC5_DECRYPT );
		buf[nonceLength] = '\0';
				
		// make rc5-encrypted nonce+1
		nonceLen = strlen( buf );
		for (int idx = 0; idx < nonceLen; idx++ )
			buf[idx]++;
		
		// include the 0-terminator
		nonceLen++;
		
		// encrypt
		for (int idx = 0; idx < nonceLen; idx += RC5_32_BLOCK )
			RC5_32_ecb_encrypt( (unsigned char *)buf + idx, (unsigned char *)encNonceStr + idx, &inContext->rc5Key, RC5_ENCRYPT );
				
		// send padded
		nonceLen = (nonceLen/8)*8 + 8;
		if ( ConvertBinaryTo64( encNonceStr, nonceLen, base64Buf ) == SASL_OK )
		{
			// send it
			serverResult = SendFlushReadWithMutex( inContext, "SYNC SESSIONKEYV", base64Buf, NULL, buf, sizeof(buf) );
			if ( serverResult.err == kAuthUserNotAuthenticated && serverResult.type == kPolicyError )
				siResult = eDSNotAuthorized;
			else
				siResult = PWSErrToDirServiceError( serverResult );
		}
 	}
    
	catch( SInt32 err )
	{
		siResult = err;
	}       
 	catch (...)
	{
		siResult = eDSAuthServerError;
	}
	   
	gPWSConnMutex->Signal();
	
	if ( bnStr != NULL )
		free( bnStr );
    if ( encryptedStr != NULL )
        free( encryptedStr );
    if ( decryptedStr != NULL )
        free( decryptedStr );
	if ( encNonceStr != NULL )
		free( encNonceStr );
    if ( authFile != NULL )
	{
		delete authFile;
	}
	
	if ( siResult != eDSNoErr )
		DEBUGLOG( "CPSPlugin::SetupSecureSyncSession returning %l", siResult );
	
    return siResult;
}


// ---------------------------------------------------------------------------
//	* SecureSyncSessionIsSetup
// ---------------------------------------------------------------------------

bool CPSPlugIn::SecureSyncSessionIsSetup ( sPSContextData *inContext )
{
	RC5_32_KEY zeroKey;
	bzero( &zeroKey, sizeof(RC5_32_KEY) );
	return ( memcmp( &zeroKey, &inContext->rc5Key, sizeof(RC5_32_KEY) ) != 0 );
}


// ---------------------------------------------------------------------------
//	* MakeContextData
// ---------------------------------------------------------------------------

sPSContextData* CPSPlugIn::MakeContextData ( void )
{
    sPSContextData  	*pOut		= NULL;
    SInt32				siResult	= eDSNoErr;

    pOut = (sPSContextData *) calloc(1, sizeof(sPSContextData));
    if ( pOut != NULL )
    {
        //do nothing with return here since we know this is new
        //and we did a calloc above
        siResult = CleanContextData(pOut);
    }

    return( pOut );

} // MakeContextData

// ---------------------------------------------------------------------------
//	* CleanContextData
// ---------------------------------------------------------------------------

SInt32 CPSPlugIn::CleanContextData ( sPSContextData *inContext )
{
    SInt32	siResult = eDSNoErr;
	   
    if ( inContext == NULL )
    {
		DEBUGLOG( "CPSPlugIn::CleanContextData eDSBadContextData");
        siResult = eDSBadContextData;
	}
    else
    {
		sasl_conn_t *lconn = NULL;
			
		// although CleanContextData() does not use the passwordserver connection directly,
		// it needs to block the connection mutex during clean-up.
		gPWSConnMutex->Wait();
		
		lconn = inContext->conn;
		inContext->conn = NULL;
		
        if (inContext->psName != NULL)
        {
            free( inContext->psName );
            inContext->psName = NULL;
        }
		
        inContext->offset = 0;
        
		EndServerSession( inContext );
        
        if (inContext->rsaPublicKeyStr != NULL)
		{
			free(inContext->rsaPublicKeyStr);
			inContext->rsaPublicKeyStr = NULL;
		}
		
		if (inContext->rsaPublicKey != NULL)
		{
			key_free(inContext->rsaPublicKey);
			inContext->rsaPublicKey = NULL;
		}
		
        if (inContext->mech != NULL)
        {
            free(inContext->mech);
            inContext->mech = NULL;
        }
        
        inContext->mechCount = 0;
        
        bzero(inContext->last.username, sizeof(inContext->last.username));
        
        if (inContext->last.password != NULL)
        {
            bzero(inContext->last.password, inContext->last.passwordLen);
            free(inContext->last.password);
            inContext->last.password = NULL;
        }
        inContext->last.passwordLen = 0;
        inContext->last.successfulAuth = false;
        
        bzero(inContext->nao.username, sizeof(inContext->nao.username));
        
        if (inContext->nao.password != NULL)
        {
            bzero(inContext->nao.password, inContext->nao.passwordLen);
            free(inContext->nao.password);
            inContext->nao.password = NULL;
        }
        inContext->nao.passwordLen = 0;
        inContext->nao.successfulAuth = false;
		
		if ( inContext->replicaFile != nil )
		{
			[(ReplicaFile *)inContext->replicaFile free];
			inContext->replicaFile = nil;
		}
		
		DSCFRelease( inContext->serverList );
		
		if ( inContext->syncFilePath != NULL )
		{
			unlink( inContext->syncFilePath );
			free( inContext->syncFilePath );
			inContext->syncFilePath = NULL;
		}
		
		bzero( &inContext->rc5Key, sizeof(RC5_32_KEY) );
		inContext->madeFirstContact = false;
		
		DSFreeString( inContext->serviceInfoStr );
		
		gPWSConnMutex->Signal();
		
		if (lconn != NULL)
        {
			// not taking any chances on dead-lock so release the conn mutex before
			// grabbing the sasl mutex
			gSASLMutex->Wait();
            sasl_dispose(&lconn);
            gSASLMutex->Signal();
			lconn = NULL;
        }
	}
	
    return( siResult );

} // CleanContextData

//------------------------------------------------------------------------------------
//	* GetAttributeEntry
//------------------------------------------------------------------------------------

SInt32 CPSPlugIn::GetAttributeEntry ( sGetAttributeEntry *inData )
{
	SInt32					siResult			= eDSNoErr;
	UInt16					usAttrTypeLen		= 0;
	UInt16					usAttrCnt			= 0;
	UInt16					usAttrLen			= 0;
	UInt16					usValueCnt			= 0;
	UInt16					usValueLen			= 0;
	UInt32					i					= 0;
	UInt32					uiIndex				= 0;
	UInt32					uiAttrEntrySize		= 0;
	UInt32					uiOffset			= 0;
	UInt32					uiTotalValueSize	= 0;
	UInt32					offset				= 4;
	UInt32					buffSize			= 0;
	UInt32					buffLen				= 0;
	char				   *p			   		= NULL;
	char				   *pAttrType	   		= NULL;
	tDataBufferPtr			pDataBuff			= NULL;
	tAttributeEntryPtr		pAttribInfo			= NULL;
	sPSContextData		   *pAttrContext		= NULL;
	sPSContextData		   *pValueContext		= NULL;

	try
	{
		Throw_NULL( inData, eMemoryError );

		pAttrContext = (sPSContextData *)gPSContextTable->GetItemData( inData->fInAttrListRef );
		Throw_NULL( pAttrContext, eDSBadContextData );

		uiIndex = inData->fInAttrInfoIndex;
		if (uiIndex == 0)
            throw( (SInt32)eDSInvalidIndex );
        
		pDataBuff = inData->fInOutDataBuff;
		Throw_NULL( pDataBuff, eDSNullDataBuff );
		
		buffSize	= pDataBuff->fBufferSize;
		//buffLen		= pDataBuff->fBufferLength;
		//here we can't use fBufferLength for the buffLen SINCE the buffer is packed at the END of the data block
		//and the fBufferLength is the overall length of the data for all blocks at the end of the data block
		//the value ALSO includes the bookkeeping data at the start of the data block
		//so we need to read it here

		p		= pDataBuff->fBufferData + pAttrContext->offset;
		offset	= pAttrContext->offset;

		// Do record check, verify that offset is not past end of buffer, etc.
		if ( 2 > (SInt32)(buffSize - offset) )
            throw( (SInt32)eDSInvalidBuffFormat );
        
		// Get the attribute count
		memcpy( &usAttrCnt, p, 2 );
		if (uiIndex > usAttrCnt)
            throw( (SInt32)eDSInvalidIndex );
        
		// Move 2 bytes
		p		+= 2;
		offset	+= 2;

		// Skip to the attribute that we want
		for ( i = 1; i < uiIndex; i++ )
		{
			// Do record check, verify that offset is not past end of buffer, etc.
			if (2 > (SInt32)(buffSize - offset) )
                throw( (SInt32)eDSInvalidBuffFormat );
            
			// Get the length for the attribute
			memcpy( &usAttrLen, p, 2 );

			// Move the offset past the length word and the length of the data
			p		+= 2 + usAttrLen;
			offset	+= 2 + usAttrLen;
		}

		// Get the attribute offset
		uiOffset = offset;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 > (SInt32)(buffSize - offset))
            throw( (SInt32)eDSInvalidBuffFormat );
		
		// Get the length for the attribute block
		memcpy( &usAttrLen, p, 2 );

		// Skip past the attribute length
		p		+= 2;
		offset	+= 2;

		//set the bufLen to stricter range
		buffLen = offset + usAttrLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 > (SInt32)(buffLen - offset))
            throw( (SInt32)eDSInvalidBuffFormat );
		
		// Get the length for the attribute type
		memcpy( &usAttrTypeLen, p, 2 );
		
		pAttrType = p + 2;
		p		+= 2 + usAttrTypeLen;
		offset	+= 2 + usAttrTypeLen;
		
		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 > (SInt32)(buffLen - offset))
            throw( (SInt32)eDSInvalidBuffFormat );
		
		// Get number of values for this attribute
		memcpy( &usValueCnt, p, 2 );
		
		p		+= 2;
		offset	+= 2;
		
		for ( i = 0; i < usValueCnt; i++ )
		{
			// Do record check, verify that offset is not past end of buffer, etc.
			if (2 > (SInt32)(buffLen - offset))
                throw( (SInt32)eDSInvalidBuffFormat );
            
			// Get the length for the value
			memcpy( &usValueLen, p, 2 );
			
			p		+= 2 + usValueLen;
			offset	+= 2 + usValueLen;
			
			uiTotalValueSize += usValueLen;
		}

		uiAttrEntrySize = sizeof( tAttributeEntry ) + usAttrTypeLen + kBuffPad;
		pAttribInfo = (tAttributeEntry *)::calloc( 1, uiAttrEntrySize );

		pAttribInfo->fAttributeValueCount				= usValueCnt;
		pAttribInfo->fAttributeDataSize					= uiTotalValueSize;
		pAttribInfo->fAttributeValueMaxSize				= 512;				// <- need to check this xxxxx
		pAttribInfo->fAttributeSignature.fBufferSize	= usAttrTypeLen + kBuffPad;
		pAttribInfo->fAttributeSignature.fBufferLength	= usAttrTypeLen;
		memcpy( pAttribInfo->fAttributeSignature.fBufferData, pAttrType, usAttrTypeLen );

		pValueContext = MakeContextData();
		Throw_NULL( pValueContext , eMemoryAllocError );

		pValueContext->offset = uiOffset;

		gPSContextTable->AddItem( inData->fOutAttrValueListRef, pValueContext );

		inData->fOutAttrInfoPtr = pAttribInfo;
	}

	catch( SInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // GetAttributeEntry


//------------------------------------------------------------------------------------
//	* GetAttributeValue
//------------------------------------------------------------------------------------

SInt32 CPSPlugIn::GetAttributeValue ( sGetAttributeValue *inData )
{
	SInt32						siResult		= eDSNoErr;
	UInt16						usValueCnt		= 0;
	UInt16						usValueLen		= 0;
	UInt16						usAttrNameLen	= 0;
	UInt32						i				= 0;
	UInt32						uiIndex			= 0;
	UInt32						offset			= 0;
	char					   *p				= NULL;
	tDataBuffer				   *pDataBuff		= NULL;
	tAttributeValueEntry	   *pAttrValue		= NULL;
	sPSContextData		   *pValueContext	= NULL;
	UInt32						buffSize		= 0;
	UInt32						buffLen			= 0;
	UInt16						attrLen			= 0;

	try
	{
		pValueContext = (sPSContextData *)gPSContextTable->GetItemData( inData->fInAttrValueListRef );
		Throw_NULL( pValueContext , eDSBadContextData );

		uiIndex = inData->fInAttrValueIndex;
		if (uiIndex == 0)
            throw( (SInt32)eDSInvalidIndex );
		
		pDataBuff = inData->fInOutDataBuff;
		Throw_NULL( pDataBuff , eDSNullDataBuff );

		buffSize	= pDataBuff->fBufferSize;
		//buffLen		= pDataBuff->fBufferLength;
		//here we can't use fBufferLength for the buffLen SINCE the buffer is packed at the END of the data block
		//and the fBufferLength is the overall length of the data for all blocks at the end of the data block
		//the value ALSO includes the bookkeeping data at the start of the data block
		//so we need to read it here

		p		= pDataBuff->fBufferData + pValueContext->offset;
		offset	= pValueContext->offset;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 > (SInt32)(buffSize - offset))
            throw( (SInt32)eDSInvalidBuffFormat );
				
		// Get the buffer length
		memcpy( &attrLen, p, 2 );

		//now add the offset to the attr length for the value of buffLen to be used to check for buffer overruns
		//AND add the length of the buffer length var as stored ie. 2 bytes
		buffLen		= attrLen + pValueContext->offset + 2;
		if (buffLen > buffSize)
            throw( (SInt32)eDSInvalidBuffFormat );
        
		// Skip past the attribute length
		p		+= 2;
		offset	+= 2;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 > (SInt32)(buffLen - offset))
            throw( (SInt32)eDSInvalidBuffFormat );
		
		// Get the attribute name length
		memcpy( &usAttrNameLen, p, 2 );
		
		p		+= 2 + usAttrNameLen;
		offset	+= 2 + usAttrNameLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 > (SInt32)(buffLen - offset))
            throw( (SInt32)eDSInvalidBuffFormat );
		
		// Get the value count
		memcpy( &usValueCnt, p, 2 );
		
		p		+= 2;
		offset	+= 2;

		if (uiIndex > usValueCnt)
            throw( (SInt32)eDSInvalidIndex );

		// Skip to the value that we want
		for ( i = 1; i < uiIndex; i++ )
		{
			// Do record check, verify that offset is not past end of buffer, etc.
			if (2 > (SInt32)(buffLen - offset))
                throw( (SInt32)eDSInvalidBuffFormat );
		
			// Get the length for the value
			memcpy( &usValueLen, p, 2 );
			
			p		+= 2 + usValueLen;
			offset	+= 2 + usValueLen;
		}

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 > (SInt32)(buffLen - offset))
            throw( (SInt32)eDSInvalidBuffFormat );
		
		memcpy( &usValueLen, p, 2 );
		
		p		+= 2;
		offset	+= 2;

		//if (usValueLen == 0) throw (eDSInvalidBuffFormat ); //if zero is it okay?
        
		pAttrValue = (tAttributeValueEntry *)::calloc( 1, sizeof( tAttributeValueEntry ) + usValueLen + kBuffPad );
        Throw_NULL(pAttrValue, eMemoryAllocError);
        
		pAttrValue->fAttributeValueData.fBufferSize		= usValueLen + kBuffPad;
		pAttrValue->fAttributeValueData.fBufferLength	= usValueLen;
		
		// Do record check, verify that offset is not past end of buffer, etc.
		if ( usValueLen > (SInt32)(buffLen - offset) )
            throw ( (SInt32)eDSInvalidBuffFormat );
		
		memcpy( pAttrValue->fAttributeValueData.fBufferData, p, usValueLen );

		// Set the attribute value ID
		pAttrValue->fAttributeValueID = CalcCRC( pAttrValue->fAttributeValueData.fBufferData );

		inData->fOutAttrValue = pAttrValue;
			
	}

	catch ( SInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // GetAttributeValue



// ---------------------------------------------------------------------------
//	* CalcCRC
// ---------------------------------------------------------------------------

UInt32 CPSPlugIn::CalcCRC ( char *inStr )
{
	char		   *p			= inStr;
	SInt32			siI			= 0;
	SInt32			siStrLen	= 0;
	UInt32			uiCRC		= 0xFFFFFFFF;
	CRCCalc			aCRCCalc;

	if ( inStr != NULL )
	{
		siStrLen = strlen( inStr );

		for ( siI = 0; siI < siStrLen; ++siI )
		{
			uiCRC = aCRCCalc.UPDC32( *p, uiCRC );
			p++;
		}
	}

	return( uiCRC );

} // CalcCRC


//------------------------------------------------------------------------------------
//	* GetDirNodeInfo
//------------------------------------------------------------------------------------

SInt32 CPSPlugIn::GetDirNodeInfo ( sGetDirNodeInfo *inData )
{
	SInt32				siResult		= eDSNoErr;
	UInt32				uiOffset		= 0;
	UInt32				uiCntr			= 1;
	UInt32				uiAttrCnt		= 0;
	CAttributeList	   *inAttrList		= NULL;
	char			   *pAttrName		= NULL;
	char			   *pData			= NULL;
	sPSContextData	   *pContext		= NULL;
	sPSContextData	   *pAttrContext	= NULL;
	CBuff				outBuff;
	CDataBuff		   *aRecData		= NULL;
	CDataBuff		   *aAttrData		= NULL;
	CDataBuff		   *aTmpData		= NULL;
	bool				nodeNameIsID	= false;
	
// Can extract here the following:
// kDSAttributesAll
// kDSNAttrNodePath
// kDS1AttrReadOnlyNode
// kDSNAttrAuthMethod
//KW need to add mappings info next

	try
	{
		Throw_NULL( inData , eMemoryError );

		pContext = (sPSContextData *)gPSContextTable->GetItemData( inData->fInNodeRef );
		Throw_NULL( pContext , eDSBadContextData );

		inAttrList = new CAttributeList( inData->fInDirNodeInfoTypeList );
		Throw_NULL( inAttrList, eDSNullNodeInfoTypeList );
		if (inAttrList->GetCount() == 0)
            throw( (SInt32)eDSEmptyNodeInfoTypeList );

		siResult = outBuff.Initialize( inData->fOutDataBuff, true );
		if ( siResult != eDSNoErr )
            throw( siResult );

		siResult = outBuff.SetBuffType( 'Gdni' );  //can't use 'StdB' since a tRecordEntry is not returned
		if ( siResult != eDSNoErr )
            throw( siResult );
        
		aRecData = new CDataBuff();
		Throw_NULL( aRecData , eMemoryError );
		aAttrData = new CDataBuff();
		Throw_NULL( aAttrData , eMemoryError );
		aTmpData = new CDataBuff();
		Throw_NULL( aTmpData , eMemoryError );

		// contact the password server to get the SASL list
		nodeNameIsID = (pContext->serverProvidedFromNode.id[0] != '\0');
		siResult = HandleFirstContact( pContext,
										nodeNameIsID ? NULL : pContext->serverProvidedFromNode.ip,
										nodeNameIsID ? pContext->serverProvidedFromNode.id : NULL );
		if ( siResult == eDSNoErr )
			pContext->madeFirstContact = true;
		
		// Even if the server is unreachable, we can report what we know
		siResult = eDSNoErr;
		
		// Set the record name and type
		aRecData->AppendShort( strlen( "dsAttrTypeStandard:DirectoryNodeInfo" ) );
		aRecData->AppendString( (char *)"dsAttrTypeStandard:DirectoryNodeInfo" );
		aRecData->AppendShort( strlen( "DirectoryNodeInfo" ) );
		aRecData->AppendString( (char *)"DirectoryNodeInfo" );

		while ( inAttrList->GetAttribute( uiCntr++, &pAttrName ) == eDSNoErr )
		{
			//package up all the dir node attributes dependant upon what was asked for
			if ((strcmp( pAttrName, kDSAttributesAll ) == 0) ||
				(strcmp( pAttrName, kDSNAttrNodePath ) == 0) )
			{
				aTmpData->Clear();
				
				uiAttrCnt++;
			
				// Append the attribute name
				aTmpData->AppendShort( strlen( kDSNAttrNodePath ) );
				aTmpData->AppendString( kDSNAttrNodePath );

				if ( inData->fInAttrInfoOnly == false )
				{
					// Attribute value count always two
					aTmpData->AppendShort( 2 );
					
					// Append attribute value
					aTmpData->AppendShort( strlen( "PasswordServer" ) );
					aTmpData->AppendString( (char *)"PasswordServer" );

					char *tmpStr = NULL;
					if (pContext->psName != NULL)
					{
						tmpStr = new char[1+strlen(pContext->psName)];
						::strcpy( tmpStr, pContext->psName );
					}
					else
					{
						tmpStr = new char[1+strlen("Unknown Node Location")];
						::strcpy( tmpStr, "Unknown Node Location" );
					}
					
					// Append attribute value
					aTmpData->AppendShort( strlen( tmpStr ) );
					aTmpData->AppendString( tmpStr );

					delete( tmpStr );

				} // fInAttrInfoOnly is false
				
				// Add the attribute length
				aAttrData->AppendShort( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
                aTmpData->Clear();
			} // kDSAttributesAll or kDSNAttrNodePath
			
			if ( (strcmp( pAttrName, kDSAttributesAll ) == 0) || 
				 (strcmp( pAttrName, kDS1AttrReadOnlyNode ) == 0) )
			{
				aTmpData->Clear();

				uiAttrCnt++;

				// Append the attribute name
				aTmpData->AppendShort( strlen( kDS1AttrReadOnlyNode ) );
				aTmpData->AppendString( kDS1AttrReadOnlyNode );

				if ( inData->fInAttrInfoOnly == false )
				{
					// Attribute value count
					aTmpData->AppendShort( 1 );

					//possible for a node to be ReadOnly, ReadWrite, WriteOnly
					//note that ReadWrite does not imply fully readable or writable
					
					// Add the root node as an attribute value
					aTmpData->AppendShort( strlen( "ReadOnly" ) );
					aTmpData->AppendString( "ReadOnly" );

				}
				// Add the attribute length and data
				aAttrData->AppendShort( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );

				// Clear the temp block
				aTmpData->Clear();
			}
				 
			if ((strcmp( pAttrName, kDSAttributesAll ) == 0) ||
				(strcmp( pAttrName, kDSNAttrAuthMethod ) == 0) )
			{
				aTmpData->Clear();
				
				uiAttrCnt++;
			
				// Append the attribute name
				aTmpData->AppendShort( strlen( kDSNAttrAuthMethod ) );
				aTmpData->AppendString( kDSNAttrAuthMethod );

				if ( inData->fInAttrInfoOnly == false )
				{
					int idx, mechCount;
					char dsTypeStr[256];
					
					// get the count for the mechs that get returned
					mechCount = this->GetSASLMechCount( pContext );
					
					// Attribute value count
					aTmpData->AppendShort( 7 + mechCount );
					
					aTmpData->AppendShort( strlen( kDSStdAuthClearText ) );
					aTmpData->AppendString( kDSStdAuthClearText );
					aTmpData->AppendShort( strlen( kDSStdAuthSetPasswd ) );
					aTmpData->AppendString( kDSStdAuthSetPasswd );
					aTmpData->AppendShort( strlen( kDSStdAuthChangePasswd ) );
					aTmpData->AppendString( kDSStdAuthChangePasswd );
					aTmpData->AppendShort( strlen( kDSStdAuthSetPasswdAsRoot ) );
					aTmpData->AppendString( kDSStdAuthSetPasswdAsRoot );
					aTmpData->AppendShort( strlen( kDSStdAuthNodeNativeClearTextOK ) );
					aTmpData->AppendString( kDSStdAuthNodeNativeClearTextOK );
					aTmpData->AppendShort( strlen( kDSStdAuthNodeNativeNoClearText ) );
					aTmpData->AppendString( kDSStdAuthNodeNativeNoClearText );
					
					// password server supports kDSStdAuth2WayRandomChangePasswd
					// with or without the plug-in
					aTmpData->AppendShort( strlen( kDSStdAuth2WayRandomChangePasswd ) );
					aTmpData->AppendString( kDSStdAuth2WayRandomChangePasswd );
					
					for ( idx = 0; idx < pContext->mechCount; idx++ )
					{
						GetAuthMethodFromSASLName( pContext->mech[idx].method, dsTypeStr );
						if ( dsTypeStr[0] != '\0' )
						{
							// Append first attribute value
							aTmpData->AppendShort( strlen( dsTypeStr ) );
							aTmpData->AppendString( dsTypeStr );
						}
                    }
				} // fInAttrInfoOnly is false
                
				// Add the attribute length
				aAttrData->AppendShort( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
                aTmpData->Clear();
			
			} // kDSAttributesAll or kDSNAttrAuthMethod

		} // while

		aRecData->AppendShort( uiAttrCnt );
		if (uiAttrCnt > 0)
		{
			aRecData->AppendBlock( aAttrData->GetData(), aAttrData->GetLength() );
		}

		outBuff.AddData( aRecData->GetData(), aRecData->GetLength() );
		inData->fOutAttrInfoCount = uiAttrCnt;

		pData = outBuff.GetDataBlock( 1, &uiOffset );
		if ( pData != NULL )
		{
			pAttrContext = MakeContextData();
			Throw_NULL( pAttrContext , eMemoryAllocError );
			
		//add to the offset for the attr list the length of the GetDirNodeInfo fixed record labels
//		record length = 4
//		aRecData->AppendShort( strlen( "dsAttrTypeStandard:DirectoryNodeInfo" ) ); = 2
//		aRecData->AppendString( "dsAttrTypeStandard:DirectoryNodeInfo" ); = 36
//		aRecData->AppendShort( strlen( "DirectoryNodeInfo" ) ); = 2
//		aRecData->AppendString( "DirectoryNodeInfo" ); = 17
//		total adjustment = 4 + 2 + 36 + 2 + 17 = 61

			pAttrContext->offset = uiOffset + 61;

			gPSContextTable->AddItem( inData->fOutAttrListRef, pAttrContext );
		}
	}

	catch( SInt32 err )
	{
		siResult = err;
	}

	if ( inAttrList != NULL )
	{
		delete( inAttrList );
		inAttrList = NULL;
	}
	if ( aRecData != NULL )
	{
		delete( aRecData );
		aRecData = NULL;
	}
	if ( aAttrData != NULL )
	{
		delete( aAttrData );
		aAttrData = NULL;
	}
	if ( aTmpData != NULL )
	{
		delete( aTmpData );
		aTmpData = NULL;
	}

	return( siResult );

} // GetDirNodeInfo


//------------------------------------------------------------------------------------
//	* CloseAttributeList
//------------------------------------------------------------------------------------

SInt32 CPSPlugIn::CloseAttributeList ( sCloseAttributeList *inData )
{
	SInt32				siResult		= eDSNoErr;
	sPSContextData	   *pContext		= NULL;

	pContext = (sPSContextData *)gPSContextTable->GetItemData( inData->fInAttributeListRef );
	if ( pContext != NULL )
	{
		//only "offset" should have been used in the Context
		gPSContextTable->RemoveItem( inData->fInAttributeListRef );
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

SInt32 CPSPlugIn::CloseAttributeValueList ( sCloseAttributeValueList *inData )
{
	SInt32				siResult		= eDSNoErr;
	sPSContextData	   *pContext		= NULL;

	pContext = (sPSContextData *)gPSContextTable->GetItemData( inData->fInAttributeValueListRef );
	if ( pContext != NULL )
	{
		//only "offset" should have been used in the Context
		gPSContextTable->RemoveItem( inData->fInAttributeValueListRef );
	}
	else
	{
		siResult = eDSInvalidAttrValueRef;
	}

	return( siResult );

} // CloseAttributeValueList


// ---------------------------------------------------------------------------
//	* GetStringFromAuthBuffer
//    retrieve a string from a standard auth buffer
//    buffer format should be 4 byte length followed by username, then optional
//    additional data after. Buffer length must be at least 5 (length + 1 character name)
// ---------------------------------------------------------------------------

SInt32 CPSPlugIn::GetStringFromAuthBuffer(tDataBufferPtr inAuthData, int stringNum, char **outString)
{
	tDataListPtr dataList = dsAuthBufferGetDataListAllocPriv(inAuthData);
	if (dataList != NULL)
	{
		*outString = dsDataListGetNodeStringPriv(dataList, stringNum);
		// this allocates a copy of the string
		
		dsDataListDeallocatePriv(dataList);
		free(dataList);
		dataList = NULL;
		return eDSNoErr;
	}
    
	return eDSInvalidBuffFormat;
}


// ---------------------------------------------------------------------------
//	* Get2StringsFromAuthBuffer
//    retrieve the first 2 strings from a standard auth buffer
//    buffer format should be 4 byte length followed by username, then optional
//    additional data after. Buffer length must be at least 5 (length + 1 character name)
// ---------------------------------------------------------------------------

SInt32 CPSPlugIn::Get2StringsFromAuthBuffer(tDataBufferPtr inAuthData, char **outString1, char **outString2)
{
	SInt32 result = GetStringFromAuthBuffer( inAuthData, 1, outString1 );
	if ( result == eDSNoErr )
		result = GetStringFromAuthBuffer( inAuthData, 2, outString2 );
	
	return result;
}


// ---------------------------------------------------------------------------
//	* GetDataFromAuthBuffer
//    retrieve data from a standard auth buffer
//    buffer format should be 4 byte length followed by username, then optional
//    additional data after. Buffer length must be at least 5 (length + 1 character name)
// ---------------------------------------------------------------------------

SInt32 CPSPlugIn::GetDataFromAuthBuffer(tDataBufferPtr inAuthData, int nodeNum, unsigned char **outData, UInt32 *outLen)
{
    tDataNodePtr pDataNode;
    tDirStatus status;
    
    *outData = NULL;
	*outLen = 0;
    
	tDataListPtr dataList = dsAuthBufferGetDataListAllocPriv(inAuthData);
	if (dataList != NULL)
	{
		status = dsDataListGetNodePriv(dataList, nodeNum, &pDataNode);
        if ( status != eDSNoErr )
            return status;
        
		if ( pDataNode->fBufferLength > 0 )
		{
			*outData = (unsigned char *) malloc(pDataNode->fBufferLength + RC5_32_BLOCK);
			if ( ! (*outData) )
				return eMemoryAllocError;
			
			memcpy(*outData, ((tDataBufferPriv*)pDataNode)->fBufferData, pDataNode->fBufferLength);
			*outLen = pDataNode->fBufferLength;
		}
        
        dsDataListDeallocatePriv(dataList);
		free(dataList);
		dataList = NULL;
		return eDSNoErr;
	}
    
	return eDSInvalidBuffFormat;
}


//------------------------------------------------------------------------------------
//	* UpdateCachedPasswordOnChange
//------------------------------------------------------------------------------------

void CPSPlugIn::UpdateCachedPasswordOnChange( sPSContextData *inContext, const char *inChangedUser, const char *inPassword, long inPasswordLen )
{
	if ( inContext == NULL || inChangedUser == NULL || inPassword == NULL )
		return;
	
	if ( strncmp( inContext->last.username, inChangedUser, 34 ) == 0 )
	{
		if ( inContext->last.password != NULL ) {
			free( inContext->last.password );
			inContext->last.password = NULL;
			inContext->last.passwordLen = 0;
		}
		inContext->last.password = (char *) malloc( inPasswordLen + 1 );
		if ( inContext->last.password != NULL ) {
			memcpy( inContext->last.password, inPassword, inPasswordLen );
			inContext->last.password[inPasswordLen] = '\0';
			inContext->last.passwordLen = inPasswordLen;
		}
	}
	
	if ( strncmp( inContext->nao.username, inChangedUser, 34 ) == 0 )
	{
		if ( inContext->nao.password != NULL ) {
			free( inContext->nao.password );
			inContext->nao.password = NULL;
			inContext->nao.passwordLen = 0;
		}
		inContext->nao.password = (char *) malloc( inPasswordLen + 1 );
		if ( inContext->nao.password != NULL ) {
			memcpy( inContext->nao.password, inPassword, inPasswordLen );
			inContext->nao.password[inPasswordLen] = '\0';
			inContext->nao.passwordLen = inPasswordLen;
		}
	}
}


//------------------------------------------------------------------------------------
//	* GetSASLMechCount
//------------------------------------------------------------------------------------

int CPSPlugIn::GetSASLMechCount( sPSContextData *inContext )
{
	int idx;
	int mechCount = 0;
	char dsTypeStr[256];
	
	for ( idx = 0; idx < inContext->mechCount; idx++ )
	{
		GetAuthMethodFromSASLName( inContext->mech[idx].method, dsTypeStr );
		if ( dsTypeStr[0] != '\0' )
			mechCount++;
	}
					
	return mechCount;
}


//------------------------------------------------------------------------------------
//	* SASLInit
//------------------------------------------------------------------------------------

void CPSPlugIn::SASLInit ( void )
{
	if ( !fCalledSASLInit )
	{
		gPWSConnMutex->Wait();
		if ( !fCalledSASLInit )
		{
			LDAP *ldp = NULL;
			if ( ldap_initialize( &ldp, NULL ) == LDAP_SUCCESS )
				ldap_unbind_ext( ldp, NULL, NULL );
			
			fCalledSASLInit = true;
		}
		gPWSConnMutex->Signal();
	}
}


//------------------------------------------------------------------------------------
//	* DoAuthentication
//------------------------------------------------------------------------------------

SInt32 CPSPlugIn::DoAuthentication ( sDoDirNodeAuth *inData )
{
	inData->fResult = this->DoAuthenticationSetup( inData );
    DEBUGLOG( "CPSPlugIn::DoAuthentication returning %l", inData->fResult );
	
	return inData->fResult;
}


//------------------------------------------------------------------------------------
//	* DoAuthenticationSetup
//------------------------------------------------------------------------------------

SInt32 CPSPlugIn::DoAuthenticationSetup ( sDoDirNodeAuth *inData )
{
	SInt32				siResult				= eDSAuthFailed;
	SInt32				siResult2				= eDSNoErr;
	char				*rsaKeyPtr				= NULL;
	Boolean				bNeedsRSAValidation		= true;
	Boolean				bHasValidAuth			= false;
	bool				bMethodCanSetPassword	= false;
	char				hexHash[34]				= {0,};
	CAuthParams			pb;
	
	SASLInit();
	
	pb.pContext = (sPSContextData *)gPSContextTable->GetItemData( inData->fInNodeRef );
	if ( pb.pContext == NULL )
		return eDSBadContextData;
	
	siResult = GetAuthMethodConstant( pb.pContext, inData->fInAuthMethod, &pb.uiAuthMethod, pb.saslMechNameStr );
	DEBUGLOG( "GetAuthMethodConstant siResult=%l, uiAuthMethod=%l, mech=%s", siResult, pb.uiAuthMethod,pb.saslMechNameStr);
	if ( siResult != eDSNoErr )
		return( siResult );
	
	// auto-downgrade kAuthNTSessionKey to kAuthSMB_NT_Key if there is no authenticator
	if ( pb.uiAuthMethod == kAuthNTSessionKey || pb.uiAuthMethod == kAuthNTLMv2WithSessionKey )
	{
		if ( GetStringFromAuthBuffer(inData->fInAuthStepData, 4, &pb.paramStr) != eDSNoErr || DSIsStringEmpty(pb.paramStr) )
			pb.uiAuthMethod = (pb.uiAuthMethod == kAuthNTSessionKey) ? kAuthSMB_NT_Key : kAuthNTLMv2;
		
		if ( pb.paramStr != NULL )
			DSFreeString( pb.paramStr );
	}
	
	if ( inData->fIOContinueData == 0 )
	{
		siResult = UnpackUsernameAndPassword(
									pb.pContext,
									pb.uiAuthMethod,
									inData->fInAuthStepData,
									&pb.userName,
									&pb.password,
									&pb.passwordLen,
									&pb.challenge );
		
		if ( siResult != eDSNoErr )
			return( siResult );
	}
	else
	{
		if ( gContinue->VerifyItem( (void *)inData->fIOContinueData ) == false )
			return( (SInt32)eDSInvalidContinueData );
	}
	
	// get a pointer to the rsa public key
	if ( pb.userName != NULL )
	{
		rsaKeyPtr = strchr( pb.userName, ',' );
		if ( rsaKeyPtr != NULL )
			rsaKeyPtr++;
		else
			syslog(LOG_INFO, "WARN: got user ID with no RSA key!" );
	}
	
	// make sure there is a server to contact
	if ( pb.pContext->madeFirstContact )
	{
		// make sure there is a connection
		if ( Connected(pb.pContext) )
		{
			bNeedsRSAValidation = false;
		}
		else
		{
			// close out anything stale
			EndServerSession( pb.pContext );
			
			gPWSConnMutex->Wait();
			siResult = ConnectToServer( pb.pContext );
			gPWSConnMutex->Signal();
			if ( siResult != 0 )
				return( siResult );
		}
	}
	else
	{
		if ( pb.userName != NULL && rsaKeyPtr != NULL )
		{
			pwsf_CalcServerUniqueID( rsaKeyPtr, hexHash );
			DEBUGLOG( "hexHash=%s", hexHash );
			
			siResult = HandleFirstContact( pb.pContext, NULL, hexHash, rsaKeyPtr );
		}
		else
		{
			siResult = HandleFirstContact( pb.pContext, NULL, NULL );
		}
		
		if ( siResult != eDSNoErr )
			return( siResult );
		
		if ( ! Connected(pb.pContext) )
		{
			// close out anything stale
			EndServerSession( pb.pContext );
			return( (SInt32)eDSAuthServerError );
		}
		
		pb.pContext->madeFirstContact = true;
	}
	
	// if bNeedsRSAValidation == true, then this is a new connection and the authentication
	// needs to be re-established.
	if ( ! bNeedsRSAValidation )
	{
		siResult = UseCurrentAuthenticationIfPossible( pb.pContext, pb.userName, pb.uiAuthMethod, &bHasValidAuth );
		if ( siResult != eDSNoErr )
			return( siResult );
	}
	
	// do not authenticate for auth methods that do not need SASL authentication
	if ( !bHasValidAuth && siResult == eDSNoErr && RequiresSASLAuthentication( pb.uiAuthMethod ) )
	{
		siResult = GetAuthMethodSASLName( pb.pContext, pb.uiAuthMethod, inData->fInDirNodeAuthOnlyFlag, pb.saslMechNameStr, &bMethodCanSetPassword );
		DEBUGLOG( "GetAuthMethodSASLName siResult=%l, mech=%s", siResult, pb.saslMechNameStr );
		if ( siResult != eDSNoErr )
			return( siResult );
		
		siResult = this->SetServiceInfo( inData, pb.pContext );
		if ( siResult != eDSNoErr ) {
			DEBUGLOG( "SetServiceInfo siResult=%l", siResult );
			return( siResult );
		}
		
		if ( rsaKeyPtr != NULL && bNeedsRSAValidation && !pb.pContext->isUNIXDomainSocket )
			siResult = DoRSAValidation( pb.pContext, rsaKeyPtr );
		
		if ( siResult == eDSNoErr )
		{
			pb.pContext->last.successfulAuth = false;
			
			if ( inData->fIOContinueData == 0 )
			{
				pb.pContinue = (sPSContinueData *)calloc( 1, sizeof( sPSContinueData ) );
				if ( pb.pContinue == NULL )
					return eMemoryError;
				
				gContinue->AddItem( pb.pContinue, inData->fInNodeRef );
				inData->fIOContinueData = pb.pContinue;
				
				pb.pContinue->fAuthPass = 0;
				pb.pContinue->fData = NULL;
				pb.pContinue->fDataLen = 0;
				pb.pContinue->fSASLSecret = NULL;
			}
			
			gPWSConnMutex->Wait();
			
			switch ( pb.uiAuthMethod )
			{
				case kAuthPPS:
					siResult = DoSASLPPSAuth( pb.pContext, pb.userName, pb.password, pb.passwordLen, inData );
					break;
				
				case kAuth2WayRandom:
					siResult = DoSASLTwoWayRandAuth( pb.pContext, pb.userName, pb.saslMechNameStr, inData );
					break;
				
				default:
					siResult = DoSASLAuth(
									pb.pContext,
									pb.userName,
									pb.password,
									pb.passwordLen,
									pb.challenge,
									pb.saslMechNameStr,
									inData,
									&pb.stepData );
			}
			
			gPWSConnMutex->Signal();
			
			if ( siResult == eDSNoErr && pb.uiAuthMethod != kAuth2WayRandom && pb.uiAuthMethod != kAuthPPS )
			{
				pb.pContext->last.successfulAuth = true;
				pb.pContext->last.methodCanSetPassword = bMethodCanSetPassword;
				
				// If authOnly == false, copy the username and password for later use
				// with SetPasswordAsRoot.
				if ( inData->fInDirNodeAuthOnlyFlag == false || pb.uiAuthMethod == kAuthNativeRetainCredential )
				{
					memcpy( pb.pContext->nao.username, pb.pContext->last.username, kMaxUserNameLength + 1 );
					
					if ( pb.pContext->nao.password != NULL ) {
						bzero( pb.pContext->nao.password, pb.pContext->nao.passwordLen );
						free( pb.pContext->nao.password );
						pb.pContext->nao.password = NULL;
						pb.pContext->nao.passwordLen = 0;
					}
					
					pb.pContext->nao.password = (char *) malloc( pb.pContext->last.passwordLen + 1 );
					if ( pb.pContext->nao.password == NULL )
						return eMemoryError;
					
					memcpy( pb.pContext->nao.password, pb.pContext->last.password, pb.pContext->last.passwordLen );
					pb.pContext->nao.password[pb.pContext->last.passwordLen] = '\0';
					
					pb.pContext->nao.passwordLen = pb.pContext->last.passwordLen;
					pb.pContext->nao.successfulAuth = true;
					pb.pContext->nao.methodCanSetPassword = pb.pContext->last.methodCanSetPassword;
				}
			}
		}
	}
	
	// For kAuthNTSessionKey and kAuthNTLMv2WithSessionKey, the SASL auth is for
	// the trust account. We only want to verify with the master if there's a
	// policy violation for the user account. The user's error is returned
	// by DoAuthenticationResponse().
	if ( siResult == eDSNoErr )
	{
		siResult2 = this->DoAuthenticationResponse( inData, pb );
		if ( siResult2 != eDSNoErr )
			DEBUGLOG( "CPSPlugIn::DoAuthenticationResponse returned %l", siResult2 );
	}
	else
	if ( pb.uiAuthMethod == kAuthNTSessionKey || pb.uiAuthMethod == kAuthNTLMv2WithSessionKey )
	{
		siResult = this->DoAuthenticationResponse( inData, pb );
		if ( siResult != eDSNoErr )
			DEBUGLOG( "CPSPlugIn::DoAuthenticationResponse returned %l", siResult );
	}
	
	if ( siResult == eDSNoErr )
	{
		if ( siResult2 != eDSNoErr )
			siResult = siResult2;
	}
	else
	{
		// for policy violations, confirm with the master if we're talking
		// to a replica
		switch ( siResult )
		{
			case eDSAuthNewPasswordRequired:
			case eDSAuthPasswordExpired:
			case eDSAuthPasswordQualityCheckFailed:
			case eDSAuthAccountDisabled:
			case eDSAuthAccountExpired:
			case eDSAuthAccountInactive:
			case eDSAuthPasswordTooShort:
			case eDSAuthPasswordTooLong:
			case eDSAuthPasswordNeedsLetter:
			case eDSAuthPasswordNeedsDigit:
			case eDSAuthInvalidLogonHours:
				if ( !pb.pContext->isUNIXDomainSocket &&
					 strcmp(pb.pContext->master.ip, pb.pContext->psName) != 0 )
				{
					// close the current session
					EndServerSession( pb.pContext, kSendQuit );
					
					// create a context for the master
					sPSContextData contextCopy;
					bzero( &contextCopy, sizeof(sPSContextData) );
					contextCopy.fd = -1;
					contextCopy.replicaFile = pb.pContext->replicaFile;
					memcpy( &contextCopy.serverProvidedFromNode, &pb.pContext->master, sizeof(sPSServerEntry) );
					contextCopy.providedNodeOnlyOrFail = true;
					
					// open a connection to the master
					if ( pb.userName != NULL && rsaKeyPtr != NULL )
					{
						siResult2 = HandleFirstContact( &contextCopy, NULL, hexHash, rsaKeyPtr );
					}
					else
					{
						siResult2 = HandleFirstContact( &contextCopy, NULL, NULL );
					}
					
					// doesn't belong to us
					contextCopy.replicaFile = NULL;
					
					// if we can't reach the master, return the original error
					if ( siResult2 == eDSNoErr )
					{
						if ( ! Connected(&contextCopy) )
						{
							// close out anything stale
							CleanContextData( &contextCopy );
							return( siResult );
						}
						
						// ask the OD master for a second opinion
						if ( rsaKeyPtr != NULL && !contextCopy.isUNIXDomainSocket )
						{
							siResult2 = DoRSAValidation( &contextCopy, rsaKeyPtr );
							if ( siResult2 == eDSNoErr )
							{
								pb.pContext->last.successfulAuth = false;
								
								if ( inData->fIOContinueData != 0 ) {
									gContinue->RemoveItem( inData->fIOContinueData );
									inData->fIOContinueData = 0;
								}
								
								pb.pContinue = (sPSContinueData *)calloc( 1, sizeof( sPSContinueData ) );
								if ( pb.pContinue == NULL ) {
									CleanContextData( &contextCopy );
									return eMemoryError;
								}
								
								gContinue->AddItem( pb.pContinue, inData->fInNodeRef );
								inData->fIOContinueData = pb.pContinue;
								
								pb.pContinue->fAuthPass = 0;
								pb.pContinue->fData = NULL;
								pb.pContinue->fDataLen = 0;
								pb.pContinue->fSASLSecret = NULL;
								
								gPWSConnMutex->Wait();
								
								switch ( pb.uiAuthMethod )
								{
									case kAuthPPS:
										siResult2 = DoSASLPPSAuth( &contextCopy, pb.userName, pb.password, pb.passwordLen, inData );
										break;
									
									case kAuth2WayRandom:
										siResult2 = DoSASLTwoWayRandAuth( &contextCopy, pb.userName, pb.saslMechNameStr, inData );
										break;
									
									default:
										siResult2 = DoSASLAuth(
														&contextCopy,
														pb.userName,
														pb.password,
														pb.passwordLen,
														pb.challenge,
														pb.saslMechNameStr,
														inData,
														&pb.stepData );
								}
								
								gPWSConnMutex->Signal();
								
								if ( siResult2 == eDSNoErr )
								{
									siResult = this->DoAuthenticationResponse( inData, pb );
									if ( siResult != eDSNoErr )
										DEBUGLOG( "CPSPlugIn::DoAuthenticationResponse returned %l", siResult );
								}
							}
						}
					}
					CleanContextData( &contextCopy );
				}
				break;
			
			default:
				break;
		}
		
		if ( siResult == eDSAuthNewPasswordRequired )
		{
			switch( pb.uiAuthMethod )
			{
				case kAuthSetPasswd:
				case kAuthSetPasswdAsRoot:
				case kAuthChangePasswd:
				case kAuthMSLMCHAP2ChangePasswd:
					siResult = this->DoAuthenticationResponse( inData, pb );
					if ( siResult != eDSNoErr )
						DEBUGLOG( "CPSPlugIn::DoAuthenticationResponse returned %l", siResult );
					break;
			}
		}
	}
	
	if ( fOpenNodeCount >= kMaxOpenNodesBeforeQuickClose )
	{
		if ( inData->fInDirNodeAuthOnlyFlag == true )
		{
			if ( pb.uiAuthMethod == kAuthClearText ||
				 pb.uiAuthMethod == kAuthNativeClearTextOK ||
				 pb.uiAuthMethod == kAuthNativeNoClearText ||
				 pb.uiAuthMethod == kAuthAPOP ||
				 pb.uiAuthMethod == kAuthSMB_NT_Key ||
				 pb.uiAuthMethod == kAuthSMB_LM_Key ||
				 pb.uiAuthMethod == kAuthDIGEST_MD5 ||
				 pb.uiAuthMethod == kAuthCRAM_MD5 ||
				 pb.uiAuthMethod == kAuthMSCHAP2 ||
				 pb.uiAuthMethod == kAuthNTLMv2 )
			{
				EndServerSession( pb.pContext, kSendQuit );
			}
		}
		
		gPSContextTable->DoOnAllItems( CPSPlugIn::ReleaseCloseWaitConnections );
	}
	
	return( siResult );
} // DoAuthentication


//------------------------------------------------------------------------------------
//	* DoAuthenticationResponse
//------------------------------------------------------------------------------------

SInt32
CPSPlugIn::DoAuthenticationResponse( sDoDirNodeAuth *inData, CAuthParams &pb )
{
	SInt32				siResult					= eDSNoErr;
	tDataBufferPtr		outBuf						= inData->fOutAuthStepDataResponse;
	int					saslResult					= SASL_OK;
	const char			*encodedStr					= NULL;
	unsigned int		encodedStrLen				= 0;
	char				encoded64Str[kOneKBuffer];
	char				buf[2 * kOneKBuffer];
	PWServerError		result;
	
	switch( pb.uiAuthMethod )
	{
		case kAuthDIGEST_MD5:
		case kAuthDIGEST_MD5_Reauth:
		case kAuthMSCHAP2:
			#pragma mark kAuthDigestMD5
			#pragma mark kAuthMSCHAP2
			
			if ( pb.stepData != NULL )
				siResult = PackStepBuffer( pb.stepData, false, NULL, NULL, NULL, outBuf );
			break;
			
		case kAuthNTLMv2SessionKey:
		case kAuthNTLMv2WithSessionKey:
			#pragma mark kAuthNTLMv2SessionKey
			siResult = DoAuthMethodNTLMv2SessionKey( siResult, pb.uiAuthMethod, inData, pb.pContext, outBuf );
			break;
			
		case kAuthSetPasswd:
			// buffer format is:
			// len1 username
			// len2 user's new password
			// len3 authenticatorID
			// len4 authenticatorPW
		case kAuthSetPasswdAsRoot:
			// buffer format is:
			// len1 username
			// len2 user's new password
			
			#pragma mark kAuthSetPasswd
			siResult = Get2StringsFromAuthBuffer( inData->fInAuthStepData, &pb.userIDToSet, &pb.paramStr );
			if ( siResult == eDSNoErr )
				StripRSAKey( pb.userIDToSet );
			
			if ( siResult == eDSNoErr )
			{
				if ( pb.paramStr == NULL )
					return( (SInt32)eDSInvalidBuffFormat );
				if (strlen(pb.paramStr) > kChangePassPaddedBufferSize )
					return( (SInt32)eDSAuthParameterError );
				
				// special-case for an empty password. DIGEST-MD5 does not
				// support empty passwords, but it's a DS requirement
				if ( DSIsStringEmpty(pb.paramStr) )
				{
					free( pb.paramStr );
					pb.paramStr = (char *) strdup( kEmptyPasswordAltStr );
				}
				
				strlcpy(buf, pb.paramStr, sizeof(buf));
				
				gSASLMutex->Wait();
				saslResult = sasl_encode(pb.pContext->conn,
										 buf,
										 kChangePassPaddedBufferSize,
										 &encodedStr,
										 &encodedStrLen); 
				gSASLMutex->Signal();
			}
			
			if ( siResult == eDSNoErr && saslResult == SASL_OK && pb.userIDToSet != NULL )
			{
				if ( ConvertBinaryTo64( encodedStr, encodedStrLen, encoded64Str ) == SASL_OK )
				{
					result = SendFlushReadWithMutex( pb.pContext, "CHANGEPASS", pb.userIDToSet, encoded64Str, buf, sizeof(buf) );
					siResult = PWSErrToDirServiceError( result );
					if ( siResult == eDSNoErr )
						UpdateCachedPasswordOnChange( pb.pContext, pb.userIDToSet, pb.paramStr, strlen(pb.paramStr) );
				}
			}
			else
			{
				printf("encode64 failed");
			}
			break;
		
		case kAuthSetComputerAcctPasswdAsRoot:
			#pragma mark kAuthSetComputerAcctPasswdAsRoot
			siResult = DoAuthMethodSetComputerAccountPassword( inData, pb.pContext, outBuf );
			break;
		
		case kAuthChangePasswd:
			#pragma mark kAuthChangePasswd
			/*!
			* @defined kDSStdAuthChangePasswd
			* @discussion Change the password for a user. Does not require prior authentication.
			*     The buffer is packed as follows:
			*
			*     4 byte length of username,
			*     username in UTF8 encoding,
			*     4 byte length of old password,
			*     old password in UTF8 encoding,
			*     4 byte length of new password,
			*     new password in UTF8 encoding
			*/
			
			siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &pb.paramStr );
			if ( siResult == eDSNoErr )
			{
				if ( pb.paramStr == NULL )
					return( (SInt32)eDSInvalidBuffFormat );
				if ( strlen(pb.paramStr) > kChangePassPaddedBufferSize )
					return( (SInt32)eDSAuthParameterError );
				
				// special-case for an empty password. DIGEST-MD5 does not
				// support empty passwords, but it's a DS requirement
				if ( DSIsStringEmpty(pb.paramStr) )
				{
					free( pb.paramStr );
					pb.paramStr = (char *) strdup( kEmptyPasswordAltStr );
				}
				
				strlcpy(buf, pb.paramStr, sizeof(buf));
				
				gSASLMutex->Wait();
				saslResult = sasl_encode(pb.pContext->conn,
										 buf,
										 kChangePassPaddedBufferSize,
										 &encodedStr,
										 &encodedStrLen); 
				gSASLMutex->Signal();
			}
			
			if ( siResult == eDSNoErr && saslResult == SASL_OK && pb.paramStr != NULL )
			{
				if ( ConvertBinaryTo64( encodedStr, encodedStrLen, encoded64Str ) == SASL_OK )
				{
					StripRSAKey( pb.paramStr );
					result = SendFlushReadWithMutex( pb.pContext, "CHANGEPASS", pb.userName, encoded64Str, buf, sizeof(buf) );
					siResult = PWSErrToDirServiceError( result );
					if ( siResult == eDSNoErr )
						UpdateCachedPasswordOnChange( pb.pContext, pb.userName, pb.paramStr, strlen(pb.paramStr) );
				}
			}
			else
			{
				printf("encode64 failed");
			}
			break;
		
		case kAuthNewUser:
			#pragma mark kAuthNewUser
			// buffer format is:
			// len1 AuthenticatorID
			// len2 AuthenticatorPW
			// len3 user's name
			// len4 user's initial password
			siResult = DoAuthMethodNewUser( inData, pb.pContext, false, outBuf );
			break;
			
		case kAuthNewUserWithPolicy:
			#pragma mark kAuthNewUserWithPolicy
			// buffer format is:
			// len1 AuthenticatorID
			// len2 AuthenticatorPW
			// len3 user's name
			// len4 user's initial password
			// len5 policy string
			siResult = DoAuthMethodNewUser( inData, pb.pContext, true, outBuf );
			break;
		
		case kAuthNewComputer:
			#pragma mark kAuthNewComputer
			siResult = DoAuthMethodNewComputer( inData, pb.pContext, outBuf );
			break;
			
		case kAuthGetPolicy:
			#pragma mark kAuthGetPolicy
			// buffer format is:
			// len1 AuthenticatorID
			// len2 AuthenticatorPW
			// len3 AccountID
			siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &pb.userIDToSet );
			if ( siResult == eDSNoErr )
			{
				if ( pb.userIDToSet == NULL )
					return( (SInt32)eDSInvalidBuffFormat );
				
				StripRSAKey( pb.userIDToSet );
				
				result = SendFlushReadWithMutex( pb.pContext, "GETPOLICY", pb.userIDToSet, NULL, buf, sizeof(buf) );
				if ( result.err != 0 )
					return( PWSErrToDirServiceError(result) );
				
				sasl_chop(buf);
				siResult = PackStepBuffer( buf, true, NULL, NULL, NULL, outBuf );
			}
			break;
		
		case kAuthGetEffectivePolicy:
			#pragma mark kAuthGetEffectivePolicy
			// buffer format is:
			// len1 AccountID
			StripRSAKey( pb.userName );
			result = SendFlushReadWithMutex( pb.pContext, "GETPOLICY", pb.userName, "ACTUAL", buf, sizeof(buf) );
			siResult = PWSErrToDirServiceError( result );
			if ( siResult == eDSNoErr )
			{
				sasl_chop( buf );
				siResult = PackStepBuffer( buf, true, NULL, NULL, NULL, outBuf );
			}
			break;
			
		case kAuthSetPolicy:
			#pragma mark kAuthSetPolicy
			// buffer format is:
			// len1 AuthenticatorID
			// len2 AuthenticatorPW
			// len3 AccountID
			// len4 PolicyString
			siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &pb.userIDToSet );
			if ( siResult == eDSNoErr ) 
			{
				StripRSAKey( pb.userIDToSet );
					
				siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 4, &pb.paramStr );
			}
			
			if ( siResult == eDSNoErr )
			{
				result = SendFlushReadWithMutex( pb.pContext, "SETPOLICY", pb.userIDToSet, pb.paramStr, buf, sizeof(buf) );
				siResult = PWSErrToDirServiceError( result );
			}
			break;
			
		case kAuthSetPolicyAsRoot:
			#pragma mark kAuthSetPolicyAsRoot
			// buffer format is:
			// len1 AccountID
			// len2 PolicyString
			siResult = Get2StringsFromAuthBuffer( inData->fInAuthStepData, &pb.userIDToSet, &pb.paramStr );
			if ( siResult == eDSNoErr )
			{
				StripRSAKey( pb.userIDToSet );
				result = SendFlushReadWithMutex( pb.pContext, "SETPOLICY", pb.userIDToSet, pb.paramStr, buf, sizeof(buf) );
				siResult = PWSErrToDirServiceError( result );
			}
			break;
		
		case kAuthGetGlobalPolicy:
			#pragma mark kAuthGetGlobalPolicy
			// buffer format is:
			// len1 AuthenticatorID
			// len2 AuthenticatorPW
			
			result = SendFlushReadWithMutex( pb.pContext, "GETGLOBALPOLICY", NULL, NULL, buf, sizeof(buf) );
			if ( result.err != 0 )
				return( PWSErrToDirServiceError(result) );
			
			sasl_chop( buf );
			siResult = PackStepBuffer( buf, true, NULL, NULL, NULL, outBuf );
			break;
			
		case kAuthSetGlobalPolicy:
			#pragma mark kAuthSetGlobalPolicy
			// buffer format is:
			// len1 AuthenticatorID
			// len2 AuthenticatorPW
			// len3 PolicyString
			siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &pb.paramStr );
			if ( siResult == eDSNoErr )
			{
				result = SendFlushReadWithMutex( pb.pContext, "SETGLOBALPOLICY", pb.paramStr, NULL, buf, sizeof(buf) );
				siResult = PWSErrToDirServiceError( result );
			}
			break;
			
		case kAuthGetUserName:
			#pragma mark kAuthGetUserName
			// buffer format is:
			// len1 AuthenticatorID
			// len2 AuthenticatorPW
			// len3 AccountID
			siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &pb.userIDToSet );
			if ( siResult == eDSNoErr )
			{
				StripRSAKey( pb.userIDToSet );
				
				result = SendFlushReadWithMutex( pb.pContext, "GETUSERNAME", pb.userIDToSet, NULL, buf, sizeof(buf) );
				if ( result.err != 0 )
					return( PWSErrToDirServiceError(result) );
				
				sasl_chop( buf );
				siResult = PackStepBuffer( buf, true, NULL, NULL, NULL, outBuf );
			}
			break;
			
		case kAuthSetUserName:
			#pragma mark kAuthSetUserName
			// buffer format is:
			// len1 AuthenticatorID
			// len2 AuthenticatorPW
			// len3 AccountID
			// len4 NewUserName
			siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &pb.userIDToSet );
			if ( siResult == eDSNoErr )
				siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 4, &pb.paramStr );
			if ( siResult == eDSNoErr )
			{
				StripRSAKey( pb.userIDToSet );
				
				result = SendFlushReadWithMutex( pb.pContext, "SETUSERNAME", pb.userIDToSet, pb.paramStr, buf, sizeof(buf) );
				siResult = PWSErrToDirServiceError( result );
			}
			break;
			
		case kAuthGetUserData:
			#pragma mark kAuthGetUserData
			// buffer format is:
			// len1 AuthenticatorID
			// len2 AuthenticatorPW
			// len3 AccountID
			siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &pb.userIDToSet );
			if ( siResult == eDSNoErr )
			{
				char *outData = NULL;
				unsigned long outDataLen;
				unsigned long decodedStrLen;
				
				StripRSAKey( pb.userIDToSet );
				
				result = SendFlushReadWithMutex( pb.pContext, "GETUSERDATA", pb.userIDToSet, NULL, buf, sizeof(buf) );
				siResult = PWSErrToDirServiceError( result );
				
				if ( siResult == eDSNoErr )
				{
					// base64 decode user data
					outDataLen = strlen( buf );
					outData = (char *)malloc( outDataLen );
					if ( outData == NULL )
						return eMemoryError;
					
					if ( Convert64ToBinary( buf, outData, outDataLen, &decodedStrLen ) == 0 )
					{
						if ( decodedStrLen <= outBuf->fBufferSize )
						{
							memcpy( outBuf->fBufferData, &decodedStrLen, 4 );
							memcpy( outBuf->fBufferData + 4, outData, decodedStrLen );
							outBuf->fBufferLength = decodedStrLen;
						}
						else
						{
							siResult = eDSBufferTooSmall;
						}
					}
					
					free( outData );
				}
			}
			break;
			
		case kAuthSetUserData:
			#pragma mark kAuthSetUserData
			// buffer format is:
			// len1 AuthenticatorID
			// len2 AuthenticatorPW
			// len3 AccountID
			// len4 NewUserData
			{
				char *tptr;
				long dataSegmentLen;
				
				siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &pb.userIDToSet );
				if ( siResult == eDSNoErr )
				{
					StripRSAKey(pb.userIDToSet);
					
					tptr = inData->fInAuthStepData->fBufferData;
					
					for (int repeatCount = 3; repeatCount > 0; repeatCount--)
					{
						memcpy(&dataSegmentLen, tptr, 4);
						tptr += 4 + dataSegmentLen;
					}
					
					memcpy(&dataSegmentLen, tptr, 4);
					
					pb.paramStr = (char *)malloc( dataSegmentLen * 4/3 + 20 );
					if ( pb.paramStr == NULL )
						return eMemoryError;
					
					// base64 encode user data
					siResult = ConvertBinaryTo64( tptr, dataSegmentLen, pb.paramStr );
				}
				
				if ( siResult == eDSNoErr )
				{
					result = SendFlushReadWithMutex( pb.pContext, "SETUSERDATA", pb.userIDToSet, pb.paramStr, buf, sizeof(buf) );
					siResult = PWSErrToDirServiceError( result );
				}
			}
			break;
			
		case kAuthDeleteUser:
			#pragma mark kAuthDeleteUser
			// buffer format is:
			// len1 AuthenticatorID
			// len2 AuthenticatorPW
			// len3 AccountID
			siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &pb.userIDToSet );
			if ( siResult == eDSNoErr )
			{
				StripRSAKey( pb.userIDToSet );
				
				result = SendFlushReadWithMutex( pb.pContext, "DELETEUSER", pb.userIDToSet, NULL, buf, sizeof(buf) );
				siResult = PWSErrToDirServiceError( result );
			}
			break;
		
		case kAuthGetIDByName:
			#pragma mark kAuthGetIDByName
			// buffer format is:
			// len1 AuthenticatorID
			// len2 AuthenticatorPW
			// len3 Name to look up
			// len4 ALL (optional)
			
			GetStringFromAuthBuffer( inData->fInAuthStepData, 4, &pb.userIDToSet );
			
			siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &pb.paramStr );
			if ( siResult == eDSNoErr )
			{
				result = SendFlushReadWithMutex( pb.pContext, "GETIDBYNAME", pb.paramStr, pb.userIDToSet, buf, sizeof(buf) );
				if ( result.err != 0 )
					return( PWSErrToDirServiceError(result) );
				
				sasl_chop( buf );
				
				// add the public rsa key
				if ( pb.pContext->rsaPublicKeyStr )
				{
					strcat(buf, ",");
					strlcat(buf, pb.pContext->rsaPublicKeyStr, sizeof(buf));
				}
				
				siResult = PackStepBuffer( buf, true, NULL, NULL, NULL, outBuf );
			}
			break;
		
		case kAuthGetDisabledUsers:
			#pragma mark kAuthGetDisabledUsers
			siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &pb.paramStr );
			if ( siResult != eDSNoErr )
				return( siResult );
			if ( DSIsStringEmpty(pb.paramStr) )
				return( (SInt32)eDSInvalidBuffFormat );
			
			// Note: the password server's line limit is 1023 bytes so we may need to send
			// multiple requests.
			{
				long loopIndex;
				long byteCount = strlen( pb.paramStr );
				long loopCount = (byteCount / 1023) + 1;
				char *sptr = pb.paramStr;
				char *tptr;
				
				outBuf->fBufferLength = 0;
				
				for ( loopIndex = 0; loopIndex < loopCount; loopIndex++ )
				{
					strlcpy( buf, sptr, sizeof(buf) );
					if ( byteCount > 1023 )
					{
						tptr = buf + 1022;
						while ( (*tptr != ' ') && (tptr > buf) )
							*tptr-- = '\0';
						
						sptr += strlen( buf );
						byteCount = strlen(pb.paramStr) - (sptr - pb.paramStr);
					}
					
					DEBUGLOG( "ulist: %s", buf );
					result = SendFlushReadWithMutex( pb.pContext, "GETDISABLEDUSERS", buf, NULL, buf, sizeof(buf) );
					if ( result.err != 0 )
						return( PWSErrToDirServiceError(result) );
					
					// use encodedStrLen; it's available
					encodedStrLen = strlen( buf );
					if ( outBuf->fBufferLength + encodedStrLen > outBuf->fBufferSize - 4 )
						return( (SInt32)eDSBufferTooSmall );
					
					// put a 4-byte length in the buffer
					memcpy( outBuf->fBufferData + outBuf->fBufferLength, &encodedStrLen, 4 );
					outBuf->fBufferLength += 4;
					
					// add the string
					strcpy( outBuf->fBufferData + outBuf->fBufferLength, buf );
					outBuf->fBufferLength += encodedStrLen;
				}
			}
			break;
		
		case kAuth2WayRandomChangePass:
			#pragma mark kAuth2WayRandomChangePass
			StripRSAKey( pb.paramStr );
			siResult = ConvertBinaryTo64( pb.password, 8, encoded64Str );
			if ( siResult == eDSNoErr )
			{
				char desDataBuf[50];
				
				snprintf( desDataBuf, sizeof(desDataBuf), "%s ", encoded64Str );
				
				siResult = ConvertBinaryTo64( pb.password + 8, 8, encoded64Str );
				if ( siResult == eDSNoErr )
				{
					strcat( desDataBuf, encoded64Str );
					strcat( desDataBuf, "\r\n" );
					
					result = SendFlushReadWithMutex( pb.pContext, "TWRNDCHANGEPASS", pb.paramStr, desDataBuf, buf, sizeof(buf) );
				}
				
				if ( siResult == eDSNoErr && result.err != 0 )
					siResult = PWSErrToDirServiceError( result );
			}
			break;
			
		case kAuthSyncSetupReplica:
			#pragma mark kAuthSyncSetupReplica
			
			siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &pb.paramStr );
			if ( siResult == eDSNoErr )
			{
				result = SendFlushReadWithMutex( pb.pContext, "SYNC SETUPREPLICA", pb.paramStr, NULL, buf, sizeof(buf) );
				if ( result.err != 0 )
					return( PWSErrToDirServiceError(result) );
				
				sasl_chop( buf );
				
				if ( strcasecmp( pb.paramStr, "GET" ) == 0 )
				{
					const char *decodedStr;
					unsigned long decodedStrLen;
					unsigned decryptedStrLen;
					char *replicaNamePtr = NULL;
					
					replicaNamePtr = strchr( buf + 4, ' ' );
					if ( replicaNamePtr != NULL )
						*replicaNamePtr++ = '\0';
					
					decodedStrLen = strlen( buf + 4 );
					pb.stepData = (char *) malloc( decodedStrLen + 9 );
					if ( pb.stepData == NULL )
						return( (SInt32)eMemoryError );
					
					// decode
					if ( Convert64ToBinary( buf + 4, pb.stepData, decodedStrLen, &decodedStrLen ) != SASL_OK )
						return( (SInt32)eDSAuthServerError );
					
					if (decodedStrLen % 8)
						decodedStrLen += 8 - (decodedStrLen % 8);
					
					// decrypt
					gSASLMutex->Wait();
					saslResult = sasl_decode(pb.pContext->conn,
												pb.stepData,
												decodedStrLen,
												&decodedStr,
												&decryptedStrLen);
					gSASLMutex->Signal();
					
					// use encodedStrLen; it's available
					encodedStrLen = 4 + decryptedStrLen + 4 + strlen(pb.pContext->rsaPublicKeyStr);
					if ( replicaNamePtr != NULL )
						encodedStrLen += 4 + strlen( replicaNamePtr );
					if ( encodedStrLen > outBuf->fBufferSize )
						return( (SInt32)eDSBufferTooSmall );
					
					// put a 4-byte length in the buffer
					decodedStrLen = decryptedStrLen;
					memcpy( outBuf->fBufferData, &decodedStrLen, 4 );
					outBuf->fBufferLength = 4;
					
					// copy the private key
					memcpy( outBuf->fBufferData + outBuf->fBufferLength, decodedStr, decryptedStrLen );
					outBuf->fBufferLength += decryptedStrLen;
					
					// put a 4-byte length in the buffer
					encodedStrLen = strlen( pb.pContext->rsaPublicKeyStr );
					memcpy( outBuf->fBufferData + outBuf->fBufferLength, &encodedStrLen, 4 );
					outBuf->fBufferLength += 4;
					
					// copy the public key
					strcpy( outBuf->fBufferData + outBuf->fBufferLength, pb.pContext->rsaPublicKeyStr );
					outBuf->fBufferLength += encodedStrLen;
					
					if ( replicaNamePtr != NULL )
					{
						// put a 4-byte length in the buffer
						encodedStrLen = strlen( replicaNamePtr );
						memcpy( outBuf->fBufferData + outBuf->fBufferLength, &encodedStrLen, 4 );
						outBuf->fBufferLength += 4;
						
						// copy the replica name
						strcpy( outBuf->fBufferData + outBuf->fBufferLength, replicaNamePtr );
						outBuf->fBufferLength += encodedStrLen;
					}
				}
			}
			break;
			
		case kAuthListReplicas:
			#pragma mark kAuthListReplicas
			gPWSConnMutex->Wait();
			siResult = DoAuthMethodListReplicas( inData, pb.pContext, outBuf );
			gPWSConnMutex->Signal();
			break;
			
		case kAuthPull:
			#pragma mark kAuthPull
			siResult = DoAuthMethodPull( inData, pb.pContext, outBuf );
			break;
			
		case kAuthPush:
			#pragma mark kAuthPush
			siResult = DoAuthMethodPush( inData, pb.pContext, outBuf );
			break;
			
		case kAuthProcessNoReply:
			#pragma mark kAuthProcessNoReply
			// get size from somewhere
			snprintf( buf, sizeof(buf), "%lu", pb.pContext->pushByteCount );
			pb.pContext->pushByteCount = 0;
			result = SendFlushReadWithMutex( pb.pContext, "SYNC PROCESS-NO-REPLY", buf, NULL, buf, sizeof(buf) );
			siResult = PWSErrToDirServiceError( result );
			break;
			
		case kAuthSMB_NTUserSessionKey:
		case kAuthNTSessionKey:
			#pragma mark kAuthSMB_NTUserSessionKey
			siResult = DoAuthMethodNTUserSessionKey( siResult, pb.uiAuthMethod, inData, pb.pContext, outBuf );
			break;
			
		case kAuthSMBWorkstationCredentialSessionKey:
			#pragma mark kAuthSMBWorkstationCredentialSessionKey
			{
				UInt32 paramLen;
				const char *decryptedStr;
				unsigned long decodedStrLen;
				unsigned decryptedStrLen;
				char base64Param[30];
				
				siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 1, &pb.userIDToSet );
				if ( siResult == eDSNoErr )
					siResult = GetDataFromAuthBuffer( inData->fInAuthStepData, 2, (unsigned char **)&pb.paramStr, &paramLen );
				if ( siResult == eDSNoErr )
				{
					if ( pb.userIDToSet == NULL || paramLen > 16 )
						return( (SInt32)eDSInvalidBuffFormat );
					
					StripRSAKey( pb.userIDToSet );
					
					if ( ConvertBinaryTo64( pb.paramStr, paramLen, base64Param ) != SASL_OK )
						return( (SInt32)eParameterError );
					
					result = SendFlushReadWithMutex( pb.pContext, "GETWCSK", pb.userIDToSet, base64Param, buf, sizeof(buf) );
					if ( result.err != 0 )
						return( PWSErrToDirServiceError(result) );
					
					decodedStrLen = strlen( buf ) - 4;
					if ( decodedStrLen < 2 )
						return( (SInt32)eDSAuthServerError );
					
					pb.stepData = (char *) malloc( decodedStrLen );
					if ( pb.stepData == NULL )
						return( (SInt32)eMemoryError );
					
					// decode
					if ( Convert64ToBinary( buf + 4, pb.stepData, decodedStrLen, &decodedStrLen ) != SASL_OK )
						return( (SInt32)eDSAuthServerError );
					
					// decrypt
					gSASLMutex->Wait();
					saslResult = sasl_decode( pb.pContext->conn,
												pb.stepData,
												decodedStrLen,
												&decryptedStr,
												&decryptedStrLen);
					gSASLMutex->Signal();
					
					if ( outBuf->fBufferSize < decryptedStrLen + 4 )
						return( (SInt32)eDSBufferTooSmall );
					
					outBuf->fBufferLength = decryptedStrLen + 4;
					decodedStrLen = decryptedStrLen;
					memcpy( outBuf->fBufferData, &decodedStrLen, 4 );
					memcpy( outBuf->fBufferData + 4, decryptedStr, decryptedStrLen );
				}
			}
			break;
			
		case kAuthNTSetWorkstationPasswd:
			#pragma mark kAuthNTSetWorkstationPasswd
			siResult = DoAuthMethodSetHash( inData, pb.pContext, "CHANGENTHASH" );
			break;
		
		case kAuthSetLMHash:
			#pragma mark kAuthSetLMHash
			siResult = DoAuthMethodSetHash( inData, pb.pContext, "CHANGELMHASH" );
			break;
		
		case kAuthGetKerberosPrincipal:
			#pragma mark kAuthGetKerberosPrincipal
			
			siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 1, &pb.userIDToSet );
			if ( siResult != eDSNoErr )
				return( siResult );
			
			result = SendFlushReadWithMutex( pb.pContext, "GETKERBPRINC", pb.userIDToSet, NULL, buf, sizeof(buf) );
			siResult = PWSErrToDirServiceError( result );
			if ( siResult != eDSNoErr )
				return( siResult );
			
			sasl_chop( buf );
			siResult = PackStepBuffer( buf, true, NULL, NULL, NULL, outBuf );
			break;
		
		case kAuthVPN_PPTPMasterKeys:
			#pragma mark kAuthVPN_PPTPMasterKeys
			
			siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 1, &pb.userIDToSet );
			if ( siResult == eDSNoErr )
				siResult = GetDataFromAuthBuffer( inData->fInAuthStepData, 2, (unsigned char **)&pb.paramStr, &pb.passwordLen );
			if ( siResult == eDSNoErr )
			{
				ConvertBinaryToHex( (unsigned char *)pb.paramStr, pb.passwordLen, buf );
				free( pb.paramStr );
				pb.paramStr = NULL;
				siResult = GetDataFromAuthBuffer( inData->fInAuthStepData, 3, (unsigned char **)&pb.paramStr, &pb.passwordLen );
				if ( siResult == eDSNoErr && pb.passwordLen == 1 && (pb.paramStr[0] == 8 || pb.paramStr[0] == 16) )
				{
					strcat( buf, " " );
					strcat( buf, (pb.paramStr[0] == 8) ? "8" : "16" );
				}
				else
				{
					if ( siResult == eDSNoErr )
						siResult = eDSInvalidBuffFormat;
				}
			}
			
			if ( siResult == eDSNoErr )
			{
				const char *decryptedStr;
				unsigned long buffItemLen;
				unsigned decryptedStrLen;
				
				if ( pb.userIDToSet == NULL )
					return( (SInt32)eDSInvalidBuffFormat );
				
				StripRSAKey( pb.userIDToSet );
				
				result = SendFlushReadWithMutex( pb.pContext, "GETPPTPKEYS", pb.userIDToSet, buf, buf, sizeof(buf) );
				if ( result.err != 0 )
					return( PWSErrToDirServiceError(result) );
				
				buffItemLen = strlen( buf ) - 4;
				if ( buffItemLen < 2 )
					return( (SInt32)eDSAuthServerError );
				
				pb.stepData = (char *) malloc( buffItemLen );
				if ( pb.stepData == NULL )
					return( (SInt32)eMemoryError );
				
				// decode
				if ( Convert64ToBinary( buf + 4, pb.stepData, buffItemLen, &buffItemLen ) != SASL_OK )
					return( (SInt32)eDSAuthServerError );
				
				// decrypt
				gSASLMutex->Wait();
				saslResult = sasl_decode( pb.pContext->conn,
											pb.stepData,
											buffItemLen,
											&decryptedStr,
											&decryptedStrLen);
				gSASLMutex->Signal();
				
				if ( outBuf->fBufferSize < decryptedStrLen + 4 )
					return( (SInt32)eDSBufferTooSmall );
				
				buffItemLen = decryptedStr[0];
				outBuf->fBufferLength = buffItemLen * 2 + 8;
				memcpy( outBuf->fBufferData, &buffItemLen, 4 );
				memcpy( outBuf->fBufferData + 4, decryptedStr + 1, buffItemLen );
				memcpy( outBuf->fBufferData + 4 + buffItemLen, &buffItemLen, 4 );
				memcpy( outBuf->fBufferData + 4 + buffItemLen + 4, decryptedStr + 1 + buffItemLen, buffItemLen );
			}
			break;
			
		case kAuthMSLMCHAP2ChangePasswd:
			#pragma mark kAuthMSLMCHAP2ChangePasswd
			siResult = DoAuthMethodMSChapChangePass( inData, pb.pContext );
			break;
			
		case kAuthEncryptToUser:
			#pragma mark kAuthEncryptToUser
			siResult = DoAuthMethodEncryptToUser( inData, pb.pContext, outBuf );
			break;
		
		case kAuthDecrypt:
			#pragma mark kAuthDecrypt
			siResult = DoAuthMethodDecrypt( inData, pb.pContext, outBuf );
			break;
		
		case kAuthGetStats:
			#pragma mark kAuthGetStats
			siResult = DoAuthMethodGetStats( inData, pb.pContext, outBuf );
			break;
		
		case kAuthGetChangeList:
			#pragma mark kAuthGetChangeList
			siResult = DoAuthMethodGetChangeList( inData, pb.pContext, outBuf );
			break;
	}
		
	return( siResult );
} // DoAuthenticationResponse


#pragma mark -

// ---------------------------------------------------------------------------
//	* DoAuthMethodNewUser
// ---------------------------------------------------------------------------

SInt32 CPSPlugIn::DoAuthMethodNewUser( sDoDirNodeAuth *inData, sPSContextData *inContext, bool inWithPolicy, tDataBufferPtr outBuf )
{
	SInt32					siResult			= eDSNoErr;
	char					*princToSet			= NULL;
	char					*paramStr			= NULL;
	char					*policyStr			= NULL;
	int						saslResult			= SASL_OK;
	const char				*encodedStr			= NULL;
	unsigned int			encodedStrLen		= 0;
	long					commandStrLen		= 0;
	long					policyStrLen		= 0;
	unsigned long			princToSetStrLen	= 0;
	bool					needPolicyLater		= false;
	bool					hasDotInName		= false;
	NewUserParamListType	paramListType		= kNewUserParamsNone;
	PWServerError			result				= {0};
	char					buf[kOneKBuffer];
	char					encoded64Str[kOneKBuffer];
	char					userNameToSet[kOneKBuffer];
	
	try
	{
		siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &princToSet );
		if ( siResult == eDSNoErr )
			siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 4, &paramStr );
			if ( siResult == eDSNoErr && inWithPolicy )
				siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 5, &policyStr );

		if ( siResult == eDSNoErr )
		{
			if ( princToSet == NULL || paramStr == NULL )
				throw( (SInt32)eDSInvalidBuffFormat );
			princToSetStrLen = strlen( princToSet );
			if ( princToSetStrLen == 0 )
				DEBUGLOG( "CPSPlugIn::DoAuthMethodNewUser(): empty username in buffer" );
			if ( princToSetStrLen == 0 ||
				 princToSetStrLen >= sizeof(userNameToSet) ||
				 strlen(paramStr) >= kChangePassPaddedBufferSize )
			{
				throw( (SInt32)eDSAuthParameterError );
			}
			
			// special-case for an empty password. DIGEST-MD5 does not
			// support empty passwords, but it's a DS requirement
			if ( DSIsStringEmpty(paramStr) )
			{
				free( paramStr );
				paramStr = (char *) strdup( kEmptyPasswordAltStr );
			}
			
			strlcpy(buf, paramStr, sizeof(buf));
			
			gSASLMutex->Wait();
			saslResult = sasl_encode(inContext->conn,
									 buf,
									 kChangePassPaddedBufferSize,
									 &encodedStr,
									 &encodedStrLen); 
			gSASLMutex->Signal();
		}
		
		if ( siResult == eDSNoErr && saslResult == SASL_OK )
		{
			if ( ConvertBinaryTo64( encodedStr, encodedStrLen, encoded64Str ) == SASL_OK )
			{
				if ( inContext->rsaPublicKeyStr == NULL )
					throw( (SInt32)eDSAuthServerError );
				
				// figure out the param list type
				{
					long shortnameLen;
					char *tptr = strchr( princToSet, '.' );
					
					hasDotInName = ( tptr != NULL );
					if ( hasDotInName )
					{
						shortnameLen = (long)(tptr - princToSet);
						if ( shortnameLen > 0 )
						{
							strlcpy( userNameToSet, princToSet, shortnameLen + 1 );
						}
						else
						{
							strcpy( userNameToSet, princToSet );
						}
					}
					else
					{
						strcpy( userNameToSet, princToSet );
					}
				}
				
				if ( inWithPolicy )
				{
					paramListType = hasDotInName ? kNewUserParamsPrincipalNameAndPolicy : kNewUserParamsPolicy;
				}
				else
				{
					paramListType = hasDotInName ? kNewUserParamsPrincipalName : kNewUserParamsNone;
				}
				
				// construct the param list
				commandStrLen = snprintf( buf, sizeof(buf), "NEWUSER %s %s", userNameToSet, encoded64Str );
			
				switch( paramListType )
				{
					case kNewUserParamsNone:
						result = SendFlushReadWithMutex( inContext, buf, NULL, NULL, buf, sizeof(buf) );
						break;
						
					case kNewUserParamsPolicy:
						if ( policyStr != NULL )
							policyStrLen = strlen( policyStr );
						needPolicyLater = (commandStrLen + sizeof(" WITHPOLICY ") + policyStrLen + 2 > 1535);
						if ( policyStrLen > 0 && !needPolicyLater )
						{
							result = SendFlushReadWithMutex( inContext, buf, "WITHPOLICY", policyStr, buf, sizeof(buf) );
						}
						else
						{
							result = SendFlushReadWithMutex( inContext, buf, NULL, NULL, buf, sizeof(buf) );
						}
						break;
	
					case kNewUserParamsPrincipalName:
						result = SendFlushReadWithMutex( inContext, buf, "WITHPRINC", princToSet, buf, sizeof(buf) );
						break;
	
					case kNewUserParamsPrincipalNameAndPolicy:
						if ( policyStr != NULL )
							policyStrLen = strlen( policyStr );
						needPolicyLater = (commandStrLen + sizeof(" WITHPRINC_AND_POLICY ") + princToSetStrLen + 1 + policyStrLen + 2 > 1535);
						if ( policyStrLen > 0 && !needPolicyLater )
						{
							strcat( buf, " WITHPRINC_AND_POLICY " );
							strlcat( buf, princToSet, sizeof(buf) );
							
							result = SendFlushReadWithMutex( inContext, buf, policyStr, NULL, buf, sizeof(buf) );
						}
						else
						{
							result = SendFlushReadWithMutex( inContext, buf, "WITHPRINC", princToSet, buf, sizeof(buf) );
						}
						break;
				}
				
				if ( result.err != 0 )
					throw( PWSErrToDirServiceError(result) );
				
				sasl_chop( buf );
				
				// use encodedStrLen; it's available
				encodedStrLen = strlen(buf) + 1 + strlen(inContext->rsaPublicKeyStr);
				if ( encodedStrLen > outBuf->fBufferSize )
					throw( (SInt32)eDSBufferTooSmall );
				
				// put a 4-byte length in the buffer
				encodedStrLen -= 4;
				memcpy( outBuf->fBufferData, &encodedStrLen, 4 );
				outBuf->fBufferLength = 4;
				
				// copy the ID
				encodedStrLen = strlen( buf + 4 );
				memcpy( outBuf->fBufferData + outBuf->fBufferLength, buf+4, encodedStrLen );
				outBuf->fBufferLength += encodedStrLen;
				
				// add a separator
				outBuf->fBufferData[outBuf->fBufferLength] = ',';
				outBuf->fBufferLength++;
			
				// copy the public key
				strcpy( outBuf->fBufferData + outBuf->fBufferLength, inContext->rsaPublicKeyStr );
				outBuf->fBufferLength += strlen(inContext->rsaPublicKeyStr);
			
				if ( needPolicyLater && policyStrLen > 0 )
				{
					result = SendFlushReadWithMutex( inContext, "SETPOLICY", outBuf->fBufferData + 4, policyStr, buf, sizeof(buf) );
				}
			}
			else
			{
				DEBUGLOG( "CPSPlugIn::DoAuthMethodNewUser(): encode64 failed, data length = %u", encodedStrLen );
			}
		}
	}
	catch( SInt32 error )
	{
		siResult = error;
	}
	catch( ... )
	{
	}
	
	DSFreeString( princToSet );
	DSFreeString( paramStr );
	DSFreeString( policyStr );
	
	return siResult;
}


// ---------------------------------------------------------------------------
//	* DoAuthMethodNewComputer
// ---------------------------------------------------------------------------

SInt32 CPSPlugIn::DoAuthMethodNewComputer( sDoDirNodeAuth *inData, sPSContextData *inContext, tDataBufferPtr outBuf )
{
	SInt32					siResult			= eDSNoErr;
	char					*computerName		= NULL;
	char					*computerPass		= NULL;
	char					*ownerList			= NULL;
	int						saslResult			= SASL_OK;
	const char				*encodedStr			= NULL;
	unsigned int			encodedStrLen		= 0;
	long					commandStrLen		= 0;
	PWServerError			result				= {0};
	char					buf[kOneKBuffer];
	char					encoded64Str[kOneKBuffer];
	
	try
	{
		siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &computerName );
		if ( siResult == eDSNoErr )
			siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 4, &computerPass );
		if ( siResult == eDSNoErr )
			siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 5, &ownerList );
		if ( siResult != eDSNoErr )
			throw( (SInt32)eDSInvalidBuffFormat );
		if ( strlen(computerPass) > kChangePassPaddedBufferSize )
			throw( (SInt32)eDSAuthPasswordTooLong );
	
		// special-case for an empty password. DIGEST-MD5 does not
		// support empty passwords, but it's a DS requirement
		if ( DSIsStringEmpty(computerPass) )
		{
			free( computerPass );
			computerPass = strdup( kEmptyPasswordAltStr );
		}
		
		strlcpy(buf, computerPass, sizeof(buf));
		
		gSASLMutex->Wait();
		saslResult = sasl_encode(inContext->conn,
								 buf,
								 kChangePassPaddedBufferSize,
								 &encodedStr,
								 &encodedStrLen); 
		gSASLMutex->Signal();
		
		if ( saslResult != SASL_OK ) {
			DEBUGLOG( "CPSPlugIn::DoAuthMethodNewComputer(): sasl_encode = %d", saslResult );
			throw( (SInt32)eDSAuthFailed );
		}
		
		if ( ConvertBinaryTo64( encodedStr, encodedStrLen, encoded64Str ) == SASL_OK )
		{
			if ( inContext->rsaPublicKeyStr == NULL )
				throw( (SInt32)eDSAuthServerError );
			
			// construct the param list
			commandStrLen = snprintf( buf, sizeof(buf), "NEWUSER %s %s", computerName, encoded64Str );
			result = SendFlushReadWithMutex( inContext, buf, "COMPUTERACCT", ownerList, buf, sizeof(buf) );			
			if ( result.err != 0 )
				throw( PWSErrToDirServiceError(result) );
			
			sasl_chop( buf );
			
			// use encodedStrLen; it's available
			encodedStrLen = strlen(buf) + 1 + strlen(inContext->rsaPublicKeyStr);
			if ( encodedStrLen > outBuf->fBufferSize )
				throw( (SInt32)eDSBufferTooSmall );
			
			// put a 4-byte length in the buffer
			encodedStrLen -= 4;
			memcpy( outBuf->fBufferData, &encodedStrLen, 4 );
			outBuf->fBufferLength = 4;
			
			// copy the ID
			encodedStrLen = strlen( buf + 4 );
			memcpy( outBuf->fBufferData + outBuf->fBufferLength, buf+4, encodedStrLen );
			outBuf->fBufferLength += encodedStrLen;
			
			// add a separator
			outBuf->fBufferData[outBuf->fBufferLength] = ',';
			outBuf->fBufferLength++;
			
			// copy the public key
			strcpy( outBuf->fBufferData + outBuf->fBufferLength, inContext->rsaPublicKeyStr );
			outBuf->fBufferLength += strlen(inContext->rsaPublicKeyStr);
		}
	}
	catch( SInt32 error )
	{
		siResult = error;
	}
	catch( ... )
	{
	}
	
	DSFreeString( computerName );
	DSFreePassword( computerPass );
	DSFreeString( ownerList );
	
	return siResult;
}


// ---------------------------------------------------------------------------
//	* DoAuthMethodSetComputerAccountPassword
// ---------------------------------------------------------------------------

SInt32
CPSPlugIn::DoAuthMethodSetComputerAccountPassword(
	sDoDirNodeAuth *inData,
	sPSContextData *inContext,
	tDataBufferPtr outBuf )
{
	SInt32					siResult			= eDSNoErr;
	char					*computerName		= NULL;
	char					*computerPass		= NULL;
	char					*serviceList		= NULL;
	char					*hostList			= NULL;
	char					*realm				= NULL;
	int						saslResult			= SASL_OK;
	const char				*encodedStr			= NULL;
	unsigned int			encodedStrLen		= 0;
	long					commandStrLen		= 0;
	PWServerError			result				= {0};
	char					buf[4 * kOneKBuffer];
	char					encoded64Str[kOneKBuffer];
	
	try
	{
		siResult = Get2StringsFromAuthBuffer( inData->fInAuthStepData, &computerName, &computerPass );
		if ( siResult == eDSNoErr )
			siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &serviceList );
		if ( siResult == eDSNoErr )
			siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 4, &hostList );
		if ( siResult == eDSNoErr )
			siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 5, &realm );
		if ( siResult != eDSNoErr )
			throw( (SInt32)eDSInvalidBuffFormat );
		if ( strlen(computerPass) > kChangePassPaddedBufferSize )
			throw( (SInt32)eDSAuthPasswordTooLong );
		
		StripRSAKey( computerName );
		
		// special-case for an empty password. DIGEST-MD5 does not
		// support empty passwords, but it's a DS requirement
		if ( DSIsStringEmpty(computerPass) )
		{
			free( computerPass );
			computerPass = strdup( kEmptyPasswordAltStr );
		}
		
		strlcpy(buf, computerPass, sizeof(buf));
		
		gSASLMutex->Wait();
		saslResult = sasl_encode(inContext->conn,
								 buf,
								 kChangePassPaddedBufferSize,
								 &encodedStr,
								 &encodedStrLen); 
		gSASLMutex->Signal();
		
		if ( saslResult != SASL_OK ) {
			DEBUGLOG( "CPSPlugIn::DoAuthMethodNewComputer(): sasl_encode = %d", saslResult );
			throw( (SInt32)eDSAuthFailed );
		}
		
		if ( ConvertBinaryTo64( encodedStr, encodedStrLen, encoded64Str ) == SASL_OK )
		{
			if ( inContext->rsaPublicKeyStr == NULL )
				throw( (SInt32)eDSAuthServerError );
			
			// construct the param list
			commandStrLen = snprintf( buf, sizeof(buf),
								"CHANGEPASS %s %s COMPUTERACCT %s %s %s",
								computerName, encoded64Str, serviceList, hostList, realm );
			result = SendFlushReadWithMutex( inContext, buf, NULL, NULL, buf, sizeof(buf) );			
			if ( result.err != 0 )
				throw( PWSErrToDirServiceError(result) );
			
			// read back the list of principals
			sasl_chop( buf );
			
		}
	}
	catch( SInt32 error )
	{
		siResult = error;
	}
	catch( ... )
	{
	}
	
	DSFreeString( computerName );
	DSFreePassword( computerPass );
	DSFreeString( serviceList );
	DSFreeString( hostList );
	DSFreeString( realm );
	
	return siResult;
}


// ---------------------------------------------------------------------------
//	* DoAuthMethodListReplicas
// ---------------------------------------------------------------------------

SInt32 CPSPlugIn::DoAuthMethodListReplicas( sDoDirNodeAuth *inData, sPSContextData *inContext, tDataBufferPtr outBuf )
{
	SInt32				siResult				= eDSNoErr;
	sPSContinueData		*pContinue				= NULL;
	char				*replicaDataBuff		= NULL;
	unsigned long		replicaDataReceived		= 0;
	unsigned long		replicaDataLen			= 0;
	
	try
	{
		if ( outBuf->fBufferSize < 5 )
			throw( (SInt32)eDSBufferTooSmall );
		
		// repeat visit?
		if ( inData->fIOContinueData != 0 )
		{
			// already verified in DoAuthentication()
			pContinue = (sPSContinueData *)inData->fIOContinueData;
			
			replicaDataLen = pContinue->fDataLen - pContinue->fDataPos;
			if ( 4 + replicaDataLen > outBuf->fBufferSize )
				replicaDataLen = outBuf->fBufferSize - 4;
			
			memcpy( outBuf->fBufferData, &replicaDataLen, 4 );
			memcpy( outBuf->fBufferData + 4, pContinue->fData + pContinue->fDataPos, replicaDataLen );
			outBuf->fBufferLength = 4 + replicaDataLen;
			
			pContinue->fDataPos += replicaDataLen;
			
			if ( pContinue->fDataPos >= pContinue->fDataLen )
			{
				// we are done
				gContinue->RemoveItem( pContinue );
				inData->fIOContinueData = 0;
			}
		}
		else
		{
			siResult = GetReplicaListFromServer( inContext, &replicaDataBuff, &replicaDataReceived );
			if ( siResult != eDSNoErr )
				throw( siResult );
			
			// Do we need a continue data?
			if ( 4 + replicaDataReceived > outBuf->fBufferSize )
			{
				if ( inData->fIOContinueData == 0 )
				{
					pContinue = (sPSContinueData *)::calloc( 1, sizeof( sPSContinueData ) );
					Throw_NULL( pContinue, eMemoryError );
					
					gContinue->AddItem( pContinue, inData->fInNodeRef );
					inData->fIOContinueData = pContinue;
				}
				else
				{
					throw( (SInt32)eDSInvalidContinueData );
				}
				
				pContinue->fData = (unsigned char *) replicaDataBuff;
				pContinue->fDataLen = replicaDataReceived;
				
				pContinue->fDataPos = outBuf->fBufferSize - 4;
				memcpy( outBuf->fBufferData, &(pContinue->fDataPos), 4 );
				memcpy( outBuf->fBufferData + 4, replicaDataBuff, pContinue->fDataPos );
				outBuf->fBufferLength = 4 + pContinue->fDataPos;
			}
			else
			{
				memcpy( outBuf->fBufferData, &replicaDataReceived, 4 );
				memcpy( outBuf->fBufferData + 4, replicaDataBuff, replicaDataReceived );
				outBuf->fBufferLength = 4 + replicaDataReceived;
				
				free( replicaDataBuff );
				replicaDataBuff = NULL;
			}
		}
	}
	catch( SInt32 error )
	{
		siResult = error;
	}
	catch( ... )
	{
	}
	
	return siResult;
}


// ---------------------------------------------------------------------------
//	* DoAuthMethodPull
// ---------------------------------------------------------------------------

SInt32 CPSPlugIn::DoAuthMethodPull( sDoDirNodeAuth *inData, sPSContextData *inContext, tDataBufferPtr outBuf )
{
	SInt32				siResult			= eDSNoErr;
	char				*replicaNameStr			= NULL;
	char				*allStr					= NULL;
	int					dataType				= 0;
	unsigned int		dataSize				= 0;
	unsigned int		dataLen					= 0;
	unsigned char		*dataBuffer				= NULL;
	unsigned char		*decryptPtr				= NULL;
	sPSContinueData		*pContinue				= NULL;
	unsigned char		dbHeaderBlock[sizeof(SInt16) + sizeof(PWFileHeader)];
	unsigned char		passRecBlock[sizeof(SInt16) + sizeof(PWFileEntry)];
	PWServerError		result;
	const char *		dataTypeDesc			= "";
	
	try
	{
		Throw_NULL( outBuf, eParameterError );
		outBuf->fBufferLength = 0;
		
		// repeat visit?
		if ( inData->fIOContinueData != NULL )
		{
			// already verified in DoAuthentication()
			pContinue = (sPSContinueData *)inData->fIOContinueData;
			if ( pContinue != NULL )
			{
				if ( pContinue->fData != NULL )
				{
					decryptPtr = pContinue->fData;
					dataLen = pContinue->fDataLen;
					
					pContinue->fData = NULL;
					pContinue->fDataLen = 0;
				}
				else
				{
					// we are done
					gContinue->RemoveItem( pContinue );
					inData->fIOContinueData = NULL;
				}
			}
		}
		else
		{
			siResult = SetupSecureSyncSession( inContext );
			if ( siResult != eDSNoErr )
				throw( siResult );
		
			siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 1, &replicaNameStr );
			if ( siResult != eDSNoErr )
				throw( siResult );
			
			// ALL parameter is optional
			GetStringFromAuthBuffer( inData->fInAuthStepData, 2, &allStr );
		
			// send
			gPWSConnMutex->Wait();
			result = SendFlush( inContext, "SYNC PULL", replicaNameStr, allStr );
			if ( result.err == 0 )
			{	
				// get results
				result = pwsf_ReadSyncDataFromServerWithCASTKey( inContext, &dataBuffer, &dataType, &dataSize, &dataLen );
			}
			gPWSConnMutex->Signal();
		
			if ( result.err != 0 ) {
				DEBUGLOG( "Error while initiating PULL command = %l", result.err );
			throw( PWSErrToDirServiceError(result) );
			}
		
			decryptPtr = (unsigned char *) malloc( sizeof(SInt16) + dataSize );
			if ( decryptPtr == NULL )
			throw( (SInt32)eMemoryError );
		
			*((SInt16 *)decryptPtr) = (SInt16)dataType;
			RC5_32_cbc_encrypt( dataBuffer, decryptPtr + sizeof(SInt16), dataSize, &inContext->rc5Key, inContext->psIV, RC5_DECRYPT );
		}
		
		while ( decryptPtr != NULL )
		{
			dataTypeDesc = "invalid";
			
			switch( dataType )
			{
				case kDBTypeLastSyncTime:
					// no compression, nothing to do
					siResult = dsAppendAuthBuffer( outBuf, 1, sizeof(SInt16) + dataLen, decryptPtr );
					dataTypeDesc = "kDBTypeLastSyncTime";
					break;
				
				case kDBTypeHeader:
					*((SInt16 *)dbHeaderBlock) = (SInt16)dataType;
					siResult = pwsf_expand_header( decryptPtr + sizeof(SInt16), dataLen,
													(PWFileHeader *)&dbHeaderBlock[sizeof(SInt16)] );
					if ( siResult == eDSNoErr )
						siResult = dsAppendAuthBuffer( outBuf, 1, sizeof(SInt16) + sizeof(PWFileHeader), dbHeaderBlock );
					dataTypeDesc = "kDBTypeHeader";
					break;
				
				case kDBTypeSlot:
					*((SInt16 *)passRecBlock) = (SInt16)dataType;
					siResult = pwsf_expand_slot( decryptPtr + sizeof(SInt16), dataLen,
													(PWFileEntry *)&passRecBlock[sizeof(SInt16)] );
					if ( siResult == eDSNoErr )
						siResult = dsAppendAuthBuffer( outBuf, 1, sizeof(SInt16) + sizeof(PWFileEntry), passRecBlock );
					dataTypeDesc = "kDBTypeSlot";
					break;
				
				case kDBTypeKerberosPrincipal:
					// no compression, nothing to do
					siResult = dsAppendAuthBuffer( outBuf, 1, sizeof(SInt16) + dataLen, decryptPtr );
					dataTypeDesc = "kDBTypeKerberosPrincipal";
				break;
			}
		
			DEBUGLOG( "Received dataType=%s dataSize=%d", dataTypeDesc, dataSize );
			
			if ( siResult == eDSBufferTooSmall )
			{
				if ( inData->fIOContinueData == 0 )
				{
					pContinue = (sPSContinueData *) calloc( 1, sizeof(sPSContinueData) );
					Throw_NULL( pContinue, eMemoryError );
					
					gContinue->AddItem( pContinue, inData->fInNodeRef );
					inData->fIOContinueData = pContinue;
				}
				
				pContinue->fData = decryptPtr;
				pContinue->fDataLen = dataLen;
				decryptPtr = NULL;
				
				siResult = eDSNoErr;
				break;
			}
			else
			if ( siResult != eDSNoErr )
				throw( (SInt32)siResult );
				
			DSFree( dataBuffer );
			DSFree( decryptPtr );
				
			gPWSConnMutex->Wait();
			result = pwsf_ReadSyncDataFromServerWithCASTKey( inContext, &dataBuffer, &dataType, &dataSize, &dataLen );
			gPWSConnMutex->Signal();
				
			if ( result.err != 0 ) {
				DEBUGLOG( "pwsf_ReadSyncDataFromServerWithCASTKey(2) = %l", result.err );
				throw( PWSErrToDirServiceError(result) );
				}
				
			if ( dataBuffer != NULL )
			{
				decryptPtr = (unsigned char *) malloc( sizeof(SInt16) + dataSize );
				if ( decryptPtr == NULL )
					throw( (SInt32)eMemoryError );
				
				*((SInt16 *)decryptPtr) = (SInt16)dataType;
				RC5_32_cbc_encrypt( dataBuffer, decryptPtr + sizeof(SInt16), dataSize, &inContext->rc5Key, inContext->psIV, RC5_DECRYPT );
			}
		}
	}
	catch( SInt32 error )
	{
		siResult = error;
	}
	catch( ... )
	{
		siResult = eDSAuthFailed;
	}
	
	DSFreeString( replicaNameStr );
	DSFreeString( allStr );
	DSFree( dataBuffer );
	DSFree( decryptPtr );
	
	return siResult;
}
    

// ---------------------------------------------------------------------------
//	* DoAuthMethodPush
// ---------------------------------------------------------------------------

SInt32 CPSPlugIn::DoAuthMethodPush( sDoDirNodeAuth *inData, sPSContextData *inContext, tDataBufferPtr outBuf )
{
	SInt32				siResult			= eDSNoErr;
	char				*paramStr			= NULL;
	PWServerError		result				= {0, kPolicyError};
	unsigned char		*syncData			= NULL;
	unsigned char		*encSyncData		= NULL;
	UInt32				syncDataLen			= 0;
	UInt32				*dataType			= 0;
	UInt32				dataTypeLen			= 0;
	char				buf[kOneKBuffer]	= {0};
	int					bufferItemIndex		= 1;
	
	if ( inContext->pushByteCount > 0 && !SecureSyncSessionIsSetup(inContext) )
		return eDSAuthFailed;
	
	try
	{
		siResult = GetDataFromAuthBuffer( inData->fInAuthStepData, bufferItemIndex++, (unsigned char **)&dataType, &dataTypeLen );
		if ( siResult != eDSNoErr )
			throw( siResult );
		if ( dataType == NULL || dataTypeLen != sizeof(UInt32) )
			throw( (SInt32)eDSInvalidBuffFormat );
		
		siResult = GetDataFromAuthBuffer( inData->fInAuthStepData, bufferItemIndex++, &syncData, &syncDataLen );
		if ( siResult != eDSNoErr )
			throw( siResult );
		
		siResult = SetupSecureSyncSession( inContext );
		if ( siResult != eDSNoErr )
			throw( siResult );
		
		inContext->pushByteCount += syncDataLen;
		
		// BEGIN no throw zone
		
		// GetDataFromAuthBuffer pads the allocation for us so we can append zeros
		bzero( syncData + syncDataLen, RC5_32_BLOCK );
		if ( (syncDataLen % RC5_32_BLOCK) != 0 )
			syncDataLen = (syncDataLen / RC5_32_BLOCK) * RC5_32_BLOCK + RC5_32_BLOCK;
		
		encSyncData = (unsigned char *) calloc( 1, syncDataLen + RC5_32_BLOCK );
		if ( encSyncData == NULL )
			siResult = eMemoryError;
		
		if ( siResult == eDSNoErr )
		{
			RC5_32_cbc_encrypt( (unsigned char *)syncData, encSyncData, syncDataLen, &inContext->rc5Key, inContext->psIV, RC5_ENCRYPT );
			paramStr = (char *)malloc( syncDataLen * 4/3 + 20 );
			if ( paramStr == NULL )
				siResult = eMemoryError;
		}
		
		if ( siResult == eDSNoErr ) {
			siResult = ConvertBinaryTo64( (char *)encSyncData, syncDataLen, paramStr );
			if ( siResult != eDSNoErr )
				DEBUGLOG( "PasswordServer PlugIn: CPSPlugIn::DoAuthMethodPush, ConvertBinaryTo64 = %l", siResult );
		}
		
		if ( encSyncData != NULL )
			free( encSyncData );
		
		// END no throw zone
		
		if ( siResult == eDSNoErr )
		{
			snprintf( buf, sizeof(buf), "SYNC PUSH %lu", *dataType );
			result = SendFlushReadWithMutex( inContext, buf, paramStr, NULL, buf, sizeof(buf) );
			siResult = PWSErrToDirServiceError( result );
			if ( siResult != eDSNoErr )
				DEBUGLOG( "PasswordServer PlugIn: CPSPlugIn::DoAuthMethodPush, SendFlushReadWithMutex = %l", siResult );
		}
	}
	catch( SInt32 error )
	{
		siResult = error;
	}
	catch( ... )
	{
		DEBUGLOG( "PasswordServer PlugIn: CPSPlugIn::DoAuthMethodPush, uncasted throw" );
	}
	
	if ( paramStr != NULL )
		free( paramStr );
	if ( syncData != NULL )
		free( syncData );
	
	return siResult;
}


// ---------------------------------------------------------------------------
//	* DoAuthMethodNTUserSessionKey
// ---------------------------------------------------------------------------

SInt32 CPSPlugIn::DoAuthMethodNTUserSessionKey(
	SInt32 inAuthenticatorAuthResult,
	UInt32 inAuthMethod,
	sDoDirNodeAuth *inData,
	sPSContextData *inContext,
	tDataBufferPtr outBuf )
{
	SInt32				siResult			= eDSNoErr;
	char				*userID				= NULL;
	char				*paramStr			= NULL;
	char				*stepData			= NULL;
	int					saslResult			= SASL_OK;
	sPSContinueData		*pContinue			= NULL;
	PWServerError		result				= {0};
	unsigned char		*challenge			= NULL;
	unsigned char		*digest				= NULL;
	UInt32				len					= 0;
	char				password[32]		= {0};
	char				buf[kOneKBuffer]	= {0,};
	
	try
	{
		siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 1, &userID );
		if ( siResult == eDSNoErr )
		{
			const char *decryptedStr;
			unsigned long decodedStrLen;
			unsigned decryptedStrLen;
			
			if ( userID == NULL )
				throw( (SInt32)eDSInvalidBuffFormat );
			
			StripRSAKey( userID );
			
			if ( inAuthenticatorAuthResult == eDSNoErr )
			{
				result = SendFlushReadWithMutex( inContext, "GETNTHASHHASH", userID, NULL, buf, sizeof(buf) );
				if ( result.err != 0 )
					throw( PWSErrToDirServiceError(result) );
				
				decodedStrLen = strlen( buf ) - 4;
				if ( decodedStrLen < 2 )
					throw( (SInt32)eDSAuthServerError );
				
				stepData = (char *) malloc( decodedStrLen );
				if ( stepData == NULL )
					throw( (SInt32)eMemoryError );
				
				// decode
				if ( Convert64ToBinary( buf + 4, stepData, decodedStrLen, &decodedStrLen ) != SASL_OK )
					throw( (SInt32)eDSAuthServerError );
				
				// decrypt
				gSASLMutex->Wait();
				saslResult = sasl_decode( inContext->conn,
											stepData,
											decodedStrLen,
											&decryptedStr,
											&decryptedStrLen);
				gSASLMutex->Signal();
				
				if ( saslResult != SASL_OK )
					throw( (SInt32)eDSAuthFailed );
				
				if ( outBuf->fBufferSize < decryptedStrLen + 4 )
					throw( (SInt32)eDSBufferTooSmall );
				
				outBuf->fBufferLength = decryptedStrLen + 4;
				decodedStrLen = decryptedStrLen;
				memcpy( outBuf->fBufferData, &decodedStrLen, 4 );
				memcpy( outBuf->fBufferData + 4, decryptedStr, decryptedStrLen );
			}
			
			if ( inAuthMethod == kAuthNTSessionKey )
			{
				// now do the NT auth
				siResult = GetDataFromAuthBuffer( inData->fInAuthStepData, 2, &challenge, &len );
				if ( siResult != eDSNoErr || challenge == NULL || len != 8 )
					throw( (SInt32)eDSInvalidBuffFormat );
				
				siResult = GetDataFromAuthBuffer( inData->fInAuthStepData, 3, &digest, &len );
				if ( siResult != eDSNoErr || digest == NULL || len != 24 )
					throw( (SInt32)eDSInvalidBuffFormat );
				
				memcpy( password, challenge, 8 );
				memcpy( password + 8, digest, 24 );
				
				if ( inData->fIOContinueData == 0 )
				{
					pContinue = (sPSContinueData *) calloc( 1, sizeof( sPSContinueData ) );
					Throw_NULL( pContinue, eMemoryError );
					
					gContinue->AddItem( pContinue, inData->fInNodeRef );
					inData->fIOContinueData = pContinue;
				}
				
				siResult = DoSASLAuth( inContext, userID, password, 32, NULL, "SMB-NT", inData, &stepData );
			}
			else
			{
				siResult = inAuthenticatorAuthResult;
			}
		}
	}
	catch( SInt32 error )
	{
		siResult = error;
	}
	catch( ... )
	{
	}
	
	if ( userID != NULL )
		free( userID );
	if ( paramStr != NULL )
		free( paramStr );
	if ( stepData != NULL )
		free( stepData );
	
	return siResult;
}


// ---------------------------------------------------------------------------
//	* DoAuthMethodMSChapChangePass
// ---------------------------------------------------------------------------

SInt32 CPSPlugIn::DoAuthMethodMSChapChangePass( sDoDirNodeAuth *inData, sPSContextData *inContext )
{
	SInt32				siResult			= eDSNoErr;
	char				*userIDToSet		= NULL;
	char				*paramStr			= NULL;
	UInt32 				paramLen			= 0;
	long				encoding			= 0;
	PWServerError		result;
	char				buf[kOneKBuffer];
	char				encoded64Str[kOneKBuffer];
	
	try
	{
		siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 1, &userIDToSet );
		if ( siResult == eDSNoErr )
		{
			StripRSAKey( userIDToSet );
			siResult = GetDataFromAuthBuffer( inData->fInAuthStepData, 2, (unsigned char **)&paramStr, &paramLen );
			if ( siResult == eDSNoErr )
			{
				encoding = paramStr[0];
				free( paramStr );
				paramStr = NULL;
				siResult = GetDataFromAuthBuffer( inData->fInAuthStepData, 3, (unsigned char **)&paramStr, &paramLen );
			}
		}
		if ( siResult == eDSNoErr )
		{
			snprintf( encoded64Str, sizeof(encoded64Str), "%ld ", encoding );
			siResult = ConvertBinaryTo64( paramStr, paramLen, encoded64Str + strlen(encoded64Str) );
			if ( siResult != eDSNoErr )
				throw( siResult );
			
			result = SendFlushReadWithMutex( inContext, "MSCHAPCHANGEPASS", userIDToSet, encoded64Str, buf, sizeof(buf) );
			if ( result.err != 0 )
				throw( PWSErrToDirServiceError(result) );
		}
	}
	catch( SInt32 error )
	{
		siResult = error;
	}
	catch( ... )
	{
	}
	
	if ( userIDToSet != NULL )
		free( userIDToSet );
	if ( paramStr != NULL )
		free( paramStr );
	
	return siResult;
}


// ---------------------------------------------------------------------------
//	* DoAuthMethodEncryptToUser
// ---------------------------------------------------------------------------

SInt32 CPSPlugIn::DoAuthMethodEncryptToUser( sDoDirNodeAuth *inData, sPSContextData *inContext, tDataBufferPtr outBuf )
{
	SInt32				siResult			= eDSNoErr;
	char				*userIDToSet		= NULL;
	unsigned char		*dataToEncrypt		= NULL;
	UInt32				dataToEncryptLen	= 0;
	unsigned long		binBufLen			= 0;
	const char			*encodedStr			= NULL;
	unsigned int		encodedStrLen		= 0;
	int					saslResult			= SASL_OK;
	PWServerError		result;
	char				buf[kOneKBuffer];
	char				binBuf[kOneKBuffer];
	
	try
	{
		siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 1, &userIDToSet );
		if ( siResult == eDSNoErr )
        	siResult = GetDataFromAuthBuffer( inData->fInAuthStepData, 2, &dataToEncrypt, &dataToEncryptLen );
		if ( siResult != eDSNoErr )
			throw( siResult );
		
		gSASLMutex->Wait();
		saslResult = sasl_encode( inContext->conn, (char *)dataToEncrypt, dataToEncryptLen, &encodedStr, &encodedStrLen ); 
		gSASLMutex->Signal();
		
		if ( saslResult != SASL_OK )
			throw( (SInt32)eDSAuthFailed );
		
		siResult = ConvertBinaryTo64( (char *)encodedStr, encodedStrLen, buf );
		if ( siResult != eDSNoErr )
			throw( siResult );
		
		StripRSAKey( userIDToSet );
		result = SendFlushReadWithMutex( inContext, "CYPHER ENCRYPT", userIDToSet, buf, buf, sizeof(buf) );
		siResult = PWSErrToDirServiceError( result );
		if ( siResult == eDSNoErr )
		{
			siResult = Convert64ToBinary( buf + 4, binBuf, sizeof(binBuf), &binBufLen );
			if ( siResult == eDSNoErr )
			{
				if ( outBuf->fBufferSize < 4 + binBufLen )
					throw( (SInt32)eDSBufferTooSmall );
				
				outBuf->fBufferLength = 4 + binBufLen;
				memcpy( outBuf->fBufferData, &binBufLen, 4 );
				memcpy( outBuf->fBufferData + 4, binBuf, binBufLen );
			}
		}
	}
	catch( SInt32 error )
	{
		siResult = error;
	}
	catch( ... )
	{
	}
	
	if ( userIDToSet != NULL )
		free( userIDToSet );
	if ( dataToEncrypt != NULL )
		free( dataToEncrypt );
	
	return siResult;
}


// ---------------------------------------------------------------------------
//	* DoAuthMethodDecrypt
// ---------------------------------------------------------------------------

SInt32 CPSPlugIn::DoAuthMethodDecrypt( sDoDirNodeAuth *inData, sPSContextData *inContext, tDataBufferPtr outBuf )
{
	SInt32				siResult			= eDSNoErr;
	unsigned char		*dataToDecrypt		= NULL;
	UInt32				dataToDecryptLen	= 0;
	unsigned long		binBufLen			= 0;
	const char			*decodedStr			= NULL;
	unsigned int		decodedStrLen		= 0;
	int					saslResult			= SASL_OK;
	PWServerError		result;
	char				buf[kOneKBuffer];
	char				binBuf[kOneKBuffer];
	
	try
	{
		siResult = GetDataFromAuthBuffer( inData->fInAuthStepData, 2, &dataToDecrypt, &dataToDecryptLen );
		if ( siResult != eDSNoErr )
			throw( siResult );
		
		siResult = ConvertBinaryTo64( (char *)dataToDecrypt, dataToDecryptLen, buf );
		if ( siResult != eDSNoErr )
			throw( siResult );
		
		result = SendFlushReadWithMutex( inContext, "CYPHER DECRYPT", buf, NULL, buf, sizeof(buf) );
		siResult = PWSErrToDirServiceError( result );
		if ( siResult != eDSNoErr )
			throw( siResult );
		
		// the buffer to sasl_decode needs to be padded to the key size (CAST128 == 16 bytes)
		// prefill with zeros
		bzero( binBuf, sizeof(binBuf) );
		siResult = Convert64ToBinary( buf + 4, binBuf, sizeof(binBuf), &binBufLen );
		if ( siResult != eDSNoErr )
			throw( siResult );
		
		// add the remainder
		if ( (binBufLen % 16) )
			binBufLen += 16 - (binBufLen % 16);
		gSASLMutex->Wait();
		saslResult = sasl_decode( inContext->conn, binBuf, binBufLen, &decodedStr, &decodedStrLen ); 
		gSASLMutex->Signal();
		if ( saslResult != SASL_OK )
			throw( (SInt32)eDSAuthFailed );
		
		if ( outBuf->fBufferSize < decodedStrLen + 4 )
			throw( (SInt32)eDSBufferTooSmall );

		binBufLen = (UInt32)decodedStrLen;
		outBuf->fBufferLength = binBufLen + 4;
		memcpy( outBuf->fBufferData, &binBufLen, 4 );
		memcpy( outBuf->fBufferData + 4, decodedStr, binBufLen );
	}
	catch( SInt32 error )
	{
		siResult = error;
	}
	catch( ... )
	{
	}
	
	if ( dataToDecrypt != NULL )
		free( dataToDecrypt );
	
	return siResult;
}


// ---------------------------------------------------------------------------
//	* DoAuthMethodSetHash
// ---------------------------------------------------------------------------

SInt32 CPSPlugIn::DoAuthMethodSetHash( sDoDirNodeAuth *inData, sPSContextData *inContext, const char *inCommandStr )
{
	SInt32				siResult			= eDSNoErr;
	char				*userIDToSet		= NULL;
	char				*paramStr			= NULL;
	UInt32 				paramLen			= 0;
	int					saslResult			= SASL_OK;
	const char			*encodedStr			= NULL;
	unsigned int		encodedStrLen		= 0;
	PWServerError		result;
	char				buf[kOneKBuffer];
	char				encoded64Str[kOneKBuffer];

	try
	{
		siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 1, &userIDToSet );
		if ( siResult == eDSNoErr )
			siResult = GetDataFromAuthBuffer( inData->fInAuthStepData, 2, (unsigned char **)&paramStr, &paramLen );
		if ( siResult != eDSNoErr )
			throw( siResult );
		
		memcpy( buf, paramStr, paramLen );
		
		gSASLMutex->Wait();
		saslResult = sasl_encode(inContext->conn,
								 buf,
								 kChangePassPaddedBufferSize,
								 &encodedStr,
								 &encodedStrLen); 
		gSASLMutex->Signal();
		
		if ( siResult == eDSNoErr && saslResult == SASL_OK && userIDToSet != NULL )
		{
			if ( ConvertBinaryTo64( encodedStr, encodedStrLen, encoded64Str ) == SASL_OK )
			{
				result = SendFlushReadWithMutex( inContext, inCommandStr, userIDToSet, encoded64Str, buf, sizeof(buf) );
				siResult = PWSErrToDirServiceError( result );
			}
		}
	}
	catch( SInt32 error )
	{
		siResult = error;
	}
	catch( ... )
	{
	}
	
	if ( userIDToSet != NULL )
		free( userIDToSet );
	if ( paramStr != NULL )
		free( paramStr );
	
	return siResult;
}
	

// ---------------------------------------------------------------------------
//	* DoAuthMethodNTLMv2SessionKey
// ---------------------------------------------------------------------------

SInt32 CPSPlugIn::DoAuthMethodNTLMv2SessionKey(
	SInt32 inAuthenticatorAuthResult,
	UInt32 inAuthMethod,
	sDoDirNodeAuth *inData,
	sPSContextData *inContext,
	tDataBufferPtr outBuf )
{
	SInt32				siResult			= eDSNoErr;
	char				*userIDToGet		= NULL;
	unsigned char		*clientBlob			= NULL;
	UInt32				clientBlobLen		= 0;
	char				*user				= NULL;
	char				*domain				= NULL;
	char				*paramStr			= NULL;
	size_t				paramStrLen			= 0;
	char				*stepData			= NULL;
	int					saslResult			= SASL_OK;
	const char			*decryptedStr;
	unsigned long		encodedStrLen;
	unsigned long		decodedStrLen;
	unsigned			decryptedStrLen;
	PWServerError		result;
	char				buf[kOneKBuffer];
	char				encoded64Str[kOneKBuffer];
	sPSContinueData		*pContinue			= NULL;
	int					bufferOffset		= (inAuthMethod == kAuthNTLMv2WithSessionKey) ? 1 : 0;
	
	try
	{	
		// User ID
		siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 1, &userIDToGet );
		if ( siResult != eDSNoErr )
			throw( siResult );
		if ( userIDToGet == NULL )
			throw( (SInt32)eDSInvalidBuffFormat );
		StripRSAKey( userIDToGet );
		
		// Client Blob, username, domain
		siResult = GetDataFromAuthBuffer( inData->fInAuthStepData, 2 + bufferOffset,
											&clientBlob, &clientBlobLen );
		if ( siResult == eDSNoErr )
		{
			if ( clientBlobLen < 24 )
				throw( (SInt32)eParameterError );
			siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3 + bufferOffset, &user );
			if ( siResult == eDSNoErr )
				siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 4 + bufferOffset, &domain );
		}
		if ( siResult != eDSNoErr )
			throw( siResult );
		
		// pack request
		if ( ConvertBinaryTo64( (char *)clientBlob, 16, encoded64Str ) != SASL_OK )
			throw( (SInt32)eParameterError );
		paramStrLen = strlen(userIDToGet) + strlen(encoded64Str) + strlen(user) + strlen(domain) + 4;
		paramStr = (char *) malloc( paramStrLen );
		if ( paramStr == NULL )
			throw( (SInt32)eMemoryError );
		snprintf( paramStr, paramStrLen, "%s %s %s %s", userIDToGet, encoded64Str, user, domain );
		
		// round-trip to password server
		result = SendFlushReadWithMutex( inContext, "GETNTLM2SESSKEY", paramStr, NULL, buf, sizeof(buf) );
		if ( result.err != 0 )
			throw( PWSErrToDirServiceError(result) );
		
		// pack response
		encodedStrLen = strlen( buf ) - 4;
		if ( encodedStrLen < 24 )
			throw( (SInt32)eDSAuthServerError );
		
		stepData = (char *) malloc( encodedStrLen );
		if ( stepData == NULL )
			throw( (SInt32)eMemoryError );
		
		// decode
		if ( Convert64ToBinary( buf + 4, stepData, encodedStrLen, &decodedStrLen ) != SASL_OK )
			throw( (SInt32)eDSAuthServerError );
		
		// decrypt
		gSASLMutex->Wait();
		saslResult = sasl_decode( inContext->conn,
									stepData,
									decodedStrLen,
									&decryptedStr,
									&decryptedStrLen );
		gSASLMutex->Signal();
		
		if ( saslResult != SASL_OK )
			throw( (SInt32)eDSAuthFailed );
		
		if ( outBuf->fBufferSize < decryptedStrLen + 4 )
			throw( (SInt32)eDSBufferTooSmall );
		
		outBuf->fBufferLength = decryptedStrLen + 4;
		decodedStrLen = decryptedStrLen;
		memcpy( outBuf->fBufferData, &decodedStrLen, 4 );
		memcpy( outBuf->fBufferData + 4, decryptedStr, decryptedStrLen );
		
		if ( inAuthMethod == kAuthNTLMv2WithSessionKey )
		{
			char *userName;
			char *passwordStr;
			UInt32 passwordLen;
			char *challenge;
			
			// now do the NTLMv2 auth
			siResult = UnpackUsernameAndPassword( inContext, kAuthNTLMv2, inData->fInAuthStepData, &userName, &passwordStr, 
													&passwordLen, &challenge );
			if ( siResult != eDSNoErr )
				throw( siResult );
			
			if ( inData->fIOContinueData == 0 )
			{
				pContinue = (sPSContinueData *) calloc( 1, sizeof(sPSContinueData) );
				Throw_NULL( pContinue, eMemoryError );
				
				gContinue->AddItem( pContinue, inData->fInNodeRef );
				inData->fIOContinueData = pContinue;
			}
			
			siResult = DoSASLAuth( inContext, userName, passwordStr, passwordLen, challenge, "SMB-NTLMv2", inData, &stepData );
		}
		else
		{
			siResult = inAuthenticatorAuthResult;
		}
	}
	catch( SInt32 error )
	{
		siResult = error;
	}
	catch( ... )
	{
		siResult = eDSAuthFailed;
	}
	
	if ( userIDToGet != NULL )
		free( userIDToGet );
	if ( user != NULL )
		free( user );
	if ( domain != NULL )
		free( domain );
	if ( paramStr != NULL )
		free( paramStr );
	if ( stepData != NULL )
		free( stepData );
	
	return siResult;
}


// ---------------------------------------------------------------------------
//	* DoAuthMethodGetStats
// ---------------------------------------------------------------------------

SInt32 CPSPlugIn::DoAuthMethodGetStats( sDoDirNodeAuth *inData, sPSContextData *inContext, tDataBufferPtr outBuf )
{
	SInt32				siResult				= eDSNoErr;
	char				*sampleCountStr			= NULL;
	sPSContinueData		*pContinue				= NULL;
	char				*statDataBuff			= NULL;
	unsigned long		statDataReceived		= 0;
	unsigned long		statDataLen				= 0;
	char				commandBuff[256]		= {0,};
	int					sampleCount				= 0;
	
	try
	{
		if ( outBuf->fBufferSize < 5 )
			throw( (SInt32)eDSBufferTooSmall );
		
		// repeat visit?
		if ( inData->fIOContinueData != 0 )
		{
			// already verified in DoAuthentication()
			pContinue = (sPSContinueData *)inData->fIOContinueData;
			
			statDataLen = pContinue->fDataLen - pContinue->fDataPos;
			if ( 4 + statDataLen > outBuf->fBufferSize )
				statDataLen = outBuf->fBufferSize - 4;
			
			memcpy( outBuf->fBufferData, &statDataLen, 4 );
			memcpy( outBuf->fBufferData + 4, pContinue->fData + pContinue->fDataPos, statDataLen );
			outBuf->fBufferLength = 4 + statDataLen;
			
			pContinue->fDataPos += statDataLen;
			
			if ( pContinue->fDataPos >= pContinue->fDataLen )
			{
				// we are done
				gContinue->RemoveItem( pContinue );
				inData->fIOContinueData = 0;
			}
		}
		else
		{
			siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 1, &sampleCountStr );
			if ( siResult != eDSNoErr )
				throw( siResult );
			if ( DSIsStringEmpty(sampleCountStr) )
			{
				strcpy( commandBuff, "GETSTATS" );
			}
			else
			{
				sscanf( sampleCountStr, "%d", &sampleCount );
				if ( sampleCount <= 0 )
					throw( (SInt32)eDSInvalidBuffFormat );
				
				snprintf( commandBuff, sizeof(commandBuff), "GETSTATS %d", sampleCount );
			}
			siResult = GetLargeReplyFromServer( commandBuff, inContext, &statDataBuff, &statDataReceived );
			if ( siResult != eDSNoErr )
				throw( siResult );
			
			// Do we need a continue data?
			if ( 4 + statDataReceived > outBuf->fBufferSize )
			{
				if ( inData->fIOContinueData == 0 )
				{
					pContinue = (sPSContinueData *)::calloc( 1, sizeof( sPSContinueData ) );
					Throw_NULL( pContinue, eMemoryError );
					
					gContinue->AddItem( pContinue, inData->fInNodeRef );
					inData->fIOContinueData = pContinue;
				}
				else
				{
					throw( (SInt32)eDSInvalidContinueData );
				}
				
				pContinue->fData = (unsigned char *) statDataBuff;
				pContinue->fDataLen = statDataReceived;
				
				pContinue->fDataPos = outBuf->fBufferSize - 4;
				memcpy( outBuf->fBufferData, &(pContinue->fDataPos), 4 );
				memcpy( outBuf->fBufferData + 4, statDataBuff, pContinue->fDataPos );
				outBuf->fBufferLength = 4 + pContinue->fDataPos;
			}
			else
			{
				memcpy( outBuf->fBufferData, &statDataReceived, 4 );
				memcpy( outBuf->fBufferData + 4, statDataBuff, statDataReceived );
				outBuf->fBufferLength = 4 + statDataReceived;
				
				free( statDataBuff );
				statDataBuff = NULL;
			}
		}
	}
	catch( SInt32 error )
	{
		siResult = error;
	}
	catch( ... )
	{
	}
	
	if ( sampleCountStr != NULL ) {
		free( sampleCountStr );
		sampleCountStr = NULL;
	}
	
	return siResult;
}


// ---------------------------------------------------------------------------
//	* DoAuthMethodGetChangeList
// ---------------------------------------------------------------------------

SInt32 CPSPlugIn::DoAuthMethodGetChangeList( sDoDirNodeAuth *inData, sPSContextData *inContext, tDataBufferPtr outBuf )
{
	SInt32				siResult				= eDSNoErr;
	sPSContinueData		*pContinue				= NULL;
	char				*statDataBuff			= NULL;
	unsigned long		statDataReceived		= 0;
	unsigned long		statDataLen				= 0;
	
	try
	{
		if ( outBuf->fBufferSize < 5 )
			throw( (SInt32)eDSBufferTooSmall );
		
		// repeat visit?
		if ( inData->fIOContinueData != 0 )
		{
			// already verified in DoAuthentication()
			pContinue = (sPSContinueData *)inData->fIOContinueData;
			
			statDataLen = pContinue->fDataLen - pContinue->fDataPos;
			if ( 4 + statDataLen > outBuf->fBufferSize )
				statDataLen = outBuf->fBufferSize - 4;
			
			memcpy( outBuf->fBufferData, &statDataLen, 4 );
			memcpy( outBuf->fBufferData + 4, pContinue->fData + pContinue->fDataPos, statDataLen );
			outBuf->fBufferLength = 4 + statDataLen;
			
			pContinue->fDataPos += statDataLen;
			
			if ( pContinue->fDataPos >= pContinue->fDataLen )
			{
				// we are done
				gContinue->RemoveItem( pContinue );
				inData->fIOContinueData = 0;
			}
		}
		else
		{			
			siResult = GetLargeReplyFromServer( "CHANGELIST", inContext, &statDataBuff, &statDataReceived );
			if ( siResult != eDSNoErr )
				throw( siResult );

			// Do we need a continue data?
			if ( 4 + statDataReceived > outBuf->fBufferSize )
			{
				if ( inData->fIOContinueData == 0 )
				{
					pContinue = (sPSContinueData *)::calloc( 1, sizeof( sPSContinueData ) );
					Throw_NULL( pContinue, eMemoryError );
					
					gContinue->AddItem( pContinue, inData->fInNodeRef );
					inData->fIOContinueData = pContinue;
				}
				else
				{
					throw( (SInt32)eDSInvalidContinueData );
				}
				
				pContinue->fData = (unsigned char *) statDataBuff;
				pContinue->fDataLen = statDataReceived;
				
				pContinue->fDataPos = outBuf->fBufferSize - 3;
				memcpy( outBuf->fBufferData, &(pContinue->fDataPos), 4 );
				memcpy( outBuf->fBufferData + 4, statDataBuff + 1, pContinue->fDataPos - 1 );
				outBuf->fBufferLength = 4 + pContinue->fDataPos;
			}
			else
			{
				memcpy( outBuf->fBufferData, &statDataReceived, 4 );
				memcpy( outBuf->fBufferData + 4, statDataBuff + 1, statDataReceived );
				outBuf->fBufferLength = 4 + statDataReceived;
				
				free( statDataBuff );
				statDataBuff = NULL;
			}
		}
	}
	catch( SInt32 error )
	{
		siResult = error;
	}
	catch( ... )
	{
	}
		
	return siResult;
}


#pragma mark -

// ---------------------------------------------------------------------------
//	* SetServiceInfo
//
//	Returns: DS error
//
//	Checks for service information in the second dsDoDirNodeAuth() buffer.
//	Returns eDSNoErr if the information is not included; only returns an
//	error if the data is provided but invalid.
// ---------------------------------------------------------------------------

SInt32 CPSPlugIn::SetServiceInfo( sDoDirNodeAuth *inData, sPSContextData *inContext )
{
	SInt32					siResult				= eDSNoErr;
	char					*plistStr				= NULL;
	char					*infoStr				= NULL;
	char					*base64Str				= NULL;
	UInt32					plistStrLen				= 0;
	CFDataRef				plistData				= NULL;
	CFDictionaryRef			plistDict				= NULL;
	CFDictionaryRef			infoDict				= NULL;
	CFDataRef				infoData				= NULL;
	CFIndex					infoDataLength			= 0;
	CFStringRef				errorString				= NULL;
	
	do
	{
		DSFreeString( inContext->serviceInfoStr );
		
		if ( GetStringFromAuthBuffer(inData->fOutAuthStepDataResponse, 1, &plistStr) == eDSNoErr && plistStr != NULL )
		{
			// make sure we got a plist
			plistStrLen = strlen( plistStr );
			plistData = CFDataCreate( kCFAllocatorDefault, (const unsigned char *)plistStr, plistStrLen );
			if ( plistData == NULL ) {
				siResult = eMemoryError;
				break;
			}
			
			plistDict = (CFDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, plistData,
							kCFPropertyListImmutable, &errorString );
			if ( plistDict == NULL || CFGetTypeID(plistDict) != CFDictionaryGetTypeID() ) {
				DEBUGLOG( "CPSPlugIn::SetServiceInfo(): data from service is not in dictionary form." );
				siResult = eDSInvalidBuffFormat;
				break;
			}
			
			// Extract the ServiceInformation dictionary
			if ( CFDictionaryGetValueIfPresent(plistDict, CFSTR("ServiceInformation"), (const void **)&infoDict) && infoDict != NULL )
			{
				if ( CFGetTypeID(infoDict) != CFDictionaryGetTypeID() ) {
					DEBUGLOG( "CPSPlugIn::SetServiceInfo(): ServiceInformation from service is not in dictionary form." );
					siResult = eDSInvalidBuffFormat;
					break;
				}
				
				infoData = CFPropertyListCreateXMLData( kCFAllocatorDefault, infoDict );
				if ( infoData != NULL )
				{
					infoDataLength = CFDataGetLength( infoData );
					if ( infoDataLength == 0 ) {
						DEBUGLOG( "CPSPlugIn::SetServiceInfo(): ServiceInformation from service was sent but the dictionary is empty." );
						siResult = eDSInvalidBuffFormat;
						break;
					}
					
					infoStr = (char *) calloc( 1, infoDataLength + 1 );
					if ( infoStr == NULL ) {
						siResult = eMemoryError;
						break;
					}
					
					CFDataGetBytes( infoData, CFRangeMake(0,infoDataLength), (UInt8 *)infoStr );
					
					// The plist is valid, convert to base-64.
					base64Str = (char *) malloc( ((infoDataLength + 1) * 4 / 3) + 1 );
					if ( base64Str == NULL ) {
						siResult = eMemoryError;
						break;
					}
					
					ConvertBinaryTo64( infoStr, infoDataLength, base64Str );
					inContext->serviceInfoStr = base64Str;
				}
			}
			inData->fOutAuthStepDataResponse->fBufferLength = 0;
		}
	}
	while (0);
	
	DSCFRelease( plistDict );
	DSCFRelease( plistData );
	DSCFRelease( errorString );
	DSCFRelease( infoData );
	DSFreeString( infoStr );
	DSFreeString( plistStr );		
	
	return siResult;
}


// ---------------------------------------------------------------------------
//	* GetReplicaListFromServer
//
//	Returns: DS error
// ---------------------------------------------------------------------------

SInt32 CPSPlugIn::GetReplicaListFromServer( sPSContextData *inContext, char **outData, unsigned long *outDataLen )
{
	return GetLargeReplyFromServer( "LISTREPLICAS", inContext, outData, outDataLen );
}


// ---------------------------------------------------------------------------
//	* GetLargeReplyFromServer
//
//	Returns: DS error
// ---------------------------------------------------------------------------

SInt32 CPSPlugIn::GetLargeReplyFromServer( const char *inCommand, sPSContextData *inContext, char **outData, unsigned long *outDataLen )
{
	SInt32				siResult				= eDSNoErr;
	char				*replicaDataBuff		= NULL;
	bool				moreData				= false;
	unsigned long		replicaDataLen			= 0;
	unsigned long		replicaDataReceived		= 0;
	unsigned long		replicaDataOneTimeLen   = 0;
	char *				bufPtr;
	fd_set				fdset;
	PWServerError		result;
	struct timeval		waitMax					= { 10, 0 };
	char				buf[kOneKBuffer];
	
	if ( outData == NULL || outDataLen == NULL )
		return eParameterError;
	*outData = NULL;
	*outDataLen = 0;
	
	try
	{
		gPWSConnMutex->Wait();
		result = SendFlush( inContext, inCommand, NULL, NULL );
		if ( result.err == 0 )
		{
			// LISTREPLICAS always comes back without encryption
			result = readFromServer( inContext->fd, buf, sizeof(buf) );
		}
		gPWSConnMutex->Signal();
		if ( result.err != 0 )
			throw( PWSErrToDirServiceError(result) );
				
		sscanf( buf + 4, "%lu", &replicaDataLen );

		bufPtr = strchr( buf + 4, ' ' );
		if ( bufPtr == NULL )
			throw( (SInt32)eDSAuthServerError );
		
		replicaDataReceived = strlen( ++bufPtr );

		replicaDataBuff = (char *) malloc( replicaDataLen + 20 );
		Throw_NULL( replicaDataBuff, eMemoryError );
		
		memcpy( replicaDataBuff, bufPtr, replicaDataReceived );
		
		FD_ZERO( &fdset );
		FD_SET( inContext->fd, &fdset );
		
		while ( replicaDataReceived < replicaDataLen ) 
		{
			moreData = select( FD_SETSIZE, &fdset, NULL, NULL, &waitMax );
			if ( moreData <= 0 )
				break;
			
			result = readFromServer( inContext->fd, buf, sizeof(buf) );
			if ( result.err != 0 )
				throw( PWSErrToDirServiceError(result) );
			
			replicaDataOneTimeLen = strlen( buf );
			memcpy( replicaDataBuff + replicaDataReceived, buf, replicaDataOneTimeLen );
			replicaDataReceived += replicaDataOneTimeLen;
		}

		if ( replicaDataReceived == replicaDataLen + 2 )
		{
			// all went well, take off the /r/n at the end of the line
			replicaDataReceived -= 2;
			
			// allow data to be used as a c-string
			*(replicaDataBuff + replicaDataReceived) = '\0';
			
			// attach the results
			*outData = replicaDataBuff;
			*outDataLen = replicaDataReceived;
		}
	}
	catch( SInt32 error )
	{
		siResult = error;
	}
	catch( ... )
	{
	}
	
	if ( siResult != eDSNoErr && replicaDataBuff != NULL )
	{
		free( replicaDataBuff );
		*outData = NULL;
	}
	
	return siResult;	
}

	
// ---------------------------------------------------------------------------
//	* UseCurrentAuthenticationIfPossible
//
//	Returns: eDSNoErr, eParameterError or eMemoryError
//	
//	If the user account has not changed, set <inOutHasValidAuth> by auth
//	method, depending on whether or not that method allows reuse. Generally,
//	administrative auth methods are reuseable, and methods requiring secure
//	sessions or plain auths are not. 
//	If the user account has changed, switch back if the last
//	authenticated user was not authOnly.
// ---------------------------------------------------------------------------

SInt32 CPSPlugIn::UseCurrentAuthenticationIfPossible( sPSContextData *inContext, const char *inUserName, UInt32 inAuthMethod, Boolean *inOutHasValidAuth )
{
	SInt32			siResult				= eDSNoErr;
	char			*strippedUserName		= NULL;
	long			len;
	
	if ( inContext == NULL || inOutHasValidAuth == NULL )
		return eParameterError;
	
	*inOutHasValidAuth = false;
	
	try
	{
		if ( (inContext->last.successfulAuth || inContext->nao.successfulAuth) && inUserName != NULL )
		{
			len = strlen( inUserName );
			
			strippedUserName = (char *) malloc( len + 1 );
			Throw_NULL( strippedUserName, eMemoryError );
			
			strcpy( strippedUserName, inUserName );
			StripRSAKey( strippedUserName );
			if ( strcmp( strippedUserName, inContext->last.username ) == 0 )
			{
				// if the name is a match for the last authentication, then
				// the state is correct.
				switch ( inAuthMethod )
				{
					case kAuthGetPolicy:
					case kAuthSetPolicy:
					case kAuthGetGlobalPolicy:
					case kAuthSetGlobalPolicy:
					case kAuthGetUserName:
					case kAuthSetUserName:
					case kAuthGetUserData:
					case kAuthSetUserData:
					case kAuthDeleteUser:
					case kAuthGetIDByName:
					case kAuthSyncSetupReplica:
					case kAuthListReplicas:
					case kAuthGetEffectivePolicy:
					case kAuthGetKerberosPrincipal:
					case kAuthMSLMCHAP2ChangePasswd:
					case kAuthVPN_PPTPMasterKeys:
					case kAuthGetStats:
					case kAuthGetChangeList:
						*inOutHasValidAuth = true;
						break;
					
					case kAuthNewUser:
					case kAuthNewUserWithPolicy:
					case kAuthNewComputer:
					case kAuthSetPasswdAsRoot:
					case kAuthSetComputerAcctPasswdAsRoot:
					case kAuthSetPolicyAsRoot:
					case kAuthSMB_NTUserSessionKey:
					case kAuthNTSessionKey:
					case kAuthNTLMv2WithSessionKey:
					case kAuthSMBWorkstationCredentialSessionKey:
					case kAuthNTSetWorkstationPasswd:
					case kAuthEncryptToUser:
					case kAuthDecrypt:
					case kAuthSetLMHash:
					case kAuthNTLMv2SessionKey:
						*inOutHasValidAuth = inContext->last.methodCanSetPassword;
						break;
						
					default:
						*inOutHasValidAuth = false;
				}
			}
			else
			if ( inContext->nao.successfulAuth && strcmp( strippedUserName, inContext->nao.username ) == 0 )
			{
				// if the name is a match for the saved authentication (but
				// not the last one), then the state needs to be reset
				memcpy( inContext->last.username, inContext->nao.username, kMaxUserNameLength + 1 );
				
				if ( inContext->last.password != NULL ) {
					bzero( inContext->last.password, inContext->last.passwordLen );
					free( inContext->last.password );
					inContext->last.password = NULL;
					inContext->last.passwordLen = 0;
				}
				
				inContext->last.password = (char *) malloc( inContext->nao.passwordLen + 1 );
				Throw_NULL( inContext->last.password, eMemoryError );
				
				memcpy( inContext->last.password, inContext->nao.password, inContext->nao.passwordLen );
				inContext->last.password[inContext->nao.passwordLen] = '\0';
				
				inContext->last.passwordLen = inContext->nao.passwordLen;
			}
		}
	}
	catch( SInt32 error )
	{
		siResult = error;
	}
	
	if ( strippedUserName != NULL )
		free( strippedUserName );
	
	return siResult;
}


// ---------------------------------------------------------------------------
//	* PackStepBuffer
//
//	Loads the return buffer in standard format ( 4 byte length + data ... ).
//	The <inUseBuffPlus4> parameter will strip "+OK " from arg1.
// ---------------------------------------------------------------------------

SInt32
CPSPlugIn::PackStepBuffer( const char *inArg1, bool inUseBuffPlus4, const char *inArg2, const char *inArg3, const char *inArg4, tDataBufferPtr inOutDataBuffer )
{
	unsigned long bufferLength = 0;
	unsigned long lengthOfArg[4] = { 0, 0, 0, 0 };
	const char *arg[4] = { inArg1, inArg2, inArg3, inArg4 };
	int argIndex;
	
	if ( inArg1 == NULL )
		return eParameterError;
	
	if ( inUseBuffPlus4 )
	{
		if ( strncmp( inArg1, "+OK ", 4 ) != 0 )
			return eDSAuthFailed;
		
		arg[0] = inArg1 + 4;
	}
	
	for ( argIndex = 0; argIndex < 4 && arg[argIndex] != NULL; argIndex++ )
	{
		lengthOfArg[argIndex] = strlen( arg[argIndex] );
		bufferLength = 4 + lengthOfArg[argIndex];
	}
	
	if ( bufferLength > inOutDataBuffer->fBufferSize )
		return eDSBufferTooSmall;
	
	inOutDataBuffer->fBufferLength = 0;
	
	for ( argIndex = 0; argIndex < 4 && arg[argIndex] != NULL; argIndex++ )
	{
		// put a 4-byte length in the buffer
		memcpy( inOutDataBuffer->fBufferData + inOutDataBuffer->fBufferLength, &(lengthOfArg[argIndex]), 4 );
		inOutDataBuffer->fBufferLength += 4;
		
		// put the data in the buffer
		memcpy( inOutDataBuffer->fBufferData + inOutDataBuffer->fBufferLength, arg[argIndex], lengthOfArg[argIndex] );
		inOutDataBuffer->fBufferLength += lengthOfArg[argIndex];
	}
	
	return eDSNoErr;
}


// ---------------------------------------------------------------------------
//	* UnpackUsernameAndPassword
//
// ---------------------------------------------------------------------------

SInt32
CPSPlugIn::UnpackUsernameAndPassword(
    sPSContextData *inContext,
    UInt32 uiAuthMethod,
    tDataBufferPtr inAuthBuf,
    char **outUserName,
    char **outPassword,
    UInt32 *outPasswordLen,
    char **outChallenge )
{
    SInt32					siResult		= eDSNoErr;
    unsigned char			*challenge		= NULL;
    unsigned char			*digest			= NULL;
    char					*method			= NULL;
	char					*domain			= NULL;
	char					*user			= NULL;
    UInt32					len				= 0;
					
    // sanity
    if ( outUserName == NULL || outPassword == NULL || outPasswordLen == NULL || outChallenge == NULL )
        return eParameterError;
    
    // init vars
    *outUserName = NULL;
    *outPassword = NULL;
    *outPasswordLen = 0;
    *outChallenge = NULL;
    
    try
    {
        switch (uiAuthMethod)
        {
			case kAuthPull:
			case kAuthPush:
			case kAuthProcessNoReply:
			case kAuthGetStats:
			case kAuthGetChangeList:
				// these methods do not have user names
				return eDSNoErr;
				break;
			
            case kAuthAPOP:
				siResult = Get2StringsFromAuthBuffer( inAuthBuf, outUserName, (char **)&challenge );
                if ( siResult == eDSNoErr )
                    siResult = GetStringFromAuthBuffer( inAuthBuf, 3, (char **)&digest );
                if ( siResult == eDSNoErr )
                {
                    if ( challenge == NULL || digest == NULL )
                    	throw( (SInt32)eDSAuthParameterError );
                    
                    long challengeLen = strlen((char *)challenge);
                    long digestLen = strlen((char *)digest);
                    
                    if ( challengeLen > 0 && digestLen > 0 )
                    {
                        *outPasswordLen = challengeLen + 1 + digestLen;
                        *outPassword = (char *) malloc( *outPasswordLen + 1 );
                        Throw_NULL( (*outPassword), eMemoryAllocError );
                        
                        strcpy( *outPassword, (char *)challenge );
                        strcat( *outPassword, " " );
                        strcat( *outPassword, (char *)digest );
                    }
                }
                break;
            
			case kAuthDIGEST_MD5_Reauth:
				siResult = GetStringFromAuthBuffer( inAuthBuf, 1, outUserName );
                if ( siResult == eDSNoErr )
					siResult = GetDataFromAuthBuffer( inAuthBuf, 2, &digest, &len );
				if ( siResult == eDSNoErr && digest != NULL )
                {
					*outPassword = (char *) malloc( len + 1 );
					Throw_NULL( (*outPassword), eMemoryAllocError );
					
					// put a leading null to tell the DIGEST-MD5 plug-in we're sending
					// a hash.
					**outPassword = '\0';
					memcpy( (*outPassword) + 1, digest, len );
					*outPasswordLen = len + 1;
                }
                break;
				
			case kAuthDIGEST_MD5:
				siResult = Get2StringsFromAuthBuffer( inAuthBuf, outUserName, (char **)&challenge );
				if ( siResult == eDSNoErr )
					siResult = GetDataFromAuthBuffer( inAuthBuf, 3, &digest, &len );
				if ( siResult == eDSNoErr )
					siResult = GetStringFromAuthBuffer( inAuthBuf, 4, &method );
				if ( siResult == eDSNoErr && digest != NULL && method != NULL )
				{
					*outPassword = (char *) malloc( len + 1 );
					Throw_NULL( (*outPassword), eMemoryAllocError );
					
					// put a leading null to tell the WEBDAV-DIGEST plug-in we're sending
					// a hash.
					**outPassword = '\0';
					memcpy( (*outPassword) + 1, digest, len );
					*outPasswordLen = len + 1;
					
					*outChallenge = (char *) malloc( strlen((char *)challenge) + sizeof(kPWSPlugInDigestMethodStr) + strlen(method) + 1 );
					strcpy( *outChallenge, (char *)challenge );
					strcat( *outChallenge, kPWSPlugInDigestMethodStr );
					strcat( *outChallenge, method );
					strcat( *outChallenge, "\"" );
				}
                break;
			
			case kAuthMSCHAP2:
				siResult = Get2StringsFromAuthBuffer( inAuthBuf, outUserName, (char **)&challenge );
				if ( siResult == eDSNoErr )
					siResult = GetStringFromAuthBuffer( inAuthBuf, 3, &method );
				if ( siResult == eDSNoErr )
					siResult = GetDataFromAuthBuffer( inAuthBuf, 4, &digest, &len );
				if ( siResult == eDSNoErr )
					siResult = GetStringFromAuthBuffer( inAuthBuf, 5, &user );
				if ( siResult == eDSNoErr && challenge != NULL && digest != NULL && method != NULL && len == 24 && user != NULL )
				{
					*outPassword = (char *) calloc( 1, 65 + strlen(user) + 1 );
					Throw_NULL( (*outPassword), eMemoryAllocError );
					
					memcpy( (*outPassword), challenge, 16 );
					memcpy( (*outPassword) + 16, method, 16 );
					memcpy( (*outPassword) + 40, digest, len );
					strcpy( (*outPassword) + 65, user );
					*outPasswordLen = 65 + strlen( user );
				}
                break;
			
			case kAuthNTLMv2:
				siResult = GetStringFromAuthBuffer( inAuthBuf, 1, outUserName );
				if ( siResult == eDSNoErr )
				{
					UInt32 challengeLen = 0;
					long userLen = 0;
					long domainLen = 0;
					
					siResult = GetDataFromAuthBuffer( inAuthBuf, 2, &challenge, &challengeLen );
					if ( siResult == eDSNoErr )
						siResult = GetDataFromAuthBuffer( inAuthBuf, 3, &digest, &len );
					if ( siResult == eDSNoErr )
						siResult = GetStringFromAuthBuffer( inAuthBuf, 4, &user );
					if ( siResult == eDSNoErr )
						siResult = GetStringFromAuthBuffer( inAuthBuf, 5, &domain );
					
					// blob length is variable, but must contain at least:
					// [digest (16 bytes) + 0x01010000 (4 bytes) + 0x00000000 (4 bytes) + Timestamp (8 bytes) +
					//  client challenge (8 bytes) + unknown (4 bytes)]
					// LMv2 blobs are only 8 bytes.
					if ( siResult == eDSNoErr && challenge != NULL && digest != NULL && len >= 24 && user != NULL && domain != NULL )
					{
						userLen = strlen( user );
						domainLen = strlen( domain );
						*outPassword = (char *) calloc( 1, userLen + 1 + domainLen + 1 + challengeLen + len + 1 );
						Throw_NULL( (*outPassword), eMemoryAllocError );
						
						strcpy( (*outPassword), user );
						strcpy( (*outPassword) + userLen + 1, domain );
						memcpy( (*outPassword) + userLen + 1 + domainLen + 1, challenge, challengeLen );
						memcpy( (*outPassword) + userLen + 1 + domainLen + 1 + challengeLen, digest, len );
						*outPasswordLen = userLen + 1 + domainLen + 1 + challengeLen + len;
					}
				}
				break;
				
			case kAuthCRAM_MD5:
				siResult = Get2StringsFromAuthBuffer( inAuthBuf, outUserName, outChallenge );
                if ( siResult == eDSNoErr )
                    siResult = GetDataFromAuthBuffer( inAuthBuf, 3, &digest, &len );
                if ( siResult == eDSNoErr && digest != NULL )
                {
					*outPassword = (char *) malloc( len + 1 );
					Throw_NULL( (*outPassword), eMemoryAllocError );
					
					// put a leading null to tell the CRAM-MD5 plug-in we're sending
					// a hash.
					**outPassword = '\0';
					memcpy( (*outPassword) + 1, digest, len );
					*outPasswordLen = len + 1;
                }
                break;
            				
            case kAuthSMB_NT_Key:
            case kAuthSMB_LM_Key:
                siResult = GetStringFromAuthBuffer( inAuthBuf, 1, outUserName );
                if ( siResult == eDSNoErr )
                {
                    *outPassword = (char *)malloc(32);
                    Throw_NULL( (*outPassword), eMemoryAllocError );
                    
                    *outPasswordLen = 32;
                    
                    siResult = GetDataFromAuthBuffer( inAuthBuf, 2, &challenge, &len );
                    if ( siResult != eDSNoErr || challenge == NULL || len != 8 )
                        throw( (SInt32)eDSInvalidBuffFormat );
                    
                    siResult = GetDataFromAuthBuffer( inAuthBuf, 3, &digest, &len );
                    if ( siResult != eDSNoErr || digest == NULL || len != 24 )
                        throw( (SInt32)eDSInvalidBuffFormat );
                    
                    memcpy( *outPassword, challenge, 8 );
                    memcpy( (*outPassword) + 8, digest, 24 );
                    
                    free( challenge );
                    challenge = NULL;
                    
                    free( digest );
                    digest = NULL;
                }
                break;
                
            case kAuth2WayRandom:
                // for 2way random the first buffer is the username
                if ( inAuthBuf->fBufferLength > inAuthBuf->fBufferSize )
                    throw( (SInt32)eDSInvalidBuffFormat );
                
                *outUserName = (char*)calloc( inAuthBuf->fBufferLength + 1, 1 );
                strlcpy( *outUserName, inAuthBuf->fBufferData, inAuthBuf->fBufferLength + 1 );
                break;
			
			case kAuth2WayRandomChangePass:
				siResult = GetStringFromAuthBuffer( inAuthBuf, 1, outUserName );
                if ( siResult == eDSNoErr )
				{
					char *tempPWStr = NULL;
					siResult = GetStringFromAuthBuffer( inAuthBuf, 2, &tempPWStr );
					if ( siResult == eDSNoErr && tempPWStr != NULL && strlen(tempPWStr) == 8 )
					{
						*outPasswordLen = 16;
						*outPassword = (char *)malloc(16);
						memcpy( *outPassword, tempPWStr, 8 );
						free( tempPWStr );
						
						siResult = GetStringFromAuthBuffer( inAuthBuf, 3, &tempPWStr );
						if ( siResult == eDSNoErr && tempPWStr != NULL && strlen(tempPWStr) == 8 )
						{
							memcpy( *outPassword + 8, tempPWStr, 8 );
							free( tempPWStr );
						}
					}
                }
				break;
				
            case kAuthSetPasswd:
				siResult = UnpackUsernameAndPasswordAtOffset( inAuthBuf, 3, outUserName, outPassword, outPasswordLen );
                break;
            
			case kAuthNTSessionKey:
				siResult = UnpackUsernameAndPasswordAtOffset( inAuthBuf, 4, outUserName, outPassword, outPasswordLen );
                break;
			
			case kAuthNTLMv2WithSessionKey:
				siResult = UnpackUsernameAndPasswordAtOffset( inAuthBuf, 6, outUserName, outPassword, outPasswordLen );
				break;
			
            case kAuthSetPasswdAsRoot:
			case kAuthSetComputerAcctPasswdAsRoot:
			case kAuthSetPolicyAsRoot:
            case kAuthSMB_NTUserSessionKey:
            case kAuthSMBWorkstationCredentialSessionKey:
            case kAuthNTSetWorkstationPasswd:
			case kAuthVPN_PPTPMasterKeys:
			case kAuthEncryptToUser:
			case kAuthDecrypt:
			case kAuthMSLMCHAP2ChangePasswd:
			case kAuthSetLMHash:
			case kAuthNTLMv2SessionKey:
                // uses current credentials
                if ( inContext->nao.successfulAuth && inContext->nao.password != NULL )
                {
                    long pwLen;
                    
                    *outUserName = (char *)malloc(kUserIDLength + 1);
                    strlcpy(*outUserName, inContext->nao.username, kUserIDLength + 1);
                    
                    pwLen = strlen(inContext->nao.password);
                    *outPassword = (char *)malloc(pwLen + 1);
                    strncpy(*outPassword, inContext->nao.password, pwLen + 1);
                    *outPasswordLen = pwLen;   
                    siResult = eDSNoErr;
                }
                else
                {
					siResult = eDSNotAuthorized;
                }
                break;
            
            default:
				siResult = UnpackUsernameAndPasswordDefault( inAuthBuf, outUserName, outPassword, outPasswordLen );
		}
    }
    
    catch ( SInt32 error )
    {
        siResult = error;
    }
    
    catch (...)
    {
        DEBUGLOG( "PasswordServer PlugIn: uncasted throw" );
        siResult = eDSAuthFailed;
    }
    
    if ( challenge != NULL ) {
        free( challenge );
        challenge = NULL;
    }
    if ( digest != NULL ) {
        free( digest );
        digest = NULL;
    }
    if ( method != NULL ) {
		free( method );
		method = NULL;
	}
	if ( domain != NULL ) {
		free( domain );
		domain = NULL;
	}
	if ( user != NULL ) {
		free( user );
		user = NULL;
	}
	
    // user name is a required value
    // kAuth2WayRandom is multi-pass and only has a username for pass 1
    if ( siResult == eDSNoErr && *outUserName == NULL && uiAuthMethod != kAuth2WayRandom )
        siResult = eDSUserUnknown;
    
    return siResult;
}


SInt32
CPSPlugIn::UnpackUsernameAndPasswordDefault(
	tDataBufferPtr inAuthBuf,
	char **outUserName,
	char **outPassword,
	UInt32 *outPasswordLen )
{
    SInt32 siResult = eDSNoErr;
	
	siResult = Get2StringsFromAuthBuffer( inAuthBuf, outUserName, outPassword );
	if ( siResult == eDSNoErr && *outPassword != NULL )
		*outPasswordLen = strlen( *outPassword );
	
	if ( *outPassword == NULL || *outPasswordLen == 0 )
	{
		if ( *outPassword != NULL )
			free( *outPassword );
		*outPasswordLen = strlen( kEmptyPasswordAltStr );
		*outPassword = strdup( kEmptyPasswordAltStr );
	}
	
	return siResult;
}

														
SInt32
CPSPlugIn::UnpackUsernameAndPasswordAtOffset(
	tDataBufferPtr inAuthBuf,
	UInt32 inUserItem,
	char **outUserName,
	char **outPassword,
	UInt32 *outPasswordLen )
{
    SInt32 siResult = eDSNoErr;
	
	siResult = GetStringFromAuthBuffer( inAuthBuf, inUserItem, outUserName );
	if ( siResult == eDSNoErr )
		siResult = GetStringFromAuthBuffer( inAuthBuf, inUserItem + 1, outPassword );
	if ( siResult == eDSNoErr && *outPassword != NULL )
		*outPasswordLen = strlen( *outPassword );
	
	if ( *outPassword == NULL || *outPasswordLen == 0 )
	{
		if ( *outPassword != NULL )
			free( *outPassword );
		*outPasswordLen = strlen( kEmptyPasswordAltStr );
		*outPassword = strdup( kEmptyPasswordAltStr );
	}
	
	return siResult;
}


// ---------------------------------------------------------------------------
//	* GetAuthMethodConstant
//
//	Returns a constant that represents a DirectoryServices auth method.
//	If the auth method is a native type, this function also returns
//	the SASL mech name in outNativeAuthMethodSASLName.
// ---------------------------------------------------------------------------

SInt32
CPSPlugIn::GetAuthMethodConstant(
    sPSContextData *inContext,
    tDataNode *inData,
    UInt32 *outAuthMethod,
	char *outNativeAuthMethodSASLName )
{
	SInt32			siResult		= eDSNoErr;
	char		   *p				= NULL;
    SInt32			prefixLen;
    
	if ( inData == NULL )
	{
		*outAuthMethod = kAuthUnknownMethod;
		return( eDSAuthParameterError );
	}

    if ( outNativeAuthMethodSASLName != NULL )
        *outNativeAuthMethodSASLName = '\0';
    
	p = (char *)inData->fBufferData;

	DEBUGLOG( "PasswordServer PlugIn: Attempting use of authentication method %s", p );

    prefixLen = strlen(kDSNativeAuthMethodPrefix);
    if ( ::strncmp( p, kDSNativeAuthMethodPrefix, prefixLen ) == 0 )
    {        
        *outAuthMethod = kAuthUnknownMethod;
		siResult = eDSAuthMethodNotSupported;
        
        p += prefixLen;
        
        // check for GetIDByName
        if ( strcmp( p, "dsAuthGetIDByName" ) == 0 )
        {
            *outAuthMethod = kAuthGetIDByName;
            return eDSNoErr;
        }
		else
		if ( strcmp( p, "dsAuthGetDisabledUsers" ) == 0 )
        {
            *outAuthMethod = kAuthGetDisabledUsers;
            return eDSNoErr;
        }
        else
		if ( strcmp( p, "dsAuthSyncSetupReplica" ) == 0 )
        {
            *outAuthMethod = kAuthSyncSetupReplica;
            return eDSNoErr;
        }
		else
		if ( strcmp( p, "dsAuthListReplicas" ) == 0 )
        {
            *outAuthMethod = kAuthListReplicas;
            return eDSNoErr;
        }
		else
		if ( strcmp( p, "dsAuthGetStats" ) == 0 )
		{
			*outAuthMethod = kAuthGetStats;
			return eDSNoErr;
		}
		else
		if ( strcmp( p, "dsAuthGetChangeList" ) == 0 )
		{
			*outAuthMethod = kAuthGetChangeList;
			return eDSNoErr;
		}
		else
		if ( strcmp( p, "dsAuthPull" ) == 0 )
        {
            *outAuthMethod = kAuthPull;
            return eDSNoErr;
        }
		else
		if ( strcmp( p, "dsAuthPush" ) == 0 )
        {
            *outAuthMethod = kAuthPush;
            return eDSNoErr;
        }
		else
		if ( strcmp( p, "dsAuthProcessNoReply" ) == 0 )
        {
            *outAuthMethod = kAuthProcessNoReply;
            return eDSNoErr;
        }
		
		/*
		 * we don't get the mech list up-front anymore
		 */
		 /*
        for ( SInt32 index = inContext->mechCount - 1; index >= 0; index-- )
        {
            if ( strcmp( p, inContext->mech[index].method ) == 0 )
            {
                if ( outNativeAuthMethodSASLName != NULL )
                    strcpy( outNativeAuthMethodSASLName, inContext->mech[index].method );
                
                *outAuthMethod = kAuthNativeMethod;
                siResult = eDSNoErr;
                break;
            }
        }
		*/
    }
    else
	{
		siResult = dsGetAuthMethodEnumValue( inData, outAuthMethod );
		if ( siResult == eDSAuthMethodNotSupported )
		{
			siResult = eDSNoErr;
			
			if ( strcmp( p, "dsAuthMethodStandard:dsAuthNodeDIGEST-MD5-Reauth" ) == 0 )
			{
				*outAuthMethod = kAuthDIGEST_MD5_Reauth;
			}
			else
			if ( strcmp( p, kDSStdAuthSMBNTv2UserSessionKey ) == 0 ||
				 strcmp( p, "dsAuthMethodStandard:dsAuthNodeNTLMv2SessionKey" ) == 0 )
			{
				*outAuthMethod = kAuthNTLMv2SessionKey;
			}
			else
			if ( strcmp( p, "dsAuthMethodStandard:dsAuthMSLMCHAP2ChangePasswd" ) == 0 )
			{
				*outAuthMethod = kAuthMSLMCHAP2ChangePasswd;
			}
			else
			if ( strcmp( p, "dsAuthMethodStandard:dsAuthEncryptToUser" ) == 0 )
			{
				*outAuthMethod = kAuthEncryptToUser;
			}
			else
			if ( strcmp( p, "dsAuthMethodStandard:dsAuthDecrypt" ) == 0 )
			{
				*outAuthMethod = kAuthDecrypt;
			}
			else
			if ( ::strcmp( p, "dsAuthMethodStandard:dsAuthNodePPS" ) == 0 )
			{
				*outAuthMethod = kAuthPPS;
			}
			else
			{
				*outAuthMethod = kAuthUnknownMethod;
				siResult = eDSAuthMethodNotSupported;
			}
		}
	}
	
	return( siResult );

} // GetAuthMethodConstant


// ---------------------------------------------------------------------------
//	* RequiresSASLAuthentication
// ---------------------------------------------------------------------------

bool CPSPlugIn::RequiresSASLAuthentication(	UInt32 inAuthMethodConstant )
{
	switch( inAuthMethodConstant )
	{
		case kAuthGetPolicy:
		case kAuthGetGlobalPolicy:
		case kAuthGetIDByName:
		case kAuthGetDisabledUsers:
		case kAuth2WayRandomChangePass:
		case kAuthListReplicas:
		case kAuthPull:
		case kAuthPush:
		case kAuthProcessNoReply:
		case kAuthGetEffectivePolicy:
		case kAuthGetKerberosPrincipal:
		case kAuthGetStats:
		case kAuthGetChangeList:
			return false;
		
		default:
			return true;
	}
	
	return true;
}

	
// ---------------------------------------------------------------------------
//	* GetAuthMethodSASLName
//
//	Returns the name of a SASL mechanism for
//	standard (kDSStdAuthMethodPrefix) auth mehthods 
// ---------------------------------------------------------------------------

SInt32
CPSPlugIn::GetAuthMethodSASLName ( sPSContextData *inContext, UInt32 inAuthMethodConstant, bool inAuthOnly, char *outMechName,
	bool *outMethodCanSetPassword )
{
    SInt32 result = eDSNoErr;
    bool isNewEnough = false;
	
    if ( outMechName == NULL || outMethodCanSetPassword == NULL )
        return -1;
    *outMechName = '\0';
	*outMethodCanSetPassword = false;
	
    switch ( inAuthMethodConstant )
    {
        case kAuthClearText:
			if ( inAuthOnly )
			{
				strcpy( outMechName, "PLAIN " );
				strcat( outMechName, fSASLMechPriority );
			}
			else
			{
				strcpy( outMechName, kDHX_SASL_Name );
				*outMethodCanSetPassword = true;
			}
            break;
            
        case kAuthCrypt:
            strcpy( outMechName, "CRYPT" );
            break;

        case kAuthSetPasswd:
        case kAuthChangePasswd:
        case kAuthSetPasswdAsRoot:
		case kAuthSetComputerAcctPasswdAsRoot:
        case kAuthSMB_NTUserSessionKey:
        case kAuthSMBWorkstationCredentialSessionKey:
        case kAuthNTSetWorkstationPasswd:
		case kAuthVPN_PPTPMasterKeys:
		case kAuthEncryptToUser:
		case kAuthDecrypt:
		case kAuthSetLMHash:
			strcpy( outMechName, kDHX_SASL_Name );
			*outMethodCanSetPassword = true;
            break;
					
		case kAuthAPOP:
            strcpy( outMechName, "APOP" );
            break;
            
        case kAuth2WayRandom:
            strcpy( outMechName, "TWOWAYRANDOM" );
            break;
        
        case kAuthNativeClearTextOK:
            // If <inAuthOnly> == false, then a "kDSStdSetPasswdAsRoot" auth method
            // could be called later and will require DHX
            strcpy( outMechName, inAuthOnly ? fSASLMechPriority : kDHX_SASL_Name );
            strcat( outMechName, " PLAIN" );
			if ( ! inAuthOnly )
				*outMethodCanSetPassword = true;
            break;
        
        case kAuthNativeNoClearText:
            // If <inAuthOnly> == false, then a "kDSStdSetPasswdAsRoot" auth method
            // could be called later and will require DHX
            strcpy( outMechName, inAuthOnly ? fSASLMechPriority : kDHX_SASL_Name );
            if ( ! inAuthOnly )
				*outMethodCanSetPassword = true;
            break;
			
        case kAuthNativeRetainCredential:
			strcpy( outMechName, kDHX_SASL_Name );
            *outMethodCanSetPassword = true;
			break;
		
        case kAuthSMB_NT_Key:
            strcpy( outMechName, "SMB-NT" );
            break;
            
        case kAuthSMB_LM_Key:
            strcpy( outMechName, "SMB-LAN-MANAGER" );
            break;
        
        case kAuthDIGEST_MD5:
		case kAuthDIGEST_MD5_Reauth:
            strcpy( outMechName, "WEBDAV-DIGEST" );
            break;
        
		case kAuthMSCHAP2:
			strcpy( outMechName, "MS-CHAPv2" );
            break;
		
		case kAuthNTLMv2:
			strcpy( outMechName, "SMB-NTLMv2" );
			break;
		
        case kAuthCRAM_MD5:
            strcpy( outMechName, "CRAM-MD5" );
            break;
		
		case kAuthPPS:
            strcpy( outMechName, "PPS" );
            break;
		
        case kAuthGetPolicy:
        case kAuthSetPolicy:
		case kAuthSetPolicyAsRoot:
        case kAuthGetGlobalPolicy:
        case kAuthSetGlobalPolicy:
        case kAuthGetUserName:
        case kAuthSetUserName:
        case kAuthGetUserData:
        case kAuthSetUserData:
        case kAuthDeleteUser:
		case kAuthListReplicas:
		case kAuthGetEffectivePolicy:
		case kAuthGetKerberosPrincipal:
		case kAuthMSLMCHAP2ChangePasswd:
		case kAuthGetStats:
		case kAuthGetChangeList:
            strcpy( outMechName, fSASLMechPriority );
            break;
        
        case kAuthNewUser:
        case kAuthNewUserWithPolicy:
        case kAuthNewComputer:
        case kAuthSyncSetupReplica:
			strcpy( outMechName, kDHX_SASL_Name );
			*outMethodCanSetPassword = true;
            break;
        
		case kAuthNTSessionKey:
		case kAuthNTLMv2SessionKey:
		case kAuthNTLMv2WithSessionKey:
			isNewEnough = CheckServerVersionMin( inContext->serverVers, 10, 4, 5, 0 );			
			strcpy( outMechName, isNewEnough ? "DIGEST-MD5" : kDHX_SASL_Name );
            break;
				
        case kAuthUnknownMethod:
        case kAuthNativeMethod:
        default:
            result = eDSAuthMethodNotSupported;
    }
    
    return result;
}


//------------------------------------------------------------------------------------
//	* GetAuthMethodFromSASLName
//------------------------------------------------------------------------------------

void
CPSPlugIn::GetAuthMethodFromSASLName( const char *inMechName, char *outDSType )
{
	if ( outDSType == NULL )
		return;
	
	*outDSType = '\0';
	
	if ( inMechName != NULL ) {
		for ( int index = 0; gMethodMap[index].saslName != NULL; index++ ) {
			if ( strcmp(inMechName, gMethodMap[index].saslName) == 0 ) {
				strcpy( outDSType, gMethodMap[index].odName );
				break;
			}
		}
	}
}


//------------------------------------------------------------------------------------
//	* DoSASLNew
//------------------------------------------------------------------------------------
SInt32
CPSPlugIn::DoSASLNew( sPSContextData *inContext, sPSContinueData *inContinue )
{
	SInt32 ret = eDSNoErr;
	const char *localaddr = NULL;
	const char *remoteaddr = NULL;
	
	if ( inContext == NULL )
		return eDSBadContextData;
	if ( inContinue == NULL )
		return eDSAuthContinueDataBad;
	
	// clean up the old conn
	if ( inContext->conn != NULL )
	{
		gSASLMutex->Wait();
		sasl_dispose(&inContext->conn);
		gSASLMutex->Signal();
		
		inContext->conn = NULL;
	}
	
	// callbacks we support
	inContext->callbacks[0].id = SASL_CB_GETREALM;
	inContext->callbacks[0].proc = (sasl_cbproc *)&getrealm;
	inContext->callbacks[0].context = inContext;
	
	inContext->callbacks[1].id = SASL_CB_USER;
	inContext->callbacks[1].proc = (sasl_cbproc *)&simple;
	inContext->callbacks[1].context = inContinue;
	
	inContext->callbacks[2].id = SASL_CB_AUTHNAME;
	inContext->callbacks[2].proc = (sasl_cbproc *)&simple;
	inContext->callbacks[2].context = inContinue;
	
	inContext->callbacks[3].id = SASL_CB_PASS;
	inContext->callbacks[3].proc = (sasl_cbproc *)&getsecret;
	inContext->callbacks[3].context = inContinue;
	
	inContext->callbacks[4].id = SASL_CB_LIST_END;
	inContext->callbacks[4].proc = NULL;
	inContext->callbacks[4].context = NULL;
	
	localaddr = (inContext->localaddr[0]) ? inContext->localaddr : "127.0.0.1;3659";
	remoteaddr = (inContext->remoteaddr[0]) ? inContext->remoteaddr : "127.0.0.1;3659";
	
	gSASLMutex->Wait();
	ret = sasl_client_new( "rcmd",
						inContext->psName,
						localaddr,
						remoteaddr,
						inContext->callbacks,
						0,
						&inContext->conn);
	gSASLMutex->Signal();
	
	return ret;
}


//------------------------------------------------------------------------------------
//	* DoSASLAuth
//------------------------------------------------------------------------------------

SInt32
CPSPlugIn::DoSASLAuth(
    sPSContextData *inContext,
    char *userName,
    const char *password,
    long inPasswordLen,
    const char *inChallenge,
    const char *inMechName,
	sDoDirNodeAuth *inData,
	char **outStepData )
{
	SInt32				siResult			= eDSAuthFailed;
	sPSContinueData		*pContinue			= NULL;
	char				*tptr				= NULL;
	char				*commandBuf			= NULL;
	UInt32				commandBufLen		= 0;
	
    DEBUGLOG( "CPSPlugIn::Attempting SASL Authentication");
	try
	{
		Throw_NULL( inContext, eDSBadContextData );
        Throw_NULL( password, eParameterError );
        Throw_NULL( inData, eParameterError );
        
		pContinue = (sPSContinueData *) inData->fIOContinueData;
		Throw_NULL( pContinue, eDSAuthContinueDataBad );
        
		if ( outStepData != NULL )
			*outStepData = NULL;
		
		// need username length, password length, and username must be at least 1 character
		// yes do it here
        {
            char buf[4096];
            const char *data;
            char dataBuf[4096];
			unsigned long binLen = 0;
            const char *chosenmech = NULL;
            unsigned int len = 0;
            int r;
            PWServerError serverResult;
        	sasl_security_properties_t secprops = {0,65535,4096,0,NULL,NULL};
            
            // attach the username and password to the sasl connection's context
            // set these before calling sasl_client_start
            if ( userName != NULL )
            {
                long userNameLen;
                char *userNameEnd = strchr( userName, ',' );
                
                if ( userNameEnd != NULL )
                {
                    userNameLen = userNameEnd - userName;
                    if ( userNameLen >= kMaxUserNameLength )
                        throw( (SInt32)eDSAuthInvalidUserName );
                    
                    strlcpy(inContext->last.username, userName, userNameLen + 1 );
                }
                else
                {
                    strlcpy( inContext->last.username, userName, kMaxUserNameLength );
                }
				
				strcpy( pContinue->fUsername, inContext->last.username );
            }
            
            // if not enough space, toss
			if ( inContext->last.password != NULL )
			{
				bzero( inContext->last.password, inContext->last.passwordLen );
				if ( inPasswordLen > inContext->last.passwordLen )
				{
					free( inContext->last.password );
					inContext->last.password = NULL;
					inContext->last.passwordLen = 0;
				}
			}
            
			// if first allocation, or not enough space in the old one, allocate
			if ( inContext->last.password == NULL )
			{
				inContext->last.password = (char *) malloc( inPasswordLen + 1 );
				Throw_NULL( inContext->last.password, eMemoryError );
			}
			
            memcpy( inContext->last.password, password, inPasswordLen );
            inContext->last.password[inPasswordLen] = '\0';
            inContext->last.passwordLen = inPasswordLen;
            
			// now, make the struct ptr for this sasl session
			pContinue->fSASLSecret = (sasl_secret_t *) malloc(sizeof(sasl_secret_t) + inPasswordLen + 1);
			Throw_NULL( pContinue->fSASLSecret, eMemoryError );
			
			pContinue->fSASLSecret->len = inPasswordLen;
			memcpy( pContinue->fSASLSecret->data, password, inPasswordLen );
			
            r = DoSASLNew( inContext, pContinue );
            if ( r != SASL_OK || inContext->conn == NULL ) {
                DEBUGLOG( "sasl_client_new failed, err=%d.", r);
                throw( SASLErrToDirServiceError(r) );
            }
            
            gSASLMutex->Wait();
            r = sasl_setprop(inContext->conn, SASL_SEC_PROPS, &secprops);
            r = sasl_client_start( inContext->conn, inMechName, NULL, &data, &len, &chosenmech ); 
            gSASLMutex->Signal();
            
#if DEBUG
			{
				char *tmpData = (char *)malloc(len+1);
				memcpy(tmpData, data, len);
				tmpData[len] = '\0';
				DEBUGLOG( "sasl_client_start=%d, start data=%s", r, tmpData);
				free(tmpData);
			}
#endif
			
            //DEBUGLOG( "chosenmech=%s, datalen=%u", chosenmech, len);
            
            if ( r != SASL_OK && r != SASL_CONTINUE ) {
                DEBUGLOG( "starting SASL negotiation, err=%d", r);
                throw( SASLErrToDirServiceError(r) );
            }
            
            // send the auth method
            dataBuf[0] = 0;
            
            if ( inChallenge != NULL )
            {
                // for CRAM-MD5 and potentially DIGEST-MD5, we can attach the nonce to the
                // initial data.
				if ( strcmp(chosenmech, "WEBDAV-DIGEST") == 0 )
				{
					strcpy(dataBuf, "replay ");
					ConvertBinaryToHex( (const unsigned char *)inChallenge, strlen(inChallenge), dataBuf+7 );
					len = strlen(dataBuf);
				}
				else
				{
					ConvertBinaryToHex( (const unsigned char *)inChallenge, strlen(inChallenge), dataBuf );
					len = strlen(dataBuf);
				}
            }
            else
			if ( len > 0 )
            	ConvertBinaryToHex( (const unsigned char *)data, len, dataBuf );
            
            // set a user
			StripRSAKey( userName );
			
			// build the command
			commandBufLen = sizeof("USER  INFO  AUTH  ") + strlen(userName) + SASL_MECHNAMEMAX + len*2;
			if ( inContext->serviceInfoStr != NULL )
				commandBufLen += strlen( inContext->serviceInfoStr );
			
			commandBuf = (char *) malloc( commandBufLen );
			if ( commandBuf == NULL )
				throw( (SInt32)eMemoryError );
			
			snprintf( commandBuf, commandBufLen, "USER %s", userName );
			if ( inContext->serviceInfoStr != NULL && CheckServerVersionMin(inContext->serverVers, 10, 5, 0, 0) ) {
				strlcat( commandBuf, " INFO ", commandBufLen );
				strlcat( commandBuf, inContext->serviceInfoStr, commandBufLen );
			}
			strlcat( commandBuf, " AUTH ", commandBufLen );
			strlcat( commandBuf, chosenmech, commandBufLen );
			
			if ( len > 0 ) {
				strlcat( commandBuf, " ", commandBufLen );
				strlcat( commandBuf, dataBuf, commandBufLen );
			}
			
			serverResult = SendFlushReadWithMutex( inContext, commandBuf, NULL, NULL, buf, sizeof(buf) );
            if (serverResult.err != 0) {
                DEBUGLOG( "server returned an error, err=%d", serverResult.err);
                throw( PWSErrToDirServiceError(serverResult) );
            }
            
            sasl_chop(buf);
            len = strlen(buf);
            
			// check for old server
			if ( (len >= 3 && strncmp(buf, "+OK", 3) == 0) ||
				 (len >= 4 && strncmp(buf, "-ERR", 4) == 0) )
			{
				if ( len > 0 )
					snprintf(buf, sizeof(buf), "AUTH %s %s", chosenmech, dataBuf);
				else
					snprintf(buf, sizeof(buf), "AUTH %s", chosenmech);
				
				serverResult = SendFlushReadWithMutex( inContext, buf, NULL, NULL, buf, sizeof(buf) );
				if (serverResult.err != 0) {
					DEBUGLOG( "server returned an error, err=%d", serverResult.err);
					throw( PWSErrToDirServiceError(serverResult) );
				}
				
				sasl_chop(buf);
				len = strlen(buf);
			}
			
            while ( r == SASL_CONTINUE )
            {
                // skip the "+OK " at the begining of the response
                if ( (len >= 7 && strncmp(buf, "+AUTHOK ", 7) == 0) ||
					 (len >= 4 && strncmp(buf, "+OK ", 4) == 0) )
                {
					tptr = strchr( buf, ' ' );
                    if ( tptr != NULL )
                    {
                        ConvertHexToBinary( tptr + 1, (unsigned char *) dataBuf, &binLen );
#if DEBUG
						{
							char *tmpData = (char *)malloc(binLen+1);
							memcpy(tmpData, dataBuf, binLen);
							tmpData[binLen] = '\0';
							DEBUGLOG( "server data=%s", tmpData);
							free(tmpData);
						}
#endif
                       
                        gSASLMutex->Wait();
                        r = sasl_client_step(inContext->conn, dataBuf, binLen, NULL, &data, &len);
				
#if DEBUG
						{
							char *tmpData = (char *)malloc(len+1);
							memcpy(tmpData, data, len);
							tmpData[len] = '\0';
							DEBUGLOG( "step data=%s", tmpData);
							free(tmpData);
						}
#endif

                        gSASLMutex->Signal();
                    }
                    else
                    {
                        // we are done
                        data = NULL;
                        len = 0;
                        r = SASL_OK;
                    }
                }
                else 
                    r = SASL_FAIL;
				
                if (r != SASL_OK && r != SASL_CONTINUE) {
                    DEBUGLOG( "sasl_client_step=%d", r);
                    throw( SASLErrToDirServiceError(r) );
                }
                
                if (data && len != 0)
                {
                    //DEBUGLOG( "sending response length %d", len);
                    //DEBUGLOG( "client step data = %s", data );
                    
                    ConvertBinaryToHex( (const unsigned char *)data, len, dataBuf );
                    
                    DEBUGLOG( "AUTH2 %s", dataBuf);
					serverResult = SendFlushReadWithMutex( inContext, "AUTH2", dataBuf, NULL, buf, sizeof(buf) );
                }
                else
                if (r==SASL_CONTINUE)
                {
                    DEBUGLOG( "sending null response");
                    serverResult = SendFlushReadWithMutex( inContext, "AUTH2 ", NULL, NULL, buf, sizeof(buf) );
                }
                else
				{
                    break;
                }
				
                if ( serverResult.err != 0 ) {
                    DEBUGLOG( "server returned an error, err=%d", serverResult.err);
                    throw( PWSErrToDirServiceError(serverResult) );
                }
                
                sasl_chop(buf);
                len = strlen(buf);
                
                if ( r != SASL_CONTINUE )
                    break;
            }
            
			if ( outStepData != NULL && binLen > 0 && dataBuf != NULL && 
				 (strcmp(chosenmech, "WEBDAV-DIGEST") == 0 || strcmp(chosenmech, "MS-CHAPv2") == 0) )
			{
				*outStepData = (char *) malloc( binLen + 1 );
				if ( *outStepData == NULL )
					throw( (SInt32)eMemoryError );
				
				memcpy( *outStepData, dataBuf, binLen );
				(*outStepData)[binLen] = '\0';
				
				//DEBUGLOG( "outStepData = %s", *outStepData );
			}
			
            throw( SASLErrToDirServiceError(r) );
        }
	}
	catch ( SInt32 err )
	{
		DEBUGLOG( "PasswordServer PlugIn: SASL authentication error %l", err );
		siResult = err;
	}
	catch ( ... )
	{
		DEBUGLOG( "PasswordServer PlugIn: SASL uncasted authentication error" );
		siResult = eDSAuthFailed;
	}
	
	if ( pContinue != NULL ) {
		// No 2-pass auths handled in this method so clean up now
		gContinue->RemoveItem( pContinue );
        inData->fIOContinueData = 0;
	}
	
	DSFreeString( commandBuf );
	
	return( siResult );

} // DoSASLAuth


//------------------------------------------------------------------------------------
//	* DoSASLTwoWayRandAuth
//------------------------------------------------------------------------------------

SInt32
CPSPlugIn::DoSASLTwoWayRandAuth(
    sPSContextData *inContext,
    const char *userName,
    const char *inMechName,
    sDoDirNodeAuth *inData )
{
	SInt32			siResult			= eDSAuthFailed;
    char buf[4096];
    const char *data;
    char dataBuf[4096];
    const char *chosenmech = NULL;
    unsigned int len = 0;
    int r;
    PWServerError serverResult;
    sasl_security_properties_t secprops = {0,65535,4096,0,NULL,NULL};
	sPSContinueData *pContinue = (sPSContinueData *) inData->fIOContinueData;
    tDataBufferPtr outAuthBuff = inData->fOutAuthStepDataResponse;
    tDataBufferPtr inAuthBuff = inData->fInAuthStepData;
    
    DEBUGLOG( "CPSPlugIn::DoSASLTwoWayRandAuth");
	try
	{
		Throw_NULL( inContext, eDSBadContextData );
        Throw_NULL( inMechName, eParameterError );
        Throw_NULL( inData, eParameterError );
        Throw_NULL( inAuthBuff, eDSNullAuthStepData );
        Throw_NULL( outAuthBuff, eDSNullAuthStepDataResp );
        Throw_NULL( pContinue, eDSAuthContinueDataBad );
        
        if ( outAuthBuff->fBufferSize < 8 )
            throw( (SInt32)eDSAuthResponseBufTooSmall );
        
		// need username length, password length, and username must be at least 1 character
        // This information may not come in the first step, so check each step.
        
		DEBUGLOG( "PasswordServer PlugIn: Attempting Authentication" );
        
        if ( pContinue->fAuthPass == 0 )
        {
            // first pass contains the user name
            if ( userName != NULL )
            {
                long userNameLen;
                char *userNameEnd = strchr( userName, ',' );
                
                if ( userNameEnd != NULL )
                {
                    userNameLen = userNameEnd - userName;
                    if ( userNameLen >= kMaxUserNameLength )
                        throw( (SInt32)eDSAuthInvalidUserName );
                    
                    strlcpy(inContext->last.username, userName, userNameLen + 1 );
                }
                else
                {
                    strlcpy( inContext->last.username, userName, kMaxUserNameLength );
                }
            }
            
			r = DoSASLNew( inContext, pContinue );
			if ( r != SASL_OK || inContext->conn == NULL ) {
                DEBUGLOG( "sasl_client_new failed, err=%d.", r);
                throw( SASLErrToDirServiceError(r) );
            }
            
            r = sasl_setprop(inContext->conn, SASL_SEC_PROPS, &secprops);
            
            // set a user
            snprintf(dataBuf, sizeof(dataBuf), "USER %s\r\n", userName);
            writeToServer(inContext->serverOut, dataBuf);
            
            // flush the read buffer
            serverResult = readFromServer( inContext->fd, buf, sizeof(buf) );
            if (serverResult.err != 0) {
                DEBUGLOG( "server returned an error, err=%d", serverResult.err);
                throw( PWSErrToDirServiceError(serverResult) );
            }
            
            // send the auth method
            snprintf(buf, sizeof(buf), "AUTH %s\r\n", inMechName);
            writeToServer(inContext->serverOut, buf);
            
            // get server response
            serverResult = readFromServer(inContext->fd, buf, sizeof(buf));
            if (serverResult.err != 0) {
                DEBUGLOG( "server returned an error, err=%d", serverResult.err);
                throw( PWSErrToDirServiceError(serverResult) );
            }
            
            sasl_chop(buf);
            len = strlen(buf);
            
            // skip the "+OK " at the begining of the response
            if ( len >= 3 && strncmp( buf, "+OK", 3 ) == 0 )
            {
                if ( len > 4 )
                {
                    unsigned long binLen;
                    unsigned long num1, num2;
                    char *num2Ptr = NULL;
                    unsigned char *saveData = NULL;
                    
                    ConvertHexToBinary( buf + 4, (unsigned char *) dataBuf, &binLen );
                    dataBuf[binLen] = '\0';
                    
                    // save a copy for the next pass (do not trust the client)
                    saveData = (unsigned char *) malloc(binLen + 1);
                    Throw_NULL( saveData, eMemoryError );
                    
                    memcpy(saveData, dataBuf, binLen+1);
                    pContinue->fData = saveData;
                    pContinue->fDataLen = binLen;
                    
                    // make an out buffer
                    num2Ptr = strchr( dataBuf, ' ' );
                    
                    if ( binLen < 3 || num2Ptr == NULL )
                        throw( (SInt32)eDSInvalidBuffFormat );
                    
                    sscanf(dataBuf, "%lu", &num1);
                    sscanf(num2Ptr+1, "%lu", &num2);
                    
                    outAuthBuff->fBufferLength = 8;
                    memcpy(outAuthBuff->fBufferData, &num1, sizeof(long));
                    memcpy(outAuthBuff->fBufferData + sizeof(long), &num2, sizeof(long));
                    
                    siResult = eDSNoErr;
                }
                else
                {
                    // we're done, although it would be odd to finish here since
                    // it's a multi-pass auth.
                    data = NULL;
                    len = 0;
                    r = SASL_OK;
                }
            }
            else
            {
                r = SASL_FAIL;
            }
        }
        else
        if ( pContinue->fAuthPass == 1 )
        {
            DEBUGLOG( "inAuthBuff->fBufferLength=%l", inAuthBuff->fBufferLength);
            
            // buffer should be:
            // 8 byte DES digest
            // 8 bytes of random
            if ( inAuthBuff->fBufferLength < 16 )
                throw( (SInt32)eDSAuthInBuffFormatError );
            
            // attach the username and password to the sasl connection's context
            // set these before calling sasl_client_start
            if ( inContext->last.password != NULL )
            {
                bzero( inContext->last.password, inContext->last.passwordLen );
                free( inContext->last.password );
                inContext->last.password = NULL;
                inContext->last.passwordLen = 0;
            }
            
            inContext->last.password = (char *) malloc( inAuthBuff->fBufferLength );
            Throw_NULL( inContext->last.password, eMemoryError );
            
            memcpy( inContext->last.password, inAuthBuff->fBufferData, inAuthBuff->fBufferLength );
            inContext->last.passwordLen = inAuthBuff->fBufferLength;
            
            // start sasling
            r = sasl_client_start( inContext->conn, inMechName, NULL, &data, &len, &chosenmech ); 
            DEBUGLOG( "chosenmech=%s, datalen=%u", chosenmech, len);
            
            if ( r != SASL_OK && r != SASL_CONTINUE ) {
                DEBUGLOG( "starting SASL negotiation, err=%d", r);
                throw( SASLErrToDirServiceError(r) );
            }
            
            r = sasl_client_step(inContext->conn, (const char *)pContinue->fData, pContinue->fDataLen, NULL, &data, &len);
            
            // clean up
            if ( pContinue->fData != NULL ) {
                free( pContinue->fData );
                pContinue->fData = NULL;
            }
            pContinue->fDataLen = 0;
            
            if ( r != SASL_OK && r != SASL_CONTINUE ) {
                DEBUGLOG( "stepping SASL negotiation, err=%d", r);
                throw( SASLErrToDirServiceError(r) );
            }
            
            if (data && len != 0)
            {
                ConvertBinaryToHex( (const unsigned char *)data, len, dataBuf );
                
                DEBUGLOG( "AUTH2 %s", dataBuf);
                fprintf(inContext->serverOut, "AUTH2 %s\r\n", dataBuf );
                fflush(inContext->serverOut);
                
                // get server response
                serverResult = readFromServer(inContext->fd, buf, sizeof(buf));
                if (serverResult.err != 0) {
                    DEBUGLOG( "server returned an error, err=%d", serverResult.err);
                    throw( PWSErrToDirServiceError(serverResult) );
                }
                
                sasl_chop(buf);
                len = strlen(buf);
                
                // make an out buffer
                if ( len > 4 )
                {
                    unsigned long binLen;
                    
                    ConvertHexToBinary( buf + 4, (unsigned char *)dataBuf, &binLen );
                    if ( binLen > outAuthBuff->fBufferSize )
                        throw( (SInt32)eDSAuthResponseBufTooSmall );
                    
                    outAuthBuff->fBufferLength = binLen;
                    memcpy(outAuthBuff->fBufferData, dataBuf, binLen);
                    
                    siResult = eDSNoErr;
                }
            }
        }
        else
        {
            // too many passes
            siResult = eDSAuthFailed;
        }
	}

	catch ( SInt32 err )
	{
		DEBUGLOG( "PasswordServer PlugIn: SASL authentication error %l", err );
		siResult = err;
	}
	catch ( ... )
	{
		DEBUGLOG( "PasswordServer PlugIn: SASL uncasted authentication error" );
		siResult = eDSAuthFailed;
	}
	
    if ( pContinue->fAuthPass == 1 )
    {
        gContinue->RemoveItem( pContinue );
        inData->fIOContinueData = 0;
    }
    else
    {
        pContinue->fAuthPass++;
    }
    
	return( siResult );
}
                                                        

//------------------------------------------------------------------------------------
//	* DoSASLPPSAuth
//------------------------------------------------------------------------------------

tDirStatus
CPSPlugIn::DoSASLPPSAuth(
    sPSContextData *inContext,
    const char *inUserName,
	const char *inMechData,
	long inMechDataLen,
    sDoDirNodeAuth *inData )
{
	tDirStatus				siResult			= eDSAuthFailed;
    tDataBufferPtr			inAuthBuff			= inData->fInAuthStepData;
    tDataBufferPtr			outAuthBuff			= inData->fOutAuthStepDataResponse;
	sPSContinueData			*pContinue			= (sPSContinueData *) inData->fIOContinueData;
    PWServerError			serverResult		= {0};
    unsigned long			dataBufLen			= 0;
	char					*stepData			= NULL;
    char					buf[4096];
    char					dataBuf[4096];
	
    DEBUGLOG( "CPSPlugIn::DoSASLPPSAuth" );
	
	Return_if_NULL( inContext, eDSBadContextData );
	Return_if_NULL( inData, eParameterError );
	Return_if_NULL( inAuthBuff, eDSNullAuthStepData );
	Return_if_NULL( outAuthBuff, eDSNullAuthStepDataResp );
	Return_if_NULL( pContinue, eDSAuthContinueDataBad );

	switch ( pContinue->fAuthPass++ )
	{
		case 0:
            // start password server auth
			char *userCopy = strdup( inUserName );
			StripRSAKey( userCopy );
			snprintf( buf, sizeof(buf), "USER %s AUTH PPS %s", userCopy, inMechData );
			free( userCopy );
			DEBUGLOG( "sending %s", buf);
            serverResult = SendFlushReadWithMutex( inContext, buf, NULL, NULL, buf, sizeof(buf) );
			if ( serverResult.err != 0 ) {
				DEBUGLOG( "server returned an error, err = %d", serverResult.err);
				return( (tDirStatus)PWSErrToDirServiceError(serverResult) );
			}
			
			// set step buffer
            sasl_chop( buf );
			ConvertHexToBinary( buf + 8, (unsigned char *)dataBuf, &dataBufLen );
			siResult = dsFillAuthBuffer( outAuthBuff, 1, dataBufLen, dataBuf );
			DEBUGLOG( "received: %s", buf);
			break;
			
		case 1:
			siResult = (tDirStatus) GetStringFromAuthBuffer( inData->fInAuthStepData, 2, &stepData );
			if ( siResult != eDSNoErr ) return(siResult);
			
			DEBUGLOG( "sending %s", stepData);
            serverResult = SendFlushReadWithMutex( inContext, "AUTH2", stepData, NULL, buf, sizeof(buf) );
			free( stepData );
			siResult = (tDirStatus) PWSErrToDirServiceError( serverResult );
			if ( siResult == eDSNoErr ) {
				sasl_chop( buf );
				ConvertHexToBinary( buf + 4, (unsigned char *)dataBuf, &dataBufLen );
				siResult = dsFillAuthBuffer( outAuthBuff, 1, dataBufLen, dataBuf );
				DEBUGLOG( "received: %s", buf);
			}
			gContinue->RemoveItem( pContinue );
			inData->fIOContinueData = NULL;
			break;
			
		default:
			siResult = eDSAuthFailed;
	}
	
	return siResult;
}


// ---------------------------------------------------------------------------
//	* SendFlushReadWithMutex
// ---------------------------------------------------------------------------
PWServerError
CPSPlugIn::SendFlushReadWithMutex(
	sPSContextData *inContext,
	const char *inCommandStr,
	const char *inArg1Str,
	const char *inArg2Str,
	char *inOutBuf,
	unsigned long inBufLen )
{
	PWServerError serverResult;
	
	gPWSConnMutex->Wait();
	serverResult = SendFlushRead( inContext, inCommandStr, inArg1Str, inArg2Str, inOutBuf, inBufLen );
	gPWSConnMutex->Signal();
	
	return serverResult;
}

											
// ---------------------------------------------------------------------------
//	* GetServerListFromDSDiscovery
// ---------------------------------------------------------------------------

SInt32
CPSPlugIn::GetServerListFromDSDiscovery( CFMutableArrayRef inOutServerList )
{
    tDirReference				dsRef				= 0;
    tDataBuffer				   *tNodeListDataBuff	= NULL;
    tDataBuffer				   *tDataBuff			= NULL;
    tDirNodeReference			nodeRef				= 0;
    long						status				= eDSNoErr;
    tContextData				context				= NULL;
    UInt32						index				= 0;
    UInt32						nodeCount			= 0;
	tDataList					*nodeName			= NULL;
    tDataList					*recordNameList		= NULL;
	tDataList					*recordTypeList		= NULL;
	tDataList					*attributeList		= NULL;
	UInt32						recIndex			= 0;
	UInt32						recCount			= 0;
	UInt32						attrIndex			= 0;
	UInt32						attrValueIndex		= 0;
	tRecordEntry		  		*recEntry			= NULL;
	tAttributeListRef			attrListRef			= 0;
	tAttributeValueListRef		valueRef			= 0;
	tAttributeEntry		   		*pAttrEntry			= NULL;
	tAttributeValueEntry   		*pValueEntry		= NULL;
	long						nameLen				= 0;
	sPSServerEntry 				anEntry;
	
	if ( inOutServerList == NULL )
		return -1;
	
	try
    {
		status = dsOpenDirService( &dsRef );
		if (status != eDSNoErr) throw( status );
		
		tNodeListDataBuff = dsDataBufferAllocate( dsRef, 4096 );
		if (tNodeListDataBuff == NULL) throw( (long)eMemoryError );
		
		tDataBuff = dsDataBufferAllocate( dsRef, 4096 );
		if (tDataBuff == NULL) throw( (long)eMemoryError );
		
        // find and don't open
		status = dsFindDirNodes( dsRef, tNodeListDataBuff, NULL, eDSDefaultNetworkNodes, &nodeCount, &context );
        if (status != eDSNoErr) throw( status );
        if ( nodeCount < 1 ) throw( (long)eDSNodeNotFound );
		
		recordNameList = dsBuildListFromStrings( dsRef, kDSRecordsAll, NULL );
		recordTypeList = dsBuildListFromStrings( dsRef, "dsRecTypeNative:passwordserver", NULL );
		attributeList = dsBuildListFromStrings( dsRef, kDSAttributesAll, NULL );
		
		for ( index = 1; index <= nodeCount; index++ )
        {
            status = dsGetDirNodeName( dsRef, tNodeListDataBuff, index, &nodeName );
            if ( status != eDSNoErr )
				break;
            
            status = dsOpenDirNode( dsRef, nodeName, &nodeRef );
			dsDataListDeallocFree( dsRef, nodeName );
            nodeName = NULL;
            if ( status != eDSNoErr )
				break;
            
			do
			{
				recCount = 0;
				status = dsGetRecordList( nodeRef, tDataBuff, recordNameList, eDSExact,
											recordTypeList, attributeList, false,
											&recCount, &context );
				
				if ( status != eDSNoErr )
					break;
				
				for ( recIndex = 1; recIndex <= recCount; recIndex++ )
				{
					bzero( &anEntry, sizeof(anEntry) );
					
					status = dsGetRecordEntry( nodeRef, tDataBuff, recIndex, &attrListRef, &recEntry );
					if ( status != eDSNoErr || recEntry == NULL )
						continue;
					
					for ( attrIndex = 1;
						  (attrIndex <= recEntry->fRecordAttributeCount) && (status == eDSNoErr);
						  attrIndex++ )
					{
						status = dsGetAttributeEntry( nodeRef, tDataBuff, attrListRef, attrIndex, &valueRef, &pAttrEntry );
						if ( status == eDSNoErr && pAttrEntry != NULL )
						{
							for ( attrValueIndex = 1;
								  (attrValueIndex <= pAttrEntry->fAttributeValueCount) && (status == eDSNoErr);
								  attrValueIndex++ )
							{
								status = dsGetAttributeValue( nodeRef, tDataBuff, attrValueIndex, valueRef, &pValueEntry );
								if ( status == eDSNoErr && pValueEntry != NULL )
								{
									if ( strcmp( pAttrEntry->fAttributeSignature.fBufferData, kDSNAttrRecordName ) == 0 )
									{
										nameLen = strlen( pValueEntry->fAttributeValueData.fBufferData );
										if ( nameLen >= 32 )
											strncpy( anEntry.id, pValueEntry->fAttributeValueData.fBufferData, 32 );
										strcpy( anEntry.port, kPasswordServerPortStr );
									}
									
									// DEBUG
									if ( true )
									{
										DEBUGLOG( "      %l - %l: (%s) %s", attrIndex, attrValueIndex,
																pAttrEntry->fAttributeSignature.fBufferData,
																pValueEntry->fAttributeValueData.fBufferData );
									}
									dsDeallocAttributeValueEntry( dsRef, pValueEntry );
									pValueEntry = NULL;
								}
								else
								{
									//PrintError( kErrGetAttributeEntry, error );
								}
							}
							dsDeallocAttributeEntry( dsRef, pAttrEntry );
							pAttrEntry = NULL;
							dsCloseAttributeValueList(valueRef);
							valueRef = 0;
						}
					}
				}
				
				if ( nodeRef != 0 ) {
					dsCloseDirNode( nodeRef );
					nodeRef = 0;
				}
			}
			while ( context != 0 );
		}
    }
    catch( long error )
	{
		status = error;
	}
    catch( ... )
	{
		status = eDSAuthFailed;
	}
	
	if ( recordNameList != NULL )
		dsDataListDeallocFree( dsRef, recordNameList );
	if ( recordTypeList != NULL )
		dsDataListDeallocFree( dsRef, recordTypeList );
	if ( attributeList != NULL )
		dsDataListDeallocFree( dsRef, attributeList );
	
    if (tNodeListDataBuff != NULL) {
		dsDataBufferDeAllocate( dsRef, tNodeListDataBuff );
		tNodeListDataBuff = NULL;
	}
    if (tDataBuff != NULL) {
		dsDataBufferDeAllocate( dsRef, tDataBuff );
		tDataBuff = NULL;
	}
    if (nodeRef != 0) {
		dsCloseDirNode(nodeRef);
		nodeRef = 0;
	}
	if (dsRef != 0) {
		dsCloseDirService(dsRef);
		dsRef = 0;
	}
		
DEBUGLOG( "GetServerListFromDSDiscovery = %l", status);
	return status;
}


//------------------------------------------------------------------------------------
//      * DoPlugInCustomCall
//------------------------------------------------------------------------------------ 

SInt32 CPSPlugIn::DoPlugInCustomCall ( sDoPlugInCustomCall *inData )
{
	SInt32				siResult		= eDSNoErr;
	sPSContextData		*pContext		= nil;
	
//seems that the client needs to have a tDirNodeReference 
//to make the custom call even though it will likely be non-dirnode specific related

DEBUGLOG( "CPSPlugIn::DoPlugInCustomCall" );
	
	try
	{
		if ( inData == nil ) throw( (SInt32)eDSNullParameter );
		if ( inData->fInRequestData == nil ) throw( (SInt32)eDSNullDataBuff );
		if ( inData->fInRequestData->fBufferData == nil ) throw( (SInt32)eDSEmptyBuffer );
		
		pContext = (sPSContextData *)gPSContextTable->GetItemData( inData->fInNodeRef );
		if ( pContext == nil ) throw( (SInt32)eDSBadContextData );
		
		switch( inData->fInRequestCode )
		{
			case 1:
				gPWSConnMutex->Wait();
				if ( pContext->replicaFile != nil )
				{
					[(ReplicaFile *)pContext->replicaFile free];
					pContext->replicaFile = nil;
				}
				
				//DEBUGLOG( "CPSPlugIn::DoPlugInCustomCall received replica list: %s", inData->fInRequestData->fBufferData );
				pContext->replicaFile = [[ReplicaFile alloc] initWithXMLStr:inData->fInRequestData->fBufferData];
				gPWSConnMutex->Signal();
				break;
				
			default:
				break;
		}
	}
	catch ( SInt32 err )
	{
		siResult = err;
	}
	catch (...)
	{
		siResult = eDSAuthFailed;
	}
	
	return( siResult );

} // DoPlugInCustomCall


// ---------------------------------------------------------------------------
//	* ContinueDeallocProc
// ---------------------------------------------------------------------------

void CPSPlugIn::ContinueDeallocProc ( void* inContinueData )
{
	sPSContinueData *pContinue = NULL;
    
	gPWSConnMutex->Wait();
	
	pContinue = (sPSContinueData *)inContinueData;
	if ( pContinue != nil )
	{
		if ( pContinue->fData != NULL )
		{
			free( pContinue->fData );
			pContinue->fData = NULL;
		}
		
		if ( pContinue->fSASLSecret != NULL )
		{
			bzero( pContinue->fSASLSecret->data, pContinue->fSASLSecret->len );
			free( pContinue->fSASLSecret );
			pContinue->fSASLSecret = NULL;
		}
		
		free( pContinue );
		pContinue = nil;
	}
	
	gPWSConnMutex->Signal();
	
} // ContinueDeallocProc


// ---------------------------------------------------------------------------
//	* ContextDeallocProc
// ---------------------------------------------------------------------------

void CPSPlugIn::ContextDeallocProc ( void* inContextData )
{
	sPSContextData *pContext = (sPSContextData *) inContextData;

	if ( pContext != NULL )
	{
		CleanContextData( pContext );
		
		free( pContext );
		pContext = NULL;
	}
} // ContextDeallocProc


// ---------------------------------------------------------------------------
//	* ReleaseCloseWaitConnections
// ---------------------------------------------------------------------------

void CPSPlugIn::ReleaseCloseWaitConnections( void* inContextData )
{
	sPSContextData *pContext = (sPSContextData *)inContextData;
	if ( pContext != nil && Connected(pContext) == false )
		EndServerSession( pContext, false );
}

