/*
 * Copyright (c) 1998-2007 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").	You may not use this file except in compliance with the
 * License.	 Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _IOKIT_IOFWUSERPHYPACKETLISENER_H_
#define _IOKIT_IOFWUSERPHYPACKETLISENER_H_

// private
#import "IOFireWireUserClient.h"

//public
#import <IOKit/firewire/IOFWPHYPacketListener.h>

#pragma mark -

class IOFWUserPHYPacketListener : public IOFWPHYPacketListener
{
	OSDeclareDefaultStructors( IOFWUserPHYPacketListener );

	protected:

	enum ElementState 
	{
        kFreeState = 0,
		kPendingState = 1
    };
	
	enum ElementType 
	{
		kTypeNone = 0,
		kTypeData = 1,
		kTypeSkipped = 2
	};
	
	class PHYRxElement
	{
			
	public:

		PHYRxElement * 		next;			
		PHYRxElement * 		prev;
		
		ElementState		state;
		ElementType			type;
		
		UInt32				data1;
		UInt32				data2;
				
		inline UInt32 getSkippedCount( void )
		{
			return data1;
		}
		
		inline void setSkippedCount( UInt32 count )
		{
			data1 = count;
		}
	};
		
	protected:
		IOFireWireUserClient *		fUserClient;
		UInt32						fMaxQueueCount;
		UInt32						fAllocatedQueueCount;

		OSAsyncReference64			fCallbackAsyncRef;
		OSAsyncReference64			fSkippedAsyncRef;

		PHYRxElement *				fElementWaitingCompletion;
		
		PHYRxElement *				fFreeListHead;				// pointer to first free element
		PHYRxElement *				fFreeListTail;				// pointer to last free element
		
		PHYRxElement *				fPendingListHead;			// pointer to the oldest active element
		PHYRxElement *				fPendingListTail;			// pointer to newest active element

		IOLock *					fLock;
						
	public:
	
		// factory
		static IOFWUserPHYPacketListener *		withUserClient( IOFireWireUserClient * inUserClient, UInt32 queue_count );
		
		// ctor/dtor
		virtual bool	initWithUserClient( IOFireWireUserClient * inUserClient, UInt32 queue_count );
		virtual void	free( void ) APPLE_KEXT_OVERRIDE;
	
		static void		exporterCleanup( const OSObject * self );
		
		IOReturn	setPacketCallback(	OSAsyncReference64		async_ref,
										mach_vm_address_t		callback,
										io_user_reference_t		refCon );
										
		IOReturn	setSkippedCallback(	OSAsyncReference64		async_ref,
										mach_vm_address_t		callback,
										io_user_reference_t		refCon );
																			
		void		clientCommandIsComplete( FWClientCommandID commandID );

	protected:
		virtual		void processPHYPacket( UInt32 data1, UInt32 data2 ) APPLE_KEXT_OVERRIDE;

		void			sendPacketNotification( IOFWUserPHYPacketListener::PHYRxElement * element );

		IOReturn		createAllCommandElements( void );
		void			destroyAllElements( void );
		
		PHYRxElement *	allocateElement( void );
		PHYRxElement *	allocateDataElement( void );
		
		void			deallocateElement( PHYRxElement * element );
		
		void			addElementToPendingList( PHYRxElement * element );
		void			removeElementFromPendingList( PHYRxElement * element );

};

#endif
