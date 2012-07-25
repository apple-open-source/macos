/*
 * Copyright (c) 2002-2011 Apple Inc. All rights reserved.
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
 * tlsutil.c
 * - utility functions for dealing with Secure Transport API's
 */

/* 
 * Modification History
 *
 * August 26, 2002	Dieter Siegmund (dieter@apple)
 * - created
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <syslog.h>

#include <Security/SecureTransport.h>
#if !TARGET_OS_EMBEDDED
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#endif
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFData.h>
#include <Security/SecureTransportPriv.h>
#include <TargetConditionals.h>
#if TARGET_OS_EMBEDDED
#include <Security/SecCertificatePriv.h>
#include <Security/SecPolicyPriv.h>
#else /* TARGET_OS_EMBEDDED */
#include <Security/oidsalg.h>
#include <Security/SecKeychain.h>
#include <Security/SecPolicySearch.h>
#endif /* TARGET_OS_EMBEDDED */
#include <Security/SecTrustPriv.h>
#include <SystemConfiguration/SCValidation.h>
#include <Security/SecPolicy.h>
#include "EAPClientProperties.h"
#include "EAPCertificateUtil.h"
#include "EAPTLSUtil.h"
#include "EAPSecurity.h"
#include "printdata.h"
#include "myCFUtil.h"
#include "nbo.h"

/* set a 12-hour session cache timeout */
#define kEAPTLSSessionCacheTimeoutSeconds	(12 * 60 * 60)

uint32_t
EAPTLSLengthIncludedPacketGetMessageLength(EAPTLSLengthIncludedPacketRef pkt)
{
        return (net_uint32_get(pkt->tls_message_length));
}

void
EAPTLSLengthIncludedPacketSetMessageLength(EAPTLSLengthIncludedPacketRef pkt,
                                           uint32_t length)
{
        return (net_uint32_set(pkt->tls_message_length, length));
}

#ifdef NOTYET
/*
 * Lists of SSLCipherSuites used in setCipherRestrictions. Note that the 
 * SecureTransport library does not implement all of these; we only specify
 * the ones it claims to support.
 */
static SSLCipherSuite suites40[] = {
    SSL_RSA_EXPORT_WITH_RC4_40_MD5,
    SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5,
    SSL_RSA_EXPORT_WITH_DES40_CBC_SHA,
    SSL_DH_DSS_EXPORT_WITH_DES40_CBC_SHA,
    SSL_DH_RSA_EXPORT_WITH_DES40_CBC_SHA,
    SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA,
    SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA,
    SSL_DH_anon_EXPORT_WITH_RC4_40_MD5,
    SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA,
    SSL_NO_SUCH_CIPHERSUITE
};
static SSLCipherSuite suitesDES[] = {
    SSL_RSA_WITH_DES_CBC_SHA,
    SSL_DH_DSS_WITH_DES_CBC_SHA,
    SSL_DH_RSA_WITH_DES_CBC_SHA,
    SSL_DHE_DSS_WITH_DES_CBC_SHA,
    SSL_DHE_RSA_WITH_DES_CBC_SHA,
    SSL_DH_anon_WITH_DES_CBC_SHA,
    SSL_RSA_WITH_DES_CBC_MD5,
    SSL_NO_SUCH_CIPHERSUITE
};
static SSLCipherSuite suitesDES40[] = {
    SSL_RSA_EXPORT_WITH_DES40_CBC_SHA,
    SSL_DH_DSS_EXPORT_WITH_DES40_CBC_SHA,
    SSL_DH_RSA_EXPORT_WITH_DES40_CBC_SHA,
    SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA,
    SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA,
    SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA,
    SSL_NO_SUCH_CIPHERSUITE
};
static SSLCipherSuite suites3DES[] = {
    SSL_RSA_WITH_3DES_EDE_CBC_SHA,
    SSL_DH_DSS_WITH_3DES_EDE_CBC_SHA,
    SSL_DH_RSA_WITH_3DES_EDE_CBC_SHA,
    SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA,
    SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA,
    SSL_DH_anon_WITH_3DES_EDE_CBC_SHA,
    SSL_RSA_WITH_3DES_EDE_CBC_MD5,
    SSL_NO_SUCH_CIPHERSUITE
};
static SSLCipherSuite suitesRC4[] = {
    SSL_RSA_WITH_RC4_128_MD5,
    SSL_RSA_WITH_RC4_128_SHA,
    SSL_DH_anon_WITH_RC4_128_MD5,
    SSL_NO_SUCH_CIPHERSUITE
};
static SSLCipherSuite suitesRC4_40[] = {
    SSL_RSA_EXPORT_WITH_RC4_40_MD5,
    SSL_DH_anon_EXPORT_WITH_RC4_40_MD5,
    SSL_NO_SUCH_CIPHERSUITE
};
static SSLCipherSuite suitesRC2[] = {
    SSL_RSA_WITH_RC2_CBC_MD5,
    SSL_NO_SUCH_CIPHERSUITE
};

const char *
EAPSSLCipherSuiteString(SSLCipherSuite cs)
{
    switch(cs) {
    case SSL_NULL_WITH_NULL_NULL:
	return "SSL_NULL_WITH_NULL_NULL";
    case SSL_RSA_WITH_NULL_MD5:
	return "SSL_RSA_WITH_NULL_MD5";
    case SSL_RSA_WITH_NULL_SHA:
	return "SSL_RSA_WITH_NULL_SHA";
    case SSL_RSA_EXPORT_WITH_RC4_40_MD5:
	return "SSL_RSA_EXPORT_WITH_RC4_40_MD5";
    case SSL_RSA_WITH_RC4_128_MD5:
	return "SSL_RSA_WITH_RC4_128_MD5";
    case SSL_RSA_WITH_RC4_128_SHA:
	return "SSL_RSA_WITH_RC4_128_SHA";
    case SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5:
	return "SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5";
    case SSL_RSA_WITH_IDEA_CBC_SHA:
	return "SSL_RSA_WITH_IDEA_CBC_SHA";
    case SSL_RSA_EXPORT_WITH_DES40_CBC_SHA:
	return "SSL_RSA_EXPORT_WITH_DES40_CBC_SHA";
    case SSL_RSA_WITH_DES_CBC_SHA:
	return "SSL_RSA_WITH_DES_CBC_SHA";
    case SSL_RSA_WITH_3DES_EDE_CBC_SHA:
	return "SSL_RSA_WITH_3DES_EDE_CBC_SHA";
    case SSL_DH_DSS_EXPORT_WITH_DES40_CBC_SHA:
	return "SSL_DH_DSS_EXPORT_WITH_DES40_CBC_SHA";
    case SSL_DH_DSS_WITH_DES_CBC_SHA:
	return "SSL_DH_DSS_WITH_DES_CBC_SHA";
    case SSL_DH_DSS_WITH_3DES_EDE_CBC_SHA:
	return "SSL_DH_DSS_WITH_3DES_EDE_CBC_SHA";
    case SSL_DH_RSA_EXPORT_WITH_DES40_CBC_SHA:
	return "SSL_DH_RSA_EXPORT_WITH_DES40_CBC_SHA";
    case SSL_DH_RSA_WITH_DES_CBC_SHA:
	return "SSL_DH_RSA_WITH_DES_CBC_SHA";
    case SSL_DH_RSA_WITH_3DES_EDE_CBC_SHA:
	return "SSL_DH_RSA_WITH_3DES_EDE_CBC_SHA";
    case SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA:
	return "SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA";
    case SSL_DHE_DSS_WITH_DES_CBC_SHA:
	return "SSL_DHE_DSS_WITH_DES_CBC_SHA";
    case SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA:
	return "SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA";
    case SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA:
	return "SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA";
    case SSL_DHE_RSA_WITH_DES_CBC_SHA:
	return "SSL_DHE_RSA_WITH_DES_CBC_SHA";
    case SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA:
	return "SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA";
    case SSL_DH_anon_EXPORT_WITH_RC4_40_MD5:
	return "SSL_DH_anon_EXPORT_WITH_RC4_40_MD5";
    case SSL_DH_anon_WITH_RC4_128_MD5:
	return "SSL_DH_anon_WITH_RC4_128_MD5";
    case SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA:
	return "SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA";
    case SSL_DH_anon_WITH_DES_CBC_SHA:
	return "SSL_DH_anon_WITH_DES_CBC_SHA";
    case SSL_DH_anon_WITH_3DES_EDE_CBC_SHA:
	return "SSL_DH_anon_WITH_3DES_EDE_CBC_SHA";
    case SSL_FORTEZZA_DMS_WITH_NULL_SHA:
	return "SSL_FORTEZZA_DMS_WITH_NULL_SHA";
    case SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA:
	return "SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA";
    case SSL_RSA_WITH_RC2_CBC_MD5:
	return "SSL_RSA_WITH_RC2_CBC_MD5";
    case SSL_RSA_WITH_IDEA_CBC_MD5:
	return "SSL_RSA_WITH_IDEA_CBC_MD5";
    case SSL_RSA_WITH_DES_CBC_MD5:
	return "SSL_RSA_WITH_DES_CBC_MD5";
    case SSL_RSA_WITH_3DES_EDE_CBC_MD5:
	return "SSL_RSA_WITH_3DES_EDE_CBC_MD5";
    case SSL_NO_SUCH_CIPHERSUITE:
	return "SSL_NO_SUCH_CIPHERSUITE";
    default:
	return "<unknown>";
    }
}

