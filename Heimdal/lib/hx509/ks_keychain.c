/*
 * Copyright (c) 2007 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "hx_locl.h"

#if defined(HAVE_FRAMEWORK_SECURITY) && defined(HAVE_CDSA)

#include <Security/Security.h>

/* Missing function decls in pre Leopard */
#ifdef NEED_SECKEYGETCSPHANDLE_PROTO
OSStatus SecKeyGetCSPHandle(SecKeyRef, CSSM_CSP_HANDLE *);
OSStatus SecKeyGetCredentials(SecKeyRef, CSSM_ACL_AUTHORIZATION_TAG,
			      int, const CSSM_ACCESS_CREDENTIALS **);
#define kSecCredentialTypeDefault 0
#define CSSM_SIZE uint32_t
#endif


static int
getAttribute(SecKeychainItemRef itemRef, SecItemAttr item,
	     SecKeychainAttributeList **attrs)
{
    SecKeychainAttributeInfo attrInfo;
    UInt32 attrFormat = 0;
    OSStatus ret;

    *attrs = NULL;

    attrInfo.count = 1;
    attrInfo.tag = &item;
    attrInfo.format = &attrFormat;

    ret = SecKeychainItemCopyAttributesAndData(itemRef, &attrInfo, NULL,
					       attrs, NULL, NULL);
    if (ret)
	return EINVAL;
    return 0;
}


/*
 *
 */

struct kc_rsa {
    SecKeyRef pkey;
    size_t keysize;
};


static int
kc_rsa_sign(int type, const unsigned char *from, unsigned int flen,
	    unsigned char *to, unsigned int *tlen, const RSA *rsa)
{
    struct kc_rsa *kc = RSA_get_app_data(rk_UNCONST(rsa));

    CSSM_RETURN cret;
    OSStatus ret;
    const CSSM_ACCESS_CREDENTIALS *creds;
    SecKeyRef privKeyRef = kc->pkey;
    CSSM_CSP_HANDLE cspHandle;
    const CSSM_KEY *cssmKey;
    CSSM_CC_HANDLE sigHandle = 0;
    CSSM_DATA sig, in;
    int fret = 0;
    CSSM_ALGORITHMS stype;

    if (type == NID_md5) {
	stype = CSSM_ALGID_MD5;
    } else if (type == NID_sha1) {
	stype = CSSM_ALGID_SHA1;
    } else if (type == NID_sha256) {
	stype = CSSM_ALGID_SHA256;
    } else if (type == NID_sha384) {
	stype = CSSM_ALGID_SHA384;
    } else if (type == NID_sha512) {
	stype = CSSM_ALGID_SHA512;
    } else
	return -1;

    cret = SecKeyGetCSSMKey(privKeyRef, &cssmKey);
    if(cret) heim_abort("SecKeyGetCSSMKey failed: %d", cret);

    cret = SecKeyGetCSPHandle(privKeyRef, &cspHandle);
    if(cret) heim_abort("SecKeyGetCSPHandle failed: %d", cret);

    ret = SecKeyGetCredentials(privKeyRef, CSSM_ACL_AUTHORIZATION_SIGN,
			       kSecCredentialTypeNoUI, &creds);
    if(ret) heim_abort("SecKeyGetCredentials failed: %d", (int)ret);

    ret = CSSM_CSP_CreateSignatureContext(cspHandle, CSSM_ALGID_RSA,
					  creds, cssmKey, &sigHandle);
    if(ret) heim_abort("CSSM_CSP_CreateSignatureContext failed: %d", (int)ret);

    in.Data = (uint8 *)from;
    in.Length = flen;

    sig.Data = (uint8 *)to;
    sig.Length = kc->keysize;

    cret = CSSM_SignData(sigHandle, &in, 1, stype, &sig);
    if(cret) {
	/* cssmErrorString(cret); */
	fret = -1;
    } else {
	fret = 1;
	*tlen = sig.Length;
    }

    if(sigHandle)
	CSSM_DeleteContext(sigHandle);

    return fret;
}


