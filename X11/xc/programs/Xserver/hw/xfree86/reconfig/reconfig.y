/* $XFree86: xc/programs/Xserver/hw/xfree86/reconfig/reconfig.y,v 3.8 1996/12/23 06:51:45 dawes Exp $ */





/* $XConsortium: reconfig.y /main/5 1996/02/21 17:55:14 kaleb $ */

%{
#include <stdio.h>
#include "os.h"
#include "strlist.h"

/* Author (of this quick hack): G.J. Akkerman
 * Usage: reconfig < OldXconfig > NewXconfig
 *
 * Notes:
 * This utility reads an Xconfig file into structures similar to those
 * in the server: one for server-global items, and one for each screen.
 * The datastructures used are however trivialized, since we don't want to
 * interpret data, but only to reshuffle it:
 * + Keywords without arguments are represented by ints (booleans)
 * + For keywords with arguments we use strings to represent those arguments
 *   verbatim. (Even the quotes around strings are preserved.) In this way, we
 *   avoid, for instance, round off errors.
 * After an Xconfig file is successfully read into the structures, we dump
 * them in the new format. We make up any required database keys, and we
 * give an arbitrary (and quite possibly wrong) monitor description.
 * In general, we do little error checking. It is possible that a wrong old
 * Xconfig file gets translated to a wrong new format Xconfig file.
 *
 * This is all the documentation there is. -- GJA.
 */

#define N_SCREENS 5
#define iVGA256		0
#define iVGA2		1
#define iMONO		2
#define iVGA16		3
#define iACCEL		4

#define fINTERLACE	(1 << 0)
#define fPHSYNC		(1 << 1)
#define fNHSYNC		(1 << 2)
#define fPVSYNC		(1 << 3)
#define fNVSYNC 	(1 << 4)
#define fCSYNC 		(1 << 5)

/* Indices for the keymaps */
#define N_KEYM 4
#define iLEFTALT	0
#define iRIGHTALT	1
#define iSCROLLLOCK	2
#define iRIGHTCTL	3

struct mode {
	char *mn;
	char *d;
	char *h1,*h2,*h3,*h4;
	char *v1,*v2,*v3,*v4;
	int flags;
} ;

typedef struct {
	int count;
	struct mode *datap;
} mode_list ;

struct general_struct {
	int Sharedmon;				/* ? */
	string_list *Fontpath;			/* files */
	char *Rgbpath;				/* files */
	int Notrapsignals;			/* serverflags */

	/* Keyboard stuff: */
	char *Autorepeat1, *Autorepeat2;	/* keyboard */
	int Dontzap;				/* serverflags */
	int Servernum;				/* keyboard */
	char *Xleds;				/* keyboard */
	char *Vtinit;				/* keyboard */
	char *keymap[N_KEYM];			/* keyboard */
	int Vtsysreq;				/* keyboard */

	/* Mouse stuff: */
	char *mouse_name;			/* pointer */
	char *mouse_device;			/* pointer */
	char *Baudrate;				/* pointer */
	char *Samplerate;			/* pointer */
	int Emulate3 ;				/* pointer */
	int Chordmiddle ;			/* pointer */
	int Cleardtr ;				/* pointer */
	int Clearrts ;				/* pointer */
} gen ;

struct screen_struct {
	int configured; 
	int Staticgray;				/* screen.display */
	int Grayscale ;				/* screen.display */
	int Staticcolor ;			/* screen.display */
	int Pseudocolor ;			/* screen.display */
	int Truecolor ;				/* screen.display */
	int Directcolor;			/* screen.display */
	char *Chipset;				/* device */
	char *Ramdac;				/* device */
	char *Dacspeed;				/* device */
	char *Clockchip;			/* device */
	string_list_list *Clocks;		/* device */
	char *Displaysize1,*Displaysize2 ;	/* screen.display */
	string_list *Modes;			/* screen.display */
	char *Screenno;				/* screen */
	string_list *Option;			/* device */
	char *Videoram;				/* device */
	char *Viewport1,*Viewport2;		/* screen.display */
	char *Virtual1,*Virtual2;		/* screen.display */
	char *Speedup;				/* device */
	int Nospeedup;				/* device */
	char *Clockprog1, *Clockprog2;		/* device */
		/* string + number */
	char *Biosbase;				/* device */
	char *Membase;				/* device */
	char *Black1, *Black2, *Black3;		/* screen.display */
	char *White1, *White2, *White3;		/* screen.display */
	char *Iobase;				/* device */
	char *Dacbase;				/* device */
	char *Copbase;				/* device */
	char *Posbase;				/* device */
	char *Instance;				/* device */
} screens[N_SCREENS] ;

int scrn_index, kmap_index ; /* Indices */
int flags; /* Covert data path. */
char *modename; /* Covert data path. */

/* This is the *external* mode list. */
mode_list *modelist;

mode_list *add_mode();
string_list_list *add_list();
string_list *add_string();

#define SCRN (screens[scrn_index])

%}

