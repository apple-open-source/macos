/*
 * Measure performance of SecureTransport - setup and sustained data
 * throughput. Single process version, no sockets - all data transfer
 * between client and server is via local memory shared between two 
 * threads.  
 *
 * Written by Doug Mitchell. 
 */
#include <Security/SecureTransport.h>
#include <clAppUtils/sslAppUtils.h>
#include <utilLib/fileIo.h>
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
#define DEFAULT_XFER	(1024 * 1024 * 20)	/* total xfer size in bytes */

/* we might make these user-tweakable */
#define DEFAULT_NUM_BUFS	16
#define DEFAULT_BUF_SIZE	2048		/* in the ring buffers */
#define DEFAULT_CHUNK		1024		/* bytes to write per SSLWrite() */

static void usage(char **argv)
{
    printf("Usage: %s [option ...]\n", argv[0]);
    printf("Options:\n");
	printf("   -k keychain (default = %s)\n", DEFAULT_KC);
	printf("   -x transferSize (default=%d; 0=forever)\n", DEFAULT_XFER);
	printf("   -c cipher (default = RSA/AES128\n");
	printf("      ciphers: a=RSA/AES128; r=RSA/RC4; d=RSA/DES; D=RSA/3DES;\n");
	printf("               h=DHA/RC4; H=DH/DSS/DES; A=AES256\n");
	printf("   -v version (t|2|3; default = t(TLS1)\n");
	printf("   -w password  (unlock server keychain with password)\n");
	printf("   -a (enable client authentication)\n");
	printf("   -p (pause on error)\n");
	printf("   -m (pause for malloc debug)\n");
    exit(1);
}

int main(int argc, char **argv)
{
	RingBuffer		serverToClientRing;
	RingBuffer		clientToServerRing;
	unsigned		numBufs = DEFAULT_NUM_BUFS;
	unsigned		bufSize = DEFAULT_BUF_SIZE;
	unsigned		chunkSize = DEFAULT_CHUNK;
	unsigned char	clientBuf[DEFAULT_CHUNK];
	unsigned char	serverBuf[DEFAULT_CHUNK];
	CFArrayRef		idArray;					/* for SSLSetCertificate */
	CFArrayRef		anchorArray;				/* trusted roots */
	SslRingBufferArgs clientArgs;
	SslRingBufferArgs serverArgs;
	SecKeychainRef	kcRef = NULL;
	SecCertificateRef anchorCert = NULL;
	SecIdentityRef	idRef = NULL;
	bool			abortFlag = false;
	pthread_t		client_thread = NULL;
	int				result;
	bool			diffieHellman = true;		/* FIXME needs work */
	OSStatus		ortn;
	
	/* user-spec'd variables */
	char 			*kcName = DEFAULT_KC;
	unsigned 		xferSize = DEFAULT_XFER;
	SSLCipherSuite 	cipherSuite = TLS_RSA_WITH_AES_128_CBC_SHA;
	SSLProtocol 	prot = kTLSProtocol1;
	char 			password[200];
	bool 			clientAuthEnable = false;
	bool			pauseOnError = false;
	bool			runForever = false;
	bool			mallocPause = false;
	
	password[0] = 0;

	extern int optind;
	extern char *optarg;
	int arg;
	optind = 1;
	while ((arg = getopt(argc, argv, "k:x:c:v:w:aBpm")) != -1) {
		switch (arg) {
			case 'k':
				kcName = optarg;
				break;
			case 'x':
			{
				unsigned xsize = atoi(optarg);
				if(xsize == 0) {
					runForever = true;
					/* and leave xferSize alone */
				}
				else {
					xferSize = xsize;
				}
				break;
			}
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
			case 'w':
				strcpy(password, optarg);
				break;
			case 'a':
				clientAuthEnable = true;
				break;
			case 'p':
				pauseOnError = true;
				break;
			case 'm':
				mallocPause = true;
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

	/* unlock keychain? */
	if(password[0]) {
		ortn = SecKeychainUnlock(kcRef, strlen(password), password, true);
		if(ortn) {
			cssmPerror("SecKeychainUnlock", ortn);
			/* oh well */
		}
	}
	CFRelease(kcRef);

	if(mallocPause) {
		fpurge(stdin);
		printf("Pausing for MallocDebug setup. CR to proceed: ");
		getchar();
	}
	
	/* set up server side */
	memset(&serverArgs, 0, sizeof(serverArgs));
	serverArgs.idArray = idArray;
	serverArgs.trustedRoots = anchorArray;
	serverArgs.xferSize = xferSize;
	serverArgs.xferBuf = serverBuf;
	serverArgs.chunkSize = chunkSize;
	serverArgs.runForever = runForever;
	serverArgs.cipherSuite = cipherSuite;
	serverArgs.prot = prot;
	serverArgs.ringWrite = &serverToClientRing;
	serverArgs.ringRead = &clientToServerRing;
	serverArgs.goFlag = &clientArgs.iAmReady;
	serverArgs.abortFlag = &abortFlag;
	serverArgs.pauseOnError = pauseOnError;

	/* set up client side */
	memset(&clientArgs, 0, sizeof(clientArgs));
	clientArgs.idArray = NULL;		/* until we do client auth */
	clientArgs.trustedRoots = anchorArray;
	clientArgs.xferSize = xferSize;
	clientArgs.xferBuf = clientBuf;
	clientArgs.chunkSize = chunkSize;
	clientArgs.runForever = runForever;
	clientArgs.cipherSuite = cipherSuite;
	clientArgs.prot = prot;
	clientArgs.ringWrite = &clientToServerRing;
	clientArgs.ringRead = &serverToClientRing;
	clientArgs.goFlag = &serverArgs.iAmReady;
	clientArgs.abortFlag = &abortFlag;
	clientArgs.pauseOnError = pauseOnError;
	
	/* fire up client thread */
	result = pthread_create(&client_thread, NULL, 
			sslRbClientThread, &clientArgs);
	if(result) {
		printf("***pthread_create returned %d, aborting\n", result);
		exit(1);
	}
	
	/* 
	 * And the server pseudo thread. This returns when all data has been transferred. 
	 */
	ortn = sslRbServerThread(&serverArgs);
	
	if(abortFlag) {
		printf("***Test aborted.\n");
		exit(1);
	}
	
	printf("\n");

	if(mallocPause) {
		fpurge(stdin);
		printf("End of test. Pausing for MallocDebug analysis. CR to proceed: ");
		getchar();
	}
	
	printf("SSL Protocol Version : %s\n",
		sslGetProtocolVersionString(serverArgs.negotiatedProt));
	printf("SSL Cipher           : %s\n",
		sslGetCipherSuiteString(serverArgs.negotiatedCipher));
		
	printf("SSL Handshake        : %f s\n", 
		serverArgs.startData - serverArgs.startHandshake);
	printf("Data Transfer        : %u bytes in %f s\n", (unsigned)xferSize, 
		serverArgs.endData - serverArgs.startHandshake);
	printf("                     : %.1f Kbytes/s\n", 
			xferSize / (serverArgs.endData - serverArgs.startHandshake) / 1024.0);
	return 0;
}