static int
kc_rsa_public_encrypt(int flen,
		      const unsigned char *from,
		      unsigned char *to,
		      RSA *rsa,
		      int padding)
{
    return -1;
}

static int
kc_rsa_public_decrypt(int flen,
		      const unsigned char *from,
		      unsigned char *to,
		      RSA *rsa,
		      int padding)
{
    return -1;
}


static int
kc_rsa_private_encrypt(int flen,
		       const unsigned char *from,
		       unsigned char *to,
		       RSA *rsa,
		       int padding)
{
    struct kc_rsa *kc = RSA_get_app_data(rsa);

    CSSM_RETURN cret;
    OSStatus ret;
    const CSSM_ACCESS_CREDENTIALS *creds;
    SecKeyRef privKeyRef = kc->pkey;
    CSSM_CSP_HANDLE cspHandle;
    const CSSM_KEY *cssmKey;
    CSSM_CC_HANDLE sigHandle = 0;
    CSSM_DATA sig, in;
    int fret = 0;

    if (padding != RSA_PKCS1_PADDING)
	return -1;

    cret = SecKeyGetCSSMKey(privKeyRef, &cssmKey);
    if(cret) heim_abort("SecKeyGetCSSMKey failed: %d", cret);

    cret = SecKeyGetCSPHandle(privKeyRef, &cspHandle);
    if(cret) heim_abort("SecKeyGetCSPHandle failed: %d", cret);

    ret = SecKeyGetCredentials(privKeyRef, CSSM_ACL_AUTHORIZATION_SIGN,
			       kSecCredentialTypeNoUI, &creds);
    if(ret) heim_abort("SecKeyGetCredentials failed: %d", (int)ret);

    ret = CSSM_CSP_CreateSignatureContext(cspHandle, CSSM_ALGID_RSA,
					  creds, cssmKey, &sigHandle);
    if(ret) heim_abort("CSSM_CSP_CreateSignatureContext failed: %d", (int)ret);

    in.Data = (uint8 *)from;
    in.Length = flen;

    sig.Data = (uint8 *)to;
    sig.Length = kc->keysize;

    cret = CSSM_SignData(sigHandle, &in, 1, CSSM_ALGID_NONE, &sig);
    if(cret) {
	/* cssmErrorString(cret); */
	fret = -1;
    } else
	fret = sig.Length;

    if(sigHandle)
	CSSM_DeleteContext(sigHandle);

    return fret;
}

static int
kc_rsa_private_decrypt(int flen, const unsigned char *from, unsigned char *to,
		       RSA * rsa, int padding)
{
    struct kc_rsa *kc = RSA_get_app_data(rsa);

    CSSM_RETURN cret;
    OSStatus ret;
    const CSSM_ACCESS_CREDENTIALS *creds;
    SecKeyRef privKeyRef = kc->pkey;
    CSSM_CSP_HANDLE cspHandle;
    const CSSM_KEY *cssmKey;
    CSSM_CC_HANDLE handle = 0;
    CSSM_DATA out, in, rem;
    int fret = 0;
    CSSM_SIZE outlen = 0;
    char remdata[1024];

    if (padding != RSA_PKCS1_PADDING)
	return -1;

    cret = SecKeyGetCSSMKey(privKeyRef, &cssmKey);
    if(cret) heim_abort("SecKeyGetCSSMKey failed: %d", (int)cret);

    cret = SecKeyGetCSPHandle(privKeyRef, &cspHandle);
    if(cret) heim_abort("SecKeyGetCSPHandle failed: %d", (int)cret);

    ret = SecKeyGetCredentials(privKeyRef, CSSM_ACL_AUTHORIZATION_DECRYPT,
			       kSecCredentialTypeNoUI, &creds);
    if(ret) heim_abort("SecKeyGetCredentials failed: %d", (int)ret);

    ret = CSSM_CSP_CreateAsymmetricContext (cspHandle,
					    CSSM_ALGID_RSA,
					    creds,
					    cssmKey,
					    CSSM_PADDING_PKCS1,
					    &handle);
    if(ret) heim_abort("CSSM_CSP_CreateAsymmetricContext failed: %d", (int)ret);

    in.Data = (uint8 *)from;
    in.Length = flen;

    out.Data = (uint8 *)to;
    out.Length = kc->keysize;

    rem.Data = (uint8 *)remdata;
    rem.Length = sizeof(remdata);

    cret = CSSM_DecryptData(handle, &in, 1, &out, 1, &outlen, &rem);
    if(cret) {
	/* cssmErrorString(cret); */
	fret = -1;
    } else
	fret = out.Length;

    if(handle)
	CSSM_DeleteContext(handle);

    return fret;
}

