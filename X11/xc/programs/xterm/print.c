/*
 * $XFree86: xc/programs/xterm/print.c,v 1.19 2002/08/12 00:36:33 dickey Exp $
 */

/************************************************************

Copyright 1997-2000,2002 by Thomas E. Dickey

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

#include <stdio.h>

#undef  CTRL
#define	CTRL(c)	((c) & 0x1f)

#define SHIFT_IN  '\017'
#define SHIFT_OUT '\016'

#define CSET_IN   'A'
#define CSET_OUT  '0'

#define isForm(c) ((c) == '\r' || (c) == '\n' || (c) == '\f')
#define Strlen(a) strlen((char *)a)
#define Strcmp(a,b) strcmp((char *)a,(char *)b)
#define Strncmp(a,b,c) strncmp((char *)a,(char *)b,c)

#define SGR_MASK (BOLD|BLINK|UNDERLINE|INVERSE)
#ifdef VMS
#define VMS_TEMP_PRINT_FILE "sys$scratch:xterm_print.txt"
#endif

static void charToPrinter(int chr);
static void printLine(int row, int chr);
static void send_CharSet(int row);
static void send_SGR(unsigned attr, int fg, int bg);
static void stringToPrinter(char *str);

static FILE *Printer;
static int Printer_pid;
static int initialized;

static void
closePrinter(void)
{
    if (xtermHasPrinter() != 0) {
#ifdef VMS
	TScreen *screen = &term->screen;

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
printCursorLine(void)
{
    TScreen *screen = &term->screen;

    TRACE(("printCursorLine\n"));
    printLine(screen->cur_row, '\n');
}

/*
 * DEC's manual doesn't document whether trailing blanks are removed, or what
 * happens with a line that is entirely blank.  This function prints the
 * characters that xterm would allow as a selection (which may include blanks).
 */
static void
printLine(int row, int chr)
{
    TScreen *screen = &term->screen;
    Char *c = SCRN_BUF_CHARS(screen, row);
    Char *a = SCRN_BUF_ATTRS(screen, row);
    Char attr = 0;
    unsigned ch;
    int last = screen->max_col + 1;
    int col;
#if OPT_ISO_COLORS && OPT_PRINT_COLORS
#if OPT_EXT_COLORS
    Char *fbf = 0;
    Char *fbb = 0;
#define ColorOf(col) ((fbf[col] << 8) | fbb[col])
#else
    Char *fb = 0;
#define ColorOf(col) (fb[col])
#endif
#endif
    int fg = -1, last_fg = -1;
    int bg = -1, last_bg = -1;
    int cs = CSET_IN;
    int last_cs = CSET_IN;

    TRACE(("printLine(row=%d, top=%d:%d, chr=%d):%s\n",
	   row, screen->topline, screen->max_row, chr,
	   visibleChars(PAIRED_CHARS(c,
				     screen->utf8_mode
				     ? SCRN_BUF_WIDEC(screen, row)
				     : 0), last)));

    if_OPT_EXT_COLORS(screen, {
	fbf = SCRN_BUF_FGRND(screen, row);
	fbb = SCRN_BUF_BGRND(screen, row);
    });
    if_OPT_ISO_TRADITIONAL_COLORS(screen, {
	fb = SCRN_BUF_COLOR(screen, row);
    });
    while (last > 0) {
	if ((a[last - 1] & CHARDRAWN) == 0)
	    last--;
	else
	    break;
    }
    if (last) {
	if (screen->print_attributes) {
	    send_CharSet(row);
	    send_SGR(0, -1, -1);
	}
	for (col = 0; col < last; col++) {
	    ch = c[col];
	    if_OPT_WIDE_CHARS(screen, {
		ch = getXtermCell(screen, row, col);
	    });
#if OPT_PRINT_COLORS
	    if (screen->colorMode) {
		if (screen->print_attributes > 1) {
		    fg = (a[col] & FG_COLOR)
			? extract_fg(ColorOf(col), a[col])
			: -1;
		    bg = (a[col] & BG_COLOR)
			? extract_bg(ColorOf(col), a[col])
			: -1;
		}
	    }
#endif
	    if ((((a[col] & SGR_MASK) != attr)
#if OPT_PRINT_COLORS
		 || (last_fg != fg) || (last_bg != bg)
#endif
		)
		&& ch) {
		attr = (a[col] & SGR_MASK);
		last_fg = fg;
		last_bg = bg;
		if (screen->print_attributes)
		    send_SGR(attr, fg, bg);
	    }

	    if (ch == 0)
		ch = ' ';

#if OPT_WIDE_CHARS
	    if (screen->utf8_mode)
		cs = CSET_IN;
	    else
#endif
		cs = (ch >= ' ' && ch != 0x7f) ? CSET_IN : CSET_OUT;
	    if (last_cs != cs) {
		if (screen->print_attributes) {
		    charToPrinter((cs == CSET_OUT)
				  ? SHIFT_OUT
				  : SHIFT_IN);
		}
		last_cs = cs;
	    }

	    /* FIXME:  we shouldn't have to map back from the
	     * alternate character set, except that the
	     * corresponding charset information is not encoded
	     * into the CSETS array.
	     */
	    charToPrinter((cs == CSET_OUT)
			  ? (ch == 0x7f ? 0x5f : (ch + 0x5f))
			  : ch);
	}
	if (screen->print_attributes) {
	    send_SGR(0, -1, -1);
	    if (cs != CSET_IN)
		charToPrinter(SHIFT_IN);
	}
    }
    if (screen->print_attributes)
	charToPrinter('\r');
    charToPrinter(chr);
}

