/*
 * Copyright (c) 2006-2008,2010,2013 Apple Inc. All Rights Reserved.
 *
 * sslServe.cpp : perform one server side sesssion
 */

#include <Security/SecureTransport.h>
#include <Security/Security.h>
#include <clAppUtils/sslAppUtils.h>
#include <clAppUtils/ioSock.h>
#include <clAppUtils/sslThreading.h>
#include <utilLib/fileIo.h>
#include <utilLib/common.h>
#include <security_cdsa_utils/cuPrintCert.h>

#include <Security/SecBase.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/param.h>

#define BIND_RETRIES	10

#define SERVER_MESSAGE  "HTTP/1.0 200 OK\015\012\015\012" \
	"<HTML><HEAD><TITLE>SecureTransport Test Server</TITLE></HEAD>" \
	"<BODY><H2>Secure connection established.</H2>" \
	"Message from the 'sslServe' test library.\015\012</BODY>" \
	"</HTML>\015\012"

#define READBUF_LEN		256

/* relies on SSLSetProtocolVersionEnabled */

/*
 * params->lock is held for us by runSession() - we use it as a semapahore by
 * unlocking it when we've created a port to listen on. 
 * This is generally run from a thread via sslRunSession() and 
 * sslServerThread() in sslAppUtils.cpp. 
 */
