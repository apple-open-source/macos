#include <darwintest.h>
#include <thread_stack_pcs.h>
#include <fake_swift_async.h>
#include <pthread/private.h>
#include <stdint.h>

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
        T_EXPECT_EQ(callstack[3], ptrauth_strip(&level2_func, ptrauth_key_function_pointer) + 1, "Found level2_func");
        T_EXPECT_EQ(callstack[4], ptrauth_strip(&level1_func, ptrauth_key_function_pointer) + 1, "Found level1_func");
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

