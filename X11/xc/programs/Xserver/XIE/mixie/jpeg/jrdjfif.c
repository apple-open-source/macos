/* $Xorg: jrdjfif.c,v 1.4 2001/02/09 02:04:29 xorgcvs Exp $ */
/* AGE Logic - Oct 15 1995 - Larry Hare */
/* Module jrdjfif.c */

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

****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/jpeg/jrdjfif.c,v 1.5 2001/12/14 19:58:40 dawes Exp $ */

/*
 * jrdjfif.c
 *
 * Copyright (C) 1991, 1992, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains routines to decode standard JPEG file headers/markers.
 * This code will handle "raw JPEG" and JFIF-convention JPEG files.
 *
 * You can also use this module to decode a raw-JPEG or JFIF-standard data
 * stream that is embedded within a larger file.  To do that, you must
 * position the file to the JPEG SOI marker (0xFF/0xD8) that begins the
 * data sequence to be decoded.  If nothing better is possible, you can scan
 * the file until you see the SOI marker, then use JUNGETC to push it back.
 *
 * This module relies on the JGETC macro and the read_jpeg_data method (which
 * is provided by the user interface) to read from the JPEG data stream.
 * Therefore, this module is not dependent on any particular assumption about
 * the data source; it need not be a stdio stream at all.  (This fact does
 * NOT carry over to more complex JPEG file formats such as JPEG-in-TIFF;
 * those format control modules may well need to assume stdio input.)
 *
 * These routines are invoked via the methods read_file_header,
 * read_scan_header, read_jpeg_data, read_scan_trailer, and read_file_trailer.
 */

#include "jinclude.h"
#include "macro.h"

#ifdef JFIF_SUPPORTED

#ifdef XIE_SUPPORTED
#define M_EOB    -1
#define M_SOF0   0xc0
#define M_SOF1   0xc1
#define M_SOF2   0xc2
#define M_SOF3   0xc3
  
#define M_SOF5   0xc5
#define M_SOF6   0xc6
#define M_SOF7   0xc7
  
#define M_JPG    0xc8
#define M_SOF9   0xc9
#define M_SOF10  0xca
#define M_SOF11  0xcb
  
#define M_SOF13  0xcd
#define M_SOF14  0xce
#define M_SOF15  0xcf
  
#define M_DHT    0xc4
  
#define M_DAC    0xcc
  
#define M_RST0   0xd0
#define M_RST1   0xd1
#define M_RST2   0xd2
#define M_RST3   0xd3
#define M_RST4   0xd4
#define M_RST5   0xd5
#define M_RST6   0xd6
#define M_RST7   0xd7
  
#define M_SOI    0xd8
#define M_EOI    0xd9
#define M_SOS    0xda
#define M_DQT    0xdb
#define M_DNL    0xdc
#define M_DRI    0xdd
#define M_DHP    0xde
#define M_EXP    0xdf
  
#define M_APP0   0xe0
#define M_APP15  0xef
  
#define M_JPG0   0xf0
#define M_JPG13  0xfd
#define M_COM    0xfe
  
#define M_TEM    0x01
  
#define M_ERROR  0x100
#else  
typedef enum {			/* JPEG marker codes */
  M_SOF0  = 0xc0,
  M_SOF1  = 0xc1,
  M_SOF2  = 0xc2,
  M_SOF3  = 0xc3,
  
  M_SOF5  = 0xc5,
  M_SOF6  = 0xc6,
  M_SOF7  = 0xc7,
  
  M_JPG   = 0xc8,
  M_SOF9  = 0xc9,
  M_SOF10 = 0xca,
  M_SOF11 = 0xcb,
  
  M_SOF13 = 0xcd,
  M_SOF14 = 0xce,
  M_SOF15 = 0xcf,
  
  M_DHT   = 0xc4,
  
  M_DAC   = 0xcc,
  
  M_RST0  = 0xd0,
  M_RST1  = 0xd1,
  M_RST2  = 0xd2,
  M_RST3  = 0xd3,
  M_RST4  = 0xd4,
  M_RST5  = 0xd5,
  M_RST6  = 0xd6,
  M_RST7  = 0xd7,
  
  M_SOI   = 0xd8,
  M_EOI   = 0xd9,
  M_SOS   = 0xda,
  M_DQT   = 0xdb,
  M_DNL   = 0xdc,
  M_DRI   = 0xdd,
  M_DHP   = 0xde,
  M_EXP   = 0xdf,
  
  M_APP0  = 0xe0,
  M_APP15 = 0xef,
  
  M_JPG0  = 0xf0,
  M_JPG13 = 0xfd,
  M_COM   = 0xfe,
  
  M_TEM   = 0x01,
  
  M_ERROR = 0x100
} JPEG_MARKER;
#endif	/* XIE_SUPPORTED */


/*
 * Reload the input buffer after it's been emptied, and return the next byte.
 * This is exported for direct use by the entropy decoder.
 * See the JGETC macro for calling conditions.  Note in particular that
 * read_jpeg_data may NOT return EOF.  If no more data is available, it must
 * exit via ERREXIT, or perhaps synthesize fake data (such as an RST marker).
 * For error recovery purposes, synthesizing an EOI marker is probably best.
 *
 * For this header control module, read_jpeg_data is supplied by the
 * user interface.  However, header formats that require random access
 * to the input file would need to supply their own code.  This code is
 * left here to indicate what is required.
 */

#ifndef XIE_SUPPORTED
#if NOTDEF				/* not needed in this module */

METHODDEF int
read_jpeg_data (decompress_info_ptr cinfo)
{
  cinfo->next_input_byte = cinfo->input_buffer + MIN_UNGET;

  cinfo->bytes_in_buffer = (int) JFREAD(cinfo->input_file,
					cinfo->next_input_byte,
					JPEG_BUF_SIZE);
  
  if (cinfo->bytes_in_buffer <= 0) {
    WARNMS(cinfo->emethods, "Premature EOF in JPEG file");
    cinfo->next_input_byte[0] = (char) 0xFF;
    cinfo->next_input_byte[1] = (char) M_EOI;
    cinfo->bytes_in_buffer = 2;
  }

  return JGETC(cinfo);
}

#endif
#endif	/* XIE_SUPPORTED */


/*
 * Routines to parse JPEG markers & save away the useful info.
 */


LOCAL INT32
#ifdef XIE_SUPPORTED
#if NeedFunctionPrototypes
get_2bytes (decompress_info_ptr cinfo)
#else
get_2bytes (cinfo)
	decompress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
