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
 * @header CPSPlugIn
 */

#include <sys/stat.h>
#include <sys/types.h>

#include "CPSPlugIn.h"
#include "CPSUtilities.h"
#include "CAuthFileBase.h"

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


#define kDSTempSyncFileControlStr		"/var/db/authserver/apsSyncFi%ld.%ld.gz"
#define kDSRequestNonceHashStr			"hash"

#define dsDataListDeallocFree(DSREF, NODE)		{ dsDataListDeallocate( (DSREF), (NODE) ); free( (NODE) ); }


// --------------------------------------------------------------------------------
//	Globals

CPlugInRef				   *gPSContextTable	= NULL;
static DSEventSemaphore	   *gKickSearchRequests	= NULL;
static DSMutexSemaphore	   *gSASLMutex = NULL;
static DSMutexSemaphore	   *gPWSConnMutex = NULL;
CContinue	 			   *gContinue = NULL;

extern long gOpenCount;
static long gCloseCount = 0;

// Consts ----------------------------------------------------------------------------

static const	uInt32	kBuffPad	= 16;

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
    
    try
    {
        if ( gPSContextTable == NULL )
        {
            gPSContextTable = new CPlugInRef( CPSPlugIn::ContextDeallocProc );
            Throw_NULL( gPSContextTable, eMemoryAllocError );
        }
        
        if ( gKickSearchRequests == NULL )
        {
            gKickSearchRequests = new DSEventSemaphore();
            Throw_NULL( gKickSearchRequests, eMemoryAllocError );
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
    }
    
    catch (sInt32 err)
    {
	    DEBUGLOG( "CPSPlugIn::CPSPlugIn failed: eMemoryAllocError\n");
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

sInt32 CPSPlugIn::Validate ( const char *inVersionStr, const uInt32 inSignature )
{
	fSignature = inSignature;

	return( noErr );

} // Validate


// --------------------------------------------------------------------------------
//	* Initialize ()
// --------------------------------------------------------------------------------

sInt32 CPSPlugIn::Initialize ( void )
{
    sInt32 siResult = eDSNoErr;
	
	// set the active and initted flags
	fState = kUnknownState;
	fState += kInitialized;
	fState += kActive;
	
	WakeUpRequests();

	return( siResult );

} // Initialize


// --------------------------------------------------------------------------------
//	* SetPluginState ()
// --------------------------------------------------------------------------------

sInt32 CPSPlugIn::SetPluginState ( const uInt32 inState )
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
	gKickSearchRequests->Signal();
} // WakeUpRequests


// ---------------------------------------------------------------------------
//	* WaitForInit
//
// ---------------------------------------------------------------------------

void CPSPlugIn::WaitForInit ( void )
{
	volatile	uInt32		uiAttempts	= 0;

	if (!(fState & kActive))
	{
	while ( !(fState & kInitialized) &&
			!(fState & kFailedToInit) )
	{
		try
		{
			// Try for 2 minutes before giving up
			if ( uiAttempts++ >= 240 )
			{
				return;
			}

			// Now wait until we are told that there is work to do or
			//	we wake up on our own and we will look for ourselves

			gKickSearchRequests->Wait( (uInt32)(.5 * kMilliSecsPerSec) );
            
			try
			{
				gKickSearchRequests->Reset();
			}

			catch( long err )
			{
			}
		}

		catch( long err1 )
		{
		}
	}
	}//NOT already Active
} // WaitForInit


// ---------------------------------------------------------------------------
//	* ProcessRequest
//
// ---------------------------------------------------------------------------

sInt32 CPSPlugIn::ProcessRequest ( void *inData )
{
	sInt32		siResult	= 0;

	if ( inData == NULL )
	{
		return( ePlugInDataError );
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

sInt32 CPSPlugIn::HandleRequest ( void *inData )
{
	sInt32	siResult	= 0;
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

sInt32 CPSPlugIn::ReleaseContinueData ( sReleaseContinueData *inData )
{
	sInt32	siResult	= eDSNoErr;
    
	// RemoveItem calls our ContinueDeallocProc to clean up
	if ( gContinue->RemoveItem( inData->fInContinueData ) != eDSNoErr )
	{
		siResult = eDSInvalidContext;
	}
    
	return( siResult );

} // ReleaseContinueData


//------------------------------------------------------------------------------------
//	* OpenDirNode
//------------------------------------------------------------------------------------

sInt32 CPSPlugIn::OpenDirNode ( sOpenDirNode *inData )
{
	sInt32				siResult		= eDSNoErr;
    tDataListPtr		pNodeList		= NULL;
	char			   *pathStr			= NULL;
    char			   *subStr			= NULL;
	sPSContextData	   *pContext		= NULL;
	bool				nodeNameIsID	= false;
	sPSServerEntry		anEntry;
				   
    pNodeList	=	inData->fInDirNodeName;
    
    DEBUGLOG( "CPSPlugIn::OpenDirNode \n");
	
	try
	{
		if ( inData != NULL )
		{
			pathStr = dsGetPathFromListPriv( pNodeList, (char *)"/" );
            Throw_NULL( pathStr, eDSNullNodeName );
            
            DEBUGLOG( "CPSPlugIn::OpenDirNode path = %s\n", pathStr);

            unsigned int prefixLen = strlen(kPasswordServerPrefixStr);
            
            //special case for the configure PS node?
            if (::strcmp(pathStr,"/PasswordServer") == 0)
            {
                // set up the context data now with the relevant parameters for the configure PasswordServer node
                // DS API reference number is used to access the reference table
				/*
                pContext = MakeContextData();
                pContext->psName = new char[1+::strlen("PasswordServer Configure")];
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
					throw( (sInt32)eParameterError );
				else
				if ( strlen(subStr) >= sizeof(anEntry.ip) )
					throw( (sInt32)eParameterError );
				
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
						strncpy( anEntry.dns, subStr, sizeof(anEntry.dns) );
						anEntry.dns[sizeof(anEntry.dns) - 1] = '\0';
						
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
						
						DEBUGLOG( "anEntry.ip = %s\n", anEntry.ip );
					}
					
					anEntry.ipFromNode = true;
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
	catch( sInt32 err )
	{
		siResult = err;
		if (pContext != NULL)
		{
			gPSContextTable->RemoveItem( inData->fOutNodeRef );
		}
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

sInt32 CPSPlugIn::CloseDirNode ( sCloseDirNode *inData )
{
	sInt32				siResult	= eDSNoErr;
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
    
	catch( sInt32 err )
	{
		siResult = err;
	}
    
	return( siResult );

} // CloseDirNode


// ---------------------------------------------------------------------------
//	* HandleFirstContact
// ---------------------------------------------------------------------------

sInt32 CPSPlugIn::HandleFirstContact( sPSContextData *inContext, const char *inIP, const char *inUserKeyHash, const char *inUserKeyStr, bool inSecondTime )
{
	sInt32 siResult = eDSNoErr;
	char *psName;
	CFDataRef serverRef;
	bool usingLocalCache = false;
	bool usingConfigRecord = false;
	int sock = -1;
	sPSServerEntry anEntry;
	
	DEBUGLOG( "HandleFirstContact\n");
	
	bzero( &anEntry, sizeof(anEntry) );
	
	gPWSConnMutex->Wait();
	
	try
	{
		if ( ! inContext->providedNodeOnlyOrFail )
		{
			if ( inContext->serverList != NULL )
				CFRelease( inContext->serverList );
			
			// try the directory's config record if provided
			if ( inContext->replicaFile != NULL )
			{
				siResult = GetServerListFromConfig( &inContext->serverList, inContext->replicaFile );
				if ( siResult == kCPSUtilOK && inContext->serverList != NULL && CFArrayGetCount( inContext->serverList ) > 0 )
					siResult = IdentifyReachableReplica( inContext->serverList, inUserKeyHash, &anEntry, &sock );
					
				usingConfigRecord = true;
			}
			else
			{
				if ( ! usingConfigRecord )
				{
					// try the local cache
					siResult = GetPasswordServerList( &inContext->serverList, kPWSearchLocalFile );
					if ( siResult == kCPSUtilOK && inContext->serverList != NULL && CFArrayGetCount( inContext->serverList ) > 0 )
					{
						siResult = IdentifyReachableReplica( inContext->serverList, inUserKeyHash, &anEntry, &sock );
						usingLocalCache = ( siResult == kCPSUtilOK );
					}
					
					// try the replication database
					if ( siResult != kCPSUtilOK || !usingLocalCache )
					{
						if ( inContext->serverList != NULL )
							CFRelease( inContext->serverList );
							
						siResult = GetPasswordServerList( &inContext->serverList, kPWSearchReplicaFile );
						if ( siResult == kCPSUtilOK && inContext->serverList != NULL && CFArrayGetCount( inContext->serverList ) > 0 )
							siResult = IdentifyReachableReplica( inContext->serverList, inUserKeyHash, &anEntry, &sock );
					}
					
					// try rendezvous
					if ( siResult != kCPSUtilOK )
					{
						if ( inContext->serverList != NULL )
							CFRelease( inContext->serverList );
							
						siResult = GetPasswordServerList( &inContext->serverList, kPWSearchRegisteredServices );
						if ( siResult == kCPSUtilOK && inContext->serverList != NULL && CFArrayGetCount( inContext->serverList ) > 0 )
							siResult = IdentifyReachableReplica( inContext->serverList, inUserKeyHash, &anEntry, &sock );
					}
				}
			}
		}
		
		// try node IP
		if ( inContext->serverList == NULL || siResult != kCPSUtilOK )
		{
			if ( inContext->serverList != NULL )
				CFRelease( inContext->serverList );
				
			inContext->serverList = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
			Throw_NULL( inContext->serverList, eMemoryError );
			
			// add node IP
			serverRef = CFDataCreate( kCFAllocatorDefault, (const unsigned char *)&inContext->serverProvidedFromNode, sizeof(sPSServerEntry) );
			Throw_NULL( serverRef, eMemoryError );
			CFArrayAppendValue( inContext->serverList, serverRef );
			CFRelease( serverRef );
			
			siResult = IdentifyReachableReplica( inContext->serverList, inUserKeyHash, &anEntry, &sock );
		}
				
		// try localhost
		if ( !inSecondTime && !inContext->providedNodeOnlyOrFail && (inContext->serverList == NULL || siResult != kCPSUtilOK) )
		{
			if ( inContext->serverList != NULL )
				CFRelease( inContext->serverList );
				
			inContext->serverList = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
			Throw_NULL( inContext->serverList, eMemoryError );
			
			// add localhost
			sPSServerEntry localEntry = { 0, 0, 0, "127.0.0.1", kPasswordServerPortStr, "", "" };
			serverRef = CFDataCreate( kCFAllocatorDefault, (const unsigned char *)&localEntry, sizeof(sPSServerEntry) );
			Throw_NULL( serverRef, eMemoryError );
			CFArrayAppendValue( inContext->serverList, serverRef );
			CFRelease( serverRef );
			
			siResult = IdentifyReachableReplica( inContext->serverList, inUserKeyHash, &anEntry, &sock );
		}
		
		if ( siResult != kCPSUtilOK || anEntry.ip[0] == '\0' )
			throw( (sInt32)eDSAuthNoAuthServerFound );
		
		psName = (char *) calloc( 1, strlen(anEntry.ip) + 1 );
		Throw_NULL( psName, eDSNullNodeName );
		
		strcpy( psName, anEntry.ip );
		
		if ( inContext->psName != NULL )
			free( inContext->psName );
		inContext->psName = psName;
		
		strncpy(inContext->psPort, anEntry.port, 10);
		inContext->psPort[9] = '\0';
		
		siResult = BeginServerSession( inContext, sock );
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
						
					if ( stat( kPWReplicaLocalFile, &sb ) == 0 )
					{
						remove( kPWReplicaLocalFile );
						siResult = HandleFirstContact( inContext, inIP, inUserKeyHash, inUserKeyStr, true );
					}
				}
			}
			
			// save the cache iff we did a fresh lookup and were not limited to a single IP
			if ( (!usingLocalCache) && (!inContext->providedNodeOnlyOrFail) )
			{
				if ( anEntry.id[0] == '\0' )
					snprintf( anEntry.id, sizeof(anEntry.id), "%s", inContext->rsaPublicKeyHash );
				(void)SaveLocalReplicaCache( inContext->serverList, &anEntry );
			}
		}
	}
	
	catch( sInt32 error )
	{
		siResult = error;
	}
	
	gPWSConnMutex->Signal();
	
	if ( usingConfigRecord && inContext->replicaFile != NULL )
	{
		delete inContext->replicaFile;
		inContext->replicaFile = NULL;
	}
	
	return siResult;
}


// ---------------------------------------------------------------------------
//	* BeginServerSession
//
//	
// ---------------------------------------------------------------------------

sInt32 CPSPlugIn::BeginServerSession( sPSContextData *inContext, int inSock )
{
	sInt32 siResult = eDSNoErr;
	unsigned count;
	char *tptr, *end;
	char buf[4096];
	PWServerError serverResult;
	
	try
	{
DEBUGLOG( "BeginServerSession, inSock = %d\n", inSock );

		if ( inSock != -1 )
		{
			inContext->fd = inSock;
			inContext->serverIn = fdopen(inSock, "r");
			inContext->serverOut = fdopen(inSock, "w");
			
			// discard the greeting message
			readFromServer(inSock, buf, sizeof(buf));
		}
		else
		{
			// connect to remote server
			siResult = ConnectToServer( inContext );
			if ( siResult != eDSNoErr )
				throw( siResult );
		}
		
		// set ip addresses
		int salen;
		char hbuf[NI_MAXHOST], pbuf[NI_MAXSERV];
		struct sockaddr_storage local_ip;
		
		salen = sizeof(local_ip);
		if (getsockname(inContext->fd, (struct sockaddr *)&local_ip, &salen) < 0) {
			DEBUGLOG("getsockname");
		}
		
		getnameinfo((struct sockaddr *)&local_ip, salen,
					hbuf, sizeof(hbuf), pbuf, sizeof(pbuf),
					NI_NUMERICHOST | NI_WITHSCOPEID | NI_NUMERICSERV);
		snprintf(inContext->localaddr, sizeof(inContext->localaddr), "%s;%s", hbuf, pbuf);
	
		/*
		struct sockaddr_storage remote_ip;
		salen = sizeof(remote_ip);
		if (getpeername(inContext->fd, (struct sockaddr *)&remote_ip, &salen) < 0) {
			DEBUGLOG("getpeername");
		}
	
		getnameinfo((struct sockaddr *)&remote_ip, salen,
					hbuf, sizeof(hbuf), pbuf, sizeof(pbuf),
					NI_NUMERICHOST | NI_WITHSCOPEID | NI_NUMERICSERV);
		
		snprintf(inContext->remoteaddr, sizeof(inContext->remoteaddr), "%s;%s", hbuf, pbuf);
		*/
		snprintf(inContext->remoteaddr, sizeof(inContext->remoteaddr), "%s;%s", inContext->psName, inContext->psPort);
		
		// retrieve the password server's list of available auth methods
		serverResult = SendFlushRead( inContext, "LIST RSAPUBLIC", NULL, NULL, buf, sizeof(buf) );
		if ( serverResult.err != 0 )
			throw( PWSErrToDirServiceError(serverResult) );
		
		sasl_chop(buf);
		tptr = buf;
		for (count=0; tptr; count++ ) {
			tptr = strchr( tptr, ' ' );
			if (tptr) tptr++;
		}
		
		if (count > 0) {
			inContext->mech = (AuthMethName *)calloc(count, sizeof(AuthMethName));
			Throw_NULL( inContext->mech, eMemoryAllocError );
			
			inContext->mechCount = count;
		}
		
		tptr = strstr( buf, kSASLListPrefix );
		if ( tptr )
		{
			tptr += strlen( kSASLListPrefix );
			
			for ( ; tptr && count > 0; count-- )
			{
				if ( *tptr == '\"' )
					tptr++;
				else
					break;
				
				end = strchr( tptr, '\"' );
				if ( end != NULL )
					*end = '\0';
					
				strcpy( inContext->mech[count-1].method, tptr );
				DEBUGLOG( "mech=%s\n", tptr);
				
				tptr = end;
				if ( tptr != NULL )
					tptr += 2;
			}
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
			// retrieve the password server's public RSA key
			siResult = this->GetRSAPublicKey( inContext );
		}
		
		if ( siResult != eDSNoErr )
		{
			DEBUGLOG( "rsapublic = %l\n", siResult);
			throw( siResult );
		}
	}
	catch( sInt32 error )
	{
		siResult = error;
	}
	
	return siResult;
}
    

// ---------------------------------------------------------------------------
//	* EndServerSession
// ---------------------------------------------------------------------------

sInt32 CPSPlugIn::EndServerSession( sPSContextData *inContext, bool inSendQuit )
{

DEBUGLOG( "CPSPlugIn::EndServerSession closing %d\n", inContext->fd );
	
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
			serverResult = SendFlushRead( inContext, "QUIT", NULL, NULL, buf, sizeof(buf) );
        }
	}
	
	if ( inContext->serverIn != NULL ) {
		fpurge( inContext->serverIn );
		fclose( inContext->serverIn );
		inContext->serverIn = NULL;
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
	
	gPWSConnMutex->Signal();
	
DEBUGLOG( "CPSPlugIn::EndServerSession opens: %l, closes %l\n", gOpenCount, gCloseCount );

	return eDSNoErr;
}


// ---------------------------------------------------------------------------
//	* GetRSAPublicKey
// ---------------------------------------------------------------------------

sInt32 CPSPlugIn::GetRSAPublicKey( sPSContextData *inContext, char *inData )
{
	sInt32				siResult			= eDSNoErr;
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
			serverResult = SendFlushRead( inContext, "RSAPUBLIC", NULL, NULL, buf, sizeof(buf) );
			if ( serverResult.err != 0 )
			{
				DEBUGLOG( "no public key\n");
				throw( (sInt32)eDSAuthServerError );
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
            DEBUGLOG( "no key bits\n");
            throw( (sInt32)eDSAuthServerError );
        }
		
		// calculate the hash of the ID for comparison
		CReplicaFile replicaFile(NULL);
		replicaFile.CalcServerUniqueID( inContext->rsaPublicKeyStr, inContext->rsaPublicKeyHash );
 	}
	
	catch( sInt32 err )
	{
		DEBUGLOG( "catch in GetRSAPublicKey = %l\n", err);
		siResult = err;
	}
    
    return siResult;
}


// ---------------------------------------------------------------------------
//	* RSAPublicKeysEqual
// ---------------------------------------------------------------------------

bool CPSPlugIn::RSAPublicKeysEqual ( const char *rsaKeyStr1, const char *rsaKeyStr2 )
{
	const char *end1 = rsaKeyStr1;
	const char *end2 = rsaKeyStr2;
	int index;
	bool result = false;
	
	if ( rsaKeyStr1 == NULL && rsaKeyStr2 == NULL )
		return true;
	else
	if ( rsaKeyStr1 == NULL || rsaKeyStr2 == NULL )
		return false;
	
	// locate the comment section (3rd space char)
	for ( index = 0; index < 3 && end1 != NULL; index++ )
	{
		end1 = strchr( end1, ' ' );
		if ( end1 != NULL )
			end1++;
	}
	for ( index = 0; index < 3 && end2 != NULL; index++ )
	{
		end2 = strchr( end2, ' ' );
		if ( end2 != NULL )
			end2++;
	}
	
	if ( end1 != NULL && end2 != NULL )
	{
		// the lengths of the keys (sans comment) should be the same
		if ( (end1-rsaKeyStr1) != (end2-rsaKeyStr2) )
			return false;
		result = ( strncmp( rsaKeyStr1, rsaKeyStr2, (end1-rsaKeyStr1) ) == 0 );
	}
	else
	{
		// no comment on either key
		if ( end1 == NULL && end2 == NULL )
		{
			result = ( strcmp( rsaKeyStr1, rsaKeyStr2 ) == 0 );
		}
		else
		{
			// comment on one key, get the length of the data section from that one
			if ( end1 != NULL )
				result = ( strncmp( rsaKeyStr1, rsaKeyStr2, (end1-rsaKeyStr1) ) == 0 );
			else
			if ( end2 != NULL )
				result = ( strncmp( rsaKeyStr1, rsaKeyStr2, (end2-rsaKeyStr2) ) == 0 );
		}
	}
	
	return result;
}


// ---------------------------------------------------------------------------
//	* DoRSAValidation
// ---------------------------------------------------------------------------

sInt32 CPSPlugIn::DoRSAValidation ( sPSContextData *inContext, const char *inUserKey )
{
	sInt32				siResult			= eDSNoErr;
	char				*encodedStr			= NULL;
    PWServerError		serverResult;
    char				buf[kOneKBuffer];
	char				*bnStr				= NULL;
	int					len;
    MD5_CTX				ctx;
	unsigned char		md5Result[MD5_DIGEST_LENGTH];
	
	gPWSConnMutex->Wait();
	
	try
	{
        Throw_NULL( inContext, eDSBadContextData );
		
		// make sure we are talking to the right server
		if ( ! RSAPublicKeysEqual( inContext->rsaPublicKeyStr, inUserKey ) )
			throw( (sInt32)eDSAuthServerError );
		
		siResult = GetBigNumber( inContext, &bnStr );
        switch ( siResult )
		{
			case kCPSUtilOK:
				break;
			case kCPSUtilMemoryError:
				throw( (sInt32)eMemoryError );
				break;
			case kCPSUtilParameterError:
				throw( (sInt32)eParameterError );
				break;
			default:
				throw( (sInt32)eDSAuthFailed );
		}
		
        int nonceLen = strlen(bnStr);
		
		// just to be safe
		if ( nonceLen > 256 ) {
			bnStr[256] = '\0';
			nonceLen = 256;
		}
		
		// add some scratch space for the RSA padding
        encodedStr = (char *)malloc( nonceLen + sizeof(kDSRequestNonceHashStr) + 64 );
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
            throw( (sInt32)eDSAuthServerError );
        }
        
        if ( ConvertBinaryTo64( encodedStr, (unsigned)len, buf ) == SASL_OK )
        {
            UInt32 encodedStrLen;
            
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
    
	catch( sInt32 err )
	{
		siResult = err;
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

sInt32 CPSPlugIn::SetupSecureSyncSession( sPSContextData *inContext )
{
	sInt32				siResult					= eDSNoErr;
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
	UInt32				encryptedStrLen;
	UInt32				randomLength;
	UInt32				nonceLength;
	time_t				now;
	char				timeBuf[30];
	
	if ( inContext == NULL )
		return eDSBadContextData;
	
	// already set up?
	{
		RC5_32_KEY zeroKey;
		
		bzero( &zeroKey, sizeof(RC5_32_KEY) );
		if ( memcmp( &zeroKey, &inContext->rc5Key, sizeof(RC5_32_KEY) ) != 0 )
			return eDSNoErr;
	}
	
	gPWSConnMutex->Wait();
	
	try
	{
		// load RSA private key from Password Server database
		authFile = new CAuthFileBase();
		if ( authFile == NULL )
			throw( (sInt32)eMemoryError );
		
		siResult = authFile->validateFiles();
		if ( siResult != 0 )
			throw( (sInt32)eDSAuthNoAuthServerFound );
		
		if ( authFile->loadRSAKeys() != 1 )
			throw( (sInt32)eDSAuthServerError );
		
		siResult = GetBigNumber( inContext, &bnStr );
        switch ( siResult )
		{
			case kCPSUtilOK:
				break;
			case kCPSUtilMemoryError:
				throw( (sInt32)eMemoryError );
				break;
			case kCPSUtilParameterError:
				throw( (sInt32)eParameterError );
				break;
			default:
				throw( (sInt32)eDSAuthFailed );
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
            throw( (sInt32)eDSAuthServerError );
        }
        
        if ( ConvertBinaryTo64( encryptedStr, (unsigned)len, buf ) != SASL_OK )
        {
			DEBUGLOG( "ConvertBinaryTo64() failed");
            throw( (sInt32)eParameterError );
		}
		
		time( &now );
		sprintf( timeBuf, "%lu", (unsigned long)now );
		
		serverResult = SendFlushRead( inContext, "SYNC SESSIONKEY", buf, timeBuf, buf, sizeof(buf) );
		if ( serverResult.err != 0 )
			throw( PWSErrToDirServiceError(serverResult) );
		
		if ( Convert64ToBinary( buf + 4, encryptedStr, kOneKBuffer, &encryptedStrLen ) != SASL_OK )
		{
			DEBUGLOG( "Convert64ToBinary() failed");
            throw( (sInt32)eParameterError );
		}
		if ( encryptedStrLen < 8 )
		{
			DEBUGLOG( "not enough data returned from SYNC SESSIONKEY");
            throw( (sInt32)eDSAuthServerError );
		}
		
		// get the RSA-encrypted random
		memcpy( &randomLength, encryptedStr, 4 );
		DEBUGLOG( "length of random from SYNC SESSIONKEY is %l", randomLength );
		if ( randomLength > 128 )
		{
			DEBUGLOG( "length of random from SYNC SESSIONKEY is too long");
            throw( (sInt32)eDSAuthServerError );
		}
		
		decryptedStr = (char *) malloc( randomLength + 12 );
		if ( decryptedStr == NULL )
			throw( (sInt32)eMemoryError );
		
		siResult = authFile->decryptRSA( (unsigned char *)encryptedStr + 4, randomLength, (unsigned char *)decryptedStr );
		decryptedStr[randomLength] = '\0';
		if ( siResult != 0 )
			throw( (sInt32)eDSNotAuthorized );
		
		// get the rc5-encrypted nonce
		memcpy( &nonceLength, encryptedStr + 4 + randomLength, 4 );
		if ( nonceLength > 1024 )
			throw( (sInt32)eDSAuthServerError );
		
		encNonceStr = (char *) malloc( nonceLength + RC5_32_BLOCK + 1 );
		if ( encNonceStr == NULL )
			throw( (sInt32)eMemoryError );
		
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
			serverResult = SendFlushRead( inContext, "SYNC SESSIONKEYV", base64Buf, NULL, buf, sizeof(buf) );
			if ( serverResult.err == kAuthUserNotAuthenticated && serverResult.type == kPolicyError )
				siResult = eDSNotAuthorized;
			else
				siResult = PWSErrToDirServiceError( serverResult );
		}
 	}
    
	catch( sInt32 err )
	{
		siResult = err;
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
		authFile->closePasswordFile();
		delete authFile;
	}
	
    return siResult;
}


// ---------------------------------------------------------------------------
//	* MakeContextData
// ---------------------------------------------------------------------------

sPSContextData* CPSPlugIn::MakeContextData ( void )
{
    sPSContextData  	*pOut		= NULL;
    sInt32				siResult	= eDSNoErr;

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

sInt32 CPSPlugIn::CleanContextData ( sPSContextData *inContext )
{
    sInt32	siResult = eDSNoErr;
	   
    if ( inContext == NULL )
    {
		DEBUGLOG( "CPSPlugIn::CleanContextData eDSBadContextData\n");
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
        
        memset(inContext->last.username, 0, sizeof(inContext->last.username));
        
        if (inContext->last.password != NULL)
        {
            memset(inContext->last.password, 0, inContext->last.passwordLen);
            free(inContext->last.password);
            inContext->last.password = NULL;
        }
        inContext->last.passwordLen = 0;
        inContext->last.successfulAuth = false;
        
        memset(inContext->nao.username, 0, sizeof(inContext->nao.username));
        
        if (inContext->nao.password != NULL)
        {
            memset(inContext->nao.password, 0, inContext->nao.passwordLen);
            free(inContext->nao.password);
            inContext->nao.password = NULL;
        }
        inContext->nao.passwordLen = 0;
        inContext->nao.successfulAuth = false;
		
		if ( inContext->replicaFile != NULL )
		{
			delete inContext->replicaFile;
			inContext->replicaFile = NULL;
		}
		
		if ( inContext->serverList != NULL )
		{
			CFRelease( inContext->serverList );
			inContext->serverList = NULL;
		}
		
		if ( inContext->syncFilePath != NULL )
		{
			remove( inContext->syncFilePath );
			free( inContext->syncFilePath );
			inContext->syncFilePath = NULL;
		}
		
		bzero( &inContext->rc5Key, sizeof(RC5_32_KEY) );
		inContext->madeFirstContact = false;
		
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

sInt32 CPSPlugIn::GetAttributeEntry ( sGetAttributeEntry *inData )
{
	sInt32					siResult			= eDSNoErr;
	uInt16					usAttrTypeLen		= 0;
	uInt16					usAttrCnt			= 0;
	uInt16					usAttrLen			= 0;
	uInt16					usValueCnt			= 0;
	uInt16					usValueLen			= 0;
	uInt32					i					= 0;
	uInt32					uiIndex				= 0;
	uInt32					uiAttrEntrySize		= 0;
	uInt32					uiOffset			= 0;
	uInt32					uiTotalValueSize	= 0;
	uInt32					offset				= 4;
	uInt32					buffSize			= 0;
	uInt32					buffLen				= 0;
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
            throw( (sInt32)eDSInvalidIndex );
        
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
		if ( 2 > (sInt32)(buffSize - offset) )
            throw( (sInt32)eDSInvalidBuffFormat );
        
		// Get the attribute count
		::memcpy( &usAttrCnt, p, 2 );
		if (uiIndex > usAttrCnt)
            throw( (sInt32)eDSInvalidIndex );
        
		// Move 2 bytes
		p		+= 2;
		offset	+= 2;

		// Skip to the attribute that we want
		for ( i = 1; i < uiIndex; i++ )
		{
			// Do record check, verify that offset is not past end of buffer, etc.
			if (2 > (sInt32)(buffSize - offset) )
                throw( (sInt32)eDSInvalidBuffFormat );
            
			// Get the length for the attribute
			::memcpy( &usAttrLen, p, 2 );

			// Move the offset past the length word and the length of the data
			p		+= 2 + usAttrLen;
			offset	+= 2 + usAttrLen;
		}

		// Get the attribute offset
		uiOffset = offset;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 > (sInt32)(buffSize - offset))
            throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the length for the attribute block
		::memcpy( &usAttrLen, p, 2 );

		// Skip past the attribute length
		p		+= 2;
		offset	+= 2;

		//set the bufLen to stricter range
		buffLen = offset + usAttrLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 > (sInt32)(buffLen - offset))
            throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the length for the attribute type
		::memcpy( &usAttrTypeLen, p, 2 );
		
		pAttrType = p + 2;
		p		+= 2 + usAttrTypeLen;
		offset	+= 2 + usAttrTypeLen;
		
		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 > (sInt32)(buffLen - offset))
            throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get number of values for this attribute
		::memcpy( &usValueCnt, p, 2 );
		
		p		+= 2;
		offset	+= 2;
		
		for ( i = 0; i < usValueCnt; i++ )
		{
			// Do record check, verify that offset is not past end of buffer, etc.
			if (2 > (sInt32)(buffLen - offset))
                throw( (sInt32)eDSInvalidBuffFormat );
            
			// Get the length for the value
			::memcpy( &usValueLen, p, 2 );
			
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
		::memcpy( pAttribInfo->fAttributeSignature.fBufferData, pAttrType, usAttrTypeLen );

		pValueContext = MakeContextData();
		Throw_NULL( pValueContext , eMemoryAllocError );

		pValueContext->offset = uiOffset;

		gPSContextTable->AddItem( inData->fOutAttrValueListRef, pValueContext );

		inData->fOutAttrInfoPtr = pAttribInfo;
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // GetAttributeEntry


//------------------------------------------------------------------------------------
//	* GetAttributeValue
//------------------------------------------------------------------------------------

sInt32 CPSPlugIn::GetAttributeValue ( sGetAttributeValue *inData )
{
	sInt32						siResult		= eDSNoErr;
	uInt16						usValueCnt		= 0;
	uInt16						usValueLen		= 0;
	uInt16						usAttrNameLen	= 0;
	uInt32						i				= 0;
	uInt32						uiIndex			= 0;
	uInt32						offset			= 0;
	char					   *p				= NULL;
	tDataBuffer				   *pDataBuff		= NULL;
	tAttributeValueEntry	   *pAttrValue		= NULL;
	sPSContextData		   *pValueContext	= NULL;
	uInt32						buffSize		= 0;
	uInt32						buffLen			= 0;
	uInt16						attrLen			= 0;

	try
	{
		pValueContext = (sPSContextData *)gPSContextTable->GetItemData( inData->fInAttrValueListRef );
		Throw_NULL( pValueContext , eDSBadContextData );

		uiIndex = inData->fInAttrValueIndex;
		if (uiIndex == 0)
            throw( (sInt32)eDSInvalidIndex );
		
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
		if (2 > (sInt32)(buffSize - offset))
            throw( (sInt32)eDSInvalidBuffFormat );
				
		// Get the buffer length
		::memcpy( &attrLen, p, 2 );

		//now add the offset to the attr length for the value of buffLen to be used to check for buffer overruns
		//AND add the length of the buffer length var as stored ie. 2 bytes
		buffLen		= attrLen + pValueContext->offset + 2;
		if (buffLen > buffSize)
            throw( (sInt32)eDSInvalidBuffFormat );
        
		// Skip past the attribute length
		p		+= 2;
		offset	+= 2;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 > (sInt32)(buffLen - offset))
            throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the attribute name length
		::memcpy( &usAttrNameLen, p, 2 );
		
		p		+= 2 + usAttrNameLen;
		offset	+= 2 + usAttrNameLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 > (sInt32)(buffLen - offset))
            throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the value count
		::memcpy( &usValueCnt, p, 2 );
		
		p		+= 2;
		offset	+= 2;

		if (uiIndex > usValueCnt)
            throw( (sInt32)eDSInvalidIndex );

		// Skip to the value that we want
		for ( i = 1; i < uiIndex; i++ )
		{
			// Do record check, verify that offset is not past end of buffer, etc.
			if (2 > (sInt32)(buffLen - offset))
                throw( (sInt32)eDSInvalidBuffFormat );
		
			// Get the length for the value
			::memcpy( &usValueLen, p, 2 );
			
			p		+= 2 + usValueLen;
			offset	+= 2 + usValueLen;
		}

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 > (sInt32)(buffLen - offset))
            throw( (sInt32)eDSInvalidBuffFormat );
		
		::memcpy( &usValueLen, p, 2 );
		
		p		+= 2;
		offset	+= 2;

		//if (usValueLen == 0) throw (eDSInvalidBuffFormat ); //if zero is it okay?
        
		pAttrValue = (tAttributeValueEntry *)::calloc( 1, sizeof( tAttributeValueEntry ) + usValueLen + kBuffPad );
        Throw_NULL(pAttrValue, eMemoryAllocError);
        
		pAttrValue->fAttributeValueData.fBufferSize		= usValueLen + kBuffPad;
		pAttrValue->fAttributeValueData.fBufferLength	= usValueLen;
		
		// Do record check, verify that offset is not past end of buffer, etc.
		if ( usValueLen > (sInt32)(buffLen - offset) )
            throw ( (sInt32)eDSInvalidBuffFormat );
		
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

uInt32 CPSPlugIn::CalcCRC ( char *inStr )
{
	char		   *p			= inStr;
	sInt32			siI			= 0;
	sInt32			siStrLen	= 0;
	uInt32			uiCRC		= 0xFFFFFFFF;
	CRCCalc			aCRCCalc;

	if ( inStr != NULL )
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
//	* GetDirNodeInfo
//------------------------------------------------------------------------------------

sInt32 CPSPlugIn::GetDirNodeInfo ( sGetDirNodeInfo *inData )
{
	sInt32				siResult		= eDSNoErr;
	uInt32				uiOffset		= 0;
	uInt32				uiCntr			= 1;
	uInt32				uiAttrCnt		= 0;
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
            throw( (sInt32)eDSEmptyNodeInfoTypeList );

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
					aTmpData->AppendShort( ::strlen( "PasswordServer" ) );
					aTmpData->AppendString( (char *)"PasswordServer" );

					char *tmpStr = NULL;
					if (pContext->psName != NULL)
					{
						tmpStr = new char[1+::strlen(pContext->psName)];
						::strcpy( tmpStr, pContext->psName );
					}
					else
					{
						tmpStr = new char[1+::strlen("Unknown Node Location")];
						::strcpy( tmpStr, "Unknown Node Location" );
					}
					
					// Append attribute value
					aTmpData->AppendShort( ::strlen( tmpStr ) );
					aTmpData->AppendString( tmpStr );

					delete( tmpStr );

				} // fInAttrInfoOnly is false
				
				// Add the attribute length
				aAttrData->AppendShort( aTmpData->GetLength() );
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
					aTmpData->AppendShort( ::strlen( "ReadOnly" ) );
					aTmpData->AppendString( "ReadOnly" );

				}
				// Add the attribute length and data
				aAttrData->AppendShort( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );

				// Clear the temp block
				aTmpData->Clear();
			}
				 
			if ((::strcmp( pAttrName, kDSAttributesAll ) == 0) ||
				(::strcmp( pAttrName, kDSNAttrAuthMethod ) == 0) )
			{
				aTmpData->Clear();
				
				uiAttrCnt++;
			
				// Append the attribute name
				aTmpData->AppendShort( ::strlen( kDSNAttrAuthMethod ) );
				aTmpData->AppendString( kDSNAttrAuthMethod );

				if ( inData->fInAttrInfoOnly == false )
				{
					int idx, mechCount = 0;
					char dsTypeStr[256];
					
					// get the count for the mechs that get returned
					for ( idx = 0; idx < pContext->mechCount; idx++ )
					{
						GetAuthMethodFromSASLName( pContext->mech[idx].method, dsTypeStr );
						if ( dsTypeStr[0] != '\0' )
							mechCount++;
                    }
					
					// Attribute value count
					aTmpData->AppendShort( 7 + mechCount );
					
					aTmpData->AppendShort( ::strlen( kDSStdAuthClearText ) );
					aTmpData->AppendString( kDSStdAuthClearText );
					aTmpData->AppendShort( ::strlen( kDSStdAuthSetPasswd ) );
					aTmpData->AppendString( kDSStdAuthSetPasswd );
					aTmpData->AppendShort( ::strlen( kDSStdAuthChangePasswd ) );
					aTmpData->AppendString( kDSStdAuthChangePasswd );
					aTmpData->AppendShort( ::strlen( kDSStdAuthSetPasswdAsRoot ) );
					aTmpData->AppendString( kDSStdAuthSetPasswdAsRoot );
					aTmpData->AppendShort( ::strlen( kDSStdAuthNodeNativeClearTextOK ) );
					aTmpData->AppendString( kDSStdAuthNodeNativeClearTextOK );
					aTmpData->AppendShort( ::strlen( kDSStdAuthNodeNativeNoClearText ) );
					aTmpData->AppendString( kDSStdAuthNodeNativeNoClearText );
					
					// password server supports kDSStdAuth2WayRandomChangePasswd
					// with or without the plug-in
					aTmpData->AppendShort( ::strlen( kDSStdAuth2WayRandomChangePasswd ) );
					aTmpData->AppendString( kDSStdAuth2WayRandomChangePasswd );
					
					for ( idx = 0; idx < pContext->mechCount; idx++ )
					{
						GetAuthMethodFromSASLName( pContext->mech[idx].method, dsTypeStr );
						if ( dsTypeStr[0] != '\0' )
						{
							// Append first attribute value
							aTmpData->AppendShort( ::strlen( dsTypeStr ) );
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
//		aRecData->AppendShort( ::strlen( "dsAttrTypeStandard:DirectoryNodeInfo" ) ); = 2
//		aRecData->AppendString( "dsAttrTypeStandard:DirectoryNodeInfo" ); = 36
//		aRecData->AppendShort( ::strlen( "DirectoryNodeInfo" ) ); = 2
//		aRecData->AppendString( "DirectoryNodeInfo" ); = 17
//		total adjustment = 4 + 2 + 36 + 2 + 17 = 61

			pAttrContext->offset = uiOffset + 61;

			gPSContextTable->AddItem( inData->fOutAttrListRef, pAttrContext );
		}
	}

	catch( sInt32 err )
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

sInt32 CPSPlugIn::CloseAttributeList ( sCloseAttributeList *inData )
{
	sInt32				siResult		= eDSNoErr;
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

sInt32 CPSPlugIn::CloseAttributeValueList ( sCloseAttributeValueList *inData )
{
	sInt32				siResult		= eDSNoErr;
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

sInt32 CPSPlugIn::GetStringFromAuthBuffer(tDataBufferPtr inAuthData, int stringNum, char **outString)
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

sInt32 CPSPlugIn::Get2StringsFromAuthBuffer(tDataBufferPtr inAuthData, char **outString1, char **outString2)
{
	sInt32 result = GetStringFromAuthBuffer( inAuthData, 1, outString1 );
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

sInt32 CPSPlugIn::GetDataFromAuthBuffer(tDataBufferPtr inAuthData, int nodeNum, unsigned char **outData, long *outLen)
{
    tDataNodePtr pDataNode;
    tDirStatus status;
    
    *outLen = 0;
    
	tDataListPtr dataList = dsAuthBufferGetDataListAllocPriv(inAuthData);
	if (dataList != NULL)
	{
		status = dsDataListGetNodePriv(dataList, nodeNum, &pDataNode);
        if ( status != eDSNoErr )
            return status;
        
		*outData = (unsigned char *) malloc(pDataNode->fBufferLength);
        if ( ! (*outData) )
            return eMemoryAllocError;
        
        memcpy(*outData, ((tDataBufferPriv*)pDataNode)->fBufferData, pDataNode->fBufferLength);
        *outLen = pDataNode->fBufferLength;
        
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
//	* DoAuthentication
//------------------------------------------------------------------------------------

sInt32 CPSPlugIn::DoAuthentication ( sDoDirNodeAuth *inData )
{
	sInt32				siResult				= noErr;
	UInt32				uiAuthMethod			= 0;
	sPSContextData	   *pContext				= NULL;
    char				*userName				= NULL;
	char				*password				= NULL;
    long				passwordLen				= 0;
    char				*challenge				= NULL;
    char 				*userIDToSet			= NULL;
	char 				*paramStr				= NULL;
    Boolean				bHasValidAuth			= false;
    Boolean				bNeedsRSAValidation		= true;
    sPSContinueData		*pContinue				= NULL;
    char				*stepData				= NULL;
	char				*rsaKeyPtr				= NULL;
	bool				bMethodCanSetPassword	= false;
	char 				saslMechNameStr[256];
	
    DEBUGLOG( "CPSPlugIn::DoAuthentication\n");
	
	try
	{
		pContext = (sPSContextData *)gPSContextTable->GetItemData( inData->fInNodeRef );
		Throw_NULL( pContext, eDSBadContextData );
        
		siResult = GetAuthMethodConstant( pContext, inData->fInAuthMethod, &uiAuthMethod, saslMechNameStr );
        DEBUGLOG( "GetAuthMethodConstant siResult=%l, uiAuthMethod=%l, mech=%s\n", siResult, uiAuthMethod,saslMechNameStr);
		if ( siResult != eDSNoErr )
			throw( siResult );
		
        if ( inData->fIOContinueData == NULL )
        {
            siResult = UnpackUsernameAndPassword( pContext,
                                                    uiAuthMethod,
                                                    inData->fInAuthStepData,
                                                    &userName,
                                                    &password,
                                                    &passwordLen,
                                                    &challenge );
			if ( siResult != eDSNoErr )
				throw( siResult );
        }
        else
        {
            if ( gContinue->VerifyItem( inData->fIOContinueData ) == false )
                throw( (sInt32)eDSInvalidContinueData );
        }
        
		// get a pointer to the rsa public key
		if ( userName != NULL )
		{
			rsaKeyPtr = strchr( userName, ',' );
			if ( rsaKeyPtr != NULL )
				rsaKeyPtr++;
		}
		
		// make sure there is a server to contact
		if ( pContext->madeFirstContact )
		{
			// make sure there is a connection
			if ( Connected(pContext) )
			{
				bNeedsRSAValidation = false;
			}
			else
			{
				// close out anything stale
				EndServerSession( pContext );
				
				gPWSConnMutex->Wait();
				siResult = ConnectToServer( pContext );
				gPWSConnMutex->Signal();
				if ( siResult != 0 )
					throw( siResult );
			}
		}
		else
		{
			CReplicaFile replicaFile(NULL);
			char hexHash[34];
			
			if ( userName != NULL && rsaKeyPtr != NULL )
			{
				replicaFile.CalcServerUniqueID( rsaKeyPtr, hexHash );
				
				DEBUGLOG( "hexHash=%s\n", hexHash );
				
				siResult = HandleFirstContact( pContext, NULL, hexHash, rsaKeyPtr );
			}
			else
			{
				siResult = HandleFirstContact( pContext, NULL, NULL );
			}
			
			if ( siResult != eDSNoErr )
				throw( siResult );
			
			if ( ! Connected(pContext) )
			{
				// close out anything stale
				EndServerSession( pContext );
				throw( (sInt32)eDSAuthServerError );
			}
			
			pContext->madeFirstContact = true;
		}
		
		// if bNeedsRSAValidation == true, then this is a new connection and the authentication
		// needs to be re-established.
		if ( ! bNeedsRSAValidation )
		{
			siResult = UseCurrentAuthenticationIfPossible( pContext, userName, uiAuthMethod, &bHasValidAuth );
			if ( siResult != eDSNoErr )
				throw( siResult );
		}
		
        // do not authenticate for auth methods that do not need SASL authentication
        if ( !bHasValidAuth && siResult == noErr && RequiresSASLAuthentication( uiAuthMethod ) )
        {
			siResult = GetAuthMethodSASLName( uiAuthMethod, inData->fInDirNodeAuthOnlyFlag, saslMechNameStr, &bMethodCanSetPassword );
			DEBUGLOG( "GetAuthMethodSASLName siResult=%l, mech=%s\n", siResult, saslMechNameStr );
			if ( siResult != eDSNoErr )
				throw( siResult );
			
            if ( rsaKeyPtr != NULL && bNeedsRSAValidation )
				siResult = DoRSAValidation( pContext, rsaKeyPtr );
			
            if ( siResult == noErr )
            {
                pContext->last.successfulAuth = false;
                
				if ( inData->fIOContinueData == NULL )
				{
					pContinue = (sPSContinueData *)::calloc( 1, sizeof( sPSContinueData ) );
					Throw_NULL( pContinue, eMemoryError );
					
					gContinue->AddItem( pContinue, inData->fInNodeRef );
					inData->fIOContinueData = pContinue;
					
					pContinue->fAuthPass = 0;
					pContinue->fData = NULL;
					pContinue->fDataLen = 0;
					pContinue->fSASLSecret = NULL;
				}
				
				gPWSConnMutex->Wait();
				
				if ( uiAuthMethod == kAuth2WayRandom )
				{
					siResult = DoSASLTwoWayRandAuth( pContext,
													userName,
													saslMechNameStr,
													inData );
				}
				else
				{
					siResult = DoSASLAuth( pContext,
											userName,
											password,
											passwordLen,
											challenge,
											saslMechNameStr,
											inData,
											&stepData );
                }
                
				gPWSConnMutex->Signal();
				
                if ( siResult == noErr && uiAuthMethod != kAuth2WayRandom )
                {
                    pContext->last.successfulAuth = true;
                    pContext->last.methodCanSetPassword = bMethodCanSetPassword;
					
                    // If authOnly == false, copy the username and password for later use
                    // with SetPasswordAsRoot.
                    if ( inData->fInDirNodeAuthOnlyFlag == false )
                    {
                        memcpy( pContext->nao.username, pContext->last.username, kMaxUserNameLength + 1 );
                        
						if ( pContext->nao.password != NULL ) {
							memset( pContext->nao.password, 0, pContext->nao.passwordLen );
							free( pContext->nao.password );
							pContext->nao.password = NULL;
							pContext->nao.passwordLen = 0;
						}
						
                        pContext->nao.password = (char *) malloc( pContext->last.passwordLen + 1 );
                        Throw_NULL( pContext->nao.password, eMemoryError );
                        
                        memcpy( pContext->nao.password, pContext->last.password, pContext->last.passwordLen );
                        pContext->nao.password[pContext->last.passwordLen] = '\0';
                        
                        pContext->nao.passwordLen = pContext->last.passwordLen;
                        pContext->nao.successfulAuth = true;
						pContext->nao.methodCanSetPassword = pContext->last.methodCanSetPassword;
                    }
                }
            }
        }
		
        if ( siResult == eDSNoErr || siResult == eDSAuthNewPasswordRequired )
        {
            tDataBufferPtr outBuf = inData->fOutAuthStepDataResponse;
            const char *encodedStr;
            unsigned int encodedStrLen;
            char encoded64Str[kOneKBuffer];
            char buf[kOneKBuffer];
            PWServerError result;
            int saslResult = SASL_OK;
            
            switch( uiAuthMethod )
			{
				case kAuthDIGEST_MD5:
				case kAuthDIGEST_MD5_Reauth:
				case kAuthMSCHAP2:
					#pragma mark kAuthDigestMD5
					#pragma mark kAuthMSCHAP2
					
					if ( stepData != NULL )
						siResult = PackStepBuffer( stepData, false, NULL, NULL, NULL, outBuf );
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
					siResult = Get2StringsFromAuthBuffer( inData->fInAuthStepData, &userIDToSet, &paramStr );
                    if ( siResult == noErr )
                        StripRSAKey( userIDToSet );
                    
					if ( siResult == noErr )
                    {
                        if ( paramStr == NULL )
                            throw( (sInt32)eDSInvalidBuffFormat );
                        if (strlen(paramStr) > kChangePassPaddedBufferSize )
                            throw( (sInt32)eDSAuthParameterError );
                        
						// special-case for an empty password. DIGEST-MD5 does not
						// support empty passwords, but it's a DS requirement
						if ( *paramStr == '\0' )
						{
							free( paramStr );
							paramStr = (char *) malloc( strlen(kEmptyPasswordAltStr) + 1 );
							strcpy( paramStr, kEmptyPasswordAltStr );
						}
						
                        strcpy(buf, paramStr);
                        
                        gSASLMutex->Wait();
                        saslResult = sasl_encode(pContext->conn,
												 buf,
												 kChangePassPaddedBufferSize,
												 &encodedStr,
												 &encodedStrLen); 
                        gSASLMutex->Signal();
                    }
                    
                    if ( siResult == noErr && saslResult == SASL_OK && userIDToSet != NULL )
                    {
                        if ( ConvertBinaryTo64( encodedStr, encodedStrLen, encoded64Str ) == SASL_OK )
                        {
							result = SendFlushReadWithMutex( pContext, "CHANGEPASS", userIDToSet, encoded64Str, buf, sizeof(buf) );
							siResult = PWSErrToDirServiceError( result );
							if ( siResult == eDSNoErr )
								UpdateCachedPasswordOnChange( pContext, userIDToSet, paramStr, strlen(paramStr) );
                        }
                    }
                    else
                    {
                        printf("encode64 failed\n");
                    }
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
                    
                    siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &paramStr );
                    if ( siResult == noErr )
                    {
                        if ( paramStr == NULL )
                            throw( (sInt32)eDSInvalidBuffFormat );
                        if ( strlen(paramStr) > kChangePassPaddedBufferSize )
                            throw( (sInt32)eDSAuthParameterError );
                        
						// special-case for an empty password. DIGEST-MD5 does not
						// support empty passwords, but it's a DS requirement
						if ( *paramStr == '\0' )
						{
							free( paramStr );
							paramStr = (char *) malloc( strlen(kEmptyPasswordAltStr) + 1 );
							strcpy( paramStr, kEmptyPasswordAltStr );
						}
						
                        strcpy(buf, paramStr);
                        
                        gSASLMutex->Wait();
                        saslResult = sasl_encode(pContext->conn,
												 buf,
												 kChangePassPaddedBufferSize,
												 &encodedStr,
												 &encodedStrLen); 
                        gSASLMutex->Signal();
                    }
                    
                    if ( siResult == noErr && saslResult == SASL_OK && userName != NULL )
                    {
                        if ( ConvertBinaryTo64( encodedStr, encodedStrLen, encoded64Str ) == SASL_OK )
                        {
							StripRSAKey( userName );
							result = SendFlushReadWithMutex( pContext, "CHANGEPASS", userName, encoded64Str, buf, sizeof(buf) );
							siResult = PWSErrToDirServiceError( result );
							if ( siResult == eDSNoErr )
								UpdateCachedPasswordOnChange( pContext, userName, paramStr, strlen(paramStr) );
                        }
                    }
                    else
                    {
                        printf("encode64 failed\n");
                    }
                    break;
                
                case kAuthNewUser:
                    #pragma mark kAuthNewUser
                    // buffer format is:
                    // len1 AuthenticatorID
                    // len2 AuthenticatorPW
                    // len3 user's name
                    // len4 user's initial password
                    siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &userIDToSet );
                    if ( siResult == noErr )
                        siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 4, &paramStr );
                    if ( siResult == noErr )
					{
                        if ( userIDToSet == NULL || paramStr == NULL )
                            throw( (sInt32)eDSInvalidBuffFormat );
						if ( strlen(paramStr) > kChangePassPaddedBufferSize )
                            throw( (sInt32)eDSAuthParameterError );
                        
						// special-case for an empty password. DIGEST-MD5 does not
						// support empty passwords, but it's a DS requirement
						if ( *paramStr == '\0' )
						{
							free( paramStr );
							paramStr = (char *) malloc( strlen(kEmptyPasswordAltStr) + 1 );
							strcpy( paramStr, kEmptyPasswordAltStr );
						}
						
						strcpy(buf, paramStr);
						
                        gSASLMutex->Wait();
                        saslResult = sasl_encode(pContext->conn,
												 buf,
												 kChangePassPaddedBufferSize,
												 &encodedStr,
												 &encodedStrLen); 
                        gSASLMutex->Signal();
					}
					
                    if ( siResult == noErr && saslResult == SASL_OK )
                    {
                        if ( ConvertBinaryTo64( encodedStr, encodedStrLen, encoded64Str ) == SASL_OK )
                        {
                            if ( pContext->rsaPublicKeyStr == NULL )
                                throw( (sInt32)eDSAuthServerError );
                            
							result = SendFlushReadWithMutex( pContext, "NEWUSER", userIDToSet, encoded64Str, buf, sizeof(buf) );
							if ( result.err != 0 )
								throw( PWSErrToDirServiceError(result) );
							
							sasl_chop( buf );
							
							// use encodedStrLen; it's available
                            encodedStrLen = strlen(buf) + 1 + strlen(pContext->rsaPublicKeyStr);
							if ( encodedStrLen > outBuf->fBufferSize )
								throw( (sInt32)eDSBufferTooSmall );
							
							// put a 4-byte length in the buffer
							encodedStrLen -= 4;
							memcpy( outBuf->fBufferData, &encodedStrLen, 4 );
							outBuf->fBufferLength = 4;
							
							// copy the ID
							encodedStrLen = strlen(buf+4);
							memcpy( outBuf->fBufferData + outBuf->fBufferLength, buf+4, encodedStrLen );
							outBuf->fBufferLength += encodedStrLen;
							
							// add a separator
							outBuf->fBufferData[outBuf->fBufferLength] = ',';
							outBuf->fBufferLength++;
							
							// copy the public key
							strcpy( outBuf->fBufferData + outBuf->fBufferLength, pContext->rsaPublicKeyStr );
							outBuf->fBufferLength += strlen(pContext->rsaPublicKeyStr);
                        }
                    }
                    else
                    {
                        printf("encode64 failed\n");
                    }
                    break;
                    
                case kAuthGetPolicy:
                    #pragma mark kAuthGetPolicy
                    // buffer format is:
                    // len1 AuthenticatorID
                    // len2 AuthenticatorPW
                    // len3 AccountID
                    siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &userIDToSet );
                    if ( siResult == noErr )
                    {
                        if ( userIDToSet == NULL )
                            throw( (sInt32)eDSInvalidBuffFormat );
                        
                        StripRSAKey( userIDToSet );
						
						result = SendFlushReadWithMutex( pContext, "GETPOLICY", userIDToSet, NULL, buf, sizeof(buf) );
						if ( result.err != 0 )
							throw( PWSErrToDirServiceError(result) );
						
						siResult = PackStepBuffer( buf, true, NULL, NULL, NULL, outBuf );
                    }
                    break;
				
				case kAuthGetEffectivePolicy:
					#pragma mark kAuthGetEffectivePolicy
                    // buffer format is:
                    // len1 AccountID
					
			        if ( userName == NULL )
						throw( (sInt32)eDSInvalidBuffFormat );
                        
					StripRSAKey( userName );
						
			 		result = SendFlushReadWithMutex( pContext, "GETPOLICY", userName, "ACTUAL", buf, sizeof(buf) );
					siResult = PWSErrToDirServiceError( result );
					if ( siResult == eDSNoErr )
						siResult = PackStepBuffer( buf, true, NULL, NULL, NULL, outBuf );
                    break;
					
                case kAuthSetPolicy:
                    #pragma mark kAuthSetPolicy
                    // buffer format is:
                    // len1 AuthenticatorID
                    // len2 AuthenticatorPW
                    // len3 AccountID
                    // len4 PolicyString
                    siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &userIDToSet );
                    if ( siResult == noErr ) 
					{
						StripRSAKey(userIDToSet);
                            
                        siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 4, &paramStr );
					}
					
                    if ( siResult == noErr )
                    {
						result = SendFlushReadWithMutex( pContext, "SETPOLICY", userIDToSet, paramStr, buf, sizeof(buf) );
						siResult = PWSErrToDirServiceError( result );
                    }
                    break;
                    
				case kAuthSetPolicyAsRoot:
					#pragma mark kAuthSetPolicyAsRoot
                    // buffer format is:
                    // len1 AccountID
                    // len2 PolicyString
					siResult = Get2StringsFromAuthBuffer( inData->fInAuthStepData, &userIDToSet, &paramStr );
                    if ( siResult == noErr )
                    {
						StripRSAKey( userIDToSet );
						result = SendFlushReadWithMutex( pContext, "SETPOLICY", userIDToSet, paramStr, buf, sizeof(buf) );
						siResult = PWSErrToDirServiceError( result );
                    }
					break;
				
                case kAuthGetGlobalPolicy:
                    #pragma mark kAuthGetGlobalPolicy
                    // buffer format is:
                    // len1 AuthenticatorID
                    // len2 AuthenticatorPW
					
					result = SendFlushReadWithMutex( pContext, "GETGLOBALPOLICY", NULL, NULL, buf, sizeof(buf) );
					if ( result.err != 0 )
						throw( PWSErrToDirServiceError(result) );
					
					siResult = PackStepBuffer( buf, true, NULL, NULL, NULL, outBuf );
                    break;
                    
                case kAuthSetGlobalPolicy:
                    #pragma mark kAuthSetGlobalPolicy
                    // buffer format is:
                    // len1 AuthenticatorID
                    // len2 AuthenticatorPW
                    // len3 PolicyString
                    siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &paramStr );
                    if ( siResult == noErr )
                    {
						result = SendFlushReadWithMutex( pContext, "SETGLOBALPOLICY", paramStr, NULL, buf, sizeof(buf) );
						siResult = PWSErrToDirServiceError( result );
                    }
                    break;
                    
                case kAuthGetUserName:
                    #pragma mark kAuthGetUserName
                    // buffer format is:
                    // len1 AuthenticatorID
                    // len2 AuthenticatorPW
                    // len3 AccountID
                    siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &userIDToSet );
					if ( siResult == noErr )
                    {
                        StripRSAKey( userIDToSet );
                        
						result = SendFlushReadWithMutex( pContext, "GETUSERNAME", userIDToSet, NULL, buf, sizeof(buf) );
						if ( result.err != 0 )
							throw( PWSErrToDirServiceError(result) );
						
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
                    siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &userIDToSet );
					if ( siResult == noErr )
                        siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 4, &paramStr );
                    if ( siResult == noErr )
                    {
                        StripRSAKey( userIDToSet );
                        
						result = SendFlushReadWithMutex( pContext, "SETUSERNAME", userIDToSet, paramStr, buf, sizeof(buf) );
						siResult = PWSErrToDirServiceError( result );
                    }
                    break;
                    
                case kAuthGetUserData:
                    #pragma mark kAuthGetUserData
                    // buffer format is:
                    // len1 AuthenticatorID
                    // len2 AuthenticatorPW
                    // len3 AccountID
                    siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &userIDToSet );
					if ( siResult == noErr )
                    {
                        char *outData = NULL;
                        unsigned long outDataLen;
                        unsigned long decodedStrLen;
                        
                        StripRSAKey( userIDToSet );
                        
						result = SendFlushReadWithMutex( pContext, "GETUSERDATA", userIDToSet, NULL, buf, sizeof(buf) );
						siResult = PWSErrToDirServiceError( result );
						
                        if ( siResult == eDSNoErr )
                        {
                            // base64 decode user data
							outDataLen = strlen( buf );
                            outData = (char *)malloc( outDataLen );
                            Throw_NULL( outData, eMemoryError );
                            
                            if ( Convert64ToBinary( buf, outData, outDataLen, &decodedStrLen ) == 0 )
                            {
								if ( decodedStrLen <= outBuf->fBufferSize )
                                {
                                    ::memcpy( outBuf->fBufferData, &decodedStrLen, 4 );
                                    ::memcpy( outBuf->fBufferData + 4, outData, decodedStrLen );
                                    outBuf->fBufferLength = decodedStrLen;
                                }
                                else
                                {
                                    siResult = eDSBufferTooSmall;
                                }
                            }
                            
                            free(outData);
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
                        
                        siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &userIDToSet );
						if ( siResult == noErr )
                        {
                            StripRSAKey(userIDToSet);
                            
							tptr = inData->fInAuthStepData->fBufferData;
                            
                            for (int repeatCount = 3; repeatCount > 0; repeatCount--)
                            {
                                memcpy(&dataSegmentLen, tptr, 4);
                                tptr += 4 + dataSegmentLen;
                            }
                            
                            memcpy(&dataSegmentLen, tptr, 4);
                            
                            paramStr = (char *)malloc( dataSegmentLen * 4/3 + 20 );
                            Throw_NULL( paramStr, eMemoryError );
							
                            // base64 encode user data
							siResult = ConvertBinaryTo64( tptr, dataSegmentLen, paramStr );
                        }
                        
                        if ( siResult == noErr )
                        {
                            result = SendFlushReadWithMutex( pContext, "SETUSERDATA", userIDToSet, paramStr, buf, sizeof(buf) );
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
                    siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &userIDToSet );
					if ( siResult == noErr )
                    {
                        StripRSAKey( userIDToSet );
                        
						result = SendFlushReadWithMutex( pContext, "DELETEUSER", userIDToSet, NULL, buf, sizeof(buf) );
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
					
					GetStringFromAuthBuffer( inData->fInAuthStepData, 4, &userIDToSet );
					
                    siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &paramStr );
					if ( siResult == noErr )
                    {
						result = SendFlushReadWithMutex( pContext, "GETIDBYNAME", paramStr, userIDToSet, buf, sizeof(buf) );
						if ( result.err != 0 )
							throw( PWSErrToDirServiceError(result) );
						
                        sasl_chop(buf);
                        
                        // add the public rsa key
                        if ( pContext->rsaPublicKeyStr )
                        {
                            strcat(buf, ",");
                            strcat(buf, pContext->rsaPublicKeyStr);
                        }
                        
						siResult = PackStepBuffer( buf, true, NULL, NULL, NULL, outBuf );
                    }
                    break;
				
				case kAuthGetDisabledUsers:
					#pragma mark kAuthGetDisabledUsers
					siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &paramStr );
					if ( siResult != noErr )
						throw( siResult );
					
					// Note: the password server's line limit is 1023 bytes so we may need to send
					// multiple requests.
					{
						long loopIndex;
						long byteCount = strlen( paramStr );
						long loopCount = (byteCount / 1023) + 1;
						char *sptr = paramStr;
						char *tptr;
						
						outBuf->fBufferLength = 0;
						
						for ( loopIndex = 0; loopIndex < loopCount; loopIndex++ )
						{
							strncpy( buf, sptr, 1023 );
							buf[1023] = '\0';
							
							if ( byteCount > 1023 )
							{
								tptr = buf + 1022;
								while ( (*tptr != ' ') && (tptr > buf) )
									*tptr-- = '\0';
								
								sptr += strlen( buf );
								byteCount = strlen(paramStr) - (sptr - paramStr);
							}
							
							DEBUGLOG( "ulist: %s\n", buf );
							result = SendFlushReadWithMutex( pContext, "GETDISABLEDUSERS", buf, NULL, buf, sizeof(buf) );
							if ( result.err != 0 )
								throw( PWSErrToDirServiceError(result) );
							
							// use encodedStrLen; it's available
							encodedStrLen = strlen( buf );
							if ( outBuf->fBufferLength + encodedStrLen > outBuf->fBufferSize - 4 )
								throw( (sInt32)eDSBufferTooSmall );
							
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
					StripRSAKey( userName );
					siResult = ConvertBinaryTo64( password, 8, encoded64Str );
					if ( siResult == noErr )
					{
						char desDataBuf[50];
						
						snprintf( desDataBuf, sizeof(desDataBuf), "%s ", encoded64Str );
						
						siResult = ConvertBinaryTo64( password + 8, 8, encoded64Str );
						if ( siResult == noErr )
						{
							strcat( desDataBuf, encoded64Str );
							strcat( desDataBuf, "\r\n" );
							
							result = SendFlushReadWithMutex( pContext, "TWRNDCHANGEPASS", userName, desDataBuf, buf, sizeof(buf) );
						}
						
						if ( siResult == noErr && result.err != 0 )
							siResult = PWSErrToDirServiceError( result );
					}
					break;
					
				case kAuthSyncSetupReplica:
					#pragma mark kAuthSyncSetupReplica
					
					siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &paramStr );
					if ( siResult == noErr )
                    {
						result = SendFlushReadWithMutex( pContext, "SYNC SETUPREPLICA", paramStr, NULL, buf, sizeof(buf) );
						if ( result.err != 0 )
							throw( PWSErrToDirServiceError(result) );
						
						sasl_chop( buf );
						
						if ( strcasecmp( paramStr, "GET" ) == 0 )
						{
							const char *decodedStr;
							unsigned long decodedStrLen;
							unsigned decryptedStrLen;
							
							decodedStrLen = strlen(buf + 4);
							stepData = (char *) malloc( decodedStrLen + 9 );
							if ( stepData == NULL )
								throw( (sInt32)eMemoryError );
							
							// decode
							if ( Convert64ToBinary( buf + 4, stepData, decodedStrLen, &decodedStrLen ) != SASL_OK )
								throw( (sInt32)eDSAuthServerError );
							
							if (decodedStrLen % 8)
								decodedStrLen += 8 - (decodedStrLen % 8);
							
							// decrypt
							gSASLMutex->Wait();
							saslResult = sasl_decode(pContext->conn,
														stepData,
														decodedStrLen,
														&decodedStr,
														&decryptedStrLen);
							gSASLMutex->Signal();
							
							// use encodedStrLen; it's available
							encodedStrLen = 4 + decryptedStrLen + 4 + strlen(pContext->rsaPublicKeyStr);
							if ( encodedStrLen > outBuf->fBufferSize )
								throw( (sInt32)eDSBufferTooSmall );
							
							// put a 4-byte length in the buffer
							decodedStrLen = decryptedStrLen;
							memcpy( outBuf->fBufferData, &decodedStrLen, 4 );
							outBuf->fBufferLength = 4;
							
							// copy the private key
							memcpy( outBuf->fBufferData + outBuf->fBufferLength, decodedStr, decryptedStrLen );
							outBuf->fBufferLength += decryptedStrLen;
							
							// put a 4-byte length in the buffer
							encodedStrLen = strlen( pContext->rsaPublicKeyStr );
							memcpy( outBuf->fBufferData + outBuf->fBufferLength, &encodedStrLen, 4 );
							outBuf->fBufferLength += 4;
							
							// copy the public key
							strcpy( outBuf->fBufferData + outBuf->fBufferLength, pContext->rsaPublicKeyStr );
							outBuf->fBufferLength += encodedStrLen;
						}
					}
					break;
					
				case kAuthListReplicas:
					#pragma mark kAuthListReplicas
					gPWSConnMutex->Wait();
					siResult = DoAuthMethodListReplicas( inData, pContext, outBuf );
					gPWSConnMutex->Signal();
					break;
					
				case kAuthPull:
					#pragma mark kAuthPull
					siResult = DoAuthMethodPull( inData, pContext, outBuf );
					break;
					
				case kAuthPush:
					#pragma mark kAuthPush
					siResult = DoAuthMethodPush( inData, pContext, outBuf );
					break;
					
				case kAuthProcessNoReply:
					#pragma mark kAuthProcessNoReply
					// get size from somewhere
					sprintf( buf, "%lu", pContext->pushByteCount );
					pContext->pushByteCount = 0;
					result = SendFlushReadWithMutex( pContext, "SYNC PROCESS-NO-REPLY", buf, NULL, buf, sizeof(buf) );
					siResult = PWSErrToDirServiceError( result );
					break;
					
				case kAuthSMB_NTUserSessionKey:
					#pragma mark kAuthSMB_NTUserSessionKey
					siResult = DoAuthMethodNTUserSessionKey( inData, pContext, outBuf );
					break;
					
				case kAuthSMBWorkstationCredentialSessionKey:
					#pragma mark kAuthSMBWorkstationCredentialSessionKey
					{
						long paramLen;
						const char *decryptedStr;
						unsigned long decodedStrLen;
						unsigned decryptedStrLen;
						char base64Param[30];
						
						siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 1, &userIDToSet );
						if ( siResult == noErr )
							siResult = GetDataFromAuthBuffer( inData->fInAuthStepData, 2, (unsigned char **)&paramStr, &paramLen );
						if ( siResult == noErr )
						{
							if ( userIDToSet == NULL || paramLen > 16 )
								throw( (sInt32)eDSInvalidBuffFormat );
							
							StripRSAKey( userIDToSet );
							
							if ( ConvertBinaryTo64( paramStr, paramLen, base64Param ) != SASL_OK )
								throw( (sInt32)eParameterError );
							
							result = SendFlushReadWithMutex( pContext, "GETWCSK", userIDToSet, base64Param, buf, sizeof(buf) );
							if ( result.err != 0 )
								throw( PWSErrToDirServiceError(result) );
							
							decodedStrLen = strlen( buf ) - 4;
							if ( decodedStrLen < 2 )
								throw( (sInt32)eDSAuthServerError );
							
							stepData = (char *) malloc( decodedStrLen );
							if ( stepData == NULL )
								throw( (sInt32)eMemoryError );
							
							// decode
							if ( Convert64ToBinary( buf + 4, stepData, decodedStrLen, &decodedStrLen ) != SASL_OK )
								throw( (sInt32)eDSAuthServerError );
							
							// decrypt
							gSASLMutex->Wait();
							saslResult = sasl_decode( pContext->conn,
														stepData,
														decodedStrLen,
														&decryptedStr,
														&decryptedStrLen);
							gSASLMutex->Signal();
							
							if ( outBuf->fBufferSize < decryptedStrLen + 4 )
								throw( (sInt32)eDSBufferTooSmall );
							
							outBuf->fBufferLength = decryptedStrLen + 4;
							decodedStrLen = decryptedStrLen;
							memcpy( outBuf->fBufferData, &decodedStrLen, 4 );
							memcpy( outBuf->fBufferData + 4, decryptedStr, decryptedStrLen );
						}
					}
					break;
					
				case kAuthNTSetWorkstationPasswd:
					#pragma mark kAuthNTSetWorkstationPasswd
					{
						long paramLen;
						
						siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 1, &userIDToSet );
						if ( siResult == noErr )
							siResult = GetDataFromAuthBuffer( inData->fInAuthStepData, 2, (unsigned char **)&paramStr, &paramLen );
						if ( siResult != noErr )
							throw( siResult );
						
						memcpy( buf, paramStr, paramLen );
						
						gSASLMutex->Wait();
                        saslResult = sasl_encode(pContext->conn,
												 buf,
												 kChangePassPaddedBufferSize,
												 &encodedStr,
												 &encodedStrLen); 
                        gSASLMutex->Signal();
						
						if ( siResult == noErr && saslResult == SASL_OK && userIDToSet != NULL )
						{
							if ( ConvertBinaryTo64( encodedStr, encodedStrLen, encoded64Str ) == SASL_OK )
							{
								result = SendFlushReadWithMutex( pContext, "CHANGENTHASH", userIDToSet, encoded64Str, buf, sizeof(buf) );
								siResult = PWSErrToDirServiceError( result );
							}
						}
					}
					break;
				
				case kAuthGetKerberosPrincipal:
					#pragma mark kAuthGetKerberosPrincipal
					
					siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 1, &userIDToSet );
					if ( siResult != eDSNoErr )
						throw( siResult );
					
					result = SendFlushReadWithMutex( pContext, "GETKERBPRINC", userIDToSet, NULL, buf, sizeof(buf) );
					siResult = PWSErrToDirServiceError( result );
					if ( siResult != eDSNoErr )
						throw( siResult );
					
					siResult = PackStepBuffer( buf, true, NULL, NULL, NULL, outBuf );
					break;
				
				case kAuthVPN_PPTPMasterKeys:
					#pragma mark kAuthVPN_PPTPMasterKeys
					
					siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 1, &userIDToSet );
                    if ( siResult == noErr )
						siResult = GetDataFromAuthBuffer( inData->fInAuthStepData, 2, (unsigned char **)&paramStr, &passwordLen );
                    if ( siResult == noErr )
					{
						ConvertBinaryToHex( (unsigned char *)paramStr, passwordLen, buf );
						free( paramStr );
						paramStr = NULL;
						siResult = GetDataFromAuthBuffer( inData->fInAuthStepData, 3, (unsigned char **)&paramStr, &passwordLen );
						if ( siResult == noErr && passwordLen == 1 && (paramStr[0] == 8 || paramStr[0] == 16) )
						{
							strcat( buf, " " );
							strcat( buf, (paramStr[0] == 8) ? "8" : "16" );
						}
						else
						{
							if ( siResult == eDSNoErr )
								siResult = eDSInvalidBuffFormat;
						}
                    }
					
					if ( siResult == noErr )
					{
                        const char *decryptedStr;
						unsigned long buffItemLen;
						unsigned decryptedStrLen;
						
						if ( userIDToSet == NULL )
                            throw( (sInt32)eDSInvalidBuffFormat );
                        
                        StripRSAKey( userIDToSet );
						
						result = SendFlushReadWithMutex( pContext, "GETPPTPKEYS", userIDToSet, buf, buf, sizeof(buf) );
						if ( result.err != 0 )
							throw( PWSErrToDirServiceError(result) );
						
						buffItemLen = strlen( buf ) - 4;
						if ( buffItemLen < 2 )
							throw( (sInt32)eDSAuthServerError );
						
						stepData = (char *) malloc( buffItemLen );
						if ( stepData == NULL )
							throw( (sInt32)eMemoryError );
						
						// decode
						if ( Convert64ToBinary( buf + 4, stepData, buffItemLen, &buffItemLen ) != SASL_OK )
							throw( (sInt32)eDSAuthServerError );
						
						// decrypt
						gSASLMutex->Wait();
						saslResult = sasl_decode( pContext->conn,
													stepData,
													buffItemLen,
													&decryptedStr,
													&decryptedStrLen);
						gSASLMutex->Signal();
						
						if ( outBuf->fBufferSize < decryptedStrLen + 4 )
							throw( (sInt32)eDSBufferTooSmall );
						
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
					siResult = DoAuthMethodMSChapChangePass( inData, pContext );
					break;
					
				case kAuthEncryptToUser:
					#pragma mark kAuthEncryptToUser
					siResult = DoAuthMethodEncryptToUser( inData, pContext, outBuf );
					break;
				
				case kAuthDecrypt:
					#pragma mark kAuthDecrypt
					siResult = DoAuthMethodDecrypt( inData, pContext, outBuf );
					break;
            }
        }
		
		if ( fOpenNodeCount >= kMaxOpenNodesBeforeQuickClose && inData->fInDirNodeAuthOnlyFlag == true )
		{
			if ( uiAuthMethod == kAuthClearText ||
				 uiAuthMethod == kAuthNativeClearTextOK ||
				 uiAuthMethod == kAuthNativeNoClearText ||
				 uiAuthMethod == kAuthAPOP ||
				 uiAuthMethod == kAuthSMB_NT_Key ||
				 uiAuthMethod == kAuthSMB_LM_Key ||
				 uiAuthMethod == kAuthDIGEST_MD5 ||
				 uiAuthMethod == kAuthCRAM_MD5 ||
				 uiAuthMethod == kAuthMSCHAP2 )
			{
				EndServerSession( pContext, kSendQuit );
			}
		}
	}
	catch ( sInt32 err )
	{
		siResult = err;
	}
	
	inData->fResult = siResult;

    if ( userName != NULL )
        free( userName );
    if ( password != NULL ) {
		bzero( password, passwordLen );
        free( password );
	}
    if ( userIDToSet != NULL )
        free( userIDToSet );
    if ( paramStr != NULL )
        free( paramStr );
    if ( stepData != NULL )
		free( stepData );
	
    DEBUGLOG( "CPSPlugIn::DoAuthentication returning %l\n", siResult);
	return( siResult );

} // DoAuthentication


#pragma mark -

// ---------------------------------------------------------------------------
//	* DoAuthMethodListReplicas
// ---------------------------------------------------------------------------

sInt32 CPSPlugIn::DoAuthMethodListReplicas( sDoDirNodeAuth *inData, sPSContextData *inContext, tDataBufferPtr outBuf )
{
	sInt32				siResult				= eDSNoErr;
	sPSContinueData		*pContinue				= NULL;
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
	
	try
	{
		if ( outBuf->fBufferSize < 5 )
			throw( (sInt32)eDSBufferTooSmall );
		
		// repeat visit?
		if ( inData->fIOContinueData != NULL )
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
				inData->fIOContinueData = NULL;
			}
		}
		else
		{
			result = SendFlush( inContext, "LISTREPLICAS", NULL, NULL );
			if ( result.err != 0 )
				throw( PWSErrToDirServiceError(result) );
			
			// LISTREPLICAS always comes back without encryption
			result = readFromServer( inContext->fd, buf, sizeof(buf) );
			if ( result.err != 0 )
				throw( PWSErrToDirServiceError(result) );
			
			sscanf( buf + 4, "%lu", &replicaDataLen );
			
			bufPtr = strchr( buf + 4, ' ' );
			if ( bufPtr == NULL )
				throw( (sInt32)eDSAuthServerError );
			
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
			}
			
			// Do we need a continue data?
			if ( 4 + replicaDataLen > outBuf->fBufferSize )
			{
				if ( inData->fIOContinueData == NULL )
				{
					pContinue = (sPSContinueData *)::calloc( 1, sizeof( sPSContinueData ) );
					Throw_NULL( pContinue, eMemoryError );
					
					gContinue->AddItem( pContinue, inData->fInNodeRef );
					inData->fIOContinueData = pContinue;
				}
				else
				{
					throw( (sInt32)eDSInvalidContinueData );
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
	catch( sInt32 error )
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

sInt32 CPSPlugIn::DoAuthMethodPull( sDoDirNodeAuth *inData, sPSContextData *inContext, tDataBufferPtr outBuf )
{
	const int			slush				= 100;
	sInt32				siResult			= eDSNoErr;
	char				*paramStr			= NULL;
	char				*bigbuff			= NULL;
	char				*tptr				= NULL;
	unsigned long		syncDataLen			= 0;
	unsigned long		readLen				= 0;
	FILE				*fp					= NULL;
	PWServerError		result;
	struct timeval		now;
	struct timezone		tz					= { 0, 0 };	// GMT
	struct stat			sb;
	unsigned char		iv[34];
	char				buf[kOneKBuffer];
	char				cmdBuf[256];
	
	try
	{
		siResult = SetupSecureSyncSession( inContext );
		if ( siResult != eDSNoErr )
			throw( siResult );
		
		siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 1, &paramStr );
		if ( siResult != noErr )
			throw( (sInt32)eParameterError );
		
		result = SendFlushReadWithMutex( inContext, "SYNC PULL", paramStr, NULL, buf, sizeof(buf) );
		if ( result.err != 0 )
			throw( PWSErrToDirServiceError(result) );
		
		sasl_chop( buf );
		if ( strncmp( buf, "+MORE ", 6 ) != 0 || buf[6] == '\0' )
			throw( (sInt32)eDSAuthServerError );
		
		// receive the sync blob			
		sscanf( buf + 6, "%lu", &syncDataLen );
		if ( syncDataLen <= 0 )
			throw( (sInt32)eDSAuthServerError );
		
		bigbuff = (char *) malloc( syncDataLen + slush );
		if ( bigbuff == NULL )
			throw( (sInt32)eMemoryError );
		
		tptr = bigbuff;
		
		strcpy( (char *)iv, "D5F:A24A" );
		
		while ( true )
		{
			result = readFromServer( inContext->fd, buf, sizeof(buf) );
			if ( result.err != 0 )
			{
				free( bigbuff );
				throw( PWSErrToDirServiceError(result) );
			}
			
			sasl_chop( buf );
			if ( strncmp( buf, "+MORE ", 6 ) == 0 )
			{
				readLen = strlen( buf + 6 );
				if ( readLen == 0 )
					break;
				
				if ( Convert64ToBinary( buf + 6, tptr, syncDataLen - (tptr - bigbuff) + 16, &readLen ) != SASL_OK )
				{
					DEBUGLOG( "maxLen=%l\n", syncDataLen - (tptr - bigbuff) + 16);
					throw( (sInt32)eDSAuthServerError );
				}
				
				RC5_32_cbc_encrypt( (unsigned char *)tptr, (unsigned char *)buf, readLen, &inContext->rc5Key, iv, RC5_DECRYPT );
				memcpy( tptr, buf, readLen );
				
				tptr += readLen;
			}
			else
			{
				// must be +OK because -ERR is caught by readFromServer()
				break;
			}
		}
		
		// save the gzipped sync file
		gettimeofday( &now, &tz );
		inContext->syncFilePath = (char *) malloc( strlen(kDSTempSyncFileControlStr) + 40 );
		if ( inContext->syncFilePath == NULL )
			throw( (sInt32)eMemoryError );
		
		sprintf( inContext->syncFilePath, kDSTempSyncFileControlStr, (long)now.tv_sec, (long)now.tv_usec );
		fp = fopen( inContext->syncFilePath, "w+" );
		if ( fp == NULL )
		{
			free( bigbuff );
			DEBUGLOG( "CPSPlugIn::could not create a sync file\n");
			throw( (sInt32)ePlugInError );
		}
		
		fwrite( bigbuff, syncDataLen, 1, fp );
		fclose( fp );
		
		free( bigbuff );
		
		// unzip
		sprintf( cmdBuf, "/usr/bin/gunzip %s", inContext->syncFilePath );
		fp = popen( cmdBuf, "r" );
		if ( fp == NULL )
		{
			free( bigbuff );
			DEBUGLOG( "CPSPlugIn::could not gunzip a sync file\n");
			throw( (sInt32)ePlugInError );
		}
		
		pclose( fp );
		
		// get the final size
		*(inContext->syncFilePath + strlen(inContext->syncFilePath) - 3) = '\0';
		siResult = stat( inContext->syncFilePath, &sb );
		if ( siResult != 0 )
		{
			DEBUGLOG( "CPSPlugIn::could not stat the sync file.\n");
			throw( (sInt32)ePlugInError );
		}
		
		syncDataLen = strlen( inContext->syncFilePath );
		memcpy( outBuf->fBufferData, &syncDataLen, 4 );
		memcpy( outBuf->fBufferData + 4, inContext->syncFilePath, syncDataLen );
		outBuf->fBufferLength = 4 + syncDataLen;
	}
	catch( sInt32 error )
	{
		siResult = error;
	}
	catch( ... )
	{
	}
	
	return siResult;
}
    
	
// ---------------------------------------------------------------------------
//	* DoAuthMethodPush
// ---------------------------------------------------------------------------

sInt32 CPSPlugIn::DoAuthMethodPush( sDoDirNodeAuth *inData, sPSContextData *inContext, tDataBufferPtr outBuf )
{
	sInt32				siResult			= eDSNoErr;
	char				*paramStr			= NULL;
	PWServerError		result;
	char				buf[kOneKBuffer];
	
	try
	{
		siResult = SetupSecureSyncSession( inContext );
		if ( siResult != eDSNoErr )
			throw( siResult );
		
		{
			unsigned char *syncData;
			unsigned char *encSyncData = NULL;
			long syncDataLen;
			
			siResult = GetDataFromAuthBuffer( inData->fInAuthStepData, 1, &syncData, &syncDataLen );
			if ( siResult != eDSNoErr )
				throw( siResult );
			if ( syncData == NULL )
				throw( (sInt32)eDSInvalidBuffFormat );
			
			inContext->pushByteCount += syncDataLen;
			
			// BEGIN no throw zone
			
			// ok, this is annoying. The pointer returned from GetDataFromAuthBuffer() has no extra
			// space but RC5 encrypts blocks of 8. We must copy the data and pad with zeros.
			memcpy( buf, syncData, syncDataLen );
			bzero( buf + syncDataLen, 8 );
			if ( (syncDataLen % 8) != 0 )
				syncDataLen = (syncDataLen/8)*8 + 8;
			
			free( syncData );
			
			encSyncData = (unsigned char *) calloc( 1, syncDataLen + 8 );
			if ( encSyncData == NULL )
				siResult = eMemoryError;
			
			if ( siResult == eDSNoErr )
			{
				RC5_32_cbc_encrypt( (unsigned char *)buf, encSyncData, syncDataLen, &inContext->rc5Key, inContext->psIV, RC5_ENCRYPT );
				paramStr = (char *)malloc( syncDataLen * 4/3 + 20 );
				if ( paramStr == NULL )
					siResult = eMemoryError;
			}
			
			if ( siResult == eDSNoErr )
				siResult = ConvertBinaryTo64( (char *)encSyncData, syncDataLen, paramStr );
			
			if ( encSyncData != NULL )
				free( encSyncData );
			
			// END no throw zone
			
			if ( siResult == noErr )
			{
				result = SendFlushReadWithMutex( inContext, "SYNC PUSH", paramStr, NULL, buf, sizeof(buf) );
				siResult = PWSErrToDirServiceError( result );
			}
		}
	}
	catch( sInt32 error )
	{
		siResult = error;
	}
	catch( ... )
	{
	}
	
	if ( paramStr != NULL )
		free( paramStr );
	
	return siResult;
}


// ---------------------------------------------------------------------------
//	* DoAuthMethodNTUserSessionKey
// ---------------------------------------------------------------------------

sInt32 CPSPlugIn::DoAuthMethodNTUserSessionKey( sDoDirNodeAuth *inData, sPSContextData *inContext, tDataBufferPtr outBuf )
{
	sInt32				siResult			= eDSNoErr;
	char				*userIDToSet		= NULL;
	char				*paramStr			= NULL;
	char				*stepData			= NULL;
	int					saslResult			= SASL_OK;
	PWServerError		result;
	char				buf[kOneKBuffer];
	
	try
	{
		siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 1, &userIDToSet );
		if ( siResult == noErr )
		{
			const char *decryptedStr;
			unsigned long decodedStrLen;
			unsigned decryptedStrLen;
			
			if ( userIDToSet == NULL )
				throw( (sInt32)eDSInvalidBuffFormat );
			
			StripRSAKey( userIDToSet );
			
			result = SendFlushReadWithMutex( inContext, "GETNTHASHHASH", userIDToSet, NULL, buf, sizeof(buf) );
			if ( result.err != 0 )
				throw( PWSErrToDirServiceError(result) );
			
			decodedStrLen = strlen( buf ) - 4;
			if ( decodedStrLen < 2 )
				throw( (sInt32)eDSAuthServerError );
			
			stepData = (char *) malloc( decodedStrLen );
			if ( stepData == NULL )
				throw( (sInt32)eMemoryError );
			
			// decode
			if ( Convert64ToBinary( buf + 4, stepData, decodedStrLen, &decodedStrLen ) != SASL_OK )
				throw( (sInt32)eDSAuthServerError );
			
			// decrypt
			gSASLMutex->Wait();
			saslResult = sasl_decode( inContext->conn,
										stepData,
										decodedStrLen,
										&decryptedStr,
										&decryptedStrLen);
			gSASLMutex->Signal();
			
			if ( saslResult != SASL_OK )
				throw( (sInt32)eDSAuthFailed );
			
			if ( outBuf->fBufferSize < decryptedStrLen + 4 )
				throw( (sInt32)eDSBufferTooSmall );
			
			outBuf->fBufferLength = decryptedStrLen + 4;
			decodedStrLen = decryptedStrLen;
			memcpy( outBuf->fBufferData, &decodedStrLen, 4 );
			memcpy( outBuf->fBufferData + 4, decryptedStr, decryptedStrLen );
		}
	}
	catch( sInt32 error )
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
	if ( stepData != NULL )
		free( stepData );
	
	return siResult;
}


// ---------------------------------------------------------------------------
//	* DoAuthMethodMSChapChangePass
// ---------------------------------------------------------------------------

sInt32 CPSPlugIn::DoAuthMethodMSChapChangePass( sDoDirNodeAuth *inData, sPSContextData *inContext )
{
	sInt32				siResult			= eDSNoErr;
	char				*userIDToSet		= NULL;
	char				*paramStr			= NULL;
	long 				paramLen			= 0;
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
			sprintf( encoded64Str, "%ld ", encoding );
			siResult = ConvertBinaryTo64( paramStr, paramLen, encoded64Str + strlen(encoded64Str) );
			if ( siResult != eDSNoErr )
				throw( siResult );
			
			result = SendFlushReadWithMutex( inContext, "MSCHAPCHANGEPASS", userIDToSet, encoded64Str, buf, sizeof(buf) );
			if ( result.err != 0 )
				throw( PWSErrToDirServiceError(result) );
		}
	}
	catch( sInt32 error )
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

