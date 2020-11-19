//
//  SecProtocolConfiguration.m
//  Security
//

#import "SecProtocolInternal.h"
#import <Security/SecProtocolObject.h>
#import <Security/SecProtocolConfiguration.h>
#import <Security/SecureTransportPriv.h>
#import <CoreFoundation/CFPriv.h>
#import <Foundation/Foundation.h>

#define MINIMUM_RSA_KEY_SIZE 2048
#define MINIMUM_ECDSA_KEY_SIZE 256
#define MINIMUM_HASH_ALGORITHM kSecSignatureHashAlgorithmSHA256
#define MINIMUM_PROTOCOL kTLSProtocol12

static const char *
get_running_process()
{
    static const char *processName = NULL;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        const char **procName = _CFGetProgname();
        processName = *procName;
    });
    return processName;
}

static bool
process_matches_target(const char *target_process)
{
    if (target_process == NULL) {
        return false;
    }

    const char *process = get_running_process();
    if (process != NULL) {
        return (strlen(target_process) == strlen(process) &&
                strncmp(process, target_process, strlen(target_process)) == 0);
    }
    return false;
}

static bool
client_is_WebKit()
{
    static bool is_WebKit = false;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        is_WebKit = process_matches_target("com.apple.WebKit");
    });
    return is_WebKit;
}

static bool
client_is_mediaserverd()
{
    static bool is_mediaserverd = false;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        is_mediaserverd = process_matches_target("mediaserverd");
    });
    return is_mediaserverd;
}

sec_protocol_configuration_t
sec_protocol_configuration_copy_singleton(void)
{
    static dispatch_once_t onceToken;
    static sec_protocol_configuration_t singleton = nil;
    dispatch_once(&onceToken, ^{
        singleton = sec_protocol_configuration_create_with_builder(sec_protocol_configuration_builder_copy_default());
    });
    return singleton;
}

static sec_protocol_options_t
sec_protocol_configuration_copy_transformed_options_with_ats_minimums(sec_protocol_options_t options)
{
    sec_protocol_options_set_ats_required(options, true);
    sec_protocol_options_set_trusted_peer_certificate(options, true);
    sec_protocol_options_set_minimum_rsa_key_size(options, MINIMUM_RSA_KEY_SIZE);
    sec_protocol_options_set_minimum_ecdsa_key_size(options, MINIMUM_ECDSA_KEY_SIZE);
    sec_protocol_options_set_minimum_signature_algorithm(options, MINIMUM_HASH_ALGORITHM);
    sec_protocol_options_set_min_tls_protocol_version(options, tls_protocol_version_TLSv12);
    return options;
}

sec_protocol_options_t
sec_protocol_configuration_copy_transformed_options(__unused sec_protocol_configuration_t config, sec_protocol_options_t options)
{
    sec_protocol_options_clear_tls_ciphersuites(options);
    sec_protocol_options_append_tls_ciphersuite_group(options, tls_ciphersuite_group_ats);
    return sec_protocol_configuration_copy_transformed_options_with_ats_minimums(options);
}

static const char *
_find_parent_domain(const char *domain)
{
    size_t domain_len = strlen(domain);
    size_t index = 0;
    while (index < domain_len) {
        // Once we hit a dot, the parent domain begins at the next segment.
        if (domain[index] == '.' && index < domain_len) {
            return domain + 1;
        }

        // Skip over all characters that are not dots.
        index++;
    }

    return NULL;
}

