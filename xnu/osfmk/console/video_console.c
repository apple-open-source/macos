/*
 * Copyright (c) 2000-2020 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/*
 * @OSF_FREE_COPYRIGHT@
 *
 */
/*
 * @APPLE_FREE_COPYRIGHT@
 */
/*
 *	NetBSD: ite.c,v 1.16 1995/07/17 01:24:34 briggs Exp
 *
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah $Hdr: ite.c 1.28 92/12/20$
 *
 *	@(#)ite.c	8.2 (Berkeley) 1/12/94
 */

/*
 * ite.c
 *
 * The ite module handles the system console; that is, stuff printed
 * by the kernel and by user programs while "desktop" and X aren't
 * running.  Some (very small) parts are based on hp300's 4.4 ite.c,
 * hence the above copyright.
 *
 *   -- Brad and Lawrence, June 26th, 1994
 *
 */

#include <console/video_console.h>
#include <console/serial_protos.h>

#include <kern/kern_types.h>
#include <kern/kalloc.h>
#include <kern/debug.h>
#include <kern/thread_call.h>

#include <vm/pmap.h>
#include <vm/vm_kern_xnu.h>
#include <machine/machine_cpu.h>

#include <pexpert/pexpert.h>
#include <sys/kdebug.h>

#include "iso_font.c"
#if defined(XNU_TARGET_OS_OSX)
#include "progress_meter_data.c"
#endif

#include "sys/msgbuf.h"

/*
 * Generic Console (Front-End)
 * ---------------------------
 */

struct vc_info vinfo;

void noroot_icon_test(void);


extern int       disableConsoleOutput;
static boolean_t gc_enabled     = FALSE;
static boolean_t gc_initialized = FALSE;
static boolean_t vm_initialized = FALSE;

static struct {
	void (*initialize)(struct vc_info * info);
	void (*enable)(boolean_t enable);
	void (*paint_char)(unsigned int xx, unsigned int yy, unsigned char ch,
	    int attrs, unsigned char ch_previous,
	    int attrs_previous);
	void (*clear_screen)(unsigned int xx, unsigned int yy, unsigned int top,
	    unsigned int bottom, int which);
	void (*scroll_down)(int num, unsigned int top, unsigned int bottom);
	void (*scroll_up)(int num, unsigned int top, unsigned int bottom);
	void (*hide_cursor)(unsigned int xx, unsigned int yy);
	void (*show_cursor)(unsigned int xx, unsigned int yy);
	void (*update_color)(int color, boolean_t fore);
} gc_ops;

static unsigned char *gc_buffer_attributes;
static unsigned char *gc_buffer_characters;
static unsigned char *gc_buffer_colorcodes;
static unsigned char *gc_buffer_tab_stops;
static uint32_t gc_buffer_columns;
static uint32_t gc_buffer_rows;
static uint32_t gc_buffer_size;


LCK_GRP_DECLARE(vconsole_lck_grp, "vconsole");
static lck_ticket_t vcputc_lock;


#define VCPUTC_LOCK_INIT()                              \
MACRO_BEGIN                                             \
	lck_ticket_init(&vcputc_lock, &vconsole_lck_grp);   \
MACRO_END

#define VCPUTC_LOCK_LOCK()                              \
MACRO_BEGIN                                             \
	lck_ticket_lock(&vcputc_lock, &vconsole_lck_grp);   \
MACRO_END

#define VCPUTC_LOCK_UNLOCK()                            \
MACRO_BEGIN                                             \
	lck_ticket_unlock(&vcputc_lock);                    \
MACRO_END


/*
 # Attribute codes:
 # 00=none 01=bold 04=underscore 05=blink 07=reverse 08=concealed
 # Text color codes:
 # 30=black 31=red 32=green 33=yellow 34=blue 35=magenta 36=cyan 37=white
 # Background color codes:
 # 40=black 41=red 42=green 43=yellow 44=blue 45=magenta 46=cyan 47=white
 */

#define ATTR_NONE       0
#define ATTR_BOLD       1
#define ATTR_UNDER      2
#define ATTR_REVERSE    4

#define COLOR_BACKGROUND 0
#define COLOR_FOREGROUND 7

#define COLOR_CODE_GET(code, fore)        (((code) & ((fore) ? 0xF0 : 0x0F))            >> ((fore) ? 4 : 0))
#define COLOR_CODE_SET(code, color, fore) (((code) & ((fore) ? 0x0F : 0xF0)) | ((color) << ((fore) ? 4 : 0)))

static unsigned char gc_color_code;

/* VT100 state: */
#define MAXPARS 16
static unsigned int gc_x, gc_y, gc_savex, gc_savey;
static unsigned int gc_par[MAXPARS], gc_numpars, gc_hanging_cursor, gc_attr, gc_saveattr;

/* VT100 scroll region */
static unsigned int gc_scrreg_top, gc_scrreg_bottom;

enum vt100state_e {
	ESnormal,               /* Nothing yet                             */
	ESesc,                  /* Got ESC                                 */
	ESsquare,               /* Got ESC [				   */
	ESgetpars,              /* About to get or getting the parameters  */
	ESgotpars,              /* Finished getting the parameters         */
	ESfunckey,              /* Function key                            */
	EShash,                 /* DEC-specific stuff (screen align, etc.) */
	ESsetG0,                /* Specify the G0 character set            */
	ESsetG1,                /* Specify the G1 character set            */
	ESask,
	EScharsize,
	ESignore                /* Ignore this sequence                    */
} gc_vt100state = ESnormal;


enum{
	/* secs */
	kProgressAcquireDelay   = 0,
	kProgressReacquireDelay = (12 * 60 * 60),     /* 12 hrs, ie. disabled unless overridden
	                                               * by kVCAcquireImmediate */
};

static int8_t vc_rotate_matr[4][2][2] = {
	{ {  1, 0 },
	  {  0, 1 } },
	{ {  0, 1 },
	  { -1, 0 } },
	{ { -1, 0 },
	  {  0, -1 } },
	{ {  0, -1 },
	  {  1, 0 } }
};

static int gc_wrap_mode = 1, gc_relative_origin = 0;
static int gc_charset_select = 0, gc_save_charset_s = 0;
static int gc_charset[2] = { 0, 0 };
static int gc_charset_save[2] = { 0, 0 };

static void gc_clear_line(unsigned int xx, unsigned int yy, int which);
static void gc_clear_screen(unsigned int xx, unsigned int yy, int top,
    unsigned int bottom, int which);
static void gc_enable(boolean_t enable);
static void gc_hide_cursor(unsigned int xx, unsigned int yy);
static void gc_initialize(struct vc_info * info);
static boolean_t gc_is_tab_stop(unsigned int column);
static void gc_paint_char(unsigned int xx, unsigned int yy, unsigned char ch,
    int attrs);
static void gc_putchar(char ch);
static void gc_putc_askcmd(unsigned char ch);
static void gc_putc_charsetcmd(int charset, unsigned char ch);
static void gc_putc_charsizecmd(unsigned char ch);
static void gc_putc_esc(unsigned char ch);
static void gc_putc_getpars(unsigned char ch);
static void gc_putc_gotpars(unsigned char ch);
static void gc_putc_normal(unsigned char ch);
static void gc_putc_square(unsigned char ch);
static void gc_reset_screen(void);
static void gc_reset_tabs(void);
static void gc_reset_vt100(void);
static void gc_scroll_down(int num, unsigned int top, unsigned int bottom);
static void gc_scroll_up(int num, unsigned int top, unsigned int bottom);
static void gc_set_tab_stop(unsigned int column, boolean_t enabled);
static void gc_show_cursor(unsigned int xx, unsigned int yy);
static void gc_update_color(int color, boolean_t fore);

static void
gc_clear_line(unsigned int xx, unsigned int yy, int which)
{
	unsigned int start, end, i;

	/*
	 * This routine runs extremely slowly.  I don't think it's
	 * used all that often, except for To end of line.  I'll go
	 * back and speed this up when I speed up the whole vc
	 * module. --LK
	 */

	switch (which) {
	case 0:         /* To end of line	 */
		start = xx;
		end = vinfo.v_columns - 1;
		break;
	case 1:         /* To start of line	 */
		start = 0;
		end = xx;
		break;
	case 2:         /* Whole line		 */
		start = 0;
		end = vinfo.v_columns - 1;
		break;
	default:
		return;
	}

	for (i = start; i <= end; i++) {
		gc_paint_char(i, yy, ' ', ATTR_NONE);
	}
}

static void
gc_clear_screen(unsigned int xx, unsigned int yy, int top, unsigned int bottom,
    int which)
{
	if (!gc_buffer_size) {
		return;
	}

	if (xx < gc_buffer_columns && yy < gc_buffer_rows && bottom <= gc_buffer_rows) {
		uint32_t start, end;

		switch (which) {
		case 0:                 /* To end of screen	 */
			start = (yy * gc_buffer_columns) + xx;
			end = (bottom * gc_buffer_columns) - 1;
			break;
		case 1:                 /* To start of screen	 */
			start = (top * gc_buffer_columns);
			end = (yy * gc_buffer_columns) + xx;
			break;
		case 2:                 /* Whole screen		 */
			start = (top * gc_buffer_columns);
			end = (bottom * gc_buffer_columns) - 1;
			break;
		default:
			start = 0;
			end = 0;
			break;
		}

		memset(gc_buffer_attributes + start, ATTR_NONE, end - start + 1);
		memset(gc_buffer_characters + start, ' ', end - start + 1);
		memset(gc_buffer_colorcodes + start, gc_color_code, end - start + 1);
	}

	gc_ops.clear_screen(xx, yy, top, bottom, which);
}

static void
gc_enable( boolean_t enable )
{
	unsigned char *buffer_attributes = NULL;
	unsigned char *buffer_characters = NULL;
	unsigned char *buffer_colorcodes = NULL;
	unsigned char *buffer_tab_stops  = NULL;
	uint32_t buffer_columns = 0;
	uint32_t buffer_rows = 0;
	uint32_t buffer_size = 0;

	if (enable == FALSE) {
		// only disable console output if it goes to the graphics console
		if (console_is_serial() == FALSE) {
			disableConsoleOutput = TRUE;
		}
		gc_enabled           = FALSE;
		gc_ops.enable(FALSE);
	}

	VCPUTC_LOCK_LOCK();

	if (gc_buffer_size) {
		buffer_attributes = gc_buffer_attributes;
		buffer_characters = gc_buffer_characters;
		buffer_colorcodes = gc_buffer_colorcodes;
		buffer_tab_stops  = gc_buffer_tab_stops;
		buffer_columns    = gc_buffer_columns;
		buffer_rows       = gc_buffer_rows;
		buffer_size       = gc_buffer_size;

		gc_buffer_attributes = NULL;
		gc_buffer_characters = NULL;
		gc_buffer_colorcodes = NULL;
		gc_buffer_tab_stops  = NULL;
		gc_buffer_columns    = 0;
		gc_buffer_rows       = 0;
		gc_buffer_size       = 0;

		VCPUTC_LOCK_UNLOCK();

		kfree_data(buffer_attributes, buffer_size);
		kfree_data(buffer_characters, buffer_size);
		kfree_data(buffer_colorcodes, buffer_size);
		kfree_data(buffer_tab_stops, buffer_columns);
	} else {
		VCPUTC_LOCK_UNLOCK();
	}

	if (enable) {
		if (vm_initialized) {
			buffer_columns = vinfo.v_columns;
			buffer_rows    = vinfo.v_rows;
			buffer_size    = buffer_columns * buffer_rows;

			if (buffer_size) {
				buffer_attributes = kalloc_data(buffer_size, Z_WAITOK);
				buffer_characters = kalloc_data(buffer_size, Z_WAITOK);
				buffer_colorcodes = kalloc_data(buffer_size, Z_WAITOK);
				buffer_tab_stops  = kalloc_data(buffer_columns, Z_WAITOK);

				if (buffer_attributes == NULL ||
				    buffer_characters == NULL ||
				    buffer_colorcodes == NULL ||
				    buffer_tab_stops == NULL) {
					kfree_data(buffer_attributes, buffer_size);
					kfree_data(buffer_characters, buffer_size);
					kfree_data(buffer_colorcodes, buffer_size);
					kfree_data(buffer_tab_stops, buffer_columns);

					buffer_attributes = NULL;
					buffer_characters = NULL;
					buffer_colorcodes = NULL;
					buffer_tab_stops  = NULL;
					buffer_columns = 0;
					buffer_rows    = 0;
					buffer_size    = 0;
				} else {
					memset( buffer_attributes, ATTR_NONE, buffer_size );
					memset( buffer_characters, ' ', buffer_size );
					memset( buffer_colorcodes, COLOR_CODE_SET( 0, COLOR_FOREGROUND, TRUE ), buffer_size );
					memset( buffer_tab_stops, 0, buffer_columns );
				}
			}
		}

		VCPUTC_LOCK_LOCK();

		gc_buffer_attributes = buffer_attributes;
		gc_buffer_characters = buffer_characters;
		gc_buffer_colorcodes = buffer_colorcodes;
		gc_buffer_tab_stops  = buffer_tab_stops;
		gc_buffer_columns    = buffer_columns;
		gc_buffer_rows       = buffer_rows;
		gc_buffer_size       = buffer_size;

		gc_reset_screen();

		VCPUTC_LOCK_UNLOCK();

		gc_ops.clear_screen(gc_x, gc_y, 0, vinfo.v_rows, 2);
		gc_ops.show_cursor(gc_x, gc_y);

		gc_ops.enable(TRUE);
		gc_enabled           = TRUE;
		disableConsoleOutput = FALSE;
	}
}

