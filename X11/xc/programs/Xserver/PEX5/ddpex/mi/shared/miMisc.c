/* $Xorg: miMisc.c,v 1.4 2001/02/09 02:04:13 xorgcvs Exp $ */
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
supporting documentation, and that the name of Sun Microsystems
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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/shared/miMisc.c,v 1.9 2001/12/14 19:57:39 dawes Exp $ */

#include "mipex.h"
#include "miInfo.h"
#include "pexUtils.h"
#include "PEXprotost.h"
#include "pexos.h"


/* pex device dependent initialization */
ddpex43rtn
ddpexInit()
{
    extern int predef_initialized;	/* in miLUT.c */
    extern ddBOOL pcflag;		/* in miRender.c */
    extern ddBOOL init_pick_flag;	/* in miWks.c */

    predef_initialized = 0;
    pcflag = MI_FALSE;
    init_pick_flag = MI_FALSE;

    return (Success);
}

/* pex device dependent reset */
/*
	This function is called during server reset.
	It should free any buffers and initialize any device-specific
	data that must be done during server reset.

	The PEX-ME allocates no buffers and so this function does 
	not much.  It is provided as an aid to porting.  It is
	called from the dipex routine PEXResetProc
*/
void
ddpexReset()
{
	/* YOUR CODE HERE */
}

/* define the imp dep info and enum type info */

/* Theoretically, all of this info could depend on the drawable of the
 * workstation, so put everything into arrays based on the drawable.  
 * There is an imp dep drawable type value defined in mi.h which is
 * determined by the implementation based on the workstations drawable.
 * The MI_MAXDRAWABLES is the number of imp dep defined drawable types
 * used for PEX.  In the SI, its value is 1.
 */

/* also, these values are all hard coded for the SI (using #defines
 * in miInfo.h).  You may want to
 * inquire into a library that you are porting to for these values.
 * To do this, replace the code in the procedures below to retrieve 
 * the value from your library instead of from the table. 
 */

/* imp dep constants */

/* These values are in two arrays that can be accessed by
 * the PEXID constant value.  One array has the values which are
 * type CARD32. The other is for FLOAT values.
 * SI_NUM_INT_IMPS and SI_NUM_FLOAT_IMPS specify how many values
 * there are of each
 */

/* add one to number of ints because imps start at 1 */
static ddULONG	intImpDeps[MI_MAXDRAWABLES][SI_NUM_INT_IMPS + 1] = {
	0,		/* dummy */
	SI_DITHERING_SUPPORTED,
	SI_MAX_EDGE_WIDTH,
	SI_MAX_LINE_WIDTH,
	SI_MAX_MARKER_SIZE,
	SI_MAX_MODEL_CLIP_PLANES,
	SI_MAX_NAME_SET_NAMES,
	SI_MAX_NON_AMBIENT_LIGHTS,
	SI_MAX_NURB_ORDER,
	SI_MAX_TRIM_CURVE_ORDER,
	SI_MIN_EDGE_WIDTH,
	SI_MIN_LINE_WIDTH,
	SI_MIN_MARKER_SIZE,
	SI_NOM_EDGE_WIDTH,
	SI_NOM_LINE_WIDTH,
	SI_NOM_MARKER_SIZE,
	SI_SUPP_EDGE_WIDTHS,
	SI_SUPP_LINE_WIDTHS,
	SI_SUPP_MARKER_SIZES,
	SI_BEST_COLOUR_APPROX_VALUES,
	SI_TRANSPARENCY_SUPPORTED,
	SI_DOUBLE_BUFFERING_SUPPORTED,
	SI_MAX_HITS_EVENT_SUPPORTED
};

#define	FLOAT_INDEX(n)	 (n) - (SI_NUM_INT_IMPS + 1) 

static ddFLOAT	floatImpDeps[MI_MAXDRAWABLES][SI_NUM_FLOAT_IMPS] = {
	SI_CHROM_RED_U,
	SI_CHROM_RED_V,
	SI_LUM_RED,
	SI_CHROM_GREEN_U,
	SI_CHROM_GREEN_V,
	SI_LUM_GREEN,
	SI_CHROM_BLUE_U,
	SI_CHROM_BLUE_V,
	SI_LUM_BLUE,
	SI_CHROM_WHITE_U,
	SI_CHROM_WHITE_V,
	SI_LUM_WHITE
};

