/*
 * Copyright (c) 2003-2010 Apple Inc. All Rights Reserved.
 *
 * SSL viewer tool, SecureTransport / OS X version.
 */

#include <Security/SecureTransport.h>
#include <Security/SecureTransportPriv.h>	// for SSLGetPeerSecTrust
#include <Security/SecCertificate.h>
#include <clAppUtils/sslAppUtils.h>
#include <clAppUtils/ioSock.h>
#include <security_cdsa_utils/cuPrintCert.h>
#include <utilLib/fileIo.h>

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <CoreFoundation/CoreFoundation.h>

#define DEFAULT_GETMSG  	"GET"
#define DEFAULT_PATH		"/"
#define DEFAULT_GET_SUFFIX	"HTTP/1.0\r\n\r\n"

#define DEFAULT_HOST   	  	"www.amazon.com"
#define DEFAULT_PORT     	443

#define CFRELEASE(cf)	if (cf) { CFRelease(cf); cf = NULL; }

/* true when using SSLCopyPeerCertificates() per Radar 3311892 */
#define USE_COPY_PEER_CERTS		1

static void usageNorm(char **argv)
{
    printf("Usage: %s [hostname|-] [path] [option ...]\n", argv[0]);
    printf("       %s hostname [path] [option ...]\n", argv[0]);
	printf("Specifying '-' for hostname, or no args, uses default of %s.\n",
		DEFAULT_HOST);
	printf("Optional path argument must start with leading '/'.\n");
    printf("Options:\n");
    printf("   e           Allow Expired Certs\n");
    printf("   E           Allow Expired Roots\n");
    printf("   r           Allow any root cert\n");
	printf("   c           Display peer certs\n");
	printf("   d           Display received data\n");
	printf("   S           Display enabled cipher suites\n");
	printf("   2           SSLv2 only (default is TLSv1)\n");
	printf("   3           SSLv3 only w/SSLv2 enabled (default is TLSv1)\n");
	printf("   t           TLSv1 only w/SSLv2,SSLv3 enabled (this is the default)\n");
	printf("   L           all - TLSv1, SSLv3, SSLv2 (default = TLSv1)\n");
	printf("   o           TLSv1, SSLv3 use kSSLProtocol__X__Only\n");
	printf("   g={prot...} Specify legal protocols; prot = any combo of"
							" [23t]\n");
	printf("   k=keychain  Contains (client|server) cert and keys. Optional.\n");
	printf("   l=loopCount Perform loopCount ops (default = 1)\n");
	printf("   P=port      Default = %d\n", DEFAULT_PORT); 
	printf("   p           Pause after each loop\n");
	printf("   q           Quiet/diagnostic mode (site names and errors"
								" only)\n");
	printf("   a fileName  Add fileName to list of trusted roots\n");
	printf("   A fileName  fileName is ONLY trusted root\n");
    printf("   Z fileName  fileName is a trusted leaf cert\n");
	printf("   x           Disable Cert Verification\n");
	printf("   z=password  Unlock client keychain with password.\n");
	printf("   8           Complete cert chains (default is out cert is a root)\n");
	printf("   s           Silent\n");
	printf("   V           Verbose\n");
	printf("   h           Help\n");
	printf("   hv          More, verbose help\n");
}

static void usageVerbose(char **argv)
{
    usageNorm(argv);
	printf("Obscure Usage:\n");
	printf("   u           kSSLProtocolUnknown only (TLSv1)\n");
	printf("   M           Manual cert verification via "
							"SecTrustEvaluate\n");
	printf("   f fileBase  Write Peer Certs to fileBase*\n");
	printf("   D fileBase  Write DNList to fileBase*\n");
	printf("   C=cipherSuite (e=40-bit d=DES D=40-bit DES 3=3DES 4=RC4"
								"$=40-bit RC4\n"
		   "                  2=RC2 a=AES128 A=AES256 h=DH H=Anon DH r=DHE/RSA s=DH/DSS\n"
		   "                  n=RSA/NULL E=ECDHE F=ECDH\n");
	printf("   y=keychain  Encryption-only cert and keys. Optional.\n");
	printf("   K           Keep connected until server disconnects\n");
	printf("   n           Require closure notify message in TLSv1, "
								"SSLv3 mode (implies K)\n");
	printf("   R           Disable resumable session support\n");
	printf("   i=timeout   Session cache timeout\n");
	printf("   b           Non-blocking I/O\n");
	printf("   v           Verify negotiated protocol equals attempted\n");
	printf("   m=[23t]     Max protocol supported as specified; implies "
								"v\n");
	printf("   T=[nrsj]    Verify client cert state = "
								"none/requested/sent/rejected\n");
	printf("   H           allow hostname spoofing\n");
	printf("   F=vfyHost   Verify certs with specified host name\n");
	printf("   G=getMsg    Specify entire GET, POST, etc.\n");
	printf("   I           Interactive client authentication\n");
	printf("   N           Log handshake timing\n");
	printf("   4           Disable anonymous ciphers\n");
	printf("   7           Pause only after first loop\n");
	exit(1);
}

static void usage(char **argv)
{
    usageNorm(argv);
	exit(1);
}

/* 
 * Arguments to top-level sslPing()
 */
typedef struct {
	SSLProtocol				tryVersion;			// only used if acceptedProts NULL
												// uses SSLSetProtocolVersion
	char					*acceptedProts;		// optional, any combo of {2,3,t}
												// uses SSLSetProtocolVersionEnabled
	const char				*hostName;			// e.g., "www.amazon.com"
	const char				*vfyHostName;		// use this for cert vfy if non-NULL,
												//   else use hostName
	unsigned short			port;
	const char				*getMsg;			// e.g., 
												//   "GET / HTTP/1.0\r\n\r\n" 
	bool					allowExpired;
	bool					allowAnyRoot;
	bool					allowExpiredRoot;
	bool					disableCertVerify;
	bool					manualCertVerify;
	bool					dumpRxData;			// display server data
	char					cipherRestrict;		// '2', 'd'. etc...; '\0' for 
												//   no restriction
	bool					keepConnected;
	bool					requireNotify;		// require closure notify 
												//   in V3 mode
	bool					resumableEnable;
	bool					allowHostnameSpoof;
	bool					nonBlocking;
	char					*anchorFile;
    char					*trustedLeafFile;
	bool					replaceAnchors;
	bool					interactiveAuth;
	CFArrayRef				clientCerts;		// optional
	CFArrayRef				encryptClientCerts;	// optional
	uint32					sessionCacheTimeout;// optional
	bool					disableAnonCiphers;
	bool					showCipherSuites;
	bool					quiet;				// minimal stdout
	bool					silent;				// no stdout
	bool					verbose;
	SSLProtocol				negVersion;			// RETURNED
	SSLCipherSuite			negCipher;			// RETURNED
	CFArrayRef				peerCerts;			// mallocd & RETURNED
	SecTrustRef				peerTrust;			// RETURNED
	SSLClientCertificateState certState;		// RETURNED
	SSLClientAuthenticationType authType;		// RETURNED
	CFArrayRef				dnList;				// RETURNED
	char					*password;			// optional to open clientCerts
	char					**argv;
	Boolean					sessionWasResumed;
	unsigned char			sessionID[MAX_SESSION_ID_LENGTH];
	size_t					sessionIDLength;
	CFAbsoluteTime			handshakeTimeOp;		// time for this op
	CFAbsoluteTime			handshakeTimeFirst;		// time for FIRST op, not averaged
	CFAbsoluteTime			handshakeTimeTotal;		// time for all ops except first
	unsigned				numHandshakes;		

} sslPingArgs;

#include <signal.h>
static void sigpipe(int sig) 
{ 
	fflush(stdin);
	printf("***SIGPIPE***\n");
}

/* 
 * Start up a CFRunLoop. This is needed to field keychain event callbacks, used
 * to maintain root cert cache coherency. 
 */
 
/* first we need something to register so we *have* a run loop */
static OSStatus kcCacheCallback (
   SecKeychainEvent keychainEvent,
   SecKeychainCallbackInfo *info,
   void *context)
{
	return noErr;
}

