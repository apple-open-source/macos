#include <darwintest.h>
#include <mach-o/dyld_priv.h>
#include <thread_stack_pcs.h>
#include <execinfo.h>
#include <fake_swift_async.h>
#include <pthread/private.h>
#include <stdint.h>
#include <stdlib.h>

#if __arm64e__
#define TARGET_CPU_ARM64E true
#else
#define TARGET_CPU_ARM64E false
#endif

static void *test_function_ret_addr;

__attribute__((noinline))
static void test_no_async() {
    vm_address_t* callstack[16], asyncstack[16];
    unsigned frames1, frames2, i;
    int ret1 = thread_stack_pcs(callstack, 16, &frames1);
    int ret2 = thread_stack_async_pcs(asyncstack, 16, &frames2);

    T_EXPECT_EQ(ret1, 0 , "thread_stack_pcs finds no extended frame");
    T_EXPECT_EQ(ret2, 0 , "thread_stack_async_pcs finds no extended frame");
    T_EXPECT_EQ(frames1, frames2 ,
                "thread_stack_async_pcs and thread_stack_pcs return same amount of frames");
    // Start at frame 2 as frame 0 is thread_{stack,async}_pcs and
    // frame 1 is this frame which has 2 different call sites for those 2
    // functions.
    for (i = 2; i<frames1; ++i) {
        T_EXPECT_EQ(callstack[i], asyncstack[i],
                "thread_stack_async_pcs and thread_stack_pcs return same frames");
    }
}

__attribute__((noinline))
static void test(void *unused __attribute__((unused)), bool async) {
    vm_address_t* callstack[16];

    if (async) {
        unsigned frames;
        int ret = thread_stack_async_pcs(callstack, 16, &frames);
        T_EXPECT_EQ(ret, FAKE_TASK_ID, "thread_stack_async_pcs detects an async frame");

        // The 5 frames we expect are
        // 0 thread_stack_async_pcs
        // 1 test
        // 2 fake_async_frame
        // 3 level2_func
        // 4 level1_func
        T_EXPECT_EQ(frames, 5, "Got the right number of async frames");
        T_EXPECT_EQ(callstack[2], __builtin_return_address(0), "Found fake_async_frame");
        T_EXPECT_EQ(callstack[3], ptrauth_strip((void*)&level2_func, ptrauth_key_function_pointer) + 1, "Found level2_func");
        T_EXPECT_EQ(callstack[4], ptrauth_strip((void*)&level1_func, ptrauth_key_function_pointer) + 1, "Found level1_func");
  } else {
        unsigned frames;
        int ret = thread_stack_pcs(callstack, 16, &frames);
        T_EXPECT_EQ(ret, 1, "thread_stack_pcs detects an async frame");
        // The 5 frames we expect are
        // 0 thread_stack_pcs
        // 1 test
        // 2 fake_async_frame
        // 3 <Test function>
        // ... Potential test runner frames
        T_EXPECT_GE(frames, 4, "Got the right number of stack frames");
        T_EXPECT_EQ(callstack[2], __builtin_return_address(0), "Found fake_async_frame");
        T_EXPECT_EQ(callstack[3], test_function_ret_addr, "Found test runner");
  }
}


__attribute__((noinline))
static void fake_async_frame(bool async) {
    test_function_ret_addr = __builtin_return_address(0);
    uint64_t *fp = __builtin_frame_address(0);
    // We cannot use a variable of pointer type, because this ABI is valid
    // on arm64_32 where pointers are 32bits, but the context pointer will
    // still be stored in a 64bits slot on the stack.
    /* struct fake_async_context * */ uint64_t ctx  = (uintptr_t)&level2;

    // The Swift runtime stashes the current async task address in its 3rd
    // private TSD slot.
    _pthread_setspecific_direct(__PTK_FRAMEWORK_SWIFT_KEY3, &task);

#if __LP64__ || __ARM64_ARCH_8_32__
    // The signature of an async frame on the OS stack is:
    // [ <AsyncContext address>, <Saved FP | (1<<60)>, <return address> ]
    // The Async context must be right before the saved FP on the stack. This
    // should happen naturraly in an optimized build as it is the only
    // variable on the stack.
    // This function cannot use T_ASSERT_* beacuse it changes the stack
    // layout.
    assert((uintptr_t)fp - (uintptr_t)&ctx == 8);

    // Modify the saved FP on the stack to include the async frame marker
    *fp |= (0x1ll << 60);
    test(&ctx, async);
    *fp ^= (0x1ll << 60);
#endif
    test_no_async();
}

