/*
 *  KLogClient.cpp
 *  KLog
 *
 *  $Log: KLogClient.cpp,v $
 *  Revision 1.1  2003/08/22 05:15:54  nano
 *  Added KLog.kext sources
 *
 *  Revision 1.4  2001/10/13 02:27:29  bubba
 *  Got rid of need for config.h file.
 *
 *  Revision 1.3  2001/08/20 23:42:12  bubba
 *  More cleanup.
 *
 */
 
#include <IOKit/IOLib.h>
#include <IOKit/IODataQueue.h>
#include "KLog.h"
#include <string.h>

#define super IOUserClient


//================================================================================================
//	IOKit stuff and Constants
//================================================================================================
OSDefineMetaClassAndStructors( com_apple_iokit_KLogClient, IOUserClient )

#define Q_ON	1
#define Q_OFF	0

#define DEBUG_NAME 				"[KLogClient]"
#define kMethodObjectUserClient ((IOService*) 0 )
#define kMethodObjectDevice 	((IOService*) 1 )

enum
{
	kIOIgnoreCount = 0xFFFFFFFF
};


//================================================================================================
//	Static member variables
//================================================================================================

///
/// Method Table
///
const IOExternalMethod com_apple_iokit_KLogClient::sMethods[] =
{	
	{
		kMethodObjectUserClient,				// object
		( IOMethod ) &com_apple_iokit_KLogClient::QueueMSG,	// func
		kIOUCScalarIScalarO,					// flags
		1,							// # of params in
		0							// # of params out
	},
};
const IOItemCount com_apple_iokit_KLogClient::sMethodCount = sizeof( com_apple_iokit_KLogClient::sMethods ) / 
							sizeof( com_apple_iokit_KLogClient::sMethods[ 0 ] );


//================================================================================================
//   init
//================================================================================================

bool 	com_apple_iokit_KLogClient::init()
{
    bool res;
	
    Q_Err		= 0;
    ActiveFlag	= false;
    ClientLock	= IOLockAlloc();

    if(super::init() == false)
    {
        IOLog( DEBUG_NAME "super::init failed\n");
        Q_Err++;
        return(false);
    }

    //Get mem for new queue of calcuated size 
    myLogQueue = new IODataQueue;      
    if(myLogQueue == 0)
    {
        IOLog( DEBUG_NAME "[ERR]  Failed to allocate memory for buffer\n");
        Q_Err++;
        return false;
    }

    res = myLogQueue->initWithEntries(MAXENTRIES, sizeof(char)*BUFSIZE);
    if(res == false)
    {
        IOLog( DEBUG_NAME "[ERR] Could not initWithEntries\n");
        Q_Err++;
        return false;
    }
        
    State = 1;
	
    return true;
}


//================================================================================================
//   free
//================================================================================================

void 	com_apple_iokit_KLogClient::free()
{
    myLogQueue->release();
	IOLockFree(ClientLock);
    super::free();
}


//================================================================================================
//   start
//================================================================================================

bool	com_apple_iokit_KLogClient::start(IOService *provider)
{
    bool result = false;

    IOLog( DEBUG_NAME "start(%p)\n", provider);
    
    myProvider = OSDynamicCast(com_apple_iokit_KLog, provider);

    if (myProvider != NULL)
        result = super::start(provider);
    else
        result = false;

    if (result == false)
	{
        IOLog( DEBUG_NAME "provider start failed\n");
    }

    return (result);
}


//====================================================================================================
// getTargetAndMethodForIndex
//	We should ALWAYS use an index of 0 for our retrieved method. The reason for this is that we have
//	a single dispatch routine, and the user should have passed in the user client routine index in the
//	param block. Warn if they passed us anything but 0. Maybe we should always return 0 no matter what?
//====================================================================================================

IOExternalMethod * com_apple_iokit_KLogClient::getTargetAndMethodForIndex(IOService **target, UInt32 index)
{
    IOExternalMethod * methodPtr = NULL;
	
    if( index <= (UInt32) sMethodCount ) 
    {
        if ( sMethods[index].object == kMethodObjectUserClient )
		{
			*target = this;
        }
			
		methodPtr = (IOExternalMethod *) &sMethods[0];
		
    }
	else
	{
		IOLog( DEBUG_NAME "[getTargetAndMethodForIndex] BAD index! Value passed was %d.\n", (int)index );
    }
	
    return( methodPtr );
}


//================================================================================================
//   QueueMSG
//================================================================================================