sInt32 CPSPlugIn::DoAuthMethodEncryptToUser( sDoDirNodeAuth *inData, sPSContextData *inContext, tDataBufferPtr outBuf )
{
	sInt32				siResult			= eDSNoErr;
	char				*userIDToSet		= NULL;
	unsigned char		*dataToEncrypt		= NULL;
	long				dataToEncryptLen	= 0;
	UInt32				binBufLen			= 0;
	const char			*encodedStr			= NULL;
	unsigned int		encodedStrLen		= 0;
	int					saslResult			= SASL_OK;
	PWServerError		result;
	char				buf[kOneKBuffer];
	char				binBuf[kOneKBuffer];
	
	try
	{
		siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 1, &userIDToSet );
		if ( siResult == noErr )
        	siResult = GetDataFromAuthBuffer( inData->fInAuthStepData, 2, &dataToEncrypt, &dataToEncryptLen );
		if ( siResult != eDSNoErr )
			throw( siResult );
		
		gSASLMutex->Wait();
		saslResult = sasl_encode( inContext->conn, (char *)dataToEncrypt, dataToEncryptLen, &encodedStr, &encodedStrLen ); 
		gSASLMutex->Signal();
		
		if ( saslResult != SASL_OK )
			throw( (sInt32)eDSAuthFailed );
		
		siResult = ConvertBinaryTo64( (char *)encodedStr, encodedStrLen, buf );
		if ( siResult != noErr )
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
					throw( (sInt32)eDSBufferTooSmall );
				
				outBuf->fBufferLength = 4 + binBufLen;
				memcpy( outBuf->fBufferData, &binBufLen, 4 );
				memcpy( outBuf->fBufferData + 4, binBuf, binBufLen );
			}
		}
	}
	catch( sInt32 error )
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

