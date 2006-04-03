/*
 * KClientPrincipal
 *
 * Abstraction for a KClient principal
 *
 * $Header$
 */
 
#ifndef UPrincipal_h_
#define UPrincipal_h_

#include <string>
#include <stdexcept>

#include <Kerberos/krb5.h>
#include <Kerberos/CredentialsCache.h>
#include <Kerberos/KerberosLogin.h>
#include <Kerberos/UAutoPtr.h>
#include <Kerberos/UKerberos5Context.h>

class UPrincipalAutoPtrDeleter {
	public: static void Delete (krb5_principal	inPrincipal);
};

typedef	UAutoPtr <krb5_principal_data, UPrincipalAutoPtrDeleter>		UPrincipalAutoPtr;
typedef	UPrincipalAutoPtr::UAutoPtrRef									UPrincipalAutoPtrRef;

class UPrincipal;

// UPrincipal is a stack based class which represents Kerberos principals,
// both v4 and v5.

class UPrincipal:
	public UPrincipalAutoPtr {
public:
		enum EVersion {
			kerberosV4 = cc_credentials_v4,
			kerberosV5 = cc_credentials_v5,
			kerberosV4And5 = cc_credentials_v4_v5
		};
	
		explicit UPrincipal (
			krb5_principal				inString = 0):
			UPrincipalAutoPtr (inString) {}
		UPrincipal (
                         UPrincipal&				inOriginal):
                    UPrincipalAutoPtr (inOriginal) {}
		UPrincipal (
			UPrincipalAutoPtrRef		inReference):
			UPrincipalAutoPtr (inReference) {}
		UPrincipal (
			EVersion						inVersion,
			const char*						inPrincipal);
		UPrincipal (
			EVersion						inVersion,
			const char*						inName,
			const char*						inInstance,
			const char*						inRealm);
		~UPrincipal () {}
	
		UPrincipal&			operator = (
			UPrincipalAutoPtr::UAutoPtrRef	inOriginal) {
			Reset (inOriginal.mPtr);
			return *this;
		}
		
		UPrincipal&			operator = (
			UPrincipal&	inOriginal) {
			Reset (inOriginal.Release ());
			return *this;
		}
		
		UPrincipal&			operator = (
			krb5_principal	inOriginal) {
			Reset (inOriginal);
			return *this;
		}
		
		std::string GetString (
			EVersion						inVersion) const;
											
		std::string GetDisplayString (
			EVersion						inVersion) const;
											
		void GetTriplet (
			EVersion						inVersion,
			std::string&					outName,
			std::string&					outInstance,
			std::string&					outRealm) const;
											
		std::string GetName (
			EVersion						inVersion) const;

		std::string GetInstance (
			EVersion						inVersion,
			u_int32_t						inIndex = 1) const;

		std::string GetRealm (
			EVersion						inVersion) const;
			
		UPrincipal	Clone () const;
			
		KLPrincipal GetKLPrincipal () const;
											
        bool operator == (const UPrincipal& inCompareTo);
	private:
		ULazyKerberos5Context		mContext;
		friend class UPrincipalAutoPtrDeleter;		
};

/*
 * Exception classes
 */
 
class UPrincipalLogicError:
	public std::logic_error {
public:
	explicit UPrincipalLogicError (
		long		inError):
		std::logic_error ("UPrincipalLogicError"),
		mError (inError) {}
	
	long Error () const { return mError; }
private:
	long			mError;
};

class UPrincipalRuntimeError:
	public std::runtime_error {
public:
	explicit UPrincipalRuntimeError (
		long		inError):
		std::runtime_error ("UPrincipalRuntimeError"),
		mError (inError) {}
	
	long Error () const { return mError; }
private:
	long			mError;
};

#endif /* UPrincipal_h_ */
