/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* The purpose of this MPM is to fix the design flaws in the threaded
 * model.  Because of the way that pthreads and mutex locks interact,
 * it is basically impossible to cleanly gracefully shutdown a child
 * process if multiple threads are all blocked in accept.  This model
 * fixes those problems.
 */

#include "apr.h"
#include "apr_portable.h"
#include "apr_strings.h"
#include "apr_file_io.h"
#include "apr_thread_proc.h"
#include "apr_signal.h"
#include "apr_thread_mutex.h"
#include "apr_proc_mutex.h"
#include "apr_poll.h"

#include <stdlib.h>

#define APR_WANT_STRFUNC
#include "apr_want.h"

#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif
#if APR_HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if APR_HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_SYS_PROCESSOR_H
#include <sys/processor.h> /* for bindprocessor() */
#endif

#if !APR_HAS_THREADS
#error The Worker MPM requires APR threads, but they are unavailable.
#endif

#include "ap_config.h"
#include "httpd.h"
#include "http_main.h"
#include "http_log.h"
#include "http_config.h"        /* for read_config */
#include "http_core.h"          /* for get_remote_host */
#include "http_connection.h"
#include "ap_mpm.h"
#include "mpm_common.h"
#include "ap_listen.h"
#include "scoreboard.h"
#include "mpm_fdqueue.h"
#include "mpm_default.h"
#include "util_mutex.h"
#include "unixd.h"

#include <signal.h>
#include <limits.h>             /* for INT_MAX */

/* Limit on the total --- clients will be locked out if more servers than
 * this are needed.  It is intended solely to keep the server from crashing
 * when things get out of hand.
 *
 * We keep a hard maximum number of servers, for two reasons --- first off,
 * in case something goes seriously wrong, we want to stop the fork bomb
 * short of actually crashing the machine we're running on by filling some
 * kernel table.  Secondly, it keeps the size of the scoreboard file small
 * enough that we can read the whole thing without worrying too much about
 * the overhead.
 */
#ifndef DEFAULT_SERVER_LIMIT
#define DEFAULT_SERVER_LIMIT 16
#endif

/* Admin can't tune ServerLimit beyond MAX_SERVER_LIMIT.  We want
 * some sort of compile-time limit to help catch typos.
 */
#ifndef MAX_SERVER_LIMIT
#define MAX_SERVER_LIMIT 20000
#endif

/* Limit on the threads per process.  Clients will be locked out if more than
 * this  * server_limit are needed.
 *
 * We keep this for one reason it keeps the size of the scoreboard file small
 * enough that we can read the whole thing without worrying too much about
 * the overhead.
 */
#ifndef DEFAULT_THREAD_LIMIT
#define DEFAULT_THREAD_LIMIT 64
#endif

/* Admin can't tune ThreadLimit beyond MAX_THREAD_LIMIT.  We want
 * some sort of compile-time limit to help catch typos.
 */
#ifndef MAX_THREAD_LIMIT
#define MAX_THREAD_LIMIT 20000
#endif

/*
 * Actual definitions of config globals
 */

static int threads_per_child = 0;     /* Worker threads per child */
static int ap_daemons_to_start = 0;
static int min_spare_threads = 0;
static int max_spare_threads = 0;
static int ap_daemons_limit = 0;
static int max_workers = 0;
static int server_limit = 0;
static int thread_limit = 0;
static int had_healthy_child = 0;
static volatile int dying = 0;
static int workers_may_exit = 0;
static int start_thread_may_exit = 0;
static int listener_may_exit = 0;
static int listener_is_wakeable = 0; /* Pollset supports APR_POLLSET_WAKEABLE */
static int requests_this_child;
static int num_listensocks = 0;
static int resource_shortage = 0;
static fd_queue_t *worker_queue;
static fd_queue_info_t *worker_queue_info;
static apr_pollset_t *worker_pollset;


/* data retained by worker across load/unload of the module
 * allocated on first call to pre-config hook; located on
 * subsequent calls to pre-config hook
 */
typedef struct worker_retained_data {
    ap_unixd_mpm_retained_data *mpm;

    int first_server_limit;
    int first_thread_limit;
    int sick_child_detected;
    int maxclients_reported;
    int near_maxclients_reported;
    /*
     * The max child slot ever assigned, preserved across restarts.  Necessary
     * to deal with MaxRequestWorkers changes across AP_SIG_GRACEFUL restarts.
     * We use this value to optimize routines that have to scan the entire
     * scoreboard.
     */
    int max_daemons_limit;
    /*
     * idle_spawn_rate is the number of children that will be spawned on the
     * next maintenance cycle if there aren't enough idle servers.  It is
     * maintained per listeners bucket, doubled up to MAX_SPAWN_RATE, and
     * reset only when a cycle goes by without the need to spawn.
     */
    int *idle_spawn_rate;
#ifndef MAX_SPAWN_RATE
#define MAX_SPAWN_RATE        (32)
#endif
    int hold_off_on_exponential_spawning;
} worker_retained_data;
static worker_retained_data *retained;

typedef struct worker_child_bucket {
    ap_pod_t *pod;
    ap_listen_rec *listeners;
    apr_proc_mutex_t *mutex;
} worker_child_bucket;
static worker_child_bucket *all_buckets, /* All listeners buckets */
                           *my_bucket;   /* Current child bucket */

#define MPM_CHILD_PID(i) (ap_scoreboard_image->parent[i].pid)

/* The structure used to pass unique initialization info to each thread */
typedef struct {
    int pid;
    int tid;
    int sd;
} proc_info;

/* Structure used to pass information to the thread responsible for
 * creating the rest of the threads.
 */
typedef struct {
    apr_thread_t **threads;
    apr_thread_t *listener;
    int child_num_arg;
    apr_threadattr_t *threadattr;
} thread_starter;

#define ID_FROM_CHILD_THREAD(c, t)    ((c * thread_limit) + t)

/* The worker MPM respects a couple of runtime flags that can aid
 * in debugging. Setting the -DNO_DETACH flag will prevent the root process
 * from detaching from its controlling terminal. Additionally, setting
 * the -DONE_PROCESS flag (which implies -DNO_DETACH) will get you the
 * child_main loop running in the process which originally started up.
 * This gives you a pretty nice debugging environment.  (You'll get a SIGHUP
 * early in standalone_main; just continue through.  This is the server
 * trying to kill off any child processes which it might have lying
 * around --- Apache doesn't keep track of their pids, it just sends
 * SIGHUP to the process group, ignoring it in the root process.
 * Continue through and you'll be fine.).
 */

static int one_process = 0;

#ifdef DEBUG_SIGSTOP
int raise_sigstop_flags;
#endif

static apr_pool_t *pconf;                 /* Pool for config stuff */
static apr_pool_t *pchild;                /* Pool for httpd child stuff */
static apr_pool_t *pruntime;              /* Pool for MPM threads stuff */

static pid_t ap_my_pid; /* Linux getpid() doesn't work except in main
                           thread. Use this instead */
static pid_t parent_pid;
static apr_os_thread_t *listener_os_thread;

#ifdef SINGLE_LISTEN_UNSERIALIZED_ACCEPT
#define SAFE_ACCEPT(stmt) (ap_listeners->next ? (stmt) : APR_SUCCESS)
#else
#define SAFE_ACCEPT(stmt) (stmt)
#endif

/* The LISTENER_SIGNAL signal will be sent from the main thread to the
 * listener thread to wake it up for graceful termination (what a child
 * process from an old generation does when the admin does "apachectl
 * graceful").  This signal will be blocked in all threads of a child
 * process except for the listener thread.
 */
#define LISTENER_SIGNAL     SIGHUP

/* The WORKER_SIGNAL signal will be sent from the main thread to the
 * worker threads during an ungraceful restart or shutdown.
 * This ensures that on systems (i.e., Linux) where closing the worker
 * socket doesn't awake the worker thread when it is polling on the socket
 * (especially in apr_wait_for_io_or_timeout() when handling
 * Keep-Alive connections), close_worker_sockets() and join_workers()
 * still function in timely manner and allow ungraceful shutdowns to
 * proceed to completion.  Otherwise join_workers() doesn't return
 * before the main process decides the child process is non-responsive
 * and sends a SIGKILL.
 */
#define WORKER_SIGNAL       AP_SIG_GRACEFUL

/* An array of socket descriptors in use by each thread used to
 * perform a non-graceful (forced) shutdown of the server. */
static apr_socket_t **worker_sockets;

static void close_worker_sockets(void)
{
    int i;
    for (i = 0; i < threads_per_child; i++) {
        if (worker_sockets[i]) {
            apr_socket_close(worker_sockets[i]);
            worker_sockets[i] = NULL;
        }
    }
}

static void wakeup_listener(void)
{
    listener_may_exit = 1;

    /* Unblock the listener if it's poll()ing */
    if (worker_pollset && listener_is_wakeable) {
        apr_pollset_wakeup(worker_pollset);
    }

    /* unblock the listener if it's waiting for a worker */
    ap_queue_info_term(worker_queue_info);

    if (!listener_os_thread) {
        /* XXX there is an obscure path that this doesn't handle perfectly:
         *     right after listener thread is created but before
         *     listener_os_thread is set, the first worker thread hits an
         *     error and starts graceful termination
         */
        return;
    }
    /*
     * we should just be able to "kill(ap_my_pid, LISTENER_SIGNAL)" on all
     * platforms and wake up the listener thread since it is the only thread
     * with SIGHUP unblocked, but that doesn't work on Linux
     */
#ifdef HAVE_PTHREAD_KILL
    pthread_kill(*listener_os_thread, LISTENER_SIGNAL);
#else
    kill(ap_my_pid, LISTENER_SIGNAL);
#endif
}

#define ST_INIT              0
#define ST_GRACEFUL          1
#define ST_UNGRACEFUL        2

static int terminate_mode = ST_INIT;

static void signal_threads(int mode)
{
    if (terminate_mode == mode) {
        return;
    }
    terminate_mode = mode;
    retained->mpm->mpm_state = AP_MPMQ_STOPPING;

    /* in case we weren't called from the listener thread, wake up the
     * listener thread
     */
    wakeup_listener();

    /* for ungraceful termination, let the workers exit now;
     * for graceful termination, the listener thread will notify the
     * workers to exit once it has stopped accepting new connections
     */
    if (mode == ST_UNGRACEFUL) {
        workers_may_exit = 1;
        ap_queue_interrupt_all(worker_queue);
        close_worker_sockets(); /* forcefully kill all current connections */
    }

    ap_run_child_stopping(pchild, mode == ST_GRACEFUL);
}

