//
//  ssl-utils.h
//  libsecurity_ssl
//
//  Created by Fabrice Gautier on 8/7/12.
//
//

#ifndef __SSL_UTILS_H__
#define __SSL_UTILS_H__

#include <Security/SecureTransport.h>

#define CFReleaseSafe(CF) { CFTypeRef _cf = (CF); if (_cf) {  CFRelease(_cf); } }
#define CFReleaseNull(CF) { CFTypeRef _cf = (CF); if (_cf) {  (CF) = NULL; CFRelease(_cf); } }

CFArrayRef server_chain(void);
CFArrayRef client_chain(void);

const char *ciphersuite_name(SSLCipherSuite cs);

#endif
