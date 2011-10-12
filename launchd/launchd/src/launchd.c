/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_APACHE_LICENSE_HEADER_START@
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 * @APPLE_APACHE_LICENSE_HEADER_END@
 */

static const char *const __rcs_file_version__ = "$Revision: 24863 $";

#include "config.h"
#include "launchd.h"

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/event.h>
#include <sys/stat.h>
#include <sys/ucred.h>
#include <sys/fcntl.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/sysctl.h>
#include <sys/sockio.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/kern_event.h>
#include <sys/reboot.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet6/nd6.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <paths.h>
#include <pwd.h>
#include <grp.h>
#include <ttyent.h>
#include <dlfcn.h>
#include <dirent.h>
#include <string.h>
#include <setjmp.h>
#include <spawn.h>
#include <sched.h>
#include <pthread.h>
#include <util.h>

#if HAVE_LIBAUDITD
#include <bsm/auditd_lib.h>
#include <bsm/audit_session.h>
#endif

#include "bootstrap.h"
#include "vproc.h"
#include "vproc_priv.h"
#include "vproc_internal.h"
#include "launch.h"
#include "launch_internal.h"

#include "launchd_runtime.h"
#include "launchd_core_logic.h"
#include "launchd_unix_ipc.h"

#define LAUNCHD_CONF ".launchd.conf"

extern char **environ;

static void pfsystem_callback(void *, struct kevent *);

static kq_callback kqpfsystem_callback = pfsystem_callback;

static void pid1_magic_init(void);

static void testfd_or_openfd(int fd, const char *path, int flags);
static bool get_network_state(void);
static void monitor_networking_state(void);
static void fatal_signal_handler(int sig, siginfo_t *si, void *uap);
static void handle_pid1_crashes_separately(void);
static void do_pid1_crash_diagnosis_mode(const char *msg);
static int basic_fork(void);
static bool do_pid1_crash_diagnosis_mode2(const char *msg);

static void *update_thread(void *nothing);

static bool re_exec_in_single_user_mode;
static void *crash_addr;
static pid_t crash_pid;

bool shutdown_in_progress;
bool fake_shutdown_in_progress;
bool network_up;
char g_username[128] = "__Uninitialized__";
char g_my_label[128] = "__Uninitialized__";
char g_launchd_database_dir[PATH_MAX];
FILE *g_console = NULL;
int32_t g_sync_frequency = 30;

