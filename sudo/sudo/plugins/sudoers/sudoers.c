/*
 * SPDX-License-Identifier: ISC
 *
 * Copyright (c) 1993-1996, 1998-2022 Todd C. Miller <Todd.Miller@sudo.ws>
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
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 */

/*
 * This is an open source non-commercial project. Dear PVS-Studio, please check it.
 * PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
 */

#ifdef __TANDEM
# include <floss.h>
#endif

#include <config.h>

#include <sys/types.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <netdb.h>
#ifdef HAVE_LOGIN_CAP_H
# include <login_cap.h>
# ifndef LOGIN_DEFROOTCLASS
#  define LOGIN_DEFROOTCLASS	"daemon"
# endif
# ifndef LOGIN_SETENV
#  define LOGIN_SETENV	0
# endif
#endif
#ifdef HAVE_SELINUX
# include <selinux/selinux.h>
#endif
#include <ctype.h>
#ifndef HAVE_GETADDRINFO
# include "compat/getaddrinfo.h"
#endif

#include "sudoers.h"
#include "parse.h"
#include "check.h"
#include "auth/sudo_auth.h"
#include "sudo_iolog.h"

/*
 * Prototypes
 */
static int set_cmnd(void);
static bool init_vars(char * const *);
static bool set_loginclass(struct passwd *);
static bool set_runasgr(const char *, bool);
static bool set_runaspw(const char *, bool);
static bool tty_present(void);
static void set_callbacks(void);

/*
 * Globals
 */
struct sudo_user sudo_user;
struct passwd *list_pw;
uid_t timestamp_uid;
gid_t timestamp_gid;
bool force_umask;
int sudo_mode;

static char *prev_user;
static struct sudo_nss_list *snl;
static bool unknown_runas_uid;
static bool unknown_runas_gid;
static int cmnd_status = -1;
static struct defaults_list initial_defaults = TAILQ_HEAD_INITIALIZER(initial_defaults);

#ifdef __linux__
static struct rlimit nproclimit;
#endif

/* XXX - must be extern for audit bits of sudo_auth.c */
int NewArgc;
char **NewArgv;
char **saved_argv;

#ifdef SUDOERS_LOG_CLIENT
# define remote_iologs	(!SLIST_EMPTY(&def_log_servers))
#else
# define remote_iologs	0
#endif

/*
 * Unlimit the number of processes since Linux's setuid() will
 * apply resource limits when changing uid and return EAGAIN if
 * nproc would be exceeded by the uid switch.
 */
static void
unlimit_nproc(void)
{
#ifdef __linux__
    struct rlimit rl;
    debug_decl(unlimit_nproc, SUDOERS_DEBUG_UTIL);

    if (getrlimit(RLIMIT_NPROC, &nproclimit) != 0)
	    sudo_warn("getrlimit(RLIMIT_NPROC)");
    rl.rlim_cur = rl.rlim_max = RLIM_INFINITY;
    if (setrlimit(RLIMIT_NPROC, &rl) != 0) {
	rl.rlim_cur = rl.rlim_max = nproclimit.rlim_max;
	if (setrlimit(RLIMIT_NPROC, &rl) != 0)
	    sudo_warn("setrlimit(RLIMIT_NPROC)");
    }
    debug_return;
#endif /* __linux__ */
}

/*
 * Restore saved value of RLIMIT_NPROC.
 */
static void
restore_nproc(void)
{
#ifdef __linux__
    debug_decl(restore_nproc, SUDOERS_DEBUG_UTIL);

    if (setrlimit(RLIMIT_NPROC, &nproclimit) != 0)
	sudo_warn("setrlimit(RLIMIT_NPROC)");

    debug_return;
#endif /* __linux__ */
}

/*
 * Re-initialize Defaults settings.
 * We do not warn, log or send mail for errors when reinitializing,
 * this would have already been done the first time through.
 */
static bool
sudoers_reinit_defaults(void)
{
    struct sudo_nss *nss, *nss_next;
    sudoers_logger_t logger = sudoers_error_hook;
    debug_decl(sudoers_reinit_defaults, SUDOERS_DEBUG_PLUGIN);

    if (!init_defaults()) {
	sudo_warnx("%s", U_("unable to initialize sudoers default values"));
	debug_return_bool(false);
    }

    /* It should not be possible for the initial defaults to fail to apply. */
    if (!update_defaults(NULL, &initial_defaults,
	    SETDEF_GENERIC|SETDEF_HOST|SETDEF_USER|SETDEF_RUNAS, false))
	debug_return_bool(false);

    /* Disable error logging while re-processing defaults. */
    sudoers_error_hook = NULL;

    TAILQ_FOREACH_SAFE(nss, snl, entries, nss_next) {
	/* Missing/invalid defaults is not a fatal error. */
	if (nss->getdefs(nss) != -1) {
	    (void)update_defaults(nss->parse_tree, NULL,
		SETDEF_GENERIC|SETDEF_HOST|SETDEF_USER|SETDEF_RUNAS, true);
	}
    }

    /* Restore error logging. */
    sudoers_error_hook = logger;

    /* No need to check the admin flag file multiple times. */
    if (ISSET(sudo_mode, MODE_POLICY_INTERCEPTED))
	def_admin_flag = NULL;

    debug_return_bool(true);
}

int
sudoers_init(void *info, sudoers_logger_t logger, char * const envp[])
{
    struct sudo_nss *nss, *nss_next;
    int oldlocale, sources = 0;
    static int ret = -1;
    debug_decl(sudoers_init, SUDOERS_DEBUG_PLUGIN);

    /* Only initialize once. */
    if (snl != NULL)
	debug_return_int(ret);

    bindtextdomain("sudoers", LOCALEDIR);

    /* Hook up logging function for parse errors. */
    sudoers_error_hook = logger;

    /* Register fatal/fatalx callback. */
    sudo_fatal_callback_register(sudoers_cleanup);

    /* Initialize environment functions (including replacements). */
    if (!env_init(envp))
	debug_return_int(-1);

    /* Setup defaults data structures. */
    if (!init_defaults()) {
	sudo_warnx("%s", U_("unable to initialize sudoers default values"));
	debug_return_int(-1);
    }

    /* Parse info from front-end. */
    sudo_mode = sudoers_policy_deserialize_info(info, &initial_defaults);
    if (ISSET(sudo_mode, MODE_ERROR))
	debug_return_int(-1);

    if (!init_vars(envp))
	debug_return_int(-1);

    /* Parse nsswitch.conf for sudoers order. */
    snl = sudo_read_nss();

    /* LDAP or NSS may modify the euid so we need to be root for the open. */
    if (!set_perms(PERM_ROOT))
	debug_return_int(-1);

    /* Use the C locale unless another is specified in sudoers. */
    sudoers_setlocale(SUDOERS_LOCALE_SUDOERS, &oldlocale);
    sudo_warn_set_locale_func(sudoers_warn_setlocale);

    /* Update defaults set by front-end. */
    if (!update_defaults(NULL, &initial_defaults,
	    SETDEF_GENERIC|SETDEF_HOST|SETDEF_USER|SETDEF_RUNAS, false)) {
	goto cleanup;
    }

    /* Open and parse sudoers, set global defaults.  */
    init_parser(sudoers_file, false, false);
    TAILQ_FOREACH_SAFE(nss, snl, entries, nss_next) {
	if (nss->open(nss) == -1 || (nss->parse_tree = nss->parse(nss)) == NULL) {
	    TAILQ_REMOVE(snl, nss, entries);
	    continue;
	}
	sources++;

	/* Missing/invalid defaults is not a fatal error. */
	if (nss->getdefs(nss) == -1) {
	    log_warningx(SLOG_SEND_MAIL|SLOG_NO_STDERR,
		N_("unable to get defaults from %s"), nss->source);
	} else {
	    (void)update_defaults(nss->parse_tree, NULL,
		SETDEF_GENERIC|SETDEF_HOST|SETDEF_USER|SETDEF_RUNAS, false);
	}
    }
    if (sources == 0) {
	sudo_warnx("%s", U_("no valid sudoers sources found, quitting"));
	goto cleanup;
    }

    /* Set login class if applicable (after sudoers is parsed). */
    if (set_loginclass(runas_pw ? runas_pw : sudo_user.pw))
	ret = true;

cleanup:
    mail_parse_errors();

    if (!restore_perms())
	ret = -1;

    /* Restore user's locale. */
    sudo_warn_set_locale_func(NULL);
    sudoers_setlocale(oldlocale, NULL);

    debug_return_int(ret);
}

