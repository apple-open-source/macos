/* $Xorg: gbits.h,v 1.4 2001/02/09 02:04:27 xorgcvs Exp $ */
/**** module fax/gbits.h ****/
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
  
	fax/gbits.h -- DDXIE fax decode bitstream - get macros
  
	Ben Fahy -- AGE Logic, Inc. May, 1993
  
*****************************************************************************/

/* 
 *	gbits.h - sub-include file of bits.h,  the 'g' stands for "get"
 *		  as in:
 *
 * 	bit      = get_bit( byteptr,bitpos,endptr);
 * 	byte     = get_byte(byteptr,bitpos,endptr);
 * 	code     = get_wcode(byteptr,bitpos,endptr);
 * 	code     = get_bcode(byteptr,bitpos,endptr);
 *
 *	but I am also including:
 *
 *	adjust_bitstream_8(n,byteptr,bitpos,endptr);
 *	adjust_bitstream(  n,byteptr,bitpos,endptr);
 *	adjust_1bit(         byteptr,bitpos,endptr);
 *
*/

/* ------------------------------------------------------------------- */
/*
 *	do_magic(byteptr,bitpos,endptr);
 *
 * 	Macro to attempt to recover from an end-of-strip condition.
 * 	It bridges the gap between current strip and the next by
 *	copying the last 4 bytes of current strip,  together with the
 *	first four bytes of the next strip, into a 'magic strip',
 *	which consists of at 8 bytes. Since no FAX codes are longer 
 *	than 13 bits in length, this implies that we have plenty of 
 *	room in the magic strip to finish off decoding everything that 
 *	was copied from the first strip.  When we hit the second long 
 *	word in the magic strip,  we will trip the endptr condition for 
 *	it and then restore the logical next strip,  marking the first 
 *	one as done. 
 *
 *	The nice thing about this method is that we don't have to do
 *	any copying of real-life strips.  We just insert a pseudo-strip
 *	for a very brief amount of time,  and the behavior is more or
 *	less hidden from routines outside this module.
 *
 */

#define do_magic(byteptr,bitpos,endptr)					 \
	{								 \
	  int o;							 \
	  if (state->use_magic) {					 \
	     /* already using magic strip, switch back to normal */	 \
	     								 \
	     if (state->strip_state == StripStateNoMore) {		 \
	       /* we are out of data! return number of lines found */	 \
	       state->decoder_done = 1;					 \
	     }								 \
	     else { 							 \
	        /* switch over to next input strip */			 \
	        o = byteptr - endptr;					 \
	        byteptr = (unsigned char *)state->strip + o;		 \
	        endptr  = (unsigned char *)state->strip+state->strip_size-4;\
	        state->use_magic = 0;  /* aw - the magic is gone  :-( */ \
	     }								 \
	  }								 \
	  else {							 \
	    /* will switch over to using magic strip */			 \
	    state->use_magic = 1;					 \
	    state->magicStrip[0] = *(endptr);				 \
	    state->magicStrip[1] = *(endptr+1);				 \
	    state->magicStrip[2] = *(endptr+2);				 \
	    state->magicStrip[3] = *(endptr+3);				 \
	  								 \
	    /* set byteptr in equivalent position in the magic strip */	 \
	    o = byteptr - endptr;					 \
	    byteptr = o + (unsigned char *)&state->magicStrip[0];	 \
	    endptr  = (unsigned char *)&state->magicStrip[4]; 		 \
	     /* will swap back after magicStrip[3] is done */		 \
	  								 \
	    /* now signal to caller that we want a new strip */		 \
	     state->magic_needs = 1;			 		 \
	     state->strip_state = StripStateDone;	 		 \
		/* you can toss the old one */				 \
	  								 \
	     save_state_and_return(state);				 \
	  }  /* end of if already magic */				 \
	}

/* ------------------------------------------------------------------- */
/*
 *	finish_magic(final);
 *
 * 	This macro finishes the job of assembling the magic strip.
 *	It plugs in the first long word of the new strip in as the
 *	second long word of the magic strip.
 *
 *	Note that when 'magic_needs' is set, the caller MUST NOT call 
 *	the decoder again until that strip is available, unless final
 *	is 1 (no more strips coming).
 */

