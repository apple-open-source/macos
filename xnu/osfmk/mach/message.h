/*
 * Copyright (c) 2000-2005 Apple Computer, Inc. All rights reserved.
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
/*
 * @OSF_COPYRIGHT@
 */
/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988,1987 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 * NOTICE: This file was modified by McAfee Research in 2004 to introduce
 * support for mandatory and extensible security protections.  This notice
 * is included in support of clause 2.2 (b) of the Apple Public License,
 * Version 2.0.
 * Copyright (c) 2005 SPARTA, Inc.
 */
/*
 */
/*
 *	File:	mach/message.h
 *
 *	Mach IPC message and primitive function definitions.
 */

#ifndef _MACH_MESSAGE_H_
#define _MACH_MESSAGE_H_

#include <stddef.h>
#include <stdint.h>
#include <machine/limits.h>
#include <machine/types.h> /* user_addr_t */
#include <mach/port.h>
#include <mach/boolean.h>
#include <mach/kern_return.h>
#include <mach/machine/vm_types.h>

#include <sys/cdefs.h>
#include <sys/appleapiopts.h>
#include <Availability.h>
#if !KERNEL && PRIVATE
#include <TargetConditionals.h>
#endif
#if __has_feature(ptrauth_calls)
#include <ptrauth.h>
#endif

/*
 *  The timeout mechanism uses mach_msg_timeout_t values,
 *  passed by value.  The timeout units are milliseconds.
 *  It is controlled with the MACH_SEND_TIMEOUT
 *  and MACH_RCV_TIMEOUT options.
 */

typedef natural_t mach_msg_timeout_t;

/*
 *  The value to be used when there is no timeout.
 *  (No MACH_SEND_TIMEOUT/MACH_RCV_TIMEOUT option.)
 */

#define MACH_MSG_TIMEOUT_NONE           ((mach_msg_timeout_t) 0)

/*
 *  The kernel uses MACH_MSGH_BITS_COMPLEX as a hint.  If it isn't on, it
 *  assumes the body of the message doesn't contain port rights or OOL
 *  data.  The field is set in received messages.  A user task must
 *  use caution in interpreting the body of a message if the bit isn't
 *  on, because the mach_msg_type's in the body might "lie" about the
 *  contents.  If the bit isn't on, but the mach_msg_types
 *  in the body specify rights or OOL data, the behavior is undefined.
 *  (Ie, an error may or may not be produced.)
 *
 *  The value of MACH_MSGH_BITS_REMOTE determines the interpretation
 *  of the msgh_remote_port field.  It is handled like a msgt_name,
 *  but must result in a send or send-once type right.
 *
 *  The value of MACH_MSGH_BITS_LOCAL determines the interpretation
 *  of the msgh_local_port field.  It is handled like a msgt_name,
 *  and also must result in a send or send-once type right.
 *
 *  The value of MACH_MSGH_BITS_VOUCHER determines the interpretation
 *  of the msgh_voucher_port field.  It is handled like a msgt_name,
 *  but must result in a send right (and the msgh_voucher_port field
 *  must be the name of a send right to a Mach voucher kernel object.
 *
 *  MACH_MSGH_BITS() combines two MACH_MSG_TYPE_* values, for the remote
 *  and local fields, into a single value suitable for msgh_bits.
 *
 *  MACH_MSGH_BITS_CIRCULAR should be zero; is is used internally.
 *
 *  The unused bits should be zero and are reserved for the kernel
 *  or for future interface expansion.
 */

#define MACH_MSGH_BITS_ZERO             0x00000000

#define MACH_MSGH_BITS_REMOTE_MASK      0x0000001f
#define MACH_MSGH_BITS_LOCAL_MASK       0x00001f00
#define MACH_MSGH_BITS_VOUCHER_MASK     0x001f0000

#define MACH_MSGH_BITS_PORTS_MASK               \
	        (MACH_MSGH_BITS_REMOTE_MASK |   \
	         MACH_MSGH_BITS_LOCAL_MASK |    \
	         MACH_MSGH_BITS_VOUCHER_MASK)

#define MACH_MSGH_BITS_COMPLEX          0x80000000U     /* message is complex */

#define MACH_MSGH_BITS_USER             0x801f1f1fU     /* allowed bits user->kernel */

#define MACH_MSGH_BITS_RAISEIMP         0x20000000U     /* importance raised due to msg */
#define MACH_MSGH_BITS_DENAP            MACH_MSGH_BITS_RAISEIMP

#define MACH_MSGH_BITS_IMPHOLDASRT      0x10000000U     /* assertion help, userland private */
#define MACH_MSGH_BITS_DENAPHOLDASRT    MACH_MSGH_BITS_IMPHOLDASRT

#define MACH_MSGH_BITS_CIRCULAR         0x10000000U     /* message circular, kernel private */

#define MACH_MSGH_BITS_USED             0xb01f1f1fU

/* setter macros for the bits */
#define MACH_MSGH_BITS(remote, local)  /* legacy */             \
	        ((remote) | ((local) << 8))
#define MACH_MSGH_BITS_SET_PORTS(remote, local, voucher)        \
	(((remote) & MACH_MSGH_BITS_REMOTE_MASK) |              \
	 (((local) << 8) & MACH_MSGH_BITS_LOCAL_MASK) |         \
	 (((voucher) << 16) & MACH_MSGH_BITS_VOUCHER_MASK))
#define MACH_MSGH_BITS_SET(remote, local, voucher, other)       \
	(MACH_MSGH_BITS_SET_PORTS((remote), (local), (voucher)) \
	 | ((other) &~ MACH_MSGH_BITS_PORTS_MASK))

/* getter macros for pulling values out of the bits field */
#define MACH_MSGH_BITS_REMOTE(bits)                             \
	        ((bits) & MACH_MSGH_BITS_REMOTE_MASK)
#define MACH_MSGH_BITS_LOCAL(bits)                              \
	        (((bits) & MACH_MSGH_BITS_LOCAL_MASK) >> 8)
#define MACH_MSGH_BITS_VOUCHER(bits)                            \
	        (((bits) & MACH_MSGH_BITS_VOUCHER_MASK) >> 16)
#define MACH_MSGH_BITS_PORTS(bits)                              \
	((bits) & MACH_MSGH_BITS_PORTS_MASK)
#define MACH_MSGH_BITS_OTHER(bits)                              \
	        ((bits) &~ MACH_MSGH_BITS_PORTS_MASK)

/* checking macros */
#define MACH_MSGH_BITS_HAS_REMOTE(bits)                         \
	(MACH_MSGH_BITS_REMOTE(bits) != MACH_MSGH_BITS_ZERO)
#define MACH_MSGH_BITS_HAS_LOCAL(bits)                          \
	(MACH_MSGH_BITS_LOCAL(bits) != MACH_MSGH_BITS_ZERO)
#define MACH_MSGH_BITS_HAS_VOUCHER(bits)                        \
	(MACH_MSGH_BITS_VOUCHER(bits) != MACH_MSGH_BITS_ZERO)
#define MACH_MSGH_BITS_IS_COMPLEX(bits)                         \
	(((bits) & MACH_MSGH_BITS_COMPLEX) != MACH_MSGH_BITS_ZERO)

/* importance checking macros */
#define MACH_MSGH_BITS_RAISED_IMPORTANCE(bits)                  \
	(((bits) & MACH_MSGH_BITS_RAISEIMP) != MACH_MSGH_BITS_ZERO)
#define MACH_MSGH_BITS_HOLDS_IMPORTANCE_ASSERTION(bits)         \
	(((bits) & MACH_MSGH_BITS_IMPHOLDASRT) != MACH_MSGH_BITS_ZERO)

/*
 *  Every message starts with a message header.
 *  Following the message header, if the message is complex, are a count
 *  of type descriptors and the type descriptors themselves
 *  (mach_msg_descriptor_t). The size of the message must be specified in
 *  bytes, and includes the message header, descriptor count, descriptors,
 *  and inline data.
 *
 *  The msgh_remote_port field specifies the destination of the message.
 *  It must specify a valid send or send-once right for a port.
 *
 *  The msgh_local_port field specifies a "reply port".  Normally,
 *  This field carries a send-once right that the receiver will use
 *  to reply to the message.  It may carry the values MACH_PORT_NULL,
 *  MACH_PORT_DEAD, a send-once right, or a send right.
 *
 *  The msgh_voucher_port field specifies a Mach voucher port. Only
 *  send rights to kernel-implemented Mach Voucher kernel objects in
 *  addition to MACH_PORT_NULL or MACH_PORT_DEAD may be passed.
 *
 *  The msgh_id field is uninterpreted by the message primitives.
 *  It normally carries information specifying the format
 *  or meaning of the message.
 */

typedef unsigned int mach_msg_bits_t;
typedef natural_t mach_msg_size_t;
typedef integer_t mach_msg_id_t;

#define MACH_MSG_SIZE_NULL (mach_msg_size_t *) 0

typedef unsigned int mach_msg_priority_t;

#define MACH_MSG_PRIORITY_UNSPECIFIED (mach_msg_priority_t) 0

