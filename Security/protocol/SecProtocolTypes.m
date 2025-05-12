//
//  SecProtocolTypes.m
//  Security
//

#import "utilities/SecCFRelease.h"
#import "utilities/SecCFWrappers.h"

#import <corecrypto/ccspake.h>
#import <corecrypto/ccscrypt.h>

#define OS_OBJECT_HAVE_OBJC_SUPPORT 1

#define SEC_NULL_BAD_INPUT ((void *_Nonnull)NULL)
#define SEC_NULL_OUT_OF_MEMORY SEC_NULL_BAD_INPUT

#define SEC_NIL_BAD_INPUT ((void *_Nonnull)nil)
#define SEC_NIL_OUT_OF_MEMORY SEC_NIL_BAD_INPUT

#define SEC_CONCRETE_CLASS_NAME(external_type) SecConcrete_##external_type
#define SEC_CONCRETE_PREFIX_STR "SecConcrete_"

#define SEC_OBJECT_DECL_INTERNAL_OBJC(external_type)                                                    \
    @class SEC_CONCRETE_CLASS_NAME(external_type);                                                      \
    typedef SEC_CONCRETE_CLASS_NAME(external_type) *external_type##_t

#define SEC_OBJECT_IMPL_INTERNAL_OBJC_WITH_PROTOCOL_AND_VISBILITY(external_type, _protocol, visibility, ...)    \
    @protocol OS_OBJECT_CLASS(external_type) <_protocol>                                                        \
    @end                                                                                                        \
    visibility                                                                                                  \
    @interface SEC_CONCRETE_CLASS_NAME(external_type) : NSObject<OS_OBJECT_CLASS(external_type)>                \
        _Pragma("clang diagnostic push")                                                                    \
        _Pragma("clang diagnostic ignored \"-Wobjc-interface-ivars\"")                                      \
            __VA_ARGS__                                                                                     \
        _Pragma("clang diagnostic pop")                                                                     \
    @end                                                                                                    \
    typedef int _useless_typedef_oio_##external_type

#define SEC_OBJECT_IMPL_INTERNAL_OBJC_WITH_PROTOCOL(external_type, _protocol, ...)                      \
    SEC_OBJECT_IMPL_INTERNAL_OBJC_WITH_PROTOCOL_AND_VISBILITY(external_type, _protocol, ,__VA_ARGS__)

