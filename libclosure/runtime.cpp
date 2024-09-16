/*
 * runtime.cpp
 * libclosure
 *
 * Copyright (c) 2008-2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LLVM_LICENSE_HEADER@
 */

#include "Block_private.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <tuple>
#include <utility>

#if __has_include(<os/assumes.h>)
 #include <os/assumes.h>
#else // __has_include(<os/assumes.h>)
#define os_assert(_x) assert(_x)
#define os_assumes(_x) (_x)
#define os_unlikely(_x) (_x)
#define os_hardware_trap() abort()
#endif // __has_include(<os/assumes.h>)

#if __has_include(<platform/string.h>)
#include <platform/string.h>
#define memmove _platform_memmove
#endif

#if TARGET_OS_WIN32
#define _CRT_SECURE_NO_WARNINGS 1
#include <windows.h>
static __inline bool OSAtomicCompareAndSwapLong(long oldl, long newl, long volatile *dst) 
{ 
    // fixme barrier is overkill -- see objc-os.h
    long original = InterlockedCompareExchange(dst, newl, oldl);
    return (original == oldl);
}

static __inline bool OSAtomicCompareAndSwapInt(int oldi, int newi, int volatile *dst) 
{ 
    // fixme barrier is overkill -- see objc-os.h
    int original = InterlockedCompareExchange(dst, newi, oldi);
    return (original == oldi);
}
#else
#define OSAtomicCompareAndSwapLong(_Old, _New, _Ptr) __sync_bool_compare_and_swap(_Ptr, _Old, _New)
#define OSAtomicCompareAndSwapInt(_Old, _New, _Ptr) __sync_bool_compare_and_swap(_Ptr, _Old, _New)
#endif


/*******************************************************************************
Internal Utilities
********************************************************************************/

static int32_t latching_incr_int(volatile int32_t *where) {
    while (1) {
        int32_t old_value = *where;
        if ((old_value & BLOCK_REFCOUNT_MASK) == BLOCK_REFCOUNT_MASK) {
            return BLOCK_REFCOUNT_MASK;
        }
        if (OSAtomicCompareAndSwapInt(old_value, old_value+2, where)) {
            return old_value+2;
        }
    }
}

static bool latching_incr_int_not_deallocating(volatile int32_t *where) {
    while (1) {
        int32_t old_value = *where;
        if (old_value & BLOCK_DEALLOCATING) {
            // if deallocating we can't do this
            return false;
        }
        if ((old_value & BLOCK_REFCOUNT_MASK) == BLOCK_REFCOUNT_MASK) {
            // if latched, we're leaking this block, and we succeed
            return true;
        }
        if (OSAtomicCompareAndSwapInt(old_value, old_value+2, where)) {
            // otherwise, we must store a new retained value without the deallocating bit set
            return true;
        }
    }
}


// return should_deallocate?
static bool latching_decr_int_should_deallocate(volatile int32_t *where) {
    while (1) {
        int32_t old_value = *where;
        if ((old_value & BLOCK_REFCOUNT_MASK) == BLOCK_REFCOUNT_MASK) {
            return false; // latched high
        }
        if ((old_value & BLOCK_REFCOUNT_MASK) == 0) {
            return false;   // underflow, latch low
        }
        int32_t new_value = old_value - 2;
        bool result = false;
        if ((old_value & (BLOCK_REFCOUNT_MASK|BLOCK_DEALLOCATING)) == 2) {
            new_value = old_value - 1;
            result = true;
        }
        if (OSAtomicCompareAndSwapInt(old_value, new_value, where)) {
            return result;
        }
    }
}


/**************************************************************************
Framework callback functions and their default implementations.
***************************************************************************/
#if !TARGET_OS_WIN32
#pragma mark Framework Callback Routines
#endif

static void _Block_retain_object_default(const void *ptr __unused) { }

static void _Block_release_object_default(const void *ptr __unused) { }

static void _Block_destructInstance_default(const void *aBlock __unused) {}

static void (*_Block_retain_object)(const void *ptr) = _Block_retain_object_default;
static void (*_Block_release_object)(const void *ptr) = _Block_release_object_default;
static void (*_Block_destructInstance) (const void *aBlock) = _Block_destructInstance_default;


/**************************************************************************
Callback registration from ObjC runtime and CoreFoundation
***************************************************************************/

void _Block_use_RR2(const Block_callbacks_RR *callbacks) {
    _Block_retain_object = callbacks->retain;
    _Block_release_object = callbacks->release;
    _Block_destructInstance = callbacks->destructInstance;
}

/****************************************************************************
Accessors for block descriptor fields
*****************************************************************************/

template <class T>
static T *unwrap_relative_pointer(int32_t &offset)
{
    if (offset == 0)
        return nullptr;

    uintptr_t base = (uintptr_t)&offset;
    uintptr_t extendedOffset = (uintptr_t)(intptr_t)offset;
    uintptr_t pointer = base + extendedOffset;
    return (T *)pointer;
}

static struct Block_descriptor_2 * _Block_descriptor_2(struct Block_layout *aBlock)
{
    uint8_t *desc = (uint8_t *)_Block_get_descriptor(aBlock);
    desc += sizeof(struct Block_descriptor_1);
    return (struct Block_descriptor_2 *)desc;
}

static struct Block_descriptor_3 * _Block_descriptor_3(struct Block_layout *aBlock)
{
    uint8_t *desc = (uint8_t *)_Block_get_descriptor(aBlock);
    desc += sizeof(struct Block_descriptor_1);
    if (aBlock->flags & BLOCK_HAS_COPY_DISPOSE) {
        desc += sizeof(struct Block_descriptor_2);
    }
    return (struct Block_descriptor_3 *)desc;
}

