/* $Xorg: dg32d.c,v 1.4 2001/02/09 02:04:25 xorgcvs Exp $ */
/* AGE Logic - Oct 15 1995 - Larry Hare */
/**** module fax/dg32d.c ****/
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
  
	fax/dg32d.c -- DDXIE G32D fax decode technique/element
  
	Ben Fahy -- AGE Logic, Inc. May, 1993
  
*****************************************************************************/

#define lenient_decoder

/* the folling define causes extra stuff to be saved in state recorder */
#define _G32D

#include "fax.h"
#include "faxint.h"
#include "bits.h"

#include <servermd.h> /* pick up the BITMAP_BIT_ORDER from Core X*/


/**********************************************************************/
int decode_g32d(state)
FaxState *state;
{
register int bitpos;
register unsigned char *byteptr;
register unsigned char *endptr;
register int a0_color;
register int a0_pos,a1_pos;
register int goal;
register int mode;	/* could overload goal, but will resist */
register int length;
register int width;
register int g32d_horiz;
register int rl;

int 	lines_found=0;
int	n_old_trans;
int	n_new_trans;
int	*old_trans;
int	*new_trans;
int 	a0a1,a1a2;
int 	length_acc=0;
int 	last_b1_idx=0;
int 	b1_pos,b2_pos;
int 	code,nbits;

	if (state == (FaxState *)NULL)
		return(-1);

	/* set up initial bitstream for the very first strip */
	if (!state->bits.started) {
	    if (state->strip_state != StripStateNew) {
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
	  if (endptr < byteptr) {
	      return(-1);
	  }
	  switch(goal) {

	  case FAX_GOAL_SkipPastAnyToEOL:
		code = 0;	/* get Gnu cc to shut up. */
		while (byteptr < endptr) {
	    	  /* look for EOL code */
	    	  code = get_wcode(byteptr,bitpos,endptr);
	    	  rl     = _WhiteFaxTable[code].run_length;
	      	  if (rl == EOL_RUN_LENGTH) 
			break;
#if defined(lenient_decoder)
		  else
			/* move bitstream one bit further and try again */
 		 	adjust_1bit(byteptr,bitpos,endptr);
#else
		  else if (!state->o_line && !lines_found) {
			/* we'll be lenient starting out */
 		 	adjust_1bit(byteptr,bitpos,endptr);
		  }
		  else {
			state->decoder_done = FAX_DECODE_DONE_ErrorSkipPast;
	    		save_state_and_return(state);
		  }
#endif
		}
		if (byteptr >= endptr) {
			state->decoder_done = FAX_DECODE_DONE_ErrorSkipPast;
	    		save_state_and_return(state);
		}
	    	nbits  = _WhiteFaxTable[code].n_bits;
	    	goal = FAX_GOAL_SeekTagBit;
	    	adjust_bitstream(nbits,byteptr,bitpos,endptr);
		break;

	  case FAX_GOAL_SeekEOLandTag:
	    goal =  FAX_GOAL_SkipPastAnyToEOL;
	    break;

	    case FAX_GOAL_SeekTagBit:
	    if (get_bit(byteptr,bitpos,endptr)) {
		g32d_horiz = 1;
	    }
	    else {
		g32d_horiz = 0;
	    }
	    goal = FAX_GOAL_AdjustTag;
		/* have to set this goal in case adjusting throws us 
		   back to the caller in order  to get more data. */

	    adjust_1bit(byteptr,bitpos,endptr);
	    case FAX_GOAL_AdjustTag:

		
	    /*** ok, now start a new line ***/
	    goal = FAX_GOAL_StartNewLine;
	    break;

	  case FAX_GOAL_StartNewLine:

	    /* if we got all the data we want, write it out */
	    if (lines_found >= state->nl_sought)
		    save_state_and_return(state);

	    /* normal line initialization stuff */
	    a0_pos   = -1;
	    a0_color = WHITE;
	    if (g32d_horiz)
		goal = FAX_GOAL_HandleHoriz;
	    else
	        goal = FAX_GOAL_DetermineMode;

	    reset_transitions();
	    break;
	  case FAX_GOAL_DetermineMode:
	    get_mode_and_length(mode,length,byteptr,bitpos,endptr); 
	    goal = mode;
		/* our goal is now to handle whatever mode we're in! */

	    if (mode == FAX_MODE_Unknown)  {
		/* hopefully, we just hit the first 0 in an EOL */
		goal = FAX_GOAL_SeekEOLandTag;
	    }
	    else
	         adjust_bitstream_8(length,byteptr,bitpos,endptr);
	    break;

	  case  FAX_GOAL_RecoverZero:

	    /* have to set new goal in case adjusting throws us	*/
	    /* back to the caller (to get more data) 		*/
	    goal = FAX_GOAL_AdjustedButStillRecovering;
	    adjust_bitstream(nbits,byteptr,bitpos,endptr);

	  case FAX_GOAL_AdjustedButStillRecovering:
	    {
	    int next_goal = FAX_GOAL_FoundOneEOL;

	    while (1) {
	       if (*byteptr) {
		register int mask;
	        /* important assumption: we only get here if we have */
	        /* found at least twelve bits of zero leading up to  */
	        /* the current bitposition.  Therefore, there can't  */
		/* be any bits in the byte before the one we want    */

		mask = 1 << (7-bitpos);
		while (bitpos < 8) {
		   if (mask & (*byteptr))
			break;
		   mask >>= 1;
		   ++bitpos;
		 }	     /* can't really reach bitpos = 8 above */
		 goal = next_goal;
 		 adjust_1bit(byteptr,bitpos,endptr);
		 break;
			/* break out of while (1) */
	       }
	       else {
		 bitpos = 0;
		 if (++byteptr >= endptr)
	           do_magic(byteptr,bitpos,endptr);
		 /* loop around to search some more */
	       }
	    } /* end of while */

	    }
	    break;

	  case  FAX_GOAL_FoundOneEOL:
	    /* found an EOL.  If this is RTC, there will be five more EOL's */
	    /* on the way.  If this is a normal EOL,  next code will not    */
	    /* be an EOL,  and we should interpret the next bit as tag bit  */


	    /* look for second EOL code */
	    code = get_wcode(byteptr,bitpos,endptr);
	    rl = _WhiteFaxTable[code].run_length;
	    if (rl != EOL_RUN_LENGTH) {
	        goal =  FAX_GOAL_SeekTagBit;
		break;
	    }

	    /* Got a second EOL code */
	    return(-1);
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

	  case  FAX_GOAL_RecordA0A1:
	    a0a1 = length_acc;
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
#else  /* not so lenient */
		  state->decoder_done = FAX_DECODE_DONE_ErrorPastWidth;
		  return(lines_found);
#endif
		}
		FlushLineData();
		goal = FAX_GOAL_SeekEOLandTag;
		break;
	    }
#if defined(lenient_decoder)
	    if (rl < 0) {
		FlushLineData();
	        goal =  FAX_GOAL_SeekTagBit;
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
#else  /* not so lenient */
	    if (goal == FAX_GOAL_RecoverZero) {
	        /* it's possible we got here by reading a zero in get_a0a1, */
	        /* in which case 'goal' was set to FAX_GOAL_RecoverZero. We */
	        /* must assume that the zero is an EOL pre-padded with a    */
	        /* variable number of zeros, which is legal according to    */
	        /* the coding spec.  So we will record the increment in     */
	        /* line number and then attempt to recover.		    */
		FlushLineData();
	  	goal = FAX_GOAL_SkipPastAnyToEOL;
	        break;
	    } else if(rl == BAD_RUN_LENGTH) {
		state->decoder_done = FAX_DECODE_DONE_ErrorBada0a1;
		return(lines_found);
	    }
#endif
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
	    new_trans[n_new_trans] = a0_pos = new_trans[n_new_trans-1]+a1a2;
	    n_new_trans++;

#if defined(lenient_decoder)
	    if (rl < 0) {
		FlushLineData();
	        goal =  FAX_GOAL_SeekTagBit;
		break;
	    }
#else  /* not so lenient */
	    /* it's possible we got here by reading a zero in get_a1a2, */
	    /* in which case 'goal' was set to FAX_GOAL_RecoverZero. We */
	    /* must assume that the zero is an EOL pre-padded with a 	*/
	    /* variable number of zeros, which is legal according to	*/
	    /* the coding spec.  So we will update the line count and	*/
	    /* then attempt to recover.					*/
	    if (goal == FAX_GOAL_RecoverZero) {
	        /* assume we have hit the begining of an EOL */
		FlushLineData();
	  	goal = FAX_GOAL_SkipPastAnyToEOL;
	        break;
	    } else if(rl == BAD_RUN_LENGTH) {
		state->decoder_done = FAX_DECODE_DONE_ErrorBada1a2;
		return(lines_found);
	    }
#endif
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
		goal = FAX_GOAL_SeekEOLandTag;
		break;
	    } else if(a0_pos > 0) {
		if (g32d_horiz)
	            goal = FAX_GOAL_HandleHoriz;
		else
	            goal = FAX_GOAL_DetermineMode;
	    } else {
		state->decoder_done = FAX_DECODE_DONE_ErrorBadA0pos;
		return(lines_found);
	    }
	    break;

	  case FAX_MODE_Pass:
	    if (!n_old_trans) {	/* line above all white */
	      return(-1);
	    }
	    find_b2pos(a0_pos,a0_color,n_old_trans,old_trans);

	    a0_pos = b2_pos;
	    if (a0_pos < 0 || a0_pos >= width) {
		FlushLineData();
	       	goal = FAX_GOAL_SeekEOLandTag;
	       	break;
	    }
	    goal = FAX_GOAL_DetermineMode;
	    break;

	  case FAX_GOAL_HandleVertL3:
	  case FAX_GOAL_HandleVertL2:
	  case FAX_GOAL_HandleVertL1:
	  case FAX_GOAL_HandleVert0:
	  case FAX_GOAL_HandleVertR1:
	  case FAX_GOAL_HandleVertR2:
	  case FAX_GOAL_HandleVertR3:
	    if (n_old_trans) {	/* line above not all white */
		find_b1pos(a0_pos,a0_color,n_old_trans,old_trans);

		if (b1_pos < 0) {
		   if (goal > FAX_GOAL_HandleVert0) {
		     return(-1);	/* error! */
		   }
		   b1_pos = width;
		}
	    } else {		
	       /* line above was all white. Since b1 was first non-white */
	       /* b1 is imaginary transition off right edge.		 */
		b1_pos = width;
	    }
	    /* ok, we are guaranteed that 0 <= b1_pos <= width */
	    /* set a0_pos = a1_pos,  which is relative to b1_pos */
	    a0_pos   = b1_pos + (goal-FAX_GOAL_HandleVert0);
	    a0_color = 1-a0_color;
	    new_trans[n_new_trans++] = a0_pos > 0 ? a0_pos : 0;
	    if (a0_pos < 0 || a0_pos >= width) {
		FlushLineData();
		goal = FAX_GOAL_SeekEOLandTag;
	    } else {
		/* not at eol yet */
		goal = FAX_GOAL_DetermineMode;
	    }
	    break;

	  default:
	    return(-1);
	  break;
	  }  /* end of switch */
	}
/***	End, Main Decoding Loop	***/
}
/**** module fax/g32d.c ****/