int
main(int argc, char *const *argv)
{
	bool sflag = false;
	int ch;

	testfd_or_openfd(STDIN_FILENO, _PATH_DEVNULL, O_RDONLY);
	testfd_or_openfd(STDOUT_FILENO, _PATH_DEVNULL, O_WRONLY);
	testfd_or_openfd(STDERR_FILENO, _PATH_DEVNULL, O_WRONLY);

	if (g_use_gmalloc) {
		if (!getenv("DYLD_INSERT_LIBRARIES")) {
			setenv("DYLD_INSERT_LIBRARIES", "/usr/lib/libgmalloc.dylib", 1);
			setenv("MALLOC_STRICT_SIZE", "1", 1);
			execv(argv[0], argv);
		} else {
			unsetenv("DYLD_INSERT_LIBRARIES");
			unsetenv("MALLOC_STRICT_SIZE");
		}
	} else if (g_malloc_log_stacks) {
		if (!getenv("MallocStackLogging")) {
			setenv("MallocStackLogging", "1", 1);
			execv(argv[0], argv);
		} else {
			unsetenv("MallocStackLogging");
		}
	}

	while ((ch = getopt(argc, argv, "s")) != -1) {
		switch (ch) {
		case 's': sflag = true; break;	/* single user */
		case '?': /* we should do something with the global optopt variable here */
		default:
			fprintf(stderr, "%s: ignoring unknown arguments\n", getprogname());
			break;
		}
	}

	if (getpid() != 1 && getppid() != 1) {
		fprintf(stderr, "%s: This program is not meant to be run directly.\n", getprogname());
		exit(EXIT_FAILURE);
	}

	launchd_runtime_init();

	if (pid1_magic) {
		int cfd = -1;
		if (launchd_assumes((cfd = open(_PATH_CONSOLE, O_WRONLY | O_NOCTTY)) != -1)) {
			_fd(cfd);
			if (!launchd_assumes((g_console = fdopen(cfd, "w")) != NULL)) {
				close(cfd);
			}
		}
	}

	if (NULL == getenv("PATH")) {
		setenv("PATH", _PATH_STDPATH, 1);
	}

	if (pid1_magic) {
		pid1_magic_init();
	} else {
		ipc_server_init();
		
		runtime_log_push();
		
		struct passwd *pwent = getpwuid(getuid());
		if (pwent) {
			strlcpy(g_username, pwent->pw_name, sizeof(g_username) - 1);
		}

		snprintf(g_my_label, sizeof(g_my_label), "com.apple.launchd.peruser.%u", getuid());
		
		auditinfo_addr_t auinfo;
		if (launchd_assumes(getaudit_addr(&auinfo, sizeof(auinfo)) != -1)) {
			g_audit_session = auinfo.ai_asid;
			runtime_syslog(LOG_DEBUG, "Our audit session ID is %i", g_audit_session);
		}
		
		g_audit_session_port = _audit_session_self();
		snprintf(g_launchd_database_dir, sizeof(g_launchd_database_dir), LAUNCHD_DB_PREFIX "/com.apple.launchd.peruser.%u", getuid());
		runtime_syslog(LOG_DEBUG, "Per-user launchd for UID %u (%s) has begun.", getuid(), g_username);
	}

	if (pid1_magic) {
		runtime_syslog(LOG_NOTICE | LOG_CONSOLE, "*** launchd[1] has started up. ***");
		if (g_use_gmalloc) {
			runtime_syslog(LOG_NOTICE | LOG_CONSOLE, "*** Using libgmalloc. ***");
		}
		if (g_malloc_log_stacks) {
			runtime_syslog(LOG_NOTICE | LOG_CONSOLE, "*** Logging stacks of malloc(3) allocations. ***");
		}

		if (g_verbose_boot) {
			runtime_syslog(LOG_NOTICE | LOG_CONSOLE, "*** Verbose boot, will log to /dev/console. ***");
		}

		if (g_shutdown_debugging) {
			runtime_syslog(LOG_NOTICE | LOG_CONSOLE, "*** Shutdown debugging is enabled. ***");
		}

		/* PID 1 doesn't have a flat namespace. */
		g_flat_mach_namespace = false;
	} else {
		if (g_use_gmalloc) {
			runtime_syslog(LOG_NOTICE, "*** Per-user launchd using libgmalloc. ***");
		}
	}

	monitor_networking_state();

	if (pid1_magic) {
		handle_pid1_crashes_separately();
	} else {
	#if !TARGET_OS_EMBEDDED
		/* prime shared memory before the 'bootstrap_port' global is set to zero */
		_vproc_transaction_begin();
		_vproc_transaction_end();
	#endif
	}

	if (pid1_magic) {
		/* Start the update thread -- rdar://problem/5039559&6153301 */
		pthread_t t = NULL;
		int err = pthread_create(&t, NULL, update_thread, NULL);
		(void)launchd_assumes(err == 0);
		(void)launchd_assumes(pthread_detach(t) == 0);
	}

	jobmgr_init(sflag);
	
	launchd_runtime_init2();

	launchd_runtime();
}

void
handle_pid1_crashes_separately(void)
{
	struct sigaction fsa;

	fsa.sa_sigaction = fatal_signal_handler;
	fsa.sa_flags = SA_SIGINFO;
	sigemptyset(&fsa.sa_mask);

	(void)launchd_assumes(sigaction(SIGILL, &fsa, NULL) != -1);
	(void)launchd_assumes(sigaction(SIGFPE, &fsa, NULL) != -1);
	(void)launchd_assumes(sigaction(SIGBUS, &fsa, NULL) != -1);
	(void)launchd_assumes(sigaction(SIGSEGV, &fsa, NULL) != -1);
	(void)launchd_assumes(sigaction(SIGABRT, &fsa, NULL) != -1);
	(void)launchd_assumes(sigaction(SIGTRAP, &fsa, NULL) != -1);
}

