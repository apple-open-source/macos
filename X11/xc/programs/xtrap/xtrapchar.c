/* $XFree86: xc/programs/xtrap/xtrapchar.c,v 1.3 2001/12/12 00:43:50 dawes Exp $ */
/*
 * @DEC_COPYRIGHT@
 */
/*
 * HISTORY
 * Log: xtrapchar.c,v $
 * Revision 1.1.2.2  1993/12/14  12:37:15  Kenneth_Miller
 * 	ANSI-standardize code and turn client build on
 * 	[1993/12/09  20:15:33  Kenneth_Miller]
 *
 * EndLog$
 */
#if !defined(lint) && 0
static char *rcsid = "@(#)RCSfile: xtrapchar.c,v $ Revision: 1.1.2.2 $ (DEC) Date: 1993/12/14 12:37:15 $";
#endif
/*****************************************************************************
Copyright 1987, 1988, 1989, 1990, 1991, 1993 by Digital Equipment Corp., 
Maynard, MA

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*****************************************************************************/
#define ProgName "xtrapchar"
/*
**++
**  FACILITY:  xtrapchar - Converts ANSI character sequences to X.
**
**  MODULE DESCRIPTION:
**
**      Parses ANSI character sequences including application program 
**      sequences to synthesize input events to X Window servers 
**      using the XTrap server extension.  Additionally, this main
**      module is designed to be used with the voice
**      recognition systems which will allow voice input into X Servers.
**
**  AUTHORS:
**
**      Kenneth B. Miller
**
**  CREATION DATE:  March 23, 1991
**
**  DESIGN ISSUES:
**
**      Accepts non-buffered Ascii characters as input and 
**      performs a table look-up to determine what the corresponding
**      X actions are to be performed.
**
**      Uses chparse() which was contributed to DECUS C by Roy
**      Lomicka and later revised by Martin Minow and myself.
**      Also, getopt() is used to parse the command
**      line arguments prior to calling XtAppInitialize().
**      Currently only the -v argument is supported to indicate
**      echoing of characters received for debugging
**      purposes.
**
**
**  CAVEAT:
**
**      This program has *only* been used with Digital Workstations
**      using the LK201 compatible keyboard.  Though reasonable
**      effort was done to maintain portability, no claims are made
**      as to the current level of portability to non-DEC servers
**      for this program.
**--
*/
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <X11/extensions/xtraplib.h>
#include <X11/extensions/xtraplibp.h>
#include <X11/keysym.h>

#include "chparse.h"

#ifndef vaxc
#define globalref extern
#endif
    /* Special private indicators */
#define BPRESS      '!'
#define BRELEASE    '"'
#define BCLICK      '#'
#define APRESS      '$'
#define ARELEASE    '%'
#define CPRESS      '('
#define CRELEASE    ')'
#define SPRESS      '+'
#define SRELEASE    '-'
#define DPRIVATE    '='
#define MNOTIFY     '>'
#define RMNOTIFY    '?'

#define	NPARAM	8
#define	NINTER	8
#define	NUL	0x00
#define	CAN	0x18
#define	SUB	0x1A
#define	ESC	0x1B
#define DEL	0x7F
#define SS3     0x8f
#define	DCS	0x90
#define	CSI	0x9B
#define	ST	0x9C
#define	OSC	0x9D
#define	PM	0x9E
#define	APC	0x9F

static BOOL     verbose_flag = FALSE;
static INT16    column = 0;
static int	state = NUL;	/* Parser state (n.z. if incomplete)	*/
static	Window root;
static BOOL passive_shift; /* Cap's assumed?                       */
static BOOL passive_ctrl;  /* Control key assumed?                 */
static BOOL shift;         /* Cap's on?                            */
static BOOL ctrl;          /* Control key?                         */
static BOOL alt;           /* Alt key?                             */
KeyCode alt_code;
KeyCode ctrl_code;
KeyCode shift_code;



#define _AdjustCol(length) \
    if ((column += length) >= 79) \
    { \
        printf("\n"); \
        column = length; \
    }

