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
 * @header CLDAPNode
 * LDAP node management class.
 */

#include <stdio.h>
#include <string.h>		//used for strcpy, etc.
#include <stdlib.h>		//used for malloc
#include <ctype.h>		//use for isprint
#include <syslog.h>		//error logging
#include <arpa/inet.h>	// inet_ntop
#include <netinet/in.h>	// struct sockaddr_in
#include <ifaddrs.h>
#include <fcntl.h>
#include <sasl.h>
#include <mach/mach.h>
#include <mach/mach_time.h>

#include "CLDAPNode.h"
#include "CLDAPv3Plugin.h"
#include "CLog.h"

#define OCSEPCHARS			" '()$"

extern CPlugInRef		   *gLDAPConfigTable; 		//TODO need STL type for config table instead
extern uInt32				gLDAPConfigTableLen;
extern	bool				gServerOS;

typedef struct saslDefaults
{
	char *authcid;
	char *password;
} saslDefaults;

bool CLDAPNode::fCheckThreadActive   = false;

class CLDAPv3Plugin;

int sasl_interact( LDAP *ld, unsigned flags, void *inDefaults, void *inInteract );
int sasl_interact( LDAP *ld, unsigned flags, void *inDefaults, void *inInteract )
{
	sasl_interact_t *interact = (sasl_interact_t *)inInteract;
	saslDefaults *defaults = (saslDefaults *) inDefaults;
	
	if( ld == NULL ) return LDAP_PARAM_ERROR;
	
	while( interact->id != SASL_CB_LIST_END )
	{
		const char *dflt = interact->defresult;
		
		switch( interact->id )
		{
			case SASL_CB_AUTHNAME:
				if( defaults ) dflt = defaults->authcid;
				break;
			case SASL_CB_PASS:
				if( defaults ) dflt = defaults->password;
				break;
		}
		
		// if we have a value to return or the SASL request is for a USER
		if( (dflt && *dflt) || interact->id == SASL_CB_USER )
		{
			// we must either return something or an empty value otherwise....
			interact->result = (dflt && *dflt) ? dflt : "";
			interact->len = strlen( (char *)interact->result );
		} else {
			return LDAP_OTHER;
		}
		
		interact++;
	}
	return LDAP_SUCCESS;
}

int doSASLBindAttemptIfPossible( LDAP *inLDAPHost, sLDAPConfigData *pConfig, char *ldapAcct, char *ldapPasswd );
int doSASLBindAttemptIfPossible( LDAP *inLDAPHost, sLDAPConfigData *pConfig, char *ldapAcct, char *ldapPasswd )
{
	int siResult = LDAP_OTHER;
	
	if( pConfig != NULL && pConfig->fSASLmethods != NULL && CFArrayGetCount(pConfig->fSASLmethods) && ldapAcct && strlen(ldapAcct) && ldapPasswd && strlen(ldapPasswd) )
	{
		CFRange range = CFRangeMake( 0, CFArrayGetCount(pConfig->fSASLmethods) );
		struct  saslDefaults	defaults;
		
		char	*tempString		= strdup( ldapAcct );
		char	*workString		= strchr( tempString, '=' );
		
		if( workString != NULL ) {
			// let's get past the '='
			workString++;
			defaults.authcid = strsep( &workString, "," );
		} else {
			defaults.authcid = tempString;
		}
		defaults.password = ldapPasswd;
		
		// let's do CRAM-MD5 if it is available, if not then we'll go to DIGEST-MD5
		if( CFArrayContainsValue(pConfig->fSASLmethods, range, CFSTR("CRAM-MD5")) )
		{
			DBGLOG( kLogPlugin, "CLDAPNode::Attempting CRAM-MD5 Authentication" );
			siResult = ldap_sasl_interactive_bind_s( inLDAPHost, NULL, "CRAM-MD5", NULL, NULL, LDAP_SASL_QUIET, sasl_interact, &defaults );
		/*}
		else if( CFArrayContainsValue(pConfig->fSASLmethods, range, CFSTR("DIGEST-MD5")) )
		{
			DBGLOG( kLogPlugin, "CLDAPNode::Attempting DIGEST-MD5 Authentication" );
			siResult = ldap_sasl_interactive_bind_s( inLDAPHost, NULL, "DIGEST-MD5", NULL, NULL, LDAP_SASL_QUIET, sasl_interact, &defaults );*/
		} else {
			DBGLOG( kLogPlugin, "CLDAPNode::No SASL methods found for server." );
		}
		
		free( tempString );
	} else {
		DBGLOG( kLogPlugin, "CLDAPNode::Skipping SASL methods for server." );
	}
	return siResult;
}

void *checkFailedServers( void *data );
void *checkFailedServers( void *data )
{
	CLDAPNode   *ldapNode = (CLDAPNode *)data;
	
	ldapNode->CheckFailed();
	CLDAPNode::fCheckThreadActive = false;
	return NULL;
}

void LogFailedConnection(const char *inTag, const char *inServerName, int inDisabledIntervalDuration);
void LogFailedConnection(const char *inTag, const char *inServerName, int inDisabledIntervalDuration)
{
	if ((inTag != nil) && (inServerName != nil))
	{
		//log this timed out connection
		syslog(LOG_INFO,"%s: Timed out in attempt to bind to [%s] LDAP server.", inTag, inServerName);
		syslog(LOG_INFO,"%s: Disabled future attempts to bind to [%s] LDAP server for next %d seconds.", inTag, inServerName, inDisabledIntervalDuration);
		DBGLOG1( kLogPlugin, "CLDAPNode::Disabled future attempts to bind to LDAP server %s", inServerName );
	}
	else
	{
		syslog(LOG_INFO,"%s: Logging Failed LDAP connection with incomplete data", (inTag ? inTag : "unknown"));
		DBGLOG( kLogPlugin, "CLDAPNode::Failed LDAP connection" );
	}
}

void SetSockList(int *inSockList, int inSockCount, bool inClose);
void SetSockList(int *inSockList, int inSockCount, bool inClose)
{
	for (int iCount = 0; iCount < inSockCount; iCount++)
	{
		if ( (inClose) && (inSockList[iCount] >= 0) )
		{
			close(inSockList[iCount]);
		}
		inSockList[iCount] = -1;
	}
}

// --------------------------------------------------------------------------------
//	* CLDAPNode ()
// --------------------------------------------------------------------------------

CLDAPNode::CLDAPNode ( void )
{
} // CLDAPNode


// --------------------------------------------------------------------------------
//	* ~CLDAPNode ()
// --------------------------------------------------------------------------------

CLDAPNode::~CLDAPNode ( void )
{
} // ~CLDAPNode


// ---------------------------------------------------------------------------
//	* SafeOpen
// ---------------------------------------------------------------------------

sInt32	CLDAPNode::SafeOpen		(	char	   *inNodeName,
									LDAP	  **outLDAPHost,
									uInt32	  *outLDAPConfigTableIndex,
                                    CLDAPv3Configs *inConfigFromXML )
{
	sInt32					siResult		= eDSNoErr;
	sLDAPNodeStruct		   *pLDAPNodeStruct	= nil;
	uInt32					iTableIndex		= 0;
	sLDAPConfigData		   *pConfig			= nil;
    int						ldapPort		= LDAP_PORT;
	char				   *aLDAPName		= nil;
	bool					bConfigFound	= false;
	LDAPNodeMapI			aLDAPNodeMapI;
	string					aNodeName(inNodeName);
	uInt32					openTO			= kLDAPDefaultOpenCloseTimeoutInSeconds;
	
//if already open then just get host and config index
//if not open then bind to get host and search for config index
//called from OpenDirNode

//allow the inNodeName to have a suffixed ":portNumber" for directed open

//inNodeName is NOT consumed here

	fLDAPNodeOpenMutex.Wait();
	aLDAPNodeMapI	= fLDAPNodeMap.find(aNodeName);
	if (aLDAPNodeMapI == fLDAPNodeMap.end())
	{
		pLDAPNodeStruct = (sLDAPNodeStruct *) calloc(1, sizeof(sLDAPNodeStruct));
		pLDAPNodeStruct->fRefCount = 1;
		pLDAPNodeStruct->fLDAPSessionMutex = new DSMutexSemaphore();
		// don't care if this was originally in the config file or not
		// ie. allow non-configured connections if possible
		// however, they need to use the standard LDAP PORT if no config entry exists
		// search now for possible LDAP port entry
		//Cycle through the gLDAPConfigTable to get the LDAP port to use for the ldap_open
		//start at index of one since 0 is generic config
		for (iTableIndex=1; iTableIndex<gLDAPConfigTableLen; iTableIndex++)
		{
			pConfig = (sLDAPConfigData *)gLDAPConfigTable->GetItemData( iTableIndex );
			if (pConfig != nil)
			{
				openTO = pConfig->fOpenCloseTimeout;
				if (pConfig->fServerName != nil)
				{
					if (::strcmp(pConfig->fServerName,inNodeName) == 0)
					{
						ldapPort = pConfig->fServerPort;
						bConfigFound = true;
						pLDAPNodeStruct->fLDAPConfigTableIndex = iTableIndex;
						//add the idle connection TO value here based on user defined minutes and 30 sec periodic task
						pLDAPNodeStruct->fIdleTO = 2 * pConfig->fIdleTimeout;
						//add in the delay rebind try after failed bind time
						pLDAPNodeStruct->fDelayRebindTry = pConfig->fDelayRebindTry;
						//exit the for loop if entry found
						break;
					} // if name found
				} // if name not nil
			}// if config entry not nil
		} // loop over config table entries
		
		if (!bConfigFound)
		{
			//here we have not found a configuration but will allow the open
			//first check if there is a suffixed ':' port number on the inNodeName
			siResult = ParseLDAPNodeName( inNodeName, &aLDAPName, &ldapPort );
			if (siResult == eDSNoErr)
			{
				pLDAPNodeStruct->fServerName			= aLDAPName;
				pLDAPNodeStruct->fDirectLDAPPort		= ldapPort;
				
				pLDAPNodeStruct->fLDAPConfigTableIndex	= 0;
				//TODO need to access the LDAP server for possible mapping configuration that can be added
				//thus fLDAPConfigTableIndex would then be non-zero
			}
		}
		
		//add this to the fLDAPNodeMap
		fLDAPNodeMap[aNodeName] = pLDAPNodeStruct;
		//fLDAPNodeMap.insert(pair<string, sLDAPNodeStruct*>(aNodeName, pLDAPNodeStruct));
		fLDAPNodeOpenMutex.Signal();
	}
	else
	{
		pLDAPNodeStruct = aLDAPNodeMapI->second;
		pLDAPNodeStruct->fRefCount++;
		if (( pLDAPNodeStruct->fLDAPConfigTableIndex < gLDAPConfigTableLen) && ( pLDAPNodeStruct->fLDAPConfigTableIndex >= 1 ))
		{
			pConfig = (sLDAPConfigData *)gLDAPConfigTable->GetItemData( pLDAPNodeStruct->fLDAPConfigTableIndex );
			if (pConfig != nil)
			{
				openTO = pConfig->fOpenCloseTimeout;
				if (pConfig->bGetServerMappings) //server mappings still need to be retrieved
				{
					bConfigFound = true;
				}
			}
		}
		fLDAPNodeOpenMutex.Signal();
	}
	
	if (siResult == eDSNoErr)
	{
		if (pLDAPNodeStruct->fConnectionStatus == kConnectionUnknown)
		{
			//first lets spawn our checking thread if this has not already been done
			EnsureCheckFailedConnectionsThreadIsRunning();
			
			// let's go to sleep for the designated time and wait to see if it becomes available.
			// rather than take the chance and hang the system on a timeout...
			struct mach_timebase_info	timeBaseInfo;
			uint64_t					delay;
			
			mach_timebase_info( &timeBaseInfo );
			delay = (((uint64_t)(NSEC_PER_SEC * openTO) * (uint64_t)timeBaseInfo.denom) / (uint64_t)timeBaseInfo.numer);
	
			// let's wait openTO seconds...
			mach_wait_until( mach_absolute_time() + delay );
		}
		
		if (pLDAPNodeStruct->fConnectionStatus != kConnectionSafe)
		{
			siResult = eDSCannotAccessSession;
		}
		
		if (siResult == eDSNoErr)
		{
			if (pLDAPNodeStruct->fLDAPSessionMutex != nil)
			{
				pLDAPNodeStruct->fLDAPSessionMutex->Wait();
			}
	
			// SASLMethods are first so that we can do non-clear text auth methods
			CheckSASLMethods( pLDAPNodeStruct, inConfigFromXML );
	
			//call to bind here
			siResult = BindProc( pLDAPNodeStruct, inConfigFromXML );
			
			// we need to preflight ServerMappings here... then rebind again...
			if( bConfigFound && pConfig->bGetServerMappings && pLDAPNodeStruct->fHost )
			{
				RetrieveServerMappingsIfRequired(pLDAPNodeStruct, inConfigFromXML);
				ldap_unbind_ext( pLDAPNodeStruct->fHost, NULL, NULL );
				pLDAPNodeStruct->fHost = nil;
				
				// we need to rebind to see if we found replicas now....
				siResult = BindProc( pLDAPNodeStruct, inConfigFromXML );
			}
			
			// if we had an error on the SafeOpen, then we should decrement the RefCount otherwise
			// we get out of control on refcount
			if( siResult != eDSNoErr ) {
				pLDAPNodeStruct->fRefCount--;
			}
			
			//set the out parameters now
			*outLDAPHost				= pLDAPNodeStruct->fHost;
			*outLDAPConfigTableIndex	= pLDAPNodeStruct->fLDAPConfigTableIndex;
			
			if (pLDAPNodeStruct->fLDAPSessionMutex != nil)
			{
				pLDAPNodeStruct->fLDAPSessionMutex->Signal();
			}
		}
	}

	return(siResult);
} // SafeOpen

// ---------------------------------------------------------------------------
//	* AuthOpen
// ---------------------------------------------------------------------------

sInt32	CLDAPNode::AuthOpen		(	char	   *inNodeName,
									LDAP	   *inHost,
									char	   *inUserName,
									void	   *inAuthCredential,
									char	   *inAuthType,
									LDAP	  **outLDAPHost,
									uInt32	  *inOutLDAPConfigTableIndex,
									bool	   shouldCloseOld )
{
	sInt32					siResult			= eDSNoErr;
	sLDAPNodeStruct		   *pLDAPNodeStruct		= nil;
	sLDAPNodeStruct		   *pLDAPAuthNodeStruct	= nil;
    int						ldapPort			= LDAP_PORT;
	char				   *aLDAPName			= nil;
	LDAPNodeMapI			aLDAPNodeMapI;
	string					aNodeName(inNodeName);

//there must be a fLDAPNodeMap entry for this ie. SafeOpen was already called since
//this would come from a known Node Ref then need to SafeClose the non Auth Open
//this is not ref counted NOR reused
//inOutLDAPConfigTableIndex could be passed in for a directed open ie. not zero
//called from the hierarchy below DoAuthentication

//inNodeName is NOT consumed here

	fLDAPNodeOpenMutex.Wait();
	aLDAPNodeMapI	= fLDAPNodeMap.find(aNodeName);
	if (aLDAPNodeMapI != fLDAPNodeMap.end())
	{
		pLDAPNodeStruct = aLDAPNodeMapI->second;
		
		pLDAPAuthNodeStruct = (sLDAPNodeStruct *) calloc(1, sizeof(sLDAPNodeStruct));
		if (pLDAPNodeStruct->fLDAPConfigTableIndex != 0)
		{
			pLDAPAuthNodeStruct->fLDAPConfigTableIndex = pLDAPNodeStruct->fLDAPConfigTableIndex;
		}
		else
		{
			pLDAPAuthNodeStruct->fServerName		= pLDAPNodeStruct->fServerName;
			pLDAPAuthNodeStruct->fDirectLDAPPort	= pLDAPNodeStruct->fDirectLDAPPort;
			
		}
		pLDAPAuthNodeStruct->fUserName			= inUserName;
		pLDAPAuthNodeStruct->fAuthCredential	= inAuthCredential;
		pLDAPAuthNodeStruct->fAuthType			= inAuthType;
		fLDAPNodeOpenMutex.Signal();

		//call to bind here
		siResult = BindProc( pLDAPAuthNodeStruct, nil, shouldCloseOld );
	
		if (siResult == eDSNoErr)
		{
			//set the out parameters now
			*outLDAPHost				= pLDAPAuthNodeStruct->fHost;
			*inOutLDAPConfigTableIndex	= pLDAPNodeStruct->fLDAPConfigTableIndex;

			//using duplicate of SafeClose(inNodeName) code here directly since within mutex
			if ( shouldCloseOld )
			{
				fLDAPNodeOpenMutex.Wait();
				if (inHost == pLDAPNodeStruct->fHost)
				{
					pLDAPNodeStruct->fRefCount--;
					if (pLDAPNodeStruct->fRefCount == 0 && pLDAPNodeStruct->fConnectionStatus == kConnectionSafe)
					{
						//remove the entry if refcount is zero after cleaning contents
						CleanLDAPNodeStruct(pLDAPNodeStruct);
						fLDAPNodeMap.erase(aNodeName);
					}
				}
				
				else if (inHost != nil)
				{
					// close an existing authenticated connection
					ldap_unbind(inHost);
				}
				inHost = nil;

				fLDAPNodeOpenMutex.Signal();
			}
		}
		
		if (pLDAPAuthNodeStruct != nil)
		{
			free(pLDAPAuthNodeStruct);
			pLDAPAuthNodeStruct = nil;
		}
	}
	else if (inHost != nil)
	//case where a second Auth is made on the Dir Node but
	//original LDAPNodeMap entry was already SafeClosed
	{
		fLDAPNodeOpenMutex.Signal();
		pLDAPAuthNodeStruct = (sLDAPNodeStruct *) calloc(1, sizeof(sLDAPNodeStruct));
		
		//here we have no configuration but will allow the open
		//first check if there is a suffixed ':' port number on the inNodeName
		siResult = ParseLDAPNodeName( inNodeName, &aLDAPName, &ldapPort );
		if (siResult == eDSNoErr)
		{
			pLDAPAuthNodeStruct->fServerName			= aLDAPName;
			pLDAPAuthNodeStruct->fDirectLDAPPort		= ldapPort;
			pLDAPAuthNodeStruct->fLDAPConfigTableIndex	= *inOutLDAPConfigTableIndex;
			//if ( *inOutLDAPConfigTableIndex == 0 )
			//{
				//NO try again to access the LDAP server for possible mapping configuration that can be added
				//SINCE we assume that this cannot be retrieved since attempt will have already been made in
				//the original SafeOpen for this directed open
			//}
				
			//call to bind here
			siResult = BindProc( pLDAPAuthNodeStruct, nil, shouldCloseOld );
		
			if (siResult == eDSNoErr)
			{
				//set the out parameter now
				*outLDAPHost = pLDAPAuthNodeStruct->fHost;
				if ( shouldCloseOld )
				{
					ldap_unbind( inHost );
					inHost = nil;
				}
			}
			else
			{
				siResult = eDSAuthFailed;
			}
		}
		
		if (pLDAPAuthNodeStruct != nil)
		{
			if (pLDAPAuthNodeStruct->fServerName != nil)
			{
				free(pLDAPAuthNodeStruct->fServerName); //only var owned by this temporary struct here
			}
			free(pLDAPAuthNodeStruct);
			pLDAPAuthNodeStruct = nil;
		}
		
	}
	else
	{
		fLDAPNodeOpenMutex.Signal();
		siResult = eDSOpenNodeFailed;
	}
	
	return(siResult);

}// AuthOpen

