/* $Xorg: jcmcu.c,v 1.4 2001/02/09 02:04:28 xorgcvs Exp $ */
/* Module jcmcu.c */

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
 * jcmcu.c
 *
 * Copyright (C) 1991, 1992, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains MCU extraction routines and quantization scaling.
 * These routines are invoked via the extract_MCUs and
 * extract_init/term methods.
 */

#include "jinclude.h"


/*
 * If this file is compiled with -DDCT_ERR_STATS, it will reverse-DCT each
 * block and sum the total errors across the whole picture.  This provides
 * a convenient method of using real picture data to test the roundoff error
 * of a DCT algorithm.  DCT_ERR_STATS should *not* be defined for a production
 * compression program, since compression is much slower with it defined.
 * Also note that jrevdct.o must be linked into the compressor when this
 * switch is defined.
 */

#ifndef XIE_SUPPORTED
#ifdef DCT_ERR_STATS
static int dcterrorsum;		/* these hold the error statistics */
static int dcterrormax;
static int dctcoefcount;	/* This will probably overflow on a 16-bit-int machine */
#endif
#endif	/* XIE_SUPPORTED */


/* ZAG[i] is the natural-order position of the i'th element of zigzag order. */

static const short ZAG[DCTSIZE2] = {
  0,  1,  8, 16,  9,  2,  3, 10,
 17, 24, 32, 25, 18, 11,  4,  5,
 12, 19, 26, 33, 40, 48, 41, 34,
 27, 20, 13,  6,  7, 14, 21, 28,
 35, 42, 49, 56, 57, 50, 43, 36,
 29, 22, 15, 23, 30, 37, 44, 51,
 58, 59, 52, 45, 38, 31, 39, 46,
 53, 60, 61, 54, 47, 55, 62, 63
};


LOCAL void
#ifdef XIE_SUPPORTED
#if NeedFunctionPrototypes
extract_block (JSAMPARRAY input_data, int start_row, long start_col,
	       JBLOCK output_data, QUANT_TBL_PTR quanttbl)
#else
extract_block (input_data, start_row, start_col, output_data, quanttbl)
	JSAMPARRAY input_data;
	int start_row;
	long start_col;
	JBLOCK output_data;
	QUANT_TBL_PTR quanttbl;
#endif	/* NeedFunctionPrototypes */
#else
extract_block (JSAMPARRAY input_data, int start_row, long start_col,
	       JBLOCK output_data, QUANT_TBL_PTR quanttbl)
