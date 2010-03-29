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
    uint32_t c = c0; STOREINC(d, c0 >> 24, UInt8); n -= 1; \
    c0 = ((c0 << 8) & 0xFFFFFF00) | ((c1 >> 24) & 0x000000FF); \
    c1 = ((c1 << 8) & 0xFFFFFF00) | ((c  >> 24) & 0x000000FF); \
} while(0)
#define WRITESHORT(c0,c1,d,n) do { \
    uint32_t c = c0; STOREINC(d, c0 >> 16, UInt16); n -= 2; \
    c0 = ((c0 << 16) & 0xFFFF0000) | ((c1 >> 16) & 0x0000FFFF); \
    c1 = ((c1 << 16) & 0xFFFF0000) | ((c  >> 16) & 0x0000FFFF); \
} while(0)
#else
#define WRITEBYTE(c0,c1,d,n) do { \
    uint32_t c = c0; STOREINC(d, c0 & 0xFF, UInt8); n -= 1; \
    c0 = ((c0 >> 8) & 0x00FFFFFF) | ((c1 << 24) & 0xFF000000); \
    c1 = ((c1 >> 8) & 0x00FFFFFF) | ((c  << 24) & 0xFF000000); \
} while(0)
#define WRITESHORT(c0,c1,d,n) do { \
    uint32_t c = c0; STOREINC(d, c0 & 0xFFFF, UInt16); n -= 2;  \
    c0 = ((c0 >> 16) & 0x0000FFFF) | ((c1 << 16) & 0xFFFF0000); \
    c1 = ((c1 >> 16) & 0x0000FFFF) | ((c  << 16) & 0xFFFF0000); \
} while(0)
#endif
#define WRITEWORD(c0,c1,d,n) do { \
    uint32_t c = c0; STOREINC(d, c0, UInt32); n -= 4; c0 = c1; c1 = c; \
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

        if((uintptr_t)d & 0x0001)
            WRITEBYTE(c0, c1, d, n);

        if(((uintptr_t)d & 0x0002) && n>=2)
            WRITESHORT(c0, c1, d, n);

        if(((uintptr_t)d & 0x0004) && n>=4)
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
                STOREINC(d, kd, double)
#else
            v  = n >> 3;
            n  = n & 0x07;
            for(i=0 ; i<v ; i++)
            {
                STOREINC(d, c0, uint32_t);
                STOREINC(d, c1, uint32_t);
            }
#endif
        }
        else if(n >= 8)
        {
            STOREINC(d, c0, uint32_t);
            n = n - 8;
            STOREINC(d, c1, uint32_t);
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
    typedef u_int16_t   Pixel_Type;
    typedef u_int32_t   CodeWord_Type;
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
                c |= c << 16;   //Note: line not depth independent
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
    typedef u_int8_t    Pixel_Type;
    typedef u_int32_t   CodeWord_Type;
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
                c |= c << 8;    //Note: line not depth independent
                c |= c << 16;   //Note: line not depth independent
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
    typedef u_int16_t   Pixel_Type;
    typedef u_int32_t   CodeWord_Type;
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
            CodeWord_Type       *codeWord = (CodeWord_Type*) dst;
            codeWord[0] = cpyCnt;
            dst = (Pixel_Type*) (codeWord + 1);
            bcopy_nc(start, dst, sizeof( Pixel_Type )*cpyCnt);

            wrtCnt = wrtCnt + cpyCnt + sizeof(CodeWord_Type) / sizeof
( Pixel_Type );
            dst    = dst + cpyCnt;
        }

        if(rplCnt > 0)
        {
            CodeWord_Type       *codeWord = (CodeWord_Type*) dst;
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
    typedef u_int8_t    Pixel_Type;
    typedef u_int32_t   CodeWord_Type;
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
            CodeWord_Type       *codeWord = (CodeWord_Type*) dst;
            codeWord[0] = cpyCnt;
            dst = (Pixel_Type*) (codeWord + 1);
            bcopy_nc(start, dst, sizeof( Pixel_Type )*cpyCnt);

            wrtCnt = wrtCnt + cpyCnt + sizeof(CodeWord_Type) / sizeof
( Pixel_Type );
            dst    = dst + cpyCnt;
        }

        if(rplCnt > 0)
        {
            CodeWord_Type       *codeWord = (CodeWord_Type*) dst;
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


static int CompressData(uint8_t *srcbase, uint32_t depth, uint32_t width, uint32_t height,
                 uint32_t rowbytes, uint8_t *dstbase, uint32_t dlen)
{
    uint32_t * dst;
    uint32_t * cScan,*pScan;
    UInt8 *    lineBuffer;
    int32_t    cSize, pSize;
    uint32_t   y, lineLen;

    if(dlen <= (3+height)*sizeof(uint32_t))
    {
        DEBG("", "compressData: destination buffer size %d too small for y index (%ld)\n",
                dlen, (3+height)*sizeof(uint32_t));
        return 0;
    }

    dst      = (uint32_t *)dstbase;

    lineLen = width * depth;
    lineBuffer = dstbase + dlen - lineLen;
    dlen -= lineLen;

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
            DEBG("", "compressData: overflow: %ld bytes in %d byte buffer at scanline %d (of %d).\n",
                (size_t)(((uint8_t *)cScan)-dstbase), dlen, y, height);

            return 0;
        }

        bcopy_nc(srcbase + y*rowbytes, lineBuffer, lineLen);

        if (0 == (y & 7))
        {
            AbsoluteTime deadline;
            clock_interval_to_deadline(8, kMicrosecondScale, &deadline);
            assert_wait_deadline((event_t)&clock_delay_until, THREAD_UNINT, __OSAbsoluteTime(deadline));
            thread_block(NULL);
        }

        cSize = (depth <= 1 ? compress_line_8 :
                (depth <= 2 ? compress_line_16 :
                 compress_line_32))(lineBuffer, width, (uint8_t *)cScan);

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
        DEBG("", " DecompressData mismatch %dx%d, %dx%d\n", dw, dh, src[1], src[2]);
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

        if (0 == (y & 7))
        {
            AbsoluteTime deadline;
            clock_interval_to_deadline(8, kMicrosecondScale, &deadline);
            assert_wait_deadline((event_t)&clock_delay_until, THREAD_UNINT, __OSAbsoluteTime(deadline));
            thread_block(NULL);
        }

        (depth <= 1  ? DecompressRLE8 :
            (depth <= 2 ? DecompressRLE16 : DecompressRLE32))
                (scan, dst, xMin,xMax);

        dst = dst + rowbytes;
        src = src + 1;
    }
}