#if PRIVATE
typedef uint8_t mach_msg_qos_t; // same as thread_qos_t
#define MACH_MSG_QOS_UNSPECIFIED        0
#define MACH_MSG_QOS_MAINTENANCE        1
#define MACH_MSG_QOS_BACKGROUND         2
#define MACH_MSG_QOS_UTILITY            3
#define MACH_MSG_QOS_DEFAULT            4
#define MACH_MSG_QOS_USER_INITIATED     5
#define MACH_MSG_QOS_USER_INTERACTIVE   6
#define MACH_MSG_QOS_LAST               6

extern int mach_msg_priority_is_pthread_priority(mach_msg_priority_t pri);
extern mach_msg_priority_t mach_msg_priority_encode(
	mach_msg_qos_t override_qos,
	mach_msg_qos_t qos,
	int relpri);
extern mach_msg_qos_t mach_msg_priority_overide_qos(mach_msg_priority_t pri);
extern mach_msg_qos_t mach_msg_priority_qos(mach_msg_priority_t pri);
extern int mach_msg_priority_relpri(mach_msg_priority_t pri);

#if KERNEL || !TARGET_OS_SIMULATOR
static inline int
mach_msg_priority_is_pthread_priority_inline(mach_msg_priority_t pri)
{
	return (pri & 0xff) == 0xff;
}

#define MACH_MSG_PRIORITY_RELPRI_SHIFT    8
#define MACH_MSG_PRIORITY_RELPRI_MASK     (0xff << MACH_MSG_PRIORITY_RELPRI_SHIFT)
#define MACH_MSG_PRIORITY_QOS_SHIFT       16
#define MACH_MSG_PRIORITY_QOS_MASK        (0xf << MACH_MSG_PRIORITY_QOS_SHIFT)
#define MACH_MSG_PRIORITY_OVERRIDE_SHIFT  20
#define MACH_MSG_PRIORITY_OVERRIDE_MASK   (0xf << MACH_MSG_PRIORITY_OVERRIDE_SHIFT)

static inline mach_msg_priority_t
mach_msg_priority_encode_inline(mach_msg_qos_t override_qos, mach_msg_qos_t qos, int relpri)
{
	mach_msg_priority_t pri = 0;
	if (qos > 0 && qos <= MACH_MSG_QOS_LAST) {
		pri |= (uint32_t)(qos << MACH_MSG_PRIORITY_QOS_SHIFT);
		pri |= (uint32_t)((uint8_t)(relpri - 1) << MACH_MSG_PRIORITY_RELPRI_SHIFT);
	}
	if (override_qos > 0 && override_qos <= MACH_MSG_QOS_LAST) {
		pri |= (uint32_t)(override_qos << MACH_MSG_PRIORITY_OVERRIDE_SHIFT);
	}
	return pri;
}

static inline mach_msg_qos_t
mach_msg_priority_overide_qos_inline(mach_msg_priority_t pri)
{
	pri &= MACH_MSG_PRIORITY_OVERRIDE_MASK;
	pri >>= MACH_MSG_PRIORITY_OVERRIDE_SHIFT;
	return (mach_msg_qos_t)(pri <= MACH_MSG_QOS_LAST ? pri : 0);
}

static inline mach_msg_qos_t
mach_msg_priority_qos_inline(mach_msg_priority_t pri)
{
	pri &= MACH_MSG_PRIORITY_QOS_MASK;
	pri >>= MACH_MSG_PRIORITY_QOS_SHIFT;
	return (mach_msg_qos_t)(pri <= MACH_MSG_QOS_LAST ? pri : 0);
}

static inline int
mach_msg_priority_relpri_inline(mach_msg_priority_t pri)
{
	if (mach_msg_priority_qos_inline(pri)) {
		return (int8_t)(pri >> MACH_MSG_PRIORITY_RELPRI_SHIFT) + 1;
	}
	return 0;
}

#define mach_msg_priority_is_pthread_priority(...) \
	mach_msg_priority_is_pthread_priority_inline(__VA_ARGS__)
#define mach_msg_priority_encode(...) \
	mach_msg_priority_encode_inline(__VA_ARGS__)
#define mach_msg_priority_overide_qos(...) \
	mach_msg_priority_overide_qos_inline(__VA_ARGS__)
#define mach_msg_priority_qos(...) \
	mach_msg_priority_qos_inline(__VA_ARGS__)
#define mach_msg_priority_relpri(...) \
	mach_msg_priority_relpri_inline(__VA_ARGS__)
#endif

#endif // PRIVATE

#if XNU_KERNEL_PRIVATE
__enum_decl(mach_msg_type_name_t, unsigned int, {
	MACH_MSG_TYPE_NONE            =  0,     /* no disposition */
	MACH_MSG_TYPE_MOVE_RECEIVE    = 16,     /* Must hold receive right */
	MACH_MSG_TYPE_MOVE_SEND       = 17,     /* Must hold send right(s) */
	MACH_MSG_TYPE_MOVE_SEND_ONCE  = 18,     /* Must hold sendonce right */
	MACH_MSG_TYPE_COPY_SEND       = 19,     /* Must hold send right(s) */
	MACH_MSG_TYPE_MAKE_SEND       = 20,     /* Must hold receive right */
	MACH_MSG_TYPE_MAKE_SEND_ONCE  = 21,     /* Must hold receive right */
});
#else
typedef unsigned int mach_msg_type_name_t;

#define MACH_MSG_TYPE_MOVE_RECEIVE      16      /* Must hold receive right */
#define MACH_MSG_TYPE_MOVE_SEND         17      /* Must hold send right(s) */
#define MACH_MSG_TYPE_MOVE_SEND_ONCE    18      /* Must hold sendonce right */
#define MACH_MSG_TYPE_COPY_SEND         19      /* Must hold send right(s) */
#define MACH_MSG_TYPE_MAKE_SEND         20      /* Must hold receive right */
#define MACH_MSG_TYPE_MAKE_SEND_ONCE    21      /* Must hold receive right */
#define MACH_MSG_TYPE_COPY_RECEIVE      22      /* NOT VALID */
#define MACH_MSG_TYPE_DISPOSE_RECEIVE   24      /* must hold receive right */
#define MACH_MSG_TYPE_DISPOSE_SEND      25      /* must hold send right(s) */
#define MACH_MSG_TYPE_DISPOSE_SEND_ONCE 26      /* must hold sendonce right */
#endif

typedef unsigned int mach_msg_copy_options_t;

#define MACH_MSG_PHYSICAL_COPY          0
#define MACH_MSG_VIRTUAL_COPY           1
#define MACH_MSG_ALLOCATE               2
#define MACH_MSG_OVERWRITE              3       /* deprecated */
#ifdef  MACH_KERNEL
#define MACH_MSG_KALLOC_COPY_T          4
#endif  /* MACH_KERNEL */

#define MACH_MSG_GUARD_FLAGS_NONE                   0x0000
#define MACH_MSG_GUARD_FLAGS_IMMOVABLE_RECEIVE      0x0001    /* Move the receive right and mark it as immovable */
#define MACH_MSG_GUARD_FLAGS_UNGUARDED_ON_SEND      0x0002    /* Verify that the port is unguarded */
#define MACH_MSG_GUARD_FLAGS_MASK                   0x0003    /* Valid flag bits */
typedef unsigned int mach_msg_guard_flags_t;

/*
 * In a complex mach message, the mach_msg_header_t is followed by
 * a descriptor count, then an array of that number of descriptors
 * (mach_msg_*_descriptor_t). The type field of mach_msg_type_descriptor_t
 * (which any descriptor can be cast to) indicates the flavor of the
 * descriptor.
 *
 * Note that in LP64, the various types of descriptors are no longer all
 * the same size as mach_msg_descriptor_t, so the array cannot be indexed
 * as expected.
 */

typedef unsigned int mach_msg_descriptor_type_t;

#define MACH_MSG_PORT_DESCRIPTOR                0
#define MACH_MSG_OOL_DESCRIPTOR                 1
#define MACH_MSG_OOL_PORTS_DESCRIPTOR           2
#define MACH_MSG_OOL_VOLATILE_DESCRIPTOR        3
#define MACH_MSG_GUARDED_PORT_DESCRIPTOR        4

#define MACH_MSG_DESCRIPTOR_MAX MACH_MSG_GUARDED_PORT_DESCRIPTOR

#if XNU_KERNEL_PRIVATE && __has_feature(ptrauth_calls)
#define __ipc_desc_sign(d) \
	__ptrauth(ptrauth_key_process_independent_data, \
	    1, ptrauth_string_discriminator("ipc_desc." d))
#else
#define __ipc_desc_sign(d)
#endif /* KERNEL */

#pragma pack(push, 4)

typedef struct {
	natural_t                     pad1;
	mach_msg_size_t               pad2;
	unsigned int                  pad3 : 24;
	mach_msg_descriptor_type_t    type : 8;
} mach_msg_type_descriptor_t;

typedef struct {
#if KERNEL
	union {
		mach_port_t __ipc_desc_sign("port") name;
		mach_port_t           kext_name;
		mach_port_t           u_name;
	};
#else
	mach_port_t                   name;
	mach_msg_size_t               pad1;
#endif
	unsigned int                  pad2 : 16;
	mach_msg_type_name_t          disposition : 8;
	mach_msg_descriptor_type_t    type : 8;
#if defined(KERNEL)
	uint32_t                      pad_end;
#endif
} mach_msg_port_descriptor_t;

