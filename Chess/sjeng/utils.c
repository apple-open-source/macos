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

    File: utils.c                                   
    Purpose: misc. functions used throughout the program

*/

#include "config.h"
#include "sjeng.h"
#include "extvars.h"
#include "protos.h"

#include "limits.h" 
#ifdef HAVE_SELECT
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
fd_set read_fds;
struct timeval timeout = { 0, 0 };
#else
#ifdef _WIN32
#undef frame
#include <windows.h>
#include <time.h>
#define frame 0
#endif
#endif

/* Random number generator stuff */

#define N              (624)                 
#define M              (397)                 
#define K              (0x9908B0DFU)         
#define hiBit(u)       ((u) & 0x80000000U)   
#define loBit(u)       ((u) & 0x00000001U)   
#define loBits(u)      ((u) & 0x7FFFFFFFU)   
#define mixBits(u, v)  (hiBit(u)|loBits(v))  

static uint32_t   state[N+1];     
static uint32_t   *next;          
int                    left = -1;      

int32_t allocate_time (void) {

  /* calculate the ammount of time the program can use in its search, measured
     in centi-seconds (calculate everything in float for more accuracy as
     we go, and return the result as a int32_t) */

  float allocated_time = 0.0f, move_speed = 20.0f;

  /* sudden death time allocation: */
  if (!moves_to_tc) {
    /* calculate move speed.  The idea is that if we are behind, we move
       faster, and if we have < 1 min left and a small increment, we REALLY
       need to start moving fast.  Also, if we aren't in a super fast
       game, don't worry about being behind on the clock at the beginning,
       because some players will make instant moves in the opening, and Sjeng
       will play poorly if it tries to do the same. */

    /* check to see if we're behind on time and need to speed up: */
    if ((min_per_game < 6 && !inc) 
	|| time_left < (((min_per_game*6000) + (sec_per_game*100))*4.0/5.0)) 
    {
      if ((opp_time-time_left) > (opp_time/5.0) && xb_mode)
	move_speed = 40.0f;
      else if ((opp_time-time_left) > (opp_time/10.0) && xb_mode)
	move_speed = 30.0f;
      else if ((opp_time-time_left) > (opp_time/20.0) && xb_mode)
	move_speed = 25.0f;
    }
   
    if ((Variant != Suicide) && (Variant != Losers))
    {
    	if ((time_left-opp_time) > (time_left/5.0) && xb_mode)
        	move_speed -= 10;
    	else if ((time_left-opp_time) > (time_left/10.0) && xb_mode)
        	move_speed -= 5;
    }    
    else if (Variant == Suicide)
    {
	move_speed -= 10;
    }
    else if (Variant == Losers)
    {
	move_speed -= 5;
    }

    /* allocate our base time: */
    allocated_time = time_left/move_speed;

    /* add our increment if applicable: */
    if (inc) {
      if (time_left-allocated_time-inc > 500) {
        allocated_time += inc;
      }
      else if (time_left-allocated_time-(inc*2.0/3.0) > 100) {
        allocated_time += inc*2.0f/3.0f;
       }
     }
  }
  
  /* conventional clock time allocation: */
  else {
    allocated_time = (((float)min_per_game * 6000.0f
	    + (float)sec_per_game * 100.0f)/(float)moves_to_tc) - 100.0f;
    
    /* if we've got extra time, use some of it: */
    if (time_cushion) {
      allocated_time += time_cushion*2.1f/3.0f;
      time_cushion -= time_cushion*2.1f/3.0f;
    }
  }

  if (Variant == Bughouse)
  {
	allocated_time *= 1.0f/4.0f;

	if ((opp_time > time_left) || (opp_time < 1500))
	{
	  /* behind on time or blitzing out */
	  allocated_time *= 1.0f/2.0f;
	}
  }
 
  return ((int32_t) allocated_time);

}