void *update_thread(void *nothing __attribute__((unused)))
{
	/* <rdar://problem/7385963> use IOPOL_PASSIVE for sync thread */
	(void)launchd_assumes(setiopolicy_np(IOPOL_TYPE_DISK, IOPOL_SCOPE_THREAD, IOPOL_PASSIVE) != -1);
	
	while( g_sync_frequency ) {
		sync();
		sleep(g_sync_frequency);
	}
	
	runtime_syslog(LOG_DEBUG, "Update thread exiting.");
	return NULL;
}

#define PID1_CRASH_LOGFILE "/var/log/launchd-pid1.crash"

/* This hack forces the dynamic linker to resolve these symbols ASAP */
static __attribute__((unused)) typeof(sync) *__junk_dyld_trick1 = sync;
static __attribute__((unused)) typeof(sleep) *__junk_dyld_trick2 = sleep;
static __attribute__((unused)) typeof(reboot) *__junk_dyld_trick3 = reboot;

void
do_pid1_crash_diagnosis_mode(const char *msg)
{
	if (g_wsp) {
		kill(g_wsp, SIGKILL);
		sleep(3);
		g_wsp = 0;
	}

	while (g_shutdown_debugging && !do_pid1_crash_diagnosis_mode2(msg)) {
		sleep(1);
	}
}

int
basic_fork(void)
{
	int wstatus = 0;
	pid_t p;
	
	switch ((p = fork())) {
	case -1:
		runtime_syslog(LOG_ERR | LOG_CONSOLE, "Can't fork PID 1 copy for crash debugging: %m");
		return p;
	case 0:
		return p;
	default:
		do {
			(void)waitpid(p, &wstatus, 0);
		} while(!WIFEXITED(wstatus));

		fprintf(stdout, "PID 1 copy: exit status: %d\n", WEXITSTATUS(wstatus));

		return 1;
	}
	
	return -1;
}

bool
do_pid1_crash_diagnosis_mode2(const char *msg)
{
	if (basic_fork() == 0) {
		/* Neuter our bootstrap port so that the shell doesn't try talking to us while
		 * we're blocked waiting on it.
		 */
		if (g_console) {
			fflush(g_console);
		}
		task_set_bootstrap_port(mach_task_self(), MACH_PORT_NULL);
		if (basic_fork() != 0) {
			if (g_console) {
				fflush(g_console);
			}
			return true;
		}
	} else {
		return true;
	}
	
	int fd;
	revoke(_PATH_CONSOLE);
	if ((fd = open(_PATH_CONSOLE, O_RDWR)) == -1) {
		_exit(2);
	}
	if (login_tty(fd) == -1) {
		_exit(3);
	}
	setenv("TERM", "vt100", 1);
	fprintf(stdout, "\n");
	fprintf(stdout, "Entering launchd PID 1 debugging mode...\n");
	fprintf(stdout, "The PID 1 launchd has crashed %s.\n", msg);
	fprintf(stdout, "It has fork(2)ed itself for debugging.\n");
	fprintf(stdout, "To debug the crashing thread of PID 1:\n");
	fprintf(stdout, "    gdb attach %d\n", getppid());
	fprintf(stdout, "To exit this shell and shut down:\n");
	fprintf(stdout, "    kill -9 1\n");
	fprintf(stdout, "A sample of PID 1 has been written to %s\n", PID1_CRASH_LOGFILE);
	fprintf(stdout, "\n");
	fflush(stdout);
	
	execl(_PATH_BSHELL, "-sh", NULL);
	syslog(LOG_ERR, "can't exec %s for PID 1 crash debugging: %m", _PATH_BSHELL);
	_exit(EXIT_FAILURE);
}

void
fatal_signal_handler(int sig, siginfo_t *si, void *uap __attribute__((unused)))
{
	const char *doom_why = "at instruction";
	char msg[128];
	char *sample_args[] = { "/usr/bin/sample", "1", "1", "-file", PID1_CRASH_LOGFILE, NULL };
	pid_t sample_p;
	int wstatus;

	crash_addr = si->si_addr;
	crash_pid = si->si_pid;
	
	unlink(PID1_CRASH_LOGFILE);

	switch ((sample_p = vfork())) {
	case 0:
		execve(sample_args[0], sample_args, environ);
		_exit(EXIT_FAILURE);
		break;
	default:
		waitpid(sample_p, &wstatus, 0);
		break;
	case -1:
		break;
	}

	switch (sig) {
	default:
	case 0:
		break;
	case SIGBUS:
	case SIGSEGV:
		doom_why = "trying to read/write";
	case SIGILL:
	case SIGFPE:
		snprintf(msg, sizeof(msg), "%s: %p (%s sent by PID %u)", doom_why, crash_addr, strsignal(sig), crash_pid);
		sync();
		do_pid1_crash_diagnosis_mode(msg);
		sleep(3);
		reboot(0);
		break;
	}
}

