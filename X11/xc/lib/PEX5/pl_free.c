/* $Xorg: pl_free.c,v 1.4 2001/02/09 02:03:28 xorgcvs Exp $ */
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

#define CHECK_AND_FREE(_ptr) if (_ptr) Xfree (_ptr)


void PEXFreeEnumInfo (numCounts, infoCount, enumInfo)

INPUT unsigned long 	numCounts;
INPUT unsigned long 	*infoCount;
INPUT PEXEnumTypeDesc 	*enumInfo;

{
    PEXEnumTypeDesc	*desc = enumInfo;
    int			i, j;


    if (enumInfo)
	for (i = 0; i < numCounts; i++)
	    for (j = 0; j < infoCount[i]; j++)
	    {
		CHECK_AND_FREE ((char *) desc->descriptor);
		desc++;
	    }

    CHECK_AND_FREE ((char *) infoCount);
    CHECK_AND_FREE ((char *) enumInfo);
}


void PEXFreeFontInfo (numFontInfo, fontInfo)

INPUT unsigned long	numFontInfo;
INPUT PEXFontInfo	*fontInfo;

{
    PEXFontInfo		*info = fontInfo;
    int			i;


    for (i = 0; i < numFontInfo; i++)
    {
	CHECK_AND_FREE ((char *) info->props);
	info++;
    }

    CHECK_AND_FREE ((char *) fontInfo);
}


void PEXFreeFontNames (numFontNames, fontNames)

INPUT unsigned long	numFontNames;
INPUT char		**fontNames;

{
    int i;


    for (i = 0; i < numFontNames; i++)
	CHECK_AND_FREE (fontNames[i]);

    CHECK_AND_FREE ((char *) fontNames);
}


void PEXFreePCAttributes (pcAttr)

INPUT PEXPCAttributes	*pcAttr;

{
    CHECK_AND_FREE ((char *) pcAttr->model_clip_volume.half_spaces);
    CHECK_AND_FREE ((char *) pcAttr->light_state.indices);

    if (pcAttr->para_surf_char.type == PEXPSCMCLevelCurves ||
        pcAttr->para_surf_char.type == PEXPSCWCLevelCurves)
    {
	CHECK_AND_FREE ((char *)
	    pcAttr->para_surf_char.psc.level_curves.parameters);
    }
    else if (pcAttr->para_surf_char.type == PEXPSCImpDep)
    {
	CHECK_AND_FREE ((char *) pcAttr->para_surf_char.psc.imp_dep.data);
    }

    CHECK_AND_FREE ((char *) pcAttr);
}


void PEXFreePDAttributes (pdAttr)

PEXPDAttributes		*pdAttr;

{
    CHECK_AND_FREE ((char *) pdAttr->path.elements);

    CHECK_AND_FREE ((char *) pdAttr);
}


void PEXFreePMAttributes (pmAttr)

PEXPMAttributes	 *pmAttr;

{
    CHECK_AND_FREE ((char *) pmAttr->pick_path.elements);

    CHECK_AND_FREE ((char *) pmAttr);
}


void PEXFreePickPaths (numPickPaths, pickPaths)

INPUT unsigned long	numPickPaths;
INPUT PEXPickPath	*pickPaths;

{
    int total_size, i;


    /*
     * Note that memory allocation of pick paths is optimized by
     * allocating one chunk for all the pick paths in the list, instead
     * of allocating a seperate buffer for each pick path.
     */

    if (pickPaths == PEXPickCache)
    {
	/*
	 * Make the pick cache available again.
	 */

	PEXPickCacheInUse = 0;
    }
    else if (PEXPickCacheInUse)
    {
	/*
	 * The pick cache is in use, so we must free this pick path.
	 */

	Xfree ((char *) pickPaths);
    }
    else
    {
	/*
	 * Calculate the size of the pick path being freed.
	 */

	total_size = numPickPaths * sizeof (PEXPickPath);
	for (i = 0; i < numPickPaths; i++)
	    total_size += (pickPaths[i].count * sizeof (PEXPickElementRef));


	/*
	 * If the size is smaller than the pick cache size or bigger than
	 * the max size, free the pick path.  Otherwise, make this path the
	 * new pick cache buffer.
	 */
	
	if (total_size <= PEXPickCacheSize || total_size > MAX_PICK_CACHE_SIZE)
	    Xfree ((char *) pickPaths);
	else
	{
	    if (PEXPickCache)
		Xfree ((char *) PEXPickCache);
	    PEXPickCache = pickPaths;
	    PEXPickCacheSize = total_size;
	}
    }
}


