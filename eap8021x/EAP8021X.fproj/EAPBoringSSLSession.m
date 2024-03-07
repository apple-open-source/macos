/*
 * Copyright (c) 2022-2024 Apple Inc. All rights reserved.
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
				    ┌─────────────────────────────────────────┐
				    │   EAP8021X (eaptls_plugin application)  │
				    └────────────┬───────────────▲────────────┘
		       Outbound application data │               │ Inbound application data
				    ┌────────────▼───────────────┴────────────┐
				    │         TLS Protocol (boingssl) 	      │
				    └────────────┬───────────────▲────────────┘
			 Outbound TLS Records    │               │ Inbound TLS Records
				    ┌────────────▼───────────────┴────────────┐
				    │    [Output Handler]        │            │
				    │            |               │ 	      │
				    │            |     Custom    │            │
				    │            |    Protocol   │            │
				    └────────────┬───────────────▲────────────┘
					         │     		 │
				    ┌────────────▼───────────────┴────────────┐
				    │ [MemIO Write Buffer] [MemIO Read Buffer]│
				    │               			      │
				    │      EAP8021X (eaptls_plugin method)    │
				    └────────────┬───────────────▲────────────┘
		          Outbound EAP-TLS frame │     		 │ Inbound EAP-TLS frame
				    ┌────────────▼───────────────┴────────────┐
				    │        NDRV Socket I/O (Supplicant)     │
				    └─────────────────────────────────────────┘
 */


#import <Foundation/Foundation.h>
#import <Security/SecCertificatePriv.h>
#import <Security/SecKeyPriv.h>
#import <Security/SecProtocolPriv.h>
#import <Security/SecureTransport.h>
#import <Security/SecTrustPriv.h>
#import <nw/private.h>
#import <os/lock.h>
#import <CoreUtils/CoreUtils.h>
#import "EAPClientProperties.h"
#import "EAP.h"
#import "EAPSecurity.h"
#import "EAPTLSUtil.h"
#import "EAPBoringSSLSession.h"
#import "EAPLog.h"

#define EAP_BORINGSSL_DUMMY_HOST "0.0.0.0"
#define EAP_BORINGSSL_DUMMY_PORT "0"

/* TLS Record Buffer Length
 * https://www.rfc-editor.org/rfc/rfc5246#section-6.2.3
 * https://www.rfc-editor.org/rfc/rfc8446#section-5.1
 */
#define EAP_TLS_DEFAULT_TLS_RECORD_BUFFER_SIZE (16384 + 2048)

typedef NS_ENUM(NSInteger, EAPBoringSSLSessionStatusUpdate) {
    EAPBoringSSLSessionStatusUpdateReady, /* status is ready to be read by EAP-TLS plugin */
    EAPBoringSSLSessionStatusUpdatePending /* status is pending to be updated by TLS protocol */
};

#pragma mark - EAPBoringSSLSession Objective-C Interface

__attribute__((visibility("hidden")))
@interface EAPBoringSSLSession : NSObject

@property (nonatomic) nw_connection_t connection;
@property (nonatomic) nw_protocol_options_t tlsProtocol;
@property (nonatomic) nw_protocol_options_t customProtocol;
@property (nonatomic, assign) sec_protocol_metadata_t secProtocolMetadata;
@property (nonatomic) nw_framer_t customFramer;
@property (nonatomic) dispatch_queue_t queue;
@property (nonatomic) NSData *msk;
@property (nonatomic, assign) EAPBoringSSLSessionState state;
@property (nonatomic, assign) EAPBoringSSLSessionReadFunc read;
@property (nonatomic, assign) EAPBoringSSLSessionWriteFunc write;
@property (nonatomic, assign) EAPBoringSSLClientContextRef clientContext;
@property (nonatomic, assign) OSStatus handshakeStatus;
@property (nonatomic) SecTrustRef serverSecTrust;
@property (copy) sec_protocol_verify_complete_t secTrustCompletionHandler;
@property (nonatomic) NSConditionLock *statusUpdateLock;
@property (nonatomic, assign) EAPType eapType;
@property (nonatomic, assign) memoryIORef memIO;

@end


@implementation EAPBoringSSLSession {
    EAPBoringSSLSessionState _state;
    OSStatus _handshakeStatus;
}

- (nullable instancetype)init {
    if ((self = [super init])) {
	_statusUpdateLock = [[NSConditionLock alloc] initWithCondition:EAPBoringSSLSessionStatusUpdatePending];
	EAPLOG_FL(LOG_DEBUG, "EAPBoringSSLSession initialized");
    }
    return self;
}

- (void)dealloc
{
    if (_serverSecTrust) {
	CFRelease(_serverSecTrust);
    }
    EAPLOG_FL(LOG_DEBUG, "EAPBoringSSLSession deallocated");
}

