#include <Kerberos/krb.h>
#include <Kerberos/UKerberos5Context.h>
#include <Kerberos/UPrincipal.h>
#include "ThrowUtils.h"

#include <cstring>

#if __MACH__ && __MWERKS__
namespace std {
	using ::strlen;
}
#endif /* __MACH__ && __MWERKS__ */

//#include <CoreServices/CoreServices.h>

/*
 * List of possible errors returned by krb5 functions, not specified in
 * krb5 API...  Actually any of these functions can return profile errors...
 *
 * krb5_425_conv_principal:
 *	krb5_get_realm_domain + krb5_build_principal
 * krb5_get_realm_domain: 
 *	profile_get_string + ENOMEM
 * krb5_build_principal:
 *	ENOMEM, 0
 * profile_get_string:
 *	PROF_NO_SECTION, PROF_NO_RELATION, ENOMEM, 0
 * krb5_build_principal_ext:
 *	ENOMEM, 0
 * krb5_parse_name:
 *	KRB5_PARSE_MALFORMED, ENOMEM, 0
 * krb5_unparse_name:
 *	KRB5_PARSE_MALFORMED, ENOMEM, 0
 * krb5_524_conv_principal
 *	KRB5_INVALID_PRINCIPAL, 0
 * 
 */

void
UPrincipalAutoPtrDeleter::Delete (
	krb5_principal			inPrincipal)
{
	ULazyKerberos5Context	context;
	krb5_free_principal (context.Get (), inPrincipal);
}

// create principal from version and triplet
UPrincipal::UPrincipal (
	UPrincipal::EVersion			inVersion,
	const char*						inName,
	const char*						inInstance,
	const char*						inRealm)
{
	krb5_principal	principal;

	if (inVersion == kerberosV4) {
		// v4 converted to v5 first
		krb5_error_code err = krb5_425_conv_principal (mContext.Get (), 
                                                       inName, inInstance, inRealm, 
                                                       &principal);
		Assert_ ((err == ENOMEM) 
                 || (err == PROF_NO_SECTION) 
                 || (err == PROF_NO_RELATION) 
                 || (err == 0));
		if (err == ENOMEM) {
            std::bad_alloc foo;	// Mac OS X gcc sucks.  You have to do it this stupid way
			DebugThrow_ (foo);
        } else if (err != 0) 
            DebugThrow_ (UProfileConfigurationError (err));
	} else {
		krb5_error_code err = krb5_build_principal_ext (mContext.Get (), &principal,
                                                        std::strlen (inRealm), inRealm,
                                                        std::strlen (inName), inName,
                                                        std::strlen (inInstance), inInstance,
                                                        NULL);
		Assert_ ((err == ENOMEM) 
                 || (err == PROF_NO_SECTION) 
                 || (err == PROF_NO_RELATION) 
                 || (err == 0));
		if (err == ENOMEM) {
            std::bad_alloc foo;	// Mac OS X gcc sucks.  You have to do it this stupid way
			DebugThrow_ (foo);
        } else if (err != 0) 
            DebugThrow_ (UProfileConfigurationError (err));
	}

	Reset (principal);
}

// Create principal from version and string
UPrincipal::UPrincipal (
	EVersion						inVersion,
	const char*						inPrincipal)
{
	krb5_principal		principal;
	
	if (inVersion == kerberosV4) {
		// v4 converted to v5 first
		char		name [ANAME_SZ];
		char		instance [INST_SZ];
		char		realm [REALM_SZ];

                name[0] = '\0';
                instance[0] = '\0';
                realm[0] = '\0';    // So we can detect that kname_parse didn't change realm

		int v4err = kname_parse (name, instance, realm, const_cast <char*> (inPrincipal));
		ThrowIfKerberos4Error (v4err, std::runtime_error ("UPrincipal::UPrincipal: Bad Kerberos v4 principal."));

		if (realm[0] == '\0') {
			v4err = krb_get_lrealm (realm, 1);
			ThrowIfKerberos4Error (v4err, std::runtime_error ("UPrincipal::UPrincipal: Bad Kerberos configuration."));
		}
                
		krb5_error_code v5err = krb5_425_conv_principal (mContext.Get (), name, instance, realm, &principal);
		Assert_ ((v5err == ENOMEM) 
                 || (v5err == PROF_NO_SECTION) 
                 || (v5err == PROF_NO_RELATION) 
                 || (v5err == 0));
		if (v5err == ENOMEM) {
            std::bad_alloc foo;	// Mac OS X gcc sucks.  You have to do it this stupid way
			DebugThrow_ (foo);
        } else if (v5err != 0) 
            DebugThrow_ (UProfileConfigurationError (v5err));

	} else {
		krb5_error_code v5err = krb5_parse_name (mContext.Get (), inPrincipal, &principal);
        Assert_ ((v5err == ENOMEM) 
                 || (v5err == KRB5_PARSE_MALFORMED) 
                 || (v5err == PROF_NO_SECTION) 
                 || (v5err == PROF_NO_RELATION) 
                 || (v5err == 0));
		if (v5err == ENOMEM) {
            std::bad_alloc foo;	// Mac OS X gcc sucks.  You have to do it this stupid way
			DebugThrow_ (foo);
        } else if (v5err != 0) 
            DebugThrow_ (UProfileConfigurationError (v5err));
	}
	
	Reset (principal);
}

