/* $Xorg: fetabs.h,v 1.4 2001/02/09 02:04:27 xorgcvs Exp $ */
/**** module fetabs.h ****/
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

	fetabs.h -- header file holding fax encoding tables

	Ben Fahy, AGE Logic, Sept 1993 

******************************************************************************/

typedef struct _shifted_codes {
	unsigned short code;
	unsigned short nbits;
} ShiftedCodes;

#define EOL_CODE (0x001 << 4)
#define EOL_BIT_LENGTH 12

#define EOL_1D_CODE (0x0018)
#define EOL_1D_BIT_LENGTH 13

#define EOL_2D_CODE (0x0010)
#define EOL_2D_BIT_LENGTH 13

#define PASS_CODE (0x1 << 12)
#define PASS_CODE_BIT_LENGTH 4

#define HORIZONTAL_CODE (0x1 << 13)
#define HORIZONTAL_CODE_LENGTH 3

#define DEDUCE_Vcode(b1_minus_a1,rlcode,nmbits) 	\
    { rlcode = VerticalCodes[3+b1_minus_a1].code; 	\
      nmbits = VerticalCodes[3+b1_minus_a1].nbits; 	\
    }
   

#ifdef FETABS_OWNER
ShiftedCodes VerticalCodes[] = {
	(0x03 << 9), 7,		/* a1 3 rite of b1 */
	(0x03 <<10), 6,		/* a1 2 rite of b1 */
	(0x03 <<13), 3,		/* a1 1 rite of b1 */
	(0x01 <<15), 1,		/* a1 0 left of b1 */
	(0x02 <<13), 3,		/* a1 1 left of b1 */
	(0x02 <<10), 6,		/* a1 2 left of b1 */
	(0x02 << 9), 7,		/* a1 3 left of b1 */
};

