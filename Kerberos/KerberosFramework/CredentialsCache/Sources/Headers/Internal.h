#pragma once

//
// CCIInternal
//
// Class Internal should inherit from CCIInternal if Internal will
// be exported from the API via an opaque pointer. This class adds
// public data (visible to the clients of the API) and functions to
// internalize opaque pointer and externalize the internal class
//

template <class Internal, class Opaque>
class CCIInternal {
	public:
		typedef		CCIInternal <Internal, Opaque> CCIInternalType;

		// Initialize the public function table in constructor
		CCIInternal () {
			mPublicData.functions = &Internal::sFunctionTable;
		}
		
		~CCIInternal () {}
		
		// To internalize, we calculate the offset of the public data in the
		// private class, and then subtract that offset from the opaque pointer.
		// That yields the pointer to the internal structure, which we then validate.		
		static Internal* Internalize (
			Opaque*			inOpaque) {
			
			Internal* internal = 
				static_cast <Internal*> (
					reinterpret_cast <CCIInternalType*> (
						reinterpret_cast <char*> (inOpaque) -
							offsetof (CCIInternalType, mPublicData)));
					
			internal -> Validate ();
			return internal;
		}
		
		Opaque* Externalize () {
			return &mPublicData;
		}
		
		bool Valid () {
			return mPublicData.functions == &Internal::sFunctionTable;
		}
		
	protected:
		Opaque&		GetPublicData () { return mPublicData; }

	private:
		Opaque	mPublicData;
};

#if CCache_v2_compat
#include "CredentialsCache2.h"

// cred_union doesn't have function table, so we have to special-case this
// We can't make this a partial template specialization because of VC++ bugs
template <class Internal, class Opaque>
class CCIInternalWorkaround {
	public:
		typedef		CCIInternalWorkaround <Internal, Opaque> CCIInternalType;
		CCIInternalWorkaround () {
		}
		
		~CCIInternalWorkaround () {}
		
		static Internal* Internalize (
			Opaque*			inOpaque) {
			
			Internal* internal = 
				static_cast <Internal*> (
					reinterpret_cast <CCIInternalType*> (
						reinterpret_cast <char*> (inOpaque) -
							offsetof (CCIInternalType, mPublicData)));
					
			internal -> Validate ();
			return internal;
		}
		
		Opaque* Externalize () {
			return &mPublicData;
		}
		
		bool Valid () {
			return true;
		}
		
	protected:
		Opaque&		GetPublicData () { return mPublicData; }

	private:
		Opaque	mPublicData;
};
#endif