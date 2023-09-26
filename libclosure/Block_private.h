/*
 * Block_private.h
 *
 * SPI for Blocks
 *
 * Copyright (c) 2008-2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 *
 */

#ifndef _BLOCK_PRIVATE_H_
#define _BLOCK_PRIVATE_H_

#include <TargetConditionals.h>

#include <Availability.h>
#include <AvailabilityMacros.h>

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include <Block.h>

#if __has_include(<ptrauth.h>)
#include <ptrauth.h>
#endif

#ifndef __ptrauth_objc_isa_pointer
#define __ptrauth_objc_isa_pointer
#endif

#if __has_feature(ptrauth_calls) &&  __cplusplus < 201103L

// C ptrauth or old C++ ptrauth

#define _Block_set_function_pointer(field, value)                       \
    ((value)                                                            \
     ? ((field) =                                                       \
        (__typeof__(field))                                             \
        ptrauth_auth_and_resign((void*)(value),                         \
                                ptrauth_key_function_pointer, 0,        \
                                ptrauth_key_block_function, &(field)))  \
     : ((field) = 0))

#define _Block_get_function_pointer(field)                              \
    ((field)                                                            \
     ? (__typeof__(field))                                              \
       ptrauth_auth_function((void*)(field),                            \
                             ptrauth_key_block_function, &(field))      \
     : (__typeof__(field))0)

#else

// C++11 ptrauth or no ptrauth

#define _Block_set_function_pointer(field, value)       \
    (field) = (value)

#define _Block_get_function_pointer(field)      \
    (field)

#endif

#define BLOCK_HIDDEN __attribute__((__visibility__("hidden")))

struct Block_byref {
    void * __ptrauth_objc_isa_pointer isa;
    struct Block_byref *forwarding;
    volatile int32_t flags; // contains ref count
    uint32_t size;
};

#if __has_feature(ptrauth_calls)  &&  __cplusplus >= 201103L

// StorageSignedFunctionPointer<Key, Fn> stores a function pointer of type
// Fn but signed with the given ptrauth key and with the address of its
// storage as extra data.
// Function pointers inside block objects are signed this way.
template <typename Fn, ptrauth_key Key>
class StorageSignedFunctionPointer {
    uintptr_t bits;

 public:

    // Authenticate function pointer fn as a C function pointer.
    // Re-sign it with our key and the storage address as extra data.
    // DOES NOT actually write to our storage.
    uintptr_t prepareWrite(Fn fn) const
    {
        if (fn == nullptr) {
            return 0;
        } else {
            return (uintptr_t)
                ptrauth_auth_and_resign(fn, ptrauth_key_function_pointer,
                                        ptrauth_function_pointer_type_discriminator(Fn),
                                        Key, &bits);
        }
    }

    // Authenticate otherBits at otherStorage.
    // Re-sign it with our storage address.
    // DOES NOT actually write to our storage.
    uintptr_t prepareWrite(const StorageSignedFunctionPointer& other) const
    {
        if (other.bits == 0) {
            return 0;
        } else {
            return (uintptr_t)
                ptrauth_auth_and_resign((void*)other.bits, Key, &other.bits,
                                        Key, &bits);
        }
    }

    // Authenticate ptr as if it were stored at our storage address.
    // Re-sign it as a C function pointer.
    // DOES NOT actually read from our storage.
    Fn completeReadFn(uintptr_t ptr) const
    {
        if (ptr == 0) {
            return nullptr;
        } else {
            return (Fn)ptrauth_auth_function((void *)ptr, Key, &bits);
        }
    }

    // Authenticate ptr as if it were at our storage address.
    // Return it as a dereferenceable pointer.
    // DOES NOT actually read from our storage.
    void* completeReadRaw(uintptr_t ptr) const
    {
        if (ptr == 0) {
            return nullptr;
        } else {
            return ptrauth_auth_data((void*)ptr, Key, &bits);
        }
    }

    StorageSignedFunctionPointer() { }

    StorageSignedFunctionPointer(Fn value)
        : bits(prepareWrite(value)) { }

