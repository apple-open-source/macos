/* $Xorg: fencode.h,v 1.4 2001/02/09 02:04:27 xorgcvs Exp $ */
/**** module fencode.h ****/
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

	fencode.h -- DDXIE header module for FAX encoders

	Ben Fahy, AGE Logic, Sept 1993 

******************************************************************************/

#include "misc.h" /* for pointer */
#include <Xmd.h>  /* for CARD32 */

/***	entry points */
int encode_g4();
int encode_g32d();
int encode_g31d();
int encode_tiffpb();
int encode_tiff2();

#define FAX_ENCODE_DONE_NOT			0
#define FAX_ENCODE_DONE_OK			1
#define ENCODE_ERROR_BadMagic			2
#define ENCODE_ERROR_BadGoal			3
#define ENCODE_ERROR_BadStager			4
#define ENCODE_ERROR_BadRunLength		5
#define ENCODE_ERROR_BadState			6
#define ENCODE_ERROR_StripStateNotNew		7
#define ENCODE_ERROR_NoB1Found			8
#define ENCODE_ERROR_EncodeRunsFailure		9

#define ENCODE_FAX_GOAL_StartNewLine		1
#define ENCODE_FAX_GOAL_OutputNextRunLength	2
#define ENCODE_FAX_GOAL_DeduceCode		3
#define ENCODE_FAX_GOAL_FlushStager		4
#define ENCODE_FAX_GOAL_FinishLine		5
#define ENCODE_FAX_GOAL_FlushEOL		6
#define ENCODE_FAX_GOAL_EOLWritten		7
#define ENCODE_FAX_GOAL_FindPositions		8
#define ENCODE_FAX_GOAL_PassMode		9
#define ENCODE_FAX_GOAL_VerticalMode		10
#define ENCODE_FAX_GOAL_HorizontalMode		11
#define ENCODE_FAX_GOAL_Flush2DStager		12
#define ENCODE_FAX_GOAL_DoneWithA0A1		13
#define ENCODE_FAX_GOAL_DoneWithA1A2		14
#define ENCODE_FAX_GOAL_FlushHCode		15
#define ENCODE_FAX_GOAL_FlushedHCode		16
#define ENCODE_FAX_GOAL_G4SneaksIn		17

#define ENCODE_PB_GOAL_StartNewLine		1
#define ENCODE_PB_GOAL_OutputNextRunLength	2
#define ENCODE_PB_GOAL_FlushRunLength		3
#define ENCODE_PB_GOAL_FlushLiterals		4
#define ENCODE_PB_GOAL_WriteLiterals		5
#define ENCODE_PB_GOAL_WriteCodeForRunLength	6
#define ENCODE_PB_GOAL_WriteValueForRunLength	7
#define ENCODE_PB_GOAL_WriteNlits		8
#define ENCODE_PB_GOAL_WriteThisLiteral		9
#define ENCODE_PB_GOAL_WriteTheLiteral		10

typedef struct _fax_encode_state {
	int final;	 /* if final = 1, all input data is available	*/
	int encoder_done;/* if done = 1, all data sent has been encoded */

	char **i_lines;  /* pointers to beginning of input lines	*/
	int i_line;	 /* absolute input line position		*/
	int nl_tocode;	 /* how many lines we want encoded		*/

	unsigned char *strip;	 /* latest available *output* strip	*/
	int strip_size;	 /* size of that strips				*/
	int strip_state; /* whether strip is new, being used, or done	*/
	int use_magic;	 /* 1 means we are using magic strip		*/
	int magic_needs; /* 1 means magic strip only have filled	*/
	unsigned char magicStrip[8];
			 /* "pretend" strip for handling strip edges	*/
	BitStream bits;	 /* definition of bitstream, current strip	*/

	pointer private;/* private data, depends on encoding scheme	*/

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
	CARD32 stager;	 /* place for bits to be merged into bytes	*/
	int radiometric; /* if 1, then white is bright (1)		*/
} FaxEncodeState;

typedef struct _tiff2_epvt {
	int	*counts;	/* runlengths for W/B runs 	*/
	int	nvals;		/* number of entries in tables 	*/
	int	index;		/* current position in tables	*/
	int	rlcode;		/* code we need to write out	*/
	int	terminating;	/* whether this is end or not 	*/
	int	codelength;	/* how many bits a code takes	*/
	int	align_eol;	/* pad last byte in a line w/0	*/

} Tiff2EncodePvt,G31DEncodePvt;

