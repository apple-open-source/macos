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
 * @header CNiNodeList
 */

#include "CNiNodeList.h"
#include "CNetInfoPlugin.h"
#include "DSUtils.h"
#include "ServerModuleLib.h"
#include "CSharedData.h"
#include "CLog.h"
#include "CNiUtilities.h"
#include "netinfo_open.h"


// ---------------------------------------------------------------------------
//	CNiNodeList ()
// ---------------------------------------------------------------------------

CNiNodeList::CNiNodeList ( void )
{

} // CNiNodeList


// ---------------------------------------------------------------------------
//	~CNiNodeList ()
// ---------------------------------------------------------------------------

CNiNodeList::~CNiNodeList ( void )
{
	sNode	   *aNode		= nil;
	NiNodeMapI	aNiNodeMapI;
	
	for (aNiNodeMapI = fNiNodeMap.begin(); aNiNodeMapI != fNiNodeMap.end(); ++aNiNodeMapI)
	{
		aNode = aNiNodeMapI->second;
		if (aNode->listPtr != nil)
		{
			::dsDataListDeallocatePriv( aNode->listPtr );
			//need to free the header as well
			free( aNode->listPtr );
			aNode->listPtr = nil;
		}
		if (aNode->fDomain != nil)
		{
			//TODO KW what to do here?
			aNode->fDomain = nil;
		}
		if (aNode->fDomainName != nil)
		{
			free(aNode->fDomainName);
			aNode->fDomainName = nil;
		}
		free(aNode);
	}
	fNiNodeMap.clear();
} // ~CNiNodeList


// ---------------------------------------------------------------------------
//	Lock ()
// ---------------------------------------------------------------------------

void CNiNodeList::Lock ( void )
{
	fMutex.Wait();
} // Lock


// ---------------------------------------------------------------------------
//	UnLock ()
// ---------------------------------------------------------------------------

void CNiNodeList::UnLock ( void )
{
	fMutex.Signal();
} // UnLock


// ---------------------------------------------------------------------------
//	AddNode ()
// ---------------------------------------------------------------------------

sInt32 CNiNodeList::AddNode ( const char *inStr, tDataList *inListPtr, bool inRegistered, uInt32 inLocalOrParent )
{
	string		aNodeName(inStr);
	sNode	   *aNode		= nil;
	NiNodeMapI	aNiNodeMapI;

	fMutex.Wait();
	
	aNiNodeMapI	= fNiNodeMap.find(aNodeName);
	if (aNiNodeMapI == fNiNodeMap.end()) //not already there
	{
		// Setup the new node
		aNode = (sNode *)::calloc( sizeof( sNode ), sizeof( char ) );

		aNode->listPtr		= inListPtr;
		aNode->refCount		= 1; 
		aNode->bisDirty		= false;
		aNode->fDomain		= nil;
		aNode->fDomainName	= nil;
		aNode->bRegistered	= inRegistered;
		aNode->localOrParent= inLocalOrParent;
		
		fNiNodeMap[aNodeName] = aNode;
		
		DBGLOG1( kLogPlugin, "CNiNodeList::AddNode - added the nodename %s", inStr);

	}
	else //we have a duplicate
	{
		DBGLOG1( kLogPlugin, "CNiNodeList::AddNode - duplicate nodename %s", inStr);
		aNode = aNiNodeMapI->second;
		if (inRegistered)
		{
			//need to check if the local hierarchy changed at all for us to actually tear down and create new connections
			//other connections should not get here in the duplicate path since they are actually removed in CleanAllDirty
			if (aNode->localOrParent != inLocalOrParent)
			{
				DBGLOG1( kLogPlugin, "CNiNodeList::AddNode - hierarchy changed for nodename %s", inStr);
				aNode->bRegistered = true;
				aNode->localOrParent= inLocalOrParent;
				if (aNode->fDomainName != nil)
				{
					free(aNode->fDomainName);
					aNode->fDomainName = nil;
				}
				if (aNode->fDomain != nil)
				{
					gNetInfoMutex->Wait();
					DBGLOG( kLogPlugin, "CNiNodeList::AddNode - free the NI domain since it is being replaced");
					//ni_free(aNode->fDomain); //management of domains left to netinfo_open package
					gNetInfoMutex->Signal();
					aNode->fDomain = nil;
				}
			}
			if (aNode->bisDirty)
			{
				aNode->bisDirty = false;
			}
		}
		else if ( !(aNode->bisDirty) ) //need else condition so that setting bisDirty to false above does not drop into this logic
		{
			if (inRegistered)
			{
				if (!(aNode->bRegistered))
				{
					aNode->bRegistered = true;	//always note if a node is registered at any point in time
					aNode->refCount++;			//up refcount if first registered call
				}
			}
			else //duplicate unregistered nodes need to up the ref count
			{
				aNode->refCount++;
			}
		}
		
		if (aNode->listPtr == nil)
		{
			aNode->listPtr = inListPtr;
		}
		else
		{
			::dsDataListDeallocatePriv( inListPtr );
			//need to free the header as well
			free( inListPtr );
			inListPtr = nil;
		}
	}
		
	fMutex.Signal();

	return( eDSNoErr );

} // AddNode


