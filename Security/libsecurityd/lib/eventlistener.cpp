/*
 * Copyright (c) 2003-2004,2006 Apple Computer, Inc. All Rights Reserved.
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

#include <list>
#include <security_utilities/globalizer.h>
#include <security_utilities/threading.h>
#include "eventlistener.h"
#include "SharedMemoryClient.h"
#include <notify.h>
#include "sscommon.h"
#include <sys/syslog.h>

using namespace MachPlusPlus;


namespace Security {
namespace SecurityServer {

typedef RefPointer<EventListener> EventPointer;
typedef std::list<EventPointer> EventListenerList;

static const char* GetNotificationName ()
{
	// the name we give the client depends on the value of the environment variable "SECURITYSERVER"
	const char* name = getenv (SECURITYSERVER_BOOTSTRAP_ENV);
	if (name == NULL)
	{
		name = SECURITY_MESSAGES_NAME;
	}
	
	return name;
}



class SharedMemoryClientMaker
{
private:
	SharedMemoryClient mClient;

public:
	SharedMemoryClientMaker ();
	SharedMemoryClient* Client ();
};



SharedMemoryClientMaker::SharedMemoryClientMaker () : mClient (GetNotificationName (), kSharedMemoryPoolSize)
{
}



SharedMemoryClient* SharedMemoryClientMaker::Client ()
{
	return &mClient;
}



ModuleNexus<EventListenerList> gEventListeners;
ModuleNexus<Mutex> gNotificationLock;
ModuleNexus<SharedMemoryClientMaker> gMemoryClient;

class NotificationPort : public MachPlusPlus::CFAutoPort
{
protected:
	SharedMemoryClient *mClient;

    void ReceiveImplementation(u_int8_t* buffer, SegmentOffsetType length, UnavailableReason ur);
    static void HandleRunLoopTimer(CFRunLoopTimerRef timer, void* info);

public:
	NotificationPort (mach_port_t port);
	virtual ~NotificationPort ();
	virtual void receive(const MachPlusPlus::Message &msg);
};

NotificationPort::NotificationPort (mach_port_t mp) : CFAutoPort (mp)
{
	mClient = gMemoryClient ().Client ();
}



NotificationPort::~NotificationPort ()
{
}



void NotificationPort::ReceiveImplementation(u_int8_t* buffer, SegmentOffsetType length, UnavailableReason ur)
{
    EventListenerList& eventList = gEventListeners();

	// route the message to its destination
    u_int32_t* ptr = (u_int32_t*) buffer;
    
    // we have a message, do the semantics...
    SecurityServer::NotificationDomain domain = (SecurityServer::NotificationDomain) OSSwapBigToHostInt32 (*ptr++);
    SecurityServer::NotificationEvent event = (SecurityServer::NotificationEvent) OSSwapBigToHostInt32 (*ptr++);
    CssmData data ((u_int8_t*) ptr, buffer + length - (u_int8_t*) ptr);

    EventListenerList::iterator it = eventList.begin ();
    while (it != eventList.end ())
    {
        try
        {
            EventPointer ep = *it++;
            if (ep->GetDomain () == domain &&
                (ep->GetMask () & (1 << event)) != 0)
            {
                ep->consume (domain, event, data);
            }
        }
        catch (CssmError &e)
        {
            if (e.error != CSSM_ERRCODE_INTERNAL_ERROR)
            {
                throw;
            }
        }
    }
}



typedef void (^NotificationBlock)();



void NotificationPort::HandleRunLoopTimer(CFRunLoopTimerRef timer, void* info)
{
    // reconstruct our context and call it
    NotificationBlock nb = (NotificationBlock) info;
    nb();
    
    // clean up
    Block_release(nb);
    CFRunLoopTimerInvalidate(timer);
    CFRelease(timer);
}



void NotificationPort::receive (const MachPlusPlus::Message &msg)
{
    /*
        Read each notification received and post a timer for each with an expiration of
        zero.  I'd prefer to use a notification here, but I can't because, according to
        the documentation, each application may only have one notification center and
        the main application should have the right to pick the one it needs.
    */
    
    SegmentOffsetType length;
    UnavailableReason ur;

    bool result;

    while (true)
    {
        u_int8_t *buffer = new u_int8_t[kSharedMemoryPoolSize];
    
       {
            StLock<Mutex> lock (gNotificationLock ());
            result = mClient->ReadMessage(buffer, length, ur);
            if (!result)
            {
                delete [] buffer;
                return;
            }
        }

        // make a block that contains our data
        NotificationBlock nb =
            ^{
                ReceiveImplementation(buffer, length, ur);
                delete [] buffer;
            };
        
        // keep it in scope
        nb = Block_copy(nb);
        
        // set up to run the next time the run loop fires
        CFRunLoopTimerContext ctx;
        memset(&ctx, 0, sizeof(ctx));
        ctx.info = nb;
        
        // make a run loop timer
        CFRunLoopTimerRef timerRef =
            CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent(), 0,
                                 0, 0, NotificationPort::HandleRunLoopTimer, &ctx);
        
        // install it to be run.
        CFRunLoopAddTimer(CFRunLoopGetCurrent(), timerRef, kCFRunLoopDefaultMode);
    }
}



class ThreadNotifier
{
protected:
	NotificationPort *mNotificationPort;
	int mNotifyToken;

public:
	ThreadNotifier();
	~ThreadNotifier();
};



ThreadNotifier::ThreadNotifier()
    : mNotificationPort(NULL)
{
	mach_port_t mp;
	if (notify_register_mach_port (GetNotificationName (), &mp, 0, &mNotifyToken) == NOTIFY_STATUS_OK) {
		mNotificationPort = new NotificationPort (mp);
		mNotificationPort->enable ();
	}
}



ThreadNotifier::~ThreadNotifier()
{
	if (mNotificationPort) {
		notify_cancel (mNotifyToken);
		delete mNotificationPort;
	}
}



ModuleNexus<ThreadNexus<ThreadNotifier> > threadInfo;



static void InitializeNotifications ()
{
	threadInfo()(); // cause the notifier for this thread to initialize
}



EventListener::EventListener (NotificationDomain domain, NotificationMask eventMask)
	: mDomain (domain), mMask (eventMask)
{
	// make sure that notifications are turned on.
	InitializeNotifications ();
}


//
// StopNotification() is needed on destruction; everyone else cleans up after themselves.
//
EventListener::~EventListener ()
{
	StLock<Mutex> lock (gNotificationLock ());
	
	// find the listener in the list and remove it
	EventListenerList::iterator it = std::find (gEventListeners ().begin (),
												gEventListeners ().end (),
												this);
	if (it != gEventListeners ().end ())
	{
		gEventListeners ().erase (it);
	}
}



// get rid of the pure virtual
void EventListener::consume(NotificationDomain, NotificationEvent, const Security::CssmData&)
{
}



void EventListener::FinishedInitialization(EventListener *eventListener)
{
	StLock<Mutex> lock (gNotificationLock ());
	gEventListeners().push_back (eventListener);
}



} // end namespace SecurityServer
} // end namespace Security
