/*
 * Copyright (c) 1995-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
** Write quadword fill pattern helper macros
*/
#if __BIG_ENDIAN__
#define WRITEBYTE(c0,c1,d,n) do { \
    uint32_t c = c0; *((UInt8 *)d)++ = c0 >> 24; n -= 1; \
    c0 = ((c0 << 8) & 0xFFFFFF00) | ((c1 >> 24) & 0x000000FF); \
    c1 = ((c1 << 8) & 0xFFFFFF00) | ((c  >> 24) & 0x000000FF); \
} while(0)
#define WRITESHORT(c0,c1,d,n) do { \
    uint32_t c = c0; *((UInt16 *)d)++ = c0 >> 16; n -= 2; \
    c0 = ((c0 << 16) & 0xFFFF0000) | ((c1 >> 16) & 0x0000FFFF); \
    c1 = ((c1 << 16) & 0xFFFF0000) | ((c  >> 16) & 0x0000FFFF); \
} while(0)
#else
#define WRITEBYTE(c0,c1,d,n) do { \
    uint32_t c = c0; *((UInt8 *)d)++ = c0 & 0xFF; n -= 1; \
    c0 = ((c0 >> 8) & 0x00FFFFFF) | ((c1 << 24) & 0xFF000000); \
    c1 = ((c1 >> 8) & 0x00FFFFFF) | ((c  << 24) & 0xFF000000); \
} while(0)
#define WRITESHORT(c0,c1,d,n) do { \
    uint32_t c = c0; *((UInt16 *)d)++ = c0 & 0xFFFF; n -= 2;  \
    c0 = ((c0 >> 16) & 0x0000FFFF) | ((c1 << 16) & 0xFFFF0000); \
    c1 = ((c1 >> 16) & 0x0000FFFF) | ((c  << 16) & 0xFFFF0000); \
} while(0)
#endif
#define WRITEWORD(c0,c1,d,n) do { \
    uint32_t c = c0; *((UInt32 *)d)++ = c0; n -= 4; c0 = c1; c1 = c; \
} while(0)

static void FillVRAM8by1(int w, int h, uint32_t k0,uint32_t k1, uint8_t *dst, int dbpr)
{
    uint8_t *d;
    uint32_t c0,c1, n,v, i;

    if(w<=0 || h<=0)
        return;

    while(h > 0)
    {
        n   = w;
        d   = dst;
        dst = dst + dbpr;
        h   = h - 1;

        c0  = k0;
        c1  = k1;

        if((uint32_t)d & 0x0001)
	    WRITEBYTE(c0, c1, d, n);

        if(((uint32_t)d & 0x0002) && n>=2)
	    WRITESHORT(c0, c1, d, n);

        if(((uint32_t)d & 0x0004) && n>=4)
	    WRITEWORD(c0, c1, d, n);

        if(n >= 16)
        {
#if __BIG_ENDIAN__
	    double  kd, km[1];

            ((uint32_t *)km)[0] = c0;
            ((uint32_t *)km)[1] = c1;
            v  = n >> 3;
            n  = n & 0x07;
            kd = km[0];

            for(i=0 ; i<v ; i++)
                *((double*)d)++ = kd;
#else
            v  = n >> 3;
            n  = n & 0x07;
            for(i=0 ; i<v ; i++)
	    {
                *((uint32_t *)d)++ = c0;
		*((uint32_t *)d)++ = c1;
	    }
#endif
        }
        else if(n >= 8)
        {
            *((uint32_t *)d)++ = c0;
            n = n - 8;
            *((uint32_t *)d)++ = c1;
        }

        if(n & 0x04)
	    WRITEWORD(c0, c1, d, n);

        if(n & 0x02)
	    WRITESHORT(c0, c1, d, n);

        if(n & 0x01)
	    WRITEBYTE(c0, c1, d, n);
    }
}

