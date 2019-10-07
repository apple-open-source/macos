//
//  SecProtocolTypes.m
//  Security
//

#import "utilities/SecCFRelease.h"
#import "utilities/SecCFWrappers.h"

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

#ifndef SEC_ANALYZER_HIDE_DEADSTORE
#  ifdef __clang_analyzer__
#    define SEC_ANALYZER_HIDE_DEADSTORE(var) do { if (var) {} } while (0)
#  else // __clang_analyzer__
#    define SEC_ANALYZER_HIDE_DEADSTORE(var) do {} while (0)
#  endif // __clang_analyzer__
#endif // SEC_ANALYZER_HIDE_DEADSTORE

SEC_OBJECT_IMPL_INTERNAL_OBJC(sec_array,
{
    xpc_object_t xpc_array;
});

@implementation SEC_CONCRETE_CLASS_NAME(sec_array)

- (instancetype)init
{
    self = [super init];
    if (self == nil) {
        return SEC_NIL_OUT_OF_MEMORY;
    }
    self->xpc_array = xpc_array_create(NULL, 0);
    return self;
}

- (void)dealloc
{
    if (self->xpc_array != nil) {
        xpc_array_apply(self->xpc_array, ^bool(size_t index, __unused xpc_object_t value) {
            void *pointer = xpc_array_get_pointer(self->xpc_array, index);
            sec_object_t object = (sec_object_t)CFBridgingRelease(pointer);
            SEC_ANALYZER_HIDE_DEADSTORE(object);
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
});

@implementation SEC_CONCRETE_CLASS_NAME(sec_identity)

- (instancetype)initWithIdentity:(SecIdentityRef)_identity
{
    if (_identity == NULL) {
        return SEC_NIL_BAD_INPUT;
    }

    self = [super init];
    if (self == nil) {
        return SEC_NIL_OUT_OF_MEMORY;
    }
    self->identity = __DECONST(SecIdentityRef, CFRetainSafe(_identity));
    return self;
}

- (instancetype)initWithIdentityAndCertificates:(SecIdentityRef)_identity certificates:(CFArrayRef)certificates
{
    if (_identity == NULL) {
        return SEC_NIL_BAD_INPUT;
    }
    
    self = [super init];
    if (self == nil) {
        return SEC_NIL_OUT_OF_MEMORY;
    }
    self->identity = __DECONST(SecIdentityRef, CFRetainSafe(_identity));
    self->certs = __DECONST(CFArrayRef, CFRetainSafe(certificates));
    
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

    self = [super init];
    if (self == nil) {
        return SEC_NIL_OUT_OF_MEMORY;
    }

    self->certs = __DECONST(CFArrayRef, CFRetainSafe(certificates));
    self->sign_block = sign;
    self->decrypt_block = decrypt;
    self->operation_queue = queue;

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

    self = [super init];
    if (self == nil) {
        return SEC_NIL_OUT_OF_MEMORY;
    }
    self->certificate = __DECONST(SecCertificateRef, CFRetainSafe(_certificate));
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

    self = [super init];
    if (self == nil) {
        return SEC_NIL_OUT_OF_MEMORY;
    }
    self->trust = __DECONST(SecTrustRef, CFRetainSafe(_trust));
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

- (id)init {
    self = [super init];
    if (self) {
        CFBundleRef bundle = CFBundleGetMainBundle();
        if (bundle != NULL) {
            CFTypeRef rawATS = CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR(kATSInfoKey));
            self->dictionary = (CFDictionaryRef)rawATS;
            CFRetainSafe(self->dictionary);
            self->is_apple = _is_apple_bundle();
        }
    }
    return self;
}

- (id)initWithDictionary:(CFDictionaryRef)dict andInternalFlag:(bool)flag {
    self = [super init];
    if (self) {
        self->dictionary = dict;
        CFRetainSafe(dict);
        self->is_apple = flag;
    }
    return self;
}

@end

sec_protocol_configuration_builder_t
sec_protocol_configuration_builder_copy_default()
{
    return [[SEC_CONCRETE_CLASS_NAME(sec_protocol_configuration_builder) alloc] init];
}

sec_protocol_configuration_builder_t
sec_protocol_configuration_builder_create(CFDictionaryRef dictionary, bool is_apple)
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
    self = [super init];
    if (self) {
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
