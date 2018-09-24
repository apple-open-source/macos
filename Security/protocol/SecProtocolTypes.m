//
//  SecProtocolTypes.m
//  Security
//

#include "utilities/SecCFRelease.h"

#define OS_OBJECT_HAVE_OBJC_SUPPORT 1

#define SEC_NULL_BAD_INPUT ((void *_Nonnull)NULL)
#define SEC_NULL_OUT_OF_MEMORY SEC_NULL_BAD_INPUT

#define SEC_NIL_BAD_INPUT ((void *_Nonnull)nil)
#define SEC_NIL_OUT_OF_MEMORY SEC_NIL_BAD_INPUT

#define SEC_CONCRETE_CLASS_NAME(external_type) SecConcrete_##external_type
#define SEC_CONCRETE_PREFIX_STR "SecConcrete_"

#define SEC_OBJECT_DECL_INTERNAL_OBJC(external_type)													\
	@class SEC_CONCRETE_CLASS_NAME(external_type);														\
	typedef SEC_CONCRETE_CLASS_NAME(external_type) *external_type##_t

#define SEC_OBJECT_IMPL_INTERNAL_OBJC_WITH_PROTOCOL_AND_VISBILITY(external_type, _protocol, visibility, ...)	\
	@protocol OS_OBJECT_CLASS(external_type) <_protocol>														\
	@end																										\
	visibility																									\
	@interface SEC_CONCRETE_CLASS_NAME(external_type) : NSObject<OS_OBJECT_CLASS(external_type)>				\
		_Pragma("clang diagnostic push")																	\
		_Pragma("clang diagnostic ignored \"-Wobjc-interface-ivars\"")										\
			__VA_ARGS__																						\
		_Pragma("clang diagnostic pop")																		\
	@end																									\
	typedef int _useless_typedef_oio_##external_type

#define SEC_OBJECT_IMPL_INTERNAL_OBJC_WITH_PROTOCOL(external_type, _protocol, ...)						\
	SEC_OBJECT_IMPL_INTERNAL_OBJC_WITH_PROTOCOL_AND_VISBILITY(external_type, _protocol, ,__VA_ARGS__)

#define SEC_OBJECT_IMPL_INTERNAL_OBJC(external_type, ...)												\
	SEC_OBJECT_IMPL_INTERNAL_OBJC_WITH_PROTOCOL(external_type, NSObject, ##__VA_ARGS__)

#define SEC_OBJECT_IMPL_INTERNAL_OBJC_WITH_VISIBILITY(external_type, visibility, ...)					\
	SEC_OBJECT_IMPL_INTERNAL_OBJC_WITH_PROTOCOL_AND_VISBILITY(external_type, NSObject, visibility, ##__VA_ARGS__)

#define SEC_OBJECT_IMPL 1

SEC_OBJECT_DECL_INTERNAL_OBJC(sec_array);
SEC_OBJECT_DECL_INTERNAL_OBJC(sec_identity);
SEC_OBJECT_DECL_INTERNAL_OBJC(sec_trust);
SEC_OBJECT_DECL_INTERNAL_OBJC(sec_certificate);
SEC_OBJECT_DECL_INTERNAL_OBJC(sec_tls_extension);
SEC_OBJECT_DECL_INTERNAL_OBJC(sec_object);
SEC_OBJECT_DECL_INTERNAL_OBJC(sec_protocol_options);
SEC_OBJECT_DECL_INTERNAL_OBJC(sec_protocol_metadata);

#import <Security/SecProtocolPriv.h>

#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>
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

SEC_OBJECT_IMPL_INTERNAL_OBJC(sec_tls_extension,
{
	uint16_t type;
    sec_protocol_tls_ext_add_callback adder;
    sec_protocol_tls_ext_parse_callback parser;
    sec_protocol_tls_ext_free_callback freer;
});

@implementation SEC_CONCRETE_CLASS_NAME(sec_tls_extension)

- (instancetype)initWithCallbacks:(uint16_t)ext_type
                            adder:(sec_protocol_tls_ext_add_callback)add_block
                           parser:(sec_protocol_tls_ext_parse_callback)parse_block
                            freer:(sec_protocol_tls_ext_free_callback)free_block
{
    if (add_block == nil) {
        return SEC_NIL_BAD_INPUT;
    }
    if (parse_block == nil) {
        return SEC_NIL_BAD_INPUT;
    }
    if (free_block == nil) {
        return SEC_NIL_BAD_INPUT;
    }

    self = [super init];
    if (self == nil) {
        return SEC_NIL_OUT_OF_MEMORY;
    }

    self->type = ext_type;
    self->adder = add_block;
    self->parser = parse_block;
    self->freer = free_block;
    return self;
}

uint16_t
sec_tls_extension_get_type(sec_tls_extension_t extension)
{
	if (extension == NULL) {
		return 0;
	}

	return extension->type;
}

sec_protocol_tls_ext_add_callback
sec_tls_extension_copy_add_block(sec_tls_extension_t extension)
{
	if (extension == NULL) {
		return SEC_NULL_BAD_INPUT;
	}

	return extension->adder;
}

sec_protocol_tls_ext_parse_callback
sec_tls_extension_copy_parse_block(sec_tls_extension_t extension)
{
	if (extension == NULL) {
		return SEC_NULL_BAD_INPUT;
	}

	return extension->parser;
}

sec_protocol_tls_ext_free_callback
sec_tls_extension_copy_free_block(sec_tls_extension_t extension)
{
	if (extension == NULL) {
		return SEC_NULL_BAD_INPUT;
	}

	return extension->freer;
}

sec_tls_extension_t
sec_tls_extension_create(uint16_t type, sec_protocol_tls_ext_add_callback adder, sec_protocol_tls_ext_parse_callback parser, sec_protocol_tls_ext_free_callback freer)
{
    return [[SEC_CONCRETE_CLASS_NAME(sec_tls_extension) alloc] initWithCallbacks:type adder:adder parser:parser freer:freer];
}

@end

