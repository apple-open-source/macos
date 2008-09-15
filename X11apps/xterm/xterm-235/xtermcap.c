/* $XTermId: xtermcap.c,v 1.12 2007/12/31 17:27:42 tom Exp $ */

/*
 * Copyright 2007 by Thomas E. Dickey
 *
 *                         All Rights Reserved
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the
 * sale, use or other dealings in this Software without prior written
 * authorization.
 */

#include <xtermcap.h>

#include <X11/keysym.h>

#ifdef VMS
#include <X11/keysymdef.h>
#endif

#include <xstrings.h>

#ifndef HAVE_TIGETSTR
#undef USE_TERMINFO
#endif

#ifndef USE_TERMINFO
#define USE_TERMINFO 0
#endif

#if OPT_TCAP_QUERY || OPT_TCAP_FKEYS

typedef struct {
    char *tc;
    char *ti;
    int code;
    unsigned state;
} TCAPINFO;
/* *INDENT-OFF* */
#define DATA(tc,ti,x,y) { tc, ti, x, y }
static TCAPINFO table[] = {
	/*	tcap	terminfo	code		param */
	DATA(	"%1",	"khlp",		XK_Help,	0	),
	DATA(	"#1",	"kHLP",		XK_Help,	2	),
	DATA(	"@0",	"kfnd",		XK_Find,	0	),
	DATA(	"*0",	"kFND",		XK_Find,	2	),
	DATA(	"*6",	"kslt",		XK_Select,	0	),
	DATA(	"#6",	"kSLT",		XK_Select,	2	),

	DATA(	"kh",	"khome",	XK_Home,	0	),
	DATA(	"#2",	"kHOM",		XK_Home,	2	),
	DATA(	"@7",	"kend",		XK_End,		0	),
	DATA(	"*7",	"kEND",		XK_End,		2	),

	DATA(	"kl",	"kcub1",	XK_Left,	0	),
	DATA(	"kr",	"kcuf1",	XK_Right,	0	),
	DATA(	"ku",	"kcuu1",	XK_Up,		0	),
	DATA(	"kd",	"kcud1",	XK_Down,	0	),

	DATA(	"#4",	"kLFT",		XK_Left,	2	),
	DATA(	"%i",	"kRIT",		XK_Right,	2	),
	DATA(	"%e",	"kPRV",		XK_Up,		2	),
	DATA(	"%c",	"kNXT",		XK_Down,	2	),

	DATA(	"k1",	"kf1",		XK_Fn(1),	0	),
	DATA(	"k2",	"kf2",		XK_Fn(2),	0	),
	DATA(	"k3",	"kf3",		XK_Fn(3),	0	),
	DATA(	"k4",	"kf4",		XK_Fn(4),	0	),
	DATA(	"k5",	"kf5",		XK_Fn(5),	0	),
	DATA(	"k6",	"kf6",		XK_Fn(6),	0	),
	DATA(	"k7",	"kf7",		XK_Fn(7),	0	),
	DATA(	"k8",	"kf8",		XK_Fn(8),	0	),
	DATA(	"k9",	"kf9",		XK_Fn(9),	0	),
	DATA(	"k;",	"kf10",		XK_Fn(10),	0	),

	DATA(	"F1",	"kf11",		XK_Fn(11),	0	),
	DATA(	"F2",	"kf12",		XK_Fn(12),	0	),
	DATA(	"F3",	"kf13",		XK_Fn(13),	0	),
	DATA(	"F4",	"kf14",		XK_Fn(14),	0	),
	DATA(	"F5",	"kf15",		XK_Fn(15),	0	),
	DATA(	"F6",	"kf16",		XK_Fn(16),	0	),
	DATA(	"F7",	"kf17",		XK_Fn(17),	0	),
	DATA(	"F8",	"kf18",		XK_Fn(18),	0	),
	DATA(	"F9",	"kf19",		XK_Fn(19),	0	),
	DATA(	"FA",	"kf20",		XK_Fn(20),	0	),
	DATA(	"FB",	"kf21",		XK_Fn(21),	0	),
	DATA(	"FC",	"kf22",		XK_Fn(22),	0	),
	DATA(	"FD",	"kf23",		XK_Fn(23),	0	),
	DATA(	"FE",	"kf24",		XK_Fn(24),	0	),
	DATA(	"FF",	"kf25",		XK_Fn(25),	0	),
	DATA(	"FG",	"kf26",		XK_Fn(26),	0	),
	DATA(	"FH",	"kf27",		XK_Fn(27),	0	),
	DATA(	"FI",	"kf28",		XK_Fn(28),	0	),
	DATA(	"FJ",	"kf29",		XK_Fn(29),	0	),
	DATA(	"FK",	"kf30",		XK_Fn(30),	0	),
	DATA(	"FL",	"kf31",		XK_Fn(31),	0	),
	DATA(	"FM",	"kf32",		XK_Fn(32),	0	),
	DATA(	"FN",	"kf33",		XK_Fn(33),	0	),
	DATA(	"FO",	"kf34",		XK_Fn(34),	0	),
	DATA(	"FP",	"kf35",		XK_Fn(35),	0	),

	DATA(	"FQ",	"kf36",		-36,		0	),
	DATA(	"FR",	"kf37",		-37,		0	),
	DATA(	"FS",	"kf38",		-38,		0	),
	DATA(	"FT",	"kf39",		-39,		0	),
	DATA(	"FU",	"kf40",		-40,		0	),
	DATA(	"FV",	"kf41",		-41,		0	),
	DATA(	"FW",	"kf42",		-42,		0	),
	DATA(	"FX",	"kf43",		-43,		0	),
	DATA(	"FY",	"kf44",		-44,		0	),
	DATA(	"FZ",	"kf45",		-45,		0	),
	DATA(	"Fa",	"kf46",		-46,		0	),
	DATA(	"Fb",	"kf47",		-47,		0	),
	DATA(	"Fc",	"kf48",		-48,		0	),
	DATA(	"Fd",	"kf49",		-49,		0	),
	DATA(	"Fe",	"kf50",		-50,		0	),
	DATA(	"Ff",	"kf51",		-51,		0	),
	DATA(	"Fg",	"kf52",		-52,		0	),
	DATA(	"Fh",	"kf53",		-53,		0	),
	DATA(	"Fi",	"kf54",		-54,		0	),
	DATA(	"Fj",	"kf55",		-55,		0	),
	DATA(	"Fk",	"kf56",		-56,		0	),
	DATA(	"Fl",	"kf57",		-57,		0	),
	DATA(	"Fm",	"kf58",		-58,		0	),
	DATA(	"Fn",	"kf59",		-59,		0	),
	DATA(	"Fo",	"kf60",		-60,		0	),
	DATA(	"Fp",	"kf61",		-61,		0	),
	DATA(	"Fq",	"kf62",		-62,		0	),
	DATA(	"Fr",	"kf63",		-63,		0	),

	DATA(	"K1",	"ka1",		XK_KP_Home,	0	),
	DATA(	"K4",	"kc1",		XK_KP_End,	0	),

#ifdef XK_ISO_Left_Tab
	DATA(	"kB",	"kcbt",		XK_ISO_Left_Tab, 0	),
#endif
	DATA(	"kC",	"kclr",		XK_Clear,	0	),
	DATA(	"kD",	"kdch1",	XK_Delete,	0	),
	DATA(	"kI",	"kich1",	XK_Insert,	0	),
	DATA(	"kN",	"knp",		XK_Next,	0	),
	DATA(	"kP",	"kpp",		XK_Prior,	0	),
	DATA(	"kb",	"kbs",		XK_BackSpace,	0	),
# if OPT_TCAP_QUERY && OPT_ISO_COLORS
	/* XK_COLORS is a fake code. */
	DATA(	"Co",	"colors",	XK_COLORS,	0	),
# endif
#if USE_TERMINFO && defined(NCURSES_VERSION)
#define DEXT(name, parm, code) DATA("", name, code, parm)
#define D1ST(name, parm, code) DEXT("k" #name, parm, code)
#define DMOD(name, parm, code) DEXT("k" #name #parm, parm, code)

#define DGRP(name, code) \
	D1ST(name, 2, code), \
	DMOD(name, 3, code), \
	DMOD(name, 4, code), \
	DMOD(name, 5, code), \
	DMOD(name, 6, code), \
	DMOD(name, 7, code), \
	DMOD(name, 8, code)

	DGRP(DN,   XK_Down),
	DGRP(LFT,  XK_Left),
	DGRP(RIT,  XK_Right),
	DGRP(UP,   XK_Up),
	DGRP(DC,   XK_Delete),
	DGRP(END,  XK_End),
	DGRP(HOM,  XK_Home),
	DGRP(IC,   XK_Insert),
	DGRP(NXT,  XK_Next),
	DGRP(PRV,  XK_Prior),
#endif
};
#undef DATA
/* *INDENT-ON* */

