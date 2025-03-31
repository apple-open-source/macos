//
//  SecProtocolConfigurationTest.m
//  SecureTransportTests
//

#import <XCTest/XCTest.h>

#include <os/log.h>
#include <dlfcn.h>
#include <sys/param.h>

#import "SecProtocolConfiguration.h"
#import "SecProtocolPriv.h"
#import "SecProtocolInternal.h"

#import <nw/private.h> // Needed for the mock protocol

#define SEC_PROTOCOL_METADATA_VALIDATE(m, r) \
    if (((void *)(m) == NULL) || ((size_t)(m) == 0)) { \
        return (r); \
    }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

typedef struct mock_protocol {
    struct nw_protocol protocol;
    char *name;
} *mock_protocol_t;

static nw_protocol_t
_mock_protocol_create_extended(nw_protocol_identifier_const_t identifier,
                               nw_endpoint_t endpoint,
                               nw_parameters_t parameters)
{
    mock_protocol_t handle = (mock_protocol_t)calloc(1, sizeof(struct mock_protocol));
    if (handle == NULL) {
        return NULL;
    }

    struct nw_protocol_callbacks *callbacks = (struct nw_protocol_callbacks *)malloc(sizeof(struct nw_protocol_callbacks));
    memset(callbacks, 0, sizeof(struct nw_protocol_callbacks));

    handle->protocol.callbacks = callbacks;
    handle->protocol.handle = (void *)handle;

    return &handle->protocol;
}

static bool
mock_protocol_register_extended(nw_protocol_identifier_const_t identifier,
                                nw_protocol_create_extended_f create_extended_function)
{
    static void *libnetworkImage = NULL;
    static dispatch_once_t onceToken;
    static bool (*_nw_protocol_register_extended)(nw_protocol_identifier_const_t, nw_protocol_create_extended_f) = NULL;

    dispatch_once(&onceToken, ^{
        libnetworkImage = dlopen("/usr/lib/libnetwork.dylib", RTLD_LAZY | RTLD_LOCAL);
        if (NULL != libnetworkImage) {
            _nw_protocol_register_extended = (__typeof(_nw_protocol_register_extended))dlsym(libnetworkImage, "nw_protocol_register_extended");
            if (NULL == _nw_protocol_register_extended) {
                os_log_error(OS_LOG_DEFAULT, "dlsym libnetwork nw_protocol_register_extended");
            }
        } else {
            os_log_error(OS_LOG_DEFAULT, "dlopen libnetwork");
        }
    });

    if (_nw_protocol_register_extended == NULL) {
        return false;
    }

    return _nw_protocol_register_extended(identifier, create_extended_function);
}

static nw_protocol_identifier_t
_mock_protocol_identifier(const char *name, size_t name_len)
{
    static struct nw_protocol_identifier mock_identifier = {};
    static dispatch_once_t onceToken;

    dispatch_once(&onceToken, ^{
        memset(&mock_identifier, 0, sizeof(mock_identifier));

        strlcpy((char *)mock_identifier.name, name, name_len);

        mock_identifier.level = nw_protocol_level_application;
        mock_identifier.mapping = nw_protocol_mapping_one_to_one;

        mock_protocol_register_extended(&mock_identifier, _mock_protocol_create_extended);
    });

    return &mock_identifier;
}

static void * _Nullable
mock_protocol_allocate_metadata(__unused nw_protocol_definition_t definition)
{
    return calloc(1, sizeof(struct sec_protocol_metadata_content));
}

static void
mock_protocol_deallocate_metadata(__unused nw_protocol_definition_t definition, void *metadata)
{
    sec_protocol_metadata_content_t content = (sec_protocol_metadata_content_t)metadata;
    if (content) {
        // pass
    }
    free(content);
}

static void
mock_protocol_set_metadata_allocator(nw_protocol_definition_t definition, nw_protocol_definition_allocate_f allocator, nw_protocol_definition_deallocate_f deallocator)
{
    static void *libnetworkImage = NULL;
    static dispatch_once_t onceToken;
    static void (*_nw_protocol_definition_set_metadata_allocator)(nw_protocol_definition_t, nw_protocol_definition_allocate_f, nw_protocol_definition_deallocate_f) = NULL;

    dispatch_once(&onceToken, ^{
        libnetworkImage = dlopen("/usr/lib/libnetwork.dylib", RTLD_LAZY | RTLD_LOCAL);
        if (NULL != libnetworkImage) {
            _nw_protocol_definition_set_metadata_allocator = (__typeof(_nw_protocol_definition_set_metadata_allocator))dlsym(libnetworkImage, "nw_protocol_definition_set_metadata_allocator");
            if (NULL == _nw_protocol_definition_set_metadata_allocator) {
                os_log_error(OS_LOG_DEFAULT, "dlsym libnetwork nw_protocol_definition_set_metadata_allocator");
            }
        } else {
            os_log_error(OS_LOG_DEFAULT, "dlopen libnetwork");
        }
    });

    if (_nw_protocol_definition_set_metadata_allocator == NULL) {
        return;
    }

    _nw_protocol_definition_set_metadata_allocator(definition, allocator, deallocator);
}

static void * _Nullable
mock_protocol_copy_options(__unused nw_protocol_definition_t definition, void *options)
{
    void *new_options = calloc(1, sizeof(struct sec_protocol_options_content));

    sec_protocol_options_content_t copy = (sec_protocol_options_content_t)new_options;
    sec_protocol_options_content_t original = (sec_protocol_options_content_t)options;

    copy->min_version = original->min_version;
    copy->max_version = original->max_version;
    copy->disable_sni = original->disable_sni;
    copy->enable_fallback_attempt = original->enable_fallback_attempt;
    copy->enable_false_start = original->enable_false_start;
    copy->enable_tickets = original->enable_tickets;
    copy->enable_sct = original->enable_sct;
    copy->enable_ocsp = original->enable_ocsp;
    copy->enable_resumption = original->enable_resumption;
    copy->enable_renegotiation = original->enable_renegotiation;
    copy->enable_early_data = original->enable_early_data;

    if (original->server_name) {
        copy->server_name = strdup(original->server_name);
    }
    if (original->identity) {
        copy->identity = original->identity;
    }
    if (original->application_protocols) {
        copy->application_protocols = xpc_copy(original->application_protocols);
    }
    if (original->ciphersuites) {
        copy->ciphersuites = xpc_copy(original->ciphersuites);
    }
    if (original->dh_params) {
        copy->dh_params = original->dh_params;
    }
    if (original->key_update_block) {
        copy->key_update_block = original->key_update_block;
        copy->key_update_queue = original->key_update_queue;
    }
    if (original->challenge_block) {
        copy->challenge_block = original->challenge_block;
        copy->challenge_queue = original->challenge_queue;
    }
    if (original->verify_block) {
        copy->verify_block = original->verify_block;
        copy->verify_queue = original->verify_queue;
    }
    if (original->session_state) {
        copy->session_state = original->session_state;
    }
    if (original->session_update_block) {
        copy->session_update_block = original->session_update_block;
        copy->session_update_queue = original->session_update_queue;
    }
    if (original->pre_shared_keys) {
        copy->pre_shared_keys = xpc_copy(original->pre_shared_keys);
    }

    return new_options;
}

