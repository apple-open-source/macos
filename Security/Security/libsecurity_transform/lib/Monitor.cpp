#include "Monitor.h"
#include <Block.h>
#include "misc.h"
#include "GroupTransform.h"
#include "Utilities.h"

void Monitor::Wait()
{
}



bool Monitor::IsExternalizable()
{
	return false; // monitors aren't really part of the transform
}

void BlockMonitor::AttributeChanged(CFStringRef name, CFTypeRef value)
{
	// deliver the attribute to the queue
	CFTypeRef realValue = value;
	CFErrorRef error = NULL;
	bool isFinal = false;
	
	if (mSeenFinal)
	{
		// A NULL and CFErrorRef might both be enqueued already, and the 2nd can race the teardown.   Without this check we would trigger final processing
		// more then once resulting in our own overlease issues, and could well cause our client to make similar errors.
		return;
	}

	if (realValue != NULL)
	{
		// do some basic checking
		if (CFGetTypeID(value) == CFErrorGetTypeID())
		{
			realValue = NULL;
			error = (CFErrorRef) value;
			isFinal = true;
		}
	}
	else
	{
		isFinal = true;
	}
	
	mSeenFinal = isFinal;
	
	if (realValue)
	{
		CFRetain(realValue);
	}	
	
	if (mDispatchQueue == NULL)
	{
		mBlock(realValue, error, isFinal);
	}
	else
	{
        // ^{ mBlock } gets referenced via this (no retain), localBlock gets owned by
        // the block passed to dispatch_async
        SecMessageBlock localBlock = mBlock;
        dispatch_async(mDispatchQueue, ^{
            localBlock(realValue, error, isFinal);
        });
	}
}



BlockMonitor::BlockMonitor(dispatch_queue_t queue, SecMessageBlock block) : Monitor(CFSTR("BlockMonitor")), mDispatchQueue(queue), mSeenFinal(FALSE)
{
    mBlock = ^(CFTypeRef value, CFErrorRef error, Boolean isFinal) {
		block(value, error, isFinal);
		if (value)
		{
			CFRelease(value);
		}
		if (isFinal && mGroup) {
            LastValueSent();
		}
	};
	mBlock = Block_copy(mBlock);
}

BlockMonitor::~BlockMonitor()
{
	Block_release(mBlock);
}

void BlockMonitor::LastValueSent()
{
    // The initial execute did a retain on our parent to keep it from
    // going out of scope.  Since this chain is now done, release it.
    // NOTE: this needs to be the last thing we do otherwise *this
    // can be deleted out from under us, leading to a crash most frequently
    // inside the block we dispatch_async, sometimes inside of mBlock.
    Transform *rootGroup = this->GetRootGroup();
    CFTypeRef rootGroupRef = rootGroup->GetCFObject();
    dispatch_async(rootGroup->mDispatchQueue, ^{
        CFRelease(rootGroupRef);
    });
}

CFTypeRef BlockMonitor::Make(dispatch_queue_t queue, SecMessageBlock block)
{
	return CoreFoundationHolder::MakeHolder(gInternalCFObjectName, new BlockMonitor(queue, block));
}