static void KeyClick(XETC *tc, KeyCode keycode)
{
    if (passive_ctrl && !ctrl)
    {
        XESimulateXEventRequest(tc, KeyPress, ctrl_code, 0, 0, 0);
    }
    if (passive_shift && !shift)
    {
        XESimulateXEventRequest(tc, KeyPress, shift_code, 0, 0, 0);
    }
    XESimulateXEventRequest(tc, KeyPress, keycode, 0, 0, 0);
    XESimulateXEventRequest(tc, KeyRelease, keycode, 0, 0, 0);
    if (passive_shift && !shift)
    {
        XESimulateXEventRequest(tc, KeyRelease, shift_code, 0, 0, 0);
    }
    if (passive_ctrl && !ctrl)
    {
        XESimulateXEventRequest(tc, KeyRelease, ctrl_code, 0, 0, 0);
    }
    passive_ctrl = passive_shift = FALSE;   /* Action's been completed */
}


/*
**
** FORWARD DECLARATIONS
**
*/
static int get_csi_key ( XETC *tc , int private , int param [], 
    int nparam , int inter [], int ninter , int final );
static int get_ss3_key ( XETC *tc , int private , int param [], 
    int nparam , int inter [], int ninter , int final );
static void send_special ( XETC *tc , int private , int param [],
    int nparam , int inter [], int ninter , int final );
static KeyCode get_typical_char ( XETC *tc , CARD32 keysym);
static KeyCode get_keycode ( XETC *tc , KeySym keysym);


