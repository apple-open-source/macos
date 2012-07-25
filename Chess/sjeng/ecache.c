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

    File: ecache.c                                             
    Purpose: handling of the evaluation cache

*/

#include "sjeng.h"
#include "protos.h"
#include "extvars.h"

typedef struct  
{
uint32_t stored_hash;
uint32_t hold_hash;
int32_t score;
} ECacheType;

/*ECacheType ECache[ECACHESIZE];*/
ECacheType *ECache;

uint32_t ECacheProbes;
uint32_t ECacheHits;

void storeECache(int32_t score)
{
  int index;

  index = hash % ECacheSize;

  ECache[index].stored_hash = hash;
  ECache[index].hold_hash = hold_hash;
  ECache[index].score = score;
}

void checkECache(int32_t *score, int *in_cache)
{
  int index;

  ECacheProbes++;

  index = hash % ECacheSize;

  if(ECache[index].stored_hash == hash &&
	  ECache[index].hold_hash == hold_hash)
    
    {
      ECacheHits++;  

      *in_cache = 1;
      *score = ECache[index].score;
    }
}

void reset_ecache(void)
{
  memset(ECache, 0, sizeof(ECacheType)*ECacheSize);
  return;
}

void alloc_ecache(void)
{
  ECache = (ECacheType*)malloc(sizeof(ECacheType)*ECacheSize);

  if (ECache == NULL)
  {
    printf("Out of memory allocating ECache.\n");
    exit(EXIT_FAILURE);
  }
  
  printf("Allocated %u eval cache entries, totalling %u bytes.\n",
		 (uint32_t)ECacheSize, (uint32_t)(sizeof(ECacheType)*ECacheSize));
  return;
}

void free_ecache(void)
{
  free(ECache);
  return;
}