    StorageSignedFunctionPointer(const StorageSignedFunctionPointer& value)
        : bits(prepareWrite(value)) { }

    StorageSignedFunctionPointer&
    operator = (Fn rhs) {
        bits = prepareWrite(rhs);
        return *this;
    }

    StorageSignedFunctionPointer&
    operator = (const StorageSignedFunctionPointer& rhs) {
        bits = prepareWrite(rhs);
        return *this;
    }

    operator Fn () const {
        return completeReadFn(bits);
    }

    explicit operator void* () const {
        return completeReadRaw(bits);
    }

    explicit operator bool () const {
        return completeReadRaw(bits) != nullptr;
    }
};

using BlockCopyFunction = StorageSignedFunctionPointer
    <void(*)(void *, const void *),
     ptrauth_key_block_function>;

using BlockDisposeFunction = StorageSignedFunctionPointer
    <void(*)(const void *),
     ptrauth_key_block_function>;

using BlockInvokeFunction = StorageSignedFunctionPointer
    <void(*)(void *, ...),
     ptrauth_key_block_function>;

using BlockByrefKeepFunction = StorageSignedFunctionPointer
    <void(*)(struct Block_byref *, struct Block_byref *),
     ptrauth_key_block_function>;

using BlockByrefDestroyFunction = StorageSignedFunctionPointer
    <void(*)(struct Block_byref *),
     ptrauth_key_block_function>;

// c++11 and ptrauth_calls
#elif !__has_feature(ptrauth_calls)
// not ptrauth_calls

typedef void(*BlockCopyFunction)(void *, const void *);
typedef void(*BlockDisposeFunction)(const void *);
typedef void(*BlockInvokeFunction)(void *, ...);
typedef void(*BlockByrefKeepFunction)(struct Block_byref*, struct Block_byref*);
typedef void(*BlockByrefDestroyFunction)(struct Block_byref *);

#else
// ptrauth_calls but not c++11

typedef uintptr_t BlockCopyFunction;
typedef uintptr_t BlockDisposeFunction;
typedef uintptr_t BlockInvokeFunction;
typedef uintptr_t BlockByrefKeepFunction;
typedef uintptr_t BlockByrefDestroyFunction;

#endif

#if __has_feature(ptrauth_calls)
#define _Block_get_relative_function_pointer(field, type)                 \
    ((type)ptrauth_sign_unauthenticated(                                  \
            (void *)((uintptr_t)(intptr_t)(field) + (uintptr_t)&(field)), \
            ptrauth_key_function_pointer, 0))
#else
#define _Block_get_relative_function_pointer(field, type)                   \
    ((type)((uintptr_t)(intptr_t)(field) + (uintptr_t)&(field)))
#endif

#define _Block_descriptor_ptrauth_discriminator 0xC0BB

// Values for Block_layout->flags to describe block objects
enum {
    BLOCK_DEALLOCATING =      (0x0001),  // runtime
    BLOCK_REFCOUNT_MASK =     (0xfffe),  // runtime

    BLOCK_INLINE_LAYOUT_STRING = (1 << 21), // compiler

#if BLOCK_SMALL_DESCRIPTOR_SUPPORTED
    // Note: small block descriptors are not currently supported anywhere.
    // Don't enable this without security review; see rdar://91727169
    BLOCK_SMALL_DESCRIPTOR =  (1 << 22), // compiler
#endif

    BLOCK_IS_NOESCAPE =       (1 << 23), // compiler
    BLOCK_NEEDS_FREE =        (1 << 24), // runtime
    BLOCK_HAS_COPY_DISPOSE =  (1 << 25), // compiler
    BLOCK_HAS_CTOR =          (1 << 26), // compiler: helpers have C++ code
    BLOCK_IS_GC =             (1 << 27), // runtime
    BLOCK_IS_GLOBAL =         (1 << 28), // compiler
    BLOCK_USE_STRET =         (1 << 29), // compiler: undefined if !BLOCK_HAS_SIGNATURE
    BLOCK_HAS_SIGNATURE  =    (1 << 30), // compiler
    BLOCK_HAS_EXTENDED_LAYOUT=(1 << 31)  // compiler
};