/*
 * Expand I/O log dir and file into a full path.
 * Returns the full I/O log path prefixed with "iolog_path=".
 * Sets sudo_user.iolog_file as a side effect.
 */
static char *
format_iolog_path(void)
{
    char dir[PATH_MAX], file[PATH_MAX];
    char *iolog_path = NULL;
    int oldlocale;
    bool ok;
    debug_decl(format_iolog_path, SUDOERS_DEBUG_PLUGIN);

    /* Use sudoers locale for strftime() */
    sudoers_setlocale(SUDOERS_LOCALE_SUDOERS, &oldlocale);
    ok = expand_iolog_path(def_iolog_dir, dir, sizeof(dir),
	&sudoers_iolog_path_escapes[1], NULL);
    if (ok) {
	ok = expand_iolog_path(def_iolog_file, file, sizeof(file),
	    &sudoers_iolog_path_escapes[0], dir);
    }
    sudoers_setlocale(oldlocale, NULL);
    if (!ok)
	goto done;

    if (asprintf(&iolog_path, "iolog_path=%s/%s", dir, file) == -1) {
	iolog_path = NULL;
	goto done;
    }
	
    /* Stash pointer to the I/O log for the event log. */
    sudo_user.iolog_path = iolog_path + sizeof("iolog_path=") - 1;
    sudo_user.iolog_file = sudo_user.iolog_path + 1 + strlen(dir);

done:
    debug_return_str(iolog_path);
}

static int
check_user_runchroot(void)
{
    debug_decl(check_user_runchroot, SUDOERS_DEBUG_PLUGIN);

    if (user_runchroot == NULL)
	debug_return_bool(true);

    sudo_debug_printf(SUDO_DEBUG_INFO|SUDO_DEBUG_LINENO,
	"def_runchroot %s, user_runchroot %s",
	def_runchroot ? def_runchroot : "none",
	user_runchroot ? user_runchroot : "none");

    if (def_runchroot == NULL || (strcmp(def_runchroot, "*") != 0 &&
	    strcmp(def_runchroot, user_runchroot) != 0)) {
	log_warningx(SLOG_NO_STDERR|SLOG_AUDIT,
	    N_("user not allowed to change root directory to %s"),
	    user_runchroot);
	sudo_warnx(U_("you are not permitted to use the -R option with %s"),
	    user_cmnd);
	debug_return_bool(false);
    }
    free(def_runchroot);
    if ((def_runchroot = strdup(user_runchroot)) == NULL) {
	sudo_warnx(U_("%s: %s"), __func__, U_("unable to allocate memory"));
	debug_return_int(-1);
    }
    debug_return_bool(true);
}

static int
check_user_runcwd(void)
{
    debug_decl(check_user_runcwd, SUDOERS_DEBUG_PLUGIN);

    sudo_debug_printf(SUDO_DEBUG_INFO|SUDO_DEBUG_LINENO,
	"def_runcwd %s, user_runcwd %s, user_cwd %s",
	def_runcwd ? def_runcwd : "none", user_runcwd ? user_runcwd : "none",
	user_cwd ? user_cwd : "none");

    if (strcmp(user_cwd, user_runcwd) != 0) {
	if (def_runcwd == NULL || strcmp(def_runcwd, "*") != 0) {
	    log_warningx(SLOG_NO_STDERR|SLOG_AUDIT,
		N_("user not allowed to change directory to %s"), user_runcwd);
	    sudo_warnx(U_("you are not permitted to use the -D option with %s"),
		user_cmnd);
	    debug_return_bool(false);
	}
	free(def_runcwd);
	if ((def_runcwd = strdup(user_runcwd)) == NULL) {
	    sudo_warnx(U_("%s: %s"), __func__, U_("unable to allocate memory"));
	    debug_return_int(-1);
	}
    }
    debug_return_bool(true);
}

static bool need_reinit;

