/*
 * Utility classes for access to KerberosProfileLib
 */
 
#ifndef UProfile_h
#define UProfile_h

#include <stdexcept>
#include <string>

#include <Kerberos/profile.h>
#include <Kerberos/UAutoPtr.h>

//#include <CoreServices/CoreServices.h>
/*
 * Deleter objects for various UAutoPtr incarnations
 */
 
class UProfileStringAutoPtrDeleter {
	public: static void Delete (const char* inString);
};

class UProfileAutoPtrDeleter {
	public: static void Delete (profile_t 	inString);
};

class UProfileIteratorAutoPtrDeleter {
	public: static void Delete (void* 		inString);
};

class UProfileListAutoPtrDeleter {
	public: static void Delete (char** 		inList);
};

typedef	UAutoPtr <char, UProfileStringAutoPtrDeleter>			UProfileOutputStringAutoPtr;
typedef	UAutoPtr <const char, UProfileStringAutoPtrDeleter>		UProfileInputStringAutoPtr;
typedef	UAutoPtr <struct _profile_t, UProfileAutoPtrDeleter>	UProfileAutoPtr;
typedef UAutoPtr <void, UProfileIteratorAutoPtrDeleter>			UProfileIteratorAutoPtr;
typedef UAutoPtr <char*, UProfileListAutoPtrDeleter>			UProfileListAutoPtr;

typedef	UProfileOutputStringAutoPtr::UAutoPtrRef	UProfileOutputStringAutoPtrRef;
typedef	UProfileInputStringAutoPtr::UAutoPtrRef		UProfileInputStringAutoPtrRef;
typedef	UProfileAutoPtr::UAutoPtrRef				UProfileAutoPtrRef;
typedef	UProfileIteratorAutoPtr::UAutoPtrRef		UProfileIteratorAutoPtrRef;
typedef	UProfileListAutoPtr::UAutoPtrRef			UProfileListAutoPtrRef;

/*
 * Most classes in this header file have an Input and Output
 * This is to deal with the distinction between const and non-const
 * profile types. It's possible this could be done properly within a
 * single class, but I haven't been able to make it work
 */
 
/*
 * Class pair representing strings taken and returned by the
 * profile library
 */
 
class UProfileInputString:
	public UProfileInputStringAutoPtr {
public:
		UProfileInputString ():
			UProfileInputStringAutoPtr () {}
		UProfileInputString (
			const char*				inString):
			UProfileInputStringAutoPtr (inString) {}
		UProfileInputString (
			UProfileInputStringAutoPtrRef		inReference):
			UProfileInputStringAutoPtr (inReference) {}
		~UProfileInputString () {}

		UProfileInputString&			operator = (
			UProfileInputStringAutoPtr::UAutoPtrRef	inOriginal) {
			Reset (inOriginal.mPtr);
			return *this;
		}
		
		UProfileInputString&			operator = (
			UProfileInputString&	inOriginal) {
			Reset (inOriginal.Release ());
			return *this;
		}
		
		UProfileInputString&			operator = (
			const char*	inOriginal) {
			Reset (inOriginal);
			return *this;
		}
};

class UProfileOutputString:
	public UProfileOutputStringAutoPtr {
public:
		UProfileOutputString ():
			UProfileOutputStringAutoPtr () {}
		UProfileOutputString (
			char*				inString):
			UProfileOutputStringAutoPtr (inString) {}
		UProfileOutputString (
			UProfileOutputStringAutoPtrRef		inReference):
			UProfileOutputStringAutoPtr (inReference) {}
		~UProfileOutputString () {}
		
		operator const UProfileInputString () { return UProfileInputString (Get ()); }

		UProfileOutputString&			operator = (
			UProfileOutputStringAutoPtr::UAutoPtrRef	inOriginal) {
			Reset (inOriginal.mPtr);
			return *this;
		}
		
		UProfileOutputString&			operator = (
			UProfileOutputString&	inOriginal) {
			Reset (inOriginal.Release ());
			return *this;
		}
		
		UProfileOutputString&			operator = (
			char*	inOriginal) {
			Reset (inOriginal);
			return *this;
		}
};

 
/*
 * Utility class for manipulating profile lib lists.
 * Knows to free the list when appropriate
 * Limited to 4 items (naive implementation)
 */
 
class UProfileInputList {
public:
	UProfileInputList ();
	UProfileInputList (
		const char*			inItem1);
	UProfileInputList (
		const char*			inItem1,
		const char*			inItem2);
	UProfileInputList (
		const char*			inItem1,
		const char*			inItem2,
		const char*			inItem3);
	UProfileInputList (
		const char*			inItem1,
		const char*			inItem2,
		const char*			inItem3,
		const char*			inItem4);
	UProfileInputList (
		char**				inList);
		
	const char** Get () const { return mItemsPtr; }

private:
	const char*				mItems [5];
	const char **	mItemsPtr;
};

