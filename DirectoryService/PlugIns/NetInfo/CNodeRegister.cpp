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
 * @header CNodeRegister
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <sys/stat.h>
#include <syslog.h>		// for syslog() to log calls
#include <SystemConfiguration/SystemConfiguration.h>

#include <netinfo/ni.h>
#include <netinfo/ni_util.h>
#include <netinfo/ni_prot.h>

#include "CNodeRegister.h"
#include "CNiNodeList.h"
#include "CNetInfoPlugin.h"
#include "ServerModuleLib.h"
#include "CSharedData.h"
#include "PrivateTypes.h"
#include "CString.h"
#include "DSSemaphore.h"
#include "CLog.h"

#include "DSUtils.h"
#include "NiLib2.h"
#include "nibind_glue.h"
#include "netinfo_open.h"

// --------------------------------------------------------------------------------
// * Globals
// --------------------------------------------------------------------------------

static const sInt32		kCatchErr	= -80128;

extern char			   *gNIHierarchyTagString;
//--------------------------------------------------------------------------------------------------
// * CNodeRegister()
//
//--------------------------------------------------------------------------------------------------

CNodeRegister::CNodeRegister ( uInt32 inToken, CNiNodeList *inNodeList, bool inbReInit, CNetInfoPlugin *parentClass )
	: DSCThread( kTSNodeRegisterThread )
{
	fThreadSignature = kTSNodeRegisterThread;

	fToken		= inToken;
	fCount		= 0;
	fTotal		= 0;
	fNiNodeList	= inNodeList;
	bReInit		= inbReInit;
	bRestart	= true;
	fParentClass= parentClass;
	bNewNIHierarchy = false;

} // CNodeRegister



//--------------------------------------------------------------------------------------------------
// * ~CNodeRegister()
//
//--------------------------------------------------------------------------------------------------

CNodeRegister::~CNodeRegister()
{
} // ~CNodeRegister



//--------------------------------------------------------------------------------------------------
// * StartThread()
//
//--------------------------------------------------------------------------------------------------

void CNodeRegister::StartThread ( void )
{
	if ( this == nil ) throw((sInt32)eMemoryError);

	this->Resume();
} // StartThread



//--------------------------------------------------------------------------------------------------
// * StopThread()
//
//--------------------------------------------------------------------------------------------------

void CNodeRegister::StopThread ( void )
{
	if ( this == nil ) throw((sInt32)eMemoryError);

	SetThreadRunState( kThreadStop );		// Tell our thread to stop

} // StopThread


//--------------------------------------------------------------------------------------------------
// * ThreadMain()
//
//--------------------------------------------------------------------------------------------------

