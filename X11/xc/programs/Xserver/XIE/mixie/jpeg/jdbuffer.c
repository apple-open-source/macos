/* $Xorg: jdbuffer.c,v 1.4 2001/02/09 02:04:28 xorgcvs Exp $ */
/*** 	jdbuffer.c - transfer data from input strip to decode buffer */

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

	Ben Fahy, AGE Logic, Oct 1993
	Gary Rogers, AGE Logic, Inc., January 1994

****************************************************************************/

#include "jpeg.h"
#include "macro.h"

/************************************************************************/
int fill_jpeg_decode_buffer(state)
JpegDecodeState *state;
{
decompress_info_ptr cinfo=state->cinfo;
unsigned char *start_of_rest_of_buffer;
int bytes_to_fill,sbytes_done,sbytes_left;

/***	It is an error to call fill_jpeg_decode_buffer after
	we have found out there is no more strip data
***/
	if (state->no_more_strips)
		return(JPEG_BUFFER_BufferError);

#define START_OF_JPEG_INPUT_BUFFER  (cinfo->input_buffer + MIN_UNGET)

/***	Whenever we fill the JPEG decode buffer, we always start by
	copying existing data to start of buffer.  This way we know
	we have a nice contiguous buffer of JPEG_BUF_SIZE bytes long
***/
	if (cinfo->bytes_in_buffer)  {
		memcpy(START_OF_JPEG_INPUT_BUFFER,
			cinfo->next_input_byte,
			cinfo->bytes_in_buffer);
	}
	cinfo->next_input_byte = START_OF_JPEG_INPUT_BUFFER;
		/* tells decoder (s)he can start reading here */

/***	Ok, now calculate how much fills the rest of the buffer, and
	where to put it (assuming we can get the data from a strip)
***/
	start_of_rest_of_buffer = (unsigned char *)
		cinfo->next_input_byte + cinfo->bytes_in_buffer;

	bytes_to_fill = JPEG_BUF_SIZE - cinfo->bytes_in_buffer;
		/* how much room is left in the buffer */

/***	Is there enough data in the current strip to fill buffer? 	***/
	sbytes_done = state->sptr - state->strip;
	sbytes_left = state->strip_size - sbytes_done;

	if (sbytes_left >= bytes_to_fill) {
		/* yes! */
		memcpy(start_of_rest_of_buffer,state->sptr,bytes_to_fill);
		state->sptr += bytes_to_fill;
		cinfo->bytes_in_buffer += bytes_to_fill;

		cinfo->XIEbytes_in_buffer = cinfo->bytes_in_buffer;
		cinfo->XIEnext_input_byte = cinfo->next_input_byte;
			/* save these in case we have to restart */

		return(JPEG_BUFFER_BufferFilled);
	}

/***	Well, no.  We will copy what we can.  If this is the last
	strip, we will set NoMoreStrips to signal the next state.
	If there are more strips to come, we will return the
	signal NeedAnotherStrip,  so caller will go get another.
***/
	memcpy(start_of_rest_of_buffer,state->sptr,sbytes_left);
	cinfo->bytes_in_buffer += sbytes_left;

	cinfo->XIEbytes_in_buffer = cinfo->bytes_in_buffer;
	cinfo->XIEnext_input_byte = cinfo->next_input_byte;
			/* save these in case we have to restart */

	if (state->final) {
		state->no_more_strips = 1;
		return(JPEG_BUFFER_LastBuffer);
	}
	else 
		return(JPEG_BUFFER_NeedAnotherStrip);
}
/************************************************************************/
