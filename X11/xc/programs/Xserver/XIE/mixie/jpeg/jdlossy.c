/* $Xorg: jdlossy.c,v 1.4 2001/02/09 02:04:28 xorgcvs Exp $ */
/* Module jdlossy.c */

/****************************************************************************

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

	jdlossy.c - decode JPEG images,  Lossy method

	this contain code for:

	1) JPEG Lossless, grayscale
	2) JPEG Lossless, color

	Ben Fahy, AGE Logic, Oct 1993
	Gary Rogers, AGE Logic, Inc., January 1994

****************************************************************************/

#include "jpeg.h"

/**********************************************************************/
decode_jpeg_lossy_gray(state)
JpegDecodeState *state;
{
	return( decode_jpeg_lossy_color(state) );
}
/**********************************************************************/
decode_jpeg_lossy_color(state)
JpegDecodeState *state;
{
int status;

/*  our output buffer is guaranteed to be clean on entry.   */
/*  (every time we write data below, we return to flush it) */
    state->nl_found=0;
    state->nl_flushed=0;

    while(1) {		/* loop forever (we're a state machine) */

	switch( state->goal) {
	case JPEG_DECODE_GOAL_Startup:
	   state->goal = JPEG_DECODE_GOAL_ReadDataForInit;

	case JPEG_DECODE_GOAL_ReadDataForInit:
	   /* we start up by reading data into our buffer and calling INIT */
	   status = fill_jpeg_decode_buffer(state);
	   switch(status) {
	    case JPEG_BUFFER_LastBuffer:
		break;

	    case JPEG_BUFFER_BufferFilled:
		/* all is coolness */
		break;

	    case JPEG_BUFFER_NeedAnotherStrip:
		state->needs_input_strip = 1;
		return(state->nl_found);
		break;

	    case JPEG_BUFFER_BufferError:
	    default: 
  	      	state->error_code = JPEG_DECODE_ERROR_CouldNotStart;
	   	return(-1);
	   }

	   /* if here, we should have a nice full input buffer */

	case JPEG_DECODE_GOAL_TryToInit:
	   status = JD_INIT(state->cinfo,
			    state->dc_methods,
			    state->e_methods,
			    state->up_sample);
	   if (status == XIE_ERR) {
  	        state->error_code = JPEG_DECODE_ERROR_CouldNotInit;
		return(-1);
	   }
	   if (status == XIE_NRML) {
		state->goal = JPEG_DECODE_GOAL_InitDone;
		break;
	   } 
	   if (status == XIE_INP) {
		state->goal = JPEG_DECODE_GOAL_ReadDataForInit;
		break;
	   }
	   /* hey! we aren't supposed to be here */
	   state->error_code = JPEG_DECODE_ERROR_BadInitRetCode;
	   return(-1);
	   break;

	case JPEG_DECODE_GOAL_InitDone:

	   /* now we know enough to allocate our output buffer */
	   state->cinfo->output_workspace =
		alloc_sampimage(state->cinfo,
			(int) state->cinfo->comps_in_scan,
			(long) state->cinfo->rows_in_mem,
			state->cinfo->image_width
		);
	   if (state->cinfo->output_workspace == NULL) {
  	       state->error_code = JPEG_DECODE_ERROR_BadOutputAlloc;
	       return(-1);
	   }
	   state->goal = JPEG_DECODE_GOAL_ProcessData;

	case JPEG_DECODE_GOAL_ProcessData:
	   status = JD_PROCESS(state->cinfo);

	   if (status == XIE_INP) {
		state->goal = JPEG_DECODE_GOAL_ReadDataForProcess;
		break;
	   }
	   if (status == XIE_OUT) {
		state->goal = JPEG_DECODE_GOAL_WriteDataForProcess;
		break;
	   }
	   if (status == XIE_EOI) {
		state->goal = JPEG_DECODE_GOAL_EndOfInput;
		break;
	   }
	   if (status == XIE_NRML) 
		break;

	   if (status == XIE_ERR) {
  	       state->error_code = JPEG_DECODE_ERROR_DecodeError;
	       return(-1);
	   }
	   else {
		/* unexpected return status */
  	        state->error_code = JPEG_DECODE_ERROR_DecoderIsFreakingOut;
	        return(-1);
	   }
	   break;

	case JPEG_DECODE_GOAL_ReadDataForProcess:
	   status = fill_jpeg_decode_buffer(state);
	   switch(status) {
	    case JPEG_BUFFER_LastBuffer:
		break;

	    case JPEG_BUFFER_BufferFilled:
		/* all is coolness */
		break;

	    case JPEG_BUFFER_NeedAnotherStrip:
		state->needs_input_strip = 1;
		return(state->nl_found);
		break;

	    case JPEG_BUFFER_BufferError:
	    default: 
  	      	state->error_code = JPEG_DECODE_ERROR_NoMoreProcessData;
	   	return(-1);
	   }
	   state->goal = JPEG_DECODE_GOAL_ProcessData;
	   break;

	case JPEG_DECODE_GOAL_EndOfInput:
	   state->goal = JPEG_DECODE_GOAL_Done;
	   state->nl_found = (state->cinfo->image_height - 
			      state->cinfo->pixel_rows_output);
	      /* see jdXIE_get() if this seems *too* self-serving */

		/* leave for good */
	   return(state->nl_found);
	   break;

	case JPEG_DECODE_GOAL_WriteDataForProcess:
	   state->goal = JPEG_DECODE_GOAL_ProcessData;
	   state->cinfo->pixel_rows_output += state->cinfo->rows_in_mem;
	   state->nl_found = state->cinfo->rows_in_mem;
		/* leave to flush data */
	   return(state->nl_found);
	   break;

	default:
  	   state->error_code = JPEG_DECODE_ERROR_BadGoal;
	   return(-1);
	}
    }
}
/**********************************************************************/