int
sudoers_policy_main(int argc, char * const argv[], int pwflag, char *env_add[],
    bool verbose, void *closure)
{
    char *iolog_path = NULL;
    mode_t cmnd_umask = ACCESSPERMS;
    int oldlocale, validated, ret = -1;
    debug_decl(sudoers_policy_main, SUDOERS_DEBUG_PLUGIN);

    sudo_warn_set_locale_func(sudoers_warn_setlocale);

    if (argc == 0) {
	sudo_warnx("%s", U_("no command specified"));
	debug_return_int(-1);
    }

    if (need_reinit) {
	/* Was previous command intercepted? */
	if (ISSET(sudo_mode, MODE_RUN) && def_intercept)
	    SET(sudo_mode, MODE_POLICY_INTERCEPTED);

	/* Only certain mode flags are legal for intercepted commands. */
	if (ISSET(sudo_mode, MODE_POLICY_INTERCEPTED))
	    sudo_mode &= MODE_INTERCEPT_MASK;

	/* Re-initialize defaults if we are called multiple times. */
	if (!sudoers_reinit_defaults())
	    debug_return_int(-1);
    }
    need_reinit = true;

    unlimit_nproc();

    /* Is root even allowed to run sudo? */
    if (user_uid == 0 && !def_root_sudo) {
	/* Not an audit event (should it be?). */
	sudo_warnx("%s",
	    U_("sudoers specifies that root is not allowed to sudo"));
	goto bad;
    }

    if (!set_perms(PERM_INITIAL))
	goto bad;

    /* Environment variables specified on the command line. */
    if (env_add != NULL && env_add[0] != NULL)
	sudo_user.env_vars = env_add;

    /*
     * Make a local copy of argc/argv, with special handling for the
     * '-i' option.  We also allocate an extra slot for bash's --login.
     */
    if (NewArgv != NULL && NewArgv != saved_argv) {
	sudoers_gc_remove(GC_PTR, NewArgv);
	free(NewArgv);
    }
    NewArgv = reallocarray(NULL, argc + 2, sizeof(char *));
    if (NewArgv == NULL) {
	sudo_warnx(U_("%s: %s"), __func__, U_("unable to allocate memory"));
	goto done;
    }
    sudoers_gc_add(GC_PTR, NewArgv);
    if (ISSET(sudo_mode, MODE_CHECK)) {
	/* For "sudo -l [-U otheruser] command" */
	NewArgv[0] = (char *)"list";
	memcpy(NewArgv + 1, argv, argc * sizeof(char *));
	NewArgc = argc + 1;
    } else {
	memcpy(NewArgv, argv, argc * sizeof(char *));
	NewArgc = argc;
    }
    NewArgv[NewArgc] = NULL;
    if (ISSET(sudo_mode, MODE_LOGIN_SHELL) && runas_pw != NULL) {
	NewArgv[0] = strdup(runas_pw->pw_shell);
	if (NewArgv[0] == NULL) {
	    sudo_warnx(U_("%s: %s"), __func__, U_("unable to allocate memory"));
	    goto done;
	}
	sudoers_gc_add(GC_PTR, NewArgv[0]);
    }

    /* If given the -P option, set the "preserve_groups" flag. */
    if (ISSET(sudo_mode, MODE_PRESERVE_GROUPS))
	def_preserve_groups = true;

    /* Find command in path and apply per-command Defaults. */
    cmnd_status = set_cmnd();
    if (cmnd_status == NOT_FOUND_ERROR)
	goto done;

    /* Check for -C overriding def_closefrom. */
    if (user_closefrom >= 0 && user_closefrom != def_closefrom) {
	if (!def_closefrom_override) {
	    log_warningx(SLOG_NO_STDERR|SLOG_AUDIT,
		N_("user not allowed to override closefrom limit"));
	    sudo_warnx("%s", U_("you are not permitted to use the -C option"));
	    goto bad;
	}
	def_closefrom = user_closefrom;
    }

    /*
     * Check sudoers sources, using the locale specified in sudoers.
     */
    sudoers_setlocale(SUDOERS_LOCALE_SUDOERS, &oldlocale);
    validated = sudoers_lookup(snl, sudo_user.pw, &cmnd_status, pwflag);
    if (ISSET(validated, VALIDATE_ERROR)) {
	/* The lookup function should have printed an error. */
	goto done;
    }

    /* Restore user's locale. */
    sudoers_setlocale(oldlocale, NULL);

    if (safe_cmnd == NULL) {
	if ((safe_cmnd = strdup(user_cmnd)) == NULL) {
	    sudo_warnx(U_("%s: %s"), __func__, U_("unable to allocate memory"));
	    goto done;
	}
    }

    /* Defer uid/gid checks until after defaults have been updated. */
    if (unknown_runas_uid && !def_runas_allow_unknown_id) {
	log_warningx(SLOG_AUDIT, N_("unknown user %s"), runas_pw->pw_name);
	goto done;
    }
    if (runas_gr != NULL) {
	if (unknown_runas_gid && !def_runas_allow_unknown_id) {
	    log_warningx(SLOG_AUDIT, N_("unknown group %s"),
		runas_gr->gr_name);
	    goto done;
	}
    }

    /*
     * Look up the timestamp dir owner if one is specified.
     */
    if (def_timestampowner) {
	struct passwd *pw = NULL;

	if (*def_timestampowner == '#') {
	    const char *errstr;
	    uid_t uid = sudo_strtoid(def_timestampowner + 1, &errstr);
	    if (errstr == NULL)
		pw = sudo_getpwuid(uid);
	}
	if (pw == NULL)
	    pw = sudo_getpwnam(def_timestampowner);
	if (pw != NULL) {
	    timestamp_uid = pw->pw_uid;
	    timestamp_gid = pw->pw_gid;
	    sudo_pw_delref(pw);
	} else {
	    /* XXX - audit too? */
	    log_warningx(SLOG_SEND_MAIL,
		N_("timestamp owner (%s): No such user"), def_timestampowner);
	    timestamp_uid = ROOT_UID;
	    timestamp_gid = ROOT_GID;
	}
    }

    /* If no command line args and "shell_noargs" is not set, error out. */
    if (ISSET(sudo_mode, MODE_IMPLIED_SHELL) && !def_shell_noargs) {
	/* Not an audit event. */
	ret = -2; /* usage error */
	goto done;
    }

    /* Bail if a tty is required and we don't have one.  */
    if (def_requiretty && !tty_present()) {
	log_warningx(SLOG_NO_STDERR|SLOG_AUDIT, N_("no tty"));
	sudo_warnx("%s", U_("sorry, you must have a tty to run sudo"));
	goto bad;
    }

    /* Check runas user's shell. */
    if (!check_user_shell(runas_pw)) {
	log_warningx(SLOG_RAW_MSG|SLOG_AUDIT,
	    N_("invalid shell for user %s: %s"),
	    runas_pw->pw_name, runas_pw->pw_shell);
	goto bad;
    }

    /*
     * We don't reset the environment for sudoedit or if the user
     * specified the -E command line flag and they have setenv privs.
     */
    if (ISSET(sudo_mode, MODE_EDIT) ||
	(ISSET(sudo_mode, MODE_PRESERVE_ENV) && def_setenv))
	def_env_reset = false;

    /* Build a new environment that avoids any nasty bits. */
    if (!rebuild_env())
	goto bad;

    /* Require a password if sudoers says so.  */
    switch (check_user(validated, sudo_mode)) {
    case true:
	/* user authenticated successfully. */
	break;
    case false:
	/* Note: log_denial() calls audit for us. */
	if (!ISSET(validated, VALIDATE_SUCCESS)) {
	    /* Only display a denial message if no password was read. */
	    if (!log_denial(validated, def_passwd_tries <= 0))
		goto done;
	}
	goto bad;
    default:
	/* some other error, ret is -1. */
	goto done;
    }

    /* Check whether user_runchroot is permitted (if specified). */
    switch (check_user_runchroot()) {
    case true:
	break;
    case false:
	goto bad;
    default:
	goto done;
    }

    /* Check whether user_runcwd is permitted (if specified). */
    switch (check_user_runcwd()) {
    case true:
	break;
    case false:
	goto bad;
    default:
	goto done;
    }

    /* If run as root with SUDO_USER set, set sudo_user.pw to that user. */
    /* XXX - causes confusion when root is not listed in sudoers */
    if (ISSET(sudo_mode, MODE_RUN|MODE_EDIT) && prev_user != NULL) {
	if (user_uid == 0 && strcmp(prev_user, "root") != 0) {
	    struct passwd *pw;

	    if ((pw = sudo_getpwnam(prev_user)) != NULL) {
		    if (sudo_user.pw != NULL)
			sudo_pw_delref(sudo_user.pw);
		    sudo_user.pw = pw;
	    }
	}
    }

    /* If the user was not allowed to run the command we are done. */
    if (!ISSET(validated, VALIDATE_SUCCESS)) {
	/* Note: log_failure() calls audit for us. */
	if (!log_failure(validated, cmnd_status))
	    goto done;
	goto bad;
    }

    /* Create Ubuntu-style dot file to indicate sudo was successful. */
    if (create_admin_success_flag() == -1)
	goto done;

    /* Finally tell the user if the command did not exist. */
    if (cmnd_status == NOT_FOUND_DOT) {
	audit_failure(NewArgv, N_("command in current directory"));
	sudo_warnx(U_("ignoring \"%s\" found in '.'\nUse \"sudo ./%s\" if this is the \"%s\" you wish to run."), user_cmnd, user_cmnd, user_cmnd);
	goto bad;
    } else if (cmnd_status == NOT_FOUND) {
	if (ISSET(sudo_mode, MODE_CHECK)) {
	    audit_failure(NewArgv, N_("%s: command not found"),
		NewArgv[1]);
	    sudo_warnx(U_("%s: command not found"), NewArgv[1]);
	} else {
	    audit_failure(NewArgv, N_("%s: command not found"),
		user_cmnd);
	    sudo_warnx(U_("%s: command not found"), user_cmnd);
	    if (strncmp(user_cmnd, "cd", 2) == 0 && (user_cmnd[2] == '\0' ||
		    isblank((unsigned char)user_cmnd[2]))) {
		sudo_warnx("%s",
		    U_("\"cd\" is a shell built-in command, it cannot be run directly."));
		sudo_warnx("%s",
		    U_("the -s option may be used to run a privileged shell."));
		sudo_warnx("%s",
		    U_("the -D option may be used to run a command in a specific directory."));
	    }
	}
	goto bad;
    }

    /* If user specified a timeout make sure sudoers allows it. */
    if (!def_user_command_timeouts && user_timeout > 0) {
	log_warningx(SLOG_NO_STDERR|SLOG_AUDIT,
	    N_("user not allowed to set a command timeout"));
	sudo_warnx("%s",
	    U_("sorry, you are not allowed set a command timeout"));
	goto bad;
    }

    /* If user specified env vars make sure sudoers allows it. */
    if (ISSET(sudo_mode, MODE_RUN) && !def_setenv) {
	if (ISSET(sudo_mode, MODE_PRESERVE_ENV)) {
	    log_warningx(SLOG_NO_STDERR|SLOG_AUDIT,
		N_("user not allowed to preserve the environment"));
	    sudo_warnx("%s",
		U_("sorry, you are not allowed to preserve the environment"));
	    goto bad;
	} else {
	    if (!validate_env_vars(sudo_user.env_vars))
		goto bad;
	}
    }

    if (ISSET(sudo_mode, (MODE_RUN | MODE_EDIT)) && !remote_iologs) {
	if (iolog_enabled && def_iolog_file && def_iolog_dir) {
	    if ((iolog_path = format_iolog_path()) == NULL) {
		if (!def_ignore_iolog_errors)
		    goto done;
		/* Unable to expand I/O log path, disable I/O logging. */
		def_log_input = false;
		def_log_output = false;
		def_log_stdin = false;
		def_log_stdout = false;
		def_log_stderr = false;
		def_log_ttyin = false;
		def_log_ttyout = false;
	    }
	}
    }

    switch (sudo_mode & MODE_MASK) {
	case MODE_CHECK:
	    ret = display_cmnd(snl, list_pw ? list_pw : sudo_user.pw);
	    goto done;
	case MODE_LIST:
	    ret = display_privs(snl, list_pw ? list_pw : sudo_user.pw, verbose);
	    goto done;
	case MODE_VALIDATE:
	    ret = true;
	    goto done;
	case MODE_RUN:
	case MODE_EDIT:
	    /* ret will not be set until the very end. */
	    break;
	default:
	    /* Should not happen. */
	    sudo_warnx("internal error, unexpected sudo mode 0x%x", sudo_mode);
	    goto done;
    }

    /*
     * Set umask based on sudoers.
     * If user's umask is more restrictive, OR in those bits too
     * unless umask_override is set.
     */
    if (def_umask != ACCESSPERMS) {
	cmnd_umask = def_umask;
	if (!def_umask_override)
	    cmnd_umask |= user_umask;
    }

    if (ISSET(sudo_mode, MODE_LOGIN_SHELL)) {
	char *p;

	/* Convert /bin/sh -> -sh so shell knows it is a login shell */
	if ((p = strrchr(NewArgv[0], '/')) == NULL)
	    p = NewArgv[0];
	*p = '-';
	NewArgv[0] = p;

	/*
	 * Newer versions of bash require the --login option to be used
	 * in conjunction with the -c option even if the shell name starts
	 * with a '-'.  Unfortunately, bash 1.x uses -login, not --login
	 * so this will cause an error for that.
	 */
	if (NewArgc > 1 && strcmp(NewArgv[0], "-bash") == 0 &&
	    strcmp(NewArgv[1], "-c") == 0) {
	    /* We allocated extra space for the --login above. */
	    memmove(&NewArgv[2], &NewArgv[1], sizeof(char *) * NewArgc);
	    NewArgv[1] = (char *)"--login";
	    NewArgc++;
	}

#if defined(_AIX) || (defined(__linux__) && !defined(HAVE_PAM))
	/* Insert system-wide environment variables. */
	if (!read_env_file(_PATH_ENVIRONMENT, true, false))
	    sudo_warn("%s", _PATH_ENVIRONMENT);
#endif
#ifdef HAVE_LOGIN_CAP_H
	/* Set environment based on login class. */
	if (login_class) {
	    login_cap_t *lc = login_getclass(login_class);
	    if (lc != NULL) {
		setusercontext(lc, runas_pw, runas_pw->pw_uid, LOGIN_SETPATH|LOGIN_SETENV);
		login_close(lc);
	    }
	}
#endif /* HAVE_LOGIN_CAP_H */
    }

    /* Insert system-wide environment variables. */
    if (def_restricted_env_file) {
	if (!read_env_file(def_restricted_env_file, false, true))
	    sudo_warn("%s", def_restricted_env_file);
    }
    if (def_env_file) {
	if (!read_env_file(def_env_file, false, false))
	    sudo_warn("%s", def_env_file);
    }

    /* Insert user-specified environment variables. */
    if (!insert_env_vars(sudo_user.env_vars)) {
	sudo_warnx("%s",
	    U_("error setting user-specified environment variables"));
	goto done;
    }

    /* Note: must call audit before uid change. */
    if (ISSET(sudo_mode, MODE_EDIT)) {
	const char *env_editor = NULL;
	char **edit_argv;
	int edit_argc;

	sudoedit_nfiles = NewArgc - 1;
	free(safe_cmnd);
	safe_cmnd = find_editor(sudoedit_nfiles, NewArgv + 1, &edit_argc,
	    &edit_argv, NULL, &env_editor);
	if (safe_cmnd == NULL) {
	    switch (errno) {
	    case ENOENT:
		audit_failure(NewArgv, N_("%s: command not found"),
		    env_editor ? env_editor : def_editor);
		sudo_warnx(U_("%s: command not found"),
		    env_editor ? env_editor : def_editor);
		goto bad;
	    case EINVAL:
		if (def_env_editor && env_editor != NULL) {
		    /* User tried to do something funny with the editor. */
		    log_warningx(SLOG_NO_STDERR|SLOG_AUDIT|SLOG_SEND_MAIL,
			"invalid user-specified editor: %s", env_editor);
		    goto bad;
		}
		FALLTHROUGH;
	    default:
		goto done;
	    }
	}
	/* find_editor() already g/c'd edit_argv[] */
	if (NewArgv != saved_argv) {
	    sudoers_gc_remove(GC_PTR, NewArgv);
	    free(NewArgv);
	}
	NewArgv = edit_argv;
	NewArgc = edit_argc;

	/* We want to run the editor with the unmodified environment. */
	env_swap_old();
    }

    /* Save the initial command and argv so we have it for exit logging. */
    if (saved_cmnd == NULL) {
	saved_cmnd = strdup(safe_cmnd);
	if (saved_cmnd == NULL) {
	    sudo_warnx(U_("%s: %s"), __func__, U_("unable to allocate memory"));
	    goto done;
	}
	saved_argv = NewArgv;
    }

    ret = true;
    goto done;

bad:
    ret = false;

done:
    mail_parse_errors();

    if (def_group_plugin)
	group_plugin_unload();
    init_parser(NULL, false, false);

    if (ret == -1) {
	/* Free stashed copy of the environment. */
	(void)env_init(NULL);

	/* Free locally-allocated strings. */
	free(iolog_path);
    } else {
	/* Store settings to pass back to front-end. */
	if (!sudoers_policy_store_result(ret, NewArgv, env_get(), cmnd_umask,
		iolog_path, closure))
	    ret = -1;
    }

    if (!rewind_perms())
	ret = -1;

    restore_nproc();

    sudo_warn_set_locale_func(NULL);

    debug_return_int(ret);
}

