#ifndef _SSLTEST_H_
#define _SSLTEST_H_ 1

#include <Security/SecureTransport.h>

#ifdef	__cplusplus
extern "C" {
#endif

const char *sslGetCipherSuiteString(SSLCipherSuite cs);
const char *sslGetProtocolVersionString(SSLProtocol prot);
const char *sslGetSSLErrString(OSStatus err);
void printSslErrStr(const char 	*op, OSStatus 	err);

#ifdef	__cplusplus
}
#endif

#endif	/* _SSLTEST_H_ */
