/*
 * Copyright (c) 2008-2012 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * Trivial SSL server example, using SecureTransport / OS X version.
 *
 * Written by Doug Mitchell.
 */

#include <Security/SecureTransport.h>
#include <Security/SecureTransportPriv.h>
#include "sslAppUtils.h"
#include "ioSock.h"
#include "fileIo.h"

#include <Security/SecBase.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/param.h>

#include <Security/Security.h>
#include <Security/SecCertificatePriv.h>

#include <CoreFoundation/CoreFoundation.h>
#include "printCert.h"

#if NO_SERVER
#include <securityd/spi.h>
#endif

/* Set true when PR-3074739 is merged to TOT */
#define SET_DH_PARAMS_ENABLE		1

/* true when using SSLCopyPeerCertificates() per Radar 3311892 */
#define USE_COPY_PEER_CERTS		1

/*
 * Defaults, overridable by user.
 */
#define SERVER_MESSAGE  "HTTP/1.0 200 OK\015\012Content-Type: text/html\015\012\015\012" \
	"<HTML><HEAD><TITLE>SecureTransport Test Server</TITLE></HEAD>" \
	"<BODY><H2>Secure connection established.</H2>" \
	"Message from the 'sslServer' sample application.\015\012</BODY>" \
	"</HTML>\015\012"

/* For ease of debugging, pick a non-privileged port */
#define DEFAULT_PORT     1200
// #define DEFAULT_PORT     443

#define DEFAULT_HOST	"localhost"

#define DEFAULT_KC		"certkc"

static void usage(char **argv)
{
    printf("Usage: %s [option ...]\n", argv[0]);
    printf("Options:\n");
	printf("   P=port      Port to listen on; default is %d\n", DEFAULT_PORT);
	printf("   k=keychain  Contains server cert and keys.\n");
	printf("   y=keychain  Encryption-only cert and keys.\n");
    printf("   e           Allow Expired Certs\n");
    printf("   r           Allow any root cert\n");
    printf("   E           Allow Expired Roots\n");
	printf("   x           Disable Cert Verification\n");
	printf("   f=fileBase  Write Peer Certs to fileBase*\n");
	printf("   c           Display peer certs\n");
	printf("   d           Display received data\n");
	printf("   C=cipherSuite (e=40-bit d=DES D=40-bit DES 3=3DES 4=RC4 $=40-bit RC4\n"
		   "                  2=RC2 a=AES128 A=AES256 h=DH H=Anon DH r=DHE/RSA s=DH/DSS\n"
		   "                  n=RSA/NULL\n");
	printf("   2           SSLv2 only (default is best fit)\n");
	printf("   3           SSLv3 only (default is best fit)\n");
	printf("   t           TLSv1 only (default is best fit)\n");
	printf("   o           TLSv1, SSLv3 use kSSLProtocol__X__Only\n");
	printf("   g={prot...} Specify legal protocols; prot = any combo of [23t]\n");
	printf("   T=[nrsj]    Verify client cert state = "
							"none/requested/sent/rejected\n");
	printf("   R           Disable resumable session support\n");
	printf("   i=timeout   Session cache timeout\n");
	printf("   u=[nat]     Authentication: n=never; a=always; t=try\n");
	printf("   b           Non-blocking I/O\n");
	printf("   a fileNmae  Add fileName to list of trusted roots\n");
	printf("   A fileName  fileName is ONLY trusted root\n");
	printf("   U filename  Add filename to acceptable DNList (multiple times OK)\n");
	printf("   D filename  Diffie-Hellman parameters from filename\n");
	printf("   z=password  Unlock server keychain with password.\n");
	printf("   H           Do SecIdentityRef search instead of specific keychain\n");
	printf("   M           Complete cert chain (default assumes that our identity is root)\n");
	printf("   4           Disable anonymous ciphers\n");
	printf("   p           Pause after each phase\n");
	printf("   l[=loops]   Loop, performing multiple transactions\n");
	printf("   q           Quiet/diagnostic mode (site names and errors only)\n");
	printf("   h           Help\n");
    exit(1);
}

/* snag a copy of current connection's peer certs so we can
 * examine them later after the connection is closed */