- (nw_protocol_definition_t)customProtocolDefinition
{
    nw_protocol_definition_t definition = nil;

    __weak typeof(self) weakSelf = self;
    definition = nw_framer_create_definition("EAPBoringSSLSessionInterceptor",
					     NW_FRAMER_CREATE_FLAGS_DEFAULT,
					     ^nw_framer_start_result_t(nw_framer_t framer) {
	EAPLOG_FL(LOG_DEBUG, "start handler for custom protocol called");

	typeof(self) strongSelf = weakSelf;
	if (strongSelf == nil) {
	    return nw_framer_start_result_will_mark_ready;
	}

	/* store framer and protocol objects as we need them later */
	strongSelf.customFramer = framer;
	strongSelf.customProtocol = nw_framer_copy_options(framer);

	/* Set the input handler. This just fulfills the API requirement, and has no role
	 * to play in the functionality
	 */
	nw_framer_set_input_handler(framer, ^size_t(__unused nw_framer_t parser) {
	    return 0;
	});

	/* Set the output handler. This is the handler that receives outbound TLS records from the
	 * TLS protocol, and it writes that data into MemIO write buffer. This unblocks the
	 * EAPBoringSSLSessionHandshake() call in EAP-TLS protocol.
	 */
	nw_framer_set_output_handler(framer, ^(nw_framer_t writer, nw_framer_message_t message, size_t message_length,  bool is_complete) {
	    EAPLOG_FL(LOG_NOTICE, "output handler received message_length: [%zu], is_complete:[%s]",
		      message_length, is_complete ? "true" : "false");

	    if (message_length == 0) {
		nw_framer_mark_failed_with_error(framer, EIO);
		return;
	    }
	    bool do_write_output = true;

	    if (strongSelf.state != EAPBoringSSLSessionStateConnecting) {
		/* Cancellation of connection causes TLS protocol to generate TLS Alert (rdar://102327114),
		 * which we don't want to send to the server as the session is already closed for EAP protocol.
		 * The session must be in connecting state in order to process outbound TLS message,
		 * so don't bother to send it to MemIO write buffer, just tell the stack it's done
		 * reading the outbound data.
		 */
		do_write_output = false;
	    }

	    __block size_t sent_length = 0;
	    __block OSStatus status = errSSLWouldBlock;
	    while (sent_length < message_length) {
		nw_framer_parse_output(framer, message_length, message_length,
					nil, ^size_t(uint8_t *buffer,
						    size_t buffer_length,
						    __unused bool complete) {

		    if (do_write_output == false) {
			/* no need to write to MemIO Write Buffer, just leave this block */
			return buffer_length;
		    }
		    if (buffer != nil && buffer_length > 0) {
			EAPLOG_FL(LOG_DEBUG, "writing %zu bytes to MemIO Write Buffer", buffer_length);
			status = strongSelf.write(strongSelf.memIO, buffer, &buffer_length);
			if (status != errSecSuccess) {
			    EAPLOG_FL(LOG_ERR, "failed to write to MemIO write buffer, reporting EPROTO");
			    nw_framer_mark_failed_with_error(framer, EPROTO);
			    return 0;
			}
			EAPLOG_FL(LOG_DEBUG, "completed writing %zu bytes to MemIO Buffer", buffer_length);
			sent_length += buffer_length;
			return buffer_length;
		    }
		    return 0;
		});
		if (do_write_output == false) {
		    break;
		}
	    }
	    if (do_write_output && sent_length > 0) {
		/* update the handshake status only when data is written to MemIO write buffer */
		status = (status == errSecSuccess) ? errSSLWouldBlock : status;
		[strongSelf updateHandshakeStatus:status];
		EAPLOG_FL(LOG_NOTICE, "[output_handler]: updated handshake status to [%s]:[%d]",
			  EAPSecurityErrorString(status), (int)status);
	    }
	});
	strongSelf.state = EAPBoringSSLSessionStateConnecting;
	EAPLOG_FL(LOG_DEBUG, "custom protocol reported ready status");
	/* tell the stack this protocol is ready to function */
	return nw_framer_start_result_ready;
    });
    return definition;
}

- (BOOL)setClientIdentity:(SecIdentityRef)identity certificates:(NSArray *)certificates
{
    sec_protocol_options_t security_options = nw_tls_copy_sec_protocol_options(self.tlsProtocol);
    if (security_options == nil) {
	EAPLOG_FL(LOG_ERR, "nw_tls_copy_sec_protocol_options() returned nil");
	return NO;
    }
    // set client's identity
    if (identity != NULL) {
	sec_identity_t secIdentity = nil;
	if (certificates != nil && certificates.count > 0) {
	    secIdentity = sec_identity_create_with_certificates(identity, (__bridge CFArrayRef)certificates);
	} else {
	    secIdentity = sec_identity_create(identity);
	}
	if (secIdentity == nil) {
	    EAPLOG_FL(LOG_ERR, "sec_identity_create()/sec_identity_create_with_certificates() returned nil");
	    return NO;
	}
	sec_protocol_options_set_local_identity(security_options, secIdentity);
    }
    return YES;
}