/*
 * Initialize timezone and fill in sudo_user struct.
 */
static bool
init_vars(char * const envp[])
{
    char * const * ep;
    bool unknown_user = false;
    debug_decl(init_vars, SUDOERS_DEBUG_PLUGIN);

    if (!sudoers_initlocale(setlocale(LC_ALL, NULL), def_sudoers_locale)) {
	sudo_warnx(U_("%s: %s"), __func__, U_("unable to allocate memory"));
	debug_return_bool(false);
    }

#define MATCHES(s, v)	\
    (strncmp((s), (v), sizeof(v) - 1) == 0 && (s)[sizeof(v) - 1] != '\0')

    for (ep = envp; *ep; ep++) {
	switch (**ep) {
	    case 'K':
		if (MATCHES(*ep, "KRB5CCNAME="))
		    user_ccname = *ep + sizeof("KRB5CCNAME=") - 1;
		break;
	    case 'P':
		if (MATCHES(*ep, "PATH="))
		    user_path = *ep + sizeof("PATH=") - 1;
		break;
	    case 'S':
		if (MATCHES(*ep, "SUDO_PROMPT=")) {
		    /* Don't override "sudo -p prompt" */
		    if (user_prompt == NULL)
			user_prompt = *ep + sizeof("SUDO_PROMPT=") - 1;
		    break;
		}
		if (MATCHES(*ep, "SUDO_USER="))
		    prev_user = *ep + sizeof("SUDO_USER=") - 1;
		break;
	    }
    }
#undef MATCHES

    /*
     * Get a local copy of the user's passwd struct and group list if we
     * don't already have them.
     */
    if (sudo_user.pw == NULL) {
	if ((sudo_user.pw = sudo_getpwnam(user_name)) == NULL) {
	    /*
	     * It is not unusual for users to place "sudo -k" in a .logout
	     * file which can cause sudo to be run during reboot after the
	     * YP/NIS/NIS+/LDAP/etc daemon has died.
	     */
	    if (sudo_mode == MODE_KILL || sudo_mode == MODE_INVALIDATE) {
		sudo_warnx(U_("unknown uid %u"), (unsigned int) user_uid);
		debug_return_bool(false);
	    }

	    /* Need to make a fake struct passwd for the call to log_warningx(). */
	    sudo_user.pw = sudo_mkpwent(user_name, user_uid, user_gid, NULL, NULL);
	    unknown_user = true;
	}
    }
    if (user_gid_list == NULL)
	user_gid_list = sudo_get_gidlist(sudo_user.pw, ENTRY_TYPE_ANY);

    /* Store initialize permissions so we can restore them later. */
    if (!set_perms(PERM_INITIAL))
	debug_return_bool(false);

    /* Set parse callbacks */
    set_callbacks();

    /* It is now safe to use log_warningx() and set_perms() */
    if (unknown_user) {
	log_warningx(SLOG_SEND_MAIL, N_("unknown uid %u"),
	    (unsigned int) user_uid);
	debug_return_bool(false);
    }

    /*
     * Set runas passwd/group entries based on command line or sudoers.
     * Note that if runas_group was specified without runas_user we
     * run the command as the invoking user.
     */
    if (sudo_user.runas_group != NULL) {
	if (!set_runasgr(sudo_user.runas_group, false))
	    debug_return_bool(false);
	if (!set_runaspw(sudo_user.runas_user ?
		sudo_user.runas_user : user_name, false))
	    debug_return_bool(false);
    } else {
	if (!set_runaspw(sudo_user.runas_user ?
		sudo_user.runas_user : def_runas_default, false))
	    debug_return_bool(false);
    }

    debug_return_bool(true);
}

