/* $Xorg: jdmcu.c,v 1.4 2001/02/09 02:04:28 xorgcvs Exp $ */
/* Module jdmcu.c */

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

	Gary Rogers, AGE Logic, Inc., January 1994

****************************************************************************/

/*
 * jdmcu.c
 *
 * Copyright (C) 1991, 1992, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains MCU disassembly and IDCT control routines.
 * These routines are invoked via the disassemble_MCU, reverse_DCT, and
 * disassemble_init/term methods.
 */

#include "jinclude.h"


/*
 * Fetch one MCU row from entropy_decode, build coefficient array.
 * This version is used for noninterleaved (single-component) scans.
 */

#ifdef XIE_SUPPORTED
METHODDEF int
#if NeedFunctionPrototypes
disassemble_noninterleaved_MCU (decompress_info_ptr cinfo,
				JBLOCKIMAGE image_data)
#else
disassemble_noninterleaved_MCU (cinfo, image_data)
				decompress_info_ptr cinfo;
				JBLOCKIMAGE image_data;
#endif	/* NeedFunctionPrototypes */
{
  long mcuindex;
  long start;

  /* this is pretty easy since there is one component and one block per MCU */

  if (cinfo->XIErestart == XIE_RMCU) {
    start = cinfo->XIEmcuindex;
    cinfo->XIErestart = XIE_RNUL;
  } else {
    start = 0;
    /* Pre-zero the target area to speed up entropy decoder */
    /* (we assume wholesale zeroing is faster than retail) */
    jzero_far((pointer) image_data[0][0],
	    (size_t) (cinfo->MCUs_per_row * SIZEOF(JBLOCK)));
  }
  for (mcuindex = start; mcuindex < cinfo->MCUs_per_row; mcuindex++) {
    /* Point to the proper spot in the image array for this MCU */
    cinfo->MCU_data[0] = image_data[0][0] + mcuindex;
    /* Fetch the coefficient data */
    /* Save the current input buffer position (for restart) */
    cinfo->XIEnext_input_byte = cinfo->next_input_byte;
    cinfo->XIEbytes_in_buffer = cinfo->bytes_in_buffer;
    if (((*cinfo->methods->entropy_decode) (cinfo, cinfo->MCU_data)) < 0) {
      cinfo->XIErestart = XIE_RMCU;
      cinfo->XIEmcuindex = mcuindex;
      cinfo->next_input_byte = cinfo->XIEnext_input_byte;
      cinfo->bytes_in_buffer = cinfo->XIEbytes_in_buffer;
      return(-1);
    }
  }
  return(0);
}
#else
METHODDEF void
disassemble_noninterleaved_MCU (decompress_info_ptr cinfo,
				JBLOCKIMAGE image_data)
{
  JBLOCKROW MCU_data[1];
  long mcuindex;

  /* this is pretty easy since there is one component and one block per MCU */

  /* Pre-zero the target area to speed up entropy decoder */
  /* (we assume wholesale zeroing is faster than retail) */
  jzero_far((pointer) image_data[0][0],
	    (size_t) (cinfo->MCUs_per_row * SIZEOF(JBLOCK)));

  for (mcuindex = 0; mcuindex < cinfo->MCUs_per_row; mcuindex++) {
    /* Point to the proper spot in the image array for this MCU */
    MCU_data[0] = image_data[0][0] + mcuindex;
    /* Fetch the coefficient data */
    (*cinfo->methods->entropy_decode) (cinfo, MCU_data);
  }
}
#endif	/* XIE_SUPPORTED */


/*
 * Fetch one MCU row from entropy_decode, build coefficient array.
 * This version is used for interleaved (multi-component) scans.
 */

#ifdef XIE_SUPPORTED
METHODDEF int
#if NeedFunctionPrototypes
disassemble_interleaved_MCU (decompress_info_ptr cinfo,
			     JBLOCKIMAGE image_data)
#else
disassemble_interleaved_MCU (cinfo,	image_data)
				 decompress_info_ptr cinfo;
			     JBLOCKIMAGE image_data;