%union {
	char *string;
	char *number;
	string_list *stringlist;
}

%token ACCEL AUTOREPEAT BAUDRATE BIOSBASE BLACK BUSMOUSE
	CHIPSET CHORDMIDDLE CLEARDTR CLEARRTS CLOCKPROG
	CLOCKS CSYNC DIRECTCOLOR DISPLAYSIZE DONTZAP EMULATE3
	FONTPATH GRAYSCALE INTERLACE KEYBOARD K_COMPOSE K_CONTROL
	K_META K_MODELOCK K_MODESHIFT K_SCROLLLOCK LEFTALT LOGIMAN
	LOGITECH MEMBASE MICROSOFT MMHITTAB MMSERIES MODEDB MODES
	MONO MOUSESYS NHSYNC NOSPEEDUP NOTRAPSIGNALS NVSYNC OPTION
	OSMOUSE PHSYNC PSEUDOCOLOR PS_2 PVSYNC RGBPATH RIGHTALT
	RIGHTCTL SAMPLERATE SCREENNO SCROLLLOCK SERVERNUM
	SHAREDMON SPEEDUP STATICCOLOR STATICGRAY TRUECOLOR VGA16 VGA2
	VGA256 VIDEORAM VIEWPORT VIRTUAL VTINIT VTSYSREQ WHITE XLEDS
	XQUE

/* Relatively new keywords: */
%token RAMDAC DACSPEED IOBASE DACBASE COPBASE POSBASE INSTANCE

/* Nowhere used: */
%token WRONG

%type <string> string
%type <number> number
%type <stringlist> clocks strings

%token <string> STRING
%token <number> NUMBER

%% 

file:
	/* empty */ |
	file top ;

top :
	SHAREDMON { gen.Sharedmon = 1; } |
	FONTPATH string { gen.Fontpath = add_string(gen.Fontpath,$2); } |
	RGBPATH string { gen.Rgbpath = $2; } |
	NOTRAPSIGNALS { gen.Notrapsignals = 1; } |
	KEYBOARD keyboardconfig |

	/* We have only one mouse: */
	MICROSOFT string mouseconfig
		{ gen.mouse_device = $2;
		  gen.mouse_name = "microsoft"; } |
	MOUSESYS string mouseconfig
		{ gen.mouse_device = $2;
		  gen.mouse_name = "mousesystems"; } |
	MMSERIES string mouseconfig
		{ gen.mouse_device = $2;
		  gen.mouse_name = "mmseries"; } |
	LOGITECH string mouseconfig
		{ gen.mouse_device = $2;
		  gen.mouse_name = "logitech"; } |
	BUSMOUSE string mouseconfig
		{ gen.mouse_device = $2;
		  gen.mouse_name = "busmouse"; } |
	LOGIMAN string mouseconfig
		{ gen.mouse_device = $2;
		  gen.mouse_name = "mouseman"; } |
	PS_2 string mouseconfig
		{ gen.mouse_device = $2;
		  gen.mouse_name = "ps/2"; } |
	MMHITTAB string mouseconfig
		{ gen.mouse_device = $2;
		  gen.mouse_name = "mmhittab"; } |

	XQUE keyboardconfig mouseconfig
		{ gen.mouse_name = "xqueue"; } |

	OSMOUSE mouseconfig
		{ gen.mouse_name = "osmouse"; } |

	OSMOUSE string mouseconfig
		{ gen.mouse_device = $2;
		  gen.mouse_name = "osmouse"; } |

	/* We might have any graphics section */
	VGA256 { scrn_index = iVGA256; SCRN.configured = 1; }
		graphicsconfig |
	VGA2 { scrn_index = iVGA2; SCRN.configured = 1; }
		graphicsconfig |
	MONO { scrn_index = iMONO; SCRN.configured = 1; }
		graphicsconfig |
	VGA16 { scrn_index = iVGA16; SCRN.configured = 1; }
		graphicsconfig |
	ACCEL { scrn_index = iACCEL; SCRN.configured = 1; }
		graphicsconfig |
	MODEDB modedb ;