void comp_to_san (move_s move, char str[])
{
  move_s moves[MOVE_BUFF];
  move_s evade_moves[MOVE_BUFF];
  char type_to_char[] = { 'F', 'P', 'P', 'N', 'N', 'K', 'K', 'R', 'R', 'Q', 'Q', 'B', 'B', 'E' };
  int i, num_moves, evasions, ambig, mate;
  int f_rank, t_rank, converter;
  char f_file, t_file;
  int ic;

  //eps = ep_square;
  
  f_rank = rank (move.from);
  t_rank = rank (move.target);
  converter = (int) 'a';
  f_file = file (move.from)+converter-1;
  t_file = file (move.target)+converter-1;
  
  if (move.from == 0)
    {
      sprintf (str, "%c@%c%d", type_to_char[move.promoted], t_file, t_rank);
    }
  else if ((board[move.from] == wpawn) || (board[move.from] == bpawn))
    { 
      if (board[move.target] == npiece && !move.ep)
	{
	  if(!move.promoted)
	    {
	      sprintf (str, "%c%d", t_file, t_rank);
	    }
	  else
	    {
	      sprintf (str, "%c%d=%c", t_file, t_rank, type_to_char[move.promoted]);
	    }
	}
      else
	{
	  if (!move.promoted)
	    {
	      sprintf (str, "%cx%c%d", f_file, t_file, t_rank);
	    }
	  else
	    {
	      sprintf (str, "%cx%c%d=%c", f_file, t_file, t_rank, 
		       type_to_char[move.promoted]);
	    }
	}
    }
  else if (move.castled != no_castle)
    {
      if (move.castled == wck || move.castled == bck)
	{
	  sprintf (str, "O-O");
	}
      else
	{
	  sprintf(str, "O-O-O");
	}
    }
  else
    {
      ambig = -1;
      num_moves = 0;
      
      gen(&moves[0]);
      num_moves = numb_moves;
      
      ic = in_check();
      
      /* check whether there is another, identical piece that
	 could also move to this square */
      for(i = 0; i < num_moves; i++)
	{
	  if ((moves[i].target == move.target) &&
	      (board[moves[i].from] == board[move.from]) &&
	      (moves[i].from != move.from))
	    {
	      /* would it be a legal move ? */
	      make(&moves[0], i);
	      if (check_legal(&moves[0], i, ic))
		{
		  unmake(&moves[0], i);
		  ambig = i;
		  break;
		}
	      unmake(&moves[0], i);
	    }
	}
      
      if (ambig != -1)
	{
	  
	  if (board[move.target] == npiece)
	    {
	      if (file(moves[ambig].from) != file(move.from))
		sprintf(str, "%c%c%c%d", type_to_char[board[move.from]],
			f_file, t_file, t_rank);
	      else
		sprintf(str, "%c%d%c%d", type_to_char[board[move.from]],
			f_rank, t_file, t_rank);
	    }
	  else
	    {
	      if (file(moves[ambig].from) != file(move.from))
		sprintf(str, "%c%cx%c%d", type_to_char[board[move.from]],
			f_file, t_file, t_rank);
	      else
		sprintf(str, "%c%dx%c%d", type_to_char[board[move.from]],
			f_rank, t_file, t_rank);
	    }
	}
      else
	{
	  if (board[move.target] == npiece)
	    {
	      sprintf(str, "%c%c%d", type_to_char[board[move.from]],
		      t_file, t_rank);
	    }
	  else
	    {
	      sprintf(str, "%cx%c%d", type_to_char[board[move.from]],
		      t_file, t_rank);
	    }
	}
    }
  
  //ep_square = eps;
  
  make(&move, 0);

  if (!check_legal(&move, 0, 1))
  {
    strcpy(str, "illg");
    unmake(&move, 0);
    return;
  }
  
  if (in_check())
    {
      mate = TRUE;
      evasions = 0;
      gen(&evade_moves[0]); 
      evasions = numb_moves;
      
      for (i = 0; i < evasions; i++)
	{
	  make(&evade_moves[0], i);
	  if (check_legal(&evade_moves[0], i, TRUE))
	    {
	      mate = FALSE;
	      unmake(&evade_moves[0], i);
	      break;
	    }
	  unmake(&evade_moves[0], i);
	}
      if (mate == TRUE)
	strcat(str, "#");
      else
	strcat(str, "+");
    }
  unmake(&move, 0);
  
}

