/*
 * params.c - parameters
 *
 * This file is part of zsh, the Z shell.
 *
 * Copyright (c) 1992-1996 Paul Falstad
 * All rights reserved.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and to distribute modified versions of this software for any
 * purpose, provided that the above copyright notice and the following
 * two paragraphs appear in all copies of this software.
 *
 * In no event shall Paul Falstad or the Zsh Development Group be liable
 * to any party for direct, indirect, special, incidental, or consequential
 * damages arising out of the use of this software and its documentation,
 * even if Paul Falstad and the Zsh Development Group have been advised of
 * the possibility of such damage.
 *
 * Paul Falstad and the Zsh Development Group specifically disclaim any
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose.  The software
 * provided hereunder is on an "as is" basis, and Paul Falstad and the
 * Zsh Development Group have no obligation to provide maintenance,
 * support, updates, enhancements, or modifications.
 *
 */

#include "zsh.h"

static Param argvparam;

/* Set up parameter hash table.  This will add predefined  *
 * parameter entries as well as setting up parameter table *
 * entries for environment variables we inherit.           */

/**/
void
createparamtable(void)
{
    Param ip, pm;
    char **new_environ, **envp, **envp2, **sigptr, **t;
    char buf[50], *str, *iname;
    int num_env;

    paramtab = newhashtable(151);

    paramtab->hash        = hasher;
    paramtab->emptytable  = NULL;
    paramtab->filltable   = NULL;
    paramtab->addnode     = addhashnode;
    paramtab->getnode     = gethashnode2;
    paramtab->getnode2    = gethashnode2;
    paramtab->removenode  = removehashnode;
    paramtab->disablenode = NULL;
    paramtab->enablenode  = NULL;
    paramtab->freenode    = freeparamnode;
    paramtab->printnode   = printparamnode;
#ifdef ZSH_HASH_DEBUG
    paramtab->printinfo   = printhashtabinfo;
    paramtab->tablename   = ztrdup("paramtab");
#endif

    /* Add the special parameters to the hash table */
    for (ip = special_params; ip->nam; ip++)
	paramtab->addnode(paramtab, ztrdup(ip->nam), ip);
    if (emulation != EMULATE_SH && emulation != EMULATE_KSH)
	while ((++ip)->nam)
	    paramtab->addnode(paramtab, ztrdup(ip->nam), ip);

    argvparam = (Param) paramtab->getnode(paramtab, "*");

    noerrs = 2;

    HEAPALLOC {
	int allexp = opts[ALLEXPORT];

	opts[ALLEXPORT] = 0;

	/* Add the standard non-special parameters which have to    *
	 * be initialized before we copy the environment variables. *
	 * We don't want to override whatever values the users has  *
	 * given them in the environment.                           */
	setiparam("MAILCHECK", 60);
	setiparam("LOGCHECK", 60);
	setiparam("KEYTIMEOUT", 40);
	setiparam("LISTMAX", 100);
#ifdef HAVE_SELECT
	setiparam("BAUD", getbaudrate(&shttyinfo));  /* get the output baudrate */
#endif
	setsparam("FCEDIT", ztrdup(DEFAULT_FCEDIT));
	setsparam("TMPPREFIX", ztrdup(DEFAULT_TMPPREFIX));
	setsparam("TIMEFMT", ztrdup(DEFAULT_TIMEFMT));
	setsparam("WATCHFMT", ztrdup(DEFAULT_WATCHFMT));
	setsparam("HOST", ztrdup(hostnam));
	setsparam("LOGNAME", ztrdup((str = getlogin()) && *str ? str : cached_username));

	/* Copy the environment variables we are inheriting to dynamic *
	 * memory, so we can do mallocs and frees on it.               */
	num_env = arrlen(environ);
	new_environ = (char **) zalloc(sizeof(char *) * (num_env + 1));
	*new_environ = NULL;

	/* Now incorporate environment variables we are inheriting *
	 * into the parameter hash table.                          */
	for (envp = new_environ, envp2 = environ; *envp2; envp2++) {
	    for (str = *envp2; *str && *str != '='; str++);
	    if (*str == '=') {
		iname = ztrdup(*envp2);
		str = iname + (str - *envp2);
		*str = '\0';
		if (!idigit(*iname) && isident(iname) && !strchr(iname, '[') &&
		    !((pm = (Param) paramtab->getnode(paramtab, iname)) &&
		      (pm->flags & (PM_DONTIMPORT | PM_EXPORTED))) &&
		    (pm = setsparam(iname, metafy(str + 1, -1, META_DUP)))) {
		    *str = '=';
		    pm->flags |= PM_EXPORTED;
		    if (pm->flags & PM_SPECIAL) {
			zsfree(iname);
			iname = mkenvstr(pm->nam, getsparam(pm->nam));
		    }
		    pm->env = *envp++ = iname;
		    *envp = NULL;
		} else
		    zsfree(iname);
	    }
	}
	environ = new_environ;

	pm = (Param) paramtab->getnode(paramtab, "HOME");
	if (!(pm->flags & PM_EXPORTED)) {
	    pm->flags |= PM_EXPORTED;
	    pm->env = addenv("HOME", home);
	}
	pm = (Param) paramtab->getnode(paramtab, "PWD");
	if (!(pm->flags & PM_EXPORTED)) {
	    pm->flags |= PM_EXPORTED;
	    pm->env = addenv("PWD", pwd);
	}
	pm = (Param) paramtab->getnode(paramtab, "LOGNAME");
	if (!(pm->flags & PM_EXPORTED)) {
	    pm->flags |= PM_EXPORTED;
	    pm->env = addenv("LOGNAME", pm->u.str);
	}
	pm = (Param) paramtab->getnode(paramtab, "SHLVL");
	if (!(pm->flags & PM_EXPORTED))
	    pm->flags |= PM_EXPORTED;
	sprintf(buf, "%d", (int)++shlvl);
	pm->env = addenv("SHLVL", buf);

	/* Add the standard non-special parameters */
	setsparam("MACHTYPE", ztrdup(MACHTYPE));
	setsparam("OSTYPE", ztrdup(OSTYPE));
	setsparam("TTY", ztrdup(ttystrname));
	setsparam("VENDOR", ztrdup(VENDOR));
	setsparam("ZSH_NAME", ztrdup(zsh_name));
	setsparam("ZSH_VERSION", ztrdup(ZSH_VERSION));
	setaparam("signals", sigptr = zalloc((SIGCOUNT+4) * sizeof(char *)));
	for (t = sigs; (*sigptr++ = ztrdup(*t++)); );
	opts[ALLEXPORT] = allexp;
    } LASTALLOC;

    noerrs = 0;
}

