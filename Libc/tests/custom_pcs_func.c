#include <darwintest.h>
#include <execinfo.h>
#include <libc_private.h>

#define MAGIC ((void *)0xdeadbeef)

static void custom_thread_stack_pcs(vm_address_t *buffer, unsigned max,
               unsigned *nb, __unused unsigned skip, __unused void *startfp) {
	T_EXPECT_GE(max, 1, "need to be allowed to write at least one address for this test to be sane");

	buffer[0] = (vm_address_t)MAGIC;
	*nb = 1;
}

T_DECL(custom_pcs_func, "make sure backtrace respects custom get pcs functions")
{
	backtrace_set_pcs_func(custom_thread_stack_pcs);

	void *array[2] = { NULL, NULL };
	int nframes = backtrace(array, 2);
	T_EXPECT_EQ(nframes, 1, "custom_thread_stack_pcs should only find one pc");

	T_EXPECT_EQ(array[1], NULL, "the second pc should not be written");
	T_EXPECT_EQ(array[0], MAGIC, "the first pc magic should be %p", MAGIC);
}
