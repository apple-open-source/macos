/* $Xorg: pl_store.h,v 1.4 2001/02/09 02:03:29 xorgcvs Exp $ */
/*

Copyright 1992, 1998  The Open Group

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

#define PUT_CARD32(_val, _pBuf) \
    *((CARD32 *) _pBuf) = _val

#define PUT_CARD16(_val, _pBuf) \
    *((CARD16 *) _pBuf) = _val

#define PUT_INT32(_val, _pBuf) \
    *((INT32 *) _pBuf) = _val

#define PUT_INT16(_val, _pBuf) \
    *((INT16 *) _pBuf) = _val

#else /* WORD64 */

#define PUT_CARD32(_val, _pBuf) \
    CARD64_TO_32 (_val, _pBuf)

#define PUT_CARD16(_val, _pBuf) \
    CARD64_TO_16 (_val, _pBuf)

#define PUT_INT32(_val, _pBuf) \
    INT64_TO_32 (_val, _pBuf)

#define PUT_INT16(_val, _pBuf) \
    INT64_TO_16 (_val, _pBuf)

#endif /* WORD64 */


#define STORE_CARD32(_val, _pBuf) \
{ \
    PUT_CARD32 (_val, _pBuf); \
    _pBuf += SIZEOF (CARD32); \
}

#define STORE_CARD16(_val, _pBuf) \
{ \
    PUT_CARD16 (_val, _pBuf); \
    _pBuf += SIZEOF (CARD16); \
}

#define STORE_INT32(_val, _pBuf) \
{ \
    PUT_INT32 (_val, _pBuf); \
    _pBuf += SIZEOF (INT32); \
}

#define STORE_INT16(_val, _pBuf) \
{ \
    PUT_INT16 (_val, _pBuf); \
    _pBuf += SIZEOF (INT16); \
}

/* ------------------------------------------------------------------------ */

#ifndef WORD64

#define STORE_LISTOF_CARD32(_count, _pList, _pBuf) \
{ \
    memcpy (_pBuf, _pList, _count * SIZEOF (CARD32)); \
    _pBuf += (_count * SIZEOF (CARD32)); \
}

#define STORE_LISTOF_CARD16(_count, _pList, _pBuf) \
{ \
    memcpy (_pBuf, _pList, _count * SIZEOF (CARD16)); \
    _pBuf += (_count * SIZEOF (CARD16)); \
}

#define STORE_LISTOF_INT32(_count, _pList, _pBuf) \
{ \
    memcpy (_pBuf, _pList, _count * SIZEOF (INT32)); \
    _pBuf += (_count * SIZEOF (INT32)); \
}

#define STORE_LISTOF_INT16(_count, _pList, _pBuf) \
{ \
    memcpy (_pBuf, _pList, _count * SIZEOF (INT16)); \
    _pBuf += (_count * SIZEOF (INT16)); \
}

#else /* WORD64 */

#define STORE_LISTOF_CARD32(_count, _pList, _pBuf) \
{ \
    int _i; \
    for (_i = 0; _i < _count; _i++) \
        STORE_CARD32 (_pList[_i], _pBuf); \
}

#define STORE_LISTOF_CARD16(_count, _pList, _pBuf) \
{ \
    int _i; \
    for (_i = 0; _i < _count; _i++) \
        STORE_CARD16 (_pList[_i], _pBuf); \
}

#define STORE_LISTOF_INT32(_count, _pList, _pBuf) \
{ \
    int _i; \
    for (_i = 0; _i < _count; _i++) \
        STORE_INT32 (_pList[_i], _pBuf); \
}

#define STORE_LISTOF_INT16(_count, _pList, _pBuf) \
{ \
    int _i; \
    for (_i = 0; _i < _count; _i++) \
        STORE_INT16 (_pList[_i], _pBuf); \
}

#endif /* WORD64 */

/* ------------------------------------------------------------------------ */

#ifndef WORD64

#define STORE_FLOAT32(_val, _pBuf, _fpConvert, _fpFormat) \
{ \
    if (!_fpConvert) \
    { \
        *((float *) _pBuf) = _val; \
    } \
    else \
    { \
        FP_CONVERT_HTON_BUFF (_val, _pBuf, _fpFormat); \
    } \
    _pBuf += SIZEOF (float); \
}

