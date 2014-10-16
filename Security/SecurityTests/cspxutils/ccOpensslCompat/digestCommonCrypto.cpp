/* 
 * digestOpenssl.cpp - use the common code in digestCommon.h as CommonCrypto calls
 */

#define COMMON_DIGEST_FOR_OPENSSL	1
#include <CommonCrypto/CommonDigest.h>
#include "digestCommon.h"
