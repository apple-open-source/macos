/* $Xorg: pl_extract.h,v 1.4 2001/02/09 02:03:27 xorgcvs Exp $ */
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

#define GET_CARD32(_pBuf, _val) \
    _val = *((CARD32 *) _pBuf)

#define GET_CARD16(_pBuf, _val) \
    _val = *((CARD16 *) _pBuf)

#define GET_INT32(_pBuf, _val) \
    _val = *((INT32 *) _pBuf)

#define GET_INT16(_pBuf, _val) \
    _val = *((INT16 *) _pBuf)

#else /* WORD64 */

#define GET_CARD32(_pBuf, _val) \
    CARD32_TO_64 (_pBuf, _val)

#define GET_CARD16(_pBuf, _val) \
    CARD16_TO_64 (_pBuf, _val)

#define GET_INT32(_pBuf, _val) \
    INT32_TO_64 (_pBuf, _val)

#define GET_INT16(_pBuf, _val) \
    INT16_TO_64 (_pBuf, _val)

#endif /* WORD64 */


#define EXTRACT_CARD32(_pBuf, _val) \
{ \
    GET_CARD32 (_pBuf, _val); \
    _pBuf += SIZEOF (CARD32); \
}

#define EXTRACT_CARD16(_pBuf, _val) \
{ \
    GET_CARD16 (_pBuf, _val); \
    _pBuf += SIZEOF (CARD16); \
}

#define EXTRACT_INT32(_pBuf, _val) \
{ \
    GET_INT32 (_pBuf, _val); \
    _pBuf += SIZEOF (INT32); \
}

#define EXTRACT_INT16(_pBuf, _val) \
{ \
    GET_INT16 (_pBuf, _val); \
    _pBuf += SIZEOF (INT16); \
}

/* ------------------------------------------------------------------------ */

#ifndef WORD64

#define EXTRACT_LISTOF_CARD32(_count, _pBuf, _pList) \
{ \
    memcpy (_pList, _pBuf, _count * SIZEOF (CARD32)); \
    _pBuf += (_count * SIZEOF (CARD32)); \
}

#define EXTRACT_LISTOF_CARD16(_count, _pBuf, _pList) \
{ \
    memcpy (_pList, _pBuf, _count * SIZEOF (CARD16)); \
    _pBuf += (_count * SIZEOF (CARD16)); \
}

#define EXTRACT_LISTOF_INT32(_count, _pBuf, _pList) \
{ \
    memcpy (_pList, _pBuf, _count * SIZEOF (INT32)); \
    _pBuf += (_count * SIZEOF (INT32)); \
}

#define EXTRACT_LISTOF_INT16(_count, _pBuf, _pList) \
{ \
    memcpy (_pList, _pBuf, _count * SIZEOF (INT16)); \
    _pBuf += (_count * SIZEOF (INT16)); \
}

#else /* WORD64 */

#define EXTRACT_LISTOF_CARD32(_count, _pBuf, _pList) \
{ \
    int _i; \
    for (_i = 0; _i < _count; _i++) \
        EXTRACT_CARD32 (_pBuf, _pList[_i]); \
}

#define EXTRACT_LISTOF_CARD16(_count, _pBuf, _pList) \
{ \
    int _i; \
    for (_i = 0; _i < _count; _i++) \
        EXTRACT_CARD16 (_pBuf, _pList[_i]); \
}

#define EXTRACT_LISTOF_INT32(_count, _pBuf, _pList) \
{ \
    int _i; \
    for (_i = 0; _i < _count; _i++) \
        EXTRACT_INT32 (_pBuf, _pList[_i]); \
}

#define EXTRACT_LISTOF_INT16(_count, _pBuf, _pList) \
{ \
    int _i; \
    for (_i = 0; _i < _count; _i++) \
        EXTRACT_INT16 (_pBuf, _pList[_i]); \
}

#endif /* WORD64 */

/* ------------------------------------------------------------------------ */

/* List of Values - all values extracted from 4 byte field */

#ifndef WORD64

#define EXTRACT_LOV_CARD16(_pBuf, _val) \
    EXTRACT_CARD32 (_pBuf, _val);

#define EXTRACT_LOV_INT16(_pBuf, _val) \
    EXTRACT_CARD32 (_pBuf, _val);

#else /* WORD64 */

#define EXTRACT_LOV_CARD16(_pBuf, _val) \
{ \
    _pBuf += 2; \
    EXTRACT_CARD16 (_pBuf, _val); \
}

#define EXTRACT_LOV_INT16(_pBuf, _val) \
{ \
    _pBuf += 2; \
    EXTRACT_INT16 (_pBuf, _val); \
}

#endif /* WORD64 */


#define EXTRACT_LOV_CARD8(_pBuf, _val) \
    EXTRACT_CARD32 (_pBuf, _val);


/* ------------------------------------------------------------------------ */

#ifndef WORD64

#define EXTRACT_FLOAT32(_pBuf, _val, _fpConvert, _fpFormat) \
{ \
    if (!_fpConvert) \
    { \
        _val = *((float *) _pBuf); \
    } \
    else \
    { \
        FP_CONVERT_NTOH_BUFF (_pBuf, _val, _fpFormat); \
    } \
    _pBuf += SIZEOF (float); \
}

