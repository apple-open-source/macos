/*
 *  IOFWUserObjectExporter.h
 *  IOFireWireFamily
 *
 *  Created by Niels on Mon Apr 14 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 *	$Log: IOFWUserObjectExporter.h,v $
 *	Revision 1.7  2007/01/06 06:20:43  collin
 *	*** empty log message ***
 *	
 *	Revision 1.6  2006/02/09 00:21:51  niels
 *	merge chardonnay branch to tot
 *	
 *	Revision 1.5  2005/04/02 02:43:46  niels
 *	exporter works outside IOFireWireFamily
 *	
 *	Revision 1.4  2005/03/31 02:31:44  niels
 *	more object exporter fixes
 *	
 *	Revision 1.3  2005/03/30 22:14:55  niels
 *	Fixed compile errors see on Tiger w/ GCC 4.0
 *	Moved address-of-member-function calls to use OSMemberFunctionCast
 *	Added owner field to IOFWUserObjectExporter
 *	User client now cleans up published unit directories when client dies
 *	
 *	Revision 1.2.20.2  2006/01/31 04:49:50  collin
 *	*** empty log message ***
 *	
 *	Revision 1.2  2003/07/21 06:52:59  niels
 *	merge isoch to TOT
 *	
 *	Revision 1.1.2.1  2003/07/01 20:54:07  niels
 *	isoch merge
 *	
 */

// yes, it's thread safe..
// do not subclass!

#ifdef KERNEL

	namespace IOFireWireLib
	{
		typedef struct AKernelObject* UserObjectHandle;
	}
	
	class IOFWUserObjectExporter : public OSObject
	{
		OSDeclareDefaultStructors (IOFWUserObjectExporter )

		public :
		
			typedef void (*CleanupFunction)( const OSObject * obj ) ;
			typedef void (*CleanupFunctionWithExporter)( const OSObject * obj, IOFWUserObjectExporter * ) ;
			
		private :
		
			unsigned				fCapacity ;
			unsigned				fObjectCount ;
			const OSObject **		fObjects ;
			CleanupFunctionWithExporter *		fCleanupFunctions ;
			IOLock *				fLock ;
			OSObject *				fOwner ;
			
		public :
		
			// OSObject
			virtual bool			init() ;
			bool					initWithOwner( OSObject * owner ) ;
			virtual void			free () ;
			virtual bool			serialize ( OSSerialize * s ) const ;
			
			// me
			IOReturn				addObject ( OSObject & obj, CleanupFunction cleanup, IOFireWireLib::UserObjectHandle & outHandle ) ;
			void					removeObject ( IOFireWireLib::UserObjectHandle handle ) ;
			
			// the returned object is retained! This is for thread safety.. if someone else released
			// the object from the pool after you got it, you be in for Trouble
			// Release the returned value when you're done!!
			const OSObject *		lookupObject ( IOFireWireLib::UserObjectHandle handle ) const ;
			void					removeAllObjects () ;

			inline void				lock () const				{ IOLockLock ( fLock ) ; }
			inline void				unlock () const				{ IOLockUnlock ( fLock ) ; }
			
			OSObject *				getOwner() const ;
	} ;

#else

// always 32 bits wide in user space

namespace IOFireWireLib
{
	typedef UInt32      UserObjectHandle;
}

#endif