// ---------------------------------------------------------------------------
//	DeleteNode ()
// ---------------------------------------------------------------------------

void* CNiNodeList::DeleteNode ( const char *inStr )
{
	string		aNodeName(inStr);
	sNode	   *aNode		= nil;
	NiNodeMapI	aNiNodeMapI;
	void	   *domain		= nil;

	fMutex.Wait();

	aNiNodeMapI	= fNiNodeMap.find(aNodeName);
	if (aNiNodeMapI != fNiNodeMap.end()) //found the entry
	{
		aNode = aNiNodeMapI->second;
		aNode->refCount--;
		if ( aNode->refCount < 1 )
		{
			if (aNode->listPtr != nil)
			{
				::dsDataListDeallocatePriv( aNode->listPtr );
				//need to free the header as well
				free( aNode->listPtr );
				aNode->listPtr = nil;
			}
			if ( (aNode->fDomain != nil) && !(aNode->bisDirty) )
			{
				domain = aNode->fDomain;
				aNode->fDomain = nil;
			}
			if (aNode->fDomainName != nil)
			{
				DBGLOG1( kLogPlugin, "CNiNodeList::DeleteNode - deleting the nodename %s", aNode->fDomainName);
				free(aNode->fDomainName);
				aNode->fDomainName = nil;
			}
			fNiNodeMap.erase(aNodeName);
			free(aNode);
		}
	}

	fMutex.Signal();

	return( domain );

} // DeleteNode


// ---------------------------------------------------------------------------
//	IsPresent ()
// ---------------------------------------------------------------------------

bool CNiNodeList::IsPresent ( const char *inStr )
{
	bool		found	= false;
	string		aNodeName(inStr);
	sNode	   *aNode	= nil;
	NiNodeMapI	aNiNodeMapI;

	fMutex.Wait();

	aNiNodeMapI	= fNiNodeMap.find(aNodeName);
	if (aNiNodeMapI != fNiNodeMap.end()) //found the entry
	{
		aNode = aNiNodeMapI->second;
		//if (!(aNode->bisDirty))
		{
			found = true;
		}
	}
	
	fMutex.Signal();

	return( found );

} // IsPresent 


// ---------------------------------------------------------------------------
//	IsPresent ()
// ---------------------------------------------------------------------------

bool CNiNodeList::IsPresent ( const char *inStr, tDataList **inListPtr )
{
	bool		found	= false;
	string		aNodeName(inStr);
	sNode	   *aNode	= nil;
	NiNodeMapI	aNiNodeMapI;

	fMutex.Wait();

	aNiNodeMapI	= fNiNodeMap.find(aNodeName);
	if (aNiNodeMapI != fNiNodeMap.end()) //found the entry
	{
		aNode = aNiNodeMapI->second;
		//if (!(aNode->bisDirty))
		{
			found = true;
			if ( inListPtr != nil )
			{
				*inListPtr = aNode->listPtr;
			}
		}
	}
	
	fMutex.Signal();

	return( found );

} // IsPresent 


// ---------------------------------------------------------------------------
//	IsOpen ()
// ---------------------------------------------------------------------------