void comp_to_coord (move_s move, char str[]) {

  /* convert a move_s internal format move to coordinate notation: */

  int prom, from, target, f_rank, t_rank, converter;
  char f_file, t_file;

  char type_to_char[] = { 'F', 'P', 'p', 'N', 'n', 'K', 'k', 'R', 'r', 'Q', 'q', 'B', 'b', 'E' };

  prom = move.promoted;
  from = move.from;
  target = move.target;
  
  f_rank = rank (from);
  t_rank = rank (target);
  converter = (int) 'a';
  f_file = file (from)+converter-1;
  t_file = file (target)+converter-1;


  if (from == 0)
    {
      sprintf (str, "%c@%c%d", type_to_char[prom], t_file, t_rank);
    }
  else
    {
      /* "normal" move: */
      if (!prom) {
	sprintf (str, "%c%d%c%d", f_file, f_rank, t_file, t_rank);
      }
      
      /* promotion move: */
      else {
	if (prom == wknight || prom == bknight) {
	  sprintf (str, "%c%d%c%dn", f_file, f_rank, t_file, t_rank);
	}
	else if (prom == wrook || prom == brook) {
	  sprintf (str, "%c%d%c%dr", f_file, f_rank, t_file, t_rank);
	}
	else if (prom == wbishop || prom == bbishop) {
	  sprintf (str, "%c%d%c%db", f_file, f_rank, t_file, t_rank);
	}
	else if (prom == wking || prom == bking) 
	{
	  sprintf (str, "%c%d%c%dk", f_file, f_rank, t_file, t_rank);
	}
	else
	{
	  sprintf (str, "%c%d%c%dq", f_file, f_rank, t_file, t_rank);
	}
      }
    }
}


void display_board (FILE *stream, int color) {

  /* prints a text-based representation of the board: */
  
  char *line_sep = "+----+----+----+----+----+----+----+----+";
  char *piece_rep[14] = {"!!", " P", "*P", " N", "*N", " K", "*K", " R",
			  "*R", " Q", "*Q", " B", "*B", "  "};
  int a,b,c;

  if (color % 2) {
    fprintf (stream, "  %s\n", line_sep);
    for (a = 1; a <= 8; a++) {
      fprintf (stream, "%d |", 9 - a);
      for (b = 0; b <= 11; b++) {
	c = 120 - a*12 + b;
	if (board[c] != 0)
	  fprintf (stream, " %s |", piece_rep[board[c]]);
      }
      fprintf (stream, "\n  %s\n", line_sep);
    }
    fprintf (stream, "\n     a    b    c    d    e    f    g    h\n\n");
  }

  else {
    fprintf (stream, "  %s\n", line_sep);
    for (a = 1; a <= 8; a++) {
      fprintf (stream, "%d |", a);
      for (b = 0; b <= 11; b++) {
	c = 24 + a*12 -b;
	if (board[c] != 0)
	  fprintf (stream, " %s |", piece_rep[board[c]]);
      }
      fprintf (stream, "\n  %s\n", line_sep);
    }
    fprintf (stream, "\n     h    g    f    e    d    c    b    a\n\n");
  }

}

void init_game (void) {

  /* set up a new game: */

  int init_board[144] = {
  0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,7,3,11,9,5,11,3,7,0,0,
  0,0,1,1,1,1,1,1,1,1,0,0,
  0,0,13,13,13,13,13,13,13,13,0,0,
  0,0,13,13,13,13,13,13,13,13,0,0,
  0,0,13,13,13,13,13,13,13,13,0,0,
  0,0,13,13,13,13,13,13,13,13,0,0,
  0,0,2,2,2,2,2,2,2,2,0,0,
  0,0,8,4,12,10,6,12,4,8,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0
  };

  memcpy (board, init_board, sizeof (init_board));
  memset (moved, 0, sizeof(moved));
  
  white_to_move = 1;
  ep_square = 0;
  wking_loc = 30;
  bking_loc = 114;
  white_castled = no_castle;
  black_castled = no_castle;

  result = no_result;
  captures = FALSE;

  piece_count = 32;

  Material = 0;

  memset(is_promoted, 0, sizeof(is_promoted));
  memset(holding, 0, sizeof(holding));

  white_hand_eval = 0;
  black_hand_eval = 0;

  reset_piece_square ();
  
  bookidx = 0;
  book_ply = 0;
  fifty = 0;
  ply = 0;
  
  phase = Opening;
}


bool is_move (char str[]) {

  /* check to see if the input string is a move or not.  Returns true if it
     is in a move format supported by Sjeng. */

  if (isalpha (str[0]) && isdigit (str[1]) && isalpha (str[2])
      && isdigit (str[3])) {
    return TRUE;
  }
  else if (isalpha(str[0]) && str[1] == '@' && isalpha(str[2]) && isdigit(str[3]))
    {
      return TRUE;
    }
  else {
    return FALSE;
  }

}


