/*
    Sjeng - a chess variants playing program
    Copyright (C) 2000-2001 Gian-Carlo Pascutto

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

    As a special exception, Gian-Carlo Pascutto gives permission
    to link this program with the Nalimov endgame database access
    code. See the Copying/Distribution section in the README file
    for more details.

    File: probe.c                                        
    Purpose: access Nalimov endgame databases

*/
#include "sjeng.h"
#include "protos.h"
#include "extvars.h"

#undef USE_EGTB

#define  XX  127
#define  KINGCAP 50000

typedef unsigned int INDEX;
typedef int square;

#if defined (_MSC_VER)
#define  TB_FASTCALL  __fastcall
#else
#define  TB_FASTCALL
#endif

typedef  int  color;
#define  x_colorWhite  0
#define  x_colorBlack  1
#define  x_colorNeutral  2
#define COLOR_DECLARED

typedef  int  piece;
#define  x_pieceNone    0
#define  x_piecePawn    1
#define  x_pieceKnight  2
#define  x_pieceBishop  3
#define  x_pieceRook    4
#define  x_pieceQueen   5
#define  x_pieceKing    6
#define PIECES_DECLARED

typedef signed char tb_t;

#define pageL       65536
#define tbbe_ssL    ((pageL-4)/2)
#define bev_broken  (tbbe_ssL+1)  /* illegal or busted */
#define bev_mi1     tbbe_ssL      /* mate in 1 move */
#define bev_mimin   1               /* mate in max moves */
#define bev_draw    0               /* draw */
#define bev_limax   (-1)            /* mated in max moves */
#define bev_li0     (-tbbe_ssL)   /* mated in 0 moves */

typedef INDEX (TB_FASTCALL * PfnCalcIndex)
    (square*, square*, square, int fInverse);

extern int IDescFindFromCounters (int*);
extern int FRegisteredFun (int, color);
extern PfnCalcIndex PfnIndCalcFun (int, color);
extern int TB_FASTCALL L_TbtProbeTable (int, color, INDEX);

extern int IInitializeTb (char*);
extern int FTbSetCacheSize(void *pv, unsigned long cbSize);

#define PfnIndCalc PfnIndCalcFun
#define FRegistered FRegisteredFun

int EGTBProbes;
int EGTBHits;
int EGTBPieces;
int EGTBCacheSize;
char EGTBDir[STR_BUFF];

void init_egtb(void)
{
#ifdef USE_EGTB
  void *buffer;

  buffer = malloc(EGTBCacheSize);

  if (buffer == NULL && (EGTBCacheSize != 0)) 
  {
	printf("Could not allocate EGTB buffer.\n");
	exit(EXIT_FAILURE);
  };

  EGTBPieces = IInitializeTb (EGTBDir);

  printf("%d piece endgame tablebases found\n", EGTBPieces);
  printf("Allocated %dKb for indices and tables.\n",((cbEGTBCompBytes+1023)/1024));
  
  if(FTbSetCacheSize (buffer, EGTBCacheSize) == FALSE
      && (EGTBCacheSize != 0))
  {
  	printf("Could not enable EGTB buffer.\n");
	exit(EXIT_FAILURE);
  };
  
  return;
#else
  return;
#endif
}

const static int EGTranslate(int sqidx)
{
  int r;

  r = (((rank(sqidx)-1)*8)+(file(sqidx)-1));
  
  return r;
}

int probe_egtb(void)
{
#ifdef USE_EGTB
  int *psqW, *psqB;
  int rgiCounters[10] = {0,0,0,0,0,0,0,0,0,0};
  int side;
  int fInvert;
  int sqEnP;
  int wi = 1, W[8] = {6,0,0,0,0,0};
  int bi = 1, B[8] = {6,0,0,0,0,0};
  int tbScore;
  INDEX ind;
  int j, a, i;
  int iTb;

  EGTBProbes++;
  
  W[4] = EGTranslate(wking_loc);
  B[4] = EGTranslate(bking_loc);

  for (j = 1, a = 1;(a <= piece_count); j++)
  {
  	i = pieces[j];
	if (!i)
		continue;
	else
		a++;
	switch(board[i])
	{
		case wpawn:
			rgiCounters[0]++;
			W[wi] = 1;
			W[wi+4] = EGTranslate(i);
			wi++;
			break;
		case wknight:
			rgiCounters[1]++;
			W[wi] = 2;
			W[wi+4] = EGTranslate(i);
			wi++;
			break;
		case wbishop:
			rgiCounters[2]++;
			W[wi] = 3;
			W[wi+4] = EGTranslate(i);
			wi++;
			break;
		case wrook:
			rgiCounters[3]++;
			W[wi] = 4;
			W[wi+4] = EGTranslate(i);
			wi++;
			break;
		case wqueen:
			rgiCounters[4]++;
			W[wi] = 5;
			W[wi+4] = EGTranslate(i);
			wi++;
			break;
		case bpawn:
			rgiCounters[5]++;
			B[bi] = 1;
			B[bi+4] = EGTranslate(i);
			bi++;
			break;
		case bknight:
			rgiCounters[6]++;
			B[bi] = 2;
			B[bi+4] = EGTranslate(i);
			bi++;
			break;
		case bbishop:
			rgiCounters[7]++;
			B[bi] = 3;
			B[bi+4] = EGTranslate(i);
			bi++;
			break;
		case brook:
			rgiCounters[8]++;
			B[bi] = 4;
			B[bi+4] = EGTranslate(i);
			bi++;
			break;
		case bqueen:
			rgiCounters[9]++;
			B[bi] = 5;
			B[bi+4] = EGTranslate(i);
			bi++;
			break;
	}
  }

  /* more than 4 pieces for one side: not a class we can index */
  if (wi >= 4 || bi >= 4)
  {
	  return KINGCAP;
  }

  iTb = IDescFindFromCounters (rgiCounters);
  if (0 == iTb)
  {
  	return KINGCAP;
  }
  else if (iTb > 0)
  {
  	/* white = 0*/
  	side = !white_to_move;
	fInvert = 0;
	psqW = W;
	psqB = B;
  }
  else
  {
  	side = white_to_move;
	fInvert = 1;
	psqW = B;
	psqB = W;
	iTb = -iTb;
  }
  if (!FRegistered(iTb, side))
  	return KINGCAP;
  
  if (ep_square == 0)
  {
  	sqEnP = XX;
  }
  else
  {
    	if (white_to_move)
	{
	  if (board[ep_square - 11] == wpawn || board[ep_square - 13] == wpawn)
  		sqEnP = EGTranslate(ep_square);
	  else
	    	sqEnP = XX;
	}
	else
	{
	  if (board[ep_square + 11] == bpawn || board[ep_square + 13] == bpawn)
  		sqEnP = EGTranslate(ep_square);
	  else
	    	sqEnP = XX;
	  
	}
  }

  ind = PfnIndCalc(iTb, side) (psqW, psqB, sqEnP, fInvert);

  tbScore = L_TbtProbeTable( iTb, side, ind);

  if (tbScore == bev_broken) return KINGCAP;

  EGTBHits++;
  
  if (tbScore > 0)
  {
  	return ((tbScore-bev_mi1)*2+INF-ply-1);
  }
  else if (tbScore < 0)
  {
  	return ((tbScore+bev_mi1)*2-INF+ply);
  }
  return 0;
#else
  return KINGCAP;
#endif
}
