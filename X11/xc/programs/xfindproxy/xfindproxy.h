/* $Xorg: xfindproxy.h,v 1.4 2001/02/09 02:05:42 xorgcvs Exp $ */

/*
Copyright 1996, 1998  The Open Group

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


/*
 * Pad to a 64 bit boundary
 */

#define PAD64(_bytes) ((8 - ((unsigned int) (_bytes) % 8)) % 8)

#define PADDED_BYTES64(_bytes) (_bytes + PAD64 (_bytes))


/*
 * Number of 8 byte units in _bytes.
 */

#define WORD64COUNT(_bytes) (((unsigned int) ((_bytes) + 7)) >> 3)


/*
 * Compute the number of bytes for a STRING representation
 */

#define STRING_BYTES(_str) (2 + (_str ? strlen (_str) : 0) + \
		     PAD64 (2 + (_str ? strlen (_str) : 0)))



#define SKIP_STRING(_pBuf, _swap) \
{ \
    CARD16 _len; \
    EXTRACT_CARD16 (_pBuf, _swap, _len); \
    _pBuf += _len; \
    if (PAD64 (2 + _len)) \
        _pBuf += PAD64 (2 + _len); \
}

/*
 * STORE macros
 */

#define STORE_CARD16(_pBuf, _val) \
{ \
    *((CARD16 *) _pBuf) = _val; \
    _pBuf += 2; \
}

#define STORE_STRING(_pBuf, _string) \
{ \
    int _len = _string ? strlen (_string) : 0; \
    STORE_CARD16 (_pBuf, _len); \
    if (_len) { \
        memcpy (_pBuf, _string, _len); \
        _pBuf += _len; \
    } \
    if (PAD64 (2 + _len)) \
        _pBuf += PAD64 (2 + _len); \
}


/*
 * EXTRACT macros
 */

#define EXTRACT_CARD16(_pBuf, _swap, _val) \
{ \
    _val = *((CARD16 *) _pBuf); \
    _pBuf += 2; \
    if (_swap) \
        _val = lswaps (_val); \
}

#define EXTRACT_STRING(_pBuf, _swap, _string) \
{ \
    CARD16 _len; \
    EXTRACT_CARD16 (_pBuf, _swap, _len); \
    _string = (char *) malloc (_len + 1); \
    memcpy (_string, _pBuf, _len); \
    _string[_len] = '\0'; \
    _pBuf += _len; \
    if (PAD64 (2 + _len)) \
        _pBuf += PAD64 (2 + _len); \
}


/*
 * Byte swapping
 */

/* byte swap a long literal */
#define lswapl(_val) ((((_val) & 0xff) << 24) |\
		   (((_val) & 0xff00) << 8) |\
		   (((_val) & 0xff0000) >> 8) |\
		   (((_val) >> 24) & 0xff))

/* byte swap a short literal */
#define lswaps(_val) ((((_val) & 0xff) << 8) | (((_val) >> 8) & 0xff))


#define CHECK_AT_LEAST_SIZE(_iceConn, _majorOp, _minorOp, _expected_len, _actual_len, _severity) \
    if ((((_actual_len) - SIZEOF (iceMsg)) >> 3) > _expected_len) \
    { \
       _IceErrorBadLength (_iceConn, _majorOp, _minorOp, _severity); \
       return; \
    }


#define CHECK_COMPLETE_SIZE(_iceConn, _majorOp, _minorOp, _expected_len, _actual_len, _pStart, _severity) \
    if (((PADDED_BYTES64((_actual_len)) - SIZEOF (iceMsg)) >> 3) \
        != _expected_len) \
    { \
       _IceErrorBadLength (_iceConn, _majorOp, _minorOp, _severity); \
       IceDisposeCompleteMessage (iceConn, _pStart); \
       return; \
    }
