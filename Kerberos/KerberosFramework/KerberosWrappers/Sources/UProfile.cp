/*
 * Utility classes for access to KerberosProfileLib
 */

#include <Kerberos/krb.h>
#include <Kerberos/profile.h>
#include <Kerberos/UProfile.h>
#include "ThrowUtils.h"

#pragma mark -

const char*	
UProfileOutputList::operator [] (
			u_int32_t	inIndex) {

#if defined (MACDEV_DEBUG) && MACDEV_DEBUG
	// Debugging version uses safe accessor
	for (u_int32_t i = 0; i < inIndex; i++) {
		Assert_ (Get () [i] != NULL);
	}
#endif

	return Get () [inIndex];
}

// List constructors
UProfileInputList::UProfileInputList (
	const char*		inItem1):
	mItemsPtr (&mItems [0])
{
	mItems [0] = inItem1;
	mItems [1] = NULL;
}

UProfileInputList::UProfileInputList (
	const char*		inItem1,
	const char*		inItem2):
	mItemsPtr (&mItems [0])
{
	mItems [0] = inItem1;
	mItems [1] = inItem2;
	mItems [2] = NULL;
}

UProfileInputList::UProfileInputList (
	const char*		inItem1,
	const char*		inItem2,
	const char*		inItem3):
	mItemsPtr (&mItems [0])
{
	mItems [0] = inItem1;
	mItems [1] = inItem2;
	mItems [2] = inItem3;
	mItems [3] = NULL;
}

UProfileInputList::UProfileInputList (
	const char*		inItem1,
	const char*		inItem2,
	const char*		inItem3,
	const char*		inItem4):
	mItemsPtr (&mItems [0])
{
	mItems [0] = inItem1;
	mItems [1] = inItem2;
	mItems [2] = inItem3;
	mItems [3] = inItem4;
	mItems [4] = NULL;
}

void
UProfileListAutoPtrDeleter::Delete (
	char**				inList)
{
	profile_free_list (inList);
}

#pragma mark -

// UProfile constructor calls krb_get_profile
UProfile::UProfile ():
	UProfileAutoPtr ()
{
	profile_t profile;
	int err = krb_get_profile (&profile);
	if (err == KFAILURE) {
		DebugThrow_ (std::runtime_error ("UProfile::UProfile: Kerberos error while opening the preferences file"));
	} else
		ThrowIfProfileError (err);
	
	Reset (profile);		
}

// Various getters and setters in the profile
bool
UProfile::GetBoolean (
	const	UProfileInputList&	inName,
			bool				inDefaultValue) const {
	
	int		value;
	long	profErr = profile_get_boolean (Get (), inName.Get ()[0], inName.Get ()[1], 
											inName.Get ()[2], inDefaultValue, &value);
	ThrowIfProfileError (profErr);
	
	return value;
}

void
UProfile::GetValues (
	const	UProfileInputList&	inName,
			UProfileOutputList&	outValues) const {
	
	char**	values;
	long	profErr = profile_get_values (Get (), inName.Get (), &values);
	ThrowIfProfileError (profErr);

	UProfileOutputList	result (values);	
	outValues = result;
}
	
void
UProfile::UpdateRelation (
	const	UProfileInputList&		inName,
	const	UProfileInputString&	inOldValue,
	const	UProfileInputString&	inNewValue) {
	long	profErr = profile_update_relation (Get (), inName.Get (), inOldValue.Get (), inNewValue.Get ());
	ThrowIfProfileError (profErr);
}
	
void
UProfile::AddRelation (
	const	UProfileInputList&		inName,
	const	UProfileInputString&	inValue) {
	long	profErr = profile_add_relation (Get (), inName.Get (), inValue.Get ());
	ThrowIfProfileError (profErr);
}

void
UProfile::ClearRelation (
	const	UProfileInputList&	inName) {
	long	profErr = profile_clear_relation (Get (), inName.Get ());
	ThrowIfProfileError (profErr);
}

UProfileIterator
UProfile::NewIterator (
	const	UProfileInputList&	inName,
			int					inFlags) const {
			
	void*		iterator;
	long	profErr = profile_iterator_create (Get (), inName.Get (), inFlags, &iterator);
	ThrowIfProfileError (profErr);
		
	return UProfileIterator (iterator);
}

void
UProfileAutoPtrDeleter::Delete (
	profile_t					inProfile)
{
	profile_release (inProfile);
}
			
#pragma mark -

// Iterator iteration function
bool
UProfileIterator::Next (
			UProfileOutputString&	outName,
			UProfileOutputString&	outValue) {
	char*	name = NULL;
	char*	value = NULL;
	
	// Stupid profile API takes a pointer to an iterator here, instead
	// the iterator itself, so I have to do a little dance.
	
	void*	myIterator = Release ();
	long	profErr = profile_iterator (&myIterator, &name, &value);
	ThrowIfProfileError (profErr);
		
	Reset (myIterator);
		
	if (name == NULL)
		return false;
	
	UProfileOutputString nameResult (name);
	outName = nameResult;

	UProfileOutputString valueResult (value);
	outValue = valueResult;
	
	return true;
}

void
UProfileIteratorAutoPtrDeleter::Delete (
	void*				inIterator)
{
	profile_iterator_free (&inIterator);
}

#pragma mark -

void
UProfileStringAutoPtrDeleter::Delete (
	const char*			inString)
{
	profile_release_string (const_cast <char*> (inString));
}
