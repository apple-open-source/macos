/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
	File:		sslctx.h

	Contains:	Private SSL typedefs: SSLContext and its components

	Written by:	Doug Mitchell, based on Netscape SSLRef 3.0

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/

/*  *********************************************************************
    File: sslctx.h

    SSLRef 3.0 Final -- 11/19/96

    Copyright (c)1996 by Netscape Communications Corp.

    By retrieving this software you are bound by the licensing terms
    disclosed in the file "LICENSE.txt". Please read it, and if you don't
    accept the terms, delete this software.

    SSLRef 3.0 was developed by Netscape Communications Corp. of Mountain
    View, California <http://home.netscape.com/> and Consensus Development
    Corporation of Berkeley, California <http://www.consensus.com/>.

    *********************************************************************

    File: sslctx.h     Internal state of an SSL connection

    Contains the SSLContext structure which encapsulates the state of the
    connection at any time. Whenever SSLREF might have to return (mostly
    when I/O is done), this structure must completely represent the
    connection state

    ****************************************************************** */

#ifndef _SSLCTX_H_
#define _SSLCTX_H_ 1

#include <Security/SecureTransport.h>
#include "sslBuildFlags.h"
#include <Security/cssmtype.h>

#include "sslerrs.h"
#include "sslPriv.h"
#include "tls_ssl.h"

typedef struct
{   SSLReadFunc         read;
    SSLWriteFunc        write;
    SSLConnectionRef   	ioRef;
} IOContext;

struct SystemContext
{   
	/* 
	 * This struct is a remnant of the original SSLRef implementation; it
	 * held things like caller-provided memory allocator callbacks.
	 * We'll keep the struct (and an instance of it in SSLContext, below)
	 * around in case we want to use it in SSLAllocBuffer and its siblings.
	 */
	int 				foo;
};

typedef struct SystemContext SystemContext;

/*
 * A carryover from original SSLRef 3.0 - we'll store the DER-encoded
 * certs in an SSLCertificate this way for now; there's a lot of code
 * which munges these lists.
 */
typedef struct SSLCertificate
{   
	struct SSLCertificate   *next;
    SSLBuffer               derCert;
} SSLCertificate;

#include "cryptType.h"

/*
 * An SSLContext contains four of these - one for each of {read,write} and for
 * {current, pending}.
 */
struct CipherContext
{   
	 
	const HashHmacReference   	*macRef;			/* HMAC (TLS) or digest (SSL) */
    const SSLSymmetricCipher  	*symCipher;
	
	/* this is a context which is reused once per record */
    HashHmacContext				macCtx;
	
    /* 
     * symKey is obtained from the CSP at cspHand. Normally this 
     * cspHand is the same as ctx->cspHand; some day they might differ.
     * Code which deals with this struct doesn't ever have to 
     * attach or detach from cspHand - that's taken care of at the
     * SSLContext level.
     */
    CSSM_KEY_PTR		symKey;	
    CSSM_CSP_HANDLE		cspHand;
    CSSM_CC_HANDLE		ccHand;

	/* needed in CDSASymmInit */
	uint8				encrypting;
	
    sslUint64           sequenceNum;
    uint8               ready;

	/* in SSL2 mode, the macSecret is the same size as the
	 * cipher key - which is 24 bytes in the 3DES case. */
	uint8				macSecret[MAX_SYMKEY_SIZE];
};
/* typedef in cryptType.h */

#include "sslhdshk.h"

typedef struct WaitingRecord
{   struct WaitingRecord    *next;
    SSLBuffer				data;
    uint32                  sent;
} WaitingRecord;

typedef struct DNListElem
{   struct DNListElem   *next;
    SSLBuffer		    derDN;
} DNListElem;

struct SSLContext
{   
	/*
	 * For Apple CDSA version, SystemContext is empty; we'll leave it in for now
	 * because it gets passed around so often for SSLAllocBuffer().
	 */
	SystemContext       sysCtx;
    IOContext           ioCtx;
    
	/* 
	 * For the first two, SSL_Version_Undetermined means "get the best we
	 * can, up to macProtocolVersion".
	 */
    SSLProtocolVersion  reqProtocolVersion;	/* requested by app */
    SSLProtocolVersion  negProtocolVersion;	/* negotiated */
    SSLProtocolVersion  maxProtocolVersion;	/* max allowed by app */
    SSLProtocolSide     protocolSide;
    const struct _SslTlsCallouts *sslTslCalls; /* selects between SSLv3 and TLSv1 */
	
    /* crypto state in CDSA-centric terms */
    
    CSSM_KEY_PTR		signingPrivKey;/* our private signing key */
    CSSM_KEY_PTR		signingPubKey;	/* our public signing key */
    CSSM_CSP_HANDLE		signingKeyCsp;	/* associated DL/CSP */
	#if			ST_KEYCHAIN_ENABLE
		#if 	ST_KC_KEYS_NEED_REF
		SecKeychainRef	signingKeyRef;	/* for signingPrivKey */
		#else
		void			*signingKeyRef;	/* TBD */
		#endif	/* ST_KC_KEYS_NEED_REF */
    #endif
	
