/* $XFree86: xc/programs/Xserver/hw/xfree86/xf24_32bpp/cfbcpyarea.c,v 1.5 1999/05/16 10:13:05 dawes Exp $ */

#include "X.h"
#include "Xmd.h"
#include "servermd.h"
#include "scrnintstr.h"
#include "pixmapstr.h"
#include "resource.h"
#include "colormap.h"
#include "colormapst.h"
#define PSZ 8
#include "cfb.h"
#undef PSZ
#include "cfb24.h"
#include "cfb32.h"
#include "cfb24_32.h"
#include "mi.h"
#include "mistruct.h"
#include "dix.h"
#include "mibstore.h"



RegionPtr
cfb24_32CopyArea(
    DrawablePtr pSrcDraw,
    DrawablePtr pDstDraw,
    GC *pGC,
    int srcx, int srcy,
    int width, int height,
    int dstx, int dsty 
){

   if(pSrcDraw->bitsPerPixel == 32) {
	if(pDstDraw->bitsPerPixel == 32) {
	    return(cfb32CopyArea(pSrcDraw, pDstDraw, pGC, srcx, srcy,
		    			width, height, dstx, dsty));
	} else {
	    /* have to translate 32 -> 24 copies */
	    return cfb32BitBlt (pSrcDraw, pDstDraw,
            		pGC, srcx, srcy, width, height, dstx, dsty, 
			cfbDoBitblt32To24, 0L);
	}
   } else {
	if(pDstDraw->bitsPerPixel == 32) {
	    /* have to translate 24 -> 32 copies */
	    return cfb32BitBlt (pSrcDraw, pDstDraw,
            		pGC, srcx, srcy, width, height, dstx, dsty, 
			cfbDoBitblt24To32, 0L);
	} else if((pDstDraw->type == DRAWABLE_WINDOW) &&
		(pSrcDraw->type == DRAWABLE_WINDOW) && (pGC->alu == GXcopy) &&
		((pGC->planemask & 0x00ffffff) == 0x00ffffff)) {

                return cfb24BitBlt (pSrcDraw, pDstDraw,
                        pGC, srcx, srcy, width, height, dstx, dsty, 
                        cfb24_32DoBitblt24To24GXcopy, 0L);
	} else {
	    return(cfb24CopyArea(pSrcDraw, pDstDraw, pGC, srcx, srcy,
		    width, height, dstx, dsty));
	}
   }
}



