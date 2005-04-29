/*
 *  IOFWBufferFillIsochPort.h
 *  IOFireWireFamily
 *
 *  Created by Niels on Mon Sep 09 2002.
 *  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * $Log: IOFWBufferFillIsochPort.h,v $
 * Revision 1.4  2004/05/04 22:52:19  niels
 * *** empty log message ***
 *
 * Revision 1.3  2004/01/22 01:49:59  niels
 * fix user space physical address space getPhysicalSegments
 *
 * Revision 1.2  2003/07/21 06:52:58  niels
 * merge isoch to TOT
 *
 * Revision 1.1.2.2  2003/07/18 00:17:41  niels
 * *** empty log message ***
 *
 * Revision 1.1.2.1  2003/07/01 20:54:06  niels
 * isoch merge
 *
 */

#import <IOKit/firewire/IOFWIsochPort.h>

class IOFireWireBus ;


/*!
	@class IOFWBufferFillIsochPort
	@discussion Create using IOFireWireBus::createBufferFillIsochPort()
*/
class IOFWBufferFillIsochPort : public IOFWIsochPort
{
	OSDeclareAbstractStructors( IOFWBufferFillIsochPort )

	public:
	
		typedef void (PacketProc)( 
				OSObject * target, 
				IOFWBufferFillIsochPort * port, 
				IOVirtualRange packets[], 
				unsigned packetCount ) ;
	
	protected:
	
		IOFireWireBus *				fBus ;
		IOByteCount					fBytesPerSecond ;
		UInt32						fIntUSec ;
		PacketProc *				fPacketProc ;
		OSObject *					fTarget ;
		IOFWIsochResourceFlags		fIsochResourceFlags ;
		UInt32						fFlags ;
		UInt8 *						fBackingStore ;
		UInt64						fChannels ;
		
	protected:
	
		
		// Return maximum speed and channels supported
		// (bit n set = chan n supported)
		virtual IOReturn					getSupported( IOFWSpeed& maxSpeed, UInt64& chanSupported ) ;
		
	
		// Allocate hardware resources for port
//		virtual IOReturn					allocatePort( IOFWSpeed speed, UInt32 chan ) ;
//		virtual IOReturn					releasePort() ;
//		virtual IOReturn					start() ;
//		virtual IOReturn					stop() ;
	
	public :
	
		//
		// BufferFillIsochPort methods
		//

		unsigned int						gotIsoch( IOVirtualRange packets[], unsigned int maxPacketCount ) ;
		unsigned int						gotIsochAll( IOVirtualRange packets[], unsigned int maxPacketCount ) ;
		void								pushIsoch() ;

		virtual IOReturn					setIsochResourceFlags( IOFWIsochResourceFlags flags ) ;
		virtual IOFWIsochResourceFlags		getIsochResourceFlags() const ;
		virtual IOReturn					setFlags( UInt32 flags ) ;
		virtual UInt32						getFlags() ;
		virtual IOReturn					setChannels( UInt64 channelMask ) ;

		virtual AbsoluteTime				getInterruptTime() ;
		
	public :
	
		virtual bool						init( 
													IOFireWireBus &			bus, 
													IOByteCount				expectedBytesPerSecond, 
													UInt32					interruptMicroseconds, 
													UInt64					channelMask,
													PacketProc				packetProc,
													OSObject *				target ) ;
#if 0
		virtual bool						initWithBufferSize( 
													IOFireWireBus &			bus, 
													IOByteCount				bufferSize, 
													UInt32					interruptMicroseconds, 
													UInt64					channels,
													PacketProc				packetProc,
													OSObject *				target ) ;
#endif
		virtual void						free () ;
} ;