modedb :
	/* empty */ { modelist = NULL; } |
	modedb string { modename = $2; } modes ;

modes :
	/* empty */ |
	modes
		number 
		number number number number
		number number number number
		{ flags = 0; } flags
			{ modelist = add_mode(modelist, modename,
				$2, $3,$4,$5,$6, $7,$8,$9,$10, flags); } ;

number : NUMBER { $$ = $1; } ; /* This once had a function */

string : STRING { $$ = $1; } ;

flags :
	/* empty */ |
	flags INTERLACE { flags |= fINTERLACE; } |
	flags PHSYNC { flags |= fPHSYNC; } |
	flags NHSYNC { flags |= fNHSYNC; } |
	flags PVSYNC { flags |= fPVSYNC; } |
	flags NVSYNC { flags |= fNVSYNC; } |
	flags CSYNC { flags |= fCSYNC; } ;
	
graphicsconfig :
	/* empty */ | graphicsconfig gstmt  ;

/* Note: we may have more of these. */
gstmt :
	STATICGRAY { SCRN.Staticgray = 1; } |
	GRAYSCALE { SCRN.Grayscale = 1; } |
	STATICCOLOR { SCRN.Staticcolor = 1; } |
	PSEUDOCOLOR { SCRN.Pseudocolor = 1; } |
	TRUECOLOR { SCRN.Truecolor = 1; } |
	DIRECTCOLOR { SCRN.Directcolor = 1; } |
	CHIPSET string { SCRN.Chipset = $2; } |
	RAMDAC string { SCRN.Ramdac = $2; } |
	DACSPEED number { SCRN.Dacspeed = $2; } |
	CLOCKS string { SCRN.Clockchip = $2; } |
	CLOCKS clocks { SCRN.Clocks = add_list(SCRN.Clocks,$2); } |
	DISPLAYSIZE number number
		{ SCRN.Displaysize1 = $2; SCRN.Displaysize2 = $3; } |
	MODES strings { SCRN.Modes = $2; } |
	SCREENNO number { SCRN.Screenno = $2; } |
	OPTION string { SCRN.Option = add_string(SCRN.Option,$2); } |
	VIDEORAM number { SCRN.Videoram = $2; } |
	VIEWPORT number number
		{ SCRN.Viewport1 = $2; SCRN.Viewport2 = $3; } |
	VIRTUAL number number
		{ SCRN.Virtual1 = $2; SCRN.Virtual2 = $3; } |
	SPEEDUP string { SCRN.Speedup = $2; } |
	SPEEDUP number { SCRN.Speedup = $2; } |
	NOSPEEDUP { SCRN.Nospeedup = 1; } |
	CLOCKPROG string { SCRN.Clockprog1 = $2; } |
	CLOCKPROG string number
		{ SCRN.Clockprog1 = $2; SCRN.Clockprog2 = $3; } |
	BIOSBASE number { SCRN.Biosbase = $2; } |
	MEMBASE number { SCRN.Membase = $2; } |
	BLACK number number number
		{ SCRN.Black1 = $2; SCRN.Black2 = $3; SCRN.Black3 = $4; }  |
	WHITE number number number
		{ SCRN.White1 = $2; SCRN.White2 = $3; SCRN.White3 = $4; }  |
	IOBASE number { SCRN.Iobase = $2; } |
	DACBASE number { SCRN.Dacbase = $2; } |
	COPBASE number { SCRN.Copbase = $2; } |
	POSBASE number { SCRN.Posbase = $2; } |
	INSTANCE number { SCRN.Instance = $2; } ;

