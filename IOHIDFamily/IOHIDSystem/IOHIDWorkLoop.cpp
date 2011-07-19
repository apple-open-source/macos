#include "IOHIDWorkLoop.h"

// system
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOLocksPrivate.h>

#define super IOWorkLoop
OSDefineMetaClassAndStructors( IOHIDWorkLoop, IOWorkLoop )

static SInt32 gCount = 0;

IOHIDWorkLoop * IOHIDWorkLoop::workLoop()
{
    IOHIDWorkLoop *loop;
    
    loop = new IOHIDWorkLoop;
    if(!loop)
        return loop;
    if(!loop->init()) {
        loop->release();
        loop = NULL;
    }
    return loop;
}

bool
IOHIDWorkLoop::init ( void )
{
	
	SInt32	count = OSIncrementAtomic ( &gCount );
	char	name[64];
	
	snprintf ( name, 64, "HID %d", ( int ) count );
	fLockGroup = lck_grp_alloc_init ( name, LCK_GRP_ATTR_NULL );
	if ( fLockGroup )
	{
		gateLock = IORecursiveLockAllocWithLockGroup ( fLockGroup );
	}
	
	return super::init ( );
	
}

void IOHIDWorkLoop::free ( void )
{
	
	if ( fLockGroup )
	{
		lck_grp_free ( fLockGroup );
		fLockGroup = NULL;
	}
	
	super::free ( );
	
}