T_DECL(thread_stack_pcs, "tests thread_stack_pcs in the presence of an async frame")
{
    fake_async_frame(false);
}

T_DECL(thread_stack_async_pcs, "tests thread_stack_async_pcs")
{
    fake_async_frame(true);
}

//
// split_stack_call_impl assembly implementation.
//

#if defined(__x86_64__)
asm("\t.private_extern _split_stack_call_impl\n\t"
    ".p2align    4, 0x90\n"
"_split_stack_call_impl:\n\t"
    "## new_stack -> %rdi\n\t"
    "## new_stack_size -> %rsi\n\t"
    "## func -> %rdx\n\t"
    "## data -> %rcx\n\t"
    ".cfi_startproc\n\t"
    "pushq %rbp\n\t"
    ".cfi_def_cfa_offset 16\n\t"
    ".cfi_offset %rbp, -16\n\t"
    "movq %rsp, %rbp\n\t"
    ".cfi_def_cfa_register %rbp\n\t"
    "# setup %rsp in the new stack space\n\t"
    "leaq (%rdi, %rsi), %rsp\n\t"
    "# move 'data' argument in %rdi\n\t"
    "movq %rcx, %rdi\n\t"
	"# move return address in %rsi\n\t"
	"movq 0x8(%rbp), %rsi\n\t"
    "# indirect jump to the function\n\t"
    "callq *%rdx\n\t"
    "# restore original %rsp\n\t"
    "movq %rbp, %rsp\n\t"
    "popq %rbp\n\t"
    "retq\n\t"
    ".cfi_endproc");
#elif defined(__arm64__)
#ifdef __arm64e__
asm(".macro FN_PROLOG\n\t"
    "pacibsp\n"
".endmacro\n"
".macro FN_EPILOG\n\t"
    "retab\n"
".endmacro\n"
".macro RETADDR reg\n\t"
	"mov \\reg, x30\n\t"
	"xpaci \\reg\n"
".endmacro\n"
".macro INDCALL reg\n\t"
    "blraaz \\reg\n"
".endmacro");

#else
asm(".macro FN_PROLOG\n"
".endmacro\n"
".macro FN_EPILOG\n\t"
    "ret\n"
".endmacro\n"
".macro RETADDR reg\n\t"
	"mov \\reg, x30\n"
".endmacro\n"
".macro INDCALL reg\n\t"
    "blr \\reg\n"
".endmacro");
#endif
asm("\t.private_extern _split_stack_call_impl\n\t"
	  ".p2align	2\n"
"_split_stack_call_impl:\n\t"
    "; new_stack -> x0\n\t"
    "; new_stack_size -> x1\n\t"
    "; func -> x2\n\t"
    "; data -> x3\n\t"
    ".cfi_startproc\n\t"
    "FN_PROLOG\n\t"
    "stp x29, x30, [sp, #-16]!\n\t"
    "mov x29, sp\n\t"
	".cfi_def_cfa w29, 16\n\t"
	".cfi_offset w30, -8\n\t"
	".cfi_offset w29, -16\n\t"
    "; setup `sp` in the new stack space\n\t"
    "add sp, x0, x1\n\t"
    "; move 'data' argument in `x0`\n\t"
    "mov x0, x3\n\t"
	"; move our return address (in lr) into `x1`\n\t"
	"RETADDR x1\n\t"
    "; indirect call to the function\n\t"
    "INDCALL x2\n\t"
    "; restore original `sp`\n\t"
    "mov sp, x29\n\t"
    "ldp x29, x30, [sp], #16\n\t"
    "FN_EPILOG\n\t"
    ".cfi_endproc");