	/* this stuff should probably be #if ST_SERVER_MODE_ENABLE....  */
    CSSM_KEY_PTR		encryptPrivKey;/* our private encrypt key, for 
    									 * server-initiated key exchange */
    CSSM_KEY_PTR		encryptPubKey;	/* public version of above */
    CSSM_CSP_HANDLE		encryptKeyCsp;
	#if		ST_KEYCHAIN_ENABLE
		#if 	ST_KC_KEYS_NEED_REF
		SecKeychainRef	encryptKeyRef;	/* for signingPrivKey */
		#else
		void			*encryptKeyRef;	/* TBD */
		#endif	/* ST_KC_KEYS_NEED_REF */
    #endif 	/* ST_KEYCHAIN_ENABLE */
	
    CSSM_KEY_PTR		peerPubKey;
    CSSM_CSP_HANDLE		peerPubKeyCsp;	/* may not be needed, we figure this
    									 * one out by trial&error, right? */
    									 
  	/* 
  	 * Various cert chains stored in an SSLRef-centric way for now
  	 * (see comments above re: SSLCertificate).
  	 * For all three, the root is the first in the chain. 
  	 */
    SSLCertificate      *localCert;
    SSLCertificate		*encryptCert;
    SSLCertificate      *peerCert;
    
    /* 
     * trusted root certs; specific to this implementation, we'll store
     * them conveniently...these will be used as AnchorCerts in a TP
     * call. 
     */
    UInt32				numTrustedCerts;
    CSSM_DATA_PTR		trustedCerts;
    
    /*
     * Keychain to which newly encountered root certs are attempted
     * to be added. AccessCreds untyped for now.
     */
	#if		ST_KEYCHAIN_ENABLE && ST_MANAGES_TRUSTED_ROOTS
    SecKeychainRef		newRootCertKc;
    void				*accessCreds;
    #endif	/* ST_KEYCHAIN_ENABLE && ST_MANAGES_TRUSTED_ROOTS */
	
    /* for symmetric cipher and RNG */
    CSSM_CSP_HANDLE		cspHand;
    
    /* session-wide handles for Apple TP, CL */
    CSSM_TP_HANDLE		tpHand;
    CSSM_CL_HANDLE		clHand;
    
	#if 	ST_FAKE_KEYCHAIN || ST_FAKE_GET_CSPDL_HANDLE
	/* we manually attach to this for now; eventually we get it from KC */
	CSSM_CSP_HANDLE		cspDlHand;
	#endif
	
    /* FIXME - how will we represent this? */
    void         		*dhAnonParams;
    void         		*peerDHParams;
        
	Boolean				allowExpiredCerts;
	
    SSLBuffer		    sessionID;
    
    SSLBuffer			dhPeerPublic;
    SSLBuffer 			dhExchangePublic;
    SSLBuffer 			dhPrivate;
    
    SSLBuffer			peerID;
    SSLBuffer			resumableSession;
    
	char				*peerDomainName;
	UInt32				peerDomainNameLen;
	
    CipherContext       readCipher;
    CipherContext       writeCipher;
    CipherContext       readPending;
    CipherContext       writePending;
    
    uint16              selectedCipher;			/* currently selected */
    const SSLCipherSpec *selectedCipherSpec;	/* ditto */
    SSLCipherSpec		*validCipherSpecs;		/* context's valid specs */ 
    unsigned			numValidCipherSpecs;	/* size of validCipherSpecs */
    SSLHandshakeState   state;
    
	#if		ST_SERVER_MODE_ENABLE
    SSLAuthenticate		clientAuth;			/* kNeverAuthenticate, etc. */
    Boolean				tryClientAuth;
	#endif	/* ST_SERVER_MODE_ENABLE */
    int                 certRequested;
    int                 certSent;
    int                 certReceived;
    int                 x509Requested;
    DNListElem          *acceptableDNList;
    
    uint8               clientRandom[SSL_CLIENT_SRVR_RAND_SIZE];
    uint8               serverRandom[SSL_CLIENT_SRVR_RAND_SIZE];
    SSLBuffer   		preMasterSecret;
    uint8               masterSecret[48];
    
	/* running digests of all handshake messages */
    SSLBuffer   		shaState, md5State;
    
    SSLBuffer		    fragmentedMessageCache;
    
    int                 ssl2ChallengeLength;
    int                 ssl2ConnectionIDLength;
    int                 ssl2SessionMatch;
    
/* Record layer fields */
    SSLBuffer    		partialReadBuffer;
    uint32              amountRead;
    
/* Transport layer fields */
    WaitingRecord       *recordWriteQueue;
    SSLBuffer			receivedDataBuffer;
    uint32              receivedDataPos;
    
    Boolean				allowAnyRoot;		// don't require known roots
    #if		SSL_DEBUG
    char				*rootCertName;		// if non-null, write root cert here    
    #endif	/* SSL_DEBUG */
    
};

#endif /* _SSLCTX_H_ */