#define STORE_LISTOF_FLOAT32(_count, _pList, _pBuf, _fpConvert, _fpFormat) \
{ \
    if (!_fpConvert) \
    { \
        memcpy (_pBuf, _pList, _count * SIZEOF (float)); \
        _pBuf += (_count * SIZEOF (float)); \
    } \
    else \
    { \
        int _i; \
        float *fptr = (float *) _pList; \
\
        for (_i = 0; _i < (int) _count; _i++) \
	{ \
	    FP_CONVERT_HTON_BUFF (*fptr, _pBuf, _fpFormat); \
	    _pBuf += SIZEOF (float); \
	    fptr++; \
        } \
    }\
}

#else /* WORD64 */

#define STORE_FLOAT32(_val, _pBuf, _fpConvert, _fpFormat) \
{ \
    FP_CONVERT_HTON_BUFF (_val, _pBuf, _fpFormat); \
    _pBuf += SIZEOF (float); \
}

#define STORE_LISTOF_FLOAT32(_count, _pList, _pBuf, _fpConvert, _fpFormat) \
{ \
    int _i; \
    float *fptr = (float *) _pList; \
\
    for (_i = 0; _i < _count; _i++) \
    { \
	FP_CONVERT_HTON_BUFF (*fptr, _pBuf, _fpFormat); \
	_pBuf += SIZEOF (float); \
	fptr++; \
    } \
}

#endif /* WORD64 */



/* ------------------------------------------------------------------------ */
/*   STORE_FOO and STORE_LISTOF_FOO					    */
/*   (where FOO has no floating point values in it)	    		    */
/* ------------------------------------------------------------------------ */


#if (defined(__STDC__) && !defined(UNIXCPP)) || defined(ANSICPP)
#define PUT_TEMP(_pexType, _foo, _tFoo) PUT_TEMP_##_pexType (_foo, _tFoo);
#else
#define PUT_TEMP(_pexType, _foo, _tFoo) PUT_TEMP_/**/_pexType (_foo, _tFoo);
#endif


#ifndef WORD64

#define STORE_FOO(_pexType, _foo, _pBuf) \
{ \
    memcpy (_pBuf, &(_foo), SIZEOF (_pexType)); \
    _pBuf += SIZEOF (_pexType); \
}

#define STORE_LISTOF_FOO(_pexType, _count, _fooList, _pBuf) \
{ \
    memcpy (_pBuf, _fooList, _count * SIZEOF (_pexType)); \
    _pBuf += (_count * SIZEOF (_pexType)); \
}

#else /* WORD64 */

#define STORE_FOO(_pexType, _foo, _pBuf) \
{ \
    _pexType	tFoo; \
    PUT_TEMP(_pexType, _foo, tFoo); \
    memcpy (_pBuf, &tFoo, SIZEOF (_pexType)); \
    _pBuf += SIZEOF (_pexType); \
}

#define STORE_LISTOF_FOO(_pexType, _count, _fooList, _pBuf) \
{ \
    _pexType	tFoo; \
    int		_i; \
\
    for (_i = 0; _i < (int) _count; _i++) \
    { \
        PUT_TEMP(_pexType, _fooList[_i], tFoo); \
        memcpy (_pBuf, &tFoo, SIZEOF (_pexType)); \
        _pBuf += SIZEOF (_pexType); \
    } \
}

#endif /* WORD64 */


/*
 * TEXT ALIGNMENT
 */

#define STORE_TEXTALIGN(_align, _pBuf) \
    STORE_FOO (pexTextAlignmentData, _align, _pBuf)

#define PUT_TEMP_pexTextAlignmentData(_src, _dst) \
    _dst.vertical   = _src.vertical; \
    _dst.horizontal = _src.horizontal;


/*
 * PSC ISO CURVES 
 */

#define STORE_PSC_ISOCURVES(_isoCurves, _pBuf) \
    STORE_FOO (pexPSC_IsoparametricCurves, _isoCurves, _pBuf)

#define PUT_TEMP_pexPSC_IsoparametricCurves(_src, _dst) \
    _dst.placementType = _src.placement_type; \
    _dst.numUcurves    = _src.u_count; \
    _dst.numVcurves    = _src.v_count;


/*
 * List of ELEMENT REF
 */

#define STORE_LISTOF_ELEMREF(_count, _pList, _pBuf) \
    STORE_LISTOF_FOO (pexElementRef, _count, _pList, _pBuf)

#define PUT_TEMP_pexElementRef(_src, _dst) \
    _dst.structure = _src.structure; \
    _dst.offset    = _src.offset;


/*
 * List of PICK ELEMENT REF
 */