OSStatus sslAppServe(
	SslAppTestParams	*params)
{
	otSocket			listenSock = 0;
	otSocket			acceptSock = 0;
    PeerSpec            peerId;
    OSStatus            ortn;
    SSLContextRef       ctx = NULL;
	SecKeychainRef		serverKc = nil;
	CFArrayRef			serverCerts = nil;
	
	sslThrDebug("Server", "starting");
    params->negVersion = kSSLProtocolUnknown;
    params->negCipher = SSL_NULL_WITH_NULL_NULL;
	params->ortn = noHardwareErr;
    
	/* set up a socket on which to listen */
	for(unsigned retry=0; retry<BIND_RETRIES; retry++) {
		ortn = ListenForClients(params->port, params->nonBlocking,
			&listenSock);
		switch(ortn) {
			case errSecSuccess:
				break;
			case errSecOpWr:
				/* port already in use - try another */
				params->port++;
				if(params->verbose || THREADING_DEBUG) {
					printf("...retrying ListenForClients at port %d\n",
						params->port);
				}
				break;
			default:
				break;
		}
		if(ortn != errSecOpWr) {
			break;
		}
	}
	
	/* let main thread know a socket is ready */
	if(pthread_mutex_lock(&params->pthreadMutex)) {
		printf("***Error acquiring server lock; aborting.\n");
		return -1;
	}
	params->serverReady = true;
	if(pthread_cond_broadcast(&params->pthreadCond)) {
		printf("***Error waking main thread; aborting.\n");
		return -1;
	}
	if(pthread_mutex_unlock(&params->pthreadMutex)) {
		printf("***Error acquiring server lock; aborting.\n");
		return -1;
	}

	if(ortn) {
		printf("ListenForClients returned %d; aborting\n", (int)ortn);
		return ortn;
	}

	/* wait for a connection */
	if(params->verbose) {
		printf("Waiting for client connection...");
		fflush(stdout);
	}
	ortn = AcceptClientConnection(listenSock, &acceptSock, &peerId);
    if(ortn) {
    	printf("AcceptClientConnection returned %d; aborting\n", (int)ortn);
    	return ortn;
    }

	/* 
	 * Set up a SecureTransport session.
	 */
	ortn = SSLNewContext(true, &ctx);
	if(ortn) {
		printSslErrStr("SSLNewContext", ortn);
		goto cleanup;
	} 
	ortn = SSLSetIOFuncs(ctx, SocketRead, SocketWrite);
	if(ortn) {
		printSslErrStr("SSLSetIOFuncs", ortn);
		goto cleanup;
	} 
	ortn = SSLSetConnection(ctx, (SSLConnectionRef)acceptSock);
	if(ortn) {
		printSslErrStr("SSLSetConnection", ortn);
		goto cleanup;
	}
	
	if(params->anchorFile) {
		ortn = sslAddTrustedRoot(ctx, params->anchorFile, 
			params->replaceAnchors);
		if(ortn) {
			goto cleanup;
		}
	}
	if(params->myCertKcName != NULL) {
		/* if not, better be trying anonymous diff-hellman... :-) */
		serverCerts = getSslCerts(params->myCertKcName, false, false, NULL,
			&serverKc);
		if(serverCerts == nil) {
			exit(1);
		}
		if(params->password) {
			ortn = SecKeychainUnlock(serverKc, strlen(params->password), 
					(void *)params->password, true);
			if(ortn) {
				printf("SecKeychainUnlock returned %d\n", (int)ortn);
				/* oh well */
			}
		}
		if(params->idIsTrustedRoot) {
			/* assume this is a root we want to implicitly trust */
			ortn = addIdentityAsTrustedRoot(ctx, serverCerts);
			if(ortn) {
				goto cleanup;
			}
		}
		ortn = SSLSetCertificate(ctx, serverCerts);
		if(ortn) {
			printSslErrStr("SSLSetCertificate", ortn);
			goto cleanup;
		}
	}
	
	if(params->disableCertVerify) {
		ortn = SSLSetEnableCertVerify(ctx, false);
		if(ortn) {
			printSslErrStr("SSLSetEnableCertVerify", ortn);
			goto cleanup;
		}
	}
	ortn = sslSetProtocols(ctx, params->acceptedProts, params->tryVersion);
	if(ortn) {
		goto cleanup;
	}
	if(params->resumeEnable) {
		ortn = SSLSetPeerID(ctx, &peerId, sizeof(PeerSpec));
		if(ortn) {
			printSslErrStr("SSLSetPeerID", ortn);
			goto cleanup;
		}
	}
	if(params->ciphers != NULL) {
		ortn = sslSetEnabledCiphers(ctx, params->ciphers);
		if(ortn) {
			goto cleanup;
		}
	}
	if(params->authenticate != kNeverAuthenticate) {
		ortn = SSLSetClientSideAuthenticate(ctx, params->authenticate);
		if(ortn) {
			printSslErrStr("SSLSetClientSideAuthenticate", ortn);
			goto cleanup;
		}
	}
	if(params->dhParams) {
		#if JAGUAR_BUILD
		printf("***Diffie-Hellman not supported in this config.\n");
		#else
		ortn = SSLSetDiffieHellmanParams(ctx, params->dhParams, 
			params->dhParamsLen);
		if(ortn) {
			printSslErrStr("SSLSetDiffieHellmanParams", ortn);
			goto cleanup;
		}
		#endif
	}

	/* Perform SSL/TLS handshake */
    do {   
		ortn = SSLHandshake(ctx);
	    if((ortn == errSSLWouldBlock) && !params->silent) {
	    	/* keep UI responsive */ 
	    	sslOutputDot();
	    }
    } while (ortn == errSSLWouldBlock);
	
	SSLGetClientCertificateState(ctx, &params->certState);
	SSLGetNegotiatedCipher(ctx, &params->negCipher);
	SSLGetNegotiatedProtocolVersion(ctx, &params->negVersion);
	
	if(params->verbose) {
		printf("\n");
	}
	if(ortn) {
		goto cleanup;
	}

	/* wait for one complete line */
	char readBuf[READBUF_LEN];
	size_t length;
	while(ortn == errSecSuccess) {
	    length = READBUF_LEN;
	    ortn = SSLRead(ctx, readBuf, length, &length);
	    if (ortn == errSSLWouldBlock) {
			/* keep trying */
	        ortn = errSecSuccess;
			continue;
	    }
	    if(length == 0) {
			/* keep trying */
			continue;
	    }
	    
	    /* poor person's line completion scan */
	    for(unsigned i=0; i<length; i++) {
	    	if((readBuf[i] == '\n') || (readBuf[i] == '\r')) {
	    		goto serverResp;
	    	}
	    }
	}
	
serverResp:
	/* send out canned response */
	ortn = SSLWrite(ctx, SERVER_MESSAGE, strlen(SERVER_MESSAGE), &length);
	if(ortn) {
		printSslErrStr("SSLWrite", ortn);
	}

cleanup:
	/*
	 * always do close, even on error - to flush outgoing write queue 
	 */
	if(ctx) {
		OSStatus cerr = SSLClose(ctx);
		if(ortn == errSecSuccess) {
			ortn = cerr;
		}
	}
	if(acceptSock) {
		while(!params->clientDone && !params->serverAbort) {
			usleep(100);
		}
		endpointShutdown(acceptSock);
	}
	if(listenSock) {
		endpointShutdown(listenSock);
	}
	if(ctx) {
	    SSLDisposeContext(ctx);  
	}    
	params->ortn = ortn;
	sslThrDebug("Server", "done");
	return ortn;
}
