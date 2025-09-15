//
//  SecIdentityInternal.h
//  Security
//

#ifndef _SECURITY_SECIDENTITYINTERNAL_H_
#define _SECURITY_SECIDENTITYINTERNAL_H_

#include <Security/SecBase.h>
#include <AvailabilityMacros.h>

#if defined(__cplusplus)
extern "C" {
#endif

CF_ASSUME_NONNULL_BEGIN

CF_RETURNS_RETAINED _Nullable
SecIdentityRef SecIdentityImportToFileBackedKeychain(SecIdentityRef identity, SecKeychainRef importKeychain, _Nullable SecAccessRef importAccess);

CF_ASSUME_NONNULL_END

#if defined(__cplusplus)
}
#endif

#endif // !_SECURITY_SECIDENTITYINTERNAL_H_
