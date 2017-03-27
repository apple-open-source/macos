/*
 * Copyright (c) 2006-2013, 2015 Apple Inc. All Rights Reserved.
 *
 * SSL viewer tool
 */

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <Security/SecureTransport.h>
#include <Security/SecureTransportPriv.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecPolicyPriv.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>

#include "sslAppUtils.h"
#include "ioSock.h"
#include "utilities/fileIo.h"
#include "utilities/SecCFWrappers.h"
#include "utilities/SecIOFormat.h"
#include "SecurityTool/print_cert.h"

#define DEFAULT_GETMSG  	"GET"
#define DEFAULT_PATH		"/"
#define DEFAULT_GET_SUFFIX	"HTTP/1.0\r\n\r\n"

#define DEFAULT_HOST   	  	"www.amazon.com"
#define DEFAULT_PORT     	443

static const int _maxFileStringSize = 100;

static void usageNorm(char **argv)
{
    printf("Usage: %s [hostname|-] [path] [option ...]\n", argv[0]);
    printf("       %s hostname [path] [option ...]\n", argv[0]);
	printf("Specifying '-' for hostname, or no args, uses default of %s.\n",
		DEFAULT_HOST);
	printf("Optional path argument must start with leading '/'.\n");
    printf("Options:\n");
    printf("   e           Allow Expired Certs\n");
    printf("   E           Allow Expired Roots\n");
    printf("   r           Allow any root cert\n");
	printf("   c           Display peer certs\n");
	printf("   c c         Display peer SecTrust\n");
	printf("   d           Display received data\n");
	printf("   2           SSLv2 only (default is TLSv1)\n");
	printf("   3           SSLv3 only (default is TLSv1)\n");
	printf("   t           TLSv1\n");
    printf("   %%           TLSv1.1 only\n");
    printf("   ^           TLSv1.2 only\n");
	printf("   L           all - TLSv1.2, TLSv1.1, TLSv1.0, SSLv3, SSLv2 (default = TLSv1.2)\n");
	printf("   g={prot...} Specify legal protocols; prot = any combo of"
							" [23t]\n");
	printf("   k=keychain  Contains cert and keys. Optional.\n");
	printf("   l=loopCount Perform loopCount ops (default = 1)\n");
	printf("   P=port      Default = %d\n", DEFAULT_PORT);
	printf("   p           Pause after each loop\n");
	printf("   q           Quiet/diagnostic mode (site names and errors only)\n");
    printf("   a fileName  Add fileName to list of trusted roots\n");
	printf("   A fileName  fileName is ONLY trusted root\n");
	printf("   x           Disable Cert Verification\n");
    printf("   Z string    ALPN setting\n");
	printf("   z=password  Unlock client keychain with password.\n");
	printf("   8           Complete cert chains (default is out cert is a root)\n");
	printf("   s           Silent\n");
	printf("   V           Verbose\n");
	printf("   h           Help\n");
	printf("   hv          More, verbose help\n");
}

static void usageVerbose(char **argv) __attribute__((noreturn));
static void usageVerbose(char **argv)
{
    usageNorm(argv);
	printf("Obscure Usage:\n");
	printf("   u           kSSLProtocolUnknown only (TLSv1)\n");
	printf("   M           Manual cert verification via "
							"SecTrustEvaluate\n");
	printf("   f fileBase  Write Peer Certs to fileBase*\n");
	printf("   o           TLSv1, SSLv3 use kSSLProtocol__X__Only\n");
	printf("   C=cipherSuite (e=40-bit d=DES D=40-bit DES 3=3DES 4=RC4 "
								"$=40-bit RC4\n"
		   "                  2=RC2 a=AES128 A=AES256 h=DH H=Anon DH r=DHE/RSA s=DH/DSS\n");
	printf("   y=keychain  Encryption-only cert and keys. Optional.\n");
	printf("   K           Keep connected until server disconnects\n");
	printf("   n           Require closure notify message in TLSv1, "
								"SSLv3 mode (implies K)\n");
	printf("   R           Disable resumable session support\n");
	printf("   b           Non-blocking I/O\n");
	printf("   v           Verify negotiated protocol equals attempted\n");
	printf("   m=[23t]     Max protocol supported as specified; implies "
								"v\n");
	printf("   T=[nrsj]    Verify client cert state = "
								"none/requested/sent/rejected\n");
	printf("   H           allow hostname spoofing\n");
	printf("   F=vfyHost   Verify certs with specified host name\n");
	printf("   G=getMsg    Specify entire GET, POST, etc.\n");
	printf("   N           Log handshake timing\n");
	printf("   7           Pause only after first loop\n");
	exit(1);
}

static void usage(char **argv) __attribute__((noreturn));
static void usage(char **argv)
{
    usageNorm(argv);
	exit(1);
}

/*
 * Arguments to top-level sslPing()
 */
typedef struct {
	SSLProtocol				tryVersion;			// only used if acceptedProts NULL
												// uses SSLSetProtocolVersion
	char					*acceptedProts;		// optional, any combo of {2,3,t}
												// uses SSLSetProtocolVersionEnabled
	const char				*hostName;			// e.g., "www.amazon.com"
	const char				*vfyHostName;		// use this for cert vfy if non-NULL,
												//   else use hostName
	unsigned short			port;
	const char				*getMsg;			// e.g.,
												//   "GET / HTTP/1.0\r\n\r\n"
	bool				allowExpired;
	bool				allowAnyRoot;
	bool				allowExpiredRoot;
	bool				disableCertVerify;
	bool				manualCertVerify;
	bool				dumpRxData;			// display server data
	char					cipherRestrict;		// '2', 'd'. etc...; '\0' for
												//   no restriction
	bool				keepConnected;
	bool				requireNotify;		// require closure notify
												//   in V3 mode
	bool				resumableEnable;
	bool				allowHostnameSpoof;
	bool				nonBlocking;
	char					*anchorFile;
	bool				replaceAnchors;
	CFArrayRef				clientCerts;		// optional
	bool				quiet;				// minimal stdout
	bool				silent;				// no stdout
	bool				verbose;
	SSLProtocol				negVersion;			// RETURNED
	SSLCipherSuite			negCipher;			// RETURNED
	CFArrayRef				peerCerts;			// mallocd & RETURNED
	SecTrustRef				peerTrust;			// RETURNED
	SSLClientCertificateState certState;		// RETURNED
	char					*password;			// optional to open clientCerts
	char					**argv;
	Boolean					sessionWasResumed;
	unsigned char			sessionID[MAX_SESSION_ID_LENGTH];
	size_t					sessionIDLength;
	CFAbsoluteTime			handshakeTimeOp;		// time for this op
	CFAbsoluteTime			handshakeTimeFirst;		// time for FIRST op, not averaged
	CFAbsoluteTime			handshakeTimeTotal;		// time for all ops except first
	unsigned				numHandshakes;

    CFMutableArrayRef       alpnNames;
    CFMutableArrayRef       policies;

} sslPingArgs;