/*++
 |
 |  Function Name:      InquireImpDepConstants
 |
 |  Function Description:
 |       Handles the PEXGetImpDepConstants request.
 |
 |  Note(s):
 |
 --*/

ddpex43rtn
InquireImpDepConstants(pDrawable, numNames, pNames, pBuffer)
/* in */
    DrawablePtr         pDrawable;/* drawable */
    ddULONG             numNames; /* number of names */
    ddUSHORT           *pNames;	  /* list of names */
/* out */
    ddBufferPtr         pBuffer;  /* list of constants */

{

    register short      i;
    register ddULONG    dsize;

    register union
    {
	ddULONG            *C32;
	ddFLOAT            *F32;
    }                   pbuf;

    register ddUSHORT  *pname;
    register int        drawType;

    pBuffer->dataSize = 0;

    dsize = numNames * sizeof(ddULONG);
    PU_CHECK_BUFFER_SIZE(pBuffer, dsize);

    pBuffer->dataSize = dsize;
    MI_WHICHDRAW(pDrawable, drawType);

    /* process each inquiry request in the list */

    for (i = 0, pname = pNames, pbuf.C32 = (ddULONG *) (pBuffer->pBuf);
	i < numNames; i++, pname++, pbuf.C32++)
    {

	/*
	 * use a switch here for each constant type if you don't hard code
	 * the values (e.g. you want to call into a library to get the
	 * values)
	 */
	if ((int) *pname < SI_NUM_INT_IMPS)
	    *pbuf.C32 = intImpDeps[drawType][(int) *pname];
	else
	    *pbuf.F32 = floatImpDeps[drawType][(int)FLOAT_INDEX(*pname)];

    }				  /* for (i=0, pname = pNames... */

    return (Success);

}				  /* InquireImpDepConstants */

/*  now  enumerated type info */

/* again, these can theoretically vary depending on the drawable type
 * and arrays bases on the imp dep drawable type are used again
 */

/* some of these are accessed in other code to make sure that only valid
 * enum types are used, so they are not declared static
 * the defined constants used with these are in miInfo.h
 * TODO: make proceures to check for valid ets to call instead of
 * declaring these global
 */

miEnumType	miMarkerTypeET[MI_MAXDRAWABLES][SI_MARKER_NUM] = {
	{{1, SI_MARKER_1}, 
	 {2, SI_MARKER_2}, 
	 {3, SI_MARKER_3}, 
	 {4, SI_MARKER_4}, 
	 {5, SI_MARKER_5}}
};

miEnumType	miATextStyleET[MI_MAXDRAWABLES][SI_ATEXT_NUM] = {
	{{1, SI_ATEXT_1}, 
	 {2, SI_ATEXT_2}}
};

miEnumType	miInteriorStyleET[MI_MAXDRAWABLES][SI_INT_NUM] = {
	{{1, SI_INT_1}, 
	 {2, SI_INT_2}, 
	 {5, SI_INT_5}}
};

/* hatches are not supported but put in a dummy  */
miEnumType	miHatchStyleET[MI_MAXDRAWABLES][SI_HATCH_NUM + 1] = {
	{{0, ""}}
};

miEnumType	miLineTypeET[MI_MAXDRAWABLES][SI_LINE_NUM] = {
	{{1, SI_LINE_1}, 
	 {2, SI_LINE_2}, 
	 {3, SI_LINE_3}, 
	 {4, SI_LINE_4}}
};

miEnumType	miSurfaceEdgeTypeET[MI_MAXDRAWABLES][SI_EDGE_NUM] = {
	{{1, SI_EDGE_1},
	 {2, SI_EDGE_2},
	 {3, SI_EDGE_3},
	 {4, SI_EDGE_4}}
};

miEnumType	miPickDeviceTypeET[MI_MAXDRAWABLES][SI_PICK_DEVICE_NUM] = {
	{{1, SI_PICK_DEVICE_1},
	 {2, SI_PICK_DEVICE_2}}
};

