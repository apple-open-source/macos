/*
 * Copyright (c) 2001 by The XFree86 Project, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *  
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE XFREE86 PROJECT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of the XFree86 Project shall
 * not be used in advertising or otherwise to promote the sale, use or other
 * dealings in this Software without prior written authorization from the
 * XFree86 Project.
 *
 * Author: Paulo César Pereira de Andrade
 */

/* $XFree86: xc/programs/xedit/lisp/pathname.c,v 1.17 2002/12/24 00:25:39 dawes Exp $ */

#include <stdio.h>		/* including dirent.h first may cause problems */
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include "pathname.h"
#include "private.h"

#define NOREAD_SKIP	0
#define NOREAD_ERROR	1

/*
 * Initialization
 */
LispObj *Oparse_namestring, *Kerror, *Kabsolute, *Krelative, *Kskip;

/*
 * Implementation
 */
void
LispPathnameInit(void)
{
    Kerror		= KEYWORD("ERROR");
    Oparse_namestring	= STATIC_ATOM("PARSE-NAMESTRING");
    Kabsolute		= KEYWORD("ABSOLUTE");
    Krelative		= KEYWORD("RELATIVE");
}

static int
glob_match(char *cmp1, char *cmp2)
/*
 * Note: this code was written from scratch, and may generate incorrect
 * results for very complex glob masks.
 */
{
    for (;;) {
	while (*cmp1 && *cmp1 == *cmp2) {
	    ++cmp1;
	    ++cmp2;
	}
	if (*cmp2) {
	    if (*cmp1 == '*') {
		while (*cmp1 == '*')
		    ++cmp1;
		if (*cmp1) {
		    int count = 0, settmp = 1;
		    char *tmp = cmp2, *sav2;

		    while (*cmp1 && *cmp1 == '?') {
			++cmp1;
			++count;
		    }

		    /* need to recurse here to make sure
		     * all cases are tested.
		     */
		    while (*cmp2 && *cmp2 != *cmp1)
			++cmp2;
		    if (!*cmp1 && cmp2 - tmp < count)
			return (0);
		    sav2 = cmp2;

		    /* if recursive calls fails, make sure all '?'
		     * following '*' are processed */
		    while (*sav2 && sav2 - tmp < count)
			++sav2;

		    for (; *cmp2;) {
			if (settmp) /* repeated letters: *?o? => boot, root */
			    tmp = cmp2;
			else
			    settmp = 1;
			while (*cmp2 && *cmp2 != *cmp1)
			    ++cmp2;
			if (cmp2 - tmp < count) {
			    if (*cmp2)
				++cmp2;
			    settmp = 0;
			    continue;
			}
			if (*cmp2) {
			    if (glob_match(cmp1, cmp2))
				return (1);
			    ++cmp2;
			}
		    }
		    cmp2 = sav2;
		}
		else {
		    while (*cmp2)
			++cmp2;
		    break;
		}
	    }
	    else if (*cmp1 == '?') {
		while (*cmp1 == '?' && *cmp2) {
		    ++cmp1;
		    ++cmp2;
		}
		continue;
	    }
	    else
		break;
	}
	else {
	    while (*cmp1 == '*')
		++cmp1;
	    break;
	}
    }

    return (*cmp1 == '\0' && *cmp2 == '\0');
}

/*
 * Since directory is a function to be extended by the implementation,
 * current extensions are:
 *	all		=> list files and directories
 *			   it is an error to call
 *			   (directory "<pathname-spec>/" :all t)
 *			   if non nil, it is like the shell command
 *			   echo <pathname-spec>, but normally, not in the
 *			   same order, as the code does not sort the result.
 *		!=nil	=> list files and directories
 * (default)	nil	=> list only files, or only directories if
 *			   <pathname-spec> ends with PATH_SEP char.
 *	if-cannot-read	=> if opendir fails on a directory
 *		:error	=> generate an error
 * (default)	:skip	=> skip search in this directory
 */