#endif	/* NeedFunctionPrototypes */
{
  long mcuindex;
  long start;
  short blkn, ci, xpos, ypos;
  jpeg_component_info * compptr;
  JBLOCKROW image_ptr;

  /* Pre-zero the target area to speed up entropy decoder */
  /* (we assume wholesale zeroing is faster than retail) */
  if (cinfo->XIErestart == XIE_RMCU) {
    start = cinfo->XIEmcuindex;
    cinfo->XIErestart = XIE_RNUL;
  } else {
    start = 0;
    for (ci = 0; ci < cinfo->comps_in_scan; ci++) {
      compptr = cinfo->cur_comp_info[ci];
      for (ypos = 0; ypos < compptr->MCU_height; ypos++) {
        jzero_far((pointer) image_data[ci][ypos],
	    (size_t) (cinfo->MCUs_per_row * compptr->MCU_width * SIZEOF(JBLOCK)));
      }
    }
  }

  for (mcuindex = start; mcuindex < cinfo->MCUs_per_row; mcuindex++) {
    /* Point to the proper spots in the image array for this MCU */
    blkn = 0;
    for (ci = 0; ci < cinfo->comps_in_scan; ci++) {
      compptr = cinfo->cur_comp_info[ci];
      for (ypos = 0; ypos < compptr->MCU_height; ypos++) {
        image_ptr = image_data[ci][ypos] + (mcuindex * compptr->MCU_width);
        for (xpos = 0; xpos < compptr->MCU_width; xpos++) {
          cinfo->MCU_data[blkn] = image_ptr;
          image_ptr++;
          blkn++;
        }
      }
    }
    /* Fetch the coefficient data */
    /* Save the current input buffer position (for restart) */
    cinfo->XIEnext_input_byte = cinfo->next_input_byte;
    cinfo->XIEbytes_in_buffer = cinfo->bytes_in_buffer;
    if (((*cinfo->methods->entropy_decode) (cinfo, cinfo->MCU_data)) < 0) {
      cinfo->XIErestart = XIE_RMCU;
      cinfo->XIEmcuindex = mcuindex;
      cinfo->next_input_byte = cinfo->XIEnext_input_byte;
      cinfo->bytes_in_buffer = cinfo->XIEbytes_in_buffer;
      return(-1);
    }
  }
  return(0);
}
#else
METHODDEF void
disassemble_interleaved_MCU (decompress_info_ptr cinfo,
			     JBLOCKIMAGE image_data)
{
  JBLOCKROW MCU_data[MAX_BLOCKS_IN_MCU];
  long mcuindex;
  short blkn, ci, xpos, ypos;
  jpeg_component_info * compptr;
  JBLOCKROW image_ptr;

  /* Pre-zero the target area to speed up entropy decoder */
  /* (we assume wholesale zeroing is faster than retail) */
  for (ci = 0; ci < cinfo->comps_in_scan; ci++) {
    compptr = cinfo->cur_comp_info[ci];
    for (ypos = 0; ypos < compptr->MCU_height; ypos++) {
      jzero_far((pointer) image_data[ci][ypos],
		(size_t) (cinfo->MCUs_per_row * compptr->MCU_width * SIZEOF(JBLOCK)));
    }
  }

  for (mcuindex = 0; mcuindex < cinfo->MCUs_per_row; mcuindex++) {
    /* Point to the proper spots in the image array for this MCU */
    blkn = 0;
    for (ci = 0; ci < cinfo->comps_in_scan; ci++) {
      compptr = cinfo->cur_comp_info[ci];
      for (ypos = 0; ypos < compptr->MCU_height; ypos++) {
        image_ptr = image_data[ci][ypos] + (mcuindex * compptr->MCU_width);
        for (xpos = 0; xpos < compptr->MCU_width; xpos++) {
	      MCU_data[blkn] = image_ptr;
          image_ptr++;
          blkn++;
        }
      }
    }
    /* Fetch the coefficient data */
    (*cinfo->methods->entropy_decode) (cinfo, MCU_data);
  }
}
#endif	/* XIE_SUPPORTED */    

/*
 * Perform inverse DCT on each block in an MCU row's worth of data;
 * output the results into a sample array starting at row start_row.
 * NB: start_row can only be nonzero when dealing with a single-component
 * scan; otherwise we'd have to pass different offsets for different
 * components, since the heights of interleaved MCU rows can vary.
 * But the pipeline controller logic is such that this is not necessary.
 */

METHODDEF void
#ifdef XIE_SUPPORTED
#if NeedFunctionPrototypes
reverse_DCT (decompress_info_ptr cinfo,
	     JBLOCKIMAGE coeff_data, JSAMPIMAGE output_data, int start_row)
#else
reverse_DCT (cinfo,	coeff_data, output_data, start_row)
		 decompress_info_ptr cinfo;
	     JBLOCKIMAGE coeff_data; 
	     JSAMPIMAGE output_data; 
	     int start_row;
#endif	/* NeedFunctionPrototypes */
#else
reverse_DCT (decompress_info_ptr cinfo,
	     JBLOCKIMAGE coeff_data, JSAMPIMAGE output_data, int start_row)