/*
 * Fill in user_cmnd and user_stat variables.
 * Does not fill in user_base.
 */
int
set_cmnd_path(const char *runchroot)
{
    const char *cmnd_in;
    char *cmnd_out = NULL;
    char *path = user_path;
    int ret;
    debug_decl(set_cmnd_path, SUDOERS_DEBUG_PLUGIN);

    cmnd_in = ISSET(sudo_mode, MODE_CHECK) ? NewArgv[1] : NewArgv[0];

    free(list_cmnd);
    list_cmnd = NULL;
    free(user_cmnd);
    user_cmnd = NULL;
    if (def_secure_path && !user_is_exempt())
	path = def_secure_path;
    if (!set_perms(PERM_RUNAS))
	goto error;
    ret = find_path(cmnd_in, &cmnd_out, user_stat, path,
	runchroot, def_ignore_dot, NULL);
    if (!restore_perms())
	goto error;
    if (ret == NOT_FOUND) {
	/* Failed as root, try as invoking user. */
	if (!set_perms(PERM_USER))
	    goto error;
	ret = find_path(cmnd_in, &cmnd_out, user_stat, path,
	    runchroot, def_ignore_dot, NULL);
	if (!restore_perms())
	    goto error;
    }

    if (ISSET(sudo_mode, MODE_CHECK))
	list_cmnd = cmnd_out;
    else
	user_cmnd = cmnd_out;

    debug_return_int(ret);
error:
    free(cmnd_out);
    debug_return_int(NOT_FOUND_ERROR);
}

/*
 * Fill in user_cmnd, user_args, user_base and user_stat variables
 * and apply any command-specific defaults entries.
 */
static int
set_cmnd(void)
{
    struct sudo_nss *nss;
    int ret = FOUND;
    debug_decl(set_cmnd, SUDOERS_DEBUG_PLUGIN);

    /* Allocate user_stat for find_path() and match functions. */
    free(user_stat);
    user_stat = calloc(1, sizeof(struct stat));
    if (user_stat == NULL) {
	sudo_warnx(U_("%s: %s"), __func__, U_("unable to allocate memory"));
	debug_return_int(NOT_FOUND_ERROR);
    }

    /* Re-initialize for when we are called multiple times. */
    free(safe_cmnd);
    safe_cmnd = NULL;

    if (ISSET(sudo_mode, MODE_RUN|MODE_EDIT|MODE_CHECK)) {
	if (!ISSET(sudo_mode, MODE_EDIT)) {
	    const char *runchroot = user_runchroot;
	    if (runchroot == NULL && def_runchroot != NULL &&
		    strcmp(def_runchroot, "*") != 0)
		runchroot = def_runchroot;

	    ret = set_cmnd_path(runchroot);
	    if (ret == NOT_FOUND_ERROR) {
		if (errno == ENAMETOOLONG) {
		    audit_failure(NewArgv, N_("command too long"));
		}
		log_warning(0, "%s", NewArgv[0]);
		debug_return_int(ret);
	    }
	}

	/* set user_args */
	free(user_args);
	user_args = NULL;
	if (NewArgc > 1) {
	    if (ISSET(sudo_mode, MODE_SHELL|MODE_LOGIN_SHELL) &&
		    ISSET(sudo_mode, MODE_RUN)) {
		/*
		 * When running a command via a shell, the sudo front-end
		 * escapes potential meta chars.  We unescape non-spaces
		 * for sudoers matching and logging purposes.
		 * TODO: move escaping to the policy plugin instead
		 */
		user_args = strvec_join(NewArgv + 1, ' ', strlcpy_unescape);
	    } else {
		user_args = strvec_join(NewArgv + 1, ' ', NULL);
	    }
	    if (user_args == NULL)
		debug_return_int(NOT_FOUND_ERROR);
	}
    }
    if (user_cmnd == NULL) {
	user_cmnd = strdup(NewArgv[0]);
	if (user_cmnd == NULL)
	    debug_return_int(NOT_FOUND_ERROR);
    }
    user_base = sudo_basename(user_cmnd);

    /* Convert "sudo sudoedit" -> "sudoedit" */
    if (ISSET(sudo_mode, MODE_RUN) && strcmp(user_base, "sudoedit") == 0) {
	char *new_cmnd;

	CLR(sudo_mode, MODE_RUN);
	SET(sudo_mode, MODE_EDIT);
	sudo_warnx("%s", U_("sudoedit doesn't need to be run via sudo"));
	if ((new_cmnd = strdup("sudoedit")) == NULL) {
	    sudo_warnx(U_("%s: %s"), __func__, U_("unable to allocate memory"));
	    debug_return_int(NOT_FOUND_ERROR);
	}
	free(user_cmnd);
	user_base = user_cmnd = new_cmnd;
    }

    TAILQ_FOREACH(nss, snl, entries) {
	/* Missing/invalid defaults is not a fatal error. */
	(void)update_defaults(nss->parse_tree, NULL, SETDEF_CMND, false);
    }

    debug_return_int(ret);
}

/*
 * Open sudoers file and check mode/owner/type.
 * Returns a handle to the sudoers file or NULL on error.
 */
