/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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

#ifndef SecProtocolTypesPriv_h
#define SecProtocolTypesPriv_h

#include <Security/SecProtocolPriv.h>
#include <Security/SecProtocolTypes.h>

__BEGIN_DECLS

SEC_ASSUME_NONNULL_BEGIN

/*!
 * @function sec_identity_create_with_certificates_and_external_private_key
 *
 * @abstract
 *      Create an ARC-able `sec_identity_t` instance from an array of `SecCertificateRef`
 *      instances and blocks to be invoked for private key opertions. Callers may use this
 *      constructor to build a `sec_identity_t` instance with an external private key.
 *
 * @param certificates
 *      An array of `SecCertificateRef` instances.
 *
 * @param sign_block
 *      A `sec_protocol_private_key_sign_t` block.
 *
 * @param decrypt_block
 *      A `sec_protocol_private_key_decrypt_t` block.
 *
 * @param operation_queue
 *      The `dispatch_queue_t` queue on which each private key operation is invoked.
 *
 * @return a `sec_identity_t` instance.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
SEC_RETURNS_RETAINED _Nullable sec_identity_t
sec_identity_create_with_certificates_and_external_private_key(CFArrayRef certificates,
                                                               sec_protocol_private_key_sign_t sign_block,
                                                               sec_protocol_private_key_decrypt_t decrypt_block,
                                                               dispatch_queue_t operation_queue);

// SEC_PROTOCOL_SPAKE2PLUSV1_INPUT_PASSWORD_VERIFIER_NBYTES is the expected size of an input password
// verifier for SPAKE2PLUS_V1; see https://datatracker.ietf.org/doc/html/rfc9383#section-3.2
#define SEC_PROTOCOL_SPAKE2PLUSV1_INPUT_PASSWORD_VERIFIER_NBYTES 80

// SEC_PROTOCOL_SPAKE2PLUSV1_CLIENT_PASSWORD_VERIFIER_NBYTES is the expected size of a client password
// verifier for SPAKE2PLUS_V1; see https://datatracker.ietf.org/doc/html/rfc9383#section-3.2
#define SEC_PROTOCOL_SPAKE2PLUSV1_CLIENT_PASSWORD_VERIFIER_NBYTES 64

// SEC_PROTOCOL_SPAKE2PLUSV1_SERVER_PASSWORD_VERIFIER_NBYTES is the expected size of a server password
// verifier for SPAKE2PLUS_V1; see https://datatracker.ietf.org/doc/html/rfc9383#section-3.2
#define SEC_PROTOCOL_SPAKE2PLUSV1_SERVER_PASSWORD_VERIFIER_NBYTES 32

// SEC_PROTOCOL_SPAKE2PLUSV1_REGISTRATION_RECORD_NBYTES is the size of a SPAKE2PLUS_V1
// registration record; see https://datatracker.ietf.org/doc/html/rfc9383#section-3.2
#define SEC_PROTOCOL_SPAKE2PLUSV1_REGISTRATION_RECORD_NBYTES 65

/// TLS-SPAKE2+.
///
/// TLS-SPAKE2+ is supported via a custom `sec_identity` type. Calling applications configure `sec_protocol_options`
/// with client and server `sec_identity_t` values. The client and server identity values are created using
/// `sec_identity_create_client_SPAKE2PLUSV1_identity` and `sec_identity_create_server_SPAKE2PLUSV1_identity`,
/// respectively. Clients create identities using a password and a chosen PBKDF method; see `pake_pbkdf_params_t`.
/// Applications can copy the server password verifier and registration record using `sec_identity_copy_SPAKE2PLUSV1_server_password_verifier`
/// and `sec_identity_copy_SPAKE2PLUSV1_registration_record`, respectively. Clients must securely transmit
/// these to the server out-of-band. This is referred to as the offline registration step of SPAKE2+; see
/// https://datatracker.ietf.org/doc/html/rfc9383#name-offline-registration for more details. Servers then
/// create an identity using these values with `sec_identity_create_server_SPAKE2PLUSV1_identity`.

/*!
 * `pake_pbkdf_params_t` is an enumeration for the supported PBKDF methods for deriving PAKE authentication material
 * from a low-entropy password.
 */
typedef CF_ENUM(uint16_t, pake_pbkdf_params_t) {
    // PAKE_PBKDF_PARAMS_SCRYPT_DEFAULT uses Scrypt with N=32768, r=8, and p=1; see https://datatracker.ietf.org/doc/html/rfc7914
    PAKE_PBKDF_PARAMS_SCRYPT_DEFAULT = 0,
};

