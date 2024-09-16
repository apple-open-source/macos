%union {
	char	*str;
	int	 num;
	unsigned int	flag;
}

%token	ANALYTICS
%token	APPLICATION
%token	DEFAULT
%token	NOARGS
%token	PATH
%token	ADDARG
%token	CWDPATH
%token	FLAG
%token	ARG
%token	OPTARG
%token	ENV
%token	ARGMODE
%token	LOGONLY
%token	PATTERN

%token	<str>	ID

%type	<num>	flag_argspec;
%type	<flag>	flag_flags;

%{
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

#include <sys/queue.h>

#include <assert.h>
#include <err.h>
#include <regex.h>
#include <string.h>

#include "genwrap.h"

const char *yyfile;
int yyline;

static struct app *current_app;

struct stringlist {
	STAILQ_ENTRY(stringlist)	entries;
	char				*str;
};

static struct flagspec {
	const char	*flag;
	const char	*alias;
	const char	*pattern;
	int		 argument;
	uint32_t	 argflags;
} flagspec;

STAILQ_HEAD(stringhead, stringlist);

static int string_count;
static STAILQ_HEAD(, stringlist) current_stringlist =
    STAILQ_HEAD_INITIALIZER(current_stringlist);

static void stringlist_init(const char *str);
static void stringlist_append(const char *str);
static void stringlist_done(char ***, int *);

static void addflag(struct flagspec *sp);
static char *checkpattern(const char *pat);
%}
%%

configuration:
	wrapper_settings application_specs | application_specs

wrapper_settings:
	wrapper_setting | wrapper_setting wrapper_settings

wrapper_setting:
	analytic_spec | environment_spec

analytic_spec:
	ANALYTICS ID {
		wrapper_set_analytics($2, false);
	} |
	ANALYTICS ID NOARGS {
		wrapper_set_analytics($2, true);
	}

environment_spec:
	ENV ID {
		wrapper_set_envvar($2);
	}

string_list:
	ID {
		stringlist_init($1);
	} |
	string_list ID {
		stringlist_append($2);
	}

application_specs:
	application_spec | application_spec application_specs

application_spec:
	APPLICATION ID {
		if (current_app != NULL && app_get_path(current_app) == NULL)
			yyerror("previous application block is missing a path");
		current_app = app_add(current_app, $2);
	} |
	DEFAULT {
		app_set_default(current_app);
	} |
	PATH ID {
		app_set_path(current_app, $2, false);
	} |
	ADDARG string_list {
		char **argv;
		int nargv;

		stringlist_done(&argv, &nargv);
		app_add_addarg(current_app, argv, nargv);

		/*
		 * current_app took possession of all of the entries in argv,
		 * all we need to do is cleanup the array itself; it should just
		 * be an array of NULLs after the above call.
		 */
#ifndef NDEBUG
		for (int i = 0; i < nargv; i++) {
			assert(argv[i] == NULL);
		}
#endif
		free(argv);
	} |
	ARGMODE LOGONLY {
		app_set_argmode_logonly(current_app);
	} |
	CWDPATH ID {
		if ($2[0] == '/')
			yyerror("cwd paths must be relative");
		app_set_path(current_app, $2, true);
	} |
	full_argspec {
		addflag(&flagspec);
	}

full_argspec:
	basic_flagspec | basic_flagspec flagspec_extension

flagspec_extension:
	flagspec_extension flagspec_extension |
	flag_argspec {
		flagspec.argument = $1;
	} |
	flag_flags {
		flagspec.argflags |= $1;
	} |
	PATTERN ID {
		flagspec.pattern = checkpattern($2);
		/* Should not have allocated memory. */
		assert(flagspec.pattern == $2);
	}

basic_flagspec:
	FLAG ID {
		memset(&flagspec, 0, sizeof(flagspec));
		flagspec.flag = $2;
	} |
	FLAG ID ID {
		memset(&flagspec, 0, sizeof(flagspec));
		flagspec.flag = $2;
		flagspec.alias = $3;
	}

flag_argspec:
	ARG {
		$$ = required_argument;
	} |
	OPTARG {
		$$ = optional_argument;
	}

flag_flags:
	flag_flags '|' flag_flags {
		$$ = $1 | $3;
	} |
	LOGONLY {
		$$ = ARGFLAG_LOGONLY;
	}
%%

static void
addflag(struct flagspec *fs)
{
	if (fs->flag[0] == '\0')
		yyerror("provided flag must not be empty");
	if (fs->alias != NULL && fs->alias[0] == '\0')
		yyerror("provided alias must not be empty");

	/* Allow whatever order for flag, alias. */
	if (fs->flag[1] != '\0' || fs->alias == NULL) {
		app_add_flag(current_app, fs->flag, fs->alias, fs->argument,
		    fs->argflags, fs->pattern);
	} else {
		app_add_flag(current_app, fs->alias, fs->flag, fs->argument,
		    fs->argflags, fs->pattern);
	}

	/* Pattern memory now belongs to someone else. */
	fs->pattern = NULL;
}

void
yyerror(const char *s)
{

	errx(1, "%s:%d: %s", yyfile, yyline + 1, s);
}

static void
stringlist_init(const char *str)
{

	assert(STAILQ_EMPTY(&current_stringlist));
	string_count = 0;

	stringlist_append(str);
}

static void
stringlist_append(const char *str)
{
	struct stringlist *next;

	next = malloc(sizeof(*next));
	if (next == NULL)
		yyerror("out of memory");

	next->str = strdup(str);
	if (next->str == NULL)
		yyerror("out of memory");

	STAILQ_INSERT_TAIL(&current_stringlist, next, entries);
	string_count++;
}

static void
stringlist_done(char ***outlist, int *nelem)
{
	struct stringlist *elem;
	char **out;
	int n;

	assert(string_count != 0);

	out = malloc(sizeof(*out) * string_count);
	if (out == NULL)
		yyerror("out of memory");

	n = 0;
	while (!STAILQ_EMPTY(&current_stringlist)) {
		elem = STAILQ_FIRST(&current_stringlist);
		STAILQ_REMOVE_HEAD(&current_stringlist, entries);

		/*
		 * Transfer ownership of elem->str here, free the containing
		 * stringlist entry.
		 */
		out[n++] = elem->str;
		free(elem);

		string_count--;
	}

	assert(string_count == 0);
	*outlist = out;
	*nelem = n;
}

static char *
checkpattern(const char *pat)
{
	regex_t reg;
	int error;

	if (*pat == '\0')
		yyerror("pattern must not be empty");

	/*
	 * Try to compile it as an ERE, so that we have some idea up front if
	 * it is basically sane or not.
	 */
	if ((error = regcomp(&reg, pat, REG_EXTENDED | REG_NOSUB)) != 0) {
		char errbuf[128];
		size_t errsz;

		errsz = regerror(error, NULL, errbuf, sizeof(errbuf));
		fprintf(stderr, "pattern error: %s%s\n", errbuf,
		    errsz > sizeof(errbuf) ? " [...]" : "");
		yyerror("failed to compile pattern");
	}

	regfree(&reg);
	return (pat);
}