#define SEC_OBJECT_IMPL_INTERNAL_OBJC(external_type, ...)                                               \
    SEC_OBJECT_IMPL_INTERNAL_OBJC_WITH_PROTOCOL(external_type, NSObject, ##__VA_ARGS__)

#define SEC_OBJECT_IMPL_INTERNAL_OBJC_WITH_VISIBILITY(external_type, visibility, ...)                   \
    SEC_OBJECT_IMPL_INTERNAL_OBJC_WITH_PROTOCOL_AND_VISBILITY(external_type, NSObject, visibility, ##__VA_ARGS__)

#define SEC_OBJECT_IMPL 1

SEC_OBJECT_DECL_INTERNAL_OBJC(sec_array);
SEC_OBJECT_DECL_INTERNAL_OBJC(sec_identity);
SEC_OBJECT_DECL_INTERNAL_OBJC(sec_trust);
SEC_OBJECT_DECL_INTERNAL_OBJC(sec_certificate);
SEC_OBJECT_DECL_INTERNAL_OBJC(sec_protocol_configuration_builder);
SEC_OBJECT_DECL_INTERNAL_OBJC(sec_object);
SEC_OBJECT_DECL_INTERNAL_OBJC(sec_protocol_options);
SEC_OBJECT_DECL_INTERNAL_OBJC(sec_protocol_metadata);
SEC_OBJECT_DECL_INTERNAL_OBJC(sec_protocol_configuration);

#import "SecProtocolInternal.h"
#import <Security/SecProtocolPriv.h>
#import "SecProtocolTypesPriv.h"

#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>
#import <CoreFoundation/CFPriv.h>

#import <os/log.h>
#import <xpc/private.h>

#import <os/object.h>

SEC_OBJECT_IMPL_INTERNAL_OBJC(sec_array,
{
    xpc_object_t xpc_array;
});

@implementation SEC_CONCRETE_CLASS_NAME(sec_array)

- (instancetype)init
{
    if ((self = [super init])) {
        self->xpc_array = xpc_array_create(NULL, 0);
    } else {
        return SEC_NIL_OUT_OF_MEMORY;
    }
    return self;
}

- (void)dealloc
{
    if (self->xpc_array != nil) {
        xpc_array_apply(self->xpc_array, ^bool(size_t index, __unused xpc_object_t value) {
            void *pointer = xpc_array_get_pointer(self->xpc_array, index);
            sec_object_t object = (sec_object_t)CFBridgingRelease(pointer);
            do [[clang::suppress]] { if (object) {} } while (0);
            object = nil;
            return true;
        });
        self->xpc_array = nil;
    }
}

sec_array_t
sec_array_create(void)
{
    return [[SEC_CONCRETE_CLASS_NAME(sec_array) alloc] init];
}

void
sec_array_append(sec_array_t array, sec_object_t object)
{
    if (array != NULL &&
        array->xpc_array != NULL && xpc_get_type(array->xpc_array) == XPC_TYPE_ARRAY &&
        object != NULL) {
        void *retained_pointer = __DECONST(void *, CFBridgingRetain(object));
        xpc_array_set_pointer(array->xpc_array, XPC_ARRAY_APPEND, retained_pointer);
        // 'Leak' the retain, and save the pointer into the array
    }
}

size_t
sec_array_get_count(sec_array_t array)
{
    if (array != NULL &&
        array->xpc_array != NULL && xpc_get_type(array->xpc_array) == XPC_TYPE_ARRAY) {
        return xpc_array_get_count(array->xpc_array);
    }
    return 0;
}

bool
sec_array_apply(sec_array_t array, sec_array_applier_t applier)
{
    if (array != NULL &&
        array->xpc_array != NULL && xpc_get_type(array->xpc_array) == XPC_TYPE_ARRAY) {
        return xpc_array_apply(array->xpc_array, ^bool(size_t index, __unused xpc_object_t value) {
            void *pointer = xpc_array_get_pointer(array->xpc_array, index);
            return applier(index, (__bridge sec_object_t)(pointer));
        });
    }
    return false;
}

@end

SEC_OBJECT_IMPL_INTERNAL_OBJC(sec_identity,
{
    SecIdentityRef identity;
    CFArrayRef certs;
    sec_protocol_private_key_sign_t sign_block;
    sec_protocol_private_key_decrypt_t decrypt_block;
    dispatch_queue_t operation_queue;
    sec_identity_type_t type;

    // SPAKE2+ credential information
    dispatch_data_t spake2_context;
    dispatch_data_t client_identity;
    dispatch_data_t server_identity;
    dispatch_data_t client_password_verifier;
    dispatch_data_t server_password_verifier;
    dispatch_data_t registration_record;
});

@implementation SEC_CONCRETE_CLASS_NAME(sec_identity)

- (instancetype)initWithIdentity:(SecIdentityRef)_identity
{
    if (_identity == NULL) {
        return SEC_NIL_BAD_INPUT;
    }

    if ((self = [super init])) {
        self->identity = __DECONST(SecIdentityRef, CFRetainSafe(_identity));
        self->type = SEC_PROTOCOL_IDENTITY_TYPE_CERTIFICATE;
    } else {
        return SEC_NIL_OUT_OF_MEMORY;
    }
    return self;
}

- (instancetype)initWithIdentityAndCertificates:(SecIdentityRef)_identity certificates:(CFArrayRef)certificates
{
    if (_identity == NULL) {
        return SEC_NIL_BAD_INPUT;
    }
    
    if ((self = [super init])) {
        self->identity = __DECONST(SecIdentityRef, CFRetainSafe(_identity));
        self->certs = __DECONST(CFArrayRef, CFRetainSafe(certificates));
        self->type = SEC_PROTOCOL_IDENTITY_TYPE_CERTIFICATE;
    } else {
        return SEC_NIL_OUT_OF_MEMORY;
    }
    
    return self;
}

- (instancetype)initWithCertificates:(CFArrayRef)certificates signBlock:(sec_protocol_private_key_sign_t)sign decryptBlock:(sec_protocol_private_key_decrypt_t)decrypt queue:(dispatch_queue_t)queue
{
    if (certificates == NULL) {
        return SEC_NIL_BAD_INPUT;
    }
    if (sign == NULL) {
        return SEC_NIL_BAD_INPUT;
    }
    if (decrypt == NULL) {
        return SEC_NIL_BAD_INPUT;
    }

    if ((self = [super init])) {
        self->certs = __DECONST(CFArrayRef, CFRetainSafe(certificates));
        self->sign_block = sign;
        self->decrypt_block = decrypt;
        self->operation_queue = queue;
        self->type = SEC_PROTOCOL_IDENTITY_TYPE_CERTIFICATE;
    } else {
        return SEC_NIL_OUT_OF_MEMORY;
    }
    return self;
}

- (instancetype)initWithSPAKE2PLUSV1Context:(dispatch_data_t)context clientIdentity:(dispatch_data_t)clientIdentity serverIdentity:(dispatch_data_t)serverIdentity clientPasswordVerifier:(dispatch_data_t)clientPasswordVerifier
                     serverPasswordVerifier:(dispatch_data_t)serverPasswordVerifier registrationRecord:(dispatch_data_t)registrationRecord
{
    if (context == NULL) {
        return SEC_NIL_BAD_INPUT;
    }

    if ((self = [super init])) {
        self->spake2_context = context;
        self->client_identity = clientIdentity;
        self->server_identity = serverIdentity;
        self->client_password_verifier = clientPasswordVerifier;
        self->server_password_verifier = serverPasswordVerifier;
        self->registration_record = registrationRecord;
        self->type = SEC_PROTOCOL_IDENTITY_TYPE_SPAKE2PLUSV1;
    } else {
        return SEC_NIL_OUT_OF_MEMORY;
    }
    return self;
}

- (void)dealloc
{
    if (self->identity != NULL) {
        CFRelease(self->identity);
        self->identity = NULL;
        
        if (self->certs) {
            CFRelease(self->certs);
        }
        self->certs = NULL;
    }
}

sec_identity_t
sec_identity_create(SecIdentityRef identity)
{
    return [[SEC_CONCRETE_CLASS_NAME(sec_identity) alloc] initWithIdentity:identity];
}

sec_identity_t
sec_identity_create_with_certificates(SecIdentityRef identity, CFArrayRef certificates)
{
    return [[SEC_CONCRETE_CLASS_NAME(sec_identity) alloc] initWithIdentityAndCertificates:identity certificates:certificates];
}

sec_identity_t
sec_identity_create_with_certificates_and_external_private_key(CFArrayRef __nonnull certificates,
                                                      sec_protocol_private_key_sign_t sign_block,
                                                      sec_protocol_private_key_decrypt_t decrypt_block,
                                                      dispatch_queue_t queue)
{
    return [[SEC_CONCRETE_CLASS_NAME(sec_identity) alloc] initWithCertificates:certificates signBlock:sign_block decryptBlock:decrypt_block queue:queue];
}

sec_identity_t
sec_identity_create_client_SPAKE2PLUSV1_identity_internal(dispatch_data_t context,
                                                          dispatch_data_t client_identity,
                                                          dispatch_data_t server_identity,
                                                          dispatch_data_t password_verifier)
{
    if (password_verifier == nil || dispatch_data_get_size(password_verifier) != SEC_PROTOCOL_SPAKE2PLUSV1_INPUT_PASSWORD_VERIFIER_NBYTES) {
        return SEC_NIL_BAD_INPUT;
    }

    dispatch_data_t client_password_verifier = sec_identity_create_SPAKE2PLUSV1_client_password_verifier(password_verifier);
    dispatch_data_t server_password_verifier = sec_identity_create_SPAKE2PLUSV1_server_password_verifier(password_verifier);
    dispatch_data_t registration_record = sec_identity_create_SPAKE2PLUSV1_registration_record(password_verifier);

    return [[SEC_CONCRETE_CLASS_NAME(sec_identity) alloc] initWithSPAKE2PLUSV1Context:context clientIdentity:client_identity serverIdentity:server_identity clientPasswordVerifier:client_password_verifier serverPasswordVerifier:server_password_verifier registrationRecord:registration_record];
}

static size_t
dispatch_data_copyout(dispatch_data_t data, void *destination, size_t maxlen)
{
    __block size_t copied = 0;
    __block uint8_t *buffer = (uint8_t *)destination;
    if (data) {
        dispatch_data_apply(
            data, ^bool(__unused dispatch_data_t region, __unused size_t offset, const void *dbuffer, size_t size) {
                size_t consumed = MIN(maxlen - copied, size);
                if (consumed) {
                    memcpy(&buffer[copied], dbuffer, consumed);
                    copied += consumed;
                }
                return copied < maxlen;
            });
    }
    return copied;
}

static bool
dispatch_data_copyout_and_alloc(dispatch_data_t data, void **destination, size_t *len)
{
    *destination = NULL;
    *len = dispatch_data_get_size(data);
    if (*len > 0) {
        *destination = malloc(*len);
        if (*destination == NULL) {
            return false;
        }
        return dispatch_data_copyout(data, *destination, *len) == *len;
    } else {
        // If the length is 0, then there's nothing to copy out
        return true;
    }
}

static sec_identity_t
sec_identity_create_client_SPAKE2PLUSV1_identity_for_scrypt_default(dispatch_data_t context_data,
                                                                    dispatch_data_t client_identity_data,
                                                                    dispatch_data_t server_identity_data,
                                                                    dispatch_data_t password_data)
{
    if (password_data == nil) {
        return SEC_NIL_BAD_INPUT;
    }
    size_t client_identity_len = 0;
    if (client_identity_data != NULL) {
        client_identity_len = dispatch_data_get_size(client_identity_data);
    }
    if (client_identity_len > 0xFFFF) {
        return SEC_NIL_BAD_INPUT;
    }
    size_t server_identity_len = 0;
    if (server_identity_data != NULL) {
        server_identity_len = dispatch_data_get_size(server_identity_data);
    }
    if (server_identity_len > 0xFFFF) {
        return SEC_NIL_BAD_INPUT;
    }
    size_t password_len = dispatch_data_get_size(password_data);
    if (password_len > 0xFFFF) {
        return SEC_NIL_BAD_INPUT;
    }

    uint8_t *client_identity = NULL;
    if (!dispatch_data_copyout_and_alloc(client_identity_data, (void **)&client_identity, &client_identity_len)) {
        return SEC_NIL_BAD_INPUT;
    }
    uint8_t *server_identity = NULL;
    if (!dispatch_data_copyout_and_alloc(server_identity_data, (void **)&server_identity, &server_identity_len)) {
        free(client_identity);
        return SEC_NIL_BAD_INPUT;
    }
    uint8_t *password = NULL;
    if (!dispatch_data_copyout_and_alloc(password_data, (void **)&password, &password_len)) {
        free(client_identity);
        free(server_identity);
        return SEC_NIL_BAD_INPUT;
    }

    size_t scrypt_input_len = 8 + password_len + 8 + client_identity_len + 8 + server_identity_len;
    uint8_t *scrypt_input = (uint8_t *)malloc(scrypt_input_len);
    if (scrypt_input == NULL) {
        free(client_identity);
        free(server_identity);
        free(password);
        return SEC_NIL_OUT_OF_MEMORY;
    }

    // Encode the PBKDF input according to https://datatracker.ietf.org/doc/html/rfc9383#section-3.2
    size_t offset = 0;
    uint64_t length = (uint64_t) password_len;
    memcpy(scrypt_input + offset, (uint8_t *)&length, 8); offset += 8;
    memcpy(scrypt_input + offset, password, length); offset += length;

    length = (uint64_t) client_identity_len;
    memcpy(scrypt_input + offset, (uint8_t *)&length, 8); offset += 8;
    memcpy(scrypt_input + offset, client_identity, length); offset += length;

    length = (uint64_t) server_identity_len;
    memcpy(scrypt_input + offset, (uint8_t *)&length, 8); offset += 8;
    memcpy(scrypt_input + offset, server_identity, length); offset += length;

    if (offset != scrypt_input_len) {
        free(client_identity);
        free(server_identity);
        free(password);
        free(scrypt_input);
        return SEC_NIL_BAD_INPUT;
    }

    int64_t buffer_size = ccscrypt_storage_size(32768, 8, 1);
    uint8_t *buffer = (uint8_t *)malloc((size_t)buffer_size);
    if (buffer == NULL) {
        free(client_identity);
        free(server_identity);
        free(password);
        free(scrypt_input);
        return SEC_NIL_BAD_INPUT;
    }

    memset(buffer, 0, (size_t)buffer_size);

    uint8_t input_password_verifier[SEC_PROTOCOL_SPAKE2PLUSV1_INPUT_PASSWORD_VERIFIER_NBYTES];
    int result = ccscrypt(scrypt_input_len, scrypt_input, 0, NULL, buffer, 32768, 8, 1, sizeof(input_password_verifier), input_password_verifier);
    if (result != CCERR_OK) {
        free(client_identity);
        free(server_identity);
        free(password);
        free(scrypt_input);
        free(buffer);
        return SEC_NIL_BAD_INPUT;
    }

    dispatch_data_t input_password_verifier_data = dispatch_data_create(input_password_verifier, sizeof(input_password_verifier), NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    free(client_identity);
    free(server_identity);
    free(password);
    free(scrypt_input);
    free(buffer);

    return sec_identity_create_client_SPAKE2PLUSV1_identity_internal(context_data, client_identity_data, server_identity_data, input_password_verifier_data);
}

sec_identity_t
sec_identity_create_client_SPAKE2PLUSV1_identity(dispatch_data_t context,
                                                 dispatch_data_t client_identity,
                                                 dispatch_data_t server_identity,
                                                 dispatch_data_t password,
                                                 pake_pbkdf_params_t pbkdf_params)
{
    switch (pbkdf_params) {
        case PAKE_PBKDF_PARAMS_SCRYPT_DEFAULT:
            return sec_identity_create_client_SPAKE2PLUSV1_identity_for_scrypt_default(context, client_identity, server_identity, password);
        default:
            return SEC_NIL_BAD_INPUT;
    }
}

sec_identity_t
sec_identity_create_server_SPAKE2PLUSV1_identity(dispatch_data_t context,
                                                 dispatch_data_t client_identity,
                                                 dispatch_data_t server_identity,
                                                 dispatch_data_t server_password_verifier,
                                                 dispatch_data_t registration_record)
{
    if (server_password_verifier == nil || dispatch_data_get_size(server_password_verifier) != SEC_PROTOCOL_SPAKE2PLUSV1_SERVER_PASSWORD_VERIFIER_NBYTES) {
        return SEC_NIL_BAD_INPUT;
    }
    if (registration_record == nil || dispatch_data_get_size(registration_record) != SEC_PROTOCOL_SPAKE2PLUSV1_REGISTRATION_RECORD_NBYTES) {
        return SEC_NIL_BAD_INPUT;
    }
    return [[SEC_CONCRETE_CLASS_NAME(sec_identity) alloc] initWithSPAKE2PLUSV1Context:context clientIdentity:client_identity serverIdentity:server_identity clientPasswordVerifier:nil serverPasswordVerifier:server_password_verifier registrationRecord:registration_record];
}

sec_identity_type_t
sec_identity_copy_type(sec_identity_t identity)
{
    if (identity == NULL) {
        return SEC_PROTOCOL_IDENTITY_TYPE_INVALID;
    }
    return identity->type;
}

dispatch_data_t
sec_identity_create_SPAKE2PLUSV1_client_password_verifier(dispatch_data_t input_password_verifier)
{
    if (input_password_verifier == NULL) {
        return SEC_NULL_BAD_INPUT;
    }

    ccspake_const_cp_t cp = ccspake_cp_256_rfc();
    size_t expanded_element_len = ccspake_sizeof_w(cp) + 8;
    size_t buffer_len = dispatch_data_get_size(input_password_verifier);
    if (buffer_len != (2 * expanded_element_len)) {
        return SEC_NULL_BAD_INPUT;
    }

    uint8_t *buffer = (uint8_t *)malloc(buffer_len);
    if (buffer == NULL) {
        return SEC_NULL_OUT_OF_MEMORY;
    }

    size_t copied = dispatch_data_copyout(input_password_verifier, buffer, buffer_len);
    if (copied != buffer_len) {
        free(buffer);
        return SEC_NULL_OUT_OF_MEMORY;
    }

    uint8_t *w0 = buffer;
    uint8_t *w1 = buffer + expanded_element_len;
    size_t verifier_len = ccspake_sizeof_w(cp) * 2;
    uint8_t *verifier = (uint8_t *)malloc(verifier_len);
    if (verifier == NULL) {
        free(buffer);
        return SEC_NULL_OUT_OF_MEMORY;
    }

    // Reduce each expanded element to a scalar
#ifdef CCSPAKE_HAS_REDUCE_W_RFC9383
    if (ccspake_reduce_w_RFC9383(cp, expanded_element_len, w0, ccspake_sizeof_w(cp), verifier) != CCERR_OK) {
#else
    if (ccspake_reduce_w(cp, expanded_element_len, w0, ccspake_sizeof_w(cp), verifier) != CCERR_OK) {
#endif // CCSPAKE_HAS_REDUCE_W_RFC9383
        free(buffer);
        free(verifier);
        return SEC_NULL_OUT_OF_MEMORY;
    }
#ifdef CCSPAKE_HAS_REDUCE_W_RFC9383
    if (ccspake_reduce_w_RFC9383(cp, expanded_element_len, w1, ccspake_sizeof_w(cp), verifier + ccspake_sizeof_w(cp)) != CCERR_OK) {
#else
    if (ccspake_reduce_w(cp, expanded_element_len, w1, ccspake_sizeof_w(cp), verifier + ccspake_sizeof_w(cp)) != CCERR_OK) {
#endif // CCSPAKE_HAS_REDUCE_W_RFC9383
        free(buffer);
        free(verifier);
        return SEC_NULL_OUT_OF_MEMORY;
    }

    dispatch_data_t password_verifier = dispatch_data_create(verifier, verifier_len, NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    free(buffer);
    free(verifier);

    return password_verifier;
}

dispatch_data_t
sec_identity_create_SPAKE2PLUSV1_server_password_verifier(dispatch_data_t input_password_verifier)
{
    if (input_password_verifier == NULL) {
        return SEC_NULL_BAD_INPUT;
    }

    ccspake_const_cp_t cp = ccspake_cp_256_rfc();
    size_t expanded_element_len = ccspake_sizeof_w(cp) + 8;
    size_t buffer_len = dispatch_data_get_size(input_password_verifier);
    if (buffer_len != (2 * expanded_element_len)) {
        return SEC_NULL_BAD_INPUT;
    }

    uint8_t *buffer = (uint8_t *)malloc(buffer_len);
    if (buffer == NULL) {
        return SEC_NULL_OUT_OF_MEMORY;
    }

    size_t copied = dispatch_data_copyout(input_password_verifier, buffer, buffer_len);
    if (copied != buffer_len) {
        free(buffer);
        return SEC_NULL_OUT_OF_MEMORY;
    }

    uint8_t *w0 = buffer;
    size_t verifier_len = ccspake_sizeof_w(cp);
    uint8_t *verifier = (uint8_t *)malloc(verifier_len);
    if (verifier == NULL) {
        free(buffer);
        return SEC_NULL_OUT_OF_MEMORY;
    }

    // Reduce w0 to a scalar
#ifdef CCSPAKE_HAS_REDUCE_W_RFC9383
    if (ccspake_reduce_w_RFC9383(cp, expanded_element_len, w0, ccspake_sizeof_w(cp), verifier) != CCERR_OK) {
#else
    if (ccspake_reduce_w(cp, expanded_element_len, w0, ccspake_sizeof_w(cp), verifier) != CCERR_OK) {
#endif // CCSPAKE_HAS_REDUCE_W_RFC9383
        free(buffer);
        free(verifier);
        return SEC_NULL_OUT_OF_MEMORY;
    }

    dispatch_data_t password_verifier = dispatch_data_create(verifier, verifier_len, NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    free(buffer);
    free(verifier);

    return password_verifier;
}

dispatch_data_t
sec_identity_create_SPAKE2PLUSV1_registration_record(dispatch_data_t password_verifier)
{
    if (password_verifier == NULL) {
        return SEC_NULL_BAD_INPUT;
    }

    ccspake_const_cp_t cp = ccspake_cp_256_rfc();
    size_t expanded_element_len = ccspake_sizeof_w(cp) + 8;
    size_t buffer_len = dispatch_data_get_size(password_verifier);
    if (buffer_len != (2 * expanded_element_len)) {
        return SEC_NULL_BAD_INPUT;
    }

    uint8_t *buffer = (uint8_t *)malloc(buffer_len);
    if (buffer == NULL) {
        return SEC_NULL_OUT_OF_MEMORY;
    }

    size_t copied = dispatch_data_copyout(password_verifier, buffer, buffer_len);
    if (copied != buffer_len) {
        free(buffer);
        return SEC_NULL_OUT_OF_MEMORY;
    }

    // The format of the input verifier is w0 || w1, and we just need w1
    uint8_t *w1 = buffer + expanded_element_len;
    struct ccrng_state *rng = ccrng(NULL);
    if (rng == NULL) {
        free(buffer);
        return SEC_NULL_OUT_OF_MEMORY;
    }

    size_t L_len = ccspake_sizeof_point(cp);
    uint8_t *L = (uint8_t *)malloc(L_len);
    if (L == NULL) {
        free(buffer);
        return SEC_NULL_OUT_OF_MEMORY;
    }

#ifdef CCSPAKE_HAS_REDUCE_W_RFC9383
    if (ccspake_reduce_w_RFC9383(cp, expanded_element_len, w1, ccspake_sizeof_w(cp), w1) != CCERR_OK) {
#else
    if (ccspake_reduce_w(cp, expanded_element_len, w1, ccspake_sizeof_w(cp), w1) != CCERR_OK) {
#endif // CCSPAKE_HAS_REDUCE_W_RFC9383
        free(buffer);
        free(L);
        return SEC_NULL_OUT_OF_MEMORY;
    }

    int result = ccspake_generate_L(cp, ccspake_sizeof_w(cp), w1, L_len, L, rng);
    if (result != CCERR_OK) {
        free(buffer);
        free(L);
        return SEC_NULL_OUT_OF_MEMORY;
    }

    dispatch_data_t record = dispatch_data_create(L, L_len, NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    free(buffer);
    free(L);

    return record;
}

SecIdentityRef
sec_identity_copy_ref(sec_identity_t object)
{
    if (object == NULL) {
        return SEC_NULL_BAD_INPUT;
    }
    if (object->identity != NULL) {
        return __DECONST(SecIdentityRef, CFRetain(object->identity));
    }
    return SEC_NULL_BAD_INPUT;
}

CFArrayRef
sec_identity_copy_certificates_ref(sec_identity_t object)
{
    if (object == NULL) {
        return SEC_NULL_BAD_INPUT;
    }
    if (object->certs != NULL) {
        return __DECONST(CFArrayRef, CFRetain(object->certs));
    }
    return SEC_NULL_BAD_INPUT;
}

bool
sec_identity_access_certificates(sec_identity_t identity,
                                 void (^handler)(sec_certificate_t certificate))
{
    if (identity == NULL) {
        return false;
    }
    if (identity->certs != NULL) {
        CFArrayForEach(identity->certs, ^(const void *value) {
            SecCertificateRef certificate_ref = (SecCertificateRef)value;
            if (certificate_ref != NULL) {
                sec_certificate_t certificate = sec_certificate_create(certificate_ref);
                handler(certificate);
            }
        });
        return true;
    }
    return false;
}

bool
sec_identity_has_certificates(sec_identity_t identity)
{
    if (identity == NULL) {
        return false;
    }
    return identity->certs != NULL;
}

sec_protocol_private_key_sign_t
sec_identity_copy_private_key_sign_block(sec_identity_t object)
{
    if (object == NULL) {
        return SEC_NULL_BAD_INPUT;
    }
    if (object->sign_block != NULL) {
        return object->sign_block;
    }
    return SEC_NIL_BAD_INPUT;
}

sec_protocol_private_key_decrypt_t
sec_identity_copy_private_key_decrypt_block(sec_identity_t object)
{
    if (object == NULL) {
        return SEC_NULL_BAD_INPUT;
    }
    if (object->decrypt_block != NULL) {
        return object->decrypt_block;
    }
    return SEC_NIL_BAD_INPUT;
}

dispatch_queue_t
sec_identity_copy_private_key_queue(sec_identity_t object)
{
    if (object == NULL) {
        return SEC_NULL_BAD_INPUT;
    }
    if (object->operation_queue != nil) {
        return object->operation_queue;
    }
    return SEC_NIL_BAD_INPUT;
}

dispatch_data_t
sec_identity_copy_SPAKE2PLUSV1_context(sec_identity_t identity)
{
    if (identity == NULL) {
        return SEC_NULL_BAD_INPUT;
    }
    return identity->spake2_context;
}

dispatch_data_t
sec_identity_copy_SPAKE2PLUSV1_client_identity(sec_identity_t identity)
{
    if (identity == NULL) {
        return SEC_NULL_BAD_INPUT;
    }
    return identity->client_identity;
}

dispatch_data_t
sec_identity_copy_SPAKE2PLUSV1_server_identity(sec_identity_t identity)
{
    if (identity == NULL) {
        return SEC_NULL_BAD_INPUT;
    }
    return identity->server_identity;
}

dispatch_data_t
sec_identity_copy_SPAKE2PLUSV1_server_password_verifier(sec_identity_t identity)
{
    if (identity == NULL) {
        return SEC_NULL_BAD_INPUT;
    }
    return identity->server_password_verifier;
}

dispatch_data_t
sec_identity_copy_SPAKE2PLUSV1_client_password_verifier(sec_identity_t identity)
{
    if (identity == NULL) {
        return SEC_NULL_BAD_INPUT;
    }
    return identity->client_password_verifier;
}

dispatch_data_t
sec_identity_copy_SPAKE2PLUSV1_registration_record(sec_identity_t identity)
{
    if (identity == NULL) {
        return SEC_NULL_BAD_INPUT;
    }
    return identity->registration_record;
}

@end

SEC_OBJECT_IMPL_INTERNAL_OBJC(sec_certificate,
{
    SecCertificateRef certificate;
});

@implementation SEC_CONCRETE_CLASS_NAME(sec_certificate)

- (instancetype)initWithCertificate:(SecCertificateRef)_certificate
{
    if (_certificate == NULL) {
        return SEC_NIL_BAD_INPUT;
    }

    if ((self = [super init])) {
        self->certificate = __DECONST(SecCertificateRef, CFRetainSafe(_certificate));
    } else {
        return SEC_NIL_OUT_OF_MEMORY;
    }
    return self;
}

- (void)dealloc
{
    if (self->certificate != NULL) {
        CFRelease(self->certificate);
        self->certificate = NULL;
    }
}

sec_certificate_t
sec_certificate_create(SecCertificateRef certificate)
{
    return [[SEC_CONCRETE_CLASS_NAME(sec_certificate) alloc] initWithCertificate:certificate];
}

SecCertificateRef
sec_certificate_copy_ref(sec_certificate_t object)
{
    if (object == NULL) {
        return SEC_NULL_BAD_INPUT;
    }
    if (object->certificate != NULL) {
        return __DECONST(SecCertificateRef, CFRetain(object->certificate));
    }
    return SEC_NULL_BAD_INPUT;
}

@end

SEC_OBJECT_IMPL_INTERNAL_OBJC(sec_trust,
{
    SecTrustRef trust;
});

@implementation SEC_CONCRETE_CLASS_NAME(sec_trust)

- (instancetype)initWithTrust:(SecTrustRef)_trust
{
    if (_trust == NULL) {
        return SEC_NIL_BAD_INPUT;
    }

    if ((self = [super init])) {
        self->trust = __DECONST(SecTrustRef, CFRetainSafe(_trust));
    } else {
        return SEC_NIL_OUT_OF_MEMORY;
    }
    return self;
}

- (void)dealloc
{
    if (self->trust != NULL) {
        CFRelease(self->trust);
        self->trust = NULL;
    }
}

sec_trust_t
sec_trust_create(SecTrustRef trust)
{
    return [[SEC_CONCRETE_CLASS_NAME(sec_trust) alloc] initWithTrust:trust];
}

SecTrustRef
sec_trust_copy_ref(sec_trust_t object)
{
    if (object == NULL) {
        return SEC_NULL_BAD_INPUT;
    }
    if (object->trust != NULL) {
        return __DECONST(SecTrustRef, CFRetain(object->trust));
    }
    return SEC_NULL_BAD_INPUT;
}

@end

static bool
_is_apple_bundle_exception_enabled(void)
{
    //sudo defaults write /Library/Preferences/com.apple.security EnableAppTransportSecurityAppleBundleException -bool YES
    NSNumber *enabled = CFBridgingRelease(CFPreferencesCopyValue(CFSTR("EnableAppTransportSecurityAppleBundleException"),
                                                                 CFSTR("com.apple.security"),
                                                                 kCFPreferencesAnyUser, kCFPreferencesAnyHost));
    return ([enabled isKindOfClass:[NSNumber class]] && [enabled boolValue] == YES);
}

static bool
_is_apple_bundle(void)
{
    static dispatch_once_t onceToken;
    static bool result = false;
    dispatch_once(&onceToken, ^{
        CFBundleRef bundle = CFBundleGetMainBundle();
        CFStringRef bundleID = CFBundleGetIdentifier(bundle);
        result = !bundleID || CFStringHasPrefix(bundleID, CFSTR("com.apple."));
    });
    return result;
}

SEC_OBJECT_IMPL_INTERNAL_OBJC(sec_protocol_configuration_builder,
{
@package
    CFDictionaryRef dictionary;
    bool is_apple;
});

@implementation SEC_CONCRETE_CLASS_NAME(sec_protocol_configuration_builder)

- (id)init
{
    if (self = [super init]) {
        CFBundleRef bundle = CFBundleGetMainBundle();
        if (bundle != NULL) {
            CFDictionaryRef info = CFBundleGetInfoDictionary(bundle);
            if (info != NULL) {
                CFTypeRef rawATS = CFDictionaryGetValue(info, CFSTR(kATSInfoKey));
                self->dictionary = (CFDictionaryRef)rawATS;
                CFRetainSafe(self->dictionary);
                if (_is_apple_bundle_exception_enabled()) {
                    self->is_apple = _is_apple_bundle();
                } else {
                    self->is_apple = client_is_WebKit();
                }
            }
        }
    }
    return self;
}

- (id)initWithDictionary:(CFDictionaryRef)dict
         andInternalFlag:(bool)flag
{
    if ((self = [super init])) {
        self->dictionary = dict;
        CFRetainSafe(dict);
        self->is_apple = flag;
    }
    return self;
}

@end

sec_protocol_configuration_builder_t
sec_protocol_configuration_builder_copy_default(void)
{
    return [[SEC_CONCRETE_CLASS_NAME(sec_protocol_configuration_builder) alloc] init];
}

sec_protocol_configuration_builder_t
sec_protocol_configuration_builder_create(__nullable CFDictionaryRef dictionary, bool is_apple)
{
    return [[SEC_CONCRETE_CLASS_NAME(sec_protocol_configuration_builder) alloc] initWithDictionary:dictionary andInternalFlag:is_apple];
}

CFDictionaryRef
sec_protocol_configuration_builder_get_ats_dictionary(sec_protocol_configuration_builder_t builder)
{
    return builder->dictionary;
}

bool
sec_protocol_configuration_builder_get_is_apple_bundle(sec_protocol_configuration_builder_t builder)
{
    return builder->is_apple;
}

SEC_OBJECT_IMPL_INTERNAL_OBJC(sec_protocol_configuration,
{
    xpc_object_t dictionary;
});

@implementation SEC_CONCRETE_CLASS_NAME(sec_protocol_configuration)

- (id)init {
    if ((self = [super init])) {
        self->dictionary = xpc_dictionary_create(NULL, NULL, 0);
    }
    return self;
}

static sec_protocol_configuration_t
sec_protocol_configuration_create(void)
{
    return [[SEC_CONCRETE_CLASS_NAME(sec_protocol_configuration) alloc] init];
}

sec_protocol_configuration_t
sec_protocol_configuration_create_with_builder(sec_protocol_configuration_builder_t builder)
{
    sec_protocol_configuration_t configuration = sec_protocol_configuration_create();
    if (configuration) {
        if (builder->is_apple) {
            os_log_debug(OS_LOG_DEFAULT, "Building default configuration for first-party bundle");
            sec_protocol_configuration_populate_insecure_defaults(configuration);
        } else {
            os_log_debug(OS_LOG_DEFAULT, "Building default configuration for third-party bundle");
            sec_protocol_configuration_populate_secure_defaults(configuration);
        }

        sec_protocol_configuration_register_builtin_exceptions(configuration);
        CFDictionaryRef dictionary = builder->dictionary;
        if (dictionary) {
            os_log_debug(OS_LOG_DEFAULT, "Setting configuration overrides based on AppTransportSecurity exceptions");
            sec_protocol_configuration_set_ats_overrides(configuration, dictionary);
        } else {
            os_log_debug(OS_LOG_DEFAULT, "Using default configuration settings");
        }
    } else {
        os_log_error(OS_LOG_DEFAULT, "sec_protocol_configuration_create failed");
    }
    return configuration;
}

xpc_object_t
sec_protocol_configuration_get_map(sec_protocol_configuration_t configuration)
{
    return configuration->dictionary;
}

@end
