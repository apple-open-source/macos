/* $Xorg: etiff2.c,v 1.4 2001/02/09 02:04:25 xorgcvs Exp $ */
/**** module etiff2.c ****/
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

	etiff2.c -- DDXIE module for encoding tiff2 format

	Ben Fahy, AGE Logic, Sept 1993 

******************************************************************************/

#include "fax.h"
#define _TIFF2
#include "fencode.h"
#include "xiemd.h"
#define FETABS_OWNER
#include "fetabs.h"

/************************************************************************/
encode_tiff2(state)
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
Tiff2EncodePvt	*epvt;

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
	    /* every new line we need to compute its runlength data */

	    if (lines_coded >= state->nl_tocode) {
		/* We have read the whole input strip, need more data  */
		save_state_and_return(state);
	    }

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
	    a0_color = WHITE;

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
	     if (bitpos) {
		/* have to flush stager */
	        if (byteptr >= endptr) {
	       	    state->strip_state = StripStateDone;
	       	    state->magic_needs = 1;
		    save_state_and_return(state);
	        }
		/* make sure nothing bogus is happening */
		if (bitpos > 8) {
		    state->encoder_done = ENCODE_ERROR_BadStager;
		    save_state_and_return(state);
		}

	        *byteptr++ = (stager&0xff000000) >> 24;
	        stager <<= 8;
	        bitpos = 0;
	     }
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
int encode_runs(src,nbits,counts,white_is_one,width)
LogInt *src;
unsigned int *counts;
register int nbits;
register int white_is_one;
register int width;
{
register unsigned int white_count=0,black_count=0;
register int i,current_is_white=1;
register int n_runs=0;
register int total=0;

register LogInt test_bit = LOGLEFT;
	/* LOGLEFT = the logically left-most bit in a word in 	*/
	/* the server, we allways fill the logically left-most	*/
	/* bit in a word first,  the logically right-most last	*/

/***	handle first bit differently than others	***/

    if (white_is_one) {	
	for (i=0; i<nbits; ++i) {
	   if (!test_bit) 
		test_bit = LOGLEFT;
	   
	   if (current_is_white) {
	        if (LOG_tstbit(src,i))
		   ++white_count;
		else {
		   counts[n_runs++] = white_count;
		   total += white_count;
		   current_is_white = 0;
		   black_count = 1;
		}
	   }  	/* end current_is_white */
	   else {
	        if (!LOG_tstbit(src,i))
		   ++black_count;
		else {
		   counts[n_runs++] = black_count;
		   total += black_count;
		   current_is_white = 1;
		   white_count = 1;
		}
	   }  	/* end current_is_not_white */

	   LOGRIGHT(test_bit);
		/* go to the next-filled bit in the word */

	}  /* end for (i<nbits)    */
    }      /* end if (white_is_one) */

    else {			/* white is zero */
	for (i=0; i<nbits; ++i) {
	   if (!test_bit) 
		test_bit = LOGLEFT;
	   
	   if (current_is_white) {
	        if (!LOG_tstbit(src,i))
		   ++white_count;
		else {
		   counts[n_runs++] = white_count;
		   total += white_count;
		   current_is_white = 0;
		   black_count = 1;
		}
	   }  	/* end current_is_white */
	   else {
	        if (LOG_tstbit(src,i))
		   ++black_count;
		else {
		   counts[n_runs++] = black_count;
		   total += black_count;
		   current_is_white = 1;
		   white_count = 1;
		}
	   }  	/* end current_is_not_white */

	   LOGRIGHT(test_bit);
		/* go to the next-filled bit in the word */

	}  /* end for (i<nbits)    */

    }      /* end else (!white_is_one) */

    if (current_is_white && white_count)  {
	counts[n_runs++] = white_count;
	total += white_count;
    }
    else if (!current_is_white && black_count)  {
	counts[n_runs++] = black_count;
	total += black_count;
    }
    if (total != width) 
       	return(-1);
    else
    	return(n_runs);
}
/************************************************************************/
int deduce_code(a0_color,rl,rlcode,terminating)
int a0_color,*rl,*rlcode,*terminating; 
{
int nbits;
ShiftedCodes *table;

	if (a0_color == WHITE)
		table = ShiftedWhites;
	else
		table = ShiftedBlacks;

	if (*rl < 0) 
		return(-1);

	if (*rl <= 63) {
		*terminating = 1;
		*rlcode = table[*rl].code;
		nbits = table[*rl].nbits;
		*rl = 0;
	}
	else if (*rl <= 2560) {
		*terminating = 0;
		*rlcode = table[63 +  (*rl >> 6)].code;
		nbits   = table[63 +  (*rl >> 6)].nbits;
		*rl %= 64;
	}
	else {
		*terminating = 0;
		*rlcode = table[63 + 2560/64].code;
		nbits   = table[63 + 2560/64].nbits;
		*rl -= 2560;
	}
	return(nbits);
}
/************************************************************************/
