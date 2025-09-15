//
//  SecProtocol.m
//  Security
//

#include <Security/SecProtocolPriv.h>
#include <Security/SecProtocolRestrictedOptionsGoAwayIfNotApprovedForEPSKsPriv.h>
#include "SecProtocolInternal.h"

#define SEC_PROTOCOL_METADATA_VALIDATE(m,r)                                  \
    if ((m == NULL) || ((size_t)m == 0)) {                                    \
        return r;                                                            \
    }

#define SEC_PROTOCOL_OPTIONS_VALIDATE(o,r)                                   \
    if (o == NULL) {                                                         \
        return r;                                                            \
    }

@implementation SecSessionInfo
- (nonnull instancetype)initWithPSK:(nonnull NSData *)psk :(nonnull NSData *)psk_id :(uint32_t)ticket_age_add :(uint64_t)ticket_creation_time :(uint64_t)ticket_lifetime {
    if (self = [super init]) {
        _psk = [psk copy];
        _psk_id = [psk_id copy];
        _ticket_age_add = ticket_age_add;
        _ticket_creation_time = ticket_creation_time;
        _ticket_lifetime = ticket_lifetime;
    }
    return self;
}
@end

@implementation SecExternalPreSharedKey
- (nonnull instancetype)initWithExternalIdentity:(nonnull NSData *)external_identity :(nonnull NSData *)epsk :(nonnull NSData *)context {
    if (self = [super init]) {
        self.external_identity = external_identity;
        self.epsk = epsk;
        self.context = context;
    }
    return self;
}
@end

@implementation ExternalPreSharedKey : SecExternalPreSharedKey
@end

@implementation SecOfferedEPSK
- (nonnull instancetype)initWithExternalIdentity:(nonnull NSData *)external_identity :(NSData *)context {
    if (self = [super init]) {
        self.external_identity = external_identity;
        self.context = context;
    }
    return self;
}
@end

@implementation OfferedEPSK : SecOfferedEPSK
@end

SecSessionInfo*
sec_protocol_metadata_get_sec_session_ticket_info(sec_protocol_metadata_t metadata)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, NULL);

    __block SecSessionInfo* session_ticket_info = nil;
    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *metadata_handle) {
        sec_protocol_metadata_content_t metadata_content = (sec_protocol_metadata_content_t)metadata_handle;
        SEC_PROTOCOL_METADATA_VALIDATE(metadata_content, false);

        SecSessionInfo* origSessionTicketInfo = sec_protocol_content_metadata_get_session_ticket_info(metadata_content);
        if (origSessionTicketInfo != nil) {
            // Create a copy of the session info
            session_ticket_info = [[SecSessionInfo alloc] initWithPSK:origSessionTicketInfo.psk
                                                                     :origSessionTicketInfo.psk_id
                                                                     :origSessionTicketInfo.ticket_age_add
                                                                     :origSessionTicketInfo.ticket_creation_time
                                                                     :origSessionTicketInfo.ticket_lifetime];
        }
        return true;
    });

    return session_ticket_info;
}

void
sec_protocol_options_set_session_ticket_info(sec_protocol_options_t options, SecSessionInfo* session_info)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);
    SEC_PROTOCOL_OPTIONS_VALIDATE(session_info,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        // Create a copy of the session info
        SecSessionInfo *session_info_copy = [[SecSessionInfo alloc] initWithPSK:session_info.psk
                                                                               :session_info.psk_id
                                                                               :session_info.ticket_age_add
                                                                               :session_info.ticket_creation_time
                                                                               :session_info.ticket_lifetime];

        content->session_ticket_info = CFBridgingRetain(session_info_copy);
        return true;
    });
}

void
sec_protocol_options_add_external_pre_shared_key(sec_protocol_options_t options, SecExternalPreSharedKey *EPSK) {
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);
    SEC_PROTOCOL_OPTIONS_VALIDATE(EPSK,);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        NSMutableArray *external_pre_shared_keys = NULL;
        if (content->external_pre_shared_keys == NULL) {
            external_pre_shared_keys = [NSMutableArray array];
            content->external_pre_shared_keys = CFBridgingRetain(external_pre_shared_keys);
        } else {
            external_pre_shared_keys = sec_protocol_options_content_get_external_pre_shared_keys(content);
        }
        [external_pre_shared_keys addObject:EPSK];

        return true;
    });
}

void
sec_protocol_options_set_external_pre_shared_key_selection_block(sec_protocol_options_t options, sec_protocol_external_pre_shared_key_selection_t external_psk_selection_block, dispatch_queue_t external_psk_selection_queue)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,);
    SEC_PROTOCOL_OPTIONS_VALIDATE(external_psk_selection_block,);
    SEC_PROTOCOL_OPTIONS_VALIDATE(external_psk_selection_queue,);

    sec_protocol_options_set_external_pre_shared_key_selection_queue_helper(options, external_psk_selection_queue);

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        if (content->external_psk_selection_block != NULL) {
            CFReleaseNull(content->external_psk_selection_block);
        }

        content->external_psk_selection_block = CFBridgingRetain(external_psk_selection_block);
        return true;
    });
}

