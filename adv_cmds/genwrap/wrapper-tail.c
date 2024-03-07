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
wrapper_check_args_long(const char *optstr, const struct option *longopts,
    int argc, char *argv[])
{
	int ch;

	opterr = 0;
	optind = optreset = 1;
	while ((ch = getopt_long(argc, argv, optstr, longopts, NULL)) != -1) {
		/*
		 * If we encounter an unrecognized flag, we can't use this one.
		 */
		if (ch == '?')
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

	return (wrapper_check_args_long(app->app_optstr, app->app_longopts,
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
	if (app->app_path[0] == '/' || app->app_path_relcwd) {
		if (strlcpy(path, app->app_path, sizeof(path)) >= sizeof(path))
			errx(1, "File name too long: %s", path);
		goto run;
	}

#ifdef WRAPPER_NEEDS_XCSELECT
	bool env, cltools, dflt;

	if (!xcselect_get_developer_dir_path(path, sizeof(path), &env, &cltools,
	    &dflt)) {
		snprintf(path, sizeof(path), "/%s", app->app_path);
		goto run;
	}

	/* We'll catch this strlcat() error with the next one; just ignore it. */
	(void)strlcat(path, "/", sizeof(path));

	if (strlcat(path, app->app_path, sizeof(path)) >= sizeof(path))
		errx(1, "File name too long: %s", path);
#else	/* !WRAPPER_NEEDS_XCSELECT */
	/* UNREACHABLE */
#endif	/* WRAPPER_NEEDS_XCSELECT */

run:
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