#define EXTRACT_LISTOF_FLOAT32(_count, _pBuf, _pList, _fpConvert, _fpFormat) \
{ \
    if (!_fpConvert) \
    { \
        memcpy (_pList, _pBuf, _count * SIZEOF (float)); \
        _pBuf += (_count * SIZEOF (float)); \
    } \
    else \
    { \
        int _i; \
        float *fptr = (float *) _pList; \
\
        for (_i = 0; _i < (int) _count; _i++) \
	{ \
	    FP_CONVERT_NTOH_BUFF (_pBuf, *fptr, _fpFormat); \
	    _pBuf += SIZEOF (float); \
	    fptr++; \
        } \
    }\
}

#else /* WORD64 */

#define EXTRACT_FLOAT32(_pBuf, _val, _fpConvert, _fpFormat) \
{ \
    FP_CONVERT_NTOH_BUFF (_pBuf, _val, _fpFormat); \
    _pBuf += SIZEOF (float); \
}

#define EXTRACT_LISTOF_FLOAT32(_count, _pBuf, _pList, _fpConvert, _fpFormat) \
{ \
    int _i; \
    float *fptr = (float *) _pList; \
\
    for (_i = 0; _i < (int) _count; _i++) \
    { \
	FP_CONVERT_NTOH_BUFF (_pBuf, *fptr, _fpFormat); \
	_pBuf += SIZEOF (float); \
	fptr++; \
    } \
}

#endif /* WORD64 */



/* ------------------------------------------------------------------------ */
/*   EXTRACT_FOO and EXTRACT_LISTOF_FOO					    */
/*   (where FOO has no floating point values in it)	    		    */
/* ------------------------------------------------------------------------ */


#if (defined(__STDC__) && !defined(UNIXCPP)) || defined(ANSICPP)
#define GET_TEMP(_pexType, _tFoo, _foo) GET_TEMP_##_pexType (_tFoo, _foo);
#else
#define GET_TEMP(_pexType, _tFoo, _foo) GET_TEMP_/**/_pexType (_tFoo, _foo);
#endif


#ifndef WORD64

#define EXTRACT_FOO(_pexType, _pBuf, _foo) \
{ \
    memcpy (&(_foo), _pBuf, SIZEOF (_pexType)); \
    _pBuf += SIZEOF (_pexType); \
}

#define EXTRACT_LISTOF_FOO(_pexType, _count, _pBuf, _fooList) \
{ \
    memcpy (_fooList, _pBuf, _count * SIZEOF (_pexType)); \
    _pBuf += (_count * SIZEOF (_pexType)); \
}

#else /* WORD64 */

#define EXTRACT_FOO(_pexType, _pBuf, _foo) \
{ \
    _pexType	tFoo; \
    memcpy (&tFoo, _pBuf, SIZEOF (_pexType)); \
    GET_TEMP(_pexType, tFoo, _foo); \
    _pBuf += SIZEOF (_pexType); \
}

#define EXTRACT_LISTOF_FOO(_pexType, _count, _pBuf, _fooList) \
{ \
    _pexType	tFoo; \
    int		_i; \
\
    for (_i = 0; _i < (int) _count; _i++) \
    { \
        memcpy (&tFoo, _pBuf, SIZEOF (_pexType)); \
        GET_TEMP (_pexType, tFoo, _fooList[_i]); \
        _pBuf += SIZEOF (_pexType); \
    } \
}

#endif /* WORD64 */


/*
 * TEXT ALIGNMENT
 */

#define EXTRACT_TEXTALIGN(_pBuf, _align) \
    EXTRACT_FOO (pexTextAlignmentData, _pBuf, _align)

#define GET_TEMP_pexTextAlignmentData(_src, _dst) \
    _dst.vertical   = _src.vertical; \
    _dst.horizontal = _src.horizontal;


/*
 * PSC ISO CURVES 
 */

#define EXTRACT_PSC_ISOCURVES(_pBuf, _isoCurves) \
    EXTRACT_FOO (pexPSC_IsoparametricCurves, _pBuf, _isoCurves)

#define GET_TEMP_pexPSC_IsoparametricCurves(_src, _dst) \
    _dst.placement_type = _src.placementType; \
    _dst.u_count = _src.numUcurves; \
    _dst.v_count = _src.numVcurves;


/*
 * List of ELEMENT REF
 */

#define EXTRACT_LISTOF_ELEMREF(_count, _pBuf, _pList) \
    EXTRACT_LISTOF_FOO (pexElementRef, _count, _pBuf, _pList)

#define GET_TEMP_pexElementRef(_src, _dst) \
    _dst.structure = _src.structure; \
    _dst.offset    = _src.offset;


/*
 * List of ELEMENT INFO
 */

#define EXTRACT_LISTOF_ELEMINFO(_count, _pBuf, _pList) \
    EXTRACT_LISTOF_FOO (pexElementInfo, _count, _pBuf, _pList)

#define GET_TEMP_pexElementInfo(_src, _dst) \
    _dst.type   = _src.elementType; \
    _dst.length = _src.length;


/*
 * List of PICK ELEMENT REF
 */

#define EXTRACT_LISTOF_PICKELEMREF(_count, _pBuf, _pList) \
    EXTRACT_LISTOF_FOO (pexPickElementRef, _count, _pBuf, _pList)

#define GET_TEMP_pexPickElementRef(_src, _dst) \
    _dst.sid    = _src.sid; \
    _dst.offset = _src.offset; \
    _dst.pick_id = _src.pickid;


/*
 * List of DEVICE RECT
 */