static int worker_query(int query_code, int *result, apr_status_t *rv)
{
    *rv = APR_SUCCESS;
    switch (query_code) {
        case AP_MPMQ_MAX_DAEMON_USED:
            *result = retained->max_daemons_limit;
            break;
        case AP_MPMQ_IS_THREADED:
            *result = AP_MPMQ_STATIC;
            break;
        case AP_MPMQ_IS_FORKED:
            *result = AP_MPMQ_DYNAMIC;
            break;
        case AP_MPMQ_HARD_LIMIT_DAEMONS:
            *result = server_limit;
            break;
        case AP_MPMQ_HARD_LIMIT_THREADS:
            *result = thread_limit;
            break;
        case AP_MPMQ_MAX_THREADS:
            *result = threads_per_child;
            break;
        case AP_MPMQ_MIN_SPARE_DAEMONS:
            *result = 0;
            break;
        case AP_MPMQ_MIN_SPARE_THREADS:
            *result = min_spare_threads;
            break;
        case AP_MPMQ_MAX_SPARE_DAEMONS:
            *result = 0;
            break;
        case AP_MPMQ_MAX_SPARE_THREADS:
            *result = max_spare_threads;
            break;
        case AP_MPMQ_MAX_REQUESTS_DAEMON:
            *result = ap_max_requests_per_child;
            break;
        case AP_MPMQ_MAX_DAEMONS:
            *result = ap_daemons_limit;
            break;
        case AP_MPMQ_MPM_STATE:
            *result = retained->mpm->mpm_state;
            break;
        case AP_MPMQ_GENERATION:
            *result = retained->mpm->my_generation;
            break;
        default:
            *rv = APR_ENOTIMPL;
            break;
    }
    return OK;
}

static void worker_note_child_killed(int childnum, pid_t pid, ap_generation_t gen)
{
    if (childnum != -1) { /* child had a scoreboard slot? */
        ap_run_child_status(ap_server_conf,
                            ap_scoreboard_image->parent[childnum].pid,
                            ap_scoreboard_image->parent[childnum].generation,
                            childnum, MPM_CHILD_EXITED);
        ap_scoreboard_image->parent[childnum].pid = 0;
    }
    else {
        ap_run_child_status(ap_server_conf, pid, gen, -1, MPM_CHILD_EXITED);
    }
}

static void worker_note_child_started(int slot, pid_t pid)
{
    ap_generation_t gen = retained->mpm->my_generation;
    ap_scoreboard_image->parent[slot].pid = pid;
    ap_scoreboard_image->parent[slot].generation = gen;
    ap_run_child_status(ap_server_conf, pid, gen, slot, MPM_CHILD_STARTED);
}

static void worker_note_child_lost_slot(int slot, pid_t newpid)
{
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf, APLOGNO(00263)
                 "pid %" APR_PID_T_FMT " taking over scoreboard slot from "
                 "%" APR_PID_T_FMT "%s",
                 newpid,
                 ap_scoreboard_image->parent[slot].pid,
                 ap_scoreboard_image->parent[slot].quiescing ?
                 " (quiescing)" : "");
    ap_run_child_status(ap_server_conf,
                        ap_scoreboard_image->parent[slot].pid,
                        ap_scoreboard_image->parent[slot].generation,
                        slot, MPM_CHILD_LOST_SLOT);
    /* Don't forget about this exiting child process, or we
     * won't be able to kill it if it doesn't exit by the
     * time the server is shut down.
     */
    ap_register_extra_mpm_process(ap_scoreboard_image->parent[slot].pid,
                                  ap_scoreboard_image->parent[slot].generation);
}

static const char *worker_get_name(void)
{
    return "worker";
}

/* a clean exit from a child with proper cleanup */
static void clean_child_exit(int code) __attribute__ ((noreturn));
static void clean_child_exit(int code)
{
    retained->mpm->mpm_state = AP_MPMQ_STOPPING;
    if (terminate_mode == ST_INIT) {
        ap_run_child_stopping(pchild, 0);
    }

    if (pchild) {
        apr_pool_destroy(pchild);
    }

    if (one_process) {
        worker_note_child_killed(/* slot */ 0, 0, 0);
    }

    exit(code);
}

static void just_die(int sig)
{
    clean_child_exit(0);
}

/*****************************************************************
 * Connection structures and accounting...
 */

static int child_fatal;

/*****************************************************************
 * Here follows a long bunch of generic server bookkeeping stuff...
 */

/*****************************************************************
 * Child process main loop.
 */

static void process_socket(apr_thread_t *thd, apr_pool_t *p, apr_socket_t *sock,
                           int my_child_num,
                           int my_thread_num, apr_bucket_alloc_t *bucket_alloc)
{
    conn_rec *current_conn;
    long conn_id = ID_FROM_CHILD_THREAD(my_child_num, my_thread_num);
    ap_sb_handle_t *sbh;

    ap_create_sb_handle(&sbh, p, my_child_num, my_thread_num);

    current_conn = ap_run_create_connection(p, ap_server_conf, sock,
                                            conn_id, sbh, bucket_alloc);
    if (current_conn) {
        current_conn->current_thread = thd;
        ap_process_connection(current_conn, sock);
        ap_lingering_close(current_conn);
    }
}

/* requests_this_child has gone to zero or below.  See if the admin coded
   "MaxConnectionsPerChild 0", and keep going in that case.  Doing it this way
   simplifies the hot path in worker_thread */
static void check_infinite_requests(void)
{
    if (ap_max_requests_per_child) {
        signal_threads(ST_GRACEFUL);
    }
    else {
        requests_this_child = INT_MAX;      /* keep going */
    }
}

static void unblock_signal(int sig)
{
    sigset_t sig_mask;

    sigemptyset(&sig_mask);
    sigaddset(&sig_mask, sig);
#if defined(SIGPROCMASK_SETS_THREAD_MASK)
    sigprocmask(SIG_UNBLOCK, &sig_mask, NULL);
#else
    pthread_sigmask(SIG_UNBLOCK, &sig_mask, NULL);
#endif
}

static void dummy_signal_handler(int sig)
{
    /* XXX If specifying SIG_IGN is guaranteed to unblock a syscall,
     *     then we don't need this goofy function.
     */
}

static void accept_mutex_error(const char *func, apr_status_t rv, int process_slot)
{
    int level = APLOG_EMERG;

    if (ap_scoreboard_image->parent[process_slot].generation !=
        ap_scoreboard_image->global->running_generation) {
        level = APLOG_DEBUG; /* common to get these at restart time */
    }
    else if (requests_this_child == INT_MAX
        || ((requests_this_child == ap_max_requests_per_child)
            && ap_max_requests_per_child)) {
        ap_log_error(APLOG_MARK, level, rv, ap_server_conf, APLOGNO(00272)
                     "apr_proc_mutex_%s failed "
                     "before this child process served any requests.",
                     func);
        clean_child_exit(APEXIT_CHILDSICK);
    }
    ap_log_error(APLOG_MARK, level, rv, ap_server_conf, APLOGNO(00273)
                 "apr_proc_mutex_%s failed. Attempting to "
                 "shutdown process gracefully.", func);
    signal_threads(ST_GRACEFUL);
}

static void * APR_THREAD_FUNC listener_thread(apr_thread_t *thd, void * dummy)
{
    proc_info * ti = dummy;
    int process_slot = ti->pid;
    void *csd = NULL;
    apr_pool_t *ptrans = NULL;            /* Pool for per-transaction stuff */
    apr_status_t rv;
    ap_listen_rec *lr = NULL;
    int have_idle_worker = 0;
    int last_poll_idx = 0;

    free(ti);

    /* Unblock the signal used to wake this thread up, and set a handler for
     * it.
     */
    apr_signal(LISTENER_SIGNAL, dummy_signal_handler);
    unblock_signal(LISTENER_SIGNAL);

    /* TODO: Switch to a system where threads reuse the results from earlier
       poll calls - manoj */
    while (1) {
        /* TODO: requests_this_child should be synchronized - aaron */
        if (requests_this_child <= 0) {
            check_infinite_requests();
        }
        if (listener_may_exit) break;

        if (!have_idle_worker) {
            rv = ap_queue_info_wait_for_idler(worker_queue_info, NULL);
            if (APR_STATUS_IS_EOF(rv)) {
                break; /* we've been signaled to die now */
            }
            else if (rv != APR_SUCCESS) {
                ap_log_error(APLOG_MARK, APLOG_EMERG, rv, ap_server_conf,
                             "apr_queue_info_wait failed. Attempting to "
                             " shutdown process gracefully.");
                signal_threads(ST_GRACEFUL);
                break;
            }
            have_idle_worker = 1;
        }

        /* We've already decremented the idle worker count inside
         * ap_queue_info_wait_for_idler. */

        if ((rv = SAFE_ACCEPT(apr_proc_mutex_lock(my_bucket->mutex)))
            != APR_SUCCESS) {

            if (!listener_may_exit) {
                accept_mutex_error("lock", rv, process_slot);
            }
            break;                    /* skip the lock release */
        }

        if (!my_bucket->listeners->next) {
            /* Only one listener, so skip the poll */
            lr = my_bucket->listeners;
        }
        else {
            while (!listener_may_exit) {
                apr_int32_t numdesc;
                const apr_pollfd_t *pdesc;

                rv = apr_pollset_poll(worker_pollset, -1, &numdesc, &pdesc);
                if (rv != APR_SUCCESS) {
                    if (APR_STATUS_IS_EINTR(rv)) {
                        continue;
                    }

                    /* apr_pollset_poll() will only return errors in catastrophic
                     * circumstances. Let's try exiting gracefully, for now. */
                    ap_log_error(APLOG_MARK, APLOG_ERR, rv, ap_server_conf, APLOGNO(03137)
                                 "apr_pollset_poll: (listen)");
                    signal_threads(ST_GRACEFUL);
                }

                if (listener_may_exit) break;

                /* We can always use pdesc[0], but sockets at position N
                 * could end up completely starved of attention in a very
                 * busy server. Therefore, we round-robin across the
                 * returned set of descriptors. While it is possible that
                 * the returned set of descriptors might flip around and
                 * continue to starve some sockets, we happen to know the
                 * internal pollset implementation retains ordering
                 * stability of the sockets. Thus, the round-robin should
                 * ensure that a socket will eventually be serviced.
                 */
                if (last_poll_idx >= numdesc)
                    last_poll_idx = 0;

                /* Grab a listener record from the client_data of the poll
                 * descriptor, and advance our saved index to round-robin
                 * the next fetch.
                 *
                 * ### hmm... this descriptor might have POLLERR rather
                 * ### than POLLIN
                 */
                lr = pdesc[last_poll_idx++].client_data;
                break;

            } /* while */

        } /* if/else */

        if (!listener_may_exit) {
            /* the following pops a recycled ptrans pool off a stack */
            ap_queue_info_pop_pool(worker_queue_info, &ptrans);
            if (ptrans == NULL) {
                /* we can't use a recycled transaction pool this time.
                 * create a new transaction pool */
                apr_allocator_t *allocator;

                apr_allocator_create(&allocator);
                apr_allocator_max_free_set(allocator, ap_max_mem_free);
                apr_pool_create_ex(&ptrans, pconf, NULL, allocator);
                apr_allocator_owner_set(allocator, ptrans);
                apr_pool_tag(ptrans, "transaction");
            }
            rv = lr->accept_func(&csd, lr, ptrans);
            /* later we trash rv and rely on csd to indicate success/failure */
            AP_DEBUG_ASSERT(rv == APR_SUCCESS || !csd);

            if (rv == APR_EGENERAL) {
                /* E[NM]FILE, ENOMEM, etc */
                resource_shortage = 1;
                signal_threads(ST_GRACEFUL);
            }
            if ((rv = SAFE_ACCEPT(apr_proc_mutex_unlock(my_bucket->mutex)))
                != APR_SUCCESS) {

                if (listener_may_exit) {
                    break;
                }
                accept_mutex_error("unlock", rv, process_slot);
            }
            if (csd != NULL) {
                rv = ap_queue_push_socket(worker_queue, csd, NULL, ptrans);
                if (rv) {
                    /* trash the connection; we couldn't queue the connected
                     * socket to a worker
                     */
                    apr_socket_close(csd);
                    ap_log_error(APLOG_MARK, APLOG_CRIT, rv, ap_server_conf, APLOGNO(03138)
                                 "ap_queue_push_socket failed");
                }
                else {
                    have_idle_worker = 0;
                }
            }
        }
        else {
            if ((rv = SAFE_ACCEPT(apr_proc_mutex_unlock(my_bucket->mutex)))
                != APR_SUCCESS) {
                int level = APLOG_EMERG;

                if (ap_scoreboard_image->parent[process_slot].generation !=
                    ap_scoreboard_image->global->running_generation) {
                    level = APLOG_DEBUG; /* common to get these at restart time */
                }
                ap_log_error(APLOG_MARK, level, rv, ap_server_conf, APLOGNO(00274)
                             "apr_proc_mutex_unlock failed. Attempting to "
                             "shutdown process gracefully.");
                signal_threads(ST_GRACEFUL);
            }
            break;
        }
    }

    ap_close_listeners_ex(my_bucket->listeners);
    ap_queue_info_free_idle_pools(worker_queue_info);
    ap_queue_term(worker_queue);
    dying = 1;
    ap_scoreboard_image->parent[process_slot].quiescing = 1;

    /* wake up the main thread */
    kill(ap_my_pid, SIGTERM);

    apr_thread_exit(thd, APR_SUCCESS);
    return NULL;
}