void perft_debug (void) {

  /* A function to debug the move gen by doing perft's, showing the board, and
     accepting move input */

  char input[STR_BUFF], *p;
  move_s move;
  int depth;

  init_game ();

  /* go into a loop of doing a perft(), then making the moves the user inputs
     until the user enters "exit" or "quit" */
  while (TRUE) {
    /* get the desired depth to generate to: */
    printf ("\n\nPlease enter the desired depth for perft():\n");
    rinput (input, STR_BUFF, stdin);
    depth = atoi (input);

    /* print out the number of raw nodes for this depth: */
    raw_nodes = 0;
    perft (depth);
    printf ("\n\nRaw nodes for depth %d: %d\n\n", depth, raw_nodes);

    /* print out the board: */
    display_board (stdout, 1);

    printf ("\nPlease input a move/command:\n");
    rinput (input, STR_BUFF, stdin);

    /* check to see if we have an exit/quit: */
    for (p = input; *p; p++) *p = tolower (*p);
    if (!strcmp (input, "exit") || !strcmp (input, "quit")) {
      exit (EXIT_SUCCESS);
    }

    if (!verify_coord (input, &move)) {
      /* loop until we get a legal move or an exit/quit: */
      do {
	printf ("\nIllegal move/command!  Please input a new move/command:\n");
	rinput (input, STR_BUFF, stdin);

	/* check to see if we have an exit/quit: */
	for (p = input; *p; p++) *p = tolower (*p);
	if (!strcmp (input, "exit") || !strcmp (input, "quit")) {
	  exit (EXIT_SUCCESS);
	}
      } while (!verify_coord (input, &move));
    }

    make (&move, 0);
  }
}

void hash_extract_pv(int level, char str[])
{
  int dummy, bm;
  move_s moves[MOVE_BUFF];
  int num_moves;
  char output[STR_BUFF];
  
  /* avoid loop on repetitions */
  level--;
  if (!level) return;
  
  if(ProbeTT(&dummy, 0, 0, &bm, &dummy, &dummy, 0) != HMISS)
    {
      gen(&moves[0]); 
      num_moves = numb_moves;
      if ((bm >= 0) && (bm < num_moves))
	{
	  comp_to_san(moves[bm], output);
	  make(&moves[0], bm);
	  if (check_legal(&moves[0], bm, 1))
	    {
	      /* only print move AFTER legal check is done */
	      strcat(str, "<");
	      strcat(str, output);
	      strcat(str, "> ");
	      hash_extract_pv(level, str);
	    }
	  unmake(&moves[0], bm);
	}
    }
}

void stringize_pv (char str[])
{
  char output[STR_BUFF];
  int i;
  
  memset(str, 0, STR_BUFF);

   for (i = 1; i < pv_length[1]; i++) 
   {
        	comp_to_san (pv[1][i], output);
          	make(&pv[1][i], 0);
          	strcat (str, output);
	  	strcat (str, " ");
   }

   hash_extract_pv(7, str);
   
   for (i = (pv_length[1]-1); i > 0; i--)
   {
   	unmake(&pv[1][i], 0);
   }

}

void post_thinking (int32_t score) {

  /* post our thinking output: */
  char output[STR_BUFF];

  /* if root move is already/still played, back it up */
  /* 25-06-2000 our en passant info is unrecoverable here
	 so we cannot gen.... */
    
  if (pv_length[1]) {
	  comp_to_coord (pv[1][1], output);
	  printf ("ponder %s\n", output);
  }
}

void post_fail_thinking(int32_t score, move_s *failmove)
{
  /* post our thinking output: */

  char output[STR_BUFF];

  /* in xboard mode, follow xboard conventions for thinking output, otherwise
     output the iterative depth, human readable score, and the pv */
  comp_to_coord (*failmove, output);
  printf ("ponder %s\n", output);
}

void post_fh_thinking(int32_t score, move_s *failmove)
{
  /* post our thinking output: */

  char output[STR_BUFF];

  /* in xboard mode, follow xboard conventions for thinking output, otherwise
     output the iterative depth, human readable score, and the pv */
  comp_to_coord (*failmove, output);
  printf ("ponder %s\n", output);
}

void post_fl_thinking(int32_t score, move_s *failmove)
{
}

void post_stat_thinking(void)
{
  /* post our thinking output: */

  int32_t elapsed;

  elapsed = rdifftime (rtime (), start_time);

  if (xb_mode == 1)
  {
    printf ("stat01: %d %d %d %d %d\n", elapsed, nodes, i_depth, moveleft, movetotal);
  }
  else if (xb_mode == 2)
  {
    printf ("stat01: %d %d %d %d %d %s\n", elapsed, nodes, i_depth, moveleft, movetotal, searching_move);
  }
}


