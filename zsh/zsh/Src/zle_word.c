/*
 * zle_word.c - word-related editor functions
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

/**/
void
forwardword(void)
{
    if (zmult < 0) {
	zmult = -zmult;
	backwardword();
	return;
    }
    while (zmult--) {
	while (cs != ll && iword(line[cs]))
	    cs++;
	if (wordflag && !zmult)
	    return;
	while (cs != ll && !iword(line[cs]))
	    cs++;
    }
}

/**/
void
viforwardword(void)
{
    if (zmult < 0) {
	zmult = -zmult;
	backwardword();
	return;
    }
    while (zmult--) {
	if (iident(line[cs]))
	    while (cs != ll && iident(line[cs]))
		cs++;
	else
	    while (cs != ll && !iident(line[cs]) && !iblank(line[cs]))
		cs++;
	if (wordflag && !zmult)
	    return;
	while (cs != ll && (iblank(line[cs]) || line[cs] == '\n'))
	    cs++;
    }
}

/**/
void
viforwardblankword(void)
{
    if (zmult < 0) {
	zmult = -zmult;
	vibackwardblankword();
	return;
    }
    while (zmult--) {
	while (cs != ll && !iblank(line[cs]))
	    cs++;
	if (wordflag && !zmult)
	    return;
	while (cs != ll && iblank(line[cs]))
	    cs++;
    }
}

/**/
void
emacsforwardword(void)
{
    if (zmult < 0) {
	zmult = -zmult;
	emacsbackwardword();
	return;
    }
    while (zmult--) {
	while (cs != ll && !iword(line[cs]))
	    cs++;
	if (wordflag && !zmult)
	    return;
	while (cs != ll && iword(line[cs]))
	    cs++;
    }
}

/**/
void
viforwardblankwordend(void)
{
    if (zmult < 0)
	return;
    while (zmult--) {
	while (cs != ll && iblank(line[cs + 1]))
	    cs++;
	while (cs != ll && !iblank(line[cs + 1]))
	    cs++;
    }
    if (cs != ll && virangeflag)
	cs++;
}

/**/
void
viforwardwordend(void)
{
    if (zmult < 0) {
	zmult = -zmult;
	backwardword();
	return;
    }
    while (zmult--) {
	if (iblank(line[cs + 1]))
	    while (cs != ll && iblank(line[cs + 1]))
		cs++;
	if (iident(line[cs + 1]))
	    while (cs != ll && iident(line[cs + 1]))
		cs++;
	else
	    while (cs != ll && !iident(line[cs + 1]) && !iblank(line[cs + 1]))
		cs++;
    }
    if (cs != ll && virangeflag)
	cs++;
}

/**/
void
backwardword(void)
{
    if (zmult < 0) {
	zmult = -zmult;
	forwardword();
	return;
    }
    while (zmult--) {
	while (cs && !iword(line[cs - 1]))
	    cs--;
	while (cs && iword(line[cs - 1]))
	    cs--;
    }
}

/**/
void
vibackwardword(void)
{
    if (zmult < 0) {
	zmult = -zmult;
	backwardword();
	return;
    }
    while (zmult--) {
	while (cs && iblank(line[cs - 1]))
	    cs--;
	if (iident(line[cs - 1]))
	    while (cs && iident(line[cs - 1]))
		cs--;
	else
	    while (cs && !iident(line[cs - 1]) && !iblank(line[cs - 1]))
		cs--;
    }
}

/**/
void
vibackwardblankword(void)
{
    if (zmult < 0) {
	zmult = -zmult;
	viforwardblankword();
	return;
    }
    while (zmult--) {
	while (cs && iblank(line[cs - 1]))
	    cs--;
	while (cs && !iblank(line[cs - 1]))
	    cs--;
    }
}

/**/
void
emacsbackwardword(void)
{
    if (zmult < 0) {
	zmult = -zmult;
	emacsforwardword();
	return;
    }
    while (zmult--) {
	while (cs && !iword(line[cs - 1]))
	    cs--;
	while (cs && iword(line[cs - 1]))
	    cs--;
    }
}

/**/
void
backwarddeleteword(void)
{
    int x = cs;

    if (zmult < 0) {
	zmult = -zmult;
	deleteword();
	return;
    }
    while (zmult--) {
	while (x && !iword(line[x - 1]))
	    x--;
	while (x && iword(line[x - 1]))
	    x--;
    }
    backdel(cs - x);
}