#define EXTRACT_LISTOF_DEVRECT(_count, _pBuf, _pList) \
    EXTRACT_LISTOF_FOO (pexDeviceRect, _count, _pBuf, _pList)

#define GET_TEMP_pexDeviceRect(_src, _dst) \
    _dst.xmin = _src.xmin; \
    _dst.ymin = _src.ymin; \
    _dst.xmax = _src.xmax; \
    _dst.ymax = _src.ymax;


/*
 * List of NAME SET PAIR
 */

#define EXTRACT_LISTOF_NAMESET_PAIR(_count, _pBuf, _pList) \
    EXTRACT_LISTOF_FOO (pexNameSetPair, _count, _pBuf, _pList)

#define GET_TEMP_pexNameSetPair(_src, _dst) \
    _dst.inclusion = _src.incl; \
    _dst.exclusion = _src.excl;


/*
 * List of FONT PROP
 */

#define EXTRACT_LISTOF_FONTPROP(_count, _pBuf, _pList) \
    EXTRACT_LISTOF_FOO (pexFontProp, _count, _pBuf, _pList)

#define GET_TEMP_pexFontProp(_src, _dst) \
    _dst.name = _src.name; \
    _dst.value = _src.value;



/* ------------------------------------------------------------------------ */
/*   EXTRACT_FOOFP and EXTRACT_LISTOF_FOOFP    			    	    */
/* ------------------------------------------------------------------------ */

#if (defined(__STDC__) && !defined(UNIXCPP)) || defined(ANSICPP)
#define DOEXTRACT(_pexType, _src, _dst, _fpConvert, _fpFormat) \
    DOEXTRACT_##_pexType (_src, _dst, _fpConvert, _fpFormat)
#else
#define DOEXTRACT(_pexType, _src, _dst, _fpConvert, _fpFormat) \
    DOEXTRACT_/**/_pexType (_src, _dst, _fpConvert, _fpFormat)
#endif


#ifndef WORD64

#define EXTRACT_FOOFP(_pexType, _pBuf, _foo, _fpConvert, _fpFormat) \
{ \
    if (!_fpConvert) \
    { \
        memcpy (&(_foo), _pBuf, SIZEOF (_pexType)); \
    } \
    else \
    { \
        _pexType *pFoo = (_pexType *) _pBuf; \
        DOEXTRACT (_pexType, pFoo, _foo, _fpConvert, _fpFormat); \
    } \
    _pBuf += SIZEOF (_pexType); \
}

#define EXTRACT_LISTOF_FOOFP(_pexType, _count, _pBuf, _fooList, _fpConvert, _fpFormat) \
{ \
    if (!_fpConvert) \
    { \
        memcpy (_fooList, _pBuf, _count * SIZEOF (_pexType)); \
        _pBuf += (_count * SIZEOF (_pexType)); \
    } \
    else \
    { \
        int _i; \
        for (_i = 0; _i < (int) _count; _i++) \
	{ \
	    _pexType *pFoo = (_pexType *) _pBuf; \
            DOEXTRACT (_pexType, pFoo, _fooList[_i], _fpConvert, _fpFormat); \
	    _pBuf += SIZEOF (_pexType); \
        } \
    } \
}

#else /* WORD64 */

#define EXTRACT_FOOFP(_pexType, _pBuf, _foo, _fpConvert, _fpFormat) \
{ \
    _pexType tFoo; \
    _pexType *pFoo = &tFoo; \
    memcpy (pFoo, _pBuf, SIZEOF (_pexType)); \
    DOEXTRACT (_pexType, pFoo, _foo, _fpConvert, _fpFormat); \
    _pBuf += SIZEOF (_pexType); \
}

#define EXTRACT_LISTOF_FOOFP(_pexType, _count, _pBuf, _fooList, _fpConvert, _fpFormat) \
{ \
    int _i; \
    _pexType tFoo; \
    _pexType *pFoo = &tFoo; \
    for (_i = 0; _i < (int) _count; _i++) \
    { \
        memcpy (pFoo, _pBuf, SIZEOF (_pexType)); \
        DOEXTRACT (_pexType, pFoo, _fooList[_i], _fpConvert, _fpFormat); \
        _pBuf += SIZEOF (_pexType); \
    } \
}

#endif /* WORD64 */


/*
 * NPC SUBVOLUME
 */

#define EXTRACT_NPC_SUBVOLUME(_pBuf, _volume, _fpConvert, _fpFormat) \
    EXTRACT_FOOFP (pexNpcSubvolume, _pBuf, _volume, _fpConvert, _fpFormat)

#define DOEXTRACT_pexNpcSubvolume(_src, _dst, _fpConvert, _fpFormat) \
    if (_fpConvert) \
    { \
        FP_CONVERT_NTOH (_src->xmin, _dst.min.x, _fpFormat); \
	FP_CONVERT_NTOH (_src->ymin, _dst.min.y, _fpFormat); \
	FP_CONVERT_NTOH (_src->zmin, _dst.min.z, _fpFormat); \
	FP_CONVERT_NTOH (_src->xmax, _dst.max.x, _fpFormat); \
	FP_CONVERT_NTOH (_src->ymax, _dst.max.y, _fpFormat); \
	FP_CONVERT_NTOH (_src->zmax, _dst.max.z, _fpFormat); \
    } \
    else \
    { \
	_dst.min.x = _src->xmin; \
	_dst.min.y = _src->ymin; \
	_dst.min.z = _src->zmin; \
	_dst.max.x = _src->xmax; \
	_dst.max.y = _src->ymax; \
	_dst.max.z = _src->zmax; \
    }


