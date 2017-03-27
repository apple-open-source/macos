/*
 * Copyright (c) 1999-2001,2005-2014 Apple Inc. All Rights Reserved.
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
 * sslContext.c - SSLContext accessors
 */

#include "SecureTransport.h"

#include "SSLRecordInternal.h"
#include "SecureTransportPriv.h"
#include "ssl.h"
#include "sslCipherSpecs.h"
#include "sslContext.h"
#include "sslCrypto.h"
#include "sslDebug.h"
#include "sslKeychain.h"
#include "sslMemory.h"

#include "tlsCallbacks.h"

#include <AssertMacros.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFPreferences.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecTrust.h>
#include <Security/SecTrustSettingsPriv.h>
#include <Security/oidsalg.h>
#include "utilities/SecCFRelease.h"
#include "utilities/SecCFWrappers.h"
#include <pthread.h>
#include <string.h>

#if TARGET_OS_IPHONE
#include <Security/SecCertificateInternal.h>
#else
#include <Security/oidsalg.h>
#include <Security/oidscert.h>
#include <Security/SecTrustSettingsPriv.h>
#endif


static void sslFreeDnList(SSLContext *ctx)
{
    DNListElem      *dn, *nextDN;

    dn = ctx->acceptableDNList;
    while (dn)
    {   
        SSLFreeBuffer(&dn->derDN);
        nextDN = dn->next;
        sslFree(dn);
        dn = nextDN;
    }
    ctx->acceptableDNList = NULL;
}

Boolean sslIsSessionActive(const SSLContext *ctx)
{
	assert(ctx != NULL);


	switch(ctx->state) {
		case SSL_HdskStateUninit:
		case SSL_HdskStateGracefulClose:
		case SSL_HdskStateErrorClose:
			return false;
		default:
			return true;
	}

}

/*
 * Minimum and maximum supported versions
 */
//#define MINIMUM_STREAM_VERSION  SSL_Version_2_0 /* Disabled */
#define MINIMUM_STREAM_VERSION  SSL_Version_3_0
#define MAXIMUM_STREAM_VERSION  TLS_Version_1_2
#define MINIMUM_DATAGRAM_VERSION  DTLS_Version_1_0

/* This should be changed when we start supporting DTLS_Version_1_x */
#define MAXIMUM_DATAGRAM_VERSION  DTLS_Version_1_0

#define SSL_ENABLE_ECDSA_SIGN_AUTH			1
#define SSL_ENABLE_RSA_FIXED_ECDH_AUTH		0
#define SSL_ENABLE_ECDSA_FIXED_ECDH_AUTH	0

#define DEFAULT_DTLS_TIMEOUT    1
#define DEFAULT_DTLS_MTU        1400
#define MIN_ALLOWED_DTLS_MTU    64      /* this ensure than there will be no integer
                                            underflow when calculating max write size */

/* Preferences values */
CFIndex kMinDhGroupSizeDefaultValue;
CFIndex kMinProtocolVersionDefaultValue;
CFStringRef kSSLSessionConfigDefaultValue;
Boolean kSSLDisableRecordSplittingDefaultValue;

static tls_cache_t g_session_cache = NULL;

#if TARGET_OS_IPHONE
/*
 * Instead of using CFPropertyListReadFromFile we use a
 * CFPropertyListCreateWithStream directly
 * here. CFPropertyListReadFromFile() uses
 * CFURLCopyResourcePropertyForKey() and CF pulls in CoreServices for
 * CFURLCopyResourcePropertyForKey() and that doesn't work in install
 * enviroment.
 */
static CFPropertyListRef
CopyPlistFromFile(CFURLRef url)
{
    CFDictionaryRef d = NULL;
    CFReadStreamRef s = CFReadStreamCreateWithFile(kCFAllocatorDefault, url);
    if (s && CFReadStreamOpen(s)) {
        d = (CFDictionaryRef)CFPropertyListCreateWithStream(kCFAllocatorDefault, s, 0, kCFPropertyListImmutable, NULL, NULL);
    }
    CFReleaseSafe(s);

    return d;
}
#endif


static
CFTypeRef SSLPreferencesCopyValue(CFStringRef key, CFPropertyListRef managed_prefs)
{
    CFTypeRef value = (CFTypeRef) CFPreferencesCopyAppValue(CFSTR("SSLSessionConfig"), kCFPreferencesCurrentApplication);

    if(!value && managed_prefs) {
        value =  CFDictionaryGetValue(managed_prefs, key);
        if (value)
            CFRetain(value);
    }

    return value;
}

static
CFIndex SSLPreferencesGetInteger(CFStringRef key, CFPropertyListRef managed_prefs)
{
    CFTypeRef value = SSLPreferencesCopyValue(key, managed_prefs);
    CFIndex int_value = 0;
    if (isNumber(value)) {
        CFNumberGetValue(value, kCFNumberCFIndexType, &int_value);
    }
    CFReleaseSafe(value);
    return int_value;
}

static
Boolean SSLPreferencesGetBoolean(CFStringRef key, CFPropertyListRef managed_prefs)
{
    CFTypeRef value = SSLPreferencesCopyValue(key, managed_prefs);
    Boolean bool_value = FALSE;
    if (isBoolean(value)) {
        bool_value = CFBooleanGetValue(value);
    }

    CFReleaseSafe(value);
    return bool_value;
}

static
CFStringRef SSLPreferencesCopyString(CFStringRef key, CFPropertyListRef managed_prefs)
{
    CFTypeRef value = SSLPreferencesCopyValue(key, managed_prefs);
    if (isString(value)) {
        return value;
    } else {
        CFReleaseSafe(value);
        return NULL;
    }
}

static void _SSLContextReadDefault()
{
    CFPropertyListRef managed_prefs = NULL;

#if TARGET_OS_IPHONE
    /* on iOS, we also look for preferences from mobile's Managed Preferences */
    /* Note that if the process is running as mobile, the above call will already have read the Managed Preference plist.
     As a result, if you have some preferences set manually with defaults, which preference applies may be different for mobile vs not-mobile. */
    CFURLRef prefURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, CFSTR("/Library/Managed Preferences/mobile/.GlobalPreferences.plist"), kCFURLPOSIXPathStyle, false);
    if(prefURL) {
        managed_prefs = CopyPlistFromFile(prefURL);
    }
    CFReleaseSafe(prefURL);
#endif

    /* Disable record splitting */
    /* Enabled by default, this may cause some interop issues, see <rdar://problem/12307662> and <rdar://problem/12323307> */
    kSSLDisableRecordSplittingDefaultValue = SSLPreferencesGetBoolean(CFSTR("SSLDisableRecordSplitting"), managed_prefs);

    /* Min DH Group Size */
    kMinDhGroupSizeDefaultValue = SSLPreferencesGetInteger(CFSTR("SSLMinDhGroupSize"), managed_prefs);

    /* Default Min Prototcol Version */
    kMinProtocolVersionDefaultValue = SSLPreferencesGetInteger(CFSTR("SSLMinProtocolVersion"), managed_prefs);

    /* Default Config */
    kSSLSessionConfigDefaultValue = SSLPreferencesCopyString(CFSTR("SSLSessionConfig"), managed_prefs);

    CFReleaseSafe(managed_prefs);
}

/* This functions initialize global variables, run once per process */
static void SSLContextOnce(void)
{
    _SSLContextReadDefault();
    g_session_cache = tls_cache_create();
}

CFGiblisWithHashFor(SSLContext)

OSStatus
SSLNewContext				(Boolean 			isServer,
							 SSLContextRef 		*contextPtr)	/* RETURNED */
{
	if(contextPtr == NULL) {
		return errSecParam;
	}

	*contextPtr = SSLCreateContext(kCFAllocatorDefault, isServer?kSSLServerSide:kSSLClientSide, kSSLStreamType);

	if (*contextPtr == NULL)
		return errSecAllocate;

	return errSecSuccess;
}

SSLContextRef SSLCreateContext(CFAllocatorRef alloc, SSLProtocolSide protocolSide, SSLConnectionType connectionType)
{
    SSLContextRef ctx;
    SSLRecordContextRef recCtx;
    
    ctx = SSLCreateContextWithRecordFuncs(alloc, protocolSide, connectionType, &SSLRecordLayerInternal);

    if(ctx==NULL)
        return NULL;

    recCtx = SSLCreateInternalRecordLayer(ctx);
    if(recCtx==NULL) {
    	CFRelease(ctx);
		return NULL;
    }

    SSLSetRecordContext(ctx, recCtx);

    return ctx;
}

SSLContextRef SSLCreateContextWithRecordFuncs(CFAllocatorRef alloc, SSLProtocolSide protocolSide, SSLConnectionType connectionType, const struct SSLRecordFuncs *recFuncs)
{
	SSLContext  *ctx = (SSLContext*) _CFRuntimeCreateInstance(alloc, SSLContextGetTypeID(), sizeof(SSLContext) - sizeof(CFRuntimeBase), NULL);

	if(ctx == NULL) {
		return NULL;
	}

	/* subsequent errors to errOut: */
    memset(((uint8_t*) ctx) + sizeof(CFRuntimeBase), 0, sizeof(SSLContext) - sizeof(CFRuntimeBase));


    ctx->hdsk = tls_handshake_create(connectionType==kSSLDatagramType, protocolSide==kSSLServerSide);
    if(ctx->hdsk == NULL) {
        CFRelease(ctx);
        return NULL;
    }

    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        SSLContextOnce();
    });

    ctx->cache = g_session_cache;

    tls_handshake_set_callbacks(ctx->hdsk,
                                &tls_handshake_callbacks,
                                ctx);

    ctx->isDTLS = (connectionType==kSSLDatagramType);

    ctx->state = SSL_HdskStateUninit;
    ctx->timeout_duration = DEFAULT_DTLS_TIMEOUT;
    ctx->mtu = DEFAULT_DTLS_MTU;

    tls_handshake_get_min_protocol_version(ctx->hdsk, &ctx->minProtocolVersion);
    tls_handshake_get_max_protocol_version(ctx->hdsk, &ctx->maxProtocolVersion);

    if(protocolSide == kSSLClientSide) {
        tls_handshake_set_sct_enable(ctx->hdsk, true);
        tls_handshake_set_ocsp_enable(ctx->hdsk, true);
    }
    
	ctx->negProtocolVersion = SSL_Version_Undetermined;
    ctx->protocolSide = protocolSide;
    ctx->recFuncs = recFuncs;

	/* Initial cert verify state: verify with default system roots */
	ctx->enableCertVerify = true;

	/* Default for RSA blinding is ENABLED */
	ctx->rsaBlindingEnable = true;

	/* Default for sending one-byte app data record is ENABLED */
    ctx->oneByteRecordEnable = !kSSLDisableRecordSplittingDefaultValue;

    /* Dont enable fallback behavior by default */
    ctx->fallbackEnabled = false;

    if(kSSLSessionConfigDefaultValue) {
        SSLSetSessionConfig(ctx, kSSLSessionConfigDefaultValue);
    }

    if(kMinDhGroupSizeDefaultValue) {
        tls_handshake_set_min_dh_group_size(ctx->hdsk, (unsigned)kMinDhGroupSizeDefaultValue);
    }

    if(kMinProtocolVersionDefaultValue) {
        SSLSetProtocolVersionMin(ctx, (unsigned)kMinProtocolVersionDefaultValue);
    }

	/* default for anonymous ciphers is DISABLED */
	ctx->anonCipherEnable = false;

    ctx->breakOnServerAuth = false;
    ctx->breakOnCertRequest = false;
    ctx->breakOnClientAuth = false;
    ctx->signalServerAuth = false;
    ctx->signalCertRequest = false;
    ctx->signalClientAuth = false;

	return ctx;
}

