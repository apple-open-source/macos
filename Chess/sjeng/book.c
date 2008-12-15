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

    File: book.c                                             
    Purpose: book initialization, selection of book moves, etc...

*/

#include "sjeng.h"
#include "protos.h"
#include "extvars.h"

char book[4000][161];
char book_flags[4000][41];
int num_book_lines;
int book_ply;
int use_book;
char opening_history[STR_BUFF];

#define book_always 1        /* corresponds with ! */
#define book_never 2         /* corresponds with ? */
#define book_interesting 3   /* !?  */
#define book_solid 4         /* =   */
#define book_murky 5         /* ?!  */

int init_book (void) {

   /* simply read all the book moves into a book array.  The book will be
      a simple char[5000][81] array (5000 max book lines, since you can't
      have *too* many when you're doing this slow strncpy method ;)  After
      all, this is really just a kludge 'till I add hashing, and support
      transpositions and such ;) Returns true if reading in the book
      was successful. */

   FILE *f_book;
   int ch, i = 0, j = 0;

   int tagmode = FALSE; /* recognize multiple tags */
   int commentmode = FALSE; 

   memset(book_flags, 0, sizeof(book_flags));
   memset(book, 0, sizeof(book));

   num_book_lines = 0;
   
   /* init our random numbers: */
   srand((unsigned) time (NULL));

   if (Variant == Normal)
     {
       if ((f_book = fopen ("normal.opn", "r")) == NULL)
	 return FALSE;
     }
   else if (Variant == Crazyhouse)
     {
       if ((f_book = fopen ("zh.opn", "r")) == NULL)
	 return FALSE;
     }
   else if (Variant == Suicide)
     {
       if ((f_book = fopen ("suicide.opn", "r")) == NULL)
	 return FALSE;
     }
   else if (Variant == Losers)
   {
       if ((f_book = fopen ("losers.opn", "r")) == NULL)
	 return FALSE;
   }
   else
     {
       if ((f_book = fopen ("bug.opn", "r")) == NULL)
	 return FALSE;
     }

   while ((ch = getc(f_book)) != EOF) {

     if (commentmode)
       {
	 if (ch == '/') /* end comment */
	   {
	     commentmode = FALSE;
	   }
       }
     else
       {
	 if (ch == '\n') { /* end of book line */
	   
	   /* not ending an empty book line */
	   if (j > 0)
	     {
	       book[i++][j] = '\0';
	       j = 0;
	     }
	   
	   tagmode = FALSE;
	 }
	 else if (ch == '!')
	   {
	     if (!tagmode)
	       {
		 book_flags[i][((j + 1) / 4) - 1] = book_always;
		 tagmode = TRUE;
	       }
	     else
	       {
		 book_flags[i][((j + 1) / 4) - 1] = book_murky;
	       }
	   }
	 else if (ch == '?')
	   {
	     if (!tagmode)
	       {
		 book_flags[i][((j + 1) / 4) - 1] = book_never;
		 tagmode = TRUE;
	       }
	     else
	       {
		 book_flags[i][((j + 1) / 4) - 1] = book_interesting;
	       }
	   }
	 else if (ch == '=')
	   {
	     book_flags[i][((j + 1) / 4) - 1] = book_solid;
	     tagmode = TRUE;
	   }
	 else if ((ch == ' ') || (ch == '\t'))
	   {
	     /* skip spaces and tabs */
	     tagmode = FALSE;
	   }
	 else if (ch == '/') /* start comment */
	   {
	     commentmode = TRUE;
	   }
	 else if (j < 160 && i < 4000) /* middle of book line */
	   {
	     book[i][j++] = ch;
	     tagmode = FALSE;
	   }
	 /* If j >= 100, then the book line was too long.  The rest will be
	    read, but ignored, and will be null-character terminated when a
	    '\n' is read.  If i >= 2000, then there are too many book lines.
	    The remaining lines will be read, but ignored. */
       }
   }

   num_book_lines = i;

   fclose(f_book);

   return TRUE;

}

move_s choose_book_move (void) {

   /* Since the book is sorted alphabetically, we can use strncpy, and hope
      to get early exits once we've found the first few moves in the book.
      Once we choose a book move, we'll make a variable indicate where
      it was found, so we can start our search for the next move there. */

   static int last_book_move = 0;
   int book_match = FALSE;
   int i, j, num_moves, random_number, num_replies;
   char possible_move[5], coord_move[5];
   move_s book_replies[4000], moves[400];
   char force_move = FALSE;
   int ic;

   srand((unsigned)time(0));

   if (!book_ply)
     last_book_move = 0;

   num_replies = 0;
   gen(&moves[0]); 
   num_moves = numb_moves;

   for (i = last_book_move; i < num_book_lines; i++) {
      /* check to see if opening line matches up to current book_ply: */
      if (!strncmp(opening_history, book[i], (book_ply * 4)) || (!book_ply)) {
	/* if book move is legal, add it to possible list of book moves */

	if ((book_flags[i][book_ply] != book_never) 
	    && (book_flags[i][book_ply] != book_murky))
	  {
	    if (book_flags[i][book_ply] == book_always)
	      {
		if (force_move != TRUE)
		  {
		    /* found 1st ! move -> remove normal ones */
		    force_move = TRUE;
		    num_replies = 0;
		  }
	      }

	    if ((force_move != TRUE) || 
		((force_move == TRUE) && 
		 (book_flags[i][book_ply] == book_always)))
	      {
		strncpy(possible_move, book[i] + (book_ply * 4), 4);
		possible_move[4] = '\0';
		
		for (j = 0; j < num_moves; j++) 
		  {
		    comp_to_coord(moves[j], coord_move);
		    
		    if (!strcmp(possible_move, coord_move)) 
		      {                      
			ic = in_check();
			make(&moves[0], j);
			if (check_legal(&moves[0], j, ic)) 
			  {
			    book_replies[num_replies++] = moves[j];
			    book_match = TRUE;
			  }
			unmake(&moves[0], j);
		      }
		  }
	      }
	  }
      }
      /* we can exit the search for a book move early, if we've no longer
         have a match, but we have found at least one match */
      if (!book_match && num_replies)
         break;
   }

   /* now, if we have some book replies, pick our move randomly from
      book_replies: */
   if (!num_replies)
      return dummy;

   printf("Book moves: %d\n", num_replies);

   random_number = rand() % num_replies;
   return book_replies[random_number];

}