/*
 * VIEWPORT
 */

#define EXTRACT_VIEWPORT(_pBuf, _viewport, _fpConvert, _fpFormat) \
    EXTRACT_FOOFP (pexViewport, _pBuf, _viewport, _fpConvert, _fpFormat)

#define DOEXTRACT_pexViewport(_src, _dst, _fpConvert, _fpFormat) \
    _dst.min.x = _src->xmin; \
    _dst.min.y = _src->ymin; \
    _dst.max.x = _src->xmax; \
    _dst.max.y = _src->ymax; \
    _dst.use_drawable = _src->useDrawable; \
\
    if (_fpConvert) \
    { \
        FP_CONVERT_NTOH (_src->zmin, _dst.min.z, _fpFormat); \
	FP_CONVERT_NTOH (_src->zmax, _dst.max.z, _fpFormat); \
    } \
    else \
    { \
	_dst.min.z = _src->zmin; \
	_dst.max.z = _src->zmax; \
    }


/*
 * COORD 4D
 */

#define EXTRACT_COORD4D(_pBuf, _coord4D, _fpConvert, _fpFormat) \
    EXTRACT_FOOFP (pexCoord4D, _pBuf, _coord4D, _fpConvert, _fpFormat)


#define EXTRACT_LISTOF_COORD4D(_count, _pBuf, _pList, _fpConvert, _fpFormat)\
    EXTRACT_LISTOF_FOOFP (pexCoord4D, _count, _pBuf, _pList, \
	_fpConvert, _fpFormat);


#define DOEXTRACT_pexCoord4D(_src, _dst, _fpConvert, _fpFormat) \
    if (_fpConvert) \
    { \
        FP_CONVERT_NTOH (_src->x, _dst.x, _fpFormat); \
        FP_CONVERT_NTOH (_src->y, _dst.y, _fpFormat); \
        FP_CONVERT_NTOH (_src->z, _dst.z, _fpFormat); \
        FP_CONVERT_NTOH (_src->w, _dst.w, _fpFormat); \
    } \
    else \
    { \
	_dst.x = _src->x; \
	_dst.y = _src->y; \
	_dst.z = _src->z; \
	_dst.w = _src->w; \
    }


/*
 * COORD 3D
 */

#define EXTRACT_COORD3D(_pBuf, _coord3D, _fpConvert, _fpFormat) \
    EXTRACT_FOOFP (pexCoord3D, _pBuf, _coord3D, _fpConvert, _fpFormat)


#define EXTRACT_LISTOF_COORD3D(_count, _pBuf, _pList, _fpConvert, _fpFormat)\
    EXTRACT_LISTOF_FOOFP (pexCoord3D, _count, _pBuf, _pList, \
	_fpConvert, _fpFormat);


#define DOEXTRACT_pexCoord3D(_src, _dst, _fpConvert, _fpFormat) \
    if (_fpConvert) \
    { \
        FP_CONVERT_NTOH (_src->x, _dst.x, _fpFormat); \
        FP_CONVERT_NTOH (_src->y, _dst.y, _fpFormat); \
        FP_CONVERT_NTOH (_src->z, _dst.z, _fpFormat); \
    } \
    else \
    { \
	_dst.x = _src->x; \
	_dst.y = _src->y; \
	_dst.z = _src->z; \
    }


/*
 * COORD 2D
 */

#define EXTRACT_COORD2D(_pBuf, _coord2D, _fpConvert, _fpFormat) \
    EXTRACT_FOOFP (pexCoord2D, _pBuf, _coord2D, _fpConvert, _fpFormat)


#define EXTRACT_LISTOF_COORD2D(_count, _pBuf, _pList, _fpConvert, _fpFormat)\
    EXTRACT_LISTOF_FOOFP (pexCoord2D, _count, _pBuf, _pList, \
	_fpConvert, _fpFormat);


#define DOEXTRACT_pexCoord2D(_src, _dst, _fpConvert, _fpFormat) \
    if (_fpConvert) \
    { \
        FP_CONVERT_NTOH (_src->x, _dst.x, _fpFormat); \
        FP_CONVERT_NTOH (_src->y, _dst.y, _fpFormat); \
    } \
    else \
    { \
	_dst.x = _src->x; \
	_dst.y = _src->y; \
    }


/*
 * VECTOR 3D
 */

#define EXTRACT_VECTOR3D(_pBuf, _vector3D, _fpConvert, _fpFormat) \
    EXTRACT_FOOFP (pexVector3D, _pBuf, _vector3D, _fpConvert, _fpFormat)


#define DOEXTRACT_pexVector3D(_src, _dst, _fpConvert, _fpFormat) \
    if (_fpConvert) \
    { \
        FP_CONVERT_NTOH (_src->x, _dst.x, _fpFormat); \
        FP_CONVERT_NTOH (_src->y, _dst.y, _fpFormat); \
        FP_CONVERT_NTOH (_src->z, _dst.z, _fpFormat); \
    } \
    else \
    { \
	_dst.x = _src->x; \
	_dst.y = _src->y; \
	_dst.z = _src->z; \
    }


/*
 * VECTOR 2D
 */

