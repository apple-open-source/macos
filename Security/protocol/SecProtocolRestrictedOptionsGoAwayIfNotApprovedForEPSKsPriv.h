#ifndef SecProtocolRestrictedOptionsGoAwayIfNotApprovedForEPSKsPriv_h
#define SecProtocolRestrictedOptionsGoAwayIfNotApprovedForEPSKsPriv_h

#ifdef __OBJC__
#import <Foundation/Foundation.h>
#endif // __OBJC__

#include <Security/SecProtocolOptions.h>

__BEGIN_DECLS

SEC_ASSUME_NONNULL_BEGIN

#ifdef __OBJC__

#define SEC_PROTOCOL_HAS_EXTERNAL_PRE_SHARED_KEYS 1
#define SEC_PROTOCOL_HAS_SEC_PREFIXED_EPSKs 1
@interface SecExternalPreSharedKey : NSObject
@property (retain) NSData *external_identity;
@property (retain) NSData *epsk;
@property (retain) NSData *context;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithExternalIdentity:(NSData *)external_identity :(NSData *)epsk :(NSData *)context;
@end

// remove this once dependents have stopped using: rdar://143006052
@interface ExternalPreSharedKey : SecExternalPreSharedKey
@end

#define SEC_PROTOCOL_OFFERED_EPSK_NULLABLE_CONTEXT_INIT 1
@interface SecOfferedEPSK : NSObject
@property (retain) NSData *external_identity;
@property (retain, nullable) NSData *context;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithExternalIdentity:(NSData *)external_identity :(NSData * _Nullable)context;
@end

// remove this once dependents have stopped using: rdar://143006052
@interface OfferedEPSK : SecOfferedEPSK
@end

/*!
 * @block sec_protocol_external_pre_shared_key_selection_complete_t
 *
 * @abstract
 *      Block to be invoked when a TLS 1.3 External PSK selection event is complete and an EPSK is chosen.
 *@param EPSK
 *      EPSK selected or Nil if none chosen.
 */
typedef void (^sec_protocol_external_pre_shared_key_selection_complete_t)(SecExternalPreSharedKey * _Nullable EPSK);

/*!
 * @block sec_protocol_external_pre_shared_key_selection_t
 *
 * @abstract
 *      Block to be invoked when the server must choose an TLS 1.3 external PSK  given offered (`external_identity`, `context`) pairs from client.
 *      HOWEVER if `sec_protocol_options_set_use_raw_external_pre_shared_keys` is called then
 *      "raw" epsks are used  as originally specified in RFC 8446 (not supporting  RFC 9258). So the offered pairs really represent the raw psk identity and the context field is ignored.
 *
 * @param metadata
 *      A `sec_protocol_metadata_t` instance.
 *
 * @param offered_epsks
 *      A list of offered external pre shared keys containing the external_identity, context pairs offered by the client.
 *
 * @param complete
 *      A `sec_protocol_pre_shared_key_selection_complete_t` block to be invoked when PSK selection is complete.
 */
typedef void (^sec_protocol_external_pre_shared_key_selection_t)(sec_protocol_metadata_t metadata, NSArray<SecOfferedEPSK *> *offered_epsks, sec_protocol_external_pre_shared_key_selection_complete_t complete);

/*!
 * @function sec_protocol_options_add_external_pre_shared_key
 *
 * @abstract
 *      Configure TLS to import a TLS 1.3 high entropy external pre-shared key (EPSK) according to RFC 9258.
 *      HOWEVER if `sec_protocol_options_set_use_raw_external_pre_shared_keys` is called then
 *      this key is used as a "raw" epsk as originally specified in RFC 8446 (not supporting  RFC 9258)
 *      Only usable if SwiftTLS has been enabled.
 *      DO NOT USE WITHOUT EXPLAINING
 *      YOUR USE CASE AND GETTING SECURE TRANSPORTS APPROVAL.
 *      REACH OUT ON SLACK OR VIA EMAIL TO: secure-transports-team@group.apple.com
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param EPSK
 *      A SecExternalPreSharedKey object.
 */

SPI_AVAILABLE(macos(16.0), ios(19.0), watchos(12), tvos(19.0))
void
sec_protocol_options_add_external_pre_shared_key(sec_protocol_options_t options, SecExternalPreSharedKey *EPSK);

/*!
 * @function sec_protocol_options_set_external_pre_shared_key_selection_block
 * @abstract
 *      Set the TLS 1.3 external PSK selection block.
 *      Only supported when SwiftTLS is enabled.
 *      DO NOT USE WITHOUT EXPLAINING
 *      YOUR USE CASE AND GETTING SECURE TRANSPORTS APPROVAL.
 *      REACH OUT ON SLACK OR VIA EMAIL TO: secure-transports-team@group.apple.com
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param external_psk_selection_block
 *      A `sec_protocol_external_pre_shared_key_selection_t` block.
 *
 * @param external_psk_selection_queue
 *      A `dispatch_queue_t` on which the external PSK selection block should be called.
 */

SPI_AVAILABLE(macos(16.0), ios(19.0), watchos(12), tvos(19.0))
void
sec_protocol_options_set_external_pre_shared_key_selection_block(sec_protocol_options_t options, sec_protocol_external_pre_shared_key_selection_t external_psk_selection_block, dispatch_queue_t external_psk_selection_queue);

#endif // __OBJC__

/*!
 * @function sec_protocol_options_set_use_raw_external_pre_shared_keys
 * @abstract
 *      Enable "raw" EPSK support as originally specified in RFC 8446 instead of
 *      the default behavior of using RFC 9258 imported psks.
 *      This is not recommended behavior for most use cases.
 *      Only supported when SwiftTLS is enabled.
 *      ABSOLUTELY DO NOT USE WITHOUT EXPLAINING
 *      YOUR USE CASE AND GETTING SECURE TRANSPORTS APPROVAL.
 *      REACH OUT ON SLACK OR VIA EMAIL TO: secure-transports-team@group.apple.com
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param enable
 *      Whether to enable or disable raw epsks. Default behavior is disabled.
 */

SPI_AVAILABLE(macos(16.0), ios(19.0), watchos(12), tvos(19.0))
void
sec_protocol_options_set_use_raw_external_pre_shared_keys(sec_protocol_options_t options, bool enable);

#define SEC_PROTOCOL_OPTIONS_HAS_RAW_EPSK_GETTER 1
/*!
 * @function sec_protocol_options_get_raw_external_pre_shared_keys_enabled
 * @abstract
 *      Returns whether TLS will use configured TLS 1.3 EPSKs as specified in RFC 8446 (aka "raw")
 *
 * @param options
 *      A `sec_protocol_options_t` instance.
 *
 * @param enable
 *      Whether raw epsks are enabled or disabled. Default is disabled.
 */
SPI_AVAILABLE(macos(26.0), ios(26.0), watchos(26.0), tvos(26.0), visionos(26.0))
bool
sec_protocol_options_get_raw_external_pre_shared_keys_enabled(sec_protocol_options_t options);

#define SEC_PROTOCOL_HAS_RAW_EPSKS 1

SEC_ASSUME_NONNULL_END

__END_DECLS

#endif /* SecProtocolRestrictedOptionsGoAwayIfNotApprovedForEPSKsPriv_h */