extern "C" {
void _call_custom_copy_helper(struct Block_layout *result,
                              struct Block_layout *aBlock) {
    if (auto *pFn = _Block_get_copy_function(aBlock))
        pFn(result, aBlock);
}

void _call_custom_dispose_helper(struct Block_layout *aBlock) {
    if (auto *pFn = _Block_get_dispose_function(aBlock))
        pFn(aBlock);
}
}

#if HAVE_UNWIND

static uint32_t extractBitfield(unsigned pos, unsigned width,
                                uint32_t layoutint) {
    return (layoutint >> pos) & ((1 << width) - 1);
}

/// Get a field of the block descriptor. fieldNum is the index of the field
/// relative to the address point of the descriptor struct.
template <class FieldType>
static FieldType &getDescField(struct Block_layout *aBlock, int fieldNum) {
    auto *desc = (FieldType *)_Block_get_descriptor(aBlock);
    return *(desc + fieldNum);
}

// Determines whether the descriptor has in-descriptor flags.
static bool hasInDescriptorFlags(struct Block_layout *aBlock) {
    void *desc = _Block_get_descriptor(aBlock);
#if BLOCK_SMALL_DESCRIPTOR_SUPPORTED
    if (aBlock->flags & BLOCK_SMALL_DESCRIPTOR)
        return ((Block_descriptor_small *)desc)->has_in_descriptor_flags;
#endif
    return ((Block_descriptor_1 *)desc)->reserved;
}

static uint32_t getInDescriptorFlags(struct Block_layout *aBlock) {
#if BLOCK_SMALL_DESCRIPTOR_SUPPORTED
    if (aBlock->flags & BLOCK_SMALL_DESCRIPTOR)
        return getDescField<uint32_t>(aBlock, -1);
#endif
    return (uint32_t)((Block_descriptor_1 *)_Block_get_descriptor(aBlock))
        ->reserved;
}

// Determines whether the extended layout string is inline.
static bool hasInlineExtendedLayout(struct Block_layout *bl) {
    uintptr_t p = (uintptr_t)_Block_extended_layout(bl);
    return p < 0x1000;
}

// Get field generic_helper_info.
static uintptr_t getGenericHelperInfo(struct Block_layout *aBlock,
                                      bool IsInline) {
#if BLOCK_SMALL_DESCRIPTOR_SUPPORTED
    if (aBlock->flags & BLOCK_SMALL_DESCRIPTOR) {
        if (IsInline)
            return getDescField<uint32_t>(aBlock, -2);
        return (uintptr_t)unwrap_relative_pointer<unsigned char>(
            getDescField<int32_t>(aBlock, -2));
    }
#endif
    return getDescField<uintptr_t>(aBlock, -1);
}

// Get bit 15-14 of the in-descriptor block flags, which contains information
// about whether generic helpers are used to copy/destroy captures and whether
// the generic helper information string is inline or out-of-line if it is
// emitted.
static unsigned getGenericHelperFlags(struct Block_layout *aBlock) {
    uint32_t flags = getInDescriptorFlags(aBlock);
    return extractBitfield(14, 2, flags);
}

// Compute the offset of the first capture from the head of the block.
static unsigned getOffsetOfFirstCapture() { return sizeof(Block_layout); }

// Advance pointer by 'distance' bytes.
static void advancePointer(unsigned distance, unsigned char *&ptr) {
    ptr += distance;
}

// Advance pointers by 'distance' bytes.
static void advancePointers(unsigned distance, unsigned char *&dst,
                            unsigned char *&src) {
    advancePointer(distance, dst);
    advancePointer(distance, src);
}

#if HAVE_OBJC
extern "C" {
void *objc_retain(void *) noexcept;
void *objc_release(void *) noexcept;
void objc_copyWeak(void *, void *) noexcept;
void objc_destroyWeak(void *) noexcept;
}
#endif

static void *loadPtr(unsigned char *p) { return *((void **)p); }

/// Read the opcode and count from a byte in an out-of-line string. The opcode
/// is in the upper four bits and the count is in the lower four bits. The
/// compiler subtracts one from the real count, so we add it back here.
static std::pair<unsigned, unsigned> getOutOfLineOpcodeCount(unsigned char c) {
    return std::make_pair((unsigned)(c >> 4 & 0xf), (unsigned)(c & 0xf) + 1);
}

/// This is the CRTP base class of the four derived classes that copy/destroy
/// captures using either field 'layout' or field 'generic_helper_info' of the
/// descriptors.
///
/// The derived classes must implement the following methods:
///
/// initState: initialize the state.
/// nextState: move to the next state.
/// getNextOpcodeAndCount: return a (BlockCaptureKind, count) pair.