#else
get_2bytes (decompress_info_ptr cinfo)
#endif	/* XIE_SUPPORTED */
/* Get a 2-byte unsigned integer (e.g., a marker parameter length field) */
{
  INT32 a;
#ifdef XIE_SUPPORTED
  INT32 b;
  if ((a = JGETC(cinfo)) < 0)
    return(-1);
  if ((b = JGETC(cinfo)) < 0)
    return(-1);
  return (a << 8) + b;
#else
  a = JGETC(cinfo);
  return (a << 8) + JGETC(cinfo);
#endif	/* XIE_SUPPORTED */
}


#ifdef XIE_SUPPORTED
LOCAL int
#if NeedFunctionPrototypes
skip_variable (decompress_info_ptr cinfo, int code)
#else
skip_variable (cinfo, code)
	decompress_info_ptr cinfo;
	int code;
#endif	/* NeedFunctionPrototypes */
#else
LOCAL void
skip_variable (decompress_info_ptr cinfo, int code)
#endif	/* XIE_SUPPORTED */
/* Skip over an unknown or uninteresting variable-length marker */
{
  INT32 length;
  
  length = get_2bytes(cinfo);
#ifdef XIE_SUPPORTED
  if (length < 0)
  	return(-1);
#else  
  TRACEMS2(cinfo->emethods, 1,
	   "Skipping marker 0x%02x, length %u", code, (int) length);
#endif	/* XIE_SUPPORTED */
  
  for (length -= 2; length > 0; length--)
#ifdef XIE_SUPPORTED
    if (JGETC(cinfo) < 0)
      return(-1);
#else
    (void) JGETC(cinfo);
#endif	/* XIE_SUPPORTED */

#ifdef XIE_SUPPORTED
  return(0);
#endif	/* XIE_SUPPORTED */
}


#ifdef XIE_SUPPORTED
LOCAL int
#if NeedFunctionPrototypes
get_dht (decompress_info_ptr cinfo)
#else
get_dht (cinfo)
	decompress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
#else
LOCAL void
get_dht (decompress_info_ptr cinfo)
#endif	/* XIE_SUPPORTED */
/* Process a DHT marker */
{
  INT32 length;
  UINT8 bits[17];
  UINT8 huffval[256];
  int i, index, count;
  HUFF_TBL **htblptr;
#ifdef XIE_SUPPORTED
  int j;
#endif	/* XIE_SUPPORTED */
  
#ifdef XIE_SUPPORTED
  if ((length = get_2bytes(cinfo)) < 0)
  	return(-1);
  length -= 2;
#else
  length = get_2bytes(cinfo)-2;
#endif	/* XIE_SUPPORTED */
  
  
  while (length > 0) {
    index = JGETC(cinfo);
#ifdef XIE_SUPPORTED
    if (index < 0)
      return(-1);
#else
    TRACEMS1(cinfo->emethods, 1, "Define Huffman Table 0x%02x", index);
#endif	/* XIE_SUPPORTED */
      
    bits[0] = 0;
    count = 0;
    for (i = 1; i <= 16; i++) {
#ifdef XIE_SUPPORTED
      if ((j = JGETC(cinfo)) < 0)
        return(-1);
      bits[i] = (UINT8) j;
#else
      bits[i] = (UINT8) JGETC(cinfo);
#endif	/* XIE_SUPPORTED */
      count += bits[i];
    }

#ifndef XIE_SUPPORTED
    TRACEMS8(cinfo->emethods, 2, "        %3d %3d %3d %3d %3d %3d %3d %3d",
	     bits[1], bits[2], bits[3], bits[4],
	     bits[5], bits[6], bits[7], bits[8]);
    TRACEMS8(cinfo->emethods, 2, "        %3d %3d %3d %3d %3d %3d %3d %3d",
	     bits[9], bits[10], bits[11], bits[12],
	     bits[13], bits[14], bits[15], bits[16]);
#endif	/* XIE_SUPPORTED */

    if (count > 256)
#ifdef XIE_SUPPORTED
      return(XIE_ERR);
#else
      ERREXIT(cinfo->emethods, "Bogus DHT counts");
#endif	/* XIE_SUPPORTED */

    for (i = 0; i < count; i++) {
#ifdef XIE_SUPPORTED
      if ((j = JGETC(cinfo)) < 0)
        return(-1);
      huffval[i] = (UINT8) j;
#else
      huffval[i] = (UINT8) JGETC(cinfo);
#endif	/* XIE_SUPPORTED */
    }

    length -= 1 + 16 + count;

    if (index & 0x10) {		/* AC table definition */
      index -= 0x10;
      htblptr = &cinfo->ac_huff_tbl_ptrs[index];
    } else {			/* DC table definition */
      htblptr = &cinfo->dc_huff_tbl_ptrs[index];
    }

    if (index < 0 || index >= NUM_HUFF_TBLS)
#ifdef XIE_SUPPORTED
      return(XIE_ERR);
#else
      ERREXIT1(cinfo->emethods, "Bogus DHT index %d", index);
#endif	/* XIE_SUPPORTED */

#ifdef XIE_SUPPORTED
    if (*htblptr == NULL)
      *htblptr = (HUFF_TBL *) (*cinfo->emethods->d_alloc_small) 
      	(cinfo, SIZEOF(HUFF_TBL));
    if (*htblptr == (HUFF_TBL *) NULL)
      return(XIE_ERR);
#else      
    if (*htblptr == NULL)
      *htblptr = (HUFF_TBL *) (*cinfo->emethods->alloc_small) (SIZEOF(HUFF_TBL));
#endif	/* XIE_SUPPORTED */
  
    MEMCOPY((*htblptr)->bits, bits, SIZEOF((*htblptr)->bits));
    MEMCOPY((*htblptr)->huffval, huffval, SIZEOF((*htblptr)->huffval));
    }
#ifdef XIE_SUPPORTED
  return(0);
#endif	/* XIE_SUPPORTED */
}


#ifdef XIE_SUPPORTED
LOCAL int
#if NeedFunctionPrototypes
get_dac (decompress_info_ptr cinfo)
#else
get_dac (cinfo)
	decompress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
