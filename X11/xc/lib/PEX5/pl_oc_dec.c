/* $Xorg: pl_oc_dec.c,v 1.4 2001/02/09 02:03:28 xorgcvs Exp $ */
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

#include "PEXlib.h"
#include "PEXlibint.h"
#include "pl_oc_util.h"


PEXOCData *
PEXDecodeOCs (float_format, oc_count, length, encoded_ocs)

INPUT int		float_format;
INPUT unsigned long	oc_count;
INPUT unsigned long	length;
INPUT char		*encoded_ocs;

{
    extern void		(*(PEX_decode_oc_funcs[]))();
    PEXOCData		*ocDest, *ocRet;
    pexElementInfo	*elemInfo;
    char		*ocSrc;
    int			i;


    /*
     * Allocate a buffer to hold the decoded OC data.
     */

    ocRet = (PEXOCData *) Xmalloc ((unsigned) (oc_count * sizeof (PEXOCData)));


    /*
     * Now, decode the OCs.
     */

    ocSrc = encoded_ocs;
    ocDest = ocRet;

    for (i = 0; i < oc_count; i++, ocDest++)
    {
	GET_STRUCT_PTR (pexElementInfo, ocSrc, elemInfo);
	ocDest->oc_type = elemInfo->elementType;
	(*PEX_decode_oc_funcs[elemInfo->elementType]) (float_format,
	    &ocSrc, ocDest);
    }

#ifdef DEBUG
    if (ocSrc - encoded_ocs != length)
    {
	printf ("PEXlib WARNING : Internal error in PEXDecodeOCs :\n");
	printf ("Number of bytes parsed not equal to size of input buffer.\n");
    }
#endif

    return (ocRet);
}


void _PEXDecodeEnumType (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexMarkerType *oc;

    GET_STRUCT_PTR (pexMarkerType, *ocSrc, oc);
    *ocSrc += SIZEOF (pexMarkerType);

    ocDest->data.SetMarkerType.marker_type = oc->markerType;
}


void _PEXDecodeTableIndex (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexMarkerColorIndex *oc;

    GET_STRUCT_PTR (pexMarkerColorIndex, *ocSrc, oc);
    *ocSrc += SIZEOF (pexMarkerColorIndex);

    ocDest->data.SetMarkerColorIndex.index = oc->index;
}


void _PEXDecodeColor (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexMarkerColor	*oc;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    GET_STRUCT_PTR (pexMarkerColor, *ocSrc, oc);
    *ocSrc += SIZEOF (pexMarkerColor);
    
    ocDest->data.SetMarkerColor.color_type = oc->colorType;
    
    EXTRACT_COLOR_VAL (*ocSrc, oc->colorType,
	ocDest->data.SetMarkerColor.color, fpConvert, fpFormat);
}


void _PEXDecodeFloat (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexMarkerScale	*oc;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    GET_STRUCT_PTR (pexMarkerScale, *ocSrc, oc);
    *ocSrc += SIZEOF (pexMarkerScale);

    if (fpConvert)
    {
	FP_CONVERT_NTOH (oc->scale,
	    ocDest->data.SetMarkerScale.scale, fpFormat);
    }
    else
	ocDest->data.SetMarkerScale.scale = oc->scale;
}


void _PEXDecodeCARD16 (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexTextPrecision	*oc;

    GET_STRUCT_PTR (pexTextPrecision, *ocSrc, oc);
    *ocSrc += SIZEOF (pexTextPrecision);

    ocDest->data.SetTextPrecision.precision = oc->precision;
}


void _PEXDecodeVector2D (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexCharUpVector	*oc;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    GET_STRUCT_PTR (pexCharUpVector, *ocSrc, oc);
    *ocSrc += SIZEOF (pexCharUpVector);

    if (fpConvert)
    {
	FP_CONVERT_NTOH (oc->up_x,
	    ocDest->data.SetCharUpVector.vector.x, fpFormat);
	FP_CONVERT_NTOH (oc->up_y,
	    ocDest->data.SetCharUpVector.vector.y, fpFormat);
    }
    else
    {
	ocDest->data.SetCharUpVector.vector.x = oc->up_x;
	ocDest->data.SetCharUpVector.vector.y = oc->up_y;
    }
}


void _PEXDecodeTextAlignment (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexTextAlignment *oc;

    GET_STRUCT_PTR (pexTextAlignment, *ocSrc, oc);
    *ocSrc += SIZEOF (pexTextAlignment);

    ocDest->data.SetTextAlignment.halignment = oc->alignment_horizontal;
    ocDest->data.SetTextAlignment.valignment = oc->alignment_vertical;
}


void _PEXDecodeCurveApprox (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexCurveApprox	*oc;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    GET_STRUCT_PTR (pexCurveApprox, *ocSrc, oc);
    *ocSrc += SIZEOF (pexCurveApprox);
    
    ocDest->data.SetCurveApprox.method = oc->approxMethod;

    if (fpConvert)
    {
	FP_CONVERT_NTOH (oc->tolerance,
	    ocDest->data.SetCurveApprox.tolerance, fpFormat);
    }
    else
	ocDest->data.SetCurveApprox.tolerance = oc->tolerance;
}


void _PEXDecodeReflectionAttr (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    int		fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    *ocSrc += SIZEOF (pexElementInfo);

    EXTRACT_REFLECTION_ATTR (*ocSrc,
	ocDest->data.SetReflectionAttributes.attributes,
	fpConvert, fpFormat);
}


void _PEXDecodeSurfaceApprox (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexSurfaceApprox	*oc;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    GET_STRUCT_PTR (pexSurfaceApprox, *ocSrc, oc);
    *ocSrc += SIZEOF (pexSurfaceApprox);
    
    ocDest->data.SetSurfaceApprox.method = oc->approxMethod;

    if (fpConvert)
    {
	FP_CONVERT_NTOH (oc->uTolerance,
	    ocDest->data.SetSurfaceApprox.utolerance, fpFormat);
	FP_CONVERT_NTOH (oc->vTolerance,
	    ocDest->data.SetSurfaceApprox.vtolerance, fpFormat);
    }
    else
    {
	ocDest->data.SetSurfaceApprox.utolerance = oc->uTolerance;
	ocDest->data.SetSurfaceApprox.vtolerance = oc->vTolerance;
    }
}


void _PEXDecodeCullMode (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexFacetCullingMode *oc;

    GET_STRUCT_PTR (pexFacetCullingMode, *ocSrc, oc);
    *ocSrc += SIZEOF (pexFacetCullingMode);
    
    ocDest->data.SetFacetCullingMode.mode = oc->cullMode;
}


void _PEXDecodeSwitch (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexFacetDistinguishFlag *oc;

    GET_STRUCT_PTR (pexFacetDistinguishFlag, *ocSrc, oc);
    *ocSrc += SIZEOF (pexFacetDistinguishFlag);
    
    ocDest->data.SetFacetDistinguishFlag.flag = oc->distinguish;
}


void _PEXDecodePatternSize (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexPatternSize 	*oc;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    GET_STRUCT_PTR (pexPatternSize, *ocSrc, oc);
    *ocSrc += SIZEOF (pexPatternSize);
    
    if (fpConvert)
    {
	FP_CONVERT_NTOH (oc->size_x,
	    ocDest->data.SetPatternSize.width, fpFormat);
	FP_CONVERT_NTOH (oc->size_y,
	    ocDest->data.SetPatternSize.height, fpFormat);
    }
    else
    {
	ocDest->data.SetPatternSize.width = oc->size_x;
	ocDest->data.SetPatternSize.height = oc->size_y;
    }
}