int
main(int argc, char *argv[])
{
    Widget appW;
    Display *dpy;
    XETrapGetCurRep   ret_cur;
    XETC    *tc;
    XtAppContext app;
    char *tmp = NULL;
    INT16 ch;
    INT16 i;
    KeyCode keycode;
    /* ESC & CSI Parsing variables */
    int	max_delay  = 3;
    int	rest_delay = 2;
    int	private;        /* Sequence private char, 'X' if error	*/
    int	param[NPARAM];	/* numeric param, starting at param[1]	*/
    int	nparam;	        /* Number of parameters			*/
    int	inter[NINTER];	/* intermediate char, starting at [1]	*/
    int	ninter;	        /* Number of intermediates		*/
    int	final;  	/* Sequence terminator			*/
    int *popterr;
#ifndef vms
    popterr = &opterr;
#else
    popterr = XEgetopterr();
#endif
    *popterr = 0; /* don't complain about -d for display */
    while ((ch = getopt(argc, argv, "d:v")) != EOF)
    {
        switch(ch)
        {
            case 'v':
                verbose_flag = TRUE;
                break;
            case 'd':   /* -display, let's let the toolkit parse it */
                break;
            default:
                break;
        }
    }
    appW = XtAppInitialize(&app,"XTrap",(XrmOptionDescList)NULL,(Cardinal)0L,
        (int *)&argc, (String *)argv, NULL,(ArgList)&tmp,
        (Cardinal)0);

    dpy = XtDisplay(appW);
    if (verbose_flag)
    {
        printf("Display:  %s \n", DisplayString(dpy));
    }
    if ((tc = XECreateTC(dpy,0L, NULL)) == False)
    {
        fprintf(stderr,"%s: could not initialize XTrap extension\n", ProgName);
        exit (1L);
    }
    root = RootWindow(dpy,DefaultScreen(dpy));
    (void)XEStartTrapRequest(tc);
    alt_code   = XKeysymToKeycode(tc->dpy,XK_Alt_L);
    ctrl_code  = XKeysymToKeycode(tc->dpy,XK_Control_L);
    shift_code = XKeysymToKeycode(tc->dpy,XK_Shift_L);



    if (verbose_flag)
    {
        (void)XEGetCurrentRequest(tc,&ret_cur);
        XEPrintCurrent(stderr,&ret_cur);
    }

    column = 0; /* if displaying char's, don't go beyond 80 columns */

    while ((ch = chparse(max_delay, rest_delay, &state, &private, param, 
        &nparam, inter, &ninter, &final)) != -1)
    {
        if (ch == -2)
        {
            continue;   /* timeout condition */
        }
        if ((!ferror(stdin)) && (!feof(stdin)) && (state == 0))
        {   /* we got something */
            switch(ch)
            {
                case CSI:   /* Control Sequence */
                    keycode = get_csi_key(tc, private, param, nparam, inter,
                        ninter, final);
                    if (keycode)
                        KeyClick(tc, keycode);
                    break;
                case SS3:   /* Keypad stuff */
                    keycode = get_ss3_key(tc, private, param, nparam, inter,
                        ninter, final);
                    if (keycode)
                        KeyClick(tc, keycode);
                    break;
                case APC:   /* Application Cmd (Button's, Press, Release) */
                    send_special(tc, private, param, nparam, inter, ninter,
                        final);
                    break;
                case ESC:   /* Escape Sequence */
                    /* send ESCAPE */
                    if (!(keycode = XKeysymToKeycode(tc->dpy,XK_Escape)))
                    {   /* must be an LK201 keyboard */
                        BOOL orig_ctrl = ctrl;
                        /* 
                         * the following is kind of strange.  We need to
                         * have ctrl TRUE for get_typical_char() to 
                         * report the verbose message correctly.  We
                         * can't use passive_ctrl cause it resets it.
                         * Then, for KeyClick(), ctrl has to be FALSE
                         * and passive_ctrl has to be TRUE in order for
                         * us to get the desired <CTRL>[ to simulate
                         * an escape key.  Once it's all done, we need
                         * to return ctrl to whatever it was and clear
                         * the passive_ctrl.
                         */
                        ctrl = TRUE;                /* for get_typical_char */
                        keycode = get_typical_char(tc, (CARD32)'[');
                        ctrl         = FALSE;       /* for KeyClick */
                        passive_ctrl = TRUE;        /* for KeyClick */
                        KeyClick(tc, keycode);
                        passive_ctrl  = FALSE;      /* to continue */
                        ctrl          = orig_ctrl;  /* to continue */
                    }
                    else
                    {
                        KeyClick(tc, keycode);
                        if (verbose_flag)
                        {
                            _AdjustCol(strlen("<ESC>"));
                            printf("<ESC>");
                        }
                    }
                    /* send private (if valid) */
        	    if (private != NUL && private != 'X' &&
                       (keycode = get_typical_char(tc, (CARD32)private)))
                        KeyClick(tc, keycode);
                    /* send addt'l parameters, if any */
                    for (i = 1; i <= nparam; i++)
                        if ((keycode = get_typical_char(tc, (CARD32)param[i])))
                            KeyClick(tc, keycode);
                    /* send intermediate's, if any */
                    for (i = 1; i <= ninter; i++)
                        if ((keycode = get_typical_char(tc, (CARD32)inter[i])))
                            KeyClick(tc, keycode);
                    /* send final character */
                    if ((keycode = get_typical_char(tc, (CARD32)final)))
                        KeyClick(tc, keycode);
                    break;

                case DCS:   /* We don't deal with these */
                case OSC:
                case PM:
                    if (verbose_flag)
                    {
                        printf("Ignoring the following: ");
                        dumpsequence(state, ch, private, param, nparam, 
                            inter, ninter, final, &column);
                    }
                    break;
                default:    /* typical character */
                    keycode = get_typical_char(tc, (CARD32)ch);
                    if (keycode)
                        KeyClick(tc, keycode);
                    break;
            }
        }
        else
        {   /* error? */
            if (ferror(stdin) || state != 0)
            {
                perror("Error occurred parsing input characters!\n");
            }
            break;
        }
    }
    /* Clean things up */
    XEFreeTC(tc);      
    (void)XCloseDisplay(dpy);

    exit(0L);
}

