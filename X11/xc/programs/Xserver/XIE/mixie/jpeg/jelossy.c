/* $Xorg: jelossy.c,v 1.4 2001/02/09 02:04:29 xorgcvs Exp $ */
/* Module jelossy.c */

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


	jelossy.c - encode JPEG images,  Lossy method

	this contain code for:

	1) JPEG Lossless, grayscale
	2) JPEG Lossless, color

	Ben Fahy, AGE Logic, Oct 1993
	Gary Rogers, AGE Logic, Inc., January 1994


****************************************************************************/

#include "jpeg.h"

/**********************************************************************/
encode_jpeg_lossy_gray(state)
JpegEncodeState *state;
{
	return( encode_jpeg_lossy_color(state) );
}
/**********************************************************************/
encode_jpeg_lossy_color(state)
JpegEncodeState *state;
{
int status;	/* some compilers may complain, but close examination
		** shows the useage of this variable 'status' to be correct.
		*/

/*  our output buffer is guaranteed to be clean on entry.   */
/*  (every time we write data below, we return to flush it) */

    state->flush_output = 0;
    state->nl_coded     = 0;

    while(1) {		/* loop forever (we're a state machine) */

	switch( state->goal) {
	case JPEG_ENCODE_GOAL_Startup:
	   state->goal = JPEG_ENCODE_GOAL_TryToBeginFrame;

/***	Write out the beginning frame information	***/
	case JPEG_ENCODE_GOAL_TryToBeginFrame:

	   state->cinfo->bytes_in_buffer = 0;
		/* initialize buffer to empty */

	   status  = JC_BEGINFRAME(state->cinfo,
				   state->n_bands,
				   state->width,
				   state->height,
				   state->Qtable,
				   state->lenQtable,
				   state->ACtable,
				   state->lenACtable,
				   state->DCtable,
				   state->lenDCtable,
				   state->h_sample,
				   state->v_sample);

	   if (status == XIE_ERR) {
  	        state->error_code = JPEG_ENCODE_ERROR_CouldNotBeginFrame;
		return(-1);
	   }
	   if (status == XIE_NRML) {
		state->goal = JPEG_ENCODE_GOAL_BeginFrameDone;
		break;
	   } 
	   if (status == XIE_OUT) {
		state->goal = JPEG_ENCODE_GOAL_WriteHdrData;
		break;
	   }
	   /* hey! we aren't supposed to be here */
	   state->error_code = JPEG_ENCODE_ERROR_BadBeginFrameRetCode;
	   return(-1);
	   break;

	case JPEG_ENCODE_GOAL_WriteHdrData:
	   return(-1);
	   break;

	case JPEG_ENCODE_GOAL_BeginFrameDone:
	   state->goal = JPEG_ENCODE_GOAL_ProcessData;
	   state->needs_input_strip = 1;
	   state->flush_output = state->cinfo->bytes_in_buffer;
	   return(0);
	   break;

	case JPEG_ENCODE_GOAL_ProcessData:
	   if (state->n_bands == 3)
	       state->goal = JPEG_ENCODE_GOAL_EncodeRGB;
	   else
	       state->goal = JPEG_ENCODE_GOAL_EncodeGray;
	   break;

	case JPEG_ENCODE_GOAL_EncodeGray:
	   if (state->nl_tocode <= 0) {
	     state->flush_output = state->cinfo->bytes_in_buffer;
	     return(state->nl_coded);
	   }
	   status = JC_SCANLINE_GRAY(state->cinfo,
		&state->i_line,				/* global line #   */
		state->i_lines[0][state->nl_coded]); 	/* gray  data line */

	   /* keep track of how many lines in strip we've given encoder */
	   state->nl_coded++;
	   state->i_line++;
	   state->nl_tocode--;
	   state->goal = JPEG_ENCODE_GOAL_CheckStatus;
	   break;

	case JPEG_ENCODE_GOAL_EncodeRGB:
	   if (state->nl_tocode <= 0) {
	     state->flush_output = state->cinfo->bytes_in_buffer;
	     return(state->nl_coded);
	   }
	   status = JC_SCANLINE_RGB(state->cinfo,
		&state->i_line,				/* global line #   */
		state->i_lines[0][state->nl_coded], 	/* red   data line */
		state->i_lines[1][state->nl_coded], 	/* green data line */
		state->i_lines[2][state->nl_coded]);	/* blue  data line */

	   /* keep track of how many lines in strip we've given encoder */
	   state->nl_coded++;
	   state->i_line++;
	   state->nl_tocode--;
		
	case JPEG_ENCODE_GOAL_CheckStatus:
	   if (status == XIE_OUT) {
		state->flush_output = state->cinfo->bytes_in_buffer;
	        return(state->nl_coded);
		break;
	   }
	   if (status == XIE_NRML)  {
		state->goal = JPEG_ENCODE_GOAL_ProcessData;
		if (state->flush_output = state->cinfo->bytes_in_buffer) 
	          return(state->nl_coded);
		
		break;
	   }

	   if (status == XIE_ERR) {
  	       state->error_code = JPEG_ENCODE_ERROR_EncodeError;
	       return(-1);
	   }
	   else {
		/* unexpected return status */
  	        state->error_code = JPEG_ENCODE_ERROR_EncoderIsFreakingOut;
	        return(-1);
	   }
	   break;

	case JPEG_ENCODE_GOAL_EndFrame:
	   status = JC_ENDFRAME(state->cinfo);

	   if (state->flush_output = state->cinfo->bytes_in_buffer) {
	        if (status == XIE_NRML)  
		    state->goal = JPEG_ENCODE_GOAL_Done;
		
	        return(state->nl_coded);
	   }
	   else if (status == XIE_NRML)  {
		state->goal = JPEG_ENCODE_GOAL_Done;
	        state->flush_output = state->cinfo->bytes_in_buffer;
		return(state->nl_coded);
	   }
	   else if (status == XIE_ERR) {
  	       state->error_code = JPEG_ENCODE_ERROR_EndFrameError;
	       return(-1);
	   }
	   else {
		/* unexpected return status */
  	        state->error_code = JPEG_ENCODE_ERROR_EndFrameFreakOut;
	        return(-1);
	   }
	   break;

	default:
  	   state->error_code = JPEG_ENCODE_ERROR_BadGoal;
	   return(-1);
	}
    }
}
/**********************************************************************/
