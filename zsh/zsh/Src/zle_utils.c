/*
 * zle_utils.c - miscellaneous line editor utilities
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

#define ZLE
#include "zsh.h"

/* make sure that the line buffer has at least sz chars */

/**/
void
sizeline(int sz)
{
    while (sz > linesz)
	line = (unsigned char *)realloc(line, (linesz *= 4) + 2);
}

/* insert space for ct chars at cursor position */

/**/
void
spaceinline(int ct)
{
    int i;

    sizeline(ct + ll);
    for (i = ll; --i >= cs;)
	line[i + ct] = line[i];
    ll += ct;
    line[ll] = '\0';

    if (mark > cs)
	mark += ct;
}

/**/
void
shiftchars(int to, int cnt)
{
    if (mark >= to + cnt)
	mark -= cnt;
    else if (mark > to)
	mark = to;

    while (to + cnt < ll) {
	line[to] = line[to + cnt];
	to++;
    }
    line[ll = to] = '\0';
}

/**/
void
backkill(int ct, int dir)
{
    int i = (cs -= ct);

    cut(i, ct, dir);
    shiftchars(i, ct);
}

/**/
void
forekill(int ct, int dir)
{
    int i = cs;

    cut(i, ct, dir);
    shiftchars(i, ct);
}

/**/
void
cut(int i, int ct, int dir)
{
    if (gotvibufspec) {
	if ((vibuf[vibufspec].flags & CUTBUFFER_LINE) && !vilinerange)
	    vibufappend = 0;
	if (!vibufappend || !vibuf[vibufspec].buf) {
	    zfree(vibuf[vibufspec].buf, vibuf[vibufspec].len);
	    vibuf[vibufspec].buf = (char *)zalloc(ct);
	    memcpy(vibuf[vibufspec].buf, (char *) line + i, ct);
	    vibuf[vibufspec].len = ct;
	    vibuf[vibufspec].flags = 0;
	} else {
	    int len = vibuf[vibufspec].len;

	    vibuf[vibufspec].buf = realloc(vibuf[vibufspec].buf, ct + len + 1);
	    if (vilinerange)
		vibuf[vibufspec].buf[len++] = '\n';
	    memcpy(vibuf[vibufspec].buf + len, (char *) line + i, ct);
	    vibuf[vibufspec].len = len + ct;
	}
	if(vilinerange)
	    vibuf[vibufspec].flags |= CUTBUFFER_LINE;
	else
	    vibuf[vibufspec].flags &= ~CUTBUFFER_LINE;
	return;
    } else {
	/* Save in "1, shifting "1-"8 along to "2-"9 */
	int n;
	zfree(vibuf[34].buf, vibuf[34].len);
	for(n=34; n>26; n--)
	    vibuf[n] = vibuf[n-1];
	vibuf[26].buf = (char *)zalloc(ct);
	memcpy(vibuf[26].buf, (char *) line + i, ct);
	vibuf[26].len = ct;
	vibuf[26].flags = vilinerange ? CUTBUFFER_LINE : 0;
    }
    if (!cutbuf.buf) {
	cutbuf.buf = ztrdup("");
	cutbuf.len = cutbuf.flags = 0;
    } else if (!(lastcmd & ZLE_KILL)) {
	kringnum = (kringnum + 1) % KRINGCT;
	if (kring[kringnum].buf)
	    free(kring[kringnum].buf);
	kring[kringnum] = cutbuf;
	cutbuf.buf = ztrdup("");
	cutbuf.len = cutbuf.flags = 0;
    }
    if (dir) {
	char *s = (char *)zalloc(cutbuf.len + ct);

	memcpy(s, (char *) line + i, ct);
	memcpy(s + ct, cutbuf.buf, cutbuf.len);
	free(cutbuf.buf);
	cutbuf.buf = s;
	cutbuf.len += ct;
    } else {
	cutbuf.buf = realloc(cutbuf.buf, cutbuf.len + ct);
	memcpy(cutbuf.buf + cutbuf.len, (char *) line + i, ct);
	cutbuf.len += ct;
    }
    if(vilinerange)
	cutbuf.flags |= CUTBUFFER_LINE;
    else
	cutbuf.flags &= ~CUTBUFFER_LINE;
}

