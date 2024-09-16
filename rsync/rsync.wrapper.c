/* START OF SPEC @generated CONTENTS */

/* General settings */
#define WRAPPER_APPLICATION_COUNT 2
#define WRAPPER_ENV_VAR "CHOSEN_RSYNC"
#define WRAPPER_ANALYTICS_IDENT "com.apple.rsync"
#define WRAPPER_ANALYTICS_NOARGS true

/* END OF SPEC @generated CONTENTS */

/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * NOTE: The contents of this file are constructed by genwrap(8); it should not
 * be manually modified.
 */

#include <sys/param.h>

#include <assert.h>
#include <err.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xpc/xpc.h>

#if defined(WRAPPER_ANALYTICS_IDENT) && !defined(WRAPPER_ANALYTICS_TESTING)
#include <CoreAnalytics/CoreAnalytics.h>
#endif
#ifdef WRAPPER_NEEDS_XCSELECT
#include <xcselect_private.h>
#endif

#ifndef nitems
#define	nitems(x)	(sizeof((x)) / sizeof((x)[0]))
#endif

#if !defined(WRAPPER_ANALYTICS_IDENT) && defined(WRAPPER_ANALYTICS_TESTING)
#error shim was improperly modified to remove the analytics identifier
#endif

/*
 * The wrapper generator will provide an array of struct application that we
 * will sift through to determine which to choose.  We'll default to the first
 * element of the array, but may choose to use one of the others depending on
 * arguments used or if the environment is set to force one.
 */
struct application {
	const char		*app_name;
	const char		*app_path;

	/*
	 * Additional arguments that the wrapper should insert when this
	 * application is selected.
	 */
	const char		 **app_add_argv;
	int			 app_add_nargv;

	/*
	 * If optstr is set, we'll use getopt(3) or getopt_long(3) to determine
	 * if we can use this application as the backend.
	 */
	const char		*app_optstr;
	const struct option	*app_longopts;

	size_t			 app_nlogonly;
	const bool		*app_logonly_opts;

	/*
	 * Relative paths are relative to cwd, rather than to the selected
	 * developer tools.
	 */
	bool			 app_path_relcwd;

	/*
	 * Options are specified for logging purposes only, not to be considered
	 * for selecting an application to execute.
	 */
	bool			 app_opts_logonly;
};

/* START OF SPEC @generated CONTENTS */