static sec_protocol_options_t
sec_protocol_configuration_copy_transformed_options_for_host_internal(sec_protocol_configuration_t config, sec_protocol_options_t options,
                                                                const char *host, bool parent_domain)
{
    xpc_object_t map = sec_protocol_configuration_get_map(config);
    if (map == nil) {
        return options;
    }

    xpc_object_t domain_map = xpc_dictionary_get_dictionary(map, kExceptionDomains);
    if (domain_map == nil) {
        return options;
    }

    xpc_object_t entry = xpc_dictionary_get_dictionary(domain_map, host);
    if (entry == nil) {
        const char *parent_host = _find_parent_domain(host);
        if (parent_host != NULL) {
            return sec_protocol_configuration_copy_transformed_options_for_host_internal(config, options, parent_host, true);
        }

        // If we could not find a matching domain, apply the default connection properties.
        return sec_protocol_configuration_copy_transformed_options(config, options);
    }

    bool pfs_required = xpc_dictionary_get_bool(entry, kExceptionRequiresForwardSecrecy);
    if (pfs_required) {
        sec_protocol_options_clear_tls_ciphersuites(options);
        sec_protocol_options_append_tls_ciphersuite_group(options, tls_ciphersuite_group_ats);
    } else {
        // Otherwise, record the fact that non-PFS ciphersuites are permitted. 
        // This does not mean that the caller actually configured a non-PFS ciphersuite.
        sec_protocol_options_set_ats_non_pfs_ciphersuite_allowed(options, true);
    }

    tls_protocol_version_t minimum_protocol = (SSLProtocol)xpc_dictionary_get_int64(entry, kExceptionMinimumTLSVersion);
    if (minimum_protocol != 0) {
        // Record the fact that an excepted TLS version was configured.
        sec_protocol_options_set_min_tls_protocol_version(options, minimum_protocol);
        sec_protocol_options_set_ats_minimum_tls_version_allowed(options, true);
    }

    return options;
}

sec_protocol_options_t
sec_protocol_configuration_copy_transformed_options_for_host(sec_protocol_configuration_t config, sec_protocol_options_t options, const char *host)
{
    return sec_protocol_configuration_copy_transformed_options_for_host_internal(config, sec_protocol_configuration_copy_transformed_options_with_ats_minimums(options), host, false);
}

bool
sec_protocol_configuration_tls_required(sec_protocol_configuration_t config)
{
    xpc_object_t map = sec_protocol_configuration_get_map(config);
    if (map == nil) {
        // Fail closed.
        return true;
    }

    bool allows_media_loads = xpc_dictionary_get_bool(map, kAllowsArbitraryLoadsForMedia);
    if (allows_media_loads && client_is_mediaserverd()) {
        return false;
    }

    bool allows_web_loads = xpc_dictionary_get_bool(map, kAllowsArbitraryLoadsInWebContent);
    if (allows_web_loads && client_is_WebKit()) {
        return false;
    }

    return !xpc_dictionary_get_bool(map, kAllowsArbitraryLoads);
}

static bool
sec_protocol_configuration_tls_required_for_host_internal(sec_protocol_configuration_t config, const char *host, bool parent_domain, bool is_direct)
{
    xpc_object_t map = sec_protocol_configuration_get_map(config);
    if (map == nil) {
        // Fail closed.
        return true;
    }

    if (is_direct && xpc_dictionary_get_bool(map, kAllowsLocalNetworking)) {
        // Local domains do not require TLS if the kAllowsLocalNetworking flag is set.
        return false;
    }

    xpc_object_t domain_map = xpc_dictionary_get_dictionary(map, kExceptionDomains);
    if (domain_map == nil) {
        // Absent per-domain exceptions, use the default.
        return sec_protocol_configuration_tls_required(config);
    }

    xpc_object_t entry = xpc_dictionary_get_dictionary(domain_map, host);
    if (entry == nil) {
        const char *parent_host = _find_parent_domain(host);
        if (parent_host != NULL) {
            return sec_protocol_configuration_tls_required_for_host_internal(config, parent_host, true, is_direct);
        }
        return sec_protocol_configuration_tls_required(config);
    }

    bool requires_tls = !xpc_dictionary_get_bool(entry, kExceptionAllowsInsecureHTTPLoads);
    bool includes_subdomains = !xpc_dictionary_get_bool(entry, kExceptionAllowsInsecureHTTPLoads);

    if (parent_domain && !includes_subdomains) {
        // If this domain's exceptions do not apply to subdomains, then default to the application default policy.
        return sec_protocol_configuration_tls_required(config);
    }

    return requires_tls;
}

bool
sec_protocol_configuration_tls_required_for_host(sec_protocol_configuration_t config, const char *host, bool is_direct)
{
    return sec_protocol_configuration_tls_required_for_host_internal(config, host, false, is_direct);
}

bool
sec_protocol_configuration_tls_required_for_address(sec_protocol_configuration_t config, const char *address)
{
    xpc_object_t map = sec_protocol_configuration_get_map(config);
    if (map == nil) {
        // Fail closed.
        return true;
    }

    return !xpc_dictionary_get_bool(map, kAllowsLocalNetworking);
}

