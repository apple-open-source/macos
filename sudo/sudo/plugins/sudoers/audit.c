/*
 * SPDX-License-Identifier: ISC
 *
 * Copyright (c) 2009-2015, 2019-2020 Todd C. Miller <Todd.Miller@sudo.ws>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This is an open source non-commercial project. Dear PVS-Studio, please check it.
 * PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
 */

#include <config.h>

#include <sys/wait.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sudoers.h"
#include "log_client.h"

#ifdef __APPLE__
#include <errno.h>
#include <EndpointSecuritySystem/ESSubmitSPI.h>
#endif /*__APPLE__ */

#ifdef HAVE_BSM_AUDIT
# include "bsm_audit.h"
#endif
#ifdef HAVE_LINUX_AUDIT
# include "linux_audit.h"
#endif
#ifdef HAVE_SOLARIS_AUDIT
# include "solaris_audit.h"
#endif

#ifdef SUDOERS_LOG_CLIENT
static struct log_details audit_details;
#endif
char *audit_msg = NULL;

/* sudoers_audit is declared at the end of this file. */
extern sudo_dso_public struct audit_plugin sudoers_audit;

static int
audit_success(char *const argv[])
{
    int rc = 0;
    debug_decl(audit_success, SUDOERS_DEBUG_AUDIT);

    if (argv != NULL) {
#ifdef HAVE_BSM_AUDIT
	if (bsm_audit_success(argv) == -1)
	    rc = -1;
#endif
#ifdef HAVE_LINUX_AUDIT
	if (linux_audit_command(argv, 1) == -1)
	    rc = -1;
#endif
#ifdef HAVE_SOLARIS_AUDIT
	if (solaris_audit_success(argv) == -1)
	    rc = -1;
#endif
    }

    debug_return_int(rc);
}

#ifdef __APPLE_AUDIT__
static void es_parse_command_info(
    char * const command_info[],
    char **command_out,
    char **to_username_out,
    uid_t *to_uid_out
)
{
    debug_decl(es_parse_command_info, SUDOERS_DEBUG_PLUGIN);
    char *p = NULL;
    for (size_t i = 0; command_info[i] != NULL; i++) {
         p = strstr(command_info[i], "command=");
        if (NULL != p) {
            *command_out = &command_info[i][sizeof("command=") - 1];
        }
        
        p = strstr(command_info[i], "runas_user=");
        if (NULL != p) {
            *to_username_out = &command_info[i][sizeof("runas_user=") - 1];
        }

        p = strstr(command_info[i], "runas_uid=");
        if (NULL != p) {
            char *to_uid_str = &command_info[i][sizeof("runas_uid=") - 1];
            char *end;
            long value = strtol(to_uid_str, &end, 10);
            if (!(end == to_uid_str || *end != '\0' || errno == ERANGE)) {
                *to_uid_out = (uid_t)value;
            }
        }
    }
    debug_return;
}

API_AVAILABLE(macos(14.0)) API_UNAVAILABLE(ios, tvos, watchos)
static void
es_accept(
    char * const command_info[],
    char * const run_argv[],
    char * const run_envp[]
)
{
    debug_decl(es_accept, SUDOERS_DEBUG_PLUGIN);
    __attribute__((weak_import)) typeof(ess_notify_sudo) ess_notify_sudo;
    if (ess_notify_sudo == NULL) {
        sudo_debug_printf(SUDO_DEBUG_INFO,  "%s: failed link ess_notify", __func__);
        return;
    }

    char *from_username = sudo_user.name;
    uid_t from_uid = sudo_user.uid;
    char *command = NULL;
    char *to_username = NULL;
    uid_t to_uid = -1;
    
    es_parse_command_info(command_info, &command, &to_username, &to_uid);

    ess_notify_sudo(
        true, // bool isAccepted
        NULL, // const ess_sudo_reject_info_t * __nullable reject_info
        &from_uid, // const uid_t * _Nullable from_uid,
        from_username, // const char * _Nullable from_username,
        to_uid == -1 ? NULL: &to_uid, // const uid_t * _Nullable to_uid,
        to_username, // const char * _Nullable to_username,
        command // const char* _Nullable command
    );
    debug_return;
}