#define finish_magic(final)						\
	{								\
	  if (state->strip_state == StripStateNew) {			\
	     /* what a guy! he gave me a new strip.  :-) */		\
	     state->magicStrip[4] = *(state->strip);			\
	     state->magicStrip[5] = *(state->strip+1);			\
	     state->magicStrip[6] = *(state->strip+2);			\
	     state->magicStrip[7] = *(state->strip+3);			\
	     state->strip_state = StripStateInUse;			\
	  } 								\
	  else {							\
	     /* no new strip?? Better have a good reason! */		\
	     if (final) {						\
	       /* Out of data (good reason), set 2nd magic word to 0 */	\
	       state->magicStrip[4] = 0;				\
	       state->magicStrip[5] = 0;				\
	       state->magicStrip[6] = 0;				\
	       state->magicStrip[7] = 0;				\
	     } 								\
	     else {							\
	       /* strips left, but I am DENIED?? Shocking! */		\
	       state->decoder_done = FAX_DECODE_DONE_ErrorBadMagic;	\
	       return(lines_found);					\
	    }								\
	  }  /* end of if (final) */					\
	  /* byteptr, bitpos, endptr should already be set up ok */	\
	  state->magic_needs = 0;					\
	}    /* end of finish_magic(final) */

/* ------------------------------------------------------------------- */
/*
 *	skip_bits_at_eol(byteptr,bitpos,endptr);
 *
 * 	Macro to move bitstream position forward to the next even byte
 * 	boundary, if necessary
 *
 */
#define skip_bits_at_eol(byteptr,bitpos,endptr)				\
	{								\
	  if (bitpos) {							\
	    bitpos = 0;							\
	    if (++byteptr >= endptr) 					\
	       do_magic(byteptr,bitpos,endptr);				\
	  }								\
	}

/* ------------------------------------------------------------------- */
/*
 *	adjust_bitstream_8(n,byteptr,bitpos,endptr);
 *
 * 	Macro to move bitstream position forward n bits, where n is
 * 	known to be <= 8.
 *
 * 	Strategy:
 *		There are only two cases:
 *
 *		bitpos+n < 8,	=> byteptr same, adjust bitpos
 *
 *		bitpos+n >= 8,  => bitpos = (bitpos+n)%8, ++byteptr,
 *					need to check endptr.
 */

#define adjust_bitstream_8(n,byteptr,bitpos,endptr)			\
	{ register int tmp=n+bitpos;					\
	    if (tmp < 8)						\
		bitpos=tmp ;	/* staying in same byte */		\
	    else {		/* moving to new byte   */		\
		bitpos=tmp%8; 	/* adjust bitpos 	*/ 		\
		if (++byteptr >= endptr)  				\
	             do_magic(byteptr,bitpos,endptr);			\
	    }								\
	}

/* ------------------------------------------------------------------- */
/*
 *	adjust_1bit(byteptr,bitpos,endptr);
 *
 * 	Macro to move bitstream position forward 1 bit
 *
 * 	Strategy:
 *		oh, poo!  Just leverage adjust_bitstream_8( )
 *
 */

#define adjust_1bit(byteptr,bitpos,endptr)				\
	adjust_bitstream_8(1,byteptr,bitpos,endptr)			

/* ------------------------------------------------------------------- */
/*
 *	adjust_bitstream(n,byteptr,bitpos,endptr);
 *
 * 	Macro to move bitstream position forward n bits, where n is
 * 	known to be <= 13.
 *
 *	Strategy:
 *		We may need to adjust from 1 to 13 bits.  It's easy
 *		to calculate the new bitpos - it's just (n+bitpos)%8.
 *		Similarly, the number of bytes to move over is given
 *		by (n+bitpos)/8.  
 *
 */