void 
cfbDoBitblt24To32(
    DrawablePtr pSrc, 
    DrawablePtr pDst, 
    int rop,
    RegionPtr prgnDst, 
    DDXPointPtr pptSrc,
    unsigned long planemask,
    unsigned long bitPlane
){
    BoxPtr pbox = REGION_RECTS(prgnDst);
    int nbox = REGION_NUM_RECTS(prgnDst);
    unsigned char *data24, *data32;
    unsigned char *ptr24, *ptr32;
    int pitch24, pitch32;
    int height, width, i, j;
    unsigned char *pm;

    cfbGetByteWidthAndPointer(pSrc, pitch24, ptr24);
    cfbGetByteWidthAndPointer(pDst, pitch32, ptr32);

    planemask &= 0x00ffffff;
    pm = (unsigned char*)(&planemask);

    if((planemask == 0x00ffffff) && (rop == GXcopy)) {
	CARD32 *dst, *src;
	CARD32 tmp, phase;

	for(;nbox; pbox++, pptSrc++, nbox--) {
            data24 = ptr24 + (pptSrc->y * pitch24) + (pptSrc->x * 3);
            data32 = ptr32 + (pbox->y1 * pitch32) + (pbox->x1 << 2);
	    width = pbox->x2 - pbox->x1;
	    height = pbox->y2 - pbox->y1;

	    phase = (long)data24 & 3L;
	    data24 = (unsigned char*)((long)data24 & ~3L);

	    while(height--) {
		src = (CARD32*)data24;
		dst = (CARD32*)data32;
		i = width;

		switch(phase) {
		case 0: break;
		case 1:
		    dst[0] = src[0] >> 8;
		    dst++;
		    src++;
		    i--;
		    break;
		case 2:
		    dst[0] = src[0] >> 16;
		    tmp = src[1];
		    dst[0] |= tmp << 16;
		    if(!(--i)) break;
		    dst[1] = tmp >> 8;
		    dst += 2;
		    src += 2;
		    i--;
		    break;
		default:
		    dst[0] = src[0] >> 24;
		    tmp = src[1];
		    dst[0] |= tmp << 8;
		    if(!(--i)) break;
		    dst[1] = tmp >> 16;
		    tmp = src[2];
		    dst[1] |= tmp << 16;
		    if(!(--i)) break;
		    dst[2] = tmp >> 8;
		    dst += 3;
		    src += 3;
		    i--;
		    break;
		}

		while(i >= 4) {
		    dst[0] = src[0];
		    tmp = src[1];
		    dst[3] = src[2];
		    dst[1] = (dst[0] >> 24) | (tmp << 8);
		    dst[2] = (tmp >> 16) | (dst[3] << 16);
		    dst[3] >>= 8;

		    src += 3;
		    dst += 4;
		    i -= 4;
		}

		switch(i) {
		case 0: break;
		case 1:
		    dst[0] = src[0];
		    break;
		case 2:
		    dst[0] = src[0];
		    dst[1] = (dst[0] >> 24) | (src[1] << 8);
		    break;
		default:
		    dst[0] = src[0];
		    tmp = src[1];
		    dst[2] = src[2] << 16;
		    dst[1] = (dst[0] >> 24) | (tmp << 8);
		    dst[2] |= tmp >> 16;
		    break;
		}

		data24 += pitch24;
		data32 += pitch32;
	    }
	}
    } else {  /* it ain't pretty, but hey */
	for(;nbox; pbox++, pptSrc++, nbox--) {
	    data24 = ptr24 + (pptSrc->y * pitch24) + (pptSrc->x * 3);
	    data32 = ptr32 + (pbox->y1 * pitch32) + (pbox->x1 << 2);
	    width = (pbox->x2 - pbox->x1) << 2;
	    height = pbox->y2 - pbox->y1;

	    while(height--) {	
		switch(rop) {
		case GXcopy:
		    for(i = j = 0; i < width; i += 4, j += 3) {
			data32[i] = (data24[j] & pm[0]) | (data32[i] & ~pm[0]);
			data32[i+1] = (data24[j+1] & pm[1]) | 
					(data32[i+1] & ~pm[1]);
			data32[i+2] = (data24[j+2] & pm[2]) | 
					(data32[i+2] & ~pm[2]);
		    }
		    break;
		case GXor:
		    for(i = j = 0; i < width; i += 4, j += 3) {
			data32[i] |= data24[j] & pm[0];
			data32[i+1] |= data24[j+1] & pm[1];
			data32[i+2] |= data24[j+2] & pm[2];
		    }
		    break;
		case GXclear:
		    for(i = 0; i < width; i += 4) {
			data32[i] &= ~pm[0];
			data32[i+1] &= ~pm[1];
			data32[i+2] &= ~pm[2];
		    }
		    break;
		case GXand:
		    for(i = j = 0; i < width; i += 4, j += 3) {
			data32[i] &= data24[j] | ~pm[0];
			data32[i+1] &= data24[j+1] | ~pm[1];
			data32[i+2] &= data24[j+2] | ~pm[2];
		    }
		    break;
		case GXandReverse:
		    for(i = j = 0; i < width; i += 4, j += 3) {
			data32[i] = ~data32[i] & (data24[j] | ~pm[0]);
			data32[i+1] = ~data32[i+1] & (data24[j+1] | ~pm[1]);
			data32[i+2] = ~data32[i+2] & (data24[j+2] | ~pm[2]);
		    }
		    break;
		case GXandInverted:
		    for(i = j = 0; i < width; i += 4, j += 3) {
			data32[i] &= ~(data24[j] & pm[0]);
			data32[i+1] &= ~(data24[j+1] & pm[1]);
			data32[i+2] &= ~(data24[j+2] & pm[2]);
		    }
		    break;
		case GXnoop:
		    return;
		case GXxor:
		    for(i = j = 0; i < width; i += 4, j += 3) {
			data32[i] ^= data24[j] & pm[0];
			data32[i+1] ^= data24[j+1] & pm[1];
			data32[i+2] ^= data24[j+2] & pm[2];
		    }
		    break;
		case GXnor:
		    for(i = j = 0; i < width; i += 4, j += 3) {
			data32[i] = ~(data32[i] | (data24[j] & pm[0]));
			data32[i+1] = ~(data32[i+1] | (data24[j+1] & pm[1]));
			data32[i+2] = ~(data32[i+2] | (data24[j+2] & pm[2]));
		    }
		    break;
		case GXequiv:
		    for(i = j = 0; i < width; i += 4, j += 3) {
			data32[i] = ~(data32[i] ^ (data24[j] & pm[0]));
			data32[i+1] = ~(data32[i+1] ^ (data24[j+1] & pm[1]));
			data32[i+2] = ~(data32[i+2] ^ (data24[j+2] & pm[2]));
		    }
		    break;
		case GXinvert:
		    for(i = 0; i < width; i += 4) {
			data32[i] ^= pm[0];
			data32[i+1] ^= pm[1];
			data32[i+2] ^= pm[2];
		    }
		    break;
		case GXorReverse:
		    for(i = j = 0; i < width; i += 4, j += 3) {
			data32[i] = ~data32[i] | (data24[j] & pm[0]);
			data32[i+1] = ~data32[i+1] | (data24[j+1] & pm[1]);
			data32[i+2] = ~data32[i+2] | (data24[j+2] & pm[2]);
		    }
		    break;
		case GXcopyInverted:
		    for(i = j = 0; i < width; i += 4, j += 3) {
			data32[i] = (~data24[j] & pm[0]) | (data32[i] & ~pm[0]);
			data32[i+1] = (~data24[j+1] & pm[1]) | 
						(data32[i+1] & ~pm[1]);
			data32[i+2] = (~data24[j+2] & pm[2]) | 
						(data32[i+2] & ~pm[2]);
		    }
		    break;
		case GXorInverted:
		    for(i = j = 0; i < width; i += 4, j += 3) {
			data32[i] |= ~data24[j] & pm[0];
			data32[i+1] |= ~data24[j+1] & pm[1];
			data32[i+2] |= ~data24[j+2] & pm[2];
		    }
		    break;
		case GXnand:
		    for(i = j = 0; i < width; i += 4, j += 3) {
			data32[i] = ~(data32[i] & (data24[j] | ~pm[0]));
			data32[i+1] = ~(data32[i+1] & (data24[j+1] | ~pm[1]));
			data32[i+2] = ~(data32[i+2] & (data24[j+2] | ~pm[2]));
		    }
		    break;
		case GXset:
		    for(i = 0; i < width; i+=4) {
			data32[i] |= pm[0];
			data32[i+1] |= pm[1];
			data32[i+2] |= pm[2];
		    }
		    break;
		}
	        data24 += pitch24;
	        data32 += pitch32;
	    }
	}
    }
}