/* XXX For ungraceful termination/restart, we definitely don't want to
 *     wait for active connections to finish but we may want to wait
 *     for idle workers to get out of the queue code and release mutexes,
 *     since those mutexes are cleaned up pretty soon and some systems
 *     may not react favorably (i.e., segfault) if operations are attempted
 *     on cleaned-up mutexes.
 */
static void * APR_THREAD_FUNC worker_thread(apr_thread_t *thd, void * dummy)
{
    proc_info * ti = dummy;
    int process_slot = ti->pid;
    int thread_slot = ti->tid;
    apr_socket_t *csd = NULL;
    apr_bucket_alloc_t *bucket_alloc;
    apr_pool_t *last_ptrans = NULL;
    apr_pool_t *ptrans;                /* Pool for per-transaction stuff */
    apr_status_t rv;
    int is_idle = 0;

    free(ti);

    ap_scoreboard_image->servers[process_slot][thread_slot].pid = ap_my_pid;
    ap_scoreboard_image->servers[process_slot][thread_slot].tid = apr_os_thread_current();
    ap_scoreboard_image->servers[process_slot][thread_slot].generation = retained->mpm->my_generation;
    ap_update_child_status_from_indexes(process_slot, thread_slot,
                                        SERVER_STARTING, NULL);

#ifdef HAVE_PTHREAD_KILL
    apr_signal(WORKER_SIGNAL, dummy_signal_handler);
    unblock_signal(WORKER_SIGNAL);
#endif

    while (!workers_may_exit) {
        if (!is_idle) {
            rv = ap_queue_info_set_idle(worker_queue_info, last_ptrans);
            last_ptrans = NULL;
            if (rv != APR_SUCCESS) {
                ap_log_error(APLOG_MARK, APLOG_EMERG, rv, ap_server_conf,
                             "ap_queue_info_set_idle failed. Attempting to "
                             "shutdown process gracefully.");
                signal_threads(ST_GRACEFUL);
                break;
            }
            is_idle = 1;
        }

        ap_update_child_status_from_indexes(process_slot, thread_slot,
                                            SERVER_READY, NULL);
worker_pop:
        if (workers_may_exit) {
            break;
        }
        rv = ap_queue_pop_socket(worker_queue, &csd, &ptrans);

        if (rv != APR_SUCCESS) {
            /* We get APR_EOF during a graceful shutdown once all the connections
             * accepted by this server process have been handled.
             */
            if (APR_STATUS_IS_EOF(rv)) {
                break;
            }
            /* We get APR_EINTR whenever ap_queue_pop_*() has been interrupted
             * from an explicit call to ap_queue_interrupt_all(). This allows
             * us to unblock threads stuck in ap_queue_pop_*() when a shutdown
             * is pending.
             *
             * If workers_may_exit is set and this is ungraceful termination/
             * restart, we are bound to get an error on some systems (e.g.,
             * AIX, which sanity-checks mutex operations) since the queue
             * may have already been cleaned up.  Don't log the "error" if
             * workers_may_exit is set.
             */
            else if (APR_STATUS_IS_EINTR(rv)) {
                goto worker_pop;
            }
            /* We got some other error. */
            else if (!workers_may_exit) {
                ap_log_error(APLOG_MARK, APLOG_CRIT, rv, ap_server_conf, APLOGNO(03139)
                             "ap_queue_pop_socket failed");
            }
            continue;
        }
        is_idle = 0;
        worker_sockets[thread_slot] = csd;
        bucket_alloc = apr_bucket_alloc_create(ptrans);
        process_socket(thd, ptrans, csd, process_slot, thread_slot, bucket_alloc);
        worker_sockets[thread_slot] = NULL;
        requests_this_child--;
        apr_pool_clear(ptrans);
        last_ptrans = ptrans;
    }

    ap_update_child_status_from_indexes(process_slot, thread_slot,
                                        dying ? SERVER_DEAD
                                              : SERVER_GRACEFUL, NULL);

    apr_thread_exit(thd, APR_SUCCESS);
    return NULL;
}

static int check_signal(int signum)
{
    switch (signum) {
    case SIGTERM:
    case SIGINT:
        return 1;
    }
    return 0;
}

static void create_listener_thread(thread_starter *ts)
{
    int my_child_num = ts->child_num_arg;
    apr_threadattr_t *thread_attr = ts->threadattr;
    proc_info *my_info;
    apr_status_t rv;

    my_info = (proc_info *)ap_malloc(sizeof(proc_info));
    my_info->pid = my_child_num;
    my_info->tid = -1; /* listener thread doesn't have a thread slot */
    my_info->sd = 0;
    rv = ap_thread_create(&ts->listener, thread_attr, listener_thread,
                          my_info, pruntime);
    if (rv != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ALERT, rv, ap_server_conf, APLOGNO(00275)
                     "ap_thread_create: unable to create listener thread");
        /* let the parent decide how bad this really is */
        clean_child_exit(APEXIT_CHILDSICK);
    }
    apr_os_thread_get(&listener_os_thread, ts->listener);
}

static void setup_threads_runtime(void)
{
    ap_listen_rec *lr;
    int pollset_flags;
    apr_status_t rv;

    /* All threads (listener, workers) and synchronization objects (queues,
     * pollset, mutexes...) created here should have at least the lifetime of
     * the connections they handle (i.e. ptrans). We can't use this thread's
     * self pool because all these objects survive it, nor use pchild or pconf
     * directly because this starter thread races with other modules' runtime,
     * nor finally pchild (or subpool thereof) because it is killed explicitly
     * before pconf (thus connections/ptrans can live longer, which matters in
     * ONE_PROCESS mode). So this leaves us with a subpool of pconf, created
     * before any ptrans hence destroyed after.
     */
    apr_pool_create(&pruntime, pconf);
    apr_pool_tag(pruntime, "mpm_runtime");

    /* We must create the fd queues before we start up the listener
     * and worker threads. */
    rv = ap_queue_create(&worker_queue, threads_per_child, pruntime);
    if (rv != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ALERT, rv, ap_server_conf, APLOGNO(03140)
                     "ap_queue_create() failed");
        clean_child_exit(APEXIT_CHILDFATAL);
    }

    rv = ap_queue_info_create(&worker_queue_info, pruntime,
                              threads_per_child, -1);
    if (rv != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ALERT, rv, ap_server_conf, APLOGNO(03141)
                     "ap_queue_info_create() failed");
        clean_child_exit(APEXIT_CHILDFATAL);
    }

    /* Create the main pollset. When APR_POLLSET_WAKEABLE is asked we account
     * for the wakeup pipe explicitely with num_listensocks+1 because some
     * pollset implementations don't do it implicitely in APR.
     */
    pollset_flags = APR_POLLSET_NOCOPY | APR_POLLSET_WAKEABLE;
    rv = apr_pollset_create(&worker_pollset, num_listensocks + 1, pruntime,
                            pollset_flags);
    if (rv == APR_SUCCESS) {
        listener_is_wakeable = 1;
    }
    else {
        pollset_flags &= ~APR_POLLSET_WAKEABLE;
        rv = apr_pollset_create(&worker_pollset, num_listensocks, pruntime,
                                pollset_flags);
    }
    if (rv != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_EMERG, rv, ap_server_conf, APLOGNO(03285)
                     "Couldn't create pollset in thread;"
                     " check system or user limits");
        /* let the parent decide how bad this really is */
        clean_child_exit(APEXIT_CHILDSICK);
    }

    for (lr = my_bucket->listeners; lr != NULL; lr = lr->next) {
        apr_pollfd_t *pfd = apr_pcalloc(pruntime, sizeof *pfd);

        pfd->desc_type = APR_POLL_SOCKET;
        pfd->desc.s = lr->sd;
        pfd->reqevents = APR_POLLIN;
        pfd->client_data = lr;

        rv = apr_pollset_add(worker_pollset, pfd);
        if (rv != APR_SUCCESS) {
            ap_log_error(APLOG_MARK, APLOG_EMERG, rv, ap_server_conf, APLOGNO(03286)
                         "Couldn't create add listener to pollset;"
                         " check system or user limits");
            /* let the parent decide how bad this really is */
            clean_child_exit(APEXIT_CHILDSICK);
        }

        lr->accept_func = ap_unixd_accept;
    }

    worker_sockets = apr_pcalloc(pruntime, threads_per_child *
                                           sizeof(apr_socket_t *));
}

/* XXX under some circumstances not understood, children can get stuck
 *     in start_threads forever trying to take over slots which will
 *     never be cleaned up; for now there is an APLOG_DEBUG message issued
 *     every so often when this condition occurs
 */