- (BOOL)configureSecProtocol:(EAPBoringSSLSessionParametersRef)sessionParameters
{
    if (sessionParameters == nil) {
	return NO;
    }

    sec_protocol_options_t security_options = nw_tls_copy_sec_protocol_options(self.tlsProtocol);
    if (security_options == nil) {
	EAPLOG_FL(LOG_ERR, "nw_tls_copy_sec_protocol_options() returned nil");
	return NO;
    }
    /* early data must not be used in EAP-TLS (https://www.rfc-editor.org/rfc/rfc9190.html#section-2.1) */
    sec_protocol_options_set_tls_early_data_enabled(security_options, false);

    // set the min/max protocol version
    sec_protocol_options_set_min_tls_protocol_version(security_options, sessionParameters->min_tls_version);
    sec_protocol_options_set_max_tls_protocol_version(security_options, sessionParameters->max_tls_version);

    // set EAP Method
    sec_protocol_options_eap_method_t secEapType;

    switch (self.eapType) {
	case kEAPTypeTLS:
	    secEapType = sec_protocol_options_eap_method_tls;
	    break;
	default:
	    secEapType = sec_protocol_options_eap_method_none;
	    break;
    }
    if (secEapType == sec_protocol_options_eap_method_none) {
	return NO;
    }
    sec_protocol_options_set_eap_method(security_options, secEapType);


    // set client identity
    if (sessionParameters->client_identity != NULL) {
	sec_identity_t secIdentity = nil;
	if (sessionParameters->client_certificates != NULL &&
	    CFArrayGetCount(sessionParameters->client_certificates) > 0) {
	    secIdentity = sec_identity_create_with_certificates(sessionParameters->client_identity,
								sessionParameters->client_certificates);
	} else {
	    secIdentity = sec_identity_create(sessionParameters->client_identity);
	}
	if (secIdentity == nil) {
	    EAPLOG_FL(LOG_ERR, "sec_identity_create()/sec_identity_create_with_certificates() returned nil");
	    return NO;
	}
	sec_protocol_options_set_local_identity(security_options, secIdentity);
    }

    __weak typeof(self) weakSelf = self;
    sec_protocol_options_set_verify_block(security_options,
					  ^(sec_protocol_metadata_t metadata,
					    sec_trust_t trust_ref,
					    sec_protocol_verify_complete_t complete)
    {
	EAPLOG_FL(LOG_INFO, "verify_block called");
	typeof(self) strongSelf = weakSelf;
	if (strongSelf == nil) {
	    complete(false);
	    return;
	}
	strongSelf.serverSecTrust = sec_trust_copy_ref(trust_ref);
	strongSelf.secTrustCompletionHandler = complete;
	[strongSelf updateHandshakeStatus:errSSLServerAuthCompleted];
	EAPLOG_FL(LOG_DEBUG, "[verify_block]: updated handshake status to [%s]:[%d]",
		  EAPSecurityErrorString(errSSLServerAuthCompleted), (int)errSSLServerAuthCompleted);
    }, self.queue);

    return YES;
}