#define EXTRACT_VECTOR2D(_pBuf, _vector2D, _fpConvert, _fpFormat) \
    EXTRACT_FOOFP (pexVector2D, _pBuf, _vector2D, _fpConvert, _fpFormat)


#define DOEXTRACT_pexVector2D(_src, _dst, _fpConvert, _fpFormat) \
    if (_fpConvert) \
    { \
        FP_CONVERT_NTOH (_src->x, _dst.x, _fpFormat); \
        FP_CONVERT_NTOH (_src->y, _dst.y, _fpFormat); \
    } \
    else \
    { \
	_dst.x = _src->x; \
	_dst.y = _src->y; \
    }


/*
 * PSC LEVEL CURVES
 */

#define EXTRACT_PSC_LEVELCURVES(_pBuf, _levCurv, _fpConvert, _fpFormat) \
    EXTRACT_FOOFP (pexPSC_LevelCurves, _pBuf, _levCurv, _fpConvert, _fpFormat)


#define DOEXTRACT_pexPSC_LevelCurves(_src, _dst, _fpConvert, _fpFormat) \
    if (_fpConvert) \
    { \
        FP_CONVERT_NTOH (_src->origin_x, _dst.origin.x, _fpFormat); \
        FP_CONVERT_NTOH (_src->origin_y, _dst.origin.y, _fpFormat); \
        FP_CONVERT_NTOH (_src->origin_z, _dst.origin.z, _fpFormat); \
        FP_CONVERT_NTOH (_src->direction_x, _dst.direction.x, _fpFormat); \
        FP_CONVERT_NTOH (_src->direction_y, _dst.direction.y, _fpFormat); \
        FP_CONVERT_NTOH (_src->direction_z, _dst.direction.z, _fpFormat); \
    } \
    else \
    { \
	_dst.origin.x = _src->origin_x; \
	_dst.origin.y = _src->origin_y; \
	_dst.origin.z = _src->origin_z; \
	_dst.direction.x = _src->direction_x; \
	_dst.direction.y = _src->direction_y; \
	_dst.direction.z = _src->direction_z; \
    } \
    _dst.count = _src->numberIntersections;


/*
 * REFLECTION ATTRIBUTES
 */

#define EXTRACT_REFLECTION_ATTR(_pBuf, _reflAttr, _fpConvert, _fpFormat) \
    EXTRACT_FOOFP (pexReflectionAttr, _pBuf, _reflAttr, _fpConvert, _fpFormat)\
    EXTRACT_COLOR_VAL (_pBuf, _reflAttr.specular_color.type, \
	_reflAttr.specular_color.value, _fpConvert, _fpFormat)


#define DOEXTRACT_pexReflectionAttr(_src, _dst, _fpConvert, _fpFormat) \
    if (_fpConvert) \
    { \
        FP_CONVERT_NTOH (_src->ambient, _dst.ambient, _fpFormat); \
        FP_CONVERT_NTOH (_src->diffuse, _dst.diffuse, _fpFormat); \
        FP_CONVERT_NTOH (_src->specular, _dst.specular, _fpFormat); \
        FP_CONVERT_NTOH (_src->specularConc, _dst.specular_conc, _fpFormat); \
        FP_CONVERT_NTOH (_src->transmission, _dst.transmission, _fpFormat); \
    } \
    else \
    { \
	_dst.ambient = _src->ambient; \
	_dst.diffuse = _src->diffuse; \
	_dst.specular = _src->specular; \
	_dst.specular_conc = _src->specularConc; \
	_dst.transmission = _src->transmission; \
    } \
    _dst.specular_color.type = _src->specular_colorType;


/*
 * List of HALF SPACE
 */ 

#define EXTRACT_LISTOF_HALFSPACE3D(_count, _pBuf, _pList, _fpConvert, _fpFormat)\
    EXTRACT_LISTOF_FOOFP (pexHalfSpace, _count, _pBuf, _pList, \
	_fpConvert, _fpFormat);

#define DOEXTRACT_pexHalfSpace(_src, _dst, _fpConvert, _fpFormat) \
    if (_fpConvert) \
    { \
	FP_CONVERT_NTOH (_src->point_x, _dst.point.x, _fpFormat); \
	FP_CONVERT_NTOH (_src->point_y, _dst.point.y, _fpFormat); \
	FP_CONVERT_NTOH (_src->point_z, _dst.point.z, _fpFormat); \
	FP_CONVERT_NTOH (_src->vector_x, _dst.vector.x, _fpFormat); \
	FP_CONVERT_NTOH (_src->vector_y, _dst.vector.y, _fpFormat); \
	FP_CONVERT_NTOH (_src->vector_z, _dst.vector.z, _fpFormat); \
    } \
    else \
    { \
	_dst.point.x = _src->point_x; \
	_dst.point.y = _src->point_y; \
	_dst.point.z = _src->point_z; \
	_dst.vector.x = _src->vector_x; \
	_dst.vector.y = _src->vector_y; \
	_dst.vector.z = _src->vector_z; \
    }

#define EXTRACT_LISTOF_HALFSPACE2D(_count, _pBuf, _pList, _fpConvert, _fpFormat)\
    EXTRACT_LISTOF_FOOFP (pexHalfSpace2D, _count, _pBuf, _pList, \
	_fpConvert, _fpFormat);

