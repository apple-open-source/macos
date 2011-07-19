%{

/* Rely on vasprintf (GNU extension) instead of vsnprintf if
   possible... */
#ifdef HAVE_VASPRINTF
#define _GNU_SOURCE
#include <stdio.h>
#endif

#include <xcb/xcb.h>
#include <stdlib.h>
#include <stdarg.h>
#include "xcb_atom.h"

define(`COUNT', 0)dnl
define(`DO', `const xcb_atom_t $1 = define(`COUNT', incr(COUNT))COUNT;')dnl
include(atomlist.m4)`'dnl
%}

%readonly-tables
%pic
%null-strings
%enum
%includes
%compare-strncmp

%struct-type
struct atom_map { int name; xcb_atom_t value; };
%%
define(`COUNT', 0)dnl
define(`DO', `$1,define(`COUNT', incr(COUNT))COUNT')dnl
include(atomlist.m4)`'dnl
%%

static const char atom_names[] =
define(`DO', `	"$1\0"')dnl
include(atomlist.m4);

static const uint16_t atom_name_offsets[] = {
define(`OFFSET', 0)dnl
define(`DO', `	OFFSET,define(`OFFSET', eval(OFFSET+1+len($1)))')dnl
include(atomlist.m4)`'dnl
};

xcb_atom_t xcb_atom_get(xcb_connection_t *connection, const char *atom_name)
{
	if(atom_name == NULL)
		return XCB_NONE;
	xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(connection,
		xcb_intern_atom(connection, 0, strlen(atom_name), atom_name), NULL);
	if(!reply)
		return XCB_NONE;
	xcb_atom_t atom = reply->atom;
	free(reply);
	return atom;
}

xcb_atom_t xcb_atom_get_predefined(uint16_t name_len, const char *name)
{
	const struct atom_map *value = in_word_set(name, name_len);
	xcb_atom_t ret = XCB_NONE;
	if(value)
		ret = value->value;
	return ret;
}

xcb_atom_fast_cookie_t xcb_atom_get_fast(xcb_connection_t *c, uint8_t only_if_exists, uint16_t name_len, const char *name)
{
	xcb_atom_fast_cookie_t cookie;

	if((cookie.u.atom = xcb_atom_get_predefined(name_len, name)) != XCB_NONE)
	{
		cookie.tag = TAG_VALUE;
		return cookie;
	}

	cookie.tag = TAG_COOKIE;
	cookie.u.cookie = xcb_intern_atom(c, only_if_exists, name_len, name);
	return cookie;
}

xcb_atom_t xcb_atom_get_fast_reply(xcb_connection_t *c, xcb_atom_fast_cookie_t cookie, xcb_generic_error_t **e)
{
	switch(cookie.tag)
	{
		xcb_intern_atom_reply_t *reply;
	case TAG_VALUE:
		if(e)
			*e = 0;
		break;
	case TAG_COOKIE:
		reply = xcb_intern_atom_reply(c, cookie.u.cookie, e);
		if(reply)
		{
			cookie.u.atom = reply->atom;
			free(reply);
		}
		else
			cookie.u.atom = XCB_NONE;
		break;
	}
	return cookie.u.atom;
}

const char *xcb_atom_get_name_predefined(xcb_atom_t atom)
{
	if(atom <= 0 || atom > (sizeof(atom_name_offsets) / sizeof(*atom_name_offsets)))
		return 0;
	return atom_names + atom_name_offsets[atom - 1];
}

int xcb_atom_get_name(xcb_connection_t *c, xcb_atom_t atom, const char **namep, int *lengthp)
{
	static char buf[100];
	const char *name = xcb_atom_get_name_predefined(atom);
	int namelen;
	xcb_get_atom_name_cookie_t atomc;
	xcb_get_atom_name_reply_t *atomr;
	if(name)
	{
		*namep = name;
		*lengthp = strlen(name);
		return 1;
	}
	atomc = xcb_get_atom_name(c, atom);
	atomr = xcb_get_atom_name_reply(c, atomc, 0);
	if(!atomr)
		return 0;
	namelen = xcb_get_atom_name_name_length(atomr);
	if(namelen > sizeof(buf))
		namelen = sizeof(buf);
	*lengthp = namelen;
	memcpy(buf, xcb_get_atom_name_name(atomr), namelen);
	*namep = buf;
	free(atomr);
	return 1;
}

static char *makename(const char *fmt, ...)
{
	char *ret;
	int n;
	va_list ap;

#ifndef HAVE_VASPRINTF
	char *np;
	int size = 64;

	/* First allocate 'size' bytes, should be enough usually */
	if((ret = malloc(size)) == NULL)
		return NULL;

	while(1)
	{
		va_start(ap, fmt);
		n = vsnprintf(ret, size, fmt, ap);
		va_end(ap);

		if(n < 0)
			return NULL;

		if(n < size)
			return ret;

		size = n + 1;
		if((np = realloc(ret, size)) == NULL)
		{
			free(ret);
			return NULL;
		}

		ret = np;
	}
#else
	va_start(ap, fmt);
	n = vasprintf(&ret, fmt, ap);
	va_end(ap);

	if(n < 0)
		return NULL;

	return ret;
#endif
}

char *xcb_atom_name_by_screen(const char *base, uint8_t screen)
{
	return makename("%s_S%u", base, screen);
}

char *xcb_atom_name_by_resource(const char *base, uint32_t resource)
{
	return makename("%s_R%08X", base, resource);
}

char *xcb_atom_name_unique(const char *base, uint32_t id)
{
	if(base)
		return makename("%s_U%lu", base, id);
	else
		return makename("U%lu", id);
}
