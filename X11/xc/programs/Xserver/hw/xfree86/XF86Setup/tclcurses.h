/* $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/tclcurses.h,v 1.1 1999/04/05 07:13:02 dawes Exp $ */

#include <tcl.h>
#ifdef NCURSES
#include <ncurses.h>
#else
#include <curses.h>
#endif

#define MAXBUTTONS	8

#define BORDER_NONE	0
#define BORDER_BLANK	1
#define BORDER_LINE	2

#define TYPE_NONE	0
#define TYPE_MENU	1
#define TYPE_RADIO	2
#define TYPE_CHECK	3

#define PAIR_SCREEN		1, COLOR_CYAN,  COLOR_BLUE
#define PAIR_ULBORD		2, COLOR_WHITE, COLOR_WHITE
#define PAIR_LRBORD		3, COLOR_BLACK, COLOR_WHITE
#define PAIR_TEXT		4, COLOR_WHITE, COLOR_WHITE
#define PAIR_TITLE		5, COLOR_BLUE,  COLOR_WHITE
#define PAIR_SELECT		6, COLOR_BLUE,  COLOR_WHITE

#define ATTR_SCREEN		COLOR_PAIR(1)
#define ATTR_ULBORD		(COLOR_PAIR(2)|A_BOLD)
#define ATTR_LRBORD		COLOR_PAIR(3)
#define ATTR_TEXT		ATTR_LRBORD
#define ATTR_TITLE		(COLOR_PAIR(5)|A_BOLD)
#define ATTR_SELECT		(COLOR_PAIR(6)|A_REVERSE)

#define KEY_ESC	27

#define SCROLL_LINE	0
#define SCROLL_PAGE	1
#define SCROLL_ALL	2

struct windata {
	WINDOW		*win;
	short		width;
	short		height;
	char		*title;
	short		border;			/* border type */
	short		shadow;			/* Add shadow? */

	struct {
		short		type;
		short		lines;
		int		items;
		int		topline;
		int		current;
		int		rsel;
		int		*chkd;
		WINDOW		*win;
		char		**data;
	} menu;

	struct {
		int		items;		/* lines of text */
		int		topline; 	/* top text line */
		char		*buffer;	/* text area contents */
		char		**data;		/* text lines */
	} text;

	short		butsel;			/* which button is selected */
	char		*button[MAXBUTTONS];	/* button text labels */
};

#define DEBUG1(s)		  if (curs_debug) fprintf(stderr, s)
#define DEBUG2(s,a)		  if (curs_debug) fprintf(stderr, s,a)
#define DEBUG3(s,a,b)		  if (curs_debug) fprintf(stderr, s,a,b)
#define DEBUG4(s,a,b,c)		  if (curs_debug) fprintf(stderr, s,a,b,c)
#define DEBUG5(s,a,b,c,d)	  if (curs_debug) fprintf(stderr, s,a,b,c,d)
#define DEBUG6(s,a,b,c,d,e)	  if (curs_debug) fprintf(stderr, s,a,b,c,d,e)
#define DEBUG7(s,a,b,c,d,e,f)	  if (curs_debug) fprintf(stderr, s,a,b,c,d,e,f)
#define DEBUG8(s,a,b,c,d,e,f,g)	  if (curs_debug) fprintf(stderr, s,a,b,c,d,e,f,g)
#define DEBUG9(s,a,b,c,d,e,f,g,h) if (curs_debug) fprintf(stderr, s,a,b,c,d,e,f,g,h)

#define when	break; case

int TCL_Curses(
#if NeedNestedPrototypes
    ClientData	clientData,
    Tcl_Interp	*interp,
    int		argc,
    char	**argv
#endif
);

void TCL_WinDelete(
#if NeedNestedPrototypes
    ClientData	clientData
#endif
);

int TCL_WinProc(
#if NeedNestedPrototypes
    ClientData	clientData,
    Tcl_Interp	*interp,
    int		argc,
    char	**argv
#endif
);

static int settext(
#if NeedNestedPrototypes
    struct windata	winPtr,
    Tcl_Interp		*interp,
    char		*text
#endif
);

static int updatetext(
#if NeedNestedPrototypes
    struct windata	winPtr,
    Tcl_Interp		*interp,
    char		*text
#endif
);

static int textlines(
#if NeedNestedPrototypes
    struct windata	winPtr
#endif
);

static int setbuttons(
#if NeedNestedPrototypes
    struct windata	winPtr,
    Tcl_Interp		*interp,
    int			argc,
    char		**argv
#endif
);

static int setmenu(
#if NeedNestedPrototypes
    struct windata	winPtr,
    Tcl_Interp		*interp,
    int			argc,
    char		**argv
#endif
);

static int winconfig(
#if NeedNestedPrototypes
    struct windata	winPtr,
    Tcl_Interp		*interp,
    int			argc,
    char		**argv
#endif
);

static int processinput(
#if NeedNestedPrototypes
    struct windata	winPtr,
    Tcl_Interp		*interp
#endif
);

static int updatemenu(
#if NeedNestedPrototypes
    struct windata	winPtr,
    Tcl_Interp		*interp
#endif
);

static void updatebuttons(
#if NeedNestedPrototypes
    struct windata	winPtr
#endif
);

static int updatepos(
#if NeedNestedPrototypes
    struct windata	winPtr,
    Tcl_Interp		*interp,
    int			dir;
    int			amt;
#endif
);

static struct windata * initwinPtr(
#if NeedNestedPrototypes
    WINDOW	*win
#endif
);

static void drawbox(
#if NeedNestedPrototypes
    struct windata	winPtr,
    int			begrow,
    int			begcol,
    int			rows,
    int			cols,
    int			lchar,
    chtype		ulattr,
    chtype		lrattr
#endif
);