/// Large and small block descriptors have the following layout:
///
/// struct Large_block_descriptor {
///   // inline or out-of-line generic helper info.
///   uintptr_t generic_helper_info;
///   uintptr_t reserved; // Reserved or in-descriptor flag field.
///   uintptr_t size;  // size of Block_layout including the captured variables.
///   void *copy_helper_func;  // optional custom copy helper.
///   void *destroy_helper_func;  // optional custom copy helper.
///   void *signature; // pointer to the signature string.
///   void *extended_block_layout; // inline or out-of-line layout string.
/// };
///
/// struct Small_block_descriptor {
///   // inline or out-of-line generic helper info.
///   int32_t generic_helper_info;
///   uint32_t in_descriptor_flags;
///
///   // size of Block_layout including the captured variables.
///   uint32_t size : sizeof(uint32_t) * 8 - 1;
///
///   // A flag indicating the presence of in-descriptor flags.
///   uint32_t has_in_descriptor_flags : 1;
///
///   // the following fields contain relative offsets to pointers.
///   int32_t signature; // pointer to the signature string.
///   int32_t extended_block_layout; //  inline or out-of-line layout string.
///   int32_t copy_helper_func;  // optional custom copy helper.
///   int32_t destroy_helper_func; // optional custom destroy helper.
/// }
///
/// The in-descriptor flag field contains an exact copy of field `flags` of
/// `Block_layout` except that bit 22 (BLOCK_SMALL_DESCRIPTOR) is cleared out.
/// Bit 15_14 are used for generic block helper flags.
///
/// The address points of Large_block_descriptor and Small_block_descriptor are
/// fields `reserved` and `size` respectively.

#define BLOCK_DESCRIPTOR_1 1
struct Block_descriptor_1 {
    uintptr_t reserved;
    uintptr_t size;
};

#define BLOCK_DESCRIPTOR_2 1
struct Block_descriptor_2 {
    // requires BLOCK_HAS_COPY_DISPOSE
    BlockCopyFunction copy;
    BlockDisposeFunction dispose;
};

#define BLOCK_DESCRIPTOR_3 1
struct Block_descriptor_3 {
    // requires BLOCK_HAS_SIGNATURE
    const char *signature;
    const char *layout;     // contents depend on BLOCK_HAS_EXTENDED_LAYOUT
};

struct Block_descriptor_small {
    uint32_t size : sizeof(uint32_t) * 8 - 1;
    uint32_t has_in_descriptor_flags : 1;

    int32_t signature;
    int32_t layout;

    /* copy & dispose are optional, only access them if
       Block_layout->flags & BLOCK_HAS_COPY_DIPOSE */
    int32_t copy;
    int32_t dispose;
};

struct Block_layout {
    void * __ptrauth_objc_isa_pointer isa;
    volatile int32_t flags; // contains ref count
    int32_t reserved;
    BlockInvokeFunction invoke;
    struct Block_descriptor_1 *descriptor;
    // imported variables
};


// Values for Block_byref->flags to describe __block variables
enum {
    // Byref refcount must use the same bits as Block_layout's refcount.
    // BLOCK_DEALLOCATING =      (0x0001),  // runtime
    // BLOCK_REFCOUNT_MASK =     (0xfffe),  // runtime

    BLOCK_BYREF_LAYOUT_MASK =       (0xf << 28), // compiler
    BLOCK_BYREF_LAYOUT_EXTENDED =   (  1 << 28), // compiler
    BLOCK_BYREF_LAYOUT_NON_OBJECT = (  2 << 28), // compiler
    BLOCK_BYREF_LAYOUT_STRONG =     (  3 << 28), // compiler
    BLOCK_BYREF_LAYOUT_WEAK =       (  4 << 28), // compiler
    BLOCK_BYREF_LAYOUT_UNRETAINED = (  5 << 28), // compiler