/*!
 * @function sec_identity_create_client_SPAKE2PLUSV1_identity
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
 * @param password
 *      A dispatch_data_t containing the SPAKE2+ client password.
 *
 * @param pbkdf_params
 *      A pake_pbkdf_params_t value indicating the type of PBKDF to use.
 *
 * @return a `sec_identity_t` instance.
 */
#define SEC_PROTOCOL_HAS_TLS_SPAKE2PLUS_IDENTITY 1
SPI_AVAILABLE(macos(15.4), ios(18.4), watchos(11.4), tvos(18.4), visionos(2.4))
SEC_RETURNS_RETAINED _Nullable sec_identity_t
sec_identity_create_client_SPAKE2PLUSV1_identity(dispatch_data_t context,
                                                 dispatch_data_t client_identity,
                                                 dispatch_data_t server_identity,
                                                 dispatch_data_t password,
                                                 pake_pbkdf_params_t pbkdf_params);

/*!
 * @function sec_identity_create_server_SPAKE2PLUSV1_identity
 *
 * @abstract
 *      Create an ARC-able `sec_identity_t` instance containing the information needed
 *      for a server to authenticate with a specific client using the SPAKE2PLUS_V1 named PAKE from:
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
 * @param server_password_verifier
 *      A dispatch_data_t containing the SPAKE2+ server password veriifer.
 *
 * @param registration_record
 *      A dispatch_data_t containing the SPAKE2+ client registration record.
 *
 * @return a `sec_identity_t` instance.
 */
SPI_AVAILABLE(macos(15.4), ios(18.4), watchos(11.4), tvos(18.4), visionos(2.4))
SEC_RETURNS_RETAINED _Nullable sec_identity_t
sec_identity_create_server_SPAKE2PLUSV1_identity(dispatch_data_t context,
                                                 dispatch_data_t client_identity,
                                                 dispatch_data_t server_identity,
                                                 dispatch_data_t server_password_verifier,
                                                 dispatch_data_t registration_record);

/*
 * Different types of sec_identity_t values supported.
 */
typedef CF_ENUM(uint16_t, sec_identity_type_t) {
    SEC_PROTOCOL_IDENTITY_TYPE_INVALID = 0,
    SEC_PROTOCOL_IDENTITY_TYPE_CERTIFICATE = 1,
    SEC_PROTOCOL_IDENTITY_TYPE_SPAKE2PLUSV1 = 2,
};

/*!
 * @function sec_identity_copy_type
 *
 * @abstract
 *      Copy the type of the sec_identity_t.
 *
 * @param identity
 *      A sec_identity_t instance
 *
 * @return a `sec_identity_type_t` value.
 */
SPI_AVAILABLE(macos(15.4), ios(18.4), watchos(11.4), tvos(18.4), visionos(2.4))
sec_identity_type_t
sec_identity_copy_type(sec_identity_t identity);

/*!
 * @function sec_identity_copy_private_key_sign_block
 *
 * @abstract
 *      Copy a retained reference to the underlying `sec_protocol_private_key_sign_t` used by the identity.
 *
 * @param identity
 *      A `sec_identity_t` instance.
 *
 * @return a `sec_protocol_private_key_sign_t` block, or nil.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
SEC_RETURNS_RETAINED _Nullable sec_protocol_private_key_sign_t
sec_identity_copy_private_key_sign_block(sec_identity_t identity);

/*!
 * @function sec_identity_copy_private_key_decrypt_block
 *
 * @abstract
 *      Copy a retained reference to the underlying `sec_protocol_private_key_decrypt_t` used by the identity.
 *
 * @param identity
 *      A `sec_identity_t` instance.
 *
 * @return a `sec_protocol_private_key_decrypt_t` block, or nil.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
SEC_RETURNS_RETAINED _Nullable sec_protocol_private_key_decrypt_t
sec_identity_copy_private_key_decrypt_block(sec_identity_t identity);

/*!
 * @function sec_identity_copy_private_key_queue
 *
 * @abstract
 *      Copy a retained reference to the `dispatch_queue_t` to be used by external private key
 *      operations, if any.
 *
 * @param identity
 *      A `sec_identity_t` instance.
 *
 * @return a `dispatch_queue_t` queue, or nil.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
SEC_RETURNS_RETAINED _Nullable dispatch_queue_t
sec_identity_copy_private_key_queue(sec_identity_t identity);

/*!
 * @function sec_identity_has_certificates
 *
 * @abstract
 *      Determine if the `sec_identity_t` has a list of certificates associated with it.
 *
 * @param identity
 *      A `sec_identity_t` instance.
 *
 * @return True if the identity has certificates associated with it, and false otherwise.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
bool
sec_identity_has_certificates(sec_identity_t identity);

/*!
 * @function sec_identity_copy_SPAKE2PLUSV1_context
 *
 * @abstract
 *      Copy a retained reference to the `dispatch_data_t`carrying the SPAKE2+ context.
 *
 * @param identity
 *      A `sec_identity_t` instance.
 *
 * @return a `dispatch_data_t` value, or nil.
 */