miEnumType	miPickOneMethodET[MI_MAXDRAWABLES][SI_PICK_ONE_NUM] = {
	 {{1, SI_PICK_ONE_LAST}}
};

miEnumType	miPickAllMethodET[MI_MAXDRAWABLES][SI_PICK_ALL_NUM] = {
	 {{1, SI_PICK_ALL_ALL}}
};

miEnumType	miPolylineInterpMethodET[MI_MAXDRAWABLES][SI_LINE_INTERP_NUM] = {
	{{1, SI_LINE_INTERP_1}}
};

miEnumType	miCurveApproxMethodET[MI_MAXDRAWABLES][SI_CURVE_APPROX_NUM] = {
	{{1, SI_CURVE_APPROX_1}, 
	 {2, SI_CURVE_APPROX_2}, 
	 {3, SI_CURVE_APPROX_3}, 
	 {4, SI_CURVE_APPROX_4}, 
	 {6, SI_CURVE_APPROX_6}, 
	 {7, SI_CURVE_APPROX_7}}
};

miEnumType	miReflectionModelET[MI_MAXDRAWABLES][SI_REFLECT_NUM] = {
	{{1, SI_REFLECT_1},
	 {2, SI_REFLECT_2},
	 {3, SI_REFLECT_3},
	 {4, SI_REFLECT_4}}
};

miEnumType	miSurfaceInterpMethodET[MI_MAXDRAWABLES][SI_SURF_INTERP_NUM] = {
	{{1, SI_SURF_INTERP_1}}
};

miEnumType	miSurfaceApproxMethodET[MI_MAXDRAWABLES][SI_SURF_APPROX_NUM] = {
	{{1, SI_SURF_APPROX_1}, 
	 {2, SI_SURF_APPROX_2},
	 {3, SI_SURF_APPROX_3},
	 {4, SI_SURF_APPROX_4},
	 {6, SI_SURF_APPROX_6},
	 {7, SI_SURF_APPROX_7}}
};

miEnumType	miTrimCurveApproxMethodET[MI_MAXDRAWABLES][SI_TRIM_CURVE_NUM] = {
	{{1, SI_TRIM_CURVE_1}, 
	 {2, SI_TRIM_CURVE_2}}
};

miEnumType	miModelClipOperatorET[MI_MAXDRAWABLES][SI_MODEL_CLIP_NUM] = {
	{{1, SI_MODEL_CLIP_1}, 
	 {2, SI_MODEL_CLIP_2}}
};

miEnumType	miLightTypeET[MI_MAXDRAWABLES][SI_LIGHT_NUM] = {
	{{1, SI_LIGHT_1},
	 {2, SI_LIGHT_2},
	 {3, SI_LIGHT_3},
	 {4, SI_LIGHT_4}}
};

miEnumType	miColourTypeET[MI_MAXDRAWABLES][SI_COLOUR_NUM] = {
	{{0, SI_COLOUR_0}, 
	 {1, SI_COLOUR_1}}
};

miEnumType	miFloatFormatET[MI_MAXDRAWABLES][SI_FLOAT_NUM] = {
	{{1, SI_FLOAT_1}, 
	 {2, SI_FLOAT_2}}
};

miEnumType	miHlhsrModeET[MI_MAXDRAWABLES][SI_HLHSR_NUM] = {
	{{1, SI_HLHSR_1}}
};

miEnumType	miPromptEchoTypeET[MI_MAXDRAWABLES][SI_PET_NUM] = {
	{{1, SI_PET_1}}
};

miEnumType	miDisplayUpdateModeET[MI_MAXDRAWABLES][SI_UPDATE_NUM] = {
	{{1, SI_UPDATE_1}, 
	 {2, SI_UPDATE_2}, 
	 {3, SI_UPDATE_3}, 
	 {4, SI_UPDATE_4}, 
	 {5, SI_UPDATE_5}}
};

