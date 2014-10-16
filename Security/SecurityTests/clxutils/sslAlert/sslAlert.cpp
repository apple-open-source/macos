/*
 * sslAlert.cpp - test alert msg sending and processing, client and server side
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

#define STARTING_PORT			2000

/* 
 * localcert is a KC containing server cert and signing key
 * assumptions: 
 *	-- common name = "localcert"
 *  -- password of KC = "localcert"
 */
#define SERVER_KC		"localcert"
#define SERVER_ROOT		"localcert.cer"
/*
 * clientcert is a KC containing client cert and signing key
 * assumptions: 
 *  -- password of KC = "clientcert"
 *  -- note common name not checked by SecureTransport when verifying client cert chain
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
	printf("   b (non blocking I/O)\n");
	printf("   s=serverCertName; default %s\n", SERVER_ROOT);
	printf("   c=clientCertName; default %s\n", CLIENT_ROOT);
	printf("   R (ringBuffer I/O)\n");
	printf("   l=loops (default=1; 0=forever)\n");
	exit(1);
}

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
	kTLSProtocol1,
	NULL,				// acceptedProts
	serverKcPath,		// myCerts
	SERVER_KC,			// password
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

SslAppTestParams clientDefaults = 
{
	"localhost",
	false,				// skipHostNameCHeck
	0,					// port - test must set this
	NULL, NULL,			// RingBuffers
	false,				// noProtSpec
	kTLSProtocol1,
	NULL,				// acceptedProts
	NULL,				// myCertKcName
	CLIENT_KC,			// password - only meaningful when test sets myCertKcName
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
	int 				thisRtn;
	SslAppTestParams 	clientParams;
	SslAppTestParams 	serverParams;
	const char			*desc;
	unsigned short		portNum = STARTING_PORT;
	const char			*clientCert = CLIENT_ROOT;
	RingBuffer			serverToClientRing;
	RingBuffer			clientToServerRing;
	bool				ringBufferIo = false;
	unsigned 			loopNum = 0;
	unsigned 			loops = 1;

	for(int arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'q':
				serverDefaults.quiet = clientDefaults.quiet = true;
				break;
			case 'v':
				serverDefaults.verbose = clientDefaults.verbose = true;
				break;
			case 'b':
				serverDefaults.nonBlocking = clientDefaults.nonBlocking = 
						true;
				break;
			case 'p':
				portNum = atoi(&argp[2]);
				break;
			case 's':
				clientDefaults.anchorFile = &argp[2];
				break;
			case 'c':
				clientCert = &argp[2];
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
	
	if(sslCheckFile(clientDefaults.anchorFile)) {
		exit(1);
	}
	if(sslCheckFile(clientCert)) {
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

	/* convert keychain names to paths for root */
	sslKeychainPath(SERVER_KC, serverKcPath);
	sslKeychainPath(CLIENT_KC, clientKcPath);
	
	testStartBanner("sslAlert", argc, argv);
	// printf("sslAlert: server KC: %s\n", serverKcPath);
	// printf("sslAlert: client KC: %s\n", clientKcPath);
	
	/*
	 * We could get real fancy and have a bunch of elaborate tables describing
	 * what's supposed to happen in each test case, but I really don't think
	 * it's worth it. 
	 */
	for(;;) {
		desc = "basic TLS1, nothing fancy";
		clientParams = clientDefaults; serverParams = serverDefaults;
		serverParams.port = portNum;
		SSL_THR_RUN(serverParams, clientParams, desc, ourRtn);

		desc = "client doesn't recognize server root";
		if(ringBufferIo) {
			ringBufferReset(&serverToClientRing);
			ringBufferReset(&clientToServerRing);
		}
		SSL_THR_SETUP(serverParams, clientParams, clientDefaults, serverDefault);
		clientParams.anchorFile = NULL;
		clientParams.expectRtn = errSSLUnknownRootCert;
		serverParams.expectRtn = errSSLPeerUnknownCA;
		SSL_THR_RUN(serverParams, clientParams, desc, ourRtn);

		desc = "negotiate down to SSL3, server limited";
		if(ringBufferIo) {
			ringBufferReset(&serverToClientRing);
			ringBufferReset(&clientToServerRing);
		}
		SSL_THR_SETUP(serverParams, clientParams, clientDefaults, serverDefault);
		serverParams.tryVersion = kSSLProtocol3;
		serverParams.expectVersion = kSSLProtocol3;
		clientParams.expectVersion = kSSLProtocol3;
		SSL_THR_RUN(serverParams, clientParams, desc, ourRtn);

		desc = "negotiate down to SSL3, client limited";
		if(ringBufferIo) {
			ringBufferReset(&serverToClientRing);
			ringBufferReset(&clientToServerRing);
		}
		SSL_THR_SETUP(serverParams, clientParams, clientDefaults, serverDefault);
		clientParams.tryVersion = kSSLProtocol3;
		clientParams.expectVersion = kSSLProtocol3;
		serverParams.expectVersion = kSSLProtocol3;
		SSL_THR_RUN(serverParams, clientParams, desc, ourRtn);

		desc = "successful client authentication";
		if(ringBufferIo) {
			ringBufferReset(&serverToClientRing);
			ringBufferReset(&clientToServerRing);
		}
		SSL_THR_SETUP(serverParams, clientParams, clientDefaults, serverDefault);
		clientParams.myCertKcName = clientKcPath;
		serverParams.anchorFile = clientCert;
		serverParams.authenticate = kAlwaysAuthenticate;
		clientParams.expectCertState = kSSLClientCertSent; 
		serverParams.expectCertState = kSSLClientCertSent; 
		SSL_THR_RUN(serverParams, clientParams, desc, ourRtn);

		desc = "client authentication, server doesn't recognize root TLS1";
		if(ringBufferIo) {
			ringBufferReset(&serverToClientRing);
			ringBufferReset(&clientToServerRing);
		}
		SSL_THR_SETUP(serverParams, clientParams, clientDefaults, serverDefault);
		clientParams.myCertKcName = clientKcPath;
		/* no anchor file for server; unrecognized */
		serverParams.authenticate = kAlwaysAuthenticate;
		clientParams.expectCertState = kSSLClientCertRejected; 
		serverParams.expectCertState = kSSLClientCertRejected; 
		serverParams.expectRtn = errSSLUnknownRootCert;
		clientParams.expectRtn = errSSLPeerUnknownCA;
		SSL_THR_RUN(serverParams, clientParams, desc, ourRtn);

		desc = "client authentication, server doesn't recognize root SSL3";
		if(ringBufferIo) {
			ringBufferReset(&serverToClientRing);
			ringBufferReset(&clientToServerRing);
		}
		SSL_THR_SETUP(serverParams, clientParams, clientDefaults, serverDefault);
		clientParams.tryVersion = kSSLProtocol3;
		clientParams.expectVersion = kSSLProtocol3;
		serverParams.expectVersion = kSSLProtocol3;
		clientParams.myCertKcName = clientKcPath;
		/* no anchor file for server; unrecognized */
		serverParams.authenticate = kAlwaysAuthenticate;
		clientParams.expectCertState = kSSLClientCertRejected; 
		serverParams.expectCertState = kSSLClientCertRejected; 
		serverParams.expectRtn = errSSLUnknownRootCert;
		clientParams.expectRtn = errSSLPeerUnsupportedCert;
		thisRtn = sslRunSession(&serverParams, &clientParams, desc);
		if(thisRtn) {
			if(testError(clientParams.quiet)) {
				goto done;
			}
		}

		desc = "server requires authentication, no client cert, TLS1";
		if(ringBufferIo) {
			ringBufferReset(&serverToClientRing);
			ringBufferReset(&clientToServerRing);
		}
		SSL_THR_SETUP(serverParams, clientParams, clientDefaults, serverDefault);
		serverParams.authenticate = kAlwaysAuthenticate;
		clientParams.expectCertState = kSSLClientCertRequested; 
		serverParams.expectCertState = kSSLClientCertRequested; 
		serverParams.expectRtn = errSSLXCertChainInvalid;
		clientParams.expectRtn = errSSLPeerCertUnknown;
		SSL_THR_RUN(serverParams, clientParams, desc, ourRtn);

		desc = "server requires authentication, no client cert, SSL3";
		if(ringBufferIo) {
			ringBufferReset(&serverToClientRing);
			ringBufferReset(&clientToServerRing);
		}
		SSL_THR_SETUP(serverParams, clientParams, clientDefaults, serverDefault);
		clientParams.tryVersion      = kSSLProtocol3;
		clientParams.expectVersion   = kSSLProtocol3;
		serverParams.expectVersion   = kSSLProtocol3;
		serverParams.authenticate    = kAlwaysAuthenticate;
		clientParams.expectCertState = kSSLClientCertRequested; 
		serverParams.expectCertState = kSSLClientCertRequested; 
		serverParams.expectRtn = errSSLProtocol;
		clientParams.expectRtn = errSSLPeerUnexpectedMsg;
		SSL_THR_RUN(serverParams, clientParams, desc, ourRtn);

		desc = "server (only) requests authentication, no client cert, TLS1";
		if(ringBufferIo) {
			ringBufferReset(&serverToClientRing);
			ringBufferReset(&clientToServerRing);
		}
		SSL_THR_SETUP(serverParams, clientParams, clientDefaults, serverDefault);
		serverParams.authenticate = kTryAuthenticate;
		clientParams.expectCertState = kSSLClientCertRequested; 
		serverParams.expectCertState = kSSLClientCertRequested; 
		SSL_THR_RUN(serverParams, clientParams, desc, ourRtn);

		desc = "server (only) requests authentication, no client cert, SSL3";
		if(ringBufferIo) {
			ringBufferReset(&serverToClientRing);
			ringBufferReset(&clientToServerRing);
		}
		SSL_THR_SETUP(serverParams, clientParams, clientDefaults, serverDefault);
		clientParams.tryVersion      = kSSLProtocol3;
		clientParams.expectVersion   = kSSLProtocol3;
		serverParams.expectVersion   = kSSLProtocol3;
		serverParams.authenticate    = kTryAuthenticate;
		clientParams.expectCertState = kSSLClientCertRequested; 
		serverParams.expectCertState = kSSLClientCertRequested; 
		SSL_THR_RUN(serverParams, clientParams, desc, ourRtn);

		desc = "server (only) requests authentication, client cert w/unknown root, TLS1";
		if(ringBufferIo) {
			ringBufferReset(&serverToClientRing);
			ringBufferReset(&clientToServerRing);
		}
		SSL_THR_SETUP(serverParams, clientParams, clientDefaults, serverDefault);
		serverParams.authenticate = kTryAuthenticate;
		clientParams.expectCertState = kSSLClientCertRejected; 
		serverParams.expectCertState = kSSLClientCertRejected; 
		clientParams.myCertKcName = clientKcPath;
		/* no anchor file for server; unrecognized */
		serverParams.expectRtn = errSSLUnknownRootCert;
		clientParams.expectRtn = errSSLPeerUnknownCA;
		SSL_THR_RUN(serverParams, clientParams, desc, ourRtn);

		desc = "server (only) requests authentication, client cert w/unknown root, SSL3";
		if(ringBufferIo) {
			ringBufferReset(&serverToClientRing);
			ringBufferReset(&clientToServerRing);
		}
		SSL_THR_SETUP(serverParams, clientParams, clientDefaults, serverDefault);
		clientParams.tryVersion      = kSSLProtocol3;
		clientParams.expectVersion   = kSSLProtocol3;
		serverParams.expectVersion   = kSSLProtocol3;
		serverParams.authenticate    = kTryAuthenticate;
		clientParams.expectCertState = kSSLClientCertRejected; 
		serverParams.expectCertState = kSSLClientCertRejected; 
		clientParams.myCertKcName = clientKcPath;
		/* no anchor file for server; unrecognized */
		serverParams.expectRtn = errSSLUnknownRootCert;
		clientParams.expectRtn = errSSLPeerUnsupportedCert;
		SSL_THR_RUN(serverParams, clientParams, desc, ourRtn);
		if(ringBufferIo) {
			ringBufferReset(&serverToClientRing);
			ringBufferReset(&clientToServerRing);
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
			printf("===== sslAlert test PASSED =====\n");
		}
		else {
			printf("****FAIL: %d errors detected\n", ourRtn);
		}
	}
	
	return ourRtn;
}