static void
gc_hide_cursor(unsigned int xx, unsigned int yy)
{
	if (xx < gc_buffer_columns && yy < gc_buffer_rows) {
		uint32_t index = (yy * gc_buffer_columns) + xx;
		unsigned char attribute = gc_buffer_attributes[index];
		unsigned char character = gc_buffer_characters[index];
		unsigned char colorcode = gc_buffer_colorcodes[index];
		unsigned char colorcodesave = gc_color_code;

		gc_update_color(COLOR_CODE_GET(colorcode, TRUE ), TRUE );
		gc_update_color(COLOR_CODE_GET(colorcode, FALSE), FALSE);

		gc_ops.paint_char(xx, yy, character, attribute, 0, 0);

		gc_update_color(COLOR_CODE_GET(colorcodesave, TRUE ), TRUE );
		gc_update_color(COLOR_CODE_GET(colorcodesave, FALSE), FALSE);
	} else {
		gc_ops.hide_cursor(xx, yy);
	}
}

static void
gc_initialize(struct vc_info * info)
{
	if (gc_initialized == FALSE) {
		/* Init our lock */
		VCPUTC_LOCK_INIT();

		gc_initialized = TRUE;
	}

	gc_ops.initialize(info);

	gc_reset_vt100();
	gc_x = gc_y = 0;
}

static void
gc_paint_char(unsigned int xx, unsigned int yy, unsigned char ch, int attrs)
{
	if (xx < gc_buffer_columns && yy < gc_buffer_rows) {
		uint32_t index = (yy * gc_buffer_columns) + xx;

		gc_buffer_attributes[index] = attrs;
		gc_buffer_characters[index] = ch;
		gc_buffer_colorcodes[index] = gc_color_code;
	}

	gc_ops.paint_char(xx, yy, ch, attrs, 0, 0);
}

static void
gc_putchar(char ch)
{
	if (!ch) {
		return; /* ignore null characters */
	}
	switch (gc_vt100state) {
	default:
		gc_vt100state = ESnormal;
		OS_FALLTHROUGH;
	case ESnormal:
		gc_putc_normal(ch);
		break;
	case ESesc:
		gc_putc_esc(ch);
		break;
	case ESsquare:
		gc_putc_square(ch);
		break;
	case ESgetpars:
		gc_putc_getpars(ch);
		break;
	case ESgotpars:
		gc_putc_gotpars(ch);
		break;
	case ESask:
		gc_putc_askcmd(ch);
		break;
	case EScharsize:
		gc_putc_charsizecmd(ch);
		break;
	case ESsetG0:
		gc_putc_charsetcmd(0, ch);
		break;
	case ESsetG1:
		gc_putc_charsetcmd(1, ch);
		break;
	}

	if (gc_x >= vinfo.v_columns) {
		if (0 == vinfo.v_columns) {
			gc_x = 0;
		} else {
			gc_x = vinfo.v_columns - 1;
		}
	}
	if (gc_y >= vinfo.v_rows) {
		if (0 == vinfo.v_rows) {
			gc_y = 0;
		} else {
			gc_y = vinfo.v_rows - 1;
		}
	}
}

static void
gc_putc_askcmd(unsigned char ch)
{
	if (ch >= '0' && ch <= '9') {
		gc_par[gc_numpars] = (10 * gc_par[gc_numpars]) + (ch - '0');
		return;
	}
	gc_vt100state = ESnormal;

	switch (gc_par[0]) {
	case 6:
		gc_relative_origin = ch == 'h';
		break;
	case 7:         /* wrap around mode h=1, l=0*/
		gc_wrap_mode = ch == 'h';
		break;
	default:
		break;
	}
}

static void
gc_putc_charsetcmd(int charset, unsigned char ch)
{
	gc_vt100state = ESnormal;

	switch (ch) {
	case 'A':
	case 'B':
	default:
		gc_charset[charset] = 0;
		break;
	case '0':               /* Graphic characters */
	case '2':
		gc_charset[charset] = 0x21;
		break;
	}
}

static void
gc_putc_charsizecmd(unsigned char ch)
{
	gc_vt100state = ESnormal;

	switch (ch) {
	case '3':
	case '4':
	case '5':
	case '6':
		break;
	case '8':               /* fill 'E's */
	{
		unsigned int xx, yy;
		for (yy = 0; yy < vinfo.v_rows; yy++) {
			for (xx = 0; xx < vinfo.v_columns; xx++) {
				gc_paint_char(xx, yy, 'E', ATTR_NONE);
			}
		}
	}
	break;
	}
}

static void
gc_putc_esc(unsigned char ch)
{
	gc_vt100state = ESnormal;

	switch (ch) {
	case '[':
		gc_vt100state = ESsquare;
		break;
	case 'c':               /* Reset terminal        */
		gc_reset_vt100();
		gc_clear_screen(gc_x, gc_y, 0, vinfo.v_rows, 2);
		gc_x = gc_y = 0;
		break;
	case 'D':               /* Line feed		 */
	case 'E':
		if (gc_y >= gc_scrreg_bottom - 1) {
			gc_scroll_up(1, gc_scrreg_top, gc_scrreg_bottom);
			gc_y = gc_scrreg_bottom - 1;
		} else {
			gc_y++;
		}
		if (ch == 'E') {
			gc_x = 0;
		}
		break;
	case 'H':               /* Set tab stop		 */
		gc_set_tab_stop(gc_x, TRUE);
		break;
	case 'M':               /* Cursor up		 */
		if (gc_y <= gc_scrreg_top) {
			gc_scroll_down(1, gc_scrreg_top, gc_scrreg_bottom);
			gc_y = gc_scrreg_top;
		} else {
			gc_y--;
		}
		break;
	case '>':
		gc_reset_vt100();
		break;
	case '7':               /* Save cursor		 */
		gc_savex = gc_x;
		gc_savey = gc_y;
		gc_saveattr = gc_attr;
		gc_save_charset_s = gc_charset_select;
		gc_charset_save[0] = gc_charset[0];
		gc_charset_save[1] = gc_charset[1];
		break;
	case '8':               /* Restore cursor	 */
		gc_x = gc_savex;
		gc_y = gc_savey;
		gc_attr = gc_saveattr;
		gc_charset_select = gc_save_charset_s;
		gc_charset[0] = gc_charset_save[0];
		gc_charset[1] = gc_charset_save[1];
		break;
	case 'Z':               /* return terminal ID */
		break;
	case '#':               /* change characters height */
		gc_vt100state = EScharsize;
		break;
	case '(':
		gc_vt100state = ESsetG0;
		break;
	case ')':               /* character set sequence */
		gc_vt100state = ESsetG1;
		break;
	case '=':
		break;
	default:
		/* Rest not supported */
		break;
	}
}

static void
gc_putc_getpars(unsigned char ch)
{
	if (ch == '?') {
		gc_vt100state = ESask;
		return;
	}
	if (ch == '[') {
		gc_vt100state = ESnormal;
		/* Not supported */
		return;
	}
	if (ch == ';' && gc_numpars < MAXPARS - 1) {
		gc_numpars++;
	} else if (ch >= '0' && ch <= '9') {
		gc_par[gc_numpars] *= 10;
		gc_par[gc_numpars] += ch - '0';
	} else {
		gc_numpars++;
		gc_vt100state = ESgotpars;
		gc_putc_gotpars(ch);
	}
}

static void
gc_putc_gotpars(unsigned char ch)
{
	unsigned int i;

	if (ch < ' ') {
		/* special case for vttest for handling cursor
		 *  movement in escape sequences */
		gc_putc_normal(ch);
		gc_vt100state = ESgotpars;
		return;
	}
	gc_vt100state = ESnormal;
	switch (ch) {
	case 'A':               /* Up			 */
		gc_y -= gc_par[0] ? gc_par[0] : 1;
		if (gc_y < gc_scrreg_top) {
			gc_y = gc_scrreg_top;
		}
		break;
	case 'B':               /* Down			 */
		gc_y += gc_par[0] ? gc_par[0] : 1;
		if (gc_y >= gc_scrreg_bottom) {
			gc_y = gc_scrreg_bottom - 1;
		}
		break;
	case 'C':               /* Right		 */
		gc_x += gc_par[0] ? gc_par[0] : 1;
		if (gc_x >= vinfo.v_columns) {
			gc_x = vinfo.v_columns - 1;
		}
		break;
	case 'D':               /* Left			 */
		if (gc_par[0] > gc_x) {
			gc_x = 0;
		} else if (gc_par[0]) {
			gc_x -= gc_par[0];
		} else if (gc_x) {
			--gc_x;
		}
		break;
	case 'H':               /* Set cursor position	 */
	case 'f':
		gc_x = gc_par[1] ? gc_par[1] - 1 : 0;
		gc_y = gc_par[0] ? gc_par[0] - 1 : 0;
		if (gc_relative_origin) {
			gc_y += gc_scrreg_top;
		}
		gc_hanging_cursor = 0;
		break;
	case 'X':               /* clear p1 characters */
		if (gc_numpars) {
			for (i = gc_x; i < gc_x + gc_par[0]; i++) {
				gc_paint_char(i, gc_y, ' ', ATTR_NONE);
			}
		}
		break;
	case 'J':               /* Clear part of screen	 */
		gc_clear_screen(gc_x, gc_y, 0, vinfo.v_rows, gc_par[0]);
		break;
	case 'K':               /* Clear part of line	 */
		gc_clear_line(gc_x, gc_y, gc_par[0]);
		break;
	case 'g':               /* tab stops	         */
		switch (gc_par[0]) {
		case 1:
		case 2:         /* reset tab stops */
			        /* gc_reset_tabs(); */
			break;
		case 3:         /* Clear every tabs */
		{
			for (i = 0; i <= vinfo.v_columns; i++) {
				gc_set_tab_stop(i, FALSE);
			}
		}
		break;
		case 0:
			gc_set_tab_stop(gc_x, FALSE);
			break;
		}
		break;
	case 'm':               /* Set attribute	 */
		for (i = 0; i < gc_numpars; i++) {
			switch (gc_par[i]) {
			case 0:
				gc_attr = ATTR_NONE;
				gc_update_color(COLOR_BACKGROUND, FALSE);
				gc_update_color(COLOR_FOREGROUND, TRUE );
				break;
			case 1:
				gc_attr |= ATTR_BOLD;
				break;
			case 4:
				gc_attr |= ATTR_UNDER;
				break;
			case 7:
				gc_attr |= ATTR_REVERSE;
				break;
			case 22:
				gc_attr &= ~ATTR_BOLD;
				break;
			case 24:
				gc_attr &= ~ATTR_UNDER;
				break;
			case 27:
				gc_attr &= ~ATTR_REVERSE;
				break;
			case 5:
			case 25:        /* blink/no blink */
				break;
			default:
				if (gc_par[i] >= 30 && gc_par[i] <= 37) {
					gc_update_color(gc_par[i] - 30, TRUE);
				}
				if (gc_par[i] >= 40 && gc_par[i] <= 47) {
					gc_update_color(gc_par[i] - 40, FALSE);
				}
				break;
			}
		}
		break;
	case 'r':               /* Set scroll region	 */
		gc_x = gc_y = 0;
		/* ensure top < bottom, and both within limits */
		if ((gc_numpars > 0) && (gc_par[0] < vinfo.v_rows)) {
			gc_scrreg_top = gc_par[0] ? gc_par[0] - 1 : 0;
		} else {
			gc_scrreg_top = 0;
		}
		if ((gc_numpars > 1) && (gc_par[1] <= vinfo.v_rows) && (gc_par[1] > gc_par[0])) {
			gc_scrreg_bottom = gc_par[1];
			if (gc_scrreg_bottom > vinfo.v_rows) {
				gc_scrreg_bottom = vinfo.v_rows;
			}
		} else {
			gc_scrreg_bottom = vinfo.v_rows;
		}
		if (gc_relative_origin) {
			gc_y = gc_scrreg_top;
		}
		break;
	}
}

static void
gc_putc_normal(unsigned char ch)
{
	switch (ch) {
	case '\a':              /* Beep			 */
		break;
	case 127:               /* Delete		 */
	case '\b':              /* Backspace		 */
		if (gc_hanging_cursor) {
			gc_hanging_cursor = 0;
		} else if (gc_x > 0) {
			gc_x--;
		}
		break;
	case '\t':              /* Tab			 */
		if (gc_buffer_tab_stops) {
			while (gc_x < vinfo.v_columns && !gc_is_tab_stop(++gc_x)) {
				;
			}
		}

		if (gc_x >= vinfo.v_columns) {
			gc_x = vinfo.v_columns - 1;
		}
		break;
	case 0x0b:
	case 0x0c:
	case '\n':              /* Line feed		 */
		if (gc_y >= gc_scrreg_bottom - 1) {
			gc_scroll_up(1, gc_scrreg_top, gc_scrreg_bottom);
			gc_y = gc_scrreg_bottom - 1;
		} else {
			gc_y++;
		}
		break;
	case '\r':              /* Carriage return	 */
		gc_x = 0;
		gc_hanging_cursor = 0;
		break;
	case 0x0e:  /* Select G1 charset (Control-N) */
		gc_charset_select = 1;
		break;
	case 0x0f:  /* Select G0 charset (Control-O) */
		gc_charset_select = 0;
		break;
	case 0x18:  /* CAN : cancel */
	case 0x1A:  /* like cancel */
		/* well, i do nothing here, may be later */
		break;
	case '\033':            /* Escape		 */
		gc_vt100state = ESesc;
		gc_hanging_cursor = 0;
		break;
	default:
		if (ch >= ' ') {
			if (gc_hanging_cursor) {
				gc_x = 0;
				if (gc_y >= gc_scrreg_bottom - 1) {
					gc_scroll_up(1, gc_scrreg_top, gc_scrreg_bottom);
					gc_y = gc_scrreg_bottom - 1;
				} else {
					gc_y++;
				}
				gc_hanging_cursor = 0;
			}
			gc_paint_char(gc_x, gc_y, (ch >= 0x60 && ch <= 0x7f) ? ch + gc_charset[gc_charset_select]
			    : ch, gc_attr);
			if (gc_x == vinfo.v_columns - 1) {
				gc_hanging_cursor = gc_wrap_mode;
			} else {
				gc_x++;
			}
		}
		break;
	}
}

