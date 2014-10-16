/*
 * Measure performance of SecureTransport handshake
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

/* default - run both server and client on this machine, port 1200  */
#define HOST_DEF		"localhost"
#define PORT_DEF		1200

/* default keychain */
#define DEFAULT_KC		"localcert"

#define DH_PARAM_FILE	"dhParams_1024.der"

#define GET_MSG			"GET / HTTP/1.0\r\n\r\n"

static void usage(char **argv)
{
    printf("Usage: %s s[erver]|c[lient] loops [option ...]\n", argv[0]);
    printf("Options:\n");
	printf("   -h hostname (default = %s)\n", HOST_DEF);
	printf("   -p port (default = %d)\n", PORT_DEF);
	printf("   -k keychain (default = %s)\n", DEFAULT_KC);
	printf("   -c cipher (default = RSA/RC4; server side only)\n");
	printf("      ciphers: r=RSA/RC4; d=RSA/DES; D=RSA/3DES; h=DHA/RC4; "
					"H=DH/DSS/DES\n");
	printf("   -v version (t|2|3 default = t(TLS1); server side only)\n");
	printf("   -w password  (unlock server keychain with password)\n");
	printf("   -a (enable client authentication)\n");
	printf("   -r (resumable session enabled; default is disabled)\n");
	printf("   -n (No client side anchor specification; root is in system KC)\n");
	printf("   -d (disable cert verify)\n");
	printf("   -o (Allow hostname spoofing)\n");
	printf("   -g Send GET msg (needs for talking to real servers)\n");
	printf("   -V (verbose)\n");
    exit(1);
}

#include <signal.h>

void sigpipe(int sig) 
{ 
	fflush(stdin);
	printf("***SIGPIPE***\n");
}