#else
LOCAL void
get_dac (decompress_info_ptr cinfo)
#endif	/* XIE_SUPPORTED */
/* Process a DAC marker */
{
  INT32 length;
  int index, val;

#ifdef XIE_SUPPORTED
  if ((length = get_2bytes(cinfo)) < 0)
  	return(-1);
  length -= 2;
#else
  length = get_2bytes(cinfo)-2;
#endif	/* XIE_SUPPORTED */
  
  while (length > 0) {
    index = JGETC(cinfo);
#ifdef XIE_SUPPORTED
    if (index < 0)
      return(-1);
#endif	/* XIE_SUPPORTED */
    val = JGETC(cinfo);
#ifdef XIE_SUPPORTED
    if (val < 0)
      return(-1);
#else
    TRACEMS2(cinfo->emethods, 1,
	     "Define Arithmetic Table 0x%02x: 0x%02x", index, val);
#endif	/* XIE_SUPPORTED */

    if (index < 0 || index >= (2*NUM_ARITH_TBLS))
#ifdef XIE_SUPPORTED
      return(XIE_ERR);
#else
      ERREXIT1(cinfo->emethods, "Bogus DAC index %d", index);
#endif	/* XIE_SUPPORTED */

    if (index >= NUM_ARITH_TBLS) { /* define AC table */
      cinfo->arith_ac_K[index-NUM_ARITH_TBLS] = (UINT8) val;
    } else {			/* define DC table */
      cinfo->arith_dc_L[index] = (UINT8) (val & 0x0F);
      cinfo->arith_dc_U[index] = (UINT8) (val >> 4);
      if (cinfo->arith_dc_L[index] > cinfo->arith_dc_U[index])
#ifdef XIE_SUPPORTED
      return(XIE_ERR);
#else
      ERREXIT1(cinfo->emethods, "Bogus DAC value 0x%x", val);
#endif	/* XIE_SUPPORTED */
    }

    length -= 2;
  }
#ifdef XIE_SUPPORTED
  return(0);
#endif	/* XIE_SUPPORTED */
}


#ifdef XIE_SUPPORTED
LOCAL int
#if NeedFunctionPrototypes
get_dqt (decompress_info_ptr cinfo)
#else
get_dqt (cinfo)
	decompress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
#else
LOCAL void
get_dqt (decompress_info_ptr cinfo)
#endif	/* XIE_SUPPORTED */
/* Process a DQT marker */
{
  INT32 length;
  int n, i, prec;
  UINT16 tmp;
  QUANT_TBL_PTR quant_ptr;
#ifdef XIE_SUPPORTED
  int j;
#endif	/* XIE_SUPPORTED */
  
#ifdef XIE_SUPPORTED
  if ((length = get_2bytes(cinfo)) < 0)
  	return(-1);
  length -= 2;
#else
  length = get_2bytes(cinfo)-2;
#endif	/* XIE_SUPPORTED */
  
  while (length > 0) {
    n = JGETC(cinfo);
#ifdef XIE_SUPPORTED
    if (n < 0)
      return(-1);
#endif	/* XIE_SUPPORTED */
    prec = n >> 4;
    n &= 0x0F;

#ifndef XIE_SUPPORTED
    TRACEMS2(cinfo->emethods, 1,
	     "Define Quantization Table %d  precision %d", n, prec);
#endif	/* XIE_SUPPORTED */

    if (n >= NUM_QUANT_TBLS)
#ifdef XIE_SUPPORTED
      return(XIE_ERR);
#else
      ERREXIT1(cinfo->emethods, "Bogus table number %d", n);
#endif	/* XIE_SUPPORTED */
      
#ifdef XIE_SUPPORTED
    if (cinfo->quant_tbl_ptrs[n] == NULL)
      cinfo->quant_tbl_ptrs[n] = (QUANT_TBL_PTR)
		(*cinfo->emethods->d_alloc_small) (cinfo, SIZEOF(QUANT_TBL));
    if (cinfo->quant_tbl_ptrs[n] == (QUANT_TBL_PTR) NULL)
      return(XIE_ERR);
#else      
    if (cinfo->quant_tbl_ptrs[n] == NULL)
      cinfo->quant_tbl_ptrs[n] = (QUANT_TBL_PTR)
	(*cinfo->emethods->alloc_small) (SIZEOF(QUANT_TBL));
#endif	/* XIE_SUPPORTED */
    quant_ptr = cinfo->quant_tbl_ptrs[n];

    for (i = 0; i < DCTSIZE2; i++) {
#ifdef XIE_SUPPORTED
      j = JGETC(cinfo);
      if (j < 0)
        return(-1);
      tmp = (UINT16) j;
#else
      tmp = JGETC(cinfo);
#endif	/* XIE_SUPPORTED */
      if (prec) {
#ifdef XIE_SUPPORTED
        if ((j = JGETC(cinfo)) < 0)
          return(-1);
        tmp = (tmp<<8) + j;
#else
        tmp = (tmp<<8) + JGETC(cinfo);
#endif	/* XIE_SUPPORTED */
      }
      quant_ptr[i] = tmp;
    }

#ifndef XIE_SUPPORTED
    for (i = 0; i < DCTSIZE2; i += 8) {
      TRACEMS8(cinfo->emethods, 2, "        %4u %4u %4u %4u %4u %4u %4u %4u",
	       quant_ptr[i  ], quant_ptr[i+1], quant_ptr[i+2], quant_ptr[i+3],
	       quant_ptr[i+4], quant_ptr[i+5], quant_ptr[i+6], quant_ptr[i+7]);
    }
#endif	/* XIE_SUPPORTED */

    length -= DCTSIZE2+1;
    if (prec) length -= DCTSIZE2;
  }
#ifdef XIE_SUPPORTED
  return(0);
#endif	/* XIE_SUPPORTED */
}


#ifdef XIE_SUPPORTED
LOCAL int
#if NeedFunctionPrototypes
get_dri (decompress_info_ptr cinfo)
#else
get_dri (cinfo)
	decompress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
#else
LOCAL void
get_dri (decompress_info_ptr cinfo)
#endif	/* XIE_SUPPORTED */
/* Process a DRI marker */
{
#ifdef XIE_SUPPORTED
  INT32 j;
#endif	/* XIE_SUPPORTED */
  if (get_2bytes(cinfo) != 4)
#ifdef XIE_SUPPORTED
    return(XIE_ERR);
#else
    ERREXIT(cinfo->emethods, "Bogus length in DRI");
#endif	/* XIE_SUPPORTED */

#ifdef XIE_SUPPORTED
  if ((j = get_2bytes(cinfo)) < 0)
    return(-1);
  cinfo->restart_interval = (UINT16) j;
  
  return(0);
#else
  cinfo->restart_interval = (UINT16) get_2bytes(cinfo);

  TRACEMS1(cinfo->emethods, 1,
	   "Define Restart Interval %u", cinfo->restart_interval);
#endif	/* XIE_SUPPORTED */
}


