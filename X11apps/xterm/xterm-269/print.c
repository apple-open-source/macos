/* $XTermId: print.c,v 1.121 2011/02/09 10:11:44 tom Exp $ */

/************************************************************

Copyright 1997-2010,2011 by Thomas E. Dickey

                        All Rights Reserved

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name(s) of the above copyright
holders shall not be used in advertising or otherwise to promote the
sale, use or other dealings in this Software without prior written
authorization.

********************************************************/

#include <xterm.h>
#include <data.h>
#include <menu.h>
#include <error.h>
#include <xstrings.h>

#include <stdio.h>

#undef  CTRL
#define	CTRL(c)	((c) & 0x1f)

#define SHIFT_IN  '\017'
#define SHIFT_OUT '\016'

#define CSET_IN   'A'
#define CSET_OUT  '0'

#define isForm(c)      ((c) == '\r' || (c) == '\n' || (c) == '\f')
#define Strlen(a)      strlen((char *)a)
#define Strcmp(a,b)    strcmp((char *)a,(char *)b)
#define Strncmp(a,b,c) strncmp((char *)a,(char *)b,c)

#ifdef VMS
#define VMS_TEMP_PRINT_FILE "sys$scratch:xterm_print.txt"
#endif

static void charToPrinter(XtermWidget /* xw */ ,
			  unsigned /* chr */ );
static void printLine(XtermWidget /* xw */ ,
		      int /* row */ ,
		      unsigned /* chr */ ,
		      PrinterFlags * /* p */ );
static void send_CharSet(XtermWidget /* xw */ ,
			 LineData * /* ld */ );
static void send_SGR(XtermWidget /* xw */ ,
		     unsigned /* attr */ ,
		     unsigned /* fg */ ,
		     unsigned /* bg */ );
static void stringToPrinter(XtermWidget /* xw */ ,
			    const char * /*str */ );

static FILE *Printer;
static pid_t Printer_pid;
static int initialized;

static void
closePrinter(XtermWidget xw GCC_UNUSED)
{
    if (xtermHasPrinter(xw) != 0) {
#ifdef VMS
	TScreen *screen = TScreenOf(xw);

	char pcommand[256];
	(void) sprintf(pcommand, "%s %s;",
		       screen->printer_command,
		       VMS_TEMP_PRINT_FILE);
#endif

	if (Printer != 0) {
	    fclose(Printer);
	    TRACE(("closed printer, waiting...\n"));
#ifdef VMS			/* This is a quick hack, really should use
				   spawn and check status or system services
				   and go straight to the queue */
	    (void) system(pcommand);
#else /* VMS */
	    while (nonblocking_wait() > 0)
#endif /* VMS */
		;
	    Printer = 0;
	    initialized = 0;
	    TRACE(("closed printer\n"));
	}
    }
}

static void
printCursorLine(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);

    TRACE(("printCursorLine\n"));
    printLine(xw, screen->cur_row, '\n', getPrinterFlags(xw, NULL, 0));
}

#define NO_COLOR	((unsigned)-1)

/*
 * DEC's manual doesn't document whether trailing blanks are removed, or what
 * happens with a line that is entirely blank.  This function prints the
 * characters that xterm would allow as a selection (which may include blanks).
 */
