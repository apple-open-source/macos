/*
	KLog.cpp
	
	$Source: /cvs/root/IOUSBFamily/USBProberV2/KLog/KLog.cpp,v $
	$Log: KLog.cpp,v $
	Revision 1.1.34.1  2004/10/25 15:36:04  nano
	Bring in TOT fixes to PantherUpdate.
	
	Revision 1.1.98.1  2004/10/20 15:27:41  nano
	Potential submissions to Sandbox -- create their own branch
	
	Bug #:
	<rdar://problem/3826068> USB devices on a P30 attached to Q88 do not function after restart
	<rdar://problem/3779852> Q16B EVT Build run in fail Checkconfig Bluetooth *2
	<rdar://problem/3816739>IOUSBFamily needs to support polling interval for High Speed devices
	<rdar://problem/3816743> Low latency for hi-speed API do not fill frTimeStamp.hi and low in completion.
	<rdar://problem/3816749> Low latency for hi-speed API incorrectly treats buffer striding across mem-page
	
	Submitted by:
	Reviewed by:
	
	Revision 1.1  2003/08/22 05:15:54  nano
	Added KLog.kext sources
	
	Revision 1.7  2002/04/28 03:50:02  nano
	Remove offensive IOLogs! Who the heck put those there
	
	Revision 1.6  2001/12/05 16:23:36  nano
	In Log method, call vLog method instead of self!
	
	Revision 1.5  2001/10/13 02:27:29  bubba
	Got rid of need for config.h file.
	
	Revision 1.4  2001/10/09 23:12:28  bubba
	Updated version to 1.0.2, removed non-building targets from the BUILD_ALL
	target, upped the buffers to allow faster logging.
	
	Revision 1.3  2001/08/20 23:42:12  bubba
	More cleanup.
	
	Revision 1.2  2001/08/20 21:33:51  bubba
	Cleaned up code a bit.
	
	Revision 1.1  2001/08/13 22:37:19  davidson
	initial commit of iLogger into io/Tools directory.  Also added the
	BT-iLoggerPlugin in the ToolSources of Odin.
	
	Revision 1.5  2001/07/27 20:02:10  bubba
	According to convention, 'v' denotes functions that take a va_list parameter. I had these backwards
	when I made the logger changes. And since I'm a conventional guy (or not), I changed the function
	names to reflect this naming convention.
	
	Revision 1.4  2001/07/26 20:38:38  davidson
	Updates! Just some basic code cleanup, renamed some vars, etc.
	
	Revision 1.3  2001/07/26 18:23:49  bubba
	Fix misprinted values in the logging mechanism.
	
*/

extern "C" {
#include <sys/time.h>
#include <pexpert/pexpert.h>
}

#include <sys/unistd.h>
#include <sys/systm.h>

#include <IOKit/IOLib.h>
#include <IOKit/IOLocks.h>

#include "KLog.h"
#include "KLogClient.h"


//================================================================================================
//   Defines
//================================================================================================

// Define my superclass
#define super IOService

#define TIMESTAMPSIZE 	sizeof(struct timeval)
#define LEVELSIZE 		sizeof(UInt32)
#define TAGSIZE			sizeof(UInt32)
#define DATAOFFSET 		(TIMESTAMPSIZE + LEVELSIZE + TAGSIZE)
#define MSGSIZE			BUFSIZE - (TIMESTAMPSIZE + LEVELSIZE + TAGSIZE)

#define	DEBUG_NAME		"[KLog]"

//================================================================================================
//   Misc
//================================================================================================

// Make a pointer available to others. 
com_apple_iokit_KLog *	com_apple_iokit_KLog::sLogger = NULL;

OSDefineMetaClassAndStructors( com_apple_iokit_KLog, IOService )

#pragma mark -
#pragma mark ¥ IOService Overrides ¥

//================================================================================================
//   init
//================================================================================================

bool	com_apple_iokit_KLog::init(OSDictionary *dict)
{
    bool res = super::init(dict);

    mClientCount=0;
    mMsgSize = MSGSIZE;
    mMsgBuffer = (unsigned char *)IOMalloc(mMsgSize + DATAOFFSET + 1);
    
    for(UInt8 i=0; i<MAXUSERS;i++) 
        mClientPtr[i]=NULL;
        
    mErrFlag = false;

    mTimeVal = new timeval;
    
    mLogLock = IOLockAlloc();
    
    sLogger = this;
    return res;
}


//================================================================================================
//   free
//================================================================================================

void	com_apple_iokit_KLog::free(void)
{    
    sLogger = NULL;
   
    if (mLogLock) {
        IOLockFree(mLogLock);
        mLogLock = NULL;
    }
    
    if (mMsgBuffer) {
        IOFree(mMsgBuffer, mMsgSize + DATAOFFSET + 1);
        mMsgBuffer = NULL;
    }
    
    super::free();
}


//================================================================================================
//   probe
//================================================================================================

IOService *	com_apple_iokit_KLog::probe(IOService *provider, SInt32 *score)
{
    IOService *res = super::probe(provider, score);
    return res;
}

//================================================================================================
//   start
//================================================================================================

bool	com_apple_iokit_KLog::start(IOService *provider)
{
    bool res = super::start(provider);
    	
    registerService();

    return res;
}

//================================================================================================
//   stop
//================================================================================================

void	com_apple_iokit_KLog::stop(IOService *provider)
{
    super::stop(provider);
}


//================================================================================================
//   closeChild
//================================================================================================

