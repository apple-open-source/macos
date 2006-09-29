
/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFData.h>
#include <Security/SecureTransportPriv.h>
#include <Security/oidsalg.h>
#include <Security/SecKeychain.h>
#include <Security/SecPolicySearch.h>
#include <Security/SecPolicy.h>
#include "EAPTLSUtil.h"
#include "EAPSecurity.h"
#include "printdata.h"
#include "myCFUtil.h"

/* set a 12-hour session cache timeout */
#define kEAPTLSSessionCacheTimeoutSeconds	(12 * 60 * 60)

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

#endif NOTYET

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
	    first = (EAPTLSLengthIncludedPacket *)eaptls;
	    eaptls->flags |= kEAPTLSPacketFlagsLengthIncluded;
	    *((u_int32_t *)first->tls_message_length) = htonl(buf->length);
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
    CFArrayRef	list;
    OSStatus 	status;

    status = SSLGetPeerCertificates(context, certs);
    list = *certs;
    if (status == noErr && list != NULL) {
	int	count = CFArrayGetCount(list);
	int	i;
	
	for (i = 0; i < count; i++) {
	    CFRelease(CFArrayGetValueAtIndex(list, i));
	}
    }
    return (status);
}

OSStatus
EAPSecPolicyCopy(SecPolicyRef * ret_policy)
{
    SecPolicyRef	policy = NULL;
    SecPolicySearchRef	policy_search = NULL;
    OSStatus		status;

    *ret_policy = NULL;
    status = SecPolicySearchCreate(CSSM_CERT_X_509v3,
				   &CSSMOID_APPLE_TP_EAP, NULL, &policy_search);
    if (status != noErr) {
	goto done;
    }
    status = SecPolicySearchCopyNext(policy_search, &policy);
    if (status != noErr) {
	goto done;
    }
    *ret_policy = policy;
 done:
    my_CFRelease(&policy_search);
    return (status);
}

#include <Security/SecTrustPriv.h>
#include <EAP8021X/EAPClientProperties.h>
#include <EAP8021X/EAPCertificateUtil.h>
#include <SystemConfiguration/SCValidation.h>

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
my_CFArrayCreateByAppendingArrays(CFArrayRef array1, CFArrayRef array2)
{
    int			count;
    CFMutableArrayRef	new_array;
    CFArrayRef		ret;

    new_array = CFArrayCreateMutableCopy(NULL, 0, array1);
    count = CFArrayGetCount(array2);
    CFArrayAppendArray(new_array, array2,
		       CFRangeMake(0, count));
    ret = CFArrayCreateCopy(NULL, new_array);
    my_CFRelease(&new_array);
    return (ret);
}