void print_move (move_s moves[], int m, FILE *stream) {

  /* print out a move */

  char move[STR_BUFF];

  comp_to_san (moves[m], move);

  fprintf (stream, "%s", move);

}


void rdelay (int time_in_s) {

  /* My delay function to cause a delay of time_in_s seconds */

  rtime_t time1, time2;
  int32_t timer = 0;

  time1 = rtime ();
  while (timer/100 < time_in_s) {
    time2 = rtime ();
    timer = rdifftime (time2, time1);
  }

}


int32_t rdifftime (rtime_t end, rtime_t start) {

  /* determine the time taken between start and the current time in
     centi-seconds */

  /* using ftime(): */
#if defined(HAVE_SYS_TIMEB_H) && (defined(HAVE_FTIME) || defined(HAVE_GETTIMEOFDAY))
	return (int32_t)((end.time-start.time)*100 + (end.millitm-start.millitm)/10);

  /* -------------------------------------------------- */

  /* using time(): */
#else
  return (100*(int32_t) difftime (end, start));
#endif

}


void check_piece_square (void)
{
  int i;
  
  for (i = 1; i <= piece_count; i++)
  {
    if (squares[pieces[i]] != i && pieces[i] != 0)
    {
      printf("Piece->square->piece inconsistency\n");
      display_board(stdout, 0);
      DIE;
    }
      if (board[pieces[i]] == npiece && pieces[i] != 0)
      {
	printf("Board/Piece->square inconsistency\n");
        display_board(stdout, 0);
	DIE;
      }
	if (pieces[i] == 0 && squares[pieces[i]] != 0)
    {
      printf("Zero-ed piece inconsistency\n");
      display_board(stdout, 0);
      DIE;
    }
  }
  for (i = 0; i < 144; i++)
  {
    if ((board[i] == npiece || board[i] == frame) && squares[i] != 0)
    {
      printf("Empty square has piece pointer\n");
      display_board(stdout, 0);
      DIE;
    }
    if (board[i] != npiece && board[i] != frame && squares[i] == 0)
    {
      printf("Filled square %d has no piece pointer\n", i);
      display_board(stdout, 0);
      DIE;
    }
    if (pieces[squares[i]] != i && squares[i] != 0)
    {
      printf("Square->piece->square inconsistency\n");
      display_board(stdout, 0);
      DIE;
    }
  }
}

void reset_piece_square (void) {

  /* we use piece number 0 to show a piece taken off the board, so don't
     use that piece number for other things: */

   /* reset the piece / square tables: */

   int i, promoted_board[144];

   memset(promoted_board, 0, sizeof(promoted_board));

   /* save our promoted info as we cant determine it from the board */

   for (i = 1; i <= piece_count; i++)
     if(is_promoted[i])
	 promoted_board[pieces[i]] = 1;
   
   Material = 0;

   piece_count = 0;

   memset(pieces, 0, sizeof(pieces));
   memset(is_promoted, 0, sizeof(is_promoted));

   pieces[0] = 0;
   
   for (i = 26; i < 118; i++)
     if (board[i] && (board[i] < npiece)) {
       
       AddMaterial(board[i]);
       
       piece_count += 1;
       
       pieces[piece_count] = i;
       squares[i] = piece_count;
       
       /* restored promoted info */
       if (promoted_board[i])
	 is_promoted[piece_count] = 1;
     }
     else
	squares[i] = 0;
}


void rinput (char str[], int n, FILE *stream) {

  /* My input function - reads in up to n-1 characters from stream, or until
     we encounter a \n or an EOF.  Appends a null character at the end of the
     string, and stores the string in str[] */

  int ch, i = 0;

  while ((ch = getc (stream)) != (int) '\n' && ch != EOF) {
	/* Skip stray interrupt characters at beginning of line */
	if (!i && ch=='?')
	  continue;
    if (i < n-1) {
      str[i++] = ch;
    }
  }

  str [i] = '\0';

}