/* 
 * Given a SSLProtocolVersion - typically from SSLGetProtocolVersion -
 * return a string representation.
 */
const char *
EAPSSLProtocolVersionString(SSLProtocol prot)
{
    switch(prot) {
    case kSSLProtocolUnknown:
	return "kSSLProtocolUnknown";
    case kSSLProtocol2:
	return "kSSLProtocol2";
    case kSSLProtocol3:
	return "kSSLProtocol3";
    case kSSLProtocol3Only:
	return "kSSLProtocol3Only";
    case kTLSProtocol1:
	return "kTLSProtocol1";
    case kTLSProtocol1Only:
	return "kTLSProtocol1Only";
    default:
	return "<unknown>";
    }
}

/*
 * Given an SSLContextRef and an array of SSLCipherSuites, terminated by
 * SSL_NO_SUCH_CIPHERSUITE, select those SSLCipherSuites which the library
 * supports and do a SSLSetEnabledCiphers() specifying those. 
 */
static OSStatus 
setEnabledCiphers(SSLContextRef ctx,
		  const SSLCipherSuite * ciphers)
{
    UInt32 numSupported;
    OSStatus ortn = noErr;
    SSLCipherSuite *supported = NULL;
    SSLCipherSuite *enabled = NULL;
    unsigned enabledDex = 0;	// index into enabled
    unsigned supportedDex = 0;	// index into supported
    unsigned inDex = 0;			// index into ciphers
	
    /* first get all the supported ciphers */
    ortn = SSLGetNumberSupportedCiphers(ctx, &numSupported);
    if (ortn) {
	goto done;
    }
    supported = (SSLCipherSuite *)malloc(numSupported * sizeof(SSLCipherSuite));
    ortn = SSLGetSupportedCiphers(ctx, supported, &numSupported);
    if (ortn) {
	goto done;
    }
	
    /* 
     * Malloc an array we'll use for SSLGetEnabledCiphers - this will  be
     * bigger than the number of suites we actually specify 
     */
    enabled = (SSLCipherSuite *)malloc(numSupported * sizeof(SSLCipherSuite));
	
    /* 
     * For each valid suite in ciphers, see if it's in the list of 
     * supported ciphers. If it is, add it to the list of ciphers to be
     * enabled. 
     */
    for(inDex=0; ciphers[inDex] != SSL_NO_SUCH_CIPHERSUITE; inDex++) {
	for(supportedDex=0; supportedDex<numSupported; supportedDex++) {
	    if(ciphers[inDex] == supported[supportedDex]) {
		enabled[enabledDex++] = ciphers[inDex];
		break;
	    }
	}
    }
	
    /* send it on down. */
    ortn = SSLSetEnabledCiphers(ctx, enabled, enabledDex);
 done:
    if (enabled != NULL) {
	free(enabled);
    }
    if (supported != NULL) {
	free(supported);
    }
    return ortn;
}

/*
 * Specify a restricted set of cipherspecs.
 */
OSStatus
EAPSSLContextSetCipherRestrictions(SSLContextRef ctx, char cipherRestrict)
{
    OSStatus ortn = noErr;
	
    switch(cipherRestrict) {
    case 'e':
	ortn = setEnabledCiphers(ctx, suites40);
	break;
    case 'd':
	ortn = setEnabledCiphers(ctx, suitesDES);
	break;
    case 'D':
	ortn = setEnabledCiphers(ctx, suitesDES40);
	break;
    case '3':
	ortn = setEnabledCiphers(ctx, suites3DES);
	break;
    case '4':
	ortn = setEnabledCiphers(ctx, suitesRC4);
	break;
    case '$':
	ortn = setEnabledCiphers(ctx, suitesRC4_40);
	break;
    case '2':
	ortn = setEnabledCiphers(ctx, suitesRC2);
	break;
    default:
	break;
    }
    return ortn;
}

#endif /* NOTYET */

const char *
EAPSSLErrorString(OSStatus err)
{
    return (EAPSecurityErrorString(err));
}

SSLContextRef
EAPSSLContextCreate(SSLProtocol protocol, bool is_server, 
		    SSLReadFunc func_read, SSLWriteFunc func_write, 
		    void * handle, char * peername, OSStatus * ret_status)
{
    SSLContextRef       ctx = NULL;
    OSStatus		status;

    *ret_status = noErr;
    status = SSLNewContext(is_server, &ctx);
    if (status != noErr) {
	goto cleanup;
    } 
    status = SSLSetIOFuncs(ctx, func_read, func_write);
    if (status) {
	goto cleanup;
    }
    status = SSLSetProtocolVersion(ctx, protocol);
    if (status) {
	goto cleanup;
    } 
    status = SSLSetConnection(ctx, handle);
    if (status) {
	goto cleanup;
    }
    if (peername != NULL) {
	status = SSLSetPeerDomainName(ctx, peername, strlen(peername) + 1);
	if (status) {
	    goto cleanup;
	}
    }
    (void)SSLSetSessionCacheTimeout(ctx, kEAPTLSSessionCacheTimeoutSeconds);
    return (ctx);

 cleanup:
    if (ctx != NULL) {
	SSLDisposeContext(ctx);
    }
    *ret_status = status;
    return (NULL);
}

SSLContextRef
EAPTLSMemIOContextCreate(bool is_server, memoryIORef mem_io, 
			 char * peername, OSStatus * ret_status)
{
    return(EAPSSLContextCreate(kTLSProtocol1Only, is_server,
			       EAPSSLMemoryIORead, EAPSSLMemoryIOWrite, 
			       mem_io, peername, ret_status));
}

OSStatus 
EAPSSLMemoryIORead(SSLConnectionRef connection, void * data_buf, 
		   size_t * data_length)
{
    size_t		bytes_left;
    size_t		length = *data_length;
    memoryIORef		mem_io = (memoryIORef)connection;
    memoryBufferRef	mem_buf = mem_io->read;

    if (mem_buf == NULL) {
	if (mem_io->debug) {
	    printf("Read not initialized\n");
	}
	*data_length = 0;
	return (noErr);
    }
    bytes_left = mem_buf->length - mem_buf->offset;
    if (mem_buf->data == NULL 
	|| mem_buf->length == 0 || bytes_left == 0) {
	*data_length = 0;
	if (mem_io->debug) {
	    printf("Read would block\n");
	}
	return (errSSLWouldBlock);
    }
    if (length > bytes_left) {
	length = bytes_left;
    }
    bcopy(mem_buf->data + mem_buf->offset, data_buf, length);
    mem_buf->offset += length;
    if (mem_buf->offset == mem_buf->length) {
	free(mem_buf->data);
	bzero(mem_buf, sizeof(*mem_buf));
    }
    *data_length = length;
    if (mem_io->debug) {
	printf("Reading %d bytes\n", (int)length);
	print_data(data_buf, length);
    }

    return (noErr);
}