static void
printLine(XtermWidget xw, int row, unsigned chr, PrinterFlags * p)
{
    TScreen *screen = TScreenOf(xw);
    int inx = ROW2INX(screen, row);
    LineData *ld;
    Char attr = 0;
    unsigned ch;
    int last = MaxCols(screen);
    int col;
#if OPT_ISO_COLORS && OPT_PRINT_COLORS
#define ColorOf(ld,col) (ld->color[col])
#endif
    unsigned fg = NO_COLOR, last_fg = NO_COLOR;
    unsigned bg = NO_COLOR, last_bg = NO_COLOR;
    int cs = CSET_IN;
    int last_cs = CSET_IN;

    ld = getLineData(screen, inx);
    if (ld == 0)
	return;

    TRACE(("printLine(row=%d/%d, top=%d:%d, chr=%d):%s\n",
	   row, ROW2INX(screen, row), screen->topline, screen->max_row, chr,
	   visibleIChars(ld->charData, (unsigned) last)));

    while (last > 0) {
	if ((ld->attribs[last - 1] & CHARDRAWN) == 0)
	    last--;
	else
	    break;
    }
    if (last) {
	if (p->print_attributes) {
	    send_CharSet(xw, ld);
	    send_SGR(xw, 0, NO_COLOR, NO_COLOR);
	}
	for (col = 0; col < last; col++) {
	    ch = ld->charData[col];
#if OPT_PRINT_COLORS
	    if (screen->colorMode) {
		if (p->print_attributes > 1) {
		    fg = (ld->attribs[col] & FG_COLOR)
			? extract_fg(xw, ColorOf(ld, col), ld->attribs[col])
			: NO_COLOR;
		    bg = (ld->attribs[col] & BG_COLOR)
			? extract_bg(xw, ColorOf(ld, col), ld->attribs[col])
			: NO_COLOR;
		}
	    }
#endif
	    if ((((ld->attribs[col] & SGR_MASK) != attr)
#if OPT_PRINT_COLORS
		 || (last_fg != fg) || (last_bg != bg)
#endif
		)
		&& ch) {
		attr = CharOf(ld->attribs[col] & SGR_MASK);
#if OPT_PRINT_COLORS
		last_fg = fg;
		last_bg = bg;
#endif
		if (p->print_attributes)
		    send_SGR(xw, attr, fg, bg);
	    }

	    if (ch == 0)
		ch = ' ';

#if OPT_WIDE_CHARS
	    if (screen->utf8_mode)
		cs = CSET_IN;
	    else
#endif
		cs = (ch >= ' ' && ch != ANSI_DEL) ? CSET_IN : CSET_OUT;
	    if (last_cs != cs) {
		if (p->print_attributes) {
		    charToPrinter(xw,
				  (unsigned) ((cs == CSET_OUT)
					      ? SHIFT_OUT
					      : SHIFT_IN));
		}
		last_cs = cs;
	    }

	    /* FIXME:  we shouldn't have to map back from the
	     * alternate character set, except that the
	     * corresponding charset information is not encoded
	     * into the CSETS array.
	     */
	    charToPrinter(xw,
			  ((cs == CSET_OUT)
			   ? (ch == ANSI_DEL ? 0x5f : (ch + 0x5f))
			   : ch));
	    if_OPT_WIDE_CHARS(screen, {
		size_t off;
		for_each_combData(off, ld) {
		    ch = ld->combData[off][col];
		    if (ch == 0)
			break;
		    charToPrinter(xw, ch);
		}
	    });
	}
	if (p->print_attributes) {
	    send_SGR(xw, 0, NO_COLOR, NO_COLOR);
	    if (cs != CSET_IN)
		charToPrinter(xw, SHIFT_IN);
	}
    }

    /* finish line (protocol for attributes needs a CR */
    if (p->print_attributes)
	charToPrinter(xw, '\r');

    if (chr && !(p->printer_newline)) {
	if (LineTstWrapped(ld))
	    chr = '\0';
    }

    if (chr)
	charToPrinter(xw, chr);

    return;
}

#define PrintNewLine() (unsigned) (((top < bot) || p->printer_newline) ? '\n' : '\0')

void
xtermPrintScreen(XtermWidget xw, Bool use_DECPEX, PrinterFlags * p)
{
    if (XtIsRealized((Widget) xw)) {
	TScreen *screen = TScreenOf(xw);
	Bool extent = (use_DECPEX && p->printer_extent);
	int top = extent ? 0 : screen->top_marg;
	int bot = extent ? screen->max_row : screen->bot_marg;
	int was_open = initialized;

	TRACE(("xtermPrintScreen, rows %d..%d\n", top, bot));

	while (top <= bot) {
	    printLine(xw, top, PrintNewLine(), p);
	    ++top;
	}
	if (p->printer_formfeed)
	    charToPrinter(xw, '\f');

	if (!was_open || screen->printer_autoclose) {
	    closePrinter(xw);
	}
    } else {
	Bell(xw, XkbBI_MinorError, 0);
    }
}