long CNodeRegister::ThreadMain ( void )
{
#ifdef DEBUG
	fMutex.Wait( 60 * kMilliSecsPerSec );
#endif
	DBGLOG( kLogPlugin, "Begin registering NetInfo nodes..." );

	nibind_registration *reg;
	ni_status			status				= NI_FAILED;
	unsigned			nreg;
	void				*nb;
	struct in_addr		addr;
	ni_name				dname				= NULL;
	void			   *domain				= nil;
	tDataList		   *pDataList			= nil;
	CString				tmpName( 254 );
	struct stat			statResult;
	bool				bSetLocallyHosted	= false;
	time_t				delayedNI			= 0;

	while (bRestart)
	{
		bRestart			= false;
		bSetLocallyHosted	= false;
		gNetInfoMutex->Wait();
	
		try
		{
			if (stat( "/var/run/nibindd.pid", &statResult ) == eDSNoErr)
			{
				addr.s_addr = htonl( INADDR_LOOPBACK );
		
				nb = nibind_new( &addr );
				if ( nb != NULL)
				{
					status = nibind_listreg( nb, &reg, &nreg );
					if ( status == NI_OK )
					{
						for ( unsigned i = 0; i < nreg; i++ )
						{
							if ((reg[i].tag) != NULL)
							{
								struct in_addr addr;
								addr.s_addr = htonl(INADDR_LOOPBACK);							
		
								status = (ni_status)netinfo_connect( &addr, reg[i].tag, &domain, 30 ); //30 sec timeout since this is on a separate thread
								if ( status == NI_OK )
								{
									delayedNI = time(nil) + 2; //normally netinfo_domainname will complete in under 2 secs
									dname = netinfo_domainname( domain );
									if ( delayedNI < time(nil) )
									{
										syslog(LOG_ALERT,"CNodeRegister:ThreadMain::Call to netinfo_domainname was with argument domain name from tag: %s and lasted %d seconds.", reg[i].tag, (uInt32)(2 + time(nil) - delayedNI));
										if (dname != nil)
										{
											syslog(LOG_ALERT,"CNodeRegister:ThreadMain::Call to netinfo_domainname returned domain name: %s.", dname);
										}
									}
									if ( dname != NULL )
									{
										// Register name
		
										tmpName.Set( "/NetInfo/root" );
										if ( ::strcmp( dname, "/" ) != 0 )
										{
											tmpName.Append( dname );
										}
		
										pDataList = ::dsBuildFromPathPriv( tmpName.GetData(), (char *)"/" );
										if ( pDataList != nil )
										{
											CServerPlugin::_RegisterNode( fToken, pDataList, kLocalHostedType );
											fTotal++;
											::dsDataListDeallocatePriv( pDataList );
											free( pDataList );
											pDataList = nil;
										}
		
										ni_name_free( &dname );
										dname = NULL;
									}
								} //if got a domain
							} // for over tags
						}
					}
					nibind_free(nb); //does this work?
				} // nb != NULL
			}
			else
			{
				//nibindd is not running so only local node is locally hosted
				bSetLocallyHosted = true;
			}
		}
	
		catch( sInt32 err )
		{
		}

		//cleanup if required
		fNiNodeList->CleanUpUnknownConnections(fToken);
		gNetInfoMutex->Signal();

		if (!bRestart)
		{
		//would like to maximize the priority of this thread here
		//since it registers the netinfo hierarchy likely used by the Search Policy
		int			myPolicy;
		sched_param	myStruct;
	
		if (pthread_getschedparam( pthread_self(), &myPolicy, &myStruct) == 0)
		{
			myStruct.sched_priority = sched_get_priority_max(myPolicy);
	
			if (pthread_setschedparam( pthread_self(), myPolicy, &myStruct) == 0)
			{
				DBGLOG( kLogPlugin, "Thread priority set to the maximum for registering NetInfo local hierarchy nodes." );
			}
		}
		}

		//register the local NetInfo hierarchy used by this machine
		RegisterLocalNetInfoHierarchy(bSetLocallyHosted);
	
		// now that we are done, let's notify the SearchPlugin or anyone else that we have updated based on the NetInfo hierarchy
		if( bNewNIHierarchy )
		{
			bNewNIHierarchy = false;
			CFStringRef service = CFStringCreateWithCString( NULL, "DirectoryService", kCFStringEncodingUTF8 );
			
			if ( service )
			{
				SCDynamicStoreRef   store = SCDynamicStoreCreate(NULL, service, NULL, NULL);
				
				if (store != NULL)
				{   // we don't have to change it we can just cause a notify....
					CFStringRef notify = CFStringCreateWithCString( NULL, "com.apple.DirectoryService.NotifyTypeStandard:NI_HIERARCHY_CHANGE", kCFStringEncodingUTF8 );
					
					if( notify ) {
						SCDynamicStoreNotifyValue( store, notify );
						CFRelease( notify );
					}
					CFRelease( store );
				}
				CFRelease( service );
			}
		}
		
	} //while bRestart

	if (fParentClass != nil)
	{
		fParentClass->NodeRegisterComplete(this);
	}
	
	return( 0 );

} // ThreadMain


// --------------------------------------------------------------------------------
//	* RegisterLocalNetInfoHierarchy ()
// --------------------------------------------------------------------------------