#define STORE_LISTOF_PICKELEMREF(_count, _pList, _pBuf) \
    STORE_LISTOF_FOO (pexPickElementRef, _count, _pList, _pBuf)

#define PUT_TEMP_pexPickElementRef(_src, _dst) \
    _dst.sid    = _src.sid; \
    _dst.offset = _src.offset; \
    _dst.pickid = _src.pick_id;


/*
 * List of DEVICE RECT
 */

#define STORE_LISTOF_DEVRECT(_count, _pList, _pBuf) \
    STORE_LISTOF_FOO (pexDeviceRect, _count, _pList, _pBuf)

#define PUT_TEMP_pexDeviceRect(_src, _dst) \
    _dst.xmin = _src.xmin; \
    _dst.ymin = _src.ymin; \
    _dst.xmax = _src.xmax; \
    _dst.ymax = _src.ymax;


/*
 * List of NAME SET PAIR
 */

#define STORE_LISTOF_NAMESET_PAIR(_count, _pList, _pBuf) \
    STORE_LISTOF_FOO (pexNameSetPair, _count, _pList, _pBuf)

#define PUT_TEMP_pexNameSetPair(_src, _dst) \
    _dst.incl = _src.inclusion; \
    _dst.excl = _src.exclusion;



/* ------------------------------------------------------------------------ */
/*   STORE_FOOFP and STORE_LISTOF_FOOFP				    	    */
/* ------------------------------------------------------------------------ */

#if (defined(__STDC__) && !defined(UNIXCPP)) || defined(ANSICPP)
#define DOSTORE(_pexType, _src, _dst, _fpConvert, _fpFormat) \
    DOSTORE_##_pexType (_src, _dst, _fpConvert, _fpFormat)
#else
#define DOSTORE(_pexType, _src, _dst, _fpConvert, _fpFormat) \
    DOSTORE_/**/_pexType (_src, _dst, _fpConvert, _fpFormat)
#endif


#ifndef WORD64

#define STORE_FOOFP(_pexType, _foo, _pBuf, _fpConvert, _fpFormat) \
{ \
    if (!_fpConvert) \
    { \
        memcpy (_pBuf, &(_foo), SIZEOF (_pexType)); \
    } \
    else \
    { \
        _pexType *pFoo = (_pexType *) _pBuf; \
        DOSTORE (_pexType, _foo, pFoo, _fpConvert, _fpFormat); \
    } \
    _pBuf += SIZEOF (_pexType); \
}

#define STORE_LISTOF_FOOFP(_pexType, _count, _fooList, _pBuf, _fpConvert, _fpFormat) \
{ \
    if (!_fpConvert) \
    { \
        memcpy (_pBuf, _fooList, _count * SIZEOF (_pexType)); \
        _pBuf += (_count * SIZEOF (_pexType)); \
    } \
    else \
    { \
        int _i; \
        for (_i = 0; _i < (int) _count; _i++) \
	{ \
	    _pexType *pFoo = (_pexType *) _pBuf; \
            DOSTORE (_pexType, _fooList[_i], pFoo, _fpConvert, _fpFormat); \
	    _pBuf += SIZEOF (_pexType); \
        } \
    } \
}

#else /* WORD64 */

#define STORE_FOOFP(_pexType, _foo, _pBuf, _fpConvert, _fpFormat) \
{ \
    _pexType tFoo; \
    _pexType *pFoo = &tFoo; \
    DOSTORE (_pexType, _foo, pFoo, _fpConvert, _fpFormat); \
    memcpy (_pBuf, pFoo, SIZEOF (_pexType)); \
    _pBuf += SIZEOF (_pexType); \
}

#define STORE_LISTOF_FOOFP(_pexType, _count, _fooList, _pBuf, _fpConvert, _fpFormat) \
{ \
    int _i; \
    _pexType tFoo; \
    _pexType *pFoo = &tFoo; \
    for (_i = 0; _i < (int) _count; _i++) \
    { \
        DOSTORE (_pexType, _fooList[_i], pFoo, _fpConvert, _fpFormat); \
        memcpy (_pBuf, pFoo, SIZEOF (_pexType)); \
        _pBuf += SIZEOF (_pexType); \
    } \
}

#endif /* WORD64 */


/*
 * NPC SUBVOLUME
 */

#define STORE_NPC_SUBVOLUME(_volume, _pBuf, _fpConvert, _fpFormat) \
    STORE_FOOFP (pexNpcSubvolume, _volume, _pBuf, _fpConvert, _fpFormat)