template <class Derived> struct HelperBase : HelperBaseData {
    enum BlockCaptureKind {
        BCK_DONE = 0,
        BCK_NON_OBJECT_BYTES = 1,
        BCK_NON_OBJECT_WORDS = 2,
        BCK_STRONG = 3,
        BCK_BLOCK = 4,
        BCK_BYREF = 5,
        BCK_WEAK = 6,
    };

    HelperBase(unsigned kind, unsigned usesOutOfLineLayout) : HelperBaseData(kind, usesOutOfLineLayout) {}

    template <BlockCaptureKind BCK> void copyCapture(unsigned count) {
        assert(capcounter == 0 && "capcounter expected to be zero");

        // Copy consecutive captures of kind BCK.
        for (; capcounter < count; ++capcounter) {
            switch (BCK) {
            case BCK_STRONG:
#if HAVE_OBJC
                objc_retain(loadPtr(srccapptr));
#else
                assert(false && "found strong capture with no ObjC support");
#endif // HAVE_OBJC
                break;
            case BCK_BYREF:
                _Block_object_assign((void *)capptr, loadPtr(srccapptr),
                                     BLOCK_FIELD_IS_BYREF);
                break;
            case BCK_BLOCK:
                _Block_object_assign((void *)capptr, loadPtr(srccapptr),
                                     BLOCK_FIELD_IS_BLOCK);
                break;
            case BCK_WEAK:
#if HAVE_OBJC
                objc_copyWeak((void *)capptr, (void *)srccapptr);
#else
                assert(false && "found weak capture with no ObjC support");
#endif // HAVE_OBJC
                break;
            default:
                assert(false && "unexpected opcode");
            }

            // Advance both destination and source pointers by the pointer size.
            advancePointers(__SIZEOF_POINTER__, capptr, srccapptr);
        }
    }

    void initState(struct Block_layout *bl) {
        derived().initState(bl);
        capcounter = 0;
        // Initialize capptr so that it points to the first capture.
        capptr = (unsigned char *)bl;
        advancePointer(getOffsetOfFirstCapture(), capptr);
    }

    void initCopyState(struct Block_layout *dstbl, struct Block_layout *srcbl) {
        initState(dstbl);
        // Initialize srccapptr so that it points to the first capture.
        srccapptr = (unsigned char *)srcbl;
        advancePointer(getOffsetOfFirstCapture(), srccapptr);
    }

    void nextState() {
        derived().nextState();
        capcounter = 0;
    }

    void copyBlock(struct Block_layout *dstbl,
                                  struct Block_layout *srcbl) {
        initCopyState(dstbl, srcbl);

        // Keep copying the captutes until opcode BCK_DONE is seen.
        while (true) {
            unsigned opcode, count;
            std::tie(opcode, count) = derived().getNextOpcodeAndCount();

            switch (opcode) {
            case BCK_DONE:
                return;
            case BCK_NON_OBJECT_BYTES:
                advancePointers(count, capptr, srccapptr);
                break;
            case BCK_NON_OBJECT_WORDS:
                advancePointers(__SIZEOF_POINTER__ * count, capptr, srccapptr);
                break;
            case BCK_STRONG:
                copyCapture<BCK_STRONG>(count);
                break;
            case BCK_BYREF:
                copyCapture<BCK_BYREF>(count);
                break;
            case BCK_BLOCK:
                copyCapture<BCK_BLOCK>(count);
                break;
            case BCK_WEAK:
                copyCapture<BCK_WEAK>(count);
                break;
            default:
                assert(false && "unexpected opcode");
            }

            nextState();
        }
    }

    template <BlockCaptureKind BCK>
    bool disposeCapture(unsigned count, unsigned char *endptr) {
        // Copy consecutive captures of kind BCK.
        // Note that capcounter isn't zero if an exception was thrown while the
        // generic dispose helper method (destroyBlock) was running.

        for (; capcounter < count; ++capcounter) {
            // If endptr isn't null, that means an exception was thrown while
            // the generic copy helper method (copyBlock) was running. In that
            // case, all the captures up to (but not including) the one endptr
            // points to are destroyed.
            if (capptr == endptr)
                // Return true to indicate no more captures should be destroyed.
                return true;

            switch (BCK) {
#if HAVE_OBJC
            case BCK_STRONG:
                objc_release(loadPtr(capptr));
                break;
#endif
            case BCK_BYREF:
                _Block_object_dispose(loadPtr(capptr), BLOCK_FIELD_IS_BYREF);
                break;
            case BCK_BLOCK:
                _Block_object_dispose(loadPtr(capptr), BLOCK_FIELD_IS_BLOCK);
                break;
#if HAVE_OBJC
            case BCK_WEAK:
                objc_destroyWeak((void *)capptr);
                break;
#endif
            default:
                assert(false && "unexpected opcode");
            }

            advancePointer(__SIZEOF_POINTER__, capptr);
        }

        return false;
    }

    // endptr is the address of the capture in the block structure that threw an
    // exception while being copied.
    void destroyBlock(struct Block_layout *bl,
                                     bool initialize = true,
                                     unsigned char *endptr = nullptr) {
        // Don't initialize the state if an exception was thrown while the
        // generic dispose helper method (destroyBlock) was running.
        if (initialize)
            initState(bl);

        // Keep destroying the captutes until opcode BCK_DONE is seen.
        while (true) {
            unsigned count;
            unsigned opcode;
            std::tie(opcode, count) = derived().getNextOpcodeAndCount();

            bool earlyExit = false;

            switch (opcode) {
            case BCK_DONE:
                return;
            case BCK_NON_OBJECT_BYTES:
                advancePointer(count, capptr);
                break;
            case BCK_NON_OBJECT_WORDS:
                advancePointer(__SIZEOF_POINTER__ * count, capptr);
                break;
            case BCK_STRONG:
                earlyExit =
                    disposeCapture<BCK_STRONG>(count, endptr);
                break;
            case BCK_BYREF:
                earlyExit =
                    disposeCapture<BCK_BYREF>(count, endptr);
                break;
            case BCK_BLOCK:
                earlyExit =
                    disposeCapture<BCK_BLOCK>(count, endptr);
                break;
            case BCK_WEAK:
                earlyExit = disposeCapture<BCK_WEAK>(count, endptr);
                break;
            default:
                assert(false && "unexpected opcode");
            }

            if (earlyExit) {
                // We've destroyed/released all the captures that have been
                // copied.
                assert(endptr && "endptr isn't expected to be null");
                return;
            }

            nextState();
        }
    }

    void call_copy_helpers(Block_layout *dstbl, Block_layout *srcbl) {
        // Call the C function.
        _call_copy_helpers_excp(dstbl, srcbl, this);
    }

    void call_dispose_helpers(Block_layout *bl) {
        // Call the C function.
        _call_dispose_helpers_excp(bl, this);
    }

    void cleanup_captures(ExcpCleanupInfo *info) {
        if (info->state == EXCP_COPY_CUSTOM ||
            info->state == EXCP_DESTROY_CUSTOM) {
            // Destroy all captures.
            destroyBlock(info->dstbl);
        } else if (info->state == EXCP_COPY_GENERIC) {
            // Destroy all captures that had been copied before copying the
            // capture capptr points to caused an exception.
            destroyBlock(info->dstbl, true, capptr);
        } else {
            // Skip the capture that threw an exception and destroy the
            // remaining captures.
            assert(info->state == EXCP_DESTROY_GENERIC);
            advancePointer(__SIZEOF_POINTER__, capptr);
            ++capcounter;
            destroyBlock(info->dstbl, false);
        }
    }

    Derived &derived() { return static_cast<Derived &>(*this); }
};