API_AVAILABLE(macos(14.0)) API_UNAVAILABLE(ios, tvos, watchos)
static void
es_reject (
    const char *plugin_name, 
    unsigned int plugin_type,
    const char *message, 
    char * const command_info[]
)
{
    debug_decl(es_reject, SUDOERS_DEBUG_PLUGIN);
    __attribute__((weak_import)) typeof(ess_notify_sudo) ess_notify_sudo;
    if (ess_notify_sudo == NULL) {
        sudo_debug_printf(SUDO_DEBUG_INFO,  "%s: failed link ess_notify", __func__);
        return;
    }

    char *from_username = sudo_user.name;
    uid_t from_uid = sudo_user.uid;
    char *command = NULL;
    char *to_username = NULL;
    uid_t to_uid = -1;
    
    es_parse_command_info(command_info, &command, &to_username, &to_uid);

    ess_sudo_plugin_type_t lastElement = ESS_SUDO_PLUGIN_TYPE_APPROVAL;
    ess_sudo_reject_info_t reject_info = {};
    reject_info.failure_message = message;
    reject_info.plugin_name = plugin_name;
    reject_info.plugin_type = plugin_type <= lastElement ? plugin_type: ESS_SUDO_PLUGIN_TYPE_UNKNOWN;

    ess_notify_sudo(
        false, // bool isAccepted
        &reject_info, // const ess_sudo_reject_info_t * __nullable reject_info
        &from_uid, // const uid_t * _Nullable from_uid,
        from_username, // const char * _Nullable from_username,
        to_uid == -1 ? NULL: &to_uid, // const uid_t * _Nullable to_uid,
        to_username, // const char * _Nullable to_username,
        command // const char* _Nullable command
    );
    debug_return;
}
#endif /* __APPLE_AUDIT__ */

static int
audit_failure_int(char *const argv[], const char *message)
{
    int ret = 0;
    debug_decl(audit_failure_int, SUDOERS_DEBUG_AUDIT);

#if defined(HAVE_BSM_AUDIT) || defined(HAVE_LINUX_AUDIT)
    if (def_log_denied && argv != NULL) {
#ifdef HAVE_BSM_AUDIT
	if (bsm_audit_failure(argv, message) == -1)
	    ret = -1;
#endif
#ifdef HAVE_LINUX_AUDIT
	if (linux_audit_command(argv, 0) == -1)
	    ret = -1;
#endif
#ifdef HAVE_SOLARIS_AUDIT
	if (solaris_audit_failure(argv, message) == -1)
	    ret = -1;
#endif
    }
#endif /* HAVE_BSM_AUDIT || HAVE_LINUX_AUDIT */

    debug_return_int(ret);
}

int
vaudit_failure(char *const argv[], char const *const fmt, va_list ap)
{
    int oldlocale, ret;
    char *message;
    debug_decl(vaudit_failure, SUDOERS_DEBUG_AUDIT);

    /* Audit messages should be in the sudoers locale. */
    sudoers_setlocale(SUDOERS_LOCALE_SUDOERS, &oldlocale);

    if ((ret = vasprintf(&message, _(fmt), ap)) == -1)
	sudo_warnx(U_("%s: %s"), __func__, U_("unable to allocate memory"));

    if (ret != -1) {
	/* Set audit_msg for audit plugins.  */
	free(audit_msg);
	audit_msg = message;

	ret = audit_failure_int(argv, audit_msg);
    }

    sudoers_setlocale(oldlocale, NULL);

    debug_return_int(ret);
}

int
audit_failure(char *const argv[], char const *const fmt, ...)
{
    va_list ap;
    int ret;
    debug_decl(audit_failure, SUDOERS_DEBUG_AUDIT);

    va_start(ap, fmt);
    ret = vaudit_failure(argv, fmt, ap);
    va_end(ap);

    debug_return_int(ret);
}

static int
sudoers_audit_open(unsigned int version, sudo_conv_t conversation,
    sudo_printf_t plugin_printf, char * const settings[],
    char * const user_info[], int submit_optind, char * const submit_argv[],
    char * const submit_envp[], char * const plugin_options[],
    const char **errstr)
{
    struct sudo_conf_debug_file_list debug_files = TAILQ_HEAD_INITIALIZER(debug_files);
    struct sudoers_open_info info;
    const char *cp, *plugin_path = NULL;
    char * const *cur;
    int ret;
    debug_decl(sudoers_audit_open, SUDOERS_DEBUG_PLUGIN);

    sudo_conv = conversation;
    sudo_printf = plugin_printf;
    if (sudoers_audit.event_alloc != NULL)
	plugin_event_alloc = sudoers_audit.event_alloc;

    bindtextdomain("sudoers", LOCALEDIR);

    /* Initialize the debug subsystem.  */
    for (cur = settings; (cp = *cur) != NULL; cur++) {
	if (strncmp(cp, "debug_flags=", sizeof("debug_flags=") - 1) == 0) {
	    cp += sizeof("debug_flags=") - 1;
	    if (!sudoers_debug_parse_flags(&debug_files, cp))
		debug_return_int(-1);
	    continue;
	}
	if (strncmp(cp, "plugin_path=", sizeof("plugin_path=") - 1) == 0) {
	    plugin_path = cp + sizeof("plugin_path=") - 1;
	    continue;
	}
    }
    if (!sudoers_debug_register(plugin_path, &debug_files))
	debug_return_int(-1);

    /* Call the sudoers init function. */
    info.settings = settings;
    info.user_info = user_info;
    info.plugin_args = plugin_options;
    ret = sudoers_init(&info, log_parse_error, submit_envp);

    if (ret == true) {
	/* Unset close function if we don't need it to avoid extra process. */
#ifdef SUDOERS_LOG_CLIENT
	if (client_closure == NULL)
#endif
	    sudoers_audit.close = NULL;
    } else {
	/* The audit functions set audit_msg on failure. */
	if (audit_msg != NULL)
	    *errstr = audit_msg;
    }

    debug_return_int(ret);
}

