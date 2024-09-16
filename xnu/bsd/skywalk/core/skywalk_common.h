/*
 * Copyright (c) 2017-2021 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#ifndef _SKYWALK_COMMON_H_
#define _SKYWALK_COMMON_H_

#if defined(PRIVATE) || defined(BSD_KERNEL_PRIVATE)
/*
 * Routines common to kernel and userland.  This file is intended to
 * be included by the Skywalk kernel and libsyscall code.
 */

#include <skywalk/os_skywalk_private.h>

#ifndef KERNEL
#if defined(LIBSYSCALL_INTERFACE)
__BEGIN_DECLS
extern int fprintf_stderr(const char *format, ...);
__END_DECLS

/* CSTYLED */

#define SK_ABORT(msg) do {                                              \
	(void) fprintf_stderr("%s\n", msg);                             \
	__asm__(""); __builtin_trap();                                  \
} while (0)

#define SK_ABORT_WITH_CAUSE(msg, cause) do {                            \
	(void) fprintf_stderr("%s: cause 0x%x\n", msg, cause);          \
	__asm__(""); __builtin_trap();                                  \
} while (0)

#define SK_ABORT_DYNAMIC(msg)   SK_ABORT(msg)


#define VERIFY(EX) do {                                                 \
	if (__improbable(!(EX))) {                                      \
	        SK_ABORT("assertion failed: " #EX);                     \
	/* NOTREACHED */                                        \
	        __builtin_unreachable();                                \
	}                                                               \
} while (0)

#if (DEBUG || DEVELOPMENT)
#define ASSERT(EX)      VERIFY(EX)
#else /* !DEBUG && !DEVELOPMENT */
#define ASSERT(EX)      ((void)0)
#endif /* !DEBUG && !DEVELOPMENT */
#endif /* !LIBSYSCALL_INTERFACE */
#endif /* !KERNEL */

#ifndef container_of
#define container_of(ptr, type, member) \
	((type*)(((uintptr_t)ptr) - offsetof(type, member)))
#endif

/*
 * Prefetch.
 */
#define SK_PREFETCH(a, n) \
	__builtin_prefetch((const void *)((uintptr_t)(a) + (n)), 0, 3)
#define SK_PREFETCHW(a, n) \
	__builtin_prefetch((const void *)((uintptr_t)(a) + (n)), 1, 3)

/*
 * Slower roundup function; if "align" is not power of 2 (else use P2ROUNDUP)
 */
#define SK_ROUNDUP(x, align)    \
	((((x) % (align)) == 0) ? (x) : ((x) + ((align) - ((x) % (align)))))

/* compile time assert */
#ifndef _CASSERT
#define _CASSERT(x)     _Static_assert(x, "compile-time assertion failed")
#endif /* !_CASSERT */

/* power of 2 address alignment */
#ifndef IS_P2ALIGNED
#define IS_P2ALIGNED(v, a)      \
	((((uintptr_t)(v)) & ((uintptr_t)(a) - 1)) == 0)
#endif /* IS_P2ALIGNED */

#define __sk_aligned(a) __attribute__((__aligned__(a)))
#define __sk_packed     __attribute__((__packed__))
#define __sk_unused     __attribute__((__unused__))

#ifdef KERNEL
#include <sys/sdt.h>

/*
 * Copy 8-bytes total, 64-bit aligned, scalar.
 */
__attribute__((always_inline))
static inline void
__sk_copy64_8(uint64_t *src, uint64_t *dst)
{
	*dst = *src;            /* [#0*8] */
}

/*
 * Copy 8-bytes total, 32-bit aligned, scalar.
 */
__attribute__((always_inline))
static inline void
__sk_copy32_8(uint32_t *__counted_by(2)src, uint32_t *__counted_by(2)dst)
{
#if defined(__x86_64__)
	/* use unaligned scalar move on x86_64 */
	__sk_copy64_8((uint64_t *)(void *)src, (uint64_t *)(void *)dst);
#else
	dst[0] = src[0]; /* dw[0] */
	dst[1] = src[1]; /* dw[1] */
#endif
}

/*
 * Copy 16-bytes total, 64-bit aligned, scalar.
 */
static inline void
__sk_copy64_16(uint64_t *__counted_by(2) src, uint64_t *__counted_by(2) dst)
{
	dst[0] = src[0]; /* [#0*8] */
	dst[1] = src[1]; /* [#1*8] */
}

/*
 * Copy 16-bytes total, 32-bit aligned, scalar.
 */
__attribute__((always_inline))
static inline void
__sk_copy32_16(uint32_t *__counted_by(4) src, uint32_t *__counted_by(4) dst)
{
	dst[0] = src[0]; /* [#0*4] */
	dst[1] = src[1]; /* [#1*4] */
	dst[2] = src[2]; /* [#2*4] */
	dst[3] = src[3]; /* [#3*4] */
}

/*
 * Copy 20-bytes total, 64-bit aligned, scalar.
 */
__attribute__((always_inline))
static inline void
__sk_copy64_20(uint64_t *__sized_by(20) src, uint64_t *__sized_by(20) dst)
{
	dst[0] = src[0]; /* [#0*8] */
	dst[1] = src[1]; /* [#1*8] */
	*(uint32_t *)(dst + 2) = *(uint32_t *)(src + 2); /* [#2*4] */
}

/*
 * Copy 24-bytes total, 64-bit aligned, scalar.
 */