static int
kc_rsa_init(RSA *rsa)
{
    return 1;
}

static int
kc_rsa_finish(RSA *rsa)
{
    struct kc_rsa *kc_rsa = RSA_get_app_data(rsa);
    if (kc_rsa) {
	CFRelease(kc_rsa->pkey);
	free(kc_rsa);
    }
    return 1;
}

static const RSA_METHOD kc_rsa_pkcs1_method = {
    "hx509 Keychain PKCS#1 RSA",
    kc_rsa_public_encrypt,
    kc_rsa_public_decrypt,
    kc_rsa_private_encrypt,
    kc_rsa_private_decrypt,
    NULL,
    NULL,
    kc_rsa_init,
    kc_rsa_finish,
    0,
    NULL,
    kc_rsa_sign,
    NULL
};

static int
set_private_key(hx509_context context, hx509_cert cert, SecKeyRef pkey)
{
    const SubjectPublicKeyInfo *spi;
    const Certificate *c;
    struct kc_rsa *kc;
    RSAPublicKey pk;
    hx509_private_key key;
    size_t size;
    RSA *rsa;
    int ret;

    ret = hx509_private_key_init(&key, NULL, NULL);
    if (ret)
	return ret;

    kc = calloc(1, sizeof(*kc));
    if (kc == NULL)
	_hx509_abort("out of memory");

    CFRetain(pkey);
    kc->pkey = pkey;

    rsa = RSA_new();
    if (rsa == NULL)
	_hx509_abort("out of memory");

    RSA_set_method(rsa, &kc_rsa_pkcs1_method);
    ret = RSA_set_app_data(rsa, kc);
    if (ret != 1)
	_hx509_abort("RSA_set_app_data");

    /*
     * Set up n and e to please RSA_size()
     */

    c = _hx509_get_cert(cert);
    spi = &c->tbsCertificate.subjectPublicKeyInfo;

    ret = decode_RSAPublicKey(spi->subjectPublicKey.data,
			      spi->subjectPublicKey.length / 8,
			      &pk, &size);
    if (ret) {
	RSA_free(rsa);
	return 0;
    }
    rsa->n = _hx509_int2BN(&pk.modulus);
    rsa->e = _hx509_int2BN(&pk.publicExponent);
    free_RSAPublicKey(&pk);

    kc->keysize = BN_num_bytes(rsa->n);

    /*
     *
     */

    hx509_private_key_assign_rsa(key, rsa);
    _hx509_cert_set_key(cert, key);

    hx509_private_key_free(&key);

    return 0;
}

/*
 *
 */

struct ks_keychain {
    int anchors;
    SecKeychainRef keychain;
};

static int
keychain_init(hx509_context context,
	      hx509_certs certs, void **data, int flags,
	      const char *residue, hx509_lock lock)
{
    struct ks_keychain *ctx;

    ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
	hx509_clear_error_string(context);
	return ENOMEM;
    }

    if (residue) {
	if (strcasecmp(residue, "system-anchors") == 0) {
	    ctx->anchors = 1;
	} else if (strncasecmp(residue, "FILE:", 5) == 0) {
	    OSStatus ret;

	    ret = SecKeychainOpen(residue + 5, &ctx->keychain);
	    if (ret != noErr) {
		hx509_set_error_string(context, 0, ENOENT,
				       "Failed to open %s", residue);
		return ENOENT;
	    }
	} else {
	    hx509_set_error_string(context, 0, ENOENT,
				   "Unknown subtype %s", residue);
	    return ENOENT;
	}
    }

    *data = ctx;
    return 0;
}

