/*
 * sslClient.cpp : perform one SSL client side sesssion 
 */
#include <Security/SecureTransport.h>
#include <Security/Security.h>
#include <clAppUtils/sslAppUtils.h>
#include <clAppUtils/ioSock.h>
#include <clAppUtils/sslThreading.h>
#include <clAppUtils/ringBufferIo.h>
#include <utilLib/common.h>
#include <security_cdsa_utils/cuPrintCert.h>

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/param.h>

/* when true, keep listening until server disconnects */
#define KEEP_CONNECTED	1

#define CLIENT_GETMSG  	"GET / HTTP/1.0\r\n\r\n"

#define READBUF_LEN		256

/* relies on SSLSetProtocolVersionEnabled */
OSStatus sslAppClient(
	SslAppTestParams	*params)
{
    PeerSpec            peerId;
	otSocket			sock = 0;
    OSStatus            ortn;
    SSLContextRef       ctx = NULL;
	SecKeychainRef		clientKc = nil;
	CFArrayRef			clientCerts = nil;
	RingBuffers			ringBufs = {params->serverToClientRing, params->clientToServerRing};
	
	sslThrDebug("Client", "starting");
    params->negVersion = kSSLProtocolUnknown;
    params->negCipher  = SSL_NULL_WITH_NULL_NULL;
    params->ortn       = noHardwareErr;
	
	if(params->serverToClientRing == NULL) {
		/* first make sure requested server is there */
		ortn = MakeServerConnection(params->hostName, params->port,
			params->nonBlocking, &sock, &peerId);
		if(ortn) {
			printf("MakeServerConnection returned %d; aborting\n", (int)ortn);
			return ortn;
		}
	}

	/* 
	 * Set up a SecureTransport session.
	 */
	ortn = SSLNewContext(false, &ctx);
	if(ortn) {
		printSslErrStr("SSLNewContext", ortn);
		goto cleanup;
	} 
	if(params->serverToClientRing) {
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
		ortn = SSLSetIOFuncs(ctx, SocketRead, SocketWrite);
		if(ortn) {
			printSslErrStr("SSLSetIOFuncs", ortn);
			goto cleanup;
		} 
		ortn = SSLSetConnection(ctx, (SSLConnectionRef)sock);
		if(ortn) {
			printSslErrStr("SSLSetConnection", ortn);
			goto cleanup;
		}
	}
	if(!params->skipHostNameCheck) {
		ortn = SSLSetPeerDomainName(ctx, params->hostName, 
			strlen(params->hostName) + 1);
		if(ortn) {
			printSslErrStr("SSLSetPeerDomainName", ortn);
			goto cleanup;
		}
	}
	
	/* remainder of setup is optional */
	if(params->anchorFile) {
		ortn = sslAddTrustedRoot(ctx, params->anchorFile, params->replaceAnchors);
		if(ortn) {
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
	if(params->disableCertVerify) {
		ortn = SSLSetEnableCertVerify(ctx, false);
		if(ortn) {
			printSslErrStr("SSLSetEnableCertVerify", ortn);
			goto cleanup;
		}
	}
	if(params->ciphers != NULL) {
		ortn = sslSetEnabledCiphers(ctx, params->ciphers);
		if(ortn) {
			goto cleanup;
		}
	}
	if(params->myCertKcName) {
		clientCerts = getSslCerts(params->myCertKcName, CSSM_FALSE, CSSM_FALSE, NULL, &clientKc);
		if(clientCerts == nil) {
			exit(1);
		}
		if(params->password) {
			ortn = SecKeychainUnlock(clientKc, strlen(params->password), 
					(void *)params->password, true);
			if(ortn) {
				printf("SecKeychainUnlock returned %d\n", (int)ortn);
				/* oh well */
			}
		}
		if(params->idIsTrustedRoot) {
			/* assume this is a root we want to implicitly trust */
			ortn = addIdentityAsTrustedRoot(ctx, clientCerts);
			if(ortn) {
				goto cleanup;
			}
		}
		ortn = SSLSetCertificate(ctx, clientCerts);
		if(ortn) {
			printSslErrStr("SSLSetCertificate", ortn);
			goto cleanup;
		}
	}
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
	
	if(ortn != noErr) {
		goto cleanup;
	}

	/* send a GET msg */
	size_t actLen;
	ortn = SSLWrite(ctx, CLIENT_GETMSG, strlen(CLIENT_GETMSG), &actLen);
	if(ortn) {
		printSslErrStr("SSLWrite", ortn);
		goto cleanup;
	}
	
	#if KEEP_CONNECTED
	
	/*
	 * Consume any server data and wait for server to disconnect
	 */
	char readBuf[READBUF_LEN];
    do {
		ortn = SSLRead(ctx, readBuf, READBUF_LEN, &actLen);
    } while (ortn == errSSLWouldBlock);
	
    /* convert normal "shutdown" into zero err rtn */
	if(ortn == errSSLClosedGraceful) {
		ortn = noErr;
	}
	#endif	/* KEEP_CONNECTED */
	
cleanup:
	if(ctx) {
		OSStatus cerr = SSLClose(ctx);
		if(ortn == noErr) {
			ortn = cerr;
		}
	}
	if(sock) {
		endpointShutdown(sock);
	}
	ringBuffersClose(&ringBufs);	/* tolerates NULLs */
	if(ctx) {
	    SSLDisposeContext(ctx);  
	}    
	params->ortn = ortn;
	sslThrDebug("Client", "done");
	return ortn;
}
