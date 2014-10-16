/*
 * sslAuth.cpp - test client-side authentication, client and server side
 *
 * This mainly tests proper reporting of SSLGetClientCertificateState.
 * Detailed error reporting for the myriad things that can go
 * wrong during client authentication is tested in sslAlert. 
 */
#include <Security/SecureTransport.h>
#include <Security/Security.h>
#include <clAppUtils/sslAppUtils.h>
#include <clAppUtils/ioSock.h>
#include <clAppUtils/sslThreading.h>
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

#define STARTING_PORT			4000

/* 
 * localcert is a KC containing server cert and signing key.
 * Password is same as filename of the keychain. 
 */
#define SERVER_KC		"localcert"
#define SERVER_ROOT		"localcert.cer"

/*
 * clientcert is a KC containing client cert and signing key.
 * Password is same as filename of the keychain. 
 * 
 * Note common name not checked by SecureTransport when 
 * verifying client cert chain. 
 */
#define CLIENT_KC		"clientcert"
#define CLIENT_ROOT		"clientcert.cer"

/* main() fills these in using sslKeychainPath() */
static char serverKcPath[MAXPATHLEN];
static char clientKcPath[MAXPATHLEN];

static void usage(char **argv)
{
	printf("Usage: %s [options]\n", argv[0]);
	printf("options:\n");
	printf("   q(uiet)\n");
	printf("   v(erbose)\n");
	printf("   p=startingPortNum\n");
	printf("   t=startTestNum\n");
	printf("   b (non blocking I/O)\n");
	printf("   s=serverCertName; default %s\n", SERVER_ROOT);
	printf("   c=clientCertName; default %s\n", CLIENT_ROOT);
	printf("   R (ringBuffer I/O)\n");
	printf("   k (keep keychain list open)\n");
	printf("   l=loops (default=1; 0=forever)\n");
	printf("   o (One test only)\n");
	exit(1);
}

/*
 * Parameters defining one run
 */
typedef struct {
	const char		*testDesc;		
	SSLProtocol		serverTryProt;
	SSLProtocol		serverActProt;		// expected negotiated result
	SSLAuthenticate	tryAuth;
	SSLClientCertificateState	serverAuthState;
	OSStatus		serverStatus;		// expected OSStatus
	SSLProtocol		clientTryProt;
	const char		*clientKcName;		// determines whether or not
										//   to do authentication
	SSLClientCertificateState	clientAuthState;
	OSStatus		clientStatus;
} SslAuthParams;

SslAuthParams authTestParams[] = 
{
	{ 	
		"Server doesn't authenticate, client tries, TLS1",
		kTLSProtocol1, kTLSProtocol1, 
		kNeverAuthenticate, kSSLClientCertNone, noErr,
		kTLSProtocol1, NULL, kSSLClientCertNone, noErr
	},
	{ 	
		"Server doesn't authenticate, client tries, SSL3",
		kTLSProtocol1, kSSLProtocol3, 
		kNeverAuthenticate, kSSLClientCertNone, noErr,
		kSSLProtocol3, NULL, kSSLClientCertNone, noErr
	},
	{ 	
		"Server tries authentication, client refuses, TLS1",
		kTLSProtocol1, kTLSProtocol1, 
		kTryAuthenticate, kSSLClientCertRequested, noErr,
		kTLSProtocol1, NULL, kSSLClientCertRequested, noErr
	},
	{ 	
		"Server tries authentication, client refuses, SSL3",
		kTLSProtocol1, kSSLProtocol3, 
		kTryAuthenticate, kSSLClientCertRequested, noErr,
		kSSLProtocol3, NULL, kSSLClientCertRequested, noErr
	},
	{ 	
		"Server tries authentication, client sends cert, TLS1",
		kTLSProtocol1, kTLSProtocol1, 
		kTryAuthenticate, kSSLClientCertSent, noErr,
		kTLSProtocol1, clientKcPath, kSSLClientCertSent, noErr
	},
	{ 	
		"Server tries authentication, client sends cert, SSL3",
		kSSLProtocol3, kSSLProtocol3, 
		kTryAuthenticate, kSSLClientCertSent, noErr,
		kTLSProtocol1, clientKcPath, kSSLClientCertSent, noErr
	},
	{ 	
		"Server requires authentication, client refuses, TLS1",
		kTLSProtocol1, kTLSProtocol1, 
		kAlwaysAuthenticate, kSSLClientCertRequested, 
			errSSLXCertChainInvalid,
		kTLSProtocol1, NULL, kSSLClientCertRequested, 
			errSSLPeerCertUnknown
	},
	{ 	
		"Server requires authentication, client refuses, SSL3",
		kSSLProtocol3, kSSLProtocol3, 
		kAlwaysAuthenticate, kSSLClientCertRequested, errSSLProtocol,
		kTLSProtocol1, NULL, kSSLClientCertRequested, errSSLPeerUnexpectedMsg
	},

};

#define NUM_SSL_AUTH_TESTS	(sizeof(authTestParams) / sizeof(authTestParams[0]))

#define IGNORE_SIGPIPE	1
#if 	IGNORE_SIGPIPE
#include <signal.h>

void sigpipe(int sig) 
{ 
}
#endif	/* IGNORE_SIGPIPE */

/*
 * Default params for each test. Main() will make a copy of this and 
 * adjust its copy on a per-test basis.
 */
