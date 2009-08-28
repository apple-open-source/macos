/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include "macros.hpp"
#include "common.hpp"

#include <launch.h>
#include <spawn.h>
#include <errno.h>
#include <sys/fcntl.h>

#define LAUNCHD_MSG_ERR(job, key) \
	    VERBOSE("launch_msg(%s, %s): %s\n", job, #key, strerror(errno));

/* Uses gcc CPP extension. */
#define LAUNCHD_MSG_ERRMSG(job, key, fmt, ...) \
	    VERBOSE("launch_msg(%s, %s): " fmt, job, #key, ##__VA_ARGS__)

bool launchd_checkin(unsigned * idle_timeout_secs)
{
	launch_data_t msg;
	launch_data_t resp;
	launch_data_t item;
	bool is_launchd = true;

	msg = launch_data_new_string(LAUNCH_KEY_CHECKIN);
	resp = launch_msg(msg);
	if (resp == NULL) {
		/* IPC to launchd failed. */
		LAUNCHD_MSG_ERR("checkin", LAUNCH_KEY_CHECKIN);
		launch_data_free(msg);
		return false;
	}

	if (launch_data_get_type(resp) == LAUNCH_DATA_ERRNO) {
		errno = launch_data_get_errno(resp);
		goto done;
	}

	/* At this point, we know we are running under launchd. */
	is_launchd = true;

	if ((item = launch_data_dict_lookup(resp, LAUNCH_JOBKEY_TIMEOUT))) {
		long long val = launch_data_get_integer(item);

		*idle_timeout_secs =
			(val < 0) ? 0 : ((val > UINT_MAX) ?  UINT_MAX : val);
	}

done:
	launch_data_free(msg);
	launch_data_free(resp);
	return is_launchd;
}

typedef void (*dict_iterate_callback)(const launch_data_t,
					    const char *, void *);

static void fill_job_info(launch_data_t data, const char *key,
		    LaunchJobStatus * info)
{
	switch (launch_data_get_type(data)) {
	case LAUNCH_DATA_STRING:
	    if (strcmp(key, LAUNCH_JOBKEY_LABEL) == 0) {
		info->m_label = std::string(launch_data_get_string(data));
	    } else if (strcmp(key, LAUNCH_JOBKEY_PROGRAM) == 0) {
		info->m_program = std::string(launch_data_get_string(data));
	    }

	    return;

	case LAUNCH_DATA_INTEGER:
	    if (strcmp(key, LAUNCH_JOBKEY_PID) == 0) {
		info->m_pid = (pid_t)launch_data_get_integer(data);
	    }

	    return;

	case LAUNCH_DATA_DICTIONARY:
	    launch_data_dict_iterate(data,
			(dict_iterate_callback)fill_job_info, info);
	    return;

	default:
	    return; /* do nothing */
	}
}

bool
launchd_job_status(const char * job, LaunchJobStatus& info)
{
	launch_data_t resp;
	launch_data_t msg;

	msg = launch_data_alloc(LAUNCH_DATA_DICTIONARY);
	if (msg == NULL) {
	    throw std::runtime_error("out of memory");
	}

	launch_data_dict_insert(msg, launch_data_new_string(job),
				LAUNCH_KEY_GETJOB);

	resp = launch_msg(msg);
	launch_data_free(msg);

	if (resp == NULL) {
	    LAUNCHD_MSG_ERR(job, LAUNCH_KEY_GETJOB);
	    return false;
	}

	switch (launch_data_get_type(resp)) {
	case LAUNCH_DATA_DICTIONARY:
	    fill_job_info(resp, NULL, &info);
	    launch_data_free(resp);
	    return true;

	case LAUNCH_DATA_ERRNO:
	    errno = launch_data_get_errno(resp);
	    launch_data_free(resp);
	    if (errno != 0) {
		LAUNCHD_MSG_ERR(job, LAUNCH_KEY_GETJOB);
		return false;
	    }

	    return true;

	default:
	    LAUNCHD_MSG_ERRMSG(job, LAUNCH_KEY_GETJOB,
		    "unexpected respose type %d",
		    launch_data_get_type(resp));
	    launch_data_free(resp);
	    return false;
	}

}

bool
launchd_stop_job(const char * job)
{
	launch_data_t resp;
	launch_data_t msg;

	msg = launch_data_alloc(LAUNCH_DATA_DICTIONARY);
	if (msg == NULL) {
	    throw std::runtime_error("out of memory");
	}

	launch_data_dict_insert(msg, launch_data_new_string(job),
				LAUNCH_KEY_STOPJOB);

	resp = launch_msg(msg);
	launch_data_free(msg);

	if (resp == NULL) {
	    LAUNCHD_MSG_ERR(job, LAUNCH_KEY_STOPJOB);
	    return false;
	}

	switch (launch_data_get_type(resp)) {
	case LAUNCH_DATA_ERRNO:
	    errno = launch_data_get_errno(resp);
	    launch_data_free(resp);
	    if (errno != 0) {
		LAUNCHD_MSG_ERR(job, LAUNCH_KEY_STOPJOB);
		return false;
	    }

	    return true;

	default:
	    LAUNCHD_MSG_ERRMSG(job, LAUNCH_KEY_STOPJOB,
		    "unexpected respose type %d",
		    launch_data_get_type(resp));
	    launch_data_free(resp);
	    return false;
	}
}

static bool run_helper(const char ** argv)
{
    posix_spawn_file_actions_t action;
    int status;
    int err;
    pid_t child;

    err = posix_spawn_file_actions_init(&action);
    if (err != 0) {
	VERBOSE("spawn init failed for %s: %s\n", *argv, strerror(err));
	return false;
    }

    /* Redirect /dev/null to stdin. */
    err = posix_spawn_file_actions_addopen(&action, STDIN_FILENO,
		"/dev/null", O_RDONLY, 0 /* mode */);
    if (err != 0) {
	VERBOSE("stdin redirect failed for %s: %s\n", *argv, strerror(err));
    }

    /* Redirect stdout to /dev/null unless we are debugging. We continue past
     * any errors because redirecting is a nicety, rather than an essential
     * functions.
     */
    if (!(Options::Verbose || Options::Debug)) {
	err = posix_spawn_file_actions_addopen(&action, STDOUT_FILENO,
		    "/dev/null", O_WRONLY, 0 /* mode */);
	if (err != 0) {
	    VERBOSE("stdout redirect failed for %s: %s\n",
		    *argv, strerror(err));
	}

	err = posix_spawn_file_actions_addopen(&action, STDERR_FILENO,
		    "/dev/null", O_WRONLY, 0 /* mode */);
	if (err != 0) {
	    VERBOSE("stderr redirect failed for %s: %s\n",
		    *argv, strerror(err));
	}
    }

    err = posix_spawn(&child, *argv, &action, NULL /* posix_spawnattr_t */,
			(char * const *)argv, NULL /* environ */);
    if (err != 0) {
	VERBOSE("failed to spawn %s: %s\n", *argv, strerror(err));
	posix_spawn_file_actions_destroy(&action);
	return false;
    }

    posix_spawn_file_actions_destroy(&action);

    while (waitpid(child, &status, 0) != child) {
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS) {
	/* yay, the helper ran and secceeded. */
	return true;
    }

    return false;
}

bool launchd_unload_job(const LaunchService& svc)
{
    const char * unload_cmd[] =
    {
	"/bin/launchctl",
	"unload",
	"-w",
	NULL,
	NULL
    };

    unload_cmd[3] = svc.plist().c_str();
    return run_helper(unload_cmd);
}

bool launchd_load_job(const LaunchService& svc)
{
    const char * unload_cmd[] =
    {
	"/bin/launchctl",
	"load",
	"-w",
	NULL,
	NULL
    };

    unload_cmd[3] = svc.plist().c_str();
    return run_helper(unload_cmd);
}

bool smbcontrol_reconfigure(const char * daemon)
{
    const char * smbcontrol_cmd[] =
    {
	"/usr/bin/smbcontrol",
	NULL,
	"reload-config",
	NULL
    };

    smbcontrol_cmd[1] = daemon;
    return run_helper(smbcontrol_cmd);
}

/* vim: set cindent ts=8 sts=4 tw=79 : */
