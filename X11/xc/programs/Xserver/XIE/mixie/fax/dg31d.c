/* $Xorg: dg31d.c,v 1.4 2001/02/09 02:04:25 xorgcvs Exp $ */
/**** module fax/dg31d.c ****/
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
  
	fax/dg31d.c -- DDXIE G31D fax decode technique/element
  
	Ben Fahy -- AGE Logic, Inc. July, 1993
  
*****************************************************************************/


#define lenient_decoder


/* the folling define causes extra stuff to be saved in state recorder */
#define _G31D
#include "fax.h"
#include "faxint.h"
#include "bits.h"

#include <servermd.h> /* pick up the BITMAP_BIT_ORDER from Core X*/


/**********************************************************************/
int decode_g31d(state)
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

int 	lines_found=0;
int	n_old_trans;
int	n_new_trans;
int	*old_trans;
int	*new_trans;
int 	a0a1,a1a2;
int 	length_acc=0;
int 	last_b1_idx=0;
int 	code,nbits;

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
	  case FAX_GOAL_SkipPastAnyToEOL:
		code = 0;	/* get gnu compiler to shut up */
		while (byteptr < endptr) {
	    	  /* look for EOL code */
	    	  code = get_wcode(byteptr,bitpos,endptr);
	    	  rl     = _WhiteFaxTable[code].run_length;
	      	  if (rl == EOL_RUN_LENGTH) 
			break;
		  else
			/* move bitstream one bit further and try again */
 		 	adjust_1bit(byteptr,bitpos,endptr);
		}
		if (byteptr >= endptr) {
			state->decoder_done = FAX_DECODE_DONE_ErrorSkipPast;
			return(lines_found);
		}
	    	nbits  = _WhiteFaxTable[code].n_bits;
	    	goal = FAX_GOAL_StartNewLine;
	    	adjust_bitstream(nbits,byteptr,bitpos,endptr);
		break;

	  case FAX_GOAL_SeekFillAndEOL:
	    /* look for EOL code */
		code = 0;  /* get gnu compiler to shut up */
		while (byteptr < endptr) {
	    	  /* look for EOL code */
	    	  code = get_wcode(byteptr,bitpos,endptr);
	    	  rl     = _WhiteFaxTable[code].run_length;
	      	  if (rl == EOL_RUN_LENGTH) 
			break;
		  else if (code) {
		     state->decoder_done =  FAX_DECODE_DONE_ErrorBadFill;
		     return(lines_found);
		  }
		  else
			/* move bitstream one bit further and try again */
 		 	adjust_1bit(byteptr,bitpos,endptr);
		}
		if (byteptr >= endptr) {
		    state->decoder_done =  FAX_DECODE_DONE_ErrorBadPtr;
		    return(-1);
		}
	    	nbits  = _WhiteFaxTable[code].n_bits;
	    	goal = FAX_GOAL_StartNewLine;
		/* set goal before adjusting in case we run out of data */
	    	adjust_bitstream(nbits,byteptr,bitpos,endptr);
		break;

	  case FAX_GOAL_StartNewLine:

	    if (lines_found >= state->nl_sought)
		 save_state_and_return(state);

	    /* normal line initialization stuff */
	    a0_pos   = -1;
	    a0_color = WHITE;
	    goal = FAX_GOAL_HandleHoriz;

	    reset_transitions();
	    break;

	  case  FAX_GOAL_RecoverZero:
	    goal = FAX_GOAL_SkipPastAnyToEOL;
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
#if defined(lenient_decoder)
	        goal = FAX_GOAL_SkipPastAnyToEOL;
		FlushLineData();
	        break;

#else
		state->decoder_done = FAX_DECODE_DONE_ErrorBadCode;
		return(lines_found);
