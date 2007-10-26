/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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
 * Directory Service does all flat file lookups when the system is multi-user.
 * These routines are only used in single-user mode.
 *
 * 17 Apr 1997 file created - Marc Majka
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define forever for (;;)

#define _PWENT_ 0
#define _PWNAM_ 1
#define _PWUID_ 2

static struct passwd _pw = { 0 };
static FILE *_pfp = NULL;
static int _pwFileFormat = 1;

#define _HENT_  0
#define _HNAM_  1
#define _HADDR_ 2

static struct hostent _h = { 0 };
static FILE *_hfp = NULL;

/* Forward */
__private_extern__ void LI_files_setpwent();
__private_extern__ void LI_files_endhostent();

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
		if (i > -1) { /* did we actually copy anything? */
			while ((buf[i] == ' ') || (buf[i] == '\t') || (buf[i] == '\n')) i--;
		}
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

static char *
getLine(FILE *fp)
{
	char s[1024];
	char *out;

	s[0] = '\0';

	fgets(s, 1024, fp);
	if ((s == NULL) || (s[0] == '\0')) return NULL;

	if (s[0] != '#') s[strlen(s) - 1] = '\0';

	out = copyString(s);
	return out;
}

/* USERS */

static void
LI_files_free_user()
{
	if (_pw.pw_name != NULL) free(_pw.pw_name);
	if (_pw.pw_passwd != NULL) free(_pw.pw_passwd);
	if (_pw.pw_class != NULL) free(_pw.pw_class);
	if (_pw.pw_gecos != NULL) free(_pw.pw_gecos);
	if (_pw.pw_dir != NULL) free(_pw.pw_dir);
	if (_pw.pw_shell != NULL) free(_pw.pw_shell);

	_pw.pw_name = NULL;
	_pw.pw_passwd = NULL;
	_pw.pw_class = NULL;
	_pw.pw_gecos = NULL;
	_pw.pw_dir = NULL;
	_pw.pw_shell = NULL;
}

static struct passwd *
LI_files_parse_user(char *data)
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

	LI_files_free_user();

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

static struct passwd *
LI_files_getpw(const char *name, uid_t uid, int which)
{
	char *line;
	struct passwd *pw;

	if (_pfp == NULL) LI_files_setpwent();
	if (_pfp == NULL) return NULL;

	if (which != _PWENT_) rewind(_pfp);

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

		pw = LI_files_parse_user(line);
		free(line);
		line = NULL;

		if (pw == NULL) continue;

		if (which == _PWENT_) return pw;

		if (((which == _PWNAM_) && (!strcmp(name, pw->pw_name))) || ((which == _PWUID_) && (uid == pw->pw_uid)))
		{
			fclose(_pfp);
			_pfp = NULL;
			return pw;
		}
	}

	fclose(_pfp);
	_pfp = NULL;

	return NULL;
}

/* "Public" */

__private_extern__ struct passwd *
LI_files_getpwent()
{
	return LI_files_getpw(NULL, 0, _PWENT_);
}

__private_extern__ struct passwd *
LI_files_getpwnam(const char *name)
{
	return LI_files_getpw(name, 0, _PWNAM_);
}

__private_extern__ struct passwd *
LI_files_getpwuid(uid_t uid)
{
	return LI_files_getpw(NULL, uid, _PWUID_);
}

int
setpassent(int stayopen)
{
	return 1;
}

__private_extern__ void
LI_files_setpwent()
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
	}
	else rewind(_pfp);
}

__private_extern__ void
LI_files_endpwent()
{
	if (_pfp != NULL)
	{
		fclose(_pfp);
		_pfp = NULL;
	}
}

/* HOSTS */

static void
LI_files_free_host()
{
	int i;

	if (_h.h_name != NULL) free(_h.h_name);

	if (_h.h_aliases != NULL)
	{
		for (i = 0; _h.h_aliases[i] != NULL; i++) free(_h.h_aliases[i]);
		free(_h.h_aliases);
	}

	if (_h.h_addr_list != NULL)
	{
		for (i = 0; _h.h_addr_list[i] != NULL; i++) free(_h.h_addr_list[i]);
		free(_h.h_addr_list);
	}

	_h.h_name = NULL;
	_h.h_aliases = NULL;
	_h.h_addrtype = 0;
	_h.h_length = 0;
	_h.h_addr_list = NULL;
}