/// This class copies/destroys captures using the inline generic helper
/// information.
struct GenericInline : HelperBase<GenericInline> {
    GenericInline() : HelperBase(BLOCK_GENERIC_HELPER_INLINE, false) {}
    void initState(struct Block_layout *bl) {
        layoutint = ((uint32_t)getGenericHelperInfo(bl, true));
        // Skip the first 8 bits, which are reserved for future extension.
        pos = 8;
    }

    void nextState() { pos += 4; }

    std::pair<unsigned, unsigned> getNextOpcodeAndCount() {
        // __strong captures are processed first, then block captutes, then
        // byref captures, and then __weak captures.
        unsigned opcode;

        switch (pos) {
        case 8:
            opcode = BCK_STRONG;
            break;
        case 12:
            opcode = BCK_BLOCK;
            break;
        case 16:
            opcode = BCK_BYREF;
            break;
        case 20:
            opcode = BCK_WEAK;
            break;
        case 24:
            return {BCK_DONE, 0};
        default:
            assert(false);
            break;
        };

        unsigned count = extractBitfield(pos, 4, layoutint);
        return {opcode, count};
    }

    uint32_t layoutint;
    unsigned pos;
};

/// This class copies/destroys captures using the out-of-line generic helper
/// information.
struct GenericOutOfLine : HelperBase<GenericOutOfLine> {
    GenericOutOfLine() : HelperBase(BLOCK_GENERIC_HELPER_OUTOFLINE, false) {}
    void initState(struct Block_layout *bl) {
        layoutstr = (const unsigned char *)getGenericHelperInfo(bl, false);
        // Skip the first byte, which is reserved for future extension.
        ++layoutstr;
    }
    void nextState() { ++layoutstr; }
    std::pair<unsigned, unsigned> getNextOpcodeAndCount() {
        return ::getOutOfLineOpcodeCount(*layoutstr);
    }
    const unsigned char *layoutstr;
};

/// This class copies/destroys captures using the inline extended layout
/// form.
struct ExtendedInline : HelperBase<ExtendedInline> {
    ExtendedInline() : HelperBase(BLOCK_GENERIC_HELPER_FROM_LAYOUT, false) {}
    void initState(struct Block_layout *bl) {
        // Read the integer stored in the extended layout field.
        layoutint = (uint16_t)(uintptr_t)_Block_extended_layout(bl);

        // Start at bit 8 since that is where information for __strong captures
        // is stored.
        pos = 8;
    }

    void nextState() { pos -= 4; }

    std::pair<unsigned, unsigned> getNextOpcodeAndCount() {
        // __strong captures are processed first, then byref captures, and then
        // __weak captures.
        unsigned opcode;

        switch (pos) {
        case 8:
            opcode = BCK_STRONG;
            break;
        case 4:
            opcode = BCK_BYREF;
            break;
        case 0:
            opcode = BCK_WEAK;
            break;
        case -4:
            return {BCK_DONE, 0};
            break;
        default:
            assert(false);
            break;
        }

        unsigned count = extractBitfield((unsigned)pos, 4, layoutint);
        return {opcode, count};
    }

    // The inline extended layout integer stored in field 'layout' of the
    // descriptor structs.
    uint16_t layoutint;

    // The offset in bits of the byte in the layout integer currently being
    // processed.
    int pos;
};

/// This class copies/destroys captures using the out-of-line extended layout
/// encoding string.
struct ExtendedOutOfLine : HelperBase<ExtendedOutOfLine> {
    ExtendedOutOfLine() : HelperBase(BLOCK_GENERIC_HELPER_FROM_LAYOUT, true) {}
    void initState(struct Block_layout *bl) {
        // Initialize laytoutstr to the head of the extended layout string.
        layoutstr = (const unsigned char *)_Block_extended_layout(bl);
    }

    void nextState() { ++layoutstr; }

    std::pair<unsigned, unsigned> getNextOpcodeAndCount() {
        unsigned opcode, count;
        std::tie(opcode, count) = ::getOutOfLineOpcodeCount(*layoutstr);

        switch (opcode) {
        case BLOCK_LAYOUT_ESCAPE:
            opcode = BCK_DONE;
            break;
        case BLOCK_LAYOUT_NON_OBJECT_BYTES:
            opcode = BCK_NON_OBJECT_BYTES;
            break;
        case BLOCK_LAYOUT_NON_OBJECT_WORDS:
            opcode = BCK_NON_OBJECT_WORDS;
            break;
        case BLOCK_LAYOUT_STRONG:
            opcode = BCK_STRONG;
            break;
        case BLOCK_LAYOUT_BYREF:
            opcode = BCK_BYREF;
            break;
        case BLOCK_LAYOUT_WEAK:
            opcode = BCK_WEAK;
            break;
        case BLOCK_LAYOUT_UNRETAINED:
            opcode = BCK_NON_OBJECT_WORDS;
            break;
        default:
            assert(false && "unexpected opcode");
            break;
        }

        return {opcode, count};
    }