/* Long and Logonly Option Definitions */
static const struct option rsync_openrsync_longopts[] = {
	{"address", required_argument, NULL, CHAR_MAX + 0 },
	{"archive", no_argument, NULL, 'a' },
	{"compare-dest", required_argument, NULL, CHAR_MAX + 1 },
	{"link-dest", required_argument, NULL, CHAR_MAX + 2 },
	{"copy-dirlinks", no_argument, NULL, 'k' },
	{"copy-links", no_argument, NULL, 'L' },
	{"no-D", no_argument, NULL, CHAR_MAX + 3 },
	{"del", no_argument, NULL, CHAR_MAX + 4 },
	{"delete", no_argument, NULL, CHAR_MAX + 5 },
	{"delete-before", no_argument, NULL, CHAR_MAX + 6 },
	{"delete-during", no_argument, NULL, CHAR_MAX + 7 },
	{"delete-delay", no_argument, NULL, CHAR_MAX + 8 },
	{"delete-excluded", no_argument, NULL, CHAR_MAX + 9 },
	{"devices", no_argument, NULL, CHAR_MAX + 10 },
	{"no-devices", no_argument, NULL, CHAR_MAX + 11 },
	{"dry-run", no_argument, NULL, 'n' },
	{"exclude", required_argument, NULL, CHAR_MAX + 12 },
	{"exclude-from", required_argument, NULL, CHAR_MAX + 13 },
	{"existing", no_argument, NULL, CHAR_MAX + 14 },
	{"group", no_argument, NULL, 'g' },
	{"no-group", no_argument, NULL, CHAR_MAX + 15 },
	{"no-g", no_argument, NULL, CHAR_MAX + 16 },
	{"hard-links", no_argument, NULL, 'H' },
	{"help", no_argument, NULL, 'h' },
	{"ignore-existing", no_argument, NULL, CHAR_MAX + 17 },
	{"ignore-non-existing", no_argument, NULL, CHAR_MAX + 18 },
	{"ignore-times", no_argument, NULL, 'I' },
	{"include", required_argument, NULL, CHAR_MAX + 19 },
	{"include-from", required_argument, NULL, CHAR_MAX + 20 },
	{"links", no_argument, NULL, 'l' },
	{"max-size", required_argument, NULL, CHAR_MAX + 21 },
	{"min-size", required_argument, NULL, CHAR_MAX + 22 },
	{"no-links", no_argument, NULL, CHAR_MAX + 23 },
	{"no-l", no_argument, NULL, CHAR_MAX + 24 },
	{"no-motd", no_argument, NULL, CHAR_MAX + 25 },
	{"numeric-ids", no_argument, NULL, CHAR_MAX + 26 },
	{"owner", no_argument, NULL, 'o' },
	{"no-owner", no_argument, NULL, CHAR_MAX + 27 },
	{"no-o", no_argument, NULL, CHAR_MAX + 28 },
	{"perms", no_argument, NULL, 'p' },
	{"no-perms", no_argument, NULL, CHAR_MAX + 29 },
	{"no-p", no_argument, NULL, CHAR_MAX + 30 },
	{"port", required_argument, NULL, CHAR_MAX + 31 },
	{"recursive", no_argument, NULL, 'r' },
	{"no-recursive", no_argument, NULL, CHAR_MAX + 32 },
	{"no-r", no_argument, NULL, CHAR_MAX + 33 },
	{"one-file-system", no_argument, NULL, 'x' },
	{"rsh", required_argument, NULL, 'e' },
	{"rsync-path", required_argument, NULL, CHAR_MAX + 34 },
	{"sender", no_argument, NULL, CHAR_MAX + 35 },
	{"server", no_argument, NULL, CHAR_MAX + 36 },
	{"specials", no_argument, NULL, CHAR_MAX + 37 },
	{"sparse", no_argument, NULL, 'S' },
	{"no-specials", no_argument, NULL, CHAR_MAX + 38 },
	{"timeout", required_argument, NULL, CHAR_MAX + 39 },
	{"times", no_argument, NULL, 't' },
	{"no-times", no_argument, NULL, CHAR_MAX + 40 },
	{"no-t", no_argument, NULL, CHAR_MAX + 41 },
	{"verbose", no_argument, NULL, 'v' },
	{"no-verbose", no_argument, NULL, CHAR_MAX + 42 },
	{"version", no_argument, NULL, 'V' },
	{"relative", no_argument, NULL, 'R' },
	{"no-R", no_argument, NULL, CHAR_MAX + 43 },
	{"no-relative", no_argument, NULL, CHAR_MAX + 44 },
	{"dirs", no_argument, NULL, 'd' },
	{"no-dirs", no_argument, NULL, CHAR_MAX + 45 },
	{"files-from", required_argument, NULL, CHAR_MAX + 46 },
	{"delay-updates", no_argument, NULL, CHAR_MAX + 47 },
	{ NULL, 0, 0, 0 },
};
static const bool rsync_openrsync_logonly[] = {
	[CHAR_MAX + 1] = true,
	[CHAR_MAX + 12] = true,
	[CHAR_MAX + 13] = true,
	[CHAR_MAX + 19] = true,
	[CHAR_MAX + 20] = true,
	[CHAR_MAX + 46] = true,
};