/* Create a new parameter node */

/**/
Param
createparam(char *name, int flags)
{
    Param pm, oldpm, altpm;
    int spec;

    if (name != nulstring) {
	oldpm = (Param) paramtab->getnode(paramtab, name);
	spec = oldpm && (oldpm->flags & PM_SPECIAL);

	if ((oldpm && oldpm->level == locallevel) || spec) {
	    if (oldpm && !(oldpm->flags & PM_UNSET))
		return NULL;
	    pm = oldpm;
	    pm->ct = 0;
	    oldpm = pm->old;
	    pm->flags = (flags & (PM_EXPORTED | PM_LEFT | PM_RIGHT_B |
				  PM_RIGHT_Z | PM_LOWER | PM_UPPER |
				  PM_READONLY | PM_TAGGED | PM_UNIQUE)) |
		(pm->flags & (PM_SCALAR | PM_INTEGER | PM_ARRAY | PM_SPECIAL));
	    if (pm->ename &&
		(altpm = (Param) paramtab->getnode(paramtab, pm->ename))) {
		altpm->flags &= ~(PM_UNSET | PM_UNIQUE | PM_UPPER | PM_LEFT |
				  PM_RIGHT_B | PM_RIGHT_Z | PM_LOWER |
				  PM_READONLY | PM_TAGGED | PM_EXPORTED);
	    }
	} else {
	    pm = (Param) zcalloc(sizeof *pm);
	    if ((pm->old = oldpm)) {
		/* needed to avoid freeing oldpm */
		paramtab->removenode(paramtab, name);
	    }
	    paramtab->addnode(paramtab, ztrdup(name), pm);
	}

	if (isset(ALLEXPORT) && !oldpm)
	    pm->flags |= PM_EXPORTED;
    } else {
	pm = (Param) alloc(sizeof *pm);
	spec = 0;
    }

    if (!spec) {
	pm->flags = flags;
	switch (PM_TYPE(flags)) {
	case PM_SCALAR:
	    pm->sets.cfn = strsetfn;
	    pm->gets.cfn = strgetfn;
	    break;
	case PM_INTEGER:
	    pm->sets.ifn = intsetfn;
	    pm->gets.ifn = intgetfn;
	    break;
	case PM_ARRAY:
	    pm->sets.afn = arrsetfn;
	    pm->gets.afn = arrgetfn;
	    break;
#ifdef DEBUG
	default:
	    DPUTS(1, "oops, tried to create param node without valid flag");
	    break;
#endif
	}
    }
    return pm;
}

/* Return 1 if the string s is a valid identifier, else return 0. */

/**/
int
isident(char *s)
{
    char *ss;
    int ne;

    ne = noeval;		/* save the current value of noeval     */
    if (!*s)			/* empty string is definitely not valid */
	return 0;

    /* find the first character in `s' not in the iident type table */
    for (ss = s; *ss; ss++)
	if (!iident(*ss))
	    break;

    /* If this exhaust `s' or the next two characters *
     * are [(, then it is a valid identifier.         */
    if (!*ss || (*ss == '[' && ss[1] == '('))
	return 1;

    /* Else if the next character is not [, then it is *
     * definitely not a valid identifier.              */
    if (*ss != '[')
	return 0;
    noeval = 1;
    (void)mathevalarg(++ss, &ss);
    if (*ss == ',')
	(void)mathevalarg(++ss, &ss);
    noeval = ne;		/* restore the value of noeval */
    if (*ss != ']' || ss[1])
	return 0;
    return 1;
}