static void
gc_putc_square(unsigned char ch)
{
	int     i;

	for (i = 0; i < MAXPARS; i++) {
		gc_par[i] = 0;
	}

	gc_numpars = 0;
	gc_vt100state = ESgetpars;

	gc_putc_getpars(ch);
}

static void
gc_reset_screen(void)
{
	gc_reset_vt100();
	gc_x = gc_y = 0;
}

static void
gc_reset_tabs(void)
{
	unsigned int i;

	if (!gc_buffer_tab_stops) {
		return;
	}

	for (i = 0; i < vinfo.v_columns; i++) {
		gc_buffer_tab_stops[i] = ((i % 8) == 0);
	}
}

static void
gc_set_tab_stop(unsigned int column, boolean_t enabled)
{
	if (gc_buffer_tab_stops && (column < vinfo.v_columns)) {
		gc_buffer_tab_stops[column] = enabled;
	}
}

static boolean_t
gc_is_tab_stop(unsigned int column)
{
	if (gc_buffer_tab_stops == NULL) {
		return (column % 8) == 0;
	}
	if (column < vinfo.v_columns) {
		return gc_buffer_tab_stops[column];
	} else {
		return FALSE;
	}
}

static void
gc_reset_vt100(void)
{
	gc_reset_tabs();
	gc_scrreg_top    = 0;
	gc_scrreg_bottom = vinfo.v_rows;
	gc_attr = ATTR_NONE;
	gc_charset[0] = gc_charset[1] = 0;
	gc_charset_select = 0;
	gc_wrap_mode = 1;
	gc_relative_origin = 0;
	gc_update_color(COLOR_BACKGROUND, FALSE);
	gc_update_color(COLOR_FOREGROUND, TRUE);
}

static void
gc_scroll_down(int num, unsigned int top, unsigned int bottom)
{
	if (!gc_buffer_size) {
		return;
	}

	if (bottom <= gc_buffer_rows) {
		unsigned char colorcodesave = gc_color_code;
		uint32_t column, row;
		uint32_t index, jump;

		jump = num * gc_buffer_columns;

		for (row = bottom - 1; row >= top + num; row--) {
			index = row * gc_buffer_columns;

			for (column = 0; column < gc_buffer_columns; index++, column++) {
				if (gc_buffer_attributes[index] != gc_buffer_attributes[index - jump] ||
				    gc_buffer_characters[index] != gc_buffer_characters[index - jump] ||
				    gc_buffer_colorcodes[index] != gc_buffer_colorcodes[index - jump]) {
					if (gc_color_code != gc_buffer_colorcodes[index - jump]) {
						gc_update_color(COLOR_CODE_GET(gc_buffer_colorcodes[index - jump], TRUE ), TRUE );
						gc_update_color(COLOR_CODE_GET(gc_buffer_colorcodes[index - jump], FALSE), FALSE);
					}

					if (gc_buffer_colorcodes[index] != gc_buffer_colorcodes[index - jump]) {
						gc_ops.paint_char( /* xx             */ column,
						    /* yy             */ row,
						    /* ch             */ gc_buffer_characters[index - jump],
						    /* attrs          */ gc_buffer_attributes[index - jump],
						    /* ch_previous    */ 0,
						    /* attrs_previous */ 0 );
					} else {
						gc_ops.paint_char( /* xx             */ column,
						    /* yy             */ row,
						    /* ch             */ gc_buffer_characters[index - jump],
						    /* attrs          */ gc_buffer_attributes[index - jump],
						    /* ch_previous    */ gc_buffer_characters[index],
						    /* attrs_previous */ gc_buffer_attributes[index] );
					}

					gc_buffer_attributes[index] = gc_buffer_attributes[index - jump];
					gc_buffer_characters[index] = gc_buffer_characters[index - jump];
					gc_buffer_colorcodes[index] = gc_buffer_colorcodes[index - jump];
				}
			}
		}

		if (colorcodesave != gc_color_code) {
			gc_update_color(COLOR_CODE_GET(colorcodesave, TRUE ), TRUE );
			gc_update_color(COLOR_CODE_GET(colorcodesave, FALSE), FALSE);
		}

		/* Now set the freed up lines to the background colour */

		for (row = top; row < top + num; row++) {
			index = row * gc_buffer_columns;

			for (column = 0; column < gc_buffer_columns; index++, column++) {
				if (gc_buffer_attributes[index] != ATTR_NONE ||
				    gc_buffer_characters[index] != ' ' ||
				    gc_buffer_colorcodes[index] != gc_color_code) {
					if (gc_buffer_colorcodes[index] != gc_color_code) {
						gc_ops.paint_char( /* xx             */ column,
						    /* yy             */ row,
						    /* ch             */ ' ',
						    /* attrs          */ ATTR_NONE,
						    /* ch_previous    */ 0,
						    /* attrs_previous */ 0 );
					} else {
						gc_ops.paint_char( /* xx             */ column,
						    /* yy             */ row,
						    /* ch             */ ' ',
						    /* attrs          */ ATTR_NONE,
						    /* ch_previous    */ gc_buffer_characters[index],
						    /* attrs_previous */ gc_buffer_attributes[index] );
					}

					gc_buffer_attributes[index] = ATTR_NONE;
					gc_buffer_characters[index] = ' ';
					gc_buffer_colorcodes[index] = gc_color_code;
				}
			}
		}
	} else {
		gc_ops.scroll_down(num, top, bottom);

		/* Now set the freed up lines to the background colour */

		gc_clear_screen(vinfo.v_columns - 1, top + num - 1, top, bottom, 1);
	}
}

static void
gc_scroll_up(int num, unsigned int top, unsigned int bottom)
{
	if (!gc_buffer_size) {
		return;
	}

	if (bottom <= gc_buffer_rows) {
		unsigned char colorcodesave = gc_color_code;
		uint32_t column, row;
		uint32_t index, jump;

		jump = num * gc_buffer_columns;

		for (row = top; row < bottom - num; row++) {
			index = row * gc_buffer_columns;

			for (column = 0; column < gc_buffer_columns; index++, column++) {
				if (gc_buffer_attributes[index] != gc_buffer_attributes[index + jump] ||
				    gc_buffer_characters[index] != gc_buffer_characters[index + jump] ||
				    gc_buffer_colorcodes[index] != gc_buffer_colorcodes[index + jump]) {
					if (gc_color_code != gc_buffer_colorcodes[index + jump]) {
						gc_update_color(COLOR_CODE_GET(gc_buffer_colorcodes[index + jump], TRUE ), TRUE );
						gc_update_color(COLOR_CODE_GET(gc_buffer_colorcodes[index + jump], FALSE), FALSE);
					}

					if (gc_buffer_colorcodes[index] != gc_buffer_colorcodes[index + jump]) {
						gc_ops.paint_char( /* xx             */ column,
						    /* yy             */ row,
						    /* ch             */ gc_buffer_characters[index + jump],
						    /* attrs          */ gc_buffer_attributes[index + jump],
						    /* ch_previous    */ 0,
						    /* attrs_previous */ 0 );
					} else {
						gc_ops.paint_char( /* xx             */ column,
						    /* yy             */ row,
						    /* ch             */ gc_buffer_characters[index + jump],
						    /* attrs          */ gc_buffer_attributes[index + jump],
						    /* ch_previous    */ gc_buffer_characters[index],
						    /* attrs_previous */ gc_buffer_attributes[index] );
					}

					gc_buffer_attributes[index] = gc_buffer_attributes[index + jump];
					gc_buffer_characters[index] = gc_buffer_characters[index + jump];
					gc_buffer_colorcodes[index] = gc_buffer_colorcodes[index + jump];
				}
			}
		}

		if (colorcodesave != gc_color_code) {
			gc_update_color(COLOR_CODE_GET(colorcodesave, TRUE ), TRUE );
			gc_update_color(COLOR_CODE_GET(colorcodesave, FALSE), FALSE);
		}

		/* Now set the freed up lines to the background colour */

		for (row = bottom - num; row < bottom; row++) {
			index = row * gc_buffer_columns;

			for (column = 0; column < gc_buffer_columns; index++, column++) {
				if (gc_buffer_attributes[index] != ATTR_NONE ||
				    gc_buffer_characters[index] != ' ' ||
				    gc_buffer_colorcodes[index] != gc_color_code) {
					if (gc_buffer_colorcodes[index] != gc_color_code) {
						gc_ops.paint_char( /* xx             */ column,
						    /* yy             */ row,
						    /* ch             */ ' ',
						    /* attrs          */ ATTR_NONE,
						    /* ch_previous    */ 0,
						    /* attrs_previous */ 0 );
					} else {
						gc_ops.paint_char( /* xx             */ column,
						    /* yy             */ row,
						    /* ch             */ ' ',
						    /* attrs          */ ATTR_NONE,
						    /* ch_previous    */ gc_buffer_characters[index],
						    /* attrs_previous */ gc_buffer_attributes[index] );
					}

					gc_buffer_attributes[index] = ATTR_NONE;
					gc_buffer_characters[index] = ' ';
					gc_buffer_colorcodes[index] = gc_color_code;
				}
			}
		}
	} else {
		gc_ops.scroll_up(num, top, bottom);

		/* Now set the freed up lines to the background colour */

		gc_clear_screen(0, bottom - num, top, bottom, 0);
	}
}

static void
gc_show_cursor(unsigned int xx, unsigned int yy)
{
	if (xx < gc_buffer_columns && yy < gc_buffer_rows) {
		uint32_t index = (yy * gc_buffer_columns) + xx;
		unsigned char attribute = gc_buffer_attributes[index];
		unsigned char character = gc_buffer_characters[index];
		unsigned char colorcode = gc_buffer_colorcodes[index];
		unsigned char colorcodesave = gc_color_code;

		gc_update_color(COLOR_CODE_GET(colorcode, FALSE), TRUE );
		gc_update_color(COLOR_CODE_GET(colorcode, TRUE ), FALSE);

		gc_ops.paint_char(xx, yy, character, attribute, 0, 0);

		gc_update_color(COLOR_CODE_GET(colorcodesave, TRUE ), TRUE );
		gc_update_color(COLOR_CODE_GET(colorcodesave, FALSE), FALSE);
	} else {
		gc_ops.show_cursor(xx, yy);
	}
}

static void
gc_update_color(int color, boolean_t fore)
{
	assert(gc_ops.update_color);

	gc_color_code = COLOR_CODE_SET(gc_color_code, color, fore);
	gc_ops.update_color(color, fore);
}

void
vcputc_options(char c, __unused bool poll)
{
	if (gc_initialized && gc_enabled) {
		VCPUTC_LOCK_LOCK();
		if (gc_enabled) {
			gc_hide_cursor(gc_x, gc_y);
			gc_putchar(c);
			gc_show_cursor(gc_x, gc_y);
		}
#if SCHED_HYGIENE_DEBUG
		abandon_preemption_disable_measurement();
#endif /* SCHED_HYGIENE_DEBUG */
		VCPUTC_LOCK_UNLOCK();
	}
}

void
vcputc(char c)
{
	vcputc_options(c, false);
}

/*
 * Video Console (Back-End)
 * ------------------------
 */

/*
 * For the color support (Michel Pollet)
 */
static unsigned char vc_color_index_table[33] =
{  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 2 };

static uint32_t vc_colors[8][4] = {
	{ 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 },     /* black */
	{ 0x23232323, 0x7C007C00, 0x00FF0000, 0x3FF00000 },     /* red	*/
	{ 0xb9b9b9b9, 0x03e003e0, 0x0000FF00, 0x000FFC00 },     /* green */
	{ 0x05050505, 0x7FE07FE0, 0x00FFFF00, 0x3FFFFC00 },     /* yellow */
	{ 0xd2d2d2d2, 0x001f001f, 0x000000FF, 0x000003FF },     /* blue	 */
//	{ 0x80808080, 0x31933193, 0x00666699, 0x00000000 },	/* blue	 */
	{ 0x18181818, 0x7C1F7C1F, 0x00FF00FF, 0x3FF003FF },     /* magenta */
	{ 0xb4b4b4b4, 0x03FF03FF, 0x0000FFFF, 0x000FFFFF },     /* cyan	*/
	{ 0x00000000, 0x7FFF7FFF, 0x00FFFFFF, 0x3FFFFFFF }      /* white */
};

static uint32_t vc_color_fore = 0;
static uint32_t vc_color_back = 0;

/*
 * New Rendering code from Michel Pollet
 */

/* Rendered Font Buffer */
static unsigned char *vc_rendered_font = NULL;

/* Rendered Font Size */
static uint32_t vc_rendered_font_size = 0;

/* Size of a character in the table (bytes) */
static int vc_rendered_char_size = 0;

#define REN_MAX_DEPTH   32
static unsigned char vc_rendered_char[ISO_CHAR_HEIGHT * ((REN_MAX_DEPTH / 8) * ISO_CHAR_WIDTH)];

#if defined(XNU_TARGET_OS_OSX)
#define CONFIG_VC_PROGRESS_METER_SUPPORT        1
#endif /* XNU_TARGET_OS_OSX */

#if defined(XNU_TARGET_OS_OSX)
static void
internal_set_progressmeter(int new_value);
static void
internal_enable_progressmeter(int new_value);

enum{
	kProgressMeterOff    = FALSE,
	kProgressMeterUser   = TRUE,
	kProgressMeterKernel = 3,
};
enum{
	kProgressMeterMax    = 1024,
	kProgressMeterEnd    = 512,
};

