/*
 * sslSession.cpp - basic 2-thread SSL server/client session
 */
#include <Security/SecureTransport.h>
#include <Security/Security.h>
#include <clAppUtils/sslAppUtils.h>
#include <clAppUtils/ioSock.h>
#include <clAppUtils/sslThreading.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <utilLib/common.h>
#include <security_cdsa_utils/cuPrintCert.h>
#include <security_utilities/threading.h>
#include <security_utilities/devrandom.h>

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/param.h>

#define PORT_DEF			4000
#define HOST_DEF			"localhost"
#define DH_PARAMS			"dhParams_512.der"

static void usage(char **argv)
{
	printf("Usage: %s server_kc [options]\n", argv[0]);
	printf("options:\n");
	printf("   P=port (default = %d)\n", PORT_DEF);
	printf("   c=client_kc (default is none)\n");
	printf("   d (DSA, default is RSA)\n");
	printf("   f (D-H, default is RSA)\n");
	printf("   a anchor File for client side (typically, the server's cert)\n");
	printf("   A anchor file for server side (typically, the client's cert)\n");
	printf("   h hostname (default is %s)\n", HOST_DEF);
	printf("   k (skip hostname check)\n");
	printf("   b (non blocking I/O)\n");
	printf("   u Require client authentication\n");
	printf("   x Expect policy verify error on client side\n");
	printf("   X Expect policy verify error on server side\n");
	printf("   z=kc_pwd\n");
	printf("   R (ringBuffer I/O)\n");
	printf("   l=loops (default 1)\n");
	printf("   q(uiet)\n");
	printf("   v(erbose)\n");
	exit(1);
}

#define IGNORE_SIGPIPE	1
#if 	IGNORE_SIGPIPE
#include <signal.h>

void sigpipe(int sig) 
{ 
}
#endif	/* IGNORE_SIGPIPE */

static SSLCipherSuite ciphers[] = {
	SSL_RSA_WITH_RC4_128_SHA, SSL_NO_SUCH_CIPHERSUITE
};

/*
 * Default params for each test. Main() adjust this per cmd line
 * args.
 */
SslAppTestParams serverDefaults = 
{
	"no name here",
	false,				// skipHostNameCHeck
	PORT_DEF,	
	NULL, NULL,			// RingBuffers
	false,				// noProtSpec
	kTLSProtocol1,	
	NULL,				// acceptedProts - not used in this test
	NULL,				// myCerts - const
	NULL,				// password
	true,				// idIsTrustedRoot
	false,				// disableCertVerify
	NULL,				// anchorFile
	false,				// replaceAnchors
	kNeverAuthenticate,
	false,				// resumeEnable
	ciphers,			// ciphers
	false,				// nonBlocking 
	NULL,				// dhParams
	0,					// dhParamsLen
	noErr,				// expectRtn
	kTLSProtocol1,		// expectVersion
	kSSLClientCertNone,
	SSL_CIPHER_IGNORE,
	false,				// quiet
	false,				// silent
	false,				// verbose
	{0},				// lock
	{0},				// cond
	false,				// serverReady
	0,					// clientDone
	false,				// serverAbort
	/* returned */
	kSSLProtocolUnknown,
	SSL_NULL_WITH_NULL_NULL,
	kSSLClientCertNone,
	noHardwareErr
	
};

SslAppTestParams clientDefaults = 
{
	HOST_DEF,
	false,				// skipHostNameCHeck
	PORT_DEF,		
	NULL, NULL,			// RingBuffers
	false,				// noProtSpec
	kTLSProtocol1,	
	NULL,				// acceptedProts - not used in this test
	NULL,				// myCerts - const
	NULL,				// password
	true,				// idIsTrustedRoot
	false,				// disableCertVerify
	NULL,				// anchorFile
	false,				// replaceAnchors
	kNeverAuthenticate,
	false,				// resumeEnable
	NULL,				// ciphers
	false,				// nonBlocking 
	NULL,				// dhParams
	0,					// dhParamsLen
	noErr,				// expectRtn
	kTLSProtocol1,		// expectVersion
	kSSLClientCertNone,
	SSL_CIPHER_IGNORE,
	false,				// quiet
	false,				// silent
	false,				// verbose
	{0},				// lock
	{0},				// cond
	false,				// serverReady
	0,					// clientDone
	false,				// serverAbort
	/* returned */
	kSSLProtocolUnknown,
	SSL_NULL_WITH_NULL_NULL,
	kSSLClientCertNone,
	noHardwareErr
	
};