static void DecompressRLE32(uint8_t *srcbase, uint8_t *dstbase, int minx, int maxx)
{
    uint32_t  *src, *dst;
    int        cnt, code, n,s,x;

    src = (uint32_t *)srcbase;
    dst = (uint32_t *)dstbase;

    for(x=0 ; x<maxx ;)
    {
        code = src[0];
        cnt  = (code & 0x00FFFFFF);
        code = (code & 0xFF000000) >> 24;

        if(code == 0x80)
        {
            if(x+cnt > minx)
            {
                n = (maxx-x < cnt)  ?  (maxx-x)  :  cnt;
                s = (minx-x > 0)  ?  (minx-x)  :  0;
                n = n - s;

                FillVRAM8by1(4*n, 1, src[1],src[1], (UInt8 *) dst, 0);
                dst = dst + n;
            }

            x   = x + cnt;
            src = src + 2;
        }

        else
        {
            if(x+cnt > minx)
            {
                n = (maxx-x < cnt)  ?  (maxx-x)  :  cnt;
                s = (minx-x > 0)  ?  (minx-x)  :  0;
                n = n - s;

                bcopy_nc((void*)(src+s+1), (void*)dst, 4*n);
                dst = dst + n;
            }

            x   = x + cnt;
            src = src + cnt + 1;
        }
    }
}

static void DecompressRLE16(uint8_t *srcbase, uint8_t *dstbase, int minx, int maxx)
{
    typedef u_int16_t	Pixel_Type;
    typedef u_int32_t	CodeWord_Type;
    Pixel_Type  *src, *dst;
    CodeWord_Type *codePtr;
    int        cnt, code, n,s,c, x;

    src = (Pixel_Type *)srcbase;
    dst = (Pixel_Type *)dstbase;

    for(x=0 ; x<maxx ;)
    {
        codePtr = (CodeWord_Type*) src;
        code = codePtr[0];
        src = (Pixel_Type*) &codePtr[1];
        cnt  = (code & 0x00FFFFFF);
        code = (code & 0xFF000000) >> 24;

        if(code == 0x80)
        {
            if(x+cnt > minx)
            {
                n = (maxx-x < cnt)  ?  (maxx-x)  :  cnt;
                s = (minx-x > 0)  ?  (minx-x)  :  0;
                n = n - s;

                c = src[0];
                c |= c << 16;	//Note: line not depth independent
                FillVRAM8by1(sizeof( Pixel_Type)*n,1, c,c, (UInt8 *)dst,0);
                dst = dst + n;
            }

            x   = x + cnt;
            src = src + 1;
        }

        else
        {
            if(x+cnt > minx)
            {
                n = (maxx-x < cnt)  ?  (maxx-x)  :  cnt;
                s = (minx-x > 0)  ?  (minx-x)  :  0;
                n = n - s;

                bcopy_nc((void*)(src+s), (void*)dst, sizeof(Pixel_Type)*n);
                dst = dst + n;
            }

            x   = x + cnt;
            src = src + cnt;
        }
    }
}

static void DecompressRLE8(uint8_t *srcbase, uint8_t *dstbase, int minx, int maxx)
{
    typedef u_int8_t	Pixel_Type;
    typedef u_int32_t	CodeWord_Type;
    Pixel_Type  *src, *dst;
    CodeWord_Type *codePtr;
    int        cnt, code, n,s,c, x;

    src = (Pixel_Type *)srcbase;
    dst = (Pixel_Type *)dstbase;

    for(x=0 ; x<maxx ;)
    {
        codePtr = (CodeWord_Type*) src;
        code = codePtr[0];
        src = (Pixel_Type*) &codePtr[1];
        cnt  = (code & 0x00FFFFFF);
        code = (code & 0xFF000000) >> 24;

        if(code == 0x80)
        {
            if(x+cnt > minx)
            {
                n = (maxx-x < cnt)  ?  (maxx-x)  :  cnt;
                s = (minx-x > 0)  ?  (minx-x)  :  0;
                n = n - s;

                c = src[0];
                c |= c << 8;	//Note: line not depth independent
                c |= c << 16;	//Note: line not depth independent
                FillVRAM8by1(sizeof( Pixel_Type)*n,1, c,c, (UInt8 *)dst,0);
                dst = dst + n;
            }

            x   = x + cnt;
            src = src + 1;
        }

        else
        {
            if(x+cnt > minx)
            {
                n = (maxx-x < cnt)  ?  (maxx-x)  :  cnt;
                s = (minx-x > 0)  ?  (minx-x)  :  0;
                n = n - s;

                bcopy_nc((void*)(src+s), (void*)dst, sizeof(Pixel_Type)*n);
                dst = dst + n;
            }

            x   = x + cnt;
            src = src + cnt;
        }
    }
}


