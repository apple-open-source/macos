/* $Xorg: exec.c,v 1.4 2001/02/09 02:05:56 xorgcvs Exp $ */
/*

Copyright 1988, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/

/*
 * Copyright 1987 by Sun Microsystems, Inc. Mountain View, CA.
 *
 *                     All Rights Reserved
 * 
 * Permission  to  use,  copy,  modify,  and  distribute   this
 * software  and  its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright no-
 * tice  appear  in all copies and that both that copyright no-
 * tice and this permission notice appear in  supporting  docu-
 * mentation,  and  that the name of Sun not be used in
 * advertising or publicity pertaining to distribution  of  the
 * software  without specific prior written permission. Sun
 * makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without any
 * express or implied warranty.
 * 
 * SUN DISCLAIMS ALL WARRANTIES WITH REGARD TO  THIS  SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FIT-
 * NESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL SUN BE  LI-
 * ABLE  FOR  ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
 * ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,  DATA  OR
 * PROFITS,  WHETHER  IN  AN  ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION  WITH
 * THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Jim Fulton, MIT X Consortium; derived from parts of the
 * original xmodmap, written by David Rosenthal, of Sun Microsystems.
 */
/* $XFree86: xc/programs/xmodmap/exec.c,v 1.6 2003/12/02 13:13:57 pascal Exp $ */

#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include "xmodmap.h"
#include "wq.h"

static void
mapping_busy_key(int timeout)
{
    int i;
    unsigned char keymap[32];
    static unsigned int masktable[8] = {
	0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };

    XQueryKeymap (dpy, (char *) keymap);

    fprintf (stderr,
	     "%s:  please release the following keys within %d seconds:\n",
	     ProgramName, timeout);
    for (i = 0; i < 256; i++) {
	if (keymap[i >> 3] & masktable[i & 7]) {
	    KeySym ks = XKeycodeToKeysym (dpy, (KeyCode) i, 0);
	    char *cp = XKeysymToString (ks);
	    fprintf (stderr, "    %s (keysym 0x%x, keycode %d)\n",
		     cp ? cp : "UNNAMED", (unsigned int)ks, i);
	}
    }
    sleep (timeout);
    return;
}

static void
mapping_busy_pointer(int timeout)
{
    int i;
    Window root, child;			/* dummy variables */
    int rx, ry, wx, wy;			/* dummy variables */
    unsigned int mask;
    static unsigned int masks[5] = {
	Button1Mask, Button2Mask, Button3Mask, Button4Mask, Button5Mask };

    if (!XQueryPointer (dpy, RootWindow(dpy,DefaultScreen(dpy)),
			&root, &child, &rx, &ry, &wx, &wy, &mask))
      mask = 0;

    fprintf (stderr,
	     "%s:  please release the following buttons within %d seconds:\n",
	     ProgramName, timeout);
    for (i = 0; i < 5; i++) {
	if (mask & masks[i]) 
	  fprintf (stderr, "    Button%d\n", i+1);
    }
    sleep (timeout);
    return;
}


/*
 * UpdateModifierMapping - this sends the modifier map to the server
 * and deals with retransmissions due to the keyboard being busy.
 */

int 
UpdateModifierMapping(XModifierKeymap *map)
{
    int retries, timeout;

    for (retries = 5, timeout = 2; retries > 0; retries--, timeout *= 2) {
	int result;

	result = XSetModifierMapping (dpy, map);
	switch (result) {
	  case MappingSuccess:	/* Success */
	    return (0);
	  case MappingBusy:		/* Busy */
	    mapping_busy_key (timeout);
	    continue;
	  case MappingFailed:
	    fprintf (stderr, "%s: bad set modifier mapping.\n",
		     ProgramName);
	    return (-1);
	  default:
	    fprintf (stderr, "%s:  bad return %d from XSetModifierMapping\n",
		     ProgramName, result);
	    return (-1);
	}
    }
    fprintf (stderr,
	     "%s:  unable to set modifier mapping, keyboard problem\n",
	     ProgramName);
    return (-1);
}


/*
 * AddModifier - this adds a keycode to the modifier list
 */

int 
AddModifier(XModifierKeymap **mapp, KeyCode keycode, int modifier)
{
    if (keycode) {
	*mapp = XInsertModifiermapEntry (*mapp, keycode, modifier);
	return (0);
    } else {
	return (-1);
    }
    /*NOTREACHED*/
}


/*
 * DeleteModifier - this removes a keycode from the modifier list
 */

int 
RemoveModifier(XModifierKeymap **mapp, KeyCode keycode, int modifier)
{
    if (keycode) {
	*mapp = XDeleteModifiermapEntry (*mapp, keycode, modifier);
	return (0);
    } else {
	return (-1);
    }
    /*NOTREACHED*/
}


/*
 * ClearModifier - this removes all entries from the modifier list
 */

int 
ClearModifier(XModifierKeymap **mapp, int modifier)
{
    int i;
    XModifierKeymap *map = *mapp;
    KeyCode *kcp;

    kcp = &map->modifiermap[modifier * map->max_keypermod];
    for (i = 0; i < map->max_keypermod; i++) {
	*kcp++ = (KeyCode) 0;
    }
    return (0);
}