__attribute__((always_inline))
static inline void
__sk_copy64_24(uint64_t *__counted_by(3) src, uint64_t *__counted_by(3) dst)
{
	dst[0] = src[0]; /* [#0*8] */
	dst[1] = src[1]; /* [#1*8] */
	dst[2] = src[2]; /* [#2*8] */
}

/*
 * Copy 32-bytes total, 64-bit aligned, scalar.
 */
__attribute__((always_inline))
static inline void
__sk_copy64_32(uint64_t *__counted_by(4) src, uint64_t *__counted_by(4) dst)
{
	dst[0] = src[0]; /* [#0*8] */
	dst[1] = src[1]; /* [#1*8] */
	dst[2] = src[2]; /* [#2*8] */
	dst[3] = src[3]; /* [#3*8] */
}

/*
 * Copy 32-bytes total, 32-bit aligned, scalar.
 */
__attribute__((always_inline))
static inline void
__sk_copy32_32(uint32_t *__counted_by(8) src, uint32_t *__counted_by(8) dst)
{
	dst[0] = src[0]; /* [#0*4] */
	dst[1] = src[1]; /* [#1*4] */
	dst[2] = src[2]; /* [#2*4] */
	dst[3] = src[3]; /* [#3*4] */
	dst[4] = src[4]; /* [#4*4] */
	dst[5] = src[5]; /* [#5*4] */
	dst[6] = src[6]; /* [#6*4] */
	dst[7] = src[7]; /* [#7*4] */
}

/*
 * Copy 40-bytes total, 64-bit aligned, scalar.
 */
__attribute__((always_inline))
static inline void
__sk_copy64_40(uint64_t *__sized_by(40) src, uint64_t *__sized_by(40) dst)
{
	dst[0] = src[0]; /* [#0*8] */
	dst[1] = src[1]; /* [#1*8] */
	dst[2] = src[2]; /* [#2*8] */
	dst[3] = src[3]; /* [#3*8] */
	dst[4] = src[4]; /* [#4*8] */
}

#if defined(__arm64__)
/*
 * Copy 16-bytes total, 64-bit aligned, SIMD (if available).
 */
__attribute__((always_inline))
static inline void
__sk_vcopy64_16(uint64_t *__counted_by(2) src, uint64_t *__counted_by(2) dst)
{
	/* no need to save/restore registers on arm64 (SPILL_REGISTERS) */
	/* BEGIN CSTYLED */
	__asm__ __volatile__ (
                "ldr	q0, [%[src]]		\n\t"
                "str	q0, [%[dst]]		\n\t"
                :
                : [src] "r" ((uint64_t *__unsafe_indexable)src), [dst] "r" ((uint64_t *__unsafe_indexable)dst)
                : "v0", "memory"
        );
	/* END CSTYLED */
}

/*
 * Copy 16-bytes total, 32-bit aligned, SIMD (if available).
 */
__attribute__((always_inline))
static inline void
__sk_vcopy32_16(uint32_t *__counted_by(4) src, uint32_t *__counted_by(4) dst)
{
	/* use SIMD unaligned move on arm64 */
	__sk_vcopy64_16((uint64_t *)(void *)src, (uint64_t *)(void *)dst);
}

/*
 * Copy 20-bytes total, 64-bit aligned, SIMD (if available).
 */
__attribute__((always_inline))
static inline void
__sk_vcopy64_20(uint64_t *__sized_by(20) src, uint64_t *__sized_by(20) dst)
{
	/*
	 * Load/store 16 + 4 bytes;
	 * no need to save/restore registers on arm64 (SPILL_REGISTERS).
	 */
	/* BEGIN CSTYLED */
	__asm__ __volatile__ (
                "ldr	q0, [%[src]]		\n\t"
                "str	q0, [%[dst]]		\n\t"
                "ldr	s0, [%[src], #16]	\n\t"
                "str	s0, [%[dst], #16]	\n\t"
                :
                : [src] "r" ((uint64_t *__unsafe_indexable)src), [dst] "r" ((uint64_t *__unsafe_indexable)dst)
                : "v0", "memory"
        );
	/* END CSTYLED */
}

/*
 * Copy 24-bytes total, 64-bit aligned, SIMD (if available).
 */
__attribute__((always_inline))
static inline void
__sk_vcopy64_24(uint64_t *__counted_by(3) src, uint64_t *__counted_by(3) dst)
{
	/*
	 * Use 16-bytes load/store and 8-bytes load/store on arm64;
	 * no need to save/restore registers on arm64 (SPILL_REGISTERS).
	 */
	/* BEGIN CSTYLED */
	__asm__ __volatile__ (
                "ldr	q0, [%[src]]		\n\t"
                "str	q0, [%[dst]]		\n\t"
                "ldr	d0, [%[src], #16]	\n\t"
                "str	d0, [%[dst], #16]	\n\t"
                :
                : [src] "r" ((uint64_t *__unsafe_indexable)src), [dst] "r" ((uint64_t *__unsafe_indexable)dst)
                : "v0", "memory"
        );
	/* END CSTYLED */
}

/*
 * Copy 32-bytes total, 64-bit aligned, SIMD (if available).
 */
