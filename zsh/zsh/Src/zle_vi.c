/*
 * zle_vi.c - vi-specific functions
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

static int lastmult, lastbuf, lastgotmult, lastgotbuf, inrepeat, vichgrepeat;

static void startvichange _((int im));

static void
startvichange(int im)
{
    if (im != -1) {
	insmode = im;
	vichgflag = 1;
    }
    if (inrepeat) {
	zmult = lastmult;
	vibufspec = lastbuf;
	gotmult = lastgotmult;
	gotvibufspec = lastgotbuf;
	inrepeat = vichgflag = 0;
	vichgrepeat = 1;
    } else {
	lastmult = zmult;
	lastbuf = vibufspec;
	lastgotmult = gotmult;
	lastgotbuf = gotvibufspec;
	if (vichgbuf)
	    free(vichgbuf);
	vichgbuf = (char *)zalloc(vichgbufsz = 16);
	vichgbuf[0] = c;
	vichgbufptr = 1;
	vichgrepeat = 0;
    }
}

static void startvitext _((int im));

static void
startvitext(int im)
{
    startvichange(im);
    bindtab = mainbindtab;
    undoing = 0;
    viinsbegin = cs;
}

/**/
int
vigetkey(void)
{
    int cmd;

    if((c = getkey(0)) == EOF) {
	feep();
	return -1;
    }
    cmd = mainbindtab[c];
    if(cmd == z_prefix) {
	char buf[2];
	Key ky;
	buf[0] = c;
	buf[1] = 0;
	ky = (Key) keybindtab->getnode(keybindtab, buf);
	if(!ky)
	    cmd = z_undefinedkey;
	else
	    cmd = ky->func;
    }

    if (cmd < 0 || cmd == z_sendbreak) {
	feep();
	return -1;
    } else if (cmd == z_quotedinsert) {
	if ((c = getkey(0)) == EOF) {
	    feep();
	    return -1;
	}
    } else if(cmd == z_viquotedinsert) {
	char sav = line[cs];

	line[cs] = '^';
	refresh();
	c = getkey(0);
	line[cs] = sav;
	if(c == EOF) {
	    feep();
	    return -1;
	}
    } else if (cmd == z_vicmdmode)
	return -1;
    return c;
}