static struct hostent *
LI_files_parse_host(char *data)
{
	char **tokens, *addrstr;
	int i, ntokens, af;
	struct in_addr a4;
	struct in6_addr a6;

	if (data == NULL) return NULL;

	tokens = tokenize(data, " 	");
	ntokens = listLength(tokens);
	if (ntokens < 2)
	{
		freeList(tokens);
		return NULL;
	}

	LI_files_free_host();

	af = AF_UNSPEC;
	if (inet_pton(AF_INET, tokens[0], &a4) == 1) af = AF_INET;
	else if (inet_pton(AF_INET6, tokens[0], &a6) == 1) af = AF_INET6;

	if (af == AF_UNSPEC)
	{
		freeList(tokens);
		return NULL;
	}

	addrstr = tokens[0];

	_h.h_addrtype = af;
	if (af == AF_INET)
	{
		_h.h_length = sizeof(struct in_addr);
		_h.h_addr_list = (char **)calloc(2, sizeof(char *));
		_h.h_addr_list[0] = (char *)calloc(1, _h.h_length);
		memcpy(_h.h_addr_list[0], &a4, _h.h_length);
	}
	else
	{
		_h.h_length = sizeof(struct in6_addr);
		_h.h_addr_list = (char **)calloc(2, sizeof(char *));
		_h.h_addr_list[0] = (char *)calloc(1, _h.h_length);
		memcpy(_h.h_addr_list[0], &a6, _h.h_length);
	}

	_h.h_name = tokens[1];

	_h.h_aliases = (char **)calloc(ntokens - 1, sizeof(char *));
	for (i = 2; i < ntokens; i++) _h.h_aliases[i-2] = tokens[i];

	free(addrstr);
	free(tokens);

	return &_h;
}

static struct hostent *
LI_files_get_host(const char *name, const void *addr, int af, int which)
{
	char *line;
	struct hostent *h;
	int i, got_host;

	if ((which == _HADDR_) && (addr == NULL)) return NULL;

	if (_hfp == NULL) _hfp = fopen(_PATH_HOSTS, "r");
	if (_hfp == NULL) return NULL;

	if (which != _HENT_) rewind(_hfp);

	forever
	{
		line = getLine(_hfp);
		if (line == NULL) break;

		if (line[0] == '#') 
		{
			free(line);
			line = NULL;
			continue;
		}

		h = LI_files_parse_host(line);
		free(line);
		line = NULL;

		if (h == NULL) continue;

		if (which == _HENT_) return h;

		got_host = 0;

		if ((which == _HNAM_) && (af == h->h_addrtype))
		{
			if (!strcmp(name, h->h_name)) got_host = 1;
			else if (h->h_aliases != NULL)
			{
				for (i = 0; (h->h_aliases[i] != NULL) && (got_host == 0); i++)
					if (!strcmp(name, h->h_aliases[i])) got_host = 1;
			}
		}

		if ((which == _HADDR_) && (h->h_addrtype == af))
		{
			for (i = 0; (h->h_addr_list[i] != NULL) && (got_host == 0); i++)
				if (memcmp(addr, h->h_addr_list[i], h->h_length) == 0) got_host = 1;
		}

		if (got_host == 1)
		{
			fclose(_hfp);
			_hfp = NULL;
			return h;
		}
	}

	fclose(_hfp);
	_hfp = NULL;

	return NULL;
}

/* "Public" */

__private_extern__ struct hostent *
LI_files_gethostbyname(const char *name)
{
	return LI_files_get_host(name, NULL, AF_INET, _HNAM_);
}

__private_extern__ struct hostent *
LI_files_gethostbyname2(const char *name, int af)
{
	return LI_files_get_host(name, NULL, af, _HNAM_);
}

__private_extern__ struct hostent *
LI_files_gethostbyaddr(const void *addr, socklen_t len, int type)
{
	if ((type == AF_INET) || (type == AF_INET6)) return LI_files_get_host(NULL, addr, type, _HADDR_);
	return NULL;
}

__private_extern__ struct hostent *
LI_files_gethostent()
{
	return LI_files_get_host(NULL, NULL, AF_UNSPEC, _HENT_);
}

__private_extern__ void
LI_files_sethostent(int stayopen)
{
}

__private_extern__ void
LI_files_endhostent()
{
	if (_hfp != NULL)
	{
		fclose(_hfp);
		_hfp = NULL;
	}
}
