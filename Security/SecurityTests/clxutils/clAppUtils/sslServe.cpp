/*
 * sslServe.cpp : perform one server side sesssion 
 */
#include <Security/SecureTransport.h>
#include <Security/Security.h>
#include <clAppUtils/sslAppUtils.h>
#include <clAppUtils/ioSock.h>
#include <clAppUtils/sslThreading.h>
#include <clAppUtils/ringBufferIo.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <utilLib/common.h>
#include <security_cdsa_utils/cuPrintCert.h>

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/param.h>

#define BIND_RETRIES	50

#define SERVER_MESSAGE  "HTTP/1.0 200 OK\015\012\015\012" \
	"<HTML><HEAD><TITLE>SecureTransport Test Server</TITLE></HEAD>" \
	"<BODY><H2>Secure connection established.</H2>" \
	"Message from the 'sslServe' test library.\015\012</BODY>" \
	"</HTML>\015\012"

#define READBUF_LEN		256

/*
 * When true, delay setting the serverReady semaphore until we've finished
 * setting up our SSLContext. This is a workaround for known thread-unsafety
 * related to module attach and detach and context create/destroy:
 *
 *   <rdar://problem/6618834> Crash in KCCursorImpl::next
 *   <rdar://problem/6621552> module/context handles not thread safe
 */
#define SERVER_READY_DELAY      1

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
    OSStatus            ortn = noErr;
    SSLContextRef       ctx = NULL;
	SecKeychainRef		serverKc = nil;
	CFArrayRef			serverCerts = nil;
	RingBuffers			ringBufs = {params->clientToServerRing, params->serverToClientRing};
	
	sslThrDebug("Server", "starting");
    params->negVersion = kSSLProtocolUnknown;
    params->negCipher = SSL_NULL_WITH_NULL_NULL;
	params->ortn = noHardwareErr;
    
	if(params->serverToClientRing == NULL) {
		/* set up a socket on which to listen */
		for(unsigned retry=0; retry<BIND_RETRIES; retry++) {
			ortn = ListenForClients(params->port, params->nonBlocking,
				&listenSock);
			switch(ortn) {
				case noErr:
					break;
				case opWrErr:
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
			if(ortn != opWrErr) {
				break;
			}
		}
	}

    #if     !SERVER_READY_DELAY
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

	if(params->serverToClientRing == NULL) {
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
	}
    #endif  /* SERVER_READY_DELAY */

	/* 
	 * Set up a SecureTransport session.
	 */
	ortn = SSLNewContext(true, &ctx);
	if(ortn) {
		printSslErrStr("SSLNewContext", ortn);
		goto cleanup;
	} 
    
    #if     !SERVER_READY_DELAY
	if(params->serverToClientRing) {
		/* RingBuffer I/O */
		ortn = SSLSetIOFuncs(ctx, ringReadFunc, ringWriteFunc);
		if(ortn) {
			printSslErrStr("SSLSetIOFuncs", ortn);
			goto cleanup;
		} 
		ortn = SSLSetConnection(ctx, (SSLConnectionRef)&ringBufs);
		if(ortn) {
			printSslErrStr("SSLSetConnection", ortn);
			goto cleanup;
		}
	}
	else {
		/* normal socket I/O */		
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
	}
    #endif  /* SERVER_READY_DELAY */
    
	if(params->anchorFile) {
		ortn = sslAddTrustedRoot(ctx, params->anchorFile, 
			params->replaceAnchors);
		if(ortn) {
			goto cleanup;
		}
	}
	if(params->myCertKcName != NULL) {
		/* if not, better be trying anonymous diff-hellman... :-) */
		serverCerts = getSslCerts(params->myCertKcName, CSSM_FALSE, CSSM_FALSE, NULL,
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
	if(!params->noProtSpec) {
		ortn = sslSetProtocols(ctx, params->acceptedProts, params->tryVersion);
		if(ortn) {
			goto cleanup;
		}
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
		ortn = SSLSetDiffieHellmanParams(ctx, params->dhParams, 
			params->dhParamsLen);
		if(ortn) {
			printSslErrStr("SSLSetDiffieHellmanParams", ortn);
			goto cleanup;
		}
	}

    #if     SERVER_READY_DELAY
	/* let main thread know server is fully functional */
	if(pthread_mutex_lock(&params->pthreadMutex)) {
		printf("***Error acquiring server lock; aborting.\n");
		ortn = internalComponentErr;
        goto cleanup;
	}
	params->serverReady = true;
	if(pthread_cond_broadcast(&params->pthreadCond)) {
		printf("***Error waking main thread; aborting.\n");
		ortn = internalComponentErr;
        goto cleanup;
	}
	if(pthread_mutex_unlock(&params->pthreadMutex)) {
		printf("***Error acquiring server lock; aborting.\n");
		ortn = internalComponentErr;
        goto cleanup;
	}
    
	if(params->serverToClientRing == NULL) {
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
	}

    /* Last part of SSLContext setup, now that we're connected to the client */
	if(params->serverToClientRing) {
		/* RingBuffer I/O */
		ortn = SSLSetIOFuncs(ctx, ringReadFunc, ringWriteFunc);
		if(ortn) {
			printSslErrStr("SSLSetIOFuncs", ortn);
			goto cleanup;
		} 
		ortn = SSLSetConnection(ctx, (SSLConnectionRef)&ringBufs);
		if(ortn) {
			printSslErrStr("SSLSetConnection", ortn);
			goto cleanup;
		}
	}
	else {
		/* normal socket I/O */		
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
	}

    #endif  /* SERVER_READY_DELAY */

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
	while(ortn == noErr) {
	    length = READBUF_LEN;
	    ortn = SSLRead(ctx, readBuf, length, &length);
	    if (ortn == errSSLWouldBlock) {
			/* keep trying */
	        ortn = noErr;
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
		if(ortn == noErr) {
			ortn = cerr;
		}
	}
	while(!params->clientDone && !params->serverAbort && (ortn == params->expectRtn)) {
		usleep(100);
	}
	if(acceptSock) {
		endpointShutdown(acceptSock);
	}
	ringBuffersClose(&ringBufs);	/* tolerates NULLs */
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