/*
 * print the contents of the map
 */
void
PrintModifierMapping(XModifierKeymap *map, FILE *fp)
{
    int i, k = 0;
    int min_keycode, max_keycode, keysyms_per_keycode = 0;

    XDisplayKeycodes (dpy, &min_keycode, &max_keycode);
    XGetKeyboardMapping (dpy, min_keycode, (max_keycode - min_keycode + 1),
			 &keysyms_per_keycode);

    fprintf (fp,
    	     "%s:  up to %d keys per modifier, (keycodes in parentheses):\n\n", 
    	     ProgramName, map->max_keypermod);
    for (i = 0; i < 8; i++) {
	int j;

	fprintf(fp, "%-10s", modifier_table[i].name);
	for (j = 0; j < map->max_keypermod; j++) {
	    if (map->modifiermap[k]) {
		KeySym ks;
		int index = 0;
		char *nm;
		do {
		    ks = XKeycodeToKeysym(dpy, map->modifiermap[k], index);
		    index++;
		} while ( !ks && index < keysyms_per_keycode);
		nm = XKeysymToString(ks);

		fprintf (fp, "%s  %s (0x%0x)", (j > 0 ? "," : ""), 
			 (nm ? nm : "BadKey"), map->modifiermap[k]);
	    }
	    k++;
	}
	fprintf(fp, "\n");
    }
    fprintf (fp, "\n");
    return;
}

void
PrintKeyTable(Bool exprs, FILE *fp)
{
    int         i;
    int min_keycode, max_keycode, keysyms_per_keycode;
    KeySym *keymap, *origkeymap;

    XDisplayKeycodes (dpy, &min_keycode, &max_keycode);
    origkeymap = XGetKeyboardMapping (dpy, min_keycode,
				      (max_keycode - min_keycode + 1),
				      &keysyms_per_keycode);

    if (!origkeymap) {
	fprintf (stderr, "%s:  unable to get keyboard mapping table.\n",
		 ProgramName);
	return;
    }
    if (!exprs) {
	fprintf (fp, 
		 "There are %d KeySyms per KeyCode; KeyCodes range from %d to %d.\n\n", 
		 keysyms_per_keycode, min_keycode, max_keycode);
	fprintf (fp, "    KeyCode\tKeysym (Keysym)\t...\n");
	fprintf (fp, "    Value  \tValue   (Name) \t...\n\n");
    }
    keymap = origkeymap;
    for (i = min_keycode; i <= max_keycode; i++) {
	int  j, max;

	if (exprs)
	    fprintf(fp, "keycode %3d =", i);
	else
	    fprintf(fp, "    %3d    \t", i);
	max = keysyms_per_keycode - 1;
	while ((max >= 0) && (keymap[max] == NoSymbol))
	    max--;
	for (j = 0; j <= max; j++) {
	    register KeySym ks = keymap[j];
	    char *s;
	    if (ks != NoSymbol)
		s = XKeysymToString (ks);
	    else
		s = "NoSymbol";
	    if (!exprs)
		fprintf (fp, "0x%04x (%s)\t", 
			 (unsigned int)ks, s ? s : "no name");
	    else if (s)
		fprintf (fp, " %s", s);
	    else
		fprintf (fp, " 0x%04x", (unsigned int)ks);
	}
	keymap += keysyms_per_keycode;
	fprintf (fp, "\n");
    }

    XFree ((char *) origkeymap);
    return;
}

void
PrintPointerMap(FILE *fp)
{
    unsigned char pmap[256];		/* there are 8 bits of buttons */
    int count, i;

    count = XGetPointerMapping (dpy, pmap, 256);

    fprintf (fp, "There are %d pointer buttons defined.\n\n", count);
    fprintf (fp, "    Physical        Button\n");
    fprintf (fp, "     Button          Code\n");
/*               "      ###            ###\n"               */
    for (i = 0; i < count; i++) {
	fprintf (fp, "      %3u            %3u\n", 
		 i+1, (unsigned int) pmap[i]);
    }
    fprintf (fp, "\n");
    return;
}


/*
 * SetPointerMap - set the pointer map
 */

int 
SetPointerMap(unsigned char *map, int n)
{
    unsigned char defmap[MAXBUTTONCODES];
    int j;
    int retries, timeout;

    if (n == 0) {				/* reset to default */
	n = XGetPointerMapping (dpy, defmap, MAXBUTTONCODES);
	for (j = 0; j < n; j++) defmap[j] = (unsigned char) (j + 1);
	map = defmap;
    }

    for (retries = 5, timeout = 2; retries > 0; retries--, timeout *= 2) {
	int result;

	switch (result = XSetPointerMapping (dpy, map, n)) {
	  case MappingSuccess:
	    return 0;
	  case MappingBusy:
	    mapping_busy_pointer (timeout);
	    continue;
	  case MappingFailed:
	    fprintf (stderr, "%s:  bad pointer mapping\n", ProgramName);
	    return -1;
	  default:
	    fprintf (stderr, "%s:  bad return %d from XSetPointerMapping\n",
		     ProgramName, result);
	    return -1;
	}
    }
    fprintf (stderr, "%s:  unable to set pointer mapping\n", ProgramName);
    return -1;
}