/**/
zlong
getarg(char **str, int *inv, Value v, int a2, zlong *w)
{
    int word = 0, rev = 0, ind = 0, down = 0, l, i;
    char *s = *str, *sep = NULL, *t, sav, *d, **ta, **p, *tt;
    zlong num = 1, r = 0;
    Comp c;

    /* first parse any subscription flags */
    if (*s == '(' || *s == Inpar) {
	int escapes = 0;
	int waste;
	for (s++; *s != ')' && *s != Outpar && s != *str; s++) {
	    switch (*s) {
	    case 'r':
		rev = 1;
		down = ind = 0;
		break;
	    case 'R':
		rev = down = 1;
		ind = 0;
		break;
	    case 'i':
		rev = ind = 1;
		down = 0;
		break;
	    case 'I':
		rev = ind = down = 1;
		break;
	    case 'w':
		/* If the parameter is a scalar, then make subscription *
		 * work on a per-word basis instead of characters.      */
		word = 1;
		break;
	    case 'f':
		word = 1;
		sep = "\n";
		break;
	    case 'e':
		/* obsolate compatibility flag without any real effect */
		break;
	    case 'n':
		t = get_strarg(++s);
		if (!*t)
		    goto flagerr;
		sav = *t;
		*t = '\0';
		num = mathevalarg(s + 1, &d);
		if (!num)
		    num = 1;
		*t = sav;
		s = t;
		break;
	    case 'p':
		escapes = 1;
		break;
	    case 's':
		/* This gives the string that separates words *
		 * (for use with the `w' flag.                */
		t = get_strarg(++s);
		if (!*t)
		    goto flagerr;
		sav = *t;
		*t = '\0';
		sep = escapes ? getkeystring(s + 1, &waste, 1, &waste) :
				dupstring(s + 1);
		*t = sav;
		s = t;
		break;
	    default:
	      flagerr:
		num = 1;
		word = rev = ind = down = 0;
		sep = NULL;
		s = *str - 1;
	    }
	}
	if (s != *str)
	    s++;
    }
    if (num < 0) {
	down = !down;
	num = -num;
    }
    *inv = ind;

    for (t=s, i=0; *t && ((*t != ']' && *t != Outbrack && *t != ',') || i); t++)
	if (*t == '[' || *t == Inbrack)
	    i++;
	else if (*t == ']' || *t == Outbrack)
	    i--;

    if (!*t)
	return 0;
    s = dupstrpfx(s, t - s);
    *str = tt = t;
    if (parsestr(s))
	return 0;
    singsub(&s);

    if (!rev) {
	if (!(r = mathevalarg(s, &s)) || (isset(KSHARRAYS) && r >= 0))
	    r++;
	if (word && !v->isarr) {
	    s = t = getstrvalue(v);
	    i = wordcount(s, sep, 0);
	    if (r < 0)
		r += i + 1;
	    if (r < 1)
		r = 1;
	    if (r > i)
		r = i;
	    if (!s || !*s)
		return 0;
	    while ((d = findword(&s, sep)) && --r);
	    if (!d)
		return 0;

	    if (!a2 && *tt != ',')
		*w = (zlong)(s - t) - 1;

	    return (a2 ? s : d + 1) - t;
	} else if (!v->isarr && !word) {
	    s = getstrvalue(v);
	    if (r > 0) {
		for (t = s + r - 1; *s && s < t;)
		    if (*s++ == Meta)
			s++, t++, r++;
	    } else {
		r += ztrlen(s);
		for (t = s + r; *s && s < t; r--)
		    if (*s++ == Meta)
			t++, r++;
		r -= strlen(s);
	    }
	}
    } else {
	if (!v->isarr && !word) {
	    l = strlen(s);
	    if (a2) {
		if (!l || *s != '*') {
		    d = (char *) ncalloc(l + 2);
		    *d = '*';
		    strcpy(d + 1, s);
		    s = d;
		}
	    } else {
		if (!l || s[l - 1] != '*') {
		    d = (char *) ncalloc(l + 2);
		    strcpy(d, s);
		    strcat(d, "*");
		    s = d;
		}
	    }
	}
	tokenize(s);

	if ((c = parsereg(s))) {
	    if (v->isarr) {
		ta = getarrvalue(v);
		if (!ta || !*ta)
		    return 0;
		if (down)
		    for (r = -1, p = ta + arrlen(ta) - 1; p >= ta; r--, p--) {
			if (domatch(*p, c, 0) && !--num)
			    return r;
		} else
		    for (r = 1, p = ta; *p; r++, p++)
			if (domatch(*p, c, 0) && !--num)
			    return r;
	    } else if (word) {
		ta = sepsplit(d = s = getstrvalue(v), sep, 1);
		if (down) {
		    for (p = ta + (r = arrlen(ta)) - 1; p >= ta; p--, r--)
			if (domatch(*p, c, 0) && !--num)
			    break;
		    if (p < ta)
			return 0;
		} else {
		    for (r = 1, p = ta; *p; r++, p++)
			if (domatch(*p, c, 0) && !--num)
			    break;
		    if (!*p)
			return 0;
		}
		if (a2)
		    r++;
		for (i = 0; (t = findword(&d, sep)) && *t; i++)
		    if (!--r) {
			r = (zlong)(t - s + (a2 ? -1 : 1));
			if (!a2 && *tt != ',')
			    *w = r + strlen(ta[i]) - 2;
			return r;
		    }
		return a2 ? -1 : 0;
	    } else {
		d = getstrvalue(v);
		if (!d || !*d)
		    return 0;
		if (a2) {
		    if (down)
			for (r = -2, t = d + strlen(d) - 1; t >= d; r--, t--) {
			    sav = *t;
			    *t = '\0';
			    if (domatch(d, c, 0) && !--num) {
				*t = sav;
				return r;
			    }
			    *t = sav;
		    } else
			for (r = 0, t = d; *t; r++, t++) {
			    sav = *t;
			    *t = '\0';
			    if (domatch(d, c, 0) && !--num) {
				*t = sav;
				return r;
			    }
			    *t = sav;
			}
		} else {
		    if (down)
			for (r = -1, t = d + strlen(d) - 1; t >= d; r--, t--) {
			    if (domatch(t, c, 0) && !--num)
				return r;
		    } else
			for (r = 1, t = d; *t; r++, t++)
			    if (domatch(t, c, 0) && !--num)
				return r;
		}
		return 0;
	    }
	}
    }
    return r;
}

/**/
int
getindex(char **pptr, Value v)
{
    int a, b, inv = 0;
    char *s = *pptr, *tbrack;

    *s++ = '[';
    for (tbrack = s; *tbrack && *tbrack != ']' && *tbrack != Outbrack; tbrack++)
	if (itok(*tbrack))
	    *tbrack = ztokens[*tbrack - Pound];
    if (*tbrack == Outbrack)
	*tbrack = ']';
    if ((s[0] == '*' || s[0] == '@') && s[1] == ']') {
	if (v->isarr)
	    v->isarr = (s[0] == '*') ? 1 : -1;
	v->a = 0;
	v->b = -1;
	s += 2;
    } else {
	zlong we = 0, dummy;

	a = getarg(&s, &inv, v, 0, &we);

	if (inv) {
	    if (!v->isarr && a != 0) {
		char *t, *p;
		t = getstrvalue(v);
		if (a > 0) {
		    for (p = t + a - 1; p-- > t; )
			if (*p == Meta)
			    a--;
		} else
		    a = -ztrlen(t + a + strlen(t));
	    }
	    if (a > 0 && isset(KSHARRAYS))
		a--;
	    v->inv = 1;
	    v->isarr = 0;
	    v->a = v->b = a;
	    if (*s == ',') {
		zerr("invalid subscript", NULL, 0);
		while (*s != ']' && *s != Outbrack)
		    s++;
		*pptr = s;
		return 1;
	    }
	    if (*s == ']' || *s == Outbrack)
		s++;
	} else {
	    if (a > 0)
		a--;
	    if (*s == ',') {
		s++;
		b = getarg(&s, &inv, v, 1, &dummy);
		if (b > 0)
		    b--;
	    } else {
		b = we ? we : a;
	    }
	    if (*s == ']' || *s == Outbrack) {
		s++;
		if (v->isarr && a == b)
		    v->isarr = 0;
		v->a = a;
		v->b = b;
	    } else
		s = *pptr;
	}
    }
    *pptr = s;
    return 0;
}


