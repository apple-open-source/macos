/*
 *  IOFWUserObjectExporter.cpp
 *  IOFireWireFamily
 *
 *  Created by Niels on Mon Apr 14 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 *	$Log: IOFWUserObjectExporter.cpp,v $
 *	Revision 1.8  2003/12/18 00:08:12  niels
 *	fix panic calling methods on deallocated user objects
 *	
 *	Revision 1.7  2003/12/15 23:31:32  niels
 *	fix object exporter panic when passed NULL object handle
 *	
 *	Revision 1.6  2003/11/20 21:32:58  niels
 *	fix radar 3490815
 *	
 *	Revision 1.5  2003/08/30 00:16:44  collin
 *	*** empty log message ***
 *	
 *	Revision 1.4  2003/08/27 19:27:04  niels
 *	*** empty log message ***
 *	
 *	Revision 1.3  2003/08/08 22:30:32  niels
 *	*** empty log message ***
 *	
 *	Revision 1.2  2003/07/21 06:52:59  niels
 *	merge isoch to TOT
 *	
 *	Revision 1.1.2.1  2003/07/01 20:54:07  niels
 *	isoch merge
 *	
 */

#undef min
#undef max

#import <sys/systm.h>   // for snprintf...

#import "IOFWUserObjectExporter.h"
#import "FWDebugging.h"


#undef super
#define super OSObject

OSDefineMetaClassAndStructors ( IOFWUserObjectExporter, super )


bool
IOFWUserObjectExporter :: init ()
{
	fLock = IOLockAlloc () ;
	if ( ! fLock )
		return false ;
	
	return super :: init () ;
}

void
IOFWUserObjectExporter :: free ()
{
	DebugLog( "free object exporter %p, fObjectCount = %d\n", this, fObjectCount ) ;

	removeAllObjects () ;

	if ( fLock )
		IOLockFree( fLock ) ;
	
	super :: free () ;
}

bool
IOFWUserObjectExporter :: serialize ( 
	OSSerialize * s ) const
{
	lock() ;

	const OSString * keys[ 3 ] =
	{
		OSString::withCString( "capacity" )
		, OSString::withCString( "used" )
		, OSString::withCString( "objects" )
	} ;
	
	const OSObject * objects[ 3 ] =
	{
		OSNumber::withNumber( (unsigned long long)fCapacity, 32 )
		, OSNumber::withNumber( (unsigned long long)fObjectCount, 32 )
		, fObjects ? OSArray::withObjects( fObjects, fObjectCount ) : OSArray::withCapacity(0)
	} ;
	
	OSDictionary * dict = OSDictionary::withObjects( objects, keys, sizeof( keys ) / sizeof( OSObject* ) ) ;

	if ( !dict )
	{
		unlock() ;
		return false ;
	}
		
	bool result = dict->serialize( s ) ;
	
	unlock() ;

	return result ;
}