static const struct option rsync_samba_longopts[] = {
	{"modify-window", required_argument, NULL, CHAR_MAX + 0 },
	{"chmod", required_argument, NULL, CHAR_MAX + 1 },
	{"max-size", required_argument, NULL, CHAR_MAX + 2 },
	{"min-size", required_argument, NULL, CHAR_MAX + 3 },
	{"max-delete", required_argument, NULL, CHAR_MAX + 4 },
	{"filter", required_argument, NULL, 'f' },
	{"exclude", required_argument, NULL, CHAR_MAX + 5 },
	{"include", required_argument, NULL, CHAR_MAX + 6 },
	{"exclude-from", required_argument, NULL, CHAR_MAX + 7 },
	{"include-from", required_argument, NULL, CHAR_MAX + 8 },
	{"block-size", required_argument, NULL, 'B' },
	{"compare-dest", required_argument, NULL, CHAR_MAX + 9 },
	{"copy-dest", required_argument, NULL, CHAR_MAX + 10 },
	{"link-dest", required_argument, NULL, CHAR_MAX + 11 },
	{"compress-level", required_argument, NULL, CHAR_MAX + 12 },
	{"partial-dir", required_argument, NULL, CHAR_MAX + 13 },
	{"log-file", required_argument, NULL, CHAR_MAX + 14 },
	{"log-file-format", required_argument, NULL, CHAR_MAX + 15 },
	{"out-format", required_argument, NULL, CHAR_MAX + 16 },
	{"log-format", required_argument, NULL, CHAR_MAX + 17 },
	{"bwlimit", required_argument, NULL, CHAR_MAX + 18 },
	{"backup-dir", required_argument, NULL, CHAR_MAX + 19 },
	{"suffix", required_argument, NULL, CHAR_MAX + 20 },
	{"read-batch", required_argument, NULL, CHAR_MAX + 21 },
	{"write-batch", required_argument, NULL, CHAR_MAX + 22 },
	{"only-write-batch", required_argument, NULL, CHAR_MAX + 23 },
	{"files-from", required_argument, NULL, CHAR_MAX + 24 },
	{"timeout", required_argument, NULL, CHAR_MAX + 25 },
	{"rsh", required_argument, NULL, 'e' },
	{"rsync-path", required_argument, NULL, CHAR_MAX + 26 },
	{"temp-dir", required_argument, NULL, 'T' },
	{"address", required_argument, NULL, CHAR_MAX + 27 },
	{"port", required_argument, NULL, CHAR_MAX + 28 },
	{"sockopts", required_argument, NULL, CHAR_MAX + 29 },
	{"password-file", required_argument, NULL, CHAR_MAX + 30 },
	{"protocol", required_argument, NULL, CHAR_MAX + 31 },
	{"checksum-seed", required_argument, NULL, CHAR_MAX + 32 },
	{"config", required_argument, NULL, CHAR_MAX + 33 },
	{ NULL, 0, 0, 0 },
};

/* Additional Arg Definitions */


/* Application Definitions */
static const struct application wrapper_apps[] = {
	{
		.app_name = "rsync_openrsync",
		.app_path = "/usr/libexec/rsync/rsync.openrsync",
		.app_path_relcwd = false,
		.app_opts_logonly = false,
		.app_optstr = "akLDngHhIloprxe:StvVRd",
		.app_longopts = rsync_openrsync_longopts,
		.app_nlogonly = nitems(rsync_openrsync_logonly),
		.app_logonly_opts = rsync_openrsync_logonly,
	},
	{
		.app_name = "rsync_samba",
		.app_path = "/usr/libexec/rsync/rsync.samba",
		.app_path_relcwd = false,
		.app_opts_logonly = true,
		.app_optstr = "f:B:e:T:",
		.app_longopts = rsync_samba_longopts,
	},
};


/* END OF SPEC @generated CONTENTS */

/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * genwrap should fail if the specification didn't include any application at
 * all; better safe than sorry, though.
 */
_Static_assert(nitems(wrapper_apps) > 0, "No applications specified");

#define wrapper_assert_unreachable(msg) assert(0 && msg)

#define	WRAPPER_XCODE_PREFIX	"$XCODE/"

#ifdef WRAPPER_ANALYTICS_IDENT
static const char wrapper_arg_redacted[] = "*REDACTED*";