- (BOOL)setup:(EAPBoringSSLSessionParametersRef)sessionParameters clientContext:(EAPBoringSSLClientContextRef)clientContext
{
    if (sessionParameters == nil) {
	return NO;
    }
    // create network parameters
    nw_parameters_t parameters = nw_parameters_create();
    if (parameters == nil) {
	EAPLOG_FL(LOG_DEBUG, "nw_parameters_create() returned nil");
	return NO;
    }

    // get the protocol stack from the network parameters
    nw_protocol_stack_t stack = nw_parameters_copy_default_protocol_stack(parameters);
    if (stack == nil) {
	EAPLOG_FL(LOG_DEBUG, "nw_parameters_copy_default_protocol_stack() returned nil");
	return NO;
    }

    /* this is necessary to make sure that network connection with these parameters use
     * custom stack only.
     */
    nw_parameters_set_custom_protocols_only(parameters, true);

    // create TLS protocol
    self.tlsProtocol = nw_tls_create_options();
    if (self.tlsProtocol == nil) {
	EAPLOG_FL(LOG_ERR, "nw_tls_create_options() returned nil");
	return NO;
    }

    self.eapType = sessionParameters->eap_method;
    self.read = sessionParameters->read_func;
    self.write = sessionParameters->write_func;
    self.queue = dispatch_queue_create("EAPBoringSSLSession", NULL);
    self.clientContext = clientContext;
    self.memIO = sessionParameters->memIO;

    if (![self configureSecProtocol:sessionParameters]) {
	EAPLOG_FL(LOG_ERR, "failed to set the security protocol options");
	return NO;
    }

    // create custom protocol from the definition
    nw_protocol_options_t customProtocol = nw_framer_create_options([self customProtocolDefinition]);
    if (customProtocol == nil) {
	EAPLOG_FL(LOG_ERR, "nw_framer_create_options() returned nil");
	return NO;
    }

    // first add TLS protocol (north)
    nw_protocol_stack_append_application_protocol(stack, self.tlsProtocol);

    // append custom protocol (south)
    nw_protocol_stack_append_application_protocol(stack, customProtocol);

    // create a dummy endpoint, this is necessary to create a network connection
    nw_endpoint_t dummyEndpoint = nw_endpoint_create_host(EAP_BORINGSSL_DUMMY_HOST,
							  EAP_BORINGSSL_DUMMY_PORT);
    if (dummyEndpoint == nil) {
	EAPLOG_FL(LOG_ERR, "nw_endpoint_create_host() returned nil");
	return NO;
    }

    // create a network connection to the dummy endpoint
    self.connection = nw_connection_create(dummyEndpoint, parameters);

    if (self.connection == nil) {
	EAPLOG_FL(LOG_ERR, "nw_connection_create() returned nil");
	return NO;
    }
    return YES;
}

- (OSStatus)handshake
{
    OSStatus status;

    if (self.handshakeStatus == errSSLServerAuthCompleted && self.secTrustCompletionHandler != nil) {
	/* This means the client application completed server certifciate verification */
	self.secTrustCompletionHandler(true);
	EAPLOG_FL(LOG_INFO, "delivered trust evaluation result=success to TLS protocol");
    } else {
	/* check if MemIO read buffer has bytes to deliver to the TLS protocol */
	[self deliverInput];
    }

    /* wait till the handshake status gets updated */
    [self.statusUpdateLock lockWhenCondition:EAPBoringSSLSessionStatusUpdateReady];
    status = self.handshakeStatus;
    [self.statusUpdateLock unlockWithCondition:EAPBoringSSLSessionStatusUpdatePending];

    return status;
}

/* this method is responsible to deliver the inbound TLS records to the TLS protocol */
- (OSStatus)deliverInput
{
    OSStatus 			status = errSecSuccess;
    void 			*data = NULL;
    nw_framer_message_t 	metadata = NULL;

    if (self.state != EAPBoringSSLSessionStateConnecting) {
	return status;
    }
    nw_protocol_definition_t customProtocolDefinition = nw_protocol_options_copy_definition(self.customProtocol);
    if (customProtocolDefinition != nil) {
	metadata = nw_protocol_metadata_create_singleton(customProtocolDefinition);
    }
    if (metadata == NULL) {
	EAPLOG_FL(LOG_ERR, "nw_protocol_metadata_create_singleton() returned nil metadata");
	return errSecParam;
    }
    data = calloc(1, EAP_TLS_DEFAULT_TLS_RECORD_BUFFER_SIZE);
    if (data == NULL) {
	return errSecAllocate;
    }
    size_t length = EAP_TLS_DEFAULT_TLS_RECORD_BUFFER_SIZE;
    size_t bytes_delivered = 0;
    BOOL sendEOD = NO;
    while(!sendEOD) {
	status = self.read(self.memIO, data, &length);
	if (status != errSecSuccess) {
	    EAPLOG_FL(LOG_ERR, "failed to read from the MemIO read buffer");
	    nw_framer_mark_failed_with_error(self.customFramer, EPROTO);
	    break;
	}
	if (length == 0 && bytes_delivered == 0) {
	    /* there is nothing to deliver to TLS */
	    break;
	}
	sendEOD = (length > 0) ? NO : YES;
	/* delivery to TLS protocol must take place in the queue maintained by the framer API */
	NSData *dataToDeliver = nil;
	if (!sendEOD) {
	    dataToDeliver = [NSData dataWithBytes:data length:length];
	}
	__weak typeof(self) weakSelf = self;
	nw_framer_async(self.customFramer, ^{
	    typeof(self) strongSelf = weakSelf;
	    if (strongSelf == nil) {
		return;
	    }
	    if (sendEOD) {
		nw_framer_deliver_input_no_copy(strongSelf.customFramer, 0, metadata, true);
		EAPLOG_FL(LOG_DEBUG, "nw_framer_deliver_input_no_copy() delivered EOD");
	    } else {
		nw_framer_deliver_input(strongSelf.customFramer, dataToDeliver.bytes, dataToDeliver.length, metadata, false);
		EAPLOG_FL(LOG_DEBUG, "nw_framer_deliver_input() delivered %zu bytes", length);
	    }
	});
	bytes_delivered += length;
	length = EAP_TLS_DEFAULT_TLS_RECORD_BUFFER_SIZE;
    }
    if (bytes_delivered > 0) {
	EAPLOG_FL(LOG_DEBUG, "delivered total %zu bytes to TLS protocol", bytes_delivered);
    }
    if (data != NULL) {
	free(data);
    }
    return status;
}

