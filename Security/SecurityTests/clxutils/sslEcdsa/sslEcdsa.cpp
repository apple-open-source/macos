/*
 * sslEcdsa.cpp - test SSL connections to a number of known servers. 
 *
 * Note this uses the keychain ecdsa.keychain in cwd; it contains an
 * SSL client auth identity. To avoid ACL hassles and to allow this
 * program to run hands-off, the identity is imported into this keychain
 * with no ACL on the private key. This is done with the kcImport tool 
 * like so:
 *
 * % kcImport ecc-secp256r1-client.pfx -k ___path_to_cwd___/ecdsa.keychain -f pkcs12 -z password -n
 */
#include <Security/SecureTransport.h>
#include <Security/SecureTransportPriv.h>
#include <Security/Security.h>
#include <clAppUtils/sslAppUtils.h>
#include <clAppUtils/ioSock.h>
#include <utilLib/common.h>

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/param.h>

static void usage(char **argv)
{
	printf("Usage: %s [options]\n", argv[0]);
	printf("options:\n");
	printf("   -t testNum   -- only do test testNum; default is all\n");
	printf("   -q           -- quiet\n");
	printf("   -b           -- non blocking I/O\n");
	printf("   -p           -- pause for malloc debug\n");
	exit(1);
}

#define IGNORE_SIGPIPE	1
#if 	IGNORE_SIGPIPE
#include <signal.h>

void sigpipe(int sig) 
{ 
}
#endif	/* IGNORE_SIGPIPE */

/* Test params */
typedef struct {
	const char				*hostName;
	int						port;

	/* We enable exacly one CipherSuite and require that to work */
	SSLCipherSuite			cipherSuite;
	
	/* Curve to specify; SSL_Curve_None means use default */
	SSL_ECDSA_NamedCurve	specCurve;
	
	/* Curve to verify; SSL_Curve_None means don't check */
	SSL_ECDSA_NamedCurve	expCurve;
	
	/* 
	 * keychain containing client-side cert, located in LOCAL_BUILD_DIR.
	 * NULL means no keychain.
	 */
	const char				*keychain;
	
	/* password for above keychain */
	const char				*kcPassword;
} EcdsaTestParams;

