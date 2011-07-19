/*
 *  IOFireWireLibCoalesceTree.h
 *  IOFireWireFamily
 *
 *  Created by Niels on Fri Mar 14 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 *	$Log: IOFireWireLibCoalesceTree.h,v $
 *	Revision 1.2  2003/07/21 06:53:10  niels
 *	merge isoch to TOT
 *
 *	Revision 1.1.2.1  2003/07/01 20:54:23  niels
 *	isoch merge
 *	
 */

#import <IOKit/IOKitLib.h>

namespace IOFireWireLib {

	// ============================================================
	//
	// CoalesceTree
	//
	// ============================================================
	
	class CoalesceTree
	{
		struct Node
		{
			Node*				left ;
			Node*				right ;
			IOVirtualRange		range ;
		} ;
	
		public:
		
			CoalesceTree() ;
			~CoalesceTree() ;
		
		public:
					
			void	 			CoalesceRange(const IOVirtualRange& inRange) ;
			const UInt32	 	GetCount() const ;
			void			 	GetCoalesceList(IOVirtualRange* outRanges) const ;
	
		protected:
		
			void				DeleteNode(Node* inNode) ;
			void				CoalesceRange(const IOVirtualRange& inRange, Node* inNode) ;
			const UInt32		GetCount(Node* inNode) const ;
			void				GetCoalesceList(IOVirtualRange* outRanges, Node* inNode, UInt32* pIndex) const ;
	
		protected:
		
			Node *	mTop ;
	} ;
	
} // namespace