OSStatus
SSLNewDatagramContext       (Boolean 			isServer,
							 SSLContextRef 		*contextPtr)	/* RETURNED */
{
	if (contextPtr == NULL)
		return errSecParam;
	*contextPtr = SSLCreateContext(kCFAllocatorDefault, isServer?kSSLServerSide:kSSLClientSide, kSSLDatagramType);
	if (*contextPtr == NULL)
		return errSecAllocate;
    return errSecSuccess;
}

/*
 * Dispose of an SSLContext. (private)
 * This function is invoked after our dispatch queue is safely released,
 * or directly from SSLDisposeContext if there is no dispatch queue.
 */
OSStatus
SSLDisposeContext				(SSLContextRef context)
{
    if(context == NULL) {
        return errSecParam;
    }
	CFRelease(context);
	return errSecSuccess;
}

CFStringRef SSLContextCopyFormatDescription(CFTypeRef arg, CFDictionaryRef formatOptions)
{
    SSLContext* ctx = (SSLContext*) arg;

    if (ctx == NULL) {
        return NULL;
    } else {
		CFStringRef result = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("<SSLContext(%p) { ... }>"), ctx);
		return result;
    }
}

Boolean SSLContextCompare(CFTypeRef a, CFTypeRef b)
{
	return a == b;
}

CFHashCode SSLContextHash(CFTypeRef arg)
{
	return (CFHashCode) arg;
}

void SSLContextDestroy(CFTypeRef arg)
{
	SSLContext* ctx = (SSLContext*) arg;

    /* destroy the coreTLS handshake object */
    tls_handshake_destroy(ctx->hdsk);

    /* Only destroy if we were using the internal record layer */
    if(ctx->recFuncs==&SSLRecordLayerInternal)
        SSLDestroyInternalRecordLayer(ctx->recCtx);

    SSLFreeBuffer(&ctx->sessionTicket);
    SSLFreeBuffer(&ctx->sessionID);
    SSLFreeBuffer(&ctx->peerID);
    SSLFreeBuffer(&ctx->resumableSession);
    SSLFreeBuffer(&ctx->receivedDataBuffer);

    CFReleaseSafe(ctx->acceptableCAs);
#if !TARGET_OS_IPHONE
    CFReleaseSafe(ctx->trustedLeafCerts);
#endif
    CFReleaseSafe(ctx->localCertArray);
    CFReleaseSafe(ctx->encryptCertArray);
    CFReleaseSafe(ctx->trustedCerts);
    CFReleaseSafe(ctx->peerSecTrust);

    sslFreeDnList(ctx);

    SSLFreeBuffer(&ctx->ownVerifyData);
    SSLFreeBuffer(&ctx->peerVerifyData);

    SSLFreeBuffer(&ctx->pskIdentity);
    SSLFreeBuffer(&ctx->pskSharedSecret);

    SSLFreeBuffer(&ctx->dhParamsEncoded);

    if(ctx->cache)
        tls_cache_cleanup(ctx->cache);

    memset(((uint8_t*) ctx) + sizeof(CFRuntimeBase), 0, sizeof(SSLContext) - sizeof(CFRuntimeBase));
}

/*
 * Determine the state of an SSL session.
 */
OSStatus
SSLGetSessionState			(SSLContextRef		context,
							 SSLSessionState	*state)		/* RETURNED */
{
	SSLSessionState rtnState = kSSLIdle;

	if(context == NULL) {
		return errSecParam;
	}
	*state = rtnState;
	switch(context->state) {
		case SSL_HdskStateUninit:
			rtnState = kSSLIdle;
			break;
		case SSL_HdskStateGracefulClose:
			rtnState = kSSLClosed;
			break;
		case SSL_HdskStateErrorClose:
		case SSL_HdskStateNoNotifyClose:
			rtnState = kSSLAborted;
			break;
		case SSL_HdskStateReady:
			rtnState = kSSLConnected;
			break;
        case SSL_HdskStatePending:
			rtnState = kSSLHandshake;
			break;
	}
	*state = rtnState;
	return errSecSuccess;
}

/*
 * Set options for an SSL session.
 */
OSStatus
SSLSetSessionOption			(SSLContextRef		context,
							 SSLSessionOption	option,
							 Boolean			value)
{
    if(context == NULL) {
	return errSecParam;
    }
    if(sslIsSessionActive(context)) {
	/* can't do this with an active session */
	return errSecBadReq;
    }
    switch(option) {
        case kSSLSessionOptionBreakOnServerAuth:
            context->breakOnServerAuth = value;
            context->enableCertVerify = !value;
            break;
        case kSSLSessionOptionBreakOnCertRequested:
            context->breakOnCertRequest = value;
            break;
        case kSSLSessionOptionBreakOnClientAuth:
            context->breakOnClientAuth = value;
            context->enableCertVerify = !value;
            break;
        case kSSLSessionOptionSendOneByteRecord:
            /* Only call the record layer function if the value changed */
            if(value != context->oneByteRecordEnable)
                context->recFuncs->setOption(context->recCtx, kSSLRecordOptionSendOneByteRecord, value);
            context->oneByteRecordEnable = value;
            break;
        case kSSLSessionOptionFalseStart:
            context->falseStartEnabled = value;
            break;
        case kSSLSessionOptionFallback:
            tls_handshake_set_fallback(context->hdsk, value);
            context->fallbackEnabled = value;
            break;
        case kSSLSessionOptionBreakOnClientHello:
            context->breakOnClientHello = value;
            break;
        case kSSLSessionOptionAllowServerIdentityChange:
            tls_handshake_set_server_identity_change(context->hdsk, value);
            break;
        case kSSLSessionOptionAllowRenegotiation:
            tls_handshake_set_renegotiation(context->hdsk, value);
            break;
        default:
            return errSecParam;
    }

    return errSecSuccess;
}

/*
 * Determine current value for the specified option in an SSL session.
 */
OSStatus
SSLGetSessionOption			(SSLContextRef		context,
							 SSLSessionOption	option,
							 Boolean			*value)
{
    if(context == NULL || value == NULL) {
        return errSecParam;
    }
    switch(option) {
        case kSSLSessionOptionBreakOnServerAuth:
            *value = context->breakOnServerAuth;
            break;
        case kSSLSessionOptionBreakOnCertRequested:
            *value = context->breakOnCertRequest;
            break;
        case kSSLSessionOptionBreakOnClientAuth:
            *value = context->breakOnClientAuth;
            break;
        case kSSLSessionOptionSendOneByteRecord:
            *value = context->oneByteRecordEnable;
            break;
        case kSSLSessionOptionFalseStart:
            *value = context->falseStartEnabled;
            break;
        case kSSLSessionOptionBreakOnClientHello:
            *value = context->breakOnClientHello;
            break;
        case kSSLSessionOptionAllowServerIdentityChange:
            tls_handshake_get_server_identity_change(context->hdsk, (bool *)value);
            break;
        default:
            return errSecParam;
    }

    return errSecSuccess;
}

OSStatus
SSLSetRecordContext         (SSLContextRef          ctx,
                             SSLRecordContextRef    recCtx)
{
	if(ctx == NULL) {
		return errSecParam;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return errSecBadReq;
	}
    ctx->recCtx = recCtx;
    return errSecSuccess;
}

OSStatus
SSLSetIOFuncs				(SSLContextRef		ctx,
							 SSLReadFunc 		readFunc,
							 SSLWriteFunc		writeFunc)
{
	if(ctx == NULL) {
		return errSecParam;
	}
    if(ctx->recFuncs!=&SSLRecordLayerInternal) {
        /* Can Only do this with the internal record layer */
        check(0);
        return errSecBadReq;
    }
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return errSecBadReq;
	}

    ctx->ioCtx.read=readFunc;
    ctx->ioCtx.write=writeFunc;

    return 0;
}

void
SSLSetNPNFunc(SSLContextRef      context,
			  SSLNPNFunc         npnFunc,
			  void               *info)
{
    if (context == NULL) {
        return;
	}
	if (sslIsSessionActive(context)) {
		return;
	}
    context->npnFunc = npnFunc;
    context->npnFuncInfo = info;
    if(context->protocolSide==kSSLClientSide) {
        tls_handshake_set_npn_enable(context->hdsk, npnFunc!=NULL);
    }
}

OSStatus
SSLSetNPNData(SSLContextRef      context,
			  const void		 *data,
			  size_t			 length)
{
    if (context == NULL || data == NULL || length == 0) {
        return errSecParam;
	}

	if (length > 255) {
		return errSecParam;
	}

    tls_buffer npn_data;

    npn_data.data = (uint8_t *)data;
    npn_data.length = length;

    return tls_handshake_set_npn_data(context->hdsk, npn_data);
}