__attribute__((always_inline))
static inline void
__sk_vcopy64_32(uint64_t *__counted_by(4) src, uint64_t *__counted_by(4) dst)
{
	/* no need to save/restore registers on arm64 (SPILL_REGISTERS) */
	/* BEGIN CSTYLED */
	__asm__ __volatile__ (
                "ldp	q0, q1, [%[src]]	\n\t"
                "stp	q0, q1, [%[dst]]	\n\t"
                :
                : [src] "r" ((uint64_t *__unsafe_indexable)src), [dst] "r" ((uint64_t *__unsafe_indexable)dst)
                : "v0", "v1", "memory"
        );
	/* END CSTYLED */
}

/*
 * Copy 32-bytes total, 32-bit aligned, SIMD (if available).
 */
__attribute__((always_inline))
static inline void
__sk_vcopy32_32(uint32_t *__counted_by(8) src, uint32_t *__counted_by(8) dst)
{
	/* use SIMD unaligned move on arm64 */
	__sk_vcopy64_32((uint64_t *)(void *)src, (uint64_t *)(void *)dst);
}

/*
 * Copy 40-bytes total, 64-bit aligned, SIMD (if available).
 */
__attribute__((always_inline))
static inline void
__sk_vcopy64_40(uint64_t *__sized_by(40) src, uint64_t *__sized_by(40) dst)
{
	/*
	 * Use 32-bytes load/store pair and 8-bytes load/store on arm64;
	 * no need to save/restore registers on arm64 (SPILL_REGISTERS).
	 */
	/* BEGIN CSTYLED */
	__asm__ __volatile__ (
                "ldp	q0, q1, [%[src]]	\n\t"
                "stp	q0, q1, [%[dst]]	\n\t"
                "ldr	d0, [%[src], #32]	\n\t"
                "str	d0, [%[dst], #32]	\n\t"
                :
                : [src] "r" ((uint64_t *__unsafe_indexable)src), [dst] "r" ((uint64_t *__unsafe_indexable)dst)
                : "v0", "v1", "memory"
        );
	/* END CSTYLED */
}

/*
 * On arm64, the following inline assembly fixed-length routines have
 * fewer clock cycles than bzero().  We can directly use vector registers
 * without saving/restoring them unlike on x86_64/arm32.
 */

/*
 * Zero 16-bytes total, SIMD.
 */
__attribute__((always_inline))
static inline void
__sk_zero_16(void *p)
{
	/*
	 * Use 16-bytes store pair using 64-bit zero register on arm64;
	 * no need to save/restore registers on arm64 (SPILL_REGISTERS).
	 */
	/* BEGIN CSTYLED */
	__asm__ __volatile__ (
                "stp	xzr, xzr, [%[p]]	\n\t"
                :
                : [p] "r" (p)
                : "memory"
        );
	/* END CSTYLED */
}

/*
 * Zero 32-bytes total, SIMD.
 */
__attribute__((always_inline))
static inline void
__sk_zero_32(void *p)
{
	/*
	 * Use 32-bytes store pair using zeroed v0 register on arm64;
	 * no need to save/restore registers on arm64 (SPILL_REGISTERS).
	 */
	/* BEGIN CSTYLED */
	__asm__ __volatile__ (
                "eor.16b v0, v0, v0		\n\t"
                "stp	 q0, q0, [%[p]]		\n\t"
                :
                : [p] "r" (p)
                : "v0", "memory", "cc"
        );
	/* END CSTYLED */
}

/*
 * Zero 48-bytes total, SIMD.
 */
__attribute__((always_inline))
static inline void
__sk_zero_48(void *p)
{
	/*
	 * Use 32-bytes store pair and 16-byte store using zeroed v0
	 * register on arm64; no need to save/restore registers on
	 * arm64 (SPILL_REGISTERS).
	 */
	/* BEGIN CSTYLED */
	__asm__ __volatile__ (
                "eor.16b v0, v0, v0		\n\t"
                "stp	 q0, q0, [%[p]]		\n\t"
                "str	 q0, [%[p], #32]	\n\t"
                :
                : [p] "r" (p)
                : "v0", "memory", "cc"
        );
	/* END CSTYLED */
}

/*
 * Zero 128-bytes total, SIMD.
 */
__attribute__((always_inline))
static inline void
__sk_zero_128(void *p)
{
	/*
	 * Use 4x 32-bytes store pairs using zeroed v0 register on arm64;
	 * no need to save/restore registers on arm64 (SPILL_REGISTERS).
	 *
	 * Note that we could optimize this routine by utilizing "dc zva"
	 * which zeroes the entire cache line.  However, that requires
	 * us to guarantee that the address is cache line aligned which
	 * we cannot (at the moment).
	 */
	/* BEGIN CSTYLED */
	__asm__ __volatile__ (
                "eor.16b v0, v0, v0		\n\t"
                "stp	 q0, q0, [%[p]]		\n\t"
                "stp	 q0, q0, [%[p], #32]	\n\t"
                "stp	 q0, q0, [%[p], #64]	\n\t"
                "stp	 q0, q0, [%[p], #96]	\n\t"
                :
                : [p] "r" (p)
                : "v0", "memory", "cc"
        );
	/* END CSTYLED */
}
#else /* !__arm64__ */
/*
 * Just use bzero() for simplicity.  On x86_64, "rep stosb" microcoded
 * implementation already uses wider stores and can go much faster than
 * one byte per clock cycle.  For arm32, bzero() is also good enough.
 */
