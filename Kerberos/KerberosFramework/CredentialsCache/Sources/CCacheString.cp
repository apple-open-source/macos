#include "CCacheString.h"
#include "Pointer.h"

const	cc_string_f	CCIString::sFunctionTable = {
	CCEString::Release
};

// Release a string
cc_int32 CCEString::Release (
	cc_string_t		inString) {
	
	CCIResult	result = ccNoError;
	
	CCIBeginSafeTry_ {
		StString	string (inString);
		
		delete string.Get ();
	} CCIEndSafeTry_ (result, ccErrBadParam)
	
	CCIAssert_ ((result == ccNoError)
	            || (result == ccErrInvalidString));
	
	return result;
}

#pragma mark -
// Create a string
CCIString::CCIString (
	const std::string&			inString) {
	mString = inString;
	GetPublicData ().data = mString.c_str ();
}

// Delete a string
CCIString::~CCIString () {
}

// Check integrity of a string
void CCIString::Validate () {

	CCIMagic <CCIString>::Validate ();
	CCIAssert_ ((CCIInternal <CCIString, cc_string_d>::Valid ()));
}