/*
 * If the alternate screen is active, we'll print only that.  Otherwise, print
 * the normal screen plus all scrolled-back lines.  The distinction is made
 * because the normal screen's buffer is part of the overall scrollback buffer.
 */
void
xtermPrintEverything(XtermWidget xw, PrinterFlags * p)
{
    TScreen *screen = TScreenOf(xw);
    int top = 0;
    int bot = screen->max_row;
    int was_open = initialized;

    if (!screen->whichBuf) {
	top = -screen->savedlines - screen->topline;
	bot -= screen->topline;
    }

    TRACE(("xtermPrintEverything, rows %d..%d\n", top, bot));
    while (top <= bot) {
	printLine(xw, top, PrintNewLine(), p);
	++top;
    }
    if (p->printer_formfeed)
	charToPrinter(xw, '\f');

    if (!was_open || screen->printer_autoclose) {
	closePrinter(xw);
    }
}

static void
send_CharSet(XtermWidget xw, LineData * ld)
{
#if OPT_DEC_CHRSET
    const char *msg = 0;

    switch (GetLineDblCS(ld)) {
    case CSET_SWL:
	msg = "\033#5";
	break;
    case CSET_DHL_TOP:
	msg = "\033#3";
	break;
    case CSET_DHL_BOT:
	msg = "\033#4";
	break;
    case CSET_DWL:
	msg = "\033#6";
	break;
    }
    if (msg != 0)
	stringToPrinter(xw, msg);
#else
    (void) xw;
    (void) ld;
#endif /* OPT_DEC_CHRSET */
}

static void
send_SGR(XtermWidget xw, unsigned attr, unsigned fg, unsigned bg)
{
    char msg[80];
    strcpy(msg, "\033[0");
    if (attr & BOLD)
	strcat(msg, ";1");
    if (attr & UNDERLINE)
	strcat(msg, ";4");	/* typo? DEC documents this as '2' */
    if (attr & BLINK)
	strcat(msg, ";5");
    if (attr & INVERSE)		/* typo? DEC documents this as invisible */
	strcat(msg, ";7");
#if OPT_PRINT_COLORS
    if (bg != NO_COLOR) {
	sprintf(msg + strlen(msg), ";%u", (bg < 8) ? (40 + bg) : (92 + bg));
    }
    if (fg != NO_COLOR) {
#if OPT_PC_COLORS
	if (TScreenOf(xw)->boldColors
	    && fg > 8
	    && (attr & BOLD) != 0)
	    fg -= 8;
#endif
	sprintf(msg + strlen(msg), ";%u", (fg < 8) ? (30 + fg) : (82 + fg));
    }
#else
    (void) bg;
    (void) fg;
#endif
    strcat(msg, "m");
    stringToPrinter(xw, msg);
}

/*
 * This implementation only knows how to write to a pipe.
 */
static void
charToPrinter(XtermWidget xw, unsigned chr)
{
    TScreen *screen = TScreenOf(xw);

    if (!initialized && xtermHasPrinter(xw)) {
#if defined(VMS)
	/*
	 * This implementation only knows how to write to a file.  When the
	 * file is closed the print command executes.  Print command must be of
	 * the form:
	 *   print/que=name/delete [/otherflags].
	 */
	Printer = fopen(VMS_TEMP_PRINT_FILE, "w");
#else
	/*
	 * This implementation only knows how to write to a pipe.
	 */
	FILE *input;
	int my_pipe[2];
	int c;

	if (pipe(my_pipe))
	    SysError(ERROR_FORK);
	if ((Printer_pid = fork()) < 0)
	    SysError(ERROR_FORK);

	if (Printer_pid == 0) {
	    TRACE_CLOSE();
	    close(my_pipe[1]);	/* printer is silent */
	    close(screen->respond);

	    close(fileno(stdout));
	    dup2(fileno(stderr), 1);

	    if (fileno(stderr) != 2) {
		dup2(fileno(stderr), 2);
		close(fileno(stderr));
	    }

	    /* don't want privileges! */
	    if (xtermResetIds(screen) < 0)
		exit(1);

	    Printer = popen(screen->printer_command, "w");
	    input = fdopen(my_pipe[0], "r");
	    while ((c = fgetc(input)) != EOF) {
		fputc(c, Printer);
		if (isForm(c))
		    fflush(Printer);
	    }
	    pclose(Printer);
	    exit(0);
	} else {
	    close(my_pipe[0]);	/* won't read from printer */
	    Printer = fdopen(my_pipe[1], "w");
	    TRACE(("opened printer from pid %d/%d\n",
		   (int) getpid(), (int) Printer_pid));
	}
#endif
	initialized++;
    }
    if (Printer != 0) {
#if OPT_WIDE_CHARS
	if (chr > 127) {
	    Char temp[10];
	    *convertToUTF8(temp, chr) = 0;
	    fputs((char *) temp, Printer);
	} else
#endif
	    fputc((int) chr, Printer);
	if (isForm(chr))
	    fflush(Printer);
    }
}