clocks :
	/* empty */ { $$ = NULL; } |
	clocks number { $$ = add_string($1,$2); } ;

strings :
	/* empty */ { $$ = NULL; } |
	strings string { $$ = add_string($1,$2); } ;
	

keyboardconfig :
	/* empty */ | keyboardconfig keybstmt ;

keybstmt :
	AUTOREPEAT number number
		{ gen.Autorepeat1 = $2; gen.Autorepeat2 = $3; } |	
	DONTZAP { gen.Dontzap = 1; } |
	SERVERNUM { gen.Servernum = 1; } |
	XLEDS number { gen.Xleds = $2; } |
	VTINIT string { gen.Vtinit = $2; } |
	LEFTALT { kmap_index = iLEFTALT; } keymap |
	RIGHTALT { kmap_index = iRIGHTALT; } keymap |
	SCROLLLOCK { kmap_index = iSCROLLLOCK; } keymap |
	RIGHTCTL { kmap_index = iRIGHTCTL; } keymap |
	VTSYSREQ { gen.Vtsysreq = 1; } ;

keymap :
	K_META { gen.keymap[kmap_index] = "meta"; } |
	K_COMPOSE { gen.keymap[kmap_index] = "compose"; } |
	K_MODESHIFT { gen.keymap[kmap_index] = "modeshift"; } |
	K_MODELOCK { gen.keymap[kmap_index] = "modelock"; } |
	K_SCROLLLOCK { gen.keymap[kmap_index] = "scrollock"; } |
	K_CONTROL { gen.keymap[kmap_index] = "control"; } ;

mouseconfig :
	/* empty */ | mouseconfig mousestmt ;

mousestmt :
	BAUDRATE number	{ gen.Baudrate = $2; } |
	SAMPLERATE number { gen.Samplerate = $2; } |
	EMULATE3 { gen.Emulate3 = 1; } |
	CHORDMIDDLE { gen.Chordmiddle = 1; } |
	CLEARDTR { gen.Cleardtr = 1; } |
	CLEARRTS { gen.Clearrts = 1; } ;

%%

#define NEW(t) (t *)malloc(sizeof(t))
#define REALLOC(n,t,d) (t *) realloc(d,n * sizeof(t))

/* Add a string to a list of strings */
string_list *add_string(sl,s)
string_list *sl;
char *s;
{
	
	if ( sl == NULL ) {
		sl = NEW(string_list);
		sl -> datap = NEW(char *);
		(sl -> datap)[0] = s;
		sl -> count = 1;
	} else {
		sl->datap = REALLOC((sl->count+1),char *,sl->datap);
		(sl->datap)[sl->count] = s;
		sl->count++;
	}
	return sl;
}

/* Add a list to a list of lists */
string_list_list *add_list(sl,s)
string_list_list *sl;
string_list *s;
{
	
	if ( sl == NULL ) {
		sl = NEW(string_list_list);
		sl -> datap = NEW(string_list *);
		(sl -> datap)[0] = s;
		sl -> count = 1;
	} else {
		sl->datap = REALLOC((sl->count+1),string_list *,sl->datap);
		(sl->datap)[sl->count] = s;
		sl->count++;
	}
	return sl;
}