/**/
int
getvirange(int wf)
{
    int k2, t0, startline, endline, obeep;
    int mult1, gotmult1;

    vilinerange = 0;
    startline = findbol();
    endline = findeol();
    /* get arguments for the command, and then the command key */
    mult1 = zmult;
    gotmult1 = gotmult;
    zmult = 1;
    gotmult = 0;
    lastcmd &= ~(ZLE_NEGARG | ZLE_DIGIT);
    while(1) {
	if ((k2 = getkeycmd()) < 0 || k2 == z_sendbreak) {
	    feep();
	    return -1;
	}
	if (!(zlecmds[k2].flags & ZLE_ARG))
	    break;
	(*zlecmds[k2].func) ();
	lastcmd = zlecmds[k2].flags;
    }

    /* double counts, such as in 3d4j, get multiplied, unless we're repeating */
    if(vichgrepeat && gotmult1) {
	zmult = mult1;
	gotmult = 1;
    } else if (gotmult1) {
	zmult *= mult1;
	gotmult = 1;
    }
    /* can't handle an empty file */
    if (!ll) {
	feep();
	return -1;
    }

    /* This bit handles repeated command keys, such as dd.  A number  *
     * of lines is taken as the range.  The current line is included. *
     * If the argument is positive, the lines go downward, otherwise  *
     * vice versa.  The argument gives the number of lines.           */
    if (k2 == bindk) {
	vilinerange = 1;
	if (!zmult) {
	    feep();
	    return -1;
	}
	t0 = cs;
	if (zmult > 0) {
	    while(zmult-- && cs <= ll)
		cs = findeol() + 1;
	    if (zmult != -1) {
		cs = t0;
		feep();
		return -1;
	    }
	    t0 = cs - 1;
	    cs = startline;
	    return t0;
	} else {
	    while(zmult++ && cs >= 0)
		cs = findbol() - 1;
	    if (zmult != 1) {
		cs = t0;
		feep();
		return -1;
	    }
	    cs++;
	    return endline;
	}
    }

    /* Not a movement?!  No, you can't do yd. */
    if (!(zlecmds[k2].flags & ZLE_MOVEMENT)) {
	feep();
	return -1;
    }

    /* Now we need to execute the movement command, to see where it *
     * actually goes.  virangeflag here indicates to the movement   *
     * function that it should place the cursor at the end of the   *
     * range, rather than where the cursor would actually go if it  *
     * were executed normally.  This makes a difference to some     *
     * commands, but not all.  For example, if searching forward    *
     * for a character, under normal circumstances the cursor lands *
     * on the character.  For a range, the range must include the   *
     * character, so the cursor gets placed after the character if  *
     * virangeflag is set.  vi-match-bracket needs to change the    *
     * value of virangeflag under some circumstances, meaning that  *
     * we need to change the *starting* position.                   */
    t0 = cs;
    virangeflag = 1;
    wordflag = wf;
    obeep = opts[BEEP];
    opts[BEEP] = 0;
    (*zlecmds[k2].func) ();
    wordflag = 0;
    opts[BEEP] = obeep;
    if (cs == t0) {
	/* An error occured -- couldn't move.  The movement command didn't *
	 * feep, because we set NO_BEEP for the duration of the command.   */
	feep();
	virangeflag = 0;
	return -1;
    }
    if(virangeflag == -1)
	t0++;
    virangeflag = 0;

    /* get the range the right way round */
    if (cs > t0) {
	int tmp = cs;
	cs = t0;
	t0 = tmp;
    }

    /* Was it a line-oriented move?  In this case, entire lines are taken. *
     * The terminating newline is left out of the range, which the real    *
     * command must deal with appropriately.  At this point we just need   *
     * to make the range encompass entire lines.                           */
    if (zlecmds[k2].flags & ZLE_LINEMOVE) {
	int newcs = findbol();
	cs = t0;
	t0 = findeol();
	cs = newcs;
	vilinerange = 1;
    }
    return t0;
}

/**/
void
viaddnext(void)
{
    if (cs != findeol())
	cs++;
    startvitext(1);
}

/**/
void
viaddeol(void)
{
    cs = findeol();
    startvitext(1);
}

/**/
void
viinsert(void)
{
    startvitext(1);
}

/**/
void
viinsertbol(void)
{
    vifirstnonblank();
    startvitext(1);
}

/**/
void
videlete(void)
{
    int c2;

    startvichange(1);
    if ((c2 = getvirange(0)) != -1) {
	forekill(c2 - cs, 0);
	if (vilinerange && ll) {
	    if (cs == ll)
		cs--;
	    foredel(1);
	    vifirstnonblank();
	}
    }
    vichgflag = vilinerange = 0;
}

/**/
void
videletechar(void)
{
    startvichange(-1);
    /* handle negative argument */
    if (zmult < 0) {
	zmult = -zmult;
	vibackwarddeletechar();
	return;
    }
    /* it is an error to be on the end of line */
    if (cs == ll || line[cs] == '\n') {
	feep();
	return;
    }
    /* Put argument into the acceptable range -- it is not an error to  *
     * specify a greater count than the number of available characters. */
    if (zmult > findeol() - cs)
	zmult = findeol() - cs;
    /* do the deletion */
    forekill(zmult, 0);
}

/**/
void
vichange(void)
{
    int c2;

    startvichange(1);
    if ((c2 = getvirange(1)) != -1) {
	forekill(c2 - cs, 0);
	bindtab = mainbindtab;
	viinsbegin = cs;
	undoing = 0;
    }
    vilinerange = 0;
}