static xpc_object_t
wrapper_logged_args_filter(const struct application *app, xpc_object_t args,
    int argc, char *argv[])
{
	int ch, lpoptind;

	opterr = 0;
	optind = optreset = 1;
	lpoptind = 1;
	while ((ch = getopt_long(argc, argv, app->app_optstr, app->app_longopts,
	    NULL)) != -1) {
		char *redacted_arg, **target_arg;

		if (ch == '?' || optarg == NULL) {
			/*
			 * Print the previously processed arg; we didn't redact
			 * anything in it.
			 */
			if (optind > lpoptind) {
				xpc_array_set_string(args, XPC_ARRAY_APPEND,
				    argv[optind - 1]);
				lpoptind = optind;
			}

			continue;
		}

		target_arg = &argv[optind - 1];

		assert(lpoptind != optind);

		/*
		 * Determine if we can replace argv[optind - 1] wholesale or if
		 * we need a new string.
		 */
		if (optarg == *target_arg) {
			/*
			 * optarg is standalone, so we need to add the long
			 * option in the preceeding index.
			 */
			xpc_array_set_string(args, XPC_ARRAY_APPEND,
			    argv[optind - 2]);
			xpc_array_set_string(args, XPC_ARRAY_APPEND,
			    wrapper_arg_redacted);

			lpoptind = optind;
			continue;
		}

		if (asprintf(&redacted_arg, "%.*s%s",
		    (int)(optarg - *target_arg), *target_arg,
		    wrapper_arg_redacted) >= 0) {
			/* Success */
			xpc_array_set_string(args, XPC_ARRAY_APPEND,
			    redacted_arg);

			free(redacted_arg);
		} else {
			/* Failed */
			char c;

			/*
			 * It's crucial that we not accidentally log
			 * a redacted argument.  If we really can't
			 * replace it with an obvious token to show that
			 * it's redacted due to memory constraints,
			 * we'll just zap it for the event and restore it before
			 * we pass it on to the chosen application.
			 */
			c = *optarg;
			*optarg = '\0';

			xpc_array_set_string(args, XPC_ARRAY_APPEND,
			    *target_arg);

			*optarg = c;
		}

		lpoptind = optind;
	}

	/*
	 * Round up the last argument, if needed.  i.e., if we processed all of
	 * the options present in it without needing to redact anything.
	 */
	if (optind > lpoptind && optind < argc)
		xpc_array_set_string(args, XPC_ARRAY_APPEND, argv[optind - 1]);

	argc -= optind;
	argv += optind;

	/*
	 * Anything left over should just be redacted, as they might be paths
	 * or other potentially sensitive data.
	 */
	for (int i = 0; i < argc; i++) {
		xpc_array_set_string(args, XPC_ARRAY_APPEND,
		    wrapper_arg_redacted);
	}

	return (args);
}

/*
 * Return an array regardless; empty if there are no arguments to be logged,
 * non-empty otherwise.
 */
static xpc_object_t
wrapper_logged_args(const struct application *app, int argc, char *argv[])
{
	xpc_object_t args = xpc_array_create_empty();

	if (WRAPPER_ANALYTICS_NOARGS)
		return (wrapper_logged_args_filter(app, args, argc, argv));

	for (int i = 1; i < argc; i++) {
		xpc_array_set_string(args, XPC_ARRAY_APPEND, argv[i]);
	}

	return (args);
}
#endif

#if WRAPPER_APPLICATION_COUNT > 1
static const struct application *
wrapper_check_env(void)
{
#ifdef WRAPPER_ENV_VAR
	const char *val;

	if ((val = getenv(WRAPPER_ENV_VAR)) != NULL) {
		const struct application *app;

		for (size_t i = 0; i < nitems(wrapper_apps); i++) {
			app = &wrapper_apps[i];

			if (strcmp(val, app->app_name) == 0)
				return (app);
		}
	}
#endif

	return (NULL);
}

