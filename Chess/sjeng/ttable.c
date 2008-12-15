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

    File: ttable.c                                       
    Purpose: handling of transposition tables and hashes

*/

#include "sjeng.h"
#include "protos.h"
#include "extvars.h"
#include "limits.h"

uint32_t zobrist[17][144];

uint32_t hash;

uint32_t TTProbes;
uint32_t TTHits;
uint32_t TTStores;

typedef struct 
{
  signed char Depth;  
  /* unsigned char may be a bit small for bughouse/crazyhouse */
  unsigned short Bestmove;
  unsigned OnMove:1, Threat:1, Type:2;
  uint32_t Hash;
  uint32_t Hold_hash;
  int32_t Bound;
}
TType;

typedef struct
{
  unsigned short Bestmove;
  unsigned OnMove:1, Type:2;
  uint32_t Hash;
  uint32_t Hold_hash;
  int32_t Bound;
}
QTType;

/*TType DP_TTable[TTSIZE];
TType AS_TTable[TTSIZE];
*/

TType *DP_TTable;
TType *AS_TTable;
QTType *QS_TTable;

void clear_tt(void)
{
  memset(DP_TTable, 0, sizeof(TType) * TTSize);
  memset(AS_TTable, 0, sizeof(TType) * TTSize);
};

void clear_dp_tt(void)
{
  memset(DP_TTable, 0, sizeof(TType) * TTSize);
};

void initialize_zobrist(void)
{
  int p, q;
  
  seedMT(31657);
  
  for(p = 0; p < 17; p++)
  {
    for(q = 0; q < 144; q++)
      {
	zobrist[p][q] = randomMT();
      }
  }
  /* our magic number */

  hash = 0xDEADBEEF;
}

void initialize_hash(void)
{
  int p;
  
  hash = 0xDEADBEEF;
  
  for(p = 0; p < 144; p++)
    {
      if (board[p] != npiece && board[p] != frame)
	hash = hash ^ zobrist[board[p]][p];
    }

  hold_hash = 0xC0FFEE00;
  /* we need to set up hold_hash here, rely on ProcessHolding for now */

}

void QStoreTT(int score, int alpha, int beta, int best)
{
  uint32_t index;
  
  TTStores++;

  index = hash % TTSize;

  if (score <= alpha)     
    QS_TTable[index].Type = UPPER;
  else if(score >= beta) 
    QS_TTable[index].Type = LOWER;
  else                  
    QS_TTable[index].Type = EXACT;
  
  QS_TTable[index].Hash = hash;
  QS_TTable[index].Hold_hash = hold_hash;
  QS_TTable[index].Bestmove = best;
  QS_TTable[index].Bound = score;
  QS_TTable[index].OnMove = ToMove;
    
  return;
}

void StoreTT(int score, int alpha, int beta, int best, int threat, int depth)
{
  uint32_t index;
  
  TTStores++;

  index = hash % TTSize;

  /* Prefer storing entries with more information */
  if ((      (DP_TTable[index].Depth < depth) 
        ||  ((DP_TTable[index].Depth == depth) && 
	        (    ((DP_TTable[index].Type == UPPER) && (score > alpha))
		 ||  ((score > alpha) && (score < beta))
		)
	    )
      )
      && !is_pondering)
    {
      if (score <= alpha)  
      {
	DP_TTable[index].Type = UPPER;
	if (score < -INF+500) score = -INF+500;
      }
      else if(score >= beta) 
      {
	DP_TTable[index].Type = LOWER;
	if (score > INF-500) score = INF-500;
      }
      else                  
      {
	DP_TTable[index].Type = EXACT;
     
	/* normalize mate scores */
       if (score > (+INF-500))
	  score += ply;
        else if (score < (-INF+500))
	  score -= ply;
      }
      
      DP_TTable[index].Hash = hash;
      DP_TTable[index].Hold_hash = hold_hash;
      DP_TTable[index].Depth = depth;
      DP_TTable[index].Bestmove = best;
      DP_TTable[index].Bound = score;
      DP_TTable[index].OnMove = ToMove;
      DP_TTable[index].Threat = threat;
    }
  else 
    {
      if (score <= alpha)  
      {
	AS_TTable[index].Type = UPPER;
	if (score < -INF+500) score = -INF+500;
      }
      else if(score >= beta) 
      {
	AS_TTable[index].Type = LOWER;
	if (score > INF-500) score = INF-500;
      }
      else                  
      {
	AS_TTable[index].Type = EXACT;
     
	/* normalize mate scores */
       if (score > (+INF-500))
	  score += ply;
        else if (score < (-INF+500))
	  score -= ply;
      }
      
      AS_TTable[index].Hash = hash;
      AS_TTable[index].Hold_hash = hold_hash;
      AS_TTable[index].Depth = depth;
      AS_TTable[index].Bestmove = best;
      AS_TTable[index].Bound = score;
      AS_TTable[index].OnMove = ToMove;
      AS_TTable[index].Threat = threat;
    };
  
  return;
}

