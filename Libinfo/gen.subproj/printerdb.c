/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * /etc/printcap reader (in case NetInfo is not running)
 * Copyright (C) 1989 by NeXT, Inc.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <printerdb.h>

static FILE *pf;
static char *getline(FILE *);
static int emptyfield(char *);

static void
_prdb_free_ent(
	       prdb_ent *ent
	       )
{
	int i;
	
	for (i = 0; ent->pe_name[i]; i++) {
		free(ent->pe_name[i]);
	}
	free(ent->pe_name);
	ent->pe_name = NULL;
	for (i = 0; i < ent->pe_nprops; i++) {
		free(ent->pe_prop[i].pp_key);
		free(ent->pe_prop[i].pp_value);
	}
	free(ent->pe_prop);
	ent->pe_prop = NULL;
}

void 
_old_prdb_end(
	      void
	      )
{
	if (pf != NULL) {
		fclose(pf);
		pf = NULL;
	}
}

void
_old_prdb_set(
	      void
	      )
{
	if (pf == NULL) {
		pf = fopen("/etc/printcap", "r");
	}
}

static void
pename_insert(
	      char ***target,
	      char *name,
	      int which
	      )
{
	if (which == 0) {
		*target = malloc(sizeof(char *) * 2);
	} else {
		*target = realloc(*target, sizeof(char *) * (which + 2));
	}
	(*target)[which] = strdup(name);
	(*target)[which + 1] = NULL;
}

static void
peprop_insert(
	      prdb_property **target,
	      prdb_property prop,
	      int which
	      )
{
	if (which == 0) {
		*target = malloc(sizeof(prop));
	} else {
		*target = realloc(*target, (which + 1) * sizeof(prop));
	}
	(*target)[which] = prop;
}

prdb_ent *
_old_prdb_get(
	      void
	      )
{
	char *line;
	char *p;
	char *end;
	char *hash;
	char *equal;
	char *where;
	static prdb_ent ent;
	prdb_property prop;
	int which;

	_old_prdb_set();
	if (pf == NULL) {
		return (NULL);
	}
	do {
		line = getline(pf);
		if (line == NULL) {
			return (NULL);
		}
	} while (*line == 0);
	where = line;
	end = index(where, ':');
	if (end != NULL) {
		*end++ = 0;
	}
	which = 0;
	if (ent.pe_name != NULL) {
		_prdb_free_ent(&ent);
	}
	for (;;) {
		p = index(where, '|');
		if (p != NULL && (end == NULL || p < end)) {
			*p++ = 0;
			pename_insert(&ent.pe_name, where, which++);
			where = p;
		} else {
			pename_insert(&ent.pe_name, where, which);
			break;
		}
	}
	where = end;
	which = 0;
	for (;;) {
		end = index(where, ':');
		if (end != NULL) {
			*end++ = 0;
		}
		hash = index(where, '#');
		equal = index(where, '=');
		if (hash != NULL && (end == NULL || hash < end)) {
			*hash = 0;
			prop.pp_key = strdup(where);
			*hash = '#';
			prop.pp_value = strdup(hash);
			peprop_insert(&ent.pe_prop, prop, which++);
		} else if (equal != NULL && (end == NULL || 
					     equal < end)) {
			*equal++ = 0;
			prop.pp_key = strdup(where);			
			prop.pp_value = strdup(equal);
			peprop_insert(&ent.pe_prop, prop, which++);
		} else if (!emptyfield(where)) {
			prop.pp_key = strdup(where);
			prop.pp_value = strdup("");
			peprop_insert(&ent.pe_prop, prop, which++);
		}
		where = end;
		if (end == NULL) {
			break;
		}
	}
	free(line);
	ent.pe_nprops = which;
	return (&ent);
}

static int
prmatch(
	prdb_ent *ent,
	char *name
	)
{
	int i;

	for (i = 0; ent->pe_name[i] != NULL; i++) {
		if (strcmp(ent->pe_name[i], name) == 0) {
			return (1);
		}
	}
	return (0);
}

prdb_ent *
_old_prdb_getbyname(
		    char *prname
		    )
{
	prdb_ent *ent;

	_old_prdb_set();
	if (pf == NULL) {
		return (NULL);
	}
	while ((ent = _old_prdb_get())) {
		if (prmatch(ent, prname)) {
			break;
		}
	}
	_old_prdb_end();
	return (ent);
}



static char *
getline(
	FILE *f
	)
{
	char line[BUFSIZ];
	char *res = NULL;
	int more = 1;
	int len;
	int inclen;

	len = 0;
	while (more && fgets(line, sizeof(line), f)) {
		inclen = strlen(line);
		if (line[inclen - 1] == '\n') {
			line[inclen - 1] = 0;
			inclen--;
		}
		if (*line == '#') {
			continue;
		}
		if (res == NULL) {
			res = malloc(inclen + 1);
		} else {
			res = realloc(res, len + inclen + 1);
		}
		if (line[inclen - 1] == '\\') {
			line[inclen - 1] = 0;
			inclen--;
		} else {
			more = 0;
		}
		bcopy(line, res + len, inclen);
		len += inclen;
		res[len] = 0;
	}
	return (res);
}

static int
emptyfield(
	   char *line
	   )
{
	while (*line) {
		if (*line != ' ' && *line != '\t') {
			return (0);
		}
		line++;
	}
	return (1);
}
