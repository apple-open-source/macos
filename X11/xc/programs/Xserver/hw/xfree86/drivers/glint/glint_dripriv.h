/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/glint/glint_dripriv.h,v 1.6 2002/10/30 12:52:16 alanh Exp $ */
/**************************************************************************

Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
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

/*
 * Author:
 *   Jens Owen <jens@tungstengraphics.com>
 *
 */

extern void GlxSetVisualConfigs(
    int nconfigs,
    __GLXvisualConfig *configs,
    void **configprivs
);

extern Bool GLINTCreateContext(ScreenPtr pScreen,
                               VisualPtr visual,
                               drmContext hwContext,
                               void* pVisualConfigPriv,
			       DRIContextType contextStore);

extern void GLINTDRISwapContext( ScreenPtr pScreen,
				 DRISyncType syncType,
				 DRIContextType readContextType,
				 void* readContextStore,
				 DRIContextType writeContextType,
				 void* writeContextStore);

/* Macros to Setup Generic Kernel Device Driver to Handle DMA for gamma */

/* WARNING!!! MAGIC NUMBER!!!  The number of regions already added to the
   kernel must be specified here.  Currently, the number is 2. */
#define DRM_REG(reg)                                               \
        (2                                                         \
	 + ((reg < 0x1000)                                         \
	    ? 0                                                    \
	    : ((reg < 0x10000) ? 1 : ((reg < 0x11000) ? 2 : 3))))

#define DRM_OFF(reg)                                               \
        ((reg < 0x1000)                                            \
	 ? reg                                                     \
	 : ((reg < 0x10000)                                        \
	    ? (reg - 0x1000)                                       \
	    : ((reg < 0x11000)                                     \
	       ? (reg - 0x10000)                                   \
	       : (reg - 0x11000))))


#define DRM_I_WRITE_R(reg,a,b,c,d) \
        DRM_I_WRITE(DRM_REG(reg), DRM_OFF(reg), a, b, c, d)

#define DRM_I_WRITE_IMM_R(reg,a) \
        DRM_I_WRITE_IMM(DRM_REG(reg), DRM_OFF(reg), a)

#define DRM_I_READ_R(reg) \
        DRM_I_READ(DRM_REG(reg), DRM_OFF(reg))

#define DRM_I_WHILE_IMM_R(reg,a,b) \
        DRM_I_WHILE_IMM(DRM_REG(reg), DRM_OFF(reg), a, b)

#define DRM_I_IF_IMM_R(reg,a,b,c) \
        DRM_I_IF_IMM(DRM_REG(reg), DRM_OFF(reg), a, b, c)

#define GLINTSync    GlintSync
#define GLINTSyncTag 0x188

#define GLINT_MAX_DRAWABLES 		15
#define GLINT_GAMMA_CONTEXT_SIZE	964
#define GLINT_GAMMA_CONTEXT_MASK	0x7ff

typedef struct {
    int		index;
} GLINTConfigPrivRec, *GLINTConfigPrivPtr;