void LearnStoreTT(int score, unsigned nhash, unsigned hhash, int tomove, int best, int depth)
{
  uint32_t index;

  index = nhash % TTSize;

  AS_TTable[index].Depth = depth;
  
  if (Variant != Suicide && Variant != Losers)
  {
    AS_TTable[index].Type = EXACT;
  }
  else
  {
    AS_TTable[index].Type = UPPER;
  }
  
  AS_TTable[index].Hash = nhash;
  AS_TTable[index].Hold_hash = hhash;
  AS_TTable[index].Bestmove = best;
  AS_TTable[index].Bound = score;
  AS_TTable[index].OnMove = tomove;
  AS_TTable[index].Threat = 0;

}

int ProbeTT(int *score, int alpha, int beta, int *best, int *threat, int *donull, int depth)
{

  uint32_t index;

  *donull = TRUE;

  TTProbes++;

  index = hash % TTSize;
  
  if ((DP_TTable[index].Hash == hash) 
      && (DP_TTable[index].Hold_hash == hold_hash) 
      && (DP_TTable[index].OnMove == ToMove))
    {
      TTHits++;
      
      if ((DP_TTable[index].Type == UPPER) 
      	   && ((depth-2-1) <= DP_TTable[index].Depth) 
      	   && (DP_TTable[index].Bound < beta)) 
      	  *donull = FALSE;

      if (DP_TTable[index].Threat) depth++;
      
      if (DP_TTable[index].Depth >= depth)
	{
	  *score = DP_TTable[index].Bound;
	  
	  if (*score > (+INF-500))
	   *score -= ply;
	  else if (*score < (-INF+500))
	    *score += ply;

	  *best = DP_TTable[index].Bestmove;
	  *threat = DP_TTable[index].Threat;

	  return DP_TTable[index].Type;
	}
      else
	{
	  *best = DP_TTable[index].Bestmove;
	  *threat = DP_TTable[index].Threat;

	  return DUMMY;
	}
    }
  else if ((AS_TTable[index].Hash == hash) 
      && (AS_TTable[index].Hold_hash == hold_hash) 
      && (AS_TTable[index].OnMove == ToMove))
    {
      TTHits++;
      
      if ((AS_TTable[index].Type == UPPER) 
      	   && ((depth-2-1) <= AS_TTable[index].Depth) 
      	   && (AS_TTable[index].Bound < beta)) 
      	  *donull = FALSE;

      if (AS_TTable[index].Threat) depth++;
      
      if (AS_TTable[index].Depth >= depth)
	{
	  *score = AS_TTable[index].Bound;
	  
	  if (*score > (+INF-500))
	   *score -= ply;
	  else if (*score < (-INF+500))
	    *score += ply;

	  *best = AS_TTable[index].Bestmove;
	  *threat = AS_TTable[index].Threat;

	  return AS_TTable[index].Type;
	}
      else
	{
	  *best = AS_TTable[index].Bestmove;
	  *threat = AS_TTable[index].Threat;

	  return DUMMY;
	}
    }
  else
    return HMISS;

}

int QProbeTT(int *score, int alpha, int beta, int *best)
{

  uint32_t index;

  TTProbes++;

  index = hash % TTSize;
  
  if ((QS_TTable[index].Hash == hash) 
      && (QS_TTable[index].Hold_hash == hold_hash) 
      && (QS_TTable[index].OnMove == ToMove))
    {
      TTHits++;
      
      *score = QS_TTable[index].Bound;
      
      *best = QS_TTable[index].Bestmove;
      
      return QS_TTable[index].Type;
    }
  else
    return HMISS;

}


void alloc_hash(void)
{
  AS_TTable = (TType *) malloc(sizeof(TType) * TTSize);
  DP_TTable = (TType *) malloc(sizeof(TType) * TTSize);
  QS_TTable = (QTType *) malloc(sizeof(QTType) * TTSize);

  if (AS_TTable == NULL || DP_TTable == NULL || QS_TTable == NULL)
  {
    printf("Out of memory allocating hashtables.\n");
    exit(EXIT_FAILURE);
  }
  
  printf("Allocated 2*%d hash entries, totalling %u bytes.\n",
          TTSize, (uint32_t)(2*sizeof(TType)*TTSize));
  printf("Allocated %d quiescenthash entries, totalling %u bytes.\n",
          TTSize, (uint32_t)(sizeof(QTType)*TTSize));
  return; 
}

void free_hash(void)
{
  free(AS_TTable);
  free(DP_TTable);
  free(QS_TTable);
  return;
}