/**/
Value
getvalue(char **pptr, int bracks)
{
    char *s, *t;
    char sav;
    Value v;
    int ppar = 0;

    s = t = *pptr;

    if (idigit(*s)) {
	if (bracks >= 0)
	    ppar = zstrtol(s, &s, 10);
	else
	    ppar = *s++ - '0';
    } else if (iident(*s)) {
	while (iident(*s))
	    s++;
    } else if (*s == Quest)
	*s++ = '?';
    else if (*s == Pound)
	*s++ = '#';
    else if (*s == String)
	*s++ = '$';
    else if (*s == Qstring)
	*s++ = '$';
    else if (*s == Star)
	*s++ = '*';
    else if (*s == '#' || *s == '-' || *s == '?' || *s == '$' ||
	     *s == '_' || *s == '!' || *s == '@' || *s == '*')
	s++;
    else
	return NULL;

    if ((sav = *s))
	*s = '\0';
    if (ppar) {
	v = (Value) hcalloc(sizeof *v);
	v->pm = argvparam;
	v->inv = 0;
	v->a = v->b = ppar - 1;
	if (sav)
	    *s = sav;
    } else {
	Param pm;
	int isvarat;

        isvarat = !strcmp(t, "@");
	pm = (Param) paramtab->getnode(paramtab, *t == '0' ? "0" : t);
	if (sav)
	    *s = sav;
	*pptr = s;
	if (!pm || (pm->flags & PM_UNSET))
	    return NULL;
	v = (Value) hcalloc(sizeof *v);
	if (PM_TYPE(pm->flags) == PM_ARRAY)
	    v->isarr = isvarat ? -1 : 1;
	v->pm = pm;
	v->inv = 0;
	v->a = 0;
	v->b = -1;
	if (bracks > 0 && (*s == '[' || *s == Inbrack)) {
	    if (getindex(&s, v)) {
		*pptr = s;
		return v;
	    }
	} else if (v->isarr && iident(*t) && isset(KSHARRAYS))
	    v->b = 0, v->isarr = 0;
    }
    if (!bracks && *s)
	return NULL;
    *pptr = s;
    if (v->a > MAX_ARRLEN ||
	v->a < -MAX_ARRLEN) {
	zerr("subscript to %s: %d", (v->a < 0) ? "small" : "big", v->a);
	return NULL;
    }
    if (v->b > MAX_ARRLEN ||
	v->b < -MAX_ARRLEN) {
	zerr("subscript to %s: %d", (v->b < 0) ? "small" : "big", v->b);
	return NULL;
    }
    return v;
}

/**/
char *
getstrvalue(Value v)
{
    char *s, **ss;
    static char buf[(SIZEOF_ZLONG * 8) + 4];

    if (!v)
	return hcalloc(1);
    HEAPALLOC {
	if (v->inv) {
	    sprintf(buf, "%d", v->a);
	    s = dupstring(buf);
	    LASTALLOC_RETURN s;
	}

	switch(PM_TYPE(v->pm->flags)) {
	case PM_ARRAY:
	    if (v->isarr)
		s = sepjoin(v->pm->gets.afn(v->pm), NULL);
	    else {
		ss = v->pm->gets.afn(v->pm);
		if (v->a < 0)
		    v->a += arrlen(ss);
		s = (v->a >= arrlen(ss) || v->a < 0) ? (char *) hcalloc(1) : ss[v->a];
	    }
	    LASTALLOC_RETURN s;
	case PM_INTEGER:
	    convbase(s = buf, v->pm->gets.ifn(v->pm), v->pm->ct);
	    break;
	case PM_SCALAR:
	    s = v->pm->gets.cfn(v->pm);
	    break;
	default:
	    s = NULL;
	    DPUTS(1, "oops, param node without valid type");
	    break;
	}

	if (v->a == 0 && v->b == -1) {
	    LASTALLOC_RETURN s;
	}
	if (v->a < 0)
	    v->a += strlen(s);
	if (v->b < 0)
	    v->b += strlen(s);
	s = (v->a > (int)strlen(s)) ? dupstring("") : dupstring(s + v->a);
	if (v->b < v->a)
	    s[0] = '\0';
	else if (v->b - v->a < (int)strlen(s))
	    s[v->b - v->a + 1 + (s[v->b - v->a] == Meta)] = '\0';
    } LASTALLOC;
    return s;
}

static char *nular[] = {"", NULL};

/**/
char **
getarrvalue(Value v)
{
    char **s;

    if (!v)
	return arrdup(nular);
    if (v->inv) {
	char buf[DIGBUFSIZE];

	s = arrdup(nular);
	sprintf(buf, "%d", v->a);
	s[0] = dupstring(buf);
	return s;
    }
    s = v->pm->gets.afn(v->pm);
    if (v->a == 0 && v->b == -1)
	return s;
    if (v->a < 0)
	v->a += arrlen(s);
    if (v->b < 0)
	v->b += arrlen(s);
    if (v->a > arrlen(s) || v->a < 0)
	s = arrdup(nular);
    else
	s = arrdup(s) + v->a;
    if (v->b < v->a)
	s[0] = NULL;
    else if (v->b - v->a < arrlen(s))
	s[v->b - v->a + 1] = NULL;
    return s;
}

/**/
zlong
getintvalue(Value v)
{
    if (!v || v->isarr)
	return 0;
    if (v->inv)
	return v->a;
    if (PM_TYPE(v->pm->flags) == PM_INTEGER)
	return v->pm->gets.ifn(v->pm);
    return matheval(getstrvalue(v));
}

/**/
void
setstrvalue(Value v, char *val)
{
    char buf[(SIZEOF_ZLONG * 8) + 4];

    if (v->pm->flags & PM_READONLY) {
	zerr("read-only variable: %s", v->pm->nam, 0);
	zsfree(val);
	return;
    }
    switch (PM_TYPE(v->pm->flags)) {
    case PM_SCALAR:
	MUSTUSEHEAP("setstrvalue");
	if (v->a == 0 && v->b == -1) {
	    (v->pm->sets.cfn) (v->pm, val);
	    if (v->pm->flags & (PM_LEFT | PM_RIGHT_B | PM_RIGHT_Z) && !v->pm->ct)
		v->pm->ct = strlen(val);
	} else {
	    char *z, *x;
	    int zlen;

	    z = dupstring((v->pm->gets.cfn) (v->pm));
	    zlen = strlen(z);
	    if (v->inv && unset(KSHARRAYS))
		v->a--, v->b--;
	    if (v->a < 0) {
		v->a += zlen;
		if (v->a < 0)
		    v->a = 0;
	    }
	    if (v->a > zlen)
		v->a = zlen;
	    if (v->b < 0)
		v->b += zlen;
	    if (v->b > zlen - 1)
		v->b = zlen - 1;
	    x = (char *) zalloc(v->a + strlen(val) + zlen - v->b);
	    strncpy(x, z, v->a);
	    strcpy(x + v->a, val);
	    strcat(x + v->a, z + v->b + 1);
	    (v->pm->sets.cfn) (v->pm, x);
	    zsfree(val);
	}
	break;
    case PM_INTEGER:
	if (val) {
	    (v->pm->sets.ifn) (v->pm, matheval(val));
	    zsfree(val);
	}
	if (!v->pm->ct && lastbase != -1)
	    v->pm->ct = lastbase;
	break;
    case PM_ARRAY:
	MUSTUSEHEAP("setstrvalue");
	{
	    char **ss = (char **) zalloc(2 * sizeof(char *));

	    ss[0] = val;
	    ss[1] = NULL;
	    setarrvalue(v, ss);
	}
	break;
    }
    if ((!v->pm->env && !(v->pm->flags & PM_EXPORTED) &&
	 !(isset(ALLEXPORT) && !v->pm->old)) ||
	(v->pm->flags & PM_ARRAY) || v->pm->ename)
	return;
    if (PM_TYPE(v->pm->flags) == PM_INTEGER)
	convbase(val = buf, v->pm->gets.ifn(v->pm), v->pm->ct);
    else
	val = v->pm->gets.cfn(v->pm);
    if (v->pm->env)
	v->pm->env = replenv(v->pm->env, val);
    else {
	v->pm->flags |= PM_EXPORTED;
	v->pm->env = addenv(v->pm->nam, val);
    }
}