#else
#error "Unsupported architecture"
#endif

// Passes ctx to f in the first argument, and the return address of
// _split_stack_call_impl in the second
void split_stack_call_impl(void * stack, size_t stack_size, void (*f)(void*, void*), void *ctx);

__attribute__((noinline)) __attribute__((not_tail_called))
void non_default_stack_test_l2(void *ret2, void *ret3, void *ret4)
{
	void *ret1 = __builtin_return_address(0);

	vm_address_t callstack[16];
	unsigned frames;
	thread_stack_pcs(callstack, 16, &frames);

	// The 4 pcs we expect
	// 0 thread_stack_pcs
	// 1 non_default_stack_test_l2
	// 2 non_default_stack_test (in ret1)
	// 3 split_stack_call_impl (in ret2)
	// 4 testmain (in ret3)
	// 5 darwintest testrunner (in ret4)
	// ...
	T_EXPECT_GE(frames, 4, "Got the right number of stack frames");
	T_EXPECT_EQ((void *)callstack[2], ret1, "Found non_default_stack_test_l2 frame");
	T_EXPECT_EQ((void *)callstack[3], ret2, "Found non_default_stack_test frame");
	T_EXPECT_EQ((void *)callstack[4], ret3, "Found split_stack_call_impl frame");
	T_EXPECT_EQ((void *)callstack[5], ret4, "Found test main frame");
}

__attribute__((noinline))
void non_default_stack_test(void *testmain_retaddr, void *stack_call_retaddr)
{
	void *our_retaddr = __builtin_return_address(0);
	non_default_stack_test_l2(our_retaddr, stack_call_retaddr, testmain_retaddr);
}

T_DECL(thread_stack_pcs_alt_stack, "tests thread_stack_pcs when called from a non-default stack")
{
	const size_t stack_size = 32768;
	void *stack = malloc(stack_size);
	T_ASSERT_NOTNULL(stack, "Allocated non-default stack %p", stack);

	void *ret_addr = __builtin_return_address(0);
	split_stack_call_impl(stack, stack_size, non_default_stack_test, ret_addr);

	free(stack);
}

// Sets some high bits in the stacked link register to corrupt the fp chain,
// then calls into backtrace() to make sure we don't crash inside libc.
void call_func_with_broken_fp_chain(void (*f)(void), void *ssi_retaddr);


// On 64 bit systems, increment the stacked frame pointer by 16GB to make it
// unlikely that it lands on a mapped address. On 32 bit systems, we need to
// decrease this to 1GB to avoid overflowing to the same (legal) value
#ifdef __LP64__
#define FP_BREAK_OFFSET "0x400000000"
#else
#define FP_BREAK_OFFSET "0x40000000"
#endif

#if defined(__x86_64__)
asm("\t.private_extern _call_func_with_broken_fp_chain\n\t"
    ".p2align    4, 0x90\n"
"_call_func_with_broken_fp_chain:\n\t"
    "## ctx -> %rdi\n\t"
    "## ssi_retaddr (unused) -> %rsi\n\t"
    ".cfi_startproc\n\t"
    "pushq %rbp\n\t"
    ".cfi_def_cfa_offset 16\n\t"
    ".cfi_offset %rbp, -16\n\t"
    "movq %rsp, %rbp\n\t"
	"# increment bp by 16GB to get callee to push a broken fp chain\n\t"
	"movq $" FP_BREAK_OFFSET ", %rsi\n\t"
	"addq %rsi, %rbp\n\t"
    ".cfi_def_cfa_register %rbp\n\t"
    "# indirect call to f\n\t"
    "callq *%rdi\n\t"
	"# decrement bp by 16GB before moving it to sp\n\t"
	"movq $" FP_BREAK_OFFSET ", %rsi\n\t"
	"subq %rsi, %rbp\n\t"
    "# restore original %rsp\n\t"
    "movq %rbp, %rsp\n\t"
	"popq %rbp\n\t"
    "retq\n\t"
    ".cfi_endproc");
