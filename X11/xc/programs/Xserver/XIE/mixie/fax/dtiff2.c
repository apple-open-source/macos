/* $Xorg: dtiff2.c,v 1.4 2001/02/09 02:04:25 xorgcvs Exp $ */
/**** module fax/dtiff2.c ****/
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
  
	fax/dtiff2.c -- DDXIE TIFF2 fax decode technique/element
  
	Ben Fahy -- AGE Logic, Inc. Sept, 1993
  
*****************************************************************************/

/* the folling define causes extra stuff to be saved in state recorder */
#define _G31D
#include "fax.h"
#include "faxint.h"
#include "bits.h"

#include <servermd.h>
	/* pick up the BITMAP_BIT_ORDER from Core X*/

/**********************************************************************/
int decode_tiff2(state)
FaxState *state;
{
register int bitpos;
register unsigned char *byteptr;
register unsigned char *endptr;
register int a0_color;
register int a0_pos;
register int a1_pos;
register int goal;
register int width;
register int rl;
register int last_b1_idx = 0;

int 	lines_found=0;
int	n_old_trans;
int	n_new_trans;
int	*old_trans;
int	*new_trans;
int 	a0a1,a1a2;
int 	length_acc=0;

	if (state == (FaxState *)NULL)
		return(-1);

	/* set up initial bitstream for the very first strip */
	if (!state->bits.started) {
	    if (state->strip_state != StripStateNew) {
	       	state->decoder_done = FAX_DECODE_DONE_ErrorBadStripper;
		return(-1);
	    }
	    state->bits.byteptr = (unsigned char *)state->strip;
	    state->bits.endptr  = state->bits.byteptr + state->strip_size-4;
	       /* we will panic with four bytes to go */

	    state->bits.bitpos = 0;
	    state->bits.started = 1;
	}

	localize_state(state);

	if (state->magic_needs) 
	    finish_magic(state->final);
		/* a magic strip was waiting for 1st word of next strip */

/***	Main Decoding Loop	***/
	while(1) {
	  switch(goal) {

	  case FAX_GOAL_StartNewLine:

 	    skip_bits_at_eol(byteptr,bitpos,endptr);

	    if (lines_found >= state->nl_sought)
		 save_state_and_return(state);

	    /* normal line initialization stuff */
	    a0_pos   = -1;
	    a0_color = WHITE;
	    goal = FAX_GOAL_HandleHoriz;

	    reset_transitions();
	    break;

	  case  FAX_GOAL_HandleHoriz: 
	    length_acc=0;
	    goal = FAX_GOAL_AccumulateA0A1;
		/* in case I run out of data while getting a0-a1 distance */

	  case FAX_GOAL_AccumulateA0A1:
	    get_a0a1(FAX_GOAL_RecordA0A1);
		/* If we have to return for data before getting the 	*/
		/* whole a0a1 distance, we want to return to the a0a1	*/
		/* accumulate state when we reenter the decoder. If	*/
		/* adjusting the bitstream forces us to return after	*/
		/* getting the last part of the a0a1 distance, then	*/
		/* we want to jump to state FAX_GOAL_RecordA0A1 when	*/
		/* we reenter the decoder.				*/

	  case FAX_GOAL_FallOnSword:
	    if (goal == FAX_GOAL_FallOnSword) {
	        /* received a yucky code */
		state->decoder_done = FAX_DECODE_DONE_ErrorBadCode;
		save_state_and_return(state);
	    }

	  case FAX_GOAL_RecoverZero:
	    /* g31d can recover.  tiff2, alas, cannot */
	    if (goal == FAX_GOAL_RecoverZero) {
		state->decoder_done = FAX_DECODE_DONE_ErrorBadZero;
		save_state_and_return(state);
	    }

	  case  FAX_GOAL_RecordA0A1:
	    a0a1 = length_acc;
	    if (a0a1 < 0) {
		state->decoder_done = FAX_DECODE_DONE_ErrorBada0a1;
		save_state_and_return(state);
	    }
	    if (a0_pos < 0) {
		/* at start of line, a0a1 is the number of white pixels,  */
		/* which is also the index on the line where white->black */
		new_trans[n_new_trans++] = a1_pos = a0a1;
	    } else {
		/* in middle of the line, a0a1 is run-length, so
		 * a1_pos = a0_pos + a0a1, a2_pos = a1_pos + a1a2
		 *
		 *  0  1  2  3  4  5  6  7  8  9  A  B  C  D 
		 *    |w |w |w |b |b |b |b |b |w |  |  |  |  |
		 *     a0       a1             a2
		 */ 
		new_trans[n_new_trans++] = a1_pos = a0_pos + a0a1;
	    }
	    if (a1_pos >= width) {
		if (a1_pos > width) {
		   state->decoder_done = FAX_DECODE_DONE_ErrorPastWidth;
		   save_state_and_return(state);
		}
		FlushLineData();
	        goal = FAX_GOAL_StartNewLine;
		break;
	    }
	    if (new_trans[n_new_trans-1] < 0){
		state->decoder_done = FAX_DECODE_DONE_ErrorPastWidth;
		save_state_and_return(state);
	    }

	    if (rl == EOL_RUN_LENGTH) {
		state->decoder_done = FAX_DECODE_DONE_ErrorBadEOL;
		save_state_and_return(state);
		break;
			/* break out of the switch, loop with the while */
	    }
	    length_acc=0;
	    goal = FAX_GOAL_AccumulateA1A2;
		/* in case I run out of data while getting a1-a2 distance */

	  case  FAX_GOAL_AccumulateA1A2:
	    get_a1a2(FAX_GOAL_FinishHoriz);
		/* If we have to return for data before getting the 	*/
		/* whole a1a2 distance, we want to return to the a1a2	*/
		/* accumulate state when we reenter the decoder. If	*/
		/* adjusting the bitstream forces us to return after	*/
		/* getting the last part of the a1a2 distance, then	*/
		/* we want to jump to state FAX_GOAL_FinishHoriz when	*/
		/* we reenter the decoder.				*/

	    /* handle various possible errors found by get_a1a2 */
	    if (goal == FAX_GOAL_FallOnSword) {
		state->decoder_done = FAX_DECODE_DONE_ErrorBadCode;
		save_state_and_return(state);
	    }
	    if (goal == FAX_GOAL_RecoverZero ) {
		state->decoder_done = FAX_DECODE_DONE_ErrorBadZero;
		save_state_and_return(state);
	    }
	    if (rl == EOL_RUN_LENGTH) {
		state->decoder_done = FAX_DECODE_DONE_ErrorBadEOL;
		save_state_and_return(state);
		break;
	    }

	  case  FAX_GOAL_FinishHoriz:
	    a1a2 = length_acc;
	    /* XXX - I may regret not checking for a1a2 > 0 later... */
	    if (a1a2 < 0) {
		state->decoder_done = FAX_DECODE_DONE_ErrorBada1a2;
		save_state_and_return(state);
	    }

	    new_trans[n_new_trans] = a0_pos = new_trans[n_new_trans-1]+a1a2;
	    ++n_new_trans;
	    if (a0_pos < 0) {
		state->decoder_done = FAX_DECODE_DONE_ErrorBadA0pos;
		save_state_and_return(state);
	    }

	    if (a0_pos >= width) {
		if (a0_pos > width) {
		   state->decoder_done = FAX_DECODE_DONE_ErrorPastWidth;
		   save_state_and_return(state);
		}
		FlushLineData();
		if (state->decoder_done)
	    		save_state_and_return(state);
	        goal = FAX_GOAL_StartNewLine;
		break;
	    }
	    else 
	        goal = FAX_GOAL_HandleHoriz;
	    break;


	  default:
	    state->decoder_done = FAX_DECODE_DONE_ErrorBadGoal;
	    save_state_and_return(state);
	  break;
	  }  /* end of switch */
	}
/***	End, Main Decoding Loop	***/
}
/**** module fax/dtiff2.c ****/
