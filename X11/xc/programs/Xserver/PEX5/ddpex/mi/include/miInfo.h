/* $Xorg: miInfo.h,v 1.4 2001/02/09 02:04:08 xorgcvs Exp $ */
/*

Copyright 1989, 1990, 1991, 1998  The Open Group

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


Copyright 1989, 1990, 1991 by Sun Microsystems, Inc. 

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Sun Microsystems,
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

SUN MICROSYSTEMS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT
SHALL SUN MICROSYSTEMS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*/

/* this file contains the definitions for the enum type and imp dep constant
 * values
 * Theoretically, these values can depend on the capabilities of the 
 * different workstation types, which basically depends on the drawable
 * of the workstation.  Currently, the SI support is the same for all
 * drawables.  If your implementation requires different support for each
 * drawables, then define a similar set of values as these for
 * each type.  They are then loaded in a table which is accessed
 * according to the drawable type (see ddpex/shared/miMisc.c).
 */

#ifndef MI_INFO_H
#define MI_INFO_H

#include "miNS.h"
/* imp dep constants */

/* These values are in two arrays that can be accessed by
 * the PEXID constant value.  Two arrays are used  since some values are CARD32 and
 * some are FLOAT.  SI_NUM_..._IMPS define how many values there are of each type
 */

/* card32s */
#define	SI_NUM_INT_IMPS		22

#define	SI_DITHERING_SUPPORTED		MI_FALSE
#define	SI_MAX_EDGE_WIDTH		~((unsigned long)0)
#define SI_MAX_LINE_WIDTH		~((unsigned long)0)
#define	SI_MAX_MARKER_SIZE		~((unsigned long)0)
#define	SI_MAX_MODEL_CLIP_PLANES	64
#define SI_MAX_NAME_SET_NAMES		MINS_NAMESET_SIZE
#define SI_MAX_NON_AMBIENT_LIGHTS	64
#define SI_MAX_NURB_ORDER		10
#define SI_MAX_TRIM_CURVE_ORDER		6
#define SI_MIN_EDGE_WIDTH		1
#define SI_MIN_LINE_WIDTH		1
#define SI_MIN_MARKER_SIZE		1
#define SI_NOM_EDGE_WIDTH		1	/* nominal edge width */
#define SI_NOM_LINE_WIDTH		1	/* nominal line width */
#define SI_NOM_MARKER_SIZE		1	/* nominal marker size */
#define	SI_SUPP_EDGE_WIDTHS		~((unsigned long)0)	/* number of supported edge widths */
#define	SI_SUPP_LINE_WIDTHS		~((unsigned long)0)	/* number of supported line widths */
#define	SI_SUPP_MARKER_SIZES		~((unsigned long)0)	/* number of supported marker sizes */
#define SI_BEST_COLOUR_APPROX_VALUES	PEXColourApproxAnyValues
#define	SI_TRANSPARENCY_SUPPORTED	MI_FALSE
#define	SI_DOUBLE_BUFFERING_SUPPORTED	MI_TRUE
#define	SI_MAX_HITS_EVENT_SUPPORTED     MI_TRUE

/* floats */
#define	SI_NUM_FLOAT_IMPS	12

/* ALL CIE primary chromaticity coefficients are taken from
 * Rodgers' Procedural Elements for Computer Graphics
 * for Color CRT monitor aligned to d6500 white
 */
#define SI_CHROM_RED_U			0.628
#define SI_CHROM_RED_V			0.346
#define SI_LUM_RED			1.0
#define SI_CHROM_GREEN_U		0.268
#define SI_CHROM_GREEN_V		0.588
#define SI_LUM_GREEN			1.0
#define SI_CHROM_BLUE_U			0.150
#define SI_CHROM_BLUE_V			0.070
#define SI_LUM_BLUE			1.0
#define SI_CHROM_WHITE_U		0.313
#define SI_CHROM_WHITE_V		0.329
#define SI_LUM_WHITE			1.0


/* enumerated type info */

/* the SI_..._NUM value is the number of supported types */

/* If you are changing these values.....
 * OK, I blew it here.  You gotta change the NUM info here
 * AND you gotta go into ../shared/miMisc.c and change the
 * stuff that's in the info tables.  Maybe there's a way
 * to do this so you can just go to one place and change it
 * Also, this info isn't coded to match what's really happening
 * in the rendering, so's if you change what happens during
 * rendering, you gotta come here and change dese tables too.
 * It isn't all done automagically and it probably should, but
 * it's too late now.  These values are used when setting the
 * real_entry of LUTS. 
 */

