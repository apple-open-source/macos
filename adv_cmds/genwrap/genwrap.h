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

#ifndef GENWRAP_H
#define	GENWRAP_H

#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>

/* Parser bits */
void yyerror(const char *s);
int yylex(void);
int yyparse(void);

extern FILE		*yyin;
extern const char	*yyfile;
extern int		yyline;

/* Application logic */
struct app;
#define	ARGFLAG_LOGONLY	0x0001

/* Don't add these flags to aliases. */
#define	ARGFLAG_NO_ALIAS	(ARGFLAG_LOGONLY)

struct app *app_add(struct app *current_app, const char *name);
void app_set_default(struct app *app);
void app_set_argmode_logonly(struct app *app);
void app_add_addarg(struct app *app, const char **argv, int nargv);
void app_set_path(struct app *app, const char *path, bool relcwd);
const char *app_get_path(const struct app *app);
void app_add_flag(struct app *app, const char *flag, const char *alias,
    int argument, uint32_t flags, const char *pattern);

void wrapper_set_analytics(const char *id, bool noargs);
void wrapper_set_envvar(const char *var);

#endif	/* GENWRAP_H */
