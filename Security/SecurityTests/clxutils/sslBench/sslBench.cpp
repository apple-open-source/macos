/*
 * Measure performance of SecureTransport - setup and sustained data
 * throughput.
 *
 * Written by Doug Mitchell. 
 */
#include <Security/SecureTransport.h>
#include <clAppUtils/sslAppUtils.h>
#include <clAppUtils/ioSock.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <utilLib/common.h>

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/param.h>
#include <CoreFoundation/CoreFoundation.h>
#include <security_utilities/devrandom.h>

/* default - run both server and client on this machine, port 1200  */
#define HOST_DEF		"localhost"
#define PORT_DEF		1200

/* default keychain */
#define DEFAULT_KC		"localcert"

#define XFERSIZE_DEF	(1024 * 1024)
#define	BUFSIZE			1024

#define DH_PARAM_FILE	"dhParams_1024.der"

static void usage(char **argv)
{
    printf("Usage: %s s[erver]|c[lient] [option ...]\n", argv[0]);
    printf("Options:\n");
	printf("   -h hostname (default = %s)\n", HOST_DEF);
	printf("   -p port (default = %d)\n", PORT_DEF);
	printf("   -k keychain (default = %s)\n", DEFAULT_KC);
	printf("   -x transferSize (default = %d)\n", XFERSIZE_DEF);
	printf("   -c cipher (default = RSA/AES128; server side only)\n");
	printf("      ciphers: r=RSA/RC4; d=RSA/DES; D=RSA/3DES; h=DHA/RC4; "
					"H=DH/DSS/DES; A=AES256\n");
	printf("   -v version (t|2|3 default = t(TLS1); server side only)\n");
	printf("   -b bufsize (default = %d)\n", BUFSIZE);
	printf("   -w password  (unlock server keychain with password)\n");
	printf("   -a (enable client authentication)\n");
	printf("   -B non-blocking I/O\n");
    exit(1);
}


