/* $Xorg: jchuff.c,v 1.4 2001/02/09 02:04:28 xorgcvs Exp $ */
/* Module jchuff.c */

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

	Gary Rogers, AGE Logic, Inc., October 1993
	Gary Rogers, AGE Logic, Inc., January 1994

****************************************************************************/

/*
 * jchuff.c
 *
 * Copyright (C) 1991, 1992, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains Huffman entropy encoding routines.
 * These routines are invoked via the methods entropy_encode,
 * entropy_encode_init/term, and entropy_optimize.
 */

#include "jinclude.h"
#include "macro.h"


/* Static variables to avoid passing 'round extra parameters */

static compress_info_ptr cinfo;

static INT32 huff_put_buffer;	/* current bit-accumulation buffer */
static int huff_put_bits;	/* # of bits now in it */

#ifndef XIE_SUPPORTED
static char * output_buffer;	/* output buffer */
static int bytes_in_buffer;
#endif	/* XIE_SUPPORTED */


LOCAL void
#ifdef XIE_SUPPORTED
#if NeedFunctionPrototypes
fix_huff_tbl (HUFF_TBL * htbl)
#else
fix_huff_tbl (htbl)
	HUFF_TBL * htbl;
#endif	/* NeedFunctionPrototypes */
#else
fix_huff_tbl (HUFF_TBL * htbl)
#endif	/* XIE_SUPPORTED */
/* Compute derived values for a Huffman table */
{
  int p, i, l, lastp, si;
  char huffsize[257];
  UINT16 huffcode[257];
  UINT16 code;
  
  /* Figure C.1: make table of Huffman code length for each symbol */
  /* Note that this is in code-length order. */

  p = 0;
  for (l = 1; l <= 16; l++) {
    for (i = 1; i <= (int) htbl->bits[l]; i++)
      huffsize[p++] = (char) l;
  }
  huffsize[p] = 0;
  lastp = p;
  
  /* Figure C.2: generate the codes themselves */
  /* Note that this is in code-length order. */
  
  code = 0;
  si = huffsize[0];
  p = 0;
  while (huffsize[p]) {
    while (((int) huffsize[p]) == si) {
      huffcode[p++] = code;
      code++;
    }
    code <<= 1;
    si++;
  }
  
  /* Figure C.3: generate encoding tables */
  /* These are code and size indexed by symbol value */

  /* Set any codeless symbols to have code length 0;
   * this allows emit_bits to detect any attempt to emit such symbols.
   */
  MEMZERO(htbl->ehufsi, SIZEOF(htbl->ehufsi));

  for (p = 0; p < lastp; p++) {
    htbl->ehufco[htbl->huffval[p]] = huffcode[p];
    htbl->ehufsi[htbl->huffval[p]] = huffsize[p];
  }
  
  /* We don't bother to fill in the decoding tables mincode[], maxcode[], */
  /* and valptr[], since they are not used for encoding. */
}


/* Outputting bytes to the file */

#ifdef XIE_SUPPORTED
LOCAL void
#if NeedFunctionPrototypes
flush_bytes (void)
#else
flush_bytes ()
#endif	/* NeedFunctionPrototypes */
{
}

#define emit_byte(val)  \
  MAKESTMT( if (cinfo->bytes_in_buffer >= cinfo->jpeg_buf_size) \
	      return(-1); \
	    cinfo->output_buffer[cinfo->bytes_in_buffer++] = (char) (val); )

#else
LOCAL void
flush_bytes (void)
{
  if (bytes_in_buffer)
    (*cinfo->methods->entropy_output) (cinfo, output_buffer, bytes_in_buffer);
  bytes_in_buffer = 0;
}

#define emit_byte(val)  \
  MAKESTMT( if (bytes_in_buffer >= JPEG_BUF_SIZE) \
	      flush_bytes(); \
	    output_buffer[bytes_in_buffer++] = (char) (val); )

#endif	/* XIE_SUPPORTED */

/* Outputting bits to the file */

/* Only the right 24 bits of huff_put_buffer are used; the valid bits are
 * left-justified in this part.  At most 16 bits can be passed to emit_bits
 * in one call, and we never retain more than 7 bits in huff_put_buffer
 * between calls, so 24 bits are sufficient.
 */