#endif  /* defined(XNU_TARGET_OS_OSX) */

static boolean_t vc_progress_white =
#ifdef CONFIG_VC_PROGRESS_WHITE
    TRUE;
#else /* !CONFIG_VC_PROGRESS_WHITE */
    FALSE;
#endif /* !CONFIG_VC_PROGRESS_WHITE */

static int vc_acquire_delay = kProgressAcquireDelay;

static void
vc_clear_screen(unsigned int xx, unsigned int yy, unsigned int scrreg_top,
    unsigned int scrreg_bottom, int which)
{
	uint32_t *p, *endp, *row;
	int      linelongs, col;
	int      rowline, rowlongs;

	if (!vinfo.v_depth) {
		return;
	}

	linelongs = vinfo.v_rowbytes * (ISO_CHAR_HEIGHT >> 2);
	rowline = vinfo.v_rowscanbytes >> 2;
	rowlongs = vinfo.v_rowbytes >> 2;

	p = (uint32_t*) vinfo.v_baseaddr;
	endp = (uint32_t*) vinfo.v_baseaddr;

	switch (which) {
	case 0:         /* To end of screen	 */
		gc_clear_line(xx, yy, 0);
		if (yy < scrreg_bottom - 1) {
			p += (yy + 1) * linelongs;
			endp += scrreg_bottom * linelongs;
		}
		break;
	case 1:         /* To start of screen	 */
		gc_clear_line(xx, yy, 1);
		if (yy > scrreg_top) {
			p += scrreg_top * linelongs;
			endp += yy * linelongs;
		}
		break;
	case 2:         /* Whole screen		 */
		p += scrreg_top * linelongs;
		if (scrreg_bottom == vinfo.v_rows) {
			endp += rowlongs * vinfo.v_height;
		} else {
			endp += scrreg_bottom * linelongs;
		}
		break;
	}

	for (row = p; row < endp; row += rowlongs) {
		for (col = 0; col < rowline; col++) {
			*(row + col) = vc_color_back;
		}
	}
}

static void
vc_render_char(unsigned char ch, unsigned char *renderptr, short newdepth)
{
	union {
		unsigned char  *charptr;
		unsigned short *shortptr;
		uint32_t  *longptr;
	} current;      /* current place in rendered font, multiple types. */
	unsigned char *theChar; /* current char in iso_font */
	int line;

	current.charptr = renderptr;
	theChar = iso_font + (ch * ISO_CHAR_HEIGHT);

	for (line = 0; line < ISO_CHAR_HEIGHT; line++) {
		unsigned char mask = 1;
		do {
			switch (newdepth) {
			case 8:
				*current.charptr++ = (*theChar & mask) ? 0xFF : 0;
				break;
			case 16:
				*current.shortptr++ = (*theChar & mask) ? 0xFFFF : 0;
				break;

			case 30:
			case 32:
				*current.longptr++ = (*theChar & mask) ? 0xFFFFFFFF : 0;
				break;
			}
			mask <<= 1;
		} while (mask); /* while the single bit drops to the right */
		theChar++;
	}
}

static void
vc_paint_char_8(unsigned int xx, unsigned int yy, unsigned char ch, int attrs,
    __unused unsigned char ch_previous, __unused int attrs_previous)
{
	uint32_t *theChar;
	uint32_t *where;
	int i;

	if (vc_rendered_font) {
		theChar = (uint32_t*)(vc_rendered_font + (ch * vc_rendered_char_size));
	} else {
		vc_render_char(ch, vc_rendered_char, 8);
		theChar = (uint32_t*)(vc_rendered_char);
	}
	where = (uint32_t*)(vinfo.v_baseaddr +
	    (yy * ISO_CHAR_HEIGHT * vinfo.v_rowbytes) +
	    (xx * ISO_CHAR_WIDTH));

	if (!attrs) {
		for (i = 0; i < ISO_CHAR_HEIGHT; i++) {         /* No attr? FLY !*/
			uint32_t *store = where;
			int x;
			for (x = 0; x < 2; x++) {
				uint32_t val = *theChar++;
				val = (vc_color_back & ~val) | (vc_color_fore & val);
				*store++ = val;
			}

			where = (uint32_t*)(((unsigned char*)where) + vinfo.v_rowbytes);
		}
	} else {
		for (i = 0; i < ISO_CHAR_HEIGHT; i++) { /* a little slower */
			uint32_t *store = where, lastpixel = 0;
			int x;
			for (x = 0; x < 2; x++) {
				uint32_t val = *theChar++, save = val;
				if (attrs & ATTR_BOLD) { /* bold support */
					if (lastpixel && !(save & 0xFF000000)) {
						val |= 0xff000000;
					}
					if ((save & 0xFFFF0000) == 0xFF000000) {
						val |= 0x00FF0000;
					}
					if ((save & 0x00FFFF00) == 0x00FF0000) {
						val |= 0x0000FF00;
					}
					if ((save & 0x0000FFFF) == 0x0000FF00) {
						val |= 0x000000FF;
					}
				}
				if (attrs & ATTR_REVERSE) {
					val = ~val;
				}
				if (attrs & ATTR_UNDER && i == ISO_CHAR_HEIGHT - 1) {
					val = ~val;
				}

				val = (vc_color_back & ~val) | (vc_color_fore & val);
				*store++ = val;
				lastpixel = save & 0xff;
			}

			where = (uint32_t*)(((unsigned char*)where) + vinfo.v_rowbytes);
		}
	}
}

static void
vc_paint_char_16(unsigned int xx, unsigned int yy, unsigned char ch, int attrs,
    __unused unsigned char ch_previous,
    __unused int attrs_previous)
{
	uint32_t *theChar;
	uint32_t *where;
	int i;

	if (vc_rendered_font) {
		theChar = (uint32_t*)(vc_rendered_font + (ch * vc_rendered_char_size));
	} else {
		vc_render_char(ch, vc_rendered_char, 16);
		theChar = (uint32_t*)(vc_rendered_char);
	}
	where = (uint32_t*)(vinfo.v_baseaddr +
	    (yy * ISO_CHAR_HEIGHT * vinfo.v_rowbytes) +
	    (xx * ISO_CHAR_WIDTH * 2));

	if (!attrs) {
		for (i = 0; i < ISO_CHAR_HEIGHT; i++) {         /* No attrs ? FLY ! */
			uint32_t *store = where;
			int x;
			for (x = 0; x < 4; x++) {
				uint32_t val = *theChar++;
				val = (vc_color_back & ~val) | (vc_color_fore & val);
				*store++ = val;
			}

			where = (uint32_t*)(((unsigned char*)where) + vinfo.v_rowbytes);
		}
	} else {
		for (i = 0; i < ISO_CHAR_HEIGHT; i++) { /* a little bit slower */
			uint32_t *store = where, lastpixel = 0;
			int x;
			for (x = 0; x < 4; x++) {
				uint32_t val = *theChar++, save = val;
				if (attrs & ATTR_BOLD) { /* bold support */
					if (save == 0xFFFF0000) {
						val |= 0xFFFF;
					} else if (lastpixel && !(save & 0xFFFF0000)) {
						val |= 0xFFFF0000;
					}
				}
				if (attrs & ATTR_REVERSE) {
					val = ~val;
				}
				if (attrs & ATTR_UNDER && i == ISO_CHAR_HEIGHT - 1) {
					val = ~val;
				}

				val = (vc_color_back & ~val) | (vc_color_fore & val);

				*store++ = val;
				lastpixel = save & 0x7fff;
			}

			where = (uint32_t*)(((unsigned char*)where) + vinfo.v_rowbytes);
		}
	}
}

static void
vc_paint_char_32(unsigned int xx, unsigned int yy, unsigned char ch, int attrs,
    unsigned char ch_previous, int attrs_previous)
{
	uint32_t *theChar;
	uint32_t *theCharPrevious;
	uint32_t *where;
	int i;

	if (vc_rendered_font) {
		theChar = (uint32_t*)(vc_rendered_font + (ch * vc_rendered_char_size));
		theCharPrevious = (uint32_t*)(vc_rendered_font + (ch_previous * vc_rendered_char_size));
	} else {
		vc_render_char(ch, vc_rendered_char, 32);
		theChar = (uint32_t*)(vc_rendered_char);
		theCharPrevious = NULL;
	}
	if (!ch_previous) {
		theCharPrevious = NULL;
	}
	if (attrs_previous) {
		theCharPrevious = NULL;
	}
	where = (uint32_t*)(vinfo.v_baseaddr +
	    (yy * ISO_CHAR_HEIGHT * vinfo.v_rowbytes) +
	    (xx * ISO_CHAR_WIDTH * 4));

	if (!attrs) {
		for (i = 0; i < ISO_CHAR_HEIGHT; i++) {         /* No attrs ? FLY ! */
			uint32_t *store = where;
			int x;
			for (x = 0; x < 8; x++) {
				uint32_t val = *theChar++;
				if (theCharPrevious == NULL || val != *theCharPrevious++) {
					val = (vc_color_back & ~val) | (vc_color_fore & val);
					*store++ = val;
				} else {
					store++;
				}
			}

			where = (uint32_t *)(((unsigned char*)where) + vinfo.v_rowbytes);
		}
	} else {
		for (i = 0; i < ISO_CHAR_HEIGHT; i++) { /* a little slower */
			uint32_t *store = where, lastpixel = 0;
			int x;
			for (x = 0; x < 8; x++) {
				uint32_t val = *theChar++, save = val;
				if (attrs & ATTR_BOLD) { /* bold support */
					if (lastpixel && !save) {
						val = 0xFFFFFFFF;
					}
				}
				if (attrs & ATTR_REVERSE) {
					val = ~val;
				}
				if (attrs & ATTR_UNDER && i == ISO_CHAR_HEIGHT - 1) {
					val = ~val;
				}

				val = (vc_color_back & ~val) | (vc_color_fore & val);
				*store++ = val;
				lastpixel = save;
			}

			where = (uint32_t*)(((unsigned char*)where) + vinfo.v_rowbytes);
		}
	}
}

static void
vc_paint_char(unsigned int xx, unsigned int yy, unsigned char ch, int attrs,
    unsigned char ch_previous, int attrs_previous)
{
	if (!vinfo.v_depth) {
		return;
	}

	switch (vinfo.v_depth) {
	case 8:
		vc_paint_char_8(xx, yy, ch, attrs, ch_previous, attrs_previous);
		break;
	case 16:
		vc_paint_char_16(xx, yy, ch, attrs, ch_previous,
		    attrs_previous);
		break;
	case 30:
	case 32:
		vc_paint_char_32(xx, yy, ch, attrs, ch_previous,
		    attrs_previous);
		break;
	}
}

static void
vc_render_font(short newdepth)
{
	static short olddepth = 0;

	int charindex;  /* index in ISO font */
	unsigned char *rendered_font;
	unsigned int rendered_font_size;
	int rendered_char_size;

	if (vm_initialized == FALSE) {
		return; /* nothing to do */
	}
	if (olddepth == newdepth && vc_rendered_font) {
		return; /* nothing to do */
	}

	VCPUTC_LOCK_LOCK();

	rendered_font      = vc_rendered_font;
	rendered_font_size = vc_rendered_font_size;
	rendered_char_size = vc_rendered_char_size;

	vc_rendered_font      = NULL;
	vc_rendered_font_size = 0;
	vc_rendered_char_size = 0;

	VCPUTC_LOCK_UNLOCK();

	kfree_data(rendered_font, rendered_font_size);

	if (newdepth) {
		rendered_char_size = ISO_CHAR_HEIGHT * (((newdepth + 7) / 8) * ISO_CHAR_WIDTH);
		rendered_font_size = (ISO_CHAR_MAX - ISO_CHAR_MIN + 1) * rendered_char_size;
		rendered_font = kalloc_data(rendered_font_size, Z_WAITOK);
	}

	if (rendered_font == NULL) {
		return;
	}

	for (charindex = ISO_CHAR_MIN; charindex <= ISO_CHAR_MAX; charindex++) {
		vc_render_char(charindex, rendered_font + (charindex * rendered_char_size), newdepth);
	}

	olddepth = newdepth;

	VCPUTC_LOCK_LOCK();

	vc_rendered_font      = rendered_font;
	vc_rendered_font_size = rendered_font_size;
	vc_rendered_char_size = rendered_char_size;

	VCPUTC_LOCK_UNLOCK();
}

static void
vc_enable(boolean_t enable)
{
	vc_render_font(enable ? vinfo.v_depth : 0);
}

static void
vc_reverse_cursor(unsigned int xx, unsigned int yy)
{
	uint32_t *where;
	int line, col;

	if (!vinfo.v_depth) {
		return;
	}

	where = (uint32_t*)(vinfo.v_baseaddr +
	    (yy * ISO_CHAR_HEIGHT * vinfo.v_rowbytes) +
	    (xx /** ISO_CHAR_WIDTH*/ * vinfo.v_depth));
	for (line = 0; line < ISO_CHAR_HEIGHT; line++) {
		switch (vinfo.v_depth) {
		case 8:
			where[0] = ~where[0];
			where[1] = ~where[1];
			break;
		case 16:
			for (col = 0; col < 4; col++) {
				where[col] = ~where[col];
			}
			break;
		case 32:
			for (col = 0; col < 8; col++) {
				where[col] = ~where[col];
			}
			break;
		}
		where = (uint32_t*)(((unsigned char*)where) + vinfo.v_rowbytes);
	}
}

