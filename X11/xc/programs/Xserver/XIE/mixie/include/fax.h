/* $Xorg: fax.h,v 1.4 2001/02/09 02:04:28 xorgcvs Exp $ */
/**** module fax.h ****/
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

	fax.h -- DDXIE common include file for new spiffy fax modules

	Ben Fahy -- AGE Logic, Inc.  May 1993

******************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/include/fax.h,v 1.4 2001/12/14 19:58:30 dawes Exp $ */

#ifndef XFree86LOADER
#include <stdio.h>
#else
#ifndef NULL
#define NULL 0
#endif
#endif

/***	entry points */
extern int decode_g4();
extern int decode_g32d();
extern int decode_g31d();
extern int decode_tiff2();
extern int decode_tiffpb();

/* must match what is in pretab.h */
#define	FAX_MODE_Unknown	0
#define FAX_MODE_Pass		1
#define FAX_MODE_Horizontal	2
#define FAX_MODE_Vertical_L3	3
#define FAX_MODE_Vertical_L2	4
#define FAX_MODE_Vertical_L1	5
#define FAX_MODE_Vertical_C0	6
#define FAX_MODE_Vertical_R1	7
#define FAX_MODE_Vertical_R2	8
#define FAX_MODE_Vertical_R3	9

/* extension stuff is optional, so not part of SI  ***/
#define FAX_MODE_Extension_1D	FAX_MODE_Unknown
#define FAX_MODE_Extension_2D	FAX_MODE_Unknown

#define FAX_GOAL_StartNewLine			12
#define FAX_GOAL_DetermineMode			13
#define FAX_GOAL_HandleVertL3 			FAX_MODE_Vertical_L3
#define FAX_GOAL_HandleVertL2 			FAX_MODE_Vertical_L2
#define FAX_GOAL_HandleVertL1 			FAX_MODE_Vertical_L1
#define FAX_GOAL_HandleVert0  			FAX_MODE_Vertical_C0
#define FAX_GOAL_HandleVertR1 			FAX_MODE_Vertical_R1
#define FAX_GOAL_HandleVertR2 			FAX_MODE_Vertical_R2
#define FAX_GOAL_HandleVertR3 			FAX_MODE_Vertical_R3
#define FAX_GOAL_HandlePass 			FAX_MODE_Pass
#define FAX_GOAL_HandleHoriz 			FAX_MODE_Horizontal
#define FAX_GOAL_AccumulateA0A1			14
#define FAX_GOAL_RecordA0A1			15
#define FAX_GOAL_AccumulateA1A2			16
#define FAX_GOAL_RecordA1A2			17
#define FAX_GOAL_FinishHoriz 			18
#define FAX_GOAL_SeekEOLandTag			19
#define FAX_GOAL_SeekTagBit			20
#define FAX_GOAL_AdjustTag			21
#define FAX_GOAL_RecoverZero			22
#define FAX_GOAL_AdjustedButStillRecovering 	23
#define FAX_GOAL_FoundOneEOL			24
#define FAX_GOAL_SkipPastAnyToEOL		25
#define FAX_GOAL_SeekFillAndEOL			26
#define FAX_GOAL_FallOnSword			27

#define FAX_DECODE_DONE_NOT			0
#define FAX_DECODE_DONE_OK			1
#define FAX_DECODE_DONE_ErrorSkipPast		2
#define FAX_DECODE_DONE_ErrorBadFill		3
#define FAX_DECODE_DONE_ErrorBadGoal		4
#define FAX_DECODE_DONE_ErrorBadMagic		5
#define FAX_DECODE_DONE_ErrorBadStripper	6
#define FAX_DECODE_DONE_ErrorPastWidth		7
#define FAX_DECODE_DONE_ErrorBadCode		8
#define FAX_DECODE_DONE_ErrorBadPtr		9
#define FAX_DECODE_DONE_ErrorBadZero		10
#define FAX_DECODE_DONE_ErrorBada0a1		11
#define FAX_DECODE_DONE_ErrorBada1a2		12
#define FAX_DECODE_DONE_ErrorBadEOL		13
#define FAX_DECODE_DONE_ErrorBadA0pos		14
#define FAX_DECODE_DONE_ErrorPassAboveAllWhite	15
#define FAX_DECODE_DONE_ErrorMissingEOL		16


