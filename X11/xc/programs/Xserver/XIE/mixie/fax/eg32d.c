/* $Xorg: eg32d.c,v 1.4 2001/02/09 02:04:25 xorgcvs Exp $ */
/**** module eg32d.c ****/
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
******************************************************************************

	eg32d.c -- DDXIE module for encoding g32d format. Since
			g42d is so similar,  we also do that in
			this module. 

	Ben Fahy, AGE Logic, Oct 1993 

******************************************************************************/

#include "fax.h"
#define _G32D
#include "fencode.h"
#include "xiemd.h"
#include "fetabs.h"

/************************************************************************/
encode_g32d(state)
FaxEncodeState *state;
{
/* stuff exchanged with state variable */
register int goal,next_goal;
register unsigned char *byteptr;
register unsigned char *endptr;
register int bitpos;
register int width;
CARD32 stager;
unsigned rl;
register int a0_color,b1_color;
register int a0_pos,a1_pos,a2_pos,b1_pos,b2_pos;
register int length_acc;
register int a0a1,a1a2;
G32DEncodePvt	*epvt;

/* stuff exchanged with private state variable */
register int index,aindex;
register int save_index,save_aindex;
register int nvals,avals;
register int *counts;
register int *above;
register int nbits;
int rlcode;
int terminating;
register int codelength;
register int k,kcnt,save_b1pos,save_b1color;

register int 	lines_coded=0;
register int	lines_to_code=0;
register int a1b1,abs_a1b1;
int old_rl;

	if (state == (FaxEncodeState *) NULL) {
		return(-1);
	}

	/* set up initial bitstream for the very first output strip */
	if (!state->bits.started) {
	    if (state->strip_state != StripStateNew)  {
	        state->encoder_done = ENCODE_ERROR_StripStateNotNew;
		return(-1);
	    }
	    
	    state->bits.byteptr = (unsigned char *)state->strip;
	    state->bits.endptr  = state->bits.byteptr + state->strip_size;
	    	/* record end of output strip */

	    state->bits.bitpos = 0;
	    state->bits.started = 1;
	    state->strip_state = StripStateInUse; 
	}

	/* or, reset bitstream for a new output strip */
	if (state->magic_needs)  {
	  if (state->strip_state == StripStateNew)  {
	    state->strip_state = StripStateInUse;
	    state->bits.byteptr = (unsigned char *)state->strip;
	    state->bits.endptr  = state->bits.byteptr + state->strip_size;
	    	/* record end of output strip */
	    state->bits.started = 1;
	  }
	  else {
	     /* no new strip?? But... I *need* more data! */
	      	state->encoder_done = ENCODE_ERROR_BadMagic;
		return(0);
	  }
	  state->magic_needs = 0;
	}

	localize_state(state);
		/* loads epvt and other variables */


/***	Main Encoding Loop	***/
	while (1) {

	  switch(goal) {

	  case ENCODE_FAX_GOAL_StartNewLine:
	    /* every new line need to write an EOL. Should call it BOL,huh? */

	    /* make old counts the new reference line */
	    {
	    int *foo;
	    foo = counts;
	    counts = above;
	    above = foo;
	    avals = nvals;
	    }

	    if (epvt->really_g4) {
		/* G4 doesn't write the silly EOL's except at very end */
	  	goal = ENCODE_FAX_GOAL_G4SneaksIn;
		break;
	    }

	    if (epvt->align_eol) {
		/* advance bitpos to 4 + 8k, where k is an integer */
		bitpos = 4 + 8*( (bitpos+3)>>3 );
	    }

	    if (kcnt % k) {

		/* need to encode in 2-D mode */
		rlcode = EOL_2D_CODE;

	       stager |= (EOL_2D_CODE << (16-bitpos));
	       bitpos += EOL_2D_BIT_LENGTH;

	    }
	    else {
		rlcode = EOL_1D_CODE;
	    	stager |= (EOL_1D_CODE << (16-bitpos));
	    	bitpos += EOL_1D_BIT_LENGTH;
	    }

	    goal = ENCODE_FAX_GOAL_FlushEOL;

	  case ENCODE_FAX_GOAL_FlushEOL:
	    if (byteptr >= endptr) {
	        state->strip_state = StripStateDone;
	        state->magic_needs = 1;
		save_state_and_return(state);
	    }
	    /* if here, we know it is safe to flush at least one byte */
	    *byteptr++ = (stager&0xff000000) >> 24;
	    stager <<= 8;
	    bitpos -= 8;
	    if (bitpos >=8) {
		/* go around and flush some more */
		break;
	    }

	  goal = ENCODE_FAX_GOAL_EOLWritten;

	  case ENCODE_FAX_GOAL_G4SneaksIn:
	    if (lines_coded >= state->nl_tocode) {
		/* We have read the whole input strip, need more data  */
		save_state_and_return(state);
	    }

	  case ENCODE_FAX_GOAL_EOLWritten:

	    /* every new line we need to compute its runlength data */
	    /* get the sequence of white and black run lengths */
	    nvals = encode_runs(
		state->i_lines[lines_coded],	/* input data */
		width,				/* # of bits  */
		counts,				/* where to put lengths */
		state->radiometric,		/* if 1, white is 1 */
		width
	    );
	    if (nvals < 0) {
	        state->encoder_done = ENCODE_ERROR_EncodeRunsFailure;
		save_state_and_return(state);
	    }
	    a0_color = WHITE;
	    index = 0;
	    if (epvt->really_g4 || kcnt % k) {
	    	aindex = 0;
		a0_pos = -1;
		b1_color = WHITE;
		b1_pos = 0;
		goal = ENCODE_FAX_GOAL_FindPositions;
		length_acc = 0;
		break;
	    }

	  case ENCODE_FAX_GOAL_OutputNextRunLength:
	    rl = counts[index++];
	    old_rl = rl;
	    next_goal = ENCODE_FAX_GOAL_OutputNextRunLength;
	       /* after we write code for this runlength, go to next */

	  case ENCODE_FAX_GOAL_DeduceCode:
	    nbits = deduce_code(a0_color,&rl,&rlcode,&terminating);
		/* should be a macro but for now, a function */
	    
	    /* shift code over so it merges with stager contents */

	    stager |= (rlcode << (16-bitpos));
	    bitpos += nbits;


	    if (bitpos >=8) {
		goal = ENCODE_FAX_GOAL_FlushStager;
		break;
	    }
	    if (terminating) {
		/* all done with this runlength */
		if (next_goal != ENCODE_FAX_GOAL_OutputNextRunLength) {
			goal = next_goal;
			break;
		}

		if (index < nvals) {
		    a0_color = ~a0_color;
	  	    goal = next_goal;
		    break;
		}
		else {
	  	    goal = ENCODE_FAX_GOAL_FinishLine;
		    break;
		}
	    }
	    /* go around the loop again */
	    goal = ENCODE_FAX_GOAL_DeduceCode;
	    old_rl = rl;
	    break;

	  case ENCODE_FAX_GOAL_FlushStager:
	    if (byteptr >= endptr) {
	        state->strip_state = StripStateDone;
	        state->magic_needs = 1;
		save_state_and_return(state);
	    }
	    /* if here, we know it is safe to flush at least one byte */
	    *byteptr++ = (stager&0xff000000) >> 24;
	    stager <<= 8;
	    bitpos -= 8;
	    
	    if (bitpos >= 8) {
		/* go around the loop again, trying to flush more */
		break;
	    }
	    /* if here, we have flushed stager to the point there is	*/
	    /* at most a partial byte of data left in it.		*/
	    if (terminating) {
		/* all done with this runlength */
		if (next_goal != ENCODE_FAX_GOAL_OutputNextRunLength) {
		    goal = next_goal;
		    break;
		}

		if (index < nvals) {
		    a0_color = ~a0_color;
	  	    goal = next_goal;
		    break;
		}
		else {
	  	    goal = ENCODE_FAX_GOAL_FinishLine;
		    break;
		}
	    }
	    /* not done with this runlength yet */
	    goal = ENCODE_FAX_GOAL_DeduceCode;
	    old_rl = rl;
	    break;

	  case ENCODE_FAX_GOAL_FinishLine:
	     ++lines_coded;
	     ++kcnt;
	     goal = ENCODE_FAX_GOAL_StartNewLine;
	     break;

	  case ENCODE_FAX_GOAL_FindPositions:
	    save_index = index;
	    save_aindex = aindex;
	    save_b1pos = b1_pos;
	    save_b1color = b1_color;
	       /* it's easier to just remember these than to back up */
	    if (index >= nvals) {
	       a1_pos = a2_pos = state->width;
	       ++index;
		 /* this is for the sake of vertical mode below */
	    }
	    else {
	       if (a0_pos < 0)
		   a1_pos = counts[index++];
	       else
	           a1_pos = a0_pos + (counts[index++] - length_acc);
		   /* note: we need to subtract length_acc in case
		      we hit one or more pass modes for this index,
		      in which case a0_pos was incremented part of
		      the distance between a0 and a1...
		   */
	    }

	    if (index >= nvals) {
	       a2_pos = state->width;
	       ++index;
		 /* this is just so we can subtract to get a1's index */
	    }
	    else
	       a2_pos = a1_pos + counts[index++];

	    /* now find b1, which is first changing element to right 
	       of a0 of opposite color */

	    while (aindex < avals && 
		  (b1_color == a0_color || b1_pos <= a0_pos) ) {
		b1_pos += above[aindex++];
		b1_color = ~b1_color;
	    }
	    if (aindex >= avals) {

		/* if no b1 found, set it just past normal image data	*/
		/* now, the sum of all 'above[index]'s should be equal	*/
		/* to the image width, so this gives us a check that 	*/
		/* the runlength algorithm is working 			*/

		if (b1_pos != width) {
	           state->encoder_done = ENCODE_ERROR_NoB1Found;
		   save_state_and_return(state);
		}
		b2_pos = width+1;
	    }
	    if (aindex >= avals) {
	       if (b1_pos == width)
	          b2_pos = state->width + 1;
	       else
	          b2_pos = state->width;
	    }
	    else
	       b2_pos = b1_pos + above[aindex];

	    if (b2_pos < a1_pos) {
		goal = ENCODE_FAX_GOAL_PassMode;
		break;
	    }
	    length_acc = 0;
		/* we won't linger on this a0 anymore */

	    a1b1 = b1_pos-a1_pos;
	    abs_a1b1 = (a1b1 >=0)? a1b1 : -a1b1;
	    if (abs_a1b1 <= 3) {
		goal = ENCODE_FAX_GOAL_VerticalMode;
	    }
	    else  
		goal = ENCODE_FAX_GOAL_HorizontalMode;
	    break;
	    

	  case ENCODE_FAX_GOAL_PassMode:
	    /* pass mode is nice and simple. a0_pos = b2_pos */

	    rlcode = PASS_CODE;
	    nbits = PASS_CODE_BIT_LENGTH;

	    stager |= (rlcode << (16-bitpos));
	    bitpos += nbits;

	    /* before worrying about flushing,  let's update positions */
	    if (a0_pos < 0)
		length_acc = b2_pos;
	    else
	    	length_acc += b2_pos - a0_pos;
		/* don't count passed length twice */

	    a0_pos = b2_pos;
	    index = save_index;

	    b1_pos = b2_pos;
		/* new b1_position will be old b2_position */
	    b1_color = ~b1_color;
	    ++aindex;

	    if (bitpos >=8)  
	        goal = ENCODE_FAX_GOAL_Flush2DStager;
	    else {
		
		if (a0_pos >= width) {
		   goal = ENCODE_FAX_GOAL_FinishLine;
		}
		else {
		    goal = ENCODE_FAX_GOAL_FindPositions;
		}
	    }
	    break;

	  case ENCODE_FAX_GOAL_HorizontalMode:

	    rlcode = HORIZONTAL_CODE;
	    nbits  = HORIZONTAL_CODE_LENGTH;

	    stager |= (rlcode << (16-bitpos));
	    bitpos += nbits;

	    if (bitpos < 8) {
		goal = ENCODE_FAX_GOAL_FlushedHCode;
		break;
	    }
	    goal = ENCODE_FAX_GOAL_FlushHCode;

	    if (byteptr >= endptr) {
	        state->strip_state = StripStateDone;
	        state->magic_needs = 1;
		save_state_and_return(state);
	    }

	  case ENCODE_FAX_GOAL_FlushHCode:

	    /* if here, we know it is safe to flush at least one byte */
	    *byteptr++ = (stager&0xff000000) >> 24;
	    stager <<= 8;
	    bitpos -= 8;
	    
	    if (bitpos >= 8) {
		/* go around the loop again, trying to flush more */
		break;
	    }
	    /* if here, we have flushed stager */

	  case ENCODE_FAX_GOAL_FlushedHCode:
	    a0a1 = a1_pos - a0_pos;
	    if (a0_pos == -1)
		a0a1 -= 1;
	    a1a2 = a2_pos - a1_pos;
	    rl = old_rl = a0a1;
	    goal = ENCODE_FAX_GOAL_DeduceCode;
		/* will loop through writing out the code for a0a1 */

	    next_goal = ENCODE_FAX_GOAL_DoneWithA0A1;
		/* when done writing a0a1 codes, will go to DoneWithA0A1 */

	    /* before going on, let's set up for the next iteration */
	    a0_pos = a2_pos;
	       /* a0 skips to a2,  index adjusted automatically */

	    b1_pos = save_b1pos;
	    b1_color = save_b1color;
	       /* it's too painful to figure out what b1 was */

	    aindex = save_aindex;
	       /* we don't know enough to advance b1 */
	    break;

	  case ENCODE_FAX_GOAL_DoneWithA0A1:
	    rl = old_rl = a1a2;
	    a0_color = ~a0_color;  
	       /* here, "a0_color" represents color of next rl  */
	    goal = ENCODE_FAX_GOAL_DeduceCode;
		/* will loop through writing out the code for a0a1 */
	    next_goal = ENCODE_FAX_GOAL_DoneWithA1A2;
		/* when done writing a1a2 codes, will go to DoneWithA1A2 */
	    break;

	  case ENCODE_FAX_GOAL_DoneWithA1A2:
	    a0_color = ~a0_color;  
	       /* restore "a0_color" to really meaning a0 color */
	    if (a0_pos >= width) {
	       	goal = ENCODE_FAX_GOAL_FinishLine;
	    }
	    else {
		goal = ENCODE_FAX_GOAL_FindPositions;
	    }
	    break;

	  case ENCODE_FAX_GOAL_VerticalMode:

	    DEDUCE_Vcode(a1b1,rlcode,nbits);

	    stager |= (rlcode << (16-bitpos));
	    bitpos += nbits;

	    /* before picking next state, set new a0_position and
	       point indexes into 'count' and 'above' to the right
	       place.  saved indexes help a lot here.
	    */
	    a0a1 = a1_pos - a0_pos;
	    a0_pos = a1_pos;

	    if (a1b1 > 0) {
		/* b1 is to the right of a1.  It is possible that
		   when we set a0' = a1, b1' will be left of b1:

		    0   1   2   3   4
			       b1'  b1
		    O   O   0   X   0
		    O   X   O   X
		       a0  a1
			   a0'

		   In the example above,  b1 for the original a0 at 1
		   is the X->O transition at 4.  When a1 is coded, and
		   a1 becomes the new a0 (a0'), the O->X transition at
		   3 is b1'.  In other words, the progression of b1 is
		   *not* monotonic!!  So we have to be careful...

		   The condition under which backup is necessary is
		   when b1 is 2 or 3 to the right of a1,  and there
		   was a transition just before b1, right of a1. To 
		   get the next position on a line, we always take 
		   
		      new_pos = old_pos + above[index++]

		   Therefore, the previous position was 

		      old_pos = new_pos - above[index-1]

		    We need to back up if above[index-1] < a1b1.
		    If we incremented index twice to get to b2,
		    then we have to consider above[index-2]

		*/
		
		/* things are simple if b1_pos was off the edge */
		if (b1_pos >= width && avals>=2 && above[avals-1] < a1b1) {
			/* if b1_pos was off the edge, above[avals-1]  */
			/* added to the previous b1 got us there. So   */
			/* we get old b1 by subtracting above[avals-1] */
			b1_pos = width - above[avals-1];
			aindex = avals-2;
			b1_color = ~b1_color;
		}
		else if (aindex >= 1 && above[aindex-1] < a1b1) {
			b1_pos = b1_pos - above[aindex-1];
			    /* note: we don't increment aindex after 	  */
			    /* computing b2_pos = b1_pos + above[aindex], */
			    /* so above[aindex-1] is how we got b1_pos	  */
			aindex = aindex-1;
			    /* index for element before the b1 */
			b1_color = ~b1_color;
			    /* previous color */
		}

	    }
	    else {
		/* b1 is left of or over a1.  

		    0   1   2   3   4
			       b1 
		    O   O   X   0   
		    O   X   X   X   O   X
		       a0           a1
			            a0'

		    Since the b1' for a0' must be to the right of a0'=a1,
		    b1' must be to the right of b1. However, if we set
		    our position to b2, we may go too far, so stay where
		    we are.  Be coo'!
		*/
		
		/* aindex was updated while seeking b2 */
	    }
	    a0_color = ~a0_color;
	    --index; 
	      /* in code above, we made sure we always increment index  */
	      /* after setting a2, even if a1 or a2 was off end of line */ 

	    if (bitpos >=8)  
	        goal = ENCODE_FAX_GOAL_Flush2DStager;
	    else {
		if (a0_pos >= width) {
		   goal = ENCODE_FAX_GOAL_FinishLine;
		}
		else {
		    goal = ENCODE_FAX_GOAL_FindPositions;
		}
	    }
	    break;

	  case ENCODE_FAX_GOAL_Flush2DStager:
	    if (byteptr >= endptr) {
	        state->strip_state = StripStateDone;
	        state->magic_needs = 1;
		save_state_and_return(state);
	    }
	    /* if here, we know it is safe to flush at least one byte */
	    *byteptr++ = (stager&0xff000000) >> 24;
	    stager <<= 8;
	    bitpos -= 8;
	    
	    if (bitpos >= 8) {
		/* go around the loop again, trying to flush more */
		break;
	    }
	    /* if here, we have flushed stager to the point there is	*/
	    /* at most a partial byte of data left in it.		*/
	    if (a0_pos >= width) {
		goal = ENCODE_FAX_GOAL_FinishLine;
	    }
	    else {
		goal = ENCODE_FAX_GOAL_FindPositions;
	    }
	    break;

	  default:
	    state->encoder_done = ENCODE_ERROR_BadMagic;
	    save_state_and_return(state);

	  } /* end of switch(goal) in main decoding loop */

	} /* end of main encoding loop */
}
/************************************************************************/