void
xtermPrintScreen(Boolean use_DECPEX)
{
    TScreen *screen = &term->screen;
    Boolean extent = (use_DECPEX && screen->printer_extent);
    int top = extent ? 0 : screen->top_marg;
    int bot = extent ? screen->max_row : screen->bot_marg;
    int was_open = initialized;

    TRACE(("xtermPrintScreen, rows %d..%d\n", top, bot));

    while (top <= bot)
	printLine(top++, '\n');
    if (screen->printer_formfeed)
	charToPrinter('\f');

    if (!was_open || screen->printer_autoclose) {
	closePrinter();
    }
}

/*
 * If the alternate screen is active, we'll print only that.  Otherwise, print
 * the normal screen plus all scrolled-back lines.  The distinction is made
 * because the normal screen's buffer is part of the overall scrollback buffer.
 */
static void
xtermPrintEverything(void)
{
    TScreen *screen = &term->screen;
    int top = 0;
    int bot = screen->max_row;
    int was_open = initialized;

    if (!screen->altbuf)
	top = -screen->savedlines;

    TRACE(("xtermPrintEverything, rows %d..%d\n", top, bot));
    while (top <= bot)
	printLine(top++, '\n');
    if (screen->printer_formfeed)
	charToPrinter('\f');

    if (!was_open || screen->printer_autoclose) {
	closePrinter();
    }
}

static void
send_CharSet(int row)
{
#if OPT_DEC_CHRSET
    TScreen *screen = &term->screen;
    char *msg = 0;

    switch (SCRN_BUF_CSETS(screen, row)[0]) {
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
	stringToPrinter(msg);
#endif /* OPT_DEC_CHRSET */
}

static void
send_SGR(unsigned attr, int fg, int bg)
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
    if (bg >= 0) {
	sprintf(msg + strlen(msg), ";%d", (bg < 8) ? (40 + bg) : (92 + bg));
    }
    if (fg >= 0) {
#if OPT_PC_COLORS
	if (term->screen.boldColors
	    && fg > 8
	    && (attr & BOLD) != 0)
	    fg -= 8;
#endif
	sprintf(msg + strlen(msg), ";%d", (fg < 8) ? (30 + fg) : (82 + fg));
    }
#endif
    strcat(msg, "m");
    stringToPrinter(msg);
}

/*
 * This implementation only knows how to write to a pipe.
 */
