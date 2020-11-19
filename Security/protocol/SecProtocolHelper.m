//
//  SecProtocolHelper.m
//  Security_ios
//
//

#import "SecProtocolInternal.h"

#define DefineTLSCiphersuiteGroupList(XXX, ...) \
    static const tls_ciphersuite_t List##XXX[] = { \
        __VA_ARGS__ \
    };

DefineTLSCiphersuiteGroupList(tls_ciphersuite_group_default,
                              CiphersuitesTLS13,
                              CiphersuitesPFS);
DefineTLSCiphersuiteGroupList(tls_ciphersuite_group_compatibility,
                              CiphersuitesNonPFS,
                              CiphersuitesTLS10,
                              CiphersuitesTLS10_3DES);
DefineTLSCiphersuiteGroupList(tls_ciphersuite_group_legacy,
                              CiphersuitesDHE);
DefineTLSCiphersuiteGroupList(tls_ciphersuite_group_ats,
                              CiphersuitesTLS13,
                              CiphersuitesPFS);
DefineTLSCiphersuiteGroupList(tls_ciphersuite_group_ats_compatibility,
                              CiphersuitesNonPFS);

typedef struct tls_ciphersuite_definition {
    tls_ciphersuite_t ciphersuite;
    tls_protocol_version_t min_version;
    tls_protocol_version_t max_version;
    char ciphersuite_name[64];
} *tls_ciphersuite_definition_t;

#define DefineTLSCiphersuiteDefinition(XXX, MIN_VERSION, MAX_VERSION) \
{ \
    .ciphersuite = XXX, \
    .ciphersuite_name = "##XXX", \
    .min_version = MIN_VERSION, \
    .max_version = MAX_VERSION, \
}