/**/
void
visubstitute(void)
{
    startvichange(1);
    if (zmult < 0) {
	feep();
	return;
    }
    /* it is an error to be on the end of line */
    if (cs == ll || line[cs] == '\n') {
	feep();
	return;
    }
    /* Put argument into the acceptable range -- it is not an error to  *
     * specify a greater count than the number of available characters. */
    if (zmult > findeol() - cs)
	zmult = findeol() - cs;
    /* do the substitution */
    forekill(zmult, 0);
    startvitext(1);
}

/**/
void
vichangeeol(void)
{
    forekill(findeol() - cs, 0);
    startvitext(1);
}

/**/
void
vichangewholeline(void)
{
    vifirstnonblank();
    vichangeeol();
}

/**/
void
viyank(void)
{
    int oldcs = cs, c2;

    startvichange(1);
    if ((c2 = getvirange(0)) != -1)
	cut(cs, c2 - cs, 0);
    vichgflag = vilinerange = 0;
    cs = oldcs;
}

/**/
void
viyankeol(void)
{
    int x = findeol();

    startvichange(-1);
    if (x == cs) {
	feep();
	return;
    }
    cut(cs, x - cs, 0);
}

/**/
void
viyankwholeline(void)
{
    int bol = findbol(), oldcs = cs;

    startvichange(-1);
    if (zmult < 1)
	return;
    while(zmult--) {
     if (cs > ll) {
	feep();
	cs = oldcs;
	return;
     }
     cs = findeol() + 1;
    }
    vilinerange = 1;
    cut(bol, cs - bol - 1, 0);
    cs = oldcs;
}

/**/
void
vireplace(void)
{
    startvitext(0);
}

/* vi-replace-chars has some oddities relating to vi-repeat-change.  In *
 * the real vi, if one does 3r at the end of a line, it feeps without   *
 * reading the argument.  A successful rx followed by 3. at the end of  *
 * a line (or 3rx followed by . at the end of a line) will obviously    *
 * feep after the ., even though it has the argument available.  Here   *
 * repeating is tied very closely to argument reading, such that we     *
 * can't do that.  The solution is to just read the argument even if    *
 * the command will fail -- not exactly vi compatible, but it is more   *
 * consistent (consider dd in an empty file in vi).                     */
/**/
void
vireplacechars(void)
{
    int ch;

    startvichange(1);
    /* get key */
    if((ch = vigetkey()) == -1) {
	vichgflag = 0;
	feep();
	return;
    }
    /* check argument range */
    if (zmult < 0 || zmult + cs > findeol()) {
	vichgflag = 0;
	feep();
	return;
    }
    /* do change */
    if (ch == '\r' || ch == '\n') {
	/* <return> handled specially */
	cs += zmult - 1;
	backkill(zmult - 1, 0);
	line[cs++] = '\n';
    } else {
	while (zmult--)
	    line[cs++] = ch;
	cs--;
    }
    vichgflag = 0;
}

/**/
void
vicmdmode(void)
{
    if (bindtab == altbindtab)
	feep();
    else {
	bindtab = altbindtab;
	undoing = 1;
	vichgflag = 0;
	if (cs != findbol())
	    cs--;
    }
}

/**/
void
viopenlinebelow(void)
{
    cs = findeol();
    spaceinline(1);
    line[cs++] = '\n';
    startvitext(1);
    clearlist = 1;
}

/**/
void
viopenlineabove(void)
{
    cs = findbol();
    spaceinline(1);
    line[cs] = '\n';
    startvitext(1);
    clearlist = 1;
}

