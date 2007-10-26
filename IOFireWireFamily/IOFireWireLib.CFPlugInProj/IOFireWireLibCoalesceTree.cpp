/*
 *  IOFireWireLibCoalesceTree.cpp
 *  IOFireWireFamily
 *
 *  Created by Niels on Fri Mar 14 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 *	$ Log:IOFireWireLibCoalesceTree.cpp,v $
 */

#import "IOFireWireLibCoalesceTree.h"
#import "IOFireWireLibPriv.h"

#import <unistd.h>

namespace IOFireWireLib {

	// ============================================================
	// CoalesceTree
	// ============================================================

	CoalesceTree::CoalesceTree()
	{
		mTop = nil ;
	}
	
	CoalesceTree::~CoalesceTree()
	{
		DeleteNode(mTop) ;
	}
	
	void
	CoalesceTree::DeleteNode(Node* inNode)
	{
		if (inNode)
		{
			DeleteNode(inNode->left) ;
			DeleteNode(inNode->right) ;
			delete inNode ;
		}
	}
				
	void
	CoalesceTree::CoalesceRange(const IOVirtualRange& inRange)
	{
		if ( inRange.address == 0 or inRange.length == 0)
			return ;
	
		// ranges must be page aligned and have lengths in multiples of the vm page size only:
		IOVirtualRange range = { trunc_page(inRange.address), round_page( (inRange.address & getpagesize() - 1) + inRange.length - 1) } ;
	
		if (mTop)
			CoalesceRange(range, mTop) ;
		else
		{
			mTop					= new Node ;
			mTop->left 				= nil ;
			mTop->right				= nil ;
			mTop->range.address		= range.address ;
			mTop->range.length		= range.length ;
		}
	}
	
	void
	CoalesceTree::CoalesceRange(const IOVirtualRange& inRange, Node* inNode)
	{
		if (inRange.address > inNode->range.address)
		{
			if ( (inRange.address - inNode->range.address) <= inNode->range.length)
			{
				// merge
				inNode->range.length = MAX( inNode->range.length, ( inRange.address + inRange.length - inNode->range.address) ) ;
			}
			else
				if (inNode->right)
					CoalesceRange(inRange, inNode->right) ;
				else
				{
					inNode->right 					= new Node ;
					inNode->right->left				= nil ;
					inNode->right->right			= nil ;
					
					inNode->right->range.address	= inRange.address ;
					inNode->right->range.length		= inRange.length ;
				}
		}
		else	
		{
			if ((inNode->range.address - inRange.address) <= inRange.length)
			{
				// merge
				inNode->range.length 	= MAX( inRange.length, ( inNode->range.address + inNode->range.length - inRange.address) ) ;
				inNode->range.address 	= inRange.address ;
			}
			else
				if (inNode->left)
					CoalesceRange(inRange, inNode->left) ;
				else
				{
					inNode->left					= new Node ;
					inNode->left->left			= nil ;
					inNode->left->right			= nil ;
					
					inNode->left->range.address	= inRange.address ;
					inNode->left->range.length	= inRange.length ;
				}
		}
	}
	
	const UInt32
	CoalesceTree::GetCount() const
	{
		return GetCount(mTop) ;
	}
	
	const UInt32
	CoalesceTree::GetCount(Node* inNode) const
	{
		if (inNode)
			return 1 + GetCount(inNode->left) + GetCount(inNode->right) ;
		else
			return 0 ;
	}
	
	void
	CoalesceTree::GetCoalesceList(IOVirtualRange* outRanges) const
	{
		UInt32 index = 0 ;
		GetCoalesceList(outRanges, mTop, & index) ;
	}
	
	void
	CoalesceTree::GetCoalesceList(IOVirtualRange* outRanges, Node* inNode, UInt32* pIndex) const
	{
		if (inNode)
		{
			// add ranges less than us first
			GetCoalesceList(outRanges, inNode->left, pIndex) ;
	
			// add us
			outRanges[*pIndex].address	= inNode->range.address ;
			outRanges[*pIndex].length	= inNode->range.length ;
			++(*pIndex) ;
			
			// add ranges to the right of us
			GetCoalesceList(outRanges, inNode->right, pIndex) ;
		}
	}
	
} // namespace