    // Pointer to the byte of the extended layout string currently being
    // processed.
    const unsigned char *layoutstr;
};

extern "C" {
// These two functions do cleanups if an exception is thrown while copying or
// destroying the captures. They aren't implemented in this file (which is a
// C++ file) since doing so would cause the code to use the C++ personality
// function, which would require linking to libc++.

void _call_copy_helpers_excp(Block_layout *dstbl, Block_layout *srcbl,
                             HelperBaseData *helper);
void _call_dispose_helpers_excp(Block_layout *bl, HelperBaseData *helper);

// The following two functions copy/destroy the captures using the extended
// layout strings or the generic helper information and are called by the above
// two functions.
void _call_generic_copy_helper(struct Block_layout *dstbl,
                               struct Block_layout *srcbl,
                               HelperBaseData *helper) {
    assert(helper && "helper is expected to be non-null");
    switch (helper->kind) {
    case BLOCK_GENERIC_HELPER_NONE:
        assert(false && "unexpected helper kind");
        break;
    case BLOCK_GENERIC_HELPER_FROM_LAYOUT:
        if (helper->usesOutOfLineLayout)
            ((ExtendedOutOfLine *)helper)
                ->copyBlock(dstbl, srcbl);
        else
            ((ExtendedInline *)helper)->copyBlock(dstbl, srcbl);
        break;
    case BLOCK_GENERIC_HELPER_INLINE:
        ((GenericInline *)helper)->copyBlock(dstbl, srcbl);
        break;
    case BLOCK_GENERIC_HELPER_OUTOFLINE:
        ((GenericOutOfLine *)helper)->copyBlock(dstbl, srcbl);
        break;
    }
}

void _call_generic_destroy_helper(struct Block_layout *bl,
                                  HelperBaseData *helper) {
    assert(helper && "helper is expected to be non-null");
    switch (helper->kind) {
    case BLOCK_GENERIC_HELPER_NONE:
        assert(false && "unexpected helper kind");
        break;
    case BLOCK_GENERIC_HELPER_FROM_LAYOUT:
        if (helper->usesOutOfLineLayout)
            ((ExtendedOutOfLine *)helper)->destroyBlock(bl);
        else
            ((ExtendedInline *)helper)->destroyBlock(bl);
        break;
    case BLOCK_GENERIC_HELPER_INLINE:
        ((GenericInline *)helper)->destroyBlock(bl);
        break;
    case BLOCK_GENERIC_HELPER_OUTOFLINE:
        ((GenericOutOfLine *)helper)->destroyBlock(bl);
        break;
    }
}

// Cleanup function that is called during stack unwinding after an exception is
// thrown while captures are copied or destroyed.
void _cleanup_generic_captures(ExcpCleanupInfo *info) {
    // The cleanup variable went out of scope. No exceptions were thrown.
    if (info->state == EXCP_NONE)
        return;

    // If an exception is thrown, only the generic dispose helper code has to
    // be run. The custom copy/dispose helpers clean up the captures before
    // stack unwinding is resumed.
    if (HelperBaseData *helper = info->helperClass) {
        switch (helper->kind) {
        case BLOCK_GENERIC_HELPER_NONE:
            assert(false && "helper should be null in this case");
            break;
        case BLOCK_GENERIC_HELPER_FROM_LAYOUT:
            if (helper->usesOutOfLineLayout)
                ((ExtendedOutOfLine *)helper)->cleanup_captures(info);
            else
                ((ExtendedInline *)helper)->cleanup_captures(info);
            break;
        case BLOCK_GENERIC_HELPER_INLINE:
            ((GenericInline *)helper)->cleanup_captures(info);
            break;
        case BLOCK_GENERIC_HELPER_OUTOFLINE:
            ((GenericOutOfLine *)helper)->cleanup_captures(info);
            break;
        }
    }

    // Delete the destination block.
    free(info->dstbl);
}
}

static void _call_copy_helpers(void *result, struct Block_layout *srcbl) {
    Block_layout *dstbl = (Block_layout *)result;

    if (!hasInDescriptorFlags(srcbl))
        // Generic helpers aren't used if the in-descriptor block flags aren't
        // used. Copy the captures using just the custom helper.
        _call_copy_helpers_excp(dstbl, srcbl, nullptr);
    else
        switch (getGenericHelperFlags(srcbl)) {
        case BLOCK_GENERIC_HELPER_NONE:
            // Copy the captures using just the custom helper.
            _call_copy_helpers_excp(dstbl, srcbl, nullptr);
            break;
        case BLOCK_GENERIC_HELPER_FROM_LAYOUT:
            if (hasInlineExtendedLayout(srcbl))
                ExtendedInline().call_copy_helpers(dstbl, srcbl);
            else
                ExtendedOutOfLine().call_copy_helpers(dstbl, srcbl);
            break;
        case BLOCK_GENERIC_HELPER_INLINE:
            GenericInline().call_copy_helpers(dstbl, srcbl);
            break;
        case BLOCK_GENERIC_HELPER_OUTOFLINE:
            GenericOutOfLine().call_copy_helpers(dstbl, srcbl);
            break;
        }
}

static void _call_dispose_helpers(struct Block_layout *bl) {
    if (!hasInDescriptorFlags(bl))
        // Generic helpers aren't used if the in-descriptor block flags aren't
        // used. Destroy the captures using just the custom helper.
        _call_dispose_helpers_excp(bl, nullptr);
    else
        switch (getGenericHelperFlags(bl)) {
        case BLOCK_GENERIC_HELPER_NONE:
            // Destroy the captures using just the custom helper.
            _call_dispose_helpers_excp(bl, nullptr);
            break;
        case BLOCK_GENERIC_HELPER_FROM_LAYOUT:
            if (hasInlineExtendedLayout(bl))
                ExtendedInline().call_dispose_helpers(bl);
            else
                ExtendedOutOfLine().call_dispose_helpers(bl);
            break;
        case BLOCK_GENERIC_HELPER_INLINE:
            GenericInline().call_dispose_helpers(bl);
            break;
        case BLOCK_GENERIC_HELPER_OUTOFLINE:
            GenericOutOfLine().call_dispose_helpers(bl);
            break;
        }
}