static void
charToPrinter(int chr)
{
    TScreen *screen = &term->screen;

    if (!initialized && xtermHasPrinter()) {
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
	    TRACE(((char *) 0));
	    close(my_pipe[1]);	/* printer is silent */
	    close(screen->respond);

	    close(fileno(stdout));
	    dup2(fileno(stderr), 1);

	    if (fileno(stderr) != 2) {
		dup2(fileno(stderr), 2);
		close(fileno(stderr));
	    }

	    setgid(screen->gid);	/* don't want privileges! */
	    setuid(screen->uid);

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
		   (int) getpid(), Printer_pid));
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
	    fputc(chr, Printer);
	if (isForm(chr))
	    fflush(Printer);
    }
}

static void
stringToPrinter(char *str)
{
    while (*str)
	charToPrinter(*str++);
}

/*
 * This module implements the MC (Media Copy) and related printing control
 * sequences for VTxxx emulation.  This is based on the description in the
 * VT330/VT340 Programmer Reference Manual EK-VT3XX-TP-001 (Digital Equipment
 * Corp., March 1987).
 */
void
xtermMediaControl(int param, int private_seq)
{
    TRACE(("MediaCopy param=%d, private=%d\n", param, private_seq));

    if (private_seq) {
	switch (param) {
	case 1:
	    printCursorLine();
	    break;
	case 4:
	    setPrinterControlMode(0);
	    break;
	case 5:
	    setPrinterControlMode(1);
	    break;
	case 10:		/* VT320 */
	    xtermPrintScreen(FALSE);
	    break;
	case 11:		/* VT320 */
	    xtermPrintEverything();
	    break;
	}
    } else {
	switch (param) {
	case -1:
	case 0:
	    xtermPrintScreen(TRUE);
	    break;
	case 4:
	    setPrinterControlMode(0);
	    break;
	case 5:
	    setPrinterControlMode(2);
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
xtermAutoPrint(int chr)
{
    TScreen *screen = &term->screen;

    if (screen->printer_controlmode == 1) {
	TRACE(("AutoPrint %d\n", chr));
	printLine(screen->cursor_row, chr);
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
xtermPrinterControl(int chr)
{
    TScreen *screen = &term->screen;
    /* *INDENT-OFF* */
    static struct {
	Char seq[5];
	int active;
    } tbl[] = {
	{ { CSI, '5', 'i'      }, 2 },
	{ { CSI, '4', 'i'      }, 0 },
	{ { ESC, LB,  '5', 'i' }, 2 },
	{ { ESC, LB,  '4', 'i' }, 0 },
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

    case CSI:
    case ESC:
    case '[':
    case '4':
    case '5':
    case 'i':
	bfr[length++] = chr;
	for (n = 0; n < sizeof(tbl) / sizeof(tbl[0]); n++) {
	    size_t len = Strlen(tbl[n].seq);

	    if (length == len
		&& Strcmp(bfr, tbl[n].seq) == 0) {
		setPrinterControlMode(tbl[n].active);
		if (screen->printer_autoclose
		    && screen->printer_controlmode == 0)
		    closePrinter();
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
	    charToPrinter(bfr[n]);
	bfr[0] = chr;
	length = 1;
	return 0;
    }
}

/*
 * If there is no printer command, we will ignore printer controls.
 */
Boolean
xtermHasPrinter(void)
{
    TScreen *screen = &term->screen;

    return (strlen(screen->printer_command) != 0);
}

#define showPrinterControlMode(mode) \
		(((mode) == 0) \
		 ? "normal" \
		 : ((mode) == 1 \
		    ? "autoprint" \
		    : "printer controller"))

void
setPrinterControlMode(int mode)
{
    if (xtermHasPrinter()
	&& term->screen.printer_controlmode != mode) {
	TRACE(("%s %s mode\n",
	       (mode
		? "set"
		: "reset"),
	       (mode
		? showPrinterControlMode(mode)
		: showPrinterControlMode(term->screen.printer_controlmode))));
	term->screen.printer_controlmode = mode;
	update_print_redir();
    }
}
