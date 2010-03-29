/* Copyright (c) 2008 Apple Inc.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT
 * HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above
 * copyright holders shall not be used in advertising or otherwise to
 * promote the sale, use or other dealings in this Software without
 * prior written authorization.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <mach/mach.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>
#include <limits.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/time.h>
#include <launch.h>
#include <asl.h>
#include <errno.h>

#include "privileged_startx.h"
#include "privileged_startxServer.h"

union MaxMsgSize {
    union __RequestUnion__privileged_startx_subsystem req;
    union __ReplyUnion__privileged_startx_subsystem rep; 
};

#ifdef LAUNCH_JOBKEY_MACHSERVICES
#include <pthread.h>
static void* idle_thread(void* param __attribute__((unused)));

/* globals to trigger idle exit */
#define DEFAULT_IDLE_TIMEOUT 60 /* 60 second timeout, then the server exits */

struct idle_globals {
	mach_port_t      mp;
	long    timeout;
	struct timeval   lastmsg;
};

struct idle_globals idle_globals;
#endif

#ifndef SCRIPTDIR
#define SCRIPTDIR="/usr/X11/lib/X11/xinit/privileged_startx.d"
#endif

/* Default script dir */
const char *script_dir = SCRIPTDIR;

#ifndef LAUNCH_JOBKEY_MACHSERVICES
static mach_port_t checkin_or_register(char *bname) {
    kern_return_t kr;
    mach_port_t mp;
    
    /* If we're started by launchd or the old mach_init */
    kr = bootstrap_check_in(bootstrap_port, bname, &mp);
    if (kr == KERN_SUCCESS)
        return mp;
    
    /* We probably were not started by launchd or the old mach_init */
    kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &mp);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "mach_port_allocate(): %s\n", mach_error_string(kr));
        exit(EXIT_FAILURE);
    }
    
    kr = mach_port_insert_right(mach_task_self(), mp, mp, MACH_MSG_TYPE_MAKE_SEND);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "mach_port_insert_right(): %s\n", mach_error_string(kr));
        exit(EXIT_FAILURE);
    }
    
    kr = bootstrap_register(bootstrap_port, bname, mp);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "bootstrap_register(): %s\n", mach_error_string(kr));
        exit(EXIT_FAILURE);
    }
    
    return mp;
}
#endif

