#include "eventlistener.h"


namespace Security {
namespace SecurityServer {

EventListener::EventListener (CssmAllocator &standard, CssmAllocator &returning) : mClientSession (standard, returning),
																				   	mMachPortRef (NULL),
																					mRunLoopSourceRef (NULL)

{
	Initialize ();
}



EventListener::~EventListener ()
{
	if (mMachPortRef != NULL)
	{
		mach_port_t mp = CFMachPortGetPort (mMachPortRef);
		mClientSession.stopNotification (mp);
		CFRelease (mMachPortRef);
	}
	
	if (mRunLoopSourceRef != NULL)
	{
		CFRelease (mRunLoopSourceRef);
	}
}



void EventListener::Callback (CFMachPortRef port, void *msg, CFIndex size, void *info)
{
	reinterpret_cast<EventListener*>(info)->HandleCallback (port, msg, size);
}



void EventListener::Initialize ()
{
	// create a callback information structure
	CFMachPortContext context = {1, this, NULL, NULL, NULL};
	
	// create the CFMachPort
	mMachPortRef = CFMachPortCreate (NULL, Callback, &context, NULL);
	if (mMachPortRef == NULL)
	{
		return;
	}
	
    // set the buffer limit for the port
	mach_port_t mp = CFMachPortGetPort (mMachPortRef);

    mach_port_limits_t limits;
    limits.mpl_qlimit = MACH_PORT_QLIMIT_MAX;
    kern_return_t result =
        mach_port_set_attributes (mach_task_self (), mp, MACH_PORT_LIMITS_INFO,
                                  mach_port_info_t (&limits), MACH_PORT_LIMITS_INFO_COUNT);

    if (result != KERN_SUCCESS)
    {
        secdebug ("notify", "Got error %d when trying to maximize queue size", result);
    }
    
	// make a run loop source for this ref
	mRunLoopSourceRef = CFMachPortCreateRunLoopSource (NULL, mMachPortRef, NULL);
	if (mRunLoopSourceRef == NULL)
	{
		CFRelease (mMachPortRef);
		return;
	}
	
	// attach this run loop source to the main run loop
	CFRunLoopAddSource (CFRunLoopGetCurrent (), mRunLoopSourceRef, kCFRunLoopDefaultMode);
	
	// extract the actual port from the run loop, and request callbacks on that port
	mClientSession.requestNotification (mp, Listener::databaseNotifications,
											Listener::allEvents);
}



void EventListener::HandleCallback (CFMachPortRef port, void *msg, CFIndex size)
{
	// we need to parse the message and see what happened
	mClientSession.dispatchNotification (reinterpret_cast<mach_msg_header_t *>(msg), ProcessMessage, this);
}



OSStatus EventListener::ProcessMessage (Listener::Domain domain, Listener::Event event, const void *data, size_t dataLength, void *context)
{
	reinterpret_cast<EventListener*>(context)->EventReceived (domain, event, data, dataLength);
	return noErr;
}



void EventListener::RequestEvents (Listener::Domain whichDomain, Listener::EventMask whichEvents)
{
	// stop the old event request and change to the new one
	mach_port_t mp = CFMachPortGetPort (mMachPortRef);
	mClientSession.stopNotification (mp);
	mClientSession.requestNotification (mp, whichDomain, whichEvents);
}



void EventListener::EventReceived (Listener::Domain domain, Listener::Event event, const void* data, size_t dataLength)
{
}



};
};
