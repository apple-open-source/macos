
#include <Kerberos/UKerberos5Context.h>
#include "ThrowUtils.h"

void
UKerberos5ContextAutoPtrDeleter::Delete (
	krb5_context		inContext)
{
	krb5_free_context (inContext);
}

void
UKerberos5SecureContextAutoPtrDeleter::Delete (
	krb5_context		inContext)
{
	krb5_free_context (inContext);
}

// Create a new default context
UKerberos5Context::UKerberos5Context ():
	UKerberos5ContextAutoPtr ()
{
	krb5_context	context;
	krb5_error_code err = krb5_init_context (&context);
	Assert_ ((err == ENOENT) || (err == ENOMEM) || (err == KRB5_CONFIG_BADFORMAT) ||
		(err == KRB5_CONFIG_CANTOPEN) || (err == 0));
	if (err == ENOMEM)  {
                        std::bad_alloc foo;	// Mac OS X gcc sucks.  You have to do it this stupid way
			DebugThrow_ (foo);
                }
		else if (err == KRB5_CONFIG_BADFORMAT) DebugThrow_ (UProfileSyntaxError (err));
		else if ((err == KRB5_CONFIG_CANTOPEN) || (err == ENOENT)) DebugThrow_ (UProfileConfigurationError (err));
	
	Reset (context);
}

// Create a new default context
UKerberos5SecureContext::UKerberos5SecureContext ():
	UKerberos5SecureContextAutoPtr ()
{
	krb5_context	context;
	krb5_error_code err = krb5_init_secure_context (&context);
	Assert_ ((err == ENOENT) || (err == ENOMEM) || (err == KRB5_CONFIG_BADFORMAT) ||
		(err == KRB5_CONFIG_CANTOPEN) || (err == 0));
	if (err == ENOMEM)  {
                        std::bad_alloc foo;	// Mac OS X gcc sucks.  You have to do it this stupid way
			DebugThrow_ (foo);
                }
		else if (err == KRB5_CONFIG_BADFORMAT) DebugThrow_ (UProfileSyntaxError (err));
		else if ((err == KRB5_CONFIG_CANTOPEN) || (err == ENOENT)) DebugThrow_ (UProfileConfigurationError (err));
	
	Reset (context);
}

// Create a new default context
void ULazyKerberos5Context::InitializeContext () const {
	krb5_context	context;
	krb5_error_code err = krb5_init_context (&context);
	Assert_ ((err == ENOENT) || (err == ENOMEM) || (err == KRB5_CONFIG_BADFORMAT) ||
		(err == KRB5_CONFIG_CANTOPEN) || (err == 0));
	if (err == ENOMEM)  {
                        std::bad_alloc foo;	// Mac OS X gcc sucks.  You have to do it this stupid way
			DebugThrow_ (foo);
                }
		else if (err == KRB5_CONFIG_BADFORMAT) DebugThrow_ (UProfileSyntaxError (err));
		else if ((err == KRB5_CONFIG_CANTOPEN) || (err == ENOENT)) DebugThrow_ (UProfileConfigurationError (err));
	
	const_cast <ULazyKerberos5Context*> (this) -> Reset (context);
}

// Create a new default context
void ULazyKerberos5SecureContext::InitializeSecureContext () const {
	krb5_context	context;
	krb5_error_code err = krb5_init_secure_context (&context);
	Assert_ ((err == ENOENT) || (err == ENOMEM) || (err == KRB5_CONFIG_BADFORMAT) ||
		(err == KRB5_CONFIG_CANTOPEN) || (err == 0));
	if (err == ENOMEM)  {
                        std::bad_alloc foo;	// Mac OS X gcc sucks.  You have to do it this stupid way
			DebugThrow_ (foo);
                }
		else if (err == KRB5_CONFIG_BADFORMAT) DebugThrow_ (UProfileSyntaxError (err));
		else if ((err == KRB5_CONFIG_CANTOPEN) || (err == ENOENT)) DebugThrow_ (UProfileConfigurationError (err));
	
	const_cast <ULazyKerberos5SecureContext*> (this) -> Reset (context);
}
