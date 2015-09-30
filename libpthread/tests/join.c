#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <mach/mach.h>

#define WAITTIME (100 * 1000)

static inline void*
test(void)
{
	static uintptr_t idx;
	printf("Join %lu\n", ++idx);
	return (void*)idx;
}

static void *
thread(void *param)
{
	usleep(WAITTIME);
	return param;
}

static void *
thread1(void *param)
{
	int res;
	pthread_t p = param;

	usleep(WAITTIME);
	res = pthread_join(p, NULL);
	assert(res == 0);
	printf("Done\n");
	return 0;
}

__attribute((noreturn))
int
main(void)
{
	int res;
	kern_return_t kr;
	pthread_t p = NULL;
	void *param, *value;

	param = test();
	res = pthread_create(&p, NULL, thread, param);
	assert(res == 0);
	value = NULL;
	res = pthread_join(p, &value);
	assert(res == 0);
	assert(param == value);

	param = test();
	res = pthread_create(&p, NULL, thread, param);
	assert(res == 0);
	usleep(3 * WAITTIME);
	value = NULL;
	res = pthread_join(p, &value);
	assert(res == 0);
	assert(param == value);

	param = test();
	res = pthread_create_suspended_np(&p, NULL, thread, param);
	assert(res == 0);
	kr = thread_resume(pthread_mach_thread_np(p));
	assert(kr == 0);
	value = NULL;
	res = pthread_join(p, &value);
	assert(res == 0);
	assert(param == value);

	param = test();
	res = pthread_create_suspended_np(&p, NULL, thread, param);
	assert(res == 0);
	kr = thread_resume(pthread_mach_thread_np(p));
	assert(kr == 0);
	usleep(3 * WAITTIME);
	value = NULL;
	res = pthread_join(p, &value);
	assert(res == 0);
	assert(param == value);

	test();
	param = pthread_self();
	res = pthread_create_suspended_np(&p, NULL, thread1, param);
	assert(res == 0);
	res = pthread_detach(p);
	assert(res == 0);
	kr = thread_resume(pthread_mach_thread_np(p));
	assert(kr == 0);
	pthread_exit(0);
}

