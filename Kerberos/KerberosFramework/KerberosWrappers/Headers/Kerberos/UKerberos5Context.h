#ifndef UKerberos5Context_h_
#define UKerberos5Context_h_

#include <Kerberos/UAutoPtr.h>
#include <Kerberos/krb5.h>

class UKerberos5ContextAutoPtrDeleter {
	public: static void Delete (krb5_context		inContext);
};
class UKerberos5SecureContextAutoPtrDeleter {
	public: static void Delete (krb5_context		inContext);
};

typedef UAutoPtr <_krb5_context, UKerberos5ContextAutoPtrDeleter>		UKerberos5ContextAutoPtr;
typedef UAutoPtr <_krb5_context, UKerberos5SecureContextAutoPtrDeleter>	UKerberos5SecureContextAutoPtr;
typedef UKerberos5ContextAutoPtr::UAutoPtrRef							UKerberos5ContextAutoPtrRef;
typedef UKerberos5SecureContextAutoPtr::UAutoPtrRef						UKerberos5SecureContextAutoPtrRef;

// UKerberos5Context is a stack based class which creates a krb5 context in 
// the constructor and releases it in the destructor. Use Get() to get the context

class UKerberos5Context:
	public UKerberos5ContextAutoPtr {
public:
        UKerberos5Context ();
		UKerberos5Context (
			krb5_context				inContext):
			UKerberos5ContextAutoPtr (inContext) {}
		UKerberos5Context (
			UKerberos5ContextAutoPtrRef		inReference):
			UKerberos5ContextAutoPtr (inReference) {}
		~UKerberos5Context () {}

		UKerberos5Context&			operator = (
			UKerberos5ContextAutoPtr::UAutoPtrRef	inOriginal) {
			Reset (inOriginal.mPtr);
			return *this;
		}
		
		UKerberos5Context&			operator = (
			UKerberos5Context&	inOriginal) {
			Reset (inOriginal.Release ());
			return *this;
		}
		
		UKerberos5Context&			operator = (
			krb5_context		inOriginal) {
			Reset (inOriginal);
			return *this;
		}
		
};

// UKerberos5SecureContext is a stack based class which creates a secure krb5 context in 
// the constructor and releases it in the destructor. Use Get() to get the context

class UKerberos5SecureContext:
	public UKerberos5SecureContextAutoPtr {
public:
        UKerberos5SecureContext ();
		UKerberos5SecureContext (
			krb5_context				inContext):
			UKerberos5SecureContextAutoPtr (inContext) {}
		UKerberos5SecureContext (
			UKerberos5SecureContextAutoPtrRef		inReference):
			UKerberos5SecureContextAutoPtr (inReference) {}
		~UKerberos5SecureContext () {}

		UKerberos5SecureContext&			operator = (
			UKerberos5SecureContextAutoPtr::UAutoPtrRef	inOriginal) {
			Reset (inOriginal.mPtr);
			return *this;
		}
		
		UKerberos5SecureContext&			operator = (
			UKerberos5SecureContext&	inOriginal) {
			Reset (inOriginal.Release ());
			return *this;
		}
		
		UKerberos5SecureContext&			operator = (
			krb5_context		inOriginal) {
			Reset (inOriginal);
			return *this;
		}
		
};

// ULazyKerberos5Context is similar to UKerberos5Context, except that it
// does not create the context until it's needed (which is the first time
// you call Get ()

class ULazyKerberos5Context {
	public:
		struct ULazyKerberos5ContextRef {
			ULazyKerberos5Context&		mAutoPtr;
			ULazyKerberos5ContextRef (
				const ULazyKerberos5Context&	inAutoPtr):
				mAutoPtr (const_cast <ULazyKerberos5Context&> (inAutoPtr)) {}
		};
		
		explicit ULazyKerberos5Context (
			_krb5_context*							inPointer = 0):
			mPointer (inPointer) {
		}
		