#if OPT_TCAP_QUERY
static int
hex2int(int c)
{
    if (c >= '0' && c <= '9')
	return c - '0';
    if (c >= 'a' && c <= 'f')
	return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
	return c - 'A' + 10;
    return -1;
}

static TCAPINFO *
lookupTcapByName(const char *name)
{
    TCAPINFO *result = 0;
    Cardinal n;

    if (name != 0 && *name != '\0') {
	for (n = 0; n < XtNumber(table); n++) {
	    if (!strcmp(table[n].ti, name) || !strcmp(table[n].tc, name)) {
		result = table + n;
		break;
	    }
	}
    }
    return result;
}

/*
 * Parse the termcap/terminfo name from the string, returning a positive number
 * (the keysym) if found, otherwise -1.  Update the string pointer.
 * Returns the (shift, control) state in *state.
 *
 * This does not attempt to construct control/shift modifiers to construct
 * function-key values.  Instead, it sets the *fkey flag to pass to Input()
 * and bypass the lookup of keysym altogether.
 */
int
xtermcapKeycode(XtermWidget xw, char **params, unsigned *state, Bool * fkey)
{
    TCAPINFO *data;
    unsigned len = 0;
    int code = -1;
#define MAX_TNAME_LEN 6
    char name[MAX_TNAME_LEN + 1];
    char *p;

    TRACE(("xtermcapKeycode(%s)\n", *params));

    /* Convert hex encoded name to ascii */
    for (p = *params; hex2int(p[0]) >= 0 && hex2int(p[1]) >= 0; p += 2) {
	if (len >= MAX_TNAME_LEN)
	    break;
	name[len++] = (hex2int(p[0]) << 4) + hex2int(p[1]);
    }
    name[len] = 0;
    *params = p;

    *state = 0;
    *fkey = False;

    if (*p == 0 || *p == ';') {
	if ((data = lookupTcapByName(name)) != 0) {
	    code = data->code;
	    *state = xtermParamToState(xw, data->state);
	    if (IsFunctionKey(code)) {
		*fkey = True;
	    } else if (code < 0) {
		*fkey = True;
		code = XK_Fn((-code));
	    }
#if OPT_SUN_FUNC_KEYS
	    if (*fkey && xw->keyboard.type == keyboardIsSun) {
		int num = code - XK_Fn(0);

		/* match function-key case in sunfuncvalue() */
		if (num > 20) {
		    if (num <= 30 || num > 47) {
			code = -1;
		    } else {
			code -= 10;
			switch (num) {
			case 37:	/* khome */
			case 39:	/* kpp */
			case 41:	/* kb2 */
			case 43:	/* kend */
			case 45:	/* knp */
			    code = -1;
			    break;
			}
		    }
		}
	    }
#endif
	}
    }

    TRACE(("... xtermcapKeycode(%s, %u, %d) -> %#06x\n",
	   name, *state, *fkey, code));
    return code;
}
#endif /* OPT_TCAP_QUERY */