/* marker type */
#define	SI_MARKER_NUM		5
#define	SI_MARKER_1		"Dot"
#define	SI_MARKER_2		"Cross"
#define	SI_MARKER_3		"Asterisk"
#define	SI_MARKER_4		"Circle"
#define	SI_MARKER_5		"X"

/* annotation text style */
#define	SI_ATEXT_NUM		2
#define	SI_ATEXT_1		"NotConnected"
#define	SI_ATEXT_2		"Connected"

/* interior style */
#define	SI_INT_NUM		3
#define	SI_INT_1		"Hollow"
#define	SI_INT_2		"Solid"
#define	SI_INT_5		"Empty"
/* others */
#define	SI_INT_3		"Pattern"
#define	SI_INT_4		"Hatch"

/* hatch style */
#define	SI_HATCH_NUM		0

/* line type */
#define	SI_LINE_NUM		4
#define	SI_LINE_1		"Solid"
#define	SI_LINE_2		"Dashed"
#define	SI_LINE_3		"Dotted"
#define	SI_LINE_4		"DashDot"

/* surface edge type */
#define	SI_EDGE_NUM		4
#define	SI_EDGE_1		"Solid"
#define	SI_EDGE_2		"Dashed"
#define	SI_EDGE_3		"Dotted"
#define	SI_EDGE_4		"DashDot"

/* pick device type */
#define	SI_PICK_DEVICE_NUM	2
#define	SI_PICK_DEVICE_1	"DC_HitBox"
#define	SI_PICK_DEVICE_2	"NPC_HitVolume"

/* pick one methods */
#define SI_PICK_ONE_NUM		1
#define SI_PICK_ONE_LAST	"Last"
/* others */
#define SI_PICK_ONE_CLOSEST_Z	 "ClosestZ"
#define SI_PICK_ONE_VISIBLE_ANY	 "VisibleAny"
#define SI_PICK_ONE_VISIBLE_CLOSEST	 "VisibleClosest"

/* pick all methods */
#define SI_PICK_ALL_NUM		1
#define SI_PICK_ALL_ALL		"All"
/* others */
#define SI_PICK_ALL_VISIBLE	"Visible"

/* polyline interpolation method */
#define	SI_LINE_INTERP_NUM	1
#define	SI_LINE_INTERP_1	"None"
/* others */
#define	SI_LINE_INTERP_2	"Color"

/* curve approximation method */
#define	SI_CURVE_APPROX_NUM	6
#define	SI_CURVE_APPROX_1	"ConstantBetweenKnots"	/* (Imp. Dep.) */
#define	SI_CURVE_APPROX_2	"ConstantBetweenKnots"
#define	SI_CURVE_APPROX_3	"WCS_ChordalSize"
#define	SI_CURVE_APPROX_4	"NPC_ChordalSize"
#define	SI_CURVE_APPROX_6	"WCS_ChordalDev"
#define	SI_CURVE_APPROX_7	"NPC_ChordalDev"
/* others */ 
#define	SI_CURVE_APPROX_5	"DC_ChordalSize"
#define	SI_CURVE_APPROX_8	"DC_ChordalDev"
#define	SI_CURVE_APPROX_9	"WCS_Relative"
#define	SI_CURVE_APPROX_10	"NPC_Relative"
#define	SI_CURVE_APPROX_11	"DC_Relative"

/* reflection method */
#define	SI_REFLECT_NUM		4
#define	SI_REFLECT_1		"NoShading"
#define	SI_REFLECT_2		"Ambient"
#define	SI_REFLECT_3		"Diffuse"
#define	SI_REFLECT_4		"Specular"
/* others */

/* surface interpolation method */
#define	SI_SURF_INTERP_NUM	1
#define	SI_SURF_INTERP_1	"None"
/* others */
#define	SI_SURF_INTERP_2	"Color"
#define	SI_SURF_INTERP_3	"DotProduct"
#define	SI_SURF_INTERP_4	"Normal"

/* surface approximation method */
#define	SI_SURF_APPROX_NUM	6
#define	SI_SURF_APPROX_1	"ConstantBetweenKnots"	/* (Imp. Dep.) */
#define	SI_SURF_APPROX_2	"ConstantBetweenKnots"
#define	SI_SURF_APPROX_3	"WCS_ChordalSize"
#define	SI_SURF_APPROX_4	"NPC_ChordalSize"
#define	SI_SURF_APPROX_6	"WCS_PlanarDev"
#define	SI_SURF_APPROX_7	"NPC_PlanarDev"
/* others */ 
#define	SI_SURF_APPROX_5	"DC_ChordalSize"
#define	SI_SURF_APPROX_8	"DC_PlanarDev"
#define	SI_SURF_APPROX_9	"WCS_Relative"
#define	SI_SURF_APPROX_10	"NPC_Relative"
#define	SI_SURF_APPROX_11	"DC_Relative"