static void * _Nullable
mock_protocol_allocate_options(__unused nw_protocol_definition_t definition)
{
    return calloc(1, sizeof(struct sec_protocol_options_content));
}

static void
mock_protocol_deallocate_options(__unused nw_protocol_definition_t definition, void *options)
{
    sec_protocol_options_content_t content = (sec_protocol_options_content_t)options;
    if (content) {
        // pass
    }
    free(content);
}

static void
mock_protocol_set_options_allocator(nw_protocol_definition_t definition,
                                    nw_protocol_definition_allocate_f allocate_function,
                                    nw_protocol_definition_copy_f copy_function,
                                    nw_protocol_definition_deallocate_f deallocate_function)
{
    static void *libnetworkImage = NULL;
    static dispatch_once_t onceToken;
    static void (*_nw_protocol_definition_set_options_allocator)(nw_protocol_definition_t, nw_protocol_definition_allocate_f, nw_protocol_definition_copy_f, nw_protocol_definition_deallocate_f) = NULL;

    dispatch_once(&onceToken, ^{
        libnetworkImage = dlopen("/usr/lib/libnetwork.dylib", RTLD_LAZY | RTLD_LOCAL);
        if (NULL != libnetworkImage) {
            _nw_protocol_definition_set_options_allocator = (__typeof(_nw_protocol_definition_set_options_allocator))dlsym(libnetworkImage, "nw_protocol_definition_set_options_allocator");
            if (NULL == _nw_protocol_definition_set_options_allocator) {
                os_log_error(OS_LOG_DEFAULT, "dlsym libnetwork nw_protocol_definition_set_options_allocator");
            }
        } else {
            os_log_error(OS_LOG_DEFAULT, "dlopen libnetwork");
        }
    });

    if (_nw_protocol_definition_set_options_allocator == NULL) {
        return;
    }

    _nw_protocol_definition_set_options_allocator(definition, allocate_function, copy_function, deallocate_function);
}

static nw_protocol_definition_t
mock_protocol_definition_create_with_identifier(nw_protocol_identifier_const_t identifier)
{
    static void *libnetworkImage = NULL;
    static dispatch_once_t onceToken;
    static nw_protocol_definition_t (*_nw_protocol_definition_create_with_identifier)(nw_protocol_identifier_const_t) = NULL;

    dispatch_once(&onceToken, ^{
        libnetworkImage = dlopen("/usr/lib/libnetwork.dylib", RTLD_LAZY | RTLD_LOCAL);
        if (NULL != libnetworkImage) {
            _nw_protocol_definition_create_with_identifier = (__typeof(_nw_protocol_definition_create_with_identifier))dlsym(libnetworkImage, "nw_protocol_definition_create_with_identifier");
            if (NULL == _nw_protocol_definition_create_with_identifier) {
                os_log_error(OS_LOG_DEFAULT, "dlsym libnetwork nw_protocol_definition_create_with_identifier");
            }
        } else {
            os_log_error(OS_LOG_DEFAULT, "dlopen libnetwork");
        }
    });

    if (_nw_protocol_definition_create_with_identifier == NULL) {
        return NULL;
    }

    return _nw_protocol_definition_create_with_identifier(identifier);
}

static nw_protocol_definition_t
mock_protocol_copy_definition(void)
{
    static nw_protocol_definition_t definition = NULL;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        const char *mock_protocol_name = "SecProtocolConfigTestMock";
        definition = mock_protocol_definition_create_with_identifier(_mock_protocol_identifier(mock_protocol_name, strlen(mock_protocol_name)));
        mock_protocol_set_options_allocator(definition,
                                            mock_protocol_allocate_options,
                                            mock_protocol_copy_options,
                                            mock_protocol_deallocate_options);
        mock_protocol_set_metadata_allocator(definition,
                                             mock_protocol_allocate_metadata,
                                             mock_protocol_deallocate_metadata);

    });

    return definition;
}

@interface SecProtocolConfigurationTest : XCTestCase
@end

@implementation SecProtocolConfigurationTest

- (void)setUp {
}

- (void)tearDown {
}

- (sec_protocol_options_t)create_sec_protocol_options {
    static void *libnetworkImage = NULL;
    static dispatch_once_t onceToken;

    static sec_protocol_options_t (*_nw_protocol_create_options)(nw_protocol_definition_t) = NULL;

    dispatch_once(&onceToken, ^{
        libnetworkImage = dlopen("/usr/lib/libnetwork.dylib", RTLD_LAZY | RTLD_LOCAL);
        if (NULL != libnetworkImage) {
            _nw_protocol_create_options = (__typeof(_nw_protocol_create_options))dlsym(libnetworkImage, "nw_protocol_create_options");
            if (NULL == _nw_protocol_create_options) {
                os_log_error(OS_LOG_DEFAULT, "dlsym libnetwork _nw_protocol_create_options");
            }
        } else {
            os_log_error(OS_LOG_DEFAULT, "dlopen libnetwork");
        }
    });

    if (_nw_protocol_create_options == NULL) {
        return nil;
    }

    return (sec_protocol_options_t)_nw_protocol_create_options(mock_protocol_copy_definition());
}

static bool
isLocalTLD(NSString *host)
{
    if ([host length] == 0) {
        return false;
    }
    if ([host hasSuffix:@".local"] || [host hasSuffix:@".local."]) {
        return true;
    }
    if ([host rangeOfString:@"."].location == NSNotFound) {
        return true;
    }
    return false;
}

#pragma mark - ATS Exceptions Tests