OSStatus
EAPSSLMemoryIOWrite(SSLConnectionRef connection, const void * data_buf, 
		    size_t * data_length)
{
    bool		additional = FALSE;
    size_t		length = *data_length;
    memoryIORef		mem_io = (memoryIORef)connection;
    memoryBufferRef	mem_buf = mem_io->write;

    if (mem_buf == NULL) {
	if (mem_io->debug) {
	    printf("Write not initialized\n");
	}
	*data_length = 0;
	return (noErr);
    }
    if (mem_buf->data == NULL) {
	mem_buf->data = malloc(length);
	mem_buf->offset = 0;
	mem_buf->length = length;
	bcopy(data_buf, mem_buf->data, length);
    }
    else {
	additional = TRUE;
	mem_buf->data = realloc(mem_buf->data, length + mem_buf->length);
	bcopy(data_buf, mem_buf->data + mem_buf->length, length);
	mem_buf->length += length;
    }
    if (mem_io->debug) {
	printf("Writing %s%d bytes\n", additional ? "additional " : "",
	       (int)length);
	print_data((void *)data_buf, length);
    }
    return (noErr);
}

void
memoryBufferInit(memoryBufferRef buf)
{
    bzero(buf, sizeof(*buf));
    return;
}

void
memoryBufferClear(memoryBufferRef buf)
{
    if (buf == NULL) {
	return;
    }
    if (buf->data != NULL) {
	free(buf->data);
    }
    bzero(buf, sizeof(*buf));
    return;
}

void
memoryIOClearBuffers(memoryIORef mem_io)
{
    memoryBufferClear(mem_io->read);
    memoryBufferClear(mem_io->write);
    return;
}

void
memoryIOInit(memoryIORef mem_io, memoryBufferRef read_buf, 
	     memoryBufferRef write_buf)
{
    bzero(mem_io, sizeof(*mem_io));
    memoryBufferInit(read_buf);
    memoryBufferInit(write_buf);
    mem_io->read = read_buf;
    mem_io->write = write_buf;
    return;
}

void
memoryIOSetDebug(memoryIORef mem_io, bool debug)
{
    mem_io->debug = debug;
    return;
}

OSStatus
EAPTLSComputeKeyData(SSLContextRef ssl_context, 
		     const void * label, int label_length,
		     void * key, int key_length)
{
    char		master_secret[SSL_MASTER_SECRET_SIZE];
    size_t		master_secret_length;
    size_t		offset;
    char		random[SSL_CLIENT_SRVR_RAND_SIZE * 2];
    size_t		random_size = 0;
    size_t		size;
    OSStatus		status;

    offset = 0;
    size = sizeof(random);
    status = SSLInternalClientRandom(ssl_context, random, &size);
    if (status != noErr) {
	fprintf(stderr, 
		"EAPTLSComputeSessionKey: SSLInternalClientRandom failed, %s\n",
		EAPSSLErrorString(status));
	return (status);
    }
    offset += size;
    random_size += size;
    if ((size + SSL_CLIENT_SRVR_RAND_SIZE) > sizeof(random)) {
	fprintf(stderr,
		"EAPTLSComputeSessionKey: buffer overflow %ld >= %ld\n",
		size + SSL_CLIENT_SRVR_RAND_SIZE, sizeof(random));
	return (errSSLBufferOverflow);
    }
    size = sizeof(random) - size;
    status = SSLInternalServerRandom(ssl_context, random + offset, &size);
    if (status != noErr) {
	fprintf(stderr, 
		"EAPTLSComputeSessionKey: SSLInternalServerRandom failed, %s\n",
		EAPSSLErrorString(status));
	return (status);
    }
    random_size += size;
    master_secret_length = sizeof(master_secret);
    status = SSLInternalMasterSecret(ssl_context, master_secret,
				     &master_secret_length);
    if (status != noErr) {
	fprintf(stderr, 
		"EAPTLSComputeSessionKey: SSLInternalMasterSecret failed, %s\n",
		EAPSSLErrorString(status));
	return (status);
    }
    status = SSLInternal_PRF(ssl_context,
			     master_secret, master_secret_length,
			     label, label_length,
			     random, random_size, 
			     key, key_length);
    if (status != noErr) {
	fprintf(stderr,
		"EAPTLSComputeSessionKey: SSLInternal_PRF failed, %s\n",
		EAPSSLErrorString(status));
	return (status);
    }
    return (status);
}


EAPPacket *
EAPTLSPacketCreate2(EAPCode code, int type, u_char identifier, int mtu,
		    memoryBufferRef buf, int * ret_fraglen, 
		    bool always_mark_first)
{
    bool		first_fragment = FALSE;
    bool		more_fragments = FALSE;
    EAPTLSPacket *	eaptls = NULL;
    int 		pkt_size;
    size_t		fraglen = 0;

    if (buf != NULL && buf->data != NULL && buf->offset < buf->length) {
	int	max_payload;

	if (buf->offset == 0 && always_mark_first) {
	    first_fragment = TRUE;
	    pkt_size = sizeof(EAPTLSLengthIncludedPacket);
	}
	else {
	    pkt_size = sizeof(EAPTLSPacket);
	}
	max_payload = mtu - pkt_size;
	fraglen = buf->length - buf->offset;
	if (fraglen > max_payload) {
	    if (buf->offset == 0 && always_mark_first == FALSE) {
		first_fragment = TRUE;
		pkt_size = sizeof(EAPTLSLengthIncludedPacket);
		max_payload = mtu - pkt_size;
	    }
	    more_fragments = TRUE;
	    fraglen = max_payload;
	}
    }
    else {
	pkt_size = sizeof(EAPTLSPacket);
    }

    if (ret_fraglen != NULL) {
	*ret_fraglen = fraglen;
    }
    pkt_size += fraglen;
    eaptls = malloc(pkt_size);
    if (eaptls == NULL) {
	return (NULL);
    }
    eaptls->code = code;
    eaptls->identifier = identifier;
    EAPPacketSetLength((EAPPacketRef)eaptls, pkt_size);
    eaptls->type = type;
    eaptls->flags = 0;
    if (fraglen != 0) {
	void *		dest;

	dest = eaptls->tls_data;
	if (more_fragments) {
	    eaptls->flags = kEAPTLSPacketFlagsMoreFragments;
	}
	if (first_fragment) {
	    EAPTLSLengthIncludedPacket * first;

	    /* ALIGN: void * cast OK, 
	     * we don't expect proper alignment */
	    first = (EAPTLSLengthIncludedPacket *)(void *)eaptls;
	    eaptls->flags |= kEAPTLSPacketFlagsLengthIncluded;
            EAPTLSLengthIncludedPacketSetMessageLength(first, 
							buf->length);
	    dest = first->tls_data;
	}
	bcopy(buf->data + buf->offset, dest, fraglen);
    }
    return ((EAPPacket *)eaptls);
}

EAPPacket *
EAPTLSPacketCreate(EAPCode code, int type, u_char identifier, int mtu,
		   memoryBufferRef buf, int * ret_fraglen)
{
    return (EAPTLSPacketCreate2(code, type, identifier, mtu,
				buf, ret_fraglen, TRUE));
}

OSStatus 
EAPSSLCopyPeerCertificates(SSLContextRef context, CFArrayRef * certs)
{
    return (SSLCopyPeerCertificates(context, certs));
}

#if TARGET_OS_EMBEDDED
OSStatus
EAPSecPolicyCopy(SecPolicyRef * ret_policy)
{
    *ret_policy = SecPolicyCreateEAP(FALSE, NULL);
    if (*ret_policy != NULL) {
	return (noErr);
    }
    return (-1);
}

#else /* TARGET_OS_EMBEDDED */

OSStatus
EAPSecPolicyCopy(SecPolicyRef * ret_policy)
{

    *ret_policy = SecPolicyCreateWithOID(kSecPolicyAppleEAP);
    if (*ret_policy != NULL) {
	return (noErr);
    }
    return (-1);
}
#endif /* TARGET_OS_EMBEDDED */

static CFArrayRef
copy_cert_list(CFDictionaryRef properties, CFStringRef prop_name)
{
    CFArrayRef		data_list;

    if (properties == NULL) {
	return (NULL);
    }
    data_list = CFDictionaryGetValue(properties, prop_name);
    if (isA_CFArray(data_list) == NULL) {
	return (NULL);
    }
    return (EAPCFDataArrayCreateSecCertificateArray(data_list));
}

static CFArrayRef
copy_user_trust_proceed_certs(CFDictionaryRef properties)
{
    CFArrayRef		p;

    p = CFDictionaryGetValue(properties,
			     kEAPClientPropTLSUserTrustProceedCertificateChain);
    if (p != NULL) {
	p = EAPCFDataArrayCreateSecCertificateArray(p);
    }
    return (p);
}

