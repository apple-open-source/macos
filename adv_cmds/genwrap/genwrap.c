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

#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/queue.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "genwrap.h"

#define	GENWRAP_MAXNAMELEN	NAME_MAX

#ifndef nitems
#define	nitems(x)	(sizeof((x)) / sizeof((x)[0]))
#endif

/*
 * The genwrap_static build will override _PATH_SKELDIR to pull the wrapper-*
 * artifacts straight from the source tree.
 */
#ifndef _PATH_SKELDIR
#define	_PATH_SKELDIR	"/usr/local/share/genwrap/"
#endif

struct appflag {
	const char	*appflag_flag;
	const char	*appflag_pattern;
	int		 appflag_arg;
	int		 appflag_alias;
	uint32_t	 appflag_flags;

	LIST_ENTRY(appflag)	appflag_entries;
};

struct app {
	LIST_ENTRY(app)		app_entries;
	LIST_HEAD(, appflag)	app_flags;
	const char	*app_name;
	const char	*app_path;
	struct appflag	*app_lastflag;
	const char	**app_add_argv;
	size_t		 app_short_patterns;
	size_t		 app_long_patterns;
	int		 app_add_nargv;
	unsigned int	 app_shortflags;
	unsigned int	 app_longflags;
	bool		 app_default;
	bool		 app_anyarg_logonly;
	bool		 app_argmode_logonly;
	bool		 app_path_relcwd;
};

#define	TOKEN(str)	{ str, sizeof(str) - 1 }

static const struct app_path_token {
	const char *token;
	size_t tokensz;
} app_path_tokens[] = {
	TOKEN("XCODE"),
};

static LIST_HEAD(, app) apps = LIST_HEAD_INITIALIZER(apps);
static size_t app_count;
static size_t app_maxnamelen;
static const char *analytics_id;
static bool analytics_no_args;
static const char *envvar;

static int debug;
#define	dprintf(...)	do {			\
	if (debug)				\
		fprintf(stderr, __VA_ARGS__);	\
} while(0)

static char *wrapper_name;

/*
 * Allow consumers to build without xcselect if they're using all absolute or
 * cwd-relative paths; xcselect requires an internal SDK.
 */
static bool needs_xcselect;

static void wrapper_output_file(FILE *outfile, const char *path);
static void wrapper_write(FILE *outfile);

static void
usage(void)
{

	fprintf(stderr, "usage: %s [-d] [-n name] [-o output] spec\n", getprogname());
	exit(1);
}

/*
 * The implied name of the wrapper is the filename with a trailing .wrapper
 * chopped off.  This is used for checking /var/select/<wrapper_name>.
 */
static char *
implied_name(const char *filename)
{
	const char *basefile, *suffix;
	char *name = NULL;

	basefile = basename(filename);
	suffix = strrchr(basefile, '.');
	if (suffix != NULL) {
		/*
		 * If it has a suffix, see if it's ".wrapper".  If it's not,
		 * we'll just use the full name.
		 */
		if (strcmp(suffix, ".wrapper") == 0) {
			name = strndup(basefile, suffix - basefile);
			if (name == NULL)
				err(1, "strndup");
		}
	}

	if (name == NULL)
		name = strdup(basefile);

	return (name);
}