// ---------------------------------------------------------------------------
//	* RebindSession
// ---------------------------------------------------------------------------

sInt32	CLDAPNode::RebindSession(	char	   *inNodeName,
									LDAP	   *inHost,
									CLDAPv3Configs *inConfigFromXML,
									LDAP	  **outLDAPHost )
{
	sInt32					siResult		= eDSNoErr;
	sLDAPNodeStruct		   *pLDAPNodeStruct	= nil;
	LDAPNodeMapI			aLDAPNodeMapI;
	string					aNodeName(inNodeName);

//must already be open
//called from CLDAPv3Plugin:: RebindLDAPSession

	fLDAPNodeOpenMutex.Wait();
	aLDAPNodeMapI	= fLDAPNodeMap.find(aNodeName);
	if (aLDAPNodeMapI != fLDAPNodeMap.end())
	{
		pLDAPNodeStruct = aLDAPNodeMapI->second;
		
		if (pLDAPNodeStruct->fHost != nil)
		{
			ldap_unbind(pLDAPNodeStruct->fHost);
			pLDAPNodeStruct->fHost = nil;
		}
		
		fLDAPNodeOpenMutex.Signal();
		
		if (pLDAPNodeStruct->fLDAPSessionMutex != nil)
		{
			pLDAPNodeStruct->fLDAPSessionMutex->Wait();
		}
		
		//call to bind here
		//TODO How many retries do we do?
		siResult = BindProc( pLDAPNodeStruct );
		
		if (pLDAPNodeStruct->fLDAPSessionMutex != nil)
		{
			pLDAPNodeStruct->fLDAPSessionMutex->Signal();
		}
		
		if (siResult == eDSNoErr)
		{
			//set the out parameters now
			*outLDAPHost = pLDAPNodeStruct->fHost;
			pLDAPNodeStruct->fConnectionStatus = kConnectionSafe;
			RetrieveServerMappingsIfRequired(pLDAPNodeStruct, inConfigFromXML);
		}
	}
	else //no entry in fLDAPNodeMap
	{
		fLDAPNodeOpenMutex.Signal();
		siResult = eDSOpenNodeFailed;
	}
	
	return(siResult);

}// x

// ---------------------------------------------------------------------------
//	* SimpleAuth
// ---------------------------------------------------------------------------

sInt32	CLDAPNode::SimpleAuth(	char	   *inNodeName,
								char	   *inUserName,
								void	   *inAuthCredential )
{
	sInt32					siResult			= eDSNoErr;
	sLDAPNodeStruct		   *pLDAPAuthNodeStruct	= nil;
	sLDAPNodeStruct		   *pLDAPNodeStruct		= nil;
	LDAPNodeMapI			aLDAPNodeMapI;
	string					aNodeName(inNodeName);

//use bind to do the auth
//called from DoClearTextAuth

	fLDAPNodeOpenMutex.Wait();
	aLDAPNodeMapI	= fLDAPNodeMap.find(aNodeName);
	if (aLDAPNodeMapI != fLDAPNodeMap.end())
	{
		pLDAPNodeStruct = aLDAPNodeMapI->second;
		//build the temporary auth node struct for use with BindProc
		pLDAPAuthNodeStruct = (sLDAPNodeStruct *) calloc(1, sizeof(sLDAPNodeStruct));
		
		if (pLDAPNodeStruct->fLDAPConfigTableIndex != 0)
		{
			pLDAPAuthNodeStruct->fLDAPConfigTableIndex = pLDAPNodeStruct->fLDAPConfigTableIndex;
		}
		else
		{
			pLDAPAuthNodeStruct->fServerName		= pLDAPNodeStruct->fServerName;
			pLDAPAuthNodeStruct->fDirectLDAPPort	= pLDAPNodeStruct->fDirectLDAPPort;
		}
		
		pLDAPAuthNodeStruct->fHost				= nil;	//new session handle for this directed auth
														//don't want this bind to disrupt
														//any other ops on the LDAP server
		pLDAPAuthNodeStruct->fLDAPSessionMutex	= nil;
		pLDAPAuthNodeStruct->fUserName			= inUserName;
		pLDAPAuthNodeStruct->fAuthCredential	= inAuthCredential;
		//fAuthType nil is default for using a password
		
		//now try to bind
		siResult = BindProc( pLDAPAuthNodeStruct );
		if (siResult != eDSNoErr)
		{
			siResult = eDSAuthFailed;
		}
		//no cleanup of contents other than session handle
		if (pLDAPAuthNodeStruct->fHost != nil)
		{
			ldap_unbind(pLDAPAuthNodeStruct->fHost);
		}
		free(pLDAPAuthNodeStruct);
	}
	else //no entry in fLDAPNodeMap
	{
		siResult = eDSAuthFailed;
	}
	fLDAPNodeOpenMutex.Signal();
	
	return(siResult);

}// SimpleAuth

// ---------------------------------------------------------------------------
//	* RebindAuthSession
// ---------------------------------------------------------------------------

sInt32	CLDAPNode::RebindAuthSession(	char	   *inNodeName,
										LDAP	   *inHost,
										char	   *inUserName,
										void	   *inAuthCredential,
										char	   *inAuthType,
										uInt32		inLDAPConfigTableIndex,
										LDAP	  **outLDAPHost )
{
	sInt32					siResult			= eDSNoErr;
	sLDAPNodeStruct		   *pLDAPAuthNodeStruct	= nil;
    int						ldapPort			= LDAP_PORT;
	char				   *aLDAPName			= nil;

//must already have had a session
//called from CLDAPv3Plugin::RebindLDAPSession

	if (inHost != nil)
	//want to use this Auth session for the Dir Node context
	//original LDAPNodeMap entry was already SafeClosed
	{
		pLDAPAuthNodeStruct = (sLDAPNodeStruct *) calloc(1, sizeof(sLDAPNodeStruct));
		
		//here we have no configuration but will allow the open
		//first check if there is a suffixed ':' port number on the inNodeName
		siResult = ParseLDAPNodeName( inNodeName, &aLDAPName, &ldapPort );

		if (siResult == eDSNoErr)
		{
			pLDAPAuthNodeStruct->fServerName			= aLDAPName;
			pLDAPAuthNodeStruct->fDirectLDAPPort		= ldapPort;
			pLDAPAuthNodeStruct->fHost					= nil;
			pLDAPAuthNodeStruct->fLDAPConfigTableIndex	= inLDAPConfigTableIndex;
				
			pLDAPAuthNodeStruct->fUserName				= inUserName;
			pLDAPAuthNodeStruct->fAuthCredential		= inAuthCredential;
			//fAuthType nil is default for using a password

			ldap_unbind(inHost);
			//call to bind here
			//TODO How many retries do we do?
			siResult = BindProc( pLDAPAuthNodeStruct );
		
			if (siResult == eDSNoErr)
			{
				//set the out parameter now
				*outLDAPHost				= pLDAPAuthNodeStruct->fHost;
			}
		}
		
		if (pLDAPAuthNodeStruct != nil)
		{
			if (pLDAPAuthNodeStruct->fServerName != nil)
			{
				free(pLDAPAuthNodeStruct->fServerName); //only var owned by this temporary struct here
			}
			free(pLDAPAuthNodeStruct);
			pLDAPAuthNodeStruct = nil;
		}
		
	}
	else
	{
		siResult = eDSOpenNodeFailed;
	}
	return(siResult);

}// RebindAuthSession

// ---------------------------------------------------------------------------
//	* SafeClose
// ---------------------------------------------------------------------------

sInt32	CLDAPNode::SafeClose	(	char	   *inNodeName,
									LDAP	   *inHost )
{
	sInt32					siResult		= eDSNoErr;
	sLDAPNodeStruct		   *pLDAPNodeStruct	= nil;
	LDAPNodeMapI			aLDAPNodeMapI;
	string					aNodeName(inNodeName);

//decrement ref count and delete if ref count zero
//if Auth call was active then the inHost will NOT be nil and we need to unbind it
//called from CloseDirNode

	fLDAPNodeOpenMutex.Wait();
	if (inHost != nil)
	{
		//this check is for auth call active closes
		ldap_unbind( inHost );
	}
	else
	{
		aLDAPNodeMapI	= fLDAPNodeMap.find(aNodeName);
		if (aLDAPNodeMapI != fLDAPNodeMap.end())
		{
			pLDAPNodeStruct = aLDAPNodeMapI->second;
			pLDAPNodeStruct->fRefCount--;
			
			// if our refcount is 0 and the connection hasn't failed, let's clear it
			// otherwise leave it around so that future connections do rebind tries if necessary
			if (pLDAPNodeStruct->fRefCount == 0 && pLDAPNodeStruct->fConnectionStatus == kConnectionSafe)
			{
				//remove the entry if refcount is zero
				fLDAPNodeMap.erase(aNodeName);
				CleanLDAPNodeStruct(pLDAPNodeStruct);
				free(pLDAPNodeStruct);
			}
		}
	}
	fLDAPNodeOpenMutex.Signal();
	
	return(siResult);

}// SafeClose

// ---------------------------------------------------------------------------
//	* ForcedSafeClose
// ---------------------------------------------------------------------------

sInt32	CLDAPNode::ForcedSafeClose	( char *inNodeName)
{
	sInt32					siResult		= eDSNoErr;
	sLDAPNodeStruct		   *pLDAPNodeStruct	= nil;
	LDAPNodeMapI			aLDAPNodeMapI;
	string					aNodeName(inNodeName);

//remove regardless of ref count as config has been removed
//called from Initialize

	fLDAPNodeOpenMutex.Wait();

	aLDAPNodeMapI	= fLDAPNodeMap.find(aNodeName);
	if (aLDAPNodeMapI != fLDAPNodeMap.end() )
	{
		pLDAPNodeStruct = aLDAPNodeMapI->second;
		//remove the entry
		if (pLDAPNodeStruct->fConnectionStatus == kConnectionSafe)
		{
			if (pLDAPNodeStruct->fOperationsCount == 0)
			{
				//no ongoing operations so we can clean up now
				fLDAPNodeMap.erase(aNodeName);
				CleanLDAPNodeStruct(pLDAPNodeStruct);
				free(pLDAPNodeStruct);
			}
			else
			{
				//add to a dead pool for cleanup later
				fLDAPNodeMap.erase(aNodeName);
				fDeadPoolLDAPNodeVector.push_back(pLDAPNodeStruct);
			}
		}
	}

	fLDAPNodeOpenMutex.Signal();
	
	return(siResult);

}// ForcedSafeClose

// ---------------------------------------------------------------------------
//	* GetSchema
// ---------------------------------------------------------------------------

void	CLDAPNode::GetSchema	( sLDAPContextData *inContext )
{
	sInt32					siResult		= eDSNoErr;
	sLDAPConfigData		   *pConfig			= nil;
	LDAPMessage			   *LDAPResult		= nil;
	BerElement			   *ber;
	struct berval		  **bValues;
	char				   *pAttr			= nil;
	sObjectClassSchema	   *aOCSchema		= nil;
	bool					bSkipToTag		= true;
	char				   *lineEntry		= nil;
	char				   *strtokContext	= nil;
	LDAP				   *aHost			= nil;
	
	if ( inContext != nil )
	{
		pConfig = (sLDAPConfigData *)gLDAPConfigTable->GetItemData( inContext->fConfigTableIndex );
		
		if (pConfig != nil)
		{
			aHost = LockSession(inContext);
			if ( (aHost != nil) && !(pConfig->bOCBuilt) ) //valid LDAP handle and schema not already built
			{
				//at this point we can make the call to the LDAP server to determine the object class schema
				//then after building the ObjectClassMap we can assign it to the pConfig->fObjectClassSchema
				//in either case we set the pConfig->bOCBuilt since we made the attempt
				siResult = GetSchemaMessage( aHost, pConfig->fSearchTimeout, &LDAPResult);
				
				if (siResult == eDSNoErr)
				{
					//parse the attributes in the LDAPResult - should only be one ie. objectclass
					for (	pAttr = ldap_first_attribute (aHost, LDAPResult, &ber );
							pAttr != NULL; pAttr = ldap_next_attribute(aHost, LDAPResult, ber ) )
					{
						if (( bValues = ldap_get_values_len (aHost, LDAPResult, pAttr )) != NULL)
						{
							ObjectClassMap *aOCClassMap = new(ObjectClassMap);
						
							// for each value of the attribute we need to parse and add as an entry to the objectclass schema map
							for (int i = 0; bValues[i] != NULL; i++ )
							{
								aOCSchema = nil;
								if (lineEntry != nil) //delimiter chars will be overwritten by NULLs
								{
									free(lineEntry);
									lineEntry = nil;
								}
								
								//here we actually parse the values
								lineEntry = (char *)calloc(1,bValues[i]->bv_len+1);
								strcpy(lineEntry, bValues[i]->bv_val);
								
								char	   *aToken			= nil;
								
								//find the objectclass name
								aToken = strtok_r(lineEntry,OCSEPCHARS, &strtokContext);
								while ( (aToken != nil) && (strcmp(aToken,"NAME") != 0) )
								{
									aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
								}
								if (aToken != nil)
								{
									aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
									if (aToken != nil)
									{
										//now use the NAME to create an entry
										//first check if that NAME is already present - unlikely
										if (aOCClassMap->find(aToken) == aOCClassMap->end())
										{
											aOCSchema = new(sObjectClassSchema);
											(*aOCClassMap)[aToken] = aOCSchema;
										}
									}
								}
								
								if (aOCSchema == nil)
								{
									continue;
								}
								if (aToken == nil)
								{
									continue;
								}
								//here we have the NAME - at least one of them
								//now check if there are any more NAME values
								bSkipToTag = true;
								while (bSkipToTag)
								{
									aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
									if (aToken == nil)
									{
										break;
									}
									bSkipToTag = IsTokenNotATag(aToken);
									if (bSkipToTag)
									{
										aOCSchema->fOtherNames.insert(aOCSchema->fOtherNames.begin(),aToken);
									}
								}
								if (aToken == nil)
								{
									continue;
								}
								
								if (strcmp(aToken,"DESC") == 0)
								{
									bSkipToTag = true;
									while (bSkipToTag)
									{
										aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
										if (aToken == nil)
										{
											break;
										}
										bSkipToTag = IsTokenNotATag(aToken);
									}
									if (aToken == nil)
									{
										continue;
									}
								}
								
								if (strcmp(aToken,"OBSOLETE") == 0)
								{
									bSkipToTag = true;
									while (bSkipToTag)
									{
										aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
										if (aToken == nil)
										{
											break;
										}
										bSkipToTag = IsTokenNotATag(aToken);
									}
									if (aToken == nil)
									{
										continue;
									}
								}
		
								if (strcmp(aToken,"SUP") == 0)
								{
									aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
									if (aToken == nil)
									{
										continue;
									}
									aOCSchema->fParentOCs.insert(aOCSchema->fParentOCs.begin(),aToken);
									//get the other SUP entries
									bSkipToTag = true;
									while (bSkipToTag)
									{
										aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
										if (aToken == nil)
										{
											break;
										}
										bSkipToTag = IsTokenNotATag(aToken);
										if (bSkipToTag)
										{
											aOCSchema->fParentOCs.insert(aOCSchema->fParentOCs.begin(),aToken);
										}
									}
									if (aToken == nil)
									{
										continue;
									}
								}
								
								if (strcmp(aToken,"ABSTRACT") == 0)
								{
									aOCSchema->fType = 0;
									aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
									if (aToken == nil)
									{
										continue;
									}
								}
								
								if (strcmp(aToken,"STRUCTURAL") == 0)
								{
									aOCSchema->fType = 1;
									aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
									if (aToken == nil)
									{
										continue;
									}
								}
								
								if (strcmp(aToken,"AUXILIARY") == 0)
								{
									aOCSchema->fType = 2;
									aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
									if (aToken == nil)
									{
										continue;
									}
								}
								
								if (strcmp(aToken,"MUST") == 0)
								{
									aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
									if (aToken == nil)
									{
										continue;
									}
									aOCSchema->fRequiredAttrs.insert(aOCSchema->fRequiredAttrs.begin(),aToken);
									//get the other MUST entries
									bSkipToTag = true;
									while (bSkipToTag)
									{
										aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
										if (aToken == nil)
										{
											break;
										}
										bSkipToTag = IsTokenNotATag(aToken);
										if (bSkipToTag)
										{
											aOCSchema->fRequiredAttrs.insert(aOCSchema->fRequiredAttrs.begin(),aToken);
										}
									}
									if (aToken == nil)
									{
										continue;
									}
								}
								
								if (strcmp(aToken,"MAY") == 0)
								{
									aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
									if (aToken == nil)
									{
										continue;
									}
									aOCSchema->fAllowedAttrs.insert(aOCSchema->fAllowedAttrs.begin(),aToken);
									//get the other MAY entries
									bSkipToTag = true;
									while (bSkipToTag)
									{
										aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
										if (aToken == nil)
										{
											break;
										}
										bSkipToTag = IsTokenNotATag(aToken);
										if (bSkipToTag)
										{
											aOCSchema->fAllowedAttrs.insert(aOCSchema->fAllowedAttrs.begin(),aToken);
										}
									}
									if (aToken == nil)
									{
										continue;
									}
								}
								
							} // for each bValues[i]
							
							if (lineEntry != nil) //delimiter chars will be overwritten by NULLs
							{
								free(lineEntry);
								lineEntry = nil;
							}
							
							ldap_value_free_len(bValues);
							
							pConfig->fObjectClassSchema = aOCClassMap;
							
						} // if bValues = ldap_get_values_len ...
												
						if (pAttr != nil)
						{
							ldap_memfree( pAttr );
						}
						
					} // for ( loop over ldap_next_attribute )
					
					if (ber != nil)
					{
						ber_free( ber, 0 );
					}
					
					ldap_msgfree( LDAPResult );
				}
			}
			pConfig->bOCBuilt = true;
			UnLockSession(inContext);
		}
	}

} // GetSchema