/* Add a mode to a list of modes */
mode_list *add_mode(sl,mn,d,h1,h2,h3,h4,v1,v2,v3,v4,flags)
mode_list *sl;
char *mn;
char *d, *h1,*h2,*h3,*h4, *v1,*v2,*v3,*v4;
int flags;
{
	if ( sl == NULL ) {
		sl = NEW(mode_list);
		sl -> datap = NEW(struct mode);
		sl -> datap[0].mn = mn;
		sl -> datap[0].d =   d;
		sl -> datap[0].h1 = h1;
		sl -> datap[0].h2 = h2;
		sl -> datap[0].h3 = h3;
		sl -> datap[0].h4 = h4;
		sl -> datap[0].v1 = v1;
		sl -> datap[0].v2 = v2;
		sl -> datap[0].v3 = v3;
		sl -> datap[0].v4 = v4;
		sl -> datap[0].flags = flags;
		sl -> count = 1;
	} else {
		sl->datap = REALLOC((sl->count+1),struct mode,sl->datap);
		sl -> datap[sl->count].mn = mn;
		sl -> datap[sl->count].d =   d;
		sl -> datap[sl->count].h1 = h1;
		sl -> datap[sl->count].h2 = h2;
		sl -> datap[sl->count].h3 = h3;
		sl -> datap[sl->count].h4 = h4;
		sl -> datap[sl->count].v1 = v1;
		sl -> datap[sl->count].v2 = v2;
		sl -> datap[sl->count].v3 = v3;
		sl -> datap[sl->count].v4 = v4;
		sl -> datap[sl->count].flags = flags;
		sl->count++;
	}
	return sl;
}

main()
{
	if ( ! yyparse() ) {
		copyright();
		printf("# This file was generated by reconfig(1)\n");
		printf("# Refer to the XF86Config(4/5) man page for a ");
		printf("description of the format\n\n");
		dump();
		fprintf(stderr, "\n*** Note the XF86Config file generated ");
		fprintf(stderr, "must be edited before use.\n\n");
	}
	exit(0);
}

yyerror(s)
char *s;
{
	extern int line;
	extern char *yytext;

#if 0
	fprintf(stderr, "%s on line %d while facing '%s'\n",s,line,yytext);
#else
	fprintf(stderr, "%s on line %d\n",s,line);
#endif
}