#if MACH_KERNEL_PRIVATE
typedef struct {
	mach_port_name_t              name;
	mach_msg_size_t               pad1;
	uint32_t                      pad2 : 16;
	mach_msg_type_name_t          disposition : 8;
	mach_msg_descriptor_type_t    type : 8;
} mach_msg_user_port_descriptor_t;
#endif /* MACH_KERNEL_PRIVATE */

typedef struct {
	uint32_t                      address;
	mach_msg_size_t               size;
	boolean_t                     deallocate: 8;
	mach_msg_copy_options_t       copy: 8;
	unsigned int                  pad1: 8;
	mach_msg_descriptor_type_t    type: 8;
} mach_msg_ool_descriptor32_t;

typedef struct {
	uint64_t                      address;
	boolean_t                     deallocate: 8;
	mach_msg_copy_options_t       copy: 8;
	unsigned int                  pad1: 8;
	mach_msg_descriptor_type_t    type: 8;
	mach_msg_size_t               size;
} mach_msg_ool_descriptor64_t;

typedef struct {
#if KERNEL
	union {
		void *__ipc_desc_sign("address") address;
		void                 *kext_address;
		user_addr_t           u_address;
	};
#else
	void                         *address;
#endif
#if !defined(__LP64__)
	mach_msg_size_t               size;
#endif
	boolean_t                     deallocate: 8;
	mach_msg_copy_options_t       copy: 8;
	unsigned int                  pad1: 8;
	mach_msg_descriptor_type_t    type: 8;
#if defined(__LP64__)
	mach_msg_size_t               size;
#endif
#if defined(KERNEL) && !defined(__LP64__)
	uint32_t          pad_end;
#endif
} mach_msg_ool_descriptor_t;

typedef struct {
	uint32_t                      address;
	mach_msg_size_t               count;
	boolean_t                     deallocate: 8;
	mach_msg_copy_options_t       copy: 8;
	mach_msg_type_name_t          disposition : 8;
	mach_msg_descriptor_type_t    type : 8;
} mach_msg_ool_ports_descriptor32_t;

typedef struct {
	uint64_t                      address;
	boolean_t                     deallocate: 8;
	mach_msg_copy_options_t       copy: 8;
	mach_msg_type_name_t          disposition : 8;
	mach_msg_descriptor_type_t    type : 8;
	mach_msg_size_t               count;
} mach_msg_ool_ports_descriptor64_t;

typedef struct {
#if KERNEL
	union {
		void *__ipc_desc_sign("port_array") address;
		void                 *kext_address;
		user_addr_t           u_address;
	};
#else
	void                         *address;
#endif
#if !defined(__LP64__)
	mach_msg_size_t               count;
#endif
	boolean_t                     deallocate: 8;
	mach_msg_copy_options_t       copy: 8;
	mach_msg_type_name_t          disposition : 8;
	mach_msg_descriptor_type_t    type : 8;
#if defined(__LP64__)
	mach_msg_size_t               count;
#endif
#if defined(KERNEL) && !defined(__LP64__)
	uint32_t          pad_end;
#endif
} mach_msg_ool_ports_descriptor_t;

typedef struct {
	uint32_t                      context;
	mach_port_name_t              name;
	mach_msg_guard_flags_t        flags : 16;
	mach_msg_type_name_t          disposition : 8;
	mach_msg_descriptor_type_t    type : 8;
} mach_msg_guarded_port_descriptor32_t;

typedef struct {
	uint64_t                      context;
	mach_msg_guard_flags_t        flags : 16;
	mach_msg_type_name_t          disposition : 8;
	mach_msg_descriptor_type_t    type : 8;
	mach_port_name_t              name;
} mach_msg_guarded_port_descriptor64_t;

typedef struct {
#if defined(KERNEL)
	union {
		mach_port_t __ipc_desc_sign("guarded_port") name;
		mach_port_t           kext_name;
		mach_port_context_t   u_context;
	};
	mach_msg_guard_flags_t        flags : 16;
	mach_msg_type_name_t          disposition : 8;
	mach_msg_descriptor_type_t    type : 8;
	union {
		uint32_t              pad_end;
		mach_port_name_t      u_name;
	};
#else
	mach_port_context_t           context;
#if !defined(__LP64__)
	mach_port_name_t              name;
#endif
	mach_msg_guard_flags_t        flags : 16;
	mach_msg_type_name_t          disposition : 8;
	mach_msg_descriptor_type_t    type : 8;
#if defined(__LP64__)
	mach_port_name_t              name;
#endif /* defined(__LP64__) */
#endif /* defined(KERNEL) */
} mach_msg_guarded_port_descriptor_t;

/*
 * LP64support - This union definition is not really
 * appropriate in LP64 mode because not all descriptors
 * are of the same size in that environment.
 */
#if defined(__LP64__) && defined(KERNEL)
typedef union {
	mach_msg_port_descriptor_t            port;
	mach_msg_ool_descriptor32_t           out_of_line;
	mach_msg_ool_ports_descriptor32_t     ool_ports;
	mach_msg_type_descriptor_t            type;
	mach_msg_guarded_port_descriptor32_t  guarded_port;
} mach_msg_descriptor_t;
#else
typedef union {
	mach_msg_port_descriptor_t            port;
	mach_msg_ool_descriptor_t             out_of_line;
	mach_msg_ool_ports_descriptor_t       ool_ports;
	mach_msg_type_descriptor_t            type;
	mach_msg_guarded_port_descriptor_t    guarded_port;
} mach_msg_descriptor_t;
#endif

typedef struct {
	mach_msg_size_t msgh_descriptor_count;
} mach_msg_body_t;

#define MACH_MSG_BODY_NULL            ((mach_msg_body_t *) 0)
#define MACH_MSG_DESCRIPTOR_NULL      ((mach_msg_descriptor_t *) 0)

typedef struct {
	mach_msg_bits_t               msgh_bits;
	mach_msg_size_t               msgh_size;
	mach_port_t                   msgh_remote_port;
	mach_port_t                   msgh_local_port;
	mach_port_name_t              msgh_voucher_port;
	mach_msg_id_t                 msgh_id;
} mach_msg_header_t;

#if PRIVATE

/* mach msg2 data vectors are positional */
__enum_decl(mach_msgv_index_t, uint32_t, {
	MACH_MSGV_IDX_MSG = 0,
	MACH_MSGV_IDX_AUX = 1,
});

#define MACH_MSGV_MAX_COUNT (MACH_MSGV_IDX_AUX + 1)
/* at least DISPATCH_MSGV_AUX_MAX_SIZE in libdispatch */
#define LIBSYSCALL_MSGV_AUX_MAX_SIZE 128

typedef struct {
	/* a mach_msg_header_t* or mach_msg_aux_header_t* */
	mach_vm_address_t               msgv_data;
	/* if msgv_rcv_addr is non-zero, use it as rcv address instead */
	mach_vm_address_t               msgv_rcv_addr;
	mach_msg_size_t                 msgv_send_size;
	mach_msg_size_t                 msgv_rcv_size;
} mach_msg_vector_t;

typedef struct {
	mach_msg_size_t                 msgdh_size;
	uint32_t                        msgdh_reserved; /* For future */
} mach_msg_aux_header_t;

#endif /* PRIVATE */

#define msgh_reserved                 msgh_voucher_port
#define MACH_MSG_NULL                 ((mach_msg_header_t *) 0)

typedef struct {
	mach_msg_header_t             header;
	mach_msg_body_t               body;
} mach_msg_base_t;

#if MACH_KERNEL_PRIVATE

typedef struct {
	/* first two fields must align with mach_msg_header_t */
	mach_msg_bits_t               msgh_bits;
	mach_msg_size_t               msgh_size;
	mach_port_name_t              msgh_remote_port;
	mach_port_name_t              msgh_local_port;
	mach_port_name_t              msgh_voucher_port;
	mach_msg_id_t                 msgh_id;
} mach_msg_user_header_t;

typedef struct {
	mach_msg_user_header_t        header;
	mach_msg_body_t               body;
} mach_msg_user_base_t;

typedef union {
	mach_msg_type_descriptor_t            kdesc_header;
	mach_msg_port_descriptor_t            kdesc_port;
	mach_msg_ool_descriptor_t             kdesc_memory;
	mach_msg_ool_ports_descriptor_t       kdesc_port_array;
	mach_msg_guarded_port_descriptor_t    kdesc_guarded_port;
} mach_msg_kdescriptor_t;

static inline mach_msg_descriptor_type_t
mach_msg_kdescriptor_type(const mach_msg_kdescriptor_t *kdesc)
{
	return kdesc->kdesc_header.type;
}

typedef struct {
	mach_msg_header_t             msgb_header;
	mach_msg_size_t               msgb_dsc_count;
	mach_msg_kdescriptor_t        msgb_dsc_array[];
} mach_msg_kbase_t;

static inline mach_msg_kbase_t *
mach_msg_header_to_kbase(mach_msg_header_t *hdr)
{
	return __container_of(hdr, mach_msg_kbase_t, msgb_header);
}

#define mach_port_array_alloc(count, flags) \
	kalloc_type(mach_port_ool_t, count, flags)