/**/
void
setintvalue(Value v, zlong val)
{
    char buf[DIGBUFSIZE];

    if (v->pm->flags & PM_READONLY) {
	zerr("read-only variable: %s", v->pm->nam, 0);
	return;
    }
    switch (PM_TYPE(v->pm->flags)) {
    case PM_SCALAR:
    case PM_ARRAY:
	convbase(buf, val, 0);
	setstrvalue(v, ztrdup(buf));
	break;
    case PM_INTEGER:
	(v->pm->sets.ifn) (v->pm, val);
	setstrvalue(v, NULL);
	break;
    }
}

/**/
void
setarrvalue(Value v, char **val)
{
    if (v->pm->flags & PM_READONLY) {
	zerr("read-only variable: %s", v->pm->nam, 0);
	freearray(val);
	return;
    }
    if (PM_TYPE(v->pm->flags) != PM_ARRAY) {
	freearray(val);
	zerr("attempt to assign array value to non-array", NULL, 0);
	return;
    }
    if (v->a == 0 && v->b == -1)
	(v->pm->sets.afn) (v->pm, val);
    else {
	char **old, **new, **p, **q, **r;
	int n, ll, i;

	if (v->inv && unset(KSHARRAYS))
	    v->a--, v->b--;
	q = old = v->pm->gets.afn(v->pm);
	n = arrlen(old);
	if (v->a < 0)
	    v->a += n;
	if (v->b < 0)
	    v->b += n;
	if (v->a < 0)
	    v->a = 0;
	if (v->b < 0)
	    v->b = 0;

	ll = v->a + arrlen(val);
	if (v->b < n)
	    ll += n - v->b;

	p = new = (char **) zcalloc(sizeof(char *) * (ll + 1));

	for (i = 0; i < v->a; i++)
	    *p++ = i < n ? ztrdup(*q++) : ztrdup("");
	for (r = val; *r;)
	    *p++ = ztrdup(*r++);
	if (v->b + 1 < n)
	    for (q = old + v->b + 1; *q;)
		*p++ = ztrdup(*q++);
	*p = NULL;

	(v->pm->sets.afn) (v->pm, new);
	freearray(val);
    }
}

/* Retrieve an integer parameter */

/**/
zlong
getiparam(char *s)
{
    Value v;

    if (!(v = getvalue(&s, 1)))
	return 0;
    return getintvalue(v);
}

/* Retrieve a scalar (string) parameter */

/**/
char *
getsparam(char *s)
{
    Value v;

    if (!(v = getvalue(&s, 0)))
	return NULL;
    return getstrvalue(v);
}

/* Retrieve an array parameter */

/**/
char **
getaparam(char *s)
{
    Value v;

    if (!idigit(*s) && (v = getvalue(&s, 0)) &&
	PM_TYPE(v->pm->flags) == PM_ARRAY)
	return v->pm->gets.afn(v->pm);
    return NULL;
}

/**/
Param
setsparam(char *s, char *val)
{
    Value v;
    char *t = s;
    char *ss;

    if (!isident(s)) {
	zerr("not an identifier: %s", s, 0);
	zsfree(val);
	errflag = 1;
	return NULL;
    }
    if ((ss = strchr(s, '['))) {
	*ss = '\0';
	if (!(v = getvalue(&s, 1)))
	    createparam(t, PM_ARRAY);
	*ss = '[';
	v = NULL;
    } else {
	if (!(v = getvalue(&s, 1)))
	    createparam(t, PM_SCALAR);
	else if (PM_TYPE(v->pm->flags) == PM_ARRAY &&
		 !(v->pm->flags & PM_SPECIAL) && unset(KSHARRAYS)) {
	    unsetparam(t);
	    createparam(t, PM_SCALAR);
	    v = NULL;
	}
    }
    if (!v && !(v = getvalue(&t, 1))) {
	zsfree(val);
	return NULL;
    }
    setstrvalue(v, val);
    return v->pm;
}

/**/
Param
setaparam(char *s, char **val)
{
    Value v;
    char *t = s;
    char *ss;

    if (!isident(s)) {
	zerr("not an identifier: %s", s, 0);
	freearray(val);
	errflag = 1;
	return NULL;
    }
    if ((ss = strchr(s, '['))) {
	*ss = '\0';
	if (!(v = getvalue(&s, 1)))
	    createparam(t, PM_ARRAY);
	*ss = '[';
	v = NULL;
    } else {
	if (!(v = getvalue(&s, 1)))
	    createparam(t, PM_ARRAY);
	else if (PM_TYPE(v->pm->flags) != PM_ARRAY &&
		 !(v->pm->flags & PM_SPECIAL)) {
	    int uniq = v->pm->flags & PM_UNIQUE;
	    unsetparam(t);
	    createparam(t, PM_ARRAY | uniq);
	    v = NULL;
	}
    }
    if (!v)
	if (!(v = getvalue(&t, 1)))
	    return NULL;
    if (isset(KSHARRAYS) && !ss)
	/* the whole array should be set instead of only the first element */
	v->b = -1;
    setarrvalue(v, val);
    return v->pm;
}