FILE *
open_sudoers(const char *file, bool doedit, bool *keepopen)
{
    FILE *fp = NULL;
    struct stat sb;
    int error, fd;
    debug_decl(open_sudoers, SUDOERS_DEBUG_PLUGIN);

    if (!set_perms(PERM_SUDOERS))
	debug_return_ptr(NULL);

again:
    fd = sudo_secure_open_file(file, sudoers_uid, sudoers_gid, &sb, &error);
    if (fd != -1) {
	/*
	 * Make sure we can read the file so we can present the
	 * user with a reasonable error message (unlike the lexer).
	 */
	if ((fp = fdopen(fd, "r")) == NULL) {
	    log_warning(SLOG_SEND_MAIL, N_("unable to open %s"), file);
	    close(fd);
	} else {
	    if (sb.st_size != 0 && fgetc(fp) == EOF) {
		log_warning(SLOG_SEND_MAIL,
		    N_("unable to read %s"), file);
		fclose(fp);
		fp = NULL;
	    } else {
		/* Rewind fp and set close on exec flag. */
		rewind(fp);
		(void) fcntl(fileno(fp), F_SETFD, 1);
	    }
	}
    } else {
	switch (error) {
	case SUDO_PATH_MISSING:
	    /*
	     * If we tried to open sudoers as non-root but got EACCES,
	     * try again as root.
	     */
	    if (errno == EACCES && geteuid() != ROOT_UID) {
		int serrno = errno;
		if (restore_perms()) {
		    if (!set_perms(PERM_ROOT))
			debug_return_ptr(NULL);
		    goto again;
		}
		errno = serrno;
	    }
	    log_warning(SLOG_SEND_MAIL, N_("unable to open %s"), file);
	    break;
	case SUDO_PATH_BAD_TYPE:
	    log_warningx(SLOG_SEND_MAIL,
		N_("%s is not a regular file"), file);
	    break;
	case SUDO_PATH_WRONG_OWNER:
	    log_warningx(SLOG_SEND_MAIL,
		N_("%s is owned by uid %u, should be %u"), file,
		(unsigned int) sb.st_uid, (unsigned int) sudoers_uid);
	    break;
	case SUDO_PATH_WORLD_WRITABLE:
	    log_warningx(SLOG_SEND_MAIL, N_("%s is world writable"), file);
	    break;
	case SUDO_PATH_GROUP_WRITABLE:
	    log_warningx(SLOG_SEND_MAIL,
		N_("%s is owned by gid %u, should be %u"), file,
		(unsigned int) sb.st_gid, (unsigned int) sudoers_gid);
	    break;
	default:
	    sudo_warnx("%s: internal error, unexpected error %d",
		__func__, error);
	    break;
	}
    }

    if (!restore_perms()) {
	/* unable to change back to root */
	if (fp != NULL) {
	    fclose(fp);
	    fp = NULL;
	}
    }

    debug_return_ptr(fp);
}

#ifdef HAVE_LOGIN_CAP_H
static bool
set_loginclass(struct passwd *pw)
{
    const int errflags = SLOG_RAW_MSG;
    login_cap_t *lc;
    bool ret = true;
    debug_decl(set_loginclass, SUDOERS_DEBUG_PLUGIN);

    if (!def_use_loginclass)
	goto done;

    if (login_class && strcmp(login_class, "-") != 0) {
	if (user_uid != 0 && pw->pw_uid != 0) {
	    sudo_warnx(U_("only root can use \"-c %s\""), login_class);
	    ret = false;
	    goto done;
	}
    } else {
	login_class = pw->pw_class;
	if (!login_class || !*login_class)
	    login_class = (char *)
		((pw->pw_uid == 0) ? LOGIN_DEFROOTCLASS : LOGIN_DEFCLASS);
    }

    /* Make sure specified login class is valid. */
    lc = login_getclass(login_class);
    if (!lc || !lc->lc_class || strcmp(lc->lc_class, login_class) != 0) {
	/*
	 * Don't make it an error if the user didn't specify the login
	 * class themselves.  We do this because if login.conf gets
	 * corrupted we want the admin to be able to use sudo to fix it.
	 */
	log_warningx(errflags, N_("unknown login class %s"), login_class);
	def_use_loginclass = false;
	if (login_class)
	    ret = false;
    }
    login_close(lc);
done:
    debug_return_bool(ret);
}
#else
static bool
set_loginclass(struct passwd *pw)
{
    return true;
}
#endif /* HAVE_LOGIN_CAP_H */

#ifndef AI_FQDN
# define AI_FQDN AI_CANONNAME
#endif

/*
 * Look up the fully qualified domain name of host.
 * Use AI_FQDN if available since "canonical" is not always the same as fqdn.
 * Returns 0 on success, setting longp and shortp.
 * Returns non-zero on failure, longp and shortp are unchanged.
 * See gai_strerror() for the list of error return codes.
 */
static int
resolve_host(const char *host, char **longp, char **shortp)
{
    struct addrinfo *res0, hint;
    char *cp, *lname, *sname;
    int ret;
    debug_decl(resolve_host, SUDOERS_DEBUG_PLUGIN);

    memset(&hint, 0, sizeof(hint));
    hint.ai_family = PF_UNSPEC;
    hint.ai_flags = AI_FQDN;

    if ((ret = getaddrinfo(host, NULL, &hint, &res0)) != 0)
	debug_return_int(ret);
    if ((lname = strdup(res0->ai_canonname)) == NULL) {
	freeaddrinfo(res0);
	debug_return_int(EAI_MEMORY);
    }
    if ((cp = strchr(lname, '.')) != NULL) {
	sname = strndup(lname, (size_t)(cp - lname));
	if (sname == NULL) {
	    free(lname);
	    freeaddrinfo(res0);
	    debug_return_int(EAI_MEMORY);
	}
    } else {
	sname = lname;
    }
    freeaddrinfo(res0);
    *longp = lname;
    *shortp = sname;

    debug_return_int(0);
}

/*
 * Look up the fully qualified domain name of user_host and user_runhost.
 * Sets user_host, user_shost, user_runhost and user_srunhost.
 */
static bool
cb_fqdn(const char *file, int line, int column,
    const union sudo_defs_val *sd_un, int op)
{
    bool remote;
    int rc;
    char *lhost, *shost;
    debug_decl(cb_fqdn, SUDOERS_DEBUG_PLUGIN);

    /* Nothing to do if fqdn flag is disabled. */
    if (sd_un != NULL && !sd_un->flag)
	debug_return_bool(true);

    /* If the -h flag was given we need to resolve both host and runhost. */
    remote = strcmp(user_runhost, user_host) != 0;

    /* First resolve user_host, setting user_host and user_shost. */
    if (resolve_host(user_host, &lhost, &shost) != 0) {
	if ((rc = resolve_host(user_runhost, &lhost, &shost)) != 0) {
	    gai_log_warning(SLOG_SEND_MAIL|SLOG_RAW_MSG, rc,
		N_("unable to resolve host %s"), user_host);
	    debug_return_bool(false);
	}
    }
    if (user_shost != user_host)
	free(user_shost);
    free(user_host);
    user_host = lhost;
    user_shost = shost;

    /* Next resolve user_runhost, setting user_runhost and user_srunhost. */
    lhost = shost = NULL;
    if (remote) {
	if ((rc = resolve_host(user_runhost, &lhost, &shost)) != 0) {
	    gai_log_warning(SLOG_NO_LOG|SLOG_RAW_MSG, rc,
		N_("unable to resolve host %s"), user_runhost);
	    debug_return_bool(false);
	}
    } else {
	/* Not remote, just use user_host. */
	if ((lhost = strdup(user_host)) != NULL) {
	    if (user_shost != user_host)
		shost = strdup(user_shost);
	    else
		shost = lhost;
	}
	if (lhost == NULL || shost == NULL) {
	    free(lhost);
	    if (lhost != shost)
		free(shost);
	    sudo_warnx(U_("%s: %s"), __func__, U_("unable to allocate memory"));
	    debug_return_bool(false);
	}
    }
    if (lhost != NULL && shost != NULL) {
	if (user_srunhost != user_runhost)
	    free(user_srunhost);
	free(user_runhost);
	user_runhost = lhost;
	user_srunhost = shost;
    }

    sudo_debug_printf(SUDO_DEBUG_INFO|SUDO_DEBUG_LINENO,
	"host %s, shost %s, runhost %s, srunhost %s",
	user_host, user_shost, user_runhost, user_srunhost);
    debug_return_bool(true);
}

/*
 * Get passwd entry for the user we are going to run commands as
 * and store it in runas_pw.  By default, commands run as "root".
 */
static bool
set_runaspw(const char *user, bool quiet)
{
    struct passwd *pw = NULL;
    debug_decl(set_runaspw, SUDOERS_DEBUG_PLUGIN);

    unknown_runas_uid = false;
    if (*user == '#') {
	const char *errstr;
	uid_t uid = sudo_strtoid(user + 1, &errstr);
	if (errstr == NULL) {
	    if ((pw = sudo_getpwuid(uid)) == NULL) {
		unknown_runas_uid = true;
		pw = sudo_fakepwnam(user, user_gid);
	    }
	}
    }
    if (pw == NULL) {
	if ((pw = sudo_getpwnam(user)) == NULL) {
	    if (!quiet)
		log_warningx(SLOG_AUDIT, N_("unknown user %s"), user);
	    debug_return_bool(false);
	}
    }
    if (runas_pw != NULL)
	sudo_pw_delref(runas_pw);
    runas_pw = pw;
    debug_return_bool(true);
}