static int get_csi_key(tc, private, param, nparam, inter, ninter, final)
    XETC    *tc;
    int     private;
    int     param[], nparam;
    int     inter[], ninter;
    int     final;
{
    KeySym keysym = 0;
    switch(param[1])
    {
        case 0:
            switch ((char )final)
            {
                case 'A':    keysym = XK_Up;         break;
                case 'B':    keysym = XK_Down;       break;
                case 'C':    keysym = XK_Right;      break;
                case 'D':    keysym = XK_Left;       break;
                default:
                    dumpsequence(state, CSI, private, param, nparam, 
                        inter, ninter, final, &column);
                    break;
            }
            break;
        case 1:     keysym = XK_Find;       break;
        case 2:     keysym = XK_Insert;     break;
#ifdef DXK_Remove
        case 3:     keysym = DXK_Remove;    break;
#endif
        case 4:     keysym = XK_Select;     break;
        case 5:     keysym = XK_Prior;      break;
        case 6:     keysym = XK_Next;       break;
        case 17:    keysym = XK_F6;         break;
        case 18:    keysym = XK_F7;         break;
        case 19:    keysym = XK_F8;         break;
        case 20:    keysym = XK_F9;         break;
        case 21:    keysym = XK_F10;        break;
        case 23:    keysym = XK_F11;        break;
        case 24:    keysym = XK_F12;        break;
        case 25:    keysym = XK_F13;        break;
        case 26:    keysym = XK_F14;        break;
        case 28:    keysym = XK_Help;       break;
        case 29:    keysym = XK_Menu;       break;
        case 31:    keysym = XK_F17;        break;
        case 32:    keysym = XK_F18;        break;
        case 33:    keysym = XK_F19;        break;
        case 34:    keysym = XK_F20;        break;
        default:
            dumpsequence(state, CSI, private, param, nparam, 
                inter, ninter, final, &column);
    }

    return(get_keycode(tc, keysym));
}

    /*
     * XTrap special sequences:
     * ButtonPress:     <APC>=!X        (where 'X' is 'A', 'B', or 'C' 
     *                                  for MB1, MB2, MB3 respectively)
     * ButtonRelease:   <APC>="X        (where 'X' is 'A', 'B', or 'C' 
     *                                  for MB1, MB2, MB3 respectively)
     * ButtonClick:     <APC>=#X        (where 'X' is 'A', 'B', or 'C' 
     *                                  for MB1, MB2, MB3 respectively)
     * AltPress:        <APC>=$~
     * AltRelease:      <APC>=%~
     * CtrlPress:       <APC>=(~
     * CtrlRelease:     <APC>=)~
     * ShiftPress:      <APC>=+~
     * ShiftRelease:    <APC>=-~
     * MotionNotify:    <APC>>;X;Y~     (where 'X' is the X coord and 'Y'
     *                                  is the Y coord of the desired
     *                                  pointer position)
     * Relative MotionNotify:    
     *                  <APC>?;X;Y~     (where 'X' is the X coord and 'Y'
     *                                  is the Y coord of the desired
     *                                  pointer position)
     *
     */    
