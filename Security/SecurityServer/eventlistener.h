#ifndef _H_EVENTLISTENER
#define _H_EVENTLISTENER

#include <Security/ssclient.h>



#undef verify

namespace Security {
namespace SecurityServer {


class EventListener
{
protected:
	ClientSession mClientSession;
	CFMachPortRef mMachPortRef;
	CFRunLoopSourceRef mRunLoopSourceRef;

	static void Callback (CFMachPortRef port, void *msg, CFIndex size, void *info);
	static OSStatus ProcessMessage (Listener::Domain domain, Listener::Event event, const void *data, size_t dataLength, void *context);
	void HandleCallback (CFMachPortRef port, void *msg, CFIndex size);
	void HandleMessage ();
	void Initialize ();
	
public:
	EventListener (CssmAllocator &standard = CssmAllocator::standard(), CssmAllocator &returning = CssmAllocator::standard());
	virtual ~EventListener ();

	void RequestEvents (Listener::Domain domain, Listener::EventMask eventMask);
	virtual void EventReceived (Listener::Domain domain, Listener::Event event, const void* data, size_t dataLength);
};


}; // end namespace SecurityServer
}; // end namespace Security



#endif