void PEXFreeRendererAttributes (rdrAttr)

INPUT PEXRendererAttributes	*rdrAttr;

{
    CHECK_AND_FREE ((char *) rdrAttr->current_path.elements);
    CHECK_AND_FREE ((char *) rdrAttr->clip_list.rectangles);
    CHECK_AND_FREE ((char *) rdrAttr->pick_start_path.elements);

    CHECK_AND_FREE ((char *) rdrAttr);
}


void PEXFreeSCAttributes (scAttr)

PEXSCAttributes		*scAttr;

{
    CHECK_AND_FREE ((char *) scAttr->start_path.elements);
    CHECK_AND_FREE ((char *) scAttr->normal.pairs);
    CHECK_AND_FREE ((char *) scAttr->inverted.pairs);

    CHECK_AND_FREE ((char *) scAttr);
}


void PEXFreeStructurePaths (numPaths, paths)

INPUT unsigned long	numPaths;
INPUT PEXStructurePath	*paths;

{
    int i;


    for (i = 0; i < numPaths; i++)
	CHECK_AND_FREE ((char *) paths[i].elements);

    CHECK_AND_FREE ((char *) paths);
}


void PEXFreeTableEntries (tableType, numTableEntries, tableEntries)

INPUT int		tableType;
INPUT unsigned int	numTableEntries;
INPUT PEXPointer	tableEntries;

{
    int 	i;
    

    switch (tableType)
    {
    case PEXLUTPattern:
    {
	PEXPatternEntry *entries = (PEXPatternEntry *) tableEntries;
	
	for (i = 0; i < numTableEntries; i++)
	    CHECK_AND_FREE ((char *) entries[i].colors.indexed);
	break;
    }
    
    case PEXLUTTextFont:
    {
	PEXTextFontEntry *entries = (PEXTextFontEntry *) tableEntries;
	
	for (i = 0; i < numTableEntries; i++)
	    CHECK_AND_FREE ((char *) entries[i].fonts);
	break;
    }

    case PEXLUTLineBundle:
    case PEXLUTMarkerBundle:
    case PEXLUTTextBundle:
    case PEXLUTInteriorBundle:
    case PEXLUTEdgeBundle:
    case PEXLUTColor:
    case PEXLUTView:
    case PEXLUTLight:
    case PEXLUTDepthCue:
    case PEXLUTColorApprox:
        break;
    }
}


void PEXFreeWorkstationAttributes (wksAttr)

INPUT PEXWorkstationAttributes	*wksAttr;

{
    CHECK_AND_FREE ((char *) wksAttr->defined_views.views);
    CHECK_AND_FREE ((char *) wksAttr->posted_structures.structures);

    CHECK_AND_FREE ((char *) wksAttr);
}


void PEXFreeOCData (count, oc_data)

INPUT unsigned long	count;
INPUT PEXOCData		*oc_data;