int main(int argc, char **argv)
{
	/* user-spec'd variables */
	const char 		*kcName = DEFAULT_KC;
	unsigned 		xferSize = XFERSIZE_DEF;
	int 			port = PORT_DEF;
	const char 		*hostName = HOST_DEF;
	SSLCipherSuite 	cipherSuite = TLS_RSA_WITH_AES_128_CBC_SHA;
	SSLProtocol 	prot = kTLSProtocol1Only;
	char 			password[200];
	bool 			clientAuthEnable = false;
	bool 			isServer = false;
	unsigned 		bufSize = BUFSIZE;
	bool			diffieHellman = false;
	int				nonBlocking = 0;
	
	if(argc < 2) {
		usage(argv);
	}
	password[0] = 0;
	switch(argv[1][0]) {
		case 's':
			isServer = true;
			break;
		case 'c':
			isServer = false;
			break;
		default:
			usage(argv);
	}
	
	extern int optind;
	extern char *optarg;
	int arg;
	optind = 2;
	while ((arg = getopt(argc, argv, "h:p:k:x:c:v:w:b:aB")) != -1) {
		switch (arg) {
			case 'h':
				hostName = optarg;
				break;
			case 'p':
				port = atoi(optarg);
				break;
			case 'k':
				kcName = optarg;
				break;
			case 'x':
				xferSize = atoi(optarg);
				break;
			case 'c':
				if(!isServer) {
					printf("***Specify cipherSuite on server side.\n");
					exit(1);
				}
				switch(optarg[0]) {
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
				if(!isServer) {
					printf("***Specify protocol on server side.\n");
					exit(1);
				}
				switch(optarg[0]) {
					case 't':
						prot = kTLSProtocol1Only;
						break;
					case '2':
						prot = kSSLProtocol2;
						break;
					case '3':
						prot = kSSLProtocol3Only;
						break;
					default:
						usage(argv);
				}
				break;
			case 'w':
				strcpy(password, optarg);
				break;
			case 'b':
				bufSize = atoi(optarg);
				break;
			case 'a':
				clientAuthEnable = true;
				break;
			case 'B':
				nonBlocking = 1;
				break;
			default:
				usage(argv);
		}
	}
	
	/* per-transfer buffer - make it random for server */
	char *buf = (char *)malloc(bufSize);
	if(isServer) {
		Security::DevRandomGenerator rng;
		rng.random(buf, bufSize);
	}
	
	/* gather Diffie-Hellman params from cwd */
	unsigned char *dhParams = NULL;
	unsigned dhParamsLen = 0;
	if(diffieHellman && isServer) {
		if(readFile(DH_PARAM_FILE, &dhParams, &dhParamsLen)) {
			printf("***Error reading Diffie-Hellman Params. Prepare to "
				"wait for a minute during SSL handshake.\n");
		}
	}
	
	/*
	 * Open keychain; both sides use the same one.
	 */
	OSStatus ortn;
	SecKeychainRef certKc = NULL;
	CFAbsoluteTime kcOpenStart = CFAbsoluteTimeGetCurrent();
	ortn = SecKeychainOpen(kcName, &certKc);
	if(ortn) {
		printf("Error opening keychain %s (%d); aborting.\n",
			kcName, (int)ortn);
		exit(1);
	}
	if(password[0]) {
		ortn = SecKeychainUnlock(certKc, strlen(password), password, true);
		if(ortn) {
			printf("SecKeychainUnlock returned %d\n", (int)ortn);
			/* oh well */
		}
	}
	CFAbsoluteTime kcOpenEnd = CFAbsoluteTimeGetCurrent();
	
	otSocket peerSock = 0;
	otSocket listenSock = 0;			// for server only
    PeerSpec peerId;
	
	if(isServer) {
		printf("...listening for client connection on port %d\n", port);
		ortn = ListenForClients(port, nonBlocking, &listenSock);
		if(ortn) {
			printf("...error establishing a listen socket. Aborting.\n");
			exit(1);
		}
		ortn = AcceptClientConnection(listenSock, &peerSock, &peerId);
		if(ortn) {
			printf("...error listening for connection. Aborting.\n");
			exit(1);
		}
	}
	else {
		printf("...connecting to host %s at port %d\n", hostName, port);
		ortn = MakeServerConnection(hostName, port, nonBlocking, &peerSock,
			&peerId);
		if(ortn) {
			printf("...error connecting to server %s. Aborting.\n",
				hostName);
			exit(1);
		}
	}

	/* start timing SSL setup */
	CFAbsoluteTime setupStart = CFAbsoluteTimeGetCurrent();

	SSLContextRef ctx;
	ortn = SSLNewContext(isServer, &ctx);
	if(ortn) {
		printSslErrStr("SSLNewContext", ortn);
		exit(1);
	} 
	ortn = SSLSetIOFuncs(ctx, SocketRead, SocketWrite);
	if(ortn) {
		printSslErrStr("SSLSetIOFuncs", ortn);
		exit(1);
	} 
	ortn = SSLSetConnection(ctx, (SSLConnectionRef)peerSock);
	if(ortn) {
		printSslErrStr("SSLSetConnection", ortn);
		exit(1);
	}
	ortn = SSLSetPeerDomainName(ctx, hostName, strlen(hostName) + 1);
	if(ortn) {
		printSslErrStr("SSLSetPeerDomainName", ortn);
		exit(1);
	}	
	
	/*
	 * Server/client specific setup.
	 *
	 * Client uses the same keychain as server, but it uses it for 
	 * sslAddTrustedRoots() instead of getSslCerts() and 
	 * SSLSetCertificate().
	 */
	CFArrayRef myCerts = NULL;
	if(clientAuthEnable || isServer) {
		myCerts = sslKcRefToCertArray(certKc, CSSM_FALSE, CSSM_FALSE, NULL, NULL);
		if(myCerts == NULL) {
			exit(1);
		}
		ortn = addIdentityAsTrustedRoot(ctx, myCerts);
		if(ortn) {
			exit(1);
		}
		ortn = SSLSetCertificate(ctx, myCerts);
		if(ortn) {
			printSslErrStr("SSLSetCertificate", ortn);
			exit(1);
		}
	}
	if(isServer) {
		SSLAuthenticate auth;
		if(clientAuthEnable) {
			auth = kAlwaysAuthenticate;
		}
		else {
			auth = kNeverAuthenticate;
		}
		ortn = SSLSetClientSideAuthenticate(ctx, auth);
		if(ortn) {
			printSslErrStr("SSLSetClientSideAuthenticate", ortn);
			exit(1);
		}
		ortn = SSLSetEnabledCiphers(ctx, &cipherSuite, 1);
		if(ortn) {
			printSslErrStr("SSLSetEnabledCiphers", ortn);
			exit(1);
		}
		ortn = SSLSetProtocolVersion(ctx, prot);
		if(ortn) {
			printSslErrStr("SSLSetProtocolVersion", ortn);
			exit(1);
		}
		if(dhParams != NULL) {
			ortn = SSLSetDiffieHellmanParams(ctx, dhParams, dhParamsLen);
			if(ortn) {
				printSslErrStr("SSLSetDiffieHellmanParams", ortn);
				exit(1);
			}
		}
	}
	else {
		/* client setup */
		if(!clientAuthEnable) {
			/* We're not presenting a cert; trust the server certs */
			bool foundOne;
			ortn = sslAddTrustedRoots(ctx, certKc, &foundOne);
			if(ortn) {
				printSslErrStr("sslAddTrustedRoots", ortn);
				exit(1);
			}
		}
	}
	
	/*
	 * Context setup complete. Start timing handshake.
	 */
	CFAbsoluteTime hshakeStart = CFAbsoluteTimeGetCurrent();
    do {   
		ortn = SSLHandshake(ctx);
    } while (ortn == errSSLWouldBlock);
	if(ortn) {
		printSslErrStr("SSLHandshake", ortn);
		exit(1);
	}
	CFAbsoluteTime hshakeEnd = CFAbsoluteTimeGetCurrent();
	
	/* snag these before data xfer possibly shuts down connection */
	SSLProtocol	negVersion;
	SSLCipherSuite negCipher;
	SSLClientCertificateState certState;		// RETURNED

	SSLGetNegotiatedCipher(ctx, &negCipher);
	SSLGetNegotiatedProtocolVersion(ctx, &negVersion);
	SSLGetClientCertificateState(ctx, &certState);
	
	/* server sends xferSize bytes to client and shuts down */
	size_t bytesMoved;
	
	CFAbsoluteTime dataStart = CFAbsoluteTimeGetCurrent();
	size_t totalMoved = 0;
	if(isServer) {
		size_t bytesToGo = xferSize;
		bool done = false;
		do {
			size_t thisMove = bufSize;
			if(thisMove > bytesToGo) {
				thisMove = bytesToGo;
			}
			ortn = SSLWrite(ctx, buf, thisMove, &bytesMoved);
			switch(ortn) {
				case noErr:
				case errSSLWouldBlock:
					break;
				default:
					done = true;
					break;
			}
			bytesToGo -= bytesMoved;
			totalMoved += bytesMoved;
			if(bytesToGo == 0) {
				done = true;
			}
		} while(!done);
		if(ortn != noErr) {
			printSslErrStr("SSLWrite", ortn);
			exit(1);
		}
	}
	else {
		/* client reads until error or errSSLClosedGraceful */
		bool done = false;
		do {
			ortn = SSLRead(ctx, buf, bufSize, &bytesMoved);
			switch(ortn) {
				case errSSLClosedGraceful:
					done = true;
					break;
				case noErr:
				case errSSLWouldBlock:
					break;
				default:
					done = true;
					break;
			}
			totalMoved += bytesMoved;
		} while(!done);
		if(ortn != errSSLClosedGraceful) {
			printSslErrStr("SSLRead", ortn);
			exit(1);
		}
	}
	
	/* shut down channel */
	ortn = SSLClose(ctx);
	if(ortn) {
		printSslErrStr("SSLCLose", ortn);
		exit(1);
	}
	CFAbsoluteTime dataEnd = CFAbsoluteTimeGetCurrent();

	/* how'd we do? */
	printf("SSL version          : %s\n", 
		sslGetProtocolVersionString(negVersion));
	printf("CipherSuite          : %s\n",
		sslGetCipherSuiteString(negCipher));
	printf("Client Cert State    : %s\n",
			sslGetClientCertStateString(certState));

	if(password[0]) {
		printf("keychain open/unlock : ");
	}
	else {
		printf("keychain open        : ");
	}
	printf("%f s\n", kcOpenEnd - kcOpenStart);
	printf("SSLContext setup     : %f s\n", hshakeStart - setupStart);
	printf("SSL Handshake        : %f s\n", hshakeEnd - hshakeStart);
	printf("Data Transfer        : %u bytes in %f s\n", (unsigned)totalMoved, 
		dataEnd - dataStart);
	printf("                     : %.1f Kbytes/s\n", 
			totalMoved / (dataEnd - dataStart) / 1024.0);
	return 0;
}