void *	com_apple_iokit_KLogClient::QueueMSG(	void * inPtr,
												void * outPtr, 
												IOByteCount inSize, 
												IOByteCount *outSize, 
												void * inUnused1, 
												void * inUnused2 )
{
    void * result;

	//IOLockLock(ClientLock);

    result = ( void * ) kIOReturnUnsupported;

    if((int)inPtr == Q_ON)
    {
		ActiveFlag = true;
		result = kIOReturnSuccess;
    }
    else if((int)inPtr == Q_OFF)
    {
		ActiveFlag = false;
		result = kIOReturnSuccess;
    }

    return( result );
}



//================================================================================================
//   withTask
//================================================================================================

com_apple_iokit_KLogClient * com_apple_iokit_KLogClient::withTask(task_t owningTask)
{
    com_apple_iokit_KLogClient *client;

    client = new com_apple_iokit_KLogClient;
    if (client != NULL)
	{
        if (client->init() == false)
		{
            client->release();
            client = NULL;
        }
    }
    if (client != NULL)
	{
        client->fTask = owningTask;
    }
    return (client);
}


//================================================================================================
//   registerNotificationPort
//================================================================================================

IOReturn 	com_apple_iokit_KLogClient::registerNotificationPort(mach_port_t port, UInt32 type, UInt32 refCon)
{
    myLogQueue->setNotificationPort(port);
    return(kIOReturnSuccess);
}


//================================================================================================
//   clientMemoryForType
//================================================================================================

IOReturn 	com_apple_iokit_KLogClient::clientMemoryForType( UInt32 type, IOOptionBits * options, IOMemoryDescriptor ** memory )
{
    IOMemoryDescriptor *descriptor;

    descriptor = myLogQueue->getMemoryDescriptor();
    if (descriptor)
	{
        *memory = descriptor;
        *options = 0;
    }
	else
	{	
        Q_Err++;
		IOLog(  DEBUG_NAME "could not allocate memory for data queue");
        return kIOReturnNoMemory;
	}
    
    myProvider->setErr(false);
    return(kIOReturnSuccess);
}


//================================================================================================
//   AddEntry
//================================================================================================

void	com_apple_iokit_KLogClient::AddEntry(void *entry, UInt32 sizeOfentry)
{
    bool res = false;

    if (entry == NULL)
	{
        IOLog( DEBUG_NAME "AddEntry - NULL entry\n");
        return;
    }
    
    if(ActiveFlag == false)
	{
		return;
    };
    
    if((Q_Err == 0) && ((int)sizeOfentry > 0))
    {
        res = myLogQueue->enqueue(entry, sizeOfentry);
		if(res == false)
		{
			if(State == 1)
			{
				IOLog( DEBUG_NAME "ATTN: Could not enqueue, buffer probably full, stalling....\n");
				State = 0;
			}
	    
		}
		else
		{ 
			State = 1;
		}
    }
	else
	{
        IOLog( DEBUG_NAME " Possible Q_Err, or bad log attempt: %d %d\n", Q_Err, (int)sizeOfentry);
        myProvider->setErr(true);
    }
	
}


//================================================================================================
//   set_Q_Size
//================================================================================================

bool	com_apple_iokit_KLogClient::set_Q_Size(UInt32 capacity)
{
    bool res;

    if(capacity == 0)
    {
		return true;
    }
    IOLog( DEBUG_NAME "Reseting size of data queue, all data in queue is lost");

    myLogQueue->release();

    //Get mem for new queue of calcuated size 
    myLogQueue = new IODataQueue;      
    if(myLogQueue == 0)
    {
        IOLog( DEBUG_NAME "[ERR]  Failed to allocate memory for buffer\n");
        Q_Err++;
        return false;
    }

    res = myLogQueue->initWithCapacity(capacity);
    if(res == false)
    {
        IOLog( DEBUG_NAME "[ERR] Could not initWithEntries\n");
        Q_Err++;
        return false;
    }

    return true;
}



//================================================================================================
//   clientClose
//================================================================================================

IOReturn	com_apple_iokit_KLogClient::clientClose(void)
{
    myProvider->setErr(false);
    
    if (myProvider != NULL)
	{
        myProvider->closeChild(this);
        myProvider = NULL;
    }
    return (kIOReturnSuccess);
}


//================================================================================================
//   clientDied
//================================================================================================

IOReturn	com_apple_iokit_KLogClient::clientDied(void)
{
    return (clientClose());
}




