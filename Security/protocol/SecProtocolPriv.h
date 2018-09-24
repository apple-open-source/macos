//
//  SecProtocolPriv.h
//  Security
//

#ifndef SecProtocolPriv_h
#define SecProtocolPriv_h

#include <Security/SecProtocolOptions.h>
#include <Security/SecProtocolMetadata.h>

__BEGIN_DECLS

typedef struct sec_protocol_options_content {
    SSLProtocol min_version;
    SSLProtocol max_version;

    void *ciphersuites; // xpc_object_t (array of uint64)

    void *application_protocols; // xpc_object_t (array of strings)

    void *identity; // sec_identity_t
    char *server_name;

    void *pre_shared_keys; // xpc_object_t (array of (data, identity))

    void *key_update_block; // sec_protocol_key_update_t
    void *key_update_queue; // dispatch_queue_t
    void *challenge_block; // sec_protocol_challenge_t
    void *challenge_queue; // dispatch_queue_t
    void *verify_block; // sec_protocol_verify_t
    void *verify_queue; // dispatch_queue_t

    void *dh_params; // dispatch_data_t

    void *custom_extensions; // sec_array_t of sec_tls_extension_t

    unsigned disable_sni : 1;
    unsigned enable_fallback_attempt : 1;
    unsigned enable_false_start : 1;
    unsigned enable_tickets : 1;
    unsigned enable_sct : 1;
    unsigned enable_ocsp : 1;
    unsigned enforce_ev : 1;
    unsigned enable_resumption : 1;
    unsigned enable_renegotiation : 1;
    unsigned enable_early_data : 1;
    unsigned peer_authentication_required : 1;
    unsigned peer_authentication_override : 1;
} *sec_protocol_options_content_t;

typedef dispatch_data_t (*sec_protocol_metadata_exporter)(void * handle, size_t label_len, const char *label,
                                                          size_t context_len, const uint8_t *context, size_t exporter_len);

typedef struct sec_protocol_metadata_content {
    void *peer_certificate_chain; // sec_array_t of sec_certificate_t
    void *peer_public_key; // dispatch_data_t

    const char *negotiated_protocol;

    SSLProtocol negotiated_protocol_version;
    SSLCipherSuite negotiated_ciphersuite;

    void *supported_signature_algorithms; // xpc_object_t (array of uint64)
    void *request_certificate_types; // dispatch_data
    void *ocsp_response; // sec_array_t of dispatch_data
    void *distinguished_names; // sec_array_t of dispatch_data

    void *exporter_context; // Opaque context for the exporter function
    sec_protocol_metadata_exporter exporter_function; // Exporter function pointer. This MUST be set by the metadata allocator.

    unsigned early_data_accepted : 1;
    unsigned false_start_used : 1;
    unsigned ticket_offered : 1;
    unsigned ticket_received : 1;
    unsigned session_resumed : 1;
    unsigned session_renewed : 1;

    // Struct padding
    unsigned __pad_bits : 2;
} *sec_protocol_metadata_content_t;

#ifndef SEC_OBJECT_IMPL
SEC_OBJECT_DECL(sec_array);
#endif // !SEC_OBJECT_IMPL

API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
SEC_RETURNS_RETAINED sec_array_t
sec_array_create(void);

API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
void
sec_array_append(sec_array_t array, sec_object_t object);

API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
size_t
sec_array_get_count(sec_array_t array);

#ifdef __BLOCKS__
typedef bool (^sec_array_applier_t) (size_t index, sec_object_t object);

API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
bool
sec_array_apply(sec_array_t array, sec_array_applier_t applier);

#ifdef __BLOCKS__
/*!
 * @block sec_protocol_tls_ext_add_callback
 *
 * @param metadata
 * A valid `sec_protocol_metadata_t` instance.
 *
 * @param extension_type
 *     The 2-byte identifier for the extension.
 *
 * @param data
 *     Pointer to a uint8_t buffer where the encoded extension data is located.
 *
 * @param data_length
 *     Pointer to a variable containing the data length. This should be set to the size of the `data` buffer.
 *
 * @param error
 *     Pointer to a return error code that's populated in the event of an error.
 */
API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
typedef int (^sec_protocol_tls_ext_add_callback)(sec_protocol_metadata_t metadata, uint16_t extension_type,
                                                 const uint8_t **data, size_t *data_length, int *error);

/*!
 * @block sec_protocol_tls_ext_free_callback
 *
 * @param metadata
 *     A valid `sec_protocol_metadata_t` instance.
 *
 * @param extension_type
 *     The 2-byte identifier for the extension.
 *
 * @param data
 *     Pointer to a uint8_t buffer where the encoded extension data is located.
 */
API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
typedef void (^sec_protocol_tls_ext_free_callback)(sec_protocol_metadata_t metadata, uint16_t extension_type,
                                                   const uint8_t *data);

/*!
 * @block sec_protocol_tls_ext_parse_callback
 *
 * @param metadata
 *     A valid `sec_protocol_metadata_t` handle.
 *
 * @param extension_type
 *     The 2-byte identifier for the extension.
 *
 * @param data
 *     A buffer where the encoded extension data is stored.
 *
 * @param data_length
 *     Length of the encoded extension data.
 *
 * @param error
 *     Pointer to a return error code that's populated in the event of an error.
 */