    BLOCK_BYREF_IS_GC =             (  1 << 27), // runtime

    BLOCK_BYREF_HAS_COPY_DISPOSE =  (  1 << 25), // compiler
    BLOCK_BYREF_NEEDS_FREE =        (  1 << 24), // runtime
};

#define BLOCK_BYREF_LAYOUT(byref)  ((byref)->flags & BLOCK_BYREF_LAYOUT_MASK)

struct Block_byref_2 {
    // requires BLOCK_BYREF_HAS_COPY_DISPOSE
    BlockByrefKeepFunction byref_keep;
    BlockByrefDestroyFunction byref_destroy;
};

struct Block_byref_3 {
    // requires BLOCK_BYREF_LAYOUT_EXTENDED
    const char *layout;
};


// Extended layout encoding.

// Values for Block_descriptor_3->layout with BLOCK_HAS_EXTENDED_LAYOUT
// and for Block_byref_3->layout with BLOCK_BYREF_LAYOUT_EXTENDED

// If the layout field is less than 0x1000, then it is a compact encoding 
// of the form 0xXYZ: X strong pointers, then Y byref pointers, 
// then Z weak pointers.

// If the layout field is 0x1000 or greater, it points to a 
// string of layout bytes. Each byte is of the form 0xPN.
// Operator P is from the list below. Value N is a parameter for the operator.
// Byte 0x00 terminates the layout; remaining block data is non-pointer bytes.

enum {
    BLOCK_LAYOUT_ESCAPE = 0, // N=0 halt, rest is non-pointer. N!=0 reserved.
    BLOCK_LAYOUT_NON_OBJECT_BYTES = 1,    // N bytes non-objects
    BLOCK_LAYOUT_NON_OBJECT_WORDS = 2,    // N words non-objects
    BLOCK_LAYOUT_STRONG           = 3,    // N words strong pointers
    BLOCK_LAYOUT_BYREF            = 4,    // N words byref pointers
    BLOCK_LAYOUT_WEAK             = 5,    // N words weak pointers
    BLOCK_LAYOUT_UNRETAINED       = 6,    // N words unretained pointers
    BLOCK_LAYOUT_UNKNOWN_WORDS_7  = 7,    // N words, reserved
    BLOCK_LAYOUT_UNKNOWN_WORDS_8  = 8,    // N words, reserved
    BLOCK_LAYOUT_UNKNOWN_WORDS_9  = 9,    // N words, reserved
    BLOCK_LAYOUT_UNKNOWN_WORDS_A  = 0xA,  // N words, reserved
    BLOCK_LAYOUT_UNUSED_B         = 0xB,  // unspecified, reserved
    BLOCK_LAYOUT_UNUSED_C         = 0xC,  // unspecified, reserved
    BLOCK_LAYOUT_UNUSED_D         = 0xD,  // unspecified, reserved
    BLOCK_LAYOUT_UNUSED_E         = 0xE,  // unspecified, reserved
    BLOCK_LAYOUT_UNUSED_F         = 0xF,  // unspecified, reserved
};


// Runtime support functions used by compiler when generating copy/dispose helpers

// Values for _Block_object_assign() and _Block_object_dispose() parameters
enum {
    // see function implementation for a more complete description of these fields and combinations
    BLOCK_FIELD_IS_OBJECT   =  3,  // id, NSObject, __attribute__((NSObject)), block, ...
    BLOCK_FIELD_IS_BLOCK    =  7,  // a block variable
    BLOCK_FIELD_IS_BYREF    =  8,  // the on stack structure holding the __block variable
    BLOCK_FIELD_IS_WEAK     = 16,  // declared __weak, only used in byref copy helpers
    BLOCK_BYREF_CALLER      = 128, // called from __block (byref) copy/dispose support routines.
};

enum {
    BLOCK_ALL_COPY_DISPOSE_FLAGS = 
        BLOCK_FIELD_IS_OBJECT | BLOCK_FIELD_IS_BLOCK | BLOCK_FIELD_IS_BYREF |
        BLOCK_FIELD_IS_WEAK | BLOCK_BYREF_CALLER
};