static CFArrayRef
get_trusted_server_names(CFDictionaryRef properties)
{
    int		count;
    int		i;
    CFArrayRef	list;

    list = CFDictionaryGetValue(properties, 
				kEAPClientPropTLSTrustedServerNames);
    if (list == NULL) {
	list = CFDictionaryGetValue(properties, 
				    CFSTR("TLSTrustedServerCommonNames"));
	if (list == NULL) {
	    return (NULL);
	}
    }
    if (isA_CFArray(list) == NULL) {
	syslog(LOG_NOTICE,
	       "EAPTLSVerifyServerCertificateChain: TLSTrustedServerNames is not an array");
	return (NULL);
    }
    count = CFArrayGetCount(list);
    if (count == 0) {
	syslog(LOG_NOTICE,
	       "EAPTLSVerifyServerCertificateChain: TLSTrustedServerNames is empty");
	return (NULL);
    }
    for (i = 0; i < count; i++) {
	CFStringRef	name = CFArrayGetValueAtIndex(list, i);

	if (isA_CFString(name) == NULL) {
	    syslog(LOG_NOTICE, 
	       "EAPTLSVerifyServerCertificateChain: TLSTrustedServerNames contains a non-string value");
	    return (NULL);
	}
    }
    return (list);
}

#if TARGET_OS_EMBEDDED
#include <CoreFoundation/CFPreferences.h>
#include <notify.h>

#define kEAPTLSTrustExceptionsID 		"com.apple.network.eapclient.tls.TrustExceptions"
#define kEAPTLSTrustExceptionsApplicationID	CFSTR(kEAPTLSTrustExceptionsID)

static int	token;
static bool	token_valid;

static void
exceptions_change_check(void)
{
    int		check = 0;
    uint32_t	status;

    if (!token_valid) {
	status = notify_register_check(kEAPTLSTrustExceptionsID, &token);
	if (status != NOTIFY_STATUS_OK) {
	    syslog(LOG_NOTICE,
		   "EAPTLSTrustExceptions: notify_register_check returned %d",
		   status);
	    return;
	}
	token_valid = TRUE;
    }
    status = notify_check(token, &check);
    if (status != NOTIFY_STATUS_OK) {
	syslog(LOG_NOTICE,
	       "EAPTLSTrustExceptions: notify_check returned %d",
	       status);
	return;
    }
    if (check != 0) {
	CFPreferencesSynchronize(kEAPTLSTrustExceptionsApplicationID,
				 kCFPreferencesCurrentUser,
				 kCFPreferencesAnyHost);
    }
    return;
}

static void
exceptions_change_notify(void)
{
    uint32_t	status;

    status = notify_post(kEAPTLSTrustExceptionsID);
    if (status != NOTIFY_STATUS_OK) {
	syslog(LOG_NOTICE,
	       "EAPTLSTrustExceptions: notify_post returned %d",
	       status);
    }
    return;
}

