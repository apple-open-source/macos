/*
 *  IOFWUserObjectExporter.h
 *  IOFireWireFamily
 *
 *  Created by Niels on Mon Apr 14 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 *	$Log: IOFWUserObjectExporter.h,v $
 *	Revision 1.2  2003/07/21 06:52:59  niels
 *	merge isoch to TOT
 *	
 *	Revision 1.1.2.1  2003/07/01 20:54:07  niels
 *	isoch merge
 *	
 */

// yes, it's thread safe..

#import "IOFireWireLibPriv.h"

using namespace IOFireWireLib ;

class IOFWUserObjectExporter : public OSObject
{
	OSDeclareDefaultStructors (IOFWUserObjectExporter )

	public :
	
		typedef void (*CleanupFunction)( const OSObject * obj ) ;
		
	private :
	
		unsigned				fCapacity ;
		unsigned				fObjectCount ;
		const OSObject **		fObjects ;
		CleanupFunction *		fCleanupFunctions ;
		IOLock *				fLock ;
		
	public :
	
		// OSObject
		virtual bool			init () ;
		virtual void			free () ;
		virtual bool			serialize ( OSSerialize * s ) const ;
		
		// me
		IOReturn				addObject ( OSObject & obj, CleanupFunction cleanup, UserObjectHandle & outHandle ) ;
		void					removeObject ( UserObjectHandle handle ) ;
		
		// the returned object is retained! This is for thread safety.. if someone else released
		// the object from the pool after you got it, you be in for Trouble
		// Release the returned value when you're done!!
		const OSObject *		lookupObject ( UserObjectHandle handle ) const ;
		void					removeAllObjects () ;

		inline void				lock () const				{ IOLockLock ( fLock ) ; }
		inline void				unlock () const				{ IOLockUnlock ( fLock ) ; }
} ;
