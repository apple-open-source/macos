/**************************************************************************

Copyright 2000 Silicon Integrated Systems Corp, Inc., HsinChu, Taiwan.
All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/
/* $XFree86: xc/lib/GL/mesa/src/drv/sis/sis_init.h,v 1.3 2000/09/26 15:56:48 tsi Exp $ */

/*
 * Authors:
 *    Sung-Ching Lin <sclin@sis.com.tw>
 *
 */

#ifndef _sis_init_h_
#define _sis_init_h_

typedef LONG fixed;

typedef struct _FIXEDCOLOR
{
  fixed r, g, b, a;
}
FIXEDCOLOR;

#define COMMANDMODE_MMIO    1
#define COMMANDMODE_WC      2
#define COMMANDMODE_AGP     0
#define	_HAS_AGPBUF 1

#define floatConvertToFixedRGB(fixedColor, floatColor)\
{\
  fixedColor.r = (fixed) (floatColor.r * gc->constants.oneOverRedScale * ((float) 0xffffff));\
  fixedColor.g = (fixed) (floatColor.g * gc->constants.oneOverGreenScale * ((float) 0xffffff));\
  fixedColor.b = (fixed) (floatColor.b * gc->constants.oneOverBlueScale * ((float) 0xffffff));\
  fixedColor.a = (fixed) (floatColor.a * gc->constants.oneOverAlphaScale * ((float) 0xffffff));\
}

/* Optimize */
#define RGBA8ConvertToBGRA8(fixedColor, color)\
{\
  ((GLubyte *)fixedColor)[0] = ((GLubyte *)color)[2] ; \
  ((GLubyte *)fixedColor)[1] = ((GLubyte *)color)[1] ; \
  ((GLubyte *)fixedColor)[2] = ((GLubyte *)color)[0] ; \
  ((GLubyte *)fixedColor)[3] = ((GLubyte *)color)[3] ; \
}

/* Put ARGB into DWORD */
#define setTSARGB(dcSARGB, fixedColor)\
{\
  dcSARGB = ((fixedColor.a & 0x00ff0000) << 8 | \
            (fixedColor.r & 0x00ff0000) | \
            (fixedColor.g & 0x00ff0000) >> 8 | \
            (fixedColor.b & 0x00ff0000) >> 16 ); \
}

#define setTSFS(dwFactor, fFactor) \
{\
  (dwFactor) = ((fixed)((fFactor) *  0xff))  << 24;\
}

#define MMIO(reg, value) \
{\
  *(LPDWORD)(GET_IOBase(hwcx) + (reg)) = value; \
}

#define mEndPrimitive()  \
{       \
  *(GET_IOBase(hwcx) + REG_3D_EndPrimitiveList) = 0xFF;   \
  *(DWORD *)(GET_IOBase(hwcx) + 0x8b60) = (DWORD)(-1); \
}

#define INIT_6327_CapsEnable            0x00000080
#define INIT_6327_CapsEnable2           0x00000000

#define INIT_6327_ZSet                  0x00030000
#define INIT_6327_ZMask			0xffffffff
#define INIT_6327_AlphaSet              0x07000000
#define INIT_6327_DstSet                0x0C000000
#define INIT_6327_DstMask		0xffffffff
#define INIT_6327_FogSet                0x04000000
#define INIT_6327_BlendMode             0x00000001
#define INIT_6327_TextureSet            0x00030000
#define INIT_6327_TextureMip            0x00000000
/* #define INIT_6327_Texture0BlendSet    0x33031941 */
#define INIT_6327_TextureColorBlend0    0xC1485000
#define INIT_6327_TextureAlphaBlend0    0x333A0000
#define INIT_6327_Texture1Set           0x00030000
#define INIT_6327_Texture1Set2          0x00000000
/* #define INIT_6327_Texture1BlendSet    0x00000000 */
#define INIT_6327_TextureColorBlend1    0x294B4000
#define INIT_6327_TextureAlphaBlend1    0x333A0000
/* #define INIT_6327_TexAddrType         0x00001000 */
/* #define INIT_6326_InputColorFormat    0xA0000000 */
#define INIT_6327_ParsingSet            0x00000060

#define SiS_Z_COMP_NEVER                  0x00000000
#define SiS_Z_COMP_S_LT_B                 0x00010000
#define SiS_Z_COMP_S_EQ_B                 0x00020000
#define SiS_Z_COMP_S_LE_B                 0x00030000
#define SiS_Z_COMP_S_GT_B                 0x00040000
#define SiS_Z_COMP_S_NE_B                 0x00050000
#define SiS_Z_COMP_S_GE_B                 0x00060000
#define SiS_Z_COMP_ALWAYS                 0x00070000