ShiftedCodes ShiftedWhites[] = {
	(0x35 << 8), 8,		/* run length of zero */
	(0x07 <<10), 6,		/* run length of 1 */
	(0x07 <<12), 4,		/* run length of 2 */
	(0x08 <<12), 4,		/* run length of 3 */
	(0x0b <<12), 4,		/* run length of 4 */
	(0x0c <<12), 4,		/* run length of 5 */
	(0x0e <<12), 4,		/* run length of 6 */
	(0x0f <<12), 4,		/* run length of 7 */
	(0x13 <<11), 5,		/* run length of 8 */
	(0x14 <<11), 5,		/* run length of 9 */
	(0x07 <<11), 5,		/* run length of 10 */
	(0x08 <<11), 5,		/* run length of 11 */
	(0x08 <<10), 6,		/* run length of 12 */
	(0x03 <<10), 6,		/* run length of 13 */
	(0x34 <<10), 6,		/* run length of 14 */
	(0x35 <<10), 6,		/* run length of 15 */
	(0x2a <<10), 6,		/* run length of 16 */
	(0x2b <<10), 6,		/* run length of 17 */
	(0x27 << 9), 7,		/* run length of 18 */
	(0x0c << 9), 7,		/* run length of 19 */
	(0x08 << 9), 7,		/* run length of 20 */
	(0x17 << 9), 7,		/* run length of 21 */
	(0x03 << 9), 7,		/* run length of 22 */
	(0x04 << 9), 7,		/* run length of 23 */
	(0x28 << 9), 7,		/* run length of 24 */
	(0x2b << 9), 7,		/* run length of 25 */
	(0x13 << 9), 7,		/* run length of 26 */
	(0x24 << 9), 7,		/* run length of 27 */
	(0x18 << 9), 7,		/* run length of 28 */
	(0x02 << 8), 8,		/* run length of 29 */
	(0x03 << 8), 8,		/* run length of 30 */
	(0x1a << 8), 8,		/* run length of 31 */
	(0x1b << 8), 8,		/* run length of 32 */
	(0x12 << 8), 8,		/* run length of 33 */
	(0x13 << 8), 8,		/* run length of 34 */
	(0x14 << 8), 8,		/* run length of 35 */
	(0x15 << 8), 8,		/* run length of 36 */
	(0x16 << 8), 8,		/* run length of 37 */
	(0x17 << 8), 8,		/* run length of 38 */
	(0x28 << 8), 8,		/* run length of 39 */
	(0x29 << 8), 8,		/* run length of 40 */
	(0x2a << 8), 8,		/* run length of 41 */
	(0x2b << 8), 8,		/* run length of 42 */
	(0x2c << 8), 8,		/* run length of 43 */
	(0x2d << 8), 8,		/* run length of 44 */
	(0x04 << 8), 8,		/* run length of 45 */
	(0x05 << 8), 8,		/* run length of 46 */
	(0x0a << 8), 8,		/* run length of 47 */
	(0x0b << 8), 8,		/* run length of 48 */
	(0x52 << 8), 8,		/* run length of 49 */
	(0x53 << 8), 8,		/* run length of 50 */
	(0x54 << 8), 8,		/* run length of 51 */
	(0x55 << 8), 8,		/* run length of 52 */
	(0x24 << 8), 8,		/* run length of 53 */
	(0x25 << 8), 8,		/* run length of 54 */
	(0x58 << 8), 8,		/* run length of 55 */
	(0x59 << 8), 8,		/* run length of 56 */
	(0x5a << 8), 8,		/* run length of 57 */
	(0x5b << 8), 8,		/* run length of 58 */
	(0x4a << 8), 8,		/* run length of 59 */
	(0x4b << 8), 8,		/* run length of 60 */
	(0x32 << 8), 8,		/* run length of 61 */
	(0x33 << 8), 8,		/* run length of 62 */
	(0x34 << 8), 8,		/* run length of 63 */
	(0x1b << 11),5,		/* run length of 64 */
	(0x12 << 11),5,		/* run length of 2*64 = 128 */
	(0x17 << 10),6,		/* run length of 3*64 = 192 */
	(0x37 << 9), 7,		/* run length of 4*64 = 256 */
	(0x36 << 8), 8,		/* run length of 5*64 = 320 */
	(0x37 << 8), 8,		/* run length of 6*64 = 384 */
	(0x64 << 8), 8,		/* run length of 7*64 = 448 */
	(0x65 << 8), 8,		/* run length of 8*64 = 512 */
	(0x68 << 8), 8,		/* run length of 9*64 = 576 */
	(0x67 << 8), 8,		/* run length of 10*64 = 640 */
	(0x0cc<< 7), 9,		/* run length of 11*64 = 704 */
	(0x0cd<< 7), 9,		/* run length of 12*64 = 768 */
	(0x0d2<< 7), 9,		/* run length of 13*64 = 832 */
	(0x0d3<< 7), 9,		/* run length of 14*64 = 896 */
	(0x0d4<< 7), 9,		/* run length of 15*64 = 960 */
	(0x0d5<< 7), 9,		/* run length of 16*64 = 1024 */
	(0x0d6<< 7), 9,		/* run length of 17*64 = 1088 */
	(0x0d7<< 7), 9,		/* run length of 18*64 = 1152 */
	(0x0d8<< 7), 9,		/* run length of 19*64 = 1216 */
	(0x0d9<< 7), 9,		/* run length of 20*64 = 1280 */
	(0x0da<< 7), 9,		/* run length of 21*64 = 1344 */
	(0x0db<< 7), 9,		/* run length of 22*64 = 1408 */
	(0x098<< 7), 9,		/* run length of 23*64 = 1472 */
	(0x099<< 7), 9,		/* run length of 24*64 = 1536 */
	(0x09a<< 7), 9,		/* run length of 25*64 = 1600 */
	(0x18 <<10), 6,		/* run length of 26*64 = 1664 */
	(0x09b<< 7), 9,		/* run length of 27*64 = 1728 */
	(0x08 << 5), 11,	/* run length of 28*64 = 1792 */
	(0x0c << 5), 11,	/* run length of 29*64 = 1856 */
	(0x0d << 5), 11,	/* run length of 30*64 = 1920 */
	(0x12 << 4), 12,	/* run length of 31*64 = 1984 */
	(0x13 << 4), 12,	/* run length of 32*64 = 2048 */
	(0x14 << 4), 12,	/* run length of 33*64 = 2112 */
	(0x15 << 4), 12,	/* run length of 34*64 = 2176 */
	(0x16 << 4), 12,	/* run length of 35*64 = 2240 */
	(0x17 << 4), 12,	/* run length of 36*64 = 2304 */
	(0x1c << 4), 12,	/* run length of 37*64 = 2368 */
	(0x1d << 4), 12,	/* run length of 38*64 = 2432 */
	(0x1e << 4), 12,	/* run length of 39*64 = 2496 */
	(0x1f << 4), 12,	/* run length of 40*64 = 2560 */
};

