#ifndef __crypto_header__
#define __crypto_header__

#ifndef PACKAGE_NAME
#error "need config.h"
#endif

#ifdef KRB5
#include <krb5-types.h>
#endif

#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonHMAC.h>

#include <CommonCrypto/CommonDigestSPI.h>
#include <CommonCrypto/CommonCryptorSPI.h>
#include <CommonCrypto/CommonRandomSPI.h>
#ifndef __APPLE_TARGET_EMBEDDED__
#include <hcrypto/des.h>
#include <hcrypto/rc4.h>
#include <hcrypto/rc2.h>
#include <hcrypto/rand.h>
#include <hcrypto/pkcs12.h>
#include <hcrypto/engine.h>
#include <hcrypto/hmac.h>
#endif

#include <hcrypto/evp.h>
#include <hcrypto/ui.h>
#include <hcrypto/ec.h>
#include <hcrypto/ecdsa.h>
#include <hcrypto/ecdh.h>

#ifndef CC_DIGEST_MAX_OUTPUT_SIZE
#define CC_DIGEST_MAX_OUTPUT_SIZE 128
#endif


#endif /* __crypto_header__ */