		ULazyKerberos5Context (
			ULazyKerberos5Context&				inOriginal):
			mPointer (inOriginal.Release ()) {
		}
		
		ULazyKerberos5Context&			operator = (
			ULazyKerberos5Context&		inOriginal) {
			Reset (inOriginal.Release ());
			return *this;
		}
		
		ULazyKerberos5Context (
			ULazyKerberos5ContextRef			inOriginal):
			mPointer (inOriginal.mAutoPtr.Release ()) {}
		
		~ULazyKerberos5Context () {
			if (mPointer != NULL) {
				UKerberos5ContextAutoPtrDeleter::Delete (mPointer);
			}
		}
		
		_krb5_context&						operator* () const {
			return *Get ();
		}
		
		_krb5_context*						operator -> () const {
			return Get ();
		}
		
		operator ULazyKerberos5ContextRef () const {
			return ULazyKerberos5ContextRef (*this);
		}
		
		_krb5_context*						Get () const {
			if (mPointer == NULL) {
				InitializeContext ();
			}
			return mPointer;
		}
		
		_krb5_context*						Release () {
			_krb5_context*	result = mPointer;
			mPointer = NULL;
			return result;
		}
		
		void					Reset (
			_krb5_context*							inNewPointer) {
			if (inNewPointer != mPointer) {
				if (mPointer != NULL)
					UKerberos5ContextAutoPtrDeleter::Delete (mPointer);
				mPointer = inNewPointer;
			}
		}
		
		
	private:
		
		_krb5_context*	mPointer;
		
		void InitializeContext () const;
};

// ULazyKerberos5SecureContext is similar to UKerberos5SecureContext, except that it
// does not create the context until it's needed (which is the first time
// you call Get ()

class ULazyKerberos5SecureContext {
	public:
		struct ULazyKerberos5SecureContextRef {
			ULazyKerberos5SecureContext&		mAutoPtr;
			ULazyKerberos5SecureContextRef (
				const ULazyKerberos5SecureContext&	inAutoPtr):
				mAutoPtr (const_cast <ULazyKerberos5SecureContext&> (inAutoPtr)) {}
		};
		
		explicit ULazyKerberos5SecureContext (
			_krb5_context*							inPointer = 0):
			mPointer (inPointer) {
		}
		
		ULazyKerberos5SecureContext (
			ULazyKerberos5SecureContext&				inOriginal):
			mPointer (inOriginal.Release ()) {
		}
		
		ULazyKerberos5SecureContext&			operator = (
			ULazyKerberos5SecureContext&		inOriginal) {
			Reset (inOriginal.Release ());
			return *this;
		}
		
		ULazyKerberos5SecureContext (
			ULazyKerberos5SecureContextRef			inOriginal):
			mPointer (inOriginal.mAutoPtr.Release ()) {}
		
		~ULazyKerberos5SecureContext () {
			if (mPointer != NULL) {
				UKerberos5SecureContextAutoPtrDeleter::Delete (mPointer);
			}
		}
		
		_krb5_context&						operator* () const {
			return *Get ();
		}
		
		_krb5_context*						operator -> () const {
			return Get ();
		}
		
		operator ULazyKerberos5SecureContextRef () const {
			return ULazyKerberos5SecureContextRef (*this);
		}
		
		_krb5_context*						Get () const {
			if (mPointer == NULL) {
				InitializeSecureContext ();
			}
			return mPointer;
		}
		
		_krb5_context*						Release () {
			_krb5_context*	result = mPointer;
			mPointer = NULL;
			return result;
		}
		
		void					Reset (
			_krb5_context*							inNewPointer) {
			if (inNewPointer != mPointer) {
				if (mPointer != NULL)
					UKerberos5SecureContextAutoPtrDeleter::Delete (mPointer);
				mPointer = inNewPointer;
			}
		}
		
		
	private:
		
		_krb5_context*	mPointer;
		
		void InitializeSecureContext () const;
};

#endif /* UKerberos5Context_h_ */