API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
typedef int (^sec_protocol_tls_ext_parse_callback)(sec_protocol_metadata_t metadata, uint16_t extension_type,
                                                   const uint8_t *data, size_t data_length,
                                                   int *error);
#endif // __BLOCKS__

#ifndef SEC_OBJECT_IMPL
SEC_OBJECT_DECL(sec_tls_extension);
#endif // !SEC_OBJECT_IMPL

#ifdef __BLOCKS__
API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
uint16_t
sec_tls_extension_get_type(sec_tls_extension_t extension);

API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
SEC_RETURNS_RETAINED sec_protocol_tls_ext_add_callback
sec_tls_extension_copy_add_block(sec_tls_extension_t extension);

API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
SEC_RETURNS_RETAINED sec_protocol_tls_ext_parse_callback
sec_tls_extension_copy_parse_block(sec_tls_extension_t extension);

API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
SEC_RETURNS_RETAINED sec_protocol_tls_ext_free_callback
sec_tls_extension_copy_free_block(sec_tls_extension_t extension);

API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
sec_tls_extension_t
sec_tls_extension_create(uint16_t type, sec_protocol_tls_ext_add_callback add_block,
                         sec_protocol_tls_ext_parse_callback parse_block,
                         sec_protocol_tls_ext_free_callback free_block);
#endif // __BLOCKS__

/*!
 * @function sec_protocol_options_add_tls_extension
 *
 * @abstract
 *      Add support for a custom TLS extension.
 *
 *      Clients such as QUIC use this when custom TLS extensions are needed.
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param extension
 *      A `sec_tls_extension_t` instance.
 */
API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
void
sec_protocol_options_add_tls_extension(sec_protocol_options_t options, sec_tls_extension_t extension);

#endif // __BLOCKS__

/*!
 * @function sec_protocol_options_set_tls_early_data_enabled
 *
 * @abstract
 *      Enable or disable early (0-RTT) data for TLS.
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param early_data_enabled
 *      Flag to enable or disable early (0-RTT) data.
 */
API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
void
sec_protocol_options_set_tls_early_data_enabled(sec_protocol_options_t options, bool early_data_enabled);

/*!
 * @function sec_protocol_options_set_tls_sni_disabled
 *
 * @abstract
 *      Enable or disable the TLS SNI extension. This defaults to `false`.
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param sni_disabled
 *      Flag to enable or disable use of the TLS SNI extension.
 */
API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
void
sec_protocol_options_set_tls_sni_disabled(sec_protocol_options_t options, bool sni_disabled);

/*!
 * @function sec_protocol_options_set_enforce_ev
 *
 * @abstract
 *      Enable or disable EV enforcement.
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param enforce_ev
 *      Flag to determine if EV is enforced.
 */
API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
void
sec_protocol_options_set_enforce_ev(sec_protocol_options_t options, bool enforce_ev);

/*!
 * @function sec_protocol_metadata_get_tls_false_start_used
 *
 * @abstract
 *      Determine if False Start was used.
 *
 * @param metadata
 *      A `sec_protocol_metadata_t` instance.
 *
 * @return True if False Start was used, and false otherwise.
 */
API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
bool
sec_protocol_metadata_get_tls_false_start_used(sec_protocol_metadata_t metadata);

/*!
 * @function sec_protocol_metadata_get_ticket_offered
 *
 * @abstract
 *      Determine if a ticket was offered for session resumption.
 *
 * @param metadata
 *      A `sec_protocol_metadata_t` instance.
 *
 * @return True if a ticket was offered for resumption, and false otherwise.
 */
API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
bool
sec_protocol_metadata_get_ticket_offered(sec_protocol_metadata_t metadata);

/*!
 * @function sec_protocol_metadata_get_ticket_received
 *
 * @abstract
 *      Determine if a ticket was received upon completing the new connection.
 *
 * @param metadata
 *      A `sec_protocol_metadata_t` instance.
 *
 * @return True if a ticket was received from the peer (server), and false otherwise.
 */
API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
bool
sec_protocol_metadata_get_ticket_received(sec_protocol_metadata_t metadata);

/*!
 * @function sec_protocol_metadata_get_session_resumed
 *
 * @abstract
 *      Determine if this new connection was a session resumption.
 *
 * @param metadata
 *      A `sec_protocol_metadata_t` instance.
 *
 * @return True if this new connection was resumed, and false otherwise.
 */
API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
bool
sec_protocol_metadata_get_session_resumed(sec_protocol_metadata_t metadata);

/*!
 * @function sec_protocol_metadata_get_session_renewed
 *
 * @abstract
 *      Determine if this resumed connection was renewed with a new ticket.
 *
 * @param metadata
 *      A `sec_protocol_metadata_t` instance.
 *
 * @return True if this resumed connection was renewed with a new ticket, and false otherwise.
 */
API_AVAILABLE(macos(10.14), ios(12.0), watchos(5.0), tvos(12.0))
bool
sec_protocol_metadata_get_session_renewed(sec_protocol_metadata_t metadata);

__END_DECLS

#endif /* SecProtocolPriv_h */