#endif
	    }

	  case  FAX_GOAL_RecordA0A1:
	    a0a1 = length_acc;
	    if (a0a1 < 0) {
		state->decoder_done = FAX_DECODE_DONE_ErrorBada0a1;
		return(-1);
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
#if defined(lenient_decoder)
		   /* we went too far, but we'll be forgiving */
		   a1_pos = width;
#else
		   state->decoder_done = FAX_DECODE_DONE_ErrorPastWidth;
		   return(-1);
#endif
		}
		FlushLineData();
	        goal = FAX_GOAL_SkipPastAnyToEOL;
		break;
	    }
	    if (new_trans[n_new_trans-1] < 0){
		state->decoder_done = FAX_DECODE_DONE_ErrorBadA0pos;
	       	return(-1);
	    }


	    if (goal == FAX_GOAL_RecoverZero) {

	        /* it's possible we got here by reading a zero in get_a0a1, */
	        /* in which case 'goal' was set to FAX_GOAL_RecoverZero. We */
	        /* must assume that the zero is an EOL pre-padded with a    */
	        /* variable number of zeros, which is legal according to    */
	        /* the coding spec.  So we will record the increment in     */
	        /* line number and then attempt to recover.		    */
	        goal = FAX_GOAL_SkipPastAnyToEOL;
		FlushLineData();
	        break;
			/* break out of the switch, loop with the while */
	    }

	    if (rl == EOL_RUN_LENGTH) {
		FlushLineData();
	        goal =  FAX_GOAL_StartNewLine;
		/* if we got a non-zero length, remember the transition */
		/* in case the next line is coded in vertical mode	*/
		if (a0a1)
		   new_trans[n_new_trans++] = a0a1;

		/* notice we don't have to worry about a0_pos - 
		   it will be reset when we start the new line
		*/
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

	  case  FAX_GOAL_FinishHoriz:
	    a1a2 = length_acc;
	    /* XXX - I may regret not checking for a1a2 > 0 later... */
	    if (a1a2 < 0) {
		state->decoder_done = FAX_DECODE_DONE_ErrorBada1a2;
		return(-1);
	    }


	    new_trans[n_new_trans] = a0_pos = new_trans[n_new_trans-1]+a1a2;
	    ++n_new_trans;
	    if (a0_pos < 0) {
		state->decoder_done = FAX_DECODE_DONE_ErrorBadA0pos;
		return(-1);
	    }

	    /* it's possible we got here by reading a zero in get_a1a2, */
	    /* in which case 'goal' was set to FAX_GOAL_RecoverZero. We */
	    /* must assume that the zero is an EOL pre-padded with a 	*/
	    /* variable number of zeros, which is legal according to	*/
	    /* the coding spec.  So we will update the line count and	*/
	    /* then attempt to recover.					*/
	    if (goal == FAX_GOAL_FallOnSword) {
#if defined(lenient_decoder)
	        goal = FAX_GOAL_SkipPastAnyToEOL;
		FlushLineData();
	        break;

#else
		state->decoder_done = FAX_DECODE_DONE_ErrorBadCode;
		return(lines_found);
#endif
	    }
	    if (goal == FAX_GOAL_RecoverZero ) {
	        /* assume we have hit the begining of an EOL */
	        /* goal = FAX_GOAL_SeekFillAndEOL; */
	        goal = FAX_GOAL_SkipPastAnyToEOL;
		FlushLineData();
	        break;
	    }
	    if (rl == EOL_RUN_LENGTH) {
		FlushLineData();
	        goal =  FAX_GOAL_StartNewLine;
		if (state->decoder_done)
	    		save_state_and_return(state);
		break;
	    }
	    if (a0_pos >= width) {
		if (a0_pos > width) {
#if defined(lenient_decoder)
		   /* we went too far, but we'll be forgiving */
		   a0_pos = width;
#else
		   state->decoder_done = FAX_DECODE_DONE_ErrorPastWidth;
		   return(lines_found);
#endif
		}
		FlushLineData();
		if (state->decoder_done)
	    		save_state_and_return(state);
		/* goal = FAX_GOAL_SeekFillAndEOL; */
	        goal = FAX_GOAL_SkipPastAnyToEOL;
		break;
	    }
	    else {
	        goal = FAX_GOAL_HandleHoriz;
	    }
	    break;


	  default:
	    state->decoder_done = FAX_DECODE_DONE_ErrorBadGoal;
	    return(lines_found);
	  break;
	  }  /* end of switch */
	}
/***	End, Main Decoding Loop	***/
}
/**** module fax/g31d.c ****/
