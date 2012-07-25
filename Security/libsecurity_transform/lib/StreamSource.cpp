#include "StreamSource.h"
#include <string>
#include "misc.h"

using namespace std;

CFStringRef gStreamSourceName = CFSTR("StreamSource");

const CFIndex kMaximumSize = 2048;

StreamSource::StreamSource(CFReadStreamRef input, Transform* transform, CFStringRef name)
	: Source(gStreamSourceName, transform, name),
	mReadStream(input),
	mReading(dispatch_group_create())
{
	dispatch_group_enter(mReading);
	CFRetain(mReadStream);
}

void StreamSource::BackgroundActivate()
{
	CFIndex result = 0;
	
	do
	{
		// make a buffer big enough to handle the object
		// NOTE: allocating this on the stack and letting CFDataCreate copy it is _faster_ then malloc and CFDataCreateWithBytes(..., kCFAllactorMalloc) by a fair margin.   At least for 2K chunks.   Retest if changing the size.
		UInt8 buffer[kMaximumSize];
		
		result = CFReadStreamRead(mReadStream, buffer, kMaximumSize);
		
		if (result > 0) // was data returned?
		{
			// make the data and send it to the transform
			CFDataRef data = CFDataCreate(NULL, buffer, result);

			CFErrorRef error = mDestination->SetAttribute(mDestinationName, data);
			
			CFRelease(data);

			if (error != NULL) // we have a problem, there was probably an abort on the chain
			{
				return; // quiesce the source
			}
		}
	} while (result > 0);
	
	if (result < 0)
	{
		// we got an error!
		CFErrorRef error = CFReadStreamCopyError(mReadStream);
		mDestination->SetAttribute(mDestinationName, error);
		if (error)
		{
			// NOTE: CF doesn't always tell us about this error.   Arguably it could be better to
			// "invent" a generic error, but it is a hard argument that we want to crash in CFRelease(NULL)...
			CFRelease(error);
		}
	}
	else
	{
		// send an EOS
		mDestination->SetAttribute(mDestinationName, NULL); // end of stream
	}
}

void StreamSource::DoActivate()
{
	CFRetain(mDestination->GetCFObject());
	dispatch_group_async(mReading, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_LOW, 0), ^{ 
		this->BackgroundActivate();
		CFRelease(mDestination->GetCFObject());
	});
	dispatch_group_leave(mReading);
}

void StreamSource::Finalize()
{
	dispatch_group_notify(mReading, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
		delete this;
	});
}

StreamSource::~StreamSource()
{
	CFRelease(mReadStream);
	mReadStream = NULL;
	dispatch_release(mReading);
	mReading = NULL;
}


Boolean StreamSource::Equal(const CoreFoundationObject* object)
{
	// not equal if we are not the same object
	if (Source::Equal(object))
	{
		const StreamSource* ss = (StreamSource*) object;
		return CFEqual(ss->mReadStream, mReadStream);
	}

	return false;
}



CFTypeRef StreamSource::Make(CFReadStreamRef input, Transform* transform, CFStringRef name)
{
	return CoreFoundationHolder::MakeHolder(gInternalCFObjectName, new StreamSource(input, transform, name));
}



string StreamSource::DebugDescription()
{
	string result = Source::DebugDescription() + ": Stream ";
	
	char buffer[256];
	snprintf(buffer, sizeof(buffer), "(mReadStream = %p)", mReadStream);
	
	result += buffer;
	
	return result;
}