static void * APR_THREAD_FUNC start_threads(apr_thread_t *thd, void *dummy)
{
    thread_starter *ts = dummy;
    apr_thread_t **threads = ts->threads;
    apr_threadattr_t *thread_attr = ts->threadattr;
    int my_child_num = ts->child_num_arg;
    proc_info *my_info;
    apr_status_t rv;
    int threads_created = 0;
    int listener_started = 0;
    int prev_threads_created;
    int loops, i;

    loops = prev_threads_created = 0;
    while (1) {
        /* threads_per_child does not include the listener thread */
        for (i = 0; i < threads_per_child; i++) {
            int status = ap_scoreboard_image->servers[my_child_num][i].status;

            if (status != SERVER_GRACEFUL && status != SERVER_DEAD) {
                continue;
            }

            my_info = (proc_info *)ap_malloc(sizeof(proc_info));
            my_info->pid = my_child_num;
            my_info->tid = i;
            my_info->sd = 0;

            /* We are creating threads right now */
            ap_update_child_status_from_indexes(my_child_num, i,
                                                SERVER_STARTING, NULL);
            /* We let each thread update its own scoreboard entry.  This is
             * done because it lets us deal with tid better.
             */
            rv = ap_thread_create(&threads[i], thread_attr,
                                  worker_thread, my_info, pruntime);
            if (rv != APR_SUCCESS) {
                ap_log_error(APLOG_MARK, APLOG_ALERT, rv, ap_server_conf, APLOGNO(03142)
                             "ap_thread_create: unable to create worker thread");
                /* let the parent decide how bad this really is */
                clean_child_exit(APEXIT_CHILDSICK);
            }
            threads_created++;
        }
        /* Start the listener only when there are workers available */
        if (!listener_started && threads_created) {
            create_listener_thread(ts);
            listener_started = 1;
        }
        if (start_thread_may_exit || threads_created == threads_per_child) {
            break;
        }
        /* wait for previous generation to clean up an entry */
        apr_sleep(apr_time_from_sec(1));
        ++loops;
        if (loops % 120 == 0) { /* every couple of minutes */
            if (prev_threads_created == threads_created) {
                ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf,
                             "child %" APR_PID_T_FMT " isn't taking over "
                             "slots very quickly (%d of %d)",
                             ap_my_pid, threads_created, threads_per_child);
            }
            prev_threads_created = threads_created;
        }
    }

    /* What state should this child_main process be listed as in the
     * scoreboard...?
     *  ap_update_child_status_from_indexes(my_child_num, i, SERVER_STARTING,
     *                                      (request_rec *) NULL);
     *
     *  This state should be listed separately in the scoreboard, in some kind
     *  of process_status, not mixed in with the worker threads' status.
     *  "life_status" is almost right, but it's in the worker's structure, and
     *  the name could be clearer.   gla
     */
    apr_thread_exit(thd, APR_SUCCESS);
    return NULL;
}

static void join_workers(apr_thread_t *listener, apr_thread_t **threads,
                         int mode)
{
    int i;
    apr_status_t rv, thread_rv;

    if (listener) {
        int iter;

        /* deal with a rare timing window which affects waking up the
         * listener thread...  if the signal sent to the listener thread
         * is delivered between the time it verifies that the
         * listener_may_exit flag is clear and the time it enters a
         * blocking syscall, the signal didn't do any good...  work around
         * that by sleeping briefly and sending it again
         */

        iter = 0;
        while (!dying) {
            apr_sleep(apr_time_from_msec(500));
            if (dying || ++iter > 10) {
                break;
            }
            /* listener has not stopped accepting yet */
            ap_log_error(APLOG_MARK, APLOG_TRACE1, 0, ap_server_conf,
                         "listener has not stopped accepting yet (%d iter)", iter);
            wakeup_listener();
        }
        if (iter > 10) {
            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf, APLOGNO(00276)
                         "the listener thread didn't exit");
        }
        else {
            rv = apr_thread_join(&thread_rv, listener);
            if (rv != APR_SUCCESS) {
                ap_log_error(APLOG_MARK, APLOG_CRIT, rv, ap_server_conf, APLOGNO(00277)
                             "apr_thread_join: unable to join listener thread");
            }
        }
    }

    for (i = 0; i < threads_per_child; i++) {
        if (threads[i]) { /* if we ever created this thread */
            if (mode != ST_GRACEFUL) {
#ifdef HAVE_PTHREAD_KILL
                apr_os_thread_t *worker_os_thread;

                apr_os_thread_get(&worker_os_thread, threads[i]);
                pthread_kill(*worker_os_thread, WORKER_SIGNAL);
#endif
            }

            rv = apr_thread_join(&thread_rv, threads[i]);
            if (rv != APR_SUCCESS) {
                ap_log_error(APLOG_MARK, APLOG_CRIT, rv, ap_server_conf, APLOGNO(00278)
                             "apr_thread_join: unable to join worker "
                             "thread %d",
                             i);
            }
        }
    }
}

static void join_start_thread(apr_thread_t *start_thread_id)
{
    apr_status_t rv, thread_rv;

    start_thread_may_exit = 1; /* tell it to give up in case it is still
                                * trying to take over slots from a
                                * previous generation
                                */
    rv = apr_thread_join(&thread_rv, start_thread_id);
    if (rv != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_CRIT, rv, ap_server_conf, APLOGNO(00279)
                     "apr_thread_join: unable to join the start "
                     "thread");
    }
}

static void child_main(int child_num_arg, int child_bucket)
{
    apr_thread_t **threads;
    apr_status_t rv;
    thread_starter *ts;
    apr_threadattr_t *thread_attr;
    apr_thread_t *start_thread_id;
    int i;

    /* for benefit of any hooks that run as this child initializes */
    retained->mpm->mpm_state = AP_MPMQ_STARTING;

    ap_my_pid = getpid();
    ap_fatal_signal_child_setup(ap_server_conf);

    /* Get a sub context for global allocations in this child, so that
     * we can have cleanups occur when the child exits.
     */
    apr_pool_create(&pchild, pconf);
    apr_pool_tag(pchild, "pchild");

#if AP_HAS_THREAD_LOCAL
    if (!one_process) {
        apr_thread_t *thd = NULL;
        if ((rv = ap_thread_main_create(&thd, pchild))) {
            ap_log_error(APLOG_MARK, APLOG_EMERG, rv, ap_server_conf, APLOGNO(10375)
                         "Couldn't initialize child main thread");
            clean_child_exit(APEXIT_CHILDFATAL);
        }
    }
#endif

    /* close unused listeners and pods */
    for (i = 0; i < retained->mpm->num_buckets; i++) {
        if (i != child_bucket) {
            ap_close_listeners_ex(all_buckets[i].listeners);
            ap_mpm_podx_close(all_buckets[i].pod);
        }
    }

    /*stuff to do before we switch id's, so we have permissions.*/
    ap_reopen_scoreboard(pchild, NULL, 0);

    rv = SAFE_ACCEPT(apr_proc_mutex_child_init(&my_bucket->mutex,
                                    apr_proc_mutex_lockfile(my_bucket->mutex),
                                    pchild));
    if (rv != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_EMERG, rv, ap_server_conf, APLOGNO(00280)
                     "Couldn't initialize cross-process lock in child");
        clean_child_exit(APEXIT_CHILDFATAL);
    }

    /* done with init critical section */
    if (ap_run_drop_privileges(pchild, ap_server_conf)) {
        clean_child_exit(APEXIT_CHILDFATAL);
    }

    /* Just use the standard apr_setup_signal_thread to block all signals
     * from being received.  The child processes no longer use signals for
     * any communication with the parent process. Let's also do this before
     * child_init() hooks are called and possibly create threads that
     * otherwise could "steal" (implicitly) MPM's signals.
     */
    rv = apr_setup_signal_thread();
    if (rv != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_EMERG, rv, ap_server_conf, APLOGNO(00281)
                     "Couldn't initialize signal thread");
        clean_child_exit(APEXIT_CHILDFATAL);
    }

    ap_run_child_init(pchild, ap_server_conf);

    if (ap_max_requests_per_child) {
        requests_this_child = ap_max_requests_per_child;
    }
    else {
        /* coding a value of zero means infinity */
        requests_this_child = INT_MAX;
    }

    /* Setup threads */

    /* Globals used by signal_threads() so to be initialized before */
    setup_threads_runtime();

    /* clear the storage; we may not create all our threads immediately,
     * and we want a 0 entry to indicate a thread which was not created
     */
    threads = (apr_thread_t **)ap_calloc(1,
                                  sizeof(apr_thread_t *) * threads_per_child);
    ts = (thread_starter *)apr_palloc(pchild, sizeof(*ts));

    apr_threadattr_create(&thread_attr, pchild);
    /* 0 means PTHREAD_CREATE_JOINABLE */
    apr_threadattr_detach_set(thread_attr, 0);

    if (ap_thread_stacksize != 0) {
        rv = apr_threadattr_stacksize_set(thread_attr, ap_thread_stacksize);
        if (rv != APR_SUCCESS && rv != APR_ENOTIMPL) {
            ap_log_error(APLOG_MARK, APLOG_WARNING, rv, ap_server_conf, APLOGNO(02435)
                         "WARNING: ThreadStackSize of %" APR_SIZE_T_FMT " is "
                         "inappropriate, using default", 
                         ap_thread_stacksize);
        }
    }

    ts->threads = threads;
    ts->listener = NULL;
    ts->child_num_arg = child_num_arg;
    ts->threadattr = thread_attr;

    rv = ap_thread_create(&start_thread_id, thread_attr, start_threads,
                          ts, pchild);
    if (rv != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ALERT, rv, ap_server_conf, APLOGNO(00282)
                     "ap_thread_create: unable to create worker thread");
        /* let the parent decide how bad this really is */
        clean_child_exit(APEXIT_CHILDSICK);
    }

    retained->mpm->mpm_state = AP_MPMQ_RUNNING;

    /* If we are only running in one_process mode, we will want to
     * still handle signals. */
    if (one_process) {
        /* Block until we get a terminating signal. */
        apr_signal_thread(check_signal);
        /* make sure the start thread has finished; signal_threads()
         * and join_workers() depend on that
         */
        /* XXX join_start_thread() won't be awakened if one of our
         *     threads encounters a critical error and attempts to
         *     shutdown this child
         */
        join_start_thread(start_thread_id);
        signal_threads(ST_UNGRACEFUL); /* helps us terminate a little more
                           * quickly than the dispatch of the signal thread
                           * beats the Pipe of Death and the browsers
                           */
        /* A terminating signal was received. Now join each of the
         * workers to clean them up.
         *   If the worker already exited, then the join frees
         *   their resources and returns.
         *   If the worker hasn't exited, then this blocks until
         *   they have (then cleans up).
         */
        join_workers(ts->listener, threads, ST_UNGRACEFUL);
    }
    else { /* !one_process */
        /* remove SIGTERM from the set of blocked signals...  if one of
         * the other threads in the process needs to take us down
         * (e.g., for MaxConnectionsPerChild) it will send us SIGTERM
         */
        apr_signal(SIGTERM, dummy_signal_handler);
        unblock_signal(SIGTERM);
        /* Watch for any messages from the parent over the POD */
        while (1) {
            rv = ap_mpm_podx_check(my_bucket->pod);
            if (rv == AP_MPM_PODX_NORESTART) {
                /* see if termination was triggered while we slept */
                switch(terminate_mode) {
                case ST_GRACEFUL:
                    rv = AP_MPM_PODX_GRACEFUL;
                    break;
                case ST_UNGRACEFUL:
                    rv = AP_MPM_PODX_RESTART;
                    break;
                }
            }
            if (rv == AP_MPM_PODX_GRACEFUL || rv == AP_MPM_PODX_RESTART) {
                /* make sure the start thread has finished;
                 * signal_threads() and join_workers depend on that
                 */
                join_start_thread(start_thread_id);
                signal_threads(rv == AP_MPM_PODX_GRACEFUL ? ST_GRACEFUL : ST_UNGRACEFUL);
                break;
            }
        }

        /* A terminating signal was received. Now join each of the
         * workers to clean them up.
         *   If the worker already exited, then the join frees
         *   their resources and returns.
         *   If the worker hasn't exited, then this blocks until
         *   they have (then cleans up).
         */
        join_workers(ts->listener, threads,
                     rv == AP_MPM_PODX_GRACEFUL ? ST_GRACEFUL : ST_UNGRACEFUL);
    }

    free(threads);

    clean_child_exit(resource_shortage ? APEXIT_CHILDSICK : 0);
}

