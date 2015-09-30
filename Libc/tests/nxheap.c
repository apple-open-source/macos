#include <bsdtests.h>
#include <stdlib.h>
#include <unistd.h>

char *heap;
volatile int pass;
sigjmp_buf jbuf;

void
action(int signo, struct __siginfo *info, void *uap __attribute__((unused)))
{
	if (info) {
		pass = (signo == SIGBUS && info->si_addr == heap);
	}
	return siglongjmp(jbuf, 0);
}

int
main(void)
{
	int ret;

	struct sigaction sa = {
		.__sigaction_u.__sa_sigaction = action,
		.sa_flags = SA_SIGINFO,
	};

	test_start("Non-executable heap");
	
	ret = sigaction(SIGBUS, &sa, NULL);
	assert(ret == 0);
	test_long("sigaction", ret, 0);
	
	if (sigsetjmp(jbuf, 0)) {
		// PASS
		test_long("SIGBUS", 1, 1);
		test_stop();
		return EXIT_FAILURE;
	}

	heap = malloc(1);
	test_ptr_notnull("malloc", heap);

	*heap = 0xc3; // retq
	((void (*)(void))heap)(); // call *%eax

	// FAIL
	test_long("SIGBUS", 0, 1);

	test_stop();
	
	return EXIT_SUCCESS;
}