// ---------------------------------------------------------------------------
//	* ParseLDAPNodeName
// ---------------------------------------------------------------------------

sInt32	CLDAPNode::ParseLDAPNodeName(	char	   *inNodeName,
										char	  **outLDAPName,
										int		   *outLDAPPort )
{
	sInt32			siResult	= eDSNoErr;
	char		   *portPos		= nil;
	uInt32			inLength	= 0;
	int				ldapPort	= LDAP_PORT;
	char		   *ldapName	= nil;
	
//parse a string with a name and possibly a suffix of ':' followed by a port number

	if (inNodeName != nil)
	{
		inLength	= strlen(inNodeName);
		portPos		= strchr(inNodeName, ':');
		//check if ':' found
		if (portPos != nil)
		{
			portPos++;
			//check if nothing after ':'
			if (portPos != nil)
			{
				ldapPort = strtoul(portPos,NULL,NULL);
				//if error in conversion set back to default
				if (ldapPort == 0)
				{
					ldapPort = LDAP_PORT;
				}
				
				inLength = inLength - strlen(portPos);					
			}
			//strip off the suffix ':???'
			ldapName = (char *) calloc(1, inLength);
			strncpy(ldapName, inNodeName, inLength-1);
		}
		else
		{
			ldapName = (char *) calloc(1, inLength+1);
			strncpy(ldapName, inNodeName, inLength);
		}
		
		*outLDAPName	= ldapName;
		*outLDAPPort	= ldapPort;
	}
	else
	{
		siResult = eDSNullParameter;
	}

	return(siResult);

}// ParseLDAPNodeName

// ---------------------------------------------------------------------------
//	* CleanLDAPNodeStruct
// ---------------------------------------------------------------------------

sInt32	CLDAPNode::CleanLDAPNodeStruct	( sLDAPNodeStruct *inLDAPNodeStruct )
{
	sInt32 siResult = eDSNoErr;

//assumes struct has ownership of all members
	if (inLDAPNodeStruct != nil)
	{
		if (inLDAPNodeStruct->fLDAPSessionMutex != nil)
		{
			inLDAPNodeStruct->fLDAPSessionMutex->Wait();
		}
		if (inLDAPNodeStruct->fHost != nil)
		{
			ldap_unbind( inLDAPNodeStruct->fHost );
			inLDAPNodeStruct->fHost = nil;
		}
		if (inLDAPNodeStruct->fLDAPSessionMutex != nil)
		{
			inLDAPNodeStruct->fLDAPSessionMutex->Signal();
			delete(inLDAPNodeStruct->fLDAPSessionMutex);
			inLDAPNodeStruct->fLDAPSessionMutex = nil;
		}
		
		if (inLDAPNodeStruct->fServerName != nil)
		{
			free( inLDAPNodeStruct->fServerName );
			inLDAPNodeStruct->fServerName = nil;
		}
		
		if (inLDAPNodeStruct->fUserName != nil)
		{
			free( inLDAPNodeStruct->fUserName );
			inLDAPNodeStruct->fUserName = nil;
		}
		
		if (inLDAPNodeStruct->fAuthCredential != nil)
		{
			free( inLDAPNodeStruct->fAuthCredential );
			inLDAPNodeStruct->fAuthCredential = nil;
		}

		if (inLDAPNodeStruct->fAuthType != nil)
		{
			free( inLDAPNodeStruct->fAuthType );
			inLDAPNodeStruct->fAuthType = nil;
		}
		inLDAPNodeStruct->fRefCount					= 0;
		inLDAPNodeStruct->fOperationsCount			= 0;
		inLDAPNodeStruct->fLDAPConfigTableIndex		= 0;
		inLDAPNodeStruct->fDirectLDAPPort			= 389;
		inLDAPNodeStruct->fConnectionStatus			= kConnectionSafe;
		inLDAPNodeStruct->fDelayedBindTime			= time( nil ) + 120;
		inLDAPNodeStruct->fConnectionActiveCount	= 0;
		inLDAPNodeStruct->fIdleTOCount				= 0;
		inLDAPNodeStruct->fIdleTO					= 4; //based on periodic task of 30 secs and 2 minutes default TO
		inLDAPNodeStruct->fDelayRebindTry			= 120;
	}

	return(siResult);

}// CleanLDAPNodeStruct

// ---------------------------------------------------------------------------
//	* BindProc
// ---------------------------------------------------------------------------

sInt32 CLDAPNode::BindProc ( sLDAPNodeStruct *inLDAPNodeStruct, CLDAPv3Configs *inConfigFromXML, bool bSessionBased, bool bForceBind )
{

    sInt32				siResult		= eDSNoErr;
    int					bindMsgId		= 0;
	int					version			= -1;
    sLDAPConfigData	   *pConfig			= nil;
    char			   *ldapAcct		= nil;
    char			   *ldapPasswd		= nil;
    int					openTO			= kLDAPDefaultOpenCloseTimeoutInSeconds;
	LDAP			   *inLDAPHost		= inLDAPNodeStruct->fHost;
	LDAPMessage		   *result			= nil;
	int					ldapReturnCode	= 0;
	
	try
	{
		if ( inLDAPNodeStruct == nil ) throw( (sInt32)eDSNullParameter );
		
		if (inLDAPNodeStruct->fLDAPSessionMutex != nil)
		{
			inLDAPNodeStruct->fLDAPSessionMutex->Wait();
		}
        // Here is the bind to the LDAP server
		// Note that there may be stored name/password in the config table
		// ie. always use the config table data if authentication has not explicitly been set
		// use LDAPAuthNodeMap if inLDAPNodeStruct contains a username
		
		//check that we were already here
		if (inLDAPHost == NULL)
		{
			//retrieve the config data
			//don't need to retrieve for the case of "generic unknown" so don't check index 0
			if (( inLDAPNodeStruct->fLDAPConfigTableIndex < gLDAPConfigTableLen) && ( inLDAPNodeStruct->fLDAPConfigTableIndex >= 1 ))
			{
				pConfig = (sLDAPConfigData *)gLDAPConfigTable->GetItemData( inLDAPNodeStruct->fLDAPConfigTableIndex );
				if (pConfig != nil)
				{
					if ( (pConfig->bSecureUse) && (inLDAPNodeStruct->fUserName == nil) )
					{
						if (pConfig->fServerAccount != nil)
						{
							ldapAcct = new char[1+::strlen(pConfig->fServerAccount)];
							::strcpy( ldapAcct, pConfig->fServerAccount );
						}
						if (pConfig->fServerPassword != nil)
						{
							ldapPasswd = new char[1+::strlen(pConfig->fServerPassword)];
							::strcpy( ldapPasswd, pConfig->fServerPassword );
						}
					}
					else
					{
						if (inLDAPNodeStruct->fUserName != nil)
						{
							ldapAcct = new char[1+::strlen(inLDAPNodeStruct->fUserName)];
							::strcpy( ldapAcct, inLDAPNodeStruct->fUserName );
						}
						if (inLDAPNodeStruct->fAuthCredential != nil)
						{
							if (inLDAPNodeStruct->fAuthType != nil)
							{
								//auth type of clear text means char * password
								if (strcmp(inLDAPNodeStruct->fAuthType,kDSStdAuthClearText) == 0)
								{
									ldapPasswd = new char[1+::strlen((char*)(inLDAPNodeStruct->fAuthCredential))];
									::strcpy( ldapPasswd, (char*)(inLDAPNodeStruct->fAuthCredential) );
								}
							}
							else //default is password
							{
								ldapPasswd = new char[1+::strlen((char*)(inLDAPNodeStruct->fAuthCredential))];
								::strcpy( ldapPasswd, (char*)(inLDAPNodeStruct->fAuthCredential) );
							}
						}
					}
					openTO = pConfig->fOpenCloseTimeout;
				}
			}

			// if we are in an unknown state, and we are not forcebinding (i.e., checkFailed), let's wait
			if( !bForceBind && inLDAPNodeStruct->fConnectionStatus == kConnectionUnknown )
			{
				//first lets spawn our checking thread if this has not already been done
				EnsureCheckFailedConnectionsThreadIsRunning();
				// let's go to sleep for the designated time and wait to see if it becomes available.
				// rather than take the chance and hang the system on a timeout...
				struct mach_timebase_info	timeBaseInfo;
				uint64_t					delay;
				
				mach_timebase_info( &timeBaseInfo );
				delay = (((uint64_t)(NSEC_PER_SEC * openTO) * (uint64_t)timeBaseInfo.denom) / (uint64_t)timeBaseInfo.numer);

				// we need to unlock this session so the thread can check it out of band..
				if (inLDAPNodeStruct->fLDAPSessionMutex != nil)
				{
					inLDAPNodeStruct->fLDAPSessionMutex->Signal();
				}
				
				// let's wait openTO seconds...
				mach_wait_until( mach_absolute_time() + delay );
				
				if (inLDAPNodeStruct->fLDAPSessionMutex != nil)
				{
					inLDAPNodeStruct->fLDAPSessionMutex->Wait();
				}

				if (pConfig != nil)
				{
					// let's also reset the last used one so we try the full suite of replicas up front
					sReplicaInfo *replicaList = pConfig->fReplicaHosts;
					while( replicaList != nil )
					{
						replicaList->bUsedLast = false;
						replicaList = replicaList->fNext;
					}
				}
			}

			if( !bForceBind && inLDAPNodeStruct->fConnectionStatus != kConnectionSafe )
			{
				throw( (sInt32)eDSCannotAccessSession );
			}
			
			if (inLDAPNodeStruct->fLDAPConfigTableIndex != 0)
			{
				if (pConfig != nil)
				{
					inLDAPHost = InitLDAPConnection( inLDAPNodeStruct, pConfig, inConfigFromXML, bSessionBased );
					//inLDAPHost = ldap_init( pConfig->fServerName, pConfig->fServerPort );
				}
			}
			else
			{
				//directed open with no configuration will not support replica picking
				inLDAPHost = ldap_init( inLDAPNodeStruct->fServerName, inLDAPNodeStruct->fDirectLDAPPort );
			}
			
			if ( inLDAPHost == nil )
			{
				// since we did an InitLDAPConnection, which does Establish, we can safely assume we've failed all together

				//log this failed connection
				LogFailedConnection("InitLDAPConnection or ldap_init failure", inLDAPNodeStruct->fServerName, inLDAPNodeStruct->fDelayRebindTry);
				if (bForceBind)
				{
					inLDAPNodeStruct->fConnectionStatus = kConnectionUnsafe;
				}
				else
				{
					inLDAPNodeStruct->fConnectionStatus = kConnectionUnknown;
				}
				inLDAPNodeStruct->fDelayedBindTime = time( nil ) + inLDAPNodeStruct->fDelayRebindTry;
				throw( (sInt32)eDSCannotAccessSession );
			}
			
			//if not already connected from replica searching
			if (inLDAPNodeStruct->fHost == nil)
			{
				if (pConfig != nil)
				{
					if ( pConfig->bIsSSL )
					{
						int ldapOptVal = LDAP_OPT_X_TLS_HARD;
						ldap_set_option(inLDAPHost, LDAP_OPT_X_TLS, &ldapOptVal);
					}
				}
				/* LDAPv3 only */
				version = LDAP_VERSION3;
				ldap_set_option( inLDAPHost, LDAP_OPT_PROTOCOL_VERSION, &version );
	
				// let's do a SASLbind if possible
				ldapReturnCode = doSASLBindAttemptIfPossible( inLDAPHost, pConfig, ldapAcct, ldapPasswd );
				
				// if we didn't have a local error or LDAP_OTHER, then we either failed or succeeded so no need to continue
				if( ldapReturnCode != LDAP_LOCAL_ERROR && ldapReturnCode != LDAP_OTHER )
				{
					// if we didn't have a success then we failed for some reason
					if( ldapReturnCode != LDAP_SUCCESS )
					{
						DBGLOG( kLogPlugin, "CLDAPNode::Failed doing SASL Authentication" );
						if (bForceBind)
						{
							inLDAPNodeStruct->fConnectionStatus = kConnectionUnsafe;
						}
						else
						{
							inLDAPNodeStruct->fConnectionStatus = kConnectionUnknown;
						}
						inLDAPNodeStruct->fDelayedBindTime = time( nil ) + inLDAPNodeStruct->fDelayRebindTry;
						throw( (sInt32)eDSCannotAccessSession );
					}
				}
				else
				{
					// if we couldn't do a SASL, let's fallback to normal bind.
					DBGLOG( kLogPlugin, "CLDAPNode::SASL Authentication didn't work, failing through to bind" );

					//this is our and only our LDAP session for now
					//need to use our timeout so we don't hang indefinitely
					bindMsgId = ldap_bind( inLDAPHost, ldapAcct, ldapPasswd, LDAP_AUTH_SIMPLE );
				
					struct	timeval	tv;
					tv.tv_sec		= (openTO ? openTO : kLDAPDefaultOpenCloseTimeoutInSeconds);  // never allow 0
					tv.tv_usec		= 0;
					ldapReturnCode	= ldap_result(inLDAPHost, bindMsgId, 0, &tv, &result);
					
					if ( ldapReturnCode == -1 )
					{
						throw( (sInt32)eDSCannotAccessSession );
					}
					else if ( ldapReturnCode == 0 )
					{
						// timed out, let's forget it
						ldap_abandon(inLDAPHost, bindMsgId);
						
						//log this timed out connection
						if ( (pConfig != nil) && (pConfig->fServerName != nil) )
						{
							LogFailedConnection("Bind timeout", pConfig->fServerName, inLDAPNodeStruct->fDelayRebindTry);
						}
						else
						{
							LogFailedConnection("Bind timeout", inLDAPNodeStruct->fServerName, inLDAPNodeStruct->fDelayRebindTry);
						}
						if (bForceBind)
						{
							inLDAPNodeStruct->fConnectionStatus = kConnectionUnsafe;
						}
						else
						{
							inLDAPNodeStruct->fConnectionStatus = kConnectionUnknown;
						}
						inLDAPNodeStruct->fDelayedBindTime = time( nil ) + inLDAPNodeStruct->fDelayRebindTry;
						throw( (sInt32)eDSCannotAccessSession );
					}
					else if ( ldap_result2error(inLDAPHost, result, 1) != LDAP_SUCCESS )
					{
						//log this failed connection
						if ( (pConfig != nil) && (pConfig->fServerName != nil) )
						{
							LogFailedConnection("Bind failure", pConfig->fServerName, inLDAPNodeStruct->fDelayRebindTry);;
						}
						else
						{
							LogFailedConnection("Bind failure", inLDAPNodeStruct->fServerName, inLDAPNodeStruct->fDelayRebindTry);
						}
						if (bForceBind)
						{
							inLDAPNodeStruct->fConnectionStatus = kConnectionUnsafe;
						}
						else
						{
							inLDAPNodeStruct->fConnectionStatus = kConnectionUnknown;
						}
						inLDAPNodeStruct->fDelayedBindTime = time( nil ) + inLDAPNodeStruct->fDelayRebindTry;
						inLDAPNodeStruct->fHost = inLDAPHost;
						throw( (sInt32)eDSCannotAccessSession );
					}
				}
				inLDAPNodeStruct->fConnectionStatus = kConnectionSafe;
				inLDAPNodeStruct->fHost = inLDAPHost;
			}

			//result is consumed above within ldap_result2error
			result = nil;
		}
		
	} // try
	
	catch ( sInt32 err )
	{
		siResult = err;
	}
	
	if (ldapAcct != nil)
	{
		delete (ldapAcct);
		ldapAcct = nil;
	}
	if (ldapPasswd != nil)
	{
		delete (ldapPasswd);
		ldapPasswd = nil;
	}
	
	if (inLDAPNodeStruct->fLDAPSessionMutex != nil)
	{
		inLDAPNodeStruct->fLDAPSessionMutex->Signal();
	}

	return (siResult);
	
}// BindProc