// Convert principal to string representation in specific version
std::string
UPrincipal::GetString (
	EVersion			inVersion) const
{
	ThrowIfInvalid (*this);

	if (inVersion == kerberosV4) {
		// Since internal rep of principals is v5, we have to convert back to v4
		char		name [ANAME_SZ];
		char		instance [INST_SZ];
		char		realm [REALM_SZ];
		char		principal [MAX_K_NAME_SZ];
		krb5_error_code v5err = krb5_524_conv_principal (
			mContext.Get (), Get (), name, instance, realm);
		Assert_ ((v5err == KRB5_LNAME_BADFORMAT) || (v5err == 0));
		if (v5err == KRB5_LNAME_BADFORMAT) DebugThrow_ (std::invalid_argument ("UPrincipal::GetString: Principal cannot be converted to Kerberos v4 format"));
		
		int v4err = kname_unparse (principal, name, instance, realm);
		Assert_ (v4err == KSUCCESS);

		return std::string (principal);
	} else {
		char*	principal;
		krb5_error_code v5err = krb5_unparse_name (
			mContext.Get (), Get (), &principal);
		Assert_ ((v5err == KRB5_PARSE_MALFORMED) || (v5err == ENOMEM) || (v5err == 0));
		if (v5err == ENOMEM) {
                        std::bad_alloc foo;	// Mac OS X gcc sucks.  You have to do it this stupid way
			DebugThrow_ (foo);
                }
			else if (v5err == KRB5_PARSE_MALFORMED) DebugThrow_ (std::logic_error ("UPrincipal::GetString: Principal is not valid"));
			
		std::string		result = principal;
		krb5_free_unparsed_name (mContext.Get (), principal);
		return result;
	}
}

// Convert principal to display string (for user presentation, with \ removed)
std::string
UPrincipal::GetDisplayString (
	EVersion			inVersion) const
{
	// To make the string pretty for display, we remove all backslashes used to escape 
	// special characters. For example, "John Q\. Public@ATHENA.MIT.EDU" becomes
	// "John Q. Public@ATHENA.MIT.EDU". This is lossy and unrecoverable!
	
	std::string		result = GetString (inVersion);
	std::string::size_type where = 0;

	for (;;) {
		where = result.find ('\\', where);
		if (where == std::string::npos) {
			// not found
			break;
		}
		
		result.erase (where, 1);
	}
		
	return result;
}

// Get name (1st component) of the principal
std::string
UPrincipal::GetName (
	EVersion				inVersion) const
{
	ThrowIfInvalid (*this);

	if (inVersion == kerberosV4) {
		char		name [ANAME_SZ];
		char		instance [INST_SZ];
		char		realm [REALM_SZ];
		krb5_error_code v5err = krb5_524_conv_principal (
			mContext.Get (), Get (), name, instance, realm);
		Assert_ ((v5err == KRB5_LNAME_BADFORMAT) || (v5err == 0));
		if (v5err == KRB5_LNAME_BADFORMAT) DebugThrow_ (std::invalid_argument ("UPrincipal::GetName: Principal cannot be converted to Kerberos v4 format"));
		
		return std::string (name);
	} else {
		return std::string (krb5_princ_name (mContext.Get (), Get ()) -> data, krb5_princ_name (mContext.Get (), Get ()) -> length);
	}
}

// Get the instance (in the case of a multi-instance principal, just the first instance)
std::string
UPrincipal::GetInstance (
	EVersion				inVersion,
	u_int32_t				inIndex) const
{
	ThrowIfInvalid (*this);

	if (inVersion == kerberosV4) {
		if (inIndex != 1) DebugThrow_ (std::invalid_argument ("UPrincipal::GetInstance: index != 0 for a v4 principal."));
		char		name [ANAME_SZ];
		char		instance [INST_SZ];
		char		realm [REALM_SZ];
		krb5_error_code v5err = krb5_524_conv_principal (
			mContext.Get (), Get (), name, instance, realm);
		Assert_ ((v5err == KRB5_LNAME_BADFORMAT) || (v5err == 0));
		if (v5err == KRB5_LNAME_BADFORMAT) DebugThrow_ (std::invalid_argument ("UPrincipal::GetInstance: Principal cannot be converted to Kerberos v4 format"));
		
		return std::string (instance);
	} else {
		if (static_cast<int32_t>(inIndex) < krb5_princ_size (mContext.Get (), Get ())) {
			return std::string (krb5_princ_component (mContext.Get (), Get (), inIndex) -> data, krb5_princ_component (mContext.Get (), Get (), inIndex) -> length);
		}
		return std::string ("");
	}
}

