/*
 *  IOFireWireLibBufferFillIsochPort.h
 *  IOFireWireFamily
 *
 *  Created by Niels on Fri Feb 21 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 *	$Log: IOFireWireLibBufferFillIsochPort.h,v $
 *	Revision 1.3  2003/07/24 20:49:50  collin
 *	*** empty log message ***
 *	
 *	Revision 1.2  2003/07/21 06:53:10  niels
 *	merge isoch to TOT
 *	
 *	Revision 1.1.2.2  2003/07/14 22:08:57  niels
 *	*** empty log message ***
 *	
 *	Revision 1.1.2.1  2003/07/01 20:54:23  niels
 *	isoch merge
 *	
 */

#import <IOKit/IOCFPlugIn.h>

// this is to be made public in future: (move to IOFireWireLibIsoch.h)

typedef struct IOFireWireBufferFillIsochPortInterface_t
{
/*!	@class IOFireWireBufferFillIsochPortInterface
	@discussion
*/
/* headerdoc parse workaround	
class IOFireWireBufferFillIsochPortInterface {
public:
*/
	IUNKNOWN_C_GUTS ;
	UInt32 revision, version ;


} IOFireWireBufferFillIsochPortInterface ;


#import "IOFireWireLibIUnknown.h"

#import <IOKit/firewire/IOFireWireLibIsoch.h>

#import <IOKit/IOKitLib.h>

namespace IOFireWireLib {

	class Device ;
	
	class BufferFillIsochPort : public IOFireWireIUnknown
	{
		protected:
		
			BufferFillIsochPort( const IUnknownVTbl & vtable, Device& device, UInt32 interruptMicroseconds, UInt32 numRanges, IOVirtualRange ranges[] ) ;
	} ;
	
	class BufferFillIsochPortCOM : public BufferFillIsochPort
	{
		private:
		
			static const IOFireWireBufferFillIsochPortInterface sInterface ;
	
		public:
		
			BufferFillIsochPortCOM( Device& device, UInt32 interruptMicroseconds, UInt32 numRanges, IOVirtualRange ranges[] ) ;
		
		public:
				
			static IUnknownVTbl**		Alloc( Device& device, UInt32 interruptMicroseconds, UInt32 numRanges, IOVirtualRange ranges[] ) ;
			virtual HRESULT				QueryInterface( REFIID iid, LPVOID* ppv ) ;
	} ;
	
} // namespace