static void
vc_scroll_down(int num, unsigned int scrreg_top, unsigned int scrreg_bottom)
{
	uint32_t *from, *to, linelongs, i, line, rowline, rowscanline;

	if (!vinfo.v_depth) {
		return;
	}

	linelongs = vinfo.v_rowbytes * (ISO_CHAR_HEIGHT >> 2);
	rowline = vinfo.v_rowbytes >> 2;
	rowscanline = vinfo.v_rowscanbytes >> 2;

	to = (uint32_t *) vinfo.v_baseaddr + (linelongs * scrreg_bottom)
	    - (rowline - rowscanline);
	from = to - (linelongs * num);  /* handle multiple line scroll (Michel Pollet) */

	i = (scrreg_bottom - scrreg_top) - num;

	while (i-- > 0) {
		for (line = 0; line < ISO_CHAR_HEIGHT; line++) {
			/*
			 * Only copy what is displayed
			 */
			video_scroll_down(from,
			    (from - (vinfo.v_rowscanbytes >> 2)),
			    to);

			from -= rowline;
			to -= rowline;
		}
	}
}

static void
vc_scroll_up(int num, unsigned int scrreg_top, unsigned int scrreg_bottom)
{
	uint32_t *from, *to, linelongs, i, line, rowline, rowscanline;

	if (!vinfo.v_depth) {
		return;
	}

	linelongs = vinfo.v_rowbytes * (ISO_CHAR_HEIGHT >> 2);
	rowline = vinfo.v_rowbytes >> 2;
	rowscanline = vinfo.v_rowscanbytes >> 2;

	to = (uint32_t *) vinfo.v_baseaddr + (scrreg_top * linelongs);
	from = to + (linelongs * num);  /* handle multiple line scroll (Michel Pollet) */

	i = (scrreg_bottom - scrreg_top) - num;

	while (i-- > 0) {
		for (line = 0; line < ISO_CHAR_HEIGHT; line++) {
			/*
			 * Only copy what is displayed
			 */
			video_scroll_up(from,
			    (from + (vinfo.v_rowscanbytes >> 2)),
			    to);

			from += rowline;
			to += rowline;
		}
	}
}

static void
vc_update_color(int color, boolean_t fore)
{
	if (!vinfo.v_depth) {
		return;
	}
	if (fore) {
		vc_color_fore = vc_colors[color][vc_color_index_table[vinfo.v_depth]];
	} else {
		vc_color_back = vc_colors[color][vc_color_index_table[vinfo.v_depth]];
	}
}

/*
 * Video Console (Back-End): Icon Control
 * --------------------------------------
 */

static vc_progress_element *    vc_progress;
enum { kMaxProgressData = 3 };
static const unsigned char *    vc_progress_data[kMaxProgressData];
static const unsigned char *    vc_progress_alpha;
static boolean_t                vc_progress_enable;
static const unsigned char *    vc_clut;
static const unsigned char *    vc_clut8;
static unsigned char            vc_revclut8[256];
static uint32_t                 vc_progress_interval;
static uint32_t                 vc_progress_count;
static uint32_t                 vc_progress_angle;
static uint64_t                 vc_progress_deadline;
static thread_call_data_t       vc_progress_call;
static boolean_t                vc_needsave;
static void *                   vc_saveunder;
static vm_size_t                vc_saveunder_len;
static int8_t                   vc_uiscale = 1;
vc_progress_user_options        vc_progress_options;
vc_progress_user_options        vc_user_options;

decl_simple_lock_data(, vc_progress_lock);

#if defined(XNU_TARGET_OS_OSX)
static int                      vc_progress_withmeter = 3;
int                             vc_progressmeter_enable;
static int                      vc_progressmeter_drawn;
int                             vc_progressmeter_value;
static uint32_t                 vc_progressmeter_count;
static uint32_t                 vc_progress_meter_start;
static uint32_t                 vc_progress_meter_end;
static uint64_t                 vc_progressmeter_interval;
static uint64_t                 vc_progressmeter_deadline;
static thread_call_data_t       vc_progressmeter_call;
static void *                   vc_progressmeter_backbuffer;
static uint32_t                 vc_progressmeter_diskspeed = 256;

#endif  /* defined(XNU_TARGET_OS_OSX) */

enum {
	kSave          = 0x10,
	kDataIndexed   = 0x20,
	kDataAlpha     = 0x40,
	kDataBack      = 0x80,
	kDataRotate    = 0x03,
};

static void vc_blit_rect(int x, int y, int bx,
    int width, int height,
    int sourceWidth, int sourceHeight,
    int sourceRow, int backRow,
    const unsigned char * dataPtr,
    void * backBuffer,
    unsigned int flags);
static void vc_blit_rect_8(int x, int y, int bx,
    int width, int height,
    int sourceWidth, int sourceHeight,
    int sourceRow, int backRow,
    const unsigned char * dataPtr,
    unsigned char * backBuffer,
    unsigned int flags);
static void vc_blit_rect_16(int x, int y, int bx,
    int width, int height,
    int sourceWidth, int sourceHeight,
    int sourceRow, int backRow,
    const unsigned char * dataPtr,
    unsigned short * backBuffer,
    unsigned int flags);
static void vc_blit_rect_32(int x, int y, int bx,
    int width, int height,
    int sourceWidth, int sourceHeight,
    int sourceRow, int backRow,
    const unsigned char * dataPtr,
    unsigned int * backBuffer,
    unsigned int flags);
static void vc_blit_rect_30(int x, int y, int bx,
    int width, int height,
    int sourceWidth, int sourceHeight,
    int sourceRow, int backRow,
    const unsigned char * dataPtr,
    unsigned int * backBuffer,
    unsigned int flags);
static void vc_progress_task( void * arg0, void * arg );
#if defined(XNU_TARGET_OS_OSX)
static void vc_progressmeter_task( void * arg0, void * arg );
#endif  /* defined(XNU_TARGET_OS_OSX) */

static void
vc_blit_rect(int x, int y, int bx,
    int width, int height,
    int sourceWidth, int sourceHeight,
    int sourceRow, int backRow,
    const unsigned char * dataPtr,
    void * backBuffer,
    unsigned int flags)
{
	if (!vinfo.v_depth) {
		return;
	}
	if (((unsigned int)(x + width)) > vinfo.v_width) {
		return;
	}
	if (((unsigned int)(y + height)) > vinfo.v_height) {
		return;
	}

	switch (vinfo.v_depth) {
	case 8:
		if (vc_clut8 == vc_clut) {
			vc_blit_rect_8( x, y, bx, width, height, sourceWidth, sourceHeight, sourceRow, backRow, dataPtr, (unsigned char *) backBuffer, flags );
		}
		break;
	case 16:
		vc_blit_rect_16( x, y, bx, width, height, sourceWidth, sourceHeight, sourceRow, backRow, dataPtr, (unsigned short *) backBuffer, flags );
		break;
	case 32:
		vc_blit_rect_32( x, y, bx, width, height, sourceWidth, sourceHeight, sourceRow, backRow, dataPtr, (unsigned int *) backBuffer, flags );
		break;
	case 30:
		vc_blit_rect_30( x, y, bx, width, height, sourceWidth, sourceHeight, sourceRow, backRow, dataPtr, (unsigned int *) backBuffer, flags );
		break;
	}
}

static void
vc_blit_rect_8(int x, int y, __unused int bx,
    int width, int height,
    int sourceWidth, int sourceHeight,
    int sourceRow, __unused int backRow,
    const unsigned char * dataPtr,
    __unused unsigned char * backBuffer,
    __unused unsigned int flags)
{
	volatile unsigned short * dst;
	int line, col;
	unsigned int data = 0, out = 0;
	int sx, sy, a, b, c, d;
	int scale = 0x10000;

	a = (sourceRow == 1) ? 0 : vc_rotate_matr[kDataRotate & flags][0][0] * scale;
	b = (sourceRow == 1) ? 0 : vc_rotate_matr[kDataRotate & flags][0][1] * scale;
	c = vc_rotate_matr[kDataRotate & flags][1][0] * scale;
	d = vc_rotate_matr[kDataRotate & flags][1][1] * scale;

	sx = ((a + b) < 0) ? ((sourceWidth * scale)  - 0x8000) : 0;
	sy = ((c + d) < 0) ? ((sourceHeight * scale) - 0x8000) : 0;

	if (!sourceRow) {
		data = (unsigned int)(uintptr_t)dataPtr;
	}

	dst = (volatile unsigned short *) (vinfo.v_baseaddr +
	    (y * vinfo.v_rowbytes) +
	    (x * 4));

	for (line = 0; line < height; line++) {
		for (col = 0; col < width; col++) {
			if (sourceRow) {
				data = dataPtr[((sx + (col * a) + (line * b)) >> 16)
				    + sourceRow * (((sy + (col * c) + (line * d)) >> 16))];
			}
			if (kDataAlpha & flags) {
				out = vc_revclut8[data];
			} else {
				out = data;
			}
			*(dst + col) = out;
		}
		dst = (volatile unsigned short *) (((volatile char*)dst) + vinfo.v_rowbytes);
	}
}

/* 16-bit is 1555 (XRGB) on all platforms */

#define CLUT_MASK_R     0xf8
#define CLUT_MASK_G     0xf8
#define CLUT_MASK_B     0xf8
#define CLUT_SHIFT_R    << 7
#define CLUT_SHIFT_G    << 2
#define CLUT_SHIFT_B    >> 3
#define MASK_R          0x7c00
#define MASK_G          0x03e0
#define MASK_B          0x001f
#define MASK_R_8        0x3fc00
#define MASK_G_8        0x01fe0
#define MASK_B_8        0x000ff

static void
vc_blit_rect_16( int x, int y, int bx,
    int width, int height,
    int sourceWidth, int sourceHeight,
    int sourceRow, int backRow,
    const unsigned char * dataPtr,
    unsigned short * backPtr,
    unsigned int flags)
{
	volatile unsigned short * dst;
	int line, col;
	unsigned int data = 0, out = 0, back = 0;
	int sx, sy, a, b, c, d;
	int scale = 0x10000;

	a = (sourceRow == 1) ? 0 : vc_rotate_matr[kDataRotate & flags][0][0] * scale;
	b = (sourceRow == 1) ? 0 : vc_rotate_matr[kDataRotate & flags][0][1] * scale;
	c = vc_rotate_matr[kDataRotate & flags][1][0] * scale;
	d = vc_rotate_matr[kDataRotate & flags][1][1] * scale;

	sx = ((a + b) < 0) ? ((sourceWidth * scale)  - 0x8000) : 0;
	sy = ((c + d) < 0) ? ((sourceHeight * scale) - 0x8000) : 0;

	if (!sourceRow) {
		data = (unsigned int)(uintptr_t)dataPtr;
	}

	if (backPtr) {
		backPtr += bx;
	}
	dst = (volatile unsigned short *) (vinfo.v_baseaddr +
	    (y * vinfo.v_rowbytes) +
	    (x * 2));

	for (line = 0; line < height; line++) {
		for (col = 0; col < width; col++) {
			if (sourceRow) {
				data = dataPtr[((sx + (col * a) + (line * b)) >> 16)
				    + sourceRow * (((sy + (col * c) + (line * d)) >> 16))];
			}
			if (backPtr) {
				if (kSave & flags) {
					back = *(dst + col);
					*backPtr++ = back;
				} else {
					back = *backPtr++;
				}
			}
			if (kDataIndexed & flags) {
				out = ((CLUT_MASK_R & (vc_clut[data * 3 + 0]))CLUT_SHIFT_R)
				    | ((CLUT_MASK_G & (vc_clut[data * 3 + 1]))CLUT_SHIFT_G)
				    | ((CLUT_MASK_B & (vc_clut[data * 3 + 2]))CLUT_SHIFT_B);
			} else if (kDataAlpha & flags) {
				out = (((((back & MASK_R) * data) + MASK_R_8) >> 8) & MASK_R)
				    | (((((back & MASK_G) * data) + MASK_G_8) >> 8) & MASK_G)
				    | (((((back & MASK_B) * data) + MASK_B_8) >> 8) & MASK_B);
				if (vc_progress_white) {
					out += (((0xff - data) & CLUT_MASK_R)CLUT_SHIFT_R)
					    | (((0xff - data) & CLUT_MASK_G)CLUT_SHIFT_G)
					    | (((0xff - data) & CLUT_MASK_B)CLUT_SHIFT_B);
				}
			} else if (kDataBack & flags) {
				out = back;
			} else {
				out = data;
			}
			*(dst + col) = out;
		}
		dst = (volatile unsigned short *) (((volatile char*)dst) + vinfo.v_rowbytes);
		if (backPtr) {
			backPtr += backRow - width;
		}
	}
}


static void
vc_blit_rect_32(int x, int y, int bx,
    int width, int height,
    int sourceWidth, int sourceHeight,
    int sourceRow, int backRow,
    const unsigned char * dataPtr,
    unsigned int * backPtr,
    unsigned int flags)
{
	volatile unsigned int * dst;
	int line, col;
	unsigned int data = 0, out = 0, back = 0;
	int sx, sy, a, b, c, d;
	int scale = 0x10000;

	a = (sourceRow == 1) ? 0 : vc_rotate_matr[kDataRotate & flags][0][0] * scale;
	b = (sourceRow == 1) ? 0 : vc_rotate_matr[kDataRotate & flags][0][1] * scale;
	c = vc_rotate_matr[kDataRotate & flags][1][0] * scale;
	d = vc_rotate_matr[kDataRotate & flags][1][1] * scale;

	sx = ((a + b) < 0) ? ((sourceWidth * scale)  - 0x8000) : 0;
	sy = ((c + d) < 0) ? ((sourceHeight * scale) - 0x8000) : 0;

	if (!sourceRow) {
		data = (unsigned int)(uintptr_t)dataPtr;
	}

	if (backPtr) {
		backPtr += bx;
	}
	dst = (volatile unsigned int *) (vinfo.v_baseaddr +
	    (y * vinfo.v_rowbytes) +
	    (x * 4));

	for (line = 0; line < height; line++) {
		for (col = 0; col < width; col++) {
			if (sourceRow) {
				data = dataPtr[((sx + (col * a) + (line * b)) >> 16)
				    + sourceRow * (((sy + (col * c) + (line * d)) >> 16))];
			}
			if (backPtr) {
				if (kSave & flags) {
					back = *(dst + col);
					*backPtr++ = back;
				} else {
					back = *backPtr++;
				}
			}
			if (kDataIndexed & flags) {
				out =     (vc_clut[data * 3 + 0] << 16)
				    | (vc_clut[data * 3 + 1] << 8)
				    | (vc_clut[data * 3 + 2]);
			} else if (kDataAlpha & flags) {
				out = (((((back & 0x00ff00ff) * data) + 0x00ff00ff) >> 8) & 0x00ff00ff)
				    | (((((back & 0x0000ff00) * data) + 0x0000ff00) >> 8) & 0x0000ff00);
				if (vc_progress_white) {
					out += ((0xff - data) << 16)
					    | ((0xff - data) << 8)
					    |  (0xff - data);
				}
			} else if (kDataBack & flags) {
				out = back;
			} else {
				out = data;
			}
			*(dst + col) = out;
		}
		dst = (volatile unsigned int *) (((volatile char*)dst) + vinfo.v_rowbytes);
		if (backPtr) {
			backPtr += backRow - width;
		}
	}
}

