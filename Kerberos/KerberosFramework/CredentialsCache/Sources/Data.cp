#include "Data.h"
#include "Allocators.h"

// Override operator new and operator delete on CFM platforms for shared memory
#if TARGET_RT_MAC_CFM
CCISharedDataAllocator <char>		CCISharedData::sAllocator;
#endif // TARGET_RT_MAC_CFM

CCISharedCCData::CCISharedCCData (
	const cc_data&		inData):
	mType (inData.type),
	mSize (inData.length),
	mData (mSize) {
	
	for (CCIUInt32	index = 0; index < mSize; index++) {
		mData [index] = static_cast <unsigned char*> (inData.data) [index];
	}
}

std::istream& operator >> (std::istream& inStream, CCISharedCCData& ioData) {

        inStream >> ioData.mType;
        inStream >> ioData.mSize;
        ioData.mData.resize (ioData.mSize);
        unsigned char* data = new unsigned char [ioData.mSize];
        for (CCIUInt32 i = 0; i < ioData.mSize; i++) {
            int c;
            inStream >> c;
            ioData.mData [i] = static_cast <unsigned char> (c);
        }
        
        delete data;
        
        return inStream;
}

CCISharedCCDataArray::CCISharedCCDataArray (
	const cc_data* const*		inData) {

	if (inData != NULL) {
		for (const cc_data* const*	item = inData; *item != NULL; item++) {
			mItems.push_back (new CCISharedCCData (**item));
		}
	}
};

std::istream& operator >> (std::istream& inStream, CCISharedCCDataArray& ioArray) {

        CCIUInt32	size;
        inStream >> size;
        ioArray.mItems.resize (size);
        for (CCIUInt32 i = 0; i < size; i++) {
            CCISharedCCData*	data = new CCISharedCCData ();
            inStream >> *data;
            ioArray.mItems [i] = data;
        }
        
        return inStream;
}

namespace CallImplementations {
	typedef ::CCISharedCCData CCISharedCCData;
	typedef ::CCISharedCCDataArray CCISharedCCDataArray;
}