/*
 * Get group entry for the group we are going to run commands as
 * and store it in runas_gr.
 */
static bool
set_runasgr(const char *group, bool quiet)
{
    struct group *gr = NULL;
    debug_decl(set_runasgr, SUDOERS_DEBUG_PLUGIN);

    unknown_runas_gid = false;
    if (*group == '#') {
	const char *errstr;
	gid_t gid = sudo_strtoid(group + 1, &errstr);
	if (errstr == NULL) {
	    if ((gr = sudo_getgrgid(gid)) == NULL) {
		unknown_runas_gid = true;
		gr = sudo_fakegrnam(group);
	    }
	}
    }
    if (gr == NULL) {
	if ((gr = sudo_getgrnam(group)) == NULL) {
	    if (!quiet)
		log_warningx(SLOG_AUDIT, N_("unknown group %s"), group);
	    debug_return_bool(false);
	}
    }
    if (runas_gr != NULL)
	sudo_gr_delref(runas_gr);
    runas_gr = gr;
    debug_return_bool(true);
}

/*
 * Callback for runas_default sudoers setting.
 */
static bool
cb_runas_default(const char *file, int line, int column,
    const union sudo_defs_val *sd_un, int op)
{
    debug_decl(cb_runas_default, SUDOERS_DEBUG_PLUGIN);

    /* Only reset runaspw if user didn't specify one. */
    if (sudo_user.runas_user == NULL && sudo_user.runas_group == NULL)
	debug_return_bool(set_runaspw(sd_un->str, true));
    debug_return_bool(true);
}

/*
 * Callback for tty_tickets sudoers setting.
 */
static bool
cb_tty_tickets(const char *file, int line, int column,
    const union sudo_defs_val *sd_un, int op)
{
    debug_decl(cb_tty_tickets, SUDOERS_DEBUG_PLUGIN);

    /* Convert tty_tickets -> timestamp_type */
    if (sd_un->flag)
	def_timestamp_type = tty;
    else
	def_timestamp_type = global;
    debug_return_bool(true);
}

/*
 * Callback for umask sudoers setting.
 */
static bool
cb_umask(const char *file, int line, int column,
    const union sudo_defs_val *sd_un, int op)
{
    debug_decl(cb_umask, SUDOERS_DEBUG_PLUGIN);

    /* Force umask if explicitly set in sudoers. */
    force_umask = sd_un->mode != ACCESSPERMS;

    debug_return_bool(true);
}

/*
 * Callback for runchroot sudoers setting.
 */
static bool
cb_runchroot(const char *file, int line, int column,
    const union sudo_defs_val *sd_un, int op)
{
    debug_decl(cb_runchroot, SUDOERS_DEBUG_PLUGIN);

    sudo_debug_printf(SUDO_DEBUG_INFO|SUDO_DEBUG_LINENO,
	"def_runchroot now %s", sd_un->str);
    if (user_cmnd != NULL) {
	/* Update user_cmnd based on the new chroot. */
	cmnd_status = set_cmnd_path(sd_un->str);
	sudo_debug_printf(SUDO_DEBUG_INFO|SUDO_DEBUG_LINENO,
	    "user_cmnd now %s", user_cmnd);
    }

    debug_return_bool(true);
}

static bool
cb_logfile(const char *file, int line, int column,
    const union sudo_defs_val *sd_un, int op)
{
    int logtype = def_syslog ? EVLOG_SYSLOG : EVLOG_NONE;
    debug_decl(cb_logfile, SUDOERS_DEBUG_PLUGIN);

    if (sd_un->str != NULL)
	SET(logtype, EVLOG_FILE);
    eventlog_set_type(logtype);
    eventlog_set_logpath(sd_un->str);

    debug_return_bool(true);
}

static bool
cb_log_format(const char *file, int line, int column,
    const union sudo_defs_val *sd_un, int op)
{
    debug_decl(cb_log_format, SUDOERS_DEBUG_PLUGIN);

    eventlog_set_format(sd_un->tuple == sudo ? EVLOG_SUDO : EVLOG_JSON);

    debug_return_bool(true);
}

static bool
cb_syslog(const char *file, int line, int column,
    const union sudo_defs_val *sd_un, int op)
{
    int logtype = def_logfile ? EVLOG_FILE : EVLOG_NONE;
    debug_decl(cb_syslog, SUDOERS_DEBUG_PLUGIN);

    if (sd_un->str != NULL)
	SET(logtype, EVLOG_SYSLOG);
    eventlog_set_type(logtype);

    debug_return_bool(true);
}

static bool
cb_syslog_goodpri(const char *file, int line, int column,
    const union sudo_defs_val *sd_un, int op)
{
    debug_decl(cb_syslog_goodpri, SUDOERS_DEBUG_PLUGIN);

    eventlog_set_syslog_acceptpri(sd_un->ival);

    debug_return_bool(true);
}

static bool
cb_syslog_badpri(const char *file, int line, int column,
    const union sudo_defs_val *sd_un, int op)
{
    debug_decl(cb_syslog_badpri, SUDOERS_DEBUG_PLUGIN);

    eventlog_set_syslog_rejectpri(sd_un->ival);
    eventlog_set_syslog_alertpri(sd_un->ival);

    debug_return_bool(true);
}

static bool
cb_syslog_maxlen(const char *file, int line, int column,
    const union sudo_defs_val *sd_un, int op)
{
    debug_decl(cb_syslog_maxlen, SUDOERS_DEBUG_PLUGIN);

    eventlog_set_syslog_maxlen(sd_un->ival);

    debug_return_bool(true);
}

static bool
cb_loglinelen(const char *file, int line, int column,
    const union sudo_defs_val *sd_un, int op)
{
    debug_decl(cb_loglinelen, SUDOERS_DEBUG_PLUGIN);

    eventlog_set_file_maxlen(sd_un->ival);

    debug_return_bool(true);
}

static bool
cb_log_year(const char *file, int line, int column,
    const union sudo_defs_val *sd_un, int op)
{
    debug_decl(cb_syslog_maxlen, SUDOERS_DEBUG_PLUGIN);

    eventlog_set_time_fmt(sd_un->flag ? "%h %e %T %Y" : "%h %e %T");

    debug_return_bool(true);
}

static bool
cb_log_host(const char *file, int line, int column,
    const union sudo_defs_val *sd_un, int op)
{
    debug_decl(cb_syslog_maxlen, SUDOERS_DEBUG_PLUGIN);

    eventlog_set_omit_hostname(!sd_un->flag);

    debug_return_bool(true);
}

static bool
cb_mailerpath(const char *file, int line, int column,
    const union sudo_defs_val *sd_un, int op)
{
    debug_decl(cb_mailerpath, SUDOERS_DEBUG_PLUGIN);

    eventlog_set_mailerpath(sd_un->str);

    debug_return_bool(true);
}

static bool
cb_mailerflags(const char *file, int line, int column,
    const union sudo_defs_val *sd_un, int op)
{
    debug_decl(cb_mailerflags, SUDOERS_DEBUG_PLUGIN);

    eventlog_set_mailerflags(sd_un->str);

    debug_return_bool(true);
}

static bool
cb_mailfrom(const char *file, int line, int column,
    const union sudo_defs_val *sd_un, int op)
{
    debug_decl(cb_mailfrom, SUDOERS_DEBUG_PLUGIN);

    eventlog_set_mailfrom(sd_un->str);

    debug_return_bool(true);
}

static bool
cb_mailto(const char *file, int line, int column,
    const union sudo_defs_val *sd_un, int op)
{
    debug_decl(cb_mailto, SUDOERS_DEBUG_PLUGIN);

    eventlog_set_mailto(sd_un->str);

    debug_return_bool(true);
}

static bool
cb_mailsub(const char *file, int line, int column,
    const union sudo_defs_val *sd_un, int op)
{
    debug_decl(cb_mailsub, SUDOERS_DEBUG_PLUGIN);

    eventlog_set_mailsub(sd_un->str);

    debug_return_bool(true);
}

