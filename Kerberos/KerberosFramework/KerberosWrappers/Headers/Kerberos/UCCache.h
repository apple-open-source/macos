/*
 * Utility classes for access to CCache API
 */
 
#ifndef UCCache_h_
#define UCCache_h_

#include <memory>
#include <stdexcept>

#include <Kerberos/CredentialsCache.h>
#include <Kerberos/krb.h>
#include <Kerberos/UAutoPtr.h>
#include <Kerberos/UPrincipal.h>

//#include <CoreServices/CoreServices.h>

/*
 * UAutoPtr needs to know how to delete objects from CCache API. Fortunately, CCache API
 * is nice enough to let us solve that problem with a simple template:
 */

template <class T>
class CCacheAutoPtrDeleter {
	public:
		static void Delete (T* inPointer) {
			inPointer -> functions -> release (inPointer);
		}
};

/* Typedefs for conciseness */

typedef	UAutoPtr <cc_credentials_d, CCacheAutoPtrDeleter <cc_credentials_d> >
				UCredentialsAutoPtr;
typedef	UAutoPtr <cc_ccache_d, CCacheAutoPtrDeleter <cc_ccache_d> >									
				UCCacheAutoPtr;
typedef	UAutoPtr <cc_credentials_iterator_d, CCacheAutoPtrDeleter <cc_credentials_iterator_d> >		
				UCredentialsIteratorAutoPtr;
typedef	UAutoPtr <cc_ccache_iterator_d, CCacheAutoPtrDeleter <cc_ccache_iterator_d> >				
				UCCacheIteratorAutoPtr;
typedef	UAutoPtr <cc_context_d, CCacheAutoPtrDeleter <cc_context_d> >								
				UCCacheContextAutoPtr;
typedef UAutoPtr <cc_string_d, CCacheAutoPtrDeleter <cc_string_d> >
				UCCacheStringAutoPtr;

typedef	UCredentialsAutoPtr::UAutoPtrRef					UCredentialsAutoPtrRef;
typedef	UCCacheAutoPtr::UAutoPtrRef							UCCacheAutoPtrRef;
//typedef	UCredentialsIteratorAutoPtr::UAutoPtrRef			UCredentialsIteratorAutoPtrRef;
typedef	UCCacheIteratorAutoPtr::UAutoPtrRef					UCCacheIteratorAutoPtrRef;
typedef	UCCacheContextAutoPtr::UAutoPtrRef					UCCacheContextAutoPtrRef;
typedef	UCCacheStringAutoPtr::UAutoPtrRef					UCCacheStringAutoPtrRef;

/*
 * Utility class for manipulating strings returned by the CCache API
 */

#pragma mark ¥ UCCacheString
 
class UCCacheString:
	public UCCacheStringAutoPtr {
public:
		UCCacheString ():
			UCCacheStringAutoPtr () {}
		UCCacheString (
			cc_string_t				inString):
			UCCacheStringAutoPtr (inString) {}
		UCCacheString (
			UCCacheStringAutoPtrRef	inReference):
			UCCacheStringAutoPtr (inReference) {}
		UCCacheString (
			UCCacheString&					inOriginal):
			UCCacheStringAutoPtr (inOriginal) {}
		~UCCacheString () {}
		
		UCCacheString&			operator = (
			UCCacheStringAutoPtr::UAutoPtrRef	inOriginal) {
			Reset (inOriginal.mPtr);
			return *this;
		}
		
		UCCacheString&			operator = (
			UCCacheString&	inOriginal) {
			Reset (inOriginal.Release ());
			return *this;
		}
		
		UCCacheString&			operator = (
			cc_string_t	inOriginal) {
			Reset (inOriginal);
			return *this;
		}
		
	const char*
		CString () const { return Get () -> data; }
};
		
 


/*
 * Utility class for manipulating credentials.
 */
 
#pragma mark ¥ UCredentials
 
class UCredentials:
	public UCredentialsAutoPtr {
public:
		UCredentials ():
			UCredentialsAutoPtr () {}
		UCredentials (
			cc_credentials_t				inCredentials):
			UCredentialsAutoPtr (inCredentials) {}
		UCredentials (
			UCredentialsAutoPtrRef	inReference):
			UCredentialsAutoPtr (inReference) {}
		UCredentials (
			UCredentials&					inOriginal):
			UCredentialsAutoPtr (inOriginal) {}
		~UCredentials () {}
		
		UCredentials&			operator = (
			UCredentialsAutoPtr::UAutoPtrRef	inOriginal) {
			Reset (inOriginal.mPtr);
			return *this;
		}
		
		UCredentials&			operator = (
			UCredentials&	inOriginal) {
			Reset (inOriginal.Release ());
			return *this;
		}
		
		UCredentials&			operator = (
			cc_credentials_t	inOriginal) {
			Reset (inOriginal);
			return *this;
		}
		
	void
		CopyToV4Credentials (
			CREDENTIALS&					outCredentials) const;
			
	void
        CopyToV5Credentials (
            krb5_creds&						outCredentials) const;
    
    UPrincipal
		GetServicePrincipal () const;
				
	UPrincipal
		GetClientPrincipal () const;
		
	des_cblock&
		GetV4SessionKey (
			des_cblock&			outSessionKey) const;
		
	cc_credentials_v4_t*
		GetV4Credentials () const;
		
	cc_credentials_v5_t*
		GetV5Credentials () const;
		
	UPrincipal::EVersion
		GetVersion () const;
		
	bool 
		operator == (
			const UCredentials& inCompareTo);

	static void V4CredentialsToCCacheCredentials (
			const CREDENTIALS&				inCredentials,
			cc_credentials_union&			outCredentialsUnion,
			cc_credentials_v4_t&			outCredentialsV4);

	static void V5CredentialsToCCacheCredentials (
			const krb5_creds&				inCredentials,
			cc_credentials_union&			outCredentialsUnion,
			cc_credentials_v4_t&			outCredentialsV4);

};