INLINE
#ifdef XIE_SUPPORTED
LOCAL int
#if NeedFunctionPrototypes
emit_bits (UINT16 code, int size)
#else
emit_bits (code, size)
	UINT16 code;
	int size;
#endif	/* NeedFunctionPrototypes */
#else
LOCAL void
emit_bits (UINT16 code, int size)
#endif	/* XIE_SUPPORTED */
{
  /* This routine is heavily used, so it's worth coding tightly. */
  register INT32 put_buffer = code;
#ifdef XIE_SUPPORTED
  register int put_bits = cinfo->huff_put_bits;
#else
  register int put_bits = huff_put_bits;
#endif	/* XIE_SUPPORTED */

  /* if size is 0, caller used an invalid Huffman table entry */
  if (size == 0)
#ifdef XIE_SUPPORTED
    return(XIE_ERR);
#else
    ERREXIT(cinfo->emethods, "Missing Huffman code table entry");
#endif	/* XIE_SUPPORTED */

  put_buffer &= (((INT32) 1) << size) - 1; /* Mask off any excess bits in code */
  
  put_bits += size;		/* new number of bits in buffer */
  
  put_buffer <<= 24 - put_bits; /* align incoming bits */

#ifdef XIE_SUPPORTED
  put_buffer |= cinfo->huff_put_buffer; /* and merge with old buffer contents */
#else
  put_buffer |= huff_put_buffer; /* and merge with old buffer contents */
#endif	/* XIE_SUPPORTED */
  
  while (put_bits >= 8) {
    int c = (int) ((put_buffer >> 16) & 0xFF);
    
    emit_byte(c);
    if (c == 0xFF) {		/* need to stuff a zero byte? */
      emit_byte(0);
    }
    put_buffer <<= 8;
    put_bits -= 8;
  }

#ifdef XIE_SUPPORTED
  cinfo->huff_put_buffer = put_buffer;	/* Update global variables */
  cinfo->huff_put_bits = put_bits;
  return(0);
#else
  huff_put_buffer = put_buffer;	/* Update global variables */
  huff_put_bits = put_bits;
#endif	/* XIE_SUPPORTED */
}


#ifdef XIE_SUPPORTED
LOCAL int
#if NeedFunctionPrototypes
flush_bits (void)
#else
flush_bits ()
#endif	/* NeedFunctionPrototypes */
{
  register int status;
  	
  /* fill any partial byte with ones */
  if ((status = emit_bits((UINT16) 0x7F, 7)) < 0)
  	return(status);
  cinfo->huff_put_buffer = 0;		/* and reset bit-buffer to empty */
  cinfo->huff_put_bits = 0;
  return(0);
}
#else
LOCAL void
flush_bits (void)
{
  emit_bits((UINT16) 0x7F, 7);	/* fill any partial byte with ones */
  huff_put_buffer = 0;		/* and reset bit-buffer to empty */
  huff_put_bits = 0;
}
#endif	/* XIE_SUPPORTED */



/* Encode a single block's worth of coefficients */
/* Note that the DC coefficient has already been converted to a difference */

#ifdef XIE_SUPPORTED
LOCAL int
#if NeedFunctionPrototypes
encode_one_block (JBLOCK block, HUFF_TBL *dctbl, HUFF_TBL *actbl)
#else
encode_one_block (block, dctbl, actbl)
	JBLOCK block;
	HUFF_TBL *dctbl;
	HUFF_TBL *actbl;