#define PB_GOAL_StartNewLine		0
#define PB_GOAL_GetRunLength		1
#define PB_GOAL_GetLiteralBytes		2
#define PB_GOAL_GetRepeatByte		3
#define PB_GOAL_RepeatBytes		4

typedef struct _bit_stream {
  int		 started;	/* 0 if requires initialization	    */
  unsigned char *byteptr;	/* address of current byte in strip */
  unsigned char *endptr;	/* 1 past last byte in strip	    */
  int 		 bitpos;	/* address of bit in byte (left->r) */
} BitStream;

#define StripStateNone	 0	/* no strip available yet		*/
#define StripStateNew	 1	/* offered to decoder for first time 	*/
#define StripStateInUse	 2	/* decoder is using the strip		*/
#define StripStateDone	 3	/* decoder is done  with the strip	*/
#define StripStateNoMore 4	/* that's all, folks. 			*/

typedef struct _fax_state {
	int final;	 /* if final = 1, all input data is available	*/
	int decoder_done;/* if done = 1, all data sent has been decoded */
	int write_data;	 /* distinguish between decode only		*/
	unsigned char *strip;	 /* latest available strip		*/
	int strip_size;	 /* size of that strips				*/
	int strip_state; /* whether strip is new, being used, or done	*/
	int use_magic;	 /* 1 means we are using magic strip		*/
	int magic_needs; /* 1 means magic strip only have filled	*/
	unsigned char magicStrip[8];
			 /* "pretend" strip for handling strip edges	*/
	int o_line;	 /* absolute output line position		*/
	int nl_sought;	 /* how many lines we want, this decode round	*/
	char **o_lines;  /* pointers to beginning of output lines	*/
	BitStream bits;	 /* definition of bitstream, current strip	*/
	int goal;	 /* our current goal				*/
	int a0_pos;	 /* starting changing position on coding line	*/
	int a1_pos;	 /* next changing position to right of a0_pos 	*/
	int a0a1;	 /* distance from a0 to a1			*/
	int b1_pos;	 /* first change above and right of a0, ~color	*/
	int b2_pos;	 /* next change above and right of a0, ao_color	*/
	int a0_color;	 /* color of a0,  either WHITE or BLACK		*/
	int n_old_trans; /* number of transitions, line above us	*/
	int *old_trans;	 /* list of transitions on line above us	*/
	int n_new_trans; /* number of transitions, current line (so far)*/
	int *new_trans;	 /* list of transitions on current line		*/
	int last_b1_idx; /* index of last saved transition we looked at	*/
	int length_acc;	 /* accumulates run-lengths for a0a1 or a1a2	*/
	int width;	 /* width of image.  So we know end of line	*/
	int g32d_horiz;	 /* only relevant for g32d encoding		*/
	int rl;		 /* needed if I get EOL in get_a0a1 or get_a1a2 */
	int radiometric; /* if 1, then expand white as 1, else 0	*/
} FaxState;

/* these definitions should agree with what is in pretab.h */
#define BAD_RUN_LENGTH	(-1)
#define EOL_RUN_LENGTH	(-2)
#define WHITE 		( 0)
#define BLACK 		(~WHITE)
#define MIN_BYTES_NEEDED 7
	/* sorry, we can't handle strips of less than 7 bytes.  :-(    */
	/* it wreaks havoc with our magic strips if we try to go lower */

#define WriteLineData(olp,w)	\
	zero_even(olp,new_trans,n_new_trans,w,state->radiometric);

#define FlushLineData()						    	\
	{								\
	   WriteLineData((char *)state->o_lines[lines_found],width); 	\
	   ++lines_found;						\
	}								
/* 
   note: I'm declaring this with no functions, so the caller won't
   be tempted to call with lines_found++, which wouldn't work
*/

/**** module fax.h ****/