#define SiS_ALPHA_NEVER                   0x00000000
#define SiS_ALPHA_LESS                    0x01000000
#define SiS_ALPHA_EQUAL                   0x02000000
#define SiS_ALPHA_LEQUAL                  0x03000000
#define SiS_ALPHA_GREATER                 0x04000000
#define SiS_ALPHA_NOTEQUAL                0x05000000
#define SiS_ALPHA_GEQUAL                  0x06000000
#define SiS_ALPHA_ALWAYS                  0x07000000

#define SiS_STENCIL_NEVER		  0x00000000
#define SiS_STENCIL_LESS                  0x01000000
#define SiS_STENCIL_EQUAL                 0x02000000
#define SiS_STENCIL_LEQUAL                0x03000000
#define SiS_STENCIL_GREATER               0x04000000
#define SiS_STENCIL_NOTEQUAL		  0x05000000
#define SiS_STENCIL_GEQUAL                0x06000000
#define SiS_STENCIL_ALWAYS                0x07000000

#define SiS_SFAIL_KEEP			  0x00000000
#define SiS_SFAIL_ZERO			  0x00100000
#define SiS_SFAIL_REPLACE                 0x00200000
#define SiS_SFAIL_INVERT		  0x00500000
#define SiS_SFAIL_INCR			  0x00600000
#define SiS_SFAIL_DECR			  0x00700000

#define SiS_SPASS_ZFAIL_KEEP			  0x00000000
#define SiS_SPASS_ZFAIL_ZERO			  0x00010000
#define SiS_SPASS_ZFAIL_REPLACE			  0x00020000
#define SiS_SPASS_ZFAIL_INVERT			  0x00050000
#define SiS_SPASS_ZFAIL_INCR			  0x00060000
#define SiS_SPASS_ZFAIL_DECR			  0x00070000

#define SiS_SPASS_ZPASS_KEEP			  0x00000000
#define SiS_SPASS_ZPASS_ZERO			  0x00001000
#define SiS_SPASS_ZPASS_REPLACE			  0x00002000
#define SiS_SPASS_ZPASS_INVERT			  0x00005000
#define SiS_SPASS_ZPASS_INCR			  0x00006000
#define SiS_SPASS_ZPASS_DECR			  0x00007000

#define SiS_D_ZERO			      0x00000000
#define SiS_D_ONE			      0x00000010
#define SiS_D_SRC_COLOR			      0x00000020
#define SiS_D_ONE_MINUS_SRC_COLOR	      0x00000030
#define SiS_D_SRC_ALPHA			      0x00000040
#define SiS_D_ONE_MINUS_SRC_ALPHA	      0x00000050
#define SiS_D_DST_ALPHA			      0x00000060
#define SiS_D_ONE_MINUS_DST_ALPHA	      0x00000070

#define SiS_S_ZERO			  0x00000000
#define SiS_S_ONE			  0x00000001
#define SiS_S_SRC_ALPHA                   0x00000004
#define SiS_S_ONE_MINUS_SRC_ALPHA	  0x00000005
#define SiS_S_DST_ALPHA                   0x00000006
#define SiS_S_ONE_MINUS_DST_ALPHA         0x00000007
#define SiS_S_DST_COLOR                   0x00000008
#define SiS_S_ONE_MINUS_DST_COLOR         0x00000009
#define SiS_S_SRC_ALPHA_SATURATE          0x0000000a

/* Logic Op */
#define LOP_CLEAR						  0x00000000
#define LOP_NOR							  0x01000000
#define LOP_AND_INVERTED				  0x02000000
#define LOP_COPY_INVERTED				  0x03000000
#define LOP_AND_REVERSE					  0x04000000
#define LOP_INVERT						  0x05000000
#define LOP_XOR							  0x06000000
#define LOP_NAND						  0x07000000
#define LOP_AND							  0x08000000
#define LOP_EQUIV						  0x09000000
#define LOP_NOOP						  0x0a000000
#define LOP_OR_INVERTED					  0x0b000000
#define LOP_COPY						  0x0c000000
#define LOP_OR_REVERSE					  0x0d000000
#define LOP_OR							  0x0e000000
#define LOP_SET							  0x0f000000

/* Get lock before calling this */
#define mWait3DCmdQueue(wLen)\
do{\
  while ( *(hwcx->CurrentQueueLenPtr) < (int)(wLen))\
    {\
      *(hwcx->CurrentQueueLenPtr) = \
      (int)(*(DWORD *)(GET_IOBase(hwcx) + REG_QUELEN) & MASK_QUELEN) \
      - (int)20; \
    }\
  *(hwcx->CurrentQueueLenPtr) -= (int)(wLen);\
}while(0)

#if 0
#define mWait3DCmdQueue(wLen) do{}while(0);
#endif