void _PEXDecodePatternAttr2D (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexPatternAttributes2D 	*oc;
    int				fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    GET_STRUCT_PTR (pexPatternAttributes2D, *ocSrc, oc);
    *ocSrc += SIZEOF (pexPatternAttributes2D);
    
    if (fpConvert)
    {
	FP_CONVERT_NTOH (oc->point_x,
	    ocDest->data.SetPatternAttributes2D.ref_point.x, fpFormat);
	FP_CONVERT_NTOH (oc->point_y,
	    ocDest->data.SetPatternAttributes2D.ref_point.y, fpFormat);
    }
    else
    {
	ocDest->data.SetPatternAttributes2D.ref_point.x = oc->point_x;
	ocDest->data.SetPatternAttributes2D.ref_point.y = oc->point_y;
    }
}


void _PEXDecodePatternAttr (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexPatternAttributes 	*oc;
    int				fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    GET_STRUCT_PTR (pexPatternAttributes, *ocSrc, oc);
    *ocSrc += SIZEOF (pexPatternAttributes);
    
    if (fpConvert)
    {
	FP_CONVERT_NTOH (oc->refPt_x,
	    ocDest->data.SetPatternAttributes.ref_point.x, fpFormat);
	FP_CONVERT_NTOH (oc->refPt_y,
	    ocDest->data.SetPatternAttributes.ref_point.y, fpFormat);
	FP_CONVERT_NTOH (oc->refPt_z,
	    ocDest->data.SetPatternAttributes.ref_point.z, fpFormat);
	FP_CONVERT_NTOH (oc->vector1_x,
	    ocDest->data.SetPatternAttributes.vector1.x, fpFormat);
	FP_CONVERT_NTOH (oc->vector1_y,
	    ocDest->data.SetPatternAttributes.vector1.y, fpFormat);
	FP_CONVERT_NTOH (oc->vector1_z,
	    ocDest->data.SetPatternAttributes.vector1.z, fpFormat);
	FP_CONVERT_NTOH (oc->vector2_x,
	    ocDest->data.SetPatternAttributes.vector2.x, fpFormat);
	FP_CONVERT_NTOH (oc->vector2_y,
	    ocDest->data.SetPatternAttributes.vector2.y, fpFormat);
	FP_CONVERT_NTOH (oc->vector2_z,
	    ocDest->data.SetPatternAttributes.vector2.z, fpFormat);
    }
    else
    {
	ocDest->data.SetPatternAttributes.ref_point.x = oc->refPt_x;
	ocDest->data.SetPatternAttributes.ref_point.y = oc->refPt_y;
	ocDest->data.SetPatternAttributes.ref_point.z = oc->refPt_z;
	ocDest->data.SetPatternAttributes.vector1.x = oc->vector1_x;
	ocDest->data.SetPatternAttributes.vector1.y = oc->vector1_y;
	ocDest->data.SetPatternAttributes.vector1.z = oc->vector1_z;
	ocDest->data.SetPatternAttributes.vector2.x = oc->vector2_x;
	ocDest->data.SetPatternAttributes.vector2.y = oc->vector2_y;
	ocDest->data.SetPatternAttributes.vector2.z = oc->vector2_z;
    }
}


void _PEXDecodeASF (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexIndividualASF *oc;

    GET_STRUCT_PTR (pexIndividualASF, *ocSrc, oc);
    *ocSrc += SIZEOF (pexIndividualASF);
    
    ocDest->data.SetIndividualASF.attribute = oc->attribute;
    ocDest->data.SetIndividualASF.asf = oc->source;
}


void _PEXDecodeLocalTransform (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    int		fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    *ocSrc += SIZEOF (pexElementInfo);

    EXTRACT_CARD16 (*ocSrc, ocDest->data.SetLocalTransform.composition);
    *ocSrc += 2;

    EXTRACT_LISTOF_FLOAT32 (16, *ocSrc,
	ocDest->data.SetLocalTransform.transform, fpConvert, fpFormat);
}


void _PEXDecodeLocalTransform2D (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    int		fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    *ocSrc += SIZEOF (pexElementInfo);

    EXTRACT_CARD16 (*ocSrc, ocDest->data.SetLocalTransform2D.composition);
    *ocSrc += 2;

    EXTRACT_LISTOF_FLOAT32 (9, *ocSrc,
	ocDest->data.SetLocalTransform2D.transform, fpConvert, fpFormat);
}


void _PEXDecodeGlobalTransform (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    int		fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    *ocSrc += SIZEOF (pexElementInfo);

    EXTRACT_LISTOF_FLOAT32 (16, *ocSrc,
	ocDest->data.SetGlobalTransform.transform, fpConvert, fpFormat);
}


void _PEXDecodeGlobalTransform2D (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    int		fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    *ocSrc += SIZEOF (pexElementInfo);

    EXTRACT_LISTOF_FLOAT32 (9, *ocSrc,
	ocDest->data.SetGlobalTransform2D.transform, fpConvert, fpFormat);
}


void _PEXDecodeModelClipVolume (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexModelClipVolume 	*oc;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    GET_STRUCT_PTR (pexModelClipVolume, *ocSrc, oc);
    *ocSrc += SIZEOF (pexModelClipVolume);
    
    ocDest->data.SetModelClipVolume.op = oc->modelClipOperator;
    ocDest->data.SetModelClipVolume.count = oc->numHalfSpaces;

    ocDest->data.SetModelClipVolume.half_spaces = (PEXHalfSpace *)
	Xmalloc ((unsigned) (oc->numHalfSpaces * sizeof (PEXHalfSpace)));

    EXTRACT_LISTOF_HALFSPACE3D (oc->numHalfSpaces, *ocSrc,
	ocDest->data.SetModelClipVolume.half_spaces, fpConvert, fpFormat);
}


void _PEXDecodeModelClipVolume2D (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexModelClipVolume2D	*oc;
    int				fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    GET_STRUCT_PTR (pexModelClipVolume2D, *ocSrc, oc);
    *ocSrc += SIZEOF (pexModelClipVolume2D);
    
    ocDest->data.SetModelClipVolume2D.op = oc->modelClipOperator;
    ocDest->data.SetModelClipVolume2D.count = oc->numHalfSpaces;

    ocDest->data.SetModelClipVolume2D.half_spaces = (PEXHalfSpace2D *)
	Xmalloc ((unsigned) (oc->numHalfSpaces * sizeof (PEXHalfSpace2D)));

    EXTRACT_LISTOF_HALFSPACE2D (oc->numHalfSpaces, *ocSrc,
	ocDest->data.SetModelClipVolume2D.half_spaces, fpConvert, fpFormat);
}


void _PEXDecodeRestoreModelClip (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    /* no data to decode */

    *ocSrc += SIZEOF (pexRestoreModelClipVolume);
}


void _PEXDecodeLightSourceState (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexLightSourceState 	*oc;

    GET_STRUCT_PTR (pexLightSourceState, *ocSrc, oc);
    *ocSrc += SIZEOF (pexLightSourceState);
    
    ocDest->data.SetLightSourceState.enable_count = oc->numEnable;
    ocDest->data.SetLightSourceState.disable_count = oc->numDisable;
    
    ocDest->data.SetLightSourceState.enable = (PEXTableIndex *)
	Xmalloc ((unsigned) (oc->numEnable * sizeof (PEXTableIndex)));

    ocDest->data.SetLightSourceState.disable = (PEXTableIndex *)
	Xmalloc ((unsigned) (oc->numDisable * sizeof (PEXTableIndex)));

    EXTRACT_LISTOF_CARD16 (oc->numEnable, *ocSrc,
	ocDest->data.SetLightSourceState.enable);

    if (oc->numEnable & 1)
	*ocSrc += 2;

    EXTRACT_LISTOF_CARD16 (oc->numDisable, *ocSrc,
	ocDest->data.SetLightSourceState.disable);

    if (oc->numDisable & 1)
	*ocSrc += 2;
}