// These enumerators are used for bit 15-14 of the in-descriptor block flags.
enum {
    // Generic block helpers aren't used.
    BLOCK_GENERIC_HELPER_NONE = 0,

    // Generic block helpers use the extended layout string to copy/destroy
    // blocks.
    BLOCK_GENERIC_HELPER_FROM_LAYOUT = 1,

    // Generic block helpers use inline generic helper info to copy/destroy
    // blocks.
    BLOCK_GENERIC_HELPER_INLINE = 2,

    // Generic block helpers use out-of-line generic helper info to copy/destroy
    // blocks.
    BLOCK_GENERIC_HELPER_OUTOFLINE = 3
};

// Generic block helpers
//
// The idea of generic block helpers is to use the extended layout string or the
// generic helper layout string in the descriptor to copy/destroy certain types
// of captures instead of calling the custom copy/dispose helpers in
// Block_descriptor_2.
//
// The generic helper code currenty handles the following non-trivial capture
// types:
//
// 1. Non-block ARC/non-ARC strong ObjC pointers.
// 2. ARC weak ObjC pointers.
// 3. Block pointers.
// 4. __block qualified types.
//
// Any types that are non-trivial to copy/destroy and can’t be handled by the
// generic helpers (e.g., non-trivial C++ types) are handled by the custom
// copy/dispose helpers.
//
// The generic helper layout string is either inline or out-of-line. The inline
// string has the following format:
//
// b0-b7: reserved for future extension.
// b8-b11: number of consecutive strong pointers.
// b12-b15: number of consecutive block pointers.
// b16-b19: number of consecutive byref pointers.
// b20-b23: number of consecutive weak pointers.
// b24-b63 (or b31 for small descriptors): reserved for future extension.
//
// The inline format is used only if the number of captures for each type is
// smaller than 16 and all the captures are laid out consecutively.
//
// The out-of-line string has the following format:
//
// b0-b7:   reserved for future extension.
// b8-b15:  (4-bit BlockCaptureKind opcode, 4-bit count) pair
// b16-b23: (4-bit BlockCaptureKind opcode, 4-bit count) pair
// ...
// 8-bit terminator, which is just zero.
//
// The extended layout string, which clang currently emits when flag
// BLOCK_HAS_EXTENDED_LAYOUT is set, can be used by the generic helpers instead
// of the generic helper layout string to save space. It cannot be used if a
// block type is captured and currently isn't used if the block captures types
// that aren't 1, 2, or 4.

// These enumerators are used to capture the context in which an exception was
// thrown.
enum {
    // No exceptions were thrown.
    EXCP_NONE,

    // An exception was thrown when the generic copy helper was running.
    EXCP_COPY_GENERIC,

    // An exception was thrown when the custom copy helper was running.
    EXCP_COPY_CUSTOM,

    // An exception was thrown when the generic dispose helper was running.
    EXCP_DESTROY_GENERIC,

    // An exception was thrown when the custom dispose helper was running.
    EXCP_DESTROY_CUSTOM,
};

// This is the base class of template class HelperBase.
struct HelperBaseData {
    // This field takes the value of one of the BLOCK_GENERIC_HELPER_*
    // enumerators that aren't BLOCK_GENERIC_HELPER_NONE.
    unsigned kind : 2;

    // True if the generic helpers use out-of-line extended layout strings. Only
    // meaningful if kind is BLOCK_GENERIC_HELPER_FROM_LAYOUT.
    unsigned usesOutOfLineLayout : 1;

    // Pointers to the captures in the destination and source block structure.
    // srccapptr is valid only when copy helper functions are running.
    unsigned char *capptr, *srccapptr;

    // The number of captures of a particuler opcode that have been
    // copied/destroyed so far.
    unsigned capcounter;

#ifdef __cplusplus
    HelperBaseData(unsigned k, unsigned u) : kind(k), usesOutOfLineLayout(u) {}
#endif
};