typedef struct _packbits_epvt {
	int	*values;	/* array of byte values 	*/
	int	*counts;	/* runlengths for byte values 	*/
	int	nvals;		/* number of entries in tables 	*/
	int	nlits;		/* number of literals to write  */
	int	index;		/* current position in tables	*/
	int	start;		/* start position in tables	*/
	int	lits_to_write;	/* how many to write at one time*/
	int	rlcode;		/* code we need to write out	*/
	int	bytes_out;	/* sum of literals flushed	*/

} PackBitsEncodePvt;

typedef struct _g32d_epvt {
	int	*counts;	/* runlengths for W/B runs 	*/
	int	nvals;		/* number of entries in tables 	*/
	int	index;		/* current position in tables	*/
	int 	save_index;	/* save for recovery of index	*/
	int	*above;		/* runlengths for lines above	*/
	int	avals;		/* number of values, above	*/
	int	aindex;		/* current position in above	*/
	int	save_aindex;	/* save for recover of aindex	*/
	int	rlcode;		/* code we need to write out	*/
	int	terminating;	/* whether this is end or not 	*/
	int	codelength;	/* how many bits a code takes	*/
	int	k;		/* do 1-D every K lines		*/
	int	kcnt;		/* count of what k we are on	*/
	int	next_goal;	/* what to do after rl encode	*/
	int	a1a2;		/* second distance in Hmode	*/
	int 	a2_pos;	 	/* next position to right of a1	*/
	int 	save_b1pos;	/* makes it easier to back up	*/
	int 	save_b1color;	/* makes it easier to back up	*/
	int 	b1_color;	/* not a part of state 		*/
	int	align_eol;	/* pad last byte in a line w/0	*/
	int	uncompressed;	/* not that we should use it... */
	int	really_g4;	/* if (1), skip EOL junk	*/

} G32DEncodePvt,G42DEncodePvt;

#if  defined(_G32D)

#define save_state_and_return(state) 					\
	{								\
	    state->private 	= (pointer) epvt;			\
	    epvt->counts 	= counts;				\
	    epvt->above 	= above;				\
	    epvt->nvals 	= nvals;				\
	    epvt->avals 	= avals;				\
	    epvt->index 	= index;				\
	    epvt->aindex 	= aindex;				\
	    epvt->rlcode	= rlcode;				\
	    epvt->terminating	= terminating;				\
	    epvt->codelength	= codelength;				\
	    epvt->save_index	= save_index;				\
	    epvt->save_aindex	= save_aindex;				\
	    epvt->k		= k;					\
	    epvt->kcnt		= kcnt;					\
	    epvt->a1a2		= a1a2;					\
	    epvt->save_b1pos	= save_b1pos;				\
	    epvt->save_b1color	= save_b1color;				\
	    epvt->next_goal	= next_goal;				\
	    epvt->b1_color     	= b1_color;				\
	    state->bits.byteptr = byteptr;				\
	    state->bits.endptr  = endptr;				\
	    state->bits.bitpos  = bitpos;				\
	    state->rl		= rl;					\
	    state->goal     	= goal;					\
	    state->stager     	= stager;				\
	    state->a0_color     = a0_color;				\
	    state->a0a1     	= a0a1;					\
	    state->a0_pos     	= a0_pos;				\
	    state->a1_pos     	= a1_pos;				\
	    epvt->a2_pos     	= a2_pos;				\
	    state->b1_pos     	= b1_pos;				\
	    state->length_acc   = length_acc;				\
	    return(lines_coded);					\
	}