#define mach_port_array_free(ptr, count) \
	kfree_type(mach_port_ool_t, count, ptr)

#endif /* MACH_KERNEL_PRIVATE */

typedef unsigned int mach_msg_trailer_type_t;

#define MACH_MSG_TRAILER_FORMAT_0       0

typedef unsigned int mach_msg_trailer_size_t;
typedef char *mach_msg_trailer_info_t;

typedef struct {
	mach_msg_trailer_type_t       msgh_trailer_type;
	mach_msg_trailer_size_t       msgh_trailer_size;
} mach_msg_trailer_t;

/*
 *  The msgh_seqno field carries a sequence number
 *  associated with the received-from port.  A port's
 *  sequence number is incremented every time a message
 *  is received from it and included in the received
 *  trailer to help put messages back in sequence if
 *  multiple threads receive and/or process received
 *  messages.
 */
typedef struct {
	mach_msg_trailer_type_t       msgh_trailer_type;
	mach_msg_trailer_size_t       msgh_trailer_size;
	mach_port_seqno_t             msgh_seqno;
} mach_msg_seqno_trailer_t;

typedef struct {
	unsigned int                  val[2];
} security_token_t;

typedef struct {
	mach_msg_trailer_type_t       msgh_trailer_type;
	mach_msg_trailer_size_t       msgh_trailer_size;
	mach_port_seqno_t             msgh_seqno;
	security_token_t              msgh_sender;
} mach_msg_security_trailer_t;

/*
 * The audit token is an opaque token which identifies
 * Mach tasks and senders of Mach messages as subjects
 * to the BSM audit system.  Only the appropriate BSM
 * library routines should be used to interpret the
 * contents of the audit token as the representation
 * of the subject identity within the token may change
 * over time.
 */
typedef struct {
	unsigned int                  val[8];
} audit_token_t;

/*
 * Safe initializer for audit_token_t.
 * Variables holding unset audit tokens should generally
 * be initialized to INVALID_AUDIT_TOKEN_VALUE, to allow
 * unset audit tokens be distinguished from the kernel's
 * audit token, KERNEL_AUDIT_TOKEN_VALUE.  It is `safe'
 * in that it limits potential damage if such an unset
 * audit token, or one of its fields, were ever to be
 * interpreted as valid by mistake.  Notably, the pid is
 * outside of range of valid pids, and none of the
 * fields correspond to privileged users or groups.
 */
#define INVALID_AUDIT_TOKEN_VALUE     {{ \
	UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX, \
	UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX }}

typedef struct {
	mach_msg_trailer_type_t       msgh_trailer_type;
	mach_msg_trailer_size_t       msgh_trailer_size;
	mach_port_seqno_t             msgh_seqno;
	security_token_t              msgh_sender;
	audit_token_t                 msgh_audit;
} mach_msg_audit_trailer_t;

typedef struct {
	mach_msg_trailer_type_t       msgh_trailer_type;
	mach_msg_trailer_size_t       msgh_trailer_size;
	mach_port_seqno_t             msgh_seqno;
	security_token_t              msgh_sender;
	audit_token_t                 msgh_audit;
	mach_port_context_t           msgh_context;
} mach_msg_context_trailer_t;

#if defined(MACH_KERNEL_PRIVATE) && defined(__arm64__)
typedef struct {
	mach_msg_trailer_type_t       msgh_trailer_type;
	mach_msg_trailer_size_t       msgh_trailer_size;
	mach_port_seqno_t             msgh_seqno;
	security_token_t              msgh_sender;
	audit_token_t                 msgh_audit;
	mach_port_context32_t         msgh_context;
} mach_msg_context_trailer32_t;

typedef struct {
	mach_msg_trailer_type_t       msgh_trailer_type;
	mach_msg_trailer_size_t       msgh_trailer_size;
	mach_port_seqno_t             msgh_seqno;
	security_token_t              msgh_sender;
	audit_token_t                 msgh_audit;
	mach_port_context64_t         msgh_context;
} mach_msg_context_trailer64_t;
#endif


typedef struct {
	mach_port_name_t sender;
} msg_labels_t;

typedef int mach_msg_filter_id;
#define MACH_MSG_FILTER_POLICY_ALLOW (mach_msg_filter_id)0

/*
 *  Trailer type to pass MAC policy label info as a mach message trailer.
 *
 */

typedef struct {
	mach_msg_trailer_type_t       msgh_trailer_type;
	mach_msg_trailer_size_t       msgh_trailer_size;
	mach_port_seqno_t             msgh_seqno;
	security_token_t              msgh_sender;
	audit_token_t                 msgh_audit;
	mach_port_context_t           msgh_context;
	mach_msg_filter_id            msgh_ad;
	msg_labels_t                  msgh_labels;
} mach_msg_mac_trailer_t;

#if defined(MACH_KERNEL_PRIVATE) && defined(__arm64__)
typedef struct {
	mach_msg_trailer_type_t       msgh_trailer_type;
	mach_msg_trailer_size_t       msgh_trailer_size;
	mach_port_seqno_t             msgh_seqno;
	security_token_t              msgh_sender;
	audit_token_t                 msgh_audit;
	mach_port_context32_t         msgh_context;
	mach_msg_filter_id            msgh_ad;
	msg_labels_t                  msgh_labels;
} mach_msg_mac_trailer32_t;

typedef struct {
	mach_msg_trailer_type_t       msgh_trailer_type;
	mach_msg_trailer_size_t       msgh_trailer_size;
	mach_port_seqno_t             msgh_seqno;
	security_token_t              msgh_sender;
	audit_token_t                 msgh_audit;
	mach_port_context64_t         msgh_context;
	mach_msg_filter_id            msgh_ad;
	msg_labels_t                  msgh_labels;
} mach_msg_mac_trailer64_t;

#endif

#define MACH_MSG_TRAILER_MINIMUM_SIZE  sizeof(mach_msg_trailer_t)

/*
 * These values can change from release to release - but clearly
 * code cannot request additional trailer elements one was not
 * compiled to understand.  Therefore, it is safe to use this
 * constant when the same module specified the receive options.
 * Otherwise, you run the risk that the options requested by
 * another module may exceed the local modules notion of
 * MAX_TRAILER_SIZE.
 */
#if defined(MACH_KERNEL_PRIVATE) && defined(__arm64__)
typedef mach_msg_mac_trailer64_t mach_msg_max_trailer64_t;
typedef mach_msg_mac_trailer32_t mach_msg_max_trailer32_t;
#endif

typedef mach_msg_mac_trailer_t mach_msg_max_trailer_t;
#define MAX_TRAILER_SIZE ((mach_msg_size_t)sizeof(mach_msg_max_trailer_t))

/*
 * Legacy requirements keep us from ever updating these defines (even
 * when the format_0 trailers gain new option data fields in the future).
 * Therefore, they shouldn't be used going forward.  Instead, the sizes
 * should be compared against the specific element size requested using
 * REQUESTED_TRAILER_SIZE.
 */
typedef mach_msg_security_trailer_t mach_msg_format_0_trailer_t;

/*typedef mach_msg_mac_trailer_t mach_msg_format_0_trailer_t;
 */

#define MACH_MSG_TRAILER_FORMAT_0_SIZE sizeof(mach_msg_format_0_trailer_t)

#define   KERNEL_SECURITY_TOKEN_VALUE  { {0, 1} }
extern const security_token_t KERNEL_SECURITY_TOKEN;

#define   KERNEL_AUDIT_TOKEN_VALUE  { {0, 0, 0, 0, 0, 0, 0, 0} }
extern const audit_token_t KERNEL_AUDIT_TOKEN;

typedef integer_t mach_msg_options_t;

#define MACH_MSG_HEADER_EMPTY (mach_msg_header_t){ }

typedef struct {
	mach_msg_header_t     header;
} mach_msg_empty_send_t;

typedef struct {
	mach_msg_header_t     header;
	mach_msg_trailer_t    trailer;
} mach_msg_empty_rcv_t;

typedef union{
	mach_msg_empty_send_t send;
	mach_msg_empty_rcv_t  rcv;
} mach_msg_empty_t;

#pragma pack(pop)

/* utility to round the message size - will become machine dependent */
#define round_msg(x)    (((mach_msg_size_t)(x) + sizeof (natural_t) - 1) & \
	                        ~(sizeof (natural_t) - 1))

#ifdef XNU_KERNEL_PRIVATE

#include <os/base.h>
#include <os/overflow.h>
#include <kern/debug.h>

#define round_msg_overflow(in, out) __os_warn_unused(({ \
	        bool __ovr = os_add_overflow(in, (__typeof__(*out))(sizeof(natural_t) - 1), out); \
	        *out &= ~((__typeof__(*out))(sizeof(natural_t) - 1)); \
	        __ovr; \
	}))

static inline mach_msg_size_t
mach_round_msg(mach_msg_size_t x)
{
	if (round_msg_overflow(x, &x)) {
		panic("round msg overflow");
	}
	return x;
}
#endif /* XNU_KERNEL_PRIVATE */

/*
 *  There is no fixed upper bound to the size of Mach messages.
 */
#define MACH_MSG_SIZE_MAX       ((mach_msg_size_t) ~0)

