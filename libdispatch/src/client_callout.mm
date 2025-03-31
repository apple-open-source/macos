/*
 * Copyright (c) 2011-2014 Apple Inc. All rights reserved.
 *
 * @APPLE_APACHE_LICENSE_HEADER_START@
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @APPLE_APACHE_LICENSE_HEADER_END@
 */

#include "internal.h"

#if USE_OBJC

#pragma mark -
#pragma mark dispatch_client_callout

// The client callouts abort on uncaught exceptions thrown from client callouts
// (see rdar://8577499)
//
// This is implemented by using the DISPATCH_NOTHROW attribute on the
// declaration of these functions, which results in the program terminating in
// case any exception would escape them. This is very important since it allows
// the compiler to retain the call stack of where the exception was originally
// thrown (in the client code), which wouldn't be the case if we used a
// try-catch to implement this. For more context, see rdar://20746379
//
// Also note that this requires compiling this code as Objective-C++ instead of
// Objective-C, otherwise the DISPATCH_NOTHROW attribute doesn't do anything.
#if DISPATCH_USE_CLIENT_CALLOUT && !__USING_SJLJ_EXCEPTIONS__ && \
		OS_OBJECT_HAVE_OBJC2

// At time of writing, if the unwinder runs into a __nothrow__ attributed
// function, it calls std::terminate, dropping the call stack that originally
// threw the exception. To work around this, we override the personality
// routine with one that returns an error to the unwinder, which causes the
// crashlog to include the throwing callstack. Once the compiler implementation
// of __nothrow__ is improved, we can remove this personality routine
#include <unwind.h>

extern "C" _Unwind_Reason_Code
__dispatch_noexcept_personality(int, _Unwind_Action, uint64_t,
		struct _Unwind_Exception *, struct _Unwind_Context *);

extern "C" __attribute__((used)) _Unwind_Reason_Code
__dispatch_noexcept_personality(int version, _Unwind_Action action,
		uint64_t exceptionClass, struct _Unwind_Exception *exceptionObject,
		struct _Unwind_Context *context)
{
	(void)version;
	(void)action;
	(void)exceptionClass;
	(void)exceptionObject;
	(void)context;
	return _URC_FATAL_PHASE1_ERROR;
}

// The .cfi_personality directive is used to control the personality routine
// (used for exception handling) encoded in the CFI (Call Frame Information).
// We use that directive to override the normal personality routine with one
// that always reports an error, leading the Phase 1 of unwinding to abort the
// program.
//
// The encoding we use here is 155, which is 'indirect pcrel sdata4'
// (DW_EH_PE_indirect | DW_EH_PE_pcrel | DW_EH_PE_sdata4). This is known to
// work for x86_64 and arm64.
#define OVERRIDE_PERSONALITY_ASSEMBLY() \
	__asm__(".cfi_personality 155, ___dispatch_noexcept_personality")

#undef _dispatch_client_callout
extern "C" void
_dispatch_client_callout(void *ctxt, dispatch_function_t f)
{
	OVERRIDE_PERSONALITY_ASSEMBLY();
	f(ctxt);
	__asm__ __volatile__("");  // prevent tailcall
}

#undef _dispatch_client_callout2
extern "C" void
_dispatch_client_callout2(void *ctxt, size_t i, void (*f)(void *, size_t))
{
	OVERRIDE_PERSONALITY_ASSEMBLY();
	f(ctxt, i);
	__asm__ __volatile__("");  // prevent tailcall
}

#if HAVE_MACH
#undef _dispatch_client_callout3
extern "C" void
_dispatch_client_callout3(void *ctxt, dispatch_mach_reason_t reason,
		dispatch_mach_msg_t dmsg, dispatch_mach_async_reply_callback_t f)
{
	OVERRIDE_PERSONALITY_ASSEMBLY();
	f(ctxt, reason, dmsg);
	__asm__ __volatile__("");  // prevent tailcall
}

#undef _dispatch_client_callout4
extern "C" void
_dispatch_client_callout4(void *ctxt, dispatch_mach_reason_t reason,
		dispatch_mach_msg_t dmsg, mach_error_t error,
		dispatch_mach_handler_function_t f)
{
	OVERRIDE_PERSONALITY_ASSEMBLY();
	f(ctxt, reason, dmsg, error);
	__asm__ __volatile__("");  // prevent tailcall
}
#endif // HAVE_MACH

#undef _dispatch_client_callout3_a
extern "C" void
_dispatch_client_callout3_a(void *ctxt, size_t i, size_t w,
		dispatch_apply_attr_function_t f)
{
	OVERRIDE_PERSONALITY_ASSEMBLY();
	f(ctxt, i, w);
	__asm__ __volatile__("");  // prevent tailcall
}

#endif // DISPATCH_USE_CLIENT_CALLOUT

#endif // USE_OBJC
