
#include "testutil.h"
#include <stdlib.h>
#include <stdio.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

const char *sslGetCipherSuiteString(SSLCipherSuite cs)
{
	static char noSuite[40];
	
	switch(cs) {
		case SSL_NULL_WITH_NULL_NULL:
			return "SSL_NULL_WITH_NULL_NULL";
		case SSL_RSA_WITH_NULL_MD5:
			return "SSL_RSA_WITH_NULL_MD5";
		case SSL_RSA_WITH_NULL_SHA:
			return "SSL_RSA_WITH_NULL_SHA";
		case SSL_RSA_EXPORT_WITH_RC4_40_MD5:
			return "SSL_RSA_EXPORT_WITH_RC4_40_MD5";
		case SSL_RSA_WITH_RC4_128_MD5:
			return "SSL_RSA_WITH_RC4_128_MD5";
		case SSL_RSA_WITH_RC4_128_SHA:
			return "SSL_RSA_WITH_RC4_128_SHA";
		case SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5:
			return "SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5";
		case SSL_RSA_WITH_IDEA_CBC_SHA:
			return "SSL_RSA_WITH_IDEA_CBC_SHA";
		case SSL_RSA_EXPORT_WITH_DES40_CBC_SHA:
			return "SSL_RSA_EXPORT_WITH_DES40_CBC_SHA";
		case SSL_RSA_WITH_DES_CBC_SHA:
			return "SSL_RSA_WITH_DES_CBC_SHA";
		case SSL_RSA_WITH_3DES_EDE_CBC_SHA:
			return "SSL_RSA_WITH_3DES_EDE_CBC_SHA";
		case SSL_DH_DSS_EXPORT_WITH_DES40_CBC_SHA:
			return "SSL_DH_DSS_EXPORT_WITH_DES40_CBC_SHA";
		case SSL_DH_DSS_WITH_DES_CBC_SHA:
			return "SSL_DH_DSS_WITH_DES_CBC_SHA";
		case SSL_DH_DSS_WITH_3DES_EDE_CBC_SHA:
			return "SSL_DH_DSS_WITH_3DES_EDE_CBC_SHA";
		case SSL_DH_RSA_EXPORT_WITH_DES40_CBC_SHA:
			return "SSL_DH_RSA_EXPORT_WITH_DES40_CBC_SHA";
		case SSL_DH_RSA_WITH_DES_CBC_SHA:
			return "SSL_DH_RSA_WITH_DES_CBC_SHA";
		case SSL_DH_RSA_WITH_3DES_EDE_CBC_SHA:
			return "SSL_DH_RSA_WITH_3DES_EDE_CBC_SHA";
		case SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA:
			return "SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA";
		case SSL_DHE_DSS_WITH_DES_CBC_SHA:
			return "SSL_DHE_DSS_WITH_DES_CBC_SHA";
		case SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA:
			return "SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA";
		case SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA:
			return "SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA";
		case SSL_DHE_RSA_WITH_DES_CBC_SHA:
			return "SSL_DHE_RSA_WITH_DES_CBC_SHA";
		case SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA:
			return "SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA";
		case SSL_DH_anon_EXPORT_WITH_RC4_40_MD5:
			return "SSL_DH_anon_EXPORT_WITH_RC4_40_MD5";
		case SSL_DH_anon_WITH_RC4_128_MD5:
			return "SSL_DH_anon_WITH_RC4_128_MD5";
		case SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA:
			return "SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA";
		case SSL_DH_anon_WITH_DES_CBC_SHA:
			return "SSL_DH_anon_WITH_DES_CBC_SHA";
		case SSL_DH_anon_WITH_3DES_EDE_CBC_SHA:
			return "SSL_DH_anon_WITH_3DES_EDE_CBC_SHA";
		case SSL_FORTEZZA_DMS_WITH_NULL_SHA:
			return "SSL_FORTEZZA_DMS_WITH_NULL_SHA";
		case SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA:
			return "SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA";
		case SSL_RSA_WITH_RC2_CBC_MD5:
			return "SSL_RSA_WITH_RC2_CBC_MD5";
		case SSL_RSA_WITH_IDEA_CBC_MD5:
			return "SSL_RSA_WITH_IDEA_CBC_MD5";
		case SSL_RSA_WITH_DES_CBC_MD5:
			return "SSL_RSA_WITH_DES_CBC_MD5";
		case SSL_RSA_WITH_3DES_EDE_CBC_MD5:
			return "SSL_RSA_WITH_3DES_EDE_CBC_MD5";
		case SSL_NO_SUCH_CIPHERSUITE:
			return "SSL_NO_SUCH_CIPHERSUITE";
		default:
			sprintf(noSuite, "Unknown (%d)", (unsigned)cs);
			return noSuite;	
	}
}

/* 
 * Given a SSLProtocolVersion - typically from SSLGetProtocolVersion -
 * return a string representation.
 */
const char *sslGetProtocolVersionString(SSLProtocol prot)
{
	static char noProt[20];
	
	switch(prot) {
		case kSSLProtocolUnknown:
			return "kSSLProtocolUnknown";
		case kSSLProtocol2:
			return "kSSLProtocol2";
		case kSSLProtocol3:
			return "kSSLProtocol3";
		case kSSLProtocol3Only:
			return "kSSLProtocol3Only";
		default:
			sprintf(noProt, "Unknown (%d)", (unsigned)prot);
			return noProt;	
	}
}

/* 
 * Return string representation of SecureTransport-related OSStatus.
 */
const char *sslGetSSLErrString(OSStatus err)
{
	static char noErrStr[20];
	
	switch(err) {
		case noErr:
			return "noErr";
		case memFullErr:
			return "memFullErr";
		case unimpErr:
			return "unimpErr";
		case errSSLProtocol:
			return "errSSLProtocol";
		case errSSLNegotiation:
			return "errSSLNegotiation";
		case errSSLFatalAlert:
			return "errSSLFatalAlert";
		case errSSLWouldBlock:
			return "errSSLWouldBlock";
		case ioErr:
			return "ioErr";
		case errSSLSessionNotFound:
			return "errSSLSessionNotFound";
		case errSSLClosedGraceful:
			return "errSSLClosedGraceful";
		case errSSLClosedAbort:
			return "errSSLClosedAbort";
   		case errSSLXCertChainInvalid:
			return "errSSLXCertChainInvalid";
		case errSSLBadCert:
			return "errSSLBadCert"; 
		case errSSLCrypto:
			return "errSSLCrypto";
		case errSSLInternal:
			return "errSSLInternal";
		case errSSLModuleAttach:
			return "errSSLModuleAttach";
		case errSSLUnknownRootCert:
			return "errSSLUnknownRootCert";
		case errSSLNoRootCert:
			return "errSSLNoRootCert";
		case errSSLCertExpired:
			return "errSSLCertExpired";
		case errSSLCertNotYetValid:
			return "errSSLCertNotYetValid";
		case badReqErr:
			return "badReqErr";
		case errSSLClosedNoNotify:
			return "errSSLClosedNoNotify";
		default:
			sprintf(noErrStr, "Unknown (%d)", (unsigned)err);
			return noErrStr;	
	}
}

void printSslErrStr(
	const char 	*op,
	OSStatus 	err)
{
	printf("*** %s: %s\n", op, sslGetSSLErrString(err));
}

