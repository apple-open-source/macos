/* $Xorg: dtiffpb.c,v 1.4 2001/02/09 02:04:25 xorgcvs Exp $ */
/* AGE Logic - Oct 15 1995 - Larry Hare */
/**** module fax/tiffpb.c ****/
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
  
	fax/tiffpb.c -- DDXIE TIFF Packbits decode technique/element
  
	Packbits is output-type independent.  The compression algorithm
	just compresses a stream. The 'strip_size' parameter should be 
	how many bytes long the strip is...

	Coding assumption:  it is always valid to do val = *byteptr++;
	But any code which does this is responsible for making sure
	the next access will also be valid.  Otherwise, we should
	save state and return.

	Ben Fahy -- AGE Logic, Inc. Sept, 1993
  
*****************************************************************************/

/* the folling define causes extra stuff to be saved in state recorder */
#define _PBits

#include "fax.h"
#include "faxint.h"
#include "bits.h"
#include "xiemd.h"

/* by decree of PackBits spec */

/**********************************************************************/
int decode_tiffpb(state)
FaxState *state;
{
register int a0_color,a0_pos;
register unsigned char *byteptr;
register unsigned char *endptr;
register int goal;
register int width,rnd_width;
register int rl;

register int 	lines_found=0;
int 	length_acc=0;

/*** output line stuff ***/
register unsigned char 	*olp;
register int i,a0_in_bytes;

	if (state == (FaxState *)NULL)
		return(-1);

	/* set up initial bitstream for the very first strip */
	if (!state->bits.started) {
	    if (state->strip_state != StripStateNew)  {
	       state->decoder_done = FAX_DECODE_DONE_ErrorBadStripper;
	       return(-1);
	    }
	    
	    state->bits.byteptr = (unsigned char *)state->strip;
	    state->bits.endptr  = state->bits.byteptr + state->strip_size;
	    	/* record end of input strip */

	    state->bits.bitpos = 0;
	    state->bits.started = 1;

	}

	if (state->magic_needs)  {
	  if (state->strip_state == StripStateNew)  {
	    state->strip_state = StripStateInUse;
	    state->bits.byteptr = (unsigned char *)state->strip;
	    state->bits.endptr  = state->bits.byteptr + state->strip_size;
	    	/* record end of input strip */
	    state->bits.bitpos = 0;
	    state->bits.started = 1;
	  }
	  else {
	     /* no new strip?? But... I *need* more data! */
	      state->decoder_done = FAX_DECODE_DONE_ErrorBadMagic;
	      return(lines_found);
	  }
	  state->magic_needs = 0;
	}

	localize_state(state);
	rnd_width = 8 * ((width+7)/8);

	olp = (unsigned char *) state->o_lines[lines_found];

/***	Main Decoding Loop	***/
	while(1) {
	  switch(goal) {
	  case PB_GOAL_StartNewLine:
	    if (lines_found >= state->nl_sought) {
		save_state_and_return(state);
	    }
	    
	    a0_pos   = 0;
	    olp = (unsigned char *) state->o_lines[lines_found];
	    goal = PB_GOAL_GetRunLength;
	    break;

	  case PB_GOAL_GetRunLength:

/*** Basic Packbits algorithm:

code = *((signed char *)byteptr++);

if (0 <= code  && code <= 127) 	===> length = code + 1;

if (-127 <= code && code < 0)  	===> length = 1 - code;

else				===> length = 0

We can make things a little quicker (and avoid signed char compiler issues)
if we simply look for whether the high bit is on or off, and then compute
the result directly.  Thus, 

if code && 0x80 =>  -128 <= code < 0

    0x80 + foo = -128 + foo

    so length = 1 - code = 1 - (-128 + foo) = 129 - foo.

    note foo=0 is code for runlength 0 (fill)

The positive case is even easier.

***/
	    if (byteptr >= endptr) {
	       state->magic_needs = 1;
	       state->strip_state = StripStateDone;
	       save_state_and_return(state);
	    }
	    if (*byteptr & 0x80) { 		/* -128 <= code < 0    */
		register int foo = 0x7f & *byteptr;
		if (foo) {
		   length_acc = 129 - foo  ;
		   goal = PB_GOAL_GetRepeatByte;
		}
		else {
		   length_acc = 0;		/* -128 => code = 0     */
		}
	    }
	    else { 				/*    0 <= code <= 127 */
		length_acc = 1 + *byteptr;
		goal = PB_GOAL_GetLiteralBytes;
	    } 
	    ++byteptr;

	    /* make sure all the pixels fit on this output line */
	    if (8*length_acc + a0_pos > rnd_width ) {
		state->decoder_done = FAX_DECODE_DONE_ErrorPastWidth;
	       	save_state_and_return(state);
	    }
	    break;

	  case PB_GOAL_GetLiteralBytes:
	    /* Important Note!!  we are guaranteed that output line 
	       has enough space to hold output pixels here. We checked
	       in the GetRunLength state.  However, it is possible we
	       may run out of input bytes, in which case we may have
	       to save state and return.
	    */
	    a0_in_bytes = a0_pos / 8;
	    while (length_acc > 0) {
	        if (byteptr >= endptr) {
	       	   a0_pos = a0_in_bytes << 3;
	     	   state->magic_needs = 1;
	           state->strip_state = StripStateDone;
	           save_state_and_return(state);
	        }
		a0_color = *byteptr++; length_acc--;
#if (IMAGE_BYTE_ORDER == MSBFirst)
		olp[a0_in_bytes++] = a0_color;
#else
		olp[a0_in_bytes++] = _ByteReverseTable[a0_color];
#endif
	    }
	    a0_pos = a0_in_bytes << 3;

	    if (a0_pos >= width) {
		++lines_found;
	       	goal = PB_GOAL_StartNewLine;
	    }
	    else
	     	goal = PB_GOAL_GetRunLength;

	    break;

	  case PB_GOAL_GetRepeatByte:
	    if (byteptr >= endptr) {
	     	state->magic_needs = 1;
	     	state->strip_state = StripStateDone;
	       	save_state_and_return(state);
	    }
	    a0_color = *byteptr++;
	
	    /* Important Note!!  we are guaranteed that output line 
	       has enough space to hold output pixels here. We checked
	       in the GetRunLength state 
	    */

	    a0_in_bytes = a0_pos / 8;
	    for (i=0; i<length_acc; ++i) 
#if (IMAGE_BYTE_ORDER == MSBFirst)
		olp[a0_in_bytes+i] = a0_color;
#else
		olp[a0_in_bytes+i] = _ByteReverseTable[a0_color];
#endif

	    a0_pos += 8*length_acc;
	    if (a0_pos >= width) {
		++lines_found;
	       	goal = PB_GOAL_StartNewLine;
	    }
	    else
		goal = PB_GOAL_GetRunLength;

	    break;

	  default:
	    state->decoder_done = FAX_DECODE_DONE_ErrorBadGoal;
	    save_state_and_return(state);
	  } /* end of switch */
	} /* end of main decoding loop */
}

/**** module fax/tiffpb.c ****/