void _PEXDecodeID (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexPickID *oc;

    GET_STRUCT_PTR (pexPickID, *ocSrc, oc);
    *ocSrc += SIZEOF (pexPickID);
    
    ocDest->data.SetPickID.pick_id = oc->pickId;
}


void _PEXDecodePSC (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexParaSurfCharacteristics 	*oc;
    int				fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    GET_STRUCT_PTR (pexParaSurfCharacteristics, *ocSrc, oc);
    *ocSrc += SIZEOF (pexParaSurfCharacteristics);

    ocDest->data.SetParaSurfCharacteristics.psc_type = oc->characteristics;

    switch (oc->characteristics)
    {
    case PEXPSCIsoCurves:
    {
	EXTRACT_PSC_ISOCURVES (*ocSrc,
	   ocDest->data.SetParaSurfCharacteristics.characteristics.iso_curves);
	break;
    }
	
    case PEXPSCMCLevelCurves:
    case PEXPSCWCLevelCurves:
    {
	PEXPSCLevelCurves 	*levelDest = (PEXPSCLevelCurves *)
	 &ocDest->data.SetParaSurfCharacteristics.characteristics.level_curves;

	EXTRACT_PSC_LEVELCURVES (*ocSrc, (*levelDest), fpConvert, fpFormat);

	levelDest->parameters = (float *) Xmalloc (
	    (unsigned) (sizeof (float) * levelDest->count));

	EXTRACT_LISTOF_FLOAT32 (levelDest->count, *ocSrc,
	    levelDest->parameters, fpConvert, fpFormat);
	break;
    }
	
    default:
    	*ocSrc += PADDED_BYTES (oc->length);
	break;
    }
}


void _PEXDecodeNameSet (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexElementInfo	*elemInfo;
    unsigned		count;

    GET_STRUCT_PTR (pexElementInfo, *ocSrc, elemInfo);
    *ocSrc += SIZEOF (pexElementInfo);

    ocDest->data.AddToNameSet.count = count = elemInfo->length - 1;

    ocDest->data.AddToNameSet.names =
	(PEXName *) Xmalloc (count * sizeof (PEXName));

    EXTRACT_LISTOF_CARD32 (count, *ocSrc, ocDest->data.AddToNameSet.names);
}


void _PEXDecodeExecuteStructure (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexExecuteStructure *oc;

    GET_STRUCT_PTR (pexExecuteStructure, *ocSrc, oc);
    *ocSrc += SIZEOF (pexExecuteStructure);

    ocDest->data.ExecuteStructure.structure = oc->id;
}


void _PEXDecodeLabel (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexLabel *oc;

    GET_STRUCT_PTR (pexLabel, *ocSrc, oc);
    *ocSrc += SIZEOF (pexLabel);

    ocDest->data.Label.label = oc->label;
}


void _PEXDecodeApplicationData (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexApplicationData *oc;

    GET_STRUCT_PTR (pexApplicationData, *ocSrc, oc);
    *ocSrc += SIZEOF (pexApplicationData);
    
    ocDest->data.ApplicationData.length = oc->numElements;
    ocDest->data.ApplicationData.data =
	(PEXPointer) Xmalloc ((unsigned) oc->numElements);
    
    memcpy (ocDest->data.ApplicationData.data, *ocSrc, oc->numElements);
    *ocSrc += PADDED_BYTES (oc->numElements);
}


void _PEXDecodeGSE (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexGSE *oc;

    GET_STRUCT_PTR (pexGSE, *ocSrc, oc);
    *ocSrc += SIZEOF (pexGSE);
    
    ocDest->data.GSE.id = oc->id;
    ocDest->data.GSE.length = oc->numElements;
    ocDest->data.GSE.data = (char *) Xmalloc ((unsigned) oc->numElements);
    
    memcpy (ocDest->data.GSE.data, *ocSrc, oc->numElements);
    *ocSrc += PADDED_BYTES (oc->numElements);
}


void _PEXDecodeMarkers (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexMarkers 		*oc;
    unsigned		count;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    GET_STRUCT_PTR (pexMarkers, *ocSrc, oc);
    *ocSrc += SIZEOF (pexMarkers);
    
    ocDest->data.Markers.count = count =
	(SIZEOF (CARD32) * ((int) oc->oc_length - 1)) / SIZEOF (pexCoord3D);
    
    ocDest->data.Markers.points =
	(PEXCoord *) Xmalloc (count * sizeof (PEXCoord));

    EXTRACT_LISTOF_COORD3D (count, *ocSrc,
	ocDest->data.Markers.points, fpConvert, fpFormat);
}


void _PEXDecodeMarkers2D (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexMarkers2D	*oc;
    unsigned		count;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    GET_STRUCT_PTR (pexMarkers2D, *ocSrc, oc);
    *ocSrc += SIZEOF (pexMarkers2D);
    
    ocDest->data.Markers2D.count = count =
	(SIZEOF (CARD32) * ((int) oc->oc_length - 1)) / SIZEOF (pexCoord2D);
    
    ocDest->data.Markers2D.points =
	(PEXCoord2D *) Xmalloc (count * sizeof (PEXCoord2D));

    EXTRACT_LISTOF_COORD2D (count, *ocSrc,
	ocDest->data.Markers2D.points, fpConvert, fpFormat);
}


void _PEXDecodePolyline (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexPolyline 	*oc;
    unsigned		count;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    GET_STRUCT_PTR (pexPolyline, *ocSrc, oc);
    *ocSrc += SIZEOF (pexPolyline);
    
    ocDest->data.Polyline.count = count =
	(SIZEOF (CARD32) * ((int) oc->oc_length - 1)) / SIZEOF (pexCoord3D);
    
    ocDest->data.Polyline.points =
	(PEXCoord *) Xmalloc (count * sizeof (PEXCoord));

    EXTRACT_LISTOF_COORD3D (count, *ocSrc,
	ocDest->data.Polyline.points, fpConvert, fpFormat);
}


void _PEXDecodePolyline2D (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexPolyline2D	*oc;
    unsigned		count;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    GET_STRUCT_PTR (pexPolyline2D, *ocSrc, oc);
    *ocSrc += SIZEOF (pexPolyline2D);
    
    ocDest->data.Polyline2D.count = count =
	(SIZEOF (CARD32) * ((int) oc->oc_length - 1)) / SIZEOF (pexCoord2D);
    
    ocDest->data.Polyline2D.points =
	(PEXCoord2D *) Xmalloc (count * sizeof (PEXCoord2D));

    EXTRACT_LISTOF_COORD2D (count, *ocSrc,
	ocDest->data.Polyline2D.points, fpConvert, fpFormat);
}


void _PEXDecodeText (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    /* Text is always mono encoded */

    pexText 		*oc;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    GET_STRUCT_PTR (pexText, *ocSrc, oc);
    *ocSrc += SIZEOF (pexText);

    if (fpConvert)
    {
	FP_CONVERT_NTOH (oc->origin_x,
	    ocDest->data.EncodedText.origin.x, fpFormat);
	FP_CONVERT_NTOH (oc->origin_y,
	    ocDest->data.EncodedText.origin.y, fpFormat);
	FP_CONVERT_NTOH (oc->origin_z,
	    ocDest->data.EncodedText.origin.z, fpFormat);
	FP_CONVERT_NTOH (oc->vector1_x,
	    ocDest->data.EncodedText.vector1.x, fpFormat);
	FP_CONVERT_NTOH (oc->vector1_y,
	    ocDest->data.EncodedText.vector1.y, fpFormat);
	FP_CONVERT_NTOH (oc->vector1_z,
	    ocDest->data.EncodedText.vector1.z, fpFormat);
	FP_CONVERT_NTOH (oc->vector2_x,
	    ocDest->data.EncodedText.vector2.x, fpFormat);
	FP_CONVERT_NTOH (oc->vector2_y,
	    ocDest->data.EncodedText.vector2.y, fpFormat);
	FP_CONVERT_NTOH (oc->vector2_z,
	    ocDest->data.EncodedText.vector2.z, fpFormat);
    }
    else
    {
	ocDest->data.EncodedText.origin.x = oc->origin_x;
	ocDest->data.EncodedText.origin.y = oc->origin_y;
	ocDest->data.EncodedText.origin.z = oc->origin_z;
	ocDest->data.EncodedText.vector1.x = oc->vector1_x;
	ocDest->data.EncodedText.vector1.y = oc->vector1_y;
	ocDest->data.EncodedText.vector1.z = oc->vector1_z;
	ocDest->data.EncodedText.vector2.x = oc->vector2_x;
	ocDest->data.EncodedText.vector2.y = oc->vector2_y;
	ocDest->data.EncodedText.vector2.z = oc->vector2_z;
    }

    ocDest->data.EncodedText.count = oc->numEncodings;

    ocDest->data.EncodedText.encoded_text = (PEXEncodedTextData *)
	Xmalloc ((unsigned) (oc->numEncodings * sizeof (PEXEncodedTextData)));

    EXTRACT_LISTOF_MONOENCODING (oc->numEncodings,
	*ocSrc, ocDest->data.EncodedText.encoded_text);
}


