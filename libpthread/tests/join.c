#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <mach/mach.h>

#include <darwintest.h>

#define WAITTIME (100 * 1000)

static inline void*
test(void)
{
	static uintptr_t idx;
	return (void*)idx;
}

static void *
thread(void *param)
{
	usleep(WAITTIME);
	return param;
}

/*
static void *
thread1(void *param)
{
	int res;
	pthread_t p = param;

	usleep(WAITTIME);
	res = pthread_join(p, NULL);
	assert(res == 0);
	return 0;
}
*/

T_DECL(join, "pthread_join",
		T_META_ALL_VALID_ARCHS(YES))
{
	int res;
	kern_return_t kr;
	pthread_t p = NULL;
	void *param, *value;

	param = test();
	res = pthread_create(&p, NULL, thread, param);
	T_ASSERT_POSIX_ZERO(res, "pthread_create");
	value = NULL;
	res = pthread_join(p, &value);
	T_ASSERT_POSIX_ZERO(res, "pthread_join");
	T_ASSERT_EQ_PTR(param, value, "early join value");

	param = test();
	res = pthread_create(&p, NULL, thread, param);
	T_ASSERT_POSIX_ZERO(res, "pthread_create");
	usleep(3 * WAITTIME);
	value = NULL;
	res = pthread_join(p, &value);
	T_ASSERT_POSIX_ZERO(res, "pthread_join");
	T_ASSERT_EQ_PTR(param, value, "late join value");

	param = test();
	res = pthread_create_suspended_np(&p, NULL, thread, param);
	T_ASSERT_POSIX_ZERO(res, "pthread_create_suspended_np");
	kr = thread_resume(pthread_mach_thread_np(p));
	T_ASSERT_EQ_INT(kr, 0, "thread_resume");
	value = NULL;
	res = pthread_join(p, &value);
	T_ASSERT_POSIX_ZERO(res, "pthread_join");
	T_ASSERT_EQ_PTR(param, value, "suspended early join value");

	param = test();
	res = pthread_create_suspended_np(&p, NULL, thread, param);
	T_ASSERT_POSIX_ZERO(res, "pthread_create_suspended_np");
	kr = thread_resume(pthread_mach_thread_np(p));
	T_ASSERT_EQ_INT(kr, 0, "thread_resume");
	usleep(3 * WAITTIME);
	value = NULL;
	res = pthread_join(p, &value);
	T_ASSERT_POSIX_ZERO(res, "pthread_join");
	T_ASSERT_EQ_PTR(param, value, "suspended late join value");

	// This test is supposed to test joining on the main thread.  It's not
	// clear how to express this with libdarwintest for now.
	/*
	test();
	param = pthread_self();
	res = pthread_create_suspended_np(&p, NULL, thread1, param);
	T_ASSERT_POSIX_ZERO(res, "pthread_create_suspended_np");
	res = pthread_detach(p);
	T_ASSERT_POSIX_ZERO(res, "pthread_detach");
	kr = thread_resume(pthread_mach_thread_np(p));
	T_ASSERT_EQ_INT(kr, 0, "thread_resume");
	pthread_exit(0);
	*/
}

static void *
thread_stub(__unused void *arg)
{
	return NULL;
}

T_DECL(pthread_join_stress, "pthread_join in a loop")
{
	for (int i = 0; i < 1000; i++) {
		pthread_t th[16];
		for (int j = 0; j < i%16; j++){
			T_QUIET; T_ASSERT_POSIX_SUCCESS(pthread_create(&th[j], NULL, thread_stub, NULL), NULL);
		}
		for (int j = i%16; j >= 0; j--){
			T_QUIET; T_ASSERT_POSIX_SUCCESS(pthread_join(th[j], NULL), NULL);
		}
	}
	T_PASS("Success!");
}