static int make_child(server_rec *s, int slot, int bucket)
{
    int pid;

    if (slot + 1 > retained->max_daemons_limit) {
        retained->max_daemons_limit = slot + 1;
    }

    if (one_process) {
        my_bucket = &all_buckets[0];

        worker_note_child_started(slot, getpid());
        child_main(slot, 0);
        /* NOTREACHED */
        ap_assert(0);
        return -1;
    }

    if ((pid = fork()) == -1) {
        ap_log_error(APLOG_MARK, APLOG_ERR, errno, s, APLOGNO(00283)
                     "fork: Unable to fork new process");
        /* fork didn't succeed.  There's no need to touch the scoreboard;
         * if we were trying to replace a failed child process, then
         * server_main_loop() marked its workers SERVER_DEAD, and if
         * we were trying to replace a child process that exited normally,
         * its worker_thread()s left SERVER_DEAD or SERVER_GRACEFUL behind.
         */

        /* In case system resources are maxxed out, we don't want
           Apache running away with the CPU trying to fork over and
           over and over again. */
        apr_sleep(apr_time_from_sec(10));

        return -1;
    }

    if (!pid) {
#if AP_HAS_THREAD_LOCAL
        ap_thread_current_after_fork();
#endif

        my_bucket = &all_buckets[bucket];

#ifdef HAVE_BINDPROCESSOR
        /* By default, AIX binds to a single processor.  This bit unbinds
         * children which will then bind to another CPU.
         */
        int status = bindprocessor(BINDPROCESS, (int)getpid(),
                               PROCESSOR_CLASS_ANY);
        if (status != OK)
            ap_log_error(APLOG_MARK, APLOG_DEBUG, errno,
                         ap_server_conf, APLOGNO(00284)
                         "processor unbind failed");
#endif
        RAISE_SIGSTOP(MAKE_CHILD);

        apr_signal(SIGTERM, just_die);
        child_main(slot, bucket);
        /* NOTREACHED */
        ap_assert(0);
        return -1;
    }

    if (ap_scoreboard_image->parent[slot].pid != 0) {
        /* This new child process is squatting on the scoreboard
         * entry owned by an exiting child process, which cannot
         * exit until all active requests complete.
         */
        worker_note_child_lost_slot(slot, pid);
    }
    ap_scoreboard_image->parent[slot].quiescing = 0;
    worker_note_child_started(slot, pid);
    return 0;
}

/* start up a bunch of children */
static void startup_children(int number_to_start)
{
    int i;

    for (i = 0; number_to_start && i < ap_daemons_limit; ++i) {
        if (ap_scoreboard_image->parent[i].pid != 0) {
            continue;
        }
        if (make_child(ap_server_conf, i, i % retained->mpm->num_buckets) < 0) {
            break;
        }
        --number_to_start;
    }
}

static void perform_idle_server_maintenance(int child_bucket)
{
    int num_buckets = retained->mpm->num_buckets;
    int idle_thread_count;
    process_score *ps;
    int free_length;
    int totally_free_length = 0;
    int free_slots[MAX_SPAWN_RATE];
    int last_non_dead;
    int total_non_dead;
    int active_thread_count = 0;
    int i, j;

    /* initialize the free_list */
    free_length = 0;

    idle_thread_count = 0;
    last_non_dead = -1;
    total_non_dead = 0;

    for (i = 0; i < ap_daemons_limit; ++i) {
        /* Initialization to satisfy the compiler. It doesn't know
         * that threads_per_child is always > 0 */
        int any_dying_threads = 0;
        int any_dead_threads = 0;
        int all_dead_threads = 1;
        int child_threads_active = 0;

        if (num_buckets > 1 && (i % num_buckets) != child_bucket) {
            /* We only care about child_bucket in this call */
            continue;
        }
        if (i >= retained->max_daemons_limit &&
            totally_free_length == retained->idle_spawn_rate[child_bucket]) {
            /* short cut if all active processes have been examined and
             * enough empty scoreboard slots have been found
             */
            break;
        }
        ps = &ap_scoreboard_image->parent[i];
        for (j = 0; j < threads_per_child; j++) {
            int status = ap_scoreboard_image->servers[i][j].status;

            /* XXX any_dying_threads is probably no longer needed    GLA */
            any_dying_threads = any_dying_threads ||
                                (status == SERVER_GRACEFUL);
            any_dead_threads = any_dead_threads || (status == SERVER_DEAD);
            all_dead_threads = all_dead_threads &&
                                   (status == SERVER_DEAD ||
                                    status == SERVER_GRACEFUL);

            /* We consider a starting server as idle because we started it
             * at least a cycle ago, and if it still hasn't finished starting
             * then we're just going to swamp things worse by forking more.
             * So we hopefully won't need to fork more if we count it.
             * This depends on the ordering of SERVER_READY and SERVER_STARTING.
             */
            if (ps->pid != 0) { /* XXX just set all_dead_threads in outer for
                                   loop if no pid?  not much else matters */
                if (status <= SERVER_READY &&
                        !ps->quiescing &&
                        ps->generation == retained->mpm->my_generation) {
                    ++idle_thread_count;
                }
                if (status >= SERVER_READY && status < SERVER_GRACEFUL) {
                    ++child_threads_active;
                }
            }
        }
        active_thread_count += child_threads_active;
        if (any_dead_threads
                && totally_free_length < retained->idle_spawn_rate[child_bucket]
                && free_length < MAX_SPAWN_RATE / num_buckets
                && (!ps->pid               /* no process in the slot */
                    || ps->quiescing)) {   /* or at least one is going away */
            if (all_dead_threads) {
                /* great! we prefer these, because the new process can
                 * start more threads sooner.  So prioritize this slot
                 * by putting it ahead of any slots with active threads.
                 *
                 * first, make room by moving a slot that's potentially still
                 * in use to the end of the array
                 */
                free_slots[free_length] = free_slots[totally_free_length];
                free_slots[totally_free_length++] = i;
            }
            else {
                /* slot is still in use - back of the bus
                 */
                free_slots[free_length] = i;
            }
            ++free_length;
        }
        else if (child_threads_active == threads_per_child) {
            had_healthy_child = 1;
        }
        /* XXX if (!ps->quiescing)     is probably more reliable  GLA */
        if (!any_dying_threads) {
            ++total_non_dead;
        }
        if (ps->pid != 0) {
            last_non_dead = i;
        }
    }

    retained->max_daemons_limit = last_non_dead + 1;

    if (retained->sick_child_detected) {
        if (had_healthy_child) {
            /* Assume this is a transient error, even though it may not be.  Leave
             * the server up in case it is able to serve some requests or the
             * problem will be resolved.
             */
            retained->sick_child_detected = 0;
        }
        else if (child_bucket < num_buckets - 1) {
            /* check for had_healthy_child up to the last child bucket */
            return;
        }
        else {
            /* looks like a basket case, as no child ever fully initialized; give up.
             */
            retained->mpm->shutdown_pending = 1;
            child_fatal = 1;
            ap_log_error(APLOG_MARK, APLOG_ALERT, 0,
                         ap_server_conf, APLOGNO(02325)
                         "A resource shortage or other unrecoverable failure "
                         "was encountered before any child process initialized "
                         "successfully... httpd is exiting!");
            /* the child already logged the failure details */
            return;
        }
    }

    if (idle_thread_count > max_spare_threads / num_buckets) {
        /* Kill off one child */
        ap_mpm_podx_signal(all_buckets[child_bucket].pod,
                           AP_MPM_PODX_GRACEFUL);
        retained->idle_spawn_rate[child_bucket] = 1;
    }
    else if (idle_thread_count < min_spare_threads / num_buckets) {
        /* terminate the free list */
        if (free_length == 0) { /* scoreboard is full, can't fork */

            if (active_thread_count >= max_workers / num_buckets) {
                /* no threads are "inactive" - starting, stopping, etc. */
                /* have we reached MaxRequestWorkers, or just getting close? */
                if (0 == idle_thread_count) {
                    if (!retained->maxclients_reported) {
                        /* only report this condition once */
                        ap_log_error(APLOG_MARK, APLOG_ERR, 0, ap_server_conf, APLOGNO(00286)
                                     "server reached MaxRequestWorkers "
                                     "setting, consider raising the "
                                     "MaxRequestWorkers setting");
                        retained->maxclients_reported = 1;
                    }
                } else {
                    if (!retained->near_maxclients_reported) {
                        ap_log_error(APLOG_MARK, APLOG_ERR, 0, ap_server_conf, APLOGNO(00287)
                                     "server is within MinSpareThreads of "
                                     "MaxRequestWorkers, consider raising the "
                                     "MaxRequestWorkers setting");
                        retained->near_maxclients_reported = 1;
                    }
                }
            }
            else {
                ap_log_error(APLOG_MARK, APLOG_ERR, 0,
                             ap_server_conf, APLOGNO(00288)
                             "scoreboard is full, not at MaxRequestWorkers");
            }
            retained->idle_spawn_rate[child_bucket] = 1;
        }
        else {
            if (free_length > retained->idle_spawn_rate[child_bucket]) {
                free_length = retained->idle_spawn_rate[child_bucket];
            }
            if (retained->idle_spawn_rate[child_bucket] >= 8) {
                ap_log_error(APLOG_MARK, APLOG_INFO, 0,
                             ap_server_conf, APLOGNO(00289)
                             "server seems busy, (you may need "
                             "to increase StartServers, ThreadsPerChild "
                             "or Min/MaxSpareThreads), "
                             "spawning %d children, there are around %d idle "
                             "threads, and %d total children", free_length,
                             idle_thread_count, total_non_dead);
            }
            for (i = 0; i < free_length; ++i) {
                make_child(ap_server_conf, free_slots[i], child_bucket);
            }
            /* the next time around we want to spawn twice as many if this
             * wasn't good enough, but not if we've just done a graceful
             */
            if (retained->hold_off_on_exponential_spawning) {
                --retained->hold_off_on_exponential_spawning;
            }
            else if (retained->idle_spawn_rate[child_bucket]
                     < MAX_SPAWN_RATE / num_buckets) {
                retained->idle_spawn_rate[child_bucket] *= 2;
            }
        }
    }
    else {
        retained->idle_spawn_rate[child_bucket] = 1;
    }
}