static void 
PreviewDecompress16(uint32_t * compressBuffer, 
                        uint32_t width, uint32_t height, uint32_t row, 
                        uint16_t * output)
{
    uint32_t i, j;
    uint32_t * input;
    
    uint16_t * sc0 = IONew(uint16_t, (width+2));
    uint16_t * sc1 = IONew(uint16_t, (width+2));
    uint16_t * sc2 = IONew(uint16_t, (width+2));
    uint16_t * sc3 = IONew(uint16_t, (width+2));
    uint32_t   sr0, sr1, sr2, sr3;

    bzero(sc0, (width+2) * sizeof(uint16_t));
    bzero(sc1, (width+2) * sizeof(uint16_t));
    bzero(sc2, (width+2) * sizeof(uint16_t));
    bzero(sc3, (width+2) * sizeof(uint16_t));

    uint32_t tmp1, tmp2, out;
    for (j = 0; j < (height + 2); j++)
    {
        input = compressBuffer;
        if (j < height)
            input += j;
        else
            input += height - 1;
        input = (uint32_t *)(input[3] + ((uint8_t *)compressBuffer));

        uint32_t data = 0, repeat = 0, fetch, count = 0;
        sr0 = sr1 = sr2 = sr3 = 0;

        for (i = 0; i < (width + 2); i++)
        {
            if (i < width)
            {
                if (!count)
                {
                    count = *input++;
                    repeat = (count & 0xff000000);
                    count ^= repeat;
                    fetch = true;
                }
                else
                    fetch = (0 == repeat);
    
                count--;
    
                if (fetch)
                {
                    data = *((uint16_t *)input);
                    input = (uint32_t *)(((uint8_t *) input) + sizeof(uint16_t));
    
                    // grayscale
                    // srgb 13933, 46871, 4732
                    // ntsc 19595, 38470, 7471
                    data = 13933 * (0x1f & (data >> 10))
                         + 46871 * (0x1f & (data >> 5))
                         +  4732 * (0x1f & data);
                    data >>= 13;
        
                    // 70% white, 30 % black
                    data *= 19661;
                    data += (103 << 16);
                    data >>= 16;
                }
            }

            // gauss blur
            tmp2 = sr0 + data;
            sr0 = data;
            tmp1 = sr1 + tmp2;
            sr1 = tmp2;
            tmp2 = sr2 + tmp1;
            sr2 = tmp1;
            tmp1 = sr3 + tmp2;
            sr3 = tmp2;
            
            tmp2 = sc0[i] + tmp1;
            sc0[i] = tmp1;
            tmp1 = sc1[i] + tmp2;
            sc1[i] = tmp2;
            tmp2 = sc2[i] + tmp1;
            sc2[i] = tmp1;
            out = (128 + sc3[i] + tmp2) >> 11;
            sc3[i] = tmp2;

            out &= 0x1f;
            if ((i > 1) && (j > 1))
                output[i-2] = out | (out << 5) | (out << 10);
        }

        if (j > 1)
            output += row;
    }
    IODelete(sc3, uint16_t, (width+2));
    IODelete(sc2, uint16_t, (width+2));
    IODelete(sc1, uint16_t, (width+2));
    IODelete(sc0, uint16_t, (width+2));
}