const void *
SSLGetNPNData(SSLContextRef      context,
							 size_t				*length)
{
	if (context == NULL || length == NULL)
		return NULL;

    const tls_buffer *npn_data;

    npn_data = tls_handshake_get_peer_npn_data(context->hdsk);

    if(npn_data) {
        *length = npn_data->length;
        return npn_data->data;
    } else {
        return NULL;
    }
}

// ALNP begin

void
SSLSetALPNFunc(SSLContextRef      context,
               SSLALPNFunc        alpnFunc,
               void               *info)
{
    if (context == NULL) {
        return;
    }
    if (sslIsSessionActive(context)) {
        return;
    }
    context->alpnFunc = alpnFunc;
    context->alpnFuncInfo = info;
    if(context->protocolSide==kSSLServerSide) {
        // to do :
    }
}

OSStatus
SSLSetALPNData(SSLContextRef      context,
              const void		 *data,
              size_t			 length)
{
    if (context == NULL || data == NULL || length == 0) {
        return errSecParam;
    }

    if (length > 255) {
        return errSecParam;
    }

    tls_buffer alpn_data;

    alpn_data.data = (uint8_t *)data;
    alpn_data.length = length;

    return tls_handshake_set_alpn_data(context->hdsk, alpn_data);
}

const void *
SSLGetALPNData(SSLContextRef      context,
              size_t				*length)
{
    if (context == NULL || length == NULL)
        return NULL;

    const tls_buffer *alpn_data;

    alpn_data = tls_handshake_get_peer_alpn_data(context->hdsk);

    if(alpn_data) {
        *length = alpn_data->length;
        return alpn_data->data;
    } else {
        return NULL;
    }
}

// ALPN end

OSStatus
SSLSetConnection			(SSLContextRef		ctx,
							 SSLConnectionRef	connection)
{
	if(ctx == NULL) {
		return errSecParam;
	}
    if(ctx->recFuncs!=&SSLRecordLayerInternal) {
        /* Can Only do this with the internal record layer */
        check(0);
        return errSecBadReq;
    }
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return errSecBadReq;
	}

    /* Need to keep a copy of it this layer for the Get function */
    ctx->ioCtx.ioRef = connection;

    return 0;
}

OSStatus
SSLGetConnection			(SSLContextRef		ctx,
							 SSLConnectionRef	*connection)
{
	if((ctx == NULL) || (connection == NULL)) {
		return errSecParam;
	}
	*connection = ctx->ioCtx.ioRef;
	return errSecSuccess;
}

OSStatus
SSLSetPeerDomainName		(SSLContextRef		ctx,
							 const char			*peerName,
							 size_t				peerNameLen)
{
	if(ctx == NULL) {
		return errSecParam;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return errSecBadReq;
	}

    if(ctx->protocolSide == kSSLClientSide) {
        return tls_handshake_set_peer_hostname(ctx->hdsk, peerName, peerNameLen);
    } else {
        return 0; // This should probably return an error, but historically didnt.
    }
}

/*
 * Determine the buffer size needed for SSLGetPeerDomainName().
 */
OSStatus
SSLGetPeerDomainNameLength	(SSLContextRef		ctx,
							 size_t				*peerNameLen)	// RETURNED
{
	if(ctx == NULL) {
		return errSecParam;
	}
    const char *hostname;

    return tls_handshake_get_peer_hostname(ctx->hdsk, &hostname, peerNameLen);
}

OSStatus
SSLGetPeerDomainName		(SSLContextRef		ctx,
							 char				*peerName,		// returned here
							 size_t				*peerNameLen)	// IN/OUT
{
    const char *hostname;
    size_t len;

    int err;

	if(ctx == NULL) {
		return errSecParam;
	}

    err=tls_handshake_get_peer_hostname(ctx->hdsk, &hostname, &len);

    if(err) {
        return err;
    } else if(*peerNameLen<len) {
        return errSSLBufferOverflow;
    } else {
        memcpy(peerName, hostname, len);
        *peerNameLen = len;
        return 0;
    }
}

OSStatus
SSLCopyRequestedPeerNameLength	(SSLContextRef		ctx,
                                 size_t				*peerNameLen)	// RETURNED
{
    const tls_buffer *hostname;
    if(ctx == NULL) {
        return errSecParam;
    }
    hostname = tls_handshake_get_sni_hostname(ctx->hdsk);
    if(!hostname) {
        return errSecParam;
    } else {
        *peerNameLen = hostname->length;
    }
    return 0;
}

OSStatus
SSLCopyRequestedPeerName    (SSLContextRef		ctx,
                             char				*peerName,		// returned here
                             size_t				*peerNameLen)	// IN/OUT
{
    const tls_buffer *hostname;

    if(ctx == NULL) {
        return errSecParam;
    }

    hostname = tls_handshake_get_sni_hostname(ctx->hdsk);

    if(!hostname) {
        return errSecParam;
    } else if(*peerNameLen < hostname->length) {
        return errSSLBufferOverflow;
    } else {
        memcpy(peerName, hostname->data, hostname->length);
        *peerNameLen = hostname->length;
        return 0;
    }

}

OSStatus
SSLSetDatagramHelloCookie   (SSLContextRef	ctx,
                             const void         *cookie,
                             size_t             cookieLen)
{
    OSStatus err;

    if(ctx == NULL) {
		return errSecParam;
	}

    if(!ctx->isDTLS) return errSecParam;

	if((ctx == NULL) || (cookieLen>32)) {
		return errSecParam;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return errSecBadReq;
	}

	/* free possible existing cookie */
	if(ctx->dtlsCookie.data) {
        SSLFreeBuffer(&ctx->dtlsCookie);
	}

	/* copy in */
    if((err=SSLAllocBuffer(&ctx->dtlsCookie, cookieLen)))
       return err;

	memmove(ctx->dtlsCookie.data, cookie, cookieLen);
    return errSecSuccess;
}

OSStatus
SSLSetMaxDatagramRecordSize (SSLContextRef		ctx,
                             size_t             maxSize)
{

    if(ctx == NULL) return errSecParam;
    if(!ctx->isDTLS) return errSecParam;

    tls_handshake_set_mtu(ctx->hdsk, maxSize);

    return errSecSuccess;
}

OSStatus
SSLGetMaxDatagramRecordSize (SSLContextRef		ctx,
                             size_t             *maxSize)
{
    if(ctx == NULL) return errSecParam;
    if(!ctx->isDTLS) return errSecParam;

    *maxSize = ctx->mtu;

    return errSecSuccess;
}

/*

 Keys to to math below:

 A DTLS record looks like this: | header (13 bytes) | fragment |

 For Null cipher, fragment is clear text as follows:
 | Contents | Mac |

 For block cipher, fragment size must be a multiple of the cipher block size, and is the
 encryption of the following plaintext :
 | IV (1 block) | content | MAC | padding (0 to 255 bytes) | Padlen (1 byte) |

 The maximum content length in that case is achieved for 0 padding bytes.

*/

OSStatus
SSLGetDatagramWriteSize		(SSLContextRef ctx,
							 size_t *bufSize)
{
    if(ctx == NULL) return errSecParam;
    if(!ctx->isDTLS) return errSecParam;
    if(bufSize == NULL) return errSecParam;

    size_t max_fragment_size = ctx->mtu-13; /* 13 = dtls record header */

#if 0
    SSLCipherSpecParams *currCipher = &ctx->selectedCipherSpecParams;

    size_t blockSize = currCipher->blockSize;
    size_t macSize = currCipher->macSize;
#else
    size_t blockSize = 16;
    size_t macSize = 32;
#endif

    if (blockSize > 0) {
        /* max_fragment_size must be a multiple of blocksize */
        max_fragment_size = max_fragment_size & ~(blockSize-1);
        max_fragment_size -= blockSize; /* 1 block for IV */
        max_fragment_size -= 1; /* 1 byte for pad length */
    }

    /* less the mac size */
    max_fragment_size -= macSize;

    /* Thats just a sanity check */
    assert(max_fragment_size<ctx->mtu);

    *bufSize = max_fragment_size;

    return errSecSuccess;
}

/* 
 All the complexity around protocol version is to support legacy APIs.
 Eventually that should go away:
 <rdar://problem/20842025> Remove deprecated SecureTransport function related to protocol version selection.
 */

static tls_protocol_version SSLProtocolToProtocolVersion(SSLProtocol protocol) {
    switch (protocol) {
        case kSSLProtocol2:             return SSL_Version_2_0;
        case kSSLProtocol3:             return tls_protocol_version_SSL_3;
        case kTLSProtocol1:             return tls_protocol_version_TLS_1_0;
        case kTLSProtocol11:            return tls_protocol_version_TLS_1_1;
        case kTLSProtocol12:            return tls_protocol_version_TLS_1_2;
        case kDTLSProtocol1:            return tls_protocol_version_DTLS_1_0;
        default:                        return tls_protocol_version_Undertermined;
    }
}

/* concert between private SSLProtocolVersion and public SSLProtocol */
static SSLProtocol SSLProtocolVersionToProtocol(SSLProtocolVersion version)
{
	switch(version) {
		case tls_protocol_version_SSL_3:     return kSSLProtocol3;
		case tls_protocol_version_TLS_1_0:   return kTLSProtocol1;
		case tls_protocol_version_TLS_1_1:   return kTLSProtocol11;
		case tls_protocol_version_TLS_1_2:   return kTLSProtocol12;
		case tls_protocol_version_DTLS_1_0:  return kDTLSProtocol1;
		default:
			sslErrorLog("SSLProtocolVersionToProtocol: bad prot (%04x)\n",
                        version);
            /* DROPTHROUGH */
		case tls_protocol_version_Undertermined:  return kSSLProtocolUnknown;
	}
}

OSStatus
SSLSetProtocolVersionMin  (SSLContextRef      ctx,
                           SSLProtocol        minVersion)
{
    if(ctx == NULL) return errSecParam;

    tls_protocol_version version = SSLProtocolToProtocolVersion(minVersion);
    if (ctx->isDTLS) {
        if (version > MINIMUM_DATAGRAM_VERSION ||
            version < MAXIMUM_DATAGRAM_VERSION)
            return errSSLIllegalParam;
        if (version < ctx->maxProtocolVersion)
            ctx->maxProtocolVersion = version;
    } else {
        if (version < MINIMUM_STREAM_VERSION || version > MAXIMUM_STREAM_VERSION)
            return errSSLIllegalParam;
        if (version > ctx->maxProtocolVersion)
            ctx->maxProtocolVersion = version;
    }
    ctx->minProtocolVersion = version;

    tls_handshake_set_min_protocol_version(ctx->hdsk, ctx->minProtocolVersion);
    tls_handshake_set_max_protocol_version(ctx->hdsk, ctx->maxProtocolVersion);

    return errSecSuccess;
}