static inline int compress_line_32(UInt8 *srcbase, int width, UInt8 *dstbase)
{
    uint32_t  *src, *dst;
    uint32_t  *start, *end;
    uint32_t   c0,c1;
    int        cpyCnt, rplCnt, wrtCnt;

    wrtCnt = 0;
    src    = (uint32_t *)srcbase;
    dst    = (uint32_t *)dstbase;
    end    = src + width;

    while(src<end)
    {
        cpyCnt = 0;
        rplCnt = 1;
        start  = src;
        c0     = src[0];

        for(src++ ; src<end ; src++)
        {
            c1 = src[0];

            if(c1 != c0)
            {
                if(rplCnt >= 4)  break;

                cpyCnt = cpyCnt + rplCnt;
                rplCnt = 1;
                c0     = c1;
            }

            else
                rplCnt++;
        }

        if(rplCnt < 4)
        {
            cpyCnt = cpyCnt + rplCnt;
            rplCnt = 0;
        }

        if(cpyCnt > 0)
        {
            dst[0] = cpyCnt;
            bcopy_nc(start, &dst[1], 4*cpyCnt);

            wrtCnt = wrtCnt + cpyCnt + 1;
            dst    = dst + cpyCnt + 1;
        }

        if(rplCnt > 0)
        {
            dst[0] = 0x80000000 | rplCnt;
            dst[1] = c0;

            wrtCnt = wrtCnt + 2;
            dst    = dst + 2;
        }
    }

    return 4*wrtCnt;
}

static inline int compress_line_16(uint8_t *srcbase, int width, uint8_t *dstbase)
{
    typedef u_int16_t	Pixel_Type;
    typedef u_int32_t	CodeWord_Type;
    Pixel_Type  *src, *dst;
    Pixel_Type  *start, *end;
    Pixel_Type   c0,c1;
    int        cpyCnt, rplCnt, wrtCnt;
    const int kMinRunLength = ( 2 * sizeof(CodeWord_Type)  + 2 * sizeof
( Pixel_Type ) ) / sizeof( Pixel_Type );

    wrtCnt = 0;
    src    = (Pixel_Type *)srcbase;
    dst    = (Pixel_Type *)dstbase;
    end    = src + width;

    while(src < end)
    {
        cpyCnt = 0;
        rplCnt = 1;
        start  = src;
        c0     = src[0];

        for(src++ ; src < end ; src++)
        {
            c1 = src[0];

            if(c1 != c0)
            {
                if(rplCnt >= kMinRunLength)  break;

                cpyCnt = cpyCnt + rplCnt;
                rplCnt = 1;
                c0     = c1;
            }
            else
                rplCnt++;
        }

        if(rplCnt < kMinRunLength )
        {
            cpyCnt = cpyCnt + rplCnt;
            rplCnt = 0;
        }

        if(cpyCnt > 0)
        {
            CodeWord_Type	*codeWord = (CodeWord_Type*) dst;
            codeWord[0] = cpyCnt;
            dst = (Pixel_Type*) (codeWord + 1);
            bcopy_nc(start, dst, sizeof( Pixel_Type )*cpyCnt);

            wrtCnt = wrtCnt + cpyCnt + sizeof(CodeWord_Type) / sizeof
( Pixel_Type );
            dst    = dst + cpyCnt;
        }

        if(rplCnt > 0)
        {
            CodeWord_Type	*codeWord = (CodeWord_Type*) dst;
            codeWord[0] = 0x80000000UL | rplCnt;
            dst = (Pixel_Type*) (codeWord + 1);
            dst[0] = c0;

            wrtCnt = wrtCnt + 1 + sizeof(CodeWord_Type) / sizeof
( Pixel_Type );
            dst    = dst + 1;
        }
    }

    return sizeof( Pixel_Type )*wrtCnt;
}