#define DOEXTRACT_pexHalfSpace2D(_src, _dst, _fpConvert, _fpFormat) \
    if (_fpConvert) \
    { \
	FP_CONVERT_NTOH (_src->point_x, _dst.point.x, _fpFormat); \
	FP_CONVERT_NTOH (_src->point_y, _dst.point.y, _fpFormat); \
	FP_CONVERT_NTOH (_src->vector_x, _dst.vector.x, _fpFormat); \
	FP_CONVERT_NTOH (_src->vector_y, _dst.vector.y, _fpFormat); \
    } \
    else \
    { \
	_dst.point.x = _src->point_x; \
	_dst.point.y = _src->point_y; \
	_dst.vector.x = _src->vector_x; \
	_dst.vector.y = _src->vector_y; \
    }


/*
 * List of DEVICE COORD
 */

#define EXTRACT_LISTOF_DEVCOORD(_count, _pBuf, _pList, _fpConvert, _fpFormat) \
    EXTRACT_LISTOF_FOOFP (pexDeviceCoord, _count, _pBuf, _pList, \
	 _fpConvert, _fpFormat)

#define DOEXTRACT_pexDeviceCoord(_src, _dst, _fpConvert, _fpFormat) \
    _dst.x = _src->x; \
    _dst.y = _src->y; \
    if (_fpConvert) \
    { \
        FP_CONVERT_NTOH (_src->z, _dst.z, _fpFormat); \
    } \
    else \
	_dst.z = _src->z;


/*
 * List of EXTENT INFO
 */

#define EXTRACT_LISTOF_EXTENT_INFO(_count, _pBuf, _pList, _fpConvert, _fpFormat) \
    EXTRACT_LISTOF_FOOFP (pexExtentInfo, _count, _pBuf, _pList, \
        _fpConvert, _fpFormat)


#define DOEXTRACT_pexExtentInfo(_src, _dst, _fpConvert, _fpFormat) \
    if (_fpConvert) \
    { \
        FP_CONVERT_NTOH (_src->lowerLeft_x, _dst.lower_left.x, _fpFormat); \
        FP_CONVERT_NTOH (_src->lowerLeft_y, _dst.lower_left.y, _fpFormat); \
        FP_CONVERT_NTOH (_src->upperRight_x, _dst.upper_right.x, _fpFormat); \
        FP_CONVERT_NTOH (_src->upperRight_y, _dst.upper_right.y, _fpFormat); \
        FP_CONVERT_NTOH (_src->concatpoint_x, _dst.concat_point.x, _fpFormat);\
        FP_CONVERT_NTOH (_src->concatpoint_y, _dst.concat_point.y, _fpFormat);\
    } \
    else \
    { \
        _dst.lower_left.x = _src->lowerLeft_x; \
        _dst.lower_left.y = _src->lowerLeft_y; \
        _dst.upper_right.x = _src->upperRight_x; \
        _dst.upper_right.y = _src->upperRight_y; \
        _dst.concat_point.x = _src->concatpoint_x; \
        _dst.concat_point.y = _src->concatpoint_y; \
    }


/*
 * List of POSTED STRUCS
 */

#define EXTRACT_LISTOF_POSTED_STRUCS(_count, _pBuf, _pList, _fpConvert, _fpFormat) \
    EXTRACT_LISTOF_FOOFP (pexStructureInfo, _count, _pBuf, _pList, \
	 _fpConvert, _fpFormat)

#define DOEXTRACT_pexStructureInfo(_src, _dst, _fpConvert, _fpFormat) \
    _dst.sid = _src->sid; \
    if (_fpConvert) \
    { \
        FP_CONVERT_NTOH (_src->priority, _dst.priority, _fpFormat); \
    } \
    else \
	_dst.priority = _src->priority;


/* ------------------------------------------------------------------------ */
/* COLOR VALUE and COLOR SPECIFIER					    */
/* ------------------------------------------------------------------------ */

#define EXTRACT_COLOR_VAL(_pBuf, _colType, _colVal, _fpConvert, _fpFormat) \
{ \
    if (!_fpConvert) \
    { \
	int sizeColor = GetColorSize (_colType); \
        memcpy (&(_colVal), _pBuf, sizeColor); \
	_pBuf += sizeColor; \
    } \
    else \
    { \
        switch (_colType) \
        { \
        case PEXColorTypeIndexed: \
\
	    EXTRACT_CARD16 (_pBuf, _colVal.indexed.index); \
	    _pBuf += 2; \
	    break; \
\
        case PEXColorTypeRGB: \
        case PEXColorTypeCIE: \
        case PEXColorTypeHSV: \
        case PEXColorTypeHLS: \
\
	    FP_CONVERT_NTOH_BUFF (_pBuf, _colVal.rgb.red, _fpFormat); \
	    _pBuf += SIZEOF (float); \
	    FP_CONVERT_NTOH_BUFF (_pBuf, _colVal.rgb.green, _fpFormat); \
	    _pBuf += SIZEOF (float); \
	    FP_CONVERT_NTOH_BUFF (_pBuf, _colVal.rgb.blue, _fpFormat); \
	    _pBuf += SIZEOF (float); \
	    break; \
\
        case PEXColorTypeRGB8: \
\
	    memcpy (&(_colVal.rgb8), _pBuf, 4); \
	    _pBuf += 4; \
	    break; \
\
        case PEXColorTypeRGB16: \
\
	    EXTRACT_CARD16 (_pBuf, _colVal.rgb16.red); \
	    EXTRACT_CARD16 (_pBuf, _colVal.rgb16.green); \
	    EXTRACT_CARD16 (_pBuf, _colVal.rgb16.blue); \
	    _pBuf += 2; \
	    break; \
	} \
    } \
}


