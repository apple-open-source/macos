/* $Xorg: wq.h,v 1.4 2001/02/09 02:05:56 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/xmodmap/wq.h,v 1.4 2001/01/17 23:46:21 dawes Exp $ */

#ifndef _WQ_H
#define _WQ_H

/* 
 * Input is parsed and a work queue is built that is executed later.  This
 * allows us to swap keys as well as ensure that we don't mess up the keyboard
 * by doing a partial rebind.
 */

enum opcode { doKeycode, doAddModifier, doRemoveModifier, doClearModifier,
	      doPointer };

struct op_generic {
    enum opcode type;			/* oneof enum opcode */
    union op *next;			/* next element in list or NULL */
};


/*
 * keycode KEYCODE = KEYSYM
 * keysym OLDKEYSYM = NEWKEYSYM
 *
 * want to eval the OLDKEYSYM before executing the work list so that it isn't
 * effected by any assignments.
 */

struct op_keycode {
    enum opcode type;			/* doKeycode */
    union op *next;			/* next element in list or NULL */
    KeyCode target_keycode;		/* key to which we are assigning */
    int count;				/* number of new keysyms */
    KeySym *keysyms;			/* new values to insert */
};


/*
 * add MODIFIER = KEYSYM ...
 */

struct op_addmodifier {
    enum opcode type;			/* doAddModifier */
    union op *next;			/* next element in list or NULL */
    int modifier;			/* index into modifier list */
    int count;				/* number of keysyms */
    KeySym *keysyms;			/* new values to insert */
};


/*
 * remove MODIFIER = OLDKEYSYM ...
 *
 * want to eval the OLDKEYSYM before executing the work list so that it isn't
 * effected by any assignments.
 */

struct op_removemodifier {
    enum opcode type;			/* doRemoveModifier */
    union op *next;			/* next element in list or NULL */
    int modifier;			/* index into modifier list */
    int count;				/* number of keysyms */
    KeyCode *keycodes;			/* old values to remove */
};


/*
 * clear MODIFIER
 */

struct op_clearmodifier {
    enum opcode type;			/* doClearModifier */
    union op *next;			/* next element in list or NULL */
    int modifier;			/* index into modifier list */
};

/*
 * pointer = NUMBER ...
 *
 * set pointer map to the positive numbers given on the right hand side
 */

#define MAXBUTTONCODES 256		/* there are eight bits of buttons */

struct op_pointer {
    enum opcode type;			/* doPointer */
    union op *next;			/* next element in list or NULL */
    int count;				/* number of new button codes */
    unsigned char button_codes[MAXBUTTONCODES];
};


/*
 * all together now
 */
union op {
    struct op_generic generic;
    struct op_keycode keycode;
    struct op_addmodifier addmodifier;
    struct op_removemodifier removemodifier;
    struct op_clearmodifier clearmodifier;
    struct op_pointer pointer;
};

extern struct wq {
    union op *head;
    union op *tail;
} work_queue;


extern struct modtab {
    char *name;
    int length;
    int value;
} modifier_table[];

#define AllocStruct(s) ((s *) malloc (sizeof (s)))

#define MAXKEYSYMNAMESIZE 80		/* absurdly large */

#endif
