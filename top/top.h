/*
 * Copyright (c) 2002-2004 Apple Computer, Inc.  All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * System header includes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <pwd.h>
#include <sys/time.h>
#include <time.h>
#include <sys/ioctl.h>

#define TOP_DBG
#ifndef TOP_DBG
/* Disable assertions. */
#ifndef NDEBUG
#define NDEBUG
#endif
#endif
#include <assert.h>

/*
 * Project includes.
 */
#include "libtop.h"

#include "disp.h"
#include "log.h"
#include "samp.h"

/* Fields by which process samples can be sorted. */
typedef enum {
	TOP_SORT_command,
	TOP_SORT_cpu,
	TOP_SORT_pid,
	TOP_SORT_prt,
	TOP_SORT_reg,
	TOP_SORT_rprvt,
	TOP_SORT_rshrd,
	TOP_SORT_rsize,
	TOP_SORT_th,
	TOP_SORT_time,
	TOP_SORT_uid,
	TOP_SORT_username,
	TOP_SORT_vprvt,
	TOP_SORT_vsize
} top_sort_key_t;

/* Return a pointer to a string representation of a sorting key. */
const char *
top_sort_key_str(top_sort_key_t a_key, boolean_t a_ascend);

/*
 * Program options.
 */

/* Event counting mode. [aden]. */
extern char		top_opt_c;

/* Report shared library statistics. */
extern boolean_t	top_opt_f;

/* Interval between samples */
extern unsigned		top_opt_i;
#define TOP_MAX_INTERVAL	100

/* Forced interactive mode. */
extern boolean_t	top_opt_L;

/* Logging mode. */
extern boolean_t	top_opt_l;

/* Number of log samples (if opt_l). */
extern unsigned		top_opt_l_samples;

/* Number of processes to display. */
extern unsigned		top_opt_n;
#define TOP_MAX_NPROCS	0x7fffffff

/* Secondary sorting key. */
extern top_sort_key_t top_opt_O;

/* Ascend/descend for opt_O. */
extern boolean_t	top_opt_O_ascend;

/* Primary sorting key. */
extern top_sort_key_t   top_opt_o;

/* Ascend/descend for opt_o. */
extern boolean_t	top_opt_o_ascend;

/* Optional format string for process lines */
extern char *           top_opt_p_format;

/* Optional custom legend string */
extern char *           top_opt_P_legend;

/* Report memory object map for each process. */
extern boolean_t	top_opt_r;

/* Display swap usage and purgeable info */
extern boolean_t	top_opt_S;

/* Sample delay, in seconds. */
extern unsigned		top_opt_s;

/* Translate uid numbers to usernames. */
extern boolean_t	top_opt_t;

/* Only display processes owned by opt_uid. */
extern boolean_t	top_opt_U;

/* Display procs owned by uid (if opt_U). */
extern boolean_t	top_opt_U_uid;

/* Display deltas in wide format. */
extern boolean_t	top_opt_w;

#ifdef TOP_DEPRECATED
/* Enable deprecated functionality and display format. */
extern boolean_t	top_opt_x;
#endif

/* format identifiers */
#define FORMAT_PID           'a'
#define FORMAT_COMMAND       'b'
#define FORMAT_PERCENT_CPU   'c'
#define FORMAT_TIME          'd'
#define FORMAT_THREADS       'e'
#define FORMAT_PORTS         'f'
#define FORMAT_REGIONS       'g'
#define FORMAT_RPRVT         'h'
#define FORMAT_RSHRD         'i'
#define FORMAT_RSIZE         'j'
#define FORMAT_VPRVT         'k'
#define FORMAT_VSIZE         'l'
#define FORMAT_UID           'm'
#define FORMAT_USERNAME      'n'
#define FORMAT_FAULTS        'o'
#define FORMAT_PAGEINS       'p'
#define FORMAT_PAGEINS_D     'P'
#define FORMAT_COW_FAULTS    'q'
#define FORMAT_MSGS_SENT     'r'
#define FORMAT_MSGS_RECEIVED 's'
#define FORMAT_BSYSCALL      't'
#define FORMAT_MSYSCALL      'u'
#define FORMAT_CSWITCH       'v'
#define FORMAT_TIME_HHMMSS   'w'

#define VALID_FORMATS "abcdefghijklmnopqrstuvw"
#define FORMAT_ESCAPE        '\'
#define FORMAT_LEFT          '^'
#define FORMAT_RIGHT         '$'
#define FORMAT_DELTA         '-'
#define VALID_ESCAPES        "\\^$-"

