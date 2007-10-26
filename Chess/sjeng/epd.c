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

    File: epd.c                                             
    Purpose: run EPD test suite

*/

#include "sjeng.h"
#include "protos.h"
#include "extvars.h"

void setup_epd_line(char* inbuff)
{
  int i = 0;
  int rankp = 0;   // a8
  int rankoffset = 0;
  int fileoffset = 0;
  int j;

  /* 0 : FEN data */
  /* 1 : Active color */
  /* 2 : Castling status */
  /* 3 : EP info */
  /* 4 : 50 move */
  /* 5 : movenumber */
  /* 6 : EPD data */
  int stage = 0;

  static int rankoffsets[] = {110, 98, 86, 74, 62, 50, 38, 26};
 
  /* conversion from algebraic to sjeng internal for ep squares */
  int converterf = (int) 'a';
  int converterr = (int) '1';
  int ep_file, ep_rank, norm_file, norm_rank;
  
  memset(board, frame, sizeof(board));
  
  white_castled = no_castle;
  black_castled = no_castle;

  book_ply = 50;

  rankoffset = rankoffsets[0];

  while (inbuff[i] == ' ') {i++;};

  while((inbuff[i] != '\n') && (inbuff[i] != '\0'))
    {
      if(stage == 0 && isdigit(inbuff[i]))
	{
	  for (j = 0; j < atoi(&inbuff[i]); j++)
	    board[rankoffset + j + fileoffset] = npiece;
	  
	  fileoffset += atoi(&inbuff[i]);
	}
      else if (stage == 0 && inbuff[i] == '/')
	{
	  rankp++;
	  rankoffset = rankoffsets[rankp];	
	  fileoffset = 0;
	}
      else if (stage == 0 && isalpha(inbuff[i]))
	{
	  switch (inbuff[i])
	    {
	    case 'p' : board[rankoffset + fileoffset] = bpawn; break;
	    case 'P' : board[rankoffset + fileoffset] = wpawn; break;	
	    case 'n' : board[rankoffset + fileoffset] = bknight; break;
	    case 'N' : board[rankoffset + fileoffset] = wknight; break;	
	    case 'b' : board[rankoffset + fileoffset] = bbishop; break;
	    case 'B' : board[rankoffset + fileoffset] = wbishop; break;	
	    case 'r' : board[rankoffset + fileoffset] = brook; break;
	    case 'R' : board[rankoffset + fileoffset] = wrook; break;	
	    case 'q' : board[rankoffset + fileoffset] = bqueen; break;
	    case 'Q' : board[rankoffset + fileoffset] = wqueen; break;	
	    case 'k' : 
	      bking_loc = rankoffset + fileoffset;
	      board[bking_loc] = bking; 
	      break;
	    case 'K' :
	      wking_loc = rankoffset + fileoffset;
	      board[wking_loc] = wking; 
	      break;	
	    }
	  fileoffset++;
	}
      else if (inbuff[i] == ' ')
	{
	  stage++;

	  if (stage == 1)
	    {
	      /* skip spaces */
	      while (inbuff[i] == ' ') i++;
	      
	      if (inbuff[i] == 'w') 
		white_to_move = 1;
	      else
		white_to_move = 0;
	    }
	  else if (stage == 2)
	    {
	      /* assume no castling at all */
	      moved[26] = moved[33] = moved[30] = 1;
	      moved[110] = moved[114] = moved[117] = 1;

	      while(inbuff[i] == ' ') i++;
	     
	      while (inbuff[i] != ' ')
		{
		  switch (inbuff[i])
		    {
		    case '-' :
		      break;
		    case 'K' :
		      moved[30] = moved[33] = 0;
		      break;
		    case 'Q' :
		      moved[30] = moved[26] = 0;
		      break;
		    case 'k' :
		      moved[114] = moved[117] = 0;
		      break;
		    case 'q' :
		      moved[114] = moved[110] = 0;
		      break;
		    }
		  i++;
		}
	      i--; /* go back to space so we move to next stage */
	      
	    }
	  else if (stage == 3)
	    {
	      /* skip spaces */
	      while (inbuff[i] == ' ') i++;
	      
	      if (inbuff[i] == '-')
		{
		  ep_square = 0;
		}
	      else
		{
		  ep_file = inbuff[i++];
		  ep_rank = inbuff[i++];
		  
		  norm_file = ep_file - converterf;
		  norm_rank = ep_rank - converterr;
		  
		  ep_square = ((norm_rank * 12) + 26) + (norm_file);		  
		}
	    }
	  else if (stage == 4)
	    {
	      /* ignore this for now */
	    }
	  else if (stage == 5)
	    {
	      /* ignore this for now */
	    }
	  else if (stage == 6)
	    {
	      /* ignore this for now */
	    }	  
	};
      
      i++;
    }

  reset_piece_square();
  initialize_hash();

}

