%union {
	char	*str;
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

%token	<str>	ID

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
#include <string.h>

#include "genwrap.h"

const char *yyfile;
int yyline;

static struct app *current_app;
static void addflag(const char *first, const char *second, int argument);

struct stringlist {
	STAILQ_ENTRY(stringlist)	entries;
	char				*str;
};

STAILQ_HEAD(stringhead, stringlist);

static int string_count;
static STAILQ_HEAD(, stringlist) current_stringlist =
    STAILQ_HEAD_INITIALIZER(current_stringlist);

static void stringlist_init(const char *str);
static void stringlist_append(const char *str);
static void stringlist_done(char ***, int *);
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
	FLAG ID {
		addflag($2, NULL, no_argument);
	} |
	FLAG ID ARG {
		addflag($2, NULL, required_argument);
	} |
	FLAG ID OPTARG {
		addflag($2, NULL, optional_argument);
	} |
	FLAG ID ID {
		addflag($2, $3, no_argument);
	} |
	FLAG ID ID ARG {
		addflag($2, $3, required_argument);
	} |
	FLAG ID ID OPTARG {
		addflag($2, $3, optional_argument);
	}
%%

static void
addflag(const char *first, const char *second, int argument)
{
	if (first[0] == '\0')
		yyerror("provided flag must not be empty");
	if (second != NULL && second[0] == '\0')
		yyerror("provided alias must not be empty");

	if (second == NULL) {
		app_add_flag(current_app, first, NULL, argument);
		return;
	}

	/* Allow whatever order for flag, alias. */
	if (first[1] != '\0')
		app_add_flag(current_app, first, second, argument);
	else
		app_add_flag(current_app, second, first, argument);
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
