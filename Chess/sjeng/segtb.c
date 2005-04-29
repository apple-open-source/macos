/*
    Sjeng - a chess variants playing program
    Copyright (C) 2001 Gian-Carlo Pascutto and Nubie

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

    File: segtb.c                                        
    Purpose: suicide endgame tablebases

*/

#include <limits.h>
#include "sjeng.h"
#include "extvars.h"
#include "protos.h"
#include "squares.h"

#define FILE(x)			   ((x) & 7)
#define RANK(x)			   ((x) >> 3)

#define TWO_PIECE_SIZE		   4096
#define TWO_PIECE_HASH(x,y,z)	   (((((x) << 5) | (y)) << 6) | (z))
#define TWO_PIECE_FILE		   "stb/2pieces.bin"

#define THREE_PIECE_SIZE	   (64*TWO_PIECE_SIZE)
#define THREE_PIECE_HASH(x,y,z,w)  (((((((x) << 5) | (y)) << 6) | (z)) << 6) | (w))
#define THREE_PIECE_FILE	   "stb/xxx.bin"

#define TABLE_KEY(x,y,z)	   (((((x) << 3) | (y)) << 3) | (z))

#define IO_BUFSIZE		    4096
#define CACHE_SIZE		    8

int upscale[64] = {
  A1,B1,C1,D1,E1,F1,G1,H1,
  A2,B2,C2,D2,E2,F2,G2,H2,
  A3,B3,C3,D3,E3,F3,G3,H3,
  A4,B4,C4,D4,E4,F4,G4,H4,
  A5,B5,C5,D5,E5,F5,G5,H5,
  A6,B6,C6,D6,E6,F6,G6,H6,
  A7,B7,C7,D7,E7,F7,G7,H7,
  A8,B8,C8,D8,E8,F8,G8,H8
};

int vertical_flip[64] = {
  7,  6,  5,  4,  3,  2,  1,  0,
  15, 14, 13, 12, 11, 10,  9,  8,
  23, 22, 21, 20, 19, 18, 17, 16,
  31, 30, 29, 28, 27, 26, 25, 24,
  39, 38, 37, 36, 35, 34, 33, 32,
  47, 46, 45, 44, 43, 42, 41, 40,
  55, 54, 53, 52, 51, 50, 49, 48,
  63, 62, 61, 60, 59, 58, 57, 56
};

/* angrim : this is 63-x, no need to lookup */

int rotate[64] = {
  63, 62, 61, 60, 59, 58, 57, 56,
  55, 54, 53, 52, 51, 50, 49, 48,
  47, 46, 45, 44, 43, 42, 41, 40,
  39, 38, 37, 36, 35, 34, 33, 32,
  31, 30, 29, 28, 27, 26, 25, 24,
  23, 22, 21, 20, 19, 18, 17, 16,
  15, 14, 13, 12, 11, 10,  9,  8,
  7,  6,  5,  4,  3,  2,  1,  0
};

int white_addr[64] = {
  0,  1,  2,  3, -1, -1, -1, -1,
  4,  5,  6,  7, -1, -1, -1, -1,
  8,  9, 10, 11, -1, -1, -1, -1,
  12, 13, 14, 15, -1, -1, -1, -1,
  16, 17, 18, 19, -1, -1, -1, -1,
  20, 21, 22, 23, -1, -1, -1, -1,
  24, 25, 26, 27, -1, -1, -1, -1,
  28, 29, 30, 31, -1, -1, -1, -1
};

int section_map[6][6] = {
  {  0,  1,  2,  3,  4,  5 },
  { -1,  6,  7,  8,  9, 10 },
  { -1, -1, 11, 12, 13, 14 },
  { -1, -1, -1, 15, 16, 17 },
  { -1, -1, -1, -1, 18, 19 },
  { -1, -1, -1, -1, -1, 20 }
};


