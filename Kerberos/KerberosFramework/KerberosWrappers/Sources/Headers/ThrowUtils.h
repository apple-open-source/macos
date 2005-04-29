// Various error remapping and exception utilities

#include <Kerberos/krb5.h>
#include <Kerberos/KerberosDebug.h>
#include <Kerberos/UProfile.h>
#include <Kerberos/UCCache.h>
#include <Kerberos/UAutoPtr.h>
#include <Kerberos/UPrincipal.h>

#include <exception>
#include <new>

#include <errno.h>

inline void ThrowIfKerberos4Error (int inErr, const std::exception& inException) {
	if (inErr != KSUCCESS)
		DebugThrow_ (inException);
}

inline void ThrowIfCCacheError (cc_int32 inErr) {
	switch (inErr) {
		case ccNoError:
			return;
		
		case ccErrBadAPIVersion:
		case ccErrBadParam:
		case ccErrInvalidContext:
		case ccErrInvalidCCache:
		case ccErrInvalidString:
		case ccErrInvalidCredentials:
		case ccErrInvalidCCacheIterator:
		case ccErrInvalidCredentialsIterator:
		case ccErrBadName:
		case ccErrBadCredentialsVersion:
		case ccErrCCacheLocked:
		case ccErrCCacheUnlocked:
		case ccErrContextLocked:
		case ccErrContextUnlocked:
		case ccErrBadLockType:
			DebugThrow_ (UCCacheLogicError (inErr));
			
		case ccErrNoMem: {
                        std::bad_alloc foo;	// Mac OS X gcc sucks.  You have to do it this stupid way
			DebugThrow_ (foo);
                }
			
		case ccErrCredentialsNotFound:
		case ccErrCCacheNotFound:
		case ccErrNeverDefault:
        case ccErrServerUnavailable:
        case ccErrServerInsecure:
        case ccErrServerCantBecomeUID:
			DebugThrow_ (UCCacheRuntimeError (inErr));
		
		default:
			DebugThrow_ (std::logic_error ("ThrowIfCCacheError: Unknown error"));
	}
}

inline void ThrowIfProfileError (long inErr) {
	switch (inErr) {
		case 0:
			return;

		case ENOMEM: {
                        std::bad_alloc foo;	// Mac OS X gcc sucks.  You have to do it this stupid way
			DebugThrow_ (foo);
                }
		
		case PROF_NO_SECTION:
		case PROF_NO_RELATION:
			DebugThrow_ (UProfileConfigurationError (inErr));
			
		case PROF_SECTION_SYNTAX:
		case PROF_RELATION_SYNTAX:
		case PROF_EXTRA_CBRACE:
		case PROF_MISSING_OBRACE:
			DebugThrow_ (UProfileSyntaxError (inErr));

		case PROF_VERSION:
		case PROF_FAIL_OPEN:
		case ENOENT:
			DebugThrow_ (UProfileRuntimeError (inErr));

		case PROF_MAGIC_NODE:
		case PROF_ADD_NOT_SECTION:
		case PROF_SECTION_WITH_VALUE:
		case PROF_BAD_LINK_LIST:
		case PROF_BAD_GROUP_LVL:
		case PROF_BAD_PARENT_PTR:
		case PROF_MAGIC_ITERATOR:
		case PROF_SET_SECTION_VALUE:
		case PROF_EINVAL:
		case PROF_READ_ONLY:
		case PROF_SECTION_NOTOP:
		case PROF_MAGIC_PROFILE:
		case PROF_MAGIC_SECTION:
		case PROF_TOPSECTION_ITER_NOSUPP:
		case PROF_INVALID_SECTION:
		case PROF_END_OF_SECTIONS:
		case PROF_BAD_NAMESET:
		case PROF_NO_PROFILE:
		case PROF_MAGIC_FILE:
		case PROF_EXISTS:
			DebugThrow_ (UProfileLogicError (inErr));

		default:
			DebugThrow_ (std::logic_error ("ThrowIfProfileError: Unknown error"));
	}
}

inline void ThrowIfInvalid (const UCredentials& inCredentials) {
	if (inCredentials.Get () == NULL)
		DebugThrow_ (UCCacheLogicError (ccErrInvalidCredentials));
}

inline void ThrowIfInvalid (const UCCacheIterator& inIterator) {
	if (inIterator.Get () == NULL)
		DebugThrow_ (UCCacheLogicError (ccErrInvalidCCacheIterator));
}

inline void ThrowIfInvalid (const UCredentialsIterator& inIterator) {
	if (inIterator.Get () == NULL)
		DebugThrow_ (UCCacheLogicError (ccErrInvalidCredentialsIterator));
}

inline void ThrowIfInvalid (const UCCacheContext& inContext) {
	if (inContext.Get () == NULL)
		DebugThrow_ (UCCacheLogicError (ccErrInvalidContext));
}

inline void ThrowIfInvalid (const UCCache& inCCache) {
	if (inCCache.Get () == NULL)
		DebugThrow_ (UCCacheLogicError (ccErrInvalidCCache));
}

inline void ThrowIfInvalid (const UPrincipal& inPrincipal) {
	if (inPrincipal.Get () == NULL)
		DebugThrow_ (std::logic_error ("Invalid principal"));
}