#else /* #if !HAVE_UNWIND */

// Driverkit doesn't link to some of the libraries that are needed to support
// generic helpers (libunwind, libobjc, and libcompiler_rt). Just call the
// custom copy/dispose helpers.

static void _call_copy_helpers(void *result, struct Block_layout *srcbl) {
    _call_custom_copy_helper((Block_layout *)result, srcbl);
}

static void _call_dispose_helpers(struct Block_layout *bl) {
    _call_custom_dispose_helper(bl);
}

#endif

/*******************************************************************************
Internal Support routines for copying
********************************************************************************/

#if !TARGET_OS_WIN32
#pragma mark Copy/Release support
#endif

// Copy, or bump refcount, of a block.  If really copying, call the copy helper if present.
void *_Block_copy(const void *arg) {
    struct Block_layout *aBlock;

    if (!arg) return NULL;
    
    // The following would be better done as a switch statement
    aBlock = (struct Block_layout *)arg;
    if (aBlock->flags & BLOCK_NEEDS_FREE) {
        // latches on high
        latching_incr_int(&aBlock->flags);
        return aBlock;
    }
    else if (aBlock->flags & BLOCK_IS_GLOBAL) {
        return aBlock;
    }
    else {
        // Its a stack block.  Make a copy.
        size_t size = Block_size(aBlock);
        struct Block_layout *result = (struct Block_layout *)malloc(size);
        if (!result) return NULL;

        // If the invoke pointer is NULL, this block is bogus. Crash immediately
        // instead of returning a bogus copied block.
        if (os_unlikely(aBlock->invoke == NULL)) {
            // This is a macro that expands to two statements, so the braces are
            // mandatory here.
            os_hardware_trap();
        }

        memmove(result, aBlock, size); // bitcopy first
#if __has_feature(ptrauth_calls)
        // Resign the invoke pointer as it uses address authentication.
        result->invoke = aBlock->invoke;

#if BLOCK_SMALL_DESCRIPTOR_SUPPORTED && \
    __has_feature(ptrauth_signed_block_descriptors)
        if (aBlock->flags & BLOCK_SMALL_DESCRIPTOR) {
            uintptr_t oldDesc = ptrauth_blend_discriminator(
                    &aBlock->descriptor,
                    _Block_descriptor_ptrauth_discriminator);
            uintptr_t newDesc = ptrauth_blend_discriminator(
                    &result->descriptor,
                    _Block_descriptor_ptrauth_discriminator);

            result->descriptor =
                    ptrauth_auth_and_resign(aBlock->descriptor,
                                            ptrauth_key_asda, oldDesc,
                                            ptrauth_key_asda, newDesc);
        }
#endif
#endif
        // reset refcount
        result->flags &= ~(BLOCK_REFCOUNT_MASK|BLOCK_DEALLOCATING);    // XXX not needed
        result->flags |= BLOCK_NEEDS_FREE | 2;  // logical refcount 1
        _call_copy_helpers(result, aBlock);
        // Set isa last so memory analysis tools see a fully-initialized object.
        result->isa = _NSConcreteMallocBlock;
        return result;
    }
}


// Runtime entry points for maintaining the sharing knowledge of byref data blocks.

// A closure has been copied and its fixup routine is asking us to fix up the reference to the shared byref data
// Closures that aren't copied must still work, so everyone always accesses variables after dereferencing the forwarding ptr.
// We ask if the byref pointer that we know about has already been copied to the heap, and if so, increment and return it.
// Otherwise we need to copy it and update the stack forwarding pointer
static struct Block_byref *_Block_byref_copy(const void *arg) {
    struct Block_byref *src = (struct Block_byref *)arg;

    if ((src->forwarding->flags & BLOCK_REFCOUNT_MASK) == 0) {
        // src points to stack
        struct Block_byref *copy = (struct Block_byref *)malloc(src->size);
        copy->isa = NULL;
        // byref value 4 is logical refcount of 2: one for caller, one for stack
        copy->flags = src->flags | BLOCK_BYREF_NEEDS_FREE | 4;
        copy->forwarding = copy; // patch heap copy to point to itself
        src->forwarding = copy;  // patch stack to point to heap copy
        copy->size = src->size;

        if (src->flags & BLOCK_BYREF_HAS_COPY_DISPOSE) {
            // Trust copy helper to copy everything of interest
            // If more than one field shows up in a byref block this is wrong XXX
            struct Block_byref_2 *src2 = (struct Block_byref_2 *)(src+1);
            struct Block_byref_2 *copy2 = (struct Block_byref_2 *)(copy+1);
            copy2->byref_keep = src2->byref_keep;
            copy2->byref_destroy = src2->byref_destroy;

            if (BLOCK_BYREF_LAYOUT(src) == BLOCK_BYREF_LAYOUT_EXTENDED) {
                struct Block_byref_3 *src3 = (struct Block_byref_3 *)(src2+1);
                struct Block_byref_3 *copy3 = (struct Block_byref_3*)(copy2+1);
                copy3->layout = src3->layout;
            }

            (*src2->byref_keep)(copy, src);
        }
        else {
            // Bitwise copy.
            // This copy includes Block_byref_3, if any.
            memmove(copy+1, src+1, src->size - sizeof(*src));
        }
    }
    // already copied to heap
    else if ((src->forwarding->flags & BLOCK_BYREF_NEEDS_FREE) == BLOCK_BYREF_NEEDS_FREE) {
        latching_incr_int(&src->forwarding->flags);
    }
    