/*
 *
 */

static int
keychain_free(hx509_certs certs, void *data)
{
    struct ks_keychain *ctx = data;
    if (ctx->keychain)
	CFRelease(ctx->keychain);
    memset(ctx, 0, sizeof(*ctx));
    free(ctx);
    return 0;
}

/*
 *
 */


static void
setPersistentRef(hx509_cert cert, SecCertificateRef itemRef)
{
    CFDataRef persistent;
    OSStatus ret;

    ret = SecKeychainItemCreatePersistentReference((SecKeychainItemRef)itemRef, &persistent);
    if (ret == noErr) {
	heim_octet_string os;

	os.data = rk_UNCONST(CFDataGetBytePtr(persistent));
	os.length = CFDataGetLength(persistent);

	hx509_cert_set_persistent(cert, &os);
	CFRelease(persistent);
    }
}

/*
 *
 */

static int
keychain_query(hx509_context context,
	       hx509_certs certs,
	       void *data,
	       const hx509_query *query,
	       hx509_cert *retcert)
{
    CFArrayRef identities = NULL;
    hx509_cert cert = NULL;
    CFIndex n, count;
    int ret;
    int kdcLookupHack = 0;

    /*
     * First to course filtering using security framework ....
     */

#define FASTER_FLAGS (HX509_QUERY_MATCH_PERSISTENT|HX509_QUERY_PRIVATE_KEY)

    if ((query->match & FASTER_FLAGS) == 0)
	return HX509_UNIMPLEMENTED_OPERATION;

    CFMutableDictionaryRef secQuery = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,  &kCFTypeDictionaryValueCallBacks);

    /*
     * XXX this is so broken, SecItem doesn't find the kdc certificte,
     * and kdc certificates happend to be searched by friendly name,
     * so find that and mundge on the structure.
     */

    if ((query->match & HX509_QUERY_MATCH_FRIENDLY_NAME) &&
	(query->match & HX509_QUERY_PRIVATE_KEY) && 
	strcmp(query->friendlyname, "O=System Identity,CN=com.apple.kerberos.kdc") == 0)
    {
	((hx509_query *)query)->match &= ~HX509_QUERY_PRIVATE_KEY;
	kdcLookupHack = 1;
    }

    if (kdcLookupHack || (query->match & HX509_QUERY_MATCH_PERSISTENT)) {
	CFDictionaryAddValue(secQuery, kSecClass, kSecClassCertificate);
    } else
	CFDictionaryAddValue(secQuery, kSecClass, kSecClassIdentity);

    CFDictionaryAddValue(secQuery, kSecReturnRef, kCFBooleanTrue);
    CFDictionaryAddValue(secQuery, kSecMatchLimit, kSecMatchLimitAll);

    if (query->match & HX509_QUERY_MATCH_PERSISTENT) {
	CFDataRef refdata = CFDataCreateWithBytesNoCopy(NULL, query->persistent->data, query->persistent->length, kCFAllocatorNull);
	CFDictionaryAddValue(secQuery, kSecValuePersistentRef, refdata);
	CFRelease(refdata);
    }


    OSStatus status = SecItemCopyMatching(secQuery, (CFTypeRef *)&identities);
    CFRelease(secQuery);
    if (status || identities == NULL) {
	hx509_clear_error_string(context);
	return HX509_CERT_NOT_FOUND;
    }
    
    heim_assert(CFArrayGetTypeID() == CFGetTypeID(identities), "return value not an array");

    /*
     * ... now do hx509 filtering
     */

    count = CFArrayGetCount(identities);
    for (n = 0; n < count; n++) {
	CFTypeRef secitem = (CFTypeRef)CFArrayGetValueAtIndex(identities, n);

	if (query->match & HX509_QUERY_MATCH_PERSISTENT) {
	    SecIdentityRef other = NULL;
	    OSStatus osret;

	    osret = SecIdentityCreateWithCertificate(NULL, (SecCertificateRef)secitem, &other);
	    if (osret == noErr) {
		ret = hx509_cert_init_SecFramework(context, (void *)other, &cert);
		CFRelease(other);
		if (ret)
		    continue;
	    } else {
		ret = hx509_cert_init_SecFramework(context, (void *)secitem, &cert);
		if (ret)
		    continue;
	    }
	} else {

	    ret = hx509_cert_init_SecFramework(context, (void *)secitem, &cert);
	    if (ret)
		continue;
	}

	if (_hx509_query_match_cert(context, query, cert)) {

	    /* certtool/keychain doesn't glue togheter the cert with keys for system keys */
	    if (kdcLookupHack) {
		SecIdentityRef other = NULL;
		OSStatus osret;

		osret = SecIdentityCreateWithCertificate(NULL, (SecCertificateRef)secitem, &other);
		if (osret == noErr) {
		    hx509_cert_free(cert);
		    ret = hx509_cert_init_SecFramework(context, other, &cert);
		    CFRelease(other);
		    if (ret)
			continue;
		}
	    }	    

	    *retcert = cert;
	    break;
	}
	hx509_cert_free(cert);
    }

    if (kdcLookupHack)
	((hx509_query *)query)->match |= HX509_QUERY_PRIVATE_KEY;

    CFRelease(identities);

    if (*retcert == NULL) {
	hx509_clear_error_string(context);
	return HX509_CERT_NOT_FOUND;
    }

    return 0;
}

