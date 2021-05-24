/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef __PTHREAD_JIT_PRIVATE_H__
#define __PTHREAD_JIT_PRIVATE_H__

#include <pthread/pthread.h>

#if __has_feature(assume_nonnull)
_Pragma("clang assume_nonnull begin")
#endif
__BEGIN_DECLS

#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || defined(_DARWIN_C_SOURCE) || defined(__cplusplus)

/*!
 * @typedef pthread_jit_write_callback_t
 * The type of a function that can be supplied to {@link
 * pthread_jit_write_with_callback_np} to write to the MAP_JIT region while it
 * is writeable.
 *
 * @param ctx
 * A pointer to context that will be passed through to the callback function.
 *
 * @result
 * A result code to be returned to the caller of @{link
 * pthread_jit_write_with_callback_np}.  The system does not interpret/act on
 * the value of this result.
 */
typedef int (*pthread_jit_write_callback_t)(void * _Nullable ctx);

/*!
 * @define PTHREAD_JIT_WRITE_ALLOW_CALLBACKS_NP
 * A macro to be used at file scope to list the functions allowed to be passed
 * to {@link pthread_jit_write_with_callback_np} to write to the MAP_JIT region
 * while it is writeable.  It may be invoked only once per executable/library.
 *
 * @param callbacks
 * The pthread_jit_write_callback_t functions to allow.  They should be supplied
 * as a comma-delimited list.
 */
#if (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L) || defined(__OBJC__) || defined(__cplusplus)
#define PTHREAD_JIT_WRITE_ALLOW_CALLBACKS_NP(...) \
		__attribute__((__used__, __section__("__DATA_CONST,__pth_jit_func"))) \
		static const pthread_jit_write_callback_t __pthread_jit_write_callback_allowlist[] = { \
			__VA_ARGS__, NULL \
		}
#endif /* (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L) || defined(__OBJC__) || defined(__cplusplus) */

/*!
 * @function pthread_jit_write_with_callback_np
 *
 * @abstract
 * Toggles per-thread write-protection of the MAP_JIT region to writeable,
 * invokes an allowed callback function to write to it, and toggles protection
 * back to executable.
 *
 * @param callback
 * The callback function to invoke to write to the MAP_JIT region.  It must be
 * statically allowed using {@link PTHREAD_JIT_WRITE_ALLOW_CALLBACKS_NP}.
 *
 * @param ctx
 * Context to pass through to the invocation of the callback function.
 *
 * @result
 * The result code returned by the callback function.
 *
 * @discussion
 * This function assumes that the MAP_JIT region has executable protection when
 * called.  It is therefore invalid to call it recursively from within a write
 * callback.  The implementation does not detect such invalid recursive calls,
 * so the client is responsible for preventing them.
 *
 * Callbacks _must not_ perform any non-local transfer of control flow (e.g.
 * throw an exception, longjmp(3)), as doing so would leave the MAP_JIT region
 * writeable.
 *
 * On systems where pthread_jit_write_protect_supported_np(3) is false, this
 * function calls @callback directly and does nothing else.
 *
 * Only callbacks in libraries/images present at process start-up are allowed -
 * callbacks in images loaded dynamically via dlopen(3)/etc. are not permitted.
 *
 * This function only enforces that @callback is allowed if the caller has the
 * com.apple.security.cs.jit-write-allowlist entitlement.  That entitlement also
 * disallows use of pthread_jit_write_protect_np(3).  Adopting the entitlement
 * is therefore crucial in realizing the security benefits of this interface.
 *
 * If the entitlement is not present then this function toggles protection of
 * the MAP_JIT to writeable, calls @callback and then toggles protection back to
 * executable, without validating that @callback is an allowed function.  This
 * behavior is intended to permit independent adoption of this interface by
 * libraries - once all libraries in an application have adopted, the
 * application should add the entitlement.
 *
 * The goal of this interface is to allow applications that execute JIT-compiled
 * code to mitigate against attempts from attackers to escalate to code
 * execution by getting their own instructions written to the MAP_JIT region.
 *
 * Callbacks should assume an attacker can control the input to this function.
 * They must therefore carefully validate the data that they are passed and do
 * so using as little attackable state as possible. This means simplifying
 * control flow and avoiding spills of sensitive registers (e.g. those used for
 * validation or control flow).
 *
 * In the event a callback detects that its input is invalid, it should either
 * abort in the simplest fashion possible (preferring e.g. __builtin_trap() over
 * abort(3), the latter being encumbered by various conformance requirements) or
 * return a result indicating failure.
 */
__API_AVAILABLE(macos(11.4))
__API_UNAVAILABLE(ios, tvos, watchos, bridgeos, driverkit)
__SWIFT_UNAVAILABLE_MSG("This interface cannot be safely used from Swift")
int pthread_jit_write_with_callback_np(
		pthread_jit_write_callback_t _Nonnull callback, void * _Nullable ctx);

#endif /* (!_POSIX_C_SOURCE && !_XOPEN_SOURCE) || _DARWIN_C_SOURCE || __cplusplus */

__END_DECLS
#if __has_feature(assume_nonnull)
_Pragma("clang assume_nonnull end")
#endif

#endif /* __PTHREAD_JIT_PRIVATE_H__ */