static OSStatus copyPeerCerts(
	SSLContext 	*ctx,
	CFArrayRef	*peerCerts)		// mallocd & RETURNED
{
	#if USE_COPY_PEER_CERTS
	OSStatus ortn = SSLCopyPeerCertificates(ctx, peerCerts);
	#else
	OSStatus ortn = SSLGetPeerCertificates(ctx, peerCerts);
	#endif
	if(ortn) {
		printf("***Error obtaining peer certs: %s\n",
			sslGetSSLErrString(ortn));
	}
	return ortn;
}

/* free the cert array obtained via SSLGetPeerCertificates() */
static void	freePeerCerts(
	CFArrayRef			peerCerts)
{
	if(peerCerts == NULL) {
		return;
	}

	#if USE_COPY_PEER_CERTS

	/* Voila! Problem fixed. */
	CFRelease(peerCerts);
	return;

	#else

	CFIndex numCerts;
	SecCertificateRef certData;
	CFIndex i;

	numCerts = CFArrayGetCount(peerCerts);
	for(i=0; i<numCerts; i++) {
		certData = (SecCertificateRef)CFArrayGetValueAtIndex(peerCerts, i);
		CFRelease(certData);
	}
	CFRelease(peerCerts);
	#endif
}

/* print reply received from server */
static void dumpAscii(
	uint8_t *rcvBuf,
	uint32_t len)
{
	char *cp = (char *)rcvBuf;
	uint32_t i;
	char c;

	for(i=0; i<len; i++) {
		c = *cp++;
		if(c == '\0') {
			break;
		}
		switch(c) {
			case '\n':
				printf("\\n");
				break;
			case '\r':
				printf("\\r");
				break;
			default:
				if(isprint(c) && (c != '\n')) {
					printf("%c", c);
				}
				else {
					printf("<%02X>", ((unsigned)c) & 0xff);
				}
			break;
		}

	}
	printf("\n");
}

static void doPause(const char *prompt) {
	if(prompt) {
		printf("%s. ", prompt);
	}
	fpurge(stdin);
	printf("Continue (n/anything)? ");
	char c = getchar();
	if(c == 'n') {
		exit(0);
	}
}

/*
 * Perform one SSL diagnostic server-side session. Returns nonzero on error.
 * Normally no output to stdout except initial "waiting for connection" message,
 * unless there is a really screwed up error (i.e., something not directly related
 * to the SSL connection).
 */
#define RCV_BUF_SIZE		256

