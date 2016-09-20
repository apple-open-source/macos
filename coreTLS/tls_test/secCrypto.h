//
//  SecCrypto.h
//  coretls
//
//  Created by Fabrice Gautier on 12/17/13.
//
//

/* Helper functions for coreTLS test */

#ifndef __SEC_CRYPTO_H__
#define __SEC_CRYPTO_H__

#include <tls_handshake.h>
#include <Security/SecTrust.h>

#define CFReleaseNull(CF) { CFTypeRef _cf = (CF); if (_cf) CFRelease(_cf); CF = NULL; }
#define CFReleaseSafe(CF) { CFTypeRef _cf = (CF); if (_cf) CFRelease(_cf);}


extern unsigned char Server1_Cert_rsa_rsa_der[];
extern unsigned Server1_Cert_rsa_rsa_der_len;

extern unsigned char Server1_Key_rsa_der[];
extern unsigned Server1_Key_rsa_der_len;

extern unsigned char Server2_Cert_rsa_rsa_der[];
extern unsigned Server2_Cert_rsa_rsa_der_len;

extern unsigned char Server2_Key_rsa_der[];
extern unsigned Server2_Key_rsa_der_len;

extern unsigned char Server1_Cert_ecc_ecc_der[];
extern unsigned Server1_Cert_ecc_ecc_der_len;

extern unsigned char Server1_Key_ecc_der[];
extern unsigned Server1_Key_ecc_der_len;

extern unsigned char ecclientkey_der[];
extern unsigned int ecclientkey_der_len;

extern unsigned char ecclientcert_der[];
extern unsigned int ecclientcert_der_len;

extern unsigned char eckey_der[];
unsigned int eckey_der_len;

extern unsigned char eccert_der[];
extern unsigned int eccert_der_len;

int tls_evaluate_trust(tls_handshake_t hdsk, bool server);
int tls_set_encrypt_pubkey(tls_handshake_t hdsk, const SSLCertificate *certchain);

int init_server_keys(bool ecdsa,
                     unsigned char *cert_der, size_t cert_der_len,
                     unsigned char *key_der, size_t key_der_len,
                     SSLCertificate *cert, tls_private_key_t *key);

void clean_server_keys(tls_private_key_t key);


#endif