#define DOSTORE_pexNpcSubvolume(_src, _dst, _fpConvert, _fpFormat) \
    if (_fpConvert) \
    { \
        FP_CONVERT_HTON (_src.min.x, _dst->xmin, _fpFormat); \
	FP_CONVERT_HTON (_src.min.y, _dst->ymin, _fpFormat); \
	FP_CONVERT_HTON (_src.min.z, _dst->zmin, _fpFormat); \
	FP_CONVERT_HTON (_src.max.x, _dst->xmax, _fpFormat); \
	FP_CONVERT_HTON (_src.max.y, _dst->ymax, _fpFormat); \
	FP_CONVERT_HTON (_src.max.z, _dst->zmax, _fpFormat); \
    } \
    else \
    { \
	_dst->xmin = _src.min.x; \
	_dst->ymin = _src.min.y; \
	_dst->zmin = _src.min.z; \
	_dst->xmax = _src.max.x; \
	_dst->ymax = _src.max.y; \
	_dst->zmax = _src.max.z; \
    }


/*
 * VIEWPORT
 */

#define STORE_VIEWPORT(_viewport, _pBuf, _fpConvert, _fpFormat) \
    STORE_FOOFP (pexViewport, _viewport, _pBuf, _fpConvert, _fpFormat)

#define DOSTORE_pexViewport(_src, _dst, _fpConvert, _fpFormat) \
    _dst->xmin = _src.min.x; \
    _dst->ymin = _src.min.y; \
    _dst->xmax = _src.max.x; \
    _dst->ymax = _src.max.y; \
    _dst->useDrawable = _src.use_drawable; \
    if (_fpConvert) \
    { \
        FP_CONVERT_HTON (_src.min.z, _dst->zmin, _fpFormat); \
	FP_CONVERT_HTON (_src.max.z, _dst->zmax, _fpFormat); \
    } \
    else \
    { \
	_dst->zmin = _src.min.z; \
	_dst->zmax = _src.max.z; \
    }


/*
 * COORD 4D
 */

#define STORE_COORD4D(_coord4D, _pBuf, _fpConvert, _fpFormat) \
    STORE_FOOFP (pexCoord4D, _coord4D, _pBuf, _fpConvert, _fpFormat)


#define STORE_LISTOF_COORD4D(_count, _pList, _pBuf, _fpConvert, _fpFormat) \
    STORE_LISTOF_FOOFP (pexCoord4D, _count, _pList, _pBuf, \
	_fpConvert, _fpFormat);


#define DOSTORE_pexCoord4D(_src, _dst, _fpConvert, _fpFormat) \
    if (_fpConvert) \
    { \
        FP_CONVERT_HTON (_src.x, _dst->x, _fpFormat); \
        FP_CONVERT_HTON (_src.y, _dst->y, _fpFormat); \
        FP_CONVERT_HTON (_src.z, _dst->z, _fpFormat); \
        FP_CONVERT_HTON (_src.w, _dst->w, _fpFormat); \
    } \
    else \
    { \
	_dst->x = _src.x; \
	_dst->y = _src.y; \
	_dst->z = _src.z; \
	_dst->w = _src.w; \
    }


/*
 * COORD 3D
 */

#define STORE_COORD3D(_coord3D, _pBuf, _fpConvert, _fpFormat) \
    STORE_FOOFP (pexCoord3D, _coord3D, _pBuf, _fpConvert, _fpFormat)


#define STORE_LISTOF_COORD3D(_count, _pList, _pBuf, _fpConvert, _fpFormat) \
    STORE_LISTOF_FOOFP (pexCoord3D, _count, _pList, _pBuf, \
	_fpConvert, _fpFormat);


#define DOSTORE_pexCoord3D(_src, _dst, _fpConvert, _fpFormat) \
    if (_fpConvert) \
    { \
        FP_CONVERT_HTON (_src.x, _dst->x, _fpFormat); \
        FP_CONVERT_HTON (_src.y, _dst->y, _fpFormat); \
        FP_CONVERT_HTON (_src.z, _dst->z, _fpFormat); \
    } \
    else \
    { \
	_dst->x = _src.x; \
	_dst->y = _src.y; \
	_dst->z = _src.z; \
    }


/*
 * COORD 2D
 */

#define STORE_COORD2D(_coord2D, _pBuf, _fpConvert, _fpFormat) \
    STORE_FOOFP (pexCoord2D, _coord2D, _pBuf, _fpConvert, _fpFormat)