- (sec_protocol_options_eap_method_t)getEAPMethodInUse
{
    sec_protocol_options_t securityOptions = nw_tls_copy_sec_protocol_options(self.tlsProtocol);
    if (securityOptions == nil) {
	EAPLOG_FL(LOG_ERR, "nw_tls_copy_sec_protocol_options() returned nil");
	return sec_protocol_options_eap_method_none;
    }
    return sec_protocol_options_get_eap_method(securityOptions);
}

- (void)setSecProtocolMetadata
{
    /* if necessary, we need to set the Sec Metadata before passing it to Sec API */
    if (self.secProtocolMetadata == nil) {
	nw_protocol_definition_t tls_proto_definition = nw_protocol_options_copy_definition(self.tlsProtocol);
	if (tls_proto_definition != nil) {
	    self.secProtocolMetadata =
		(sec_protocol_metadata_t)nw_connection_copy_protocol_metadata(self.connection,
									      tls_proto_definition);
	}
    }
}

- (NSData *)getEAPKeyMaterial
{
    sec_protocol_options_eap_method_t secEAPType = [self getEAPMethodInUse];
    if (secEAPType == sec_protocol_options_eap_method_none ||
	secEAPType > sec_protocol_options_eap_method_max) {
	return nil;
    }
    [self setSecProtocolMetadata];
    if (self.secProtocolMetadata == nil) {
	return nil;
    }
    uint8_t key[EAP_KEY_MATERIAL_SIZE];
    size_t key_length = EAP_KEY_MATERIAL_SIZE;
    bool ret = sec_protocol_metadata_get_eap_key_material(self.secProtocolMetadata, key, key_length);
    if (ret == false) {
	return nil;
    }
    return [NSData dataWithBytes:key length:key_length];
}

- (tls_protocol_version_t)getNegotiatedTLSVersion
{
    [self setSecProtocolMetadata];
    if (self.secProtocolMetadata != nil) {
	return (sec_protocol_metadata_get_negotiated_tls_protocol_version(self.secProtocolMetadata));
    }
    return 0x0000;
}

- (BOOL)getSessionResumed
{
    bool ret = false;

    [self setSecProtocolMetadata];
    if (self.secProtocolMetadata != nil) {
	ret = sec_protocol_metadata_get_session_resumed(self.secProtocolMetadata);
    }
    return (ret ? YES : NO);
}

- (NSArray *)copyPeerCertificateChain
{
    [self setSecProtocolMetadata];
    if (self.secProtocolMetadata == nil) {
	return nil;
    }
    __block NSMutableArray *certificates = [[NSMutableArray alloc] init];
    bool hasCerts = sec_protocol_metadata_access_peer_certificate_chain(self.secProtocolMetadata, ^(sec_certificate_t certificate) {
	SecCertificateRef certificateRef = sec_certificate_copy_ref(certificate);
	[certificates addObject:(__bridge_transfer NSObject *)certificateRef];
    });
    if (hasCerts && certificates.count > 0) {
	return certificates;
    }
    return nil;
}

- (EAPBoringSSLSessionState)state
{
    @synchronized (self) {
	return _state;
    }
}

- (void)setState:(EAPBoringSSLSessionState)state
{
    @synchronized (self) {
	_state = state;
    }
}

- (OSStatus)handshakeStatus
{
    @synchronized (self) {
	return _handshakeStatus;
    }
}

- (void)setHandshakeStatus:(OSStatus)handshakeStatus
{
    @synchronized (self) {
	_handshakeStatus = handshakeStatus;
    }
}

static const char *
eap_boringssl_nw_state_to_string(nw_connection_state_t state)
{
    switch (state) {
#define NWSC_CASE_STATE_RETURN_STRING(s) case nw_connection_state_ ## s: return #s;
	NWSC_CASE_STATE_RETURN_STRING(invalid)
	NWSC_CASE_STATE_RETURN_STRING(waiting)
	NWSC_CASE_STATE_RETURN_STRING(preparing)
	NWSC_CASE_STATE_RETURN_STRING(ready)
	NWSC_CASE_STATE_RETURN_STRING(failed)
	NWSC_CASE_STATE_RETURN_STRING(cancelled)
#undef NWSC_CASE_STATE_RETURN_STRING
    }
    return "unknown";
}