static const struct tls_ciphersuite_definition tls_ciphersuite_definitions[] = {
    // TLS 1.3 ciphersuites
    DefineTLSCiphersuiteDefinition(TLS_AES_128_GCM_SHA256,                          tls_protocol_version_TLSv13, tls_protocol_version_TLSv13),
    DefineTLSCiphersuiteDefinition(TLS_AES_256_GCM_SHA384,                          tls_protocol_version_TLSv13, tls_protocol_version_TLSv13),
    DefineTLSCiphersuiteDefinition(TLS_CHACHA20_POLY1305_SHA256,                    tls_protocol_version_TLSv13, tls_protocol_version_TLSv13),
    
    // RFC 7905: ChaCha20-Poly1305 Cipher Suites for Transport Layer Security (TLS)
    DefineTLSCiphersuiteDefinition(TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256,   tls_protocol_version_TLSv12, tls_protocol_version_TLSv12),
    DefineTLSCiphersuiteDefinition(TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,     tls_protocol_version_TLSv12, tls_protocol_version_TLSv12),
    
    // RFC 5289: TLS Elliptic Curve Cipher Suites with SHA-256/384 and AES Galois Counter Mode (GCM)
    DefineTLSCiphersuiteDefinition(TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,         tls_protocol_version_TLSv12, tls_protocol_version_TLSv12),
    DefineTLSCiphersuiteDefinition(TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,         tls_protocol_version_TLSv12, tls_protocol_version_TLSv12),
    DefineTLSCiphersuiteDefinition(TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384,         tls_protocol_version_TLSv12, tls_protocol_version_TLSv12),
    DefineTLSCiphersuiteDefinition(TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,         tls_protocol_version_TLSv12, tls_protocol_version_TLSv12),
    DefineTLSCiphersuiteDefinition(TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,           tls_protocol_version_TLSv12, tls_protocol_version_TLSv12),
    DefineTLSCiphersuiteDefinition(TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,           tls_protocol_version_TLSv12, tls_protocol_version_TLSv12),
    DefineTLSCiphersuiteDefinition(TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384,           tls_protocol_version_TLSv12, tls_protocol_version_TLSv12),
    DefineTLSCiphersuiteDefinition(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,           tls_protocol_version_TLSv12, tls_protocol_version_TLSv12),
    
    // RFC 5288: AES Galois Counter Mode (GCM) Cipher Suites for TLS
    DefineTLSCiphersuiteDefinition(TLS_RSA_WITH_AES_256_GCM_SHA384,                 tls_protocol_version_TLSv12, tls_protocol_version_TLSv12),
    DefineTLSCiphersuiteDefinition(TLS_RSA_WITH_AES_128_GCM_SHA256,                 tls_protocol_version_TLSv12, tls_protocol_version_TLSv12),
    DefineTLSCiphersuiteDefinition(TLS_DHE_RSA_WITH_AES_256_GCM_SHA384,             tls_protocol_version_TLSv12, tls_protocol_version_TLSv12),
    DefineTLSCiphersuiteDefinition(TLS_DHE_RSA_WITH_AES_128_GCM_SHA256,             tls_protocol_version_TLSv12, tls_protocol_version_TLSv12),
    
    // RFC 5246: The Transport Layer Security (TLS) Protocol Version 1.2
    DefineTLSCiphersuiteDefinition(TLS_RSA_WITH_AES_256_CBC_SHA256,                 tls_protocol_version_TLSv12, tls_protocol_version_TLSv12),
    DefineTLSCiphersuiteDefinition(TLS_RSA_WITH_AES_128_CBC_SHA256,                 tls_protocol_version_TLSv12, tls_protocol_version_TLSv12),
    DefineTLSCiphersuiteDefinition(SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA,               tls_protocol_version_TLSv12, tls_protocol_version_TLSv12),
    DefineTLSCiphersuiteDefinition(SSL_RSA_WITH_3DES_EDE_CBC_SHA,                   tls_protocol_version_TLSv12, tls_protocol_version_TLSv12),
    DefineTLSCiphersuiteDefinition(TLS_DHE_RSA_WITH_AES_256_CBC_SHA256,             tls_protocol_version_TLSv12, tls_protocol_version_TLSv12),
    DefineTLSCiphersuiteDefinition(TLS_DHE_RSA_WITH_AES_128_CBC_SHA256,             tls_protocol_version_TLSv12, tls_protocol_version_TLSv12),
    
    // RFC 4492: Elliptic Curve Cryptography (ECC) Cipher Suites for Transport Layer Security (TLS)
    DefineTLSCiphersuiteDefinition(TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,            tls_protocol_version_TLSv10,  tls_protocol_version_TLSv11),
    DefineTLSCiphersuiteDefinition(TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,            tls_protocol_version_TLSv10,  tls_protocol_version_TLSv11),
    DefineTLSCiphersuiteDefinition(TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,              tls_protocol_version_TLSv10,  tls_protocol_version_TLSv11),
    DefineTLSCiphersuiteDefinition(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,              tls_protocol_version_TLSv10,  tls_protocol_version_TLSv11),
    DefineTLSCiphersuiteDefinition(TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA,           tls_protocol_version_TLSv10,  tls_protocol_version_TLSv11),
    DefineTLSCiphersuiteDefinition(TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA,             tls_protocol_version_TLSv10,  tls_protocol_version_TLSv11),
    
    // RFC 3268: Advanced Encryption Standard (AES) Ciphersuites for Transport Layer Security (TLS)
    DefineTLSCiphersuiteDefinition(TLS_RSA_WITH_AES_256_CBC_SHA,                    tls_protocol_version_TLSv10,  tls_protocol_version_TLSv11),
    DefineTLSCiphersuiteDefinition(TLS_RSA_WITH_AES_128_CBC_SHA,                    tls_protocol_version_TLSv10,  tls_protocol_version_TLSv11),
    DefineTLSCiphersuiteDefinition(TLS_RSA_WITH_AES_256_CBC_SHA,                    tls_protocol_version_TLSv10,  tls_protocol_version_TLSv11),
    DefineTLSCiphersuiteDefinition(TLS_DHE_RSA_WITH_AES_256_CBC_SHA,                tls_protocol_version_TLSv10,  tls_protocol_version_TLSv11),
    DefineTLSCiphersuiteDefinition(TLS_RSA_WITH_AES_128_CBC_SHA,                    tls_protocol_version_TLSv10,  tls_protocol_version_TLSv11),
    DefineTLSCiphersuiteDefinition(TLS_DHE_RSA_WITH_AES_128_CBC_SHA,                tls_protocol_version_TLSv10,  tls_protocol_version_TLSv11),
    DefineTLSCiphersuiteDefinition(TLS_DHE_RSA_WITH_AES_256_CBC_SHA,                tls_protocol_version_TLSv10,  tls_protocol_version_TLSv11),
    DefineTLSCiphersuiteDefinition(TLS_DHE_RSA_WITH_AES_128_CBC_SHA,                tls_protocol_version_TLSv10,  tls_protocol_version_TLSv11),
};

// Size of the definition list
static const size_t tls_ciphersuite_definitions_length = \
    sizeof(tls_ciphersuite_definitions) / sizeof(struct tls_ciphersuite_definition);