- (void)runATSExceptionTestForHostname:(const char *)hostname
                         configuration:(sec_protocol_configuration_t)configuration
                             isAddress:(bool)isAddress
                              isDirect:(bool)isDirect
                           atsRequired:(bool)atsRequired
                         minTLSVersion:(tls_protocol_version_t)minTLSVersion
                forwardSecrecyRequired:(bool)forwardSecrecyRequired
{
    sec_protocol_options_t options = [self create_sec_protocol_options];
    sec_protocol_options_set_sec_protocol_configuration(options, configuration);
    sec_protocol_options_t transformed = nil;
    if (isAddress) {
        XCTAssertTrue(atsRequired == sec_protocol_configuration_tls_required_for_address(configuration, hostname, isDirect), "TLS should have been required: %d for hostname %s", atsRequired, hostname);
        transformed = sec_protocol_configuration_copy_transformed_options_for_address(options, hostname, isDirect);
    } else {
        XCTAssertTrue(atsRequired == sec_protocol_configuration_tls_required_for_host(configuration, hostname, isDirect), "TLS should have been required: %d for hostname %s", atsRequired, hostname);
        transformed = sec_protocol_configuration_copy_transformed_options_for_host(options, hostname, isDirect);
    }
    (void)sec_protocol_options_access_handle(transformed, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);
        XCTAssertTrue(content->ats_required == atsRequired, "ats_required should have been set to: %d for hostname %s", atsRequired, hostname);
        XCTAssertTrue(content->min_version == minTLSVersion, "Minimum TLS version should have been set to: %d, instead received: %d for hostname %s", minTLSVersion, content->min_version, hostname);
        if (forwardSecrecyRequired || content->ats_required) {
            XCTAssertTrue(content->ciphersuites != nil, "expected ciphersuites to be set for hostname %s since forwardSecrecyRequired = %d, ats_required = %d", hostname, forwardSecrecyRequired, content->ats_required);
        } else {
            XCTAssertTrue(content->ciphersuites == nil, "forward secrecy was not required for hostname %s, expected ciphersuites to be nil", hostname);
        }
        return true;
    });
}

- (void)testExceptionRaisesSecurityRequirements
{
    NSDictionary *exampleATS = @{@"NSAllowsArbitraryLoads" : @YES,
                                 @"NSExceptionDomains" : @{
                                     @"neverssl.com" : @{
                                         @"NSExceptionAllowsInsecureHTTPLoads" : @NO,
                                         @"NSExceptionMinimumTLSVersion" : @"TLSv1.3",
                                         @"NSExceptionRequiresForwardSecrecy" : @YES
                                     },
                                     @"google.com" : @{
                                         @"NSExceptionAllowsInsecureHTTPLoads" : @YES,
                                         @"NSExceptionMinimumTLSVersion" : @"TLSv1.3",
                                         @"NSExceptionRequiresForwardSecrecy" : @YES
                                     },
                                     @"youtube.com" : @{
                                         @"NSExceptionRequiresForwardSecrecy" : @YES
                                     },
                                     @"myhost.local" : @{
                                         @"NSExceptionAllowsInsecureHTTPLoads" : @NO,
                                         @"NSExceptionRequiresForwardSecrecy" : @YES
                                     },
                                 }};

    sec_protocol_configuration_builder_t builder = sec_protocol_configuration_builder_create((__bridge CFDictionaryRef) exampleATS, false);
    sec_protocol_configuration_t configuration = sec_protocol_configuration_create_with_builder(builder);
    XCTAssertTrue(configuration != nil, @"failed to build configuration");
    if (!configuration) {
        return;
    }
    [self runATSExceptionTestForHostname:"example.com" configuration:configuration isAddress:false isDirect:false atsRequired:false minTLSVersion:0 forwardSecrecyRequired:false];
    [self runATSExceptionTestForHostname:"neverssl.com" configuration:configuration isAddress:false isDirect:false atsRequired:true minTLSVersion:tls_protocol_version_TLSv13 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"google.com" configuration:configuration isAddress:false isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv13 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"youtube.com" configuration:configuration isAddress:false isDirect:false atsRequired:true minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"myhost.local" configuration:configuration isAddress:false isDirect:true atsRequired:true minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];

    // Check built in exceptions
    [self runATSExceptionTestForHostname:"apple.com" configuration:configuration isAddress:false isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:false];
    [self runATSExceptionTestForHostname:"csd4.apple.com" configuration:configuration isAddress:false isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:false];
    [self runATSExceptionTestForHostname:"setup.icloud.com" configuration:configuration isAddress:false isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:false];
    [self runATSExceptionTestForHostname:"ls.apple.com" configuration:configuration isAddress:false isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv10 forwardSecrecyRequired:false];
    [self runATSExceptionTestForHostname:"gs.apple.com" configuration:configuration isAddress:false isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv10 forwardSecrecyRequired:false];
    [self runATSExceptionTestForHostname:"geo.apple.com" configuration:configuration isAddress:false isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv10 forwardSecrecyRequired:false];
    [self runATSExceptionTestForHostname:"is.autonavi.com" configuration:configuration isAddress:false isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv10 forwardSecrecyRequired:false];
    [self runATSExceptionTestForHostname:"apple-mapkit.com" configuration:configuration isAddress:false isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv10 forwardSecrecyRequired:false];
}

- (void)testBuiltInExceptions
{

    NSDictionary *exampleATS = @{@"NSAllowsArbitraryLoads" : @NO};
    sec_protocol_configuration_builder_t builder = sec_protocol_configuration_builder_create((__bridge CFDictionaryRef) exampleATS, false);
    sec_protocol_configuration_t configuration = sec_protocol_configuration_create_with_builder(builder);
    XCTAssertTrue(configuration != nil, @"failed to build configuration");
    if (!configuration) {
        return;
    }
    [self runATSExceptionTestForHostname:"example.com" configuration:configuration isAddress:false isDirect:false atsRequired:true minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"neverssl.com" configuration:configuration isAddress:false isDirect:false atsRequired:true minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"myhost.local" configuration:configuration isAddress:false isDirect:true atsRequired:false minTLSVersion:0 forwardSecrecyRequired:false];

    // Check built in exceptions
    [self runATSExceptionTestForHostname:"apple.com" configuration:configuration isAddress:false isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:false];
    [self runATSExceptionTestForHostname:"csd4.apple.com" configuration:configuration isAddress:false isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:false];
    [self runATSExceptionTestForHostname:"setup.icloud.com" configuration:configuration isAddress:false isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:false];
    [self runATSExceptionTestForHostname:"ls.apple.com" configuration:configuration isAddress:false isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv10 forwardSecrecyRequired:false];
    [self runATSExceptionTestForHostname:"gs.apple.com" configuration:configuration isAddress:false isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv10 forwardSecrecyRequired:false];
    [self runATSExceptionTestForHostname:"geo.apple.com" configuration:configuration isAddress:false isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv10 forwardSecrecyRequired:false];
    [self runATSExceptionTestForHostname:"is.autonavi.com" configuration:configuration isAddress:false isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv10 forwardSecrecyRequired:false];
    [self runATSExceptionTestForHostname:"apple-mapkit.com" configuration:configuration isAddress:false isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv10 forwardSecrecyRequired:false];
}

