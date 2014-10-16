/* sslPing.c - simple version sslPing test */

#include "testParams.h"
#include <stdlib.h>
#include <stdio.h>
#include <Security/SecureTransport.h>
#include "ioSockThr.h"
#include "testutil.h"
#include <security_utilities/threading.h>
#include <utilLib/common.h>

#define DEFAULT_GETMSG  	"GET / HTTP/1.0\r\n\r\n"
#define DEFAULT_PORT    	 443

#define LOCALHOST_RANGE			0

#define ALLOW_ANY_ROOT			0

/*
 * List of hosts. All support all three protocols and access to "/".
 */
typedef struct {
	const char *hostName;
	unsigned short port;
} sslHostDef;

#if		LOCALHOST_RANGE
static const sslHostDef knownSslHosts[] = 
{
	{ "localhost", 1300 },
	{ "localhost", 1301 },
	{ "localhost", 1302 },
	{ "localhost", 1303 },
	{ "localhost", 1304 },
	{ "localhost", 1305 },
	{ "localhost", 1306 },
	{ "localhost", 1307 }
};
#else	/* LOCALHOST_RANGE */
static const sslHostDef knownSslHosts[] = 
{
	{"www.amazon.com", DEFAULT_PORT },
	{"store.apple.com", DEFAULT_PORT },
	{"www.thawte.com", DEFAULT_PORT },
	{"account.authorize.net", DEFAULT_PORT },
	{"gmail.google.com", DEFAULT_PORT },
	{"digitalid.verisign.com", DEFAULT_PORT},
	{"www.firstamlink.com", DEFAULT_PORT},
	{"remote.harpercollins.com", DEFAULT_PORT},
	{"mbanxonlinebanking.harrisbank.com", DEFAULT_PORT},
};
#endif	/* LOCALHOST_RANGE */
#define NUM_KNOWN_HOSTS (sizeof(knownSslHosts) / sizeof(sslHostDef))

/* for memory leak debug only, with only one thread running */
#define DO_PAUSE			0

/*
 * Snag test-specific opts. 
 *
 * -- [23t] for SSL2, SSL3, TLS1 only operation. Default is all, randomly.
 * -- m - multi sites; default is just one
 * -- r - enable resumable sessions.  
 */
static int initFlag;
static SSLProtocol globalTryProt = kSSLProtocolUnknown;
static const char *globalProtStr = NULL;
static bool justOneHost = 1;

/*
 * Enable resumable sessions. Setting this true exercises the session cache
 * logic in ST but significantly decreases the testing of most of the
 * rest of the handshaking (including cert chain verification). 
 * Also, when this is true, once a given site has negotiated a given
 * protocol version, ST disallows negotiation of a higher version with
 * that site.
 */
static bool resumeEnable = 0;


int sslPingInit(TestParams *testParams)
{
	if(initFlag) {
		return 0;
	}
	if(testParams->testOpts == NULL) {
		initFlag = 1;
		return 0;
	}
	char *testOpts;
	for(testOpts=testParams->testOpts; *testOpts; testOpts++) {
		switch(*testOpts) {
			case '2':
				globalTryProt = kSSLProtocol2;
				globalProtStr = "SSL2";
				break;
			case '3':
				globalTryProt = kSSLProtocol3Only;
				globalProtStr = "SSL3";
				break;
			case 't':
				globalTryProt = kTLSProtocol1Only;
				globalProtStr = "TLS1";
				break;
			case 'm':
				justOneHost = 0;
				break;
			case 'r':
				resumeEnable = 1;
				break;
			default:
				/* for other tests */
				break;
		}
	}
	if(!testParams->quiet) {
		printf("...sslPing using %s only\n", globalProtStr);
	}
	initFlag = 1;
	return 0;
}


/* gethostbyname, called by MakeServerConnection, is not thread-safe. */
static Mutex connectLock;

#define ENABLE_SSL2 0

/* 
 * Roll the dice and select a random host and SSL protocol 
 */