#define STORE_LISTOF_COORD2D(_count, _pList, _pBuf, _fpConvert, _fpFormat) \
    STORE_LISTOF_FOOFP (pexCoord2D, _count, _pList, _pBuf, \
	_fpConvert, _fpFormat);


#define DOSTORE_pexCoord2D(_src, _dst, _fpConvert, _fpFormat) \
    if (_fpConvert) \
    { \
        FP_CONVERT_HTON (_src.x, _dst->x, _fpFormat); \
        FP_CONVERT_HTON (_src.y, _dst->y, _fpFormat); \
    } \
    else \
    { \
	_dst->x = _src.x; \
	_dst->y = _src.y; \
    }


/*
 * VECTOR 3D
 */

#define STORE_VECTOR3D(_vector3D, _pBuf, _fpConvert, _fpFormat) \
    STORE_FOOFP (pexVector3D, _vector3D, _pBuf, _fpConvert, _fpFormat)


#define DOSTORE_pexVector3D(_src, _dst, _fpConvert, _fpFormat) \
    if (_fpConvert) \
    { \
        FP_CONVERT_HTON (_src.x, _dst->x, _fpFormat); \
        FP_CONVERT_HTON (_src.y, _dst->y, _fpFormat); \
        FP_CONVERT_HTON (_src.z, _dst->z, _fpFormat); \
    } \
    else \
    { \
	_dst->x = _src.x; \
	_dst->y = _src.y; \
	_dst->z = _src.z; \
    }


/*
 * VECTOR 2D
 */

#define STORE_VECTOR2D(_vector2D, _pBuf, _fpConvert, _fpFormat) \
    STORE_FOOFP (pexVector2D, _vector2D, _pBuf, _fpConvert, _fpFormat)


#define DOSTORE_pexVector2D(_src, _dst, _fpConvert, _fpFormat) \
    if (_fpConvert) \
    { \
        FP_CONVERT_HTON (_src.x, _dst->x, _fpFormat); \
        FP_CONVERT_HTON (_src.y, _dst->y, _fpFormat); \
    } \
    else \
    { \
	_dst->x = _src.x; \
	_dst->y = _src.y; \
    }


/*
 * PSC LEVEL CURVES
 */

#define STORE_PSC_LEVELCURVES(_levCurv, _pBuf, _fpConvert, _fpFormat) \
    STORE_FOOFP (pexPSC_LevelCurves, _levCurv, _pBuf, _fpConvert, _fpFormat)


#define DOSTORE_pexPSC_LevelCurves(_src, _dst, _fpConvert, _fpFormat) \
    if (_fpConvert) \
    { \
        FP_CONVERT_HTON (_src.origin.x, _dst->origin_x, _fpFormat); \
        FP_CONVERT_HTON (_src.origin.y, _dst->origin_y, _fpFormat); \
        FP_CONVERT_HTON (_src.origin.z, _dst->origin_z, _fpFormat); \
        FP_CONVERT_HTON (_src.direction.x, _dst->direction_x, _fpFormat); \
        FP_CONVERT_HTON (_src.direction.y, _dst->direction_y, _fpFormat); \
        FP_CONVERT_HTON (_src.direction.z, _dst->direction_z, _fpFormat); \
    } \
    else \
    { \
	_dst->origin_x = _src.origin.x; \
	_dst->origin_y = _src.origin.y; \
	_dst->origin_z = _src.origin.z; \
	_dst->direction_x = _src.direction.x; \
	_dst->direction_y = _src.direction.y; \
	_dst->direction_z = _src.direction.z; \
    } \
    _dst->numberIntersections = _src.count;


/*
 * REFLECTION ATTRIBUTES
 */

#define STORE_REFLECTION_ATTR(_reflAttr, _pBuf, _fpConvert, _fpFormat) \
    STORE_FOOFP (pexReflectionAttr, _reflAttr, _pBuf, _fpConvert, _fpFormat) \
    STORE_COLOR_VAL (_reflAttr.specular_color.type, \
	_reflAttr.specular_color.value, _pBuf, _fpConvert, _fpFormat)