#define LEGEND_CN  "  PID COMMAND      %CPU   TIME   #TH #PRTS #MREGS RPRVT  RSHRD  RSIZE  VSIZE"
#define FORMAT_CN  "$aaaa ^bbbbbbbbb $cccc% $wwwwwww $ee $ffff-$ggggg $hhhh- $iiii- $jjjj- $llll-"
#define LEGEND_CNW "  PID COMMAND      %CPU   TIME   #TH #PRTS(delta) #MREGS RPRVT(delta) RSHRD(delta) RSIZE(delta) VSIZE(delta)"
#define FORMAT_CNW "$aaaa ^bbbbbbbbb $cccc% $wwwwwww $ee $ffff------- $ggggg $hhhh------- $iiii------- $jjjj------- $llll-------"
#define LEGEND_CA  "  PID COMMAND      %CPU   TIME    FAULTS   PAGEINS COW_FAULTS MSGS_SENT  MSGS_RCVD  BSDSYSCALL MACHSYSCALL CSWITCH"
#define FORMAT_CA  "$aaaa ^bbbbbbbbb $cccc% $wwwwwww ^oooooo  ^ppppppp ^qqqqqqqqq ^rrrrrrrr  ^ssssssss  ^ttttttttt ^uuuuuuuuuu ^vvvvvv"
#define LEGEND_CD  "  PID COMMAND      %CPU   TIME   FAULTS PGINS/COWS MSENT/MRCVD    BSD/MACH      CSW"
#define FORMAT_CD  "$aaaa ^bbbbbbbbb $cccc% $wwwwwww $ooooo $pppp/^qqq $rrrr/^ssss $ttttt/^uuuuu $vvvvv"
#define LEGEND_CE LEGEND_CA
#define FORMAT_CE FORMAT_CA

#define LEGEND_XCN  "  PID   UID  REG RPRVT  RSHRD  RSIZE  VPRVT  VSIZE  TH PRT    TIME  %CPU COMMAND"
#define FORMAT_XCN  "$aaaa $mmmm $ggg $hhhh- $iiii- $jjjj- $kkkk- $llll-$ee $ff-$dddddd $cccc ^bbbbbbbbbbbbbbb"
#define LEGEND_XCNW "  PID   UID  REG RPRVT( delta) RSHRD( delta) RSIZE( delta) VPRVT( delta) VSIZE( delta) TH PRT(delta)   TIME  %CPU COMMAND"
#define FORMAT_XCNW "$aaaa $mmmm $ggg $hhhh-------- $iiii-------- $jjjj-------- $kkkk-------- $llll--------$ee $ff-------$dddddd $cccc ^bbbbbbbbbbbbbbb"
#define LEGEND_XTCN  "  PID USERNAME  REG  RPRVT  RSHRD  RSIZE  VPRVT  VSIZE  TH PRT    TIME  %CPU COMMAND"
#define FORMAT_XTCN  "$aaaa ^nnnnnnnn $ggg $hhhh- $iiii- $jjjj- $kkkk- $llll-$ee $ff-$dddddd $cccc ^bbbbbbbbbbbbbbb"
#define LEGEND_XTCNW "  PID USERNAME  REG  RPRVT( delta) RSHRD( delta) RSIZE( delta) VPRVT( delta) VSIZE( delta) TH PRT(delta)   TIME  %CPU COMMAND"
#define FORMAT_XTCNW "$aaaa ^nnnnnnnn $ggg $hhhh-------- $iiii-------- $jjjj-------- $kkkk-------- $llll--------$ee $ff-------$dddddd $cccc ^bbbbbbbbbbbbbbb"
#define LEGEND_XCA "  PID   UID  REG RPRVT  RSHRD  RSIZE  VPRVT  VSIZE  TH PRT      FAULTS    PAGEINS COW_FAULTS  MSGS_SENT  MSGS_RCVD   BSYSCALL   MSYSCALL    CSWITCH   TIME  %CPU COMMAND"
#define FORMAT_XCA "$aaaa $mmmm $ggg $hhhh  $iiii $jjjjj  $kkkk  $llll $ee $ff  $ooooooooo  $pppppppp $qqqqqqqqq  $rrrrrrrr  $ssssssss   $ttttttt   $uuuuuuu    $vvvvvv $ddddd$ccccc $bbbbbbbbbbbbbbbb"
#define LEGEND_XTCA "  PID USERNAME  REG RPRVT  RSHRD  RSIZE  VPRVT  VSIZE  TH PRT      FAULTS    PAGEINS COW_FAULTS  MSGS_SENT  MSGS_RCVD   BSYSCALL   MSYSCALL    CSWITCH   TIME  %CPU COMMAND"
#define FORMAT_XTCA "$aaaa ^nnnnnnnn $ggg $hhhh  $iiii $jjjjj  $kkkk  $llll $ee $ff  $ooooooooo  $ppppppp $qqqqqqqqq  $rrrrrrrr  $ssssssss   $ttttttt   $uuuuuuu    $vvvvvv $ddddd$ccccc $bbbbbbbbbbbbbbbb"
#define LEGEND_XCD LEGEND_XCA
#define FORMAT_XCD FORMAT_XCA
#define LEGEND_XCE LEGEND_XCA
#define FORMAT_XCE FORMAT_XCA
#define LEGEND_XTCD LEGEND_XTCA
#define FORMAT_XTCD FORMAT_XTCA
#define LEGEND_XTCE LEGEND_XTCA
#define FORMAT_XTCE FORMAT_XTCA

