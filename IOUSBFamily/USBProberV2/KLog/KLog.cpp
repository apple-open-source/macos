/*
 * Copyright © 1998-2012 Apple Inc.  All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
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

#define TIMESTAMPSIZE 	sizeof(struct klog64_timeval)
#define LEVELSIZE 		sizeof(uint32_t)
#define TAGSIZE			sizeof(uint32_t)
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

    mTimeVal = new klog64_timeval;
    
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

    if (mClientCount == 0)
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
        if (mClientPtr[i] == ptr)
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

    if (mClientCount > MAXUSERS)
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
	struct timeval 			timeVal;
    if (!format)
    {
        return 0;
    }

	//Lock it up!
	IOLockLock(mLogLock);
	
	//if no clients....
	if (mClientCount == 0)
	{
		returnValue = 0;
		goto exit;
	}
	
	//Get time and pack into buffer
	microtime(&timeVal);
	mTimeVal->tv_sec = timeVal.tv_sec;
	mTimeVal->tv_usec = timeVal.tv_usec;
	
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
    if (!mErrFlag)
    {   
        //Send out to the children
        for(i=0 ; i<=mClientCount ; i++)
        {
            if (mClientPtr[i] != NULL)
            {                            
                mClientPtr[i]->AddEntry((void*)mMsgBuffer, (returnValue + DATAOFFSET + 1));
            }
        }
    }

exit:
    IOLockUnlock(mLogLock);

    return returnValue;
}