OSStatus
SSLGetProtocolVersionMin  (SSLContextRef      ctx,
                           SSLProtocol        *minVersion)
{
    if(ctx == NULL) return errSecParam;

    *minVersion = SSLProtocolVersionToProtocol(ctx->minProtocolVersion);
    return errSecSuccess;
}

OSStatus
SSLSetProtocolVersionMax  (SSLContextRef      ctx,
                           SSLProtocol        maxVersion)
{
    if(ctx == NULL) return errSecParam;

    SSLProtocolVersion version = SSLProtocolToProtocolVersion(maxVersion);
    if (ctx->isDTLS) {
        if (version > MINIMUM_DATAGRAM_VERSION ||
            version < MAXIMUM_DATAGRAM_VERSION)
            return errSSLIllegalParam;
        if (version > ctx->minProtocolVersion)
            ctx->minProtocolVersion = version;
    } else {
        if (version < MINIMUM_STREAM_VERSION || version > MAXIMUM_STREAM_VERSION)
            return errSSLIllegalParam;
        if (version < ctx->minProtocolVersion)
            ctx->minProtocolVersion = version;
    }
    ctx->maxProtocolVersion = version;

    tls_handshake_set_min_protocol_version(ctx->hdsk, ctx->minProtocolVersion);
    tls_handshake_set_max_protocol_version(ctx->hdsk, ctx->maxProtocolVersion);

    return errSecSuccess;
}

OSStatus
SSLGetProtocolVersionMax  (SSLContextRef      ctx,
                           SSLProtocol        *maxVersion)
{
    if(ctx == NULL) return errSecParam;

    *maxVersion = SSLProtocolVersionToProtocol(ctx->maxProtocolVersion);
    return errSecSuccess;
}

#define max(x,y) ((x)<(y)?(y):(x))

OSStatus
SSLSetProtocolVersionEnabled(SSLContextRef     ctx,
							 SSLProtocol		protocol,
							 Boolean			enable)
{
	if(ctx == NULL) {
		return errSecParam;
	}
	if(sslIsSessionActive(ctx) || ctx->isDTLS) {
		/* Can't do this with an active session, nor with a DTLS session */
		return errSecBadReq;
	}
    if (protocol == kSSLProtocolAll) {
        if (enable) {
            ctx->minProtocolVersion = MINIMUM_STREAM_VERSION;
            ctx->maxProtocolVersion = MAXIMUM_STREAM_VERSION;
        } else {
            ctx->minProtocolVersion = SSL_Version_Undetermined;
            ctx->maxProtocolVersion = SSL_Version_Undetermined;
        }
	} else {
		SSLProtocolVersion version = SSLProtocolToProtocolVersion(protocol);
        if (enable) {
			if (version < MINIMUM_STREAM_VERSION || version > MAXIMUM_STREAM_VERSION) {
				return errSecParam;
			}
            if (version > ctx->maxProtocolVersion) {
                ctx->maxProtocolVersion = version;
                if (ctx->minProtocolVersion == SSL_Version_Undetermined)
                    ctx->minProtocolVersion = version;
            }
            if (version < ctx->minProtocolVersion) {
                ctx->minProtocolVersion = version;
            }
        } else {
			if (version < SSL_Version_2_0 || version > MAXIMUM_STREAM_VERSION) {
				return errSecParam;
			}
			/* Disabling a protocol version now resets the minimum acceptable
			 * version to the next higher version. This means it's no longer
			 * possible to enable a discontiguous set of protocol versions.
			 */
			SSLProtocolVersion nextVersion;
			switch (version) {
				case SSL_Version_2_0:
					nextVersion = SSL_Version_3_0;
					break;
				case SSL_Version_3_0:
					nextVersion = TLS_Version_1_0;
					break;
				case TLS_Version_1_0:
					nextVersion = TLS_Version_1_1;
					break;
				case TLS_Version_1_1:
					nextVersion = TLS_Version_1_2;
					break;
				case TLS_Version_1_2:
				default:
					nextVersion = SSL_Version_Undetermined;
					break;
			}
			ctx->minProtocolVersion = max(ctx->minProtocolVersion, nextVersion);
			if (ctx->minProtocolVersion > ctx->maxProtocolVersion) {
				ctx->minProtocolVersion = SSL_Version_Undetermined;
				ctx->maxProtocolVersion = SSL_Version_Undetermined;
			}
        }
    }

    tls_handshake_set_min_protocol_version(ctx->hdsk, ctx->minProtocolVersion);
    tls_handshake_set_max_protocol_version(ctx->hdsk, ctx->maxProtocolVersion);

	return errSecSuccess;
}

OSStatus
SSLGetProtocolVersionEnabled(SSLContextRef 		ctx,
							 SSLProtocol		protocol,
							 Boolean			*enable)		/* RETURNED */
{
	if(ctx == NULL) {
		return errSecParam;
	}
	if(ctx->isDTLS) {
		/* Can't do this with a DTLS session */
		return errSecBadReq;
	}
	switch(protocol) {
		case kSSLProtocol2:
		case kSSLProtocol3:
		case kTLSProtocol1:
        case kTLSProtocol11:
        case kTLSProtocol12:
        {
            SSLProtocolVersion version = SSLProtocolToProtocolVersion(protocol);
			*enable = (ctx->minProtocolVersion <= version
                       && ctx->maxProtocolVersion >= version);
			break;
        }
		case kSSLProtocolAll:
            *enable = (ctx->minProtocolVersion <= MINIMUM_STREAM_VERSION
                       && ctx->maxProtocolVersion >= MAXIMUM_STREAM_VERSION);
			break;
		default:
			return errSecParam;
	}
	return errSecSuccess;
}

/* deprecated */
OSStatus
SSLSetProtocolVersion		(SSLContextRef 		ctx,
							 SSLProtocol		version)
{
	if(ctx == NULL) {
		return errSecParam;
	}
	if(sslIsSessionActive(ctx) || ctx->isDTLS) {
		/* Can't do this with an active session, nor with a DTLS session */
		return errSecBadReq;
	}

	switch(version) {
		case kSSLProtocol3:
			/* this tells us to do our best, up to 3.0 */
            ctx->minProtocolVersion = MINIMUM_STREAM_VERSION;
            ctx->maxProtocolVersion = SSL_Version_3_0;
			break;
		case kSSLProtocol3Only:
            ctx->minProtocolVersion = SSL_Version_3_0;
            ctx->maxProtocolVersion = SSL_Version_3_0;
			break;
		case kTLSProtocol1:
			/* this tells us to do our best, up to TLS, but allows 3.0 */
            ctx->minProtocolVersion = MINIMUM_STREAM_VERSION;
            ctx->maxProtocolVersion = TLS_Version_1_0;
            break;
        case kTLSProtocol1Only:
            ctx->minProtocolVersion = TLS_Version_1_0;
            ctx->maxProtocolVersion = TLS_Version_1_0;
			break;
        case kTLSProtocol11:
			/* This tells us to do our best, up to TLS 1.1, currently also
               allows 3.0 or TLS 1.0 */
            ctx->minProtocolVersion = MINIMUM_STREAM_VERSION;
            ctx->maxProtocolVersion = TLS_Version_1_1;
			break;
        case kTLSProtocol12:
        case kSSLProtocolAll:
		case kSSLProtocolUnknown:
			/* This tells us to do our best, up to TLS 1.2, currently also
               allows 3.0 or TLS 1.0 or TLS 1.1 */
            ctx->minProtocolVersion = MINIMUM_STREAM_VERSION;
            ctx->maxProtocolVersion = MAXIMUM_STREAM_VERSION;
			break;
		default:
			return errSecParam;
	}

    tls_handshake_set_min_protocol_version(ctx->hdsk, ctx->minProtocolVersion);
    tls_handshake_set_max_protocol_version(ctx->hdsk, ctx->maxProtocolVersion);

    return errSecSuccess;
}

/* deprecated */
OSStatus
SSLGetProtocolVersion		(SSLContextRef		ctx,
							 SSLProtocol		*protocol)		/* RETURNED */
{
	if(ctx == NULL) {
		return errSecParam;
	}
	/* translate array of booleans to public value; not all combinations
	 * are legal (i.e., meaningful) for this call */
    if (ctx->maxProtocolVersion == MAXIMUM_STREAM_VERSION) {
        if(ctx->minProtocolVersion == MINIMUM_STREAM_VERSION) {
            /* traditional 'all enabled' */
            *protocol = kSSLProtocolAll;
            return errSecSuccess;
		}
	} else if (ctx->maxProtocolVersion == TLS_Version_1_1) {
        if(ctx->minProtocolVersion == MINIMUM_STREAM_VERSION) {
            /* traditional 'all enabled' */
            *protocol = kTLSProtocol11;
            return errSecSuccess;
        }
	} else if (ctx->maxProtocolVersion == TLS_Version_1_0) {
        if(ctx->minProtocolVersion == MINIMUM_STREAM_VERSION) {
            /* TLS1.1 and below enabled */
            *protocol = kTLSProtocol1;
            return errSecSuccess;
        } else if(ctx->minProtocolVersion == TLS_Version_1_0) {
        	*protocol = kTLSProtocol1Only;
		}
	} else if(ctx->maxProtocolVersion == SSL_Version_3_0) {
        if(ctx->minProtocolVersion == MINIMUM_STREAM_VERSION) {
            /* Could also return kSSLProtocol3Only since
               MINIMUM_STREAM_VERSION == SSL_Version_3_0. */
            *protocol = kSSLProtocol3;
			return errSecSuccess;
		}
	}

    return errSecParam;
}

OSStatus
SSLGetNegotiatedProtocolVersion		(SSLContextRef		ctx,
									 SSLProtocol		*protocol) /* RETURNED */
{
	if(ctx == NULL) {
		return errSecParam;
	}
	*protocol = SSLProtocolVersionToProtocol(ctx->negProtocolVersion);
	return errSecSuccess;
}

