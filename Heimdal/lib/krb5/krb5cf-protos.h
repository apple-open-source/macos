#ifndef __krb5cf_protos_h__
#define __krb5cf_protos_h__

#include <stdarg.h>
#include <CoreFoundation/CoreFoundation.h>

#include <krb5.h>
#include <krb5-protos.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Returns an array of dictionaries containing principal name
 * currently logged in, its audit session ID and its expiration time
 *
 * @return array of dictionaries containing principal name
 * currently logged in, its audit session ID and its expiration time
 * The array needs to be released.
 *
 * @ingroup krb5
 */

KRB5_LIB_FUNCTION CFArrayRef KRB5_LIB_CALL
krb5_kcm_get_principal_list(krb5_context context) CF_RETURNS_RETAINED;

#ifdef __cplusplus
}
#endif

#endif /* __krb5cf_protos_h__ */