#elif defined(__arm64__)
asm("\t.private_extern _call_func_with_broken_fp_chain\n\t"
	  ".p2align	2\n"
"_call_func_with_broken_fp_chain:\n\t"
    "; f -> x0\n\t"
    "; ssi_retaddr (unused) -> x1\n\t"
    ".cfi_startproc\n\t"
    "FN_PROLOG\n\t"
	"; store fp+16GB on the stack (clobbers unused arg1)\n\t"
	"mov x1, #" FP_BREAK_OFFSET "\n\t"
	"add x1, x29, x1\n\t"
    "stp x1, x30, [sp, #-16]!\n\t"
    "mov x29, sp\n\t"
	".cfi_def_cfa w29, 16\n\t"
	".cfi_offset w30, -8\n\t"
	".cfi_offset w29, -16\n\t"
    "; indirect call to f\n\t"
    "INDCALL x0\n\t"
    "; restore original `sp`\n\t"
    "mov sp, x29\n\t"
    "ldp x29, x30, [sp], #16\n\t"
	"; Remove 16GB offset from fp on stack\n\t"
	"mov x1, #" FP_BREAK_OFFSET "\n\t"
    "sub x29, x29, x1\n\t"
    "FN_EPILOG\n\t"
    ".cfi_endproc");
#else
#error "Unsupported architecture"
#endif

static void call_backtrace_helper(void)
{
	// called on non-default stack with a broken fp chain.
	void * backtrace_buffer[10];
	int frames = backtrace(&backtrace_buffer[0], 10);
	// Without the maximum frame size, we'd typically crash here. Assert that
	// we didn't enumerate past the broken fp linkage, so that we're guaranteed
	// to assert at least one thing in this test case and get a pass
	T_ASSERT_LT(frames, 5, "Didn't enumerate too many frames\n", frames);
}

T_DECL(thread_stack_pcs_alt_broken_fp_chain,
		"Check that backtrace() doesn't crash when fp chain is broken")
{
	const size_t stack_size = 32768;
	void *stack = malloc(stack_size);
	T_ASSERT_NOTNULL(stack, "Allocated non-default stack %p", stack);

	split_stack_call_impl(stack, stack_size,
			(void *)&call_func_with_broken_fp_chain, (void *)&call_backtrace_helper);

	free(stack);
}

struct many_frames_args {
	vm_address_t *buffer;
	unsigned buffer_size;
	unsigned recurse_depth;
	unsigned ret_frames;
	bool use_backtrace;
};

__attribute__((noinline))
__attribute__((disable_tail_calls))
void recursive_thread_stack_pcs(struct many_frames_args* args, void *unused)
{
	T_QUIET; T_ASSERT_NE(args->recurse_depth, 0, "Invalid recurse depth");
	if (--args->recurse_depth) {
		return recursive_thread_stack_pcs(args, NULL);
	} else {
		if (args->use_backtrace) {
			args->ret_frames = backtrace(args->buffer, args->buffer_size);
		} else {
			thread_stack_pcs(args->buffer, args->buffer_size, &args->ret_frames);
		}
	}
}

T_DECL(thread_stack_pcs_alt_exact_frames,
		"Enumerate a max of 16 frames with 16 frames in the non-default stack")
{
	const size_t stack_size = 32768;
	void *stack = malloc(stack_size);
	T_ASSERT_NOTNULL(stack, "Allocated non-default stack %p", stack);

	// Oversize buffer to avoid breaking the stack and crashing
	vm_address_t buffer[32] = { 0 };
	struct many_frames_args arg = {
		.buffer = &buffer,
		.buffer_size = 16,
		 // The top frame is thread_stack_pcs, so only recurse 15 times
		.recurse_depth = 15,
		.ret_frames = 0,
		.use_backtrace = false,
	};

	split_stack_call_impl(stack, stack_size, recursive_thread_stack_pcs, &arg);

	T_ASSERT_LE(arg.ret_frames, 16, "Too many frames walked");
	T_ASSERT_EQ(buffer[16], 0, "Buffer not overwritten");

	free(stack);
}

