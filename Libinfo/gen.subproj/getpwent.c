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
 * Copyright 1997 Apple Computer, Inc. (unpublished)
 * 
 * /etc/passwd file access routines.
 * Just read from the /etc/passwd file and skip the dbm database, since
 * lookupd does all flat file lookups when the system is multi-user.
 * These routines are only used in single-user mode.
 *
 * 17 Apr 1997 file created - Marc Majka
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>

#define forever for (;;)

#define _PWENT_ 0
#define _PWNAM_ 1
#define _PWUID_ 2

static struct passwd _pw = { 0 };
static FILE *_pfp;
static int _pwStayOpen;
static int _pwFileFormat = 1;

static void
free_pw()
{
	if (_pw.pw_name != NULL)   free(_pw.pw_name);
	if (_pw.pw_passwd != NULL) free(_pw.pw_passwd);
	if (_pw.pw_class != NULL)  free(_pw.pw_class);
	if (_pw.pw_gecos != NULL)  free(_pw.pw_gecos);
	if (_pw.pw_dir != NULL)    free(_pw.pw_dir);
	if (_pw.pw_shell != NULL)  free(_pw.pw_shell);

	_pw.pw_name = NULL;
	_pw.pw_passwd = NULL;
	_pw.pw_class = NULL;
	_pw.pw_gecos = NULL;
	_pw.pw_dir = NULL;
	_pw.pw_shell = NULL;
}

static void
freeList(char **l)
{
	int i;

	if (l == NULL) return;
	for (i = 0; l[i] != NULL; i++)
	{
		if (l[i] != NULL) free(l[i]);
		l[i] = NULL;
	}
	if (l != NULL) free(l);
}

static unsigned int
listLength(char **l)
{
	int i;

	if (l == NULL) return 0;
	for (i = 0; l[i] != NULL; i++);
	return i;
}

static char *
copyString(char *s)
{
	int len;
	char *t;

	if (s == NULL) return NULL;

	len = strlen(s) + 1;
	t = malloc(len);
	bcopy(s, t, len);
	return t;
}


static char **
insertString(char *s, char **l, unsigned int x)
{
	int i, len;

	if (s == NULL) return l;
	if (l == NULL) 
	{
		l = (char **)malloc(2 * sizeof(char *));
		l[0] = copyString(s);
		l[1] = NULL;
		return l;
	}

	for (i = 0; l[i] != NULL; i++);
	len = i + 1; /* count the NULL on the end of the list too! */

	l = (char **)realloc(l, (len + 1) * sizeof(char *));

	if ((x >= (len - 1)) || (x == (unsigned int)-1))
	{
		l[len - 1] = copyString(s);
		l[len] = NULL;
		return l;
	}

	for (i = len; i > x; i--) l[i] = l[i - 1];
	l[x] = copyString(s);
	return l;
}

static char **
appendString(char *s, char **l)
{
	return insertString(s, l, (unsigned int)-1);
}


static char **
tokenize(const char *data, const char *sep)
{
	char **tokens = NULL;
	const char *p;
	int i, j, len;
	char buf[4096];
	int scanning;

	if (data == NULL) return NULL;
	if (sep == NULL)
	{
		tokens = appendString((char *)data, tokens);
		return tokens;
	}

	len = strlen(sep);

	p = data;

	while (p[0] != '\0')
	{
		/* skip leading white space */
		while ((p[0] == ' ') || (p[0] == '\t') || (p[0] == '\n')) p++;

		/* check for end of line */
		if (p[0] == '\0') break;

		/* copy data */
		i = 0;
		scanning = 1;
		for (j = 0; (j < len) && (scanning == 1); j++)
		{
			if (p[0] == sep[j] || (p[0] == '\0')) scanning = 0;
		}

		while (scanning == 1)
		{
			buf[i++] = p[0];
			p++;
			for (j = 0; (j < len) && (scanning == 1); j++)
			{
				if (p[0] == sep[j] || (p[0] == '\0')) scanning = 0;
			}
		}
	
		/* back over trailing whitespace */
		i--;
		while ((buf[i] == ' ') || (buf[i] == '\t') || (buf[i] == '\n')) i--;
		buf[++i] = '\0';
	
		tokens = appendString(buf, tokens);

		/* check for end of line */
		if (p[0] == '\0') break;

		/* skip separator */
		scanning = 1;
		for (j = 0; (j < len) && (scanning == 1); j++)
		{
			if (p[0] == sep[j])
			{
				p++;
				scanning = 0;
			}
		}

		if ((scanning == 0) && p[0] == '\0')
		{
			/* line ended at a separator - add a null member */
			tokens = appendString("", tokens);
			return tokens;
		}
	}
	return tokens;
}