static void
EAPTLSTrustExceptionsSave(CFStringRef domain, CFStringRef identifier,
			  CFStringRef hash_str, CFDataRef exceptions)
{
    CFDictionaryRef	domain_list;
    CFDictionaryRef	exceptions_list = NULL;
    bool		store_exceptions = TRUE;

    exceptions_change_check();
    domain_list = CFPreferencesCopyValue(domain,
					 kEAPTLSTrustExceptionsApplicationID,
					 kCFPreferencesCurrentUser,
					 kCFPreferencesAnyHost);
    if (domain_list != NULL && isA_CFDictionary(domain_list) == NULL) {
	CFRelease(domain_list);
	domain_list = NULL;
    }
    if (domain_list != NULL) {
	exceptions_list = CFDictionaryGetValue(domain_list, identifier);
	exceptions_list = isA_CFDictionary(exceptions_list);
	if (exceptions_list != NULL) {
	    CFDataRef	stored_exceptions;

	    stored_exceptions = CFDictionaryGetValue(exceptions_list, hash_str);
	    if (isA_CFData(stored_exceptions) != NULL 
		&& CFEqual(stored_exceptions, exceptions)) {
		/* stored exceptions are correct, don't store them again */
		store_exceptions = FALSE;
	    }
	}
    }
    if (store_exceptions) {
	if (exceptions_list == NULL) {
	    /* no exceptions for this identifier yet, create one */
	    exceptions_list
		= CFDictionaryCreate(NULL,
				     (const void * * )&hash_str,
				     (const void * *)&exceptions,
				     1,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
	}
	else {
	    /* update existing exceptions with this one */
	    CFMutableDictionaryRef	new_exceptions_list;

	    new_exceptions_list
		= CFDictionaryCreateMutableCopy(NULL, 0,
						exceptions_list);
	    CFDictionarySetValue(new_exceptions_list, hash_str, exceptions);
	    /* don't CFRelease(exceptions_list), it's from domain_list */
	    exceptions_list = (CFDictionaryRef)new_exceptions_list;

	}
	if (domain_list == NULL) {
	    domain_list
		= CFDictionaryCreate(NULL, 
				     (const void * *)&identifier,
				     (const void * *)&exceptions_list,
				     1,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
	}
	else {
	    CFMutableDictionaryRef	new_domain_list;

	    new_domain_list
		= CFDictionaryCreateMutableCopy(NULL, 0,
						domain_list);
	    CFDictionarySetValue(new_domain_list, identifier, exceptions_list);
	    CFRelease(domain_list);
	    domain_list = (CFDictionaryRef)new_domain_list;

	}
	CFRelease(exceptions_list);
	CFPreferencesSetValue(domain, domain_list,
			      kEAPTLSTrustExceptionsApplicationID,
			      kCFPreferencesCurrentUser,
			      kCFPreferencesAnyHost);
	CFPreferencesSynchronize(kEAPTLSTrustExceptionsApplicationID,
				 kCFPreferencesCurrentUser,
				 kCFPreferencesAnyHost);
	exceptions_change_notify();
    }
    my_CFRelease(&domain_list);
    return;
}

/*
 * Function: EAPTLSSecTrustSaveExceptions
 * Purpose:
 *   Given the evaluated SecTrustRef object, save an exceptions binding for the 
 *   given domain, identifier, and server_hash_str, all of which must be
 *   specified.
 * Returns:
 *   FALSE if the trust object was not in a valid state, 
 *   TRUE otherwise.
 */
bool
EAPTLSSecTrustSaveExceptionsBinding(SecTrustRef trust, 
				    CFStringRef domain, CFStringRef identifier,
				    CFStringRef server_hash_str)
{
    CFDataRef 	exceptions;

    exceptions = SecTrustCopyExceptions(trust);
    if (exceptions == NULL) {
	syslog(LOG_NOTICE,
	       "EAPTLSSecTrustSaveExceptionsBinding():"
	       " failed to copy exceptions");
	return (FALSE);
    }
    EAPTLSTrustExceptionsSave(domain, identifier, server_hash_str,
			      exceptions);
    CFRelease(exceptions);
    return (TRUE);
}

/* 
 * Function: EAPTLSRemoveTrustExceptionsBindings
 * Purpose:
 *   Remove all of the trust exceptions bindings for the given
 *   trust domain and identifier.
 * Example:
 * EAPTLSRemoveTrustExceptionsBindings(kEAPTLSTrustExceptionsDomainWirelessSSID,
 */
void
EAPTLSRemoveTrustExceptionsBindings(CFStringRef domain, CFStringRef identifier)
{
    CFDictionaryRef	domain_list;
    CFDictionaryRef	exceptions_list;

    exceptions_change_check();
    domain_list = CFPreferencesCopyValue(domain,
					 kEAPTLSTrustExceptionsApplicationID,
					 kCFPreferencesCurrentUser,
					 kCFPreferencesAnyHost);
    if (domain_list != NULL && isA_CFDictionary(domain_list) == NULL) {
	CFRelease(domain_list);
	domain_list = NULL;
    }
    if (domain_list == NULL) {
	return;
    }
    exceptions_list = CFDictionaryGetValue(domain_list, identifier);
    if (exceptions_list != NULL) {
	CFMutableDictionaryRef	new_domain_list;
	
	new_domain_list
	    = CFDictionaryCreateMutableCopy(NULL, 0,
					    domain_list);
	CFDictionaryRemoveValue(new_domain_list, identifier);
	CFPreferencesSetValue(domain, new_domain_list,
			      kEAPTLSTrustExceptionsApplicationID,
			      kCFPreferencesCurrentUser,
			      kCFPreferencesAnyHost);
	CFPreferencesSynchronize(kEAPTLSTrustExceptionsApplicationID,
				 kCFPreferencesCurrentUser,
				 kCFPreferencesAnyHost);
	exceptions_change_notify();
    }
    CFRelease(domain_list);
    return;
}

static CFDataRef
EAPTLSTrustExceptionsCopy(CFStringRef domain, CFStringRef identifier,
			  CFStringRef hash_str)
{
    CFDataRef		exceptions = NULL;
    CFDictionaryRef	domain_list;

    exceptions_change_check();
    domain_list = CFPreferencesCopyValue(domain,
					 kEAPTLSTrustExceptionsApplicationID,
					 kCFPreferencesCurrentUser,
					 kCFPreferencesAnyHost);
    if (isA_CFDictionary(domain_list) != NULL) {
	CFDictionaryRef		exceptions_list;

	exceptions_list = CFDictionaryGetValue(domain_list, identifier);
	if (isA_CFDictionary(exceptions_list) != NULL) {
	    exceptions = isA_CFData(CFDictionaryGetValue(exceptions_list,
							 hash_str));
	    if (exceptions != NULL) {
		CFRetain(exceptions);
	    }
	}
    }
    my_CFRelease(&domain_list);
    return (exceptions);
}

/*
 * Function: EAPTLSSecTrustApplyExceptionsBinding
 * Purpose:
 *   Finds a stored trust exceptions object for the given domain, identifier,
 *   and server_cert_hash.  If it exists, sets the exceptions on the given
 *   trust object.
 */
void
EAPTLSSecTrustApplyExceptionsBinding(SecTrustRef trust, CFStringRef domain, 
				     CFStringRef identifier,
				     CFStringRef server_cert_hash)
{
    CFDataRef		exceptions;

    exceptions = EAPTLSTrustExceptionsCopy(domain, identifier,
					   server_cert_hash);
    if (exceptions != NULL) {
	if (SecTrustSetExceptions(trust, exceptions) == FALSE) {
	    syslog(LOG_NOTICE, "SecTrustSetExceptions failed");
	}
    }
    my_CFRelease(&exceptions);
    return;
}

static SecTrustRef
_EAPTLSCreateSecTrust(CFDictionaryRef properties, 
		      CFArrayRef server_certs,
		      OSStatus * ret_status,
		      EAPClientStatus * ret_client_status,
		      bool * ret_allow_exceptions,
		      bool * ret_has_server_certs_or_names,
		      CFStringRef * ret_server_hash_str)
{
    bool		allow_exceptions;
    EAPClientStatus	client_status;
    int			count;
    CFStringRef		domain = NULL;
    CFStringRef		identifier = NULL;
    SecPolicyRef	policy = NULL;
    OSStatus		status = noErr;
    CFStringRef		server_hash_str = NULL;
    CFArrayRef		trusted_certs = NULL;
    CFArrayRef		trusted_server_names;
    SecTrustRef		trust = NULL;

    client_status = kEAPClientStatusInternalError;
    if (server_certs == NULL) {
	goto done;
    }
    count = CFArrayGetCount(server_certs);
    if (count == 0) {
	goto done;
    }
    client_status = kEAPClientStatusSecurityError;
    trusted_server_names = get_trusted_server_names(properties);
    policy = SecPolicyCreateEAP(FALSE, trusted_server_names);
    if (policy == NULL) {
	goto done;
    }
    status = SecTrustCreateWithCertificates(server_certs, policy, &trust);
    if (status != noErr) {
	syslog(LOG_NOTICE, 
	       "_EAPTLSCreateSecTrust: "
	       "SecTrustCreateWithCertificates failed, %s (%d)",
	       EAPSecurityErrorString(status), (int)status);
	goto done;
    }
    trusted_certs = copy_cert_list(properties,
				   kEAPClientPropTLSTrustedCertificates);
    if (trusted_certs != NULL) {
	status = SecTrustSetAnchorCertificates(trust, trusted_certs);
	if (status != noErr) {
	    syslog(LOG_NOTICE, 
		   "_EAPTLSCreateSecTrust:"
		   " SecTrustSetAnchorCertificates failed, %s (%d)",
		   EAPSecurityErrorString(status), (int)status);
	    goto done;
	}
    }

    /*
     * Don't allow exceptions by default if either trusted certs or trusted 
     * server names is specified.  Trust exceptions must be explicitly enabled
     * in that case using the kEAPClientPropTLSAllowTrustExceptions property.
     */
    if (trusted_certs != NULL || trusted_server_names != NULL) {
	allow_exceptions = FALSE;
    }
    else {
	allow_exceptions = TRUE;
    }
    allow_exceptions
	= my_CFDictionaryGetBooleanValue(properties, 
					 kEAPClientPropTLSAllowTrustExceptions,
					 allow_exceptions);

    /* both the trust exception domain and identifier must be specified */
    domain 
	= CFDictionaryGetValue(properties,
			       kEAPClientPropTLSTrustExceptionsDomain);
    identifier 
	= CFDictionaryGetValue(properties,
			       kEAPClientPropTLSTrustExceptionsID);
    if (isA_CFString(domain) == NULL || isA_CFString(identifier) == NULL) {
	allow_exceptions = FALSE;
    }
    if (allow_exceptions) {
	SecCertificateRef	server;

	server = (SecCertificateRef)CFArrayGetValueAtIndex(server_certs, 0);
	server_hash_str = EAPSecCertificateCopySHA1DigestString(server);
	EAPTLSSecTrustApplyExceptionsBinding(trust, domain, identifier, 
					     server_hash_str);
    }
    client_status = kEAPClientStatusOK;

 done:
    if (client_status == kEAPClientStatusOK) {
	if (ret_allow_exceptions != NULL) {
	    *ret_allow_exceptions = allow_exceptions;
	}
	if (ret_has_server_certs_or_names != NULL) {
	    *ret_has_server_certs_or_names 
		= (trusted_certs != NULL || trusted_server_names != NULL);
	}
	if (ret_server_hash_str != NULL) {
	    *ret_server_hash_str = server_hash_str;
	}
	else {
	    my_CFRelease(&server_hash_str);
	}
    }
    else {
	my_CFRelease(&trust);
    }
    if (ret_status != NULL) {
	*ret_status = status;
    }
    if (ret_client_status != NULL) {
	*ret_client_status = client_status;
    }
    my_CFRelease(&policy);
    my_CFRelease(&trusted_certs);
    return (trust);
}

SecTrustRef
EAPTLSCreateSecTrust(CFDictionaryRef properties, CFArrayRef server_certs,
		     CFStringRef domain, CFStringRef identifier)
{
    CFMutableDictionaryRef	dict;
    SecTrustRef			trust;

    dict = CFDictionaryCreateMutableCopy(NULL, 0, properties);
    CFDictionarySetValue(dict, kEAPClientPropTLSTrustExceptionsDomain, domain);
    CFDictionarySetValue(dict, kEAPClientPropTLSTrustExceptionsID, identifier);
    trust = _EAPTLSCreateSecTrust(dict, server_certs, 
				  NULL, NULL, NULL, NULL, NULL);
    CFRelease(dict);
    return (trust);
}

EAPClientStatus
EAPTLSVerifyServerCertificateChain(CFDictionaryRef properties, 
				   CFArrayRef server_certs,
				   OSStatus * ret_status)
{
    bool		allow_exceptions;
    bool		has_server_certs_or_names = FALSE;
    EAPClientStatus	client_status;
    OSStatus		status;
    CFStringRef		server_hash_str = NULL;
    SecTrustRef		trust = NULL;
    SecTrustResultType 	trust_result;


    trust = _EAPTLSCreateSecTrust(properties, 
				  server_certs,
				  &status,
				  &client_status,
				  &allow_exceptions,
				  &has_server_certs_or_names,
				  &server_hash_str);
    if (trust == NULL) {
	goto done;
    }
    client_status = kEAPClientStatusSecurityError;
    status = SecTrustEvaluate(trust, &trust_result);
    if (status != noErr) {
	syslog(LOG_NOTICE, 
	       "EAPTLSVerifyServerCertificateChain: "
	       "SecTrustEvaluate failed, %s (%d)",
	       EAPSecurityErrorString(status), (int)status);
	goto done;
    }
    switch (trust_result) {
    case kSecTrustResultProceed:
	client_status = kEAPClientStatusOK;
	break;
    case kSecTrustResultUnspecified:
	if (has_server_certs_or_names) {
	    /* trusted certs or server names specified, it's OK to proceed */
	    client_status = kEAPClientStatusOK;
	    break;
	}
	/* FALL THROUGH */
    case kSecTrustResultRecoverableTrustFailure:
	if (allow_exceptions) {
	    client_status = kEAPClientStatusUserInputRequired;
	    break;
	}
	/* FALL THROUGH */
    case kSecTrustResultDeny:
    default:
	status = errSSLXCertChainInvalid;
	break;
    }

    /* if the trust is recoverable, check whether the user already said OK */
    if (client_status == kEAPClientStatusUserInputRequired) {
	CFArrayRef	proceed;

	proceed = copy_user_trust_proceed_certs(properties);
	if (proceed != NULL
	    && CFEqual(proceed, server_certs)) {
	    bool	save_it;

	    client_status = kEAPClientStatusOK;
	    save_it = my_CFDictionaryGetBooleanValue(properties, 
						     kEAPClientPropTLSSaveTrustExceptions,
						     FALSE);
	    if (save_it && server_hash_str != NULL) {
		CFStringRef	domain;
		CFStringRef	identifier;

		domain 
		    = CFDictionaryGetValue(properties,
					   kEAPClientPropTLSTrustExceptionsDomain);
		identifier 
		    = CFDictionaryGetValue(properties,
					   kEAPClientPropTLSTrustExceptionsID);
		EAPTLSSecTrustSaveExceptionsBinding(trust, domain, identifier,
						    server_hash_str);
	    }
	}
	my_CFRelease(&proceed);
    }

 done:
    if (ret_status != NULL) {
	*ret_status = status;
    }
    my_CFRelease(&trust);
    my_CFRelease(&server_hash_str);
    return (client_status);
}

#else /* TARGET_OS_EMBEDDED */

static bool
cert_list_contains_cert(CFArrayRef list, SecCertificateRef cert)
{
    int		count;
    int		i;

    count = CFArrayGetCount(list);
    for (i = 0; i < count; i++) {
	SecCertificateRef	this_cert;

	this_cert = (SecCertificateRef)CFArrayGetValueAtIndex(list, i);
	if (CFEqual(cert, this_cert)) {
	    return (TRUE);
	}
    }
    return (FALSE);
}

static CFArrayRef
EAPSecTrustCopyCertificateChain(SecTrustRef trust)
{
    CFMutableArrayRef	array = NULL;
    int			count = SecTrustGetCertificateCount(trust);
    int			i;
    
    if (count == 0) {
	fprintf(stderr, "SecTrustGetCertificateCount returned 0)\n");
	goto done;
    }
    array = CFArrayCreateMutable(NULL, count, &kCFTypeArrayCallBacks);
    for (i = 0; i < count; i++) {
	SecCertificateRef	s;
	
	s = SecTrustGetCertificateAtIndex(trust, i);
	CFArrayAppendValue(array, s);
    }
 done:
    return (array);
}

static bool
server_cert_chain_is_trusted(SecTrustRef trust, CFArrayRef trusted_certs)
{
    CFArrayRef 				cert_chain;
    int					count;
    int					i;
    bool				ret = FALSE;

    cert_chain = EAPSecTrustCopyCertificateChain(trust);
    if (cert_chain != NULL) {
	count = CFArrayGetCount(cert_chain);
    }
    else {
	count = 0;
    }
    if (count == 0) {
	syslog(LOG_NOTICE,
	       "EAPTLSVerifyCertificateChain: failed to get evidence chain");
	goto done;
    }
    for (i = 0; i < count; i++) {
	SecCertificateRef	cert;

	cert = (SecCertificateRef)CFArrayGetValueAtIndex(cert_chain, i);
	if (cert_list_contains_cert(trusted_certs, cert)) {
	    ret = TRUE;
	    break;
	}
    }

 done:
    my_CFRelease(&cert_chain);
    return (ret);
}

static bool
server_name_matches_server_names(CFStringRef name,
				 CFArrayRef trusted_server_names)
{
    int			count;
    int			i;
    bool		trusted = FALSE;

    count = CFArrayGetCount(trusted_server_names);
    for (i = 0; i < count; i++) {
	CFStringRef	this_name = CFArrayGetValueAtIndex(trusted_server_names,
							   i);
	if (CFEqual(name, this_name)) {
	    trusted = TRUE;
	    break;
	}
	if (CFStringHasPrefix(this_name, CFSTR("*."))) {
	    bool		match = FALSE;
	    CFMutableStringRef	suffix;

	    suffix = CFStringCreateMutableCopy(NULL, 0, this_name);
	    CFStringDelete(suffix, CFRangeMake(0, 1)); /* remove dot */
	    if (CFStringHasSuffix(name, suffix)) {
		match = TRUE;
	    }
	    CFRelease(suffix);
	    if (match) {
		trusted = TRUE;
		break;
	    }
	}
    }
    return (trusted);
}

static bool
server_cert_matches_server_names(SecCertificateRef cert,
				 CFArrayRef trusted_server_names)
{
    CFDictionaryRef	attrs;
    bool		match = FALSE;
    CFStringRef		name;

    attrs = EAPSecCertificateCopyAttributesDictionary(cert);
    if (attrs == NULL) {
	goto done;
    }
    name = CFDictionaryGetValue(attrs, kEAPSecCertificateAttributeCommonName);
    if (name == NULL) {
	goto done;
    }
    match = server_name_matches_server_names(name, trusted_server_names);

 done:
    my_CFRelease(&attrs);
    return (match);
}

/*
 * Function: verify_server_certs
 * Purpose:
 *   If the trusted_server_names list is specified, verify that the server
 *   cert name matches.  Similarly, if the trusted_certs list is specified,
 *   make sure that one of the certs in the cert chain in the SecTrust object
 *   is in the trusted_certs list.
 * Notes:
 *   The assumption here is that TrustSettings are already in place, and
 *   we perform additional checks to validate the cert chain on top of that.
 */
static bool
verify_server_certs(SecTrustRef trust,
		    CFArrayRef server_certs,
		    CFArrayRef trusted_certs,
		    CFArrayRef trusted_server_names)
{
    if (trusted_server_names != NULL) {
	SecCertificateRef	cert;

	cert = (SecCertificateRef)CFArrayGetValueAtIndex(server_certs, 0);
	if (server_cert_matches_server_names(cert, trusted_server_names)
	    == FALSE) {
	    return (FALSE);
	}
    }
    if (trusted_certs != NULL
	&& server_cert_chain_is_trusted(trust, trusted_certs) == FALSE) {
	return (FALSE);
    }
    return (TRUE);
}

EAPClientStatus
EAPTLSVerifyServerCertificateChain(CFDictionaryRef properties, 
				   CFArrayRef server_certs, 
				   OSStatus * ret_status)
{
    bool		allow_trust_decisions;
    EAPClientStatus	client_status;
    int			count;
    bool		is_recoverable;
    SecPolicyRef	policy = NULL;
    CFStringRef		profileID;
    OSStatus		status = noErr;
    SecTrustRef		trust = NULL;
    SecTrustResultType 	trust_result;
    CFArrayRef		trusted_certs = NULL;
    CFArrayRef		trusted_server_names;

    client_status = kEAPClientStatusInternalError;

    /* don't bother verifying server's identity */
    if (my_CFDictionaryGetBooleanValue(properties, 
				       kEAPClientPropTLSVerifyServerCertificate,
				       TRUE) == FALSE) {
	client_status = kEAPClientStatusOK;
    }
    else {
	CFArrayRef	proceed;

	proceed = copy_user_trust_proceed_certs(properties);
	if (proceed != NULL
	    && CFEqual(proceed, server_certs)) {
	    /* user said it was OK to go */
	    client_status = kEAPClientStatusOK;
	}
	my_CFRelease(&proceed);
    }
    if (client_status == kEAPClientStatusOK) {
	goto done;
    }
    if (server_certs == NULL) {
	goto done;
    }
    count = CFArrayGetCount(server_certs);
    if (count == 0) {
	goto done;
    }
    profileID = CFDictionaryGetValue(properties, kEAPClientPropProfileID);
    trusted_certs = copy_cert_list(properties,
				   kEAPClientPropTLSTrustedCertificates);
    trusted_server_names = get_trusted_server_names(properties);

    /*
     * Don't allow trust decisions by the user by default if either trusted
     * certs or trusted server names is specified. Trust decisions must be
     * explicitly enabled in that case using the 
     * kEAPClientPropTLSAllowTrustDecisions property.
     */
    if (trusted_certs != NULL || trusted_server_names != NULL) {
	allow_trust_decisions = FALSE;
    }
    else {
	allow_trust_decisions = TRUE;
    }
    allow_trust_decisions
	= my_CFDictionaryGetBooleanValue(properties, 
					 kEAPClientPropTLSAllowTrustDecisions,
					 allow_trust_decisions);
    client_status = kEAPClientStatusSecurityError;
    status = EAPSecPolicyCopy(&policy);
    if (status != noErr) {
	goto done;
    }
    status = SecTrustCreateWithCertificates(server_certs, policy, &trust);
    if (status != noErr) {
	syslog(LOG_NOTICE, 
	       "EAPTLSVerifyServerCertificateChain: "
	       "SecTrustCreateWithCertificates failed, %s (%d)",
	       EAPSecurityErrorString(status), (int)status);
	goto done;
    }
    if (profileID != NULL && trusted_certs != NULL) {
	status = SecTrustSetAnchorCertificates(trust, trusted_certs);
	if (status != noErr) {
	    syslog(LOG_NOTICE, 
		   "_EAPTLSCreateSecTrust:"
		   " SecTrustSetAnchorCertificates failed, %s (%d)",
		   EAPSecurityErrorString(status), (int)status);
	    goto done;
	}
    }
    status = SecTrustEvaluate(trust, &trust_result);
    switch (status) {
    case noErr:
	break;
    case errSecNoDefaultKeychain:
	status = SecKeychainSetPreferenceDomain(kSecPreferencesDomainSystem);
	if (status != noErr) {
	    syslog(LOG_NOTICE, 
		   "EAPTLSVerifyServerCertificateChain: "
		   "SecKeychainSetPreferenceDomain failed, %s (%d)",
		   EAPSecurityErrorString(status), (int)status);
	    goto done;
	}
	status = SecTrustEvaluate(trust, &trust_result);
	if (status == noErr) {
	    break;
	}
	/* FALL THROUGH */
    default:
	syslog(LOG_NOTICE, 
	       "EAPTLSVerifyServerCertificateChain: "
	       "SecTrustEvaluate failed, %s (%d)",
	       EAPSecurityErrorString(status), (int)status);
	goto done;
	break;
    }
    is_recoverable = FALSE;
    switch (trust_result) {
    case kSecTrustResultProceed:
	if (verify_server_certs(trust, server_certs, trusted_certs,
				trusted_server_names)) {
	    /* the chain and/or name is valid */
	    client_status = kEAPClientStatusOK;
	    break;
	}
	is_recoverable = TRUE;
	break;
    case kSecTrustResultUnspecified:
	if (profileID != NULL
	    && (trusted_certs != NULL || trusted_server_names != NULL)) {
	    /* still need to check server names */
	    if (trusted_server_names != NULL) {
		if (verify_server_certs(NULL, server_certs, NULL,
					trusted_server_names)) {
		    client_status = kEAPClientStatusOK;
		    break;
		}
	    }
	    else {
		client_status = kEAPClientStatusOK;
		break;
	    }
	}
	is_recoverable = TRUE;
	break;
    case kSecTrustResultRecoverableTrustFailure:
	is_recoverable = TRUE;
	break;
    case kSecTrustResultDeny:
    default:
	status = errSSLXCertChainInvalid;
	break;
    }
    if (is_recoverable) {
	if (allow_trust_decisions == FALSE) {
	    client_status = kEAPClientStatusServerCertificateNotTrusted;
	}
	else {
	    client_status = kEAPClientStatusUserInputRequired;
	}
    }

 done:
    if (ret_status != NULL) {
	*ret_status = status;
    }
    my_CFRelease(&policy);
    my_CFRelease(&trust);
    my_CFRelease(&trusted_certs);
    return (client_status);
}

#endif /* TARGET_OS_EMBEDDED */

#if defined(TEST_TRUST_EXCEPTIONS) || defined(TEST_EAPTLSVerifyServerCertificateChain)

#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

static CFDataRef
file_create_data(const char * filename)
{
    CFMutableDataRef	data = NULL;
    size_t		len = 0;
    int			fd = -1;
    struct stat		sb;

    if (stat(filename, &sb) < 0) {
	goto done;
    }
    len = sb.st_size;
    if (len == 0) {
	goto done;
    }
    data = CFDataCreateMutable(NULL, len);
    if (data == NULL) {
	goto done;
    }
    fd = open(filename, O_RDONLY);
    if (fd < 0) {
	goto done;
    }
    CFDataSetLength(data, len);
    if (read(fd, CFDataGetMutableBytePtr(data), len) != len) {
	goto done;
    }
 done:
    if (fd >= 0) {
	close(fd);
    }
    return (data);
}

static SecCertificateRef
file_create_certificate(const char  * filename)
{
    CFDataRef		data;
    SecCertificateRef	cert;

    data = file_create_data(filename);
    if (data == NULL) {
	return (NULL);
    }
    cert = SecCertificateCreateWithData(NULL, data);
    CFRelease(data);
    return (cert);
}
#endif /* defined(TEST_TRUST_EXCEPTIONS) || defined(TEST_EAPTLSVerifyServerCertificateChain) */

#ifdef TEST_TRUST_EXCEPTIONS
#if TARGET_OS_EMBEDDED
#include <SystemConfiguration/SCPrivate.h>

static void
usage(const char * progname)
{
    fprintf(stderr, "usage:\n%s get <domain> <identifier> <cert-file>\n",
	    progname);
    fprintf(stderr, "%s remove_all <domain> <identifier>\n", progname);
    exit(1);
    return;
}
enum {
    kCommandGet,
    kCommandRemoveAll
};

static CFStringRef
file_create_certificate_hash(const char * filename)
{
    SecCertificateRef	cert;
    CFStringRef		str;

    cert = file_create_certificate(filename);
    if (cert == NULL) {
	return (NULL);
    }
    str = EAPSecCertificateCopySHA1DigestString(cert);
    CFRelease(cert);
    return (str);
}

static void
getTrustExceptions(char * domain, char * identifier, char * cert_file)
{
    CFStringRef		domain_cf;
    CFDataRef		exceptions;
    CFStringRef 	identifier_cf;
    CFStringRef		cert_hash;

    domain_cf 
	= CFStringCreateWithCStringNoCopy(NULL, 
					  domain,
					  kCFStringEncodingUTF8,
					  kCFAllocatorNull);
    identifier_cf
	= CFStringCreateWithCStringNoCopy(NULL,
					  identifier,
					  kCFStringEncodingUTF8,
					  kCFAllocatorNull);
    cert_hash = file_create_certificate_hash(cert_file);
    if (cert_hash == NULL) {
	fprintf(stderr, "error reading certificate file '%s', %s\n",
		cert_file, strerror(errno));
	exit(1);
    }
    exceptions = EAPTLSTrustExceptionsCopy(domain_cf,
					   identifier_cf,
					   cert_hash);
    if (exceptions != NULL) {
	SCPrint(TRUE, stdout,
		CFSTR("Exceptions for %@/%@/%@ are defined:\n%@\n"),
		domain_cf, identifier_cf, cert_hash,
		exceptions);
    }
    else {
	SCPrint(TRUE, stdout,
		CFSTR("No exceptions for %@/%@/%@ are defined\n"),
		domain_cf, identifier_cf, cert_hash);
    }
    return;
}

static void
removeAllTrustExceptions(char * domain, char * identifier)
{
    CFStringRef	domain_cf;
    CFStringRef identifier_cf;

    domain_cf 
	= CFStringCreateWithCStringNoCopy(NULL, 
					  domain,
					  kCFStringEncodingUTF8,
					  kCFAllocatorNull);
    identifier_cf
	= CFStringCreateWithCStringNoCopy(NULL,
					  identifier,
					  kCFStringEncodingUTF8,
					  kCFAllocatorNull);
    EAPTLSRemoveTrustExceptionsBindings(domain_cf, identifier_cf);
    CFRelease(domain_cf);
    CFRelease(identifier_cf);
    return;
}

int
main(int argc, char * argv[])
{
    int		command;

    if (argc < 2) {
	usage(argv[0]);
    }
    if (strcmp(argv[1], "get") == 0) {
	command = kCommandGet;
    }
    else if (strcmp(argv[1], "remove_all") == 0) {    
	command = kCommandRemoveAll;
    }
    else {
	usage(argv[0]);
    }
    switch (command) {
    case kCommandGet:
	if (argc < 5) {
	    usage(argv[0]);
	}
	getTrustExceptions(argv[2], argv[3], argv[4]);
	break;
    case kCommandRemoveAll:
	if (argc < 4) {
	    usage(argv[0]);
	}
	removeAllTrustExceptions(argv[2], argv[3]);
	break;
    }
    exit(0);
    return (0);
}

#else /* TARGET_OS_EMBEDDED */

#error "TrustExceptions are only available with TARGET_OS_EMBEDDED"
#endif /* TARGET_OS_EMBEDDED */
#endif /* TEST_TRUST_EXCEPTIONS */

#ifdef TEST_SEC_TRUST

#if TARGET_OS_EMBEDDED
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <SystemConfiguration/SCPrivate.h>

static CFDictionaryRef
read_dictionary(const char * filename)
{
    CFDictionaryRef dict;

    dict = my_CFPropertyListCreateFromFile(filename);
    if (dict != NULL) {
	if (isA_CFDictionary(dict) == NULL) {
	    CFRelease(dict);
	    dict = NULL;
	}
    }
    return (dict);
}

static CFArrayRef
read_array(const char * filename)
{
    CFArrayRef array;

    array = my_CFPropertyListCreateFromFile(filename);
    if (array != NULL) {
	if (isA_CFArray(array) == NULL) {
	    CFRelease(array);
	    array = NULL;
	}
    }
    return (array);
}

static void
usage(const char * progname)
{
    fprintf(stderr, "usage: %s <domain> <identifier> <config> <certs>\n",
	    progname);
    exit(1);
    return;
}

int
main(int argc, char * argv[])
{
    CFArrayRef		certs;
    CFArrayRef		certs_data;
    CFDictionaryRef	config;
    const char *	domain;
    CFStringRef		domain_cf;
    const char *	identifier;
    CFStringRef 	identifier_cf;
    const char *	result_str;
    OSStatus		status;
    SecTrustRef		trust;
    SecTrustResultType 	trust_result;

    if (argc < 5) {
	usage(argv[0]);
    }
    domain = argv[1];
    identifier = argv[2];
    config = read_dictionary(argv[3]);
    if (config == NULL) {
	fprintf(stderr, "failed to load '%s'\n", 
		argv[3]);
	exit(1);
    }
    certs_data = read_array(argv[4]);
    if (certs_data == NULL) {
	fprintf(stderr, "failed to load '%s'\n", 
		argv[4]);
	exit(1);
    }
    certs = EAPCFDataArrayCreateSecCertificateArray(certs_data);
    if (certs == NULL) {
	fprintf(stderr,
		"the file '%s' does not contain a certificate array data\n",
		argv[4]);
	exit(1);
    }
    domain_cf 
	= CFStringCreateWithCStringNoCopy(NULL, 
					  domain,
					  kCFStringEncodingUTF8,
					  kCFAllocatorNull);
    identifier_cf
	= CFStringCreateWithCStringNoCopy(NULL,
					  identifier,
					  kCFStringEncodingUTF8,
					  kCFAllocatorNull);
    trust = EAPTLSCreateSecTrust(config, certs, domain_cf, identifier_cf);
    if (trust == NULL) {
	fprintf(stderr, "EAPTLSCreateSecTrustFailed failed\n");
	exit(1);
    }
    status = SecTrustEvaluate(trust, &trust_result);
    if (status != noErr) {
	fprintf(stderr, "SecTrustEvaluate failed, %s (%d)",
		EAPSecurityErrorString(status), (int)status);
	exit(1);
    }
    switch (trust_result) {
    case kSecTrustResultProceed:
	result_str = "Proceed";
	break;
    case kSecTrustResultUnspecified:
	result_str = "Unspecified";
	break;
    case kSecTrustResultRecoverableTrustFailure:
	result_str = "RecoverableTrustFailure";
	break;
    case kSecTrustResultDeny:
	result_str = "Deny";
	break;
    default:
	result_str = "<unknown>";
	break;
    }
    printf("Trust result is %s\n", result_str);
    CFRelease(domain_cf);
    CFRelease(identifier_cf);
    CFRelease(certs_data);
    CFRelease(certs);
    CFRelease(trust);
    exit(0);
    return (0);
}

#else /* TARGET_OS_EMBEDDED */

#error "SecTrust test is only available with TARGET_OS_EMBEDDED"
#endif /* TARGET_OS_EMBEDDED */
#endif /* TEST_SEC_TRUST */

#ifdef TEST_SERVER_NAMES
#if TARGET_OS_EMBEDDED
#error "Can't test server names with TARGET_OS_EMBEDDED"
#else /* TARGET_OS_EMBEDDED */

#include <SystemConfiguration/SCPrivate.h>

int
main()
{
    const void *	name_list[] = {
	CFSTR("siegdi.apple.com"),
	CFSTR("radius1.testing123.org"),
	CFSTR("apple.com"),
	CFSTR("radius1.foo.bar.nellie.joe.edu"),
	NULL 
    };
    int			i;
    CFStringRef		match1 = CFSTR("*.apple.com");
    CFStringRef		match2 = CFSTR("*.testing123.org");
    CFStringRef		match3 = CFSTR("*.foo.bar.nellie.joe.edu");
    CFStringRef		match4 = CFSTR("apple.com");
    const void *	vlist[3] = { match1, match2, match4 };
    CFArrayRef		list[3] = { NULL, NULL, NULL };

    list[0] = CFArrayCreate(NULL, (const void **)&match1,
			    1, &kCFTypeArrayCallBacks);
    list[1] = CFArrayCreate(NULL, vlist, 3, &kCFTypeArrayCallBacks);
    list[2] = CFArrayCreate(NULL, (const void **)&match3, 1,
			    &kCFTypeArrayCallBacks);
    SCPrint(TRUE, stdout, CFSTR("list[0] = %@\n"), list[0]);
    SCPrint(TRUE, stdout, CFSTR("list[1] = %@\n"), list[1]);
    SCPrint(TRUE, stdout, CFSTR("list[2] = %@\n"), list[2]);
    for (i = 0; name_list[i] != NULL; i++) {
	int	j;
	for (j = 0; j < 3; j++) {
	    if (server_name_matches_server_names(name_list[i],
						 list[j])) {
		SCPrint(TRUE, stdout, CFSTR("%@ matches list[%d]\n"),
			name_list[i], j);
	    }
	    else {
		SCPrint(TRUE, stdout, CFSTR("%@ does not match list[%d]\n"),
			name_list[i], j);
	    }
	}
    }
    exit(0);
    return (0);
}

#endif /* TARGET_OS_EMBEDDED */
#endif /* TEST_SERVER_NAMES */

#ifdef TEST_EAPTLSVerifyServerCertificateChain

static CFDictionaryRef
read_dictionary(const char * filename)
{
    CFDictionaryRef dict;

    dict = my_CFPropertyListCreateFromFile(filename);
    if (dict != NULL) {
	if (isA_CFDictionary(dict) == NULL) {
	    CFRelease(dict);
	    dict = NULL;
	}
    }
    return (dict);
}

int
main(int argc, char * argv[])
{
    CFArrayRef		array;
    CFDictionaryRef	properties;
    OSStatus		sec_status;
    SecCertificateRef	server_cert;
    EAPClientStatus	status;

    if (argc < 3) {
	fprintf(stderr, "usage: verify_server <cert-file> <properties>\n");
	exit(1);
    }
    server_cert = file_create_certificate(argv[1]);
    if (server_cert == NULL) {
	fprintf(stderr, "failed to load cert file\n");
	exit(2);
    }
    properties = read_dictionary(argv[2]);
    if (properties == NULL) {
	fprintf(stderr, "failed to load properties\n");
	exit(2);
    }
    array = CFArrayCreate(NULL, (const void **)&server_cert, 1,
			  &kCFTypeArrayCallBacks);
    
    status = EAPTLSVerifyServerCertificateChain(properties, 
						array,
						&sec_status);
    printf("status is %d, sec status is %d\n", 
	   status, sec_status);
    exit(0);
}
#endif /* TEST_EAPTLSVerifyServerCertificateChain */
