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
		
//TRM	string aString;
//	sNode	   *ourNode		= nil;
//	for (aNiNodeMapI = fNiNodeMap.begin(); aNiNodeMapI != fNiNodeMap.end(); ++aNiNodeMapI)
//	{
//		ourNode = aNiNodeMapI->second;
//		aString = aNiNodeMapI->first;
//		cout << "Before insert " << aString << " " << ourNode->refCount << " " << ourNode->bRegistered << endl;
//	}
	
		fNiNodeMap[aNodeName] = aNode;
		
//	for (aNiNodeMapI = fNiNodeMap.begin(); aNiNodeMapI != fNiNodeMap.end(); ++aNiNodeMapI)
//	{
//		ourNode = aNiNodeMapI->second;
//		aString = aNiNodeMapI->first;
//		cout << "After insert " << aString << " " << ourNode->refCount << " " << ourNode->bRegistered << endl;
//	}

	}
	else //we have a duplicate
	{
		aNode = aNiNodeMapI->second;
		if (aNode->bisDirty)
		{
			//first call in to duplicate resets the flag and the listPtr
			//while maintaining the ref count
			//here we also clean the isDirty flag
			aNode->bisDirty		= false;
			if (inRegistered)
			{
				//need to check if the local hierarchy changed at all for us to actually tear down and create new connections
				if (aNode->localOrParent != inLocalOrParent)
				{
					aNode->bRegistered = true;
					aNode->localOrParent= inLocalOrParent;
					if (aNode->fDomainName != nil)
					{
						free(aNode->fDomainName);
						aNode->fDomainName = nil;
					}
					if (aNode->fDomain != nil)
					{
						//free(aNode->fDomainName); //TODO KW need to have true re-connection strategy
													//but for now we leak if there is truly a change in the netinfo bindings
													//which should be the minority case
						aNode->fDomain = nil;
					}
				}
			}
		}
		else
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
		if (!(aNode->bisDirty))
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
		if (!(aNode->bisDirty))
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
							void		  **outDomain,
							char		  **outDomName,
							ni_id		   *outDirID )
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
		if ( !(aNode->bisDirty) && (aNode->fDomain != nil) && (aNode->fDomainName != nil) )
		{
			*outDomain	= aNode->fDomain;
			*outDomName	= strdup(aNode->fDomainName);
			::memcpy( outDirID, &aNode->fDirID, sizeof( ni_id ) );
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
									char		*inDomainName,
									ni_id		*inDirID )
{
	bool		found		= false;
	string		aNodeName(inStr);
	sNode	   *aNode		= nil;
	NiNodeMapI	aNiNodeMapI;

	fMutex.Wait();

	aNiNodeMapI	= fNiNodeMap.find(aNodeName);
	if (aNiNodeMapI == fNiNodeMap.end()) //no entry so add it
	{
		this->AddNode(inStr, nil, false); //node not registered
		aNiNodeMapI	= fNiNodeMap.find(aNodeName);
	}

	if (aNiNodeMapI != fNiNodeMap.end()) //found the entry
	{
		aNode = aNiNodeMapI->second;
		if ( ( inDomain != nil ) && ( inDomainName != nil ) )
		{
			//KW TODO what if this already exists - unlikely?
			aNode->fDomain = inDomain;
			aNode->fDomainName = inDomainName;
			::memcpy( &aNode->fDirID, inDirID, sizeof( ni_id ) );
			aNode->refCount++;
			found = true;
		}
	}

	fMutex.Signal();

	return( found );

} // SetDomainInfo 


// ---------------------------------------------------------------------------
//	SetAllDirty ()
// ---------------------------------------------------------------------------

void CNiNodeList::SetAllDirty ( void )
{
	sNode	   *aNode		= nil;
	NiNodeMapI	aNiNodeMapI;

	fMutex.Wait();

	for (aNiNodeMapI = fNiNodeMap.begin(); aNiNodeMapI != fNiNodeMap.end(); ++aNiNodeMapI)
	{
		aNode = aNiNodeMapI->second;
		if (aNode->bRegistered) //if not registered then no cleanup performed
		{
			aNode->bisDirty = true;
		}
	}

	fMutex.Signal();

} // SetAllDirty

// ---------------------------------------------------------------------------
//	CleanAllDirty ()
// ---------------------------------------------------------------------------

void CNiNodeList::CleanAllDirty ( const uInt32 inSignature )
{
	sNode	   *aNode		= nil;
	NiNodeMapI	aNiNodeMapI;
	NiNodeMapI	delNiNodeMapI;
	string		aString;

	fMutex.Wait();

	//set the flags of the entries to delete
	for (aNiNodeMapI = fNiNodeMap.begin(); aNiNodeMapI != fNiNodeMap.end();)
	{
		aNode = aNiNodeMapI->second;
		delNiNodeMapI = aNiNodeMapI;
		++aNiNodeMapI;
//TRM	aString = aNiNodeMapI->first;
//		cout << aString << " " << aNode->refCount << " " << aNode->bRegistered << endl;
		
		if ( (aNode->bisDirty) && (aNode->bRegistered) && (aNode->listPtr != nil) )
		{
			if (CNetInfoPlugin::UnregisterNode( inSignature, aNode->listPtr ) == eDSNoErr)
			{
				aNode->refCount--;
				if (aNode->refCount == 0)
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
						//TODO KW what to do with this since it was dirty?
						aNode->fDomain = nil;
					}
					if (aNode->fDomainName != nil)
					{
						free(aNode->fDomainName);
						aNode->fDomainName = nil;
					}
					fNiNodeMap.erase(delNiNodeMapI);
					free(aNode);
				}
			}
		}
	}

	fMutex.Signal();

} // CleanAllDirty


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