int
main(int argc, char *argv[])
{
	FILE *outfile;
	int ch;

	outfile = NULL;
	while ((ch = getopt(argc, argv, "dn:o:")) != -1) {
		switch (ch) {
		case 'd':
			debug++;
			break;
		case 'n':
			free(wrapper_name);
			wrapper_name = strdup(optarg);
			if (wrapper_name == NULL)
				err(1, "strdup");
			break;
		case 'o':
			if (strcmp(optarg, "-") == 0 ||
			    strcmp(optarg, "/dev/stdout") == 0)
				outfile = stdout;
			else
				outfile = fopen(optarg, "w");

			if (outfile == NULL)
				err(1, "fopen(%s)", optarg);
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (outfile == NULL)
		outfile = stdout;

	if (argc != 1)
		usage();

	if (strcmp(argv[0], "-") == 0 || strcmp(argv[0], "/dev/stdin") == 0) {
		yyin = stdin;
		yyfile = "(stdin)";
	} else {
		if (wrapper_name == NULL)
			wrapper_name = implied_name(argv[0]);

		yyin = fopen(argv[0], "r");
		yyfile = argv[0];
	}

	if (yyin == NULL)
		err(1, "fopen(%s)", argv[0]);

	yyparse();

	if (LIST_EMPTY(&apps))
		errx(1, "ERROR: no applications defined");

	wrapper_write(outfile);

	return (0);
}

struct app *
app_add(struct app *app, const char *name)
{
	struct app *new_app;
	size_t namelen;

	assert(name != NULL);

	/*
	 * name + NUL terminator must be no more than MAXNAMELEN, because we
	 * want it to fit into a symlink.
	 */
	namelen = strlen(name) + 1;
	if (namelen > GENWRAP_MAXNAMELEN)
		yyerror("name too long");

	if (namelen > app_maxnamelen)
		app_maxnamelen = namelen;

	new_app = calloc(1, sizeof(*new_app));
	if (new_app == NULL)
		err(1, "calloc");

	new_app->app_name = strdup(name);
	if (new_app->app_name == NULL)
		err(1, "strdup");

	LIST_INIT(&new_app->app_flags);

	if (app == NULL)
		LIST_INSERT_HEAD(&apps, new_app, app_entries);
	else
		LIST_INSERT_AFTER(app, new_app, app_entries);

	app_count++;

	dprintf("application[name=\"%s\"]\n", new_app->app_name);

	return (new_app);
}

void
app_set_default(struct app *app)
{
	static struct app *default_app;

	dprintf("  default");

	/*
	 * Tracking the specified default app because it's not really erroneous
	 * to specify the default app out-of-order in the specification, just to
	 * explicitly label more than one as the default.
	 */
	if (default_app != NULL) {
		warnx("WARNING: '%s' previously set as default app",
		    default_app->app_name);
		default_app->app_default = false;
		dprintf("[from=\"%s\"]", default_app->app_name);
	}

	dprintf("\n");

	default_app = app;
	app->app_default = true;

	/*
	 * Simplify the generated wrapper a little bit by always pushing the
	 * the default app to the front.  Doing it here simplifies the output
	 * process a little bit, too, so that we don't need to make two passes
	 * to write out definitions.
	 */
	if (LIST_FIRST(&apps) != app) {
		LIST_REMOVE(app, app_entries);
		LIST_INSERT_HEAD(&apps, app, app_entries);
	}
}

void
app_set_argmode_logonly(struct app *app)
{

	app->app_argmode_logonly = true;
	dprintf("  argmode[logonly=true]\n");
}

void
app_add_addarg(struct app *app, const char **argv, int nargv)
{
	int total_args;

	/*
	 * We'll extend addargs as needed; capturing everything in order that
	 * it's specified.
	 */
	total_args = app->app_add_nargv + nargv;
	app->app_add_argv = realloc(app->app_add_argv,
	    sizeof(*app->app_add_argv) * total_args);
	if (app->app_add_argv == NULL)
		err(1, "realloc");

	for (int i = app->app_add_nargv; i < total_args; i++) {
		int idx = i - app->app_add_nargv;

		dprintf("  addarg[arg=\"%s\"]\n", argv[idx]);

		/*
		 * The caller won't be needing this anymore, just take it rather
		 * than making our own copy.  NULL out the caller's copy so that
		 * it can't free it out from underneath us.
		 */
		app->app_add_argv[i] = argv[idx];
		argv[idx] = NULL;
	}

	app->app_add_nargv = total_args;
}

static bool
app_validate_path(struct app *app, const char *path, bool relcwd)
{
	const struct app_path_token *token;
	size_t pos;

	/*
	 * The only validation we even attempt at the moment is just that a
	 * leading $ should match a defined token.
	 */
	if (path[0] != '$')
		return (true);

	/* Validate leading tokens last */
	path++;
	for (size_t i = 0; i < nitems(app_path_tokens); i++) {
		token = &app_path_tokens[i];

		if (strncmp(path, token->token, token->tokensz) != 0)
			continue;

		/*
		 * Must not end right after the token.
		 */
		if (path[token->tokensz] == '\0')
			return (false);

		/*
		 * Make sure we don't have trailing garbage; it could also be
		 * that we're looking at a token that's incidentally a prefix of
		 * another token, so we don't outright reject it now.
		 */
		if (path[token->tokensz] != '/')
			continue;

		/* Make sure it's not just trailing / then the end... */
		pos = token->tokensz + 1;
		while (path[pos] == '/')
			pos++;

		if (path[pos] == '\0')
			return (false);

		return (true);
	}

	return (false);
}

void
app_set_path(struct app *app, const char *path, bool relcwd)
{

	dprintf("  path[");

	if (app->app_path != NULL) {
		warnx("WARNING: overriding path for '%s'", app->app_name);
		dprintf("from=\"%s\", ", app->app_path);
		free(__DECONST(char *, app->app_path));
	}

	if (!app_validate_path(app, path, relcwd))
		yyerror("bad path specified");

	app->app_path = strdup(path);
	if (app->app_path == NULL)
		err(1, "strdup");
	app->app_path_relcwd = relcwd;

	if (!relcwd && path[0] != '/')
		needs_xcselect = true;

	dprintf("to=\"%s\"]\n", app->app_path);
}

const char *
app_get_path(const struct app *app)
{

	return (app->app_path);
}

static void
app_add_one_flag(struct app *app, const char *flag, char alias, int argument,
    uint32_t flags, const char *pattern)
{
	struct appflag *af;

	af = calloc(1, sizeof(*af));
	if (af == NULL)
		err(1, "calloc");

	af->appflag_arg = argument;
	af->appflag_flags = flags;
	af->appflag_alias = alias;
	af->appflag_flag = strdup(flag);
	if (af->appflag_flag == NULL)
		err(1, "strdup");
	if (pattern != NULL) {
		af->appflag_pattern = strdup(pattern);
		if (af->appflag_pattern == NULL)
			err(1, "strdup");
	}

	/* Preserve arg order for the aesthetics of it... */
	if (app->app_lastflag == NULL)
		LIST_INSERT_HEAD(&app->app_flags, af, appflag_entries);
	else
		LIST_INSERT_AFTER(app->app_lastflag, af, appflag_entries);

	app->app_lastflag = af;

	/* A bit of accounting to simplify later iteration when writing out. */
	if (flag[1] == '\0') {
		if (pattern != NULL)
			app->app_short_patterns++;
		app->app_shortflags++;
	} else {
		if (pattern != NULL)
			app->app_long_patterns++;
		app->app_longflags++;
	}

	if ((flags & ARGFLAG_LOGONLY) != 0)
		app->app_anyarg_logonly = true;
}

void
app_add_flag(struct app *app, const char *flag, const char *alias, int argument,
    uint32_t flags, const char *pattern)
{

	assert(flag != NULL);
	if (alias != NULL && alias[1] != '\0')
		yyerror("short flag alias must only have one character");

	if (debug) {
		fprintf(stderr, "  arg[flag=\"%s\"", flag);
		if (alias != NULL)
			fprintf(stderr, ", alias=\"%s\"", alias);
		switch (argument) {
		case required_argument:
			fprintf(stderr, ", arg=required");
			break;
		case optional_argument:
			fprintf(stderr, ", arg=optional");
			break;
		default:
			fprintf(stderr, ", arg=none");
			break;
		}

		if (pattern != NULL)
			fprintf(stderr, ", pattern=\"%s\"", pattern);
		fprintf(stderr, ", flags=%x]\n", flags);
	}

	/* The alias must be added as its own flag to the optstr as well. */
	if (alias != NULL) {
		app_add_one_flag(app, alias, 0, argument,
		    flags & ~ARGFLAG_NO_ALIAS, pattern);
	}

	app_add_one_flag(app, flag, (alias != NULL ? alias[0] : 0), argument,
	    flags, pattern);
}

static const char *
app_longopt_name(const struct app *app)
{
	/* Single threaded; kludgy but OK. */
	static char namebuf[PATH_MAX];

	snprintf(namebuf, sizeof(namebuf), "%s_longopts", app->app_name);
	return (namebuf);
}

static const char *
app_pattern_name(const struct app *app, const char *type)
{
	/* Single threaded; kludgy but OK. */
	static char namebuf[PATH_MAX];

	snprintf(namebuf, sizeof(namebuf), "%s_%s_patterns", app->app_name,
	    type);
	return (namebuf);
}

static const char *
app_addarg_name(const struct app *app)
{
	/* Single threaded; kludgy but OK. */
	static char namebuf[PATH_MAX];

	snprintf(namebuf, sizeof(namebuf), "%s_add_args", app->app_name);
	return (namebuf);
}

static const char *
app_logonly_name(const struct app *app)
{
	/* Single threaded; kludgy but OK. */
	static char namebuf[PATH_MAX];

	snprintf(namebuf, sizeof(namebuf), "%s_logonly", app->app_name);
	return (namebuf);
}

void
wrapper_set_analytics(const char *id, bool noargs)
{
	const char *c;

	for (c = id; *c != '\0'; c++) {
		/* XXX Other invalid characters? */
		if (*c == '"' || *c == '\'')
			yyerror("invalid analytics identifier");
	}

	/* Noarg version not yet implemented */
	analytics_id = strdup(id);
	if (analytics_id == NULL)
		err(1, "strdup");

	analytics_no_args = noargs;
	dprintf("analytics[id=\"%s\"]\n", analytics_id);
}

void
wrapper_set_envvar(const char *var)
{

	if (var == NULL || *var == '\0')
		yyerror("invalid env var value");

	envvar = var;
	dprintf("env[name=\"%s\"]\n", envvar);
}

static void
wrapper_output_file(FILE *outfile, const char *path)
{
	static char buf[MAXBSIZE];
	ssize_t rsz;
	int fd;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		err(1, "open");
	while ((rsz = read(fd, buf, sizeof(buf))) > 0) {
		if (fwrite(buf, 1, rsz, outfile) < rsz)
			err(1, "fwrite");
	}

	if (rsz < 0)
		err(1, "read");

	close(fd);
}

static void
wrapper_write_logonly_args(FILE *outfile, const struct app *app)
{
	const struct appflag *af;
	int val;

	fprintf(outfile, "static const bool %s[] = {\n",
	    app_logonly_name(app));
	LIST_FOREACH(af, &app->app_flags, appflag_entries) {
		if ((af->appflag_flags & ARGFLAG_LOGONLY) == 0)
			continue;
		if (af->appflag_flag[1] == '\0') {
			/* Short option */
			val = (unsigned char)af->appflag_flag[0];
		} else {
			/*
			 * Either set by the wrapper spec, or set to a CHAR_MAX
			 * + n constant below.
			 */
			val = af->appflag_alias;
		}

		if (val >= CHAR_MAX) {
			fprintf(outfile, "\t[CHAR_MAX + %d] = true,\n",
			    val - CHAR_MAX);
		} else {
			fprintf(outfile, "\t['%c'] = true,\n",
			    val);
		}
	}
	fprintf(outfile, "};\n");
}

static void
wrapper_write_pattern(FILE *outfile, const char *pattern)
{

	/*
	 * We write the pattern out byte-by-byte so that we can escape any
	 * slashes and quotes as needed.
	 */
	for (const char *walker = pattern; *walker != '\0'; walker++) {
		if (*walker == '\\' || *walker == '"')
			fputc('\\', outfile);
		fputc(*walker, outfile);
	}
}

static void
wrapper_write_patterns(FILE *outfile, const struct app *app)
{
	const struct appflag *af;
	size_t idx;

	/*
	 * These will all be indexed by position for the given type of flag in
	 * app_flags, which must be the order they're written into the
	 * wrapper's optstr and options array by wrapper_write_args() and
	 * wrapper_write_long_args() respectively.
	 */
	if (app->app_short_patterns != 0) {
		idx = 0;
		fprintf(outfile, "static struct arg_expr %s[%u] = {\n",
		    app_pattern_name(app, "short"), app->app_shortflags);
		LIST_FOREACH(af, &app->app_flags, appflag_entries) {
			if (af->appflag_flag[1] != '\0')
				continue;
			if (af->appflag_pattern != NULL) {
				fprintf(outfile,
				    "\t[%zu] = { .expr_str = \"", idx);

				wrapper_write_pattern(outfile,
				    af->appflag_pattern);

				fprintf(outfile, "\" },\n");
			}

			idx++;
		}
		fprintf(outfile, "};\n");
	}

	if (app->app_long_patterns != 0) {
		const char *expname;

		idx = 0;
		expname = app_pattern_name(app, "long");
		fprintf(outfile, "static struct arg_expr %s[%u] = {\n",
		    expname, app->app_longflags);
		LIST_FOREACH(af, &app->app_flags, appflag_entries) {
			if (af->appflag_flag[1] == '\0')
				continue;
			if (af->appflag_pattern != NULL) {
				fprintf(outfile,
				    "\t[%zu] = { .expr_str = \"", idx);

				wrapper_write_pattern(outfile,
				    af->appflag_pattern);

				fprintf(outfile, "\" },\n");
			}

			idx++;
		}
		fprintf(outfile, "};\n");
		fprintf(outfile,
		    "_Static_assert(nitems(%s) == nitems(%s) - 1, \"Long option mismatch\");\n",
		    expname, app_longopt_name(app));
	}
}

static void
wrapper_write_long_args(FILE *outfile, const struct app *app)
{
	static const char *argvalues[] = {
		[no_argument] = "no_argument",
		[optional_argument] = "optional_argument",
		[required_argument] = "required_argument",
	};
	struct appflag *af;
	int coff;	/* Offset from CHAR_MAX */

	coff = 0;
	fprintf(outfile, "static const struct option %s[] = {\n",
	    app_longopt_name(app));
	LIST_FOREACH(af, &app->app_flags, appflag_entries) {
		if (af->appflag_flag[1] == '\0')
			continue;

		fprintf(outfile, "	{");
		fprintf(outfile, "\"%s\", ", af->appflag_flag);

		assert(af->appflag_arg >= 0);
		assert(af->appflag_arg < nitems(argvalues));
		assert(argvalues[af->appflag_arg] != NULL);
		fprintf(outfile, "%s, ", argvalues[af->appflag_arg]);

		fprintf(outfile, "NULL, ");
		if (af->appflag_alias != '\0') {
			fprintf(outfile, "'%c'", af->appflag_alias);
		} else {
			af->appflag_alias = CHAR_MAX + coff;
			fprintf(outfile, "CHAR_MAX + %d", coff++);
		}
		fprintf(outfile, " },\n");
	}
	fprintf(outfile, "	{ NULL, 0, 0, 0 },\n");
	fprintf(outfile, "};\n");
}

static void
wrapper_write_addargs(FILE *outfile, const struct app *app)
{

	fprintf(outfile, "static const char *%s[] = {\n", app_addarg_name(app));
	for (int i = 0; i < app->app_add_nargv; i++) {
		fprintf(outfile, "\t\"%s\",\n", app->app_add_argv[i]);
	}
	fprintf(outfile, "};\n");
}

static void
wrapper_write_args(FILE *outfile, const struct app *app)
{
	const struct appflag *af;

	if (app->app_shortflags > 0) {
		/* app_optstr */
		fprintf(outfile, "		.app_optstr = \"+");
		LIST_FOREACH(af, &app->app_flags, appflag_entries) {
			if (af->appflag_flag[1] != '\0')
				continue;

			fprintf(outfile, "%c", af->appflag_flag[0]);

			if (af->appflag_arg == required_argument)
				fprintf(outfile, ":");
			else if (af->appflag_arg == optional_argument)
				fprintf(outfile, "::");
		}
		fprintf(outfile, "\",\n");

	}

	if (app->app_longflags > 0) {
		/* app_longopts */
		fprintf(outfile, "		.app_longopts = %s,\n",
		    app_longopt_name(app));
	}

	if (app->app_anyarg_logonly) {
		fprintf(outfile, "		.app_nlogonly = nitems(%s),\n",
		    app_logonly_name(app));
		fprintf(outfile, "		.app_logonly_opts = %s,\n",
		    app_logonly_name(app));
	}
}

static void
wrapper_write(FILE *outfile)
{
	struct app *app;
	bool empty;

	/*
	 * Write out some prologue before we include the start of the wrapper;
	 * mainly, settings that may influence whether we #include in some
	 * files.  Primarily, we don't want non-analytics enabled wrappers to
	 * require the CoreAnalytics framework.
	 */
	fprintf(outfile, "/* START OF SPEC @" "generated CONTENTS */\n\n");
	fprintf(outfile, "/* General settings */\n");

	/*
	 * We have to #define this because sizeof() isn't usable in C preproc,
	 * unfortunately.
	 */
	fprintf(outfile, "#define WRAPPER_APPLICATION_COUNT %zu\n", app_count);

	fprintf(outfile, "#define WRAPPER_MAXNAMELEN %zu\n", app_maxnamelen);
	if (wrapper_name != NULL)
		fprintf(outfile, "#define WRAPPER_NAME \"%s\"\n", wrapper_name);

	if (envvar != NULL) {
		fprintf(outfile, "#define WRAPPER_ENV_VAR \"%s\"\n",
		    envvar);
	}

	if (analytics_id != NULL) {
		fprintf(outfile, "#define WRAPPER_ANALYTICS_IDENT \"%s\"\n",
		    analytics_id);
		fprintf(outfile, "#define WRAPPER_ANALYTICS_NOARGS %s\n",
		    analytics_no_args ? "true" : "false");
	}

	if (needs_xcselect) {
		fprintf(outfile, "#define WRAPPER_NEEDS_XCSELECT\n");
	}

	fprintf(outfile, "\n/* END OF SPEC @" "generated CONTENTS */\n\n");
	wrapper_output_file(outfile, _PATH_SKELDIR "wrapper-head.c");
	fprintf(outfile, "\n/* START OF SPEC @" "generated CONTENTS */\n\n");

	fprintf(outfile, "/* Long, Logonly Option and Pattern Definitions */\n");
	empty = true;
	LIST_FOREACH(app, &apps, app_entries) {
		bool printed = false;

		if (app->app_longflags != 0) {
			wrapper_write_long_args(outfile, app);
			empty = false;
			printed = true;
		}

		if (app->app_anyarg_logonly) {
			wrapper_write_logonly_args(outfile, app);
			empty = false;
			printed = true;
		}

		if (app->app_short_patterns != 0 ||
		    app->app_long_patterns != 0) {
			wrapper_write_patterns(outfile, app);
			empty = false;
			printed = true;
		}

		if (printed) {
			fprintf(outfile, "\n");
		}
	}

	if (empty)
		fprintf(outfile, "\n");

	fprintf(outfile, "/* Additional Arg Definitions */\n");
	empty = true;
	LIST_FOREACH(app, &apps, app_entries) {
		if (app->app_add_nargv == 0)
			continue;

		wrapper_write_addargs(outfile, app);
		empty = false;
	}

	if (empty)
		fprintf(outfile, "\n");

	fprintf(outfile, "\n/* Application Definitions */\n");
	fprintf(outfile, "static const struct application wrapper_apps[] = {\n");

	LIST_FOREACH(app, &apps, app_entries) {
		fprintf(outfile, "	{\n");
		fprintf(outfile, "		.app_name = \"%s\",\n",
		    app->app_name);
		fprintf(outfile, "		.app_path = \"%s\",\n",
		    app->app_path);
		fprintf(outfile, "		.app_path_relcwd = %s,\n",
		    app->app_path_relcwd ? "true" : "false");
		fprintf(outfile, "		.app_opts_logonly = %s,\n",
		    app->app_argmode_logonly ? "true" : "false");
		if (app->app_add_nargv != 0) {
			fprintf(outfile, "		.app_add_argv = %s,\n",
			    app_addarg_name(app));
			fprintf(outfile,
			    "		.app_add_nargv = nitems(%s),\n",
			    app_addarg_name(app));
		}
		if (!LIST_EMPTY(&app->app_flags))
			wrapper_write_args(outfile, app);
		if (app->app_short_patterns != 0) {
			fprintf(outfile,
			    "		.app_shortopt_expr = %s,\n",
			    app_pattern_name(app, "short"));
		}
		if (app->app_long_patterns != 0) {
			fprintf(outfile,
			    "		.app_longopt_expr = %s,\n",
			    app_pattern_name(app, "long"));
		}

		fprintf(outfile, "	},\n");
	}

	fprintf(outfile, "};\n\n");

	fprintf(outfile, "\n/* END OF SPEC @" "generated CONTENTS */\n\n");
	wrapper_output_file(outfile, _PATH_SKELDIR "wrapper-tail.c");
}
