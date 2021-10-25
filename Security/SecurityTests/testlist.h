/* Don't prevent multiple inclusion of this file. */
#include <regressions/test/test_regressions.h>
#include <Regressions/utilities_regressions.h>
#include "keychain/securityd/Regressions/securityd_regressions.h"
#include "keychain/SecureObjectSync/Regressions/SOSCircle_regressions.h"
#include <libsecurity_ssl/regressions/ssl_regressions.h>
#include <sectask/regressions/sectask_regressions.h>
#include <shared_regressions/shared_regressions.h>

#if TARGET_OS_OSX
#include <libsecurity_keychain/regressions/keychain_regressions.h>
#include <libsecurity_cms/regressions/cms_regressions.h>
#include <libsecurity_transform/regressions/transform_regressions.h>
#endif

#if TARGET_OS_IPHONE
#include <Security/Regressions/Security_regressions.h>
#endif
