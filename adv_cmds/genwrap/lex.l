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

#include <sys/param.h>

#include "y.tab.h"
#include "genwrap.h"

#ifndef nitems
#define	nitems(x)	(sizeof((x)) / sizeof((x)[0]))
#endif

struct kword {
	const char	*kword_name;
	int		 kword_val;
} keywords[] = {
	{ "analytics",		ANALYTICS },
	{ "noargs",		NOARGS },
	{ "application",	APPLICATION },
	{ "cwdpath",		CWDPATH },
	{ "path",		PATH },
	{ "addarg",		ADDARG },
	{ "flag",		FLAG },
	{ "default",		DEFAULT },
	{ "arg",		ARG },
	{ "optional_arg",	OPTARG },
	{ "env",		ENV },
	{ "argmode",		ARGMODE },
	{ "logonly",		LOGONLY },
	{ "pattern",		PATTERN },
};

int kword_lookup(const char *);

%}

%option nounput
%option noinput
%option noyywrap

ID	[A-Za-z_][-A-Za-z_0-9]*

%%

{ID}		{
			int i;

			BEGIN 0;
			if ((i = kword_lookup(yytext)) == -1) {
				yylval.str = strdup(yytext);
				return ID;
			}

			return i;
		}
\"[^"]*\"	{
			BEGIN 0;
			yytext[yyleng-1] = '\0';
			yylval.str = strdup(yytext + 1);
			return ID;
		}
'[^']*'		{
			BEGIN 0;
			yytext[yyleng-1] = '\0';
			yylval.str = strdup(yytext + 1);
			return ID;
		}
\n/[ \t]	{	yyline++;		}
\n		{	yyline++;		}
#.*		{	/* Comment */;		}
[ \t\f]*	{	/* Whitespace */;	}
.		{	return yytext[0];	}
%%

int
kword_lookup(const char *word)
{
	struct kword *kw;

	for (size_t i = 0; i < nitems(keywords); i++) {
		kw = &keywords[i];

		if (strcmp(kw->kword_name, word) == 0)
			return (kw->kword_val);
	}

	return (-1);
}