/**/
Param
setiparam(char *s, zlong val)
{
    Value v;
    char *t = s;
    Param pm;

    if (!isident(s)) {
	zerr("not an identifier: %s", s, 0);
	errflag = 1;
	return NULL;
    }
    if (!(v = getvalue(&s, 1))) {
	pm = createparam(t, PM_INTEGER);
	DPUTS(!pm, "BUG: parameter not created");
	pm->u.val = val;
	return pm;
    }
    setintvalue(v, val);
    return v->pm;
}

/* Unset a parameter */

/**/
void
unsetparam(char *s)
{
    Param pm;

    if ((pm = (Param) paramtab->getnode(paramtab, s)))
	unsetparam_pm(pm, 0);
}

/* Unset a parameter */

/**/
void
unsetparam_pm(Param pm, int altflag)
{
    Param oldpm, altpm;
    int spec;

    if ((pm->flags & PM_READONLY) && pm->level <= locallevel) {
	zerr("read-only variable: %s", pm->nam, 0);
	return;
    }
    spec = (pm->flags & PM_SPECIAL) && !pm->level;
    switch (PM_TYPE(pm->flags)) {
    case PM_SCALAR:
	(pm->sets.cfn) (pm, NULL);
	break;
    case PM_ARRAY:
	(pm->sets.afn) (pm, NULL);
	break;
    }
    if ((pm->flags & PM_EXPORTED) && pm->env) {
	delenv(pm->env);
	zsfree(pm->env);
	pm->env = NULL;
    }

    /* remove it under its alternate name if necessary */
    if (pm->ename && !altflag) {
	altpm = (Param) paramtab->getnode(paramtab, pm->ename);
	if (altpm)
	    unsetparam_pm(altpm, 1);
    }

    if ((pm->level && locallevel >= pm->level) || spec) {
	pm->flags |= PM_UNSET;
	return;
    }

    paramtab->removenode(paramtab, pm->nam); /* remove parameter node from table */

    if (pm->old) {
	oldpm = pm->old;
	paramtab->addnode(paramtab, oldpm->nam, oldpm);
	if ((PM_TYPE(oldpm->flags) == PM_SCALAR) && oldpm->sets.cfn == strsetfn)
	    adduserdir(oldpm->nam, oldpm->u.str, 0, 0);
    }

    paramtab->freenode((HashNode) pm); /* free parameter node */
}

/* Function to get value of an integer parameter */

/**/
zlong
intgetfn(Param pm)
{
    return pm->u.val;
}

/* Function to set value of an integer parameter */

/**/
void
intsetfn(Param pm, zlong x)
{
    pm->u.val = x;
}

/* Function to get value of a scalar (string) parameter */

/**/
char *
strgetfn(Param pm)
{
    return pm->u.str ? pm->u.str : (char *) hcalloc(1);
}

/* Function to set value of a scalar (string) parameter */

/**/
void
strsetfn(Param pm, char *x)
{
    zsfree(pm->u.str);
    pm->u.str = x;
    adduserdir(pm->nam, x, 0, 0);
}

/* Function to get value of an array parameter */

/**/
char **
arrgetfn(Param pm)
{
    static char *nullarray = NULL;

    return pm->u.arr ? pm->u.arr : &nullarray;
}

/* Function to set value of an array parameter */

/**/
void
arrsetfn(Param pm, char **x)
{
    if (pm->u.arr && pm->u.arr != x)
	freearray(pm->u.arr);
    if (pm->flags & PM_UNIQUE)
	uniqarray(x);
    pm->u.arr = x;
}

/* This function is used as the set function for      *
 * special parameters that cannot be set by the user. */

/**/
void
nullsetfn(Param pm, char *x)
{
    zsfree(x);
}

/* Function to get value of generic special integer *
 * parameter.  data is pointer to global variable   *
 * containing the integer value.                    */

/**/
zlong
intvargetfn(Param pm)
{
    return *((zlong *)pm->data);
}

/* Function to set value of generic special integer *
 * parameter.  data is pointer to global variable   *
 * where the value is to be stored.                 */

/**/
void
intvarsetfn(Param pm, zlong x)
{
    *((zlong *)pm->data) = x;
}

/* Function to set value of any ZLE-related integer *
 * parameter.  data is pointer to global variable   *
 * where the value is to be stored.                 */

/**/
void
zlevarsetfn(Param pm, zlong x)
{
    zlong *p = (zlong *)pm->data;

    *p = x;
    if (p == &lines || p == &columns)
	adjustwinsize(2 + (p == &columns));
}

/* Function to set value of generic special scalar    *
 * parameter.  data is pointer to a character pointer *
 * representing the scalar (string).                  */

/**/
void
strvarsetfn(Param pm, char *x)
{
    char **q = ((char **)pm->data);

    zsfree(*q);
    *q = x;
}

/* Function to get value of generic special scalar    *
 * parameter.  data is pointer to a character pointer *
 * representing the scalar (string).                  */

/**/
char *
strvargetfn(Param pm)
{
    char *s = *((char **)pm->data);

    if (!s)
	return hcalloc(1);
    return s;
}

/* Function to get value of generic special array  *
 * parameter.  data is a pointer to the pointer to *
 * a pointer (a pointer to a variable length array *
 * of pointers).                                   */

/**/
char **
arrvargetfn(Param pm)
{
    return *((char ***)pm->data);
}

/* Function to set value of generic special array parameter.    *
 * data is pointer to a variable length array of pointers which *
 * represents this array of scalars (strings).  If pm->ename is *
 * non NULL, then it is a colon separated environment variable  *
 * version of this array which will need to be updated.         */

/**/
void
arrvarsetfn(Param pm, char **x)
{
    char ***dptr = (char ***)pm->data;

    if (*dptr != x)
	freearray(*dptr);
    if (pm->flags & PM_UNIQUE)
	uniqarray(x);
    *dptr = x ? x : mkarray(NULL);
    if (pm->ename && x)
	arrfixenv(pm->ename, x);
}

/**/
char *
colonarrgetfn(Param pm)
{
    return zjoin(*(char ***)pm->data, ':');
}

/**/
void
colonarrsetfn(Param pm, char *x)
{
    char ***dptr = (char ***)pm->data;

    freearray(*dptr);
    *dptr = x ? colonsplit(x, pm->flags & PM_UNIQUE) : mkarray(NULL);
    zsfree(x);
    if (pm->ename)
	arrfixenv(pm->nam, *dptr);
}

