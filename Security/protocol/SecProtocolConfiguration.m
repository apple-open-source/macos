//
//  SecProtocolConfiguration.m
//  Security
//

#import "SecProtocolInternal.h"
#import <Security/SecProtocolObject.h>
#import <Security/SecProtocolConfiguration.h>
#import <Security/SecureTransportPriv.h>
#import <arpa/inet.h>
#import <CoreFoundation/CFPriv.h>
#import <Foundation/Foundation.h>
#import <os/log.h>

#define MINIMUM_RSA_KEY_SIZE 2048
#define MINIMUM_ECDSA_KEY_SIZE 256
#define MINIMUM_HASH_ALGORITHM kSecSignatureHashAlgorithmSHA256
#define MINIMUM_PROTOCOL kTLSProtocol12
#define TLS_V1_PREFIX "TLSv1."
#define MAXIMUM_V6_ADDRESS_LENGTH 39
#define MAXIMUM_V4_ADDRESS_LENGTH 15

static const char *
get_running_process(void)
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
        return strncmp(process, target_process, strlen(target_process)) == 0;
    }
    return false;
}

bool
client_is_WebKit(void)
{
    static bool is_WebKit = false;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        is_WebKit = process_matches_target("com.apple.WebKit");
    });
    return is_WebKit;
}

static bool
client_is_mediaserverd(void)
{
    static bool is_mediaplaybackd = false;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        is_mediaplaybackd = process_matches_target("mediaplaybackd");
    });
    return is_mediaplaybackd;
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
            return domain + index + 1;
        }

        // Skip over all characters that are not dots.
        index++;
    }

    return NULL;
}

static xpc_object_t
_find_cidr_exception(xpc_object_t cidr_exceptions_map, const union sockaddr_in_4_6 *address, uint8_t version)
{
    __block xpc_object_t exception = nullptr;
    __block uint8_t prefix = 0;
    xpc_dictionary_apply(cidr_exceptions_map, ^bool(const char * _Nonnull key, xpc_object_t  _Nonnull value) {
        size_t address_length;
        const void *address_data = xpc_dictionary_get_data(value, _kATSParsedCIDRAddressKey, &address_length);
        const union sockaddr_in_4_6 *exception_address = (const union sockaddr_in_4_6 *) address_data;
        if (exception_address && exception_address->sa.sa_family == version) {
            size_t mask_length;
            const void *mask_data = xpc_dictionary_get_data(value, _kATSParsedCIDRMaskKey, &mask_length);
            const union sockaddr_in_4_6 *mask = (const union sockaddr_in_4_6 *) mask_data;
            const uint8_t *dstp = NULL;
            const uint8_t *exceptionp = NULL;
            const uint8_t *maskp = NULL;
            int size = 0;

            if (version == AF_INET) {
                dstp = (const uint8_t*)&(address->sin.sin_addr);
                exceptionp = (const uint8_t*)&(exception_address->sin.sin_addr);
                maskp = (const uint8_t*)&(mask->sin.sin_addr);
                size = sizeof(address->sin.sin_addr);
            } else {
                dstp = (const uint8_t*)&(address->sin6.sin6_addr);
                exceptionp = (const uint8_t*)&(exception_address->sin6.sin6_addr);
                maskp = (const uint8_t*)&(mask->sin6.sin6_addr);
                size = sizeof(address->sin6.sin6_addr);
            }

            bool found_match = true;
            for (int i = 0; i < size; i++) {
                if ((dstp[i] & maskp[i]) != (exceptionp[i] & maskp[i])) {
                    found_match = false;
                    break;
                }
            }
            uint8_t found_prefix = xpc_dictionary_get_uint64(value, _kATSParsedCIDRPrefixKey);
            if (found_match && found_prefix >= prefix) {
                exception = value;
                prefix = found_prefix;
            }
        }
        return true;
    });
    return exception;
}