/**/
void
vioperswapcase(void)
{
    int oldcs, c2;

    /* get the range */
    startvichange(1);
    if ((c2 = getvirange(0)) != -1) {
	oldcs = cs;
	/* swap the case of all letters within range */
	while (cs < c2) {
	    if (islower(line[cs]))
		line[cs] = tuupper(line[cs]);
	    else if (isupper(line[cs]))
		line[cs] = tulower(line[cs]);
	    cs++;
	}
	cs = oldcs;
    }
    vichgflag = vilinerange = 0;
}

/**/
void
virepeatchange(void)
{
    /* make sure we have a change to repeat */
    if (!vichgbuf || vichgflag) {
	feep();
	return;
    }
    /* restore or update the saved count and buffer */
    if (gotmult) {
	lastmult = zmult;
	lastgotmult = 1;
    }
    if (gotvibufspec) {
	lastbuf = vibufspec;
	lastgotbuf = 1;
    }
    /* repeat the command */
    inrepeat = 1;
    ungetkeys(vichgbuf, vichgbufptr);
}

/**/
void
viindent(void)
{
    int oldcs = cs, c2;

    /* get the range */
    startvichange(1);
    if ((c2 = getvirange(0)) == -1) {
	vichgflag = vilinerange = 0;
	return;
    }
    vichgflag = 0;
    /* must be a line range */
    if (!vilinerange) {
	feep();
	cs = oldcs;
	return;
    }
    vilinerange = 0;
    oldcs = cs;
    /* add a tab to the beginning of each line within range */
    while (cs < c2) {
	spaceinline(1);
	line[cs] = '\t';
	cs = findeol() + 1;
    }
    /* go back to the first line of the range */
    cs = oldcs;
    vifirstnonblank();
}

/**/
void
viunindent(void)
{
    int oldcs = cs, c2;

    /* get the range */
    startvichange(1);
    if ((c2 = getvirange(0)) == -1) {
	vichgflag = vilinerange = 0;
	return;
    }
    vichgflag = 0;
    /* must be a line range */
    if (!vilinerange) {
	feep();
	cs = oldcs;
	return;
    }
    vilinerange = 0;
    oldcs = cs;
    /* remove a tab from the beginning of each line within range */
    while (cs < c2) {
	if (line[cs] == '\t')
	    foredel(1);
	cs = findeol() + 1;
    }
    /* go back to the first line of the range */
    cs = oldcs;
    vifirstnonblank();
}

/**/
void
vibackwarddeletechar(void)
{
    if (bindtab == altbindtab)
	startvichange(-1);
    /* handle negative argument */
    if (zmult < 0) {
	zmult = -zmult;
	videletechar();
	return;
    }
    /* It is an error to be at the beginning of the line, or (in *
     * insert mode) to delete past the beginning of insertion.   */
    if ((bindtab != altbindtab && cs - zmult < viinsbegin) || cs == findbol()) {
	feep();
	return;
    }
    /* Put argument into the acceptable range -- it is not an error to  *
     * specify a greater count than the number of available characters. */
    if (zmult > cs - findbol())
	zmult = cs - findbol();
    /* do the deletion */
    backkill(zmult, 1);
}

/**/
void
vikillline(void)
{
    if (viinsbegin > cs) {
	feep();
	return;
    }
    backdel(cs - viinsbegin);
}

/**/
void
viputbefore(void)
{
    Cutbuffer buf = &cutbuf;

    startvichange(-1);
    if (zmult < 0)
	return;
    if (gotvibufspec)
	buf = &vibuf[vibufspec];
    if (!buf->buf) {
	feep();
	return;
    }
    vilinerange = !!(buf->flags & CUTBUFFER_LINE);
    if (vilinerange) {
	cs = findbol();
	spaceinline(buf->len + 1);
	memcpy((char *)line + cs, buf->buf, buf->len);
	line[cs + buf->len] = '\n';
	vifirstnonblank();
    } else {
	while (zmult--) {
	    spaceinline(buf->len);
	    memcpy((char *)line + cs, buf->buf, buf->len);
	    cs += buf->len;
	}
	if (cs)
	    cs--;
    }
}