/**/
void
uniqarray(char **x)
{
    char **t, **p = x;

    if (!x || !*x)
	return;
    while (*++p)
	for (t = x; t < p; t++)
	    if (!strcmp(*p, *t)) {
		zsfree(*p);
		for (t = p--; (*t = t[1]) != NULL; t++);
		break;
	    }
}

/* Function to get value of special parameter `#' and `ARGC' */

/**/
zlong
poundgetfn(Param pm)
{
    return arrlen(pparams);
}

/* Function to get value for special parameter `RANDOM' */

/**/
zlong
randomgetfn(Param pm)
{
    return rand() & 0x7fff;
}

/* Function to set value of special parameter `RANDOM' */

/**/
void
randomsetfn(Param pm, zlong v)
{
    srand((unsigned int)v);
}

/* Function to get value for special parameter `SECONDS' */

/**/
zlong
secondsgetfn(Param pm)
{
    return time(NULL) - shtimer.tv_sec;
}

/* Function to set value of special parameter `SECONDS' */

/**/
void
secondssetfn(Param pm, zlong x)
{
    shtimer.tv_sec = time(NULL) - x;
    shtimer.tv_usec = 0;
}

/* Function to get value for special parameter `USERNAME' */

/**/
char *
usernamegetfn(Param pm)
{
    return get_username();
}

/* Function to set value of special parameter `USERNAME' */

/**/
void
usernamesetfn(Param pm, char *x)
{
#ifdef HAVE_SETUID
    struct passwd *pswd;

    if (x && (pswd = getpwnam(x)) && (pswd->pw_uid != cached_uid)) {
#ifdef HAVE_INITGROUPS
	initgroups(x, pswd->pw_gid);
#endif
	if(!setgid(pswd->pw_gid) && !setuid(pswd->pw_uid)) {
	    zsfree(cached_username);
	    cached_username = x;
	    cached_uid = pswd->pw_uid;
	    return;
	}
    }
    zsfree(x);
#endif
}

/* Function to get value for special parameter `UID' */

/**/
zlong
uidgetfn(Param pm)
{
    return getuid();
}

/* Function to set value of special parameter `UID' */

/**/
void
uidsetfn(Param pm, uid_t x)
{
#ifdef HAVE_SETUID
    setuid(x);
#endif
}

/* Function to get value for special parameter `EUID' */

/**/
zlong
euidgetfn(Param pm)
{
    return geteuid();
}

/* Function to set value of special parameter `EUID' */

/**/
void
euidsetfn(Param pm, uid_t x)
{
#ifdef HAVE_SETEUID
    seteuid(x);
#endif
}

/* Function to get value for special parameter `GID' */

/**/
zlong
gidgetfn(Param pm)
{
    return getgid();
}

/* Function to set value of special parameter `GID' */

/**/
void
gidsetfn(Param pm, gid_t x)
{
#ifdef HAVE_SETUID
    setgid(x);
#endif
}

/* Function to get value for special parameter `EGID' */

/**/
zlong
egidgetfn(Param pm)
{
    return getegid();
}

/* Function to set value of special parameter `EGID' */

/**/
void
egidsetfn(Param pm, gid_t x)
{
#ifdef HAVE_SETEUID
    setegid(x);
#endif
}

/**/
zlong
ttyidlegetfn(Param pm)
{
    struct stat ttystat;

    if (SHTTY == -1 || fstat(SHTTY, &ttystat))
	return -1;
    return time(NULL) - ttystat.st_atime;
}

/* Function to get value for special parameter `IFS' */

/**/
char *
ifsgetfn(Param pm)
{
    return ifs;
}

/* Function to set value of special parameter `IFS' */

/**/
void
ifssetfn(Param pm, char *x)
{
    zsfree(ifs);
    ifs = x;
    inittyptab();
}

/* Functions to set value of special parameters `LANG' and `LC_*' */

#ifdef USE_LOCALE
static struct localename {
    char *name;
    int category;
} lc_names[] = {
#ifdef LC_COLLATE
    {"LC_COLLATE", LC_COLLATE},
#endif
#ifdef LC_CTYPE
    {"LC_CTYPE", LC_CTYPE},
#endif
#ifdef LC_MESSAGES
    {"LC_MESSAGES", LC_MESSAGES},
#endif
#ifdef LC_TIME
    {"LC_TIME", LC_TIME},
#endif
    {NULL, 0}
};

/**/
void
setlang(char *x)
{
    struct localename *ln;

    setlocale(LC_ALL, x ? x : "");
    for (ln = lc_names; ln->name; ln++)
	if ((x = getsparam(ln->name)))
	    setlocale(ln->category, x);
}

/**/
void
lc_allsetfn(Param pm, char *x)
{
    strsetfn(pm, x);
    if (!x)
	setlang(getsparam("LANG"));
    else
	setlocale(LC_ALL, x);
}

/**/
void
langsetfn(Param pm, char *x)
{
    strsetfn(pm, x);
    setlang(x);
}

/**/
void
lcsetfn(Param pm, char *x)
{
    strsetfn(pm, x);
    if (getsparam("LC_ALL"))
	return;
    if (!x)
	x = getsparam("LANG");
    setlocale((long) pm->data, x ? x : "");
}
#endif /* USE_LOCALE */

/* Function to get value for special parameter `HISTSIZE' */

/**/
zlong
histsizegetfn(Param pm)
{
    return histsiz;
}

/* Function to set value of special parameter `HISTSIZE' */

/**/
void
histsizesetfn(Param pm, zlong v)
{
    if ((histsiz = v) <= 2)
	histsiz = 2;
    resizehistents();
}

/* Function to get value for special parameter `ERRNO' */

/**/
zlong
errnogetfn(Param pm)
{
    return errno;
}

/* Function to get value for special parameter `-' */

/**/
char *
dashgetfn(Param pm)
{
    static char buf[63];
    char *val = buf;
    int optno;

    for(optno = 1; optno < OPT_SIZE; optno++)
	if(optid(optns[optno]) &&
		opts[optno] == !(optid(optns[optno]) & OPT_REV))
	    *val++ = optid(optns[optno]) & ~OPT_REV;
    *val = '\0';
    return buf;
}

/* Function to get value for special parameter `histchar' */