static const EcdsaTestParams ecdsaTestParams[] = 
{
	/* client auth */
	{
		"tls.secg.org", 8443, TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
		SSL_Curve_None, SSL_Curve_None,
		"ecdsa.keychain", "password"
	},
	/* tla.secg.org -- port 40023 - secp256r1  */
	{
		"tls.secg.org", 40023, TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
		SSL_Curve_None, SSL_Curve_secp256r1
	},
	{
		"tls.secg.org", 40023, TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
		SSL_Curve_secp256r1, SSL_Curve_secp256r1
	},
	{
		"tls.secg.org", 40023, TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
		SSL_Curve_None, SSL_Curve_secp256r1
	},
	{
		"tls.secg.org", 40023, TLS_ECDHE_ECDSA_WITH_RC4_128_SHA,
		SSL_Curve_None, SSL_Curve_secp256r1
	},
	{
		"tls.secg.org", 40023, TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA,
		SSL_Curve_None, SSL_Curve_secp256r1
	},
	{
		"tls.secg.org", 40023, TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA,
		SSL_Curve_None, SSL_Curve_secp256r1
	},
	{
		"tls.secg.org", 40023, TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA,
		SSL_Curve_None, SSL_Curve_secp256r1
	},
	{
		"tls.secg.org", 40023, TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA,
		SSL_Curve_secp256r1, SSL_Curve_secp256r1
	},
	{
		"tls.secg.org", 40023, TLS_ECDH_ECDSA_WITH_RC4_128_SHA,
		SSL_Curve_None, SSL_Curve_secp256r1
	},
	{
		"tls.secg.org", 40023, TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA,
		SSL_Curve_None, SSL_Curve_secp256r1
	},
	{
		"tls.secg.org", 40023, TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA,
		SSL_Curve_secp256r1, SSL_Curve_secp256r1
	},
	
	/* tla.secg.org -- port 40024 - secp384r1 */
	/* This one doesn't let you specify a curve */
	{
		"tls.secg.org", 40024, TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
		SSL_Curve_None, SSL_Curve_secp384r1
	},
	{
		"tls.secg.org", 40024, TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
		SSL_Curve_None, SSL_Curve_secp384r1
	},
	{
		"tls.secg.org", 40024, TLS_ECDHE_ECDSA_WITH_RC4_128_SHA,
		SSL_Curve_None, SSL_Curve_secp384r1
	},
	{
		"tls.secg.org", 40024, TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA,
		SSL_Curve_None, SSL_Curve_secp384r1
	},
	{
		"tls.secg.org", 40024, TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA,
		SSL_Curve_None, SSL_Curve_secp384r1
	},
	{
		"tls.secg.org", 40024, TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA,
		SSL_Curve_None, SSL_Curve_secp384r1
	},
	{
		"tls.secg.org", 40024, TLS_ECDH_ECDSA_WITH_RC4_128_SHA,
		SSL_Curve_None, SSL_Curve_secp384r1
	},
	{
		"tls.secg.org", 40024, TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA,
		SSL_Curve_None, SSL_Curve_secp384r1
	},

	/* tla.secg.org -- port 40025 - secp521r1 */
	{
		"tls.secg.org", 40025, TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
		SSL_Curve_None, SSL_Curve_secp521r1
	},
	{
		"tls.secg.org", 40025, TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
		SSL_Curve_None, SSL_Curve_secp521r1
	},
	{
		"tls.secg.org", 40025, TLS_ECDHE_ECDSA_WITH_RC4_128_SHA,
		SSL_Curve_None, SSL_Curve_secp521r1
	},
	{
		"tls.secg.org", 40025, TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA,
		SSL_Curve_None, SSL_Curve_secp521r1
	},
	{
		"tls.secg.org", 40025, TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA,
		SSL_Curve_None, SSL_Curve_secp521r1
	},
	{
		"tls.secg.org", 40025, TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA,
		SSL_Curve_None, SSL_Curve_secp521r1
	},
	{
		"tls.secg.org", 40025, TLS_ECDH_ECDSA_WITH_RC4_128_SHA,
		SSL_Curve_None, SSL_Curve_secp521r1
	},
	{
		"tls.secg.org", 40025, TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA,
		SSL_Curve_None, SSL_Curve_secp521r1
	},
	
	
	/* ecc.fedora.redhat.com - port 8443 - secp256r1 */
	{
		"ecc.fedora.redhat.com", 8443, TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
		SSL_Curve_None, SSL_Curve_secp256r1
	},
	{
		"ecc.fedora.redhat.com", 8443, TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
		SSL_Curve_secp256r1, SSL_Curve_secp256r1
	},
	{
		"ecc.fedora.redhat.com", 8443, TLS_ECDHE_ECDSA_WITH_RC4_128_SHA,
		SSL_Curve_None, SSL_Curve_secp256r1
	},
	{
		"ecc.fedora.redhat.com", 8443, TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA,
		SSL_Curve_secp256r1, SSL_Curve_secp256r1
	},
	{
		"ecc.fedora.redhat.com", 8443, TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA,
		SSL_Curve_secp256r1, SSL_Curve_secp256r1
	},
	{
		"ecc.fedora.redhat.com", 8443, TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA,
		SSL_Curve_None, SSL_Curve_secp256r1
	},
	{
		"ecc.fedora.redhat.com", 8443, TLS_ECDH_ECDSA_WITH_RC4_128_SHA,
		SSL_Curve_secp256r1, SSL_Curve_secp256r1
	},
	{
		"ecc.fedora.redhat.com", 8443, TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA,
		SSL_Curve_secp256r1, SSL_Curve_secp256r1
	},
	
	/* ecc.fedora.redhat.com - port 8444 - SSL_Curve_secp384r1 */
	/* This doesn't work, the server requires a redirect ...
	{
		"ecc.fedora.redhat.com", 8444, TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
		SSL_Curve_None, SSL_Curve_secp384r1
	},
	*/
	{
		"ecc.fedora.redhat.com", 8445, TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
		SSL_Curve_None, SSL_Curve_secp521r1
	},
	{
		"ecc.fedora.redhat.com", 8444, TLS_ECDHE_ECDSA_WITH_RC4_128_SHA,
		SSL_Curve_secp384r1, SSL_Curve_secp384r1
	},
	{
		"ecc.fedora.redhat.com", 8444, TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA,
		SSL_Curve_secp384r1, SSL_Curve_secp384r1
	},
	{
		"ecc.fedora.redhat.com", 8444, TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA,
		SSL_Curve_None, SSL_Curve_secp384r1
	},
	{
		"ecc.fedora.redhat.com", 8444, TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA,
		SSL_Curve_None, SSL_Curve_secp384r1
	},
	{
		"ecc.fedora.redhat.com", 8444, TLS_ECDH_ECDSA_WITH_RC4_128_SHA,
		SSL_Curve_secp384r1, SSL_Curve_secp384r1
	},
	{
		"ecc.fedora.redhat.com", 8444, TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA,
		SSL_Curve_secp384r1, SSL_Curve_secp384r1
	},
	
	/* ecc.fedora.redhat.com - port 8445 - SSL_Curve_secp521r1 */
	/* This one can't do RC4_128 without some HTTP redirection */
	{
		"ecc.fedora.redhat.com", 8445, TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA,
		SSL_Curve_None, SSL_Curve_secp521r1
	},
	{
		"ecc.fedora.redhat.com", 8445, TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA,
		SSL_Curve_secp521r1, SSL_Curve_secp521r1
	},
	{
		"ecc.fedora.redhat.com", 8445, TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
		SSL_Curve_secp521r1, SSL_Curve_secp521r1
	},
	
	/* ecc.fedora.redhat.com - port 443 - secp256r1 with RSA authentication */
	{
		"ecc.fedora.redhat.com", 443, TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
		SSL_Curve_secp256r1, SSL_Curve_secp256r1
	},
	{
		"ecc.fedora.redhat.com", 443, TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,
		SSL_Curve_secp256r1, SSL_Curve_secp256r1
	},
	{
		"ecc.fedora.redhat.com", 443, TLS_ECDHE_RSA_WITH_RC4_128_SHA,
		SSL_Curve_secp256r1, SSL_Curve_secp256r1
	},
	{
		"ecc.fedora.redhat.com", 443, TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA,
		SSL_Curve_secp256r1, SSL_Curve_secp256r1
	},
	{
		"ecc.fedora.redhat.com", 443, TLS_ECDH_RSA_WITH_AES_128_CBC_SHA,
		SSL_Curve_None, SSL_Curve_secp256r1
	},
	{
		"ecc.fedora.redhat.com", 443, TLS_ECDH_RSA_WITH_AES_256_CBC_SHA,
		SSL_Curve_secp256r1, SSL_Curve_secp256r1
	},
	{
		"ecc.fedora.redhat.com", 443, TLS_ECDH_RSA_WITH_RC4_128_SHA,
		SSL_Curve_None, SSL_Curve_secp256r1
	},
	{
		"ecc.fedora.redhat.com", 443, TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA,
		SSL_Curve_None, SSL_Curve_secp256r1
	},
	
	/* etc. */
};
#define NUM_TEST_PARAMS		(sizeof(ecdsaTestParams) / sizeof(ecdsaTestParams[0]))