static bool
cb_intercept_type(const char *file, int line, int column,
    const union sudo_defs_val *sd_un, int op)
{
    debug_decl(cb_intercept_type, SUDOERS_DEBUG_PLUGIN);

    if (op != -1) {
	/* Set explicitly in sudoers. */
	if (sd_un->tuple == dso) {
	    /* Reset intercept_allow_setid default value. */
	    if (!ISSET(sudo_user.flags, USER_INTERCEPT_SETID))
		def_intercept_allow_setid = false;
	}
    }

    debug_return_bool(true);
}

static bool
cb_intercept_allow_setid(const char *file, int line, int column,
    const union sudo_defs_val *sd_un, int op)
{
    debug_decl(cb_intercept_allow_setid, SUDOERS_DEBUG_PLUGIN);

    /* Operator will be -1 if set by front-end. */
    if (op != -1) {
	/* Set explicitly in sudoers. */
	SET(sudo_user.flags, USER_INTERCEPT_SETID);
    }

    debug_return_bool(true);
}

bool
cb_log_input(const char *file, int line, int column,
    const union sudo_defs_val *sd_un, int op)
{
    debug_decl(cb_log_input, SUDOERS_DEBUG_PLUGIN);

    def_log_stdin = op;
    def_log_ttyin = op;

    debug_return_bool(true);
}

bool
cb_log_output(const char *file, int line, int column,
    const union sudo_defs_val *sd_un, int op)
{
    debug_decl(cb_log_output, SUDOERS_DEBUG_PLUGIN);

    def_log_stdout = op;
    def_log_stderr = op;
    def_log_ttyout = op;

    debug_return_bool(true);
}

/*
 * Set parse Defaults callbacks.
 * We do this here instead in def_data.in so we don't have to
 * stub out the callbacks for visudo and testsudoers.
 */
static void
set_callbacks(void)
{
    debug_decl(set_callbacks, SUDOERS_DEBUG_PLUGIN);

    /* Set fqdn callback. */
    sudo_defs_table[I_FQDN].callback = cb_fqdn;

    /* Set group_plugin callback. */
    sudo_defs_table[I_GROUP_PLUGIN].callback = cb_group_plugin;

    /* Set runas callback. */
    sudo_defs_table[I_RUNAS_DEFAULT].callback = cb_runas_default;

    /* Set locale callback. */
    sudo_defs_table[I_SUDOERS_LOCALE].callback = sudoers_locale_callback;

    /* Set maxseq callback. */
    sudo_defs_table[I_MAXSEQ].callback = cb_maxseq;

    /* Set iolog_user callback. */
    sudo_defs_table[I_IOLOG_USER].callback = cb_iolog_user;

    /* Set iolog_group callback. */
    sudo_defs_table[I_IOLOG_GROUP].callback = cb_iolog_group;

    /* Set iolog_mode callback. */
    sudo_defs_table[I_IOLOG_MODE].callback = cb_iolog_mode;

    /* Set tty_tickets callback. */
    sudo_defs_table[I_TTY_TICKETS].callback = cb_tty_tickets;

    /* Set umask callback. */
    sudo_defs_table[I_UMASK].callback = cb_umask;

    /* Set runchroot callback. */
    sudo_defs_table[I_RUNCHROOT].callback = cb_runchroot;

    /* eventlog callbacks */
    sudo_defs_table[I_SYSLOG].callback = cb_syslog;
    sudo_defs_table[I_SYSLOG_GOODPRI].callback = cb_syslog_goodpri;
    sudo_defs_table[I_SYSLOG_BADPRI].callback = cb_syslog_badpri;
    sudo_defs_table[I_SYSLOG_MAXLEN].callback = cb_syslog_maxlen;
    sudo_defs_table[I_LOGLINELEN].callback = cb_loglinelen;
    sudo_defs_table[I_LOG_HOST].callback = cb_log_host;
    sudo_defs_table[I_LOGFILE].callback = cb_logfile;
    sudo_defs_table[I_LOG_FORMAT].callback = cb_log_format;
    sudo_defs_table[I_LOG_YEAR].callback = cb_log_year;
    sudo_defs_table[I_MAILERPATH].callback = cb_mailerpath;
    sudo_defs_table[I_MAILERFLAGS].callback = cb_mailerflags;
    sudo_defs_table[I_MAILFROM].callback = cb_mailfrom;
    sudo_defs_table[I_MAILTO].callback = cb_mailto;
    sudo_defs_table[I_MAILSUB].callback = cb_mailsub;
    sudo_defs_table[I_PASSPROMPT_REGEX].callback = cb_passprompt_regex;
    sudo_defs_table[I_INTERCEPT_TYPE].callback = cb_intercept_type;
    sudo_defs_table[I_INTERCEPT_ALLOW_SETID].callback = cb_intercept_allow_setid;
    sudo_defs_table[I_LOG_INPUT].callback = cb_log_input;
    sudo_defs_table[I_LOG_OUTPUT].callback = cb_log_output;

    debug_return;
}

/*
 * Cleanup hook for sudo_fatal()/sudo_fatalx()
 * Also called at policy close time.
 */
void
sudoers_cleanup(void)
{
    struct sudo_nss *nss;
    struct defaults *def;
    debug_decl(sudoers_cleanup, SUDOERS_DEBUG_PLUGIN);

    if (snl != NULL) {
	TAILQ_FOREACH(nss, snl, entries) {
	    nss->close(nss);
	}
	snl = NULL;
	init_parser(NULL, false, false);
    }
    while ((def = TAILQ_FIRST(&initial_defaults)) != NULL) {
	TAILQ_REMOVE(&initial_defaults, def, entries);
	free(def->var);
	free(def->val);
	free(def);
    }
    need_reinit = false;
    if (def_group_plugin)
	group_plugin_unload();
    sudo_user_free();
    sudo_freepwcache();
    sudo_freegrcache();

    /* Clear globals */
    list_pw = NULL;
    saved_argv = NULL;
    NewArgv = NULL;
    NewArgc = 0;
    prev_user = NULL;

    debug_return;
}

static bool
tty_present(void)
{
    debug_decl(tty_present, SUDOERS_DEBUG_PLUGIN);
    
    if (user_tcpgid == 0 && user_ttypath == NULL) {
	/* No job control or terminal, check /dev/tty. */
	int fd = open(_PATH_TTY, O_RDWR);
	if (fd == -1)
	    debug_return_bool(false);
	close(fd);
    }
    debug_return_bool(true);
}

/*
 * Free memory allocated for struct sudo_user.
 */
void
sudo_user_free(void)
{
    debug_decl(sudo_user_free, SUDOERS_DEBUG_PLUGIN);

    /* Free remaining references to password and group entries. */
    if (sudo_user.pw != NULL)
	sudo_pw_delref(sudo_user.pw);
    if (runas_pw != NULL)
	sudo_pw_delref(runas_pw);
    if (runas_gr != NULL)
	sudo_gr_delref(runas_gr);
    if (user_gid_list != NULL)
	sudo_gidlist_delref(user_gid_list);

    /* Free dynamic contents of sudo_user. */
    free(user_cwd);
    free(user_name);
    free(user_gids);
    if (user_ttypath != NULL)
	free(user_ttypath);
    else
	free(user_tty);
    if (user_shost != user_host)
	    free(user_shost);
    free(user_host);
    if (user_srunhost != user_runhost)
	    free(user_srunhost);
    free(user_runhost);
    free(user_cmnd);
    free(user_args);
    free(list_cmnd);
    free(safe_cmnd);
    free(saved_cmnd);
    free(user_stat);
#ifdef HAVE_SELINUX
    free(user_role);
    free(user_type);
#endif
#ifdef HAVE_APPARMOR
    free(user_apparmor_profile);
#endif
#ifdef HAVE_PRIV_SET
    free(runas_privs);
    free(runas_limitprivs);
#endif
    memset(&sudo_user, 0, sizeof(sudo_user));

    debug_return;
}