/*
 * Stack-based class for creating credentials unions from krb4 and krb5 credentials.
 */

class StCredentialsUnion {
public:
	StCredentialsUnion (
		const CREDENTIALS&		inCredentials);
	
	StCredentialsUnion (
		const krb5_creds&		inCredentials);
	
	~StCredentialsUnion ();

	const cc_credentials_union* 
		Get () const { return &mCredentialsUnion; }
	
private:
	cc_credentials_union	mCredentialsUnion;
	cc_credentials_v5_t		mV5Creds;
	cc_credentials_v4_t		mV4Creds;
};

/*
 * Utility class for manipulating credentials iterators.
 */
 
#pragma mark ¥ UCredentialsIterator
 
class UCredentialsIterator:
	public UCredentialsIteratorAutoPtr {
public:
		struct UCredentialsIteratorAutoPtrRef {
			cc_credentials_iterator_t		mPtr;
			UPrincipal::EVersion	mVersion;
		};
		
		UCredentialsIterator (
			UPrincipal::EVersion		inVersion = UPrincipal::kerberosV4And5):
			UCredentialsIteratorAutoPtr (),
			mVersion (inVersion) {}
		UCredentialsIterator (
			cc_credentials_iterator_t	inCredentialsIterator,
			UPrincipal::EVersion		inVersion = UPrincipal::kerberosV4And5):
			UCredentialsIteratorAutoPtr (inCredentialsIterator),
			mVersion (inVersion) {}
		UCredentialsIterator (
			UCredentialsIterator&		inOriginal):
			UCredentialsIteratorAutoPtr (inOriginal),
			mVersion (inOriginal.mVersion) {}
		UCredentialsIterator (
			UCredentialsIteratorAutoPtrRef	inReference):
			UCredentialsIteratorAutoPtr (inReference.mPtr),
			mVersion (inReference.mVersion) {}
		~UCredentialsIterator () {}
		
		UCredentialsIterator&			operator = (
			UCredentialsIteratorAutoPtrRef	inOriginal) {
			Reset (inOriginal.mPtr);
			mVersion = inOriginal.mVersion;
			return *this;
		}
		
		operator UCredentialsIteratorAutoPtrRef () {
			UCredentialsIteratorAutoPtrRef	ref;
			ref.mPtr = Release ();
			ref.mVersion = mVersion;
			return ref;
		}
		
		UCredentialsIterator&			operator = (
			UCredentialsIterator&	inOriginal) {
			Reset (inOriginal.Release ());
			return *this;
		}
		
		UCredentialsIterator&			operator = (
			cc_credentials_iterator_t	inOriginal) {
			Reset (inOriginal);
			return *this;
		}
		
	bool
		Next (
			UCredentials&				ioCredentials);
			
private:
	UPrincipal::EVersion		mVersion;
};

/*
 * Utility class for manipulating ccaches.
 */
 
#pragma mark ¥ UCCache
 
class UCCache:
	public UCCacheAutoPtr {
public:
		explicit UCCache (
			cc_ccache_t				inCCache = 0):
			UCCacheAutoPtr (inCCache) {}
		UCCache (
			UCCache&				inOriginal):
			UCCacheAutoPtr (inOriginal) {}
		UCCache (
			UCCacheAutoPtrRef	inReference):
			UCCacheAutoPtr (inReference) {}
		~UCCache () {}
		
		UCCache&			operator = (
			UCCacheAutoPtr::UAutoPtrRef	inOriginal) {
			Reset (inOriginal.mPtr);
			return *this;
		}
		
		UCCache&			operator = (
			UCCache&	inOriginal) {
			Reset (inOriginal.Release ());
			return *this;
		}
		
		UCCache&			operator = (
			cc_ccache_t	inOriginal) {
			Reset (inOriginal);
			return *this;
		}
		
	void
		Destroy ();
	
	void
		MoveTo (
			UCCache&						destination);
	
	void
		SetDefault ();
		
	UPrincipal::EVersion
		GetCredentialsVersion () const;
		
	UCCacheString
		GetName () const;
		
	UPrincipal
		GetPrincipal (
			UPrincipal::EVersion			inVersion) const;
		
	void
		SetPrincipal (
			UPrincipal::EVersion			inVersion,
			const char*						inPrincipal);
			
	void
		SetPrincipal (
			UPrincipal::EVersion			inVersion,
			UPrincipal&						inPrincipal);
			
	void
		StoreCredentials (
			const cc_credentials_union*		inCredentials);
		
	void
		StoreCredentials (
			const CREDENTIALS*				inCredentials);
	
	void
		StoreCredentials (
			const krb5_creds*				inCredentials);
	
	void
		RemoveCredentials (
			UCredentials&					inCredentials);
		
	UCredentialsIterator
		NewCredentialsIterator (
			UPrincipal::EVersion			inVersion = UPrincipal::kerberosV4And5) const;
		
	UCredentials
		GetCredentialsForService (
			const UPrincipal&				inService,
			UPrincipal::EVersion			inVersion) const;

	void
		DeleteCredentialsForService (
			const UPrincipal&				inService,
			UPrincipal::EVersion			inVersion);

	bool 
		operator == (
			const UCCache& inCompareTo);
};

