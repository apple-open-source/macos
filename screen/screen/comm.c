/* Copyright (c) 1993-2002
 *      Juergen Weigert (jnweiger@immd4.informatik.uni-erlangen.de)
 *      Michael Schroeder (mlschroe@immd4.informatik.uni-erlangen.de)
 * Copyright (c) 1987 Oliver Laumann
 *
#ifdef HAVE_BRAILLE
 * Modified by:
 *      Authors:  Hadi Bargi Rangin  bargi@dots.physics.orst.edu
 *                Bill Barry         barryb@dots.physics.orst.edu
 *
 * Modifications Copyright (c) 1995 by
 * Science Access Project, Oregon State University.
#endif
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (see the file COPYING); if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 ****************************************************************
 */

#include "config.h"
#include "acls.h"
#include "comm.h"

#define bcopy :-(		/* or include screen.h here */

/* Must be in alpha order ! */

struct comm comms[RC_LAST + 1] =
{
#ifdef MULTIUSER
  { "acladd",		ARGS_1234 },
  { "aclchg",		ARGS_23 },
  { "acldel",		ARGS_1 },
  { "aclgrp",		ARGS_12 },
  { "aclumask",		ARGS_1|ARGS_ORMORE },
#endif
  { "activity",		ARGS_1 },
#ifdef MULTIUSER
  { "addacl",		ARGS_1234 },
#endif
  { "allpartial",	NEED_DISPLAY|ARGS_1 },
  { "altscreen",	ARGS_01 },
  { "at",		NEED_DISPLAY|ARGS_2|ARGS_ORMORE },
#ifdef COLOR
  { "attrcolor",	ARGS_12 },
#endif
  { "autodetach",	ARGS_1 },
#ifdef AUTO_NUKE
  { "autonuke",		NEED_DISPLAY|ARGS_1 },
#endif
  { "backtick",		ARGS_1|ARGS_ORMORE },
#ifdef COLOR
  { "bce",		NEED_FORE|ARGS_01 },
#endif

#ifdef HAVE_BRAILLE
/* keywords for braille display (bd) */
  { "bd_bc_down",	ARGS_0 },
  { "bd_bc_left",	ARGS_0 },
  { "bd_bc_right",	ARGS_0 },
  { "bd_bc_up",		ARGS_0 },
  { "bd_bell",		ARGS_01 },
  { "bd_braille_table",	ARGS_01 },
  { "bd_eightdot",	ARGS_01 },
  { "bd_info",		ARGS_01 },
  { "bd_link",		ARGS_01 },
  { "bd_lower_left",	ARGS_0 },
  { "bd_lower_right",	ARGS_0 },
  { "bd_ncrc",		ARGS_01 },
  { "bd_port",		ARGS_01 },
  { "bd_scroll",	ARGS_01 },
  { "bd_skip",		ARGS_01 },
  { "bd_start_braille",	ARGS_01 },
  { "bd_type",		ARGS_01 },
  { "bd_upper_left",	ARGS_0 },
  { "bd_upper_right",	ARGS_0 },
  { "bd_width",		ARGS_01 },
#endif