EAPClientStatus
EAPTLSVerifyServerCertificateChain(CFDictionaryRef properties, 
				   int32_t trust_proceed_id,
				   CFArrayRef server_certs, 
				   OSStatus * ret_status)
{
    bool		allow_any_root;
    EAPClientStatus	client_status;
    int			count;
    CSSM_RETURN		crtn;
    SecPolicyRef	policy = NULL;
    CFNumberRef		proceed_cf;
    bool		replace_roots = FALSE;
    SecCertificateRef	root_cert;
    OSStatus		status;
    SecTrustRef		trust = NULL;
    SecTrustResultType 	trust_result;
    SecTrustUserSetting	trust_setting;
    CFArrayRef		trusted_roots = NULL;

    *ret_status = 0;

    /* don't bother verifying server's identity */
    if (my_CFDictionaryGetBooleanValue(properties, 
				       kEAPClientPropTLSVerifyServerCertificate,
				       TRUE) == FALSE) {
	client_status = kEAPClientStatusOK;
	goto done;
    }

    proceed_cf = CFDictionaryGetValue(properties, 
				      kEAPClientPropTLSUserTrustProceed);
    if (isA_CFNumber(proceed_cf) != NULL) {
	int32_t		proceed;

	if (CFNumberGetValue(proceed_cf, kCFNumberSInt32Type, &proceed)) {
	    if (trust_proceed_id == proceed) {
		/* user said it was OK to go */
		client_status = kEAPClientStatusOK;
		goto done;
	    }
	}
    }

    if (server_certs == NULL) {
	client_status = kEAPClientStatusInternalError;
	goto done;
    }
    count = CFArrayGetCount(server_certs);
    if (count == 0) {
	client_status = kEAPClientStatusInternalError;
	goto done;
    }
    allow_any_root
	= my_CFDictionaryGetBooleanValue(properties, 
					 kEAPClientPropTLSAllowAnyRoot,
					 FALSE);
    replace_roots
	= my_CFDictionaryGetBooleanValue(properties,
					 kEAPClientPropTLSReplaceTrustedRootCertificates,
					 FALSE);
    client_status = kEAPClientStatusSecurityError;
    status = EAPSecPolicyCopy(&policy);
    if (status != noErr) {
	*ret_status = status;
	goto done;
    }
    status = SecTrustCreateWithCertificates(server_certs, policy, &trust);
    if (status != noErr) {
	*ret_status = status;
	syslog(LOG_NOTICE, 
	       "EAPTLSVerifyServerCertificateChain: "
	       "SecTrustCreateWithCertificates failed, %s (%d)",
	       EAPSecurityErrorString(status), status);
	goto done;
    }
    root_cert = (const SecCertificateRef)
	CFArrayGetValueAtIndex(server_certs, count - 1);
    if (replace_roots == FALSE) {
	CFDictionaryRef		attrs;
	bool			is_root = FALSE;

	attrs = EAPSecCertificateCopyAttributesDictionary(root_cert);
	if (attrs != NULL) {
	    is_root =
		my_CFDictionaryGetBooleanValue(attrs,
					       kEAPSecCertificateAttributeIsRoot,
					       FALSE);
	}
	my_CFRelease(&attrs);
	if (is_root) {
	    status = SecTrustGetUserTrust(root_cert, policy, &trust_setting);
	    if (status == noErr 
		&& trust_setting == kSecTrustResultProceed) {
		trusted_roots = CFArrayCreate(NULL, 
					      (const void **)&root_cert,
					      1, &kCFTypeArrayCallBacks);
	    }
	}
    }
    if (CFDictionaryContainsValue(properties, 
				  kEAPClientPropTLSReplaceTrustedRootCertificates)) {
	CFArrayRef	more_trusted_roots;

	more_trusted_roots
	    = copy_cert_list(properties,
			     kEAPClientPropTLSTrustedRootCertificates);
	if (more_trusted_roots != NULL) {
	    if (trusted_roots == NULL) {
		trusted_roots = more_trusted_roots;
	    }
	    else {
		CFArrayRef	new_trusted_roots;
		new_trusted_roots 
		    = my_CFArrayCreateByAppendingArrays(more_trusted_roots,
							trusted_roots);
		my_CFRelease(&more_trusted_roots);
		my_CFRelease(&trusted_roots);
		trusted_roots = new_trusted_roots;
	    }
	}
    }
    if (trusted_roots != NULL && replace_roots == FALSE) {
	CFArrayRef		existing = NULL;

	status = SecTrustCopyAnchorCertificates(&existing);
	if (status != noErr) {
	    syslog(LOG_NOTICE, 
		   "EAPTLSVerifyServerCertificateChain: "
		   "SecTrustCopyAnchorCertificates failed, %s (%d)",
		   EAPSecurityErrorString(status), status);
	}
	if (existing != NULL) { /* merge the two lists */
	    CFArrayRef	new_trusted_roots;
	    new_trusted_roots 
		= my_CFArrayCreateByAppendingArrays(existing,
						    trusted_roots);
	    my_CFRelease(&trusted_roots);
	    my_CFRelease(&existing);
	    trusted_roots = new_trusted_roots;
	}
    }
    if (trusted_roots != NULL) {
	status = SecTrustSetAnchorCertificates(trust, trusted_roots);
	if (status != noErr) {
	    syslog(LOG_NOTICE, 
		   "EAPTLSVerifyServerCertificateChain:"
		   " SecTrustSetAnchorCertificates failed, %s (%d)"
		   " - non-fatal",
		   EAPSecurityErrorString(status), status);
	}
    }
    my_CFRelease(&trusted_roots);

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
		   EAPSecurityErrorString(status), status);
	    goto done;
	}
	status = SecTrustEvaluate(trust, &trust_result);
	if (status == noErr) {
	    break;
	}
	/* FALL THROUGH */
    default:
	*ret_status = status;
	syslog(LOG_NOTICE, 
	       "EAPTLSVerifyServerCertificateChain: "
	       "SecTrustEvaluate failed, %s (%d)",
	       EAPSecurityErrorString(status), status);
	goto done;
	break;
    }
    switch (trust_result) {
    case kSecTrustResultProceed:
	/* cert chain valid AND user explicitly trusts this */
	client_status = kEAPClientStatusOK;
	break;
    case kSecTrustResultUnspecified:
	/* cert chain valid, no special UserTrust assignments */
	client_status = kEAPClientStatusServerCertificateNotTrusted;
	break;
    case kSecTrustResultDeny:
	/*
	 * Cert chain may well have verified OK, but user has flagged
	 * one of these certs as untrustable.
	 */
	*ret_status = errSSLXCertChainInvalid;
	break;
    case kSecTrustResultConfirm:
	/*
	 * Cert chain verified OK, but the user asked that we confirm the
	 * selection first.
	 */
	client_status = kEAPClientStatusCertificateRequiresConfirmation;
	break;
    default:
	status = SecTrustGetCssmResultCode(trust, &crtn);
	if (status) {
	    syslog(LOG_NOTICE, "SecTrustGetCssmResultCode failed, %s (%d)",
		   EAPSecurityErrorString(status), status);
	    break;
	}
	/* map CSSM error to SSL error */
	switch (crtn) {
	case 0:
	    client_status = kEAPClientStatusOK;
	    break;
	case CSSMERR_TP_INVALID_ANCHOR_CERT: 
	    /* root found but we don't trust it */
	    if (allow_any_root == FALSE) {
		client_status = kEAPClientStatusUnknownRootCertificate;
	    }
	    else {
		client_status = kEAPClientStatusOK;
	    }
	    break;
	case CSSMERR_TP_NOT_TRUSTED:
	    /* no root, not even in implicit SSL roots */
	    if (allow_any_root == FALSE) {
		client_status = kEAPClientStatusNoRootCertificate;
	    }
	    else {
		client_status = kEAPClientStatusOK;
	    }
	    break;
	case CSSMERR_TP_CERT_EXPIRED:
	    client_status = kEAPClientStatusCertificateExpired;
	    break;
	case CSSMERR_TP_CERT_NOT_VALID_YET:
	    client_status = kEAPClientStatusCertificateNotYetValid;
	    break;
	default:
	    *ret_status = errSSLXCertChainInvalid;
	    break;
	}
    }

 done:
    my_CFRelease(&policy);
    my_CFRelease(&trust);
    return (client_status);
}