int server_main(const char *dir) {
    mach_msg_size_t mxmsgsz = sizeof(union MaxMsgSize) + MAX_TRAILER_SIZE;
    mach_port_t mp;
    kern_return_t kr;
#ifdef LAUNCH_JOBKEY_MACHSERVICES
    long idle_timeout = DEFAULT_IDLE_TIMEOUT;
#endif

    launch_data_t config = NULL, checkin = NULL;
    checkin = launch_data_new_string(LAUNCH_KEY_CHECKIN);
    config = launch_msg(checkin);
    if (!config || launch_data_get_type(config) == LAUNCH_DATA_ERRNO) {
        asl_log(NULL, NULL, ASL_LEVEL_ERR, "launchd checkin failed");
        exit(EXIT_FAILURE);
    }

    if(dir) {
        script_dir = dir;
        asl_log(NULL, NULL, ASL_LEVEL_DEBUG,
                "script directory set: %s", script_dir);
    }

#ifdef LAUNCH_JOBKEY_MACHSERVICES
    launch_data_t tmv;
    tmv = launch_data_dict_lookup(config, LAUNCH_JOBKEY_TIMEOUT);
    if (tmv) {
        idle_timeout = launch_data_get_integer(tmv);
        asl_log(NULL, NULL, ASL_LEVEL_DEBUG,
                "idle timeout set: %ld seconds", idle_timeout);
    }

    launch_data_t svc;
    svc = launch_data_dict_lookup(config, LAUNCH_JOBKEY_MACHSERVICES);
    if (!svc) {
        asl_log(NULL, NULL, ASL_LEVEL_ERR, "no mach services");
        exit(EXIT_FAILURE);
    }

    svc = launch_data_dict_lookup(svc, BOOTSTRAP_NAME);
    if (!svc) {
        asl_log(NULL, NULL, ASL_LEVEL_ERR, "no mach service: %s",
                BOOTSTRAP_NAME);
        exit(EXIT_FAILURE);
    }

    mp = launch_data_get_machport(svc);
#else
    mp = checkin_or_register(LAUNCHD_ID_PREFIX".privileged_startx");
#endif

    if (mp == MACH_PORT_NULL) {
        asl_log(NULL, NULL, ASL_LEVEL_ERR, "NULL mach service: %s",
                BOOTSTRAP_NAME);
        exit(EXIT_FAILURE);
    }

    /* insert a send right so we can send our idle exit message */
    kr = mach_port_insert_right(mach_task_self(), mp, mp,
                                MACH_MSG_TYPE_MAKE_SEND);
    if (kr != KERN_SUCCESS) {
        asl_log(NULL, NULL, ASL_LEVEL_ERR, "send right failed: %s",
                mach_error_string(kr));
        exit(EXIT_FAILURE);
    }

#ifdef LAUNCH_JOBKEY_MACHSERVICES
    /* spawn a thread to monitor our idle timeout */
    pthread_t thread;
    idle_globals.mp = mp;
    idle_globals.timeout = idle_timeout;
    gettimeofday(&idle_globals.lastmsg, NULL);
    pthread_create(&thread, NULL, &idle_thread, NULL);
#endif

    /* Main event loop */
    kr = mach_msg_server(privileged_startx_server, mxmsgsz, mp, 0);
    if (kr != KERN_SUCCESS) {
        asl_log(NULL, NULL, ASL_LEVEL_ERR,
                "mach_msg_server(mp): %s\n", mach_error_string(kr));
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}

static int ftscmp(const FTSENT **a, const FTSENT **b) {
    return strcmp((**a).fts_name, (**b).fts_name);
}

kern_return_t do_privileged_startx(mach_port_t test_port __attribute__((unused))) {
    kern_return_t retval = KERN_SUCCESS;
    char fn_buf[PATH_MAX + 1];
    char *s;
    int error_code;
    FTS *ftsp;
    FTSENT *ftsent;

    const char * path_argv[2] = {script_dir, NULL};

#ifdef LAUNCH_JOBKEY_MACHSERVICES
    /* Store that we were called, so the idle timer will reset */
    gettimeofday(&idle_globals.lastmsg, NULL);
#endif

    /* script_dir contains a set of files to run with root privs when X11 starts */
    ftsp = fts_open(path_argv, FTS_PHYSICAL, ftscmp);
    if(!ftsp) {
        asl_log(NULL, NULL, ASL_LEVEL_ERR,
                "do_privileged_startx: fts_open(%s): %s\n",
                script_dir, strerror(errno));
        return KERN_FAILURE;
    }

    /* Grab our dir */
    ftsent = fts_read(ftsp);
    if(!ftsent) {
        asl_log(NULL, NULL, ASL_LEVEL_ERR,
                "do_privileged_startx: fts_read(): %s\n", strerror(errno));
        fts_close(ftsp);
        return KERN_FAILURE;
    }

    /* Get a list of the files in this directory */
    ftsent = fts_children(ftsp, 0);
    if(!ftsent) {
        asl_log(NULL, NULL, ASL_LEVEL_ERR,
                "do_privileged_startx: fts_children(): %s\n", strerror(errno));
        fts_close(ftsp);
        return KERN_FAILURE;
    }

    /* Setup the buffer to have the path to the script dir */
    strncpy(fn_buf, script_dir, PATH_MAX-1);
    strcat(fn_buf, "/");
    s = strrchr(fn_buf, 0);

    /* Itterate over these files in alphabetical order */
    for(; ftsent; ftsent = ftsent->fts_link) {
        /* We only source regular files that are executable */
        /* Note: This assumes we own them, which should always be the case */
        if((ftsent->fts_statp->st_mode & S_IFREG) &&
           (ftsent->fts_statp->st_mode & S_IXUSR)) {

            /* Complete the full path filename in fn_buf */
            strcpy(s, ftsent->fts_name);

            /* Run it */
            error_code = system(fn_buf);
            if(error_code != 0) {
                asl_log(NULL, NULL, ASL_LEVEL_ERR,
                        "do_privileged_startx: %s: exited with status %d\n",
                        fn_buf, error_code);
                retval = KERN_FAILURE;
            }
        }
    }

    fts_close(ftsp);
    return retval;
}

kern_return_t do_idle_exit(mach_port_t test_port __attribute__((unused))) {
#ifdef LAUNCH_JOBKEY_MACHSERVICES
    struct timeval now;
    gettimeofday(&now, NULL);

    long delta = now.tv_sec - idle_globals.lastmsg.tv_sec;
    if (delta >= idle_globals.timeout) {
        exit(EXIT_SUCCESS);
    }

    return KERN_SUCCESS;
#else
    return KERN_FAILURE;
#endif
}

#ifdef LAUNCH_JOBKEY_MACHSERVICES
static void *idle_thread(void* param __attribute__((unused))) {
    for(;;) {
        struct timeval now;
        gettimeofday(&now, NULL);
        long delta = (now.tv_sec - idle_globals.lastmsg.tv_sec);
        if (delta < idle_globals.timeout) {
            /* sleep for remainder of timeout */
            sleep(idle_globals.timeout - delta);
        } else {
            /* timeout has elapsed, attempt to idle exit */
            idle_exit(idle_globals.mp);
        }
    }
    return NULL;
}
#endif