rtime_t rtime (void) {

  /* using ftime(): */
#if defined(HAVE_FTIME) && defined(HAVE_SYS_TIMEB_H)
  rtime_t temp;
  ftime(&temp);
  return (temp);

  /* -------------------------------------------------- */

  /* gettimeofday replacement by Daniel Clausen */
#else
#if defined(HAVE_GETTIMEOFDAY) && defined(HAVE_SYS_TIMEB_H)
  rtime_t temp;
  struct timeval tmp;

  gettimeofday(&tmp, NULL);
  temp.time = tmp.tv_sec;
  temp.millitm = tmp.tv_usec / 1000;
  temp.timezone = 0;
  temp.dstflag = 0;
  
  return (temp);

#else
  return (time (0));
#endif  
#endif

}


void start_up (void) {

  /* things to do on start up of the program */

  printf("\nSjeng version " VERSION ", Copyright (C) 2000-2001 Gian-Carlo Pascutto\n\n"
         "Sjeng comes with ABSOLUTELY NO WARRANTY; for details type 'warranty'\n"
         "This is free software, and you are welcome to redistribute it\n"
         "under certain conditions; type 'distribution'\n\n");
}


void toggle_bool (bool *var) {

  /* toggle FALSE -> TRUE, TRUE -> FALSE */

  if (*var) {
    *var = FALSE;
  }
  else {
    *var = TRUE;
  }

}


void tree_debug (void) {

  /* A function to make a tree of output at a certain depth and print out
     the number of nodes: */

  char input[STR_BUFF];
  FILE *stream;
  int depth;

  init_game ();

  /* get the desired depth to generate to: */
  printf ("\nPlease enter the desired depth:\n");
  rinput (input, STR_BUFF, stdin);
  depth = atoi (input);

  /* does the user want to output tree () ? */
  printf ("\nDo you want tree () output?  (y/n)\n");
  rinput (input, STR_BUFF, stdin);
  if (input[0] == 'y') {
    /* get our output file: */
    printf ("\nPlease enter the name of the output file for tree ():\n");
    rinput (input, STR_BUFF, stdin);
    if ((stream = fopen (input, "w")) == NULL) {
      fprintf (stderr, "Couldn't open file %s\n", input);
    }

    /* does the user want to output diagrams? */
    printf ("\nDo you want to output diagrams? (y/n)\n");
    rinput (input, STR_BUFF, stdin);

    tree (depth, 0, stream, input);
  }

  /* print out the number of raw nodes for this depth: */
  raw_nodes = 0;
  perft (depth);
  printf ("\n\n%s\nRaw nodes for depth %d: %d\n%s\n\n", divider,
	  depth, raw_nodes, divider);

}


bool verify_coord (char input[], move_s *move) {

  /* checks to see if the move the user entered was legal or not, returns
     true if the move was legal, and stores the legal move inside move */

  move_s moves[MOVE_BUFF];
  int num_moves, i;
  char comp_move[6];
  bool legal = FALSE;
  bool mate;

  if (Variant == Losers)
    {
      captures = TRUE;
      num_moves = 0;
      gen (&moves[0]);
      num_moves = numb_moves;
      captures = FALSE;
      
      mate = TRUE;
      
      for (i = 0; i < num_moves; i++) 
	{
	  make (&moves[0], i);
	  
	  /* check to see if our move is legal: */
	  if (check_legal (&moves[0], i, TRUE)) 
	    {
	      mate = FALSE;
	      unmake(&moves[0], i);
	      break;
	    };
	  
	  unmake(&moves[0], i);
	}
      
      if (mate == TRUE)
	{
	  /* no legal capture..do non-captures */
	  captures = FALSE;
	  num_moves = 0;
	  gen (&moves[0]);
	  num_moves = numb_moves;
	}
    }
  else
    {
      gen (&moves[0]); 
      num_moves = numb_moves;
    }  

  /* compare user input to the generated moves: */
  for (i = 0; i < num_moves; i++) {
    comp_to_coord (moves[i], comp_move);
    if (!strcasecmp (input, comp_move)) {
      make (&moves[0], i);
      if (check_legal (&moves[0], i, TRUE)) {
	legal = TRUE;
	*move = moves[i];
      }
      unmake (&moves[0], i);
    }
  }

  return (legal);

}