static bool
wrapper_check_args_excluded(const struct application *app, int opt)
{

	/*
	 * Checks whether the returned option is a logonly option to be excluded
	 * from considering this a candidate.
	 */
	if (opt >= app->app_nlogonly)
		return (false);

	return (app->app_logonly_opts[opt]);
}

static bool
wrapper_check_args_long(const struct application *app, const char *optstr,
    const struct option *longopts, int argc, char *argv[])
{
	int ch;

	opterr = 0;
	optind = optreset = 1;
	while ((ch = getopt_long(argc, argv, optstr, longopts, NULL)) != -1) {
		/*
		 * If we encounter an unrecognized flag or a logonly flag, we
		 * can't use this one.
		 */
		if (ch == '?' || wrapper_check_args_excluded(app, ch))
			return (false);
	}

	return (true);
}

/*
 * Return true if this app is compatible with the given options.
 */
static bool
wrapper_check_args_app(const struct application *app, int argc, char *argv[])
{

	/*
	 * If no arguments are specified, this one wins by default.
	 */
	if (app->app_optstr == NULL && app->app_longopts == NULL)
		return (true);

	/*
	 * If arguments are only specified for logging purposes, we don't need
	 * to filter through them -- this one wins.
	 */
	if (app->app_opts_logonly)
		return (true);

	return (wrapper_check_args_long(app, app->app_optstr, app->app_longopts,
	    argc, argv));
}

static const struct application *
wrapper_check_args(int argc, char *argv[])
{
	const struct application *app;

	/*
	 * If we only have the name, there are no arguments to check and we can
	 * simple execute the default application.
	 */
	if (argc == 1)
		return (&wrapper_apps[0]);

	for (size_t i = 0; i < nitems(wrapper_apps); i++) {
		app = &wrapper_apps[i];

		if (wrapper_check_args_app(app, argc, argv))
			return (app);
	}

	return (NULL);
}
#endif

#ifdef WRAPPER_ANALYTICS_TESTING
static int
wrapper_execute_analytics_testing(const struct application *app, int argc,
    char *argv[])
{
	/*
	 * If we're testing the wrapper analytics, we're just emitting the
	 * final arguments being reported.  name/chosen are assumed to be
	 * correct and may be tested separately.
	 */
	xpc_object_t args = wrapper_logged_args(app, argc, argv);
	printf("arguments\n");
	xpc_array_apply(args, ^bool(size_t idx, xpc_object_t val) {
		/* All of our elements are strings. */
		assert(xpc_get_type(val) == XPC_TYPE_STRING);

		printf("\t%s\n", xpc_string_get_string_ptr(val));
		return (true);
	});
	xpc_release(args);

	return (0);
}
#endif	/* WRAPPER_ANALYTICS_TESTING */

#ifdef WRAPPER_NEEDS_XCSELECT
static void
wrapper_invoke_xcrun(const struct application *app, int argc, char *argv[])
{
	const char *path;

	path = strchr(app->app_path, '/');
	assert(path != NULL);

	while (path[0] == '/')
		path++;

	assert(path[0] != '\0');

	/* Chop off the program name when we're running xcrun */
	xcselect_invoke_xcrun(path, argc - 1, argv + 1, true);
}
#endif

/*
 * wrapper_handle_relpath will do one of three things:
 * - Execute something else
 * - Error out
 * - Return with path populated to something that we should try to exec.
 */
static void
wrapper_handle_relpath(const struct application *app, char *path, size_t pathsz,
    int argc, char *argv[])
{
#ifndef WRAPPER_NEEDS_XCSELECT
	wrapper_assert_unreachable("broken wrapper shim configuration");
#else
	bool env, cltools, dflt;

	/* Shell out to xcrun for $XCODE/ paths */
	if (strncmp(app->app_path, WRAPPER_XCODE_PREFIX,
	    sizeof(WRAPPER_XCODE_PREFIX) - 1) == 0) {
		wrapper_invoke_xcrun(app, argc, argv);
		/* UNREACHABLE */
	}

	if (!xcselect_get_developer_dir_path(path, pathsz, &env, &cltools,
	    &dflt))
		errx(1, "Could not obtain developer dir path");

	/* We'll catch this strlcat() error with the next one; just ignore it. */
	(void)strlcat(path, "/", pathsz);

	if (strlcat(path, app->app_path, pathsz) >= pathsz)
		errx(1, "File name too long: %s", path);
#endif	/* !WRAPPER_NEEDS_XCSELECT */
}