static OSStatus sslServe(
	otSocket				listenSock,
	unsigned short			portNum,
	SSLProtocol				tryVersion,			// only used if acceptedProts NULL
	const char				*acceptedProts,
	CFArrayRef				serverCerts,		// required
	char					*password,			// optional
	CFArrayRef				encryptServerCerts,	// optional
	bool				allowExpired,
	bool				allowAnyRoot,
	bool				allowExpiredRoot,
	bool				disableCertVerify,
	char					*anchorFile,
	bool				replaceAnchors,
	char					cipherRestrict,		// '2', 'd'. etc...'\0' for no
												//   restriction
	SSLAuthenticate			authenticate,
	unsigned char			*dhParams,			// optional D-H parameters
	unsigned				dhParamsLen,
	CFArrayRef				acceptableDNList,	// optional
	bool				resumableEnable,
	uint32_t					sessionCacheTimeout,// optional
	bool				disableAnonCiphers,
	bool				silent,				// no stdout
	bool				pause,
	SSLProtocol				*negVersion,		// RETURNED
	SSLCipherSuite			*negCipher,			// RETURNED
	SSLClientCertificateState *certState,		// RETURNED
	Boolean					*sessionWasResumed,	// RETURNED
	unsigned char			*sessionID,			// mallocd by caller, RETURNED
	size_t					*sessionIDLength,	// RETURNED
	CFArrayRef				*peerCerts,			// mallocd & RETURNED
	char					**argv)
{
	otSocket			acceptSock;
    PeerSpec            peerId;
    OSStatus            ortn;
    SSLContextRef       ctx = NULL;
    size_t              length;
    uint8_t               rcvBuf[RCV_BUF_SIZE];
	const char *outMsg = SERVER_MESSAGE;

    *negVersion = kSSLProtocolUnknown;
    *negCipher = SSL_NULL_WITH_NULL_NULL;
    *peerCerts = NULL;

	#if IGNORE_SIGPIPE
	signal(SIGPIPE, sigpipe);
	#endif

	/* first wait for a connection */
	if(!silent) {
		printf("Waiting for client connection on port %u...", portNum);
		fflush(stdout);
	}
	ortn = AcceptClientConnection(listenSock, &acceptSock, &peerId);
    if(ortn) {
    	printf("AcceptClientConnection returned %d; aborting\n", (int)ortn);
    	return ortn;
    }

	/*
	 * Set up a SecureTransport session.
	 * First the standard calls.
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

	/* have to do these options befor setting server certs */
	if(allowExpired) {
		ortn = SSLSetAllowsExpiredCerts(ctx, true);
		if(ortn) {
			printSslErrStr("SSLSetAllowExpiredCerts", ortn);
			goto cleanup;
		}
	}
	if(allowAnyRoot) {
		ortn = SSLSetAllowsAnyRoot(ctx, true);
		if(ortn) {
			printSslErrStr("SSLSetAllowAnyRoot", ortn);
			goto cleanup;
		}
	}

	if(anchorFile) {
		ortn = sslAddTrustedRoot(ctx, anchorFile, replaceAnchors);
		if(ortn) {
			printf("***Error obtaining anchor file %s\n", anchorFile);
			goto cleanup;
		}
	}
	if(serverCerts != NULL) {
		if(anchorFile == NULL) {
			/* no specific anchors, so assume we want to trust this one */
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
	if(encryptServerCerts) {
		ortn = SSLSetEncryptionCertificate(ctx, encryptServerCerts);
		if(ortn) {
			printSslErrStr("SSLSetEncryptionCertificate", ortn);
			goto cleanup;
		}
	}
	if(allowExpiredRoot) {
		ortn = SSLSetAllowsExpiredRoots(ctx, true);
		if(ortn) {
			printSslErrStr("SSLSetAllowsExpiredRoots", ortn);
			goto cleanup;
		}
	}
	if(disableCertVerify) {
		ortn = SSLSetEnableCertVerify(ctx, false);
		if(ortn) {
			printSslErrStr("SSLSetEnableCertVerify", ortn);
			goto cleanup;
		}
	}

	/*
	 * SecureTransport options.
	 */
	if(acceptedProts) {
		ortn = SSLSetProtocolVersionEnabled(ctx, kSSLProtocolAll, false);
		if(ortn) {
			printSslErrStr("SSLSetProtocolVersionEnabled(all off)", ortn);
			goto cleanup;
		}
		for(const char *cp = acceptedProts; *cp; cp++) {
			SSLProtocol prot = kSSLProtocolUnknown;
			switch(*cp) {
				case '2':
					prot = kSSLProtocol2;
					break;
				case '3':
					prot = kSSLProtocol3;
					break;
				case 't':
					prot = kTLSProtocol1;
					break;
				default:
					usage(argv);
			}
			ortn = SSLSetProtocolVersionEnabled(ctx, prot, true);
			if(ortn) {
				printSslErrStr("SSLSetProtocolVersionEnabled", ortn);
				goto cleanup;
			}
		}
	}
	else {
		ortn = SSLSetProtocolVersion(ctx, tryVersion);
		if(ortn) {
			printSslErrStr("SSLSetProtocolVersion", ortn);
			goto cleanup;
		}
	}
	if(resumableEnable) {
		ortn = SSLSetPeerID(ctx, &peerId, sizeof(PeerSpec));
		if(ortn) {
			printSslErrStr("SSLSetPeerID", ortn);
			goto cleanup;
		}
	}
	if(cipherRestrict != '\0') {
		ortn = sslSetCipherRestrictions(ctx, cipherRestrict);
		if(ortn) {
			goto cleanup;
		}
	}
	if(authenticate != kNeverAuthenticate) {
		ortn = SSLSetClientSideAuthenticate(ctx, authenticate);
		if(ortn) {
			printSslErrStr("SSLSetClientSideAuthenticate", ortn);
			goto cleanup;
		}
	}
	if(dhParams) {
		ortn = SSLSetDiffieHellmanParams(ctx, dhParams, dhParamsLen);
		if(ortn) {
			printSslErrStr("SSLSetDiffieHellmanParams", ortn);
			goto cleanup;
		}
	}
	if(sessionCacheTimeout) {
		ortn = SSLSetSessionCacheTimeout(ctx, sessionCacheTimeout);
		if(ortn) {
			printSslErrStr("SSLSetSessionCacheTimeout", ortn);
			goto cleanup;
		}
	}
	if(disableAnonCiphers) {
		ortn = SSLSetAllowAnonymousCiphers(ctx, false);
		if(ortn) {
			printSslErrStr("SSLSetAllowAnonymousCiphers", ortn);
			goto cleanup;
		}
		/* quickie test of the getter */
		Boolean e;
		ortn = SSLGetAllowAnonymousCiphers(ctx, &e);
		if(ortn) {
			printSslErrStr("SSLGetAllowAnonymousCiphers", ortn);
			goto cleanup;
		}
		if(e) {
			printf("***SSLGetAllowAnonymousCiphers() returned true; expected false\n");
			ortn = errSecIO;
			goto cleanup;
		}
	}
/* XXX/cs
	if(acceptableDNList) {
		ortn = SSLSetCertificateAuthorities(ctx, acceptableDNList, TRUE);
		if(ortn) {
			printSslErrStr("SSLSetCertificateAuthorities", ortn);
			goto cleanup;
		}
	}
*/
	/* end options */

	if(pause) {
		doPause("SSLContext initialized");
	}

	/* Perform SSL/TLS handshake */
    do
    {   ortn = SSLHandshake(ctx);
		if((ortn == errSSLWouldBlock) && !silent) {
			/* keep UI responsive */
			sslOutputDot();
		}
    } while (ortn == errSSLWouldBlock);

	/* this works even if handshake failed due to cert chain invalid */
	copyPeerCerts(ctx, peerCerts);

	SSLGetClientCertificateState(ctx, certState);
	SSLGetNegotiatedCipher(ctx, negCipher);
	SSLGetNegotiatedProtocolVersion(ctx, negVersion);
	*sessionIDLength = MAX_SESSION_ID_LENGTH;
	SSLGetResumableSessionInfo(ctx, sessionWasResumed, sessionID,
		sessionIDLength);

	if(!silent) {
		printf("\n");
	}
    if(ortn) {
    	goto cleanup;
    }
	if(pause) {
		doPause("SSLContext handshake complete");
	}

	/* wait for one complete line or user says they've had enough */
	while(ortn == errSecSuccess) {
		length = sizeof(rcvBuf);
		ortn = SSLRead(ctx, rcvBuf, length, &length);
		if(length == 0) {
			/* keep UI responsive */
			sslOutputDot();
		}
		else {
			/* print what we have */
			printf("client request: ");
			dumpAscii(rcvBuf, length);
		}
		if(pause) {
			/* allow user to bail */
			char resp;

			fpurge(stdin);
			printf("\nMore client request (y/anything): ");
			resp = getchar();
			if(resp != 'y') {
				break;
			}
		}

		/* poor person's line completion scan */
		for(unsigned i=0; i<length; i++) {
			if((rcvBuf[i] == '\n') || (rcvBuf[i] == '\r')) {
				/* a labelled break would be nice here.... */
				goto serverResp;
			}
		}
		if (ortn == errSSLWouldBlock) {
			ortn = errSecSuccess;
		}
	}

serverResp:
	if(pause) {
		doPause("Client GET msg received");
	}

	/* send out canned response */
	length = strlen(outMsg);
	ortn = SSLWrite(ctx, outMsg, length, &length);
	if(ortn) {
		printSslErrStr("SSLWrite", ortn);
	}
	if(pause) {
		doPause("Server response sent");
	}
cleanup:
	/*
	 * always do close, even on error - to flush outgoing write queue
	 */
	OSStatus cerr = SSLClose(ctx);
	if(ortn == errSecSuccess) {
		ortn = cerr;
	}
	if(acceptSock) {
		endpointShutdown(acceptSock);
	}
	if(ctx) {
	    SSLDisposeContext(ctx);
	}
	/* FIXME - dispose of serverCerts */
	return ortn;
}

static void showPeerCerts(
	CFArrayRef			peerCerts,
	bool			verbose)
{
	CFIndex numCerts;
	SecCertificateRef certRef;
	CFIndex i;

	if(peerCerts == NULL) {
		return;
	}
	numCerts = CFArrayGetCount(peerCerts);
	for(i=0; i<numCerts; i++) {
		certRef = (SecCertificateRef)CFArrayGetValueAtIndex(peerCerts, i);
		printf("\n================== Server Cert %lu ===================\n\n", i);
		print_cert(certRef, verbose);
		printf("\n=============== End of Server Cert %lu ===============\n", i);
	}
}

static void writePeerCerts(
	CFArrayRef			peerCerts,
	const char			*fileBase)
{
	CFIndex numCerts;
	SecCertificateRef certRef;
	CFIndex i;
	char fileName[100];

	if(peerCerts == NULL) {
		return;
	}
	numCerts = CFArrayGetCount(peerCerts);
	for(i=0; i<numCerts; i++) {
		sprintf(fileName, "%s%02d.cer", fileBase, (int)i);
		certRef = (SecCertificateRef)CFArrayGetValueAtIndex(peerCerts, i);
		writeFile(fileName, SecCertificateGetBytePtr(certRef),
			SecCertificateGetLength(certRef));
	}
	printf("...wrote %lu certs to fileBase %s\n", numCerts, fileBase);
}

static void showSSLResult(
	SSLProtocol			tryVersion,
	char				*acceptedProts,
	OSStatus			err,
	SSLProtocol			negVersion,
	SSLCipherSuite		negCipher,
	Boolean				sessionWasResumed,
	unsigned char		*sessionID,
	size_t				sessionIDLength,
	CFArrayRef			peerCerts,
	bool			displayPeerCerts,
	SSLClientCertificateState	certState,
	char				*fileBase)		// non-NULL: write certs to file
{
	CFIndex numPeerCerts;

	printf("\n");
	if(acceptedProts) {
		printf("   Allowed SSL versions   : %s\n", acceptedProts);
	}
	else {
		printf("   Attempted  SSL version : %s\n",
			sslGetProtocolVersionString(tryVersion));
	}
	printf("   Result                 : %s\n", sslGetSSLErrString(err));
	printf("   Negotiated SSL version : %s\n",
		sslGetProtocolVersionString(negVersion));
	printf("   Negotiated CipherSuite : %s\n",
		sslGetCipherSuiteString(negCipher));
	if(certState != kSSLClientCertNone) {
		printf("   Client Cert State      : %s\n",
			sslGetClientCertStateString(certState));
	}
	printf("   Resumed Session        : ");
	if(sessionWasResumed) {
		for(unsigned dex=0; dex<sessionIDLength; dex++) {
			printf("%02X ", sessionID[dex]);
			if(((dex % 8) == 7) && (dex != (sessionIDLength - 1))) {
				printf("\n                            ");
			}
		}
		printf("\n");
	}
	else {
		printf("NOT RESUMED\n");
	}
	if(peerCerts == NULL) {
		numPeerCerts = 0;
	}
	else {
		numPeerCerts = CFArrayGetCount(peerCerts);
	}
	printf("   Number of peer certs : %lu\n", numPeerCerts);
	if(numPeerCerts != 0) {
		if(displayPeerCerts) {
			showPeerCerts(peerCerts, false);
		}
		if(fileBase != NULL) {
			writePeerCerts(peerCerts, fileBase);
		}
	}
	printf("\n");
}

static int verifyClientCertState(
	bool					verifyCertState,
	SSLClientCertificateState	expectState,
	SSLClientCertificateState	gotState)
{
	if(!verifyCertState) {
		return 0;
	}
	if(expectState == gotState) {
		return 0;
	}
	printf("***Expected clientCertState %s; got %s\n",
		sslGetClientCertStateString(expectState),
		sslGetClientCertStateString(gotState));
	return 1;
}

int main(int argc, char **argv)
{
	OSStatus            err;
	int					arg;
	char				fullFileBase[100];
	SSLProtocol			negVersion;
	SSLCipherSuite		negCipher;
	Boolean				sessionWasResumed;
	unsigned char		sessionID[MAX_SESSION_ID_LENGTH];
	size_t				sessionIDLength;
	CFArrayRef			peerCerts = NULL;
	char				*argp;
	otSocket			listenSock;
	CFArrayRef			serverCerts = nil;		// required
	CFArrayRef			encryptCerts = nil;		// optional
	SecKeychainRef		serverKc = nil;
	SecKeychainRef		encryptKc = nil;
	int 				loopNum;
	int					errCount = 0;
	SSLClientCertificateState certState;		// obtained from sslServe

	/* user-spec'd parameters */
	unsigned short		portNum = DEFAULT_PORT;
	bool			allowExpired = false;
	bool			allowAnyRoot = false;
	char				*fileBase = NULL;
	bool			displayRxData = false;
	bool			displayCerts = false;
	char				cipherRestrict = '\0';
	SSLProtocol			attemptProt = kTLSProtocol1;
	bool			protXOnly = false;	// kSSLProtocol3Only,
												//    kTLSProtocol1Only
	char				*acceptedProts = NULL;	// "23t" ==> SSLSetProtocolVersionEnabled
	bool			quiet = false;
	bool			resumableEnable = true;
	bool			pause = false;
	char				*keyChainName = NULL;
	char				*encryptKeyChainName = NULL;
	int					loops = 1;
	SSLAuthenticate		authenticate = kNeverAuthenticate;
	bool			nonBlocking = false;
	bool			allowExpiredRoot = false;
	bool			disableCertVerify = false;
	char				*anchorFile = NULL;
	bool			replaceAnchors = false;
	bool			vfyCertState = false;
	SSLClientCertificateState expectCertState = kSSLClientCertNone;
	char				*password = NULL;
	char				*dhParamsFile = NULL;
	unsigned char		*dhParams = NULL;
	unsigned			dhParamsLen = 0;
	bool			doIdSearch = false;
	bool			completeCertChain = false;
	uint32_t				sessionCacheTimeout = 0;
	bool			disableAnonCiphers = false;
	CFMutableArrayRef	acceptableDNList = NULL;

	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'P':
				portNum = atoi(&argp[2]);
				break;
			case 'k':
				keyChainName = &argp[2];
				break;
			case 'y':
				encryptKeyChainName = &argp[2];
				break;
			case 'e':
				allowExpired = true;
				break;
			case 'E':
				allowExpiredRoot = true;
				break;
			case 'x':
				disableCertVerify = true;
				break;
			case 'a':
				if(++arg == argc)  {
					/* requires another arg */
					usage(argv);
				}
				anchorFile = argv[arg];
				break;
			case 'A':
				if(++arg == argc)  {
					/* requires another arg */
					usage(argv);
				}
				anchorFile = argv[arg];
				replaceAnchors = true;
				break;
			case 'T':
				if(argp[1] != '=') {
					usage(argv);
				}
				vfyCertState = true;
				switch(argp[2]) {
					case 'n':
						expectCertState = kSSLClientCertNone;
						break;
					case 'r':
						expectCertState = kSSLClientCertRequested;
						break;
					case 's':
						expectCertState = kSSLClientCertSent;
						break;
					case 'j':
						expectCertState = kSSLClientCertRejected;
						break;
					default:
						usage(argv);
				}
				break;
			case 'r':
				allowAnyRoot = true;
				break;
			case 'd':
				displayRxData = true;
				break;
			case 'c':
				displayCerts = true;
				break;
			case 'f':
				fileBase = &argp[2];
				break;
			case 'C':
				cipherRestrict = argp[2];
				break;
			case '2':
				attemptProt = kSSLProtocol2;
				break;
			case '3':
				attemptProt = kSSLProtocol3;
				break;
			case 't':
				attemptProt = kTLSProtocol1;
				break;
			case 'o':
				protXOnly = true;
				break;
			case 'g':
				if(argp[1] != '=') {
					usage(argv);
				}
				acceptedProts = &argp[2];
				break;
			case 'R':
				resumableEnable = false;
				break;
			case 'b':
				nonBlocking = true;
				break;
			case 'u':
				if(argp[1] != '=') {
					usage(argv);
				}
				switch(argp[2]) {
					case 'a': authenticate = kAlwaysAuthenticate; break;
					case 'n': authenticate = kNeverAuthenticate; break;
					case 't': authenticate = kTryAuthenticate; break;
					default: usage(argv);
				}
				break;
			case 'D':
				if(++arg == argc)  {
					/* requires another arg */
					usage(argv);
				}
				dhParamsFile = argv[arg];
				break;
			case 'z':
				password = &argp[2];
				break;
			case 'H':
				doIdSearch = true;
				break;
			case 'M':
				completeCertChain = true;
				break;
			case 'i':
				sessionCacheTimeout = atoi(&argp[2]);
				break;
			case '4':
				disableAnonCiphers = true;
				break;
			case 'p':
				pause = true;
				break;
			case 'q':
				quiet = true;
				break;
#if 0
			case 'U':
				if(++arg == argc)  {
					/* requires another arg */
					usage(argv);
				}
				if(cspReadFile(argv[arg], &caCert, &caCertLen)) {
					printf("***Error reading file %s. Aborting.\n", argv[arg]);
					exit(1);
				}
				if(acceptableDNList == NULL) {
					acceptableDNList = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
				}
				certData.Data = caCert;
				certData.Length = caCertLen;
				ortn = SecCertificateCreateFromData(&certData,
													CSSM_CERT_X_509v3,
													CSSM_CERT_ENCODING_DER,
													&secCert);
				if(ortn) {
					cssmPerror("SecCertificateCreateFromData", ortn);
					exit(1);
				}
				CFArrayAppendValue(acceptableDNList, secCert);
				CFRelease(secCert);
				break;
#endif
			case 'l':
				if(argp[1] == '\0') {
					/* no loop count --> loop forever */
					loops = 0;
					break;
				}
				else if(argp[1] != '=') {
					usage(argv);
				}
				loops = atoi(&argp[2]);
				break;
			default:
				usage(argv);
		}
	}

#if NO_SERVER
# if DEBUG
    securityd_init();
# endif
#endif

	/* get server cert and optional encryption cert as CFArrayRef */
	if(keyChainName) {
		serverCerts = getSslCerts(keyChainName, false, completeCertChain,
			anchorFile, &serverKc);
		if(serverCerts == nil) {
			exit(1);
		}
	}
	else
#if 0
    if(doIdSearch) {
		OSStatus ortn = sslIdentityPicker(NULL, anchorFile, true, NULL, &serverCerts);
		if(ortn) {
			printf("***IdentitySearch failure; aborting.\n");
			exit(1);
		}
	}
	if(password) {
		OSStatus ortn = SecKeychainUnlock(serverKc, strlen(password), password, true);
		if(ortn) {
			printf("SecKeychainUnlock returned %d\n", (int)ortn);
			/* oh well */
		}
	}
	if(encryptKeyChainName) {
		encryptCerts = getSslCerts(encryptKeyChainName, true, completeCertChain,
			anchorFile, &encryptKc);
		if(encryptCerts == nil) {
			exit(1);
		}
	}
#endif
	if(protXOnly) {
		switch(attemptProt) {
			case kTLSProtocol1:
				attemptProt = kTLSProtocol1Only;
				break;
			case kSSLProtocol3:
				attemptProt = kSSLProtocol3Only;
				break;
			default:
				break;
		}
	}
#if 0
	if(dhParamsFile) {
		int r = cspReadFile(dhParamsFile, &dhParams, &dhParamsLen);
		if(r) {
			printf("***Error reading diffie-hellman params from %s; aborting\n",
				dhParamsFile);
		}
	}
#endif

	/* one-time only server port setup */
	err = ListenForClients(portNum, nonBlocking, &listenSock);
	if(err) {
    	printf("ListenForClients returned %d; aborting\n", (int)err);
		exit(1);
	}

	for(loopNum=1; ; loopNum++) {
		err = sslServe(listenSock,
			portNum,
			attemptProt,
			acceptedProts,
			serverCerts,
			password,
			encryptCerts,
			allowExpired,
			allowAnyRoot,
			allowExpiredRoot,
			disableCertVerify,
			anchorFile,
			replaceAnchors,
			cipherRestrict,
			authenticate,
			dhParams,
			dhParamsLen,
			acceptableDNList,
			resumableEnable,
			sessionCacheTimeout,
			disableAnonCiphers,
			quiet,
			pause,
			&negVersion,
			&negCipher,
			&certState,
			&sessionWasResumed,
			sessionID,
			&sessionIDLength,
			&peerCerts,
			argv);
		if(err) {
			errCount++;
		}
		if(!quiet) {
			SSLProtocol tryProt = attemptProt;
			showSSLResult(tryProt,
				acceptedProts,
				err,
				negVersion,
				negCipher,
				sessionWasResumed,
				sessionID,
				sessionIDLength,
				peerCerts,
				displayCerts,
				certState,
				fileBase ? fullFileBase : NULL);
		}
		errCount += verifyClientCertState(vfyCertState, expectCertState,
			certState);
		freePeerCerts(peerCerts);
		if(loops && (loopNum == loops)) {
			break;
		}
	};

	endpointShutdown(listenSock);

	if(serverKc) {
		CFRelease(serverKc);
	}
	if(encryptKc) {
		CFRelease(encryptKc);
	}
    return errCount;

}


