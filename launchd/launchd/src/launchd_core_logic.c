/*
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

static const char *const __rcs_file_version__ = "$Revision: 23932 $";

#include "config.h"
#include "launchd_core_logic.h"

#include <TargetConditionals.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/boolean.h>
#include <mach/message.h>
#include <mach/notify.h>
#include <mach/mig_errors.h>
#include <mach/mach_traps.h>
#include <mach/mach_interface.h>
#include <mach/host_info.h>
#include <mach/mach_host.h>
#include <mach/exception.h>
#include <mach/host_reboot.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/event.h>
#include <sys/stat.h>
#include <sys/ucred.h>
#include <sys/fcntl.h>
#include <sys/un.h>
#include <sys/reboot.h>
#include <sys/wait.h>
#include <sys/sysctl.h>
#include <sys/sockio.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/pipe.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet6/nd6.h>
#include <bsm/libbsm.h>
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
#include <ctype.h>
#include <glob.h>
#include <spawn.h>
#include <libproc.h>
#include <malloc/malloc.h>
#include <pthread.h>
#if HAVE_SANDBOX
#define __APPLE_API_PRIVATE
#include <sandbox.h>
#endif
#if HAVE_QUARANTINE
#include <quarantine.h>
#endif
#if TARGET_OS_EMBEDDED
#include <sys/kern_memorystatus.h>
#else
/* To make my life easier. */
typedef struct jetsam_priority_entry {
    pid_t pid;
    uint32_t flags;
} jetsam_priority_entry_t;

enum {
    kJetsamFlagsFrontmost = (1 << 0),
    kJetsamFlagsKilled =    (1 << 1)
};
#endif

#include "launch.h"
#include "launch_priv.h"
#include "launch_internal.h"
#include "bootstrap.h"
#include "bootstrap_priv.h"
#include "vproc.h"
#include "vproc_internal.h"

#include "reboot2.h"

#include "launchd.h"
#include "launchd_runtime.h"
#include "launchd_unix_ipc.h"
#include "protocol_vproc.h"
#include "protocol_vprocServer.h"
#include "protocol_job_reply.h"
#include "protocol_job_forward.h"
#include "mach_excServer.h"

/*
 * LAUNCHD_SAMPLE_TIMEOUT
 *   If the job hasn't exited in the given number of seconds after sending
 *   it a SIGTERM, start sampling it.
 * LAUNCHD_DEFAULT_EXIT_TIMEOUT
 *   If the job hasn't exited in the given number of seconds after sending
 *   it a SIGTERM, SIGKILL it. Can be overriden in the job plist.
 */
#define LAUNCHD_MIN_JOB_RUN_TIME		10
#define LAUNCHD_SAMPLE_TIMEOUT			2
#define LAUNCHD_DEFAULT_EXIT_TIMEOUT	20
#define LAUNCHD_SIGKILL_TIMER			5
#define LAUNCHD_CLEAN_KILL_TIMER		1
#define LAUNCHD_JETSAM_PRIORITY_UNSET	0xdead1eebabell

#define SHUTDOWN_LOG_DIR "/var/log/shutdown"

#define TAKE_SUBSET_NAME	"TakeSubsetName"
#define TAKE_SUBSET_PID		"TakeSubsetPID"
#define TAKE_SUBSET_PERPID	"TakeSubsetPerPID"

#define IS_POWER_OF_TWO(v)	(!(v & (v - 1)) && v)

extern char **environ;

struct waiting_for_removal {
	SLIST_ENTRY(waiting_for_removal) sle;
	mach_port_t reply_port;
};

static bool waiting4removal_new(job_t j, mach_port_t rp);
static void waiting4removal_delete(job_t j, struct waiting_for_removal *w4r);

struct waiting_for_exit {
	LIST_ENTRY(waiting_for_exit) sle;
	mach_port_t rp;
	bool legacy;
};

static bool waiting4exit_new(job_t j, mach_port_t rp, bool legacy);
static void waiting4exit_delete(job_t j, struct waiting_for_exit *w4e);

struct machservice {
	SLIST_ENTRY(machservice) sle;
	SLIST_ENTRY(machservice) special_port_sle;
	LIST_ENTRY(machservice) name_hash_sle;
	LIST_ENTRY(machservice) port_hash_sle;
	job_t				job;
	unsigned int		gen_num;
	mach_port_name_t	port;
	unsigned int		isActive					:1,
						reset						:1,
						recv						:1,
						hide						:1,
						kUNCServer					:1,
						per_user_hack				:1,
						debug_on_close				:1,
						per_pid						:1,
						delete_on_destruction		:1,
						drain_one_on_crash			:1,
						drain_all_on_crash			:1,
						/* Don't let the size of this field to get too small. It has to be large enough
						 * to represent the reasonable range of special port numbers.
						 */
						special_port_num			:20;
	
	const char		name[0];
};

static SLIST_HEAD(, machservice) special_ports; /* hack, this should be per jobmgr_t */

#define PORT_HASH_SIZE 32
#define HASH_PORT(x)	(IS_POWER_OF_TWO(PORT_HASH_SIZE) ? (MACH_PORT_INDEX(x) & (PORT_HASH_SIZE - 1)) : (MACH_PORT_INDEX(x) % PORT_HASH_SIZE))

static LIST_HEAD(, machservice) port_hash[PORT_HASH_SIZE];

static void machservice_setup(launch_data_t obj, const char *key, void *context);
static void machservice_setup_options(launch_data_t obj, const char *key, void *context);
static void machservice_resetport(job_t j, struct machservice *ms);
static struct machservice *machservice_new(job_t j, const char *name, mach_port_t *serviceport, bool pid_local);
static void machservice_ignore(job_t j, struct machservice *ms);
static void machservice_watch(job_t j, struct machservice *ms);
static void machservice_delete(job_t j, struct machservice *, bool port_died);
static void machservice_request_notifications(struct machservice *);
static mach_port_t machservice_port(struct machservice *);
static job_t machservice_job(struct machservice *);
static bool machservice_hidden(struct machservice *);
static bool machservice_active(struct machservice *);
static const char *machservice_name(struct machservice *);
static bootstrap_status_t machservice_status(struct machservice *);
void machservice_drain_port(struct machservice *);

struct socketgroup {
	SLIST_ENTRY(socketgroup) sle;
	int *fds;
	unsigned int junkfds:1, fd_cnt:31;
	union {
		const char name[0];
		char name_init[0];
	};
};

static bool socketgroup_new(job_t j, const char *name, int *fds, size_t fd_cnt, bool junkfds);
static void socketgroup_delete(job_t j, struct socketgroup *sg);
static void socketgroup_watch(job_t j, struct socketgroup *sg);
static void socketgroup_ignore(job_t j, struct socketgroup *sg);
static void socketgroup_callback(job_t j);
static void socketgroup_setup(launch_data_t obj, const char *key, void *context);
static void socketgroup_kevent_mod(job_t j, struct socketgroup *sg, bool do_add);

struct calendarinterval {
	LIST_ENTRY(calendarinterval) global_sle;
	SLIST_ENTRY(calendarinterval) sle;
	job_t job;
	struct tm when;
	time_t when_next;
};

static LIST_HEAD(, calendarinterval) sorted_calendar_events;

static bool calendarinterval_new(job_t j, struct tm *w);
static bool calendarinterval_new_from_obj(job_t j, launch_data_t obj);
static void calendarinterval_new_from_obj_dict_walk(launch_data_t obj, const char *key, void *context);
static void calendarinterval_delete(job_t j, struct calendarinterval *ci);
static void calendarinterval_setalarm(job_t j, struct calendarinterval *ci);
static void calendarinterval_callback(void);
static void calendarinterval_sanity_check(void);

struct envitem {
	SLIST_ENTRY(envitem) sle;
	bool one_shot;
	char *value;
	union {
		const char key[0];
		char key_init[0];
	};
};

static bool envitem_new(job_t j, const char *k, const char *v, bool global, bool one_shot);
static void envitem_delete(job_t j, struct envitem *ei, bool global);
static void envitem_setup(launch_data_t obj, const char *key, void *context);
static void envitem_setup_one_shot(launch_data_t obj, const char *key, void *context);

struct limititem {
	SLIST_ENTRY(limititem) sle;
	struct rlimit lim;
	unsigned int setsoft:1, sethard:1, which:30;
};

static bool limititem_update(job_t j, int w, rlim_t r);
static void limititem_delete(job_t j, struct limititem *li);
static void limititem_setup(launch_data_t obj, const char *key, void *context);
#if HAVE_SANDBOX
static void seatbelt_setup_flags(launch_data_t obj, const char *key, void *context);
#endif

typedef enum {
	NETWORK_UP = 1,
	NETWORK_DOWN,
	SUCCESSFUL_EXIT,
	FAILED_EXIT,
	PATH_EXISTS,
	PATH_MISSING,
	OTHER_JOB_ENABLED,
	OTHER_JOB_DISABLED,
	OTHER_JOB_ACTIVE,
	OTHER_JOB_INACTIVE,
	PATH_CHANGES,
	DIR_NOT_EMPTY,
	// FILESYSTEMTYPE_IS_MOUNTED,	/* for nfsiod, but maybe others */
} semaphore_reason_t;

struct semaphoreitem {
	SLIST_ENTRY(semaphoreitem) sle;
	semaphore_reason_t why;
	bool watching_parent;
	int fd;
	union {
		const char what[0];
		char what_init[0];
	};
};

struct semaphoreitem_dict_iter_context {
	job_t j;
	semaphore_reason_t why_true;
	semaphore_reason_t why_false;
};

static bool semaphoreitem_new(job_t j, semaphore_reason_t why, const char *what);
static void semaphoreitem_delete(job_t j, struct semaphoreitem *si);
static void semaphoreitem_setup(launch_data_t obj, const char *key, void *context);
static void semaphoreitem_setup_dict_iter(launch_data_t obj, const char *key, void *context);
static void semaphoreitem_callback(job_t j, struct kevent *kev);
static void semaphoreitem_watch(job_t j, struct semaphoreitem *si);
static void semaphoreitem_ignore(job_t j, struct semaphoreitem *si);
static void semaphoreitem_runtime_mod_ref(struct semaphoreitem *si, bool add);

#define ACTIVE_JOB_HASH_SIZE	32
#define ACTIVE_JOB_HASH(x)	(IS_POWER_OF_TWO(ACTIVE_JOB_HASH_SIZE) ? (x & (ACTIVE_JOB_HASH_SIZE - 1)) : (x % ACTIVE_JOB_HASH_SIZE))

#define MACHSERVICE_HASH_SIZE	37

enum {
	JOBMGR_PHASE_HOPEFULLY_EXITS_FIRST,
	JOBMGR_PHASE_NORMAL,
	JOBMGR_PHASE_HOPEFULLY_EXITS_LAST,
	JOBMGR_PHASE_LAST,
};

static char *s_phases[JOBMGR_PHASE_LAST + 1] = {
	"HopefullyExitsFirst",
	"Normal",
	"HopefullyExitsLast",
	"Finalized",
};

struct jobmgr_s {
	kq_callback kqjobmgr_callback;
	SLIST_ENTRY(jobmgr_s) sle;
	SLIST_HEAD(, jobmgr_s) submgrs;
	LIST_HEAD(, job_s) jobs;
	LIST_HEAD(, job_s) jetsam_jobs;
	LIST_HEAD(, job_s) active_jobs[ACTIVE_JOB_HASH_SIZE];
	LIST_HEAD(, machservice) ms_hash[MACHSERVICE_HASH_SIZE];
	LIST_HEAD(, job_s) global_env_jobs;
	STAILQ_HEAD(, job_s) pending_samples;
	mach_port_t jm_port;
	mach_port_t req_port;
	mach_port_t init_audit_session;
	jobmgr_t parentmgr;
	int reboot_flags;
	int shutdown_phase;
	unsigned int global_on_demand_cnt;
	unsigned int hopefully_first_cnt;
	unsigned int normal_active_cnt;
	unsigned int jetsam_jobs_cnt;
	unsigned int 	shutting_down					:1,
					session_initialized				:1, 
					killed_hopefully_first_jobs		:1,
					killed_normal_jobs				:1,
					killed_hopefully_last_jobs		:1,
					killed_stray_jobs				:1;
	char sample_log_file[PATH_MAX];
	uint32_t properties;
	union {
		const char name[0];
		char name_init[0];
	};
};

#define jobmgr_assumes(jm, e)	\
	(unlikely(!(e)) ? jobmgr_log_bug(jm, __LINE__), false : true)

static jobmgr_t jobmgr_new(jobmgr_t jm, mach_port_t requestorport, mach_port_t transfer_port, bool sflag, const char *name, bool no_init, mach_port_t session_port);
static job_t jobmgr_import2(jobmgr_t jm, launch_data_t pload);
static jobmgr_t jobmgr_parent(jobmgr_t jm);
static jobmgr_t jobmgr_do_garbage_collection(jobmgr_t jm);
static bool jobmgr_label_test(jobmgr_t jm, const char *str);
static void jobmgr_reap_bulk(jobmgr_t jm, struct kevent *kev);
static void jobmgr_log_stray_children(jobmgr_t jm, bool kill_strays);
static void jobmgr_kill_stray_children(jobmgr_t jm, pid_t *p, size_t np);
static void jobmgr_remove(jobmgr_t jm);
static void jobmgr_dispatch_all(jobmgr_t jm, bool newmounthack);
static void jobmgr_dequeue_next_sample(jobmgr_t jm);
static job_t jobmgr_init_session(jobmgr_t jm, const char *session_type, bool sflag);
static job_t jobmgr_find_by_pid_deep(jobmgr_t jm, pid_t p, bool anon_okay);
static job_t jobmgr_find_by_pid(jobmgr_t jm, pid_t p, bool create_anon);
static jobmgr_t jobmgr_find_by_name(jobmgr_t jm, const char *where);
static job_t job_mig_intran2(jobmgr_t jm, mach_port_t mport, pid_t upid);
static job_t jobmgr_lookup_per_user_context_internal(job_t j, uid_t which_user, bool dispatch, mach_port_t *mp);
static void job_export_all2(jobmgr_t jm, launch_data_t where);
static void jobmgr_callback(void *obj, struct kevent *kev);
static void jobmgr_setup_env_from_other_jobs(jobmgr_t jm);
static void jobmgr_export_env_from_other_jobs(jobmgr_t jm, launch_data_t dict);
static struct machservice *jobmgr_lookup_service(jobmgr_t jm, const char *name, bool check_parent, pid_t target_pid);
static void jobmgr_logv(jobmgr_t jm, int pri, int err, const char *msg, va_list ap) __attribute__((format(printf, 4, 0)));
static void jobmgr_log(jobmgr_t jm, int pri, const char *msg, ...) __attribute__((format(printf, 3, 4)));
/* static void jobmgr_log_error(jobmgr_t jm, int pri, const char *msg, ...) __attribute__((format(printf, 3, 4))); */
static void jobmgr_log_bug(jobmgr_t jm, unsigned int line);

#define AUTO_PICK_LEGACY_LABEL (const char *)(~0)
#define AUTO_PICK_ANONYMOUS_LABEL (const char *)(~1)

struct suspended_peruser {
	LIST_ENTRY(suspended_peruser) sle;
	job_t j;
};

struct job_s {
	kq_callback kqjob_callback;	/* MUST be first element of this structure for benefit of launchd's run loop. */
	LIST_ENTRY(job_s) sle;
	LIST_ENTRY(job_s) needing_session_sle;
	LIST_ENTRY(job_s) jetsam_sle;
	LIST_ENTRY(job_s) pid_hash_sle;
	LIST_ENTRY(job_s) label_hash_sle;
	LIST_ENTRY(job_s) global_env_sle;
	STAILQ_ENTRY(job_s) pending_samples_sle;
	SLIST_ENTRY(job_s) curious_jobs_sle;
	LIST_HEAD(, suspended_peruser) suspended_perusers;
	LIST_HEAD(, waiting_for_exit) exit_watchers;
	SLIST_HEAD(, socketgroup) sockets;
	SLIST_HEAD(, calendarinterval) cal_intervals;
	SLIST_HEAD(, envitem) global_env;
	SLIST_HEAD(, envitem) env;
	SLIST_HEAD(, limititem) limits;
	SLIST_HEAD(, machservice) machservices;
	SLIST_HEAD(, semaphoreitem) semaphores;
	SLIST_HEAD(, waiting_for_removal) removal_watchers;
	struct rusage ru;
	cpu_type_t *j_binpref;
	size_t j_binpref_cnt;
	mach_port_t j_port;
	mach_port_t wait_reply_port; /* we probably should switch to a list of waiters */
	uid_t mach_uid;
	jobmgr_t mgr;
	size_t argc;
	char **argv;
	char *prog;
	char *rootdir;
	char *workingdir;
	char *username;
	char *groupname;
	char *stdinpath;
	char *stdoutpath;
	char *stderrpath;
	char *alt_exc_handler;
	struct vproc_shmem_s *shmem;
	struct machservice *lastlookup;
	unsigned int lastlookup_gennum;
#if HAVE_SANDBOX
	char *seatbelt_profile;
	uint64_t seatbelt_flags;
#endif
#if HAVE_QUARANTINE
	void *quarantine_data;
	size_t quarantine_data_sz;
#endif
	pid_t p;
	int last_exit_status;
	int stdin_fd;
	int fork_fd;
	int log_redirect_fd;
	int nice;
	int stdout_err_fd;
	long long jetsam_priority;
	long long main_thread_priority;
	uint32_t timeout;
	uint32_t exit_timeout;
	uint64_t sent_signal_time;
	uint64_t start_time;
	uint32_t min_run_time;
	uint32_t start_interval;
	uint32_t peruser_suspend_count; /* The number of jobs that have disabled this per-user launchd. */
#if 0
	/* someday ... */
	enum {
		J_TYPE_ANONYMOUS = 1,
		J_TYPE_LANCHSERVICES,
		J_TYPE_MACHINIT,
		J_TYPE_INETD,
	} j_type;
#endif
	bool 	debug						:1,	/* man launchd.plist --> Debug */
		    ondemand					:1,	/* man launchd.plist --> KeepAlive == false */
		    session_create				:1,	/* man launchd.plist --> SessionCreate */
		    low_pri_io					:1,	/* man launchd.plist --> LowPriorityIO */
		    no_init_groups				:1,	/* man launchd.plist --> InitGroups */
		    priv_port_has_senders		:1,	/* a legacy mach_init concept to make bootstrap_create_server/service() work */
		    importing_global_env		:1,	/* a hack during job importing */
		    importing_hard_limits		:1,	/* a hack during job importing */
		    setmask						:1,	/* man launchd.plist --> Umask */
		    anonymous					:1,	/* a process that launchd knows about, but isn't managed by launchd */
		    checkedin					:1,	/* a legacy mach_init concept to detect sick jobs */
		    legacy_mach_job				:1,	/* a job created via bootstrap_create_server() */
		    legacy_LS_job				:1,	/* a job created via spawn_via_launchd() */
		    inetcompat					:1,	/* a legacy job that wants inetd compatible semantics */
		    inetcompat_wait				:1,	/* a twist on inetd compatibility */
		    start_pending				:1,	/* an event fired and the job should start, but not necessarily right away */
		    globargv					:1,	/* man launchd.plist --> EnableGlobbing */
		    wait4debugger				:1,	/* man launchd.plist --> WaitForDebugger */
			wait4debugger_oneshot		:1, /* One-shot WaitForDebugger. */
		    internal_exc_handler		:1,	/* MachExceptionHandler == true */
		    stall_before_exec			:1,	/* a hack to support an option of spawn_via_launchd() */
		    only_once 					:1,	/* man launchd.plist --> LaunchOnlyOnce. Note: 5465184 Rename this to "HopefullyNeverExits" */
		    currently_ignored			:1,	/* Make job_ignore() / job_watch() work. If these calls were balanced, then this wouldn't be necessarily. */
		    forced_peers_to_demand_mode	:1,	/* A job that forced all other jobs to be temporarily launch-on-demand */
		    setnice						:1,	/* man launchd.plist --> Nice */
		    hopefully_exits_last		:1,	/* man launchd.plist --> HopefullyExitsLast */
		    removal_pending				:1,	/* a job was asked to be unloaded/removed while running, we'll remove it after it exits */
		    sent_sigkill				:1,	/* job_kill() was called */
		    sampling_complete			:1,	/* job_force_sampletool() was called (or is disabled) */
		    debug_before_kill			:1,	/* enter the kernel debugger before killing a job */
		    weird_bootstrap				:1,	/* a hack that launchd+launchctl use during jobmgr_t creation */
		    start_on_mount				:1,	/* man launchd.plist --> StartOnMount */
		    per_user					:1,	/* This job is a per-user launchd managed by the PID 1 launchd */
		    hopefully_exits_first		:1,	/* man launchd.plist --> HopefullyExitsFirst */
		    deny_unknown_mslookups		:1,	/* A flag for changing the behavior of bootstrap_look_up() */
		    unload_at_mig_return		:1,	/* A job thoroughly confused launchd. We need to unload it ASAP */
		    abandon_pg					:1,	/* man launchd.plist --> AbandonProcessGroup */
			ignore_pg_at_shutdown		:1, /* During shutdown, do not send SIGTERM to stray processes in the process group of this job. */
	     	poll_for_vfs_changes		:1,	/* a hack to work around the fact that kqueues don't work on all filesystems */
	     	deny_job_creation			:1,	/* Don't let this job create new 'job_t' objects in launchd */
	     	kill_via_shmem				:1,	/* man launchd.plist --> EnableTransactions */
	     	sent_kill_via_shmem			:1,	/* We need to 'kill_via_shmem' once-and-only-once */
			clean_kill					:1, /* The job was sent SIGKILL because it was clean. */
			pending_sample				:1, /* This job needs to be sampled for some reason. */
			kill_after_sample			:1, /* The job is to be killed after sampling. */
			is_being_sampled			:1, /* We've spawned a sample tool to sample the job. */
			reap_after_trace			:1,	/* The job exited before sample did, so we should reap it after sample is done. */
			nosy						:1, /* The job has an OtherJobEnabled KeepAlive criterion. */
			crashed						:1, /* The job is the default Mach exception handler, and it crashed. */
			reaped						:1, /* We've received NOTE_EXIT for the job. */
			stopped						:1, /* job_stop() was called. */
			jetsam_frontmost			:1, /* The job is considered "frontmost" by Jetsam. */
			needs_kickoff				:1, /* The job is to be kept alive continuously, but it must be initially kicked off. */
			is_bootstrapper				:1, /* The job is a bootstrapper. */
			has_console					:1, /* The job owns the console. */
			clean_exit_timer_expired	:1, /* The job was clean, received SIGKILL and failed to exit after LAUNCHD_CLEAN_KILL_TIMER seconds. */
			embedded_special_privileges	:1, /* The job runs as a non-root user on embedded but has select privileges of the root user. */
			did_exec					:1, /* The job exec(2)ed successfully. */
			migratory					:1; /* The (anonymous) job called vprocmgr_switch_to_session(). */
	mode_t mask;
	pid_t tracing_pid;
	mach_port_t audit_session;
	uuid_t expected_audit_uuid;
	const char label[0];
};

#define LABEL_HASH_SIZE 53

static LIST_HEAD(, job_s) label_hash[LABEL_HASH_SIZE];
static size_t hash_label(const char *label) __attribute__((pure));
static size_t hash_ms(const char *msstr) __attribute__((pure));
static SLIST_HEAD(, job_s) s_curious_jobs;

#define job_assumes(j, e)	\
	(unlikely(!(e)) ? job_log_bug(j, __LINE__), false : true)

static void job_import_keys(launch_data_t obj, const char *key, void *context);
static void job_import_bool(job_t j, const char *key, bool value);
static void job_import_string(job_t j, const char *key, const char *value);
static void job_import_integer(job_t j, const char *key, long long value);
static void job_import_dictionary(job_t j, const char *key, launch_data_t value);
static void job_import_array(job_t j, const char *key, launch_data_t value);
static void job_import_opaque(job_t j, const char *key, launch_data_t value);
static bool job_set_global_on_demand(job_t j, bool val);
static const char *job_active(job_t j);
static void job_watch(job_t j);
static void job_ignore(job_t j);
static void job_cleanup_after_tracer(job_t j);
static void job_reap(job_t j);
static bool job_useless(job_t j);
static bool job_keepalive(job_t j);
static void job_dispatch_curious_jobs(job_t j);
static void job_start(job_t j);
static void job_start_child(job_t j) __attribute__((noreturn));
static void job_setup_attributes(job_t j);
static bool job_setup_machport(job_t j);
static void job_setup_fd(job_t j, int target_fd, const char *path, int flags);
static void job_postfork_become_user(job_t j);
static void job_postfork_test_user(job_t j);
static void job_log_pids_with_weird_uids(job_t j);
static void job_setup_exception_port(job_t j, task_t target_task);
static void job_callback(void *obj, struct kevent *kev);
static void job_callback_proc(job_t j, struct kevent *kev);
static void job_callback_timer(job_t j, void *ident);
static void job_callback_read(job_t j, int ident);
static void job_log_stray_pg(job_t j);
static void job_log_children_without_exec(job_t j);
static job_t job_new_anonymous(jobmgr_t jm, pid_t anonpid) __attribute__((malloc, nonnull, warn_unused_result));
static job_t job_new(jobmgr_t jm, const char *label, const char *prog, const char *const *argv) __attribute__((malloc, nonnull(1,2), warn_unused_result));
static job_t job_new_via_mach_init(job_t j, const char *cmd, uid_t uid, bool ond) __attribute__((malloc, nonnull, warn_unused_result));
static void job_kill(job_t j);
static void job_uncork_fork(job_t j);
static void job_log_stdouterr(job_t j);
static void job_logv(job_t j, int pri, int err, const char *msg, va_list ap) __attribute__((format(printf, 4, 0)));
static void job_log_error(job_t j, int pri, const char *msg, ...) __attribute__((format(printf, 3, 4)));
static void job_log_bug(job_t j, unsigned int line);
static void job_log_stdouterr2(job_t j, const char *msg, ...);
static void job_set_exception_port(job_t j, mach_port_t port);
static kern_return_t job_handle_mpm_wait(job_t j, mach_port_t srp, int *waitstatus);

static const struct {
	const char *key;
	int val;
} launchd_keys2limits[] = {
	{ LAUNCH_JOBKEY_RESOURCELIMIT_CORE,	RLIMIT_CORE	},
	{ LAUNCH_JOBKEY_RESOURCELIMIT_CPU,	RLIMIT_CPU	},
	{ LAUNCH_JOBKEY_RESOURCELIMIT_DATA,	RLIMIT_DATA	},
	{ LAUNCH_JOBKEY_RESOURCELIMIT_FSIZE,	RLIMIT_FSIZE	},
	{ LAUNCH_JOBKEY_RESOURCELIMIT_MEMLOCK,	RLIMIT_MEMLOCK	},
	{ LAUNCH_JOBKEY_RESOURCELIMIT_NOFILE,	RLIMIT_NOFILE	},
	{ LAUNCH_JOBKEY_RESOURCELIMIT_NPROC,	RLIMIT_NPROC	},
	{ LAUNCH_JOBKEY_RESOURCELIMIT_RSS,	RLIMIT_RSS	},
	{ LAUNCH_JOBKEY_RESOURCELIMIT_STACK,	RLIMIT_STACK	},
};

static time_t cronemu(int mon, int mday, int hour, int min);
static time_t cronemu_wday(int wday, int hour, int min);
static bool cronemu_mon(struct tm *wtm, int mon, int mday, int hour, int min);
static bool cronemu_mday(struct tm *wtm, int mday, int hour, int min);
static bool cronemu_hour(struct tm *wtm, int hour, int min);
static bool cronemu_min(struct tm *wtm, int min);

/* miscellaneous file local functions */
static size_t get_kern_max_proc(void);
static int dir_has_files(job_t j, const char *path);
static char **mach_cmd2argv(const char *string);
static size_t our_strhash(const char *s) __attribute__((pure));
static void extract_rcsid_substr(const char *i, char *o, size_t osz);
static void simulate_pid1_crash(void);
static pid_t basic_spawn(job_t j, void (*what_to_do)(job_t));
static void take_sample(job_t j);

void eliminate_double_reboot(void);

/* For Jetsam. */
static void jetsam_priority_from_job(job_t j, bool front, jetsam_priority_entry_t *jp);
static int job_cmp(const job_t *lhs, const job_t *rhs);
int launchd_set_jetsam_priorities(launch_data_t priorities);

/* file local globals */
static size_t total_children;
static size_t total_anon_children;
static mach_port_t the_exception_server;
static job_t workaround_5477111;
static LIST_HEAD(, job_s) s_needing_sessions;
mach_port_t g_audit_session_port = MACH_PORT_NULL;

#if !TARGET_OS_EMBEDDED
static job_t s_embedded_privileged_job = (job_t)&root_jobmgr;
au_asid_t g_audit_session = AU_DEFAUDITSID;
#else
static job_t s_embedded_privileged_job = NULL;
pid_t g_audit_session = 0;
#endif

static int s_no_hang_fd = -1;

/* process wide globals */
mach_port_t inherited_bootstrap_port;
jobmgr_t root_jobmgr;
bool g_shutdown_debugging = false;
bool g_verbose_boot = false;
bool g_embedded_privileged_action = false;

void
job_ignore(job_t j)
{
	struct semaphoreitem *si;
	struct socketgroup *sg;
	struct machservice *ms;

	if (j->currently_ignored) {
		return;
	}

	job_log(j, LOG_DEBUG, "Ignoring...");

	j->currently_ignored = true;

	if (j->poll_for_vfs_changes) {
		j->poll_for_vfs_changes = false;
		job_assumes(j, kevent_mod((uintptr_t)&j->semaphores, EVFILT_TIMER, EV_DELETE, 0, 0, j) != -1);
	}

	SLIST_FOREACH(sg, &j->sockets, sle) {
		socketgroup_ignore(j, sg);
	}

	SLIST_FOREACH(ms, &j->machservices, sle) {
		machservice_ignore(j, ms);
	}

	SLIST_FOREACH(si, &j->semaphores, sle) {
		semaphoreitem_ignore(j, si);
	}
}

void
job_watch(job_t j)
{
	struct semaphoreitem *si;
	struct socketgroup *sg;
	struct machservice *ms;

	if (!j->currently_ignored) {
		return;
	}

	job_log(j, LOG_DEBUG, "Watching...");

	j->currently_ignored = false;

	SLIST_FOREACH(sg, &j->sockets, sle) {
		socketgroup_watch(j, sg);
	}

	SLIST_FOREACH(ms, &j->machservices, sle) {
		machservice_watch(j, ms);
	}

	SLIST_FOREACH(si, &j->semaphores, sle) {
		semaphoreitem_watch(j, si);
	}
}

void
job_stop(job_t j)
{
	char extralog[100];
	int32_t newval = 1;

	if (unlikely(!j->p || j->anonymous)) {
		return;
	}
	
#if !TARGET_OS_EMBEDDED
	if (j->kill_via_shmem && !g_force_old_kill_path) {
		if (j->shmem) {
			if (!j->sent_kill_via_shmem) {
				j->shmem->vp_shmem_flags |= VPROC_SHMEM_EXITING;
				newval = __sync_sub_and_fetch(&j->shmem->vp_shmem_transaction_cnt, 1);
				j->sent_kill_via_shmem = true;
			} else {
				newval = j->shmem->vp_shmem_transaction_cnt;
			}
		} else {
			newval = -1;
		}
	} else if( j->kill_via_shmem ) {
		job_log(j, LOG_DEBUG, "Stopping transactional job the old-fashioned way.");
	}
#endif

#if TARGET_OS_EMBEDDED
	if( g_embedded_privileged_action && s_embedded_privileged_job ) {
		if( !job_assumes(j, s_embedded_privileged_job->username != NULL && j->username != NULL) ) {
			errno = EPERM;
			return;
		}
	
		if( strcmp(j->username, s_embedded_privileged_job->username) != 0 ) {
			errno = EPERM;
			return;
		}
	} else if( g_embedded_privileged_action ) {
		errno = EINVAL;
		return;
	}
#endif

	j->sent_signal_time = runtime_get_opaque_time();

	if (newval < 0) {
		j->clean_kill = true;
		job_kill(j);
	} else {
		/*
		 * If sampling is enabled and SAMPLE_TIMEOUT is earlier than the job exit_timeout,
		 * then set a timer for SAMPLE_TIMEOUT seconds after killing
		 */
		unsigned int exit_timeout = j->exit_timeout;
		bool do_sample = do_apple_internal_logging;
		unsigned int timeout = exit_timeout;

		if (do_sample && (!exit_timeout || (LAUNCHD_SAMPLE_TIMEOUT < exit_timeout))) {
			timeout = LAUNCHD_SAMPLE_TIMEOUT;
		}

		job_assumes(j, runtime_kill(j->p, SIGTERM) != -1);

		if (timeout) {
			j->sampling_complete = !do_sample;
			job_assumes(j, kevent_mod((uintptr_t)&j->exit_timeout, EVFILT_TIMER,
						EV_ADD|EV_ONESHOT, NOTE_SECONDS, timeout, j) != -1);
		}

		if (!exit_timeout) {
			job_log(j, LOG_DEBUG, "This job has an infinite exit timeout");
		}

		if (j->kill_via_shmem) {
			snprintf(extralog, sizeof(extralog), ": %d remaining transactions", newval + 1);
		} else {
			extralog[0] = '\0';
		}

		job_log(j, LOG_DEBUG, "Sent SIGTERM signal%s", extralog);
	}
	
	j->stopped = true;
}

launch_data_t
job_export(job_t j)
{
	launch_data_t tmp, tmp2, tmp3, r = launch_data_alloc(LAUNCH_DATA_DICTIONARY);

	if (r == NULL) {
		return NULL;
	}

	if ((tmp = launch_data_new_string(j->label))) {
		launch_data_dict_insert(r, tmp, LAUNCH_JOBKEY_LABEL);
	}
	if ((tmp = launch_data_new_string(j->mgr->name))) {
		launch_data_dict_insert(r, tmp, LAUNCH_JOBKEY_LIMITLOADTOSESSIONTYPE);
	}
	if ((tmp = launch_data_new_bool(j->ondemand))) {
		launch_data_dict_insert(r, tmp, LAUNCH_JOBKEY_ONDEMAND);
	}
	if ((tmp = launch_data_new_integer(j->last_exit_status))) {
		launch_data_dict_insert(r, tmp, LAUNCH_JOBKEY_LASTEXITSTATUS);
	} 
	if (j->p && (tmp = launch_data_new_integer(j->p))) {
		launch_data_dict_insert(r, tmp, LAUNCH_JOBKEY_PID);
	}
	if ((tmp = launch_data_new_integer(j->timeout))) {
		launch_data_dict_insert(r, tmp, LAUNCH_JOBKEY_TIMEOUT);
	}
	if (j->prog && (tmp = launch_data_new_string(j->prog))) {
		launch_data_dict_insert(r, tmp, LAUNCH_JOBKEY_PROGRAM);
	}
	if (j->stdinpath && (tmp = launch_data_new_string(j->stdinpath))) {
		launch_data_dict_insert(r, tmp, LAUNCH_JOBKEY_STANDARDINPATH);
	}
	if (j->stdoutpath && (tmp = launch_data_new_string(j->stdoutpath))) {
		launch_data_dict_insert(r, tmp, LAUNCH_JOBKEY_STANDARDOUTPATH);
	}
	if (j->stderrpath && (tmp = launch_data_new_string(j->stderrpath))) {
		launch_data_dict_insert(r, tmp, LAUNCH_JOBKEY_STANDARDERRORPATH);
	}
	if (likely(j->argv) && (tmp = launch_data_alloc(LAUNCH_DATA_ARRAY))) {
		size_t i;

		for (i = 0; i < j->argc; i++) {
			if ((tmp2 = launch_data_new_string(j->argv[i]))) {
				launch_data_array_set_index(tmp, tmp2, i);
			}
		}

		launch_data_dict_insert(r, tmp, LAUNCH_JOBKEY_PROGRAMARGUMENTS);
	}

	if (j->kill_via_shmem && (tmp = launch_data_new_bool(true))) {
		int32_t tmp_cnt = -1;

		launch_data_dict_insert(r, tmp, LAUNCH_JOBKEY_ENABLETRANSACTIONS);

		if (j->shmem) {
			tmp_cnt = j->shmem->vp_shmem_transaction_cnt;
		}

		if (j->sent_kill_via_shmem) {
			tmp_cnt++;
		}

		if ((tmp = launch_data_new_integer(tmp_cnt))) {
			launch_data_dict_insert(r, tmp, LAUNCH_JOBKEY_TRANSACTIONCOUNT);
		}
	}

	if (j->session_create && (tmp = launch_data_new_bool(true))) {
		launch_data_dict_insert(r, tmp, LAUNCH_JOBKEY_SESSIONCREATE);
	}

	if (j->inetcompat && (tmp = launch_data_alloc(LAUNCH_DATA_DICTIONARY))) {
		if ((tmp2 = launch_data_new_bool(j->inetcompat_wait))) {
			launch_data_dict_insert(tmp, tmp2, LAUNCH_JOBINETDCOMPATIBILITY_WAIT);
		}
		launch_data_dict_insert(r, tmp, LAUNCH_JOBKEY_INETDCOMPATIBILITY);
	}

	if (!SLIST_EMPTY(&j->sockets) && (tmp = launch_data_alloc(LAUNCH_DATA_DICTIONARY))) {
		struct socketgroup *sg;
		int i;

		SLIST_FOREACH(sg, &j->sockets, sle) {
			if (sg->junkfds) {
				continue;
			}
			if ((tmp2 = launch_data_alloc(LAUNCH_DATA_ARRAY))) {
				for (i = 0; i < sg->fd_cnt; i++) {
					if ((tmp3 = launch_data_new_fd(sg->fds[i]))) {
						launch_data_array_set_index(tmp2, tmp3, i);
					}
				}
				launch_data_dict_insert(tmp, tmp2, sg->name);
			}
		}

		launch_data_dict_insert(r, tmp, LAUNCH_JOBKEY_SOCKETS);
	}

	if (!SLIST_EMPTY(&j->machservices) && (tmp = launch_data_alloc(LAUNCH_DATA_DICTIONARY))) {
		struct machservice *ms;
		
		tmp3 = NULL;

		SLIST_FOREACH(ms, &j->machservices, sle) {
			if (ms->per_pid) {
				if (tmp3 == NULL) {
					tmp3 = launch_data_alloc(LAUNCH_DATA_DICTIONARY);
				}
				if (tmp3) {
					tmp2 = launch_data_new_machport(MACH_PORT_NULL);
					launch_data_dict_insert(tmp3, tmp2, ms->name);
				}
			} else {
				tmp2 = launch_data_new_machport(MACH_PORT_NULL);
				launch_data_dict_insert(tmp, tmp2, ms->name);
			}
		}

		launch_data_dict_insert(r, tmp, LAUNCH_JOBKEY_MACHSERVICES);

		if (tmp3) {
			launch_data_dict_insert(r, tmp3, LAUNCH_JOBKEY_PERJOBMACHSERVICES);
		}
	}

	return r;
}

static void
jobmgr_log_active_jobs(jobmgr_t jm)
{
	const char *why_active;
	jobmgr_t jmi;
	job_t ji;

	SLIST_FOREACH(jmi, &jm->submgrs, sle) {
		jobmgr_log_active_jobs(jmi);
	}

	LIST_FOREACH(ji, &jm->jobs, sle) {
		if( (why_active = job_active(ji)) ) {
			job_log(ji, LOG_DEBUG | LOG_CONSOLE, "%s", why_active);
		}
	}
}

static void
jobmgr_still_alive_with_check(jobmgr_t jm)
{
	jobmgr_log(jm, LOG_DEBUG | LOG_CONSOLE, "Still alive with %lu/%lu (normal/anonymous) children. In %s phase of shutdown.", total_children, total_anon_children, s_phases[jm->shutdown_phase]);
	jobmgr_log_active_jobs(jm);
}

jobmgr_t
jobmgr_shutdown(jobmgr_t jm)
{
	jobmgr_t jmi, jmn;
	jobmgr_log(jm, LOG_DEBUG, "Beginning job manager shutdown with flags: %s", reboot_flags_to_C_names(jm->reboot_flags));

	jm->shutting_down = true;

	SLIST_FOREACH_SAFE(jmi, &jm->submgrs, sle, jmn) {
		jobmgr_shutdown(jmi);
	}
	
	if (jm->parentmgr == NULL && pid1_magic) {
		jobmgr_assumes(jm, kevent_mod((uintptr_t)jm, EVFILT_TIMER, EV_ADD, NOTE_SECONDS, 5, jm));
	#if !TARGET_OS_EMBEDDED
		/* Kill the update thread. */
		jobmgr_assumes(jm, __sync_sub_and_fetch(&g_sync_frequency, 30) == 0);
	#endif
	}

	return jobmgr_do_garbage_collection(jm);
}

void
jobmgr_remove(jobmgr_t jm)
{
	jobmgr_t jmi;
	job_t ji;

	jobmgr_log(jm, LOG_DEBUG, "Removing job manager.");
	if (!jobmgr_assumes(jm, SLIST_EMPTY(&jm->submgrs))) {
		while ((jmi = SLIST_FIRST(&jm->submgrs))) {
			jobmgr_remove(jmi);
		}
	}

	while( (ji = LIST_FIRST(&jm->jobs)) ) {
		if( !ji->anonymous && ji->p ) {
			job_log(ji, LOG_WARNING | LOG_CONSOLE, "Job has overstayed its welcome. Forcing removal.");
			ji->p = 0;
		}
		job_remove(ji);
	}

	if (jm->req_port) {
		jobmgr_assumes(jm, launchd_mport_deallocate(jm->req_port) == KERN_SUCCESS);
	}

	if (jm->jm_port) {
		jobmgr_assumes(jm, launchd_mport_close_recv(jm->jm_port) == KERN_SUCCESS);
	}

	if (jm->parentmgr) {
		runtime_del_weak_ref();
		SLIST_REMOVE(&jm->parentmgr->submgrs, jm, jobmgr_s, sle);
	} else if (pid1_magic) {
		eliminate_double_reboot();
		launchd_log_vm_stats();
		jobmgr_log(root_jobmgr, LOG_NOTICE | LOG_CONSOLE, "About to call: reboot(%s).", reboot_flags_to_C_names(jm->reboot_flags));
		runtime_closelog();
		jobmgr_assumes(jm, reboot(jm->reboot_flags) != -1);
	} else {
		jobmgr_log(jm, LOG_DEBUG, "About to exit");
		runtime_closelog();
		exit(EXIT_SUCCESS);
	}
	
	free(jm);
}

void
job_remove(job_t j)
{
	struct waiting_for_removal *w4r;
	struct calendarinterval *ci;
	struct semaphoreitem *si;
	struct socketgroup *sg;
	struct machservice *ms;
	struct limititem *li;
	struct envitem *ei;
	
#if TARGET_OS_EMBEDDED
	if( g_embedded_privileged_action && s_embedded_privileged_job ) {
		if( !job_assumes(j, s_embedded_privileged_job->username != NULL && j->username != NULL) ) {
			errno = EPERM;
			return;
		}
	
		if( strcmp(j->username, s_embedded_privileged_job->username) != 0 ) {
			errno = EPERM;
			return;
		}
	} else if( g_embedded_privileged_action ) {
		errno = EINVAL;
		return;
	}
#endif
	
	if (unlikely(j->p)) {
		if (j->anonymous) {
			job_reap(j);
		} else {
			job_log(j, LOG_DEBUG, "Removal pended until the job exits");

			if (!j->removal_pending) {
				j->removal_pending = true;
				job_stop(j);
			}
			return;
		}
	}
	
	job_dispatch_curious_jobs(j);

	ipc_close_all_with_job(j);

	job_log(j, LOG_INFO, "Total rusage: utime %ld.%06u stime %ld.%06u maxrss %lu ixrss %lu idrss %lu isrss %lu minflt %lu majflt %lu nswap %lu inblock %lu oublock %lu msgsnd %lu msgrcv %lu nsignals %lu nvcsw %lu nivcsw %lu",
			j->ru.ru_utime.tv_sec, j->ru.ru_utime.tv_usec,
			j->ru.ru_stime.tv_sec, j->ru.ru_stime.tv_usec,
			j->ru.ru_maxrss, j->ru.ru_ixrss, j->ru.ru_idrss, j->ru.ru_isrss,
			j->ru.ru_minflt, j->ru.ru_majflt,
			j->ru.ru_nswap, j->ru.ru_inblock, j->ru.ru_oublock,
			j->ru.ru_msgsnd, j->ru.ru_msgrcv,
			j->ru.ru_nsignals, j->ru.ru_nvcsw, j->ru.ru_nivcsw);

	if (j->forced_peers_to_demand_mode) {
		job_set_global_on_demand(j, false);
	}

	if (!job_assumes(j, j->fork_fd == 0)) {
		job_assumes(j, runtime_close(j->fork_fd) != -1);
	}

	if (j->stdin_fd) {
		job_assumes(j, runtime_close(j->stdin_fd) != -1);
	}

	if (!job_assumes(j, j->log_redirect_fd == 0)) {
		job_assumes(j, runtime_close(j->log_redirect_fd) != -1);
	}

	if (j->j_port) {
		job_assumes(j, launchd_mport_close_recv(j->j_port) == KERN_SUCCESS);
	}

	if (!job_assumes(j, j->wait_reply_port == MACH_PORT_NULL)) {
		job_assumes(j, launchd_mport_deallocate(j->wait_reply_port) == KERN_SUCCESS);
	}

	while ((sg = SLIST_FIRST(&j->sockets))) {
		socketgroup_delete(j, sg);
	}
	while ((ci = SLIST_FIRST(&j->cal_intervals))) {
		calendarinterval_delete(j, ci);
	}
	while ((ei = SLIST_FIRST(&j->env))) {
		envitem_delete(j, ei, false);
	}
	while ((ei = SLIST_FIRST(&j->global_env))) {
		envitem_delete(j, ei, true);
	}
	while ((li = SLIST_FIRST(&j->limits))) {
		limititem_delete(j, li);
	}
	while ((ms = SLIST_FIRST(&j->machservices))) {
		machservice_delete(j, ms, false);
	}
	while ((si = SLIST_FIRST(&j->semaphores))) {
		semaphoreitem_delete(j, si);
	}
	while ((w4r = SLIST_FIRST(&j->removal_watchers))) {
		waiting4removal_delete(j, w4r);
	}

	if (j->prog) {
		free(j->prog);
	}
	if (j->argv) {
		free(j->argv);
	}
	if (j->rootdir) {
		free(j->rootdir);
	}
	if (j->workingdir) {
		free(j->workingdir);
	}
	if (j->username) {
		free(j->username);
	}
	if (j->groupname) {
		free(j->groupname);
	}
	if (j->stdinpath) {
		free(j->stdinpath);
	}
	if (j->stdoutpath) {
		free(j->stdoutpath);
	}
	if (j->stderrpath) {
		free(j->stderrpath);
	}
	if (j->alt_exc_handler) {
		free(j->alt_exc_handler);
	}
#if HAVE_SANDBOX
	if (j->seatbelt_profile) {
		free(j->seatbelt_profile);
	}
#endif
#if HAVE_QUARANTINE
	if (j->quarantine_data) {
		free(j->quarantine_data);
	}
#endif
	if (j->j_binpref) {
		free(j->j_binpref);
	}
	if (j->start_interval) {
		runtime_del_weak_ref();
		job_assumes(j, kevent_mod((uintptr_t)&j->start_interval, EVFILT_TIMER, EV_DELETE, 0, 0, NULL) != -1);
	}
	if (j->poll_for_vfs_changes) {
		job_assumes(j, kevent_mod((uintptr_t)&j->semaphores, EVFILT_TIMER, EV_DELETE, 0, 0, j) != -1);
	}
	if( j->exit_timeout ) {
		/* Not a big deal if this fails. It means that the timer's already been freed. */
		kevent_mod((uintptr_t)&j->exit_timeout, EVFILT_TIMER, EV_DELETE, 0, 0, NULL);
	}
	if( j->jetsam_priority != LAUNCHD_JETSAM_PRIORITY_UNSET ) {
		LIST_REMOVE(j, jetsam_sle);
		j->mgr->jetsam_jobs_cnt--;
	}
	if( j->audit_session != MACH_PORT_NULL ) {
		job_assumes(j, mach_port_deallocate(mach_task_self(), j->audit_session) == KERN_SUCCESS);
	}
	if( !uuid_is_null(j->expected_audit_uuid) ) {
		LIST_REMOVE(j, needing_session_sle);
	}
	if( j->embedded_special_privileges ) {
		s_embedded_privileged_job = NULL;
	}
	
	kevent_mod((uintptr_t)j, EVFILT_TIMER, EV_DELETE, 0, 0, NULL);
	
	LIST_REMOVE(j, sle);
	LIST_REMOVE(j, label_hash_sle);

	job_log(j, LOG_DEBUG, "Removed");

	free(j);
}

void
socketgroup_setup(launch_data_t obj, const char *key, void *context)
{
	launch_data_t tmp_oai;
	job_t j = context;
	size_t i, fd_cnt = 1;
	int *fds;

	if (launch_data_get_type(obj) == LAUNCH_DATA_ARRAY) {
		fd_cnt = launch_data_array_get_count(obj);
	}

	fds = alloca(fd_cnt * sizeof(int));

	for (i = 0; i < fd_cnt; i++) {
		if (launch_data_get_type(obj) == LAUNCH_DATA_ARRAY) {
			tmp_oai = launch_data_array_get_index(obj, i);
		} else {
			tmp_oai = obj;
		}

		fds[i] = launch_data_get_fd(tmp_oai);
	}

	socketgroup_new(j, key, fds, fd_cnt, strcmp(key, LAUNCH_JOBKEY_BONJOURFDS) == 0);

	ipc_revoke_fds(obj);
}

bool
job_set_global_on_demand(job_t j, bool val)
{
	if (j->forced_peers_to_demand_mode && val) {
		return false;
	} else if (!j->forced_peers_to_demand_mode && !val) {
		return false;
	}

	if ((j->forced_peers_to_demand_mode = val)) {
		j->mgr->global_on_demand_cnt++;
	} else {
		j->mgr->global_on_demand_cnt--;
	}

	if (j->mgr->global_on_demand_cnt == 0) {
		jobmgr_dispatch_all(j->mgr, false);
	}

	return true;
}

bool
job_setup_machport(job_t j)
{
	mach_msg_size_t mxmsgsz;

	if (!job_assumes(j, launchd_mport_create_recv(&j->j_port) == KERN_SUCCESS)) {
		goto out_bad;
	}

	/* Sigh... at the moment, MIG has maxsize == sizeof(reply union) */
	mxmsgsz = (typeof(mxmsgsz)) sizeof(union __RequestUnion__job_mig_protocol_vproc_subsystem);
	if (job_mig_protocol_vproc_subsystem.maxsize > mxmsgsz) {
		mxmsgsz = job_mig_protocol_vproc_subsystem.maxsize;
	}

	if (!job_assumes(j, runtime_add_mport(j->j_port, protocol_vproc_server, mxmsgsz) == KERN_SUCCESS)) {
		goto out_bad2;
	}

	if (!job_assumes(j, launchd_mport_notify_req(j->j_port, MACH_NOTIFY_NO_SENDERS) == KERN_SUCCESS)) {
		job_assumes(j, launchd_mport_close_recv(j->j_port) == KERN_SUCCESS);
		goto out_bad;
	}

	return true;
out_bad2:
	job_assumes(j, launchd_mport_close_recv(j->j_port) == KERN_SUCCESS);
out_bad:
	return false;
}

job_t 
job_new_via_mach_init(job_t j, const char *cmd, uid_t uid, bool ond)
{
	const char **argv = (const char **)mach_cmd2argv(cmd);
	job_t jr = NULL;

	if (!job_assumes(j, argv != NULL)) {
		goto out_bad;
	}

	jr = job_new(j->mgr, AUTO_PICK_LEGACY_LABEL, NULL, argv);

	free(argv);

	/* jobs can easily be denied creation during shutdown */
	if (unlikely(jr == NULL)) {
		goto out_bad;
	}

	jr->mach_uid = uid;
	jr->ondemand = ond;
	jr->legacy_mach_job = true;
	jr->abandon_pg = true;
	jr->priv_port_has_senders = true; /* the IPC that called us will make-send on this port */

	if (!job_setup_machport(jr)) {
		goto out_bad;
	}

	job_log(jr, LOG_INFO, "Legacy%s server created", ond ? " on-demand" : "");

	return jr;

out_bad:
	if (jr) {
		job_remove(jr);
	}
	return NULL;
}

kern_return_t
job_handle_mpm_wait(job_t j, mach_port_t srp, int *waitstatus)
{
	if (j->p) {
		j->wait_reply_port = srp;
		return MIG_NO_REPLY;
	}

	*waitstatus = j->last_exit_status;

	return 0;
}

job_t
job_new_anonymous(jobmgr_t jm, pid_t anonpid)
{
	int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, anonpid };
	struct kinfo_proc kp;
	size_t len = sizeof(kp);
	bool shutdown_state;
	job_t jp = NULL, jr = NULL;
	uid_t kp_euid, kp_uid, kp_svuid;
	gid_t kp_egid, kp_gid, kp_svgid;

	if (!jobmgr_assumes(jm, anonpid != 0)) {
		errno = EINVAL;
		return NULL;
	}
	
	if (!jobmgr_assumes(jm, anonpid < 100000)) {
		/* The kernel current defines PID_MAX to be 99999, but that define isn't exported */
		errno = EINVAL;
		return NULL;
	}

	if (!jobmgr_assumes(jm, sysctl(mib, 4, &kp, &len, NULL, 0) != -1)) {
		return NULL;
	}

	if (unlikely(len != sizeof(kp))) {
		jobmgr_log(jm, LOG_DEBUG, "Tried to create an anonymous job for nonexistent PID: %u", anonpid);
		errno = ESRCH;
		return NULL;
	}

	if (!jobmgr_assumes(jm, kp.kp_proc.p_comm[0] != '\0')) {
		errno = EINVAL;
		return NULL;
	}

	if (unlikely(kp.kp_proc.p_stat == SZOMB)) {
		jobmgr_log(jm, LOG_DEBUG, "Tried to create an anonymous job for zombie PID %u: %s", anonpid, kp.kp_proc.p_comm);
	}

	if (unlikely(kp.kp_proc.p_flag & P_SUGID)) {
		jobmgr_log(jm, LOG_DEBUG, "Inconsistency: P_SUGID is set on PID %u: %s", anonpid, kp.kp_proc.p_comm);
	}

	kp_euid = kp.kp_eproc.e_ucred.cr_uid;
	kp_uid = kp.kp_eproc.e_pcred.p_ruid;
	kp_svuid = kp.kp_eproc.e_pcred.p_svuid;
	kp_egid = kp.kp_eproc.e_ucred.cr_gid;
	kp_gid = kp.kp_eproc.e_pcred.p_rgid;
	kp_svgid = kp.kp_eproc.e_pcred.p_svgid;

	if (unlikely(kp_euid != kp_uid || kp_euid != kp_svuid || kp_uid != kp_svuid || kp_egid != kp_gid || kp_egid != kp_svgid || kp_gid != kp_svgid)) {
		jobmgr_log(jm, LOG_DEBUG, "Inconsistency: Mixed credentials (e/r/s UID %u/%u/%u GID %u/%u/%u) detected on PID %u: %s",
				kp_euid, kp_uid, kp_svuid, kp_egid, kp_gid, kp_svgid, anonpid, kp.kp_proc.p_comm);
	}

	switch (kp.kp_eproc.e_ppid) {
	case 0:
		/* the kernel */
		break;
	case 1:
		if (!pid1_magic) {
			/* we cannot possibly find a parent job_t that is useful in this function */
			break;
		}
		/* fall through */
	default:
		jp = jobmgr_find_by_pid(jm, kp.kp_eproc.e_ppid, true);
		jobmgr_assumes(jm, jp != NULL);
		break;
	}

	if (jp && !jp->anonymous && unlikely(!(kp.kp_proc.p_flag & P_EXEC))) {
		job_log(jp, LOG_DEBUG, "Called *fork(). Please switch to posix_spawn*(), pthreads or launchd. Child PID %u",
				kp.kp_proc.p_pid);
	}


	/* A total hack: Normally, job_new() returns an error during shutdown, but anonymous jobs are special. */
	if (unlikely(shutdown_state = jm->shutting_down)) {
		jm->shutting_down = false;
	}

	if (jobmgr_assumes(jm, (jr = job_new(jm, AUTO_PICK_ANONYMOUS_LABEL, kp.kp_proc.p_comm, NULL)) != NULL)) {
		u_int proc_fflags = NOTE_EXEC|NOTE_FORK|NOTE_EXIT|NOTE_REAP;

		total_anon_children++;
		jr->anonymous = true;
		jr->p = anonpid;

		/* anonymous process reaping is messy */
		LIST_INSERT_HEAD(&jm->active_jobs[ACTIVE_JOB_HASH(jr->p)], jr, pid_hash_sle);

		if (unlikely(kevent_mod(jr->p, EVFILT_PROC, EV_ADD, proc_fflags, 0, root_jobmgr) == -1) && job_assumes(jr, errno == ESRCH)) {
			/* zombies are weird */
			job_log(jr, LOG_ERR, "Failed to add kevent for PID %u. Will unload at MIG return", jr->p);
			jr->unload_at_mig_return = true;
		}

		if (unlikely(shutdown_state && jm->hopefully_first_cnt == 0)) {
			job_log(jr, LOG_SCOLDING, "This process showed up to the party while all the guests were leaving. Odds are that it will have a miserable time.");
		}

		job_log(jr, LOG_DEBUG, "Created PID %u anonymously by PPID %u%s%s", anonpid, kp.kp_eproc.e_ppid, jp ? ": " : "", jp ? jp->label : "");
	}

	if (unlikely(shutdown_state)) {
		jm->shutting_down = true;
	}

	return jr;
}

job_t 
job_new(jobmgr_t jm, const char *label, const char *prog, const char *const *argv)
{
	const char *const *argv_tmp = argv;
	char tmp_path[PATH_MAX];
	char auto_label[1000];
	const char *bn = NULL;
	char *co;
	size_t minlabel_len;
	size_t i, cc = 0;
	job_t j;

	launchd_assert(offsetof(struct job_s, kqjob_callback) == 0);

	if (unlikely(jm->shutting_down)) {
		errno = EINVAL;
		return NULL;
	}

	if (unlikely(prog == NULL && argv == NULL)) {
		errno = EINVAL;
		return NULL;
	}

	char *anon_or_legacy = ( label == AUTO_PICK_ANONYMOUS_LABEL ) ? "anonymous" : "mach_init";
	if (unlikely(label == AUTO_PICK_LEGACY_LABEL || label == AUTO_PICK_ANONYMOUS_LABEL)) {
		if (prog) {
			bn = prog;
		} else {
			strlcpy(tmp_path, argv[0], sizeof(tmp_path));
			bn = basename(tmp_path); /* prog for auto labels is kp.kp_kproc.p_comm */
		}
		snprintf(auto_label, sizeof(auto_label), "%s.%s.%s", sizeof(void *) == 8 ? "0xdeadbeeffeedface" : "0xbabecafe", anon_or_legacy, bn);
		label = auto_label;
		/* This is so we can do gross things later. See NOTE_EXEC for anonymous jobs */
		minlabel_len = strlen(label) + MAXCOMLEN;
	} else {
		minlabel_len = strlen(label);
	}

	j = calloc(1, sizeof(struct job_s) + minlabel_len + 1);

	if (!jobmgr_assumes(jm, j != NULL)) {
		return NULL;
	}

	if (unlikely(label == auto_label)) {
		snprintf((char *)j->label, strlen(label) + 1, "%p.%s.%s", j, anon_or_legacy, bn);
	} else {
		strcpy((char *)j->label, label);
	}
	j->kqjob_callback = job_callback;
	j->mgr = jm;
	j->min_run_time = LAUNCHD_MIN_JOB_RUN_TIME;
	j->timeout = RUNTIME_ADVISABLE_IDLE_TIMEOUT;
	j->exit_timeout = LAUNCHD_DEFAULT_EXIT_TIMEOUT;
	j->currently_ignored = true;
	j->ondemand = true;
	j->checkedin = true;
	j->jetsam_priority = LAUNCHD_JETSAM_PRIORITY_UNSET;
	uuid_clear(j->expected_audit_uuid);
	
	if (prog) {
		j->prog = strdup(prog);
		if (!job_assumes(j, j->prog != NULL)) {
			goto out_bad;
		}
	}

	if (likely(argv)) {
		while (*argv_tmp++) {
			j->argc++;
		}

		for (i = 0; i < j->argc; i++) {
			cc += strlen(argv[i]) + 1;
		}

		j->argv = malloc((j->argc + 1) * sizeof(char *) + cc);

		if (!job_assumes(j, j->argv != NULL)) {
			goto out_bad;
		}

		co = ((char *)j->argv) + ((j->argc + 1) * sizeof(char *));

		for (i = 0; i < j->argc; i++) {
			j->argv[i] = co;
			strcpy(co, argv[i]);
			co += strlen(argv[i]) + 1;
		}
		j->argv[i] = NULL;
	}

	if( strcmp(j->label, "com.apple.WindowServer") == 0 ) {
		j->has_console = true;
	}

	LIST_INSERT_HEAD(&jm->jobs, j, sle);
	LIST_INSERT_HEAD(&label_hash[hash_label(j->label)], j, label_hash_sle);
	uuid_clear(j->expected_audit_uuid);

	job_log(j, LOG_DEBUG, "Conceived");

	return j;

out_bad:
	if (j->prog) {
		free(j->prog);
	}
	free(j);

	return NULL;
}

job_t 
job_import(launch_data_t pload)
{
	job_t j = jobmgr_import2(root_jobmgr, pload);

	if (unlikely(j == NULL)) {
		return NULL;
	}

	/* Since jobs are effectively stalled until they get security sessions assigned
	 * to them, we may wish to reconsider this behavior of calling the job "enabled"
	 * as far as other jobs with the OtherJobEnabled KeepAlive criterion set.
	 */
	job_dispatch_curious_jobs(j);
	return job_dispatch(j, false);
}

launch_data_t
job_import_bulk(launch_data_t pload)
{
	launch_data_t resp = launch_data_alloc(LAUNCH_DATA_ARRAY);
	job_t *ja;
	size_t i, c = launch_data_array_get_count(pload);

	ja = alloca(c * sizeof(job_t));

	for (i = 0; i < c; i++) {
		if( (likely(ja[i] = jobmgr_import2(root_jobmgr, launch_data_array_get_index(pload, i)))) && errno != ENEEDAUTH ) {
			errno = 0;
		}
		launch_data_array_set_index(resp, launch_data_new_errno(errno), i);
	}

	for (i = 0; i < c; i++) {
		if (likely(ja[i])) {
			job_dispatch_curious_jobs(ja[i]);
			job_dispatch(ja[i], false);
		}
	}

	return resp;
}

void
job_import_bool(job_t j, const char *key, bool value)
{
	bool found_key = false;

	switch (key[0]) {
	case 'a':
	case 'A':
		if (strcasecmp(key, LAUNCH_JOBKEY_ABANDONPROCESSGROUP) == 0) {
			j->abandon_pg = value;
			found_key = true;
		}
		break;
	case 'k':
	case 'K':
		if (strcasecmp(key, LAUNCH_JOBKEY_KEEPALIVE) == 0) {
			j->ondemand = !value;
			found_key = true;
		}
		break;
	case 'o':
	case 'O':
		if (strcasecmp(key, LAUNCH_JOBKEY_ONDEMAND) == 0) {
			j->ondemand = value;
			found_key = true;
		}
		break;
	case 'd':
	case 'D':
		if (strcasecmp(key, LAUNCH_JOBKEY_DEBUG) == 0) {
			j->debug = value;
			found_key = true;
		} else if (strcasecmp(key, LAUNCH_JOBKEY_DISABLED) == 0) {
			job_assumes(j, !value);
			found_key = true;
		}
		break;
	case 'h':
	case 'H':
		if (strcasecmp(key, LAUNCH_JOBKEY_HOPEFULLYEXITSLAST) == 0) {
			j->hopefully_exits_last = value;
			found_key = true;
		} else if (strcasecmp(key, LAUNCH_JOBKEY_HOPEFULLYEXITSFIRST) == 0) {
			j->hopefully_exits_first = value;
			found_key = true;
		}
		break;
	case 's':
	case 'S':
		if (strcasecmp(key, LAUNCH_JOBKEY_SESSIONCREATE) == 0) {
			j->session_create = value;
			found_key = true;
		} else if (strcasecmp(key, LAUNCH_JOBKEY_STARTONMOUNT) == 0) {
			j->start_on_mount = value;
			found_key = true;
		} else if (strcasecmp(key, LAUNCH_JOBKEY_SERVICEIPC) == 0) {
			/* this only does something on Mac OS X 10.4 "Tiger" */
			found_key = true;
		}
		break;
	case 'l':
	case 'L':
		if (strcasecmp(key, LAUNCH_JOBKEY_LOWPRIORITYIO) == 0) {
			j->low_pri_io = value;
			found_key = true;
		} else if (strcasecmp(key, LAUNCH_JOBKEY_LAUNCHONLYONCE) == 0) {
			j->only_once = value;
			found_key = true;
		}
		break;
	case 'm':
	case 'M':
		if (strcasecmp(key, LAUNCH_JOBKEY_MACHEXCEPTIONHANDLER) == 0) {
			j->internal_exc_handler = value;
			found_key = true;
		}
		break;
	case 'i':
	case 'I':
		if (strcasecmp(key, LAUNCH_JOBKEY_INITGROUPS) == 0) {
			if (getuid() != 0) {
				job_log(j, LOG_WARNING, "Ignored this key: %s", key);
				return;
			}
			j->no_init_groups = !value;
			found_key = true;
		} else if( strcasecmp(key, LAUNCH_JOBKEY_IGNOREPROCESSGROUPATSHUTDOWN) == 0 ) {
			j->ignore_pg_at_shutdown = value;
			found_key = true;
		}
		break;
	case 'r':
	case 'R':
		if (strcasecmp(key, LAUNCH_JOBKEY_RUNATLOAD) == 0) {
			if (value) {
				/* We don't want value == false to change j->start_pending */
				j->start_pending = true;
			}
			found_key = true;
		}
		break;
	case 'e':
	case 'E':
		if (strcasecmp(key, LAUNCH_JOBKEY_ENABLEGLOBBING) == 0) {
			j->globargv = value;
			found_key = true;
		} else if (strcasecmp(key, LAUNCH_JOBKEY_ENABLETRANSACTIONS) == 0) {
			j->kill_via_shmem = value;
			found_key = true;
		} else if (strcasecmp(key, LAUNCH_JOBKEY_ENTERKERNELDEBUGGERBEFOREKILL) == 0) {
			j->debug_before_kill = value;
			found_key = true;
		} else if( strcasecmp(key, LAUNCH_JOBKEY_EMBEDDEDPRIVILEGEDISPENSATION) == 0 ) {
			if( !s_embedded_privileged_job ) {
				j->embedded_special_privileges = value;
				s_embedded_privileged_job = j;
			} else {
				job_log(j, LOG_ERR, "Job tried to claim %s after it has already been claimed.", key);
			}
			found_key = true;
		}
		break;
	case 'w':
	case 'W':
		if (strcasecmp(key, LAUNCH_JOBKEY_WAITFORDEBUGGER) == 0) {
			j->wait4debugger = value;
			found_key = true;
		}
		break;
	default:
		break;
	}

	if (unlikely(!found_key)) {
		job_log(j, LOG_WARNING, "Unknown key for boolean: %s", key);
	}
}

void
job_import_string(job_t j, const char *key, const char *value)
{
	char **where2put = NULL;

	switch (key[0]) {
	case 'm':
	case 'M':
		if (strcasecmp(key, LAUNCH_JOBKEY_MACHEXCEPTIONHANDLER) == 0) {
			where2put = &j->alt_exc_handler;
		}
		break;
	case 'p':
	case 'P':
		if (strcasecmp(key, LAUNCH_JOBKEY_PROGRAM) == 0) {
			return;
		}
		break;
	case 'l':
	case 'L':
		if (strcasecmp(key, LAUNCH_JOBKEY_LABEL) == 0) {
			return;
		} else if (strcasecmp(key, LAUNCH_JOBKEY_LIMITLOADTOHOSTS) == 0) {
			return;
		} else if (strcasecmp(key, LAUNCH_JOBKEY_LIMITLOADFROMHOSTS) == 0) {
			return;
		} else if (strcasecmp(key, LAUNCH_JOBKEY_LIMITLOADTOSESSIONTYPE) == 0) {
			return;
		}
		break;
	case 'r':
	case 'R':
		if (strcasecmp(key, LAUNCH_JOBKEY_ROOTDIRECTORY) == 0) {
			if (getuid() != 0) {
				job_log(j, LOG_WARNING, "Ignored this key: %s", key);
				return;
			}
			where2put = &j->rootdir;
		}
		break;
	case 'w':
	case 'W':
		if (strcasecmp(key, LAUNCH_JOBKEY_WORKINGDIRECTORY) == 0) {
			where2put = &j->workingdir;
		}
		break;
	case 'u':
	case 'U':
		if (strcasecmp(key, LAUNCH_JOBKEY_USERNAME) == 0) {
			if (getuid() != 0) {
				job_log(j, LOG_WARNING, "Ignored this key: %s", key);
				return;
			} else if (strcmp(value, "root") == 0) {
				return;
			}
			where2put = &j->username;
		}
		break;
	case 'g':
	case 'G':
		if (strcasecmp(key, LAUNCH_JOBKEY_GROUPNAME) == 0) {
			if (getuid() != 0) {
				job_log(j, LOG_WARNING, "Ignored this key: %s", key);
				return;
			} else if (strcmp(value, "wheel") == 0) {
				return;
			}
			where2put = &j->groupname;
		}
		break;
	case 's':
	case 'S':
		if (strcasecmp(key, LAUNCH_JOBKEY_STANDARDOUTPATH) == 0) {
			where2put = &j->stdoutpath;
		} else if (strcasecmp(key, LAUNCH_JOBKEY_STANDARDERRORPATH) == 0) {
			where2put = &j->stderrpath;
		} else if (strcasecmp(key, LAUNCH_JOBKEY_STANDARDINPATH) == 0) {
			where2put = &j->stdinpath;
			j->stdin_fd = _fd(open(value, O_RDONLY|O_CREAT|O_NOCTTY|O_NONBLOCK, DEFFILEMODE));
			if (job_assumes(j, j->stdin_fd != -1)) {
				/* open() should not block, but regular IO by the job should */
				job_assumes(j, fcntl(j->stdin_fd, F_SETFL, 0) != -1);
				/* XXX -- EV_CLEAR should make named pipes happy? */
				job_assumes(j, kevent_mod(j->stdin_fd, EVFILT_READ, EV_ADD|EV_CLEAR, 0, 0, j) != -1);
			} else {
				j->stdin_fd = 0;
			}
#if HAVE_SANDBOX
		} else if (strcasecmp(key, LAUNCH_JOBKEY_SANDBOXPROFILE) == 0) {
			where2put = &j->seatbelt_profile;
#endif
		}
		break;
	default:
		job_log(j, LOG_WARNING, "Unknown key for string: %s", key);
		break;
	}

	if (likely(where2put)) {
		job_assumes(j, (*where2put = strdup(value)) != NULL);
	} else {
		/* See rdar://problem/5496612. These two are okay. */
		if( strncmp(key, "SHAuthorizationRight", sizeof("SHAuthorizationRight")) != 0 && strncmp(key, "ServiceDescription", sizeof("ServiceDescription")) != 0 ) {
			job_log(j, LOG_WARNING, "Unknown key: %s", key);
		}
	}
}

void
job_import_integer(job_t j, const char *key, long long value)
{
	switch (key[0]) {
	case 'e':
	case 'E':
		if (strcasecmp(key, LAUNCH_JOBKEY_EXITTIMEOUT) == 0) {
			if (unlikely(value < 0)) {
				job_log(j, LOG_WARNING, "%s less than zero. Ignoring.", LAUNCH_JOBKEY_EXITTIMEOUT);
			} else if (unlikely(value > UINT32_MAX)) {
				job_log(j, LOG_WARNING, "%s is too large. Ignoring.", LAUNCH_JOBKEY_EXITTIMEOUT);
			} else {
				j->exit_timeout = (typeof(j->exit_timeout)) value;
			}
		} else if( strcasecmp(key, LAUNCH_JOBKEY_EMBEDDEDMAINTHREADPRIORITY) == 0 ) {
			j->main_thread_priority = value;
		}
		break;
	case 'j':
	case 'J':
		if( strcasecmp(key, LAUNCH_JOBKEY_JETSAMPRIORITY) == 0 ) {
			job_log(j, LOG_DEBUG, "Importing job with priority: %lld", value);
			j->jetsam_priority = (typeof(j->jetsam_priority))value;
			LIST_INSERT_HEAD(&j->mgr->jetsam_jobs, j, jetsam_sle);
			j->mgr->jetsam_jobs_cnt++;
		}
		break;
	case 'n':
	case 'N':
		if (strcasecmp(key, LAUNCH_JOBKEY_NICE) == 0) {
			if (unlikely(value < PRIO_MIN)) {
				job_log(j, LOG_WARNING, "%s less than %d. Ignoring.", LAUNCH_JOBKEY_NICE, PRIO_MIN);
			} else if (unlikely(value > PRIO_MAX)) {
				job_log(j, LOG_WARNING, "%s is greater than %d. Ignoring.", LAUNCH_JOBKEY_NICE, PRIO_MAX);
			} else {
				j->nice = (typeof(j->nice)) value;
				j->setnice = true;
			}
		}
		break;
	case 't':
	case 'T':
		if (strcasecmp(key, LAUNCH_JOBKEY_TIMEOUT) == 0) {
			if (unlikely(value < 0)) {
				job_log(j, LOG_WARNING, "%s less than zero. Ignoring.", LAUNCH_JOBKEY_TIMEOUT);
			} else if (unlikely(value > UINT32_MAX)) {
				job_log(j, LOG_WARNING, "%s is too large. Ignoring.", LAUNCH_JOBKEY_TIMEOUT);
			} else {
				j->timeout = (typeof(j->timeout)) value;
			}
		} else if (strcasecmp(key, LAUNCH_JOBKEY_THROTTLEINTERVAL) == 0) {
			if (value < 0) {
				job_log(j, LOG_WARNING, "%s less than zero. Ignoring.", LAUNCH_JOBKEY_THROTTLEINTERVAL);
			} else if (value > UINT32_MAX) {
				job_log(j, LOG_WARNING, "%s is too large. Ignoring.", LAUNCH_JOBKEY_THROTTLEINTERVAL);
			} else {
				j->min_run_time = (typeof(j->min_run_time)) value;
			}
		}
		break;
	case 'u':
	case 'U':
		if (strcasecmp(key, LAUNCH_JOBKEY_UMASK) == 0) {
			j->mask = value;
			j->setmask = true;
		}
		break;
	case 's':
	case 'S':
		if (strcasecmp(key, LAUNCH_JOBKEY_STARTINTERVAL) == 0) {
			if (unlikely(value <= 0)) {
				job_log(j, LOG_WARNING, "%s is not greater than zero. Ignoring.", LAUNCH_JOBKEY_STARTINTERVAL);
			} else if (unlikely(value > UINT32_MAX)) {
				job_log(j, LOG_WARNING, "%s is too large. Ignoring.", LAUNCH_JOBKEY_STARTINTERVAL);
			} else {
				runtime_add_weak_ref();
				j->start_interval = (typeof(j->start_interval)) value;

				job_assumes(j, kevent_mod((uintptr_t)&j->start_interval, EVFILT_TIMER, EV_ADD, NOTE_SECONDS, j->start_interval, j) != -1);
			}
#if HAVE_SANDBOX
		} else if (strcasecmp(key, LAUNCH_JOBKEY_SANDBOXFLAGS) == 0) {
			j->seatbelt_flags = value;
#endif
		}

		break;
	default:
		job_log(j, LOG_WARNING, "Unknown key for integer: %s", key);
		break;
	}
}

void
job_import_opaque(job_t j __attribute__((unused)),
	const char *key, launch_data_t value __attribute__((unused)))
{
	switch (key[0]) {
	case 'q':
	case 'Q':
#if HAVE_QUARANTINE
		if (strcasecmp(key, LAUNCH_JOBKEY_QUARANTINEDATA) == 0) {
			size_t tmpsz = launch_data_get_opaque_size(value);

			if (job_assumes(j, j->quarantine_data = malloc(tmpsz))) {
				memcpy(j->quarantine_data, launch_data_get_opaque(value), tmpsz);
				j->quarantine_data_sz = tmpsz;
			}
		}
#endif
	case 's':
	case 'S':
		if( strcasecmp(key, LAUNCH_JOBKEY_SECURITYSESSIONUUID) == 0 ) {
			size_t tmpsz = launch_data_get_opaque_size(value);
			if( job_assumes(j, tmpsz == sizeof(uuid_t)) ) {
				memcpy(j->expected_audit_uuid, launch_data_get_opaque(value), sizeof(uuid_t));
			}
		}
		break;
	default:
		break;
	}
}

static void
policy_setup(launch_data_t obj, const char *key, void *context)
{
	job_t j = context;
	bool found_key = false;

	switch (key[0]) {
	case 'd':
	case 'D':
		if (strcasecmp(key, LAUNCH_JOBPOLICY_DENYCREATINGOTHERJOBS) == 0) {
			j->deny_job_creation = launch_data_get_bool(obj);
			found_key = true;
		}
		break;
	default:
		break;
	}

	if (unlikely(!found_key)) {
		job_log(j, LOG_WARNING, "Unknown policy: %s", key);
	}
}

void
job_import_dictionary(job_t j, const char *key, launch_data_t value)
{
	launch_data_t tmp;

	switch (key[0]) {
	case 'p':
	case 'P':
		if (strcasecmp(key, LAUNCH_JOBKEY_POLICIES) == 0) {
			launch_data_dict_iterate(value, policy_setup, j);
		}
		break;
	case 'k':
	case 'K':
		if (strcasecmp(key, LAUNCH_JOBKEY_KEEPALIVE) == 0) {
			launch_data_dict_iterate(value, semaphoreitem_setup, j);
		}
		break;
	case 'i':
	case 'I':
		if (strcasecmp(key, LAUNCH_JOBKEY_INETDCOMPATIBILITY) == 0) {
			j->inetcompat = true;
			j->abandon_pg = true;
			if ((tmp = launch_data_dict_lookup(value, LAUNCH_JOBINETDCOMPATIBILITY_WAIT))) {
				j->inetcompat_wait = launch_data_get_bool(tmp);
			}
		}
		break;
	case 'e':
	case 'E':
		if (strcasecmp(key, LAUNCH_JOBKEY_ENVIRONMENTVARIABLES) == 0) {
			launch_data_dict_iterate(value, envitem_setup, j);
		}		
		break;
	case 'u':
	case 'U':
		if (strcasecmp(key, LAUNCH_JOBKEY_USERENVIRONMENTVARIABLES) == 0) {
			j->importing_global_env = true;
			launch_data_dict_iterate(value, envitem_setup, j);
			j->importing_global_env = false;
		}
		break;
	case 's':
	case 'S':
		if (strcasecmp(key, LAUNCH_JOBKEY_SOCKETS) == 0) {
			launch_data_dict_iterate(value, socketgroup_setup, j);
		} else if (strcasecmp(key, LAUNCH_JOBKEY_STARTCALENDARINTERVAL) == 0) {
			calendarinterval_new_from_obj(j, value);
		} else if (strcasecmp(key, LAUNCH_JOBKEY_SOFTRESOURCELIMITS) == 0) {
			launch_data_dict_iterate(value, limititem_setup, j);
#if HAVE_SANDBOX
		} else if (strcasecmp(key, LAUNCH_JOBKEY_SANDBOXFLAGS) == 0) {
			launch_data_dict_iterate(value, seatbelt_setup_flags, j);
#endif
		}
		break;
	case 'h':
	case 'H':
		if (strcasecmp(key, LAUNCH_JOBKEY_HARDRESOURCELIMITS) == 0) {
			j->importing_hard_limits = true;
			launch_data_dict_iterate(value, limititem_setup, j);
			j->importing_hard_limits = false;
		}
		break;
	case 'm':
	case 'M':
		if (strcasecmp(key, LAUNCH_JOBKEY_MACHSERVICES) == 0) {
			launch_data_dict_iterate(value, machservice_setup, j);
		}
		break;
	default:
		job_log(j, LOG_WARNING, "Unknown key for dictionary: %s", key);
		break;
	}
}

void
job_import_array(job_t j, const char *key, launch_data_t value)
{
	size_t i, value_cnt = launch_data_array_get_count(value);
	const char *str;

	switch (key[0]) {
	case 'p':
	case 'P':
		if (strcasecmp(key, LAUNCH_JOBKEY_PROGRAMARGUMENTS) == 0) {
			return;
		}
		break;
	case 'l':
	case 'L':
		if (strcasecmp(key, LAUNCH_JOBKEY_LIMITLOADTOHOSTS) == 0) {
			return;
		} else if (strcasecmp(key, LAUNCH_JOBKEY_LIMITLOADFROMHOSTS) == 0) {
			return;
		} else if (strcasecmp(key, LAUNCH_JOBKEY_LIMITLOADTOSESSIONTYPE) == 0) {
			job_log(j, LOG_NOTICE, "launchctl should have transformed the \"%s\" array to a string", LAUNCH_JOBKEY_LIMITLOADTOSESSIONTYPE);
			return;
		}
		break;
	case 'q':
	case 'Q':
		if (strcasecmp(key, LAUNCH_JOBKEY_QUEUEDIRECTORIES) == 0) {
			for (i = 0; i < value_cnt; i++) {
				str = launch_data_get_string(launch_data_array_get_index(value, i));
				if (job_assumes(j, str != NULL)) {
					semaphoreitem_new(j, DIR_NOT_EMPTY, str);
				}
			}

		}
		break;
	case 'w':
	case 'W':
		if (strcasecmp(key, LAUNCH_JOBKEY_WATCHPATHS) == 0) {
			for (i = 0; i < value_cnt; i++) {
				str = launch_data_get_string(launch_data_array_get_index(value, i));
				if (job_assumes(j, str != NULL)) {
					semaphoreitem_new(j, PATH_CHANGES, str);
				}
			}
		}
		break;
	case 'b':
	case 'B':
		if (strcasecmp(key, LAUNCH_JOBKEY_BONJOURFDS) == 0) {
			socketgroup_setup(value, LAUNCH_JOBKEY_BONJOURFDS, j);
		} else if (strcasecmp(key, LAUNCH_JOBKEY_BINARYORDERPREFERENCE) == 0) {
			if (job_assumes(j, j->j_binpref = malloc(value_cnt * sizeof(*j->j_binpref)))) {
				j->j_binpref_cnt = value_cnt;
				for (i = 0; i < value_cnt; i++) {
					j->j_binpref[i] = (cpu_type_t) launch_data_get_integer(launch_data_array_get_index(value, i));
				}
			}
		}
		break;
	case 's':
	case 'S':
		if (strcasecmp(key, LAUNCH_JOBKEY_STARTCALENDARINTERVAL) == 0) {
			for (i = 0; i < value_cnt; i++) {
				calendarinterval_new_from_obj(j, launch_data_array_get_index(value, i));
			}
		}
		break;
	default:
		job_log(j, LOG_WARNING, "Unknown key for array: %s", key);
		break;
	}
}

void
job_import_keys(launch_data_t obj, const char *key, void *context)
{
	job_t j = context;
	launch_data_type_t kind;

	if (!launchd_assumes(obj != NULL)) {
		return;
	}

	kind = launch_data_get_type(obj);

	switch (kind) {
	case LAUNCH_DATA_BOOL:
		job_import_bool(j, key, launch_data_get_bool(obj));
		break;
	case LAUNCH_DATA_STRING:
		job_import_string(j, key, launch_data_get_string(obj));
		break;
	case LAUNCH_DATA_INTEGER:
		job_import_integer(j, key, launch_data_get_integer(obj));
		break;
	case LAUNCH_DATA_DICTIONARY:
		job_import_dictionary(j, key, obj);
		break;
	case LAUNCH_DATA_ARRAY:
		job_import_array(j, key, obj);
		break;
	case LAUNCH_DATA_OPAQUE:
		job_import_opaque(j, key, obj);
		break;
	default:
		job_log(j, LOG_WARNING, "Unknown value type '%d' for key: %s", kind, key);
		break;
	}
}

job_t 
jobmgr_import2(jobmgr_t jm, launch_data_t pload)
{
	launch_data_t tmp, ldpa;
	const char *label = NULL, *prog = NULL;
	const char **argv = NULL;
	job_t j;

	if (!jobmgr_assumes(jm, pload != NULL)) {
		errno = EINVAL;
		return NULL;
	}

	if (unlikely(launch_data_get_type(pload) != LAUNCH_DATA_DICTIONARY)) {
		errno = EINVAL;
		return NULL;
	}

	if (unlikely(!(tmp = launch_data_dict_lookup(pload, LAUNCH_JOBKEY_LABEL)))) {
		errno = EINVAL;
		return NULL;
	}

	if (unlikely(launch_data_get_type(tmp) != LAUNCH_DATA_STRING)) {
		errno = EINVAL;
		return NULL;
	}

	if (unlikely(!(label = launch_data_get_string(tmp)))) {
		errno = EINVAL;
		return NULL;
	}

#if TARGET_OS_EMBEDDED
	if( unlikely(g_embedded_privileged_action && s_embedded_privileged_job) ) {
		if( unlikely(!(tmp = launch_data_dict_lookup(pload, LAUNCH_JOBKEY_USERNAME))) ) {
			errno = EPERM;
			return NULL;
		}
		
		const char *username = NULL;
		if( likely(tmp && launch_data_get_type(tmp) == LAUNCH_DATA_STRING) ) {
			username = launch_data_get_string(tmp);
		} else {
			errno = EPERM;
			return NULL;
		}
		
		if( !jobmgr_assumes(jm, s_embedded_privileged_job->username != NULL && username != NULL) ) {
			errno = EPERM;
			return NULL;
		}
		
		if( unlikely(strcmp(s_embedded_privileged_job->username, username) != 0) ) {
			errno = EPERM;
			return NULL;
		}
	} else if( g_embedded_privileged_action ) {
		errno = EINVAL;
		return NULL;
	}
#endif

	if ((tmp = launch_data_dict_lookup(pload, LAUNCH_JOBKEY_PROGRAM)) &&
			(launch_data_get_type(tmp) == LAUNCH_DATA_STRING)) {
		prog = launch_data_get_string(tmp);
	}

	if ((ldpa = launch_data_dict_lookup(pload, LAUNCH_JOBKEY_PROGRAMARGUMENTS))) {
		size_t i, c;

		if (launch_data_get_type(ldpa) != LAUNCH_DATA_ARRAY) {
			errno = EINVAL;
			return NULL;
		}

		c = launch_data_array_get_count(ldpa);

		argv = alloca((c + 1) * sizeof(char *));

		for (i = 0; i < c; i++) {
			tmp = launch_data_array_get_index(ldpa, i);

			if (launch_data_get_type(tmp) != LAUNCH_DATA_STRING) {
				errno = EINVAL;
				return NULL;
			}

			argv[i] = launch_data_get_string(tmp);
		}

		argv[i] = NULL;
	}

	/* Hack to make sure the proper job manager is set the whole way through. */
	launch_data_t session = launch_data_dict_lookup(pload, LAUNCH_JOBKEY_LIMITLOADTOSESSIONTYPE);
	if( session ) {
		jm = jobmgr_find_by_name(jm, launch_data_get_string(session)) ?: jm;
	}
	
	jobmgr_log(jm, LOG_DEBUG, "Importing %s.", label);
	
	if (unlikely((j = job_find(label)) != NULL)) {
		errno = EEXIST;
		return NULL;
	} else if (unlikely(!jobmgr_label_test(jm, label))) {
		errno = EINVAL;
		return NULL;
	}

	if (likely(j = job_new(jm, label, prog, argv))) {
		launch_data_dict_iterate(pload, job_import_keys, j);
		if( !uuid_is_null(j->expected_audit_uuid) ) {
			uuid_string_t uuid_str;
			uuid_unparse(j->expected_audit_uuid, uuid_str);
			job_log(j, LOG_DEBUG, "Imported job. Waiting for session for UUID %s.", uuid_str);
			LIST_INSERT_HEAD(&s_needing_sessions, j, needing_session_sle);
			errno = ENEEDAUTH;
		} else {
			job_log(j, LOG_DEBUG, "No security session specified.");
			j->audit_session = MACH_PORT_NULL;
		}
	}

	return j;
}

bool
jobmgr_label_test(jobmgr_t jm, const char *str)
{
	char *endstr = NULL;
	const char *ptr;

	if (str[0] == '\0') {
		jobmgr_log(jm, LOG_ERR, "Empty job labels are not allowed");
		return false;
	}

	for (ptr = str; *ptr; ptr++) {
		if (iscntrl(*ptr)) {
			jobmgr_log(jm, LOG_ERR, "ASCII control characters are not allowed in job labels. Index: %td Value: 0x%hhx", ptr - str, *ptr);
			return false;
		}
	}

	strtoll(str, &endstr, 0);

	if (str != endstr) {
		jobmgr_log(jm, LOG_ERR, "Job labels are not allowed to begin with numbers: %s", str);
		return false;
	}

	if ((strncasecmp(str, "com.apple.launchd", strlen("com.apple.launchd")) == 0) ||
			(strncasecmp(str, "com.apple.launchctl", strlen("com.apple.launchctl")) == 0)) {
		jobmgr_log(jm, LOG_ERR, "Job labels are not allowed to use a reserved prefix: %s", str);
		return false;
	}

	return true;
}

job_t 
job_find(const char *label)
{
	job_t ji;

	LIST_FOREACH(ji, &label_hash[hash_label(label)], label_hash_sle) {
		if (unlikely(ji->removal_pending || ji->mgr->shutting_down)) {
			continue; /* 5351245 and 5488633 respectively */
		}

		if (strcmp(ji->label, label) == 0) {
			return ji;
		}
	}

	errno = ESRCH;
	return NULL;
}

/* Should try and consolidate with job_mig_intran2() and jobmgr_find_by_pid(). */
job_t
jobmgr_find_by_pid_deep(jobmgr_t jm, pid_t p, bool anon_okay)
{
	job_t ji = NULL;
	LIST_FOREACH( ji, &jm->active_jobs[ACTIVE_JOB_HASH(p)], pid_hash_sle ) {
		if (ji->p == p && (!ji->anonymous || (ji->anonymous && anon_okay)) ) {
			return ji;
		}
	}

	jobmgr_t jmi = NULL;
	SLIST_FOREACH( jmi, &jm->submgrs, sle ) {
		if( (ji = jobmgr_find_by_pid_deep(jmi, p, anon_okay)) ) {
			break;
		}
	}

	return ji;
}

job_t
jobmgr_find_by_pid(jobmgr_t jm, pid_t p, bool create_anon)
{
	job_t ji;

	LIST_FOREACH(ji, &jm->active_jobs[ACTIVE_JOB_HASH(p)], pid_hash_sle) {
		if (ji->p == p) {
			return ji;
		}
	}

	return create_anon ? job_new_anonymous(jm, p) : NULL;
}

job_t 
job_mig_intran2(jobmgr_t jm, mach_port_t mport, pid_t upid)
{
	jobmgr_t jmi;
	job_t ji;

	if (jm->jm_port == mport) {
		return jobmgr_find_by_pid(jm, upid, true);
	}

	SLIST_FOREACH(jmi, &jm->submgrs, sle) {
		job_t jr;

		if ((jr = job_mig_intran2(jmi, mport, upid))) {
			return jr;
		}
	}

	LIST_FOREACH(ji, &jm->jobs, sle) {
		if (ji->j_port == mport) {
			return ji;
		}
	}

	return NULL;
}

job_t 
job_mig_intran(mach_port_t p)
{
	struct ldcred *ldc = runtime_get_caller_creds();
	job_t jr;

	jr = job_mig_intran2(root_jobmgr, p, ldc->pid);

	if (!jobmgr_assumes(root_jobmgr, jr != NULL)) {
		int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, 0 };
		struct kinfo_proc kp;
		size_t len = sizeof(kp);

		mib[3] = ldc->pid;

		if (jobmgr_assumes(root_jobmgr, sysctl(mib, 4, &kp, &len, NULL, 0) != -1)
				&& jobmgr_assumes(root_jobmgr, len == sizeof(kp))) {
			jobmgr_log(root_jobmgr, LOG_ERR, "%s() was confused by PID %u UID %u EUID %u Mach Port 0x%x: %s", __func__, ldc->pid, ldc->uid, ldc->euid, p, kp.kp_proc.p_comm);
		}
	}

	return jr;
}

job_t
job_find_by_service_port(mach_port_t p)
{
	struct machservice *ms;

	LIST_FOREACH(ms, &port_hash[HASH_PORT(p)], port_hash_sle) {
		if (ms->recv && (ms->port == p)) {
			return ms->job;
		}
	}

	return NULL;
}

void
job_mig_destructor(job_t j)
{
	/*
	 * 5477111
	 *
	 * 'j' can be invalid at this point. We should fix this up after Leopard ships.
	 */

	if (unlikely(j && (j != workaround_5477111) && j->unload_at_mig_return)) {
		job_log(j, LOG_NOTICE, "Unloading PID %u at MIG return.", j->p);
		job_remove(j);
	}

	workaround_5477111 = NULL;

	calendarinterval_sanity_check();
}

void
job_export_all2(jobmgr_t jm, launch_data_t where)
{
	jobmgr_t jmi;
	job_t ji;

	SLIST_FOREACH(jmi, &jm->submgrs, sle) {
		job_export_all2(jmi, where);
	}

	LIST_FOREACH(ji, &jm->jobs, sle) {
		launch_data_t tmp;

		if (jobmgr_assumes(jm, (tmp = job_export(ji)) != NULL)) {
			launch_data_dict_insert(where, tmp, ji->label);
		}
	}
}

launch_data_t
job_export_all(void)
{
	launch_data_t resp = launch_data_alloc(LAUNCH_DATA_DICTIONARY);

	if (launchd_assumes(resp != NULL)) {
		job_export_all2(root_jobmgr, resp);
	}

	return resp;
}

void
job_log_stray_pg(job_t j)
{
	int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PGRP, j->p };
	size_t i, kp_cnt, len = sizeof(struct kinfo_proc) * get_kern_max_proc();
	struct kinfo_proc *kp;

	if (!do_apple_internal_logging) {
		return;
	}

	runtime_ktrace(RTKT_LAUNCHD_FINDING_STRAY_PG, j->p, 0, 0);

	if (!job_assumes(j, (kp = malloc(len)) != NULL)) {
		return;
	}
	if (!job_assumes(j, sysctl(mib, 4, kp, &len, NULL, 0) != -1)) {
		goto out;
	}

	kp_cnt = len / sizeof(struct kinfo_proc);

	for (i = 0; i < kp_cnt; i++) {
		pid_t p_i = kp[i].kp_proc.p_pid;
		pid_t pp_i = kp[i].kp_eproc.e_ppid;
		const char *z = (kp[i].kp_proc.p_stat == SZOMB) ? "zombie " : "";
		const char *n = kp[i].kp_proc.p_comm;

		if (p_i == j->p) {
			continue;
		} else if (!job_assumes(j, p_i != 0 && p_i != 1)) {
			continue;
		}

		job_log(j, LOG_WARNING, "Stray %sprocess with PGID equal to this dead job: PID %u PPID %u %s", z, p_i, pp_i, n);
	}

out:
	free(kp);
}

void
job_reap(job_t j)
{
	struct rusage ru;
	int status;

	bool is_system_bootstrapper = j->is_bootstrapper && pid1_magic && !j->mgr->parentmgr;
	
	job_log(j, LOG_DEBUG, "Reaping");

	if (j->shmem) {
		job_assumes(j, vm_deallocate(mach_task_self(), (vm_address_t)j->shmem, getpagesize()) == 0);
		j->shmem = NULL;
	}

	if (unlikely(j->weird_bootstrap)) {
		int64_t junk = 0;
		job_mig_swap_integer(j, VPROC_GSK_WEIRD_BOOTSTRAP, 0, 0, &junk);
	}

	if (j->log_redirect_fd && !j->legacy_LS_job) {
		job_log_stdouterr(j); /* one last chance */

		if (j->log_redirect_fd) {
			job_assumes(j, runtime_close(j->log_redirect_fd) != -1);
			j->log_redirect_fd = 0;
		}
	}

	if (j->fork_fd) {
		job_assumes(j, runtime_close(j->fork_fd) != -1);
		j->fork_fd = 0;
	}

	if (j->anonymous) {
		status = 0;
		memset(&ru, 0, sizeof(ru));
	} else {
		/*
		 * The job is dead. While the PID/PGID is still known to be
		 * valid, try to kill abandoned descendant processes.
		 */
		job_log_stray_pg(j);
		if (!j->abandon_pg) {
			if (unlikely(runtime_killpg(j->p, SIGTERM) == -1 && errno != ESRCH)) {
#ifdef __LP64__
				job_log(j, LOG_APPLEONLY, "Bug: 5487498");
#else
				job_assumes(j, false);
#endif
			}
		}
		
		/*
		 * 5020256
		 *
		 * The current implementation of ptrace() causes the traced process to
		 * be abducted away from the true parent and adopted by the tracer.
		 *
		 * Once the tracing process relinquishes control, the kernel then
		 * restores the true parent/child relationship.
		 *
		 * Unfortunately, the wait*() family of APIs is unaware of the temporarily
		 * data structures changes, and they return an error if reality hasn't
		 * been restored by the time they are called.
		 */
		if (!job_assumes(j, wait4(j->p, &status, 0, &ru) != -1)) {
			job_log(j, LOG_NOTICE, "Working around 5020256. Assuming the job crashed.");

			status = W_EXITCODE(0, SIGSEGV);
			memset(&ru, 0, sizeof(ru));
		}
	}

	if (j->exit_timeout) {
		kevent_mod((uintptr_t)&j->exit_timeout, EVFILT_TIMER, EV_DELETE, 0, 0, NULL);
	}
	
	LIST_REMOVE(j, pid_hash_sle);

	if (j->wait_reply_port) {
		job_log(j, LOG_DEBUG, "MPM wait reply being sent");
		job_assumes(j, job_mig_wait_reply(j->wait_reply_port, 0, status) == 0);
		j->wait_reply_port = MACH_PORT_NULL;
	}

	if( j->pending_sample ) {
		job_log(j, LOG_DEBUG | LOG_CONSOLE, "Job exited before we could sample it.");
		STAILQ_REMOVE(&j->mgr->pending_samples, j, job_s, pending_samples_sle);
		j->pending_sample = false;
	}

	if (j->sent_signal_time) {
		uint64_t td_sec, td_usec, td = runtime_get_nanoseconds_since(j->sent_signal_time);

		td_sec = td / NSEC_PER_SEC;
		td_usec = (td % NSEC_PER_SEC) / NSEC_PER_USEC;

		job_log(j, LOG_DEBUG, "Exited %llu.%06llu seconds after the first signal was sent", td_sec, td_usec);
	}

	timeradd(&ru.ru_utime, &j->ru.ru_utime, &j->ru.ru_utime);
	timeradd(&ru.ru_stime, &j->ru.ru_stime, &j->ru.ru_stime);
	j->ru.ru_maxrss += ru.ru_maxrss;
	j->ru.ru_ixrss += ru.ru_ixrss;
	j->ru.ru_idrss += ru.ru_idrss;
	j->ru.ru_isrss += ru.ru_isrss;
	j->ru.ru_minflt += ru.ru_minflt;
	j->ru.ru_majflt += ru.ru_majflt;
	j->ru.ru_nswap += ru.ru_nswap;
	j->ru.ru_inblock += ru.ru_inblock;
	j->ru.ru_oublock += ru.ru_oublock;
	j->ru.ru_msgsnd += ru.ru_msgsnd;
	j->ru.ru_msgrcv += ru.ru_msgrcv;
	j->ru.ru_nsignals += ru.ru_nsignals;
	j->ru.ru_nvcsw += ru.ru_nvcsw;
	j->ru.ru_nivcsw += ru.ru_nivcsw;

	if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
		job_log(j, LOG_WARNING, "Exited with exit code: %d", WEXITSTATUS(status));
	}

	if (WIFSIGNALED(status)) {
		int s = WTERMSIG(status);
		if ((SIGKILL == s || SIGTERM == s) && !j->stopped) {
			job_log(j, LOG_NOTICE, "Exited: %s", strsignal(s));
		} else if( !j->stopped && !j->clean_kill ) {			
			switch( s ) {
				/* Signals which indicate a crash. */
				case SIGILL		:
				case SIGABRT	:
				case SIGFPE		:
				case SIGBUS		:
				case SIGSEGV	:
				case SIGSYS		:
				/* If the kernel has posted NOTE_EXIT and the signal sent to the process was
				 * SIGTRAP, assume that it's a crash.
				 */
				case SIGTRAP	:
					j->crashed = true;
					job_log(j, LOG_WARNING, "Job appears to have crashed: %s", strsignal(s));
					break;
				default			:
					job_log(j, LOG_WARNING, "Exited abnormally: %s", strsignal(s));
					break;
			}
			
			if( is_system_bootstrapper && j->crashed ) {
				job_log(j, LOG_ERR | LOG_CONSOLE, "The %s bootstrapper has crashed: %s", j->mgr->name, strsignal(s));
			}
		}
	}

	j->reaped = true;
	
	struct machservice *msi = NULL;
	if( j->crashed || !(j->did_exec || j->anonymous) ) {
		SLIST_FOREACH( msi, &j->machservices, sle ) {
			if( j->crashed && !msi->isActive && (msi->drain_one_on_crash || msi->drain_all_on_crash) ) {
				machservice_drain_port(msi);
			}
			
			if( !j->did_exec && msi->reset && job_assumes(j, !msi->isActive) ) {
				machservice_resetport(j, msi);
			}
		}
	}
	
	struct suspended_peruser *spi = NULL;
	while( (spi = LIST_FIRST(&j->suspended_perusers)) ) {
		job_log(j, LOG_ERR, "Job exited before resuming per-user launchd for UID %u. Will forcibly resume.", spi->j->mach_uid);
		spi->j->peruser_suspend_count--;
		if( spi->j->peruser_suspend_count == 0 ) {
			job_dispatch(spi->j, false);
		}
		LIST_REMOVE(spi, sle);
		free(spi);
	}
	
	struct waiting_for_exit *w4e = NULL;
	while( (w4e = LIST_FIRST(&j->exit_watchers)) ) {
		waiting4exit_delete(j, w4e);
	}
	
	if (j->anonymous) {
		total_anon_children--;
		if( j->migratory ) {
			runtime_del_ref();
		}
	} else {
		runtime_del_ref();
		total_children--;
	}
	
	if( j->has_console ) {
		g_wsp = 0;
	}
	
	if (j->hopefully_exits_first) {
		j->mgr->hopefully_first_cnt--;
	} else if (!j->anonymous && !j->hopefully_exits_last) {
		j->mgr->normal_active_cnt--;
	}
	j->last_exit_status = status;
	j->sent_signal_time = 0;
	j->sent_sigkill = false;
	j->clean_kill = false;
	j->sampling_complete = false;
	j->sent_kill_via_shmem = false;
	j->lastlookup = NULL;
	j->lastlookup_gennum = 0;
	j->p = 0;

	/*
	 * We need to someday evaluate other jobs and find those who wish to track the
	 * active/inactive state of this job. The current job_dispatch() logic makes
	 * this messy, given that jobs can be deleted at dispatch.
	 */
}

void
jobmgr_dispatch_all(jobmgr_t jm, bool newmounthack)
{
	jobmgr_t jmi, jmn;
	job_t ji, jn;

	if (jm->shutting_down) {
		return;
	}

	SLIST_FOREACH_SAFE(jmi, &jm->submgrs, sle, jmn) {
		jobmgr_dispatch_all(jmi, newmounthack);
	}

	LIST_FOREACH_SAFE(ji, &jm->jobs, sle, jn) {
		if (newmounthack && ji->start_on_mount) {
			ji->start_pending = true;
		}

		job_dispatch(ji, false);
	}
}

pid_t
basic_spawn(job_t j, void (*what_to_do)(job_t))
{
	pid_t p = 0;
	thread_state_flavor_t f = 0;
#if defined (__ppc__) || defined(__ppc64__)
	f = PPC_THREAD_STATE64;
#elif defined(__i386__) || defined(__x86_64__)
	f = x86_THREAD_STATE;
#elif defined(__arm__)
	f = ARM_THREAD_STATE;
#else
	#error "unknown architecture"
#endif
	
	int execpair[2] = { 0, 0 };
	job_assumes(j, socketpair(AF_UNIX, SOCK_STREAM, 0, execpair) != -1);
	
	switch( (p = fork()) ) {
		case 0	:
			job_assumes(j, runtime_close(execpair[0]) != -1);
			/* Wait for the parent to attach a kevent. */
			read(_fd(execpair[1]), &p, sizeof(p));
			what_to_do(j);
			_exit(EXIT_FAILURE);
		case -1	:
			job_assumes(j, runtime_close(execpair[0]) != -1);
			job_assumes(j, runtime_close(execpair[1]) != -1);
			execpair[0] = -1;
			execpair[1] = -1;
			job_log(j, LOG_NOTICE | LOG_CONSOLE, "fork(2) failed: %d", errno);
			break;
		default	:
			job_assumes(j, runtime_close(execpair[1]) != -1);
			execpair[1] = -1;
			break;
	}
	
	int r = -1;
	if( p != -1 ) {
		/* Let us know when sample is done. ONESHOT is implicit if we're just interested in NOTE_EXIT. */
		if( job_assumes(j, (r = kevent_mod(p, EVFILT_PROC, EV_ADD, NOTE_EXIT, 0, j)) != -1) ) {
			if( !job_assumes(j, write(execpair[0], &p, sizeof(p)) == sizeof(p)) ) {
				job_assumes(j, kevent_mod(p, EVFILT_PROC, EV_DELETE, 0, 0, NULL) != -1);
				job_assumes(j, runtime_kill(p, SIGKILL) != -1);
				r = -1;
				p = -1;
			}
		} else {
			job_assumes(j, runtime_kill(p, SIGKILL) != -1);
		}
		
		int status = 0;
		if( r == -1 ) {
			job_assumes(j, waitpid(p, &status, WNOHANG) != -1);
		}
	}
	
	if( execpair[0] != -1 ) {
		job_assumes(j, runtime_close(execpair[0]) != -1);
	}
	
	if( execpair[1] != -1 ) {
		job_assumes(j, runtime_close(execpair[0]) != -1);
	}
	
	return p;
}

void 
take_sample(job_t j)
{
	char pidstr[32];
	snprintf(pidstr, sizeof(pidstr), "%u", j->p);
#if !TARGET_OS_EMBEDDED
	/* -nodsyms so sample doesn't try to use Spotlight to find dsym files after mds has gone away. */
	char *sample_args[] = { "/usr/bin/sample", pidstr, "1", "-unsupportedShowArch", "-mayDie", "-nodsyms", "-file", j->mgr->sample_log_file, NULL };
#else
	char *sample_args[] = { "/usr/bin/sample", pidstr, "1", "-unsupportedShowArch", "-mayDie", "-file", j->mgr->sample_log_file, NULL };
#endif
	
	execve(sample_args[0], sample_args, environ);
	_exit(EXIT_FAILURE);
}

void 
jobmgr_dequeue_next_sample(jobmgr_t jm)
{
	if( STAILQ_EMPTY(&jm->pending_samples) ) {
		jobmgr_log(jm, LOG_DEBUG | LOG_CONSOLE, "Sample queue is empty.");
		return;
	}
	
	/* Dequeue the next in line. */
	job_t j = STAILQ_FIRST(&jm->pending_samples);
	if( j->is_being_sampled ) {
		job_log(j, LOG_DEBUG | LOG_CONSOLE, "Sampling is in progress. Not dequeuing next job.");
		return;
	}

	if( !job_assumes(j, !j->sampling_complete) ) {
		return;
	}

	if (!job_assumes(j, do_apple_internal_logging)) {
		return;
	}

	if (!job_assumes(j, mkdir(SHUTDOWN_LOG_DIR, S_IRWXU) != -1 || errno == EEXIST)) {
		return;
	}

	char pidstr[32];
	snprintf(pidstr, sizeof(pidstr), "%u", j->p);
	snprintf(j->mgr->sample_log_file, sizeof(j->mgr->sample_log_file), SHUTDOWN_LOG_DIR "/%s-%u.sample.txt", j->label, j->p);
	
	if (job_assumes(j, unlink(jm->sample_log_file) != -1 || errno == ENOENT)) {
		pid_t sp = basic_spawn(j, take_sample);
		
		if( sp == -1 ) {
			job_log(j, LOG_ERR | LOG_CONSOLE, "Sampling for job failed!");
			STAILQ_REMOVE(&jm->pending_samples, j, job_s, pending_samples_sle);
			j->sampling_complete = true;
			jobmgr_dequeue_next_sample(jm);
		} else {
			j->tracing_pid = sp;
			j->is_being_sampled = true;
			job_log(j, LOG_DEBUG | LOG_CONSOLE, "Sampling job (sample PID: %i, file: %s).", sp, j->mgr->sample_log_file);
		}
	} else {
		STAILQ_REMOVE(&jm->pending_samples, j, job_s, pending_samples_sle);
		j->sampling_complete = true;
	}
	
	j->pending_sample = false;
}

void
job_dispatch_curious_jobs(job_t j)
{	
	job_t ji = NULL, jt = NULL;
	SLIST_FOREACH_SAFE( ji, &s_curious_jobs, curious_jobs_sle, jt ) {
		struct semaphoreitem *si = NULL;
		SLIST_FOREACH( si, &ji->semaphores, sle ) {			
			if( !(si->why == OTHER_JOB_ENABLED || si->why == OTHER_JOB_DISABLED) ) {
				continue;
			}
			
			if( strncmp(si->what, j->label, strlen(j->label)) == 0 ) {
				job_log(ji, LOG_DEBUG, "Dispatching out of interest in \"%s\".", j->label);
				
				job_dispatch(ji, false);
				/* ji could be removed here, so don't do anything with it or its semaphores
				 * after this point.
				 */
				break;
			}
		}
	}
}

job_t
job_dispatch(job_t j, bool kickstart)
{
	/* Don't dispatch a job if it has no audit session set. */
	if( !uuid_is_null(j->expected_audit_uuid) ) {
		return NULL;
	}

#if TARGET_OS_EMBEDDED
	if( g_embedded_privileged_action && s_embedded_privileged_job ) {
		if( !job_assumes(j, s_embedded_privileged_job->username != NULL && j->username != NULL) ) {
			errno = EPERM;
			return NULL;
		}
	
		if( strcmp(j->username, s_embedded_privileged_job->username) != 0 ) {
			errno = EPERM;
			return NULL;
		}
	} else if( g_embedded_privileged_action ) {
		errno = EINVAL;
		return NULL;
	}
#endif

	/*
	 * The whole job removal logic needs to be consolidated. The fact that
	 * a job can be removed from just about anywhere makes it easy to have
	 * stale pointers left behind somewhere on the stack that might get
	 * used after the deallocation. In particular, during job iteration.
	 *
	 * This is a classic example. The act of dispatching a job may delete it.
	 */	
	if (!job_active(j)) {
		if (job_useless(j)) {
			job_remove(j);
			return NULL;
		}
		if( unlikely(j->per_user && j->peruser_suspend_count > 0) ) {
			return NULL;
		}
		
		if (kickstart || job_keepalive(j)) {
			job_log(j, LOG_DEBUG, "Starting job (kickstart = %s)", kickstart ? "true" : "false");
			job_start(j);
		} else {
			job_log(j, LOG_DEBUG, "Watching job (kickstart = %s)", kickstart ? "true" : "false");
			job_watch(j);

			/*
			 * 5455720
			 *
			 * Path checking and monitoring is really racy right now.
			 * We should clean this up post Leopard.
			 */
			if (job_keepalive(j)) {
				job_start(j);
			}
		}
	} else {
		job_log(j, LOG_DEBUG, "Tried to dispatch an already active job (%s).", job_active(j));
	}

	return j;
}

void
job_log_stdouterr2(job_t j, const char *msg, ...)
{
	struct runtime_syslog_attr attr = { j->label, j->label, j->mgr->name, LOG_NOTICE, getuid(), j->p, j->p };
	va_list ap;

	va_start(ap, msg);
	runtime_vsyslog(&attr, msg, ap);
	va_end(ap);
}

void
job_log_stdouterr(job_t j)
{
	char *msg, *bufindex, *buf = malloc(BIG_PIPE_SIZE + 1);
	bool close_log_redir = false;
	ssize_t rsz;

	if (!job_assumes(j, buf != NULL)) {
		return;
	}

	bufindex = buf;

	rsz = read(j->log_redirect_fd, buf, BIG_PIPE_SIZE);

	if (unlikely(rsz == 0)) {
		job_log(j, LOG_DEBUG, "Standard out/error pipe closed");
		close_log_redir = true;
	} else if (rsz == -1) {
		if( !job_assumes(j, errno == EAGAIN) ) {
			close_log_redir = true;
		}
	} else {
		buf[rsz] = '\0';

		while ((msg = strsep(&bufindex, "\n\r"))) {
			if (msg[0]) {
				job_log_stdouterr2(j, "%s", msg);
			}
		}
	}

	free(buf);

	if (unlikely(close_log_redir)) {
		job_assumes(j, runtime_close(j->log_redirect_fd) != -1);
		j->log_redirect_fd = 0;
		job_dispatch(j, false);
	}
}

void
job_kill(job_t j)
{
	if (unlikely(!j->p || j->anonymous)) {
		return;
	}

	job_assumes(j, runtime_kill(j->p, SIGKILL) != -1);

	j->sent_sigkill = true;

	intptr_t timer = j->clean_kill ? LAUNCHD_CLEAN_KILL_TIMER : LAUNCHD_SIGKILL_TIMER;
	job_assumes(j, kevent_mod((uintptr_t)&j->exit_timeout, EVFILT_TIMER, EV_ADD, NOTE_SECONDS, timer, j) != -1);

	job_log(j, LOG_DEBUG, "Sent SIGKILL signal");
}

void
job_log_children_without_exec(job_t j)
{
	/* <rdar://problem/5701343> ER: Add a KERN_PROC_PPID sysctl */
#ifdef KERN_PROC_PPID
	int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PPID, j->p };
#else
	int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL };
#endif
	size_t mib_sz = sizeof(mib) / sizeof(mib[0]);
	size_t i, kp_cnt, len = sizeof(struct kinfo_proc) * get_kern_max_proc();
	struct kinfo_proc *kp;

	if (!do_apple_internal_logging || j->anonymous || j->per_user) {
		return;
	}

	if (!job_assumes(j, (kp = malloc(len)) != NULL)) {
		return;
	}
	if (!job_assumes(j, sysctl(mib, (u_int) mib_sz, kp, &len, NULL, 0) != -1)) {
		goto out;
	}

	kp_cnt = len / sizeof(struct kinfo_proc);

	for (i = 0; i < kp_cnt; i++) {
#ifndef KERN_PROC_PPID
		if (kp[i].kp_eproc.e_ppid != j->p) {
			continue;
		}
#endif
		if (kp[i].kp_proc.p_flag & P_EXEC) {
			continue;
		}

		job_log(j, LOG_DEBUG, "Called *fork(). Please switch to posix_spawn*(), pthreads or launchd. Child PID %u",
				kp[i].kp_proc.p_pid);
	}

out:
	free(kp);
}

void
job_cleanup_after_tracer(job_t j)
{
	jobmgr_t jm = NULL;
	if( j->is_being_sampled ) {
		int wstatus = 0;
		job_log(j, LOG_DEBUG | LOG_CONSOLE, "sample[%i] finished with job.", j->tracing_pid);
		if( job_assumes(j, waitpid(j->tracing_pid, &wstatus, 0) != -1) ) {
			job_assumes(j, WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0);
		}
		STAILQ_REMOVE(&j->mgr->pending_samples, j, job_s, pending_samples_sle);
		
		if( j->kill_after_sample ) {
			if (unlikely(j->debug_before_kill)) {
				job_log(j, LOG_NOTICE, "Exit timeout elapsed. Entering the kernel debugger");
				job_assumes(j, host_reboot(mach_host_self(), HOST_REBOOT_DEBUGGER) == KERN_SUCCESS);
			}
			
			job_log(j, LOG_NOTICE, "Killing...");
			job_kill(j);
		}
		j->sampling_complete = true;
		j->is_being_sampled = false;
		jm = j->mgr;
	}

	j->tracing_pid = 0;
	if( j->reap_after_trace ) {
		job_log(j, LOG_DEBUG | LOG_CONSOLE, "Reaping job now that attached tracer is gone.");
		struct kevent kev;
		EV_SET(&kev, j->p, 0, 0, NOTE_EXIT, 0, 0);
		
		/* Fake a kevent to keep our logic consistent. */
		job_callback_proc(j, &kev);
		
		/* Normally, after getting a EVFILT_PROC event, we do garbage collection
		 * on the root job manager. To make our fakery complete, we will do garbage
		 * collection at the beginning of the next run loop cycle (after we're done
		 * draining the current queue of kevents).
		 */
		job_assumes(j, kevent_mod((uintptr_t)&root_jobmgr->reboot_flags, EVFILT_TIMER, EV_ADD | EV_ONESHOT, NOTE_NSECONDS, 1, root_jobmgr) != -1);
	}
	
	if( jm ) {
		jobmgr_dequeue_next_sample(jm);
	}
}

void
job_callback_proc(job_t j, struct kevent *kev)
{
	bool program_changed = false;
	int fflags = kev->fflags;
	
	job_log(j, LOG_DEBUG, "EVFILT_PROC event for job:");
	log_kevent_struct(LOG_DEBUG, kev, 0);
	
	if( fflags & NOTE_EXIT ) {
		if( j->p == (pid_t)kev->ident && !j->anonymous && !j->is_being_sampled ) {
			int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, j->p };
			struct kinfo_proc kp;
			size_t len = sizeof(kp);
			
			/* Sometimes, the kernel says it succeeded but really didn't. */
			if( job_assumes(j, sysctl(mib, 4, &kp, &len, NULL, 0) != -1) && len == sizeof(kp) ) {
				if( !job_assumes(j, kp.kp_eproc.e_ppid == getpid()) ) {
					/* Someone has attached to the process with ptrace(). There's a race here. 
					 * If we determine that we are not the parent process and then fail to attach 
					 * a kevent to the parent PID (who is probably using ptrace()), we can take that as an
					 * indication that the parent exited between sysctl(3) and kevent_mod(). The
					 * reparenting of the PID should be atomic to us, so in that case, we reap the
					 * job as normal.
					 *
					 * Otherwise, we wait for the death of the parent tracer and then reap, just as we
					 * would if a job died while we were sampling it at shutdown.
					 */
					if( job_assumes(j, kevent_mod(kp.kp_eproc.e_ppid, EVFILT_PROC, EV_ADD, NOTE_EXIT, 0, j) != -1) ) {
						j->tracing_pid = kp.kp_eproc.e_ppid;
						j->reap_after_trace = true;
						return;
					}
				}
			}
		} else if( !j->anonymous ) {
			if( j->tracing_pid == (pid_t)kev->ident ) {
				job_cleanup_after_tracer(j);
				
				return;
			} else if( j->tracing_pid && !j->reap_after_trace ) {
				/* The job exited before our sample completed. */
				job_log(j, LOG_DEBUG | LOG_CONSOLE, "Job has exited. Will reap after tracing PID %i exits.", j->tracing_pid);
				j->reap_after_trace = true;
				return;
			}
		}
	}
		
	if (fflags & NOTE_EXEC) {
		program_changed = true;

		if (j->anonymous) {
			int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, j->p };
			struct kinfo_proc kp;
			size_t len = sizeof(kp);

			/* Sometimes, the kernel says it succeeded but really didn't. */
			if (job_assumes(j, sysctl(mib, 4, &kp, &len, NULL, 0) != -1) && len == sizeof(kp)) {
				char newlabel[1000];

				snprintf(newlabel, sizeof(newlabel), "%p.anonymous.%s", j, kp.kp_proc.p_comm);

				job_log(j, LOG_INFO, "Program changed. Updating the label to: %s", newlabel);
				j->lastlookup = NULL;
				j->lastlookup_gennum = 0;

				LIST_REMOVE(j, label_hash_sle);
				strcpy((char *)j->label, newlabel);
				LIST_INSERT_HEAD(&label_hash[hash_label(j->label)], j, label_hash_sle);
			}
		} else {
			j->did_exec = true;
			job_log(j, LOG_DEBUG, "Program changed");
		}
	}

	if (fflags & NOTE_FORK) {
		job_log(j, LOG_DEBUG, "fork()ed%s", program_changed ? ". For this message only: We don't know whether this event happened before or after execve()." : "");
		job_log_children_without_exec(j);
	}

	if (fflags & NOTE_EXIT) {
		job_reap(j);

		if( !j->anonymous ) {
			j = job_dispatch(j, false);
		} else {
			job_remove(j);
			j = NULL;
		}
	}

	if (j && (fflags & NOTE_REAP)) {
		job_assumes(j, j->p == 0);
	}
}

void
job_callback_timer(job_t j, void *ident)
{
	if (j == ident) {
		job_log(j, LOG_DEBUG, "j == ident (%p)", ident);
		job_dispatch(j, true);
	} else if (&j->semaphores == ident) {
		job_log(j, LOG_DEBUG, "&j->semaphores == ident (%p)", ident);
		job_dispatch(j, false);
	} else if (&j->start_interval == ident) {
		job_log(j, LOG_DEBUG, "&j->start_interval == ident (%p)", ident);
		j->start_pending = true;
		job_dispatch(j, false);
	} else if (&j->exit_timeout == ident) {
		if( !job_assumes(j, j->p != 0) ) {
			return;
		}
		
		if( j->clean_kill ) {
			job_log(j, LOG_ERR | LOG_CONSOLE, "Clean job failed to exit %u second after receiving SIGKILL.", LAUNCHD_CLEAN_KILL_TIMER);
			job_assumes(j, kevent_mod((uintptr_t)&j->exit_timeout, EVFILT_TIMER, EV_DELETE, 0, 0, NULL));
			j->clean_exit_timer_expired = true;
			
			jobmgr_do_garbage_collection(j->mgr);
			return;
		}
		
		/*
		 * This block might be executed up to 3 times for a given (slow) job
		 *  - once for the SAMPLE_TIMEOUT timer, at which point sampling is triggered
		 *  - once for the exit_timeout timer, at which point:
		 *          - sampling is performed if not triggered previously
		 *          - SIGKILL is being sent to the job
		 *  - once for the SIGKILL_TIMER timer, at which point we log an issue
		 *    with the long SIGKILL
		 */
		
		if( j->per_user ) {
			/* Don't sample per-user launchd's. */
			j->sampling_complete = true;
		}
		bool was_is_or_will_be_sampled = ( j->sampling_complete || j->is_being_sampled || j->pending_sample );
		bool should_enqueue = ( !was_is_or_will_be_sampled && do_apple_internal_logging );
		
		if (j->sent_sigkill) {
			uint64_t td = runtime_get_nanoseconds_since(j->sent_signal_time);

			td /= NSEC_PER_SEC;
			td -= j->clean_kill ? 0 : j->exit_timeout;

			job_log(j, LOG_WARNING | LOG_CONSOLE, "Did not die after sending SIGKILL %llu seconds ago...", td);
		} else if( should_enqueue && (!j->exit_timeout || (LAUNCHD_SAMPLE_TIMEOUT < j->exit_timeout)) ) {
			/* This should work even if the job changes its exit_timeout midstream */
			job_log(j, LOG_NOTICE | LOG_CONSOLE, "Sampling timeout elapsed (%u seconds). Scheduling a sample...", LAUNCHD_SAMPLE_TIMEOUT);
			if (j->exit_timeout) {
				unsigned int ttk = (j->exit_timeout - LAUNCHD_SAMPLE_TIMEOUT);
				job_assumes(j, kevent_mod((uintptr_t)&j->exit_timeout, EVFILT_TIMER,
										  EV_ADD|EV_ONESHOT, NOTE_SECONDS, ttk, j) != -1);
				job_log(j, LOG_NOTICE | LOG_CONSOLE, "Scheduled new exit timeout for %u seconds later", ttk);
			}
			
			STAILQ_INSERT_TAIL(&j->mgr->pending_samples, j, pending_samples_sle);
			j->pending_sample = true;
			jobmgr_dequeue_next_sample(j->mgr);
		} else {
			if( do_apple_internal_logging && !j->sampling_complete ) {
				if( j->is_being_sampled || j->pending_sample ) {
					char pidstr[24] = { 0 };
					snprintf(pidstr, sizeof(pidstr), "[%i] ", j->tracing_pid);
					
					job_log(j, LOG_DEBUG | LOG_CONSOLE, "Exit timeout elapsed (%u seconds). Will kill after sample%shas completed.", j->exit_timeout, j->tracing_pid ? pidstr : " ");
					j->kill_after_sample = true;
				} else {
					job_log(j, LOG_DEBUG | LOG_CONSOLE, "Exit timeout elapsed (%u seconds). Will sample and then kill.", j->exit_timeout);
					
					STAILQ_INSERT_TAIL(&j->mgr->pending_samples, j, pending_samples_sle);
					j->pending_sample = true;
				}			

				jobmgr_dequeue_next_sample(j->mgr);
			} else {
				if (unlikely(j->debug_before_kill)) {
					job_log(j, LOG_NOTICE, "Exit timeout elapsed. Entering the kernel debugger");
					job_assumes(j, host_reboot(mach_host_self(), HOST_REBOOT_DEBUGGER) == KERN_SUCCESS);
				}
				job_log(j, LOG_WARNING | LOG_CONSOLE, "Exit timeout elapsed (%u seconds). Killing", j->exit_timeout);
				job_kill(j);
				jobmgr_do_garbage_collection(j->mgr);
			}
		}
	} else {
		job_assumes(j, false);
	}
}

void
job_callback_read(job_t j, int ident)
{
	if (ident == j->log_redirect_fd) {
		job_log_stdouterr(j);
	} else if (ident == j->stdin_fd) {
		job_dispatch(j, true);
	} else {
		socketgroup_callback(j);
	}
}

void
jobmgr_reap_bulk(jobmgr_t jm, struct kevent *kev)
{
	jobmgr_t jmi;
	job_t j;

	SLIST_FOREACH(jmi, &jm->submgrs, sle) {
		jobmgr_reap_bulk(jmi, kev);
	}

	if ((j = jobmgr_find_by_pid(jm, (pid_t)kev->ident, false))) {
		kev->udata = j;
		job_callback(j, kev);
	}
}

void
jobmgr_callback(void *obj, struct kevent *kev)
{
	jobmgr_t jm = obj;
	job_t ji;

	switch (kev->filter) {
	case EVFILT_PROC:
		jobmgr_reap_bulk(jm, kev);
		root_jobmgr = jobmgr_do_garbage_collection(root_jobmgr);
		break;
	case EVFILT_SIGNAL:
		switch (kev->ident) {
		case SIGTERM:			
			return launchd_shutdown();
		case SIGUSR1:
			return calendarinterval_callback();
		case SIGUSR2:
			fake_shutdown_in_progress = true;
			runtime_setlogmask(LOG_UPTO(LOG_DEBUG));

			runtime_closelog(); /* HACK -- force 'start' time to be set */

			if (pid1_magic) {
				int64_t now = runtime_get_wall_time();

				jobmgr_log(jm, LOG_NOTICE, "Anticipatory shutdown began at: %lld.%06llu", now / USEC_PER_SEC, now % USEC_PER_SEC);

				LIST_FOREACH(ji, &root_jobmgr->jobs, sle) {
					if (ji->per_user && ji->p) {
						job_assumes(ji, runtime_kill(ji->p, SIGUSR2) != -1);
					}
				}
			} else {
				jobmgr_log(jm, LOG_NOTICE, "Anticipatory per-user launchd shutdown");
			}

			return;
		default:
			return (void)jobmgr_assumes(jm, false);
		}
		break;
	case EVFILT_FS:
		if (kev->fflags & VQ_MOUNT) {
			jobmgr_dispatch_all(jm, true);
		}
		jobmgr_dispatch_all_semaphores(jm);
		break;
	case EVFILT_TIMER:
		if( kev->ident == (uintptr_t)&sorted_calendar_events ) {
			calendarinterval_callback();
		} else if( kev->ident == (uintptr_t)jm ) {
			jobmgr_log(jm, LOG_DEBUG, "Shutdown timer firing.");
			jobmgr_still_alive_with_check(jm);
		} else if( kev->ident == (uintptr_t)&jm->reboot_flags ) {
			jobmgr_do_garbage_collection(jm);
		}
		break;
	case EVFILT_VNODE:
		if( kev->ident == (uintptr_t)s_no_hang_fd ) {
			int _no_hang_fd = open("/dev/autofs_nowait", O_EVTONLY | O_NONBLOCK);
			if( unlikely(_no_hang_fd != -1) ) {
				jobmgr_log(root_jobmgr, LOG_DEBUG, "/dev/autofs_nowait has appeared!");
				jobmgr_assumes(root_jobmgr, kevent_mod((uintptr_t)s_no_hang_fd, EVFILT_VNODE, EV_DELETE, 0, 0, NULL) != -1);
				jobmgr_assumes(root_jobmgr, runtime_close(s_no_hang_fd) != -1);
				s_no_hang_fd = _fd(_no_hang_fd);
			}
		} else if( pid1_magic && g_console && kev->ident == (uintptr_t)fileno(g_console) ) {
			int cfd = -1;
			if( launchd_assumes((cfd = open(_PATH_CONSOLE, O_WRONLY | O_NOCTTY)) != -1) ) {
				_fd(cfd);
				if( !launchd_assumes((g_console = fdopen(cfd, "w")) != NULL) ) {
					close(cfd);
				}
			}
		}
		break;
	default:
		return (void)jobmgr_assumes(jm, false);
	}
}

void
job_callback(void *obj, struct kevent *kev)
{
	job_t j = obj;

	job_log(j, LOG_DEBUG, "Dispatching kevent callback.");

	switch (kev->filter) {
	case EVFILT_PROC:
		return job_callback_proc(j, kev);
	case EVFILT_TIMER:
		return job_callback_timer(j, (void *) kev->ident);
	case EVFILT_VNODE:
		return semaphoreitem_callback(j, kev);
	case EVFILT_READ:
		return job_callback_read(j, (int) kev->ident);
	case EVFILT_MACHPORT:
		return (void)job_dispatch(j, true);
	default:
		return (void)job_assumes(j, false);
	}
}

void
job_start(job_t j)
{
	uint64_t td;
	int spair[2];
	int execspair[2];
	int oepair[2];
	char nbuf[64];
	pid_t c;
	bool sipc = false;
	u_int proc_fflags = NOTE_EXIT|NOTE_FORK|NOTE_EXEC|NOTE_REAP;
	
	if (!job_assumes(j, j->mgr != NULL)) {
		return;
	}
	
	if (unlikely(job_active(j))) {
		job_log(j, LOG_DEBUG, "Already started");
		return;
	}
	
	/*
	 * Some users adjust the wall-clock and then expect software to not notice.
	 * Therefore, launchd must use an absolute clock instead of the wall clock
	 * wherever possible.
	 */
	td = runtime_get_nanoseconds_since(j->start_time);
	td /= NSEC_PER_SEC;
	
	if (j->start_time && (td < j->min_run_time) && !j->legacy_mach_job && !j->inetcompat) {
		time_t respawn_delta = j->min_run_time - (uint32_t)td;
		
		/*
		 * We technically should ref-count throttled jobs to prevent idle exit,
		 * but we're not directly tracking the 'throttled' state at the moment.
		 */
		
		job_log(j, LOG_WARNING, "Throttling respawn: Will start in %ld seconds", respawn_delta);
		job_assumes(j, kevent_mod((uintptr_t)j, EVFILT_TIMER, EV_ADD|EV_ONESHOT, NOTE_SECONDS, respawn_delta, j) != -1);
		job_ignore(j);
		return;
	}
	
	if (likely(!j->legacy_mach_job)) {
		sipc = ((!SLIST_EMPTY(&j->sockets) || !SLIST_EMPTY(&j->machservices)) && !j->deny_job_creation) || j->embedded_special_privileges;
	}

	if( sipc ) {
		job_assumes(j, socketpair(AF_UNIX, SOCK_STREAM, 0, spair) != -1);
	}
	
	job_assumes(j, socketpair(AF_UNIX, SOCK_STREAM, 0, execspair) != -1);
	
	if (likely(!j->legacy_mach_job) && job_assumes(j, pipe(oepair) != -1)) {
		j->log_redirect_fd = _fd(oepair[0]);
		job_assumes(j, fcntl(j->log_redirect_fd, F_SETFL, O_NONBLOCK) != -1);
		job_assumes(j, kevent_mod(j->log_redirect_fd, EVFILT_READ, EV_ADD, 0, 0, j) != -1);
	}
	
	switch (c = runtime_fork(j->weird_bootstrap ? j->j_port : j->mgr->jm_port)) {
	case -1:
		job_log_error(j, LOG_ERR, "fork() failed, will try again in one second");
		job_assumes(j, kevent_mod((uintptr_t)j, EVFILT_TIMER, EV_ADD|EV_ONESHOT, NOTE_SECONDS, 1, j) != -1);
		job_ignore(j);
		
		job_assumes(j, runtime_close(execspair[0]) == 0);
		job_assumes(j, runtime_close(execspair[1]) == 0);
		if (sipc) {
			job_assumes(j, runtime_close(spair[0]) == 0);
			job_assumes(j, runtime_close(spair[1]) == 0);
		}
		if (likely(!j->legacy_mach_job)) {
			job_assumes(j, runtime_close(oepair[0]) != -1);
			job_assumes(j, runtime_close(oepair[1]) != -1);
			j->log_redirect_fd = 0;
		}
		break;
	case 0:
		if (unlikely(_vproc_post_fork_ping())) {
			_exit(EXIT_FAILURE);
		}
		if (!j->legacy_mach_job) {
			job_assumes(j, dup2(oepair[1], STDOUT_FILENO) != -1);
			job_assumes(j, dup2(oepair[1], STDERR_FILENO) != -1);
			job_assumes(j, runtime_close(oepair[1]) != -1);
		}
		job_assumes(j, runtime_close(execspair[0]) == 0);
		/* wait for our parent to say they've attached a kevent to us */
		read(_fd(execspair[1]), &c, sizeof(c));
		
		if (sipc) {
			job_assumes(j, runtime_close(spair[0]) == 0);
			snprintf(nbuf, sizeof(nbuf), "%d", spair[1]);
			setenv(LAUNCHD_TRUSTED_FD_ENV, nbuf, 1);
		}
		job_start_child(j);
		break;
	default:
		j->start_time = runtime_get_opaque_time();
		
		job_log(j, LOG_DEBUG, "Started as PID: %u", c);
		
		j->did_exec = false;
		j->checkedin = false;
		j->start_pending = false;
		j->reaped = false;
		j->crashed = false;
		j->stopped = false;
		if( j->needs_kickoff ) {
			j->needs_kickoff = false;
			
			if( SLIST_EMPTY(&j->semaphores) ) {
				j->ondemand = false;
			}
		}
		
		if( j->has_console ) {
			g_wsp = c;
		}
		
		runtime_add_ref();
		total_children++;
		LIST_INSERT_HEAD(&j->mgr->active_jobs[ACTIVE_JOB_HASH(c)], j, pid_hash_sle);
		
		if (likely(!j->legacy_mach_job)) {
			job_assumes(j, runtime_close(oepair[1]) != -1);
		}
		j->p = c;
		if (unlikely(j->hopefully_exits_first)) {
			j->mgr->hopefully_first_cnt++;
		} else if (likely(!j->hopefully_exits_last)) {
			j->mgr->normal_active_cnt++;
		}
		j->fork_fd = _fd(execspair[0]);
		job_assumes(j, runtime_close(execspair[1]) == 0);
		if (sipc) {
			job_assumes(j, runtime_close(spair[1]) == 0);
			ipc_open(_fd(spair[0]), j);
		}
		if (job_assumes(j, kevent_mod(c, EVFILT_PROC, EV_ADD, proc_fflags, 0, root_jobmgr ? root_jobmgr : j->mgr) != -1)) {
			job_ignore(j);
		} else {
			job_reap(j);
		}
		
		j->wait4debugger_oneshot = false;

		struct envitem *ei = NULL, *et = NULL;
		SLIST_FOREACH_SAFE( ei, &j->env, sle, et ) {
			if( ei->one_shot ) {
				SLIST_REMOVE(&j->env, ei, envitem, sle);
			}
		}
			
		if (likely(!j->stall_before_exec)) {
			job_uncork_fork(j);
		}
		break;
	}
}

void
job_start_child(job_t j)
{
	typeof(posix_spawn) *psf;
	const char *file2exec = "/usr/libexec/launchproxy";
	const char **argv;
	posix_spawnattr_t spattr;
	int gflags = GLOB_NOSORT|GLOB_NOCHECK|GLOB_TILDE|GLOB_DOOFFS;
	glob_t g;
	short spflags = POSIX_SPAWN_SETEXEC;
	size_t binpref_out_cnt = 0;
	size_t i;

	job_assumes(j, posix_spawnattr_init(&spattr) == 0);

	job_setup_attributes(j);

	if (unlikely(j->argv && j->globargv)) {
		g.gl_offs = 1;
		for (i = 0; i < j->argc; i++) {
			if (i > 0) {
				gflags |= GLOB_APPEND;
			}
			if (glob(j->argv[i], gflags, NULL, &g) != 0) {
				job_log_error(j, LOG_ERR, "glob(\"%s\")", j->argv[i]);
				exit(EXIT_FAILURE);
			}
		}
		g.gl_pathv[0] = (char *)file2exec;
		argv = (const char **)g.gl_pathv;
	} else if (likely(j->argv)) {
		argv = alloca((j->argc + 2) * sizeof(char *));
		argv[0] = file2exec;
		for (i = 0; i < j->argc; i++) {
			argv[i + 1] = j->argv[i];
		}
		argv[i + 1] = NULL;
	} else {
		argv = alloca(3 * sizeof(char *));
		argv[0] = file2exec;
		argv[1] = j->prog;
		argv[2] = NULL;
	}

	if (likely(!j->inetcompat)) {
		argv++;
	}

	if (unlikely(j->wait4debugger || j->wait4debugger_oneshot)) {
		job_log(j, LOG_WARNING, "Spawned and waiting for the debugger to attach before continuing...");
		spflags |= POSIX_SPAWN_START_SUSPENDED;
	}

	job_assumes(j, posix_spawnattr_setflags(&spattr, spflags) == 0);

	if (unlikely(j->j_binpref_cnt)) {
		job_assumes(j, posix_spawnattr_setbinpref_np(&spattr, j->j_binpref_cnt, j->j_binpref, &binpref_out_cnt) == 0);
		job_assumes(j, binpref_out_cnt == j->j_binpref_cnt);
	}

#if HAVE_QUARANTINE
	if (j->quarantine_data) {
		qtn_proc_t qp;

		if (job_assumes(j, qp = qtn_proc_alloc())) {
			if (job_assumes(j, qtn_proc_init_with_data(qp, j->quarantine_data, j->quarantine_data_sz) == 0)) {
				job_assumes(j, qtn_proc_apply_to_self(qp) == 0);
			}
		}
	}
#endif

#if HAVE_SANDBOX
	if (j->seatbelt_profile) {
		char *seatbelt_err_buf = NULL;

		if (!job_assumes(j, sandbox_init(j->seatbelt_profile, j->seatbelt_flags, &seatbelt_err_buf) != -1)) {
			if (seatbelt_err_buf) {
				job_log(j, LOG_ERR, "Sandbox failed to init: %s", seatbelt_err_buf);
			}
			goto out_bad;
		}
	}
#endif

	psf = j->prog ? posix_spawn : posix_spawnp;

	if (likely(!j->inetcompat)) {
		file2exec = j->prog ? j->prog : argv[0];
	}

	errno = psf(NULL, file2exec, NULL, &spattr, (char *const*)argv, environ);
	job_log_error(j, LOG_ERR, "posix_spawn(\"%s\", ...)", file2exec);
	
#if HAVE_SANDBOX
out_bad:
#endif
	_exit(EXIT_FAILURE);
}

void
jobmgr_export_env_from_other_jobs(jobmgr_t jm, launch_data_t dict)
{
	launch_data_t tmp;
	struct envitem *ei;
	job_t ji;

	if (jm->parentmgr) {
		jobmgr_export_env_from_other_jobs(jm->parentmgr, dict);
	} else {
		char **tmpenviron = environ;
		for (; *tmpenviron; tmpenviron++) {
			char envkey[1024];
			launch_data_t s = launch_data_alloc(LAUNCH_DATA_STRING);
			launch_data_set_string(s, strchr(*tmpenviron, '=') + 1);
			strncpy(envkey, *tmpenviron, sizeof(envkey));
			*(strchr(envkey, '=')) = '\0';
			launch_data_dict_insert(dict, s, envkey);
		}
	}

	LIST_FOREACH(ji, &jm->jobs, sle) {
		SLIST_FOREACH(ei, &ji->global_env, sle) {
			if ((tmp = launch_data_new_string(ei->value))) {
				launch_data_dict_insert(dict, tmp, ei->key);
			}
		}
	}
}

void
jobmgr_setup_env_from_other_jobs(jobmgr_t jm)
{
	struct envitem *ei;
	job_t ji;

	if (jm->parentmgr) {
		jobmgr_setup_env_from_other_jobs(jm->parentmgr);
	}

	LIST_FOREACH(ji, &jm->global_env_jobs, global_env_sle) {
		SLIST_FOREACH(ei, &ji->global_env, sle) {
			setenv(ei->key, ei->value, 1);
		}
	}
}

void
job_log_pids_with_weird_uids(job_t j)
{
	int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL };
	size_t i, kp_cnt, len = sizeof(struct kinfo_proc) * get_kern_max_proc();
	struct kinfo_proc *kp;
	uid_t u = j->mach_uid;

	if (!do_apple_internal_logging) {
		return;
	}

	kp = malloc(len);

	if (!job_assumes(j, kp != NULL)) {
		return;
	}

	runtime_ktrace(RTKT_LAUNCHD_FINDING_WEIRD_UIDS, j->p, u, 0);

	if (!job_assumes(j, sysctl(mib, 3, kp, &len, NULL, 0) != -1)) {
		goto out;
	}

	kp_cnt = len / sizeof(struct kinfo_proc);

	for (i = 0; i < kp_cnt; i++) {
		uid_t i_euid = kp[i].kp_eproc.e_ucred.cr_uid;
		uid_t i_uid = kp[i].kp_eproc.e_pcred.p_ruid;
		uid_t i_svuid = kp[i].kp_eproc.e_pcred.p_svuid;
		pid_t i_pid = kp[i].kp_proc.p_pid;

		if (i_euid != u && i_uid != u && i_svuid != u) {
			continue;
		}

		job_log(j, LOG_ERR, "PID %u \"%s\" has no account to back it! Real/effective/saved UIDs: %u/%u/%u",
				i_pid, kp[i].kp_proc.p_comm, i_uid, i_euid, i_svuid);

/* Temporarily disabled due to 5423935 and 4946119. */
#if 0
		/* Ask the accountless process to exit. */
		job_assumes(j, runtime_kill(i_pid, SIGTERM) != -1);
#endif
	}

out:
	free(kp);
}

void
job_postfork_test_user(job_t j)
{
	/* This function is all about 5201578 */

	const char *home_env_var = getenv("HOME");
	const char *user_env_var = getenv("USER");
	const char *logname_env_var = getenv("LOGNAME");
	uid_t tmp_uid, local_uid = getuid();
	gid_t tmp_gid, local_gid = getgid();
	char shellpath[PATH_MAX];
	char homedir[PATH_MAX];
	char loginname[2000];
	struct passwd *pwe;


	if (!job_assumes(j, home_env_var && user_env_var && logname_env_var
				&& strcmp(user_env_var, logname_env_var) == 0)) {
		goto out_bad;
	}

	if ((pwe = getpwnam(user_env_var)) == NULL) {
		job_log(j, LOG_ERR, "The account \"%s\" has been deleted out from under us!", user_env_var);
		goto out_bad;
	}

	/*
	 * We must copy the results of getpw*().
	 *
	 * Why? Because subsequent API calls may call getpw*() as a part of
	 * their implementation. Since getpw*() returns a [now thread scoped]
	 * global, we must therefore cache the results before continuing.
	 */

	tmp_uid = pwe->pw_uid;
	tmp_gid = pwe->pw_gid;

	strlcpy(shellpath, pwe->pw_shell, sizeof(shellpath));
	strlcpy(loginname, pwe->pw_name, sizeof(loginname));
	strlcpy(homedir, pwe->pw_dir, sizeof(homedir));

	if (strcmp(loginname, logname_env_var) != 0) {
		job_log(j, LOG_ERR, "The %s environmental variable changed out from under us!", "USER");
		goto out_bad;
	}
	if (strcmp(homedir, home_env_var) != 0) {
		job_log(j, LOG_ERR, "The %s environmental variable changed out from under us!", "HOME");
		goto out_bad;
	}
	if (local_uid != tmp_uid) {
		job_log(j, LOG_ERR, "The %cID of the account (%u) changed out from under us (%u)!",
				'U', tmp_uid, local_uid);
		goto out_bad;
	}
	if (local_gid != tmp_gid) {
		job_log(j, LOG_ERR, "The %cID of the account (%u) changed out from under us (%u)!",
				'G', tmp_gid, local_gid);
		goto out_bad;
	}

	return;
out_bad:
#if 0
	job_assumes(j, runtime_kill(getppid(), SIGTERM) != -1);
	_exit(EXIT_FAILURE);
#else
	job_log(j, LOG_WARNING, "In a future build of the OS, this error will be fatal.");
#endif
}

void
job_postfork_become_user(job_t j)
{
	char loginname[2000];
	char tmpdirpath[PATH_MAX];
	char shellpath[PATH_MAX];
	char homedir[PATH_MAX];
	struct passwd *pwe;
	size_t r;
	gid_t desired_gid = -1;
	uid_t desired_uid = -1;

	if (getuid() != 0) {
		return job_postfork_test_user(j);
	}

	/*
	 * I contend that having UID == 0 and GID != 0 is of dubious value.
	 * Nevertheless, this used to work in Tiger. See: 5425348
	 */
	if (j->groupname && !j->username) {
		j->username = "root";
	}

	if (j->username) {
		if ((pwe = getpwnam(j->username)) == NULL) {
			job_log(j, LOG_ERR, "getpwnam(\"%s\") failed", j->username);
			_exit(EXIT_FAILURE);
		}
	} else if (j->mach_uid) {
		if ((pwe = getpwuid(j->mach_uid)) == NULL) {
			job_log(j, LOG_ERR, "getpwuid(\"%u\") failed", j->mach_uid);
			job_log_pids_with_weird_uids(j);
			_exit(EXIT_FAILURE);
		}
	} else {
		return;
	}

	/*
	 * We must copy the results of getpw*().
	 *
	 * Why? Because subsequent API calls may call getpw*() as a part of
	 * their implementation. Since getpw*() returns a [now thread scoped]
	 * global, we must therefore cache the results before continuing.
	 */

	desired_uid = pwe->pw_uid;
	desired_gid = pwe->pw_gid;

	strlcpy(shellpath, pwe->pw_shell, sizeof(shellpath));
	strlcpy(loginname, pwe->pw_name, sizeof(loginname));
	strlcpy(homedir, pwe->pw_dir, sizeof(homedir));

	if (unlikely(pwe->pw_expire && time(NULL) >= pwe->pw_expire)) {
		job_log(j, LOG_ERR, "Expired account");
		_exit(EXIT_FAILURE);
	}


	if (unlikely(j->username && strcmp(j->username, loginname) != 0)) {
		job_log(j, LOG_WARNING, "Suspicious setup: User \"%s\" maps to user: %s", j->username, loginname);
	} else if (unlikely(j->mach_uid && (j->mach_uid != desired_uid))) {
		job_log(j, LOG_WARNING, "Suspicious setup: UID %u maps to UID %u", j->mach_uid, desired_uid);
	}

	if (j->groupname) {
		struct group *gre;

		if (unlikely((gre = getgrnam(j->groupname)) == NULL)) {
			job_log(j, LOG_ERR, "getgrnam(\"%s\") failed", j->groupname);
			_exit(EXIT_FAILURE);
		}

		desired_gid = gre->gr_gid;
	}

	if (!job_assumes(j, setlogin(loginname) != -1)) {
		_exit(EXIT_FAILURE);
	}

	if (!job_assumes(j, setgid(desired_gid) != -1)) {
		_exit(EXIT_FAILURE);
	}

	/*
	 * The kernel team and the DirectoryServices team want initgroups()
	 * called after setgid(). See 4616864 for more information.
	 */

	if (likely(!j->no_init_groups)) {
	#if 1
		if (!job_assumes(j, initgroups(loginname, desired_gid) != -1)) {
			_exit(EXIT_FAILURE);
		}
	#else
		/* Do our own little initgroups(). We do this to guarantee that we're
		 * always opted into dynamic group resolution in the kernel. initgroups(3)
		 * does not make this guarantee.
		 */
		int groups[NGROUPS], ngroups;
		
		/* A failure here isn't fatal, and we'll still get data we can use. */
		job_assumes(j, getgrouplist(j->username, desired_gid, groups, &ngroups) != -1);
		
		if( !job_assumes(j, syscall(SYS_initgroups, ngroups, groups, desired_uid) != -1) ) {
			_exit(EXIT_FAILURE);
		}
	#endif
	}

	if (!job_assumes(j, setuid(desired_uid) != -1)) {
		_exit(EXIT_FAILURE);
	}

	r = confstr(_CS_DARWIN_USER_TEMP_DIR, tmpdirpath, sizeof(tmpdirpath));

	if (likely(r > 0 && r < sizeof(tmpdirpath))) {
		setenv("TMPDIR", tmpdirpath, 0);
	}

	setenv("SHELL", shellpath, 0);
	setenv("HOME", homedir, 0);
	setenv("USER", loginname, 0);
	setenv("LOGNAME", loginname, 0);
}

void
job_setup_attributes(job_t j)
{
	struct limititem *li;
	struct envitem *ei;

	if (unlikely(j->setnice)) {
		job_assumes(j, setpriority(PRIO_PROCESS, 0, j->nice) != -1);
	}

	SLIST_FOREACH(li, &j->limits, sle) {
		struct rlimit rl;

		if (!job_assumes(j, getrlimit(li->which, &rl) != -1)) {
			continue;
		}

		if (li->sethard) {
			rl.rlim_max = li->lim.rlim_max;
		}
		if (li->setsoft) {
			rl.rlim_cur = li->lim.rlim_cur;
		}

		if (setrlimit(li->which, &rl) == -1) {
			job_log_error(j, LOG_WARNING, "setrlimit()");
		}
	}

#if !TARGET_OS_EMBEDDED
	if( unlikely(j->per_user) ) {
		auditinfo_addr_t auinfo = {
			.ai_termid = { .at_type = AU_IPv4 },
			.ai_auid = j->mach_uid,
			.ai_asid = AU_ASSIGN_ASID,
		};
		(void)au_user_mask(j->username, &auinfo.ai_mask);
		
		if( !launchd_assumes(setaudit_addr(&auinfo, sizeof(auinfo)) != -1) ) {
			runtime_syslog(LOG_WARNING, "Could not set audit session! (errno = %d)", errno);
			_exit(EXIT_FAILURE);
		} else {
			job_log(j, LOG_DEBUG, "Created new security session for per-user launchd.");
		}
	}
#endif

	if (unlikely(!j->inetcompat && j->session_create)) {
		launchd_SessionCreate();
	}

	if (unlikely(j->low_pri_io)) {
		job_assumes(j, setiopolicy_np(IOPOL_TYPE_DISK, IOPOL_SCOPE_PROCESS, IOPOL_THROTTLE) != -1);
	}
	if (unlikely(j->rootdir)) {
		job_assumes(j, chroot(j->rootdir) != -1);
		job_assumes(j, chdir(".") != -1);
	}

	job_postfork_become_user(j);

	if (unlikely(j->workingdir)) {
		job_assumes(j, chdir(j->workingdir) != -1);
	}

	if (unlikely(j->setmask)) {
		umask(j->mask);
	}

	if (j->stdin_fd) {
		job_assumes(j, dup2(j->stdin_fd, STDIN_FILENO) != -1);
	} else {
		job_setup_fd(j, STDIN_FILENO, j->stdinpath, O_RDONLY|O_CREAT);
	}
	job_setup_fd(j, STDOUT_FILENO, j->stdoutpath, O_WRONLY|O_CREAT|O_APPEND);
	job_setup_fd(j, STDERR_FILENO, j->stderrpath, O_WRONLY|O_CREAT|O_APPEND);

	jobmgr_setup_env_from_other_jobs(j->mgr);

	SLIST_FOREACH(ei, &j->env, sle) {
		setenv(ei->key, ei->value, 1);
	}

	if( do_apple_internal_logging ) {
		setenv(LAUNCHD_DO_APPLE_INTERNAL_LOGGING, "true", 1);
	}

#if !TARGET_OS_EMBEDDED	
	if( j->jetsam_priority != LAUNCHD_JETSAM_PRIORITY_UNSET ) {
		job_assumes(j, proc_setpcontrol(PROC_SETPC_TERMINATE) == 0);
	}
#endif

#if TARGET_OS_EMBEDDED
	if( j->main_thread_priority != 0 ) {
		struct sched_param params;
		bzero(&params, sizeof(params));
		params.sched_priority = j->main_thread_priority;
		job_assumes(j, pthread_setschedparam(pthread_self(), SCHED_OTHER, &params) != -1);
	}
#endif

	/*
	 * We'd like to call setsid() unconditionally, but we have reason to
	 * believe that prevents launchd from being able to send signals to
	 * setuid children. We'll settle for process-groups.
	 */
	if (getppid() != 1) {
		job_assumes(j, setpgid(0, 0) != -1);
	} else {
		job_assumes(j, setsid() != -1);
	}
}

void
job_setup_fd(job_t j, int target_fd, const char *path, int flags)
{
	int fd;

	if (!path) {
		return;
	}

	if ((fd = open(path, flags|O_NOCTTY, DEFFILEMODE)) == -1) {
		job_log_error(j, LOG_WARNING, "open(\"%s\", ...)", path);
		return;
	}

	job_assumes(j, dup2(fd, target_fd) != -1);
	job_assumes(j, runtime_close(fd) == 0);
}

int
dir_has_files(job_t j, const char *path)
{
	DIR *dd = opendir(path);
	struct dirent *de;
	bool r = 0;

	if (unlikely(!dd)) {
		return -1;
	}

	while ((de = readdir(dd))) {
		if (strcmp(de->d_name, ".") && strcmp(de->d_name, "..")) {
			r = 1;
			break;
		}
	}

	job_assumes(j, closedir(dd) == 0);
	return r;
}

void
calendarinterval_setalarm(job_t j, struct calendarinterval *ci)
{
	struct calendarinterval *ci_iter, *ci_prev = NULL;
	time_t later, head_later;

	later = cronemu(ci->when.tm_mon, ci->when.tm_mday, ci->when.tm_hour, ci->when.tm_min);

	if (ci->when.tm_wday != -1) {
		time_t otherlater = cronemu_wday(ci->when.tm_wday, ci->when.tm_hour, ci->when.tm_min);

		if (ci->when.tm_mday == -1) {
			later = otherlater;
		} else {
			later = later < otherlater ? later : otherlater;
		}
	}

	ci->when_next = later;

	LIST_FOREACH(ci_iter, &sorted_calendar_events, global_sle) {
		if (ci->when_next < ci_iter->when_next) {
			LIST_INSERT_BEFORE(ci_iter, ci, global_sle);
			break;
		}

		ci_prev = ci_iter;
	}

	if (ci_iter == NULL) {
		/* ci must want to fire after every other timer, or there are no timers */

		if (LIST_EMPTY(&sorted_calendar_events)) {
			LIST_INSERT_HEAD(&sorted_calendar_events, ci, global_sle);
		} else {
			LIST_INSERT_AFTER(ci_prev, ci, global_sle);
		}
	}

	head_later = LIST_FIRST(&sorted_calendar_events)->when_next;

	if (job_assumes(j, kevent_mod((uintptr_t)&sorted_calendar_events, EVFILT_TIMER, EV_ADD, NOTE_ABSOLUTE|NOTE_SECONDS, head_later, root_jobmgr) != -1)) {
		char time_string[100];
		size_t time_string_len;

		ctime_r(&later, time_string);
		time_string_len = strlen(time_string);

		if (likely(time_string_len && time_string[time_string_len - 1] == '\n')) {
			time_string[time_string_len - 1] = '\0';
		}

		job_log(j, LOG_INFO, "Scheduled to run again at %s", time_string);
	}
}

void
extract_rcsid_substr(const char *i, char *o, size_t osz)
{
	char *rcs_rev_tmp = strchr(i, ' ');

	if (!rcs_rev_tmp) {
		strlcpy(o, i, osz);
	} else {
		strlcpy(o, rcs_rev_tmp + 1, osz);
		rcs_rev_tmp = strchr(o, ' ');
		if (rcs_rev_tmp) {
			*rcs_rev_tmp = '\0';
		}
	}
}

void
jobmgr_log_bug(jobmgr_t jm, unsigned int line)
{
	static const char *file;
	int saved_errno = errno;
	char buf[100];

	runtime_ktrace1(RTKT_LAUNCHD_BUG);

	extract_rcsid_substr(__rcs_file_version__, buf, sizeof(buf));

	if (!file) {
		file = strrchr(__FILE__, '/');
		if (!file) {
			file = __FILE__;
		} else {
			file += 1;
		}
	}

	/* the only time 'jm' should not be set is if setting up the first bootstrap fails for some reason */
	if (likely(jm)) {
		jobmgr_log(jm, LOG_NOTICE, "Bug: %s:%u (%s):%u", file, line, buf, saved_errno);
	} else {
		runtime_syslog(LOG_NOTICE, "Bug: %s:%u (%s):%u", file, line, buf, saved_errno);
	}
}

void
job_log_bug(job_t j, unsigned int line)
{
	static const char *file;
	int saved_errno = errno;
	char buf[100];

	runtime_ktrace1(RTKT_LAUNCHD_BUG);

	extract_rcsid_substr(__rcs_file_version__, buf, sizeof(buf));

	if (!file) {
		file = strrchr(__FILE__, '/');
		if (!file) {
			file = __FILE__;
		} else {
			file += 1;
		}
	}

	/* I cannot think of any reason why 'j' should ever be NULL, nor have I ever seen the case in the wild */
	if (likely(j)) {
		job_log(j, LOG_NOTICE, "Bug: %s:%u (%s):%u", file, line, buf, saved_errno);
	} else {
		runtime_syslog(LOG_NOTICE, "Bug: %s:%u (%s):%u", file, line, buf, saved_errno);
	}
}

void
job_logv(job_t j, int pri, int err, const char *msg, va_list ap)
{
	const char *label2use = j ? j->label : "com.apple.launchd.NULL";
	const char *mgr2use = j ? j->mgr->name : "NULL";
	struct runtime_syslog_attr attr = { g_my_label, label2use, mgr2use, pri, getuid(), getpid(), j ? j->p : 0 };
	char *newmsg;
	int oldmask = 0;
	size_t newmsgsz;

	/*
	 * Hack: If bootstrap_port is set, we must be on the child side of a
	 * fork(), but before the exec*(). Let's route the log message back to
	 * launchd proper.
	 */
	if (bootstrap_port) {
		return _vproc_logv(pri, err, msg, ap);
	}

	newmsgsz = strlen(msg) + 200;
	newmsg = alloca(newmsgsz);

	if (err) {
	#if !TARGET_OS_EMBEDDED
		snprintf(newmsg, newmsgsz, "%s: %s", msg, strerror(err));
	#else
		snprintf(newmsg, newmsgsz, "(%s) %s: %s", label2use, msg, strerror(err));
	#endif
	} else {
	#if !TARGET_OS_EMBEDDED
		snprintf(newmsg, newmsgsz, "%s", msg);
	#else
		snprintf(newmsg, newmsgsz, "(%s) %s", label2use, msg);
	#endif
	}

	if( j && unlikely(j->debug) ) {
		oldmask = setlogmask(LOG_UPTO(LOG_DEBUG));
	}

	runtime_vsyslog(&attr, newmsg, ap);

	if( j && unlikely(j->debug) ) {
		setlogmask(oldmask);
	}
}

void
job_log_error(job_t j, int pri, const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	job_logv(j, pri, errno, msg, ap);
	va_end(ap);
}

void
job_log(job_t j, int pri, const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	job_logv(j, pri, 0, msg, ap);
	va_end(ap);
}

#if 0
void
jobmgr_log_error(jobmgr_t jm, int pri, const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	jobmgr_logv(jm, pri, errno, msg, ap);
	va_end(ap);
}
#endif

void
jobmgr_log(jobmgr_t jm, int pri, const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	jobmgr_logv(jm, pri, 0, msg, ap);
	va_end(ap);
}

void
jobmgr_logv(jobmgr_t jm, int pri, int err, const char *msg, va_list ap)
{
	char *newmsg;
	char *newname;
	size_t i, o, jmname_len = strlen(jm->name), newmsgsz;

	newname = alloca((jmname_len + 1) * 2);
	newmsgsz = (jmname_len + 1) * 2 + strlen(msg) + 100;
	newmsg = alloca(newmsgsz);

	for (i = 0, o = 0; i < jmname_len; i++, o++) {
		if (jm->name[i] == '%') {
			newname[o] = '%';
			o++;
		}
		newname[o] = jm->name[i];
	}
	newname[o] = '\0';

	if (err) {
		snprintf(newmsg, newmsgsz, "%s: %s: %s", newname, msg, strerror(err));
	} else {
		snprintf(newmsg, newmsgsz, "%s: %s", newname, msg);
	}

	if (jm->parentmgr) {
		jobmgr_logv(jm->parentmgr, pri, 0, newmsg, ap);
	} else {
		struct runtime_syslog_attr attr = { g_my_label, g_my_label, jm->name, pri, getuid(), getpid(), getpid() };

		runtime_vsyslog(&attr, newmsg, ap);
	}
}

void
semaphoreitem_ignore(job_t j, struct semaphoreitem *si)
{
	if (si->fd != -1) {
		job_log(j, LOG_DEBUG, "Ignoring Vnode: %d", si->fd);
		job_assumes(j, kevent_mod(si->fd, EVFILT_VNODE, EV_DELETE, 0, 0, NULL) != -1);
	}
}

void
semaphoreitem_watch(job_t j, struct semaphoreitem *si)
{
	char *parentdir, tmp_path[PATH_MAX];
	int saved_errno = 0;
	int fflags = NOTE_DELETE|NOTE_RENAME;

	switch (si->why) {
	case DIR_NOT_EMPTY:
	case PATH_CHANGES:
		fflags |= NOTE_ATTRIB|NOTE_LINK;
		/* fall through */
	case PATH_EXISTS:
		fflags |= NOTE_REVOKE|NOTE_EXTEND|NOTE_WRITE;
		/* fall through */
	case PATH_MISSING:
		break;
	default:
		return;
	}

	/* dirname() may modify tmp_path */
	strlcpy(tmp_path, si->what, sizeof(tmp_path));

	if (!job_assumes(j, (parentdir = dirname(tmp_path)))) {
		return;
	}

	/* See 5321044 for why we do the do-while loop and 5415523 for why ENOENT is checked */
	do {
		if (si->fd == -1) {
			struct stat sb;
			if( stat(si->what, &sb) == 0 ) {
				/* If we're watching a character or block device, only watch the parent directory. 
				 * See rdar://problem/6489900 for the gory details. Basically, holding an open file
				 * descriptor to a devnode could end up (a) blocking us on open(2) until someone else
				 * open(2)s the file (like a character device that waits for a carrier signal) or 
				 * (b) preventing other processes from obtaining an exclusive lock on the file, even
				 * though we're opening it with O_EVTONLY.
				 *
				 * The main point of contention is that O_EVTONLY doesn't actually mean "event only".
				 * It means "Don't prevent unmounts of this descriptor's volume". We work around this
				 * for dev nodes by only watching the parent directory and stat(2)ing our desired file
				 * each time the parent changes to see if it appeared or disappeared.
				 */
				if( S_ISREG(sb.st_mode) || S_ISDIR(sb.st_mode) ) {
					si->fd = _fd(open(si->what, O_EVTONLY | O_NOCTTY | O_NONBLOCK));
				}
			}
			
			if( si->fd == -1 ) {
				si->watching_parent = job_assumes(j, (si->fd = _fd(open(parentdir, O_EVTONLY | O_NOCTTY | O_NONBLOCK))) != -1);
			} else {
				si->watching_parent = false;
			}
		}

		if (si->fd == -1) {
			return job_log_error(j, LOG_ERR, "Path monitoring failed on \"%s\"", si->what);
		}

		job_log(j, LOG_DEBUG, "Watching %svnode (%s): %d", si->watching_parent ? "parent ": "", si->what, si->fd);

		if (kevent_mod(si->fd, EVFILT_VNODE, EV_ADD, fflags, 0, j) == -1) {
			saved_errno = errno;
			/*
			 * The FD can be revoked between the open() and kevent().
			 * This is similar to the inability for kevents to be
			 * attached to short lived zombie processes after fork()
			 * but before kevent().
			 */
			job_assumes(j, runtime_close(si->fd) == 0);
			si->fd = -1;
		}
	} while (unlikely((si->fd == -1) && (saved_errno == ENOENT)));

	if (saved_errno == ENOTSUP) {
		/*
		 * 3524219 NFS needs kqueue support
		 * 4124079 VFS needs generic kqueue support
		 * 5226811 EVFILT: Launchd EVFILT_VNODE doesn't work on /dev
		 */
		job_log(j, LOG_DEBUG, "Falling back to polling for path: %s", si->what);

		if (!j->poll_for_vfs_changes) {
			j->poll_for_vfs_changes = true;
			job_assumes(j, kevent_mod((uintptr_t)&j->semaphores, EVFILT_TIMER, EV_ADD, NOTE_SECONDS, 3, j) != -1);
		}
	}
}

void
semaphoreitem_callback(job_t j, struct kevent *kev)
{
	char invalidation_reason[100] = "";
	struct semaphoreitem *si;

	SLIST_FOREACH(si, &j->semaphores, sle) {
		switch (si->why) {
		case PATH_CHANGES:
		case PATH_EXISTS:
		case PATH_MISSING:
		case DIR_NOT_EMPTY:
			job_log(j, LOG_DEBUG, "P%s changed (%u): %s", si->watching_parent ? "arent path" : "ath", si->why, si->what);
			break;
		default:
			continue;
		}

		if (si->fd == (int)kev->ident) {
			break;
		}
	}

	if (!job_assumes(j, si != NULL)) {
		return;
	}

	if (NOTE_DELETE & kev->fflags) {
		strcat(invalidation_reason, "deleted");
	}

	if (NOTE_RENAME & kev->fflags) {
		if (invalidation_reason[0]) {
			strcat(invalidation_reason, "/renamed");
		} else {
			strcat(invalidation_reason, "renamed");
		}
	}

	if (NOTE_REVOKE & kev->fflags) {
		if (invalidation_reason[0]) {
			strcat(invalidation_reason, "/revoked");
		} else {
			strcat(invalidation_reason, "revoked");
		}
	}

	if (invalidation_reason[0]) {
		job_log(j, LOG_DEBUG, "Path %s: %s", invalidation_reason, si->what);
		job_assumes(j, runtime_close(si->fd) == 0);
		si->fd = -1; /* this will get fixed in semaphoreitem_watch() */
	}

	if( !si->watching_parent ) {
		if (si->why == PATH_CHANGES) {
			j->start_pending = true;
		} else {
			semaphoreitem_watch(j, si);
		}
	} else { /* Something happened to the parent directory. See if our target file appeared. */
		if( !invalidation_reason[0] ) {
			job_assumes(j, runtime_close(si->fd) == 0);
			si->fd = -1; /* this will get fixed in semaphoreitem_watch() */
			semaphoreitem_watch(j, si);
		}
		/* Need to think about what should happen if the parent directory goes invalid. */
	}
	
	job_dispatch(j, false);
}

struct cal_dict_walk {
	job_t j;
	struct tm tmptm;
};

void
calendarinterval_new_from_obj_dict_walk(launch_data_t obj, const char *key, void *context)
{
	struct cal_dict_walk *cdw = context;
	struct tm *tmptm = &cdw->tmptm;
	job_t j = cdw->j;
	int64_t val;

	if (unlikely(LAUNCH_DATA_INTEGER != launch_data_get_type(obj))) {
		/* hack to let caller know something went wrong */
		tmptm->tm_sec = -1;
		return;
	}

	val = launch_data_get_integer(obj);

	if (val < 0) {
		job_log(j, LOG_WARNING, "The interval for key \"%s\" is less than zero.", key);
	} else if (strcasecmp(key, LAUNCH_JOBKEY_CAL_MINUTE) == 0) {
		if( val > 59 ) {
			job_log(j, LOG_WARNING, "The interval for key \"%s\" is not between 0 and 59 (inclusive).", key);
			tmptm->tm_sec = -1;
		} else {
			tmptm->tm_min = (typeof(tmptm->tm_min)) val;
		}
	} else if (strcasecmp(key, LAUNCH_JOBKEY_CAL_HOUR) == 0) {
		if( val > 23 ) {
			job_log(j, LOG_WARNING, "The interval for key \"%s\" is not between 0 and 23 (inclusive).", key);
			tmptm->tm_sec = -1;
		} else {
			tmptm->tm_hour = (typeof(tmptm->tm_hour)) val;
		}
	} else if (strcasecmp(key, LAUNCH_JOBKEY_CAL_DAY) == 0) {
		if( val < 1 || val > 31 ) {
			job_log(j, LOG_WARNING, "The interval for key \"%s\" is not between 1 and 31 (inclusive).", key);
			tmptm->tm_sec = -1;
		} else {
			tmptm->tm_mday = (typeof(tmptm->tm_mday)) val;
		}
	} else if (strcasecmp(key, LAUNCH_JOBKEY_CAL_WEEKDAY) == 0) {
		if( val > 7 ) {
			job_log(j, LOG_WARNING, "The interval for key \"%s\" is not between 0 and 7 (inclusive).", key);
			tmptm->tm_sec = -1;
		} else {
			tmptm->tm_wday = (typeof(tmptm->tm_wday)) val;
		}
	} else if (strcasecmp(key, LAUNCH_JOBKEY_CAL_MONTH) == 0) {
		if( val > 12 ) {
			job_log(j, LOG_WARNING, "The interval for key \"%s\" is not between 0 and 12 (inclusive).", key);
			tmptm->tm_sec = -1;
		} else {
			tmptm->tm_mon = (typeof(tmptm->tm_mon)) val;
			tmptm->tm_mon -= 1; /* 4798263 cron compatibility */
		}
	}
}

bool
calendarinterval_new_from_obj(job_t j, launch_data_t obj)
{
	struct cal_dict_walk cdw;

	cdw.j = j;
	memset(&cdw.tmptm, 0, sizeof(0));

	cdw.tmptm.tm_min = -1;
	cdw.tmptm.tm_hour = -1;
	cdw.tmptm.tm_mday = -1;
	cdw.tmptm.tm_wday = -1;
	cdw.tmptm.tm_mon = -1;

	if (!job_assumes(j, obj != NULL)) {
		return false;
	}

	if (unlikely(LAUNCH_DATA_DICTIONARY != launch_data_get_type(obj))) {
		return false;
	}

	launch_data_dict_iterate(obj, calendarinterval_new_from_obj_dict_walk, &cdw);

	if (unlikely(cdw.tmptm.tm_sec == -1)) {
		return false;
	}

	return calendarinterval_new(j, &cdw.tmptm);
}

bool
calendarinterval_new(job_t j, struct tm *w)
{
	struct calendarinterval *ci = calloc(1, sizeof(struct calendarinterval));

	if (!job_assumes(j, ci != NULL)) {
		return false;
	}

	ci->when = *w;
	ci->job = j;

	SLIST_INSERT_HEAD(&j->cal_intervals, ci, sle);
	
	calendarinterval_setalarm(j, ci);

	runtime_add_weak_ref();

	return true;
}

void
calendarinterval_delete(job_t j, struct calendarinterval *ci)
{
	SLIST_REMOVE(&j->cal_intervals, ci, calendarinterval, sle);
	LIST_REMOVE(ci, global_sle);

	free(ci);

	runtime_del_weak_ref();
}

void
calendarinterval_sanity_check(void)
{
	struct calendarinterval *ci = LIST_FIRST(&sorted_calendar_events);
	time_t now = time(NULL);

	if (unlikely(ci && (ci->when_next < now))) {
		jobmgr_assumes(root_jobmgr, raise(SIGUSR1) != -1);
	}
}

void
calendarinterval_callback(void)
{
	struct calendarinterval *ci, *ci_next;
	time_t now = time(NULL);

	LIST_FOREACH_SAFE(ci, &sorted_calendar_events, global_sle, ci_next) {
		job_t j = ci->job;

		if (ci->when_next > now) {
			break;
		}

		LIST_REMOVE(ci, global_sle);
		calendarinterval_setalarm(j, ci);

		j->start_pending = true;
		job_dispatch(j, false);
	}
}

bool
socketgroup_new(job_t j, const char *name, int *fds, size_t fd_cnt, bool junkfds)
{
	struct socketgroup *sg = calloc(1, sizeof(struct socketgroup) + strlen(name) + 1);

	if (!job_assumes(j, sg != NULL)) {
		return false;
	}

	sg->fds = calloc(1, fd_cnt * sizeof(int));
	sg->fd_cnt = fd_cnt;
	sg->junkfds = junkfds;

	if (!job_assumes(j, sg->fds != NULL)) {
		free(sg);
		return false;
	}

	memcpy(sg->fds, fds, fd_cnt * sizeof(int));
	strcpy(sg->name_init, name);

	SLIST_INSERT_HEAD(&j->sockets, sg, sle);

	runtime_add_weak_ref();

	return true;
}

void
socketgroup_delete(job_t j, struct socketgroup *sg)
{
	unsigned int i;

	for (i = 0; i < sg->fd_cnt; i++) {
#if 0
		struct sockaddr_storage ss;
		struct sockaddr_un *sun = (struct sockaddr_un *)&ss;
		socklen_t ss_len = sizeof(ss);

		/* 5480306 */
		if (job_assumes(j, getsockname(sg->fds[i], (struct sockaddr *)&ss, &ss_len) != -1)
				&& job_assumes(j, ss_len > 0) && (ss.ss_family == AF_UNIX)) {
			job_assumes(j, unlink(sun->sun_path) != -1);
			/* We might conditionally need to delete a directory here */
		}
#endif
		job_assumes(j, runtime_close(sg->fds[i]) != -1);
	}

	SLIST_REMOVE(&j->sockets, sg, socketgroup, sle);

	free(sg->fds);
	free(sg);

	runtime_del_weak_ref();
}

void
socketgroup_kevent_mod(job_t j, struct socketgroup *sg, bool do_add)
{
	struct kevent kev[sg->fd_cnt];
	char buf[10000];
	unsigned int i, buf_off = 0;

	if (unlikely(sg->junkfds)) {
		return;
	}

	for (i = 0; i < sg->fd_cnt; i++) {
		EV_SET(&kev[i], sg->fds[i], EVFILT_READ, do_add ? EV_ADD : EV_DELETE, 0, 0, j);
		buf_off += snprintf(buf + buf_off, sizeof(buf) - buf_off, " %d", sg->fds[i]);
	}

	job_log(j, LOG_DEBUG, "%s Sockets:%s", do_add ? "Watching" : "Ignoring", buf);

	job_assumes(j, kevent_bulk_mod(kev, sg->fd_cnt) != -1);

	for (i = 0; i < sg->fd_cnt; i++) {
		job_assumes(j, kev[i].flags & EV_ERROR);
		errno = (typeof(errno)) kev[i].data;
		job_assumes(j, kev[i].data == 0);
	}
}

void
socketgroup_ignore(job_t j, struct socketgroup *sg)
{
	socketgroup_kevent_mod(j, sg, false);
}

void
socketgroup_watch(job_t j, struct socketgroup *sg)
{
	socketgroup_kevent_mod(j, sg, true);
}

void
socketgroup_callback(job_t j)
{
	job_dispatch(j, true);
}

bool
envitem_new(job_t j, const char *k, const char *v, bool global, bool one_shot)
{
	struct envitem *ei = calloc(1, sizeof(struct envitem) + strlen(k) + 1 + strlen(v) + 1);

	if (!job_assumes(j, ei != NULL)) {
		return false;
	}

	strcpy(ei->key_init, k);
	ei->value = ei->key_init + strlen(k) + 1;
	strcpy(ei->value, v);
	ei->one_shot = one_shot;

	if (global) {
		if (SLIST_EMPTY(&j->global_env)) {
			LIST_INSERT_HEAD(&j->mgr->global_env_jobs, j, global_env_sle);
		}
		SLIST_INSERT_HEAD(&j->global_env, ei, sle);
	} else {
		SLIST_INSERT_HEAD(&j->env, ei, sle);
	}

	job_log(j, LOG_DEBUG, "Added environmental variable: %s=%s", k, v);

	return true;
}

void
envitem_delete(job_t j, struct envitem *ei, bool global)
{
	if (global) {
		SLIST_REMOVE(&j->global_env, ei, envitem, sle);
		if (SLIST_EMPTY(&j->global_env)) {
			LIST_REMOVE(j, global_env_sle);
		}
	} else {
		SLIST_REMOVE(&j->env, ei, envitem, sle);
	}

	free(ei);
}

void
envitem_setup(launch_data_t obj, const char *key, void *context)
{
	job_t j = context;

	if (launch_data_get_type(obj) != LAUNCH_DATA_STRING) {
		return;
	}

	if( strncmp(LAUNCHD_TRUSTED_FD_ENV, key, sizeof(LAUNCHD_TRUSTED_FD_ENV) - 1) != 0 ) {
		envitem_new(j, key, launch_data_get_string(obj), j->importing_global_env, false);
	} else {
		job_log(j, LOG_DEBUG, "Ignoring reserved environmental variable: %s", key);
	}
}

void
envitem_setup_one_shot(launch_data_t obj, const char *key, void *context)
{
	job_t j = context;
	
	if (launch_data_get_type(obj) != LAUNCH_DATA_STRING) {
		return;
	}
	
	if( strncmp(LAUNCHD_TRUSTED_FD_ENV, key, sizeof(LAUNCHD_TRUSTED_FD_ENV) - 1) != 0 ) {
		envitem_new(j, key, launch_data_get_string(obj), j->importing_global_env, true);
	} else {
		job_log(j, LOG_DEBUG, "Ignoring reserved environmental variable: %s", key);
	}	
}

bool
limititem_update(job_t j, int w, rlim_t r)
{
	struct limititem *li;

	SLIST_FOREACH(li, &j->limits, sle) {
		if (li->which == w) {
			break;
		}
	}

	if (li == NULL) {
		li = calloc(1, sizeof(struct limititem));

		if (!job_assumes(j, li != NULL)) {
			return false;
		}

		SLIST_INSERT_HEAD(&j->limits, li, sle);

		li->which = w;
	}

	if (j->importing_hard_limits) {
		li->lim.rlim_max = r;
		li->sethard = true;
	} else {
		li->lim.rlim_cur = r;
		li->setsoft = true;
	}

	return true;
}

void
limititem_delete(job_t j, struct limititem *li)
{
	SLIST_REMOVE(&j->limits, li, limititem, sle);

	free(li);
}

#if HAVE_SANDBOX
void
seatbelt_setup_flags(launch_data_t obj, const char *key, void *context)
{
	job_t j = context;

	if (launch_data_get_type(obj) != LAUNCH_DATA_BOOL) {
		job_log(j, LOG_WARNING, "Sandbox flag value must be boolean: %s", key);
		return;
	}

	if (launch_data_get_bool(obj) == false) {
		return;
	}

	if (strcasecmp(key, LAUNCH_JOBKEY_SANDBOX_NAMED) == 0) {
		j->seatbelt_flags |= SANDBOX_NAMED;
	}
}
#endif

void
limititem_setup(launch_data_t obj, const char *key, void *context)
{
	job_t j = context;
	size_t i, limits_cnt = (sizeof(launchd_keys2limits) / sizeof(launchd_keys2limits[0]));
	rlim_t rl;

	if (launch_data_get_type(obj) != LAUNCH_DATA_INTEGER) {
		return;
	}

	rl = launch_data_get_integer(obj);

	for (i = 0; i < limits_cnt; i++) {
		if (strcasecmp(launchd_keys2limits[i].key, key) == 0) {
			break;
		}
	}

	if (i == limits_cnt) {
		return;
	}

	limititem_update(j, launchd_keys2limits[i].val, rl);
}

bool
job_useless(job_t j)
{
	if ((j->legacy_LS_job || j->only_once) && j->start_time != 0) {
		if (j->legacy_LS_job && j->j_port) {
			return false;
		}
		job_log(j, LOG_INFO, "Exited. Was only configured to run once.");
		return true;
	} else if (j->removal_pending) {
		job_log(j, LOG_DEBUG, "Exited while removal was pending.");
		return true;
	} else if (j->mgr->shutting_down && (j->hopefully_exits_first || j->mgr->hopefully_first_cnt == 0)) {
		job_log(j, LOG_DEBUG, "Exited while shutdown in progress. Processes remaining: %lu/%lu", total_children, total_anon_children);
		if( total_children == 0 && !j->anonymous ) {
			job_log(j, LOG_DEBUG | LOG_CONSOLE, "Job was last (non-anonymous) to exit during %s shutdown.", (pid1_magic && j->mgr == root_jobmgr) ? "system" : "job manager");
		} else if( total_anon_children == 0 && j->anonymous ) {
			job_log(j, LOG_DEBUG | LOG_CONSOLE, "Job was last (anonymous) to exit during %s shutdown.", (pid1_magic && j->mgr == root_jobmgr) ? "system" : "job manager");
		}
		return true;
	} else if (j->legacy_mach_job) {
		if (SLIST_EMPTY(&j->machservices)) {
			job_log(j, LOG_INFO, "Garbage collecting");
			return true;
		} else if (!j->checkedin) {
			job_log(j, LOG_WARNING, "Failed to check-in!");
			return true;
		}
	}

	return false;
}

bool
job_keepalive(job_t j)
{
	mach_msg_type_number_t statusCnt;
	mach_port_status_t status;
	struct semaphoreitem *si;
	struct machservice *ms;
	struct stat sb;
	bool good_exit = (WIFEXITED(j->last_exit_status) && WEXITSTATUS(j->last_exit_status) == 0);
	bool is_not_kextd = (do_apple_internal_logging || (strcmp(j->label, "com.apple.kextd") != 0));

	if (unlikely(j->mgr->shutting_down)) {
		return false;
	}

	/*
	 * 5066316
	 *
	 * We definitely need to revisit this after Leopard ships. Please see
	 * launchctl.c for the other half of this hack.
	 */
	if (unlikely((j->mgr->global_on_demand_cnt > 0) && is_not_kextd)) {
		return false;
	}

	if( unlikely(j->needs_kickoff) ) {
		job_log(j, LOG_DEBUG, "KeepAlive check: Job needs to be kicked off on-demand before KeepAlive sets in.");
		return false;
	}

	if (j->start_pending) {
		job_log(j, LOG_DEBUG, "KeepAlive check: Pent-up non-IPC launch criteria.");
		return true;
	}

	if (!j->ondemand) {
		job_log(j, LOG_DEBUG, "KeepAlive check: job configured to run continuously.");
		return true;
	}

	SLIST_FOREACH(ms, &j->machservices, sle) {
		statusCnt = MACH_PORT_RECEIVE_STATUS_COUNT;
		if (mach_port_get_attributes(mach_task_self(), ms->port, MACH_PORT_RECEIVE_STATUS,
					(mach_port_info_t)&status, &statusCnt) != KERN_SUCCESS) {
			continue;
		}
		if (status.mps_msgcount) {
			job_log(j, LOG_DEBUG, "KeepAlive check: %d queued Mach messages on service: %s",
					status.mps_msgcount, ms->name);
			return true;
		}
	}


	SLIST_FOREACH(si, &j->semaphores, sle) {
		bool wanted_state = false;
		int qdir_file_cnt;
		job_t other_j;

		switch (si->why) {
		case NETWORK_UP:
			wanted_state = true;
		case NETWORK_DOWN:
			if (network_up == wanted_state) {
				job_log(j, LOG_DEBUG, "KeepAlive: The network is %s.", wanted_state ? "up" : "down");
				return true;
			}
			break;
		case SUCCESSFUL_EXIT:
			wanted_state = true;
		case FAILED_EXIT:
			if (good_exit == wanted_state) {
				job_log(j, LOG_DEBUG, "KeepAlive: The exit state was %s.", wanted_state ? "successful" : "failure");
				return true;
			}
			break;
		case OTHER_JOB_ENABLED:
			wanted_state = true;
		case OTHER_JOB_DISABLED:
			if ((bool)job_find(si->what) == wanted_state) {
				job_log(j, LOG_DEBUG, "KeepAlive: The following job is %s: %s", wanted_state ? "enabled" : "disabled", si->what);
				return true;
			}
			break;
		case OTHER_JOB_ACTIVE:
			wanted_state = true;
		case OTHER_JOB_INACTIVE:
			if ((other_j = job_find(si->what))) {
				if ((bool)other_j->p == wanted_state) {
					job_log(j, LOG_DEBUG, "KeepAlive: The following job is %s: %s", wanted_state ? "active" : "inactive", si->what);
					return true;
				}
			}
			break;
		case PATH_EXISTS:
			wanted_state = true;
		case PATH_MISSING:
			if ((bool)(stat(si->what, &sb) == 0) == wanted_state) {
				job_log(j, LOG_DEBUG, "KeepAlive: The following path %s: %s", wanted_state ? "exists" : "is missing", si->what);
				return true;
			} else {
				if( wanted_state ) { /* File is not there but we wish it was. */
					if( si->fd != -1 && !si->watching_parent ) { /* Need to be watching the parent now. */
						job_assumes(j, runtime_close(si->fd) == 0);
						si->fd = -1;
						semaphoreitem_watch(j, si);
					}
				} else { /* File is there but we wish it wasn't. */
					if( si->fd != -1 && si->watching_parent ) { /* Need to watch the file now. */
						job_assumes(j, runtime_close(si->fd) == 0);
						si->fd = -1;
						semaphoreitem_watch(j, si);
					}
				}
			}
			break;
		case PATH_CHANGES:
			break;
		case DIR_NOT_EMPTY:
			if (-1 == (qdir_file_cnt = dir_has_files(j, si->what))) {
				job_log_error(j, LOG_ERR, "Failed to count the number of files in \"%s\"", si->what);
			} else if (qdir_file_cnt > 0) {
				job_log(j, LOG_DEBUG, "KeepAlive: Directory is not empty: %s", si->what);
				return true;
			}
			break;
		}
	}

	return false;
}

const char *
job_active(job_t j)
{
	struct machservice *ms;
	if (j->p) {
		return "PID is still valid";
	}

	if (j->mgr->shutting_down && j->log_redirect_fd) {
		job_assumes(j, runtime_close(j->log_redirect_fd) != -1);
		j->log_redirect_fd = 0;
	}

	if (j->log_redirect_fd) {
		if (job_assumes(j, j->legacy_LS_job)) {
			return "Standard out/error is still valid";
		} else {
			job_assumes(j, runtime_close(j->log_redirect_fd) != -1);
			j->log_redirect_fd = 0;
		}
	}

	if (j->priv_port_has_senders) {
		return "Privileged Port still has outstanding senders";
	}

	SLIST_FOREACH(ms, &j->machservices, sle) {
		if (ms->recv && machservice_active(ms)) {
			return "Mach service is still active";
		}
	}

	return NULL;
}

void
machservice_watch(job_t j, struct machservice *ms)
{
	if (ms->recv) {
		job_assumes(j, runtime_add_mport(ms->port, NULL, 0) == KERN_SUCCESS);
	}
}

void
machservice_ignore(job_t j, struct machservice *ms)
{
	job_assumes(j, runtime_remove_mport(ms->port) == KERN_SUCCESS);
}

void
machservice_resetport(job_t j, struct machservice *ms)
{
	LIST_REMOVE(ms, port_hash_sle);
	job_assumes(j, launchd_mport_close_recv(ms->port) == KERN_SUCCESS);
	job_assumes(j, launchd_mport_deallocate(ms->port) == KERN_SUCCESS);
	ms->gen_num++;
	job_assumes(j, launchd_mport_create_recv(&ms->port) == KERN_SUCCESS);
	job_assumes(j, launchd_mport_make_send(ms->port) == KERN_SUCCESS);
	LIST_INSERT_HEAD(&port_hash[HASH_PORT(ms->port)], ms, port_hash_sle);
}

struct machservice *
machservice_new(job_t j, const char *name, mach_port_t *serviceport, bool pid_local)
{
	struct machservice *ms = calloc(1, sizeof(struct machservice) + strlen(name) + 1);

	if (!job_assumes(j, ms != NULL)) {
		return NULL;
	}

	strcpy((char *)ms->name, name);
	ms->job = j;
	ms->gen_num = 1;
	ms->per_pid = pid_local;

	if (likely(*serviceport == MACH_PORT_NULL)) {
		if (!job_assumes(j, launchd_mport_create_recv(&ms->port) == KERN_SUCCESS)) {
			goto out_bad;
		}

		if (!job_assumes(j, launchd_mport_make_send(ms->port) == KERN_SUCCESS)) {
			goto out_bad2;
		}
		*serviceport = ms->port;
		ms->recv = true;
	} else {
		ms->port = *serviceport;
		ms->isActive = true;
	}

	SLIST_INSERT_HEAD(&j->machservices, ms, sle);
	
	jobmgr_t jm_to_insert = j->mgr;
	if( g_flat_mach_namespace ) {
		jm_to_insert = (j->mgr->properties & BOOTSTRAP_PROPERTY_EXPLICITSUBSET) ? j->mgr : root_jobmgr;
	}
	
	LIST_INSERT_HEAD(&jm_to_insert->ms_hash[hash_ms(ms->name)], ms, name_hash_sle);	
	LIST_INSERT_HEAD(&port_hash[HASH_PORT(ms->port)], ms, port_hash_sle);

	job_log(j, LOG_DEBUG, "Mach service added%s: %s", (j->mgr->properties & BOOTSTRAP_PROPERTY_EXPLICITSUBSET) ? " to private namespace" : "", name);

	return ms;
out_bad2:
	job_assumes(j, launchd_mport_close_recv(ms->port) == KERN_SUCCESS);
out_bad:
	free(ms);
	return NULL;
}

bootstrap_status_t
machservice_status(struct machservice *ms)
{
	if (ms->isActive) {
		return BOOTSTRAP_STATUS_ACTIVE;
	} else if (ms->job->ondemand) {
		return BOOTSTRAP_STATUS_ON_DEMAND;
	} else {
		return BOOTSTRAP_STATUS_INACTIVE;
	}
}

void
job_setup_exception_port(job_t j, task_t target_task)
{
	struct machservice *ms;
	thread_state_flavor_t f = 0;
	mach_port_t exc_port = the_exception_server;

	if (unlikely(j->alt_exc_handler)) {
		ms = jobmgr_lookup_service(j->mgr, j->alt_exc_handler, true, 0);
		if (likely(ms)) {
			exc_port = machservice_port(ms);
		} else {
			job_log(j, LOG_WARNING, "Falling back to default Mach exception handler. Could not find: %s", j->alt_exc_handler);
		}
	} else if (unlikely(j->internal_exc_handler)) {
		exc_port = runtime_get_kernel_port();
	} else if (unlikely(!exc_port)) {
		return;
	}

#if defined (__ppc__) || defined(__ppc64__)
	f = PPC_THREAD_STATE64;
#elif defined(__i386__) || defined(__x86_64__)
	f = x86_THREAD_STATE;
#elif defined(__arm__)
	f = ARM_THREAD_STATE;
#else
#error "unknown architecture"
#endif

	if (likely(target_task)) {
		job_assumes(j, task_set_exception_ports(target_task, EXC_MASK_CRASH, exc_port, EXCEPTION_STATE_IDENTITY | MACH_EXCEPTION_CODES, f) == KERN_SUCCESS);
	} else if (pid1_magic && the_exception_server) {
		mach_port_t mhp = mach_host_self();
		job_assumes(j, host_set_exception_ports(mhp, EXC_MASK_CRASH, the_exception_server, EXCEPTION_STATE_IDENTITY | MACH_EXCEPTION_CODES, f) == KERN_SUCCESS);
		job_assumes(j, launchd_mport_deallocate(mhp) == KERN_SUCCESS);
	}
}

void
job_set_exception_port(job_t j, mach_port_t port)
{
	if (unlikely(!the_exception_server)) {
		the_exception_server = port;
		job_setup_exception_port(j, 0);
	} else {
		job_log(j, LOG_WARNING, "The exception server is already claimed!");
	}
}

void
machservice_setup_options(launch_data_t obj, const char *key, void *context)
{
	struct machservice *ms = context;
	mach_port_t mhp = mach_host_self();
	int which_port;
	bool b;

	if (!job_assumes(ms->job, mhp != MACH_PORT_NULL)) {
		return;
	}

	switch (launch_data_get_type(obj)) {
	case LAUNCH_DATA_INTEGER:
		which_port = (int)launch_data_get_integer(obj); /* XXX we should bound check this... */
		if (strcasecmp(key, LAUNCH_JOBKEY_MACH_TASKSPECIALPORT) == 0) {
			switch (which_port) {
			case TASK_KERNEL_PORT:
			case TASK_HOST_PORT:
			case TASK_NAME_PORT:
			case TASK_BOOTSTRAP_PORT:
			/* I find it a little odd that zero isn't reserved in the header.
			 * Normally Mach is fairly good about this convention... */
			case 0:
				job_log(ms->job, LOG_WARNING, "Tried to set a reserved task special port: %d", which_port);
				break;
			default:
				ms->special_port_num = which_port;
				SLIST_INSERT_HEAD(&special_ports, ms, special_port_sle);
				break;
			}
		} else if (strcasecmp(key, LAUNCH_JOBKEY_MACH_HOSTSPECIALPORT) == 0 && pid1_magic) {
			if (which_port > HOST_MAX_SPECIAL_KERNEL_PORT) {
				job_assumes(ms->job, (errno = host_set_special_port(mhp, which_port, ms->port)) == KERN_SUCCESS);
			} else {
				job_log(ms->job, LOG_WARNING, "Tried to set a reserved host special port: %d", which_port);
			}
		}
	case LAUNCH_DATA_BOOL:
		b = launch_data_get_bool(obj);
		if (strcasecmp(key, LAUNCH_JOBKEY_MACH_ENTERKERNELDEBUGGERONCLOSE) == 0) {
			ms->debug_on_close = b;
		} else if (strcasecmp(key, LAUNCH_JOBKEY_MACH_RESETATCLOSE) == 0) {
			ms->reset = b;
		} else if (strcasecmp(key, LAUNCH_JOBKEY_MACH_HIDEUNTILCHECKIN) == 0) {
			ms->hide = b;
		} else if (strcasecmp(key, LAUNCH_JOBKEY_MACH_EXCEPTIONSERVER) == 0) {
			job_set_exception_port(ms->job, ms->port);
		} else if (strcasecmp(key, LAUNCH_JOBKEY_MACH_KUNCSERVER) == 0) {
			ms->kUNCServer = b;
			job_assumes(ms->job, host_set_UNDServer(mhp, ms->port) == KERN_SUCCESS);
		}
		break;
	case LAUNCH_DATA_STRING:
		if( strcasecmp(key, LAUNCH_JOBKEY_MACH_DRAINMESSAGESONCRASH) == 0 ) {
			const char *option = launch_data_get_string(obj);
			if( strcasecmp(option, "One") == 0 ) {
				ms->drain_one_on_crash = true;
			} else if( strcasecmp(option, "All") == 0 ) {
				ms->drain_all_on_crash = true;
			}
		}
		break;
	case LAUNCH_DATA_DICTIONARY:
		job_set_exception_port(ms->job, ms->port);
		break;
	default:
		break;
	}

	job_assumes(ms->job, launchd_mport_deallocate(mhp) == KERN_SUCCESS);
}

void
machservice_setup(launch_data_t obj, const char *key, void *context)
{
	job_t j = context;
	struct machservice *ms;
	mach_port_t p = MACH_PORT_NULL;

	if (unlikely(ms = jobmgr_lookup_service(j->mgr, key, false, 0))) {
		job_log(j, LOG_WARNING, "Conflict with job: %s over Mach service: %s", ms->job->label, key);
		return;
	}

	if (!job_assumes(j, (ms = machservice_new(j, key, &p, false)) != NULL)) {
		return;
	}

	ms->isActive = false;

	if (launch_data_get_type(obj) == LAUNCH_DATA_DICTIONARY) {
		launch_data_dict_iterate(obj, machservice_setup_options, ms);
	}
}

jobmgr_t
jobmgr_do_garbage_collection(jobmgr_t jm)
{
	jobmgr_t jmi = NULL, jmn = NULL;
	SLIST_FOREACH_SAFE(jmi, &jm->submgrs, sle, jmn) {
		jobmgr_do_garbage_collection(jmi);
	}

	if( !jm->shutting_down ) {
		return jm;
	}
	
	if( SLIST_EMPTY(&jm->submgrs) ) {
		jobmgr_log(jm, LOG_DEBUG, "No submanagers left.");
	} else {
		jobmgr_log(jm, LOG_DEBUG, "Still have submanagers.");
	}
	
	int phase = -1;
	for( phase = jm->shutdown_phase; phase < JOBMGR_PHASE_LAST; phase++ ) {
		if( phase == JOBMGR_PHASE_HOPEFULLY_EXITS_LAST ) {
			if( jm == root_jobmgr ) {
				simulate_pid1_crash();
			}
			
			if( jm == root_jobmgr && pid1_magic && !jm->killed_stray_jobs ) {
				jobmgr_log_stray_children(jm, true);
				jm->killed_stray_jobs = true;
			}
		}

		uint32_t unkilled_cnt = 0;
		job_t ji = NULL, jn = NULL;
		LIST_FOREACH_SAFE( ji, &jm->jobs, sle, jn ) {
			if( phase == JOBMGR_PHASE_HOPEFULLY_EXITS_FIRST && !ji->hopefully_exits_first ) {
				continue;
			} else if( phase == JOBMGR_PHASE_NORMAL ) {
				if( ji->migratory ) {
					/* If we're shutting down, release the hold migratory jobs
					 * have on us.
					 */
					job_remove(ji);			
				}
				
				if( ji->hopefully_exits_first || ji->hopefully_exits_last ) {
					continue;
				}
			} else if( phase == JOBMGR_PHASE_HOPEFULLY_EXITS_LAST && !ji->hopefully_exits_last ) {
				continue;
			}
			
			if( ji->anonymous ) {
				continue;
			}
			
			const char *active = job_active(ji);
			if( !active ) {
				job_log(ji, LOG_DEBUG, "Job is inactive. Removing.");
				job_remove(ji);
			} else {
				if( ji->p ) {
					if( !ji->stopped ) {
						job_log(ji, LOG_DEBUG, "Stopping job.");
						job_stop(ji);
						unkilled_cnt++;
					} else {
						if( ji->clean_kill ) {
							job_log(ji, LOG_DEBUG, "Job was clean and sent SIGKILL.");
							if( !ji->clean_exit_timer_expired ) {
								/* Give jobs that were clean and sent SIGKILL 1 second to exit after receipt. */
								unkilled_cnt++;
							} else {
								job_log(ji, LOG_ERR, "Job was clean, killed and has not exited after 1 second. Moving on.");
							}
						} else {
							job_log(ji, LOG_DEBUG, "Job was sent SIGTERM%s.", ji->sent_sigkill ? " and SIGKILL" : "");
							unkilled_cnt += !ji->sent_sigkill;
						}
					}
				} else {
					job_log(ji, LOG_DEBUG, "Job is active: %s", active);
				}
			}
		} /* LIST_FOREACH_SAFE */
		
		if( unkilled_cnt == 0 ) {
			jobmgr_log(jm, LOG_DEBUG, "Done with the %s bucket, advancing.", s_phases[jm->shutdown_phase]);
			jm->shutdown_phase++;
		} else {
			jobmgr_log(jm, LOG_DEBUG, "Still %u unkilled job%s in %s bucket.", unkilled_cnt, unkilled_cnt > 1 ? "s" : "", s_phases[jm->shutdown_phase]);
			phase = JOBMGR_PHASE_LAST;
		}
	} /* for */
	
	jobmgr_t r = jm;
	if( jm->shutdown_phase > JOBMGR_PHASE_HOPEFULLY_EXITS_LAST && SLIST_EMPTY(&jm->submgrs) ) {
		jobmgr_log(jm, LOG_DEBUG, "Removing.");
		jobmgr_log_stray_children(jm, false);
		jobmgr_remove(jm);
		r = NULL;
	}
	
	return r;
}

void
jobmgr_kill_stray_children(jobmgr_t jm, pid_t *p, size_t np)
{
#if 1
	/* I maintain that stray processes should be at the mercy of launchd during shutdown,
	 * but nevertheless, things like diskimages-helper can stick around, and SIGKILLing 
	 * them can result in data loss. So we send SIGTERM to all the strays and don't wait
	 * for them to exit before moving on.
	 * 
	 * See rdar://problem/6562592
	 */
	size_t i = 0;
	for( i = 0; i < np; i++ ) {
		if( p[i] != 0 ) {
			jobmgr_log(jm, LOG_DEBUG | LOG_CONSOLE, "Sending SIGTERM to PID %u and continuing...", p[i]);
			jobmgr_assumes(jm, runtime_kill(p[i], SIGTERM) != -1);
		}
	}
#else
	struct timespec tts = { 2, 0 }; /* Wait 2 seconds for stray children to die after being SIGTERM'ed. */
	struct timespec kts = { 1, 0 }; /* Wait 1 second for stray children to die after being SIGKILL'ed. */
	uint64_t start, end, nanosec;
	struct kevent kev;
	int r, kq = kqueue();
	
	if (!jobmgr_assumes(jm, kq != -1)) {
		return;
	}

	start = runtime_get_opaque_time();
	size_t i = 0, n2t = 0;
	for( i = 0; i < np; i++ ) {
		if( p[i] != 0 ) {
			EV_SET(&kev, p[i], EVFILT_PROC, EV_ADD, NOTE_EXIT, 0, 0);
			
			if( jobmgr_assumes(jm, kevent(kq, &kev, 1, NULL, 0, NULL) != -1) ) {
				jobmgr_assumes(jm, runtime_kill(p[i], SIGTERM) != -1);
				n2t++;
			} else {
				jobmgr_log(jm, LOG_DEBUG | LOG_CONSOLE, "Disregarding PID %u and continuing.", p[i]);
				p[i] = 0;
			}
		}
	}

	while( n2t > 0 && (r = kevent(kq, NULL, 0, &kev, 1, &tts)) ) {
		int status = 0;
		waitpid((pid_t)kev.ident, &status, WNOHANG);
		
		end = runtime_get_opaque_time();
		nanosec = runtime_opaque_time_to_nano(end - start);
		jobmgr_log(jm, LOG_DEBUG | LOG_CONSOLE, "PID %u died after %llu nanoseconds.", (pid_t)kev.ident, nanosec);
		
		for( i = 0; i < np; i++ ) {
			p[i] = ( p[i] == (pid_t)kev.ident ) ? 0 : p[i];
		}
	}

	size_t n2k = 0;
	for( i = 0; i < np; i++ ) {
		if( p[i] != 0 ) {
			jobmgr_assumes(jm, runtime_kill(p[i], SIGKILL) != -1);
			n2k++;
		}
	}

	while( n2k > 0 && (r = kevent(kq, NULL, 0, &kev, 1, &kts)) ) {
		int status = 0;
		waitpid((pid_t)kev.ident, &status, WNOHANG);
		
		end = runtime_get_opaque_time();
		nanosec = runtime_opaque_time_to_nano(end - start);
		jobmgr_log(jm, LOG_DEBUG | LOG_CONSOLE, "PID %u was killed and died after %llu nanoseconds.", (pid_t)kev.ident, nanosec);
		
		for( i = 0; i < np; i++ ) {
			p[i] = ( p[i] == (pid_t)kev.ident ) ? 0 : p[i];
		}
	}
	
	for( i = 0; i < np; i++ ) {
		if( p[i] != 0 ) {
			jobmgr_log(jm, LOG_NOTICE | LOG_CONSOLE, "PID %u did not die after being SIGKILL'ed 1 second ago.", p[i]);
		}
	}
#endif
}

void
jobmgr_log_stray_children(jobmgr_t jm, bool kill_strays)
{
	int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL };
	size_t i, kp_cnt = 0, kp_skipped = 0, len = sizeof(struct kinfo_proc) * get_kern_max_proc();
	struct kinfo_proc *kp;

	if (likely(jm->parentmgr || !pid1_magic)) {
		return;
	}

	if (!jobmgr_assumes(jm, (kp = malloc(len)) != NULL)) {
		return;
	}

	runtime_ktrace0(RTKT_LAUNCHD_FINDING_ALL_STRAYS);

	if (!jobmgr_assumes(jm, sysctl(mib, 3, kp, &len, NULL, 0) != -1)) {
		goto out;
	}

	kp_cnt = len / sizeof(struct kinfo_proc);
	pid_t *ps = (pid_t *)calloc(sizeof(pid_t), kp_cnt);
	
	for (i = 0; i < kp_cnt; i++) {
		pid_t p_i = kp[i].kp_proc.p_pid;
		pid_t pp_i = kp[i].kp_eproc.e_ppid;
		pid_t pg_i = kp[i].kp_eproc.e_pgid;
		const char *z = (kp[i].kp_proc.p_stat == SZOMB) ? "zombie " : "";
		const char *n = kp[i].kp_proc.p_comm;

		if (unlikely(p_i == 0 || p_i == 1)) {
			kp_skipped++;
			continue;
		}
		
		/* We might have some jobs hanging around that we've decided to shut down in spite of. */
		job_t j = jobmgr_find_by_pid(jm, p_i, false);
		if( !j || (j && j->anonymous) ) {
			jobmgr_log(jm, LOG_INFO | LOG_CONSOLE, "Stray %s%s at shutdown: PID %u PPID %u PGID %u %s", z, j ? "anonymous job" : "process", p_i, pp_i, pg_i, n);
			
			int status = 0;
			if( pp_i == getpid() && !jobmgr_assumes(jm, kp[i].kp_proc.p_stat != SZOMB) ) {
				if( jobmgr_assumes(jm, waitpid(p_i, &status, WNOHANG) == 0) ) {
					jobmgr_log(jm, LOG_INFO | LOG_CONSOLE, "Unreaped zombie stray exited with status %i.", WEXITSTATUS(status));
				}
				kp_skipped++;
			} else {
				job_t leader = jobmgr_find_by_pid(jm, pg_i, false);
				/* See rdar://problem/6745714. Some jobs have child processes that back kernel state,
				 * so we don't want to terminate them. Long-term, I'd really like to provide shutdown
				 * hints to the kernel along the way, so that it could shutdown certain subsystems when
				 * their userspace emissaries go away, before the call to reboot(2).
				 */
				if( leader && leader->ignore_pg_at_shutdown ) {
					kp_skipped++;
				} else {
					ps[i] = p_i;
				}
			}
		} else {
			kp_skipped++;
		}
	}

	if( (kp_cnt - kp_skipped > 0) && kill_strays ) {
		jobmgr_kill_stray_children(jm, ps, kp_cnt);
	}

	free(ps);
out:
	free(kp);
}

jobmgr_t 
jobmgr_parent(jobmgr_t jm)
{
	return jm->parentmgr;
}

void
job_uncork_fork(job_t j)
{
	pid_t c = j->p;

	job_log(j, LOG_DEBUG, "Uncorking the fork().");
	/* this unblocks the child and avoids a race
	 * between the above fork() and the kevent_mod() */
	job_assumes(j, write(j->fork_fd, &c, sizeof(c)) == sizeof(c));
	job_assumes(j, runtime_close(j->fork_fd) != -1);
	j->fork_fd = 0;
}

jobmgr_t 
jobmgr_new(jobmgr_t jm, mach_port_t requestorport, mach_port_t transfer_port, bool sflag, const char *name, bool no_init, mach_port_t session_port)
{
	mach_msg_size_t mxmsgsz;
	job_t bootstrapper = NULL;
	jobmgr_t jmr;

	launchd_assert(offsetof(struct jobmgr_s, kqjobmgr_callback) == 0);

	if (unlikely(jm && requestorport == MACH_PORT_NULL)) {
		jobmgr_log(jm, LOG_ERR, "Mach sub-bootstrap create request requires a requester port");
		return NULL;
	}

	jmr = calloc(1, sizeof(struct jobmgr_s) + (name ? (strlen(name) + 1) : NAME_MAX + 1));

	if (!jobmgr_assumes(jm, jmr != NULL)) {
		return NULL;
	}

	jmr->kqjobmgr_callback = jobmgr_callback;
	strcpy(jmr->name_init, name ? name : "Under construction");

	jmr->req_port = requestorport;

	if ((jmr->parentmgr = jm)) {
		SLIST_INSERT_HEAD(&jm->submgrs, jmr, sle);
	}

	if (jm && !jobmgr_assumes(jmr, launchd_mport_notify_req(jmr->req_port, MACH_NOTIFY_DEAD_NAME) == KERN_SUCCESS)) {
		goto out_bad;
	}

	if (transfer_port != MACH_PORT_NULL) {
		jobmgr_assumes(jmr, jm != NULL);
		jmr->jm_port = transfer_port;
	} else if (!jm && !pid1_magic) {
		char *trusted_fd = getenv(LAUNCHD_TRUSTED_FD_ENV);
		name_t service_buf;

		snprintf(service_buf, sizeof(service_buf), "com.apple.launchd.peruser.%u", getuid());

		if (!jobmgr_assumes(jmr, bootstrap_check_in(bootstrap_port, service_buf, &jmr->jm_port) == 0)) {
			goto out_bad;
		}

		if (trusted_fd) {
			int dfd, lfd = (int) strtol(trusted_fd, NULL, 10);

			if ((dfd = dup(lfd)) >= 0) {
				jobmgr_assumes(jmr, runtime_close(dfd) != -1);
				jobmgr_assumes(jmr, runtime_close(lfd) != -1);
			}

			unsetenv(LAUNCHD_TRUSTED_FD_ENV);
		}

		/* cut off the Libc cache, we don't want to deadlock against ourself */
		inherited_bootstrap_port = bootstrap_port;
		bootstrap_port = MACH_PORT_NULL;
		launchd_assert(launchd_mport_notify_req(inherited_bootstrap_port, MACH_NOTIFY_DEAD_NAME) == KERN_SUCCESS);

		/* We set this explicitly as we start each child */
		launchd_assert(launchd_set_bport(MACH_PORT_NULL) == KERN_SUCCESS);
	} else if (!jobmgr_assumes(jmr, launchd_mport_create_recv(&jmr->jm_port) == KERN_SUCCESS)) {
		goto out_bad;
	}

	if (!name) {
		sprintf(jmr->name_init, "%u", MACH_PORT_INDEX(jmr->jm_port));
	}

	/* Sigh... at the moment, MIG has maxsize == sizeof(reply union) */
	mxmsgsz = (typeof(mxmsgsz)) sizeof(union __RequestUnion__job_mig_protocol_vproc_subsystem);
	if (job_mig_protocol_vproc_subsystem.maxsize > mxmsgsz) {
		mxmsgsz = job_mig_protocol_vproc_subsystem.maxsize;
	}

	if (!jm) {
		jobmgr_assumes(jmr, kevent_mod(SIGTERM, EVFILT_SIGNAL, EV_ADD, 0, 0, jmr) != -1);
		jobmgr_assumes(jmr, kevent_mod(SIGUSR1, EVFILT_SIGNAL, EV_ADD, 0, 0, jmr) != -1);
		jobmgr_assumes(jmr, kevent_mod(SIGUSR2, EVFILT_SIGNAL, EV_ADD, 0, 0, jmr) != -1);
		jobmgr_assumes(jmr, kevent_mod(0, EVFILT_FS, EV_ADD, VQ_MOUNT|VQ_UNMOUNT|VQ_UPDATE, 0, jmr) != -1);
	}

	if (name && !no_init) {
		bootstrapper = jobmgr_init_session(jmr, name, sflag);
	}

	if (!bootstrapper || !bootstrapper->weird_bootstrap) {
		if (!jobmgr_assumes(jmr, runtime_add_mport(jmr->jm_port, protocol_vproc_server, mxmsgsz) == KERN_SUCCESS)) {
			goto out_bad;
		}
	}

	STAILQ_INIT(&jmr->pending_samples);

	jobmgr_log(jmr, LOG_DEBUG, "Created job manager%s%s", jm ? " with parent: " : ".", jm ? jm->name : "");

	if (bootstrapper) {
		bootstrapper->audit_session = session_port;
		if( session_port != MACH_PORT_NULL ) {
			mach_port_mod_refs(mach_task_self(), session_port, MACH_PORT_RIGHT_SEND, 1);
		}
		
		jobmgr_log(jmr, LOG_DEBUG, "Bootstrapping new job manager with audit session %u", session_port);
		jobmgr_assumes(jmr, job_dispatch(bootstrapper, true) != NULL);
	}

	if (jmr->parentmgr) {
		runtime_add_weak_ref();
	}

	return jmr;

out_bad:
	if (jmr) {
		jobmgr_remove(jmr);
	}
	return NULL;
}

job_t
jobmgr_init_session(jobmgr_t jm, const char *session_type, bool sflag)
{
	const char *bootstrap_tool[] = { "/bin/launchctl", "bootstrap", "-S", session_type, sflag ? "-s" : NULL, NULL };
	char thelabel[1000];
	job_t bootstrapper;

	snprintf(thelabel, sizeof(thelabel), "com.apple.launchctl.%s", session_type);
	bootstrapper = job_new(jm, thelabel, NULL, bootstrap_tool);
	
	if( jobmgr_assumes(jm, bootstrapper != NULL) && (jm->parentmgr || !pid1_magic) ) {
		bootstrapper->is_bootstrapper = true;
		char buf[100];

		/* <rdar://problem/5042202> launchd-201: can't ssh in with AFP OD account (hangs) */
		snprintf(buf, sizeof(buf), "0x%X:0:0", getuid());
		envitem_new(bootstrapper, "__CF_USER_TEXT_ENCODING", buf, false, false);
		bootstrapper->weird_bootstrap = true;
		jobmgr_assumes(jm, job_setup_machport(bootstrapper));
	} else if( bootstrapper && strncmp(session_type, VPROCMGR_SESSION_SYSTEM, sizeof(VPROCMGR_SESSION_SYSTEM)) == 0 ) {
		bootstrapper->is_bootstrapper = true;
		if( jobmgr_assumes(jm, pid1_magic) ) {
			/* Have our system bootstrapper print out to the console. */
			bootstrapper->stdoutpath = strdup(_PATH_CONSOLE);
			bootstrapper->stderrpath = strdup(_PATH_CONSOLE);

			if( g_console ) {
				jobmgr_assumes(jm, kevent_mod((uintptr_t)fileno(g_console), EVFILT_VNODE, EV_ADD | EV_ONESHOT, NOTE_REVOKE, 0, jm) != -1);
			}
		}
	}

	jm->session_initialized = true;

	return bootstrapper;
}

jobmgr_t
jobmgr_delete_anything_with_port(jobmgr_t jm, mach_port_t port)
{
	struct machservice *ms, *next_ms;
	jobmgr_t jmi, jmn;

	/* Mach ports, unlike Unix descriptors, are reference counted. In other
	 * words, when some program hands us a second or subsequent send right
	 * to a port we already have open, the Mach kernel gives us the same
	 * port number back and increments an reference count associated with
	 * the port. This forces us, when discovering that a receive right at
	 * the other end has been deleted, to wander all of our objects to see
	 * what weird places clients might have handed us the same send right
	 * to use.
	 */

	if (jm == root_jobmgr) {
		if (port == inherited_bootstrap_port) {
			jobmgr_assumes(jm, launchd_mport_deallocate(port) == KERN_SUCCESS);
			inherited_bootstrap_port = MACH_PORT_NULL;

			return jobmgr_shutdown(jm);
		}

		LIST_FOREACH_SAFE(ms, &port_hash[HASH_PORT(port)], port_hash_sle, next_ms) {
			if (ms->port == port && !ms->recv) {
				machservice_delete(ms->job, ms, true);
			}
		}
	}

	SLIST_FOREACH_SAFE(jmi, &jm->submgrs, sle, jmn) {
		jobmgr_delete_anything_with_port(jmi, port);
	}

	if (jm->req_port == port) {
		jobmgr_log(jm, LOG_DEBUG, "Request port died: %i", MACH_PORT_INDEX(port));
		return jobmgr_shutdown(jm);
	}

	return jm;
}

struct machservice *
jobmgr_lookup_service(jobmgr_t jm, const char *name, bool check_parent, pid_t target_pid)
{
	struct machservice *ms;
	job_t target_j;

	jobmgr_log(jm, LOG_DEBUG, "Looking up %sservice %s", target_pid ? "per-PID " : "", name);

	if (target_pid) {
		/* This is a hack to let FileSyncAgent look up per-PID Mach services from the Background
		 * bootstrap in other bootstraps.
		 */
		
		/* Start in the given bootstrap. */
		if( unlikely((target_j = jobmgr_find_by_pid(jm, target_pid, false)) == NULL) ) {
			/* If we fail, do a deep traversal. */
			if (unlikely((target_j = jobmgr_find_by_pid_deep(root_jobmgr, target_pid, true)) == NULL)) {
				jobmgr_log(jm, LOG_DEBUG, "Didn't find PID %i", target_pid);
				return NULL;
			}
		}
		
		SLIST_FOREACH(ms, &target_j->machservices, sle) {
			if (ms->per_pid && strcmp(name, ms->name) == 0) {
				return ms;
			}
		}

		job_log(target_j, LOG_DEBUG, "Didn't find per-PID Mach service: %s", name);
		return NULL;
	}
	
	jobmgr_t jm_to_search = ( g_flat_mach_namespace && !(jm->properties & BOOTSTRAP_PROPERTY_EXPLICITSUBSET) ) ? root_jobmgr : jm;
	LIST_FOREACH(ms, &jm_to_search->ms_hash[hash_ms(name)], name_hash_sle) {
		if (!ms->per_pid && strcmp(name, ms->name) == 0) {
			return ms;
		}
	}

	if (jm->parentmgr == NULL || !check_parent) {
		return NULL;
	}

	return jobmgr_lookup_service(jm->parentmgr, name, true, 0);
}

mach_port_t
machservice_port(struct machservice *ms)
{
	return ms->port;
}

job_t 
machservice_job(struct machservice *ms)
{
	return ms->job;
}

bool
machservice_hidden(struct machservice *ms)
{
	return ms->hide;
}

bool
machservice_active(struct machservice *ms)
{
	return ms->isActive;
}

const char *
machservice_name(struct machservice *ms)
{
	return ms->name;
}

void
machservice_drain_port(struct machservice *ms)
{
	bool drain_one = ms->drain_one_on_crash;
	bool drain_all = ms->drain_all_on_crash;
	
	if( !job_assumes(ms->job, (drain_one || drain_all) == true) ) {
		return;
	}

	job_log(ms->job, LOG_INFO, "Draining %s...", ms->name);
	
	char req_buff[sizeof(union __RequestUnion__catch_mach_exc_subsystem) * 2];
	char rep_buff[sizeof(union __ReplyUnion__catch_mach_exc_subsystem)];
	mig_reply_error_t *req_hdr = (mig_reply_error_t *)&req_buff;
	mig_reply_error_t *rep_hdr = (mig_reply_error_t *)&rep_buff;

	mach_msg_return_t mr = ~MACH_MSG_SUCCESS;
	
	do {
		/* This should be a direct check on the Mach service to see if it's an exception-handling
		 * port, and it will break things if ReportCrash or SafetyNet start advertising other
		 * Mach services. But for now, it should be okay.
		 */
		if( ms->job->alt_exc_handler || ms->job->internal_exc_handler ) {
			mr = launchd_exc_runtime_once(ms->port, sizeof(req_buff), sizeof(rep_buff), req_hdr, rep_hdr, 0);
		} else {
			mach_msg_options_t options =	MACH_RCV_MSG		|
											MACH_RCV_TIMEOUT	;

			mr = mach_msg((mach_msg_header_t *)req_hdr, options, 0, sizeof(req_buff), ms->port, 0, MACH_PORT_NULL);
			switch( mr ) {
				case MACH_MSG_SUCCESS	:
					mach_msg_destroy((mach_msg_header_t *)req_hdr);
					break;
				case MACH_RCV_TIMED_OUT	:
					break;
				case MACH_RCV_TOO_LARGE	:
					runtime_syslog(LOG_WARNING, "Tried to receive message that was larger than %lu bytes", sizeof(req_buff));
					break;
				default					:
					break;
			}
		}
	} while( drain_all && mr != MACH_RCV_TIMED_OUT );
}

void
machservice_delete(job_t j, struct machservice *ms, bool port_died)
{
	if (unlikely(ms->debug_on_close)) {
		job_log(j, LOG_NOTICE, "About to enter kernel debugger because of Mach port: 0x%x", ms->port);
		job_assumes(j, host_reboot(mach_host_self(), HOST_REBOOT_DEBUGGER) == KERN_SUCCESS);
	}

	if (ms->recv && job_assumes(j, !machservice_active(ms))) {
		job_log(j, LOG_DEBUG, "Closing receive right for %s", ms->name);
		job_assumes(j, launchd_mport_close_recv(ms->port) == KERN_SUCCESS);
	}

	job_assumes(j, launchd_mport_deallocate(ms->port) == KERN_SUCCESS);

	if (unlikely(ms->port == the_exception_server)) {
		the_exception_server = 0;
	}

	job_log(j, LOG_DEBUG, "Mach service deleted%s: %s", port_died ? " (port died)" : "", ms->name);

	if (ms->special_port_num) {
		SLIST_REMOVE(&special_ports, ms, machservice, special_port_sle);
	}

	SLIST_REMOVE(&j->machservices, ms, machservice, sle);
	LIST_REMOVE(ms, name_hash_sle);
	LIST_REMOVE(ms, port_hash_sle);

	free(ms);
}

void
machservice_request_notifications(struct machservice *ms)
{
	mach_msg_id_t which = MACH_NOTIFY_DEAD_NAME;

	ms->isActive = true;

	if (ms->recv) {
		which = MACH_NOTIFY_PORT_DESTROYED;
		job_checkin(ms->job);
	}

	job_assumes(ms->job, launchd_mport_notify_req(ms->port, which) == KERN_SUCCESS);
}

#define NELEM(x)	(sizeof(x)/sizeof(x[0]))
#define END_OF(x)	(&(x)[NELEM(x)])

char **
mach_cmd2argv(const char *string)
{
	char *argv[100], args[1000];
	const char *cp;
	char *argp = args, term, **argv_ret, *co;
	unsigned int nargs = 0, i;

	for (cp = string; *cp;) {
		while (isspace(*cp))
			cp++;
		term = (*cp == '"') ? *cp++ : '\0';
		if (nargs < NELEM(argv)) {
			argv[nargs++] = argp;
		}
		while (*cp && (term ? *cp != term : !isspace(*cp)) && argp < END_OF(args)) {
			if (*cp == '\\') {
				cp++;
			}
			*argp++ = *cp;
			if (*cp) {
				cp++;
			}
		}
		*argp++ = '\0';
	}
	argv[nargs] = NULL;

	if (nargs == 0) {
		return NULL;
	}

	argv_ret = malloc((nargs + 1) * sizeof(char *) + strlen(string) + 1);

	if (!launchd_assumes(argv_ret != NULL)) {
		return NULL;
	}

	co = (char *)argv_ret + (nargs + 1) * sizeof(char *);

	for (i = 0; i < nargs; i++) {
		strcpy(co, argv[i]);
		argv_ret[i] = co;
		co += strlen(argv[i]) + 1;
	}
	argv_ret[i] = NULL;
	
	return argv_ret;
}

void
job_checkin(job_t j)
{
	j->checkedin = true;
}

bool job_is_god(job_t j)
{
	return j->embedded_special_privileges;
}

bool
job_ack_port_destruction(mach_port_t p)
{
	struct machservice *ms;
	job_t j;

	LIST_FOREACH(ms, &port_hash[HASH_PORT(p)], port_hash_sle) {
		if (ms->recv && (ms->port == p)) {
			break;
		}
	}

	if (!jobmgr_assumes(root_jobmgr, ms != NULL)) {
		return false;
	}

	j = ms->job;

	jobmgr_log(root_jobmgr, LOG_DEBUG, "Receive right returned to us: %s", ms->name);
	
	/* Without being the exception handler, NOTE_EXIT is our only way to tell if the job 
	 * crashed, and we can't rely on NOTE_EXIT always being processed after all the job's
	 * receive rights have been returned.
	 *
	 * So when we get receive rights back, check to see if the job has been reaped yet. If
	 * not, then we add this service to a list of services to be drained on crash if it's 
	 * requested that behavior. So, for a job with N receive rights all requesting that they
	 * be drained on crash, we can safely handle the following sequence of events.
	 * 
	 * ReceiveRight0Returned
	 * ReceiveRight1Returned
	 * ReceiveRight2Returned
	 * NOTE_EXIT (reap, get exit status)
	 * ReceiveRight3Returned
	 * .
	 * .
	 * .
	 * ReceiveRight(N - 1)Returned
	 */
	
	if( ms->drain_one_on_crash || ms->drain_all_on_crash ) {
		if( j->crashed && j->reaped ) {
			job_log(j, LOG_DEBUG, "Job has crashed. Draining port...");
			machservice_drain_port(ms);
		} else if( !(j->crashed || j->reaped) ) {
			job_log(j, LOG_DEBUG, "Job's exit status is still unknown. Deferring drain.");
		}
	}
	
	ms->isActive = false;
	if (ms->delete_on_destruction) {
		machservice_delete(j, ms, false);
	} else if (ms->reset) {
		machservice_resetport(j, ms);
	}
	
	job_dispatch(j, false);

	root_jobmgr = jobmgr_do_garbage_collection(root_jobmgr);

	return true;
}

void
job_ack_no_senders(job_t j)
{
	j->priv_port_has_senders = false;

	job_assumes(j, launchd_mport_close_recv(j->j_port) == KERN_SUCCESS);
	j->j_port = 0;

	job_log(j, LOG_DEBUG, "No more senders on privileged Mach bootstrap port");

	job_dispatch(j, false);
}

bool
semaphoreitem_new(job_t j, semaphore_reason_t why, const char *what)
{
	struct semaphoreitem *si;
	size_t alloc_sz = sizeof(struct semaphoreitem);

	if (what) {
		alloc_sz += strlen(what) + 1;
	}

	if (!job_assumes(j, si = calloc(1, alloc_sz))) {
		return false;
	}

	si->fd = -1;
	si->why = why;

	if (what) {
		strcpy(si->what_init, what);
	}

	SLIST_INSERT_HEAD(&j->semaphores, si, sle);
	
	if( (why == OTHER_JOB_ENABLED || why == OTHER_JOB_DISABLED) && !j->nosy ) {
		job_log(j, LOG_DEBUG, "Job is interested in \"%s\".", what);
		SLIST_INSERT_HEAD(&s_curious_jobs, j, curious_jobs_sle);
		j->nosy = true;
	}

	semaphoreitem_runtime_mod_ref(si, true);

	return true;
}

void
semaphoreitem_runtime_mod_ref(struct semaphoreitem *si, bool add)
{
	/*
	 * External events need to be tracked.
	 * Internal events do NOT need to be tracked.
	 */

	switch (si->why) {
	case SUCCESSFUL_EXIT:
	case FAILED_EXIT:
	case OTHER_JOB_ENABLED:
	case OTHER_JOB_DISABLED:
	case OTHER_JOB_ACTIVE:
	case OTHER_JOB_INACTIVE:
		return;
	default:
		break;
	}

	if (add) {
		runtime_add_weak_ref();
	} else {
		runtime_del_weak_ref();
	}
}

void
semaphoreitem_delete(job_t j, struct semaphoreitem *si)
{
	semaphoreitem_runtime_mod_ref(si, false);

	SLIST_REMOVE(&j->semaphores, si, semaphoreitem, sle);

	if (si->fd != -1) {
		job_assumes(j, runtime_close(si->fd) != -1);
	}
	
	/* We'll need to rethink this if it ever becomes possible to dynamically add or remove semaphores. */
	if( (si->why == OTHER_JOB_ENABLED || si->why == OTHER_JOB_DISABLED) && j->nosy ) {
		j->nosy = false;
		SLIST_REMOVE(&s_curious_jobs, j, job_s, curious_jobs_sle);
	}

	free(si);
}

void
semaphoreitem_setup_dict_iter(launch_data_t obj, const char *key, void *context)
{
	struct semaphoreitem_dict_iter_context *sdic = context;
	semaphore_reason_t why;

	why = launch_data_get_bool(obj) ? sdic->why_true : sdic->why_false;

	semaphoreitem_new(sdic->j, why, key);
}

void
semaphoreitem_setup(launch_data_t obj, const char *key, void *context)
{
	struct semaphoreitem_dict_iter_context sdic = { context, 0, 0 };
	job_t j = context;
	semaphore_reason_t why;

	switch (launch_data_get_type(obj)) {
	case LAUNCH_DATA_BOOL:
		if (strcasecmp(key, LAUNCH_JOBKEY_KEEPALIVE_NETWORKSTATE) == 0) {
			why = launch_data_get_bool(obj) ? NETWORK_UP : NETWORK_DOWN;
			semaphoreitem_new(j, why, NULL);
		} else if (strcasecmp(key, LAUNCH_JOBKEY_KEEPALIVE_SUCCESSFULEXIT) == 0) {
			why = launch_data_get_bool(obj) ? SUCCESSFUL_EXIT : FAILED_EXIT;
			semaphoreitem_new(j, why, NULL);
			j->start_pending = true;
		} else if( strcasecmp(key, LAUNCH_JOBKEY_KEEPALIVE_AFTERINITIALDEMAND) == 0 ) {
			j->needs_kickoff = launch_data_get_bool(obj);
		} else {
			job_assumes(j, false);
		}
		break;
	case LAUNCH_DATA_DICTIONARY:
		if (strcasecmp(key, LAUNCH_JOBKEY_KEEPALIVE_PATHSTATE) == 0) {
			sdic.why_true = PATH_EXISTS;
			sdic.why_false = PATH_MISSING;
		} else if (strcasecmp(key, LAUNCH_JOBKEY_KEEPALIVE_OTHERJOBACTIVE) == 0) {
			sdic.why_true = OTHER_JOB_ACTIVE;
			sdic.why_false = OTHER_JOB_INACTIVE;
		} else if (strcasecmp(key, LAUNCH_JOBKEY_KEEPALIVE_OTHERJOBENABLED) == 0) {
			sdic.why_true = OTHER_JOB_ENABLED;
			sdic.why_false = OTHER_JOB_DISABLED;
		} else {
			job_assumes(j, false);
			break;
		}

		launch_data_dict_iterate(obj, semaphoreitem_setup_dict_iter, &sdic);
		break;
	default:
		job_assumes(j, false);
		break;
	}
}

void
jobmgr_dispatch_all_semaphores(jobmgr_t jm)
{
	jobmgr_t jmi, jmn;
	job_t ji, jn;


	SLIST_FOREACH_SAFE(jmi, &jm->submgrs, sle, jmn) {
		jobmgr_dispatch_all_semaphores(jmi);
	}

	LIST_FOREACH_SAFE(ji, &jm->jobs, sle, jn) {
		if (!SLIST_EMPTY(&ji->semaphores)) {
			job_dispatch(ji, false);
		}
	}
}

time_t
cronemu(int mon, int mday, int hour, int min)
{
	struct tm workingtm;
	time_t now;

	now = time(NULL);
	workingtm = *localtime(&now);

	workingtm.tm_isdst = -1;
	workingtm.tm_sec = 0;
	workingtm.tm_min++;

	while (!cronemu_mon(&workingtm, mon, mday, hour, min)) {
		workingtm.tm_year++;
		workingtm.tm_mon = 0;
		workingtm.tm_mday = 1;
		workingtm.tm_hour = 0;
		workingtm.tm_min = 0;
		mktime(&workingtm);
	}

	return mktime(&workingtm);
}

time_t
cronemu_wday(int wday, int hour, int min)
{
	struct tm workingtm;
	time_t now;

	now = time(NULL);
	workingtm = *localtime(&now);

	workingtm.tm_isdst = -1;
	workingtm.tm_sec = 0;
	workingtm.tm_min++;

	if (wday == 7) {
		wday = 0;
	}

	while (!(workingtm.tm_wday == wday && cronemu_hour(&workingtm, hour, min))) {
		workingtm.tm_mday++;
		workingtm.tm_hour = 0;
		workingtm.tm_min = 0;
		mktime(&workingtm);
	}

	return mktime(&workingtm);
}

bool
cronemu_mon(struct tm *wtm, int mon, int mday, int hour, int min)
{
	if (mon == -1) {
		struct tm workingtm = *wtm;
		int carrytest;

		while (!cronemu_mday(&workingtm, mday, hour, min)) {
			workingtm.tm_mon++;
			workingtm.tm_mday = 1;
			workingtm.tm_hour = 0;
			workingtm.tm_min = 0;
			carrytest = workingtm.tm_mon;
			mktime(&workingtm);
			if (carrytest != workingtm.tm_mon) {
				return false;
			}
		}
		*wtm = workingtm;
		return true;
	}

	if (mon < wtm->tm_mon) {
		return false;
	}

	if (mon > wtm->tm_mon) {
		wtm->tm_mon = mon;
		wtm->tm_mday = 1;
		wtm->tm_hour = 0;
		wtm->tm_min = 0;
	}

	return cronemu_mday(wtm, mday, hour, min);
}

bool
cronemu_mday(struct tm *wtm, int mday, int hour, int min)
{
	if (mday == -1) {
		struct tm workingtm = *wtm;
		int carrytest;

		while (!cronemu_hour(&workingtm, hour, min)) {
			workingtm.tm_mday++;
			workingtm.tm_hour = 0;
			workingtm.tm_min = 0;
			carrytest = workingtm.tm_mday;
			mktime(&workingtm);
			if (carrytest != workingtm.tm_mday) {
				return false;
			}
		}
		*wtm = workingtm;
		return true;
	}

	if (mday < wtm->tm_mday) {
		return false;
	}

	if (mday > wtm->tm_mday) {
		wtm->tm_mday = mday;
		wtm->tm_hour = 0;
		wtm->tm_min = 0;
	}

	return cronemu_hour(wtm, hour, min);
}

bool
cronemu_hour(struct tm *wtm, int hour, int min)
{
	if (hour == -1) {
		struct tm workingtm = *wtm;
		int carrytest;

		while (!cronemu_min(&workingtm, min)) {
			workingtm.tm_hour++;
			workingtm.tm_min = 0;
			carrytest = workingtm.tm_hour;
			mktime(&workingtm);
			if (carrytest != workingtm.tm_hour) {
				return false;
			}
		}
		*wtm = workingtm;
		return true;
	}

	if (hour < wtm->tm_hour) {
		return false;
	}

	if (hour > wtm->tm_hour) {
		wtm->tm_hour = hour;
		wtm->tm_min = 0;
	}

	return cronemu_min(wtm, min);
}

bool
cronemu_min(struct tm *wtm, int min)
{
	if (min == -1) {
		return true;
	}

	if (min < wtm->tm_min) {
		return false;
	}

	if (min > wtm->tm_min) {
		wtm->tm_min = min;
	}

	return true;
}

kern_return_t
job_mig_setup_shmem(job_t j, mach_port_t *shmem_port)
{
	memory_object_size_t size_of_page, size_of_page_orig;
	vm_address_t vm_addr;
	kern_return_t kr;

	if (!launchd_assumes(j != NULL)) {
		return BOOTSTRAP_NO_MEMORY;
	}

	if (unlikely(j->anonymous)) {
		job_log(j, LOG_DEBUG, "Anonymous job tried to setup shared memory");
		return BOOTSTRAP_NOT_PRIVILEGED;
	}

	if (unlikely(j->shmem)) {
		job_log(j, LOG_ERR, "Tried to setup shared memory more than once");
		return BOOTSTRAP_NOT_PRIVILEGED;
	}

	size_of_page_orig = size_of_page = getpagesize();

	kr = vm_allocate(mach_task_self(), &vm_addr, size_of_page, true);

	if (!job_assumes(j, kr == 0)) {
		return kr;
	}

	j->shmem = (typeof(j->shmem))vm_addr;
	j->shmem->vp_shmem_standby_timeout = j->timeout;

	kr = mach_make_memory_entry_64(mach_task_self(), &size_of_page,
			(memory_object_offset_t)vm_addr, VM_PROT_READ|VM_PROT_WRITE, shmem_port, 0);

	if (job_assumes(j, kr == 0)) {
		job_assumes(j, size_of_page == size_of_page_orig);
	}

	/* no need to inherit this in child processes */
	job_assumes(j, vm_inherit(mach_task_self(), (vm_address_t)j->shmem, size_of_page_orig, VM_INHERIT_NONE) == 0);

	return kr;
}

kern_return_t
job_mig_create_server(job_t j, cmd_t server_cmd, uid_t server_uid, boolean_t on_demand, mach_port_t *server_portp)
{
	struct ldcred *ldc = runtime_get_caller_creds();
	job_t js;

	if (!launchd_assumes(j != NULL)) {
		return BOOTSTRAP_NO_MEMORY;
	}

	if (unlikely(j->deny_job_creation)) {
		return BOOTSTRAP_NOT_PRIVILEGED;
	}

#if HAVE_SANDBOX
	const char **argv = (const char **)mach_cmd2argv(server_cmd);
	if (unlikely(argv == NULL)) {
		return BOOTSTRAP_NO_MEMORY;
	}
	if (unlikely(sandbox_check(ldc->pid, "job-creation", SANDBOX_FILTER_PATH, argv[0]) > 0)) {
		free(argv);
		return BOOTSTRAP_NOT_PRIVILEGED;
	}
	free(argv);
#endif

	job_log(j, LOG_DEBUG, "Server create attempt: %s", server_cmd);

	if (pid1_magic) {
		if (ldc->euid || ldc->uid) {
			job_log(j, LOG_WARNING, "Server create attempt moved to per-user launchd: %s", server_cmd);
			return VPROC_ERR_TRY_PER_USER;
		}
	} else {
		if (unlikely(server_uid != getuid())) {
			job_log(j, LOG_WARNING, "Server create: \"%s\": As UID %d, we will not be able to switch to UID %d",
					server_cmd, getuid(), server_uid);
		}
		server_uid = 0; /* zero means "do nothing" */
	}

	js = job_new_via_mach_init(j, server_cmd, server_uid, on_demand);

	if (unlikely(js == NULL)) {
		return BOOTSTRAP_NO_MEMORY;
	}

	*server_portp = js->j_port;
	return BOOTSTRAP_SUCCESS;
}

kern_return_t
job_mig_send_signal(job_t j, mach_port_t srp, name_t targetlabel, int sig)
{
	struct ldcred *ldc = runtime_get_caller_creds();
	job_t otherj;

	if (!launchd_assumes(j != NULL)) {
		return BOOTSTRAP_NO_MEMORY;
	}

	if( unlikely(ldc->euid != 0 && ldc->euid != getuid()) || j->deny_job_creation ) {

	}

	if( unlikely(ldc->euid != 0 && ldc->euid != getuid()) || j->deny_job_creation ) {
	#if TARGET_OS_EMBEDDED	
		if( !j->embedded_special_privileges ) {
			return BOOTSTRAP_NOT_PRIVILEGED;
		}
	#else
		return BOOTSTRAP_NOT_PRIVILEGED;
	#endif
	}

#if HAVE_SANDBOX
	if (unlikely(sandbox_check(ldc->pid, "job-creation", SANDBOX_FILTER_NONE) > 0)) {
		return BOOTSTRAP_NOT_PRIVILEGED;
	}
#endif
	
	if (unlikely(!(otherj = job_find(targetlabel)))) {
		return BOOTSTRAP_UNKNOWN_SERVICE;
	}

#if TARGET_OS_EMBEDDED
	if( j->embedded_special_privileges && strcmp(j->username, otherj->username) != 0 ) {
		return BOOTSTRAP_NOT_PRIVILEGED;
	}
#endif	

	if (sig == VPROC_MAGIC_UNLOAD_SIGNAL) {
		bool do_block = otherj->p;

		if (otherj->anonymous) {
			return BOOTSTRAP_NOT_PRIVILEGED;
		}

		job_remove(otherj);

		if (do_block) {
			job_log(j, LOG_DEBUG, "Blocking MIG return of job_remove(): %s", otherj->label);
			/* this is messy. We shouldn't access 'otherj' after job_remove(), but we check otherj->p first... */
			job_assumes(otherj, waiting4removal_new(otherj, srp));
			return MIG_NO_REPLY;
		} else {
			return 0;
		}
	} else if (sig == VPROC_MAGIC_TRYKILL_SIGNAL) {
		if (!j->kill_via_shmem) {
			return BOOTSTRAP_NOT_PRIVILEGED;
		}

		if (!j->shmem) {
			j->sent_kill_via_shmem = true;
			job_assumes(j, runtime_kill(otherj->p, SIGKILL) != -1);
			return 0;
		}
	#if !TARGET_OS_EMBEDDED
		if (__sync_bool_compare_and_swap(&j->shmem->vp_shmem_transaction_cnt, 0, -1)) {
			j->shmem->vp_shmem_flags |= VPROC_SHMEM_EXITING;
			j->sent_kill_via_shmem = true;
			job_assumes(j, runtime_kill(otherj->p, SIGKILL) != -1);
			return 0;
		}
	#endif
		return BOOTSTRAP_NOT_PRIVILEGED;
	} else if (otherj->p) {
		job_assumes(j, runtime_kill(otherj->p, sig) != -1);
	}

	return 0;
}

kern_return_t
job_mig_log_forward(job_t j, vm_offset_t inval, mach_msg_type_number_t invalCnt)
{
	struct ldcred *ldc = runtime_get_caller_creds();

	if (!launchd_assumes(j != NULL)) {
		return BOOTSTRAP_NO_MEMORY;
	}

	if (!job_assumes(j, j->per_user)) {
		return BOOTSTRAP_NOT_PRIVILEGED;
	}

	return runtime_log_forward(ldc->euid, ldc->egid, inval, invalCnt);
}

kern_return_t
job_mig_log_drain(job_t j, mach_port_t srp, vm_offset_t *outval, mach_msg_type_number_t *outvalCnt)
{
	struct ldcred *ldc = runtime_get_caller_creds();

	if (!launchd_assumes(j != NULL)) {
		return BOOTSTRAP_NO_MEMORY;
	}

	if (unlikely(ldc->euid)) {
		return BOOTSTRAP_NOT_PRIVILEGED;
	}

	return runtime_log_drain(srp, outval, outvalCnt);
}

kern_return_t
job_mig_swap_complex(job_t j, vproc_gsk_t inkey, vproc_gsk_t outkey,
					 vm_offset_t inval,				mach_msg_type_number_t invalCnt,
					 vm_offset_t *outval,			mach_msg_type_number_t *outvalCnt)
{
	const char *action;
	launch_data_t input_obj = NULL, output_obj = NULL;
	size_t data_offset = 0;
	size_t packed_size;
	struct ldcred *ldc = runtime_get_caller_creds();

	if (!launchd_assumes(j != NULL)) {
		return BOOTSTRAP_NO_MEMORY;
	}

	if (unlikely(inkey && ldc->euid && ldc->euid != getuid())) {
		return BOOTSTRAP_NOT_PRIVILEGED;
	}

	if (unlikely(inkey && outkey && !job_assumes(j, inkey == outkey))) {
		return 1;
	}

	if (inkey && outkey) {
		action = "Swapping";
	} else if (inkey) {
		action = "Setting";
	} else {
		action = "Getting";
	}

	job_log(j, LOG_DEBUG, "%s key: %u", action, inkey ? inkey : outkey);

	*outvalCnt = 20 * 1024 * 1024;
	mig_allocate(outval, *outvalCnt);
	if (!job_assumes(j, *outval != 0)) {
		return 1;
	}

	runtime_ktrace0(RTKT_LAUNCHD_DATA_UNPACK);
	if (unlikely(invalCnt && !job_assumes(j, (input_obj = launch_data_unpack((void *)inval, invalCnt, NULL, 0, &data_offset, NULL)) != NULL))) {
		goto out_bad;
	}

	switch (outkey) {
	case VPROC_GSK_ENVIRONMENT:
		if (!job_assumes(j, (output_obj = launch_data_alloc(LAUNCH_DATA_DICTIONARY)))) {
			goto out_bad;
		}
		jobmgr_export_env_from_other_jobs(j->mgr, output_obj);
		runtime_ktrace0(RTKT_LAUNCHD_DATA_PACK);
		if (!job_assumes(j, launch_data_pack(output_obj, (void *)*outval, *outvalCnt, NULL, NULL) != 0)) {
			goto out_bad;
		}
		launch_data_free(output_obj);
		break;
	case VPROC_GSK_ALLJOBS:
		if (!job_assumes(j, (output_obj = job_export_all()) != NULL)) {
			goto out_bad;
		}
		ipc_revoke_fds(output_obj);
		runtime_ktrace0(RTKT_LAUNCHD_DATA_PACK);
		packed_size = launch_data_pack(output_obj, (void *)*outval, *outvalCnt, NULL, NULL);
		if (!job_assumes(j, packed_size != 0)) {
			goto out_bad;
		}
		launch_data_free(output_obj);
		break;
	case VPROC_GSK_MGR_NAME:
		if( !job_assumes(j, (output_obj = launch_data_new_string(j->mgr->name)) != NULL) ) {
			goto out_bad;
		}
		packed_size = launch_data_pack(output_obj, (void *)*outval, *outvalCnt, NULL, NULL);
		if (!job_assumes(j, packed_size != 0)) {
			goto out_bad;
		}
			
		launch_data_free(output_obj);
		break;
	case VPROC_GSK_JOB_OVERRIDES_DB:
		if( !job_assumes(j, (output_obj = launch_data_new_string(launchd_data_base_path(LAUNCHD_DB_TYPE_OVERRIDES))) != NULL) ) {
			goto out_bad;
		}
		packed_size = launch_data_pack(output_obj, (void *)*outval, *outvalCnt, NULL, NULL);
		if (!job_assumes(j, packed_size != 0)) {
			goto out_bad;
		}
		
		launch_data_free(output_obj);
		break;
	case VPROC_GSK_JOB_CACHE_DB:
		if( !job_assumes(j, (output_obj = launch_data_new_string(launchd_data_base_path(LAUNCHD_DB_TYPE_JOBCACHE))) != NULL) ) {
			goto out_bad;
		}
		packed_size = launch_data_pack(output_obj, (void *)*outval, *outvalCnt, NULL, NULL);
		if (!job_assumes(j, packed_size != 0)) {
			goto out_bad;
		}
		
		job_log(j, LOG_DEBUG, "Location of job cache database: %s", launch_data_get_string(output_obj));
			
		launch_data_free(output_obj);
		break;
	case 0:
		mig_deallocate(*outval, *outvalCnt);
		*outval = 0;
		*outvalCnt = 0;
		break;
	default:
		goto out_bad;
	}

	if (invalCnt) switch (inkey) {
	case VPROC_GSK_ENVIRONMENT:	
		if( launch_data_get_type(input_obj) == LAUNCH_DATA_DICTIONARY ) {
			if( j->p ) {
				job_log(j, LOG_INFO, "Setting environment for a currently active job. This environment will take effect on the next invocation of the job.");
			}
			launch_data_dict_iterate(input_obj, envitem_setup_one_shot, j);
		}
		break;
	case 0:
		break;
	default:
		goto out_bad;
	}

	mig_deallocate(inval, invalCnt);

	return 0;

out_bad:
	if (*outval) {
		mig_deallocate(*outval, *outvalCnt);
	}
	return 1;
}

kern_return_t
job_mig_swap_integer(job_t j, vproc_gsk_t inkey, vproc_gsk_t outkey, int64_t inval, int64_t *outval)
{
	const char *action;
	kern_return_t kr = 0;
	struct ldcred *ldc = runtime_get_caller_creds();
	int oldmask;

	if (!launchd_assumes(j != NULL)) {
		return BOOTSTRAP_NO_MEMORY;
	}

	if (unlikely(inkey && ldc->euid && ldc->euid != getuid())) {
		return BOOTSTRAP_NOT_PRIVILEGED;
	}

	if (unlikely(inkey && outkey && !job_assumes(j, inkey == outkey))) {
		return 1;
	}

	if (inkey && outkey) {
		action = "Swapping";
	} else if (inkey) {
		action = "Setting";
	} else {
		action = "Getting";
	}

	job_log(j, LOG_DEBUG, "%s key: %u", action, inkey ? inkey : outkey);

	switch (outkey) {
	case VPROC_GSK_ABANDON_PROCESS_GROUP:
		*outval = j->abandon_pg;
		break;
	case VPROC_GSK_LAST_EXIT_STATUS:
		*outval = j->last_exit_status;
		break;
	case VPROC_GSK_MGR_UID:
		*outval = getuid();
		break;
	case VPROC_GSK_MGR_PID:
		*outval = getpid();
		break;
	case VPROC_GSK_IS_MANAGED:
		*outval = j->anonymous ? 0 : 1;
		break;
	case VPROC_GSK_BASIC_KEEPALIVE:
		*outval = !j->ondemand;
		break;
	case VPROC_GSK_START_INTERVAL:
		*outval = j->start_interval;
		break;
	case VPROC_GSK_IDLE_TIMEOUT:
		*outval = j->timeout;
		break;
	case VPROC_GSK_EXIT_TIMEOUT:
		*outval = j->exit_timeout;
		break;
	case VPROC_GSK_GLOBAL_LOG_MASK:
		oldmask = runtime_setlogmask(LOG_UPTO(LOG_DEBUG));
		*outval = oldmask;
		runtime_setlogmask(oldmask);
		break;
	case VPROC_GSK_GLOBAL_UMASK:
		oldmask = umask(0);
		*outval = oldmask;
		umask(oldmask);
		break;
	case VPROC_GSK_TRANSACTIONS_ENABLED:
		job_log(j, LOG_DEBUG, "Reading transaction model status.");
		*outval = j->kill_via_shmem;
		break;
	case VPROC_GSK_WAITFORDEBUGGER:
		*outval = j->wait4debugger;
		break;
	case VPROC_GSK_EMBEDDEDROOTEQUIVALENT:
		*outval = j->embedded_special_privileges;
		break;
	case 0:
		*outval = 0;
		break;
	default:
		kr = 1;
		break;
	}

	switch (inkey) {
	case VPROC_GSK_ABANDON_PROCESS_GROUP:
		j->abandon_pg = (bool)inval;
		break;
	case VPROC_GSK_GLOBAL_ON_DEMAND:
		job_log(j, LOG_DEBUG, "Job is setting global on-demand mode to %s (j->forced_peers_to_demand_mode = %s)", (bool)inval ? "true" : "false", j->forced_peers_to_demand_mode ? "true" : "false");
		kr = job_set_global_on_demand(j, (bool)inval) ? 0 : 1;
		break;
	case VPROC_GSK_BASIC_KEEPALIVE:
		j->ondemand = !inval;
		break;
	case VPROC_GSK_START_INTERVAL:
		if (inval > UINT32_MAX || inval < 0) {
			kr = 1;
		} else if (inval) {
			if (j->start_interval == 0) {
				runtime_add_weak_ref();
			}
			j->start_interval = (typeof(j->start_interval)) inval;
			job_assumes(j, kevent_mod((uintptr_t)&j->start_interval, EVFILT_TIMER, EV_ADD, NOTE_SECONDS, j->start_interval, j) != -1);
		} else if (j->start_interval) {
			job_assumes(j, kevent_mod((uintptr_t)&j->start_interval, EVFILT_TIMER, EV_DELETE, 0, 0, NULL) != -1);
			if (j->start_interval != 0) {
				runtime_del_weak_ref();
			}
			j->start_interval = 0;
		}
		break;
	case VPROC_GSK_IDLE_TIMEOUT:
		if (inval < 0 || inval > UINT32_MAX) {
			kr = 1;
		} else {
			j->timeout = (typeof(j->timeout)) inval;
		}
		break;
	case VPROC_GSK_EXIT_TIMEOUT:
		if (inval < 0 || inval > UINT32_MAX) {
			kr = 1;
		} else {
			j->exit_timeout = (typeof(j->exit_timeout)) inval;
		}
		break;
	case VPROC_GSK_GLOBAL_LOG_MASK:
		if (inval < 0 || inval > UINT32_MAX) {
			kr = 1;
		} else {
			runtime_setlogmask((int) inval);
		}
		break;
	case VPROC_GSK_GLOBAL_UMASK:
		launchd_assert(sizeof (mode_t) == 2);
		if (inval < 0 || inval > UINT16_MAX) {
			kr = 1;
		} else {
			umask((mode_t) inval);
		}
		break;
	case VPROC_GSK_TRANSACTIONS_ENABLED:
		if( !job_assumes(j, inval != 0) ) {
			job_log(j, LOG_WARNING, "Attempt to unregister from transaction model. This is not supported.");
			kr = 1;
		} else {
			job_log(j, LOG_DEBUG, "Now participating in transaction model.");
			j->kill_via_shmem = (bool)inval;
			job_log(j, LOG_DEBUG, "j->kill_via_shmem = %s", j->kill_via_shmem ? "true" : "false");
		}
		break;
	case VPROC_GSK_WEIRD_BOOTSTRAP:
		if( job_assumes(j, j->weird_bootstrap) ) {
			job_log(j, LOG_DEBUG, "Unsetting weird bootstrap.");
			
			mach_msg_size_t mxmsgsz = (typeof(mxmsgsz)) sizeof(union __RequestUnion__job_mig_protocol_vproc_subsystem);
			
			if (job_mig_protocol_vproc_subsystem.maxsize > mxmsgsz) {
				mxmsgsz = job_mig_protocol_vproc_subsystem.maxsize;
			}
			
			job_assumes(j, runtime_add_mport(j->mgr->jm_port, protocol_vproc_server, mxmsgsz) == KERN_SUCCESS);
			j->weird_bootstrap = false;
		}
		break;
	case VPROC_GSK_WAITFORDEBUGGER:
		j->wait4debugger_oneshot = inval;
		break;
	case VPROC_GSK_PERUSER_SUSPEND:
		if( job_assumes(j, pid1_magic && ldc->euid == 0) ) {
			mach_port_t junk = MACH_PORT_NULL;
			job_t jpu = jobmgr_lookup_per_user_context_internal(j, (uid_t)inval, false, &junk);
			if( job_assumes(j, jpu != NULL) ) {
				struct suspended_peruser *spi = NULL;
				LIST_FOREACH( spi, &j->suspended_perusers, sle ) {
					if( (int64_t)(spi->j->mach_uid) == inval ) {
						job_log(j, LOG_WARNING, "Job tried to suspend per-user launchd for UID %lli twice.", inval);
						break;
					}
				}

				if( spi == NULL ) {
					job_log(j, LOG_INFO, "Job is suspending the per-user launchd for UID %lli.", inval);
					spi = (struct suspended_peruser *)calloc(sizeof(struct suspended_peruser), 1);
					if( job_assumes(j, spi != NULL) ) {
						spi->j = jpu;
						spi->j->peruser_suspend_count++;
						LIST_INSERT_HEAD(&j->suspended_perusers, spi, sle);
						job_stop(spi->j);
					} else {
						kr = BOOTSTRAP_NO_MEMORY;
					}
				}
			}
		} else {
			kr = 1;
		}
		break;
	case VPROC_GSK_PERUSER_RESUME:
		if( job_assumes(j, pid1_magic == true) ) {
			struct suspended_peruser *spi = NULL, *spt = NULL;
			LIST_FOREACH_SAFE( spi, &j->suspended_perusers, sle, spt ) {
				if( (int64_t)(spi->j->mach_uid) == inval ) {
					spi->j->peruser_suspend_count--;
					LIST_REMOVE(spi, sle);
					job_log(j, LOG_INFO, "Job is resuming the per-user launchd for UID %lli.", inval);
					break;
				}
			}
			
			if( !job_assumes(j, spi != NULL) ) {
				job_log(j, LOG_WARNING, "Job tried to resume per-user launchd for UID %lli that it did not suspend.", inval);
				kr = BOOTSTRAP_NOT_PRIVILEGED;
			} else if( spi->j->peruser_suspend_count == 0 ) {
				job_dispatch(spi->j, false);
				free(spi);
			}
		} else {
			kr = 1;
		}
		break;
	case 0:
		break;
	default:
		kr = 1;
		break;
	}

	return kr;
}

kern_return_t
job_mig_post_fork_ping(job_t j, task_t child_task, mach_port_t *audit_session)
{
	struct machservice *ms;

	if (!launchd_assumes(j != NULL)) {
		return BOOTSTRAP_NO_MEMORY;
	}

	job_log(j, LOG_DEBUG, "Post fork ping.");

	job_setup_exception_port(j, child_task);

	SLIST_FOREACH(ms, &special_ports, special_port_sle) {
		if (j->per_user && (ms->special_port_num != TASK_ACCESS_PORT)) {
			/* The TASK_ACCESS_PORT funny business is to workaround 5325399. */
			continue;
		}

		errno = task_set_special_port(child_task, ms->special_port_num, ms->port);

		if (unlikely(errno)) {
			int desired_log_level = LOG_ERR;

			if (j->anonymous) {
				/* 5338127 */

				desired_log_level = LOG_WARNING;

				if (ms->special_port_num == TASK_SEATBELT_PORT) {
					desired_log_level = LOG_DEBUG;
				}
			}

			job_log(j, desired_log_level, "Could not setup Mach task special port %u: %s", ms->special_port_num, mach_error_string(errno));
		}
	}

	mach_port_t _session = MACH_PORT_NULL;
#if !TARGET_OS_EMBEDDED
	if( !j->anonymous && !j->per_user ) {
		job_log(j, LOG_DEBUG, "Returning session port %u", j->audit_session);
		_session = j->audit_session;
	}
#endif	
	*audit_session = _session;
	job_assumes(j, launchd_mport_deallocate(child_task) == KERN_SUCCESS);

	return 0;
}

kern_return_t
job_mig_reboot2(job_t j, uint64_t flags)
{
	char who_started_the_reboot[2048] = "";
	struct kinfo_proc kp;
	struct ldcred *ldc = runtime_get_caller_creds();
	pid_t pid_to_log;

	if (!launchd_assumes(j != NULL)) {
		return BOOTSTRAP_NO_MEMORY;
	}

	if (unlikely(!pid1_magic)) {
		return BOOTSTRAP_NOT_PRIVILEGED;
	}

#if !TARGET_OS_EMBEDDED
	if (unlikely(ldc->euid)) {
#else
	if( unlikely(ldc->euid) && !j->embedded_special_privileges ) {
#endif
		return BOOTSTRAP_NOT_PRIVILEGED;
	}

	for (pid_to_log = ldc->pid; pid_to_log; pid_to_log = kp.kp_eproc.e_ppid) {
		int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, pid_to_log };
		size_t who_offset, len = sizeof(kp);

		if (!job_assumes(j, sysctl(mib, 4, &kp, &len, NULL, 0) != -1)) {
			return 1;
		}

		who_offset = strlen(who_started_the_reboot);
		snprintf(who_started_the_reboot + who_offset, sizeof(who_started_the_reboot) - who_offset,
				" %s[%u]%s", kp.kp_proc.p_comm, pid_to_log, kp.kp_eproc.e_ppid ? " ->" : "");
	}

	root_jobmgr->reboot_flags = (int)flags;

	launchd_shutdown();

	job_log(j, LOG_DEBUG, "reboot2() initiated by:%s", who_started_the_reboot);

	return 0;
}

kern_return_t
job_mig_getsocket(job_t j, name_t spr)
{
	if (!launchd_assumes(j != NULL)) {
		return BOOTSTRAP_NO_MEMORY;
	}

	if( j->deny_job_creation ) {
		return BOOTSTRAP_NOT_PRIVILEGED;
	}

	ipc_server_init();

	if (unlikely(!sockpath)) {
		return BOOTSTRAP_NO_MEMORY;
	}

	strncpy(spr, sockpath, sizeof(name_t));
	
	return BOOTSTRAP_SUCCESS;
}

kern_return_t
job_mig_log(job_t j, int pri, int err, logmsg_t msg)
{
	if (!launchd_assumes(j != NULL)) {
		return BOOTSTRAP_NO_MEMORY;
	}

	if ((errno = err)) {
		job_log_error(j, pri, "%s", msg);
	} else {
		job_log(j, pri, "%s", msg);
	}

	return 0;
}

job_t
jobmgr_lookup_per_user_context_internal(job_t j, uid_t which_user, bool dispatch, mach_port_t *mp)
{
	job_t ji = NULL;
	LIST_FOREACH(ji, &root_jobmgr->jobs, sle) {
		if (!ji->per_user) {
			continue;
		}
		if (ji->mach_uid != which_user) {
			continue;
		}
		if (SLIST_EMPTY(&ji->machservices)) {
			continue;
		}
		if (!SLIST_FIRST(&ji->machservices)->per_user_hack) {
			continue;
		}
		break;
	}
	
	if( unlikely(ji == NULL) ) {
		struct machservice *ms;
		char lbuf[1024];
		
		job_log(j, LOG_DEBUG, "Creating per user launchd job for UID: %u", which_user);
		
		sprintf(lbuf, "com.apple.launchd.peruser.%u", which_user);
		
		ji = job_new(root_jobmgr, lbuf, "/sbin/launchd", NULL);
		
		if( ji != NULL ) {
			ji->mach_uid = which_user;
			ji->per_user = true;
			ji->kill_via_shmem = true;
			
			struct stat sb;
			char pu_db[PATH_MAX];
			snprintf(pu_db, sizeof(pu_db), LAUNCHD_DB_PREFIX "/%s", lbuf);
			
			bool created = false;
			int err = stat(pu_db, &sb);
			if( (err == -1 && errno == ENOENT) || (err == 0 && !S_ISDIR(sb.st_mode)) ) {
				if( err == 0 ) {
					char move_aside[PATH_MAX];
					snprintf(move_aside, sizeof(move_aside), LAUNCHD_DB_PREFIX "/%s.movedaside", lbuf);
					
					job_assumes(ji, rename(pu_db, move_aside) != -1);
				}

				job_assumes(ji, mkdir(pu_db, S_IRWXU) != -1);
				job_assumes(ji, chown(pu_db, which_user, 0) != -1);
				created = true;
			}
			
			if( !created ) {
				if( !job_assumes(ji, sb.st_uid == which_user) ) {
					job_assumes(ji, chown(pu_db, which_user, 0) != -1);
				}
				if( !job_assumes(ji, sb.st_gid == 0) ) {
					job_assumes(ji, chown(pu_db, which_user, 0) != -1);
				}
				if( !job_assumes(ji, sb.st_mode == (S_IRWXU | S_IFDIR)) ) {
					job_assumes(ji, chmod(pu_db, S_IRWXU) != -1);
				}
			}
			
			if ((ms = machservice_new(ji, lbuf, mp, false)) == NULL) {
				job_remove(ji);
				ji = NULL;
			} else {
				ms->per_user_hack = true;
				ms->hide = true;
				
				ji = dispatch ? job_dispatch(ji, false) : ji;
			}
		}
	} else {
		*mp = machservice_port(SLIST_FIRST(&ji->machservices));
		job_log(j, LOG_DEBUG, "Per user launchd job found for UID: %u", which_user);
	}
	
	return ji;
}

kern_return_t
job_mig_lookup_per_user_context(job_t j, uid_t which_user, mach_port_t *up_cont)
{
	struct ldcred *ldc = runtime_get_caller_creds();
	job_t jpu;
	
#if TARGET_OS_EMBEDDED
	/* There is no need for per-user launchd's on embedded. */
	job_log(j, LOG_ERR, "Per-user launchds are not supported on this platform.");
	return BOOTSTRAP_NOT_PRIVILEGED;
#endif
	
#if HAVE_SANDBOX
	if (unlikely(sandbox_check(ldc->pid, "mach-per-user-lookup", SANDBOX_FILTER_NONE) > 0)) {
		return BOOTSTRAP_NOT_PRIVILEGED;
	}
#endif
	
	if (!launchd_assumes(j != NULL)) {
		return BOOTSTRAP_NO_MEMORY;
	}
	
	job_log(j, LOG_INFO, "Looking up per user launchd for UID: %u", which_user);
	
	if (unlikely(!pid1_magic)) {
		job_log(j, LOG_ERR, "Only PID 1 supports per user launchd lookups.");
		return BOOTSTRAP_NOT_PRIVILEGED;
	}
	
	if (ldc->euid || ldc->uid) {
		which_user = ldc->euid ?: ldc->uid;
	}
	
	*up_cont = MACH_PORT_NULL;
	
	jpu = jobmgr_lookup_per_user_context_internal(j, which_user, true, up_cont);
	
	return 0;
}

kern_return_t
job_mig_check_in2(job_t j, name_t servicename, mach_port_t *serviceportp, uint64_t flags)
{
	bool per_pid_service = flags & BOOTSTRAP_PER_PID_SERVICE;
	struct ldcred *ldc = runtime_get_caller_creds();
	struct machservice *ms;
	job_t jo;

	if (!launchd_assumes(j != NULL)) {
		return BOOTSTRAP_NO_MEMORY;
	}

	ms = jobmgr_lookup_service(j->mgr, servicename, false, per_pid_service ? ldc->pid : 0);

	if (ms == NULL) {
		*serviceportp = MACH_PORT_NULL;

		if (unlikely((ms = machservice_new(j, servicename, serviceportp, per_pid_service)) == NULL)) {
			return BOOTSTRAP_NO_MEMORY;
		}
		
		/* Treat this like a legacy job. */
		if( !j->legacy_mach_job ) {
			ms->isActive = true;
			ms->recv = false;
		}

		if (!(j->anonymous || j->legacy_LS_job || j->legacy_mach_job)) {
			job_log(j, LOG_SCOLDING, "Please add the following service to the configuration file for this job: %s", servicename);
		}
	} else {
		if (unlikely((jo = machservice_job(ms)) != j)) {
			static pid_t last_warned_pid;

			if (last_warned_pid != ldc->pid) {
				job_log(jo, LOG_WARNING, "The following job tried to hijack the service \"%s\" from this job: %s", servicename, j->label);
				last_warned_pid = ldc->pid;
			}

			return BOOTSTRAP_NOT_PRIVILEGED;
		}
		if (unlikely(machservice_active(ms))) {
			job_log(j, LOG_WARNING, "Check-in of Mach service failed. Already active: %s", servicename);
			return BOOTSTRAP_SERVICE_ACTIVE;
		}
	}

	job_checkin(j);
	machservice_request_notifications(ms);

	job_log(j, LOG_INFO, "Check-in of service: %s", servicename);

	*serviceportp = machservice_port(ms);
	return BOOTSTRAP_SUCCESS;
}

kern_return_t
job_mig_register2(job_t j, name_t servicename, mach_port_t serviceport, uint64_t flags)
{
	struct machservice *ms;
	struct ldcred *ldc = runtime_get_caller_creds();

	if (!launchd_assumes(j != NULL)) {
		return BOOTSTRAP_NO_MEMORY;
	}

	if (!(flags & BOOTSTRAP_PER_PID_SERVICE) && !j->legacy_LS_job) {
		job_log(j, LOG_SCOLDING, "Performance: bootstrap_register() is deprecated. Service: %s", servicename);
	}

	job_log(j, LOG_DEBUG, "%sMach service registration attempt: %s", flags & BOOTSTRAP_PER_PID_SERVICE ? "Per PID " : "", servicename);

	/* 5641783 for the embedded hack */
#if !TARGET_OS_EMBEDDED
	/*
	 * From a per-user/session launchd's perspective, SecurityAgent (UID
	 * 92) is a rogue application (not our UID, not root and not a child of
	 * us). We'll have to reconcile this design friction at a later date.
	 */
	if (unlikely(j->anonymous && j->mgr->parentmgr == NULL && ldc->uid != 0 && ldc->uid != getuid() && ldc->uid != 92)) {
		if (pid1_magic) {
			return VPROC_ERR_TRY_PER_USER;
		} else {
			return BOOTSTRAP_NOT_PRIVILEGED;
		}
	}
#endif
	
	ms = jobmgr_lookup_service(j->mgr, servicename, false, flags & BOOTSTRAP_PER_PID_SERVICE ? ldc->pid : 0);

	if (unlikely(ms)) {
		if (machservice_job(ms) != j) {
			return BOOTSTRAP_NOT_PRIVILEGED;
		}
		if (machservice_active(ms)) {
			job_log(j, LOG_DEBUG, "Mach service registration failed. Already active: %s", servicename);
			return BOOTSTRAP_SERVICE_ACTIVE;
		}
		if (ms->recv && (serviceport != MACH_PORT_NULL)) {
			job_log(j, LOG_ERR, "bootstrap_register() erroneously called instead of bootstrap_check_in(). Mach service: %s", servicename);
			return BOOTSTRAP_NOT_PRIVILEGED;
		}
		job_checkin(j);
		machservice_delete(j, ms, false);
	}

	if (likely(serviceport != MACH_PORT_NULL)) {
		if (likely(ms = machservice_new(j, servicename, &serviceport, flags & BOOTSTRAP_PER_PID_SERVICE ? true : false))) {
			machservice_request_notifications(ms);
		} else {
			return BOOTSTRAP_NO_MEMORY;
		}
	}

	
	return BOOTSTRAP_SUCCESS;
}

kern_return_t
job_mig_look_up2(job_t j, mach_port_t srp, name_t servicename, mach_port_t *serviceportp, pid_t target_pid, uint64_t flags)
{
	struct machservice *ms;
	struct ldcred *ldc = runtime_get_caller_creds();
	kern_return_t kr;
	bool per_pid_lookup = flags & BOOTSTRAP_PER_PID_SERVICE;

	if (!launchd_assumes(j != NULL)) {
		return BOOTSTRAP_NO_MEMORY;
	}

	/* 5641783 for the embedded hack */
#if !TARGET_OS_EMBEDDED
	if (unlikely(pid1_magic && j->anonymous && j->mgr->parentmgr == NULL && ldc->uid != 0 && ldc->euid != 0)) {
		return VPROC_ERR_TRY_PER_USER;
	}
#endif

#if HAVE_SANDBOX
	if (unlikely(sandbox_check(ldc->pid, "mach-lookup", per_pid_lookup ? SANDBOX_FILTER_LOCAL_NAME : SANDBOX_FILTER_GLOBAL_NAME, servicename) > 0)) {
		return BOOTSTRAP_NOT_PRIVILEGED;
	}
#endif

	if (per_pid_lookup) {
		ms = jobmgr_lookup_service(j->mgr, servicename, false, target_pid);
	} else {
		ms = jobmgr_lookup_service(j->mgr, servicename, true, 0);
	}

	if (likely(ms)) {
		if (machservice_hidden(ms) && !machservice_active(ms)) {
			ms = NULL;
		} else if (unlikely(ms->per_user_hack)) {
			ms = NULL;
		}
	}

	if (likely(ms)) {
		job_assumes(j, machservice_port(ms) != MACH_PORT_NULL);
		job_log(j, LOG_DEBUG, "%sMach service lookup: %s", per_pid_lookup ? "Per PID " : "", servicename);

		if (unlikely(!per_pid_lookup && j->lastlookup == ms && j->lastlookup_gennum == ms->gen_num && !j->per_user)) {
			/* we need to think more about the per_pid_lookup logic before logging about repeated lookups */
			job_log(j, LOG_DEBUG, "Performance: Please fix the framework that talks to \"%s\" to cache the Mach port for service: %s", ms->job->label, servicename);
		}

		j->lastlookup = ms;
		j->lastlookup_gennum = ms->gen_num;

		*serviceportp = machservice_port(ms);

		kr = BOOTSTRAP_SUCCESS;
	} else if (!per_pid_lookup && (inherited_bootstrap_port != MACH_PORT_NULL)) {
		job_log(j, LOG_DEBUG, "Mach service lookup forwarded: %s", servicename);
		/* Clients potentially check the audit token of the reply to verify that the returned send right is trustworthy. */
		job_assumes(j, vproc_mig_look_up2_forward(inherited_bootstrap_port, srp, servicename, 0, 0) == 0);
		/* The previous routine moved the reply port, we're forced to return MIG_NO_REPLY now */
		return MIG_NO_REPLY;
	} else if (pid1_magic && j->anonymous && ldc->euid >= 500 && strcasecmp(j->mgr->name, VPROCMGR_SESSION_LOGINWINDOW) == 0) {
		/*
		 * 5240036 Should start background session when a lookup of CCacheServer occurs
		 *
		 * This is a total hack. We sniff out loginwindow session, and attempt to guess what it is up to.
		 * If we find a EUID that isn't root, we force it over to the per-user context.
		 */
		return VPROC_ERR_TRY_PER_USER;
	} else {
		job_log(j, LOG_DEBUG, "%sMach service lookup failed: %s", per_pid_lookup ? "Per PID " : "", servicename);
		kr = BOOTSTRAP_UNKNOWN_SERVICE;
	}

	return kr;
}

kern_return_t
job_mig_parent(job_t j, mach_port_t srp, mach_port_t *parentport)
{
	if (!launchd_assumes(j != NULL)) {
		return BOOTSTRAP_NO_MEMORY;
	}

	job_log(j, LOG_DEBUG, "Requested parent bootstrap port");
	jobmgr_t jm = j->mgr;

	if (jobmgr_parent(jm)) {
		*parentport = jobmgr_parent(jm)->jm_port;
	} else if (MACH_PORT_NULL == inherited_bootstrap_port) {
		*parentport = jm->jm_port;
	} else {
		job_assumes(j, vproc_mig_parent_forward(inherited_bootstrap_port, srp) == 0);
		/* The previous routine moved the reply port, we're forced to return MIG_NO_REPLY now */
		return MIG_NO_REPLY;
	}
	return BOOTSTRAP_SUCCESS;
}

kern_return_t
job_mig_info(job_t j,	name_array_t *servicenamesp, unsigned int *servicenames_cnt,
						name_array_t *servicejobsp, unsigned int *servicejobs_cnt,
						bootstrap_status_array_t *serviceactivesp, unsigned int *serviceactives_cnt,
						uint64_t flags)
{
	name_array_t service_names = NULL;
	name_array_t service_jobs = NULL;
	bootstrap_status_array_t service_actives = NULL;
	unsigned int cnt = 0, cnt2 = 0;
	jobmgr_t jm;

	if (!launchd_assumes(j != NULL)) {
		return BOOTSTRAP_NO_MEMORY;
	}

	if( g_flat_mach_namespace ) {
		if( (j->mgr->properties & BOOTSTRAP_PROPERTY_EXPLICITSUBSET) || (flags & BOOTSTRAP_FORCE_LOCAL) ) {
			jm = j->mgr;
		} else {
			jm = root_jobmgr;
		}
	} else {
		jm = j->mgr;
	}

	unsigned int i = 0;
	struct machservice *msi = NULL;
	for( i = 0; i < MACHSERVICE_HASH_SIZE; i++ ) {
		LIST_FOREACH( msi, &jm->ms_hash[i], name_hash_sle ) {
			cnt += !msi->per_pid ? 1 : 0;
		}
	}

	if (cnt == 0) {
		goto out;
	}

	mig_allocate((vm_address_t *)&service_names, cnt * sizeof(service_names[0]));
	if (!job_assumes(j, service_names != NULL)) {
		goto out_bad;
	}

	mig_allocate((vm_address_t *)&service_jobs, cnt * sizeof(service_jobs[0]));
	if (!job_assumes(j, service_jobs != NULL)) {
		goto out_bad;
	}

	mig_allocate((vm_address_t *)&service_actives, cnt * sizeof(service_actives[0]));
	if (!job_assumes(j, service_actives != NULL)) {
		goto out_bad;
	}

	for( i = 0; i < MACHSERVICE_HASH_SIZE; i++ ) {
		LIST_FOREACH( msi, &jm->ms_hash[i], name_hash_sle ) {
			if( !msi->per_pid ) {
				strlcpy(service_names[cnt2], machservice_name(msi), sizeof(service_names[0]));
				strlcpy(service_jobs[cnt2], msi->job->label, sizeof(service_jobs[0]));
				service_actives[cnt2] = machservice_status(msi);
				cnt2++;
			}
		}
	}

	job_assumes(j, cnt == cnt2);

out:
	*servicenamesp = service_names;
	*servicejobsp = service_jobs;
	*serviceactivesp = service_actives;
	*servicenames_cnt = *servicejobs_cnt = *serviceactives_cnt = cnt;

	return BOOTSTRAP_SUCCESS;

out_bad:
	if (service_names) {
		mig_deallocate((vm_address_t)service_names, cnt * sizeof(service_names[0]));
	}
	if (service_jobs) {
		mig_deallocate((vm_address_t)service_jobs, cnt * sizeof(service_jobs[0]));
	}
	if (service_actives) {
		mig_deallocate((vm_address_t)service_actives, cnt * sizeof(service_actives[0]));
	}

	return BOOTSTRAP_NO_MEMORY;
}

kern_return_t
job_mig_lookup_children(job_t j,	mach_port_array_t *child_ports,					mach_msg_type_number_t *child_ports_cnt, 
									name_array_t *child_names,						mach_msg_type_number_t *child_names_cnt, 
									bootstrap_property_array_t *child_properties,	mach_msg_type_number_t *child_properties_cnt)
{
	kern_return_t kr = BOOTSTRAP_NO_MEMORY;
	if( !launchd_assumes(j != NULL) ) {
		return BOOTSTRAP_NO_MEMORY;
	}
	
	struct ldcred *ldc = runtime_get_caller_creds();
	
	/* Only allow root processes to look up children, even if we're in the per-user launchd.
	 * Otherwise, this could be used to cross sessions, which counts as a security vulnerability
	 * in a non-flat namespace.
	 */
	if( ldc->euid != 0 ) {
		job_log(j, LOG_WARNING, "Attempt to look up children of bootstrap by unprivileged job.");
		return BOOTSTRAP_NOT_PRIVILEGED;
	}
	
	unsigned int cnt = 0;
	
	jobmgr_t jmr = j->mgr;
	jobmgr_t jmi = NULL;
	SLIST_FOREACH( jmi, &jmr->submgrs, sle ) {
		cnt++;
	}
	
	/* Find our per-user launchds if we're PID 1. */
	job_t ji = NULL;
	if( pid1_magic ) {
		LIST_FOREACH( ji, &jmr->jobs, sle ) {
			cnt += ji->per_user ? 1 : 0;
		}
	}
	
	if( cnt == 0 ) {
		return BOOTSTRAP_NO_CHILDREN;
	}
	
	mach_port_array_t _child_ports = NULL;
	mig_allocate((vm_address_t *)&_child_ports, cnt * sizeof(_child_ports[0]));
	if( !job_assumes(j, _child_ports != NULL) ) {
		kr = BOOTSTRAP_NO_MEMORY;
		goto out_bad;
	}
	
	name_array_t _child_names = NULL;
	mig_allocate((vm_address_t *)&_child_names, cnt * sizeof(_child_names[0]));
	if( !job_assumes(j, _child_names != NULL) ) {
		kr = BOOTSTRAP_NO_MEMORY;
		goto out_bad;
	}
	
	bootstrap_property_array_t _child_properties = NULL;
	mig_allocate((vm_address_t *)&_child_properties, cnt * sizeof(_child_properties[0]));
	if( !job_assumes(j, _child_properties != NULL) ) {
		kr = BOOTSTRAP_NO_MEMORY;
		goto out_bad;
	}
	
	unsigned int cnt2 = 0;
	SLIST_FOREACH( jmi, &jmr->submgrs, sle ) {
		if( jobmgr_assumes(jmi, launchd_mport_make_send(jmi->jm_port) == KERN_SUCCESS) ) {
			_child_ports[cnt2] = jmi->jm_port;
		} else {
			_child_ports[cnt2] = MACH_PORT_NULL;
		}
		
		strlcpy(_child_names[cnt2], jmi->name, sizeof(_child_names[0]));
		_child_properties[cnt2] = jmi->properties;
		
		cnt2++;
	}
	
	if( pid1_magic ) LIST_FOREACH( ji, &jmr->jobs, sle ) {
		if( ji->per_user ) {
			if( job_assumes(ji, SLIST_FIRST(&ji->machservices)->per_user_hack == true) ) {
				mach_port_t port = machservice_port(SLIST_FIRST(&ji->machservices));
				
				if( job_assumes(ji, launchd_mport_copy_send(port) == KERN_SUCCESS) ) {
					_child_ports[cnt2] = port;
				} else {
					_child_ports[cnt2] = MACH_PORT_NULL;
				}
			} else {
				_child_ports[cnt2] = MACH_PORT_NULL;
			}
			
			strlcpy(_child_names[cnt2], ji->label, sizeof(_child_names[0]));
			_child_properties[cnt2] |= BOOTSTRAP_PROPERTY_PERUSER;
			
			cnt2++;
		}
	}
	
	*child_names_cnt = cnt;
	*child_ports_cnt = cnt;
	*child_properties_cnt = cnt;
	
	*child_names = _child_names;
	*child_ports = _child_ports;
	*child_properties = _child_properties;
	
	unsigned int i = 0;
	for( i = 0; i < cnt; i++ ) {
		job_log(j, LOG_DEBUG, "child_names[%u] = %s", i, (char *)_child_names[i]);
	}
	
	return BOOTSTRAP_SUCCESS;
out_bad:
	if( _child_ports ) {
		mig_deallocate((vm_address_t)_child_ports, cnt * sizeof(_child_ports[0]));
	}
	
	if( _child_names ) {
		mig_deallocate((vm_address_t)_child_names, cnt * sizeof(_child_ports[0]));
	}
	
	if( _child_properties ) {
		mig_deallocate((vm_address_t)_child_properties, cnt * sizeof(_child_properties[0]));
	}
	
	return kr;
}

kern_return_t
job_mig_transaction_count_for_pid(job_t j, pid_t p, int32_t *cnt, boolean_t *condemned)
{
	kern_return_t kr = KERN_FAILURE;
	struct ldcred *ldc = runtime_get_caller_creds();
	if( (ldc->euid != geteuid()) && (ldc->euid != 0) ) {
		return BOOTSTRAP_NOT_PRIVILEGED;
	}
	
	job_t j_for_pid = jobmgr_find_by_pid_deep(j->mgr, p, false);
	if( j_for_pid ) {
		if( j_for_pid->kill_via_shmem ) {
			if( j_for_pid->shmem ) {
				*cnt = j_for_pid->shmem->vp_shmem_transaction_cnt;
				*condemned = j_for_pid->shmem->vp_shmem_flags & VPROC_SHMEM_EXITING;
				*cnt += *condemned ? 1 : 0;
			} else {
				*cnt = 0;
				*condemned = false;
			}
			
			kr = BOOTSTRAP_SUCCESS;
		} else {
			kr = BOOTSTRAP_NO_MEMORY;
		}
	} else {
		kr = BOOTSTRAP_UNKNOWN_SERVICE;
	}
	
	return kr;
}

kern_return_t
job_mig_pid_is_managed(job_t j __attribute__((unused)), pid_t p, boolean_t *managed)
{
	struct ldcred *ldc = runtime_get_caller_creds();
	if( (ldc->euid != geteuid()) && (ldc->euid != 0) ) {
		return BOOTSTRAP_NOT_PRIVILEGED;
	}
	
	/* This is so loginwindow doesn't try to quit GUI apps that have been launched
	 * directly by launchd as agents.
	 */
	job_t j_for_pid = jobmgr_find_by_pid_deep(root_jobmgr, p, false);
	if( j_for_pid && !j_for_pid->anonymous && !j_for_pid->legacy_LS_job ) {
		*managed = true;
	}
	
	return BOOTSTRAP_SUCCESS;
}

kern_return_t
job_mig_port_for_label(job_t j __attribute__((unused)), name_t label, mach_port_t *mp)
{
	struct ldcred *ldc = runtime_get_caller_creds();
	kern_return_t kr = BOOTSTRAP_NOT_PRIVILEGED;
	
	mach_port_t _mp = MACH_PORT_NULL;
	if( !j->deny_job_creation && (ldc->euid == 0 || ldc->euid == geteuid()) ) {
		job_t target_j = job_find(label);
		if( jobmgr_assumes(root_jobmgr, target_j != NULL) ) {
			if( target_j->j_port == MACH_PORT_NULL ) {
				job_assumes(target_j, job_setup_machport(target_j) == true);
			}
			
			_mp = target_j->j_port;
			kr = _mp != MACH_PORT_NULL ? BOOTSTRAP_SUCCESS : BOOTSTRAP_NO_MEMORY;
		} else {
			kr = BOOTSTRAP_NO_MEMORY;
		}
	}

	*mp = _mp;
	return kr;
}

#if !TARGET_OS_EMBEDDED
kern_return_t
job_mig_set_security_session(job_t j, uuid_t uuid, mach_port_t session)
{
	uuid_string_t uuid_str;
	uuid_unparse(uuid, uuid_str);
	job_log(j, LOG_DEBUG, "Setting session %u for UUID %s...", session, uuid_str);
	
	job_t ji = NULL, jt = NULL;
	LIST_FOREACH_SAFE( ji, &s_needing_sessions, sle, jt ) {
		uuid_string_t uuid_str2;
		uuid_unparse(ji->expected_audit_uuid, uuid_str2);

		if( uuid_compare(uuid, ji->expected_audit_uuid) == 0 ) {
			uuid_clear(ji->expected_audit_uuid);
			if( session != MACH_PORT_NULL  ) {
				job_log(ji, LOG_DEBUG, "Job should join session with port %u", session);
				mach_port_mod_refs(mach_task_self(), session, MACH_PORT_RIGHT_SEND, 1);
			} else {
				job_log(ji, LOG_DEBUG, "No session to set for job. Using our session.");
			}
			
			ji->audit_session = session;
			LIST_REMOVE(ji, needing_session_sle);
			job_dispatch(ji, false);
		}
	}
	
	/* Each job that the session port was set for holds a reference. At the end of
	 * the loop, there will be one extra reference belonging to this MiG protocol.
	 * We need to release it so that the session goes away when all the jobs
	 * referencing it are unloaded.
	 */
	mach_port_deallocate(mach_task_self(), session);

	return KERN_SUCCESS;
}
#else
kern_return_t
job_mig_set_security_session(job_t j __attribute__((unused)), uuid_t uuid __attribute__((unused)), mach_port_t session __attribute__((unused)))
{
	return KERN_SUCCESS;
}
#endif

jobmgr_t 
jobmgr_find_by_name(jobmgr_t jm, const char *where)
{
	jobmgr_t jmi, jmi2;

	/* NULL is only passed for our custom API for LaunchServices. If that is the case, we do magic. */
	if (where == NULL) {
		if (strcasecmp(jm->name, VPROCMGR_SESSION_LOGINWINDOW) == 0) {
			where = VPROCMGR_SESSION_LOGINWINDOW;
		} else {
			where = VPROCMGR_SESSION_AQUA;
		}
	}

	if (strcasecmp(jm->name, where) == 0) {
		return jm;
	}
	
	if( strcasecmp(where, VPROCMGR_SESSION_BACKGROUND) == 0 && !pid1_magic ) {
		jmi = root_jobmgr;
		goto jm_found;
	}

	SLIST_FOREACH(jmi, &root_jobmgr->submgrs, sle) {	
		if (unlikely(jmi->shutting_down)) {
			continue;
		} else if (strcasecmp(jmi->name, where) == 0) {
			goto jm_found;
		} else if (strcasecmp(jmi->name, VPROCMGR_SESSION_BACKGROUND) == 0 && pid1_magic) {
			SLIST_FOREACH(jmi2, &jmi->submgrs, sle) {
				if (strcasecmp(jmi2->name, where) == 0) {
					jmi = jmi2;
					goto jm_found;
				}
			}
		}
	}
	
jm_found:
	return jmi;
}

kern_return_t
job_mig_move_subset(job_t j, mach_port_t target_subset, name_t session_type, mach_port_t audit_session, uint64_t flags)
{
	mach_msg_type_number_t l2l_i, l2l_port_cnt = 0;
	mach_port_array_t l2l_ports = NULL;
	mach_port_t reqport, rcvright;
	kern_return_t kr = 1;
	launch_data_t out_obj_array = NULL;
	struct ldcred *ldc = runtime_get_caller_creds();
	jobmgr_t jmr = NULL;

	if (!launchd_assumes(j != NULL)) {
		return BOOTSTRAP_NO_MEMORY;
	}

	if (job_mig_intran2(root_jobmgr, target_subset, ldc->pid)) {
		job_log(j, LOG_ERR, "Moving a session to ourself is bogus.");

		kr = BOOTSTRAP_NOT_PRIVILEGED;
		goto out;
	}

	job_log(j, LOG_DEBUG, "Move subset attempt: 0x%x", target_subset);

	kr = _vproc_grab_subset(target_subset, &reqport, &rcvright, &out_obj_array, &l2l_ports, &l2l_port_cnt);

	if (!job_assumes(j, kr == 0)) {
		goto out;
	}

	launchd_assert(launch_data_array_get_count(out_obj_array) == l2l_port_cnt);

	if (!job_assumes(j, (jmr = jobmgr_new(j->mgr, reqport, rcvright, false, session_type, false, audit_session)) != NULL)) {
		kr = BOOTSTRAP_NO_MEMORY;
		goto out;
	}

	jmr->properties |= BOOTSTRAP_PROPERTY_MOVEDSUBSET;

	/* This is a hack. We should be doing this in jobmgr_new(), but since we're in the middle of
	 * processing an IPC request, we'll do this action before the new job manager can get any IPC
	 * requests. This serialization is guaranteed since we are single-threaded in that respect.
	 */
	if( flags & LAUNCH_GLOBAL_ON_DEMAND ) {
		/* This is so awful. */
		/* Remove the job from its current job manager. */
		LIST_REMOVE(j, sle);
		LIST_REMOVE(j, pid_hash_sle);

		/* Put the job into the target job manager. */
		LIST_INSERT_HEAD(&jmr->jobs, j, sle);
		LIST_INSERT_HEAD(&jmr->active_jobs[ACTIVE_JOB_HASH(j->p)], j, pid_hash_sle);
		
		j->mgr = jmr;
		job_set_global_on_demand(j, true);
	}
	
	for (l2l_i = 0; l2l_i < l2l_port_cnt; l2l_i++) {
		launch_data_t tmp, obj_at_idx;
		struct machservice *ms;
		job_t j_for_service;
		const char *serv_name;
		pid_t target_pid;
		bool serv_perpid;

		job_assumes(j, obj_at_idx = launch_data_array_get_index(out_obj_array, l2l_i));
		job_assumes(j, tmp = launch_data_dict_lookup(obj_at_idx, TAKE_SUBSET_PID));
		target_pid = (pid_t)launch_data_get_integer(tmp);
		job_assumes(j, tmp = launch_data_dict_lookup(obj_at_idx, TAKE_SUBSET_PERPID));
		serv_perpid = launch_data_get_bool(tmp);
		job_assumes(j, tmp = launch_data_dict_lookup(obj_at_idx, TAKE_SUBSET_NAME));
		serv_name = launch_data_get_string(tmp);

		j_for_service = jobmgr_find_by_pid(jmr, target_pid, true);

		if (unlikely(!j_for_service)) {
			/* The PID probably exited */
			job_assumes(j, launchd_mport_deallocate(l2l_ports[l2l_i]) == KERN_SUCCESS);
			continue;
		}

		if (likely(ms = machservice_new(j_for_service, serv_name, &l2l_ports[l2l_i], serv_perpid))) {
			job_log(j, LOG_DEBUG, "Importing %s into new bootstrap.", serv_name);
			machservice_request_notifications(ms);
		}
	}

	kr = 0;

out:
	if (out_obj_array) {
		launch_data_free(out_obj_array);
	}

	if (l2l_ports) {
		mig_deallocate((vm_address_t)l2l_ports, l2l_port_cnt * sizeof(l2l_ports[0]));
	}

	if (kr == 0) {
		if (target_subset) {
			job_assumes(j, launchd_mport_deallocate(target_subset) == KERN_SUCCESS);
		}
	} else if (jmr) {
		jobmgr_shutdown(jmr);
	}

	return kr;
}

kern_return_t
job_mig_init_session(job_t j, name_t session_type, mach_port_t audit_session)
{
	job_t j2;
	
	kern_return_t kr = BOOTSTRAP_NO_MEMORY;
	if (j->mgr->session_initialized) {
		job_log(j, LOG_ERR, "Tried to initialize an already setup session!");
		kr = BOOTSTRAP_NOT_PRIVILEGED;
	} else if (strcmp(session_type, VPROCMGR_SESSION_LOGINWINDOW) == 0) {
		jobmgr_t jmi;
		
		/*
		 * 5330262
		 *
		 * We're working around LoginWindow and the WindowServer.
		 *
		 * In practice, there is only one LoginWindow session. Unfortunately, for certain
		 * scenarios, the WindowServer spawns loginwindow, and in those cases, it frequently
		 * spawns a replacement loginwindow session before cleaning up the previous one.
		 *
		 * We're going to use the creation of a new LoginWindow context as a clue that the
		 * previous LoginWindow context is on the way out and therefore we should just
		 * kick-start the shutdown of it.
		 */
		
		SLIST_FOREACH(jmi, &root_jobmgr->submgrs, sle) {
			if (unlikely(jmi->shutting_down)) {
				continue;
			} else if (strcasecmp(jmi->name, session_type) == 0) {
				jobmgr_shutdown(jmi);
				break;
			}
		}
	}
	
	jobmgr_log(j->mgr, LOG_DEBUG, "Initializing as %s", session_type);
	strcpy(j->mgr->name_init, session_type);
	
	if (job_assumes(j, (j2 = jobmgr_init_session(j->mgr, session_type, false)))) {
		j2->audit_session = audit_session;
		job_assumes(j, job_dispatch(j2, true));
		kr = BOOTSTRAP_SUCCESS;
	}
	
	return kr;
}

kern_return_t
job_mig_switch_to_session(job_t j, mach_port_t requestor_port, name_t session_name, mach_port_t audit_session, mach_port_t *new_bsport)
{
	job_log(j, LOG_DEBUG, "Job wants to move to %s session.", session_name);
	
	if( !job_assumes(j, pid1_magic == false) ) {
		job_log(j, LOG_WARNING, "Switching sessions is not allowed in the system Mach bootstrap.");
		return BOOTSTRAP_NOT_PRIVILEGED;
	}
	
	if( !j->anonymous ) {
		job_log(j, LOG_NOTICE, "Non-anonymous job tried to switch sessions. Please use LimitLoadToSessionType instead.");
		return BOOTSTRAP_NOT_PRIVILEGED;
	}
	
	jobmgr_t target_jm = jobmgr_find_by_name(root_jobmgr, session_name);
	if( target_jm == j->mgr ) {
		job_log(j, LOG_DEBUG, "Job is already in its desired session (%s).", session_name);
		*new_bsport = target_jm->jm_port;
		return BOOTSTRAP_SUCCESS;
	}
	
	if( !target_jm ) {
		target_jm = jobmgr_new(j->mgr, requestor_port, MACH_PORT_NULL, false, session_name, false, audit_session);
		if( !target_jm ) {
			mach_port_deallocate(mach_task_self(), audit_session);
		} else {
			target_jm->properties |= BOOTSTRAP_PROPERTY_IMPLICITSUBSET;
		}
	}
	
	if( !job_assumes(j, target_jm != NULL) ) {
		job_log(j, LOG_WARNING, "Could not find %s session!", session_name);
		return BOOTSTRAP_NO_MEMORY;
	}
	
	/* Remove the job from it's current job manager. */
	LIST_REMOVE(j, sle);
	LIST_REMOVE(j, pid_hash_sle);

	job_t ji = NULL, jit = NULL;
	LIST_FOREACH_SAFE( ji, &j->mgr->global_env_jobs, global_env_sle, jit ) {
		if( ji == j ) {
			LIST_REMOVE(ji, global_env_sle);
			break;
		}
	}
	
	/* Put the job into the target job manager. */
	LIST_INSERT_HEAD(&target_jm->jobs, j, sle);
	LIST_INSERT_HEAD(&target_jm->active_jobs[ACTIVE_JOB_HASH(j->p)], j, pid_hash_sle);
	
	if( ji ) {
		LIST_INSERT_HEAD(&target_jm->global_env_jobs, j, global_env_sle);
	}
	
	/* Move our Mach services over if we're not in a flat namespace. */
	if( !g_flat_mach_namespace && !SLIST_EMPTY(&j->machservices) ) {
		struct machservice *msi = NULL, *msit = NULL;
		SLIST_FOREACH_SAFE( msi, &j->machservices, sle, msit ) {
			LIST_REMOVE(msi, name_hash_sle);
			LIST_INSERT_HEAD(&target_jm->ms_hash[hash_ms(msi->name)], msi, name_hash_sle);
		}
	}
	
	j->mgr = target_jm;
	j->migratory = true;
	*new_bsport = target_jm->jm_port;
	
	/* Anonymous jobs which move around are particularly interesting to us, so we want to
	 * stick around while they're still around.
	 * For example, login calls into the PAM launchd module, which moves the process into
	 * the StandardIO session by default. So we'll hold a reference on that job to prevent
	 * ourselves from going away.
	 */
	runtime_add_ref();
	
	return KERN_SUCCESS;
}

kern_return_t
job_mig_take_subset(job_t j, mach_port_t *reqport, mach_port_t *rcvright,
		vm_offset_t *outdata, mach_msg_type_number_t *outdataCnt,
		mach_port_array_t *portsp, unsigned int *ports_cnt)
{
	launch_data_t tmp_obj, tmp_dict, outdata_obj_array = NULL;
	mach_port_array_t ports = NULL;
	unsigned int cnt = 0, cnt2 = 0;
	size_t packed_size;
	struct machservice *ms;
	jobmgr_t jm;
	job_t ji;

	if (!launchd_assumes(j != NULL)) {
		return BOOTSTRAP_NO_MEMORY;
	}

	jm = j->mgr;

	if (unlikely(!pid1_magic)) {
		job_log(j, LOG_ERR, "Only the system launchd will transfer Mach sub-bootstraps.");
		return BOOTSTRAP_NOT_PRIVILEGED;
	}
	if (unlikely(jobmgr_parent(jm) == NULL)) {
		job_log(j, LOG_ERR, "Root Mach bootstrap cannot be transferred.");
		return BOOTSTRAP_NOT_PRIVILEGED;
	}
	if (unlikely(strcasecmp(jm->name, VPROCMGR_SESSION_AQUA) == 0)) {
		job_log(j, LOG_ERR, "Cannot transfer a setup GUI session.");
		return BOOTSTRAP_NOT_PRIVILEGED;
	}
	if (unlikely(!j->anonymous)) {
		job_log(j, LOG_ERR, "Only the anonymous job can transfer Mach sub-bootstraps.");
		return BOOTSTRAP_NOT_PRIVILEGED;
	}

	job_log(j, LOG_DEBUG, "Transferring sub-bootstrap to the per session launchd.");

	outdata_obj_array = launch_data_alloc(LAUNCH_DATA_ARRAY);
	if (!job_assumes(j, outdata_obj_array)) {
		goto out_bad;
	}

	*outdataCnt = 20 * 1024 * 1024;
	mig_allocate(outdata, *outdataCnt);
	if (!job_assumes(j, *outdata != 0)) {
		return 1;
	}

	LIST_FOREACH(ji, &j->mgr->jobs, sle) {
		if (!ji->anonymous) {
			continue;
		}
		SLIST_FOREACH(ms, &ji->machservices, sle) {
			cnt++;
		}
	}

	mig_allocate((vm_address_t *)&ports, cnt * sizeof(ports[0]));
	if (!job_assumes(j, ports != NULL)) {
		goto out_bad;
	}

	LIST_FOREACH(ji, &j->mgr->jobs, sle) {
		if (!ji->anonymous) {
			continue;
		}

		SLIST_FOREACH(ms, &ji->machservices, sle) {
			if (job_assumes(j, (tmp_dict = launch_data_alloc(LAUNCH_DATA_DICTIONARY)))) {
				job_assumes(j, launch_data_array_set_index(outdata_obj_array, tmp_dict, cnt2));
			} else {
				goto out_bad;
			}

			if (job_assumes(j, (tmp_obj = launch_data_new_string(machservice_name(ms))))) {
				job_assumes(j, launch_data_dict_insert(tmp_dict, tmp_obj, TAKE_SUBSET_NAME));
			} else {
				goto out_bad;
			}

			if (job_assumes(j, (tmp_obj = launch_data_new_integer((ms->job->p))))) {
				job_assumes(j, launch_data_dict_insert(tmp_dict, tmp_obj, TAKE_SUBSET_PID));
			} else {
				goto out_bad;
			}

			if (job_assumes(j, (tmp_obj = launch_data_new_bool((ms->per_pid))))) {
				job_assumes(j, launch_data_dict_insert(tmp_dict, tmp_obj, TAKE_SUBSET_PERPID));
			} else {
				goto out_bad;
			}

			ports[cnt2] = machservice_port(ms);

			/* Increment the send right by one so we can shutdown the jobmgr cleanly */
			jobmgr_assumes(jm, (errno = mach_port_mod_refs(mach_task_self(), ports[cnt2], MACH_PORT_RIGHT_SEND, 1)) == 0);
			cnt2++;
		}
	}

	job_assumes(j, cnt == cnt2);

	runtime_ktrace0(RTKT_LAUNCHD_DATA_PACK);
	packed_size = launch_data_pack(outdata_obj_array, (void *)*outdata, *outdataCnt, NULL, NULL);
	if (!job_assumes(j, packed_size != 0)) {
		goto out_bad;
	}

	launch_data_free(outdata_obj_array);

	*portsp = ports;
	*ports_cnt = cnt;

	*reqport = jm->req_port;
	*rcvright = jm->jm_port;

	jm->req_port = 0;
	jm->jm_port = 0;

	workaround_5477111 = j;

	jobmgr_shutdown(jm);

	return BOOTSTRAP_SUCCESS;

out_bad:
	if (outdata_obj_array) {
		launch_data_free(outdata_obj_array);
	}
	if (*outdata) {
		mig_deallocate(*outdata, *outdataCnt);
	}
	if (ports) {
		mig_deallocate((vm_address_t)ports, cnt * sizeof(ports[0]));
	}

	return BOOTSTRAP_NO_MEMORY;
}

kern_return_t
job_mig_subset(job_t j, mach_port_t requestorport, mach_port_t *subsetportp)
{
	int bsdepth = 0;
	jobmgr_t jmr;

	if (!launchd_assumes(j != NULL)) {
		return BOOTSTRAP_NO_MEMORY;
	}

	jmr = j->mgr;

	while ((jmr = jobmgr_parent(jmr)) != NULL) {
		bsdepth++;
	}

	/* Since we use recursion, we need an artificial depth for subsets */
	if (unlikely(bsdepth > 100)) {
		job_log(j, LOG_ERR, "Mach sub-bootstrap create request failed. Depth greater than: %d", bsdepth);
		return BOOTSTRAP_NO_MEMORY;
	}

	char name[NAME_MAX];
	snprintf(name, sizeof(name), "%s[%i].subset.%i", j->anonymous ? j->prog : j->label, j->p, MACH_PORT_INDEX(requestorport));

	if (!job_assumes(j, (jmr = jobmgr_new(j->mgr, requestorport, MACH_PORT_NULL, false, name, true, j->audit_session)) != NULL)) {
		if (unlikely(requestorport == MACH_PORT_NULL)) {
			return BOOTSTRAP_NOT_PRIVILEGED;
		}
		return BOOTSTRAP_NO_MEMORY;
	}

	*subsetportp = jmr->jm_port;
	jmr->properties |= BOOTSTRAP_PROPERTY_EXPLICITSUBSET;
	
	job_log(j, LOG_DEBUG, "Job created a subset named \"%s\"", jmr->name);
	return BOOTSTRAP_SUCCESS;
}

kern_return_t
job_mig_embedded_wait(job_t j, name_t targetlabel, integer_t *waitstatus)
{
	job_t otherj;

	if (!launchd_assumes(j != NULL)) {
		return BOOTSTRAP_NO_MEMORY;
	}

	if (unlikely(!(otherj = job_find(targetlabel)))) {
		return BOOTSTRAP_UNKNOWN_SERVICE;
	}

	*waitstatus = j->last_exit_status;

	return 0;
}

kern_return_t
job_mig_kickstart(job_t j, name_t targetlabel, pid_t *out_pid, mach_port_t *out_name_port, mach_port_t *obsrvr_port, unsigned int flags)
{
	struct ldcred *ldc = runtime_get_caller_creds();
	job_t otherj;

	if (!launchd_assumes(j != NULL)) {
		return BOOTSTRAP_NO_MEMORY;
	}

	if (unlikely(!(otherj = job_find(targetlabel)))) {
		return BOOTSTRAP_UNKNOWN_SERVICE;
	}

#if TARGET_OS_EMBEDDED
	bool allow_non_root_kickstart = j->username && otherj->username && ( strcmp(j->username, otherj->username) == 0 );
#else
	bool allow_non_root_kickstart = false;
#endif

	if( ldc->euid != 0 && ldc->euid != geteuid() && !allow_non_root_kickstart ) {
		return BOOTSTRAP_NOT_PRIVILEGED;
	}

	if( otherj->p && (flags & VPROCFLAG_STALL_JOB_EXEC) ) {
		return BOOTSTRAP_SERVICE_ACTIVE;
	}

	otherj->stall_before_exec = ( flags & VPROCFLAG_STALL_JOB_EXEC );
	otherj = job_dispatch(otherj, true);

	if (!job_assumes(j, otherj && otherj->p)) {
		/* <rdar://problem/6787083> Clear this flag if we failed to start the job. */
		otherj->stall_before_exec = false;
		return BOOTSTRAP_NO_MEMORY;
	}

	/* If any of these proceeding steps fail, we return an error to the client.
	 * the problem is that, if the client has requested the job be stalled before
	 * exec(2), the client won't be able to uncork the fork(2), leaving the job
	 * forever stalled until the client tries again and we successfully start
	 * the job.
	 *
	 * See <rdar://problem/6787083> for more about the implications.
	 *
	 * Fortunately, these next actions should pretty much never fail. In the
	 * future, we should look at cleaning up after these failures if the job
	 * was started in a stalled state.
	 */

	kern_return_t kr = task_name_for_pid(mach_task_self(), otherj->p, out_name_port);
	if (!job_assumes(j, kr == 0)) {
		return kr;
	}

	if (!job_setup_machport(otherj)) {
		return BOOTSTRAP_NO_MEMORY;
	}
	
	*obsrvr_port = otherj->j_port;
	*out_pid = otherj->p;

	return 0;
}

kern_return_t
job_mig_wait(job_t j, mach_port_t srp, integer_t *waitstatus)
{
#if 0
	if (!launchd_assumes(j != NULL)) {
		return BOOTSTRAP_NO_MEMORY;
	}
	return job_handle_mpm_wait(j, srp, waitstatus);
#else
	if( false ) {
		/* To make the compiler happy. */
		job_handle_mpm_wait(NULL, MACH_PORT_NULL, NULL);
	}
	struct ldcred *ldc = runtime_get_caller_creds();
	job_t calling_j = job_mig_intran2(j->mgr, MACH_PORT_NULL, ldc->pid);
	
	return job_mig_wait2(calling_j, j, srp, waitstatus, true);
#endif
}

kern_return_t
job_mig_wait2(job_t j, job_t target_j, mach_port_t srp, integer_t *status, boolean_t legacy)
{
	if( !launchd_assumes(j != NULL) ) {
		return BOOTSTRAP_NO_MEMORY;
	}
	if( !launchd_assumes(target_j != NULL) ) {
		return BOOTSTRAP_NO_MEMORY;
	}
	if( !launchd_assumes(status != NULL) ) {
		return BOOTSTRAP_NO_MEMORY;
	}
	
	if( target_j->p == 0 ) {
		*status = target_j->last_exit_status;
		return BOOTSTRAP_SUCCESS;
	}
	
	if( !job_assumes(j, waiting4exit_new(target_j, srp, legacy) == true) ) {
		return BOOTSTRAP_NO_MEMORY;
	}
	
	return MIG_NO_REPLY;
}

kern_return_t
job_mig_uncork_fork(job_t j)
{
	if (!launchd_assumes(j != NULL)) {
		return BOOTSTRAP_NO_MEMORY;
	}

	if (unlikely(!j->stall_before_exec)) {
		job_log(j, LOG_WARNING, "Attempt to uncork a job that isn't in the middle of a fork().");
		return 1;
	}

	job_uncork_fork(j);
	j->stall_before_exec = false;
	return 0;
}

kern_return_t
job_mig_spawn(job_t j, vm_offset_t indata, mach_msg_type_number_t indataCnt, mach_port_t audit_session, pid_t *child_pid, mach_port_t *obsvr_port)
{
	launch_data_t input_obj = NULL;
	size_t data_offset = 0;
	struct ldcred *ldc = runtime_get_caller_creds();
	job_t jr;

	if (!launchd_assumes(j != NULL)) {
		return BOOTSTRAP_NO_MEMORY;
	}

	if (unlikely(j->deny_job_creation)) {
		return BOOTSTRAP_NOT_PRIVILEGED;
	}

#if HAVE_SANDBOX
	if (unlikely(sandbox_check(ldc->pid, "job-creation", SANDBOX_FILTER_NONE) > 0)) {
		return BOOTSTRAP_NOT_PRIVILEGED;
	}
#endif

	if (unlikely(pid1_magic && ldc->euid && ldc->uid)) {
		job_log(j, LOG_DEBUG, "Punting spawn to per-user-context");
		return VPROC_ERR_TRY_PER_USER;
	}

	if (!job_assumes(j, indataCnt != 0)) {
		return 1;
	}

	runtime_ktrace0(RTKT_LAUNCHD_DATA_UNPACK);
	if (!job_assumes(j, (input_obj = launch_data_unpack((void *)indata, indataCnt, NULL, 0, &data_offset, NULL)) != NULL)) {
		return 1;
	}

	jobmgr_t target_jm = jobmgr_find_by_name(j->mgr, NULL);
	if( !jobmgr_assumes(j->mgr, target_jm != NULL) ) {
		jobmgr_log(j->mgr, LOG_NOTICE, "%s() can't find its session!", __func__);
		return 1;
	}

	jr = jobmgr_import2(target_jm ?: j->mgr, input_obj);
	
	if (!job_assumes(j, jr != NULL)) {
		switch (errno) {
		case EEXIST:
			return BOOTSTRAP_NAME_IN_USE;
		default:
			return BOOTSTRAP_NO_MEMORY;
		}
	}

	if (pid1_magic) {
		jr->mach_uid = ldc->uid;
	}

	jr->legacy_LS_job = true;
	jr->abandon_pg = true;
	jr->stall_before_exec = jr->wait4debugger;
	jr->wait4debugger = false;
	jr->audit_session = audit_session;
	uuid_clear(jr->expected_audit_uuid);

	jr = job_dispatch(jr, true);

	if (!job_assumes(j, jr != NULL)) {
		return BOOTSTRAP_NO_MEMORY;
	}

	if (!job_assumes(jr, jr->p)) {
		job_remove(jr);
		return BOOTSTRAP_NO_MEMORY;
	}

	if (!job_setup_machport(jr)) {
		job_remove(jr);
		return BOOTSTRAP_NO_MEMORY;
	}

	job_log(jr, LOG_DEBUG, "Spawned by PID %u: %s", j->p, j->label);

	*child_pid = jr->p;
	*obsvr_port = jr->j_port;

	mig_deallocate(indata, indataCnt);

	return BOOTSTRAP_SUCCESS;
}

void
jobmgr_init(bool sflag)
{
	const char *root_session_type = pid1_magic ? VPROCMGR_SESSION_SYSTEM : VPROCMGR_SESSION_BACKGROUND;
	SLIST_INIT(&s_curious_jobs);
	LIST_INIT(&s_needing_sessions);
	
	launchd_assert((root_jobmgr = jobmgr_new(NULL, MACH_PORT_NULL, MACH_PORT_NULL, sflag, root_session_type, false, MACH_PORT_NULL)) != NULL);
	
	uint32_t fflags = NOTE_ATTRIB | NOTE_LINK | NOTE_REVOKE | NOTE_EXTEND | NOTE_WRITE;
	s_no_hang_fd = open("/dev/autofs_nowait", O_EVTONLY | O_NONBLOCK);
	if( likely(s_no_hang_fd == -1) ) {
		if( jobmgr_assumes(root_jobmgr, (s_no_hang_fd = open("/dev", O_EVTONLY | O_NONBLOCK)) != -1) ) {
			jobmgr_assumes(root_jobmgr, kevent_mod((uintptr_t)s_no_hang_fd, EVFILT_VNODE, EV_ADD, fflags, 0, root_jobmgr) != -1);
		}
	}
	s_no_hang_fd = _fd(s_no_hang_fd);
}

size_t
our_strhash(const char *s)
{
	size_t c, r = 5381;

	/* djb2
	 * This algorithm was first reported by Dan Bernstein many years ago in comp.lang.c
	 */

	while ((c = *s++)) {
		r = ((r << 5) + r) + c; /* hash*33 + c */
	}

	return r;
}

size_t
hash_label(const char *label)
{
	return our_strhash(label) % LABEL_HASH_SIZE;
}

size_t
hash_ms(const char *msstr)
{
	return our_strhash(msstr) % MACHSERVICE_HASH_SIZE;
}

bool
waiting4removal_new(job_t j, mach_port_t rp)
{
	struct waiting_for_removal *w4r;

	if (!job_assumes(j, (w4r = malloc(sizeof(struct waiting_for_removal))) != NULL)) {
		return false;
	}

	w4r->reply_port = rp;

	SLIST_INSERT_HEAD(&j->removal_watchers, w4r, sle);

	return true;
}

void
waiting4removal_delete(job_t j, struct waiting_for_removal *w4r)
{
	job_assumes(j, job_mig_send_signal_reply(w4r->reply_port, 0) == 0);

	SLIST_REMOVE(&j->removal_watchers, w4r, waiting_for_removal, sle);

	free(w4r);
}

bool 
waiting4exit_new(job_t j, mach_port_t rp, bool legacy)
{
	struct waiting_for_exit *w4e = NULL;
	if( !job_assumes(j, (w4e = malloc(sizeof(struct waiting_for_exit))) != NULL) ) {
		return false;
	}
	
	w4e->rp = rp;
	w4e->legacy = legacy;
	LIST_INSERT_HEAD(&j->exit_watchers, w4e, sle);
	
	return true;
}

void
waiting4exit_delete(job_t j, struct waiting_for_exit *w4e)
{
	if( !w4e->legacy ) {
		job_assumes(j, job_mig_wait2_reply(w4e->rp, KERN_SUCCESS, j->last_exit_status, false) == KERN_SUCCESS);
	} else {
		job_assumes(j, job_mig_wait_reply(w4e->rp, KERN_SUCCESS, j->last_exit_status) == KERN_SUCCESS);
	}
	
	LIST_REMOVE(w4e, sle);
	
	free(w4e);
}

size_t
get_kern_max_proc(void)
{
	int mib[] = { CTL_KERN, KERN_MAXPROC };
	int max = 100;
	size_t max_sz = sizeof(max);
	
	launchd_assumes(sysctl(mib, 2, &max, &max_sz, NULL, 0) != -1);
	
	return max;
}

/* See rdar://problem/6271234 */
void
eliminate_double_reboot(void)
{
	if( unlikely(!pid1_magic) ) {
		return;
	}
	
	struct stat sb;
	const char *argv[] = { _PATH_BSHELL, "/etc/rc.deferred_install", NULL };
	char *try_again = "Will try again at next boot.";
	int result = ~0;
	
	if( unlikely(stat(argv[1], &sb) != -1) ) {
		jobmgr_log(root_jobmgr, LOG_DEBUG | LOG_CONSOLE, "Going to run deferred install script.");
		
		int wstatus;
		pid_t p;
		
		jobmgr_assumes(root_jobmgr, (errno = posix_spawnp(&p, argv[0], NULL, NULL, (char **)argv, environ)) == 0);
		
		if (errno) {
			jobmgr_log(root_jobmgr, LOG_WARNING | LOG_CONSOLE, "Couldn't run deferred install script! %s", try_again);
			goto out;
		}
		
		if( !jobmgr_assumes(root_jobmgr, waitpid(p, &wstatus, 0) != -1) ) {
			jobmgr_log(root_jobmgr, LOG_WARNING | LOG_CONSOLE, "Couldn't confirm that deferred install script exited successfully! %s", try_again);
			goto out;
		}
		
		if( jobmgr_assumes(root_jobmgr, WIFEXITED(wstatus) != 0) ) {
			if( jobmgr_assumes(root_jobmgr, (result = WEXITSTATUS(wstatus)) == EXIT_SUCCESS) ) {
				jobmgr_log(root_jobmgr, LOG_DEBUG | LOG_CONSOLE, "Deferred install script completed successfully.");
			} else {
				jobmgr_log(root_jobmgr, LOG_WARNING | LOG_CONSOLE, "Deferred install script exited with status %d. %s", WEXITSTATUS(wstatus), try_again);
			}
		} else {
			jobmgr_log(root_jobmgr, LOG_WARNING | LOG_CONSOLE, "Confirmed that deferred install script exited, but couldn't confirm that it was successful. %s", try_again);
		}
	}
out:
	if( result == 0 ) {
		/* If the unlink(2) was to fail, it would be most likely fail with EBUSY. All the other
		 * failure cases for unlink(2) don't apply when we're running under PID 1 and have verified
		 * that the file exists. Outside of someone deliberately messing with us (like if /etc/rc.deferredinstall
		 * is actually a looping sym-link or a mount point for a filesystem) and I/O errors, we should be good.
		 */
		if( !jobmgr_assumes(root_jobmgr, unlink(argv[1]) != -1) ) {
			jobmgr_log(root_jobmgr, LOG_WARNING | LOG_CONSOLE, "Deferred install script couldn't be removed!");
		}
	}
}

static void
simulate_pid1_crash(void)
{
	if( pid1_magic && g_simulate_pid1_crash ) {
		runtime_syslog(LOG_EMERG | LOG_CONSOLE, "About to simulate a crash.");
		raise(SIGSEGV);
	}
}

static void
jetsam_priority_from_job(job_t j, bool front, jetsam_priority_entry_t *jp)
{
	jp->pid = j->p;
	jp->flags |= front ? kJetsamFlagsFrontmost : 0;
}

static int
job_cmp(const job_t *lhs, const job_t *rhs)
{
	job_t _lhs = *lhs;
	job_t _rhs = *rhs;
	/* Sort in descending order. (Priority correlates to the soonishness with which you will be killed.) */
	if( _lhs->jetsam_priority > _rhs->jetsam_priority ) {
		return -1;
	} else if( _lhs->jetsam_priority < _rhs->jetsam_priority ) {
		return 1;
	}
	
	return 0;
}

int
launchd_set_jetsam_priorities(launch_data_t priorities)
{
	if( !launchd_assumes(launch_data_get_type(priorities) == LAUNCH_DATA_ARRAY) ) {
		return EINVAL;
	}
	
	jobmgr_t jm = NULL;
#if !TARGET_OS_EMBEDDED
	/* For testing. */
	jm = jobmgr_find_by_name(root_jobmgr, VPROCMGR_SESSION_AQUA);
	if( !launchd_assumes(jm != NULL) ) {
		return EINVAL;
	}
#else
	/* Since this is for embedded, we can assume that the root job manager holds the Jetsam jobs. */
	jm = root_jobmgr;
	
	if( !g_embedded_privileged_action ) {
		return EPERM;
	}
#endif
		
	size_t npris = launch_data_array_get_count(priorities);

	job_t ji = NULL;
	size_t i = 0;
	for( i = 0; i < npris; i++ ) {
		launch_data_t ldi = launch_data_array_get_index(priorities, i);
		if( !launchd_assumes(launch_data_get_type(ldi) == LAUNCH_DATA_DICTIONARY) ) {
			continue;
		}
		
		launch_data_t label = NULL;
		if( !launchd_assumes(label = launch_data_dict_lookup(ldi, LAUNCH_KEY_JETSAMLABEL)) ) {
			continue;
		}
		const char *_label = launch_data_get_string(label);
		
		ji = job_find(_label);
		if( !launchd_assumes(ji != NULL) ) {
			continue;
		}
		
		launch_data_t pri;
		long long _pri = 0;
		if( !launchd_assumes(pri = launch_data_dict_lookup(ldi, LAUNCH_KEY_JETSAMPRIORITY)) ) {
			continue;
		}
		_pri = launch_data_get_integer(pri);
		
		if( ji->jetsam_priority == LAUNCHD_JETSAM_PRIORITY_UNSET ) {
			LIST_INSERT_HEAD(&ji->mgr->jetsam_jobs, ji, jetsam_sle);
			ji->mgr->jetsam_jobs_cnt++;
		}
		ji->jetsam_priority = _pri;
		
		launch_data_t frontmost = NULL;
		if( !(frontmost = launch_data_dict_lookup(ldi, LAUNCH_KEY_JETSAMFRONTMOST)) ) {
			ji->jetsam_frontmost = false;
			continue;
		}
		ji->jetsam_frontmost = launch_data_get_bool(frontmost);
	}
	
	i = 0;
	job_t *jobs = (job_t *)calloc(jm->jetsam_jobs_cnt, sizeof(job_t));
	LIST_FOREACH( ji, &jm->jetsam_jobs, jetsam_sle ) {
		if( ji->p ) {
			jobs[i] = ji;
			i++;
		}
	}
	size_t totalpris = i;
	
	int result = EINVAL;
	if( launchd_assumes(totalpris > 0) ) {
		qsort((void *)jobs, totalpris, sizeof(job_t), (int (*)(const void *, const void *))job_cmp);
		
		jetsam_priority_entry_t *jpris = (jetsam_priority_entry_t *)calloc(totalpris, sizeof(jetsam_priority_entry_t));
		if( !launchd_assumes(jpris != NULL) ) {
			result = ENOMEM;
		} else {
			for( i = 0; i < totalpris; i++ ) {
				jetsam_priority_from_job(jobs[i], jobs[i]->jetsam_frontmost, &jpris[i]);
			}
			
			int _result = 0;
			launchd_assumes((_result = sysctlbyname("kern.memorystatus_priority_list", NULL, NULL, &jpris[0], totalpris * sizeof(jetsam_priority_entry_t))) != -1);
			result = _result != 0 ? errno : 0;
			
			free(jpris);
		}
	}
	free(jobs);
	
	return result;
}