void 
cfbDoBitblt32To24(
    DrawablePtr pSrc, 
    DrawablePtr pDst, 
    int rop,
    RegionPtr prgnDst, 
    DDXPointPtr pptSrc,
    unsigned long planemask,
    unsigned long bitPlane
){
    BoxPtr pbox = REGION_RECTS(prgnDst);
    int nbox = REGION_NUM_RECTS(prgnDst);
    unsigned char *ptr24, *ptr32;
    unsigned char *data24, *data32;
    int pitch24, pitch32;
    int height, width, i, j;
    unsigned char *pm;

    cfbGetByteWidthAndPointer(pDst, pitch24, ptr24);
    cfbGetByteWidthAndPointer(pSrc, pitch32, ptr32);

    planemask &= 0x00ffffff;
    pm = (unsigned char*)(&planemask);

    if((planemask == 0x00ffffff) && (rop == GXcopy)) {
	CARD32 *src;
	CARD8 *dst;
	long phase;
	for(;nbox; pbox++, pptSrc++, nbox--) {
	    data24 = ptr24 + (pbox->y1 * pitch24) + (pbox->x1 * 3);
	    data32 = ptr32 + (pptSrc->y * pitch32) + (pptSrc->x << 2);
	    width = pbox->x2 - pbox->x1;
	    height = pbox->y2 - pbox->y1;
	    
	    phase = (long)data24 & 3L;

	    while(height--) {
		src = (CARD32*)data32;
		dst = data24;
		j = width;

		switch(phase) {
		case 0: break;
		case 1:
		    dst[0] = src[0];
		    *((CARD16*)(dst + 1)) = src[0] >> 8;
		    dst += 3;
		    src++;
		    j--;
		    break;
		case 2:
		    if(j == 1) break;
		    *((CARD16*)dst) = src[0];
		    *((CARD32*)(dst + 2)) = ((src[0] >> 16) & 0x000000ff) | 
							(src[1] << 8);
		    dst += 6;
		    src += 2;
		    j -= 2;
		    break;
		default:
		    if(j < 3) break; 
		    dst[0] = src[0];
		    *((CARD32*)(dst + 1)) = ((src[0] >> 8) & 0x0000ffff) | 
							(src[1] << 16);
		    *((CARD32*)(dst + 5)) = ((src[1] >> 16) & 0x000000ff) | 
							(src[2] << 8);
		    dst += 9;
		    src += 3;
		    j -= 3;
		}

		while(j >= 4) {
		    *((CARD32*)dst) = (src[0] & 0x00ffffff) | (src[1] << 24);
		    *((CARD32*)(dst + 4)) = ((src[1] >> 8) & 0x0000ffff)| 
							(src[2] << 16);
		    *((CARD32*)(dst + 8)) = ((src[2] >> 16) & 0x000000ff) |
		 					(src[3] << 8);
		    dst += 12;
		    src += 4;
		    j -= 4;
		}
		switch(j) {
		case 0:	
		    break;
		case 1:	
		    *((CARD16*)dst) = src[0];
		    dst[2] = src[0] >> 16;
		    break;
		case 2:	
		    *((CARD32*)dst) = (src[0] & 0x00ffffff) | (src[1] << 24);
		    *((CARD16*)(dst + 4)) = src[1] >> 8;
		    break;
		default: 
		    *((CARD32*)dst) = (src[0] & 0x00ffffff) | (src[1] << 24);
		    *((CARD32*)(dst + 4)) = ((src[1] >> 8) & 0x0000ffff) | 
							(src[2] << 16);
		    dst[8] = src[2] >> 16;
		    break;
		}

		data24 += pitch24;
		data32 += pitch32;
	    }
	}
    } else {
	for(;nbox; pbox++, pptSrc++, nbox--) {
	    data24 = ptr24 + (pbox->y1 * pitch24) + (pbox->x1 * 3);
	    data32 = ptr32 + (pptSrc->y * pitch32) + (pptSrc->x << 2);
	    width = pbox->x2 - pbox->x1;
	    height = pbox->y2 - pbox->y1;

	    while(height--) {	
		switch(rop) {
		case GXcopy:
		    for(i = j = 0; j < width; i += 4, j += 3) {
			data24[j] = (data32[i] & pm[0]) | (data24[j] & ~pm[0]);
			data24[j+1] = (data32[i+1] & pm[1]) | 
						(data24[j+1] & ~pm[1]);
			data24[j+2] = (data32[i+2] & pm[2]) | 
						(data24[j+2] & ~pm[2]);
		    }
		    break;
		case GXor:
		    for(i = j = 0; j < width; i += 4, j += 3) {
			data24[j] |= data32[i] & pm[0];
			data24[j+1] |= data32[i+1] & pm[1];
			data24[j+2] |= data32[i+2] & pm[2];
		    }
		    break;
		case GXclear:
		    for(j = 0; j < width; j += 3) {
			data24[j] &= ~pm[0];
			data24[j+1] &= ~pm[1];
			data24[j+2] &= ~pm[2];
		    }
		    break;
		case GXand:
		    for(i = j = 0; j < width; i += 4, j += 3) {
			data24[j] &= data32[i] | ~pm[0];
			data24[j+1] &= data32[i+1] | ~pm[1];
			data24[j+2] &= data32[i+2] | ~pm[2];
		    }
		    break;
		case GXandReverse:
		    for(i = j = 0; j < width; i += 4, j += 3) {
			data24[j] = ~data24[j] & (data32[i] | ~pm[0]);
			data24[j+1] = ~data24[j+1] & (data32[i+1] | ~pm[1]);
			data24[j+2] = ~data24[j+2] & (data32[i+2] | ~pm[2]);
		    }
		    break;
		case GXandInverted:
		    for(i = j = 0; j < width; i += 4, j += 3) {
			data24[j] &= ~(data32[i] & pm[0]);
			data24[j+1] &= ~(data32[i+1] & pm[1]);
			data24[j+2] &= ~(data32[i+2] & pm[2]);
		    }
		    break;
		case GXnoop:
		    return;
		case GXxor:
		    for(i = j = 0; j < width; i += 4, j += 3) {
			data24[j] ^= data32[i] & pm[0];
			data24[j+1] ^= data32[i+1] & pm[1];
			data24[j+2] ^= data32[i+2] & pm[2];
		    }
		    break;
		case GXnor:
		    for(i = j = 0; j < width; i += 4, j += 3) {
			data24[j] = ~(data24[j] | (data32[i] & pm[0]));
			data24[j+1] = ~(data24[j+1] | (data32[i+1] & pm[1]));
			data24[j+2] = ~(data24[j+2] | (data32[i+2] & pm[2]));
		    }
		    break;
		case GXequiv:
		    for(i = j = 0; j < width; i += 4, j += 3) {
			data24[j] = ~(data24[j] ^ (data32[i] & pm[0]));
			data24[j+1] = ~(data24[j+1] ^ (data32[i+1] & pm[1]));
			data24[j+2] = ~(data24[j+2] ^ (data32[i+2] & pm[2]));
		    }
		    break;
		case GXinvert:
		    for(j = 0; j < width; j+=3) {
			data24[j] ^= pm[0];
			data24[j+1] ^= pm[1];
			data24[j+2] ^= pm[2];
		    }
		    break;
		case GXorReverse:
		    for(i = j = 0; j < width; i += 4, j += 3) {
			data24[j] = ~data24[j] | (data32[i] & pm[0]);
			data24[j+1] = ~data24[j+1] | (data32[i+1] & pm[1]);
			data24[j+2] = ~data24[j+2] | (data32[i+2] & pm[2]);
		    }
		    break;
		case GXcopyInverted:
		    for(i = j = 0; j < width; i += 4, j += 3) {
			data24[j] = (~data32[i] & pm[0]) | (data24[j] & ~pm[0]);
			data24[j+1] = (~data32[i+1] & pm[1]) | 
						(data24[j+1] & ~pm[1]);
			data24[j+2] = (~data32[i+2] & pm[2]) | 
						(data24[j+2] & ~pm[2]);
		    }
		    break;
		case GXorInverted:
		    for(i = j = 0; j < width; i += 4, j += 3) {
			data24[j] |= ~data32[i] & pm[0];
			data24[j+1] |= ~data32[i+1] & pm[1];
			data24[j+2] |= ~data32[i+2] & pm[2];
		    }
		    break;
		case GXnand:
		    for(i = j = 0; j < width; i += 4, j += 3) {
			data24[j] = ~(data24[j] & (data32[i] | ~pm[0]));
			data24[j+1] = ~(data24[j+1] & (data32[i+1] | ~pm[1]));
			data24[j+2] = ~(data24[j+2] & (data32[i+2] | ~pm[2]));
		    }
		    break;
		case GXset:
		    for(j = 0; j < width; j+=3) {
			data24[j] |= pm[0];
			data24[j+1] |= pm[1];
			data24[j+2] |= pm[2];
		    }
		    break;
		}
	        data24 += pitch24;
	        data32 += pitch32;
	    }
	}
    }
}




