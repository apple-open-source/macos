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
 * Useful stuff for programming netinfo
 * Copyright (C) 1989 by NeXT, Inc.
 */
#include <stdlib.h>
#include <netinfo/ni.h>
#include <string.h>
#include <ctype.h>
#include <netdb.h>
#include <stdio.h>
#include <arpa/inet.h>

extern void *_ni_dup(void *);

static const char *
eatslash(
	 const char *path
	 )
{
	while (*path == '/') {
		path++;
	}
	return (path);
}

static void
unescape(
	 ni_name *name
	 )
{
	ni_name newname;
	ni_name p;
	int len;
	int i;

	p = *name;
	len = strlen(p);
	newname = malloc(len + 1);
	for (i = 0; *p != 0; i++) {
		if (*p == '\\') {
			p++;
		}
		newname[i] = *p++;
	}
	ni_name_free(name);
	newname[i] = 0;
	*name = newname;
}

static const char *
escindex(
	 const char *str,
	 char ch
	 )
{
	char *p;

	p = index(str, ch);
	if (p == NULL) {
		return (NULL);
	}
	if (p == str) {
		return (p);
	}
	if (p[-1] == '\\') {
		return (escindex(p + 1, ch));
	}
	return (p);
}

static void
setstuff(
	 void *ni,
	 ni_fancyopenargs *args
	 )
{
	if (args != NULL) {
	  	ni_setabort(ni, args->abort);
	  	if (args->rtimeout) {
	    		ni_setreadtimeout(ni, args->rtimeout);
		}
	  	if (args->wtimeout) {
			ni_setwritetimeout(ni, args->wtimeout);
		}
	}
}

static void *
ni_relopen(
	   void *ni,
	   const char *domain,
	   int freeold,
	   ni_fancyopenargs *args
	   )
{
	void *newni;
	void *tmpni;
	char *start;
	char *slash;
	char *component;

	/* look for <tag>@<address> in last component of domain */
	start = (char *)domain;
	while ((slash = (char *)escindex(start, '/')) != NULL) {
		/* found a slash, keep looking for the last one */
		start = slash + 1;
	}
	if (index(start, '@') != NULL) {
		/*
		 * last component in <tag>@<address> form, skip
		 * all of the leading components.
		 */
		component = ni_name_dup(start);
		newni = ni_new(NULL, component);
		free(component);
		if (newni != NULL && args != NULL)
			setstuff(newni, args);
		if (ni != NULL && freeold)
			ni_free(ni);
		return (newni);
	}

	component = ni_name_dup(domain);
	slash = (char *)escindex(component, '/');
	if (slash != NULL) {
		*slash = 0;
	}
	unescape(&component);

	tmpni = NULL;
	if (ni != NULL && args != NULL) {
	  	tmpni = _ni_dup(ni);
		if (freeold) {
			ni_free(ni);
		}
		ni = tmpni;
		setstuff(ni, args);
	}

	newni = ni_new(ni, component);
	free(component);

	if (tmpni != NULL) {
		ni_free(ni);
		ni = NULL;
	}

	if (ni != NULL && freeold) {
		ni_free(ni);
	}


	if (newni == NULL) {
		return (NULL);
	}
	setstuff(newni, args);
	ni = newni;
	if (slash != NULL) {
	  	slash = (char *)escindex(domain, '/');
		domain = eatslash(slash + 1);
		return (ni_relopen(ni, domain, TRUE, NULL));
	} else {
		return (ni);
	}
}

static void *
ni_rootopen(
	    ni_fancyopenargs *args
	    )
{
	void *ni;
	void *newni;

	ni = ni_new(NULL, ".");
	if (ni == NULL) {
		return (NULL);
	}
	setstuff(ni, args);
	for (;;) {
		newni = ni_new(ni, "..");
		if (newni == NULL) {
			break;
		}
		ni_free(ni);
		ni = newni;
	}
	return (ni);
}

