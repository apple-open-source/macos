/* $Xorg: bits.h,v 1.4 2001/02/09 02:04:25 xorgcvs Exp $ */
/* AGE Logic - Oct 15 1995 - Larry Hare */
/**** module fax/bits.h ****/
/******************************************************************************

Copyright 1993, 1994, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.


				NOTICE
                              
This software is being provided by AGE Logic, Inc. under the
following license.  By obtaining, using and/or copying this software,
you agree that you have read, understood, and will comply with these
terms and conditions:

     Permission to use, copy, modify, distribute and sell this
     software and its documentation for any purpose and without
     fee or royalty and to grant others any or all rights granted
     herein is hereby granted, provided that you agree to comply
     with the following copyright notice and statements, including
     the disclaimer, and that the same appears on all copies and
     derivative works of the software and documentation you make.
     
     "Copyright 1993, 1994 by AGE Logic, Inc."
     
     THIS SOFTWARE IS PROVIDED "AS IS".  AGE LOGIC MAKES NO
     REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED.  By way of
     example, but not limitation, AGE LOGIC MAKE NO
     REPRESENTATIONS OR WARRANTIES OF MERCHANTABILITY OR FITNESS
     FOR ANY PARTICULAR PURPOSE OR THAT THE SOFTWARE DOES NOT
     INFRINGE THIRD-PARTY PROPRIETARY RIGHTS.  AGE LOGIC 
     SHALL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE.  IN NO
     EVENT SHALL EITHER PARTY BE LIABLE FOR ANY INDIRECT,
     INCIDENTAL, SPECIAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOSS
     OF PROFITS, REVENUE, DATA OR USE, INCURRED BY EITHER PARTY OR
     ANY THIRD PARTY, WHETHER IN AN ACTION IN CONTRACT OR TORT OR
     BASED ON A WARRANTY, EVEN IF AGE LOGIC LICENSEES
     HEREUNDER HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH
     DAMAGES.
    
     The name of AGE Logic, Inc. may not be used in
     advertising or publicity pertaining to this software without
     specific, written prior permission from AGE Logic.

     Title to this software shall at all times remain with AGE
     Logic, Inc.
*****************************************************************************
  
	fax/bits.h -- DDXIE G4 fax bitstream management macros
  
	Ben Fahy -- AGE Logic, Inc. May, 1993
  
*****************************************************************************/

#include "gbits.h"

/* ------------------------------------------------------------------- */
/*
 *	get_white_run_length(next_goal)
 *	get_black_run_length(next_goal)
 *
 * 	These macros try to determine run lengths in horizonal mode.
 *	All possible bit combinations have been previously coded in
 *	a lookup table, so it's not a very big deal.  The only real
 *	complication arises from the fact I may run out of strip data
 *	before finishing.  In this case I want to save my length
 * 	accumulator and read it back in again.  If I have finished
 *	with this length,  I want to move on to the next goal specified
 *	by 'next_goal'.  Otherwise my goal remains to accumulate the
 *	current desired run length.
 */

#define get_white_run_length(next_goal)					\
	{ 								\
	register int code=0,nbits,makeup;				\
	  rl = 0;							\
	  while (1) {							\
 	    code = get_wcode(byteptr,bitpos,endptr);			\
	    rl     = _WhiteFaxTable[code].run_length;			\
	    nbits  = _WhiteFaxTable[code].n_bits;			\
	    makeup = _WhiteFaxTable[code].makeup;			\
	    if (rl == BAD_RUN_LENGTH) {					\
	      length_acc = 0;						\
	      if (!code) {						\
		/* could just be fill bits, try to recover */		\
		goal = FAX_GOAL_RecoverZero;				\
	        break;							\
	      }								\
	      else {							\
		goal = FAX_GOAL_FallOnSword;				\
	        break;							\
	      }								\
	    }								\
	    if (rl != EOL_RUN_LENGTH)					\
	      length_acc += rl;						\
	    if (!makeup)						\
	      goal = next_goal;						\
	      /* just in case adjust_bitstream returns for more data */	\
	    adjust_bitstream(nbits,byteptr,bitpos,endptr);		\
	    if (!makeup)						\
	      break;							\
	  }								\
	}