typedef struct {

    /* 2D components */
    CARD32	CStartXDom;
    CARD32	CdXDom;
    CARD32	CStartXSub;
    CARD32	CdXSub;
    CARD32	CStartY;
    CARD32	CdY;
    CARD32	CGLINTCount;
    CARD32	CPointTable0;
    CARD32	CPointTable1;
    CARD32	CPointTable2;
    CARD32	CPointTable3;
    CARD32	CRasterizerMode;
    CARD32	CYLimits;
    CARD32	CScanLineOwnership;
    CARD32	CPixelSize;
    CARD32	CScissorMode;
    CARD32	CScissorMinXY;
    CARD32	CScissorMaxXY;
    CARD32	CScreenSize;
    CARD32	CAreaStippleMode;
    CARD32	CLineStippleMode;
    CARD32	CLoadLineStippleCounters;
    CARD32	CWindowOrigin;
    CARD32	CRouterMode;
    CARD32	CTextureAddressMode;
    CARD32	CTextureReadMode;
    CARD32	CTextureColorMode;
    CARD32	CFogMode;
    CARD32	CColorDDAMode;
    CARD32	CGLINTColor;
    CARD32	CAlphaTestMode;
    CARD32	CAntialiasMode;
    CARD32	CAlphaBlendMode;
    CARD32	CDitherMode;
    CARD32	CFBSoftwareWriteMask;
    CARD32	CLogicalOpMode;
    CARD32	CFBWriteData;
    CARD32	CLBReadMode;
    CARD32	CLBSourceOffset;
    CARD32	CLBWindowBase;
    CARD32	CLBWriteMode;
    CARD32	CTextureDownloadOffset;
    CARD32	CLBWindowOffset;
    CARD32	CGLINTWindow;
    CARD32	CStencilMode;
    CARD32	CDepthMode;
    CARD32	CGLINTDepth;
    CARD32	CFBReadMode;
    CARD32	CFBSourceOffset;
    CARD32	CFBPixelOffset;
    CARD32	CFBWindowBase;
    CARD32	CFBWriteMode;
    CARD32	CFBHardwareWriteMask;
    CARD32	CFBBlockColor;
    CARD32	CPatternRamMode;
    CARD32	CFBBlockColorU;
    CARD32	CFBBlockColorL;
    CARD32	CFilterMode;
    CARD32	CStatisticMode;
    CARD32	CBroadcastMask;

    /* 3D components */
    CARD32	CSStart;
    CARD32	CdSdx;
    CARD32	CdSdyDom;
    CARD32	CTStart;
    CARD32	CdTdx;
    CARD32	CdTdyDom;
    CARD32	CQStart;
    CARD32	CdQdx;
    CARD32	CdQdyDom;
    CARD32	CLOD;
    CARD32	CdSdy;
    CARD32	CdTdy;
    CARD32	CdQdy;
    CARD32	CTex;
    CARD32	CTextureFormat;
    CARD32	CTextureCacheControl;
    CARD32	CGLINTBorderColor;
    CARD32	CTexelLUTIndex;
    CARD32	CTexelLUTData;
    CARD32	CTexelLUTAddress;
    CARD32	CTexelLUTTransfer;
    CARD32	CTextureFilterMode;
    CARD32	CTextureChromaUpper;
    CARD32	CTextureChromaLower;
    CARD32	CTxBaseAddr0;
    CARD32	CTxBaseAddr1;
    CARD32	CTxBaseAddr2;
    CARD32	CTxBaseAddr3;
    CARD32	CTxBaseAddr4;
    CARD32	CTxBaseAddr5;
    CARD32	CTxBaseAddr6;
    CARD32	CTxBaseAddr7;
    CARD32	CTxBaseAddr8;
    CARD32	CTxBaseAddr9;
    CARD32	CTxBaseAddr10;
    CARD32	CTxBaseAddr11;
    CARD32	CTxBaseAddr12;
    CARD32	CTexelLUT0;
    CARD32	CTexelLUT1;
    CARD32	CTexelLUT2;
    CARD32	CTexelLUT3;
    CARD32	CTexelLUT4;
    CARD32	CTexelLUT5;
    CARD32	CTexelLUT6;
    CARD32	CTexelLUT7;
    CARD32	CTexelLUT8;
    CARD32	CTexelLUT9;
    CARD32	CTexelLUT10;
    CARD32	CTexelLUT11;
    CARD32	CTexelLUT12;
    CARD32	CTexelLUT13;
    CARD32	CTexelLUT14;
    CARD32	CTexelLUT15;
    CARD32	CTexel0;
    CARD32	CTexel1;
    CARD32	CTexel2;
    CARD32	CTexel3;
    CARD32	CTexel4;
    CARD32	CTexel5;
    CARD32	CTexel6;
    CARD32	CTexel7;
    CARD32	CInterp0;
    CARD32	CInterp1;
    CARD32	CInterp2;
    CARD32	CInterp3;
    CARD32	CInterp4;
    CARD32	CTextureFilter;
    CARD32	CTextureEnvColor;
    CARD32	CFogColor;
    CARD32	CFStart;
    CARD32	CdFdx;
    CARD32	CdFdyDom;
    CARD32	CKsStart;
    CARD32	CdKsdx;
    CARD32	CdKsdyDom;
    CARD32	CKdStart;
    CARD32	CdKdStart;
    CARD32	CdKddyDom;
    CARD32	CRStart;
    CARD32	CdRdx;
    CARD32	CdRdyDom;
    CARD32	CGStart;
    CARD32	CdGdx;
    CARD32	CdGdyDom;
    CARD32	CBStart;
    CARD32	CdBdx;
    CARD32	CdBdyDom;
    CARD32	CAStart;
    CARD32	CdAdx;
    CARD32	CdAdyDom;
    CARD32	CConstantColor;
    CARD32	CChromaUpper;
    CARD32	CChromaLower;
    CARD32	CChromaTestMode;
    CARD32	CStencilData;
    CARD32	CGLINTStencil;
    CARD32	CZStartU;
    CARD32	CZStartL;
    CARD32	CdZdxU;
    CARD32	CdZdxL;
    CARD32	CdZdyDomU;
    CARD32	CdZdyDomL;
    CARD32	CFastClearDepth;
    CARD32	CMinRegion;
    CARD32	CMaxRegion;
    CARD32	CKsRStart;
    CARD32	CdKsRdx;
    CARD32	CdKsRdyDom;
    CARD32	CKsGStart;
    CARD32	CdKsGdx;
    CARD32	CdKsGdyDom;
    CARD32	CKsBStart;
    CARD32	CdKsBdx;
    CARD32	CdKsBdyDom;
    CARD32	CKdRStart;
    CARD32	CdKdRdx;
    CARD32	CdKdRdyDom;
    CARD32	CKdGStart;
    CARD32	CdKdGdx;
    CARD32	CdKdGdyDom;
    CARD32	CKdBStart;
    CARD32	CdKdBdx;
    CARD32	CdKdBdyDom;

} GLINTMXRec;

typedef struct {
    GLINTMXRec	MX1;
    GLINTMXRec	MX2;
    CARD32      Gamma[GLINT_GAMMA_CONTEXT_SIZE];
} GLINTDRIContextRec, *GLINTDRIContextPtr;