const tls_ciphersuite_t *
sec_protocol_helper_ciphersuite_group_to_ciphersuite_list(tls_ciphersuite_group_t group, size_t *list_count)
{
    if (list_count == NULL) {
        return NULL;
    }
    
    const tls_ciphersuite_t *ciphersuites = NULL;
    size_t count = 0;
    
#define CASE_CONFIG(GROUPNAME) \
    case GROUPNAME: \
        ciphersuites = List##GROUPNAME; \
        count = sizeof(List##GROUPNAME) / sizeof(tls_ciphersuite_t); \
        break;
    
    switch (group) {
        CASE_CONFIG(tls_ciphersuite_group_default);
        CASE_CONFIG(tls_ciphersuite_group_compatibility);
        CASE_CONFIG(tls_ciphersuite_group_legacy);
        CASE_CONFIG(tls_ciphersuite_group_ats);
        CASE_CONFIG(tls_ciphersuite_group_ats_compatibility);
    }
    
#undef CASE_CONFIG
    
    if (ciphersuites != NULL) {
        *list_count = count;
        return ciphersuites;
    }
    
    *list_count = 0;
    return NULL;
}

bool
sec_protocol_helper_ciphersuite_group_contains_ciphersuite(tls_ciphersuite_group_t group, tls_ciphersuite_t suite)
{
    size_t list_size = 0;
    const tls_ciphersuite_t *list = sec_protocol_helper_ciphersuite_group_to_ciphersuite_list(group, &list_size);
    if (list == NULL) {
        return false;
    }
    
    for (size_t i = 0; i < list_size; i++) {
        tls_ciphersuite_t other = list[i];
        if (other == suite) {
            return true;
        }
    }
    
    return false;
}

tls_protocol_version_t
sec_protocol_helper_ciphersuite_minimum_TLS_version(tls_ciphersuite_t ciphersuite)
{
    for (size_t i = 0; i < tls_ciphersuite_definitions_length; i++) {
        if (tls_ciphersuite_definitions[i].ciphersuite == ciphersuite) {
            return tls_ciphersuite_definitions[i].min_version;
        }
    }
    return 0;
}

tls_protocol_version_t
sec_protocol_helper_ciphersuite_maximum_TLS_version(tls_ciphersuite_t ciphersuite)
{
    for (size_t i = 0; i < tls_ciphersuite_definitions_length; i++) {
        if (tls_ciphersuite_definitions[i].ciphersuite == ciphersuite) {
            return tls_ciphersuite_definitions[i].max_version;
        }
    }
    return 0;
}

const char *
sec_protocol_helper_get_ciphersuite_name(tls_ciphersuite_t ciphersuite)
{
#define CIPHERSUITE_TO_NAME(ciphersuite) \
    case ciphersuite: { \
        return #ciphersuite; \
    }
    
    switch (ciphersuite) {
        CIPHERSUITE_TO_NAME(TLS_AES_128_GCM_SHA256);
        CIPHERSUITE_TO_NAME(TLS_AES_256_GCM_SHA384);
        CIPHERSUITE_TO_NAME(TLS_CHACHA20_POLY1305_SHA256);
        CIPHERSUITE_TO_NAME(TLS_RSA_WITH_AES_256_GCM_SHA384);
        CIPHERSUITE_TO_NAME(TLS_RSA_WITH_AES_128_GCM_SHA256);
        CIPHERSUITE_TO_NAME(TLS_RSA_WITH_AES_256_CBC_SHA256);
        CIPHERSUITE_TO_NAME(TLS_RSA_WITH_AES_128_CBC_SHA256);
        CIPHERSUITE_TO_NAME(TLS_RSA_WITH_AES_256_CBC_SHA);
        CIPHERSUITE_TO_NAME(TLS_RSA_WITH_AES_128_CBC_SHA);
        CIPHERSUITE_TO_NAME(TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA);
        CIPHERSUITE_TO_NAME(TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA);
        CIPHERSUITE_TO_NAME(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA);
        CIPHERSUITE_TO_NAME(TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA);
        CIPHERSUITE_TO_NAME(TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA);
        CIPHERSUITE_TO_NAME(TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA);
        CIPHERSUITE_TO_NAME(SSL_RSA_WITH_3DES_EDE_CBC_SHA);
        CIPHERSUITE_TO_NAME(TLS_DHE_RSA_WITH_AES_256_GCM_SHA384);
        CIPHERSUITE_TO_NAME(TLS_DHE_RSA_WITH_AES_128_GCM_SHA256);
        CIPHERSUITE_TO_NAME(TLS_DHE_RSA_WITH_AES_256_CBC_SHA256);
        CIPHERSUITE_TO_NAME(TLS_DHE_RSA_WITH_AES_128_CBC_SHA256);
        CIPHERSUITE_TO_NAME(TLS_DHE_RSA_WITH_AES_256_CBC_SHA);
        CIPHERSUITE_TO_NAME(TLS_DHE_RSA_WITH_AES_128_CBC_SHA);
        CIPHERSUITE_TO_NAME(SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA);
        CIPHERSUITE_TO_NAME(TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384);
        CIPHERSUITE_TO_NAME(TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256);
        CIPHERSUITE_TO_NAME(TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384);
        CIPHERSUITE_TO_NAME(TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256);
        CIPHERSUITE_TO_NAME(TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256);
        CIPHERSUITE_TO_NAME(TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384);
        CIPHERSUITE_TO_NAME(TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256);
        CIPHERSUITE_TO_NAME(TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384);
        CIPHERSUITE_TO_NAME(TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256);
        CIPHERSUITE_TO_NAME(TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256);
    }
    
#undef CIPHERSUITE_TO_NAME
    return NULL;
}

#define KeyExchangeGroupsDefault \
    tls_key_exchange_group_X25519, \
    tls_key_exchange_group_X448
#define KeyExchangeGroupsCompatibility \
    tls_key_exchange_group_Secp256r1, \
    tls_key_exchange_group_Secp384r1, \
    tls_key_exchange_group_Secp521r1
#define KeyExchangeGroupsLegacy \
    tls_key_exchange_group_FFDHE2048, \
    tls_key_exchange_group_FFDHE3072, \
    tls_key_exchange_group_FFDHE4096, \
    tls_key_exchange_group_FFDHE6144, \
    tls_key_exchange_group_FFDHE8192

#define DefineTLSKeyExchangeGroupList(XXX, ...) \
    static const tls_key_exchange_group_t List##XXX[] = { \
        __VA_ARGS__ \
    };