#define __sk_zero_16(_p)        bzero(_p, 16)
#define __sk_zero_32(_p)        bzero(_p, 32)
#define __sk_zero_48(_p)        bzero(_p, 48)
#define __sk_zero_128(_p)       bzero(_p, 128)
#endif /* !__arm64__ */

/*
 * The following are optimized routines which rely on the caller
 * rounding up the source and destination buffers to multiples of
 * 4, 8 or 64 bytes, and are 64-bit aligned; faster than memcpy().
 *
 * Note: they do not support overlapping ranges.
 */

/*
 * Threshold as to when we use memcpy() rather than unrolled copy.
 */
#if defined(__x86_64__)
#define SK_COPY_THRES 2048
#elif defined(__arm64__)
#define SK_COPY_THRES 1024
#else /* !__x86_64__ && !__arm64__ */
#define SK_COPY_THRES 1024
#endif /* !__x86_64__ && !__arm64__ */

#if (DEVELOPMENT || DEBUG)
extern size_t sk_copy_thres;
#endif /* (DEVELOPMENT || DEBUG) */

/*
 * Scalar version, 4-bytes multiple.
 */
__attribute__((always_inline))
static inline void
sk_copy64_4x(uint32_t *__sized_by(l)src, uint32_t *__sized_by(l)dst, size_t l)
{
#if (DEVELOPMENT || DEBUG)
	if (__probable(l <= sk_copy_thres)) {
#else
	if (__probable(l <= SK_COPY_THRES)) {
#endif /* (!DEVELOPMENT && !DEBUG! */
		int i;

		/*
		 * Clang is unable to optimize away bounds checks in the presence of
		 * divisions in the loop bound at this time. However, the caller
		 * already bounds-checked that each of `src` and `dst` have `l` bytes
		 * at them each. It's therefore safe to copy that many bytes.
		 */
		uint32_t *__unsafe_indexable src_unsafe = src;
		uint32_t *__unsafe_indexable dst_unsafe = dst;
		for (i = 0; i < l / 4; i++) {
			dst_unsafe[i] = src_unsafe[i]; /* [#i*4] */
		}
	} else {
		(void) memcpy((void *)dst, (void *)src, l);
	}
}

/*
 * Scalar version, 8-bytes multiple.
 */
__attribute__((always_inline))
static inline void
sk_copy64_8x(uint64_t *__sized_by(l)src, uint64_t *__sized_by(l)dst, size_t l)
{
#if (DEVELOPMENT || DEBUG)
	if (__probable(l <= sk_copy_thres)) {
#else
	if (__probable(l <= SK_COPY_THRES)) {
#endif /* (!DEVELOPMENT && !DEBUG! */
		int i;

		/*
		 * Clang is unable to optimize away bounds checks in the presence of
		 * divisions in the loop bound at this time. However, the caller
		 * already bounds-checked that each of `src` and `dst` have `l` bytes
		 * at them each. It's therefore safe to copy that many bytes.
		 */
		uint64_t *__unsafe_indexable src_unsafe = src;
		uint64_t *__unsafe_indexable dst_unsafe = dst;
		for (i = 0; i < l / 8; i++) {
			dst_unsafe[i] = src_unsafe[i]; /* [#i*8] */
		}
	} else {
		(void) memcpy((void *)dst, (void *)src, l);
	}
}

/*
 * Scalar version (usually faster than SIMD), 32-bytes multiple.
 */
__attribute__((always_inline))
static inline void
sk_copy64_32x(uint64_t *__sized_by(l)src, uint64_t *__sized_by(l)dst, size_t l)
{
#if (DEVELOPMENT || DEBUG)
	if (__probable(l <= sk_copy_thres)) {
#else
	if (__probable(l <= SK_COPY_THRES)) {
#endif /* (!DEVELOPMENT && !DEBUG! */
		int n, i;

		/*
		 * Clang is unable to optimize away bounds checks in the presence of
		 * divisions in the loop bound at this time. However, the caller
		 * already bounds-checked that each of `src` and `dst` have `l` bytes
		 * at them each. It's therefore safe to copy that many bytes.
		 */
		uint64_t *__unsafe_indexable src_unsafe = src;
		uint64_t *__unsafe_indexable dst_unsafe = dst;
		for (n = 0; n < l / 32; n++) {
			i = n * 4;
			dst_unsafe[i] = src_unsafe[i];         /* [#(i+0)*8] */
			dst_unsafe[i + 1] = src_unsafe[i + 1]; /* [#(i+1)*8] */
			dst_unsafe[i + 2] = src_unsafe[i + 2]; /* [#(i+2)*8] */
			dst_unsafe[i + 3] = src_unsafe[i + 3]; /* [#(i+3)*8] */
		}
	} else {
		(void) memcpy((void *)dst, (void *)src, l);
	}
}

/*
 * Scalar version (usually faster than SIMD), 64-bytes multiple.
 */
__attribute__((always_inline))
static inline void
sk_copy64_64x(uint64_t *__sized_by(l)src, uint64_t *__sized_by(l)dst, size_t l)
{
#if (DEVELOPMENT || DEBUG)
	if (__probable(l <= sk_copy_thres)) {
#else
	if (__probable(l <= SK_COPY_THRES)) {
#endif /* (!DEVELOPMENT && !DEBUG! */
		int n, i;

		/*
		 * Clang is unable to optimize away bounds checks in the presence of
		 * divisions in the loop bound at this time. However, the caller
		 * already bounds-checked that each of `src` and `dst` have `l` bytes
		 * at them each. It's therefore safe to copy that many bytes.
		 */
		uint64_t *__unsafe_indexable src_unsafe = src;
		uint64_t *__unsafe_indexable dst_unsafe = dst;
		for (n = 0; n < l / 64; n++) {
			i = n * 8;
			dst_unsafe[i] = src_unsafe[i];         /* [#(i+0)*8] */
			dst_unsafe[i + 1] = src_unsafe[i + 1]; /* [#(i+1)*8] */
			dst_unsafe[i + 2] = src_unsafe[i + 2]; /* [#(i+2)*8] */
			dst_unsafe[i + 3] = src_unsafe[i + 3]; /* [#(i+3)*8] */
			dst_unsafe[i + 4] = src_unsafe[i + 4]; /* [#(i+4)*8] */
			dst_unsafe[i + 5] = src_unsafe[i + 5]; /* [#(i+5)*8] */
			dst_unsafe[i + 6] = src_unsafe[i + 6]; /* [#(i+6)*8] */
			dst_unsafe[i + 7] = src_unsafe[i + 7]; /* [#(i+7)*8] */
		}
	} else {
		(void) memcpy((void *)dst, (void *)src, l);
	}
}

/*
 * Use scalar or SIMD based on platform/size.
 */
#if defined(__x86_64__)
#define sk_copy64_8     __sk_copy64_8           /* scalar only */
#define sk_copy32_8     __sk_copy32_8           /* scalar only */
#define sk_copy64_16    __sk_copy64_16          /* scalar */
#define sk_copy32_16    __sk_copy32_16          /* scalar */
#define sk_copy64_20    __sk_copy64_20          /* scalar */
#define sk_copy64_24    __sk_copy64_24          /* scalar */
#define sk_copy64_32    __sk_copy64_32          /* scalar */
#define sk_copy32_32    __sk_copy32_32          /* scalar */
#define sk_copy64_40    __sk_copy64_40          /* scalar */
#define sk_zero_16      __sk_zero_16            /* scalar */
#define sk_zero_32      __sk_zero_32            /* scalar */
#define sk_zero_48      __sk_zero_48            /* scalar */
#define sk_zero_128     __sk_zero_128           /* scalar */
#elif defined(__arm64__)
#define sk_copy64_8     __sk_copy64_8           /* scalar only */
#define sk_copy32_8     __sk_copy32_8           /* scalar only */
#define sk_copy64_16    __sk_vcopy64_16         /* SIMD */
#define sk_copy32_16    __sk_vcopy32_16         /* SIMD */
#define sk_copy64_20    __sk_vcopy64_20         /* SIMD */
#define sk_copy64_24    __sk_vcopy64_24         /* SIMD */
#define sk_copy64_32    __sk_vcopy64_32         /* SIMD */
#define sk_copy32_32    __sk_vcopy32_32         /* SIMD */
#define sk_copy64_40    __sk_vcopy64_40         /* SIMD */
#define sk_zero_16      __sk_zero_16            /* SIMD */
#define sk_zero_32      __sk_zero_32            /* SIMD */
#define sk_zero_48      __sk_zero_48            /* SIMD */
#define sk_zero_128     __sk_zero_128           /* SIMD */
#else
#define sk_copy64_8     __sk_copy64_8           /* scalar only */
#define sk_copy32_8     __sk_copy32_8           /* scalar only */
#define sk_copy64_16    __sk_copy64_16          /* scalar */
#define sk_copy32_16    __sk_copy32_16          /* scalar */
#define sk_copy64_20    __sk_copy64_20          /* scalar */
#define sk_copy64_24    __sk_copy64_24          /* scalar */
#define sk_copy64_32    __sk_copy64_32          /* scalar */
#define sk_copy32_32    __sk_copy32_32          /* scalar */
#define sk_copy64_40    __sk_copy64_40          /* scalar */
#define sk_zero_16      __sk_zero_16            /* scalar */
#define sk_zero_32      __sk_zero_32            /* scalar */
#define sk_zero_48      __sk_zero_48            /* scalar */
#define sk_zero_128     __sk_zero_128           /* scalar */
#endif

/*
 * Do not use these directly.
 * Use the skn_ variants if you need custom probe names.
 */
#define _sk_alloc_type(probename, type, flags, name)                    \
({                                                                      \
	/* XXX Modify this to use KT_PRIV_ACCT later  */                \
	__auto_type ret = kalloc_type_tag(type, Z_ZERO | (flags),       \
	    (name)->tag);                                               \
	DTRACE_SKYWALK3(probename, char *, #type, int, (flags),         \
	    void *, ret);                                               \
	ret;                                                            \
})

#define _sk_alloc_type_array(probename, type, count, flags, name)       \
({                                                                      \
	__auto_type ret = kalloc_type_tag(type, (count),                \
	    Z_ZERO | (flags), (name)->tag);                             \
	DTRACE_SKYWALK4(probename, char *, #type, size_t, (count),      \
	    int, (flags), void *, ret);                                 \
	ret;                                                            \
})

#define _sk_alloc_type_hash(probename, heap, size, flags, name)         \
({                                                                      \
	__auto_type ret = kalloc_type_var_impl((heap), (size),          \
	    __zone_flags_mix_tag((flags) | Z_ZERO, (name)->tag), NULL); \
	DTRACE_SKYWALK4(probename, char *, (heap)->kt_name + 5,         \
	    size_t, (size), int, (flags), void *, ret);                 \
	ret;                                                            \
})

#define _sk_realloc_type_array(probename, type, oldcount, newcount, elem, flags, name) \
({                                                                      \
	__auto_type ret = krealloc_type_tag(type, (oldcount),           \
	    (newcount), (elem), Z_ZERO | (flags), (name)->tag);         \
	DTRACE_SKYWALK5(probename, void *, (elem), size_t, (oldcount),  \
	    size_t, (newcount), int, (flags), void *, ret);             \
	ret;                                                            \
})

#define _sk_alloc_type_header_array(probename, htype, type, count, flags, name) \
({                                                                      \
	__auto_type ret = kalloc_type_tag(htype, type, (count),         \
	    Z_ZERO | (flags), (name)->tag);                             \
	DTRACE_SKYWALK5(probename, char *, #htype, char *, #type,       \
	    size_t, (count), int, (flags), void *, ret);                \
	ret;                                                            \
})

#define _sk_free_type(probename, type, elem)                            \
{                                                                       \
	DTRACE_SKYWALK2(probename, char *, #type, void *, (elem));      \
	kfree_type(type, (elem));                                       \
}

#define _sk_free_type_array(probename, type, count, elem)               \
{                                                                       \
	DTRACE_SKYWALK3(probename, char *, #type, size_t, (count),      \
	    void *, (elem));                                            \
	kfree_type(type, (count), (elem));                              \
}

#define _sk_free_type_array_counted_by(probename, type, count, elem)    \
{                                                                       \
	DTRACE_SKYWALK3(probename, char *, #type, size_t, (count),      \
	    void *, (elem));                                            \
	kfree_type_counted_by(type, (count), (elem));                   \
}

#define _sk_free_type_hash(probename, heap, size, elem)                 \
{                                                                       \
	DTRACE_SKYWALK3(probename, char *, (heap)->kt_name + 5,         \
	    size_t, (size), void *, (elem));                            \
	kfree_type_var_impl((heap), (elem), (size));                    \
}

#define _sk_free_type_header_array(probename, htype, type, count, elem) \
{                                                                       \
	DTRACE_SKYWALK4(probename, char *, #htype, char *, #type,       \
	    size_t, (count), void *, (elem));                           \
	kfree_type(htype, type, (count), (elem));                       \
}

#define _sk_alloc_data(probename, size, flags, name)                    \
({                                                                      \
	void *ret;                                                      \
                                                                        \
	ret = kalloc_data_tag((size), Z_ZERO | (flags), (name)->tag);   \
	DTRACE_SKYWALK3(probename, size_t, (size), int, (flags),        \
	    void *, ret);                                               \
	ret;                                                            \
})

#define _sk_realloc_data(probename, elem, oldsize, newsize, flags, name) \
({                                                                      \
	void *ret;                                                      \
                                                                        \
	ret = krealloc_data_tag((elem), (oldsize), (newsize),           \
	    Z_ZERO | (flags), (name)->tag);                             \
	DTRACE_SKYWALK5(probename, void *, (elem), size_t, (oldsize),   \
	    size_t, (newsize), int, (flags), void *, ret);              \
	ret;                                                            \
})

#define _sk_free_data(probename, elem, size)                            \
{                                                                       \
	DTRACE_SKYWALK2(probename, void *, (elem), size_t, (size));     \
	kfree_data((elem), (size));                                     \
}

#define _sk_free_data_sized_by(probename, elem, size)                   \
{                                                                       \
	DTRACE_SKYWALK2(probename, void *, (elem), size_t, (size));     \
	kfree_data_sized_by((elem), (size));                            \
}

#define sk_alloc_type(type, flags, tag)                                 \
	_sk_alloc_type(sk_alloc_type, type, flags, tag)

#define sk_alloc_type_array(type, count, flags, tag)                    \
	_sk_alloc_type_array(sk_alloc_type_array, type, count, flags, tag)

#define sk_alloc_type_hash(heap, size, flags, tag)                      \
	_sk_alloc_type_hash(sk_alloc_type_hash, heap, size, flags, tag)

#define sk_alloc_type_header_array(htype, type, count, flags, tag)      \
	_sk_alloc_type_header_array(sk_alloc_type_header_array, htype,  \
	type, count, flags, tag)

#define sk_realloc_type_array(type, oldsize, newsize, elem, flags, tag) \
	_sk_realloc_type_array(sk_realloc_type_array, type,             \
	oldsize, newsize, elem, flags, tag)

#define sk_free_type(type, elem)                                        \
	_sk_free_type(sk_free_type, type, elem)

#define sk_free_type_array(type, count, elem)                           \
	_sk_free_type_array(sk_free_type_array, type, count, elem)

#define sk_free_type_array_counted_by(type, count, elem)                \
	_sk_free_type_array_counted_by(sk_free_type_array_counted_by, type, count, elem)

#define sk_free_type_hash(heap, size, elem)                             \
	_sk_free_type_hash(sk_free_type_hash, heap, size, elem)

#define sk_free_type_header_array(htype, type, count, elem)             \
	_sk_free_type_header_array(sk_free_type_header_array, htype,    \
	type, count, elem)

#define sk_alloc_data(size, flags, tag)                                 \
	_sk_alloc_data(sk_alloc_data, size, flags, tag)

#define sk_realloc_data(elem, oldsize, newsize, flags, tag)             \
	_sk_realloc_data(sk_realloc_data, elem, oldsize, newsize,       \
	flags, tag)

#define sk_free_data(elem, size)                                        \
	_sk_free_data(sk_free_data, elem, size)

#define sk_free_data_sized_by(elem, size)                               \
	_sk_free_data_sized_by(sk_free_data_sized_by, elem, size)

/*
 * The skn_ variants are meant to be used if you need to use two or more
 * of the same call within the same function and you want the dtrace
 * probename to be different at each callsite.
 */
#define skn_realloc(name, elem, oldsize, newsize, flags, tag)           \
	_sk_realloc(sk_realloc_ ## name, elem, oldsize, newsize, flags, \
	tag)

#define skn_alloc_type(name, type, flags, tag)                          \
	_sk_alloc_type(sk_alloc_type_ ## name, type, flags, tag)

#define skn_alloc_type_array(name, type, count, flags, tag)             \
	_sk_alloc_type_array(sk_alloc_type_array_ ## name, type, count, \
	flags, tag)

#define skn_alloc_type_hash(name, heap, size, flags, tag)               \
	_sk_alloc_type_hash(sk_alloc_type_hash_ ## name, heap, size,    \
	flags, tag)

#define skn_alloc_type_header_array(name, htype, type, count, flags, tag) \
	_sk_alloc_type_header_array(sk_alloc_type_header_array_ ## name, \
	htype, type, count, flags, tag)

#define skn_free_type(name, type, elem)                                 \
	_sk_free_type(sk_free_type_ ## name, type, elem)

#define skn_free_type_array(name, type, count, elem)                    \
	_sk_free_type_array(sk_free_type_array_ ## name, type, count,   \
	elem)

#define skn_free_type_array_counted_by(name, type, count, elem)         \
	_sk_free_type_array_counted_by(sk_free_type_array_ ## name, type, count,   \
	elem)

#define skn_free_type_hash(name, heap, size, elem)                      \
	_sk_free_type_hash(sk_free_type_hash_ ## name, heap, size, elem)

#define skn_free_type_header_array(name, htype, type, count, elem)      \
	_sk_free_type_header_array(sk_free_type_header_array_ ## name,  \
	htype, type, count, elem)

#define skn_alloc_data(name, size, flags, tag)                          \
	_sk_alloc_data(sk_alloc_data_ ## name, size, flags, tag)

#define skn_realloc_data(name, elem, oldsize, newsize, flags, tag)      \
	_sk_realloc_data(sk_realloc_data_ ## name, elem, oldsize, newsize,\
	flags, tag)

#define skn_free_data(name, elem, size)                                 \
	_sk_free_data(sk_free_data_ ## name, elem, size)

struct sk_tag_spec {
	kern_allocation_name_t *skt_var;
	const char             *skt_name;
};

extern void __sk_tag_make(const struct sk_tag_spec *spec);

#define SKMEM_TAG_DEFINE(var, name) \
	SECURITY_READ_ONLY_LATE(kern_allocation_name_t) var;            \
	__startup_data struct sk_tag_spec __sktag_##var = {             \
	    .skt_var = &var, .skt_name = name,                          \
	};                                                              \
	STARTUP_ARG(ZALLOC, STARTUP_RANK_LAST, __sk_tag_make, &__sktag_##var)

/*!
 *  @abstract Compare byte buffers of n bytes long src1 against src2, applying
 *  the byte masks to input data before comparison.  (Scalar version)
 *
 *  @discussion
 *  Returns zero if the two buffers are identical after applying the byte
 *  masks, otherwise non-zero.
 *  Zero-length buffers are always identical.
 *
 *  @param src1 first input buffer of n bytes long
 *  @param src2 second input buffer of n bytes long
 *  @param byte_mask byte mask of n bytes long applied before comparision
 *  @param n number of bytes
 */
static inline int
__sk_memcmp_mask_scalar(const uint8_t *__counted_by(n)src1,
    const uint8_t *__counted_by(n)src2,
    const uint8_t *__counted_by(n)byte_mask, size_t n)
{
	uint32_t result = 0;
	for (size_t i = 0; i < n; i++) {
		result |= (src1[i] ^ src2[i]) & byte_mask[i];
	}
	return result;
}

static inline int
__sk_memcmp_mask_16B_scalar(const uint8_t *__counted_by(16)src1,
    const uint8_t *__counted_by(16)src2,
    const uint8_t *__counted_by(16)byte_mask)
{
	return __sk_memcmp_mask_scalar(src1, src2, byte_mask, 16);
}

static inline int
__sk_memcmp_mask_32B_scalar(const uint8_t *__counted_by(32)src1,
    const uint8_t *__counted_by(32)src2,
    const uint8_t *__counted_by(32)byte_mask)
{
	return __sk_memcmp_mask_scalar(src1, src2, byte_mask, 32);
}

static inline int
__sk_memcmp_mask_48B_scalar(const uint8_t *__counted_by(48)src1,
    const uint8_t *__counted_by(48)src2,
    const uint8_t *__counted_by(48)byte_mask)
{
	return __sk_memcmp_mask_scalar(src1, src2, byte_mask, 48);
}

static inline int
__sk_memcmp_mask_64B_scalar(const uint8_t *__counted_by(64)src1,
    const uint8_t *__counted_by(64)src2,
    const uint8_t *__counted_by(64)byte_mask)
{
	return __sk_memcmp_mask_scalar(src1, src2, byte_mask, 64);
}

static inline int
__sk_memcmp_mask_80B_scalar(const uint8_t *__counted_by(80)src1,
    const uint8_t *__counted_by(80)src2,
    const uint8_t *__counted_by(80)byte_mask)
{
	return __sk_memcmp_mask_scalar(src1, src2, byte_mask, 80);
}

#if defined(__arm64__) || defined(__arm__) || defined(__x86_64__)
extern int os_memcmp_mask_16B(const uint8_t *__counted_by(16)src1,
    const uint8_t *__counted_by(16)src2,
    const uint8_t *__counted_by(16)byte_mask);
extern int os_memcmp_mask_32B(const uint8_t *__counted_by(32)src1,
    const uint8_t *__counted_by(32)src2,
    const uint8_t *__counted_by(32)byte_mask);
extern int os_memcmp_mask_48B(const uint8_t *__counted_by(48)src1,
    const uint8_t *__counted_by(48)src2,
    const uint8_t *__counted_by(48)byte_mask);
extern int os_memcmp_mask_64B(const uint8_t *__counted_by(64)src1,
    const uint8_t *__counted_by(64)src2,
    const uint8_t *__counted_by(64)byte_mask);
extern int os_memcmp_mask_80B(const uint8_t *__counted_by(80)src1,
    const uint8_t *__counted_by(80)src2,
    const uint8_t *__counted_by(80)byte_mask);

/*
 * Use SIMD variants based on ARM64 and x86_64.
 */
#define sk_memcmp_mask                  __sk_memcmp_mask
#define sk_memcmp_mask_16B              os_memcmp_mask_16B
#define sk_memcmp_mask_32B              os_memcmp_mask_32B
#define sk_memcmp_mask_48B              os_memcmp_mask_48B
#define sk_memcmp_mask_64B              os_memcmp_mask_64B
#define sk_memcmp_mask_80B              os_memcmp_mask_80B

/*!
 *  @abstract Compare byte buffers of n bytes long src1 against src2, applying
 *  the byte masks to input data before comparison.  (SIMD version)
 *
 *  @discussion
 *  Returns zero if the two buffers are identical after applying the byte
 *  masks, otherwise non-zero.
 *  Zero-length buffers are always identical.
 *
 *  @param src1 first input buffer of n bytes long
 *  @param src2 second input buffer of n bytes long
 *  @param byte_mask byte mask of n bytes long applied before comparision
 *  @param n number of bytes
 */
static inline int
__sk_memcmp_mask(const uint8_t *__counted_by(n)src1,
    const uint8_t *__counted_by(n)src2,
    const uint8_t *__counted_by(n)byte_mask, size_t n)
{
	uint32_t result = 0;
	size_t i = 0;
	for (; i + 64 <= n; i += 64) {
		result |= sk_memcmp_mask_64B(src1 + i, src2 + i,
		    byte_mask + i);
	}
	for (; i + 32 <= n; i += 32) {
		result |= sk_memcmp_mask_32B(src1 + i, src2 + i,
		    byte_mask + i);
	}
	for (; i + 16 <= n; i += 16) {
		result |= sk_memcmp_mask_16B(src1 + i, src2 + i,
		    byte_mask + i);
	}
	if (i < n) {
		if (n >= 16) {
			/* Compare the last 16 bytes with vector code. */
			result |= sk_memcmp_mask_16B(src1 + n - 16,
			    src2 + n - 16, byte_mask + n - 16);
		} else {
			/* Use scalar code if n < 16. */
			for (; i < n; i++) {
				result |= (src1[i] ^ src2[i]) & byte_mask[i];
			}
		}
	}
	return result;
}
#else /* !(__arm64__ || __arm__ || __x86_64__) */
/*
 * Use scalar variants elsewhere.
 */
#define sk_memcmp_mask                  __sk_memcmp_mask_scalar
#define sk_memcmp_mask_16B              __sk_memcmp_mask_16B_scalar
#define sk_memcmp_mask_32B              __sk_memcmp_mask_32B_scalar
#define sk_memcmp_mask_48B              __sk_memcmp_mask_48B_scalar
#define sk_memcmp_mask_64B              __sk_memcmp_mask_64B_scalar
#define sk_memcmp_mask_80B              __sk_memcmp_mask_80B_scalar
#endif /* !(__arm64__ || __arm__ || __x86_64__) */

/*
 * Scalar variants are available on all platforms if needed.
 */
#define sk_memcmp_mask_scalar           __sk_memcmp_mask_scalar
#define sk_memcmp_mask_16B_scalar       __sk_memcmp_mask_16B_scalar
#define sk_memcmp_mask_32B_scalar       __sk_memcmp_mask_32B_scalar
#define sk_memcmp_mask_48B_scalar       __sk_memcmp_mask_48B_scalar
#define sk_memcmp_mask_64B_scalar       __sk_memcmp_mask_64B_scalar
#define sk_memcmp_mask_80B_scalar       __sk_memcmp_mask_80B_scalar

#endif /* KERNEL */
#endif /* PRIVATE || BSD_KERNEL_PRIVATE */
#endif /* !_SKYWALK_COMMON_H_ */