#define adjust_bitstream(n,byteptr,bitpos,endptr)			\
	{ register int tmp=n+bitpos;					\
	    if (tmp < 8)						\
		bitpos=tmp ;	/* staying in same byte */		\
	    else {		/* moving to new byte   */		\
		bitpos   = tmp%8;					\
		byteptr += tmp/8;					\
		if (byteptr >= endptr) 					\
	             do_magic(byteptr,bitpos,endptr);			\
	    }								\
	}

/* ------------------------------------------------------------------- */
/*
 * 	bit      = get_bit(byteptr,bitpos,endptr);
 *
 * 	Macro to get next coding bit (when searching for tag bit, g32d). 
 * 
 *	Strategy:
 *		mask everything but current bitpos, where bitpos counts
 *		from msbit to lsbit (bitpos=0 means msbit)
 *
 *	NOTE:   we do *not* try to align the bit in any way.  It is 
 *		assumed that the whole masked byte will be inspected 
 *		at once for non-zero status.
 *
 */
#define get_bit(byteptr,bitpos,endptr) 					\
	(  (*byteptr) & ((unsigned char) 0x080 >> bitpos) ) 

	/* Notice endptr isn't actually used, it's just there for show */
/* ------------------------------------------------------------------- */
/*
 * 	byte     = get_byte(byteptr,bitpos,endptr);
 *
 * 	Macro to get next coding byte. 
 * 
 * 	IMPORTANT!!  There is no check to make sure that byteptr+1
 *		actually point to valid data in the macro below. It
 *		is assumed this macro will never be called unless it
 *		has already been verified it is safe to do so.
 *
 *	Strategy:
 *		If bitpos is 0,  all data available in this byte, and
 *		it's trivial. So let's assume bitpos != 0, so that the
 *		8 bits we want straddles two bytes, as below:
 *
 *		bitpos = 3, counting from the left:
 *
 *		7   6   5   4   3   2   1   0   7  6  5  4  3  2  1  0
 *		            X   X   X   X   X   Y  Y  Y
 *
 *		We want to form an 8-bit byte out of bits XXXXXYYY as
 *		shown.  By inspection, we need to shift the first byte to 
 *		the left three bits, and add the second byte shifted to 
 *		the right the complementary number of times.  Thus:
 */
#define get_byte(byteptr,bitpos,endptr) 				\
	(  bitpos?  ( (*byteptr)    <<  bitpos    ) | 			\
		    ( (*(byteptr+1))>> (8-bitpos) ) :			\
	   *byteptr							\
	)

	/* Notice endptr isn't actually used, it's just there for show */

#define get_pbyte(byteptr,endptr)					\
	(  *byteptr )


/* ------------------------------------------------------------------- */
/*
 * 	code     = get_wcode(byteptr,bitpos,endptr);
 *
 * 	Macro to get next white run length
 * 
 * 	IMPORTANT!!  There is no check to make sure that byteptr+1 or
 *		byteptr+2 actually point to valid data in the macro 
 *		below. It is assumed this macro will never be called 
 *		unless it has already been verified it is safe to do so.
 *
 *	Strategy:
 *		For the white table we need 12 bits.  If bitpos <=4,
 *		then the first byte will have at least four bits, so
 *		we only need two bytes.  If bitpos >4, we need three
 *		bytes.
 *
 *		bitpos = 4, starting from 0 counting from the left:
 *
 *		0 1 2 3 4
 *		7 6 5 4 3 2 1 0   7 6 5 4 3 2 1 0   7 6 5 4 3 2 1 0   
 *		        X X X X   Y Y Y Y Y Y Y Y   
 *
 *		bitpos = 5, starting from 0 counting from the left:
 *
 *		0 1 2 3 4 5
 *		7 6 5 4 3 2 1 0   7 6 5 4 3 2 1 0   7 6 5 4 3 2 1 0   
 *		          X X X   Y Y Y Y Y Y Y Y   Z
 *
 *		Supose bitpos <=4. For example,  suppose bitpos = 3,
 *		as is pictured below:
 *
 *		0 1 2 3 4
 *		7 6 5 4 3 2 1 0   7 6 5 4 3 2 1 0   7 6 5 4 3 2 1 0   
 *		      X X X X X   Y Y Y Y Y Y Y     
 *
 *		We need to form a 12 bit word out of XXXXXYYYYYYY. By
 *		inspection it is easy to see that (4-bitpos) represents
 *		the number of bits "spilled" to the left of bitpos=4.
 *		The left-most bit needs to be shifted right to the bit4
 *		position,  so the solution is to form a word from the
 *		first two bytes, shift right by (4-bitpos) and mask. 
 *
 *		For the case of bitpos=5 (above), (bitpos-4) bits have
 *		"spilled" into the third byte.  We need to form a word
 *		out of the three consecutive bytes and right shift by
 *		the amount (8-spill) = 8 - (bitpos-4) = 12-bitpos, then
 *		mask.
 */
