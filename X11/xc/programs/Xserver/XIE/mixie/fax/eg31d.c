/* $Xorg: eg31d.c,v 1.4 2001/02/09 02:04:25 xorgcvs Exp $ */
/**** module eg31d.c ****/
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

	eg31d.c -- DDXIE module for encoding g31d format

	Ben Fahy, AGE Logic, Oct 1993 

	XXX - this module should be merged with etiff2.c - the only
		difference is the way EOL's are coded.

******************************************************************************/

#include "fax.h"
#define _G31D
#include "fencode.h"
#include "xiemd.h"
#include "fetabs.h"

/************************************************************************/
encode_g31d(state)
FaxEncodeState *state;
{
/* stuff exchanged with state variable */
register int goal;
register unsigned char *byteptr;
register unsigned char *endptr;
register int bitpos;
register int width;
register int stager;
int rl;
register int a0_color;
G31DEncodePvt	*epvt;

/* stuff exchanged with private state variable */
register int index;
register int nvals;
register int *counts;
register int nbits;
int rlcode;
int terminating;
register int codelength;

register int 	lines_coded=0;
register int	lines_to_code=0;

int old_rl;

	if (state == (FaxEncodeState *) NULL)
		return(-1);

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
	      return(-1);
	  }
	  state->magic_needs = 0;
	}

	localize_state(state);


/***	Main Encoding Loop	***/
	while (1) {

	  switch(goal) {

	  case ENCODE_FAX_GOAL_StartNewLine:
	    /* every new line need to write an EOL. Should call it BOL,huh? */

	    if (epvt->align_eol) {
		/* advance bitpos to 4 + 8k, where k is an integer */
		bitpos = 4 + 8*( (bitpos+3)>>3 );
	    }

	    stager |= (EOL_CODE << (16-bitpos));
	    bitpos += EOL_BIT_LENGTH;

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

	  case ENCODE_FAX_GOAL_OutputNextRunLength:
	    rl = counts[index++];
	    old_rl = rl;

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
		if (index < nvals) {
		    a0_color = ~a0_color;
	  	    goal = ENCODE_FAX_GOAL_OutputNextRunLength;
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
		if (index < nvals) {
		    a0_color = ~a0_color;
	  	    goal = ENCODE_FAX_GOAL_OutputNextRunLength;
		    break;
		}
		else {
	  	    goal = ENCODE_FAX_GOAL_FinishLine;
		    break;
		}
	    }
	    /* not done with this runlength yet */
	    goal = ENCODE_FAX_GOAL_DeduceCode;
	    break;

	  case ENCODE_FAX_GOAL_FinishLine:
	     ++lines_coded;
	     goal = ENCODE_FAX_GOAL_StartNewLine;
	     break;

	  default:
	    state->encoder_done = ENCODE_ERROR_BadMagic;
	    save_state_and_return(state);
	    break;

	  } /* end of switch(goal) in main decoding loop */

	} /* end of main encoding loop */
}
/************************************************************************/
