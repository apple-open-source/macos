/*
 * sslHandshakeTimeRB - measure SSL handshake timing, RingBuffer version (no
 *						socket I/O).
 *
 * Written by Doug Mitchell. 
 */
#include <Security/SecureTransport.h>
#include <clAppUtils/sslAppUtils.h>
#include <utilLib/common.h>
#include <clAppUtils/ringBufferIo.h>
#include <clAppUtils/sslRingBufferThreads.h>

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/param.h>
#include <CoreFoundation/CoreFoundation.h>
#include <security_utilities/devrandom.h>

#define DEFAULT_KC		"localcert"			/* default keychain */

/* we might make these user-tweakable */
#define DEFAULT_NUM_BUFS	16
#define DEFAULT_BUF_SIZE	1024		/* in the ring buffers */
#define LOOPS_DEF			20

static void usage(char **argv)
{
    printf("Usage: %s [option ...]\n", argv[0]);
    printf("Options:\n");
	printf("   -l loops (default= %d)\n", LOOPS_DEF);
	printf("   -k keychain (default = %s)\n", DEFAULT_KC);
	printf("   -c cipher (default = RSA/AES128\n");
	printf("      ciphers: a=RSA/AES128; r=RSA/RC4; d=RSA/DES; D=RSA/3DES;\n");
	printf("               h=DHA/RC4; H=DH/DSS/DES; A=AES256\n");
	printf("   -v version (t|2|3; default = t(TLS1)\n");
	printf("   -a (enable client authentication)\n");
    exit(1);
}

static int doTest(
	SslRingBufferArgs	*clientArgs,
	SslRingBufferArgs	*serverArgs,
	unsigned			loops,
	CFAbsoluteTime		*totalElapsedClient,	/* RETURNED */
	CFAbsoluteTime		*totalElapsedServer)	/* RETURNED */
{
	CFAbsoluteTime elapsedClient = 0.0;
	CFAbsoluteTime elapsedServer = 0.0;
	int result;
	pthread_t client_thread = NULL;
	unsigned dex;
	void *status;
	
	for(dex=0; dex<loops; dex++) {
		
		ringBufferReset(clientArgs->ringWrite);
		ringBufferReset(clientArgs->ringRead);
		clientArgs->iAmReady = false;
		serverArgs->iAmReady = false;
	
		/* fire up client thread */
		result = pthread_create(&client_thread, NULL, 
				sslRbClientThread, clientArgs);
		if(result) {
			printf("***pthread_create returned %d, aborting\n", result);
			return -1;
		}
		
		/* 
		 * And the server pseudo thread. This returns when all data has been transferred. 
		 */
		OSStatus ortn = sslRbServerThread(serverArgs);
		if(*serverArgs->abortFlag || ortn) {
			printf("***Test aborted (1).\n");
			return -1;
		}
		
		/* now wait for client */
		result = pthread_join(client_thread, &status);
		if(result || *clientArgs->abortFlag) {
			printf("***Test aborted (2).\n");
			return -1;
		}
		elapsedClient += (clientArgs->startData - clientArgs->startHandshake);
		elapsedServer += (serverArgs->startData - serverArgs->startHandshake);
	}
	*totalElapsedClient = elapsedClient;
	*totalElapsedServer = elapsedServer;
	return 0;
}