static void
sigpipe(int sig)
{
	fflush(stdin);
	printf("***SIGPIPE***\n");
}

/*
 * Manually evaluate session's SecTrustRef.
 */

static OSStatus sslEvaluateTrust(
	SSLContextRef ctx,
    sslPingArgs *pargs,
	CFArrayRef *peerCerts)		// fetched and retained
{
	OSStatus ortn = errSecSuccess;
	SecTrustRef secTrust = NULL;

	ortn = SSLGetPeerSecTrust(ctx, &secTrust);
	if(ortn) {
		printf("\n***Error obtaining peer SecTrustRef: %s\n",
			sslGetSSLErrString(ortn));
		return ortn;
	}
	if(secTrust == NULL) {
		/* this is the normal case for resumed sessions, in which
		 * no cert evaluation is performed */
		if(!pargs->silent) {
			printf("...No SecTrust available - this is a resumed session, right?\n");
		}
		return errSecSuccess;
	}


    if (pargs->policies) {
        SecTrustSetPolicies(secTrust, pargs->policies);
    }

	SecTrustResultType	secTrustResult;
	ortn = SecTrustEvaluate(secTrust, &secTrustResult);
	if(ortn) {
		printf("\n***Error on SecTrustEvaluate: %d\n", (int)ortn);
		return ortn;
	}
	if(pargs->verbose) {
		const char *res = NULL;
		switch(secTrustResult) {
			case kSecTrustResultInvalid:
				res = "kSecTrustResultInvalid"; break;
			case kSecTrustResultProceed:
				res = "kSecTrustResultProceed"; break;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            case kSecTrustResultConfirm:
#pragma clang diagnostic pop
				res = "kSecTrustResultConfirm"; break;
			case kSecTrustResultDeny:
				res = "kSecTrustResultDeny"; break;
			case kSecTrustResultUnspecified:
				res = "kSecTrustResultUnspecified"; break;
			case kSecTrustResultRecoverableTrustFailure:
				res = "kSecTrustResultRecoverableTrustFailure"; break;
			case kSecTrustResultFatalTrustFailure:
				res = "kSecTrustResultFatalTrustFailure"; break;
			case kSecTrustResultOtherError:
				res = "kSecTrustResultOtherError"; break;
			default:
				res = "UNKNOWN"; break;
		}
		printf("\nSecTrustEvaluate(): secTrustResult %s\n", res);
	}

	switch(secTrustResult) {
		case kSecTrustResultUnspecified:
			/* cert chain valid, no special UserTrust assignments */
		case kSecTrustResultProceed:
			/* cert chain valid AND user explicitly trusts this */
			break;
		default:
			printf("\n***SecTrustEvaluate reported secTrustResult %d\n",
				(int)secTrustResult);
			ortn = errSSLXCertChainInvalid;
			break;
	}

	*peerCerts = NULL;

	return ortn;
}

/* print reply received from server, safely */
static void dumpAscii(
	uint8_t *rcvBuf,
	size_t len)
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