#define DOSTORE_pexReflectionAttr(_src, _dst, _fpConvert, _fpFormat) \
    if (_fpConvert) \
    { \
        FP_CONVERT_HTON (_src.ambient, _dst->ambient, _fpFormat); \
        FP_CONVERT_HTON (_src.diffuse, _dst->diffuse, _fpFormat); \
        FP_CONVERT_HTON (_src.specular, _dst->specular, _fpFormat); \
        FP_CONVERT_HTON (_src.specular_conc, _dst->specularConc, _fpFormat); \
        FP_CONVERT_HTON (_src.transmission, _dst->transmission, _fpFormat); \
    } \
    else \
    { \
	_dst->ambient = _src.ambient; \
	_dst->diffuse = _src.diffuse; \
	_dst->specular = _src.specular; \
	_dst->specularConc = _src.specular_conc; \
	_dst->transmission = _src.transmission; \
    } \
    _dst->specular_colorType = _src.specular_color.type;


/*
 * List of DEVICE COORD
 */ 

#define STORE_LISTOF_DEVCOORD(_count, _pList, _pBuf, _fpConvert, _fpFormat) \
    STORE_LISTOF_FOOFP (pexDeviceCoord, _count, _pList, _pBuf, \
	_fpConvert, _fpFormat);

#define DOSTORE_pexDeviceCoord(_src, _dst, _fpConvert, _fpFormat) \
    _dst->x = _src.x; \
    _dst->y = _src.y; \
    if (_fpConvert) \
    { \
        FP_CONVERT_HTON (_src.z, _dst->z, _fpFormat); \
    } \
    else \
	_dst->z = _src.z;


/*
 * List of HALF SPACE
 */ 

#define STORE_LISTOF_HALFSPACE3D(_count, _pList, _pBuf, _fpConvert, _fpFormat)\
    STORE_LISTOF_FOOFP (pexHalfSpace, _count, _pList, _pBuf, \
	_fpConvert, _fpFormat);

#define DOSTORE_pexHalfSpace(_src, _dst, _fpConvert, _fpFormat) \
    if (_fpConvert) \
    { \
	FP_CONVERT_HTON (_src.point.x, _dst->point_x, _fpFormat); \
	FP_CONVERT_HTON (_src.point.y, _dst->point_y, _fpFormat); \
	FP_CONVERT_HTON (_src.point.z, _dst->point_z, _fpFormat); \
	FP_CONVERT_HTON (_src.vector.x, _dst->vector_x, _fpFormat); \
	FP_CONVERT_HTON (_src.vector.y, _dst->vector_y, _fpFormat); \
	FP_CONVERT_HTON (_src.vector.z, _dst->vector_z, _fpFormat); \
    } \
    else \
    { \
	_dst->point_x = _src.point.x; \
	_dst->point_y = _src.point.y; \
	_dst->point_z = _src.point.z; \
	_dst->vector_x = _src.vector.x; \
	_dst->vector_y = _src.vector.y; \
	_dst->vector_z = _src.vector.z; \
    }


#define STORE_LISTOF_HALFSPACE2D(_count, _pList, _pBuf, _fpConvert, _fpFormat)\
    STORE_LISTOF_FOOFP (pexHalfSpace2D, _count, _pList, _pBuf, \
	_fpConvert, _fpFormat);

#define DOSTORE_pexHalfSpace2D(_src, _dst, _fpConvert, _fpFormat) \
    if (_fpConvert) \
    { \
	FP_CONVERT_HTON (_src.point.x, _dst->point_x, _fpFormat); \
	FP_CONVERT_HTON (_src.point.y, _dst->point_y, _fpFormat); \
	FP_CONVERT_HTON (_src.vector.x, _dst->vector_x, _fpFormat); \
	FP_CONVERT_HTON (_src.vector.y, _dst->vector_y, _fpFormat); \
    } \
    else \
    { \
	_dst->point_x = _src.point.x; \
	_dst->point_y = _src.point.y; \
	_dst->vector_x = _src.vector.x; \
	_dst->vector_y = _src.vector.y; \
    }


/* ------------------------------------------------------------------------ */
/* COLOR VALUE and COLOR SPECIFIER					    */
/* ------------------------------------------------------------------------ */