- (void)readApplicationData
{
    __weak typeof(self) weakSelf = self;
    nw_connection_receive(self.connection, 0, 1,
			  ^(dispatch_data_t data, __unused nw_content_context_t context, bool is_complete, nw_error_t nw_error) {
	typeof(self) strongSelf = weakSelf;
	OSStatus status = errSecSuccess;
	if (nw_error != NULL) {
	    // handle error
	    NSError *error = (__bridge_transfer NSError *)nw_error_copy_cf_error(nw_error);
	    EAPLOG_FL(LOG_DEBUG, "application data receive completion handler gor error : %@", error);
	    status = errSecInternalError;
	} else if (data != NULL) {
	    NSData *dataReceived = (NSData *)data;
	    uint8_t *receivedBytes = (uint8_t *)dataReceived.bytes;
	    if (dataReceived.length == 1 && receivedBytes[0] == 0x00) {
		EAPLOG_FL(LOG_DEBUG, "Received expected application data from the EAP-TLS 1.3 server");
		strongSelf.state = EAPBoringSSLSessionStateConnected;
	    } else {
		/* server interoperability issue,
		 * this will lead to send TLS alert close-notify to the server.
		 */
		status = errSSLProtocol;
	    }
	}
	[strongSelf updateHandshakeStatus:status];
	EAPLOG_FL(LOG_DEBUG, "[Application Data Reader]: updated handshake status to [%s]:[%d]",
		  EAPSecurityErrorString(status), (int)status);
    });
}

- (void)start
{
    __weak typeof(self) weakSelf = self;
    nw_connection_set_state_changed_handler(self.connection,
					    ^(nw_connection_state_t state, __unused nw_error_t error) {
	typeof(self) strongSelf = weakSelf;
	EAPLOG_FL(LOG_INFO, "connection state changed to %s", eap_boringssl_nw_state_to_string(state));
	OSStatus status;
	BOOL doUpdate = YES;
	tls_protocol_version_t version;
	sec_protocol_options_eap_method_t eapMethod;
	BOOL sessionResumed;
	switch (state) {
	    case nw_connection_state_ready:
		eapMethod = [strongSelf getEAPMethodInUse];
		version = [strongSelf getNegotiatedTLSVersion];
		sessionResumed = [strongSelf getSessionResumed];
		if (eapMethod == sec_protocol_options_eap_method_tls &&
		    version == tls_protocol_version_TLSv13 && !sessionResumed) {
		    /* now read application data 0x00 from the EAP-TLS 1.3 server
		     * if EAP-TLS is in use and session is not resumed
		     */
		    [strongSelf readApplicationData];
		    doUpdate = NO;
		} else {
		    strongSelf.state = EAPBoringSSLSessionStateConnected;
		    status = errSecSuccess;
		}
		break;
	    case nw_connection_state_failed:
		strongSelf.state = EAPBoringSSLSessionStateDisconnected;
		status = errSSLClosedAbort;
		break;
	    case nw_connection_state_cancelled:
		strongSelf.state = EAPBoringSSLSessionStateDisconnected;
		status = errSSLClosedGraceful;
		doUpdate = NO;
		break;
	    case nw_connection_state_waiting:
		if (strongSelf.state == EAPBoringSSLSessionStateConnecting) {
		    /* something went wrong */
		    status = errSecInternalError;
		    break;
		}
	    default:
		status = errSSLWouldBlock;
		doUpdate = NO;
		break;
	}
	if (doUpdate) {
	    [strongSelf updateHandshakeStatus:status];
	    EAPLOG_FL(LOG_DEBUG, "[State Change Handler]: updated handshake status to [%s]:[%d]",
		      EAPSecurityErrorString(status), (int)status);
	}
    });
    nw_connection_set_queue(self.connection, self.queue);
    self.state = EAPBoringSSLSessionStateIdle;
    nw_connection_start(self.connection);
}

- (void)stop
{
    if (self.handshakeStatus == errSSLServerAuthCompleted && self.secTrustCompletionHandler != nil) {
	/* This means the client application failed to trust the server certifciate */
	self.secTrustCompletionHandler(false);
	/* this should invoke output handler of the custom protocol with bad_certificate TLS Alert */
	EAPLOG_FL(LOG_INFO, "delivered trust evaluation result=failure to TLS protocol");
	nw_connection_set_state_changed_handler(self.connection, NULL);
	EAPLOG_FL(LOG_DEBUG, "removed network connection state change handler");
	/* wait till the handshake status gets updated */
	[self.statusUpdateLock lockWhenCondition:EAPBoringSSLSessionStatusUpdateReady];
	OSStatus status = self.handshakeStatus;
	[self.statusUpdateLock unlockWithCondition:EAPBoringSSLSessionStatusUpdatePending];
	EAPLOG_FL(LOG_DEBUG, "[session stopper]: handshake status updated to [%s]:[%d]",
		  EAPSecurityErrorString(status), (int)status);
    } else if (self.connection != nil) {
	self.state = EAPBoringSSLSessionStateDisconnected;
	nw_connection_set_state_changed_handler(self.connection, NULL);
	EAPLOG_FL(LOG_DEBUG, "removed network connection state change handler");
	nw_connection_cancel(self.connection);
	EAPLOG_FL(LOG_DEBUG, "cancelled network connection");
	self.connection = nil;
    }
}

