#pragma once

//
// CCIMagic
//
// Template used to put a magic tag in an object; use this for objects
// exported by the API via opaque pointers, so that they can be verified
// when they are passed back into the API. Inherit from this template
// to use it
//

template <class T>
class CCIMagic {
	public:
	
		typedef CCIUInt32	Magic;
		
		CCIMagic ():
			mMagic (T::class_ID) {}
			
		// Invalidate magic on destruction
		~CCIMagic () { mMagic = 'Bam!'; }
		
		// Check if the magic is set correctly		
		void Validate () { if (mMagic != T::class_ID) throw T::invalidObject; }
		
	private:
		Magic	mMagic;

		// Disallowed
		CCIMagic (const CCIMagic&);
		CCIMagic& operator = (const CCIMagic&);
};