/* trim curve approximation method */
#define	SI_TRIM_CURVE_NUM	2
#define	SI_TRIM_CURVE_1		"ConstantBetweenKnots"	/* (Imp. Dep.) */
#define	SI_TRIM_CURVE_2		"ConstantBetweenKnots"

/* model clip operator */
#define	SI_MODEL_CLIP_NUM	2
#define	SI_MODEL_CLIP_1		"Replace"
#define	SI_MODEL_CLIP_2		"Intersection"

/* light type */
#define	SI_LIGHT_NUM		4
#define	SI_LIGHT_1		"Ambient"
#define	SI_LIGHT_2		"WCS_Vector"
#define	SI_LIGHT_3		"WCS_Point"
#define	SI_LIGHT_4		"WCS_Spot"

/* colour type */
#define	SI_COLOUR_NUM		2
#define	SI_COLOUR_0		"Indexed"
#define	SI_COLOUR_1		"RGBFloat"
/* others */
#define	SI_COLOUR_2		"CIEFloat"
#define	SI_COLOUR_3		"HSVFloat"
#define	SI_COLOUR_4		"HLSFloat"
#define	SI_COLOUR_5		"RGBInt8"
#define	SI_COLOUR_6		"RGBInt16"

/* float format */
#define	SI_FLOAT_NUM		2
#define	SI_FLOAT_1		"IEEE_754_32"
#define	SI_FLOAT_2		"DEC_F_Floating"
/* others */
#define	SI_FLOAT_3		"IEEE_754_64"
#define	SI_FLOAT_4		"DEC_D_Floating"

/* hlhsr mode */
#define	SI_HLHSR_NUM		1
#define SI_HLHSR_1		"Off"
/* others */
#define SI_HLHSR_2		"ZBuffer"
#define SI_HLHSR_3		"Painters"
#define SI_HLHSR_4		"Scanline"
#define SI_HLHSR_5		"HiddenLineOnly"
#define SI_HLHSR_6		"ZBufferId"

/* prompt echo type */
#define	SI_PET_NUM		1
#define	SI_PET_1		"EchoPrimitive"
/* others */
#define	SI_PET_2		"EchoStructure"
#define	SI_PET_3		"EchoNetwork"

/* display update mode */
#define	SI_UPDATE_NUM		5
#define	SI_UPDATE_1		"VisualizeEach"
#define	SI_UPDATE_2		"VisualizeEasy"
#define	SI_UPDATE_3		"VisualizeNone"
#define	SI_UPDATE_4		"SimulateSome"
#define	SI_UPDATE_5		"VisualizeWhenever"

/* colour approximation type */
#define	SI_CLR_APPROX_TYPE_NUM	2
#define	SI_CLR_APPROX_TYPE_1	"ColorSpace"
#define	SI_CLR_APPROX_TYPE_2	"ColorRange"

/* colour approximation model */
#define	SI_CLR_APPROX_MODEL_NUM	1
#define	SI_CLR_APPROX_MODEL_1	"RGB"
/* others */
#define	SI_CLR_APPROX_MODEL_2	"CIE"
#define	SI_CLR_APPROX_MODEL_3	"HSV"
#define	SI_CLR_APPROX_MODEL_4	"HLS"
#define	SI_CLR_APPROX_MODEL_5	"YIQ"

/* gdp */
#define	SI_GDP_NUM		0

/* gdp3 */
#define	SI_GDP3_NUM		0

/* gse */
#define	SI_GSE_NUM		0

/* escape */
#define SI_ESCAPE_NUM           1
#define SI_ESCAPE_1             "SetEchoColor"

/* rendering colour model */
#define	SI_REND_COLOUR_NUM	1
#define	SI_REND_COLOUR_1		"RGB"
/* others */
#define	SI_REND_COLOUR_0		"(Imp. Dep.)"
#define	SI_REND_COLOUR_2		"CIE"
#define	SI_REND_COLOUR_3		"HSV"
#define	SI_REND_COLOUR_4		"HLS"

/* parametric surface characteristics */
#define	SI_P_SURF_CHAR_NUM	3
#define	SI_P_SURF_CHAR_1	"None"
#define	SI_P_SURF_CHAR_2	"None"
#define	SI_P_SURF_CHAR_3	"IsoparametricCurves"
/* others */ 
#define	SI_P_SURF_CHAR_4	"MC_LevelCurves"
#define	SI_P_SURF_CHAR_5	"WC_Levelcurves"

#endif				  /* MI_INFO_H */