/*
 *
 */

struct iter {
    hx509_certs certs;
    void *cursor;
    CFArrayRef search;
    CFIndex index;
};

static int
keychain_iter_start(hx509_context context,
		    hx509_certs certs, void *data, void **cursor)
{
    struct ks_keychain *ctx = data;
    struct iter *iter;

    iter = calloc(1, sizeof(*iter));
    if (iter == NULL) {
	hx509_set_error_string(context, 0, ENOMEM, "out of memory");
	return ENOMEM;
    }

    if (ctx->anchors) {
        CFArrayRef anchors;
	int ret;
	int i;

	ret = hx509_certs_init(context, "MEMORY:ks-file-create",
			       0, NULL, &iter->certs);
	if (ret) {
	    free(iter);
	    return ret;
	}

	ret = SecTrustCopyAnchorCertificates(&anchors);
	if (ret != 0) {
	    hx509_certs_free(&iter->certs);
	    free(iter);
	    hx509_set_error_string(context, 0, ENOMEM,
				   "Can't get trust anchors from Keychain");
	    return ENOMEM;
	}
	for (i = 0; i < CFArrayGetCount(anchors); i++) {
	    SecCertificateRef cr;
	    hx509_cert cert;
	    CFDataRef dataref;

	    cr = (SecCertificateRef)CFArrayGetValueAtIndex(anchors, i);

	    dataref = SecCertificateCopyData(cr);
	    if (dataref == NULL)
		continue;

	    ret = hx509_cert_init_data(context, CFDataGetBytePtr(dataref), CFDataGetLength(dataref), &cert);
	    CFRelease(dataref);
	    if (ret)
		continue;

	    ret = hx509_certs_add(context, iter->certs, cert);
	    hx509_cert_free(cert);
	}
	CFRelease(anchors);
	if (ret != 0) {
	    hx509_certs_free(&iter->certs);
	    free(iter);
	    hx509_set_error_string(context, 0, ret,
				   "Failed to add cert");
	    return ret;
	}
    }

    if (iter->certs) {
	int ret;
	ret = hx509_certs_start_seq(context, iter->certs, &iter->cursor);
	if (ret) {
	    hx509_certs_free(&iter->certs);
	    free(iter);
	    return ret;
	}
    } else {
	OSStatus ret;
	const void *keys[] = {
	    kSecClass,
	    kSecReturnRef,
	    kSecMatchLimit
	};
	const void *values[] = {
	    kSecClassCertificate,
	    kCFBooleanTrue,
	    kSecMatchLimitAll
	};

	CFDictionaryRef secQuery;

	secQuery = CFDictionaryCreate(NULL, keys, values,
				      sizeof(keys) / sizeof(*keys),
				      &kCFTypeDictionaryKeyCallBacks,
				      &kCFTypeDictionaryValueCallBacks);
	
	ret = SecItemCopyMatching(secQuery, (CFTypeRef *)&iter->search);
	CFRelease(secQuery);
	if (ret) {
	    free(iter);
	    return ENOMEM;
	}
    }

    *cursor = iter;
    return 0;
}