int section_trans[] = {666, 0, 0, 1, 1, 5, 5, 3, 3, 4, 4, 2, 2, 6};
char xpiece_char[] = {'F','P','P','N','N','K','K','R','R','Q','Q','B','B','E' };

typedef struct 
{
  int table_key;
  int last_access;
} cache_data;

signed char *two_piece_data;
signed char *three_piece_data;
signed char *temp_table; /* used when generating new tables */

int cache_counter;
cache_data table_cache[CACHE_SIZE];
int temp_key;

int SEGTB;

int valid_2piece(int w, int b, int w_man, int b_man)
{
  /* white piece on the wrong half-board? */
  
  if(white_addr[w] == -1) 
    return 0;
  
  /* pieces on the same square? */
  if(w == b) 
    return 0;

  if(w_man == wpawn || w_man == bpawn)
    if(w < 8 || w > 55) 
      return 0; 
  
  if(b_man == bpawn || b_man == wpawn)
    if(b < 8 || b > 55) 
      return 0;
  
  return 1;
}


int valid_3piece(int w, int b1, int b2, int w_man, int b1_man, int b2_man)
{
  /* white piece on the wrong half-board? */
  if(white_addr[w] == -1) 
    return 0;

  /* pieces on the same square? */
  if(w == b1) 
    return 0;
  
  if(w == b2) 
    return 0;
  
  if(b1 == b2) 
    return 0;
  
  /* pawn on a bad rank? */
  if(w_man == wpawn || w_man == bpawn)
    if(w < 8 || w > 55) 
      return 0;
  
  if(b1_man == bpawn || b1_man == wpawn)
    if(b1 < 8 || b1 > 55) 
      return 0;
  
  if(b2_man == bpawn || b2_man == wpawn)
    if(b2 < 8 || b2 > 55) 
      return 0;
  
  return 1;
}

int check_result()
{
  int p, xp, res;
  int wp = 0, bp = 0;
  int a, j, i;
  move_s moves[MOVE_BUFF];
  int num_moves;
 
  for (j = 1, a = 1; (a <= piece_count); j++) 
    {
      i = pieces[j];
      
      if (!i)
	continue;
      else
	a++;
      
      switch (board[i])
	{
	case wpawn:
	case wbishop:
	case wrook:
	case wking:
	case wqueen:
	case wknight: wp++; break;
	case bpawn:
	case bbishop:
	case brook:
	case bking:
	case bqueen:
	case bknight: bp++; break;
	}
    }

  if (!(wp) && (bp))
    {
      if (white_to_move)
	{
	  return -127;
	}
      else
	{
	  return 126;
	}
    }
  else if ((!bp) && (wp))
    {
      if (white_to_move)
	{
	  return 126;
	}
      else
	{
	  return -127;
	}
    }
 
  
  gen(&moves[0]);
  num_moves = numb_moves;
  
  if(!num_moves) 
    {
      if (white_to_move)
	{
	  p = wp;
	  xp = bp;
	}
      else
	{
	  p = bp;
	  xp = wp;
	}
   
      if(p < xp) return -127;
      else if(xp < p) return 126; /* can't really happen */
      else return 0;

    }
  
  res = egtb(white_to_move);
  
  if(res > -10 && res < 10 && res) 
     printf("Warning: near zero values!\n");
  
  if(res > 0) 
    return -res+1;
  else if(res < 0 && res > -128) 
    return -res-1;
  
  return res;
}

