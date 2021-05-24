#include <darwintest.h>
#include <darwintest_utils.h>

#include "pthread_jit_write_test_inline.h"

int
test_dylib_jit_write_callback(void *ctx);

static int
test_jit_write_callback_1(void *ctx)
{
	T_EXPECT_EQ(does_access_fault(ACCESS_WRITE, rwx_addr), false,
			"Do not expect write access to fault in write callback");

	// as a side-effect, that assertion also wrote an instruction that will next
	// let us test ACCESS_EXECUTE
	return *(int *)ctx;
}

static int
test_jit_write_callback_2(void *ctx)
{
	bool expect_fault = *(bool *)ctx;
	T_EXPECT_EQ(does_access_fault(ACCESS_EXECUTE, rwx_addr), expect_fault,
			"Expect execute access to fault in callback if supported");
	return 0;
}

static int
dylib_callback_callback(void *ctx)
{
	T_EXPECT_EQ(does_access_fault(ACCESS_WRITE, rwx_addr), false,
			"Write access should also not fault in dylib callback");
	return 4242;
}

PTHREAD_JIT_WRITE_ALLOW_CALLBACKS_NP(test_jit_write_callback_1,
		test_jit_write_callback_2);

T_DECL(pthread_jit_write_with_callback_allowed,
		"Verify pthread_jit_write_with_callback_np(3) works correctly when used correctly")
{
	bool expect_fault = pthread_jit_write_protect_supported_np();

	pthread_jit_test_setup();

	T_EXPECT_EQ(does_access_fault(ACCESS_WRITE, rwx_addr), expect_fault,
			"If supported, write should initially fault");

	int write_ctx = 42;
	int rc = pthread_jit_write_with_callback_np(test_jit_write_callback_1,
			&write_ctx);
	T_EXPECT_EQ(rc, 42, "Callback had expected return value");

	T_EXPECT_EQ(does_access_fault(ACCESS_EXECUTE, rwx_addr), false,
			"Do not expect execute access to fault after write callback");

	T_EXPECT_EQ(does_access_fault(ACCESS_WRITE, rwx_addr), expect_fault,
			"Write access should fault outside of callback if supported");

	// test that more than one callback can be allowed
	rc = pthread_jit_write_with_callback_np(test_jit_write_callback_2,
			&expect_fault);
	T_EXPECT_EQ(rc, 0, "Callback had expected return value");

	// test that callbacks in dylibs can be allowed
	rc = pthread_jit_write_with_callback_np(test_dylib_jit_write_callback,
			dylib_callback_callback);
	T_EXPECT_EQ(rc, 4242, "Callback callback returned expected result");

	pthread_jit_test_teardown();
}

#if TARGET_CPU_ARM64 // should effectively match _PTHREAD_CONFIG_JIT_WRITE_PROTECT

#define HELPER_TOOL_PATH "/AppleInternal/Tests/libpthread/assets/pthread_jit_write_with_callback_tool"

T_DECL(pthread_jit_write_protect_np_disallowed,
		"Verify pthread_jit_write_protect_np prohibited by allowlist",
		T_META_IGNORECRASHES(".*pthread_jit_write_with_callback_tool.*"))
{
	char *cmd[] = { HELPER_TOOL_PATH, "pthread_jit_write_protect_np", NULL };
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

T_DECL(pthread_jit_write_with_invalid_callback_disallowed,
		"Verify pthread_jit_write_with_callback fails bad callbacks",
		T_META_IGNORECRASHES(".*pthread_jit_write_with_callback_tool.*"))
{
	char *cmd[] = { HELPER_TOOL_PATH, "pthread_jit_write_with_callback_np", NULL };
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