- (void)testExceptionOverridesBuiltInException
{
    NSDictionary *exampleATS = @{@"NSAllowsArbitraryLoads" : @NO,
                                 @"NSExceptionDomains" : @{
                                     @"neverssl.com" : @{
                                         @"NSExceptionAllowsInsecureHTTPLoads" : @NO,
                                         @"NSExceptionMinimumTLSVersion" : @"TLSv1.3",
                                         @"NSExceptionRequiresForwardSecrecy" : @YES
                                     },
                                     @"apple.com" : @{
                                         @"NSExceptionAllowsInsecureHTTPLoads" : @NO,
                                         @"NSExceptionMinimumTLSVersion" : @"TLSv1.3",
                                         @"NSIncludesSubdomains" : @NO,
                                         @"NSExceptionRequiresForwardSecrecy" : @YES
                                     },
                                     @"setup.icloud.com" : @{
                                         @"NSExceptionAllowsInsecureHTTPLoads" : @NO,
                                         @"NSExceptionMinimumTLSVersion" : @"TLSv1.3",
                                     },
                                 }};

    sec_protocol_configuration_builder_t builder = sec_protocol_configuration_builder_create((__bridge CFDictionaryRef) exampleATS, false);
    sec_protocol_configuration_t configuration = sec_protocol_configuration_create_with_builder(builder);
    XCTAssertTrue(configuration != nil, @"failed to build configuration");
    if (!configuration) {
        return;
    }
    [self runATSExceptionTestForHostname:"example.com" configuration:configuration isAddress:false isDirect:false atsRequired:true minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"neverssl.com" configuration:configuration isAddress:false isDirect:false atsRequired:true minTLSVersion:tls_protocol_version_TLSv13 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"google.com" configuration:configuration isAddress:false isDirect:false atsRequired:true minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"youtube.com" configuration:configuration isAddress:false isDirect:false atsRequired:true minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"myhost.local" configuration:configuration isAddress:false isDirect:true atsRequired:false minTLSVersion:0 forwardSecrecyRequired:false];

    // Check built in exceptions
    [self runATSExceptionTestForHostname:"apple.com" configuration:configuration isAddress:false isDirect:false atsRequired:true minTLSVersion:tls_protocol_version_TLSv13 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"csd4.apple.com" configuration:configuration isAddress:false isDirect:false atsRequired:true minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"setup.icloud.com" configuration:configuration isAddress:false isDirect:false atsRequired:true minTLSVersion:tls_protocol_version_TLSv13 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"test.setup.icloud.com" configuration:configuration isAddress:false isDirect:false atsRequired:true minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"ls.apple.com" configuration:configuration isAddress:false isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv10 forwardSecrecyRequired:false];
    [self runATSExceptionTestForHostname:"gs.apple.com" configuration:configuration isAddress:false isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv10 forwardSecrecyRequired:false];
    [self runATSExceptionTestForHostname:"geo.apple.com" configuration:configuration isAddress:false isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv10 forwardSecrecyRequired:false];
    [self runATSExceptionTestForHostname:"is.autonavi.com" configuration:configuration isAddress:false isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv10 forwardSecrecyRequired:false];
    [self runATSExceptionTestForHostname:"apple-mapkit.com" configuration:configuration isAddress:false isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv10 forwardSecrecyRequired:false];
}