#define get_black_run_length(next_goal)					\
	{ 								\
	register int code=0,nbits,makeup;				\
	  rl = 0;							\
	  while (1) {							\
 	    code = get_bcode(byteptr,bitpos,endptr);			\
	    rl     = _BlackFaxTable[code].run_length;			\
	    nbits  = _BlackFaxTable[code].n_bits;			\
	    makeup = _BlackFaxTable[code].makeup;			\
	    if (rl == BAD_RUN_LENGTH) {					\
	      length_acc = 0;						\
	      if (!code) {						\
		/* could just be fill bits, will try to recover */	\
		goal = FAX_GOAL_RecoverZero;				\
	        break;							\
	      }								\
	      else {							\
		goal = FAX_GOAL_FallOnSword;				\
	        break;							\
	      }								\
	    }								\
	    if (rl != EOL_RUN_LENGTH)					\
	      length_acc += rl;						\
	    /* if this is an EOL of terminating code, go to next goal */\
	    if (!makeup)						\
	      goal = next_goal;						\
	      /* (set in case adjust_bitstream returns for more data) */\
	    adjust_bitstream(nbits,byteptr,bitpos,endptr);		\
	    if (!makeup)						\
	      break;							\
	  }								\
	}

/* ------------------------------------------------------------------- */
/*
 *	get_a0a1(next_goal)
 *	get_a1a2(next_goal)
 *
 * 	These macros try to determine run lengths in horizonal mode.
 *	They just leverage off of lower-level macros, so they're easy
 */
#define get_a0a1(next_goal)						\
	{								\
	   if (a0_color == WHITE)					\
	      get_white_run_length(next_goal) /* ; */			\
	   else								\
	      get_black_run_length(next_goal);				\
	}
#define get_a1a2(next_goal)						\
	{								\
	   if (a0_color == WHITE)					\
	      get_black_run_length(next_goal) /* ; */			\
	   else								\
	      get_white_run_length(next_goal);				\
	}
/* ------------------------------------------------------------------- */
/*
 *	save_state_and_return(state);
 *	localize_state(state);
 *
 * 	These macros transfer state variables back and forth between
 *	the reentrant data structure 'state' and register variables.
 *	Hopefully neither will be called often,  so we don't have to
 *	spend energy analyzing what is 'dirty' versus what hasn't been
 *	touched since the last save, etc.
 *
 * 	Strategy:
 *		brute force.
 *
 */
#if defined(_G32D)
#define save_state_and_return(state) 					\
	{								\
	    state->a0_color 	= a0_color;				\
	    state->a0_pos   	= a0_pos;				\
	    state->a0a1   	= a0a1;					\
	    state->bits.bitpos  = bitpos;				\
	    state->bits.byteptr = byteptr;				\
	    state->bits.endptr  = endptr;				\
	    state->goal     	= goal;					\
	    state->n_old_trans	= n_old_trans;				\
	    state->old_trans	= old_trans;				\
	    state->n_new_trans	= n_new_trans;				\
	    state->new_trans	= new_trans;				\
	    state->length_acc	= length_acc;				\
	    state->last_b1_idx	= last_b1_idx;				\
	    state->width	= width;				\
	    state->rl  		= rl;					\
	    state->g32d_horiz	= g32d_horiz;				\
	    return(lines_found);					\
	}

#define	localize_state(state)						\
	    a0_color  		= state->a0_color;			\
	    a0_pos	  	= state->a0_pos;			\
	    a0a1		= state->a0a1;   			\
	    bitpos 	  	= state->bits.bitpos;			\
	    byteptr	  	= state->bits.byteptr;			\
	    endptr    		= state->bits.endptr;			\
	    goal	  	= state->goal;				\
	    n_old_trans		= state->n_old_trans;			\
	    old_trans		= state->old_trans;			\
	    n_new_trans		= state->n_new_trans;			\
	    new_trans		= state->new_trans;			\
	    length_acc		= state->length_acc;			\
	    last_b1_idx		= state->last_b1_idx;			\
	    width		= state->width;				\
	    rl			= state->rl;				\
	    g32d_horiz		= state->g32d_horiz;
#endif  /* if defined(_G32D) */