void gen_2piece(int w_man, int b_man, signed char *table)
{
  int i, w, b, t, addr;
  signed char best, res;
  int f, unknown;
  move_s moves[MOVE_BUFF];
  int num_moves;
  
  ply = 1;
  reset_board();
  
  /* initialise the table */
  memset(table, -128, TWO_PIECE_SIZE);
      
  if (!(w_man & 1)) w_man--;
  if (b_man & 1) b_man++;
  
  do 
    {
      f = FALSE;
    
      for(t = 0; t < 2; t++) 
	{
	  white_to_move = t;
	  
	  for(w = 0; w < 64; w++)
	    {
	      for(b = 0; b < 64; b++) 
		{
		  if(!valid_2piece(w, b, w_man, b_man)) continue;
		  
		  addr = TWO_PIECE_HASH(t, white_addr[w], b);
		  if(table[addr] != -128) continue;
		  
		  board[upscale[w]] = w_man;
		  board[upscale[b]] = b_man;
		  pieces[1] = upscale[w];
		  pieces[2] = upscale[b];
		  squares[upscale[w]] = 1;
		  squares[upscale[b]] = 2;
		  piece_count = 2;
                  ep_square = 0;
		  
		  gen(&moves[0]);
                  num_moves = numb_moves;
		  
		  if(num_moves == 0) 
		    {
		      if (table[addr] != 0) f = TRUE;
		      table[addr] = 0;
		    } 
		  else 
		    {
		      best = -128;
		      unknown = FALSE;
		
		      for(i = 0; i < num_moves; i++) 
			{
			  make(&moves[0], i);
			  res = (signed char) check_result();
			  unmake(&moves[0], i);

			  
			  if(res == -128) 
			    {
			      unknown = TRUE;
			      continue;
			    }

			  if(res > best) best = res;
			}
		    
		      if(best > 0 || (best < 0 && !unknown)) 
			{
			  if (table[addr] != best) f = TRUE;
			  table[addr] = best;
			}
		    }
	  
		  board[upscale[w]] = npiece;
		  board[upscale[b]] = npiece;
		  squares[upscale[w]] = 0;
		  squares[upscale[b]] = 0;
		  pieces[1] = 0;
		  pieces[2] = 0;

		}
	    }
	}
    printf(".");     
    } 
  while(f);

  printf("\n");
  
  for(i = 0; i < TWO_PIECE_SIZE; i++)
    if(table[i] == -128) table[i] = 0;
  
}


void gen_3piece(int w_man, int b1_man, int b2_man, signed char *table)
{
  int i, w, b1, b2, t, addr;
  signed char best, res;
  int f, unknown;
  move_s moves[MOVE_BUFF];
  int num_moves;
  
  ply = 1;
  reset_board();

  /* initialise the table */
  memset(table, -128, THREE_PIECE_SIZE);
		      
  /* normalize colors if needed */
  if (!(w_man & 1)) w_man--;
  if (b1_man & 1) b1_man++;
  if (b2_man & 1) b2_man++;

  do 
    {
      f = FALSE;
      
      for(t = 0; t < 2; t++) 
	{
	  white_to_move = t;

	  for(w = 0; w < 64; w++)
	    {
	      for(b1 = 0;  b1 < 64; b1++)
		{
		  for(b2 = 0; b2 < 64; b2++) 
		    {
		      
		      if(!valid_3piece(w, b1, b2, w_man, b1_man, b2_man)) continue;
		      
		      addr = THREE_PIECE_HASH(t, white_addr[w], b1, b2);
		     // if(table[addr] != -128) continue;
		      
		      board[upscale[w]] = w_man;
		      board[upscale[b1]] = b1_man;
		      board[upscale[b2]] = b2_man;
                      piece_count = 3;
		      pieces[1] = upscale[w];
		      pieces[2] = upscale[b1];
		      pieces[3] = upscale[b2];
		      squares[upscale[w]] = 1;
		      squares[upscale[b1]] = 2;
		      squares[upscale[b2]] = 3;
		      ep_square = 0;
		      
		      gen(&moves[0]);
                      num_moves = numb_moves;
		      
		      if(!num_moves) 
			{
			   if (table[addr] != (white_to_move ? 126 : -127)) 
			    	f = TRUE;
			  table[addr] = (white_to_move ? 126 : -127);
			} 
		      else 
			{
			  best = -128;
			  unknown = FALSE;
			  
			  for(i = 0; i < num_moves; i++) 
			    {
			      make(&moves[0], i);
			      res = (signed char) check_result();
			      unmake(&moves[0], i);
			
			      if(res == -128) 
				{
				  unknown = TRUE;
				  continue;
				}
			      
			      if(res > best) best = res;
			    }
			  
			  if(best > 0 || (best < 0 && !unknown)) 
			    {
			      if (table[addr] != best) f = TRUE;
			      table[addr] = best;
			    }
			}
		     
		      board[upscale[w]] = npiece;
		      board[upscale[b1]] = npiece;
		      board[upscale[b2]] = npiece;
		      squares[upscale[w]] = 0;
		      squares[upscale[b1]] = 0;
		      squares[upscale[b2]] = 0;
		      pieces[1] = 0;
		      pieces[2] = 0;
		      pieces[3] = 0;
		    }
		}
	    }
	}      
	printf(".");
    } 
  while(f);
  
  printf("\n");
  
  for(i = 0; i < THREE_PIECE_SIZE; i++)
    if(table[i] == -128) table[i] = 0;
  
}

