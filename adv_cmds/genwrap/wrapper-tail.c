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

#define	WRAPPER_ARGPREF_SHORT	"arg_"
#define	WRAPPER_ARGPREF_LONG	"arg__"

#ifdef WRAPPER_ANALYTICS_IDENT
static const char wrapper_arg_redacted[] = "*REDACTED*";

static char *
wrapper_logged_arg_long_name(struct arg_expr *expr, const struct option *lopt)
{
	char *argkey = NULL;

	if (expr == NULL) {
		if (asprintf(&argkey, WRAPPER_ARGPREF_LONG "%s",
		    lopt->name) < 0)
			return (NULL);
	} else {
		if (asprintf(&argkey, WRAPPER_ARGPREF_LONG "%s__%.02zu",
		    lopt->name, expr->expr_count++) < 0)
			return (NULL);
	}

	assert(argkey != NULL);
	return (argkey);
}

static char *
wrapper_logged_arg_short_name(struct arg_expr *expr, int ch)
{
	char *argkey = NULL;

	if (expr == NULL) {
		if (asprintf(&argkey, WRAPPER_ARGPREF_SHORT "%c", ch) < 0)
			return (NULL);
	} else {
		if (asprintf(&argkey, WRAPPER_ARGPREF_SHORT "%c__%.02zu", ch,
		    expr->expr_count++) < 0)
			return (NULL);
	}

	assert(argkey != NULL);
	return (argkey);
}

static char *
wrapper_logged_arg_name(struct arg_expr *expr, const struct option *lopt, int ch)
{

	if (lopt != NULL)
		return (wrapper_logged_arg_long_name(expr, lopt));
	return (wrapper_logged_arg_short_name(expr, ch));
}

static regex_t *
wrapper_logged_arg_expr(struct arg_expr *expr, struct arg_expr **oexpr)
{

	if (expr->expr_str == NULL)
		return (NULL);

	if (!expr->expr_compiled) {
		int error;

		expr->expr_compiled = true;
		error = regcomp(&expr->expr_reg, expr->expr_str, REG_EXTENDED);
		if (error != 0)
			expr->expr_error = true;
	}

	if (expr->expr_error)
		return (NULL);
	*oexpr = expr;
	return (&expr->expr_reg);
}

static regex_t *
wrapper_logged_arg_short_expr(const struct application *app, char flag,
    struct arg_expr **oexpr)
{
	const char *walker;
	size_t idx;

	if (app->app_shortopt_expr == NULL)
		return (NULL);

	walker = app->app_optstr;
	if (walker == NULL)
		return (NULL);

	if (*walker == '+')
		walker++;

	for (idx = 0; *walker != '\0' && *walker != flag; walker++) {
		if (*walker == ':')
			continue;	/* Skip */
		idx++;
	}

	/* Unrecognized options are skipped, this shouldn't happen. */
	assert(*walker != '\0');

	return (wrapper_logged_arg_expr(&app->app_shortopt_expr[idx], oexpr));
}

static regex_t *
wrapper_logged_arg_long_expr(const struct application *app, size_t idx,
    struct arg_expr **oexpr)
{

	if (app->app_longopt_expr == NULL)
		return (NULL);
	return (wrapper_logged_arg_expr(&app->app_longopt_expr[idx], oexpr));
}