void _PEXDecodeText2D (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    /* Text is always mono encoded */

    pexText2D 		*oc;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    GET_STRUCT_PTR (pexText2D, *ocSrc, oc);
    *ocSrc += SIZEOF (pexText2D);

    if (fpConvert)
    {
	FP_CONVERT_NTOH (oc->origin_x,
	    ocDest->data.EncodedText2D.origin.x, fpFormat);
	FP_CONVERT_NTOH (oc->origin_y,
	    ocDest->data.EncodedText2D.origin.y, fpFormat);
    }
    else
    {
	ocDest->data.EncodedText2D.origin.x = oc->origin_x;
	ocDest->data.EncodedText2D.origin.y = oc->origin_y;
    }

    ocDest->data.EncodedText2D.count = oc->numEncodings;

    ocDest->data.EncodedText2D.encoded_text = (PEXEncodedTextData *)
	Xmalloc ((unsigned) (oc->numEncodings * sizeof (PEXEncodedTextData)));

    EXTRACT_LISTOF_MONOENCODING (oc->numEncodings,
	*ocSrc, ocDest->data.EncodedText2D.encoded_text);
}


void _PEXDecodeAnnoText (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    /* Anno Text is always mono encoded */

    pexAnnotationText	*oc;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    GET_STRUCT_PTR (pexAnnotationText, *ocSrc, oc);
    *ocSrc += SIZEOF (pexAnnotationText);

    if (fpConvert)
    {
	FP_CONVERT_NTOH (oc->origin_x,
	    ocDest->data.EncodedAnnoText.origin.x, fpFormat);
	FP_CONVERT_NTOH (oc->origin_y,
	    ocDest->data.EncodedAnnoText.origin.y, fpFormat);
	FP_CONVERT_NTOH (oc->origin_z,
	    ocDest->data.EncodedAnnoText.origin.z, fpFormat);
	FP_CONVERT_NTOH (oc->offset_x,
	    ocDest->data.EncodedAnnoText.offset.x, fpFormat);
	FP_CONVERT_NTOH (oc->offset_y,
	    ocDest->data.EncodedAnnoText.offset.y, fpFormat);
	FP_CONVERT_NTOH (oc->offset_z,
	    ocDest->data.EncodedAnnoText.offset.z, fpFormat);
    }
    else
    {
	ocDest->data.EncodedAnnoText.origin.x = oc->origin_x;
	ocDest->data.EncodedAnnoText.origin.y = oc->origin_y;
	ocDest->data.EncodedAnnoText.origin.z = oc->origin_z;
	ocDest->data.EncodedAnnoText.offset.x = oc->offset_x;
	ocDest->data.EncodedAnnoText.offset.y = oc->offset_y;
	ocDest->data.EncodedAnnoText.offset.z = oc->offset_z;
    }

    ocDest->data.EncodedAnnoText.count = oc->numEncodings;

    ocDest->data.EncodedAnnoText.encoded_text = (PEXEncodedTextData *)
	Xmalloc ((unsigned) (oc->numEncodings *	sizeof (PEXEncodedTextData)));

    EXTRACT_LISTOF_MONOENCODING (oc->numEncodings,
	*ocSrc, ocDest->data.EncodedAnnoText.encoded_text);
}


void _PEXDecodeAnnoText2D (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    /* Anno Text is always mono encoded */

    pexAnnotationText2D	*oc;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    GET_STRUCT_PTR (pexAnnotationText2D, *ocSrc, oc);
    *ocSrc += SIZEOF (pexAnnotationText2D);

    if (fpConvert)
    {
	FP_CONVERT_NTOH (oc->origin_x,
	    ocDest->data.EncodedAnnoText2D.origin.x, fpFormat);
	FP_CONVERT_NTOH (oc->origin_y,
	    ocDest->data.EncodedAnnoText2D.origin.y, fpFormat);
	FP_CONVERT_NTOH (oc->offset_x,
	    ocDest->data.EncodedAnnoText2D.offset.x, fpFormat);
	FP_CONVERT_NTOH (oc->offset_y,
	    ocDest->data.EncodedAnnoText2D.offset.y, fpFormat);
    }
    else
    {
	ocDest->data.EncodedAnnoText2D.origin.x = oc->origin_x;
	ocDest->data.EncodedAnnoText2D.origin.y = oc->origin_y;
	ocDest->data.EncodedAnnoText2D.offset.x = oc->offset_x;
	ocDest->data.EncodedAnnoText2D.offset.y = oc->offset_y;
    }

    ocDest->data.EncodedAnnoText2D.count = oc->numEncodings;

    ocDest->data.EncodedAnnoText2D.encoded_text = (PEXEncodedTextData *)
	Xmalloc ((unsigned) (oc->numEncodings *	sizeof (PEXEncodedTextData)));

    EXTRACT_LISTOF_MONOENCODING (oc->numEncodings,
	*ocSrc, ocDest->data.EncodedAnnoText2D.encoded_text);
}


void _PEXDecodePolylineSet (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexPolylineSetWithData 	*oc;
    PEXListOfVertex		*plset;
    int				fpConvert = (fpFormat != NATIVE_FP_FORMAT);
    int				vertexSize;
    int				i;

    GET_STRUCT_PTR (pexPolylineSetWithData, *ocSrc, oc);
    *ocSrc += SIZEOF (pexPolylineSetWithData);

    ocDest->data.PolylineSetWithData.vertex_attributes = oc->vertexAttribs;
    ocDest->data.PolylineSetWithData.color_type = oc->colorType;
    ocDest->data.PolylineSetWithData.count = oc->numLists;

    ocDest->data.PolylineSetWithData.vertex_lists = plset = (PEXListOfVertex *)
	Xmalloc ((unsigned) (oc->numLists * sizeof (PEXListOfVertex)));

    vertexSize = GetClientVertexSize (oc->colorType, oc->vertexAttribs);

    for (i = 0; i < oc->numLists; i++, plset++)
    {
	EXTRACT_CARD32 (*ocSrc, plset->count);

	plset->vertices.no_data = (PEXCoord *) Xmalloc (
	    (unsigned) (plset->count * vertexSize));

	EXTRACT_LISTOF_VERTEX (plset->count, *ocSrc, vertexSize,
	    oc->colorType, oc->vertexAttribs,
	    plset->vertices, fpConvert, fpFormat);
    }
}