static const char *selectHostAndProt(
	unsigned short &port,
	SSLProtocol &tryProt,
	const char *&protStr)
{
	unsigned char r[2];
	
	appGetRandomBytes(r, 2);
	if(globalTryProt != kSSLProtocolUnknown) {
		/* user spec'd at cmd line */
		tryProt = globalTryProt;
		protStr = globalProtStr;
	}
	else {
        unsigned modulo = ENABLE_SSL2 ? 5 : 4;
		switch(r[0] % modulo) {
			case 0:	
				tryProt = kSSLProtocol3; 
				protStr = "SSL3";
				break;
			case 1:	
				tryProt = kSSLProtocol3Only; 
				protStr = "SSL3Only";
				break;
			case 2:	
				tryProt = kTLSProtocol1; 
				protStr = "TLS1";
				break;
			case 3:	
				tryProt = kTLSProtocol1Only; 
				protStr = "TLS1Only";
				break;
			case 4:	
				tryProt = kSSLProtocol2; 
				protStr = "SSL2";
				break;
			default:
				printf("Huh?\n");
				exit(1);
		}
	}
	const sslHostDef *hostDef;
	if(justOneHost) {
		hostDef = &knownSslHosts[0];
	}
	else {
		hostDef = &(knownSslHosts[r[1] % NUM_KNOWN_HOSTS]);
	}
	port = hostDef->port;
	return hostDef->hostName;
}
	
/*
 * Perform one SSL diagnostic session. Returns nonzero on error. Normally no
 * output to stdout except initial "connecting to" message, unless there 
 * is a really screwed up error (i.e., something not directly related 
 * to the SSL conection). 
 */
#define RCV_BUF_SIZE		256

