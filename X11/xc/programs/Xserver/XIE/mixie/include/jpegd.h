/* $Xorg: jpegd.h,v 1.4 2001/02/09 02:04:28 xorgcvs Exp $ */
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

	jpegd.h - common include file for jpeg decoders

	Ben Fahy -- AGE Logic, Inc.  Oct 1993

******************************************************************************/

#define JPEG_DECODE_GOAL_Startup		0
#define JPEG_DECODE_GOAL_Done			1
#define JPEG_DECODE_GOAL_ReadDataForInit	2
#define JPEG_DECODE_GOAL_TryToInit		3
#define JPEG_DECODE_GOAL_InitDone		4
#define JPEG_DECODE_GOAL_ProcessData		5
#define JPEG_DECODE_GOAL_ReadDataForProcess	6
#define JPEG_DECODE_GOAL_WriteDataForProcess	7
#define JPEG_DECODE_GOAL_EndOfInput		8
#define JPEG_DECODE_GOAL_FlushData		9

#define JPEG_DECODE_ERROR_BadGoal		1
#define JPEG_DECODE_ERROR_DecoderIsFreakingOut	2
#define JPEG_DECODE_ERROR_CouldNotStart		3
#define JPEG_DECODE_ERROR_CouldNotInit		4
#define JPEG_DECODE_ERROR_BadInitRetCode	5
#define JPEG_DECODE_ERROR_BadOutputAlloc	6
#define JPEG_DECODE_ERROR_DecodeError		7
#define JPEG_DECODE_ERROR_NoMoreProcessData	8
#define JPEG_DECODE_ERROR_ColorMismatch		9
#define JPEG_DECODE_ERROR_WidthMismatch		10
#define JPEG_DECODE_ERROR_HeightMismatch	11

typedef struct _jpeg_decode_state {
	int goal;		/* what to do next			*/
	int error_code;		/* ooops				*/

	int up_sample;		/* allow up-sampling or leave as is?	*/
	int color;		/* color image or grayscale?		*/
	int width,height;	/* available elsewhere, but I'm lazy 	*/

	int o_line;	 	/* absolute output line position	*/
	int nl_flushed;	 	/* how many lines we have flusehd	*/
	int nl_found;	 	/* how many lines we have, this round	*/
	unsigned char **o_lines[3];   
				/* pointers to output lines, 3 bands	*/

	decompress_info_ptr cinfo;	
				/* decoder's private state		*/
	decompress_methods_ptr dc_methods;
	external_methods_ptr    e_methods;
				/* decoder's private methods		*/

	unsigned char *jpeg_input_buffer;
	unsigned char *jpeg_output_buffer;
				/* names self-explanatory?		*/

	int  strip_size;	/* size  of currently available strip	*/
	unsigned char *strip;	/* start of currently available strip	*/
	unsigned char *sptr;	/* position within the strip		*/
	int needs_input_strip;	/* need a new strip			*/
	int i_strip;		/*  input strip #. nice for debugging	*/
	int o_strip;		/* output strip #. nice for debugging	*/
	int final;		/* this is the last strip		*/
	int no_more_strips;	/* flag saying you can't have any more	*/

} JpegDecodeState;

extern int decode_jpeg_lossless_color(
#ifdef NEED_PROTOTYPES
	JPegDecodeState *	 /* state */
#endif
);

extern int decode_jpeg_lossy_color(
#ifdef NEED_PROTOTYPES
	JPegDecodetSate *	 /* state */
#endif
);

extern int decode_jpeg_lossless_gray(
#ifdef NEED_PROTOTYPES
	JPegDecodeState *	 /* state */
#endif
);

extern int decode_jpeg_lossy_gray(
#ifdef NEED_PROTOTYPES
	JPegDecodeState *	 /* state */
#endif
);