/**/
void
backdel(int ct)
{
    shiftchars(cs -= ct, ct);
}

/**/
void
foredel(int ct)
{
    shiftchars(cs, ct);
}

/**/
void
setline(char const *s)
{
    sizeline(strlen(s));
    strcpy((char *) line, s);
    unmetafy((char *) line, &ll);
    if ((cs = ll) && bindtab == altbindtab)
	cs--;
    clearlist = 1;
}

/**/
int
findbol(void)
{
    int x = cs;

    while (x > 0 && line[x - 1] != '\n')
	x--;
    return x;
}

/**/
int
findeol(void)
{
    int x = cs;

    while (x != ll && line[x] != '\n')
	x++;
    return x;
}

/**/
void
findline(int *a, int *b)
{
    *a = findbol();
    *b = findeol();
}

static int lastlinelen;

/**/
void
initundo(void)
{
    int t0;

    for (t0 = 0; t0 != UNDOCT; t0++)
	undos[t0].change = NULL;
    undoct = 0;
    lastline = (unsigned char *)zalloc(lastlinelen = linesz + 1);
    memcpy((char *)lastline, (char *)line, ll);
    lastll = ll;
    lastcs = cs;
}

/**/
void
addundo(void)
{
    int pf, sf;
    unsigned char *s, *s2, *t, *t2;
    struct undoent *ue;

    for (s = line, t = lastline;
	s < line+ll && t < lastline+lastll && *s == *t; s++, t++);
    if (s == line+ll && t == lastline+lastll)
	return;
    pf = s - line;
    for (s2 = (unsigned char *)line + ll, t2 = lastline + lastll;
	 s2 > s && t > t2 && s2[-1] == t2[-1]; s2--, t2--);
    sf = line+ll - s2;
    ue = undos + (undoct = (undoct + 1) % UNDOCT);
    ue->pref = pf;
    ue->suff = sf;
    ue->len = t2 - t;
    ue->cs = lastcs;
    memcpy(ue->change = (char *)halloc(ue->len), (char *)t, ue->len);
    if(linesz + 1 > lastlinelen) {
	free(lastline);
	lastline = (unsigned char *)zalloc(lastlinelen = linesz + 1);
    }
    memcpy((char *)lastline, (char *)line, ll);
    lastll = ll;
    lastcs = cs;
}

/* Search for needle in haystack.  Haystack is a metafied string while *
 * needle is unmetafied and len-long.  Start the search at position    *
 * pos.  Search forward if dir > 0 otherwise search backward.          */

/**/
char *
hstrnstr(char *haystack, int pos, char *needle, int len, int dir, int sens)
{
    char *s = haystack + pos;

    if (dir > 0) {
	while (*s) {
	    if (metadiffer(s, needle, len) < sens)
		return s;
	    s += 1 + (*s == Meta);
	}
    } else {
	for (;;) {
	    if (metadiffer(s, needle, len) < sens)
		return s;
	    if (s == haystack)
		break;
	    s -= 1 + (s != haystack+1 && s[-2] == Meta);
	}
    }
    return NULL;
}

/* Query the user, and return a single character response.  The *
 * question is assumed to have been printed already, and the    *
 * cursor is left immediately after the response echoed.        *
 * (Might cause a problem if this takes it onto the next line.) *
 * <Tab> is interpreted as 'y'; any other control character is  *
 * interpreted as 'n'.  If there are any characters in the      *
 * buffer, this is taken as a negative response, and no         *
 * characters are read.  Case is folded.                        */

/**/
int
getzlequery(void)
{
    int c;
#ifdef FIONREAD
    int val;

    /* check for typeahead, which is treated as a negative response */
    ioctl(SHTTY, FIONREAD, (char *)&val);
    if (val) {
	putc('n', shout);
	return 'n';
    }
#endif

    /* get a character from the tty and interpret it */
    c = getkey(0);
    if (c == '\t')
	c = 'y';
    else if (icntrl(c) || c == EOF)
	c = 'n';
    else
	c = tulower(c);

    /* echo response and return */
    putc(c, shout);
    return c;
}