#ifdef XIE_SUPPORTED
LOCAL int
#if NeedFunctionPrototypes
get_app0 (decompress_info_ptr cinfo)
#else
get_app0 (cinfo)
	decompress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
#else
LOCAL void
get_app0 (decompress_info_ptr cinfo)
#endif	/* XIE_SUPPORTED */
/* Process an APP0 marker */
{
#define JFIF_LEN 14
  INT32 length;
  UINT8 b[JFIF_LEN];
  int buffp;
#ifdef XIE_SUPPORTED
  int j;
#endif	/* XIE_SUPPORTED */

#ifdef XIE_SUPPORTED
  if ((length = get_2bytes(cinfo)) < 0)
  	return(-1);
  length -= 2;
#else
  length = get_2bytes(cinfo)-2;
#endif	/* XIE_SUPPORTED */

  /* See if a JFIF APP0 marker is present */

  if (length >= JFIF_LEN) {
    for (buffp = 0; buffp < JFIF_LEN; buffp++) {
#ifdef XIE_SUPPORTED
      if ((j = JGETC(cinfo)) < 0)
        return(-1);
      b[buffp] = (UINT8) j;
#else
      b[buffp] = (UINT8) JGETC(cinfo);
#endif	/* XIE_SUPPORTED */
    }
    length -= JFIF_LEN;

    if (b[0]==0x4A && b[1]==0x46 && b[2]==0x49 && b[3]==0x46 && b[4]==0) {
      /* Found JFIF APP0 marker: check version */
      /* Major version must be 1 */
      if (b[5] != 1)
#ifdef XIE_SUPPORTED
        return(XIE_ERR);
#else
        ERREXIT2(cinfo->emethods, "Unsupported JFIF revision number %d.%02d",
		 b[5], b[6]);
      /* Minor version should be 0..2, but try to process anyway if newer */
      if (b[6] > 2)
	    TRACEMS2(cinfo->emethods, 1, "Warning: unknown JFIF revision number %d.%02d",
		 b[5], b[6]);
#endif	/* XIE_SUPPORTED */
      /* Save info */
      cinfo->density_unit = b[7];
      cinfo->X_density = (b[8] << 8) + b[9];
      cinfo->Y_density = (b[10] << 8) + b[11];
      /* Assume colorspace is YCbCr, unless UI has overridden me */
      if (cinfo->jpeg_color_space == CS_UNKNOWN)
    	cinfo->jpeg_color_space = CS_YCbCr;
#ifndef XIE_SUPPORTED
      TRACEMS3(cinfo->emethods, 1, "JFIF APP0 marker, density %dx%d  %d",
	       cinfo->X_density, cinfo->Y_density, cinfo->density_unit);
      if (b[12] | b[13])
	    TRACEMS2(cinfo->emethods, 1, "    with %d x %d thumbnail image",
		 b[12], b[13]);
      if (length != ((INT32) b[12] * (INT32) b[13] * (INT32) 3))
	    TRACEMS1(cinfo->emethods, 1,
		 "Warning: thumbnail image size does not match data length %u",
		 (int) length);
#endif	/* XIE_SUPPORTED */
    } else {
#ifndef XIE_SUPPORTED
      TRACEMS1(cinfo->emethods, 1, "Unknown APP0 marker (not JFIF), length %u",
	       (int) length + JFIF_LEN);
#endif	/* XIE_SUPPORTED */
    }
  } else {
#ifndef XIE_SUPPORTED
    TRACEMS1(cinfo->emethods, 1, "Short APP0 marker, length %u", (int) length);
#endif	/* XIE_SUPPORTED */
  }

  while (length-- > 0)		/* skip any remaining data */
#ifdef XIE_SUPPORTED
    if (JGETC(cinfo) < 0)
      return(-1);
#else
    (void) JGETC(cinfo);
#endif	/* XIE_SUPPORTED */

#ifdef XIE_SUPPORTED
  return(0);
#endif	/* XIE_SUPPORTED */
}

#ifdef XIE_SUPPORTED
LOCAL int
#if NeedFunctionPrototypes
get_com (decompress_info_ptr cinfo)
#else
get_com (cinfo)
	decompress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
#else
LOCAL void
get_com (decompress_info_ptr cinfo)
#endif	/* XIE_SUPPORTED */
/* Process a COM marker */
/* Actually we just pass this off to an application-supplied routine */
{
  INT32 length;
  
#ifdef XIE_SUPPORTED
  if ((length = get_2bytes(cinfo)) < 0)
  	return(-1);
  length -= 2;
#else
  length = get_2bytes(cinfo) - 2;
  
  TRACEMS1(cinfo->emethods, 1, "Comment, length %u", (int) length);
#endif	/* XIE_SUPPORTED */
  
  (*cinfo->methods->process_comment) (cinfo, (long) length);
  
#ifdef XIE_SUPPORTED
  return(0);
#endif	/* XIE_SUPPORTED */
}

#ifdef XIE_SUPPORTED
LOCAL int
#if NeedFunctionPrototypes
get_sof (decompress_info_ptr cinfo, int code)
#else
get_sof (cinfo, code)
	decompress_info_ptr cinfo;
	int code;