sInt32 CPSPlugIn::DoAuthMethodDecrypt( sDoDirNodeAuth *inData, sPSContextData *inContext, tDataBufferPtr outBuf )
{
	sInt32				siResult			= eDSNoErr;
	unsigned char		*dataToDecrypt		= NULL;
	long				dataToDecryptLen	= 0;
	UInt32				binBufLen			= 0;
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
		if ( siResult != noErr )
			throw( siResult );
		
		result = SendFlushReadWithMutex( inContext, "CYPHER DECRYPT", buf, NULL, buf, sizeof(buf) );
		siResult = PWSErrToDirServiceError( result );
		if ( siResult != noErr )
			throw( siResult );
		
		// the buffer to sasl_decode needs to be padded to the key size (CAST128 == 16 bytes)
		// prefill with zeros
		bzero( binBuf, sizeof(binBuf) );
		siResult = Convert64ToBinary( buf + 4, binBuf, sizeof(binBuf), &binBufLen );
		if ( siResult != noErr )
			throw( siResult );
		
		// add the remainder
		if ( (binBufLen % 16) )
			binBufLen += 16 - (binBufLen % 16);
		gSASLMutex->Wait();
		saslResult = sasl_decode( inContext->conn, binBuf, binBufLen, &decodedStr, &decodedStrLen ); 
		gSASLMutex->Signal();
		if ( saslResult != SASL_OK )
			throw( (sInt32)eDSAuthFailed );
		
		if ( outBuf->fBufferSize < decodedStrLen + 4 )
			throw( (sInt32)eDSBufferTooSmall );

		binBufLen = (UInt32)decodedStrLen;
		outBuf->fBufferLength = binBufLen + 4;
		memcpy( outBuf->fBufferData, &binBufLen, 4 );
		memcpy( outBuf->fBufferData + 4, decodedStr, binBufLen );
	}
	catch( sInt32 error )
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