{
    PEXOCData		*oc = oc_data;
    PEXEncodedTextData 	*encText;
    PEXListOfVertex	*plset;
    PEXListOfCoord	*fillset;
    PEXListOfCoord2D	*fillset2D;
    PEXListOfVertex	*fillsetdata;
    PEXConnectivityData	*pCon;
    PEXListOfTrimCurve	*pTrim;
    int 		i, j, k;


    for (i = 0; i < count; i++, oc++)
    {
	switch (oc->oc_type)
	{
	case PEXOCModelClipVolume:

	    CHECK_AND_FREE ((char *) oc->data.SetModelClipVolume.half_spaces);
	    break;

	case PEXOCModelClipVolume2D:

	    CHECK_AND_FREE ((char *) oc->data.SetModelClipVolume2D.half_spaces);
	    break;

	case PEXOCLightSourceState:

	    CHECK_AND_FREE ((char *) oc->data.SetLightSourceState.enable);
	    CHECK_AND_FREE ((char *) oc->data.SetLightSourceState.disable);
	    break;

	case PEXOCParaSurfCharacteristics:

	    if (oc->data.SetParaSurfCharacteristics.psc_type ==
		PEXPSCMCLevelCurves ||
                oc->data.SetParaSurfCharacteristics.psc_type ==
		PEXPSCWCLevelCurves)
		CHECK_AND_FREE ((char *) oc->data.SetParaSurfCharacteristics.characteristics.level_curves.parameters);
	    break;

	case PEXOCAddToNameSet:

	    CHECK_AND_FREE ((char *) oc->data.AddToNameSet.names);
	    break;

	case PEXOCRemoveFromNameSet:

	    CHECK_AND_FREE ((char *) oc->data.RemoveFromNameSet.names);
	    break;

	case PEXOCApplicationData:

	    CHECK_AND_FREE ((char *) oc->data.ApplicationData.data);
	    break;

	case PEXOCGSE:

	    CHECK_AND_FREE ((char *) oc->data.GSE.data);
	    break;

	case PEXOCMarkers:

	    CHECK_AND_FREE ((char *) oc->data.Markers.points);
	    break;

	case PEXOCMarkers2D:

	    CHECK_AND_FREE ((char *) oc->data.Markers2D.points);
	    break;

	case PEXOCPolyline:

	    CHECK_AND_FREE ((char *) oc->data.Polyline.points);
	    break;

	case PEXOCPolyline2D:

	    CHECK_AND_FREE ((char *) oc->data.Polyline2D.points);
	    break;

	case PEXOCText:

	    encText = oc->data.EncodedText.encoded_text;
	    for (j = 0; j < oc->data.EncodedText.count; j++, encText++)
		CHECK_AND_FREE ((char *) encText->ch);
	    CHECK_AND_FREE ((char *) oc->data.EncodedText.encoded_text);
	    break;

	case PEXOCText2D:

	    encText = oc->data.EncodedText2D.encoded_text;
	    for (j = 0; j < oc->data.EncodedText2D.count; j++, encText++)
		CHECK_AND_FREE ((char *) encText->ch);
	    CHECK_AND_FREE ((char *) oc->data.EncodedText2D.encoded_text);
	    break;

	case PEXOCAnnotationText:

	    encText = oc->data.EncodedAnnoText.encoded_text;
	    for (j = 0; j < oc->data.EncodedAnnoText.count; j++, encText++)
		CHECK_AND_FREE ((char *) encText->ch);
	    CHECK_AND_FREE ((char *) oc->data.EncodedAnnoText.encoded_text);
	    break;

	case PEXOCAnnotationText2D:

	    encText = oc->data.EncodedAnnoText2D.encoded_text;
	    for (j = 0; j < oc->data.EncodedAnnoText2D.count; j++, encText++)
		CHECK_AND_FREE ((char *) encText->ch);
	    CHECK_AND_FREE ((char *) oc->data.EncodedAnnoText2D.encoded_text);
	    break;

	case PEXOCPolylineSetWithData:

	    plset = oc->data.PolylineSetWithData.vertex_lists;
	    for (j = 0; j < oc->data.PolylineSetWithData.count; j++, plset++)
	    {
		CHECK_AND_FREE ((char *) plset->vertices.no_data);
	    }
	    CHECK_AND_FREE ((char *) oc->data.PolylineSetWithData.vertex_lists);
	    break;

	case PEXOCNURBCurve:

	    CHECK_AND_FREE ((char *) oc->data.NURBCurve.knots);
	    CHECK_AND_FREE ((char *) oc->data.NURBCurve.points.point);
	    break;

	case PEXOCFillArea:

	    CHECK_AND_FREE ((char *) oc->data.FillArea.points);
	    break;

	case PEXOCFillArea2D:

	    CHECK_AND_FREE ((char *) oc->data.FillArea2D.points);
	    break;

	case PEXOCFillAreaWithData:

	    CHECK_AND_FREE ((char *)
		oc->data.FillAreaWithData.vertices.no_data);
	    break;

	case PEXOCFillAreaSet:

	    fillset = oc->data.FillAreaSet.point_lists;
	    for (j = 0; j < oc->data.FillAreaSet.count; j++, fillset++)
	    {
		CHECK_AND_FREE ((char *) fillset->points);
	    }
	    CHECK_AND_FREE ((char *) oc->data.FillAreaSet.point_lists);
	    break;

	case PEXOCFillAreaSet2D:

	    fillset2D = oc->data.FillAreaSet2D.point_lists;
	    for (j = 0; j < oc->data.FillAreaSet2D.count; j++, fillset2D++)
	    {
		CHECK_AND_FREE ((char *) fillset2D->points);
	    }
	    CHECK_AND_FREE ((char *) oc->data.FillAreaSet2D.point_lists);
	    break;

	case PEXOCFillAreaSetWithData:

	    fillsetdata = oc->data.FillAreaSetWithData.vertex_lists;
	    for (j = 0; j < oc->data.FillAreaSetWithData.count;
		j++, fillsetdata++)
	    {
		CHECK_AND_FREE ((char *) fillsetdata->vertices.no_data);
	    }
	    CHECK_AND_FREE ((char *) oc->data.FillAreaSetWithData.vertex_lists);
	    break;

	case PEXOCTriangleStrip:

	    CHECK_AND_FREE ((char *) oc->data.TriangleStrip.facet_data.index);
	    CHECK_AND_FREE ((char *) oc-> data.TriangleStrip.vertices.no_data);
	    break;

	case PEXOCQuadrilateralMesh:

	    CHECK_AND_FREE ((char *)
		oc->data.QuadrilateralMesh.facet_data.index);
	    CHECK_AND_FREE ((char *)
		oc->data.QuadrilateralMesh.vertices.no_data);
	    break;

	case PEXOCSetOfFillAreaSets:

	    CHECK_AND_FREE ((char *)
		oc->data.SetOfFillAreaSets.facet_data.index);
	    CHECK_AND_FREE ((char *)
		oc->data.SetOfFillAreaSets.vertices.no_data);
	    CHECK_AND_FREE ((char *) oc->data.SetOfFillAreaSets.edge_flags);

	    pCon = oc->data.SetOfFillAreaSets.connectivity;
	    for (j = 0; j < oc->data.SetOfFillAreaSets.set_count; j++, pCon++)
	    {
		for (k = 0; k < (int) pCon->count; k++)
		    CHECK_AND_FREE ((char *) pCon->lists[k].shorts);
		CHECK_AND_FREE ((char *) pCon->lists);
	    }
	    CHECK_AND_FREE ((char *) oc->data.SetOfFillAreaSets.connectivity);
	    break;

	case PEXOCNURBSurface:
	   
	    CHECK_AND_FREE ((char *) oc->data.NURBSurface.uknots);
	    CHECK_AND_FREE ((char *) oc->data.NURBSurface.vknots);
	    CHECK_AND_FREE ((char *) oc->data.NURBSurface.points.point);
	    
	    pTrim = oc->data.NURBSurface.trim_curves;
	    for (j = 0; j < oc->data.NURBSurface.curve_count; j++, pTrim++)
	    {
		for (k = 0; k < (int) pTrim->count; k++)
		{
		    CHECK_AND_FREE ((char *) pTrim->curves[k].knots.floats);
		    CHECK_AND_FREE ((char *)
			pTrim->curves[k].control_points.point);
		}
		CHECK_AND_FREE ((char *) pTrim->curves);
	    }
	    CHECK_AND_FREE ((char *) oc->data.NURBSurface.trim_curves);
	    break;

	case PEXOCCellArray:

	    CHECK_AND_FREE ((char *) oc->data.CellArray.color_indices);
	    break;

	case PEXOCCellArray2D:

	    CHECK_AND_FREE ((char *) oc->data.CellArray2D.color_indices);
	    break;

	case PEXOCExtendedCellArray:

	    CHECK_AND_FREE ((char *) oc->data.ExtendedCellArray.colors.indexed);
	    break;

	default:
	    break;
	}
    }

    CHECK_AND_FREE ((char *) oc_data);
}
