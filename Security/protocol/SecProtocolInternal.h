//
//  SecProtocolInternal.h
//  Security
//

#ifndef SecProtocolInternal_h
#define SecProtocolInternal_h

#include "SecProtocolPriv.h"

#define kATSInfoKey "NSAppTransportSecurity"
#define kAllowsArbitraryLoads "NSAllowsArbitraryLoads"
#define kAllowsArbitraryLoadsForMedia "NSAllowsArbitraryLoadsForMedia"
#define kAllowsArbitraryLoadsInWebContent "NSAllowsArbitraryLoadsInWebContent"
#define kAllowsLocalNetworking "NSAllowsLocalNetworking"
#define kExceptionDomains "NSExceptionDomains"
#define kIncludesSubdomains "NSIncludesSubdomains"
#define kExceptionAllowsInsecureHTTPLoads "NSExceptionAllowsInsecureHTTPLoads"
#define kExceptionMinimumTLSVersion "NSExceptionMinimumTLSVersion"
#define kExceptionRequiresForwardSecrecy "NSExceptionRequiresForwardSecrecy"
#define _kCIDRExceptions "NSCIDRExceptions"
#define _kATSParsedCIDRAddressKey "NSParsedCIDRAddressKey"
#define _kATSParsedCIDRMaskKey "NSParsedCIDRMaskKey"
#define _kATSParsedCIDRPrefixKey "NSParsedCIDRPrefixKey"

#define CiphersuitesTLS13 \
    TLS_AES_128_GCM_SHA256, \
    TLS_AES_256_GCM_SHA384, \
    TLS_CHACHA20_POLY1305_SHA256

#define CiphersuitesPFS \
    TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384, \
    TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, \
    TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256, \
    TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384, \
    TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256, \
    TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256, \
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA, \
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA, \
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA, \
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA

#define CiphersuitesNonPFS \
    TLS_RSA_WITH_AES_256_GCM_SHA384, \
    TLS_RSA_WITH_AES_128_GCM_SHA256, \
    TLS_RSA_WITH_AES_256_CBC_SHA, \
    TLS_RSA_WITH_AES_128_CBC_SHA

#define CiphersuitesTLS10 \
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA, \
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA, \
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA, \
    TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA, \
    TLS_RSA_WITH_AES_256_CBC_SHA, \
    TLS_RSA_WITH_AES_128_CBC_SHA

#define CiphersuitesTLS10_3DES \
    TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA, \
    TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA, \
    SSL_RSA_WITH_3DES_EDE_CBC_SHA

#define CiphersuitesDHE \
    TLS_DHE_RSA_WITH_AES_256_GCM_SHA384, \
    TLS_DHE_RSA_WITH_AES_128_GCM_SHA256, \
    TLS_DHE_RSA_WITH_AES_256_CBC_SHA256, \
    TLS_DHE_RSA_WITH_AES_128_CBC_SHA256, \
    TLS_DHE_RSA_WITH_AES_256_CBC_SHA, \
    TLS_DHE_RSA_WITH_AES_128_CBC_SHA, \
    SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA

typedef CF_ENUM(uint16_t, kATSGlobalKey) {
    kATSGlobalKeyNotPresent = 0,
    kATSGlobalKeyValueFalse = 1,
    kATSGlobalKeyValueTrue = 2,
};

SEC_RETURNS_RETAINED sec_protocol_configuration_builder_t
sec_protocol_configuration_builder_copy_default(void);

CFDictionaryRef
sec_protocol_configuration_builder_get_ats_dictionary(sec_protocol_configuration_builder_t builder);

bool
sec_protocol_configuration_builder_get_is_apple_bundle(sec_protocol_configuration_builder_t builder);

SEC_RETURNS_RETAINED xpc_object_t
sec_protocol_configuration_get_map(sec_protocol_configuration_t configuration);

tls_protocol_version_t
sec_protocol_configuration_protocol_string_to_version(const char *protocol);

void
sec_protocol_options_clear_tls_ciphersuites(sec_protocol_options_t options);

void
sec_protocol_options_set_ats_non_pfs_ciphersuite_allowed(sec_protocol_options_t options, bool ats_non_pfs_ciphersuite_allowed);

void
sec_protocol_options_set_ats_minimum_tls_version_allowed(sec_protocol_options_t options, bool ats_minimum_tls_version_allowed);

void
sec_protocol_options_set_ats_required(sec_protocol_options_t options, bool required);

void
sec_protocol_options_set_minimum_rsa_key_size(sec_protocol_options_t options, size_t minimum_key_size);

void
sec_protocol_options_set_minimum_ecdsa_key_size(sec_protocol_options_t options, size_t minimum_key_size);

void
sec_protocol_options_set_minimum_signature_algorithm(sec_protocol_options_t options, SecSignatureHashAlgorithm algorithm);

void
sec_protocol_options_set_trusted_peer_certificate(sec_protocol_options_t options, bool trusted_peer_certificate);

SEC_RETURNS_RETAINED _Nullable sec_protocol_options_t
sec_protocol_options_copy(sec_protocol_options_t options);

