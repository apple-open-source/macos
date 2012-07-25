#ifndef __MONITOR__
#define __MONITOR__




#include "Transform.h"



typedef CFTypeRef SecMonitorRef;


class Monitor : public Transform
{
public:
	virtual ~Monitor() { }
	Monitor(CFStringRef mName) : Transform(mName) {}
	virtual void Wait();
	bool IsExternalizable();
};



class BlockMonitor : public Monitor
{
protected:
	dispatch_queue_t mDispatchQueue;
	SecMessageBlock mBlock;
	bool mSeenFinal;

	virtual void AttributeChanged(CFStringRef name, CFTypeRef value);
    void LastValueSent();

	BlockMonitor(dispatch_queue_t queue, SecMessageBlock block);
	
public:
	virtual ~BlockMonitor();
	static CFTypeRef Make(dispatch_queue_t dispatch_queue, SecMessageBlock block);
};


#endif
