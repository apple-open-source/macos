#include <darwintest.h>
#include <darwintest_utils.h>

#include <pthread.h>
#include <dlfcn.h>

static int
dylib_callback_callback(void *ctx)
{
	return 2424;
}

#define HELPER_LIBRARY_PATH "/AppleInternal/Tests/libpthread/assets/libpthreadjittest.notdylib"

T_DECL(pthread_jit_write_freeze_callbacks_correct,
		"Verify pthread_jit_write_freeze_callbacks_np(3) works correctly when used correctly")
{
	void *handle = dlopen(HELPER_LIBRARY_PATH, RTLD_NOW);
	T_ASSERT_NOTNULL(handle, "dlopen()");

	pthread_jit_write_freeze_callbacks_np();

	pthread_jit_write_callback_t cb = dlsym(handle,
			"test_dylib_jit_write_callback");
	T_ASSERT_NOTNULL(cb, "dlsym()");

	int rc = pthread_jit_write_with_callback_np(cb, dylib_callback_callback);
	T_EXPECT_EQ(rc, 2424, "Callback callback returned expected result");
}

#if TARGET_CPU_ARM64 // should effectively match _PTHREAD_CONFIG_JIT_WRITE_PROTECT

#define HELPER_TOOL_PATH "/AppleInternal/Tests/libpthread/assets/pthread_jit_write_freeze_callbacks_tool"

T_DECL(pthread_jit_write_freeze_callbacks_incorrect,
		"Verify pthread_jit_write_protect_np prohibited by allowlist",
		T_META_IGNORECRASHES(".*pthread_jit_write_freeze_callbacks_tool.*"))
{
	char *cmd[] = { HELPER_TOOL_PATH, "pthread_jit_write_freeze_callbacks_np",
			NULL };
	dt_spawn_t spawn = dt_spawn_create(NULL);
	dt_spawn(spawn, cmd,
			^(char *line, __unused size_t size){
				T_LOG("+ %s", line);
			},
			^(char *line, __unused size_t size){
				T_LOG("stderr: %s\n", line);
			});

	bool exited, signaled;
	int status, signal;
	dt_spawn_wait(spawn, &exited, &signaled, &status, &signal);
	T_EXPECT_FALSE(exited, "helper tool should not have exited");
	T_EXPECT_TRUE(signaled, "helper tool should have been signaled");
}

#endif // TARGET_CPU_ARM64