IOReturn
IOFWUserObjectExporter :: addObject ( OSObject & obj, CleanupFunction cleanupFunction, UserObjectHandle & outHandle )
{
	IOReturn error = kIOReturnSuccess ;
	
	lock () ;
	
	if ( ! fObjects )
	{
		fCapacity = 8 ;
		fObjects = (const OSObject **) new (const OSObject*)[ fCapacity ] ;
		fCleanupFunctions = new (CleanupFunction)[ fCapacity ] ;
		
		if ( ! fObjects || !fCleanupFunctions )
		{
			DebugLog( "Couldn't make fObjects\n" ) ;
			error = kIOReturnNoMemory ;
		}
	}
	
	// if at capacity, expand pool
	if ( fObjectCount == fCapacity )
	{
		unsigned newCapacity = fCapacity + ( fCapacity >> 1 ) ;
		if ( newCapacity > 0xFFFE )
			newCapacity = 0xFFFE ;
			
		if ( newCapacity == fCapacity )	// can't grow!
		{
			DebugLog( "Can't grow object exporter\n" ) ;
			error = kIOReturnNoMemory ;
		}
		
		const OSObject ** newObjects = NULL ;
		CleanupFunction * newCleanupFunctions = NULL ;

		if ( ! error )
		{
			newObjects = (const OSObject **) new (OSObject*)[ newCapacity ] ;
		
			if ( !newObjects )
				error = kIOReturnNoMemory ;
		}
		
		if ( !error )
		{
			newCleanupFunctions = new (CleanupFunction)[ newCapacity ] ;
			if ( !newCleanupFunctions )
				error = kIOReturnNoMemory ;
		}
		
		if ( ! error )
		{
			bcopy ( fObjects, newObjects, fCapacity * sizeof ( OSObject * ) ) ;
			delete[] fObjects ;

			bcopy ( fCleanupFunctions, newCleanupFunctions, fCapacity * sizeof( CleanupFunction * ) ) ;
			delete[] fCleanupFunctions ;

			fObjects = newObjects ;
			fCleanupFunctions = newCleanupFunctions ;
			fCapacity = newCapacity ;
		}
	}
	
	if ( ! error )
	{
		error = kIOReturnNoMemory ;
		unsigned index = 0 ;
		
		while ( index < fCapacity )
		{
			if ( ! fObjects [ index ] )
			{
				obj.retain () ;
				fObjects[ index ] = & obj ;
				fCleanupFunctions[ index ] = cleanupFunction ;
				outHandle = (UserObjectHandle)(index + 1) ;		// return index + 1; this means 0 is always an invalid/NULL index...
				++fObjectCount ;
				error = kIOReturnSuccess ;
				break ;
			}
			
			++index ;
		}
	}
	
	unlock () ;

	ErrorLogCond( error, "fExporter->addObject returning error %x\n", error ) ;
	
	return error ;
}

void
IOFWUserObjectExporter :: removeObject ( UserObjectHandle handle )
{
	if ( !handle )
	{
		return ;
	}
	
	lock () ;
	
	DebugLog("user object exporter removing handle %p\n", handle)

	unsigned index = (unsigned)handle - 1 ;		// handle is object's index + 1; this means 0 is always in invalid/NULL index...
	
	if ( fObjects && ( index < fCapacity ) )
	{
		if ( fObjects[ index ] )
		{
			DebugLog( "found object %p (%s), retain count=%d\n", fObjects[ index ], fObjects[ index ]->getMetaClass()->getClassName(), fObjects[ index ]->getRetainCount() )

			if ( fCleanupFunctions[ index ] )
			{
				DebugLog("calling cleanup function for object %p\n", fObjects[ index ]) ;
				(*(fCleanupFunctions[ index ]))( fObjects[index] ) ;
			}
			
			fObjects[ index ]->release() ;
			fObjects[ index ] = NULL ;
			fCleanupFunctions[ index ] = NULL ;
			
			--fObjectCount ;
		}
	}

	unlock () ;
}

const OSObject *
IOFWUserObjectExporter :: lookupObject ( UserObjectHandle handle ) const
{
	if ( !handle )
	{
		return NULL ;
	}

	const OSObject * result = NULL ;
	
	lock () ;
	
	unsigned index = (unsigned)handle - 1 ;
	
	if ( fObjects && ( index < fCapacity ) )
	{
		result = fObjects [ index ] ;
		if ( result )
		{
			result->retain() ;
		}
	}
		
	unlock () ;
	
	return result ;
}

void
IOFWUserObjectExporter :: removeAllObjects ()
{
	lock () ;
	
	if ( fObjects )
	{
		for ( unsigned index=0; index < fCapacity; ++index )
		{
			if ( fObjects[index] )
			{
				DebugLog("remove object %p (%s)\n", fObjects[ index ], fObjects[ index ]->getMetaClass()->getClassName() ) ;
				
				if ( fCleanupFunctions[ index ] )
				{
					DebugLog("calling cleanup function for object %p\n", fObjects[ index ] ) ;
					(*(fCleanupFunctions[ index ] ))( fObjects[ index ] ) ;
				}
				
				fObjects[index]->release() ;
			}
		}
			
		delete[] fObjects ;
		fObjects = NULL ;
		
		delete[] fCleanupFunctions ;
		fCleanupFunctions = NULL ;
	}
		
	unlock () ;
}