void _PEXDecodeNURBCurve (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexNURBCurve	*oc;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    GET_STRUCT_PTR (pexNURBCurve, *ocSrc, oc);
    *ocSrc += SIZEOF (pexNURBCurve);

    ocDest->data.NURBCurve.rationality = oc->coordType;
    ocDest->data.NURBCurve.order = oc->curveOrder;
    ocDest->data.NURBCurve.count = oc->numPoints;

    if (fpConvert)
    {
	FP_CONVERT_NTOH (oc->tmin, ocDest->data.NURBCurve.tmin, fpFormat);
	FP_CONVERT_NTOH (oc->tmax, ocDest->data.NURBCurve.tmax, fpFormat);
    }
    else
    {
	ocDest->data.NURBCurve.tmin = oc->tmin;
	ocDest->data.NURBCurve.tmax = oc->tmax;
    }

    ocDest->data.NURBCurve.knots =
	(float *) Xmalloc ((unsigned) (oc->numKnots * sizeof (float)));

    EXTRACT_LISTOF_FLOAT32 (oc->numKnots, *ocSrc,
	ocDest->data.NURBCurve.knots, fpConvert, fpFormat);

    if (oc->coordType == PEXRational)
    {
	ocDest->data.NURBCurve.points.point_4d = (PEXCoord4D *) Xmalloc (
	    (unsigned) (oc->numPoints * sizeof (PEXCoord4D)));

	EXTRACT_LISTOF_COORD4D (oc->numPoints, *ocSrc,
	    ocDest->data.NURBCurve.points.point_4d, fpConvert, fpFormat);
    }
    else
    {
	ocDest->data.NURBCurve.points.point = (PEXCoord *) Xmalloc (
	    (unsigned) (oc->numPoints * sizeof (PEXCoord)));

	EXTRACT_LISTOF_COORD3D (oc->numPoints, *ocSrc,
	    ocDest->data.NURBCurve.points.point, fpConvert, fpFormat);
    }
}


void _PEXDecodeFillArea (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexFillArea 	*oc;
    unsigned		count;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    GET_STRUCT_PTR (pexFillArea, *ocSrc, oc);
    *ocSrc += SIZEOF (pexFillArea);
    
    ocDest->data.FillArea.shape_hint = oc->shape;
    ocDest->data.FillArea.ignore_edges = oc->ignoreEdges;
    
    ocDest->data.FillArea.count = count =
	(SIZEOF (CARD32) * ((int) oc->oc_length - 2)) / SIZEOF (pexCoord3D);
    
    ocDest->data.FillArea.points =
	(PEXCoord *) Xmalloc (count * sizeof (PEXCoord));

    EXTRACT_LISTOF_COORD3D (count, *ocSrc,
	ocDest->data.FillArea.points, fpConvert, fpFormat);
}


void _PEXDecodeFillArea2D (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexFillArea2D 	*oc;
    unsigned		count;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    GET_STRUCT_PTR (pexFillArea2D, *ocSrc, oc);
    *ocSrc += SIZEOF (pexFillArea2D);
    
    ocDest->data.FillArea2D.shape_hint = oc->shape;
    ocDest->data.FillArea2D.ignore_edges = oc->ignoreEdges;
    
    ocDest->data.FillArea2D.count = count =
	(SIZEOF (CARD32) * ((int) oc->oc_length - 2)) / SIZEOF (pexCoord2D);
    
    ocDest->data.FillArea2D.points =
	(PEXCoord2D *) Xmalloc (count * sizeof (PEXCoord2D));

    EXTRACT_LISTOF_COORD2D (count, *ocSrc,
	ocDest->data.FillArea2D.points, fpConvert, fpFormat);
}


void _PEXDecodeFillAreaWithData (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexFillAreaWithData	*oc;
    CARD32		count;
    int			vertexSize;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    GET_STRUCT_PTR (pexFillAreaWithData, *ocSrc, oc);
    *ocSrc += SIZEOF (pexFillAreaWithData);
    
    ocDest->data.FillAreaWithData.shape_hint = oc->shape;
    ocDest->data.FillAreaWithData.ignore_edges = oc->ignoreEdges;
    ocDest->data.FillAreaWithData.facet_attributes = oc->facetAttribs;
    ocDest->data.FillAreaWithData.vertex_attributes = oc->vertexAttribs;
    ocDest->data.FillAreaWithData.color_type = oc->colorType;

    if (oc->facetAttribs)
    {
	EXTRACT_FACET (*ocSrc, oc->colorType, oc->facetAttribs,
	    ocDest->data.FillAreaWithData.facet_data, fpConvert, fpFormat);
    }

    EXTRACT_CARD32 (*ocSrc, count);
    ocDest->data.FillAreaWithData.count = count;

    vertexSize = GetClientVertexSize (oc->colorType, oc->vertexAttribs);

    ocDest->data.FillAreaWithData.vertices.no_data =
	(PEXCoord *) Xmalloc ((unsigned) (count * vertexSize));

    EXTRACT_LISTOF_VERTEX (count, *ocSrc, vertexSize,
	oc->colorType, oc->vertexAttribs,
	ocDest->data.FillAreaWithData.vertices, fpConvert, fpFormat);
}


void _PEXDecodeFillAreaSet (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexFillAreaSet 	*oc;
    PEXListOfCoord	*pList;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);
    int			i;

    GET_STRUCT_PTR (pexFillAreaSet, *ocSrc, oc);
    *ocSrc += SIZEOF (pexFillAreaSet);

    ocDest->data.FillAreaSet.shape_hint = oc->shape;
    ocDest->data.FillAreaSet.ignore_edges = oc->ignoreEdges;
    ocDest->data.FillAreaSet.contour_hint = oc->contourHint;
    ocDest->data.FillAreaSet.count = oc->numLists;

    ocDest->data.FillAreaSet.point_lists = pList = (PEXListOfCoord *)
	Xmalloc ((unsigned) (oc->numLists * sizeof (PEXListOfCoord)));

    for (i = 0; i < oc->numLists; i++, pList++)
    {
	EXTRACT_CARD32 (*ocSrc, pList->count);

	pList->points = (PEXCoord *)
	    Xmalloc ((unsigned) (pList->count * sizeof (PEXCoord)));

	EXTRACT_LISTOF_COORD3D (pList->count, *ocSrc,
	    pList->points, fpConvert, fpFormat);
    }
}


void _PEXDecodeFillAreaSet2D (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexFillAreaSet2D 	*oc;
    PEXListOfCoord2D	*pList;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);
    int			i;

    GET_STRUCT_PTR (pexFillAreaSet2D, *ocSrc, oc);
    *ocSrc += SIZEOF (pexFillAreaSet2D);

    ocDest->data.FillAreaSet2D.shape_hint = oc->shape;
    ocDest->data.FillAreaSet2D.ignore_edges = oc->ignoreEdges;
    ocDest->data.FillAreaSet2D.contour_hint = oc->contourHint;
    ocDest->data.FillAreaSet2D.count = oc->numLists;

    ocDest->data.FillAreaSet2D.point_lists = pList = (PEXListOfCoord2D *)
	Xmalloc ((unsigned) (oc->numLists * sizeof (PEXListOfCoord2D)));

    for (i = 0; i < oc->numLists; i++, pList++)
    {
	EXTRACT_CARD32 (*ocSrc, pList->count);

	pList->points = (PEXCoord2D *)
	    Xmalloc ((unsigned) (pList->count * sizeof (PEXCoord2D)));

	EXTRACT_LISTOF_COORD2D (pList->count, *ocSrc,
	    pList->points, fpConvert, fpFormat);
    }
}


