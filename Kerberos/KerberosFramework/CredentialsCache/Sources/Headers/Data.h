#pragma once

#include "Allocators.h"
#include "Implementations.h"

class CCISharedCCData:
	public Implementations::SharedData {
	public:
		CCISharedCCData (
			const cc_data&		inData);
		CCISharedCCData () {}
			
		typedef Implementations::Vector <unsigned char>::Shared::iterator	iterator;
		typedef Implementations::Vector <unsigned char>::Shared::const_iterator	const_iterator;
			
		CCIUInt32 GetType () const { return mType; }
		CCIUInt32 GetSize () const { return mSize; }
		
		iterator begin () { return mData.begin (); }
		iterator end () { return mData.end (); }
		const_iterator begin () const { return mData.begin (); }
		const_iterator end () const { return mData.end (); }
			
	private:
		CCIUInt32											mType;
		CCIUInt32											mSize;
		Implementations::Vector <unsigned char>::Shared		mData;

                friend void ReadData (std::istream& inStream, CCISharedCCData& ioData);
                friend void WriteData (std::ostream& ioStream, const CCISharedCCData& inData);
};


class CCISharedCCDataArray:
	public Implementations::SharedData {
	public:
		CCISharedCCDataArray (
			const cc_data* const*		inData);
		CCISharedCCDataArray () {}
			
		typedef Implementations::Vector <CCISharedCCData*>::Shared::iterator	iterator;
		typedef Implementations::Vector <CCISharedCCData*>::Shared::const_iterator	const_iterator;
			
		CCIUInt32 GetSize () const { return mItems.size (); }
			
		iterator begin () { return mItems.begin (); }
		iterator end () { return mItems.end (); }
			
		const_iterator begin () const { return mItems.begin (); }
		const_iterator end () const { return mItems.end (); }

	private:
		Implementations::Vector <CCISharedCCData*>::Shared	mItems;
                
                friend void ReadDataArray (std::istream& ioStream, CCISharedCCDataArray& ioArray); 
                friend void WriteDataArray (std::ostream& ioStream, const CCISharedCCDataArray& inDataArray);
 };


namespace CallImplementations {
	typedef ::CCISharedCCData CCISharedCCData;
	typedef ::CCISharedCCDataArray CCISharedCCDataArray;
}

namespace AEImplementations {
	typedef ::CCISharedCCData CCISharedCCData;
	typedef ::CCISharedCCDataArray CCISharedCCDataArray;
}

namespace MachIPCImplementations {
	typedef ::CCISharedCCData CCISharedCCData;
	typedef ::CCISharedCCDataArray CCISharedCCDataArray;
}
