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
 * @header CNodeList
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "CLog.h"
#include "CNodeList.h"
#include "CRefTable.h"
#include "CServerPlugin.h"
#include "CString.h"
#include "DSCThread.h"
#include "PrivateTypes.h"
#include "DSUtils.h"
#include "CPlugInList.h"
#include "ServerControl.h"

extern	CPlugInList		*gPlugins;

// ---------------------------------------------------------------------------
//	* CNodeList ()
// ---------------------------------------------------------------------------

CNodeList::CNodeList ( void )
{
	fTreePtr					= nil;
	fCount						= 0;
	fNodeChangeToken			= 1001;	//some arbitrary start value
	fLocalNode					= nil;
	fAuthenticationSearchNode	= nil;
	fContactsSearchNode			= nil;
	fNetworkSearchNode			= nil;
	fConfigureNode				= nil;
	fLocalHostedNodes			= nil;
	fDefaultNetworkNodes		= nil;
    bDHCPLDAPv3InitComplete		= false;
} // CNodeList


// ---------------------------------------------------------------------------
//	* ~CNodeList ()
// ---------------------------------------------------------------------------

CNodeList::~CNodeList ( void )
{
	this->DeleteTree( &fTreePtr );
	fTreePtr = nil;

	if ( fLocalNode != nil )
	{
		if ( fLocalNode->fNodeName != nil )
		{
			free( fLocalNode->fNodeName );
		}
		free( fLocalNode );
		fLocalNode = nil;
	}

	if ( fAuthenticationSearchNode != nil )
	{
		if ( fAuthenticationSearchNode->fNodeName != nil )
		{
			free( fAuthenticationSearchNode->fNodeName );
		}
		free( fAuthenticationSearchNode );
		fAuthenticationSearchNode = nil;
	}

	if ( fContactsSearchNode != nil )
	{
		if ( fContactsSearchNode->fNodeName != nil )
		{
			free( fContactsSearchNode->fNodeName );
		}
		free( fContactsSearchNode );
		fContactsSearchNode = nil;
	}

	if ( fNetworkSearchNode != nil )
	{
		if ( fNetworkSearchNode->fNodeName != nil )
		{
			free( fNetworkSearchNode->fNodeName );
		}
		free( fNetworkSearchNode );
		fNetworkSearchNode = nil;
	}

	if ( fConfigureNode != nil )
	{
		if ( fConfigureNode->fNodeName != nil )
		{
			free( fConfigureNode->fNodeName );
		}
		free( fConfigureNode );
		fConfigureNode = nil;
	}
	
	if ( fLocalHostedNodes != nil )
	{
		this->DeleteTree( &fLocalHostedNodes );
		fLocalHostedNodes = nil;
	}

	if ( fDefaultNetworkNodes != nil )
	{
		this->DeleteTree( &fDefaultNetworkNodes );
		fDefaultNetworkNodes = nil;
	}
	
} // ~CNodeList


// ---------------------------------------------------------------------------
//	* DeleteTree ()
// ---------------------------------------------------------------------------

sInt32 CNodeList::DeleteTree ( sTreeNode **inTree )
{
	fMutex.Wait();

	try
	{
		if ( *inTree != nil )
		{
			this->DeleteTree( &((*inTree)->left) );
			this->DeleteTree( &((*inTree)->right) );
			if ( (*inTree)->fDataListPtr != nil )
			{
				::dsDataListDeallocatePriv( (*inTree)->fDataListPtr );
				//need to free the header as well
				free( (*inTree)->fDataListPtr );
				(*inTree)->fDataListPtr = nil;
			}
			delete( *inTree );
		}
		*inTree = nil;
	}

	catch( sInt32 err )
	{
	}

	fMutex.Signal();


	return( 1 );

} // DeleteTree


// ---------------------------------------------------------------------------
//	* AddNode () RETURNS ZERO IF NODE ALREADY EXISTS
// ---------------------------------------------------------------------------