typedef struct HelperBaseData HelperBaseData;

#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    unsigned state;
    struct Block_layout *dstbl;
    HelperBaseData *helperClass;
} ExcpCleanupInfo;

BLOCK_HIDDEN void *getHelper(struct Block_layout *dstbl,
                             struct Block_layout *srcbl);
BLOCK_HIDDEN void _call_generic_copy_helper(struct Block_layout *dstbl,
                                            struct Block_layout *srcbl,
                                            HelperBaseData *helperClass);
BLOCK_HIDDEN void _call_generic_destroy_helper(struct Block_layout *bl,
                                               HelperBaseData *helperClass);
BLOCK_HIDDEN void _call_custom_copy_helper(struct Block_layout *dstbl,
                                           struct Block_layout *srcbl);
BLOCK_HIDDEN void _call_custom_dispose_helper(struct Block_layout *bl);
BLOCK_HIDDEN void _call_copy_helpers_excp(struct Block_layout *dstbl,
                                          struct Block_layout *srcbl,
                                          HelperBaseData *helper);
BLOCK_HIDDEN void _call_dispose_helpers_excp(struct Block_layout *bl,
                                             HelperBaseData *helper);
BLOCK_HIDDEN void _cleanup_generic_captures(ExcpCleanupInfo *info);
#ifdef __cplusplus
}
#endif

// Function pointer accessors

static inline __typeof__(void (*)(void *, ...))
_Block_get_invoke_fn(struct Block_layout *block)
{
    return (void (*)(void *, ...))_Block_get_function_pointer(block->invoke);
}

static inline void 
_Block_set_invoke_fn(struct Block_layout *block, void (*fn)(void *, ...))
{
    _Block_set_function_pointer(block->invoke, fn);
}


static inline __typeof__(void (*)(void *, const void *))
_Block_get_copy_fn(struct Block_descriptor_2 *desc)
{
    return (void (*)(void *, const void *))_Block_get_function_pointer(desc->copy);
}

static inline void 
_Block_set_copy_fn(struct Block_descriptor_2 *desc,
                   void (*fn)(void *, const void *))
{
    _Block_set_function_pointer(desc->copy, fn);
}


static inline __typeof__(void (*)(const void *))
_Block_get_dispose_fn(struct Block_descriptor_2 *desc)
{
    return (void (*)(const void *))_Block_get_function_pointer(desc->dispose);
}

static inline void 
_Block_set_dispose_fn(struct Block_descriptor_2 *desc,
                      void (*fn)(const void *))
{
    _Block_set_function_pointer(desc->dispose, fn);
}

static inline void *
_Block_get_descriptor(struct Block_layout *aBlock)
{
    void *descriptor;

#if BLOCK_SMALL_DESCRIPTOR_SUPPORTED && \
    __has_feature(ptrauth_signed_block_descriptors)
    if (aBlock->flags & BLOCK_SMALL_DESCRIPTOR) {
        uintptr_t disc = ptrauth_blend_discriminator(
                &aBlock->descriptor, _Block_descriptor_ptrauth_discriminator);
        descriptor = (void *)ptrauth_auth_data(
                aBlock->descriptor, ptrauth_key_asda, disc);
        return descriptor;
    }
#endif

#if __has_feature(ptrauth_calls)
    descriptor = (void *)ptrauth_strip(aBlock->descriptor, ptrauth_key_asda);
#else
    descriptor = (void *)aBlock->descriptor;
#endif

    return descriptor;
}

static inline void
_Block_set_descriptor(struct Block_layout *aBlock, void *desc)
{
    aBlock->descriptor = (struct Block_descriptor_1 *)desc;
}