static void dumpParams(
	const EcdsaTestParams *testParams)
{
	printf("%s:%d %-33s ",
		testParams->hostName, testParams->port,
		/* skip leading "TLS_" */
		sslGetCipherSuiteString(testParams->cipherSuite)+4);
	if(testParams->expCurve != SSL_Curve_None) {
		printf("expCurve = %s ", sslCurveString(testParams->expCurve));
	}
	if(testParams->specCurve != SSL_Curve_None) {
		printf("specCurve = %s ", sslCurveString(testParams->specCurve));
	}
	if(testParams->keychain) {
		printf("Client Auth Enabled");
	}
	putchar('\n');
}

static void dumpErrInfo(
	const char *op,
	const EcdsaTestParams *testParams,
	OSStatus ortn)
{
	printf("***%s failed for ", op);
	dumpParams(testParams);
	printf("   error: %s\n", sslGetSSLErrString(ortn));
}

/* 
 * Custom ping for this test.
 */
#define RCV_BUF_SIZE		256

static int doSslPing(
	const EcdsaTestParams	*testParams,
	bool					quiet,
	int						nonBlocking)
{
    PeerSpec            peerId;
	otSocket			sock = 0;
    OSStatus            ortn;
    SSLContextRef       ctx = NULL;
	SSLCipherSuite		negCipher;
	
	/* first make sure requested server is there */
	ortn = MakeServerConnection(testParams->hostName, testParams->port, 
		nonBlocking, &sock, &peerId);
    if(ortn) {
    	printf("MakeServerConnection(%s) returned %d\n", 
			testParams->hostName, (int)ortn);
    	return -1;
    }

	/* 
	 * Set up a SecureTransport session.
	 * First the standard calls.
	 */
	ortn = SSLNewContext(false, &ctx);
	if(ortn) {
		printSslErrStr("SSLNewContext", ortn);
		goto cleanup;
	} 
	ortn = SSLSetIOFuncs(ctx, SocketRead, SocketWrite);
	if(ortn) {
		printSslErrStr("SSLSetIOFuncs", ortn);
		goto cleanup;
	} 
	
	/* Restrict to only TLSv1 - we have to do this because of Radar 6133465 */
	ortn = SSLSetProtocolVersionEnabled(ctx, kSSLProtocolAll, false);
	if(ortn) {
		printSslErrStr("SSLSetProtocolVersionEnabled", ortn);
		goto cleanup;
	}
	ortn = SSLSetProtocolVersionEnabled(ctx, kTLSProtocol1, true);
	if(ortn) {
		printSslErrStr("SSLSetProtocolVersionEnabled", ortn);
		goto cleanup;
	}
	
	/* Restrict to only one CipherSuite */
	ortn = SSLSetEnabledCiphers(ctx, &testParams->cipherSuite, 1);
	if(ortn) {
		printSslErrStr("SSLSetEnabledCiphers", ortn);
		goto cleanup;
	}
	
	ortn = SSLSetConnection(ctx, (SSLConnectionRef)sock);
	if(ortn) {
		printSslErrStr("SSLSetConnection", ortn);
		goto cleanup;
	}
	
	/* These test servers have custom roots, just allow any roots for this test */
	ortn = SSLSetAllowsExpiredCerts(ctx, true);
	if(ortn) {
		printSslErrStr("SSLSetAllowExpiredCerts", ortn);
		goto cleanup;
	}
	ortn = SSLSetAllowsAnyRoot(ctx, true);
	if(ortn) {
		printSslErrStr("SSLSetAllowAnyRoot", ortn);
		goto cleanup;
	}
	
	if(testParams->specCurve != SSL_Curve_None) {
		ortn = SSLSetECDSACurves(ctx, &testParams->specCurve, 1);
		if(ortn) {
			printSslErrStr("SSLSetAllowAnyRoot", ortn);
			goto cleanup;
		}
	}
	
	if(testParams->keychain) {
		char kcPath[2000];
		const char *lbd = getenv("LOCAL_BUILD_DIR");
		if(lbd == NULL) {
			printf("WARNING: no LOCAL_BUILD_DIR env var faound\n");
			lbd = "";
		}
		snprintf(kcPath, 2000, "%s/%s", lbd, testParams->keychain);
		SecKeychainRef kcRef = NULL;
		CFArrayRef certArray = getSslCerts(kcPath, 
			CSSM_FALSE,		// encryptOnly
			CSSM_FALSE,		// completeCertChain
			NULL,			// anchorFile
			&kcRef);
		if(kcRef) {
			/* Unlock it */
			ortn = SecKeychainUnlock(kcRef, 
				strlen(testParams->kcPassword), testParams->kcPassword, 
				true);
			if(ortn) {
				cssmPerror("SecKeychainUnlock", ortn);
				/* oh well */
			}
			CFRelease(kcRef);
		}
		if(certArray == NULL) {
			printf("***WARNING no keychain found at %s\n", kcPath);
		}
		ortn = SSLSetCertificate(ctx, certArray);
		if(ortn) {
			printSslErrStr("SSLSetAllowAnyRoot", ortn);
			goto cleanup;
		}
		CFRelease(certArray);
	}
    do {   
		ortn = SSLHandshake(ctx);
    } while (ortn == errSSLWouldBlock);

    /* convert normal "shutdown" into zero err rtn */
	switch(ortn) {
		case noErr:
			break;
		case errSSLClosedGraceful:
		case errSSLClosedNoNotify:
			ortn = noErr;
			break;
		default:
			dumpErrInfo("SSLHandshake", testParams, ortn);
			goto cleanup;
	}

	
	/* 
	 * Unlike other ping tests we don't bother with a GET - just validate 
	 * the handshake 
	 */
	ortn = SSLGetNegotiatedCipher(ctx, &negCipher);
	if(ortn) {
		dumpErrInfo("SSLHandshake", testParams, ortn);
		goto cleanup;
	}
	
	/* here is really what we're testing */
	if(negCipher != testParams->cipherSuite) {
		printf("***Cipher mismatch for ");
		dumpParams(testParams);
		printf("Negotiated cipher: %s\n", sslGetCipherSuiteString(negCipher));
		ortn = ioErr;
		goto cleanup;
	}
	if(testParams->expCurve != SSL_Curve_None) {
		SSL_ECDSA_NamedCurve actNegCurve;
		ortn = SSLGetNegotiatedCurve(ctx, &actNegCurve);
		if(ortn) {
			printSslErrStr("SSLGetNegotiatedCurve", ortn);
			goto cleanup;
		}
		if(actNegCurve != testParams->expCurve) {
			printf("***Negotiated curve error\n");
			printf("Specified curve: %s\n", sslCurveString(testParams->specCurve));
			printf("Expected  curve: %s\n", sslCurveString(testParams->expCurve));
			printf("Obtained  curve: %s\n", sslCurveString(actNegCurve));
			ortn = ioErr;
			goto cleanup;
		}
	}
	if(testParams->keychain) {
		/* Verify client auth */
		SSLClientCertificateState authState;
		ortn = SSLGetClientCertificateState(ctx, &authState);
		if(ortn) {
			printSslErrStr("SSLGetClientCertificateState", ortn);
			goto cleanup;
		}
		if(authState != kSSLClientCertSent) {
			printf("***Unexpected ClientCertificateState\n");
			printf("   Expected: ClientCertSent\n");
			printf("   Received: %s\n", sslGetClientCertStateString(authState));
			ortn = ioErr;
			goto cleanup;
		}
	}

    ortn = SSLClose(ctx);

cleanup:
	if(sock) {
		endpointShutdown(sock);
	}
	if(ctx) {
	    SSLDisposeContext(ctx);  
	}    
	return (int)ortn;
}


