/* $Xorg: etiffpb.c,v 1.4 2001/02/09 02:04:25 xorgcvs Exp $ */
/* AGE Logic - Oct 15 1995 - Larry Hare */
/**** module etiffpb.h ****/
/******************************************************************************

Copyright 1993, 1994,1998  The Open Group

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

	etiffpb.c -- DDXIE module for encoding TIFF PackBits format

	Ben Fahy, AGE Logic, Sept 1993 

******************************************************************************/


#include "fax.h"
#define _PBits
#include "fencode.h"
#include "xiemd.h"

/************************************************************************/
encode_tiffpb(state)
FaxEncodeState *state;
{
/* stuff exchanged with state variable */
register int goal;
register unsigned char *byteptr;
register unsigned char *endptr;
register int width;
register int rl;
PackBitsEncodePvt	*epvt;

/* stuff exchanged with private state variable */
int index;
int start;
int nlits;
int nvals;
register int *counts;
register int *values;


register int 	lines_coded=0;
register int	lines_to_code=0;
register int 	rlcode,lits_to_write;
register int	bytes_out;

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
	    state->bits.bitpos = 0;
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

	  case ENCODE_PB_GOAL_StartNewLine:
	    /* every new line we need to compute its runlength data */

	    { 
	    register unsigned char *ibptr=NULL;
	    register unsigned char *ebptr=NULL;
	    register int color0,color1, n_reps;

	    if (lines_coded >= state->nl_tocode) {
		/* We have read the whole input strip, need more data  */
		save_state_and_return(state);
	    }

	    ibptr = (unsigned char *) state->i_lines[lines_coded];
	    ebptr = ibptr+(width+7)/8;

#if (IMAGE_BYTE_ORDER == MSBFirst)
	    color0 = *ibptr++;
#else
	    color0 = _ByteReverseTable[*ibptr++];
#endif
	    n_reps = 1;
	    nvals = 0;
	    epvt->values[nvals] = color0;

	    while (ibptr < ebptr) {
#if (IMAGE_BYTE_ORDER == MSBFirst)
		color1 = *ibptr++;
#else
		color1 = _ByteReverseTable[*ibptr++];
#endif
		if (color1 == color0) ++n_reps;
		else {
			epvt->counts[nvals++] = n_reps;
			color0 = color1;
			n_reps = 1;
			epvt->values[nvals] = color0;
		}
	    }
	    epvt->counts[nvals++] = n_reps;
	    epvt->nvals = nvals;

	    }
	    /* now that we have an encoding of the line in runlengths,
	       we need to decide what we write out as R V,  where R
	       is the runlength and V is the value, vs L V1 V2 ... VL
	       where Vi are individual values and L is the number of 
	       literal values that follow.  There isn't really any 
	       advantage to coding runlengths of size 2 or less, so
	       we will only code as a runlength for runs of 3 or more.
	    */
	    index = 0;	/* where we are in our list of runs and values */
	    start = 0;  /* where we were when we started */
	    nlits = 0;  /* number of literals we will concatenate */
	    goal = ENCODE_PB_GOAL_OutputNextRunLength;

	  case ENCODE_PB_GOAL_OutputNextRunLength:
	    /* starting from the current index, search for the first
	       runlength of three or more. Everything before then will
	       be encoded as a string of literals.
	    */

	    start = index;
	    while (index < nvals && (rl = counts[index]) < 3) {
		nlits += rl;
		++index;
	    }

	    if (start != index) {
	       /* Note: we can never call FlushLiterals when start >= nvals,
		  because index <= nvals,  and start < index if here */
	       goal = ENCODE_PB_GOAL_FlushLiterals;
	    }
	    else {
		/* found a suitable runlength */
		goal = ENCODE_PB_GOAL_FlushRunLength;
	    }
	    break;

	  case ENCODE_PB_GOAL_FlushRunLength:
	    if (!rl) {
	       state->encoder_done = ENCODE_ERROR_BadRunLength;
	       save_state_and_return(state);
	    }
	    /* Packbits represents runlengths as rl = 1 - rlcode, where */
	    /* rlcode is a negative number between -1 and -127.  So we  */
	    /* may have to break a number larger than 128 into multiple */
	    /* runlengths */
	    if (rl <= 128) {
		   rlcode = 1-rl;		/* encode the runlength */
		   rl = 0;		
	    }
	    else {
		   rlcode = -127;		/* encode the maximum 	*/
		   rl -= 128;
	    } 
	    /* before writing, make sure we aren't off end of strip 	*/
	    if ( byteptr >= endptr) {
	       goal = ENCODE_PB_GOAL_WriteCodeForRunLength;
	       state->strip_state = StripStateDone;
	       state->magic_needs = 1;
	       save_state_and_return(state);
	    }

	  case ENCODE_PB_GOAL_WriteCodeForRunLength:
	    *byteptr++ = rlcode;

	    /* before writing, make sure we aren't off end of strip 	*/
	    if ( byteptr >= endptr) {
	       goal = ENCODE_PB_GOAL_WriteValueForRunLength;
	       state->strip_state = StripStateDone;
	       state->magic_needs = 1;
	       save_state_and_return(state);
	    }
	  case ENCODE_PB_GOAL_WriteValueForRunLength:
	    *byteptr++ = values[index];
	    if (!rl) {
		if (++index >= nvals) {		/* see if done with line */
		   ++lines_coded;
		   goal = ENCODE_PB_GOAL_StartNewLine;
		}
		else
		   goal = ENCODE_PB_GOAL_OutputNextRunLength;
	    }
	    else 
	        goal = ENCODE_PB_GOAL_FlushRunLength; 
		break; 
	  case ENCODE_PB_GOAL_FlushLiterals: 
	    /* Packbits represents literal lengths as rl = 1 + rlcode, 	*/
	    /* rlcode is a number between 0 and 127.  So we have to break  */
	    /* break numbers larger than 128 into multiple runlengths	*/
	    if (nlits <= 128) {
		   rlcode = nlits-1;		/* encode the runlength */
		   nlits = 0;		
	    }
	    else {
		   rlcode = 127;		/* encode the maximum */
		   nlits -= 128;
	    } 
	    rl = 1 + rlcode;	/* need this for comparison later 	*/
	    bytes_out = 0;	/* ditto. bytes_out must be <= rl 	*/

	    /* before writing, make sure we aren't off end of strip 	*/
	    if ( byteptr >= endptr) {
	       goal = ENCODE_PB_GOAL_WriteNlits;
	       state->strip_state = StripStateDone;
	       state->magic_needs = 1;
	       save_state_and_return(state);
	    }

	  case ENCODE_PB_GOAL_WriteNlits:
	    *byteptr++ = rlcode;
	    goal = ENCODE_PB_GOAL_WriteLiterals;
		/* note that lits_to_write is always 1 or 2 */

	  case ENCODE_PB_GOAL_WriteLiterals:
	    /* this is actually a loop. 'start' gets incremented each 	*/
	    /* time through until it is equal to 'index'. We never get	*/
	    /* to this point unless start < nvals, though 		*/

	    lits_to_write = counts[start];
	    goal = ENCODE_PB_GOAL_WriteThisLiteral;

	  case ENCODE_PB_GOAL_WriteThisLiteral:
	    /* before writing, make sure we aren't off end of strip 	*/
	    if ( byteptr >= endptr) {
	       goal = ENCODE_PB_GOAL_WriteTheLiteral;
	       state->strip_state = StripStateDone;
	       state->magic_needs = 1;
	       save_state_and_return(state);
	    }

	  case ENCODE_PB_GOAL_WriteTheLiteral:
	    *byteptr++ = values[start];
	    if (++bytes_out >= 128 && nlits) {
		/* we have written the maximum amount we can	*/
		/* write at a time, yet we know there are more  */
		/* to write out. 				*/

		goal = ENCODE_PB_GOAL_FlushLiterals;

		/* If we need to write current value some more, */
		/* change count so we don't write extra		*/

		if (--lits_to_write) 
		   counts[start] = lits_to_write;
		else {
		   /* otherwise,  advance position	*/
		   ++start;
		}

		/* note that we KNOW we have to write more literals */
		/* so it is safe to increment 'start', which could  */
		/* only be equal to 'index' if we were done	    */

		break;
	    }
	    else if (--lits_to_write) {
		/* loop back and write this value again */
		goal = ENCODE_PB_GOAL_WriteThisLiteral;
		break;
	    }

	    /* If here, we are done with this literal */
	    if (++start < index)  {
	       /* if other literals to write, loop back */
	       goal = ENCODE_PB_GOAL_WriteLiterals;
	       break;
	    }

	    /* ok,  we wrote out all the literals */

	    if (index >= nvals) {		/* see if done with line */
		++lines_coded;
		goal = ENCODE_PB_GOAL_StartNewLine;
	    }
	    else
		goal = ENCODE_PB_GOAL_OutputNextRunLength;
	    break;

	  default:
	    state->encoder_done = ENCODE_ERROR_BadMagic;
	    save_state_and_return(state);
	    break;

	  } /* end of switch(goal) in main decoding loop */

	} /* end of main encoding loop */
}
/************************************************************************/

