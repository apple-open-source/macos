/* $Xorg: sunKeyMap.c,v 1.3 2000/08/17 19:48:30 cpqbld Exp $ */
/************************************************************
Copyright 1987 by Sun Microsystems, Inc. Mountain View, CA.

                    All Rights Reserved

Permission  to  use,  copy,  modify,  and  distribute   this
software  and  its documentation for any purpose and without
fee is hereby granted, provided that the above copyright no-
tice  appear  in all copies and that both that copyright no-
tice and this permission notice appear in  supporting  docu-
mentation,  and  that the names of Sun or The Open Group
not be used in advertising or publicity pertaining to 
distribution  of  the software  without specific prior 
written permission. Sun and The Open Group make no 
representations about the suitability of this software for 
any purpose. It is provided "as is" without any express or 
implied warranty.

SUN DISCLAIMS ALL WARRANTIES WITH REGARD TO  THIS  SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FIT-
NESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL SUN BE  LI-
ABLE  FOR  ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,  DATA  OR
PROFITS,  WHETHER  IN  AN  ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION  WITH
THE USE OR PERFORMANCE OF THIS SOFTWARE.

********************************************************/
/* $XFree86: xc/programs/Xserver/hw/sun/sunKeyMap.c,v 1.4 2001/10/28 03:33:12 tsi Exp $ */

#include	"sun.h"
#define		XK_KATAKANA
#include	"keysym.h"
#include	"Sunkeysym.h"

/* 
  By default all keyboards are hardcoded on the theory that people
  might remove /usr/openwin making it impossible to parse the files
 in /usr/openwin/share/etc/keytables.
*/
#define CAN4
#define CANFR5
#define DEN4
#define DEN5
#define FR5
#define FRBE4
#define GER4
#define GER5
#define ITALY4
#define ITALY5
#define JAPAN4
#define JAPAN5
#define KOREA4
#define KOREA5
#define NETH4
#define NETH5
#define NORW4
#define NORW5
#define PORT4
#define PORT5
#define SPAIN5
#define SPAINLATAM4 
#define SWED5
#define SWEDFIN4
#define SWFR4
#define SWFR5
#define SWGE4
#define SWGE5
#define TAI4
#define TAI5
#define UK4
#define UK5
#define US101A
#define US2
#define US3
#define US4
#define US5
#define US_UNIX5

/*
 *	XXX - Its not clear what to map these to for now.
 *	keysyms.h doesn't define enough function key names.
 */

#ifndef	XK_L1
#define	XK_L1	XK_Cancel
#define	XK_L2	XK_Redo
#define	XK_R1	NoSymbol
#define	XK_R2	NoSymbol
#define	XK_R3	NoSymbol
#define	XK_L3	XK_Menu
#define	XK_L4	XK_Undo
#define	XK_R4	NoSymbol
#define	XK_R5	NoSymbol
#define	XK_R6	NoSymbol
#define	XK_L5	XK_Insert
#define	XK_L6	XK_Select
#define	XK_R7	NoSymbol
#define	XK_R8	XK_Up
#define	XK_R9	NoSymbol
#define	XK_L7	XK_Execute
#define	XK_L8	XK_Print
#define	XK_R10	XK_Left
#define	XK_R11	XK_Home
#define	XK_R12	XK_Right
#define	XK_L9	XK_Find
#define	XK_L10	XK_Help
#define	XK_R13	NoSymbol
#define	XK_R14	XK_Down
#define	XK_R15	NoSymbol
#endif

/* twm and Motif have hard-coded dependencies on Meta being Mod1 :-( */
#if 0
/* This set has optimal characteristics for use in the Toolkit... */
#define Meta_Mask Mod1Mask
#define Mode_switch_Mask Mod2Mask
#define Num_Lock_Mask Mod3Mask
#define Alt_Mask Mod4Mask
#else
/* but this set is compatible with what we shipped in R6. */
#define Meta_Mask Mod1Mask
#define Mode_switch_Mask Mod2Mask
#define Alt_Mask Mod3Mask
#define Num_Lock_Mask Mod4Mask
#endif

#ifdef US2

static KeySym US2Keymap[] = {
	XK_L1,		NoSymbol,		/* 0x1 */
	NoSymbol,	NoSymbol,		/* 0x2 */
	XK_L2,		NoSymbol,		/* 0x3 */
	NoSymbol,	NoSymbol,		/* 0x4 */
	XK_F1,		NoSymbol,		/* 0x5 */
	XK_F2,		NoSymbol,		/* 0x6 */
	NoSymbol,	NoSymbol,		/* 0x7 */
	XK_F3,		NoSymbol,		/* 0x8 */
	NoSymbol,	NoSymbol,		/* 0x9 */
	XK_F4,		NoSymbol,		/* 0xa */
	NoSymbol,	NoSymbol,		/* 0xb */
	XK_F5,		NoSymbol,		/* 0xc */
	NoSymbol,	NoSymbol,		/* 0xd */
	XK_F6,		NoSymbol,		/* 0xe */
	NoSymbol,	NoSymbol,		/* 0xf */
	XK_F7,		NoSymbol,		/* 0x10 */
	XK_F8,		NoSymbol,		/* 0x11 */
	XK_F9,		NoSymbol,		/* 0x12 */
	XK_Break,	NoSymbol,		/* 0x13 */
	NoSymbol,	NoSymbol,		/* 0x14 */
	XK_R1,		NoSymbol,		/* 0x15 */
	XK_R2,		NoSymbol,		/* 0x16 */
	XK_R3,		NoSymbol,		/* 0x17 */
	NoSymbol,	NoSymbol,		/* 0x18 */
	XK_L3,		NoSymbol,		/* 0x19 */
	XK_L4,		NoSymbol,		/* 0x1a */
	NoSymbol,	NoSymbol,		/* 0x1b */
	NoSymbol,	NoSymbol,		/* 0x1c */
	XK_Escape,	NoSymbol,		/* 0x1d */
	XK_1,		XK_exclam,		/* 0x1e */
	XK_2,		XK_at,			/* 0x1f */
	XK_3,		XK_numbersign,		/* 0x20 */
	XK_4,		XK_dollar,		/* 0x21 */
	XK_5,		XK_percent,		/* 0x22 */
	XK_6,		XK_asciicircum,		/* 0x23 */
	XK_7,		XK_ampersand,		/* 0x24 */
	XK_8,		XK_asterisk,		/* 0x25 */
	XK_9,		XK_parenleft,		/* 0x26 */
	XK_0,		XK_parenright,		/* 0x27 */
	XK_minus,	XK_underscore,		/* 0x28 */
	XK_equal,	XK_plus,		/* 0x29 */
	XK_grave,	XK_asciitilde,		/* 0x2a */
	XK_BackSpace,	NoSymbol,		/* 0x2b */
	NoSymbol,	NoSymbol,		/* 0x2c */
	XK_R4,		NoSymbol,		/* 0x2d */
	XK_R5,		NoSymbol,		/* 0x2e */
	XK_R6,		NoSymbol,		/* 0x2f */
	NoSymbol,	NoSymbol,		/* 0x30 */
	XK_L5,		NoSymbol,		/* 0x31 */
	NoSymbol,	NoSymbol,		/* 0x32 */
	XK_L6,		NoSymbol,		/* 0x33 */
	NoSymbol,	NoSymbol,		/* 0x34 */
	XK_Tab,		NoSymbol,		/* 0x35 */
	XK_Q,		NoSymbol,		/* 0x36 */
	XK_W,		NoSymbol,		/* 0x37 */
	XK_E,		NoSymbol,		/* 0x38 */
	XK_R,		NoSymbol,		/* 0x39 */
	XK_T,		NoSymbol,		/* 0x3a */
	XK_Y,		NoSymbol,		/* 0x3b */
	XK_U,		NoSymbol,		/* 0x3c */
	XK_I,		NoSymbol,		/* 0x3d */
	XK_O,		NoSymbol,		/* 0x3e */
	XK_P,		NoSymbol,		/* 0x3f */
	XK_bracketleft,	XK_braceleft,		/* 0x40 */
	XK_bracketright,	XK_braceright,	/* 0x41 */
	XK_Delete,	NoSymbol,		/* 0x42 */
	NoSymbol,	NoSymbol,		/* 0x43 */
	XK_R7,		NoSymbol,		/* 0x44 */
	XK_Up,		XK_R8,			/* 0x45 */
	XK_R9,		NoSymbol,		/* 0x46 */
	NoSymbol,	NoSymbol,		/* 0x47 */
	XK_L7,		NoSymbol,		/* 0x48 */
	XK_L8,		NoSymbol,		/* 0x49 */
	NoSymbol,	NoSymbol,		/* 0x4a */
	NoSymbol,	NoSymbol,		/* 0x4b */
	XK_Control_L,	NoSymbol,		/* 0x4c */
	XK_A,		NoSymbol,		/* 0x4d */
	XK_S,		NoSymbol,		/* 0x4e */
	XK_D,		NoSymbol,		/* 0x4f */
	XK_F,		NoSymbol,		/* 0x50 */
	XK_G,		NoSymbol,		/* 0x51 */
	XK_H,		NoSymbol,		/* 0x52 */
	XK_J,		NoSymbol,		/* 0x53 */
	XK_K,		NoSymbol,		/* 0x54 */
	XK_L,		NoSymbol,		/* 0x55 */
	XK_semicolon,	XK_colon,		/* 0x56 */
	XK_apostrophe,	XK_quotedbl,		/* 0x57 */
	XK_backslash,	XK_bar,			/* 0x58 */
	XK_Return,	NoSymbol,		/* 0x59 */
	NoSymbol,	NoSymbol,		/* 0x5a */
	XK_Left,	XK_R10,			/* 0x5b */
	XK_R11,		NoSymbol,		/* 0x5c */
	XK_Right,	XK_R12,			/* 0x5d */
	NoSymbol,	NoSymbol,		/* 0x5e */
	XK_L9,		NoSymbol,		/* 0x5f */
	NoSymbol,	NoSymbol,		/* 0x60 */
	XK_L10,		NoSymbol,		/* 0x61 */
	NoSymbol,	NoSymbol,		/* 0x62 */
	XK_Shift_L,	NoSymbol,		/* 0x63 */
	XK_Z,		NoSymbol,		/* 0x64 */
	XK_X,		NoSymbol,		/* 0x65 */
	XK_C,		NoSymbol,		/* 0x66 */
	XK_V,		NoSymbol,		/* 0x67 */
	XK_B,		NoSymbol,		/* 0x68 */
	XK_N,		NoSymbol,		/* 0x69 */
	XK_M,		NoSymbol,		/* 0x6a */
	XK_comma,	XK_less,		/* 0x6b */
	XK_period,	XK_greater,		/* 0x6c */
	XK_slash,	XK_question,		/* 0x6d */
	XK_Shift_R,	NoSymbol,		/* 0x6e */
	XK_Linefeed,	NoSymbol,		/* 0x6f */
	XK_R13,		NoSymbol,		/* 0x70 */
	XK_Down,	XK_R14,			/* 0x71 */
	XK_R15,		NoSymbol,		/* 0x72 */
	NoSymbol,	NoSymbol,		/* 0x73 */
	NoSymbol,	NoSymbol,		/* 0x74 */
	NoSymbol,	NoSymbol,		/* 0x75 */
	NoSymbol,	NoSymbol,		/* 0x76 */
	NoSymbol,	NoSymbol,		/* 0x77 */
	XK_Meta_L,	NoSymbol,		/* 0x78 */
	XK_space,	NoSymbol,		/* 0x79 */
	XK_Meta_R,	NoSymbol,		/* 0x7a */
	NoSymbol,	NoSymbol,		/* 0x7b */
	NoSymbol,	NoSymbol,		/* 0x7c */
	NoSymbol,	NoSymbol,		/* 0x7d */
	NoSymbol,	NoSymbol,		/* 0x7e */
	NoSymbol,	NoSymbol,		/* 0x7f */
};

static SunModmapRec US2Modmap[] = {
	{  99,	ShiftMask },
	{ 110,	ShiftMask },
	{  76,	ControlMask },
	{ 120,	Meta_Mask },
	{ 122,	Meta_Mask },
	{   0,	0 }
};

#else

#define US2Keymap NULL
#define US2Modmap NULL

#endif /* US2 */

#ifdef US3

static KeySym US3Keymap[] = {
	XK_L1,		NoSymbol,		/* 0x1 */
	NoSymbol,	NoSymbol,		/* 0x2 */
	XK_L2,		NoSymbol,		/* 0x3 */
	NoSymbol,	NoSymbol,		/* 0x4 */
	XK_F1,		NoSymbol,		/* 0x5 */
	XK_F2,		NoSymbol,		/* 0x6 */
	NoSymbol,	NoSymbol,		/* 0x7 */
	XK_F3,		NoSymbol,		/* 0x8 */
	NoSymbol,	NoSymbol,		/* 0x9 */
	XK_F4,		NoSymbol,		/* 0xa */
	NoSymbol,	NoSymbol,		/* 0xb */
	XK_F5,		NoSymbol,		/* 0xc */
	NoSymbol,	NoSymbol,		/* 0xd */
	XK_F6,		NoSymbol,		/* 0xe */
	NoSymbol,	NoSymbol,		/* 0xf */
	XK_F7,		NoSymbol,		/* 0x10 */
	XK_F8,		NoSymbol,		/* 0x11 */
	XK_F9,		NoSymbol,		/* 0x12 */
	XK_Alt_R,	NoSymbol,		/* 0x13 */
	NoSymbol,	NoSymbol,		/* 0x14 */
	XK_R1,		NoSymbol,		/* 0x15 */
	XK_R2,		NoSymbol,		/* 0x16 */
	XK_R3,		NoSymbol,		/* 0x17 */
	NoSymbol,	NoSymbol,		/* 0x18 */
	XK_L3,		NoSymbol,		/* 0x19 */
	XK_L4,		NoSymbol,		/* 0x1a */
	NoSymbol,	NoSymbol,		/* 0x1b */
	NoSymbol,	NoSymbol,		/* 0x1c */
	XK_Escape,	NoSymbol,		/* 0x1d */
	XK_1,		XK_exclam,		/* 0x1e */
	XK_2,		XK_at,			/* 0x1f */
	XK_3,		XK_numbersign,		/* 0x20 */
	XK_4,		XK_dollar,		/* 0x21 */
	XK_5,		XK_percent,		/* 0x22 */
	XK_6,		XK_asciicircum,		/* 0x23 */
	XK_7,		XK_ampersand,		/* 0x24 */
	XK_8,		XK_asterisk,		/* 0x25 */
	XK_9,		XK_parenleft,		/* 0x26 */
	XK_0,		XK_parenright,		/* 0x27 */
	XK_minus,	XK_underscore,		/* 0x28 */
	XK_equal,	XK_plus,		/* 0x29 */
	XK_grave,	XK_asciitilde,		/* 0x2a */
	XK_BackSpace,	NoSymbol,		/* 0x2b */
	NoSymbol,	NoSymbol,		/* 0x2c */
	XK_R4,		NoSymbol,		/* 0x2d */
	XK_R5,		NoSymbol,		/* 0x2e */
	XK_R6,		NoSymbol,		/* 0x2f */
	NoSymbol,	NoSymbol,		/* 0x30 */
	XK_L5,		NoSymbol,		/* 0x31 */
	NoSymbol,	NoSymbol,		/* 0x32 */
	XK_L6,		NoSymbol,		/* 0x33 */
	NoSymbol,	NoSymbol,		/* 0x34 */
	XK_Tab,		NoSymbol,		/* 0x35 */
	XK_Q,		NoSymbol,		/* 0x36 */
	XK_W,		NoSymbol,		/* 0x37 */
	XK_E,		NoSymbol,		/* 0x38 */
	XK_R,		NoSymbol,		/* 0x39 */
	XK_T,		NoSymbol,		/* 0x3a */
	XK_Y,		NoSymbol,		/* 0x3b */
	XK_U,		NoSymbol,		/* 0x3c */
	XK_I,		NoSymbol,		/* 0x3d */
	XK_O,		NoSymbol,		/* 0x3e */
	XK_P,		NoSymbol,		/* 0x3f */
	XK_bracketleft,	XK_braceleft,		/* 0x40 */
	XK_bracketright,	XK_braceright,	/* 0x41 */
	XK_Delete,	NoSymbol,		/* 0x42 */
	NoSymbol,	NoSymbol,		/* 0x43 */
	XK_R7,		NoSymbol,		/* 0x44 */
	XK_Up,		XK_R8,			/* 0x45 */
	XK_R9,		NoSymbol,		/* 0x46 */
	NoSymbol,	NoSymbol,		/* 0x47 */
	XK_L7,		NoSymbol,		/* 0x48 */
	XK_L8,		NoSymbol,		/* 0x49 */
	NoSymbol,	NoSymbol,		/* 0x4a */
	NoSymbol,	NoSymbol,		/* 0x4b */
	XK_Control_L,	NoSymbol,		/* 0x4c */
	XK_A,		NoSymbol,		/* 0x4d */
	XK_S,		NoSymbol,		/* 0x4e */
	XK_D,		NoSymbol,		/* 0x4f */
	XK_F,		NoSymbol,		/* 0x50 */
	XK_G,		NoSymbol,		/* 0x51 */
	XK_H,		NoSymbol,		/* 0x52 */
	XK_J,		NoSymbol,		/* 0x53 */
	XK_K,		NoSymbol,		/* 0x54 */
	XK_L,		NoSymbol,		/* 0x55 */
	XK_semicolon,	XK_colon,		/* 0x56 */
	XK_apostrophe,	XK_quotedbl,		/* 0x57 */
	XK_backslash,	XK_bar,			/* 0x58 */
	XK_Return,	NoSymbol,		/* 0x59 */
	NoSymbol,	NoSymbol,		/* 0x5a */
	XK_Left,	XK_R10,			/* 0x5b */
	XK_R11,		NoSymbol,		/* 0x5c */
	XK_Right,	XK_R12,			/* 0x5d */
	NoSymbol,	NoSymbol,		/* 0x5e */
	XK_L9,		NoSymbol,		/* 0x5f */
	NoSymbol,	NoSymbol,		/* 0x60 */
	XK_L10,		NoSymbol,		/* 0x61 */
	NoSymbol,	NoSymbol,		/* 0x62 */
	XK_Shift_L,	NoSymbol,		/* 0x63 */
	XK_Z,		NoSymbol,		/* 0x64 */
	XK_X,		NoSymbol,		/* 0x65 */
	XK_C,		NoSymbol,		/* 0x66 */
	XK_V,		NoSymbol,		/* 0x67 */
	XK_B,		NoSymbol,		/* 0x68 */
	XK_N,		NoSymbol,		/* 0x69 */
	XK_M,		NoSymbol,		/* 0x6a */
	XK_comma,	XK_less,		/* 0x6b */
	XK_period,	XK_greater,		/* 0x6c */
	XK_slash,	XK_question,		/* 0x6d */
	XK_Shift_R,	NoSymbol,		/* 0x6e */
	XK_Linefeed,	NoSymbol,		/* 0x6f */
	XK_R13,		NoSymbol,		/* 0x70 */
	XK_Down,	XK_R14,			/* 0x71 */
	XK_R15,		NoSymbol,		/* 0x72 */
	NoSymbol,	NoSymbol,		/* 0x73 */
	NoSymbol,	NoSymbol,		/* 0x74 */
	NoSymbol,	NoSymbol,		/* 0x75 */
	NoSymbol,	NoSymbol,		/* 0x76 */
	XK_Caps_Lock,	NoSymbol,		/* 0x77 */
	XK_Meta_L,	NoSymbol,		/* 0x78 */
	XK_space,	NoSymbol,		/* 0x79 */
	XK_Meta_R,	NoSymbol,		/* 0x7a */
	NoSymbol,	NoSymbol,		/* 0x7b */
	NoSymbol,	NoSymbol,		/* 0x7c */
	NoSymbol,	NoSymbol,		/* 0x7d */
	NoSymbol,	NoSymbol,		/* 0x7e */
	NoSymbol,	NoSymbol,		/* 0x7f */
};

static SunModmapRec US3Modmap[] = {
	{  99,	ShiftMask },
	{ 110,	ShiftMask },
	{  76,	ControlMask },
	{ 119,	LockMask },
	{ 120,	Meta_Mask },
	{ 122,	Meta_Mask },
	{   0,	0 }
};

#else

#define US3Keymap NULL
#define US3Modmap NULL

#endif /* US3 */

KeySymsRec sunKeySyms[] = {
    /*	map	    minKeyCode	maxKC	width */
    { (KeySym *)NULL,	0,	0,	0 },
    { (KeySym *)NULL,	0,	0,	0 },
    { US2Keymap,	1,	0x7a,	2 },
    { US3Keymap,	1,	0x7a,	2 },
    { (KeySym *)NULL,	1,	0x7d,	4 }
};

SunModmapRec *sunModMaps[] = {
    NULL,
    NULL,
    US2Modmap,
    US3Modmap,
    NULL
};

static SunModmapRec Generic5Modmap[] = {
	{  99,	ShiftMask },
	{ 110,	ShiftMask },
	{ 119,	LockMask },
	{  76,	ControlMask },
	{ 120,	Meta_Mask },
	{ 122,	Meta_Mask },
	{  13,	Mode_switch_Mask },
	{  98,	Num_Lock_Mask },
	{  19,	Alt_Mask },
	{   0,	0}
};

#if defined(DEN4) || defined(SWEDFIN4) || defined(SWFR4) || defined(SWGE4)

static SunModmapRec DenSwedFinSw4Modmap[] = {
        {  99,	ShiftMask },
        { 110,	ShiftMask },
        {  76,	LockMask },
        { 119,	ControlMask },
        { 120,	Meta_Mask },
        { 122,	Meta_Mask },
        {  67,	Mode_switch_Mask },
        {  98,	Num_Lock_Mask },
        {  19,	Alt_Mask },
        {   0,	0 }
};

#endif

#if defined(FRBE4) || defined(NETH4)

static SunModmapRec FrBeNeth4Modmap[] = {
	{  99,	ShiftMask },
	{ 110,	ShiftMask },
	{  13,	LockMask },
	{  76,	ControlMask },
	{ 120,	Meta_Mask },
	{ 122,	Meta_Mask },
	{ 119,	Mode_switch_Mask },
	{  98,	Num_Lock_Mask },
	{  19,	Alt_Mask },
	{   0,	0 }
};

#endif

#if defined(ITALY4) || defined(NORW4) || defined(PORT4) || defined(SPAINLATAM4)

static SunModmapRec ItNorPortSp4Modmap[] = {
	{  99,	ShiftMask },
	{ 110,	ShiftMask },
	{  76,	LockMask },
	{ 119,	ControlMask },
	{ 120,	Meta_Mask },
	{ 122,	Meta_Mask },
	{  13,	Mode_switch_Mask },
	{  98,	Num_Lock_Mask },
	{  19,	Alt_Mask },
	{   0,	0 }
};

#endif

#ifdef CAN4

static KeySym Canada4Keymap[] = {
	SunXK_Stop,  	NoSymbol,  	XK_L1,		NoSymbol,	/*  1*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  2*/
	SunXK_Again,  	NoSymbol,  	XK_L2,		NoSymbol,	/*  3*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  4*/
	XK_Escape,	NoSymbol,	NoSymbol,	NoSymbol,	/*  5*/
	XK_F1,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  6*/
	XK_F9,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  7*/
	XK_F2,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  8*/
	XK_F10, 	NoSymbol,	NoSymbol,	NoSymbol,	/*  9*/
	XK_F3,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 10*/
	XK_F11,		NoSymbol,	SunXK_F36,	NoSymbol,	/* 11*/
	XK_F4,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 12*/
	XK_Control_R,	NoSymbol,	NoSymbol,	NoSymbol,	/* 13*/
	XK_F5,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 14*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 15*/
	XK_F6,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 16*/
	XK_F7,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 17*/
	XK_F8,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 18*/
	XK_Alt_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 19*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 20*/
	XK_Pause,  	NoSymbol,  	XK_R1,		NoSymbol,	/* 21*/
	XK_Print,  	NoSymbol,  	XK_R2,		NoSymbol,	/* 22*/
	XK_Break,  	XK_Scroll_Lock,	XK_R3,		NoSymbol,	/* 23*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 24*/
	SunXK_Props,  	NoSymbol,  	XK_L3,		NoSymbol,	/* 25*/
	SunXK_Undo,  	NoSymbol,  	XK_L4,		NoSymbol,	/* 26*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 27*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 28*/
	XK_degree,	NoSymbol,	XK_notsign,	NoSymbol,	/* 29*/
	XK_1,   	XK_exclam,	NoSymbol,	NoSymbol,	/* 30*/
	XK_2,   	XK_quotedbl,	XK_at,  	NoSymbol,	/* 31*/
	XK_3,   	XK_numbersign,	NoSymbol,	NoSymbol,	/* 32*/
	XK_4,   	XK_dollar,	NoSymbol,	NoSymbol,	/* 33*/
	XK_5,   	XK_percent,	NoSymbol,	NoSymbol,	/* 34*/
	XK_6,   	XK_question,	NoSymbol,	NoSymbol,	/* 35*/
	XK_7,   	XK_ampersand,	XK_braceleft,	NoSymbol,	/* 36*/
	XK_8,   	XK_asterisk,	XK_bracketleft,	NoSymbol,	/* 37*/
	XK_9,   	XK_parenleft,	XK_bracketright,NoSymbol,	/* 38*/
	XK_0,   	XK_parenright,	XK_braceright,	NoSymbol,	/* 39*/
	XK_minus,	XK_underscore,	XK_bar, 	NoSymbol,	/* 40*/
	XK_equal,	XK_plus,	NoSymbol,	NoSymbol,	/* 41*/
	XK_Agrave,	NoSymbol,	XK_grave,	NoSymbol,	/* 42*/
	XK_BackSpace,	NoSymbol,	NoSymbol,	NoSymbol,	/* 43*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 44*/
	XK_KP_Equal,  	XK_KP_Equal,  	XK_R4,		NoSymbol,	/* 45*/
	XK_KP_Divide,  	XK_KP_Divide,  	XK_R5,		NoSymbol,	/* 46*/
	XK_KP_Multiply,	XK_KP_Multiply,	XK_R6,		NoSymbol,	/* 47*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 48*/
	SunXK_Front,  	NoSymbol,  	XK_L5,		NoSymbol,	/* 49*/
	XK_KP_Delete,	XK_KP_Decimal,	NoSymbol,	NoSymbol,	/* 50*/
	SunXK_Copy,  	NoSymbol,  	XK_L6,		NoSymbol,	/* 51*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 52*/
	XK_Tab, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 53*/
	XK_Q,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 54*/
	XK_W,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 55*/
	XK_E,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 56*/
	XK_R,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 57*/
	XK_T,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 58*/
	XK_Y,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 59*/
	XK_U,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 60*/
	XK_I,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 61*/
	XK_O,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 62*/
	XK_P,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 63*/
	SunXK_FA_Circum,SunXK_FA_Diaeresis,XK_asciicircum,NoSymbol,	/* 64*/
	XK_Ccedilla,	NoSymbol,	XK_asciitilde,	NoSymbol,	/* 65*/
	XK_Delete,	NoSymbol,	NoSymbol,	NoSymbol,	/* 66*/
	SunXK_Compose,	NoSymbol,	NoSymbol,	NoSymbol,	/* 67*/
	XK_KP_Home,  	XK_KP_7,  	XK_R7,		NoSymbol,	/* 68*/
	XK_KP_Up,  	XK_KP_8,  	XK_R8,		NoSymbol,	/* 69*/
	XK_KP_Prior,  	XK_KP_9,  	XK_R9,		XK_Prior,	/* 70*/
	XK_KP_Subtract,	NoSymbol,	NoSymbol,	NoSymbol,	/* 71*/
	SunXK_Open,  	NoSymbol,  	XK_L7,		NoSymbol,	/* 72*/
	SunXK_Paste,  	NoSymbol,  	XK_L8,		NoSymbol,	/* 73*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 74*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 75*/
	XK_Caps_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 76*/
	XK_A,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 77*/
	XK_S,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 78*/
	XK_D,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 79*/
	XK_F,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 80*/
	XK_G,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 81*/
	XK_H,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 82*/
	XK_J,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 83*/
	XK_K,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 84*/
	XK_L,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 85*/
	XK_semicolon,	XK_colon,	NoSymbol,	NoSymbol,	/* 86*/
	XK_Egrave,	NoSymbol,	NoSymbol,	NoSymbol,	/* 87*/
	XK_F12,		NoSymbol,	SunXK_F37,	NoSymbol,	/* 88*/
	XK_Return,	NoSymbol,	NoSymbol,	NoSymbol,	/* 89*/
	XK_KP_Enter,	NoSymbol,	NoSymbol,	NoSymbol,	/* 90*/
	XK_KP_Left,	XK_KP_4, 	XK_R10,		NoSymbol,	/* 91*/
	NoSymbol, 	XK_KP_5, 	XK_R11,		NoSymbol,	/* 92*/
	XK_KP_Right,	XK_KP_6, 	XK_R12,		NoSymbol,	/* 93*/
	XK_KP_Insert,	XK_KP_0,	NoSymbol,	NoSymbol,	/* 94*/
	SunXK_Find,  	NoSymbol,  	XK_L9,		NoSymbol,	/* 95*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 96*/
	SunXK_Cut, 	NoSymbol, 	XK_L10,		NoSymbol,	/* 97*/
	XK_Num_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 98*/
	XK_Shift_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 99*/
	XK_Z,   	NoSymbol,	XK_guillemotleft,NoSymbol,	/*100*/
	XK_X,   	NoSymbol,	XK_guillemotright,NoSymbol,	/*101*/
	XK_C,   	NoSymbol,	NoSymbol,	NoSymbol,	/*102*/
	XK_V,   	NoSymbol,	NoSymbol,	NoSymbol,	/*103*/
	XK_B,   	NoSymbol,	NoSymbol,	NoSymbol,	/*104*/
	XK_N,   	NoSymbol,	NoSymbol,	NoSymbol,	/*105*/
	XK_M,   	NoSymbol,	NoSymbol,	NoSymbol,	/*106*/
	XK_comma,	XK_apostrophe,	XK_less,	NoSymbol,	/*107*/
	XK_period,	NoSymbol,	XK_greater,	NoSymbol,	/*108*/
	XK_Eacute,	NoSymbol,	XK_slash,	NoSymbol,	/*109*/
	XK_Shift_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*110*/
	XK_Linefeed,	NoSymbol,	NoSymbol,	NoSymbol,	/*111*/
	XK_KP_End, 	XK_KP_1, 	XK_R13,		NoSymbol, 	/*112*/
	XK_KP_Down,	XK_KP_2, 	XK_R14,		NoSymbol,	/*113*/
	XK_KP_Next, 	XK_KP_3, 	XK_R15,		NoSymbol,	/*114*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*115*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*116*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*117*/
	XK_Help,	NoSymbol,	NoSymbol,	NoSymbol,	/*118*/
	SunXK_AltGraph,	NoSymbol,	NoSymbol,	NoSymbol,	/*119*/
	XK_Meta_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*120*/
	XK_space,	NoSymbol,	NoSymbol,	NoSymbol,	/*121*/
	XK_Meta_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*122*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*123*/
	XK_Ugrave,	NoSymbol,	XK_backslash,	NoSymbol,	/*124*/
	XK_KP_Add,	NoSymbol,	NoSymbol,	NoSymbol,	/*125*/
};

static SunModmapRec Canada4Modmap[] = {
	{  99,	ShiftMask },
	{ 110,	ShiftMask },
	{  76,	LockMask },
	{  13,	ControlMask },
	{ 120,	Meta_Mask },
	{ 122,	Meta_Mask },
	{ 119,	Mode_switch_Mask },
	{  98,	Num_Lock_Mask },
	{  19,	Alt_Mask },
	{   0,	0}
};

#else

#define Canada4Keymap NULL
#define Canada4Modmap NULL

#endif /* CANADA4 */

#ifdef CANFR5

static KeySym CanadaFr5Keymap[] = {
	SunXK_Stop,  	NoSymbol,  	XK_L1,		NoSymbol,	/*  1*/
	SunXK_AudioLowerVolume,SunXK_VideoLowerBrightness,NoSymbol,	NoSymbol,	/*  2*/
	SunXK_Again,  	NoSymbol,  	XK_L2,		NoSymbol,	/*  3*/
	SunXK_AudioRaiseVolume,SunXK_VideoRaiseBrightness,NoSymbol,	NoSymbol,	/*  4*/
	XK_F1,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  5*/
	XK_F2,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  6*/
	XK_F10, 	NoSymbol,	NoSymbol,	NoSymbol,	/*  7*/
	XK_F3,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  8*/
	XK_F11,		NoSymbol,	SunXK_F36,	NoSymbol,	/*  9*/
	XK_F4,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 10*/
	XK_F12,		NoSymbol,	SunXK_F37,	NoSymbol,	/* 11*/
	XK_F5,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 12*/
	SunXK_AltGraph,	NoSymbol,	NoSymbol,	NoSymbol,	/* 13*/
	XK_F6,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 14*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 15*/
	XK_F7,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 16*/
	XK_F8,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 17*/
	XK_F9,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 18*/
	XK_Alt_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 19*/
	XK_Up,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 20*/
	XK_Pause,	XK_Break,	XK_R1,		XK_R1,		/* 21*/
	XK_Print,	NoSymbol,	XK_Sys_Req,	SunXK_Sys_Req,	/* 22*/
	XK_Scroll_Lock,	NoSymbol,	XK_R3,		XK_R3,		/* 23*/
	XK_Left,	NoSymbol,	NoSymbol,	NoSymbol,	/* 24*/
	SunXK_Props,  	NoSymbol,  	XK_L3,		NoSymbol,	/* 25*/
	SunXK_Undo,  	NoSymbol,  	XK_L4,		NoSymbol,	/* 26*/
	XK_Down,	NoSymbol,	NoSymbol,	NoSymbol,	/* 27*/
	XK_Right,	NoSymbol,	NoSymbol,	NoSymbol,	/* 28*/
	XK_Escape,	NoSymbol,	NoSymbol,	NoSymbol,	/* 29*/
	XK_1,   	XK_exclam,	XK_plusminus, 	NoSymbol,	/* 30*/
	XK_2,   	XK_at,		NoSymbol,  	NoSymbol,	/* 31*/
	XK_3,   	XK_numbersign,	XK_sterling,	NoSymbol,	/* 32*/
	XK_4,   	XK_dollar,	XK_cent,	NoSymbol,	/* 33*/
	XK_5,   	XK_percent,	XK_currency,	NoSymbol,	/* 34*/
	XK_6,   	XK_question,	NoSymbol,	NoSymbol,	/* 35*/
	XK_7,   	XK_ampersand,	NoSymbol,	NoSymbol,	/* 36*/
	XK_8,   	XK_asterisk,	NoSymbol,	NoSymbol,	/* 37*/
	XK_9,   	XK_parenleft,	XK_bracketleft,	NoSymbol,	/* 38*/
	XK_0,   	XK_parenright,	XK_bracketright,NoSymbol,	/* 39*/
	XK_minus,	XK_underscore,	NoSymbol,	NoSymbol,	/* 40*/
	XK_equal,	XK_plus,	XK_notsign,	NoSymbol,	/* 41*/
	XK_slash,	XK_backslash,	XK_bar,		NoSymbol,	/* 42*/
	XK_BackSpace,	NoSymbol,	NoSymbol,	NoSymbol,	/* 43*/
	XK_Insert,	NoSymbol,	NoSymbol,	NoSymbol,	/* 44*/
	SunXK_AudioMute,SunXK_VideoDegauss,NoSymbol,	NoSymbol,	/* 45*/
	XK_KP_Divide,	NoSymbol,	XK_R5,		XK_R5,		/* 46*/
	XK_KP_Multiply,	NoSymbol,	XK_R6,		XK_R6,		/* 47*/
	SunXK_PowerSwitch,SunXK_PowerSwitchShift,NoSymbol,NoSymbol,	/* 48*/
	SunXK_Front,  	NoSymbol,  	XK_L5,		NoSymbol,	/* 49*/
	XK_KP_Delete,	XK_KP_Decimal,	NoSymbol,	NoSymbol,	/* 50*/
	SunXK_Copy,  	NoSymbol,  	XK_L6,		NoSymbol,	/* 51*/
	XK_Home,	NoSymbol,	NoSymbol,	NoSymbol,	/* 52*/
	XK_Tab, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 53*/
	XK_Q,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 54*/
	XK_W,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 55*/
	XK_E,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 56*/
	XK_R,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 57*/
	XK_T,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 58*/
	XK_Y,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 59*/
	XK_U,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 60*/
	XK_I,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 61*/
	XK_O,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 62*/
	XK_P,   	NoSymbol,	XK_paragraph,	NoSymbol,	/* 63*/
	SunXK_FA_Circum,SunXK_FA_Diaeresis,SunXK_FA_Grave,NoSymbol,	/* 64*/
	XK_Ccedilla,	NoSymbol,	XK_asciitilde,	NoSymbol,	/* 65*/
	XK_Delete,	NoSymbol,	NoSymbol,	NoSymbol,	/* 66*/
	SunXK_Compose,	NoSymbol,	NoSymbol,	NoSymbol,	/* 67*/
	XK_KP_Home,  	XK_KP_7,  	NoSymbol,	NoSymbol,	/* 68*/
	XK_KP_Up,  	XK_KP_8,  	NoSymbol,	NoSymbol,	/* 69*/
	XK_KP_Prior,  	XK_KP_9,  	NoSymbol,	NoSymbol,	/* 70*/
	XK_KP_Subtract,	NoSymbol,	XK_R4,		XK_R4,		/* 71*/
	SunXK_Open,  	NoSymbol,  	XK_L7,		NoSymbol,	/* 72*/
	SunXK_Paste,  	NoSymbol,  	XK_L8,		NoSymbol,	/* 73*/
	XK_End, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 74*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 75*/
	XK_Control_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 76*/
	XK_A,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 77*/
	XK_S,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 78*/
	XK_D,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 79*/
	XK_F,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 80*/
	XK_G,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 81*/
	XK_H,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 82*/
	XK_J,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 83*/
	XK_K,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 84*/
	XK_L,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 85*/
	XK_semicolon,	XK_colon,	XK_degree,	NoSymbol,	/* 86*/
	XK_Egrave,	NoSymbol,	NoSymbol,	NoSymbol,	/* 87*/
	XK_Agrave,	NoSymbol,	NoSymbol,	NoSymbol,	/* 88*/
	XK_Return,	NoSymbol,	NoSymbol,	NoSymbol,	/* 89*/
	XK_KP_Enter,	NoSymbol,	NoSymbol,	NoSymbol,	/* 90*/
	XK_KP_Left,	XK_KP_4, 	NoSymbol,	NoSymbol,	/* 91*/
	NoSymbol, 	XK_KP_5, 	NoSymbol,	NoSymbol,	/* 92*/
	XK_KP_Right,	XK_KP_6, 	NoSymbol,	NoSymbol,	/* 93*/
	XK_KP_Insert,	XK_KP_0,	NoSymbol,	NoSymbol,	/* 94*/
	SunXK_Find,  	NoSymbol,  	XK_L9,		NoSymbol,	/* 95*/
	XK_Prior,	NoSymbol,	NoSymbol,	NoSymbol,	/* 96*/
	SunXK_Cut, 	NoSymbol, 	XK_L10,		NoSymbol,	/* 97*/
	XK_Num_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 98*/
	XK_Shift_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 99*/
	XK_Z,   	NoSymbol,	XK_guillemotleft,NoSymbol,	/*100*/
	XK_X,   	NoSymbol,	XK_guillemotright,NoSymbol,	/*101*/
	XK_C,   	NoSymbol,	NoSymbol,	NoSymbol,	/*102*/
	XK_V,   	NoSymbol,	NoSymbol,	NoSymbol,	/*103*/
	XK_B,   	NoSymbol,	NoSymbol,	NoSymbol,	/*104*/
	XK_N,   	NoSymbol,	NoSymbol,	NoSymbol,	/*105*/
	XK_M,   	NoSymbol,	XK_mu,		NoSymbol,	/*106*/
	XK_comma,	XK_quoteright,	XK_less,	NoSymbol,	/*107*/
	XK_period,	XK_quotedbl,	XK_greater,	NoSymbol,	/*108*/
	XK_Eacute,	NoSymbol,	XK_quoteleft,	NoSymbol,	/*109*/
	XK_Shift_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*110*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*111*/
	XK_KP_End, 	XK_KP_1, 	NoSymbol,	NoSymbol, 	/*112*/
	XK_KP_Down,	XK_KP_2, 	NoSymbol,	NoSymbol,	/*113*/
	XK_KP_Next, 	XK_KP_3, 	NoSymbol,	NoSymbol,	/*114*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*115*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*116*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*117*/
	XK_Help,	NoSymbol,	NoSymbol,	NoSymbol,	/*118*/
	XK_Caps_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/*119*/
	XK_Meta_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*120*/
	XK_space,	NoSymbol,	NoSymbol,	NoSymbol,	/*121*/
	XK_Meta_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*122*/
	XK_Next,	NoSymbol,	NoSymbol,	NoSymbol,	/*123*/
	XK_Uacute,	NoSymbol,	NoSymbol,	NoSymbol,	/*124*/
	XK_KP_Add,	NoSymbol,	NoSymbol,	NoSymbol,	/*125*/
};

#define CanadaFr5Modmap Generic5Modmap

#else

#define CanadaFr5Keymap NULL
#define CanadaFr5Modmap NULL

#endif /* CANFR5 */


#ifdef DEN4

static KeySym Denmark4Keymap[] = {
	SunXK_Stop,  	NoSymbol,  	XK_L1,		NoSymbol,	/*  1*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  2*/
	SunXK_Again,  	NoSymbol,  	XK_L2,		NoSymbol,	/*  3*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  4*/
	XK_F1,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  5*/
	XK_F2,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  6*/
	XK_F10, 	NoSymbol,	NoSymbol,	NoSymbol,	/*  7*/
	XK_F3,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  8*/
	XK_F11,		NoSymbol,	SunXK_F36,	NoSymbol,	/*  9*/
	XK_F4,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 10*/
	XK_F12,		NoSymbol,	SunXK_F37,	NoSymbol,	/* 11*/
	XK_F5,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 12*/
	SunXK_Compose,	NoSymbol,	NoSymbol,	NoSymbol,	/* 13*/
	XK_F6,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 14*/
	XK_asciitilde,	XK_asciicircum,	NoSymbol,	NoSymbol,	/* 15*/
	XK_F7,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 16*/
	XK_F8,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 17*/
	XK_F9,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 18*/
	XK_Alt_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 19*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 20*/
	XK_Pause,  	XK_R1,  	XK_Pause,	NoSymbol,	/* 21*/
	XK_Print,  	XK_R2,  	XK_Print,	NoSymbol,	/* 22*/
	XK_Break,  	XK_Scroll_Lock,	XK_R3,		NoSymbol,	/* 23*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 24*/
	SunXK_Props,  	NoSymbol,  	XK_L3,		NoSymbol,	/* 25*/
	SunXK_Undo,  	NoSymbol,  	XK_L4,		NoSymbol,	/* 26*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 27*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 28*/
	XK_Escape,	NoSymbol,	NoSymbol,	NoSymbol,	/* 29*/
	XK_1,   	XK_exclam,	NoSymbol,	NoSymbol,	/* 30*/
	XK_2,   	XK_quotedbl,	XK_at,  	NoSymbol,	/* 31*/
	XK_3,   	XK_numbersign,	XK_sterling,	NoSymbol,	/* 32*/
	XK_4,   	XK_currency,	XK_dollar,	NoSymbol,	/* 33*/
	XK_5,   	XK_percent,	NoSymbol,	NoSymbol,	/* 34*/
	XK_6,   	XK_ampersand,	NoSymbol,	NoSymbol,	/* 35*/
	XK_7,   	XK_slash,	XK_braceleft,	NoSymbol,	/* 36*/
	XK_8,   	XK_parenleft,	XK_bracketleft,	NoSymbol,	/* 37*/
	XK_9,   	XK_parenright,	XK_bracketright,NoSymbol,	/* 38*/
	XK_0,   	XK_equal,	XK_braceright,	NoSymbol,	/* 39*/
	XK_plus,	XK_question,	NoSymbol,	NoSymbol,	/* 40*/
	SunXK_FA_Acute,	SunXK_FA_Grave,	XK_bar, 	NoSymbol,	/* 41*/
	XK_apostrophe,	XK_asterisk,	XK_grave,	NoSymbol,	/* 42*/
	XK_BackSpace,	NoSymbol,	NoSymbol,	NoSymbol,	/* 43*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 44*/
	XK_KP_Equal,  	NoSymbol,  	XK_R4,		NoSymbol,	/* 45*/
	XK_KP_Divide,  	NoSymbol,  	XK_R5,		NoSymbol,	/* 46*/
	XK_KP_Multiply, NoSymbol,  	XK_R6,		NoSymbol,	/* 47*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 48*/
	SunXK_Front,  	NoSymbol,  	XK_L5,		NoSymbol,	/* 49*/
	XK_KP_Delete,	XK_KP_Decimal,	NoSymbol,	NoSymbol,	/* 50*/
	SunXK_Copy,  	NoSymbol,  	XK_L6,		NoSymbol,	/* 51*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 52*/
	XK_Tab, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 53*/
	XK_Q,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 54*/
	XK_W,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 55*/
	XK_E,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 56*/
	XK_R,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 57*/
	XK_T,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 58*/
	XK_Y,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 59*/
	XK_U,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 60*/
	XK_I,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 61*/
	XK_O,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 62*/
	XK_P,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 63*/
	XK_Aring,	NoSymbol,	NoSymbol,	NoSymbol,	/* 64*/
	SunXK_FA_Diaeresis,SunXK_FA_Circum,SunXK_FA_Tilde,NoSymbol,	/* 65*/
	XK_Delete,	NoSymbol,	NoSymbol,	NoSymbol,	/* 66*/
	SunXK_AltGraph,	NoSymbol,	NoSymbol,	NoSymbol,	/* 67*/
	XK_KP_Home,  	XK_KP_7,  	XK_R7,		NoSymbol,	/* 68*/
	XK_KP_Up,  	XK_KP_8,  	XK_R8,		NoSymbol,	/* 69*/
	XK_KP_Prior,  	XK_KP_9,  	XK_R9,		NoSymbol,	/* 70*/
	XK_KP_Subtract,	NoSymbol,	NoSymbol,	NoSymbol,	/* 71*/
	SunXK_Open,  	NoSymbol,  	XK_L7,		NoSymbol,	/* 72*/
	SunXK_Paste,  	NoSymbol,  	XK_L8,		NoSymbol,	/* 73*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 74*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 75*/
	XK_Caps_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 76*/
	XK_A,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 77*/
	XK_S,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 78*/
	XK_D,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 79*/
	XK_F,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 80*/
	XK_G,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 81*/
	XK_H,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 82*/
	XK_J,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 83*/
	XK_K,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 84*/
	XK_L,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 85*/
	XK_AE,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 86*/
	XK_Ooblique,	NoSymbol,	NoSymbol,	NoSymbol,	/* 87*/
	XK_onehalf,	XK_section,	NoSymbol,	NoSymbol,	/* 88*/
	XK_Return,	NoSymbol,	NoSymbol,	NoSymbol,	/* 89*/
	XK_KP_Enter,	NoSymbol,	NoSymbol,	NoSymbol,	/* 90*/
	XK_KP_Left,	XK_KP_4, 	XK_R10,		NoSymbol,	/* 91*/
	NoSymbol, 	XK_KP_5, 	XK_R11,		NoSymbol,	/* 92*/
	XK_KP_Right,	XK_KP_6, 	XK_R12,		NoSymbol,	/* 93*/
	XK_KP_Insert,	XK_KP_0,	NoSymbol,	NoSymbol,	/* 94*/
	SunXK_Find,  	NoSymbol,  	XK_L9,		NoSymbol,	/* 95*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 96*/
	SunXK_Cut, 	NoSymbol, 	XK_L10,		NoSymbol,	/* 97*/
	XK_Num_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 98*/
	XK_Shift_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 99*/
	XK_Z,   	NoSymbol,	NoSymbol,	NoSymbol,	/*100*/
	XK_X,   	NoSymbol,	NoSymbol,	NoSymbol,	/*101*/
	XK_C,   	NoSymbol,	NoSymbol,	NoSymbol,	/*102*/
	XK_V,   	NoSymbol,	NoSymbol,	NoSymbol,	/*103*/
	XK_B,   	NoSymbol,	NoSymbol,	NoSymbol,	/*104*/
	XK_N,   	NoSymbol,	NoSymbol,	NoSymbol,	/*105*/
	XK_M,   	NoSymbol,	NoSymbol,	NoSymbol,	/*106*/
	XK_comma,	XK_semicolon,	NoSymbol,	NoSymbol,	/*107*/
	XK_period,	XK_colon,	NoSymbol,	NoSymbol,	/*108*/
	XK_minus,	XK_underscore,	NoSymbol,	NoSymbol,	/*109*/
	XK_Shift_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*110*/
	XK_Linefeed,	NoSymbol,	NoSymbol,	NoSymbol,	/*111*/
	XK_KP_End, 	XK_KP_1, 	XK_R13,		NoSymbol, 	/*112*/
	XK_KP_Down,	XK_KP_2, 	XK_R14,		NoSymbol,	/*113*/
	XK_KP_Next, 	XK_KP_3, 	XK_R15,		NoSymbol,	/*114*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*115*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*116*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*117*/
	XK_Help,	NoSymbol,	NoSymbol,	NoSymbol,	/*118*/
	XK_Control_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*119*/
	XK_Meta_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*120*/
	XK_space,	NoSymbol,	NoSymbol,	NoSymbol,	/*121*/
	XK_Meta_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*122*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*123*/
	XK_less,	XK_greater,	XK_backslash,	NoSymbol,	/*124*/
	XK_KP_Add,	NoSymbol,	NoSymbol,	NoSymbol,	/*125*/
};

#define Denmark4Modmap DenSwedFinSw4Modmap

#else

#define Denmark4Keymap NULL
#define Denmark4Modmap NULL

#endif /* DENMARK4 */


#ifdef DEN5

static KeySym Denmark5Keymap[] = {
	SunXK_Stop,  	NoSymbol,  	XK_L1,		NoSymbol,	/*  1*/
	SunXK_AudioLowerVolume,SunXK_VideoLowerBrightness,NoSymbol,	NoSymbol,	/*  2*/
	SunXK_Again,  	NoSymbol,  	XK_L2,		NoSymbol,	/*  3*/
	SunXK_AudioRaiseVolume,SunXK_VideoRaiseBrightness,NoSymbol,	NoSymbol,	/*  4*/
	XK_F1,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  5*/
	XK_F2,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  6*/
	XK_F10, 	NoSymbol,	NoSymbol,	NoSymbol,	/*  7*/
	XK_F3,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  8*/
	XK_F11,		NoSymbol,	SunXK_F36,	NoSymbol,	/*  9*/
	XK_F4,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 10*/
	XK_F12,		NoSymbol,	SunXK_F37,	NoSymbol,	/* 11*/
	XK_F5,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 12*/
	SunXK_AltGraph,	NoSymbol,	NoSymbol,	NoSymbol,	/* 13*/
	XK_F6,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 14*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 15*/
	XK_F7,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 16*/
	XK_F8,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 17*/
	XK_F9,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 18*/
	XK_Alt_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 19*/
	XK_Up,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 20*/
	XK_Pause,  	NoSymbol,  	XK_Break,	NoSymbol,	/* 21*/
	XK_Print,  	NoSymbol,  	XK_Sys_Req,	SunXK_Sys_Req,	/* 22*/
	XK_Scroll_Lock, NoSymbol,  	NoSymbol,	NoSymbol,	/* 23*/
	XK_Left,	NoSymbol,	NoSymbol,	NoSymbol,	/* 24*/
	SunXK_Props,  	NoSymbol,  	XK_L3,		NoSymbol,	/* 25*/
	SunXK_Undo,  	NoSymbol,  	XK_L4,		NoSymbol,	/* 26*/
	XK_Down,	NoSymbol,	NoSymbol,	NoSymbol,	/* 27*/
	XK_Right,	NoSymbol,	NoSymbol,	NoSymbol,	/* 28*/
	XK_Escape,	NoSymbol,	NoSymbol,	NoSymbol,	/* 29*/
	XK_1,   	XK_exclam,	NoSymbol,	NoSymbol,	/* 30*/
	XK_2,   	XK_quotedbl,	XK_at,  	NoSymbol,	/* 31*/
	XK_3,   	XK_numbersign,	XK_sterling,	NoSymbol,	/* 32*/
	XK_4,   	XK_currency,	XK_dollar,	NoSymbol,	/* 33*/
	XK_5,   	XK_percent,	XK_asciitilde,	NoSymbol,	/* 34*/
	XK_6,   	XK_ampersand,	XK_asciicircum,	NoSymbol,	/* 35*/
	XK_7,   	XK_slash,	XK_braceleft,	NoSymbol,	/* 36*/
	XK_8,   	XK_parenleft,	XK_bracketleft,	NoSymbol,	/* 37*/
	XK_9,   	XK_parenright,	XK_bracketright,NoSymbol,	/* 38*/
	XK_0,   	XK_equal,	XK_braceright,	NoSymbol,	/* 39*/
	XK_plus,	XK_question,	NoSymbol,	NoSymbol,	/* 40*/
	SunXK_FA_Acute,	SunXK_FA_Grave,	XK_bar, 	NoSymbol,	/* 41*/
	XK_onehalf,	XK_section,	NoSymbol,	NoSymbol,	/* 42*/
	XK_BackSpace,	NoSymbol,	NoSymbol,	NoSymbol,	/* 43*/
	XK_Insert,	NoSymbol,	NoSymbol,	NoSymbol,	/* 44*/
	SunXK_AudioMute,SunXK_VideoDegauss,NoSymbol,	NoSymbol,	/* 45*/
	XK_KP_Divide,  	NoSymbol,  	NoSymbol,	NoSymbol,	/* 46*/
	XK_KP_Multiply, NoSymbol,  	NoSymbol,	NoSymbol,	/* 47*/
	SunXK_PowerSwitch,SunXK_PowerSwitchShift,	NoSymbol,	NoSymbol,	/* 48*/
	SunXK_Front,  	NoSymbol,  	XK_L5,		NoSymbol,	/* 49*/
	XK_Delete,	NoSymbol,	NoSymbol,	NoSymbol,	/* 50*/
	SunXK_Copy,  	NoSymbol,  	XK_L6,		NoSymbol,	/* 51*/
	XK_Home,	NoSymbol,	NoSymbol,	NoSymbol,	/* 52*/
	XK_Tab, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 53*/
	XK_Q,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 54*/
	XK_W,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 55*/
	XK_E,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 56*/
	XK_R,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 57*/
	XK_T,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 58*/
	XK_Y,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 59*/
	XK_U,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 60*/
	XK_I,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 61*/
	XK_O,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 62*/
	XK_P,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 63*/
	XK_Aring,	NoSymbol,	NoSymbol,	NoSymbol,	/* 64*/
	SunXK_FA_Diaeresis,SunXK_FA_Circum,SunXK_FA_Tilde,NoSymbol,	/* 65*/
	XK_Delete,	NoSymbol,	NoSymbol,	NoSymbol,	/* 66*/
	SunXK_Compose,	NoSymbol,	NoSymbol,	NoSymbol,	/* 67*/
	XK_KP_Home,  	XK_KP_7,  	NoSymbol,	NoSymbol,	/* 68*/
	XK_KP_Up,  	XK_KP_8,  	NoSymbol,	NoSymbol,	/* 69*/
	XK_KP_Prior,  	XK_KP_9,  	NoSymbol,	NoSymbol,	/* 70*/
	XK_KP_Subtract,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 71*/
	SunXK_Open,  	NoSymbol,  	XK_L7,		NoSymbol,	/* 72*/
	SunXK_Paste,  	NoSymbol,  	XK_L8,		NoSymbol,	/* 73*/
	XK_End, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 74*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 75*/
	XK_Control_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 76*/
	XK_A,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 77*/
	XK_S,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 78*/
	XK_D,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 79*/
	XK_F,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 80*/
	XK_G,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 81*/
	XK_H,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 82*/
	XK_J,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 83*/
	XK_K,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 84*/
	XK_L,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 85*/
	XK_AE,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 86*/
	XK_Ooblique,	NoSymbol,	NoSymbol,	NoSymbol,	/* 87*/
	XK_apostrophe,	XK_asterisk,	XK_grave,	NoSymbol,	/* 88*/
	XK_Return,	NoSymbol,	NoSymbol,	NoSymbol,	/* 89*/
	XK_KP_Enter,	NoSymbol,	NoSymbol,	NoSymbol,	/* 90*/
	XK_KP_Left,	XK_KP_4, 	NoSymbol,	NoSymbol,	/* 91*/
	NoSymbol, 	XK_KP_5, 	NoSymbol,	NoSymbol,	/* 92*/
	XK_KP_Right,	XK_KP_6, 	NoSymbol,	NoSymbol,	/* 93*/
	XK_KP_Insert,	XK_KP_0,	NoSymbol,	NoSymbol,	/* 94*/
	SunXK_Find,  	NoSymbol,  	XK_L9,		NoSymbol,	/* 95*/
	XK_Prior,	NoSymbol,	NoSymbol,	NoSymbol,	/* 96*/
	SunXK_Cut, 	NoSymbol, 	XK_L10,		NoSymbol,	/* 97*/
	XK_Num_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 98*/
	XK_Shift_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 99*/
	XK_Z,   	NoSymbol,	NoSymbol,	NoSymbol,	/*100*/
	XK_X,   	NoSymbol,	NoSymbol,	NoSymbol,	/*101*/
	XK_C,   	NoSymbol,	NoSymbol,	NoSymbol,	/*102*/
	XK_V,   	NoSymbol,	NoSymbol,	NoSymbol,	/*103*/
	XK_B,   	NoSymbol,	NoSymbol,	NoSymbol,	/*104*/
	XK_N,   	NoSymbol,	NoSymbol,	NoSymbol,	/*105*/
	XK_M,   	NoSymbol,	NoSymbol,	NoSymbol,	/*106*/
	XK_comma,	XK_semicolon,	NoSymbol,	NoSymbol,	/*107*/
	XK_period,	XK_colon,	NoSymbol,	NoSymbol,	/*108*/
	XK_minus,	XK_underscore,	NoSymbol,	NoSymbol,	/*109*/
	XK_Shift_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*110*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*111*/
	XK_KP_End, 	XK_KP_1, 	NoSymbol,	NoSymbol, 	/*112*/
	XK_KP_Down,	XK_KP_2, 	NoSymbol,	NoSymbol,	/*113*/
	XK_KP_Next, 	XK_KP_3, 	NoSymbol,	NoSymbol,	/*114*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*115*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*116*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*117*/
	XK_Help,	NoSymbol,	NoSymbol,	NoSymbol,	/*118*/
	XK_Caps_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/*119*/
	XK_Meta_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*120*/
	XK_space,	NoSymbol,	NoSymbol,	NoSymbol,	/*121*/
	XK_Meta_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*122*/
	XK_Next,	NoSymbol,	NoSymbol,	NoSymbol,	/*123*/
	XK_less,	XK_greater,	XK_backslash,	NoSymbol,	/*124*/
	XK_KP_Add,	NoSymbol,	NoSymbol,	NoSymbol,	/*125*/
};

#define Denmark5Modmap Generic5Modmap

#else

#define Denmark5Keymap NULL
#define Denmark5Modmap NULL

#endif /* DEN5 */


#ifdef FR5

static KeySym France5Keymap[] = {
	SunXK_Stop,  	NoSymbol,  	XK_L1,		NoSymbol,	/*  1*/
	SunXK_AudioLowerVolume,SunXK_VideoLowerBrightness,NoSymbol,	NoSymbol,	/*  2*/
	SunXK_Again,  	NoSymbol,  	XK_L2,		NoSymbol,	/*  3*/
	SunXK_AudioRaiseVolume,SunXK_VideoRaiseBrightness,NoSymbol,	NoSymbol,	/*  4*/
	XK_F1,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  5*/
	XK_F2,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  6*/
	XK_F10, 	NoSymbol,	NoSymbol,	NoSymbol,	/*  7*/
	XK_F3,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  8*/
	XK_F11,		NoSymbol,	SunXK_F36,	NoSymbol,	/*  9*/
	XK_F4,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 10*/
	XK_F12,		NoSymbol,	SunXK_F37,	NoSymbol,	/* 11*/
	XK_F5,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 12*/
	SunXK_AltGraph,	NoSymbol,	NoSymbol,	NoSymbol,	/* 13*/
	XK_F6,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 14*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 15*/
	XK_F7,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 16*/
	XK_F8,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 17*/
	XK_F9,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 18*/
	XK_Alt_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 19*/
	XK_Up,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 20*/
	XK_Pause,  	NoSymbol,  	XK_Break,	NoSymbol,	/* 21*/
	XK_Print,  	NoSymbol,  	XK_Sys_Req,	SunXK_Sys_Req,	/* 22*/
	XK_Scroll_Lock,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 23*/
	XK_Left,	NoSymbol,	NoSymbol,	NoSymbol,	/* 24*/
	SunXK_Props,  	NoSymbol,  	XK_L3,		NoSymbol,	/* 25*/
	SunXK_Undo,  	NoSymbol,  	XK_L4,		NoSymbol,	/* 26*/
	XK_Down,	NoSymbol,	NoSymbol,	NoSymbol,	/* 27*/
	XK_Right,	NoSymbol,	NoSymbol,	NoSymbol,	/* 28*/
	XK_Escape,	NoSymbol,	NoSymbol,	NoSymbol,	/* 29*/
	XK_ampersand,	XK_1,   	NoSymbol,	NoSymbol,	/* 30*/
	XK_eacute,	XK_2,   	XK_asciitilde,	NoSymbol,	/* 31*/
	XK_quotedbl,	XK_3,   	XK_numbersign,	NoSymbol,	/* 32*/
	XK_apostrophe,	XK_4,   	XK_braceleft,	NoSymbol,	/* 33*/
	XK_parenleft,	XK_5,   	XK_bracketleft,	NoSymbol,	/* 34*/
	XK_minus,	XK_6,   	XK_bar, 	NoSymbol,	/* 35*/
	XK_egrave,	XK_7,   	XK_grave,	NoSymbol,	/* 36*/
	XK_underscore,	XK_8,   	XK_backslash,	NoSymbol,	/* 37*/
	XK_ccedilla,	XK_9,   	XK_asciicircum,	NoSymbol,	/* 38*/
	XK_agrave,	XK_0,   	XK_at,  	NoSymbol,	/* 39*/
	XK_parenright,	XK_degree,	XK_bracketright,NoSymbol,	/* 40*/
	XK_equal,	XK_plus,	XK_braceright,	NoSymbol,	/* 41*/
	XK_twosuperior,	NoSymbol,	NoSymbol,	NoSymbol,	/* 42*/
	XK_BackSpace,	NoSymbol,	NoSymbol,	NoSymbol,	/* 43*/
	XK_Insert,	NoSymbol,	NoSymbol,	NoSymbol,	/* 44*/
	SunXK_AudioMute,SunXK_VideoDegauss,NoSymbol,	NoSymbol,	/* 45*/
	XK_KP_Divide,  	NoSymbol,  	NoSymbol,	NoSymbol,	/* 46*/
	XK_KP_Multiply,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 47*/
	SunXK_PowerSwitch,SunXK_PowerSwitchShift,	NoSymbol,	NoSymbol,	/* 48*/
	SunXK_Front,  	NoSymbol,  	XK_L5,		NoSymbol,	/* 49*/
	XK_KP_Delete,	XK_KP_Decimal,	NoSymbol,	NoSymbol,	/* 50*/
	SunXK_Copy,  	NoSymbol,  	XK_L6,		NoSymbol,	/* 51*/
	XK_Home,	NoSymbol,	NoSymbol,	NoSymbol,	/* 52*/
	XK_Tab, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 53*/
	XK_A,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 54*/
	XK_Z,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 55*/
	XK_E,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 56*/
	XK_R,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 57*/
	XK_T,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 58*/
	XK_Y,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 59*/
	XK_U,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 60*/
	XK_I,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 61*/
	XK_O,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 62*/
	XK_P,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 63*/
	SunXK_FA_Circum,SunXK_FA_Diaeresis,NoSymbol,	NoSymbol,	/* 64*/
	XK_dollar,	XK_sterling,	XK_currency,	NoSymbol,	/* 65*/
	XK_Delete,	NoSymbol,	NoSymbol,	NoSymbol,	/* 66*/
	SunXK_Compose,	NoSymbol,	NoSymbol,	NoSymbol,	/* 67*/
	XK_KP_Home,  	XK_KP_7,  	NoSymbol,	NoSymbol,	/* 68*/
	XK_KP_Up,  	XK_KP_8,  	NoSymbol,	NoSymbol,	/* 69*/
	XK_KP_Prior,  	XK_KP_9,  	NoSymbol,	NoSymbol,	/* 70*/
	XK_KP_Subtract,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 71*/
	SunXK_Open,  	NoSymbol,  	XK_L7,		NoSymbol,	/* 72*/
	SunXK_Paste,  	NoSymbol,  	XK_L8,		NoSymbol,	/* 73*/
	XK_End, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 74*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 75*/
	XK_Control_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 76*/
	XK_Q,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 77*/
	XK_S,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 78*/
	XK_D,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 79*/
	XK_F,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 80*/
	XK_G,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 81*/
	XK_H,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 82*/
	XK_J,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 83*/
	XK_K,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 84*/
	XK_L,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 85*/
	XK_M,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 86*/
	XK_ugrave,	XK_percent,	NoSymbol,	NoSymbol,	/* 87*/
	XK_asterisk,	XK_mu,  	NoSymbol,	NoSymbol,	/* 88*/
	XK_Return,	NoSymbol,	NoSymbol,	NoSymbol,	/* 89*/
	XK_KP_Enter,	NoSymbol,	NoSymbol,	NoSymbol,	/* 90*/
	XK_KP_Left,	XK_KP_4, 	NoSymbol,	NoSymbol,	/* 91*/
	NoSymbol, 	XK_KP_5, 	NoSymbol,	NoSymbol,	/* 92*/
	XK_KP_Right,	XK_KP_6, 	NoSymbol,	NoSymbol,	/* 93*/
	XK_KP_Insert,	XK_KP_0,	NoSymbol,	NoSymbol,	/* 94*/
	SunXK_Find,  	NoSymbol,  	XK_L9,		NoSymbol,	/* 95*/
	XK_Prior,	NoSymbol,	NoSymbol,	NoSymbol,	/* 96*/
	SunXK_Cut, 	NoSymbol, 	XK_L10,		NoSymbol,	/* 97*/
	XK_Num_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 98*/
	XK_Shift_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 99*/
	XK_W,   	NoSymbol,	NoSymbol,	NoSymbol,	/*100*/
	XK_X,   	NoSymbol,	NoSymbol,	NoSymbol,	/*101*/
	XK_C,   	NoSymbol,	NoSymbol,	NoSymbol,	/*102*/
	XK_V,   	NoSymbol,	NoSymbol,	NoSymbol,	/*103*/
	XK_B,   	NoSymbol,	NoSymbol,	NoSymbol,	/*104*/
	XK_N,   	NoSymbol,	NoSymbol,	NoSymbol,	/*105*/
	XK_comma,	XK_question,	NoSymbol,	NoSymbol,	/*106*/
	XK_semicolon,	XK_period,	NoSymbol,	NoSymbol,	/*107*/
	XK_colon,	XK_slash,	NoSymbol,	NoSymbol,	/*108*/
	XK_exclam,	XK_section,	NoSymbol,	NoSymbol,	/*109*/
	XK_Shift_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*110*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*111*/
	XK_KP_End, 	XK_KP_1, 	NoSymbol,	NoSymbol, 	/*112*/
	XK_KP_Down,	XK_KP_2, 	NoSymbol,	NoSymbol,	/*113*/
	XK_KP_Next, 	XK_KP_3, 	NoSymbol,	XK_Next,	/*114*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*115*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*116*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*117*/
	XK_Help,	NoSymbol,	NoSymbol,	NoSymbol,	/*118*/
	XK_Caps_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/*119*/
	XK_Meta_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*120*/
	XK_space,	NoSymbol,	NoSymbol,	NoSymbol,	/*121*/
	XK_Meta_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*122*/
	XK_Next,	NoSymbol,	NoSymbol,	NoSymbol,	/*123*/
	XK_less,	XK_greater,	NoSymbol,	NoSymbol,	/*124*/
	XK_KP_Add,	NoSymbol,	NoSymbol,	NoSymbol,	/*125*/
};

#define France5Modmap Generic5Modmap

#else

#define France5Keymap NULL
#define France5Modmap NULL

#endif /* FRANCE5 */


#ifdef FRBE4

static KeySym FranceBelg4Keymap[] = {
	SunXK_Stop,  	NoSymbol,  	XK_L1,		NoSymbol,	/*  1*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  2*/
	SunXK_Again,  	NoSymbol,  	XK_L2,		NoSymbol,	/*  3*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  4*/
	XK_F1,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  5*/
	XK_F2,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  6*/
	XK_F10, 	NoSymbol,	NoSymbol,	NoSymbol,	/*  7*/
	XK_F3,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  8*/
	XK_F11,		NoSymbol,	SunXK_F36,	NoSymbol,	/*  9*/
	XK_F4,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 10*/
	XK_F12,		NoSymbol,	SunXK_F37,	NoSymbol,	/* 11*/
	XK_F5,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 12*/
	XK_Caps_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 13*/
	XK_F6,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 14*/
	XK_bracketright,XK_braceright,	XK_guillemotright,NoSymbol,	/* 15*/
	XK_F7,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 16*/
	XK_F8,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 17*/
	XK_F9,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 18*/
	XK_Alt_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 19*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 20*/
	XK_Pause,  	NoSymbol,  	XK_R1,		NoSymbol,	/* 21*/
	XK_Print,  	NoSymbol,  	XK_R2,		NoSymbol,	/* 22*/
	XK_Break,  	XK_Scroll_Lock,	XK_R3,		NoSymbol,	/* 23*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 24*/
	SunXK_Props,  	NoSymbol,  	XK_L3,		NoSymbol,	/* 25*/
	SunXK_Undo,  	NoSymbol,  	XK_L4,		NoSymbol,	/* 26*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 27*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 28*/
	XK_Escape,	NoSymbol,	NoSymbol,	NoSymbol,	/* 29*/
	XK_ampersand,	XK_1,   	NoSymbol,	NoSymbol,	/* 30*/
	XK_eacute,	XK_2,   	XK_twosuperior,	NoSymbol,	/* 31*/
	XK_quotedbl,	XK_3,   	XK_threesuperior,NoSymbol,	/* 32*/
	XK_apostrophe,	XK_4,   	XK_acute,	NoSymbol,	/* 33*/
	XK_parenleft,	XK_5,   	NoSymbol,	NoSymbol,	/* 34*/
	XK_section,	XK_6,   	XK_asciicircum,	NoSymbol,	/* 35*/
	XK_egrave,	XK_7,   	NoSymbol,	NoSymbol,	/* 36*/
	XK_exclam,	XK_8,   	XK_sterling,	NoSymbol,	/* 37*/
	XK_ccedilla,	XK_9,   	XK_backslash,	NoSymbol,	/* 38*/
	XK_agrave,	XK_0,   	NoSymbol,	NoSymbol,	/* 39*/
	XK_parenright,	XK_degree,	XK_asciitilde,	NoSymbol,	/* 40*/
	XK_minus,	XK_underscore,	XK_numbersign,	NoSymbol,	/* 41*/
	XK_asterisk,	XK_bar, 	XK_currency,	NoSymbol,	/* 42*/
	XK_BackSpace,	NoSymbol,	NoSymbol,	NoSymbol,	/* 43*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 44*/
	XK_KP_Equal,  	XK_KP_Equal,  	XK_R4,		NoSymbol,	/* 45*/
	XK_KP_Divide,  	XK_KP_Divide,  	XK_R5,		NoSymbol,	/* 46*/
	XK_KP_Multiply,	XK_KP_Multiply,	XK_R6,		NoSymbol,	/* 47*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 48*/
	SunXK_Front,  	NoSymbol,  	XK_L5,		NoSymbol,	/* 49*/
	XK_KP_Delete,	XK_KP_Decimal,	NoSymbol,	NoSymbol,	/* 50*/
	SunXK_Copy,  	NoSymbol,  	XK_L6,		NoSymbol,	/* 51*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 52*/
	XK_Tab, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 53*/
	XK_A,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 54*/
	XK_Z,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 55*/
	XK_E,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 56*/
	XK_R,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 57*/
	XK_T,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 58*/
	XK_Y,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 59*/
	XK_U,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 60*/
	XK_I,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 61*/
	XK_O,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 62*/
	XK_P,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 63*/
	SunXK_FA_Circum,SunXK_FA_Diaeresis,NoSymbol,	NoSymbol,	/* 64*/
	XK_grave,	XK_dollar,	XK_at,  	NoSymbol,	/* 65*/
	XK_Delete,	NoSymbol,	NoSymbol,	NoSymbol,	/* 66*/
	SunXK_Compose,	NoSymbol,	NoSymbol,	NoSymbol,	/* 67*/
	XK_KP_Home,  	XK_KP_7,  	XK_R7,		NoSymbol,	/* 68*/
	XK_KP_Up,  	XK_KP_8,  	XK_R8,		NoSymbol,	/* 69*/
	XK_KP_Prior,  	XK_KP_9,  	XK_R9,		NoSymbol,	/* 70*/
	XK_KP_Subtract,	XK_KP_Subtract,	NoSymbol,	NoSymbol,	/* 71*/
	SunXK_Open,  	NoSymbol,  	XK_L7,		NoSymbol,	/* 72*/
	SunXK_Paste,  	NoSymbol,  	XK_L8,		NoSymbol,	/* 73*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 74*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 75*/
	XK_Control_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 76*/
	XK_Q,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 77*/
	XK_S,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 78*/
	XK_D,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 79*/
	XK_F,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 80*/
	XK_G,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 81*/
	XK_H,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 82*/
	XK_J,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 83*/
	XK_K,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 84*/
	XK_L,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 85*/
	XK_M,   	NoSymbol,	XK_mu,  	NoSymbol,	/* 86*/
	XK_ugrave,	XK_percent,	NoSymbol,	NoSymbol,	/* 87*/
	XK_bracketleft,	XK_braceleft,	XK_guillemotleft,NoSymbol,	/* 88*/
	XK_Return,	NoSymbol,	NoSymbol,	NoSymbol,	/* 89*/
	XK_KP_Enter,	XK_KP_Enter,	NoSymbol,	NoSymbol,	/* 90*/
	XK_KP_Left,	XK_KP_4, 	XK_R10,		NoSymbol,	/* 91*/
	NoSymbol, 	XK_KP_5, 	XK_R11,		NoSymbol,	/* 92*/
	XK_KP_Right,	XK_KP_6, 	XK_R12,		NoSymbol,	/* 93*/
	XK_KP_Insert,	XK_KP_0,	NoSymbol,	NoSymbol,	/* 94*/
	SunXK_Find,  	NoSymbol,  	XK_L9,		NoSymbol,	/* 95*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 96*/
	SunXK_Cut, 	NoSymbol, 	XK_L10,		NoSymbol,	/* 97*/
	XK_Num_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 98*/
	XK_Shift_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 99*/
	XK_W,   	NoSymbol,	NoSymbol,	NoSymbol,	/*100*/
	XK_X,   	NoSymbol,	NoSymbol,	NoSymbol,	/*101*/
	XK_C,   	NoSymbol,	NoSymbol,	NoSymbol,	/*102*/
	XK_V,   	NoSymbol,	NoSymbol,	NoSymbol,	/*103*/
	XK_B,   	NoSymbol,	NoSymbol,	NoSymbol,	/*104*/
	XK_N,   	NoSymbol,	NoSymbol,	NoSymbol,	/*105*/
	XK_comma,	XK_question,	NoSymbol,	NoSymbol,	/*106*/
	XK_semicolon,	XK_period,	NoSymbol,	NoSymbol,	/*107*/
	XK_colon,	XK_slash,	NoSymbol,	NoSymbol,	/*108*/
	XK_equal,	XK_plus,	NoSymbol,	NoSymbol,	/*109*/
	XK_Shift_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*110*/
	XK_Linefeed,	NoSymbol,	NoSymbol,	NoSymbol,	/*111*/
	XK_KP_End, 	XK_KP_1, 	XK_R13,		NoSymbol, 	/*112*/
	XK_KP_Down,	XK_KP_2, 	XK_R14,		NoSymbol,	/*113*/
	XK_KP_Next, 	XK_KP_3, 	XK_R15,		NoSymbol,	/*114*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*115*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*116*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*117*/
	XK_Help,	NoSymbol,	NoSymbol,	NoSymbol,	/*118*/
	SunXK_AltGraph,	NoSymbol,	NoSymbol,	NoSymbol,	/*119*/
	XK_Meta_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*120*/
	XK_space,	NoSymbol,	NoSymbol,	NoSymbol,	/*121*/
	XK_Meta_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*122*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*123*/
	XK_less,	XK_greater,	NoSymbol,	NoSymbol,	/*124*/
	XK_KP_Add,	XK_KP_Add,	NoSymbol,	NoSymbol,	/*125*/
};

#define FranceBelg4Modmap FrBeNeth4Modmap

#else

#define FranceBelg4Keymap NULL
#define FranceBelg4Modmap NULL

#endif /* FRANCEBELG4 */


#ifdef GER4

static KeySym Germany4Keymap[] = {
	SunXK_Stop,  	NoSymbol,  	XK_L1,		NoSymbol,	/*  1*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  2*/
	SunXK_Again,  	NoSymbol,  	XK_L2,		NoSymbol,	/*  3*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  4*/
	XK_F1,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  5*/
	XK_F2,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  6*/
	XK_F10, 	NoSymbol,	NoSymbol,	NoSymbol,	/*  7*/
	XK_F3,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  8*/
	XK_F11,		NoSymbol,	SunXK_F36,	NoSymbol,	/*  9*/
	XK_F4,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 10*/
	XK_F12,		NoSymbol,	SunXK_F37,	NoSymbol,	/* 11*/
	XK_F5,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 12*/
	XK_Alt_R,	NoSymbol,	NoSymbol,	NoSymbol,	/* 13*/
	XK_F6,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 14*/
	XK_bracketright,XK_braceright,XK_guillemotright,NoSymbol,	/* 15*/
	XK_F7,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 16*/
	XK_F8,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 17*/
	XK_F9,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 18*/
	SunXK_AltGraph,	NoSymbol,	NoSymbol,	NoSymbol,	/* 19*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 20*/
	XK_Pause,  	NoSymbol,  	XK_R1,		NoSymbol,	/* 21*/
	XK_Print,  	NoSymbol,  	XK_R2,		NoSymbol,	/* 22*/
	XK_Scroll_Lock,	NoSymbol,  	XK_R3,		NoSymbol,	/* 23*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 24*/
	SunXK_Props,  	NoSymbol,  	XK_L3,		NoSymbol,	/* 25*/
	SunXK_Undo,  	NoSymbol,  	XK_L4,		NoSymbol,	/* 26*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 27*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 28*/
	XK_Escape,	NoSymbol,	NoSymbol,	NoSymbol,	/* 29*/
	XK_1,   	XK_exclam,	NoSymbol,	NoSymbol,	/* 30*/
	XK_2,   	XK_quotedbl,	XK_twosuperior,	NoSymbol,	/* 31*/
	XK_3,   	XK_section,	XK_threesuperior,NoSymbol,	/* 32*/
	XK_4,   	XK_dollar,	NoSymbol,	NoSymbol,	/* 33*/
	XK_5,   	XK_percent,	NoSymbol,	NoSymbol,	/* 34*/
	XK_6,   	XK_ampersand,	NoSymbol,	NoSymbol,	/* 35*/
	XK_7,   	XK_slash,	XK_degree,	NoSymbol,	/* 36*/
	XK_8,   	XK_parenleft,	XK_grave,	NoSymbol,	/* 37*/
	XK_9,   	XK_parenright,	XK_apostrophe,	NoSymbol,	/* 38*/
	XK_0,   	XK_equal,	XK_bar, 	NoSymbol,	/* 39*/
	XK_ssharp,	XK_question,	XK_backslash,	NoSymbol,	/* 40*/
	SunXK_FA_Acute,	SunXK_FA_Grave,	NoSymbol,	NoSymbol,	/* 41*/
	XK_numbersign,	XK_asciicircum,	XK_at,  	NoSymbol,	/* 42*/
	XK_BackSpace,	NoSymbol,	NoSymbol,	NoSymbol,	/* 43*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 44*/
	XK_KP_Equal,  	NoSymbol,  	XK_R4,		NoSymbol,	/* 45*/
	XK_KP_Divide,  	NoSymbol,  	XK_R5,		NoSymbol,	/* 46*/
	XK_KP_Multiply,	NoSymbol,	XK_R6,		NoSymbol,	/* 47*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 48*/
	SunXK_Front,  	NoSymbol,  	XK_L5,		NoSymbol,	/* 49*/
	XK_KP_Delete,	XK_KP_Delete,	XK_KP_Decimal,	NoSymbol,	/* 50*/
	SunXK_Copy,  	NoSymbol,  	XK_L6,		NoSymbol,	/* 51*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 52*/
	XK_Tab, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 53*/
	XK_Q,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 54*/
	XK_W,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 55*/
	XK_E,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 56*/
	XK_R,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 57*/
	XK_T,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 58*/
	XK_Z,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 59*/
	XK_U,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 60*/
	XK_I,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 61*/
	XK_O,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 62*/
	XK_P,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 63*/
	XK_Udiaeresis,	NoSymbol,	NoSymbol,	NoSymbol,	/* 64*/
	XK_plus,	XK_asterisk,	XK_asciitilde,	NoSymbol,	/* 65*/
	XK_Delete,	NoSymbol,	NoSymbol,	NoSymbol,	/* 66*/
	SunXK_Compose,	NoSymbol,	NoSymbol,	NoSymbol,	/* 67*/
	XK_KP_Home,  	XK_KP_7,  	XK_R7,		NoSymbol,	/* 68*/
	XK_KP_Up,  	XK_KP_8,  	XK_R8,		NoSymbol,	/* 69*/
	XK_KP_Prior,  	XK_KP_9,  	XK_R9,		NoSymbol,	/* 70*/
	XK_KP_Subtract,	XK_KP_Subtract,	NoSymbol,	NoSymbol,	/* 71*/
	SunXK_Open,  	NoSymbol,  	XK_L7,		NoSymbol,	/* 72*/
	SunXK_Paste,  	NoSymbol,  	XK_L8,		NoSymbol,	/* 73*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 74*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 75*/
	XK_Caps_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 76*/
	XK_A,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 77*/
	XK_S,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 78*/
	XK_D,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 79*/
	XK_F,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 80*/
	XK_G,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 81*/
	XK_H,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 82*/
	XK_J,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 83*/
	XK_K,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 84*/
	XK_L,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 85*/
	XK_Odiaeresis,	NoSymbol,	NoSymbol,	NoSymbol,	/* 86*/
	XK_Adiaeresis,	NoSymbol,	NoSymbol,	NoSymbol,	/* 87*/
	XK_bracketleft,	XK_braceleft,	XK_guillemotleft,NoSymbol,	/* 88*/
	XK_Return,	NoSymbol,	NoSymbol,	NoSymbol,	/* 89*/
	XK_KP_Enter,	NoSymbol,	NoSymbol,	NoSymbol,	/* 90*/
	XK_KP_Left,	XK_KP_4, 	XK_R10,		NoSymbol,	/* 91*/
	NoSymbol, 	XK_KP_5, 	XK_R11,		NoSymbol,	/* 92*/
	XK_KP_Right,	XK_KP_6, 	XK_R12,		NoSymbol,	/* 93*/
	XK_KP_Insert,	XK_KP_0,	NoSymbol,	NoSymbol,	/* 94*/
	SunXK_Find,  	NoSymbol,  	XK_L9,		NoSymbol,	/* 95*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 96*/
	SunXK_Cut, 	NoSymbol, 	XK_L10,		NoSymbol,	/* 97*/
	XK_Num_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 98*/
	XK_Shift_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 99*/
	XK_Y,   	NoSymbol,	NoSymbol,	NoSymbol,	/*100*/
	XK_X,   	NoSymbol,	NoSymbol,	NoSymbol,	/*101*/
	XK_C,   	NoSymbol,	NoSymbol,	NoSymbol,	/*102*/
	XK_V,   	NoSymbol,	NoSymbol,	NoSymbol,	/*103*/
	XK_B,   	NoSymbol,	NoSymbol,	NoSymbol,	/*104*/
	XK_N,   	NoSymbol,	NoSymbol,	NoSymbol,	/*105*/
	XK_M,   	NoSymbol,	XK_mu,  	NoSymbol,	/*106*/
	XK_comma,	XK_semicolon,	NoSymbol,	NoSymbol,	/*107*/
	XK_period,	XK_colon,	NoSymbol,	NoSymbol,	/*108*/
	XK_minus,	XK_underscore,	NoSymbol,	NoSymbol,	/*109*/
	XK_Shift_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*110*/
	XK_Linefeed,	NoSymbol,	NoSymbol,	NoSymbol,	/*111*/
	XK_KP_End, 	XK_KP_1, 	XK_R13,		NoSymbol, 	/*112*/
	XK_KP_Down,	XK_KP_2, 	XK_R14,		NoSymbol,	/*113*/
	XK_KP_Next, 	XK_KP_3, 	XK_R15,		NoSymbol,	/*114*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*115*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*116*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*117*/
	XK_Help,	NoSymbol,	NoSymbol,	NoSymbol,	/*118*/
	XK_Control_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*119*/
	XK_Meta_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*120*/
	XK_space,	NoSymbol,	NoSymbol,	NoSymbol,	/*121*/
	XK_Meta_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*122*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*123*/
	XK_less,	XK_greater,	NoSymbol,	NoSymbol,	/*124*/
	XK_KP_Add,	NoSymbol,	NoSymbol,	NoSymbol,	/*125*/
};

static SunModmapRec Germany4Modmap[] = {
	{  99,	ShiftMask },
	{ 110,	ShiftMask },
	{  76,	LockMask },
	{ 119,	ControlMask },
	{ 120,	Meta_Mask },
	{ 122,	Meta_Mask },
	{  19,	Mode_switch_Mask },
	{  98,	Num_Lock_Mask },
	{  13,	Alt_Mask },
	{   0,	0 }
};

#else

#define Germany4Keymap NULL
#define Germany4Modmap NULL

#endif /* GERMANY4 */


#ifdef GER5

static KeySym Germany5Keymap[] = {
	SunXK_Stop,  	NoSymbol,  	XK_L1,		NoSymbol,	/*  1*/
	SunXK_AudioLowerVolume,SunXK_VideoLowerBrightness,NoSymbol,	NoSymbol,	/*  2*/
	SunXK_Again,  	NoSymbol,  	XK_L2,		NoSymbol,	/*  3*/
	SunXK_AudioRaiseVolume,SunXK_VideoRaiseBrightness,NoSymbol,	NoSymbol,	/*  4*/
	XK_F1,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  5*/
	XK_F2,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  6*/
	XK_F10, 	NoSymbol,	NoSymbol,	NoSymbol,	/*  7*/
	XK_F3,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  8*/
	XK_F11,		NoSymbol,	SunXK_F36,	NoSymbol,	/*  9*/
	XK_F4,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 10*/
	XK_F12,		NoSymbol,	SunXK_F37,	NoSymbol,	/* 11*/
	XK_F5,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 12*/
	SunXK_AltGraph,	NoSymbol,	NoSymbol,	NoSymbol,	/* 13*/
	XK_F6,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 14*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 15*/
	XK_F7,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 16*/
	XK_F8,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 17*/
	XK_F9,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 18*/
	XK_Alt_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 19*/
	XK_Up,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 20*/
	XK_Pause,  	NoSymbol,  	XK_Break,	NoSymbol,	/* 21*/
	XK_Print,  	NoSymbol,  	XK_Sys_Req,	SunXK_Sys_Req,	/* 22*/
	XK_Scroll_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 23*/
	XK_Left,	NoSymbol,	NoSymbol,	NoSymbol,	/* 24*/
	SunXK_Props,  	NoSymbol,  	XK_L3,		NoSymbol,	/* 25*/
	SunXK_Undo,  	NoSymbol,  	XK_L4,		NoSymbol,	/* 26*/
	XK_Down,	NoSymbol,	NoSymbol,	NoSymbol,	/* 27*/
	XK_Right,	NoSymbol,	NoSymbol,	NoSymbol,	/* 28*/
	XK_Escape,	NoSymbol,	NoSymbol,	NoSymbol,	/* 29*/
	XK_1,   	XK_exclam,	NoSymbol,	NoSymbol,	/* 30*/
	XK_2,   	XK_quotedbl,	XK_twosuperior,	NoSymbol,	/* 31*/
	XK_3,   	XK_section,	XK_threesuperior,NoSymbol,	/* 32*/
	XK_4,   	XK_dollar,	NoSymbol,	NoSymbol,	/* 33*/
	XK_5,   	XK_percent,	NoSymbol,	NoSymbol,	/* 34*/
	XK_6,   	XK_ampersand,	NoSymbol,	NoSymbol,	/* 35*/
	XK_7,   	XK_slash,	XK_braceleft,	NoSymbol,	/* 36*/
	XK_8,   	XK_parenleft,	XK_bracketleft,	NoSymbol,	/* 37*/
	XK_9,   	XK_parenright,	XK_bracketright,NoSymbol,	/* 38*/
	XK_0,   	XK_equal,	XK_braceright,	NoSymbol,	/* 39*/
	XK_ssharp,	XK_question,	XK_backslash,	NoSymbol,	/* 40*/
	SunXK_FA_Acute,	SunXK_FA_Grave,	NoSymbol,	NoSymbol,	/* 41*/
	XK_asciicircum,	XK_degree,	NoSymbol,	NoSymbol,	/* 42*/
	XK_BackSpace,	NoSymbol,	NoSymbol,	NoSymbol,	/* 43*/
	XK_Insert,	NoSymbol,	NoSymbol,	NoSymbol,	/* 44*/
	SunXK_AudioMute,SunXK_VideoDegauss,NoSymbol,	NoSymbol,	/* 45*/
	XK_KP_Divide,  	NoSymbol,  	NoSymbol,	NoSymbol,	/* 46*/
	XK_KP_Multiply,	NoSymbol,	NoSymbol,	NoSymbol,	/* 47*/
	SunXK_PowerSwitch,SunXK_PowerSwitchShift,	NoSymbol,	NoSymbol,	/* 48*/
	SunXK_Front,  	NoSymbol,  	XK_L5,		NoSymbol,	/* 49*/
	XK_KP_Delete,	XK_KP_Separator,NoSymbol,	NoSymbol,	/* 50*/
	SunXK_Copy,  	NoSymbol,  	XK_L6,		NoSymbol,	/* 51*/
	XK_Home,	NoSymbol,	NoSymbol,	NoSymbol,	/* 52*/
	XK_Tab, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 53*/
	XK_q,   	XK_Q,   	XK_at,  	NoSymbol,	/* 54*/
	XK_W,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 55*/
	XK_E,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 56*/
	XK_R,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 57*/
	XK_T,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 58*/
	XK_Z,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 59*/
	XK_U,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 60*/
	XK_I,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 61*/
	XK_O,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 62*/
	XK_P,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 63*/
	XK_Udiaeresis,	NoSymbol,	NoSymbol,	NoSymbol,	/* 64*/
	XK_plus,	XK_asterisk,	XK_asciitilde,	NoSymbol,	/* 65*/
	XK_Delete,	NoSymbol,	NoSymbol,	NoSymbol,	/* 66*/
	SunXK_Compose,	NoSymbol,	NoSymbol,	NoSymbol,	/* 67*/
	XK_KP_Home,  	XK_KP_7,  	NoSymbol,	NoSymbol,	/* 68*/
	XK_KP_Up,  	XK_KP_8,  	NoSymbol,	NoSymbol,	/* 69*/
	XK_KP_Prior,  	XK_KP_9,  	NoSymbol,	NoSymbol,	/* 70*/
	XK_KP_Subtract,	NoSymbol,	NoSymbol,	NoSymbol,	/* 71*/
	SunXK_Open,  	NoSymbol,  	XK_L7,		NoSymbol,	/* 72*/
	SunXK_Paste,  	NoSymbol,  	XK_L8,		NoSymbol,	/* 73*/
	XK_End, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 74*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 75*/
	XK_Control_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 76*/
	XK_A,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 77*/
	XK_S,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 78*/
	XK_D,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 79*/
	XK_F,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 80*/
	XK_G,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 81*/
	XK_H,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 82*/
	XK_J,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 83*/
	XK_K,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 84*/
	XK_L,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 85*/
	XK_Odiaeresis,	NoSymbol,	NoSymbol,	NoSymbol,	/* 86*/
	XK_Adiaeresis,	NoSymbol,	NoSymbol,	NoSymbol,	/* 87*/
	XK_numbersign,	XK_apostrophe,	XK_grave,	NoSymbol,	/* 88*/
	XK_Return,	NoSymbol,	NoSymbol,	NoSymbol,	/* 89*/
	XK_KP_Enter,	NoSymbol,	NoSymbol,	NoSymbol,	/* 90*/
	XK_KP_Left,	XK_KP_4, 	NoSymbol,	NoSymbol,	/* 91*/
	NoSymbol, 	XK_KP_5, 	NoSymbol,	NoSymbol,	/* 92*/
	XK_KP_Right,	XK_KP_6, 	NoSymbol,	NoSymbol,	/* 93*/
	XK_KP_Insert,	XK_KP_0,	NoSymbol,	NoSymbol,	/* 94*/
	SunXK_Find,  	NoSymbol,  	XK_L9,		NoSymbol,	/* 95*/
	XK_Prior,	NoSymbol,	NoSymbol,	NoSymbol,	/* 96*/
	SunXK_Cut, 	NoSymbol, 	XK_L10,		NoSymbol,	/* 97*/
	XK_Num_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 98*/
	XK_Shift_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 99*/
	XK_Y,   	NoSymbol,	NoSymbol,	NoSymbol,	/*100*/
	XK_X,   	NoSymbol,	NoSymbol,	NoSymbol,	/*101*/
	XK_C,   	NoSymbol,	NoSymbol,	NoSymbol,	/*102*/
	XK_V,   	NoSymbol,	NoSymbol,	NoSymbol,	/*103*/
	XK_B,   	NoSymbol,	NoSymbol,	NoSymbol,	/*104*/
	XK_N,   	NoSymbol,	NoSymbol,	NoSymbol,	/*105*/
	XK_m,   	XK_M,   	XK_mu,  	NoSymbol,	/*106*/
	XK_comma,	XK_semicolon,	NoSymbol,	NoSymbol,	/*107*/
	XK_period,	XK_colon,	NoSymbol,	NoSymbol,	/*108*/
	XK_minus,	XK_underscore,	NoSymbol,	NoSymbol,	/*109*/
	XK_Shift_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*110*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*111*/
	XK_KP_End, 	XK_KP_1, 	NoSymbol,	NoSymbol, 	/*112*/
	XK_KP_Down,	XK_KP_2, 	NoSymbol,	NoSymbol,	/*113*/
	XK_KP_Next, 	XK_KP_3, 	NoSymbol,	NoSymbol,	/*114*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*115*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*116*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*117*/
	XK_Help,	NoSymbol,	NoSymbol,	NoSymbol,	/*118*/
	XK_Caps_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/*119*/
	XK_Meta_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*120*/
	XK_space,	NoSymbol,	NoSymbol,	NoSymbol,	/*121*/
	XK_Meta_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*122*/
	XK_Next,	NoSymbol,	NoSymbol,	NoSymbol,	/*123*/
	XK_less,	XK_greater,	XK_bar, 	NoSymbol,	/*124*/
	XK_KP_Add,	XK_KP_Add,	NoSymbol,	NoSymbol,	/*125*/
};

#define Germany5Modmap Generic5Modmap

#else

#define Germany5Keymap NULL
#define Germany5Modmap NULL

#endif /* GERMANY5 */


#ifdef ITALY4

static KeySym Italy4Keymap[] = {
	SunXK_Stop,  	NoSymbol,  	XK_L1,		NoSymbol,	/*  1*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  2*/
	SunXK_Again,  	NoSymbol,  	XK_L2,		NoSymbol,	/*  3*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  4*/
	XK_F1,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  5*/
	XK_F2,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  6*/
	XK_F10, 	NoSymbol,	NoSymbol,	NoSymbol,	/*  7*/
	XK_F3,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  8*/
	XK_F11,		NoSymbol,	SunXK_F36,	NoSymbol,	/*  9*/
	XK_F4,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 10*/
	XK_F12,		NoSymbol,	SunXK_F37,	NoSymbol,	/* 11*/
	XK_F5,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 12*/
	SunXK_AltGraph,	NoSymbol,	NoSymbol,	NoSymbol,	/* 13*/
	XK_F6,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 14*/
	XK_bracketright,XK_braceright,	XK_guillemotright,NoSymbol,	/* 15*/
	XK_F7,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 16*/
	XK_F8,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 17*/
	XK_F9,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 18*/
	XK_Alt_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 19*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 20*/
	XK_Pause,  	NoSymbol,  	XK_R1,		NoSymbol,	/* 21*/
	XK_Print,  	NoSymbol,  	XK_R2,		NoSymbol,	/* 22*/
	XK_Break, 	XK_Scroll_Lock,	XK_R3,		NoSymbol,	/* 23*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 24*/
	SunXK_Props,  	NoSymbol,  	XK_L3,		NoSymbol,	/* 25*/
	SunXK_Undo,  	NoSymbol,  	XK_L4,		NoSymbol,	/* 26*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 27*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 28*/
	XK_Escape,	NoSymbol,	NoSymbol,	NoSymbol,	/* 29*/
	XK_1,   	XK_exclam,	NoSymbol,	NoSymbol,	/* 30*/
	XK_2,   	XK_quotedbl,	XK_twosuperior,	NoSymbol,	/* 31*/
	XK_3,   	XK_sterling,	XK_threesuperior,NoSymbol,	/* 32*/
	XK_4,   	XK_dollar,	NoSymbol,	NoSymbol,	/* 33*/
	XK_5,   	XK_percent,	NoSymbol,	NoSymbol,	/* 34*/
	XK_6,   	XK_ampersand,	XK_notsign,	NoSymbol,	/* 35*/
	XK_7,   	XK_slash,	NoSymbol,	NoSymbol,	/* 36*/
	XK_8,   	XK_parenleft,	NoSymbol,	NoSymbol,	/* 37*/
	XK_9,   	XK_parenright,	XK_backslash,	NoSymbol,	/* 38*/
	XK_0,   	XK_equal,	XK_bar, 	NoSymbol,	/* 39*/
	XK_apostrophe,	XK_question,	XK_grave,	NoSymbol,	/* 40*/
	XK_igrave,	XK_asciicircum,	NoSymbol,	NoSymbol,	/* 41*/
	XK_ugrave,	XK_section,	NoSymbol,	NoSymbol,	/* 42*/
	XK_BackSpace,	NoSymbol,	NoSymbol,	NoSymbol,	/* 43*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 44*/
	XK_KP_Equal,  	NoSymbol,  	XK_R4,		NoSymbol,	/* 45*/
	XK_KP_Divide,  	NoSymbol,  	XK_R5,		NoSymbol,	/* 46*/
	XK_KP_Multiply,	NoSymbol,	XK_R6,		NoSymbol,	/* 47*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 48*/
	SunXK_Front,  	NoSymbol,  	XK_L5,		NoSymbol,	/* 49*/
	XK_KP_Delete,	XK_KP_Decimal,	NoSymbol,	NoSymbol,	/* 50*/
	SunXK_Copy,  	NoSymbol,  	XK_L6,		NoSymbol,	/* 51*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 52*/
	XK_Tab, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 53*/
	XK_Q,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 54*/
	XK_W,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 55*/
	XK_E,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 56*/
	XK_R,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 57*/
	XK_T,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 58*/
	XK_Y,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 59*/
	XK_U,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 60*/
	XK_I,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 61*/
	XK_O,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 62*/
	XK_P,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 63*/
	XK_egrave,	XK_eacute,	NoSymbol,	NoSymbol,	/* 64*/
	XK_plus,	XK_asterisk,	XK_asciitilde,	NoSymbol,	/* 65*/
	XK_Delete,	NoSymbol,	NoSymbol,	NoSymbol,	/* 66*/
	SunXK_Compose,	NoSymbol,	NoSymbol,	NoSymbol,	/* 67*/
	XK_KP_Home,  	XK_KP_7,  	XK_R7,		NoSymbol,	/* 68*/
	XK_KP_Up,  	XK_KP_8,  	XK_R8,		NoSymbol,	/* 69*/
	XK_KP_Prior,  	XK_KP_9,  	XK_R9,		NoSymbol,	/* 70*/
	XK_KP_Subtract,	NoSymbol,	NoSymbol,	NoSymbol,	/* 71*/
	SunXK_Open,  	NoSymbol,  	XK_L7,		NoSymbol,	/* 72*/
	SunXK_Paste,  	NoSymbol,  	XK_L8,		NoSymbol,	/* 73*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 74*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 75*/
	XK_Caps_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 76*/
	XK_A,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 77*/
	XK_S,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 78*/
	XK_D,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 79*/
	XK_F,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 80*/
	XK_G,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 81*/
	XK_H,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 82*/
	XK_J,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 83*/
	XK_K,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 84*/
	XK_L,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 85*/
	XK_ograve,	XK_ccedilla,	XK_at,  	NoSymbol,	/* 86*/
	XK_agrave,	XK_degree,	XK_numbersign,	NoSymbol,	/* 87*/
	XK_bracketleft,	XK_braceleft,	XK_guillemotleft,NoSymbol,	/* 88*/
	XK_Return,	NoSymbol,	NoSymbol,	NoSymbol,	/* 89*/
	XK_KP_Enter,	NoSymbol,	NoSymbol,	NoSymbol,	/* 90*/
	XK_KP_Left,	XK_KP_4, 	XK_R4,		NoSymbol,	/* 91*/
	NoSymbol, 	XK_KP_5, 	XK_R5,		NoSymbol,	/* 92*/
	XK_KP_Right,	XK_KP_6, 	XK_R6,		NoSymbol,	/* 93*/
	XK_KP_Insert,	XK_KP_0,	NoSymbol,	NoSymbol,	/* 94*/
	SunXK_Find,  	NoSymbol,  	XK_L9,		NoSymbol,	/* 95*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 96*/
	SunXK_Cut, 	NoSymbol, 	XK_L10,		NoSymbol,	/* 97*/
	XK_Num_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 98*/
	XK_Shift_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 99*/
	XK_Z,   	NoSymbol,	NoSymbol,	NoSymbol,	/*100*/
	XK_X,   	NoSymbol,	NoSymbol,	NoSymbol,	/*101*/
	XK_C,   	NoSymbol,	NoSymbol,	NoSymbol,	/*102*/
	XK_V,   	NoSymbol,	NoSymbol,	NoSymbol,	/*103*/
	XK_B,   	NoSymbol,	NoSymbol,	NoSymbol,	/*104*/
	XK_N,   	NoSymbol,	NoSymbol,	NoSymbol,	/*105*/
	XK_M,   	NoSymbol,	NoSymbol,	NoSymbol,	/*106*/
	XK_comma,	XK_semicolon,	NoSymbol,	NoSymbol,	/*107*/
	XK_period,	XK_colon,	NoSymbol,	NoSymbol,	/*108*/
	XK_minus,	XK_underscore,	NoSymbol,	NoSymbol,	/*109*/
	XK_Shift_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*110*/
	XK_Linefeed,	NoSymbol,	NoSymbol,	NoSymbol,	/*111*/
	XK_KP_End, 	XK_KP_1, 	XK_R13,		NoSymbol, 	/*112*/
	XK_KP_Down,	XK_KP_2, 	XK_R14,		NoSymbol,	/*113*/
	XK_KP_Next, 	XK_KP_3, 	XK_R15,		NoSymbol,	/*114*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*115*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*116*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*117*/
	XK_Help,	NoSymbol,	NoSymbol,	NoSymbol,	/*118*/
	XK_Control_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*119*/
	XK_Meta_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*120*/
	XK_space,	NoSymbol,	NoSymbol,	NoSymbol,	/*121*/
	XK_Meta_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*122*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*123*/
	XK_less,	XK_greater,	NoSymbol,	NoSymbol,	/*124*/
	XK_KP_Add,	NoSymbol,	NoSymbol,	NoSymbol,	/*125*/
};

#define Italy4Modmap ItNorPortSp4Modmap

#else

#define Italy4Keymap NULL
#define Italy4Modmap NULL

#endif /* ITALY4 */


#ifdef ITALY5

static KeySym Italy5Keymap[] = {
	SunXK_Stop,  	NoSymbol,  	XK_L1,		NoSymbol,	/*  1*/
	SunXK_AudioLowerVolume,SunXK_VideoLowerBrightness,NoSymbol,	NoSymbol,	/*  2*/
	SunXK_Again,  	NoSymbol,  	XK_L2,		NoSymbol,	/*  3*/
	SunXK_AudioRaiseVolume,SunXK_VideoRaiseBrightness,NoSymbol,	NoSymbol,	/*  4*/
	XK_F1,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  5*/
	XK_F2,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  6*/
	XK_F10, 	NoSymbol,	NoSymbol,	NoSymbol,	/*  7*/
	XK_F3,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  8*/
	XK_F11,		NoSymbol,	SunXK_F36,	NoSymbol,	/*  9*/
	XK_F4,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 10*/
	XK_F12,		NoSymbol,	SunXK_F37,	NoSymbol,	/* 11*/
	XK_F5,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 12*/
	SunXK_AltGraph,	NoSymbol,	NoSymbol,	NoSymbol,	/* 13*/
	XK_F6,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 14*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 15*/
	XK_F7,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 16*/
	XK_F8,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 17*/
	XK_F9,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 18*/
	XK_Alt_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 19*/
	XK_Up,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 20*/
	XK_Pause,  	XK_Break,  	NoSymbol,	NoSymbol,	/* 21*/
	XK_Print,  	NoSymbol,  	XK_Sys_Req,	SunXK_Sys_Req,	/* 22*/
	XK_Scroll_Lock,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 23*/
	XK_Left,	NoSymbol,	NoSymbol,	NoSymbol,	/* 24*/
	SunXK_Props,  	NoSymbol,  	XK_L3,		NoSymbol,	/* 25*/
	SunXK_Undo,  	NoSymbol,  	XK_L4,		NoSymbol,	/* 26*/
	XK_Down,	NoSymbol,	NoSymbol,	NoSymbol,	/* 27*/
	XK_Right,	NoSymbol,	NoSymbol,	NoSymbol,	/* 28*/
	XK_Escape,	NoSymbol,	NoSymbol,	NoSymbol,	/* 29*/
	XK_1,   	XK_exclam,	NoSymbol,	NoSymbol,	/* 30*/
	XK_2,   	XK_quotedbl,	NoSymbol,	NoSymbol,	/* 31*/
	XK_3,   	XK_sterling,	NoSymbol,	NoSymbol,	/* 32*/
	XK_4,   	XK_dollar,	NoSymbol,	NoSymbol,	/* 33*/
	XK_5,   	XK_percent,	NoSymbol,	NoSymbol,	/* 34*/
	XK_6,   	XK_ampersand,	NoSymbol,	NoSymbol,	/* 35*/
	XK_7,   	XK_slash,	NoSymbol,	NoSymbol,	/* 36*/
	XK_8,   	XK_parenleft,	XK_braceleft,	NoSymbol,	/* 37*/
	XK_9,   	XK_parenright,	XK_braceright,	NoSymbol,	/* 38*/
	XK_0,   	XK_equal,	NoSymbol,	NoSymbol,	/* 39*/
	XK_apostrophe,	XK_question,	XK_grave,	NoSymbol,	/* 40*/
	XK_igrave,	XK_asciicircum,	NoSymbol,	NoSymbol,	/* 41*/
	XK_backslash,	XK_bar, 	NoSymbol,	NoSymbol,	/* 42*/
	XK_BackSpace,	NoSymbol,	NoSymbol,	NoSymbol,	/* 43*/
	XK_Insert,	NoSymbol,	NoSymbol,	NoSymbol,	/* 44*/
	SunXK_AudioMute,SunXK_VideoDegauss,NoSymbol,	NoSymbol,	/* 45*/
	XK_KP_Divide,  	XK_KP_Divide,  	NoSymbol,	NoSymbol,	/* 46*/
	XK_KP_Multiply,	XK_KP_Multiply,	NoSymbol,	NoSymbol,	/* 47*/
	SunXK_PowerSwitch,SunXK_PowerSwitchShift,	NoSymbol,	NoSymbol,	/* 48*/
	SunXK_Front,  	NoSymbol,  	XK_L5,		NoSymbol,	/* 49*/
	XK_KP_Delete,	XK_KP_Decimal,	NoSymbol,	NoSymbol,	/* 50*/
	SunXK_Copy,  	NoSymbol,  	XK_L6,		NoSymbol,	/* 51*/
	XK_Home,	NoSymbol,	NoSymbol,	NoSymbol,	/* 52*/
	XK_Tab, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 53*/
	XK_Q,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 54*/
	XK_W,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 55*/
	XK_E,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 56*/
	XK_R,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 57*/
	XK_T,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 58*/
	XK_Y,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 59*/
	XK_U,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 60*/
	XK_I,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 61*/
	XK_O,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 62*/
	XK_P,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 63*/
	XK_egrave,	XK_eacute,	XK_bracketleft,	NoSymbol,	/* 64*/
	XK_plus,	XK_asterisk,	XK_bracketright,NoSymbol,	/* 65*/
	XK_Delete,	NoSymbol,	NoSymbol,	NoSymbol,	/* 66*/
	SunXK_Compose,	NoSymbol,	NoSymbol,	NoSymbol,	/* 67*/
	XK_KP_Home,  	XK_KP_7,  	NoSymbol,	NoSymbol,	/* 68*/
	XK_KP_Up,  	XK_KP_8,  	NoSymbol,	NoSymbol,	/* 69*/
	XK_KP_Prior,  	XK_KP_9,  	NoSymbol,	NoSymbol,	/* 70*/
	XK_KP_Subtract,	NoSymbol,	NoSymbol,	NoSymbol,	/* 71*/
	SunXK_Open,  	NoSymbol,  	XK_L7,		NoSymbol,	/* 72*/
	SunXK_Paste,  	NoSymbol,  	XK_L8,		NoSymbol,	/* 73*/
	XK_End, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 74*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 75*/
	XK_Control_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 76*/
	XK_A,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 77*/
	XK_S,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 78*/
	XK_D,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 79*/
	XK_F,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 80*/
	XK_G,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 81*/
	XK_H,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 82*/
	XK_J,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 83*/
	XK_K,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 84*/
	XK_L,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 85*/
	XK_ograve,	XK_ccedilla,	XK_at,  	NoSymbol,	/* 86*/
	XK_agrave,	XK_degree,	XK_numbersign,	NoSymbol,	/* 87*/
	XK_ugrave,	XK_section,	XK_asciitilde,	NoSymbol,	/* 88*/
	XK_Return,	NoSymbol,	NoSymbol,	NoSymbol,	/* 89*/
	XK_KP_Enter,	XK_KP_Enter,	NoSymbol,	NoSymbol,	/* 90*/
	XK_KP_Left,	XK_KP_4, 	NoSymbol,	NoSymbol,	/* 91*/
	NoSymbol, 	XK_KP_5, 	NoSymbol,	NoSymbol,	/* 92*/
	XK_KP_Right,	XK_KP_6, 	NoSymbol,	NoSymbol,	/* 93*/
	XK_KP_Insert,	XK_KP_0,	NoSymbol,	NoSymbol,	/* 94*/
	SunXK_Find,  	NoSymbol,  	XK_L9,		NoSymbol,	/* 95*/
	XK_Prior,	NoSymbol,	NoSymbol,	NoSymbol,	/* 96*/
	SunXK_Cut, 	NoSymbol, 	XK_L10,		NoSymbol,	/* 97*/
	XK_Num_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 98*/
	XK_Shift_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 99*/
	XK_Z,   	NoSymbol,	NoSymbol,	NoSymbol,	/*100*/
	XK_X,   	NoSymbol,	NoSymbol,	NoSymbol,	/*101*/
	XK_C,   	NoSymbol,	NoSymbol,	NoSymbol,	/*102*/
	XK_V,   	NoSymbol,	NoSymbol,	NoSymbol,	/*103*/
	XK_B,   	NoSymbol,	NoSymbol,	NoSymbol,	/*104*/
	XK_N,   	NoSymbol,	NoSymbol,	NoSymbol,	/*105*/
	XK_M,   	NoSymbol,	NoSymbol,	NoSymbol,	/*106*/
	XK_comma,	XK_semicolon,	NoSymbol,	NoSymbol,	/*107*/
	XK_period,	XK_colon,	NoSymbol,	NoSymbol,	/*108*/
	XK_minus,	XK_underscore,	NoSymbol,	NoSymbol,	/*109*/
	XK_Shift_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*110*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*111*/
	XK_KP_End, 	XK_KP_1, 	NoSymbol,	NoSymbol, 	/*112*/
	XK_KP_Down,	XK_KP_2, 	NoSymbol,	NoSymbol,	/*113*/
	XK_KP_Next, 	XK_KP_3, 	NoSymbol,	NoSymbol,	/*114*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*115*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*116*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*117*/
	XK_Help,	NoSymbol,	NoSymbol,	NoSymbol,	/*118*/
	XK_Caps_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/*119*/
	XK_Meta_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*120*/
	XK_space,	NoSymbol,	NoSymbol,	NoSymbol,	/*121*/
	XK_Meta_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*122*/
	XK_Next,	NoSymbol,	NoSymbol,	NoSymbol,	/*123*/
	XK_less,	XK_greater,	NoSymbol,	NoSymbol,	/*124*/
	XK_KP_Add,	XK_KP_Add,	NoSymbol,	NoSymbol,	/*125*/
};

#define Italy5Modmap Generic5Modmap

#else

#define Italy5Keymap NULL
#define Italy5Modmap NULL

#endif /* ITALY5 */


#ifdef JAPAN4

static KeySym Japan4Keymap[] = {
	SunXK_Stop,  	NoSymbol,  	XK_L1,		NoSymbol,	/*  1*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  2*/
	SunXK_Again,  	NoSymbol,  	XK_L2,		NoSymbol,	/*  3*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  4*/
	XK_F1,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  5*/
	XK_F2,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  6*/
	XK_F10, 	NoSymbol,	NoSymbol,	NoSymbol,	/*  7*/
	XK_F3,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  8*/
	XK_F11,		NoSymbol,	SunXK_F36,	NoSymbol,	/*  9*/
	XK_F4,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 10*/
	XK_F12,		NoSymbol,	SunXK_F37,	NoSymbol,	/* 11*/
	XK_F5,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 12*/
	SunXK_AltGraph,	NoSymbol,	NoSymbol,	NoSymbol,	/* 13*/
	XK_F6,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 14*/
	XK_Linefeed,	NoSymbol,	NoSymbol,	NoSymbol,	/* 15*/
	XK_F7,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 16*/
	XK_F8,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 17*/
	XK_F9,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 18*/
	XK_Alt_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 19*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 20*/
	XK_Pause,  	NoSymbol,  	XK_R1,		NoSymbol,	/* 21*/
	XK_Print,  	NoSymbol,  	XK_R2,		NoSymbol,	/* 22*/
	XK_Break,  	XK_Scroll_Lock,	XK_R3,		NoSymbol,	/* 23*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 24*/
	SunXK_Props,  	NoSymbol,  	XK_L3,		NoSymbol,	/* 25*/
	SunXK_Undo,  	NoSymbol,  	XK_L4,		NoSymbol,	/* 26*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 27*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 28*/
	XK_Escape,	NoSymbol,	NoSymbol,	NoSymbol,	/* 29*/
	XK_1,   	XK_exclam,	XK_kana_NU,	NoSymbol,	/* 30*/
	XK_2,   	XK_quotedbl,	XK_kana_FU,	NoSymbol,	/* 31*/
	XK_3,   	XK_numbersign,	XK_kana_A,	XK_kana_a,	/* 32*/
	XK_4,   	XK_dollar,	XK_kana_U,	XK_kana_u,	/* 33*/
	XK_5,   	XK_percent,	XK_kana_E,	XK_kana_e,	/* 34*/
	XK_6,   	XK_ampersand,	XK_kana_O,	XK_kana_o,	/* 35*/
	XK_7,   	XK_apostrophe,	XK_kana_YA,	XK_kana_ya,	/* 36*/
	XK_8,   	XK_parenleft,	XK_kana_YU,	XK_kana_yu,	/* 37*/
	XK_9,   	XK_parenright,	XK_kana_YO,	XK_kana_yo,	/* 38*/
	XK_0,   	XK_0,   	XK_kana_WA,	XK_kana_WO,	/* 39*/
	XK_minus,	XK_equal,	XK_kana_HO,	NoSymbol,	/* 40*/
	XK_asciicircum,	XK_asciitilde,	XK_kana_HE,	NoSymbol,	/* 41*/
	XK_bracketright,XK_braceright,	XK_kana_MU,	XK_kana_closingbracket,	/* 42*/
	XK_BackSpace,	NoSymbol,	NoSymbol,	NoSymbol,	/* 43*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 44*/
	XK_KP_Equal,  	NoSymbol,  	XK_R4,		NoSymbol,	/* 45*/
	XK_KP_Divide,  	NoSymbol,  	XK_R5,		NoSymbol,	/* 46*/
	XK_KP_Multiply,	NoSymbol,	XK_R6,		NoSymbol,	/* 47*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 48*/
	SunXK_Front,  	NoSymbol,  	XK_L5,		NoSymbol,	/* 49*/
	XK_KP_Delete,	XK_KP_Decimal,	NoSymbol,	NoSymbol,	/* 50*/
	SunXK_Copy,  	NoSymbol,  	XK_L6,		NoSymbol,	/* 51*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 52*/
	XK_Tab, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 53*/
	XK_q,   	XK_Q,   	XK_kana_TA,	NoSymbol,	/* 54*/
	XK_w,   	XK_W,   	XK_kana_TE,	NoSymbol,	/* 55*/
	XK_e,   	XK_E,   	XK_kana_I,	XK_kana_i,	/* 56*/
	XK_r,   	XK_R,   	XK_kana_SU,	NoSymbol,	/* 57*/
	XK_t,   	XK_T,   	XK_kana_KA,	NoSymbol,	/* 58*/
	XK_y,   	XK_Y,   	XK_kana_N,	NoSymbol,	/* 59*/
	XK_u,   	XK_U,   	XK_kana_NA,	NoSymbol,	/* 60*/
	XK_i,   	XK_I,   	XK_kana_NI,	NoSymbol,	/* 61*/
	XK_o,   	XK_O,   	XK_kana_RA,	NoSymbol,	/* 62*/
	XK_p,   	XK_P,   	XK_kana_SE,	NoSymbol,	/* 63*/
	XK_at,  	XK_grave,	XK_voicedsound,	NoSymbol,	/* 64*/
	XK_bracketleft,	XK_braceleft,	XK_semivoicedsound,XK_kana_openingbracket,	/* 65*/
	XK_Delete,	NoSymbol,	NoSymbol,	NoSymbol,	/* 66*/
	SunXK_Compose,	NoSymbol,	NoSymbol,	NoSymbol,	/* 67*/
	XK_KP_Home,  	XK_KP_7,  	XK_R7,		NoSymbol,	/* 68*/
	XK_KP_Up,  	XK_KP_8,  	XK_R8,		NoSymbol,	/* 69*/
	XK_KP_Prior,  	XK_KP_9,  	XK_R9,		NoSymbol,	/* 70*/
	XK_KP_Subtract,	XK_KP_Subtract,	NoSymbol,	NoSymbol,	/* 71*/
	SunXK_Open,  	NoSymbol,  	XK_L7,		NoSymbol,	/* 72*/
	SunXK_Paste,  	NoSymbol,  	XK_L8,		NoSymbol,	/* 73*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 74*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 75*/
	XK_Control_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 76*/
	XK_a,   	XK_A,   	XK_kana_CHI,	NoSymbol,	/* 77*/
	XK_s,   	XK_S,   	XK_kana_TO,	NoSymbol,	/* 78*/
	XK_d,   	XK_D,   	XK_kana_SHI,	NoSymbol,	/* 79*/
	XK_f,   	XK_F,   	XK_kana_HA,	NoSymbol,	/* 80*/
	XK_g,   	XK_G,   	XK_kana_KI,	NoSymbol,	/* 81*/
	XK_h,   	XK_H,   	XK_kana_KU,	NoSymbol,	/* 82*/
	XK_j,   	XK_J,   	XK_kana_MA,	NoSymbol,	/* 83*/
	XK_k,   	XK_K,   	XK_kana_NO,	NoSymbol,	/* 84*/
	XK_l,   	XK_L,   	XK_kana_RI,	NoSymbol,	/* 85*/
	XK_semicolon,	XK_plus,	XK_kana_RE,	NoSymbol,	/* 86*/
	XK_colon,	XK_asterisk,	XK_kana_KE,	NoSymbol,	/* 87*/
	XK_backslash,	XK_bar, 	XK_prolongedsound,NoSymbol,	/* 88*/
	XK_Return,	NoSymbol,	NoSymbol,	NoSymbol,	/* 89*/
	XK_KP_Enter,	NoSymbol,	NoSymbol,	NoSymbol,	/* 90*/
	XK_KP_Left,	XK_KP_4, 	XK_R10,		NoSymbol,	/* 91*/
	NoSymbol, 	XK_KP_5, 	XK_R11,		NoSymbol,	/* 92*/
	XK_KP_Right,	XK_KP_6, 	XK_R12,		NoSymbol,	/* 93*/
	XK_KP_Insert,	XK_KP_0,	NoSymbol,	NoSymbol,	/* 94*/
	SunXK_Find,  	NoSymbol,  	XK_L9,		NoSymbol,	/* 95*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 96*/
	SunXK_Cut, 	NoSymbol, 	XK_L10,		NoSymbol,	/* 97*/
	XK_Num_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 98*/
	XK_Shift_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 99*/
	XK_z,   	XK_Z,   	XK_kana_TSU,	XK_kana_tsu,	/*100*/
	XK_x,   	XK_X,   	XK_kana_SA,	NoSymbol,	/*101*/
	XK_c,   	XK_C,   	XK_kana_SO,	NoSymbol,	/*102*/
	XK_v,   	XK_V,   	XK_kana_HI,	NoSymbol,	/*103*/
	XK_b,   	XK_B,   	XK_kana_KO,	NoSymbol,	/*104*/
	XK_n,   	XK_N,   	XK_kana_MI,	NoSymbol,	/*105*/
	XK_m,   	XK_M,   	XK_kana_MO,	NoSymbol,	/*106*/
	XK_comma,	XK_less,	XK_kana_NE,	XK_kana_comma,	/*107*/
	XK_period,	XK_greater,	XK_kana_RU,	XK_kana_fullstop,	/*108*/
	XK_slash,	XK_question,	XK_kana_ME,	XK_kana_conjunctive,	/*109*/
	XK_Shift_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*110*/
	XK_backslash,	XK_underscore,	XK_kana_RO,	NoSymbol,	/*111*/
	XK_KP_End, 	XK_KP_1, 	XK_R13,		NoSymbol, 	/*112*/
	XK_KP_Down,	XK_KP_2, 	XK_R14,		NoSymbol,	/*113*/
	XK_KP_Next, 	XK_KP_3, 	XK_R15,		NoSymbol,	/*114*/
	XK_Execute,	NoSymbol,	NoSymbol,	NoSymbol,	/*115*/
	XK_Kanji,	NoSymbol,	NoSymbol,	NoSymbol,	/*116*/
	XK_Henkan_Mode,	NoSymbol,	NoSymbol,	NoSymbol,	/*117*/
	XK_Help,	NoSymbol,	NoSymbol,	NoSymbol,	/*118*/
	XK_Caps_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/*119*/
	XK_Meta_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*120*/
	XK_space,	NoSymbol,	NoSymbol,	NoSymbol,	/*121*/
	XK_Meta_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*122*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*123*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*124*/
	XK_KP_Add,	XK_KP_Add,	NoSymbol,	NoSymbol,	/*125*/
};

#define Japan4Modmap Generic5Modmap

#else

#define Japan4Keymap NULL
#define Japan4Modmap NULL

#endif /* JAPAN4 */


#ifdef JAPAN5

static KeySym Japan5Keymap[] = {
	SunXK_Stop,  	NoSymbol,  	XK_L1,		NoSymbol,	/*  1*/
	SunXK_AudioLowerVolume,SunXK_VideoLowerBrightness,NoSymbol,	NoSymbol,	/*  2*/
	SunXK_Again,  	NoSymbol,  	XK_L2,		NoSymbol,	/*  3*/
	SunXK_AudioRaiseVolume,SunXK_VideoRaiseBrightness,NoSymbol,	NoSymbol,	/*  4*/
	XK_F1,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  5*/
	XK_F2,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  6*/
	XK_F10, 	NoSymbol,	NoSymbol,	NoSymbol,	/*  7*/
	XK_F3,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  8*/
	XK_F11,		NoSymbol,	SunXK_F36,	NoSymbol,	/*  9*/
	XK_F4,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 10*/
	XK_F12,		NoSymbol,	SunXK_F37,	NoSymbol,	/* 11*/
	XK_F5,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 12*/
	SunXK_AltGraph,	NoSymbol,	NoSymbol,	NoSymbol,	/* 13*/
	XK_F6,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 14*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 15*/
	XK_F7,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 16*/
	XK_F8,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 17*/
	XK_F9,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 18*/
	XK_Alt_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 19*/
	XK_Up,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 20*/
	XK_Pause,  	NoSymbol,  	XK_Break,	NoSymbol,	/* 21*/
	XK_Print,  	NoSymbol,  	XK_Sys_Req,	SunXK_Sys_Req,	/* 22*/
	XK_Scroll_Lock,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 23*/
	XK_Left,	NoSymbol,	NoSymbol,	NoSymbol,	/* 24*/
	SunXK_Props,  	NoSymbol,  	XK_L3,		NoSymbol,	/* 25*/
	SunXK_Undo,  	NoSymbol,  	XK_L4,		NoSymbol,	/* 26*/
	XK_Down,	NoSymbol,	NoSymbol,	NoSymbol,	/* 27*/
	XK_Right,	NoSymbol,	NoSymbol,	NoSymbol,	/* 28*/
	XK_Escape,	NoSymbol,	NoSymbol,	NoSymbol,	/* 29*/
	XK_1,   	XK_exclam,	XK_kana_NU,	NoSymbol,	/* 30*/
	XK_2,   	XK_quotedbl,	XK_kana_FU,	NoSymbol,	/* 31*/
	XK_3,   	XK_numbersign,	XK_kana_A,	XK_kana_a,	/* 32*/
	XK_4,   	XK_dollar,	XK_kana_U,	XK_kana_u,	/* 33*/
	XK_5,   	XK_percent,	XK_kana_E,	XK_kana_e,	/* 34*/
	XK_6,   	XK_ampersand,	XK_kana_O,	XK_kana_o,	/* 35*/
	XK_7,   	XK_apostrophe,	XK_kana_YA,	XK_kana_ya,	/* 36*/
	XK_8,   	XK_parenleft,	XK_kana_YU,	XK_kana_yu,	/* 37*/
	XK_9,   	XK_parenright,	XK_kana_YO,	XK_kana_yo,	/* 38*/
	XK_0,   	XK_0,   	XK_kana_WA,	XK_kana_WO,	/* 39*/
	XK_minus,	XK_equal,	XK_kana_HO,	NoSymbol,	/* 40*/
	XK_asciicircum,	XK_asciitilde,	XK_kana_HE,	NoSymbol,	/* 41*/
	XK_backslash,	XK_bar, 	XK_prolongedsound,NoSymbol,	/* 42*/
	XK_BackSpace,	NoSymbol,	NoSymbol,	NoSymbol,	/* 43*/
	XK_Insert,	NoSymbol,	NoSymbol,	NoSymbol,	/* 44*/
	SunXK_AudioMute,SunXK_VideoDegauss,NoSymbol,	NoSymbol,	/* 45*/
	XK_KP_Divide,  	NoSymbol,  	NoSymbol,	NoSymbol,	/* 46*/
	XK_KP_Multiply,	NoSymbol,	NoSymbol,	NoSymbol,	/* 47*/
	SunXK_PowerSwitch,SunXK_PowerSwitchShift,	NoSymbol,	NoSymbol,	/* 48*/
	SunXK_Front,  	NoSymbol,  	XK_L5,		NoSymbol,	/* 49*/
	XK_KP_Delete,	XK_KP_Decimal,	NoSymbol,	NoSymbol,	/* 50*/
	SunXK_Copy,  	NoSymbol,  	XK_L6,		NoSymbol,	/* 51*/
	XK_Home,	NoSymbol,	NoSymbol,	NoSymbol,	/* 52*/
	XK_Tab, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 53*/
	XK_q,   	XK_Q,   	XK_kana_TA,	NoSymbol,	/* 54*/
	XK_w,   	XK_W,   	XK_kana_TE,	NoSymbol,	/* 55*/
	XK_e,   	XK_E,   	XK_kana_I,	XK_kana_i,	/* 56*/
	XK_r,   	XK_R,   	XK_kana_SU,	NoSymbol,	/* 57*/
	XK_t,   	XK_T,   	XK_kana_KA,	NoSymbol,	/* 58*/
	XK_y,   	XK_Y,   	XK_kana_N,	NoSymbol,	/* 59*/
	XK_u,   	XK_U,   	XK_kana_NA,	NoSymbol,	/* 60*/
	XK_i,   	XK_I,   	XK_kana_NI,	NoSymbol,	/* 61*/
	XK_o,   	XK_O,   	XK_kana_RA,	NoSymbol,	/* 62*/
	XK_p,   	XK_P,   	XK_kana_SE,	NoSymbol,	/* 63*/
	XK_at,  	XK_grave,	XK_voicedsound,	NoSymbol,	/* 64*/
	XK_bracketleft,	XK_braceleft,	XK_semivoicedsound,XK_kana_openingbracket,	/* 65*/
	XK_Delete,	NoSymbol,	NoSymbol,	NoSymbol,	/* 66*/
	SunXK_Compose,	NoSymbol,	NoSymbol,	NoSymbol,	/* 67*/
	XK_KP_Home,  	XK_KP_7,  	NoSymbol,	NoSymbol,	/* 68*/
	XK_KP_Up,  	XK_KP_8,  	NoSymbol,	NoSymbol,	/* 69*/
	XK_KP_Prior,  	XK_KP_9,  	NoSymbol,	NoSymbol,	/* 70*/
	XK_KP_Subtract,	NoSymbol,	NoSymbol,	NoSymbol,	/* 71*/
	SunXK_Open,  	NoSymbol,  	XK_L7,		NoSymbol,	/* 72*/
	SunXK_Paste,  	NoSymbol,  	XK_L8,		NoSymbol,	/* 73*/
	XK_End, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 74*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 75*/
	XK_Control_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 76*/
	XK_a,   	XK_A,   	XK_kana_CHI,	NoSymbol,	/* 77*/
	XK_s,   	XK_S,   	XK_kana_TO,	NoSymbol,	/* 78*/
	XK_d,   	XK_D,   	XK_kana_SHI,	NoSymbol,	/* 79*/
	XK_f,   	XK_F,   	XK_kana_HA,	NoSymbol,	/* 80*/
	XK_g,   	XK_G,   	XK_kana_KI,	NoSymbol,	/* 81*/
	XK_h,   	XK_H,   	XK_kana_KU,	NoSymbol,	/* 82*/
	XK_j,   	XK_J,   	XK_kana_MA,	NoSymbol,	/* 83*/
	XK_k,   	XK_K,   	XK_kana_NO,	NoSymbol,	/* 84*/
	XK_l,   	XK_L,   	XK_kana_RI,	NoSymbol,	/* 85*/
	XK_semicolon,	XK_plus,	XK_kana_RE,	NoSymbol,	/* 86*/
	XK_colon,	XK_asterisk,	XK_kana_KE,	NoSymbol,	/* 87*/
	XK_bracketright,XK_braceright,	XK_kana_MU,	XK_kana_closingbracket,	/* 88*/
	XK_Return,	NoSymbol,	NoSymbol,	NoSymbol,	/* 89*/
	XK_KP_Enter,	NoSymbol,	NoSymbol,	NoSymbol,	/* 90*/
	XK_KP_Left,	XK_KP_4, 	NoSymbol,	NoSymbol,	/* 91*/
	NoSymbol, 	XK_KP_5, 	NoSymbol,	NoSymbol,	/* 92*/
	XK_KP_Right,	XK_KP_6, 	NoSymbol,	NoSymbol,	/* 93*/
	XK_KP_Insert,	XK_KP_0,	NoSymbol,	NoSymbol,	/* 94*/
	SunXK_Find,  	NoSymbol,  	XK_L9,		NoSymbol,	/* 95*/
	XK_Prior,	NoSymbol,	NoSymbol,	NoSymbol,	/* 96*/
	SunXK_Cut, 	NoSymbol, 	XK_L10,		NoSymbol,	/* 97*/
	XK_Num_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 98*/
	XK_Shift_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 99*/
	XK_z,   	XK_Z,   	XK_kana_TSU,	XK_kana_tsu,	/*100*/
	XK_x,   	XK_X,   	XK_kana_SA,	NoSymbol,	/*101*/
	XK_c,   	XK_C,   	XK_kana_SO,	NoSymbol,	/*102*/
	XK_v,   	XK_V,   	XK_kana_HI,	NoSymbol,	/*103*/
	XK_b,   	XK_B,   	XK_kana_KO,	NoSymbol,	/*104*/
	XK_n,   	XK_N,   	XK_kana_MI,	NoSymbol,	/*105*/
	XK_m,   	XK_M,   	XK_kana_MO,	NoSymbol,	/*106*/
	XK_comma,	XK_less,	XK_kana_NE,	XK_kana_comma,	/*107*/
	XK_period,	XK_greater,	XK_kana_RU,	XK_kana_fullstop,	/*108*/
	XK_slash,	XK_question,	XK_kana_ME,	XK_kana_conjunctive,	/*109*/
	XK_Shift_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*110*/
	XK_backslash,	XK_underscore,	XK_kana_RO,	NoSymbol,	/*111*/
	XK_KP_End, 	XK_KP_1, 	NoSymbol,	NoSymbol, 	/*112*/
	XK_KP_Down,	XK_KP_2, 	NoSymbol,	NoSymbol,	/*113*/
	XK_KP_Next, 	XK_KP_3, 	NoSymbol,	NoSymbol,	/*114*/
	XK_Execute,	NoSymbol,	NoSymbol,	NoSymbol,	/*115*/
	XK_Kanji,	NoSymbol,	NoSymbol,	NoSymbol,	/*116*/
	XK_Henkan_Mode,	NoSymbol,	NoSymbol,	NoSymbol,	/*117*/
	XK_Help,	NoSymbol,	NoSymbol,	NoSymbol,	/*118*/
	XK_Caps_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/*119*/
	XK_Meta_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*120*/
	XK_space,	NoSymbol,	NoSymbol,	NoSymbol,	/*121*/
	XK_Meta_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*122*/
	XK_Next,	NoSymbol,	NoSymbol,	NoSymbol,	/*123*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*124*/
	XK_KP_Add,	XK_KP_Add,	NoSymbol,	NoSymbol,	/*125*/
};

#define Japan5Modmap Generic5Modmap

#else

#define Japan5Keymap NULL
#define Japan5Modmap NULL

#endif /* JAPAN5 */


#ifdef KOREA4

static KeySym Korea4Keymap[] = {
	SunXK_Stop,  	NoSymbol,  	XK_L1,		NoSymbol,	/*  1*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  2*/
	SunXK_Again,  	NoSymbol,  	XK_L2,		NoSymbol,	/*  3*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  4*/
	XK_F1,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  5*/
	XK_F2,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  6*/
	XK_F10, 	NoSymbol,	NoSymbol,	NoSymbol,	/*  7*/
	XK_F3,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  8*/
	XK_F11,		NoSymbol,	SunXK_F36,	NoSymbol,	/*  9*/
	XK_F4,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 10*/
	XK_F12,		NoSymbol,	SunXK_F37,	NoSymbol,	/* 11*/
	XK_F5,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 12*/
	XK_Linefeed,	NoSymbol,	NoSymbol,	NoSymbol,	/* 13*/
	XK_F6,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 14*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 15*/
	XK_F7,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 16*/
	XK_F8,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 17*/
	XK_F9,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 18*/
	XK_Alt_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 19*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 20*/
	XK_Pause,  	NoSymbol,  	XK_R1,		NoSymbol,	/* 21*/
	XK_Print,  	NoSymbol,  	XK_R2,		NoSymbol,	/* 22*/
	XK_Break,	XK_Scroll_Lock,	XK_R3,		NoSymbol,	/* 23*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 24*/
	SunXK_Props,  	NoSymbol,  	XK_L3,		NoSymbol,	/* 25*/
	SunXK_Undo,  	NoSymbol,  	XK_L4,		NoSymbol,	/* 26*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 27*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 28*/
	XK_Escape,	NoSymbol,	NoSymbol,	NoSymbol,	/* 29*/
	XK_1,   	XK_exclam,	NoSymbol,	NoSymbol,	/* 30*/
	XK_2,   	XK_at,  	NoSymbol,	NoSymbol,	/* 31*/
	XK_3,   	XK_numbersign,	NoSymbol,	NoSymbol,	/* 32*/
	XK_4,   	XK_dollar,	NoSymbol,	NoSymbol,	/* 33*/
	XK_5,   	XK_percent,	NoSymbol,	NoSymbol,	/* 34*/
	XK_6,   	XK_asciicircum,	NoSymbol,	NoSymbol,	/* 35*/
	XK_7,   	XK_ampersand,	NoSymbol,	NoSymbol,	/* 36*/
	XK_8,   	XK_asterisk,	NoSymbol,	NoSymbol,	/* 37*/
	XK_9,   	XK_parenleft,	NoSymbol,	NoSymbol,	/* 38*/
	XK_0,   	XK_parenright,	NoSymbol,	NoSymbol,	/* 39*/
	XK_minus,	XK_underscore,	NoSymbol,	NoSymbol,	/* 40*/
	XK_equal,	XK_plus,	NoSymbol,	NoSymbol,	/* 41*/
	XK_grave,	XK_asciitilde,	XK_acute,	NoSymbol,	/* 42*/
	XK_BackSpace,	NoSymbol,	NoSymbol,	NoSymbol,	/* 43*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 44*/
	XK_KP_Equal,  	NoSymbol,  	XK_R4,		NoSymbol,	/* 45*/
	XK_KP_Divide,  	NoSymbol,  	XK_R5,		NoSymbol,	/* 46*/
	XK_KP_Multiply,	NoSymbol,	XK_R6,		NoSymbol,	/* 47*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 48*/
	SunXK_Front,  	NoSymbol,  	XK_L5,		NoSymbol,	/* 49*/
	XK_KP_Delete,	XK_KP_Decimal,	NoSymbol,	NoSymbol,	/* 50*/
	SunXK_Copy,  	NoSymbol,  	XK_L6,		NoSymbol,	/* 51*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 52*/
	XK_Tab, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 53*/
	XK_Q,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 54*/
	XK_W,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 55*/
	XK_E,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 56*/
	XK_R,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 57*/
	XK_T,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 58*/
	XK_Y,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 59*/
	XK_U,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 60*/
	XK_I,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 61*/
	XK_O,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 62*/
	XK_P,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 63*/
	XK_bracketleft,	XK_braceleft,	NoSymbol,	NoSymbol,	/* 64*/
	XK_bracketright,XK_braceright,	NoSymbol,	NoSymbol,	/* 65*/
	XK_Delete,	NoSymbol,	NoSymbol,	NoSymbol,	/* 66*/
	SunXK_Compose,	NoSymbol,	NoSymbol,	NoSymbol,	/* 67*/
	XK_KP_Home,  	XK_KP_7,  	XK_R7,		NoSymbol,	/* 68*/
	XK_KP_Up,  	XK_KP_8,  	XK_R8,		NoSymbol,	/* 69*/
	XK_KP_Prior,  	XK_KP_9,  	XK_R9,		NoSymbol,	/* 70*/
	XK_KP_Subtract,	NoSymbol,	NoSymbol,	NoSymbol,	/* 71*/
	SunXK_Open,  	NoSymbol,  	XK_L7,		NoSymbol,	/* 72*/
	SunXK_Paste,  	NoSymbol,  	XK_L8,		NoSymbol,	/* 73*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 74*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 75*/
	XK_Control_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 76*/
	XK_A,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 77*/
	XK_S,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 78*/
	XK_D,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 79*/
	XK_F,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 80*/
	XK_G,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 81*/
	XK_H,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 82*/
	XK_J,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 83*/
	XK_K,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 84*/
	XK_L,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 85*/
	XK_semicolon,	XK_colon,	NoSymbol,	NoSymbol,	/* 86*/
	XK_apostrophe,	XK_quotedbl,	NoSymbol,	NoSymbol,	/* 87*/
	XK_backslash,	XK_bar, 	XK_brokenbar,	NoSymbol,	/* 88*/
	XK_Return,	NoSymbol,	NoSymbol,	NoSymbol,	/* 89*/
	XK_KP_Enter,	NoSymbol,	NoSymbol,	NoSymbol,	/* 90*/
	XK_KP_Left,	XK_KP_4, 	XK_R10,		NoSymbol,	/* 91*/
	NoSymbol, 	XK_KP_5, 	XK_R11,		NoSymbol,	/* 92*/
	XK_KP_Right,	XK_KP_6, 	XK_R12,		NoSymbol,	/* 93*/
	XK_KP_Insert,	XK_KP_0,	NoSymbol,	NoSymbol,	/* 94*/
	SunXK_Find,  	NoSymbol,  	XK_L9,		NoSymbol,	/* 95*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 96*/
	SunXK_Cut, 	NoSymbol, 	XK_L10,		NoSymbol,	/* 97*/
	XK_Num_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 98*/
	XK_Shift_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 99*/
	XK_Z,   	NoSymbol,	NoSymbol,	NoSymbol,	/*100*/
	XK_X,   	NoSymbol,	NoSymbol,	NoSymbol,	/*101*/
	XK_C,   	NoSymbol,	NoSymbol,	NoSymbol,	/*102*/
	XK_V,   	NoSymbol,	NoSymbol,	NoSymbol,	/*103*/
	XK_B,   	NoSymbol,	NoSymbol,	NoSymbol,	/*104*/
	XK_N,   	NoSymbol,	NoSymbol,	NoSymbol,	/*105*/
	XK_M,   	NoSymbol,	NoSymbol,	NoSymbol,	/*106*/
	XK_comma,	XK_less,	NoSymbol,	NoSymbol,	/*107*/
	XK_period,	XK_greater,	NoSymbol,	NoSymbol,	/*108*/
	XK_slash,	XK_question,	NoSymbol,	NoSymbol,	/*109*/
	XK_Shift_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*110*/
	SunXK_AltGraph,	NoSymbol,	NoSymbol,	NoSymbol,	/*111*/
	XK_KP_End, 	XK_KP_1, 	XK_R13,		NoSymbol, 	/*112*/
	XK_KP_Down,	XK_KP_2, 	XK_R14,		NoSymbol,	/*113*/
	XK_KP_Next, 	XK_KP_3, 	XK_R15,		NoSymbol,	/*114*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*115*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*116*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*117*/
	XK_Help,	NoSymbol,	NoSymbol,	NoSymbol,	/*118*/
	XK_Caps_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/*119*/
	XK_Meta_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*120*/
	XK_space,	NoSymbol,	NoSymbol,	NoSymbol,	/*121*/
	XK_Meta_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*122*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*123*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*124*/
	XK_KP_Add,	NoSymbol,	NoSymbol,	NoSymbol,	/*125*/
};

static SunModmapRec Korea4Modmap[] = {
	{  99,	ShiftMask },
	{ 110,	ShiftMask },
	{ 119,	LockMask },
	{  76,	ControlMask },
	{ 120,	Meta_Mask },
	{ 122,	Meta_Mask },
	{ 111,	Mode_switch_Mask },
	{  98,	Num_Lock_Mask },
	{  19,	Alt_Mask },
	{   0,	0 }
};

#else

#define Korea4Keymap NULL
#define Korea4Modmap NULL

#endif /* KOREA4 */


#ifdef KOREA5

static KeySym Korea5Keymap[] = {
	SunXK_Stop,  	NoSymbol,  	XK_L1,		NoSymbol,	/*  1*/
	SunXK_AudioLowerVolume,SunXK_VideoLowerBrightness,NoSymbol,	NoSymbol,	/*  2*/
	SunXK_Again,  	NoSymbol,  	XK_L2,		NoSymbol,	/*  3*/
	SunXK_AudioRaiseVolume,SunXK_VideoRaiseBrightness,NoSymbol,	NoSymbol,	/*  4*/
	XK_F1,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  5*/
	XK_F2,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  6*/
	XK_F10, 	NoSymbol,	NoSymbol,	NoSymbol,	/*  7*/
	XK_F3,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  8*/
	XK_F11,		NoSymbol,	SunXK_F36,	NoSymbol,	/*  9*/
	XK_F4,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 10*/
	XK_F12,		NoSymbol,	SunXK_F37,	NoSymbol,	/* 11*/
	XK_F5,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 12*/
	SunXK_AltGraph,	NoSymbol,	NoSymbol,	NoSymbol,	/* 13*/
	XK_F6,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 14*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 15*/
	XK_F7,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 16*/
	XK_F8,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 17*/
	XK_F9,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 18*/
	XK_Alt_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 19*/
	XK_Up,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 20*/
	XK_Pause,  	NoSymbol,  	XK_Break,	NoSymbol,	/* 21*/
	XK_Print,  	NoSymbol,  	XK_Sys_Req,	SunXK_Sys_Req,	/* 22*/
	XK_Scroll_Lock,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 23*/
	XK_Left,	NoSymbol,	NoSymbol,	NoSymbol,	/* 24*/
	SunXK_Props,  	NoSymbol,  	XK_L3,		NoSymbol,	/* 25*/
	SunXK_Undo,  	NoSymbol,  	XK_L4,		NoSymbol,	/* 26*/
	XK_Down,	NoSymbol,	NoSymbol,	NoSymbol,	/* 27*/
	XK_Right,	NoSymbol,	NoSymbol,	NoSymbol,	/* 28*/
	XK_Escape,	NoSymbol,	NoSymbol,	NoSymbol,	/* 29*/
	XK_1,   	XK_exclam,	NoSymbol,	NoSymbol,	/* 30*/
	XK_2,   	XK_at,  	NoSymbol,	NoSymbol,	/* 31*/
	XK_3,   	XK_numbersign,	NoSymbol,	NoSymbol,	/* 32*/
	XK_4,   	XK_dollar,	NoSymbol,	NoSymbol,	/* 33*/
	XK_5,   	XK_percent,	NoSymbol,	NoSymbol,	/* 34*/
	XK_6,   	XK_asciicircum,	NoSymbol,	NoSymbol,	/* 35*/
	XK_7,   	XK_ampersand,	NoSymbol,	NoSymbol,	/* 36*/
	XK_8,   	XK_asterisk,	NoSymbol,	NoSymbol,	/* 37*/
	XK_9,   	XK_parenleft,	NoSymbol,	NoSymbol,	/* 38*/
	XK_0,   	XK_parenright,	NoSymbol,	NoSymbol,	/* 39*/
	XK_minus,	XK_underscore,	NoSymbol,	NoSymbol,	/* 40*/
	XK_equal,	XK_plus,	NoSymbol,	NoSymbol,	/* 41*/
	XK_grave,	XK_asciitilde,	XK_acute,	NoSymbol,	/* 42*/
	XK_BackSpace,	NoSymbol,	NoSymbol,	NoSymbol,	/* 43*/
	XK_Insert,	NoSymbol,	NoSymbol,	NoSymbol,	/* 44*/
	SunXK_AudioMute,SunXK_VideoDegauss,NoSymbol,	NoSymbol,	/* 45*/
	XK_KP_Divide,  	NoSymbol,  	NoSymbol,	NoSymbol,	/* 46*/
	XK_KP_Multiply, NoSymbol,  	NoSymbol,	NoSymbol,	/* 47*/
	SunXK_PowerSwitch,SunXK_PowerSwitchShift,	NoSymbol,	NoSymbol,	/* 48*/
	SunXK_Front,  	NoSymbol,  	XK_L5,		NoSymbol,	/* 49*/
	XK_KP_Delete,	XK_KP_Decimal,	NoSymbol,	NoSymbol,	/* 50*/
	SunXK_Copy,  	NoSymbol,  	XK_L6,		NoSymbol,	/* 51*/
	XK_Home,	NoSymbol,	NoSymbol,	NoSymbol,	/* 52*/
	XK_Tab, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 53*/
	XK_Q,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 54*/
	XK_W,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 55*/
	XK_E,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 56*/
	XK_R,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 57*/
	XK_T,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 58*/
	XK_Y,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 59*/
	XK_U,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 60*/
	XK_I,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 61*/
	XK_O,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 62*/
	XK_P,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 63*/
	XK_bracketleft,	XK_braceleft,	NoSymbol,	NoSymbol,	/* 64*/
	XK_bracketright,XK_braceright,	NoSymbol,	NoSymbol,	/* 65*/
	XK_Delete,	NoSymbol,	NoSymbol,	NoSymbol,	/* 66*/
	SunXK_Compose,	NoSymbol,	NoSymbol,	NoSymbol,	/* 67*/
	XK_KP_Home,  	XK_KP_7,  	NoSymbol,	NoSymbol,	/* 68*/
	XK_KP_Up,  	XK_KP_8,  	NoSymbol,	NoSymbol,	/* 69*/
	XK_KP_Prior,  	XK_KP_9,  	NoSymbol,	NoSymbol,	/* 70*/
	XK_KP_Subtract, NoSymbol,  	NoSymbol,	NoSymbol,	/* 71*/
	SunXK_Open,  	NoSymbol,  	XK_L7,		NoSymbol,	/* 72*/
	SunXK_Paste,  	NoSymbol,  	XK_L8,		NoSymbol,	/* 73*/
	XK_End, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 74*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 75*/
	XK_Control_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 76*/
	XK_A,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 77*/
	XK_S,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 78*/
	XK_D,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 79*/
	XK_F,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 80*/
	XK_G,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 81*/
	XK_H,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 82*/
	XK_J,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 83*/
	XK_K,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 84*/
	XK_L,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 85*/
	XK_semicolon,	XK_colon,	NoSymbol,	NoSymbol,	/* 86*/
	XK_apostrophe,	XK_quotedbl,	NoSymbol,	NoSymbol,	/* 87*/
	XK_backslash,	XK_bar, 	XK_brokenbar,	NoSymbol,	/* 88*/
	XK_Return,	NoSymbol,	NoSymbol,	NoSymbol,	/* 89*/
	XK_KP_Enter,	NoSymbol,	NoSymbol,	NoSymbol,	/* 90*/
	XK_KP_Left,	XK_KP_4, 	NoSymbol,	NoSymbol,	/* 91*/
	NoSymbol, 	XK_KP_5, 	NoSymbol,	NoSymbol,	/* 92*/
	XK_KP_Right,	XK_KP_6, 	NoSymbol,	NoSymbol,	/* 93*/
	XK_KP_Insert,	XK_KP_0,	NoSymbol,	NoSymbol,	/* 94*/
	SunXK_Find,  	NoSymbol,  	XK_L9,		NoSymbol,	/* 95*/
	XK_Prior,	NoSymbol,	NoSymbol,	NoSymbol,	/* 96*/
	SunXK_Cut, 	NoSymbol, 	XK_L10,		NoSymbol,	/* 97*/
	XK_Num_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 98*/
	XK_Shift_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 99*/
	XK_Z,   	NoSymbol,	NoSymbol,	NoSymbol,	/*100*/
	XK_X,   	NoSymbol,	NoSymbol,	NoSymbol,	/*101*/
	XK_C,   	NoSymbol,	NoSymbol,	NoSymbol,	/*102*/
	XK_V,   	NoSymbol,	NoSymbol,	NoSymbol,	/*103*/
	XK_B,   	NoSymbol,	NoSymbol,	NoSymbol,	/*104*/
	XK_N,   	NoSymbol,	NoSymbol,	NoSymbol,	/*105*/
	XK_M,   	NoSymbol,	NoSymbol,	NoSymbol,	/*106*/
	XK_comma,	XK_less,	NoSymbol,	NoSymbol,	/*107*/
	XK_period,	XK_greater,	NoSymbol,	NoSymbol,	/*108*/
	XK_slash,	XK_question,	NoSymbol,	NoSymbol,	/*109*/
	XK_Shift_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*110*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*111*/
	XK_KP_End, 	XK_KP_1, 	NoSymbol,	NoSymbol, 	/*112*/
	XK_KP_Down,	XK_KP_2, 	NoSymbol,	NoSymbol,	/*113*/
	XK_KP_Next, 	XK_KP_3, 	NoSymbol,	NoSymbol,	/*114*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*115*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*116*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*117*/
	XK_Help,	NoSymbol,	NoSymbol,	NoSymbol,	/*118*/
	XK_Caps_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/*119*/
	XK_Meta_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*120*/
	XK_space,	NoSymbol,	NoSymbol,	NoSymbol,	/*121*/
	XK_Meta_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*122*/
	XK_Next,	NoSymbol,	NoSymbol,	NoSymbol,	/*123*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*124*/
	XK_KP_Add,	NoSymbol,	NoSymbol,	NoSymbol,	/*125*/
};

#define Korea5Modmap Generic5Modmap

#else

#define Korea5Keymap NULL
#define Korea5Modmap NULL

#endif /* KOREA5 */


#ifdef NETH4

static KeySym Netherland4Keymap[] = {
	SunXK_Stop,  	NoSymbol,  	XK_L1,		NoSymbol,	/*  1*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  2*/
	SunXK_Again,  	NoSymbol,  	XK_L2,		NoSymbol,	/*  3*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  4*/
	XK_F1,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  5*/
	XK_F2,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  6*/
	XK_F10, 	NoSymbol,	NoSymbol,	NoSymbol,	/*  7*/
	XK_F3,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  8*/
	XK_F11,		NoSymbol,	SunXK_F36,	NoSymbol,	/*  9*/
	XK_F4,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 10*/
	XK_F12,		NoSymbol,	SunXK_F37,	NoSymbol,	/* 11*/
	XK_F5,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 12*/
	XK_Caps_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 13*/
	XK_F6,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 14*/
	XK_backslash,	XK_bar, 	NoSymbol,	NoSymbol,	/* 15*/
	XK_F7,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 16*/
	XK_F8,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 17*/
	XK_F9,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 18*/
	XK_Alt_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 19*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 20*/
	XK_Pause,  	NoSymbol,  	XK_R1,		NoSymbol,	/* 21*/
	XK_Print,  	NoSymbol,  	XK_R2,		NoSymbol,	/* 22*/
	XK_Break,  	XK_Scroll_Lock,	XK_R3,		NoSymbol,	/* 23*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 24*/
	SunXK_Props,  	NoSymbol,  	XK_L3,		NoSymbol,	/* 25*/
	SunXK_Undo,  	NoSymbol,  	XK_L4,		NoSymbol,	/* 26*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 27*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 28*/
	XK_Escape,	NoSymbol,	NoSymbol,	NoSymbol,	/* 29*/
	XK_1,   	XK_exclam,	XK_onesuperior,	NoSymbol,	/* 30*/
	XK_2,   	XK_quotedbl,	XK_twosuperior,	NoSymbol,	/* 31*/
	XK_3,   	XK_numbersign,	XK_threesuperior,NoSymbol,	/* 32*/
	XK_4,   	XK_dollar,	XK_onequarter,	NoSymbol,	/* 33*/
	XK_5,   	XK_percent,	XK_onehalf,	NoSymbol,	/* 34*/
	XK_6,   	XK_ampersand,	XK_threequarters,NoSymbol,	/* 35*/
	XK_7,   	XK_underscore,	XK_sterling,	NoSymbol,	/* 36*/
	XK_8,   	XK_parenleft,	XK_braceleft,	NoSymbol,	/* 37*/
	XK_9,   	XK_parenright,	XK_braceright,	NoSymbol,	/* 38*/
	XK_0,   	XK_apostrophe,	XK_grave,	NoSymbol,	/* 39*/
	XK_slash,	XK_question,	NoSymbol,	NoSymbol,	/* 40*/
	XK_degree,	SunXK_FA_Tilde,	SunXK_FA_Cedilla,NoSymbol,	/* 41*/
	XK_less,	XK_greater,	NoSymbol,	NoSymbol,	/* 42*/
	XK_BackSpace,	NoSymbol,	NoSymbol,	NoSymbol,	/* 43*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 44*/
	XK_KP_Equal,  	NoSymbol,  	XK_R4,		NoSymbol,	/* 45*/
	XK_KP_Divide,  	NoSymbol,  	XK_R5,		NoSymbol,	/* 46*/
	XK_KP_Multiply, NoSymbol,  	XK_R6,		NoSymbol,	/* 47*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 48*/
	SunXK_Front,  	NoSymbol,  	XK_L5,		NoSymbol,	/* 49*/
	XK_KP_Delete,	XK_KP_Decimal,	NoSymbol,	NoSymbol,	/* 50*/
	SunXK_Copy,  	NoSymbol,  	XK_L6,		NoSymbol,	/* 51*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 52*/
	XK_Tab, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 53*/
	XK_Q,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 54*/
	XK_W,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 55*/
	XK_E,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 56*/
	XK_R,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 57*/
	XK_T,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 58*/
	XK_Y,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 59*/
	XK_U,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 60*/
	XK_I,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 61*/
	XK_O,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 62*/
	XK_P,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 63*/
	SunXK_FA_Diaeresis,XK_asciicircum,SunXK_FA_Circum,NoSymbol,	/* 64*/
	XK_asterisk,	XK_brokenbar,	XK_asciitilde,	NoSymbol,	/* 65*/
	XK_Delete,	NoSymbol,	NoSymbol,	NoSymbol,	/* 66*/
	SunXK_Compose,	NoSymbol,	NoSymbol,	NoSymbol,	/* 67*/
	XK_KP_Home,  	XK_KP_7,  	XK_R7,		NoSymbol,	/* 68*/
	XK_KP_Up,  	XK_KP_8,  	XK_R8,		NoSymbol,	/* 69*/
	XK_KP_Prior,  	XK_KP_9,  	XK_R9,		NoSymbol,	/* 70*/
	XK_KP_Subtract,	NoSymbol,	NoSymbol,	NoSymbol,	/* 71*/
	SunXK_Open,  	NoSymbol,  	XK_L7,		NoSymbol,	/* 72*/
	SunXK_Paste,  	NoSymbol,  	XK_L8,		NoSymbol,	/* 73*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 74*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 75*/
	XK_Control_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 76*/
	XK_A,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 77*/
	XK_S,   	NoSymbol,	XK_ssharp,	NoSymbol,	/* 78*/
	XK_D,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 79*/
	XK_F,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 80*/
	XK_G,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 81*/
	XK_H,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 82*/
	XK_J,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 83*/
	XK_K,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 84*/
	XK_L,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 85*/
	XK_plus,	XK_plusminus,	NoSymbol,	NoSymbol,	/* 86*/
	SunXK_FA_Acute,	SunXK_FA_Grave,	NoSymbol,	NoSymbol,	/* 87*/
	XK_at,  	XK_section,	XK_notsign,	NoSymbol,	/* 88*/
	XK_Return,	NoSymbol,	NoSymbol,	NoSymbol,	/* 89*/
	XK_KP_Enter,	NoSymbol,	NoSymbol,	NoSymbol,	/* 90*/
	XK_KP_Left,	XK_KP_4, 	XK_R10,		NoSymbol,	/* 91*/
	NoSymbol, 	XK_KP_5, 	XK_R11,		NoSymbol,	/* 92*/
	XK_KP_Right,	XK_KP_6, 	XK_R12,		NoSymbol,	/* 93*/
	XK_KP_Insert,	XK_KP_0,	NoSymbol,	NoSymbol,	/* 94*/
	SunXK_Find,  	NoSymbol,  	XK_L9,		NoSymbol,	/* 95*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 96*/
	SunXK_Cut, 	NoSymbol, 	XK_L10,		NoSymbol,	/* 97*/
	XK_Num_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 98*/
	XK_Shift_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 99*/
	XK_Z,   	NoSymbol,	XK_guillemotleft,NoSymbol,	/*100*/
	XK_X,   	NoSymbol,	XK_guillemotright,NoSymbol,	/*101*/
	XK_C,   	NoSymbol,	XK_cent,	NoSymbol,	/*102*/
	XK_V,   	NoSymbol,	NoSymbol,	NoSymbol,	/*103*/
	XK_B,   	NoSymbol,	NoSymbol,	NoSymbol,	/*104*/
	XK_N,   	NoSymbol,	NoSymbol,	NoSymbol,	/*105*/
	XK_M,   	NoSymbol,	XK_mu,  	NoSymbol,	/*106*/
	XK_comma,	XK_semicolon,	NoSymbol,	NoSymbol,	/*107*/
	XK_period,	XK_colon,	NoSymbol,	NoSymbol,	/*108*/
	XK_minus,	XK_equal,	NoSymbol,	NoSymbol,	/*109*/
	XK_Shift_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*110*/
	XK_Linefeed,	NoSymbol,	NoSymbol,	NoSymbol,	/*111*/
	XK_KP_End, 	XK_KP_1, 	XK_R13,		NoSymbol, 	/*112*/
	XK_KP_Down,	XK_KP_2, 	XK_R14,		NoSymbol,	/*113*/
	XK_KP_Next, 	XK_KP_3, 	XK_R15,		NoSymbol,	/*114*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*115*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*116*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*117*/
	XK_Help,	NoSymbol,	NoSymbol,	NoSymbol,	/*118*/
	SunXK_AltGraph,	NoSymbol,	NoSymbol,	NoSymbol,	/*119*/
	XK_Meta_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*120*/
	XK_space,	NoSymbol,	NoSymbol,	NoSymbol,	/*121*/
	XK_Meta_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*122*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*123*/
	XK_bracketright,XK_bracketleft,	NoSymbol,	NoSymbol,	/*124*/
	XK_KP_Add,	NoSymbol,	NoSymbol,	NoSymbol,	/*125*/
};

#define Netherland4Modmap FrBeNeth4Modmap

#else

#define Netherland4Keymap NULL
#define Netherland4Modmap NULL

#endif /* NETHERLAND4 */


#ifdef NETH5

static KeySym Netherland5Keymap[] = {
	SunXK_Stop,  	NoSymbol,  	XK_L1,		NoSymbol,	/*  1*/
	SunXK_AudioLowerVolume,SunXK_VideoLowerBrightness,NoSymbol,	NoSymbol,	/*  2*/
	SunXK_Again,  	NoSymbol,  	XK_L2,		NoSymbol,	/*  3*/
	SunXK_AudioRaiseVolume,SunXK_VideoRaiseBrightness,NoSymbol,	NoSymbol,	/*  4*/
	XK_F1,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  5*/
	XK_F2,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  6*/
	XK_F10, 	NoSymbol,	NoSymbol,	NoSymbol,	/*  7*/
	XK_F3,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  8*/
	XK_F11,		NoSymbol,	SunXK_F36,	NoSymbol,	/*  9*/
	XK_F4,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 10*/
	XK_F12,		NoSymbol,	SunXK_F37,	NoSymbol,	/* 11*/
	XK_F5,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 12*/
	SunXK_AltGraph,	NoSymbol,	NoSymbol,	NoSymbol,	/* 13*/
	XK_F6,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 14*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 15*/
	XK_F7,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 16*/
	XK_F8,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 17*/
	XK_F9,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 18*/
	XK_Alt_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 19*/
	XK_Up,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 20*/
	XK_Pause,  	NoSymbol,  	XK_Break,	NoSymbol,	/* 21*/
	XK_Print,  	NoSymbol,  	XK_Sys_Req,	SunXK_Sys_Req,	/* 22*/
	XK_Scroll_Lock,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 23*/
	XK_Left,	NoSymbol,	NoSymbol,	NoSymbol,	/* 24*/
	SunXK_Props,  	NoSymbol,  	XK_L3,		NoSymbol,	/* 25*/
	SunXK_Undo,  	NoSymbol,  	XK_L4,		NoSymbol,	/* 26*/
	XK_Down,	NoSymbol,	NoSymbol,	NoSymbol,	/* 27*/
	XK_Right,	NoSymbol,	NoSymbol,	NoSymbol,	/* 28*/
	XK_Escape,	NoSymbol,	NoSymbol,	NoSymbol,	/* 29*/
	XK_1,   	XK_exclam,	XK_onesuperior,	NoSymbol,	/* 30*/
	XK_2,   	XK_quotedbl,	XK_twosuperior,	NoSymbol,	/* 31*/
	XK_3,   	XK_numbersign,	XK_threesuperior,NoSymbol,	/* 32*/
	XK_4,   	XK_dollar,	XK_onequarter,	NoSymbol,	/* 33*/
	XK_5,   	XK_percent,	XK_onehalf,	NoSymbol,	/* 34*/
	XK_6,   	XK_ampersand,	XK_threequarters,NoSymbol,	/* 35*/
	XK_7,   	XK_underscore,	XK_sterling,	NoSymbol,	/* 36*/
	XK_8,   	XK_parenleft,	XK_braceleft,	NoSymbol,	/* 37*/
	XK_9,   	XK_parenright,	XK_braceright,	NoSymbol,	/* 38*/
	XK_0,   	XK_apostrophe,	XK_grave,	NoSymbol,	/* 39*/
	XK_slash,	XK_question,	XK_backslash,	NoSymbol,	/* 40*/
	XK_degree,	SunXK_FA_Tilde,	SunXK_FA_Cedilla,NoSymbol,	/* 41*/
	XK_at,  	XK_section,	XK_notsign,	NoSymbol,	/* 42*/
	XK_BackSpace,	NoSymbol,	NoSymbol,	NoSymbol,	/* 43*/
	XK_Insert,	NoSymbol,	NoSymbol,	NoSymbol,	/* 44*/
	SunXK_AudioMute,SunXK_VideoDegauss,NoSymbol,	NoSymbol,	/* 45*/
	XK_KP_Divide,  	NoSymbol,  	NoSymbol,	NoSymbol,	/* 46*/
	XK_KP_Multiply,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 47*/
	SunXK_PowerSwitch,SunXK_PowerSwitchShift,	NoSymbol,	NoSymbol,	/* 48*/
	SunXK_Front,  	NoSymbol,  	XK_L5,		NoSymbol,	/* 49*/
	XK_KP_Delete,	XK_KP_Separator,NoSymbol,	NoSymbol,	/* 50*/
	SunXK_Copy,  	NoSymbol,  	XK_L6,		NoSymbol,	/* 51*/
	XK_Home,	NoSymbol,	NoSymbol,	NoSymbol,	/* 52*/
	XK_Tab, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 53*/
	XK_Q,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 54*/
	XK_W,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 55*/
	XK_E,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 56*/
	XK_R,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 57*/
	XK_T,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 58*/
	XK_Y,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 59*/
	XK_U,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 60*/
	XK_I,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 61*/
	XK_O,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 62*/
	XK_P,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 63*/
	SunXK_FA_Diaeresis,SunXK_FA_Circum,NoSymbol,	NoSymbol,	/* 64*/
	XK_asterisk,	XK_bar, 	XK_asciitilde,	NoSymbol,	/* 65*/
	XK_Delete,	NoSymbol,	NoSymbol,	NoSymbol,	/* 66*/
	SunXK_Compose,	NoSymbol,	NoSymbol,	NoSymbol,	/* 67*/
	XK_KP_Home,  	XK_KP_7,  	NoSymbol,	NoSymbol,	/* 68*/
	XK_KP_Up,  	XK_KP_8,  	NoSymbol,	NoSymbol,	/* 69*/
	XK_KP_Prior,  	XK_KP_9,  	NoSymbol,	NoSymbol,	/* 70*/
	XK_KP_Subtract,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 71*/
	SunXK_Open,  	NoSymbol,  	XK_L7,		NoSymbol,	/* 72*/
	SunXK_Paste,  	NoSymbol,  	XK_L8,		NoSymbol,	/* 73*/
	XK_End, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 74*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 75*/
	XK_Control_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 76*/
	XK_A,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 77*/
	XK_S,   	NoSymbol,   	XK_ssharp,	NoSymbol,	/* 78*/
	XK_D,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 79*/
	XK_F,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 80*/
	XK_G,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 81*/
	XK_H,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 82*/
	XK_J,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 83*/
	XK_K,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 84*/
	XK_L,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 85*/
	XK_plus,	XK_plusminus,	NoSymbol,	NoSymbol,	/* 86*/
	SunXK_FA_Acute,	SunXK_FA_Grave,	NoSymbol,	NoSymbol,	/* 87*/
	XK_less,	XK_greater,	XK_asciicircum,	NoSymbol,	/* 88*/
	XK_Return,	NoSymbol,	NoSymbol,	NoSymbol,	/* 89*/
	XK_KP_Enter,	NoSymbol,	NoSymbol,	NoSymbol,	/* 90*/
	XK_KP_Left,	XK_KP_4, 	NoSymbol,	NoSymbol,	/* 91*/
	NoSymbol, 	XK_KP_5, 	NoSymbol,	NoSymbol,	/* 92*/
	XK_KP_Right,	XK_KP_6, 	NoSymbol,	NoSymbol,	/* 93*/
	XK_KP_Insert,	XK_KP_0,	NoSymbol,	NoSymbol,	/* 94*/
	SunXK_Find,  	NoSymbol,  	XK_L9,		NoSymbol,	/* 95*/
	XK_Prior,	NoSymbol,	NoSymbol,	NoSymbol,	/* 96*/
	SunXK_Cut, 	NoSymbol, 	XK_L10,		NoSymbol,	/* 97*/
	XK_Num_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 98*/
	XK_Shift_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 99*/
	XK_z,   	XK_Z,   	XK_guillemotleft,NoSymbol,	/*100*/
	XK_x,   	XK_X,   	XK_guillemotright,NoSymbol,	/*101*/
	XK_c,   	XK_C,   	XK_cent,	NoSymbol,	/*102*/
	XK_V,   	NoSymbol,	NoSymbol,	NoSymbol,	/*103*/
	XK_B,   	NoSymbol,	NoSymbol,	NoSymbol,	/*104*/
	XK_N,   	NoSymbol,	NoSymbol,	NoSymbol,	/*105*/
	XK_m,   	XK_M,   	XK_mu,  	NoSymbol,	/*106*/
	XK_comma,	XK_semicolon,	NoSymbol,	NoSymbol,	/*107*/
	XK_period,	XK_colon,	XK_periodcentered,NoSymbol,	/*108*/
	XK_minus,	XK_equal,	NoSymbol,	NoSymbol,	/*109*/
	XK_Shift_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*110*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*111*/
	XK_KP_End, 	XK_KP_1, 	NoSymbol,	NoSymbol, 	/*112*/
	XK_KP_Down,	XK_KP_2, 	NoSymbol,	NoSymbol,	/*113*/
	XK_KP_Next, 	XK_KP_3, 	NoSymbol,	NoSymbol,	/*114*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*115*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*116*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*117*/
	XK_Help,	NoSymbol,	NoSymbol,	NoSymbol,	/*118*/
	XK_Caps_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/*119*/
	XK_Meta_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*120*/
	XK_space,	NoSymbol,	NoSymbol,	NoSymbol,	/*121*/
	XK_Meta_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*122*/
	XK_Next,	NoSymbol,	NoSymbol,	NoSymbol,	/*123*/
	XK_bracketright,XK_bracketleft,	XK_brokenbar,	NoSymbol,	/*124*/
	XK_KP_Add,	NoSymbol,	NoSymbol,	NoSymbol,	/*125*/
};

#define Netherland5Modmap Generic5Modmap

#else

#define Netherland5Keymap NULL
#define Netherland5Modmap NULL

#endif /* NETHERLAND5 */


#ifdef NORW4

static KeySym Norway4Keymap[] = {
	SunXK_Stop,  	NoSymbol,  	XK_L1,		NoSymbol,	/*  1*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  2*/
	SunXK_Again,  	NoSymbol,  	XK_L2,		NoSymbol,	/*  3*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  4*/
	XK_F1,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  5*/
	XK_F2,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  6*/
	XK_F10, 	NoSymbol,	NoSymbol,	NoSymbol,	/*  7*/
	XK_F3,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  8*/
	XK_F11,		NoSymbol,	SunXK_F36,	NoSymbol,	/*  9*/
	XK_F4,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 10*/
	XK_F12,		NoSymbol,	SunXK_F37,	NoSymbol,	/* 11*/
	XK_F5,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 12*/
	SunXK_AltGraph,	NoSymbol,	NoSymbol,	NoSymbol,	/* 13*/
	XK_F6,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 14*/
	XK_asciitilde,	XK_asciicircum,	NoSymbol,	NoSymbol,	/* 15*/
	XK_F7,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 16*/
	XK_F8,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 17*/
	XK_F9,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 18*/
	XK_Alt_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 19*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 20*/
	XK_Pause,  	NoSymbol,  	XK_R1,		NoSymbol,	/* 21*/
	XK_Print,  	NoSymbol,  	XK_R2,		NoSymbol,	/* 22*/
	XK_Break,  	XK_Scroll_Lock,	XK_R3,		NoSymbol,	/* 23*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 24*/
	SunXK_Props,  	NoSymbol,  	XK_L3,		NoSymbol,	/* 25*/
	SunXK_Undo,  	NoSymbol,  	XK_L4,		NoSymbol,	/* 26*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 27*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 28*/
	XK_Escape,	NoSymbol,	NoSymbol,	NoSymbol,	/* 29*/
	XK_1,   	XK_exclam,	NoSymbol,	NoSymbol,	/* 30*/
	XK_2,   	XK_quotedbl,	XK_at,  	NoSymbol,	/* 31*/
	XK_3,   	XK_numbersign,	XK_sterling,	NoSymbol,	/* 32*/
	XK_4,   	XK_currency,	XK_dollar,	NoSymbol,	/* 33*/
	XK_5,   	XK_percent,	NoSymbol,	NoSymbol,	/* 34*/
	XK_6,   	XK_ampersand,	NoSymbol,	NoSymbol,	/* 35*/
	XK_7,   	XK_slash,	XK_braceleft,	NoSymbol,	/* 36*/
	XK_8,   	XK_parenleft,	XK_bracketleft,	NoSymbol,	/* 37*/
	XK_9,   	XK_parenright,	XK_bracketright,NoSymbol,	/* 38*/
	XK_0,   	XK_equal,	XK_braceright,	NoSymbol,	/* 39*/
	XK_plus,	XK_question,	NoSymbol,	NoSymbol,	/* 40*/
	XK_backslash,	SunXK_FA_Grave,	SunXK_FA_Acute,	NoSymbol,	/* 41*/
	XK_apostrophe,	XK_asterisk,	XK_grave,	NoSymbol,	/* 42*/
	XK_BackSpace,	NoSymbol,	NoSymbol,	NoSymbol,	/* 43*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 44*/
	XK_KP_Equal,  	NoSymbol,  	XK_R4,		NoSymbol,	/* 45*/
	XK_KP_Divide,  	NoSymbol,  	XK_R5,		NoSymbol,	/* 46*/
	XK_KP_Multiply,	NoSymbol,  	XK_R6,		NoSymbol,	/* 47*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 48*/
	SunXK_Front,  	NoSymbol,  	XK_L5,		NoSymbol,	/* 49*/
	XK_KP_Delete,	XK_KP_Decimal,	NoSymbol,	NoSymbol,	/* 50*/
	SunXK_Copy,  	NoSymbol,  	XK_L6,		NoSymbol,	/* 51*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 52*/
	XK_Tab, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 53*/
	XK_Q,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 54*/
	XK_W,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 55*/
	XK_E,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 56*/
	XK_R,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 57*/
	XK_T,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 58*/
	XK_Y,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 59*/
	XK_U,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 60*/
	XK_I,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 61*/
	XK_O,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 62*/
	XK_P,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 63*/
	XK_Aring,	NoSymbol,	NoSymbol,	NoSymbol,	/* 64*/
	SunXK_FA_Diaeresis,SunXK_FA_Circum,SunXK_FA_Tilde,NoSymbol,	/* 65*/
	XK_Delete,	NoSymbol,	NoSymbol,	NoSymbol,	/* 66*/
	SunXK_Compose,	NoSymbol,	NoSymbol,	NoSymbol,	/* 67*/
	XK_KP_Home,  	XK_KP_7,  	XK_R7,		NoSymbol,	/* 68*/
	XK_KP_Up,  	XK_KP_8,  	XK_R8,		NoSymbol,	/* 69*/
	XK_KP_Prior,  	XK_KP_9,  	XK_R9,		NoSymbol,	/* 70*/
	XK_KP_Subtract,	NoSymbol,	NoSymbol,	NoSymbol,	/* 71*/
	SunXK_Open,  	NoSymbol,  	XK_L7,		NoSymbol,	/* 72*/
	SunXK_Paste,  	NoSymbol,  	XK_L8,		NoSymbol,	/* 73*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 74*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 75*/
	XK_Caps_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 76*/
	XK_A,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 77*/
	XK_S,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 78*/
	XK_D,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 79*/
	XK_F,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 80*/
	XK_G,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 81*/
	XK_H,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 82*/
	XK_J,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 83*/
	XK_K,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 84*/
	XK_L,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 85*/
	XK_Ooblique,	NoSymbol,	NoSymbol,	NoSymbol,	/* 86*/
	XK_AE,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 87*/
	XK_bar, 	XK_section,	NoSymbol,	NoSymbol,	/* 88*/
	XK_Return,	NoSymbol,	NoSymbol,	NoSymbol,	/* 89*/
	XK_KP_Enter,	NoSymbol,	NoSymbol,	NoSymbol,	/* 90*/
	XK_KP_Left,	XK_KP_4, 	XK_R10,		NoSymbol,	/* 91*/
	NoSymbol, 	XK_KP_5, 	XK_R11,		NoSymbol,	/* 92*/
	XK_KP_Right,	XK_KP_6, 	XK_R12,		NoSymbol,	/* 93*/
	XK_KP_Insert,	XK_KP_0,	NoSymbol,	NoSymbol,	/* 94*/
	SunXK_Find,  	NoSymbol,  	XK_L9,		NoSymbol,	/* 95*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 96*/
	SunXK_Cut, 	NoSymbol, 	XK_L10,		NoSymbol,	/* 97*/
	XK_Num_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 98*/
	XK_Shift_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 99*/
	XK_Z,   	NoSymbol,	NoSymbol,	NoSymbol,	/*100*/
	XK_X,   	NoSymbol,	NoSymbol,	NoSymbol,	/*101*/
	XK_C,   	NoSymbol,	NoSymbol,	NoSymbol,	/*102*/
	XK_V,   	NoSymbol,	NoSymbol,	NoSymbol,	/*103*/
	XK_B,   	NoSymbol,	NoSymbol,	NoSymbol,	/*104*/
	XK_N,   	NoSymbol,	NoSymbol,	NoSymbol,	/*105*/
	XK_M,   	NoSymbol,	NoSymbol,	NoSymbol,	/*106*/
	XK_comma,	XK_semicolon,	NoSymbol,	NoSymbol,	/*107*/
	XK_period,	XK_colon,	NoSymbol,	NoSymbol,	/*108*/
	XK_minus,	XK_underscore,	NoSymbol,	NoSymbol,	/*109*/
	XK_Shift_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*110*/
	XK_Linefeed,	NoSymbol,	NoSymbol,	NoSymbol,	/*111*/
	XK_KP_End, 	XK_KP_1, 	XK_R13,		NoSymbol, 	/*112*/
	XK_KP_Down,	XK_KP_2, 	XK_R14,		NoSymbol,	/*113*/
	XK_KP_Next, 	XK_KP_3, 	XK_R15,		NoSymbol,	/*114*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*115*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*116*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*117*/
	XK_Help,	NoSymbol,	NoSymbol,	NoSymbol,	/*118*/
	XK_Control_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*119*/
	XK_Meta_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*120*/
	XK_space,	NoSymbol,	NoSymbol,	NoSymbol,	/*121*/
	XK_Meta_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*122*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*123*/
	XK_less,	XK_greater,	NoSymbol,	NoSymbol,	/*124*/
	XK_KP_Add,	NoSymbol,	NoSymbol,	NoSymbol,	/*125*/
};

#define Norway4Modmap ItNorPortSp4Modmap

#else

#define Norway4Keymap NULL
#define Norway4Modmap NULL

#endif /* NORWAY4 */


#ifdef NORW5

static KeySym Norway5Keymap[] = {
	SunXK_Stop,  	NoSymbol,  	XK_L1,		NoSymbol,	/*  1*/
	SunXK_AudioLowerVolume,SunXK_VideoLowerBrightness,NoSymbol,	NoSymbol,	/*  2*/
	SunXK_Again,  	NoSymbol,  	XK_L2,		NoSymbol,	/*  3*/
	SunXK_AudioRaiseVolume,SunXK_VideoRaiseBrightness,NoSymbol,	NoSymbol,	/*  4*/
	XK_F1,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  5*/
	XK_F2,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  6*/
	XK_F10, 	NoSymbol,	NoSymbol,	NoSymbol,	/*  7*/
	XK_F3,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  8*/
	XK_F11,		NoSymbol,	SunXK_F36,	NoSymbol,	/*  9*/
	XK_F4,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 10*/
	XK_F12,		NoSymbol,	SunXK_F37,	NoSymbol,	/* 11*/
	XK_F5,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 12*/
	SunXK_AltGraph,	NoSymbol,	NoSymbol,	NoSymbol,	/* 13*/
	XK_F6,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 14*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 15*/
	XK_F7,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 16*/
	XK_F8,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 17*/
	XK_F9,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 18*/
	XK_Alt_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 19*/
	XK_Up,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 20*/
	XK_Pause,  	NoSymbol,  	XK_Break,	NoSymbol,	/* 21*/
	XK_Print,  	NoSymbol,  	XK_Sys_Req,	SunXK_Sys_Req,	/* 22*/
	XK_Scroll_Lock,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 23*/
	XK_Left,	NoSymbol,	NoSymbol,	NoSymbol,	/* 24*/
	SunXK_Props,  	NoSymbol,  	XK_L3,		NoSymbol,	/* 25*/
	SunXK_Undo,  	NoSymbol,  	XK_L4,		NoSymbol,	/* 26*/
	XK_Down,	NoSymbol,	NoSymbol,	NoSymbol,	/* 27*/
	XK_Right,	NoSymbol,	NoSymbol,	NoSymbol,	/* 28*/
	XK_Escape,	NoSymbol,	NoSymbol,	NoSymbol,	/* 29*/
	XK_1,   	XK_exclam,	NoSymbol,	NoSymbol,	/* 30*/
	XK_2,   	XK_quotedbl,	XK_at,  	NoSymbol,	/* 31*/
	XK_3,   	XK_numbersign,	XK_sterling,	NoSymbol,	/* 32*/
	XK_4,   	XK_currency,	XK_dollar,	NoSymbol,	/* 33*/
	XK_5,   	XK_percent,	XK_asciitilde,	NoSymbol,	/* 34*/
	XK_6,   	XK_ampersand,	XK_asciicircum,	NoSymbol,	/* 35*/
	XK_7,   	XK_slash,	XK_braceleft,	NoSymbol,	/* 36*/
	XK_8,   	XK_parenleft,	XK_bracketleft,	NoSymbol,	/* 37*/
	XK_9,   	XK_parenright,	XK_bracketright,NoSymbol,	/* 38*/
	XK_0,   	XK_equal,	XK_braceright,	NoSymbol,	/* 39*/
	XK_plus,	XK_question,	NoSymbol,	NoSymbol,	/* 40*/
	XK_backslash,	SunXK_FA_Grave,	SunXK_FA_Acute,	NoSymbol,	/* 41*/
	XK_bar, 	XK_section,	NoSymbol,	NoSymbol,	/* 42*/
	XK_BackSpace,	NoSymbol,	NoSymbol,	NoSymbol,	/* 43*/
	XK_Insert,	NoSymbol,	NoSymbol,	NoSymbol,	/* 44*/
	SunXK_AudioMute,SunXK_VideoDegauss,NoSymbol,	NoSymbol,	/* 45*/
	XK_KP_Divide,  	NoSymbol,  	NoSymbol,	NoSymbol,	/* 46*/
	XK_KP_Multiply, NoSymbol,  	NoSymbol,	NoSymbol,	/* 47*/
	SunXK_PowerSwitch,SunXK_PowerSwitchShift,	NoSymbol,	NoSymbol,	/* 48*/
	SunXK_Front,  	NoSymbol,  	XK_L5,		NoSymbol,	/* 49*/
	XK_KP_Delete,	XK_KP_Separator,NoSymbol,	NoSymbol,	/* 50*/
	SunXK_Copy,  	NoSymbol,  	XK_L6,		NoSymbol,	/* 51*/
	XK_Home,	NoSymbol,	NoSymbol,	NoSymbol,	/* 52*/
	XK_Tab, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 53*/
	XK_Q,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 54*/
	XK_W,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 55*/
	XK_E,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 56*/
	XK_R,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 57*/
	XK_T,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 58*/
	XK_Y,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 59*/
	XK_U,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 60*/
	XK_I,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 61*/
	XK_O,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 62*/
	XK_P,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 63*/
	XK_Aring,	NoSymbol,	NoSymbol,	NoSymbol,	/* 64*/
	SunXK_FA_Diaeresis,SunXK_FA_Circum,SunXK_FA_Tilde,NoSymbol,	/* 65*/
	XK_Delete,	NoSymbol,	NoSymbol,	NoSymbol,	/* 66*/
	SunXK_Compose,	NoSymbol,	NoSymbol,	NoSymbol,	/* 67*/
	XK_KP_Home,  	XK_KP_7,  	NoSymbol,	NoSymbol,	/* 68*/
	XK_KP_Up,  	XK_KP_8,  	NoSymbol,	NoSymbol,	/* 69*/
	XK_KP_Prior,  	XK_KP_9,  	NoSymbol,	NoSymbol,	/* 70*/
	XK_KP_Subtract,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 71*/
	SunXK_Open,  	NoSymbol,  	XK_L7,		NoSymbol,	/* 72*/
	SunXK_Paste,  	NoSymbol,  	XK_L8,		NoSymbol,	/* 73*/
	XK_End, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 74*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 75*/
	XK_Control_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 76*/
	XK_A,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 77*/
	XK_S,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 78*/
	XK_D,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 79*/
	XK_F,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 80*/
	XK_G,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 81*/
	XK_H,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 82*/
	XK_J,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 83*/
	XK_K,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 84*/
	XK_L,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 85*/
	XK_Ooblique,	NoSymbol,	NoSymbol,	NoSymbol,	/* 86*/
	XK_AE,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 87*/
	XK_apostrophe,	XK_asterisk,	XK_grave,	NoSymbol,	/* 88*/
	XK_Return,	NoSymbol,	NoSymbol,	NoSymbol,	/* 89*/
	XK_KP_Enter,	NoSymbol,	NoSymbol,	NoSymbol,	/* 90*/
	XK_KP_Left,	XK_KP_4, 	NoSymbol,	NoSymbol,	/* 91*/
	NoSymbol, 	XK_KP_5, 	NoSymbol,	NoSymbol,	/* 92*/
	XK_KP_Right,	XK_KP_6, 	NoSymbol,	NoSymbol,	/* 93*/
	XK_KP_Insert,	XK_KP_0,	NoSymbol,	NoSymbol,	/* 94*/
	SunXK_Find,  	NoSymbol,  	XK_L9,		NoSymbol,	/* 95*/
	XK_Prior,	NoSymbol,	NoSymbol,	NoSymbol,	/* 96*/
	SunXK_Cut, 	NoSymbol, 	XK_L10,		NoSymbol,	/* 97*/
	XK_Num_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 98*/
	XK_Shift_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 99*/
	XK_Z,   	NoSymbol,	NoSymbol,	NoSymbol,	/*100*/
	XK_X,   	NoSymbol,	NoSymbol,	NoSymbol,	/*101*/
	XK_C,   	NoSymbol,	NoSymbol,	NoSymbol,	/*102*/
	XK_V,   	NoSymbol,	NoSymbol,	NoSymbol,	/*103*/
	XK_B,   	NoSymbol,	NoSymbol,	NoSymbol,	/*104*/
	XK_N,   	NoSymbol,	NoSymbol,	NoSymbol,	/*105*/
	XK_M,   	NoSymbol,	NoSymbol,	NoSymbol,	/*106*/
	XK_comma,	XK_semicolon,	NoSymbol,	NoSymbol,	/*107*/
	XK_period,	XK_colon,	NoSymbol,	NoSymbol,	/*108*/
	XK_minus,	XK_underscore,	NoSymbol,	NoSymbol,	/*109*/
	XK_Shift_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*110*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*111*/
	XK_KP_End, 	XK_KP_1, 	NoSymbol,	NoSymbol, 	/*112*/
	XK_KP_Down,	XK_KP_2, 	NoSymbol,	NoSymbol,	/*113*/
	XK_KP_Next, 	XK_KP_3, 	NoSymbol,	NoSymbol,	/*114*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*115*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*116*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*117*/
	XK_Help,	NoSymbol,	NoSymbol,	NoSymbol,	/*118*/
	XK_Caps_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/*119*/
	XK_Meta_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*120*/
	XK_space,	NoSymbol,	NoSymbol,	NoSymbol,	/*121*/
	XK_Meta_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*122*/
	XK_Next,	NoSymbol,	NoSymbol,	NoSymbol,	/*123*/
	XK_less,	XK_greater,	NoSymbol,	NoSymbol,	/*124*/
	XK_KP_Add,	NoSymbol,	NoSymbol,	NoSymbol,	/*125*/
};

#define Norway5Modmap Generic5Modmap

#else

#define Norway5Keymap NULL
#define Norway5Modmap NULL

#endif /* NORWAY5 */


#ifdef PORT4

static KeySym Portugal4Keymap[] = {
	SunXK_Stop,  	NoSymbol,  	XK_L1,		NoSymbol,	/*  1*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  2*/
	SunXK_Again,  	NoSymbol,  	XK_L2,		NoSymbol,	/*  3*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  4*/
	XK_F1,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  5*/
	XK_F2,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  6*/
	XK_F10, 	NoSymbol,	NoSymbol,	NoSymbol,	/*  7*/
	XK_F3,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  8*/
	XK_F11,		NoSymbol,	SunXK_F36,	NoSymbol,	/*  9*/
	XK_F4,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 10*/
	XK_F12,		NoSymbol,	SunXK_F37,	NoSymbol,	/* 11*/
	XK_F5,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 12*/
	SunXK_AltGraph,	NoSymbol,	NoSymbol,	NoSymbol,	/* 13*/
	XK_F6,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 14*/
	XK_bracketright,XK_braceright,	XK_guillemotright,NoSymbol,	/* 15*/
	XK_F7,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 16*/
	XK_F8,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 17*/
	XK_F9,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 18*/
	XK_Alt_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 19*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 20*/
	XK_Pause,  	NoSymbol,  	XK_R1,		NoSymbol,	/* 21*/
	XK_Print,  	NoSymbol,  	XK_R2,		NoSymbol,	/* 22*/
	XK_Break,  	XK_Scroll_Lock,	XK_R3,		XK_Break,	/* 23*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 24*/
	SunXK_Props,  	NoSymbol,  	XK_L3,		NoSymbol,	/* 25*/
	SunXK_Undo,  	NoSymbol,  	XK_L4,		NoSymbol,	/* 26*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 27*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 28*/
	XK_Escape,	NoSymbol,	NoSymbol,	NoSymbol,	/* 29*/
	XK_1,   	XK_exclam,	NoSymbol,	NoSymbol,	/* 30*/
	XK_2,   	XK_quotedbl,	XK_at,  	NoSymbol,	/* 31*/
	XK_3,   	XK_numbersign,	XK_sterling,	NoSymbol,	/* 32*/
	XK_4,   	XK_dollar,	XK_section,	NoSymbol,	/* 33*/
	XK_5,   	XK_percent,	NoSymbol,	NoSymbol,	/* 34*/
	XK_6,   	XK_ampersand,	XK_notsign,	NoSymbol,	/* 35*/
	XK_7,   	XK_slash,	NoSymbol,	NoSymbol,	/* 36*/
	XK_8,   	XK_parenleft,	NoSymbol,	NoSymbol,	/* 37*/
	XK_9,   	XK_parenright,	XK_backslash,	NoSymbol,	/* 38*/
	XK_0,   	XK_equal,	XK_bar, 	NoSymbol,	/* 39*/
	XK_apostrophe,	XK_question,	XK_grave,	NoSymbol,	/* 40*/
	XK_exclamdown,	XK_questiondown,NoSymbol,	NoSymbol,	/* 41*/
	SunXK_FA_Tilde,	SunXK_FA_Circum,XK_asciicircum,	NoSymbol,	/* 42*/
	XK_BackSpace,	NoSymbol,	NoSymbol,	NoSymbol,	/* 43*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 44*/
	XK_KP_Equal,  	NoSymbol,  	XK_R4,		NoSymbol,	/* 45*/
	XK_KP_Divide,  	NoSymbol,  	XK_R5,		NoSymbol,	/* 46*/
	XK_KP_Multiply,	NoSymbol,  	XK_R6,		NoSymbol,	/* 47*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 48*/
	SunXK_Front,  	NoSymbol,  	XK_L5,		NoSymbol,	/* 49*/
	XK_KP_Delete,	XK_KP_Decimal,	NoSymbol,	NoSymbol,	/* 50*/
	SunXK_Copy,  	NoSymbol,  	XK_L6,		NoSymbol,	/* 51*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 52*/
	XK_Tab, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 53*/
	XK_Q,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 54*/
	XK_W,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 55*/
	XK_E,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 56*/
	XK_R,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 57*/
	XK_T,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 58*/
	XK_Y,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 59*/
	XK_U,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 60*/
	XK_I,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 61*/
	XK_O,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 62*/
	XK_P,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 63*/
	SunXK_FA_Diaeresis,XK_asterisk,	XK_plus,	NoSymbol,	/* 64*/
	SunXK_FA_Acute,	SunXK_FA_Grave,	XK_asciitilde,	NoSymbol,	/* 65*/
	XK_Delete,	NoSymbol,	NoSymbol,	NoSymbol,	/* 66*/
	SunXK_Compose,	NoSymbol,	NoSymbol,	NoSymbol,	/* 67*/
	XK_KP_Home,  	XK_KP_7,  	XK_R7,		NoSymbol,	/* 68*/
	XK_KP_Up,  	XK_KP_8,  	XK_R8,		NoSymbol,	/* 69*/
	XK_KP_Prior,  	XK_KP_9,  	XK_R9,		NoSymbol,	/* 70*/
	XK_KP_Subtract,	NoSymbol,	NoSymbol,	NoSymbol,	/* 71*/
	SunXK_Open,  	NoSymbol,  	XK_L7,		NoSymbol,	/* 72*/
	SunXK_Paste,  	NoSymbol,  	XK_L8,		NoSymbol,	/* 73*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 74*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 75*/
	XK_Caps_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 76*/
	XK_A,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 77*/
	XK_S,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 78*/
	XK_D,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 79*/
	XK_F,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 80*/
	XK_G,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 81*/
	XK_H,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 82*/
	XK_J,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 83*/
	XK_K,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 84*/
	XK_L,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 85*/
	XK_Ccedilla,	NoSymbol,	NoSymbol,	NoSymbol,	/* 86*/
	XK_masculine,	XK_ordfeminine,	NoSymbol,	NoSymbol,	/* 87*/
	XK_bracketleft,	XK_braceleft,	XK_guillemotleft,NoSymbol,	/* 88*/
	XK_Return,	NoSymbol,	NoSymbol,	NoSymbol,	/* 89*/
	XK_KP_Enter,	NoSymbol,	NoSymbol,	NoSymbol,	/* 90*/
	XK_KP_Left,	XK_KP_4, 	XK_R10,		NoSymbol,	/* 91*/
	NoSymbol, 	XK_KP_5, 	XK_R11,		NoSymbol,	/* 92*/
	XK_KP_Right,	XK_KP_6, 	XK_R12,		NoSymbol,	/* 93*/
	XK_KP_Insert,	XK_KP_0,	NoSymbol,	NoSymbol,	/* 94*/
	SunXK_Find,  	NoSymbol,  	XK_L9,		NoSymbol,	/* 95*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 96*/
	SunXK_Cut, 	NoSymbol, 	XK_L10,		NoSymbol,	/* 97*/
	XK_Num_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 98*/
	XK_Shift_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 99*/
	XK_Z,   	NoSymbol,	NoSymbol,	NoSymbol,	/*100*/
	XK_X,   	NoSymbol,	NoSymbol,	NoSymbol,	/*101*/
	XK_C,   	NoSymbol,	NoSymbol,	NoSymbol,	/*102*/
	XK_V,   	NoSymbol,	NoSymbol,	NoSymbol,	/*103*/
	XK_B,   	NoSymbol,	NoSymbol,	NoSymbol,	/*104*/
	XK_N,   	NoSymbol,	NoSymbol,	NoSymbol,	/*105*/
	XK_M,   	NoSymbol,	NoSymbol,	NoSymbol,	/*106*/
	XK_comma,	XK_semicolon,	NoSymbol,	NoSymbol,	/*107*/
	XK_period,	XK_colon,	NoSymbol,	NoSymbol,	/*108*/
	XK_minus,	XK_underscore,	NoSymbol,	NoSymbol,	/*109*/
	XK_Shift_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*110*/
	XK_Linefeed,	NoSymbol,	NoSymbol,	NoSymbol,	/*111*/
	XK_KP_End, 	XK_KP_1, 	XK_R13,		NoSymbol, 	/*112*/
	XK_KP_Down,	XK_KP_2, 	XK_R14,		NoSymbol,	/*113*/
	XK_KP_Next, 	XK_KP_3, 	XK_R15,		NoSymbol,	/*114*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*115*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*116*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*117*/
	XK_Help,	NoSymbol,	NoSymbol,	NoSymbol,	/*118*/
	XK_Control_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*119*/
	XK_Meta_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*120*/
	XK_space,	NoSymbol,	NoSymbol,	NoSymbol,	/*121*/
	XK_Meta_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*122*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*123*/
	XK_less,	XK_greater,	NoSymbol,	NoSymbol,	/*124*/
	XK_KP_Add,	NoSymbol,	NoSymbol,	NoSymbol,	/*125*/
};

#define Portugal4Modmap ItNorPortSp4Modmap

#else

#define Portugal4Keymap NULL
#define Portugal4Modmap NULL

#endif /* PORTUGAL4 */


#ifdef PORT5

static KeySym Portugal5Keymap[] = {
	SunXK_Stop,  	NoSymbol,  	XK_L1,		NoSymbol,	/*  1*/
	SunXK_AudioLowerVolume,SunXK_VideoLowerBrightness,NoSymbol,	NoSymbol,	/*  2*/
	SunXK_Again,  	NoSymbol,  	XK_L2,		NoSymbol,	/*  3*/
	SunXK_AudioRaiseVolume,SunXK_VideoRaiseBrightness,NoSymbol,	NoSymbol,	/*  4*/
	XK_F1,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  5*/
	XK_F2,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  6*/
	XK_F10, 	NoSymbol,	NoSymbol,	NoSymbol,	/*  7*/
	XK_F3,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  8*/
	XK_F11,		NoSymbol,	SunXK_F36,	NoSymbol,	/*  9*/
	XK_F4,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 10*/
	XK_F12,		NoSymbol,	SunXK_F37,	NoSymbol,	/* 11*/
	XK_F5,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 12*/
	SunXK_AltGraph,	NoSymbol,	NoSymbol,	NoSymbol,	/* 13*/
	XK_F6,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 14*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 15*/
	XK_F7,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 16*/
	XK_F8,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 17*/
	XK_F9,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 18*/
	XK_Alt_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 19*/
	XK_Up,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 20*/
	XK_Pause,  	NoSymbol,  	XK_Break,	NoSymbol,	/* 21*/
	XK_Print,  	NoSymbol,  	XK_Sys_Req,	SunXK_Sys_Req,	/* 22*/
	XK_Scroll_Lock,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 23*/
	XK_Left,	NoSymbol,	NoSymbol,	NoSymbol,	/* 24*/
	SunXK_Props,  	NoSymbol,  	XK_L3,		NoSymbol,	/* 25*/
	SunXK_Undo,  	NoSymbol,  	XK_L4,		NoSymbol,	/* 26*/
	XK_Down,	NoSymbol,	NoSymbol,	NoSymbol,	/* 27*/
	XK_Right,	NoSymbol,	NoSymbol,	NoSymbol,	/* 28*/
	XK_Escape,	NoSymbol,	NoSymbol,	NoSymbol,	/* 29*/
	XK_1,   	XK_exclam,	NoSymbol,	NoSymbol,	/* 30*/
	XK_2,   	XK_quotedbl,	XK_at,  	NoSymbol,	/* 31*/
	XK_3,   	XK_numbersign,	XK_sterling,	NoSymbol,	/* 32*/
	XK_4,   	XK_dollar,	XK_section,	NoSymbol,	/* 33*/
	XK_5,   	XK_percent,	XK_asciitilde,	NoSymbol,	/* 34*/
	XK_6,   	XK_ampersand,	XK_asciicircum,	NoSymbol,	/* 35*/
	XK_7,   	XK_slash,	XK_braceleft,	NoSymbol,	/* 36*/
	XK_8,   	XK_parenleft,	XK_bracketleft,	NoSymbol,	/* 37*/
	XK_9,   	XK_parenright,	XK_bracketright,NoSymbol,	/* 38*/
	XK_0,   	XK_equal,	XK_braceright,	NoSymbol,	/* 39*/
	XK_apostrophe,	XK_question,	XK_grave,	NoSymbol,	/* 40*/
	XK_guillemotleft,XK_guillemotright,NoSymbol,	NoSymbol,	/* 41*/
	XK_backslash,	XK_bar, 	NoSymbol,	NoSymbol,	/* 42*/
	XK_BackSpace,	NoSymbol,	NoSymbol,	NoSymbol,	/* 43*/
	XK_Insert,	NoSymbol,	NoSymbol,	NoSymbol,	/* 44*/
	SunXK_AudioMute,SunXK_VideoDegauss,NoSymbol,	NoSymbol,	/* 45*/
	XK_KP_Divide,  	NoSymbol,  	NoSymbol,	NoSymbol,	/* 46*/
	XK_KP_Multiply,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 47*/
	SunXK_PowerSwitch,SunXK_PowerSwitchShift,	NoSymbol,	NoSymbol,	/* 48*/
	SunXK_Front,  	NoSymbol,  	XK_L5,		NoSymbol,	/* 49*/
	XK_KP_Delete,	XK_KP_Decimal,	NoSymbol,	NoSymbol,	/* 50*/
	SunXK_Copy,  	NoSymbol,  	XK_L6,		NoSymbol,	/* 51*/
	XK_Home,	NoSymbol,	NoSymbol,	NoSymbol,	/* 52*/
	XK_Tab, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 53*/
	XK_Q,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 54*/
	XK_W,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 55*/
	XK_E,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 56*/
	XK_R,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 57*/
	XK_T,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 58*/
	XK_Y,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 59*/
	XK_U,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 60*/
	XK_I,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 61*/
	XK_O,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 62*/
	XK_P,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 63*/
	XK_plus,	XK_asterisk,	SunXK_FA_Diaeresis,NoSymbol,	/* 64*/
	SunXK_FA_Acute,	SunXK_FA_Grave,	NoSymbol,	NoSymbol,	/* 65*/
	XK_Delete,	NoSymbol,	NoSymbol,	NoSymbol,	/* 66*/
	SunXK_Compose,	NoSymbol,	NoSymbol,	NoSymbol,	/* 67*/
	XK_KP_Home,  	XK_KP_7,  	NoSymbol,	NoSymbol,	/* 68*/
	XK_KP_Up,  	XK_KP_8,  	NoSymbol,	NoSymbol,	/* 69*/
	XK_KP_Prior,  	XK_KP_9,  	NoSymbol,	NoSymbol,	/* 70*/
	XK_KP_Subtract,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 71*/
	SunXK_Open,  	NoSymbol,  	XK_L7,		NoSymbol,	/* 72*/
	SunXK_Paste,  	NoSymbol,  	XK_L8,		NoSymbol,	/* 73*/
	XK_End, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 74*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 75*/
	XK_Control_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 76*/
	XK_A,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 77*/
	XK_S,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 78*/
	XK_D,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 79*/
	XK_F,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 80*/
	XK_G,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 81*/
	XK_H,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 82*/
	XK_J,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 83*/
	XK_K,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 84*/
	XK_L,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 85*/
	XK_Ccedilla,	NoSymbol,	NoSymbol,	NoSymbol,	/* 86*/
	XK_masculine,	XK_ordfeminine,	NoSymbol,	NoSymbol,	/* 87*/
	SunXK_FA_Tilde,	SunXK_FA_Circum,NoSymbol,	NoSymbol,	/* 88*/
	XK_Return,	NoSymbol,	NoSymbol,	NoSymbol,	/* 89*/
	XK_KP_Enter,	NoSymbol,	NoSymbol,	NoSymbol,	/* 90*/
	XK_KP_Left,	XK_KP_4, 	NoSymbol,	NoSymbol,	/* 91*/
	NoSymbol, 	XK_KP_5, 	NoSymbol,	NoSymbol,	/* 92*/
	XK_KP_Right,	XK_KP_6, 	NoSymbol,	NoSymbol,	/* 93*/
	XK_KP_Insert,	XK_KP_0,	NoSymbol,	NoSymbol,	/* 94*/
	SunXK_Find,  	NoSymbol,  	XK_L9,		NoSymbol,	/* 95*/
	XK_Prior,	NoSymbol,	NoSymbol,	NoSymbol,	/* 96*/
	SunXK_Cut, 	NoSymbol, 	XK_L10,		NoSymbol,	/* 97*/
	XK_Num_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 98*/
	XK_Shift_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 99*/
	XK_Z,   	NoSymbol,	NoSymbol,	NoSymbol,	/*100*/
	XK_X,   	NoSymbol,	NoSymbol,	NoSymbol,	/*101*/
	XK_C,   	NoSymbol,	NoSymbol,	NoSymbol,	/*102*/
	XK_V,   	NoSymbol,	NoSymbol,	NoSymbol,	/*103*/
	XK_B,   	NoSymbol,	NoSymbol,	NoSymbol,	/*104*/
	XK_N,   	NoSymbol,	NoSymbol,	NoSymbol,	/*105*/
	XK_M,   	NoSymbol,	NoSymbol,	NoSymbol,	/*106*/
	XK_comma,	XK_semicolon,	NoSymbol,	NoSymbol,	/*107*/
	XK_period,	XK_colon,	NoSymbol,	NoSymbol,	/*108*/
	XK_minus,	XK_underscore,	NoSymbol,	NoSymbol,	/*109*/
	XK_Shift_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*110*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*111*/
	XK_KP_End, 	XK_KP_1, 	NoSymbol,	NoSymbol, 	/*112*/
	XK_KP_Down,	XK_KP_2, 	NoSymbol,	NoSymbol,	/*113*/
	XK_KP_Next, 	XK_KP_3, 	NoSymbol,	NoSymbol,	/*114*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*115*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*116*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*117*/
	XK_Help,	NoSymbol,	NoSymbol,	NoSymbol,	/*118*/
	XK_Caps_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/*119*/
	XK_Meta_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*120*/
	XK_space,	NoSymbol,	NoSymbol,	NoSymbol,	/*121*/
	XK_Meta_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*122*/
	XK_Next,	NoSymbol,	NoSymbol,	NoSymbol,	/*123*/
	XK_less,	XK_greater,	NoSymbol,	NoSymbol,	/*124*/
	XK_KP_Add,	NoSymbol,	NoSymbol,	NoSymbol,	/*125*/
};

#define Portugal5Modmap Generic5Modmap

#else

#define Portugal5Keymap NULL
#define Portugal5Modmap NULL

#endif /* PORTUGAL5 */


#ifdef SPAIN5

static KeySym Spain5Keymap[] = {
	SunXK_Stop,  	NoSymbol,  	XK_L1,		NoSymbol,	/*  1*/
	SunXK_AudioLowerVolume,SunXK_VideoLowerBrightness,NoSymbol,	NoSymbol,	/*  2*/
	SunXK_Again,  	NoSymbol,  	XK_L2,		NoSymbol,	/*  3*/
	SunXK_AudioRaiseVolume,SunXK_VideoRaiseBrightness,NoSymbol,	NoSymbol,	/*  4*/
	XK_F1,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  5*/
	XK_F2,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  6*/
	XK_F10, 	NoSymbol,	NoSymbol,	NoSymbol,	/*  7*/
	XK_F3,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  8*/
	XK_F11,		NoSymbol,	SunXK_F36,	NoSymbol,	/*  9*/
	XK_F4,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 10*/
	XK_F12,		NoSymbol,	SunXK_F37,	NoSymbol,	/* 11*/
	XK_F5,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 12*/
	SunXK_AltGraph,	NoSymbol,	NoSymbol,	NoSymbol,	/* 13*/
	XK_F6,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 14*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 15*/
	XK_F7,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 16*/
	XK_F8,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 17*/
	XK_F9,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 18*/
	XK_Alt_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 19*/
	XK_Up,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 20*/
	XK_Pause,  	NoSymbol,  	XK_Break,	NoSymbol,	/* 21*/
	XK_Print,  	NoSymbol,  	XK_Sys_Req,	SunXK_Sys_Req,	/* 22*/
	XK_Scroll_Lock,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 23*/
	XK_Left,	NoSymbol,	NoSymbol,	NoSymbol,	/* 24*/
	SunXK_Props,  	NoSymbol,  	XK_L3,		NoSymbol,	/* 25*/
	SunXK_Undo,  	NoSymbol,  	XK_L4,		NoSymbol,	/* 26*/
	XK_Down,	NoSymbol,	NoSymbol,	NoSymbol,	/* 27*/
	XK_Right,	NoSymbol,	NoSymbol,	NoSymbol,	/* 28*/
	XK_Escape,	NoSymbol,	NoSymbol,	NoSymbol,	/* 29*/
	XK_1,   	XK_exclam,	XK_bar, 	NoSymbol,	/* 30*/
	XK_2,   	XK_quotedbl,	XK_at,  	NoSymbol,	/* 31*/
	XK_3,   	XK_periodcentered,XK_numbersign,NoSymbol,	/* 32*/
	XK_4,   	XK_dollar,	XK_asciicircum,	NoSymbol,	/* 33*/
	XK_5,   	XK_percent,	XK_asciitilde,	NoSymbol,	/* 34*/
	XK_6,   	XK_ampersand,	XK_notsign,	NoSymbol,	/* 35*/
	XK_7,   	XK_slash,	NoSymbol,	NoSymbol,	/* 36*/
	XK_8,   	XK_parenleft,	NoSymbol,	NoSymbol,	/* 37*/
	XK_9,   	XK_parenright,	NoSymbol,	NoSymbol,	/* 38*/
	XK_0,   	XK_equal,	NoSymbol,	NoSymbol,	/* 39*/
	XK_apostrophe,	XK_question,	XK_grave,	NoSymbol,	/* 40*/
	XK_exclamdown,	XK_questiondown,NoSymbol,	NoSymbol,	/* 41*/
	XK_masculine,	XK_ordfeminine,	XK_backslash,	NoSymbol,	/* 42*/
	XK_BackSpace,	NoSymbol,	NoSymbol,	NoSymbol,	/* 43*/
	XK_Insert,	NoSymbol,	NoSymbol,	NoSymbol,	/* 44*/
	SunXK_AudioMute,SunXK_VideoDegauss,NoSymbol,	NoSymbol,	/* 45*/
	XK_KP_Divide,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 46*/
	XK_KP_Multiply,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 47*/
	SunXK_PowerSwitch,SunXK_PowerSwitchShift,	NoSymbol,	NoSymbol,	/* 48*/
	SunXK_Front,  	NoSymbol,  	XK_L5,		NoSymbol,	/* 49*/
	XK_KP_Delete,	XK_KP_Decimal,	NoSymbol,	NoSymbol,	/* 50*/
	SunXK_Copy,  	NoSymbol,  	XK_L6,		NoSymbol,	/* 51*/
	XK_Home,	NoSymbol,	NoSymbol,	NoSymbol,	/* 52*/
	XK_Tab, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 53*/
	XK_Q,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 54*/
	XK_W,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 55*/
	XK_E,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 56*/
	XK_R,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 57*/
	XK_T,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 58*/
	XK_Y,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 59*/
	XK_U,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 60*/
	XK_I,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 61*/
	XK_O,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 62*/
	XK_P,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 63*/
	SunXK_FA_Grave,	SunXK_FA_Circum,XK_bracketleft,	NoSymbol,	/* 64*/
	XK_plus,	XK_asterisk,	XK_bracketright,NoSymbol,	/* 65*/
	XK_Delete,	NoSymbol,	NoSymbol,	NoSymbol,	/* 66*/
	SunXK_Compose,	NoSymbol,	NoSymbol,	NoSymbol,	/* 67*/
	XK_KP_Home,  	XK_KP_7,  	NoSymbol,	NoSymbol,	/* 68*/
	XK_KP_Up,  	XK_KP_8,  	NoSymbol,	NoSymbol,	/* 69*/
	XK_KP_Prior,  	XK_KP_9,  	NoSymbol,	NoSymbol,	/* 70*/
	XK_KP_Subtract,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 71*/
	SunXK_Open,  	NoSymbol,  	XK_L7,		NoSymbol,	/* 72*/
	SunXK_Paste,  	NoSymbol,  	XK_L8,		NoSymbol,	/* 73*/
	XK_End, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 74*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 75*/
	XK_Control_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 76*/
	XK_A,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 77*/
	XK_S,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 78*/
	XK_D,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 79*/
	XK_F,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 80*/
	XK_G,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 81*/
	XK_H,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 82*/
	XK_J,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 83*/
	XK_K,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 84*/
	XK_L,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 85*/
	XK_Ntilde,	NoSymbol,	NoSymbol,	NoSymbol,	/* 86*/
	SunXK_FA_Acute,	SunXK_FA_Diaeresis,XK_braceleft,NoSymbol,	/* 87*/
	XK_ccedilla,	XK_Ccedilla,	XK_braceright,	NoSymbol,	/* 88*/
	XK_Return,	NoSymbol,	NoSymbol,	NoSymbol,	/* 89*/
	XK_KP_Enter,	NoSymbol,	NoSymbol,	NoSymbol,	/* 90*/
	XK_KP_Left,	XK_KP_4, 	NoSymbol,	NoSymbol,	/* 91*/
	NoSymbol, 	XK_KP_5, 	NoSymbol,	NoSymbol,	/* 92*/
	XK_KP_Right,	XK_KP_6, 	NoSymbol,	NoSymbol,	/* 93*/
	XK_KP_Insert,	XK_KP_0,	NoSymbol,	NoSymbol,	/* 94*/
	SunXK_Find,  	NoSymbol,  	XK_L9,		NoSymbol,	/* 95*/
	XK_Prior,	NoSymbol,	NoSymbol,	NoSymbol,	/* 96*/
	SunXK_Cut, 	NoSymbol, 	XK_L10,		NoSymbol,	/* 97*/
	XK_Num_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 98*/
	XK_Shift_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 99*/
	XK_Z,   	NoSymbol,	NoSymbol,	NoSymbol,	/*100*/
	XK_X,   	NoSymbol,	NoSymbol,	NoSymbol,	/*101*/
	XK_C,   	NoSymbol,	NoSymbol,	NoSymbol,	/*102*/
	XK_V,   	NoSymbol,	NoSymbol,	NoSymbol,	/*103*/
	XK_B,   	NoSymbol,	NoSymbol,	NoSymbol,	/*104*/
	XK_N,   	NoSymbol,	NoSymbol,	NoSymbol,	/*105*/
	XK_M,   	NoSymbol,	NoSymbol,	NoSymbol,	/*106*/
	XK_comma,	XK_semicolon,	NoSymbol,	NoSymbol,	/*107*/
	XK_period,	XK_colon,	NoSymbol,	NoSymbol,	/*108*/
	XK_minus,	XK_underscore,	NoSymbol,	NoSymbol,	/*109*/
	XK_Shift_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*110*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*111*/
	XK_KP_End, 	XK_KP_1, 	NoSymbol,	NoSymbol, 	/*112*/
	XK_KP_Down,	XK_KP_2, 	NoSymbol,	NoSymbol,	/*113*/
	XK_KP_Next, 	XK_KP_3, 	NoSymbol,	NoSymbol,	/*114*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*115*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*116*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*117*/
	XK_Help,	NoSymbol,	NoSymbol,	NoSymbol,	/*118*/
	XK_Caps_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/*119*/
	XK_Meta_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*120*/
	XK_space,	NoSymbol,	NoSymbol,	NoSymbol,	/*121*/
	XK_Meta_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*122*/
	XK_Next,	NoSymbol,	NoSymbol,	NoSymbol,	/*123*/
	XK_less,	XK_greater,	NoSymbol,	NoSymbol,	/*124*/
	XK_KP_Add,	NoSymbol,	NoSymbol,	NoSymbol,	/*125*/
};

#define Spain5Modmap Generic5Modmap

#else

#define Spain5Keymap NULL
#define Spain5Modmap NULL

#endif /* SPAIN5 */


#ifdef SPAINLATAM4

static KeySym SpainLatAm4Keymap[] = {
	SunXK_Stop,  	NoSymbol,  	XK_L1,		NoSymbol,	/*  1*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  2*/
	SunXK_Again,  	NoSymbol,  	XK_L2,		NoSymbol,	/*  3*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  4*/
	XK_F1,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  5*/
	XK_F2,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  6*/
	XK_F10, 	NoSymbol,	NoSymbol,	NoSymbol,	/*  7*/
	XK_F3,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  8*/
	XK_F11,		NoSymbol,	SunXK_F36,	NoSymbol,	/*  9*/
	XK_F4,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 10*/
	XK_F12,		NoSymbol,	SunXK_F37,	NoSymbol,	/* 11*/
	XK_F5,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 12*/
	SunXK_AltGraph,	NoSymbol,	NoSymbol,	NoSymbol,	/* 13*/
	XK_F6,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 14*/
	XK_bracketright,XK_braceright,	XK_guillemotright,NoSymbol,	/* 15*/
	XK_F7,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 16*/
	XK_F8,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 17*/
	XK_F9,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 18*/
	XK_Alt_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 19*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 20*/
	XK_Pause,  	NoSymbol,  	XK_R1,		NoSymbol,	/* 21*/
	XK_Print,  	NoSymbol,  	XK_R2,		NoSymbol,	/* 22*/
	XK_Scroll_Lock,	NoSymbol,  	XK_R3,		XK_Break,	/* 23*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 24*/
	SunXK_Props,  	NoSymbol,  	XK_L3,		NoSymbol,	/* 25*/
	SunXK_Undo,  	NoSymbol,  	XK_L4,		NoSymbol,	/* 26*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 27*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 28*/
	XK_Escape,	NoSymbol,	NoSymbol,	NoSymbol,	/* 29*/
	XK_1,   	XK_exclam,	NoSymbol,	NoSymbol,	/* 30*/
	XK_2,   	XK_quotedbl,	XK_at,  	NoSymbol,	/* 31*/
	XK_3,   	XK_periodcentered,XK_numbersign,NoSymbol,	/* 32*/
	XK_4,   	XK_dollar,	NoSymbol,	NoSymbol,	/* 33*/
	XK_5,   	XK_percent,	XK_degree,	NoSymbol,	/* 34*/
	XK_6,   	XK_ampersand,	XK_notsign,	NoSymbol,	/* 35*/
	XK_7,   	XK_slash,	NoSymbol,	NoSymbol,	/* 36*/
	XK_8,   	XK_parenleft,	NoSymbol,	NoSymbol,	/* 37*/
	XK_9,   	XK_parenright,	XK_backslash,	NoSymbol,	/* 38*/
	XK_0,   	XK_equal,	XK_bar, 	NoSymbol,	/* 39*/
	XK_apostrophe,	XK_question,	XK_grave,	NoSymbol,	/* 40*/
	XK_exclamdown,	XK_questiondown,NoSymbol,	NoSymbol,	/* 41*/
	XK_Ccedilla,	NoSymbol,	NoSymbol,	NoSymbol,	/* 42*/
	XK_BackSpace,	NoSymbol,	NoSymbol,	NoSymbol,	/* 43*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 44*/
	XK_KP_Equal,  	NoSymbol,  	XK_R4,		NoSymbol,	/* 45*/
	XK_KP_Divide,  	NoSymbol,  	XK_R5,		NoSymbol,	/* 46*/
	XK_KP_Multiply,	NoSymbol,  	XK_R6,		NoSymbol,	/* 47*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 48*/
	SunXK_Front,  	NoSymbol,  	XK_L5,		NoSymbol,	/* 49*/
	XK_KP_Delete,	XK_KP_Decimal,	NoSymbol,	NoSymbol,	/* 50*/
	SunXK_Copy,  	NoSymbol,  	XK_L6,		NoSymbol,	/* 51*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 52*/
	XK_Tab, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 53*/
	XK_Q,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 54*/
	XK_W,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 55*/
	XK_E,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 56*/
	XK_R,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 57*/
	XK_T,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 58*/
	XK_Y,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 59*/
	XK_U,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 60*/
	XK_I,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 61*/
	XK_O,   	NoSymbol,	XK_masculine,	NoSymbol,	/* 62*/
	XK_P,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 63*/
	SunXK_FA_Grave,	SunXK_FA_Circum,XK_asciicircum,	NoSymbol,	/* 64*/
	XK_plus,	XK_asterisk,	XK_asciitilde,	NoSymbol,	/* 65*/
	XK_Delete,	NoSymbol,	NoSymbol,	NoSymbol,	/* 66*/
	SunXK_Compose,	NoSymbol,	NoSymbol,	NoSymbol,	/* 67*/
	XK_KP_Home,  	XK_KP_7,  	XK_R7,		NoSymbol,	/* 68*/
	XK_KP_Up,  	XK_KP_8,  	XK_R8,		NoSymbol,	/* 69*/
	XK_KP_Prior,  	XK_KP_9,  	XK_R9,		NoSymbol,	/* 70*/
	XK_KP_Subtract,	NoSymbol,	NoSymbol,	NoSymbol,	/* 71*/
	SunXK_Open,  	NoSymbol,  	XK_L7,		NoSymbol,	/* 72*/
	SunXK_Paste,  	NoSymbol,  	XK_L8,		NoSymbol,	/* 73*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 74*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 75*/
	XK_Caps_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 76*/
	XK_A,   	NoSymbol,	XK_ordfeminine,	NoSymbol,	/* 77*/
	XK_S,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 78*/
	XK_D,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 79*/
	XK_F,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 80*/
	XK_G,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 81*/
	XK_H,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 82*/
	XK_J,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 83*/
	XK_K,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 84*/
	XK_L,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 85*/
	XK_Ntilde,	NoSymbol,	NoSymbol,	NoSymbol,	/* 86*/
	SunXK_FA_Acute,	SunXK_FA_Diaeresis,NoSymbol,	NoSymbol,	/* 87*/
	XK_bracketleft,	XK_braceleft,	XK_guillemotleft,NoSymbol,	/* 88*/
	XK_Return,	NoSymbol,	NoSymbol,	NoSymbol,	/* 89*/
	XK_KP_Enter,	NoSymbol,	NoSymbol,	NoSymbol,	/* 90*/
	XK_KP_Left,	XK_KP_4, 	XK_R10,		NoSymbol,	/* 91*/
	NoSymbol, 	XK_KP_5, 	XK_R11,		NoSymbol,	/* 92*/
	XK_KP_Right,	XK_KP_6, 	XK_R12,		NoSymbol,	/* 93*/
	XK_KP_Insert,	XK_KP_0,	NoSymbol,	NoSymbol,	/* 94*/
	SunXK_Find,  	NoSymbol,  	XK_L9,		NoSymbol,	/* 95*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 96*/
	SunXK_Cut, 	NoSymbol, 	XK_L10,		NoSymbol,	/* 97*/
	XK_Num_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 98*/
	XK_Shift_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 99*/
	XK_Z,   	NoSymbol,	NoSymbol,	NoSymbol,	/*100*/
	XK_X,   	NoSymbol,	NoSymbol,	NoSymbol,	/*101*/
	XK_C,   	NoSymbol,	NoSymbol,	NoSymbol,	/*102*/
	XK_V,   	NoSymbol,	NoSymbol,	NoSymbol,	/*103*/
	XK_B,   	NoSymbol,	NoSymbol,	NoSymbol,	/*104*/
	XK_N,   	NoSymbol,	NoSymbol,	NoSymbol,	/*105*/
	XK_M,   	NoSymbol,	NoSymbol,	NoSymbol,	/*106*/
	XK_comma,	XK_semicolon,	NoSymbol,	NoSymbol,	/*107*/
	XK_period,	XK_colon,	NoSymbol,	NoSymbol,	/*108*/
	XK_minus,	XK_underscore,	NoSymbol,	NoSymbol,	/*109*/
	XK_Shift_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*110*/
	XK_Linefeed,	NoSymbol,	NoSymbol,	NoSymbol,	/*111*/
	XK_KP_End, 	XK_KP_1, 	XK_R13,		NoSymbol, 	/*112*/
	XK_KP_Down,	XK_KP_2, 	XK_R14,		NoSymbol,	/*113*/
	XK_KP_Next, 	XK_KP_3, 	XK_R15,		NoSymbol,	/*114*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*115*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*116*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*117*/
	XK_Help,	NoSymbol,	NoSymbol,	NoSymbol,	/*118*/
	XK_Control_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*119*/
	XK_Meta_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*120*/
	XK_space,	NoSymbol,	NoSymbol,	NoSymbol,	/*121*/
	XK_Meta_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*122*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*123*/
	XK_less,	XK_greater,	NoSymbol,	NoSymbol,	/*124*/
	XK_KP_Add,	NoSymbol,	NoSymbol,	NoSymbol,	/*125*/
};

#define SpainLatAm4Modmap ItNorPortSp4Modmap

#else

#define SpainLatAm4Keymap NULL
#define SpainLatAm4Modmap NULL

#endif /* SPAINLATAM4 */


#ifdef SWED5

static KeySym Sweden5Keymap[] = {
	SunXK_Stop,  	NoSymbol,  	XK_L1,		NoSymbol,	/*  1*/
	SunXK_AudioLowerVolume,SunXK_VideoLowerBrightness,NoSymbol,	NoSymbol,	/*  2*/
	SunXK_Again,  	NoSymbol,  	XK_L2,		NoSymbol,	/*  3*/
	SunXK_AudioRaiseVolume,SunXK_VideoRaiseBrightness,NoSymbol,	NoSymbol,	/*  4*/
	XK_F1,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  5*/
	XK_F2,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  6*/
	XK_F10, 	NoSymbol,	NoSymbol,	NoSymbol,	/*  7*/
	XK_F3,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  8*/
	XK_F11,		NoSymbol,	SunXK_F36,	NoSymbol,	/*  9*/
	XK_F4,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 10*/
	XK_F12,		NoSymbol,	SunXK_F37,	NoSymbol,	/* 11*/
	XK_F5,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 12*/
	SunXK_AltGraph,	NoSymbol,	NoSymbol,	NoSymbol,	/* 13*/
	XK_F6,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 14*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 15*/
	XK_F7,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 16*/
	XK_F8,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 17*/
	XK_F9,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 18*/
	XK_Alt_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 19*/
	XK_Up,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 20*/
	XK_Pause,  	NoSymbol,  	XK_Break,	NoSymbol,	/* 21*/
	XK_Print,  	NoSymbol,  	XK_Sys_Req,	SunXK_Sys_Req,	/* 22*/
	XK_Scroll_Lock,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 23*/
	XK_Left,	NoSymbol,	NoSymbol,	NoSymbol,	/* 24*/
	SunXK_Props,  	NoSymbol,  	XK_L3,		NoSymbol,	/* 25*/
	SunXK_Undo,  	NoSymbol,  	XK_L4,		NoSymbol,	/* 26*/
	XK_Down,	NoSymbol,	NoSymbol,	NoSymbol,	/* 27*/
	XK_Right,	NoSymbol,	NoSymbol,	NoSymbol,	/* 28*/
	XK_Escape,	NoSymbol,	NoSymbol,	NoSymbol,	/* 29*/
	XK_1,   	XK_exclam,	NoSymbol,	NoSymbol,	/* 30*/
	XK_2,   	XK_quotedbl,	XK_at,  	NoSymbol,	/* 31*/
	XK_3,   	XK_numbersign,	XK_sterling,	NoSymbol,	/* 32*/
	XK_4,   	XK_currency,	XK_dollar,	NoSymbol,	/* 33*/
	XK_5,   	XK_percent,	NoSymbol,	NoSymbol,	/* 34*/
	XK_6,   	XK_ampersand,	NoSymbol,	NoSymbol,	/* 35*/
	XK_7,   	XK_slash,	XK_braceleft,	NoSymbol,	/* 36*/
	XK_8,   	XK_parenleft,	XK_bracketleft,	NoSymbol,	/* 37*/
	XK_9,   	XK_parenright,	XK_bracketright,NoSymbol,	/* 38*/
	XK_0,   	XK_equal,	XK_braceright,	NoSymbol,	/* 39*/
	XK_plus,	XK_question,	XK_backslash,	NoSymbol,	/* 40*/
	SunXK_FA_Acute,	SunXK_FA_Grave,	NoSymbol,	NoSymbol,	/* 41*/
	XK_section,	XK_onehalf,	NoSymbol,	NoSymbol,	/* 42*/
	XK_BackSpace,	NoSymbol,	NoSymbol,	NoSymbol,	/* 43*/
	XK_Insert,	NoSymbol,	NoSymbol,	NoSymbol,	/* 44*/
	SunXK_AudioMute,SunXK_VideoDegauss,NoSymbol,	NoSymbol,	/* 45*/
	XK_KP_Divide,  	NoSymbol,  	NoSymbol,	NoSymbol,	/* 46*/
	XK_KP_Multiply,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 47*/
	SunXK_PowerSwitch,SunXK_PowerSwitchShift,	NoSymbol,	NoSymbol,	/* 48*/
	SunXK_Front,  	NoSymbol,  	XK_L5,		NoSymbol,	/* 49*/
	XK_KP_Delete,	XK_KP_Separator,NoSymbol,	NoSymbol,	/* 50*/
	SunXK_Copy,  	NoSymbol,  	XK_L6,		NoSymbol,	/* 51*/
	XK_Home,	NoSymbol,	NoSymbol,	NoSymbol,	/* 52*/
	XK_Tab, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 53*/
	XK_Q,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 54*/
	XK_W,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 55*/
	XK_E,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 56*/
	XK_R,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 57*/
	XK_T,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 58*/
	XK_Y,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 59*/
	XK_U,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 60*/
	XK_I,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 61*/
	XK_O,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 62*/
	XK_P,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 63*/
	XK_Aring,	NoSymbol,	NoSymbol,	NoSymbol,	/* 64*/
	SunXK_FA_Diaeresis,XK_asciicircum,XK_asciitilde,NoSymbol,	/* 65*/
	XK_Delete,	NoSymbol,	NoSymbol,	NoSymbol,	/* 66*/
	SunXK_Compose,	NoSymbol,	NoSymbol,	NoSymbol,	/* 67*/
	XK_KP_Home,  	XK_KP_7,  	NoSymbol,	NoSymbol,	/* 68*/
	XK_KP_Up,  	XK_KP_8,  	NoSymbol,	NoSymbol,	/* 69*/
	XK_KP_Prior,  	XK_KP_9,  	NoSymbol,	NoSymbol,	/* 70*/
	XK_KP_Subtract,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 71*/
	SunXK_Open,  	NoSymbol,  	XK_L7,		NoSymbol,	/* 72*/
	SunXK_Paste,  	NoSymbol,  	XK_L8,		NoSymbol,	/* 73*/
	XK_End, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 74*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 75*/
	XK_Control_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 76*/
	XK_A,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 77*/
	XK_S,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 78*/
	XK_D,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 79*/
	XK_F,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 80*/
	XK_G,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 81*/
	XK_H,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 82*/
	XK_J,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 83*/
	XK_K,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 84*/
	XK_L,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 85*/
	XK_Odiaeresis,	NoSymbol,	NoSymbol,	NoSymbol,	/* 86*/
	XK_Adiaeresis,	NoSymbol,	NoSymbol,	NoSymbol,	/* 87*/
	XK_apostrophe,	XK_asterisk,	XK_grave,	NoSymbol,	/* 88*/
	XK_Return,	NoSymbol,	NoSymbol,	NoSymbol,	/* 89*/
	XK_KP_Enter,	NoSymbol,	NoSymbol,	NoSymbol,	/* 90*/
	XK_KP_Left,	XK_KP_4, 	NoSymbol,	NoSymbol,	/* 91*/
	NoSymbol, 	XK_KP_5, 	NoSymbol,	NoSymbol,	/* 92*/
	XK_KP_Right,	XK_KP_6, 	NoSymbol,	NoSymbol,	/* 93*/
	XK_KP_Insert,	XK_KP_0,	NoSymbol,	NoSymbol,	/* 94*/
	SunXK_Find,  	NoSymbol,  	XK_L9,		NoSymbol,	/* 95*/
	XK_Prior,	NoSymbol,	NoSymbol,	NoSymbol,	/* 96*/
	SunXK_Cut, 	NoSymbol, 	XK_L10,		NoSymbol,	/* 97*/
	XK_Num_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 98*/
	XK_Shift_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 99*/
	XK_Z,   	NoSymbol,	NoSymbol,	NoSymbol,	/*100*/
	XK_X,   	NoSymbol,	NoSymbol,	NoSymbol,	/*101*/
	XK_C,   	NoSymbol,	NoSymbol,	NoSymbol,	/*102*/
	XK_V,   	NoSymbol,	NoSymbol,	NoSymbol,	/*103*/
	XK_B,   	NoSymbol,	NoSymbol,	NoSymbol,	/*104*/
	XK_N,   	NoSymbol,	NoSymbol,	NoSymbol,	/*105*/
	XK_M,   	NoSymbol,	NoSymbol,	NoSymbol,	/*106*/
	XK_comma,	XK_semicolon,	NoSymbol,	NoSymbol,	/*107*/
	XK_period,	XK_colon,	NoSymbol,	NoSymbol,	/*108*/
	XK_minus,	XK_underscore,	NoSymbol,	NoSymbol,	/*109*/
	XK_Shift_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*110*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*111*/
	XK_KP_End, 	XK_KP_1, 	NoSymbol,	NoSymbol, 	/*112*/
	XK_KP_Down,	XK_KP_2, 	NoSymbol,	NoSymbol,	/*113*/
	XK_KP_Next, 	XK_KP_3, 	NoSymbol,	NoSymbol,	/*114*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*115*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*116*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*117*/
	XK_Help,	NoSymbol,	NoSymbol,	NoSymbol,	/*118*/
	XK_Caps_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/*119*/
	XK_Meta_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*120*/
	XK_space,	NoSymbol,	NoSymbol,	NoSymbol,	/*121*/
	XK_Meta_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*122*/
	XK_Next,	NoSymbol,	NoSymbol,	NoSymbol,	/*123*/
	XK_less,	XK_greater,	XK_bar, 	NoSymbol,	/*124*/
	XK_KP_Add,	NoSymbol,	NoSymbol,	NoSymbol,	/*125*/
};

#define Sweden5Modmap Generic5Modmap

#else

#define Sweden5Keymap NULL
#define Sweden5Modmap NULL

#endif /* SWEDEN5 */


#ifdef SWEDFIN4

static KeySym SwedenFin4Keymap[] = {
	SunXK_Stop,  	NoSymbol,  	XK_L1,		NoSymbol,	/*  1*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  2*/
	SunXK_Again,  	NoSymbol,  	XK_L2,		NoSymbol,	/*  3*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  4*/
	XK_F1,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  5*/
	XK_F2,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  6*/
	XK_F10, 	NoSymbol,	NoSymbol,	NoSymbol,	/*  7*/
	XK_F3,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  8*/
	XK_F11,		NoSymbol,	SunXK_F36,	NoSymbol,	/*  9*/
	XK_F4,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 10*/
	XK_F12,		NoSymbol,	SunXK_F37,	NoSymbol,	/* 11*/
	XK_F5,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 12*/
	SunXK_Compose,	NoSymbol,	NoSymbol,	NoSymbol,	/* 13*/
	XK_F6,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 14*/
	XK_asciitilde,	XK_asciicircum,	NoSymbol,	NoSymbol,	/* 15*/
	XK_F7,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 16*/
	XK_F8,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 17*/
	XK_F9,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 18*/
	XK_Alt_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 19*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 20*/
	XK_Pause,  	NoSymbol,  	XK_R1,		NoSymbol,	/* 21*/
	XK_Print,  	NoSymbol,  	XK_R2,		NoSymbol,	/* 22*/
	XK_Break,  	XK_Scroll_Lock,	XK_R3,		NoSymbol,	/* 23*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 24*/
	SunXK_Props,  	NoSymbol,  	XK_L3,		NoSymbol,	/* 25*/
	SunXK_Undo,  	NoSymbol,  	XK_L4,		NoSymbol,	/* 26*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 27*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 28*/
	XK_Escape,	NoSymbol,	NoSymbol,	NoSymbol,	/* 29*/
	XK_1,   	XK_exclam,	NoSymbol,	NoSymbol,	/* 30*/
	XK_2,   	XK_quotedbl,	XK_at,  	NoSymbol,	/* 31*/
	XK_3,   	XK_numbersign,	XK_sterling,	NoSymbol,	/* 32*/
	XK_4,   	XK_currency,	XK_dollar,	NoSymbol,	/* 33*/
	XK_5,   	XK_percent,	NoSymbol,	NoSymbol,	/* 34*/
	XK_6,   	XK_ampersand,	NoSymbol,	NoSymbol,	/* 35*/
	XK_7,   	XK_slash,	XK_braceleft,	NoSymbol,	/* 36*/
	XK_8,   	XK_parenleft,	XK_bracketleft,	NoSymbol,	/* 37*/
	XK_9,   	XK_parenright,	XK_bracketright,NoSymbol,	/* 38*/
	XK_0,   	XK_equal,	XK_braceright,	NoSymbol,	/* 39*/
	XK_plus,	XK_question,	XK_backslash,	NoSymbol,	/* 40*/
	SunXK_FA_Acute,	SunXK_FA_Grave,	NoSymbol,	NoSymbol,	/* 41*/
	XK_apostrophe,	XK_asterisk,	XK_grave,	NoSymbol,	/* 42*/
	XK_BackSpace,	NoSymbol,	NoSymbol,	NoSymbol,	/* 43*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 44*/
	XK_KP_Equal,  	NoSymbol,  	XK_R4,		NoSymbol,	/* 45*/
	XK_KP_Divide,  	NoSymbol,  	XK_R5,		NoSymbol,	/* 46*/
	XK_KP_Multiply,	NoSymbol,  	XK_R6,		NoSymbol,	/* 47*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 48*/
	SunXK_Front,  	NoSymbol,  	XK_L5,		NoSymbol,	/* 49*/
	XK_KP_Delete,	XK_KP_Decimal,	NoSymbol,	NoSymbol,	/* 50*/
	SunXK_Copy,  	NoSymbol,  	XK_L6,		NoSymbol,	/* 51*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 52*/
	XK_Tab, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 53*/
	XK_Q,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 54*/
	XK_W,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 55*/
	XK_E,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 56*/
	XK_R,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 57*/
	XK_T,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 58*/
	XK_Y,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 59*/
	XK_U,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 60*/
	XK_I,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 61*/
	XK_O,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 62*/
	XK_P,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 63*/
	XK_Aring,	NoSymbol,	NoSymbol,	NoSymbol,	/* 64*/
	SunXK_FA_Diaeresis,SunXK_FA_Circum,SunXK_FA_Tilde,NoSymbol,	/* 65*/
	XK_Delete,	NoSymbol,	NoSymbol,	NoSymbol,	/* 66*/
	SunXK_AltGraph,	NoSymbol,	NoSymbol,	NoSymbol,	/* 67*/
	XK_KP_Home,  	XK_KP_7,  	XK_R7,		NoSymbol,	/* 68*/
	XK_KP_Up,  	XK_KP_8,  	XK_R8,		NoSymbol,	/* 69*/
	XK_KP_Prior,  	XK_KP_9,  	XK_R8,		NoSymbol,	/* 70*/
	XK_KP_Subtract,	NoSymbol,	NoSymbol,	NoSymbol,	/* 71*/
	SunXK_Open,  	NoSymbol,  	XK_L7,		NoSymbol,	/* 72*/
	SunXK_Paste,  	NoSymbol,  	XK_L8,		NoSymbol,	/* 73*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 74*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 75*/
	XK_Caps_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 76*/
	XK_A,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 77*/
	XK_S,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 78*/
	XK_D,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 79*/
	XK_F,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 80*/
	XK_G,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 81*/
	XK_H,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 82*/
	XK_J,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 83*/
	XK_K,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 84*/
	XK_L,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 85*/
	XK_Odiaeresis,	NoSymbol,	NoSymbol,	NoSymbol,	/* 86*/
	XK_Adiaeresis,	NoSymbol,	NoSymbol,	NoSymbol,	/* 87*/
	XK_section,	XK_onehalf,	NoSymbol,	NoSymbol,	/* 88*/
	XK_Return,	NoSymbol,	NoSymbol,	NoSymbol,	/* 89*/
	XK_KP_Enter,	NoSymbol,	NoSymbol,	NoSymbol,	/* 90*/
	XK_KP_Left,	XK_KP_4, 	XK_R10,		NoSymbol,	/* 91*/
	NoSymbol, 	XK_KP_5, 	XK_R11,		NoSymbol,	/* 92*/
	XK_KP_Right,	XK_KP_6, 	XK_R12,		NoSymbol,	/* 93*/
	XK_KP_Insert,	XK_KP_0,	NoSymbol,	NoSymbol,	/* 94*/
	SunXK_Find,  	NoSymbol,  	XK_L9,		NoSymbol,	/* 95*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 96*/
	SunXK_Cut, 	NoSymbol, 	XK_L10,		NoSymbol,	/* 97*/
	XK_Num_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 98*/
	XK_Shift_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 99*/
	XK_Z,   	NoSymbol,	NoSymbol,	NoSymbol,	/*100*/
	XK_X,   	NoSymbol,	NoSymbol,	NoSymbol,	/*101*/
	XK_C,   	NoSymbol,	NoSymbol,	NoSymbol,	/*102*/
	XK_V,   	NoSymbol,	NoSymbol,	NoSymbol,	/*103*/
	XK_B,   	NoSymbol,	NoSymbol,	NoSymbol,	/*104*/
	XK_N,   	NoSymbol,	NoSymbol,	NoSymbol,	/*105*/
	XK_M,   	NoSymbol,	NoSymbol,	NoSymbol,	/*106*/
	XK_comma,	XK_semicolon,	NoSymbol,	NoSymbol,	/*107*/
	XK_period,	XK_colon,	NoSymbol,	NoSymbol,	/*108*/
	XK_minus,	XK_underscore,	NoSymbol,	NoSymbol,	/*109*/
	XK_Shift_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*110*/
	XK_Linefeed,	NoSymbol,	NoSymbol,	NoSymbol,	/*111*/
	XK_KP_End, 	XK_KP_1, 	XK_R13,		NoSymbol, 	/*112*/
	XK_KP_Down,	XK_KP_2, 	XK_R14,		NoSymbol,	/*113*/
	XK_KP_Next, 	XK_KP_3, 	XK_R15,		NoSymbol,	/*114*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*115*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*116*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*117*/
	XK_Help,	NoSymbol,	NoSymbol,	NoSymbol,	/*118*/
	XK_Control_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*119*/
	XK_Meta_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*120*/
	XK_space,	NoSymbol,	NoSymbol,	NoSymbol,	/*121*/
	XK_Meta_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*122*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*123*/
	XK_less,	XK_greater,	XK_bar, 	NoSymbol,	/*124*/
	XK_KP_Add,	NoSymbol,	NoSymbol,	NoSymbol,	/*125*/
};

#define SwedenFin4Modmap DenSwedFinSw4Modmap

#else

#define SwedenFin4Keymap NULL
#define SwedenFin4Modmap NULL

#endif /* SWEDENFIN4 */


#ifdef SWFR4

static KeySym SwissFr4Keymap[] = {
	SunXK_Stop,  	NoSymbol,  	XK_L1,		NoSymbol,	/*  1*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  2*/
	SunXK_Again,  	NoSymbol,  	XK_L2,		NoSymbol,	/*  3*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  4*/
	XK_F1,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  5*/
	XK_F2,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  6*/
	XK_F10, 	NoSymbol,	NoSymbol,	NoSymbol,	/*  7*/
	XK_F3,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  8*/
	XK_F11,		NoSymbol,	SunXK_F36,	NoSymbol,	/*  9*/
	XK_F4,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 10*/
	XK_F12,		NoSymbol,	SunXK_F37,	NoSymbol,	/* 11*/
	XK_F5,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 12*/
	SunXK_Compose,	NoSymbol,	NoSymbol,	NoSymbol,	/* 13*/
	XK_F6,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 14*/
	XK_greater,	XK_braceright,	NoSymbol,	NoSymbol,	/* 15*/
	XK_F7,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 16*/
	XK_F8,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 17*/
	XK_F9,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 18*/
	XK_Alt_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 19*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 20*/
	XK_Pause,  	NoSymbol,  	XK_R1,		NoSymbol,	/* 21*/
	XK_Print,  	NoSymbol,  	XK_R2,		NoSymbol,	/* 22*/
	XK_Break,  	XK_Scroll_Lock,	XK_R3,		NoSymbol,	/* 23*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 24*/
	SunXK_Props,  	NoSymbol,  	XK_L3,		NoSymbol,	/* 25*/
	SunXK_Undo,  	NoSymbol,  	XK_L4,		NoSymbol,	/* 26*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 27*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 28*/
	XK_Escape,	NoSymbol,	NoSymbol,	NoSymbol,	/* 29*/
	XK_1,   	XK_plus,	XK_exclam,	NoSymbol,	/* 30*/
	XK_2,   	XK_quotedbl,	XK_at,  	NoSymbol,	/* 31*/
	XK_3,   	XK_asterisk,	XK_numbersign,	NoSymbol,	/* 32*/
	XK_4,   	XK_ccedilla,	XK_cent,	NoSymbol,	/* 33*/
	XK_5,   	XK_percent,	XK_asciitilde,	NoSymbol,	/* 34*/
	XK_6,   	XK_ampersand,	XK_section,	NoSymbol,	/* 35*/
	XK_7,   	XK_slash,	XK_bar, 	NoSymbol,	/* 36*/
	XK_8,   	XK_parenleft,	XK_degree,	NoSymbol,	/* 37*/
	XK_9,   	XK_parenright,	XK_backslash,	NoSymbol,	/* 38*/
	XK_0,   	XK_equal,	XK_asciicircum,	NoSymbol,	/* 39*/
	XK_apostrophe,	XK_question,	XK_grave,	NoSymbol,	/* 40*/
	SunXK_FA_Circum,SunXK_FA_Grave,	NoSymbol,	NoSymbol,	/* 41*/
	XK_dollar,	SunXK_FA_Tilde,	XK_sterling,	NoSymbol,	/* 42*/
	XK_BackSpace,	NoSymbol,	NoSymbol,	NoSymbol,	/* 43*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 44*/
	XK_KP_Equal,  	NoSymbol,  	XK_R4,		NoSymbol,	/* 45*/
	XK_KP_Divide,  	NoSymbol,  	XK_R5,		NoSymbol,	/* 46*/
	XK_KP_Multiply,	NoSymbol,  	XK_R6,		NoSymbol,	/* 47*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 48*/
	SunXK_Front,  	NoSymbol,  	XK_L5,		NoSymbol,	/* 49*/
	XK_KP_Delete,	XK_KP_Decimal,	NoSymbol,	NoSymbol,	/* 50*/
	SunXK_Copy,  	NoSymbol,  	XK_L6,		NoSymbol,	/* 51*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 52*/
	XK_Tab, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 53*/
	XK_Q,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 54*/
	XK_W,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 55*/
	XK_E,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 56*/
	XK_R,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 57*/
	XK_T,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 58*/
	XK_Z,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 59*/
	XK_U,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 60*/
	XK_I,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 61*/
	XK_O,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 62*/
	XK_P,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 63*/
	XK_egrave,	XK_udiaeresis,	NoSymbol,	NoSymbol,	/* 64*/
	SunXK_FA_Diaeresis,SunXK_FA_Acute,NoSymbol,	NoSymbol,	/* 65*/
	XK_Delete,	NoSymbol,	NoSymbol,	NoSymbol,	/* 66*/
	SunXK_AltGraph,	NoSymbol,	NoSymbol,	NoSymbol,	/* 67*/
	XK_KP_Home,  	XK_KP_7,  	XK_R7,		NoSymbol,	/* 68*/
	XK_KP_Up,  	XK_KP_8,  	XK_R8,		NoSymbol,	/* 69*/
	XK_KP_Prior,  	XK_KP_9,  	XK_R9,		NoSymbol,	/* 70*/
	XK_KP_Subtract,	NoSymbol,	NoSymbol,	NoSymbol,	/* 71*/
	SunXK_Open,  	NoSymbol,  	XK_L7,		NoSymbol,	/* 72*/
	SunXK_Paste,  	NoSymbol,  	XK_L8,		NoSymbol,	/* 73*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 74*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 75*/
	XK_Caps_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 76*/
	XK_A,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 77*/
	XK_S,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 78*/
	XK_D,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 79*/
	XK_F,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 80*/
	XK_G,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 81*/
	XK_H,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 82*/
	XK_J,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 83*/
	XK_K,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 84*/
	XK_L,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 85*/
	XK_eacute,	XK_odiaeresis,	NoSymbol,	NoSymbol,	/* 86*/
	XK_agrave,	XK_adiaeresis,	NoSymbol,	NoSymbol,	/* 87*/
	XK_less,	XK_braceleft,	NoSymbol,	NoSymbol,	/* 88*/
	XK_Return,	NoSymbol,	NoSymbol,	NoSymbol,	/* 89*/
	XK_KP_Enter,	NoSymbol,	NoSymbol,	NoSymbol,	/* 90*/
	XK_KP_Left,	XK_KP_4, 	XK_R10,		NoSymbol,	/* 91*/
	NoSymbol, 	XK_KP_5, 	XK_R11,		NoSymbol,	/* 92*/
	XK_KP_Right,	XK_KP_6, 	XK_R12,		NoSymbol,	/* 93*/
	XK_KP_Insert,	XK_KP_0,	NoSymbol,	NoSymbol,	/* 94*/
	SunXK_Find,  	NoSymbol,  	XK_L9,		NoSymbol,	/* 95*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 96*/
	SunXK_Cut, 	NoSymbol, 	XK_L10,		NoSymbol,	/* 97*/
	XK_Num_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 98*/
	XK_Shift_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 99*/
	XK_Y,   	NoSymbol,	NoSymbol,	NoSymbol,	/*100*/
	XK_X,   	NoSymbol,	NoSymbol,	NoSymbol,	/*101*/
	XK_C,   	NoSymbol,	NoSymbol,	NoSymbol,	/*102*/
	XK_V,   	NoSymbol,	NoSymbol,	NoSymbol,	/*103*/
	XK_B,   	NoSymbol,	NoSymbol,	NoSymbol,	/*104*/
	XK_N,   	NoSymbol,	NoSymbol,	NoSymbol,	/*105*/
	XK_M,   	NoSymbol,	XK_mu,  	NoSymbol,	/*106*/
	XK_comma,	XK_semicolon,	NoSymbol,	NoSymbol,	/*107*/
	XK_period,	XK_colon,	NoSymbol,	NoSymbol,	/*108*/
	XK_minus,	XK_underscore,	NoSymbol,	NoSymbol,	/*109*/
	XK_Shift_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*110*/
	XK_Linefeed,	NoSymbol,	NoSymbol,	NoSymbol,	/*111*/
	XK_KP_End, 	XK_KP_1, 	XK_R13,		NoSymbol, 	/*112*/
	XK_KP_Down,	XK_KP_2, 	XK_R14,		NoSymbol,	/*113*/
	XK_KP_Next, 	XK_KP_3, 	XK_R15,		NoSymbol,	/*114*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*115*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*116*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*117*/
	XK_Help,	NoSymbol,	NoSymbol,	NoSymbol,	/*118*/
	XK_Control_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*119*/
	XK_Meta_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*120*/
	XK_space,	NoSymbol,	NoSymbol,	NoSymbol,	/*121*/
	XK_Meta_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*122*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*123*/
	XK_bracketright,XK_bracketleft,	NoSymbol,	NoSymbol,	/*124*/
	XK_KP_Add,	NoSymbol,	NoSymbol,	NoSymbol,	/*125*/
};

#define SwissFr4Modmap DenSwedFinSw4Modmap

#else

#define SwissFr4Keymap NULL
#define SwissFr4Modmap NULL

#endif /* SWFR4 */


#ifdef SWFR5

static KeySym SwissFr5Keymap[] = {
	SunXK_Stop,  	NoSymbol,  	XK_L1,		NoSymbol,	/*  1*/
	SunXK_AudioLowerVolume,SunXK_VideoLowerBrightness,NoSymbol,	NoSymbol,	/*  2*/
	SunXK_Again,  	NoSymbol,  	XK_L2,		NoSymbol,	/*  3*/
	SunXK_AudioRaiseVolume,SunXK_VideoRaiseBrightness,NoSymbol,	NoSymbol,	/*  4*/
	XK_F1,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  5*/
	XK_F2,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  6*/
	XK_F10, 	NoSymbol,	NoSymbol,	NoSymbol,	/*  7*/
	XK_F3,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  8*/
	XK_F11,		NoSymbol,	SunXK_F36,	NoSymbol,	/*  9*/
	XK_F4,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 10*/
	XK_F12,		NoSymbol,	SunXK_F37,	NoSymbol,	/* 11*/
	XK_F5,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 12*/
	SunXK_AltGraph,	NoSymbol,	NoSymbol,	NoSymbol,	/* 13*/
	XK_F6,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 14*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 15*/
	XK_F7,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 16*/
	XK_F8,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 17*/
	XK_F9,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 18*/
	XK_Alt_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 19*/
	XK_Up,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 20*/
	XK_Pause,  	NoSymbol,  	XK_Break,	NoSymbol,	/* 21*/
	XK_Print,  	NoSymbol,  	XK_Sys_Req,	SunXK_Sys_Req,	/* 22*/
	XK_Scroll_Lock,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 23*/
	XK_Left,	NoSymbol,	NoSymbol,	NoSymbol,	/* 24*/
	SunXK_Props,  	NoSymbol,  	XK_L3,		NoSymbol,	/* 25*/
	SunXK_Undo,  	NoSymbol,  	XK_L4,		NoSymbol,	/* 26*/
	XK_Down,	NoSymbol,	NoSymbol,	NoSymbol,	/* 27*/
	XK_Right,	NoSymbol,	NoSymbol,	NoSymbol,	/* 28*/
	XK_Escape,	NoSymbol,	NoSymbol,	NoSymbol,	/* 29*/
	XK_1,   	XK_plus,	XK_bar, 	NoSymbol,	/* 30*/
	XK_2,   	XK_quotedbl,	XK_at,  	NoSymbol,	/* 31*/
	XK_3,   	XK_asterisk,	XK_numbersign,	NoSymbol,	/* 32*/
	XK_4,   	XK_ccedilla,	XK_asciicircum,	NoSymbol,	/* 33*/
	XK_5,   	XK_percent,	XK_asciitilde,	NoSymbol,	/* 34*/
	XK_6,   	XK_ampersand,	NoSymbol,	NoSymbol,	/* 35*/
	XK_7,   	XK_slash,	NoSymbol,	NoSymbol,	/* 36*/
	XK_8,   	XK_parenleft,	NoSymbol,	NoSymbol,	/* 37*/
	XK_9,   	XK_parenright,	NoSymbol,	NoSymbol,	/* 38*/
	XK_0,   	XK_equal,	XK_grave,	NoSymbol,	/* 39*/
	XK_apostrophe,	XK_question,	SunXK_FA_Acute,	NoSymbol,	/* 40*/
	SunXK_FA_Circum,SunXK_FA_Grave,	SunXK_FA_Tilde,	NoSymbol,	/* 41*/
	XK_section,	XK_degree,	NoSymbol,	NoSymbol,	/* 42*/
	XK_BackSpace,	NoSymbol,	NoSymbol,	NoSymbol,	/* 43*/
	XK_Insert,	NoSymbol,	NoSymbol,	NoSymbol,	/* 44*/
	SunXK_AudioMute,SunXK_VideoDegauss,NoSymbol,	NoSymbol,	/* 45*/
	XK_KP_Divide,  	NoSymbol,  	NoSymbol,	NoSymbol,	/* 46*/
	XK_KP_Multiply,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 47*/
	SunXK_PowerSwitch,SunXK_PowerSwitchShift,	NoSymbol,	NoSymbol,	/* 48*/
	SunXK_Front,  	NoSymbol,  	XK_L5,		NoSymbol,	/* 49*/
	XK_KP_Delete,	XK_KP_Decimal,	NoSymbol,	NoSymbol,	/* 50*/
	SunXK_Copy,  	NoSymbol,  	XK_L6,		NoSymbol,	/* 51*/
	XK_Home,	NoSymbol,	NoSymbol,	NoSymbol,	/* 52*/
	XK_Tab, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 53*/
	XK_Q,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 54*/
	XK_W,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 55*/
	XK_E,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 56*/
	XK_R,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 57*/
	XK_T,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 58*/
	XK_Z,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 59*/
	XK_U,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 60*/
	XK_I,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 61*/
	XK_O,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 62*/
	XK_P,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 63*/
	XK_egrave,	XK_udiaeresis,	XK_bracketleft,	NoSymbol,	/* 64*/
	SunXK_FA_Diaeresis,XK_exclam,	XK_bracketright,NoSymbol,	/* 65*/
	XK_Delete,	NoSymbol,	NoSymbol,	NoSymbol,	/* 66*/
	SunXK_Compose,	NoSymbol,	NoSymbol,	NoSymbol,	/* 67*/
	XK_KP_Home,  	XK_KP_7,  	NoSymbol,	NoSymbol,	/* 68*/
	XK_KP_Up,  	XK_KP_8,  	NoSymbol,	NoSymbol,	/* 69*/
	XK_KP_Prior,  	XK_KP_9,  	NoSymbol,	NoSymbol,	/* 70*/
	XK_KP_Subtract,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 71*/
	SunXK_Open,  	NoSymbol,  	XK_L7,		NoSymbol,	/* 72*/
	SunXK_Paste,  	NoSymbol,  	XK_L8,		NoSymbol,	/* 73*/
	XK_End, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 74*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 75*/
	XK_Control_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 76*/
	XK_A,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 77*/
	XK_S,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 78*/
	XK_D,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 79*/
	XK_F,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 80*/
	XK_G,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 81*/
	XK_H,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 82*/
	XK_J,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 83*/
	XK_K,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 84*/
	XK_L,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 85*/
	XK_eacute,	XK_odiaeresis,	NoSymbol,	NoSymbol,	/* 86*/
	XK_agrave,	XK_adiaeresis,	XK_braceleft,	NoSymbol,	/* 87*/
	XK_dollar,	XK_sterling,	XK_braceright,	NoSymbol,	/* 88*/
	XK_Return,	NoSymbol,	NoSymbol,	NoSymbol,	/* 89*/
	XK_KP_Enter,	NoSymbol,	NoSymbol,	NoSymbol,	/* 90*/
	XK_KP_Left,	XK_KP_4, 	NoSymbol,	NoSymbol,	/* 91*/
	NoSymbol, 	XK_KP_5, 	NoSymbol,	NoSymbol,	/* 92*/
	XK_KP_Right,	XK_KP_6, 	NoSymbol,	NoSymbol,	/* 93*/
	XK_KP_Insert,	XK_KP_0,	NoSymbol,	NoSymbol,	/* 94*/
	SunXK_Find,  	NoSymbol,  	XK_L9,		NoSymbol,	/* 95*/
	XK_Prior,	NoSymbol,	NoSymbol,	NoSymbol,	/* 96*/
	SunXK_Cut, 	NoSymbol, 	XK_L10,		NoSymbol,	/* 97*/
	XK_Num_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 98*/
	XK_Shift_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 99*/
	XK_Y,   	NoSymbol,	NoSymbol,	NoSymbol,	/*100*/
	XK_X,   	NoSymbol,	NoSymbol,	NoSymbol,	/*101*/
	XK_C,   	NoSymbol,	NoSymbol,	NoSymbol,	/*102*/
	XK_V,   	NoSymbol,	NoSymbol,	NoSymbol,	/*103*/
	XK_B,   	NoSymbol,	NoSymbol,	NoSymbol,	/*104*/
	XK_N,   	NoSymbol,	NoSymbol,	NoSymbol,	/*105*/
	XK_M,   	NoSymbol,	NoSymbol,	NoSymbol,	/*106*/
	XK_comma,	XK_semicolon,	NoSymbol,	NoSymbol,	/*107*/
	XK_period,	XK_colon,	NoSymbol,	NoSymbol,	/*108*/
	XK_minus,	XK_underscore,	NoSymbol,	NoSymbol,	/*109*/
	XK_Shift_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*110*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*111*/
	XK_KP_End, 	XK_KP_1, 	NoSymbol,	NoSymbol, 	/*112*/
	XK_KP_Down,	XK_KP_2, 	NoSymbol,	NoSymbol,	/*113*/
	XK_KP_Next, 	XK_KP_3, 	NoSymbol,	NoSymbol,	/*114*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*115*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*116*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*117*/
	XK_Help,	NoSymbol,	NoSymbol,	NoSymbol,	/*118*/
	XK_Caps_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/*119*/
	XK_Meta_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*120*/
	XK_space,	NoSymbol,	NoSymbol,	NoSymbol,	/*121*/
	XK_Meta_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*122*/
	XK_Next,	NoSymbol,	NoSymbol,	NoSymbol,	/*123*/
	XK_less,	XK_greater,	XK_backslash,	NoSymbol,	/*124*/
	XK_KP_Add,	NoSymbol,	NoSymbol,	NoSymbol,	/*125*/
};

#define SwissFr5Modmap Generic5Modmap

#else

#define SwissFr5Keymap NULL
#define SwissFr5Modmap NULL

#endif /* SWFR5 */


#ifdef SWGE4

static KeySym SwissGe4Keymap[] = {
	SunXK_Stop,  	NoSymbol,  	XK_L1,		NoSymbol,	/*  1*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  2*/
	SunXK_Again,  	NoSymbol,  	XK_L2,		NoSymbol,	/*  3*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  4*/
	XK_F1,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  5*/
	XK_F2,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  6*/
	XK_F10, 	NoSymbol,	NoSymbol,	NoSymbol,	/*  7*/
	XK_F3,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  8*/
	XK_F11,		NoSymbol,	SunXK_F36,	NoSymbol,	/*  9*/
	XK_F4,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 10*/
	XK_F12,		NoSymbol,	SunXK_F37,	NoSymbol,	/* 11*/
	XK_F5,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 12*/
	SunXK_Compose,	NoSymbol,	NoSymbol,	NoSymbol,	/* 13*/
	XK_F6,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 14*/
	XK_greater,	XK_braceright,	NoSymbol,	NoSymbol,	/* 15*/
	XK_F7,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 16*/
	XK_F8,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 17*/
	XK_F9,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 18*/
	XK_Alt_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 19*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 20*/
	XK_Pause,  	NoSymbol,  	XK_R1,		NoSymbol,	/* 21*/
	XK_Print,  	NoSymbol,  	XK_R2,		NoSymbol,	/* 22*/
	XK_Break,  	XK_Scroll_Lock,	XK_R3,		NoSymbol,	/* 23*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 24*/
	SunXK_Props,  	NoSymbol,  	XK_L3,		NoSymbol,	/* 25*/
	SunXK_Undo,  	NoSymbol,  	XK_L4,		NoSymbol,	/* 26*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 27*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 28*/
	XK_Escape,	NoSymbol,	NoSymbol,	NoSymbol,	/* 29*/
	XK_1,   	XK_plus,	XK_exclam,	NoSymbol,	/* 30*/
	XK_2,   	XK_quotedbl,	XK_at,  	NoSymbol,	/* 31*/
	XK_3,   	XK_asterisk,	XK_numbersign,	NoSymbol,	/* 32*/
	XK_4,   	XK_ccedilla,	XK_cent,	NoSymbol,	/* 33*/
	XK_5,   	XK_percent,	XK_asciitilde,	NoSymbol,	/* 34*/
	XK_6,   	XK_ampersand,	XK_section,	NoSymbol,	/* 35*/
	XK_7,   	XK_slash,	XK_bar, 	NoSymbol,	/* 36*/
	XK_8,   	XK_parenleft,	XK_degree,	NoSymbol,	/* 37*/
	XK_9,   	XK_parenright,	XK_backslash,	NoSymbol,	/* 38*/
	XK_0,   	XK_equal,	XK_asciicircum,	NoSymbol,	/* 39*/
	XK_apostrophe,	XK_question,	XK_grave,	NoSymbol,	/* 40*/
	SunXK_FA_Circum,SunXK_FA_Grave,	NoSymbol,	NoSymbol,	/* 41*/
	XK_dollar,	SunXK_FA_Tilde,	XK_sterling,	NoSymbol,	/* 42*/
	XK_BackSpace,	NoSymbol,	NoSymbol,	NoSymbol,	/* 43*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 44*/
	XK_KP_Equal,  	NoSymbol,  	XK_R4,		NoSymbol,	/* 45*/
	XK_KP_Divide,  	NoSymbol,  	XK_R5,		NoSymbol,	/* 46*/
	XK_KP_Multiply,	NoSymbol,  	XK_R6,		NoSymbol,	/* 47*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 48*/
	SunXK_Front,  	NoSymbol,  	XK_L5,		NoSymbol,	/* 49*/
	XK_KP_Delete,	XK_KP_Decimal,	NoSymbol,	NoSymbol,	/* 50*/
	SunXK_Copy,  	NoSymbol,  	XK_L6,		NoSymbol,	/* 51*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 52*/
	XK_Tab, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 53*/
	XK_Q,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 54*/
	XK_W,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 55*/
	XK_E,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 56*/
	XK_R,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 57*/
	XK_T,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 58*/
	XK_Z,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 59*/
	XK_U,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 60*/
	XK_I,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 61*/
	XK_O,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 62*/
	XK_P,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 63*/
	XK_udiaeresis,	XK_egrave,	NoSymbol,	NoSymbol,	/* 64*/
	SunXK_FA_Diaeresis,SunXK_FA_Acute,NoSymbol,	NoSymbol,	/* 65*/
	XK_Delete,	NoSymbol,	NoSymbol,	NoSymbol,	/* 66*/
	SunXK_AltGraph,	NoSymbol,	NoSymbol,	NoSymbol,	/* 67*/
	XK_KP_Home,  	XK_KP_7,  	XK_R7,		NoSymbol,	/* 68*/
	XK_KP_Up,  	XK_KP_8,  	XK_R8,		NoSymbol,	/* 69*/
	XK_KP_Prior,  	XK_KP_9,  	XK_R9,		NoSymbol,	/* 70*/
	XK_KP_Subtract,	NoSymbol,	NoSymbol,	NoSymbol,	/* 71*/
	SunXK_Open,  	NoSymbol,  	XK_L7,		NoSymbol,	/* 72*/
	SunXK_Paste,  	NoSymbol,  	XK_L8,		NoSymbol,	/* 73*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 74*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 75*/
	XK_Caps_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 76*/
	XK_A,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 77*/
	XK_S,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 78*/
	XK_D,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 79*/
	XK_F,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 80*/
	XK_G,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 81*/
	XK_H,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 82*/
	XK_J,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 83*/
	XK_K,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 84*/
	XK_L,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 85*/
	XK_odiaeresis,	XK_eacute,	NoSymbol,	NoSymbol,	/* 86*/
	XK_adiaeresis,	XK_agrave,	NoSymbol,	NoSymbol,	/* 87*/
	XK_less,	XK_braceleft,	NoSymbol,	NoSymbol,	/* 88*/
	XK_Return,	NoSymbol,	NoSymbol,	NoSymbol,	/* 89*/
	XK_KP_Enter,	NoSymbol,	NoSymbol,	NoSymbol,	/* 90*/
	XK_KP_Left,	XK_KP_4, 	XK_R10,		NoSymbol,	/* 91*/
	NoSymbol, 	XK_KP_5, 	XK_R11,		NoSymbol,	/* 92*/
	XK_KP_Right,	XK_KP_6, 	XK_R12,		NoSymbol,	/* 93*/
	XK_KP_Insert,	XK_KP_0,	NoSymbol,	NoSymbol,	/* 94*/
	SunXK_Find,  	NoSymbol,  	XK_L9,		NoSymbol,	/* 95*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 96*/
	SunXK_Cut, 	NoSymbol, 	XK_L10,		NoSymbol,	/* 97*/
	XK_Num_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 98*/
	XK_Shift_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 99*/
	XK_Y,   	NoSymbol,	NoSymbol,	NoSymbol,	/*100*/
	XK_X,   	NoSymbol,	NoSymbol,	NoSymbol,	/*101*/
	XK_C,   	NoSymbol,	NoSymbol,	NoSymbol,	/*102*/
	XK_V,   	NoSymbol,	NoSymbol,	NoSymbol,	/*103*/
	XK_B,   	NoSymbol,	NoSymbol,	NoSymbol,	/*104*/
	XK_N,   	NoSymbol,	NoSymbol,	NoSymbol,	/*105*/
	XK_M,   	NoSymbol,	XK_mu,  	NoSymbol,	/*106*/
	XK_comma,	XK_semicolon,	NoSymbol,	NoSymbol,	/*107*/
	XK_period,	XK_colon,	NoSymbol,	NoSymbol,	/*108*/
	XK_minus,	XK_underscore,	NoSymbol,	NoSymbol,	/*109*/
	XK_Shift_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*110*/
	XK_Linefeed,	NoSymbol,	NoSymbol,	NoSymbol,	/*111*/
	XK_KP_End, 	XK_KP_1, 	XK_R13,		NoSymbol, 	/*112*/
	XK_KP_Down,	XK_KP_2, 	XK_R14,		NoSymbol,	/*113*/
	XK_KP_Next, 	XK_KP_3, 	XK_R15,		NoSymbol,	/*114*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*115*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*116*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*117*/
	XK_Help,	NoSymbol,	NoSymbol,	NoSymbol,	/*118*/
	XK_Control_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*119*/
	XK_Meta_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*120*/
	XK_space,	NoSymbol,	NoSymbol,	NoSymbol,	/*121*/
	XK_Meta_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*122*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*123*/
	XK_bracketright,XK_bracketleft,	NoSymbol,	NoSymbol,	/*124*/
	XK_KP_Add,	NoSymbol,	NoSymbol,	NoSymbol,	/*125*/
};

#define SwissGe4Modmap DenSwedFinSw4Modmap

#else

#define SwissGe4Keymap NULL
#define SwissGe4Modmap NULL

#endif /* SWISSGE4 */


#ifdef SWGE5

static KeySym SwissGe5Keymap[] = {
	SunXK_Stop,  	NoSymbol,  	XK_L1,		NoSymbol,	/*  1*/
	SunXK_AudioLowerVolume,SunXK_VideoLowerBrightness,NoSymbol,	NoSymbol,	/*  2*/
	SunXK_Again,  	NoSymbol,  	XK_L2,		NoSymbol,	/*  3*/
	SunXK_AudioRaiseVolume,SunXK_VideoRaiseBrightness,NoSymbol,	NoSymbol,	/*  4*/
	XK_F1,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  5*/
	XK_F2,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  6*/
	XK_F10, 	NoSymbol,	NoSymbol,	NoSymbol,	/*  7*/
	XK_F3,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  8*/
	XK_F11,		NoSymbol,	SunXK_F36,	NoSymbol,	/*  9*/
	XK_F4,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 10*/
	XK_F12,		NoSymbol,	SunXK_F37,	NoSymbol,	/* 11*/
	XK_F5,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 12*/
	SunXK_AltGraph,	NoSymbol,	NoSymbol,	NoSymbol,	/* 13*/
	XK_F6,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 14*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 15*/
	XK_F7,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 16*/
	XK_F8,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 17*/
	XK_F9,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 18*/
	XK_Alt_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 19*/
	XK_Up,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 20*/
	XK_Pause,  	NoSymbol,  	XK_Break,	NoSymbol,	/* 21*/
	XK_Print,  	NoSymbol,  	XK_Sys_Req,	SunXK_Sys_Req,	/* 22*/
	XK_Scroll_Lock,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 23*/
	XK_Left,	NoSymbol,	NoSymbol,	NoSymbol,	/* 24*/
	SunXK_Props,  	NoSymbol,  	XK_L3,		NoSymbol,	/* 25*/
	SunXK_Undo,  	NoSymbol,  	XK_L4,		NoSymbol,	/* 26*/
	XK_Down,	NoSymbol,	NoSymbol,	NoSymbol,	/* 27*/
	XK_Right,	NoSymbol,	NoSymbol,	NoSymbol,	/* 28*/
	XK_Escape,	NoSymbol,	NoSymbol,	NoSymbol,	/* 29*/
	XK_1,   	XK_plus,	XK_bar, 	NoSymbol,	/* 30*/
	XK_2,   	XK_quotedbl,	XK_at,  	NoSymbol,	/* 31*/
	XK_3,   	XK_asterisk,	XK_numbersign,	NoSymbol,	/* 32*/
	XK_4,   	XK_ccedilla,	XK_asciicircum,	NoSymbol,	/* 33*/
	XK_5,   	XK_percent,	XK_asciitilde,	NoSymbol,	/* 34*/
	XK_6,   	XK_ampersand,	NoSymbol,	NoSymbol,	/* 35*/
	XK_7,   	XK_slash,	NoSymbol,	NoSymbol,	/* 36*/
	XK_8,   	XK_parenleft,	NoSymbol,	NoSymbol,	/* 37*/
	XK_9,   	XK_parenright,	NoSymbol,	NoSymbol,	/* 38*/
	XK_0,   	XK_equal,	XK_grave,	NoSymbol,	/* 39*/
	XK_apostrophe,	XK_question,	SunXK_FA_Acute,	NoSymbol,	/* 40*/
	SunXK_FA_Circum,SunXK_FA_Grave,	SunXK_FA_Tilde,	NoSymbol,	/* 41*/
	XK_section,	XK_degree,	NoSymbol,	NoSymbol,	/* 42*/
	XK_BackSpace,	NoSymbol,	NoSymbol,	NoSymbol,	/* 43*/
	XK_Insert,	NoSymbol,	NoSymbol,	NoSymbol,	/* 44*/
	SunXK_AudioMute,SunXK_VideoDegauss,NoSymbol,	NoSymbol,	/* 45*/
	XK_KP_Divide,  	NoSymbol,  	NoSymbol,	NoSymbol,	/* 46*/
	XK_KP_Multiply, NoSymbol,  	NoSymbol,	NoSymbol,	/* 47*/
	SunXK_PowerSwitch,SunXK_PowerSwitchShift,	NoSymbol,	NoSymbol,	/* 48*/
	SunXK_Front,  	NoSymbol,  	XK_L5,		NoSymbol,	/* 49*/
	XK_KP_Delete,	XK_KP_Decimal,	NoSymbol,	NoSymbol,	/* 50*/
	SunXK_Copy,  	NoSymbol,  	XK_L6,		NoSymbol,	/* 51*/
	XK_Home,	NoSymbol,	NoSymbol,	NoSymbol,	/* 52*/
	XK_Tab, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 53*/
	XK_Q,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 54*/
	XK_W,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 55*/
	XK_E,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 56*/
	XK_R,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 57*/
	XK_T,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 58*/
	XK_Z,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 59*/
	XK_U,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 60*/
	XK_I,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 61*/
	XK_O,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 62*/
	XK_P,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 63*/
	XK_udiaeresis,	XK_egrave,	XK_bracketleft,	NoSymbol,	/* 64*/
	SunXK_FA_Diaeresis,XK_exclam,	XK_bracketright,NoSymbol,	/* 65*/
	XK_Delete,	NoSymbol,	NoSymbol,	NoSymbol,	/* 66*/
	SunXK_Compose,	NoSymbol,	NoSymbol,	NoSymbol,	/* 67*/
	XK_KP_Home,  	XK_KP_7,  	NoSymbol,	NoSymbol,	/* 68*/
	XK_KP_Up,  	XK_KP_8,  	NoSymbol,	NoSymbol,	/* 69*/
	XK_KP_Prior,  	XK_KP_9,  	NoSymbol,	NoSymbol,	/* 70*/
	XK_KP_Subtract,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 71*/
	SunXK_Open,  	NoSymbol,  	XK_L7,		NoSymbol,	/* 72*/
	SunXK_Paste,  	NoSymbol,  	XK_L8,		NoSymbol,	/* 73*/
	XK_End, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 74*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 75*/
	XK_Control_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 76*/
	XK_A,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 77*/
	XK_S,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 78*/
	XK_D,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 79*/
	XK_F,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 80*/
	XK_G,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 81*/
	XK_H,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 82*/
	XK_J,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 83*/
	XK_K,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 84*/
	XK_L,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 85*/
	XK_odiaeresis,	XK_eacute,	NoSymbol,	NoSymbol,	/* 86*/
	XK_adiaeresis,	XK_agrave,	XK_braceleft,	NoSymbol,	/* 87*/
	XK_dollar,	XK_sterling,	XK_braceright,	NoSymbol,	/* 88*/
	XK_Return,	NoSymbol,	NoSymbol,	NoSymbol,	/* 89*/
	XK_KP_Enter,	NoSymbol,	NoSymbol,	NoSymbol,	/* 90*/
	XK_KP_Left,	XK_KP_4, 	NoSymbol,	NoSymbol,	/* 91*/
	NoSymbol, 	XK_KP_5, 	NoSymbol,	NoSymbol,	/* 92*/
	XK_KP_Right,	XK_KP_6, 	NoSymbol,	NoSymbol,	/* 93*/
	XK_KP_Insert,	XK_KP_0,	NoSymbol,	NoSymbol,	/* 94*/
	SunXK_Find,  	NoSymbol,  	XK_L9,		NoSymbol,	/* 95*/
	XK_Prior,	NoSymbol,	NoSymbol,	NoSymbol,	/* 96*/
	SunXK_Cut, 	NoSymbol, 	XK_L10,		NoSymbol,	/* 97*/
	XK_Num_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 98*/
	XK_Shift_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 99*/
	XK_Y,   	NoSymbol,	NoSymbol,	NoSymbol,	/*100*/
	XK_X,   	NoSymbol,	NoSymbol,	NoSymbol,	/*101*/
	XK_C,   	NoSymbol,	NoSymbol,	NoSymbol,	/*102*/
	XK_V,   	NoSymbol,	NoSymbol,	NoSymbol,	/*103*/
	XK_B,   	NoSymbol,	NoSymbol,	NoSymbol,	/*104*/
	XK_N,   	NoSymbol,	NoSymbol,	NoSymbol,	/*105*/
	XK_M,   	NoSymbol,	NoSymbol,	NoSymbol,	/*106*/
	XK_comma,	XK_semicolon,	NoSymbol,	NoSymbol,	/*107*/
	XK_period,	XK_colon,	NoSymbol,	NoSymbol,	/*108*/
	XK_minus,	XK_underscore,	NoSymbol,	NoSymbol,	/*109*/
	XK_Shift_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*110*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*111*/
	XK_KP_End, 	XK_KP_1, 	NoSymbol,	NoSymbol, 	/*112*/
	XK_KP_Down,	XK_KP_2, 	NoSymbol,	NoSymbol,	/*113*/
	XK_KP_Next, 	XK_KP_3, 	NoSymbol,	NoSymbol,	/*114*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*115*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*116*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*117*/
	XK_Help,	NoSymbol,	NoSymbol,	NoSymbol,	/*118*/
	XK_Caps_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/*119*/
	XK_Meta_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*120*/
	XK_space,	NoSymbol,	NoSymbol,	NoSymbol,	/*121*/
	XK_Meta_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*122*/
	XK_Next,	NoSymbol,	NoSymbol,	NoSymbol,	/*123*/
	XK_less,	XK_greater,	XK_backslash,	NoSymbol,	/*124*/
	XK_KP_Add,	NoSymbol,	NoSymbol,	NoSymbol,	/*125*/
};

#define SwissGe5Modmap Generic5Modmap

#else

#define SwissGe5Keymap NULL
#define SwissGe5Modmap NULL

#endif /* SWITZER_GE5 */


#ifdef TAI4

static KeySym Taiwan4Keymap[] = {
	SunXK_Stop,  	NoSymbol,  	XK_L1,		NoSymbol,	/*  1*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  2*/
	SunXK_Again,  	NoSymbol,  	XK_L2,		NoSymbol,	/*  3*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  4*/
	XK_F1,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  5*/
	XK_F2,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  6*/
	XK_F10, 	NoSymbol,	NoSymbol,	NoSymbol,	/*  7*/
	XK_F3,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  8*/
	XK_F11,		NoSymbol,	SunXK_F36,	NoSymbol,	/*  9*/
	XK_F4,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 10*/
	XK_F12,		NoSymbol,	SunXK_F37,	NoSymbol,	/* 11*/
	XK_F5,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 12*/
	SunXK_AltGraph,	NoSymbol,	NoSymbol,	NoSymbol,	/* 13*/
	XK_F6,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 14*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 15*/
	XK_F7,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 16*/
	XK_F8,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 17*/
	XK_F9,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 18*/
	XK_Alt_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 19*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 20*/
	XK_Pause,  	NoSymbol,  	XK_R1,		NoSymbol,	/* 21*/
	XK_Print,  	NoSymbol,  	XK_R2,		NoSymbol,	/* 22*/
	XK_Break,  	XK_Scroll_Lock,	XK_R3,		NoSymbol,	/* 23*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 24*/
	SunXK_Props,  	NoSymbol,  	XK_L3,		NoSymbol,	/* 25*/
	SunXK_Undo,  	NoSymbol,  	XK_L4,		NoSymbol,	/* 26*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 27*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 28*/
	XK_Escape,	NoSymbol,	NoSymbol,	NoSymbol,	/* 29*/
	XK_1,   	XK_exclam,	NoSymbol,	NoSymbol,	/* 30*/
	XK_2,   	XK_at,  	NoSymbol,	NoSymbol,	/* 31*/
	XK_3,   	XK_numbersign,	NoSymbol,	NoSymbol,	/* 32*/
	XK_4,   	XK_dollar,	NoSymbol,	NoSymbol,	/* 33*/
	XK_5,   	XK_percent,	NoSymbol,	NoSymbol,	/* 34*/
	XK_6,   	XK_asciicircum,	NoSymbol,	NoSymbol,	/* 35*/
	XK_7,   	XK_ampersand,	NoSymbol,	NoSymbol,	/* 36*/
	XK_8,   	XK_asterisk,	NoSymbol,	NoSymbol,	/* 37*/
	XK_9,   	XK_parenleft,	NoSymbol,	NoSymbol,	/* 38*/
	XK_0,   	XK_parenright,	NoSymbol,	NoSymbol,	/* 39*/
	XK_minus,	XK_underscore,	NoSymbol,	NoSymbol,	/* 40*/
	XK_equal,	XK_plus,	NoSymbol,	NoSymbol,	/* 41*/
	XK_grave,	XK_asciitilde,	XK_acute,	NoSymbol,	/* 42*/
	XK_BackSpace,	NoSymbol,	NoSymbol,	NoSymbol,	/* 43*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 44*/
	XK_KP_Equal,  	NoSymbol,  	XK_R4,		NoSymbol,	/* 45*/
	XK_KP_Divide,  	NoSymbol,  	XK_R5,		NoSymbol,	/* 46*/
	XK_KP_Multiply, NoSymbol,  	XK_R6,		NoSymbol,	/* 47*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 48*/
	SunXK_Front,  	NoSymbol,  	XK_L5,		NoSymbol,	/* 49*/
	XK_KP_Delete,	XK_KP_Decimal,	NoSymbol,	NoSymbol,	/* 50*/
	SunXK_Copy,  	NoSymbol,  	XK_L6,		NoSymbol,	/* 51*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 52*/
	XK_Tab, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 53*/
	XK_Q,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 54*/
	XK_W,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 55*/
	XK_E,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 56*/
	XK_R,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 57*/
	XK_T,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 58*/
	XK_Y,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 59*/
	XK_U,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 60*/
	XK_I,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 61*/
	XK_O,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 62*/
	XK_P,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 63*/
	XK_bracketleft,	XK_braceleft,	NoSymbol,	NoSymbol,	/* 64*/
	XK_bracketright,XK_braceright,	NoSymbol,	NoSymbol,	/* 65*/
	XK_Delete,	NoSymbol,	NoSymbol,	NoSymbol,	/* 66*/
	SunXK_Compose,	NoSymbol,	NoSymbol,	NoSymbol,	/* 67*/
	XK_KP_Home,  	XK_KP_7,  	XK_R7,		NoSymbol,	/* 68*/
	XK_KP_Up,  	XK_KP_8,  	XK_R8,		NoSymbol,	/* 69*/
	XK_KP_Prior,  	XK_KP_9,  	XK_R9,		NoSymbol,	/* 70*/
	XK_KP_Subtract,	NoSymbol,	NoSymbol,	NoSymbol,	/* 71*/
	SunXK_Open,  	NoSymbol,  	XK_L7,		NoSymbol,	/* 72*/
	SunXK_Paste,  	NoSymbol,  	XK_L8,		NoSymbol,	/* 73*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 74*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 75*/
	XK_Control_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 76*/
	XK_A,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 77*/
	XK_S,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 78*/
	XK_D,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 79*/
	XK_F,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 80*/
	XK_G,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 81*/
	XK_H,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 82*/
	XK_J,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 83*/
	XK_K,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 84*/
	XK_L,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 85*/
	XK_semicolon,	XK_colon,	NoSymbol,	NoSymbol,	/* 86*/
	XK_apostrophe,	XK_quotedbl,	NoSymbol,	NoSymbol,	/* 87*/
	XK_backslash,	XK_bar, 	XK_brokenbar,	NoSymbol,	/* 88*/
	XK_Return,	NoSymbol,	NoSymbol,	NoSymbol,	/* 89*/
	XK_KP_Enter,	NoSymbol,	NoSymbol,	NoSymbol,	/* 90*/
	XK_KP_Left,	XK_KP_4, 	XK_R10,		NoSymbol,	/* 91*/
	NoSymbol, 	XK_KP_5, 	XK_R11,		NoSymbol,	/* 92*/
	XK_KP_Right,	XK_KP_6, 	XK_R12,		NoSymbol,	/* 93*/
	XK_KP_Insert,	XK_KP_0,	NoSymbol,	NoSymbol,	/* 94*/
	SunXK_Find,  	NoSymbol,  	XK_L9,		NoSymbol,	/* 95*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 96*/
	SunXK_Cut, 	NoSymbol, 	XK_L10,		NoSymbol,	/* 97*/
	XK_Num_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 98*/
	XK_Shift_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 99*/
	XK_Z,   	NoSymbol,	NoSymbol,	NoSymbol,	/*100*/
	XK_X,   	NoSymbol,	NoSymbol,	NoSymbol,	/*101*/
	XK_C,   	NoSymbol,	NoSymbol,	NoSymbol,	/*102*/
	XK_V,   	NoSymbol,	NoSymbol,	NoSymbol,	/*103*/
	XK_B,   	NoSymbol,	NoSymbol,	NoSymbol,	/*104*/
	XK_N,   	NoSymbol,	NoSymbol,	NoSymbol,	/*105*/
	XK_M,   	NoSymbol,	NoSymbol,	NoSymbol,	/*106*/
	XK_comma,	XK_less,	NoSymbol,	NoSymbol,	/*107*/
	XK_period,	XK_greater,	NoSymbol,	NoSymbol,	/*108*/
	XK_slash,	XK_question,	NoSymbol,	NoSymbol,	/*109*/
	XK_Shift_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*110*/
	XK_Linefeed,	NoSymbol,	NoSymbol,	NoSymbol,	/*111*/
	XK_KP_End, 	XK_KP_1, 	XK_R13,		NoSymbol, 	/*112*/
	XK_KP_Down,	XK_KP_2, 	XK_R14,		NoSymbol,	/*113*/
	XK_KP_Next, 	XK_KP_3, 	XK_R15,		NoSymbol,	/*114*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*115*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*116*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*117*/
	XK_Help,	NoSymbol,	NoSymbol,	NoSymbol,	/*118*/
	XK_Caps_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/*119*/
	XK_Meta_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*120*/
	XK_space,	NoSymbol,	NoSymbol,	NoSymbol,	/*121*/
	XK_Meta_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*122*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*123*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*124*/
	XK_KP_Add,	NoSymbol,	NoSymbol,	NoSymbol,	/*125*/
};

#define Taiwan4Modmap Generic5Modmap

#else

#define Taiwan4Keymap NULL
#define Taiwan4Modmap NULL

#endif /* TAIWAN4 */


#ifdef TAI5

static KeySym Taiwan5Keymap[] = {
	SunXK_Stop,  	NoSymbol,  	XK_L1,		NoSymbol,	/*  1*/
	SunXK_AudioLowerVolume,SunXK_VideoLowerBrightness,NoSymbol,	NoSymbol,	/*  2*/
	SunXK_Again,  	NoSymbol,  	XK_L2,		NoSymbol,	/*  3*/
	SunXK_AudioRaiseVolume,SunXK_VideoRaiseBrightness,NoSymbol,	NoSymbol,	/*  4*/
	XK_F1,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  5*/
	XK_F2,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  6*/
	XK_F10, 	NoSymbol,	NoSymbol,	NoSymbol,	/*  7*/
	XK_F3,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  8*/
	XK_F11,		NoSymbol,	SunXK_F36,	NoSymbol,	/*  9*/
	XK_F4,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 10*/
	XK_F12,		NoSymbol,	SunXK_F37,	NoSymbol,	/* 11*/
	XK_F5,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 12*/
	SunXK_AltGraph,	NoSymbol,	NoSymbol,	NoSymbol,	/* 13*/
	XK_F6,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 14*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 15*/
	XK_F7,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 16*/
	XK_F8,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 17*/
	XK_F9,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 18*/
	XK_Alt_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 19*/
	XK_Up,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 20*/
	XK_Pause,  	NoSymbol,  	XK_Break,	NoSymbol,	/* 21*/
	XK_Print,  	NoSymbol,  	XK_Sys_Req,	SunXK_Sys_Req,	/* 22*/
	XK_Scroll_Lock,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 23*/
	XK_Left,	NoSymbol,	NoSymbol,	NoSymbol,	/* 24*/
	SunXK_Props,  	NoSymbol,  	XK_L3,		NoSymbol,	/* 25*/
	SunXK_Undo,  	NoSymbol,  	XK_L4,		NoSymbol,	/* 26*/
	XK_Down,	NoSymbol,	NoSymbol,	NoSymbol,	/* 27*/
	XK_Right,	NoSymbol,	NoSymbol,	NoSymbol,	/* 28*/
	XK_Escape,	NoSymbol,	NoSymbol,	NoSymbol,	/* 29*/
	XK_1,   	XK_exclam,	NoSymbol,	NoSymbol,	/* 30*/
	XK_2,   	XK_at,  	NoSymbol,	NoSymbol,	/* 31*/
	XK_3,   	XK_numbersign,	NoSymbol,	NoSymbol,	/* 32*/
	XK_4,   	XK_dollar,	NoSymbol,	NoSymbol,	/* 33*/
	XK_5,   	XK_percent,	NoSymbol,	NoSymbol,	/* 34*/
	XK_6,   	XK_asciicircum,	NoSymbol,	NoSymbol,	/* 35*/
	XK_7,   	XK_ampersand,	NoSymbol,	NoSymbol,	/* 36*/
	XK_8,   	XK_asterisk,	NoSymbol,	NoSymbol,	/* 37*/
	XK_9,   	XK_parenleft,	NoSymbol,	NoSymbol,	/* 38*/
	XK_0,   	XK_parenright,	NoSymbol,	NoSymbol,	/* 39*/
	XK_minus,	XK_underscore,	NoSymbol,	NoSymbol,	/* 40*/
	XK_equal,	XK_plus,	NoSymbol,	NoSymbol,	/* 41*/
	XK_grave,	XK_asciitilde,	XK_acute,	NoSymbol,	/* 42*/
	XK_BackSpace,	NoSymbol,	NoSymbol,	NoSymbol,	/* 43*/
	XK_Insert,	NoSymbol,	NoSymbol,	NoSymbol,	/* 44*/
	SunXK_AudioMute,SunXK_VideoDegauss,NoSymbol,	NoSymbol,	/* 45*/
	XK_KP_Divide,  	NoSymbol,  	NoSymbol,	NoSymbol,	/* 46*/
	XK_KP_Multiply,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 47*/
	SunXK_PowerSwitch,SunXK_PowerSwitchShift,	NoSymbol,	NoSymbol,	/* 48*/
	SunXK_Front,  	NoSymbol,  	XK_L5,		NoSymbol,	/* 49*/
	XK_KP_Delete,	XK_KP_Decimal,	NoSymbol,	NoSymbol,	/* 50*/
	SunXK_Copy,  	NoSymbol,  	XK_L6,		NoSymbol,	/* 51*/
	XK_Home,	NoSymbol,	NoSymbol,	NoSymbol,	/* 52*/
	XK_Tab, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 53*/
	XK_Q,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 54*/
	XK_W,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 55*/
	XK_E,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 56*/
	XK_R,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 57*/
	XK_T,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 58*/
	XK_Y,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 59*/
	XK_U,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 60*/
	XK_I,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 61*/
	XK_O,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 62*/
	XK_P,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 63*/
	XK_bracketleft,	XK_braceleft,	NoSymbol,	NoSymbol,	/* 64*/
	XK_bracketright,XK_braceright,	NoSymbol,	NoSymbol,	/* 65*/
	XK_Delete,	NoSymbol,	NoSymbol,	NoSymbol,	/* 66*/
	SunXK_Compose,	NoSymbol,	NoSymbol,	NoSymbol,	/* 67*/
	XK_KP_Home,  	XK_KP_7,  	NoSymbol,	NoSymbol,	/* 68*/
	XK_KP_Up,  	XK_KP_8,  	NoSymbol,	NoSymbol,	/* 69*/
	XK_KP_Prior,  	XK_KP_9,  	NoSymbol,	NoSymbol,	/* 70*/
	XK_KP_Subtract,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 71*/
	SunXK_Open,  	NoSymbol,  	XK_L7,		NoSymbol,	/* 72*/
	SunXK_Paste,  	NoSymbol,  	XK_L8,		NoSymbol,	/* 73*/
	XK_End, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 74*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 75*/
	XK_Control_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 76*/
	XK_A,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 77*/
	XK_S,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 78*/
	XK_D,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 79*/
	XK_F,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 80*/
	XK_G,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 81*/
	XK_H,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 82*/
	XK_J,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 83*/
	XK_K,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 84*/
	XK_L,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 85*/
	XK_semicolon,	XK_colon,	NoSymbol,	NoSymbol,	/* 86*/
	XK_apostrophe,	XK_quotedbl,	NoSymbol,	NoSymbol,	/* 87*/
	XK_backslash,	XK_bar, 	XK_brokenbar,	NoSymbol,	/* 88*/
	XK_Return,	NoSymbol,	NoSymbol,	NoSymbol,	/* 89*/
	XK_KP_Enter,	NoSymbol,	NoSymbol,	NoSymbol,	/* 90*/
	XK_KP_Left,	XK_KP_4, 	NoSymbol,	NoSymbol,	/* 91*/
	NoSymbol, 	XK_KP_5, 	NoSymbol,	NoSymbol,	/* 92*/
	XK_KP_Right,	XK_KP_6, 	NoSymbol,	NoSymbol,	/* 93*/
	XK_KP_Insert,	XK_KP_0,	NoSymbol,	NoSymbol,	/* 94*/
	SunXK_Find,  	NoSymbol,  	XK_L9,		NoSymbol,	/* 95*/
	XK_Prior,	NoSymbol,	NoSymbol,	NoSymbol,	/* 96*/
	SunXK_Cut, 	NoSymbol, 	XK_L10,		NoSymbol,	/* 97*/
	XK_Num_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 98*/
	XK_Shift_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 99*/
	XK_Z,   	NoSymbol,	NoSymbol,	NoSymbol,	/*100*/
	XK_X,   	NoSymbol,	NoSymbol,	NoSymbol,	/*101*/
	XK_C,   	NoSymbol,	NoSymbol,	NoSymbol,	/*102*/
	XK_V,   	NoSymbol,	NoSymbol,	NoSymbol,	/*103*/
	XK_B,   	NoSymbol,	NoSymbol,	NoSymbol,	/*104*/
	XK_N,   	NoSymbol,	NoSymbol,	NoSymbol,	/*105*/
	XK_M,   	NoSymbol,	NoSymbol,	NoSymbol,	/*106*/
	XK_comma,	XK_less,	NoSymbol,	NoSymbol,	/*107*/
	XK_period,	XK_greater,	NoSymbol,	NoSymbol,	/*108*/
	XK_slash,	XK_question,	NoSymbol,	NoSymbol,	/*109*/
	XK_Shift_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*110*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*111*/
	XK_KP_End, 	XK_KP_1, 	NoSymbol,	NoSymbol, 	/*112*/
	XK_KP_Down,	XK_KP_2, 	NoSymbol,	NoSymbol,	/*113*/
	XK_KP_Next, 	XK_KP_3, 	NoSymbol,	NoSymbol,	/*114*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*115*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*116*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*117*/
	XK_Help,	NoSymbol,	NoSymbol,	NoSymbol,	/*118*/
	XK_Caps_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/*119*/
	XK_Meta_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*120*/
	XK_space,	NoSymbol,	NoSymbol,	NoSymbol,	/*121*/
	XK_Meta_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*122*/
	XK_Next,	NoSymbol,	NoSymbol,	NoSymbol,	/*123*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*124*/
	XK_KP_Add,	NoSymbol,	NoSymbol,	NoSymbol,	/*125*/
};

#define Taiwan5Modmap Generic5Modmap

#else

#define Taiwan5Keymap NULL
#define Taiwan5Modmap NULL

#endif /* TAIWAN5 */


#ifdef UK4

static KeySym UK4Keymap[] = {
	SunXK_Stop,  	NoSymbol,  	XK_L1,		NoSymbol,	/*  1*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  2*/
	SunXK_Again,  	NoSymbol,  	XK_L2,		NoSymbol,	/*  3*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  4*/
	XK_F1,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  5*/
	XK_F2,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  6*/
	XK_F10, 	NoSymbol,	NoSymbol,	NoSymbol,	/*  7*/
	XK_F3,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  8*/
	XK_F11,		NoSymbol,	SunXK_F36,	NoSymbol,	/*  9*/
	XK_F4,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 10*/
	XK_F12,		NoSymbol,	SunXK_F37,	NoSymbol,	/* 11*/
	XK_F5,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 12*/
	SunXK_AltGraph,	NoSymbol,	NoSymbol,	NoSymbol,	/* 13*/
	XK_F6,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 14*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 15*/
	XK_F7,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 16*/
	XK_F8,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 17*/
	XK_F9,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 18*/
	XK_Alt_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 19*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 20*/
	XK_Pause,  	NoSymbol,  	XK_R1,		NoSymbol,	/* 21*/
	XK_Print,  	NoSymbol,  	XK_R2,		NoSymbol,	/* 22*/
	XK_Break,  	XK_Scroll_Lock,	XK_R3,		NoSymbol,	/* 23*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 24*/
	SunXK_Props,  	NoSymbol,  	XK_L3,		NoSymbol,	/* 25*/
	SunXK_Undo,  	NoSymbol,  	XK_L4,		NoSymbol,	/* 26*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 27*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 28*/
	XK_Escape,	NoSymbol,	NoSymbol,	NoSymbol,	/* 29*/
	XK_1,   	XK_exclam,	XK_brokenbar,	NoSymbol,	/* 30*/
	XK_2,   	XK_at,  	NoSymbol,	NoSymbol,	/* 31*/
	XK_3,   	XK_sterling,	XK_numbersign,	NoSymbol,	/* 32*/
	XK_4,   	XK_dollar,	NoSymbol,	NoSymbol,	/* 33*/
	XK_5,   	XK_percent,	NoSymbol,	NoSymbol,	/* 34*/
	XK_6,   	XK_asciicircum,	NoSymbol,	NoSymbol,	/* 35*/
	XK_7,   	XK_ampersand,	NoSymbol,	NoSymbol,	/* 36*/
	XK_8,   	XK_asterisk,	NoSymbol,	NoSymbol,	/* 37*/
	XK_9,   	XK_parenleft,	NoSymbol,	NoSymbol,	/* 38*/
	XK_0,   	XK_parenright,	NoSymbol,	NoSymbol,	/* 39*/
	XK_minus,	XK_underscore,	XK_notsign,	NoSymbol,	/* 40*/
	XK_equal,	XK_plus,	NoSymbol,	NoSymbol,	/* 41*/
	XK_grave,	XK_asciitilde,	XK_acute,	NoSymbol,	/* 42*/
	XK_BackSpace,	NoSymbol,	NoSymbol,	NoSymbol,	/* 43*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 44*/
	XK_KP_Equal,  	NoSymbol,  	XK_R4,		NoSymbol,	/* 45*/
	XK_KP_Divide,  	NoSymbol,  	XK_R5,		NoSymbol,	/* 46*/
	XK_KP_Multiply,	NoSymbol,  	XK_R6,		NoSymbol,	/* 47*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 48*/
	SunXK_Front,  	NoSymbol,  	XK_L5,		NoSymbol,	/* 49*/
	XK_KP_Delete,	XK_KP_Decimal,	NoSymbol,	NoSymbol,	/* 50*/
	SunXK_Copy,  	NoSymbol,  	XK_L6,		NoSymbol,	/* 51*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 52*/
	XK_Tab, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 53*/
	XK_Q,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 54*/
	XK_W,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 55*/
	XK_E,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 56*/
	XK_R,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 57*/
	XK_T,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 58*/
	XK_Y,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 59*/
	XK_U,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 60*/
	XK_I,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 61*/
	XK_O,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 62*/
	XK_P,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 63*/
	XK_bracketleft,	XK_braceleft,	NoSymbol,	NoSymbol,	/* 64*/
	XK_bracketright,XK_braceright,	NoSymbol,	NoSymbol,	/* 65*/
	XK_Delete,	NoSymbol,	NoSymbol,	NoSymbol,	/* 66*/
	SunXK_Compose,	NoSymbol,	NoSymbol,	NoSymbol,	/* 67*/
	XK_KP_Home,  	XK_KP_7,  	XK_R7,		NoSymbol,	/* 68*/
	XK_KP_Up,  	XK_KP_8,  	XK_R8,		NoSymbol,	/* 69*/
	XK_KP_Prior,  	XK_KP_9,  	XK_R9,		NoSymbol,	/* 70*/
	XK_KP_Subtract,	NoSymbol,	NoSymbol,	NoSymbol,	/* 71*/
	SunXK_Open,  	NoSymbol,  	XK_L7,		NoSymbol,	/* 72*/
	SunXK_Paste,  	NoSymbol,  	XK_L8,		NoSymbol,	/* 73*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 74*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 75*/
	XK_Control_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 76*/
	XK_A,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 77*/
	XK_S,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 78*/
	XK_D,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 79*/
	XK_F,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 80*/
	XK_G,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 81*/
	XK_H,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 82*/
	XK_J,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 83*/
	XK_K,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 84*/
	XK_L,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 85*/
	XK_semicolon,	XK_colon,	NoSymbol,	NoSymbol,	/* 86*/
	XK_apostrophe,	XK_quotedbl,	XK_acute,	NoSymbol,	/* 87*/
	XK_backslash,	XK_bar, 	NoSymbol,	NoSymbol,	/* 88*/
	XK_Return,	NoSymbol,	NoSymbol,	NoSymbol,	/* 89*/
	XK_KP_Enter,	NoSymbol,	NoSymbol,	NoSymbol,	/* 90*/
	XK_KP_Left,	XK_KP_4, 	XK_R10,		NoSymbol,	/* 91*/
	NoSymbol, 	XK_KP_5, 	XK_R11,		NoSymbol,	/* 92*/
	XK_KP_Right,	XK_KP_6, 	XK_R12,		NoSymbol,	/* 93*/
	XK_KP_Insert,	XK_KP_0,	NoSymbol,	NoSymbol,	/* 94*/
	SunXK_Find,  	NoSymbol,  	XK_L9,		NoSymbol,	/* 95*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 96*/
	SunXK_Cut, 	NoSymbol, 	XK_L10,		NoSymbol,	/* 97*/
	XK_Num_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 98*/
	XK_Shift_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 99*/
	XK_Z,   	NoSymbol,	NoSymbol,	NoSymbol,	/*100*/
	XK_X,   	NoSymbol,	NoSymbol,	NoSymbol,	/*101*/
	XK_C,   	NoSymbol,	NoSymbol,	NoSymbol,	/*102*/
	XK_V,   	NoSymbol,	NoSymbol,	NoSymbol,	/*103*/
	XK_B,   	NoSymbol,	NoSymbol,	NoSymbol,	/*104*/
	XK_N,   	NoSymbol,	NoSymbol,	NoSymbol,	/*105*/
	XK_M,   	NoSymbol,	NoSymbol,	NoSymbol,	/*106*/
	XK_comma,	XK_less,	NoSymbol,	NoSymbol,	/*107*/
	XK_period,	XK_greater,	NoSymbol,	NoSymbol,	/*108*/
	XK_slash,	XK_question,	NoSymbol,	NoSymbol,	/*109*/
	XK_Shift_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*110*/
	XK_Linefeed,	NoSymbol,	NoSymbol,	NoSymbol,	/*111*/
	XK_KP_End, 	XK_KP_1, 	XK_R13,		NoSymbol, 	/*112*/
	XK_KP_Down,	XK_KP_2, 	XK_R14,		NoSymbol,	/*113*/
	XK_KP_Next, 	XK_KP_3, 	XK_R15,		NoSymbol,	/*114*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*115*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*116*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*117*/
	XK_Help,	NoSymbol,	NoSymbol,	NoSymbol,	/*118*/
	XK_Caps_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/*119*/
	XK_Meta_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*120*/
	XK_space,	NoSymbol,	NoSymbol,	NoSymbol,	/*121*/
	XK_Meta_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*122*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*123*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*124*/
	XK_KP_Add,	NoSymbol,	NoSymbol,	NoSymbol,	/*125*/
};

#define UK4Modmap Generic5Modmap

#else

#define UK4Keymap NULL
#define UK4Modmap NULL

#endif /* UK4 */


#ifdef UK5

static KeySym UK5Keymap[] = {
	SunXK_Stop,  	NoSymbol,  	XK_L1,		NoSymbol,	/*  1*/
	SunXK_AudioLowerVolume,SunXK_VideoLowerBrightness,NoSymbol,	NoSymbol,	/*  2*/
	SunXK_Again,  	NoSymbol,  	XK_L2,		NoSymbol,	/*  3*/
	SunXK_AudioRaiseVolume,SunXK_VideoRaiseBrightness,NoSymbol,	NoSymbol,	/*  4*/
	XK_F1,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  5*/
	XK_F2,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  6*/
	XK_F10, 	NoSymbol,	NoSymbol,	NoSymbol,	/*  7*/
	XK_F3,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  8*/
	XK_F11,		NoSymbol,	SunXK_F36,	NoSymbol,	/*  9*/
	XK_F4,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 10*/
	XK_F12,		NoSymbol,	SunXK_F37,	NoSymbol,	/* 11*/
	XK_F5,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 12*/
	SunXK_AltGraph,	NoSymbol,	NoSymbol,	NoSymbol,	/* 13*/
	XK_F6,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 14*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 15*/
	XK_F7,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 16*/
	XK_F8,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 17*/
	XK_F9,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 18*/
	XK_Alt_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 19*/
	XK_Up,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 20*/
	XK_Pause,  	NoSymbol,  	XK_Break,	NoSymbol,	/* 21*/
	XK_Print,  	NoSymbol,  	XK_Sys_Req,	SunXK_Sys_Req,	/* 22*/
	XK_Scroll_Lock, NoSymbol,  	NoSymbol,	NoSymbol,	/* 23*/
	XK_Left,	NoSymbol,	NoSymbol,	NoSymbol,	/* 24*/
	SunXK_Props,  	NoSymbol,  	XK_L3,		NoSymbol,	/* 25*/
	SunXK_Undo,  	NoSymbol,  	XK_L4,		NoSymbol,	/* 26*/
	XK_Down,	NoSymbol,	NoSymbol,	NoSymbol,	/* 27*/
	XK_Right,	NoSymbol,	NoSymbol,	NoSymbol,	/* 28*/
	XK_Escape,	NoSymbol,	NoSymbol,	NoSymbol,	/* 29*/
	XK_1,   	XK_exclam,	NoSymbol,	NoSymbol,	/* 30*/
	XK_2,   	XK_quotedbl,	NoSymbol,	NoSymbol,	/* 31*/
	XK_3,   	XK_sterling,	NoSymbol,	NoSymbol,	/* 32*/
	XK_4,   	XK_dollar,	NoSymbol,	NoSymbol,	/* 33*/
	XK_5,   	XK_percent,	NoSymbol,	NoSymbol,	/* 34*/
	XK_6,   	XK_asciicircum,	NoSymbol,	NoSymbol,	/* 35*/
	XK_7,   	XK_ampersand,	NoSymbol,	NoSymbol,	/* 36*/
	XK_8,   	XK_asterisk,	NoSymbol,	NoSymbol,	/* 37*/
	XK_9,   	XK_parenleft,	NoSymbol,	NoSymbol,	/* 38*/
	XK_0,   	XK_parenright,	NoSymbol,	NoSymbol,	/* 39*/
	XK_minus,	XK_underscore,	NoSymbol,	NoSymbol,	/* 40*/
	XK_equal,	XK_plus,	NoSymbol,	NoSymbol,	/* 41*/
	XK_grave,	XK_notsign,	XK_brokenbar,	NoSymbol,	/* 42*/
	XK_BackSpace,	NoSymbol,	NoSymbol,	NoSymbol,	/* 43*/
	XK_Insert,	NoSymbol,	NoSymbol,	NoSymbol,	/* 44*/
	SunXK_AudioMute,SunXK_VideoDegauss,NoSymbol,	NoSymbol,	/* 45*/
	XK_KP_Divide,  	NoSymbol,  	NoSymbol,	NoSymbol,	/* 46*/
	XK_KP_Multiply,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 47*/
	SunXK_PowerSwitch,SunXK_PowerSwitchShift,	NoSymbol,	NoSymbol,	/* 48*/
	SunXK_Front,  	NoSymbol,  	XK_L5,		NoSymbol,	/* 49*/
	XK_KP_Delete,	XK_KP_Decimal,	NoSymbol,	NoSymbol,	/* 50*/
	SunXK_Copy,  	NoSymbol,  	XK_L6,		NoSymbol,	/* 51*/
	XK_Home,	NoSymbol,	NoSymbol,	NoSymbol,	/* 52*/
	XK_Tab, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 53*/
	XK_Q,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 54*/
	XK_W,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 55*/
	XK_E,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 56*/
	XK_R,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 57*/
	XK_T,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 58*/
	XK_Y,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 59*/
	XK_U,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 60*/
	XK_I,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 61*/
	XK_O,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 62*/
	XK_P,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 63*/
	XK_bracketleft,	XK_braceleft,	NoSymbol,	NoSymbol,	/* 64*/
	XK_bracketright,XK_braceright,	NoSymbol,	NoSymbol,	/* 65*/
	XK_Delete,	NoSymbol,	NoSymbol,	NoSymbol,	/* 66*/
	SunXK_Compose,	NoSymbol,	NoSymbol,	NoSymbol,	/* 67*/
	XK_KP_Home,  	XK_KP_7,  	NoSymbol,	NoSymbol,	/* 68*/
	XK_KP_Up,  	XK_KP_8,  	NoSymbol,	NoSymbol,	/* 69*/
	XK_KP_Prior,  	XK_KP_9,  	NoSymbol,	NoSymbol,	/* 70*/
	XK_KP_Subtract,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 71*/
	SunXK_Open,  	NoSymbol,  	XK_L7,		NoSymbol,	/* 72*/
	SunXK_Paste,  	NoSymbol,  	XK_L8,		NoSymbol,	/* 73*/
	XK_End, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 74*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 75*/
	XK_Control_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 76*/
	XK_A,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 77*/
	XK_S,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 78*/
	XK_D,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 79*/
	XK_F,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 80*/
	XK_G,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 81*/
	XK_H,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 82*/
	XK_J,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 83*/
	XK_K,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 84*/
	XK_L,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 85*/
	XK_semicolon,	XK_colon,	NoSymbol,	NoSymbol,	/* 86*/
	XK_apostrophe,	XK_at,  	XK_acute,	NoSymbol,	/* 87*/
	XK_numbersign,	XK_asciitilde,	NoSymbol,	NoSymbol,	/* 88*/
	XK_Return,	NoSymbol,	NoSymbol,	NoSymbol,	/* 89*/
	XK_KP_Enter,	NoSymbol,	NoSymbol,	NoSymbol,	/* 90*/
	XK_KP_Left,	XK_KP_4, 	NoSymbol,	NoSymbol,	/* 91*/
	NoSymbol, 	XK_KP_5, 	NoSymbol,	NoSymbol,	/* 92*/
	XK_KP_Right,	XK_KP_6, 	NoSymbol,	NoSymbol,	/* 93*/
	XK_KP_Insert,	XK_KP_0,	NoSymbol,	NoSymbol,	/* 94*/
	SunXK_Find,  	NoSymbol,  	XK_L9,		NoSymbol,	/* 95*/
	XK_Prior,	NoSymbol,	NoSymbol,	NoSymbol,	/* 96*/
	SunXK_Cut, 	NoSymbol, 	XK_L10,		NoSymbol,	/* 97*/
	XK_Num_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 98*/
	XK_Shift_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 99*/
	XK_Z,   	NoSymbol,	NoSymbol,	NoSymbol,	/*100*/
	XK_X,   	NoSymbol,	NoSymbol,	NoSymbol,	/*101*/
	XK_C,   	NoSymbol,	NoSymbol,	NoSymbol,	/*102*/
	XK_V,   	NoSymbol,	NoSymbol,	NoSymbol,	/*103*/
	XK_B,   	NoSymbol,	NoSymbol,	NoSymbol,	/*104*/
	XK_N,   	NoSymbol,	NoSymbol,	NoSymbol,	/*105*/
	XK_M,   	NoSymbol,	NoSymbol,	NoSymbol,	/*106*/
	XK_comma,	XK_less,	NoSymbol,	NoSymbol,	/*107*/
	XK_period,	XK_greater,	NoSymbol,	NoSymbol,	/*108*/
	XK_slash,	XK_question,	NoSymbol,	NoSymbol,	/*109*/
	XK_Shift_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*110*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*111*/
	XK_KP_End, 	XK_KP_1, 	NoSymbol,	NoSymbol, 	/*112*/
	XK_KP_Down,	XK_KP_2, 	NoSymbol,	NoSymbol,	/*113*/
	XK_KP_Next, 	XK_KP_3, 	NoSymbol,	NoSymbol,	/*114*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*115*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*116*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*117*/
	XK_Help,	NoSymbol,	NoSymbol,	NoSymbol,	/*118*/
	XK_Caps_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/*119*/
	XK_Meta_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*120*/
	XK_space,	NoSymbol,	NoSymbol,	NoSymbol,	/*121*/
	XK_Meta_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*122*/
	XK_Next,	NoSymbol,	NoSymbol,	NoSymbol,	/*123*/
	XK_backslash,	XK_bar, 	NoSymbol,	NoSymbol,	/*124*/
	XK_KP_Add,	NoSymbol,	NoSymbol,	NoSymbol,	/*125*/
};

#define UK5Modmap Generic5Modmap

#else

#define UK5Keymap NULL
#define UK5Modmap NULL

#endif /* UK5 */


#ifdef US101A

static KeySym US101AKeymap[] = {
	XK_Pause,  	NoSymbol,  	XK_Break,	NoSymbol,	/*  1*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  2*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  3*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  4*/
	XK_F1,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  5*/
	XK_F2,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  6*/
	XK_F10, 	NoSymbol,	NoSymbol,	NoSymbol,	/*  7*/
	XK_F3,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  8*/
	XK_F11,		NoSymbol,	SunXK_F36,	NoSymbol,	/*  9*/
	XK_F4,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 10*/
	XK_F12,		NoSymbol,	SunXK_F37,	NoSymbol,	/* 11*/
	XK_F5,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 12*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 13*/
	XK_F6,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 14*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 15*/
	XK_F7,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 16*/
	XK_F8,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 17*/
	XK_F9,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 18*/
	SunXK_Compose,	NoSymbol,	NoSymbol,	NoSymbol,	/* 19*/
	XK_Up,  	NoSymbol,  	NoSymbol,	NoSymbol,	/* 20*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 21*/
	XK_Print,  	NoSymbol,  	XK_Sys_Req,	SunXK_Sys_Req,	/* 22*/
	XK_Scroll_Lock, NoSymbol,  	NoSymbol,	NoSymbol,	/* 23*/
	XK_Left,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 24*/
	XK_Insert,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 25*/
	XK_End, 	NoSymbol,  	NoSymbol,	NoSymbol,	/* 26*/
	XK_Down,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 27*/
	XK_Right,	NoSymbol, 	NoSymbol,	NoSymbol,	/* 28*/
	XK_Escape,	NoSymbol,	NoSymbol,	NoSymbol,	/* 29*/
	XK_1,   	XK_exclam,	NoSymbol,	NoSymbol,	/* 30*/
	XK_2,   	XK_at,  	NoSymbol,	NoSymbol,	/* 31*/
	XK_3,   	XK_numbersign,	NoSymbol,	NoSymbol,	/* 32*/
	XK_4,   	XK_dollar,	NoSymbol,	NoSymbol,	/* 33*/
	XK_5,   	XK_percent,	NoSymbol,	NoSymbol,	/* 34*/
	XK_6,   	XK_asciicircum,	NoSymbol,	NoSymbol,	/* 35*/
	XK_7,   	XK_ampersand,	NoSymbol,	NoSymbol,	/* 36*/
	XK_8,   	XK_asterisk,	NoSymbol,	NoSymbol,	/* 37*/
	XK_9,   	XK_parenleft,	NoSymbol,	NoSymbol,	/* 38*/
	XK_0,   	XK_parenright,	NoSymbol,	NoSymbol,	/* 39*/
	XK_minus,	XK_underscore,	NoSymbol,	NoSymbol,	/* 40*/
	XK_equal,	XK_plus,	NoSymbol,	NoSymbol,	/* 41*/
	XK_grave,	XK_asciitilde,	XK_acute,	NoSymbol,	/* 42*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 43*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 44*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 45*/
	XK_KP_Divide,  	NoSymbol,  	NoSymbol,	NoSymbol,	/* 46*/
	XK_KP_Multiply,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 47*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 48*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 49*/
	XK_KP_Delete,	XK_KP_Decimal,	NoSymbol,	NoSymbol,	/* 50*/
	XK_Prior,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 51*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 52*/
	XK_Tab, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 53*/
	XK_Q,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 54*/
	XK_W,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 55*/
	XK_E,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 56*/
	XK_R,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 57*/
	XK_T,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 58*/
	XK_Y,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 59*/
	XK_U,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 60*/
	XK_I,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 61*/
	XK_O,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 62*/
	XK_P,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 63*/
	XK_bracketleft,	XK_braceleft,	NoSymbol,	NoSymbol,	/* 64*/
	XK_bracketright,XK_braceright,	NoSymbol,	NoSymbol,	/* 65*/
	XK_BackSpace,	NoSymbol,	NoSymbol,	NoSymbol,	/* 66*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 67*/
	XK_KP_Home,  	XK_KP_7,  	NoSymbol,	NoSymbol,	/* 68*/
	XK_KP_Up,  	XK_KP_8,  	NoSymbol,	NoSymbol,	/* 69*/
	XK_KP_Prior,  	XK_KP_9,  	NoSymbol,	NoSymbol,	/* 70*/
	XK_KP_Subtract,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 71*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 72*/
	XK_Next,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 73*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 74*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 75*/
	XK_Control_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 76*/
	XK_A,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 77*/
	XK_S,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 78*/
	XK_D,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 79*/
	XK_F,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 80*/
	XK_G,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 81*/
	XK_H,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 82*/
	XK_J,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 83*/
	XK_K,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 84*/
	XK_L,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 85*/
	XK_semicolon,	XK_colon,	NoSymbol,	NoSymbol,	/* 86*/
	XK_apostrophe,	XK_quotedbl,	XK_acute,	NoSymbol,	/* 87*/
	XK_backslash,	XK_bar, 	NoSymbol,	NoSymbol,	/* 88*/
	XK_Return,	NoSymbol,	NoSymbol,	NoSymbol,	/* 89*/
	XK_KP_Enter,	NoSymbol,	NoSymbol,	NoSymbol,	/* 90*/
	XK_KP_Left,	XK_KP_4, 	NoSymbol,	NoSymbol,	/* 91*/
	NoSymbol, 	XK_KP_5, 	NoSymbol,	NoSymbol,	/* 92*/
	XK_KP_Right,	XK_KP_6, 	NoSymbol,	NoSymbol,	/* 93*/
	XK_KP_Insert,	XK_KP_0,	NoSymbol,	NoSymbol,	/* 94*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 95*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 96*/
	XK_Home,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 97*/
	XK_Num_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 98*/
	XK_Shift_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 99*/
	XK_Z,   	NoSymbol,	NoSymbol,	NoSymbol,	/*100*/
	XK_X,   	NoSymbol,	NoSymbol,	NoSymbol,	/*101*/
	XK_C,   	NoSymbol,	NoSymbol,	NoSymbol,	/*102*/
	XK_V,   	NoSymbol,	NoSymbol,	NoSymbol,	/*103*/
	XK_B,   	NoSymbol,	NoSymbol,	NoSymbol,	/*104*/
	XK_N,   	NoSymbol,	NoSymbol,	NoSymbol,	/*105*/
	XK_M,   	NoSymbol,	NoSymbol,	NoSymbol,	/*106*/
	XK_comma,	XK_less,	NoSymbol,	NoSymbol,	/*107*/
	XK_period,	XK_greater,	NoSymbol,	NoSymbol,	/*108*/
	XK_slash,	XK_question,	NoSymbol,	NoSymbol,	/*109*/
	XK_Shift_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*110*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*111*/
	XK_KP_End, 	XK_KP_1, 	NoSymbol,	NoSymbol, 	/*112*/
	XK_KP_Down,	XK_KP_2, 	NoSymbol,	NoSymbol,	/*113*/
	XK_KP_Next, 	XK_KP_3, 	NoSymbol,	NoSymbol,	/*114*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*115*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*116*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*117*/
	XK_Delete,	NoSymbol,  	NoSymbol,	NoSymbol,	/*118*/
	XK_Caps_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/*119*/
	XK_Alt_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*120*/
	XK_space,	NoSymbol,	NoSymbol,	NoSymbol,	/*121*/
	XK_Meta_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*122*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*123*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*124*/
	XK_KP_Add,	NoSymbol,	NoSymbol,	NoSymbol,	/*125*/
};

static SunModmapRec US101AModmap[] = {
	{  99,	ShiftMask },
	{ 110,	ShiftMask },
	{ 119,	LockMask },
	{  76,	ControlMask },
	{ 122,	Meta_Mask },
	{  98,	Num_Lock_Mask },
	{ 120,	Alt_Mask },
	{   0,	0 }
};

#else

#define US101AKeymap NULL
#define US101AModmap NULL

#endif /* US101A */


#ifdef US4

static KeySym US4Keymap[] = {
	SunXK_Stop,  	NoSymbol,  	XK_L1,		NoSymbol,	/*  1*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  2*/
	SunXK_Again,  	NoSymbol,  	XK_L2,		NoSymbol,	/*  3*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*  4*/
	XK_F1,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  5*/
	XK_F2,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  6*/
	XK_F10, 	NoSymbol,	NoSymbol,	NoSymbol,	/*  7*/
	XK_F3,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  8*/
	XK_F11,		NoSymbol,	SunXK_F36,	NoSymbol,	/*  9*/
	XK_F4,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 10*/
	XK_F12,		NoSymbol,	SunXK_F37,	NoSymbol,	/* 11*/
	XK_F5,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 12*/
	SunXK_AltGraph,	NoSymbol,	NoSymbol,	NoSymbol,	/* 13*/
	XK_F6,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 14*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 15*/
	XK_F7,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 16*/
	XK_F8,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 17*/
	XK_F9,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 18*/
	XK_Alt_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 19*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 20*/
	XK_Pause,  	NoSymbol,  	XK_R1,		NoSymbol,	/* 21*/
	XK_Print,  	NoSymbol,  	XK_R2,		NoSymbol,	/* 22*/
	XK_Break,  	XK_Scroll_Lock,	XK_R3,		NoSymbol,	/* 23*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 24*/
	SunXK_Props,  	NoSymbol,  	XK_L3,		NoSymbol,	/* 25*/
	SunXK_Undo,  	NoSymbol,  	XK_L4,		NoSymbol,	/* 26*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 27*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 28*/
	XK_Escape,	NoSymbol,	NoSymbol,	NoSymbol,	/* 29*/
	XK_1,   	XK_exclam,	NoSymbol,	NoSymbol,	/* 30*/
	XK_2,   	XK_at,  	NoSymbol,	NoSymbol,	/* 31*/
	XK_3,   	XK_numbersign,	NoSymbol,	NoSymbol,	/* 32*/
	XK_4,   	XK_dollar,	NoSymbol,	NoSymbol,	/* 33*/
	XK_5,   	XK_percent,	NoSymbol,	NoSymbol,	/* 34*/
	XK_6,   	XK_asciicircum,	NoSymbol,	NoSymbol,	/* 35*/
	XK_7,   	XK_ampersand,	NoSymbol,	NoSymbol,	/* 36*/
	XK_8,   	XK_asterisk,	NoSymbol,	NoSymbol,	/* 37*/
	XK_9,   	XK_parenleft,	NoSymbol,	NoSymbol,	/* 38*/
	XK_0,   	XK_parenright,	NoSymbol,	NoSymbol,	/* 39*/
	XK_minus,	XK_underscore,	NoSymbol,	NoSymbol,	/* 40*/
	XK_equal,	XK_plus,	NoSymbol,	NoSymbol,	/* 41*/
	XK_grave,	XK_asciitilde,	XK_acute,	NoSymbol,	/* 42*/
	XK_BackSpace,	NoSymbol,	NoSymbol,	NoSymbol,	/* 43*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 44*/
	XK_KP_Equal,  	NoSymbol,  	XK_R4,		NoSymbol,	/* 45*/
	XK_KP_Divide,  	NoSymbol,  	XK_R5,		NoSymbol,	/* 46*/
	XK_KP_Multiply,	NoSymbol,	XK_R6,		NoSymbol,	/* 47*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 48*/
	SunXK_Front,  	NoSymbol,  	XK_L5,		NoSymbol,	/* 49*/
	XK_KP_Delete,	XK_KP_Decimal,	NoSymbol,	NoSymbol,	/* 50*/
	SunXK_Copy,  	NoSymbol,  	XK_L6,		NoSymbol,	/* 51*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 52*/
	XK_Tab, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 53*/
	XK_Q,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 54*/
	XK_W,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 55*/
	XK_E,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 56*/
	XK_R,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 57*/
	XK_T,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 58*/
	XK_Y,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 59*/
	XK_U,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 60*/
	XK_I,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 61*/
	XK_O,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 62*/
	XK_P,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 63*/
	XK_bracketleft,	XK_braceleft,	NoSymbol,	NoSymbol,	/* 64*/
	XK_bracketright,XK_braceright,	NoSymbol,	NoSymbol,	/* 65*/
	XK_Delete,	NoSymbol,	NoSymbol,	NoSymbol,	/* 66*/
	SunXK_Compose,	NoSymbol,	NoSymbol,	NoSymbol,	/* 67*/
	XK_KP_Home,  	XK_KP_7,  	XK_R7,		NoSymbol,	/* 68*/
	XK_KP_Up,  	XK_KP_8,  	XK_R8,		NoSymbol,	/* 69*/
	XK_KP_Prior,  	XK_KP_9,  	XK_R9,		NoSymbol,	/* 70*/
	XK_KP_Subtract,	NoSymbol,	NoSymbol,	NoSymbol,	/* 71*/
	SunXK_Open,  	NoSymbol,  	XK_L7,		NoSymbol,	/* 72*/
	SunXK_Paste,  	NoSymbol,  	XK_L8,		NoSymbol,	/* 73*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 74*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 75*/
	XK_Control_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 76*/
	XK_A,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 77*/
	XK_S,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 78*/
	XK_D,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 79*/
	XK_F,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 80*/
	XK_G,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 81*/
	XK_H,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 82*/
	XK_J,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 83*/
	XK_K,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 84*/
	XK_L,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 85*/
	XK_semicolon,	XK_colon,	NoSymbol,	NoSymbol,	/* 86*/
	XK_apostrophe,	XK_quotedbl,	XK_acute,	NoSymbol,	/* 87*/
	XK_backslash,	XK_bar, 	NoSymbol,	NoSymbol,	/* 88*/
	XK_Return,	NoSymbol,	NoSymbol,	NoSymbol,	/* 89*/
	XK_KP_Enter,	NoSymbol,	NoSymbol,	NoSymbol,	/* 90*/
	XK_KP_Left,	XK_KP_4, 	XK_R10,		NoSymbol,	/* 91*/
	NoSymbol, 	XK_KP_5, 	XK_R11,		NoSymbol,	/* 92*/
	XK_KP_Right,	XK_KP_6, 	XK_R12,		NoSymbol,	/* 93*/
	XK_KP_Insert,	XK_KP_0,	NoSymbol,	NoSymbol,	/* 94*/
	SunXK_Find,  	NoSymbol,  	XK_L9,		NoSymbol,	/* 95*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 96*/
	SunXK_Cut, 	NoSymbol, 	XK_L10,		NoSymbol,	/* 97*/
	XK_Num_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 98*/
	XK_Shift_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 99*/
	XK_Z,   	NoSymbol,	NoSymbol,	NoSymbol,	/*100*/
	XK_X,   	NoSymbol,	NoSymbol,	NoSymbol,	/*101*/
	XK_C,   	NoSymbol,	NoSymbol,	NoSymbol,	/*102*/
	XK_V,   	NoSymbol,	NoSymbol,	NoSymbol,	/*103*/
	XK_B,   	NoSymbol,	NoSymbol,	NoSymbol,	/*104*/
	XK_N,   	NoSymbol,	NoSymbol,	NoSymbol,	/*105*/
	XK_M,   	NoSymbol,	NoSymbol,	NoSymbol,	/*106*/
	XK_comma,	XK_less,	NoSymbol,	NoSymbol,	/*107*/
	XK_period,	XK_greater,	NoSymbol,	NoSymbol,	/*108*/
	XK_slash,	XK_question,	NoSymbol,	NoSymbol,	/*109*/
	XK_Shift_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*110*/
	XK_Linefeed,	NoSymbol,	NoSymbol,	NoSymbol,	/*111*/
	XK_KP_End, 	XK_KP_1, 	XK_R13,		NoSymbol, 	/*112*/
	XK_KP_Down,	XK_KP_2, 	XK_R14,		NoSymbol,	/*113*/
	XK_KP_Next, 	XK_KP_3, 	XK_R15,		NoSymbol,	/*114*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*115*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*116*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*117*/
	XK_Help,	XK_Help,	NoSymbol,	NoSymbol,	/*118*/
	XK_Caps_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/*119*/
	XK_Meta_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*120*/
	XK_space,	NoSymbol,	NoSymbol,	NoSymbol,	/*121*/
	XK_Meta_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*122*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*123*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*124*/
	XK_KP_Add,	NoSymbol,	NoSymbol,	NoSymbol,	/*125*/
};

#define US4Modmap Generic5Modmap

#else

#define US4Keymap NULL
#define US4Modmap NULL

#endif /* US4 */


#ifdef US5

static KeySym US5Keymap[] = {
	SunXK_Stop,  	NoSymbol,  	XK_L1,		NoSymbol,	/*  1*/
	SunXK_AudioLowerVolume,SunXK_VideoLowerBrightness,NoSymbol,	NoSymbol,	/*  2*/
	SunXK_Again,  	NoSymbol,  	XK_L2,		NoSymbol,	/*  3*/
	SunXK_AudioRaiseVolume,SunXK_VideoRaiseBrightness,NoSymbol,	NoSymbol,	/*  4*/
	XK_F1,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  5*/
	XK_F2,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  6*/
	XK_F10, 	NoSymbol,	NoSymbol,	NoSymbol,	/*  7*/
	XK_F3,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  8*/
	XK_F11,		NoSymbol,	SunXK_F36,	NoSymbol,	/*  9*/
	XK_F4,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 10*/
	XK_F12,		NoSymbol,	SunXK_F37,	NoSymbol,	/* 11*/
	XK_F5,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 12*/
	SunXK_AltGraph,	NoSymbol,	NoSymbol,	NoSymbol,	/* 13*/
	XK_F6,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 14*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 15*/
	XK_F7,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 16*/
	XK_F8,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 17*/
	XK_F9,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 18*/
	XK_Alt_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 19*/
	XK_Up,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 20*/
	XK_Pause,  	NoSymbol,  	XK_Break,	NoSymbol,	/* 21*/
	XK_Print,  	NoSymbol,  	XK_Sys_Req,	SunXK_Sys_Req,	/* 22*/
	XK_Scroll_Lock,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 23*/
	XK_Left,	NoSymbol,	NoSymbol,	NoSymbol,	/* 24*/
	SunXK_Props,  	NoSymbol,  	XK_L3,		NoSymbol,	/* 25*/
	SunXK_Undo,  	NoSymbol,  	XK_L4,		NoSymbol,	/* 26*/
	XK_Down,	NoSymbol,	NoSymbol,	NoSymbol,	/* 27*/
	XK_Right,	NoSymbol,	NoSymbol,	NoSymbol,	/* 28*/
	XK_Escape,	NoSymbol,	NoSymbol,	NoSymbol,	/* 29*/
	XK_1,   	XK_exclam,	NoSymbol,	NoSymbol,	/* 30*/
	XK_2,   	XK_at,  	NoSymbol,	NoSymbol,	/* 31*/
	XK_3,   	XK_numbersign,	NoSymbol,	NoSymbol,	/* 32*/
	XK_4,   	XK_dollar,	NoSymbol,	NoSymbol,	/* 33*/
	XK_5,   	XK_percent,	NoSymbol,	NoSymbol,	/* 34*/
	XK_6,   	XK_asciicircum,	NoSymbol,	NoSymbol,	/* 35*/
	XK_7,   	XK_ampersand,	NoSymbol,	NoSymbol,	/* 36*/
	XK_8,   	XK_asterisk,	NoSymbol,	NoSymbol,	/* 37*/
	XK_9,   	XK_parenleft,	NoSymbol,	NoSymbol,	/* 38*/
	XK_0,   	XK_parenright,	NoSymbol,	NoSymbol,	/* 39*/
	XK_minus,	XK_underscore,	NoSymbol,	NoSymbol,	/* 40*/
	XK_equal,	XK_plus,	NoSymbol,	NoSymbol,	/* 41*/
	XK_grave,	XK_asciitilde,	XK_acute,	NoSymbol,	/* 42*/
	XK_BackSpace,	NoSymbol,	NoSymbol,	NoSymbol,	/* 43*/
	XK_Insert,	NoSymbol,	NoSymbol,	NoSymbol,	/* 44*/
	SunXK_AudioMute,SunXK_VideoDegauss,NoSymbol,	NoSymbol,	/* 45*/
	XK_KP_Divide,  	NoSymbol,  	NoSymbol,	NoSymbol,	/* 46*/
	XK_KP_Multiply,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 47*/
	SunXK_PowerSwitch,SunXK_PowerSwitchShift,	NoSymbol,	NoSymbol,	/* 48*/
	SunXK_Front,  	NoSymbol,  	XK_L5,		NoSymbol,	/* 49*/
	XK_KP_Delete,	XK_KP_Decimal,	NoSymbol,	NoSymbol,	/* 50*/
	SunXK_Copy,  	NoSymbol,  	XK_L6,		NoSymbol,	/* 51*/
	XK_Home,	NoSymbol,	NoSymbol,	NoSymbol,	/* 52*/
	XK_Tab, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 53*/
	XK_Q,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 54*/
	XK_W,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 55*/
	XK_E,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 56*/
	XK_R,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 57*/
	XK_T,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 58*/
	XK_Y,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 59*/
	XK_U,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 60*/
	XK_I,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 61*/
	XK_O,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 62*/
	XK_P,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 63*/
	XK_bracketleft,	XK_braceleft,	NoSymbol,	NoSymbol,	/* 64*/
	XK_bracketright,XK_braceright,	NoSymbol,	NoSymbol,	/* 65*/
	XK_Delete,	NoSymbol,	NoSymbol,	NoSymbol,	/* 66*/
	SunXK_Compose,	NoSymbol,	NoSymbol,	NoSymbol,	/* 67*/
	XK_KP_Home,	XK_KP_7,  	NoSymbol,	NoSymbol,	/* 68*/
	XK_KP_Up,  	XK_KP_8,  	NoSymbol,	NoSymbol,	/* 69*/
	XK_KP_Prior,  	XK_KP_9,  	NoSymbol,	NoSymbol,	/* 70*/
	XK_KP_Subtract,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 71*/
	SunXK_Open,  	NoSymbol,  	XK_L7,		NoSymbol,	/* 72*/
	SunXK_Paste,  	NoSymbol,  	XK_L8,		NoSymbol,	/* 73*/
	XK_End, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 74*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 75*/
	XK_Control_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 76*/
	XK_A,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 77*/
	XK_S,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 78*/
	XK_D,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 79*/
	XK_F,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 80*/
	XK_G,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 81*/
	XK_H,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 82*/
	XK_J,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 83*/
	XK_K,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 84*/
	XK_L,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 85*/
	XK_semicolon,	XK_colon,	NoSymbol,	NoSymbol,	/* 86*/
	XK_apostrophe,	XK_quotedbl,	XK_acute,	NoSymbol,	/* 87*/
	XK_backslash,	XK_bar, 	NoSymbol,	NoSymbol,	/* 88*/
	XK_Return,	NoSymbol,	NoSymbol,	NoSymbol,	/* 89*/
	XK_KP_Enter,	NoSymbol,	NoSymbol,	NoSymbol,	/* 90*/
	XK_KP_Left,	XK_KP_4, 	NoSymbol,	NoSymbol,	/* 91*/
	NoSymbol, 	XK_KP_5, 	NoSymbol,	NoSymbol,	/* 92*/
	XK_KP_Right,	XK_KP_6, 	NoSymbol,	NoSymbol,	/* 93*/
	XK_KP_Insert,	XK_KP_0,	NoSymbol,	NoSymbol,	/* 94*/
	SunXK_Find,  	NoSymbol,  	XK_L9,		NoSymbol,	/* 95*/
	XK_Prior,	NoSymbol,	NoSymbol,	NoSymbol,	/* 96*/
	SunXK_Cut, 	NoSymbol, 	XK_L10,		NoSymbol,	/* 97*/
	XK_Num_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 98*/
	XK_Shift_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 99*/
	XK_Z,   	NoSymbol,	NoSymbol,	NoSymbol,	/*100*/
	XK_X,   	NoSymbol,	NoSymbol,	NoSymbol,	/*101*/
	XK_C,   	NoSymbol,	NoSymbol,	NoSymbol,	/*102*/
	XK_V,   	NoSymbol,	NoSymbol,	NoSymbol,	/*103*/
	XK_B,   	NoSymbol,	NoSymbol,	NoSymbol,	/*104*/
	XK_N,   	NoSymbol,	NoSymbol,	NoSymbol,	/*105*/
	XK_M,   	NoSymbol,	NoSymbol,	NoSymbol,	/*106*/
	XK_comma,	XK_less,	NoSymbol,	NoSymbol,	/*107*/
	XK_period,	XK_greater,	NoSymbol,	NoSymbol,	/*108*/
	XK_slash,	XK_question,	NoSymbol,	NoSymbol,	/*109*/
	XK_Shift_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*110*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*111*/
	XK_KP_End, 	XK_KP_1, 	NoSymbol,	NoSymbol, 	/*112*/
	XK_KP_Down,	XK_KP_2, 	NoSymbol,	NoSymbol,	/*113*/
	XK_KP_Next, 	XK_KP_3, 	NoSymbol,	NoSymbol,	/*114*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*115*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*116*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*117*/
	XK_Help,	NoSymbol,	NoSymbol,	NoSymbol,	/*118*/
	XK_Caps_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/*119*/
	XK_Meta_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*120*/
	XK_space,	NoSymbol,	NoSymbol,	NoSymbol,	/*121*/
	XK_Meta_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*122*/
	XK_Next,	NoSymbol,	NoSymbol,	NoSymbol,	/*123*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*124*/
	XK_KP_Add,	NoSymbol,	NoSymbol,	NoSymbol,	/*125*/
};

#define US5Modmap Generic5Modmap

#else

#define US5Keymap NULL
#define US5Modmap NULL

#endif /* US5 */

#ifdef US_UNIX5

static KeySym US_UNIX5Keymap[] = {
	SunXK_Stop,  	NoSymbol,  	XK_L1,		NoSymbol,	/*  1*/
	SunXK_AudioLowerVolume,SunXK_VideoLowerBrightness,NoSymbol,	NoSymbol,	/*  2*/
	SunXK_Again,  	NoSymbol,  	XK_L2,		NoSymbol,	/*  3*/
	SunXK_AudioRaiseVolume,SunXK_VideoRaiseBrightness,NoSymbol,	NoSymbol,	/*  4*/
	XK_F1,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  5*/
	XK_F2,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  6*/
	XK_F10, 	NoSymbol,	NoSymbol,	NoSymbol,	/*  7*/
	XK_F3,  	NoSymbol,	NoSymbol,	NoSymbol,	/*  8*/
	XK_F11,		NoSymbol,	SunXK_F36,	NoSymbol,	/*  9*/
	XK_F4,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 10*/
	XK_F12,		NoSymbol,	SunXK_F37,	NoSymbol,	/* 11*/
	XK_F5,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 12*/
	SunXK_AltGraph,	NoSymbol,	NoSymbol,	NoSymbol,	/* 13*/
	XK_F6,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 14*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 15*/
	XK_F7,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 16*/
	XK_F8,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 17*/
	XK_F9,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 18*/
	XK_Alt_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 19*/
	XK_Up,  	NoSymbol,	NoSymbol,	NoSymbol,	/* 20*/
	XK_Pause,  	NoSymbol,  	XK_Break,	NoSymbol,	/* 21*/
	XK_Print,  	NoSymbol,  	XK_Sys_Req,	SunXK_Sys_Req,	/* 22*/
	XK_Scroll_Lock,	NoSymbol,  	NoSymbol,	NoSymbol,	/* 23*/
	XK_Left,	NoSymbol,	NoSymbol,	NoSymbol,	/* 24*/
	SunXK_Props,  	NoSymbol,  	XK_L3,		NoSymbol,	/* 25*/
	SunXK_Undo,  	NoSymbol,  	XK_L4,		NoSymbol,	/* 26*/
	XK_Down,	NoSymbol,	NoSymbol,	NoSymbol,	/* 27*/
	XK_Right,	NoSymbol,	NoSymbol,	NoSymbol,	/* 28*/
	XK_Escape,	NoSymbol,	NoSymbol,	NoSymbol,	/* 29*/
	XK_1,   	XK_exclam,	NoSymbol,	NoSymbol,	/* 30*/
	XK_2,   	XK_at,  	NoSymbol,	NoSymbol,	/* 31*/
	XK_3,   	XK_numbersign,	NoSymbol,	NoSymbol,	/* 32*/
	XK_4,   	XK_dollar,	NoSymbol,	NoSymbol,	/* 33*/
	XK_5,   	XK_percent,	NoSymbol,	NoSymbol,	/* 34*/
	XK_6,   	XK_asciicircum,	NoSymbol,	NoSymbol,	/* 35*/
	XK_7,   	XK_ampersand,	NoSymbol,	NoSymbol,	/* 36*/
	XK_8,   	XK_asterisk,	NoSymbol,	NoSymbol,	/* 37*/
	XK_9,   	XK_parenleft,	NoSymbol,	NoSymbol,	/* 38*/
	XK_0,   	XK_parenright,	NoSymbol,	NoSymbol,	/* 39*/
	XK_minus,	XK_underscore,	NoSymbol,	NoSymbol,	/* 40*/
	XK_equal,	XK_plus,	NoSymbol,	NoSymbol,	/* 41*/
	XK_grave,	XK_asciitilde,	XK_acute,	NoSymbol,	/* 42*/
	XK_BackSpace,	NoSymbol,	NoSymbol,	NoSymbol,	/* 43*/
	XK_Insert,	NoSymbol,	NoSymbol,	NoSymbol,	/* 44*/
	SunXK_AudioMute,SunXK_VideoDegauss,NoSymbol,	NoSymbol,	/* 45*/
	XK_KP_Divide,  	NoSymbol,  	NoSymbol,	NoSymbol,	/* 46*/
	XK_KP_Multiply, NoSymbol,  	NoSymbol,	NoSymbol,	/* 47*/
	SunXK_PowerSwitch,SunXK_PowerSwitchShift,	NoSymbol,	NoSymbol,	/* 48*/
	SunXK_Front,  	NoSymbol,  	XK_L5,		NoSymbol,	/* 49*/
	XK_KP_Delete,	XK_KP_Decimal,	NoSymbol,	NoSymbol,	/* 50*/
	SunXK_Copy,  	NoSymbol,  	XK_L6,		NoSymbol,	/* 51*/
	XK_Home,	NoSymbol,	NoSymbol,	NoSymbol,	/* 52*/
	XK_Tab, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 53*/
	XK_Q,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 54*/
	XK_W,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 55*/
	XK_E,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 56*/
	XK_R,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 57*/
	XK_T,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 58*/
	XK_Y,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 59*/
	XK_U,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 60*/
	XK_I,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 61*/
	XK_O,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 62*/
	XK_P,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 63*/
	XK_bracketleft,	XK_braceleft,	NoSymbol,	NoSymbol,	/* 64*/
	XK_bracketright,XK_braceright,	NoSymbol,	NoSymbol,	/* 65*/
	XK_Delete,	NoSymbol,	NoSymbol,	NoSymbol,	/* 66*/
	SunXK_Compose,	NoSymbol,	NoSymbol,	NoSymbol,	/* 67*/
	XK_KP_Home,  	XK_KP_7,  	NoSymbol,	NoSymbol,	/* 68*/
	XK_KP_Up,  	XK_KP_8,  	NoSymbol,	NoSymbol,	/* 69*/
	XK_KP_Prior,  	XK_KP_9,  	NoSymbol,	NoSymbol,	/* 70*/
	XK_KP_Subtract, NoSymbol,  	NoSymbol,	NoSymbol,	/* 71*/
	SunXK_Open,  	NoSymbol,  	XK_L7,		NoSymbol,	/* 72*/
	SunXK_Paste,  	NoSymbol,  	XK_L8,		NoSymbol,	/* 73*/
	XK_End, 	NoSymbol,	NoSymbol,	NoSymbol,	/* 74*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/* 75*/
	XK_Control_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 76*/
	XK_A,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 77*/
	XK_S,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 78*/
	XK_D,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 79*/
	XK_F,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 80*/
	XK_G,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 81*/
	XK_H,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 82*/
	XK_J,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 83*/
	XK_K,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 84*/
	XK_L,   	NoSymbol,	NoSymbol,	NoSymbol,	/* 85*/
	XK_semicolon,	XK_colon,	NoSymbol,	NoSymbol,	/* 86*/
	XK_apostrophe,	XK_quotedbl,	XK_acute,	NoSymbol,	/* 87*/
	XK_backslash,	XK_bar, 	NoSymbol,	NoSymbol,	/* 88*/
	XK_Return,	NoSymbol,	NoSymbol,	NoSymbol,	/* 89*/
	XK_KP_Enter,	NoSymbol,	NoSymbol,	NoSymbol,	/* 90*/
	XK_KP_Left,	XK_KP_4, 	NoSymbol,	NoSymbol,	/* 91*/
	NoSymbol, 	XK_KP_5, 	NoSymbol,	NoSymbol,	/* 92*/
	XK_KP_Right,	XK_KP_6, 	NoSymbol,	NoSymbol,	/* 93*/
	XK_KP_Insert,	XK_KP_0,	NoSymbol,	NoSymbol,	/* 94*/
	SunXK_Find,  	NoSymbol,  	XK_L9,		NoSymbol,	/* 95*/
	XK_Prior,	NoSymbol,	NoSymbol,	NoSymbol,	/* 96*/
	SunXK_Cut, 	NoSymbol, 	XK_L10,		NoSymbol,	/* 97*/
	XK_Num_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/* 98*/
	XK_Shift_L,	NoSymbol,	NoSymbol,	NoSymbol,	/* 99*/
	XK_Z,   	NoSymbol,	NoSymbol,	NoSymbol,	/*100*/
	XK_X,   	NoSymbol,	NoSymbol,	NoSymbol,	/*101*/
	XK_C,   	NoSymbol,	NoSymbol,	NoSymbol,	/*102*/
	XK_V,   	NoSymbol,	NoSymbol,	NoSymbol,	/*103*/
	XK_B,   	NoSymbol,	NoSymbol,	NoSymbol,	/*104*/
	XK_N,   	NoSymbol,	NoSymbol,	NoSymbol,	/*105*/
	XK_M,   	NoSymbol,	NoSymbol,	NoSymbol,	/*106*/
	XK_comma,	XK_less,	NoSymbol,	NoSymbol,	/*107*/
	XK_period,	XK_greater,	NoSymbol,	NoSymbol,	/*108*/
	XK_slash,	XK_question,	NoSymbol,	NoSymbol,	/*109*/
	XK_Shift_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*110*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*111*/
	XK_KP_End, 	XK_KP_1, 	NoSymbol,	NoSymbol, 	/*112*/
	XK_KP_Down,	XK_KP_2, 	NoSymbol,	NoSymbol,	/*113*/
	XK_KP_Next, 	XK_KP_3, 	NoSymbol,	NoSymbol,	/*114*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*115*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*116*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*117*/
	XK_Help,	NoSymbol,	NoSymbol,	NoSymbol,	/*118*/
	XK_Caps_Lock,	NoSymbol,	NoSymbol,	NoSymbol,	/*119*/
	XK_Meta_L,	NoSymbol,	NoSymbol,	NoSymbol,	/*120*/
	XK_space,	NoSymbol,	NoSymbol,	NoSymbol,	/*121*/
	XK_Meta_R,	NoSymbol,	NoSymbol,	NoSymbol,	/*122*/
	XK_Next,	NoSymbol,	NoSymbol,	NoSymbol,	/*123*/
	NoSymbol,	NoSymbol,	NoSymbol,	NoSymbol,	/*124*/
	XK_KP_Add,	NoSymbol,	NoSymbol,	NoSymbol,	/*125*/
};

#define US_UNIX5Modmap Generic5Modmap

#else

#define US_UNIX5Keymap NULL
#define US_UNIX5Modmap NULL

#endif /* US_UNIX5 */

KeySym *sunType4KeyMaps[] = {
	US4Keymap,		/* 0 */
	US4Keymap,		/* 1 */
	FranceBelg4Keymap,	/* 2 */
	Canada4Keymap,		/* 3 */
	Denmark4Keymap,		/* 4 */
	Germany4Keymap,		/* 5 */
	Italy4Keymap,		/* 6 */
	Netherland4Keymap,	/* 7 */
	Norway4Keymap,		/* 8 */
	Portugal4Keymap,	/* 9 */
	SpainLatAm4Keymap,	/* 10 */
	SwedenFin4Keymap,	/* 11 */
	SwissFr4Keymap,		/* 12 */
	SwissGe4Keymap,		/* 13 */
	UK4Keymap,		/* 14 */
	NULL,			/* 15 */
	Korea4Keymap,		/* 16 */
	Taiwan4Keymap,		/* 17 */
	NULL,			/* 18 */
	US101AKeymap,		/* 19 */
	NULL,			/* 20 */
	NULL,			/* 21 */
	NULL,			/* 22 */
	NULL,			/* 23 */
	NULL,			/* 24 */
	NULL,			/* 25 */
	NULL,			/* 26 */
	NULL,			/* 27 */
	NULL,			/* 28 */
	NULL,			/* 29 */
	NULL,			/* 30 */
	NULL,			/* 31 */
	Japan4Keymap,		/* 32 */
	US5Keymap,		/* 33 */
	US_UNIX5Keymap,		/* 34 */
	France5Keymap,		/* 35 */
	Denmark5Keymap,		/* 36 */
	Germany5Keymap,		/* 37 */
	Italy5Keymap,		/* 38 */
	Netherland5Keymap,	/* 39 */
	Norway5Keymap,		/* 40 */
	Portugal5Keymap,	/* 41 */
	Spain5Keymap,		/* 42 */
	Sweden5Keymap,		/* 43 */
	SwissFr5Keymap,		/* 44 */
	SwissGe5Keymap,		/* 45 */
	UK5Keymap,		/* 46 */
	Korea5Keymap,		/* 47 */
	Taiwan5Keymap,		/* 48 */
	Japan5Keymap,		/* 49 */
	CanadaFr5Keymap,	/* 50 */
	NULL, /* Hungary5 */	/* 51 */
	NULL, /* Poland5 */	/* 52 */
	NULL, /* Czech5 */	/* 53 */
	NULL, /* Russia5 */	/* 54 */
	NULL, 			/* 55 */
	NULL,			/* 56 */
	NULL,			/* 57 */
	NULL,			/* 58 */
	NULL,			/* 59 */
	NULL,			/* 60 */
	NULL,			/* 61 */
	NULL,			/* 62 */
	NULL, /* CanadaFr5+ */	/* 63 */
	NULL,			/* 64 */
	NULL,			/* 65 */
	NULL,			/* 66 */
	NULL,			/* 67 */
	NULL,			/* 68 */
	NULL,			/* 69 */
	NULL,			/* 70 */
	NULL,			/* 71 */
	NULL,			/* 72 */
	NULL,			/* 73 */
	NULL,			/* 74 */
	NULL,			/* 75 */
	NULL,			/* 76 */
	NULL,			/* 77 */
	NULL,			/* 78 */
	NULL,			/* 79 */
/*
 * We're punting on SPARC Voyager support for now. The OpenLook server 
 * apparently adds special semantics to Num_Lock, which requires indexing 
 * into column 5 of the keymap, which isn't handled by the core protocol 
 * at all, (it is in XKB.) We could do some tricky remapping, sort of 
 * like what the PC ddxen need to do to deal with funky PC keyboards; but
 * for now we'll just pretend that Voyager (Hobo) keyboards are the same 
 * as the equivalent Sun5 keyboard.
 */
	US5Keymap, 		/* 80 */
	US_UNIX5Keymap,		/* 81 */
	France5Keymap,		/* 82 */
	Denmark5Keymap,		/* 83 */
	Germany5Keymap,		/* 84 */
	Italy5Keymap,		/* 85 */
	Netherland5Keymap,	/* 86 */
	Norway5Keymap,		/* 87 */
	Portugal5Keymap,	/* 88 */
	Spain5Keymap,		/* 89 */
	Sweden5Keymap,		/* 90 */
	SwissFr5Keymap,		/* 91 */
	SwissGe5Keymap,		/* 92 */
	UK5Keymap,		/* 93 */
	Korea5Keymap,		/* 94 */
	Taiwan5Keymap,		/* 95 */
	Japan5Keymap,		/* 96 */
	CanadaFr5Keymap,	/* 97 */
};

int sunMaxLayout = sizeof(sunType4KeyMaps) / sizeof(sunType4KeyMaps[0]);

SunModmapRec *sunType4ModMaps[] = {
	US4Modmap,		/* 0 */
	US4Modmap,		/* 1 */
	FranceBelg4Modmap,	/* 2 */
	Canada4Modmap,		/* 3 */
	Denmark4Modmap,		/* 4 */
	Germany4Modmap,		/* 5 */
	Italy4Modmap,		/* 6 */
	Netherland4Modmap,	/* 7 */
	Norway4Modmap,		/* 8 */
	Portugal4Modmap,	/* 9 */
	SpainLatAm4Modmap,	/* 10 */
	SwedenFin4Modmap,	/* 11 */
	SwissFr4Modmap,		/* 12 */
	SwissGe4Modmap,		/* 13 */
	UK4Modmap,		/* 14 */
	NULL,			/* 15 */
	Korea4Modmap,		/* 16 */
	Taiwan4Modmap,		/* 17 */
	NULL,			/* 18 */
	US101AModmap,		/* 19 */
	NULL,			/* 20 */
	NULL,			/* 21 */
	NULL,			/* 22 */
	NULL,			/* 23 */
	NULL,			/* 24 */
	NULL,			/* 25 */
	NULL,			/* 26 */
	NULL,			/* 27 */
	NULL,			/* 28 */
	NULL,			/* 29 */
	NULL,			/* 30 */
	NULL,			/* 31 */
	Japan4Modmap,		/* 32 */
	US5Modmap,		/* 33 */
	US_UNIX5Modmap,		/* 34 */
	France5Modmap,		/* 35 */
	Denmark5Modmap,		/* 36 */
	Germany5Modmap,		/* 37 */
	Italy5Modmap,		/* 38 */
	Netherland5Modmap,	/* 39 */
	Norway5Modmap,		/* 40 */
	Portugal5Modmap,	/* 41 */
	Spain5Modmap,		/* 42 */
	Sweden5Modmap,		/* 43 */
	SwissFr5Modmap,		/* 44 */
	SwissGe5Modmap,		/* 45 */
	UK5Modmap,		/* 46 */
	Korea5Modmap,		/* 47 */
	Taiwan5Modmap,		/* 48 */
	Japan5Modmap,		/* 49 */
	CanadaFr5Modmap,	/* 50 */
	NULL, /* Hungary5 */	/* 51 */
	NULL, /* Poland5 */	/* 52 */
	NULL, /* Czech5 */	/* 53 */
	NULL, /* Russia5 */	/* 54 */
	NULL, 			/* 55 */
	NULL,			/* 56 */
	NULL,			/* 57 */
	NULL,			/* 58 */
	NULL,			/* 59 */
	NULL,			/* 60 */
	NULL,			/* 61 */
	NULL,			/* 62 */
	NULL, /* CanadaFr5+ */	/* 63 */
	NULL,			/* 64 */
	NULL,			/* 65 */
	NULL,			/* 66 */
	NULL,			/* 67 */
	NULL,			/* 68 */
	NULL,			/* 69 */
	NULL,			/* 70 */
	NULL,			/* 71 */
	NULL,			/* 72 */
	NULL,			/* 73 */
	NULL,			/* 74 */
	NULL,			/* 75 */
	NULL,			/* 76 */
	NULL,			/* 77 */
	NULL,			/* 78 */
	NULL,			/* 79 */
	US5Modmap,		/* 80 */
	US_UNIX5Modmap,		/* 81 */
	France5Modmap,		/* 82 */
	Denmark5Modmap,		/* 83 */
	Germany5Modmap,		/* 84 */
	Italy5Modmap,		/* 85 */
	Netherland5Modmap,	/* 86 */
	Norway5Modmap,		/* 87 */
	Portugal5Modmap,	/* 88 */
	Spain5Modmap,		/* 89 */
	Sweden5Modmap,		/* 90 */
	SwissFr5Modmap,		/* 91 */
	SwissGe5Modmap,		/* 92 */
	UK5Modmap,		/* 93 */
	Korea5Modmap,		/* 94 */
	Taiwan5Modmap,		/* 95 */
	Japan5Modmap,		/* 96 */
	CanadaFr5Modmap,	/* 97 */
};
