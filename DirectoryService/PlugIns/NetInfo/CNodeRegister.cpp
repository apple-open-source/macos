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
#include "my_ni_pwdomain.h"

// --------------------------------------------------------------------------------
// * Globals
// --------------------------------------------------------------------------------

static const sInt32		kCatchErr	= -80128;

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

// here we need to check if netinfod is even running before we continue
// this has become an issue since we are now started with mach_init
// TODO KW

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
							tmpName.Set( "127.0.0.1" );							
							tmpName.Append( "/" );							
							tmpName.Append( reg[i].tag );							
	
							status = (ni_status)do_open( nil, tmpName.GetData(), &domain, true, 30, NULL, NULL );
							if ( status == NI_OK )
							{
								delayedNI = time(nil) + 2; //normally my_ni_pwdomain will complete in under 2 secs
								status = ::my_ni_pwdomain( domain, &dname );
								if ( delayedNI < time(nil) )
								{
									syslog(LOG_INFO,"CNodeRegister:ThreadMain::Call to my_ni_pwdomain was with argument domain name: %s and lasted %d seconds.", tmpName.GetData(), (uInt32)(2 + time(nil) - delayedNI));
									if (dname != nil)
									{
										syslog(LOG_INFO,"CNodeRegister:ThreadMain::Call to my_ni_pwdomain returned domain name: %s.", dname);
									}
								}
								if ( status == NI_OK )
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
							if (domain != nil)
							{
								ni_free(domain);
								domain = nil;
							}
						} // for over tags
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

			//register the local NetInfo hierarchy used by this machine
			RegisterLocalNetInfoHierarchy(bSetLocallyHosted);
		} //if !bRestart
	
		if ( (!bRestart) && (bReInit) ) //need this here to clean out the top of the hierarchy
		{
			//call to unregister all the still dirty nodes
			fNiNodeList->CleanAllDirty(fToken);
		}
		
		if (!bRestart)
		{
			//For desktop Mac OS X we don't register the world but we do on Server
			//if ever made a static plugin use gServerOS here
			if (stat( "/System/Library/CoreServices/ServerVersion.plist", &statResult ) == eDSNoErr)
			{
				//would like to minimize the priority of this thread here down to the minimum now
				//since this is now registering the rest of the world in the NetInfo hierarchy
				int			myPolicy;
				sched_param	myStruct;
			
				if (pthread_getschedparam( pthread_self(), &myPolicy, &myStruct) == 0)
				{
					myStruct.sched_priority = sched_get_priority_min(myPolicy);
			
					if (pthread_setschedparam( pthread_self(), myPolicy, &myStruct) == 0)
					{
						DBGLOG( kLogPlugin, "Thread priority set to the minimum for registering NetInfo non-local hierarchy nodes." );
					}
				}
				
				if ( !(bReInit) ) //don't wait if not at initial startup
				{
					fMutex.Wait( 30 * kMilliSecsPerSec );
				}
				//register all the other nodes that this local NetInfo hierarchy serves
				RegisterNodes( (char *)"/" );
			}
			DBGLOG( kLogPlugin, "Finished register NetInfo nodes." );
			DBGLOG1( kLogPlugin, "NetInfo nodes registered = %l.", fTotal );
		} //if !bRestart
	
		if (bRestart) //KW allow the network transition to occur ie. 2 secs?
		{
			fMutex.Wait( 2 * kMilliSecsPerSec );
			fNiNodeList->SetAllDirty();
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
	sInt32		timeOutSecs		= 3;		//is this sufficient time for NetInfo?
	void	   *domain			= nil;
	char	   *domainName		= nil;
	char	   *cpDomName		= nil;
	tDataList  *pDataList		= nil;
	tDataList  *qDataList		= nil;
	char	   *domName			= nil;
	ni_id		niRootDir;
	uInt16		retryCount		= 0;
	DSSemaphore	timedWait;
	time_t		delayedNI		= 0;
	int			numLocalRetries	= 0;

	try
	{
		if ( fNiNodeList == nil ) throw( (sInt32)ePlugInError );

		//at this point if we fail to open the local node, NetInfo must simply be restarting or ?
		//in any event we KNOW there will always be at least a local node
		//but that is not really the point here since we call SafeOpen simply to get the domain
		//and we expect that this should ALWAYS work since we are looking at "." directly
		//so for the my_ni_pwdomain call if IT were to fail then we need to go ahead and
		//possibly retry a few times and then give up by simply registering the local node as the root node

		if (bReInit) // wait 3 seconds for NetInfo to hopefully give correct answer after network transition
		{
			fMutex.Wait( 3 * kMilliSecsPerSec );
		}
		
		do
		{
			//Need to retry SafeOpen since this is a boot issue and it needs to succeed
			//however, there may be a initialization problem with netinfod and RPC that makes it fail first few times
			//eventually there will be code added above that can handle the notifications from netinfod to detect it is running
			numLocalRetries = 0;
			do
			{
				// Open the local node to get the domain
				siResult = CNetInfoPlugin::SafeOpen( ".", timeOutSecs, &niRootDir, &domain, &domainName );
				if (siResult == eDSOpenNodeFailed)
				{
					if (numLocalRetries == 0)
					{
						syslog(LOG_INFO,"RegisterLocalNetInfoHierarchy::netinfod likely not yet initialized - Error is %d.", siResult);
					}
					fMutex.Wait( 2 * kMilliSecsPerSec );
					numLocalRetries++;
				}

				if (numLocalRetries >= 60) //60 * 2sec = 2 minutes but expect netinfod to start much quicker
				{
					syslog(LOG_INFO,"RegisterLocalNetInfoHierarchy::netinfod not reachable for the local domain after two minutes of trying.");
					//not clear how to recover from this right now but a later network transition may obviously help
					break;
				}

			} while ( (siResult != eDSNoErr) && !(bReInit) );
			
			if (domainName != nil) free(domainName);

			if (siResult == eDSNoErr)
			{
				gNetInfoMutex->Wait();
				// Get local domain name full name
				delayedNI = time(nil) + 2; //normally my_ni_pwdomain will complete in under 2 secs
				siResult = ::my_ni_pwdomain( domain, &cpDomName );
				if ( delayedNI < time(nil) )
				{
					syslog(LOG_INFO,"CNodeRegister:RegisterLocalNetInfoHierarchy::Call to my_ni_pwdomain for local domain name lasted %d seconds.", (uInt32)(2 + time(nil) - delayedNI));
					if (cpDomName != nil)
					{
						syslog(LOG_INFO,"CNodeRegister:RegisterLocalNetInfoHierarchy::Call to my_ni_pwdomain returned domain name: %s.", cpDomName);
					}
				}
				gNetInfoMutex->Signal();
				
				if ( (siResult != eDSNoErr) || (cpDomName == nil) )
				{
					DBGLOG1( kLogPlugin, "RegisterLocalNetInfoHierarchy: my_ni_pwdomain call # %u failed", retryCount+1 );
					//2 second wait here
					timedWait.Wait( 2 * kMilliSecsPerSec );

					retryCount++;
				}
				
				//close out "." domain if able to open it in order to drop the refCount
				siResult = CNetInfoPlugin::SafeClose( "." );
			}
		} while ( (siResult != eDSNoErr) && (retryCount < 3) ); // only retry three times

		if ( cpDomName != nil )
		{
			if (::strcmp( cpDomName, "/" ) != 0 )
			{
				char	   *p = cpDomName + ::strlen( cpDomName );
				uInt32 localOrParentCount = 1;
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
					if (localOrParentCount < 3)
					{
						fNiNodeList->AddNode( cpDomName, pDataList, true, localOrParentCount );
					}
					else
					{
						fNiNodeList->AddNode( cpDomName, pDataList, true, 0 );
					}
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

		// always register "root" node by itself
		pDataList = ::dsBuildFromPathPriv( kstrRootNodeName, "/" );
		qDataList = ::dsBuildFromPathPriv( kstrRootNodeName, "/" );
		if ( ( pDataList != nil ) && ( qDataList != nil ) )
		{
			DBGLOG1( kLogPlugin, "Registering node %s.", kstrRootNodeName );

			// AddNode consumes the tDataList, but not the char *
			fNiNodeList->AddNode( "/", pDataList, true );
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
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // RegisterLocalNetInfoHierarchy


//------------------------------------------------------------------------------------
//	* RegisterNodes ()
//------------------------------------------------------------------------------------

sInt32 CNodeRegister::RegisterNodes ( char *inDomainName )
{
	sInt32			siResult		= 0;
	sInt32			entryListCnt	= 0;
	sInt32			entryListLen	= 0;
	sInt32			nameListCnt		= 0;
	sInt32			nameListlen		= 0;
	sInt32			timeOutSecs		= 3;
	ni_status		niStatus		= NI_OK;
	bool			isLocal			= false;
	bool			bIsPresent		= false;
	void		   *domain			= nil;
	tDataList	   *pDataList		= nil;
	char			*name			= nil;
	char			*unused			= nil;
	char			*p  			= nil;
	ni_namelist	   *nameList		= nil;
	ni_id			machines;
	ni_id			niDirID;
	ni_entrylist	niEntryList;
	char		   *domName			= nil;
	char		   *tempName		= nil;
	bool			bNodeServes		= false;
	ni_proplist		niPropList;
	uInt32			pv				= 0;
	ni_index		niIndex			= 0;

	if (bRestart)
	{
		return(eDSOperationFailed);
	}


	try
	{
		if ( fNiNodeList == nil ) throw( (sInt32)ePlugInError );

		DBGLOG1( kLogPlugin, "Registering nodes in domain = %s", inDomainName );
		
		//no way can we use this member var properly in a recursive function but we try
		fCount = 0;

		domain = nil;
		NI_INIT( &niDirID );
		siResult = CNetInfoPlugin::SafeOpen( inDomainName, timeOutSecs, &niDirID, &domain, &unused );
		if (unused != nil) free(unused);
		//don't free domain since node list owns it if the SafeOpen succeeded
		//However, let's call SafeClose below since likely these registered nodes will NEVER be used
		//in any event we ref count
		//and a call to OpenDirNode will simply lead to another SafeOpen if not already opened
		if ( siResult != eDSNoErr ) throw( siResult );

		fMutex.Wait( 1 * kMilliSecsPerSec );

		gNetInfoMutex->Wait();

		try
		{
			NI_INIT( &machines );
			siResult = ::ni_pathsearch( domain, &machines, "/machines" );
			if ( siResult != eDSNoErr )
			{
//				::ni_free( domain ); //no free here since SafeOpen owns the domain ptr
				domain = nil;
				throw( siResult );
			}

			NI_INIT( &niEntryList );
			niStatus = ::ni_list( domain, &machines, "serves", &niEntryList );
			if ( niStatus != NI_OK )
			{
//				::ni_free( domain ); //no free here since SafeOpen owns the domain ptr
				domain = nil;
				throw( (sInt32)niStatus );
			}
		}

		catch( sInt32 err )
		{
			niStatus = (ni_status)err;
		}

		catch ( ... )
		{
			niStatus = (ni_status)kCatchErr;
		}

		gNetInfoMutex->Signal();

		if ( niStatus != 0 ) throw( (sInt32)niStatus );

		entryListLen = niEntryList.ni_entrylist_len;

		for ( entryListCnt = 0; entryListCnt < entryListLen; entryListCnt++ )
		{
			if (bRestart) //don't want anymore examined here
			{
				break;
			}

			//check here for machines that serve themselves
			NI_INIT( &niPropList );
			bNodeServes = false;
			machines.nii_object = niEntryList.ni_entrylist_val[ entryListCnt ].id;

			gNetInfoMutex->Wait();

			siResult = ::ni_read( domain, &machines, &niPropList );
			//eliminate nodes that ONLY serve themselves
			if ( siResult == NI_OK )
			{
				niIndex = ::ni_proplist_match( niPropList, "serves", NULL );
				if ( niIndex != NI_INDEX_NULL )
				{
					// For each value in the namelist for this property
					for ( pv = 0; pv < niPropList.nipl_val[ niIndex ].nip_val.ni_namelist_len; pv++ )
					{
						if (nil == strstr(niPropList.nipl_val[ niIndex ].nip_val.ni_namelist_val[ pv ],"/local"))
						{
							bNodeServes = true;
							break;
						}
					}
				}
				::ni_proplist_free( &niPropList );
			}

			gNetInfoMutex->Signal();
			
			if (!bNodeServes)
			{
				continue;
			}

			nameList = niEntryList.ni_entrylist_val[ entryListCnt ].names;
			if ( IsValidNameList( nameList ) == false )
			{
				continue;
			}

			nameListlen = nameList->ni_namelist_len;
			for ( nameListCnt = 0; nameListCnt < nameListlen; nameListCnt++ )
			{
				if (bRestart) //don't want anymore added here
				{
					break;
				}

				name = nameList->ni_namelist_val[ nameListCnt ];
				if ( this->IsValidName( name ) == false )
				{
					continue;
				}

				isLocal = this->IsLocalDomain( name );

				p = ::strchr( name, '/' );
				if ( p == nil )
				{
					continue;
				}
				*p = '\0';
				
				if (domName != nil)
				{
					free(domName);
					domName = nil;
				}
				domName = (char *)calloc(1,strlen(inDomainName)+1+strlen(name)+1);
				strcpy(domName,inDomainName);
				if (strlen(domName) > 1)
				{
					strcat(domName,"/");
				}
				strcat(domName,name);
				
				pDataList = nil;

				bIsPresent = fNiNodeList->IsPresent( domName );
				if ( bIsPresent == false )
				{
					if (tempName != nil)
					{
						free(tempName);
						tempName = nil;
					}
					tempName = (char *)calloc(1,strlen(kstrRootNodeName)+strlen(domName)+1);
					strcpy(tempName,kstrRootNodeName);
					strcat(tempName,domName);
					pDataList = dsBuildFromPathPriv( tempName, (char *)"/" );
					if ( pDataList != nil )
					{
						if (isLocal == false)
						{
							if ( CServerPlugin::_RegisterNode( fToken, pDataList, kDirNodeType ) == eDSNoErr )
							{
								// AddNode consumes the tDataList, but not the char *
								fNiNodeList->AddNode( domName, pDataList, true ); //registered node
								pDataList = nil;
								fCount++;
								fTotal++;
							}
							else
							{
								::dsDataListDeallocatePriv( pDataList );
								free( pDataList );
								pDataList = nil;
							}
							
							DBGLOG2( kLogPlugin, "Nodes registered for domain %s = %l.", domName, fCount );

							siResult = this->RegisterNodes( domName );
							if ( siResult != 0 )
							{
								break;
							}
						}
						else
						{
							::dsDataListDeallocatePriv( pDataList );
							free( pDataList );
							pDataList = nil;
						}
					}
				}
			}
			if (tempName != nil)
			{
				free(tempName);
				tempName = nil;
			}
			if (domName != nil)
			{
				free(domName);
				domName = nil;
			}
			//::ni_namelist_free( &nameList ); // not needed since ptr and niEntryList freed below
		}
		
		gNetInfoMutex->Wait();
		
		//close out domain since we can easily reopen during a dsOpenDirNode
		if (inDomainName != nil)
		{
			siResult = CNetInfoPlugin::SafeClose( inDomainName );
		}

		::ni_entrylist_free(&niEntryList);
		
		gNetInfoMutex->Signal();

		fMutex.Wait( 5 * kMilliSecsPerSec );

	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	catch ( ... )
	{
		siResult = kCatchErr;
	}
	
	return( siResult );

} // RegisterNodes


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

