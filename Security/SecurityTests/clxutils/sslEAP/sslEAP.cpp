/*
 * sslEAP - test EAP-FAST style PAC-based session resumption. 
 *
 * This only works with a debug Security.framework since server side
 * PAC support is not present in deployment builds.
 *
 * Written by Doug Mitchell. 
 */
#include <Security/SecureTransport.h>
#include <Security/SecureTransportPriv.h>
#include <clAppUtils/sslAppUtils.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <utilLib/common.h>
#include <clAppUtils/ringBufferIo.h>
#include "ringBufferThreads.h"		/* like the ones in clAppUtils, tailored for EAP/PAC */

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/param.h>
#include <CoreFoundation/CoreFoundation.h>
#include <security_utilities/devrandom.h>

#define DEFAULT_XFER	1024		/* total xfer size in bytes */

/* we might make these user-tweakable */
#define DEFAULT_NUM_BUFS	16
#define DEFAULT_BUF_SIZE	2048		/* in the ring buffers */
#define DEFAULT_CHUNK		1024		/* bytes to write per SSLWrite() */
#define SESSION_TICKET_SIZE	512

static void usage(char **argv)
{
    printf("Usage: %s [option ...]\n", argv[0]);
    printf("Options:\n");
	printf("   -x transferSize   -- default=%d; 0=forever\n", DEFAULT_XFER);
	printf("   -k keychainName   -- not needed if PAC will be done\n");
	printf("   -n                -- *NO* PAC\n");
	printf("   -h hostName       -- force a SSLSetPeerDomainName on client side\n");
	printf("   -p (pause on error)\n");
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
	RingBufferArgs	clientArgs;
	RingBufferArgs	serverArgs;
	bool			abortFlag = false;
	pthread_t		client_thread = NULL;
	int				result;
	OSStatus		ortn;
	unsigned char	sessionTicket[SESSION_TICKET_SIZE];
	int				ourRtn = 0;
	CFArrayRef		idArray = NULL;				/* for SSLSetCertificate */
	CFArrayRef		anchorArray = NULL;			/* trusted roots */
	char			*hostName = NULL;
	
	/* user-spec'd variables */
	char 			*kcName = NULL;
	unsigned 		xferSize = DEFAULT_XFER;
	bool			pauseOnError = false;
	bool			runForever = false;
	bool			skipPAC = false;

	extern int optind;
	extern char *optarg;
	int arg;
	optind = 1;
	while ((arg = getopt(argc, argv, "x:c:k:h:np")) != -1) {
		switch (arg) {
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
			case 'k':
				kcName = optarg;
				break;
			case 'n':
				skipPAC = true;
				break;
			case 'h':
				/* mainly to test EAP session ticket and ServerName simultaneously */
				hostName = optarg;
				break;
			case 'p':
				pauseOnError = true;
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

	/* get optional server SecIdentity */
	if(kcName) {
		SecKeychainRef	kcRef = NULL;
		SecCertificateRef anchorCert = NULL;
		SecIdentityRef	idRef = NULL;
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
		CFRelease(anchorCert);
	}

	/* set up server side */
	memset(&serverArgs, 0, sizeof(serverArgs));
	serverArgs.xferSize = xferSize;
	serverArgs.xferBuf = serverBuf;
	serverArgs.chunkSize = chunkSize;
	serverArgs.ringWrite = &serverToClientRing;
	serverArgs.ringRead = &clientToServerRing;
	serverArgs.goFlag = &clientArgs.iAmReady;
	serverArgs.abortFlag = &abortFlag;
	serverArgs.pauseOnError = pauseOnError;
	appGetRandomBytes(serverArgs.sharedSecret, SHARED_SECRET_SIZE);
	if(!skipPAC) {
		serverArgs.setMasterSecret = true;
	}
	serverArgs.idArray = idArray;
	serverArgs.trustedRoots = anchorArray;

	/* set up client side */
	memset(&clientArgs, 0, sizeof(clientArgs));
	clientArgs.xferSize = xferSize;
	clientArgs.xferBuf = clientBuf;
	clientArgs.chunkSize = chunkSize;
	clientArgs.ringWrite = &clientToServerRing;
	clientArgs.ringRead = &serverToClientRing;
	clientArgs.goFlag = &serverArgs.iAmReady;
	clientArgs.abortFlag = &abortFlag;
	clientArgs.pauseOnError = pauseOnError;
	memmove(clientArgs.sharedSecret, serverArgs.sharedSecret, SHARED_SECRET_SIZE);
	clientArgs.hostName = hostName;
	
	/* for now set up an easily recognizable ticket */
	for(unsigned dex=0; dex<SESSION_TICKET_SIZE; dex++) {
		sessionTicket[dex] = dex;
	}
	clientArgs.sessionTicket = sessionTicket;
	clientArgs.sessionTicketLen = SESSION_TICKET_SIZE;
	/* client always tries setting the master secret in this test */
	clientArgs.setMasterSecret = true;
	clientArgs.trustedRoots = anchorArray;

	/* fire up client thread */
	result = pthread_create(&client_thread, NULL, 
			rbClientThread, &clientArgs);
	if(result) {
		printf("***pthread_create returned %d, aborting\n", result);
		exit(1);
	}
	
	/* 
	 * And the server pseudo thread. This returns when all data has been transferred. 
	 */
	ortn = rbServerThread(&serverArgs);
	
	if(abortFlag) {
		printf("***Test aborted.\n");
		exit(1);
	}
	
	printf("\n");
	
	printf("SSL Protocol Version : %s\n",
		sslGetProtocolVersionString(serverArgs.negotiatedProt));
	printf("SSL Cipher           : %s\n",
		sslGetCipherSuiteString(serverArgs.negotiatedCipher));
		
	if(skipPAC) {
		if(clientArgs.sessionWasResumed) {
			printf("***skipPAC true, but client reported sessionWasResumed\n");
			ourRtn = -1;
		}
		if(serverArgs.sessionWasResumed) {
			printf("***skipPAC true, but server reported sessionWasResumed\n");
			ourRtn = -1;
		}
		if(ourRtn == 0) {
			printf("...PAC session attempted by client; refused by server;\n");
			printf("   Normal session proceeded correctly.\n");
		}
	}
	else {
		if(!clientArgs.sessionWasResumed) {
			printf("***client reported !sessionWasResumed\n");
			ourRtn = -1;
		}
		if(!serverArgs.sessionWasResumed) {
			printf("***server reported !sessionWasResumed\n");
			ourRtn = -1;
		}
		if(memcmp(clientBuf, serverBuf, DEFAULT_CHUNK)) {
			printf("***Data miscompare***\n");
			ourRtn = -1;
		}
		if(ourRtn == 0) {
			printf("...PAC session resumed correctly.\n");
		}
	}
	/* FIXME other stuff? */

	return ourRtn;
}