static xpc_object_t
wrapper_logged_args(const struct application *app, int argc, char *argv[],
    bool *halted, unsigned int *errors)
{
	xpc_object_t args = xpc_dictionary_create_empty();
	int ch, lidx;

	*halted = false;
	if (errors != NULL)
		*errors = 0;
	if (app->app_optstr == NULL && app->app_longopts == NULL)
		return (args);

	opterr = 0;
	optind = optreset = 1;
	lidx = -1;
	while ((ch = getopt_long(argc, argv, app->app_optstr, app->app_longopts,
	    &lidx)) != -1) {
		uint64_t count;
		const struct option *lopt = NULL;
		regex_t *preg = NULL;
		struct arg_expr *expr = NULL;
		char *argkey;

		/*
		 * We must halt if we hit an argument that isn't documented in
		 * the wrapper definition.  We have no idea if whatever follows
		 * is an argument or not, we can only make some assumptions
		 * based on the basic shape of it (and we would rather not).
		 */
		if (ch == '?') {
			*halted = true;
			break;
		}

		if (lidx >= 0) {
			lopt = &app->app_longopts[lidx];
			if (optarg != NULL) {
				preg = wrapper_logged_arg_long_expr(app, lidx,
				    &expr);
			}
			lidx = -1;
		} else if (optarg != NULL) {
			preg = wrapper_logged_arg_short_expr(app, ch, &expr);
		}

		assert((preg != NULL) == (expr != NULL));

		if ((argkey = wrapper_logged_arg_name(expr, lopt, ch)) == NULL) {
			if (errors != NULL)
				(*errors)++;
			continue;
		}

		/*
		 * If we have a pattern to check, try it.  If it fails, we'll
		 * fall back to just inserting a count.
		 */
		if (preg != NULL) {
			regmatch_t match;
			int error;

			assert(optarg != NULL);

			/*
			 * We only accept a match if the match happened to be
			 * the whole string.  This way, we don't rely on the
			 * wrapper definition to anchor every single expression.
			 */
			error = regexec(preg, optarg, 1, &match, 0);
			if (error == 0 &&
			    match.rm_so == 0 &&
			    match.rm_eo == strlen(optarg)) {
				xpc_dictionary_set_string(args, argkey,
				    optarg);
				free(argkey);

				/*
				 * We'll also set the unsuffixed name with the
				 * last key found, in case we know only the last
				 * one is used and want to capture that instead.
				 */
				if ((argkey = wrapper_logged_arg_name(NULL, lopt,
				    ch)) == NULL) {
					if (errors != NULL)
						(*errors)++;
					continue;
				}
				xpc_dictionary_set_string(args, argkey,
				    optarg);
				free(argkey);

				continue;
			}
		}

		count = xpc_dictionary_get_uint64(args, argkey);
		xpc_dictionary_set_uint64(args, argkey, count + 1);

		free(argkey);

		if (expr == NULL)
			continue;

		/*
		 * If we were doing pattern matching for this option but the
		 * last key didn't match, then we just set the value to the #
		 * times that the option appeared.
		 */
		if ((argkey = wrapper_logged_arg_name(NULL, lopt, ch)) == NULL) {
			if (errors != NULL)
				(*errors)++;
			continue;
		}

		xpc_dictionary_set_uint64(args, argkey, expr->expr_count);
		free(argkey);
	}

	return (args);
}
#endif

#if WRAPPER_APPLICATION_COUNT > 1
static const struct application *
wrapper_by_name(const char *appname)
{
	const struct application *app;

	for (size_t i = 0; i < nitems(wrapper_apps); i++) {
		app = &wrapper_apps[i];

		if (strcmp(appname, app->app_name) == 0)
			return (app);
	}

	/*
	 * We've historically ignored errors in env selection; should we
	 * consider warning here instead, rather than just falling through to
	 * the "default" application?  The behavior will be documented either
	 * way.
	 */
	return (NULL);
}

static const struct application *
wrapper_check_env(void)
{
#ifdef WRAPPER_ENV_VAR
	const char *val;

	if ((val = getenv(WRAPPER_ENV_VAR)) != NULL)
		return (wrapper_by_name(val));
#endif

	return (NULL);
}