#define GFLAG_ENABLESETTING               0x00000001
#define GFLAG_ENABLESETTING2              0x00000002
#define GFLAG_ZSETTING                    0x00000004
#define GFLAG_ALPHASETTING                0x00000008
#define GFLAG_DESTSETTING                 0x00000010
#define GFLAG_LINESETTING                 0x00000020
#define GFLAG_STENCILSETTING              0x00000040
#define GFLAG_FOGSETTING                  0x00000080
#define GFLAG_DSTBLEND                    0x00000100
#define GFLAG_CLIPPING                    0x00000200
#define CFLAG_TEXTURERESET 		  0x00000400
#define GFLAG_TEXTUREMIPMAP               0x00000800
#define GFLAG_TEXBORDERCOLOR              0x00001000
#define GFLAG_TEXTUREADDRESS              0x00002000
#define GFLAG_TEXTUREENV                  0x00004000
#define CFLAG_TEXTURERESET_1 		  0x00008000
#define GFLAG_TEXTUREMIPMAP_1             0x00010000
#define GFLAG_TEXBORDERCOLOR_1            0x00020000
#define GFLAG_TEXTUREADDRESS_1            0x00040000
#define GFLAG_TEXTUREENV_1                0x00080000

#define GFLAG_TEXTURE_STATES (CFLAG_TEXTURERESET | GFLAG_TEXTUREMIPMAP | \
			      GFLAG_TEXBORDERCOLOR | GFLAG_TEXTUREADDRESS | \
			      CFLAG_TEXTURERESET_1 | GFLAG_TEXTUREMIPMAP_1 | \
			      GFLAG_TEXBORDERCOLOR_1 | \
			      GFLAG_TEXTUREADDRESS_1 | \
			      GFLAG_TEXTUREENV | GFLAG_TEXTUREENV_1)


#define GFLAG_RENDER_STATES  (GFLAG_ENABLESETTING | GFLAG_ENABLESETTING2 | \
			      GFLAG_ZSETTING | GFLAG_ALPHASETTING | \
			      GFLAG_DESTSETTING | GFLAG_FOGSETTING | \
			      GFLAG_STENCILSETTING | GFLAG_DSTBLEND | \
			      GFLAG_CLIPPING)



#define  Index_SR_Ext_BIOS                     0x10
#define REVISION_6205BASE   3
#define SiS6326             (REVISION_6205BASE+20)	/* 6326 A/B/C0 */
#define SiS6326C            (REVISION_6205BASE+21)	/* 6326 C1 */
#define SiS6326C5           (REVISION_6205BASE+22)	/* 6326 C5 */
#define SiS6326G            (REVISION_6205BASE+25)	/* 6326 C1 */
#define SiS6326D0           (REVISION_6205BASE+30)	/* 6326 D0 */
#define SiS6326D1           (REVISION_6205BASE+31)	/* 6326 D1 */
#define SiS6326D2           (REVISION_6205BASE+32)	/* 6326 D2 */
#define SiS6326H0           (REVISION_6205BASE+35)	/* 6326 H0 */
#define SiS6215A            (REVISION_6205BASE+1)	/* 6205 B2, Video only version */
#define SiS6215B            (REVISION_6205BASE+2)	/* 6205 D3, Video only version */
#define SiS6215C            (REVISION_6205BASE+3)	/* 6205 B2, Video XOR version */
#define SiS6205B            (REVISION_6205BASE+4)	/* 6205 B2 */
#define SiS6205D            (REVISION_6205BASE+5)	/* 6205 D3 */
#define SiS5597             (REVISION_6205BASE+7)	/* Jedi, 5597, 5598 */

#define REVISION_6205       {{0x6326, 0xAF, 0, SiS6326  },\
                             {0x6326, 0xC1, 0, SiS6326C },\
                             {0x6326, 0x92, 0, SiS6326D2},\
                             {0x6326, 0x92, 1, SiS6326D2},\
                             {0x6326, 0x0A, 0, SiS6326G },\
                             {0x6326, 0xD0, 0, SiS6326D0},\
                             {0x6326, 0xD1, 0, SiS6326D1},\
                             {0x6326, 0xD2, 0, SiS6326D2},\
                             {0x6326, 0xD2, 1, SiS6326D2},\
                             {0x6326, 0x0B, 0, SiS6326H0},\
                             {0x6326, 0x0B, 1, SiS6326H0},\
                             {0x0200, 0x6F, 0, SiS5597  },\
                             {0x0205, 0x6F, 0, SiS5597  },\
                             {0x0205, 0x44, 1, SiS6215A },\
                             {0x0205, 0xD3, 1, SiS6215B },\
                             {0x0204, 0x2F, 1, SiS6215C },\
                             {0x0205, 0x44, 0, SiS6205B },\
                             {0x0205, 0xD3, 0, SiS6205D }}

#define  REG_QUELEN           0x8240	/* Byte for 201C */
#define  MASK_QUELEN          0xffff

#endif