SPI_AVAILABLE(macos(15.4), ios(18.4), watchos(11.4), tvos(18.4), visionos(2.4))
SEC_RETURNS_RETAINED _Nullable dispatch_data_t
sec_identity_copy_SPAKE2PLUSV1_context(sec_identity_t identity);

/*!
 * @function sec_identity_copy_SPAKE2PLUSV1_client_identity
 *
 * @abstract
 *      Copy a retained reference to the `dispatch_data_t`carrying the SPAKE2+ client identity.
 *
 * @param identity
 *      A `sec_identity_t` instance.
 *
 * @return a `dispatch_data_t` value, or nil.
 */
SPI_AVAILABLE(macos(15.4), ios(18.4), watchos(11.4), tvos(18.4), visionos(2.4))
SEC_RETURNS_RETAINED _Nullable dispatch_data_t
sec_identity_copy_SPAKE2PLUSV1_client_identity(sec_identity_t identity);

/*!
 * @function sec_identity_copy_SPAKE2PLUSV1_server_identity
 *
 * @abstract
 *      Copy a retained reference to the `dispatch_data_t`carrying the SPAKE2+ server identity.
 *
 * @param identity
 *      A `sec_identity_t` instance.
 *
 * @return a `dispatch_data_t` value, or nil.
 */
SPI_AVAILABLE(macos(15.4), ios(18.4), watchos(11.4), tvos(18.4), visionos(2.4))
SEC_RETURNS_RETAINED _Nullable dispatch_data_t
sec_identity_copy_SPAKE2PLUSV1_server_identity(sec_identity_t identity);

/*!
 * @function sec_identity_copy_SPAKE2PLUSV1_server_password_verifier
 *
 * @abstract
 *      Copy a retained reference to the `dispatch_data_t`carrying the SPAKE2+ server password verifier.
 *
 * @param identity
 *      A `sec_identity_t` instance.
 *
 * @return a `dispatch_data_t` value, or nil.
 */
SPI_AVAILABLE(macos(15.4), ios(18.4), watchos(11.4), tvos(18.4), visionos(2.4))
SEC_RETURNS_RETAINED _Nullable dispatch_data_t
sec_identity_copy_SPAKE2PLUSV1_server_password_verifier(sec_identity_t identity);

/*!
 * @function sec_identity_copy_SPAKE2PLUSV1_client_password_verifier
 *
 * @abstract
 *      Copy a retained reference to the `dispatch_data_t`carrying the SPAKE2+ client password verifier.
 *
 * @param identity
 *      A `sec_identity_t` instance.
 *
 * @return a `dispatch_data_t` value, or nil.
 */
SPI_AVAILABLE(macos(15.4), ios(18.4), watchos(11.4), tvos(18.4), visionos(2.4))
SEC_RETURNS_RETAINED _Nullable dispatch_data_t
sec_identity_copy_SPAKE2PLUSV1_client_password_verifier(sec_identity_t identity);

/*!
 * @function sec_identity_copy_SPAKE2PLUSV1_registration_record
 *
 * @abstract
 *      Copy a retained reference to the `dispatch_data_t`carrying the SPAKE2+ registration record.
 *
 * @param identity
 *      A `sec_identity_t` instance.
 *
 * @return a `dispatch_data_t` value, or nil.
 */
SPI_AVAILABLE(macos(15.4), ios(18.4), watchos(11.4), tvos(18.4), visionos(2.4))
SEC_RETURNS_RETAINED _Nullable dispatch_data_t
sec_identity_copy_SPAKE2PLUSV1_registration_record(sec_identity_t identity);

SEC_ASSUME_NONNULL_END

__END_DECLS

#endif // SecProtocolTypesPriv_h
