/* $Xorg: jpege.h,v 1.4 2001/02/09 02:04:28 xorgcvs Exp $ */
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

	jpege.h - common include file for jpeg encoder

	Ben Fahy -- AGE Logic, Inc.  Oct 1993

******************************************************************************/


#define JPEG_ENCODE_GOAL_Startup		0
#define JPEG_ENCODE_GOAL_Done			1
#define JPEG_ENCODE_GOAL_TryToBeginFrame	2
#define JPEG_ENCODE_GOAL_WriteHdrData		3
#define JPEG_ENCODE_GOAL_BeginFrameDone		4
#define JPEG_ENCODE_GOAL_ProcessData		5
#define JPEG_ENCODE_GOAL_ReadDataForProcess	6
#define JPEG_ENCODE_GOAL_WriteDataForProcess	7
#define JPEG_ENCODE_GOAL_EndOfInput		8
#define JPEG_ENCODE_GOAL_FlushData		9
#define JPEG_ENCODE_GOAL_EncodeRGB		10
#define JPEG_ENCODE_GOAL_EncodeGray		11
#define JPEG_ENCODE_GOAL_CheckStatus		12
#define JPEG_ENCODE_GOAL_EndFrame		13

#define JPEG_ENCODE_ERROR_BadGoal		1
#define JPEG_ENCODE_ERROR_EncoderIsFreakingOut	2
#define JPEG_ENCODE_ERROR_CouldNotStart		3
#define JPEG_ENCODE_ERROR_CouldNotBeginFrame	4
#define JPEG_ENCODE_ERROR_BadBeginFrameRetCode	5
#define JPEG_ENCODE_ERROR_BadOutputAlloc	6
#define JPEG_ENCODE_ERROR_EncodeError		7
#define JPEG_ENCODE_ERROR_NoMoreProcessData	8
#define JPEG_ENCODE_ERROR_ColorMismatch		9
#define JPEG_ENCODE_ERROR_EndFrameError		10
#define JPEG_ENCODE_ERROR_EndFrameFreakOut	11

typedef struct _jpeg_encode_state {
	int goal;		/* what to do next			*/
	int error_code;		/* ooops				*/

	int n_bands;		/* color image or grayscale?		*/
	int width,height;	/* available elsewhere, but I'm lazy 	*/

	int flush_output;	/* how many bytes left to flush		*/
	int nl_coded;	 	/* how many lines we have, this round	*/
	int nl_tocode;	 	/* how many lines we want, this round	*/
	int i_line;	 	/* absolute input line position	*/
	unsigned char **i_lines[3];
				/* pointers to input lines, 3 bands	*/

	compress_info_ptr cinfo;	
				/* decoder's private state		*/
	compress_methods_ptr    c_methods;
	external_methods_ptr    e_methods;
				/* decoder's private methods		*/

	unsigned char *jpeg_input_buffer;
	unsigned char *jpeg_output_buffer;
				/* names self-explanatory?		*/
	unsigned char *jpeg_output_bpos;
				/* output buffer position during flush	*/

	int  strip_size;	/* size  of currently available strip	*/
	unsigned char *strip;	/* start of currently available strip	*/
	unsigned char *sptr;	/* position within the strip		*/
	int needs_input_strip;	/* need a new strip			*/
	int i_strip;		/*  input strip #. nice for debugging	*/
	int o_strip;		/* output strip #. nice for debugging	*/
	int final;		/* this is the last strip		*/
	int no_more_strips;	/* flag saying you can't have any more	*/

  	int  strip_req_newbytes;/* number of destination bytes we want  */

	short h_sample[3];	/* horizontal sample factors		*/
	short v_sample[3];	/* vertical sample factors		*/

	int lenQtable;
	int lenACtable;
	int lenDCtable;

	unsigned char *Qtable;
	unsigned char *ACtable;
	unsigned char *DCtable;

} JpegEncodeState;

extern int encode_jpeg_lossless_color(
#ifdef NEED_PROTOTYPES
	JPegEncodeState *	 /* state */
#endif
);

extern int encode_jpeg_lossy_color(
#ifdef NEED_PROTOTYPES
	JPegEncodeState *	 /* state */
#endif
);

extern int encode_jpeg_lossless_gray(
#ifdef NEED_PROTOTYPES
	JPegEncodeState *	 /* state */
#endif
);

extern int encode_jpeg_lossy_gray(
#ifdef NEED_PROTOTYPES
	JPegEncodeState *	 /* state */
#endif
);