#define EXTRACT_LISTOF_COLOR_VAL(_count, _pBuf, _colType, _pList, _fpConvert, _fpFormat) \
{ \
    if (!_fpConvert) \
    { \
	int sizeColor = GetColorSize (_colType); \
        memcpy (_pList.indexed, _pBuf, _count * sizeColor); \
	_pBuf += (_count * sizeColor); \
    } \
    else \
    { \
        _PEXExtractListOfColor (_count, &(_pBuf), _colType, \
	    _pList, _fpFormat); \
    } \
}


#define EXTRACT_COLOR_SPEC(_pBuf, _colSpec, _fpConvert, _fpFormat) \
{ \
    EXTRACT_INT16 (_pBuf, _colSpec.type); \
    _pBuf += 2; \
    EXTRACT_COLOR_VAL (_pBuf, _colSpec.type, _colSpec.value, \
	_fpConvert, _fpFormat); \
}


#define EXTRACT_LISTOF_COLOR_SPEC(_count, _pBuf, _pList, _fpConvert, _fpFormat) \
{ \
    int _i; \
    for (_i = 0; _i < _count; _i++) \
    { \
        EXTRACT_COLOR_SPEC (_pBuf, _pList[_i], _fpConvert, _fpFormat); \
    } \
}


/* ------------------------------------------------------------------------ */
/* FONT Info								    */
/* ------------------------------------------------------------------------ */

#ifndef WORD64

#define EXTRACT_FONTINFO(_pBuf, _fontInfo) \
{ \
    pexFontInfo	*pInfo = (pexFontInfo *) _pBuf; \
    _fontInfo.first_glyph   = pInfo->firstGlyph; \
    _fontInfo.last_glyph    = pInfo->lastGlyph; \
    _fontInfo.default_glyph = pInfo->defaultGlyph; \
    _fontInfo.all_exist     = pInfo->allExist; \
    _fontInfo.stroke        = pInfo->strokeFont; \
    _fontInfo.count         = pInfo->numProps; \
    _pBuf += SIZEOF (pexFontInfo); \
}

#else /* WORD64 */

#define EXTRACT_FONTINFO(_pBuf, _fontInfo) \
{ \
    pexFontInfo	tInfo; \
    memcpy (&tInfo, _pBuf, SIZEOF (pexFontInfo)); \
    _fontInfo.first_glyph   = tInfo.firstGlyph; \
    _fontInfo.last_glyph    = tInfo.lastGlyph; \
    _fontInfo.default_glyph = tInfo.defaultGlyph; \
    _fontInfo.all_exist     = tInfo.allExist; \
    _fontInfo.stroke        = tInfo.strokeFont; \
    _fontInfo.count         = tInfo.numProps; \
    _pBuf += SIZEOF (pexFontInfo); \
}

#endif /* WORD64 */


/* ------------------------------------------------------------------------ */
/* List of STRING							    */
/* ------------------------------------------------------------------------ */

#ifndef WORD64

#define EXTRACT_LISTOF_STRING(_count, _pBuf, _pList) \
{ \
    unsigned int length; \
    int	_i; \
\
    for (_i = 0; _i < _count; _i++) \
    { \
	pexString *repStrings = (pexString *) _pBuf; \
	length = repStrings->length; \
        _pList[_i] = (char *) Xmalloc (length + 1); \
	memcpy (_pList[_i], _pBuf + SIZEOF (pexString), length); \
	_pList[_i][length] = '\0'; \
	_pBuf += PADDED_BYTES (SIZEOF (pexString) + length); \
    } \
}

#else /* WORD64 */

#define EXTRACT_LISTOF_STRING(_count, _pBuf, _pList) \
{ \
    pexString tString; \
    unsigned int length; \
    int	_i; \
\
    for (_i = 0; _i < _count; _i++) \
    { \
	memcpy (&tString, _pBuf, SIZEOF (pexString)); \
	length = tString.length; \
        _pList[_i] = (char *) Xmalloc (length + 1); \
	memcpy (_pList[_i], _pBuf + SIZEOF (pexString), length); \
	_pList[_i][length] = '\0'; \
	_pBuf += PADDED_BYTES (SIZEOF (pexString) + length); \
    } \
}

#endif


/* ------------------------------------------------------------------------ */
/* FACET data								    */
/* ------------------------------------------------------------------------ */

#define EXTRACT_FACET(_pBuf, _colorType, _facetAttr, _facetData, _fpConvert, _fpFormat) \
{ \
    if (_fpConvert) \
    { \
        _PEXExtractFacet (&(_pBuf), _colorType, _facetAttr, \
	    &_facetData, _fpFormat); \
    } \
    else \
    { \
	int bytes = GetClientFacetSize (_colorType, _facetAttr); \
        memcpy (&_facetData, _pBuf, bytes); \
        _pBuf += bytes; \
    } \
}


#define EXTRACT_LISTOF_FACET(_count, _pBuf, _facetSize, _colorType, _facetAttr, _facetData, _fpConvert, _fpFormat) \
{ \
    if (_fpConvert) \
    { \
        _PEXExtractListOfFacet (_count, &(_pBuf), _colorType, \
	    _facetAttr, _facetData, _fpFormat); \
    } \
    else \
    { \
        int bytes = _count * _facetSize; \
        memcpy (_facetData.index, _pBuf, bytes); \
        _pBuf += bytes; \
    } \
}