miEnumType	miColourApproxTypeET[MI_MAXDRAWABLES][SI_CLR_APPROX_TYPE_NUM] = {
	{{1, SI_CLR_APPROX_TYPE_1}, 
	 {2, SI_CLR_APPROX_TYPE_2}}
};

miEnumType	miColourApproxModelET[MI_MAXDRAWABLES][SI_CLR_APPROX_MODEL_NUM] = {
	{{1, SI_CLR_APPROX_MODEL_1}}
};

/* put in a dummy */
miEnumType	miGDPET[MI_MAXDRAWABLES][SI_GDP_NUM + 1] = {
	{{0, ""}}
};

/* put in a dummy */
miEnumType	miGDP3ET[MI_MAXDRAWABLES][SI_GDP3_NUM + 1] = {
	{{0, ""}}
};

/* put in a dummy */
miEnumType	miGSEET[MI_MAXDRAWABLES][SI_GSE_NUM + 1] = {
	{{0, ""}}
};

miEnumType	miEscapeET[MI_MAXDRAWABLES][SI_ESCAPE_NUM] = {
	{{1, SI_ESCAPE_1}}
};

miEnumType	miRenderingColourModelET[MI_MAXDRAWABLES][SI_REND_COLOUR_NUM] = {
	{{1, SI_REND_COLOUR_1}}
};

miEnumType	miParametricSurfaceCharsET[MI_MAXDRAWABLES][SI_P_SURF_CHAR_NUM] = {
	{{1, SI_P_SURF_CHAR_1},
	 {2, SI_P_SURF_CHAR_2},
	 {3, SI_P_SURF_CHAR_3}}
};

/* useful macros for putting et info into buffer */
#define PUT_BUF32(buf, value) \
	   *(buf).C32++ = (value);

#define PUT_BUF16(buf, value) \
           *(buf).C16++ = (value);

#define PUT_BUF8(buf, value) \
           *(buf).C8++ = (value);

#define PADDING(n) ( (n)&3 ? (4 - ((n)&3)) : 0)

/* be sure k is defined before using this */
/* size is the size of the string
 * extra is 2 if the case is PEXETBoth, it's 0 for PEXETMnemonic
 * this macro is not used for PEXETIndex
 */
#define PUT_STR(buf, string, size, extra)			\
        { 							\
        PUT_BUF16(buf, size); 					\
        for (k=0; k < size; k++) 				\
	    PUT_BUF8(buf, string[k]); 				\
	k = PADDING( size + 2 + extra );			\
	while (k) {						\
		PUT_BUF8( buf, 0 );				\
		k--;						\
        } }

/* macro to count space needed for et info */
#define COUNT_ET( num, pet ) 						\
	count+=4;	/* space for returned num value */		\
	switch (itemMask)						\
	{								\
		case PEXETIndex:	/* return index values only */	\
			count += (num << 1);				\
			/* add pad if necessary */			\
			if (num & 1)					\
				count+=2;				\
		break;							\
									\
		case PEXETMnemonic:	/* return mnemonics only */	\
			for (j=0; j<num; j++, pet++)			\
			{						\
				/* add length of string */		\
				count += 2;				\
				/* then number of chars in string */	\
				count +=  strlen((pet)->name);		\
				/* then pads for string (and its length) */\
				count += PADDING(strlen((pet)->name) + 2);\
			}						\
		break;							\
									\
		case PEXETBoth:		/* return index and mnemonic */	\
			for (j=0; j<num; j++, pet++)			\
			{						\
				/* add index */				\
				count += 2;				\
				/* add length of string */		\
				count += 2;				\
				/* then number of chars in string */	\
				count +=  strlen((pet)->name);		\
				/* then pads for string */		\
				count += PADDING( strlen((pet)->name) );\
			}						\
		break;							\
									\
        }			  /* switch (itemMask) */