// Get the realm for a principal
std::string
UPrincipal::GetRealm (
	EVersion				inVersion) const
{
	ThrowIfInvalid (*this);

	if (inVersion == kerberosV4) {
		char		name [ANAME_SZ];
		char		instance [INST_SZ];
		char		realm [REALM_SZ];
		krb5_error_code v5err = krb5_524_conv_principal (
			mContext.Get (), Get (), name, instance, realm);
		Assert_ ((v5err == KRB5_LNAME_BADFORMAT) || (v5err == 0));
		if (v5err == KRB5_LNAME_BADFORMAT) DebugThrow_ (std::invalid_argument ("UPrincipal::GetRealm: Principal cannot be converted to Kerberos v4 format"));
		
		return std::string (realm);
	} else {
		if (krb5_princ_realm (mContext.Get (), Get ()) -> length == 0) {
			return std::string ("");
		}
		return std::string (krb5_princ_realm (mContext.Get (), Get ()) -> data, krb5_princ_realm (mContext.Get (), Get ()) -> length);
	}
}

// Get all three components of the principal (only the first instance for a multi-instance principal)
void
UPrincipal::GetTriplet (
	EVersion				inVersion,
	std::string&			outName,
	std::string&			outInstance,
	std::string&			outRealm) const
{
	ThrowIfInvalid (*this);

	if (inVersion == kerberosV4) {
		char		name [ANAME_SZ];
		char		instance [INST_SZ];
		char		realm [REALM_SZ];
		krb5_error_code v5err = krb5_524_conv_principal (
			mContext.Get (), Get (), name, instance, realm);
		Assert_ ((v5err == KRB5_LNAME_BADFORMAT) || (v5err == 0));
		if (v5err == KRB5_LNAME_BADFORMAT) DebugThrow_ (std::invalid_argument ("UPrincipal::GetTriplet: Principal cannot be converted to Kerberos v4 format"));
		
		outName = name;
		outInstance = instance;
		outRealm = realm;
	} else {
		// Name
		outName = std::string (krb5_princ_name (mContext.Get (), Get ()) -> data, krb5_princ_name (mContext.Get (), Get ()) -> length);

		// Instance
		if (krb5_princ_size (mContext.Get (), Get ()) > 1) 
			outInstance = std::string (krb5_princ_component (mContext.Get (), Get (), 1) -> data, krb5_princ_component (mContext.Get (), Get (), 1) -> length);
		else 
			outInstance = "";

		// Realm
		if (krb5_princ_realm (mContext.Get (), Get ()) -> length == 0)
			outRealm = "";
		else
			outRealm = std::string (krb5_princ_realm (mContext.Get (), Get ()) -> data, krb5_princ_realm (mContext.Get (), Get ()) -> length);
	}
}

// Convert to KLPrincipal
KLPrincipal
UPrincipal::GetKLPrincipal () const 
{
	ThrowIfInvalid (*this);
	
	KLPrincipal	newPrincipal;
	
	KLStatus err = KLCreatePrincipalFromString (GetString (kerberosV5).c_str (),
                                             kerberosVersion_V5,
                                             &newPrincipal);
		
	if (err != klNoErr) {
		Assert_ ((err == klMemFullErr) || (err == klV5InitializationFailedErr) || 
					(err == klParameterErr) || (err == klBadPrincipalErr) ||
					(err == klPreferencesReadErr));
		
		if (err == klMemFullErr) {
            std::bad_alloc foo;	// Mac OS X gcc sucks.  You have to do it this stupid way
			DebugThrow_ (foo);
		} else if ((err == klV5InitializationFailedErr) || (err == klPreferencesReadErr)) {
			DebugThrow_ (UPrincipalRuntimeError (err));
		} else {
			DebugThrow_ (UPrincipalLogicError (err));
		}
	}
	
	return newPrincipal;
}

// Clone by making a new copy of the principal data
UPrincipal
UPrincipal::Clone () const
{
	ThrowIfInvalid (*this);
	
	krb5_principal	newPrincipal = NULL;
	
	krb5_error_code	v5err = krb5_copy_principal (mContext.Get (), Get (), &newPrincipal);
	Assert_ ((v5err == 0) || (v5err == ENOMEM));
	if (v5err == ENOMEM){
                        std::bad_alloc foo;	// Mac OS X gcc sucks.  You have to do it this stupid way
			DebugThrow_ (foo);
                }
	
	return UPrincipal (newPrincipal);
}

// Compare principals
bool
operator == (
	const UPrincipal&		inLeft,
	const UPrincipal&		inRight)
{
	ThrowIfInvalid (inLeft);
	ThrowIfInvalid (inRight);
	
	return krb5_principal_compare (inLeft.mContext.Get (), inLeft.Get (), inRight.Get ());
}