int check_solution(char *inbuff, move_s cmove)
{
  char san[STR_BUFF];
  
  comp_to_san(cmove, san);
 
//  printf("Sjeng's move: %s, EPD line: %s\n", san, strstr(inbuff,"bm"));
  
  if (strstr(inbuff, "bm") != NULL)
    {
      if (strstr(inbuff, san) != NULL)
	return TRUE;
      else
	return FALSE;
    }
  else if (strstr(inbuff, "am") != NULL)
    {
      if (strstr(inbuff, san) != NULL)
	return FALSE;
      else
	return TRUE;
    }
  else
    printf("No best-move or avoid-move found!");

  return FALSE;
}

void run_epd_testsuite(void)
{
  FILE *testsuite;
  char readbuff[2000];
  char testname[FILENAME_MAX];
  char tempbuff[2000];
  float elapsed; 
  int nps;
  long thinktime;
	move_s comp_move;
  int tested, found;
	
  clock_t cpu_start, cpu_end;
  
  tested = 0;
  found = 0;
  
  printf("\nName of EPD testsuite: ");
  rinput(testname, STR_BUFF, stdin);
  printf("\nTime per move (s): ");
  rinput(readbuff, STR_BUFF, stdin);
  thinktime = atol(readbuff);
  printf("\n");

  thinktime *= 100;

  testsuite = fopen(testname, "r");

  while (fgets(readbuff, 2000, testsuite) != NULL)
    {
      tested++;

      setup_epd_line(readbuff);

      root_to_move = ToMove;
      
      clear_tt();
      initialize_hash();
      
      display_board(stdout, 1); 
 
      forcedwin = FALSE;    
    //  pn_time = thinktime;
    //  cpu_start = clock();
    //  proofnumbersearch();
    //  cpu_end = clock();
     // rdelay(2);
     
     elapsed = (cpu_end-cpu_start)/(double) CLOCKS_PER_SEC;
     printf("Time: %f\n", elapsed);
      
     if (interrupt()) rinput(tempbuff, STR_BUFF, stdin);
      
      fixed_time = thinktime;
      
       cpu_start = clock();
       comp_move = think();
       cpu_end = clock();
      

      printf ("\nNodes: %ld (%0.2f%% qnodes)\n", nodes,
	      (float) ((float) qnodes / (float) nodes * 100.0));
      
      elapsed = (cpu_end-cpu_start)/(float) CLOCKS_PER_SEC;
      nps = (int)((float) nodes/(float) elapsed);
      
      if (!elapsed)
	printf ("NPS: N/A\n");
      else
	printf ("NPS: %ld\n", (long int) nps);
      
      printf("ECacheProbes : %ld   ECacheHits : %ld   HitRate : %f%%\n", 
	     ECacheProbes, ECacheHits, 
	     ((float)ECacheHits/((float)ECacheProbes+1)) * 100);
      
      printf("TTStores : %ld TTProbes : %ld   TTHits : %ld   HitRate : %f%%\n", 
	     TTStores, TTProbes, TTHits, 
	     ((float)TTHits/((float)TTProbes+1)) * 100);
      
      printf("NTries : %d  NCuts : %d  CutRate : %f%%  TExt: %d\n", 
	     NTries, NCuts, (((float)NCuts*100)/((float)NTries+1)), TExt);
      
      printf("Check extensions: %ld  Razor drops : %ld  Razor Material : %ld\n", ext_check, razor_drop, razor_material);
      printf("EGTB Hits: %d  EGTB Probes: %d  Efficiency: %3.1f%%\n", EGTBHits, EGTBProbes,
             (((float)EGTBHits*100)/(float)(EGTBProbes+1)));		 
      
      printf("Move ordering : %f%%\n", (((float)FHF*100)/(float)FH+1));
      
      printf("Material score: %d   Eval : %ld\n", Material, eval());
      printf("\n");
     
      if (!forcedwin)
      {
      if(check_solution(readbuff, comp_move))
	{
	  found++;
	  printf("Solution found.\n");
	}
      else
	{
	  printf("Solution not found.\n");
	}
      }
      else
      {
	found++;
      }
      
      printf("Solved: %d/%d\n", found, tested);
      
    };
 
  printf("\n");
};