SslAppTestParams serverDefaults = 
{
	"no name here",
	false,				// skipHostNameCHeck
	0,					// port - test must set this	
	NULL, NULL,			// RingBuffers
	false,				// noProtSpec
	kTLSProtocol1,		// set in test loop
	NULL,				// acceptedProts - not used in this test
	serverKcPath,		// myCerts - const
	SERVER_KC,			// password
	true,				// idIsTrustedRoot
	false,				// disableCertVerify
	CLIENT_ROOT,		// anchorFile - only meaningful if client 
						//   authenticates
	false,				// replaceAnchors
	kNeverAuthenticate,	// set in test loop
	false,				// resumeEnable
	NULL,				// ciphers,
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
	"localhost",
	false,				// skipHostNameCHeck
	0,					// port - test must set this
	NULL, NULL,			// RingBuffers
	false,				// noProtSpec
	kTLSProtocol1,
	NULL,				// acceptedProts
	NULL,				// myCertKcName - varies, set in main test loop
	CLIENT_KC,			// password
	true,				// idIsTrustedRoot
	false,				// disableCertVerify
	SERVER_ROOT,		// anchorFile
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
	SslAppTestParams 	clientParams;
	SslAppTestParams 	serverParams;
	unsigned short		portNum = STARTING_PORT;
	SslAuthParams		*authParams;
	unsigned			testNum;
	int					thisRtn;
	unsigned			startTest = 0;
	unsigned 			loopNum = 0;
	unsigned 			loops = 1;
	RingBuffer			serverToClientRing;
	RingBuffer			clientToServerRing;
	bool				ringBufferIo = false;
	bool				keepKCListOpen = false;
	CFArrayRef			kcList = NULL;
	bool				oneTestOnly = false;
	
	for(int arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'q':
				serverDefaults.quiet = clientDefaults.quiet = true;
				break;
			case 'v':
				serverDefaults.verbose = clientDefaults.verbose = true;
				break;
			case 'p':
				portNum = atoi(&argp[2]);
				break;
			case 'b':
				serverDefaults.nonBlocking = clientDefaults.nonBlocking = 
						true;
				break;
			case 't':
				startTest = atoi(&argp[2]);
				break;
			case 's':
				clientDefaults.anchorFile = &argp[2];
				break;
			case 'c':
				serverDefaults.anchorFile = &argp[2];
				break;
			case 'l':
				loops = atoi(&argp[2]);
				break;
			case 'R':
				ringBufferIo = true;
				break;
			case 'k':
				keepKCListOpen = true;
				break;
			case 'o':
				oneTestOnly = true;
				break;
			default:
				usage(argv);
		}
	}
	
	if(sslCheckFile(clientDefaults.anchorFile)) {
		exit(1);
	}
	if(sslCheckFile(serverDefaults.anchorFile)) {
		exit(1);
	}
	if(ringBufferIo) {
		/* set up ring buffers */
		ringBufSetup(&serverToClientRing, "serveToClient", DEFAULT_NUM_RB_BUFS, DEFAULT_BUF_RB_SIZE);
		ringBufSetup(&clientToServerRing, "clientToServe", DEFAULT_NUM_RB_BUFS, DEFAULT_BUF_RB_SIZE);
		serverDefaults.serverToClientRing = &serverToClientRing;
		serverDefaults.clientToServerRing = &clientToServerRing;
		clientDefaults.serverToClientRing = &serverToClientRing;
		clientDefaults.clientToServerRing = &clientToServerRing;
	}
	if(keepKCListOpen) {
		/* this prevents most of the CPU cycles being spent opening keychains */
		OSStatus ortn = SecKeychainCopySearchList(&kcList);
		if(ortn) {
			cssmPerror("SecKeychainCopySearchList", ortn);
			exit(1);
		}
	}
	
#if IGNORE_SIGPIPE
	signal(SIGPIPE, sigpipe);
	#endif

	/* convert keychain names to paths for root */
	sslKeychainPath(SERVER_KC, serverKcPath);
	sslKeychainPath(CLIENT_KC, clientKcPath);
	
	testStartBanner("sslAuth", argc, argv);
	// printf("sslAuth: server KC: %s\n", serverKcPath);
	// printf("sslAuth: client KC: %s\n", clientKcPath);

	serverParams.port = portNum - 1;	// gets incremented by SSL_THR_SETUP
	
	for(;;) {
		for(testNum=startTest; testNum<NUM_SSL_AUTH_TESTS; testNum++) {
			authParams = &authTestParams[testNum];
			SSL_THR_SETUP(serverParams, clientParams, clientDefaults, 
					serverDefault);
			if(ringBufferIo) {
				ringBufferReset(&serverToClientRing);
				ringBufferReset(&clientToServerRing);
			}
					
			serverParams.tryVersion 	 = authParams->serverTryProt;
			serverParams.expectVersion 	 = authParams->serverActProt;
			serverParams.authenticate 	 = authParams->tryAuth;
			serverParams.expectCertState = authParams->serverAuthState;
			serverParams.expectRtn 		 = authParams->serverStatus;
			
			clientParams.tryVersion		 = authParams->clientTryProt;
			clientParams.expectVersion 	 = authParams->serverActProt;
			clientParams.expectRtn		 = authParams->clientStatus;
			clientParams.myCertKcName	 = authParams->clientKcName;
			clientParams.expectCertState = authParams->clientAuthState;
			
			SSL_THR_RUN_NUM(serverParams, clientParams, authParams->testDesc, 
				ourRtn, testNum);
			if(oneTestOnly) {
				break;
			}
		}
		if(loops) {
			if(++loopNum == loops) {
				break;
			}
			printf("...loop %u\n", loopNum);
		}
	}
done:
	if(!clientParams.quiet) {
		if(ourRtn == 0) {
			printf("===== %s test PASSED =====\n", argv[0]);
		}
		else {
			printf("****FAIL: %d errors detected\n", ourRtn);
		}
	}
	
	return ourRtn;
}