bool CNiNodeList::IsOpen (	const char	   *inStr,
							char		  **outDomName )
{
	bool		isOpen	= false;
	string		aNodeName(inStr);
	sNode	   *aNode	= nil;
	NiNodeMapI	aNiNodeMapI;

	fMutex.Wait();

	aNiNodeMapI	= fNiNodeMap.find(aNodeName);
	if (aNiNodeMapI != fNiNodeMap.end()) //found the entry
	{
		aNode = aNiNodeMapI->second;
		//save for fine grain logging
		//DBGLOG1( kLogPlugin, "CNiNodeList::IsOpen - <a> found the nodename %s", inStr);
		//DBGLOG1( kLogPlugin, "CNiNodeList::IsOpen - <b> domain name %s ", inStr);
		//if (aNode->bisDirty)
		//{
		//	DBGLOG( kLogPlugin, "CNiNodeList::IsOpen - <c> is set to dirty");
		//}
		//if (aNode->fDomain != nil)
		//{
		//	DBGLOG( kLogPlugin, "CNiNodeList::IsOpen - <d> has domain set");
		//}
		if ( !(aNode->bisDirty) && (aNode->fDomain != nil) && (aNode->fDomainName != nil) )
		{
			//DBGLOG( kLogPlugin, "CNiNodeList::IsOpen - able to retrieve node already open");
			*outDomName	= strdup(aNode->fDomainName);
			isOpen = true;
			aNode->refCount++;
		}
	}
	
	fMutex.Signal();

	return( isOpen );

} // IsOpen 


// ---------------------------------------------------------------------------
//	SetDomainInfo ()
// ---------------------------------------------------------------------------

bool CNiNodeList::SetDomainInfo ( const char	*inStr,
									void		*inDomain,
									char		*inDomainName )
{
	bool		found		= false;
	string		aNodeName(inStr);
	sNode	   *aNode		= nil;
	NiNodeMapI	aNiNodeMapI;

	fMutex.Wait();

	aNiNodeMapI	= fNiNodeMap.find(aNodeName);
	if (aNiNodeMapI == fNiNodeMap.end()) //no entry so add it
	{
		tDataListPtr pDataList = ::dsBuildFromPathPriv( inDomainName, "/" );
		this->AddNode(inStr, pDataList, true); //node will be registered and pDataList consumed by AddNode
		aNiNodeMapI	= fNiNodeMap.find(aNodeName);
	}

	if (aNiNodeMapI != fNiNodeMap.end()) //found the entry
	{
		aNode = aNiNodeMapI->second;
		if ( ( inDomain != nil ) && ( inDomainName != nil ) )
		{
			aNode->fDomain = inDomain;
			
			if ( aNode->fDomainName )
				free(aNode->fDomainName);
				
			aNode->fDomainName = strdup(inDomainName);
			aNode->bisDirty = false;
			aNode->refCount++;
			found = true;
		}
	}

	fMutex.Signal();

	return( found );

} // SetDomainInfo 


// ---------------------------------------------------------------------------
//	CleanUpUnknownConnections ()
// ---------------------------------------------------------------------------

void CNiNodeList::CleanUpUnknownConnections ( const uInt32 inSignature )
{
	sNode	   *aNode		= nil;
	NiNodeMapI	aNiNodeMapI;

	fMutex.Wait();

	for (aNiNodeMapI = fNiNodeMap.begin(); aNiNodeMapI != fNiNodeMap.end(); ++aNiNodeMapI)
	{
		aNode = aNiNodeMapI->second;
		if (( aNode->bRegistered ) && (aNode->localOrParent != 1)) //if registered and NOT local default node then cleanup is performed
		{
			if (aNode->fDomainName != NULL)
			{
				char *aNodeName = BuildDomainPathFromName(aNode->fDomainName);
				if (aNodeName != NULL)
				{
					DBGLOG1( kLogPlugin, "CNiNodeList::CleanUpUnknownConnections - about to clean the nodename %s", aNodeName);
					free(aNodeName);
					aNodeName = NULL;
				}
				CNetInfoPlugin::UnregisterNode( inSignature, aNode->listPtr ); //do not check return code
				//ignore the ref count
				if (aNode->listPtr != nil)
				{
					::dsDataListDeallocatePriv( aNode->listPtr );
					//need to free the header as well
					free( aNode->listPtr );
					aNode->listPtr = nil;
				}
				if (aNode->fDomain != nil)
				{
					//gNetInfoMutex->Wait();
					//DBGLOG( kLogPlugin, "CNiNodeList::CleanUpUnknownConnections - free the ni domain");
					//ni_free(aNode->fDomain); // management of domains left to netinfo_open package netinfo clear is called below
					//gNetInfoMutex->Signal();
					aNode->fDomain = nil;
				}
				if (aNode->fDomainName != nil)
				{
					free(aNode->fDomainName);
					DBGLOG1( kLogPlugin, "CNiNodeList::CleanUpUnknownConnections - cleaning the nodename %s", aNode->fDomainName);
					aNode->fDomainName = nil;
				}
				fNiNodeMap.erase(aNiNodeMapI);
				free(aNode);
				aNiNodeMapI = fNiNodeMap.begin();
			}
		}
	}
	//cleanup all the netinfo connections
	gNetInfoMutex->Wait();
	netinfo_clear(NETINFO_CLEAR_PRESERVE_LOCAL);
	gNetInfoMutex->Signal();

	fMutex.Signal();

} // CleanUpUnknownConnections