static OSStatus doSslPing(
	SSLProtocol				tryVersion,
	const char				*hostName,			// e.g., "www.amazon.com"
	unsigned short			port,
	const char				*getMsg,			// e.g., "GET / HTTP/1.0\r\n\r\n" 
	CSSM_BOOL				allowExpired,
	CSSM_BOOL				keepConnected,
	CSSM_BOOL				requireNotify,		// require closure notify in V3 mode
	SSLProtocol				*negVersion,		// RETURNED
	SSLCipherSuite			*negCipher)			// RETURNED
{
    PeerSpec            peerId;
	otSocket			sock = 0;
    OSStatus            ortn;
    SSLContextRef       ctx = NULL;
    size_t              length;
	size_t				actLen;
    uint8               rcvBuf[RCV_BUF_SIZE];
	
    *negVersion = kSSLProtocolUnknown;
    *negCipher = SSL_NULL_WITH_NULL_NULL;
    
	/* first make sure requested server is there */
	connectLock.lock();
	ortn = MakeServerConnection(hostName, port, &sock, &peerId);
	connectLock.unlock();
    if(ortn) {
    	printf("MakeServerConnection(%s) returned %d; aborting\n", 
			hostName, (int)ortn);
    	return ortn;
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
	ortn = SSLSetProtocolVersion(ctx, tryVersion);
	if(ortn) {
		printSslErrStr("SSLSetProtocolVersion", ortn);
		goto cleanup;
	} 
	ortn = SSLSetConnection(ctx, (SSLConnectionRef)sock);
	if(ortn) {
		printSslErrStr("SSLSetConnection", ortn);
		goto cleanup;
	}
	if(resumeEnable) {
		ortn = SSLSetPeerID(ctx, &peerId, sizeof(PeerSpec));
		if(ortn) {
			printSslErrStr("SSLSetPeerID", ortn);
			goto cleanup;
		}
	}
	
	/* 
	 * SecureTransport options.
	 */ 
	if(allowExpired) {
		ortn = SSLSetAllowsExpiredCerts(ctx, true);
		if(ortn) {
			printSslErrStr("SSLSetAllowExpiredCerts", ortn);
			goto cleanup;
		}
	}
	
	#if ALLOW_ANY_ROOT
	ortn = SSLSetAllowsAnyRoot(ctx, true);
	if(ortn) {
		printSslErrStr("SSLSetAllowAnyRoot", ortn);
		goto cleanup;
	}
	#endif
	
    do
    {   ortn = SSLHandshake(ctx);
	    if(ortn == errSSLWouldBlock) {
	    	/* keep UI responsive */ 
	    	// outputDot();
	    }
    } while (ortn == errSSLWouldBlock);
	
	/* this works even if handshake failed due to cert chain invalid */
	// not for this version... copyPeerCerts(ctx, peerCerts);

	SSLGetNegotiatedCipher(ctx, negCipher);
	SSLGetNegotiatedProtocolVersion(ctx, negVersion);
	
    if(ortn) {
		printf("\n");
    	goto cleanup;
    }

	length = strlen(getMsg);
	ortn = SSLWrite(ctx, getMsg, length, &actLen);

	/* 
	 * Try to snag RCV_BUF_SIZE bytes. Exit if (!keepConnected and we get any data
	 * at all), or (keepConnected and err != (none, wouldBlock)).
	 */
    while (1) {   
		actLen = 0;
        ortn = SSLRead(ctx, rcvBuf, RCV_BUF_SIZE, &actLen);
        if(actLen == 0) {
        	// outputDot();
        }
        if (ortn == errSSLWouldBlock) {
			/* for this loop, these are identical */
            ortn = noErr;
        }
		// if((actLen > 0) && dumpRxData) {
		// not here...	dumpAscii(rcvBuf, actLen);
		// }
		if(keepConnected) {
			if(ortn != noErr) {
				/* connection closed by server or by error */
				break;
			}
		}
		else if(actLen > 0) {
        	/* good enough, we connected */
        	break;
        }
    }
	//printf("\n");
	
    /* convert normal "shutdown" into zero err rtn */
	if(ortn == errSSLClosedGraceful) {
		ortn = noErr;
	}
	if((ortn == errSSLClosedNoNotify) && !requireNotify) {
		/* relaxed disconnect rules */
		ortn = noErr;
	}
    if (ortn == noErr) {
        ortn = SSLClose(ctx);
	}
cleanup:
	if(sock) {
		endpointShutdown(sock);
	}
	if(ctx) {
	    SSLDisposeContext(ctx);  
	}    
	return ortn;
}

int sslPing(TestParams *testParams)
{
	unsigned 		loopNum;
	SSLProtocol		negVersion;
	SSLProtocol		tryVersion;
	const char 		*hostName;
	unsigned short	port;
	SSLCipherSuite	negCipher;
	OSStatus		err;
	const char		*protStr;
	
	for(loopNum=0; loopNum<testParams->numLoops; loopNum++) {
		if(!testParams->quiet) {
			printChar(testParams->progressChar);
		}
		hostName = selectHostAndProt(port, tryVersion, protStr);
		if(testParams->verbose) {
			printf("\nConnecting to host %s with %s...", 
				hostName, protStr); 
			fflush(stdout);
		}
		err = doSslPing(tryVersion,
			hostName,	
			port,
			DEFAULT_GETMSG,
			CSSM_FALSE,				// allowExpired
			CSSM_FALSE,				// keepConnected
			CSSM_FALSE,				// requireNotify
			&negVersion,
			&negCipher);
		if(err) {
			printf("sslPing error (%d)\n", (int)err);
			break;
		}
		if(testParams->verbose) {
			switch(negVersion) {
				case kSSLProtocol2:
					printf("negVersion = SSL2\n");
					break;
				case kSSLProtocol3:
					printf("negVersion = SSL3\n");
					break;
				case kTLSProtocol1:
					printf("negVersion = TLS1\n");
					break;
				default:
					printf("unknown negVersion! (%d)\n", 
						(int)negVersion);
					break;
			}
		}
		#if DO_PAUSE
		fpurge(stdin);
		printf("Hit CR to proceed: ");
		getchar();
		#endif
	}
	return (int)err;
}