#endif	/* NeedFunctionPrototypes */
{
  register int status;
#else
LOCAL void
encode_one_block (JBLOCK block, HUFF_TBL *dctbl, HUFF_TBL *actbl)
{
#endif	/* XIE_SUPPORTED */
  register int temp, temp2;
  register int nbits;
  register int k, r, i;
  
  /* Encode the DC coefficient difference per section F.1.2.1 */
  
  temp = temp2 = block[0];

  if (temp < 0) {
    temp = -temp;		/* temp is abs value of input */
    /* For a negative input, want temp2 = bitwise complement of abs(input) */
    /* This code assumes we are on a two's complement machine */
    temp2--;
  }
  
  /* Find the number of bits needed for the magnitude of the coefficient */
  nbits = 0;
  while (temp) {
    nbits++;
    temp >>= 1;
  }
  
  /* Emit the Huffman-coded symbol for the number of bits */
#ifdef XIE_SUPPORTED
  if ((status = emit_bits(dctbl->ehufco[nbits], dctbl->ehufsi[nbits])) < 0)
    return(status);
#else
  emit_bits(dctbl->ehufco[nbits], dctbl->ehufsi[nbits]);
#endif	/* XIE_SUPPORTED */

  /* Emit that number of bits of the value, if positive, */
  /* or the complement of its magnitude, if negative. */
  if (nbits)			/* emit_bits rejects calls with size 0 */
#ifdef XIE_SUPPORTED
  {
    if ((status = emit_bits((UINT16) temp2, nbits)) < 0)
      return(status);
  }
#else
    emit_bits((UINT16) temp2, nbits);
#endif	/* XIE_SUPPORTED */
  
  /* Encode the AC coefficients per section F.1.2.2 */
  
  r = 0;			/* r = run length of zeros */
  
  for (k = 1; k < DCTSIZE2; k++) {
    if ((temp = block[k]) == 0) {
      r++;
    } else {
      /* if run length > 15, must emit special run-length-16 codes (0xF0) */
      while (r > 15) {
#ifdef XIE_SUPPORTED
        if ((status = emit_bits(actbl->ehufco[0xF0], actbl->ehufsi[0xF0])) < 0)
          return(status);
#else
        emit_bits(actbl->ehufco[0xF0], actbl->ehufsi[0xF0]);
#endif	/* XIE_SUPPORTED */
        r -= 16;
      }

      temp2 = temp;
      if (temp < 0) {
        temp = -temp;		/* temp is abs value of input */
        /* This code assumes we are on a two's complement machine */
        temp2--;
      }
      
      /* Find the number of bits needed for the magnitude of the coefficient */
      nbits = 1;		/* there must be at least one 1 bit */
      while (temp >>= 1)
        nbits++;
      
      /* Emit Huffman symbol for run length / number of bits */
      i = (r << 4) + nbits;
#ifdef XIE_SUPPORTED
      if ((status = emit_bits(actbl->ehufco[i], actbl->ehufsi[i])) < 0)
        return(status);
#else
      emit_bits(actbl->ehufco[i], actbl->ehufsi[i]);
#endif	/* XIE_SUPPORTED */
      
      /* Emit that number of bits of the value, if positive, */
      /* or the complement of its magnitude, if negative. */
#ifdef XIE_SUPPORTED
      if ((status = emit_bits((UINT16) temp2, nbits)) < 0)
        return(status);
#else
      emit_bits((UINT16) temp2, nbits);
#endif	/* XIE_SUPPORTED */
      
      r = 0;
    }
  }

  /* If the last coef(s) were zero, emit an end-of-block code */
  if (r > 0)
#ifdef XIE_SUPPORTED
  {
    if ((status = emit_bits(actbl->ehufco[0], actbl->ehufsi[0])) < 0)
        return(status);
  }
  return(0);
#else
    emit_bits(actbl->ehufco[0], actbl->ehufsi[0]);
#endif	/* XIE_SUPPORTED */
}



/*
 * Initialize for a Huffman-compressed scan.
 * This is invoked after writing the SOS marker.
 * The pipeline controller must establish the entropy_output method pointer
 * before calling this routine.
 */

#ifdef XIE_SUPPORTED
METHODDEF int
#if NeedFunctionPrototypes
huff_init (compress_info_ptr xinfo)
#else
huff_init (xinfo)
	compress_info_ptr xinfo;
#endif	/* NeedFunctionPrototypes */
#else
METHODDEF void
huff_init (compress_info_ptr xinfo)
#endif	/* XIE_SUPPORTED */
{
  short ci;
  jpeg_component_info * compptr;

  /* Initialize static variables */
  cinfo = xinfo;

#ifdef XIE_SUPPORTED
  cinfo->huff_put_buffer = 0;
  cinfo->huff_put_bits = 0;
  cinfo->bytes_in_buffer = 0;
#else
  huff_put_buffer = 0;
  huff_put_bits = 0;

  /* Initialize the output buffer */
  output_buffer = (char *) (*cinfo->emethods->alloc_small)
				((size_t) JPEG_BUF_SIZE);
  bytes_in_buffer = 0;
#endif	/* XIE_SUPPORTED */

  for (ci = 0; ci < cinfo->comps_in_scan; ci++) {
    compptr = cinfo->cur_comp_info[ci];
    /* Make sure requested tables are present */
    if (cinfo->dc_huff_tbl_ptrs[compptr->dc_tbl_no] == NULL ||
	cinfo->ac_huff_tbl_ptrs[compptr->ac_tbl_no] == NULL)
#ifdef XIE_SUPPORTED
      return(XIE_ERR);
#else
      ERREXIT(cinfo->emethods, "Use of undefined Huffman table");
#endif	/* XIE_SUPPORTED */
    /* Compute derived values for Huffman tables */
    /* We may do this more than once for same table, but it's not a big deal */
    fix_huff_tbl(cinfo->dc_huff_tbl_ptrs[compptr->dc_tbl_no]);
    fix_huff_tbl(cinfo->ac_huff_tbl_ptrs[compptr->ac_tbl_no]);
    /* Initialize DC predictions to 0 */
    cinfo->last_dc_val[ci] = 0;
  }

  /* Initialize restart stuff */
  cinfo->restarts_to_go = cinfo->restart_interval;
  cinfo->next_restart_num = 0;
#ifdef XIE_SUPPORTED
  return(0);
#endif	/* XIE_SUPPORTED */
}


/*
 * Emit a restart marker & resynchronize predictions.
 */

#ifdef XIE_SUPPORTED
LOCAL int
#if NeedFunctionPrototypes
emit_restart (compress_info_ptr cinfo)
#else
emit_restart (cinfo)
	compress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
{
  int status;	
#else
LOCAL void
emit_restart (compress_info_ptr cinfo)
{
#endif	/* XIE_SUPPORTED */
  short ci;

#ifdef XIE_SUPPORTED
  if ((status = flush_bits()) < 0)
    return(status);
#else
  flush_bits();
#endif	/* XIE_SUPPORTED */

  emit_byte(0xFF);
  emit_byte(RST0 + cinfo->next_restart_num);

  /* Re-initialize DC predictions to 0 */
  for (ci = 0; ci < cinfo->comps_in_scan; ci++)
    cinfo->last_dc_val[ci] = 0;

  /* Update restart state */
  cinfo->restarts_to_go = cinfo->restart_interval;
  cinfo->next_restart_num++;
  cinfo->next_restart_num &= 7;
#ifdef XIE_SUPPORTED
  return(0);
#endif	/* XIE_SUPPORTED */
}


/*
 * Encode and output one MCU's worth of Huffman-compressed coefficients.
 */
#ifdef XIE_SUPPORTED
METHODDEF int
#if NeedFunctionPrototypes
huff_encode (compress_info_ptr xinfo, JBLOCK *MCU_data)
#else
huff_encode (xinfo, MCU_data)
	compress_info_ptr xinfo;
	JBLOCK *MCU_data;
#endif	/* NeedFunctionPrototypes */
{
  int status;	
#else
METHODDEF void
huff_encode (compress_info_ptr xinfo, JBLOCK *MCU_data)
{
#endif	/* XIE_SUPPORTED */
  short blkn, ci;
  jpeg_component_info * compptr;
  JCOEF temp;
#ifdef XIE_SUPPORTED
  JCOEF last_dc_val[MAX_COMPS_IN_SCAN]; /* last DC coef for each comp */
  for (ci = 0; ci < cinfo->comps_in_scan; ci++)
    last_dc_val[ci] = cinfo->last_dc_val[ci];

  /* Initialize static variables */
  cinfo = xinfo;
  huff_put_buffer = cinfo->huff_put_buffer;
  huff_put_bits = cinfo->huff_put_bits;
#endif	/* XIE_SUPPORTED */

  /* Account for restart interval, emit restart marker if needed */
  if (cinfo->restart_interval) {
    if (cinfo->restarts_to_go == 0)
#ifdef XIE_SUPPORTED
      if ((status = emit_restart(cinfo)) < 0) {
        if (status != XIE_ERR) {
          for (ci = 0; ci < cinfo->comps_in_scan; ci++)
            cinfo->last_dc_val[ci] = last_dc_val[ci];
          cinfo->huff_put_buffer = huff_put_buffer;
          cinfo->huff_put_bits = huff_put_bits;
        }
        return(status);  
      }
#else    
      emit_restart(cinfo);
#endif	/* XIE_SUPPORTED */
    cinfo->restarts_to_go--;
  }

  for (blkn = 0; blkn < cinfo->blocks_in_MCU; blkn++) {
    ci = cinfo->MCU_membership[blkn];
    compptr = cinfo->cur_comp_info[ci];
    /* Convert DC value to difference, update last_dc_val */
    temp = MCU_data[blkn][0];
    MCU_data[blkn][0] -= cinfo->last_dc_val[ci];
    cinfo->last_dc_val[ci] = temp;
#ifdef XIE_SUPPORTED
    if ((status = encode_one_block(MCU_data[blkn],
		     cinfo->dc_huff_tbl_ptrs[compptr->dc_tbl_no],
		     cinfo->ac_huff_tbl_ptrs[compptr->ac_tbl_no])) < 0) {
      /* Reset global variables for restart */
      if (status != XIE_ERR) {
        for (ci = 0; ci < cinfo->comps_in_scan; ci++)
          cinfo->last_dc_val[ci] = last_dc_val[ci];
        cinfo->huff_put_buffer = huff_put_buffer;
        cinfo->huff_put_bits = huff_put_bits;
      }
      return(status);  
    }    
#else    
    encode_one_block(MCU_data[blkn],
		     cinfo->dc_huff_tbl_ptrs[compptr->dc_tbl_no],
		     cinfo->ac_huff_tbl_ptrs[compptr->ac_tbl_no]);
#endif	/* XIE_SUPPORTED */
  }
#ifdef XIE_SUPPORTED
  return(0);
#endif  /* XIE_SUPPORTED */
}


/*
 * Finish up at the end of a Huffman-compressed scan.
 */

#ifdef XIE_SUPPORTED
METHODDEF int
#if NeedFunctionPrototypes
huff_term (compress_info_ptr xinfo)
#else
huff_term (xinfo)
	compress_info_ptr xinfo;
#endif	/* NeedFunctionPrototypes */
{
  int status;	
#else
METHODDEF void
huff_term (compress_info_ptr xinfo)
{
#endif	/* XIE_SUPPORTED */
#ifdef XIE_SUPPORTED
  /* Initialize static variables */
  cinfo = xinfo;
  /* Flush out the last data */
  if ((status = flush_bits()) < 0)
    return(status);
#else
  /* Flush out the last data */
  flush_bits();
#endif	/* XIE_SUPPORTED */
  flush_bytes();
  /* Release the I/O buffer */
#ifdef XIE_SUPPORTED
  return(0);
#else
  (*cinfo->emethods->free_small) ((pointer ) output_buffer);
#endif	/* XIE_SUPPORTED */
}


/*
 * Huffman coding optimization.
 *
 * This actually is optimization, in the sense that we find the best possible
 * Huffman table(s) for the given data.  We first scan the supplied data and
 * count the number of uses of each symbol that is to be Huffman-coded.
 * (This process must agree with the code above.)  Then we build an
 * optimal Huffman coding tree for the observed counts.
 */

#ifndef XIE_SUPPORTED
#ifdef ENTROPY_OPT_SUPPORTED


/* These are static so htest_one_block can find 'em */
static long * dc_count_ptrs[NUM_HUFF_TBLS];
static long * ac_count_ptrs[NUM_HUFF_TBLS];


LOCAL void
gen_huff_coding (compress_info_ptr cinfo, HUFF_TBL *htbl, long freq[])
/* Generate the optimal coding for the given counts */
{
#define MAX_CLEN 32		/* assumed maximum initial code length */
  UINT8 bits[MAX_CLEN+1];	/* bits[k] = # of symbols with code length k */
  short codesize[257];		/* codesize[k] = code length of symbol k */
  short others[257];		/* next symbol in current branch of tree */
  int c1, c2;
  int p, i, j;
  long v;

  /* This algorithm is explained in section K.2 of the JPEG standard */

  MEMZERO(bits, SIZEOF(bits));
  MEMZERO(codesize, SIZEOF(codesize));
  for (i = 0; i < 257; i++)
    others[i] = -1;		/* init links to empty */
  
  freq[256] = 1;		/* make sure there is a nonzero count */
  /* including the pseudo-symbol 256 in the Huffman procedure guarantees
   * that no real symbol is given code-value of all ones, because 256
   * will be placed in the largest codeword category.
   */

  /* Huffman's basic algorithm to assign optimal code lengths to symbols */

  for (;;) {
    /* Find the smallest nonzero frequency, set c1 = its symbol */
    /* In case of ties, take the larger symbol number */
    c1 = -1;
    v = 1000000000L;
    for (i = 0; i <= 256; i++) {
      if (freq[i] && freq[i] <= v) {
	v = freq[i];
	c1 = i;
      }
    }

    /* Find the next smallest nonzero frequency, set c2 = its symbol */
    /* In case of ties, take the larger symbol number */
    c2 = -1;
    v = 1000000000L;
    for (i = 0; i <= 256; i++) {
      if (freq[i] && freq[i] <= v && i != c1) {
	v = freq[i];
	c2 = i;
      }
    }

    /* Done if we've merged everything into one frequency */
    if (c2 < 0)
      break;
    
    /* Else merge the two counts/trees */
    freq[c1] += freq[c2];
    freq[c2] = 0;

    /* Increment the codesize of everything in c1's tree branch */
    codesize[c1]++;
    while (others[c1] >= 0) {
      c1 = others[c1];
      codesize[c1]++;
    }
    
    others[c1] = c2;		/* chain c2 onto c1's tree branch */
    
    /* Increment the codesize of everything in c2's tree branch */
    codesize[c2]++;
    while (others[c2] >= 0) {
      c2 = others[c2];
      codesize[c2]++;
    }
  }

  /* Now count the number of symbols of each code length */
  for (i = 0; i <= 256; i++) {
    if (codesize[i]) {
      /* The JPEG standard seems to think that this can't happen, */
      /* but I'm paranoid... */
      if (codesize[i] > MAX_CLEN)
	ERREXIT(cinfo->emethods, "Huffman code size table overflow");

      bits[codesize[i]]++;
    }
  }

  /* JPEG doesn't allow symbols with code lengths over 16 bits, so if the pure
   * Huffman procedure assigned any such lengths, we must adjust the coding.
   * Here is what the JPEG spec says about how this next bit works:
   * Since symbols are paired for the longest Huffman code, the symbols are
   * removed from this length category two at a time.  The prefix for the pair
   * (which is one bit shorter) is allocated to one of the pair; then,
   * skipping the BITS entry for that prefix length, a code word from the next
   * shortest nonzero BITS entry is converted into a prefix for two code words
   * one bit longer.
   */
  
  for (i = MAX_CLEN; i > 16; i--) {
    while (bits[i] > 0) {
      j = i - 2;		/* find length of new prefix to be used */
      while (bits[j] == 0)
	j--;
      
      bits[i] -= 2;		/* remove two symbols */
      bits[i-1]++;		/* one goes in this length */
      bits[j+1] += 2;		/* two new symbols in this length */
      bits[j]--;		/* symbol of this length is now a prefix */
    }
  }

  /* Remove the count for the pseudo-symbol 256 from the largest codelength */
  while (bits[i] == 0)		/* find largest codelength still in use */
    i--;
  bits[i]--;
  
  /* Return final symbol counts (only for lengths 0..16) */
  MEMCOPY(htbl->bits, bits, SIZEOF(htbl->bits));
  
  /* Return a list of the symbols sorted by code length */
  /* It's not real clear to me why we don't need to consider the codelength
   * changes made above, but the JPEG spec seems to think this works.
   */
  p = 0;
  for (i = 1; i <= MAX_CLEN; i++) {
    for (j = 0; j <= 255; j++) {
      if (codesize[j] == i) {
	htbl->huffval[p] = (UINT8) j;
	p++;
      }
    }
  }
}


/* Process a single block's worth of coefficients */
/* Note that the DC coefficient has already been converted to a difference */

LOCAL void
htest_one_block (JBLOCK block, JCOEF block0,
		 long dc_counts[], long ac_counts[])
{
  register INT32 temp;
  register int nbits;
  register int k, r;
  
  /* Encode the DC coefficient difference per section F.1.2.1 */
  
  /* Find the number of bits needed for the magnitude of the coefficient */
  temp = block0;
  if (temp < 0) temp = -temp;
  
  for (nbits = 0; temp; nbits++)
    temp >>= 1;
  
  /* Count the Huffman symbol for the number of bits */
  dc_counts[nbits]++;
  
  /* Encode the AC coefficients per section F.1.2.2 */
  
  r = 0;			/* r = run length of zeros */
  
  for (k = 1; k < DCTSIZE2; k++) {
    if ((temp = block[k]) == 0) {
      r++;
    } else {
      /* if run length > 15, must emit special run-length-16 codes (0xF0) */
      while (r > 15) {
	ac_counts[0xF0]++;
	r -= 16;
      }
      
      /* Find the number of bits needed for the magnitude of the coefficient */
      if (temp < 0) temp = -temp;
      
      for (nbits = 0; temp; nbits++)
	temp >>= 1;
      
      /* Count Huffman symbol for run length / number of bits */
      ac_counts[(r << 4) + nbits]++;
      
      r = 0;
    }
  }

  /* If the last coef(s) were zero, emit an end-of-block code */
  if (r > 0)
    ac_counts[0]++;
}



/*
 * Trial-encode one MCU's worth of Huffman-compressed coefficients.
 */

LOCAL void
htest_encode (compress_info_ptr cinfo, JBLOCK *MCU_data)
{
  short blkn, ci;
  jpeg_component_info * compptr;

  /* Take care of restart intervals if needed */
  if (cinfo->restart_interval) {
    if (cinfo->restarts_to_go == 0) {
      /* Re-initialize DC predictions to 0 */
      for (ci = 0; ci < cinfo->comps_in_scan; ci++)
	cinfo->last_dc_val[ci] = 0;
      /* Update restart state */
      cinfo->restarts_to_go = cinfo->restart_interval;
    }
    cinfo->restarts_to_go--;
  }

  for (blkn = 0; blkn < cinfo->blocks_in_MCU; blkn++) {
    ci = cinfo->MCU_membership[blkn];
    compptr = cinfo->cur_comp_info[ci];
    /* NB: unlike the real entropy encoder, we may not change the input data */
    htest_one_block(MCU_data[blkn],
		    (JCOEF) (MCU_data[blkn][0] - cinfo->last_dc_val[ci]),
		    dc_count_ptrs[compptr->dc_tbl_no],
		    ac_count_ptrs[compptr->ac_tbl_no]);
    cinfo->last_dc_val[ci] = MCU_data[blkn][0];
  }
}



/*
 * Find the best coding parameters for a Huffman-coded scan.
 * When called, the scan data has already been converted to a sequence of
 * MCU groups of quantized coefficients, which are stored in a "big" array.
 * The source_method knows how to iterate through that array.
 * On return, the MCU data is unmodified, but the Huffman tables referenced
 * by the scan components may have been altered.
 */

METHODDEF void
huff_optimize (compress_info_ptr xinfo, MCU_output_caller_ptr source_method)
/* Optimize Huffman-coding parameters (Huffman symbol table) */
{
  int i, tbl;
  HUFF_TBL **htblptr;

  /* Allocate and zero the count tables */
  /* Note that gen_huff_coding expects 257 entries in each table! */

  for (i = 0; i < NUM_HUFF_TBLS; i++) {
    dc_count_ptrs[i] = NULL;
    ac_count_ptrs[i] = NULL;
  }

  for (i = 0; i < cinfo->comps_in_scan; i++) {
    /* Create DC table */
    tbl = cinfo->cur_comp_info[i]->dc_tbl_no;
    if (dc_count_ptrs[tbl] == NULL) {
      dc_count_ptrs[tbl] = (long *) (*cinfo->emethods->alloc_small)
					(257 * SIZEOF(long));
      MEMZERO(dc_count_ptrs[tbl], 257 * SIZEOF(long));
    }
    /* Create AC table */
    tbl = cinfo->cur_comp_info[i]->ac_tbl_no;
    if (ac_count_ptrs[tbl] == NULL) {
      ac_count_ptrs[tbl] = (long *) (*cinfo->emethods->alloc_small)
					(257 * SIZEOF(long));
      MEMZERO(ac_count_ptrs[tbl], 257 * SIZEOF(long));
    }
  }

  /* Initialize DC predictions to 0 */
  for (i = 0; i < cinfo->comps_in_scan; i++) {
    cinfo->last_dc_val[i] = 0;
  }
  /* Initialize restart stuff */
  cinfo->restarts_to_go = cinfo->restart_interval;

  /* Scan the MCU data, count symbol uses */
  (*source_method) (cinfo, htest_encode); 

  /* Now generate optimal Huffman tables */
  for (tbl = 0; tbl < NUM_HUFF_TBLS; tbl++) {
    if (dc_count_ptrs[tbl] != NULL) {
      htblptr = & cinfo->dc_huff_tbl_ptrs[tbl];
      if (*htblptr == NULL)
	*htblptr = (HUFF_TBL *) (*cinfo->emethods->alloc_small) (SIZEOF(HUFF_TBL));
      /* Set sent_table FALSE so updated table will be written to JPEG file. */
      (*htblptr)->sent_table = FALSE;
      /* Compute the optimal Huffman encoding */
      gen_huff_coding(cinfo, *htblptr, dc_count_ptrs[tbl]);
      /* Release the count table */
      (*cinfo->emethods->free_small) ((pointer ) dc_count_ptrs[tbl]);
    }
    if (ac_count_ptrs[tbl] != NULL) {
      htblptr = & cinfo->ac_huff_tbl_ptrs[tbl];
      if (*htblptr == NULL)
	*htblptr = (HUFF_TBL *) (*cinfo->emethods->alloc_small) (SIZEOF(HUFF_TBL));
      /* Set sent_table FALSE so updated table will be written to JPEG file. */
      (*htblptr)->sent_table = FALSE;
      /* Compute the optimal Huffman encoding */
      gen_huff_coding(cinfo, *htblptr, ac_count_ptrs[tbl]);
      /* Release the count table */
      (*cinfo->emethods->free_small) ((pointer ) ac_count_ptrs[tbl]);
    }
  }
}


#endif /* ENTROPY_OPT_SUPPORTED */
#endif	/* XIE_SUPPORTED */


/*
 * The method selection routine for Huffman entropy encoding.
 */

GLOBAL void
#ifdef XIE_SUPPORTED
#if NeedFunctionPrototypes
jselchuffman (compress_info_ptr cinfo)
#else
jselchuffman (cinfo)
	compress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
#else
jselchuffman (compress_info_ptr cinfo)
#endif	/* XIE_SUPPORTED */
{
  if (! cinfo->arith_code) {
    cinfo->methods->entropy_encode_init = huff_init;
    cinfo->methods->entropy_encode = huff_encode;
    cinfo->methods->entropy_encode_term = huff_term;
#ifndef XIE_SUPPORTED		    
#ifdef ENTROPY_OPT_SUPPORTED
    cinfo->methods->entropy_optimize = huff_optimize;
    /* The standard Huffman tables are only valid for 8-bit data precision.
     * If the precision is higher, force optimization on so that usable
     * tables will be computed.  This test can be removed if default tables
     * are supplied that are valid for the desired precision.
     */
    if (cinfo->data_precision > 8)
      cinfo->optimize_coding = TRUE;
    if (cinfo->optimize_coding)
      cinfo->total_passes++;	/* one pass needed for entropy optimization */
#endif
#endif	/* XIE_SUPPORTED */
  }
}