int main(int argc, char **argv)
{
	int 				ourRtn = 0;
	char	 			*argp;
	bool				dhEnable = false;
	unsigned			loop;
	unsigned 			loops = 1;
	bool				ringBufferIo = false;
	RingBuffer			serverToClientRing;
	RingBuffer			clientToServerRing;

	if(argc < 2) {
		usage(argv);
	}
	serverDefaults.myCertKcName = argv[1];
	for(int arg=2; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'c':
				clientDefaults.myCertKcName = &argp[2];
			case 'q':
				serverDefaults.quiet = clientDefaults.quiet = true;
				break;
			case 'v':
				serverDefaults.verbose = clientDefaults.verbose = true;
				break;
			case 'p':
				serverDefaults.port = clientDefaults.port = atoi(&argp[2]);
				break;
			case 'b':
				serverDefaults.nonBlocking = clientDefaults.nonBlocking = 
						true;
				break;
			case 'd':
				ciphers[0] = SSL_DH_DSS_WITH_3DES_EDE_CBC_SHA;
				dhEnable = true;
				break;
			case 'f':
				ciphers[0] = SSL_DH_anon_WITH_RC4_128_MD5;
				dhEnable = true;
				break;
			case 'h':
				if(++arg == argc) {
					usage(argv);
				}
				clientDefaults.hostName=argv[arg];
				break;
			case 'k':
				clientDefaults.skipHostNameCheck = true;
				break;
			case 'a':
				if(++arg == argc) {
					usage(argv);
				}
				clientDefaults.anchorFile = serverDefaults.anchorFile = argv[arg];
				break;
			case 'A':
				if(++arg == argc) {
					usage(argv);
				}
				serverDefaults.anchorFile = argv[arg];
				break;
			case 'z':
				serverDefaults.password = &argp[2];
				break;
			case 'P':
				serverDefaults.port = clientDefaults.port = atoi(&argp[2]);
				break;
			case 'u':
				serverDefaults.authenticate = kAlwaysAuthenticate;
				if(serverDefaults.expectCertState == kSSLClientCertNone) {
					serverDefaults.expectCertState = kSSLClientCertSent;
				}
				/* else it was set by 'X' option */
				if(clientDefaults.expectCertState == kSSLClientCertNone) {
					clientDefaults.expectCertState = kSSLClientCertSent;
				}
				/* else...ditto */
				break;
			case 'x':
				/* server side has bad cert */
				clientDefaults.expectRtn = errSSLXCertChainInvalid;
				serverDefaults.expectRtn = errSSLPeerCertUnknown;
				break;
			case 'X':
				/* client side has bad cert */
				serverDefaults.expectRtn = errSSLXCertChainInvalid;
				clientDefaults.expectRtn = errSSLPeerCertUnknown;
				serverDefaults.expectCertState = kSSLClientCertRejected;
				clientDefaults.expectCertState = kSSLClientCertRejected;
				break;
			case 'R':
				ringBufferIo = true;
				break;
			case 'l':
				loops = atoi(&argp[2]);
				break;
			default:
				usage(argv);
		}
	}
	
	#if IGNORE_SIGPIPE
	signal(SIGPIPE, sigpipe);
	#endif
	
	if(ringBufferIo) {
		/* set up ring buffers */
		ringBufSetup(&serverToClientRing, "serveToClient", DEFAULT_NUM_RB_BUFS, DEFAULT_BUF_RB_SIZE);
		ringBufSetup(&clientToServerRing, "clientToServe", DEFAULT_NUM_RB_BUFS, DEFAULT_BUF_RB_SIZE);
		serverDefaults.serverToClientRing = &serverToClientRing;
		serverDefaults.clientToServerRing = &clientToServerRing;
		clientDefaults.serverToClientRing = &serverToClientRing;
		clientDefaults.clientToServerRing = &clientToServerRing;
	}
	if(dhEnable) {
		/* snag D-H params */
		if(readFile(DH_PARAMS, (unsigned char **)&serverDefaults.dhParams, 
				&serverDefaults.dhParamsLen)) {
			printf("***Error reading Diffie-Hellman params."
				" Patience, grasshopper.\n");
		}
	}
	testStartBanner("sslSession", argc, argv);
	for(loop=0; loop<loops; loop++) {
		ourRtn = sslRunSession(&serverDefaults, &clientDefaults, NULL);
		if(ourRtn) {
			break;
		}
	}
	if(!clientDefaults.quiet) {
		if(ourRtn == 0) {
			if(!serverDefaults.quiet) {
				printf("===== %s test PASSED =====\n", argv[0]);
			}
		}
		else {
			printf("****FAIL: %d errors detected\n", ourRtn);
		}
	}
	
	return ourRtn;
}