static xpc_object_t sec_protocol_configuration_find_exception_for_host(sec_protocol_configuration_t config, const char *host, bool parent_domain)
{
    xpc_object_t map = sec_protocol_configuration_get_map(config);
    if (map == nil) {
        return nil;
    }

    xpc_object_t domain_map = xpc_dictionary_get_dictionary(map, kExceptionDomains);
    if (domain_map == nil) {
        return nil;
    }

    xpc_object_t entry = xpc_dictionary_get_dictionary(domain_map, host);
    if (entry == nil || (parent_domain && !xpc_dictionary_get_bool(entry, kIncludesSubdomains))) {
        const char *parent_host = _find_parent_domain(host);
        if (parent_host != NULL) {
            return sec_protocol_configuration_find_exception_for_host(config, parent_host, true);
        }
        return nil;
    } else {
        return entry;
    }
}

static xpc_object_t sec_protocol_configuration_find_exception_for_address(sec_protocol_configuration_t config, const char *address)
{
    xpc_object_t map = sec_protocol_configuration_get_map(config);
    if (map == nil) {
        return nil;
    }

    xpc_object_t domain_map = xpc_dictionary_get_dictionary(map, kExceptionDomains);
    if (domain_map == nil) {
        return nil;
    }

    xpc_object_t entry = xpc_dictionary_get_dictionary(domain_map, address);
    if (entry != nil) {
        return entry;
    }

    xpc_object_t cidr_exceptions_map = xpc_dictionary_get_dictionary(map, _kCIDRExceptions);
    if (cidr_exceptions_map == nil) {
        return nil;
    }

    union sockaddr_in_4_6 sa = {};
    size_t len = strlen(address);
    if (len != 0) {
        if (len <= MAXIMUM_V4_ADDRESS_LENGTH && inet_pton(AF_INET, address, &(sa.sin.sin_addr))) {
            return _find_cidr_exception(cidr_exceptions_map, &sa, AF_INET);
        } else if (len <= MAXIMUM_V6_ADDRESS_LENGTH && inet_pton(AF_INET6, address, &(sa.sin6.sin6_addr))) {
            return _find_cidr_exception(cidr_exceptions_map, &sa, AF_INET6);
        }
    }

    return nil;
}

bool
sec_protocol_configuration_tls_required(sec_protocol_configuration_t config)
{
    xpc_object_t map = sec_protocol_configuration_get_map(config);
    if (map == nil) {
        // Fail closed.
        return true;
    }

    kATSGlobalKey allows_media_loads = (kATSGlobalKey) xpc_dictionary_get_uint64(map, kAllowsArbitraryLoadsForMedia);
    if (allows_media_loads == kATSGlobalKeyValueTrue && client_is_mediaserverd()) {
        return false;
    }

    kATSGlobalKey allows_web_loads = (kATSGlobalKey) xpc_dictionary_get_uint64(map, kAllowsArbitraryLoadsInWebContent);
    if (allows_web_loads == kATSGlobalKeyValueTrue && client_is_WebKit()) {
        return false;
    }

    kATSGlobalKey allows_local = (kATSGlobalKey) xpc_dictionary_get_uint64(map, kAllowsLocalNetworking);
    if (allows_local != kATSGlobalKeyNotPresent) {
        return true;
    }

    // We don't check NSAllowsArbitraryLoads if any of the other global keys are set
    return allows_web_loads != kATSGlobalKeyNotPresent ||
           allows_media_loads != kATSGlobalKeyNotPresent ||
           (kATSGlobalKey) xpc_dictionary_get_uint64(map, kAllowsArbitraryLoads) != kATSGlobalKeyValueTrue;
}

static bool
sec_protocol_configuration_tls_required_for_host_or_address_internal(sec_protocol_configuration_t config, const char *host, bool is_direct, xpc_object_t exception)
{
    xpc_object_t map = sec_protocol_configuration_get_map(config);
    if (map == nil) {
        // Fail closed.
        return true;
    }

    if (exception != nil) {
        return !xpc_dictionary_get_bool(exception, kExceptionAllowsInsecureHTTPLoads);
    }

    if (is_direct && ((kATSGlobalKey) xpc_dictionary_get_uint64(map, kAllowsLocalNetworking) != kATSGlobalKeyValueFalse)) {
        // Local domains do not require TLS by default or if the kAllowsLocalNetworking flag is set to true.
        return false;
    } else {
        // Absent per-domain exceptions, use the default.
        return sec_protocol_configuration_tls_required(config);
    }
}