static void
stringToPrinter(XtermWidget xw, const char *str)
{
    while (*str)
	charToPrinter(xw, CharOf(*str++));
}

/*
 * This module implements the MC (Media Copy) and related printing control
 * sequences for VTxxx emulation.  This is based on the description in the
 * VT330/VT340 Programmer Reference Manual EK-VT3XX-TP-001 (Digital Equipment
 * Corp., March 1987).
 */
void
xtermMediaControl(XtermWidget xw, int param, int private_seq)
{
    TRACE(("MediaCopy param=%d, private=%d\n", param, private_seq));

    if (private_seq) {
	switch (param) {
	case 1:
	    printCursorLine(xw);
	    break;
	case 4:
	    setPrinterControlMode(xw, 0);
	    break;
	case 5:
	    setPrinterControlMode(xw, 1);
	    break;
	case 10:		/* VT320 */
	    xtermPrintScreen(xw, False, getPrinterFlags(xw, NULL, 0));
	    break;
	case 11:		/* VT320 */
	    xtermPrintEverything(xw, getPrinterFlags(xw, NULL, 0));
	    break;
	}
    } else {
	switch (param) {
	case -1:
	case 0:
	    xtermPrintScreen(xw, True, getPrinterFlags(xw, NULL, 0));
	    break;
	case 4:
	    setPrinterControlMode(xw, 0);
	    break;
	case 5:
	    setPrinterControlMode(xw, 2);
	    break;
	}
    }
}

/*
 * When in autoprint mode, the printer prints a line from the screen when you
 * move the cursor off that line with an LF, FF, or VT character, or an
 * autowrap occurs.  The printed line ends with a CR and the character (LF, FF
 * or VT) that moved the cursor off the previous line.
 */
void
xtermAutoPrint(XtermWidget xw, unsigned chr)
{
    TScreen *screen = TScreenOf(xw);

    if (screen->printer_controlmode == 1) {
	TRACE(("AutoPrint %d\n", chr));
	printLine(xw, screen->cursorp.row, chr, getPrinterFlags(xw, NULL, 0));
	if (Printer != 0)
	    fflush(Printer);
    }
}

/*
 * When in printer controller mode, the terminal sends received characters to
 * the printer without displaying them on the screen. The terminal sends all
 * characters and control sequences to the printer, except NUL, XON, XOFF, and
 * the printer controller sequences.
 *
 * This function eats characters, returning 0 as long as it must buffer or
 * divert to the printer.  We're only invoked here when in printer controller
 * mode, and handle the exit from that mode.
 */
#define LB '['