static void 
PreviewDecompress32(uint32_t * compressBuffer, 
                        uint32_t width, uint32_t height, uint32_t row, 
                        uint32_t * output)
{
    uint32_t i, j;
    uint32_t * input;
    
    uint16_t * sc0 = IONew(uint16_t, (width+2));
    uint16_t * sc1 = IONew(uint16_t, (width+2));
    uint16_t * sc2 = IONew(uint16_t, (width+2));
    uint16_t * sc3 = IONew(uint16_t, (width+2));
    uint32_t   sr0, sr1, sr2, sr3;

    bzero(sc0, (width+2) * sizeof(uint16_t));
    bzero(sc1, (width+2) * sizeof(uint16_t));
    bzero(sc2, (width+2) * sizeof(uint16_t));
    bzero(sc3, (width+2) * sizeof(uint16_t));

    uint32_t tmp1, tmp2, out;
    for (j = 0; j < (height + 2); j++)
    {
        input = compressBuffer;
        if (j < height)
            input += j;
        else
            input += height - 1;
        input = (uint32_t *)(input[3] + ((uint8_t *)compressBuffer));

        uint32_t data = 0, repeat = 0, fetch, count = 0;
        sr0 = sr1 = sr2 = sr3 = 0;

        for (i = 0; i < (width + 2); i++)
        {
            if (i < width)
            {
                if (!count)
                {
                    count = *input++;
                    repeat = (count & 0xff000000);
                    count ^= repeat;
                    fetch = true;
                }
                else
                    fetch = (0 == repeat);
    
                count--;
    
                if (fetch)
                {
                    data = *input++;
    
                    // grayscale
                    // srgb 13933, 46871, 4732
                    // ntsc 19595, 38470, 7471
                    data = 13933 * (0xff & (data >> 24))
                         + 46871 * (0xff & (data >> 16))
                         +  4732 * (0xff & data);
                    data >>= 16;
        
                    // 70% white, 30 % black
                    data *= 19661;
                    data += (103 << 16);
                    data >>= 16;
                }
            }

            // gauss blur
            tmp2 = sr0 + data;
            sr0 = data;
            tmp1 = sr1 + tmp2;
            sr1 = tmp2;
            tmp2 = sr2 + tmp1;
            sr2 = tmp1;
            tmp1 = sr3 + tmp2;
            sr3 = tmp2;
            
            tmp2 = sc0[i] + tmp1;
            sc0[i] = tmp1;
            tmp1 = sc1[i] + tmp2;
            sc1[i] = tmp2;
            tmp2 = sc2[i] + tmp1;
            sc2[i] = tmp1;
            out = (128 + sc3[i] + tmp2) >> 8;
            sc3[i] = tmp2;

            out &= 0xff;
            if ((i > 1) && (j > 1))
                output[i-2] = out | (out << 8) | (out << 16);
        }

        if (j > 1)
            output += row;
    }

    IODelete(sc3, uint16_t, (width+2));
    IODelete(sc2, uint16_t, (width+2));
    IODelete(sc1, uint16_t, (width+2));
    IODelete(sc0, uint16_t, (width+2));
}

bool
PreviewDecompressData(void *srcbase, void *dstbase,
                      int dw, int dh, int bytesPerPixel, int rowbytes)
{
    uint32_t * src = (uint32_t *) srcbase;

    if ((bytesPerPixel != (int) src[0]) || (dw != (int) src[1]) || (dh != (int) src[2]))
      return (false);

    switch(bytesPerPixel)
    {
      case 4:
        PreviewDecompress32((uint32_t *)srcbase, dw, dh, rowbytes >> 2, (uint32_t *) dstbase);
        return (true);
      case 2:
        PreviewDecompress16((uint32_t *)srcbase, dw, dh, rowbytes >> 1, (uint16_t *) dstbase);
        return (true);
      default:
        return (false);
    }
}