/* macro to put hard coded et info into buffer */
#define GET_ET( num, pet ) \
	/* always increment the list count and return the number of types */\
	(*pNumLists)++;							\
	PUT_BUF32(pbuf, (num));						\
	/* now put in the index and/or mnemonic */			\
	switch (itemMask)						\
	{								\
		case PEXETIndex:	/* return index values only */	\
			for (j=0; j<(num); j++, pet++)			\
				PUT_BUF16(pbuf, (pet)->index);		\
			/* add pad if necessary */			\
			if ((num) & 1)					\
				PUT_BUF16(pbuf, 0);			\
		break;							\
									\
		case PEXETMnemonic:	/* return mnemonics only */	\
			for (j=0; j<(num); j++, pet++)			\
			{						\
				size = strlen( (pet)->name );		\
				/* PUT_STR pads end of string */	\
				PUT_STR(pbuf, (pet)->name, size, 0);	\
			}						\
		break;							\
									\
		case PEXETBoth:		/* return index and mnemonic */	\
			for (j=0; j<(num); j++, pet++)			\
			{						\
				size = strlen( (pet)->name );		\
				PUT_BUF16(pbuf, (pet)->index);		\
				/* PUT_STR pads end of string */	\
				PUT_STR(pbuf, (pet)->name, size, 2);	\
			}						\
		break;							\
									\
        }			  /* switch (itemMask) */

#define	DO_ET(num, pet)			\
	if ( counting )			\
	{				\
		COUNT_ET( num, pet );	\
	}				\
	else				\
	{				\
		GET_ET( num, pet )	\
	}

/*++
 |
 |  Function Name:      InquireEnumTypeInfo
 |
 |  Function Description:
 |       Handles the PEXGetEnumTypeInfo request.
 |
 |  Note(s):
 |
 --*/

ddpex43rtn
InquireEnumTypeInfo(pDrawable, itemMask, numEnumTypes, pEnumTypeList, pNumLists, pBuffer)
/* in */
    DrawablePtr         pDrawable;
    ddBitmask           itemMask;
    ddULONG             numEnumTypes;
    ddUSHORT           *pEnumTypeList;