/**/
void
viputafter(void)
{
    Cutbuffer buf = &cutbuf;

    startvichange(-1);
    if (zmult < 0)
	return;
    if (gotvibufspec)
	buf = &vibuf[vibufspec];
    if (!buf->buf) {
	feep();
	return;
    }
    vilinerange = !!(buf->flags & CUTBUFFER_LINE);
    if (vilinerange) {
	cs = findeol();
	spaceinline(buf->len + 1);
	line[cs++] = '\n';
	memcpy((char *)line + cs, buf->buf, buf->len);
	vifirstnonblank();
    } else {
	if (cs != findeol())
	    cs++;
	while (zmult--) {
	    spaceinline(buf->len);
	    memcpy((char *)line + cs, buf->buf, buf->len);
	    cs += buf->len;
	}
	if (cs)
	    cs--;
    }

}

/**/
void
vijoin(void)
{
    int x;

    startvichange(-1);
    if ((x = findeol()) == ll) {
	feep();
	return;
    }
    cs = x + 1;
    for (x = 1; cs != ll && iblank(line[cs]); cs++, x++);
    backdel(x);
    if (cs && iblank(line[cs-1]))
	cs--;
    else {
	spaceinline(1);
	line[cs] = ' ';
    }
}

/**/
void
viswapcase(void)
{
    int eol;

    startvichange(-1);
    if (zmult < 1)
	return;
    eol = findeol();
    while (cs < eol && zmult--) {
	if (islower(line[cs]))
	    line[cs] = tuupper(line[cs]);
	else if (isupper(line[cs]))
	    line[cs] = tulower(line[cs]);
	cs++;
    }
    if (cs && cs == eol)
	cs--;
}

/**/
void
vicapslockpanic(void)
{
    clearlist = 1;
    feep();
    statusline = "press a lowercase key to continue";
    statusll = strlen(statusline);
    refresh();
    while (!islower(getkey(0)));
    statusline = NULL;
}

/**/
void
visetbuffer(void)
{
    int ch;

    if (gotvibufspec ||
	(((ch = getkey(0)) < '1' || ch > '9') &&
	 (ch < 'a' || ch > 'z') && (ch < 'A' || ch > 'Z'))) {
	feep();
	return;
    }
    if (ch >= 'A' && ch <= 'Z')	/* needed in cut() */
	vibufappend = 1;
    else
	vibufappend = 0;
    vibufspec = tulower(ch) + (idigit(ch) ? -'1' + 26 : -'a');
    gotvibufspec = 1;
}

/**/
void
vikilleol(void)
{
    int n = findeol() - cs;

    startvichange(-1);
    if (!n) {
	/* error -- line already empty */
	feep();
	return;
    }
    /* delete to end of line */
    forekill(findeol() - cs, 0);
}

/**/
void
vipoundinsert(void)
{
    int oldcs = cs;

    startvichange(-1);
    vifirstnonblank();
    if(line[cs] != '#') {
	spaceinline(1);
	line[cs] = '#';
	if(cs <= viinsbegin)
	    viinsbegin++;
	cs = oldcs + (cs <= oldcs);
    } else {
	foredel(1);
	if (cs < viinsbegin)
	    viinsbegin--;
	cs = oldcs - (cs < oldcs);
    }
}

/**/
void
viquotedinsert(void)
{
#ifndef HAS_TIO
    struct sgttyb sob;
#endif

    spaceinline(1);
    line[cs] = '^';
    refresh();
#ifndef HAS_TIO
    sob = shttyinfo.sgttyb;
    sob.sg_flags = (sob.sg_flags | RAW) & ~ECHO;
    ioctl(SHTTY, TIOCSETN, &sob);
#endif
    c = getkey(0);
#ifndef HAS_TIO
    setterm();
#endif
    foredel(1);
    if(c < 0)
	feep();
    else
	selfinsert();
}