int interrupt(void)
{
  int c;

#ifdef HAVE_SELECT
  FD_ZERO(&read_fds);
  FD_SET(0,&read_fds);
  timeout.tv_sec = timeout.tv_usec = 0;
  select(1,&read_fds,NULL,NULL,&timeout);
  if(FD_ISSET(0,&read_fds)) 
    {
      c = getc(stdin);

      if (c == '?')   /*Move now*/
	{
	  return 1;
	}
      else if (c == '.')     /* Stat request */
	{
	  getc(stdin);
	  post_stat_thinking();
	  return 0;
	}

      ungetc(c, stdin);

      if (!is_pondering && (Variant == Bughouse || Variant == Crazyhouse)) return 0; 

      return 1;
    }
  else return 0;
#else 
#ifdef _WIN32
  static int init = 0, pipe;
  static HANDLE inh;
  DWORD dw;
  if(xb_mode) {     /* winboard interrupt code taken from crafty */
    if (!init) {
      init = 1;
      inh = GetStdHandle(STD_INPUT_HANDLE);
      pipe = !GetConsoleMode(inh, &dw);
      if (!pipe) {
	SetConsoleMode(inh, dw & ~(ENABLE_MOUSE_INPUT|ENABLE_WINDOW_INPUT));
	FlushConsoleInputBuffer(inh);
      }
    }
    if(pipe) {
      if(!PeekNamedPipe(inh, NULL, 0, NULL, &dw, NULL)) 
	{
	  c = getc(stdin);
	  
	  if (c == '?')   /*Move now*/
	    {
	      return 1;
	    }
	  else if (c == '.')     /* Stat request */
	    {
	      getc(stdin);
	      post_stat_thinking();
	      return 0;
	    }
	  
	  ungetc(c, stdin);
	  
	  if (!is_pondering && (Variant == Bughouse || Variant == Crazyhouse)) return 0; 
	  
	  return 1;
	}
      if (dw)
	{
	  c = getc(stdin);
	  
	  if (c == '?')   /*Move now*/
	    {
	      return 1;
	    }
	  else if (c == '.')     /* Stat request */
	    {
	      getc(stdin);
	      post_stat_thinking();
	      return 0;
	    }
	  
	  ungetc(c, stdin);
	  
	  if (!is_pondering && (Variant == Bughouse || Variant == Crazyhouse)) return 0; 
	  
	  return 1;
	}
      else return 0;
    } else {
      GetNumberOfConsoleInputEvents(inh, &dw);
      if (dw <= 1)
	{ 
	  return 0; 
	}
      else 
	{ 
	  c = getc(stdin);
	  
	  if (c == '?')   /*Move now*/
	    {
	      return 1;
	    }
	  else if (c == '.')     /* Stat request */
	    {
	      getc(stdin);
	      post_stat_thinking();
	      return 0;
	    }
	  
	  ungetc(c, stdin);
	  
	  if (!is_pondering && (Variant == Bughouse || Variant == Crazyhouse)) return 0; 
	  
	  return 1;
	};
    }
  }
#else
#endif
#endif

  return 0;
}

void PutPiece(int color, char piece, char pfile, int prank)
{
  int converterf = (int) 'a';
  int converterr = (int) '1';
  int norm_file, norm_rank, norm_square;

  norm_file = pfile - converterf;
  norm_rank = prank - converterr;

  norm_square = ((norm_rank * 12) + 26) + (norm_file);

  if (color == WHITE)
    {
      switch (piece) 
	{
	case 'p':
	  board[norm_square] = wpawn;
	  break;
	case 'n':
	  board[norm_square] = wknight;
	  break;
	case 'b':
	  board[norm_square] = wbishop;
	  break;
	case 'r':
	  board[norm_square] = wrook;
	  break;
	case 'q':
	  board[norm_square] = wqueen;
	  break;
	case 'k':
	  board[norm_square] = wking;
	  break;
	case 'x':
	  board[norm_square] = npiece;
	  break;
	}
    }
  else if (color == BLACK)
    {
      switch (piece)
	{
	case 'p':
	  board[norm_square] = bpawn;
	  break;
	case 'n':
	  board[norm_square] = bknight;
	  break;
	case 'b':
	  board[norm_square] = bbishop;
	  break;
	case 'r':
	  board[norm_square] = brook;
	  break;
	case 'q':
	  board[norm_square] = bqueen;
	  break;
	case 'k':
	  board[norm_square] = bking;
	  break;
	case 'x':
	  board[norm_square] = npiece;
	  break;
	}
    }

  return;
}