//------------------------------------------------------------------------------------
//	* GetNamingContexts
//------------------------------------------------------------------------------------

char** CLDAPNode::GetNamingContexts( LDAP *inHost, int inSearchTO, uInt32 *outCount )
{
	sInt32				siResult			= eDSRecordNotFound;
	bool				bResultFound		= false;
    int					ldapMsgId			= 0;
	LDAPMessage		   *result				= nil;
	int					ldapReturnCode		= 0;
	char			   *attrs[2]			= {"namingContexts",NULL};
	BerElement		   *ber;
	struct berval	  **bValues;
	char			   *pAttr				= nil;
	char			  **outMapSearchBases	= nil;

	//search for the specific LDAP record namingContexts at the rootDSE which may contain
	//the list of LDAP server search bases
	
	// here is the call to the LDAP server asynchronously which requires
	// host handle, search base, search scope(LDAP_SCOPE_SUBTREE for all), search filter,
	// attribute list (NULL for all), return attrs values flag
	// Note: asynchronous call is made so that a MsgId can be used for future calls
	// This returns us the message ID which is used to query the server for the results
	
	*outCount = 0;
	
	ldapReturnCode = ldap_search_ext(	inHost,
										"",
										LDAP_SCOPE_BASE,
										"(objectclass=*)",
										attrs,
										0,
										NULL,
										NULL,
										0, 0, 
										&ldapMsgId );

	if (ldapReturnCode == LDAP_SUCCESS)
	{
		bResultFound = true;
		//retrieve the actual LDAP record data for use internally
		//useful only from the read-only perspective
		struct	timeval	tv;
		tv.tv_usec	= 0;
		if (inSearchTO == 0)
		{
			tv.tv_sec	= kLDAPDefaultOpenCloseTimeoutInSeconds; //since this may be an implicit bind and don't want to block forever
		}
		else
		{
			tv.tv_sec	= inSearchTO;
		}
		ldapReturnCode = ldap_result(inHost, ldapMsgId, 0, &tv, &result);
	}

	if (	(bResultFound) &&
			( ldapReturnCode == LDAP_RES_SEARCH_ENTRY ) )
	{
		//get the search base list here
		//parse the attributes in the result - should only be one ie. namingContexts
		pAttr = ldap_first_attribute (inHost, result, &ber );
		if (pAttr != nil)
		{
			if (( bValues = ldap_get_values_len (inHost, result, pAttr )) != NULL)
			{
				// calculate the number of values for this attribute
				uInt32 valCount = 0;
				for (int ii = 0; bValues[ii] != nil; ii++ )
				{
					valCount++;
				}
				outMapSearchBases = (char **) calloc( valCount+1, sizeof(char *));
				// for each value of the attribute we need to parse and add as an entry to the outList
				for (int i = 0; (bValues[i] != nil) && (bValues[i]->bv_val != nil); i++ )
				{
					outMapSearchBases[i] = strdup(bValues[i]->bv_val);
					(*outCount)++;
					siResult = eDSNoErr;
				}
				ldap_value_free_len(bValues);
			} // if bValues = ldap_get_values_len ...
			ldap_memfree( pAttr );
		} // if pAttr != nil
			
		if (ber != nil)
		{
			ber_free( ber, 0 );
		}
			
		ldap_msgfree( result );
		result = nil;

	} // if bResultFound and ldapReturnCode okay
	else if (ldapReturnCode == LDAP_TIMEOUT)
	{
		siResult = eDSServerTimeout;
		if ( result != nil )
		{
			ldap_msgfree( result );
			result = nil;
			outMapSearchBases = (char **) -1;   // this signifies a bad server/no response so we don't hang
		}
	}
	else
	{
		siResult = eDSRecordNotFound;
		if ( result != nil )
		{
			ldap_msgfree( result );
			result = nil;
		}
	}
	
	return( outMapSearchBases );

} // GetNamingContexts


//------------------------------------------------------------------------------------
//	* GetSchemaMessage
//------------------------------------------------------------------------------------