OSStatus
SSLSetEnableCertVerify		(SSLContextRef		ctx,
							 Boolean			enableVerify)
{
	if(ctx == NULL) {
		return errSecParam;
	}
	sslCertDebug("SSLSetEnableCertVerify %s",
		enableVerify ? "true" : "false");
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return errSecBadReq;
	}
	ctx->enableCertVerify = enableVerify;
	return errSecSuccess;
}

OSStatus
SSLGetEnableCertVerify		(SSLContextRef		ctx,
							Boolean				*enableVerify)
{
	if(ctx == NULL) {
		return errSecParam;
	}
	*enableVerify = ctx->enableCertVerify;
	return errSecSuccess;
}

OSStatus
SSLSetAllowsExpiredCerts(SSLContextRef		ctx,
						 Boolean			allowExpired)
{
    /* This has been deprecated since 10.9, and non-functional since at least 10.10 */
    return 0;
}

OSStatus
SSLGetAllowsExpiredCerts	(SSLContextRef		ctx,
							 Boolean			*allowExpired)
{
    /* This has been deprecated since 10.9, and non-functional since at least 10.10 */
    return errSecUnimplemented;
}

OSStatus
SSLSetAllowsExpiredRoots(SSLContextRef		ctx,
						 Boolean			allowExpired)
{
    /* This has been deprecated since 10.9, and non-functional since at least 10.10 */
    return 0;
}

OSStatus
SSLGetAllowsExpiredRoots	(SSLContextRef		ctx,
							 Boolean			*allowExpired)
{
    /* This has been deprecated since 10.9, and non-functional since at least 10.10 */
    return errSecUnimplemented;
}

OSStatus SSLSetAllowsAnyRoot(
	SSLContextRef	ctx,
	Boolean			anyRoot)
{
	if(ctx == NULL) {
		return errSecParam;
	}
	sslCertDebug("SSLSetAllowsAnyRoot %s",	anyRoot ? "true" : "false");
	ctx->allowAnyRoot = anyRoot;
	return errSecSuccess;
}

OSStatus
SSLGetAllowsAnyRoot(
	SSLContextRef	ctx,
	Boolean			*anyRoot)
{
	if(ctx == NULL) {
		return errSecParam;
	}
	*anyRoot = ctx->allowAnyRoot;
	return errSecSuccess;
}

#if !TARGET_OS_IPHONE
/* obtain the system roots sets for this app, policy SSL */
static OSStatus sslDefaultSystemRoots(
	SSLContextRef ctx,
	CFArrayRef *systemRoots)				// created and RETURNED

{
    const char *hostname;
    size_t len;

    tls_handshake_get_peer_hostname(ctx->hdsk, &hostname, &len);

	return SecTrustSettingsCopyQualifiedCerts(&CSSMOID_APPLE_TP_SSL,
		hostname,
		(uint32_t)len,
		(ctx->protocolSide == kSSLServerSide) ?
			/* server verifies, client encrypts */
			CSSM_KEYUSE_VERIFY : CSSM_KEYUSE_ENCRYPT,
		systemRoots);
}
#endif /* OS X only */

OSStatus
SSLSetTrustedRoots			(SSLContextRef 		ctx,
							 CFArrayRef 		trustedRoots,
							 Boolean 			replaceExisting)
{
	if (sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return errSecBadReq;
	}
	sslCertDebug("SSLSetTrustedRoot  numCerts %d  replaceExist %s",
		(int)CFArrayGetCount(trustedRoots), replaceExisting ? "true" : "false");

    if (replaceExisting) {
        ctx->trustedCertsOnly = true;
        CFReleaseNull(ctx->trustedCerts);
    }

    if (ctx->trustedCerts) {
        CFIndex count = CFArrayGetCount(trustedRoots);
        CFRange range = { 0, count };
        CFArrayAppendArray(ctx->trustedCerts, trustedRoots, range);
    } else {
        require(ctx->trustedCerts =
            CFArrayCreateMutableCopy(kCFAllocatorDefault, 0, trustedRoots),
            errOut);
    }

    return errSecSuccess;

errOut:
    return errSecAllocate;
}

OSStatus
SSLCopyTrustedRoots			(SSLContextRef 		ctx,
							 CFArrayRef 		*trustedRoots)	/* RETURNED */
{
	if(ctx == NULL || trustedRoots == NULL) {
		return errSecParam;
	}
	if(ctx->trustedCerts != NULL) {
		*trustedRoots = ctx->trustedCerts;
		CFRetain(ctx->trustedCerts);
		return errSecSuccess;
	}
#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
	/* use default system roots */
    return sslDefaultSystemRoots(ctx, trustedRoots);
#else
    *trustedRoots = NULL;
    return errSecSuccess;
#endif
}

#if !TARGET_OS_IPHONE
OSStatus
SSLSetTrustedLeafCertificates	(SSLContextRef 		ctx,
								 CFArrayRef 		trustedCerts)
{
	if(ctx == NULL) {
		return errSecParam;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return errSecBadReq;
	}

	if(ctx->trustedLeafCerts) {
		CFRelease(ctx->trustedLeafCerts);
	}
	ctx->trustedLeafCerts = CFRetainSafe(trustedCerts);
	return errSecSuccess;
}

OSStatus
SSLCopyTrustedLeafCertificates	(SSLContextRef 		ctx,
								 CFArrayRef 		*trustedCerts)	/* RETURNED */
{
	if(ctx == NULL) {
		return errSecParam;
	}
	if(ctx->trustedLeafCerts != NULL) {
		*trustedCerts = ctx->trustedLeafCerts;
		CFRetain(ctx->trustedCerts);
		return errSecSuccess;
	}
	*trustedCerts = NULL;
	return errSecSuccess;
}
#endif

OSStatus
SSLSetClientSideAuthenticate 	(SSLContext			*ctx,
								 SSLAuthenticate	auth)
{
	if(ctx == NULL) {
		return errSecParam;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return errSecBadReq;
	}
	ctx->clientAuth = auth;
	switch(auth) {
		case kNeverAuthenticate:
            tls_handshake_set_client_auth(ctx->hdsk, false);
			break;
		case kAlwaysAuthenticate:
		case kTryAuthenticate:
            tls_handshake_set_client_auth(ctx->hdsk, true);
			break;
	}
	return errSecSuccess;
}

OSStatus
SSLGetClientSideAuthenticate 	(SSLContext			*ctx,
								 SSLAuthenticate	*auth)	/* RETURNED */
{
	if(ctx == NULL || auth == NULL) {
		return errSecParam;
	}
	*auth = ctx->clientAuth;
	return errSecSuccess;
}

OSStatus
SSLGetClientCertificateState	(SSLContextRef				ctx,
								 SSLClientCertificateState	*clientState)
{
	if(ctx == NULL) {
		return errSecParam;
	}
    if(ctx->protocolSide == kSSLClientSide) {
        /* Client Side */
       switch(ctx->clientCertState) {
           case kSSLClientCertNone:
               *clientState = kSSLClientCertNone;
               break;
           case kSSLClientCertRequested:
               if(ctx->localCertArray) {
                   *clientState = kSSLClientCertSent;
               } else {
                   *clientState = kSSLClientCertRequested;
               }
               break;
           default:
               /* Anything else is an internal error */
               sslErrorLog("TLS client has invalid internal clientCertState (%d)\n", ctx->clientCertState);
               return errSSLInternal;
       }
    } else {
        /* Server side */
        switch(ctx->clientCertState) {
            case kSSLClientCertNone:
            case kSSLClientCertRejected:
                *clientState = ctx->clientCertState;
                break;
            case kSSLClientCertRequested:
                if(ctx->peerSecTrust) {
                    *clientState = kSSLClientCertSent;
                } else {
                    *clientState = kSSLClientCertRequested;
                }
                break;
            default:
                /* Anything else is an internal error */
                sslErrorLog("TLS server has invalid internal clientCertState (%d)\n", ctx->clientCertState);
                return errSSLInternal;
        }
    }
	return errSecSuccess;
}

#include <tls_helpers.h>

OSStatus
SSLSetCertificate			(SSLContextRef		ctx,
							 CFArrayRef			_Nullable certRefs)
{
    OSStatus ortn;
	/*
	 * -- free localCerts if we have any
	 * -- Get raw cert data, convert to ctx->localCert
	 * -- get pub, priv keys from certRef[0]
	 * -- validate cert chain
	 */
	if(ctx == NULL) {
		return errSecParam;
	}

    CFReleaseNull(ctx->localCertArray);
	if(certRefs == NULL) {
		return errSecSuccess; // we have cleared the cert, as requested
	}

    ortn = tls_helper_set_identity_from_array(ctx->hdsk, certRefs);

    if(ortn == noErr) {
        ctx->localCertArray = certRefs;
        CFRetain(certRefs);
    }

	return ortn;
}

OSStatus
SSLSetEncryptionCertificate	(SSLContextRef		ctx,
							 CFArrayRef			certRefs)
{
	if(ctx == NULL) {
		return errSecParam;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return errSecBadReq;
	}
    CFReleaseNull(ctx->encryptCertArray);
    ctx->encryptCertArray = certRefs;
    CFRetain(certRefs);
	return errSecSuccess;
}

OSStatus SSLGetCertificate(SSLContextRef		ctx,
						   CFArrayRef			*certRefs)
{
	if(ctx == NULL) {
		return errSecParam;
	}
	*certRefs = ctx->localCertArray;
	return errSecSuccess;
}

OSStatus SSLGetEncryptionCertificate(SSLContextRef		ctx,
								     CFArrayRef			*certRefs)
{
	if(ctx == NULL) {
		return errSecParam;
	}
	*certRefs = ctx->encryptCertArray;
	return errSecSuccess;
}

OSStatus
SSLSetPeerID				(SSLContext 		*ctx,
							 const void 		*peerID,
							 size_t				peerIDLen)
{
	OSStatus serr;

	/* copy peerId to context->peerId */
	if((ctx == NULL) ||
	   (peerID == NULL) ||
	   (peerIDLen == 0)) {
		return errSecParam;
	}
	if(sslIsSessionActive(ctx) &&
        /* kSSLClientCertRequested implies client side */
        (ctx->clientCertState != kSSLClientCertRequested))
    {
		return errSecBadReq;
	}
	SSLFreeBuffer(&ctx->peerID);
	serr = SSLAllocBuffer(&ctx->peerID, peerIDLen);
	if(serr) {
		return serr;
	}
    tls_handshake_set_resumption(ctx->hdsk, true);
	memmove(ctx->peerID.data, peerID, peerIDLen);
	return errSecSuccess;
}

