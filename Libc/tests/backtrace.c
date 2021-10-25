#include <darwintest.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <fake_swift_async.h>
#include <mach-o/dyld_priv.h>
#include <pthread/private.h>
#include <stdbool.h>
#include <stdlib.h>
#include <uuid/uuid.h>


#define MAX_FRAMES 32
static const int expected_nframes = 20;
static const int skip_nframes = 5;

static void *observed_bt[MAX_FRAMES] = {};
static int observed_nframes = 0;
static unsigned int save_fp_at_nframes = 0;
static void *save_fp = NULL;

static int __attribute__((noinline,not_tail_called,disable_tail_calls))
recurse_a(unsigned int frames);
static int __attribute__((noinline,not_tail_called,disable_tail_calls))
recurse_b(unsigned int frames);

static int __attribute__((noinline,not_tail_called,disable_tail_calls))
recurse_a(unsigned int frames)
{
	if (frames == 0) {
		if (save_fp_at_nframes > 0) {
			observed_nframes = backtrace_from_fp(save_fp, observed_bt,
					MAX_FRAMES);
		} else {
			observed_nframes = backtrace(observed_bt, MAX_FRAMES);
		}
		return 0;
	} else if (frames == save_fp_at_nframes) {
		save_fp = __builtin_frame_address(0);
	}

	return recurse_b(frames - 1);
}

static int __attribute__((noinline,not_tail_called,disable_tail_calls))
recurse_b(unsigned int frames)
{
	if (frames == 0) {
		if (save_fp_at_nframes > 0) {
			observed_nframes = backtrace_from_fp(save_fp, observed_bt,
					MAX_FRAMES);
		} else {
			observed_nframes = backtrace(observed_bt, MAX_FRAMES);
		}
		return 0;
	} else if (frames == save_fp_at_nframes) {
		save_fp = __builtin_frame_address(0);
	}

	return recurse_a(frames - 1);
}

static void __attribute__((noinline,not_tail_called,disable_tail_calls))
setup_and_backtrace(unsigned int nframes, unsigned int skip)
{
	save_fp_at_nframes = skip ? skip - 1 : 0;
	recurse_a(nframes - 1);
}

static bool
check_for_setup(int i, struct dl_info *info)
{
	int ret = dladdr(observed_bt[i], info);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "dladdr(%p)", observed_bt[i]);
	void *setup_fp = (void *)&setup_and_backtrace;
	return info->dli_saddr == setup_fp;
}

static void __attribute__((noinline,not_tail_called))
expect_backtrace(void)
{
	void *recurse_a_fp = (void *)&recurse_a;
	void *recurse_b_fp = (void *)&recurse_b;

	void *tmp_backtrace[MAX_FRAMES];
	const int observed_existing_nframes = backtrace(tmp_backtrace, MAX_FRAMES);

	T_EXPECT_EQ(expected_nframes,
			observed_nframes - observed_existing_nframes,
			"number of frames traced matches %d", expected_nframes);
	bool expect_a = !(skip_nframes % 2);
	bool found_setup = false;

	for (int i = 0; i < observed_nframes; i++) {
		struct dl_info info;
		if (check_for_setup(i, &info)) {
			found_setup = true;
			break;
		}

		void *expected_saddr = expect_a ? recurse_a_fp : recurse_b_fp;
		void *observed_saddr = info.dli_saddr;
		T_EXPECT_EQ(observed_saddr, expected_saddr,
                "frame %d (%p: %s) matches", i, observed_bt[i], info.dli_sname);
		expect_a = !expect_a;
	}

	T_EXPECT_TRUE(found_setup, "should have found the setup frame");
}

T_DECL(backtrace, "ensure backtrace(3) gives the correct backtrace")
{
	setup_and_backtrace(expected_nframes, 0);
	expect_backtrace();
}

T_DECL(backtrace_from_fp,
		"ensure backtrace_from_fp(3) starts from the correct frame")
{
	setup_and_backtrace(expected_nframes + skip_nframes, skip_nframes);
	expect_backtrace();
}

T_DECL(backtrace_image_offsets,
		"ensure backtrace_image_offsets(3) provides valid UUIDs and offsets")
{
	setup_and_backtrace(expected_nframes, 0);
	struct image_offset imgoffs[observed_nframes];
	backtrace_image_offsets(observed_bt, imgoffs, observed_nframes);

	bool found_setup = false;

	for (int i = 0; i < observed_nframes; i++) {
		struct dl_info info;
		if (check_for_setup(i, &info)) {
			found_setup = true;
			break;
		}

		const struct mach_header *mh =
				dyld_image_header_containing_address(observed_bt[i]);

		uuid_t expected_uuid;
		bool got_uuid = _dyld_get_image_uuid(mh, expected_uuid);
		T_QUIET; T_ASSERT_TRUE(got_uuid, "got UUID for Mach-O header");

		T_EXPECT_EQ(uuid_compare(expected_uuid, imgoffs[i].uuid), 0,
				"frame %d's UUID matches", i);
		T_EXPECT_EQ((uintptr_t)observed_bt[i] - (uintptr_t)info.dli_fbase,
				(uintptr_t)imgoffs[i].offset, "frame %d's offset matches", i);
	}

	T_EXPECT_TRUE(found_setup, "should have found the setup frame");
}

T_DECL(backtrace_symbols, "tests backtrace_symbols")
{
	setup_and_backtrace(expected_nframes, 0);

	char **symbols = backtrace_symbols(observed_bt, observed_nframes);

	bool found_setup = false;

	for (int i = 0; i < observed_nframes; i++) {
		T_LOG("frame[%d]: %s", i, symbols[i]);
		if (strstr(symbols[i], "setup_and_backtrace") != NULL) {
			found_setup = true;
		}
	}

	T_EXPECT_TRUE(found_setup, "should have found the setup frame");

	free(symbols);
}

__attribute__((noinline))
static void test_async(void *unused __attribute__((unused)), bool async) {
    void* callstack[16];
    unsigned task_id;
    unsigned frames = backtrace_async(callstack, 16, &task_id);

    if (async) {
        T_EXPECT_EQ(task_id, FAKE_TASK_ID, "backtrace_async returns the right task id.");

        // The 4 frames we expect are
        // 0 test
        // 1 fake_async_frame
        // 2 level2_func
        // 3 level1_func
        T_EXPECT_EQ(frames, 4, "Got the right number of async frames");
        T_EXPECT_EQ(callstack[1], __builtin_return_address(0), "Found fake_async_frame");
        T_EXPECT_EQ(callstack[2], ptrauth_strip(&level2_func, ptrauth_key_function_pointer) + 1, "Found level2_func");
        T_EXPECT_EQ(callstack[3], ptrauth_strip(&level1_func, ptrauth_key_function_pointer) + 1, "Found level1_func");
  } else {
        T_EXPECT_EQ(task_id, 0, "backtrace_async returns no task id.");
        // The 3 frames we expect are
        // 0 test
        // 1 fake_async_frame
        // 2 <Test function>
        // ... Potential test runner frames
        T_EXPECT_GE(frames, 3, "Got the right number of stack frames");
        T_EXPECT_EQ(callstack[1], __builtin_return_address(0), "Found fake_async_frame");
        T_EXPECT_EQ(callstack[2], __builtin_return_address(1), "Found test runner");
  }
}


__attribute__((noinline))
static void fake_async_frame() {
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
    test_async(&ctx, true);
    *fp ^= (0x1ll << 60);
#endif
    test_async(&ctx, false);
}


T_DECL(backtrace_async, "ensure backtrace_async(3) gives the correct backtrace")
{
    fake_async_frame();
}