#if defined(__APPLE_API_PRIVATE)
/*
 *  But architectural limits of a given implementation, or
 *  temporal conditions may cause unpredictable send failures
 *  for messages larger than MACH_MSG_SIZE_RELIABLE.
 *
 *  In either case, waiting for memory is [currently] outside
 *  the scope of send timeout values provided to IPC.
 */
#define MACH_MSG_SIZE_RELIABLE  ((mach_msg_size_t) 256 * 1024)
#endif
/*
 *  Compatibility definitions, for code written
 *  when there was a msgh_kind instead of msgh_seqno.
 */
#define MACH_MSGH_KIND_NORMAL           0x00000000
#define MACH_MSGH_KIND_NOTIFICATION     0x00000001
#define msgh_kind                       msgh_seqno
#define mach_msg_kind_t                 mach_port_seqno_t

typedef natural_t mach_msg_type_size_t;
typedef natural_t mach_msg_type_number_t;

/*
 *  Values received/carried in messages.  Tells the receiver what
 *  sort of port right he now has.
 *
 *  MACH_MSG_TYPE_PORT_NAME is used to transfer a port name
 *  which should remain uninterpreted by the kernel.  (Port rights
 *  are not transferred, just the port name.)
 */

#define MACH_MSG_TYPE_PORT_NONE         0

#define MACH_MSG_TYPE_PORT_NAME         15
#define MACH_MSG_TYPE_PORT_RECEIVE      MACH_MSG_TYPE_MOVE_RECEIVE
#define MACH_MSG_TYPE_PORT_SEND         MACH_MSG_TYPE_MOVE_SEND
#define MACH_MSG_TYPE_PORT_SEND_ONCE    MACH_MSG_TYPE_MOVE_SEND_ONCE

#define MACH_MSG_TYPE_LAST              22              /* Last assigned */

/*
 *  A dummy value.  Mostly used to indicate that the actual value
 *  will be filled in later, dynamically.
 */

#define MACH_MSG_TYPE_POLYMORPHIC       ((mach_msg_type_name_t) -1)

/*
 *	Is a given item a port type?
 */

#define MACH_MSG_TYPE_PORT_ANY(x)                       \
	(((x) >= MACH_MSG_TYPE_MOVE_RECEIVE) &&         \
	 ((x) <= MACH_MSG_TYPE_MAKE_SEND_ONCE))

#define MACH_MSG_TYPE_PORT_ANY_SEND(x)                  \
	(((x) >= MACH_MSG_TYPE_MOVE_SEND) &&            \
	 ((x) <= MACH_MSG_TYPE_MAKE_SEND_ONCE))

#define MACH_MSG_TYPE_PORT_ANY_SEND_ONCE(x)             \
	(((x) == MACH_MSG_TYPE_MOVE_SEND_ONCE) ||       \
	 ((x) == MACH_MSG_TYPE_MAKE_SEND_ONCE))

#define MACH_MSG_TYPE_PORT_ANY_RIGHT(x)                 \
	(((x) >= MACH_MSG_TYPE_MOVE_RECEIVE) &&         \
	 ((x) <= MACH_MSG_TYPE_MOVE_SEND_ONCE))

typedef integer_t mach_msg_option_t;

#define MACH_MSG_OPTION_NONE    0x00000000

#define MACH_SEND_MSG           0x00000001
#define MACH_RCV_MSG            0x00000002

#define MACH_RCV_LARGE          0x00000004      /* report large message sizes */
#define MACH_RCV_LARGE_IDENTITY 0x00000008      /* identify source of large messages */

#define MACH_SEND_TIMEOUT       0x00000010      /* timeout value applies to send */
#define MACH_SEND_OVERRIDE      0x00000020      /* priority override for send */
#define MACH_SEND_INTERRUPT     0x00000040      /* don't restart interrupted sends */
#define MACH_SEND_NOTIFY        0x00000080      /* arm send-possible notify */
#define MACH_SEND_ALWAYS        0x00010000      /* ignore qlimits - kernel only */
#define MACH_SEND_FILTER_NONFATAL        0x00010000      /* rejection by message filter should return failure - user only */
#define MACH_SEND_TRAILER       0x00020000      /* sender-provided trailer */
#define MACH_SEND_NOIMPORTANCE  0x00040000      /* msg won't carry importance */
#define MACH_SEND_NODENAP       MACH_SEND_NOIMPORTANCE
#define MACH_SEND_IMPORTANCE    0x00080000      /* msg carries importance - kernel only */
#define MACH_SEND_SYNC_OVERRIDE 0x00100000      /* msg should do sync IPC override (on legacy kernels) */
#define MACH_SEND_PROPAGATE_QOS 0x00200000      /* IPC should propagate the caller's QoS */
#define MACH_SEND_SYNC_USE_THRPRI       MACH_SEND_PROPAGATE_QOS /* obsolete name */
#define MACH_SEND_KERNEL        0x00400000      /* full send from kernel space - kernel only */
#define MACH_SEND_SYNC_BOOTSTRAP_CHECKIN  0x00800000      /* special reply port should boost thread doing sync bootstrap checkin */

#define MACH_RCV_TIMEOUT        0x00000100      /* timeout value applies to receive */
#define MACH_RCV_NOTIFY         0x00000000      /* legacy name (value was: 0x00000200) */
#define MACH_RCV_INTERRUPT      0x00000400      /* don't restart interrupted receive */
#define MACH_RCV_VOUCHER        0x00000800      /* willing to receive voucher port */
#define MACH_RCV_OVERWRITE      0x00000000      /* scatter receive (deprecated) */
#define MACH_RCV_GUARDED_DESC   0x00001000      /* Can receive new guarded descriptor */
#define MACH_RCV_SYNC_WAIT      0x00004000      /* sync waiter waiting for rcv */
#define MACH_RCV_SYNC_PEEK      0x00008000      /* sync waiter waiting to peek */

#define MACH_MSG_STRICT_REPLY   0x00000200      /* Enforce specific properties about the reply port, and
	                                         * the context in which a thread replies to a message.
	                                         * This flag must be passed on both the SEND and RCV */

#if PRIVATE

__options_decl(mach_msg_option64_t, uint64_t, {
	MACH64_MSG_OPTION_NONE                 = 0x0ull,
	/* share lower 32 bits with mach_msg_option_t */
	MACH64_SEND_MSG                        = MACH_SEND_MSG,
	MACH64_RCV_MSG                         = MACH_RCV_MSG,

	MACH64_RCV_LARGE                       = MACH_RCV_LARGE,
	MACH64_RCV_LARGE_IDENTITY              = MACH_RCV_LARGE_IDENTITY,

	MACH64_SEND_TIMEOUT                    = MACH_SEND_TIMEOUT,
	MACH64_SEND_OVERRIDE                   = MACH_SEND_OVERRIDE,
	MACH64_SEND_INTERRUPT                  = MACH_SEND_INTERRUPT,
	MACH64_SEND_NOTIFY                     = MACH_SEND_NOTIFY,
#if KERNEL
	MACH64_SEND_ALWAYS                     = MACH_SEND_ALWAYS,
	MACH64_SEND_IMPORTANCE                 = MACH_SEND_IMPORTANCE,
	MACH64_SEND_KERNEL                     = MACH_SEND_KERNEL,
#endif
	MACH64_SEND_FILTER_NONFATAL            = MACH_SEND_FILTER_NONFATAL,
	MACH64_SEND_TRAILER                    = MACH_SEND_TRAILER,
	MACH64_SEND_NOIMPORTANCE               = MACH_SEND_NOIMPORTANCE,
	MACH64_SEND_NODENAP                    = MACH_SEND_NODENAP,
	MACH64_SEND_SYNC_OVERRIDE              = MACH_SEND_SYNC_OVERRIDE,
	MACH64_SEND_PROPAGATE_QOS              = MACH_SEND_PROPAGATE_QOS,

	MACH64_SEND_SYNC_BOOTSTRAP_CHECKIN     = MACH_SEND_SYNC_BOOTSTRAP_CHECKIN,

	MACH64_RCV_TIMEOUT                     = MACH_RCV_TIMEOUT,

	MACH64_RCV_INTERRUPT                   = MACH_RCV_INTERRUPT,
	MACH64_RCV_VOUCHER                     = MACH_RCV_VOUCHER,

	MACH64_RCV_GUARDED_DESC                = MACH_RCV_GUARDED_DESC,
	MACH64_RCV_SYNC_WAIT                   = MACH_RCV_SYNC_WAIT,
	MACH64_RCV_SYNC_PEEK                   = MACH_RCV_SYNC_PEEK,

	MACH64_MSG_STRICT_REPLY                = MACH_MSG_STRICT_REPLY,
	/* following options are 64 only */

	/* Send and receive message as vectors */
	MACH64_MSG_VECTOR                      = 0x0000000100000000ull,
	/* The message is a kobject call */
	MACH64_SEND_KOBJECT_CALL               = 0x0000000200000000ull,
	/* The message is sent to a message queue */
	MACH64_SEND_MQ_CALL                    = 0x0000000400000000ull,
	/* This message destination is unknown. Used by old simulators only. */
	MACH64_SEND_ANY                        = 0x0000000800000000ull,
	/* This message is a DriverKit call */
	MACH64_SEND_DK_CALL                    = 0x0000001000000000ull,

#ifdef XNU_KERNEL_PRIVATE
	/*
	 * Policy for the mach_msg2_trap() call
	 */
	MACH64_POLICY_KERNEL_EXTENSION         = 0x0002000000000000ull,
	MACH64_POLICY_FILTER_NON_FATAL         = 0x0004000000000000ull,
	MACH64_POLICY_FILTER_MSG               = 0x0008000000000000ull,
	MACH64_POLICY_DEFAULT                  = 0x0010000000000000ull,
#if XNU_TARGET_OS_OSX
	MACH64_POLICY_SIMULATED                = 0x0020000000000000ull,
#else
	MACH64_POLICY_SIMULATED                = 0x0000000000000000ull,
#endif
#if CONFIG_ROSETTA
	MACH64_POLICY_TRANSLATED               = 0x0040000000000000ull,
#else
	MACH64_POLICY_TRANSLATED               = 0x0000000000000000ull,
#endif
	MACH64_POLICY_HARDENED                 = 0x0080000000000000ull,
	MACH64_POLICY_RIGID                    = 0x0100000000000000ull,
	MACH64_POLICY_PLATFORM                 = 0x0200000000000000ull,
	MACH64_POLICY_KERNEL                   = MACH64_SEND_KERNEL,

	/* one of these bits must be set to have a valid policy */
	MACH64_POLICY_NEEDED_MASK              = (
		MACH64_POLICY_SIMULATED |
		MACH64_POLICY_TRANSLATED |
		MACH64_POLICY_DEFAULT |
		MACH64_POLICY_HARDENED |
		MACH64_POLICY_RIGID |
		MACH64_POLICY_PLATFORM |
		MACH64_POLICY_KERNEL),

	/* extra policy modifiers */
	MACH64_POLICY_MASK                     = (
		MACH64_POLICY_KERNEL_EXTENSION |
		MACH64_POLICY_FILTER_NON_FATAL |
		MACH64_POLICY_FILTER_MSG |
		MACH64_POLICY_NEEDED_MASK),

	/*
	 * If kmsg has auxiliary data, append it immediate after the message
	 * and trailer.
	 *
	 * Must be used in conjunction with MACH64_MSG_VECTOR,
	 * only used by kevent() from the kernel.
	 */
	MACH64_RCV_LINEAR_VECTOR               = 0x1000000000000000ull,
	/* Receive into highest addr of buffer */
	MACH64_RCV_STACK                       = 0x2000000000000000ull,
#if MACH_FLIPC
	/*
	 * This internal-only flag is intended for use by a single thread per-port/set!
	 * If more than one thread attempts to MACH64_PEEK_MSG on a port or set, one of
	 * the threads may miss messages (in fact, it may never wake up).
	 */
	MACH64_PEEK_MSG                        = 0x4000000000000000ull,
#endif /* MACH_FLIPC */
	/*
	 * This is a mach_msg2() send/receive operation.
	 */
	MACH64_MACH_MSG2                       = 0x8000000000000000ull
#endif
});

