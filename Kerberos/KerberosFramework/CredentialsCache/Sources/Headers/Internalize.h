#pragma once

//
// StInternalize
//
// Template class which converts from an opaque pointer (used by the API)
// to an internal class (used by the implementation)
//

template <class Internal, class Opaque>
class StInternalize {
	public:
		// Convert an opaque pointer to the internal representation
		// using the Internalize method which must be provided
		// by the internal class
		StInternalize (
			Opaque*		inOpaque):
			mInternal (Internal::Internalize (inOpaque)) {}

		StInternalize (
			Internal*	inInternal):
			mInternal (inInternal) {}
	
		Internal* operator -> () { return mInternal; }
		operator Opaque* () { return mInternal -> Externalize (); }
		
		Internal* Get () const { return mInternal; }
	
	private:
		Internal*		mInternal;
};