ShiftedCodes ShiftedBlacks[] = {
	(0x37 << 6),10,		/* run length of zero */
	(0x2  <<13), 3,		/* run length of 1 */
	(0x3  <<14), 2,		/* run length of 2 */
	(0x2  <<14), 2,		/* run length of 3 */
	(0x03 <<13), 3,		/* run length of 4 */
	(0x03 <<12), 4,		/* run length of 5 */
	(0x02 <<12), 4,		/* run length of 6 */
	(0x03 <<11), 5,		/* run length of 7 */
	(0x05 <<10), 6,		/* run length of 8 */
	(0x04 <<10), 6,		/* run length of 9 */
	(0x04 << 9), 7,		/* run length of 10 */
	(0x05 << 9), 7,		/* run length of 11 */
	(0x07 << 9), 7,		/* run length of 12 */
	(0x04 << 8), 8,		/* run length of 13 */
	(0x07 << 8), 8,		/* run length of 14 */
	(0x18 << 7), 9,		/* run length of 15 */
	(0x17 << 6),10,		/* run length of 16 */
	(0x18 << 6),10,		/* run length of 17 */
	(0x08 << 6),10,		/* run length of 18 */
	(0x67 << 5),11,		/* run length of 19 */
	(0x68 << 5),11,		/* run length of 20 */
	(0x6c << 5),11,		/* run length of 21 */
	(0x37 << 5),11,		/* run length of 22 */
	(0x28 << 5),11,		/* run length of 23 */
	(0x17 << 5),11,		/* run length of 24 */
	(0x18 << 5),11,		/* run length of 25 */
	(0xca << 4),12,		/* run length of 26 */
	(0xcb << 4),12,		/* run length of 27 */
	(0xcc << 4),12,		/* run length of 28 */
	(0xcd << 4),12,		/* run length of 29 */
	(0x68 << 4),12,		/* run length of 30 */
	(0x69 << 4),12,		/* run length of 31 */
	(0x6a << 4),12,		/* run length of 32 */
	(0x6b << 4),12,		/* run length of 33 */
	(0xd2 << 4),12,		/* run length of 34 */
	(0xd3 << 4),12,		/* run length of 35 */
	(0xd4 << 4),12,		/* run length of 36 */
	(0xd5 << 4),12,		/* run length of 37 */
	(0xd6 << 4),12,		/* run length of 38 */
	(0xd7 << 4),12,		/* run length of 39 */
	(0x6c << 4),12,		/* run length of 40 */
	(0x6d << 4),12,		/* run length of 41 */
	(0xda << 4),12,		/* run length of 42 */
	(0xdb << 4),12,		/* run length of 43 */
	(0x54 << 4),12,		/* run length of 44 */
	(0x55 << 4),12,		/* run length of 45 */
	(0x56 << 4),12,		/* run length of 46 */
	(0x57 << 4),12,		/* run length of 47 */
	(0x64 << 4),12,		/* run length of 48 */
	(0x65 << 4),12,		/* run length of 49 */
	(0x52 << 4),12,		/* run length of 50 */
	(0x53 << 4),12,		/* run length of 51 */
	(0x24 << 4),12,		/* run length of 52 */
	(0x37 << 4),12,		/* run length of 53 */
	(0x38 << 4),12,		/* run length of 54 */
	(0x27 << 4),12,		/* run length of 55 */
	(0x28 << 4),12,		/* run length of 56 */
	(0x58 << 4),12,		/* run length of 57 */
	(0x59 << 4),12,		/* run length of 58 */
	(0x2b << 4),12,		/* run length of 59 */
	(0x2c << 4),12,		/* run length of 60 */
	(0x5a << 4),12,		/* run length of 61 */
	(0x66 << 4),12,		/* run length of 62 */
	(0x67 << 4),12,		/* run length of 63 */
	(0x0f << 6),10,		/* run length of 64 */
	(0xc8 << 4),12,		/* run length of 2*64 = 128 */
	(0xc9 << 4),12,		/* run length of 3*64 = 192 */
	(0x5b << 4),12,		/* run length of 4*64 = 256 */
	(0x33 << 4),12,		/* run length of 5*64 = 320 */
	(0x34 << 4),12,		/* run length of 6*64 = 384 */
	(0x35 << 4),12,		/* run length of 7*64 = 448 */
	(0x6c << 3),13,		/* run length of 8*64 = 512 */
	(0x6d << 3),13,		/* run length of 9*64 = 576 */
	(0x4a << 3),13,		/* run length of 10*64 = 640 */
	(0x4b << 3),13,		/* run length of 11*64 = 704 */
	(0x4c << 3),13,		/* run length of 12*64 = 768 */
	(0x4d << 3),13,		/* run length of 13*64 = 832 */
	(0x72 << 3),13,		/* run length of 14*64 = 896 */
	(0x73 << 3),13,		/* run length of 15*64 = 960 */
	(0x74 << 3),13,		/* run length of 16*64 = 1024 */
	(0x75 << 3),13,		/* run length of 17*64 = 1088 */
	(0x76 << 3),13,		/* run length of 18*64 = 1152 */
	(0x77 << 3),13,		/* run length of 19*64 = 1216 */
	(0x52 << 3),13,		/* run length of 20*64 = 1280 */
	(0x53 << 3),13,		/* run length of 21*64 = 1344 */
	(0x54 << 3),13,		/* run length of 22*64 = 1408 */
	(0x55 << 3),13,		/* run length of 23*64 = 1472 */
	(0x5a << 3),13,		/* run length of 24*64 = 1536 */
	(0x5b << 3),13,		/* run length of 25*64 = 1600 */
	(0x64 << 3),13,		/* run length of 26*64 = 1664 */
	(0x65 << 3),13,		/* run length of 27*64 = 1728 */
	(0x08 << 5), 11,	/* run length of 28*64 = 1792 */
	(0x0c << 5), 11,	/* run length of 29*64 = 1856 */
	(0x0d << 5), 11,	/* run length of 30*64 = 1920 */
	(0x12 << 4), 12,	/* run length of 31*64 = 1984 */
	(0x13 << 4), 12,	/* run length of 32*64 = 2048 */
	(0x14 << 4), 12,	/* run length of 33*64 = 2112 */
	(0x15 << 4), 12,	/* run length of 34*64 = 2176 */
	(0x16 << 4), 12,	/* run length of 35*64 = 2240 */
	(0x17 << 4), 12,	/* run length of 36*64 = 2304 */
	(0x1c << 4), 12,	/* run length of 37*64 = 2368 */
	(0x1d << 4), 12,	/* run length of 38*64 = 2432 */
	(0x1e << 4), 12,	/* run length of 39*64 = 2496 */
	(0x1f << 4), 12,	/* run length of 40*64 = 2560 */
};

#else
extern ShiftedCodes VerticalCodes[];
extern ShiftedCodes ShiftedWhites[];
extern ShiftedCodes ShiftedBlacks[];
#endif