#pragma mark -

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

sInt32 CPSPlugIn::UseCurrentAuthenticationIfPossible( sPSContextData *inContext, const char *inUserName, UInt32 inAuthMethod, Boolean *inOutHasValidAuth )
{
	sInt32			siResult				= eDSNoErr;
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
						*inOutHasValidAuth = true;
						break;
					
					case kAuthNewUser:
					case kAuthSetPasswdAsRoot:
					case kAuthSetPolicyAsRoot:
					case kAuthSMB_NTUserSessionKey:
					case kAuthSMBWorkstationCredentialSessionKey:
					case kAuthNTSetWorkstationPasswd:
					case kAuthEncryptToUser:
					case kAuthDecrypt:
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
					memset( inContext->last.password, 0, inContext->last.passwordLen );
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
	catch( sInt32 error )
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

sInt32
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

sInt32
CPSPlugIn::UnpackUsernameAndPassword(
    sPSContextData *inContext,
    UInt32 uiAuthMethod,
    tDataBufferPtr inAuthBuf,
    char **outUserName,
    char **outPassword,
    long *outPasswordLen,
    char **outChallenge )
{
    sInt32					siResult		= eDSNoErr;
    unsigned char			*challenge		= NULL;
    unsigned char			*digest			= NULL;
    char					*method			= NULL;
	char					*user			= NULL;
    long					len				= 0;
					
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
				// these methods do not have user names
				return eDSNoErr;
				break;
			
            case kAuthAPOP:
				siResult = Get2StringsFromAuthBuffer( inAuthBuf, outUserName, (char **)&challenge );
                if ( siResult == noErr )
                    siResult = GetStringFromAuthBuffer( inAuthBuf, 3, (char **)&digest );
                if ( siResult == noErr )
                {
                    if ( challenge == NULL || digest == NULL )
                    	throw( (sInt32)eDSAuthParameterError );
                    
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
                if ( siResult == noErr )
					siResult = GetDataFromAuthBuffer( inAuthBuf, 2, &digest, &len );
				if ( siResult == noErr && digest != NULL )
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
				if ( siResult == noErr )
					siResult = GetDataFromAuthBuffer( inAuthBuf, 3, &digest, &len );
				if ( siResult == noErr )
					siResult = GetStringFromAuthBuffer( inAuthBuf, 4, &method );
				if ( siResult == noErr && digest != NULL && method != NULL )
				{
					*outPassword = (char *) malloc( len + 1 );
					Throw_NULL( (*outPassword), eMemoryAllocError );
					
					// put a leading null to tell the WEBDAV-DIGEST plug-in we're sending
					// a hash.
					**outPassword = '\0';
					memcpy( (*outPassword) + 1, digest, len );
					*outPasswordLen = len + 1;
					
					*outChallenge = (char *) malloc( strlen((char *)challenge) + 10 + strlen(method) );
					strcpy( *outChallenge, (char *)challenge );
					strcat( *outChallenge, ",method=\"" );
					strcat( *outChallenge, method );
					strcat( *outChallenge, "\"" );
				}
                break;
			
			case kAuthMSCHAP2:
				siResult = Get2StringsFromAuthBuffer( inAuthBuf, outUserName, (char **)&challenge );
				if ( siResult == noErr )
					siResult = GetStringFromAuthBuffer( inAuthBuf, 3, &method );
				if ( siResult == noErr )
					siResult = GetDataFromAuthBuffer( inAuthBuf, 4, &digest, &len );
				if ( siResult == noErr )
					siResult = GetStringFromAuthBuffer( inAuthBuf, 5, &user );
				if ( siResult == noErr && challenge != NULL && digest != NULL && method != NULL && len == 24 && user != NULL )
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
			
			case kAuthCRAM_MD5:
				siResult = Get2StringsFromAuthBuffer( inAuthBuf, outUserName, outChallenge );
                if ( siResult == noErr )
                    siResult = GetDataFromAuthBuffer( inAuthBuf, 3, &digest, &len );
                if ( siResult == noErr && digest != NULL )
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
                if ( siResult == noErr )
                {
                    *outPassword = (char *)malloc(32);
                    Throw_NULL( (*outPassword), eMemoryAllocError );
                    
                    *outPasswordLen = 32;
                    
                    siResult = GetDataFromAuthBuffer( inAuthBuf, 2, &challenge, &len );
                    if ( siResult != noErr || challenge == NULL || len != 8 )
                        throw( (sInt32)eDSInvalidBuffFormat );
                    
                    siResult = GetDataFromAuthBuffer( inAuthBuf, 3, &digest, &len );
                    if ( siResult != noErr || digest == NULL || len != 24 )
                        throw( (sInt32)eDSInvalidBuffFormat );
                    
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
                    throw( (sInt32)eDSInvalidBuffFormat );
                
                *outUserName = (char*)calloc( inAuthBuf->fBufferLength + 1, 1 );
                strncpy( *outUserName, inAuthBuf->fBufferData, inAuthBuf->fBufferLength );
                (*outUserName)[inAuthBuf->fBufferLength] = '\0';
                break;
			
			case kAuth2WayRandomChangePass:
				siResult = GetStringFromAuthBuffer( inAuthBuf, 1, outUserName );
                if ( siResult == noErr )
				{
					char *tempPWStr = NULL;
					siResult = GetStringFromAuthBuffer( inAuthBuf, 2, &tempPWStr );
					if ( siResult == noErr && tempPWStr != NULL && strlen(tempPWStr) == 8 )
					{
						*outPasswordLen = 16;
						*outPassword = (char *)malloc(16);
						memcpy( *outPassword, tempPWStr, 8 );
						free( tempPWStr );
						
						siResult = GetStringFromAuthBuffer( inAuthBuf, 3, &tempPWStr );
						if ( siResult == noErr && tempPWStr != NULL && strlen(tempPWStr) == 8 )
						{
							memcpy( *outPassword + 8, tempPWStr, 8 );
							free( tempPWStr );
						}
					}
                }
				break;
				
            case kAuthSetPasswd:
                siResult = GetStringFromAuthBuffer( inAuthBuf, 3, outUserName );
                if ( siResult == noErr )
                    siResult = GetStringFromAuthBuffer( inAuthBuf, 4, outPassword );
                if ( siResult == noErr && *outPassword != NULL )
                    *outPasswordLen = strlen( *outPassword );
                break;
            
            case kAuthSetPasswdAsRoot:
			case kAuthSetPolicyAsRoot:
            case kAuthSMB_NTUserSessionKey:
            case kAuthSMBWorkstationCredentialSessionKey:
            case kAuthNTSetWorkstationPasswd:
			case kAuthVPN_PPTPMasterKeys:
			case kAuthEncryptToUser:
			case kAuthDecrypt:
			case kAuthMSLMCHAP2ChangePasswd:
                // uses current credentials
                if ( inContext->nao.successfulAuth && inContext->nao.password != NULL )
                {
                    long pwLen;
                    
                    *outUserName = (char *)malloc(kUserIDLength + 1);
                    strncpy(*outUserName, inContext->nao.username, kUserIDLength);
                    (*outUserName)[kUserIDLength] = '\0';
                    
                    pwLen = strlen(inContext->nao.password);
                    *outPassword = (char *)malloc(pwLen + 1);
                    strncpy(*outPassword, inContext->nao.password, pwLen);
                    (*outPassword)[pwLen] = '\0';
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
    
    catch ( sInt32 error )
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


sInt32
CPSPlugIn::UnpackUsernameAndPasswordDefault(
	tDataBufferPtr inAuthBuf,
	char **outUserName,
	char **outPassword,
	long *outPasswordLen )
{
    sInt32 siResult = eDSNoErr;
	
	siResult = Get2StringsFromAuthBuffer( inAuthBuf, outUserName, outPassword );
	if ( siResult == noErr && *outPassword != NULL )
		*outPasswordLen = strlen( *outPassword );
	
	if ( *outPassword == NULL || *outPasswordLen == 0 )
	{
		if ( *outPassword != NULL )
			free( *outPassword );
		*outPasswordLen = strlen( kEmptyPasswordAltStr );
		*outPassword = (char *) malloc( *outPasswordLen + 1 );
		strcpy( *outPassword, kEmptyPasswordAltStr );
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

sInt32
CPSPlugIn::GetAuthMethodConstant(
    sPSContextData *inContext,
    tDataNode *inData,
    uInt32 *outAuthMethod,
	char *outNativeAuthMethodSASLName )
{
	sInt32			siResult		= noErr;
	char		   *p				= NULL;
    sInt32			prefixLen;
    
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
        sInt32 index;
        
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
		
        for ( index = inContext->mechCount - 1; index >= 0; index-- )
        {
            if ( strcmp( p, inContext->mech[index].method ) == 0 )
            {
                if ( outNativeAuthMethodSASLName != NULL )
                    strcpy( outNativeAuthMethodSASLName, inContext->mech[index].method );
                
                *outAuthMethod = kAuthNativeMethod;
                siResult = noErr;
                break;
            }
        }
    }
    else
	if ( ::strcmp( p, kDSStdAuthClearText ) == 0 )
	{
		// Clear text auth method
		*outAuthMethod = kAuthClearText;
	}
	else
    if ( ::strcmp( p, kDSStdAuthNodeNativeClearTextOK ) == 0 )
	{
		// Node native auth method
		*outAuthMethod = kAuthNativeClearTextOK;
	}
	else
    if ( ::strcmp( p, kDSStdAuthNodeNativeNoClearText ) == 0 )
	{
		// Node native auth method
		*outAuthMethod = kAuthNativeNoClearText;
	}
	else
    if ( ::strcmp( p, kDSStdAuthSetPasswd ) == 0 )
	{
		*outAuthMethod = kAuthSetPasswd;
	}
    else
    if ( ::strcmp( p, kDSStdAuthChangePasswd ) == 0 )
	{
		*outAuthMethod = kAuthChangePasswd;
	}
    else
    if ( ::strcmp( p, kDSStdAuthSetPasswdAsRoot ) == 0 )
	{
		*outAuthMethod = kAuthSetPasswdAsRoot;
	}
	else
    if ( ::strcmp( p, kDSStdAuthAPOP ) == 0 )
	{
		// Unix crypt auth
		*outAuthMethod = kAuthAPOP;
	}
    else
    if ( ::strcmp( p, kDSStdAuth2WayRandom ) == 0 )
	{
		*outAuthMethod = kAuth2WayRandom;
	}
	else
	if ( ::strcmp( p, kDSStdAuth2WayRandomChangePasswd ) == 0 )
	{
		*outAuthMethod = kAuth2WayRandomChangePass;
	}
	else
    if ( ::strcmp( p, kDSStdAuthSMB_NT_Key ) == 0 )
	{
		*outAuthMethod = kAuthSMB_NT_Key;
	}
    else
    if ( ::strcmp( p, kDSStdAuthSMB_LM_Key ) == 0 )
	{
		*outAuthMethod = kAuthSMB_LM_Key;
	}
    else
    if ( ::strcmp( p, kDSStdAuthDIGEST_MD5 ) == 0 )
	{
		*outAuthMethod = kAuthDIGEST_MD5;
	}
    else
    if ( ::strcmp( p, kDSStdAuthMSCHAP2 ) == 0 )
	{
		*outAuthMethod = kAuthMSCHAP2;
	}
    else
    if ( ::strcmp( p, "dsAuthMethodStandard:dsAuthNodeDIGEST-MD5-Reauth" ) == 0 )
	{
		*outAuthMethod = kAuthDIGEST_MD5_Reauth;
	}
    else
    if ( ::strcmp( p, kDSStdAuthCRAM_MD5 ) == 0 )
	{
		*outAuthMethod = kAuthCRAM_MD5;
	}
    else
    if ( ::strcmp( p, kDSStdAuthNewUser ) == 0 )
	{
        *outAuthMethod = kAuthNewUser;
    }
    else
    if ( ::strcmp( p, kDSStdAuthGetPolicy ) == 0 )
	{
		*outAuthMethod = kAuthGetPolicy;
	}
    else
    if ( ::strcmp( p, kDSStdAuthSetPolicy ) == 0 )
	{
		*outAuthMethod = kAuthSetPolicy;
	}
    else
    if ( ::strcmp( p, kDSStdAuthSetPolicyAsRoot ) == 0 )
	{
		*outAuthMethod = kAuthSetPolicyAsRoot;
	}
    else
    if ( ::strcmp( p, kDSStdAuthGetGlobalPolicy ) == 0 )
	{
		*outAuthMethod = kAuthGetGlobalPolicy;
	}
    else
    if ( ::strcmp( p, kDSStdAuthSetGlobalPolicy ) == 0 )
	{
		*outAuthMethod = kAuthSetGlobalPolicy;
	}
    else
    if ( ::strcmp( p, kDSStdAuthGetUserName ) == 0 )
	{
		*outAuthMethod = kAuthGetUserName;
	}
    else
    if ( ::strcmp( p, kDSStdAuthSetUserName ) == 0 )
	{
		*outAuthMethod = kAuthSetUserName;
	}
    else
    if ( ::strcmp( p, kDSStdAuthGetUserData ) == 0 )
	{
		*outAuthMethod = kAuthGetUserData;
	}
    else
    if ( ::strcmp( p, kDSStdAuthSetUserData ) == 0 )
	{
		*outAuthMethod = kAuthSetUserData;
	}
    else
    if ( ::strcmp( p, kDSStdAuthDeleteUser ) == 0 )
	{
		*outAuthMethod = kAuthDeleteUser;
	}
	else
    if ( ::strcmp( p, kDSStdAuthSMB_NT_UserSessionKey ) == 0 )
	{
		*outAuthMethod = kAuthSMB_NTUserSessionKey;
	}
	else
    if ( ::strcmp( p, kDSStdAuthSMBWorkstationCredentialSessionKey ) == 0 )
	{
		*outAuthMethod = kAuthSMBWorkstationCredentialSessionKey;
	}
	else
    if ( ::strcmp( p, kDSStdAuthSetWorkstationPasswd ) == 0 )
	{
		*outAuthMethod = kAuthNTSetWorkstationPasswd;
	}
	else
	if ( ::strcmp( p, kDSStdAuthGetEffectivePolicy ) == 0 )
	{
		*outAuthMethod = kAuthGetEffectivePolicy;
	}
	else
	if ( ::strcmp( p, kDSStdAuthGetKerberosPrincipal ) == 0 )
	{
		*outAuthMethod = kAuthGetKerberosPrincipal;
	}
	else
	if ( ::strcmp( p, "dsAuthMethodStandard:dsAuthVPN_MPPEMasterKeys" ) == 0 ||
		 ::strcmp( p, "dsAuthMethodStandard:dsAuthVPN_PPTPMasterKeys" ) == 0 ||
		 ::strcmp( p, "dsAuthMethodStandard:dsAuthMPPEMasterKeys" ) == 0 )
	{
		*outAuthMethod = kAuthVPN_PPTPMasterKeys;
	}
	else
	if ( ::strcmp( p, "dsAuthMethodStandard:dsAuthMSLMCHAP2ChangePasswd" ) == 0 )
	{
		*outAuthMethod = kAuthMSLMCHAP2ChangePasswd;
	}
	else
	if ( ::strcmp( p, "dsAuthMethodStandard:dsAuthEncryptToUser" ) == 0 )
	{
		*outAuthMethod = kAuthEncryptToUser;
	}
	else
	if ( ::strcmp( p, "dsAuthMethodStandard:dsAuthDecrypt" ) == 0 )
	{
		*outAuthMethod = kAuthDecrypt;
	}
	else
	{
		*outAuthMethod = kAuthUnknownMethod;
		siResult = eDSAuthMethodNotSupported;
	}

	return( siResult );

} // GetAuthMethodConstant


// ---------------------------------------------------------------------------
//	* RequiresSASLAuthentication
// ---------------------------------------------------------------------------

bool CPSPlugIn::RequiresSASLAuthentication(	uInt32 inAuthMethodConstant )
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

sInt32
CPSPlugIn::GetAuthMethodSASLName ( uInt32 inAuthMethodConstant, bool inAuthOnly, char *outMechName, bool *outMethodCanSetPassword )
{
    sInt32 result = noErr;
    
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
				strcat( outMechName, kAuthNative_Priority );
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
        case kAuthSMB_NTUserSessionKey:
        case kAuthSMBWorkstationCredentialSessionKey:
        case kAuthNTSetWorkstationPasswd:
		case kAuthVPN_PPTPMasterKeys:
		case kAuthEncryptToUser:
		case kAuthDecrypt:
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
            strcpy( outMechName, inAuthOnly ? kAuthNative_Priority : kDHX_SASL_Name );
            strcat( outMechName, " PLAIN" );
			if ( ! inAuthOnly )
				*outMethodCanSetPassword = true;
            break;
        
        case kAuthNativeNoClearText:
            // If <inAuthOnly> == false, then a "kDSStdSetPasswdAsRoot" auth method
            // could be called later and will require DHX
            strcpy( outMechName, inAuthOnly ? kAuthNative_Priority : kDHX_SASL_Name );
            if ( ! inAuthOnly )
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
			
        case kAuthCRAM_MD5:
            strcpy( outMechName, "CRAM-MD5" );
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
            strcpy( outMechName, kAuthNative_Priority );
            break;
        
        case kAuthNewUser:
        case kAuthSyncSetupReplica:
			strcpy( outMechName, kDHX_SASL_Name );
			*outMethodCanSetPassword = true;
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
void CPSPlugIn::GetAuthMethodFromSASLName( const char *inMechName, char *outDSType )
{
	if ( outDSType == NULL )
		return;
	
	*outDSType = '\0';
	
	if ( inMechName == NULL )
		return;
	
	if ( strcmp( inMechName, "TWOWAYRANDOM" ) == 0 )
	{
		strcpy( outDSType, kDSStdAuth2WayRandom );
	}
	else
	if ( strcmp( inMechName, "SMB-NT" ) == 0 )
	{
		strcpy( outDSType, kDSStdAuthSMB_NT_Key );
	}
	else
	if ( strcmp( inMechName, "SMB-LAN-MANAGER" ) == 0 )
	{
		strcpy( outDSType, kDSStdAuthSMB_LM_Key );
	}
	else
	if ( strcmp( inMechName, "DIGEST-MD5" ) == 0 )
	{
		strcpy( outDSType, kDSStdAuthDIGEST_MD5 );
	}
	else
	if ( strcmp( inMechName, "MS-CHAPv2" ) == 0 )
	{
		strcpy( outDSType, kDSStdAuthMSCHAP2 );
	}
	else
	if ( strcmp( inMechName, "CRAM-MD5" ) == 0 )
	{
		strcpy( outDSType, kDSStdAuthCRAM_MD5 );
	}
	else
	if ( strcmp( inMechName, "CRYPT" ) == 0 )
	{
		strcpy( outDSType, kDSStdAuthCrypt );
	}
	else
	if ( strcmp( inMechName, "APOP" ) == 0 )
	{
		strcpy( outDSType, kDSStdAuthAPOP );
	}
}


//------------------------------------------------------------------------------------
//	* DoSASLNew
//------------------------------------------------------------------------------------
sInt32
CPSPlugIn::DoSASLNew( sPSContextData *inContext, sPSContinueData *inContinue )
{
	sInt32 ret = noErr;
	
	Throw_NULL( inContext, eDSBadContextData );
	Throw_NULL( inContinue, eDSAuthContinueDataBad );
	
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
	
	gSASLMutex->Wait();
	ret = sasl_client_new( "rcmd",
						inContext->psName,
						inContext->localaddr,
						inContext->remoteaddr,
						inContext->callbacks,
						0,
						&inContext->conn);
	gSASLMutex->Signal();
	
	return ret;
}


//------------------------------------------------------------------------------------
//	* DoSASLAuth
//------------------------------------------------------------------------------------

sInt32
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
	sInt32				siResult			= eDSAuthFailed;
	sPSContinueData		*pContinue			= NULL;
	char				*tptr				= NULL;
	
    DEBUGLOG( "CPSPlugIn::DoSASLAuth\n");
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

		DEBUGLOG( "PasswordServer PlugIn: Attempting Authentication" );
        
		// yes do it here
        {
            char buf[4096];
            const char *data;
            char dataBuf[4096];
			unsigned long binLen;
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
                        throw( (sInt32)eDSAuthInvalidUserName );
                    
                    strncpy(inContext->last.username, userName, userNameLen );
                    inContext->last.username[userNameLen] = '\0';
                }
                else
                {
                    strncpy( inContext->last.username, userName, kMaxUserNameLength );
                    inContext->last.username[kMaxUserNameLength-1] = '\0';
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
			
            /*
            const char **gmechs = sasl_global_listmech();
            for (r=0; gmechs[r] != NULL; r++)
                DEBUGLOG( "gmech=%s\n", gmechs[r]);
            */
            
            r = DoSASLNew( inContext, pContinue );
            if ( r != SASL_OK || inContext->conn == NULL ) {
                DEBUGLOG( "sasl_client_new failed, err=%d.\n", r);
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
				DEBUGLOG( "start data=%s\n", tmpData);
				free(tmpData);
			}
#endif
			
            //DEBUGLOG( "chosenmech=%s, datalen=%u\n", chosenmech, len);
            
            if ( r != SASL_OK && r != SASL_CONTINUE ) {
                DEBUGLOG( "starting SASL negotiation, err=%d\n", r);
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
			
			if ( len > 0 )
                snprintf(buf, sizeof(buf), "USER %s AUTH %s %s", userName, chosenmech, dataBuf);
            else
                snprintf(buf, sizeof(buf), "USER %s AUTH %s", userName, chosenmech);
			
			serverResult = SendFlushRead( inContext, buf, NULL, NULL, buf, sizeof(buf) );
            if (serverResult.err != 0) {
                DEBUGLOG( "server returned an error, err=%d\n", serverResult.err);
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
				
				serverResult = SendFlushRead( inContext, buf, NULL, NULL, buf, sizeof(buf) );
				if (serverResult.err != 0) {
					DEBUGLOG( "server returned an error, err=%d\n", serverResult.err);
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
							DEBUGLOG( "server data=%s\n", tmpData);
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
							DEBUGLOG( "step data=%s\n", tmpData);
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
                    DEBUGLOG( "sasl_client_step=%d\n", r);
                    throw( SASLErrToDirServiceError(r) );
                }
                
                if (data && len != 0)
                {
                    //DEBUGLOG( "sending response length %d\n", len);
                    //DEBUGLOG( "client step data = %s", data );
                    
                    ConvertBinaryToHex( (const unsigned char *)data, len, dataBuf );
                    
                    DEBUGLOG( "AUTH2 %s\n", dataBuf);
					serverResult = SendFlushRead( inContext, "AUTH2", dataBuf, NULL, buf, sizeof(buf) );
                }
                else
                if (r==SASL_CONTINUE)
                {
                    DEBUGLOG( "sending null response\n");
                    serverResult = SendFlushRead( inContext, "AUTH2 ", NULL, NULL, buf, sizeof(buf) );
                }
                else
				{
                    break;
                }
				
                if ( serverResult.err != 0 ) {
                    DEBUGLOG( "server returned an error, err=%d\n", serverResult.err);
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
					throw( (sInt32)eMemoryError );
				
				memcpy( *outStepData, dataBuf, binLen );
				(*outStepData)[binLen] = '\0';
				
				//DEBUGLOG( "outStepData = %s\n", *outStepData );
			}
			
            throw( SASLErrToDirServiceError(r) );
        }
		
		// No 2-pass auths handled in this method so clean up now
		gContinue->RemoveItem( pContinue );
        inData->fIOContinueData = NULL;
	}
	
	catch ( sInt32 err )
	{
		DEBUGLOG( "PasswordServer PlugIn: SASL authentication error %l", err );
		siResult = err;
	}
	catch ( ... )
	{
		DEBUGLOG( "PasswordServer PlugIn: SASL uncasted authentication error" );
		siResult = eDSAuthFailed;
	}
	
	return( siResult );

} // DoSASLAuth


//------------------------------------------------------------------------------------
//	* DoSASLTwoWayRandAuth
//------------------------------------------------------------------------------------

sInt32
CPSPlugIn::DoSASLTwoWayRandAuth(
    sPSContextData *inContext,
    const char *userName,
    const char *inMechName,
    sDoDirNodeAuth *inData )
{
	sInt32			siResult			= eDSAuthFailed;
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
    
    DEBUGLOG( "CPSPlugIn::DoSASLTwoWayRandAuth\n");
	try
	{
		Throw_NULL( inContext, eDSBadContextData );
        Throw_NULL( inMechName, eParameterError );
        Throw_NULL( inData, eParameterError );
        Throw_NULL( inAuthBuff, eDSNullAuthStepData );
        Throw_NULL( outAuthBuff, eDSNullAuthStepDataResp );
        Throw_NULL( pContinue, eDSAuthContinueDataBad );
        
        if ( outAuthBuff->fBufferSize < 8 )
            throw( (sInt32)eDSAuthResponseBufTooSmall );
        
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
                        throw( (sInt32)eDSAuthInvalidUserName );
                    
                    strncpy(inContext->last.username, userName, userNameLen );
                    inContext->last.username[userNameLen] = '\0';
                }
                else
                {
                    strncpy( inContext->last.username, userName, kMaxUserNameLength );
                    inContext->last.username[kMaxUserNameLength-1] = '\0';
                }
            }
            
			r = DoSASLNew( inContext, pContinue );
			if ( r != SASL_OK || inContext->conn == NULL ) {
                DEBUGLOG( "sasl_client_new failed, err=%d.\n", r);
                throw( SASLErrToDirServiceError(r) );
            }
            
            r = sasl_setprop(inContext->conn, SASL_SEC_PROPS, &secprops);
            
            // set a user
            snprintf(dataBuf, sizeof(dataBuf), "USER %s\r\n", userName);
            writeToServer(inContext->serverOut, dataBuf);
            
            // flush the read buffer
            serverResult = readFromServer( inContext->fd, buf, sizeof(buf) );
            if (serverResult.err != 0) {
                DEBUGLOG( "server returned an error, err=%d\n", serverResult.err);
                throw( PWSErrToDirServiceError(serverResult) );
            }
            
            // send the auth method
            snprintf(buf, sizeof(buf), "AUTH %s\r\n", inMechName);
            writeToServer(inContext->serverOut, buf);
            
            // get server response
            serverResult = readFromServer(inContext->fd, buf, sizeof(buf));
            if (serverResult.err != 0) {
                DEBUGLOG( "server returned an error, err=%d\n", serverResult.err);
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
                        throw( (sInt32)eDSInvalidBuffFormat );
                    
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
            DEBUGLOG( "inAuthBuff->fBufferLength=%l\n", inAuthBuff->fBufferLength);
            
            // buffer should be:
            // 8 byte DES digest
            // 8 bytes of random
            if ( inAuthBuff->fBufferLength < 16 )
                throw( (sInt32)eDSAuthInBuffFormatError );
            
            // attach the username and password to the sasl connection's context
            // set these before calling sasl_client_start
            if ( inContext->last.password != NULL )
            {
                memset( inContext->last.password, 0, inContext->last.passwordLen );
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
            DEBUGLOG( "chosenmech=%s, datalen=%u\n", chosenmech, len);
            
            if ( r != SASL_OK && r != SASL_CONTINUE ) {
                DEBUGLOG( "starting SASL negotiation, err=%d\n", r);
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
                DEBUGLOG( "stepping SASL negotiation, err=%d\n", r);
                throw( SASLErrToDirServiceError(r) );
            }
            
            if (data && len != 0)
            {
                ConvertBinaryToHex( (const unsigned char *)data, len, dataBuf );
                
                DEBUGLOG( "AUTH2 %s\r\n", dataBuf);
                fprintf(inContext->serverOut, "AUTH2 %s\r\n", dataBuf );
                fflush(inContext->serverOut);
                
                // get server response
                serverResult = readFromServer(inContext->fd, buf, sizeof(buf));
                if (serverResult.err != 0) {
                    DEBUGLOG( "server returned an error, err=%d\n", serverResult.err);
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
                        throw( (sInt32)eDSAuthResponseBufTooSmall );
                    
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

	catch ( sInt32 err )
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
        inData->fIOContinueData = NULL;
    }
    else
    {
        pContinue->fAuthPass++;
    }
    
	return( siResult );
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

sInt32
CPSPlugIn::GetServerListFromDSDiscovery( CFMutableArrayRef inOutServerList )
{
    tDirReference				dsRef				= 0;
    tDataBuffer				   *tNodeListDataBuff	= NULL;
    tDataBuffer				   *tDataBuff			= NULL;
    tDirNodeReference			nodeRef				= 0;
    long						status				= eDSNoErr;
    tContextData				context				= NULL;
    unsigned long				index				= 0;
    unsigned long				nodeCount			= 0;
	tDataList					*nodeName			= NULL;
    tDataList					*recordNameList		= NULL;
	tDataList					*recordTypeList		= NULL;
	tDataList					*attributeList		= NULL;
	unsigned long				recIndex			= 0;
	unsigned long				recCount			= 0;
	unsigned long				attrIndex			= 0;
	unsigned long				attrValueIndex		= 0;
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
										DEBUGLOG( "      %l - %l: (%s) %s\n", attrIndex, attrValueIndex,
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
			while ( context != NULL );
		}
    }
    catch( long error )
	{
		status = error;
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
		
DEBUGLOG( "GetServerListFromDSDiscovery = %l\n", status);
	return status;
}


//------------------------------------------------------------------------------------
//	* PWSErrToDirServiceError
//------------------------------------------------------------------------------------

sInt32 CPSPlugIn::PWSErrToDirServiceError( PWServerError inError )
{
    sInt32 result = 0;
    
    if ( inError.err == 0 )
        return 0;
    
    switch ( inError.type )
    {
        case kPolicyError:
            result = PolicyErrToDirServiceError( inError.err );
            break;
        
        case kSASLError:
            result = SASLErrToDirServiceError( inError.err );
            break;
		
		case kConnectionError:
			result = eDSAuthFailed;
			break;
    }
    
    return result;
}


//------------------------------------------------------------------------------------
//	* SASLErrToDirServiceError
//------------------------------------------------------------------------------------

sInt32 CPSPlugIn::PolicyErrToDirServiceError( int inPolicyError )
{
    sInt32 dirServiceErr = eDSAuthFailed;
    
    switch( inPolicyError )
    {
        case kAuthOK:						dirServiceErr = eDSNoErr;							break;
        case kAuthFail:						dirServiceErr = eDSAuthFailed;						break;
        case kAuthUserDisabled:				dirServiceErr = eDSAuthAccountDisabled;				break;
        case kAuthNeedAdminPrivs:			dirServiceErr = eDSAuthFailed;						break;
        case kAuthUserNotSet:				dirServiceErr = eDSAuthUnknownUser;					break;
        case kAuthUserNotAuthenticated:		dirServiceErr = eDSAuthFailed;						break;
        case kAuthPasswordExpired:			dirServiceErr = eDSAuthAccountExpired;				break;
        case kAuthPasswordNeedsChange:		dirServiceErr = eDSAuthNewPasswordRequired;			break;
        case kAuthPasswordNotChangeable:	dirServiceErr = eDSAuthFailed;						break;
        case kAuthPasswordTooShort:			dirServiceErr = eDSAuthPasswordTooShort;			break;
        case kAuthPasswordTooLong:			dirServiceErr = eDSAuthPasswordTooLong;				break;
        case kAuthPasswordNeedsAlpha:		dirServiceErr = eDSAuthPasswordNeedsLetter;			break;
        case kAuthPasswordNeedsDecimal:		dirServiceErr = eDSAuthPasswordNeedsDigit;			break;
        case kAuthMethodTooWeak:			dirServiceErr = eDSAuthMethodNotSupported;			break;
    }
    
    return dirServiceErr;
}


//------------------------------------------------------------------------------------
//	* SASLErrToDirServiceError
//------------------------------------------------------------------------------------

sInt32 CPSPlugIn::SASLErrToDirServiceError( int inSASLError )
{
    sInt32 dirServiceErr = eDSAuthFailed;

    switch (inSASLError)
    {
        case SASL_CONTINUE:		dirServiceErr = eDSNoErr;					break;
        case SASL_OK:			dirServiceErr = eDSNoErr;					break;
        case SASL_FAIL:			dirServiceErr = eDSAuthFailed;				break;
        case SASL_NOMEM:		dirServiceErr = eMemoryError;				break;
        case SASL_BUFOVER:		dirServiceErr = eDSBufferTooSmall;			break;
        case SASL_NOMECH:		dirServiceErr = eDSAuthMethodNotSupported;	break;
        case SASL_BADPROT:		dirServiceErr = eDSAuthParameterError;		break;
        case SASL_NOTDONE:		dirServiceErr = eDSAuthFailed;				break;
        case SASL_BADPARAM:		dirServiceErr = eDSAuthParameterError;		break;
        case SASL_TRYAGAIN:		dirServiceErr = eDSAuthFailed;				break;
        case SASL_BADMAC:		dirServiceErr = eDSAuthFailed;				break;
        case SASL_NOTINIT:		dirServiceErr = eDSAuthFailed;				break;
        case SASL_INTERACT:		dirServiceErr = eDSAuthParameterError;		break;
        case SASL_BADSERV:		dirServiceErr = eDSAuthFailed;				break;
        case SASL_WRONGMECH:	dirServiceErr = eDSAuthParameterError;		break;
        case SASL_BADAUTH:		dirServiceErr = eDSAuthFailed;				break;
        case SASL_NOAUTHZ:		dirServiceErr = eDSAuthFailed;				break;
        case SASL_TOOWEAK:		dirServiceErr = eDSAuthMethodNotSupported;	break;
        case SASL_ENCRYPT:		dirServiceErr = eDSAuthInBuffFormatError;	break;
        case SASL_TRANS:		dirServiceErr = eDSAuthFailed;				break;
        case SASL_EXPIRED:		dirServiceErr = eDSAuthFailed;				break;
        case SASL_DISABLED:		dirServiceErr = eDSAuthFailed;				break;
        case SASL_NOUSER:		dirServiceErr = eDSAuthUnknownUser;			break;
        case SASL_BADVERS:		dirServiceErr = eDSAuthServerError;			break;
        case SASL_UNAVAIL:		dirServiceErr = eDSAuthNoAuthServerFound;	break;
        case SASL_NOVERIFY:		dirServiceErr = eDSAuthNoAuthServerFound;	break;
        case SASL_PWLOCK:		dirServiceErr = eDSAuthFailed;				break;
        case SASL_NOCHANGE:		dirServiceErr = eDSAuthFailed;				break;
        case SASL_WEAKPASS:		dirServiceErr = eDSAuthBadPassword;			break;
        case SASL_NOUSERPASS:	dirServiceErr = eDSAuthFailed;				break;
    }
    
    return dirServiceErr;
}


//------------------------------------------------------------------------------------
//      * DoPlugInCustomCall
//------------------------------------------------------------------------------------ 

sInt32 CPSPlugIn::DoPlugInCustomCall ( sDoPlugInCustomCall *inData )
{
	sInt32				siResult		= eDSNoErr;
	sPSContextData		*pContext		= nil;
	
//seems that the client needs to have a tDirNodeReference 
//to make the custom call even though it will likely be non-dirnode specific related

DEBUGLOG( "CPSPlugIn::DoPlugInCustomCall\n" );

	try
	{
		if ( inData == nil ) throw( (sInt32)eDSNullParameter );
		if ( inData->fInRequestData == nil ) throw( (sInt32)eDSNullDataBuff );
		if ( inData->fInRequestData->fBufferData == nil ) throw( (sInt32)eDSEmptyBuffer );
		
		pContext = (sPSContextData *)gPSContextTable->GetItemData( inData->fInNodeRef );
		if ( pContext == nil ) throw( (sInt32)eDSBadContextData );
		
		switch( inData->fInRequestCode )
		{
			case 1:
				if ( pContext->replicaFile != NULL )
				{
					delete pContext->replicaFile;
					pContext->replicaFile = NULL;
				}
				pContext->replicaFile = new CReplicaFile( inData->fInRequestData->fBufferData );
				break;
				
			default:
				break;
		}
	}
	catch ( sInt32 err )
	{
		siResult = err;
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