sInt32 CNodeRegister::RegisterLocalNetInfoHierarchy ( bool inSetLocallyHosted )
{
	sInt32		siResult		= eDSNoErr;
	void	   *domain			= nil;
	char	   *cpDomName		= nil;
	tDataList  *pDataList		= nil;
	tDataList  *qDataList		= nil;
	char	   *domName			= nil;
	uInt16		retryCount		= 0;
	DSSemaphore	timedWait;
	time_t		delayedNI		= 0;
	int			numLocalRetries	= 0;
	ni_status	niStatus		= NI_OK;

	try
	{
		if ( fNiNodeList == nil ) throw( (sInt32)ePlugInError );

		//at this point if we fail to open the local node, NetInfo must simply be restarting or ?
		//in any event we KNOW there will always be at least a local node
		//but that is not really the point here since we call netinfo_open simply to get the domain
		//and we expect that this should ALWAYS work since we are looking at "." directly
		//so for the netinfo_domainname call if IT were to fail then we need to go ahead and
		//possibly retry a few times and then give up by simply registering the local node as the root node

		if (bReInit) // wait 3 seconds for NetInfo to hopefully give correct answer after network transition
		{
			fMutex.Wait( 3 * kMilliSecsPerSec );
		}
		
		gNetInfoMutex->Wait();
		do
		{
			//Need to retry netinfo_open since this is a boot issue and it needs to succeed
			//however, there may be a initialization problem with netinfod and RPC that makes it fail first few times
			//eventually there will be code added above that can handle the notifications from netinfod to detect it is running
			numLocalRetries = 0;
			do
			{
				// Open the local node to get the domain
				niStatus = ::netinfo_open( nil, ".", &domain, 10 );
				if (niStatus != NI_OK)
				{
					if (numLocalRetries == 0)
					{
						syslog(LOG_ALERT,"RegisterLocalNetInfoHierarchy::netinfod likely not yet initialized - netinfo_open error is %d.", niStatus);
					}
					fMutex.Wait( 2 * kMilliSecsPerSec );
					numLocalRetries++;
				}

				if (numLocalRetries >= 60) //60 * 2sec = 2 minutes but expect netinfod to start much quicker
				{
					syslog(LOG_ALERT,"RegisterLocalNetInfoHierarchy::netinfod not reachable for the local domain after two minutes of trying.");
					//not clear how to recover from this right now but a later network transition may obviously help
					break;
				}

			} while ( (niStatus != NI_OK) && !(bReInit) );
			
			if (niStatus == NI_OK)
			{
				// Get local domain name full name
				delayedNI = time(nil) + 2; //normally netinfo_domainname will complete in under 2 secs
				cpDomName = netinfo_domainname( domain );
				if ( delayedNI < time(nil) )
				{
					syslog(LOG_ALERT,"CNodeRegister:RegisterLocalNetInfoHierarchy::Call to netinfo_domainname for local domain name lasted %d seconds.", (uInt32)(2 + time(nil) - delayedNI));
					if (cpDomName != nil)
					{
						syslog(LOG_ALERT,"CNodeRegister:RegisterLocalNetInfoHierarchy::Call to netinfo_domainname returned domain name: %s.", cpDomName);
					}
				}
				
				if ( cpDomName == nil )
				{
					DBGLOG1( kLogPlugin, "RegisterLocalNetInfoHierarchy: netinfo_domainname call # %u failed", retryCount+1 );
					//2 second wait here
					timedWait.Wait( 2 * kMilliSecsPerSec );

					retryCount++;
				}				
			}
		} while ( (niStatus != NI_OK) && (cpDomName != nil) && (retryCount < 3) ); // only retry three times
		gNetInfoMutex->Signal();

		uInt32 localOrParentCount = 1;
		if ( cpDomName != nil )
		{
			if (gNIHierarchyTagString == nil)
			{
				gNIHierarchyTagString = strdup(cpDomName);
				bNewNIHierarchy = true;
			}
			else
			{
				if (strcmp(gNIHierarchyTagString, cpDomName) != 0)
				{
					//unregister what was registered for this string since we will register new below
					{
						char *regNodes = strdup(gNIHierarchyTagString);
						do
						{
							if (domName != nil)
							{
								free(domName);
								domName = nil;
							}
							domName = (char *)calloc(1,strlen(kstrRootNodeName)+strlen(regNodes)+1);
							strcpy(domName,kstrRootNodeName);
							strcat(domName,regNodes);
							
							// Build a path with our node name
							qDataList = ::dsBuildFromPathPriv( domName, "/" );
							if ( qDataList == nil ) throw( (sInt32)eMemoryAllocError );

							DBGLOG1( kLogPlugin, "Unregistering node %s.", domName );

							if ( CNetInfoPlugin::UnregisterNode( fToken, qDataList ) == eDSNoErr )
							{
								fTotal--;
							}
							::dsDataListDeallocatePriv( qDataList );
							free(qDataList);
							qDataList = nil;
							
							if (strcmp(domName, kstrRootNodeName) != 0)
							{
								char *lastDel = strrchr(regNodes,'/');
								*lastDel = '\0';
							}
							
						} while ( strcmp(domName, kstrRootNodeName) != 0 );
						
						if (domName != nil)
						{
							free(domName);
							domName = nil;
						}
						free(regNodes);
					}
					bNewNIHierarchy = true;
					free(gNIHierarchyTagString);
					gNIHierarchyTagString = strdup(cpDomName);
				}
			}
			if ( bNewNIHierarchy && (::strcmp( cpDomName, "/" ) != 0 ) )
			{
				char	   *p = cpDomName + ::strlen( cpDomName );
				do
				{
					if (domName != nil)
					{
						free(domName);
						domName = nil;
					}
					domName = (char *)calloc(1,strlen(kstrRootNodeName)+strlen(cpDomName)+1);
					strcpy(domName,kstrRootNodeName);
					strcat(domName,cpDomName);
					
					// Build a path with our node name
					pDataList = ::dsBuildFromPathPriv( domName, "/" );
					qDataList = ::dsBuildFromPathPriv( domName, "/" );
					if ( pDataList == nil ) throw( (sInt32)eMemoryAllocError );
					if ( qDataList == nil ) throw( (sInt32)eMemoryAllocError );

					DBGLOG1( kLogPlugin, "Registering node %s.", domName );

					// Register the node
					// first node parsed out is the local node but will be registered same as others
					// local node registered as "." elsewhere already
				
					// AddNode consumes the tDataList, but not the char *
					//add the node regardless whether it has been registered or not
					//ie. if already in the node list then the pDataList will be replaced
					//if (localOrParentCount < 3)
					//{
						fNiNodeList->AddNode( cpDomName, pDataList, true, localOrParentCount );
					//}
					//else
					//{
						//fNiNodeList->AddNode( cpDomName, pDataList, true, 0 );
					//}
					//pDataList is consumed above by AddNode
					pDataList = nil;
					if ( CServerPlugin::_RegisterNode( fToken, qDataList, kDirNodeType ) == eDSNoErr )
					{
						fTotal++;
					}
					::dsDataListDeallocatePriv( qDataList );
					free(qDataList);
					qDataList = nil;
					
					//if this is the lowest node then it is also the locally hosted node
					if (inSetLocallyHosted)
					{
						tDataList  *aDataList = nil;
						inSetLocallyHosted = false;
						aDataList = ::dsBuildFromPathPriv( kstrDefaultLocalNodeName, "/" );
						if (aDataList != nil)
						{
							CServerPlugin::_RegisterNode( fToken, aDataList, kLocalHostedType );
							::dsDataListDeallocatePriv( aDataList );
							free( aDataList );
							aDataList = nil;
						}
					}
					
					while ( (p != cpDomName) && (*p != '/') )
					{
						p--;
					}
					if ( *p == '/' )
					{
						*p = '\0';
					}
					
					localOrParentCount++;
				} while ( p != cpDomName );
				
				if (domName != nil)
				{
					free(domName);
					domName = nil;
				}

			} // (::strcmp( cpDomName, "/" ) != 0 )
			
			free( cpDomName );
			cpDomName = nil;
		} // ( cpDomName != nil )

		if (bNewNIHierarchy)
		{
			if (localOrParentCount == 1) //this means we have ONLY a local node
			{
				// always register the local node by itself
				pDataList = ::dsBuildFromPathPriv( kstrDefaultLocalNodeName, "/" );
				qDataList = ::dsBuildFromPathPriv( kstrDefaultLocalNodeName, "/" );
				if ( ( pDataList != nil ) && ( qDataList != nil ) )
				{
					DBGLOG1( kLogPlugin, "Registering node %s.", kstrDefaultLocalNodeName );
		
					// AddNode consumes the tDataList, but not the char *
					fNiNodeList->AddNode( ".", pDataList, true, localOrParentCount );
					pDataList = nil;
					if ( CServerPlugin::_RegisterNode( fToken, qDataList, kDirNodeType ) == eDSNoErr )
					{
						fTotal++;
					}
					//this is the lowest node so it is also the locally hosted node
					//no real need to check inSetLocallyHosted
					if (inSetLocallyHosted)
					{
						CServerPlugin::_RegisterNode( fToken, qDataList, kLocalHostedType );
					}
					::dsDataListDeallocatePriv( qDataList );
					free(qDataList);
					qDataList = nil;
				}
			}
			else
			{
				// always register "root" node by itself
				pDataList = ::dsBuildFromPathPriv( kstrRootNodeName, "/" );
				qDataList = ::dsBuildFromPathPriv( kstrRootNodeName, "/" );
				if ( ( pDataList != nil ) && ( qDataList != nil ) )
				{
					DBGLOG1( kLogPlugin, "Registering node %s.", kstrRootNodeName );
		
					// AddNode consumes the tDataList, but not the char *
					fNiNodeList->AddNode( "/", pDataList, true, localOrParentCount );
					pDataList = nil;
					if ( CServerPlugin::_RegisterNode( fToken, qDataList, kDirNodeType ) == eDSNoErr )
					{
						fTotal++;
					}
					::dsDataListDeallocatePriv( qDataList );
					free(qDataList);
					qDataList = nil;
				}
			}
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // RegisterLocalNetInfoHierarchy


// ---------------------------------------------------------------------------
//	* IsValidNameList ()
//
// ---------------------------------------------------------------------------

bool CNodeRegister::IsValidNameList ( ni_namelist *inNameList )
{
	bool		result	= true;

	if ( inNameList == nil )
	{
		result = false;
	}

	if ( result != false )
	{
		if ( inNameList->ni_namelist_len == 0 )
		{
			result = false;
		}
	}

	return( result );

} // IsValidNameList


// ---------------------------------------------------------------------------
//	* IsValidName ()
//
// ---------------------------------------------------------------------------

bool CNodeRegister::IsValidName ( char *inName )
{
	bool		result	= true;
	char		*p		= inName;

	if ( ::strncmp( p, "./", 2 ) == 0 )
  	{
		result = false;
	}

	if ( ::strncmp( p, "../", 3 ) == 0 )
	{
		result = false;
	}

	return( result );

} // IsValidName


// ---------------------------------------------------------------------------
//	* IsLocalDomain ()
//
// ---------------------------------------------------------------------------

bool CNodeRegister::IsLocalDomain ( char *inName )
{
	bool		result	= false;
	char		*p		= inName;

	if ( ::strstr( p, "/local" ) != nil )
	{
		result = true;
	}

	return( result );

} // IsLocalDomain

// ---------------------------------------------------------------------------
//	* Restart ()
//
// ---------------------------------------------------------------------------

void CNodeRegister::Restart ( void )
{
	bRestart	= true;
	bReInit		= true;
} // Restart