static void
vc_blit_rect_30(int x, int y, int bx,
    int width, int height,
    int sourceWidth, int sourceHeight,
    int sourceRow, int backRow,
    const unsigned char * dataPtr,
    unsigned int * backPtr,
    unsigned int flags)
{
	volatile unsigned int * dst;
	int line, col;
	unsigned int data = 0, out = 0, back = 0;
	unsigned long long exp;
	int sx, sy, a, b, c, d;
	int scale = 0x10000;

	a = (sourceRow == 1) ? 0 : vc_rotate_matr[kDataRotate & flags][0][0] * scale;
	b = (sourceRow == 1) ? 0 : vc_rotate_matr[kDataRotate & flags][0][1] * scale;
	c = vc_rotate_matr[kDataRotate & flags][1][0] * scale;
	d = vc_rotate_matr[kDataRotate & flags][1][1] * scale;

	sx = ((a + b) < 0) ? ((sourceWidth * scale)  - 0x8000) : 0;
	sy = ((c + d) < 0) ? ((sourceHeight * scale) - 0x8000) : 0;

	if (!sourceRow) {
		data = (unsigned int)(uintptr_t)dataPtr;
	}

	if (backPtr) {
		backPtr += bx;
	}
	dst = (volatile unsigned int *) (vinfo.v_baseaddr +
	    (y * vinfo.v_rowbytes) +
	    (x * 4));

	for (line = 0; line < height; line++) {
		for (col = 0; col < width; col++) {
			if (sourceRow) {
				data = dataPtr[((sx + (col * a) + (line * b)) >> 16)
				    + sourceRow * (((sy + (col * c) + (line * d)) >> 16))];
			}
			if (backPtr) {
				if (kSave & flags) {
					back = *(dst + col);
					*backPtr++ = back;
				} else {
					back = *backPtr++;
				}
			}
			if (kDataIndexed & flags) {
				out =     (vc_clut[data * 3 + 0] << 22)
				    | (vc_clut[data * 3 + 1] << 12)
				    | (vc_clut[data * 3 + 2] << 2);
			} else if (kDataAlpha & flags) {
				exp = back;
				exp =  (((((exp & 0x3FF003FF) * data) + 0x0FF000FF) >> 8) & 0x3FF003FF)
				    | (((((exp & 0x000FFC00) * data) + 0x0003FC00) >> 8) & 0x000FFC00);
				out = (unsigned int)exp;
				if (vc_progress_white) {
					out += ((0xFF - data) << 22)
					    | ((0xFF - data) << 12)
					    | ((0xFF - data) << 2);
				}
			} else if (kDataBack & flags) {
				out = back;
			} else {
				out = data;
			}
			*(dst + col) = out;
		}
		dst = (volatile unsigned int *) (((volatile char*)dst) + vinfo.v_rowbytes);
		if (backPtr) {
			backPtr += backRow - width;
		}
	}
}

static void
vc_clean_boot_graphics(void)
{
#if defined(XNU_TARGET_OS_OSX)
	// clean up possible FDE login graphics
	vc_progress_set(FALSE, 0);
	const unsigned char *
	    color = (typeof(color))(uintptr_t)(vc_progress_white ? 0x00000000 : 0xBFBFBFBF);
	vc_blit_rect(0, 0, 0, vinfo.v_width, vinfo.v_height, vinfo.v_width, vinfo.v_height, 0, 0, color, NULL, 0);
#endif
}

/*
 * Routines to render the lzss image format
 */

struct lzss_image_state {
	uint32_t col;
	uint32_t row;
	uint32_t width;
	uint32_t height;
	uint32_t bytes_per_row;
	volatile uint32_t * row_start;
	const uint8_t* clut;
};
typedef struct lzss_image_state lzss_image_state;

// returns 0 if OK, 1 if error
static inline int
vc_decompress_lzss_next_pixel(int next_data, lzss_image_state* state)
{
	uint32_t palette_index = 0;
	uint32_t pixel_value   = 0;

	palette_index = next_data * 3;

	pixel_value = ((uint32_t) state->clut[palette_index + 0] << 16)
	    | ((uint32_t) state->clut[palette_index + 1] << 8)
	    | ((uint32_t) state->clut[palette_index + 2]);

	*(state->row_start + state->col) = pixel_value;

	if (++state->col >= state->width) {
		state->col = 0;
		if (++state->row >= state->height) {
			return 1;
		}
		state->row_start = (volatile uint32_t *) (((uintptr_t)state->row_start) + state->bytes_per_row);
	}
	return 0;
}


/*
 * Blit an lzss compressed image to the framebuffer
 * Assumes 32 bit screen (which is everything we ship at the moment)
 * The function vc_display_lzss_icon was copied from libkern/mkext.c, then modified.
 */

/*
 * TODO: Does lzss use too much stack? 4096 plus bytes...
 *      Can probably chop it down by 1/2.
 */

/**************************************************************
*   LZSS.C -- A Data Compression Program
***************************************************************
*    4/6/1989 Haruhiko Okumura
*    Use, distribute, and modify this program freely.
*    Please send me your improved versions.
*        PC-VAN      SCIENCE
*        NIFTY-Serve PAF01022
*        CompuServe  74050,1022
*
**************************************************************/

#define N         4096  /* size of ring buffer - must be power of 2 */
#define F         18    /* upper limit for match_length */
#define THRESHOLD 2     /* encode string into position and length
	                 *  if match_length is greater than this */

// returns 0 if OK, 1 if error
// x and y indicate upper left corner of image location on screen
int
vc_display_lzss_icon(uint32_t dst_x, uint32_t dst_y,
    uint32_t image_width, uint32_t image_height,
    const uint8_t *compressed_image,
    uint32_t       compressed_size,
    const uint8_t *clut)
{
	uint32_t* image_start;
	uint32_t bytes_per_pixel = 4;
	uint32_t bytes_per_row = vinfo.v_rowbytes;

	vc_clean_boot_graphics();

	image_start = (uint32_t *) (vinfo.v_baseaddr + (dst_y * bytes_per_row) + (dst_x * bytes_per_pixel));

	lzss_image_state state = {0, 0, image_width, image_height, bytes_per_row, image_start, clut};

	int rval = 0;

	const uint8_t *src = compressed_image;
	uint32_t srclen = compressed_size;

	/* ring buffer of size N, with extra F-1 bytes to aid string comparison */
	uint8_t text_buf[N + F - 1];
	const uint8_t *srcend = src + srclen;
	int  i, j, k, r, c;
	unsigned int flags;

	srcend = src + srclen;
	for (i = 0; i < N - F; i++) {
		text_buf[i] = ' ';
	}
	r = N - F;
	flags = 0;
	for (;;) {
		if (((flags >>= 1) & 0x100) == 0) {
			if (src < srcend) {
				c = *src++;
			} else {
				break;
			}
			flags = c | 0xFF00; /* uses higher byte cleverly */
		} /* to count eight */
		if (flags & 1) {
			if (src < srcend) {
				c = *src++;
			} else {
				break;
			}
			rval = vc_decompress_lzss_next_pixel(c, &state);
			if (rval != 0) {
				return rval;
			}
			text_buf[r++] = c;
			r &= (N - 1);
		} else {
			if (src < srcend) {
				i = *src++;
			} else {
				break;
			}
			if (src < srcend) {
				j = *src++;
			} else {
				break;
			}
			i |= ((j & 0xF0) << 4);
			j  =  (j & 0x0F) + THRESHOLD;
			for (k = 0; k <= j; k++) {
				c = text_buf[(i + k) & (N - 1)];
				rval = vc_decompress_lzss_next_pixel(c, &state);
				if (rval != 0) {
					return rval;
				}
				text_buf[r++] = c;
				r &= (N - 1);
			}
		}
	}
	return 0;
}

void
noroot_icon_test(void)
{
	boolean_t o_vc_progress_enable = vc_progress_enable;

	vc_progress_enable = 1;

	PE_display_icon( 0, "noroot");

	vc_progress_enable = o_vc_progress_enable;
}


void
vc_display_icon( vc_progress_element * desc,
    const unsigned char * data )
{
	int                 x, y, width, height;

	if (vc_progress_enable && vc_clut) {
		vc_clean_boot_graphics();

		width = desc->width;
		height = desc->height;
		x = desc->dx;
		y = desc->dy;
		if (1 & desc->flags) {
			x += ((vinfo.v_width - width) / 2);
			y += ((vinfo.v_height - height) / 2);
		}
		vc_blit_rect( x, y, 0, width, height, width, height, width, 0, data, NULL, kDataIndexed );
	}
}

void
vc_progress_initialize( vc_progress_element * desc,
    const unsigned char * data1x,
    const unsigned char * data2x,
    const unsigned char * data3x,
    const unsigned char * clut )
{
	uint64_t    abstime;

	if ((!clut) || (!desc) || (!data1x)) {
		return;
	}
	vc_clut = clut;
	vc_clut8 = clut;

	vc_progress = desc;
	vc_progress_data[0] = data1x;
	vc_progress_data[1] = data2x;
	vc_progress_data[2] = data3x;
	if (2 & vc_progress->flags) {
		vc_progress_alpha = data1x
		    + vc_progress->count * vc_progress->width * vc_progress->height;
	} else {
		vc_progress_alpha = NULL;
	}

	thread_call_setup(&vc_progress_call, vc_progress_task, NULL);
	clock_interval_to_absolutetime_interval(vc_progress->time, 1000 * 1000, &abstime);
	vc_progress_interval = (uint32_t)abstime;

#if defined(XNU_TARGET_OS_OSX)
	thread_call_setup(&vc_progressmeter_call, vc_progressmeter_task, NULL);
	clock_interval_to_absolutetime_interval(1000 / 8, 1000 * 1000, &abstime);
	vc_progressmeter_interval = (uint32_t)abstime;
#endif  /* defined(XNU_TARGET_OS_OSX) */
}

void
vc_progress_set(boolean_t enable, uint32_t vc_delay)
{
	void             *saveBuf = NULL;
	vm_size_t        saveLen = 0;
	unsigned int     count;
	unsigned int     index;
	unsigned char    pdata8;
	unsigned short   pdata16;
	unsigned short * buf16;
	unsigned int     pdata32;
	unsigned int *   buf32;

	if (!vc_progress) {
		return;
	}

#if defined(CONFIG_VC_PROGRESS_METER_SUPPORT)

#if defined (__x86_64__)
	if (kBootArgsFlagBlack & ((boot_args *) PE_state.bootArgs)->flags) {
		return;
	}
#endif /* defined (__x86_64__) */

	if (1 & vc_progress_withmeter) {
		if (enable) {
			internal_enable_progressmeter(kProgressMeterKernel);
		}

		simple_lock(&vc_progress_lock, LCK_GRP_NULL);

		if (vc_progress_enable != enable) {
			vc_progress_enable = enable;
			if (enable) {
				vc_progressmeter_count = 0;
				clock_interval_to_deadline(vc_delay,
				    1000 * 1000 * 1000 /*second scale*/,
				    &vc_progressmeter_deadline);
				thread_call_enter_delayed(&vc_progressmeter_call, vc_progressmeter_deadline);
			} else {
				thread_call_cancel(&vc_progressmeter_call);
			}
		}

		simple_unlock(&vc_progress_lock);

		if (!enable) {
			internal_enable_progressmeter(kProgressMeterOff);
		}
		return;
	}

#endif /* defined(CONFIG_VC_PROGRESS_METER_SUPPORT) */

	if (enable) {
		saveLen = (vc_progress->width * vc_uiscale) * (vc_progress->height * vc_uiscale) * ((vinfo.v_depth + 7) / 8);
		saveBuf = kalloc_data(saveLen, Z_WAITOK);

		switch (vinfo.v_depth) {
		case 8:
			for (count = 0; count < 256; count++) {
				vc_revclut8[count] = vc_clut[0x01 * 3];
				pdata8 = (vc_clut[0x01 * 3] * count + 0x0ff) >> 8;
				for (index = 0; index < 256; index++) {
					if ((pdata8 == vc_clut[index * 3 + 0]) &&
					    (pdata8 == vc_clut[index * 3 + 1]) &&
					    (pdata8 == vc_clut[index * 3 + 2])) {
						vc_revclut8[count] = index;
						break;
					}
				}
			}
			memset( saveBuf, 0x01, saveLen );
			break;

		case 16:
			buf16 = (unsigned short *) saveBuf;
			pdata16 = ((vc_clut[0x01 * 3 + 0] & CLUT_MASK_R)CLUT_SHIFT_R)
			    | ((vc_clut[0x01 * 3 + 0] & CLUT_MASK_G)CLUT_SHIFT_G)
			    | ((vc_clut[0x01 * 3 + 0] & CLUT_MASK_B)CLUT_SHIFT_B);
			for (count = 0; count < saveLen / 2; count++) {
				buf16[count] = pdata16;
			}
			break;

		case 32:
			buf32 = (unsigned int *) saveBuf;
			pdata32 = ((vc_clut[0x01 * 3 + 0] & 0xff) << 16)
			    | ((vc_clut[0x01 * 3 + 1] & 0xff) << 8)
			    | ((vc_clut[0x01 * 3 + 2] & 0xff) << 0);
			for (count = 0; count < saveLen / 4; count++) {
				buf32[count] = pdata32;
			}
			break;
		}
	}

	simple_lock(&vc_progress_lock, LCK_GRP_NULL);

	if (vc_progress_enable != enable) {
		vc_progress_enable = enable;
		if (enable) {
			vc_needsave      = TRUE;
			vc_saveunder     = saveBuf;
			vc_saveunder_len = saveLen;
			saveBuf               = NULL;
			saveLen           = 0;
			vc_progress_count = 0;
			vc_progress_angle = 0;

			clock_interval_to_deadline(vc_delay,
			    1000 * 1000 * 1000 /*second scale*/,
			    &vc_progress_deadline);
			thread_call_enter_delayed(&vc_progress_call, vc_progress_deadline);
		} else {
			if (vc_saveunder) {
				saveBuf      = vc_saveunder;
				saveLen      = vc_saveunder_len;
				vc_saveunder = NULL;
				vc_saveunder_len = 0;
			}

			thread_call_cancel(&vc_progress_call);
		}
	}

	simple_unlock(&vc_progress_lock);

	kfree_data(saveBuf, saveLen);
}