static ni_name
ni_name_dupn(
	     ni_name_const start,
	     ni_name_const stop
	     )
{
	int len;
	ni_name new;

	if (stop != NULL) {
	  	len = stop - start;
	} else {
		len = strlen(start);
	}
	new = malloc(len + 1);
	bcopy(start, new, len);
	new[len] = 0;
	return (new);
}

       
static ni_status
ni_relsearch(
	     void *ni,
	     const char *path,
	     ni_id *id
	     )
{
	char *slash;
	char *equal;
	ni_name key;
	ni_name val;
	ni_idlist idl;
	ni_status status;

	slash = (char *)escindex(path, '/');
	equal = (char *)escindex(path, '=');
	if (equal != NULL && (slash == NULL || equal < slash)) {
		key = ni_name_dupn(path, equal);
		val = ni_name_dupn(equal + 1, slash);
	} else {
		if (equal == NULL || (slash != NULL && slash < equal)) {
			key = ni_name_dup("name");
			val = ni_name_dupn(path, slash);
		} else {
			key = ni_name_dupn(path, equal);
			val = ni_name_dupn(equal + 1, slash);
		}
	}
	unescape(&key);
	unescape(&val);
	NI_INIT(&idl);
	status = ni_lookup(ni, id, key, val, &idl);
	if (status != NI_OK) {
	  	ni_name_free(&key);
		ni_name_free(&val);
		return (status);
	}
	id->nii_object = idl.niil_val[0];
	ni_name_free(&key);
	ni_name_free(&val);
	ni_idlist_free(&idl);
	if (slash == NULL) {
		ni_self(ni, id);
		return (NI_OK);
	}
	path = eatslash(slash);
	return (ni_relsearch(ni, path, id));
}

ni_status
ni_open(
	void *ni,
	const char *domain,
	void **newni
	)
{
	return (ni_fancyopen(ni, domain, newni, NULL));
}

ni_status
ni_fancyopen(
	     void *ni,
	     const char *domain,
	     void **newni,
	     ni_fancyopenargs *args
	     )
{
	void *tmp = NULL;
	int rootopen = 0;

	if (*domain == '/') {
		tmp = ni_rootopen(args);
		if (tmp == NULL) {
		    return (NI_FAILED); /* XXX: should return real error */
		}
		domain = eatslash(domain);
		ni = tmp;
		rootopen++;
	}
	if (*domain != 0) {
		tmp = ni_relopen(ni, domain, FALSE, args);
		if (rootopen) {
			ni_free(ni);
		}
	}
	if (tmp == NULL) {
	    return (NI_FAILED);
	}
	*newni = tmp;
	ni_needwrite(*newni, args == NULL ? 0 : args->needwrite);
	return (NI_OK);
}

ni_status
ni_pathsearch(
	      void *ni, 
	      ni_id *id,
	      const char *path
	      )
{
	ni_status status;

	if (*path == '/') {
		status = ni_root(ni, id);
		if (status != NI_OK) {
			return (status);
		}
	}
	path = eatslash(path);
	if (*path != 0) {
		status = ni_relsearch(ni, path, id);
		if (status != NI_OK) {
			return (status);
		}
	}
	return (NI_OK);
}

static char **
_ni_append_string(char *s, char **l)
{
	int i, len;

	if (s == NULL) return l;
	if (l == NULL) 
	{
		l = (char **)malloc(2 * sizeof(char *));
		l[0] = strdup(s);
		l[1] = NULL;
		return l;
	}

	for (i = 0; l[i] != NULL; i++);
	len = i + 1; /* count the NULL on the end of the list too! */

	l = (char **)realloc(l, (len + 1) * sizeof(char *));

	l[len - 1] = strdup(s);
	l[len] = NULL;
	return l;
}

static char **
_ni_explode_string(char *s, char c)
{
	char **l = NULL;
	char *p, *t;
	int i, n;

	if (s == NULL) return NULL;

	p = s;
	while (p[0] != '\0')
	{
		for (i = 0; ((p[i] != '\0') && p[i] != c); i++);
		n = i;
		t = malloc(n + 1);
		for (i = 0; i < n; i++) t[i] = p[i];
		t[n] = '\0';
		l = _ni_append_string(t, l);
		free(t);
		t = NULL;
		if (p[i] == '\0') return l;
		if (p[i + 1] == '\0') l = _ni_append_string("", l);
		p = p + i + 1;
	}
	return l;
}