static inline __typeof__(void (*)(void *, const void *))
_Block_get_copy_function(struct Block_layout *aBlock)
{
    if (!(aBlock->flags & BLOCK_HAS_COPY_DISPOSE))
        return NULL;

    void *desc = _Block_get_descriptor(aBlock);
#if BLOCK_SMALL_DESCRIPTOR_SUPPORTED
    if (aBlock->flags & BLOCK_SMALL_DESCRIPTOR) {
        struct Block_descriptor_small *bds =
                (struct Block_descriptor_small *)desc;
        return _Block_get_relative_function_pointer(
                bds->copy, void (*)(void *, const void *));
    }
#endif

    struct Block_descriptor_2 *bd2 =
            (struct Block_descriptor_2 *)((unsigned char *)desc +
                                          sizeof(struct Block_descriptor_1));
    return _Block_get_copy_fn(bd2);
}

static inline __typeof__(void (*)(const void *))
_Block_get_dispose_function(struct Block_layout *aBlock)
{
    if (!(aBlock->flags & BLOCK_HAS_COPY_DISPOSE))
        return NULL;

    void *desc = _Block_get_descriptor(aBlock);
#if BLOCK_SMALL_DESCRIPTOR_SUPPORTED
    if (aBlock->flags & BLOCK_SMALL_DESCRIPTOR) {
        struct Block_descriptor_small *bds =
                (struct Block_descriptor_small *)desc;
        return _Block_get_relative_function_pointer(
                bds->dispose, void (*)(const void *));
    }
#endif

    struct Block_descriptor_2 *bd2 =
            (struct Block_descriptor_2 *)((unsigned char *)desc +
                                          sizeof(struct Block_descriptor_1));
    return _Block_get_dispose_fn(bd2);
}

// Other support functions

// runtime entry to get total size of a closure
BLOCK_EXPORT size_t Block_size(void *aBlock);

// indicates whether block was compiled with compiler that sets the ABI related metadata bits
BLOCK_EXPORT bool _Block_has_signature(void *aBlock)
    __OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_4_3);

// returns TRUE if return value of block is on the stack, FALSE otherwise
BLOCK_EXPORT bool _Block_use_stret(void *aBlock)
    __OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_4_3);

// Returns a string describing the block's parameter and return types.
// The encoding scheme is the same as Objective-C @encode.
// Returns NULL for blocks compiled with some compilers.
BLOCK_EXPORT const char * _Block_signature(void *aBlock)
    __OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_4_3);

// Returns a string describing the block's GC layout.
// This uses the GC skip/scan encoding.
// May return NULL.
BLOCK_EXPORT const char * _Block_layout(void *aBlock)
    __OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_4_3);

// Returns a string describing the block's layout.
// This uses the "extended layout" form described above.
// May return NULL.
BLOCK_EXPORT const char * _Block_extended_layout(void *aBlock)
    __OSX_AVAILABLE_STARTING(__MAC_10_8, __IPHONE_7_0);

// Callable only from the ARR weak subsystem while in exclusion zone
BLOCK_EXPORT bool _Block_tryRetain(const void *aBlock)
    __OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_4_3);

// Callable only from the ARR weak subsystem while in exclusion zone
BLOCK_EXPORT bool _Block_isDeallocating(const void *aBlock)
    __OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_4_3);


// the raw data space for runtime classes for blocks
// class+meta used for stack, malloc, and collectable based blocks
BLOCK_EXPORT void * _NSConcreteMallocBlock[32]
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_3_2);
BLOCK_EXPORT void * _NSConcreteAutoBlock[32]
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_3_2);
BLOCK_EXPORT void * _NSConcreteFinalizingBlock[32]
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_3_2);
BLOCK_EXPORT void * _NSConcreteWeakBlockVariable[32]
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_3_2);
// declared in Block.h
// BLOCK_EXPORT void * _NSConcreteGlobalBlock[32];
// BLOCK_EXPORT void * _NSConcreteStackBlock[32];


struct Block_callbacks_RR {
    size_t  size;                   // size == sizeof(struct Block_callbacks_RR)
    void  (*retain)(const void *);
    void  (*release)(const void *);
    void  (*destructInstance)(const void *);
};
typedef struct Block_callbacks_RR Block_callbacks_RR;

BLOCK_EXPORT void _Block_use_RR2(const Block_callbacks_RR *callbacks);


#endif