#if defined(XNU_TARGET_OS_OSX)

static uint32_t
vc_progressmeter_range(uint32_t pos)
{
	uint32_t ret;

	if (pos > kProgressMeterEnd) {
		pos = kProgressMeterEnd;
	}
	ret = vc_progress_meter_start
	    + ((pos * (vc_progress_meter_end - vc_progress_meter_start)) / kProgressMeterEnd);

	return ret;
}

static void
vc_progressmeter_task(__unused void *arg0, __unused void *arg)
{
	uint64_t interval;

	simple_lock(&vc_progress_lock, LCK_GRP_NULL);
	if (kProgressMeterKernel == vc_progressmeter_enable) {
		uint32_t pos = (vc_progressmeter_count >> 13);
		internal_set_progressmeter(vc_progressmeter_range(pos));
		if (pos < kProgressMeterEnd) {
			static uint16_t incr[8] = { 10000, 10000, 8192, 4096, 2048, 384, 384, 64 };
			vc_progressmeter_count += incr[(pos * 8) / kProgressMeterEnd];

			interval = vc_progressmeter_interval;
			interval = ((interval * 256) / vc_progressmeter_diskspeed);

			clock_deadline_for_periodic_event(interval, mach_absolute_time(), &vc_progressmeter_deadline);
			thread_call_enter_delayed(&vc_progressmeter_call, vc_progressmeter_deadline);
		}
	}
	simple_unlock(&vc_progress_lock);
}

void
vc_progress_setdiskspeed(uint32_t speed)
{
	vc_progressmeter_diskspeed = speed;
}

#endif  /* defined(XNU_TARGET_OS_OSX) */

static void
vc_progress_task(__unused void *arg0, __unused void *arg)
{
	int       x, y, width, height;
	uint64_t  x_pos, y_pos;
	const unsigned char * data;

	simple_lock(&vc_progress_lock, LCK_GRP_NULL);

	if (vc_progress_enable) {
		do {
			vc_progress_count++;
			if (vc_progress_count >= vc_progress->count) {
				vc_progress_count = 0;
				vc_progress_angle++;
			}

			width  = (vc_progress->width * vc_uiscale);
			height = (vc_progress->height * vc_uiscale);
			data   = vc_progress_data[vc_uiscale - 1];
			if (!data) {
				break;
			}

			if (kVCUsePosition & vc_progress_options.options) {
				/* Rotation: 0:normal, 1:right 90, 2:left 180, 3:left 90 */
				switch (3 & vinfo.v_rotate) {
				case kDataRotate0:
					x_pos = vc_progress_options.x_pos;
					y_pos = vc_progress_options.y_pos;
					break;
				case kDataRotate180:
					x_pos = 0xFFFFFFFF - vc_progress_options.x_pos;
					y_pos = 0xFFFFFFFF - vc_progress_options.y_pos;
					break;
				case kDataRotate90:
					x_pos = 0xFFFFFFFF - vc_progress_options.y_pos;
					y_pos = vc_progress_options.x_pos;
					break;
				case kDataRotate270:
					x_pos = vc_progress_options.y_pos;
					y_pos = 0xFFFFFFFF - vc_progress_options.x_pos;
					break;
				}
				x = (uint32_t)((x_pos * (uint64_t) vinfo.v_width) / 0xFFFFFFFFULL);
				y = (uint32_t)((y_pos * (uint64_t) vinfo.v_height) / 0xFFFFFFFFULL);
				x -= (width / 2);
				y -= (height / 2);
			} else {
				x = (vc_progress->dx * vc_uiscale);
				y = (vc_progress->dy * vc_uiscale);
				if (1 & vc_progress->flags) {
					x += ((vinfo.v_width - width) / 2);
					y += ((vinfo.v_height - height) / 2);
				}
			}

			if ((x + width) > (int)vinfo.v_width) {
				break;
			}
			if ((y + height) > (int)vinfo.v_height) {
				break;
			}

			data += vc_progress_count * width * height;

			vc_blit_rect( x, y, 0,
			    width, height, width, height, width, width,
			    data, vc_saveunder,
			    kDataAlpha
			    | (vc_progress_angle & kDataRotate)
			    | (vc_needsave ? kSave : 0));
			vc_needsave = FALSE;

			clock_deadline_for_periodic_event(vc_progress_interval, mach_absolute_time(), &vc_progress_deadline);
			thread_call_enter_delayed(&vc_progress_call, vc_progress_deadline);
		}while (FALSE);
	}

#if SCHED_HYGIENE_DEBUG
	abandon_preemption_disable_measurement();
#endif /* SCHED_HYGIENE_DEBUG */

	simple_unlock(&vc_progress_lock);
}

/*
 * Generic Console (Front-End): Master Control
 * -------------------------------------------
 */

#if defined (__i386__) || defined (__x86_64__)
#include <pexpert/i386/boot.h>
#endif

static boolean_t gc_acquired      = FALSE;
static boolean_t gc_graphics_boot = FALSE;
static boolean_t gc_desire_text   = FALSE;
static boolean_t gc_paused_progress;

static vm_offset_t  lastVideoVirt    = 0;
static vm_size_t    lastVideoMapSize = 0;
static boolean_t    lastVideoMapKmap = FALSE;

static void
gc_pause( boolean_t pause, boolean_t graphics_now )
{
	VCPUTC_LOCK_LOCK();

	disableConsoleOutput = (pause && !console_is_serial());
	gc_enabled           = (!pause && !graphics_now);

	VCPUTC_LOCK_UNLOCK();

	simple_lock(&vc_progress_lock, LCK_GRP_NULL);

	if (pause) {
		gc_paused_progress = vc_progress_enable;
		vc_progress_enable = FALSE;
	} else {
		vc_progress_enable = gc_paused_progress;
	}

	if (vc_progress_enable) {
#if defined(XNU_TARGET_OS_OSX)
		if (1 & vc_progress_withmeter) {
			thread_call_enter_delayed(&vc_progressmeter_call, vc_progressmeter_deadline);
		} else
#endif /* defined(XNU_TARGET_OS_OSX) */
		thread_call_enter_delayed(&vc_progress_call, vc_progress_deadline);
	}

	simple_unlock(&vc_progress_lock);
}

static void
vc_initialize(__unused struct vc_info * vinfo_p)
{
	vinfo.v_rows = vinfo.v_height / ISO_CHAR_HEIGHT;
	vinfo.v_columns = vinfo.v_width / ISO_CHAR_WIDTH;
	vinfo.v_rowscanbytes = ((vinfo.v_depth + 7) / 8) * vinfo.v_width;
	vc_uiscale = vinfo.v_scale;
	if (vc_uiscale > kMaxProgressData) {
		vc_uiscale = kMaxProgressData;
	} else if (!vc_uiscale) {
		vc_uiscale = 1;
	}
}

void
initialize_screen(PE_Video * boot_vinfo, unsigned int op)
{
	unsigned int newMapSize = 0;
	vm_offset_t newVideoVirt = 0;
	boolean_t graphics_now;
	uint32_t delay;

	if (boot_vinfo) {
		struct vc_info new_vinfo = vinfo;
		boolean_t makeMapping = FALSE;

		/*
		 *	Copy parameters
		 */
		if (kPEBaseAddressChange != op) {
			new_vinfo.v_width    = (unsigned int)boot_vinfo->v_width;
			new_vinfo.v_height   = (unsigned int)boot_vinfo->v_height;
			new_vinfo.v_depth    = (unsigned int)boot_vinfo->v_depth;
			new_vinfo.v_rowbytes = (unsigned int)boot_vinfo->v_rowBytes;
			if (kernel_map == VM_MAP_NULL) {
				// only booter supplies HW rotation
				new_vinfo.v_rotate   = (unsigned int)boot_vinfo->v_rotate;
			}
#if defined(__i386__) || defined(__x86_64__)
			new_vinfo.v_type     = (unsigned int)boot_vinfo->v_display;
#else
			new_vinfo.v_type = 0;
#endif
			unsigned int scale   = (unsigned int)boot_vinfo->v_scale;
			if (scale == kPEScaleFactor1x) {
				new_vinfo.v_scale = kPEScaleFactor1x;
			} else if (scale == kPEScaleFactor2x) {
				new_vinfo.v_scale = kPEScaleFactor2x;
			}
			else { /* Scale factor not set, default to 1x */
				new_vinfo.v_scale = kPEScaleFactor1x;
			}
		}
		new_vinfo.v_name[0]  = 0;
		new_vinfo.v_physaddr = 0;

		/*
		 *  Check if we are have to map the framebuffer
		 *  If VM is up, we are given a virtual address, unless b0 is set to indicate physical.
		 */
		newVideoVirt = boot_vinfo->v_baseAddr;
		makeMapping = (kernel_map == VM_MAP_NULL) || (0 != (1 & newVideoVirt));
		if (makeMapping) {
			newVideoVirt = 0;
			new_vinfo.v_physaddr = boot_vinfo->v_baseAddr & ~3UL;           /* Get the physical address */
#ifndef __LP64__
			new_vinfo.v_physaddr |= (((uint64_t) boot_vinfo->v_baseAddrHigh) << 32);
#endif
			kprintf("initialize_screen: b=%08llX, w=%08X, h=%08X, r=%08X, d=%08X\n",                  /* (BRINGUP) */
			    new_vinfo.v_physaddr, new_vinfo.v_width, new_vinfo.v_height, new_vinfo.v_rowbytes, new_vinfo.v_type);       /* (BRINGUP) */
		}

		if (!newVideoVirt && !new_vinfo.v_physaddr) {                                                   /* Check to see if we have a framebuffer */
			kprintf("initialize_screen: No video - forcing serial mode\n");         /* (BRINGUP) */
			new_vinfo.v_depth = 0;                                          /* vc routines are nop */
			(void)switch_to_serial_console();                               /* Switch into serial mode */
			gc_graphics_boot = FALSE;                                       /* Say we are not in graphics mode */
			disableConsoleOutput = FALSE;                                   /* Allow printfs to happen */
			gc_acquired = TRUE;
		} else {
			if (makeMapping) {
#if HAS_UCNORMAL_MEM
				/*
				 * Framebuffers would normally use VM_WIMG_RT, which
				 * io_map doesn't support.  However this buffer is set up
				 * by the bootloader and doesn't require D$ cleaning, so
				 * VM_WIMG_RT and VM_WIMG_WCOMB are functionally
				 * equivalent.
				 */
				unsigned int flags = VM_WIMG_WCOMB;
#else
				unsigned int flags = VM_WIMG_IO;
#endif
				if (boot_vinfo->v_length != 0) {
					newMapSize = (unsigned int) round_page(boot_vinfo->v_length);
				} else {
					newMapSize = (unsigned int) round_page(new_vinfo.v_height * new_vinfo.v_rowbytes);                      /* Remember size */
				}
				newVideoVirt = ml_io_map_unmappable((vm_map_offset_t)new_vinfo.v_physaddr, newMapSize, flags);   /* Allocate address space for framebuffer */
			}
			new_vinfo.v_baseaddr = newVideoVirt + boot_vinfo->v_offset;                     /* Set the new framebuffer address */
		}

#if defined(__x86_64__)
		// Adjust the video buffer pointer to point to where it is in high virtual (above the hole)
		new_vinfo.v_baseaddr |= (VM_MIN_KERNEL_ADDRESS & ~LOW_4GB_MASK);
#endif

		/* Update the vinfo structure atomically with respect to the vc_progress task if running */
		if (vc_progress) {
			simple_lock(&vc_progress_lock, LCK_GRP_NULL);
			vinfo = new_vinfo;
			simple_unlock(&vc_progress_lock);
		} else {
			vinfo = new_vinfo;
		}

		// If we changed the virtual address, remove the old mapping
		if (newVideoVirt != 0) {
			if (lastVideoVirt && lastVideoMapSize) {                                                /* Was the framebuffer mapped before? */
				/* XXX why only !4K? */
				if (!TEST_PAGE_SIZE_4K && lastVideoMapSize) {
					pmap_remove(kernel_pmap, trunc_page_64(lastVideoVirt),
					    round_page_64(lastVideoVirt + lastVideoMapSize));           /* Toss mappings */
				}
				/* Was this not a special pre-VM mapping? */
				if (lastVideoMapKmap) {
					kmem_free(kernel_map, lastVideoVirt, lastVideoMapSize); /* Toss kernel addresses */
				}
			}
			lastVideoMapKmap = (NULL != kernel_map);                /* Remember how mapped */
			lastVideoMapSize = newMapSize;                                  /* Remember the size */
			lastVideoVirt    = newVideoVirt;                                /* Remember the virtual framebuffer address */
		}

		if (kPEBaseAddressChange != op) {
			// Graphics mode setup by the booter.

			gc_ops.initialize   = vc_initialize;
			gc_ops.enable       = vc_enable;
			gc_ops.paint_char   = vc_paint_char;
			gc_ops.scroll_down  = vc_scroll_down;
			gc_ops.scroll_up    = vc_scroll_up;
			gc_ops.clear_screen = vc_clear_screen;
			gc_ops.hide_cursor  = vc_reverse_cursor;
			gc_ops.show_cursor  = vc_reverse_cursor;
			gc_ops.update_color = vc_update_color;
			gc_initialize(&vinfo);
		}
	}

	graphics_now = gc_graphics_boot && !gc_desire_text;
	switch (op) {
	case kPEGraphicsMode:
		gc_graphics_boot = TRUE;
		gc_desire_text = FALSE;
		break;

	case kPETextMode:
		gc_graphics_boot = FALSE;
		break;

	case kPEAcquireScreen:
		if (gc_acquired) {
			break;
		}

		vc_progress_options = vc_user_options;
		bzero(&vc_user_options, sizeof(vc_user_options));

		if (kVCAcquireImmediate & vc_progress_options.options) {
			delay = 0;
		} else if (kVCDarkReboot & vc_progress_options.options) {
			delay = 120;
		} else {
			delay = vc_acquire_delay;
		}

		if (kVCDarkBackground & vc_progress_options.options) {
			vc_progress_white = TRUE;
		} else if (kVCLightBackground & vc_progress_options.options) {
			vc_progress_white = FALSE;
		}

#if !defined(XNU_TARGET_OS_BRIDGE)
		vc_progress_set( graphics_now, delay );
#endif /* !defined(XNU_TARGET_OS_BRIDGE) */
		gc_enable( !graphics_now );
		gc_acquired = TRUE;
		gc_desire_text = FALSE;
		break;

	case kPEDisableScreen:
		if (gc_acquired) {
			gc_pause( TRUE, graphics_now );
		}
		break;

	case kPEEnableScreen:
		if (gc_acquired) {
			gc_pause( FALSE, graphics_now );
		}
		break;

	case kPETextScreen:
		if (console_is_serial()) {
			break;
		}

		if (gc_acquired == FALSE) {
			gc_desire_text = TRUE;
			break;
		}
		if (gc_graphics_boot == FALSE) {
			break;
		}

		vc_progress_set( FALSE, 0 );
#if defined(XNU_TARGET_OS_OSX)
		vc_enable_progressmeter( FALSE );
#endif
		gc_enable( TRUE );
		break;

	case kPEReleaseScreen:
		gc_acquired = FALSE;
		gc_desire_text = FALSE;
		gc_enable( FALSE );
		if (gc_graphics_boot == FALSE) {
			break;
		}

		vc_progress_set( FALSE, 0 );
		vc_acquire_delay = kProgressReacquireDelay;
		vc_progress_white      = TRUE;
#if defined(XNU_TARGET_OS_OSX)
		vc_enable_progressmeter(FALSE);
		vc_progress_withmeter &= ~1;
#endif
		vc_clut8 = NULL;
		break;


#if defined(__x86_64__)
	case kPERefreshBootGraphics:
	{
		boolean_t save;

		if (kBootArgsFlagBlack & ((boot_args *) PE_state.bootArgs)->flags) {
			break;
		}

		save = vc_progress_white;
		vc_progress_white = (0 != (kBootArgsFlagBlackBg & ((boot_args *) PE_state.bootArgs)->flags));

		internal_enable_progressmeter(kProgressMeterKernel);

		simple_lock(&vc_progress_lock, LCK_GRP_NULL);

		vc_progressmeter_drawn = 0;
		internal_set_progressmeter(vc_progressmeter_range(vc_progressmeter_count >> 13));

		simple_unlock(&vc_progress_lock);

		internal_enable_progressmeter(kProgressMeterOff);
		vc_progress_white = save;
	}
#endif
	}
}