/* out */
    ddULONG            *pNumLists;
    ddBufferPtr         pBuffer;
{
    register union
    {
	ddULONG            *C32;
	ddUSHORT           *C16;
	ddBYTE             *C8;
    }                   pbuf;

    register int        drawType;
    register ddUSHORT  *ptype;
    register unsigned   i,
                        j,
                        k;
    ddUSHORT            size;
    short               counting;
    int                 count;
    ddULONG             num;
    miEnumType         *pet;

    MI_WHICHDRAW(pDrawable, drawType);

    *pNumLists = 0;
    count = 0;
    pBuffer->dataSize = 0;

    /*
     * loop twice.  the first time, count buffer size needed second time,
     * fill the buffer
     */
    for (counting = 1; counting >= 0; counting--)
    {

	/*
	 * be sure to put this here in case the buffer is realloc'd after the
	 * first time throught
	 */
	pbuf.C32 = (ddULONG *) pBuffer->pBuf;
	for (i = numEnumTypes, ptype = pEnumTypeList; i > 0; i--, ptype++)
	{
	    /* process each enum request */

	    /*
	     * this is all hard coded (in miInfo.h) for the SI replace as
	     * needed if you don't want it hard coded
	     */
	    switch (*ptype)
	    {
	      case PEXETMarkerType:
		num = SI_MARKER_NUM;
		pet = &miMarkerTypeET[drawType][0];
		DO_ET(num, pet);
		break;

	      case PEXETATextStyle:
		num = SI_ATEXT_NUM;
		pet = miATextStyleET[drawType];
		DO_ET(num, pet);
		break;

	      case PEXETInteriorStyle:
		num = SI_INT_NUM;
		pet = miInteriorStyleET[drawType];
		DO_ET(num, pet);
		break;

	      case PEXETHatchStyle:
		num = SI_HATCH_NUM;
		pet = miHatchStyleET[drawType];
		DO_ET(num, pet);
		break;

	      case PEXETLineType:
		num = SI_LINE_NUM;
		pet = miLineTypeET[drawType];
		DO_ET(num, pet);
		break;

	      case PEXETSurfaceEdgeType:
		num = SI_EDGE_NUM;
		pet = miSurfaceEdgeTypeET[drawType];
		DO_ET(num, pet);
		break;

	      case PEXETPickDeviceType:
		num = SI_PICK_DEVICE_NUM;
		pet = miPickDeviceTypeET[drawType];
		DO_ET(num, pet);
		break;

	      case PEXETPolylineInterpMethod:
		num = SI_LINE_INTERP_NUM;
		pet = miPolylineInterpMethodET[drawType];
		DO_ET(num, pet);
		break;

	      case PEXETCurveApproxMethod:
		num = SI_CURVE_APPROX_NUM;
		pet = miCurveApproxMethodET[drawType];
		DO_ET(num, pet);
		break;

	      case PEXETReflectionModel:
		num = SI_REFLECT_NUM;
		pet = miReflectionModelET[drawType];
		DO_ET(num, pet);
		break;

	      case PEXETSurfaceInterpMethod:
		num = SI_SURF_INTERP_NUM;
		pet = miSurfaceInterpMethodET[drawType];
		DO_ET(num, pet);
		break;

	      case PEXETSurfaceApproxMethod:
		num = SI_SURF_APPROX_NUM;
		pet = miSurfaceApproxMethodET[drawType];
		DO_ET(num, pet);
		break;

	      case PEXETModelClipOperator:
		num = SI_MODEL_CLIP_NUM;
		pet = miModelClipOperatorET[drawType];
		DO_ET(num, pet);
		break;

	      case PEXETLightType:
		num = SI_LIGHT_NUM;
		pet = miLightTypeET[drawType];
		DO_ET(num, pet);
		break;

	      case PEXETColourType:
		num = SI_COLOUR_NUM;
		pet = miColourTypeET[drawType];
		DO_ET(num, pet);
		break;

	      case PEXETFloatFormat:
		num = SI_FLOAT_NUM;
		pet = miFloatFormatET[drawType];
		DO_ET(num, pet);
		break;

	      case PEXETHlhsrMode:
		num = SI_HLHSR_NUM;
		pet = miHlhsrModeET[drawType];
		DO_ET(num, pet);
		break;

	      case PEXETPromptEchoType:
		num = SI_PET_NUM;
		pet = miPromptEchoTypeET[drawType];
		DO_ET(num, pet);
		break;

	      case PEXETDisplayUpdateMode:
		num = SI_UPDATE_NUM;
		pet = miDisplayUpdateModeET[drawType];
		DO_ET(num, pet);
		break;

	      case PEXETColourApproxType:

		/*
		 * The colour approximation type is based on the depth of the
		 * drawable - > 8 bits implies indexed otherwise colorspace
		 */
		num = SI_CLR_APPROX_TYPE_NUM;
		pet = miColourApproxTypeET[drawType];
		DO_ET(num, pet);
		break;

	      case PEXETColourApproxModel:
		num = SI_CLR_APPROX_MODEL_NUM;
		pet = miColourApproxModelET[drawType];
		DO_ET(num, pet);
		break;

	      case PEXETGDP:
		num = SI_GDP_NUM;
		pet = miGDPET[drawType];
		DO_ET(num, pet);
		break;

	      case PEXETGDP3:
		num = SI_GDP3_NUM;
		pet = miGDP3ET[drawType];
		DO_ET(num, pet);
		break;

	      case PEXETGSE:
		num = SI_GSE_NUM;
		pet = miGSEET[drawType];
		DO_ET(num, pet);
		break;

	      case PEXETTrimCurveApproxMethod:
		num = SI_TRIM_CURVE_NUM;
		pet = miTrimCurveApproxMethodET[drawType];
		DO_ET(num, pet);
		break;

	      case PEXETRenderingColourModel:
		num = SI_REND_COLOUR_NUM;
		pet = miRenderingColourModelET[drawType];
		DO_ET(num, pet);
		break;

	      case PEXETParaSurfCharacteristics:
		num = SI_P_SURF_CHAR_NUM;
		pet = miParametricSurfaceCharsET[drawType];
		DO_ET(num, pet);
		break;

	      case PEXETEscape:
		num = SI_ESCAPE_NUM;
		pet = miEscapeET[drawType];
		DO_ET(num, pet);
		break;

	      case PEXETPickOneMethod:
		num = SI_PICK_ONE_NUM;
		pet = miPickOneMethodET[drawType];
		DO_ET(num, pet);
		break;

	      case PEXETPickAllMethod:
		num = SI_PICK_ALL_NUM;
		pet = miPickAllMethodET[drawType];
		DO_ET(num, pet);
		break;

	    }			  /* switch (*ptype) */

	}			  /* for (i=0, ptype = pEnumTypeList) */

	if (counting)
	{
	    PU_CHECK_BUFFER_SIZE(pBuffer, count);
	}
    }				  /* for (counting >= 0) */

    pBuffer->dataSize = count;
    return (Success);
}				  /* InquireEnumTypeInfo */