/**/
char *
histcharsgetfn(Param pm)
{
    static char buf[4];

    buf[0] = bangchar;
    buf[1] = hatchar;
    buf[2] = hashchar;
    buf[3] = '\0';
    return buf;
}

/* Function to set value of special parameter `histchar' */

/**/
void
histcharssetfn(Param pm, char *x)
{
    if (x) {
	bangchar = x[0];
	hatchar = (bangchar) ? x[1] : '\0';
	hashchar = (hatchar) ? x[2] : '\0';
	zsfree(x);
    } else {
	bangchar = '!';
	hashchar = '#';
	hatchar = '^';
    }
    inittyptab();
}

/* Function to get value for special parameter `HOME' */

/**/
char *
homegetfn(Param pm)
{
    return home;
}

/* Function to set value of special parameter `HOME' */

/**/
void
homesetfn(Param pm, char *x)
{
    zsfree(home);
    if (x && isset(CHASELINKS) && (home = xsymlink(x)))
	zsfree(x);
    else
	home = x ? x : ztrdup("");
    finddir(NULL);
}

/* Function to get value for special parameter `WORDCHARS' */

/**/
char *
wordcharsgetfn(Param pm)
{
    return wordchars;
}

/* Function to set value of special parameter `WORDCHARS' */

/**/
void
wordcharssetfn(Param pm, char *x)
{
    zsfree(wordchars);
    wordchars = x;
    inittyptab();
}

/* Function to get value for special parameter `_' */

/**/
char *
underscoregetfn(Param pm)
{
    return underscore;
}

/* Function to get value for special parameter `TERM' */

/**/
char *
termgetfn(Param pm)
{
    return term;
}

/* Function to set value of special parameter `TERM' */

/**/
void
termsetfn(Param pm, char *x)
{
    zsfree(term);
    term = x ? x : ztrdup("");

    /* If non-interactive, delay setting up term till we need it. */
    if (unset(INTERACTIVE) || !*term)
	termflags |= TERM_UNKNOWN;
    else 
	init_term();
}

/* We could probably replace the replenv with the actual code to *
 * do the replacing, since we've already scanned for the string. */

/**/
void
arrfixenv(char *s, char **t)
{
    char **ep, *u;
    int len_s;
    Param pm;

    MUSTUSEHEAP("arrfixenv");
    if (t == path)
	cmdnamtab->emptytable(cmdnamtab);
    u = zjoin(t, ':');
    len_s = strlen(s);
    pm = (Param) paramtab->getnode(paramtab, s);
    if (isset(ALLEXPORT))
	pm->flags |= PM_EXPORTED;
    if (pm->flags & PM_EXPORTED) {
	for (ep = environ; *ep; ep++)
	    if (!strncmp(*ep, s, len_s) && (*ep)[len_s] == '=') {
		pm->env = replenv(*ep, u);
		return;
	    }
	pm->env = addenv(s, u);
    }
}

/* Given *name = "foo", it searchs the environment for string *
 * "foo=bar", and returns a pointer to the beginning of "bar" */

/**/
char *
zgetenv(char *name)
{
    char **ep, *s, *t;
 
    for (ep = environ; *ep; ep++) {
	for (s = *ep, t = name; *s && *s == *t; s++, t++);
	if (*s == '=' && !*t)
	    return s + 1;
    }
    return NULL;
}

/* Change the value of an existing environment variable */

/**/
char *
replenv(char *e, char *value)
{
    char **ep, *s;
    int len_value;

    for (ep = environ; *ep; ep++)
	if (*ep == e) {
	    for (len_value = 0, s = value;
		 *s && (*s++ != Meta || *s++ != 32); len_value++);
	    s = e;
	    while (*s++ != '=');
	    *ep = (char *) zrealloc(e, s - e + len_value + 1);
	    s = s - e + *ep - 1;
	    while (*s++)
		if ((*s = *value++) == Meta)
		    *s = *value++ ^ 32;
	    return *ep;
	}
    return NULL;
}

/* Given strings *name = "foo", *value = "bar", *
 * return a new string *str = "foo=bar".        */

/**/
char *
mkenvstr(char *name, char *value)
{
    char *str, *s;
    int len_name, len_value;

    len_name = strlen(name);
    for (len_value = 0, s = value;
	 *s && (*s++ != Meta || *s++ != 32); len_value++);
    s = str = (char *) zalloc(len_name + len_value + 2);
    strcpy(s, name);
    s += len_name;
    *s = '=';
    while (*s++)
	if ((*s = *value++) == Meta)
	    *s = *value++ ^ 32;
    return str;
}

/* Given *name = "foo", *value = "bar", add the    *
 * string "foo=bar" to the environment.  Return a  *
 * pointer to the location of this new environment *
 * string.                                         */

/**/
char *
addenv(char *name, char *value)
{
    char **ep, *s, *t;
    int num_env;

    /* First check if there is already an environment *
     * variable matching string `name'.               */
    for (ep = environ; *ep; ep++) {
	for (s = *ep, t = name; *s && *s == *t; s++, t++);
	if (*s == '=' && !*t) {
	    zsfree(*ep);
	    return *ep = mkenvstr(name, value);
	}
    }

    /* Else we have to make room and add it */
    num_env = arrlen(environ);
    environ = (char **) zrealloc(environ, (sizeof(char *)) * (num_env + 2));

    /* Now add it at the end */
    ep = environ + num_env;
    *ep = mkenvstr(name, value);
    *(ep + 1) = NULL;
    return *ep;
}

/* Delete a pointer from the list of pointers to environment *
 * variables by shifting all the other pointers up one slot. */

/**/
void
delenv(char *x)
{
    char **ep;

    for (ep = environ; *ep; ep++) {
	if (*ep == x)
	    break;
    }
    if (*ep)
	for (; (ep[0] = ep[1]); ep++);
}

/**/
void
convbase(char *s, zlong v, int base)
{
    int digs = 0;
    zulong x;

    if (v < 0)
	*s++ = '-', v = -v;
    if (base <= 1)
	base = 10;

    if (base != 10) {
	sprintf(s, "%d#", base);
	s += strlen(s);
    }
    for (x = v; x; digs++)
	x /= base;
    if (!digs)
	digs = 1;
    s[digs--] = '\0';
    x = v;
    while (digs >= 0) {
	int dig = x % base;

	s[digs--] = (dig < 10) ? '0' + dig : dig - 10 + 'A';
	x /= base;
    }
}