int main(int argc, char **argv)
{
	int 		ourRtn = 0;
	bool		quiet = false;
	int			nonBlocking = false;
	unsigned	minDex = 0;
	unsigned	maxDex = NUM_TEST_PARAMS-1;
	bool		doPause = false;
	
	extern char *optarg;
	int arg;
	while ((arg = getopt(argc, argv, "t:bpqh")) != -1) {
		switch (arg) {
			case 't':
				minDex = maxDex = atoi(optarg);
				if(minDex > (NUM_TEST_PARAMS - 1)) {
					printf("***max test number is %u.\n", (unsigned)NUM_TEST_PARAMS);
					exit(1);
				}
				break;
			case 'q':
				quiet = true;
				break;
			case 'b':
				nonBlocking = true;
				break;
			case 'p':
				doPause = true;
				break;
			default:
				usage(argv);
		}
	}
	if(optind != argc) {
		usage(argv);
	}
	
	#if IGNORE_SIGPIPE
	signal(SIGPIPE, sigpipe);
	#endif
	
	testStartBanner("sslEcdsa", argc, argv);

	if(doPause) {
		fpurge(stdin);
		printf("Pausing at top of loop; CR to continue: ");
		fflush(stdout);
		getchar();
	}
	
	for(unsigned dex=minDex; dex<=maxDex; dex++) {
		const EcdsaTestParams *testParams = &ecdsaTestParams[dex];
		if(!quiet) {
			printf("[%u]: ", dex);
			dumpParams(testParams);
		}
		ourRtn = doSslPing(testParams, quiet, nonBlocking);
		if(ourRtn) {
			if(testError(quiet)) {
				break;
			}
		}
	}

	if(doPause) {
		fpurge(stdin);
		printf("Pausing at end of loop; CR to continue: ");
		fflush(stdout);
		getchar();
	}

	if(!quiet) {
		if(ourRtn == 0) {
			printf("===== sslEcdsa test PASSED =====\n");
		}
		else {
			printf("****sslEcdsa test FAILED\n");
		}
	}
	
	return ourRtn;
}