dump()
{
	int i,j,k;

	/* Files section */
	printf("Section \"Files\"\n");
	if ( gen.Fontpath ) {
		for ( i = 0 ; i < gen.Fontpath->count ; i++ ) {
			printf("    FontPath %s\n",
				gen.Fontpath->datap[i]);
		}
	}
	if ( gen.Rgbpath ) {
		printf("    RGBPath %s\n",gen.Rgbpath);
	}
	printf("EndSection\n\n");

	/* serverflags section */
	printf("Section \"ServerFlags\"\n");
	if ( gen.Notrapsignals ) {
		printf("    NoTrapSignals\n");
	}
	if ( gen.Dontzap ) {
		printf("    DontZap\n");
	}
	printf("EndSection\n\n");

	/* Keyboard section */
	printf("Section \"Keyboard\"\n");
	if ( strcmp(gen.mouse_name,"xqueue") == 0 ) {
		printf("    Protocol \"Xqueue\"\n");
	} else {
		printf("    Protocol \"Standard\"\n");
	}
	if ( gen.Autorepeat1 ) {
		printf("    AutoRepeat %s %s\n",
			gen.Autorepeat1, gen.Autorepeat2);
	}
	if ( gen.Servernum ) {
		printf("    ServerNumLock\n");
	}
	if ( gen.Xleds ) {
		printf("    Xleds %s\n", gen.Xleds);
	}
	if ( gen.Vtinit ) {
		printf("    VTInit %s\n", gen.Vtinit);
	}
	for ( i = 0 ; i < N_KEYM ; i++ ) {
		if (gen.keymap[i]) {
			switch ( i ) {
			case iLEFTALT: printf("    LEFTALT "); break;
			case iRIGHTALT: printf("    RIGHTALT "); break;
			case iSCROLLLOCK: printf("    SCROLLLOCK "); break;
			case iRIGHTCTL: printf("    RIGHTCTL "); break;
			}
			printf("%s\n", gen.keymap[i]);
		}
	}
	if ( gen.Vtsysreq ) {
		printf("    VTSysReq\n");
	}
	printf("EndSection\n\n");

	printf("Section \"Pointer\"\n");
	if ( gen.mouse_name ) {
		printf("    Protocol \"%s\"\n",gen.mouse_name);
	}
	if ( gen.mouse_device ) {
		printf("    Device %s\n",gen.mouse_device);
	}
	if ( gen.Baudrate ) {
		printf("    BaudRate %s\n",gen.Baudrate);
	}
	if ( gen.Samplerate ) {
		printf("    SampleRate %s\n",gen.Samplerate);
	}
	if ( gen.Emulate3 ) {
		printf("    Emulate3Buttons\n");
	}
	if ( gen.Chordmiddle ) {
		printf("    ChordMiddle\n");
	}
	if ( gen.Cleardtr ) {
		printf("    ClearDTR\n");
	}
	if ( gen.Clearrts ) {
		printf("    ClearRTS\n");
	}
	printf("EndSection\n\n");
	
    /* Now print monitor, device and screen sections, for each screen */
    for ( j = 0 ; j < N_SCREENS ; j++ ) {
	scrn_index = j;
	if ( !SCRN.configured ) continue;

	printf("Section \"Monitor\"\n");
	printf("    Identifier \"RandomMonitor-%d\"\n",scrn_index);
	printf("    VendorName \"Unknown\"\n");
	printf("    ModelName \"Unknown\"\n");
	printf("    BandWidth 25.2\t# EDIT THIS!\n");
	printf("    HorizSync 31.5\t# EDIT THIS!\n");
	printf("    VertRefresh 60\t# EDIT THIS!\n");
	if ( modelist ) {
		for ( i = 0 ; i < modelist->count ; i++ ) {
			printf("    ModeLine");
			printf(" %s", modelist->datap[i].mn);
			printf(" %s", modelist->datap[i].d);
			printf(" %s", modelist->datap[i].h1);
			printf(" %s", modelist->datap[i].h2);
			printf(" %s", modelist->datap[i].h3);
			printf(" %s", modelist->datap[i].h4);
			printf(" %s", modelist->datap[i].v1);
			printf(" %s", modelist->datap[i].v2);
			printf(" %s", modelist->datap[i].v3);
			printf(" %s", modelist->datap[i].v4);
			if ( modelist->datap[i].flags & fINTERLACE )
				printf(" interlace");
			if ( modelist->datap[i].flags & fPHSYNC )
				printf(" +hsync");
			if ( modelist->datap[i].flags & fNHSYNC )
				printf(" -hsync");
			if ( modelist->datap[i].flags & fPVSYNC )
				printf(" +vsync");
			if ( modelist->datap[i].flags & fNVSYNC )
				printf(" -vsync");
			if ( modelist->datap[i].flags & fCSYNC )
				printf(" composite");
			printf("\n");
		}
	}
	printf("EndSection\n\n");
	
	printf("Section \"Device\"\n");
	printf("    Identifier \"RandomDevice-%d\"\n",scrn_index);
	printf("    VendorName \"Unknown\"\n");
	printf("    BoardName \"Unknown\"\n");
	if ( SCRN.Chipset ) {
		printf("    Chipset %s\n",SCRN.Chipset);
	}
	if ( SCRN.Ramdac ) {
		printf("    Ramdac %s\n",SCRN.Ramdac);
	}
	if ( SCRN.Dacspeed ) {
		printf("    Dacspeed %s\n",SCRN.Dacspeed);
	}
	if ( SCRN.Clockchip ) {
		printf("    Clockchip %s\n",SCRN.Clockchip);
	}
	if ( SCRN.Clocks ) {
		/* j is used to go over the screens. */
		for ( k = 0 ; k < SCRN.Clocks->count ; k++ ) {
			string_list *clcks = SCRN.Clocks->datap[k];
			
			if ( clcks ) {
				printf("    Clocks");
				for ( i = 0 ; i < clcks->count ; i++ ) {
					printf(" %s",clcks->datap[i]);
				}
				printf("\n");
			}
		}
	}
	if ( SCRN.Option ) {
		for ( i = 0 ; i < SCRN.Option->count ; i++ ) {
			printf("    Option %s\n",SCRN.Option->datap[i]);
		}
	}
	if ( SCRN.Videoram ) {
		printf("    Videoram %s\n",SCRN.Videoram);
	}
	if ( SCRN.Speedup ) {
		printf("    Speedup %s\n",SCRN.Speedup);
	}
	if ( SCRN.Clockprog1 ) {
		printf("    Clockprog %s",SCRN.Clockprog1);
		if ( SCRN.Clockprog2 ) {
			printf(" %s",SCRN.Clockprog2);
		}
		printf("\n");
	}
	if ( SCRN.Biosbase ) {
		printf("    Biosbase %s\n",SCRN.Biosbase);
	}
	if ( SCRN.Membase ) {
		printf("    Membase %s\n",SCRN.Membase);
	}
	if ( SCRN.Iobase ) {
		printf("    Iobase %s\n",SCRN.Iobase);
	}
	if ( SCRN.Dacbase ) {
		printf("    Dacbase %s\n",SCRN.Dacbase);
	}
	if ( SCRN.Copbase ) {
		printf("    Copbase %s\n",SCRN.Copbase);
	}
	if ( SCRN.Posbase ) {
		printf("    Speedup %s\n",SCRN.Posbase);
	}
	if ( SCRN.Instance ) {
		printf("    Instance %s\n",SCRN.Instance);
	}
	printf("EndSection\n\n");

	/* Screen section */
	printf("Section \"Screen\"\n");
	switch ( scrn_index ) {
	case iVGA256:	printf("    Driver \"vga256\"\n"); break;
	case iVGA2:	printf("    Driver \"vga2\"\n"); break;
	case iMONO:	printf("    Driver \"mono\"\n"); break;
	case iVGA16:	printf("    Driver \"vga16\"\n"); break;
	case iACCEL:	printf("    Driver \"accel\"\n"); break;
	}
	printf("    Device \"RandomDevice-%d\"\n",scrn_index);
	printf("    Monitor \"RandomMonitor-%d\"\n",scrn_index);
	if ( SCRN.Screenno ) {
		printf("    Screenno %s\n",SCRN.Screenno);
	}
	printf("    Subsection \"Display\"\n");
	if ( SCRN.Staticgray ) {
	    printf("        StaticGray\n");
	}
	if ( SCRN.Grayscale ) {
	    printf("        GrayScale\n");
	}
	if ( SCRN.Staticcolor ) {
	    printf("        StaticColor\n");
	}
	if ( SCRN.Pseudocolor ) {
	    printf("        PseudoColor\n");
	}
	if ( SCRN.Truecolor ) {
	    printf("        TrueColor\n");
	}
	if ( SCRN.Directcolor ) {
	    printf("        DirectColor\n");
	}
	if ( SCRN.Displaysize1 ) {
	    printf("        DisplaySize %s %s\n",
			SCRN.Displaysize1,SCRN.Displaysize2);
	}
	if ( SCRN.Modes ) {
		printf("        Modes");
		for ( i = 0 ; i < SCRN.Modes->count ; i++ ) {
			printf(" %s",SCRN.Modes->datap[i]);
		}
		printf("\n");
	}
	if ( SCRN.Viewport1 ) {
	    printf("        ViewPort %s %s\n",
			SCRN.Viewport1,SCRN.Viewport2);
	}
	if ( SCRN.Virtual1 ) {
	    printf("        Virtual %s %s\n",
			SCRN.Virtual1,SCRN.Virtual2);
	}
	if ( SCRN.Black1 ) {
	    printf("        Black %s %s %s\n",
			SCRN.Black1,SCRN.Black2,SCRN.Black3);
	}
	if ( SCRN.White1 ) {
	    printf("        White %s %s %s\n",
			SCRN.White1,SCRN.White2,SCRN.White3);
	}
	printf("    EndSubsection\n");
	printf("EndSection\n\n");
    } /* screens */
} /* function */