sInt32 CLDAPNode::GetSchemaMessage ( LDAP *inHost, int inSearchTO, LDAPMessage **outResultMsg )
{
	sInt32				siResult		= eDSNoErr;
	bool				bResultFound	= false;
    int					ldapMsgId		= 0;
	LDAPMessage		   *result			= nil;
	int					ldapReturnCode	= 0;
	char			   *sattrs[2]		= {"subschemasubentry",NULL};
	char			   *attrs[2]		= {"objectclasses",NULL};
	char			   *subschemaDN		= nil;
	BerElement		   *ber;
	struct berval	  **bValues;
	char			   *pAttr			= nil;

	try
	{
		//search for the specific LDAP record subschemasubentry at the rootDSE which contains
		//the "dn" of the subschema record
		
		// here is the call to the LDAP server asynchronously which requires
		// host handle, search base, search scope(LDAP_SCOPE_SUBTREE for all), search filter,
		// attribute list (NULL for all), return attrs values flag
		// Note: asynchronous call is made so that a MsgId can be used for future calls
		// This returns us the message ID which is used to query the server for the results
		if ( (ldapMsgId = ldap_search( inHost, "", LDAP_SCOPE_BASE, "(objectclass=*)", sattrs, 0) ) == -1 )
		{
			bResultFound = false;
		}
		else
		{
			bResultFound = true;
			//retrieve the actual LDAP record data for use internally
			//useful only from the read-only perspective
			struct	timeval	tv;
			tv.tv_usec	= 0;
			if (inSearchTO == 0)
			{
				tv.tv_sec	= kLDAPDefaultSearchTimeoutInSeconds; // don't want to block forever
			}
			else
			{
				tv.tv_sec	= inSearchTO;
			}
			ldapReturnCode = ldap_result(inHost, ldapMsgId, 0, &tv, &result);
		}
	
		if (	(bResultFound) &&
				( ldapReturnCode == LDAP_RES_SEARCH_ENTRY ) )
		{
			siResult = eDSNoErr;
			//get the subschemaDN here
			//parse the attributes in the result - should only be one ie. subschemasubentry
			for (	pAttr = ldap_first_attribute (inHost, result, &ber );
						pAttr != NULL; pAttr = ldap_next_attribute(inHost, result, ber ) )
			{
				if (( bValues = ldap_get_values_len (inHost, result, pAttr )) != NULL)
				{					
					// should be only one value of the attribute
					if ( bValues[0] != NULL )
					{
						subschemaDN = (char *) calloc(1, bValues[0]->bv_len + 1);
						strcpy(subschemaDN,bValues[0]->bv_val);
					}
					
					ldap_value_free_len(bValues);
				} // if bValues = ldap_get_values_len ...
											
				if (pAttr != nil)
				{
					ldap_memfree( pAttr );
				}
					
			} // for ( loop over ldap_next_attribute )
				
			if (ber != nil)
			{
				ber_free( ber, 0 );
			}
				
			ldap_msgfree( result );
			result = nil;

		} // if bResultFound and ldapReturnCode okay
		else if (ldapReturnCode == LDAP_TIMEOUT)
		{
	     	siResult = eDSServerTimeout;
			if ( result != nil )
			{
				ldap_msgfree( result );
				result = nil;
			}
		}
		else
		{
	     	siResult = eDSRecordNotFound;
			if ( result != nil )
			{
				ldap_msgfree( result );
				result = nil;
			}
		}
		
		if (subschemaDN != nil)
		{
			//here we call to get the actual subschema record
			
			//here is the call to the LDAP server asynchronously which requires
			// host handle, search base, search scope(LDAP_SCOPE_SUBTREE for all), search filter,
			// attribute list (NULL for all), return attrs values flag
			// Note: asynchronous call is made so that a MsgId can be used for future calls
			// This returns us the message ID which is used to query the server for the results
			if ( (ldapMsgId = ldap_search( inHost, subschemaDN, LDAP_SCOPE_BASE, "(objectclass=subSchema)", attrs, 0) ) == -1 )
			{
				bResultFound = false;
			}
			else
			{
				bResultFound = true;
				//retrieve the actual LDAP record data for use internally
				//useful only from the read-only perspective
				//KW when write capability is added, we will need to re-read the result after a write
				struct	timeval	tv;
				tv.tv_usec	= 0;
				if (inSearchTO == 0)
				{
					tv.tv_sec	= kLDAPDefaultSearchTimeoutInSeconds; // don't want to block forever
				}
				else
				{
					tv.tv_sec	= inSearchTO;
				}
				ldapReturnCode = ldap_result(inHost, ldapMsgId, 0, &tv, &result);
			}
			
			free(subschemaDN);
			subschemaDN = nil;
		
			if (	(bResultFound) &&
					( ldapReturnCode == LDAP_RES_SEARCH_ENTRY ) )
			{
				siResult = eDSNoErr;
			} // if bResultFound and ldapReturnCode okay
			else if (ldapReturnCode == LDAP_TIMEOUT)
			{
				siResult = eDSServerTimeout;
				if ( result != nil )
				{
					ldap_msgfree( result );
					result = nil;
				}
			}
			else
			{
				siResult = eDSRecordNotFound;
				if ( result != nil )
				{
					ldap_msgfree( result );
					result = nil;
				}
			}
		}
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	if (result != nil)
	{
		*outResultMsg = result;
	}

	return( siResult );

} // GetSchemaMessage


//------------------------------------------------------------------------------------
//	* IsTokenNotATag
//------------------------------------------------------------------------------------

bool CLDAPNode::IsTokenNotATag ( char *inToken )
{
	
	if (inToken == nil)
	{
		return true;
	}
	
	//check for first char in inToken as an uppercase letter in the following set
	//"NDOSAMX" since that will cover the following tags
	//NAME,DESC,OBSOLETE,SUP,ABSTRACT,STRUCTURAL,AUXILIARY,MUST,MAY,X-ORIGIN

	switch(*inToken)
	{
		case 'N':
		case 'D':
		case 'O':
		case 'S':
		case 'A':
		case 'M':
		case 'X':
		
			if (strcmp(inToken,"DESC") == 0)
			{
				return false;
			}
		
			if (strcmp(inToken,"SUP") == 0)
			{
				return false;
			}
			
			if (strlen(inToken) > 7)
			{
				if (strcmp(inToken,"OBSOLETE") == 0)
				{
					return false;
				}
		
				if (strcmp(inToken,"ABSTRACT") == 0)
				{
					return false;
				}
			
				if (strcmp(inToken,"STRUCTURAL") == 0)
				{
					return false;
				}
			
				if (strcmp(inToken,"AUXILIARY") == 0)
				{
					return false;
				}

				if (strcmp(inToken,"X-ORIGIN") == 0) //appears that iPlanet uses a non-standard tag ie. post RFC 2252
				{
					return false;
				}
			}
		
			if (strcmp(inToken,"MUST") == 0)
			{
				return false;
			}
		
			if (strcmp(inToken,"MAY") == 0)
			{
				return false;
			}
		
			if (strcmp(inToken,"NAME") == 0)
			{
				return false;
			}
			break;
		default:
			break;
	}

	return( true );

} // IsTokenNotATag

// ---------------------------------------------------------------------------
//	* Lock Session
// ---------------------------------------------------------------------------

LDAP* CLDAPNode::LockSession( sLDAPContextData *inContext )
{
	sLDAPNodeStruct		   *pLDAPNodeStruct		= nil;
	LDAPNodeMapI			aLDAPNodeMapI;

	if (inContext != nil)
	{
		inContext->fLDAPNodeStruct = nil;
		if (inContext->authCallActive)
		{
			if (inContext->fLDAPSessionMutex != nil)
			{
				inContext->fLDAPSessionMutex->Wait();
			}
			return inContext->fHost;
		}
		else
		{
			string aNodeName(inContext->fName);
			fLDAPNodeOpenMutex.Wait();
			aLDAPNodeMapI	= fLDAPNodeMap.find(aNodeName);
			if (aLDAPNodeMapI != fLDAPNodeMap.end())
			{
				pLDAPNodeStruct = aLDAPNodeMapI->second;
				pLDAPNodeStruct->fRefCount++;
				pLDAPNodeStruct->fOperationsCount++;
			}
			fLDAPNodeOpenMutex.Signal();

			if (pLDAPNodeStruct != nil)
			{
				if (pLDAPNodeStruct->fLDAPSessionMutex != nil)
				{
					pLDAPNodeStruct->fLDAPSessionMutex->Wait();
				}
				//place the node struct into the context for using the mutex in the UnLockSession call
				inContext->fLDAPNodeStruct = pLDAPNodeStruct;
				return pLDAPNodeStruct->fHost;
			}
		}
	}
	return nil;
} // LockSession

// ---------------------------------------------------------------------------
//	* UnLock Session
// ---------------------------------------------------------------------------

void CLDAPNode::UnLockSession( sLDAPContextData *inContext, bool inHasFailed, bool inNewMutex )
{
	sLDAPNodeStruct		   *pLDAPNodeStruct		= nil;
	LDAPNodeMapI			aLDAPNodeMapI;
	DSMutexSemaphore	   *aMutex				= nil;
	sLDAPNodeStruct		   *aLDAPNodeStruct		= nil;
	
	if (inContext != nil)
	{
		if ( (inContext->authCallActive) && !inNewMutex )
		{
			if (inContext->fLDAPSessionMutex != nil)
			{
				inContext->fLDAPSessionMutex->Signal();
			}
		}
		else
		{
			if ( (inContext->fLDAPNodeStruct != nil) && (inContext->fLDAPNodeStruct->fLDAPSessionMutex != nil) )
			{
				if (inHasFailed)
				{
					//log this failed connection in a search operation
					LogFailedConnection("Search connection failure", inContext->fLDAPNodeStruct->fServerName, inContext->fLDAPNodeStruct->fDelayRebindTry);
					inContext->fLDAPNodeStruct->fConnectionStatus = kConnectionUnknown;
					inContext->fLDAPNodeStruct->fDelayedBindTime = time( nil ) + inContext->fLDAPNodeStruct->fDelayRebindTry;
				}
				aMutex = inContext->fLDAPNodeStruct->fLDAPSessionMutex;
				aLDAPNodeStruct = inContext->fLDAPNodeStruct;
				inContext->fLDAPNodeStruct = nil;
				aMutex->Signal();
				
				//now go ahead and update the node table
				string aNodeName(inContext->fName);
				fLDAPNodeOpenMutex.Wait();
				aLDAPNodeMapI	= fLDAPNodeMap.find(aNodeName);
				if (aLDAPNodeMapI != fLDAPNodeMap.end())
				{
					pLDAPNodeStruct = aLDAPNodeMapI->second;
					if (pLDAPNodeStruct != nil)
					{
						pLDAPNodeStruct->fRefCount--;
						pLDAPNodeStruct->fOperationsCount--;
						if (pLDAPNodeStruct->fRefCount == 0 && pLDAPNodeStruct->fConnectionStatus == kConnectionSafe)
						{
							//remove the entry if refcount is zero after cleaning contents
							CleanLDAPNodeStruct(pLDAPNodeStruct);
							fLDAPNodeMap.erase(aNodeName);
							free(pLDAPNodeStruct);
						}
					}
				}
				else
				{
					for ( LDAPNodeVectorI iter = fDeadPoolLDAPNodeVector.begin(); iter != fDeadPoolLDAPNodeVector.end(); ++iter)
					{
						if ( *iter == aLDAPNodeStruct )
						{
							//don't care about the ref count in the dead pool
							//decrement the ops ref count
							aLDAPNodeStruct->fOperationsCount--;
							if (aLDAPNodeStruct->fOperationsCount == 0 && aLDAPNodeStruct->fConnectionStatus == kConnectionSafe)
							{
								CleanLDAPNodeStruct(aLDAPNodeStruct);
								fDeadPoolLDAPNodeVector.erase(iter);
								free(aLDAPNodeStruct);
							}
							break;
						}
					}
				}
				
				fLDAPNodeOpenMutex.Signal();
			}
		}
	}
} //UnLockSession


// ---------------------------------------------------------------------------
//     * Check Idles
// ---------------------------------------------------------------------------

void CLDAPNode::CheckIdles( void )
{
	sLDAPNodeStruct	   *pLDAPNodeStruct = nil;
	LDAPNodeMapI		aLDAPNodeMapI;
	bool				bShouldWeCheck  = false;

	//DBGLOG( kLogPlugin, "CLDAPNode::CheckIdles" );

	fLDAPNodeOpenMutex.Wait();

	for (aLDAPNodeMapI = fLDAPNodeMap.begin(); aLDAPNodeMapI != fLDAPNodeMap.end(); ++aLDAPNodeMapI)
	{
		pLDAPNodeStruct = aLDAPNodeMapI->second;
		if (pLDAPNodeStruct->fConnectionStatus != kConnectionSafe)
		{
			bShouldWeCheck = true;
		}
		if (pLDAPNodeStruct->fConnectionActiveCount == 0) //no active connections
		{
			if (pLDAPNodeStruct->fIdleTOCount == pLDAPNodeStruct->fIdleTO) //idle timeout has expired
			{
				//no need to grab session mutexes since no one can get the mutex
				//while the table mutex is held and there are no active operations
				if (pLDAPNodeStruct->fHost != nil)
				{
					ldap_unbind( pLDAPNodeStruct->fHost ); //unbind the connection and nil out
					pLDAPNodeStruct->fHost = nil;
				}
				pLDAPNodeStruct->fIdleTOCount = 0;
			}
			else
			{
				pLDAPNodeStruct->fIdleTOCount++;
			}
		}
	}

	//check that there is actually at least one entry in the table that needs to be checked
	if (bShouldWeCheck)
	{
		// while we are here, let's also kick off the thread for checking failed..
		EnsureCheckFailedConnectionsThreadIsRunning();
	}
	
	fLDAPNodeOpenMutex.Signal();

} //CheckIdles

// ---------------------------------------------------------------------------
//     * CheckFailed
// ---------------------------------------------------------------------------

void CLDAPNode::CheckFailed( void )
{
	// This function is called by the thread only...
	sLDAPNodeStruct	   *pLDAPNodeStruct = nil;
	LDAPNodeMapI		aLDAPNodeMapI;
	LDAPNodeMap			aLDAPNodeMap;

	//DBGLOG( kLogPlugin, "CLDAPNode::CheckFailed" );

	// let's copy all the failed connections to a new map table....
	fLDAPNodeOpenMutex.Wait();
	for (aLDAPNodeMapI = fLDAPNodeMap.begin(); aLDAPNodeMapI != fLDAPNodeMap.end(); ++aLDAPNodeMapI)
	{
		if( aLDAPNodeMapI->second->fConnectionStatus != kConnectionSafe ) {
			pLDAPNodeStruct = (sLDAPNodeStruct *) aLDAPNodeMapI->second;

			// if the failed have a host, we should unbind from them and clear it
			if( pLDAPNodeStruct->fHost != nil )
			{
				ldap_unbind_ext( pLDAPNodeStruct->fHost, NULL, NULL );
				pLDAPNodeStruct->fHost = NULL;
			}
			aLDAPNodeMap[aLDAPNodeMapI->first] = pLDAPNodeStruct;
		}
	}
	fLDAPNodeOpenMutex.Signal();	

	for (aLDAPNodeMapI = aLDAPNodeMap.begin(); aLDAPNodeMapI != aLDAPNodeMap.end(); ++aLDAPNodeMapI) {

		pLDAPNodeStruct = (sLDAPNodeStruct *) aLDAPNodeMapI->second;
		
		// if we have an Unknown state or a failed connection and it is time to retry....
		if( pLDAPNodeStruct->fConnectionStatus == kConnectionUnknown || time( nil ) > pLDAPNodeStruct->fDelayedBindTime ) {
			
			// Let's attempt a bind, with the override, since no one else will use it right now...
			BindProc( pLDAPNodeStruct, nil, false, true );

		}
	}

} //CheckFailed

// ---------------------------------------------------------------------------
//     * NetTransition
// ---------------------------------------------------------------------------

void CLDAPNode::NetTransition( void )
{
	// let's flag all replicas to be rebuilt on network transition... seems like the right thing to do...
	fLDAPNodeOpenMutex.Wait();
	
	for (uInt32 iTableIndex=1; iTableIndex<gLDAPConfigTableLen; iTableIndex++)
	{
		sLDAPConfigData *pConfig = (sLDAPConfigData *)gLDAPConfigTable->GetItemData( iTableIndex );
		if (pConfig != nil)
		{
			pConfig->bBuildReplicaList = true;
		}// if config entry not nil
	} // loop over config table entries
	
	// if the thread isn't already going.....
	if( (fCheckThreadActive == false) && (fLDAPNodeMap.size() > 0) )
	{
		LDAPNodeMapI	aLDAPNodeMapI;
		
		// let's loop through all connections and flag them all as unknown
		for (aLDAPNodeMapI = fLDAPNodeMap.begin(); aLDAPNodeMapI != fLDAPNodeMap.end(); ++aLDAPNodeMapI)
		{
			sLDAPNodeStruct *pLDAPNodeStruct = aLDAPNodeMapI->second;
			
			pLDAPNodeStruct->fConnectionStatus = kConnectionUnknown;
			pLDAPNodeStruct->fDelayedBindTime = nil;
			
			if ( pLDAPNodeStruct->fHost != nil ) //no active connections
			{
				ldap_unbind( pLDAPNodeStruct->fHost ); //unbind the connection and nil out
				pLDAPNodeStruct->fHost = nil;
			}
		}
		
		// while we are here, let's also kick off the thread for checking failed..
		EnsureCheckFailedConnectionsThreadIsRunning();
	}

	fLDAPNodeOpenMutex.Signal();
	
} //NetTransition

// ---------------------------------------------------------------------------
//     * ActiveConnection
// ---------------------------------------------------------------------------

void CLDAPNode::ActiveConnection( char *inNodeName )
{
	sLDAPNodeStruct	   *pLDAPNodeStruct = nil;
	LDAPNodeMapI		aLDAPNodeMapI;

	//DBGLOG( kLogPlugin, "CLDAPNode::ActiveConnection" );

	if (inNodeName != nil)
	{
		fLDAPNodeOpenMutex.Wait();

		string aNodeName(inNodeName);
		aLDAPNodeMapI   = fLDAPNodeMap.find(aNodeName);
		if (aLDAPNodeMapI != fLDAPNodeMap.end())
		{
			pLDAPNodeStruct = aLDAPNodeMapI->second;
			pLDAPNodeStruct->fConnectionActiveCount++;
			pLDAPNodeStruct->fIdleTOCount = 0;
		}

		fLDAPNodeOpenMutex.Signal();
	}

} //ActiveConnection

// ---------------------------------------------------------------------------
//     * IdleConnection
// ---------------------------------------------------------------------------

void CLDAPNode::IdleConnection( char *inNodeName )
{
	sLDAPNodeStruct	   *pLDAPNodeStruct = nil;
	LDAPNodeMapI		aLDAPNodeMapI;

	//DBGLOG( kLogPlugin, "CLDAPNode::IdleConnection" );

	if (inNodeName != nil)
	{
		fLDAPNodeOpenMutex.Wait();

		string aNodeName(inNodeName);
		aLDAPNodeMapI   = fLDAPNodeMap.find(aNodeName);
		if (aLDAPNodeMapI != fLDAPNodeMap.end())
		{
			pLDAPNodeStruct = aLDAPNodeMapI->second;
			if (pLDAPNodeStruct->fConnectionActiveCount != 0)
			{
				pLDAPNodeStruct->fConnectionActiveCount--;
			}
		}

		fLDAPNodeOpenMutex.Signal();
	}

} //IdleConnection
 
 
// ---------------------------------------------------------------------------
//     * InitLDAPConnection
// ---------------------------------------------------------------------------

LDAP* CLDAPNode::InitLDAPConnection( sLDAPNodeStruct *inLDAPNodeStruct, sLDAPConfigData *inConfig, CLDAPv3Configs *inConfigFromXML, bool bInNeedWriteable )
{

	sReplicaInfo	   *inOutList				= nil;
	sReplicaInfo	   *tailList				= nil;
	struct addrinfo	   *addrList;
	LDAP			   *outHost					= nil;
	sInt32				replicaSearchResult		= eDSNoErr;
	
//use 		inConfig->bBuildReplicaList 	as indicator
//note		struct addrinfo* fReplicaHosts	inside inConfig is the built list
//using 	char *fServerName				 from inConfig was the old way

//assumptions:
//- we have a config
//- need to check if we have built a replica list
//- if not then we try to build it now
//-	check if we already have a replica host list that we can use to get the replica list
//- if yes then we start to use it
//- we retain last used replica inside the config struct

	if (inConfig->bBuildReplicaList)
	{
		// first let's create a string of the servername in case we need it...
		CFStringRef serverStrRef = CFStringCreateWithCString( NULL, inConfig->fServerName, kCFStringEncodingUTF8 );
		
		//if we don't have a list of replicas, let's start one
		if (inConfig->fReplicaHostnames == nil)
		{
			inConfig->fReplicaHostnames = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
		}

		// if we have a list, but it is empty... let's add the main host to the list...
		if( inConfig->fReplicaHostnames != nil && CFArrayGetCount(inConfig->fReplicaHostnames) == 0 )
		{
			CFArrayAppendValue(inConfig->fReplicaHostnames, serverStrRef);
		}

		//use the known replicas to fill out the addrinfo list
		CFIndex numReps = CFArrayGetCount(inConfig->fReplicaHostnames);
		for (CFIndex indexToRep=0; indexToRep < numReps; indexToRep++ )
		{
			CFStringRef replicaStrRef = (CFStringRef)::CFArrayGetValueAtIndex( inConfig->fReplicaHostnames, indexToRep );

			addrList = ResolveHostName(replicaStrRef, inConfig->fServerPort);
			sReplicaInfo* newInfo = (sReplicaInfo *)calloc(1, sizeof(sReplicaInfo));
			if (inOutList == nil)
			{
				inOutList	= newInfo;
				tailList	= newInfo;
			}
			else
			{
				tailList->fNext = newInfo;
				tailList = tailList->fNext;
			}
			newInfo->fAddrInfo = addrList;
			newInfo->hostname = CFStringCreateCopy( kCFAllocatorDefault, replicaStrRef );
		}
		
		// if we only have one, let's make it writeable too...
		if( numReps == 1 )
		{
			inOutList->bWriteable = true;
		}
		
		// First, let's try DNS to see if we can locate closer replicas
		//DNS Service records
		//bug 3249296
		//TODO KW replicaSearchResult = RetrieveDNSServiceReplicas(inConfig->fReplicaHostnames, inConfig->fWriteableHostnames, inConfig->fServerPort, &inOutList);
		replicaSearchResult = eDSNoStdMappingAvailable;  // holder for the above search...
		
		// if we didn't find and DNS records lets do the other methods...
		if( replicaSearchResult != eDSNoErr )
		{
			//root dce altservers and possibly the Open Directory config record
			replicaSearchResult = RetrieveDefinedReplicas(inLDAPNodeStruct, inConfigFromXML, inConfig->fReplicaHostnames, inConfig->fWriteableHostnames, inConfig->fServerPort, &inOutList);
		}
		
		// if we were server mappings we may not have had mappings so use the list we have for now
		if ( replicaSearchResult == eDSNoErr || replicaSearchResult == eDSNoStdMappingAvailable || inConfig->fReplicaHosts == nil )
		{
			// we should always have one, but just in case...
			if( inOutList )
			{
				// need to clean up the old list before we replace it...
				FreeReplicaList( inConfig->fReplicaHosts );
				inConfig->fReplicaHosts = inOutList;
			}
		}
		
		inConfig->bBuildReplicaList = false;
		
		//if no existing writeable list then we use the first entry in the replica list
		//ie. we assume that it is the master writeable hostname -- serverStrRef
		if (inConfig->fWriteableHostnames == nil)
		{
			inConfig->fWriteableHostnames = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
			CFArrayAppendValue(inConfig->fWriteableHostnames, serverStrRef);
		}
		
		if ( inConfigFromXML != nil )
		{
			//here we need to save the hostnames of the replica list in the config file
			inConfigFromXML->UpdateReplicaList(inConfig->fServerName, inConfig->fReplicaHostnames, inConfig->fWriteableHostnames);
		}
		
		CFRelease(serverStrRef);
	}
	
	//case where inLDAPNodeStruct->fHost != nil means that connection was established within Replica Searching methods above
	if ( (replicaSearchResult != eDSCannotAccessSession) && (inLDAPNodeStruct->fHost == nil) )
	{
		//establish a connection using replicas OR
		//simply ldap_init
		if (inConfig->fReplicaHosts != nil)
		{
			//use the writeable hostnames to establish a connection if required
			outHost = EstablishConnection( inConfig->fReplicaHosts, inConfig->fServerPort, inConfig->fOpenCloseTimeout, bInNeedWriteable );
			//provide fallback to try local loopback address if the following 
			//conditions are met
			//ie. if ip address change occurred we need to be able to write the
			//change to ourselves
			//1-failed to connect to any of the replicas
			//2-we are running on server
			//3-looking for a writable replica
			if ( (outHost == nil) && (bInNeedWriteable) )
			{
				// two cases to consider:
				// using loopback, check if this is a replica or not
				// or client needs to connect to the provided IP
				if ((strcmp( inConfig->fServerName, "127.0.0.1" ) == 0)
					 || (strcmp( inConfig->fServerName, "localhost" ) == 0))
				{
					if (gServerOS && !LocalServerIsLDAPReplica())
					{
						outHost = ldap_init( "127.0.0.1", inConfig->fServerPort );
					}
				}
				else
				{
					outHost = ldap_init( inConfig->fServerName, inConfig->fServerPort );
				}
			}
		}
		else //if no list built we fallback to original call
		{
			outHost = ldap_init( inConfig->fServerName, inConfig->fServerPort );
		}
	}
	if (inLDAPNodeStruct->fHost != nil)
	{
		outHost = inLDAPNodeStruct->fHost;
	}
	
	return(outHost);
} //InitLDAPConnection
 

// ---------------------------------------------------------------------------
//     * ResolveHostName
// ---------------------------------------------------------------------------

struct addrinfo* CLDAPNode::ResolveHostName( CFStringRef inServerNameRef, int inPortNumber )
{
	struct addrinfo		hints;
	struct addrinfo	   *res				= nil;
	char				portString[32]	= {0};
	char				serverName[512]	= {0};
	

	if (CFStringGetCString(inServerNameRef, serverName, 512, kCFStringEncodingUTF8))
	{
		//retrieve the addrinfo for this server
		memset(&hints, 0, sizeof(hints));
		hints.ai_family		= PF_UNSPEC; //IPV4 or IPV6
		hints.ai_socktype	= SOCK_STREAM;
		
		sprintf( portString, "%d", inPortNumber );
		if ( getaddrinfo(serverName, portString, &hints, &res) != 0 )
		{
			res = nil;
		}
	}
	return(res);
} //ResolveHostName
 
 
// ---------------------------------------------------------------------------
//     * EstablishConnection
// ---------------------------------------------------------------------------

LDAP* CLDAPNode::EstablishConnection( sReplicaInfo *inList, int inPort, int inOpenTimeout, bool bInNeedWriteable )
{
	const int			maxSockets				= 512;   // lets do half of our max FD_SETSIZE = 512
	LDAP			   *outHost					= nil;
	sReplicaInfo	   *lastUsedReplica			= nil;
	sReplicaInfo	   *resolvedRepIter			= nil;
	struct addrinfo	   *resolvedAddrIter		= nil;
	int					val						= 1;
	int					len						= sizeof(val);
	struct timeval		recvTimeoutVal			= { inOpenTimeout, 0 };
	struct timeval		recheckTimeoutVal		= { 1, 0 };
	struct timeval		quickCheck				= { 0, 3000 };   // 3 milliseconds
	struct timeval		pollValue				= { 0, 0 };
	int					sockCount				= 0;
	fd_set				fdset, fdwrite, fdread;
	int					fcntlFlags				= 0;
	char			   *goodHostAddress			= NULL;
	int				   *sockList				= nil;
	sReplicaInfo	  **replicaPointers			= nil;
	struct addrinfo   **addrinfoPointers		= nil;
	int					sockIter;
	bool				bReachableAddresses		= false;
	bool				bTrySelect				= false;
	
	if (inPort == 0)
	{
		inPort = 389; //default
	}

	//establish a connection
	//simple sequential approach testing connect reachability first
	//TODO KW better scheme needed here
	//use the last used one first and wait say 5 seconds for it to establish
	//then if not try the others in a concurrent manner
	//reuse the passwordserver code for this

	if( inList == NULL )
		return NULL;
	
	// We can't select on more than FD_SETSIZE, so let's limit it there..
	sockList = (int *)calloc( maxSockets, sizeof(int) );
	replicaPointers = (sReplicaInfo **)calloc( maxSockets, sizeof(sReplicaInfo *) );
	addrinfoPointers = (struct addrinfo **)calloc( maxSockets, sizeof(struct addrinfo *) );

	SetSockList( sockList, maxSockets, false );
	FD_ZERO( &fdset );

	for (resolvedRepIter = inList; resolvedRepIter != nil; resolvedRepIter = resolvedRepIter->fNext)
	{
		if ( !bInNeedWriteable || ( bInNeedWriteable && resolvedRepIter->bWriteable ))
		{
			for (resolvedAddrIter = resolvedRepIter->fAddrInfo; resolvedAddrIter != nil && sockCount < maxSockets; resolvedAddrIter = resolvedAddrIter->ai_next)
			{
				// if we haven't found a reachable address yet.
				if( bReachableAddresses == false )
				{
					bReachableAddresses = ReachableAddress( resolvedAddrIter );
				}
				
				if( resolvedRepIter->bUsedLast == true )
				{
					resolvedRepIter->bUsedLast = false;
					lastUsedReplica = resolvedRepIter;
				}
				else
				{
					replicaPointers[sockCount] = resolvedRepIter;
					addrinfoPointers[sockCount] = resolvedAddrIter;
					sockCount++;
				}
			}
		}
	}
	
	// we are worried about bootstrap issues here, so if we don't have a last used, either we were just configured
	// or we just booted, so let's go through some extra effort
	if( lastUsedReplica == NULL && bReachableAddresses == false )
	{
		// if we didn't have a reachable address, let's wait a little just in case we are in bootup or waking up before we try sockets..
		struct mach_timebase_info       timeBaseInfo;
		
		mach_timebase_info( &timeBaseInfo );
		
		int			iReachableCount = 0;
		uint64_t	delay = (((uint64_t)NSEC_PER_SEC * (uint64_t)timeBaseInfo.denom) / (uint64_t)timeBaseInfo.numer);

		// Let's timeout in 50% of the configured time if we don't have an address....
		while( bReachableAddresses == false && iReachableCount < (inOpenTimeout >> 1) )
		{
			// let's wait 1 second... and check again...
			mach_wait_until( mach_absolute_time() + delay );
			iReachableCount++;
			
			// let's check if we any addresses would be reachable
			int iCount;
			for( iCount = 0; iCount < sockCount && bReachableAddresses == false; iCount++ )
			{
				bReachableAddresses = ReachableAddress( addrinfoPointers[iCount] );
			}
		}
	}

	// if we don't have any reachable addresses, there's no reason to try.. and hang the user unnecessarily..
	if( bReachableAddresses )
	{
		//try the last used one first
		if (lastUsedReplica != nil)
		{
			for (resolvedAddrIter = lastUsedReplica->fAddrInfo; resolvedAddrIter != nil && goodHostAddress == nil; resolvedAddrIter = resolvedAddrIter->ai_next)
			{
				if( ReachableAddress(resolvedAddrIter) )
				{
					goodHostAddress = LDAPWithBlockingSocket( resolvedAddrIter, inOpenTimeout );
					if( goodHostAddress )
					{
						DBGLOG2( kLogPlugin, "CLDAPNode::EstablishConnection - Previous replica with IP Address = %s responded for %s", goodHostAddress, (bInNeedWriteable ? "write" : "read") );
					}
				}
			}
		}
	
		// so let's go through all sockets until we finish and timeout and don't have an IP address
		for( sockIter = 0; sockIter < sockCount && goodHostAddress == nil; sockIter++ )
		{
			struct addrinfo *tmpAddress = addrinfoPointers[sockIter];
			
			if( ReachableAddress(tmpAddress) )
			{
				// if it is local, we should try it blocking because it will fail immediately..
				if( IsLocalAddress( tmpAddress ) )
				{
					goodHostAddress = LDAPWithBlockingSocket( tmpAddress, inOpenTimeout );
					if( goodHostAddress )
					{
						lastUsedReplica = replicaPointers[sockIter];
						DBGLOG2( kLogPlugin, "CLDAPNode::EstablishConnection - Attempting to use local address = %s for %s", goodHostAddress, (bInNeedWriteable ? "write" : "read") );
					} else {
						char *tempaddress = ConvertToIPAddress( addrinfoPointers[sockIter] );
						if( tempaddress ) {
							DBGLOG1( kLogPlugin, "CLDAPNode::EstablishConnection - Failed local address connect to = %s", tempaddress );
							free( tempaddress );
						}
					}
				}
				else
				{
					// otherwise we should do a non-blocking socket...
					int aSock = socket( tmpAddress->ai_family, tmpAddress->ai_socktype, tmpAddress->ai_protocol );
					if( aSock != -1 )
					{
						setsockopt( aSock, SOL_SOCKET, SO_NOSIGPIPE, &val, len );
						setsockopt( aSock, SOL_SOCKET, SO_RCVTIMEO, &recvTimeoutVal, sizeof(recvTimeoutVal) );
						
						//non-blocking now
						fcntlFlags = fcntl( aSock, F_GETFL, 0 );
						if( fcntlFlags != -1 )
						{
							if( fcntl(aSock, F_SETFL, fcntlFlags | O_NONBLOCK) != -1 )
							{
								sockList[sockIter] = aSock;
								
								// if this is a -1, then we add it to our select poll...
								if (connect(aSock, tmpAddress->ai_addr, tmpAddress->ai_addrlen) == -1)
								{
									FD_SET( aSock, &fdset );
									bTrySelect = true;
									
									char *tempaddress = ConvertToIPAddress( addrinfoPointers[sockIter] );
									if( tempaddress ) {
										DBGLOG1( kLogPlugin, "CLDAPNode::EstablishConnection - Attempting Replica connect to = %s", tempaddress );
										free( tempaddress );
									}
								}
								else
								{
									goodHostAddress = ConvertToIPAddress( tmpAddress );
									if( goodHostAddress ) {
										lastUsedReplica = replicaPointers[sockIter];
										DBGLOG1( kLogPlugin, "CLDAPNode::EstablishConnection - Immediate Response to = %s", goodHostAddress );
									}
								}
							}
							else
							{
								close( aSock );
								DBGLOG1( kLogPlugin, "CLDAPNode::EstablishConnection - Unable to do non-blocking connect for socket = %d", aSock );
							}
						}
						else
						{
							close( aSock );
							DBGLOG1( kLogPlugin, "CLDAPNode::EstablishConnection - Unable to do get GETFL = %d", aSock );
						}
					}
				}
			}
			else
			{
				char *tempaddress = ConvertToIPAddress( addrinfoPointers[sockIter] );
				if( tempaddress )
				{
					DBGLOG1( kLogPlugin, "CLDAPNode::EstablishConnection - Address not reachable %s", tempaddress );
					free( tempaddress );
				}
			}
			
			if( bTrySelect )
			{
				// let's do our select to see if anything responded....
				FD_COPY( &fdset, &fdwrite );
				FD_COPY( &fdset, &fdread );
				
				// let's do a quick check to see if we've already gotten a response
				if( select( FD_SETSIZE, NULL, &fdwrite, NULL, &quickCheck ) > 0 )
				{
					select( FD_SETSIZE, &fdread, NULL, NULL, &pollValue );   // let's check the read too...

					int checkIter;
					for( checkIter = 0; checkIter <= sockIter; checkIter++ )
					{
						// if we have write, but no read
						int aSock = sockList[checkIter];
						if( aSock != -1 && FD_ISSET(aSock, &fdwrite) && !FD_ISSET(aSock, &fdread) )
						{
							goodHostAddress = ConvertToIPAddress( addrinfoPointers[checkIter] );
							if( goodHostAddress )
							{
								lastUsedReplica = replicaPointers[checkIter];
								DBGLOG( kLogPlugin, "CLDAPNode::EstablishConnection - Got quick response from LDAP Replica");
								break;
							}
						}
					else if( aSock != -1 && FD_ISSET(aSock, &fdwrite) && FD_ISSET(aSock, &fdread) )
					{
						// if we have a bad socket, we will always get an immediate response
						// so let's remove it from the poll
						FD_CLR( aSock, &fdset );
						char *tmpHostAddr = ConvertToIPAddress( addrinfoPointers[checkIter] );
						if( tmpHostAddr ) {
							DBGLOG1( kLogPlugin, "CLDAPNode::EstablishConnection - Quick Check Bad socket to host %s clearing from poll", tmpHostAddr );
							break;
						}
					}					
					}
				}
			}
		}

		// let's do our polling.....
		int iterTry = 0;
		while( goodHostAddress == NULL && iterTry++ < inOpenTimeout && bTrySelect ) 
		{
			FD_COPY( &fdset, &fdwrite ); // we need to copy the fdset, cause it get's zero'd
			FD_COPY( &fdset, &fdread ); // we need to copy the fdset, cause it get's zero'd
			
			//here were need to select on the sockets
			recheckTimeoutVal.tv_sec = 1;
			if( select(FD_SETSIZE, NULL, &fdwrite, NULL, &recheckTimeoutVal) > 0 )
			{
				int checkIter;

				select( FD_SETSIZE, &fdread, NULL, NULL, &pollValue );
				
				for( checkIter = 0; checkIter < sockCount; checkIter++ )
				{
					// if we have write, but no read
					int aSock = sockList[checkIter];
					if( aSock != -1 && FD_ISSET(aSock, &fdwrite) && !FD_ISSET(aSock, &fdread) )
					{
						goodHostAddress = ConvertToIPAddress( addrinfoPointers[checkIter] );
						if( goodHostAddress ) {
							lastUsedReplica = replicaPointers[checkIter];
							DBGLOG1( kLogPlugin, "CLDAPNode::EstablishConnection - Got a response from LDAP Replica %s", goodHostAddress );
							break;
						}
					}
					else if( aSock != -1 && FD_ISSET(aSock, &fdwrite) && FD_ISSET(aSock, &fdread) )
					{
						// if we have a bad socket, we will always get an immediate response
						// so let's remove it from the poll
						FD_CLR( aSock, &fdset );
						char *tmpHostAddr = ConvertToIPAddress( addrinfoPointers[checkIter] );
						if( tmpHostAddr ) {
							DBGLOG1( kLogPlugin, "CLDAPNode::EstablishConnection - Bad socket to host %s clearing from poll", tmpHostAddr );
							break;
						}
					}
				}
			}
		}
		
		// we have an address, so let's do the ldap_init...
		if( goodHostAddress )
		{
			//call ldap_init
			outHost = ldap_init( goodHostAddress, inPort );
			
			//if success set bUsedLast
			if (outHost != nil)
			{
				if( lastUsedReplica )
				{
					lastUsedReplica->bUsedLast = true;
				}
				DBGLOG2( kLogPlugin, "CLDAPNode::EstablishConnection - Using replica with IP Address = %s for %s", goodHostAddress, (bInNeedWriteable ? "write" : "read") );
			}
			else
			{
				DBGLOG1( kLogPlugin, "CLDAPNode::EstablishConnection - ldap_init failed for %s", goodHostAddress );
			}
			
			free( goodHostAddress );
		}
		else
		{
			DBGLOG1( kLogPlugin, "CLDAPNode::EstablishConnection - Could not establish connection for %s", (bInNeedWriteable ? "write" : "read") );
		}
	}
	else
	{
		DBGLOG( kLogPlugin, "CLDAPNode::EstablishConnection - No reachable addresses, possibly no IP addresses" );
	}
	
	// lets close all of our sockets
	SetSockList( sockList, sockCount, true );

	if (sockList != nil)
	{
		free(sockList);
	}
	
	if (replicaPointers != nil)
	{
		free(replicaPointers);
	}
	
	if (addrinfoPointers != nil)
	{
		free(addrinfoPointers);
	}
	
	return(outHost);
} //EstablishConnection

//------------------------------------------------------------------------------------
//	* RetrieveDefinedReplicas
//------------------------------------------------------------------------------------

sInt32 CLDAPNode::RetrieveDefinedReplicas( sLDAPNodeStruct *inLDAPNodeStruct, CLDAPv3Configs *inConfigFromXML, CFMutableArrayRef &inOutRepList, CFMutableArrayRef &inOutWriteableList, int inPort, sReplicaInfo **inOutList )
{
	LDAP			   *outHost			= nil;
	CFMutableArrayRef	aRepList		= NULL;
	CFMutableArrayRef	aWriteableList	= NULL;
	bool				bMessageFound	= false;
	sInt32				foundResult		= eDSRecordNotFound;
	sReplicaInfo	   *aList			= nil;
	sReplicaInfo	   *oldList			= nil;
	sLDAPConfigData    *pConfig			= nil;
	int					openTimeout		= kLDAPDefaultOpenCloseTimeoutInSeconds;
	int					searchTimeout   = 30; //default value for replica info extraction
	int					version			= -1;
    int					bindMsgId		= 0;
    char			   *ldapAcct		= nil;
    char			   *ldapPasswd		= nil;
	LDAPMessage		   *result			= nil;
	int					ldapReturnCode	= 0;

	try
	{
		if ( inLDAPNodeStruct != nil )
		{
			if (inLDAPNodeStruct->fLDAPSessionMutex != nil)
			{
				inLDAPNodeStruct->fLDAPSessionMutex->Wait();
			}
			if (( inLDAPNodeStruct->fLDAPConfigTableIndex < gLDAPConfigTableLen) && ( inLDAPNodeStruct->fLDAPConfigTableIndex >= 1 ))
			{
				pConfig = (sLDAPConfigData *)gLDAPConfigTable->GetItemData( inLDAPNodeStruct->fLDAPConfigTableIndex );
				if (pConfig != nil)
				{
					if ( (pConfig->bSecureUse) && (inLDAPNodeStruct->fUserName == nil) )
					{
						if (pConfig->fServerAccount != nil)
						{
							ldapAcct = new char[1+::strlen(pConfig->fServerAccount)];
							::strcpy( ldapAcct, pConfig->fServerAccount );
						}
						if (pConfig->fServerPassword != nil)
						{
							ldapPasswd = new char[1+::strlen(pConfig->fServerPassword)];
							::strcpy( ldapPasswd, pConfig->fServerPassword );
						}
					}
					else
					{
						if (inLDAPNodeStruct->fUserName != nil)
						{
							ldapAcct = new char[1+::strlen(inLDAPNodeStruct->fUserName)];
							::strcpy( ldapAcct, inLDAPNodeStruct->fUserName );
						}
						if (inLDAPNodeStruct->fAuthCredential != nil)
						{
							if (inLDAPNodeStruct->fAuthType != nil)
							{
								//auth type of clear text means char * password
								if (strcmp(inLDAPNodeStruct->fAuthType,kDSStdAuthClearText) == 0)
								{
									ldapPasswd = new char[1+::strlen((char*)(inLDAPNodeStruct->fAuthCredential))];
									::strcpy( ldapPasswd, (char*)(inLDAPNodeStruct->fAuthCredential) );
								}
							}
							else //default is password
							{
								ldapPasswd = new char[1+::strlen((char*)(inLDAPNodeStruct->fAuthCredential))];
								::strcpy( ldapPasswd, (char*)(inLDAPNodeStruct->fAuthCredential) );
							}
						}
					}
					openTimeout		= pConfig->fOpenCloseTimeout;
					searchTimeout   = pConfig->fSearchTimeout;
				}
			}
			if (inLDAPNodeStruct->fLDAPSessionMutex != nil)
			{
				inLDAPNodeStruct->fLDAPSessionMutex->Signal();
			}
		}
		
		//establish a connection using the addrinfo list
		outHost = EstablishConnection( *inOutList, inPort, openTimeout, false );
		
		//retrieve the current replica list
		//if we find something then we replace the current replica list and
		//rebuild the addrinfo list
		if (outHost != nil)
		{
			if (pConfig != nil)
			{
				if ( pConfig->bIsSSL )
				{
					int ldapOptVal = LDAP_OPT_X_TLS_HARD;
					ldap_set_option(outHost, LDAP_OPT_X_TLS, &ldapOptVal);
				}
			}
			/* LDAPv3 only */
			version = LDAP_VERSION3;
			ldap_set_option( outHost, LDAP_OPT_PROTOCOL_VERSION, &version );
	
			// let's do a SASLbind if possible
			ldapReturnCode = doSASLBindAttemptIfPossible( outHost, pConfig, ldapAcct, ldapPasswd );
			
			// if we didn't have a local error or LDAP_OTHER, then we either failed or succeeded so no need to continue
			if( ldapReturnCode != LDAP_LOCAL_ERROR && ldapReturnCode != LDAP_OTHER )
			{
				// if we didn't have a success then we failed for some reason
				if( ldapReturnCode != LDAP_SUCCESS )
				{
					DBGLOG( kLogPlugin, "CLDAPNode::Failed doing SASL Authentication in Replica retrieval" );
					inLDAPNodeStruct->fConnectionStatus = kConnectionUnknown;
					inLDAPNodeStruct->fDelayedBindTime = time( nil ) + inLDAPNodeStruct->fDelayRebindTry;
					throw( (sInt32)eDSCannotAccessSession );
				}
			}
			else
			{
				//this is our and only our LDAP session for now
				//need to use our timeout so we don't hang indefinitely
				bindMsgId = ldap_bind( outHost, ldapAcct, ldapPasswd, LDAP_AUTH_SIMPLE );
				
				if (openTimeout == 0)
				{
					ldapReturnCode = ldap_result(outHost, bindMsgId, 0, NULL, &result);
				}
				else
				{
					struct	timeval	tv;
					tv.tv_sec		= openTimeout;
					tv.tv_usec		= 0;
					ldapReturnCode	= ldap_result(outHost, bindMsgId, 0, &tv, &result);
				}
				
				if ( ldapReturnCode == -1 )
				{
					throw( (sInt32)eDSCannotAccessSession );
				}
				else if ( ldapReturnCode == 0 )
				{
					// timed out, let's forget it
					ldap_abandon(outHost, bindMsgId);
					
					//log this timed out connection
					if ( (pConfig != nil) && (pConfig->fServerName != nil) )
					{
						LogFailedConnection("Bind timeout in Replica retrieval", pConfig->fServerName, inLDAPNodeStruct->fDelayRebindTry);
					}
					else
					{
						LogFailedConnection("Bind timeout in Replica retrieval", inLDAPNodeStruct->fServerName, inLDAPNodeStruct->fDelayRebindTry);
					}
					inLDAPNodeStruct->fConnectionStatus = kConnectionUnknown;
					inLDAPNodeStruct->fDelayedBindTime = time( nil ) + inLDAPNodeStruct->fDelayRebindTry;
					throw( (sInt32)eDSCannotAccessSession );
				}
				else if ( ldap_result2error(outHost, result, 1) != LDAP_SUCCESS )
				{
					//log this failed connection
					if ( (pConfig != nil) && (pConfig->fServerName != nil) )
					{
						LogFailedConnection("Bind failure in Replica retrieval", pConfig->fServerName, inLDAPNodeStruct->fDelayRebindTry);
					}
					else
					{
						LogFailedConnection("Bind failure in Replica retrieval", inLDAPNodeStruct->fServerName, inLDAPNodeStruct->fDelayRebindTry);
					}
					inLDAPNodeStruct->fConnectionStatus = kConnectionUnknown;
					inLDAPNodeStruct->fDelayedBindTime = time( nil ) + inLDAPNodeStruct->fDelayRebindTry;
					inLDAPNodeStruct->fHost = outHost;
					throw( (sInt32)eDSCannotAccessSession );
				}
			}
			
			//if we haven't bound above then we don't go looking for the replica info
			
			aRepList		= CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
			aWriteableList	= CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

			if ( (foundResult = GetReplicaListMessage(outHost, searchTimeout, aRepList, aWriteableList)) == eDSNoErr )
			{
				bMessageFound = true;
			}
			else if ( (foundResult = ExtractReplicaListMessage(outHost, searchTimeout, inLDAPNodeStruct, inConfigFromXML, aRepList, aWriteableList)) == eDSNoErr )
			{
				bMessageFound = true;
			}
			if (bMessageFound)
			{
				if (aRepList != NULL)
				{
					// if we have a new list, let's erase the old and put the new in...
					if (CFArrayGetCount(aRepList) > 0)
					{
						bool	bHadLoopback = false;
						
						if( inOutRepList )
						{
							// check if we had loopback...
							CFRange inRangeList = CFRangeMake(0,CFArrayGetCount(inOutRepList));
							if( CFArrayContainsValue(inOutRepList, inRangeList, CFSTR("127.0.0.1")) || CFArrayContainsValue(inOutRepList, inRangeList, CFSTR("localhost")) )
							{
								bHadLoopback = true;
							}

							CFArrayRemoveAllValues( inOutRepList );
							CFArrayAppendArray( inOutRepList, aRepList, CFRangeMake(0,CFArrayGetCount(aRepList)));
							CFRelease(aRepList);
							aRepList = NULL;
						}
						else
						{
							// if we didn't have one, let's just subtitute.
							inOutRepList = aRepList;
							aRepList = NULL;
						}

						// if we had loopback, let's put it back...
						if( bHadLoopback )
						{
							CFStringRef loopStr = CFStringCreateWithCString( NULL, "127.0.0.1", kCFStringEncodingUTF8 );
							CFArrayInsertValueAtIndex( inOutRepList, 0, loopStr );
							CFRelease( loopStr );
						}
						foundResult = eDSNoErr;
					}
					else
					{
						CFRelease(aRepList);
						aRepList = NULL;
					}
				}
				
				if (aWriteableList != NULL)
				{
					// if we have a new list, let's erase the old and put the new in...
					if (CFArrayGetCount(aWriteableList) > 0)
					{
						if( inOutWriteableList )
						{
							CFArrayRemoveAllValues( inOutWriteableList );
							CFArrayAppendArray(inOutWriteableList, aWriteableList, CFRangeMake(0,CFArrayGetCount(aWriteableList)));
							CFRelease(aWriteableList);
							aWriteableList = NULL;
						}
						else
						{
							inOutWriteableList = aWriteableList;
							aWriteableList = NULL;
						}
						foundResult = eDSNoErr;
					}
					else
					{
						CFRelease(aWriteableList);
						aWriteableList = NULL;
					}
				}
				
				CFStringRef replicaStrRef	= NULL;

				//use the known replicas to fill out the sReplicaInfo list
				//the writeable replicas are already contained in this list as well
				//so we need only one list of replica info structs
				CFIndex numReps = NULL;
				if (inOutRepList != NULL)
				{
					numReps = CFArrayGetCount(inOutRepList);
				}

				// let's make this easy... let's use the readable replicas, as there should be more of those than writable
				if ( numReps > 0)
				{
					CFRange rangeOfWrites = CFRangeMake( 0, (inOutWriteableList ? CFArrayGetCount(inOutWriteableList) : 0) );
					for (CFIndex indexToRep=0; indexToRep < numReps; indexToRep++ )
					{
						replicaStrRef = (CFStringRef)::CFArrayGetValueAtIndex( inOutRepList, indexToRep );
						struct addrinfo *addrList = ResolveHostName(replicaStrRef, inPort);
						sReplicaInfo* newInfo = (sReplicaInfo *)calloc(1, sizeof(sReplicaInfo));
						if (indexToRep == 0)
						{
							aList	= newInfo;
							oldList	= newInfo;
						}
						else
						{
							oldList->fNext = newInfo;
							oldList = oldList->fNext;
						}
						newInfo->fAddrInfo = addrList;
						newInfo->hostname = CFStringCreateCopy( kCFAllocatorDefault, replicaStrRef );
						if( inOutWriteableList != nil && CFArrayContainsValue(inOutWriteableList, rangeOfWrites, replicaStrRef) )
						{
							newInfo->bWriteable = true;
						}
					}
					
					
					if (aList != nil) //there is a new rep list that was built
					{
						FreeReplicaList( *inOutList );

						// now lets set the new one..
						*inOutList = aList;
					}
				}
			}

			//use the connection we just made if we didn't have one already
			//this way we don't need to connect again
			if (inLDAPNodeStruct->fHost == nil)
			{
				inLDAPNodeStruct->fHost = outHost;
			}
			else
			{
				ldap_unbind(outHost);
			}
		}
		else
		{
			foundResult = eDSCannotAccessSession;
		}
	} // try
	
	catch ( sInt32 err )
	{
		foundResult = err;
	}
	
	if( aRepList != NULL )
	{
		CFRelease( aRepList );
	}
	
	if( aWriteableList != NULL )
	{
		CFRelease( aWriteableList );
	}
	
	if (ldapAcct != nil)
	{
		delete (ldapAcct);
		ldapAcct = nil;
	}
	if (ldapPasswd != nil)
	{
		delete (ldapPasswd);
		ldapPasswd = nil;
	}
	
	return(foundResult);
}// RetrieveDefinedReplicas


//------------------------------------------------------------------------------------
//	* GetReplicaListMessage
//------------------------------------------------------------------------------------

sInt32 CLDAPNode::GetReplicaListMessage( LDAP *inHost, int inSearchTO, CFMutableArrayRef outRepList, CFMutableArrayRef outWriteableList )
{
	sInt32				siResult		= eDSRecordNotFound;
	bool				bResultFound	= false;
    int					ldapMsgId		= 0;
	LDAPMessage		   *result			= nil;
	int					ldapReturnCode	= 0;
	char			   *attrs[2]		= {"altserver",NULL};
	BerElement		   *ber;
	struct berval	  **bValues;
	char			   *pAttr			= nil;

	//search for the specific LDAP record altserver at the rootDSE which may contain
	//the list of LDAP replica urls
	
	// here is the call to the LDAP server asynchronously which requires
	// host handle, search base, search scope(LDAP_SCOPE_SUBTREE for all), search filter,
	// attribute list (NULL for all), return attrs values flag
	// Note: asynchronous call is made so that a MsgId can be used for future calls
	// This returns us the message ID which is used to query the server for the results
	if ( (ldapMsgId = ldap_search( inHost, "", LDAP_SCOPE_BASE, "(objectclass=*)", attrs, 0) ) == -1 )
	{
		bResultFound = false;
	}
	else
	{
		bResultFound = true;
		//retrieve the actual LDAP record data for use internally
		//useful only from the read-only perspective
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
		ldapReturnCode = ldap_result(inHost, ldapMsgId, 0, &tv, &result);
	}

	if (	(bResultFound) &&
			( ldapReturnCode == LDAP_RES_SEARCH_ENTRY ) )
	{
		//get the replica list here
		//parse the attributes in the result - should only be one ie. altserver
		pAttr = ldap_first_attribute (inHost, result, &ber );
		if (pAttr != nil)
		{
			if (( bValues = ldap_get_values_len (inHost, result, pAttr )) != NULL)
			{					
				// for each value of the attribute we need to parse and add as an entry to the outList
				for (int i = 0; bValues[i] != NULL; i++ )
				{
					if ( bValues[i] != NULL )
					{
						//need to strip off any leading characters since this should be an url format
						//ldap:// or ldaps://
						int offset = 0;
						char *strPtr = bValues[i]->bv_val;
						if (strlen(strPtr) >= 9) //don't bother trying to strip if string not even long enough to have a prefix
						{
							if (strncmp(strPtr,"ldaps://",8) == 0)
							{
								offset = 8;
							}
							if (strncmp(strPtr,"ldap://",7) == 0)
							{
								offset = 7;
							}
						}
						//try to stop at end of server name and don't include port number or search base
						char *strEnd = nil;
						strEnd = strchr(strPtr+offset,':');
						if (strEnd != nil)
						{
							strEnd[0] = '\0';
						}
						else
						{
							strEnd = strchr(strPtr+offset,'/');
							if (strEnd != nil)
							{
								strEnd[0] = '\0';
							}
						}
						CFStringRef aCFString = CFStringCreateWithCString( NULL, strPtr+offset, kCFStringEncodingMacRoman );
						CFArrayAppendValue(outRepList, aCFString);
						//TODO KW which of these are writeable?
						//CFArrayAppendValue(outRoutWriteableListepList, aCFString);
						CFRelease(aCFString);
						siResult = eDSNoErr;
					}
				}
				ldap_value_free_len(bValues);
			} // if bValues = ldap_get_values_len ...
			ldap_memfree( pAttr );
		} // if pAttr != nil
			
		if (ber != nil)
		{
			ber_free( ber, 0 );
		}
			
		ldap_msgfree( result );
		result = nil;

	} // if bResultFound and ldapReturnCode okay
	else if (ldapReturnCode == LDAP_TIMEOUT)
	{
		siResult = eDSServerTimeout;
		if ( result != nil )
		{
			ldap_msgfree( result );
			result = nil;
		}
	}
	else
	{
		siResult = eDSRecordNotFound;
		if ( result != nil )
		{
			ldap_msgfree( result );
			result = nil;
		}
	}
	
	return( siResult );

} // GetReplicaListMessage


//------------------------------------------------------------------------------------
//	* ExtractReplicaListMessage
//------------------------------------------------------------------------------------

sInt32 CLDAPNode::ExtractReplicaListMessage( LDAP *inHost, int inSearchTO, sLDAPNodeStruct *inLDAPNodeStruct, CLDAPv3Configs *inConfigFromXML, CFMutableArrayRef outRepList, CFMutableArrayRef outWriteableList )
{
	sInt32				siResult		= eDSRecordNotFound;
	bool				bResultFound	= false;
    int					ldapMsgId		= 0;
	LDAPMessage		   *result			= nil;
	int					ldapReturnCode	= 0;
	BerElement		   *ber;
	struct berval	  **bValues;
	char			   *pAttr			= nil;
	LDAPControl		  **serverctrls		= nil;
	LDAPControl		  **clientctrls		= nil;
	char			   *nativeRecType	= nil;
	bool				bOCANDGroup		= false;
	CFArrayRef			OCSearchList	= nil;
	ber_int_t			scope			= LDAP_SCOPE_BASE;
	char			   *queryFilter		= nil;
	char			   *repListAttr		= nil;
	char			   *writeListAttr	= nil;
	int					whichAttr		= 0;

	if (inLDAPNodeStruct == nil)
	{
		return(siResult);
	}

	//search for the specific LDAP config record which may contain
	//the list of both read and write LDAP replica urls
	
	nativeRecType = CLDAPv3Plugin::MapRecToLDAPType(	kDSStdRecordTypeConfig,
														inLDAPNodeStruct->fLDAPConfigTableIndex,
														1,
														&bOCANDGroup,
														&OCSearchList,
														&scope,
														inConfigFromXML );
	if (nativeRecType == nil)
	{
		return(eDSNoStdMappingAvailable);
	}
	
	queryFilter = CLDAPv3Plugin::BuildLDAPQueryFilter(	kDSNAttrRecordName,
														"ldapreplicas",
														eDSExact,
														inLDAPNodeStruct->fLDAPConfigTableIndex,
														false,
														kDSStdRecordTypeConfig,
														nativeRecType,
														bOCANDGroup,
														OCSearchList,
														inConfigFromXML );
	if (OCSearchList != nil)
	{
		CFRelease(OCSearchList);
		OCSearchList = nil;
	}
	if (queryFilter == nil)
	{
		if (nativeRecType != nil)
		{
			free(nativeRecType);
			nativeRecType = nil;
		}
		return(siResult);
	}
	
	// here is the call to the LDAP server asynchronously which requires
	// host handle, search base, search scope(LDAP_SCOPE_SUBTREE for all), search filter,
	// attribute list (NULL for all), return attrs values flag
	// Note: asynchronous call is made so that a MsgId can be used for future calls
	// This returns us the message ID which is used to query the server for the results

	ldapReturnCode = ldap_search_ext(	inHost,
										nativeRecType,
										scope,
										queryFilter,
										NULL,
										0,
										serverctrls,
										clientctrls,
										0, 0, 
										&ldapMsgId );
	if (ldapReturnCode == LDAP_SUCCESS)
	{
		bResultFound = true;
		//retrieve the actual LDAP record data for use internally
		//useful only from the read-only perspective
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
		ldapReturnCode = ldap_result(inHost, ldapMsgId, 0, &tv, &result);
	}

	if (nativeRecType != nil)
	{
		free(nativeRecType);
		nativeRecType = nil;
	}
	
	if (queryFilter != nil)
	{
		free(queryFilter);
		queryFilter = nil;
	}
	
	if (serverctrls)  ldap_controls_free( serverctrls );
	if (clientctrls)  ldap_controls_free( clientctrls );

	if (	(bResultFound) &&
			( ldapReturnCode == LDAP_RES_SEARCH_ENTRY ) )
	{
		siResult = eDSNoErr;
		//get the replica list here
		//parse the attributes in the result - should only be two
		repListAttr		= CLDAPv3Plugin::MapAttrToLDAPType(	kDSStdRecordTypeConfig,
															kDSNAttrLDAPReadReplicas,
															inLDAPNodeStruct->fLDAPConfigTableIndex,
															1,
															inConfigFromXML );
		writeListAttr	= CLDAPv3Plugin::MapAttrToLDAPType(	kDSStdRecordTypeConfig,
															kDSNAttrLDAPWriteReplicas,
															inLDAPNodeStruct->fLDAPConfigTableIndex,
															1,
															inConfigFromXML );

				
		for (	pAttr = ldap_first_attribute (inHost, result, &ber );
					pAttr != NULL; pAttr = ldap_next_attribute(inHost, result, ber ) )
		{
			whichAttr = 0;
			if ( ( repListAttr != nil ) && ( strcmp(pAttr, repListAttr) == 0 ) )
			{
				whichAttr = 1;
			}
			if ( ( writeListAttr != nil ) && ( strcmp(pAttr, writeListAttr) == 0 ) )
			{
				whichAttr = 2;
			}
			if ( ( whichAttr != 0 ) && (( bValues = ldap_get_values_len (inHost, result, pAttr )) != NULL) )
			{					
				// for each value of the attribute we need to parse and add as an entry to the outList
				for (int i = 0; bValues[i] != NULL; i++ )
				{
					if ( bValues[i] != NULL )
					{
						//need to strip off any leading characters since this should be an url format
						//ldap:// or ldaps://
						int offset = 0;
						char *strPtr = bValues[i]->bv_val;
						if (strlen(strPtr) >= 9) //don't bother trying to strip if string not even long enough to have a prefix
						{
							if (strncmp(strPtr,"ldaps://",8) == 0)
							{
								offset = 8;
							}
							if (strncmp(strPtr,"ldap://",7) == 0)
							{
								offset = 7;
							}
						}
						//try to stop at end of server name and don't include port number or search base
						char *strEnd = nil;
						strEnd = strchr(strPtr+offset,':');
						if (strEnd != nil)
						{
							strEnd[0] = '\0';
						}
						else
						{
							strEnd = strchr(strPtr+offset,'/');
							if (strEnd != nil)
							{
								strEnd[0] = '\0';
							}
						}
						if (whichAttr == 1)
						{
							CFStringRef aCFString = CFStringCreateWithCString( NULL, strPtr+offset, kCFStringEncodingMacRoman );
							CFArrayAppendValue(outRepList, aCFString);
							CFRelease(aCFString);
						}
						else
						{
							CFStringRef aCFString = CFStringCreateWithCString( NULL, strPtr+offset, kCFStringEncodingMacRoman );
							CFArrayAppendValue(outWriteableList, aCFString);
							CFRelease(aCFString);
						}
					}
				}
				ldap_value_free_len(bValues);
			} // if bValues = ldap_get_values_len ...
			ldap_memfree( pAttr );
		} // for ( loop over ldap_next_attribute )
			
		if (ber != nil)
		{
			ber_free( ber, 0 );
		}
			
		ldap_msgfree( result );
		result = nil;

	} // if bResultFound and ldapReturnCode okay
	else if (ldapReturnCode == LDAP_TIMEOUT)
	{
		siResult = eDSServerTimeout;
		if ( result != nil )
		{
			ldap_msgfree( result );
			result = nil;
		}
	}
	else
	{
		siResult = eDSRecordNotFound;
		if ( result != nil )
		{
			ldap_msgfree( result );
			result = nil;
		}
	}
	
	if ( repListAttr != nil )
	{
		free(repListAttr);
		repListAttr = nil;
	}
	if ( writeListAttr != nil )
	{
		free(writeListAttr);
		writeListAttr = nil;
	}

	return( siResult );

} // ExtractReplicaListMessage

// ---------------------------------------------------------------------------
//	* RetrieveServerMappingsIfRequired
// ---------------------------------------------------------------------------

void CLDAPNode::RetrieveServerMappingsIfRequired(sLDAPNodeStruct *inLDAPNodeStruct, CLDAPv3Configs *inConfigFromXML)
{
	sLDAPConfigData		   *pConfig			= nil;

	if (inLDAPNodeStruct != nil)
	{
		//check if we need to retrieve the server mappings
		pConfig = (sLDAPConfigData *)gLDAPConfigTable->GetItemData( inLDAPNodeStruct->fLDAPConfigTableIndex );
		if ( (inLDAPNodeStruct->fHost != nil) && (pConfig != nil) && (pConfig->bGetServerMappings) )
		{
			char **aMapSearchBase	= nil;
			uInt32	numberBases		= 0;
			//here we check if there is a provided mappings search base
			//otherwise we will attempt to retrieve possible candidates from the namingContexts of the LDAP server
			if ( (pConfig->fMapSearchBase == nil) || ( strcmp(pConfig->fMapSearchBase,"") == 0 ) )
			{
				//use the fOpenCloseTimeout here since we don't guarantee an explicit bind occurred at this point
				aMapSearchBase = GetNamingContexts( inLDAPNodeStruct->fHost, pConfig->fOpenCloseTimeout, &numberBases );
				if( aMapSearchBase == (char **) -1 )
				{
					aMapSearchBase = nil;
					//log this failed connection
					LogFailedConnection("GetNamingContexts failure", pConfig->fServerName, inLDAPNodeStruct->fDelayRebindTry);
					inLDAPNodeStruct->fConnectionStatus = kConnectionUnknown;
					inLDAPNodeStruct->fDelayedBindTime = time( nil ) + inLDAPNodeStruct->fDelayRebindTry;
				}
			}
			else
			{
				numberBases = 1;
				aMapSearchBase = (char **)calloc(numberBases+1, sizeof (char *));
				aMapSearchBase[0] = strdup(pConfig->fMapSearchBase);
			}
			
			for (uInt32 baseIndex = 0; (baseIndex < numberBases) && (aMapSearchBase[baseIndex] != nil); baseIndex++)
			{
				if (inConfigFromXML != nil)
				{
					if ( (inConfigFromXML->UpdateLDAPConfigWithServerMappings( pConfig->fServerName, aMapSearchBase[baseIndex], pConfig->fServerPort, pConfig->bIsSSL, pConfig->bUseAsDefaultLDAP, inLDAPNodeStruct->fHost )) == eDSNoErr )
					{
						pConfig->bGetServerMappings = false;
						break;
					}
					else
					{
						syslog(LOG_INFO,"CLDAPNode::SafeOpen Can't retrieve server mappings from search base of <%s>.", aMapSearchBase[baseIndex] );
					}
				}
				else
				{
					syslog(LOG_INFO,"CLDAPNode::SafeOpen CLDAPv3Configs pointer is nil so can't retrieve server mappings.");
					break;
				}
			}
			
			if (pConfig->bGetServerMappings == true)
			{
				syslog(LOG_INFO,"CLDAPNode::SafeOpen Cannot retrieve server mappings at this time.");
			}
			
			if (aMapSearchBase != nil)
			{
				for (uInt32 bIndex = 0; bIndex < numberBases; bIndex++)
				{
					if ( aMapSearchBase[bIndex] != nil )
					{
						free(aMapSearchBase[bIndex]);
						aMapSearchBase[bIndex] = nil;
					}
				}
				free(aMapSearchBase);
			}
		}
	}
} // RetrieveServerMappingsIfRequired

// ---------------------------------------------------------------------------
//	* FreeReplicaList
// ---------------------------------------------------------------------------

void CLDAPNode::FreeReplicaList( sReplicaInfo *inList )
{
	// now let's clean up
	while( inList != nil)
	{
		sReplicaInfo *nextItem = inList->fNext;
		freeaddrinfo(inList->fAddrInfo);
		if (inList->hostname != NULL)
		{
			CFRelease(inList->hostname);
		}
		free(inList);
		inList = nextItem;
	}
} // FreeReplicaList

// ---------------------------------------------------------------------------
//	* LDAPWithBlockingSocket
// ---------------------------------------------------------------------------

char *CLDAPNode::LDAPWithBlockingSocket( struct addrinfo *addrInfo, int seconds )
{
	int					aSock;
	int					val						= 1;
	int					len						= sizeof(val);
	struct timeval		recvTimeoutVal			= { seconds, 0 };
	char				*returnHostAddress		= NULL;

	aSock = socket( addrInfo->ai_family, addrInfo->ai_socktype, addrInfo->ai_protocol );
	if (aSock != -1)
	{
		setsockopt( aSock, SOL_SOCKET, SO_NOSIGPIPE, &val, len );
		setsockopt( aSock, SOL_SOCKET, SO_RCVTIMEO, &recvTimeoutVal, sizeof(recvTimeoutVal) );
		
		//block for recvTimeoutVal
		if( connect(aSock, addrInfo->ai_addr, addrInfo->ai_addrlen) == 0 )
		{
			returnHostAddress = ConvertToIPAddress( addrInfo );
		}
		close(aSock);
	}
	return returnHostAddress;
} //LDAPWithBlockingSocket

char *CLDAPNode::ConvertToIPAddress( struct addrinfo *addrInfo )
{
	//translate to the IP address
	char *returnHostAddress = NULL;
	
	if (addrInfo->ai_family == AF_INET)
	{
		returnHostAddress = (char *) calloc( 129, 1 );
		if( inet_ntop( AF_INET, (const void *)&(((struct sockaddr_in*)(addrInfo->ai_addr))->sin_addr), returnHostAddress, 129 ) == NULL )
		{
			free( returnHostAddress );
			returnHostAddress = NULL;
		}
	}
	else if (addrInfo->ai_family == AF_INET6)
	{
		returnHostAddress = (char *) calloc( 129, 1 );
		if( inet_ntop( AF_INET6, (const void *)&(((struct sockaddr_in6*)(addrInfo->ai_addr))->sin6_addr), returnHostAddress, 129 ) == NULL ) 
		{
			free( returnHostAddress );
			returnHostAddress = NULL;
		}
	}
	
	return returnHostAddress;
}

bool CLDAPNode::IsLocalAddress( struct addrinfo *addrInfo )
{
    struct ifaddrs *ifa_list = nil, *ifa = nil;
	bool			bReturn = false;
    
    if( getifaddrs(&ifa_list) != -1 )
	{
		for( ifa = ifa_list; ifa; ifa = ifa->ifa_next )
		{
			if( ifa->ifa_addr->sa_family == addrInfo->ai_addr->sa_family )
			{
				if( ifa->ifa_addr->sa_family == AF_INET )
				{
					struct sockaddr_in *interface = (struct sockaddr_in *)ifa->ifa_addr;
					struct sockaddr_in *check = (struct sockaddr_in *) addrInfo->ai_addr;
					
					if( interface->sin_addr.s_addr == check->sin_addr.s_addr )
					{
						bReturn = true;
						break;
					}
				}
				if( ifa->ifa_addr->sa_family == AF_INET6 )
				{
					struct sockaddr_in6 *interface = (struct sockaddr_in6 *)ifa->ifa_addr;
					struct sockaddr_in6 *check = (struct sockaddr_in6 *)addrInfo->ai_addr;
					
					if( memcmp( &interface->sin6_addr, &check->sin6_addr, sizeof(struct in6_addr) ) == 0 )
					{
						bReturn = true;
						break;
					}
				}
			}
		}
		freeifaddrs(ifa_list);
	}
	return bReturn;
}

bool CLDAPNode::ReachableAddress( struct addrinfo *addrInfo )
{
	bool	bReturn = IsLocalAddress( addrInfo );
	
	// if it wasn't local
	if( bReturn == false )
	{
		struct ifaddrs *ifa_list = nil, *ifa = nil;
		
		if( getifaddrs(&ifa_list) != -1 )
		{
			for( ifa = ifa_list; ifa; ifa = ifa->ifa_next )
			{
				// if either AF_INET or AF_INET6 then we have an address
				if( ifa->ifa_addr->sa_family == AF_INET || ifa->ifa_addr->sa_family == AF_INET6 )
				{
					if( strcmp( ifa->ifa_name, "lo0" ) != 0 )  // if we aren't using loopback only....
					{
						bReturn = true;
						break;
					}
				}
			}
			freeifaddrs(ifa_list);
		}
	}
	return bReturn;
}

void CLDAPNode::CheckSASLMethods( sLDAPNodeStruct *inLDAPNodeStruct, CLDAPv3Configs *inConfigFromXML )
{
	sLDAPConfigData		*pConfig	= nil;

	if (inLDAPNodeStruct != nil )
	{
		//check if we need to retrieve the server mappings
		pConfig = (sLDAPConfigData *)gLDAPConfigTable->GetItemData( inLDAPNodeStruct->fLDAPConfigTableIndex );
		
		if( pConfig != nil && pConfig->fSASLmethods == NULL )
		{
			LDAPMessage		   *result				= nil;
			int					ldapReturnCode		= 0;
			char			   *attrs[2]			= { "supportedSASLMechanisms",NULL };
			BerElement		   *ber					= NULL;
			struct berval	  **bValues				= NULL;
			char			   *pAttr				= nil;
			struct timeval		tv					= { 0, 0 };

			// we are in bootstrap, so let's don't try to get replicas, otherwise we will have a problem
			pConfig->bBuildReplicaList = false;
			
			LDAP *aHost = InitLDAPConnection( inLDAPNodeStruct, pConfig, inConfigFromXML, false );

			// now we can go look for replicas
			pConfig->bBuildReplicaList = true;

			DBGLOG( kLogPlugin, "CLDAPNode::Getting SASL Methods" );

			if( aHost )
			{
				if ( pConfig->bIsSSL )
				{
					int ldapOptVal = LDAP_OPT_X_TLS_HARD;
					ldap_set_option( aHost, LDAP_OPT_X_TLS, &ldapOptVal );
				}
				
				/* LDAPv3 only */
				int version = LDAP_VERSION3;
				ldap_set_option( aHost, LDAP_OPT_PROTOCOL_VERSION, &version );
				
				//this is our and only our LDAP session for now
				//need to use our timeout so we don't hang indefinitely
				int bindMsgId = ldap_simple_bind( aHost, NULL, NULL );
				
				tv.tv_sec		= (pConfig->fOpenCloseTimeout ? pConfig->fOpenCloseTimeout : kLDAPDefaultOpenCloseTimeoutInSeconds);
				ldapReturnCode	= ldap_result( aHost, bindMsgId, 0, &tv, &result );
				
				if( ldapReturnCode == 0 )
				{
					// timed out, let's forget it
					ldap_abandon( aHost, bindMsgId );
				}
				else if( ldap_result2error(aHost, result, 1) == LDAP_SUCCESS )
				{
					tv.tv_sec	= (pConfig->fSearchTimeout ? pConfig->fSearchTimeout : kLDAPDefaultOpenCloseTimeoutInSeconds);
					
					pConfig->fSASLmethods = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
					
					ldapReturnCode = ldap_search_ext_s(	aHost,
										 "",
										 LDAP_SCOPE_BASE,
										 "(objectclass=*)",
										 attrs,
										 false,
										 NULL,
										 NULL,
										 &tv, 0, 
										 &result );
					
					if (ldapReturnCode == LDAP_SUCCESS)
					{
						pAttr = ldap_first_attribute (aHost, result, &ber );
						if (pAttr != nil)
						{
							if( (bValues = ldap_get_values_len (aHost, result, pAttr)) != NULL )
							{
								uInt32 ii = 0;
								
								while( bValues[ii] != NULL )
								{
									CFStringRef value = CFStringCreateWithCString( NULL, bValues[ii]->bv_val, kCFStringEncodingUTF8 ); 
									CFArrayAppendValue( pConfig->fSASLmethods, value );
									CFRelease( value );
									ii++;
								}
								ldap_value_free_len( bValues );
							} // if bValues = ldap_get_values_len ...
							ldap_memfree( pAttr );
						} // if pAttr != nil
						
						if (ber != nil)
						{
							ber_free( ber, 0 );
						}
						
						ldap_msgfree( result );
						result = nil;
						
						DBGLOG( kLogPlugin, "CLDAPNode::Successful SASL Method retrieval" );
					}
				}
				ldap_unbind_ext( aHost, NULL, NULL );
				inLDAPNodeStruct->fHost = NULL;
			}
		}
	}
}

bool CLDAPNode::LocalServerIsLDAPReplica(  )
{
	bool bResult = false;
	char* fileContents = NULL;

	try
	{
		CFile slapdConf("/etc/openldap/slapd.conf");
		CFile slapdMacOSXConf;
		fileContents = (char*)calloc( 1, slapdConf.FileSize() + 1 );
		if ( fileContents != NULL )
		{
			slapdConf.Read( fileContents, slapdConf.FileSize() );
			if ((strncmp( fileContents, "updatedn", sizeof("updatedn") ) == 0)
				|| (strstr( fileContents, "\nupdatedn" ) != NULL))
			{
				bResult = true;
			}
			free( fileContents );
			fileContents = NULL;
		}
		if ( !bResult )
		{
			slapdMacOSXConf.open("/etc/openldap/slapd_macosxserver.conf");
			fileContents = (char*)calloc( 1, slapdMacOSXConf.FileSize() + 1 );
		}
		if (fileContents != NULL)
		{
			slapdMacOSXConf.Read( fileContents, slapdConf.FileSize() );
			if ((strncmp( fileContents, "updatedn", sizeof("updatedn") ) == 0)
				|| (strstr( fileContents, "\nupdatedn" ) != NULL))
			{
				bResult = true;
			}
			free( fileContents );
			fileContents = NULL;
		}
		
	}
	catch ( ... )
	{
	}
	if (fileContents != NULL)
	{
		free( fileContents );
	}
	
	return bResult;
}

// ---------------------------------------------------------------------------
//	* EnsureCheckFailedConnectionsThreadIsRunning
// ---------------------------------------------------------------------------

void CLDAPNode::EnsureCheckFailedConnectionsThreadIsRunning( void )
{
	if( fCheckThreadActive == false )
	{
		fCheckThreadActive = true;
				
		pthread_t       checkThread;
		pthread_attr_t	_DefaultAttrs;
				
		::pthread_attr_init( &_DefaultAttrs );
	
		::pthread_attr_setdetachstate( &_DefaultAttrs, PTHREAD_CREATE_DETACHED);
		pthread_create( &checkThread, &_DefaultAttrs, checkFailedServers, (void *)this );
	}
} // EnsureCheckFailedConnectionsThreadIsRunning