T_DECL(thread_stack_pcs_non_default_many_frames,
		"Enumerate a max of 16 frames with more than that in non-default stack")
{
	const size_t stack_size = 32768;
	void *stack = malloc(stack_size);
	T_ASSERT_NOTNULL(stack, "Allocated non-default stack %p", stack);

	vm_address_t buffer[32] = { 0 };
	struct many_frames_args arg = {
		.buffer = &buffer,
		.buffer_size = 16,
		.recurse_depth = 20,
		.ret_frames = 0,
		.use_backtrace = false,
	};

	split_stack_call_impl(stack, stack_size, recursive_thread_stack_pcs, &arg);

	T_ASSERT_LE(arg.ret_frames, 16, "Too many frames walked");
	T_ASSERT_EQ(buffer[16], 0, "Buffer not overwritten");

	free(stack);
}

T_DECL(thread_stack_pcs_alt_stack_with_backtrace,
		"Check that we can skip the top frame in non-default stack")
{
	const size_t stack_size = 32768;
	void *stack = malloc(stack_size);
	T_ASSERT_NOTNULL(stack, "Allocated non-default stack %p", stack);

	vm_address_t buffer[16] = { 0 };
	struct many_frames_args arg = {
		.buffer = &buffer,
		.buffer_size = 16,
		.recurse_depth = 20,
		.ret_frames = 0,
		// backtrace() sets skip to 1 (to drop the backtrace() frame)
		.use_backtrace = true,
	};

	split_stack_call_impl(stack, stack_size, recursive_thread_stack_pcs, &arg);

	T_ASSERT_EQ(arg.ret_frames, 16, "Walked 16 frames");
	// The top two frames should both be within recursive_thread_stack_pcs.
	// They'll be slightly different from each other, but should both be within
	// the same function. To check the latter, assume that they should be
	// within 1k of each other
	uintptr_t start =
			(uintptr_t)ptrauth_strip((void*)&recursive_thread_stack_pcs,
			ptrauth_key_function_pointer);
	const vm_address_t max_allowed_difference = 1024;
	T_ASSERT_GT((uintptr_t)buffer[0], start,
		"Top frame is in recursive_thread_stack_pcs");
	T_ASSERT_GT((uintptr_t)buffer[1], start,
		"Second frame is in recursive_thread_stack_pcs");
	T_ASSERT_NE((uintptr_t)buffer[0], (uintptr_t)buffer[1],
		"Top frame and second frame differ (0x%lx, 0x%lx)", buffer[0], buffer[1]);
	T_ASSERT_EQ((uintptr_t)buffer[1], (uintptr_t)buffer[2],
		"Frames 2 and 3 are equal (recursive function)");
	if (buffer[0] > buffer[1]) {
		T_ASSERT_LE(buffer[0] - buffer[1], max_allowed_difference,
			"Top two frames are close to each other");
	} else {
		T_ASSERT_LE(buffer[1] - buffer[0], max_allowed_difference,
			"Top two frames are close to each other");
	}

	free(stack);
}