- (void)updateHandshakeStatus:(OSStatus)status
{
    [self.statusUpdateLock lockWhenCondition:EAPBoringSSLSessionStatusUpdatePending];
    self.handshakeStatus = status;
    [self.statusUpdateLock unlockWithCondition:EAPBoringSSLSessionStatusUpdateReady];
}

@end

#pragma mark - EAPBoringSSLSession C API

EAPBoringSSLSessionContextRef
EAPBoringSSLSessionContextCreate(EAPBoringSSLSessionParametersRef sessionParameters, EAPBoringSSLClientContextRef clientContext)
{
    EAPBoringSSLSession *session = [[EAPBoringSSLSession alloc] init];
    if ([session setup:sessionParameters clientContext:clientContext] ) {
	EAPLOG_FL(LOG_INFO, "EAPBoringSSLSession instance created");
	return (__bridge_retained EAPBoringSSLSessionContextRef)session;
    }
    EAPLOG_FL(LOG_ERR, "failed to set up session");
    return NULL;
}

void
EAPBoringSSLSessionStop(EAPBoringSSLSessionContextRef sessionContext)
{
    if (sessionContext != NULL) {
	EAPBoringSSLSession *session = (__bridge EAPBoringSSLSession *)sessionContext;
	[session stop];
    }
}

void
EAPBoringSSLSessionContextFree(EAPBoringSSLSessionContextRef sessionContext)
{
    if (sessionContext != NULL) {
	EAPBoringSSLSession *session = (__bridge_transfer EAPBoringSSLSession *)sessionContext;
	[session stop];
	session = nil;
	EAPLOG_FL(LOG_INFO, "EAPBoringSSLSession instance freed");
    }
}

void
EAPBoringSSLSessionStart(EAPBoringSSLSessionContextRef sessionContext)
{
    if (sessionContext != NULL) {
	EAPBoringSSLSession *session = (__bridge EAPBoringSSLSession *)sessionContext;
	[session start];
    }
}

OSStatus
EAPBoringSSLSessionGetCurrentState(EAPBoringSSLSessionContextRef sessionContext, EAPBoringSSLSessionState *state)
{
    if (sessionContext == NULL) {
	return (errSecBadReq);
    }
    EAPBoringSSLSession *session = (__bridge EAPBoringSSLSession *)sessionContext;
    *state = session.state;
    return (errSecSuccess);
}

CFStringRef
EAPBoringSSLSessionGetCurrentStateDescription(EAPBoringSSLSessionState state)
{
    switch(state) {
	case EAPBoringSSLSessionStateIdle:
	    return CFSTR("idle");
	case EAPBoringSSLSessionStateConnecting:
	    return CFSTR("connecting");
	case EAPBoringSSLSessionStateConnected:
	    return CFSTR("connected");
	case EAPBoringSSLSessionStateDisconnected:
	    return CFSTR("disconnecting");
    }
}