#endif	/* XIE_SUPPORTED */
/* Extract one 8x8 block from the specified location in the sample array; */
/* perform forward DCT, quantization scaling, and zigzag reordering on it. */
{
  /* This routine is heavily used, so it's worth coding it tightly. */
  DCTBLOCK block;
#ifdef DCT_ERR_STATS
  DCTBLOCK svblock;		/* saves input data for comparison */
#endif

  { register JSAMPROW elemptr;
    register DCTELEM *localblkptr = block;
#if DCTSIZE != 8
    register int elemc;
#endif
    register int elemr;

    for (elemr = DCTSIZE; elemr > 0; elemr--) {
      elemptr = input_data[start_row++] + start_col;
#if DCTSIZE == 8		/* unroll the inner loop */
      *localblkptr++ = (DCTELEM) (GETJSAMPLE(*elemptr++) - CENTERJSAMPLE);
      *localblkptr++ = (DCTELEM) (GETJSAMPLE(*elemptr++) - CENTERJSAMPLE);
      *localblkptr++ = (DCTELEM) (GETJSAMPLE(*elemptr++) - CENTERJSAMPLE);
      *localblkptr++ = (DCTELEM) (GETJSAMPLE(*elemptr++) - CENTERJSAMPLE);
      *localblkptr++ = (DCTELEM) (GETJSAMPLE(*elemptr++) - CENTERJSAMPLE);
      *localblkptr++ = (DCTELEM) (GETJSAMPLE(*elemptr++) - CENTERJSAMPLE);
      *localblkptr++ = (DCTELEM) (GETJSAMPLE(*elemptr++) - CENTERJSAMPLE);
      *localblkptr++ = (DCTELEM) (GETJSAMPLE(*elemptr++) - CENTERJSAMPLE);
#else
      for (elemc = DCTSIZE; elemc > 0; elemc--) {
	*localblkptr++ = (DCTELEM) (GETJSAMPLE(*elemptr++) - CENTERJSAMPLE);
      }
#endif
    }
  }

#ifdef DCT_ERR_STATS
  MEMCOPY(svblock, block, SIZEOF(DCTBLOCK));
#endif

  j_fwd_dct(block);

  { register JCOEF temp;
    register short i;

    for (i = 0; i < DCTSIZE2; i++) {
      temp = (JCOEF) block[ZAG[i]];
      /* divide by *quanttbl, ensuring proper rounding */
      if (temp < 0) {
	temp = -temp;
	temp += *quanttbl>>1;
	temp /= *quanttbl;
	temp = -temp;
      } else {
	temp += *quanttbl>>1;
	temp /= *quanttbl;
      }
      *output_data++ = temp;
      quanttbl++;
    }
  }

#ifdef DCT_ERR_STATS
  j_rev_dct(block);

  { register int diff;
    register short i;

    for (i = 0; i < DCTSIZE2; i++) {
      diff = block[i] - svblock[i];
      if (diff < 0) diff = -diff;
      dcterrorsum += diff;
      if (dcterrormax < diff) dcterrormax = diff;
    }
    dctcoefcount += DCTSIZE2;
  }
#endif
}


/*
 * Extract samples in MCU order, process & hand off to output_method.
 * The input is always exactly N MCU rows worth of data.
 */

#ifdef XIE_SUPPORTED
METHODDEF int
#if NeedFunctionPrototypes
extract_MCUs (compress_info_ptr cinfo,
	      JSAMPIMAGE image_data,
	      int num_mcu_rows,
	      MCU_output_method_ptr output_method)
#else
extract_MCUs (cinfo, image_data, num_mcu_rows, output_method)
	compress_info_ptr cinfo;
	JSAMPIMAGE image_data;
	int num_mcu_rows;
	MCU_output_method_ptr output_method;