/* main thread has to wait for this to be set to know a run loop has been set up */
static int runLoopInitialized = 0;

/* this is the thread which actually runs the CFRunLoop */
void *cfRunLoopThread(void *arg)
{
	OSStatus ortn = SecKeychainAddCallback(kcCacheCallback, 
		kSecTrustSettingsChangedEventMask, NULL);
	if(ortn) {
		printf("registerCacheCallbacks: SecKeychainAddCallback returned %d", (int32_t)ortn);
		/* Not sure how this could ever happen - maybe if there is no run loop active? */
		return NULL;
	}
	runLoopInitialized = 1;
	CFRunLoopRun();
	/* should not be reached */
	printf("\n*** Hey! CFRunLoopRun() exited!***\n");
	return NULL;
}

static int startCFRunLoop()
{
	pthread_t runLoopThread;
	
	int result = pthread_create(&runLoopThread, NULL, cfRunLoopThread, NULL);
	if(result) {
		printf("***pthread_create returned %d, aborting\n", result);
		return -1;
	}
	return 0;
}

/* 
 * Snag a copy of current connection's peer certs so we can 
 * examine them later after the connection is closed.
 * SecureTransport actually does the create and retain for us. 
 */
static OSStatus copyPeerCerts(
	SSLContext 	*ctx,
	CFArrayRef	*peerCerts)		// mallocd & RETURNED
{
	#if USE_COPY_PEER_CERTS
	OSStatus ortn = SSLCopyPeerCertificates(ctx, peerCerts);
	#else
	OSStatus ortn = SSLGetPeerCertificates(ctx, peerCerts);
	#endif
	if(ortn) {
		printf("***Error obtaining peer certs: %s\n",
			sslGetSSLErrString(ortn));
	}
	return ortn;
}

/* free the cert array obtained via SSLGetPeerCertificates() */
/* necessary due to a buggy SSLGetPeerCertificates which really should
 * release its certs after they get added to this array */
static void	freePeerCerts(
	CFArrayRef			peerCerts)
{
	if(peerCerts == NULL) {
		return;
	}

	#if USE_COPY_PEER_CERTS
	
	/* Voila! Problem fixed. */
	CFRelease(peerCerts);
	return;
	
	#else 
	CFIndex numCerts;
	SecCertificateRef certData;
	CFIndex i;
	
	numCerts = CFArrayGetCount(peerCerts);
	for(i=0; i<numCerts; i++) {
		certData = (SecCertificateRef)CFArrayGetValueAtIndex(peerCerts, i);
		CFRelease(certData);
	}
	CFRelease(peerCerts);
	#endif	/* USE_COPY_PEER_CERTS */
}	

/*
 * Manually evaluate session's SecTrustRef.
 */
#define SSL_SEC_TRUST	1

static OSStatus sslEvaluateTrust(
	SSLContext	*ctx,
	bool		verbose, 
	bool		silent,
	CFArrayRef	*peerCerts)		// fetched and retained
{
	OSStatus ortn;
	SecTrustRef secTrust = NULL;

	#if	SSL_SEC_TRUST
	ortn = SSLGetPeerSecTrust(ctx, &secTrust);
	#else
	ortn = unimpErr;
	#endif
	if(ortn) {
		printf("\n***Error obtaining peer SecTrustRef: %s\n",
			sslGetSSLErrString(ortn));
		return ortn;
	}
	if(secTrust == NULL) {
		/* this is the normal case for resumed sessions, in which 
		 * no cert evaluation is performed */
		if(!silent) {
			printf("...No SecTrust available - this is a resumed session, right?\n");
		}
		return noErr;
	}
	SecTrustResultType	secTrustResult;
	ortn = SecTrustEvaluate(secTrust, &secTrustResult);
	if(ortn) {
		printf("\n***Error on SecTrustEvaluate: %d\n", (int)ortn);
		return ortn;
	}
	if(verbose) {
		const char *res = NULL;
		switch(secTrustResult) {
			case kSecTrustResultInvalid: 
				res = "kSecTrustResultInvalid"; break;
			case kSecTrustResultProceed: 
				res = "kSecTrustResultProceed"; break;
			case kSecTrustResultConfirm: 
				res = "kSecTrustResultConfirm"; break;
			case kSecTrustResultDeny: 
				res = "kSecTrustResultDeny"; break;
			case kSecTrustResultUnspecified: 
				res = "kSecTrustResultUnspecified"; break;
			case kSecTrustResultRecoverableTrustFailure: 
				res = "kSecTrustResultRecoverableTrustFailure"; break;
			case kSecTrustResultFatalTrustFailure: 
				res = "kSecTrustResultFatalTrustFailure"; break;
			case kSecTrustResultOtherError: 
				res = "kSecTrustResultOtherError"; break;
			default:
				res = "UNKNOWN"; break;
		}
		printf("\nSecTrustEvaluate(): secTrustResult %s\n", res);
	}
	
	switch(secTrustResult) {
		case kSecTrustResultUnspecified:
			/* cert chain valid, no special UserTrust assignments */
		case kSecTrustResultProceed:
			/* cert chain valid AND user explicitly trusts this */
			break;
		default:
			printf("\n***SecTrustEvaluate reported secTrustResult %d\n", 
				(int)secTrustResult);
			ortn = errSSLXCertChainInvalid;
			break;
	}
	
	*peerCerts = NULL;

	/* one more thing - get peer certs in the form of an evidence chain */
	CSSM_TP_APPLE_EVIDENCE_INFO *dummyEv;
	OSStatus thisRtn = SecTrustGetResult(secTrust, &secTrustResult,
		peerCerts, &dummyEv);
	if(thisRtn) {
		printSslErrStr("SecTrustGetResult", thisRtn);
	}
	#if		!USE_COPY_PEER_CERTS
	else {
		/* workaround for the fact that SSLGetPeerCertificates()
		 * leaves a retain count on each element in the returned array,
		 * requiring us to do a release on each cert.
		 */
		CFIndex numCerts = CFArrayGetCount(*peerCerts);
		for(CFIndex dex=0; dex<numCerts; dex++) {
			CFRetain(CFArrayGetValueAtIndex(*peerCerts, dex));
		}
	}
	#endif	/* !USE_COPY_PEER_CERTS */
	return ortn;
}