static void server_main_loop(int remaining_children_to_start)
{
    int num_buckets = retained->mpm->num_buckets;
    int successive_kills = 0;
    ap_generation_t old_gen;
    int child_slot;
    apr_exit_why_e exitwhy;
    int status, processed_status;
    apr_proc_t pid;
    int i;

    while (!retained->mpm->restart_pending && !retained->mpm->shutdown_pending) {
        ap_wait_or_timeout(&exitwhy, &status, &pid, pconf, ap_server_conf);

        if (pid.pid != -1) {
            processed_status = ap_process_child_status(&pid, exitwhy, status);
            child_slot = ap_find_child_by_pid(&pid);
            if (processed_status == APEXIT_CHILDFATAL) {
                /* fix race condition found in PR 39311
                 * A child created at the same time as a graceful happens 
                 * can find the lock missing and create a fatal error.
                 * It is not fatal for the last generation to be in this state.
                 */
                if (child_slot < 0
                    || ap_get_scoreboard_process(child_slot)->generation
                       == retained->mpm->my_generation) {
                    retained->mpm->shutdown_pending = 1;
                    child_fatal = 1;
                    return;
                }
                else {
                    ap_log_error(APLOG_MARK, APLOG_WARNING, 0, ap_server_conf, APLOGNO(00290)
                                 "Ignoring fatal error in child of previous "
                                 "generation (pid %ld).",
                                 (long)pid.pid);
                    retained->sick_child_detected = 1;
                }
            }
            else if (processed_status == APEXIT_CHILDSICK) {
                /* tell perform_idle_server_maintenance to check into this
                 * on the next timer pop
                 */
                retained->sick_child_detected = 1;
            }
            /* non-fatal death... note that it's gone in the scoreboard. */
            if (child_slot >= 0) {
                process_score *ps;

                for (i = 0; i < threads_per_child; i++)
                    ap_update_child_status_from_indexes(child_slot, i,
                                                        SERVER_DEAD, NULL);

                worker_note_child_killed(child_slot, 0, 0);
                ps = &ap_scoreboard_image->parent[child_slot];
                ps->quiescing = 0;
                if (processed_status == APEXIT_CHILDSICK) {
                    /* resource shortage, minimize the fork rate */
                    retained->idle_spawn_rate[child_slot % num_buckets] = 1;
                }
                else if (remaining_children_to_start
                    && child_slot < ap_daemons_limit) {
                    /* we're still doing a 1-for-1 replacement of dead
                     * children with new children
                     */
                    make_child(ap_server_conf, child_slot,
                               child_slot % num_buckets);
                    --remaining_children_to_start;
                }
            }
            else if (ap_unregister_extra_mpm_process(pid.pid, &old_gen) == 1) {
                worker_note_child_killed(-1, /* already out of the scoreboard */
                                         pid.pid, old_gen);
                if (processed_status == APEXIT_CHILDSICK
                    && old_gen == retained->mpm->my_generation) {
                    /* resource shortage, minimize the fork rate */
                    for (i = 0; i < num_buckets; i++) {
                        retained->idle_spawn_rate[i] = 1;
                    }
                }
#if APR_HAS_OTHER_CHILD
            }
            else if (apr_proc_other_child_alert(&pid, APR_OC_REASON_DEATH,
                                                status) == 0) {
                /* handled */
#endif
            }
            else if (retained->mpm->was_graceful) {
                /* Great, we've probably just lost a slot in the
                 * scoreboard.  Somehow we don't know about this child.
                 */
                ap_log_error(APLOG_MARK, APLOG_WARNING, 0,
                             ap_server_conf, APLOGNO(00291)
                             "long lost child came home! (pid %ld)",
                             (long)pid.pid);
            }
            /* Don't perform idle maintenance when a child dies,
             * only do it when there's a timeout.  Remember only a
             * finite number of children can die, and it's pretty
             * pathological for a lot to die suddenly.  If a child is
             * killed by a signal (faulting) we want to restart it ASAP
             * though, up to 3 successive faults or we stop this until
             * a timeout happens again (to avoid the flood of fork()ed
             * processes that keep being killed early).
             */
            if (child_slot < 0 || !APR_PROC_CHECK_SIGNALED(exitwhy)) {
                continue;
            }
            if (++successive_kills >= 3) {
                if (successive_kills % 10 == 3) {
                    ap_log_error(APLOG_MARK, APLOG_WARNING, 0,
                                 ap_server_conf, APLOGNO(10393)
                                 "children are killed successively!");
                }
                continue;
            }
            ++remaining_children_to_start;
        }
        else {
            successive_kills = 0;
        }

        if (remaining_children_to_start) {
            /* we hit a 1 second timeout in which none of the previous
             * generation of children needed to be reaped... so assume
             * they're all done, and pick up the slack if any is left.
             */
            startup_children(remaining_children_to_start);
            remaining_children_to_start = 0;
            /* In any event we really shouldn't do the code below because
             * few of the servers we just started are in the IDLE state
             * yet, so we'd mistakenly create an extra server.
             */
            continue;
        }

        for (i = 0; i < num_buckets; i++) {
            perform_idle_server_maintenance(i);
        }
    }
}

static int worker_run(apr_pool_t *_pconf, apr_pool_t *plog, server_rec *s)
{
    int num_buckets = retained->mpm->num_buckets;
    int remaining_children_to_start;
    int i;

    ap_log_pid(pconf, ap_pid_fname);

    if (!retained->mpm->was_graceful) {
        if (ap_run_pre_mpm(s->process->pool, SB_SHARED) != OK) {
            retained->mpm->mpm_state = AP_MPMQ_STOPPING;
            return !OK;
        }
        /* fix the generation number in the global score; we just got a new,
         * cleared scoreboard
         */
        ap_scoreboard_image->global->running_generation = retained->mpm->my_generation;
    }

    ap_unixd_mpm_set_signals(pconf, one_process);

    /* Don't thrash since num_buckets depends on the
     * system and the number of online CPU cores...
     */
    if (ap_daemons_limit < num_buckets)
        ap_daemons_limit = num_buckets;
    if (ap_daemons_to_start < num_buckets)
        ap_daemons_to_start = num_buckets;
    /* We want to create as much children at a time as the number of buckets,
     * so to optimally accept connections (evenly distributed across buckets).
     * Thus min_spare_threads should at least maintain num_buckets children,
     * and max_spare_threads allow num_buckets more children w/o triggering
     * immediately (e.g. num_buckets idle threads margin, one per bucket).
     */
    if (min_spare_threads < threads_per_child * (num_buckets - 1) + num_buckets)
        min_spare_threads = threads_per_child * (num_buckets - 1) + num_buckets;
    if (max_spare_threads < min_spare_threads + (threads_per_child + 1) * num_buckets)
        max_spare_threads = min_spare_threads + (threads_per_child + 1) * num_buckets;

    /* If we're doing a graceful_restart then we're going to see a lot
     * of children exiting immediately when we get into the main loop
     * below (because we just sent them AP_SIG_GRACEFUL).  This happens pretty
     * rapidly... and for each one that exits we may start a new one, until
     * there are at least min_spare_threads idle threads, counting across
     * all children.  But we may be permitted to start more children than
     * that, so we'll just keep track of how many we're
     * supposed to start up without the 1 second penalty between each fork.
     */
    remaining_children_to_start = ap_daemons_to_start;
    if (remaining_children_to_start > ap_daemons_limit) {
        remaining_children_to_start = ap_daemons_limit;
    }
    if (!retained->mpm->was_graceful) {
        startup_children(remaining_children_to_start);
        remaining_children_to_start = 0;
    }
    else {
        /* give the system some time to recover before kicking into
            * exponential mode */
        retained->hold_off_on_exponential_spawning = 10;
    }

    ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, ap_server_conf, APLOGNO(00292)
                "%s configured -- resuming normal operations",
                ap_get_server_description());
    ap_log_error(APLOG_MARK, APLOG_INFO, 0, ap_server_conf, APLOGNO(00293)
                "Server built: %s", ap_get_server_built());
    ap_log_command_line(plog, s);
    ap_log_mpm_common(s);
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, ap_server_conf, APLOGNO(00294)
                "Accept mutex: %s (default: %s)",
                (all_buckets[0].mutex)
                    ? apr_proc_mutex_name(all_buckets[0].mutex)
                    : "none",
                apr_proc_mutex_defname());
    retained->mpm->mpm_state = AP_MPMQ_RUNNING;

    server_main_loop(remaining_children_to_start);
    retained->mpm->mpm_state = AP_MPMQ_STOPPING;

    if (retained->mpm->shutdown_pending && retained->mpm->is_ungraceful) {
        /* Time to shut down:
         * Kill child processes, tell them to call child_exit, etc...
         */
        for (i = 0; i < num_buckets; i++) {
            ap_mpm_podx_killpg(all_buckets[i].pod, ap_daemons_limit,
                               AP_MPM_PODX_RESTART);
        }
        ap_reclaim_child_processes(1, /* Start with SIGTERM */
                                   worker_note_child_killed);

        if (!child_fatal) {
            /* cleanup pid file on normal shutdown */
            ap_remove_pid(pconf, ap_pid_fname);
            ap_log_error(APLOG_MARK, APLOG_NOTICE, 0,
                         ap_server_conf, APLOGNO(00295) "caught SIGTERM, shutting down");
        }
        return DONE;
    }

    if (retained->mpm->shutdown_pending) {
        /* Time to gracefully shut down:
         * Kill child processes, tell them to call child_exit, etc...
         */
        int active_children;
        int index;
        apr_time_t cutoff = 0;

        /* Close our listeners, and then ask our children to do same */
        ap_close_listeners();

        for (i = 0; i < num_buckets; i++) {
            ap_mpm_podx_killpg(all_buckets[i].pod, ap_daemons_limit,
                               AP_MPM_PODX_GRACEFUL);
        }
        ap_relieve_child_processes(worker_note_child_killed);

        if (!child_fatal) {
            /* cleanup pid file on normal shutdown */
            ap_remove_pid(pconf, ap_pid_fname);
            ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, ap_server_conf, APLOGNO(00296)
                         "caught " AP_SIG_GRACEFUL_STOP_STRING
                         ", shutting down gracefully");
        }

        if (ap_graceful_shutdown_timeout) {
            cutoff = apr_time_now() +
                     apr_time_from_sec(ap_graceful_shutdown_timeout);
        }

        /* Don't really exit until each child has finished */
        retained->mpm->shutdown_pending = 0;
        do {
            /* Pause for a second */
            apr_sleep(apr_time_from_sec(1));

            /* Relieve any children which have now exited */
            ap_relieve_child_processes(worker_note_child_killed);

            active_children = 0;
            for (index = 0; index < ap_daemons_limit; ++index) {
                if (ap_mpm_safe_kill(MPM_CHILD_PID(index), 0) == APR_SUCCESS) {
                    active_children = 1;
                    /* Having just one child is enough to stay around */
                    break;
                }
            }
        } while (!retained->mpm->shutdown_pending && active_children &&
                 (!ap_graceful_shutdown_timeout || apr_time_now() < cutoff));

        /* We might be here because we received SIGTERM, either
         * way, try and make sure that all of our processes are
         * really dead.
         */
        for (i = 0; i < num_buckets; i++) {
            ap_mpm_podx_killpg(all_buckets[i].pod, ap_daemons_limit,
                               AP_MPM_PODX_RESTART);
        }
        ap_reclaim_child_processes(1, worker_note_child_killed);

        return DONE;
    }

    /* we've been told to restart */
    if (one_process) {
        /* not worth thinking about */
        return DONE;
    }

    /* advance to the next generation */
    /* XXX: we really need to make sure this new generation number isn't in
     * use by any of the children.
     */
    ++retained->mpm->my_generation;
    ap_scoreboard_image->global->running_generation = retained->mpm->my_generation;

    if (!retained->mpm->is_ungraceful) {
        ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, ap_server_conf, APLOGNO(00297)
                     AP_SIG_GRACEFUL_STRING " received.  Doing graceful restart");
        /* wake up the children...time to die.  But we'll have more soon */
        for (i = 0; i < num_buckets; i++) {
            ap_mpm_podx_killpg(all_buckets[i].pod, ap_daemons_limit,
                               AP_MPM_PODX_GRACEFUL);
        }

        /* This is mostly for debugging... so that we know what is still
         * gracefully dealing with existing request.
         */

    }
    else {
        /* Kill 'em all.  Since the child acts the same on the parents SIGTERM
         * and a SIGHUP, we may as well use the same signal, because some user
         * pthreads are stealing signals from us left and right.
         */
        for (i = 0; i < num_buckets; i++) {
            ap_mpm_podx_killpg(all_buckets[i].pod, ap_daemons_limit,
                               AP_MPM_PODX_RESTART);
        }

        ap_reclaim_child_processes(1, /* Start with SIGTERM */
                                   worker_note_child_killed);
        ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, ap_server_conf, APLOGNO(00298)
                    "SIGHUP received.  Attempting to restart");
    }

    return OK;
}