static void
alpnFunc(SSLContextRef          ctx,
         void                    *info,
         const void			    *alpnData,
         size_t                  alpnDataLength)
{
    printf("[selected ALPN]");
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

/*
 * Perform one SSL diagnostic session. Returns nonzero on error. Normally no
 * output to stdout except initial "connecting to" message, unless there
 * is a really screwed up error (i.e., something not directly related
 * to the SSL connection).
 */
#define RCV_BUF_SIZE		256

static OSStatus sslPing(
	sslPingArgs *pargs)
{
    PeerSpec            peerId;
	otSocket			sock = 0;
    OSStatus            ortn;
    SSLContextRef       ctx = NULL;
    size_t              length;
	size_t				actLen;
    uint8_t             rcvBuf[RCV_BUF_SIZE];
	CFAbsoluteTime		startHandshake;
	CFAbsoluteTime		endHandshake;

    pargs->negVersion = kSSLProtocolUnknown;
    pargs->negCipher = SSL_NULL_WITH_NULL_NULL;
    pargs->peerCerts = NULL;

	/* first make sure requested server is there */
	ortn = MakeServerConnection(pargs->hostName, pargs->port, pargs->nonBlocking,
		&sock, &peerId);
    if(ortn) {
    	printf("MakeServerConnection returned %d; aborting\n", (int)ortn);
    	return ortn;
    }
	if(pargs->verbose) {
		printf("...connected to server; starting SecureTransport\n");
	}

	/*
	 * Set up a SecureTransport session.
	 * First the standard calls.
	 */
    ctx = SSLCreateContext(kCFAllocatorDefault, kSSLClientSide, kSSLStreamType);
	if(ctx == NULL) {
		printf("SSLCreateContext\n");
		goto cleanup;
	}
	ortn = SSLSetIOFuncs(ctx, SocketRead, SocketWrite);
	if(ortn) {
		printSslErrStr("SSLSetIOFuncs", ortn);
		goto cleanup;
	}
	ortn = SSLSetConnection(ctx, (SSLConnectionRef)(intptr_t)sock);
	if(ortn) {
		printSslErrStr("SSLSetConnection", ortn);
		goto cleanup;
	}
	SSLConnectionRef getConn;
	ortn = SSLGetConnection(ctx, &getConn);
	if(ortn) {
		printSslErrStr("SSLGetConnection", ortn);
		goto cleanup;
	}
	if(getConn != (SSLConnectionRef)(intptr_t)sock) {
		printf("***SSLGetConnection error\n");
		ortn = errSecParam;
		goto cleanup;
	}
	if(!pargs->allowHostnameSpoof) {
		/* if this isn't set, it isn't checked by AppleX509TP */
		const char *vfyHost = pargs->hostName;
		if(pargs->vfyHostName) {
			/* generally means we're expecting an error */
			vfyHost = pargs->vfyHostName;
		}
		ortn = SSLSetPeerDomainName(ctx, vfyHost, strlen(vfyHost));
		if(ortn) {
			printSslErrStr("SSLSetPeerDomainName", ortn);
			goto cleanup;
		}
	}

	/*
	 * SecureTransport options.
	 */
	if(pargs->acceptedProts) {
		ortn = SSLSetProtocolVersionEnabled(ctx, kSSLProtocolAll, false);
		if(ortn) {
			printSslErrStr("SSLSetProtocolVersionEnabled(all off)", ortn);
			goto cleanup;
		}
		for(const char *cp = pargs->acceptedProts; *cp; cp++) {
			SSLProtocol prot;
			switch(*cp) {
				case '2':
					prot = kSSLProtocol2;
					break;
				case '3':
					prot = kSSLProtocol3;
					break;
				case 't':
					prot = kTLSProtocol12;
					break;
				default:
					usage(pargs->argv);
			}
			ortn = SSLSetProtocolVersionEnabled(ctx, prot, true);
			if(ortn) {
				printSslErrStr("SSLSetProtocolVersionEnabled", ortn);
				goto cleanup;
			}
		}
	}
	else {
		ortn = SSLSetProtocolVersion(ctx, pargs->tryVersion);
		if(ortn) {
			printSslErrStr("SSLSetProtocolVersion", ortn);
			goto cleanup;
		}
		SSLProtocol getVers;
		ortn = SSLGetProtocolVersion(ctx, &getVers);
		if(ortn) {
			printSslErrStr("SSLGetProtocolVersion", ortn);
			goto cleanup;
		}
		if(getVers != pargs->tryVersion) {
			printf("***SSLGetProtocolVersion screwup: try %s  get %s\n",
				sslGetProtocolVersionString(pargs->tryVersion),
				sslGetProtocolVersionString(getVers));
			ortn = errSecParam;
			goto cleanup;
		}
	}
	if(pargs->resumableEnable) {
		const void *rtnId = NULL;
		size_t rtnIdLen = 0;

		ortn = SSLSetPeerID(ctx, &peerId, sizeof(PeerSpec));
		if(ortn) {
			printSslErrStr("SSLSetPeerID", ortn);
			goto cleanup;
		}
		/* quick test of the get fcn */
		ortn = SSLGetPeerID(ctx, &rtnId, &rtnIdLen);
		if(ortn) {
			printSslErrStr("SSLGetPeerID", ortn);
			goto cleanup;
		}
		if((rtnId == NULL) || (rtnIdLen != sizeof(PeerSpec))) {
			printf("***SSLGetPeerID screwup\n");
		}
		else if(memcmp(&peerId, rtnId, rtnIdLen) != 0) {
			printf("***SSLGetPeerID data mismatch\n");
		}
	}
	if(pargs->allowExpired) {
		ortn = SSLSetAllowsExpiredCerts(ctx, true);
		if(ortn) {
			printSslErrStr("SSLSetAllowExpiredCerts", ortn);
			goto cleanup;
		}
	}
	if(pargs->allowExpiredRoot) {
		ortn = SSLSetAllowsExpiredRoots(ctx, true);
		if(ortn) {
			printSslErrStr("SSLSetAllowsExpiredRoots", ortn);
			goto cleanup;
		}
	}
	if(pargs->disableCertVerify) {
		ortn = SSLSetEnableCertVerify(ctx, false);
		if(ortn) {
			printSslErrStr("SSLSetEnableCertVerify", ortn);
			goto cleanup;
		}
	}
	if(pargs->allowAnyRoot) {
		ortn = SSLSetAllowsAnyRoot(ctx, true);
		if(ortn) {
			printSslErrStr("SSLSetAllowAnyRoot", ortn);
			goto cleanup;
		}
	}
	if(pargs->cipherRestrict != '\0') {
		ortn = sslSetCipherRestrictions(ctx, pargs->cipherRestrict);
		if(ortn) {
			goto cleanup;
		}
	}
	if(pargs->anchorFile) {
		ortn = sslAddTrustedRoot(ctx, pargs->anchorFile, pargs->replaceAnchors);
		if(ortn) {
			printf("***Error obtaining anchor file %s\n", pargs->anchorFile);
			goto cleanup;
		}
	}
	if(pargs->clientCerts) {
		CFArrayRef dummy;
		if(pargs->anchorFile == NULL) {
			/* assume this is a root we want to implicitly trust */
			ortn = addIdentityAsTrustedRoot(ctx, pargs->clientCerts);
			if(ortn) {
				goto cleanup;
			}
		}
		ortn = SSLSetCertificate(ctx, pargs->clientCerts);
		if(ortn) {
			printSslErrStr("SSLSetCertificate", ortn);
			goto cleanup;
		}
		/* quickie test for this new function */
		ortn = SSLGetCertificate(ctx, &dummy);
		if(ortn) {
			printSslErrStr("SSLGetCertificate", ortn);
			goto cleanup;
		}
		if(dummy != pargs->clientCerts) {
			printf("***SSLGetCertificate error\n");
			ortn = errSecIO;
			goto cleanup;
		}
	}
    if (pargs->alpnNames) {
        CFMutableDataRef alpn = CFDataCreateMutable(NULL, 0);

        CFArrayForEach(pargs->alpnNames, ^(const void *value) {
            CFDataRef data = (CFDataRef)value;
            uint8_t len = CFDataGetLength(data);
            CFDataAppendBytes(alpn, (const UInt8 *)&len, sizeof(len));
            CFDataAppend(alpn, data);
        });

        SSLSetALPNData(ctx, CFDataGetBytePtr(alpn), CFDataGetLength(alpn));
        SSLSetALPNFunc(ctx, alpnFunc, (void *)NULL);
        CFRelease(alpn);
    }

	/*** end options ***/

	if(pargs->verbose) {
		printf("...starting SSL handshake\n");
	}
	startHandshake = CFAbsoluteTimeGetCurrent();

    do
    {   ortn = SSLHandshake(ctx);
	    if((ortn == errSSLWouldBlock) && !pargs->silent) {
	    	/* keep UI responsive */
	    	sslOutputDot();
	    }
    } while (ortn == errSSLWouldBlock);

	endHandshake = CFAbsoluteTimeGetCurrent();
	pargs->handshakeTimeOp = endHandshake - startHandshake;
	if(pargs->numHandshakes == 0) {
		/* special case, this one is always way longer */
		pargs->handshakeTimeFirst = pargs->handshakeTimeOp;
	}
	else {
		/* normal running total */
		pargs->handshakeTimeTotal += pargs->handshakeTimeOp;
	}
	pargs->numHandshakes++;

    ortn = SSLCopyPeerTrust(ctx, &pargs->peerTrust);
    if(ortn) {
        printf("***SSLCopyPeerTrust error %" PRIdOSStatus "\n", ortn);
        pargs->peerTrust = NULL;
    }

	/* ditto */
	SSLGetClientCertificateState(ctx, &pargs->certState);
	SSLGetNegotiatedCipher(ctx, &pargs->negCipher);
	SSLGetNegotiatedProtocolVersion(ctx, &pargs->negVersion);
	pargs->sessionIDLength = MAX_SESSION_ID_LENGTH;
	ortn = SSLGetResumableSessionInfo(ctx, &pargs->sessionWasResumed, pargs->sessionID, &pargs->sessionIDLength);
    if(!ortn) {
		OSStatus certRtn = sslEvaluateTrust(ctx, pargs, &pargs->peerCerts);

        if (certRtn && !pargs->manualCertVerify) {
            SSLCopyPeerCertificates(ctx, &pargs->peerCerts);
            certRtn = 0;
        }

		if(certRtn && !ortn ) {
			ortn = certRtn;
		}
	}

    if(ortn) {
		if(!pargs->silent) {
			printf("\n");
		}
    	goto cleanup;
    }

	if(pargs->verbose) {
		printf("...SSL handshake complete\n");
	}
	length = strlen(pargs->getMsg);
	(void) SSLWrite(ctx, pargs->getMsg, length, &actLen);

	/*
	 * Try to snag RCV_BUF_SIZE bytes. Exit if (!keepConnected and we get any data
	 * at all), or (keepConnected and err != (none, wouldBlock)).
	 */
    while (1) {
		actLen = 0;
		if(pargs->dumpRxData) {
			size_t avail = 0;

			ortn = SSLGetBufferedReadSize(ctx, &avail);
			if(ortn) {
				printf("***SSLGetBufferedReadSize error\n");
				break;
			}
			if(avail != 0) {
				printf("\n%d bytes available: ", (int)avail);
			}
		}
        ortn = SSLRead(ctx, rcvBuf, RCV_BUF_SIZE, &actLen);
        if((actLen == 0) && !pargs->silent) {
        	sslOutputDot();
        }
        if((actLen == 0) && (ortn == errSecSuccess)) {
			printf("***Radar 2984932 confirmed***\n");
		}
        if (ortn == errSSLWouldBlock) {
			/* for this loop, these are identical */
            ortn = errSecSuccess;
        }
		if((actLen > 0) && pargs->dumpRxData) {
			dumpAscii(rcvBuf, actLen);
		}
		if(ortn != errSecSuccess) {
			/* connection closed by server or by error */
			break;
		}
		if(!pargs->keepConnected && (actLen > 0)) {
        	/* good enough, we connected */
        	break;
        }
    }
	if(!pargs->silent) {
		printf("\n");
	}

	/* snag these again in case of renegotiate */
	SSLGetClientCertificateState(ctx, &pargs->certState);
	SSLGetNegotiatedCipher(ctx, &pargs->negCipher);
	SSLGetNegotiatedProtocolVersion(ctx, &pargs->negVersion);

    /* convert normal "shutdown" into zero err rtn */
	if(ortn == errSSLClosedGraceful) {
		ortn = errSecSuccess;
	}
	if((ortn == errSSLClosedNoNotify) && !pargs->requireNotify) {
		/* relaxed disconnect rules */
		ortn = errSecSuccess;
	}
cleanup: ;
	/*
	 * always do close, even on error - to flush outgoing write queue
	 */
	OSStatus cerr = SSLClose(ctx);
	if(ortn == errSecSuccess) {
		ortn = cerr;
	}
	if(sock) {
		endpointShutdown(sock);
	}
	if(ctx) {
	    CFRelease(ctx);
	}
	return ortn;
}

#if TARGET_OS_IPHONE

static void add_key(const void *key, const void *value, void *context) {
    CFArrayAppendValue((CFMutableArrayRef)context, key);
}


static void showInfo(CFDictionaryRef info) {
    CFIndex dict_count, key_ix, key_count;
    CFMutableArrayRef keys = NULL;
    CFIndex maxWidth = 20; /* Maybe precompute this or grab from context? */

    dict_count = CFDictionaryGetCount(info);
    keys = CFArrayCreateMutable(kCFAllocatorDefault, dict_count,
        &kCFTypeArrayCallBacks);
    CFDictionaryApplyFunction(info, add_key, keys);
    key_count = CFArrayGetCount(keys);
    CFArraySortValues(keys, CFRangeMake(0, key_count),
        (CFComparatorFunction)CFStringCompare, 0);

    for (key_ix = 0; key_ix < key_count; ++key_ix) {
        CFStringRef key = (CFStringRef)CFArrayGetValueAtIndex(keys, key_ix);
        CFTypeRef value = CFDictionaryGetValue(info, key);
        CFMutableStringRef line = CFStringCreateMutable(NULL, 0);

        CFStringAppend(line, key);
        CFIndex jx;
        for (jx = CFStringGetLength(key);
            jx < maxWidth; ++jx) {
            CFStringAppend(line, CFSTR(" "));
        }
        CFStringAppend(line, CFSTR(" : "));
        if (CFStringGetTypeID() == CFGetTypeID(value)) {
            CFStringAppend(line, (CFStringRef)value);
        } else if (CFDateGetTypeID() == CFGetTypeID(value)) {
            CFLocaleRef lc = CFLocaleCopyCurrent();
            CFDateFormatterRef df = CFDateFormatterCreate(NULL, lc,
                kCFDateFormatterFullStyle, kCFDateFormatterFullStyle);
            CFDateRef date = (CFDateRef)value;
            CFStringRef ds = CFDateFormatterCreateStringWithDate(NULL, df,
                date);
            CFStringAppend(line, ds);
            CFRelease(ds);
            CFRelease(df);
            CFRelease(lc);
        } else if (CFURLGetTypeID() == CFGetTypeID(value)) {
            CFURLRef url = (CFURLRef)value;
            CFStringAppend(line, CFSTR("<"));
            CFStringAppend(line, CFURLGetString(url));
            CFStringAppend(line, CFSTR(">"));
        } else if (CFDataGetTypeID() == CFGetTypeID(value)) {
            CFDataRef v_d = (CFDataRef)value;
            CFStringRef v_s = CFStringCreateFromExternalRepresentation(
                kCFAllocatorDefault, v_d, kCFStringEncodingUTF8);
            if (v_s) {
                CFStringAppend(line, CFSTR("/"));
                CFStringAppend(line, v_s);
                CFStringAppend(line, CFSTR("/ "));
                CFRelease(v_s);
            }
            const uint8_t *bytes = CFDataGetBytePtr(v_d);
            CFIndex len = CFDataGetLength(v_d);
            for (jx = 0; jx < len; ++jx) {
                CFStringAppendFormat(line, NULL, CFSTR("%.02X"), bytes[jx]);
            }
        } else {
            CFStringAppendFormat(line, NULL, CFSTR("%@"), value);
        }
        CFStringWriteToFileWithNewline(line, stdout);
		CFRelease(line);
    }
    CFRelease(keys);
}
#endif

#pragma clang diagnostic pop

static void showPeerTrust(SecTrustRef peerTrust, bool verbose) {

	if(peerTrust == NULL) {
		return;
	}
#if TARGET_OS_IPHONE
    CFIndex numCerts;
    CFIndex i;

    printf("\n=============== Peer Trust Properties ===============\n");
    CFArrayRef plist = SecTrustCopyProperties(peerTrust);
    if (plist) {
        print_plist(plist);
        CFRelease(plist);
    }

    printf("\n================== Peer Trust Info ==================\n");
    CFDictionaryRef info = SecTrustCopyInfo(peerTrust);
    if (info && CFDictionaryGetCount(info)) {
        showInfo(info);
    }
    if (info)
        CFRelease(info);

	numCerts = SecTrustGetCertificateCount(peerTrust);
	for(i=0; i<numCerts; i++) {
        plist = SecTrustCopySummaryPropertiesAtIndex(peerTrust, i);
		printf("\n============= Peer Trust Cert %lu Summary =============\n\n", i);
        print_plist(plist);
        if (plist)
            CFRelease(plist);
		printf("\n============= Peer Trust Cert %lu Details =============\n\n", i);
		plist = SecTrustCopyDetailedPropertiesAtIndex(peerTrust, i);
        print_plist(plist);
        if (plist)
            CFRelease(plist);
		printf("\n============= End of Peer Trust Cert %lu ==============\n", i);
	}
#endif
}

static void showPeerCerts(
	CFArrayRef			__unused peerCerts,
	bool			__unused verbose)
{
#if 0
	CFIndex numCerts;
	SecCertificateRef certRef;
	CFIndex i;

	if(peerCerts == NULL) {
		return;
	}
	numCerts = CFArrayGetCount(peerCerts);
	for(i=0; i<numCerts; i++) {
		certRef = (SecCertificateRef)CFArrayGetValueAtIndex(peerCerts, i);
		printf("\n==================== Peer Cert %lu ====================\n\n", i);
        print_cert(certRef, verbose);
		printf("\n================ End of Peer Cert %lu =================\n", i);
	}
#endif
}

static void writePeerCerts(
	CFArrayRef			peerCerts,
	const char			*fileBase)
{
	CFIndex numCerts;
	SecCertificateRef certRef;
	CFIndex i;
	char fileName[_maxFileStringSize];

	if(peerCerts == NULL) {
		return;
	}
	numCerts = CFArrayGetCount(peerCerts);
	for(i=0; i<numCerts; i++) {
        snprintf(fileName, _maxFileStringSize, "%s%02d.cer", fileBase, (int)i);
		certRef = (SecCertificateRef)CFArrayGetValueAtIndex(peerCerts, i);
        CFDataRef derCert = SecCertificateCopyData(certRef);
        if (derCert) {
            writeFileSizet(fileName, CFDataGetBytePtr(derCert),
                CFDataGetLength(derCert));
            CFRelease(derCert);
        }
	}
	printf("...wrote %lu certs to fileBase %s\n", numCerts, fileBase);
}

/*
 * Show result of an sslPing().
 * Assumes the following from sslPingArgs:
 *
 *		verbose
 *		tryVersion
 *		acceptedProts
 *		negVersion
 *		negCipher
 *		peerCerts
 *		certState
 * 		sessionWasResumed
 *		sessionID
 *		sessionIDLength
 *		handshakeTime
 */
static void showSSLResult(
	const sslPingArgs	*pargs,
	OSStatus			err,
	int                 displayPeerCerts,
	char				*fileBase)		// non-NULL: write certs to file
{
	CFIndex numPeerCerts;

	printf("\n");

	if(pargs->acceptedProts) {
		printf("   Allowed SSL versions   : %s\n", pargs->acceptedProts);
	}
	else {
		printf("   Attempted  SSL version : %s\n",
			sslGetProtocolVersionString(pargs->tryVersion));
	}

	printf("   Result                 : %s\n", sslGetSSLErrString(err));
	printf("   Negotiated SSL version : %s\n",
		sslGetProtocolVersionString(pargs->negVersion));
	printf("   Negotiated CipherSuite : %s\n",
		sslGetCipherSuiteString(pargs->negCipher));
	if(pargs->certState != kSSLClientCertNone) {
		printf("   Client Cert State      : %s\n",
			sslGetClientCertStateString(pargs->certState));
	}
	if(pargs->verbose) {
		printf("   Resumed Session        : ");
		if(pargs->sessionWasResumed) {
			for(unsigned dex=0; dex<pargs->sessionIDLength; dex++) {
				printf("%02X ", pargs->sessionID[dex]);
				if(((dex % 8) == 7) && (dex != (pargs->sessionIDLength - 1))) {
					printf("\n                            ");
				}
			}
			printf("\n");
		}
		else {
			printf("NOT RESUMED\n");
		}
		printf("   Handshake time         : %f seconds\n", pargs->handshakeTimeOp);
	}
	if(pargs->peerCerts == NULL) {
		numPeerCerts = 0;
	}
	else {
		numPeerCerts = CFArrayGetCount(pargs->peerCerts);
	}
	printf("   Number of server certs : %lu\n", numPeerCerts);
	if(numPeerCerts != 0) {
		if (displayPeerCerts == 1) {
			showPeerCerts(pargs->peerCerts, false);
		} else if (displayPeerCerts == 2) {
			showPeerTrust(pargs->peerTrust, false);
        }
		if(fileBase != NULL) {
			writePeerCerts(pargs->peerCerts, fileBase);
		}
	}

	printf("\n");
}

static int verifyProtocol(
	bool	verifyProt,
	SSLProtocol	maxProtocol,
	SSLProtocol	reqProtocol,
	SSLProtocol negProtocol)
{
	if(!verifyProt) {
		return 0;
	}
	if(reqProtocol > maxProtocol) {
		/* known not to support this attempt, relax */
		reqProtocol = maxProtocol;
	}
	if(reqProtocol != negProtocol) {
		printf("***Expected protocol %s; negotiated %s\n",
			sslGetProtocolVersionString(reqProtocol),
			sslGetProtocolVersionString(negProtocol));
		return 1;
	}
	else {
		return 0;
	}
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

static SSLProtocol charToProt(
	char c,			// 2, 3, t
	char **argv)
{
	switch(c) {
		case '2':
			return kSSLProtocol2;
		case '3':
			return kSSLProtocol3;
		case 't':
			return kTLSProtocol12;
		default:
			usage(argv);
	}
}

int main(int argc, char **argv)
{
    OSStatus            err;
	int					arg;
	char 				*argp;
	char				getMsg[300];
	char				fullFileBase[_maxFileStringSize];
	int					ourRtn = 0;			// exit status - sum of all errors
	unsigned			loop;
	SecKeychainRef		serverKc = nil;
	sslPingArgs			pargs;

	/* user-spec'd parameters */
	const char				*getPath = DEFAULT_PATH;
	char				*fileBase = NULL;
	int             displayCerts = 0;
	bool			doSslV2 = false;
	bool			doSslV3 = false;
	bool			doTlsV1 = true;
    bool			doTlsV11 = false;
    bool			doTlsV12 = false;
	bool			protXOnly = false;	// kSSLProtocol3Only, kTLSProtocol1Only
	bool			doProtUnknown = false;
	unsigned			loopCount = 1;
	bool			doPause = false;
	bool			pauseFirstLoop = false;
	bool			verifyProt = false;
	SSLProtocol			maxProtocol = kTLSProtocol12;	// for verifying negotiated
														// protocol
	char				*acceptedProts = NULL;
	char				*keyChainName = NULL;
	char				*getMsgSpec = NULL;
	bool			vfyCertState = false;
	SSLClientCertificateState expectCertState = kSSLClientCertNone;
	bool			displayHandshakeTimes = false;
	bool			completeCertChain = false;

	/* special case - one arg of "h" or "-h" or "hv" */
	if(argc == 2) {
	    if((strcmp(argv[1], "h") == 0) || (strcmp(argv[1], "-h") == 0)) {
			usage(argv);
		}
		if(strcmp(argv[1], "hv") == 0) {
			usageVerbose(argv);
		}
	}

	/* set up defaults */
	memset(&pargs, 0, sizeof(sslPingArgs));
	pargs.hostName = DEFAULT_HOST;
	pargs.port = DEFAULT_PORT;
	pargs.resumableEnable = true;
	pargs.argv = argv;

	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		if(arg == 1) {
			/* first arg, is always hostname; '-' means default */
			if(argp[0] != '-') {
				pargs.hostName = argp;
			}
			continue;
		}
		if(argp[0] == '/') {
			/* path always starts with leading slash */
			getPath = argp;
			continue;
		}
		/* options */
		switch(argp[0]) {
      case 'Z': {
          if(++arg == argc)  {
              /* requires another arg */
              usage(argv);
          }
          if (pargs.alpnNames == NULL) {
              pargs.alpnNames = CFArrayCreateMutableForCFTypes(NULL);
          }

          CFDataRef alpn = CFDataCreate(NULL, (const UInt8 *)argv[arg], strlen(argv[arg]));
          CFArrayAppendValue(pargs.alpnNames, alpn);
          CFReleaseNull(alpn);
          break;
      }
      case 'W':
      case 'w': {
          CFDictionaryRef context = NULL;

          if(++arg == argc)  {
              /* requires another arg */
              usage(argv);
          }

          if (argp[0] == 'W') {
              context = CFDictionaryCreateForCFTypes(NULL,
                                                     CFSTR("AppleServerAuthenticationAllowUATAPN"), kCFBooleanTrue,
                                                     CFSTR("AppleServerAuthenticationAllowUATIDS"), kCFBooleanTrue,
                                                     CFSTR("AppleServerAuthenticationAllowUATGS"), kCFBooleanTrue,
                                                     NULL);
          }
          const char *verifyName = pargs.hostName;

          if (pargs.policies == NULL) {
              pargs.policies = CFArrayCreateMutableForCFTypes(NULL);
          }

          if (pargs.vfyHostName)
              verifyName = pargs.vfyHostName;

          SecPolicyRef policy = NULL;
          CFStringRef hostname = CFStringCreateWithCString(NULL, verifyName, kCFStringEncodingUTF8);

          if (strcasecmp(argv[arg], "PushLegacy") == 0) {
              policy = SecPolicyCreateApplePushServiceLegacy(hostname);
          } else if (strcasecmp(argv[arg], "Push") == 0) {
              policy = SecPolicyCreateApplePushService(hostname, context);
          } else if (strcasecmp(argv[arg], "IDS") == 0) {
              policy = SecPolicyCreateAppleIDSServiceContext(hostname, context);
          } else if (strcasecmp(argv[arg], "GS") == 0) {
              policy = SecPolicyCreateAppleGSService(hostname, context);
          } else {
              printf("unknown policy: %s", argv[arg]);
              CFReleaseNull(hostname);
              CFReleaseNull(context);
              usage(argv);
          }

          if (policy) {
              CFArrayAppendValue(pargs.policies, policy);
          }

          CFReleaseNull(policy);
          CFReleaseNull(hostname);
          CFReleaseNull(context);

          break;
      }
			case 'e':
				pargs.allowExpired = true;
				break;
			case 'E':
				pargs.allowExpiredRoot = true;
				break;
			case 'x':
				pargs.disableCertVerify = true;
				break;
			case 'M':
				pargs.disableCertVerify = true;	// implied
				pargs.manualCertVerify = true;
				break;
			case 'a':
				if(++arg == argc)  {
					/* requires another arg */
					usage(argv);
				}
				pargs.anchorFile = argv[arg];
				break;
			case 'A':
				if(++arg == argc)  {
					/* requires another arg */
					usage(argv);
				}
				pargs.anchorFile = argv[arg];
				pargs.replaceAnchors = true;
				break;
			case 'r':
				pargs.allowAnyRoot = true;
				break;
			case 'd':
				pargs.dumpRxData = true;
				break;
			case 'c':
				displayCerts++;
				break;
			case 'f':
				if(++arg == argc)  {
					/* requires another arg */
					usage(argv);
				}
				fileBase = argv[arg];
				break;
			case 'C':
				pargs.cipherRestrict = argp[2];
				break;
			case '2':
				doSslV3 = doTlsV1 = doTlsV11 = false;
				doSslV2 = true;
				break;
			case '3':
				doSslV2 = doTlsV1 = doTlsV11 = doTlsV12 = false;
				doSslV3 = true;
				break;
			case 't':
				doSslV2 = doSslV3 = doTlsV11 = doTlsV12 = false;
				doTlsV1 = true;
				break;
            case '%':
				doSslV2 = doSslV3 = doTlsV1 = false;
				doTlsV11 = true;
                break;
            case '^':
				doSslV2 = doSslV3 = doTlsV1 = doTlsV12 = false;
				doTlsV12 = true;
                break;
			case 'L':
				doSslV2 = doSslV3 = doTlsV1 = doTlsV11 = doTlsV12 = true;
				break;
			case 'o':
				protXOnly = true;
				break;
			case 'u':
				doSslV2 = doSslV3 = doTlsV1 = doTlsV11 = doTlsV12 = false;
				doProtUnknown = true;
				break;
			case 'K':
				pargs.keepConnected = true;
				break;
			case 'n':
				pargs.requireNotify = true;
				pargs.keepConnected = true;
				break;
			case 'R':
				pargs.resumableEnable = false;
				break;
			case 'b':
				pargs.nonBlocking = true;
				break;
			case 'v':
				verifyProt = true;
				break;
			case 'm':
				if(argp[1] != '=') {
					usage(argv);
				}
				verifyProt = true;		// implied
				maxProtocol = charToProt(argp[2], argv);
				break;
			case 'g':
				if(argp[1] != '=') {
					usage(argv);
				}
				acceptedProts = argv[arg];
				doSslV3 = doSslV2 = doTlsV1 = doTlsV11 = doTlsV12 = false;
				break;
			case 'l':
        if(++arg == argc)  {
          /* requires another arg */
          usage(argv);
        }
				int parsedLoopCount = atoi(argv[arg]);
				if (parsedLoopCount <= 0) {
					printf("***bad loopCount\n");
					usage(argv);
				}
        loopCount = (unsigned) parsedLoopCount;
				break;
			case 'P':
                if(++arg == argc)  {
                    /* requires another arg */
                    usage(argv);
                }
				pargs.port = atoi(argv[arg]);
				break;
			case 'H':
				pargs.allowHostnameSpoof = true;
				break;
			case 'F':
                if(++arg == argc)  {
                    /* requires another arg */
                    usage(argv);
                }
				pargs.vfyHostName = argv[arg];
				break;
			case 'k':
                if(++arg == argc)  {
                    /* requires another arg */
                    usage(argv);
                }
				keyChainName = &argp[2];
				break;
			case 'G':
				getMsgSpec = &argp[2];
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
			case 'z':
				pargs.password = &argp[2];
				break;
			case 'p':
				doPause = true;
				break;
			case '7':
				pauseFirstLoop = true;
				break;
			case 'q':
				pargs.quiet = true;
				break;
			case 'V':
				pargs.verbose = true;
				break;
			case 's':
				pargs.silent = pargs.quiet = true;
				break;
			case 'N':
				displayHandshakeTimes = true;
				break;
			case '8':
				completeCertChain = true;
				break;
			case 'h':
				if(pargs.verbose || (argp[1] == 'v')) {
					usageVerbose(argv);
				}
				else {
					usage(argv);
				}
			default:
				usage(argv);
		}
	}
	if(getMsgSpec) {
		pargs.getMsg = getMsgSpec;
	}
	else {
		sprintf(getMsg, "%s %s %s",
			DEFAULT_GETMSG, getPath, DEFAULT_GET_SUFFIX);
		pargs.getMsg = getMsg;
	}

    /* get client cert and optional encryption cert as CFArrayRef */
	if(keyChainName) {
		pargs.clientCerts = getSslCerts(keyChainName, false, completeCertChain,
			pargs.anchorFile, &serverKc);
		if(pargs.clientCerts == nil) {
			exit(1);
		}
	}

    {
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_flags = SA_RESTART;
        sa.sa_handler = sigpipe;
        sigaction(SIGPIPE, &sa, NULL);
    }

	for(loop=0; loop<loopCount; loop++) {
		/*
		 * One pass for each protocol version, skipping any explicit version if
		 * an attempt at a higher version and succeeded in doing so successfully fell
		 * back.
		 */
		if(doTlsV12) {
			pargs.tryVersion = kTLSProtocol12;
			pargs.acceptedProts = NULL;
			if(!pargs.silent) {
				printf("Connecting to host %s with TLS V1.2...", pargs.hostName);
			}
			fflush(stdout);
			err = sslPing(&pargs);
			if(err) {
				ourRtn++;
			}
			if(!pargs.quiet) {
				if(fileBase) {
                    snprintf(fullFileBase, _maxFileStringSize, "%s_v3.1", fileBase);
				}
				showSSLResult(&pargs,
                              err,
                              displayCerts,
                              fileBase ? fullFileBase : NULL);
			}
			CFReleaseNull(pargs.peerCerts);
			if(!err) {
				/* deal with fallbacks, skipping redundant tests */
				switch(pargs.negVersion) {
                    case kTLSProtocol11:
                        doTlsV11  =false;
                        break;
                    case kTLSProtocol1:
                        doTlsV11  =false;
                        doTlsV1  =false;
                        break;
					case kSSLProtocol3:
                        doTlsV11  =false;
                        doTlsV1  =false;
						doSslV3 = false;
						break;
					case kSSLProtocol2:
                        doTlsV11  =false;
                        doTlsV1  =false;
						doSslV3 = false;
						doSslV2 = false;
						break;
					default:
						break;
				}
				ourRtn += verifyProtocol(verifyProt, maxProtocol, kTLSProtocol12,
                                         pargs.negVersion);
			}
			/* note we do this regardless since the client state might be
			 * the cause of a failure */
			ourRtn += verifyClientCertState(vfyCertState, expectCertState,
                                            pargs.certState);
		}
		if(doTlsV11) {
			pargs.tryVersion = kTLSProtocol11;
			pargs.acceptedProts = NULL;
			if(!pargs.silent) {
				printf("Connecting to host %s with TLS V1.1...", pargs.hostName);
			}
			fflush(stdout);
			err = sslPing(&pargs);
			if(err) {
				ourRtn++;
			}
			if(!pargs.quiet) {
				if(fileBase) {
                    snprintf(fullFileBase, _maxFileStringSize, "%s_v3.1", fileBase);
				}
				showSSLResult(&pargs,
                              err,
                              displayCerts,
                              fileBase ? fullFileBase : NULL);
			}
			CFReleaseNull(pargs.peerCerts);
			if(!err) {
				/* deal with fallbacks, skipping redundant tests */
				switch(pargs.negVersion) {
                    case kTLSProtocol1:
                        doTlsV1  =false;
                        break;
					case kSSLProtocol3:
                        doTlsV1  =false;
						doSslV3 = false;
						break;
					case kSSLProtocol2:
                        doTlsV1  =false;
						doSslV3 = false;
						doSslV2 = false;
						break;
					default:
						break;
				}
				ourRtn += verifyProtocol(verifyProt, maxProtocol, kTLSProtocol11,
                                         pargs.negVersion);
			}
			/* note we do this regardless since the client state might be
			 * the cause of a failure */
			ourRtn += verifyClientCertState(vfyCertState, expectCertState,
                                            pargs.certState);
		}
		if(doTlsV1) {
			pargs.tryVersion =
				protXOnly ? kTLSProtocol1Only : kTLSProtocol1;
			pargs.acceptedProts = NULL;
			if(!pargs.silent) {
				printf("Connecting to host %s with TLS V1...", pargs.hostName);
			}
			fflush(stdout);
			err = sslPing(&pargs);
			if(err) {
				ourRtn++;
			}
			if(!pargs.quiet) {
				if(fileBase) {
                    snprintf(fullFileBase, _maxFileStringSize, "%s_v3.1", fileBase);
				}
				showSSLResult(&pargs,
					err,
					displayCerts,
					fileBase ? fullFileBase : NULL);
			}
			CFReleaseNull(pargs.peerCerts);
			if(!err) {
				/* deal with fallbacks, skipping redundant tests */
				switch(pargs.negVersion) {
					case kSSLProtocol3:
						doSslV3 = false;
						break;
					case kSSLProtocol2:
						doSslV3 = false;
						doSslV2 = false;
						break;
					default:
						break;
				}
				ourRtn += verifyProtocol(verifyProt, maxProtocol, kTLSProtocol1,
					pargs.negVersion);
			}
			/* note we do this regardless since the client state might be
			 * the cause of a failure */
			ourRtn += verifyClientCertState(vfyCertState, expectCertState,
				pargs.certState);
		}
		if(doSslV3) {
			pargs.tryVersion = protXOnly ? kSSLProtocol3Only : kSSLProtocol3;
			pargs.acceptedProts = NULL;
			if(!pargs.silent) {
				printf("Connecting to host %s with SSL V3...", pargs.hostName);
			}
			fflush(stdout);
			err = sslPing(&pargs);
			if(err) {
				ourRtn++;
			}
			if(!pargs.quiet) {
				if(fileBase) {
                    snprintf(fullFileBase, _maxFileStringSize, "%s_v3.0", fileBase);
				}
				showSSLResult(&pargs,
					err,
					displayCerts,
					fileBase ? fullFileBase : NULL);
			}
			CFReleaseNull(pargs.peerCerts);
			if(!err) {
				/* deal with fallbacks, skipping redundant tests */
				switch(pargs.negVersion) {
					case kSSLProtocol2:
						doSslV2 = false;
						break;
					default:
						break;
				}
				ourRtn += verifyProtocol(verifyProt, maxProtocol, kSSLProtocol3,
					pargs.negVersion);
			}
			/* note we do this regardless since the client state might be
			 * the cause of a failure */
			ourRtn += verifyClientCertState(vfyCertState, expectCertState,
				pargs.certState);
		}

		if(doSslV2) {
			if(fileBase) {
                snprintf(fullFileBase, _maxFileStringSize, "%s_v2", fileBase);
			}
			if(!pargs.silent) {
				printf("Connecting to host %s with SSL V2...", pargs.hostName);
			}
			fflush(stdout);
			pargs.tryVersion = kSSLProtocol2;
			pargs.acceptedProts = NULL;
			err = sslPing(&pargs);
			if(err) {
				ourRtn++;
			}
			if(!pargs.quiet) {
				if(fileBase) {
                    snprintf(fullFileBase, _maxFileStringSize, "%s_v2", fileBase);
				}
				showSSLResult(&pargs,
					err,
					displayCerts,
					fileBase ? fullFileBase : NULL);
			}
			CFReleaseNull(pargs.peerCerts);
			if(!err) {
				ourRtn += verifyProtocol(verifyProt, maxProtocol, kSSLProtocol2,
					pargs.negVersion);
			}
			/* note we do this regardless since the client state might be
			 * the cause of a failure */
			ourRtn += verifyClientCertState(vfyCertState, expectCertState,
				pargs.certState);
		}
		if(doProtUnknown) {
			if(!pargs.silent) {
				printf("Connecting to host %s with kSSLProtocolUnknown...",
					pargs.hostName);
			}
			fflush(stdout);
			pargs.tryVersion = kSSLProtocolUnknown;
			pargs.acceptedProts = NULL;
			err = sslPing(&pargs);
			if(err) {
				ourRtn++;
			}
			if(!pargs.quiet) {
				if(fileBase) {
                    snprintf(fullFileBase, _maxFileStringSize, "%s_def", fileBase);
				}
				showSSLResult(&pargs,
					err,
					displayCerts,
					fileBase ? fullFileBase : NULL);
			}
			CFReleaseNull(pargs.peerCerts);
		}
		if(acceptedProts != NULL) {
			pargs.acceptedProts = acceptedProts;
			pargs.tryVersion = kSSLProtocolUnknown; // not used
			if(!pargs.silent) {
				printf("Connecting to host %s with acceptedProts %s...",
					pargs.hostName, pargs.acceptedProts);
			}
			fflush(stdout);
			err = sslPing(&pargs);
			if(err) {
				ourRtn++;
			}
			if(!pargs.quiet) {
				if(fileBase) {
                    snprintf(fullFileBase, _maxFileStringSize, "%s_def", fileBase);
				}
				showSSLResult(&pargs,
					err,
					displayCerts,
					fileBase ? fullFileBase : NULL);
			}
			CFReleaseNull(pargs.peerCerts);
		}
		if(doPause ||
		      (pauseFirstLoop &&
				 /* pause after first, before last to grab trace */
		         ((loop == 0) || (loop == loopCount - 1))
			  )
		   ) {
			char resp;
			fpurge(stdin);
			printf("a to abort, c to continue: ");
			resp = (char) getchar();
			if(resp == 'a') {
				break;
			}
		}
    }	/* main loop */
	if(displayHandshakeTimes) {
		CFAbsoluteTime totalTime;
		unsigned numHandshakes;
		if(pargs.numHandshakes == 1) {
			/* just display the first one */
			totalTime = pargs.handshakeTimeFirst;
			numHandshakes = 1;
		}
		else {
			/* skip the first one */
			totalTime = pargs.handshakeTimeTotal;
			numHandshakes = pargs.numHandshakes - 1;
		}
		if(numHandshakes != 0) {
			printf("   %u handshakes in %f seconds; %f seconds per handshake\n",
				numHandshakes, totalTime,
				(totalTime / numHandshakes));
		}
	}

    if(ourRtn) {
		printf("===%s exiting with %d %s for host %s\n", argv[0], ourRtn,
			(ourRtn > 1) ? "errors" : "error", pargs.hostName);
	}
    return ourRtn;

}