static void sslShowEnabledCipherSuites(
	SSLContextRef ctx)
{
	OSStatus status;
	SSLCipherSuite *ciphers;
	size_t numCiphers, totalCiphers;
	const char *c;
	unsigned int i;
	
	status = SSLGetNumberSupportedCiphers(ctx, &totalCiphers);
	status = SSLGetNumberEnabledCiphers(ctx, &numCiphers);
	ciphers = (SSLCipherSuite *)malloc(sizeof(SSLCipherSuite) * numCiphers);
	status = SSLGetEnabledCiphers(ctx, ciphers, &numCiphers);

	printf("   Total enabled ciphers  : %ld of %ld\n", numCiphers, totalCiphers);
	
	for(i=0; i<numCiphers; i++) {
		switch(ciphers[i]) {
			case SSL_NULL_WITH_NULL_NULL:				c="SSL_NULL_WITH_NULL_NULL"; break;
			case SSL_RSA_WITH_NULL_MD5:					c="SSL_RSA_WITH_NULL_MD5"; break;
			case SSL_RSA_WITH_NULL_SHA:					c="SSL_RSA_WITH_NULL_SHA"; break;
			case SSL_RSA_EXPORT_WITH_RC4_40_MD5:		c="SSL_RSA_EXPORT_WITH_RC4_40_MD5"; break;
			case SSL_RSA_WITH_RC4_128_MD5:				c="SSL_RSA_WITH_RC4_128_MD5"; break;
			case SSL_RSA_WITH_RC4_128_SHA:				c="SSL_RSA_WITH_RC4_128_SHA"; break;
			case SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5:	c="SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5"; break;
			case SSL_RSA_WITH_IDEA_CBC_SHA:				c="SSL_RSA_WITH_IDEA_CBC_SHA"; break;
			case SSL_RSA_EXPORT_WITH_DES40_CBC_SHA:		c="SSL_RSA_EXPORT_WITH_DES40_CBC_SHA"; break;
			case SSL_RSA_WITH_DES_CBC_SHA:				c="SSL_RSA_WITH_DES_CBC_SHA"; break;
			case SSL_RSA_WITH_3DES_EDE_CBC_SHA:			c="SSL_RSA_WITH_3DES_EDE_CBC_SHA"; break;
			case SSL_DH_DSS_EXPORT_WITH_DES40_CBC_SHA:	c="SSL_DH_DSS_EXPORT_WITH_DES40_CBC_SHA"; break;
			case SSL_DH_DSS_WITH_DES_CBC_SHA:			c="SSL_DH_DSS_WITH_DES_CBC_SHA"; break;
			case SSL_DH_DSS_WITH_3DES_EDE_CBC_SHA:		c="SSL_DH_DSS_WITH_3DES_EDE_CBC_SHA"; break;
			case SSL_DH_RSA_EXPORT_WITH_DES40_CBC_SHA:	c="SSL_DH_RSA_EXPORT_WITH_DES40_CBC_SHA"; break;
			case SSL_DH_RSA_WITH_DES_CBC_SHA:			c="SSL_DH_RSA_WITH_DES_CBC_SHA"; break;
			case SSL_DH_RSA_WITH_3DES_EDE_CBC_SHA:		c="SSL_DH_RSA_WITH_3DES_EDE_CBC_SHA"; break;
			case SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA:	c="SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA"; break;
			case SSL_DHE_DSS_WITH_DES_CBC_SHA:			c="SSL_DHE_DSS_WITH_DES_CBC_SHA"; break;
			case SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA:		c="SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA"; break;
			case SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA:	c="SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA"; break;
			case SSL_DHE_RSA_WITH_DES_CBC_SHA:			c="SSL_DHE_RSA_WITH_DES_CBC_SHA"; break;
			case SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA:		c="SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA"; break;
			case SSL_DH_anon_EXPORT_WITH_RC4_40_MD5:	c="SSL_DH_anon_EXPORT_WITH_RC4_40_MD5"; break;
			case SSL_DH_anon_WITH_RC4_128_MD5:			c="SSL_DH_anon_WITH_RC4_128_MD5"; break;
			case SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA:	c="SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA"; break;
			case SSL_DH_anon_WITH_DES_CBC_SHA:			c="SSL_DH_anon_WITH_DES_CBC_SHA"; break;
			case SSL_DH_anon_WITH_3DES_EDE_CBC_SHA:		c="SSL_DH_anon_WITH_3DES_EDE_CBC_SHA"; break;
			case SSL_FORTEZZA_DMS_WITH_NULL_SHA:		c="SSL_FORTEZZA_DMS_WITH_NULL_SHA"; break;
			case SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA:c="SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA"; break;
	
			/* TLS addenda using AES, per RFC 3268 */
			case TLS_RSA_WITH_AES_128_CBC_SHA:			c="TLS_RSA_WITH_AES_128_CBC_SHA"; break;
			case TLS_DH_DSS_WITH_AES_128_CBC_SHA:		c="TLS_DH_DSS_WITH_AES_128_CBC_SHA"; break;
			case TLS_DH_RSA_WITH_AES_128_CBC_SHA:		c="TLS_DH_RSA_WITH_AES_128_CBC_SHA"; break;
			case TLS_DHE_DSS_WITH_AES_128_CBC_SHA:		c="TLS_DHE_DSS_WITH_AES_128_CBC_SHA"; break;
			case TLS_DHE_RSA_WITH_AES_128_CBC_SHA:		c="TLS_DHE_RSA_WITH_AES_128_CBC_SHA"; break;
			case TLS_DH_anon_WITH_AES_128_CBC_SHA:		c="TLS_DH_anon_WITH_AES_128_CBC_SHA"; break;
			case TLS_RSA_WITH_AES_256_CBC_SHA:			c="TLS_RSA_WITH_AES_256_CBC_SHA"; break;
			case TLS_DH_DSS_WITH_AES_256_CBC_SHA:		c="TLS_DH_DSS_WITH_AES_256_CBC_SHA"; break;
			case TLS_DH_RSA_WITH_AES_256_CBC_SHA:		c="TLS_DH_RSA_WITH_AES_256_CBC_SHA"; break;
			case TLS_DHE_DSS_WITH_AES_256_CBC_SHA:		c="TLS_DHE_DSS_WITH_AES_256_CBC_SHA"; break;
			case TLS_DHE_RSA_WITH_AES_256_CBC_SHA:		c="TLS_DHE_RSA_WITH_AES_256_CBC_SHA"; break;
			case TLS_DH_anon_WITH_AES_256_CBC_SHA:		c="TLS_DH_anon_WITH_AES_256_CBC_SHA"; break;
	
			/* ECDSA addenda, RFC 4492 */
			case TLS_ECDH_ECDSA_WITH_NULL_SHA:			c="TLS_ECDH_ECDSA_WITH_NULL_SHA"; break;
			case TLS_ECDH_ECDSA_WITH_RC4_128_SHA:		c="TLS_ECDH_ECDSA_WITH_RC4_128_SHA"; break;
			case TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA:	c="TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA"; break;
			case TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA:	c="TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA"; break;
			case TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA:	c="TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA"; break;
			case TLS_ECDHE_ECDSA_WITH_NULL_SHA:			c="TLS_ECDHE_ECDSA_WITH_NULL_SHA"; break;
			case TLS_ECDHE_ECDSA_WITH_RC4_128_SHA:		c="TLS_ECDHE_ECDSA_WITH_RC4_128_SHA"; break;
			case TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA:	c="TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA"; break;
			case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA:	c="TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA"; break;
			case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA:	c="TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA"; break;
			case TLS_ECDH_RSA_WITH_NULL_SHA:			c="TLS_ECDH_RSA_WITH_NULL_SHA"; break;
			case TLS_ECDH_RSA_WITH_RC4_128_SHA:			c="TLS_ECDH_RSA_WITH_RC4_128_SHA"; break;
			case TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA:	c="TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA"; break;
			case TLS_ECDH_RSA_WITH_AES_128_CBC_SHA:		c="TLS_ECDH_RSA_WITH_AES_128_CBC_SHA"; break;
			case TLS_ECDH_RSA_WITH_AES_256_CBC_SHA:		c="TLS_ECDH_RSA_WITH_AES_256_CBC_SHA"; break;
			case TLS_ECDHE_RSA_WITH_NULL_SHA:			c="TLS_ECDHE_RSA_WITH_NULL_SHA"; break;
			case TLS_ECDHE_RSA_WITH_RC4_128_SHA:		c="TLS_ECDHE_RSA_WITH_RC4_128_SHA"; break;
			case TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA:	c="TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA"; break;
			case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA:	c="TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA"; break;
			case TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA:	c="TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA"; break;
			case TLS_ECDH_anon_WITH_NULL_SHA:			c="TLS_ECDH_anon_WITH_NULL_SHA"; break;
			case TLS_ECDH_anon_WITH_RC4_128_SHA:		c="TLS_ECDH_anon_WITH_RC4_128_SHA"; break;
			case TLS_ECDH_anon_WITH_3DES_EDE_CBC_SHA:	c="TLS_ECDH_anon_WITH_3DES_EDE_CBC_SHA"; break;
			case TLS_ECDH_anon_WITH_AES_128_CBC_SHA:	c="TLS_ECDH_anon_WITH_AES_128_CBC_SHA"; break;
			case TLS_ECDH_anon_WITH_AES_256_CBC_SHA:	c="TLS_ECDH_anon_WITH_AES_256_CBC_SHA"; break;
	
			/*
			 * Tags for SSL 2 cipher kinds which are not specified 
			 * for SSL 3. 
			 */
			case SSL_RSA_WITH_RC2_CBC_MD5:				c="SSL_RSA_WITH_RC2_CBC_MD5"; break;
			case SSL_RSA_WITH_IDEA_CBC_MD5:				c="SSL_RSA_WITH_IDEA_CBC_MD5"; break;
			case SSL_RSA_WITH_DES_CBC_MD5:				c="SSL_RSA_WITH_DES_CBC_MD5"; break;
			case SSL_RSA_WITH_3DES_EDE_CBC_MD5:			c="SSL_RSA_WITH_3DES_EDE_CBC_MD5"; break;
			case SSL_NO_SUCH_CIPHERSUITE:
			default:
														c="SSL_NO_SUCH_CIPHERSUITE"; break;
		}
		printf("   %s\n", c);
		fflush(stdout);
	}
	free(ciphers);
}