#define STORE_COLOR_VAL(_colType, _colVal, _pBuf, _fpConvert, _fpFormat) \
{ \
    if (!_fpConvert) \
    { \
	int sizeColor = GetColorSize (_colType); \
        memcpy (_pBuf, &(_colVal), sizeColor); \
	_pBuf += sizeColor; \
    } \
    else \
    { \
        switch (_colType) \
        { \
        case PEXColorTypeIndexed: \
\
	    STORE_CARD16 (_colVal.indexed.index, _pBuf); \
	    _pBuf += 2; \
	    break; \
\
        case PEXColorTypeRGB: \
        case PEXColorTypeCIE: \
        case PEXColorTypeHSV: \
        case PEXColorTypeHLS: \
\
	    FP_CONVERT_HTON_BUFF (_colVal.rgb.red, _pBuf, _fpFormat); \
	    _pBuf += SIZEOF (float); \
	    FP_CONVERT_HTON_BUFF (_colVal.rgb.green, _pBuf, _fpFormat); \
	    _pBuf += SIZEOF (float); \
	    FP_CONVERT_HTON_BUFF (_colVal.rgb.blue, _pBuf, _fpFormat); \
	    _pBuf += SIZEOF (float); \
	    break; \
\
        case PEXColorTypeRGB8: \
\
	    memcpy (_pBuf, &(_colVal.rgb8), 4); \
	    _pBuf += 4; \
	    break; \
\
        case PEXColorTypeRGB16: \
\
	    STORE_CARD16 (_colVal.rgb16.red, _pBuf); \
	    STORE_CARD16 (_colVal.rgb16.green, _pBuf); \
	    STORE_CARD16 (_colVal.rgb16.blue, _pBuf); \
	    _pBuf += 2; \
	    break; \
	} \
    } \
}


#define STORE_LISTOF_COLOR_VAL(_count, _colType, _pList, _pBuf, _fpConvert, _fpFormat) \
{ \
    if (!_fpConvert) \
    { \
	int bytes = _count * GetColorSize (_colType); \
        memcpy (_pBuf, _pList.indexed, bytes); \
	_pBuf += bytes; \
    } \
    else \
    { \
        _PEXStoreListOfColor (_count, _colType, _pList, &(_pBuf), _fpFormat); \
    } \
}


#define STORE_COLOR_SPEC(_colSpec, _pBuf, _fpConvert, _fpFormat) \
{ \
    STORE_INT16 (_colSpec.type, _pBuf); \
    _pBuf += 2; \
    STORE_COLOR_VAL (_colSpec.type, _colSpec.value, _pBuf, \
	_fpConvert, _fpFormat); \
}


#define STORE_LISTOF_COLOR_SPEC(_count, _pList, _pBuf, _fpConvert, _fpFormat) \
{ \
    int _i; \
    for (_i = 0; _i < _count; _i++) \
    { \
        STORE_COLOR_SPEC (_pList[_i], _pBuf, _fpConvert, _fpFormat); \
    } \
}


/* ------------------------------------------------------------------------ */
/* FACET data								    */
/* ------------------------------------------------------------------------ */

#define STORE_FACET(_colorType, _facetAttr, _facetData, _pBuf, _fpConvert, _fpFormat) \
{ \
    if (_fpConvert) \
    { \
        _PEXStoreFacet (_colorType, _facetAttr, &_facetData, \
	    &(_pBuf), _fpFormat);\
    } \
    else \
    { \
	int lenofFacet = GetFacetDataLength (_facetAttr, \
	    GetColorLength (_colorType)); \
        memcpy (_pBuf, &_facetData, NUMBYTES (lenofFacet)); \
        _pBuf += NUMBYTES (lenofFacet); \
    } \
}


#define STORE_LISTOF_FACET(_count, _facetSize, _colorType, _facetAttr, _facetData, _pBuf, _fpConvert, _fpFormat) \
{ \
    if (_fpConvert) \
    { \
        _PEXStoreListOfFacet (_count, _colorType, \
	    _facetAttr, _facetData, &(_pBuf), _fpFormat); \
    } \
    else \
    { \
	int bytes = _count * _facetSize; \
        memcpy (_pBuf, _facetData.index, bytes); \
        _pBuf += bytes; \
    } \
}


/* ------------------------------------------------------------------------ */
/* VERTEX data								    */
/* ------------------------------------------------------------------------ */

#define STORE_LISTOF_VERTEX(_count, _vertexSize, _colorType, _vertexAttr, _vertexData, _pBuf, _fpConvert, _fpFormat) \
{ \
    if (_fpConvert) \
    { \
        _PEXStoreListOfVertex (_count, _colorType, \
	    _vertexAttr, _vertexData, &(_pBuf), _fpFormat); \
    } \
    else \
    { \
	int bytes = _count * _vertexSize; \
        memcpy (_pBuf, _vertexData.no_data, bytes); \
        _pBuf += bytes; \
    } \
}


/* ------------------------------------------------------------------------ */
/* PICK RECORD								    */
/* ------------------------------------------------------------------------ */

