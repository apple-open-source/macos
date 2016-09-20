#include <pthread.h>
#include <pthread/private.h>
#include <dispatch/dispatch.h>

#include <darwintest.h>

extern __uint64_t __thread_selfid( void );

static void *do_test(void * __unused arg)
{
	uint64_t threadid = __thread_selfid();
	T_ASSERT_NOTNULL(threadid, NULL);

	uint64_t pth_threadid = 0;
	T_ASSERT_POSIX_ZERO(pthread_threadid_np(NULL, &pth_threadid), NULL);
	T_ASSERT_POSIX_ZERO(pthread_threadid_np(pthread_self(), &pth_threadid), NULL);
	T_EXPECT_EQ(threadid, pth_threadid, "pthread_threadid_np()");

	pth_threadid = _pthread_threadid_self_np_direct();
	T_EXPECT_EQ(threadid, pth_threadid, "pthread_threadid_np_direct()");

	return NULL;
}

T_DECL(pthread_threadid_np, "pthread_threadid_np",
	   T_META_ALL_VALID_ARCHS(YES))
{
	T_LOG("Main Thread");
	do_test(NULL);

	T_LOG("Pthread");
	pthread_t pth;
	T_ASSERT_POSIX_ZERO(pthread_create(&pth, NULL, do_test, NULL), NULL);
	T_ASSERT_POSIX_ZERO(pthread_join(pth, NULL), NULL);

	T_LOG("Workqueue Thread");
	dispatch_queue_t dq = dispatch_queue_create("myqueue", NULL);
	dispatch_async(dq, ^{ do_test(NULL); });
	dispatch_sync(dq, ^{});

	T_LOG("Workqueue Thread Reuse");
	dispatch_async(dq, ^{ do_test(NULL); });
	dispatch_sync(dq, ^{});
}