void _PEXDecodeFillAreaSetWithData (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexFillAreaSetWithData 	*oc;
    PEXListOfVertex		*pList;
    int				vertexSize, i;
    int				fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    GET_STRUCT_PTR (pexFillAreaSetWithData, *ocSrc, oc);
    *ocSrc += SIZEOF (pexFillAreaSetWithData);

    ocDest->data.FillAreaSetWithData.shape_hint = oc->shape;
    ocDest->data.FillAreaSetWithData.ignore_edges = oc->ignoreEdges;
    ocDest->data.FillAreaSetWithData.contour_hint = oc->contourHint;
    ocDest->data.FillAreaSetWithData.facet_attributes = oc->facetAttribs;
    ocDest->data.FillAreaSetWithData.vertex_attributes = oc->vertexAttribs;
    ocDest->data.FillAreaSetWithData.color_type = oc->colorType;

    if (oc->facetAttribs)
    {
	EXTRACT_FACET (*ocSrc, oc->colorType, oc->facetAttribs,
	    ocDest->data.FillAreaSetWithData.facet_data, fpConvert, fpFormat);
    }

    ocDest->data.FillAreaSetWithData.count = oc->numLists;
    ocDest->data.FillAreaSetWithData.vertex_lists = pList = (PEXListOfVertex *)
	Xmalloc ((unsigned) (oc->numLists * sizeof (PEXListOfVertex)));
    
    vertexSize = GetClientVertexSize (oc->colorType, oc->vertexAttribs);
    if (oc->vertexAttribs & PEXGAEdges)
	vertexSize += sizeof (unsigned int);

    for (i = 0; i < oc->numLists; i++, pList++)
    {
	EXTRACT_CARD32 (*ocSrc, pList->count);

	pList->vertices.no_data = (PEXCoord *) Xmalloc (
	    (unsigned) (pList->count * vertexSize));

	EXTRACT_LISTOF_VERTEX (pList->count, *ocSrc, vertexSize,
	    oc->colorType, oc->vertexAttribs,
	    pList->vertices, fpConvert, fpFormat);
    }
}


void _PEXDecodeTriangleStrip (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexTriangleStrip 	*oc;
    int			facetSize;
    int			vertexSize;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    GET_STRUCT_PTR (pexTriangleStrip, *ocSrc, oc);
    *ocSrc += SIZEOF (pexTriangleStrip);

    ocDest->data.TriangleStrip.facet_attributes = oc->facetAttribs;
    ocDest->data.TriangleStrip.vertex_attributes = oc->vertexAttribs;
    ocDest->data.TriangleStrip.color_type = oc->colorType;
    ocDest->data.TriangleStrip.count = oc->numVertices;

    if (oc->facetAttribs)
    {
	facetSize = GetClientFacetSize (oc->colorType, oc->facetAttribs);

	ocDest->data.TriangleStrip.facet_data.index = (PEXColorIndexed *)
	    Xmalloc ((unsigned) ((oc->numVertices - 2) * facetSize));

	EXTRACT_LISTOF_FACET ((oc->numVertices - 2), *ocSrc, facetSize,
	    oc->colorType, oc->facetAttribs,
	    ocDest->data.TriangleStrip.facet_data, fpConvert, fpFormat);
    }
    else
	ocDest->data.TriangleStrip.facet_data.index = NULL;

    vertexSize = GetClientVertexSize (oc->colorType, oc->vertexAttribs);

    ocDest->data.TriangleStrip.vertices.no_data =
	(PEXCoord *) Xmalloc ((unsigned) (oc->numVertices * vertexSize));

    EXTRACT_LISTOF_VERTEX (oc->numVertices, *ocSrc, vertexSize,
	oc->colorType, oc->vertexAttribs,
	ocDest->data.TriangleStrip.vertices, fpConvert, fpFormat);
}


void _PEXDecodeQuadMesh (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexQuadrilateralMesh 	*oc;
    int				count;
    int				facetSize;
    int				vertexSize;
    int				fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    GET_STRUCT_PTR (pexQuadrilateralMesh, *ocSrc, oc);
    *ocSrc += SIZEOF (pexQuadrilateralMesh);

    ocDest->data.QuadrilateralMesh.shape_hint = oc->shape;
    ocDest->data.QuadrilateralMesh.facet_attributes = oc->facetAttribs;
    ocDest->data.QuadrilateralMesh.vertex_attributes = oc->vertexAttribs;
    ocDest->data.QuadrilateralMesh.color_type = oc->colorType;
    ocDest->data.QuadrilateralMesh.col_count = oc->mPts;
    ocDest->data.QuadrilateralMesh.row_count = oc->nPts;

    if (oc->facetAttribs)
    {
	facetSize = GetClientFacetSize (oc->colorType, oc->facetAttribs);

	count = (oc->mPts - 1) * (oc->nPts - 1);
	ocDest->data.QuadrilateralMesh.facet_data.index =
	    (PEXColorIndexed *) Xmalloc ((unsigned) (count * facetSize));

	EXTRACT_LISTOF_FACET (count, *ocSrc, facetSize,
	    oc->colorType, oc->facetAttribs,
	    ocDest->data.QuadrilateralMesh.facet_data, fpConvert, fpFormat);
    }
    else
	ocDest->data.QuadrilateralMesh.facet_data.index = NULL;

    vertexSize = GetClientVertexSize (oc->colorType, oc->vertexAttribs);

    count = oc->mPts * oc->nPts;
    ocDest->data.QuadrilateralMesh.vertices.no_data =
	(PEXCoord *) Xmalloc ((unsigned) (count * vertexSize));

    EXTRACT_LISTOF_VERTEX (count, *ocSrc, vertexSize,
	oc->colorType, oc->vertexAttribs,
	ocDest->data.QuadrilateralMesh.vertices, fpConvert, fpFormat);
}


void _PEXDecodeSOFA (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexSetOfFillAreaSets	*oc;
    PEXConnectivityData		*pCon;
    PEXListOfUShort		*pList;
    int				cbytes;
    int				facetSize;
    int				vertexSize;
    int				i, j;
    int				fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    GET_STRUCT_PTR (pexSetOfFillAreaSets, *ocSrc, oc);
    *ocSrc += SIZEOF (pexSetOfFillAreaSets);

    ocDest->data.SetOfFillAreaSets.shape_hint = oc->shape;
    ocDest->data.SetOfFillAreaSets.facet_attributes = oc->FAS_Attributes;
    ocDest->data.SetOfFillAreaSets.vertex_attributes = oc->vertexAttributes;
    ocDest->data.SetOfFillAreaSets.edge_attributes =
	(oc->edgeAttributes == PEXOn) ? PEXGAEdges : 0;
    ocDest->data.SetOfFillAreaSets.contour_hint = oc->contourHint;
    ocDest->data.SetOfFillAreaSets.contours_all_one = oc->contourCountsFlag;
    ocDest->data.SetOfFillAreaSets.color_type = oc->colorType;
    ocDest->data.SetOfFillAreaSets.set_count = oc->numFAS;
    ocDest->data.SetOfFillAreaSets.vertex_count = oc->numVertices;
    ocDest->data.SetOfFillAreaSets.index_count = oc->numEdges;

    if (oc->FAS_Attributes)
    {
	facetSize = GetClientFacetSize (oc->colorType, oc->FAS_Attributes);

	ocDest->data.SetOfFillAreaSets.facet_data.index =
	    (PEXColorIndexed *) Xmalloc ((unsigned) (oc->numFAS * facetSize));

	EXTRACT_LISTOF_FACET (oc->numFAS, *ocSrc, facetSize,
	    oc->colorType, oc->FAS_Attributes,
	    ocDest->data.SetOfFillAreaSets.facet_data, fpConvert, fpFormat);
    }
    else
	ocDest->data.SetOfFillAreaSets.facet_data.index = NULL;

    vertexSize = GetClientVertexSize (oc->colorType, oc->vertexAttributes);

    ocDest->data.SetOfFillAreaSets.vertices.no_data =
	(PEXCoord *) Xmalloc ((unsigned) (oc->numVertices * vertexSize));

    EXTRACT_LISTOF_VERTEX (oc->numVertices, *ocSrc, vertexSize,
	oc->colorType, oc->vertexAttributes,
	ocDest->data.SetOfFillAreaSets.vertices, fpConvert, fpFormat);

    if (oc->edgeAttributes)
    {
	unsigned int size = oc->numEdges * sizeof (CARD8);
	ocDest->data.SetOfFillAreaSets.edge_flags =
	    (PEXSwitch *) Xmalloc (size);
	memcpy (ocDest->data.SetOfFillAreaSets.edge_flags, *ocSrc, size);
	*ocSrc += PADDED_BYTES (size);
    }
    else
	ocDest->data.SetOfFillAreaSets.edge_flags = NULL;
	
    ocDest->data.SetOfFillAreaSets.connectivity = pCon =
	(PEXConnectivityData *) Xmalloc ((unsigned) (oc->numFAS *
	sizeof (PEXConnectivityData)));

    for (i = 0; i < (int) oc->numFAS; i++, pCon++)
    {
	EXTRACT_CARD16 (*ocSrc, pCon->count);

	pCon->lists = pList = (PEXListOfUShort *)
	    Xmalloc ((unsigned) (pCon->count * sizeof (PEXListOfUShort)));

	for (j = 0; j < (int) pCon->count; j++, pList++)
	{
	    EXTRACT_CARD16 (*ocSrc, pList->count);

	    pList->shorts = (unsigned short *) Xmalloc (
		(unsigned) (pList->count * sizeof (unsigned short)));

	    EXTRACT_LISTOF_CARD16 (pList->count, *ocSrc, pList->shorts);
	}
    }
	
    cbytes = SIZEOF (CARD16) * (oc->numFAS + oc->numContours + oc->numEdges);
    *ocSrc += PAD (cbytes);
}