class UProfileOutputList:
	public UProfileListAutoPtr {
public:
		UProfileOutputList ():
			UProfileListAutoPtr () {}
		UProfileOutputList (
			char**				inProfileList):
			UProfileListAutoPtr (inProfileList) {}
		UProfileOutputList (
			UProfileListAutoPtrRef		inReference):
			UProfileListAutoPtr (inReference) {}
		~UProfileOutputList () {}

		const char*	operator [] (u_int32_t	inIndex);
		
		operator	UProfileInputList () { return UProfileInputList (Get ()); }
		
		UProfileOutputList&			operator = (
			UProfileListAutoPtr::UAutoPtrRef	inOriginal) {
			Reset (inOriginal.mPtr);
			return *this;
		}
		
		UProfileOutputList&			operator = (
			UProfileOutputList&	inOriginal) {
			Reset (inOriginal.Release ());
			return *this;
		}
		
		UProfileOutputList&			operator = (
			char**	inOriginal) {
			Reset (inOriginal);
			return *this;
		}
		
};

/*
 * Utility class for using profile lib iterators.
 */
 
class UProfileIterator:
	public UProfileIteratorAutoPtr {
public:
		UProfileIterator ():
			UProfileIteratorAutoPtr () {}
		UProfileIterator (
			void*				inProfileHandle):
			UProfileIteratorAutoPtr (inProfileHandle) {}
		UProfileIterator (
			UProfileIteratorAutoPtrRef		inReference):
			UProfileIteratorAutoPtr (inReference) {}
		~UProfileIterator () {}
		
		UProfileIterator&			operator = (
			UProfileIteratorAutoPtr::UAutoPtrRef	inOriginal) {
			Reset (inOriginal.mPtr);
			return *this;
		}
		
		UProfileIterator&			operator = (
			UProfileIterator&	inOriginal) {
			Reset (inOriginal.Release ());
			return *this;
		}
		
		UProfileIterator&			operator = (
			void*	inOriginal) {
			Reset (inOriginal);
			return *this;
		}
		
	bool
		Next (
					UProfileOutputString&	outName,
					UProfileOutputString&	outValue);
};

/*
 * Utility class for manipulating profile lib profile.
 */
 
class UProfile:
	public UProfileAutoPtr {
public:
		UProfile ();
		UProfile (
			profile_t				inProfileHandle):
			UProfileAutoPtr (inProfileHandle) {}
		UProfile (
			UProfileAutoPtrRef		inReference):
			UProfileAutoPtr (inReference) {}
		~UProfile () {}
		
		UProfile&			operator = (
			UProfileAutoPtr::UAutoPtrRef	inOriginal) {
			Reset (inOriginal.mPtr);
			return *this;
		}
		
		UProfile&			operator = (
			UProfile&	inOriginal) {
			Reset (inOriginal.Release ());
			return *this;
		}
		
		UProfile&			operator = (
			profile_t	inOriginal) {
			Reset (inOriginal);
			return *this;
		}
		
	bool
		GetBoolean (
			const	UProfileInputList&	inName,
			bool				inDefaultValue) const;

	void
		GetValues (
			const	UProfileInputList&	inName,
					UProfileOutputList&	outValues) const;
			
	void
		UpdateRelation (
			const	UProfileInputList&		inName,
			const	UProfileInputString&	inOldValue,
			const	UProfileInputString&	inNewValue);
			
	void
		AddRelation (
			const	UProfileInputList&		inName,
			const	UProfileInputString&	inValue);

	void
		ClearRelation (
			const	UProfileInputList&	inName);
			
	UProfileIterator
		NewIterator (
			const	UProfileInputList&	inName,
					int				inFlags) const;


	class StProfileChanger {
	public:
			StProfileChanger (
				UProfile&			inProfile):
				mProfile (inProfile),
				mWrite (true) {}
			~StProfileChanger () { if (mWrite) Flush (); }
		
		void
			Abandon () { mWrite = false; }		
		
		void
			Flush () { profile_flush (mProfile.Get ()); }
			
	private:
		UProfile&		mProfile;
		bool			mWrite;
		
		// Disallowed
		StProfileChanger ();
		StProfileChanger (const StProfileChanger&);
		StProfileChanger& operator = (const StProfileChanger&);
		
	};
};

/*
 * Exception class thrown by UProfile classes
 */
 
class UProfileRuntimeError:
	public std::runtime_error {
public:
	explicit UProfileRuntimeError (
		long		inError):
		std::runtime_error ("UProfileRuntimeError"),
		mError (inError) {}
	
	long Error () const { return mError; }
private:
	long			mError;
};

/* Configuration file is well-formed, but something is missing */
class UProfileConfigurationError:
	public UProfileRuntimeError {
public:
	explicit UProfileConfigurationError (
		long		inError):
		UProfileRuntimeError (inError) {}
};

/* Configuration file is not well-formed */
class UProfileSyntaxError:
	public UProfileRuntimeError {
public:
	explicit UProfileSyntaxError (
		long		inError):
		UProfileRuntimeError (inError) {}
};

class UProfileLogicError:
	public std::logic_error {
public:
	explicit UProfileLogicError (
		long		inError):
		std::logic_error ("UProfileLogicError"),
		mError (inError) {}
	
	long Error () const { return mError; }
private:
	long			mError;
};

#endif /* UProfile_h */