static void 
Do24To24Blt(
   unsigned char *ptr,
   int pitch,
   int nbox,
   DDXPointPtr pptSrc,
   BoxPtr pbox,
   int xdir, int ydir
){
   int width, height, diff, phase;
   CARD8 *swap, *lineAddr;

   ydir *= pitch;

   swap = (CARD8*)ALLOCATE_LOCAL((2048 * 3) + 3);

   for(;nbox; pbox++, pptSrc++, nbox--) {
	lineAddr = ptr + (pptSrc->y * pitch) + (pptSrc->x * 3);
	diff = ((pbox->y1 - pptSrc->y) * pitch) + ((pbox->x1 - pptSrc->x) * 3);
	width = (pbox->x2 - pbox->x1) * 3;
	height = pbox->y2 - pbox->y1;

	if(ydir < 0) 
	    lineAddr += (height - 1) * pitch;

	phase = (long)lineAddr & 3L;

	while(height--) {
	    /* copy src onto the stack */
	    memcpy(swap, (CARD32*)((long)lineAddr & ~3L), 
				(width + phase + 3) & ~3L);

	    /* copy the stack to the dst */
	    memcpy(lineAddr + diff, swap + phase, width); 
	    lineAddr += ydir;
	}
    }

    DEALLOCATE_LOCAL(swap);
}