/* old spelling */
#define MACH64_SEND_USER_CALL              MACH64_SEND_MQ_CALL
#endif /* PRIVATE */

/*
 * NOTE: a 0x00------ RCV mask implies to ask for
 * a MACH_MSG_TRAILER_FORMAT_0 with 0 Elements,
 * which is equivalent to a mach_msg_trailer_t.
 *
 * XXXMAC: unlike the rest of the MACH_RCV_* flags, MACH_RCV_TRAILER_LABELS
 * needs its own private bit since we only calculate its fields when absolutely
 * required.
 */
#define MACH_RCV_TRAILER_NULL   0
#define MACH_RCV_TRAILER_SEQNO  1
#define MACH_RCV_TRAILER_SENDER 2
#define MACH_RCV_TRAILER_AUDIT  3
#define MACH_RCV_TRAILER_CTX    4
#define MACH_RCV_TRAILER_AV     7
#define MACH_RCV_TRAILER_LABELS 8

#define MACH_RCV_TRAILER_TYPE(x)     (((x) & 0xf) << 28)
#define MACH_RCV_TRAILER_ELEMENTS(x) (((x) & 0xf) << 24)
#define MACH_RCV_TRAILER_MASK        ((0xf << 24))

#define GET_RCV_ELEMENTS(y) (((y) >> 24) & 0xf)

#ifdef MACH_KERNEL_PRIVATE
/*
 * The options that the kernel honors when passed from user space, not including
 * user-only options that alias kernel-only options.
 */
#define MACH_SEND_USER (MACH_SEND_MSG | MACH_SEND_TIMEOUT | \
	        MACH_SEND_NOTIFY | MACH_SEND_OVERRIDE | \
	        MACH_SEND_TRAILER | MACH_SEND_NOIMPORTANCE | \
	        MACH_SEND_SYNC_OVERRIDE | MACH_SEND_PROPAGATE_QOS | \
	        MACH_SEND_FILTER_NONFATAL | \
	        MACH_SEND_SYNC_BOOTSTRAP_CHECKIN | \
	        MACH_MSG_STRICT_REPLY | MACH_RCV_GUARDED_DESC)

#define MACH_RCV_USER (MACH_RCV_MSG | MACH_RCV_TIMEOUT | \
	        MACH_RCV_LARGE | MACH_RCV_LARGE_IDENTITY | \
	        MACH_RCV_VOUCHER | MACH_RCV_TRAILER_MASK | \
	        MACH_RCV_SYNC_WAIT | MACH_RCV_SYNC_PEEK  | \
	        MACH_RCV_GUARDED_DESC | MACH_MSG_STRICT_REPLY)

#define MACH64_MSG_OPTION_CFI_MASK (MACH64_SEND_KOBJECT_CALL | MACH64_SEND_MQ_CALL | \
	        MACH64_SEND_ANY | MACH64_SEND_DK_CALL)

#define MACH64_RCV_USER          (MACH_RCV_USER | MACH64_MSG_VECTOR)

#define MACH_MSG_OPTION_USER     (MACH_SEND_USER | MACH_RCV_USER)

#define MACH64_MSG_OPTION_USER   (MACH64_SEND_USER | MACH64_RCV_USER)

#define MACH64_SEND_USER (MACH_SEND_USER | MACH64_MSG_VECTOR | \
	        MACH64_MSG_OPTION_CFI_MASK)

/* The options implemented by the library interface to mach_msg et. al. */
#define MACH_MSG_OPTION_LIB      (MACH_SEND_INTERRUPT | MACH_RCV_INTERRUPT)

#define MACH_SEND_WITH_STRICT_REPLY(_opts) (((_opts) & (MACH_MSG_STRICT_REPLY | MACH_SEND_MSG)) == \
	                                    (MACH_MSG_STRICT_REPLY | MACH_SEND_MSG))

#define MACH_SEND_REPLY_IS_IMMOVABLE(_opts) (((_opts) & (MACH_MSG_STRICT_REPLY | \
	                                                 MACH_SEND_MSG | MACH_RCV_MSG | \
	                                                 MACH_RCV_GUARDED_DESC)) == \
	                                     (MACH_MSG_STRICT_REPLY | MACH_SEND_MSG | MACH_RCV_GUARDED_DESC))

#define MACH_RCV_WITH_STRICT_REPLY(_opts)  (((_opts) & (MACH_MSG_STRICT_REPLY | MACH_RCV_MSG)) == \
	                                    (MACH_MSG_STRICT_REPLY | MACH_RCV_MSG))

#define MACH_RCV_WITH_IMMOVABLE_REPLY(_opts) (((_opts) & (MACH_MSG_STRICT_REPLY | \
	                                                  MACH_RCV_MSG | MACH_RCV_GUARDED_DESC)) == \
	                                      (MACH_MSG_STRICT_REPLY | MACH_RCV_MSG | MACH_RCV_GUARDED_DESC))

#endif /* MACH_KERNEL_PRIVATE */
#ifdef XNU_KERNEL_PRIVATE

/*
 * Default options to use when sending from the kernel.
 *
 * Until we are sure of its effects, we are disabling
 * importance donation from the kernel-side of user
 * threads in importance-donating tasks.
 * (11938665 & 23925818)
 */
#define MACH_SEND_KERNEL_DEFAULT \
	(mach_msg_option64_t)(MACH_SEND_MSG | MACH_SEND_ALWAYS | MACH_SEND_NOIMPORTANCE)

#define MACH_SEND_KERNEL_IMPORTANCE \
	(mach_msg_option64_t)(MACH_SEND_MSG | MACH_SEND_ALWAYS | MACH_SEND_IMPORTANCE)

#endif /* XNU_KERNEL_PRIVATE */

/*
 * XXXMAC: note that in the case of MACH_RCV_TRAILER_LABELS,
 * we just fall through to mach_msg_max_trailer_t.
 * This is correct behavior since mach_msg_max_trailer_t is defined as
 * mac_msg_mac_trailer_t which is used for the LABELS trailer.
 * It also makes things work properly if MACH_RCV_TRAILER_LABELS is ORed
 * with one of the other options.
 */