bool
sec_protocol_configuration_tls_required_for_host(sec_protocol_configuration_t config, const char *host, bool is_direct)
{
    xpc_object_t exception = sec_protocol_configuration_find_exception_for_host(config, host, false);
    return sec_protocol_configuration_tls_required_for_host_or_address_internal(config, host, is_direct, exception);
}

bool
sec_protocol_configuration_tls_required_for_address(sec_protocol_configuration_t config, const char *address, bool is_direct)
{
    xpc_object_t exception = sec_protocol_configuration_find_exception_for_address(config, address);
    return sec_protocol_configuration_tls_required_for_host_or_address_internal(config, address, is_direct, exception);
}

static sec_protocol_options_t
sec_protocol_configuration_copy_transformed_options_for_host_or_address_internal(sec_protocol_configuration_t config,
                                                                                 sec_protocol_options_t options,
                                                                                 const char *host,
                                                                                 xpc_object_t exception)
{
    xpc_object_t map = sec_protocol_configuration_get_map(config);
    if (map == nil) {
        return options;
    }

    if (exception == nil) {
        // If we could not find a matching domain, apply the default connection properties.
        return sec_protocol_configuration_copy_transformed_options(config, options);
    }

    bool allows_insecure = xpc_dictionary_get_bool(exception, kExceptionAllowsInsecureHTTPLoads);
    if (allows_insecure) {
        // NSExceptionAllowsInsecureHTTPLoads loosens the server trust requirements for a given domain
        sec_protocol_options_set_ats_required(options, false);
    }

    bool pfs_required = xpc_dictionary_get_bool(exception, kExceptionRequiresForwardSecrecy);
    if (pfs_required) {
        sec_protocol_options_clear_tls_ciphersuites(options);
        sec_protocol_options_append_tls_ciphersuite_group(options, tls_ciphersuite_group_ats);
    } else {
        // Otherwise, record the fact that non-PFS ciphersuites are permitted.
        sec_protocol_options_set_ats_non_pfs_ciphersuite_allowed(options, true);
        if (!allows_insecure) {
            sec_protocol_options_clear_tls_ciphersuites(options);
            sec_protocol_options_append_tls_ciphersuite_group(options, tls_ciphersuite_group_ats_compatibility);
            sec_protocol_options_append_tls_ciphersuite_group(options, tls_ciphersuite_group_ats);
        }
    }

    tls_protocol_version_t minimum_protocol = (tls_protocol_version_t)xpc_dictionary_get_int64(exception, kExceptionMinimumTLSVersion);
    if (minimum_protocol != 0) {
        // Record the fact that an excepted TLS version was configured.
        sec_protocol_options_set_min_tls_protocol_version(options, minimum_protocol);
        sec_protocol_options_set_ats_minimum_tls_version_allowed(options, true);
    }

    return options;
}

sec_protocol_options_t
sec_protocol_configuration_copy_transformed_options_for_host(sec_protocol_options_t options, const char *host, bool is_direct)
{
    sec_protocol_configuration_t config = sec_protocol_options_copy_sec_protocol_configuration(options);
    if (config != nil) {
        xpc_object_t exception = sec_protocol_configuration_find_exception_for_host(config, host, false);
        bool tls_required = sec_protocol_configuration_tls_required_for_host_or_address_internal(config, host, is_direct, exception);
        if (tls_required || exception != nil) {
            sec_protocol_options_t copied_options = sec_protocol_options_copy(options);
            return sec_protocol_configuration_copy_transformed_options_for_host_or_address_internal(config, sec_protocol_configuration_copy_transformed_options_with_ats_minimums(copied_options), host, exception);
        }
    }
    return options;
}