#if defined(_G31D) || defined(_G4)
#define save_state_and_return(state) 					\
	{								\
	    state->a0_color 	= a0_color;				\
	    state->a0_pos   	= a0_pos;				\
	    state->a0a1   	= a0a1;					\
	    state->bits.bitpos  = bitpos;				\
	    state->bits.byteptr = byteptr;				\
	    state->bits.endptr  = endptr;				\
	    state->goal     	= goal;					\
	    state->n_old_trans	= n_old_trans;				\
	    state->old_trans	= old_trans;				\
	    state->n_new_trans	= n_new_trans;				\
	    state->new_trans	= new_trans;				\
	    state->length_acc	= length_acc;				\
	    state->last_b1_idx	= last_b1_idx;				\
	    state->width	= width;				\
	    state->rl		= rl;					\
	    return(lines_found);					\
	}

#define	localize_state(state)						\
	    a0_color  		= state->a0_color;			\
	    a0_pos	  	= state->a0_pos;			\
	    a0a1		= state->a0a1;   			\
	    bitpos 	  	= state->bits.bitpos;			\
	    byteptr	  	= state->bits.byteptr;			\
	    endptr    		= state->bits.endptr;			\
	    goal	  	= state->goal;				\
	    n_old_trans		= state->n_old_trans;			\
	    old_trans		= state->old_trans;			\
	    n_new_trans		= state->n_new_trans;			\
	    new_trans		= state->new_trans;			\
	    length_acc		= state->length_acc;			\
	    last_b1_idx		= state->last_b1_idx;			\
	    width		= state->width;				\
	    rl			= state->rl;
#endif  /* defined(_G31D) */

#if defined(_PBits)
#define save_state_and_return(state) 					\
	{								\
	    state->a0_color 	= a0_color;				\
	    state->a0_pos   	= a0_pos;				\
	    state->bits.byteptr = byteptr;				\
	    state->bits.endptr  = endptr;				\
	    state->goal     	= goal;					\
	    state->length_acc	= length_acc;				\
	    state->width	= width;				\
	    state->rl		= rl;					\
	    return(lines_found);					\
	}

#define	localize_state(state)						\
	    a0_color  		= state->a0_color;			\
	    a0_pos	  	= state->a0_pos;			\
	    byteptr	  	= state->bits.byteptr;			\
	    endptr    		= state->bits.endptr;			\
	    goal	  	= state->goal;				\
	    length_acc		= state->length_acc;			\
	    width		= state->width;				\
	    rl			= state->rl;
#endif  /* defined(_Pbits) */

/* ------------------------------------------------------------------- */
/*
 *	reset_transitions();
 *
 * 	Macro to trade old and new transition buffers
 *
 * 	Strategy:
 *		Do the obvious
 */
#define	reset_transitions()						\
	    {								\
	    register int *tmp = old_trans;				\
	    old_trans   = new_trans;					\
	    new_trans   = tmp;						\
	    n_old_trans = n_new_trans;					\
	    n_new_trans = 0;						\
	    last_b1_idx	= 0;						\
	    }

/* ------------------------------------------------------------------- */
/*
 * 	get_mode_and_length(mode,length,byteptr,bitpos,endptr);
 *
 * 	Macro to get next coding mode (vertical or horizontal or ...)
 * 
 *	Strategy:  the modes are encoded with 8 bits or less except
 *		for EOL,  which is 001 (12 bits).  However, none of 
 *		the other modes use 00, so there's no conflict.
 *
 *		We have precomputed a lookup table that tells us for
 *		any sequence of 8 bits, what mode and length goes
 *		along with that sequence.  So all we have to do is 
 *		get the 8 bits and run them through the lookup table 
 *		and we're done.
 *
 *	Note:	I'm trying to write this so a decent compiler will only
 *		do one load to get both mode and length.  Hopefully, your
 *		compiler will fetch '*entry' all at once, then shift and
 *		mask to get components.
 *
 *		Very old compilers may have troubles with the struct =
 *		construction.  You'll have to change entry to *entry
 *		and the entry.mode to entry->mode, etc.
 */

#define get_mode_and_length(mode,length,byteptr,bitpos,endptr) 		\
	{ register unsigned char bits=get_byte(byteptr,bitpos,endptr);	\
	  register TwoDTable entry;					\
	  entry = _TwoDFaxTable[bits];					\
	  mode   = entry.mode;						\
	  length = entry.n_bits;					\
	}

/* ------------------------------------------------------------------- */