T_DECL(thread_stack_pcs_alt_stack_few_frames,
		"Check that backtrace() works with fewer than max stack frames")
{
	const size_t stack_size = 32768;
	void *stack = malloc(stack_size);
	T_ASSERT_NOTNULL(stack, "Allocated non-default stack %p", stack);

	vm_address_t buffer[16] = { 0 };
	struct many_frames_args arg = {
		.buffer = &buffer,
		.buffer_size = 16,
		.recurse_depth = 3,
		.ret_frames = 0,
		// backtrace() sets skip to 1 (to drop the backtrace() frame)
		.use_backtrace = true,
	};

	split_stack_call_impl(stack, stack_size, recursive_thread_stack_pcs, &arg);

	T_ASSERT_LT(arg.ret_frames, 16, "Walked fewer than 16 frames");
	// The top two frames should both be within recursive_thread_stack_pcs.
	// They'll be slightly different from each other, but should both be within
	// the same function. To check the latter, assume that they should be
	// within 1k of each other
	uintptr_t start =
			(uintptr_t)ptrauth_strip((void*)&recursive_thread_stack_pcs,
			ptrauth_key_function_pointer);
	const vm_address_t max_allowed_difference = 1024;
	T_ASSERT_GT((uintptr_t)buffer[0], start,
		"Top frame is in recursive_thread_stack_pcs");
	T_ASSERT_GT((uintptr_t)buffer[1], start,
		"Second frame is in recursive_thread_stack_pcs");
	T_ASSERT_NE((uintptr_t)buffer[0], (uintptr_t)buffer[1],
		"Top frame and second frame differ (0x%lx, 0x%lx)", buffer[0], buffer[1]);
	T_ASSERT_EQ((uintptr_t)buffer[1], (uintptr_t)buffer[2],
		"Frames 2 and 3 are equal (recursive function)");
	if (buffer[0] > buffer[1]) {
		T_ASSERT_LE(buffer[0] - buffer[1], max_allowed_difference,
			"Top two frames are close to each other");
	} else {
		T_ASSERT_LE(buffer[1] - buffer[0], max_allowed_difference,
			"Top two frames are close to each other");
	}

	free(stack);
}

#if defined(__arm64e__)

struct dyld_stack_context
{
    // We might go dyld->regular->dyld->...
    // These track the next stack location to push a new alternative/regular frame
    void* nextAlternativeStackAddr;
    void* nextRegularStackAddr;

    // For the test methods to record their data in
    void* thread_stack_pcs_dyld_stack_retaddr;
    void* dyld_call_alternative_stack_retaddr;
    void* dyld_call_regular_stack_retaddr;
    void* dyld_call_alternative_stack_again_retaddr;
    void* dyld_call_regular_stack_again_retaddr;
};

__attribute__((noinline))
__attribute__((naked))
__attribute__((not_tail_called))
static void callAndSwapStack(void* nextStackPtr, void** prevStackPtr, void (*callback)(struct dyld_stack_context*), struct dyld_stack_context* context)
{
    asm volatile("pacibsp\n"
                 "mov       x16, sp\n"
                 "ldr       x8,  [x1]\n"                // load the old value in prevStackPtr
                 "str       x16, [x1]\n"                // save next stack value to prevStackPtr
                 "mov       x17, x0\n"
                 "pacdb     x16, x17\n"                 // sign the old sp
                 "sub       x17, x17, #0x30\n"          // subtract space from stack
                 "stp       x1, x8,   [x17, #0x00]\n"   // save prevStackPtr and its old target value
                 "stp       xzr, x16, [x17, #0x10]\n"   // save old sp
                 "stp       x29, x30, [x17, #0x20]\n"   // save fp, lr
                 "mov       sp, x17\n"                  // switch to new stack
                 "add       x29, x17, #0x20\n"          // switch to new frame
                 "mov       x0, x3\n"                   // move context in to the first argument
                 "blraaz    x2\n"                       // call the function
                 "ldp       x1, x8,   [sp, #0x00]\n"    // load prevStackPtr and its current value when we started this function
                 "ldp       x29, x30, [sp, #0x20]\n"    // restore fp, lr
                 "ldp       xzr, x16, [sp, #0x10]\n"    // load old sp
                 "add       sp, sp, #0x30\n"            // move the stack back up before the auth
                 "autdb     x16, sp\n"                  // auth old sp
                 "mov       sp, x16\n"                  // restore old sp
                 "str       x8, [x1]\n"                 // restore the old value in prevStackPtr
                 "retab\n");
}