int main(int argc, char **argv)
{
	/* user-spec'd variables */
	unsigned		loops;
	char 			*kcName = DEFAULT_KC;
	int 			port = PORT_DEF;
	char 			*hostName = HOST_DEF;
	SSLCipherSuite 	cipherSuite = SSL_RSA_WITH_RC4_128_SHA;
	SSLProtocol 	prot = kTLSProtocol1Only;
	char 			password[200];
	bool 			clientAuthEnable = false;
	bool 			isServer = false;
	bool			diffieHellman = false;
	bool 			verbose = false;
	bool			resumeEnable = false;
	bool			setClientAnchor = true;
	bool			certVerifyEnable = true;
	bool			checkHostName = true;
	bool			sendGet = false;
	
	otSocket 		listenSock = 0;			// for server only
	CFArrayRef 		myCerts = NULL;
	
	signal(SIGPIPE, sigpipe);
	if(argc < 3) {
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
	loops = atoi(argv[2]);
	if(loops == 0) {
		usage(argv);
	}
	
	extern int optind;
	extern char *optarg;
	int arg;
	optind = 3;
	while ((arg = getopt(argc, argv, "h:p:k:x:c:v:w:b:aVrndog")) != -1) {
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
			case 'a':
				clientAuthEnable = true;
				break;
			case 'V':
				verbose = true;
				break;
			case 'r':
				resumeEnable = true;
				break;
			case 'n':
				setClientAnchor = false;
				break;
			case 'd':
				certVerifyEnable = false;
				break;
			case 'o':
				checkHostName = false;
				break;
			case 'g':
				sendGet = true;
				break;
			default:
				usage(argv);
		}
	}
	
	/* gather Diffie-Hellman params from cwd */
	if(JAGUAR_BUILD && diffieHellman) {
		printf("***SOrry, DIffie Hellman not available in this config.\n");
		exit(1);
	}
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
	if(isServer || clientAuthEnable || setClientAnchor) {
		ortn = SecKeychainOpen(kcName, &certKc);
		if(ortn) {
			printf("Error opening keychain %s (%d); aborting.\n",
				kcName, (int)ortn);
			exit(1);
		}
		if(password[0]) {
			ortn = SecKeychainUnlock(certKc, strlen(password), password, true);
			if(ortn) {
				printf("SecKeychainUnlock returned %lu\n", ortn);
				/* oh well */
			}
		}
	}

	/* just do this once */
	if(clientAuthEnable || isServer || setClientAnchor) {
		myCerts = sslKcRefToCertArray(certKc, CSSM_FALSE, CSSM_TRUE, NULL);
		if(myCerts == NULL) {
			exit(1);
		}
	}
	
	/* server sets up listen port just once */
	if(isServer) {
		printf("...listening for client connection on port %d\n", port);
		ortn = ListenForClients(port, 0, &listenSock);
		if(ortn) {
			printf("...error establishing a listen socket. Aborting.\n");
			exit(1);
		}
	}

	CFAbsoluteTime setupTotal = 0;
	CFAbsoluteTime handShakeTotal = 0;
	
	for(unsigned loop=0; loop<loops; loop++) {
		otSocket peerSock = 0;
		PeerSpec peerId;
		
		if(isServer) {
			ortn = AcceptClientConnection(listenSock, &peerSock, &peerId);
			if(ortn) {
				printf("...error listening for connection. Aborting.\n");
				exit(1);
			}
		}
		else {
			/* client side */
			if(verbose) {
				printf("...connecting to host %s at port %d\n", hostName, port);
			}
			ortn = MakeServerConnection(hostName, port, 0, &peerSock,
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
		ortn = SSLSetConnection(ctx, peerSock);
		if(ortn) {
			printSslErrStr("SSLSetConnection", ortn);
			exit(1);
		}
		if(checkHostName) {
			ortn = SSLSetPeerDomainName(ctx, hostName, strlen(hostName) + 1);
			if(ortn) {
				printSslErrStr("SSLSetPeerDomainName", ortn);
				exit(1);
			}	
		}
		if(resumeEnable) {
			ortn = SSLSetPeerID(ctx, &peerId, sizeof(PeerSpec));
			if(ortn) {
				printSslErrStr("SSLSetPeerID", ortn);
				exit(1);
			}
		}
		if(!certVerifyEnable) {
			/*
			 * Do this before setting up certs to allow for optimization 
			 * on server side (setting valid cert without verifying them)
			 */
			ortn = SSLSetEnableCertVerify(ctx, false);
			if(ortn) {
				printSslErrStr("SSLSetPeerID", ortn);
				exit(1);
			}
		}
		
		/*
		 * Server/client specific setup.
		 *
		 * Client uses the same keychain as server, but it uses it for 
		 * sslAddTrustedRoots() instead of getSslCerts() and 
		 * SSLSetCertificate().
		 */
		if(clientAuthEnable || isServer) {
			if(!certVerifyEnable) {
				/* don't bother...this is heavyweight */
				ortn = addIdentityAsTrustedRoot(ctx, myCerts);
				if(ortn) {
					exit(1);
				}
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
			#if 0
			/* FIXME why does this fail on Jaguar? */
			ortn = SSLSetProtocolVersion(ctx, prot);
			if(ortn) {
				printSslErrStr("SSLSetProtocolVersion", ortn);
				exit(1);
			}
			#endif
			#if !JAGUAR_BUILD
			if(dhParams != NULL) {
				ortn = SSLSetDiffieHellmanParams(ctx, dhParams, dhParamsLen);
				if(ortn) {
					printSslErrStr("SSLSetDiffieHellmanParams", ortn);
					exit(1);
				}
			}
			#endif
		}
		else {
			/* client setup */
			if(!clientAuthEnable && setClientAnchor) {
				/* We're not presenting a cert; trust the server certs */
				bool foundOne;
				if(certKc == NULL) {
					printf("sslAddTrustedRoots screwup\n");
					exit(1);
				}
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
			
		/*
		 * Shut down. Server does the SSLClose while client tries to read. Client gets
		 * a errSSLClosedGraceful then does its SSLCLose.
		 */
		if(!isServer) {
			char data[2];
			size_t actRead;
			bool done = false;
			if(sendGet) {
				ortn = SSLWrite(ctx, GET_MSG, strlen(GET_MSG), &actRead);
				if(ortn) {
					printSslErrStr("SSLWrite", ortn);
				}
			}
			do {
				ortn = SSLRead(ctx, data, 2, &actRead);
				switch(ortn) {
					case errSSLClosedGraceful:
						done = true;
						break;
					case noErr:
						/* try again */
						break;
					default:
						printf("Unexpected rtn on client SSLRead(); bytesRead %u\n",
							(unsigned)actRead);
						printSslErrStr("SSLRead", ortn);
						done = true;
						/* ah, keep going */
						break;
				}
			} while(!done);
		}
		/* shut down channel */
		ortn = SSLClose(ctx);
		if(ortn) {
			printSslErrStr("SSLCLose", ortn);
			exit(1);
		}
	
		/* how'd we do? */
		if(verbose) {
			printf("SSL version          : %s\n", 
				sslGetProtocolVersionString(negVersion));
			printf("CipherSuite          : %s\n",
				sslGetCipherSuiteString(negCipher));
			printf("Client Cert State    : %s\n",
					sslGetClientCertStateString(certState));
			printf("SSLContext setup     : %f s\n", hshakeStart - setupStart);
			printf("SSL Handshake        : %f s\n", hshakeEnd - hshakeStart);
		}
		setupTotal     += (hshakeStart - setupStart);
		handShakeTotal += (hshakeEnd - hshakeStart);
	}
	
	printf("\n");
	printf("SSL setup     avg %f s\n", setupTotal / loops);
	printf("SSL handshake avg %f s\n", handShakeTotal / loops);
	return 0;
}