static void
cfb24_32DoBitBlt(
    DrawablePtr	    pSrc, 
    DrawablePtr	    pDst,
    RegionPtr	    prgnDst,
    DDXPointPtr	    pptSrc,
    void	    (*DoBlt)() 
){
    int nbox, careful, pitch;
    BoxPtr pbox, pboxTmp, pboxNext, pboxBase, pboxNew1, pboxNew2;
    DDXPointPtr pptTmp, pptNew1, pptNew2;
    int xdir, ydir;
    unsigned char *ptr;

    /* XXX we have to err on the side of safety when both are windows,
     * because we don't know if IncludeInferiors is being used.
     */
    careful = ((pSrc == pDst) ||
               ((pSrc->type == DRAWABLE_WINDOW) &&
                (pDst->type == DRAWABLE_WINDOW)));

    pbox = REGION_RECTS(prgnDst);
    nbox = REGION_NUM_RECTS(prgnDst);

    pboxNew1 = NULL;
    pptNew1 = NULL;
    pboxNew2 = NULL;
    pptNew2 = NULL;
    if (careful && (pptSrc->y < pbox->y1)) {
        /* walk source botttom to top */
	ydir = -1;

	if (nbox > 1) {
	    /* keep ordering in each band, reverse order of bands */
	    pboxNew1 = (BoxPtr)ALLOCATE_LOCAL(sizeof(BoxRec) * nbox);
	    if(!pboxNew1)
		return;
	    pptNew1 = (DDXPointPtr)ALLOCATE_LOCAL(sizeof(DDXPointRec) * nbox);
	    if(!pptNew1) {
	        DEALLOCATE_LOCAL(pboxNew1);
	        return;
	    }
	    pboxBase = pboxNext = pbox+nbox-1;
	    while (pboxBase >= pbox) {
	        while ((pboxNext >= pbox) &&
		       (pboxBase->y1 == pboxNext->y1))
		    pboxNext--;
	        pboxTmp = pboxNext+1;
	        pptTmp = pptSrc + (pboxTmp - pbox);
	        while (pboxTmp <= pboxBase) {
		    *pboxNew1++ = *pboxTmp++;
		    *pptNew1++ = *pptTmp++;
	        }
	        pboxBase = pboxNext;
	    }
	    pboxNew1 -= nbox;
	    pbox = pboxNew1;
	    pptNew1 -= nbox;
	    pptSrc = pptNew1;
        }
    } else {
	/* walk source top to bottom */
	ydir = 1;
    }

    if (careful && (pptSrc->x < pbox->x1)) {
	/* walk source right to left */
        xdir = -1;

	if (nbox > 1) {
	    /* reverse order of rects in each band */
	    pboxNew2 = (BoxPtr)ALLOCATE_LOCAL(sizeof(BoxRec) * nbox);
	    pptNew2 = (DDXPointPtr)ALLOCATE_LOCAL(sizeof(DDXPointRec) * nbox);
	    if(!pboxNew2 || !pptNew2) {
		if (pptNew2) DEALLOCATE_LOCAL(pptNew2);
		if (pboxNew2) DEALLOCATE_LOCAL(pboxNew2);
		if (pboxNew1) {
		    DEALLOCATE_LOCAL(pptNew1);
		    DEALLOCATE_LOCAL(pboxNew1);
		}
	        return;
	    }
	    pboxBase = pboxNext = pbox;
	    while (pboxBase < pbox+nbox) {
	        while ((pboxNext < pbox+nbox) &&
		       (pboxNext->y1 == pboxBase->y1))
		    pboxNext++;
	        pboxTmp = pboxNext;
	        pptTmp = pptSrc + (pboxTmp - pbox);
	        while (pboxTmp != pboxBase) {
		    *pboxNew2++ = *--pboxTmp;
		    *pptNew2++ = *--pptTmp;
	        }
	        pboxBase = pboxNext;
	    }
	    pboxNew2 -= nbox;
	    pbox = pboxNew2;
	    pptNew2 -= nbox;
	    pptSrc = pptNew2;
	}
    } else {
	/* walk source left to right */
        xdir = 1;
    }

    cfbGetByteWidthAndPointer(pDst, pitch, ptr);

    (*DoBlt)(ptr, pitch, nbox, pptSrc, pbox, xdir, ydir);
 
    if (pboxNew2) {
	DEALLOCATE_LOCAL(pptNew2);
	DEALLOCATE_LOCAL(pboxNew2);
    }
    if (pboxNew1) {
	DEALLOCATE_LOCAL(pptNew1);
	DEALLOCATE_LOCAL(pboxNew1);
    }

}

void 
cfb24_32DoBitblt24To24GXcopy(
    DrawablePtr pSrc, 
    DrawablePtr pDst, 
    int rop,
    RegionPtr prgnDst, 
    DDXPointPtr pptSrc,
    unsigned long pm,
    unsigned long bitPlane
){
    cfb24_32DoBitBlt(pSrc, pDst, prgnDst, pptSrc, Do24To24Blt);
}
