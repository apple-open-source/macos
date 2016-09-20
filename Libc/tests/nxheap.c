#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>

#include <darwintest.h>

static char *heap;
static volatile int pass;
static sigjmp_buf jbuf;

static void __dead2
action(int signo, struct __siginfo *info, void *uap __attribute__((unused)))
{
	if (info) {
		pass = (signo == SIGBUS && info->si_addr == heap);
	}
	siglongjmp(jbuf, 0);
}

T_DECL(nxheap, "Non-executable heap", T_META_CHECK_LEAKS(NO))
{
	struct sigaction sa = {
		.__sigaction_u.__sa_sigaction = action,
		.sa_flags = SA_SIGINFO,
	};

	T_ASSERT_POSIX_ZERO(sigaction(SIGBUS, &sa, NULL), NULL);
	
	if (sigsetjmp(jbuf, 0)) {
		T_PASS("SIGBUS");
		T_END;
	}

	T_QUIET; T_ASSERT_NOTNULL((heap = malloc(1)), NULL);

	*heap = (char)0xc3; // retq
	((void (*)(void))heap)(); // call *%eax

	T_FAIL("SIGBUS");
}