void reset_board (void) {

  /* set up an empty  game: */

  int i;

  int init_board[144] = {
    0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,13,13,13,13,13,13,13,13,0,0,
    0,0,13,13,13,13,13,13,13,13,0,0,
    0,0,13,13,13,13,13,13,13,13,0,0,
    0,0,13,13,13,13,13,13,13,13,0,0,
    0,0,13,13,13,13,13,13,13,13,0,0,
    0,0,13,13,13,13,13,13,13,13,0,0,
    0,0,13,13,13,13,13,13,13,13,0,0,
    0,0,13,13,13,13,13,13,13,13,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0
  };

  memcpy (board, init_board, sizeof (init_board));
  for (i = 0; i <= 143; i++)
    moved[i] = 0;

  ep_square = 0;

  piece_count = 0;

  Material = 0;

  memset(is_promoted, 0, sizeof(is_promoted));
  memset(holding, 0, sizeof(holding));

  white_hand_eval = 0;
  black_hand_eval = 0;

  bookidx = 0;
  fifty = 0;
  
  reset_piece_square ();
  
}

void speed_test(void)
{
  move_s moves[MOVE_BUFF];
  int i, j;
  clock_t cpu_start, cpu_end; 
  float et;
  int ic;

  /* LCT2 Pos 1 */
  setup_epd_line("r3kb1r/3n1pp1/p6p/2pPp2q/Pp2N3/3B2PP/1PQ2P2/R3K2R w KQkq");
   
  cpu_start = clock ();

  for (i = 0; i < 5000000; i++)
    {
      gen (&moves[0]);
    }
  
  cpu_end = clock ();
  et = (cpu_end-cpu_start)/(float) CLOCKS_PER_SEC;
  
  printf("Movegen speed: %d/s\n", (int)(5000000.0/et));
  j = 0;
  
  cpu_start = clock ();
  
  for (i = 0; i < 50000000; i++)
    {
      make (&moves[0], j);
      unmake (&moves[0], j);
      
      if ((j+1) < numb_moves) j++;
      else j = 0;
    }
  
  cpu_end = clock ();
  et = (cpu_end-cpu_start)/(float) CLOCKS_PER_SEC;
  
  printf("Make+unmake speed: %d/s\n", (int)(50000000.0/et));
  
  j = 0;

  ic = in_check();
  
  cpu_start = clock ();
  
  for (i = 0; i < 50000000; i++)
    {
      make (&moves[0], j);

      check_legal(&moves[0], j, ic);
      
      unmake (&moves[0], j);
      
      if ((j+1) < numb_moves) j++;
      else j = 0;
    }
  
  cpu_end = clock ();
  et = (cpu_end-cpu_start)/(float) CLOCKS_PER_SEC;
  
  printf("Movecycle speed: %d/s\n", (int)(50000000.0/et));
  
  reset_ecache();
  
  cpu_start = clock ();
  
  for (i = 0; i < 10000000; i++)
    {
      eval();
      /* invalidate the ecache */
      hash = (++hash) % UINT_MAX; 
    }
  
  cpu_end = clock ();
  et = (cpu_end-cpu_start)/(float) CLOCKS_PER_SEC;
  
  printf("Eval speed: %d/s\n", (int)(10000000.0/et));
  
  /* restore the hash */
  initialize_hash();
  
}

/* Mersenne Twister */

void seedMT(uint32_t seed)
{
  register uint32_t x = (seed | 1U) & 0xFFFFFFFFU, *s = state;
  register int    j;

  for(left=0, *s++=x, j=N; --j;
      *s++ = (x*=69069U) & 0xFFFFFFFFU);
}

uint32_t reloadMT(void)
{
  register uint32_t *p0=state, *p2=state+2, *pM=state+M, s0, s1;
  register int    j;

  if(left < -1)
    seedMT(4357U);

  left=N-1, next=state+1;

  for(s0=state[0], s1=state[1], j=N-M+1; --j; s0=s1, s1=*p2++)
    *p0++ = *pM++ ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0U);

  for(pM=state, j=M; --j; s0=s1, s1=*p2++)
    *p0++ = *pM++ ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0U);

  s1=state[0], *p0 = *pM ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0U);
  s1 ^= (s1 >> 11);
  s1 ^= (s1 <<  7) & 0x9D2C5680U;
  s1 ^= (s1 << 15) & 0xEFC60000U;
  return(s1 ^ (s1 >> 18));
}

uint32_t randomMT(void)
{
  uint32_t y;

  if(--left < 0)
    return(reloadMT());

  y  = *next++;
  y ^= (y >> 11);
  y ^= (y <<  7) & 0x9D2C5680U;
  y ^= (y << 15) & 0xEFC60000U;
  return(y ^ (y >> 18));
}