void _PEXDecodeNURBSurface (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexNURBSurface	*oc;
    PEXListOfTrimCurve	*pList;
    pexTrimCurve	*trimSrc;
    PEXTrimCurve	*trimDest;
    unsigned		count;
    int			i, j;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    GET_STRUCT_PTR (pexNURBSurface, *ocSrc, oc);
    *ocSrc += SIZEOF (pexNURBSurface);

    ocDest->data.NURBSurface.rationality = oc->type;
    ocDest->data.NURBSurface.uorder = oc->uOrder;
    ocDest->data.NURBSurface.vorder = oc->vOrder;
    ocDest->data.NURBSurface.col_count = oc->mPts;
    ocDest->data.NURBSurface.row_count = oc->nPts;

    count = oc->uOrder + oc->mPts;
    ocDest->data.NURBSurface.uknots =
	(float *) Xmalloc (count * sizeof (float));

    EXTRACT_LISTOF_FLOAT32 (count, *ocSrc,
	ocDest->data.NURBSurface.uknots, fpConvert, fpFormat);

    count = oc->vOrder + oc->nPts;
    ocDest->data.NURBSurface.vknots =
	(float *) Xmalloc (count * sizeof (float));

    EXTRACT_LISTOF_FLOAT32 (count, *ocSrc,
	ocDest->data.NURBSurface.vknots, fpConvert, fpFormat);

    count = oc->mPts * oc->nPts;

    if (oc->type == PEXRational)
    {
	ocDest->data.NURBSurface.points.point_4d =
	    (PEXCoord4D *) Xmalloc (count * sizeof (PEXCoord4D));

	EXTRACT_LISTOF_COORD4D (count, *ocSrc,
	    ocDest->data.NURBSurface.points.point_4d, fpConvert, fpFormat);
    }
    else
    {
	ocDest->data.NURBSurface.points.point =
	    (PEXCoord *) Xmalloc (count * sizeof (PEXCoord));

	EXTRACT_LISTOF_COORD3D (count, *ocSrc,
	    ocDest->data.NURBSurface.points.point, fpConvert, fpFormat);
    }

    ocDest->data.NURBSurface.curve_count = oc->numLists;
    ocDest->data.NURBSurface.trim_curves = pList = (PEXListOfTrimCurve *)
	Xmalloc ((unsigned) (oc->numLists * sizeof (PEXListOfTrimCurve)));

    for (i = 0; i < oc->numLists; i++, pList++)
    {
	EXTRACT_CARD32 (*ocSrc, pList->count);

	pList->curves = trimDest = (PEXTrimCurve *)
	    Xmalloc ((unsigned) (pList->count * sizeof (PEXTrimCurve)));

	for (j = 0; j < (int) pList->count; j++, trimDest++)
	{
	    GET_STRUCT_PTR (pexTrimCurve, *ocSrc, trimSrc);
	    *ocSrc += SIZEOF (pexTrimCurve);

	    trimDest->visibility = trimSrc->visibility;
	    trimDest->order = trimSrc->order;
	    trimDest->rationality = trimSrc->type;
	    trimDest->approx_method = trimSrc->approxMethod;

	    if (fpConvert)
	    {
		FP_CONVERT_NTOH (trimSrc->tolerance,
		    trimDest->tolerance, fpFormat);
		FP_CONVERT_NTOH (trimSrc->tMin, trimDest->tmin, fpFormat);
		FP_CONVERT_NTOH (trimSrc->tMax, trimDest->tmax, fpFormat);
	    }
	    else
	    {
		trimDest->tolerance = trimSrc->tolerance;
		trimDest->tmin = trimSrc->tMin;
		trimDest->tmax = trimSrc->tMax;
	    }

	    count = trimSrc->order + trimSrc->numCoord;
	    trimDest->knots.count = count;
	    trimDest->knots.floats =
		(float *) Xmalloc (count * sizeof (float));

	    EXTRACT_LISTOF_FLOAT32 (count, *ocSrc,
		trimDest->knots.floats, fpConvert, fpFormat);

	    trimDest->count = trimSrc->numCoord;

	    if (trimSrc->type == PEXRational)
	    {
		trimDest->control_points.point = (PEXCoord *) Xmalloc (
		    (unsigned) (trimSrc->numCoord * sizeof (PEXCoord)));

		EXTRACT_LISTOF_COORD3D (trimSrc->numCoord, *ocSrc,
		    trimDest->control_points.point, fpConvert, fpFormat);
	    }
	    else
	    {
		trimDest->control_points.point_2d = (PEXCoord2D *) Xmalloc (
		    (unsigned) (trimSrc->numCoord * sizeof (PEXCoord2D)));

		EXTRACT_LISTOF_COORD2D (trimSrc->numCoord, *ocSrc,
		    trimDest->control_points.point_2d, fpConvert, fpFormat);
	    }
	}
    }
}


void _PEXDecodeCellArray (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexCellArray	*oc;
    unsigned		count;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);


    GET_STRUCT_PTR (pexCellArray, *ocSrc, oc);
    *ocSrc += SIZEOF (pexCellArray);

    if (fpConvert)
    {
	FP_CONVERT_NTOH (oc->point1_x,
	    ocDest->data.CellArray.point1.x, fpFormat);
	FP_CONVERT_NTOH (oc->point1_y,
	    ocDest->data.CellArray.point1.y, fpFormat);
	FP_CONVERT_NTOH (oc->point1_z,
	    ocDest->data.CellArray.point1.z, fpFormat);
	FP_CONVERT_NTOH (oc->point2_x,
	    ocDest->data.CellArray.point2.x, fpFormat);
	FP_CONVERT_NTOH (oc->point2_y,
	    ocDest->data.CellArray.point2.y, fpFormat);
	FP_CONVERT_NTOH (oc->point2_z,
	    ocDest->data.CellArray.point2.z, fpFormat);
	FP_CONVERT_NTOH (oc->point3_x,
	    ocDest->data.CellArray.point3.x, fpFormat);
	FP_CONVERT_NTOH (oc->point3_y,
	    ocDest->data.CellArray.point3.y, fpFormat);
	FP_CONVERT_NTOH (oc->point3_z,
	    ocDest->data.CellArray.point3.z, fpFormat);
    }
    else
    {
	ocDest->data.CellArray.point1.x = oc->point1_x;
	ocDest->data.CellArray.point1.y = oc->point1_y;
	ocDest->data.CellArray.point1.z = oc->point1_z;
	ocDest->data.CellArray.point2.x = oc->point2_x;
	ocDest->data.CellArray.point2.y = oc->point2_y;
	ocDest->data.CellArray.point2.z = oc->point2_z;
	ocDest->data.CellArray.point3.x = oc->point3_x;
	ocDest->data.CellArray.point3.y = oc->point3_y;
	ocDest->data.CellArray.point3.z = oc->point3_z;
    }

    ocDest->data.CellArray.col_count = oc->dx;
    ocDest->data.CellArray.row_count = oc->dy;

    count = oc->dx * oc->dy;
    ocDest->data.CellArray.color_indices =
	(PEXTableIndex *) Xmalloc (count * sizeof (PEXTableIndex));

    EXTRACT_LISTOF_CARD16 (count, *ocSrc,
	ocDest->data.CellArray.color_indices);
	
    if (count & 1)
	*ocSrc += 2;
}