int save_2piece()
{
  int i, j;
  FILE *f;
  signed char *table;
  
  if(!(f = fopen(TWO_PIECE_FILE, "w"))) return 0;
  
  for(i = 0; i < 21; i++) 
    {
    table = two_piece_data + i * TWO_PIECE_SIZE;
    for(j = 0 ; j < TWO_PIECE_SIZE; j++) 
      {
	fputc(table[j], f);
      }
    }
  
  fclose(f);
  return 1;
}


int save_3piece(int w1_man, int b1_man, int b2_man, signed char *t)
{
  FILE *f;
  signed char fname[13];
  signed char *buf;
  int i;
  
  /* generate the filename */
  
  strcpy(fname, THREE_PIECE_FILE);
  fname[4] = xpiece_char[w1_man];
  fname[5] = xpiece_char[b1_man];
  fname[6] = xpiece_char[b2_man];
  
  if(!(f = fopen(fname,"w"))) return 0;
  
  for(i = 0; i < THREE_PIECE_SIZE; i += IO_BUFSIZE) 
    {
      buf = t + i;
     
      if(!fwrite(buf, IO_BUFSIZE, 1, f)) 
	{
	  printf("Error writing %s\n",fname);
	  fclose(f);
	  return 0;
	}
    }
  
  fclose(f);
  return 1;
}


