/*
 *  IOFireWireLibIOCFPlugIn.h
 *  IOFireWireFamily
 *
 *  Created by Niels on Thu Feb 27 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 *	$Log: IOFireWireLibIOCFPlugIn.h,v $
 *	Revision 1.2  2003/07/21 06:53:10  niels
 *	merge isoch to TOT
 *
 *	Revision 1.1.2.1  2003/07/01 20:54:23  niels
 *	isoch merge
 *	
 */

#import "IOFireWireLibIUnknown.h"
#import "IOFireWireLibPriv.h"

#import <IOKit/IOCFPlugIn.h>

namespace IOFireWireLib {

	class IOCFPlugIn: public IOFireWireIUnknown
	{
		private:

			static const IOCFPlugInInterface	sInterface ;
			IOFireWireLibDeviceRef				mDevice ;

		public:

			IOCFPlugIn() ;
			virtual ~IOCFPlugIn() ;
	
			virtual HRESULT					QueryInterface( REFIID iid, LPVOID* ppv ) ;
			static IOCFPlugInInterface**	Alloc() ;
			
		private:
		
			// IOCFPlugin methods
			IOReturn 				Probe(CFDictionaryRef propertyTable, io_service_t service, SInt32 *order );
			IOReturn 				Start(CFDictionaryRef propertyTable, io_service_t service );
			IOReturn		 		Stop();	

			//
			// --- CFPlugin static methods ---------
			//
			
			static IOReturn 		SProbe( void* self, CFDictionaryRef propertyTable, io_service_t service, SInt32 *order );
			static IOReturn 		SStart( void* self, CFDictionaryRef propertyTable, io_service_t service );
			static IOReturn 		SStop( void* self );	
	} ;
	
} // namespace