#define two_bytes	 						\
	( (((unsigned int)*byteptr)<<8)  | (*(byteptr+1)) ) 	

#define three_bytes	 						\
	( (((unsigned int)*byteptr)<<16) |				\
	  (((unsigned int)*(byteptr+1))<<8) |				\
	                 (*(byteptr+2)) )

#define get_wcode(byteptr,bitpos,endptr) 				\
	(  0x0fff & 							\
	     (	bitpos>4? ( three_bytes >> (12-bitpos) ) 		\
			: (   two_bytes >> ( 4-bitpos) )		\
	     )								\
	)

	/* Notice endptr isn't actually used, it's just there for show */

/* ------------------------------------------------------------------- */
/*
 * 	code     = get_bcode(byteptr,bitpos,endptr);
 *
 * 	Macro to get next black run length.
 * 
 * 	IMPORTANT!!  There is no check to make sure that byteptr+1 or
 *		byteptr+2 actually point to valid data in the macro 
 *		below. It is assumed this macro will never be called 
 *		unless it has already been verified it is safe to do so.
 *
 *	Strategy:
 *		For the black table we need 13 bits.  If bitpos <=3,
 *		then the first byte will have at least five bits, so
 *		we only need two bytes.  If bitpos >=4, we need three
 *		bytes.
 *
 *		bitpos = 3, starting from 0 counting from the left:
 *
 *		0 1 2 3  
 *		7 6 5 4 3 2 1 0   7 6 5 4 3 2 1 0   7 6 5 4 3 2 1 0   
 *		      X X X X X   Y Y Y Y Y Y Y Y   
 *
 *		bitpos = 4, starting from 0 counting from the left:
 *
 *		0 1 2 3 4  
 *		7 6 5 4 3 2 1 0   7 6 5 4 3 2 1 0   7 6 5 4 3 2 1 0   
 *		        X X X X   Y Y Y Y Y Y Y Y   Z
 *
 *		Supose bitpos <=3. For example,  suppose bitpos = 2,
 *		as is pictured below:
 *
 *		0 1 2 3  
 *		7 6 5 4 3 2 1 0   7 6 5 4 3 2 1 0   7 6 5 4 3 2 1 0   
 *		    X X X X X X   Y Y Y Y Y Y Y     
 *
 *		We need to form a 13 bit word out of XXXXXXYYYYYYY. By
 *		inspection it is easy to see that (3-bitpos) represents
 *		the number of bits "spilled" to the left of bitpos=3.
 *		The left-most bit needs to be shifted right to the bit3
 *		position,  so the solution is to form a word from the
 *		first two bytes, shift right by (3-bitpos) and mask. 
 *
 *		For the case of bitpos=4 (above), (bitpos-3) bits have
 *		"spilled" into the third byte.  We need to form a word
 *		out of the three consecutive bytes and right shift by
 *		the amount (8-spill) = 8 - (bitpos-3) = 11-bitpos, then
 *		mask.
 */

#define get_bcode(byteptr,bitpos,endptr) 				\
	(  0x1fff & 							\
	     (	bitpos>3? ( three_bytes >> (11-bitpos) ) 		\
			: (   two_bytes >> ( 3-bitpos) )		\
	     )								\
	)

	/* Notice endptr isn't actually used, it's just there for show */

/* ------------------------------------------------------------------- */
/**** module fax/gbits.h ****/
