/*
 *  IOFWUserObjectExporter.cpp
 *  IOFireWireFamily
 *
 *  Created by Niels on Mon Apr 14 2003.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 *	$Log: IOFWUserObjectExporter.cpp,v $
 *	Revision 1.19  2008/05/08 02:33:21  collin
 *	more K64
 *	
 *	Revision 1.18  2008/05/06 03:26:57  collin
 *	more K64
 *	
 *	Revision 1.17  2008/04/30 03:02:13  collin
 *	publicize the exporter
 *	
 *	Revision 1.16  2008/04/11 00:52:37  collin
 *	some K64 changes
 *	
 *	Revision 1.15  2007/03/14 01:01:12  collin
 *	*** empty log message ***
 *	
 *	Revision 1.14  2006/02/09 00:21:51  niels
 *	merge chardonnay branch to tot
 *	
 *	Revision 1.13  2005/09/24 00:55:28  niels
 *	*** empty log message ***
 *	
 *	Revision 1.12  2005/04/12 20:09:13  niels
 *	fix memory leak importing NuDCL programs from user space
 *	
 *	Revision 1.11  2005/04/02 02:43:46  niels
 *	exporter works outside IOFireWireFamily
 *	
 *	Revision 1.10  2005/03/31 02:31:44  niels
 *	more object exporter fixes
 *	
 *	Revision 1.9  2005/03/30 22:14:55  niels
 *	Fixed compile errors see on Tiger w/ GCC 4.0
 *	Moved address-of-member-function calls to use OSMemberFunctionCast
 *	Added owner field to IOFWUserObjectExporter
 *	User client now cleans up published unit directories when client dies
 *	
 *	Revision 1.8.18.3  2006/01/31 04:49:50  collin
 *	*** empty log message ***
 *	
 *	Revision 1.8.18.1  2005/07/23 00:30:44  collin
 *	*** empty log message ***
 *	
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

OSDefineMetaClassAndStructors ( IOFWUserObjectExporter, super );

OSMetaClassDefineReservedUnused(IOFWUserObjectExporter, 0);
OSMetaClassDefineReservedUnused(IOFWUserObjectExporter, 1);
OSMetaClassDefineReservedUnused(IOFWUserObjectExporter, 2);
OSMetaClassDefineReservedUnused(IOFWUserObjectExporter, 3);
OSMetaClassDefineReservedUnused(IOFWUserObjectExporter, 4);
OSMetaClassDefineReservedUnused(IOFWUserObjectExporter, 5);
OSMetaClassDefineReservedUnused(IOFWUserObjectExporter, 6);
OSMetaClassDefineReservedUnused(IOFWUserObjectExporter, 7);

// createWithOwner
//
// static factory method

IOFWUserObjectExporter * IOFWUserObjectExporter::createWithOwner( OSObject * owner )
{
	DebugLog( "IOFWUserObjectExporter::create\n" );

	bool success = true;
	
	IOFWUserObjectExporter * object = NULL;
	
	object = OSTypeAlloc( IOFWUserObjectExporter );
	if( object == NULL )
	{
		success = false;
	}
	
	if( success )
	{
		success = object->initWithOwner( owner );
	}
	
	if( !success )
	{
		if( object )
		{
			object->release();
			object = NULL;
		}
	}
	
	return object;
}

bool
IOFWUserObjectExporter::init()
{
	fLock = IOLockAlloc () ;
	if ( ! fLock )
		return false ;
	
	return super::init () ;
}

bool
IOFWUserObjectExporter::initWithOwner ( OSObject * owner )
{
	fOwner = owner ;
	
	return init() ;
}

void
IOFWUserObjectExporter::free ()
{
	DebugLog( "free object exporter %p, fObjectCount = %d\n", this, fObjectCount ) ;

	removeAllObjects () ;

	if ( fLock )
		IOLockFree( fLock ) ;
	
	fOwner = NULL ;
	
	super::free () ;
}

bool
IOFWUserObjectExporter::serialize ( 
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
IOFWUserObjectExporter::addObject ( OSObject * obj, CleanupFunction cleanupFunction, IOFireWireLib::UserObjectHandle * outHandle )
{
	IOReturn error = kIOReturnSuccess ;
	
	lock () ;
	
	if ( ! fObjects )
	{
		fCapacity = 8 ;
		fObjects = (const OSObject **) new const OSObject * [ fCapacity ] ;
		fCleanupFunctions = new CleanupFunctionWithExporter[ fCapacity ] ;
		
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
		CleanupFunctionWithExporter * newCleanupFunctions = NULL ;

		if ( ! error )
		{
			newObjects = (const OSObject **) new OSObject * [ newCapacity ] ;
		
			if ( !newObjects )
				error = kIOReturnNoMemory ;
		}
		
		if ( !error )
		{
			newCleanupFunctions = new CleanupFunctionWithExporter[ newCapacity ] ;
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
				obj->retain () ;
				fObjects[ index ] = obj ;
				fCleanupFunctions[ index ] = (CleanupFunctionWithExporter)cleanupFunction ;
				*outHandle = (IOFireWireLib::UserObjectHandle)(index + 1) ;		// return index + 1; this means 0 is always an invalid/NULL index...
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
IOFWUserObjectExporter::removeObject ( IOFireWireLib::UserObjectHandle handle )
{
	if ( !handle )
	{
		return ;
	}
	
	lock () ;
	
	DebugLog("user object exporter removing handle %d\n", (uint32_t)handle);

	unsigned index = (unsigned)handle - 1 ;		// handle is object's index + 1; this means 0 is always in invalid/NULL index...
	
	const OSObject * object = NULL ;
	CleanupFunctionWithExporter cleanupFunction = NULL ;
	
	if ( fObjects && ( index < fCapacity ) )
	{
		if ( fObjects[ index ] )
		{
			DebugLog( "found object %p (%s), retain count=%d\n", fObjects[ index ], fObjects[ index ]->getMetaClass()->getClassName(), fObjects[ index ]->getRetainCount() );
			
			object = fObjects[ index ] ;
			fObjects[ index ] = NULL ;

			cleanupFunction = fCleanupFunctions[ index ] ;				
			fCleanupFunctions[ index ] = NULL ;
			
			--fObjectCount ;
		}
	}

	unlock () ;

	if ( object )
	{
		if ( cleanupFunction )
		{
			InfoLog("IOFWUserObjectExporter<%p>::removeObject() -- calling cleanup function for object %p of class %s\n", this, object, object->getMetaClass()->getClassName() ) ;
			(*cleanupFunction)( object, this ) ;
		}
		
		object->release() ;
	}
	
}

const IOFireWireLib::UserObjectHandle
IOFWUserObjectExporter::lookupHandle ( OSObject * object ) const
{
	IOFireWireLib::UserObjectHandle	out_handle = 0;
	
	if ( !object )
	{
		return NULL;
	}
	
	lock () ;

	unsigned index = 0 ;
	
	while ( index < fCapacity )
	{
		if( fObjects[index] == object )
		{
			out_handle = (IOFireWireLib::UserObjectHandle)(index + 1) ;		// return index + 1; this means 0 is always an invalid/NULL index...
			break;
		}
		
		++index;
	}
	
	unlock ();
	
	return out_handle;
}

const OSObject *
IOFWUserObjectExporter::lookupObject ( IOFireWireLib::UserObjectHandle handle ) const
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

const OSObject *
IOFWUserObjectExporter::lookupObjectForType( IOFireWireLib::UserObjectHandle handle, const OSMetaClass * toType ) const
{
	if( !handle )
	{
		return NULL;
	}

	const OSObject * result = NULL;
	
	lock ();
	
	unsigned index = (unsigned)handle - 1;
	
	if ( fObjects && ( index < fCapacity ) )
	{
		result = fObjects [ index ];
	}
	
	if( result )
	{
		result = (OSObject*)OSMetaClassBase::safeMetaCast( result, toType );
	}
	
	if( result )
	{
		result->retain();
	}
	
	unlock ();
	
	return result;
}

void
IOFWUserObjectExporter::removeAllObjects ()
{
	lock () ;
	
	const OSObject ** objects = NULL ;
	CleanupFunctionWithExporter * cleanupFunctions = NULL ;

	unsigned capacity = fCapacity ;

	if ( fObjects )
	{		
		objects = (const OSObject **)IOMalloc( sizeof(const OSObject *) * capacity ) ;
		cleanupFunctions = (CleanupFunctionWithExporter*)IOMalloc( sizeof( CleanupFunctionWithExporter ) * capacity ) ;
	
		if ( objects )
			bcopy( fObjects, objects, sizeof( const OSObject * ) * capacity ) ;
		
		if ( cleanupFunctions )
			bcopy( fCleanupFunctions, cleanupFunctions, sizeof( CleanupFunction) * capacity ) ;
			
		delete[] fObjects ;
		fObjects = NULL ;
		
		delete[] fCleanupFunctions ;
		fCleanupFunctions = NULL ;		

		fObjectCount = 0 ;
		fCapacity = 0 ;
	}
	
	unlock() ;

	if ( objects && cleanupFunctions )
	{
		for ( unsigned index=0; index < capacity; ++index )
		{
			if ( objects[index] )
			{
				InfoLog("IOFWUserObjectExporter<%p>::removeAllObjects() -- remove object %p of class %s\n", this, objects[ index ], objects[ index ]->getMetaClass()->getClassName() ) ;
				
				if ( cleanupFunctions[ index ] )
				{
					InfoLog("IOFWUserObjectExporter<%p>::removeAllObjects() -- calling cleanup function for object %p of type %s\n", this, objects[ index ], objects[ index ]->getMetaClass()->getClassName() ) ;
					(*cleanupFunctions[ index ])( objects[ index ], this ) ;
				}
				
				objects[index]->release() ;
			}
		}

		IOFree( objects, sizeof(const OSObject *) * capacity ) ;
		IOFree( cleanupFunctions, sizeof( CleanupFunction ) * capacity ) ;
	}
}

// getOwner
//
//

OSObject *
IOFWUserObjectExporter::getOwner() const
{
	return fOwner;
}

// lock
//
//

void 
IOFWUserObjectExporter::lock( void ) const
{ 
	IOLockLock ( fLock ); 
}

// unlock
//
//

void 
IOFWUserObjectExporter::unlock( void ) const
{ 
	IOLockUnlock ( fLock ); 
}

