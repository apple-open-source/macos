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



void NotificationPort::receive (const MachPlusPlus::Message &msg)
{
	// we got a message, which means that securityd is telling us to pick up messages
	u_int8_t buffer[kSharedMemoryPoolSize];
	SegmentOffsetType length;
	UnavailableReason ur;

	// extract all messages from the buffer, and route them on their way...
	while (mClient->ReadMessage (buffer, length, ur))
	{
		u_int32_t* ptr = (u_int32_t*) buffer;
		
		// we have a message, do the semantics...
		SecurityServer::NotificationDomain domain = (SecurityServer::NotificationDomain) OSSwapBigToHostInt32 (*ptr++);
		SecurityServer::NotificationEvent event = (SecurityServer::NotificationEvent) OSSwapBigToHostInt32 (*ptr++);
		CssmData data ((u_int8_t*) ptr, buffer + length - (u_int8_t*) ptr);

		EventListenerList tempList;
		
		// once we have figured out what the event is, send it on its way
		{
			StLock<Mutex> lock (gNotificationLock ());
			tempList = gEventListeners();
		}
		
		EventListenerList::iterator it = tempList.begin ();
		while (it != tempList.end ())
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
{
	mach_port_t mp;
	notify_register_mach_port (GetNotificationName (), &mp, 0, &mNotifyToken);
	mNotificationPort = new NotificationPort (mp);
	mNotificationPort->enable ();
}



ThreadNotifier::~ThreadNotifier()
{
	notify_cancel (mNotifyToken);
	delete mNotificationPort;
}



ModuleNexus<ThreadNexus<ThreadNotifier> > threadInfo;



static void InitializeNotifications ()
{
	threadInfo()(); // cause the notifier for this thread to initialize
}



//
// Constructing an EventListener immediately enables it for event reception.
//
EventListener::EventListener (NotificationDomain domain, NotificationMask eventMask)
	: mDomain (domain), mMask (eventMask)
{
	StLock<Mutex> lock (gNotificationLock ());

	// make sure that notifications are turned on.
	InitializeNotifications ();
	
	// add this to the global list of notifications
	gEventListeners().push_back (this);
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


} // end namespace SecurityServer
} // end namespace Security