#define REQUESTED_TRAILER_SIZE_NATIVE(y)                        \
	((mach_msg_trailer_size_t)                              \
	 ((GET_RCV_ELEMENTS(y) == MACH_RCV_TRAILER_NULL) ?      \
	  sizeof(mach_msg_trailer_t) :                          \
	  ((GET_RCV_ELEMENTS(y) == MACH_RCV_TRAILER_SEQNO) ?    \
	   sizeof(mach_msg_seqno_trailer_t) :                   \
	  ((GET_RCV_ELEMENTS(y) == MACH_RCV_TRAILER_SENDER) ?   \
	   sizeof(mach_msg_security_trailer_t) :                \
	   ((GET_RCV_ELEMENTS(y) == MACH_RCV_TRAILER_AUDIT) ?   \
	    sizeof(mach_msg_audit_trailer_t) :                  \
	    ((GET_RCV_ELEMENTS(y) == MACH_RCV_TRAILER_CTX) ?    \
	     sizeof(mach_msg_context_trailer_t) :               \
	     ((GET_RCV_ELEMENTS(y) == MACH_RCV_TRAILER_AV) ?    \
	      sizeof(mach_msg_mac_trailer_t) :                  \
	     sizeof(mach_msg_max_trailer_t))))))))


#ifdef XNU_KERNEL_PRIVATE

#if defined(__arm64__)
#define REQUESTED_TRAILER_SIZE(is64, y)                                 \
	((mach_msg_trailer_size_t)                              \
	 ((GET_RCV_ELEMENTS(y) == MACH_RCV_TRAILER_NULL) ?      \
	  sizeof(mach_msg_trailer_t) :                          \
	  ((GET_RCV_ELEMENTS(y) == MACH_RCV_TRAILER_SEQNO) ?    \
	   sizeof(mach_msg_seqno_trailer_t) :                   \
	  ((GET_RCV_ELEMENTS(y) == MACH_RCV_TRAILER_SENDER) ?   \
	   sizeof(mach_msg_security_trailer_t) :                \
	   ((GET_RCV_ELEMENTS(y) == MACH_RCV_TRAILER_AUDIT) ?   \
	    sizeof(mach_msg_audit_trailer_t) :                  \
	    ((GET_RCV_ELEMENTS(y) == MACH_RCV_TRAILER_CTX) ?    \
	     ((is64) ? sizeof(mach_msg_context_trailer64_t) : sizeof(mach_msg_context_trailer32_t)) : \
	     ((GET_RCV_ELEMENTS(y) == MACH_RCV_TRAILER_AV) ?    \
	      ((is64) ? sizeof(mach_msg_mac_trailer64_t) : sizeof(mach_msg_mac_trailer32_t)) : \
	       sizeof(mach_msg_max_trailer_t))))))))
#else
#define REQUESTED_TRAILER_SIZE(is64, y) REQUESTED_TRAILER_SIZE_NATIVE(y)
#endif

#else /* XNU_KERNEL_PRIVATE */
#define REQUESTED_TRAILER_SIZE(y) REQUESTED_TRAILER_SIZE_NATIVE(y)
#endif /* XNU_KERNEL_PRIVATE */

/*
 *  Much code assumes that mach_msg_return_t == kern_return_t.
 *  This definition is useful for descriptive purposes.
 *
 *  See <mach/error.h> for the format of error codes.
 *  IPC errors are system 4.  Send errors are subsystem 0;
 *  receive errors are subsystem 1.  The code field is always non-zero.
 *  The high bits of the code field communicate extra information
 *  for some error codes.  MACH_MSG_MASK masks off these special bits.
 */

typedef kern_return_t mach_msg_return_t;

#define MACH_MSG_SUCCESS                0x00000000


#define MACH_MSG_MASK                   0x00003e00
/* All special error code bits defined below. */
#define MACH_MSG_IPC_SPACE              0x00002000
/* No room in IPC name space for another capability name. */
#define MACH_MSG_VM_SPACE               0x00001000
/* No room in VM address space for out-of-line memory. */
#define MACH_MSG_IPC_KERNEL             0x00000800
/* Kernel resource shortage handling an IPC capability. */
#define MACH_MSG_VM_KERNEL              0x00000400
/* Kernel resource shortage handling out-of-line memory. */

#define MACH_SEND_IN_PROGRESS           0x10000001
/* Thread is waiting to send.  (Internal use only.) */
#define MACH_SEND_INVALID_DATA          0x10000002
/* Bogus in-line data. */
#define MACH_SEND_INVALID_DEST          0x10000003
/* Bogus destination port. */
#define MACH_SEND_TIMED_OUT             0x10000004
/* Message not sent before timeout expired. */
#define MACH_SEND_INVALID_VOUCHER       0x10000005
/* Bogus voucher port. */
#define MACH_SEND_INTERRUPTED           0x10000007
/* Software interrupt. */
#define MACH_SEND_MSG_TOO_SMALL         0x10000008
/* Data doesn't contain a complete message. */
#define MACH_SEND_INVALID_REPLY         0x10000009
/* Bogus reply port. */
#define MACH_SEND_INVALID_RIGHT         0x1000000a
/* Bogus port rights in the message body. */
#define MACH_SEND_INVALID_NOTIFY        0x1000000b
/* Bogus notify port argument. */
#define MACH_SEND_INVALID_MEMORY        0x1000000c
/* Invalid out-of-line memory pointer. */
#define MACH_SEND_NO_BUFFER             0x1000000d
/* No message buffer is available. */
#define MACH_SEND_TOO_LARGE             0x1000000e
/* Send is too large for port */
#define MACH_SEND_INVALID_TYPE          0x1000000f
/* Invalid msg-type specification. */
#define MACH_SEND_INVALID_HEADER        0x10000010
/* A field in the header had a bad value. */
#define MACH_SEND_INVALID_TRAILER       0x10000011
/* The trailer to be sent does not match kernel format. */
#define MACH_SEND_INVALID_CONTEXT       0x10000012
/* The sending thread context did not match the context on the dest port */
#define MACH_SEND_INVALID_OPTIONS       0x10000013
/* Send options are invalid. */
#define MACH_SEND_INVALID_RT_OOL_SIZE   0x10000015
/* compatibility: no longer a returned error */
#define MACH_SEND_NO_GRANT_DEST         0x10000016
/* The destination port doesn't accept ports in body */
#define MACH_SEND_MSG_FILTERED          0x10000017
/* Message send was rejected by message filter */
#define MACH_SEND_AUX_TOO_SMALL         0x10000018
/* Message auxiliary data is too small */
#define MACH_SEND_AUX_TOO_LARGE         0x10000019
/* Message auxiliary data is too large */

#define MACH_RCV_IN_PROGRESS            0x10004001
/* Thread is waiting for receive.  (Internal use only.) */
#define MACH_RCV_INVALID_NAME           0x10004002
/* Bogus name for receive port/port-set. */
#define MACH_RCV_TIMED_OUT              0x10004003
/* Didn't get a message within the timeout value. */
#define MACH_RCV_TOO_LARGE              0x10004004
/* Message buffer is not large enough for inline data. */
#define MACH_RCV_INTERRUPTED            0x10004005
/* Software interrupt. */
#define MACH_RCV_PORT_CHANGED           0x10004006
/* compatibility: no longer a returned error */
#define MACH_RCV_INVALID_NOTIFY         0x10004007
/* Bogus notify port argument. */
#define MACH_RCV_INVALID_DATA           0x10004008
/* Bogus message buffer for inline data. */
#define MACH_RCV_PORT_DIED              0x10004009
/* Port/set was sent away/died during receive. */
#define MACH_RCV_IN_SET                 0x1000400a
/* compatibility: no longer a returned error */
#define MACH_RCV_HEADER_ERROR           0x1000400b
/* Error receiving message header.  See special bits. */
#define MACH_RCV_BODY_ERROR             0x1000400c
/* Error receiving message body.  See special bits. */
#define MACH_RCV_INVALID_TYPE           0x1000400d
/* Invalid msg-type specification in scatter list. */
#define MACH_RCV_SCATTER_SMALL          0x1000400e
/* Out-of-line overwrite region is not large enough */
#define MACH_RCV_INVALID_TRAILER        0x1000400f
/* trailer type or number of trailer elements not supported */
#define MACH_RCV_IN_PROGRESS_TIMED      0x10004011
/* Waiting for receive with timeout. (Internal use only.) */
#define MACH_RCV_INVALID_REPLY          0x10004012
/* invalid reply port used in a STRICT_REPLY message */
#define MACH_RCV_INVALID_ARGUMENTS      0x10004013
/* invalid receive arguments, receive has not started */

#ifdef XNU_KERNEL_PRIVATE
#if MACH_FLIPC
#define MACH_PEEK_IN_PROGRESS           0x10008001
/* Waiting for a peek. (Internal use only.) */
#define MACH_PEEK_READY                 0x10008002
/* Waiting for a peek. (Internal use only.) */
#endif /* MACH_FLIPC */
#endif


__BEGIN_DECLS

