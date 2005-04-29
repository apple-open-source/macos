#include <TargetConditionals.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>

#include <string.h>
#include <stdlib.h>

#include <Kerberos/com_err.h>
#include <Kerberos/profile.h>
#include <Kerberos/krb.h>
#include <kerberosIV/prot.h>
#include <Kerberos/CredentialsCache.h>
#include <Kerberos/KerberosLogin.h>

#ifdef __cplusplus

#include <Kerberos/UCCache.h>
#include <Kerberos/UProfile.h>
#include <Kerberos/UPrincipal.h>

#include <new>
#include <stdexcept>
#include <string>

#define KClientDeprecated_

#include <Kerberos/KClient.h>
#include <Kerberos/KClientTypes.h>
#include "KClientException.h"

#endif /* __cplusplus */
