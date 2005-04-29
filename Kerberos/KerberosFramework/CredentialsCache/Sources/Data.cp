#include "Data.h"
#include "Allocators.h"
#include "FlattenCredentials.h"

// Override operator new and operator delete on CFM platforms for shared memory
#if TARGET_RT_MAC_CFM
CCISharedDataAllocator <char> CCISharedData::sAllocator;
#endif // TARGET_RT_MAC_CFM

CCISharedCCData::CCISharedCCData (const cc_data& inData) : 
    mType (inData.type), mSize (inData.length), mData (mSize) 
{	
    for (CCIUInt32 index = 0; index < mSize; index++) {
        mData [index] = static_cast <unsigned char*> (inData.data) [index];
    }
}

void ReadData (std::istream& inStream, CCISharedCCData& ioData)
{
    //dprintf ("Entering %s():", __FUNCTION__);

    cc_data tempData = {0, 0, NULL};
    unsigned char *buffer = NULL;
    
    try {
        ReadData (inStream, tempData);
        ioData.mType = tempData.type;
        ioData.mSize = tempData.length;
        buffer = static_cast<unsigned char *> (tempData.data);
        //dprintf ("Read type %d, length %d", ioData.mType, ioData.mSize);
        
        ioData.mData.resize (ioData.mSize);
        for (CCIUInt32 i = 0; i < ioData.mSize; i++) {
            ioData.mData [i] = buffer [i];
        }
        delete [] buffer;
    } catch (...) {
        if (tempData.data != NULL) { delete [] buffer; }
        throw;
    }
    //dprintf ("Read buffer");
}

// Send a data blob to a stream
void WriteData (std::ostream& ioStream, const CCISharedCCData& inData)
{
    //dprintf ("Entering %s(CCISharedCCData):", __FUNCTION__);

    cc_data tempData = {inData.GetType (), inData.GetSize (), NULL };
    unsigned char *buffer = NULL;
    
    try {
        buffer = new unsigned char [tempData.length];
        Implementations::CCISharedCCData::const_iterator iterator = inData.begin ();
        for (CCIUInt32 i = 0; (iterator < inData.end ()) && (i < tempData.length); iterator++, i++) {
            buffer [i] = *iterator;
        }
        tempData.data = buffer;
        WriteData (ioStream, tempData);
        
        delete [] buffer;
    } catch (...) {
        if (tempData.data != NULL) { delete [] buffer; }
        throw;
    }
    //dprintf ("Wrote buffer");
}

CCISharedCCDataArray::CCISharedCCDataArray (const cc_data* const* inData) 
{
    if (inData != NULL) {
        for (const cc_data* const* item = inData; *item != NULL; item++) {
            mItems.push_back (new CCISharedCCData (**item));
        }
    }
}

void ReadDataArray (std::istream& ioStream, CCISharedCCDataArray& ioArray) 
{
    // We would like to not duplicate code here but it would be too horrible
    CCIUInt32 size;
    ReadUInt32 (ioStream, size);
    ioArray.mItems.resize (size);
    for (CCIUInt32 i = 0; i < size; i++) {
        CCISharedCCData* data = new CCISharedCCData ();
        ReadData (ioStream, *data);
        ioArray.mItems [i] = data;
    }
}

// Send a data blob array to a stream
void WriteDataArray (std::ostream& ioStream, const CCISharedCCDataArray& inDataArray)
{
    // We would like to not duplicate code here but it would be too horrible
    WriteUInt32 (ioStream, inDataArray.GetSize ());
    CCISharedCCDataArray::const_iterator iterator = inDataArray.begin ();
    for (; iterator < inDataArray.end (); iterator++) {
        WriteData (ioStream, **iterator);
    }
}


namespace CallImplementations {
	typedef ::CCISharedCCData CCISharedCCData;
	typedef ::CCISharedCCDataArray CCISharedCCDataArray;
}