LispObj *
Lisp_Directory(LispBuiltin *builtin)
/*
 directory pathname &key all if-cannot-read
 */
{
    GC_ENTER();
    DIR *dir;
    struct stat st;
    struct dirent *ent;
    int length, listdirs, i, ndirs, nmatches;
    char name[PATH_MAX + 1], path[PATH_MAX + 2], directory[PATH_MAX + 2];
    char *sep, *base, *ptr, **dirs, **matches,
	  dot[] = {'.', PATH_SEP, '\0'},
	  dotdot[] = {'.', '.', PATH_SEP, '\0'};
    int cannot_read;

    LispObj *pathname, *all, *if_cannot_read, *result, *cons, *object;

    if_cannot_read = ARGUMENT(2);
    all = ARGUMENT(1);
    pathname = ARGUMENT(0);
    result = NIL;

    cons = NIL;

    if (if_cannot_read != UNSPEC) {
	if (!KEYWORDP(if_cannot_read) ||
	    (if_cannot_read != Kskip &&
	     if_cannot_read != Kerror))
	    LispDestroy("%s: bad :IF-CANNOT-READ %s",
			STRFUN(builtin), STROBJ(if_cannot_read));
	if (if_cannot_read != Kskip)
	    cannot_read = NOREAD_SKIP;
	else
	    cannot_read = NOREAD_ERROR;
    }
    else
	cannot_read = NOREAD_SKIP;

    if (PATHNAMEP(pathname))
	pathname = CAR(pathname->data.pathname);
    else if (STREAMP(pathname) && pathname->data.stream.type == LispStreamFile)
	pathname = CAR(pathname->data.stream.pathname->data.pathname);
    else if (!STRINGP(pathname))
	LispDestroy("%s: %s is not a pathname",
		    STRFUN(builtin), STROBJ(pathname));

    strncpy(name, THESTR(pathname), sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
    length = strlen(name);
    if (length < STRLEN(pathname))
	LispDestroy("%s: pathname too long %s",
		    STRFUN(builtin), name);

    if (length == 0) {
	if (getcwd(path, sizeof(path) - 2) == NULL)
	    LispDestroy("%s: getcwd(): %s", STRFUN(builtin), strerror(errno));
	length = strlen(path);
	if (!length || path[length - 1] != PATH_SEP) {
	    path[length++] = PATH_SEP;
	    path[length] = '\0';
	}
	result = APPLY1(Oparse_namestring, LSTRING(path, length));
	GC_LEAVE();

	return (result);
    }

    if (name[length - 1] == PATH_SEP) {
	listdirs = 1;
	if (length > 1) {
	    --length;
	    name[length] = '\0';
	}
    }
    else
	listdirs = 0;

    if (name[0] != PATH_SEP) {
	if (getcwd(path, sizeof(path) - 2) == NULL)
	    LispDestroy("%s: getcwd(): %s", STRFUN(builtin), strerror(errno));
	length = strlen(path);
	if (!length || path[length - 1] != PATH_SEP) {
	    path[length++] = PATH_SEP;
	    path[length] = '\0';
	}
    }
    else
	path[0] = '\0';

    result = NIL;

    /* list intermediate directories */
    matches = NULL;
    nmatches = 0;
    dirs = LispMalloc(sizeof(char*));
    ndirs = 1;
    if (snprintf(directory, sizeof(directory), "%s%s%c",
		 path, name, PATH_SEP) > PATH_MAX)
	LispDestroy("%s: pathname too long %s", STRFUN(builtin), directory);

    /* Remove ../ */
    sep = directory;
    for (sep = strstr(sep, dotdot); sep; sep = strstr(sep, dotdot)) {
	if (sep <= directory + 1)
	    strcpy(directory, sep + 2);
	else if (sep[-1] == PATH_SEP) {
	    for (base = sep - 2; base > directory; base--)
		if (*base == PATH_SEP)
		    break;
	    strcpy(base, sep + 2);
	    sep = base;
	}
	else
	    ++sep;
    }

    /* Remove "./" */
    sep = directory;
    for (sep = strstr(sep, dot); sep; sep = strstr(sep, dot)) {
	if (sep == directory || sep[-1] == PATH_SEP)
	    strcpy(sep, sep + 2);
	else
	    ++sep;
    }

    /* This will happen when there are too many '../'  in the path */
    if (directory[1] == '\0') {
	directory[1] = PATH_SEP;
	directory[2] = '\0';
    }

    base = directory;
    sep = strchr(base + 1, PATH_SEP);
    dirs[0] = LispMalloc(2);
    dirs[0][0] = PATH_SEP;
    dirs[0][1] = '\0';

    for (base = directory + 1, sep = strchr(base, PATH_SEP); ;
	 base = sep + 1, sep = strchr(base, PATH_SEP)) {
	*sep = '\0';
	if (sep[1] == '\0')
	    sep = NULL;
	length = strlen(base);
	if (length == 0) {
	    if (sep)
		*sep = PATH_SEP;
	    else
		break;
	    continue;
	}

	for (i = 0; i < ndirs; i++) {
	    length = strlen(dirs[i]);
	    if (length > 1)
		dirs[i][length - 1] = '\0';		/* remove trailing / */
	    if ((dir = opendir(dirs[i])) != NULL) {
		(void)readdir(dir);	/* "." */
		(void)readdir(dir);	/* ".." */
		if (length > 1)
		    dirs[i][length - 1] = PATH_SEP;	/* add trailing / again */

		snprintf(path, sizeof(path), "%s", dirs[i]);
		length = strlen(path);
		ptr = path + length;

		while ((ent = readdir(dir)) != NULL) {
		    int isdir;
		    unsigned d_namlen = strlen(ent->d_name);

		    if (length + d_namlen + 2 < sizeof(path))
			strcpy(ptr, ent->d_name);
		    else {
			closedir(dir);
			LispDestroy("%s: pathname too long %s",
				    STRFUN(builtin), dirs[i]);
		    }

		    if (stat(path, &st) != 0)
			isdir = 0;
		    else
			isdir = S_ISDIR(st.st_mode);

		    if (all != UNSPEC || ((isdir && (listdirs || sep)) ||
					  (!listdirs && !sep && !isdir))) {
			if (glob_match(base, ent->d_name)) {
			    if (isdir) {
				length = strlen(ptr);
				ptr[length++] = PATH_SEP;
				ptr[length] = '\0';
			    }
			    /* XXX won't closedir on memory allocation failure! */
			    matches = LispRealloc(matches, sizeof(char*) *
						  nmatches + 1);
			    matches[nmatches++] = LispStrdup(ptr);
			}
		    }
		}
		closedir(dir);

		if (nmatches == 0) {
		    if (sep || !listdirs || *base) {
			LispFree(dirs[i]);
			if (i + 1 < ndirs)
			    memmove(dirs + i, dirs + i + 1,
				    sizeof(char*) * (ndirs - (i + 1)));
			--ndirs;
			--i;		    /* XXX playing with for loop */
		    }
		}
		else {
		    int j;

		    length = strlen(dirs[i]);
		    if (nmatches > 1) {
			dirs = LispRealloc(dirs, sizeof(char*) *
					   (ndirs + nmatches));
			if (i + 1 < ndirs)
			    memmove(dirs + i + nmatches, dirs + i + 1,
				    sizeof(char*) * (ndirs - (i + 1)));
		    }
		    for (j = 1; j < nmatches; j++) {
			dirs[i + j] = LispMalloc(length +
						 strlen(matches[j]) + 1);
			sprintf(dirs[i + j], "%s%s", dirs[i], matches[j]);
		    }
		    dirs[i] = LispRealloc(dirs[i],
					  length + strlen(matches[0]) + 1);
		    strcpy(dirs[i] + length, matches[0]);
		    i += nmatches - 1;	/* XXX playing with for loop */
		    ndirs += nmatches - 1;

		    for (j = 0; j < nmatches; j++)
			LispFree(matches[j]);
		    LispFree(matches);
		    matches = NULL;
		    nmatches = 0;
		}
	    }
	    else {
		if (cannot_read == NOREAD_ERROR)
		    LispDestroy("%s: opendir(%s): %s",
				STRFUN(builtin), dirs[i], strerror(errno));
		else {
		    LispFree(dirs[i]);
		    if (i + 1 < ndirs)
			memmove(dirs + i, dirs + i + 1,
				sizeof(char*) * (ndirs - (i + 1)));
		    --ndirs;
		    --i;	    /* XXX playing with for loop */
		}
	    }
	}
	if (sep)
	    *sep = PATH_SEP;
	else
	    break;
    }

    for (i = 0; i < ndirs; i++) {
	object = APPLY1(Oparse_namestring, STRING2(dirs[i]));
	if (result == NIL) {
	    result = cons = CONS(object, NIL);
	    GC_PROTECT(result);
	}
	else {
	    RPLACD(cons, CONS(object, NIL));
	    cons = CDR(cons);
	}
    }
    LispFree(dirs);
    GC_LEAVE();

    return (result);
}

LispObj *
Lisp_ParseNamestring(LispBuiltin *builtin)
/*
 parse-namestring object &optional host defaults &key start end junk-allowed
 */
{
    GC_ENTER();
    LispObj *result;

    LispObj *object, *host, *defaults, *ostart, *oend, *junk_allowed;

    junk_allowed = ARGUMENT(5);
    oend = ARGUMENT(4);
    ostart = ARGUMENT(3);
    defaults = ARGUMENT(2);
    host = ARGUMENT(1);
    object = ARGUMENT(0);

    if (host == UNSPEC)
	host = NIL;
    if (defaults == UNSPEC)
	defaults = NIL;

    RETURN_COUNT = 1;
    if (STREAMP(object)) {
	if (object->data.stream.type == LispStreamFile)
	    object = object->data.stream.pathname;
	/* else just check for JUNK-ALLOWED... */
    }
    if (PATHNAMEP(object)) {
	RETURN(0) = FIXNUM(0);
	return (object);
    }

    if (host != NIL) {
	CHECK_STRING(host);
    }
    if (defaults != NIL) {
	if (!PATHNAMEP(defaults)) {
	    defaults = APPLY1(Oparse_namestring, defaults);
	    GC_PROTECT(defaults);
	}
    }

    result = NIL;
    if (STRINGP(object)) {
	LispObj *cons, *cdr;
	char *name = THESTR(object), *ptr, *str, data[PATH_MAX + 1],
	      string[PATH_MAX + 1], *namestr, *typestr, *send;
	long start, end, length, alength, namelen, typelen;

	LispCheckSequenceStartEnd(builtin, object, ostart, oend,
				  &start, &end, &length);
	alength = end - start;

	if (alength > sizeof(data) - 1)
	    LispDestroy("%s: string %s too large",
			STRFUN(builtin), STROBJ(object));
	memcpy(data, name + start, alength);
#ifndef KEEP_EXTRA_PATH_SEP
	ptr = data;
	send = ptr + alength;
	while (ptr < send) {
	    if (*ptr++ == PATH_SEP) {
		for (str = ptr; str < send && *str == PATH_SEP; str++)
		    ;
		if (str - ptr) {
		    memmove(ptr, str, alength - (str - data));
		    alength -= str - ptr;
		    send -= str - ptr;
		}
	    }
	}
#endif
	data[alength] = '\0';
	memcpy(string, data, alength + 1);

	if (PATHNAMEP(defaults))
	    defaults = defaults->data.pathname;

	/* string name */
	result = cons = CONS(NIL, NIL);
	GC_PROTECT(result);

	/* host */
	if (defaults != NIL)
	    defaults = CDR(defaults);
	cdr = defaults == NIL ? NIL : CAR(defaults);
	RPLACD(cons, CONS(cdr, NIL));
	cons = CDR(cons);

	/* device */
	if (defaults != NIL)
	    defaults = CDR(defaults);
	cdr = defaults == NIL ? NIL : CAR(defaults);
	RPLACD(cons, CONS(cdr, NIL));
	cons = CDR(cons);

	/* directory */
	if (defaults != NIL)
	    defaults = CDR(defaults);
	if (*data == PATH_SEP)
	    cdr = CONS(Kabsolute, NIL);
	else
	    cdr = CONS(Krelative, NIL);
	RPLACD(cons, CONS(cdr, NIL));
	cons = CDR(cons);
	/* directory components */
	ptr = data;
	send = data + alength;
	if (*ptr == PATH_SEP)
	    ++ptr;
	for (str = ptr; str < send; str++) {
	    if (*str == PATH_SEP)
		break;
	}
	while (str < send) {
	    *str++ = '\0';
	    if (str - ptr > NAME_MAX)
		LispDestroy("%s: directory name too long %s",
			    STRFUN(builtin), ptr);
	    RPLACD(cdr, CONS(LSTRING(ptr, str - ptr - 1), NIL));
	    cdr = CDR(cdr);
	    for (ptr = str; str < send; str++) {
		if (*str == PATH_SEP)
		    break;
	    }
	}
	if (str - ptr > NAME_MAX)
	    LispDestroy("%s: file name too long %s", STRFUN(builtin), ptr);
	if (CAAR(cons) == Krelative &&
	    defaults != NIL && CAAR(defaults) == Kabsolute) {
	    /* defaults specify directory and pathname doesn't */
	    char *tstring;
	    long dlength, tlength;
	    LispObj *dir = CDAR(defaults);

	    for (dlength = 1; CONSP(dir); dir = CDR(dir))
		dlength += STRLEN(CAR(dir)) + 1;
	    if (alength + dlength < PATH_MAX) {
		memmove(data + dlength, data, alength + 1);
		memmove(string + dlength, string, alength + 1);
		alength += dlength;
		ptr += dlength;
		send += dlength;
		CAAR(cons) = Kabsolute;
		for (dir = CDAR(defaults), cdr = CAR(cons);
		     CONSP(dir);
		     dir = CDR(dir)) {
		    RPLACD(cdr, CONS(CAR(dir), CDR(cdr)));
		    cdr = CDR(cdr);
		}
		dir = CDAR(defaults);
		data[0] = string[0] = PATH_SEP;
		for (dlength = 1; CONSP(dir); dir = CDR(dir)) {
		    tstring = THESTR(CAR(dir));
		    tlength = STRLEN(CAR(dir));
		    memcpy(data + dlength, tstring, tlength);
		    memcpy(string + dlength, tstring, tlength);
		    dlength += tlength;
		    data[dlength] = string[dlength] = PATH_SEP;
		    ++dlength;
		}
	    }
	}

	/* name */
	if (defaults != NIL)
	    defaults = CDR(defaults);
	cdr = defaults == NIL ? NIL : CAR(defaults);
	for (typelen = 0, str = ptr; str < send; str++) {
	    if (*str == PATH_TYPESEP) {
		typelen = 1;
		break;
	    }
	}
	if (*ptr)
	    cdr = LSTRING(ptr, str - ptr);
	if (STRINGP(cdr)) {
	    namestr = THESTR(cdr);
	    namelen = STRLEN(cdr);
	}
	else {
	    namestr = "";
	    namelen = 0;
	}
	RPLACD(cons, CONS(cdr, NIL));
	cons = CDR(cons);

	/* type */
	if (defaults != NIL)
	    defaults = CDR(defaults);
	cdr = defaults == NIL ? NIL : CAR(defaults);
	ptr = str + typelen;
	if (*ptr)
	    cdr = LSTRING(ptr, send - ptr);
	if (STRINGP(cdr)) {
	    typestr = THESTR(cdr);
	    typelen = STRLEN(cdr);
	}
	else {
	    typestr = "";
	    typelen = 0;
	}
	RPLACD(cons, CONS(cdr, NIL));
	cons = CDR(cons);

	/* version */
	if (defaults != NIL)
	    defaults = CDR(defaults);
	cdr = defaults == NIL ? NIL : CAR(defaults);
	RPLACD(cons, CONS(cdr, NIL));

	/* string representation, must be done here to use defaults */
	for (ptr = string + alength; ptr >= string; ptr--) {
	    if (*ptr == PATH_SEP)
		break;
	}
	if (ptr >= string)
	    ++ptr;
	else
	    ptr = string;
	*ptr = '\0';

	length = ptr - string;

	alength = namelen;
	if (alength) {
	    if (length + alength + 2 > sizeof(string))
		alength = sizeof(string) - length - 2;
	    memcpy(string + length, namestr, alength);
	    length += alength;
	}

	alength = typelen;
	if (alength) {
	    if (length + 2 < sizeof(string))
		string[length++] = PATH_TYPESEP;
	    if (length + alength + 2 > sizeof(string))
		alength = sizeof(string) - length - 2;
	    memcpy(string + length, typestr, alength);
	    length += alength;
	}
	string[length] = '\0';

	RPLACA(result,  LSTRING(string, length));
	RETURN(0) = FIXNUM(end);

	result = PATHNAME(result);
    }
    else if (junk_allowed == UNSPEC || junk_allowed == NIL)
	LispDestroy("%s: bad argument %s", STRFUN(builtin), STROBJ(object));
    else
	RETURN(0) = NIL;

    GC_LEAVE();

    return (result);
}

LispObj *
Lisp_MakePathname(LispBuiltin *builtin)
/*
 make-pathname &key host device directory name type version defaults
 */
{
    GC_ENTER();
    int length, alength;
    char *string, pathname[PATH_MAX + 1];
    LispObj *result, *cdr, *cons;

    LispObj *host, *device, *directory, *name, *type, *version, *defaults;

    defaults = ARGUMENT(6);
    version = ARGUMENT(5);
    type = ARGUMENT(4);
    name = ARGUMENT(3);
    directory = ARGUMENT(2);
    device = ARGUMENT(1);
    host = ARGUMENT(0);

    if (host != UNSPEC) {
	CHECK_STRING(host);
    }
    if (device != UNSPEC) {
	CHECK_STRING(device);
    }

    if (directory != UNSPEC) {
	LispObj *dir;

	CHECK_CONS(directory);
	dir = CAR(directory);
	CHECK_KEYWORD(dir);
	if (dir != Kabsolute && dir != Krelative)
	    LispDestroy("%s: directory type %s unknown",
			STRFUN(builtin), STROBJ(dir));
    }

    if (name != UNSPEC) {
	CHECK_STRING(name);
    }
    if (type != UNSPEC) {
	CHECK_STRING(type);
    }

    if (version != UNSPEC && version != NIL) {
	switch (OBJECT_TYPE(version)) {
	    case LispFixnum_t:
		if (FIXNUM_VALUE(version) >= 0)
		    goto version_ok;
	    case LispInteger_t:
		if (INT_VALUE(version) >= 0)
		    goto version_ok;
		break;
	    case LispDFloat_t:
		if (DFLOAT_VALUE(version) >= 0.0)
		    goto version_ok;
		break;
	    default:
		break;
	}
	LispDestroy("%s: %s is not a positive real number",
		    STRFUN(builtin), STROBJ(version));
    }
version_ok:

    if (defaults != UNSPEC && !PATHNAMEP(defaults) &&
	(host == UNSPEC || device == UNSPEC || directory == UNSPEC ||
	 name == UNSPEC || type == UNSPEC || version == UNSPEC)) {
	defaults = APPLY1(Oparse_namestring, defaults);
	GC_PROTECT(defaults);
    }

    if (defaults != UNSPEC) {
	defaults = defaults->data.pathname;
	defaults = CDR(defaults);	/* host */
	if (host == UNSPEC)
	    host = CAR(defaults);
	defaults = CDR(defaults);	/* device */
	if (device == UNSPEC)
	    device = CAR(defaults);
	defaults = CDR(defaults);	/* directory */
	if (directory == UNSPEC)
	    directory = CAR(defaults);
	defaults = CDR(defaults);	/* name */
	if (name == UNSPEC)
	    name = CAR(defaults);
	defaults = CDR(defaults);	/* type */
	if (type == UNSPEC)
	    type = CAR(defaults);
	defaults = CDR(defaults);	/* version */
	if (version == UNSPEC)
	    version = CAR(defaults);
    }

    /* string representation */
    length = 0;
    if (CONSP(directory)) {
	if (CAR(directory) == Kabsolute)
	    pathname[length++] = PATH_SEP;

	for (cdr = CDR(directory); CONSP(cdr); cdr = CDR(cdr)) {
	    CHECK_STRING(CAR(cdr));
	    string = THESTR(CAR(cdr));
	    alength = STRLEN(CAR(cdr));
	    if (alength > NAME_MAX)
		LispDestroy("%s: directory name too long %s",
			    STRFUN(builtin), string);
	    if (length + alength + 2 > sizeof(pathname))
		alength = sizeof(pathname) - length - 2;
	    memcpy(pathname + length, string, alength);
	    length += alength;
	    pathname[length++] = PATH_SEP;
	}
    }
    if (STRINGP(name)) {
	int xlength = 0;

	if (STRINGP(type))
	    xlength = STRLEN(type) + 1;

	string = THESTR(name);
	alength = STRLEN(name);
	if (alength + xlength > NAME_MAX)
	    LispDestroy("%s: file name too long %s",
			STRFUN(builtin), string);
	if (length + alength + 2 > sizeof(pathname))
	    alength = sizeof(pathname) - length - 2;
	memcpy(pathname + length, string, alength);
	length += alength;
    }
    if (STRINGP(type)) {
	if (length + 2 < sizeof(pathname))
	    pathname[length++] = PATH_TYPESEP;
	string = THESTR(type);
	alength = STRLEN(type);
	if (length + alength + 2 > sizeof(pathname))
	    alength = sizeof(pathname) - length - 2;
	memcpy(pathname + length, string, alength);
	length += alength;
    }
    pathname[length] = '\0';
    result = cons = CONS(LSTRING(pathname, length), NIL);
    GC_PROTECT(result);

    /* host */
    RPLACD(cons, CONS(host == UNSPEC ? NIL : host, NIL));
    cons = CDR(cons);

    /* device */
    RPLACD(cons, CONS(device == UNSPEC ? NIL : device, NIL));
    cons = CDR(cons);

    /* directory */
    if (directory == UNSPEC)
	cdr = CONS(Krelative, NIL);
    else
	cdr = directory;
    RPLACD(cons, CONS(cdr, NIL));
    cons = CDR(cons);

    /* name */
    RPLACD(cons, CONS(name == UNSPEC ? NIL : name, NIL));
    cons = CDR(cons);

    /* type */
    RPLACD(cons, CONS(type == UNSPEC ? NIL : type, NIL));
    cons = CDR(cons);

    /* version */
    RPLACD(cons, CONS(version == UNSPEC ? NIL : version, NIL));

    GC_LEAVE();

    return (PATHNAME(result));
}

LispObj *
Lisp_PathnameHost(LispBuiltin *builtin)
/*
 pathname-host pathname
 */
{
    return (LispPathnameField(PATH_HOST, 0));
}

LispObj *
Lisp_PathnameDevice(LispBuiltin *builtin)
/*
 pathname-device pathname
 */
{
    return (LispPathnameField(PATH_DEVICE, 0));
}

LispObj *
Lisp_PathnameDirectory(LispBuiltin *builtin)
/*
 pathname-device pathname
 */
{
    return (LispPathnameField(PATH_DIRECTORY, 0));
}

LispObj *
Lisp_PathnameName(LispBuiltin *builtin)
/*
 pathname-name pathname
 */
{
    return (LispPathnameField(PATH_NAME, 0));
}

LispObj *
Lisp_PathnameType(LispBuiltin *builtin)
/*
 pathname-type pathname
 */
{
    return (LispPathnameField(PATH_TYPE, 0));
}

LispObj *
Lisp_PathnameVersion(LispBuiltin *builtin)
/*
 pathname-version pathname
 */
{
    return (LispPathnameField(PATH_VERSION, 0));
}

LispObj *
Lisp_FileNamestring(LispBuiltin *builtin)
/*
 file-namestring pathname
 */
{
    return (LispPathnameField(PATH_NAME, 1));
}

LispObj *
Lisp_DirectoryNamestring(LispBuiltin *builtin)
/*
 directory-namestring pathname
 */
{
    return (LispPathnameField(PATH_DIRECTORY, 1));
}

LispObj *
Lisp_EnoughNamestring(LispBuiltin *builtin)
/*
 enough-pathname pathname &optional defaults
 */
{
    LispObj *pathname, *defaults;

    defaults = ARGUMENT(1);
    pathname = ARGUMENT(0);

    if (defaults != UNSPEC && defaults != NIL) {
	char *ppathname, *pdefaults, *pp, *pd;

	if (!STRINGP(pathname)) {
	    if (PATHNAMEP(pathname))
		pathname  = CAR(pathname->data.pathname);
	    else if (STREAMP(pathname) &&
		     pathname->data.stream.type == LispStreamFile)
		pathname  = CAR(pathname->data.stream.pathname->data.pathname);
	    else
		LispDestroy("%s: bad PATHNAME %s",
			    STRFUN(builtin), STROBJ(pathname));
	}

	if (!STRINGP(defaults)) {
	    if (PATHNAMEP(defaults))
		defaults  = CAR(defaults->data.pathname);
	    else if (STREAMP(defaults) &&
		     defaults->data.stream.type == LispStreamFile)
		defaults  = CAR(defaults->data.stream.pathname->data.pathname);
	    else
		LispDestroy("%s: bad DEFAULTS %s",
			    STRFUN(builtin), STROBJ(defaults));
	}

	ppathname = pp = THESTR(pathname);
	pdefaults = pd = THESTR(defaults);
	while (*ppathname && *pdefaults && *ppathname == *pdefaults) {
	    ppathname++;
	    pdefaults++;
	}
	if (*pdefaults == '\0' && pdefaults > pd)
	    --pdefaults;
	if (*ppathname && *pdefaults && *pdefaults != PATH_SEP) {
	    --ppathname;
	    while (*ppathname != PATH_SEP && ppathname > pp)
		--ppathname;
	    if (*ppathname == PATH_SEP)
		++ppathname;
	}

	return (STRING(ppathname));
    }
    else {
	if (STRINGP(pathname))
	    return (pathname);
	else if (PATHNAMEP(pathname))
	    return (CAR(pathname->data.pathname));
	else if (STREAMP(pathname)) {
	    if (pathname->data.stream.type == LispStreamFile)
		return (CAR(pathname->data.stream.pathname->data.pathname));
	}
    }
    LispDestroy("%s: bad PATHNAME %s", STRFUN(builtin), STROBJ(pathname));

    return (NIL);
}

LispObj *
Lisp_Namestring(LispBuiltin *builtin)
/*
 namestring pathname
 */
{
    return (LispPathnameField(PATH_STRING, 1));
}

LispObj *
Lisp_HostNamestring(LispBuiltin *builtin)
/*
 host-namestring pathname
 */
{
    return (LispPathnameField(PATH_HOST, 1));
}

LispObj *
Lisp_Pathnamep(LispBuiltin *builtin)
/*
 pathnamep object
 */
{
    LispObj *object;

    object = ARGUMENT(0);

    return (PATHNAMEP(object) ? T : NIL);
}

/* XXX only checks if host is a string and only checks the HOME enviroment
 * variable */
LispObj *
Lisp_UserHomedirPathname(LispBuiltin *builtin)
/*
 user-homedir-pathname &optional host
 */
{
    GC_ENTER();
    int length;
    char *home = getenv("HOME"), data[PATH_MAX + 1];
    LispObj *result;

    LispObj *host;

    host = ARGUMENT(0);

    if (host != UNSPEC && !STRINGP(host))
	LispDestroy("%s: bad hostname %s", STRFUN(builtin), STROBJ(host));

    length = 0;
    if (home) {
	length = strlen(home);
	strncpy(data, home, length);
	if (length && home[length - 1] != PATH_SEP)
	    data[length++] = PATH_SEP;
    }
    data[length] = '\0';

    result = LSTRING(data, length);
    GC_PROTECT(result);
    result = APPLY1(Oparse_namestring, result);
    GC_LEAVE();

    return (result);
}

LispObj *
Lisp_Truename(LispBuiltin *builtin)
{
    return (LispProbeFile(builtin, 0));
}

LispObj *
Lisp_ProbeFile(LispBuiltin *builtin)
{
    return (LispProbeFile(builtin, 1));
}