OSStatus
SSLGetPeerID				(SSLContextRef 		ctx,
							 const void 		**peerID,
							 size_t				*peerIDLen)
{
	*peerID = ctx->peerID.data;			// may be NULL
	*peerIDLen = ctx->peerID.length;
	return errSecSuccess;
}

OSStatus
SSLGetNegotiatedCipher		(SSLContextRef 		ctx,
							 SSLCipherSuite 	*cipherSuite)
{
	if(ctx == NULL) {
		return errSecParam;
	}

    if(!sslIsSessionActive(ctx)) {
		return errSecBadReq;
	}

    *cipherSuite = (SSLCipherSuite)tls_handshake_get_negotiated_cipherspec(ctx->hdsk);

	return errSecSuccess;
}

/*
 * Add an acceptable distinguished name (client authentication only).
 */
OSStatus
SSLAddDistinguishedName(
	SSLContextRef ctx,
	const void *derDN,
	size_t derDNLen)
{
    DNListElem      *dn;
    OSStatus        err;

	if(ctx == NULL) {
		return errSecParam;
	}
	if(sslIsSessionActive(ctx)) {
		return errSecBadReq;
	}

	dn = (DNListElem *)sslMalloc(sizeof(DNListElem));
	if(dn == NULL) {
		return errSecAllocate;
	}
    if ((err = SSLAllocBuffer(&dn->derDN, derDNLen)))
    {
        sslFree(dn);
        return err;
    }
    memcpy(dn->derDN.data, derDN, derDNLen);
    dn->next = ctx->acceptableDNList;
    ctx->acceptableDNList = dn;

    tls_handshake_set_acceptable_dn_list(ctx->hdsk, dn);

    return errSecSuccess;
}

/* single-cert version of SSLSetCertificateAuthorities() */
static OSStatus
sslAddCA(SSLContextRef		ctx,
		 SecCertificateRef	cert)
{
	OSStatus ortn = errSecParam;

    /* Get subject from certificate. */
#if TARGET_OS_IPHONE
    CFDataRef subjectName = NULL;
    subjectName = SecCertificateCopySubjectSequence(cert);
    require(subjectName, errOut);
#else
    CSSM_DATA_PTR subjectName = NULL;
    ortn = SecCertificateCopyFirstFieldValue(cert, &CSSMOID_X509V1SubjectNameStd, &subjectName);
    require_noerr(ortn, errOut);
#endif

	/* add to acceptableCAs as cert, creating array if necessary */
	if(ctx->acceptableCAs == NULL) {
		require(ctx->acceptableCAs = CFArrayCreateMutable(NULL, 0,
            &kCFTypeArrayCallBacks), errOut);
		if(ctx->acceptableCAs == NULL) {
			return errSecAllocate;
		}
	}
	CFArrayAppendValue(ctx->acceptableCAs, cert);

	/* then add this cert's subject name to acceptableDNList */
#if TARGET_OS_IPHONE
	ortn = SSLAddDistinguishedName(ctx,
                                   CFDataGetBytePtr(subjectName),
                                   CFDataGetLength(subjectName));
#else
    ortn = SSLAddDistinguishedName(ctx, subjectName->Data, subjectName->Length);
#endif

errOut:
#if TARGET_OS_IPHONE
    CFReleaseSafe(subjectName);
#endif
	return ortn;
}

/*
 * Add a SecCertificateRef, or a CFArray of them, to a server's list
 * of acceptable Certificate Authorities (CAs) to present to the client
 * when client authentication is performed.
 */
OSStatus
SSLSetCertificateAuthorities(SSLContextRef		ctx,
							 CFTypeRef			certificateOrArray,
							 Boolean 			replaceExisting)
{
	CFTypeID itemType;
	OSStatus ortn = errSecSuccess;

	if((ctx == NULL) || sslIsSessionActive(ctx) ||
	   (ctx->protocolSide != kSSLServerSide)) {
		return errSecParam;
	}
	if(replaceExisting) {
		sslFreeDnList(ctx);
		if(ctx->acceptableCAs) {
			CFRelease(ctx->acceptableCAs);
			ctx->acceptableCAs = NULL;
		}
	}
	/* else appending */

	itemType = CFGetTypeID(certificateOrArray);
	if(itemType == SecCertificateGetTypeID()) {
		/* one cert */
		ortn = sslAddCA(ctx, (SecCertificateRef)certificateOrArray);
	}
	else if(itemType == CFArrayGetTypeID()) {
		CFArrayRef cfa = (CFArrayRef)certificateOrArray;
		CFIndex numCerts = CFArrayGetCount(cfa);
		CFIndex dex;

		/* array of certs */
		for(dex=0; dex<numCerts; dex++) {
			SecCertificateRef cert = (SecCertificateRef)CFArrayGetValueAtIndex(cfa, dex);
			if(CFGetTypeID(cert) != SecCertificateGetTypeID()) {
				return errSecParam;
			}
			ortn = sslAddCA(ctx, cert);
			if(ortn) {
				break;
			}
		}
	}
	else {
		ortn = errSecParam;
	}
	return ortn;
}


/*
 * Obtain the certificates specified in SSLSetCertificateAuthorities(),
 * if any. Returns a NULL array if SSLSetCertificateAuthorities() has not
 * been called.
 * Caller must CFRelease the returned array.
 */
OSStatus
SSLCopyCertificateAuthorities(SSLContextRef		ctx,
							  CFArrayRef		*certificates)	/* RETURNED */
{
	if((ctx == NULL) || (certificates == NULL)) {
		return errSecParam;
	}
	if(ctx->acceptableCAs == NULL) {
		*certificates = NULL;
		return errSecSuccess;
	}
	*certificates = ctx->acceptableCAs;
	CFRetain(ctx->acceptableCAs);
	return errSecSuccess;
}


/*
 * Obtain the list of acceptable distinguished names as provided by
 * a server (if the SSLCotextRef is configured as a client), or as
 * specified by SSLSetCertificateAuthorities() (if the SSLContextRef
 * is configured as a server).
  */
OSStatus
SSLCopyDistinguishedNames	(SSLContextRef		ctx,
							 CFArrayRef			*names)
{
	CFMutableArrayRef outArray = NULL;
	const DNListElem *dn;

	if((ctx == NULL) || (names == NULL)) {
		return errSecParam;
	}
    if(ctx->protocolSide==kSSLServerSide) {
        dn = ctx->acceptableDNList;
    } else {
        dn = tls_handshake_get_peer_acceptable_dn_list(ctx->hdsk); // ctx->acceptableDNList;
    }

	if(dn == NULL) {
		*names = NULL;
		return errSecSuccess;
	}
	outArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	while (dn) {
		CFDataRef cfDn = CFDataCreate(NULL, dn->derDN.data, dn->derDN.length);
		CFArrayAppendValue(outArray, cfDn);
		CFRelease(cfDn);
		dn = dn->next;
	}
	*names = outArray;
	return errSecSuccess;
}


/*
 * Request peer certificates. Valid anytime, subsequent to
 * a handshake attempt.
 */
OSStatus
SSLCopyPeerCertificates	(SSLContextRef ctx, CFArrayRef *certs)
{
	if(ctx == NULL) {
		return errSecParam;
	}

	if (!ctx->peerSecTrust) {
		*certs = NULL;
		return errSecBadReq;
	}

    CFIndex count = SecTrustGetCertificateCount(ctx->peerSecTrust);
    CFMutableArrayRef ca = CFArrayCreateMutable(kCFAllocatorDefault, count, &kCFTypeArrayCallBacks);
    if (ca == NULL) {
        return errSecAllocate;
    }

    for (CFIndex ix = 0; ix < count; ++ix) {
        CFArrayAppendValue(ca, SecTrustGetCertificateAtIndex(ctx->peerSecTrust, ix));
    }

    *certs = ca;
    
	return errSecSuccess;
}

#if !TARGET_OS_IPHONE
// Permanently removing from iOS, keep for OSX (deprecated), removed from headers.
// <rdar://problem/14215831> Mailsmith Crashes While Getting New Mail Under Mavericks Developer Preview
OSStatus
SSLGetPeerCertificates (SSLContextRef ctx,
                        CFArrayRef *certs);
OSStatus
SSLGetPeerCertificates (SSLContextRef ctx,
                        CFArrayRef *certs)
{
    return errSecUnimplemented;
}
#endif

/*
 * Specify Diffie-Hellman parameters. Optional; if we are configured to allow
 * for D-H ciphers and a D-H cipher is negotiated, and this function has not
 * been called, a set of process-wide parameters will be calculated. However
 * that can take a long time (30 seconds).
 */
OSStatus SSLSetDiffieHellmanParams(
	SSLContextRef	ctx,
	const void 		*dhParams,
	size_t			dhParamsLen)
{
#if APPLE_DH
	if(ctx == NULL) {
		return errSecParam;
	}
	if(sslIsSessionActive(ctx)) {
		return errSecBadReq;
	}
	SSLFreeBuffer(&ctx->dhParamsEncoded);

	OSStatus ortn;
	ortn = SSLCopyBufferFromData(dhParams, dhParamsLen,
		&ctx->dhParamsEncoded);

    if(ortn) {
        return ortn;
    } else {
        return tls_handshake_set_dh_parameters(ctx->hdsk, &ctx->dhParamsEncoded);
    }

#endif /* APPLE_DH */
}

/*
 * Return parameter block specified in SSLSetDiffieHellmanParams.
 * Returned data is not copied and belongs to the SSLContextRef.
 */
OSStatus SSLGetDiffieHellmanParams(
	SSLContextRef	ctx,
	const void 		**dhParams,
	size_t			*dhParamsLen)
{
#if APPLE_DH
	if(ctx == NULL) {
		return errSecParam;
	}
	*dhParams = ctx->dhParamsEncoded.data;
	*dhParamsLen = ctx->dhParamsEncoded.length;
	return errSecSuccess;
#else
    return errSecUnimplemented;
#endif /* APPLE_DH */
}