void	com_apple_iokit_KLog::closeChild(com_apple_iokit_KLogClient *ptr)
{
    UInt8 i, idx;
    idx = 0;

    if(mClientCount == 0)
    {
        IOLog( DEBUG_NAME "No clients available to close");
        return;
    }
    
    IOLog( DEBUG_NAME "Closing: %p\n",ptr);
    for(i=0;i<mClientCount;i++)
    {
        IOLog( DEBUG_NAME "userclient ref: %d %p\n", i, mClientPtr[i]);
    }

        
    for(i=0;i<mClientCount;i++)
    {        
        if(mClientPtr[i] == ptr)
        {
            mClientCount--;
            mClientPtr[i] = NULL;
            idx = i;
            i = mClientCount+1;
        }
    }
    
    for(i=idx;i<mClientCount;i++)
    {
        mClientPtr[i] = mClientPtr[i+1];
    }
    mClientPtr[mClientCount+1] = NULL;
}

//================================================================================================
//Called from above by IOServiceOpen() on the user side
//================================================================================================

IOReturn 	com_apple_iokit_KLog::newUserClient( task_t owningTask, void * securityID,
												 UInt32 type, IOUserClient ** handler )
{
    IOReturn ioReturn = kIOReturnSuccess;
    com_apple_iokit_KLogClient *client = NULL;

    if(mClientCount > MAXUSERS)
    {
        IOLog( DEBUG_NAME "client already created, not deleted\n");
        return(kIOReturnError);
    }

    client = com_apple_iokit_KLogClient::withTask(owningTask);
    if (client == NULL) {
        ioReturn = kIOReturnNoResources;
        IOLog("KLog::newUserClient: Can't create user client\n");
    }

    if (ioReturn == kIOReturnSuccess) {
        // Start the client so it can accept requests.
        client->attach(this);
        if (client->start(this) == false) {
            ioReturn = kIOReturnError;
            IOLog("KLog::newUserClient: Can't start user client\n");
        }
    }

    if (ioReturn != kIOReturnSuccess && client != NULL) {
        IOLog( DEBUG_NAME "newUserClient error\n");
        client->detach(this);
        client->release();
    } else {

        mClientPtr[mClientCount] = client;
        
        *handler = client;
        
        client->set_Q_Size(type);
        mClientCount++;
    }
    
    IOLog( DEBUG_NAME "neUserClient() client = %p\n", mClientPtr[mClientCount]);
    return (ioReturn);
}


#pragma mark -
#pragma mark ¥ Class specific stuff ¥

//================================================================================================
//  setErr
//================================================================================================

void com_apple_iokit_KLog::setErr( bool set )
{
    mErrFlag = set;
}


#pragma mark -
#pragma mark ¥ Logging ¥

//================================================================================================
//   Log
//		Allows caller to pass a variable length of arguments; we then package this up and call
//		through to vLog below. You can also call vLog directly if you already have a va_list.
//		Inputs: a tag, a level, a format string and a variable number of arguments.
//================================================================================================

SInt8	com_apple_iokit_KLog::Log( KLogLevel level, KLogTag tag, const char *format, ... )
{
	SInt8	result;
	va_list argList;

        va_start(argList, format);
	result = vLog( tag, level, format, argList );
	va_end( argList );
	
	return( result );
}


//================================================================================================
//	vLog
//		Inputs: a tag, a level, a format string and a va_list.
//================================================================================================

SInt8	com_apple_iokit_KLog::vLog( KLogLevel level, KLogTag tag, const char *format, va_list inArgList )
{
	UInt8		i;
	UInt32		returnValue = 0;

    if(!format)
    {
        return 0;
    }

	//Lock it up!
	IOLockLock(mLogLock);
	
	//if no clients....
	if(mClientCount == 0)
	{
		returnValue = 0;
		goto exit;
	}
	
	//Get time and pack into buffer
	microtime(mTimeVal);
	memcpy(mMsgBuffer, mTimeVal, TIMESTAMPSIZE);
	memcpy((mMsgBuffer + TIMESTAMPSIZE), &tag, TAGSIZE);
	memcpy((mMsgBuffer + TIMESTAMPSIZE + TAGSIZE), &level, LEVELSIZE);
	
	//handle variable length strings
	returnValue = vsnprintf(((char*)mMsgBuffer + DATAOFFSET), (mMsgSize + 1), format, inArgList);
	
	//realloc, and reset all data
	if ((int)returnValue > mMsgSize)
	{
		IOFree(mMsgBuffer, (mMsgSize + DATAOFFSET + 1));
		mMsgSize = returnValue;
		mMsgBuffer = (unsigned char *)IOMalloc(mMsgSize + DATAOFFSET + 1);
	
		memcpy(mMsgBuffer, mTimeVal, TIMESTAMPSIZE);  
		memcpy((mMsgBuffer + TIMESTAMPSIZE), &tag, TAGSIZE);
		memcpy((mMsgBuffer + TIMESTAMPSIZE + TAGSIZE), &level, LEVELSIZE);
	
		returnValue = vsnprintf(((char*)mMsgBuffer + DATAOFFSET), (mMsgSize + 1), format, inArgList);
	
		IOLog( DEBUG_NAME "Resized my mMsgBuffer to %d\n", (int)returnValue);
	}

    //Send buffered string to client and client Queue
    // if no errors have occured
    if(!mErrFlag)
    {   
        //Send out to the children
        for(i=0 ; i<=mClientCount ; i++)
        {
            if(mClientPtr[i] != NULL)
            {                            
                mClientPtr[i]->AddEntry((void*)mMsgBuffer, (returnValue + DATAOFFSET + 1));
            }
        }
    }

exit:
    IOLockUnlock(mLogLock);

    return returnValue;
}