// ---------------------------------------------------------------------------
//	CheckForLocalOrParent ()
// ---------------------------------------------------------------------------

uInt32 CNiNodeList::CheckForLocalOrParent ( const char *inName )
{
	string		aNodeName(inName);
	uInt32		localOrParent	= 0;
	NiNodeMapI	aNiNodeMapI;
	sNode	   *aNode			= nil;
	
	fMutex.Wait();

	aNiNodeMapI	= fNiNodeMap.find(aNodeName);
	if (aNiNodeMapI != fNiNodeMap.end()) //found the entry
	{
		aNode			= aNiNodeMapI->second;
		localOrParent	= aNode->localOrParent;
	}
	
	fMutex.Signal();

	return (localOrParent);
	
} // CheckForLocalOrParent


// ---------------------------------------------------------------------------
//	RetrieveNode ()
// ---------------------------------------------------------------------------

void* CNiNodeList::RetrieveNode ( const char *inName )
{
	string		aNodeName(inName);
	void	   *aDomain			= NULL;
	NiNodeMapI	aNiNodeMapI;
	sNode	   *aNode			= nil;
	
	fMutex.Wait();

	//DBGLOG1( kLogPlugin, "CNiNodeList::RetrieveNode - asking for the nodename %s", inName);
	if (strcmp(inName, kstrDefaultLocalNodeName) == 0 )
	{
		aNiNodeMapI	= fNiNodeMap.find(kstrLocalDot);
	}
	else
	{
		aNiNodeMapI	= fNiNodeMap.find(aNodeName);
	}
	if (aNiNodeMapI != fNiNodeMap.end()) //found the entry
	{
		aNode		= aNiNodeMapI->second;
		aDomain		= aNode->fDomain;
	}
	else
	{
		DBGLOG1( kLogPlugin, "CNiNodeList::RetrieveNode - did not find the nodename %s", inName);
	}
	
	fMutex.Signal();

	return (aDomain);
	
} // RetrieveNode


// ---------------------------------------------------------------------------
//	AdjustForParent ()
// ---------------------------------------------------------------------------

void CNiNodeList::AdjustForParent ( void )
{
	string		aNodeName("/");
	sNode	   *aNode	= nil;
	NiNodeMapI	aNiNodeMapI;

	fMutex.Wait();

	aNiNodeMapI	= fNiNodeMap.find(aNodeName);
	if (aNiNodeMapI != fNiNodeMap.end()) //found the entry
	{
		aNode = aNiNodeMapI->second;
		if (aNode != nil)
		{
			if (aNode->listPtr != nil)
			{
				::dsDataListDeallocatePriv( aNode->listPtr );
				//need to free the header as well
				free( aNode->listPtr );
				aNode->listPtr = nil;
			}
			if (aNode->fDomain != nil)
			{
				//if domain outstanding then we rely on netinfo to fail for us
				//if (aNode->refCount < 1)
				//{
					//ni_free(aNode->fDomain); //management of domains left to netinfo_open package
				//}
				aNode->fDomain = nil;
				aNode->bisDirty = true;
				aNode->localOrParent = 0;
			}
			if (aNode->fDomainName != nil)
			{
				free(aNode->fDomainName);
				aNode->fDomainName = nil;
			}
		}
	}
	
	fMutex.Signal();

	return;
	
} // AdjustForParent

// ---------------------------------------------------------------------------
//	RetrieveLocalDomain ()
// ---------------------------------------------------------------------------

void* CNiNodeList::RetrieveLocalDomain ( const char *inStr )
{
	string		aNodeName(inStr);
	sNode	   *aNode		= nil;
	NiNodeMapI	aNiNodeMapI;
	void	   *domain		= nil;

	fMutex.Wait();

	aNiNodeMapI	= fNiNodeMap.find(aNodeName);
	if (aNiNodeMapI != fNiNodeMap.end()) //found the entry
	{
		aNode = aNiNodeMapI->second;
		if (aNode->fDomain != nil)
		{
			domain = aNode->fDomain;
		}
	}

	fMutex.Signal();

	return( domain );

} // RetrieveLocalDomain