/*
 *	Routine:	mach_msg_overwrite
 *	Purpose:
 *		Send and/or receive a message.  If the message operation
 *		is interrupted, and the user did not request an indication
 *		of that fact, then restart the appropriate parts of the
 *		operation silently (trap version does not restart).
 *
 *		Distinct send and receive buffers may be specified.  If
 *		no separate receive buffer is specified, the msg parameter
 *		will be used for both send and receive operations.
 *
 *		In addition to a distinct receive buffer, that buffer may
 *		already contain scatter control information to direct the
 *		receiving of the message.
 */
__WATCHOS_PROHIBITED __TVOS_PROHIBITED
extern mach_msg_return_t        mach_msg_overwrite(
	mach_msg_header_t *msg,
	mach_msg_option_t option,
	mach_msg_size_t send_size,
	mach_msg_size_t rcv_size,
	mach_port_name_t rcv_name,
	mach_msg_timeout_t timeout,
	mach_port_name_t notify,
	mach_msg_header_t *rcv_msg,
	mach_msg_size_t rcv_limit);

#ifndef KERNEL

/*
 *	Routine:	mach_msg
 *	Purpose:
 *		Send and/or receive a message.  If the message operation
 *		is interrupted, and the user did not request an indication
 *		of that fact, then restart the appropriate parts of the
 *		operation silently (trap version does not restart).
 */
__WATCHOS_PROHIBITED __TVOS_PROHIBITED
extern mach_msg_return_t        mach_msg(
	mach_msg_header_t *msg,
	mach_msg_option_t option,
	mach_msg_size_t send_size,
	mach_msg_size_t rcv_size,
	mach_port_name_t rcv_name,
	mach_msg_timeout_t timeout,
	mach_port_name_t notify);

#if PRIVATE
#if defined(__LP64__) || defined(__arm64__)
__API_AVAILABLE(macos(13.0), ios(16.0), tvos(16.0), watchos(9.0))
__IOS_PROHIBITED __WATCHOS_PROHIBITED __TVOS_PROHIBITED
extern mach_msg_return_t mach_msg2_internal(
	void *data,
	mach_msg_option64_t option64,
	uint64_t msgh_bits_and_send_size,
	uint64_t msgh_remote_and_local_port,
	uint64_t msgh_voucher_and_id,
	uint64_t desc_count_and_rcv_name,
	uint64_t rcv_size_and_priority,
	uint64_t timeout);

__API_AVAILABLE(macos(13.0), ios(16.0), tvos(16.0), watchos(9.0))
__IOS_PROHIBITED __WATCHOS_PROHIBITED __TVOS_PROHIBITED
static inline mach_msg_return_t
mach_msg2(
	void *data,
	mach_msg_option64_t option64,
	mach_msg_header_t header,
	mach_msg_size_t send_size,
	mach_msg_size_t rcv_size,
	mach_port_t rcv_name,
	uint64_t timeout,
	uint32_t priority)
{
	mach_msg_base_t *base;
	mach_msg_size_t descriptors;

	if (option64 & MACH64_MSG_VECTOR) {
		base = (mach_msg_base_t *)((mach_msg_vector_t *)data)->msgv_data;
	} else {
		base = (mach_msg_base_t *)data;
	}

	if ((option64 & MACH64_SEND_MSG) &&
	    (base->header.msgh_bits & MACH_MSGH_BITS_COMPLEX)) {
		descriptors = base->body.msgh_descriptor_count;
	} else {
		descriptors = 0;
	}

#define MACH_MSG2_SHIFT_ARGS(lo, hi) ((uint64_t)hi << 32 | (uint32_t)lo)
	return mach_msg2_internal(data, option64,
	           MACH_MSG2_SHIFT_ARGS(header.msgh_bits, send_size),
	           MACH_MSG2_SHIFT_ARGS(header.msgh_remote_port, header.msgh_local_port),
	           MACH_MSG2_SHIFT_ARGS(header.msgh_voucher_port, header.msgh_id),
	           MACH_MSG2_SHIFT_ARGS(descriptors, rcv_name),
	           MACH_MSG2_SHIFT_ARGS(rcv_size, priority), timeout);
#undef MACH_MSG2_SHIFT_ARGS
}
#endif
#endif /* PRIVATE */

/*
 *  Routine:    mach_voucher_deallocate
 *  Purpose:
 *      Deallocate a mach voucher created or received in a message.  Drops
 *      one (send right) reference to the voucher.
 */
__WATCHOS_PROHIBITED __TVOS_PROHIBITED
extern kern_return_t            mach_voucher_deallocate(
	mach_port_name_t voucher);

#elif defined(MACH_KERNEL_PRIVATE)

/*!
 * @typedef mach_msg_send_uctx_t
 *
 * @brief
 * Data structure used for the send half of a @c mach_msg() call from userspace.
 *
 * @discussion
 * Callers must fill the @c send_header, @c send_dsc_count with the user header
 * being sent, as well as the parameters of user buffers used for send
 * (@c send_{msg,aux}_{addr,size}).
 *
 * @field send_header           a copy of the user header being sent.
 * @field send_dsc_count        the number of descriptors being sent.
 *                              must be 0 if the header doesn't have
 *                              the MACH_MSGH_BITS_COMPLEX bit set.
 * @field send_msg_addr         the userspace address for the message being sent.
 * @field send_msg_size         the size of the message being sent.
 * @field send_aux_addr         the userspace address for the auxiliary data
 *                              being sent (will be 0 if not using a vector
 *                              operation)
 * @field send_aux_size         the size for the auxiliary data being sent.
 *
 * @field send_dsc_mask         internal field being used during right copyin
 *                              of descriptors.
 * @field send_dsc_usize        the size (in bytes) of the user representation
 *                              of descriptors being sent.
 * @field send_dsc_port_count   number of ports being sent in descriptors
 *                              (both in port or port array descriptors).
 * @field send_dsc_vm_size      kernel wired memory (not counting port arrays)
 *                              needed to copyin this message.
 */
typedef struct {
	/* send context/arguments */
	mach_msg_user_header_t send_header;
	mach_msg_size_t        send_dsc_count;

	mach_vm_address_t      send_msg_addr;
	mach_vm_address_t      send_aux_addr;
	mach_msg_size_t        send_msg_size;
	mach_msg_size_t        send_aux_size;

	/* filled by copyin */
	uint64_t               send_dsc_mask;
	mach_msg_size_t        send_dsc_usize;
	mach_msg_size_t        send_dsc_port_count;
	vm_size_t              send_dsc_vm_size;
} mach_msg_send_uctx_t;


/*!
 * @typedef mach_msg_recv_bufs_t
 *
 * @brief
 * Data structure representing the buffers being used by userspace to receive
 * a message.
 *
 * @field recv_msg_addr         the userspace address for the message
 *                              receive buffer.
 * @field recv_msg_size         the size of the message receive buffer.
 *
 * @field recv_aux_addr         the userspace address for the auxiliary data
 *                              receive buffer (will be 0 if not using a vector
 *                              operation)
 * @field recv_aux_size         the size for the auxiliary data receive buffer.
 */
typedef struct {
	mach_vm_address_t      recv_msg_addr;
	mach_vm_address_t      recv_aux_addr;
	mach_msg_size_t        recv_msg_size;
	mach_msg_size_t        recv_aux_size;
} mach_msg_recv_bufs_t;


/*!
 * @typedef mach_msg_recv_result_t
 *
 * @brief
 * Data structure representing the results of a receive operation,
 * in the context of the user task receiving that message.
 *
 * @field msgr_msg_size         the user size of the message being copied out
 *                              (not including trailer or auxiliary data).
 *                              set for MACH_RCV_TOO_LARGE or MACH_MSG_SUCCESS,
 *                              0 otherwise.
 *
 * @field msgr_trailer_size     the trailer size of the message being copied out.
 *                              set MACH_MSG_SUCCESS, 0 otherwise.
 *
 * @field msgr_aux_size         the auxiliary data size of the message being
 *                              copied out.
 *                              set for MACH_RCV_TOO_LARGE or MACH_MSG_SUCCESS,
 *                              0 otherwise.
 *
 * @field msgr_recv_name        the name of the port receiving the message.
 *                              Set for MACH_RCV_TOO_LARGE,
 *                              or to MSGR_PSEUDO_RECEIVE for pseudo-receive.
 *
 * @field msgr_seqno            the sequence number for the message being
 *                              received.
 *
 * @field msgr_context          the mach port context fort the port receiving
 *                              the message.
 *
 * @field msgr_priority         the pthread priority of the message being
 *                              received.
 *
 * @field msgr_qos_ovrd         the qos override for the message being received.
 */
typedef struct {
	/* general info about the message being copied out */
	mach_msg_size_t        msgr_msg_size;
	mach_msg_size_t        msgr_trailer_size;
	mach_msg_size_t        msgr_aux_size;
#define MSGR_PSEUDO_RECEIVE    (0xfffffffe)
	mach_port_name_t       msgr_recv_name;
	mach_port_seqno_t      msgr_seqno;
	mach_port_context_t    msgr_context;

	/* metadata for the sake of kevent only */
	uint32_t               msgr_priority;
	mach_msg_qos_t         msgr_qos_ovrd;
} mach_msg_recv_result_t;

extern mach_msg_return_t mach_msg_receive_results(
	mach_msg_recv_result_t *msg); /* out only, can be NULL */

#endif  /* KERNEL */

__END_DECLS

#endif  /* _MACH_MESSAGE_H_ */