sInt32 CNodeList::AddNode ( const char		*inNodeName,
							tDataList		*inListPtr,
							eDirNodeType	 inType,
							CServerPlugin	*inPlugInPtr,
							uInt32			 inToken )
{
	sInt32		siResult	= 0;

	fMutex.Wait();

	try
	{
		switch(inType)
		{
			case kLocalHostedType:
				siResult = AddNodeToTree( &fLocalHostedNodes, inNodeName, inListPtr, inType, inPlugInPtr, inToken );
				break;
			case kDefaultNetworkNodeType:
				siResult = AddNodeToTree( &fDefaultNetworkNodes, inNodeName, inListPtr, inType, inPlugInPtr, inToken );
				break;
			case kSearchNodeType:
				if (fAuthenticationSearchNode == nil)
				{
					siResult = AddAuthenticationSearchNode( inNodeName, inListPtr, inType, inPlugInPtr, inToken );
				}
				else
				{
					DBGLOG( kLogApplication, "Attempt to register second Authentication Search Node failed." );
				}
				break;
			case kContactsSearchNodeType:
				if (fContactsSearchNode == nil)
				{
					siResult = AddContactsSearchNode( inNodeName, inListPtr, inType, inPlugInPtr, inToken );
				}
				else
				{
					DBGLOG( kLogApplication, "Attempt to register second Contacts Search Node failed." );
				}
				break;
			case kNetworkSearchNodeType:
				if (fNetworkSearchNode == nil)
				{
					siResult = AddNetworkSearchNode( inNodeName, inListPtr, inType, inPlugInPtr, inToken );
				}
				else
				{
					DBGLOG( kLogApplication, "Attempt to register second Network Search Node failed." );
				}
				break;
			case kConfigNodeType:
				if (fConfigureNode == nil)
				{
					siResult = AddConfigureNode( inNodeName, inListPtr, inType, inPlugInPtr, inToken );
				}
				else
				{
					DBGLOG( kLogApplication, "Attempt to register second Configure Node failed." );
				}
				break;
			//no longer add the localnode name to the overall tree ie. fixed name
			case kLocalNodeType:
				if (fLocalNode == nil)
				{
					siResult = AddLocalNode( inNodeName, inListPtr, inType, inPlugInPtr, inToken );
				}
				else
				{
					DBGLOG( kLogApplication, "Attempt to register second Local Node failed." );
				}
				break;
			case kDHCPLDAPv3NodeType:
				if ( !bDHCPLDAPv3InitComplete )
				{
					siResult = AddDHCPLDAPv3Node( inNodeName, inListPtr, inType, inPlugInPtr, inToken );
				}
				else
				{
					DBGLOG( kLogApplication, "Duplicate attempt to indicate DHCP LDAPv3 initialization has failed." );
				}
				break;
			case kDirNodeType:
				siResult = AddNodeToTree( &fTreePtr, inNodeName, inListPtr, inType, inPlugInPtr, inToken );
				fCount++;
				fNodeChangeToken++;
				gSrvrCntl->NotifyDirNodeAdded(inNodeName);
				break;
			default:
				break;
		}//switch end
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	fMutex.Signal();

	return( siResult );

} // AddNode


// ---------------------------------------------------------------------------
//	* AddLocalNode () RETURNS ZERO IF NODE ALREADY EXISTS
// ---------------------------------------------------------------------------

sInt32 CNodeList::AddLocalNode (	const char		*inNodeName,
									tDataList		*inListPtr,
									eDirNodeType	 inType,
									CServerPlugin	*inPlugInPtr,
									uInt32			 inToken )
{
	sInt32		siResult	= 1;
	sTreeNode  *aLocalNode	= nil;
	//local needed here and in all the addxxxxxnode methods below
	//since we test elsewhere on the sTreeNode pointer to use its contents
	//however, need to be assured that there is contents
	//probably should test on the contents as well

	if ( fLocalNode != nil )
	{
		if (inListPtr != nil)
		{
			::dsDataListDeallocatePriv( inListPtr );
			//need to free the header as well
			free( inListPtr );
			inListPtr = nil;
		}
		return( 0 );
	}

	fMutex.Wait();

	try
	{
		aLocalNode 					= (sTreeNode *)::calloc( 1, sizeof( sTreeNode ) );
		if ( aLocalNode == nil ) throw((sInt32)eMemoryAllocError);
		
		aLocalNode->fNodeName 		= (char *)::calloc( 1, ::strlen( inNodeName ) + 1 );
		if ( aLocalNode->fNodeName == nil ) throw((sInt32)eMemoryAllocError);
		
		::strcpy( aLocalNode->fNodeName, inNodeName );
		aLocalNode->fDataListPtr	= inListPtr;
		aLocalNode->fPlugInPtr		= inPlugInPtr;
		aLocalNode->fPlugInToken	= inToken;
		aLocalNode->fType			= inType;
		aLocalNode->left			= nil;
		aLocalNode->right			= nil;
		fLocalNode					= aLocalNode;
	}

	catch( sInt32 err )
	{
		if ( aLocalNode != nil )
		{
			if (aLocalNode->fNodeName != nil)
			{
				free(aLocalNode->fNodeName);
				aLocalNode->fNodeName = nil;
			}
			if (aLocalNode->fDataListPtr != nil)
			{
				::dsDataListDeallocatePriv( aLocalNode->fDataListPtr );
				//need to free the header as well
				free ( aLocalNode->fDataListPtr );
				aLocalNode->fDataListPtr = nil;
			}
			free( aLocalNode );
			aLocalNode = nil;
			fLocalNode = nil;
		}
		siResult = 0;
	}

	fMutex.Signal();

	return( siResult );

} // AddLocalNode


// ---------------------------------------------------------------------------
//	* AddAuthenticationSearchNode () RETURNS ZERO IF NODE ALREADY EXISTS
// ---------------------------------------------------------------------------

sInt32 CNodeList:: AddAuthenticationSearchNode (	const char		*inNodeName,
													tDataList		*inListPtr,
													eDirNodeType	 inType,
													CServerPlugin	*inPlugInPtr,
													uInt32			 inToken )
{
	sInt32		siResult					= 1;
	sTreeNode  *anAuthenticationSearchNode	= nil;

	if ( fAuthenticationSearchNode != nil )
	{
		if (inListPtr != nil)
		{
			::dsDataListDeallocatePriv( inListPtr );
			//need to free the header as well
			free( inListPtr );
			inListPtr = nil;
		}
		return( 0 );
	}

	fMutex.Wait();

	try
	{
		anAuthenticationSearchNode 					= (sTreeNode *)::calloc( 1, sizeof( sTreeNode ) );
		if ( anAuthenticationSearchNode == nil ) throw((sInt32)eMemoryAllocError);
		
		anAuthenticationSearchNode->fNodeName		= (char *)::calloc( 1, ::strlen( inNodeName ) + 1 );
		if ( anAuthenticationSearchNode->fNodeName == nil ) throw((sInt32)eMemoryAllocError);
		
		::strcpy( anAuthenticationSearchNode->fNodeName, inNodeName );
		anAuthenticationSearchNode->fDataListPtr	= inListPtr;
		anAuthenticationSearchNode->fPlugInPtr		= inPlugInPtr;
		anAuthenticationSearchNode->fPlugInToken	= inToken;
		anAuthenticationSearchNode->fType			= inType;
		anAuthenticationSearchNode->left			= nil;
		anAuthenticationSearchNode->right			= nil;
		fAuthenticationSearchNode					= anAuthenticationSearchNode;
	}

	catch( sInt32 err )
	{
		if ( anAuthenticationSearchNode != nil )
		{
			if (anAuthenticationSearchNode->fNodeName != nil)
			{
				free(anAuthenticationSearchNode->fNodeName);
				anAuthenticationSearchNode->fNodeName = nil;
			}
			if (anAuthenticationSearchNode->fDataListPtr != nil)
			{
				::dsDataListDeallocatePriv( anAuthenticationSearchNode->fDataListPtr );
				//need to free the header as well
				free ( anAuthenticationSearchNode->fDataListPtr );
				anAuthenticationSearchNode->fDataListPtr = nil;
			}
			free( anAuthenticationSearchNode );
			anAuthenticationSearchNode = nil;
			fAuthenticationSearchNode = nil;
		}
		siResult = 0;
	}

	fMutex.Signal();

	return( siResult );

} // AddAuthenticationSearchNode


// ---------------------------------------------------------------------------
//	* AddContactsSearchNode () RETURNS ZERO IF NODE ALREADY EXISTS
// ---------------------------------------------------------------------------

sInt32 CNodeList:: AddContactsSearchNode (	const char		*inNodeName,
											tDataList		*inListPtr,
											eDirNodeType	 inType,
											CServerPlugin	*inPlugInPtr,
											uInt32			 inToken )
{
	sInt32		siResult					= 1;
	sTreeNode  *aContactsSearchNode			= nil;

	if ( fContactsSearchNode != nil )
	{
		if (inListPtr != nil)
		{
			::dsDataListDeallocatePriv( inListPtr );
			//need to free the header as well
			free( inListPtr );
			inListPtr = nil;
		}
		return( 0 );
	}

	fMutex.Wait();

	try
	{
		aContactsSearchNode 				= (sTreeNode *)::calloc( 1, sizeof( sTreeNode ) );
		if ( aContactsSearchNode == nil ) throw((sInt32)eMemoryAllocError);
		
		aContactsSearchNode->fNodeName		= (char *)::calloc( 1, ::strlen( inNodeName ) + 1 );
		if ( aContactsSearchNode->fNodeName == nil ) throw((sInt32)eMemoryAllocError);
		
		::strcpy( aContactsSearchNode->fNodeName, inNodeName );
		aContactsSearchNode->fDataListPtr	= inListPtr;
		aContactsSearchNode->fPlugInPtr		= inPlugInPtr;
		aContactsSearchNode->fPlugInToken	= inToken;
		aContactsSearchNode->fType			= inType;
		aContactsSearchNode->left			= nil;
		aContactsSearchNode->right			= nil;
		fContactsSearchNode					= aContactsSearchNode;
	}

	catch( sInt32 err )
	{
		if ( aContactsSearchNode != nil )
		{
			if (aContactsSearchNode->fNodeName != nil)
			{
				free(aContactsSearchNode->fNodeName);
				aContactsSearchNode->fNodeName = nil;
			}
			if (aContactsSearchNode->fDataListPtr != nil)
			{
				::dsDataListDeallocatePriv( aContactsSearchNode->fDataListPtr );
				//need to free the header as well
				free ( aContactsSearchNode->fDataListPtr );
				aContactsSearchNode->fDataListPtr = nil;
			}
			free( aContactsSearchNode );
			aContactsSearchNode = nil;
			fContactsSearchNode = nil;
		}
		siResult = 0;
	}

	fMutex.Signal();

	return( siResult );

} // AddContactsSearchNode


// ---------------------------------------------------------------------------
//	* AddNetworkSearchNode () RETURNS ZERO IF NODE ALREADY EXISTS
// ---------------------------------------------------------------------------

sInt32 CNodeList:: AddNetworkSearchNode (	const char		*inNodeName,
											tDataList		*inListPtr,
											eDirNodeType	 inType,
											CServerPlugin	*inPlugInPtr,
											uInt32			 inToken )
{
	sInt32		siResult					= 1;
	sTreeNode  *aNetworkSearchNode			= nil;

	if ( fNetworkSearchNode != nil )
	{
		if (inListPtr != nil)
		{
			::dsDataListDeallocatePriv( inListPtr );
			//need to free the header as well
			free( inListPtr );
			inListPtr = nil;
		}
		return( 0 );
	}

	fMutex.Wait();

	try
	{
		aNetworkSearchNode 				= (sTreeNode *)::calloc( 1, sizeof( sTreeNode ) );
		if ( aNetworkSearchNode == nil ) throw((sInt32)eMemoryAllocError);
		
		aNetworkSearchNode->fNodeName		= (char *)::calloc( 1, ::strlen( inNodeName ) + 1 );
		if ( aNetworkSearchNode->fNodeName == nil ) throw((sInt32)eMemoryAllocError);
		
		::strcpy( aNetworkSearchNode->fNodeName, inNodeName );
		aNetworkSearchNode->fDataListPtr	= inListPtr;
		aNetworkSearchNode->fPlugInPtr		= inPlugInPtr;
		aNetworkSearchNode->fPlugInToken	= inToken;
		aNetworkSearchNode->fType			= inType;
		aNetworkSearchNode->left			= nil;
		aNetworkSearchNode->right			= nil;
		fNetworkSearchNode					= aNetworkSearchNode;
	}

	catch( sInt32 err )
	{
		if ( aNetworkSearchNode != nil )
		{
			if (aNetworkSearchNode->fNodeName != nil)
			{
				free(aNetworkSearchNode->fNodeName);
				aNetworkSearchNode->fNodeName = nil;
			}
			if (aNetworkSearchNode->fDataListPtr != nil)
			{
				::dsDataListDeallocatePriv( aNetworkSearchNode->fDataListPtr );
				//need to free the header as well
				free ( aNetworkSearchNode->fDataListPtr );
				aNetworkSearchNode->fDataListPtr = nil;
			}
			free( aNetworkSearchNode );
			aNetworkSearchNode = nil;
			fNetworkSearchNode = nil;
		}
		siResult = 0;
	}

	fMutex.Signal();

	return( siResult );

} // AddNetworkSearchNode


// ---------------------------------------------------------------------------
//	* AddConfigureNode () RETURNS ZERO IF NODE ALREADY EXISTS
// ---------------------------------------------------------------------------

sInt32 CNodeList:: AddConfigureNode (	const char		*inNodeName,
									tDataList		*inListPtr,
									eDirNodeType	 inType,
									CServerPlugin	*inPlugInPtr,
									uInt32			 inToken )
{
	sInt32		siResult				= 1;
	sTreeNode  *aConfigureNode			= nil;

	if ( fConfigureNode != nil )
	{
		if (inListPtr != nil)
		{
			::dsDataListDeallocatePriv( inListPtr );
			//need to free the header as well
			free( inListPtr );
			inListPtr = nil;
		}
		return( 0 );
	}

	fMutex.Wait();

	try
	{
		aConfigureNode 					= (sTreeNode *)::calloc( 1, sizeof( sTreeNode ) );
		if ( aConfigureNode == nil ) throw((sInt32)eMemoryAllocError);
		
		aConfigureNode->fNodeName 		= (char *)::calloc( 1, ::strlen( inNodeName ) + 1 );
		if ( aConfigureNode->fNodeName == nil ) throw((sInt32)eMemoryAllocError);
		
		::strcpy( aConfigureNode->fNodeName, inNodeName );
		aConfigureNode->fDataListPtr	= inListPtr;
		aConfigureNode->fPlugInPtr		= inPlugInPtr;
		aConfigureNode->fPlugInToken	= inToken;
		aConfigureNode->fType			= inType;
		aConfigureNode->left			= nil;
		aConfigureNode->right			= nil;
		fConfigureNode					= aConfigureNode;
	}

	catch( sInt32 err )
	{
		if ( aConfigureNode != nil )
		{
			if (aConfigureNode->fNodeName != nil)
			{
				free(aConfigureNode->fNodeName);
				aConfigureNode->fNodeName = nil;
			}
			if (aConfigureNode->fDataListPtr != nil)
			{
				::dsDataListDeallocatePriv( aConfigureNode->fDataListPtr );
				//need to free the header as well
				free ( aConfigureNode->fDataListPtr );
				aConfigureNode->fDataListPtr = nil;
			}
			free( aConfigureNode );
			aConfigureNode = nil;
			fConfigureNode = nil;
		}
		siResult = 0;
	}

	fMutex.Signal();

	return( siResult );

} // AddConfigureNode


// ---------------------------------------------------------------------------
//	* AddDHCPLDAPv3Node () RETURNS ZERO IF NODE ALREADY EXISTS
// simply used as an indicator to note that DHCP LDAPv3 initialization has completed
// ---------------------------------------------------------------------------

sInt32 CNodeList::AddDHCPLDAPv3Node (	const char		*inNodeName,
                                        tDataList		*inListPtr,
                                        eDirNodeType	 inType,
                                        CServerPlugin	*inPlugInPtr,
										uInt32			 inToken )
{
    bDHCPLDAPv3InitComplete = true;

	return( eDSNoErr );

} // AddDHCPLDAPv3Node


// ---------------------------------------------------------------------------
//	* AddNodeToTree ()
// ---------------------------------------------------------------------------

sInt32 CNodeList:: AddNodeToTree (	sTreeNode	  **inTree,
									const char	   *inNodeName,
									tDataList	   *inListPtr,
									eDirNodeType	inType,
									CServerPlugin  *inPlugInPtr,
									uInt32			 inToken )
{
	sInt32			siResult	= 1;
	sTreeNode	   *current		= nil;
	sTreeNode	   *parent		= nil;
	sTreeNode	   *pNewNode	= nil;
	bool			bAddNode	= true;

	fMutex.Wait();

	try
	{
		// Go down tree to find parent node of new insertion node
		current = *inTree;
		while ( current != nil )
		{
			parent = current;
			siResult = CompareString( inNodeName, current->fNodeName );
			if ( siResult < 0 )
			{
				current = current->left;
			}
			else if ( siResult > 0 )
			{
				current = current->right;
			}
			else //we found a duplicate
			{
				current = nil;
				if (inListPtr != nil)
				{
					::dsDataListDeallocatePriv( inListPtr );
					//need to free the header as well
					free ( inListPtr );
					inListPtr = nil;
				}
				bAddNode = false;
			}
		}

		if (bAddNode)
		{
			pNewNode = (sTreeNode *)::calloc( 1, sizeof( sTreeNode ) );
			if ( pNewNode == nil ) throw((sInt32)eMemoryAllocError);
			
			pNewNode->fNodeName = (char *)::calloc( 1, ::strlen( inNodeName ) + 1 );
			if ( pNewNode->fNodeName == nil ) throw((sInt32)eMemoryAllocError);
			
			::strcpy( pNewNode->fNodeName, inNodeName );
			pNewNode->fDataListPtr	= inListPtr;
			pNewNode->fPlugInPtr	= inPlugInPtr;
			pNewNode->fPlugInToken	= inToken;
			pNewNode->fType			= inType;
			pNewNode->left			= nil;
			pNewNode->right			= nil;
	
			// If this is the first insertion then you are defining the root
			if ( parent == nil )
			{
				*inTree = pNewNode;
	
				siResult = 1;
			}
			else
			{
				// Otherwise we are assigning a newnode to the left or right of the parent leaf node
				siResult = CompareString( inNodeName, parent->fNodeName );
				if ( siResult < 0 )
				{
					parent->left = pNewNode;
				}
				else if ( siResult > 0 )
				{
					parent->right = pNewNode;
				}
			}
		} //if (bAddNode)
	}

	catch( sInt32 err )
	{
		if ( pNewNode != nil )
		{
			if (pNewNode->fNodeName != nil)
			{
				free(pNewNode->fNodeName);
				pNewNode->fNodeName = nil;
			}
			if (pNewNode->fDataListPtr != nil)
			{
				::dsDataListDeallocatePriv( pNewNode->fDataListPtr );
				//need to free the header as well
				free ( pNewNode->fDataListPtr );
				pNewNode->fDataListPtr = nil;
			}
			free( pNewNode );
			pNewNode = nil;
		}
		siResult = 0;
	}

	fMutex.Signal();

	return( siResult );

} // AddNodeToTree


// ---------------------------------------------------------------------------
//	* GetLocalNode ()
// ---------------------------------------------------------------------------

bool CNodeList::GetLocalNode ( CServerPlugin **outPlugInPtr )
{
	bool	found = false;

	fMutex.Wait();

	if ( fLocalNode != nil )
	{
		*outPlugInPtr = GetPluginPtr(fLocalNode);
		found = true;
	}

	fMutex.Signal();

	return( found );

} // GetLocalNode 


// ---------------------------------------------------------------------------
//	* GetLocalNode ()
// ---------------------------------------------------------------------------

bool CNodeList::GetLocalNode ( tDataList **outListPtr )
{
	bool	found = false;

	fMutex.Wait();

	if ( fLocalNode != nil )
	{
		*outListPtr = fLocalNode->fDataListPtr;
		found = true;
	}

	fMutex.Signal();

	return( found );

} // GetLocalNode 


// ---------------------------------------------------------------------------
//	* GetAuthenticationSearchNode ()
// ---------------------------------------------------------------------------

bool CNodeList::GetAuthenticationSearchNode ( CServerPlugin **outPlugInPtr )
{
	bool	found = false;

	fMutex.Wait();

	if ( fAuthenticationSearchNode != nil )
	{
		*outPlugInPtr = GetPluginPtr(fAuthenticationSearchNode);
		found = true;
	}

	fMutex.Signal();

	return( found );

} // GetAuthenticationSearchNode 


// ---------------------------------------------------------------------------
//	* GetAuthenticationSearchNode ()
// ---------------------------------------------------------------------------

bool CNodeList::GetAuthenticationSearchNode ( tDataList **outListPtr )
{
	bool	found = false;

	fMutex.Wait();

	if ( fAuthenticationSearchNode != nil )
	{
		*outListPtr = fAuthenticationSearchNode->fDataListPtr;
		found = true;
	}

	fMutex.Signal();

	return( found );

} // GetAuthenticationSearchNode 


// ---------------------------------------------------------------------------
//	* GetContactsSearchNode ()
// ---------------------------------------------------------------------------

bool CNodeList::GetContactsSearchNode ( CServerPlugin **outPlugInPtr )
{
	bool	found = false;

	fMutex.Wait();

	if ( fContactsSearchNode != nil )
	{
		*outPlugInPtr = GetPluginPtr(fContactsSearchNode);
		found = true;
	}

	fMutex.Signal();

	return( found );

} // GetContactsSearchNode 


// ---------------------------------------------------------------------------
//	* GetContactsSearchNode ()
// ---------------------------------------------------------------------------

bool CNodeList::GetContactsSearchNode ( tDataList **outListPtr )
{
	bool	found = false;

	fMutex.Wait();

	if ( fContactsSearchNode != nil )
	{
		*outListPtr = fContactsSearchNode->fDataListPtr;
		found = true;
	}

	fMutex.Signal();

	return( found );

} // GetContactsSearchNode 


// ---------------------------------------------------------------------------
//	* GetNetworkSearchNode ()
// ---------------------------------------------------------------------------

bool CNodeList::GetNetworkSearchNode ( CServerPlugin **outPlugInPtr )
{
	bool	found = false;

	fMutex.Wait();

	if ( fNetworkSearchNode != nil )
	{
		*outPlugInPtr = GetPluginPtr(fNetworkSearchNode);
		found = true;
	}

	fMutex.Signal();

	return( found );

} // GetNetworkSearchNode 


// ---------------------------------------------------------------------------
//	* GetNetworkSearchNode ()
// ---------------------------------------------------------------------------

bool CNodeList::GetNetworkSearchNode ( tDataList **outListPtr )
{
	bool	found = false;

	fMutex.Wait();

	if ( fNetworkSearchNode != nil )
	{
		*outListPtr = fNetworkSearchNode->fDataListPtr;
		found = true;
	}

	fMutex.Signal();

	return( found );

} // GetNetworkSearchNode 


// ---------------------------------------------------------------------------
//	* RegisterAll ()
// ---------------------------------------------------------------------------

void CNodeList::RegisterAll ( void )
{
	fMutex.Wait();

	try
	{
		this->Register( fTreePtr );
	}

	catch( sInt32 err )
	{
	}

	fMutex.Signal();

} // RegisterAll


// ---------------------------------------------------------------------------
//	* Register ()
// ---------------------------------------------------------------------------

void CNodeList::Register ( sTreeNode *inTree )
{
	if ( inTree != nil )
	{
		Register( inTree->left );
		Register( inTree->right );
	}
} // Register


// ---------------------------------------------------------------------------
//	* GetNodeCount ()
// ---------------------------------------------------------------------------

uInt32 CNodeList::GetNodeCount ( void )
{
	return( fCount );
} // GetNodeCount


// ---------------------------------------------------------------------------
//	* GetNodeChangeToken ()
// ---------------------------------------------------------------------------

uInt32 CNodeList:: GetNodeChangeToken ( void )
{
	return( fNodeChangeToken );
} // GetNodeChangeToken


// ---------------------------------------------------------------------------
//	* CountNodes ()
// ---------------------------------------------------------------------------

void CNodeList::CountNodes ( uInt32 *outCount )
{
	fMutex.Wait();

	try
	{
		this->Count( fTreePtr, outCount );
	}

	catch( sInt32 err )
	{
	}

	fMutex.Signal();

} // CountNodes


// ---------------------------------------------------------------------------
//	* Count ()
// ---------------------------------------------------------------------------


void CNodeList::Count ( sTreeNode *inTree, uInt32 *outCount )
{
	if ( inTree != nil )
	{
		Count( inTree->left, outCount );
		*outCount += 1;
		Count( inTree->right, outCount );
	}

} // Count


// ---------------------------------------------------------------------------
//	* GetNodes ()
// ---------------------------------------------------------------------------

sInt32 CNodeList::GetNodes ( char			   *inStr,
							 tDirPatternMatch	inMatch,
							 tDataBuffer	   *inBuff )
{
	sInt32			siResult	= eDSNoErr;
	sTreeNode	   *outNodePtr	= nil;

	// If the pre-defined nodes are not currently in the list, let's wait
	//	for a bit for them to show up.  Do not grab the list mutex so
	//	it is free for node registration
	if ( (inMatch == eDSLocalNodeNames) && (fLocalNode == nil) )
	{
		WaitForLocalNode();
	}
	if ( (inMatch == eDSAuthenticationSearchNodeName) && (fAuthenticationSearchNode == nil) )
	{
		WaitForLocalNode();
        WaitForDHCPLDAPv3Init();
		WaitForAuthenticationSearchNode();
	}
	else if ( (inMatch == eDSContactsSearchNodeName) && (fContactsSearchNode == nil) )
	{
		WaitForLocalNode();
        WaitForDHCPLDAPv3Init();
		WaitForContactsSearchNode();
	}
	else if ( (inMatch == eDSNetworkSearchNodeName) && (fNetworkSearchNode == nil) )
	{
		WaitForLocalNode(); //assumes that the local node is part of the networksearchnode path
		WaitForNetworkSearchNode();
	}
	else if ( (inMatch == eDSDefaultNetworkNodes) && (fNetworkSearchNode == nil) )
	{
		WaitForLocalNode(); //assumes that the local node is part of the networksearchnode path
	}
	else if ( (inMatch == eDSConfigNodeName) && (fConfigureNode == nil) )
	{
		WaitForConfigureNode();
	}


	fMutex.Wait();

	try
	{
		if ( inMatch == eDSAuthenticationSearchNodeName )
		{
			if ( fAuthenticationSearchNode != nil )
			{
				if ( fAuthenticationSearchNode->fDataListPtr != nil )
				{
					siResult = AddNodePathToTDataBuff( fAuthenticationSearchNode->fDataListPtr, inBuff );
				}
			}
		}
		else if ( inMatch == eDSContactsSearchNodeName )
		{
			if ( fContactsSearchNode != nil )
			{
				if ( fContactsSearchNode->fDataListPtr != nil )
				{
					siResult = AddNodePathToTDataBuff( fContactsSearchNode->fDataListPtr, inBuff );
				}
			}
		}
		else if ( inMatch == eDSNetworkSearchNodeName )
		{
			if ( fNetworkSearchNode != nil )
			{
				if ( fNetworkSearchNode->fDataListPtr != nil )
				{
					siResult = AddNodePathToTDataBuff( fNetworkSearchNode->fDataListPtr, inBuff );
				}
			}
		}
		else if ( inMatch == eDSConfigNodeName )
		{
			if ( fConfigureNode != nil )
			{
				if ( fConfigureNode->fDataListPtr != nil )
				{
					siResult = AddNodePathToTDataBuff( fConfigureNode->fDataListPtr, inBuff );
				}
			}
		}
		else if ( inMatch == eDSLocalNodeNames )
		{
			if ( fLocalNode != nil )
			{
				if ( fLocalNode->fDataListPtr != nil )
				{
					siResult = AddNodePathToTDataBuff( fLocalNode->fDataListPtr, inBuff );
				}
			}
		}
		else if ( inMatch == eDSLocalHostedNodes )
		{
			siResult = this->DoGetNode( fLocalHostedNodes, inStr, inMatch, inBuff, &outNodePtr );
		}
		else if ( inMatch == eDSDefaultNetworkNodes )
		{
			siResult = this->DoGetNode( fDefaultNetworkNodes, inStr, inMatch, inBuff, &outNodePtr );
		}
		else
		{
			siResult = this->DoGetNode( fTreePtr, inStr, inMatch, inBuff, &outNodePtr );
		}
	}

	catch( sInt32 err )
	{
	}

	fMutex.Signal();

	return( siResult );

} // GetNodes


// ---------------------------------------------------------------------------
//	* DoGetNode ()
// ---------------------------------------------------------------------------

sInt32 CNodeList::DoGetNode ( sTreeNode		   *inLeaf,
							 char			   *inStr,
							 tDirPatternMatch	inMatch,
							 tDataBuffer	   *inBuff,
							 sTreeNode		  **outNodePtr )
{
	char	   *aString1	= nil;
	char	   *aString2	= nil;
	bool		bAddToBuff	= false;
	sInt32		uiStrLen	= 0;
	sInt32		uiInStrLen	= 0;
	sInt32		siResult	= eDSNoErr;

	if ( inLeaf != nil )
	{
		siResult = DoGetNode( inLeaf->left, inStr, inMatch, inBuff, outNodePtr );

		switch( inMatch )
		{
			case eDSLocalNodeNames:
				if ( inLeaf->fType == kLocalNodeType )
				{
					bAddToBuff = true;
				}
				break;
				
			case eDSAuthenticationSearchNodeName:
				if ( inLeaf->fType == kSearchNodeType )
				{
					bAddToBuff = true;
				}
				break;
				
			case eDSContactsSearchNodeName:
				if ( inLeaf->fType == kContactsSearchNodeType )
				{
					bAddToBuff = true;
				}
				break;
				
			case eDSNetworkSearchNodeName:
				if ( inLeaf->fType == kNetworkSearchNodeType )
				{
					bAddToBuff = true;
				}
				break;
				
			case eDSConfigNodeName:
				if ( inLeaf->fType == kConfigNodeType )
				{
					bAddToBuff = true;
				}
				break;

			case eDSLocalHostedNodes:
				if ( inLeaf->fType == kLocalHostedType )
				{
					bAddToBuff = true;
				}
				break;

			case eDSDefaultNetworkNodes:
				if ( inLeaf->fType == kDefaultNetworkNodeType )
				{
					bAddToBuff = true;
				}
				break;

			//KW is the following pattern matching UTF-8 capable?
			case eDSExact:
				if ( ::strcmp( inLeaf->fNodeName, inStr ) == 0 )
				{
					bAddToBuff = true;
				}
				break;

			case eDSStartsWith:
				uiInStrLen = ::strlen( inStr );
				if ( ::strncmp( inLeaf->fNodeName, inStr, uiInStrLen ) == 0 )
				{
					bAddToBuff = true;
				}
				break;

			case eDSEndsWith:
				uiInStrLen = ::strlen( inStr );
				uiStrLen = ::strlen( inLeaf->fNodeName );
				if ( uiInStrLen <= uiStrLen )
				{
					aString1 = inLeaf->fNodeName + (uiStrLen - uiInStrLen);
					if ( ::strcmp( aString1, inStr ) == 0 )
					{
						bAddToBuff = true;
					}
				}
				break;

			case eDSContains:
				if ( ::strstr( inLeaf->fNodeName, inStr ) != nil )
				{
					bAddToBuff = true;
				}
				break;

			case eDSiExact:
				uiInStrLen = ::strlen( inStr );
				uiStrLen = ::strlen( inLeaf->fNodeName );
				if ( uiInStrLen == uiStrLen )
				{
					aString1 = inStr;
					aString2 = inLeaf->fNodeName;
					bAddToBuff = true;
					while ( *aString1 != '\0' )
					{
						if ( ::toupper( *aString2 ) != ::toupper( *aString1 ) )
						{
							bAddToBuff = false;
							break;
						}
						aString2++;
						aString1++;
					}
				}
				break;

			case eDSiStartsWith:
				uiInStrLen = ::strlen( inStr );
				uiStrLen = ::strlen( inLeaf->fNodeName );
				if ( uiInStrLen <= uiStrLen )
				{
					aString1 = inStr;
					aString2 = inLeaf->fNodeName;
					bAddToBuff = true;
					while ( *aString1 != '\0' )
					{
						if ( ::toupper( *aString2 ) != ::toupper( *aString1 ) )
						{
							bAddToBuff = false;
							break;
						}
						aString2++;
						aString1++;
					}
				}
				break;

			case eDSiEndsWith:
				uiInStrLen = ::strlen( inStr );
				uiStrLen = ::strlen( inLeaf->fNodeName );
				if ( uiInStrLen <= uiStrLen )
				{
					aString1 = inStr;
					aString2 = inLeaf->fNodeName + ( uiStrLen - uiInStrLen );
					bAddToBuff = true;
					while ( *aString1 != '\0' )
					{
						if ( ::toupper( *aString2 ) != ::toupper( *aString1 ) )
						{
							bAddToBuff = false;
							break;
						}
						aString2++;
						aString1++;
					}
				}
				break;

			case eDSiContains:
				uiInStrLen = ::strlen( inStr );
				uiStrLen = ::strlen( inLeaf->fNodeName );
				if ( uiInStrLen <= uiStrLen )
				{
					CString		tmpStr1( 128 );
					CString		tmpStr2( 128 );

					aString1 = inStr;
					aString2 = inLeaf->fNodeName;
					bAddToBuff = false;

					while ( *aString1 != '\0' )
					{
						tmpStr1.Append( ::toupper( *aString1 ) );
						aString1++;
					}

					while ( *aString2 != '\0' )
					{
						tmpStr2.Append( ::toupper( *aString2 ) );
						aString2++;
					}

					if ( ::strstr( tmpStr2.GetData(), tmpStr1.GetData() ) != nil )
					{
						bAddToBuff = true;
					}
				}
				break;

			default:
				break;
		}

		if ( bAddToBuff == true )
		{
			siResult = AddNodePathToTDataBuff( inLeaf->fDataListPtr, inBuff );
			*outNodePtr = inLeaf;
		}

		if ( siResult == eDSNoErr )
		{
			siResult = DoGetNode( inLeaf->right, inStr, inMatch, inBuff, outNodePtr );
		}
	}

	return( siResult );

} // DoGetNode


// ---------------------------------------------------------------------------
//	* DeleteNode ()
// ---------------------------------------------------------------------------

bool CNodeList::DeleteNode ( char *inStr )
{
	bool		found  		= false;

	fMutex.Wait();

	// kSearchNodeType | kContactsSearchNodeType | kNetworkSearchNodeType | kConfigNodeType | kLocalNodeType
	// these nodes are not allowed to be removed at this time
	
	found = DeleteNodeFromTree( inStr, fLocalHostedNodes );
	
	found = DeleteNodeFromTree( inStr, fDefaultNetworkNodes ) || found;
	
	if (DeleteNodeFromTree( inStr, fTreePtr ))
	{
		found = true;
		fCount--;
		fNodeChangeToken++;
		gSrvrCntl->NotifyDirNodeDeleted(inStr);
	}
	
	fMutex.Signal();

	return( found );

} // DeleteNode


// ---------------------------------------------------------------------------
//	* DeleteNodeFromTree ()
// ---------------------------------------------------------------------------

bool CNodeList::DeleteNodeFromTree ( char *inStr, sTreeNode  *inTree )
{
	bool			found  			= false;
	sInt32			siResult		= 0;
	sTreeNode	   *aTree			= inTree;
	sTreeNode	   *aTreeParent		= nil;
	sTreeNode	   *remTree			= nil;
	sTreeNode	   *remTreeParent	= nil;
	sTreeNode	   *remTreeChild	= nil;
	eDirNodeType	aDirNodeType	= kUnknownNodeType;

	fMutex.Wait();

	// looking only for the three node types that use trees
	if (aTree == fLocalHostedNodes)
	{
		aDirNodeType = kLocalHostedType;
	}
	else if (aTree == fDefaultNetworkNodes)
	{
		aDirNodeType = kDefaultNetworkNodeType;
	}
	else if (aTree == fTreePtr)
	{
		aDirNodeType = kDirNodeType;
	}
	else
	{
		aTree = nil;
	}

	//find the matching node
	while ( (!found) && (aTree != nil) )
	{
		siResult = CompareString( inStr, aTree->fNodeName );
		if ( siResult == 0 )
		{
			found = true;
		}
		else
		{
			aTreeParent = aTree;
			if ( siResult < 0 )
			{
				aTree = aTree->left;
			}
			else
			{
				aTree = aTree->right;
			}
		}
	}

	//remove the matching node from the tree
	if ( found == true )
	{
		if ( aTree->left == nil )
		{
			remTree = aTree->right;
		}
		else
		{
			if ( aTree->right == nil )
			{
				remTree = aTree->left;
			}
			else
			{
				remTreeParent = aTree;
				remTree = aTree->right;
				remTreeChild = remTree->left;
				while ( remTreeChild != nil )
				{
					remTreeParent = remTree;
					remTree = remTreeChild;
					remTreeChild = remTree->left;
				}
				if ( remTreeParent != aTree )
				{
					remTreeParent->left = remTree->right;
					remTree->right = aTree->right;
				}
				remTree->left = aTree->left;
			}
		}

		if ( aTreeParent == nil )
		{
			if (aDirNodeType == kLocalHostedType)
			{
				fLocalHostedNodes = remTree;
			}
			else if (aDirNodeType == kDefaultNetworkNodeType)
			{
				fDefaultNetworkNodes = remTree;
			}
			else if (aDirNodeType == kDirNodeType)
			{
				fTreePtr = remTree;
			}
		}
		else
		{
			if ( aTree == aTreeParent->left )
			{
				aTreeParent->left = remTree;
			}
			else
			{
				aTreeParent->right = remTree;
			}
		}

		if ( aTree->fNodeName != nil )
		{
			free( aTree->fNodeName );
			aTree->fNodeName = nil;
		}

		if ( aTree->fDataListPtr != nil )
		{
			::dsDataListDeallocatePriv( aTree->fDataListPtr );
			//need to free the header as well
			free ( aTree->fDataListPtr );
			aTree->fDataListPtr = nil;
		}

		delete( aTree );
	}
	
	fMutex.Signal();

	return( found );

} // DeleteNodeFromTree


// ---------------------------------------------------------------------------
//	* IsPresent ()
// ---------------------------------------------------------------------------

bool CNodeList::IsPresent ( const char *inStr, eDirNodeType inType )
{
	bool		found		= false;
	sInt32		siResult	= 0;
	sTreeNode  *current		= nil;

	fMutex.Wait();

	if	( inType & ( kSearchNodeType | kContactsSearchNodeType | kNetworkSearchNodeType | kConfigNodeType | kLocalNodeType) )
	{
		//these nodes are not allowed to be compared to at this time
		//so we always return false - actually possible mis-information
		//so the actual AddNode call will need to decide whether to add or not
		fMutex.Signal();
		return false;
	}
	else if (inType == kLocalHostedType)
	{
		current = fLocalHostedNodes;
	}
	else if (inType == kDefaultNetworkNodeType)
	{
		current = fDefaultNetworkNodes;
	}
	else //this will be the simple node type
	{
		current = fTreePtr;
	}
	
	while ( (!found) && (current != nil) )
	{
		siResult = CompareString( inStr, current->fNodeName );
		if ( siResult == 0 )
		{
			found = true;
		}
		else
		{
			if ( siResult < 0 )
			{
				current = current->left;
			}
			else
			{
				current = current->right;
			}
		}
	}

	fMutex.Signal();

	return( found );

} // IsPresent 


// ---------------------------------------------------------------------------
//	* GetPluginHandle ()
// ---------------------------------------------------------------------------

bool CNodeList::GetPluginHandle ( const char *inStr, CServerPlugin **outPlugInPtr )
{
	bool		found		= false;
	sInt32		siResult	= 0;
	sTreeNode  *current		= nil;

	fMutex.Wait();

	//check the special nodes in anticipated order of frequency of request
	if ( fLocalNode != nil)
	{
		if (fLocalNode->fNodeName != nil)
		{
			if (strcmp(inStr,fLocalNode->fNodeName) == 0)
			{
				if ( outPlugInPtr != nil )
				{
					*outPlugInPtr = GetPluginPtr(fLocalNode);
				}
				fMutex.Signal();
				return( true );
			}
		}
	}
	
	if (strlen(inStr) > 6 )  //compare to search node prefix if string long enough
	{
		if ( strncmp(inStr+1,"Search",6) == 0 )  //this is one of the search nodes
		{
			if ( fAuthenticationSearchNode != nil)
			{
				if (fAuthenticationSearchNode->fNodeName != nil)
				{
					if (strcmp(inStr,fAuthenticationSearchNode->fNodeName) == 0)
					{
						if ( outPlugInPtr != nil )
						{
							*outPlugInPtr = GetPluginPtr(fAuthenticationSearchNode);
						}
						fMutex.Signal();
						return( true );
					}
				}
			}
			if ( fNetworkSearchNode != nil)
			{
				if (fNetworkSearchNode->fNodeName != nil)
				{
					if (strcmp(inStr,fNetworkSearchNode->fNodeName) == 0)
					{
						if ( outPlugInPtr != nil )
						{
							*outPlugInPtr = GetPluginPtr(fNetworkSearchNode);
						}
						fMutex.Signal();
						return( true );
					}
				}
			}
		
			if ( fContactsSearchNode != nil)
			{
				if (fContactsSearchNode->fNodeName != nil)
				{
					if (strcmp(inStr,fContactsSearchNode->fNodeName) == 0)
					{
						if ( outPlugInPtr != nil )
						{
							*outPlugInPtr = GetPluginPtr(fContactsSearchNode);
						}
						fMutex.Signal();
						return( true );
					}
				}
			}
		}
	}

	//assumption here is that both the DefaultNetworkNodes and the LocalHostedNodes are also in the main node tree
	//KW why do we keep duplicates across different node types?
	current = fTreePtr;
	while ( (!found) && (current != nil) )
	{
		siResult = CompareString( inStr, current->fNodeName );
		if ( siResult == 0 )
		{
			found = true;
			if ( outPlugInPtr != nil )
			{
				*outPlugInPtr = GetPluginPtr(current);
			}
		}
		else
		{
			if ( siResult < 0 )
			{
				current = current->left;
			}
			else
			{
				current = current->right;
			}
		}
	}

	if ( fConfigureNode != nil)
	{
		if (fConfigureNode->fNodeName != nil)
		{
			if (strcmp(inStr,fConfigureNode->fNodeName) == 0)
			{
				found = true;
				if ( outPlugInPtr != nil )
				{
					*outPlugInPtr = GetPluginPtr(fConfigureNode);
				}
			}
		}
	}

	fMutex.Signal();

	return( found );

} // GetPluginHandle 

CServerPlugin* CNodeList::GetPluginPtr( sTreeNode* nodePtr )
{
	if ( nodePtr->fPlugInPtr == nil )
	{
		nodePtr->fPlugInPtr = gPlugins->GetPlugInPtr( nodePtr->fPlugInToken, true );
	}
	
	return nodePtr->fPlugInPtr;
} // GetPluginPtr

// ---------------------------------------------------------------------------
//	* CompareString ()
// ---------------------------------------------------------------------------

sInt32 CNodeList::CompareString ( const char *inStr_1, const char *inStr_2 )
{
	volatile sInt32	siResult = -22;

	if ( (inStr_1 != nil) && (inStr_2 != nil) )
	{
		siResult = ::strcmp( inStr_1, inStr_2 );
	}

	return( siResult );

} // CompareString


// ---------------------------------------------------------------------------
//	* BuildNodeListBuff ()
// ---------------------------------------------------------------------------

sInt32 CNodeList::BuildNodeListBuff ( sGetDirNodeList *inData )
{
	sInt32			siResult	= eDSNoErr;
	uInt32			outCount	= 0;

	fMutex.Wait();

	inData->fIOContinueData = nil;

	siResult = this->DoBuildNodeListBuff( fTreePtr, inData->fOutDataBuff, &outCount );
	inData->fOutNodeCount = outCount;

	fMutex.Signal();

	return( siResult );

} // BuildNodeListBuff


// ---------------------------------------------------------------------------
//	* DoBuildNodeListBuff ()
// ---------------------------------------------------------------------------

sInt32 CNodeList::DoBuildNodeListBuff ( sTreeNode *inTree, tDataBuffer *inBuff, uInt32 *outCount )
{
	sInt32			siResult	= eDSNoErr;
	tDataList	   *pNodeList	= nil;

	if ( inTree != nil )
	{
		siResult = DoBuildNodeListBuff( inTree->left, inBuff, outCount );
		if ( siResult == eDSNoErr )
		{
			pNodeList = inTree->fDataListPtr;
			if ( pNodeList != nil )
			{
				siResult = AddNodePathToTDataBuff( pNodeList, inBuff );
				if ( siResult == eDSNoErr )
				{
					*outCount	+= 1;
					siResult	= DoBuildNodeListBuff( inTree->right, inBuff, outCount );
				}
				else
				{
					*outCount = 0;
				}
			}
		}
	}

	return( siResult );

} // DoBuildNodeListBuff


// ---------------------------------------------------------------------------
//	* AddNodePathToTDataBuff ()
// ---------------------------------------------------------------------------

sInt32 CNodeList::AddNodePathToTDataBuff ( tDataList *inPtr, tDataBuffer *inBuff )
{
	sInt32				siResult	= eDSBufferTooSmall;
	FourCharCode		uiBuffType	= 'npss'; // node path strings
	uInt32				inBuffType	= 'xxxx';
	char			   *segmentStr	= nil;
	uInt16				uiStrLen	= 0;
	uInt32				uiItemCnt	= 0;
	uInt16				segmentCnt	= 0;
	uInt32				pathLength	= 0;
	uInt32				iSegment	= 0;
	uInt32				offset		= 0;

	if ( (inPtr != nil) && (inBuff != nil) )
	{
	//buffer format ie. only used by FW in dsGetDirNodeName
	//4 byte tag
	//4 byte count of node paths
	//repeated:
	// 2 byte count of string segments - ttt
	// sub repeated:
	// 2 byte segment string length - ttt
	// actual segment string - ttt
	// ..... blank space
	// offsets for each node path in reverse order from end of buffer
	// note that buffer length is length of data aboved tagged with ttt
		if ( inBuff->fBufferSize > 7 )
		{
			//get the current buff type
			::memcpy( &inBuffType, inBuff->fBufferData,  4 );
			
			if (inBuffType != uiBuffType) //new buffer
			{
				// set buffer type tag
				::memcpy( inBuff->fBufferData, &uiBuffType, 4 );
				//ensure length is zero
				inBuff->fBufferLength = 0;
			}
			else
			{
				//get the current count
				::memcpy( &uiItemCnt, inBuff->fBufferData + 4,  4 );
			}
			
			//retrieve number of segments in node path
			segmentCnt	= (uInt16) dsDataListGetNodeCountPriv( inPtr );
			//retrieve the segment's overall length
			pathLength	= dsGetDataLengthPriv ( inPtr );
			//adjust the pathLength for each segment string length
			//and overall segment count stored each in 2 bytes
			pathLength	+= segmentCnt * 2 + 2;
			
			//check that buffer will not overflow with this addition
			//ie. buffer length plus new length plus 8 header bytes needs to be
			//less than the buffer size minus the storage for the entry offsets
			if ( ( inBuff->fBufferLength + pathLength + 8) <= (inBuff->fBufferSize - ((uiItemCnt+1) * 4)) )
			{
				//increment the count of node paths to be in the buffer
				uiItemCnt++;
				::memcpy( inBuff->fBufferData + 4, &uiItemCnt, 4 );
				
				//set the offset for this node path
				offset = inBuff->fBufferLength + 8; //shift past header of data
				::memcpy( inBuff->fBufferData + inBuff->fBufferSize - (uiItemCnt * 4) , &(offset), 4 );
				
				//set the number of segments for this node path
				::memcpy( inBuff->fBufferData + 8 + inBuff->fBufferLength, &segmentCnt, 2 );
				
				//add to the data length the segment count 2 bytes
				inBuff->fBufferLength += 2;
				
				for (iSegment = 1; iSegment <= segmentCnt; iSegment++ )
				{
					segmentStr = dsDataListGetNodeStringPriv( inPtr, iSegment );
					if (segmentStr != nil);
					{
						uiStrLen = strlen(segmentStr);
						::memcpy( inBuff->fBufferData + 8 + inBuff->fBufferLength, &uiStrLen, 2 );
						::memcpy( inBuff->fBufferData + 8 + inBuff->fBufferLength + 2, segmentStr, uiStrLen );
						
						free(segmentStr);
						segmentStr = nil;
						
						//update the used buffer length
						inBuff->fBufferLength += 2 + uiStrLen;
					}
				}

				siResult = eDSNoErr;
			}
			else
			{
				// Buffer is full but we return that it is too small since
				// we want to return all results in a single buffer
				siResult = eDSBufferTooSmall;
			}
		}
	}

	return( siResult );

} // AddNodePathToTDataBuff


// ---------------------------------------------------------------------------
//	* WaitForAuthenticationSearchNode ()
// ---------------------------------------------------------------------------

void CNodeList::WaitForAuthenticationSearchNode( void )
{
	DSSemaphore		timedWait;
	time_t			waitTime	= ::time( nil ) + 120;

	// Grab the wait semaphore
	fWaitForAuthenticationSN.Wait();

	while ( fAuthenticationSearchNode == nil )
	{
		// Check every .5 seconds
		timedWait.Wait( (uInt32)(.5 * kMilliSecsPerSec) );

		// Wait for 2 minutes
		if ( ::time( nil ) > waitTime )
		{
			// We have waited as long as we are going to at this time
			break;
		} 
	}

	// Now let it go
	fWaitForAuthenticationSN.Signal();

} // WaitForAuthenticationSearchNode


// ---------------------------------------------------------------------------
//	* WaitForContactsSearchNode ()
// ---------------------------------------------------------------------------

void CNodeList:: WaitForContactsSearchNode ( void )
{
	DSSemaphore		timedWait;
	time_t			waitTime	= ::time( nil ) + 120;

	// Grab the wait semaphore
	fWaitForContactsSN.Wait();

	while ( fContactsSearchNode == nil )
	{
		// Check every .5 seconds
		timedWait.Wait( (uInt32)(.5 * kMilliSecsPerSec) );

		// Wait for 2 minutes
		if ( ::time( nil ) > waitTime )
		{
			// We have waited as long as we are going to at this time
			break;
		} 
	}

	// Now let it go
	fWaitForContactsSN.Signal();

} // WaitForContactsSearchNode


// ---------------------------------------------------------------------------
//	* WaitForNetworkSearchNode ()
// ---------------------------------------------------------------------------

void CNodeList:: WaitForNetworkSearchNode ( void )
{
	DSSemaphore		timedWait;
	time_t			waitTime	= ::time( nil ) + 120;

	// Grab the wait semaphore
	fWaitForNetworkSN.Wait();

	while ( fNetworkSearchNode == nil )
	{
		// Check every .5 seconds
		timedWait.Wait( (uInt32)(.5 * kMilliSecsPerSec) );

		// Wait for 2 minutes
		if ( ::time( nil ) > waitTime )
		{
			// We have waited as long as we are going to at this time
			break;
		} 
	}

	// Now let it go
	fWaitForNetworkSN.Signal();

} // WaitForNetworkSearchNode


// ---------------------------------------------------------------------------
//	* WaitForLocalNode ()
// ---------------------------------------------------------------------------

void CNodeList::WaitForLocalNode( void )
{
	DSSemaphore		timedWait;
	time_t			waitTime	= ::time( nil ) + 120;

	// Grab the wait semaphore
	fWaitForLN.Wait();

	while ( fLocalNode == nil )
	{
		// Check every .5 seconds
		timedWait.Wait( (uInt32)(.5 * kMilliSecsPerSec) );

		// Wait for 2 minutes
		if ( ::time( nil ) > waitTime )
		{
			// We have waited as long as we are going to at this time
			break;
		} 
	}

	// Now let it go
	fWaitForLN.Signal();

} // WaitForLocalNode


// ---------------------------------------------------------------------------
//	* WaitForConfigureNode ()
// ---------------------------------------------------------------------------

void CNodeList::WaitForConfigureNode( void )
{
	DSSemaphore		timedWait;
	time_t			waitTime	= ::time( nil ) + 120;

	// Grab the wait semaphore
	fWaitForConfigureN.Wait();

	while ( fConfigureNode == nil )
	{
		// Check every .5 seconds
		timedWait.Wait( (uInt32)(.5 * kMilliSecsPerSec) );

		// Wait for 2 minutes
		if ( ::time( nil ) > waitTime )
		{
			// We have waited as long as we are going to at this time
			break;
		} 
	}

	// Now let it go
	fWaitForConfigureN.Signal();

} // WaitForConfigureNode

// ---------------------------------------------------------------------------
//	* WaitForDHCPLDAPv3Init ()
// ---------------------------------------------------------------------------

void CNodeList::WaitForDHCPLDAPv3Init( void )
{
	DSSemaphore		timedWait;
	time_t			waitTime	= ::time( nil ) + 120;

	// Grab the wait semaphore
	fWaitForDHCPLDAPv3InitFlag.Wait();

	while ( !bDHCPLDAPv3InitComplete )
	{
		// Check every .5 seconds
		timedWait.Wait( (uInt32)(.5 * kMilliSecsPerSec) );

		// Wait for 2 minutes
		if ( ::time( nil ) > waitTime )
		{
			// We have waited as long as we are going to at this time
			break;
		} 
	}

	// Now let it go
	fWaitForDHCPLDAPv3InitFlag.Signal();

} // WaitForDHCPLDAPv3Init