#endif	/* NeedFunctionPrototypes */
#else
LOCAL void
get_sof (decompress_info_ptr cinfo, int code)
#endif	/* XIE_SUPPORTED */
/* Process a SOFn marker */
{
  INT32 length;
  short ci;
  int c;
  jpeg_component_info * compptr;
#ifdef XIE_SUPPORTED
  int j;
  INT32 k;
#endif	/* XIE_SUPPORTED */
  
  length = get_2bytes(cinfo);
#ifdef XIE_SUPPORTED
  if (length < 0)
  	return(-1);
#endif	/* XIE_SUPPORTED */
  
#ifdef XIE_SUPPORTED
  if ((j = JGETC(cinfo)) < 0)
    return(-1);
  cinfo->data_precision = j;
  if ((k = get_2bytes(cinfo)) < 0)
    return(-1);
  cinfo->image_height   = k;
  if ((k = get_2bytes(cinfo)) < 0)
    return(-1);
  cinfo->image_width    = k;
  if ((j = JGETC(cinfo)) < 0)
    return(-1);
  cinfo->num_components = j;
#else
  cinfo->data_precision = JGETC(cinfo);
  cinfo->image_height   = get_2bytes(cinfo);
  cinfo->image_width    = get_2bytes(cinfo);
  cinfo->num_components = JGETC(cinfo);

  TRACEMS4(cinfo->emethods, 1,
	   "Start Of Frame 0x%02x: width=%u, height=%u, components=%d",
	   code, (int) cinfo->image_width, (int) cinfo->image_height,
	   cinfo->num_components);
#endif	/* XIE_SUPPORTED */

  /* We don't support files in which the image height is initially specified */
  /* as 0 and is later redefined by DNL.  As long as we have to check that,  */
  /* might as well have a general sanity check. */
  if (cinfo->image_height <= 0 || cinfo->image_width <= 0
      || cinfo->num_components <= 0)
#ifdef XIE_SUPPORTED
    return(XIE_ERR);
#else
    ERREXIT(cinfo->emethods, "Empty JPEG image (DNL not supported)");
#endif	/* XIE_SUPPORTED */

#ifdef EIGHT_BIT_SAMPLES
  if (cinfo->data_precision != 8)
#ifdef XIE_SUPPORTED
    return(XIE_ERR);
#else
    ERREXIT(cinfo->emethods, "Unsupported JPEG data precision");
#endif	/* XIE_SUPPORTED */
#endif
#ifdef TWELVE_BIT_SAMPLES
  if (cinfo->data_precision != 12) /* this needs more thought?? */
#ifdef XIE_SUPPORTED
    return(XIE_ERR);
#else
    ERREXIT(cinfo->emethods, "Unsupported JPEG data precision");
#endif	/* XIE_SUPPORTED */
#endif
#ifdef SIXTEEN_BIT_SAMPLES
  if (cinfo->data_precision != 16) /* this needs more thought?? */
#ifdef XIE_SUPPORTED
    return(XIE_ERR);
#else
    ERREXIT(cinfo->emethods, "Unsupported JPEG data precision");
#endif	/* XIE_SUPPORTED */
#endif

  if (length != (cinfo->num_components * 3 + 8))
#ifdef XIE_SUPPORTED
    return(XIE_ERR);
#else
    ERREXIT(cinfo->emethods, "Bogus SOF length");
#endif	/* XIE_SUPPORTED */

#ifdef XIE_SUPPORTED
  cinfo->comp_info = (jpeg_component_info *) (*cinfo->emethods->d_alloc_small)
			(cinfo, (cinfo->num_components * SIZEOF(jpeg_component_info)));
  if (cinfo->comp_info == (jpeg_component_info *) NULL)
      return(XIE_ERR);
#else      
  cinfo->comp_info = (jpeg_component_info *) (*cinfo->emethods->alloc_small)
			(cinfo->num_components * SIZEOF(jpeg_component_info));
#endif	/* XIE_SUPPORTED */
  
  for (ci = 0; ci < cinfo->num_components; ci++) {
    compptr = &cinfo->comp_info[ci];
    compptr->component_index = ci;
#ifdef XIE_SUPPORTED
    if ((j = JGETC(cinfo)) < 0)
      return(-1);
    compptr->component_id = j;
    if ((c = JGETC(cinfo)) < 0)
      return(-1);
    compptr->h_samp_factor = (c >> 4) & 15;
    compptr->v_samp_factor = (c     ) & 15;
    if ((j = JGETC(cinfo)) < 0)
      return(-1);
    compptr->quant_tbl_no  = j;
#else
    compptr->component_id = JGETC(cinfo);
    c = JGETC(cinfo);
    compptr->h_samp_factor = (c >> 4) & 15;
    compptr->v_samp_factor = (c     ) & 15;
    compptr->quant_tbl_no  = JGETC(cinfo);
      
    TRACEMS4(cinfo->emethods, 1, "    Component %d: %dhx%dv q=%d",
	     compptr->component_id, compptr->h_samp_factor,
	     compptr->v_samp_factor, compptr->quant_tbl_no);
#endif	/* XIE_SUPPORTED */
  }
#ifdef XIE_SUPPORTED
  return(0);
#endif	/* XIE_SUPPORTED */
}


#ifdef XIE_SUPPORTED
LOCAL int
#if NeedFunctionPrototypes
get_sos (decompress_info_ptr cinfo)
#else
get_sos (cinfo)
	decompress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
#else
LOCAL void
get_sos (decompress_info_ptr cinfo)
#endif	/* XIE_SUPPORTED */
/* Process a SOS marker */
{
  INT32 length;
  int i, ci, n, c, cc;
  jpeg_component_info * compptr;
  
  length = get_2bytes(cinfo);
#ifdef XIE_SUPPORTED
  if (length < 0)
  	return(-1);
#endif	/* XIE_SUPPORTED */
  
  n = JGETC(cinfo);  /* Number of components */
#ifdef XIE_SUPPORTED
  if (n < 0)
  	return(-1);
#endif	/* XIE_SUPPORTED */
  cinfo->comps_in_scan = n;
  length -= 3;
  
  if (length != (n * 2 + 3) || n < 1 || n > MAX_COMPS_IN_SCAN)
#ifdef XIE_SUPPORTED
    return(XIE_ERR);
#else
    ERREXIT(cinfo->emethods, "Bogus SOS length");

  TRACEMS1(cinfo->emethods, 1, "Start Of Scan: %d components", n);
#endif	/* XIE_SUPPORTED */
  
  for (i = 0; i < n; i++) {
    cc = JGETC(cinfo);
#ifdef XIE_SUPPORTED
    if (cc < 0)
      return(-1);
#endif	/* XIE_SUPPORTED */
    c = JGETC(cinfo);
#ifdef XIE_SUPPORTED
    if (c < 0)
      return(-1);
#endif	/* XIE_SUPPORTED */
    length -= 2;
    
    for (ci = 0; ci < cinfo->num_components; ci++)
      if (cc == cinfo->comp_info[ci].component_id)
	break;
    
    if (ci >= cinfo->num_components)
#ifdef XIE_SUPPORTED
      return(XIE_ERR);
#else
      ERREXIT(cinfo->emethods, "Invalid component number in SOS");
#endif	/* XIE_SUPPORTED */
    
    compptr = &cinfo->comp_info[ci];
    cinfo->cur_comp_info[i] = compptr;
    compptr->dc_tbl_no = (c >> 4) & 15;
    compptr->ac_tbl_no = (c     ) & 15;
    
#ifndef XIE_SUPPORTED
    TRACEMS3(cinfo->emethods, 1, "    c%d: [dc=%d ac=%d]", cc,
	     compptr->dc_tbl_no, compptr->ac_tbl_no);
#endif	/* XIE_SUPPORTED */
  }
  
  while (length > 0) {
#ifdef XIE_SUPPORTED
    if (JGETC(cinfo) < 0)
      return(-1);
#else
    (void) JGETC(cinfo);
#endif	/* XIE_SUPPORTED */
    length--;
  }
#ifdef XIE_SUPPORTED
  return(0);
#endif	/* XIE_SUPPORTED */
}