__attribute__((noinline)) __attribute__((not_tail_called))
static void thread_stack_pcs_dyld_stack_test(struct dyld_stack_context* context)
{
    void *ret1 = __builtin_return_address(0);

    vm_address_t* callstack[16];
    unsigned frames;
    thread_stack_pcs(callstack, 16, &frames);

    // The pcs we expect
    // 0  thread_stack_pcs
    // 1  thread_stack_pcs_dyld_stack_test
    // 2  callAndSwapStack
    // 3  dyld_call_regular_stack_again
    // 4  callAndSwapStack
    // 5  dyld_call_alternative_stack_again
    // 6  callAndSwapStack
    // 7  dyld_call_regular_stack
    // 8  callAndSwapStack
    // 9  dyld_call_alternative_stack
    // 10 testmain
    // 11 darwintest testrunner
    // ...
    T_EXPECT_GE(frames, 10, "Got the right number of stack frames");
    T_EXPECT_EQ(callstack[4], context->dyld_call_regular_stack_again_retaddr, "Found dyld_call_regular_stack_again frame");
    T_EXPECT_EQ(callstack[6], context->dyld_call_alternative_stack_again_retaddr, "Found dyld_call_alternative_stack_again frame");
    T_EXPECT_EQ(callstack[8], context->dyld_call_regular_stack_again_retaddr, "Found dyld_call_regular_stack frame");
    T_EXPECT_EQ(callstack[10], context->dyld_call_alternative_stack_retaddr, "Found dyld_call_alternative_stack frame");
}

__attribute__((noinline)) __attribute__((not_tail_called))
static void dyld_call_regular_stack_again(struct dyld_stack_context* context)
{
    context->dyld_call_regular_stack_again_retaddr = __builtin_return_address(0);
    callAndSwapStack(context->nextRegularStackAddr, &context->nextAlternativeStackAddr, &thread_stack_pcs_dyld_stack_test, context);
}

__attribute__((noinline)) __attribute__((not_tail_called))
static void dyld_call_alternative_stack_again(struct dyld_stack_context* context)
{
    context->dyld_call_alternative_stack_again_retaddr = __builtin_return_address(0);
    callAndSwapStack(context->nextAlternativeStackAddr, &context->nextRegularStackAddr, &dyld_call_regular_stack_again, context);
}

__attribute__((noinline)) __attribute__((not_tail_called))
static void dyld_call_regular_stack(struct dyld_stack_context* context)
{
    context->dyld_call_regular_stack_retaddr = __builtin_return_address(0);
    callAndSwapStack(context->nextRegularStackAddr, &context->nextAlternativeStackAddr, &dyld_call_alternative_stack_again, context);
}

__attribute__((noinline)) __attribute__((not_tail_called))
static void dyld_call_alternative_stack(struct dyld_stack_context* context)
{
    context->dyld_call_alternative_stack_retaddr = __builtin_return_address(0);
    callAndSwapStack(context->nextAlternativeStackAddr, &context->nextRegularStackAddr, &dyld_call_regular_stack, context);
}

T_DECL(thread_stack_pcs_dyld_stack, "tests thread_stack_pcs when called from the dyld stack",
       T_META_ENVVAR("DYLD_INSERT_LIBRARIES=@executable_path/thread_stack_pcs_helper"),
       T_META_ENABLED(TARGET_CPU_ARM64E && !TARGET_OS_OSX))
{
    struct dyld_stack_context context;
    const void *dyldstacktop = NULL;
    const void *dyldstackbot = NULL;

    _dyld_stack_range(&dyldstackbot, &dyldstacktop);

    context.nextAlternativeStackAddr = dyldstacktop;
    context.nextRegularStackAddr = NULL;
    context.thread_stack_pcs_dyld_stack_retaddr = __builtin_return_address(0);
    context.dyld_call_alternative_stack_retaddr = NULL;
    context.dyld_call_regular_stack_retaddr = NULL;
    context.dyld_call_alternative_stack_again_retaddr = NULL;
    context.dyld_call_regular_stack_again_retaddr = NULL;

    dyld_call_alternative_stack(&context);
}
#endif // defined(__arm64e__)