void
sec_protocol_options_set_use_raw_external_pre_shared_keys(sec_protocol_options_t options, bool enable) {
    SEC_PROTOCOL_OPTIONS_VALIDATE(options, );
    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        content->enable_raw_external_pre_shared_keys = enable;
        return true;
    });
}

bool
sec_protocol_options_get_raw_external_pre_shared_keys_enabled(sec_protocol_options_t options) {
    SEC_PROTOCOL_OPTIONS_VALIDATE(options, false);
    __block bool raw_epsks_enabled = false;
    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);
        raw_epsks_enabled = content->enable_raw_external_pre_shared_keys;
        return true;
    });
    return raw_epsks_enabled;
}

SecSessionInfo*
sec_protocol_options_get_sec_session_ticket_info(sec_protocol_options_t options)
{
    SEC_PROTOCOL_OPTIONS_VALIDATE(options,nil);

    __block SecSessionInfo* session_ticket_info = nil;

    (void)sec_protocol_options_access_handle(options, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_OPTIONS_VALIDATE(content, false);

        SecSessionInfo* origSessionTicketInfo = sec_protocol_options_content_get_session_ticket_info(content);
        if (origSessionTicketInfo != nil) {
            // Create a copy of the session info
            session_ticket_info = [[SecSessionInfo alloc] initWithPSK:origSessionTicketInfo.psk
                                                                     :origSessionTicketInfo.psk_id
                                                                     :origSessionTicketInfo.ticket_age_add
                                                                     :origSessionTicketInfo.ticket_creation_time
                                                                     :origSessionTicketInfo.ticket_lifetime];
        }
        return true;
    });
    return session_ticket_info;
}

void
sec_protocol_metadata_set_session_ticket_info(sec_protocol_metadata_t metadata, SecSessionInfo *sessionInfo)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, );
    SEC_PROTOCOL_METADATA_VALIDATE(sessionInfo, );

    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);

        // Create a copy of the session info
        SecSessionInfo *session_info_copy = [[SecSessionInfo alloc] initWithPSK:sessionInfo.psk
                                                                               :sessionInfo.psk_id
                                                                               :sessionInfo.ticket_age_add
                                                                               :sessionInfo.ticket_creation_time
                                                                               :sessionInfo.ticket_lifetime];

        if (content->session_ticket_info) {
            CFRelease(content->session_ticket_info);
        }
        content->session_ticket_info = (__bridge_retained CFTypeRef)session_info_copy;
        return true;
    });

    return;
}

bool
sec_protocol_metadata_get_tls_negotiated_epsk(sec_protocol_metadata_t metadata)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, 0);

    __block bool negotiated_epsk = 0;
    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);
        negotiated_epsk = content->external_psk_used;
        return true;
    });

    return negotiated_epsk;
}

bool
sec_protocol_metadata_get_tls_epsk_offered(sec_protocol_metadata_t metadata)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, 0);

    __block bool epsk_offered = false;
    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);
        epsk_offered = content->external_psk_offered;
        return true;
    });

    return epsk_offered;
}


bool
sec_protocol_metadata_get_tls_pake_offered(sec_protocol_metadata_t metadata)
{
    SEC_PROTOCOL_METADATA_VALIDATE(metadata, 0);

    __block bool pake_offered = false;
    (void)sec_protocol_metadata_access_handle(metadata, ^bool(void *handle) {
        sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);
        pake_offered = content->pake_offered;
        return true;
    });

    return pake_offered;
}

bool
sec_session_tickets_are_equal(CFTypeRef session_ticket_infoA, CFTypeRef session_ticket_infoB)
{
    if (session_ticket_infoA == session_ticket_infoB) {
        return true;
    }

    if (session_ticket_infoA == NULL || session_ticket_infoA == NULL) {
        return false;
    }

    SecSessionInfo *secSessionTicketInfoA = (__bridge SecSessionInfo*)session_ticket_infoA;
    SecSessionInfo *secSessionTicketInfoB = (__bridge SecSessionInfo*)session_ticket_infoB;

    if (secSessionTicketInfoA == secSessionTicketInfoB) {
        return true;
    }

    if (secSessionTicketInfoA == NULL || secSessionTicketInfoB == NULL) {
        return false;
    }
    if (![secSessionTicketInfoA.psk isEqualToData:secSessionTicketInfoB.psk]) {
        return false;
    }
    if (![secSessionTicketInfoA.psk_id isEqualToData:secSessionTicketInfoB.psk_id]) {
        return false;
    }
    if (secSessionTicketInfoA.ticket_age_add != secSessionTicketInfoB.ticket_age_add) {
        return false;
    }
    if (secSessionTicketInfoA.ticket_lifetime != secSessionTicketInfoB.ticket_lifetime) {
        return false;
    }
    if (secSessionTicketInfoA.ticket_creation_time != secSessionTicketInfoB.ticket_creation_time) {
        return false;
    }
    return true;
}