    return src->forwarding;
}

static void _Block_byref_release(const void *arg) {
    struct Block_byref *byref = (struct Block_byref *)arg;

    // dereference the forwarding pointer since the compiler isn't doing this anymore (ever?)
    byref = byref->forwarding;
    
    if (byref->flags & BLOCK_BYREF_NEEDS_FREE) {
        int32_t refcount = byref->flags & BLOCK_REFCOUNT_MASK;
        os_assert(refcount);
        if (latching_decr_int_should_deallocate(&byref->flags)) {
            if (byref->flags & BLOCK_BYREF_HAS_COPY_DISPOSE) {
                struct Block_byref_2 *byref2 = (struct Block_byref_2 *)(byref+1);
                (*byref2->byref_destroy)(byref);
            }
            free(byref);
        }
    }
}


/************************************************************
 *
 * API supporting SPI
 * _Block_copy, _Block_release, and (old) _Block_destroy
 *
 ***********************************************************/

#if !TARGET_OS_WIN32
#pragma mark SPI/API
#endif


// API entry point to release a copied Block
void _Block_release(const void *arg) {
    struct Block_layout *aBlock = (struct Block_layout *)arg;
    if (!aBlock) return;
    if (aBlock->flags & BLOCK_IS_GLOBAL) return;
    if (! (aBlock->flags & BLOCK_NEEDS_FREE)) return;

    if (latching_decr_int_should_deallocate(&aBlock->flags)) {
        _call_dispose_helpers(aBlock);
        _Block_destructInstance(aBlock);
        free(aBlock);
    }
}

bool _Block_tryRetain(const void *arg) {
    struct Block_layout *aBlock = (struct Block_layout *)arg;
    return latching_incr_int_not_deallocating(&aBlock->flags);
}

bool _Block_isDeallocating(const void *arg) {
    struct Block_layout *aBlock = (struct Block_layout *)arg;
    return (aBlock->flags & BLOCK_DEALLOCATING) != 0;
}


/************************************************************
 *
 * SPI used by other layers
 *
 ***********************************************************/

size_t Block_size(void *aBlock) {
    auto *layout = (Block_layout *)aBlock;
    void *desc = _Block_get_descriptor(layout);
#if BLOCK_SMALL_DESCRIPTOR_SUPPORTED
    if (layout->flags & BLOCK_SMALL_DESCRIPTOR)
        return ((Block_descriptor_small *)desc)->size;
#endif
    return ((Block_descriptor_1 *)desc)->size;
}

bool _Block_use_stret(void *aBlock) {
    struct Block_layout *layout = (struct Block_layout *)aBlock;

    int requiredFlags = BLOCK_HAS_SIGNATURE | BLOCK_USE_STRET;
    return (layout->flags & requiredFlags) == requiredFlags;
}

// Checks for a valid signature, not merely the BLOCK_HAS_SIGNATURE bit.
bool _Block_has_signature(void *aBlock) {
    return _Block_signature(aBlock) ? true : false;
}

const char * _Block_signature(void *aBlock)
{
    struct Block_layout *layout = (struct Block_layout *)aBlock;
    if (!(layout->flags & BLOCK_HAS_SIGNATURE))
        return nullptr;

#if BLOCK_SMALL_DESCRIPTOR_SUPPORTED
    if (layout->flags & BLOCK_SMALL_DESCRIPTOR) {
        auto *bds = (Block_descriptor_small *)_Block_get_descriptor(layout);
        return unwrap_relative_pointer<const char>(bds->signature);
    }
#endif

    struct Block_descriptor_3 *desc3 = _Block_descriptor_3(layout);
    return desc3->signature;
}

const char * _Block_layout(void *aBlock)
{
    // Don't return extended layout to callers expecting old GC layout
    Block_layout *layout = (Block_layout *)aBlock;
    if ((layout->flags & BLOCK_HAS_EXTENDED_LAYOUT) ||
        !(layout->flags & BLOCK_HAS_SIGNATURE))
        return nullptr;

#if BLOCK_SMALL_DESCRIPTOR_SUPPORTED
    if (layout->flags & BLOCK_SMALL_DESCRIPTOR) {
        auto *bds = (Block_descriptor_small *)_Block_get_descriptor(layout);
        return unwrap_relative_pointer<const char>(bds->layout);
    }
#endif

    Block_descriptor_3 *desc = _Block_descriptor_3(layout);
    return desc->layout;
}

const char * _Block_extended_layout(void *aBlock)
{
    // Don't return old GC layout to callers expecting extended layout
    Block_layout *layout = (Block_layout *)aBlock;
    if (!(layout->flags & BLOCK_HAS_EXTENDED_LAYOUT) ||
        !(layout->flags & BLOCK_HAS_SIGNATURE))
        return nullptr;

    const char *extLayout;
#if BLOCK_SMALL_DESCRIPTOR_SUPPORTED
    if (layout->flags & BLOCK_SMALL_DESCRIPTOR) {
        auto *bds = (Block_descriptor_small *)_Block_get_descriptor(layout);
        if (layout->flags & BLOCK_INLINE_LAYOUT_STRING)
            extLayout = (const char *)(uintptr_t)bds->layout;
        else
            extLayout = unwrap_relative_pointer<const char>(bds->layout);
    } else
#endif
    {
        Block_descriptor_3 *desc3 = _Block_descriptor_3(layout);
        extLayout = desc3->layout;
    }

    // Return empty string (all non-object bytes) instead of NULL 
    // so callers can distinguish "empty layout" from "no layout".
    if (!extLayout)
        extLayout = "";
    return extLayout;
}