DefineTLSKeyExchangeGroupList(tls_key_exchange_group_set_default,
                              KeyExchangeGroupsDefault);
DefineTLSKeyExchangeGroupList(tls_key_exchange_group_set_compatibility,
                              KeyExchangeGroupsCompatibility);
DefineTLSKeyExchangeGroupList(tls_key_exchange_group_set_legacy,
                              KeyExchangeGroupsLegacy);

const tls_key_exchange_group_t *
sec_protocol_helper_tls_key_exchange_group_set_to_key_exchange_group_list(tls_key_exchange_group_set_t set, size_t *listSize)
{
    if (listSize == NULL) {
        return NULL;
    }
    
    const tls_key_exchange_group_t *groups = NULL;
    size_t count = 0;
    
#define CASE_CONFIG(SETNAME) \
case SETNAME: \
groups = List##SETNAME; \
count = sizeof(List##SETNAME) / sizeof(SSLKeyExchangeGroup); \
break;
    
    switch (set) {
        CASE_CONFIG(tls_key_exchange_group_set_default);
        CASE_CONFIG(tls_key_exchange_group_set_compatibility);
        CASE_CONFIG(tls_key_exchange_group_set_legacy);
    }
    
#undef CASE_CONFIG
    
    if (groups != NULL) {
        *listSize = count;
        return groups;
    }
    
    *listSize = 0;
    return NULL;
}

#undef DefineTLSKeyExchangeGroupList
#undef KeyExchangeGroupsDefault
#undef KeyExchangeGroupsCompatibility
#undef KeyExchangeGroupsLegacy

bool
sec_protocol_helper_dispatch_data_equal(dispatch_data_t left, dispatch_data_t right)
{
    if (!left || !right || left == right) {
        return left == right;
    }
    if (dispatch_data_get_size(left) != dispatch_data_get_size(right)) {
        return false;
    }
    __block bool is_equal = true;
    dispatch_data_apply(left,
                        ^bool(__unused dispatch_data_t _Nonnull lregion, size_t loffset, const void *_Nonnull lbuffer, size_t lsize) {
                            dispatch_data_apply(right,
                                                ^bool(__unused dispatch_data_t _Nonnull rregion, size_t roffset, const void *_Nonnull rbuffer,
                                                      size_t rsize) {
                                                    // There is some overlap
                                                    const size_t start = MAX(loffset, roffset);
                                                    const size_t end = MIN(loffset + lsize, roffset + rsize);
                                                    if (start < end) {
                                                        is_equal = memcmp(&((const uint8_t *)rbuffer)[start - roffset],
                                                                          &((const uint8_t *)lbuffer)[start - loffset], end - start) == 0;
                                                    } else {
                                                        if (roffset > loffset + lsize) {
                                                            // Iteration of right has gone past where we're at on left, bail out of inner apply
                                                            // left |---|
                                                            // right      |---|
                                                            return false;
                                                        } else if (roffset + rsize < loffset) {
                                                            // Iteration of right has not yet reached where we're at on left, keep going
                                                            // left        |---|
                                                            // right  |--|
                                                            return true;
                                                        }
                                                    }

                                                    return is_equal;
                                                });
                            return is_equal;
                        });
    return is_equal;
}