/*
 *
 */

static int
keychain_iter(hx509_context context,
	      hx509_certs certs, void *data, void *cursor, hx509_cert *cert)
{
    struct iter *iter = cursor;
    OSStatus ret = 0;

    if (iter->certs)
	return hx509_certs_next_cert(context, iter->certs, iter->cursor, cert);

    *cert = NULL;

next:
    if (iter->index < CFArrayGetCount(iter->search)) {
	
	CFTypeRef secCert = CFArrayGetValueAtIndex(iter->search, iter->index);

	ret = hx509_cert_init_SecFramework(context, (void *)secCert, cert);
	iter->index++;
	if (ret)
	    goto next;
    }
    if (iter->index == CFArrayGetCount(iter->search))
	return 0;

    return ret;
}

/*
 *
 */

static int
keychain_iter_end(hx509_context context,
		  hx509_certs certs,
		  void *data,
		  void *cursor)
{
    struct iter *iter = cursor;

    if (iter->certs) {
	hx509_certs_end_seq(context, iter->certs, iter->cursor);
	hx509_certs_free(&iter->certs);
    } else {
	CFRelease(iter->search);
    }

    memset(iter, 0, sizeof(*iter));
    free(iter);
    return 0;
}

/*
 *
 */

struct hx509_keyset_ops keyset_keychain = {
    "KEYCHAIN",
    0,
    keychain_init,
    NULL,
    keychain_free,
    NULL,
    keychain_query,
    keychain_iter_start,
    keychain_iter,
    keychain_iter_end
};

#endif /* HAVE_FRAMEWORK_SECURITY */

/*
 *
 */

void
_hx509_ks_keychain_register(hx509_context context)
{
#if defined(HAVE_FRAMEWORK_SECURITY) && defined(HAVE_CDSA)
    _hx509_ks_register(context, &keyset_keychain);
#endif
}

#if defined(HAVE_FRAMEWORK_SECURITY) && defined(HAVE_CDSA)
static void
kc_cert_release(hx509_cert cert, void *ctx)
{
    SecCertificateRef seccert = ctx;
    CFRelease(seccert);
}
#endif


int
hx509_cert_init_SecFramework(hx509_context context, void * identity,  hx509_cert *cert)
{
#if defined(HAVE_FRAMEWORK_SECURITY) && defined(HAVE_CDSA)
    CFTypeID typeid = CFGetTypeID(identity);
    SecCertificateRef seccert;
    SecKeyRef pkey = NULL;
    CFDataRef data;
    OSStatus osret;
    hx509_cert c;
    int ret;

    *cert = NULL;

    if (SecIdentityGetTypeID() == typeid) {
	osret = SecIdentityCopyCertificate(identity, &seccert);
	if (osret)
	    return ENOMEM;
    } else if (SecCertificateGetTypeID() == typeid) {
	seccert = (SecCertificateRef)identity;
	CFRetain(seccert);
    } else {
	return EINVAL;
    }

    data = SecCertificateCopyData(seccert);
    if (data == NULL) {
	CFRelease(seccert);
	return ENOMEM;
    }

    ret = hx509_cert_init_data(context, CFDataGetBytePtr(data),
			       CFDataGetLength(data), &c);
    CFRelease(data);
    if (ret) {
	CFRelease(seccert);
	return ret;
    }

    /*
     * Set Persistent identity
     */

    setPersistentRef(c, seccert);

    /* if identity assign private key too */
    if (SecIdentityGetTypeID() == typeid) {
	(void)SecIdentityCopyPrivateKey(identity, &pkey);
    }

    if (pkey) {
	set_private_key(context, c, pkey);
	CFRelease(pkey);
    }

    _hx509_cert_set_release(c, kc_cert_release, seccert);

    *cert = c;

    return 0;
#else
    *cert = NULL;
    return EINVAL;
#endif
}