sec_protocol_options_t
sec_protocol_configuration_copy_transformed_options_for_address(sec_protocol_options_t options, const char *address, bool is_direct)
{
    sec_protocol_configuration_t config = sec_protocol_options_copy_sec_protocol_configuration(options);
    if (config != nil) {
        xpc_object_t exception = sec_protocol_configuration_find_exception_for_address(config, address);
        bool tls_required = sec_protocol_configuration_tls_required_for_host_or_address_internal(config, address, is_direct, exception);
        if (tls_required || exception != nil) {
            sec_protocol_options_t copied_options = sec_protocol_options_copy(options);
            return sec_protocol_configuration_copy_transformed_options_for_host_or_address_internal(config, sec_protocol_configuration_copy_transformed_options_with_ats_minimums(copied_options), address, exception);
        }
    }
    return options;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
static tls_protocol_version_t
tls1x_minor_version_digit_to_version(char version_digit) {
    tls_protocol_version_t tls_protocol_version_unknown = 0;

    switch(version_digit) {
        case '0':
            return tls_protocol_version_TLSv10;
        case '1':
            return tls_protocol_version_TLSv11;
        case '2':
            return tls_protocol_version_TLSv12;
        case '3':
            return tls_protocol_version_TLSv13;
        default:
            return tls_protocol_version_unknown;
    }
}

static tls_protocol_version_t
dtls1x_minor_version_digit_to_version(char version_digit) {
    tls_protocol_version_t tls_protocol_version_unknown = 0;

    switch(version_digit) {
        case '0':
            return tls_protocol_version_DTLSv10;
        case '2':
            return tls_protocol_version_DTLSv12;
        default:
            return tls_protocol_version_unknown;
    }
}
#pragma clang diagnostic pop

tls_protocol_version_t
sec_protocol_configuration_protocol_string_to_version(const char *protocol)
{
    tls_protocol_version_t tls_protocol_version_unknown = 0;

    if (protocol == NULL) {
        return tls_protocol_version_unknown;
    }

    // The protocol string's prefix must be "(D)TLSv1.".
    int isDTLS = 0;
    if (*protocol == 'D') {
        isDTLS = 1; // Maybe DTLS
        protocol = protocol + 1;
    }

    if (strncmp(protocol, TLS_V1_PREFIX, strlen(TLS_V1_PREFIX)) != 0) {
        return tls_protocol_version_unknown;
    }
    protocol = protocol + strlen(TLS_V1_PREFIX);

    // Look up the minor version digit among the known (D)TLS 1.x versions.
    tls_protocol_version_t (*minor_version_digit_to_version[])(char) = {
        &tls1x_minor_version_digit_to_version,
        &dtls1x_minor_version_digit_to_version,
    };

    tls_protocol_version_t version = (*minor_version_digit_to_version[isDTLS])(*protocol);
    if (version == tls_protocol_version_unknown) {
        return tls_protocol_version_unknown;
    }
    protocol = protocol + 1;

    // The protocol string must end with the minor version digit.
    if (*protocol != '\0') {
        return tls_protocol_version_unknown;
    }

    return version;
}

static bool
cidr_string_to_subnet_and_mask(const char *netstr, union sockaddr_in_4_6 *network, union sockaddr_in_4_6 *mask, uint8_t *prefix)
{
    if (netstr == NULL || network == NULL || mask == NULL || prefix == NULL) {
        return false;
    }

    size_t len = strlen(netstr);
    size_t i = 0;

    int dots = 0;
    int colons = 0;
    bool doublecolon = false;
    int hex = 0;
    size_t slash_offset = len;

    for (i = 0; i < len; i++) {
        if (netstr[i] == '.') {
            if (hex || colons || slash_offset <= i || i == 0) {
                return false;
            }
            dots++;
        } else if (netstr[i] == ':') {
            if (dots || slash_offset <= i) {
                return false;
            }
            colons++;
            if (i > 0 && netstr[i - 1] == ':') {
                // Can't have a double double colon (or a triple colon)
                if (doublecolon) {
                    return false;
                }
                doublecolon = true;
            }
        } else if (netstr[i] == '/') {
            if (slash_offset <= i || i == 0 || (dots && netstr[i - 1] == '.') ||
                (colons && (i > 2 && netstr[i - 1] == ':' && netstr[i - 2] != ':'))) {
                return false;
            }
            slash_offset = i;
        } else if ((netstr[i] >= 'a' && netstr[i] <= 'f') ||
                   (netstr[i] >= 'A' && netstr[i] <= 'F')) {
            if (dots || slash_offset <= i) {
                return false;
            }
            hex++;
        } else if (!(netstr[i] >= '0' && netstr[i] <= '9')) {
            return false;
        }
    }

    if (slash_offset < len) {
        // This is probably a CIDR string
        // Note: The above code does not support interface scoped CIDR strings (fe80::%lo0/10)
        memset(network, 0, sizeof(*network));
        memset(mask, 0, sizeof(*mask));
        *prefix = 0;

        uint8_t    *maskp = NULL;
        long bits = strtol(&netstr[slash_offset + 1], NULL, 0);
        if (bits < 0 || errno == EINVAL) {
            return false;
        }

        if (hex == 0 && colons == 0) {
            // Assumes IPv4 unless there's hex or colons
            if (bits > 32) {
                return false;
            }
            network->sin.sin_family = mask->sin.sin_family = AF_INET;
            network->sin.sin_len = mask->sin.sin_len = sizeof(network->sin);
            maskp = (uint8_t*)&mask->sin.sin_addr;

            size_t alen = slash_offset;
            char ipv4buf[MAXIMUM_V4_ADDRESS_LENGTH];
            if (alen + 1 >= sizeof(ipv4buf)) {
                return false;
            }
            memcpy(ipv4buf, netstr, alen);
            while (dots < 3 && alen + 2 < sizeof(ipv4buf)) {
                ipv4buf[alen] = '.';
                alen++;
                ipv4buf[alen] = '0';
                alen++;
                dots++;
            }
            ipv4buf[alen] = '\0';

            if (inet_pton(AF_INET, ipv4buf, &network->sin.sin_addr) != 1) return false;
        } else {
            // IPv6
            if (bits > 128) {
                return false;
            }
            network->sin6.sin6_family = mask->sin6.sin6_family = AF_INET6;
            network->sin6.sin6_len = mask->sin6.sin6_len = sizeof(network->sin6);
            maskp = (uint8_t*)&mask->sin6.sin6_addr;

            size_t alen = slash_offset;
            char ipv6buf[MAXIMUM_V6_ADDRESS_LENGTH];
            if (alen + 1 >= sizeof(ipv6buf)) {
                return false;
            }
            memcpy(ipv6buf, netstr, alen);
            if (!doublecolon && colons < 7) {
                if (alen + 3 >= sizeof(ipv6buf)) {
                    return false;
                }
                ipv6buf[alen] = ':';
                alen++;
                ipv6buf[alen] = ':';
                alen++;
            }
            ipv6buf[alen] = '\0';

            if (inet_pton(AF_INET6, ipv6buf, &network->sin6.sin6_addr) != 1) {
                return false;
            }
        }

        *prefix = (uint8_t)bits;

        // Fill in the mask
        while (bits > 0) {
            static const uint8_t bb[] = {0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFF};
            *maskp = bb[bits - 1 > 7 ? 7 : bits - 1];
            maskp++;
            bits -= 8;
        }

        return true;
    }

    return false;
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

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
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
#pragma clang diagnostic pop

void
sec_protocol_configuration_populate_insecure_defaults(sec_protocol_configuration_t config)
{
    xpc_object_t dict = sec_protocol_configuration_get_map(config);
    xpc_object_t domain_map = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_value(dict, kExceptionDomains, domain_map);
    xpc_object_t cidr_exceptions_map = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_value(dict, _kCIDRExceptions, cidr_exceptions_map);

    xpc_dictionary_set_uint64(dict, kAllowsArbitraryLoadsInWebContent, (uint64_t)kATSGlobalKeyNotPresent);
    xpc_dictionary_set_uint64(dict, kAllowsArbitraryLoadsForMedia, (uint64_t)kATSGlobalKeyNotPresent);
    xpc_dictionary_set_uint64(dict, kAllowsLocalNetworking, (uint64_t)kATSGlobalKeyNotPresent);
    xpc_dictionary_set_uint64(dict, kAllowsArbitraryLoads, (uint64_t)kATSGlobalKeyValueTrue);
}

void
sec_protocol_configuration_populate_secure_defaults(sec_protocol_configuration_t config)
{
    xpc_object_t dict = sec_protocol_configuration_get_map(config);
    xpc_object_t domain_map = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_value(dict, kExceptionDomains, domain_map);
    xpc_object_t cidr_exceptions_map = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_value(dict, _kCIDRExceptions, cidr_exceptions_map);

    xpc_dictionary_set_uint64(dict, kAllowsArbitraryLoadsInWebContent, (uint64_t)kATSGlobalKeyNotPresent);
    xpc_dictionary_set_uint64(dict, kAllowsArbitraryLoadsForMedia, (uint64_t)kATSGlobalKeyNotPresent);
    xpc_dictionary_set_uint64(dict, kAllowsLocalNetworking, (uint64_t)kATSGlobalKeyNotPresent);
    xpc_dictionary_set_uint64(dict, kAllowsArbitraryLoads, (uint64_t)kATSGlobalKeyNotPresent);
}

bool
sec_protocol_configuration_set_ats_overrides(sec_protocol_configuration_t config, CFDictionaryRef plist)
{
    if (plist == NULL) {
        return false;
    }

#define ATS_VALUE_FOR_KEY(dictionary, key, value, default) \
    kATSGlobalKey value = default; \
    { \
        NSNumber *nsValue = dictionary[@key]; \
        if (nsValue) { \
            if ([nsValue isKindOfClass:[NSNumber class]]) { \
                value = [nsValue isEqual:@YES] ? kATSGlobalKeyValueTrue : kATSGlobalKeyValueFalse; \
            } else { \
                os_log_error(OS_LOG_DEFAULT, "App Transport Security value for key %s must be a boolean", key); \
            } \
        } \
    }
#define BOOLEAN_FOR_KEY(dictionary, key, value, default) \
    bool value = default; \
    { \
        NSNumber *nsValue = dictionary[@key]; \
        if (nsValue) { \
            if ([nsValue isKindOfClass:[NSNumber class]]) { \
                value = [nsValue isEqual:@YES]; \
            } else { \
                os_log_error(OS_LOG_DEFAULT, "App Transport Security value for key %s must be a boolean", key); \
            } \
        } \
    }
#define STRING_FOR_KEY(dictionary, key, value, default) \
    NSString *value = default; \
    { \
        NSString *nsValue = dictionary[@key]; \
        if (nsValue) { \
            if ([nsValue isKindOfClass:[NSString class]]) { \
                value = nsValue; \
            } else { \
                os_log_error(OS_LOG_DEFAULT, "App Transport Security value for key %s must be a string", key); \
            } \
        } \
    }

    xpc_object_t dict = sec_protocol_configuration_get_map(config);
    if (dict == nil) {
        return false;
    }

    NSDictionary *plist_dictionary = (__bridge NSDictionary *)plist;
    ATS_VALUE_FOR_KEY(plist_dictionary, kAllowsArbitraryLoads, arbitrary_loads, kATSGlobalKeyNotPresent);
    ATS_VALUE_FOR_KEY(plist_dictionary, kAllowsArbitraryLoadsInWebContent, web_loads, kATSGlobalKeyNotPresent);
    ATS_VALUE_FOR_KEY(plist_dictionary, kAllowsArbitraryLoadsForMedia, media_loads, kATSGlobalKeyNotPresent);
    ATS_VALUE_FOR_KEY(plist_dictionary, kAllowsLocalNetworking, local_networking, kATSGlobalKeyNotPresent);

    xpc_dictionary_set_uint64(dict, kAllowsArbitraryLoads, (uint64_t)arbitrary_loads);
    xpc_dictionary_set_uint64(dict, kAllowsArbitraryLoadsInWebContent, (uint64_t)web_loads);
    xpc_dictionary_set_uint64(dict, kAllowsArbitraryLoadsForMedia, (uint64_t)media_loads);
    xpc_dictionary_set_uint64(dict, kAllowsLocalNetworking, (uint64_t)local_networking);

    NSDictionary *exception_domains = [plist_dictionary valueForKey:@kExceptionDomains];
    if (exception_domains == nil) {
        return true;
    } else if (![exception_domains isKindOfClass:[NSDictionary class]]) {
        os_log_error(OS_LOG_DEFAULT, "App Transport Security exceptions must be a dictionary");
        return false;
    }

    xpc_object_t domain_map = xpc_dictionary_get_dictionary(dict, kExceptionDomains);
    if (domain_map == nil) {
        // The domain map MUST be present during initialization
        return false;
    }

    xpc_object_t cidr_exceptions_map = xpc_dictionary_get_dictionary(dict, _kCIDRExceptions);
    if (cidr_exceptions_map == nil) {
        // The CIDR exceptions map MUST be present during initialization
        return false;
    }

    [exception_domains enumerateKeysAndObjectsUsingBlock:^(id _key, id _obj, BOOL *stop) {
        if (![_key isKindOfClass:[NSString class]] || ![_obj isKindOfClass:[NSDictionary class]]) {
            // Exception domains MUST have ATS information set.
            os_log_error(OS_LOG_DEFAULT, "App Transport Security exception must be a dictionary");
            return;
        }
        NSString *domain = (NSString *)_key;
        NSDictionary *entry = (NSDictionary *)_obj;

        BOOLEAN_FOR_KEY(entry, kExceptionAllowsInsecureHTTPLoads, allows_http, false);
        BOOLEAN_FOR_KEY(entry, kIncludesSubdomains, includes_subdomains, false);
        BOOLEAN_FOR_KEY(entry, kExceptionRequiresForwardSecrecy, requires_pfs, true);
        STRING_FOR_KEY(entry, kExceptionMinimumTLSVersion, minimum_tls, @"TLSv1.2");

        xpc_object_t entry_map = xpc_dictionary_create(NULL, NULL, 0);
        xpc_dictionary_set_bool(entry_map, kIncludesSubdomains, includes_subdomains);
        xpc_dictionary_set_bool(entry_map, kExceptionAllowsInsecureHTTPLoads, allows_http);
        xpc_dictionary_set_bool(entry_map, kExceptionRequiresForwardSecrecy, requires_pfs);
        xpc_dictionary_set_int64(entry_map, kExceptionMinimumTLSVersion, sec_protocol_configuration_protocol_string_to_version([minimum_tls cStringUsingEncoding:NSUTF8StringEncoding]));

        if ([domain rangeOfString:@"/"].location != NSNotFound) {
            union sockaddr_in_4_6 address = {};
            union sockaddr_in_4_6 mask = {};
            uint8_t prefix = 0;
            if (cidr_string_to_subnet_and_mask(domain.UTF8String, &address, &mask, &prefix)) {
                NSData *cidr_address = [NSData dataWithBytes:(const UInt8*)&address length:sizeof(union sockaddr_in_4_6)];
                NSData *cidr_mask = [NSData dataWithBytes:(const UInt8*)&mask length:sizeof(union sockaddr_in_4_6)];
                xpc_dictionary_set_data(entry_map, _kATSParsedCIDRAddressKey, cidr_address.bytes, cidr_address.length);
                xpc_dictionary_set_data(entry_map, _kATSParsedCIDRMaskKey, cidr_mask.bytes, cidr_mask.length);
                xpc_dictionary_set_uint64(entry_map, _kATSParsedCIDRPrefixKey, (uint64_t) prefix);
            } else {
                static dispatch_once_t onceToken = 0;
                dispatch_once(&onceToken, ^{
                    os_log_fault(OS_LOG_DEFAULT, "App Transport Security exception %{public}@ is not a valid CIDR notation.", domain);
                });
            }
            xpc_dictionary_set_value(cidr_exceptions_map, [domain cStringUsingEncoding:NSUTF8StringEncoding], entry_map);
        } else {
            xpc_dictionary_set_value(domain_map, [domain cStringUsingEncoding:NSUTF8StringEncoding], entry_map);
        }
    }];

#undef STRING_FOR_KEY
#undef BOOLEAN_FOR_KEY
#undef ATS_VALUE_FOR_KEY

    return true;
}