static tls_protocol_version_t
sec_protocol_configuration_protocol_string_to_version(const char *protocol)
{
    if (protocol == NULL) {
        return 0;
    }

    const char *tlsv10 = "TLSv1.0";
    const char *tlsv11 = "TLSv1.1";
    const char *tlsv12 = "TLSv1.2";
    const char *tlsv13 = "TLSv1.3";

    if (strlen(protocol) == strlen(tlsv10) && strncmp(protocol, tlsv10, strlen(protocol)) == 0) {
        return tls_protocol_version_TLSv10;
    } else if (strlen(protocol) == strlen(tlsv11) && strncmp(protocol, tlsv11, strlen(protocol)) == 0) {
        return tls_protocol_version_TLSv11;
    } else if (strlen(protocol) == strlen(tlsv12) && strncmp(protocol, tlsv12, strlen(protocol)) == 0) {
        return tls_protocol_version_TLSv12;
    } else if (strlen(protocol) == strlen(tlsv13) && strncmp(protocol, tlsv13, strlen(protocol)) == 0) {
        return tls_protocol_version_TLSv13;
    }

    return 0;
}

static void
sec_protocol_configuration_register_builtin_exception(xpc_object_t dict, const char *name,
                                                      tls_protocol_version_t protocol, bool requires_pfs,
                                                      bool allows_http, bool includes_subdomains, bool require_ct)
{
    xpc_object_t domain_map = xpc_dictionary_get_dictionary(dict, kExceptionDomains);
    if (domain_map) {
        xpc_object_t entry = xpc_dictionary_create(NULL, NULL, 0);
        xpc_dictionary_set_value(entry, kExceptionDomains, domain_map);

        xpc_dictionary_set_bool(entry, kIncludesSubdomains, includes_subdomains);
        xpc_dictionary_set_int64(entry, kExceptionMinimumTLSVersion, protocol);
        xpc_dictionary_set_bool(entry, kExceptionAllowsInsecureHTTPLoads, allows_http);
        xpc_dictionary_set_bool(entry, kExceptionRequiresForwardSecrecy, requires_pfs);

        xpc_dictionary_set_value(domain_map, name, entry);
    }
}

void
sec_protocol_configuration_register_builtin_exceptions(sec_protocol_configuration_t config)
{
    xpc_object_t dict = sec_protocol_configuration_get_map(config);
    sec_protocol_configuration_register_builtin_exception(dict, "apple.com", tls_protocol_version_TLSv12, false, true, true, true);
    sec_protocol_configuration_register_builtin_exception(dict, "ls.apple.com", tls_protocol_version_TLSv10, false, true, true, true);
    sec_protocol_configuration_register_builtin_exception(dict, "gs.apple.com", tls_protocol_version_TLSv10, false, true, true, true);
    sec_protocol_configuration_register_builtin_exception(dict, "geo.apple.com", tls_protocol_version_TLSv10, false, true, true, true);
    sec_protocol_configuration_register_builtin_exception(dict, "is.autonavi.com", tls_protocol_version_TLSv10, false, true, true, true);
    sec_protocol_configuration_register_builtin_exception(dict, "apple-mapkit.com", tls_protocol_version_TLSv10, false, true, true, true);
    sec_protocol_configuration_register_builtin_exception(dict, "setup.icloud.com", tls_protocol_version_TLSv12, false, true, true, true);
}

void
sec_protocol_configuration_populate_insecure_defaults(sec_protocol_configuration_t config)
{
    xpc_object_t dict = sec_protocol_configuration_get_map(config);
    xpc_object_t domain_map = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_value(dict, kExceptionDomains, domain_map);

    xpc_dictionary_set_bool(dict, kAllowsArbitraryLoadsInWebContent, true);
    xpc_dictionary_set_bool(dict, kAllowsArbitraryLoadsForMedia, true);
    xpc_dictionary_set_bool(dict, kAllowsLocalNetworking, true);
    xpc_dictionary_set_bool(dict, kAllowsArbitraryLoads, true);
}

void
sec_protocol_configuration_populate_secure_defaults(sec_protocol_configuration_t config)
{
    xpc_object_t dict = sec_protocol_configuration_get_map(config);
    xpc_object_t domain_map = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_value(dict, kExceptionDomains, domain_map);

    xpc_dictionary_set_bool(dict, kAllowsArbitraryLoadsInWebContent, false);
    xpc_dictionary_set_bool(dict, kAllowsArbitraryLoadsForMedia, false);
    xpc_dictionary_set_bool(dict, kAllowsLocalNetworking, false);
    xpc_dictionary_set_bool(dict, kAllowsArbitraryLoads, false);
}