static void
wrapper_execute_addargs(const struct application *app, int *argc,
    char **argv[])
{
	char **args;
	int i, total_args;

	if (app->app_add_nargv == 0)
		return;

	total_args = *argc + app->app_add_nargv;
	args = malloc(sizeof(*args) * (total_args + 1));
	if (args == NULL)
		err(1, "malloc");

	/*
	 * Push args[1] and up out of the way for the injected args to be
	 * prepended.
	 */
	i = 0;
	args[i++] = *argv[0];

	/* Arguments are off-by-one from where they land in argv. */
	for (; i <= app->app_add_nargv; i++)
		args[i] = __DECONST(char *, app->app_add_argv[i - 1]);

	/* Copy in the trailing args */
	memcpy(&args[i], *argv + 1, sizeof(*args) * *argc);

	*argv = args;
	*argc = total_args;
}

static int
wrapper_execute(const struct application *app, int argc, char *argv[])
{

#ifdef WRAPPER_ANALYTICS_TESTING
	return (wrapper_execute_analytics_testing(app, argc, argv));
#else	/* !WRAPPER_ANALYTICS_TESTING */
	char path[MAXPATHLEN];

	path[0] = '\0';

#ifdef WRAPPER_ANALYTICS_IDENT
	analytics_send_event_lazy(WRAPPER_ANALYTICS_IDENT, ^(void) {
		xpc_object_t payload = xpc_dictionary_create_empty();
		xpc_object_t args = wrapper_logged_args(app, argc, argv);

		xpc_dictionary_set_string(payload, "name", argv[0]);
		xpc_dictionary_set_value(payload, "arguments", args);
		xpc_dictionary_set_string(payload, "chosen", app->app_name);
		xpc_release(args);

		return (payload);
	});
#endif	/* WRAPPER_ANALYTICS_IDENT */

	/*
	 * Augment argv *after* we've reported analytics on it; we only want
	 * to collect user usage for telemetry purpose.
	 */
	wrapper_execute_addargs(app, &argc, &argv);

	/*
	 * Absolute paths are run directly, relative paths are relative to the
	 * selected developer dir unless it's a cwdpath that was specified.
	 *
	 * cwdpaths are undocumented and only really intended for regression
	 * testing purposes at this time.
	 */
	if (app->app_path[0] != '/' && !app->app_path_relcwd) {
		wrapper_handle_relpath(app, path, sizeof(path), argc, argv);

		/*
		 * If wrapper_handle_relpath returns, it must have populated
		 * path with the path to execute.
		 */
		assert(path[0] != '\0');
	} else if (strlcpy(path, app->app_path, sizeof(path)) >= sizeof(path)) {
		errx(1, "File name too long: %s", path);
	}

	execv(path, argv);
	err(1, "execv(%s)", path);
	/* UNREACHABLE */
#endif	/* !WRAPPER_ANALYTICS_TESTING */
}

int
main(int argc, char *argv[])
{

	/*
	 * Obviously no need to bother checking anything if we just have a
	 * single application.  Such a setup is not unexpected, as it could be
	 * used to simply collect some data on arguments in common use via
	 * CoreAnalytics.
	 */
#if WRAPPER_APPLICATION_COUNT > 1
	const struct application *chosen_app;

	chosen_app = wrapper_check_env();
	if (chosen_app != NULL)
		return (wrapper_execute(chosen_app, argc, argv));

	chosen_app = wrapper_check_args(argc, argv);
	if (chosen_app != NULL)
		return (wrapper_execute(chosen_app, argc, argv));
#endif

	return (wrapper_execute(&wrapper_apps[0], argc, argv));
}
