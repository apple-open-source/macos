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

#if defined(HAVE_FRAMEWORK_SECURITY)
#include <Security/Security.h>
#include <Security/SecKeyPriv.h>


struct kc_rsa {
    SecKeyRef pkey;
    size_t keysize;
    CFTypeRef authContext;
};


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

static SecKeyRef
createKeyDuplicateWithAuthContext(SecKeyRef origKey, CFTypeRef authContext)
{
    SecKeyRef key = SecKeyCreateDuplicate(origKey);
    bool allowAuthenticationUI = false;

    if (authContext) {
        if (CFGetTypeID(authContext) == CFBooleanGetTypeID()) {
            if (CFBooleanGetValue((CFBooleanRef)authContext)) {
                allowAuthenticationUI = true;
            }
        } else { // otherwise we expect it's a LA context
            SecKeySetParameter(key, kSecUseAuthenticationContext, authContext, NULL);
            allowAuthenticationUI = true;
        }
    }

    if (!allowAuthenticationUI) {
        SecKeySetParameter(key, kSecUseAuthenticationUI, kSecUseAuthenticationUIFail, NULL);
    }

    return key;
}

static int
kc_rsa_sign(int type, const unsigned char *from, unsigned int flen,
            unsigned char *to, unsigned int *tlen, const RSA *rsa)
{
    struct kc_rsa *kc = RSA_get_app_data(rk_UNCONST(rsa));

    SecKeyRef privKeyRef = kc->pkey;
    SecKeyRef key;
    CFTypeRef authContext = kc->authContext;
    size_t klen = kc->keysize;
    CFDataRef sig, in;
    int fret = 0;
    SecKeyAlgorithm stype;

    if (type == NID_md5) {
        stype = kSecKeyAlgorithmRSASignatureDigestPKCS1v15MD5;
    } else if (type == NID_sha1) {
        stype = kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA1;
    } else if (type == NID_sha256) {
        stype = kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA256;
    } else if (type == NID_sha384) {
        stype = kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA384;
    } else if (type == NID_sha512) {
        stype = kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA512;
    } else
        return -1;

    key = createKeyDuplicateWithAuthContext(privKeyRef, authContext);

    in = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)from, flen);
    sig = SecKeyCreateSignature(key, stype, in, NULL);
    size_t slen = sig ? CFDataGetLength(sig) : 0;

    if (sig && slen <= klen) {
        fret = 1;
        *tlen = (unsigned int)slen;
        memcpy(to, CFDataGetBytePtr(sig), slen);
    } else {
        fret = -1;
    }

    if (key) {
        CFRelease(key);
    }

    if (in) {
        CFRelease(in);
    }

    if (sig) {
        CFRelease(sig);
    }

    return fret;
}

static int
kc_rsa_private_encrypt(int flen,
                       const unsigned char *from,
                       unsigned char *to,
                       RSA *rsa,
                       int padding)
{
    struct kc_rsa *kc = RSA_get_app_data(rsa);

    SecKeyRef privKeyRef = kc->pkey;
    SecKeyRef key;
    CFTypeRef authContext = kc->authContext;
    size_t klen = kc->keysize;
    CFDataRef sig, in;
    int fret = 0;

    if (padding != RSA_PKCS1_PADDING)
        return -1;

    key = createKeyDuplicateWithAuthContext(privKeyRef, authContext);

    in = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)from, flen);
    sig = SecKeyCreateSignature(key, kSecKeyAlgorithmRSASignatureRaw, in, NULL);
    size_t slen = sig ? CFDataGetLength(sig) : 0;

    if (sig && slen <= klen) {
        fret = (int)slen;
        memcpy(to, CFDataGetBytePtr(sig), slen);
    } else {
        fret = -1;
    }

    if (key) {
        CFRelease(key);
    }

    if (in) {
        CFRelease(in);
    }

    if (sig) {
        CFRelease(sig);
    }

    return fret;
}

static int
kc_rsa_private_decrypt(int flen, const unsigned char *from, unsigned char *to,
                       RSA * rsa, int padding)
{
    struct kc_rsa *kc = RSA_get_app_data(rsa);

    SecKeyRef privKeyRef = kc->pkey;
    SecKeyRef key;
    CFTypeRef authContext = kc->authContext;
    size_t klen = kc->keysize;
    CFDataRef out, in;
    int fret = 0;

    if (padding != RSA_PKCS1_PADDING)
        return -1;

    key = createKeyDuplicateWithAuthContext(privKeyRef, authContext);

    in = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)from, flen);
    out = SecKeyCreateDecryptedData(key, kSecKeyAlgorithmRSAEncryptionPKCS1, in, NULL);
    size_t olen = out ? CFDataGetLength(out) : 0;

    if (out && olen <= klen) {
        fret = (int)olen;
        memcpy(to, CFDataGetBytePtr(out), olen);
    } else {
        fret = -1;
    }

    if (key) {
        CFRelease(key);
    }

    if (in) {
        CFRelease(in);
    }

    if (out) {
        CFRelease(out);
    }

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
	if (kc_rsa->authContext) {
	    CFRelease(kc_rsa->authContext);
	}
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
set_private_key(hx509_context context, hx509_cert cert, SecKeyRef pkey, void *authContext)
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

    if (authContext) {
	CFRetain(authContext);
	kc->authContext = authContext;
    }

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
#ifndef __APPLE_TARGET_EMBEDDED__
    SecKeychainRef keychain;