int
xtermPrinterControl(XtermWidget xw, int chr)
{
    TScreen *screen = TScreenOf(xw);
    /* *INDENT-OFF* */
    static struct {
	Char seq[5];
	int active;
    } tbl[] = {
	{ { ANSI_CSI, '5', 'i'      }, 2 },
	{ { ANSI_CSI, '4', 'i'      }, 0 },
	{ { ANSI_ESC, LB,  '5', 'i' }, 2 },
	{ { ANSI_ESC, LB,  '4', 'i' }, 0 },
    };
    /* *INDENT-ON* */

    static Char bfr[10];
    static size_t length;
    size_t n;

    TRACE(("In printer:%04X\n", chr));

    switch (chr) {
    case 0:
    case CTRL('Q'):
    case CTRL('S'):
	return 0;		/* ignored by application */

    case ANSI_CSI:
    case ANSI_ESC:
    case '[':
    case '4':
    case '5':
    case 'i':
	bfr[length++] = CharOf(chr);
	for (n = 0; n < sizeof(tbl) / sizeof(tbl[0]); n++) {
	    size_t len = Strlen(tbl[n].seq);

	    if (length == len
		&& Strcmp(bfr, tbl[n].seq) == 0) {
		setPrinterControlMode(xw, tbl[n].active);
		if (screen->printer_autoclose
		    && screen->printer_controlmode == 0)
		    closePrinter(xw);
		length = 0;
		return 0;
	    } else if (len > length
		       && Strncmp(bfr, tbl[n].seq, length) == 0) {
		return 0;
	    }
	}
	length--;

	/* FALLTHRU */

    default:
	for (n = 0; n < length; n++)
	    charToPrinter(xw, bfr[n]);
	bfr[0] = CharOf(chr);
	length = 1;
	return 0;
    }
}

/*
 * If there is no printer command, we will ignore printer controls.
 */
Bool
xtermHasPrinter(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);

    return (strlen(screen->printer_command) != 0);
}

#define showPrinterControlMode(mode) \
		(((mode) == 0) \
		 ? "normal" \
		 : ((mode) == 1 \
		    ? "autoprint" \
		    : "printer controller"))

void
setPrinterControlMode(XtermWidget xw, int mode)
{
    TScreen *screen = TScreenOf(xw);

    if (xtermHasPrinter(xw)
	&& screen->printer_controlmode != mode) {
	TRACE(("%s %s mode\n",
	       (mode
		? "set"
		: "reset"),
	       (mode
		? showPrinterControlMode(mode)
		: showPrinterControlMode(screen->printer_controlmode))));
	screen->printer_controlmode = mode;
	update_print_redir();
    }
}

PrinterFlags *
getPrinterFlags(XtermWidget xw, String * params, Cardinal *param_count)
{
    /* *INDENT-OFF* */
    static const struct {
	const char *name;
	unsigned    offset;
	int	    value;
    } table[] = {
	{ "noFormFeed", XtOffsetOf(PrinterFlags, printer_formfeed), 0 },
	{ "FormFeed",	XtOffsetOf(PrinterFlags, printer_formfeed), 1 },
	{ "noNewLine",	XtOffsetOf(PrinterFlags, printer_newline),  0 },
	{ "NewLine",	XtOffsetOf(PrinterFlags, printer_newline),  1 },
	{ "noAttrs",	XtOffsetOf(PrinterFlags, print_attributes), 0 },
	{ "monoAttrs",	XtOffsetOf(PrinterFlags, print_attributes), 1 },
	{ "colorAttrs", XtOffsetOf(PrinterFlags, print_attributes), 2 },
    };
    /* *INDENT-ON* */

    TScreen *screen = TScreenOf(xw);
    PrinterFlags *result = &(screen->printer_flags);

    TRACE(("getPrinterFlags %d params\n", param_count ? *param_count : 0));

    result->printer_extent = screen->printer_extent;
    result->printer_formfeed = screen->printer_formfeed;
    result->printer_newline = screen->printer_newline;
    result->print_attributes = screen->print_attributes;

    if (param_count != 0 && *param_count != 0) {
	Cardinal j;
	unsigned k;
	for (j = 0; j < *param_count; ++j) {
	    TRACE(("param%d:%s\n", j, params[j]));
	    for (k = 0; k < XtNumber(table); ++k) {
		if (!x_strcasecmp(params[j], table[k].name)) {
		    int *ptr = (int *) (void *) ((char *) result + table[k].offset);
		    TRACE(("...PrinterFlags(%s) %d->%d\n",
			   table[k].name,
			   *ptr,
			   table[k].value));
		    *ptr = table[k].value;
		    break;
		}
	    }
	}
    }

    return result;
}
