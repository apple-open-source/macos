/* 
 * digestOpenssl.cpp - use the common code in digestCommon.h as actual openssl calls
 */

#include <stddef.h>
#include <openssl/md2.h>
#include <openssl/md4.h>
#include <openssl/md5.h>
/* No OpenSSL implementation nor prototypes for the SHA-2 functions.
   #include <openssl/fips_sha.h> */
#include <openssl/sha.h>

#include "digestCommon.h"