bool
sec_protocol_configuration_set_ats_overrides(sec_protocol_configuration_t config, CFDictionaryRef plist)
{
    if (plist == NULL) {
        return false;
    }

#define BOOLEAN_FOR_KEY(dictionary, key, value, default) \
    bool value = default; \
    { \
        if (dictionary[[[NSString alloc] initWithFormat:@"%s", key]]) { \
            NSNumber *nsValue = [dictionary valueForKey:[[NSString alloc] initWithFormat:@"%s", key]]; \
            if (nsValue) { \
                value = [nsValue boolValue]; \
            } \
        } \
    }
#define STRING_FOR_KEY(dictionary, key, value, default) \
    NSString *value = default; \
    { \
        if (dictionary[[[NSString alloc] initWithFormat:@"%s", key]]) { \
            NSString *nsValue = [dictionary valueForKey:[[NSString alloc] initWithFormat:@"%s", key]]; \
            if (nsValue) { \
                value = nsValue; \
            } \
        } \
    }

    xpc_object_t dict = sec_protocol_configuration_get_map(config);
    if (dict == nil) {
        return false;
    }

    NSDictionary *plist_dictionary = (__bridge NSDictionary *)plist;
    BOOLEAN_FOR_KEY(plist_dictionary, kAllowsArbitraryLoads, arbitrary_loads, false);
    BOOLEAN_FOR_KEY(plist_dictionary, kAllowsArbitraryLoadsInWebContent, web_loads, false);
    BOOLEAN_FOR_KEY(plist_dictionary, kAllowsArbitraryLoadsForMedia, media_loads, false);
    BOOLEAN_FOR_KEY(plist_dictionary, kAllowsLocalNetworking, local_networking, false);

    xpc_dictionary_set_bool(dict, kAllowsArbitraryLoads, arbitrary_loads);
    xpc_dictionary_set_bool(dict, kAllowsArbitraryLoadsInWebContent, web_loads);
    xpc_dictionary_set_bool(dict, kAllowsArbitraryLoadsForMedia, media_loads);
    xpc_dictionary_set_bool(dict, kAllowsLocalNetworking, local_networking);

    NSDictionary *exception_domains = [plist_dictionary valueForKey:[[NSString alloc] initWithFormat:@"%s", kExceptionDomains]];
    if (exception_domains == nil) {
        return true;
    }

    xpc_object_t domain_map = xpc_dictionary_get_dictionary(dict, kExceptionDomains);
    if (domain_map == nil) {
        // The domain map MUST be present during initialziation
        return false;
    }

    [exception_domains enumerateKeysAndObjectsUsingBlock:^(id _key, id _obj, BOOL *stop) {
        NSString *domain = (NSString *)_key;
        NSDictionary *entry = (NSDictionary *)_obj;
        if (entry == nil) {
            // Exception domains MUST have ATS information set.
            *stop = YES;
        }

        BOOLEAN_FOR_KEY(entry, kExceptionAllowsInsecureHTTPLoads, allows_http, false);
        BOOLEAN_FOR_KEY(entry, kIncludesSubdomains, includes_subdomains, false);
        BOOLEAN_FOR_KEY(entry, kExceptionRequiresForwardSecrecy, requires_pfs, false);
        STRING_FOR_KEY(entry, kExceptionMinimumTLSVersion, minimum_tls, @"TLSv1.2");

        xpc_object_t entry_map = xpc_dictionary_create(NULL, NULL, 0);
        xpc_dictionary_set_bool(entry_map, kIncludesSubdomains, includes_subdomains);
        xpc_dictionary_set_bool(entry_map, kExceptionAllowsInsecureHTTPLoads, allows_http);
        xpc_dictionary_set_bool(entry_map, kExceptionRequiresForwardSecrecy, requires_pfs);
        xpc_dictionary_set_int64(entry_map, kExceptionMinimumTLSVersion, sec_protocol_configuration_protocol_string_to_version([minimum_tls cStringUsingEncoding:NSUTF8StringEncoding]));
        xpc_dictionary_set_value(domain_map, [domain cStringUsingEncoding:NSUTF8StringEncoding], entry_map);
    }];

#undef STRING_FOR_KEY
#undef BOOLEAN_FOR_KEY

    return true;
}