static void
audit_to_eventlog(struct eventlog *evlog, char * const command_info[],
    char * const run_argv[], char * const run_envp[], const char *uuid_str)
{
    char * const *cur;
    debug_decl(audit_to_eventlog, SUDOERS_DEBUG_PLUGIN);

    /* Fill in evlog from sudoers Defaults, run_argv and run_envp. */
    sudoers_to_eventlog(evlog, NULL, run_argv, run_envp, uuid_str);

    /* Update iolog and execution environment from command_info[]. */
    if (command_info != NULL) {
	for (cur = command_info; *cur != NULL; cur++) {
	    switch (**cur) {
	    case 'c':
		if (strncmp(*cur, "command=", sizeof("command=") - 1) == 0) {
		    evlog->command = *cur + sizeof("command=") - 1;
		    continue;
		}
		if (strncmp(*cur, "chroot=", sizeof("chroot=") - 1) == 0) {
		    evlog->runchroot = *cur + sizeof("chroot=") - 1;
		    continue;
		}
		break;
	    case 'i':
		if (strncmp(*cur, "iolog_path=", sizeof("iolog_path=") - 1) == 0) {
		    evlog->iolog_path = *cur + sizeof("iolog_path=") - 1;
		    evlog->iolog_file = sudo_basename(evlog->iolog_path);
		    continue;
		}
		break;
	    case 'r':
		if (strncmp(*cur, "runcwd=", sizeof("runcwd=") - 1) == 0) {
		    evlog->runcwd = *cur + sizeof("runcwd=") - 1;
		    continue;
		}
		break;
	    }
	}
    }

    debug_return;
}

#ifdef SUDOERS_LOG_CLIENT
static bool
log_server_accept(struct eventlog *evlog)
{
    struct timespec now;
    bool ret = false;
    debug_decl(log_server_accept, SUDOERS_DEBUG_PLUGIN);

    if (SLIST_EMPTY(&def_log_servers))
	debug_return_bool(true);

    if (client_closure != NULL && ISSET(sudo_mode, MODE_POLICY_INTERCEPTED)) {
	/* Older servers don't support multiple commands per session. */
	if (!client_closure->subcommands)
	    debug_return_bool(true);
    } else {
	/* Only send accept event to log server if I/O log plugin did not. */
	if (iolog_enabled)
	    debug_return_bool(true);
    }

    if (sudo_gettime_real(&now) == -1) {
	sudo_warn("%s", U_("unable to get time of day"));
	goto done;
    }

    if (client_closure != NULL) {
	/* Use existing client closure. */
	if (fmt_accept_message(client_closure, evlog)) {
	    if (client_closure->write_ev->add(client_closure->write_ev,
		    &client_closure->log_details->server_timeout) == -1) {
		sudo_warn("%s", U_("unable to add event to queue"));
		goto done;
	    }
	    ret = true;
	}
    } else {
	if (!init_log_details(&audit_details, evlog))
	    goto done;

	/* Open connection to log server, send hello and accept messages. */
	client_closure = log_server_open(&audit_details, &now, false,
	    SEND_ACCEPT, NULL);
	if (client_closure != NULL)
	    ret = true;
    }

done:
    debug_return_bool(ret);
}

static void
log_server_exit(int status_type, int status)
{
    debug_decl(log_server_exit, SUDOERS_DEBUG_PLUGIN);

    if (client_closure != NULL) {
	int exit_status = 0, error = 0;

	if (status_type == SUDO_PLUGIN_WAIT_STATUS) {
	    if (WIFEXITED(status))
		exit_status = WEXITSTATUS(status);
	    else
		exit_status = WTERMSIG(status) | 128;
	} else {
	    /* Must be errno. */
	    error = status;
	}
	log_server_close(client_closure, exit_status, error);
	client_closure = NULL;
	free(audit_details.evlog);
	audit_details.evlog = NULL;
    }

    debug_return;
}
#else
static bool
log_server_accept(struct eventlog *evlog)
{
    return true;
}

static void
log_server_exit(int status_type, int status)
{
    return;
}
#endif /* SUDOERS_LOG_CLIENT */