/* ------------------------------------------------------------------------ */
/* VERTEX data								    */
/* ------------------------------------------------------------------------ */

#define EXTRACT_LISTOF_VERTEX(_count, _pBuf, _vertexSize, _colorType, _vertexAttr, _vertexData, _fpConvert, _fpFormat) \
{ \
    if (_fpConvert) \
    { \
        _PEXExtractListOfVertex (_count, &(_pBuf), _colorType, \
	    _vertexAttr, _vertexData, _fpFormat); \
    } \
    else \
    { \
        int bytes = _count * _vertexSize; \
        memcpy (_vertexData.no_data, _pBuf, bytes); \
	_pBuf += bytes; \
    } \
}


/* ------------------------------------------------------------------------ */
/* PICK RECORD								    */
/* ------------------------------------------------------------------------ */

#ifndef WORD64

#define EXTRACT_PICK_RECORD(_pBuf, _type, _numBytes, _pickRec, _fpConvert, _fpFormat) \
{ \
    if (_fpConvert) \
    { \
        DOEXTRACT_PICK_RECORD (_pBuf, _type, _numBytes, _pickRec, \
	    _fpConvert, _fpFormat); \
    } \
    else \
    { \
	memcpy (&(_pickRec), _pBuf, _numBytes); \
    } \
    _pBuf += _numBytes; \
}

#else /* WORD64 */

#define EXTRACT_PICK_RECORD(_pBuf, _type, _numBytes, _pickRec, _fpConvert, _fpFormat) \
{ \
    DOEXTRACT_PICK_RECORD (_pBuf, _type, _numBytes, _pickRec, _fpConvert, _fpFormat);\
    _pBuf += _numBytes; \
}

#endif /* WORD64 */


#define DOEXTRACT_PICK_RECORD(_pBuf, _type, _numBytes, _pickRec, _fpConvert, _fpFormat) \
{ \
    pexPD_DC_HitBox	dc_box; \
    pexPD_NPC_HitVolume	npc_vol; \
\
    if (_type == PEXPickDeviceDCHitBox) \
    { \
        memcpy (&dc_box, _pBuf, _numBytes); \
	_pickRec.box.position.x = dc_box.position_x; \
	_pickRec.box.position.y = dc_box.position_y; \
	if (_fpConvert) \
	{ \
	    FP_CONVERT_NTOH (dc_box.distance, \
	        _pickRec.box.distance, fpFormat); \
	} \
	else \
	    _pickRec.box.distance = dc_box.distance; \
    } \
    else if (_type == PEXPickDeviceNPCHitVolume) \
    { \
        memcpy (&npc_vol, _pBuf, _numBytes); \
	if (_fpConvert) \
	{ \
	    FP_CONVERT_NTOH (npc_vol.xmin, _pickRec.volume.min.x, _fpFormat);\
	    FP_CONVERT_NTOH (npc_vol.ymin, _pickRec.volume.min.y, _fpFormat);\
	    FP_CONVERT_NTOH (npc_vol.zmin, _pickRec.volume.min.z, _fpFormat);\
	    FP_CONVERT_NTOH (npc_vol.xmax, _pickRec.volume.max.x, _fpFormat);\
	    FP_CONVERT_NTOH (npc_vol.ymax, _pickRec.volume.max.y, _fpFormat);\
	    FP_CONVERT_NTOH (npc_vol.zmax, _pickRec.volume.max.z, _fpFormat);\
	} \
	else \
	{ \
	    _pickRec.volume.min.x = npc_vol.xmin; \
	    _pickRec.volume.min.y = npc_vol.ymin; \
	    _pickRec.volume.min.z = npc_vol.zmin; \
	    _pickRec.volume.max.x = npc_vol.xmax; \
	    _pickRec.volume.max.y = npc_vol.ymax; \
	    _pickRec.volume.max.z = npc_vol.zmax; \
	} \
    } \
}


/* ------------------------------------------------------------------------ */
/* Mono Encoded Strings							    */
/* ------------------------------------------------------------------------ */

#define EXTRACT_MONOENCODING(_pBuf, _enc) \
    EXTRACT_FOO (pexMonoEncoding, _pBuf, _enc)

#define GET_TEMP_pexMonoEncoding(_src, _dst) \
    _dst.character_set = _src.characterSet; \
    _dst.character_set_width = _src.characterSetWidth; \
    _dst.encoding_state = _src.encodingState;   \
    _dst.length = _src.numChars;

#define EXTRACT_LISTOF_MONOENCODING(_count, _pBuf, _pList) \
{ \
    PEXEncodedTextData  *nextString; \
    unsigned 		size; \
    int			_i; \
\
    nextString = _pList; \
    for (_i = 0; _i < (int) _count; _i++, nextString++) \
    { \
	EXTRACT_MONOENCODING (_pBuf, (*nextString)); \
\
	if (nextString->character_set_width == PEXCSLong) \
	    size = nextString->length * SIZEOF (long); \
	else if (nextString->character_set_width == PEXCSShort) \
	    size = nextString->length * SIZEOF (short); \
	else /* nextString->character_set_width == PEXCSByte) */ \
	    size = nextString->length; \
\
	nextString->ch = (char *) Xmalloc (size); \
\
        memcpy (nextString->ch, _pBuf, size); \
	_pBuf += PADDED_BYTES (size); \
    } \
}