/**/
void
vibackwardkillword(void)
{
    int x = cs, lim = (viinsbegin > findbol()) ? viinsbegin : findbol();

    if (zmult < 0) {
	feep();
	return;
    }
/* this taken from "vibackwardword" */
    while (zmult--) {
	while ((x > lim) && iblank(line[x - 1]))
	    x--;
	if (iident(line[x - 1]))
	    while ((x > lim) && iident(line[x - 1]))
		x--;
	else
	    while ((x > lim) && !iident(line[x - 1]) && !iblank(line[x - 1]))
		x--;
    }
    backkill(cs - x, 1);
}

/**/
void
backwardkillword(void)
{
    int x = cs;

    if (zmult < 0) {
	zmult = -zmult;
	killword();
	return;
    }
    while (zmult--) {
	while (x && !iword(line[x - 1]))
	    x--;
	while (x && iword(line[x - 1]))
	    x--;
    }
    backkill(cs - x, 1);
}

/**/
void
upcaseword(void)
{
    int neg = zmult < 0, ocs = cs;

    if (neg)
	zmult = -zmult;
    while (zmult--) {
	while (cs != ll && !iword(line[cs]))
	    cs++;
	while (cs != ll && iword(line[cs])) {
	    line[cs] = tuupper(line[cs]);
	    cs++;
	}
    }
    if (neg)
	cs = ocs;
}

/**/
void
downcaseword(void)
{
    int neg = zmult < 0, ocs = cs;

    if (neg)
	zmult = -zmult;
    while (zmult--) {
	while (cs != ll && !iword(line[cs]))
	    cs++;
	while (cs != ll && iword(line[cs])) {
	    line[cs] = tulower(line[cs]);
	    cs++;
	}
    }
    if (neg)
	cs = ocs;
}

/**/
void
capitalizeword(void)
{
    int first;
    int neg = zmult < 0, ocs = cs;

    if (neg)
	zmult = -zmult;
    while (zmult--) {
	first = 1;
	while (cs != ll && !iword(line[cs]))
	    cs++;
	while (cs != ll && iword(line[cs]) && !isalpha(line[cs]))
	    cs++;
	while (cs != ll && iword(line[cs])) {
	    line[cs] = (first) ? tuupper(line[cs]) : tulower(line[cs]);
	    first = 0;
	    cs++;
	}
    }
    if (neg)
	cs = ocs;
}

/**/
void
deleteword(void)
{
    int x = cs;

    if (zmult < 0) {
	zmult = -zmult;
	backwarddeleteword();
	return;
    }
    while (zmult--) {
	while (x != ll && !iword(line[x]))
	    x++;
	while (x != ll && iword(line[x]))
	    x++;
    }
    foredel(x - cs);
}

/**/
void
killword(void)
{
    int x = cs;

    if (zmult < 0) {
	zmult = -zmult;
	backwardkillword();
	return;
    }
    while (zmult--) {
	while (x != ll && !iword(line[x]))
	    x++;
	while (x != ll && iword(line[x]))
	    x++;
    }
    forekill(x - cs, 0);
}

/**/
void
transposewords(void)
{
    int p1, p2, p3, p4, x = cs;
    char *temp, *pp;
    int neg = zmult < 0, ocs = cs;

    if (neg)
	zmult = -zmult;
    while (zmult--) {
	while (x != ll && line[x] != '\n' && !iword(line[x]))
	    x++;
	if (x == ll || line[x] == '\n') {
	    x = cs;
	    while (x && line[x - 1] != '\n' && !iword(line[x]))
		x--;
	    if (!x || line[x - 1] == '\n') {
		feep();
		return;
	    }
	}
	for (p4 = x; p4 != ll && iword(line[p4]); p4++);
	for (p3 = p4; p3 && iword(line[p3 - 1]); p3--);
	if (!p3) {
	    feep();
	    return;
	}
	for (p2 = p3; p2 && !iword(line[p2 - 1]); p2--);
	if (!p2) {
	    feep();
	    return;
	}
	for (p1 = p2; p1 && iword(line[p1 - 1]); p1--);
	pp = temp = (char *)halloc(p4 - p1 + 1);
	struncpy(&pp, (char *) line + p3, p4 - p3);
	struncpy(&pp, (char *) line + p2, p3 - p2);
	struncpy(&pp, (char *) line + p1, p2 - p1);
	strncpy((char *)line + p1, temp, p4 - p1);
	cs = p4;
    }
    if (neg)
	cs = ocs;
}