/* print reply received from server, safely */
static void dumpAscii(
	uint8_t *rcvBuf, 
	uint32_t len)
{
	char *cp = (char *)rcvBuf;
	uint32_t i;
	char c;
	
	for(i=0; i<len; i++) {
		c = *cp++;
		if(c == '\0') {
			break;
		}
		switch(c) {
			case '\n':
				printf("\\n");
				break;
			case '\r':
				printf("\\r");
				break;
			default:
				if(isprint(c) && (c != '\n')) {
					printf("%c", c);
				}
				else {
					printf("<%02X>", ((unsigned)c) & 0xff);
				}
			break;
		}

	}
	printf("\n");
}

/*
 * Perform one SSL diagnostic session. Returns nonzero on error. Normally no
 * output to stdout except initial "connecting to" message, unless there 
 * is a really screwed up error (i.e., something not directly related 
 * to the SSL connection). 
 */
#define RCV_BUF_SIZE		256

static OSStatus sslPing(
	sslPingArgs *pargs)
{
    PeerSpec            peerId;
	otSocket			sock = 0;
    OSStatus            ortn;
    SSLContextRef       ctx = NULL;
    size_t              length;
	size_t				actLen;
    uint8_t             rcvBuf[RCV_BUF_SIZE];
	CFAbsoluteTime		startHandshake;
	CFAbsoluteTime		endHandshake;
	
    pargs->negVersion = kSSLProtocolUnknown;
    pargs->negCipher = SSL_NULL_WITH_NULL_NULL;
    pargs->peerCerts = NULL;
    
	/* first make sure requested server is there */
	ortn = MakeServerConnection(pargs->hostName, pargs->port, pargs->nonBlocking,
		&sock, &peerId);
    if(ortn) {
    	printf("MakeServerConnection returned %d; aborting\n", (int)ortn);
    	return ortn;
    }
	if(pargs->verbose) {
		printf("...connected to server; starting SecureTransport\n");
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
	ortn = SSLSetConnection(ctx, (SSLConnectionRef)sock);
	if(ortn) {
		printSslErrStr("SSLSetConnection", ortn);
		goto cleanup;
	}
	SSLConnectionRef getConn;
	ortn = SSLGetConnection(ctx, &getConn);
	if(ortn) {
		printSslErrStr("SSLGetConnection", ortn);
		goto cleanup;
	}
	if(getConn != (SSLConnectionRef)sock) {
		printf("***SSLGetConnection error\n");
		ortn = paramErr;
		goto cleanup; 
	}
	if(!pargs->allowHostnameSpoof) {
		/* if this isn't set, it isn't checked by AppleX509TP */
		const char *vfyHost = pargs->hostName;
		if(pargs->vfyHostName) {
			/* generally means we're expecting an error */
			vfyHost = pargs->vfyHostName;
		}
		ortn = SSLSetPeerDomainName(ctx, vfyHost, strlen(vfyHost));
		if(ortn) {
			printSslErrStr("SSLSetPeerDomainName", ortn);
			goto cleanup;
		}
	}
	
	/* 
	 * SecureTransport options.
	 */ 
	if(pargs->acceptedProts) {
		ortn = SSLSetProtocolVersionEnabled(ctx, kSSLProtocolAll, false);
		if(ortn) {
			printSslErrStr("SSLSetProtocolVersionEnabled(all off)", ortn);
			goto cleanup;
		}
		for(const char *cp = pargs->acceptedProts; *cp; cp++) {
			SSLProtocol prot;
			switch(*cp) {
				case '2':
					prot = kSSLProtocol2;
					break;
				case '3':
					prot = kSSLProtocol3;
					break;
				case 't':
					prot = kTLSProtocol1;
					break;
				default:
					usage(pargs->argv);
			}
			ortn = SSLSetProtocolVersionEnabled(ctx, prot, true);
			if(ortn) {
				printSslErrStr("SSLSetProtocolVersionEnabled", ortn);
				goto cleanup;
			}
		}
	}
	else {
		ortn = SSLSetProtocolVersion(ctx, pargs->tryVersion);
		if(ortn) {
			printSslErrStr("SSLSetProtocolVersion", ortn);
			goto cleanup;
		} 
		SSLProtocol getVers;
		ortn = SSLGetProtocolVersion(ctx, &getVers);
		if(ortn) {
			printSslErrStr("SSLSetProtocolVersion", ortn);
			goto cleanup;
		}
		if(getVers != pargs->tryVersion) {
			printf("***SSLGetProtocolVersion screwup: try %s  get %s\n",
				sslGetProtocolVersionString(pargs->tryVersion),
				sslGetProtocolVersionString(getVers));
			ortn = paramErr;
			goto cleanup;
		}
	}
	if(pargs->resumableEnable) {
		const void *rtnId = NULL;
		size_t rtnIdLen = 0;
		
		ortn = SSLSetPeerID(ctx, &peerId, sizeof(PeerSpec));
		if(ortn) {
			printSslErrStr("SSLSetPeerID", ortn);
			goto cleanup;
		}
		/* quick test of the get fcn */
		ortn = SSLGetPeerID(ctx, &rtnId, &rtnIdLen);
		if(ortn) {
			printSslErrStr("SSLGetPeerID", ortn);
			goto cleanup;
		}
		if((rtnId == NULL) || (rtnIdLen != sizeof(PeerSpec))) {
			printf("***SSLGetPeerID screwup\n");
		}
		else if(memcmp(&peerId, rtnId, rtnIdLen) != 0) {
			printf("***SSLGetPeerID data mismatch\n");
		}
	}
	if(pargs->allowExpired) {
		ortn = SSLSetAllowsExpiredCerts(ctx, true);
		if(ortn) {
			printSslErrStr("SSLSetAllowExpiredCerts", ortn);
			goto cleanup;
		}
	}
	if(pargs->allowExpiredRoot) {
		ortn = SSLSetAllowsExpiredRoots(ctx, true);
		if(ortn) {
			printSslErrStr("SSLSetAllowsExpiredRoots", ortn);
			goto cleanup;
		}
	}
	if(pargs->disableCertVerify) {
		ortn = SSLSetEnableCertVerify(ctx, false);
		if(ortn) {
			printSslErrStr("SSLSetEnableCertVerify", ortn);
			goto cleanup;
		}
	}
	if(pargs->allowAnyRoot) {
		ortn = SSLSetAllowsAnyRoot(ctx, true);
		if(ortn) {
			printSslErrStr("SSLSetAllowAnyRoot", ortn);
			goto cleanup;
		}
	}
	if(pargs->cipherRestrict != '\0') {
		ortn = sslSetCipherRestrictions(ctx, pargs->cipherRestrict);
		if(ortn) {
			goto cleanup;
		}
	}
	if(pargs->anchorFile) {
		ortn = sslAddTrustedRoot(ctx, pargs->anchorFile, pargs->replaceAnchors);
		if(ortn) {
			printf("***Error obtaining anchor file %s\n", pargs->anchorFile);
			goto cleanup;
		}
	}
    if(pargs->trustedLeafFile) {
        SecCertificateRef leafCertRef = NULL;
        CFMutableArrayRef leafCerts = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
        /* sslReadAnchor is a misnomer; it just creates a SecCertificateRef from a file */
        ortn = sslReadAnchor(pargs->trustedLeafFile, &leafCertRef);
        if (!ortn) {
            CFArrayAppendValue(leafCerts, leafCertRef);
            CFRelease(leafCertRef);
            ortn = SSLSetTrustedLeafCertificates(ctx, leafCerts);
            CFRelease(leafCerts);
        }
        if(ortn) {
            goto cleanup;
        }
    }
	if(pargs->interactiveAuth) {
		/* we want to get errSSLServerAuthCompleted from SSLHandshake on server auth completion */
		SSLSetSessionOption(ctx, kSSLSessionOptionBreakOnServerAuth, true);
		/* we want to get errSSLClientCertRequested from SSLHandshake on client auth request */
		SSLSetSessionOption(ctx, kSSLSessionOptionBreakOnCertRequested, true);
	}	
	else if(pargs->clientCerts) {
		CFArrayRef dummy;
		if(pargs->anchorFile == NULL) {
			/* assume this is a root we want to implicitly trust */
			ortn = addIdentityAsTrustedRoot(ctx, pargs->clientCerts);
			if(ortn) {
				goto cleanup;
			}
		}
		ortn = SSLSetCertificate(ctx, pargs->clientCerts);
		if(ortn) {
			printSslErrStr("SSLSetCertificate", ortn);
			goto cleanup;
		}
		/* quickie test for this new function */
		ortn = SSLGetCertificate(ctx, &dummy);
		if(ortn) {
			printSslErrStr("SSLGetCertificate", ortn);
			goto cleanup;
		}
		if(dummy != pargs->clientCerts) {
			printf("***SSLGetCertificate error\n");
			ortn = ioErr;
			goto cleanup;
		}
	}
	if(pargs->encryptClientCerts) {
		if(pargs->anchorFile == NULL) {
			ortn = addIdentityAsTrustedRoot(ctx, pargs->encryptClientCerts);
			if(ortn) {
				goto cleanup;
			}
		}
		ortn = SSLSetEncryptionCertificate(ctx, pargs->encryptClientCerts);
		if(ortn) {
			printSslErrStr("SSLSetEncryptionCertificate", ortn);
			goto cleanup;
		}
	}
	if(pargs->sessionCacheTimeout) {
		ortn = SSLSetSessionCacheTimeout(ctx, pargs->sessionCacheTimeout);
		if(ortn) {
			printSslErrStr("SSLSetSessionCacheTimeout", ortn);
			goto cleanup;
		}
	}
	if(pargs->disableAnonCiphers) {
		ortn = SSLSetAllowAnonymousCiphers(ctx, false);
		if(ortn) {
			printSslErrStr("SSLSetAllowAnonymousCiphers", ortn);
			goto cleanup;
		}
		/* quickie test of the getter */
		Boolean e;
		ortn = SSLGetAllowAnonymousCiphers(ctx, &e);
		if(ortn) {
			printSslErrStr("SSLGetAllowAnonymousCiphers", ortn);
			goto cleanup;
		}
		if(e) {
			printf("***SSLGetAllowAnonymousCiphers() returned true; expected false\n");
			ortn = ioErr;
			goto cleanup;
		}
	}
	if(pargs->showCipherSuites) {
		sslShowEnabledCipherSuites(ctx);
	}
	/*** end options ***/
	
	if(pargs->verbose) {
		printf("...starting SSL handshake\n");
	}
	startHandshake = CFAbsoluteTimeGetCurrent();
	
    do
    {   ortn = SSLHandshake(ctx);
	    if((ortn == errSSLWouldBlock) && !pargs->silent) {
	    	/* keep UI responsive */ 
	    	sslOutputDot();
	    }
		else if(ortn == errSSLServerAuthCompleted) {
			if(pargs->verbose) {
				printf("...server authentication completed\n");
			}
		}
		else if(ortn == errSSLClientCertRequested) {
			if(pargs->verbose) {
				printf("...received client cert request\n");
			}
			/* %%% could prompt interactively here for client cert to use;
			 * for now, just use the client cert passed on the command line
			 */
			if(pargs->clientCerts) {
				CFArrayRef dummy;
				if(pargs->anchorFile == NULL) {
					/* assume this is a root we want to implicitly trust */
					ortn = addIdentityAsTrustedRoot(ctx, pargs->clientCerts);
					if(ortn) {
						goto cleanup;
					}
				}
				if(pargs->verbose) {
					printf("...setting client certificate\n");
				}
				ortn = SSLSetCertificate(ctx, pargs->clientCerts);
				if(ortn) {
					printSslErrStr("SSLSetCertificate", ortn);
					goto cleanup;
				}
				/* quickie test for this new function */
				ortn = SSLGetCertificate(ctx, &dummy);
				if(ortn) {
					printSslErrStr("SSLGetCertificate", ortn);
					goto cleanup;
				}
				if(dummy != pargs->clientCerts) {
					printf("***SSLGetCertificate error\n");
					ortn = ioErr;
					goto cleanup;
				}
			}
			else {
				printf("***no client certificate specified!\n");
			}
		}
    } while (ortn == errSSLWouldBlock ||
			 ortn == errSSLServerAuthCompleted ||
			 ortn == errSSLClientCertRequested);

	endHandshake = CFAbsoluteTimeGetCurrent();
	pargs->handshakeTimeOp = endHandshake - startHandshake;
	if(pargs->numHandshakes == 0) {
		/* special case, this one is always way longer */
		pargs->handshakeTimeFirst = pargs->handshakeTimeOp;
	}
	else {
		/* normal running total */
		pargs->handshakeTimeTotal += pargs->handshakeTimeOp;
	}
	pargs->numHandshakes++;
	
	/* this works even if handshake failed due to cert chain invalid */
	CFRELEASE(pargs->peerCerts);
	if(!pargs->manualCertVerify) {
		copyPeerCerts(ctx, &pargs->peerCerts);
	}
	else {
		/* else fetched via SecTrust later */
		pargs->peerCerts = NULL;
	}
	
    ortn = SSLCopyPeerTrust(ctx, &pargs->peerTrust);
    if(ortn) {
        printf("***SSLCopyPeerTrust error %d\n", (int)ortn);
        pargs->peerTrust = NULL;
    }

	/* ditto */
	SSLGetClientCertificateState(ctx, &pargs->certState);
	SSLGetNegotiatedClientAuthType(ctx, &pargs->authType);
	SSLGetNegotiatedCipher(ctx, &pargs->negCipher);
	SSLGetNegotiatedProtocolVersion(ctx, &pargs->negVersion);
	CFRELEASE(pargs->dnList);
	SSLCopyDistinguishedNames(ctx, &pargs->dnList);
	pargs->sessionIDLength = MAX_SESSION_ID_LENGTH;
	SSLGetResumableSessionInfo(ctx, &pargs->sessionWasResumed, pargs->sessionID,
		&pargs->sessionIDLength);
	if(pargs->manualCertVerify) {
		OSStatus certRtn = sslEvaluateTrust(ctx, pargs->verbose, pargs->silent,
			&pargs->peerCerts);
		if(certRtn && !ortn ) {
			ortn = certRtn;
		}
	}
	
    if(ortn) {
		if(!pargs->silent) {
			printf("\n");
		}
    	goto cleanup;
    }

	if(pargs->verbose) {
		printf("...SSL handshake complete\n");
	}

	/* Write our GET request */
	length = strlen(pargs->getMsg);
	ortn = SSLWrite(ctx, pargs->getMsg, length, &actLen);
	if(ortn) {
		printf("***SSLWrite error: %d\n", (int)ortn);
	} else if((actLen > 0) && pargs->dumpRxData) {
		dumpAscii((uint8_t*)pargs->getMsg, actLen);
	}
	
	/* 
	 * Try to snag RCV_BUF_SIZE bytes. Exit if (!keepConnected and we get any data
	 * at all), or (keepConnected and err != (none, wouldBlock)).
	 */
    while (ortn == noErr) {   
		actLen = 0;
		if(pargs->dumpRxData) {
			size_t avail = 0;
			
			ortn = SSLGetBufferedReadSize(ctx, &avail);
			if(ortn) {
				printf("***SSLGetBufferedReadSize error\n");
				break;
			}
			if(avail != 0) {
				printf("\n%d bytes available: ", (int)avail);
			}
		}
        ortn = SSLRead(ctx, rcvBuf, RCV_BUF_SIZE, &actLen);
        if((actLen == 0) && !pargs->silent) {
        	sslOutputDot();
        }
        if((actLen == 0) && (ortn == noErr)) {
			printf("***Radar 2984932 confirmed***\n");
		}
        if (ortn == errSSLWouldBlock) {
			/* for this loop, these are identical */
            ortn = noErr;
        }
		if(ortn == errSSLServerAuthCompleted ||
		   ortn == errSSLClientCertRequested) {
			/* should never get these once the handshake is complete */
			printf("***SSLRead returned unexpected handshake error!\n");
		}
		
		if((actLen > 0) && pargs->dumpRxData) {
			dumpAscii(rcvBuf, actLen);
		}
		if(ortn != noErr) {
			/* connection closed by server or by error */
			break;
		}
		if(!pargs->keepConnected && (actLen > 0)) {
        	/* good enough, we connected */
        	break;
        }
    }
	if(!pargs->silent) {
		printf("\n");
	}

	/* snag these again in case of renegotiate */
	SSLGetClientCertificateState(ctx, &pargs->certState);
	SSLGetNegotiatedCipher(ctx, &pargs->negCipher);
	SSLGetNegotiatedProtocolVersion(ctx, &pargs->negVersion);
	CFRELEASE(pargs->dnList);
	SSLCopyDistinguishedNames(ctx, &pargs->dnList);
	
    /* convert normal "shutdown" into zero err rtn */
	if(ortn == errSSLClosedGraceful) {
		ortn = noErr;
	}
	if((ortn == errSSLClosedNoNotify) && !pargs->requireNotify) {
		/* relaxed disconnect rules */
		ortn = noErr;
	}
cleanup:
	/*
	 * always do close, even on error - to flush outgoing write queue 
	 */
	OSStatus cerr = SSLClose(ctx);
	if(ortn == noErr) {
		ortn = cerr;
	}
	if(sock) {
		endpointShutdown(sock);
	}
	if(ctx) {
	    SSLDisposeContext(ctx);  
	}    
	return ortn;
}

static void showPeerCerts(
	CFArrayRef			peerCerts,
	bool				verbose)
{
	CFIndex numCerts;
	SecCertificateRef certRef;
	OSStatus ortn;
	CSSM_DATA certData;
	CFIndex i;
	
	if(peerCerts == NULL) {
		return;
	}
	numCerts = CFArrayGetCount(peerCerts);
	for(i=0; i<numCerts; i++) {
		certRef = (SecCertificateRef)CFArrayGetValueAtIndex(peerCerts, i);
		ortn = SecCertificateGetData(certRef, &certData);
		if(ortn) {
			printf("***SecCertificateGetData returned %d\n", (int)ortn);
			continue;
		}
		printf("\n================== Server Cert %lu ===================\n\n", i);
		printCert(certData.Data, certData.Length, verbose);
		printf("\n=============== End of Server Cert %lu ===============\n", i);
	}
}

static void writePeerCerts(
	CFArrayRef			peerCerts,
	const char			*fileBase)
{
	CFIndex numCerts;
	SecCertificateRef certRef;
	OSStatus ortn;
	CSSM_DATA certData;
	CFIndex i;
	char fileName[100];
	
	if(peerCerts == NULL) {
		return;
	}
	numCerts = CFArrayGetCount(peerCerts);
	for(i=0; i<numCerts; i++) {
		sprintf(fileName, "%s%02d.cer", fileBase, (int)i);
		certRef = (SecCertificateRef)CFArrayGetValueAtIndex(peerCerts, i);
		ortn = SecCertificateGetData(certRef, &certData);
		if(ortn) {
			printf("***SecCertificateGetData returned %d\n", (int)ortn);
			continue;
		}
		cspWriteFile(fileName, certData.Data, certData.Length);
	}
	printf("...wrote %lu certs to fileBase %s\n", numCerts, fileBase);
}

static void writeDnList(
	CFArrayRef			dnList,
	const char			*fileBase)
{
	CFIndex numDns;
	CFDataRef cfDn;
	CFIndex i;
	char fileName[100];
	
	if(dnList == NULL) {
		return;
	}
	numDns = CFArrayGetCount(dnList);
	for(i=0; i<numDns; i++) {
		sprintf(fileName, "%s%02d.der", fileBase, (int)i);
		cfDn = (CFDataRef)CFArrayGetValueAtIndex(dnList, i);
		cspWriteFile(fileName, CFDataGetBytePtr(cfDn), CFDataGetLength(cfDn));
	}
	printf("...wrote %lu RDNs to fileBase %s\n", numDns, fileBase);
}

/*
 * Show result of an sslPing().
 * Assumes the following from sslPingArgs:
 *
 *		verbose
 *		tryVersion
 *		acceptedProts
 *		negVersion
 *		negCipher
 *		peerCerts
 *		certState
 *		authType
 * 		sessionWasResumed
 *		sessionID
 *		sessionIDLength
 *		handshakeTime
 */
static void showSSLResult(
	const sslPingArgs	&pargs,
	OSStatus			err,
	bool				displayPeerCerts,
	const char			*fileBase,		// non-NULL: write certs to file
	const char			*dnFileBase)	// non-NULL: write DNList to file
{
	CFIndex numPeerCerts;
	
	printf("\n");
	
	if(pargs.acceptedProts) {
		printf("   Allowed SSL versions   : %s\n", pargs.acceptedProts);
	}
	else {
		printf("   Attempted  SSL version : %s\n", 
			sslGetProtocolVersionString(pargs.tryVersion));
	}
	
	printf("   Result                 : %s\n", sslGetSSLErrString(err));
	printf("   Negotiated SSL version : %s\n", 
		sslGetProtocolVersionString(pargs.negVersion));
	printf("   Negotiated CipherSuite : %s\n",
		sslGetCipherSuiteString(pargs.negCipher));
	if(pargs.certState != kSSLClientCertNone) {
		printf("   Client Cert State      : %s\n",
			sslGetClientCertStateString(pargs.certState));
		printf("   Client Auth Type       : %s\n",
			sslGetClientAuthTypeString(pargs.authType));
	}
	if(pargs.verbose) {
		printf("   Resumed Session        : ");
		if(pargs.sessionWasResumed) {
			for(unsigned dex=0; dex<pargs.sessionIDLength; dex++) {
				printf("%02X ", pargs.sessionID[dex]);
				if(((dex % 8) == 7) && (dex != (pargs.sessionIDLength - 1))) {
					printf("\n                            ");
				}
			}
			printf("\n");
		}
		else {
			printf("NOT RESUMED\n");
		}
		printf("   Handshake time         : %f seconds\n", pargs.handshakeTimeOp);
	}
	if(pargs.peerCerts == NULL) {
		numPeerCerts = 0;
	}
	else {
		numPeerCerts = CFArrayGetCount(pargs.peerCerts);
	}
	printf("   Number of server certs : %lu\n", numPeerCerts);
	if(numPeerCerts != 0) {
		if(displayPeerCerts) {
			showPeerCerts(pargs.peerCerts, false);
		}
		if(fileBase != NULL) {
			writePeerCerts(pargs.peerCerts, fileBase);
		}
	}
	if(dnFileBase != NULL) {
		writeDnList(pargs.dnList, dnFileBase);
	}
	
	printf("\n");
}

static int verifyProtocol(
	bool		verifyProt,
	SSLProtocol	maxProtocol,
	SSLProtocol	reqProtocol,
	SSLProtocol negProtocol)
{
	if(!verifyProt) {
		return 0;
	}
	if(reqProtocol > maxProtocol) {
		/* known not to support this attempt, relax */
		reqProtocol = maxProtocol;
	}
	if(reqProtocol != negProtocol) {
		printf("***Expected protocol %s; negotiated %s\n",
			sslGetProtocolVersionString(reqProtocol),
			sslGetProtocolVersionString(negProtocol));
		return 1;
	}
	else {
		return 0;
	}
}

static int verifyClientCertState(
	bool						verifyCertState,
	SSLClientCertificateState	expectState,
	SSLClientCertificateState	gotState)
{
	if(!verifyCertState) {
		return 0;
	}
	if(expectState == gotState) {
		return 0;
	}
	printf("***Expected clientCertState %s; got %s\n",
		sslGetClientCertStateString(expectState),
		sslGetClientCertStateString(gotState));
	return 1;
}

/*
 * Free everything allocated by sslPing in an sslPingArgs. 
 * Mainly for looping and malloc debugging.
 */
static void freePingArgs(
						 sslPingArgs *pargs)
{
	freePeerCerts(pargs->peerCerts);
	pargs->peerCerts = NULL;
	CFRELEASE(pargs->peerTrust);
	CFRELEASE(pargs->dnList);
	/* more, later, for client retry/identity fetch */
}

static SSLProtocol charToProt(
	char c,			// 2, 3, t
	char **argv)
{
	switch(c) {
		case '2':
			return kSSLProtocol2;
		case '3':
			return kSSLProtocol3;
		case 't':
			return kTLSProtocol1;
		default:
			usage(argv);
	}
	/* NOT REACHED */
	return kSSLProtocolUnknown;
}

int main(int argc, char **argv)
{   
    OSStatus            err;
	int					arg;
	char 				*argp;
	char				getMsg[300];
	char				fullFileBase[100];
	int					ourRtn = 0;			// exit status - sum of all errors
	unsigned			loop;
	SecKeychainRef		serverKc = nil;
	SecKeychainRef		encryptKc = nil;
	sslPingArgs			pargs;
	
	/* user-spec'd parameters */
	char				*getPath = (char *)DEFAULT_PATH;
	char				*fileBase = NULL;
	bool				displayCerts = false;
	bool				doSslV2 = false;
	bool				doSslV3 = false;
	bool				doTlsV1 = true;
	bool				protXOnly = false;	// kSSLProtocol3Only, kTLSProtocol1Only
	bool				doProtUnknown = false;
	unsigned			loopCount = 1;
	bool				doPause = false;
	bool				pauseFirstLoop = false;
	bool				verifyProt = false;
	SSLProtocol			maxProtocol = kTLSProtocol1;	// for verifying negotiated 
														// protocol
	char				*acceptedProts = NULL;
	char				*keyChainName = NULL;
	char				*encryptKeyChainName = NULL;
	char				*getMsgSpec = NULL;
	bool				vfyCertState = false;
	SSLClientCertificateState expectCertState;
	bool				displayHandshakeTimes = false;
	bool				completeCertChain = false;
	char				*dnFileBase = NULL;

	/* special case - one arg of "h" or "-h" or "hv" */
	if(argc == 2) {
	    if((strcmp(argv[1], "h") == 0) || (strcmp(argv[1], "-h") == 0)) {
			usage(argv);
		}
		if(strcmp(argv[1], "hv") == 0) {
			usageVerbose(argv);
		}
	}
	
	/* set up defaults */
	memset(&pargs, 0, sizeof(sslPingArgs));
	pargs.hostName = DEFAULT_HOST;
	pargs.port = DEFAULT_PORT;
	pargs.resumableEnable = true;
	pargs.argv = argv;
	
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		if(arg == 1) {
			/* first arg, is always hostname; '-' means default */
			if(argp[0] != '-') {
				pargs.hostName = argp;
			}
			continue;
		}
		if(argp[0] == '/') {
			/* path always starts with leading slash */
			getPath = argp;
			continue;
		}
		/* options */
		switch(argp[0]) {
			case 'e':
				pargs.allowExpired = true;
				break;
			case 'E':
				pargs.allowExpiredRoot = true;
				break;
			case 'x':
				pargs.disableCertVerify = true;
				break;
			case 'M':
				pargs.disableCertVerify = true;	// implied
				pargs.manualCertVerify = true;
				break;
			case 'I':
				pargs.interactiveAuth = true;
				break;
			case 'a':
				if(++arg == argc)  {
					/* requires another arg */
					usage(argv);
				}
				pargs.anchorFile = argv[arg];
				break;
			case 'A':
				if(++arg == argc)  {
					/* requires another arg */
					usage(argv);
				}
				pargs.anchorFile = argv[arg];
				pargs.replaceAnchors = true;
				break;
			case 'Z':
				if(++arg == argc)  {
					/* requires another arg */
					usage(argv);
				}
				pargs.trustedLeafFile = argv[arg];
				break;
			case 'r':
				pargs.allowAnyRoot = true;
				break;
			case 'd':
				pargs.dumpRxData = true;
				break;
			case 'c':
				displayCerts = true;
				break;
			case 'f':
				if(++arg == argc)  {
					/* requires another arg */
					usage(argv);
				}
				fileBase = argv[arg];
				break;
			case 'C':
				pargs.cipherRestrict = argp[2];
				break;
			case 'S':
				pargs.showCipherSuites = true;
				break;
			case '2':
				doSslV3 = doTlsV1 = false;
				doSslV2 = true;
				break;
			case '3':
				doSslV2 = doTlsV1 = false;
				doSslV3 = true;
				break;
			case 't':
				/* currently the default... */
				doSslV2 = doSslV3 = false;
				doTlsV1 = true;
				break;
			case 'L':
				doSslV2 = doSslV3 = doTlsV1 = true;
				break;
			case 'o':
				protXOnly = true;
				break;
			case 'u':
				doSslV2 = doSslV3 = doTlsV1 = false;
				doProtUnknown = true;
				break;
			case 'K':
				pargs.keepConnected = true;
				break;
			case 'n':
				pargs.requireNotify = true;
				pargs.keepConnected = true;
				break;
			case 'R':
				pargs.resumableEnable = false;
				break;
			case 'b':
				pargs.nonBlocking = true;
				break;
			case 'v':
				verifyProt = true;
				break;
			case 'm':
				if(argp[1] != '=') {
					usage(argv);
				}
				verifyProt = true;		// implied
				maxProtocol = charToProt(argp[2], argv);
				break;
			case 'g':
				if(argp[1] != '=') {
					usage(argv);
				}
				acceptedProts = &argp[2];
				doSslV3 = doSslV2 = doTlsV1 = false;
				break;
			case 'l':
				loopCount = atoi(&argp[2]);
				if(loopCount == 0) {
					printf("***bad loopCount\n");
					usage(argv);
				}
				break;
			case 'P':
				pargs.port = atoi(&argp[2]);
				break;
			case 'H':
				pargs.allowHostnameSpoof = true;
				break;
			case 'F':
				pargs.vfyHostName = &argp[2];
				break;
			case 'k':
				keyChainName = &argp[2];
				break;
			case 'y':
				encryptKeyChainName = &argp[2];
				break;
			case 'G':
				getMsgSpec = &argp[2];
				break;
			case 'T':
				if(argp[1] != '=') {
					usage(argv);
				}
				vfyCertState = true;
				switch(argp[2]) {
					case 'n':
						expectCertState = kSSLClientCertNone;
						break;
					case 'r':
						expectCertState = kSSLClientCertRequested;
						break;
					case 's':
						expectCertState = kSSLClientCertSent;
						break;
					case 'j':
						expectCertState = kSSLClientCertRejected;
						break;
					default:
						usage(argv);
				}
				break;
			case 'z':
				pargs.password = &argp[2];
				break;
			case 'p':
				doPause = true;
				break;
			case '7':
				pauseFirstLoop = true;
				break;
			case 'q':
				pargs.quiet = true;
				break;
			case 'V':
				pargs.verbose = true;
				break;
			case 's':
				pargs.silent = pargs.quiet = true;
				break;
			case 'N':
				displayHandshakeTimes = true;
				break;
			case '8':
				completeCertChain = true;
				break;
			case 'i':
				pargs.sessionCacheTimeout = atoi(&argp[2]);
				break;
			case '4':
				pargs.disableAnonCiphers = true;
				break;
			case 'D':
				if(++arg == argc)  {
					/* requires another arg */
					usage(argv);
				}
				dnFileBase = argv[arg];
				break;
			case 'h':
				if(pargs.verbose || (argp[1] == 'v')) {
					usageVerbose(argv);
				}
				else {
					usage(argv);
				}
			default:
				usage(argv);
		}
	}
	if(getMsgSpec) {
		pargs.getMsg = getMsgSpec;
	}
	else {
		sprintf(getMsg, "%s %s %s", 
			DEFAULT_GETMSG, getPath, DEFAULT_GET_SUFFIX);
		pargs.getMsg = getMsg;
	}
			
	/* get client cert and optional encryption cert as CFArrayRef */
	if(keyChainName) {
		pargs.clientCerts = getSslCerts(keyChainName, false, completeCertChain,
			pargs.anchorFile, &serverKc);
		if(pargs.clientCerts == nil) {
			exit(1);
		}
		if(pargs.password) {
			OSStatus ortn = SecKeychainUnlock(serverKc, 
				strlen(pargs.password), pargs.password, true);
			if(ortn) {
				printf("SecKeychainUnlock returned %d\n", (int)ortn);
				/* oh well */
			}
		}
	}
	if(encryptKeyChainName) {
		pargs.encryptClientCerts = getSslCerts(encryptKeyChainName, true, 
				completeCertChain, pargs.anchorFile, &encryptKc);
		if(pargs.encryptClientCerts == nil) {
			exit(1);
		}
	}
	signal(SIGPIPE, sigpipe);

	if(loopCount != 0) {
		/* prepare to handle KC callbacks for root cert cache invalidation */
		if(startCFRunLoop()) {
			exit(1);
		}

		/* give that thread a chance right now */
		while(!runLoopInitialized) {
			usleep(1000);
		};
	}

	if(doPause) {
		char resp;
		fpurge(stdin);
		printf("Before main loop. Hit a to abort, c to continue: ");
		resp = getchar();
		if(resp == 'a') {
			exit(1);
		}
	}

	for(loop=0; loop<loopCount; loop++) {
		/* 
		 * One pass for each protocol version, skipping any explicit version if
		 * an attempt at a higher version and succeeded in doing so successfully fell
		 * back.
		 */
		if(doTlsV1) {
			pargs.tryVersion = 
				protXOnly ? kTLSProtocol1Only : kTLSProtocol1;
			pargs.acceptedProts = NULL;
			if(!pargs.silent) {
				printf("Connecting to host %s with TLS V1...\n", pargs.hostName); 
			}
			fflush(stdout);
			err = sslPing(&pargs);
			if(err) {
				ourRtn++;
			}
			if(!pargs.quiet) {
				if(fileBase) {
					sprintf(fullFileBase, "%s_v3.1", fileBase);
				}
				showSSLResult(pargs,
					err, 
					displayCerts,
				  	fileBase ? fullFileBase : NULL,
					dnFileBase);
			}
			freePingArgs(&pargs);
			if(!err) {
				/* deal with fallbacks, skipping redundant tests */
				switch(pargs.negVersion) {
					case kSSLProtocol3:
						doSslV3 = false;
						break;
					case kSSLProtocol2:
						doSslV3 = false;
						doSslV2 = false;
						break;
					default:
						break;
				}
				ourRtn += verifyProtocol(verifyProt, maxProtocol, kTLSProtocol1,
					pargs.negVersion);
			}
			/* note we do this regardless since the client state might be
			 * the cause of a failure */
			ourRtn += verifyClientCertState(vfyCertState, expectCertState,
				pargs.certState);
		}
		if(doSslV3) {
			pargs.tryVersion = protXOnly ? kSSLProtocol3Only : kSSLProtocol3;
			pargs.acceptedProts = NULL;
			if(!pargs.silent) {
				printf("Connecting to host %s with SSL V3...\n", pargs.hostName); 
			}
			fflush(stdout);
			err = sslPing(&pargs);
			if(err) {
				ourRtn++;
			}
			if(!pargs.quiet) {
				if(fileBase) {
					sprintf(fullFileBase, "%s_v3.0", fileBase);
				}
				showSSLResult(pargs,
					err, 
					displayCerts,
					fileBase ? fullFileBase : NULL,
					dnFileBase);
			}
			freePingArgs(&pargs);
			if(!err) {
				/* deal with fallbacks, skipping redundant tests */
				switch(pargs.negVersion) {
					case kSSLProtocol2:
						doSslV2 = false;
						break;
					default:
						break;
				}
				ourRtn += verifyProtocol(verifyProt, maxProtocol, kSSLProtocol3,
					pargs.negVersion);
			}
			/* note we do this regardless since the client state might be
			 * the cause of a failure */
			ourRtn += verifyClientCertState(vfyCertState, expectCertState,
				pargs.certState);
		}
		
		if(doSslV2) {
			if(fileBase) {
				sprintf(fullFileBase, "%s_v2", fileBase);
			}
			if(!pargs.silent) {
				printf("Connecting to host %s with SSL V2...\n", pargs.hostName);
			}
			fflush(stdout);
			pargs.tryVersion = kSSLProtocol2;
			pargs.acceptedProts = NULL;
			err = sslPing(&pargs);
			if(err) {
				ourRtn++;
			}
			if(!pargs.quiet) {
				if(fileBase) {
					sprintf(fullFileBase, "%s_v2", fileBase);
				}
				showSSLResult(pargs,
					err, 
					displayCerts,
					fileBase ? fullFileBase : NULL,
					dnFileBase);
			}
			freePingArgs(&pargs);	
			if(!err) {
				ourRtn += verifyProtocol(verifyProt, maxProtocol, kSSLProtocol2,
					pargs.negVersion);
			}
			/* note we do this regardless since the client state might be
			 * the cause of a failure */
			ourRtn += verifyClientCertState(vfyCertState, expectCertState,
				pargs.certState);
		}
		if(doProtUnknown) {
			if(!pargs.silent) {
				printf("Connecting to host %s with kSSLProtocolUnknown...\n", 
					pargs.hostName); 
			}
			fflush(stdout);
			pargs.tryVersion = kSSLProtocolUnknown;
			pargs.acceptedProts = NULL;
			err = sslPing(&pargs);
			if(err) {
				ourRtn++;
			}
			if(!pargs.quiet) {
				if(fileBase) {
					sprintf(fullFileBase, "%s_def", fileBase);
				}
				showSSLResult(pargs,
					err, 
					displayCerts,
				  	fileBase ? fullFileBase : NULL,
					dnFileBase);
			}
			freePingArgs(&pargs);
		}
		if(acceptedProts != NULL) {
			pargs.acceptedProts = acceptedProts;
			pargs.tryVersion = kSSLProtocolUnknown; // not used
			if(!pargs.silent) {
				printf("Connecting to host %s with acceptedProts %s...\n", 
					pargs.hostName, pargs.acceptedProts); 
			}
			fflush(stdout);
			err = sslPing(&pargs);
			if(err) {
				ourRtn++;
			}
			if(!pargs.quiet) {
				if(fileBase) {
					sprintf(fullFileBase, "%s_def", fileBase);
				}
				showSSLResult(pargs,
					err, 
					displayCerts,
					fileBase ? fullFileBase : NULL,
					dnFileBase);
			}
			freePingArgs(&pargs);
		}
		if(doPause || 
		      (pauseFirstLoop && 
				 /* pause after first, before last to grab trace */
		         ((loop == 0) || (loop == loopCount - 1))
			  )
		   ) {
			char resp;
			fpurge(stdin);
			printf("a to abort, c to continue: ");
			resp = getchar();
			if(resp == 'a') {
				break;
			}
		}
    }	/* main loop */
	if(displayHandshakeTimes) {
		CFAbsoluteTime totalTime;
		unsigned numHandshakes;
		if(pargs.numHandshakes == 1) {
			/* just display the first one */
			totalTime = pargs.handshakeTimeFirst;
			numHandshakes = 1;
		}
		else {
			/* skip the first one */
			totalTime = pargs.handshakeTimeTotal;
			numHandshakes = pargs.numHandshakes - 1;
		}
		if(numHandshakes != 0) {
			printf("   %u handshakes in %f seconds; %f seconds per handshake\n",
				numHandshakes, totalTime,
				(totalTime / numHandshakes));
		}
	}
	printCertShutdown();
	if(ourRtn) {
		printf("===%s exiting with %d %s for host %s\n", argv[0], ourRtn, 
			(ourRtn > 1) ? "errors" : "error", pargs.hostName);
	}
    return ourRtn;

}