  { "bell",		ARGS_01 },
  { "bell_msg",		ARGS_01 },
  { "bind",		ARGS_1|ARGS_ORMORE },
#ifdef MAPKEYS
  { "bindkey",		ARGS_0|ARGS_ORMORE },
#endif
  { "blanker",		NEED_DISPLAY|ARGS_0},
#ifdef BLANKER_PRG
  { "blankerprg",	ARGS_1|ARGS_ORMORE },
#endif
  { "break",		NEED_FORE|ARGS_01 },
  { "breaktype",	NEED_FORE|ARGS_01 },
#ifdef COPY_PASTE
  { "bufferfile",	ARGS_01 },
#endif
  { "c1",		NEED_FORE|ARGS_01 },
  { "caption",		ARGS_12 },
#ifdef MULTIUSER
  { "chacl",		ARGS_23 },
#endif
  { "charset",          NEED_FORE|ARGS_1 },
  { "chdir",		ARGS_01 },
  { "clear",		NEED_FORE|ARGS_0 },
  { "colon",		NEED_LAYER|ARGS_01 },
  { "command",		NEED_DISPLAY|ARGS_02 },
#ifdef COPY_PASTE
  { "compacthist",	ARGS_01 },
#endif
  { "console",		NEED_FORE|ARGS_01 },
#ifdef COPY_PASTE
  { "copy",		NEED_FORE|ARGS_0 },
  { "crlf",		ARGS_01 },
#endif
  { "debug",		ARGS_01 },
#ifdef AUTO_NUKE
  { "defautonuke",	ARGS_1 },
#endif
#ifdef COLOR
  { "defbce",		ARGS_1 },
#endif
  { "defbreaktype",	ARGS_01 },
  { "defc1",		ARGS_1 },
  { "defcharset",       ARGS_01 },
#ifdef ENCODINGS
  { "defencoding",	ARGS_1 },
#endif
  { "defescape",	ARGS_1 },
  { "defflow",		ARGS_12 },
  { "defgr",		ARGS_1 },
  { "defhstatus",	ARGS_01 },
#ifdef ENCODINGS
  { "defkanji",		ARGS_1 },
#endif
  { "deflog",		ARGS_1 },
#if defined(UTMPOK) && defined(LOGOUTOK)
  { "deflogin",		ARGS_1 },
#endif
  { "defmode",		ARGS_1 },
  { "defmonitor",	ARGS_1 },
#ifdef MULTI
  { "defnonblock",	ARGS_1 },
#endif
  { "defobuflimit",	ARGS_1 },
#ifdef COPY_PASTE
  { "defscrollback",	ARGS_1 },
#endif
  { "defshell",		ARGS_1 },
  { "defsilence",	ARGS_1 },
  { "defslowpaste",	ARGS_1 },
#ifdef UTF8
  { "defutf8",		ARGS_1 },
#endif
  { "defwrap",		ARGS_1 },
  { "defwritelock",	ARGS_1 },
#ifdef DETACH
  { "detach",		NEED_DISPLAY|ARGS_01 },
#endif
  { "digraph",		NEED_LAYER|ARGS_01 },
  { "dinfo",		NEED_DISPLAY|ARGS_0 },
  { "displays",		NEED_LAYER|ARGS_0 },
  { "dumptermcap",	NEED_FORE|ARGS_0 },
  { "echo",		ARGS_12 },
#ifdef ENCODINGS
  { "encoding",		ARGS_12 },
#endif
  { "escape",		ARGS_1 },
  { "eval",		ARGS_1|ARGS_ORMORE },
#ifdef PSEUDOS
  { "exec", 		NEED_FORE|ARGS_0|ARGS_ORMORE },
#endif
  { "fit",		NEED_DISPLAY|ARGS_0 },
  { "flow",		NEED_FORE|ARGS_01 },
  { "focus",		NEED_DISPLAY|ARGS_01 },
  { "gr",		NEED_FORE|ARGS_01 },
  { "hardcopy",		ARGS_012 },
  { "hardcopy_append",	ARGS_1 },
  { "hardcopydir",	ARGS_01 },
  { "hardstatus",	ARGS_012 },
  { "height",		ARGS_0123 },
  { "help",		NEED_LAYER|ARGS_02 },
#ifdef COPY_PASTE
  { "history",		NEED_DISPLAY|NEED_FORE|ARGS_0 },
#endif
  { "hstatus",		NEED_FORE|ARGS_1 },
  { "idle",		ARGS_0|ARGS_ORMORE },
  { "ignorecase",	ARGS_01 },
  { "info",		NEED_LAYER|ARGS_0 },
#ifdef ENCODINGS
  { "kanji",		NEED_FORE|ARGS_12 },
#endif
  { "kill",		NEED_FORE|ARGS_0 },
  { "lastmsg",		NEED_DISPLAY|ARGS_0 },
  { "license",		NEED_LAYER|ARGS_0 },
#ifdef LOCK
  { "lockscreen",	NEED_DISPLAY|ARGS_0 },
#endif
  { "log",		NEED_FORE|ARGS_01 },
  { "logfile",		ARGS_012 },
#if defined(UTMPOK) && defined(LOGOUTOK)
  { "login",		NEED_FORE|ARGS_01 },
#endif
  { "logtstamp",	ARGS_012 },
#ifdef MAPKEYS
  { "mapdefault",	NEED_DISPLAY|ARGS_0 },
  { "mapnotnext",	NEED_DISPLAY|ARGS_0 },
  { "maptimeout",	ARGS_01 },
#endif
#ifdef COPY_PASTE
  { "markkeys",		ARGS_1 },
#endif
  { "maxwin",		ARGS_1 },
  { "meta",		NEED_LAYER|ARGS_0 },
  { "monitor",		NEED_FORE|ARGS_01 },
  { "msgminwait",	ARGS_1 },
  { "msgwait",		ARGS_1 },
#ifdef MULTIUSER
  { "multiuser",	ARGS_1 },
#endif
#ifdef NETHACK
  { "nethack",		ARGS_1 },
#endif
  { "next",		ARGS_0 },
#ifdef MULTI
  { "nonblock",		NEED_DISPLAY|ARGS_01 },
#endif
  { "number",		NEED_FORE|ARGS_01 },
  { "obuflimit",	NEED_DISPLAY|ARGS_01 },
  { "only",		NEED_DISPLAY|ARGS_0 },
  { "other",		ARGS_0 },
  { "partial",		NEED_FORE|ARGS_01 },
#ifdef PASSWORD
  { "password",		ARGS_01 },
#endif
#ifdef COPY_PASTE
  { "paste",		NEED_LAYER|ARGS_012 },
  { "pastefont",	ARGS_01 },
#endif
  { "pow_break",	NEED_FORE|ARGS_01 },
#if defined(DETACH) && defined(POW_DETACH)
  { "pow_detach",	NEED_DISPLAY|ARGS_0 },
  { "pow_detach_msg",	ARGS_01 },
#endif
  { "prev",		ARGS_0 },
  { "printcmd",		ARGS_01 },
  { "process",		NEED_DISPLAY|ARGS_01 },
  { "quit",		ARGS_0 },
#ifdef COPY_PASTE
  { "readbuf",		ARGS_0123 },
#endif
  { "readreg",          ARGS_0|ARGS_ORMORE },
  { "redisplay",	NEED_DISPLAY|ARGS_0 },
  { "register",		ARGS_24 },
  { "remove",		NEED_DISPLAY|ARGS_0 },
#ifdef COPY_PASTE
  { "removebuf",	ARGS_0 },
#endif
  { "reset",		NEED_FORE|ARGS_0 },
  { "resize",		NEED_DISPLAY|ARGS_01 },
  { "screen",		ARGS_0|ARGS_ORMORE },
#ifdef COPY_PASTE
  { "scrollback",	NEED_FORE|ARGS_1 },
#endif
  { "select",		ARGS_01 },
  { "sessionname",	ARGS_01 },
  { "setenv",		ARGS_012 },
  { "setsid",		ARGS_1 },
  { "shell",		ARGS_1 },
  { "shelltitle",	ARGS_1 },
  { "silence",		NEED_FORE|ARGS_01 },
  { "silencewait",	ARGS_1 },
  { "sleep",		ARGS_1 },
  { "slowpaste",	NEED_FORE|ARGS_01 },
  { "sorendition",      ARGS_012 },
  { "source",		ARGS_1 },
  { "split",		NEED_DISPLAY|ARGS_0 },
  { "startup_message",	ARGS_1 },
  { "stuff",		NEED_LAYER|ARGS_12 },
#ifdef MULTIUSER
  { "su",		NEED_DISPLAY|ARGS_012 },
#endif
#ifdef BSDJOBS
  { "suspend",		NEED_DISPLAY|ARGS_0 },
#endif
  { "term",		ARGS_1 },
  { "termcap",		ARGS_23 },
  { "termcapinfo",	ARGS_23 },
  { "terminfo",		ARGS_23 },
  { "time",		ARGS_01 },
  { "title",		NEED_FORE|ARGS_01 },
  { "umask",		ARGS_1|ARGS_ORMORE },
  { "unsetenv",		ARGS_1 },
#ifdef UTF8
  { "utf8",		NEED_FORE|ARGS_012 },
#endif
  { "vbell",		ARGS_01 },
  { "vbell_msg",	ARGS_01 },
  { "vbellwait",	ARGS_1 },
  { "verbose",		ARGS_01 },
  { "version",		ARGS_0 },
  { "wall",		NEED_DISPLAY|ARGS_1},
  { "width",		ARGS_0123 },
  { "windowlist",	NEED_DISPLAY|ARGS_012 },
  { "windows",		NEED_DISPLAY|ARGS_0 },
  { "wrap",		NEED_FORE|ARGS_01 },
#ifdef COPY_PASTE
  { "writebuf",		ARGS_0123 },
#endif
  { "writelock",	NEED_FORE|ARGS_01 },
  { "xoff",		NEED_LAYER|ARGS_0 },
  { "xon",		NEED_LAYER|ARGS_0 },
#ifdef ZMODEM
  { "zmodem",		ARGS_012 },
#endif
  { "zombie",		ARGS_01 }
};