/*
 * 	find_b1pos(a0_pos,a0_color,n_old_trans,old_trans);
 *
 * 	Macro to find pixel of opposite color of a0, to right of a0,
 *	on the previous line.
 * 
 *	Strategy:  b1 is more or less monotonically increasing. The
 *		only possible exception is during vertical coding when
 *		a1 is left of b1 by 2 or 3:
 *
 *	 0   1   2   3   4   5   6   7   8   9  10  11  12  13  14
 *                       b1'     b1
 *       B   B   B   B   W   W   B   B   B   B   
 *       B   B   W   B   W   B   W
 *		a0  a1  
 *
 *	In the diagram above, b1 is the first changing element to the
 *	right of a0 which goes from white to black.  a1 is three to the
 *	left of b1.  
 *
 *	When we set a0'=a1 and proceed decoding, since the new a0' is 
 *	black,  we want the first changing element to the right of a0'
 *	which goes from black to white.  This is b1', as pictured. Note
 *	that b1' < b1!  So the b1 sequence is not monotically increasing,
 *	though it is close.  
 *
 *	At most we have to back up one transition to get the right 
 *	changing element for a0'.  This is because b1 is W->B, while
 *	the transition before is B-W, the one before that W->B, and
 *	so on. The transition before b1 is a candidate for b1', but
 *	not the one before that (it's the wrong color). The third
 *	transition before b1 must be at a position <= a0'.  But the
 *	definition of b1' requires that b1' be to the right of a0'.
 *	Therefore,  only the transition immediately before b1 could
 *	possibly server as b1'.
 *
 *	Moral:  If the last b1 we found was index last_b1_idx, then
 *	we can start searching for b1' at last_b1_idx-1,  with complete
 *	assurance that we'll find it.  (if it exists :)
 *
 *	One more thing:  WHITE is defined as 0. BLACK is 1.  The 1st
 *	transition (last_b1_idx=0) is W->B,  the 2nd (last_b1_idx=1)
 *	is B->W, etc.  So an even transition is black, and odd is white.
 *
 *			   BLACK    WHITE
 *			 |   0   |   1   |  bit1 of transition idx
 *             ----------|-------|-------|
 *	  a0   WHITE(0)  |  ok   |  bad  |
 *	color  BLACK(1)  |  bad  |  ok   |
 *             ----------|-------|-------|
 *
 *	We need a transition which is opposite in color to a0_color.  
 *	From the table above, if a0_color ^ (idx & 1), the colors
 *	match and we need to choose the next transition to the right.
 */
#define find_b1pos(a0_pos,a0_color,n_old,old)				\
	{  								\
	  if (last_b1_idx > 0) 						\
		--last_b1_idx;	/* avoid gotcha from above evil case */	\
									\
	  /* search for first changing element to right of a0_pos */	\
	  while (old[last_b1_idx] <= a0_pos) 				\
		if (++last_b1_idx >= n_old)				\
			break;	/* if out of transitions, give up */	\
									\
	  /* check out color - if matching, use next transition	*/	\
	  if (a0_color ^ (last_b1_idx & 1))				\
		++last_b1_idx;						\
	 	    							\
	  if (last_b1_idx < n_old)  					\
		b1_pos = old[last_b1_idx];				\
	  else {							\
		last_b1_idx = 0;					\
		b1_pos = -1;						\
	  }								\
	}

/* ------------------------------------------------------------------- */
/*
 * 	find_b2pos(a0_pos,a0_color,n_old_trans,old_trans);
 *
 * 	Macro to find the end of a chunk of pixels in the previous line,
 *	which we passed by because they are uncorrelated with this line.
 * 
 *	Strategy:  teeny tiny modification to find_b1pos.
 *
 */
#define find_b2pos(a0_pos,a0_color,n_old,old)				\
	{  								\
	  if (last_b1_idx > 0) 						\
		--last_b1_idx;	/* avoid gotcha from above evil case */	\
									\
	  /* search for first changing element to right of a0_pos */	\
	  while (old[last_b1_idx] <= a0_pos) 				\
		if (++last_b1_idx >= (n_old-1))	/* CHANGE */		\
			break;	/* if out of transitions, give up */	\
									\
	  /* check out color - if matching, use next transition	*/	\
	  if (a0_color ^ (last_b1_idx & 1))				\
		++last_b1_idx;						\
	 	    							\
	  if (last_b1_idx < n_old-1)  		/* CHANGE */		\
		b2_pos = old[last_b1_idx+1];    /* CHANGE */		\
	  else {							\
		last_b1_idx = 0;					\
		b2_pos = -1;						\
	  }								\
	}

/* ------------------------------------------------------------------- */
/**** module fax/bits.h ****/
