//
//  ssl-utils.c
//  libsecurity_ssl
//
//  Created by Fabrice Gautier on 8/7/12.
//
//

#include <Security/Security.h>
#include <AssertMacros.h>

#include "ssl-utils.h"

#if TARGET_OS_IPHONE


#include <Security/Security.h>
#include <Security/SecRSAKey.h>
#include <Security/SecECKey.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecIdentityPriv.h>


#include "privkey-1.h"
#include "cert-1.h"

static
CFArrayRef chain_from_der(const unsigned char *cert_der, size_t cert_der_len, const unsigned char *pkey_der, size_t pkey_der_len)
{
    SecKeyRef pkey = NULL;
    SecCertificateRef cert = NULL;
    SecIdentityRef ident = NULL;
    CFArrayRef items = NULL;

    require(pkey = SecKeyCreateRSAPrivateKey(kCFAllocatorDefault, pkey_der, pkey_der_len, kSecKeyEncodingPkcs1), errOut);
    require(cert = SecCertificateCreateWithBytes(kCFAllocatorDefault, cert_der, cert_der_len), errOut);
    require(ident = SecIdentityCreate(kCFAllocatorDefault, cert, pkey), errOut);
    require(items = CFArrayCreate(kCFAllocatorDefault, (const void **)&ident, 1, &kCFTypeArrayCallBacks), errOut);

errOut:
    CFReleaseSafe(pkey);
    CFReleaseSafe(cert);
    CFReleaseSafe(ident);
    return items;
}

#else

#include "identity-1.h"
#define P12_PASSWORD "password"

static
CFArrayRef chain_from_p12(const unsigned char *p12_data, size_t p12_len)
{
    char keychain_path[] = "/tmp/keychain.XXXXXX";

    SecKeychainRef keychain;
    CFArrayRef list;
    CFDataRef data;

    require_noerr(SecKeychainCopyDomainSearchList(kSecPreferencesDomainUser, &list), errOut);
    require(mktemp(keychain_path), errOut);
    require_noerr(SecKeychainCreate (keychain_path, strlen(P12_PASSWORD), P12_PASSWORD,
                                     FALSE, NULL, &keychain), errOut);
    require_noerr(SecKeychainSetDomainSearchList(kSecPreferencesDomainUser, list), errOut);	// restores the previous search list
    require(data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, p12_data, p12_len, kCFAllocatorNull), errOut);

    SecExternalFormat format=kSecFormatPKCS12;
    SecExternalItemType type=kSecItemTypeAggregate;
    SecItemImportExportFlags flags=0;
    SecKeyImportExportParameters params = {0,};
    CFArrayRef out = NULL;

    params.passphrase=CFSTR("password");
    params.keyAttributes = CSSM_KEYATTR_PERMANENT | CSSM_KEYATTR_SENSITIVE;

    require_noerr(SecKeychainItemImport(data, CFSTR(".p12"), &format, &type, flags,
                                        &params, keychain, &out), errOut);

errOut:
    CFReleaseSafe(keychain);
    CFReleaseSafe(list);

    return out;
}

#endif

CFArrayRef server_chain(void)
{
#if TARGET_OS_IPHONE
    return chain_from_der(privkey_1_der, privkey_1_der_len, cert_1_der, cert_1_der_len);
#else
    return chain_from_p12(identity_1_p12, identity_1_p12_len);
#endif
}

CFArrayRef client_chain(void)
{
#if TARGET_OS_IPHONE
    return chain_from_der(privkey_1_der, privkey_1_der_len, cert_1_der, cert_1_der_len);
#else
    return chain_from_p12(identity_1_p12, identity_1_p12_len);
#endif
}