void gen_all_tables()
{
  int w_man, b1_man, b2_man;
  signed char *base_addr;
  
  printf("Two-piece tables:\n");
  
  /* first generate pawnless tables */
  for(w_man = wknight; w_man <= wbishop; w_man += 2)
    {
      for(b1_man = bknight; b1_man <= bbishop; b1_man += 2) 
	{
	  if(section_map[section_trans[w_man]][section_trans[b1_man]] == -1) continue;

	  printf("Generating %c vs %c ",
		 xpiece_char[w_man], xpiece_char[b1_man]);

	  base_addr = two_piece_data 
	    + (section_map[section_trans[w_man]][section_trans[b1_man]] * TWO_PIECE_SIZE);

	  gen_2piece(w_man, b1_man, base_addr);
	}
    }
  
  /* pawns are the tricky ones that can be promoted,
     that's why we need the other tables to exist */
  w_man = wpawn;
  for(b1_man = bknight; b1_man <= bbishop; b1_man += 2) 
    {
      if(section_map[section_trans[w_man]][section_trans[b1_man]] == -1) continue;
      printf("Generating %c vs %c ",
	     xpiece_char[w_man], xpiece_char[b1_man]);
      
      base_addr = two_piece_data 
	+ (section_map[section_trans[w_man]][section_trans[b1_man]] * TWO_PIECE_SIZE);
      
      gen_2piece(w_man, b1_man, base_addr);
    }
  
  /* finally, pawn vs pawn */
  printf("Generating %c vs %c ",
	 xpiece_char[wpawn], xpiece_char[bpawn]);
  
  base_addr = two_piece_data 
    + (section_map[section_trans[wpawn]][section_trans[bpawn]]*TWO_PIECE_SIZE);
  
  gen_2piece(wpawn, bpawn, base_addr);
  
  if(save_2piece())
    printf("Saved two-piece tables in %s\n", TWO_PIECE_FILE);
  
  /* now, start doing pawnless three-piece tables */
  
  temp_table = malloc(THREE_PIECE_SIZE);

  if(!temp_table) return;
  
  for(w_man = wknight; w_man <= wbishop; w_man += 2)
    for(b1_man = bknight; b1_man <= bbishop ; b1_man += 2)
      for(b2_man = bknight; b2_man <= bbishop; b2_man += 2) 
	{
	  if(section_map[section_trans[b1_man]][section_trans[b2_man]] == -1) continue;

	  printf("Generating %c vs %c+%c ", xpiece_char[w_man],
		 xpiece_char[b1_man], xpiece_char[b2_man]);
	  
	  temp_key = TABLE_KEY(section_trans[w_man], 
	      	               section_trans[b1_man],
			       section_trans[b2_man]);
	  gen_3piece(w_man, b1_man, b2_man, temp_table);
	  save_3piece(w_man, b1_man, b2_man, temp_table);
	}
  
  
  /* white piece is a pawn */
  
  w_man = wpawn;
  for(b1_man = bknight; b1_man <= bbishop; b1_man += 2)
    for(b2_man = bknight; b2_man <= bbishop; b2_man += 2) 
      {
	if(section_map[section_trans[b1_man]][section_trans[b2_man]] == -1) continue;

	printf("Generating %c vs %c+%c ", xpiece_char[w_man],
	       xpiece_char[b1_man], xpiece_char[b2_man]);
	
	temp_key = TABLE_KEY(section_trans[w_man],
	                     section_trans[b1_man],
			     section_trans[b2_man]);
	gen_3piece(w_man, b1_man, b2_man, temp_table);
	save_3piece(w_man, b1_man, b2_man, temp_table);
      }
  
  /* one black piece is a pawn */
  
  for(w_man = wknight; w_man <= wbishop; w_man += 2) 
  {
    b1_man = bpawn;

    for(b2_man = wknight; b2_man <= bbishop; b2_man += 2) 
    {
      printf("Generating %c vs %c+%c ", xpiece_char[w_man],
	     xpiece_char[b1_man], xpiece_char[b2_man]);
      
      temp_key = TABLE_KEY(section_trans[w_man],
	                   section_trans[b1_man],
			   section_trans[b2_man]);
      gen_3piece(w_man, b1_man, b2_man, temp_table);
      save_3piece(w_man, b1_man, b2_man, temp_table);
    }
  }
  
  w_man = wpawn;
  b1_man = bpawn;
  for(b2_man = bknight; b2_man <= bbishop; b2_man += 2) 
    {
      printf("Generating %c vs %c+%c ", xpiece_char[w_man],
	     xpiece_char[b1_man], xpiece_char[b2_man]);
      
      temp_key = TABLE_KEY(section_trans[w_man],
	                   section_trans[b1_man],
			   section_trans[b2_man]);
      gen_3piece(w_man, b1_man, b2_man, temp_table);
      save_3piece(w_man, b1_man, b2_man, temp_table);
    }
  
  /* both black pieces are pawns */
  
  for(w_man = wknight; w_man <= wbishop; w_man += 2) 
    {
      b1_man = bpawn;
      b2_man = bpawn;
    
      printf("Generating %c vs %c+%c ", xpiece_char[w_man],
	     xpiece_char[b1_man], xpiece_char[b2_man]);
      
      temp_key = TABLE_KEY(section_trans[w_man],
	                   section_trans[b1_man],
			   section_trans[b2_man]);
      gen_3piece(w_man, b1_man, b2_man, temp_table);
      save_3piece(w_man, b1_man, b2_man, temp_table);
  }
  
  /* three pawns */
  
  w_man = wpawn;
  b1_man = bpawn;
  b2_man = bpawn;
  
  printf("Generating %c vs %c+%c ", xpiece_char[w_man],
	 xpiece_char[b1_man], xpiece_char[b2_man]);
  
  temp_key = TABLE_KEY(section_trans[w_man],
                       section_trans[b1_man],
		       section_trans[b2_man]);
  gen_3piece(w_man, b1_man, b2_man, temp_table);
  save_3piece(w_man, b1_man, b2_man, temp_table);
  
  
  printf("\nAll done.\n");
  free(temp_table);
	
  reset_board();
}