- (void)testIncludesSubdomains
{
    NSDictionary *exampleATS = @{@"NSExceptionDomains" : @{
                                     @"example.com" : @{
                                         @"NSExceptionAllowsInsecureHTTPLoads" : @YES,
                                         @"NSIncludesSubdomains" : @YES,
                                     },
                                     @"subhostname.example.com" : @{
                                         @"NSExceptionAllowsInsecureHTTPLoads" : @NO,
                                         @"NSIncludesSubdomains" : @NO,
                                         @"NSExceptionMinimumTLSVersion" : @"TLSv1.3",
                                     },
                                     @"another.subhostname.example.com" : @{
                                         @"NSExceptionAllowsInsecureHTTPLoads" : @NO,
                                         @"NSExceptionMinimumTLSVersion" : @"TLSv1.1",
                                     },
                                 }};

    sec_protocol_configuration_builder_t builder = sec_protocol_configuration_builder_create((__bridge CFDictionaryRef) exampleATS, false);
    sec_protocol_configuration_t configuration = sec_protocol_configuration_create_with_builder(builder);
    XCTAssertTrue(configuration != nil, @"failed to build configuration");
    if (!configuration) {
        return;
    }
    [self runATSExceptionTestForHostname:"example.com" configuration:configuration isAddress:false isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"subhostname.example.com" configuration:configuration isAddress:false isDirect:false atsRequired:true minTLSVersion:tls_protocol_version_TLSv13 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"another.subhostname.example.com" configuration:configuration isAddress:false isDirect:false atsRequired:true minTLSVersion:tls_protocol_version_TLSv11 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"test.another.subhostname.example.com" configuration:configuration isAddress:false isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
}

- (void)testCIDRExceptions
{
    NSDictionary *atsState = @{@"NSExceptionDomains" : @{
        @"example.com" : @{ @"NSExceptionAllowsInsecureHTTPLoads" : @YES },
        @"198.51.100.0/24" : @{ @"NSExceptionAllowsInsecureHTTPLoads" : @YES },
        @"198.51.100.0/27" : @{ @"NSExceptionAllowsInsecureHTTPLoads" : @NO },
        @"198.51.100.1" : @{ @"NSExceptionAllowsInsecureHTTPLoads" : @YES },
        @"198.51.100.101" : @{ @"NSExceptionAllowsInsecureHTTPLoads" : @NO },
        @"2001:db8::/32" : @{ @"NSExceptionAllowsInsecureHTTPLoads" : @YES },
        @"2001:db8::/64" : @{ @"NSExceptionAllowsInsecureHTTPLoads" : @NO },
    }};

    sec_protocol_configuration_builder_t builder = sec_protocol_configuration_builder_create((__bridge CFDictionaryRef)atsState, false);
        sec_protocol_configuration_t configuration = sec_protocol_configuration_create_with_builder(builder);
    XCTAssertTrue(configuration != nil, @"failed to build configuration");
    if (!configuration) {
        return;
    }
    [self runATSExceptionTestForHostname:"localhost" configuration:configuration isAddress:false isDirect:true atsRequired:false minTLSVersion:0 forwardSecrecyRequired:false];
    [self runATSExceptionTestForHostname:"myhost.local" configuration:configuration isAddress:false isDirect:true atsRequired:false minTLSVersion:0 forwardSecrecyRequired:false];
    [self runATSExceptionTestForHostname:"127.0.0.1" configuration:configuration isAddress:true isDirect:true atsRequired:false minTLSVersion:0 forwardSecrecyRequired:false];
    [self runATSExceptionTestForHostname:"::1" configuration:configuration isAddress:true isDirect:true atsRequired:false minTLSVersion:0 forwardSecrecyRequired:false];
    [self runATSExceptionTestForHostname:"example.com" configuration:configuration isAddress:false isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"google.com" configuration:configuration isAddress:false isDirect:false atsRequired:true minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];

    // External, allowed by exception
    [self runATSExceptionTestForHostname:"198.51.100.1" configuration:configuration isAddress:true isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"198.51.100.100" configuration:configuration isAddress:true isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"198.51.100.255" configuration:configuration isAddress:true isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"2001:db8:85a3::8a2e:370:7334" configuration:configuration isAddress:true isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];

    // External, no exception
    [self runATSExceptionTestForHostname:"1.1.1.1" configuration:configuration isAddress:true isDirect:false atsRequired:true minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"198.51.100.0" configuration:configuration isAddress:true isDirect:false atsRequired:true minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"198.51.100.101" configuration:configuration isAddress:true isDirect:false atsRequired:true minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"198.51.101.0" configuration:configuration isAddress:true isDirect:false atsRequired:true minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"203.0.113.0" configuration:configuration isAddress:true isDirect:false atsRequired:true minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"255.255.255.0" configuration:configuration isAddress:true isDirect:false atsRequired:true minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"fe00::" configuration:configuration isAddress:true isDirect:false atsRequired:true minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"2001:db8::8a2e:370:7334" configuration:configuration isAddress:true isDirect:false atsRequired:true minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"2001:db80:0000:0000:0000:0000:0000:0000" configuration:configuration isAddress:true isDirect:false atsRequired:true minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];

    atsState = @{@"NSExceptionDomains" : @{
        @"::/0" : @{ @"NSExceptionAllowsInsecureHTTPLoads" : @YES },
        @"::/1" : @{ @"NSExceptionAllowsInsecureHTTPLoads" : @NO },
        @"0.0.0.0/0" : @{ @"NSExceptionAllowsInsecureHTTPLoads" : @YES },
        @"0.0.0.0/1" : @{ @"NSExceptionAllowsInsecureHTTPLoads" : @NO },
    }};

    builder = sec_protocol_configuration_builder_create((__bridge CFDictionaryRef)atsState, false);
    configuration = sec_protocol_configuration_create_with_builder(builder);
    XCTAssertTrue(configuration != nil, @"failed to build configuration");
    if (!configuration) {
        return;
    }
    [self runATSExceptionTestForHostname:"localhost" configuration:configuration isAddress:false isDirect:true atsRequired:false minTLSVersion:0 forwardSecrecyRequired:false];
    [self runATSExceptionTestForHostname:"myhost.local" configuration:configuration isAddress:false isDirect:true atsRequired:false minTLSVersion:0 forwardSecrecyRequired:false];
    [self runATSExceptionTestForHostname:"example.com" configuration:configuration isAddress:false isDirect:false atsRequired:true minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];

    // Allowed by exception
    [self runATSExceptionTestForHostname:"128.0.0.0" configuration:configuration isAddress:true isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"198.51.100.1" configuration:configuration isAddress:true isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"198.51.100.100" configuration:configuration isAddress:true isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"198.51.100.255" configuration:configuration isAddress:true isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"198.51.100.0" configuration:configuration isAddress:true isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"198.51.100.101" configuration:configuration isAddress:true isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"198.51.101.0" configuration:configuration isAddress:true isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"203.0.113.0" configuration:configuration isAddress:true isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"255.255.255.0" configuration:configuration isAddress:true isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"fe00::" configuration:configuration isAddress:true isDirect:false atsRequired:false minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];

    // Blocked by exception
    [self runATSExceptionTestForHostname:"127.0.0.1" configuration:configuration isAddress:true isDirect:true atsRequired:true minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"::1" configuration:configuration isAddress:true isDirect:true atsRequired:true minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"1.1.1.1" configuration:configuration isAddress:true isDirect:false atsRequired:true minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"127.255.255.255" configuration:configuration isAddress:true isDirect:false atsRequired:true minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"2001:db8::8a2e:370:7334" configuration:configuration isAddress:true isDirect:false atsRequired:true minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"2001:db8:85a3::8a2e:370:7334" configuration:configuration isAddress:true isDirect:false atsRequired:true minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
}

- (void)testMinTLSVersionDisallowedStrings
{
    NSDictionary *exampleATS = @{@"NSExceptionDomains" : @{
                                     @"example.com" : @{
                                         @"NSExceptionMinimumTLSVersion" : @"TLSv1.4",
                                     },
                                     @"google.com" : @{
                                         @"NSExceptionMinimumTLSVersion" : @"TLSV1.3",
                                     },
                                     @"neverssl.com" : @{
                                         @"NSExceptionMinimumTLSVersion" : @"TLSv1.00",
                                     },
                                 }};

    sec_protocol_configuration_builder_t builder = sec_protocol_configuration_builder_create((__bridge CFDictionaryRef) exampleATS, false);
    sec_protocol_configuration_t configuration = sec_protocol_configuration_create_with_builder(builder);
    XCTAssertTrue(configuration != nil, @"failed to build configuration");
    if (!configuration) {
        return;
    }
    [self runATSExceptionTestForHostname:"example.com" configuration:configuration isAddress:false isDirect:false atsRequired:true minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"google.com" configuration:configuration isAddress:false isDirect:false atsRequired:true minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
    [self runATSExceptionTestForHostname:"neverssl.com" configuration:configuration isAddress:false isDirect:false atsRequired:true minTLSVersion:tls_protocol_version_TLSv12 forwardSecrecyRequired:true];
}

#pragma mark - Global Keys Tests

- (void)runGlobalKeysTestForHostname:(const char *)hostname
                       configuration:(sec_protocol_configuration_t)configuration
                           isAddress:(bool)isAddress
                            isDirect:(bool)isDirect
                         atsRequired:(bool)atsRequired
{
    XCTAssertTrue(atsRequired == sec_protocol_configuration_tls_required_for_host(configuration, hostname, isDirect), "ATS should have been required: %d for hostname: %s", atsRequired, hostname);
    sec_protocol_options_t options = [self create_sec_protocol_options];
    sec_protocol_options_set_sec_protocol_configuration(options, configuration);
    sec_protocol_options_t transformed = nil;
    if (isAddress) {
        transformed = sec_protocol_configuration_copy_transformed_options_for_address(options, hostname, isDirect);
    } else {
        transformed = sec_protocol_configuration_copy_transformed_options_for_host(options, hostname, isDirect);
    }
    (void)sec_protocol_options_access_handle(transformed, ^bool(void *handle) {
        sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
        SEC_PROTOCOL_METADATA_VALIDATE(content, false);

        if (atsRequired) {
            XCTAssertTrue(content->ats_required == true, "ATS should have been required for hostname: %s", hostname);
            XCTAssertTrue(content->min_version == tls_protocol_version_TLSv12, "Minimum TLS version should have been 1.2 for hostname: %s", hostname);
            XCTAssertTrue(content->ciphersuites != nil, "Forward secrecy should have been required for hostname: %s", hostname);
        } else {
            XCTAssertTrue(content->ats_required == false, "ATS should not have been required for hostname: %s", hostname);
            XCTAssertTrue(content->min_version == 0, "Minimum TLS version should not have been set for hostname: %s", hostname);
            XCTAssertTrue(content->ciphersuites == nil, "Forward secrecy should not have been required for hostname: %s", hostname);
        }
        return true;
    });
}

- (void)testLocalNetworkingAllowedByDefault
{
    NSDictionary *emptyATS = @{};

    sec_protocol_configuration_builder_t builder = sec_protocol_configuration_builder_create((__bridge CFDictionaryRef)emptyATS, false);
        sec_protocol_configuration_t configuration = sec_protocol_configuration_create_with_builder(builder);
    XCTAssertTrue(configuration != nil, @"failed to build configuration");
    if (!configuration) {
        return;
    }
    [self runGlobalKeysTestForHostname:"localhost" configuration:configuration isAddress:false isDirect:true atsRequired:false];
    [self runGlobalKeysTestForHostname:"myhost.local" configuration:configuration isAddress:false isDirect:true atsRequired:false];
    [self runGlobalKeysTestForHostname:"example.com" configuration:configuration isAddress:false isDirect:false atsRequired:true];
    [self runGlobalKeysTestForHostname:"127.0.0.1" configuration:configuration isAddress:true isDirect:true atsRequired:false];
    [self runGlobalKeysTestForHostname:"203.0.113.1" configuration:configuration isAddress:true isDirect:false atsRequired:true];
    [self runGlobalKeysTestForHostname:"::1" configuration:configuration isAddress:true isDirect:true atsRequired:false];
    [self runGlobalKeysTestForHostname:"fe80::1" configuration:configuration isAddress:true isDirect:true atsRequired:false];
    [self runGlobalKeysTestForHostname:"fe00::1" configuration:configuration isAddress:true isDirect:false atsRequired:true];
    [self runGlobalKeysTestForHostname:"2001:db8::8a2e:370:7334" configuration:configuration isAddress:true isDirect:false atsRequired:true];
}

- (void)testLocalNetworkingDisabled
{
    NSDictionary *atsState = @{@"NSAllowsLocalNetworking" : @NO};

    sec_protocol_configuration_builder_t builder = sec_protocol_configuration_builder_create((__bridge CFDictionaryRef)atsState, false);
        sec_protocol_configuration_t configuration = sec_protocol_configuration_create_with_builder(builder);
    XCTAssertTrue(configuration != nil, @"failed to build configuration");
    if (!configuration) {
        return;
    }
    [self runGlobalKeysTestForHostname:"localhost" configuration:configuration isAddress:false isDirect:true atsRequired:true];
    [self runGlobalKeysTestForHostname:"myhost.local" configuration:configuration isAddress:false isDirect:true atsRequired:true];
    [self runGlobalKeysTestForHostname:"example.com" configuration:configuration isAddress:false isDirect:false atsRequired:true];
    [self runGlobalKeysTestForHostname:"127.0.0.1" configuration:configuration isAddress:true isDirect:true atsRequired:true];
    [self runGlobalKeysTestForHostname:"203.0.113.1" configuration:configuration isAddress:true isDirect:false atsRequired:true];
    [self runGlobalKeysTestForHostname:"::1" configuration:configuration isAddress:true isDirect:true atsRequired:true];
    [self runGlobalKeysTestForHostname:"fe80::1" configuration:configuration isAddress:true isDirect:true atsRequired:true];
    [self runGlobalKeysTestForHostname:"fe00::1" configuration:configuration isAddress:true isDirect:false atsRequired:true];
    [self runGlobalKeysTestForHostname:"2001:db8::8a2e:370:7334" configuration:configuration isAddress:true isDirect:false atsRequired:true];
}

- (void)testAllowsArbitraryLoads
{
    NSDictionary *atsState = @{@"NSAllowsArbitraryLoads" : @YES};

    sec_protocol_configuration_builder_t builder = sec_protocol_configuration_builder_create((__bridge CFDictionaryRef)atsState, false);
        sec_protocol_configuration_t configuration = sec_protocol_configuration_create_with_builder(builder);
    XCTAssertTrue(configuration != nil, @"failed to build configuration");
    if (!configuration) {
        return;
    }
    [self runGlobalKeysTestForHostname:"localhost" configuration:configuration isAddress:false isDirect:true atsRequired:false];
    [self runGlobalKeysTestForHostname:"myhost.local" configuration:configuration isAddress:false isDirect:true atsRequired:false];
    [self runGlobalKeysTestForHostname:"example.com" configuration:configuration isAddress:false isDirect:false atsRequired:false];
    [self runGlobalKeysTestForHostname:"127.0.0.1" configuration:configuration isAddress:true isDirect:true atsRequired:false];
    [self runGlobalKeysTestForHostname:"203.0.113.1" configuration:configuration isAddress:true isDirect:false atsRequired:false];
    [self runGlobalKeysTestForHostname:"::1" configuration:configuration isAddress:true isDirect:true atsRequired:false];
    [self runGlobalKeysTestForHostname:"fe80::1" configuration:configuration isAddress:true isDirect:true atsRequired:false];
    [self runGlobalKeysTestForHostname:"fe00::1" configuration:configuration isAddress:true isDirect:false atsRequired:false];
    [self runGlobalKeysTestForHostname:"2001:db8::8a2e:370:7334" configuration:configuration isAddress:true isDirect:false atsRequired:false];
}

- (void)testAppleBundleExceptionWithNoATSKey
{
    sec_protocol_configuration_builder_t builder = sec_protocol_configuration_builder_create(nil, true);
    sec_protocol_configuration_t configuration = sec_protocol_configuration_create_with_builder(builder);
    XCTAssertTrue(configuration != nil, @"failed to build configuration");
    if (!configuration) {
        return;
    }
    [self runGlobalKeysTestForHostname:"localhost" configuration:configuration isAddress:false isDirect:true atsRequired:false];
    [self runGlobalKeysTestForHostname:"myhost.local" configuration:configuration isAddress:false isDirect:true atsRequired:false];
    [self runGlobalKeysTestForHostname:"example.com" configuration:configuration isAddress:false isDirect:false atsRequired:false];
    [self runGlobalKeysTestForHostname:"127.0.0.1" configuration:configuration isAddress:true isDirect:true atsRequired:false];
    [self runGlobalKeysTestForHostname:"203.0.113.1" configuration:configuration isAddress:true isDirect:false atsRequired:false];
    [self runGlobalKeysTestForHostname:"::1" configuration:configuration isAddress:true isDirect:true atsRequired:false];
    [self runGlobalKeysTestForHostname:"fe80::1" configuration:configuration isAddress:true isDirect:true atsRequired:false];
    [self runGlobalKeysTestForHostname:"fe00::1" configuration:configuration isAddress:true isDirect:false atsRequired:false];
    [self runGlobalKeysTestForHostname:"2001:db8::8a2e:370:7334" configuration:configuration isAddress:true isDirect:false atsRequired:false];
}

- (void)runAllowsArbitraryLoadsOverrideTestWithATSDictionary:(NSDictionary *)atsState
{
    sec_protocol_configuration_builder_t builder = sec_protocol_configuration_builder_create((__bridge CFDictionaryRef)atsState, false);
        sec_protocol_configuration_t configuration = sec_protocol_configuration_create_with_builder(builder);
    XCTAssertTrue(configuration != nil, @"failed to build configuration");
    if (!configuration) {
        return;
    }
    [self runGlobalKeysTestForHostname:"localhost" configuration:configuration isAddress:false isDirect:true atsRequired:false];
    [self runGlobalKeysTestForHostname:"myhost.local" configuration:configuration isAddress:false isDirect:true atsRequired:false];
    [self runGlobalKeysTestForHostname:"example.com" configuration:configuration isAddress:false isDirect:false atsRequired:true];
    [self runGlobalKeysTestForHostname:"google.com" configuration:configuration isAddress:false isDirect:false atsRequired:true];
    [self runGlobalKeysTestForHostname:"127.0.0.1" configuration:configuration isAddress:true isDirect:true atsRequired:false];
    [self runGlobalKeysTestForHostname:"203.0.113.1" configuration:configuration isAddress:true isDirect:false atsRequired:true];
    [self runGlobalKeysTestForHostname:"::1" configuration:configuration isAddress:true isDirect:true atsRequired:false];
    [self runGlobalKeysTestForHostname:"fe80::1" configuration:configuration isAddress:true isDirect:true atsRequired:false];
    [self runGlobalKeysTestForHostname:"fe00::1" configuration:configuration isAddress:true isDirect:false atsRequired:true];
    [self runGlobalKeysTestForHostname:"2001:db8::8a2e:370:7334" configuration:configuration isAddress:true isDirect:false atsRequired:true];
}

- (void)testLocalNetworkingKeyOverridesAllowsArbitraryLoads
{
    NSDictionary *atsState = @{@"NSAllowsArbitraryLoads" : @YES,
                               @"NSAllowsLocalNetworking" : @YES};

    [self runAllowsArbitraryLoadsOverrideTestWithATSDictionary:atsState];
}

- (void)testFalseWebContentKeyOverridesAllowsArbitraryLoads
{
    NSDictionary *atsState = @{@"NSAllowsArbitraryLoads" : @YES,
                               @"NSAllowsArbitraryLoadsInWebContent" : @NO};

    [self runAllowsArbitraryLoadsOverrideTestWithATSDictionary:atsState];
}

- (void)testTrueWebContentKeyOverridesAllowsArbitraryLoads
{
    NSDictionary *atsState = @{@"NSAllowsArbitraryLoads" : @YES,
                               @"NSAllowsArbitraryLoadsInWebContent" : @YES};

    [self runAllowsArbitraryLoadsOverrideTestWithATSDictionary:atsState];
}

- (void)testFalseMediaKeyOverridesAllowsArbitraryLoads
{
    NSDictionary *atsState = @{@"NSAllowsArbitraryLoads" : @YES,
                               @"NSAllowsArbitraryLoadsForMedia" : @NO};

    [self runAllowsArbitraryLoadsOverrideTestWithATSDictionary:atsState];
}

- (void)testTrueMediaKeyOverridesAllowsArbitraryLoads
{
    NSDictionary *atsState = @{@"NSAllowsArbitraryLoads" : @YES,
                               @"NSAllowsArbitraryLoadsForMedia" : @YES};

    [self runAllowsArbitraryLoadsOverrideTestWithATSDictionary:atsState];
}

- (void)testGarbageValueDisablesAllowsArbitraryLoads
{
    NSDictionary *atsState = @{@"NSAllowsArbitraryLoads" : @2};
    [self runAllowsArbitraryLoadsOverrideTestWithATSDictionary:atsState];

    atsState = @{@"NSAllowsArbitraryLoads" : @"YES"};
    [self runAllowsArbitraryLoadsOverrideTestWithATSDictionary:atsState];

    atsState = @{@"NSAllowsArbitraryLoads" : @NO};
    [self runAllowsArbitraryLoadsOverrideTestWithATSDictionary:atsState];
}

- (void)testExampleFile:(NSURL *)path
{
    NSData *exampleData = [[NSData alloc] initWithContentsOfURL:path];
    NSDictionary *exampleATS = [NSJSONSerialization JSONObjectWithData:exampleData options:kNilOptions error:nil];
    XCTAssertNotNil(exampleATS, @"Loading %@ failed", path);
    if (!exampleATS) {
        return;
    }

    sec_protocol_configuration_builder_t builder = sec_protocol_configuration_builder_create((__bridge CFDictionaryRef)exampleATS, true);
    sec_protocol_configuration_t configuration = sec_protocol_configuration_create_with_builder(builder);
    XCTAssertTrue(configuration != nil, @"failed to build configuration");
    if (!configuration) {
        return;
    }

    __block bool allows_local_networking = true;
    [exampleATS enumerateKeysAndObjectsUsingBlock:^(id _key, id _obj, BOOL *stop) {
        NSString *key = (NSString *)_key;
        if ([key isEqualToString:@"NSAllowsLocalNetworking"]) {
            NSNumber *value = (NSNumber *)_obj;
            if (value) {
                allows_local_networking = [value boolValue];
            }
        }
    }];

    [exampleATS enumerateKeysAndObjectsUsingBlock:^(id _key, id _obj, BOOL *stop) {
        NSString *key = (NSString *)_key;
        if ([key isEqualToString:@"NSExceptionDomains"]) {
            NSDictionary *domain_map = (NSDictionary *)_obj;
            [domain_map enumerateKeysAndObjectsUsingBlock:^(id _domain, id _domain_entry, BOOL *_domain_stop) {
                NSString *domain = (NSString *)_domain;
                NSDictionary *entry = (NSDictionary *)_domain_entry;

#define BOOLEAN_FOR_KEY(key, value, default) \
    bool value = default; \
    { \
        NSNumber *nsValue = [entry valueForKey:key]; \
        if (nsValue) { \
            value = [nsValue boolValue]; \
        } \
    }
#define STRING_FOR_KEY(key, value, default) \
    NSString *value = default; \
    { \
        NSString *nsValue = [entry valueForKey:key]; \
        if (nsValue) { \
            value = nsValue; \
        } \
    }
                BOOLEAN_FOR_KEY(@"NSExceptionAllowsInsecureHTTPLoads", allows_http, false);
                BOOLEAN_FOR_KEY(@"NSIncludesSubdomains", includes_subdomains, false);
                BOOLEAN_FOR_KEY(@"NSExceptionRequiresForwardSecrecy", requires_pfs, false);
                STRING_FOR_KEY(@"NSExceptionMinimumTLSVersion", minimum_tls, @"TLSv1.2");
#undef STRING_FOR_KEY
#undef BOOLEAN_FOR_KEY

                tls_protocol_version_t minimum_protocol_version = (sec_protocol_configuration_protocol_string_to_version([minimum_tls cStringUsingEncoding:NSUTF8StringEncoding]));

                bool is_direct = isLocalTLD(domain);
                sec_protocol_options_t options = [self create_sec_protocol_options];
                sec_protocol_options_set_sec_protocol_configuration(options, configuration);
                sec_protocol_options_t transformed = sec_protocol_configuration_copy_transformed_options_for_host(options, [domain cStringUsingEncoding:NSUTF8StringEncoding], is_direct);
                (void)sec_protocol_options_access_handle(transformed, ^bool(void *handle) {
                    sec_protocol_options_content_t content = (sec_protocol_options_content_t)handle;
                    SEC_PROTOCOL_METADATA_VALIDATE(content, false);

                    XCTAssertTrue(content->ats_required == !allows_http);
                    XCTAssertTrue(content->min_version == minimum_protocol_version);
                    if (requires_pfs || content->ats_required) {
                        XCTAssertTrue(content->ciphersuites != nil);
                    } else {
                        XCTAssertTrue(content->ciphersuites == nil);
                    }
                    return true;
                });

                bool tls_required = sec_protocol_configuration_tls_required_for_host(configuration, [domain cStringUsingEncoding:NSUTF8StringEncoding], is_direct);

                // If an exception is present for this domain, we require TLS if the NSExceptionAllowsInsecureHTTPLoads flag is not set to true.
                // This overrides the default local networking behavior
                XCTAssertTrue(allows_http != tls_required);
            }];
        }
    }];
}

- (void)testExampleATSDictionaries {
    NSArray <NSURL *>* testFiles = [[NSBundle bundleForClass:[self class]]URLsForResourcesWithExtension:@".json" subdirectory:@"."];
    [testFiles enumerateObjectsUsingBlock:^(NSURL*  _Nonnull path, __unused NSUInteger idx, BOOL * _Nonnull stop) {
        [self testExampleFile:path];
    }];
}

- (void)test_sec_protocol_configuration_protocol_string_to_version {
    tls_protocol_version_t tls_protocol_version_unknown = 0;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    struct {
        const char *protocol_string;
        tls_protocol_version_t expected_version;
    } testCases[] = {
        {
            .protocol_string = NULL,
            .expected_version = tls_protocol_version_unknown,
        },
        {
            .protocol_string = "",
            .expected_version = tls_protocol_version_unknown,
        },
        {
            .protocol_string = "A",
            .expected_version = tls_protocol_version_unknown,
        },
        {
            .protocol_string = "SSLv3.0",
            .expected_version = tls_protocol_version_unknown,
        },
        {
            .protocol_string = "TLSv1.0",
            .expected_version = tls_protocol_version_TLSv10,
        },
        {
            .protocol_string = "TLSv1.1",
            .expected_version = tls_protocol_version_TLSv11,
        },
        {
            .protocol_string = "TLSv1.2",
            .expected_version = tls_protocol_version_TLSv12,
        },
        {
            .protocol_string = "TLSv1.3",
            .expected_version = tls_protocol_version_TLSv13,
        },
        {
            .protocol_string = "TLSv1.31",
            .expected_version = tls_protocol_version_unknown,
        },
        {
            .protocol_string = "TLSv1.4",
            .expected_version = tls_protocol_version_unknown,
        },
        {
            .protocol_string = "DTLSv1.0",
            .expected_version = tls_protocol_version_DTLSv10,
        },
        {
            .protocol_string = "DTLSv1.2",
            .expected_version = tls_protocol_version_DTLSv12,
        },
        {
            .protocol_string = "DTLSv1.3",
            .expected_version = tls_protocol_version_unknown,
        },
    };
#pragma clang diagnostic pop

    for (size_t i = 0; i < sizeof(testCases)/sizeof(testCases[0]); i++) {
        tls_protocol_version_t version = sec_protocol_configuration_protocol_string_to_version(testCases[i].protocol_string);
        XCTAssertTrue(version == testCases[i].expected_version, "Test scenario %zu \"%s\" has failed.", i, testCases[i].protocol_string);
    }
}

@end

#pragma clang diagnostic pop