OSStatus SSLSetDHEEnabled(SSLContextRef ctx, bool enabled)
{
    ctx->dheEnabled = enabled;
    /* Hack a little so that only the ciphersuites change */
    tls_protocol_version min, max;
    unsigned nbits;
    tls_handshake_get_min_protocol_version(ctx->hdsk, &min);
    tls_handshake_get_max_protocol_version(ctx->hdsk, &max);
    tls_handshake_get_min_dh_group_size(ctx->hdsk, &nbits);
    tls_handshake_set_config(ctx->hdsk, enabled?tls_handshake_config_legacy_DHE:tls_handshake_config_legacy);
    tls_handshake_set_min_protocol_version(ctx->hdsk, min);
    tls_handshake_set_max_protocol_version(ctx->hdsk, max);
    tls_handshake_set_min_dh_group_size(ctx->hdsk, nbits);

    return noErr;
}

OSStatus SSLGetDHEEnabled(SSLContextRef ctx, bool *enabled)
{
    *enabled = ctx->dheEnabled;
    return noErr;
}

OSStatus SSLSetMinimumDHGroupSize(SSLContextRef ctx, unsigned nbits)
{
    return tls_handshake_set_min_dh_group_size(ctx->hdsk, nbits);
}

OSStatus SSLGetMinimumDHGroupSize(SSLContextRef ctx, unsigned *nbits)
{
    return tls_handshake_get_min_dh_group_size(ctx->hdsk, nbits);
}

OSStatus SSLSetRsaBlinding(
	SSLContextRef	ctx,
	Boolean			blinding)
{
	if(ctx == NULL) {
		return errSecParam;
	}
	ctx->rsaBlindingEnable = blinding;
	return errSecSuccess;
}

OSStatus SSLGetRsaBlinding(
	SSLContextRef	ctx,
	Boolean			*blinding)
{
	if(ctx == NULL) {
		return errSecParam;
	}
	*blinding = ctx->rsaBlindingEnable;
	return errSecSuccess;
}

OSStatus
SSLCopyPeerTrust(
    SSLContextRef 		ctx,
    SecTrustRef        *trust)	/* RETURNED */
{
	OSStatus status = errSecSuccess;
	if (ctx == NULL || trust == NULL)
		return errSecParam;

	/* Create a SecTrustRef if this was a resumed session and we
	   didn't have one yet. */
	if (!ctx->peerSecTrust) {
		status = sslCreateSecTrust(ctx, &ctx->peerSecTrust);
    }

	*trust = ctx->peerSecTrust;
    if (ctx->peerSecTrust)
        CFRetain(ctx->peerSecTrust);

	return status;
}

OSStatus SSLGetPeerSecTrust(
	SSLContextRef	ctx,
	SecTrustRef		*trust)	/* RETURNED */
{
    OSStatus status = errSecSuccess;
	if (ctx == NULL || trust == NULL)
		return errSecParam;

	/* Create a SecTrustRef if this was a resumed session and we
	   didn't have one yet. */
	if (!ctx->peerSecTrust) {
		status = sslCreateSecTrust(ctx, &ctx->peerSecTrust);
    }

	*trust = ctx->peerSecTrust;
	return status;
}

OSStatus SSLInternalMasterSecret(
   SSLContextRef ctx,
   void *secret,        // mallocd by caller, SSL_MASTER_SECRET_SIZE
   size_t *secretSize)  // in/out
{
	if((ctx == NULL) || (secret == NULL) || (secretSize == NULL)) {
		return errSecParam;
	}
    return tls_handshake_internal_master_secret(ctx->hdsk, secret, secretSize);
}

OSStatus SSLInternalServerRandom(
   SSLContextRef ctx,
   void *randBuf, 			// mallocd by caller, SSL_CLIENT_SRVR_RAND_SIZE
   size_t *randSize)	// in/out
{
	if((ctx == NULL) || (randBuf == NULL) || (randSize == NULL)) {
		return errSecParam;
	}
    return tls_handshake_internal_server_random(ctx->hdsk, randBuf, randSize);
}

OSStatus SSLInternalClientRandom(
   SSLContextRef ctx,
   void *randBuf,  		// mallocd by caller, SSL_CLIENT_SRVR_RAND_SIZE
   size_t *randSize)	// in/out
{
	if((ctx == NULL) || (randBuf == NULL) || (randSize == NULL)) {
		return errSecParam;
	}
    return tls_handshake_internal_client_random(ctx->hdsk, randBuf, randSize);
}

/* This is used by EAP 802.1x */
OSStatus SSLGetCipherSizes(
	SSLContextRef ctx,
	size_t *digestSize,
	size_t *symmetricKeySize,
	size_t *ivSize)
{
	if((ctx == NULL) || (digestSize == NULL) ||
	   (symmetricKeySize == NULL) || (ivSize == NULL)) {
		return errSecParam;
	}

    SSLCipherSuite cipher=tls_handshake_get_negotiated_cipherspec(ctx->hdsk);

	*digestSize = sslCipherSuiteGetMacSize(cipher);
	*symmetricKeySize = sslCipherSuiteGetSymmetricCipherKeySize(cipher);
	*ivSize = sslCipherSuiteGetSymmetricCipherBlockIvSize(cipher);
	return errSecSuccess;
}

OSStatus
SSLGetResumableSessionInfo(
	SSLContextRef	ctx,
	Boolean			*sessionWasResumed,		// RETURNED
	void			*sessionID,				// RETURNED, mallocd by caller
	size_t			*sessionIDLength)		// IN/OUT
{
	if((ctx == NULL) || (sessionWasResumed == NULL) ||
	   (sessionID == NULL) || (sessionIDLength == NULL) ||
	   (*sessionIDLength < MAX_SESSION_ID_LENGTH)) {
		return errSecParam;
	}

    SSLBuffer localSessionID;
    bool sessionMatch = tls_handshake_get_session_match(ctx->hdsk, &localSessionID);

	if(sessionMatch) {
		*sessionWasResumed = true;
		if(localSessionID.length > *sessionIDLength) {
			/* really should never happen - means ID > 32 */
			return errSecParam;
		}
		if(localSessionID.length) {
			/*
 			 * Note PAC-based session resumption can result in sessionMatch
			 * with no sessionID
			 */
			memmove(sessionID, localSessionID.data, localSessionID.length);
		}
		*sessionIDLength = localSessionID.length;
	}
	else {
		*sessionWasResumed = false;
		*sessionIDLength = 0;
	}
	return errSecSuccess;
}

/*
 * Get/set enable of anonymous ciphers. This is deprecated and now a no-op.
 */
OSStatus
SSLSetAllowAnonymousCiphers(
	SSLContextRef	ctx,
	Boolean			enable)
{
    return errSecSuccess;
}

OSStatus
SSLGetAllowAnonymousCiphers(
	SSLContextRef	ctx,
	Boolean			*enable)
{
    return errSecSuccess;
}

/*
 * Override the default session cache timeout for a cache entry created for
 * the current session.
 */
OSStatus
SSLSetSessionCacheTimeout(
	SSLContextRef ctx,
	uint32_t timeoutInSeconds)
{
	if(ctx == NULL) {
		return errSecParam;
	}
	ctx->sessionCacheTimeout = timeoutInSeconds;
	return errSecSuccess;
}


static
void tls_handshake_master_secret_function(const void *arg,         /* opaque to coreTLS; app-specific */
                                          void *secret,			/* mallocd by caller, SSL_MASTER_SECRET_SIZE */
                                          size_t *secretLength)
{
    SSLContextRef ctx = (SSLContextRef) arg;
    ctx->masterSecretCallback(ctx, ctx->masterSecretArg, secret, secretLength);
}


/*
 * Register a callback for obtaining the master_secret when performing
 * PAC-based session resumption.
 */
OSStatus
SSLInternalSetMasterSecretFunction(
	SSLContextRef ctx,
	SSLInternalMasterSecretFunction mFunc,
	const void *arg)		/* opaque to SecureTransport; app-specific */
{
	if(ctx == NULL) {
		return errSecParam;
	}

    ctx->masterSecretArg = arg;
    ctx->masterSecretCallback = mFunc;

    return tls_handshake_internal_set_master_secret_function(ctx->hdsk, &tls_handshake_master_secret_function, ctx);
}

/*
 * Provide an opaque SessionTicket for use in PAC-based session
 * resumption. Client side only. The provided ticket is sent in
 * the ClientHello message as a SessionTicket extension.
 *
 * We won't reject this on the server side, but server-side support
 * for PAC-based session resumption is currently enabled for
 * Development builds only. To fully support this for server side,
 * besides the rudimentary support that's here for Development builds,
 * we'd need a getter for the session ticket, so the app code can
 * access the SessionTicket when its SSLInternalMasterSecretFunction
 * callback is called.
 */
OSStatus SSLInternalSetSessionTicket(
   SSLContextRef ctx,
   const void *ticket,
   size_t ticketLength)
{
	if(ctx == NULL) {
		return errSecParam;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return errSecBadReq;
	}
    return tls_handshake_internal_set_session_ticket(ctx->hdsk, ticket, ticketLength);
}


/*
 * ECDSA curve accessors.
 */

/*
 * Obtain the SSL_ECDSA_NamedCurve negotiated during a handshake.
 * Returns errSecParam if no ECDH-related ciphersuite was negotiated.
 */
OSStatus SSLGetNegotiatedCurve(
   SSLContextRef ctx,
   SSL_ECDSA_NamedCurve *namedCurve)    /* RETURNED */
{
	if((ctx == NULL) || (namedCurve == NULL)) {
		return errSecParam;
	}
    unsigned int curve = tls_handshake_get_negotiated_curve(ctx->hdsk);
    if(curve == SSL_Curve_None) {
		return errSecParam;
	}
	*namedCurve = curve;
	return errSecSuccess;
}

/*
 * Obtain the number of currently enabled SSL_ECDSA_NamedCurves.
 */
OSStatus SSLGetNumberOfECDSACurves(
   SSLContextRef ctx,
   unsigned *numCurves)	/* RETURNED */
{
	if((ctx == NULL) || (numCurves == NULL)) {
		return errSecParam;
	}
	*numCurves = ctx->ecdhNumCurves;
	return errSecSuccess;
}