static const struct application *
wrapper_check_var(void)
{
	/*
	 * The wrapper name may not be defined if the wrapper was fed via
	 * stdin, in which case we won't have defined WRAPPER_NAME.  We'll just
	 * not check /var/select in those wrappers.
	 */
#ifdef WRAPPER_NAME
	static const char varpath[] = _PATH_VARSEL WRAPPER_NAME;
	static bool var_app_read;
	static const struct application *var_app;
	char target[WRAPPER_MAXNAMELEN];
	const struct application *app;
	ssize_t ret;

	if (var_app_read)
		return (var_app);

	ret = readlink(varpath, target, sizeof(target));
	var_app_read = true;
	if (ret <= 0 || ret == sizeof(target))
		return (NULL);

	target[ret] = '\0';

	/*
	 * We might get called twice under arg-based selection, so cache the
	 * result just in case.
	 */
	var_app = wrapper_by_name(target);
	return (var_app);
#else
	return (NULL);
#endif
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
	const struct application *app, *dflt_app;

	/*
	 * If we only have the name, there are no arguments to check and we can
	 * simple execute the default application.
	 */
	if (argc == 1)
		return (NULL);

	/*
	 * If a default has been provided via /var/select, that overrides what
	 * was specified as the default in the wrapper config -- thus, we check
	 * that one first, then check every other application specified.  If
	 * none of them are compatible with the arguments chosen, we'll use the
	 * var-specified app anyways.
	 */
	dflt_app = wrapper_check_var();
	if (dflt_app != NULL && wrapper_check_args_app(dflt_app, argc, argv))
		return (dflt_app);

	for (size_t i = 0; i < nitems(wrapper_apps); i++) {
		app = &wrapper_apps[i];
		if (app == dflt_app)
			continue;

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
	xpc_object_t args;
	bool halted;

	/*
	 * If we're testing the wrapper analytics, we're just emitting the
	 * final arguments being reported.  name/chosen are assumed to be
	 * correct and may be tested separately.
	 */
	args = wrapper_logged_args(app, argc, argv, &halted, NULL);
	printf("arguments\n");

	xpc_dictionary_apply(args, ^bool(const char *key, xpc_object_t val) {
		if (strncmp(key, WRAPPER_ARGPREF_LONG,
		    sizeof(WRAPPER_ARGPREF_LONG) - 1) == 0) {
			printf("\t--%s", &key[sizeof(WRAPPER_ARGPREF_LONG) - 1]);
		} else if (strncmp(key, WRAPPER_ARGPREF_SHORT,
		    sizeof(WRAPPER_ARGPREF_SHORT) - 1) == 0) {
			printf("\t-%s", &key[sizeof(WRAPPER_ARGPREF_SHORT) - 1]);
		} else {
			assert(0 && "Invalid arg entry");
		}

		if (xpc_get_type(val) == XPC_TYPE_STRING)
			printf(" %s", xpc_string_get_string_ptr(val));
		else if (xpc_get_type(val) == XPC_TYPE_UINT64)
			printf(" %ju", (uintmax_t)xpc_uint64_get_value(val));
		else
			assert(0 && "Bad value type");

		printf("\n");
		return (true);
	});

	if (halted)
		printf("\t<<HALTED>>\n");

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
		xpc_object_t args, payload;
		unsigned int errors;
		bool halted;

		payload = xpc_dictionary_create_empty();
		args = wrapper_logged_args(app, argc, argv, &halted, &errors);

		xpc_dictionary_set_string(payload, "name", argv[0]);
		xpc_dictionary_set_string(payload, "chosen", app->app_name);
		if (errors != 0)
			xpc_dictionary_set_uint64(payload, "argerrors", errors);
		if (halted)
			xpc_dictionary_set_bool(payload, "arghalt", true);

		xpc_dictionary_apply(args, ^bool(const char *key, xpc_object_t val) {
			xpc_type_t type;

			type = xpc_get_type(val);
			assert(type == XPC_TYPE_STRING ||
			    type == XPC_TYPE_UINT64);

			if (type == XPC_TYPE_STRING) {
				xpc_dictionary_set_string(payload, key,
				    xpc_string_get_string_ptr(val));
			} else {
				xpc_dictionary_set_uint64(payload, key,
				    xpc_uint64_get_value(val));
			}

			return (true);
		});
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

static const struct application *
wrapper_app(int argc, char *argv[])
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
		return (chosen_app);

	chosen_app = wrapper_check_args(argc, argv);
	if (chosen_app != NULL)
		return (chosen_app);

	chosen_app = wrapper_check_var();
	if (chosen_app != NULL)
		return (chosen_app);
#endif

	return (&wrapper_apps[0]);
}

int
main(int argc, char *argv[])
{

	return (wrapper_execute(wrapper_app(argc, argv), argc, argv));
}