#endif	/* XIE_SUPPORTED */
{
  DCTBLOCK block;
  JBLOCKROW browptr;
  JSAMPARRAY srowptr;
  long blocksperrow, bi;
  short numrows, ri;
  short ci;

  for (ci = 0; ci < cinfo->comps_in_scan; ci++) {
    /* calculate size of an MCU row in this component */
    blocksperrow = cinfo->cur_comp_info[ci]->downsampled_width / DCTSIZE;
    numrows = cinfo->cur_comp_info[ci]->MCU_height;
    /* iterate through all blocks in MCU row */
    for (ri = 0; ri < numrows; ri++) {
      browptr = coeff_data[ci][ri];
      srowptr = output_data[ci] + (ri * DCTSIZE + start_row);
      for (bi = 0; bi < blocksperrow; bi++) {
	/* copy the data into a local DCTBLOCK.  This allows for change of
	 * representation (if DCTELEM != JCOEF).  On 80x86 machines it also
	 * brings the data back from FAR storage to NEAR storage.
	 */
	{ register JCOEFPTR elemptr = browptr[bi];
	  register DCTELEM *localblkptr = block;
	  register int elem = DCTSIZE2;

	  while (--elem >= 0)
	    *localblkptr++ = (DCTELEM) *elemptr++;
	}

	j_rev_dct(block);	/* perform inverse DCT */

	/* Output the data into the sample array.
	 * Note change from signed to unsigned representation:
	 * DCT calculation works with values +-CENTERJSAMPLE,
	 * but sample arrays always hold 0..MAXJSAMPLE.
	 * We have to do range-limiting because of quantization errors in the
	 * DCT/IDCT phase.  We use the sample_range_limit[] table to do this
	 * quickly; the CENTERJSAMPLE offset is folded into table indexing.
	 */
	{ register JSAMPROW elemptr;
	  register DCTELEM *localblkptr = block;
	  register JSAMPLE *range_limit = cinfo->sample_range_limit +
						CENTERJSAMPLE;
#if DCTSIZE != 8
	  register int elemc;
#endif
	  register int elemr;

	  for (elemr = 0; elemr < DCTSIZE; elemr++) {
	    elemptr = srowptr[elemr] + (bi * DCTSIZE);
#if DCTSIZE == 8		/* unroll the inner loop */
	    *elemptr++ = range_limit[*localblkptr++];
	    *elemptr++ = range_limit[*localblkptr++];
	    *elemptr++ = range_limit[*localblkptr++];
	    *elemptr++ = range_limit[*localblkptr++];
	    *elemptr++ = range_limit[*localblkptr++];
	    *elemptr++ = range_limit[*localblkptr++];
	    *elemptr++ = range_limit[*localblkptr++];
	    *elemptr++ = range_limit[*localblkptr++];
#else
	    for (elemc = DCTSIZE; elemc > 0; elemc--) {
	      *elemptr++ = range_limit[*localblkptr++];
	    }
#endif
	  }
	}
      }
    }
  }
}


/*
 * Initialize for processing a scan.
 */

METHODDEF void
#ifdef XIE_SUPPORTED
#if NeedFunctionPrototypes
disassemble_init (decompress_info_ptr cinfo)
#else
disassemble_init (cinfo)
	decompress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
#else
disassemble_init (decompress_info_ptr cinfo)
#endif	/* XIE_SUPPORTED */
{
  /* no work for now */
}


/*
 * Clean up after a scan.
 */

#ifndef XIE_SUPPORTED
METHODDEF void
#ifdef XIE_SUPPORTED
#if NeedFunctionPrototypes
disassemble_term (decompress_info_ptr cinfo)
#else
disassemble_term (cinfo)
	decompress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
#else
disassemble_term (decompress_info_ptr cinfo)
#endif	/* XIE_SUPPORTED */
{
  /* no work for now */
}
#endif   /* XIE_SUPPORTED */


/*
 * The method selection routine for MCU disassembly.
 */

GLOBAL void
#ifdef XIE_SUPPORTED
#if NeedFunctionPrototypes
jseldmcu (decompress_info_ptr cinfo)
#else
jseldmcu (cinfo)
	decompress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
#else
jseldmcu (decompress_info_ptr cinfo)
#endif	/* XIE_SUPPORTED */
{
  if (cinfo->comps_in_scan == 1)
    cinfo->methods->disassemble_MCU = disassemble_noninterleaved_MCU;
  else
    cinfo->methods->disassemble_MCU = disassemble_interleaved_MCU;
  cinfo->methods->reverse_DCT = reverse_DCT;
  cinfo->methods->disassemble_init = disassemble_init;
#ifndef XIE_SUPPORTED	  
  cinfo->methods->disassemble_term = disassemble_term;
#endif   /* XIE_SUPPORTED */  
}