struct passwd *
parseUser(char *data)
{
	char **tokens;
	int ntokens;

	if (data == NULL) return NULL;

	tokens = tokenize(data, ":");
	ntokens = listLength(tokens);
	if (( _pwFileFormat && (ntokens != 10)) ||
	    (!_pwFileFormat && (ntokens !=  7)))
	{
		freeList(tokens);
		return NULL;
	}

	free_pw();

	_pw.pw_name = tokens[0];
	_pw.pw_passwd = tokens[1];
	_pw.pw_uid = atoi(tokens[2]);
	free(tokens[2]);
	_pw.pw_gid = atoi(tokens[3]);
	free(tokens[3]);

	if (_pwFileFormat)
	{
		_pw.pw_class = tokens[4];
		_pw.pw_change = atoi(tokens[5]);
		free(tokens[5]);
		_pw.pw_expire = atoi(tokens[6]);
		free(tokens[6]);
		_pw.pw_gecos = tokens[7];
		_pw.pw_dir = tokens[8];
		_pw.pw_shell = tokens[9];
	}
	else
	{
		_pw.pw_class = copyString("");
		_pw.pw_change = 0;
		_pw.pw_expire = 0;
		_pw.pw_gecos = tokens[4];
		_pw.pw_dir = tokens[5];
		_pw.pw_shell = tokens[6];
	}

	free(tokens); 

	return &_pw;
}

static char *
getLine(FILE *fp)
{
	char s[1024];
	char *out;

    s[0] = '\0';

    fgets(s, 1024, fp);
    if (s == NULL || s[0] == '\0') return NULL;

	if (s[0] != '#') s[strlen(s) - 1] = '\0';

	out = copyString(s);
	return out;
}

int
setpassent(int stayopen)
{
	_pwStayOpen = stayopen;
	return(1);
}

int
setpwent()
{
	if (_pfp == NULL)
	{
		char *pwFile;
		if (geteuid() == 0)
		{
			pwFile = _PATH_MASTERPASSWD;
		}
		else
		{
			pwFile = _PATH_PASSWD;
			_pwFileFormat = 0;
		}
		_pfp = fopen(pwFile, "r");
		if (_pfp == NULL)
		{
			perror(pwFile);
			return(0);
		}
	}
	else rewind(_pfp);
	_pwStayOpen = 0;
	return(1);
}

void
endpwent()
{
	if (_pfp != NULL)
	{
		fclose(_pfp);
		_pfp = NULL;
	}
}

static struct passwd *
getpw(const char *nam, uid_t uid, int which)
{
	char *line;
	struct passwd *pw;

	if (which != 0)
	{
		if (setpwent() == 0) return NULL;
	}

	forever
	{
		line = getLine(_pfp);
		if (line == NULL) break;

		if (line[0] == '#') 
		{
			free(line);
			line = NULL;
			continue;
		}

		pw = parseUser(line);
		free(line);
		line = NULL;

		if ((pw == NULL) || (which == _PWENT_))
		{
			if (_pwStayOpen == 0) endpwent();
			return pw;
		}

		if (((which == _PWNAM_) && (!strcmp(nam, pw->pw_name))) ||
			((which == _PWUID_) && (uid == pw->pw_uid)))
		{
			if (_pwStayOpen == 0) endpwent();
			return pw;
		}
	}

	if (_pwStayOpen == 0) endpwent();
	return NULL;
}

struct passwd *
getpwent()
{
	return getpw(NULL, 0, _PWENT_);
}

struct passwd *
getpwnam(const char *nam)
{
	return getpw(nam, 0, _PWNAM_);
}

struct passwd *
getpwuid(uid_t uid)
{
	return getpw(NULL, uid, _PWUID_);
}