#define	localize_state(state)						\
	    epvt 		= (G32DEncodePvt *) state->private;	\
	    counts 		= epvt->counts;				\
	    nvals 		= epvt->nvals;				\
	    index 		= epvt->index;				\
	    above 		= epvt->above;				\
	    avals 		= epvt->avals;				\
	    aindex 		= epvt->aindex;				\
	    rlcode 		= epvt->rlcode;				\
	    terminating		= epvt->terminating;			\
	    codelength		= epvt->codelength;			\
	    save_index		= epvt->save_index;			\
	    save_aindex		= epvt->save_aindex;			\
	    k			= epvt->k;				\
	    kcnt		= epvt->kcnt;				\
	    a1a2		= epvt->a1a2;				\
	    save_b1pos		= epvt->save_b1pos;			\
	    save_b1color	= epvt->save_b1color;			\
	    next_goal		= epvt->next_goal;			\
	    b1_color		= epvt->b1_color;			\
	    byteptr	  	= state->bits.byteptr;			\
	    endptr    		= state->bits.endptr;			\
	    bitpos    		= state->bits.bitpos;			\
	    goal	  	= state->goal;				\
	    stager	  	= state->stager;			\
	    width		= state->width;				\
	    lines_to_code	= state->nl_tocode;			\
	    a0_color		= state->a0_color;			\
	    a0a1		= state->a0a1;				\
	    a0_pos		= state->a0_pos;			\
	    a1_pos		= state->a1_pos;			\
	    a2_pos		=  epvt->a2_pos;			\
	    b1_pos  		= state->b1_pos;			\
	    length_acc		= state->length_acc;			\
	    rl			= state->rl;

#endif  /* defined(_G32D) */

#if defined(_TIFF2) || defined(_G31D)

#define save_state_and_return(state) 					\
	{								\
	    state->private 	= (pointer) epvt;			\
	    epvt->counts 	= counts;				\
	    epvt->nvals 	= nvals;				\
	    epvt->index 	= index;				\
	    epvt->rlcode	= rlcode;				\
	    epvt->terminating	= terminating;				\
	    epvt->codelength	= codelength;				\
	    state->bits.byteptr = byteptr;				\
	    state->bits.endptr  = endptr;				\
	    state->bits.bitpos  = bitpos;				\
	    state->rl		= rl;					\
	    state->goal     	= goal;					\
	    state->stager     	= stager;				\
	    state->a0_color     = a0_color;				\
	    return(lines_coded);					\
	}

#define	localize_state(state)						\
	    epvt 		= (Tiff2EncodePvt *) state->private;	\
	    counts 		= epvt->counts;				\
	    nvals 		= epvt->nvals;				\
	    index 		= epvt->index;				\
	    rlcode 		= epvt->rlcode;				\
	    terminating		= epvt->terminating;			\
	    codelength		= epvt->codelength;			\
	    byteptr	  	= state->bits.byteptr;			\
	    endptr    		= state->bits.endptr;			\
	    bitpos    		= state->bits.bitpos;			\
	    goal	  	= state->goal;				\
	    stager	  	= state->stager;			\
	    width		= state->width;				\
	    lines_to_code	= state->nl_tocode;			\
	    a0_color		= state->a0_color;			\
	    rl			= state->rl;

#endif  /* defined(_TIFF2) */

#if defined(_PBits)


#define save_state_and_return(state) 					\
	{								\
	    state->private 	= (pointer) epvt;			\
	    epvt->index 	= index;				\
	    epvt->start 	= start;				\
	    epvt->nlits 	= nlits;				\
	    epvt->nvals 	= nvals;				\
	    epvt->counts 	= counts;				\
	    epvt->values 	= values;				\
	    epvt->lits_to_write	= lits_to_write;			\
	    epvt->bytes_out	= bytes_out;				\
	    epvt->rlcode	= rlcode;				\
	    state->bits.byteptr = byteptr;				\
	    state->bits.endptr  = endptr;				\
	    state->rl		= rl;					\
	    state->goal     	= goal;					\
	    return(lines_coded);					\
	}

#define	localize_state(state)						\
	    epvt 		= (PackBitsEncodePvt *) state->private;	\
	    index 		= epvt->index;				\
	    start 		= epvt->start;				\
	    nlits 		= epvt->nlits;				\
	    nvals 		= epvt->nvals;				\
	    counts 		= epvt->counts;				\
	    values 		= epvt->values;				\
	    lits_to_write	= epvt->lits_to_write;			\
	    bytes_out		= epvt->bytes_out;			\
	    rlcode 		= epvt->rlcode;				\
	    byteptr	  	= state->bits.byteptr;			\
	    endptr    		= state->bits.endptr;			\
	    goal	  	= state->goal;				\
	    width		= state->width;				\
	    lines_to_code	= state->nl_tocode;			\
	    rl			= state->rl;

#endif  /* defined(_Pbits) */