#if OPT_TCAP_FKEYS
static TCAPINFO *
lookupTcapByCode(int code, unsigned mask)
{
    TCAPINFO *result = 0;
    Cardinal n;

    TRACE(("lookupTcapByCode %d:%#x\n", code, mask));
    for (n = 0; n < XtNumber(table); n++) {
	if (table[n].code == code &&
	    table[n].state == mask) {
	    TRACE(("lookupTcapByCode %d:%s\n", n, table[n].ti));
	    result = table + n;
	    break;
	}
    }
    return result;
}

#define NO_STRING (char *)(-1)

int
xtermcapString(XtermWidget xw, int keycode, unsigned mask)
{
    int result = 0;
    TCAPINFO *data;
    unsigned param = xtermStateToParam(xw, mask);

    if ((data = lookupTcapByCode(keycode, param)) != 0) {
	TScreen *screen = TScreenOf(xw);
	Cardinal which = data - table;
	char *fkey;

	if (screen->tcap_fkeys == 0) {
	    Cardinal want = XtNumber(table);
	    Cardinal have;
#if !(USE_TERMINFO && defined(HAVE_TIGETSTR))
	    char *area = screen->tcap_area;
#endif

	    if ((screen->tcap_fkeys = TypeCallocN(char *, want)) != 0) {
		for (have = 0; have < want; ++have) {
#if USE_TERMINFO && defined(HAVE_TIGETSTR)
		    fkey = tigetstr(table[have].ti);
#else
		    fkey = tgetstr(table[have].tc, &area);
#endif
		    if (fkey != 0 && fkey != NO_STRING) {
			screen->tcap_fkeys[have] = x_strdup(fkey);
		    } else {
			screen->tcap_fkeys[have] = NO_STRING;
		    }
		}
	    }
	}
	if (screen->tcap_fkeys != 0) {
	    if ((fkey = screen->tcap_fkeys[which]) != NO_STRING) {
		StringInput(xw, (Char *) fkey, strlen(fkey));
		result = 1;
	    }
	}
    }

    TRACE(("xtermcapString(keycode=%#x, mask=%#x) ->%d\n",
	   keycode, mask, result));

    return result;
}
#endif /* OPT_TCAP_FKEYS */

#endif /* OPT_TCAP_QUERY || OPT_TCAP_FKEYS */
/*
 * If we're linked to terminfo, tgetent() will return an empty buffer.  We
 * cannot use that to adjust the $TERMCAP variable.
 */
Bool
get_termcap(char *name, char *buffer)
{
    *buffer = 0;		/* initialize, in case we're using terminfo's tgetent */

#if USE_TERMINFO && defined(NCURSES_VERSION)
    use_extended_names(TRUE);
#endif
    if (name != 0) {
	if (tgetent(buffer, name) == 1) {
	    TRACE(("get_termcap(%s) succeeded (%s)\n", name,
		   (*buffer
		    ? "ok:termcap, we can update $TERMCAP"
		    : "assuming this is terminfo")));
	    return True;
	} else {
	    *buffer = 0;	/* just in case */
	}
    }
    return False;
}