#ifndef WORD64

#define STORE_PICK_RECORD(_type, _numBytes, _pickRec, _pBuf, _fpConvert, _fpFormat) \
{ \
    if (_fpConvert) \
    { \
        DOSTORE_PICK_RECORD (_type, _numBytes, _pickRec, _pBuf, \
	    _fpConvert, _fpFormat); \
    } \
    else \
    { \
	memcpy (_pBuf, _pickRec, _numBytes); \
    } \
    _pBuf += _numBytes; \
}

#else /* WORD64 */

#define STORE_PICK_RECORD(_type, _numBytes, _pickRec, _pBuf, _fpConvert, _fpFormat) \
{ \
    DOSTORE_PICK_RECORD (_type, _numBytes, _pickRec, _pBuf, \
	_fpConvert, _fpFormat); \
    _pBuf += _numBytes; \
}

#endif /* WORD64 */


#define DOSTORE_PICK_RECORD(_type, _numBytes, _pickRec, _pBuf, _fpConvert, _fpFormat) \
{ \
    pexPD_DC_HitBox	dc_box; \
    pexPD_NPC_HitVolume	npc_vol; \
    char 		*recPtr; \
\
    if (_type == PEXPickDeviceDCHitBox) \
    { \
	recPtr = (char *) &dc_box; \
	dc_box.position_x = _pickRec->box.position.x; \
	dc_box.position_y = _pickRec->box.position.y; \
	if (_fpConvert) \
	{ \
	    FP_CONVERT_HTON (_pickRec->box.distance, \
		dc_box.distance, fpFormat); \
	} \
	else \
	    dc_box.distance = _pickRec->box.distance; \
    } \
    else if (_type == PEXPickDeviceNPCHitVolume) \
    { \
	recPtr = (char *) &npc_vol; \
	if (_fpConvert) \
	{ \
	    FP_CONVERT_HTON (_pickRec->volume.min.x, npc_vol.xmin, _fpFormat);\
	    FP_CONVERT_HTON (_pickRec->volume.min.y, npc_vol.ymin, _fpFormat);\
	    FP_CONVERT_HTON (_pickRec->volume.min.z, npc_vol.zmin, _fpFormat);\
	    FP_CONVERT_HTON (_pickRec->volume.max.x, npc_vol.xmax, _fpFormat);\
	    FP_CONVERT_HTON (_pickRec->volume.max.y, npc_vol.ymax, _fpFormat);\
	    FP_CONVERT_HTON (_pickRec->volume.max.z, npc_vol.zmax, _fpFormat);\
	} \
	else \
	{ \
	    npc_vol.xmin = _pickRec->volume.min.x; \
	    npc_vol.ymin = _pickRec->volume.min.y; \
	    npc_vol.zmin = _pickRec->volume.min.z; \
	    npc_vol.xmax = _pickRec->volume.max.x; \
	    npc_vol.ymax = _pickRec->volume.max.y; \
	    npc_vol.zmax = _pickRec->volume.max.z; \
	} \
    } \
    memcpy (_pBuf, recPtr, _numBytes); \
}


/* ------------------------------------------------------------------------ */
/* Mono Encoded Strings							    */
/* ------------------------------------------------------------------------ */

#define STORE_MONOENCODING(_enc, _pBuf) \
    STORE_FOO (pexMonoEncoding, _enc, _pBuf)

#define PUT_TEMP_pexMonoEncoding(_src, _dst) \
    _dst.characterSet = _src.character_set; \
    _dst.characterSetWidth = _src.character_set_width; \
    _dst.encodingState = _src.encoding_state;   \
    _dst.numChars = _src.length;

#define STORE_LISTOF_MONOENCODING(_count, _pList, _pBuf) \
{ \
    PEXEncodedTextData  *nextString; \
    int 		size, _i; \
\
    nextString = _pList; \
    for (_i = 0; _i < _count; _i++, nextString++) \
    { \
        STORE_MONOENCODING ((*nextString), _pBuf); \
\
	if (nextString->character_set_width == PEXCSLong) \
	    size = nextString->length * SIZEOF (long); \
	else if (nextString->character_set_width == PEXCSShort) \
	    size = nextString->length * SIZEOF (short); \
	else /* nextString->character_set_width == PEXCSByte) */ \
	    size = nextString->length; \
\
        memcpy (_pBuf, nextString->ch, size); \
	_pBuf += PADDED_BYTES (size); \
    } \
}