/*
 * Obtain the ordered list of currently enabled SSL_ECDSA_NamedCurves.
 */
OSStatus SSLGetECDSACurves(
   SSLContextRef ctx,
   SSL_ECDSA_NamedCurve *namedCurves,		/* RETURNED */
   unsigned *numCurves)						/* IN/OUT */
{
	if((ctx == NULL) || (namedCurves == NULL) || (numCurves == NULL)) {
		return errSecParam;
	}
	if(*numCurves < ctx->ecdhNumCurves) {
		return errSecParam;
	}
	memmove(namedCurves, ctx->ecdhCurves,
		(ctx->ecdhNumCurves * sizeof(SSL_ECDSA_NamedCurve)));
	*numCurves = ctx->ecdhNumCurves;
	return errSecSuccess;
}

/*
 * Specify ordered list of allowable named curves.
 */
OSStatus SSLSetECDSACurves(
   SSLContextRef ctx,
   const SSL_ECDSA_NamedCurve *namedCurves,
   unsigned numCurves)
{
	if((ctx == NULL) || (namedCurves == NULL) || (numCurves == 0)) {
		return errSecParam;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return errSecBadReq;
	}

	size_t size = numCurves * sizeof(uint16_t);
	ctx->ecdhCurves = (uint16_t *)sslMalloc(size);
	if(ctx->ecdhCurves == NULL) {
		ctx->ecdhNumCurves = 0;
		return errSecAllocate;
	}

    for (unsigned i=0; i<numCurves; i++) {
        ctx->ecdhCurves[i] = namedCurves[i];
    }

	ctx->ecdhNumCurves = numCurves;

    tls_handshake_set_curves(ctx->hdsk, ctx->ecdhCurves, ctx->ecdhNumCurves);
	return errSecSuccess;
}

/*
 * Obtain the number of client authentication mechanisms specified by
 * the server in its Certificate Request message.
 * Returns errSecParam if server hasn't sent a Certificate Request message
 * (i.e., client certificate state is kSSLClientCertNone).
 */
OSStatus SSLGetNumberOfClientAuthTypes(
	SSLContextRef ctx,
	unsigned *numTypes)
{
	if((ctx == NULL) || (ctx->clientCertState == kSSLClientCertNone)) {
		return errSecParam;
	}
	*numTypes = ctx->numAuthTypes;
	return errSecSuccess;
}

/*
 * Obtain the client authentication mechanisms specified by
 * the server in its Certificate Request message.
 * Caller allocates returned array and specifies its size (in
 * SSLClientAuthenticationTypes) in *numType on entry; *numTypes
 * is the actual size of the returned array on successful return.
 */
OSStatus SSLGetClientAuthTypes(
   SSLContextRef ctx,
   SSLClientAuthenticationType *authTypes,		/* RETURNED */
   unsigned *numTypes)							/* IN/OUT */
{
	if((ctx == NULL) || (ctx->clientCertState == kSSLClientCertNone)) {
		return errSecParam;
	}
	memmove(authTypes, ctx->clientAuthTypes,
		ctx->numAuthTypes * sizeof(SSLClientAuthenticationType));
	*numTypes = ctx->numAuthTypes;
	return errSecSuccess;
}

/*
 * -- DEPRECATED -- Return errSecUnimplemented.
 */
OSStatus SSLGetNegotiatedClientAuthType(
   SSLContextRef ctx,
   SSLClientAuthenticationType *authType)		/* RETURNED */
{
    return errSecUnimplemented;
}

OSStatus SSLGetNumberOfSignatureAlgorithms(
    SSLContextRef ctx,
    unsigned *numSigAlgs)
{
	if(ctx == NULL){
		return errSecParam;
	}

    tls_handshake_get_peer_signature_algorithms(ctx->hdsk, numSigAlgs);
	return errSecSuccess;
}

_Static_assert(sizeof(SSLSignatureAndHashAlgorithm)==sizeof(tls_signature_and_hash_algorithm),
               "SSLSignatureAndHashAlgorithm and tls_signature_and_hash_algorithm do not match");

OSStatus SSLGetSignatureAlgorithms(
    SSLContextRef ctx,
    SSLSignatureAndHashAlgorithm *sigAlgs,		/* RETURNED */
    unsigned *numSigAlgs)							/* IN/OUT */
{
	if(ctx == NULL) {
		return errSecParam;
	}

    unsigned numPeerSigAlgs;
    const tls_signature_and_hash_algorithm *peerAlgs = tls_handshake_get_peer_signature_algorithms(ctx->hdsk, &numPeerSigAlgs);

	memmove(sigAlgs, peerAlgs,
            numPeerSigAlgs * sizeof(SSLSignatureAndHashAlgorithm));
	*numSigAlgs = numPeerSigAlgs;
    return errSecSuccess;
}

/* PSK SPIs */
OSStatus SSLSetPSKSharedSecret(SSLContextRef ctx,
                               const void *secret,
                               size_t secretLen)
{
    if(ctx == NULL) return errSecParam;

    if(ctx->pskSharedSecret.data)
        SSLFreeBuffer(&ctx->pskSharedSecret);

    if(SSLCopyBufferFromData(secret, secretLen, &ctx->pskSharedSecret))
        return errSecAllocate;

    tls_handshake_set_psk_secret(ctx->hdsk, &ctx->pskSharedSecret);

    return errSecSuccess;
}

OSStatus SSLSetPSKIdentity(SSLContextRef ctx,
                           const void *pskIdentity,
                           size_t pskIdentityLen)
{
    if((ctx == NULL) || (pskIdentity == NULL) || (pskIdentityLen == 0)) return errSecParam;

    if(ctx->pskIdentity.data)
        SSLFreeBuffer(&ctx->pskIdentity);

    if(SSLCopyBufferFromData(pskIdentity, pskIdentityLen, &ctx->pskIdentity))
        return errSecAllocate;

    tls_handshake_set_psk_identity(ctx->hdsk, &ctx->pskIdentity);

    return errSecSuccess;

}

OSStatus SSLGetPSKIdentity(SSLContextRef ctx,
                           const void **pskIdentity,
                           size_t *pskIdentityLen)
{
    if((ctx == NULL) || (pskIdentity == NULL) || (pskIdentityLen == NULL)) return errSecParam;

    *pskIdentity=ctx->pskIdentity.data;
    *pskIdentityLen=ctx->pskIdentity.length;

    return errSecSuccess;
}

OSStatus SSLInternal_PRF(
                         SSLContext *ctx,
                         const void *vsecret,
                         size_t secretLen,
                         const void *label,			// optional, NULL implies that seed contains
                         //   the label
                         size_t labelLen,
                         const void *seed,
                         size_t seedLen,
                         void *vout,					// mallocd by caller, length >= outLen
                         size_t outLen)
{
    return tls_handshake_internal_prf(ctx->hdsk,
                                      vsecret, secretLen,
                                      label, labelLen,
                                      seed, seedLen,
                                      vout, outLen);
}

const CFStringRef kSSLSessionConfig_default = CFSTR("default");
const CFStringRef kSSLSessionConfig_ATSv1 = CFSTR("ATSv1");
const CFStringRef kSSLSessionConfig_ATSv1_noPFS = CFSTR("ATSv1_noPFS");
const CFStringRef kSSLSessionConfig_legacy = CFSTR("legacy");
const CFStringRef kSSLSessionConfig_standard = CFSTR("standard");
const CFStringRef kSSLSessionConfig_RC4_fallback = CFSTR("RC4_fallback");
const CFStringRef kSSLSessionConfig_TLSv1_fallback = CFSTR("TLSv1_fallback");
const CFStringRef kSSLSessionConfig_TLSv1_RC4_fallback = CFSTR("TLSv1_RC4_fallback");
const CFStringRef kSSLSessionConfig_legacy_DHE = CFSTR("legacy_DHE");
const CFStringRef kSSLSessionConfig_anonymous = CFSTR("anonymous");
const CFStringRef kSSLSessionConfig_3DES_fallback = CFSTR("3DES_fallback");
const CFStringRef kSSLSessionConfig_TLSv1_3DES_fallback = CFSTR("TLSv1_3DES_fallback");


static
tls_handshake_config_t SSLSessionConfig_to_tls_handshake_config(CFStringRef config)
{
    if(CFEqual(config, kSSLSessionConfig_ATSv1)){
        return tls_handshake_config_ATSv1;
    } else if(CFEqual(config, kSSLSessionConfig_ATSv1_noPFS)){
        return tls_handshake_config_ATSv1_noPFS;
    } else if(CFEqual(config, kSSLSessionConfig_standard)){
        return tls_handshake_config_standard;
    } else if(CFEqual(config, kSSLSessionConfig_TLSv1_fallback)){
        return tls_handshake_config_TLSv1_fallback;
    } else if(CFEqual(config, kSSLSessionConfig_TLSv1_RC4_fallback)){
        return tls_handshake_config_TLSv1_RC4_fallback;
    } else if(CFEqual(config, kSSLSessionConfig_RC4_fallback)){
        return tls_handshake_config_RC4_fallback;
    } else if(CFEqual(config, kSSLSessionConfig_3DES_fallback)){
        return tls_handshake_config_3DES_fallback;
    } else if(CFEqual(config, kSSLSessionConfig_TLSv1_3DES_fallback)){
        return tls_handshake_config_TLSv1_3DES_fallback;
    } else if(CFEqual(config, kSSLSessionConfig_legacy)){
        return tls_handshake_config_legacy;
    } else if(CFEqual(config, kSSLSessionConfig_legacy_DHE)){
        return tls_handshake_config_legacy_DHE;
    } else if(CFEqual(config, kSSLSessionConfig_anonymous)){
        return tls_handshake_config_anonymous;
    } else if(CFEqual(config, kSSLSessionConfig_default)){
        return tls_handshake_config_default;
    } else {
        return tls_handshake_config_none;
    }
}

/* Set Predefined TLS Configuration */
OSStatus
SSLSetSessionConfig(SSLContextRef context,
                    CFStringRef config)
{
    tls_handshake_config_t cfg = SSLSessionConfig_to_tls_handshake_config(config);
    if(cfg>=0) {
        return tls_handshake_set_config(context->hdsk, cfg);
    } else {
        return errSecParam;
    }
}