void
EAPBoringSSLUtilGetPreferredTLSVersions(CFDictionaryRef properties, tls_protocol_version_t *min, tls_protocol_version_t *max)
{
    if (properties == NULL) {
	*min = tls_protocol_version_TLSv10;
	*max = tls_protocol_version_TLSv12;
	return;
    }
    CFStringRef tls_min_ver = CFDictionaryGetValue(properties, kEAPClientPropTLSMinimumVersion);
    CFStringRef tls_max_ver = CFDictionaryGetValue(properties, kEAPClientPropTLSMaximumVersion);

    EAPLOG_FL(LOG_DEBUG, "configured minimum TLS version: [%@], maximum TLS version: [%@]",
	      tls_min_ver != NULL ? tls_min_ver : CFSTR("NONE"),
	      tls_max_ver != NULL ? tls_max_ver : CFSTR("NONE"));

    if (isA_CFString(tls_min_ver) != NULL) {
	if (CFEqual(tls_min_ver, kEAPTLSVersion1_0)) {
	    *min = tls_protocol_version_TLSv10;
	} else if (CFEqual(tls_min_ver, kEAPTLSVersion1_1)) {
	    *min = tls_protocol_version_TLSv11;
	} else if (CFEqual(tls_min_ver, kEAPTLSVersion1_2)) {
	    *min = tls_protocol_version_TLSv12;
	} else if (CFEqual(tls_min_ver, kEAPTLSVersion1_3)) {
	    *min = tls_protocol_version_TLSv13;
	} else {
	    *min = tls_protocol_version_TLSv10;
	    EAPLOG_FL(LOG_ERR, "invalid minimum TLS version");
	}
    } else {
	*min = tls_protocol_version_TLSv10;
    }

    if (isA_CFString(tls_max_ver) != NULL) {
	if (CFEqual(tls_max_ver, kEAPTLSVersion1_0)) {
	    *max = tls_protocol_version_TLSv10;
	} else if (CFEqual(tls_max_ver, kEAPTLSVersion1_1)) {
	    *max = tls_protocol_version_TLSv11;
	}  else if (CFEqual(tls_max_ver, kEAPTLSVersion1_2)) {
	    *max = tls_protocol_version_TLSv12;
	} else if (CFEqual(tls_max_ver, kEAPTLSVersion1_3)) {
	    *max = tls_protocol_version_TLSv13;
	} else {
	    *max = tls_protocol_version_TLSv12;
	    EAPLOG_FL(LOG_ERR, "invalid maximum TLS version");
	}
    } else {
	*max = tls_protocol_version_TLSv12;
    }
    if (*min > *max) {
	EAPLOG_FL(LOG_ERR, "minimum TLS version cannot be higher than maximum TLS version");
	*min = *max;
    }
    return;
}

OSStatus
EAPBoringSSLSessionHandshake(EAPBoringSSLSessionContextRef sessionContext)
{
    if(sessionContext == NULL) {
	return errSecParam;
    }
    EAPBoringSSLSession *session = (__bridge EAPBoringSSLSession *)sessionContext;
    /* now block till the TLS protocol updates the handshake status */
    OSStatus status = [session handshake];
    return status;
}

OSStatus
EAPBoringSSLSessionCopyServerCertificates(EAPBoringSSLSessionContextRef sessionContext, CFArrayRef *certs)
{
    if (sessionContext == NULL) {
	return errSecParam;
    }
    EAPBoringSSLSession *session = (__bridge EAPBoringSSLSession *)sessionContext;

    if (session.serverSecTrust != NULL) {
	return SecTrustCopyInputCertificates(session.serverSecTrust, certs);
    } else {
	NSArray *peerCerts = [session copyPeerCertificateChain];
	if (peerCerts.count > 0) {
	    *certs = (__bridge_retained CFArrayRef)peerCerts;
	    return errSecSuccess;
	}
    }
    return errSecParam;
}

SecTrustRef
EAPBoringSSLSessionGetSecTrust(EAPBoringSSLSessionContextRef sessionContext)
{
    if (sessionContext == NULL) {
	return NULL;
    }
    EAPBoringSSLSession *session = (__bridge EAPBoringSSLSession *)sessionContext;

    return (session.serverSecTrust);
}

OSStatus
EAPBoringSSLSessionComputeKeyData(EAPBoringSSLSessionContextRef sessionContext, void *key, int key_length)
{
    if (sessionContext == NULL || key == NULL) {
	return errSecParam;
    }
    EAPBoringSSLSession *session = (__bridge EAPBoringSSLSession *)sessionContext;
    NSData *keyData = [session getEAPKeyMaterial];
    if (keyData == nil) {
	EAPLOG_FL(LOG_ERR, "key computation failed");
	return errSecInternalError;
    }
    if (key_length > keyData.length) {
	return errSecParam;
    }
    bcopy(keyData.bytes, key, key_length);
    EAPLOG_FL(LOG_NOTICE, "key computation is successful");
    return errSecSuccess;
}

OSStatus
EAPBoringSSLSessionGetNegotiatedTLSVersion(EAPBoringSSLSessionContextRef sessionContext, tls_protocol_version_t *tlsVersion)
{
    if (sessionContext == NULL || tlsVersion == NULL) {
	return errSecParam;
    }
    EAPBoringSSLSession *session = (__bridge EAPBoringSSLSession *)sessionContext;
    *tlsVersion = [session getNegotiatedTLSVersion];
    return errSecSuccess;
}

OSStatus
EAPBoringSSLSessionGetSessionResumed(EAPBoringSSLSessionContextRef sessionContext, bool *sessionResumed)
{
    if (sessionContext == NULL || sessionResumed == NULL) {
	return errSecParam;
    }
    EAPBoringSSLSession *session = (__bridge EAPBoringSSLSession *)sessionContext;
    *sessionResumed = [session getSessionResumed] ? true : false;
    return errSecSuccess;
}