/* This really should be a post_config hook, but the error log is already
 * redirected by that point, so we need to do this in the open_logs phase.
 */
static int worker_open_logs(apr_pool_t *p, apr_pool_t *plog, apr_pool_t *ptemp, server_rec *s)
{
    int startup = 0;
    int level_flags = 0;
    int num_buckets = 0;
    ap_listen_rec **listen_buckets;
    apr_status_t rv;
    char id[16];
    int i;

    pconf = p;

    /* the reverse of pre_config, we want this only the first time around */
    if (retained->mpm->module_loads == 1) {
        startup = 1;
        level_flags |= APLOG_STARTUP;
    }

    if ((num_listensocks = ap_setup_listeners(ap_server_conf)) < 1) {
        ap_log_error(APLOG_MARK, APLOG_ALERT | level_flags, 0,
                     (startup ? NULL : s),
                     "no listening sockets available, shutting down");
        return !OK;
    }

    if (one_process) {
        num_buckets = 1;
    }
    else if (retained->mpm->was_graceful) {
        /* Preserve the number of buckets on graceful restarts. */
        num_buckets = retained->mpm->num_buckets;
    }
    if ((rv = ap_duplicate_listeners(pconf, ap_server_conf,
                                     &listen_buckets, &num_buckets))) {
        ap_log_error(APLOG_MARK, APLOG_CRIT | level_flags, rv,
                     (startup ? NULL : s),
                     "could not duplicate listeners");
        return !OK;
    }

    all_buckets = apr_pcalloc(pconf, num_buckets * sizeof(*all_buckets));
    for (i = 0; i < num_buckets; i++) {
        if (!one_process && /* no POD in one_process mode */
                (rv = ap_mpm_podx_open(pconf, &all_buckets[i].pod))) {
            ap_log_error(APLOG_MARK, APLOG_CRIT | level_flags, rv,
                         (startup ? NULL : s),
                         "could not open pipe-of-death");
            return !OK;
        }
        /* Initialize cross-process accept lock (safe accept needed only) */
        if ((rv = SAFE_ACCEPT((apr_snprintf(id, sizeof id, "%i", i),
                               ap_proc_mutex_create(&all_buckets[i].mutex,
                                                    NULL, AP_ACCEPT_MUTEX_TYPE,
                                                    id, s, pconf, 0))))) {
            ap_log_error(APLOG_MARK, APLOG_CRIT | level_flags, rv,
                         (startup ? NULL : s),
                         "could not create accept mutex");
            return !OK;
        }
        all_buckets[i].listeners = listen_buckets[i];
    }

    if (retained->mpm->max_buckets < num_buckets) {
        int new_max, *new_ptr;
        new_max = retained->mpm->max_buckets * 2;
        if (new_max < num_buckets) {
            new_max = num_buckets;
        }
        new_ptr = (int *)apr_palloc(ap_pglobal, new_max * sizeof(int));
        if (retained->idle_spawn_rate) /* NULL at startup */
            memcpy(new_ptr, retained->idle_spawn_rate,
                   retained->mpm->num_buckets * sizeof(int));
        retained->idle_spawn_rate = new_ptr;
        retained->mpm->max_buckets = new_max;
    }
    if (retained->mpm->num_buckets < num_buckets) {
        int rate_max = 1;
        /* If new buckets are added, set their idle spawn rate to
         * the highest so far, so that they get filled as quickly
         * as the existing ones.
         */
        for (i = 0; i < retained->mpm->num_buckets; i++) {
            if (rate_max < retained->idle_spawn_rate[i]) {
                rate_max = retained->idle_spawn_rate[i];
            }
        }
        for (/* up to date i */; i < num_buckets; i++) {
            retained->idle_spawn_rate[i] = rate_max;
        }
    }
    retained->mpm->num_buckets = num_buckets;

    return OK;
}

static int worker_pre_config(apr_pool_t *pconf, apr_pool_t *plog,
                             apr_pool_t *ptemp)
{
    int no_detach, debug, foreground;
    apr_status_t rv;
    const char *userdata_key = "mpm_worker_module";

    debug = ap_exists_config_define("DEBUG");

    if (debug) {
        foreground = one_process = 1;
        no_detach = 0;
    }
    else {
        one_process = ap_exists_config_define("ONE_PROCESS");
        no_detach = ap_exists_config_define("NO_DETACH");
        foreground = ap_exists_config_define("FOREGROUND");
    }

    ap_mutex_register(pconf, AP_ACCEPT_MUTEX_TYPE, NULL, APR_LOCK_DEFAULT, 0);

    retained = ap_retained_data_get(userdata_key);
    if (!retained) {
        retained = ap_retained_data_create(userdata_key, sizeof(*retained));
        retained->mpm = ap_unixd_mpm_get_retained_data();
    }
    retained->mpm->mpm_state = AP_MPMQ_STARTING;
    if (retained->mpm->baton != retained) {
        retained->mpm->was_graceful = 0;
        retained->mpm->baton = retained;
    }
    ++retained->mpm->module_loads;

    /* sigh, want this only the second time around */
    if (retained->mpm->module_loads == 2) {
        if (!one_process && !foreground) {
            /* before we detach, setup crash handlers to log to errorlog */
            ap_fatal_signal_setup(ap_server_conf, pconf);
            rv = apr_proc_detach(no_detach ? APR_PROC_DETACH_FOREGROUND
                                           : APR_PROC_DETACH_DAEMONIZE);
            if (rv != APR_SUCCESS) {
                ap_log_error(APLOG_MARK, APLOG_CRIT, rv, NULL, APLOGNO(00299)
                             "apr_proc_detach failed");
                return HTTP_INTERNAL_SERVER_ERROR;
            }
        }
    }

    parent_pid = ap_my_pid = getpid();

    ap_listen_pre_config();
    ap_daemons_to_start = DEFAULT_START_DAEMON;
    min_spare_threads = DEFAULT_MIN_FREE_DAEMON * DEFAULT_THREADS_PER_CHILD;
    max_spare_threads = DEFAULT_MAX_FREE_DAEMON * DEFAULT_THREADS_PER_CHILD;
    server_limit = DEFAULT_SERVER_LIMIT;
    thread_limit = DEFAULT_THREAD_LIMIT;
    ap_daemons_limit = server_limit;
    threads_per_child = DEFAULT_THREADS_PER_CHILD;
    max_workers = ap_daemons_limit * threads_per_child;
    had_healthy_child = 0;
    ap_extended_status = 0;

    return OK;
}