void free_egtb()
{
  free(two_piece_data);
  free(three_piece_data);
}


int init_segtb()
{
  int i;
  
  two_piece_data = malloc(21 * TWO_PIECE_SIZE);
  three_piece_data = malloc(CACHE_SIZE * THREE_PIECE_SIZE);
  
  if(!two_piece_data || !three_piece_data) 
    {
       return FALSE;
    }
  
  if(!load_2piece()) 
    {
       return FALSE;
    }
  
  for(i = 0; i < CACHE_SIZE; i++) 
    {
      table_cache[i].table_key = -1;
      table_cache[i].last_access = 0;
    }
  
  cache_counter = 0;
  temp_key = -1;
  
  printf("Two-piece suicide endgame tables preloaded (using %d kB of memory)\n", 
	 (21 * TWO_PIECE_SIZE) / 1024);
  printf("Three-piece suicide endgame table cache %d kB\n", 
	 (CACHE_SIZE * THREE_PIECE_SIZE) / 1024);
  
  return TRUE;
}


int egtb(int s)
{
  int w1_man, w2_man, b1_man, b2_man;
  int w1, w2, b1, b2;
  int w_count = 0, b_count = 0;
  int i, t, temp, junk, bpc;
  signed char *table;

  /* first figure out what kind of position we have */
  
  for(i = 0; i < 64; i++) 
    {
      bpc = board[upscale[i]];
      
      if (bpc == npiece) continue;
      
      if(bpc & 1) 
	{
	  if(!w_count) 
	    {
	      w1_man = bpc;
	      w1 = i;
	    } 
	  else 
	    {
	      w2_man = bpc;
	      w2 = i;
	    }
	  w_count++;
	} 
      else 
	{
	  if(!b_count) 
	    {
	      b1_man = bpc;
	      b1 = i;
	    } 
	  else 
	    {
	      b2_man = bpc;
	      b2 = i;
	    }
	  b_count++;
	}
    }
 
  /* two pieces? */
  
  if(w_count == 1 && b_count == 1) {
    
    if(section_map[section_trans[w1_man]][section_trans[b1_man]] == -1) 
      {
	temp = b1_man;
	b1_man = w1_man;
	w1_man = temp;
	
	temp = b1;

/* angrim: can do this without branch */
	
	if(w1_man == wpawn || w1_man == bpawn) 
	  {
		/* pawn color was changed, we need to
		   rotate the board 180 degrees */
	    b1 = rotate[w1];
	    w1 = rotate[temp];
	  } 
	else 
	  {
	    b1 = w1;
	    w1 = temp;
	  }
	
	s ^= 1;
      }
    
    table = two_piece_data 
      + (section_map[section_trans[w1_man]][section_trans[b1_man]] * TWO_PIECE_SIZE);
    
    /* flip the board if necessary */
    if(white_addr[w1] == -1) 
      {
	w1 = vertical_flip[w1];
	b1 = vertical_flip[b1];
      }
    
    return (int)table[TWO_PIECE_HASH(s, white_addr[w1], b1)];
    
    
  } 
  else if((w_count == 1 && b_count == 2) ||
	    (w_count == 2 && b_count == 1)) 
    { /* three pieces */
      
      if(w_count > 1) 
	{ /* need to switch sides */
	  
	  b2_man = w2_man;
	  temp = w1_man;
	  w1_man = b1_man;
	  b1_man = temp;
	  
	  temp = w1;
	  
	  if(w1_man == wpawn || w1_man == bpawn || 
	     b1_man == bpawn || b1_man == wpawn || 
	     b2_man == bpawn || b2_man == wpawn) 
	    {
	      /* pawn color was changed, we need to
		 rotate the board 180 degrees */
	      b2 = rotate[w2];
	      w1 = rotate[b1];
	      b1 = rotate[temp];
	    } 
	  else 
	    {
	      b2 = w2;
	      w1 = b1;
	      b1 = temp;
	    }
	  
	  s ^= 1;
	}
      
      /* swap black pieces if necessary */
      
      if(section_map[section_trans[b1_man]][section_trans[b2_man]] == -1) 
	{
	
	  temp = b1_man;
	  b1_man = b2_man;
	  b2_man = temp;
      
	  temp = b1;
	  b1 = b2;
	  b2 = temp;
       }
      
      /* do the vertical flip */
      
      if(white_addr[w1] == -1) 
	{
	  w1 = vertical_flip[w1];
	  b1 = vertical_flip[b1];
	  b2 = vertical_flip[b2];
	}
      
    
      /* finally, find the table we need either
	 from memory or disk and return the value */

      t = TABLE_KEY(section_trans[w1_man],
	            section_trans[b1_man],
		    section_trans[b2_man]);
      
      if(temp_key == t) /* maybe we're generating a table */
	table = temp_table;
      else 
	{
	  /* check the cache */
      
	  temp = INT_MAX;
	  cache_counter++;
	  
	  for(i = 0; i < CACHE_SIZE; i++)
	    {
	      if(table_cache[i].table_key == t) 
		{
		  table = three_piece_data + (i * THREE_PIECE_SIZE);
		  table_cache[i].last_access = cache_counter;
		  return (int) table[THREE_PIECE_HASH(s, white_addr[w1], b1, b2)];
		} 
	      else if(table_cache[i].last_access < temp) 
		{
		  temp = table_cache[i].last_access;
		  junk = i;
		}
	    }      
	  /* the table was not in the cache, we'll load
	     it into the least recently accessed slot */
	  
	  table = three_piece_data + (junk * THREE_PIECE_SIZE);
	  
	  if(!load_3piece(w1_man, b1_man, b2_man, table)) 
	    {
	      //printf("Loading error\n");
	      table_cache[junk].table_key = -1;
	      return (-128); 
	    }
	  table_cache[junk].table_key = t;
	  table_cache[junk].last_access = cache_counter;
	}
    
      return (int) table[THREE_PIECE_HASH(s, white_addr[w1], b1, b2)];
    
    } 
  else 
    return (-128); /* position not in the tables */
  
}