void
pid1_magic_init(void)
{
	(void)launchd_assumes(setsid() != -1);
	(void)launchd_assumes(chdir("/") != -1);
	(void)launchd_assumes(setlogin("root") != -1);
	
	strcpy(g_my_label, "com.apple.launchd");

#if !TARGET_OS_EMBEDDED
	auditinfo_addr_t auinfo = {
		.ai_termid = { .at_type = AU_IPv4 },
		.ai_asid = AU_ASSIGN_ASID,
		.ai_auid = AU_DEFAUDITID,
		.ai_flags = AU_SESSION_FLAG_IS_INITIAL,
	};
	
	if (!launchd_assumes(setaudit_addr(&auinfo, sizeof(auinfo)) != -1)) {
		runtime_syslog(LOG_WARNING | LOG_CONSOLE, "Could not set audit session: %s.", strerror(errno));
		_exit(EXIT_FAILURE);
	}

	g_audit_session = auinfo.ai_asid;
	runtime_syslog(LOG_DEBUG, "Audit Session ID: %i", g_audit_session);

	g_audit_session_port = _audit_session_self();
#endif // !TARGET_OS_EMBEDDED
	
	strcpy(g_launchd_database_dir, LAUNCHD_DB_PREFIX "/com.apple.launchd");
}

char *
launchd_data_base_path(int db_type)
{
	static char result[PATH_MAX];
	static int last_db_type = -1;
	
	if (db_type == last_db_type) {
		return result;
	}
	
	switch (db_type) {
		case LAUNCHD_DB_TYPE_OVERRIDES	:
			snprintf(result, sizeof(result), "%s/%s", g_launchd_database_dir, "overrides.plist");
			last_db_type = db_type;
			break;
		case LAUNCHD_DB_TYPE_JOBCACHE	:
			snprintf(result, sizeof(result), "%s/%s", g_launchd_database_dir, "jobcache.launchdata");
			last_db_type = db_type;
			break;
		default							:
			break;
	}
	
	return result;
}

int
_fd(int fd)
{
	if (fd >= 0) {
		(void)launchd_assumes(fcntl(fd, F_SETFD, 1) != -1);
	}
	return fd;
}

void
launchd_shutdown(void)
{
	int64_t now;

	if (shutdown_in_progress) {
		return;
	}

	runtime_ktrace0(RTKT_LAUNCHD_EXITING);

	shutdown_in_progress = true;

	if (pid1_magic || g_log_per_user_shutdown) {
		/*
		 * When this changes to a more sustainable API, update this:
		 * http://howto.apple.com/db.cgi?Debugging_Apps_Non-Responsive_At_Shutdown
		 */
		runtime_setlogmask(LOG_UPTO(LOG_DEBUG));
	}

	runtime_log_push();

	now = runtime_get_wall_time();

	char *term_who = pid1_magic ? "System shutdown" : "Per-user launchd termination for ";
	runtime_syslog(LOG_INFO, "%s%s began", term_who, pid1_magic ? "" : g_username);

	launchd_assert(jobmgr_shutdown(root_jobmgr) != NULL);

#if HAVE_LIBAUDITD
	if (pid1_magic) {
		(void)launchd_assumes(audit_quick_stop() == 0);
	}
#endif
}

void
launchd_single_user(void)
{
	runtime_syslog(LOG_NOTICE, "Going to single-user mode");

	re_exec_in_single_user_mode = true;

	launchd_shutdown();

	sleep(3);

	runtime_kill(-1, SIGKILL);
}

