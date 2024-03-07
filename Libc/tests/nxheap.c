#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/code_signing.h>

#include <darwintest.h>

static char *heap;
static volatile int pass;
static sigjmp_buf jbuf;

static bool
code_signing_monitor_enabled(void)
{
	code_signing_config_t cs_config = 0;
	size_t cs_config_size = sizeof(cs_config);

	// Query the code signing configuration information
	(void)sysctlbyname("security.codesigning.config",
			&cs_config, &cs_config_size, NULL, 0);

	if (cs_config & (code_signing_config_t)CS_CONFIG_CSM_ENABLED) {
		return true;
	}
	return false;
}

static void __dead2
action(int signo, struct __siginfo *info, void *uap __attribute__((unused)))
{
	if (info) {
		pass = (signo == SIGBUS && info->si_addr == heap);
	}
	siglongjmp(jbuf, 0);
}

T_DECL(nxheap, "Non-executable heap", T_META_CHECK_LEAKS(false), T_META_ASROOT(true))
{
	struct sigaction sa = {
		.__sigaction_u.__sa_sigaction = action,
		.sa_flags = SA_SIGINFO,
	};

	// When the code-signing-monitor is enabled, any kind of executable
	// fault results in a direct SIGKILL. Given that, attempting to run
	// this test will cause a failure when the monitor is enabled.
	if (code_signing_monitor_enabled() == true) {
		T_SKIP("skipping test as code-signing-monitor is enabled");
	}

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