LOCAL void
#ifdef XIE_SUPPORTED
#if NeedFunctionPrototypes
get_soi (decompress_info_ptr cinfo)
#else
get_soi (cinfo)
	decompress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
/* Process an SOI marker */
{
  int i;
  
#else
get_soi (decompress_info_ptr cinfo)
/* Process an SOI marker */
{
  int i;
  
  TRACEMS(cinfo->emethods, 1, "Start of Image");
#endif	/* XIE_SUPPORTED */

  /* Reset all parameters that are defined to be reset by SOI */

  for (i = 0; i < NUM_ARITH_TBLS; i++) {
    cinfo->arith_dc_L[i] = 0;
    cinfo->arith_dc_U[i] = 1;
    cinfo->arith_ac_K[i] = 5;
  }
  cinfo->restart_interval = 0;

  cinfo->density_unit = 0;	/* set default JFIF APP0 values */
  cinfo->X_density = 1;
  cinfo->Y_density = 1;

  cinfo->CCIR601_sampling = FALSE; /* Assume non-CCIR sampling */
}


LOCAL int
#ifdef XIE_SUPPORTED
#if NeedFunctionPrototypes
next_marker (decompress_info_ptr cinfo)
#else
next_marker (cinfo)
	decompress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
#else
next_marker (decompress_info_ptr cinfo)
#endif	/* XIE_SUPPORTED */
/* Find the next JPEG marker */
/* Note that the output might not be a valid marker code, */
/* but it will never be 0 or FF */
{
  int c, nbytes;

#ifdef XIE_SUPPORTED
  /* Save the location of the next marker (for restart) */
  cinfo->XIEnext_input_byte = cinfo->next_input_byte;
  cinfo->XIEbytes_in_buffer = cinfo->bytes_in_buffer;
#endif	/* XIE_SUPPORTED */
  nbytes = 0;
  do {
    do {			/* skip any non-FF bytes */
      nbytes++;
      c = JGETC(cinfo);
#ifdef XIE_SUPPORTED
      if (c < 0)
        return(-1);
#endif	/* XIE_SUPPORTED */
    } while (c != 0xFF);
    do {			/* skip any duplicate FFs */
      /* we don't increment nbytes here since extra FFs are legal */
      c = JGETC(cinfo);
#ifdef XIE_SUPPORTED
      if (c < 0)
        return(-1);
#endif	/* XIE_SUPPORTED */
    } while (c == 0xFF);
  } while (c == 0);		/* repeat if it was a stuffed FF/00 */

#ifndef XIE_SUPPORTED
  if (nbytes != 1)
    WARNMS2(cinfo->emethods,
	    "Corrupt JPEG data: %d extraneous bytes before marker 0x%02x",
	    nbytes-1, c);
#endif	/* XIE_SUPPORTED */

  return c;
}


#ifdef XIE_SUPPORTED
LOCAL int
#if NeedFunctionPrototypes
process_tables (decompress_info_ptr cinfo)
#else
process_tables (cinfo)
	decompress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
/* Scan and process JPEG markers that can appear in any order */
/* Return when an SOI, EOI, SOFn, or SOS is found */
{
  int status;
#else
LOCAL JPEG_MARKER
process_tables (decompress_info_ptr cinfo)
/* Scan and process JPEG markers that can appear in any order */
/* Return when an SOI, EOI, SOFn, or SOS is found */
{
#endif	/* XIE_SUPPORTED */
  int c;

  while (TRUE) {
    c = next_marker(cinfo);
#ifdef XIE_SUPPORTED
    if (c < 0)
      return(-1);
#endif	/* XIE_SUPPORTED */
      
    switch (c) {
    case M_SOF0:
    case M_SOF1:
    case M_SOF2:
    case M_SOF3:
    case M_SOF5:
    case M_SOF6:
    case M_SOF7:
    case M_JPG:
    case M_SOF9:
    case M_SOF10:
    case M_SOF11:
    case M_SOF13:
    case M_SOF14:
    case M_SOF15:
    case M_SOI:
    case M_EOI:
    case M_SOS:
#ifdef XIE_SUPPORTED
      return (c);
#else
      return ((JPEG_MARKER) c);
#endif	/* XIE_SUPPORTED */
      
    case M_DHT:
#ifdef XIE_SUPPORTED
      if ((status = get_dht(cinfo)) < 0)
        return(status);
#else
      get_dht(cinfo);
#endif	/* XIE_SUPPORTED */
      break;
      
    case M_DAC:
#ifdef XIE_SUPPORTED
      if ((status = get_dac(cinfo)) < 0)
        return(status);
#else
      get_dac(cinfo);
#endif	/* XIE_SUPPORTED */
      break;
      
    case M_DQT:
#ifdef XIE_SUPPORTED
      if ((status = get_dqt(cinfo)) < 0)
        return(status);
#else
      get_dqt(cinfo);
#endif	/* XIE_SUPPORTED */
      break;
      
    case M_DRI:
#ifdef XIE_SUPPORTED
      if ((status = get_dri(cinfo)) < 0)
        return(status);
#else
      get_dri(cinfo);
#endif	/* XIE_SUPPORTED */
      break;
      
    case M_APP0:
#ifdef XIE_SUPPORTED
      if ((status = get_app0(cinfo)) < 0)
        return(status);
#else
      get_app0(cinfo);
#endif	/* XIE_SUPPORTED */
      break;

    case M_COM:
#ifdef XIE_SUPPORTED
      if ((status = get_com(cinfo)) < 0)
        return(status);
#else
      get_com(cinfo);
#endif	/* XIE_SUPPORTED */
      break;

    case M_RST0:		/* these are all parameterless */
    case M_RST1:
    case M_RST2:
    case M_RST3:
    case M_RST4:
    case M_RST5:
    case M_RST6:
    case M_RST7:
    case M_TEM:
#ifndef XIE_SUPPORTED
      TRACEMS1(cinfo->emethods, 1, "Unexpected marker 0x%02x", c);
#endif	/* XIE_SUPPORTED */
      break;

    default:	/* must be DNL, DHP, EXP, APPn, JPGn, COM, or RESn */
#ifdef XIE_SUPPORTED
      if (skip_variable(cinfo, c) < 0)
        return(-1);
#else
      skip_variable(cinfo, c);
#endif	/* XIE_SUPPORTED */
      break;
    }
  }
}



/*
 * Initialize and read the file header (everything through the SOF marker).
 */

#ifdef XIE_SUPPORTED
METHODDEF int
#if NeedFunctionPrototypes
read_file_header (decompress_info_ptr cinfo)
#else
read_file_header (cinfo)
	decompress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
{
  int i, j;
  int status;
#else
METHODDEF void
read_file_header (decompress_info_ptr cinfo)
{
#endif	/* XIE_SUPPORTED */
  int c;

  /* Demand an SOI marker at the start of the file --- otherwise it's
   * probably not a JPEG file at all.  If the user interface wants to support
   * nonstandard headers in front of the SOI, it must skip over them itself
   * before calling jpeg_decompress().
   */
#ifdef XIE_SUPPORTED
  if (cinfo->XIErestart != XIE_RRFH) {	/* don't read signature bytes on restart */
    if ((i = JGETC(cinfo)) < 0)
      return(-1);
    if ((j = JGETC(cinfo)) < 0)
      return(-1);
    if (i != 0xFF  ||  j != M_SOI)
      return(XIE_ERR);
      
    get_soi(cinfo);		/* OK, process SOI */
  }
#else
  if (JGETC(cinfo) != 0xFF  ||  JGETC(cinfo) != M_SOI)
    ERREXIT(cinfo->emethods, "Not a JPEG file");

  get_soi(cinfo);		/* OK, process SOI */
#endif	/* XIE_SUPPORTED */

  /* Process markers until SOF */
  c = process_tables(cinfo);
#ifdef XIE_SUPPORTED
  if (c < 0)
    return(c);
#endif	/* XIE_SUPPORTED */

  switch (c) {
  case M_SOF0:
  case M_SOF1:
#ifdef XIE_SUPPORTED
    if ((status = get_sof(cinfo, c)) < 0)
      return(status);
#else
    get_sof(cinfo, c);
#endif	/* XIE_SUPPORTED */
    cinfo->arith_code = FALSE;
    break;
      
  case M_SOF9:
#ifdef XIE_SUPPORTED
    if ((status = get_sof(cinfo, c)) < 0)
      return(status);
#else
    get_sof(cinfo, c);
#endif	/* XIE_SUPPORTED */
    cinfo->arith_code = TRUE;
    break;

  default:
#ifdef XIE_SUPPORTED
    return(XIE_ERR);
#else
    ERREXIT1(cinfo->emethods, "Unsupported SOF marker type 0x%02x", c);
#endif	/* XIE_SUPPORTED */
    break;
  }

  /* Figure out what colorspace we have */
  /* (too bad the JPEG committee didn't provide a real way to specify this) */

  switch (cinfo->num_components) {
  case 1:
    cinfo->jpeg_color_space = CS_GRAYSCALE;
    break;

  case 3:
    /* if we saw a JFIF marker, leave it set to YCbCr; */
    /* also leave it alone if UI has provided a value */
    if (cinfo->jpeg_color_space == CS_UNKNOWN) {
      short cid0 = cinfo->comp_info[0].component_id;
      short cid1 = cinfo->comp_info[1].component_id;
      short cid2 = cinfo->comp_info[2].component_id;

      if (cid0 == 1 && cid1 == 2 && cid2 == 3)
	cinfo->jpeg_color_space = CS_YCbCr; /* assume it's JFIF w/out marker */
      else if (cid0 == 1 && cid1 == 4 && cid2 == 5)
	cinfo->jpeg_color_space = CS_YIQ; /* prototype's YIQ matrix */
      else {
#ifndef XIE_SUPPORTED
	TRACEMS3(cinfo->emethods, 1,
		 "Unrecognized component IDs %d %d %d, assuming YCbCr",
		 cid0, cid1, cid2);
#endif	/* XIE_SUPPORTED */
	cinfo->jpeg_color_space = CS_YCbCr;
      }
    }
    break;

  case 4:
    cinfo->jpeg_color_space = CS_CMYK;
    break;

  default:
    cinfo->jpeg_color_space = CS_UNKNOWN;
    break;
  }
#ifdef XIE_SUPPORTED
  return(0);
#endif	/* XIE_SUPPORTED */
}


/*
 * Read the start of a scan (everything through the SOS marker).
 * Return TRUE if find SOS, FALSE if find EOI.
 */

#ifdef XIE_SUPPORTED
METHODDEF int
#if NeedFunctionPrototypes
read_scan_header (decompress_info_ptr cinfo)
#else
read_scan_header (cinfo)
	decompress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
#else
METHODDEF boolean
read_scan_header (decompress_info_ptr cinfo)
#endif	/* XIE_SUPPORTED */
{
  int c;
  
  /* Process markers until SOS or EOI */
  c = process_tables(cinfo);
#ifdef XIE_SUPPORTED
  if (c < 0)
    return(c);
#endif	/* XIE_SUPPORTED */
  
  switch (c) {
  case M_SOS:
#ifdef XIE_SUPPORTED
    return(get_sos(cinfo));		/* 0, OK; -1, read error */
#else
    get_sos(cinfo);
    return TRUE;
#endif	/* XIE_SUPPORTED */
    
  case M_EOI:
#ifndef XIE_SUPPORTED
    TRACEMS(cinfo->emethods, 1, "End Of Image");
#endif	/* XIE_SUPPORTED */
    return FALSE;

  default:
#ifdef XIE_SUPPORTED
    return(XIE_ERR);
#else
    ERREXIT1(cinfo->emethods, "Unexpected marker 0x%02x", c);
#endif	/* XIE_SUPPORTED */
    break;
  }
#ifdef XIE_SUPPORTED
  return(0);
#else
  return FALSE;			/* keeps lint happy */
#endif	/* XIE_SUPPORTED */
}


/*
 * The entropy decoder calls this routine if it finds a marker other than
 * the restart marker it was expecting.  (This code is *not* used unless
 * a nonzero restart interval has been declared.)  The passed parameter is
 * the marker code actually found (might be anything, except 0 or FF).
 * The desired restart marker is that indicated by cinfo->next_restart_num.
 * This routine is supposed to apply whatever error recovery strategy seems
 * appropriate in order to position the input stream to the next data segment.
 * For some file formats (eg, TIFF) extra information such as tile boundary
 * pointers may be available to help in this decision.
 *
 * This implementation is substantially constrained by wanting to treat the
 * input as a data stream; this means we can't back up.  (For instance, we
 * generally can't fseek() if the input is a Unix pipe.)  Therefore, we have
 * only the following actions to work with:
 *   1. Do nothing, let the entropy decoder resume at next byte of file.
 *   2. Read forward until we find another marker, discarding intervening
 *      data.  (In theory we could look ahead within the current bufferload,
 *      without having to discard data if we don't find the desired marker.
 *      This idea is not implemented here, in part because it makes behavior
 *      dependent on buffer size and chance buffer-boundary positions.)
 *   3. Push back the passed marker (with JUNGETC).  This will cause the
 *      entropy decoder to process an empty data segment, inserting dummy
 *      zeroes, and then re-read the marker we pushed back.
 * #2 is appropriate if we think the desired marker lies ahead, while #3 is
 * appropriate if the found marker is a future restart marker (indicating
 * that we have missed the desired restart marker, probably because it got
 * corrupted).

 * We apply #2 or #3 if the found marker is a restart marker no more than
 * two counts behind or ahead of the expected one.  We also apply #2 if the
 * found marker is not a legal JPEG marker code (it's certainly bogus data).
 * If the found marker is a restart marker more than 2 counts away, we do #1
 * (too much risk that the marker is erroneous; with luck we will be able to
 * resync at some future point).
 * For any valid non-restart JPEG marker, we apply #3.  This keeps us from
 * overrunning the end of a scan.  An implementation limited to single-scan
 * files might find it better to apply #2 for markers other than EOI, since
 * any other marker would have to be bogus data in that case.
 */

#ifdef XIE_SUPPORTED
METHODDEF int
#if NeedFunctionPrototypes
resync_to_restart (decompress_info_ptr cinfo, int marker)
#else
resync_to_restart (cinfo, marker)
	decompress_info_ptr cinfo;
    int marker;
#endif	/* NeedFunctionPrototypes */
#else
METHODDEF void
resync_to_restart (decompress_info_ptr cinfo, int marker)
#endif	/* XIE_SUPPORTED */
{
  int desired = cinfo->next_restart_num;
  int action = 1;

  /* Always put up a warning. */
#ifndef XIE_SUPPORTED
  WARNMS2(cinfo->emethods,
	  "Corrupt JPEG data: found 0x%02x marker instead of RST%d",
	  marker, desired);
#endif	/* XIE_SUPPORTED */
  /* Outer loop handles repeated decision after scanning forward. */
  for (;;) {
    if (marker < (int) M_SOF0)
      action = 2;		/* invalid marker */
    else if (marker < (int) M_RST0 || marker > (int) M_RST7)
      action = 3;		/* valid non-restart marker */
    else {
      if (marker == ((int) M_RST0 + ((desired+1) & 7)) ||
	  marker == ((int) M_RST0 + ((desired+2) & 7)))
	action = 3;		/* one of the next two expected restarts */
      else if (marker == ((int) M_RST0 + ((desired-1) & 7)) ||
	       marker == ((int) M_RST0 + ((desired-2) & 7)))
	action = 2;		/* a prior restart, so advance */
      else
	action = 1;		/* desired restart or too far away */
    }
#ifndef XIE_SUPPORTED
    TRACEMS2(cinfo->emethods, 4,
	     "At marker 0x%02x, recovery action %d", marker, action);
#endif	/* XIE_SUPPORTED */
    switch (action) {
    case 1:
      /* Let entropy decoder resume processing. */
#ifdef XIE_SUPPORTED
      return(0);
#else
      return;
#endif	/* XIE_SUPPORTED */
    case 2:
      /* Scan to the next marker, and repeat the decision loop. */
      marker = next_marker(cinfo);
#ifdef XIE_SUPPORTED
      if (marker < 0)
        return(-1);
#endif	/* XIE_SUPPORTED */
      break;
    case 3:
      /* Put back this marker & return. */
      /* Entropy decoder will be forced to process an empty segment. */
      JUNGETC(marker, cinfo);
      JUNGETC(0xFF, cinfo);
#ifdef XIE_SUPPORTED
      return(0);
#else
      return;
#endif	/* XIE_SUPPORTED */
    }
  }
}


/*
 * Finish up after a compressed scan (series of read_jpeg_data calls);
 * prepare for another read_scan_header call.
 */

#ifndef XIE_SUPPORTED
METHODDEF void
#ifdef XIE_SUPPORTED
#if NeedFunctionPrototypes
read_scan_trailer (decompress_info_ptr cinfo)
#else
read_scan_trailer (cinfo)
	decompress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
#else
read_scan_trailer (decompress_info_ptr cinfo)
#endif	/* XIE_SUPPORTED */
{
  /* no work needed */
}
#endif   /* XIE_SUPPORTED */

/*
 * Finish up at the end of the file.
 */

#ifndef XIE_SUPPORTED
METHODDEF void
#ifdef XIE_SUPPORTED
#if NeedFunctionPrototypes
read_file_trailer (decompress_info_ptr cinfo)
#else
read_file_trailer (cinfo)
	decompress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
#else
read_file_trailer (decompress_info_ptr cinfo)
#endif	/* XIE_SUPPORTED */
{
  /* no work needed */
}
#endif   /* XIE_SUPPORTED */

/*
 * The method selection routine for standard JPEG header reading.
 * Note that this must be called by the user interface before calling
 * jpeg_decompress.  When a non-JFIF file is to be decompressed (TIFF,
 * perhaps), the user interface must discover the file type and call
 * the appropriate method selection routine.
 */

GLOBAL void
#ifdef XIE_SUPPORTED
#if NeedFunctionPrototypes
jselrjfif (decompress_info_ptr cinfo)
#else
jselrjfif (cinfo)
	decompress_info_ptr cinfo;
#endif	/* NeedFunctionPrototypes */
#else
jselrjfif (decompress_info_ptr cinfo)
#endif	/* XIE_SUPPORTED */
{
  cinfo->methods->read_file_header = read_file_header;
  cinfo->methods->read_scan_header = read_scan_header;
  /* For JFIF/raw-JPEG format, the user interface supplies read_jpeg_data. */
#ifndef XIE_SUPPORTED
#if NOTDEF
  cinfo->methods->read_jpeg_data = read_jpeg_data;
#endif
#endif	/* XIE_SUPPORTED */
  cinfo->methods->resync_to_restart = resync_to_restart;
#ifndef XIE_SUPPORTED	  
  cinfo->methods->read_scan_trailer = read_scan_trailer;
  cinfo->methods->read_file_trailer = read_file_trailer;
#endif   /* XIE_SUPPORTED */  
}

#endif /* JFIF_SUPPORTED */
