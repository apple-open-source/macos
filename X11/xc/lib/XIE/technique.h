/* $Xorg: technique.h,v 1.4 2001/02/09 02:03:42 xorgcvs Exp $ */

/*

Copyright 1993, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/

#ifndef WORD64

#define BEGIN_TECHNIQUE(_name, _bufDest, _dstParam) \
    _dstParam = (_name *) _bufDest;

#define END_TECHNIQUE(_name, _bufDest, _dstParam) \
    _bufDest += SIZEOF (_name);

#else /* WORD64 */

#define BEGIN_TECHNIQUE(_name, _bufDest, _dstParam) \
{ \
    _name tParam; \
    _dstParam = &tParam;

#define END_TECHNIQUE(_name, _bufDest, _dstParam) \
    memcpy (_bufDest, _dstParam, SIZEOF (_name)); \
    _bufDest += SIZEOF (_name); \
}

#endif /* WORD64 */



#ifndef WORD64

#define STORE_CARD32(_val, _pBuf) \
{ \
    *((CARD32 *) _pBuf) = _val; \
    _pBuf += SIZEOF (CARD32); \
}

#else /* WORD64 */

typedef struct {
    int value   :32;
} Long;

#define STORE_CARD32(_val, _pBuf) \
{ \
    Long _d; \
    _d.value = _val; \
    memcpy (_pBuf, &_d, SIZEOF (CARD32)); \
    _pBuf += SIZEOF (CARD32); \
}

#endif /* WORD64 */


#define _XieRGBToCIEXYZParam         _XieRGBToCIELabParam
#define _XieCIEXYZToRGBParam         _XieCIELabToRGBParam
#define _XieDecodeG32DParam          _XieDecodeG31DParam
#define _XieDecodeG42DParam          _XieDecodeG31DParam
#define _XieDecodeTIFF2Param         _XieDecodeG31DParam
#define _XieDecodeJPEGLosslessParam  _XieDecodeJPEGBaselineParam