static void
_ni_free_list(char **l)
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

ni_status
ni_host_domain(char *host, char *domspec, void **domain)
{
	void *d0, *d1;
	struct sockaddr_in server;
	ni_name tag;
	int i, is_tag, is_local, is_relative;
	ni_status status;
	char **path;
	struct hostent *h;

	is_local = 1;

	/* NULL host implies localhost */
	if (host == NULL)
	{
		server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	}
	else
	{
		is_local = 0;
		server.sin_addr.s_addr = inet_addr(host);
		if (server.sin_addr.s_addr == -1)
		{
			h = gethostbyname(host);
			if (h == NULL)
			{
				*domain = (void *)NULL;
				return NI_CANTFINDADDRESS;
			}
			bcopy(h->h_addr_list[0], &server.sin_addr.s_addr, h->h_length);
		}
	}

	is_relative = 1;
	is_tag = 1;

	if (domspec == NULL)
	{
		is_tag = 0;
		is_relative = 0;
	}
	else if (domspec[0] == '/')
	{
		is_tag = 0;
		is_relative = 0;
	}
	else if (!strcmp(domspec, ".")) is_tag = 0;
	else if (!strcmp(domspec, "..")) is_tag = 0;
	else if (!strncmp(domspec, "./", 2)) is_tag = 0;
	else if (!strncmp(domspec, "../", 3)) is_tag = 0;

	if (is_tag == 1)
	{
		d0 = ni_connect(&server, domspec);
		status = ni_addrtag(d0, &server, &tag);
		ni_name_free(&tag);
		if (status != NI_OK)
		{
			*domain = (void *)NULL;
			return NI_FAILED;
		}

		*domain = d0;
		return NI_OK;
	}

	if (is_local)
	{
		if (domspec == NULL) status = ni_open(NULL, ".", domain);
		else status = ni_open(NULL, domspec, domain);
		return status;
	}

	d0 = ni_connect(&server, "local");
	status = ni_addrtag(d0, &server, &tag);
	ni_name_free(&tag);
	if (status != NI_OK)
	{
		*domain = (void *)NULL;
		return NI_FAILED;
	}

	if ((domspec == NULL) || (!strcmp(domspec, ".")))
	{
		*domain = d0;
		return NI_OK;
	}

	if (is_relative == 1)
	{
		path = _ni_explode_string(domspec, '/');
	}
	else
	{
		path = _ni_explode_string(domspec + 1, '/');

		status = NI_OK;
		while (status == NI_OK)
		{
			status = ni_open(d0, "..", &d1);
			if (status == NI_OK)
			{
				ni_free(d0);
				d0 = d1;
			}
		}

		if (!strcmp(domspec, "/"))
		{
			*domain = d0;
			return NI_OK;
		}
	}

	for (i = 0; path[i] != NULL; i++)
	{
		status = ni_open(d0, path[i], &d1);
		if (status != NI_OK)
		{
			_ni_free_list(path);
			*domain = (void *)NULL;
			return NI_FAILED;
		}
		ni_free(d0);
		d0 = d1;
	}

	_ni_free_list(path);
	*domain = d0;
	return NI_OK;
}

static void
_ni_parse_url_hostspec(char *s, char **u, char **p, char **h)
{

	char *p_at, *p_colon;
	int ulen, plen, hlen;

	if (s == NULL) return;
	if (s[0] == '\0') return;

	/* Check for [[[user][:[passwd]]]@]host */
	p_at = strchr(s, '@');
	if (p_at == NULL)
	{
		hlen = strlen(s);
		if (hlen == 0) return;

		*h = malloc(hlen + 1);
		strcpy(*h, s);
		return;
	}

	*p_at = '\0';
	p_at++;
	hlen = strlen(p_at);
	if (hlen > 0)
	{
		*h = malloc(hlen + 1);
		strcpy(*h, p_at);
	}

	if (s[0] == '\0') return;

	p_colon = strchr(s, ':');
	if (p_colon == NULL)
	{
		ulen = strlen(s);
		if (ulen == 0) return;

		*u = malloc(ulen + 1);
		strcpy(*u, s);
		return;
	}

	*p_colon = '\0';
	p_colon++;
	plen = strlen(p_colon);
	if (plen > 0)
	{
		*p = malloc(plen + 1);
		strcpy(*p, p_colon);
	}

	ulen = strlen(s);
	if (ulen > 0)
	{
		*u = malloc(ulen + 1);
		strcpy(*u, s);
	}
}