static inline int compress_line_8(uint8_t *srcbase, int width, uint8_t *dstbase)
{
    typedef u_int8_t	Pixel_Type;
    typedef u_int32_t	CodeWord_Type;
    Pixel_Type  *src, *dst;
    Pixel_Type  *start, *end;
    Pixel_Type   c0,c1;
    int        cpyCnt, rplCnt, wrtCnt;
    const int kMinRunLength = ( 2 * sizeof(CodeWord_Type)  + 2 * sizeof
( Pixel_Type ) ) / sizeof( Pixel_Type );

    wrtCnt = 0;
    src    = (Pixel_Type *)srcbase;
    dst    = (Pixel_Type *)dstbase;
    end    = src + width;

    while(src < end)
    {
        cpyCnt = 0;
        rplCnt = 1;
        start  = src;
        c0     = src[0];

        for(src++ ; src < end ; src++)
        {
            c1 = src[0];

            if(c1 != c0)
            {
                if(rplCnt >= kMinRunLength)  break;

                cpyCnt = cpyCnt + rplCnt;
                rplCnt = 1;
                c0     = c1;
            }
            else
                rplCnt++;
        }

        if(rplCnt < kMinRunLength )
        {
            cpyCnt = cpyCnt + rplCnt;
            rplCnt = 0;
        }

        if(cpyCnt > 0)
        {
            CodeWord_Type	*codeWord = (CodeWord_Type*) dst;
            codeWord[0] = cpyCnt;
            dst = (Pixel_Type*) (codeWord + 1);
            bcopy_nc(start, dst, sizeof( Pixel_Type )*cpyCnt);

            wrtCnt = wrtCnt + cpyCnt + sizeof(CodeWord_Type) / sizeof
( Pixel_Type );
            dst    = dst + cpyCnt;
        }

        if(rplCnt > 0)
        {
            CodeWord_Type	*codeWord = (CodeWord_Type*) dst;
            codeWord[0] = 0x80000000UL | rplCnt;
            dst = (Pixel_Type*) (codeWord + 1);
            dst[0] = c0;

            wrtCnt = wrtCnt + 1 + sizeof(CodeWord_Type) / sizeof
( Pixel_Type );
            dst    = dst + 1;
        }
    }

    return sizeof( Pixel_Type )*wrtCnt;
}


static int CompressData(UInt8 *srcbase, UInt32 depth, UInt32 width, UInt32 height,
		 UInt32 rowbytes, UInt8 *dstbase, UInt32 dlen)
{
    uint32_t *dst;
    uint32_t *cScan,*pScan;
    SInt32       cSize, pSize;
    UInt32       y;

    if(dlen <= (3+height)*sizeof(uint32_t))
    {
    	DEBG(0, "compressData: destination buffer size %ld too small for y index (%ld)\n",
		dlen, (3+height)*sizeof(uint32_t));
        return 0;
    }

    dst      = (uint32_t *)dstbase;

    dst[0]   = depth;
    dst[1]   = width;
    dst[2]   = height;
    dst      = dst + 3;

    cScan    = dst + height;
    pScan    = cScan;
    pSize    = -1;

    for(y=0 ; y<height ; y++)
    {
        if(((((uint8_t *)cScan)-dstbase) + 8*(width+1)) > dlen)
        {
            DEBG(0, "compressData: overflow: %d bytes in %ld byte buffer at scanline %ld (of %ld).\n",
		(uint8_t *)cScan-dstbase, dlen, y, height);

	    return 0;
        }

        cSize = (depth <= 1 ? compress_line_8 :
                (depth <= 2 ? compress_line_16 :
                 compress_line_32))(srcbase + y*rowbytes, width, (uint8_t *)cScan);

	if(cSize != pSize  ||  bcmp(pScan, cScan, cSize))
        {
            pScan  = cScan;
            cScan  = (uint32_t *)((uint8_t *)cScan + cSize);
            pSize  = cSize;
	}

        dst[y] = (uint8_t *)pScan - dstbase;
    }

    return (uint8_t *)cScan - dstbase;
}

static void DecompressData(UInt8 *srcbase, UInt8 *dstbase, int dx, int dy,
		    int dw, int dh, int rowbytes)
{
    uint32_t  *src;
    uint8_t   *dst;
    int        xMin,xMax;
    int        y, depth;

    src     = (uint32_t *)srcbase;
    dst     = (uint8_t *)dstbase;

    if ((dw != (int) src[1]) || (dh != (int) src[2]))
    {
	DEBG(0, " DecompressData mismatch %dx%d, %dx%d\n", dw, dh, src[1], src[2]);
	return;
    }

    depth   = src[0];
    src     = src + 3 + dy;

    xMin    = dx;
    xMax    = dx + dw;

    for(y=0 ; y<dh ; y++)
    {
        UInt8 *scan;

        scan = (UInt8 *)srcbase + *src;

        (depth <= 1  ? DecompressRLE8 :
            (depth <= 2 ? DecompressRLE16 : DecompressRLE32))
                (scan, dst, xMin,xMax);

        dst = dst + rowbytes;
        src = src + 1;
    }
}
