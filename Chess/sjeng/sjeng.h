/*
    Sjeng - a chess variants playing program
    Copyright (C) 2000 Gian-Carlo Pascutto

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
                                                          
    File: sjeng.h
    Purpose: global definitions                  

*/

#ifndef SJENG_H
#define SJENG_H

#include "config.h"
#include <ctype.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef HAVE_SYS_TIMEB_H
#include <sys/timeb.h>
#endif

#define NDEBUG 
#include <assert.h>

#define DIE (*(int *)(NULL) = 0)

/* GCP : my code uses WHITE=0 and BLACK=1 so reverse this */

#define WHITE 0
#define BLACK 1

#define ToMove (white_to_move ? 0 : 1)
#define NotToMove (white_to_move ? 1 : 0)

#define Hash(x,y) (hash ^= zobrist[(x)][(y)])

#define Crazyhouse 0
#define Bughouse 1
#define Normal 2
#define Suicide 3
#define Losers 4

#define Opening      0
#define Middlegame   1
#define Endgame      2

#define mindepth 2

/* define names for piece constants: */
#define frame   0
#define wpawn   1
#define bpawn   2
#define wknight 3
#define bknight 4
#define wking   5
#define bking   6
#define wrook   7
#define brook   8
#define wqueen  9
#define bqueen  10
#define wbishop 11
#define bbishop 12
#define npiece  13

/* result flags: */
#define no_result      0
#define stalemate      1
#define white_is_mated 2
#define black_is_mated 3
#define draw_by_fifty  4
#define draw_by_rep    5

/* arrays maybe ? */ 
#undef FASTCALC
#ifdef FASTCALC
#define rank(square) ((((square)-26)/12)+1)
#define file(square) ((((square)-26)%12)+1)
#else
#define rank(square) (rank[(square)])
#define file(square) (file[(square)])
#endif
#define diagl(square) (diagl[(square)])
#define diagr(square) (diagr[(square)])

#ifndef INPROBECODE
typedef enum {FALSE, TRUE} bool;
#endif

/* castle flags: */
#define no_castle  0
#define wck        1
#define wcq        2
#define bck        3
#define bcq        4

typedef struct {
  int from;
  int target;
  int captured;
  int promoted;	      
  int castled;
  int ep; 
} move_s;

typedef struct {
  int cap_num;
  int was_promoted;
  int epsq;
  int fifty;
} move_x;

#if defined(HAVE_SYS_TIMEB_H) && (defined(HAVE_FTIME) || defined(HAVE_GETTIMEOFDAY)) 
typedef struct timeb rtime_t;
#else
typedef time_t rtime_t;
#endif

#define STR_BUFF 256
#define MOVE_BUFF 512
#define INF 1000000
#define PV_BUFF 300

#define AddMaterial(x) Material += material[(x)]
#define RemoveMaterial(x) Material -= material[(x)]

#define UPPER 1
#define LOWER 2
#define EXACT 3
#define HMISS 4
#define DUMMY 0

#define LOSS 0
#define WIN 1
#define DRAW 2

#define max(x, y) ((x) > (y) ? (x) : (y))
#define mix(x, y) ((x) < (y) ? (x) : (y))

#endif