#endif
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

    if (residue && residue[0]) {
#ifndef __APPLE_TARGET_EMBEDDED__
	if (strcasecmp(residue, "system-anchors") == 0) {
	    ctx->anchors = 1;
	} else if (strncasecmp(residue, "FILE:", 5) == 0) {
	    OSStatus ret;

	    ret = SecKeychainOpen(residue + 5, &ctx->keychain);
	    if (ret != noErr) {
		free(ctx);
		hx509_set_error_string(context, 0, ENOENT,
				       "Failed to open %s", residue);
		return ENOENT;
	    }
	} else
#endif
	{
	    free(ctx);
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
    if (ctx) {
#ifndef __APPLE_TARGET_EMBEDDED__
        if (ctx->keychain)
            CFRelease(ctx->keychain);
#endif
        memset(ctx, 0, sizeof(*ctx));
        free(ctx);
    }
    return 0;
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

#ifndef __APPLE_TARGET_EMBEDDED__

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
	} else
#endif
        {

	    ret = hx509_cert_init_SecFramework(context, (void *)secitem, &cert);
	    if (ret)
		continue;
	}

	if (_hx509_query_match_cert(context, query, cert)) {

#ifndef __APPLE_TARGET_EMBEDDED__
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
#endif
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
#ifndef __APPLE_TARGET_EMBEDDED__
    struct ks_keychain *ctx = data;
#endif
    struct iter *iter;

    iter = calloc(1, sizeof(*iter));
    if (iter == NULL) {
	hx509_set_error_string(context, 0, ENOMEM, "out of memory");
	return ENOMEM;
    }

#ifndef __APPLE_TARGET_EMBEDDED__
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
    } else
#endif
    {
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
#if defined(HAVE_FRAMEWORK_SECURITY)
    _hx509_ks_register(context, &keyset_keychain);
#endif
}

static void
kc_cert_release(hx509_cert cert, void *ctx)
{
    SecCertificateRef seccert = ctx;
    CFRelease(seccert);
}

/*
 *
 */


static void
setPersistentRef(hx509_cert cert, SecCertificateRef itemRef)
{
#if !__APPLE_TARGET_EMBEDDED__
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
#endif
}


int
hx509_cert_init_SecFrameworkAuth(hx509_context context, void * identity, hx509_cert *cert, void *authContext)
{
    CFTypeID typeid = CFGetTypeID(identity);
    SecCertificateRef seccert;
    CFTypeRef secdata = NULL;
    SecKeyRef pkey = NULL;
    CFDataRef data;
    OSStatus osret;
    hx509_cert c;
    int ret;

    *cert = NULL;

    if (CFDataGetTypeID() == typeid) {
	void const * keys[4] =  {
	    kSecClass,
	    kSecReturnRef,
	    kSecMatchLimit,
	    kSecValuePersistentRef
	};
	void const * values[4] = {
	    kSecClassIdentity,
	    kCFBooleanTrue,
	    kSecMatchLimitOne,
	    identity
	};
	CFDictionaryRef query;

	assert(sizeof(keys) == sizeof(values));

	query = CFDictionaryCreate(NULL, keys, values,
				   sizeof(keys) / sizeof(*keys),
				   &kCFTypeDictionaryKeyCallBacks,
				   &kCFTypeDictionaryValueCallBacks);

	osret = SecItemCopyMatching(query, &secdata);
	CFRelease(query);
	if (osret || secdata == NULL) {
	    hx509_set_error_string(context, 0, HX509_CERTIFICATE_UNKNOWN_TYPE,
				   "Failed to turn persistent reference into a certifiate: %d", (int)osret);
	    return HX509_CERTIFICATE_UNKNOWN_TYPE;
	}

	typeid = CFGetTypeID(secdata);
	identity = (void *)secdata;
    }

    if (SecIdentityGetTypeID() == typeid) {
	osret = SecIdentityCopyCertificate(identity, &seccert);
	if (osret) {
	    if (secdata)
		CFRelease(secdata);
	    hx509_set_error_string(context, 0, HX509_CERTIFICATE_UNKNOWN_TYPE,
				   "Failed to convert the identity to a certificate: %d", (int)osret);
	    return HX509_CERTIFICATE_UNKNOWN_TYPE;
	}
    } else if (SecCertificateGetTypeID() == typeid) {
	seccert = (SecCertificateRef)identity;
	CFRetain(seccert);
    } else {
	if (secdata)
	    CFRelease(secdata);
	hx509_set_error_string(context, 0, HX509_CERTIFICATE_UNKNOWN_TYPE,
			       "Data from persistent ref not a identity or certificate");
	return HX509_CERTIFICATE_UNKNOWN_TYPE;
    }

    data = SecCertificateCopyData(seccert);
    if (data == NULL) {
	if (secdata)
	    CFRelease(secdata);
	CFRelease(seccert);
	return ENOMEM;
    }

    ret = hx509_cert_init_data(context, CFDataGetBytePtr(data),
			       CFDataGetLength(data), &c);
    CFRelease(data);
    if (ret) {
	if (secdata)
	    CFRelease(secdata);
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
	set_private_key(context, c, pkey, authContext);
	CFRelease(pkey);
    }

    _hx509_cert_set_release(c, kc_cert_release, seccert);

    if (secdata)
	CFRelease(secdata);

    *cert = c;

    return 0;
}

int
hx509_cert_init_SecFramework(hx509_context context, void * identity, hx509_cert *cert)
{
    return hx509_cert_init_SecFrameworkAuth(context, identity, cert, NULL);
}