static void send_special(tc, private, param, nparam, inter, ninter, final)
    XETC    *tc;
    int     private;
    int     param[], nparam;
    int     inter[], ninter;
    int     final;
{
    switch(private)
    {
        case DPRIVATE:   /* default APC */
            if (ninter != 1)
            {   /* Not my sequence */
                dumpsequence(state, APC, private, param, nparam, 
                    inter, ninter, final, &column);
                return;
            }
            else
            {
                switch(inter[1])
                {
                    Window rid, wid;
                    int x, y, wx, wy;
                    unsigned int sm;
                    CARD8 detail;
        
                    case BPRESS:
                        detail = (final - 'A' + 1);
                        if ((Bool)XQueryPointer(tc->dpy,root,&rid,&wid,&x,
                            &y,&wx,&wy,&sm) == False)
                        {
                            fprintf(stderr, "\nPointer's not on screen 0!\n");
                        }
                        else
                        {
                            XESimulateXEventRequest(tc, ButtonPress, detail, 
                                x, y, 0);
                            if (verbose_flag)
                            {
                                _AdjustCol(strlen("<MB%d-Press> ")-1);
                                printf("<MB%d-Press> ", detail);
                            }
                        }
                        break;
                    case BRELEASE:
                        detail = (final - 'A' + 1);
                        if ((Bool)XQueryPointer(tc->dpy,root,&rid,&wid,&x,
                            &y,&wx,&wy,&sm) == False)
                        {
                            fprintf(stderr, "\nPointer's not on screen 0!\n");
                        }
                        else
                        {
                            XESimulateXEventRequest(tc, ButtonRelease, detail,
                                x,y,0);
                            if (verbose_flag)
                            {
                                _AdjustCol(strlen("<MB%d-Release> ")-1);
                                printf("<MB%d-Release> ", detail);
                            }
                        }
                        break;
                    case BCLICK:
                        detail = (final - 'A' + 1);
                        if (XQueryPointer(tc->dpy,root,&rid,&wid,&x,&y,
                            &wx,&wy,&sm)
                            == False)
                        {
                            fprintf(stderr, "\nPointer's not on screen 0!\n");
                        }
                        else
                        {
                            XESimulateXEventRequest(tc,ButtonPress,
                                detail,x,y,0);
                            XESimulateXEventRequest(tc,ButtonRelease,
                                detail,x,y,0);
                            if (verbose_flag)
                            {
                                _AdjustCol(strlen("<MB%d> ")-1);
                                printf("<MB%d> ", detail);
                            }
                        }
                        break;
                    case APRESS:
                        alt = TRUE;
                        XESimulateXEventRequest(tc,KeyPress,alt_code,0,0,0);
                        break;
                    case ARELEASE:
                        alt = FALSE;
                        XESimulateXEventRequest(tc,KeyRelease,alt_code,0,0,0);
                        break;
                    case SPRESS:
                        shift = TRUE;
                        XESimulateXEventRequest(tc,KeyPress,shift_code,0,0,0);
                        break;
                    case SRELEASE:
                        shift = FALSE;
                        XESimulateXEventRequest(tc,KeyRelease,shift_code,
                            0,0,0);
                        break;
                    case CPRESS:
                        ctrl = TRUE;
                        XESimulateXEventRequest(tc,KeyPress,ctrl_code,0,0,0);
                        break;
                    case CRELEASE:
                        ctrl = FALSE;
                        XESimulateXEventRequest(tc,KeyRelease,ctrl_code,0,0,0);
                        break;
                    default:
                        fprintf(stderr, "\nInvalid Sequence!\n");
                        dumpsequence(state, APC, private, param, nparam, 
                            inter, ninter, final, &column);
                }
            }
            break;
        case MNOTIFY:
            if (nparam != 3)
            {   /* Not my sequence */
                dumpsequence(state, APC, private, param, nparam, 
                    inter, ninter, final, &column);
                return;
            }
            else
            {
                int x, y;

                x = param[2];
                y = param[3];
                XESimulateXEventRequest(tc,MotionNotify,0,x,y,0);
                if (verbose_flag)
                {
                    _AdjustCol(strlen("<M %d,%d> ")+3);
                    printf("<M %d,%d> ",x,y);
                }
            }
            break;
        case RMNOTIFY:
            if (nparam != 3)
            {   /* Not my sequence */
                dumpsequence(state, APC, private, param, nparam, 
                    inter, ninter, final, &column);
                return;
            }
            else
            {
                Window rid, wid;
                int x, y, wx, wy;
                unsigned int sm;

                if (XQueryPointer(tc->dpy,root,&rid,&wid,&x,&y,&wx,&wy,&sm)
                    == False)
                {
                    fprintf(stderr, "\nPointer's not on screen 0!\n");
                }
                else
                {   /* We're ready to go */
                    x += param[2];
                    y += param[3];
                    XESimulateXEventRequest(tc,MotionNotify,0,x,y,0);
                    if (verbose_flag)
                    {
                        _AdjustCol(strlen("<RM ddd+sddd,dddd+sdddd> "));
                        printf("<RM %d+%d,%d+%d> ",x-param[2],param[3],
                            y-param[3],param[3]);
                    }
                }
                break;
            default:
                dumpsequence(state, APC, private, param, nparam, 
                    inter, ninter, final, &column);
                break;
        }
    }
}

