#include "HandleBuffer.h"

// Initialize the buffer
CCIHandleBuffer::CCIHandleBuffer ():
	mHandle (NULL),
	mSize (0),
	mOffset (0)
{
}

// Destroy the buffer and dispose the handle
CCIHandleBuffer::~CCIHandleBuffer ()
{
	DisposeHandle ();
}

// Reset the buffer to the beginning
void CCIHandleBuffer::Reset ()
{
	mOffset = 0;
}

// Read generic data
void CCIHandleBuffer::GetData (
	void*		ioBuffer,
	CCIUInt32	inSize) const
{
	if (mOffset + inSize > mSize) {
		CCIDebugThrow_ (CCIException (ccErrBadParam));
	}

	BlockMoveData (const_cast <char*> (reinterpret_cast <volatile char*> (*mHandle) + mOffset),
		ioBuffer, static_cast <Size> (inSize));
	
	mOffset += inSize;
}

// Read various types
void CCIHandleBuffer::Get (
	CCIUniqueID&			outData) const
{
	GetData (&outData, sizeof (outData));
}

void CCIHandleBuffer::Get (
	CCITime&			outData) const
{
	GetData (&outData, sizeof (outData));
}

void CCIHandleBuffer::Get (
	CCIResult&			outData) const
{
	GetData (&outData, sizeof (outData));
}

void CCIHandleBuffer::Get (
	bool&				outData) const
{
	CCIUInt32		value;
	Get (value);
	
	outData = value == 1;
}

void CCIHandleBuffer::Get (
	std::string&			outData) const
{
	// Read 4-byte length
	CCIUInt32	length;
	Get (length);

	// Allocate
	char*		string = new char [length];

	try {
		// Read data (suboptimal -- we copy the data twice!)
		GetData (string, length);
		outData = std::string (string, length);
		delete [] string;
	} catch (...) {
		delete [] string;
		throw;
	}
}

void CCIHandleBuffer::Get (
	std::strstream&			outData) const
{
	// Read 4-byte length
	CCIUInt32	length;
	Get (length);

	// Allocate the data
	char*		data = new char [length];

	try {
		// Shove the data on the stream (suboptimal -- data is copied twice)
		GetData (data, length);
		outData << data << std::ends;
		delete [] data;
	} catch (...) {
		delete [] data;
		throw;
	}
}

void CCIHandleBuffer::Get (
	std::vector <CCIObjectID>&			outData) const
{
	// Read 4-byte length
	CCIUInt32	length;
	Get (length);
	
	// Read each of the elements
	outData.clear ();
	for (CCIUInt32 i = 0; i < length; i++) {
		CCIObjectID		id;
		Get (id);
		outData.push_back (id);
	}
}

// Put generic data
void CCIHandleBuffer::PutData (
	const void*							inData,
	CCIUInt32							inSize)
{
	// Handle is not big enough
	if (mOffset + inSize > mSize) {
		OSErr err = noErr;

		if (mHandle != NULL) {
			// Resize the handle if it's already been allocated
			SetHandleSize (mHandle, static_cast <Size> (mOffset + inSize));
			err = MemError ();
		} else {
			// Allocate a new handle if necessary
			mHandle = NewHandle (static_cast <Size> (mOffset + inSize));
			if (mHandle == NULL) {
				err = MemError ();
				if (err == noErr) {
					err = memFullErr;
				}
			}
		}
		// Check that we succeeded
		if ((err != noErr) || (GetHandleSize (mHandle) != static_cast <Size> (mOffset + inSize))) {
			CCIDebugThrow_ (CCIException (ccErrNoMem));
		}
		mSize = mOffset + inSize;
	}
	// Shovel the data over to the handle
	BlockMoveData (inData, reinterpret_cast <char*> (*mHandle) + mOffset,
		static_cast <Size> (inSize));
	
	// Move the offset to the end
	mOffset += inSize;
}

// Put various types
void CCIHandleBuffer::Put (
	const CCIUniqueID&		inData)
{
	PutData (&inData, sizeof (inData));
}

void CCIHandleBuffer::Put (
	CCITime		inData)
{
	PutData (&inData, sizeof (inData));
}

#if TARGET_RT_MAC_MACHO
// Currently: mach_msg_type_number_t == CCITime
//void CCIHandleBuffer::Put (
//	mach_msg_type_number_t		inData)
//{
//	PutData (&inData, sizeof (inData));
//}
#endif

void CCIHandleBuffer::Put (
	CCIResult				inData)
{
	PutData (&inData, sizeof (inData));
}

void CCIHandleBuffer::Put (
	const std::string&		inData)
{
	// 4-byte length + string data
	CCIUInt32	length = inData.length (); // NUL
	Put (length);
	PutData (inData.c_str (), length);
}

void CCIHandleBuffer::Put (
	std::strstream&		inData)
{
	// 4-byte length + stream data
	CCIUInt32	length = static_cast <CCIUInt32> (inData.pcount ());
	Put (length);
	PutData (inData.str (), length);
}

void CCIHandleBuffer::Put (
	bool					inData)
{
	CCIUInt32	data = inData;
	Put (data);
}

void CCIHandleBuffer::Put (
	const std::vector <CCIObjectID>&	inData)
{
	// 4-byte count + elements in order
	CCIUInt32	length = inData.size ();
	Put (length);
	
	for (CCIUInt32 i = 0; i < length; i++) {
		Put (inData [i]);
	}
}

// Tell the buffer to use a new handle
void CCIHandleBuffer::AdoptHandle (
	Handle		inHandle)
{
	// If the new handle is different from the current handle, dispose the current handle
	if ((mHandle != NULL) && (mHandle != inHandle)) {
		::DisposeHandle (mHandle);
	}
	
	// Use the new handle
	mHandle = inHandle;
	mSize = static_cast <CCIUInt32> (GetHandleSize (inHandle));
	mOffset = 0;
}

// Retrieve the current data handle
Handle CCIHandleBuffer::GetHandle () const
{
	return mHandle;
}

// Dispose the current data handle
void CCIHandleBuffer::DisposeHandle ()
{
	if (mHandle != NULL) {
		::DisposeHandle (mHandle);
	}
	
	mSize = 0;
	mHandle = NULL;
	mOffset = 0;
}

// Release the data handle to the caller (without disposing it)
void CCIHandleBuffer::ReleaseHandle ()
{
	mHandle = NULL;
	mSize = 0;
	mOffset = 0;
}

// Move the offset
void CCIHandleBuffer::SetOffset (
	CCIUInt32		inOffset)
{
	mOffset = inOffset;
}

// Update internal state if the handle has been modified externally
void CCIHandleBuffer::UpdateSize ()
{
	if (mHandle == NULL) {
		mSize = 0;
	} else {
		mSize = static_cast <CCIUInt32> (GetHandleSize (mHandle));
	}
}