void
ni_parse_url(char *url, char **user, char **password, char **host,
	char **domspec, char **dirspec)
{
	int i, x, len;
	char *str;

	*host = NULL;
	*user = NULL;
	*password = NULL;
	*domspec = NULL;
	*dirspec = NULL;

	/*
	 * url ::= "netinfo://" <hostspec> [/[<domainspec>][:[<dirspec>]]]
	 * hostspec ::= [[[user][:[password]]]@]hostref
	 * hostref ::= <inet_addr> | <hostname>
	 * domainspec ::= <abs_domain> | <rel_domain>
	 * dirspec ::= <path> | <unsigned_integer>
	 */

	x = strlen("netinfo://");

	if (strncmp(url, "netinfo://", x)) return;

	/*
	 * Look for <hostspec> part
	 * Defults to NULL user, password and host
	 * NULL host implies localhost
	 */
	len = 0;
	for (i = x; (url[i] != '\0') && (url[i] != '/'); i++) len++;

	if (len != 0)
	{
		str = malloc(len + 1);
		bcopy(url + x, str, len);
		str[len] = '\0';

		_ni_parse_url_hostspec(str, user, password, host);
	
		free(str);
	}

	/* 
	 * Look for <domainspec> part
	 * NULL domainspec implies "."
	 */
	if (url[i] != '\0') i++;
	x = i;
	len = 0;
	for (; (url[i] != '\0') && (url[i] != ':'); i++) len++;

	if (len > 0)
	{
		*domspec = malloc(len + 1);
		bcopy(url + x, *domspec, len);
		(*domspec)[len] = '\0';
	}

	/* 
	 * Look for <dirspec> part
	 * NULL <dirspec> implies "/"
	 */
	if (url[i] != '\0') i++;
	x = i;
	len = 0;
	for (; url[i] != '\0'; i++) len++;
	if (len > 0)
	{
		*dirspec = malloc(len + 1);
		bcopy(url + x, *dirspec, len);
		(*dirspec)[len] = '\0';
	}
}

ni_status
ni_url(char *url, void **domain, ni_id *dir)
{
	int nilen;
	char *user, *password, *host;
	char *domspec, *dirspec;
	ni_status status;

	nilen = strlen("netinfo://");

	if (strncmp(url, "netinfo://", nilen))
	{
		*domain = (void *)NULL;
		return NI_CANTFINDADDRESS;
	}

	ni_parse_url(url, &user, &password, &host, &domspec, &dirspec);

	status = ni_host_domain(host, domspec, domain);
	if (host != NULL) free(host);
	if (domspec != NULL) free(domspec);
	if (status != NI_OK)
	{
			if (user != NULL) free(user);
			if (password != NULL) free(password);
			if (dirspec != NULL) free(dirspec);
			return status;
	}

	if (user != NULL)
	{
		ni_setuser(*domain, user);
		free(user);
	}

	if (password != NULL)
	{
		ni_setpassword(*domain, password);
		free(password);
	}

	if (dirspec == NULL)
	{
		status = ni_root(*domain, dir);
		return status;
	}

	if ((dirspec[0] >= '0') && (dirspec[0] <= '9'))
	{
		dir->nii_object = atoi(dirspec);
		free(dirspec);
		status = ni_self(*domain, dir);
		return status;
	}

	status = ni_pathsearch(*domain, dir, dirspec);
	free(dirspec);
	return status;
}