static int
sudoers_audit_accept(const char *plugin_name, unsigned int plugin_type,
    char * const command_info[], char * const run_argv[],
    char * const run_envp[], const char **errstr)
{
    const char *uuid_str = NULL;
    struct eventlog evlog;
    static bool first = true;
    int ret = true;
    debug_decl(sudoers_audit_accept, SUDOERS_DEBUG_PLUGIN);

    /* Only log the accept event from the sudo front-end */
    if (plugin_type != SUDO_FRONT_END)
        debug_return_int(true);

#ifdef __APPLE_AUDIT__
    if (__builtin_available(macOS 14.0, *)) {
        es_accept(command_info, run_argv, run_envp);
    }
#endif /* __APPLE_AUDIT__ */

    if (!def_log_allowed)
        debug_return_int(true);
    if (audit_success(run_argv) != 0 && !def_ignore_audit_errors)
    ret = false;

    if (!ISSET(sudo_mode, MODE_POLICY_INTERCEPTED))
    uuid_str = sudo_user.uuid_str;

    audit_to_eventlog(&evlog, command_info, run_argv, run_envp, uuid_str);
    if (!log_allowed(&evlog) && !def_ignore_logfile_errors)
    ret = false;

    if (!log_server_accept(&evlog)) {
    if (!def_ignore_logfile_errors)
        ret = false;
    }

    if (first) {
    /* log_subcmds doesn't go through sudo_policy_main again to set this. */
    if (def_log_subcmds)
        SET(sudo_mode, MODE_POLICY_INTERCEPTED);
    first = false;
    }

    debug_return_int(ret);
}

static int
sudoers_audit_reject(const char *plugin_name, unsigned int plugin_type,
    const char *message, char * const command_info[], const char **errstr)
{
    struct eventlog evlog;
    int ret = true;
    debug_decl(sudoers_audit_reject, SUDOERS_DEBUG_PLUGIN);
#ifdef __APPLE_AUDIT__
    if (__builtin_available(macOS 14.0, *)) {
        es_reject(plugin_name, plugin_type, message, command_info);
    }
#endif /* __APPLE_AUDIT__ */
    /* Skip reject events that sudoers generated itself. */
    if (strncmp(plugin_name, "sudoers_", 8) == 0)
	debug_return_int(true);

    if (!def_log_denied)
	debug_return_int(true);

    if (audit_failure_int(NewArgv, message) != 0) {
	if (!def_ignore_audit_errors)
	    ret = false;
    }

    audit_to_eventlog(&evlog, command_info, NewArgv, env_get(), NULL);
    if (!eventlog_reject(&evlog, 0, message, NULL, NULL))
	ret = false;

    if (!log_server_reject(&evlog, message))
	ret = false;

    debug_return_int(ret);
}

static int
sudoers_audit_error(const char *plugin_name, unsigned int plugin_type,
    const char *message, char * const command_info[], const char **errstr)
{
    struct eventlog evlog;
    struct timespec now;
    int ret = true;
    debug_decl(sudoers_audit_error, SUDOERS_DEBUG_PLUGIN);

    /* Skip error events that sudoers generated itself. */
    if (strncmp(plugin_name, "sudoers_", 8) == 0)
	debug_return_int(true);

    if (audit_failure_int(NewArgv, message) != 0) {
	if (!def_ignore_audit_errors)
	    ret = false;
    }

    if (sudo_gettime_real(&now)) {
	sudo_warn("%s", U_("unable to get time of day"));
	debug_return_bool(false);
    }

    audit_to_eventlog(&evlog, command_info, NewArgv, env_get(), NULL);
    if (!eventlog_alert(&evlog, 0, &now, message, NULL))
	ret = false;

    if (!log_server_alert(&evlog, &now, message, NULL))
	ret = false;

    debug_return_int(ret);
}

static void
sudoers_audit_close(int status_type, int status)
{
    log_server_exit(status_type, status);
}

static int
sudoers_audit_version(int verbose)
{
    debug_decl(sudoers_audit_version, SUDOERS_DEBUG_PLUGIN);

    sudo_printf(SUDO_CONV_INFO_MSG, "Sudoers audit plugin version %s\n",
        PACKAGE_VERSION);

    debug_return_int(true);
}

sudo_dso_public struct audit_plugin sudoers_audit = {
    SUDO_AUDIT_PLUGIN,
    SUDO_API_VERSION,
    sudoers_audit_open,
    sudoers_audit_close,
    sudoers_audit_accept,
    sudoers_audit_reject,
    sudoers_audit_error,
    sudoers_audit_version,
    NULL, /* register_hooks */
    NULL, /* deregister_hooks */
    NULL /* event_alloc() filled in by sudo */
};