void _PEXDecodeCellArray2D (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexCellArray2D	*oc;
    unsigned		count;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);


    GET_STRUCT_PTR (pexCellArray2D, *ocSrc, oc);
    *ocSrc += SIZEOF (pexCellArray2D);

    if (fpConvert)
    {
	FP_CONVERT_NTOH (oc->point1_x,
	    ocDest->data.CellArray2D.point1.x, fpFormat);
	FP_CONVERT_NTOH (oc->point1_y,
	    ocDest->data.CellArray2D.point1.y, fpFormat);
	FP_CONVERT_NTOH (oc->point2_x,
	    ocDest->data.CellArray2D.point2.x, fpFormat);
	FP_CONVERT_NTOH (oc->point2_y,
	    ocDest->data.CellArray2D.point2.y, fpFormat);
    }
    else
    {
	ocDest->data.CellArray2D.point1.x = oc->point1_x;
	ocDest->data.CellArray2D.point1.y = oc->point1_y;
	ocDest->data.CellArray2D.point2.x = oc->point2_x;
	ocDest->data.CellArray2D.point2.y = oc->point2_y;
    }

    ocDest->data.CellArray2D.col_count = oc->dx;
    ocDest->data.CellArray2D.row_count = oc->dy;

    count = oc->dx * oc->dy;
    ocDest->data.CellArray2D.color_indices =
	(PEXTableIndex *) Xmalloc (count * sizeof (PEXTableIndex));

    EXTRACT_LISTOF_CARD16 (count, *ocSrc,
	ocDest->data.CellArray2D.color_indices);
	
    if (count & 1)
	*ocSrc += 2;
}


void _PEXDecodeExtendedCellArray (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexExtendedCellArray	*oc;
    unsigned			count;
    int				fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    GET_STRUCT_PTR (pexExtendedCellArray, *ocSrc, oc);
    *ocSrc += SIZEOF (pexExtendedCellArray);

    if (fpConvert)
    {
	FP_CONVERT_NTOH (oc->point1_x,
	    ocDest->data.ExtendedCellArray.point1.x, fpFormat);
	FP_CONVERT_NTOH (oc->point1_y,
	    ocDest->data.ExtendedCellArray.point1.y, fpFormat);
	FP_CONVERT_NTOH (oc->point1_z,
	    ocDest->data.ExtendedCellArray.point1.z, fpFormat);
	FP_CONVERT_NTOH (oc->point2_x,
	    ocDest->data.ExtendedCellArray.point2.x, fpFormat);
	FP_CONVERT_NTOH (oc->point2_y,
	    ocDest->data.ExtendedCellArray.point2.y, fpFormat);
	FP_CONVERT_NTOH (oc->point2_z,
	    ocDest->data.ExtendedCellArray.point2.z, fpFormat);
	FP_CONVERT_NTOH (oc->point3_x,
	    ocDest->data.ExtendedCellArray.point3.x, fpFormat);
	FP_CONVERT_NTOH (oc->point3_y,
	    ocDest->data.ExtendedCellArray.point3.y, fpFormat);
	FP_CONVERT_NTOH (oc->point3_z,
	    ocDest->data.ExtendedCellArray.point3.z, fpFormat);
    }
    else
    {
	ocDest->data.ExtendedCellArray.point1.x = oc->point1_x;
	ocDest->data.ExtendedCellArray.point1.y = oc->point1_y;
	ocDest->data.ExtendedCellArray.point1.z = oc->point1_z;
	ocDest->data.ExtendedCellArray.point2.x = oc->point2_x;
	ocDest->data.ExtendedCellArray.point2.y = oc->point2_y;
	ocDest->data.ExtendedCellArray.point2.z = oc->point2_z;
	ocDest->data.ExtendedCellArray.point3.x = oc->point3_x;
	ocDest->data.ExtendedCellArray.point3.y = oc->point3_y;
	ocDest->data.ExtendedCellArray.point3.z = oc->point3_z;
    }

    ocDest->data.ExtendedCellArray.col_count = oc->dx;
    ocDest->data.ExtendedCellArray.row_count = oc->dy;
    ocDest->data.ExtendedCellArray.color_type = oc->colorType;

    count = oc->dx * oc->dy;
    ocDest->data.ExtendedCellArray.colors.indexed = (PEXColorIndexed *)
	Xmalloc (count * GetClientColorSize (oc->colorType));

    EXTRACT_LISTOF_COLOR_VAL (count, *ocSrc, oc->colorType,
	ocDest->data.ExtendedCellArray.colors, fpConvert, fpFormat);
}


void _PEXDecodeGDP (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexGDP		*oc;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    GET_STRUCT_PTR (pexGDP, *ocSrc, oc);
    *ocSrc += SIZEOF (pexGDP);

    ocDest->data.GDP.gdp_id = oc->gdpId;
    ocDest->data.GDP.count = oc->numPoints;
    ocDest->data.GDP.length = oc->numBytes;

    ocDest->data.GDP.points =
	(PEXCoord *) Xmalloc ((unsigned) (oc->numPoints * sizeof (PEXCoord)));

    EXTRACT_LISTOF_COORD3D (oc->numPoints, *ocSrc,
	ocDest->data.GDP.points, fpConvert, fpFormat);

    ocDest->data.GDP.data = (char *) Xmalloc ((unsigned) (oc->numBytes));

    memcpy (ocDest->data.GDP.data, *ocSrc, oc->numBytes);
    *ocSrc += PADDED_BYTES (oc->numBytes);
}


void _PEXDecodeGDP2D (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    pexGDP2D		*oc;
    int			fpConvert = (fpFormat != NATIVE_FP_FORMAT);

    GET_STRUCT_PTR (pexGDP2D, *ocSrc, oc);
    *ocSrc += SIZEOF (pexGDP2D);

    ocDest->data.GDP2D.gdp_id = oc->gdpId;
    ocDest->data.GDP2D.count = oc->numPoints;
    ocDest->data.GDP2D.length = oc->numBytes;

    ocDest->data.GDP2D.points = (PEXCoord2D *) Xmalloc (
	(unsigned) (oc->numPoints * sizeof (PEXCoord2D)));

    EXTRACT_LISTOF_COORD2D (oc->numPoints, *ocSrc,
	ocDest->data.GDP2D.points, fpConvert, fpFormat);

    ocDest->data.GDP2D.data = (char *) Xmalloc ((unsigned) oc->numBytes);

    memcpy (ocDest->data.GDP2D.data, *ocSrc, oc->numBytes);
    *ocSrc += PADDED_BYTES (oc->numBytes);
}


void _PEXDecodeNoop (fpFormat, ocSrc, ocDest)

int		fpFormat;
char		**ocSrc;
PEXOCData	*ocDest;

{
    /* no data to decode */

    *ocSrc += SIZEOF (pexNoop);
}