/*************************************************************************
 * macro for MatchRendererTargets.
 */

/* 30 is arbitrary constant */
#define ADD_TRIPLET(d,t,v) { int diff; \
          if ((pexBuffer->dataSize + sizeof(pexRendererTarget)) > \
                                                pexBuffer->bufSize){\
            diff = (unsigned long)p - (unsigned long)pexBuffer->pBuf; \
            puBuffRealloc(pexBuffer,pexBuffer->bufSize + \
                                             30*sizeof(pexRendererTarget)); \
	    p = (pexRendererTarget *)(((unsigned long)pexBuffer->pBuf) +diff);\
	  } \
	  p->depth = (d); \
          p->type = (t); \
	  p->visualID = (v); \
	  pexBuffer->dataSize += sizeof(pexRendererTarget); \
          p++; nTargets++; \
          if (nTargets >= maxTriplets) return (Success); \
          }
/*++
 |
 |  Function Name:  MatchRendererTargets    
 |
 |  Function Description:
 |       Handles Match Renderer Taregets Request.
 |       Given a visualID, depth & drawable type, tell whether PEX will
 |       render into it.  Real life: PEX does not do all drawables.
 |
 |  Note(s):
 |
 --*/
			       

ddpex43rtn
MatchRendererTargets(pDraw, depth, drawType, visualID, maxTriplets, pexBuffer)
    DrawablePtr pDraw;
    int         depth;
    int         drawType;
    VisualID    visualID;
    int         maxTriplets;
    ddBuffer   *pexBuffer;
{
    int i;
    int nTargets = 0;

    register ScreenPtr pScreen;
    int idepth, ivisual;
    DepthPtr pDepth;

    pexRendererTarget *p = (pexRendererTarget *)pexBuffer->pBuf;

/*
 * Code originally lifted from CreateWindow (x11/server/dix/window.c)
 */
    pScreen = pDraw->pScreen;

    for(idepth = 0; idepth < pScreen->numDepths; idepth++) {

      pDepth = (DepthPtr) &pScreen->allowedDepths[idepth];

      /*
       * if depth is wild carded, then we need to walk them all.
       */
      if ((depth == pDepth->depth) || (depth == 0)) {

	for (ivisual = 0; ivisual < pDepth->numVids; ivisual++)	{

	  /* if visual is a match or it's wildcarded then do it */
	  if ((visualID == pDepth->vids[ivisual]) || (visualID == 0)) {
	    /*
             * Here is the moment of truth, this is just going to say
             * that everything is available for PEX rendering. It is possible
             * that vendors will want to create a global table that hangs
             * around.  That way they can be qualified in ddpexInit().
             * If compiled with -DMULTIBUFFER it assumes that mutli buffers
             * are fair game.
             */
	    if ((drawType == PEXWindow) || (drawType == PEXDontCare))
	      ADD_TRIPLET(pDepth->depth, PEXWindow, pDepth->vids[ivisual] );
	    if ((drawType == PEXPixmap) || (drawType == PEXDontCare))
	      ADD_TRIPLET(pDepth->depth, PEXPixmap, pDepth->vids[ivisual] );
#ifdef MULTIBUFFER
	    if ((drawType == PEXBuffer) || (drawType == PEXDontCare))
	      ADD_TRIPLET(pDepth->depth, PEXBuffer, pDepth->vids[ivisual] );
#endif	      
	  }
	}
      }
    }
    return (Success);
}