#endif	/* NeedFunctionPrototypes */
{
  JBLOCK MCU_data[MAX_BLOCKS_IN_MCU];
  int mcurow;
  long mcuindex;
  int startrow;
  long startindex;
  short blkn, ci, xpos, ypos;
  jpeg_component_info * compptr;
  QUANT_TBL_PTR quant_ptr;

  if (cinfo->XIErestart == XIE_RMCU) {
    startrow = cinfo->XIEmcurow;
    startindex = cinfo->XIEmcuindex;
    cinfo->XIErestart = XIE_RNUL;
  } else {
    startrow = 0;
    startindex = 0;
  }
  for (mcurow = startrow; mcurow < num_mcu_rows; mcurow++) {
    for (mcuindex = startindex; mcuindex < cinfo->MCUs_per_row; mcuindex++) {
      /* Extract data from the image array, DCT it, and quantize it */
      blkn = 0;
      for (ci = 0; ci < cinfo->comps_in_scan; ci++) {
		compptr = cinfo->cur_comp_info[ci];
		quant_ptr = cinfo->quant_tbl_ptrs[compptr->quant_tbl_no];
		for (ypos = 0; ypos < compptr->MCU_height; ypos++) {
		  for (xpos = 0; xpos < compptr->MCU_width; xpos++) {
		    extract_block(image_data[ci],
			  (mcurow * compptr->MCU_height + ypos)*DCTSIZE,
			  (mcuindex * compptr->MCU_width + xpos)*DCTSIZE,
			  MCU_data[blkn], quant_ptr);
		    blkn++;
		  }
		}
      }
      /* Send the MCU whereever the pipeline controller wants it to go */
      cinfo->XIEbytes_in_buffer = cinfo->bytes_in_buffer;
      if ((*output_method) (cinfo, MCU_data) < 0) {
        cinfo->XIErestart = XIE_RMCU;
        cinfo->XIEmcurow = mcurow;
        cinfo->XIEmcuindex = mcuindex;
        cinfo->bytes_in_buffer = cinfo->XIEbytes_in_buffer;
        return(-1);
      }
    }
  }
  return(0);
}
#else
METHODDEF void
extract_MCUs (compress_info_ptr cinfo,
	      JSAMPIMAGE image_data,
	      int num_mcu_rows,
	      MCU_output_method_ptr output_method)
{
  JBLOCK MCU_data[MAX_BLOCKS_IN_MCU];
  int mcurow;
  long mcuindex;
  short blkn, ci, xpos, ypos;
  jpeg_component_info * compptr;
  QUANT_TBL_PTR quant_ptr;

  for (mcurow = 0; mcurow < num_mcu_rows; mcurow++) {
    for (mcuindex = 0; mcuindex < cinfo->MCUs_per_row; mcuindex++) {
      /* Extract data from the image array, DCT it, and quantize it */
      blkn = 0;
      for (ci = 0; ci < cinfo->comps_in_scan; ci++) {
	compptr = cinfo->cur_comp_info[ci];
	quant_ptr = cinfo->quant_tbl_ptrs[compptr->quant_tbl_no];
	for (ypos = 0; ypos < compptr->MCU_height; ypos++) {
	  for (xpos = 0; xpos < compptr->MCU_width; xpos++) {
	    extract_block(image_data[ci],
			  (mcurow * compptr->MCU_height + ypos)*DCTSIZE,
			  (mcuindex * compptr->MCU_width + xpos)*DCTSIZE,
			  MCU_data[blkn], quant_ptr);
	    blkn++;
	  }
	}
      }
      /* Send the MCU whereever the pipeline controller wants it to go */
      (*output_method) (cinfo, MCU_data);
    }
  }
}
#endif


/*
 * Initialize for processing a scan.
 */

METHODDEF void
#ifdef XIE_SUPPORTED
#if NeedFunctionPrototypes
extract_init (compress_info_ptr cinfo)
#else
extract_init (cinfo)
	compress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
#else
extract_init (compress_info_ptr cinfo)
#endif	/* XIE_SUPPORTED */
{
  /* no work for now */
#ifdef DCT_ERR_STATS
  dcterrorsum = dcterrormax = dctcoefcount = 0;
#endif
}


/*
 * Clean up after a scan.
 */

METHODDEF void
#ifdef XIE_SUPPORTED
#if NeedFunctionPrototypes
extract_term (compress_info_ptr cinfo)
#else
extract_term (cinfo)
	compress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
#else
extract_term (compress_info_ptr cinfo)
#endif	/* XIE_SUPPORTED */
{
  /* no work for now */
#ifndef XIE_SUPPORTED
#ifdef DCT_ERR_STATS
  TRACEMS3(cinfo->emethods, 0, "DCT roundoff errors = %d/%d,  max = %d",
	   dcterrorsum, dctcoefcount, dcterrormax);
#endif
#endif	/* XIE_SUPPORTED */
}



/*
 * The method selection routine for MCU extraction.
 */

GLOBAL void
#ifdef XIE_SUPPORTED
#if NeedFunctionPrototypes
jselcmcu (compress_info_ptr cinfo)
#else
jselcmcu (cinfo)
	compress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
#else
jselcmcu (compress_info_ptr cinfo)
#endif	/* XIE_SUPPORTED */
{
  /* just one implementation for now */
  cinfo->methods->extract_init = extract_init;
  cinfo->methods->extract_MCUs = extract_MCUs;
  cinfo->methods->extract_term = extract_term;
}