int main(int argc, char **argv)
{
	RingBuffer		serverToClientRing;
	RingBuffer		clientToServerRing;
	unsigned		numBufs = DEFAULT_NUM_BUFS;
	unsigned		bufSize = DEFAULT_BUF_SIZE;
	CFArrayRef		idArray;					/* for SSLSetCertificate */
	CFArrayRef		anchorArray;				/* trusted roots */
	SslRingBufferArgs clientArgs;
	SslRingBufferArgs serverArgs;
	SecKeychainRef	kcRef = NULL;
	SecCertificateRef anchorCert = NULL;
	SecIdentityRef	idRef = NULL;
	bool			abortFlag = false;
	bool			diffieHellman = true;		/* FIXME needs work */
	OSStatus		ortn;
	CFAbsoluteTime	clientFirst;
	CFAbsoluteTime	serverFirst;
	CFAbsoluteTime	clientTotal;
	CFAbsoluteTime	serverTotal;
	
	/* user-spec'd variables */
	char 			*kcName = DEFAULT_KC;
	SSLCipherSuite 	cipherSuite = TLS_RSA_WITH_AES_128_CBC_SHA;
	SSLProtocol 	prot = kTLSProtocol1;
	bool 			clientAuthEnable = false;
	unsigned		loops = LOOPS_DEF;
	
	extern int optind;
	extern char *optarg;
	int arg;
	optind = 1;
	while ((arg = getopt(argc, argv, "l:k:x:c:v:w:aB")) != -1) {
		switch (arg) {
			case 'l':
				loops = atoi(optarg);
				break;
			case 'k':
				kcName = optarg;
				break;
			case 'c':
				switch(optarg[0]) {
					case 'a':
						cipherSuite = TLS_RSA_WITH_AES_128_CBC_SHA;
						break;
					case 'r':
						cipherSuite = SSL_RSA_WITH_RC4_128_SHA;
						break;
					case 'd':
						cipherSuite = SSL_RSA_WITH_DES_CBC_SHA;
						break;
					case 'D':
						cipherSuite = SSL_RSA_WITH_3DES_EDE_CBC_SHA;
						break;
					case 'h':
						cipherSuite = SSL_DH_anon_WITH_RC4_128_MD5;
						diffieHellman = true;
						break;
					case 'H':
						cipherSuite = SSL_DHE_DSS_WITH_DES_CBC_SHA;
						diffieHellman = true;
						break;
					case 'A':
						cipherSuite = TLS_RSA_WITH_AES_256_CBC_SHA;
						break;
					default:
						usage(argv);
				}
				break;
			case 'v':
				switch(optarg[0]) {
					case 't':
						prot = kTLSProtocol1;
						break;
					case '2':
						prot = kSSLProtocol2;
						break;
					case '3':
						prot = kSSLProtocol3;
						break;
					default:
						usage(argv);
				}
				break;
			case 'a':
				clientAuthEnable = true;
				break;
			default:
				usage(argv);
		}
	}
	if(optind != argc) {
		usage(argv);
	}
	
	/* set up ring buffers */
	ringBufSetup(&serverToClientRing, "serveToClient", numBufs, bufSize);
	ringBufSetup(&clientToServerRing, "clientToServe", numBufs, bufSize);
	
	/* get server SecIdentity */
	idArray = getSslCerts(kcName, 
		CSSM_FALSE,		/* encryptOnly */
		CSSM_FALSE,		/* completeCertChain */
		NULL,			/* anchorFile */
		&kcRef);
	if(idArray == NULL) {
		printf("***Can't get signing cert from %s\n", kcName);
		exit(1);
	}
	idRef = (SecIdentityRef)CFArrayGetValueAtIndex(idArray, 0);
	ortn = SecIdentityCopyCertificate(idRef, &anchorCert);
	if(ortn) {
		cssmPerror("SecIdentityCopyCertificate", ortn);
		exit(1);
	}
	anchorArray = CFArrayCreate(NULL, (const void **)&anchorCert,
			1, &kCFTypeArrayCallBacks);

	CFRelease(kcRef);

	/* set up server side */
	memset(&serverArgs, 0, sizeof(serverArgs));
	serverArgs.idArray = idArray;
	serverArgs.trustedRoots = anchorArray;
	serverArgs.xferSize = 0;
	serverArgs.xferBuf = NULL;
	serverArgs.chunkSize = 0;
	serverArgs.cipherSuite = cipherSuite;
	serverArgs.prot = prot;
	serverArgs.ringWrite = &serverToClientRing;
	serverArgs.ringRead = &clientToServerRing;
	serverArgs.goFlag = &clientArgs.iAmReady;
	serverArgs.abortFlag = &abortFlag;

	/* set up client side */
	memset(&clientArgs, 0, sizeof(clientArgs));
	clientArgs.idArray = NULL;		/* until we do client auth */
	clientArgs.trustedRoots = anchorArray;
	clientArgs.xferSize = 0;
	clientArgs.xferBuf = NULL;
	clientArgs.chunkSize = 0;
	clientArgs.cipherSuite = cipherSuite;
	clientArgs.prot = prot;
	clientArgs.ringWrite = &clientToServerRing;
	clientArgs.ringRead = &serverToClientRing;
	clientArgs.goFlag = &serverArgs.iAmReady;
	clientArgs.abortFlag = &abortFlag;

	/* cold start, one loop */
	if(doTest(&clientArgs, &serverArgs, 1, &clientFirst, &serverFirst)) {
		exit(1);
	}
	
	/* now the real test */
	if(doTest(&clientArgs, &serverArgs, loops, &clientTotal, &serverTotal)) {
		exit(1);
	}
	
	printf("\n");
	
	printf("SSL Protocol Version : %s\n",
		sslGetProtocolVersionString(serverArgs.negotiatedProt));
	printf("SSL Cipher           : %s\n",
		sslGetCipherSuiteString(serverArgs.negotiatedCipher));
		
	printf("Client Handshake 1st : %f ms\n", clientFirst * 1000.0);
	printf("Server Handshake 1st : %f ms\n", serverFirst * 1000.0);
	printf("Client Handshake     : %f s in %u loops\n", clientTotal, loops);
	printf("                       %f ms per handshake\n", 
		(clientTotal * 1000.0) / loops);
	printf("Server Handshake     : %f s in %u loops\n", serverTotal, loops);
	printf("                       %f ms per handshake\n", 
		(serverTotal * 1000.0) / loops);

	return 0;
}