static int worker_check_config(apr_pool_t *p, apr_pool_t *plog,
                               apr_pool_t *ptemp, server_rec *s)
{
    int startup = 0;

    /* the reverse of pre_config, we want this only the first time around */
    if (retained->mpm->module_loads == 1) {
        startup = 1;
    }

    if (server_limit > MAX_SERVER_LIMIT) {
        if (startup) {
            ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_STARTUP, 0, NULL, APLOGNO(00300)
                         "WARNING: ServerLimit of %d exceeds compile-time "
                         "limit of %d servers, decreasing to %d.",
                         server_limit, MAX_SERVER_LIMIT, MAX_SERVER_LIMIT);
        } else {
            ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00301)
                         "ServerLimit of %d exceeds compile-time limit "
                         "of %d, decreasing to match",
                         server_limit, MAX_SERVER_LIMIT);
        }
        server_limit = MAX_SERVER_LIMIT;
    }
    else if (server_limit < 1) {
        if (startup) {
            ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_STARTUP, 0, NULL, APLOGNO(00302)
                         "WARNING: ServerLimit of %d not allowed, "
                         "increasing to 1.", server_limit);
        } else {
            ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00303)
                         "ServerLimit of %d not allowed, increasing to 1",
                         server_limit);
        }
        server_limit = 1;
    }

    /* you cannot change ServerLimit across a restart; ignore
     * any such attempts
     */
    if (!retained->first_server_limit) {
        retained->first_server_limit = server_limit;
    }
    else if (server_limit != retained->first_server_limit) {
        /* don't need a startup console version here */
        ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00304)
                     "changing ServerLimit to %d from original value of %d "
                     "not allowed during restart",
                     server_limit, retained->first_server_limit);
        server_limit = retained->first_server_limit;
    }

    if (thread_limit > MAX_THREAD_LIMIT) {
        if (startup) {
            ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_STARTUP, 0, NULL, APLOGNO(00305)
                         "WARNING: ThreadLimit of %d exceeds compile-time "
                         "limit of %d threads, decreasing to %d.",
                         thread_limit, MAX_THREAD_LIMIT, MAX_THREAD_LIMIT);
        } else {
            ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00306)
                         "ThreadLimit of %d exceeds compile-time limit "
                         "of %d, decreasing to match",
                         thread_limit, MAX_THREAD_LIMIT);
        }
        thread_limit = MAX_THREAD_LIMIT;
    }
    else if (thread_limit < 1) {
        if (startup) {
            ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_STARTUP, 0, NULL, APLOGNO(00307)
                         "WARNING: ThreadLimit of %d not allowed, "
                         "increasing to 1.", thread_limit);
        } else {
            ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00308)
                         "ThreadLimit of %d not allowed, increasing to 1",
                         thread_limit);
        }
        thread_limit = 1;
    }

    /* you cannot change ThreadLimit across a restart; ignore
     * any such attempts
     */
    if (!retained->first_thread_limit) {
        retained->first_thread_limit = thread_limit;
    }
    else if (thread_limit != retained->first_thread_limit) {
        /* don't need a startup console version here */
        ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00309)
                     "changing ThreadLimit to %d from original value of %d "
                     "not allowed during restart",
                     thread_limit, retained->first_thread_limit);
        thread_limit = retained->first_thread_limit;
    }

    if (threads_per_child > thread_limit) {
        if (startup) {
            ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_STARTUP, 0, NULL, APLOGNO(00310)
                         "WARNING: ThreadsPerChild of %d exceeds ThreadLimit "
                         "of %d threads, decreasing to %d. "
                         "To increase, please see the ThreadLimit directive.",
                         threads_per_child, thread_limit, thread_limit);
        } else {
            ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00311)
                         "ThreadsPerChild of %d exceeds ThreadLimit "
                         "of %d, decreasing to match",
                         threads_per_child, thread_limit);
        }
        threads_per_child = thread_limit;
    }
    else if (threads_per_child < 1) {
        if (startup) {
            ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_STARTUP, 0, NULL, APLOGNO(00312)
                         "WARNING: ThreadsPerChild of %d not allowed, "
                         "increasing to 1.", threads_per_child);
        } else {
            ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00313)
                         "ThreadsPerChild of %d not allowed, increasing to 1",
                         threads_per_child);
        }
        threads_per_child = 1;
    }

    if (max_workers < threads_per_child) {
        if (startup) {
            ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_STARTUP, 0, NULL, APLOGNO(00314)
                         "WARNING: MaxRequestWorkers of %d is less than "
                         "ThreadsPerChild of %d, increasing to %d. "
                         "MaxRequestWorkers must be at least as large "
                         "as the number of threads in a single server.",
                         max_workers, threads_per_child, threads_per_child);
        } else {
            ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00315)
                         "MaxRequestWorkers of %d is less than ThreadsPerChild "
                         "of %d, increasing to match",
                         max_workers, threads_per_child);
        }
        max_workers = threads_per_child;
    }

    ap_daemons_limit = max_workers / threads_per_child;

    if (max_workers % threads_per_child) {
        int tmp_max_workers = ap_daemons_limit * threads_per_child;

        if (startup) {
            ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_STARTUP, 0, NULL, APLOGNO(00316)
                         "WARNING: MaxRequestWorkers of %d is not an integer "
                         "multiple of ThreadsPerChild of %d, decreasing to nearest "
                         "multiple %d, for a maximum of %d servers.",
                         max_workers, threads_per_child, tmp_max_workers,
                         ap_daemons_limit);
        } else {
            ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00317)
                         "MaxRequestWorkers of %d is not an integer multiple of "
                         "ThreadsPerChild of %d, decreasing to nearest "
                         "multiple %d", max_workers, threads_per_child,
                         tmp_max_workers);
        }
        max_workers = tmp_max_workers;
    }

    if (ap_daemons_limit > server_limit) {
        if (startup) {
            ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_STARTUP, 0, NULL, APLOGNO(00318)
                         "WARNING: MaxRequestWorkers of %d would require %d "
                         "servers and would exceed ServerLimit of %d, decreasing to %d. "
                         "To increase, please see the ServerLimit directive.",
                         max_workers, ap_daemons_limit, server_limit,
                         server_limit * threads_per_child);
        } else {
            ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00319)
                         "MaxRequestWorkers of %d would require %d servers and "
                         "exceed ServerLimit of %d, decreasing to %d",
                         max_workers, ap_daemons_limit, server_limit,
                         server_limit * threads_per_child);
        }
        ap_daemons_limit = server_limit;
    }

    /* ap_daemons_to_start > ap_daemons_limit checked in worker_run() */
    if (ap_daemons_to_start < 1) {
        if (startup) {
            ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_STARTUP, 0, NULL, APLOGNO(00320)
                         "WARNING: StartServers of %d not allowed, "
                         "increasing to 1.", ap_daemons_to_start);
        } else {
            ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00321)
                         "StartServers of %d not allowed, increasing to 1",
                         ap_daemons_to_start);
        }
        ap_daemons_to_start = 1;
    }

    if (min_spare_threads < 1) {
        if (startup) {
            ap_log_error(APLOG_MARK, APLOG_WARNING | APLOG_STARTUP, 0, NULL, APLOGNO(00322)
                         "WARNING: MinSpareThreads of %d not allowed, "
                         "increasing to 1 to avoid almost certain server failure. "
                         "Please read the documentation.", min_spare_threads);
        } else {
            ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s, APLOGNO(00323)
                         "MinSpareThreads of %d not allowed, increasing to 1",
                         min_spare_threads);
        }
        min_spare_threads = 1;
    }

    /* max_spare_threads < min_spare_threads + threads_per_child
     * checked in worker_run()
     */

    return OK;
}

static void worker_hooks(apr_pool_t *p)
{
    /* Our open_logs hook function must run before the core's, or stderr
     * will be redirected to a file, and the messages won't print to the
     * console.
     */
    static const char *const aszSucc[] = {"core.c", NULL};
    one_process = 0;

    ap_hook_open_logs(worker_open_logs, NULL, aszSucc, APR_HOOK_REALLY_FIRST);
    /* we need to set the MPM state before other pre-config hooks use MPM query
     * to retrieve it, so register as REALLY_FIRST
     */
    ap_hook_pre_config(worker_pre_config, NULL, NULL, APR_HOOK_REALLY_FIRST);
    ap_hook_check_config(worker_check_config, NULL, NULL, APR_HOOK_MIDDLE);
    ap_hook_mpm(worker_run, NULL, NULL, APR_HOOK_MIDDLE);
    ap_hook_mpm_query(worker_query, NULL, NULL, APR_HOOK_MIDDLE);
    ap_hook_mpm_get_name(worker_get_name, NULL, NULL, APR_HOOK_MIDDLE);
}

static const char *set_daemons_to_start(cmd_parms *cmd, void *dummy,
                                        const char *arg)
{
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err != NULL) {
        return err;
    }

    ap_daemons_to_start = atoi(arg);
    return NULL;
}

static const char *set_min_spare_threads(cmd_parms *cmd, void *dummy,
                                         const char *arg)
{
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err != NULL) {
        return err;
    }

    min_spare_threads = atoi(arg);
    return NULL;
}

static const char *set_max_spare_threads(cmd_parms *cmd, void *dummy,
                                         const char *arg)
{
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err != NULL) {
        return err;
    }

    max_spare_threads = atoi(arg);
    return NULL;
}

static const char *set_max_workers (cmd_parms *cmd, void *dummy,
                                     const char *arg)
{
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err != NULL) {
        return err;
    }
    if (!strcasecmp(cmd->cmd->name, "MaxClients")) {
        ap_log_error(APLOG_MARK, APLOG_INFO, 0, NULL, APLOGNO(00324)
                     "MaxClients is deprecated, use MaxRequestWorkers "
                     "instead.");
    }
    max_workers = atoi(arg);
    return NULL;
}

static const char *set_threads_per_child (cmd_parms *cmd, void *dummy,
                                          const char *arg)
{
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err != NULL) {
        return err;
    }

    threads_per_child = atoi(arg);
    return NULL;
}

static const char *set_server_limit (cmd_parms *cmd, void *dummy, const char *arg)
{
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err != NULL) {
        return err;
    }

    server_limit = atoi(arg);
    return NULL;
}

static const char *set_thread_limit (cmd_parms *cmd, void *dummy, const char *arg)
{
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err != NULL) {
        return err;
    }

    thread_limit = atoi(arg);
    return NULL;
}

static const command_rec worker_cmds[] = {
LISTEN_COMMANDS,
AP_INIT_TAKE1("StartServers", set_daemons_to_start, NULL, RSRC_CONF,
  "Number of child processes launched at server startup"),
AP_INIT_TAKE1("MinSpareThreads", set_min_spare_threads, NULL, RSRC_CONF,
  "Minimum number of idle threads, to handle request spikes"),
AP_INIT_TAKE1("MaxSpareThreads", set_max_spare_threads, NULL, RSRC_CONF,
  "Maximum number of idle threads"),
AP_INIT_TAKE1("MaxRequestWorkers", set_max_workers, NULL, RSRC_CONF,
  "Maximum number of threads alive at the same time"),
AP_INIT_TAKE1("MaxClients", set_max_workers, NULL, RSRC_CONF,
  "Deprecated name of MaxRequestWorkers"),
AP_INIT_TAKE1("ThreadsPerChild", set_threads_per_child, NULL, RSRC_CONF,
  "Number of threads each child creates"),
AP_INIT_TAKE1("ServerLimit", set_server_limit, NULL, RSRC_CONF,
  "Maximum number of child processes for this run of Apache"),
AP_INIT_TAKE1("ThreadLimit", set_thread_limit, NULL, RSRC_CONF,
  "Maximum number of worker threads per child process for this run of Apache - Upper limit for ThreadsPerChild"),
AP_GRACEFUL_SHUTDOWN_TIMEOUT_COMMAND,
{ NULL }
};

AP_DECLARE_MODULE(mpm_worker) = {
    MPM20_MODULE_STUFF,
    NULL,                       /* hook to run before apache parses args */
    NULL,                       /* create per-directory config structure */
    NULL,                       /* merge per-directory config structures */
    NULL,                       /* create per-server config structure */
    NULL,                       /* merge per-server config structures */
    worker_cmds,                /* command apr_table_t */
    worker_hooks                /* register_hooks */
};

