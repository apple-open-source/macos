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
/* $XFree86: xc/lib/GL/mesa/src/drv/sis/sis_common.h,v 1.5 2000/09/26 15:56:48 tsi Exp $ */

/*
 * Authors:
 *    Sung-Ching Lin <sclin@sis.com.tw>
 *
 */

#ifndef _sis_common_h_
#define _sis_common_h_

#include "types.h"
#include "sis_xmesaP.h"

#if 0
#define free(x)
#define calloc(x,y) sis_debug_malloc((x)*(y))
extern void *sis_debug_malloc(int x);
#endif

#if defined(SIS_DUMP)
#include "sis_debug.h"
#endif

#if SIS_STEREO
# include "sis_stereo.h"
#else
# define STEREO_OFFSET(v) 0
# define STEREO_SAMPLE(v) do{}while(0)
#endif

#ifdef XFree86Server
# include "resource.h"
# include "windowstr.h"
# include "gcstruct.h"
# include "GL/xf86glx.h"
# include "xf86glx_util.h"
# include "xf86_ansic.h"
# include "xf86_libc.h"
#else
# ifdef GLX_DIRECT_RENDERING
#  include <stdio.h>
#  include <unistd.h>
#  include <stdlib.h>
#  include <string.h>
#  include "dri_mesaint.h"
typedef struct _Box
{
  short x1, y1, x2, y2;
}
BoxRec;
#define NullBox ((BoxPtr)0)
typedef struct _Box *BoxPtr;
# endif
#endif

typedef unsigned short WORD;
typedef unsigned long DWORD;
typedef unsigned int UINT;
typedef int INT;
typedef long LONG;
typedef DWORD *LPDWORD;

/* BitBlt Commands */
#define  Index_SR_Misc_Ctrl11    0x3e
#define CMD0_DD_ENABLE      0x06
#define CMD0_SRC_VIDEO      0x00
#define CMD0_SRC_CPU        0x10
#define CMD0_PAT_FG_COLOR   0x00
#define CMD1_DIR_X_DEC      0x00
#define CMD1_DIR_X_INC      0x01
#define CMD1_DIR_Y_DEC      0x00
#define CMD1_DIR_Y_INC      0x02
#define REG_SRC_ADDR        0x8200
#define REG_CMD0            0x823c

typedef struct
{
  WORD wSrcPitch;
  WORD wDestPitch;
}
_PITCH;
typedef struct
{
  WORD wWidth;
  WORD wHeight;
}
_DIM;
typedef struct
{
  WORD wY;
  WORD wX;
}
_POS;

typedef struct
{
  BYTE cCmd0;
  BYTE cRop;
  BYTE cCmd1;
  BYTE cReserved;
}
_CMD;

typedef struct
{
  WORD wStatus0;
  BYTE cStatus0_BYTE3;
  BYTE cStatus0_BYTE4;
}
_CMDQUESTATUS;

typedef struct
{
  DWORD dwSrcBaseAddr;
  DWORD dwSrcPitch;
  _POS stdwSrcPos;
  _POS stdwDestPos;
  DWORD dwDestBaseAddr;
  WORD wDestPitch;
  WORD wDestHeight;
  _DIM stdwDim;
  DWORD dwFgRopColor;
  DWORD dwBgRopColor;
  DWORD dwSrcHiCKey;
  DWORD dwSrcLoCKey;
  DWORD dwMaskA;
  DWORD dwMaskB;
  DWORD dwClipA;
  DWORD dwClipB;
  _CMD stdwCmd;
  _CMDQUESTATUS stdwCmdQueStatus;
}
ENGPACKET, *LPENGPACKET;

/* Hardware Info */
#include "sis_reg.h"
#include "sis_init.h"

typedef struct gl_texture_object GLtextureObject;
typedef struct gl_texture_image GLtextureImage;

#define VIDEO_TYPE 0
#define AGP_TYPE 1

typedef struct sis_texure_area
{
  GLbyte *Data;
  GLenum Format;
  void *free;
  GLuint memType;
  GLuint Pitch;
  GLuint Size;
  GLuint texelSize;
  unsigned int hHWContext;

  /* Debug */
  GLuint realSize;
}
SIStextureArea;

/* dirtyFlag */
#define SIS_TEX_ENV 0x1
#define SIS_TEX_IMAGE 0x2	/* image in video memory is stale */
#define SIS_TEX_PARAMETER 0x4
#define SIS_TEX_ALL (SIS_TEX_IMAGE | SIS_TEX_PARAMETER)

typedef struct sis_texobj_area
{
  DWORD dirtyFlag;
  GLboolean valid;
  struct sis_texobj_area *prev, *next;
}
sisTexobjInfo;

typedef struct sis_buffer_private
{
  void *zbFree, *bbFree;

  ENGPACKET *pZClearPacket, *pCbClearPacket;

  ENGPACKET zClearPacket, cbClearPacket;
  
#if SIS_STEREO 
  XMesaImage *pStereoImages[3];
  ENGPACKET *pStereoPackets[3];
  void *stereoFrees[3];  /* stereoFrees[0] is useless */
  
  ENGPACKET stereoPackets[2];
#endif
}
sisBufferInfo;

/* HW capability */
#define SIS_MAX_MIPMAP_LEVEL 11
#define SIS_MAX_TEXTURE_SIZE 2048
#define SIS_MAX_TEXTURES 2

#define SIS_MAX_FRAME_LENGTH 3

DWORD doFPtoFixedNoRound (DWORD dwInValue, int nFraction);

#define Y_FLIP(Y)  (xmesa->xm_buffer->bottom-(Y))
#endif