/*
 * Utility class for manipulating ccache iterators.
 */
 
#pragma mark ¥ UCCacheIterator
 
class UCCacheIterator:
	public UCCacheIteratorAutoPtr {
public:
		UCCacheIterator ():
			UCCacheIteratorAutoPtr () {}
		UCCacheIterator (
			cc_ccache_iterator_t			inCCacheIterator):
			UCCacheIteratorAutoPtr (inCCacheIterator) {}
		UCCacheIterator (
			UCCacheIterator&				inOriginal):
			UCCacheIteratorAutoPtr (inOriginal) {}
		UCCacheIterator (
			UCCacheIteratorAutoPtrRef	inReference):
			UCCacheIteratorAutoPtr (inReference) {}
		~UCCacheIterator () {}
		
		UCCacheIterator&			operator = (
			UCCacheIteratorAutoPtr::UAutoPtrRef	inOriginal) {
			Reset (inOriginal.mPtr);
			return *this;
		}
		
		UCCacheIterator&			operator = (
			UCCacheIterator&	inOriginal) {
			Reset (inOriginal.Release ());
			return *this;
		}
		
		UCCacheIterator&			operator = (
			cc_ccache_iterator_t	inOriginal) {
			Reset (inOriginal);
			return *this;
		}
		
	bool
		Next (
			UCCache&				ioCCache);
		
};

/*
 * Utility class for manipulating ccache contexts.
 */
 
#pragma mark ¥ UCCacheContext
 
class UCCacheContext:
	public UCCacheContextAutoPtr {
public:
		UCCacheContext ();
		UCCacheContext (
			cc_context_t						inCCacheContext):
			UCCacheContextAutoPtr (inCCacheContext) {}
		UCCacheContext (
			UCCacheContext&						inOriginal):
			UCCacheContextAutoPtr (inOriginal) {}
		UCCacheContext (
			UCCacheContextAutoPtrRef	inReference):
			UCCacheContextAutoPtr (inReference) {}
		~UCCacheContext () {}
	
		UCCacheContext&			operator = (
			UCCacheContextAutoPtr::UAutoPtrRef	inOriginal) {
			Reset (inOriginal.mPtr);
			return *this;
		}
		
		UCCacheContext&			operator = (
			UCCacheContext&	inOriginal) {
			Reset (inOriginal.Release ());
			return *this;
		}
		
		UCCacheContext&			operator = (
			cc_context_t	inOriginal) {
			Reset (inOriginal);
			return *this;
		}
		
	cc_time_t
		GetChangeTime ();
		
	UCCacheString
		GetDefaultCCacheName ();
	
	UCCache
		OpenCCache (
			const char*				inName);
	
	UCCache
		OpenDefaultCCache ();
		
	UCCache
		CreateCCache (
			const char*				inName,
			UPrincipal::EVersion	inVersion,
			const char*				inPrincipal);
	
	UCCache
		CreateDefaultCCache (
			UPrincipal::EVersion	inVersion,
			const char*				inPrincipal);
			
	UCCache
		CreateNewCCache (
			UPrincipal::EVersion	inVersion,
			const char*				inPrincipal);
			
	UCCacheIterator
		NewCCacheIterator ();


	UCCache
	OpenCCacheForPrincipal (
		const UPrincipal&		inPrincipal);

	bool 
		operator == (
			const UCCacheContext& inCompareTo);
};

/*
 * Exception classes
 */
 
class UCCacheLogicError:
	public std::logic_error {
public:
	explicit UCCacheLogicError (
		cc_int32		inError):
		std::logic_error ("CCacheLogicError"),
		mError (inError) {}
	
	cc_int32 Error () const { return mError; }
private:
	cc_int32			mError;
};

class UCCacheRuntimeError:
	public std::runtime_error {
public:
	explicit UCCacheRuntimeError (
		cc_int32		inError):
		std::runtime_error ("CCacheRuntimeError"),
		mError (inError) {}
	
	cc_int32 Error () const { return mError; }
private:
	cc_int32			mError;
};

#endif /* UCCache_h_ */