void
launchd_SessionCreate(void)
{
#if !TARGET_OS_EMBEDDED
	auditinfo_addr_t auinfo = {
		.ai_termid = { .at_type = AU_IPv4 },
		.ai_asid = AU_ASSIGN_ASID,
		.ai_auid = getuid(),
		.ai_flags = 0,
	};
	if (launchd_assumes(setaudit_addr(&auinfo, sizeof(auinfo)) == 0)) {
		char session[16];
		snprintf(session, sizeof(session), "%x", auinfo.ai_asid);
		setenv("SECURITYSESSIONID", session, 1);
	} else {
		runtime_syslog(LOG_WARNING, "Could not set audit session: %s.", strerror(errno));
	}
#endif // !TARGET_OS_EMBEDDED
}

void
testfd_or_openfd(int fd, const char *path, int flags)
{
	int tmpfd;

	if (-1 != (tmpfd = dup(fd))) {
		(void)launchd_assumes(runtime_close(tmpfd) == 0);
	} else {
		if (-1 == (tmpfd = open(path, flags | O_NOCTTY, DEFFILEMODE))) {
			runtime_syslog(LOG_ERR, "open(\"%s\", ...): %m", path);
		} else if (tmpfd != fd) {
			(void)launchd_assumes(dup2(tmpfd, fd) != -1);
			(void)launchd_assumes(runtime_close(tmpfd) == 0);
		}
	}
}

bool
get_network_state(void)
{
	struct ifaddrs *ifa, *ifai;
	bool up = false;
	int r;

	/* Workaround 4978696: getifaddrs() reports false ENOMEM */
	while ((r = getifaddrs(&ifa)) == -1 && errno == ENOMEM) {
		runtime_syslog(LOG_DEBUG, "Worked around bug: 4978696");
		(void)launchd_assumes(sched_yield() != -1);
	}

	if (!launchd_assumes(r != -1)) {
		return network_up;
	}

	for (ifai = ifa; ifai; ifai = ifai->ifa_next) {
		if (!(ifai->ifa_flags & IFF_UP)) {
			continue;
		}
		if (ifai->ifa_flags & IFF_LOOPBACK) {
			continue;
		}
		if (ifai->ifa_addr->sa_family != AF_INET && ifai->ifa_addr->sa_family != AF_INET6) {
			continue;
		}
		up = true;
		break;
	}

	freeifaddrs(ifa);

	return up;
}

void
monitor_networking_state(void)
{
	int pfs = _fd(socket(PF_SYSTEM, SOCK_RAW, SYSPROTO_EVENT));
	struct kev_request kev_req;

	network_up = get_network_state();

	if (!launchd_assumes(pfs != -1)) {
		return;
	}

	memset(&kev_req, 0, sizeof(kev_req));
	kev_req.vendor_code = KEV_VENDOR_APPLE;
	kev_req.kev_class = KEV_NETWORK_CLASS;

	if (!launchd_assumes(ioctl(pfs, SIOCSKEVFILT, &kev_req) != -1)) {
		runtime_close(pfs);
		return;
	}

	(void)launchd_assumes(kevent_mod(pfs, EVFILT_READ, EV_ADD, 0, 0, &kqpfsystem_callback) != -1);
}

void
pfsystem_callback(void *obj __attribute__((unused)), struct kevent *kev)
{
	bool new_networking_state;
	char buf[1024];

	(void)launchd_assumes(read((int)kev->ident, &buf, sizeof(buf)) != -1);

	new_networking_state = get_network_state();

	if (new_networking_state != network_up) {
		network_up = new_networking_state;
		jobmgr_dispatch_all_semaphores(root_jobmgr);
	}
}

void
_log_launchd_bug(const char *rcs_rev, const char *path, unsigned int line, const char *test)
{
	int saved_errno = errno;
	char buf[100];
	const char *file = strrchr(path, '/');
	char *rcs_rev_tmp = strchr(rcs_rev, ' ');

	runtime_ktrace1(RTKT_LAUNCHD_BUG);

	if (!file) {
		file = path;
	} else {
		file += 1;
	}

	if (!rcs_rev_tmp) {
		strlcpy(buf, rcs_rev, sizeof(buf));
	} else {
		strlcpy(buf, rcs_rev_tmp + 1, sizeof(buf));
		rcs_rev_tmp = strchr(buf, ' ');
		if (rcs_rev_tmp) {
			*rcs_rev_tmp = '\0';
		}
	}

	runtime_syslog(LOG_NOTICE, "Bug: %s:%u (%s):%u: %s", file, line, buf, saved_errno, test);
}