static int get_ss3_key(tc, private, param, nparam, inter, ninter, final)
    XETC    *tc;
    int     private;
    int     param[], nparam;
    int     inter[], ninter;
    int     final;
{
    KeySym keysym = 0;
    switch(param[1])
    {
        case 0:
            switch ((char )final)
            {
                case 'A':    keysym = XK_Up;            break;
                case 'B':    keysym = XK_Down;          break;
                case 'C':    keysym = XK_Right;         break;
                case 'D':    keysym = XK_Left;          break;
                case 'p':    keysym = XK_KP_0;          break;
                case 'q':    keysym = XK_KP_1;          break;
                case 'r':    keysym = XK_KP_2;          break;
                case 's':    keysym = XK_KP_3;          break;
                case 't':    keysym = XK_KP_4;          break;
                case 'u':    keysym = XK_KP_5;          break;
                case 'v':    keysym = XK_KP_6;          break;
                case 'w':    keysym = XK_KP_7;          break;
                case 'x':    keysym = XK_KP_8;          break;
                case 'y':    keysym = XK_KP_9;          break;
                case 'm':    keysym = XK_KP_Subtract;   break;
                case 'l':    keysym = XK_KP_Separator;  break;
                case 'n':    keysym = XK_KP_Decimal;    break;
                case 'M':    keysym = XK_KP_Enter;      break;
                case 'P':    keysym = XK_KP_F1;         break;
                case 'Q':    keysym = XK_KP_F2;         break;
                case 'R':    keysym = XK_KP_F3;         break;
                case 'S':    keysym = XK_KP_F4;         break;
                default:
                    dumpsequence(state, SS3, private, param, nparam, 
                        inter, ninter, final, &column);
                    break;
            }
            break;
    }

    return(get_keycode(tc, keysym));
}

static KeyCode get_typical_char(tc, keysym)
    XETC    *tc;
    CARD32  keysym;
{
    if (iscntrl(keysym))
    {   
        switch(keysym)
        {
            case 0x09:  keysym = XK_Tab; break;
            case 0x0d:  keysym = XK_Return; break;
            case 0x7f:  keysym = XK_Delete; break;
            case ESC:   keysym = XK_Escape; break;
        }
    }
    passive_shift = (keysym >= XK_A && keysym <= XK_Z) ? TRUE : FALSE;
    switch(keysym)
    {   /* Special case shift's */
        case '!': case '"': case '@': case '#': case '$':
        case '%': case '^': case '&': case '*': case '(':
        case ')': case '_': case '+': case '{': case '}':
        case '|': case ':': case '>': case '?': case '~':
            passive_shift = TRUE;
    }

    if (keysym >= 1 && keysym <= 26)
    {
        passive_ctrl  = TRUE;
        keysym += 'a' - 1;
    }
    else
    {
        passive_ctrl = FALSE;
    }

    return(get_keycode(tc, keysym));
}

static KeyCode get_keycode(XETC *tc, KeySym keysym)
{
    char *keystr = (char *)XKeysymToString(keysym);
    KeyCode keycode;

    keystr = (keystr == NULL) ? "unknown" : keystr;
    if (verbose_flag)
    {
        if (shift || passive_shift)
        {
            _AdjustCol(strlen("<SHIFT>"));
            printf("<SHIFT>");
        }
        if (alt)
        {
            _AdjustCol(strlen("<ALT>"));
            printf("<ALT>");
        }
        if (ctrl || passive_ctrl)
        {
            _AdjustCol(strlen("<CTRL>"));
            printf("<CTRL>");
        }
        _AdjustCol(strlen(keystr)+1);
        printf("%s ", keystr);
    }
    if (!(keycode = XKeysymToKeycode(tc->dpy,keysym)))
    {
        fprintf(stderr,"\n[%s ('%%0x%04x') returns bad Keycode, ignored]\n",
            keystr, (unsigned int)keysym);
        column = 0;
    }

    return(keycode);
}