int load_2piece()
{
  int i,j;
  FILE *f;
  signed char *table;
  
  if(!(f = fopen(TWO_PIECE_FILE, "r"))) return 0;
  
  for(i = 0; i < 21; i++) 
    {
      table = two_piece_data + i * TWO_PIECE_SIZE;
      
      for(j = 0; j < TWO_PIECE_SIZE; j++)
	table[j] = (signed char) fgetc(f);
    }
  
  fclose(f);
  return 1;
}

int load_3piece(int w1_man, int b1_man, int b2_man, signed char *t)
{
  FILE *f;
  signed char fname[13];
  signed char *buf;
  int i;

  /* generate the filename */
  
  strcpy(fname, THREE_PIECE_FILE);
  fname[4]= xpiece_char[w1_man];
  fname[5]= xpiece_char[b1_man];
  fname[6]= xpiece_char[b2_man];
  
  if(!(f = fopen(fname,"r"))) return 0;
  
  for(i = 0; i < THREE_PIECE_SIZE; i += IO_BUFSIZE) 
    {
      buf = t + i;
      if(!fread(buf, IO_BUFSIZE, 1, f)) {
	printf("Error reading %s\n",fname);
	fclose(f);
	return 0;
      }
    }
  
  fclose(f);
  return 1;
}