void vcattach(void); /* XXX gcc 4 warning cleanup */

void
vcattach(void)
{
	vm_initialized = TRUE;

#if defined(CONFIG_VC_PROGRESS_METER_SUPPORT)
	const boot_args * bootargs  = (typeof(bootargs))PE_state.bootArgs;

	PE_parse_boot_argn("meter", &vc_progress_withmeter, sizeof(vc_progress_withmeter));

#if defined(__x86_64__)
	vc_progress_white = (0 != ((kBootArgsFlagBlackBg | kBootArgsFlagLoginUI)
	    & bootargs->flags));
	if (kBootArgsFlagInstallUI & bootargs->flags) {
		vc_progress_meter_start = (bootargs->bootProgressMeterStart * kProgressMeterMax) / 65535;
		vc_progress_meter_end   = (bootargs->bootProgressMeterEnd   * kProgressMeterMax) / 65535;
	} else {
		vc_progress_meter_start = 0;
		vc_progress_meter_end   = kProgressMeterEnd;
	}
#else
	vc_progress_meter_start = 0;
	vc_progress_meter_end   = kProgressMeterEnd;
#endif /* defined(__x86_64__ */
#endif /* defined(CONFIG_VC_PROGRESS_METER_SUPPORT) */
	simple_lock_init(&vc_progress_lock, 0);

	if (gc_graphics_boot == FALSE) {
		long index;

		if (gc_acquired) {
			initialize_screen(NULL, kPEReleaseScreen);
		}

		initialize_screen(NULL, kPEAcquireScreen);

		for (index = 0; index < msgbufp->msg_bufx; index++) {
			if (msgbufp->msg_bufc[index] == '\0') {
				continue;
			}

			vcputc( msgbufp->msg_bufc[index] );

			if (msgbufp->msg_bufc[index] == '\n') {
				vcputc( '\r' );
			}
		}
	}
}

#if defined(XNU_TARGET_OS_OSX)

// redraw progress meter between pixels start, end, position at pos,
// options (including rotation) passed in flags
static void
vc_draw_progress_meter(unsigned int flags, int start, int end, int pos)
{
	const unsigned char *data;
	int i, width, bx, srcRow, backRow;
	int rectX, rectY, rectW, rectH;
	int endCapPos, endCapStart;
	int barWidth  = kProgressBarWidth * vc_uiscale;
	int barHeight = kProgressBarHeight * vc_uiscale;
	int capWidth  = kProgressBarCapWidth * vc_uiscale;
	// 1 rounded fill, 0 square end
	int style = (0 == (2 & vc_progress_withmeter));
	// 1 white, 0 greyed out
	int onoff;

	for (i = start; i < end; i += width) {
		onoff       = (i < pos);
		endCapPos   = ((style && onoff) ? pos : barWidth);
		endCapStart = endCapPos - capWidth;
		if (flags & kDataBack) { // restore back bits
			width   = end;// loop done after this iteration
			data    = NULL;
			srcRow  = 0;
		} else if (i < capWidth) { // drawing the left cap
			width   = (end < capWidth) ? (end - i) : (capWidth - i);
			data    = progressmeter_leftcap[vc_uiscale >= 2][onoff];
			data    += i;
			srcRow  = capWidth;
		} else if (i < endCapStart) { // drawing the middle
			width   = (end < endCapStart) ? (end - i) : (endCapStart - i);
			data    = progressmeter_middle[vc_uiscale >= 2][onoff];
			srcRow  = 1;
		} else { // drawing the right cap
			width   = endCapPos - i;
			data    = progressmeter_rightcap[vc_uiscale >= 2][onoff];
			data    += i - endCapStart;
			srcRow  = capWidth;
		}

		switch (flags & kDataRotate) {
		case kDataRotate90: // left middle, bar goes down
			rectW   = barHeight;
			rectH   = width;
			rectX   = (6 * vinfo.v_width) / 100 + 34 * vc_uiscale - (barHeight / 2);
			rectY   = ((vinfo.v_height - barWidth) / 2) + i;
			bx      = i * barHeight;
			backRow = barHeight;
			break;
		case kDataRotate180: // middle upper, bar goes left
			rectW   = width;
			rectH   = barHeight;
			rectX   = ((vinfo.v_width - barWidth) / 2) + barWidth - width - i;
			rectY   = (6 * vinfo.v_height) / 100 + 34 * vc_uiscale - (barHeight / 2);
			bx      = barWidth - width - i;
			backRow = barWidth;
			break;
		case kDataRotate270: // right middle, bar goes up
			rectW   = barHeight;
			rectH   = width;
			rectX   = (94 * vinfo.v_width) / 100 - 34 * vc_uiscale - (barHeight / 2);
			rectY   = ((vinfo.v_height - barWidth) / 2) + barWidth - width - i;
			bx      = (barWidth - width - i) * barHeight;
			backRow = barHeight;
			break;
		default:
		case kDataRotate0: // middle lower, bar goes right
			rectW   = width;
			rectH   = barHeight;
			rectX   = ((vinfo.v_width - barWidth) / 2) + i;
			rectY   = (94 * vinfo.v_height) / 100 - 34 * vc_uiscale - (barHeight / 2);
			bx      = i;
			backRow = barWidth;
			break;
		}
		vc_blit_rect(rectX, rectY, bx, rectW, rectH, width, barHeight,
		    srcRow, backRow, data, vc_progressmeter_backbuffer, flags);
	}
}

extern void IORecordProgressBackbuffer(void * buffer, size_t size, uint32_t theme);

static void
internal_enable_progressmeter(int new_value)
{
	void    * new_buffer;
	boolean_t stashBackbuffer;
	int flags = vinfo.v_rotate;

	stashBackbuffer = FALSE;
	new_buffer = NULL;
	if (new_value) {
		new_buffer = kalloc_data((kProgressBarWidth * vc_uiscale) *
		    (kProgressBarHeight * vc_uiscale) * sizeof(int), Z_WAITOK);
	}

	simple_lock(&vc_progress_lock, LCK_GRP_NULL);

	if (kProgressMeterUser == new_value) {
		if (gc_enabled || !gc_acquired || !gc_graphics_boot) {
			new_value = vc_progressmeter_enable;
		}
	}

	if (new_value != vc_progressmeter_enable) {
		if (new_value) {
			if (kProgressMeterOff == vc_progressmeter_enable) {
				vc_progressmeter_backbuffer = new_buffer;
				vc_draw_progress_meter(kDataAlpha | kSave | flags, 0, (kProgressBarWidth * vc_uiscale), 0);
				new_buffer = NULL;
				vc_progressmeter_drawn = 0;
			}
			vc_progressmeter_enable = new_value;
		} else if (vc_progressmeter_backbuffer) {
			if (kProgressMeterUser == vc_progressmeter_enable) {
				vc_draw_progress_meter(kDataBack | flags, 0, (kProgressBarWidth * vc_uiscale), vc_progressmeter_drawn);
			} else {
				stashBackbuffer = TRUE;
			}
			new_buffer = vc_progressmeter_backbuffer;
			vc_progressmeter_backbuffer = NULL;
			vc_progressmeter_enable = FALSE;
		}
	}

	simple_unlock(&vc_progress_lock);

	if (new_buffer) {
		if (stashBackbuffer) {
			IORecordProgressBackbuffer(new_buffer,
			    (kProgressBarWidth * vc_uiscale)
			    * (kProgressBarHeight * vc_uiscale)
			    * sizeof(int),
			    vc_progress_white);
		}
		kfree_data(new_buffer, (kProgressBarWidth * vc_uiscale) *
		    (kProgressBarHeight * vc_uiscale) * sizeof(int));
	}
}

static void
internal_set_progressmeter(int new_value)
{
	int x1, x3;
	int capRedraw;
	// 1 rounded fill, 0 square end
	int style = (0 == (2 & vc_progress_withmeter));
	int flags = kDataAlpha | vinfo.v_rotate;

	if ((new_value < 0) || (new_value > kProgressMeterMax)) {
		return;
	}

	if (vc_progressmeter_enable) {
		vc_progressmeter_value = new_value;

		capRedraw = (style ? (kProgressBarCapWidth * vc_uiscale) : 0);
		x3 = (((kProgressBarWidth * vc_uiscale) - 2 * capRedraw) * vc_progressmeter_value) / kProgressMeterMax;
		x3 += (2 * capRedraw);

		if (x3 > vc_progressmeter_drawn) {
			x1 = capRedraw;
			if (x1 > vc_progressmeter_drawn) {
				x1 = vc_progressmeter_drawn;
			}
			vc_draw_progress_meter(flags, vc_progressmeter_drawn - x1, x3, x3);
		} else {
			vc_draw_progress_meter(flags, x3 - capRedraw, vc_progressmeter_drawn, x3);
		}
		vc_progressmeter_drawn = x3;
	}
}

void
vc_enable_progressmeter(int new_value)
{
	internal_enable_progressmeter(new_value ? kProgressMeterUser : kProgressMeterOff);
}

void
vc_set_progressmeter(int new_value)
{
	simple_lock(&vc_progress_lock, LCK_GRP_NULL);

	if (vc_progressmeter_enable) {
		if (kProgressMeterKernel != vc_progressmeter_enable) {
			internal_set_progressmeter(new_value);
		}
	} else {
		vc_progressmeter_value = new_value;
	}

	simple_unlock(&vc_progress_lock);
}

#endif /* defined(XNU_TARGET_OS_OSX) */
