/*
 * loop.c - loop execution
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

/**/
int
execfor(Cmd cmd)
{
    List list;
    Forcmd node;
    char *str;
    LinkList args;

    node = cmd->u.forcmd;
    args = cmd->args;
    if (!node->inflag) {
	char **x;

	args = newlinklist();
	for (x = pparams; *x; x++)
	    addlinknode(args, ztrdup(*x));
    }
    lastval = 0;
    loops++;
    pushheap();
    while ((str = (char *)ugetnode(args))) {
	setsparam(node->name, ztrdup(str));
	list = (List) dupstruct(node->list);
	execlist(list, 1, (cmd->flags & CFLAG_EXEC) && empty(args));
	if (breaks) {
	    breaks--;
	    if (breaks || !contflag)
		break;
	    contflag = 0;
	}
	if (errflag) {
	    lastval = 1;
	    break;
	}
	freeheap();
    }
    popheap();
    loops--;
    return lastval;
}

/**/
int
execselect(Cmd cmd)
{
    List list;
    Forcmd node;
    char *str, *s;
    LinkList args;
    LinkNode n;
    int i, usezle;
    FILE *inp;

    node = cmd->u.forcmd;
    args = cmd->args;
    if (!node->inflag) {
	char **x;

	args = newlinklist();
	for (x = pparams; *x; x++)
	    addlinknode(args, ztrdup(*x));
    }
    if (empty(args))
	return 1;
    loops++;
    lastval = 0;
    pushheap();
    usezle = interact && SHTTY != -1 && isset(USEZLE);
    inp = fdopen(dup(usezle ? SHTTY : 0), "r");
    for (;;) {
	do {
	    selectlist(args);
	    if (empty(bufstack)) {
	    	if (usezle) {
		    int oef = errflag;

		    isfirstln = 1;
		    str = (char *)zleread(prompt3, NULL);
		    if (errflag)
			str = NULL;
		    errflag = oef;
	    	} else {
		    int pptlen;
		    str = putprompt(prompt3, &pptlen, NULL, 1);
		    fwrite(str, pptlen, 1, stderr);
		    free(str);
		    fflush(stderr);
		    str = fgets(zalloc(256), 256, inp);
	    	}
	    } else
		str = (char *)getlinknode(bufstack);
	    if (!str || errflag) {
		if (breaks)
		    breaks--;
		fprintf(stderr, "\n");
		fflush(stderr);
		goto done;
	    }
	    if ((s = strchr(str, '\n')))
		*s = '\0';
	}
	while (!*str);
	setsparam("REPLY", ztrdup(str));
	i = atoi(str);
	if (!i)
	    str = "";
	else {
	    for (i--, n = firstnode(args); n && i; incnode(n), i--);
	    if (n)
		str = (char *) getdata(n);
	    else
		str = "";
	}
	setsparam(node->name, ztrdup(str));
	list = (List) dupstruct(node->list);
	execlist(list, 1, 0);
	freeheap();
	if (breaks) {
	    breaks--;
	    if (breaks || !contflag)
		break;
	    contflag = 0;
	}
	if (errflag)
	    break;
    }
  done:
    popheap();
    fclose(inp);
    loops--;
    return lastval;
}

/**/
int
execwhile(Cmd cmd)
{
    List list;
    struct whilecmd *node;
    int olderrexit, oldval;

    olderrexit = noerrexit;
    node = cmd->u.whilecmd;
    oldval = 0;
    pushheap();
    loops++;
    for (;;) {
	list = (List) dupstruct(node->cont);
	noerrexit = 1;
	execlist(list, 1, 0);
	noerrexit = olderrexit;
	if (!((lastval == 0) ^ node->cond)) {
	    if (breaks)
		breaks--;
	    lastval = oldval;
	    break;
	}
	list = (List) dupstruct(node->loop);
	execlist(list, 1, 0);
	if (breaks) {
	    breaks--;
	    if (breaks || !contflag)
		break;
	    contflag = 0;
	}
	freeheap();
	if (errflag) {
	    lastval = 1;
	    break;
	}
	oldval = lastval;
    }
    popheap();
    loops--;
    return lastval;
}

/**/
int
execrepeat(Cmd cmd)
{
    List list;
    int count;

    lastval = 0;
    if (empty(cmd->args) || nextnode(firstnode(cmd->args))) {
	zerr("bad argument for repeat", NULL, 0);
	return 1;
    }
    count = atoi(peekfirst(cmd->args));
    pushheap();
    loops++;
    while (count-- > 0) {
	list = (List) dupstruct(cmd->u.list);
	execlist(list, 1, 0);
	freeheap();
	if (breaks) {
	    breaks--;
	    if (breaks || !contflag)
		break;
	    contflag = 0;
	}
	if (errflag) {
	    lastval = 1;
	    break;
	}
    }
    popheap();
    loops--;
    return lastval;
}

/**/
int
execif(Cmd cmd)
{
    struct ifcmd *node;
    int olderrexit;
    List *i, *t;

    olderrexit = noerrexit;
    node = cmd->u.ifcmd;
    i = node->ifls;
    t = node->thenls;

    if (!noerrexit)
	noerrexit = 1;
    while (*i) {
	execlist(*i, 1, 0);
	if (!lastval)
	    break;
	i++;
	t++;
    }
    noerrexit = olderrexit;

    if (*t)
	execlist(*t, 1, cmd->flags & CFLAG_EXEC);
    else
	lastval = 0;

    return lastval;
}

/**/
int
execcase(Cmd cmd)
{
    struct casecmd *node;
    char *word;
    List *l;
    char **p;

    node = cmd->u.casecmd;
    l = node->lists;
    p = node->pats;

    word = *p++;
    singsub(&word);
    untokenize(word);
    lastval = 0;

    if (node) {
	while (*p) {
	    singsub(p);
	    if (matchpat(word, *p))
		break;
	    p++;
	    l++;
	}
	if (*l)
	    execlist(*l, 1, cmd->flags & CFLAG_EXEC);
    }
    return lastval;
}