#if !TARGET_OS_WIN32
#pragma mark Compiler SPI entry points
#endif

    
/*******************************************************

Entry points used by the compiler - the real API!


A Block can reference four different kinds of things that require help when the Block is copied to the heap.
1) C++ stack based objects
2) References to Objective-C objects
3) Other Blocks
4) __block variables

In these cases helper functions are synthesized by the compiler for use in Block_copy and Block_release, called the copy and dispose helpers.  The copy helper emits a call to the C++ const copy constructor for C++ stack based objects and for the rest calls into the runtime support function _Block_object_assign.  The dispose helper has a call to the C++ destructor for case 1 and a call into _Block_object_dispose for the rest.

The flags parameter of _Block_object_assign and _Block_object_dispose is set to
    * BLOCK_FIELD_IS_OBJECT (3), for the case of an Objective-C Object,
    * BLOCK_FIELD_IS_BLOCK (7), for the case of another Block, and
    * BLOCK_FIELD_IS_BYREF (8), for the case of a __block variable.
If the __block variable is marked weak the compiler also or's in BLOCK_FIELD_IS_WEAK (16)

So the Block copy/dispose helpers should only ever generate the four flag values of 3, 7, 8, and 24.

When  a __block variable is either a C++ object, an Objective-C object, or another Block then the compiler also generates copy/dispose helper functions.  Similarly to the Block copy helper, the "__block" copy helper (formerly and still a.k.a. "byref" copy helper) will do a C++ copy constructor (not a const one though!) and the dispose helper will do the destructor.  And similarly the helpers will call into the same two support functions with the same values for objects and Blocks with the additional BLOCK_BYREF_CALLER (128) bit of information supplied.

So the __block copy/dispose helpers will generate flag values of 3 or 7 for objects and Blocks respectively, with BLOCK_FIELD_IS_WEAK (16) or'ed as appropriate and always 128 or'd in, for the following set of possibilities:
    __block id                   128+3       (0x83)
    __block (^Block)             128+7       (0x87)
    __weak __block id            128+3+16    (0x93)
    __weak __block (^Block)      128+7+16    (0x97)
        

********************************************************/

//
// When Blocks or Block_byrefs hold objects then their copy routine helpers use this entry point
// to do the assignment.
//
void _Block_object_assign(void *destArg, const void *object, const int flags) {
    const void **dest = (const void **)destArg;
    switch (flags & BLOCK_ALL_COPY_DISPOSE_FLAGS) {
      case BLOCK_FIELD_IS_OBJECT:
        /*******
        id object = ...;
        [^{ object; } copy];
        ********/

        _Block_retain_object(object);
        *dest = object;
        break;

      case BLOCK_FIELD_IS_BLOCK:
        /*******
        void (^object)(void) = ...;
        [^{ object; } copy];
        ********/

        *dest = _Block_copy(object);
        break;
    
      case BLOCK_FIELD_IS_BYREF | BLOCK_FIELD_IS_WEAK:
      case BLOCK_FIELD_IS_BYREF:
        /*******
         // copy the onstack __block container to the heap
         // Note this __weak is old GC-weak/MRC-unretained.
         // ARC-style __weak is handled by the copy helper directly.
         __block ... x;
         __weak __block ... x;
         [^{ x; } copy];
         ********/

        *dest = _Block_byref_copy(object);
        break;
        
      case BLOCK_BYREF_CALLER | BLOCK_FIELD_IS_OBJECT:
      case BLOCK_BYREF_CALLER | BLOCK_FIELD_IS_BLOCK:
        /*******
         // copy the actual field held in the __block container
         // Note this is MRC unretained __block only. 
         // ARC retained __block is handled by the copy helper directly.
         __block id object;
         __block void (^object)(void);
         [^{ object; } copy];
         ********/

        *dest = object;
        break;

      case BLOCK_BYREF_CALLER | BLOCK_FIELD_IS_OBJECT | BLOCK_FIELD_IS_WEAK:
      case BLOCK_BYREF_CALLER | BLOCK_FIELD_IS_BLOCK  | BLOCK_FIELD_IS_WEAK:
        /*******
         // copy the actual field held in the __block container
         // Note this __weak is old GC-weak/MRC-unretained.
         // ARC-style __weak is handled by the copy helper directly.
         __weak __block id object;
         __weak __block void (^object)(void);
         [^{ object; } copy];
         ********/

        *dest = object;
        break;

      default:
        break;
    }
}

// When Blocks or Block_byrefs hold objects their destroy helper routines call this entry point
// to help dispose of the contents
void _Block_object_dispose(const void *object, const int flags) {
    switch (flags & BLOCK_ALL_COPY_DISPOSE_FLAGS) {
      case BLOCK_FIELD_IS_BYREF | BLOCK_FIELD_IS_WEAK:
      case BLOCK_FIELD_IS_BYREF:
        // get rid of the __block data structure held in a Block
        _Block_byref_release(object);
        break;
      case BLOCK_FIELD_IS_BLOCK:
        _Block_release(object);
        break;
      case BLOCK_FIELD_IS_OBJECT:
        _Block_release_object(object);
        break;
      case BLOCK_BYREF_CALLER | BLOCK_FIELD_IS_OBJECT:
      case BLOCK_BYREF_CALLER | BLOCK_FIELD_IS_BLOCK:
      case BLOCK_BYREF_CALLER | BLOCK_FIELD_IS_OBJECT | BLOCK_FIELD_IS_WEAK:
      case BLOCK_BYREF_CALLER | BLOCK_FIELD_IS_BLOCK  | BLOCK_FIELD_IS_WEAK:
        break;
      default:
        break;
    }
}