SEC_RETURNS_RETAINED _Nullable sec_protocol_configuration_t
sec_protocol_options_copy_sec_protocol_configuration(sec_protocol_options_t options);

void
sec_protocol_configuration_populate_insecure_defaults(sec_protocol_configuration_t configuration);

void
sec_protocol_configuration_populate_secure_defaults(sec_protocol_configuration_t configuration);

void
sec_protocol_configuration_register_builtin_exceptions(sec_protocol_configuration_t configuration);

const tls_key_exchange_group_t *
sec_protocol_helper_tls_key_exchange_group_set_to_key_exchange_group_list(tls_key_exchange_group_set_t set, size_t *listSize);

bool
sec_protocol_helper_dispatch_data_equal(dispatch_data_t left, dispatch_data_t right);

bool
client_is_WebKit(void);

SEC_ASSUME_NONNULL_BEGIN

/*!
 * @function sec_identity_create_SPAKE2PLUSV1_registration_record
 *
 * @abstract
 *      Create an ARC-able `dispatch_data_t` instance containing the SPAKE2+
 *      registration record from the input password verifier. The input password verifier
 *      MUST be 80 bytes in length, and store the concatenation of w0s and w1s. See RFC9383
 *      for more information: https://datatracker.ietf.org/doc/html/rfc9383#section-3.2
 *
 * @param password_verifier
 *      A dispatch_data_t containing the SPAKE2+ input password veriifer. This MUST be 80 bytes in length, storing w0s || w1s.
 *
 * @return a `dispatch_data_t` instance.
 */
SPI_AVAILABLE(macos(15.4), ios(18.4), watchos(11.4), tvos(18.4), visionos(2.4))
SEC_RETURNS_RETAINED _Nullable dispatch_data_t
sec_identity_create_SPAKE2PLUSV1_registration_record(dispatch_data_t input_password_verifier);

/*!
 * @function sec_identity_create_SPAKE2PLUSV1_client_password_verifier
 *
 * @abstract
 *      Create the client's password verifier from an input password verifier of length 80 bytes. See RFC9383
 *      for more information: https://datatracker.ietf.org/doc/html/rfc9383#section-3.2
 *
 * @param input_password_verifier
 *      A dispatch_data_t containing the SPAKE2+ input password veriifer. This MUST be 80 bytes in length, storing w0s || w1s.
 *
 * @return a `dispatch_data_t` instance.
 */
SPI_AVAILABLE(macos(15.4), ios(18.4), watchos(11.4), tvos(18.4), visionos(2.4))
SEC_RETURNS_RETAINED _Nullable dispatch_data_t
sec_identity_create_SPAKE2PLUSV1_client_password_verifier(dispatch_data_t input_password_verifier);

/*!
 * @function sec_identity_create_SPAKE2PLUSV1_server_password_verifier
 *
 * @abstract
 *      Create the server's password verifier from an input password verifier of length 80 bytes. See RFC9383
 *      for more information: https://datatracker.ietf.org/doc/html/rfc9383#section-3.2
 *
 * @param input_password_verifier
 *      A dispatch_data_t containing the SPAKE2+ input password veriifer. This MUST be 80 bytes in length, storing w0s || w1s.
 *
 * @return a `dispatch_data_t` instance.
 */
SPI_AVAILABLE(macos(15.4), ios(18.4), watchos(11.4), tvos(18.4), visionos(2.4))
SEC_RETURNS_RETAINED _Nullable dispatch_data_t
sec_identity_create_SPAKE2PLUSV1_server_password_verifier(dispatch_data_t input_password_verifier);

/*!
 * @function sec_identity_create_client_SPAKE2PLUSV1_identity_internal
 *
 * @abstract
 *      Create an ARC-able `sec_identity_t` instance containing the information needed
 *      for a client to authenticate with a server using the SPAKE2PLUS_V1 named PAKE from:
 *      https://chris-wood.github.io/draft-bmw-tls-pake13/draft-bmw-tls-pake13.html
 *
 * @param context
 *      A dispatch_data_t containing the SPAKE2+ context string.
 *
 * @param client_identity
 *      A dispatch_data_t containing the SPAKE2+ client identity
 **
 * @param server_identity
 *      A dispatch_data_t containing the SPAKE2+ server identity
 *
 * @param client_password_verifier
 *      A dispatch_data_t containing the SPAKE2+ client password veriifer.
 *
 * @return a `sec_identity_t` instance.
 */
SPI_AVAILABLE(macos(15.4), ios(18.4), watchos(11.4), tvos(18.4), visionos(2.4))
SEC_RETURNS_RETAINED _Nullable sec_identity_t
sec_identity_create_client_SPAKE2PLUSV1_identity_internal(dispatch_data_t context,
                                                          dispatch_data_t client_identity,
                                                          dispatch_data_t server_identity,
                                                          dispatch_data_t client_password_verifier);

SEC_ASSUME_NONNULL_END

#endif /* SecProtocolInternal_h */
