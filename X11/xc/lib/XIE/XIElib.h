/* $Xorg: XIElib.h,v 1.6 2001/02/09 02:03:41 xorgcvs Exp $ */

/*

Copyright 1993, 1994, 1998  The Open Group

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

#ifndef _XIELIB_H_
#define _XIELIB_H_

#include <X11/Xlib.h>
#include <X11/extensions/XIE.h>

/*--------------------------------------------------------------------------*
 * 				XIE types				    *
 *--------------------------------------------------------------------------*/

typedef float 	  	XieFloat;

typedef float 	  	XieConstant[3];

typedef float	  	XieMatrix[9];

typedef unsigned  	XieAlignment;

typedef unsigned  	XieArithmeticOp;

typedef unsigned  	XieColorAllocTechnique;

typedef	XID	  	XieColorList;

typedef unsigned 	XieColorspace;

typedef unsigned 	XieCompareOp;

typedef unsigned 	XieConstrainTechnique;

typedef unsigned  	XieConvolveTechnique;

typedef unsigned	XieDataClass;

typedef unsigned	XieDataType;

typedef unsigned  	XieDecodeTechnique;

typedef unsigned  	XieDitherTechnique;

typedef unsigned  	XieEncodeTechnique;

typedef XID		XiePhotospace;

typedef XID		XiePhotoflo;

typedef unsigned	XieExportNotify;

typedef unsigned	XieExportState;

typedef unsigned	XieGamutTechnique;

typedef unsigned	XieGeometryTechnique;

typedef struct {
    unsigned long	value;
    unsigned long	count;
} XieHistogramData;

typedef unsigned	XieHistogramShape;

typedef unsigned	XieInterleave;

typedef unsigned long	XieLevels[3];

typedef	XID		XieLut;

typedef unsigned	XieMathOp;

typedef unsigned	XieOrientation;

typedef unsigned	XiePhotofloOutcome;

typedef unsigned	XiePhotofloState;

typedef XID		XiePhotomap;

typedef unsigned	XiePhototag;

typedef struct {
    int			offset_x;
    int			offset_y;
    XiePhototag		phototag;		
} XieProcessDomain;

typedef struct {	       /* this is bcopyable on 32 bit machines */
    long		x;     /* when using PutClientData */
    long		y;
    unsigned long	width;
    unsigned long	height;
} XieRectangle;

typedef XID		XieRoi;

typedef unsigned	XieServiceClass;

typedef unsigned	XieTechniqueGroup;

typedef struct {
    Bool		needs_param;
    XieTechniqueGroup	group;
    unsigned int	number;
    unsigned int	speed;
    char		*name;
} XieTechnique;

typedef struct {
    XiePhototag 	src;
    int			dst_x;
    int			dst_y;
} XieTile;

typedef unsigned long	XieLTriplet[3];

typedef unsigned  	XieWhiteAdjustTechnique;

#if NeedFunctionPrototypes
typedef void *XiePointer;
#else
typedef char *XiePointer;
#endif


/*--------------------------------------------------------------------------*
 * 			  Extension information				    *
 *--------------------------------------------------------------------------*/

typedef struct {
    unsigned		server_major_rev;
    unsigned		server_minor_rev;
    XieServiceClass	service_class;
    XieAlignment	alignment;
    int			uncnst_mantissa;
    int			uncnst_min_exp;
    int			uncnst_max_exp;
    int 		n_cnst_levels;	/* number of   constrained levels */
    unsigned long	*cnst_levels;	/* recommended constrained levels */
    int                 major_opcode;
    int                 first_event;
    int                 first_error;
} XieExtensionInfo;


/*--------------------------------------------------------------------------*
 *                         Photoflo element union			    *
 *--------------------------------------------------------------------------*/

typedef struct {

    int elemType;

    union {

	/*
	 * Import Elements
	 */
	
	struct {
	    XieDataClass	data_class;
	    XieOrientation	band_order;
	    XieLTriplet		length;
	    XieLevels		levels;
	} ImportClientLUT;
	
	struct {
	    XieDataClass 	data_class;
	    XieLTriplet		width;
	    XieLTriplet		height;
	    XieLevels		levels;
	    Bool		notify;
	    XieDecodeTechnique	decode_tech;
	    XiePointer		decode_param;
	} ImportClientPhoto;
	
	struct {
	    unsigned int	rectangles;
	} ImportClientROI;
	
	struct {
	    Drawable		drawable;
	    int			src_x;
	    int			src_y;
	    unsigned int	width;
	    unsigned int	height;
	    unsigned long	fill;
	    Bool		notify;
	} ImportDrawable;
	
	struct {
	    Drawable		drawable;
	    int			src_x;
	    int			src_y;
	    unsigned int	width;
	    unsigned int	height;
	    unsigned long	fill;
	    unsigned long	bit_plane;
	    Bool		notify;
	} ImportDrawablePlane;
	
	struct {
	    XieLut		lut;
	} ImportLUT;
	 
	struct {
	    XiePhotomap		photomap;
	    Bool		notify;
	} ImportPhotomap;
	
	struct {
	    XieRoi		roi;
	} ImportROI;
	

	/*
	 * Process Elements
	 */
	
	struct {
	    XiePhototag		src1;
	    XiePhototag		src2;
	    XieProcessDomain	domain;
	    XieConstant		constant;
#if defined(__cplusplus) || defined(c_plusplus)
	    XieArithmeticOp	c_operator;
#else
	    XieArithmeticOp	operator;
#endif
	    unsigned int	band_mask;
	} Arithmetic;
	
	struct {
	    XiePhototag		src1;
	    XiePhototag		src2;
	    XiePhototag		src3;
	} BandCombine;
	
	struct {
	    XiePhototag		src;
	    unsigned int	levels;
	    float		bias;
	    XieConstant		coefficients;
	} BandExtract;
	
	struct {
	    XiePhototag		src;
	    unsigned int	band_number;
	} BandSelect;
	
	struct {
	    XiePhototag		src1;
	    XiePhototag		src2;
	    XieConstant		src_constant;
	    XiePhototag		alpha;
	    float		alpha_constant;
	    XieProcessDomain	domain;
	    unsigned int	band_mask;
	} Blend;
	
	struct {
	    XiePhototag		src1;
	    XiePhototag		src2;
	    XieProcessDomain	domain;
	    XieConstant		constant;
#if defined(__cplusplus) || defined(c_plusplus)
	    XieCompareOp	c_operator;
#else
	    XieCompareOp	operator;
#endif
	    Bool		combine;
	    unsigned int	band_mask;
	} Compare;
	
	struct {
	    XiePhototag			src;
	    XieLevels			levels;
	    XieConstrainTechnique	constrain_tech;
	    XiePointer			constrain_param;
	} Constrain;
	
	struct {
	    XiePhototag		src;
	    Colormap		colormap;
	    XieDataClass	data_class;
	    unsigned int	precision;
	} ConvertFromIndex;
	
	struct {
	    XiePhototag		src;
	    XieColorspace	color_space;
	    XiePointer		color_param;
	} ConvertFromRGB;
	
	struct {
	    XiePhototag			src;
	    Colormap			colormap;
	    XieColorList		color_list;
	    Bool			notify;
	    XieColorAllocTechnique	color_alloc_tech;
	    XiePointer			color_alloc_param;
	} ConvertToIndex;
	
	struct {
	    XiePhototag		src;
	    XieColorspace	color_space;
	    XiePointer		color_param;
	} ConvertToRGB;
	
	struct {
	    XiePhototag			src;
	    XieProcessDomain		domain;
	    float			*kernel;
	    int				kernel_size;
	    unsigned int		band_mask;
	    XieConvolveTechnique	convolve_tech;
	    XiePointer			convolve_param;
	} Convolve;
	
	struct {
	    XiePhototag		src;
	    XieLevels		levels;
	    unsigned int	band_mask;
	    XieDitherTechnique	dither_tech;
	    XiePointer		dither_param;
	} Dither;
	
	struct {
	    XiePhototag 		src;
	    unsigned int		width;
	    unsigned int		height;
	    float			coefficients[6];
	    XieConstant			constant;
	    unsigned int		band_mask;
	    XieGeometryTechnique	sample_tech;
	    XiePointer			sample_param;
	} Geometry;
	
	struct {
	    XiePhototag		src1;
	    XiePhototag		src2;
	    XieProcessDomain	domain;
	    XieConstant		constant;
#if defined(__cplusplus) || defined(c_plusplus)
	    int			c_operator;
#else
	    int			operator;
#endif
	    unsigned int	band_mask;
	} Logical;
	
	struct {
	    XiePhototag		src;
	    XieProcessDomain	domain;
	    XieHistogramShape	shape;
	    XiePointer		shape_param;
	} MatchHistogram;
	
	struct {
	    XiePhototag		src;
	    XieProcessDomain	domain;
#if defined(__cplusplus) || defined(c_plusplus)
	    XieMathOp		c_operator;
#else
	    XieMathOp		operator;
#endif
	    unsigned int	band_mask;
	} Math;
	
	struct {
	    unsigned int	width;
	    unsigned int	height;
	    XieConstant		constant;
	    XieTile		*tiles;
	    unsigned int	tile_count;
	} PasteUp;
	
	struct {
	    XiePhototag		src;
	    XieProcessDomain	domain;
	    XiePhototag		lut;
	    unsigned int	band_mask;
	} Point;
	
	struct {
	    XiePhototag			src;
	} Unconstrain;

	/*
	 * Export Elements
	 */
	
	struct {
	    XiePhototag		src;
	    XieProcessDomain	domain;
	    XieExportNotify	notify;
	} ExportClientHistogram;
	
	struct {
	    XiePhototag		src;
	    XieOrientation	band_order;
	    XieExportNotify	notify;
	    XieLTriplet 	start;
	    XieLTriplet		length;
	} ExportClientLUT;
	
	struct {
	    XiePhototag		src;
	    XieExportNotify	notify;
	    XieEncodeTechnique	encode_tech;
	    XiePointer		encode_param;
	} ExportClientPhoto;
	
	struct {
	    XiePhototag		src;
	    XieExportNotify	notify;
	} ExportClientROI;
	
	struct {
	    XiePhototag		src;
	    Drawable		drawable;
	    GC			gc;
	    int			dst_x;
	    int			dst_y;
	} ExportDrawable;
	
	struct {
	    XiePhototag		src;
	    Drawable		drawable;
	    GC			gc;
	    int			dst_x;
	    int			dst_y;
	} ExportDrawablePlane;
	
	struct {
	    XiePhototag		src;
	    XieLut		lut;
	    Bool		merge;
	    XieLTriplet		start;
	} ExportLUT;
	 
	struct {
	    XiePhototag		src;
	    XiePhotomap		photomap;
	    XieEncodeTechnique	encode_tech;
	    XiePointer		encode_param;
	} ExportPhotomap;
	
	struct {
	    XiePhototag		src;
	    XieRoi		roi;
	} ExportROI;

    } data;
} XiePhotoElement;

typedef XiePhotoElement *XiePhotofloGraph;


/*--------------------------------------------------------------------------*
 * 			     Technique Parameters			    *
 *--------------------------------------------------------------------------*/

/* Color Alloc */

typedef struct {
    unsigned long	fill;
} XieColorAllocAllParam;

typedef struct {
    float	match_limit;
    float	gray_limit;
} XieColorAllocMatchParam;

typedef struct {
    unsigned long	max_cells;
} XieColorAllocRequantizeParam;


/* Colorspace - conversion from RGB */

typedef struct {
    XieMatrix			matrix;
    XieWhiteAdjustTechnique	white_adjust_tech;
    XiePointer			white_adjust_param;
} XieRGBToCIELabParam, XieRGBToCIEXYZParam;

typedef struct {
    XieLevels	levels;
    float	luma_red;
    float	luma_green;
    float	luma_blue;
    XieConstant	bias;
} XieRGBToYCbCrParam;

typedef struct {
    XieLevels	levels;
    float	luma_red;
    float	luma_green;
    float	luma_blue;
    float	scale;
} XieRGBToYCCParam;


/* Colorspace - conversion to RGB */

typedef struct {
    XieMatrix			matrix;
    XieWhiteAdjustTechnique	white_adjust_tech;
    XiePointer			white_adjust_param;
    XieGamutTechnique		gamut_tech;
    XiePointer			gamut_param;
} XieCIELabToRGBParam, XieCIEXYZToRGBParam;

typedef struct {
    XieLevels		levels;
    float		luma_red;
    float		luma_green;
    float		luma_blue;
    XieConstant		bias;
    XieGamutTechnique	gamut_tech;
    XiePointer		gamut_param;
} XieYCbCrToRGBParam;

typedef struct {
    XieLevels		levels;
    float		luma_red;
    float		luma_green;
    float		luma_blue;
    float		scale;
    XieGamutTechnique	gamut_tech;
    XiePointer		gamut_param;
} XieYCCToRGBParam;

/* Constrain */

typedef struct {
	XieConstant input_low,input_high;
	XieLTriplet output_low,output_high;
} XieClipScaleParam;


/* Convolve */

typedef struct {
    XieConstant	constant;
} XieConvolveConstantParam;


/* Decode */

typedef struct {
    XieOrientation	fill_order;
    XieOrientation	pixel_order;
    unsigned int	pixel_stride;
    unsigned int	left_pad;
    unsigned int	scanline_pad;
} XieDecodeUncompressedSingleParam;

typedef struct {
    unsigned char	left_pad[3];
    XieOrientation	fill_order;
    unsigned char	pixel_stride[3];
    XieOrientation	pixel_order;
    unsigned char	scanline_pad[3];
    XieOrientation  	band_order;
    XieInterleave	interleave;
} XieDecodeUncompressedTripleParam;

typedef struct {
    XieOrientation	encoded_order;
    Bool		normal;
    Bool		radiometric;
} XieDecodeG31DParam, XieDecodeG32DParam, XieDecodeG42DParam,
  XieDecodeTIFF2Param;

typedef struct {
    XieOrientation	encoded_order;
    Bool		normal;
} XieDecodeTIFFPackBitsParam;

typedef struct {
    XieInterleave	interleave;
    XieOrientation  	band_order;
    Bool		up_sample;
} XieDecodeJPEGBaselineParam;

typedef struct {
    XieInterleave	interleave;
    XieOrientation  	band_order;
} XieDecodeJPEGLosslessParam;


/* Dither */

typedef struct {
    unsigned int	threshold_order;
} XieDitherOrderedParam;


/* Encode */

typedef struct {
    XieOrientation	fill_order;
    XieOrientation	pixel_order;
    unsigned int	pixel_stride;
    unsigned int	scanline_pad;
} XieEncodeUncompressedSingleParam;

typedef struct {
    unsigned char	pixel_stride[3];
    XieOrientation	pixel_order;
    unsigned char	scanline_pad[3];
    XieOrientation	fill_order;
    XieOrientation  	band_order;
    XieInterleave	interleave;
} XieEncodeUncompressedTripleParam;

typedef struct {
    Bool		align_eol;
    Bool		radiometric;
    XieOrientation	encoded_order;
} XieEncodeG31DParam;

typedef struct {
    Bool		uncompressed;
    Bool		align_eol;
    Bool		radiometric;
    XieOrientation	encoded_order;
    unsigned long	k_factor;
} XieEncodeG32DParam;

typedef struct {
    Bool		uncompressed;
    Bool		radiometric;
    XieOrientation	encoded_order;
} XieEncodeG42DParam;

typedef struct {
    unsigned int	preference;
} XieEncodeServerChoiceParam;

typedef struct {
    XieInterleave	interleave;
    XieOrientation  	band_order;
    unsigned char	horizontal_samples[3];
    unsigned char	vertical_samples[3];
    char		*q_table;
    unsigned int	q_size;
    char		*ac_table;
    unsigned int	ac_size;
    char		*dc_table;
    unsigned int	dc_size;
} XieEncodeJPEGBaselineParam;

typedef struct {
    XieInterleave	interleave;
    XieOrientation  	band_order;
    unsigned char	predictor[3];
    char		*table;
    unsigned int	table_size;
} XieEncodeJPEGLosslessParam;

typedef struct {
    XieOrientation	encoded_order;
    Bool		radiometric;
} XieEncodeTIFF2Param; 

typedef struct {
    XieOrientation	encoded_order;
} XieEncodeTIFFPackBitsParam;


/* Geometry */

typedef struct {
    int	simple;
} XieGeomAntialiasByAreaParam;

typedef struct {
    int	kernel_size;
} XieGeomAntialiasByLowpassParam;

typedef struct {
    float		sigma;
    float		normalize;
    unsigned int	radius;
    Bool		simple;
} XieGeomGaussianParam;

typedef struct {
    unsigned int	modify;
} XieGeomNearestNeighborParam;


/* Histogram */

typedef struct {
    float	mean;
    float	sigma;
} XieHistogramGaussianParam;

typedef struct {
    float	constant;
    Bool	shape_factor;
} XieHistogramHyperbolicParam;


/* White Adjust */

typedef struct {
    XieConstant	white_point;
} XieWhiteAdjustCIELabShiftParam;


/*--------------------------------------------------------------------------*
 * 				  Events				    *
 *--------------------------------------------------------------------------*/

typedef struct {
    int				type;
    unsigned long		serial;
    Bool			send_event;
    Display			*display;
    unsigned long		name_space;
    Time			time;
    unsigned long		flo_id;
    XiePhototag			src;
    unsigned int		elem_type;
    XieColorList		color_list;
    XieColorAllocTechnique	color_alloc_technique;
    unsigned long		color_alloc_data;
} XieColorAllocEvent;

typedef struct {
    int				type;
    unsigned long		serial;
    Bool			send_event;
    Display			*display;
    unsigned long		name_space;
    Time			time;
    unsigned long		flo_id;
    XiePhototag			src;
    unsigned int		elem_type;
    XieDecodeTechnique		decode_technique;
    Bool			aborted;
    unsigned int		band_number;
    unsigned long		width;
    unsigned long		height;
} XieDecodeNotifyEvent;

typedef struct {
    int				type;
    unsigned long		serial;
    Bool			send_event;
    Display			*display;
    unsigned long		name_space;
    Time			time;
    unsigned long		flo_id;
    XiePhototag			src;
    unsigned int		elem_type;
    unsigned int		band_number;
    unsigned long		data[3];
} XieExportAvailableEvent;

typedef struct {
    int				type;
    unsigned long		serial;
    Bool			send_event;
    Display			*display;
    unsigned long		name_space;
    Time			time;
    unsigned long		flo_id;
    XiePhototag			src;
    unsigned int		elem_type;
    Window			window;
    int				x;
    int				y;
    unsigned int		width;
    unsigned int		height;
} XieImportObscuredEvent;

typedef struct {
    int				type;
    unsigned long		serial;
    Bool			send_event;
    Display			*display;
    unsigned long		name_space;
    Time			time;
    unsigned long		flo_id;
    XiePhotofloOutcome		outcome;
} XiePhotofloDoneEvent;


/*--------------------------------------------------------------------------*
 * 			         Photoflo Errors			    *
 *--------------------------------------------------------------------------*/

typedef struct {
    int			type;
    Display		*display;
    unsigned long	flo_id;
    unsigned long	serial;
    unsigned char	error_code;
    unsigned char	request_code;
    unsigned char	minor_code;
    unsigned int	flo_error_code;
    unsigned long	name_space;
    XiePhototag		phototag;
    unsigned int	elem_type;
} XieFloAccessError, XieFloAllocError, XieFloElementError, XieFloIDError,
  XieFloLengthError, XieFloMatchError, XieFloSourceError;

typedef struct {
    int			type;
    Display		*display;
    unsigned long	flo_id;
    unsigned long	serial;
    unsigned char	error_code;
    unsigned char	request_code;
    unsigned char	minor_code;
    unsigned int	flo_error_code;
    unsigned long	name_space;
    XiePhototag		phototag;
    unsigned int	elem_type;
    XID			resource_id;
} XieFloResourceError;

typedef struct {
    int			type;
    Display		*display;
    unsigned long	flo_id;
    unsigned long	serial;
    unsigned char	error_code;
    unsigned char	request_code;
    unsigned char	minor_code;
    unsigned int	flo_error_code;
    unsigned long	name_space;
    XiePhototag		phototag;
    unsigned int	elem_type;
    XiePhototag		domain_src;
} XieFloDomainError;

typedef struct {
    int			type;
    Display		*display;
    unsigned long	flo_id;
    unsigned long	serial;
    unsigned char	error_code;
    unsigned char	request_code;
    unsigned char	minor_code;
    unsigned int	flo_error_code;
    unsigned long	name_space;
    XiePhototag		phototag;
    unsigned int	elem_type;
#if defined(__cplusplus) || defined(c_plusplus)
    unsigned int	c_operator;
#else
    unsigned int	operator;
#endif
} XieFloOperatorError;

typedef struct {
    int			type;
    Display		*display;
    unsigned long	flo_id;
    unsigned long	serial;
    unsigned char	error_code;
    unsigned char	request_code;
    unsigned char	minor_code;
    unsigned int	flo_error_code;
    unsigned long	name_space;
    XiePhototag		phototag;
    unsigned int	elem_type;
    unsigned int	technique_number;
    unsigned int	num_tech_params;
    XieTechniqueGroup	tech_group;
} XieFloTechniqueError;

typedef struct {
    int			type;
    Display		*display;
    unsigned long	flo_id;
    unsigned long	serial;
    unsigned char	error_code;
    unsigned char	request_code;
    unsigned char	minor_code;
    unsigned int	flo_error_code;
    unsigned long	name_space;
    XiePhototag		phototag;
    unsigned int	elem_type;
    unsigned long	bad_value;
} XieFloValueError;


/*--------------------------------------------------------------------------*
 *                            Function prototypes			    *
 *--------------------------------------------------------------------------*/

/* Startup functions -------------------------------------------------------*/

extern Status XieInitialize (
#if NeedFunctionPrototypes
    Display *		/* display */,
    XieExtensionInfo **	/* extinfo_ret */
#endif
);

extern Status XieQueryTechniques (
#if NeedFunctionPrototypes
    Display *		/* display */,
    XieTechniqueGroup	/* technique_group */,
    int *		/* ntechniques_ret */,
    XieTechnique **	/* techniques_ret */
#endif
);

extern void XieFreeTechniques (
#if NeedFunctionPrototypes
    XieTechnique *	/* techs */,
    unsigned int	/* count */
#endif
);


/* Color List functions ---------------------------------------------------*/

extern XieColorList XieCreateColorList (
#if NeedFunctionPrototypes
    Display *		/* display */
#endif
);

extern void XieDestroyColorList (
#if NeedFunctionPrototypes
    Display *		/* display */,
    XieColorList	/* color_list */
#endif
);

extern void XiePurgeColorList (
#if NeedFunctionPrototypes
    Display *		/* display */,
    XieColorList	/* color_list */
#endif
);

extern Status XieQueryColorList (
#if NeedFunctionPrototypes
    Display *		/* display */,
    XieColorList  	/* color_list */,
    Colormap *		/* colormap_ret */,
    unsigned *		/* ncolors_ret */,
    unsigned long **	/* colors_ret */
#endif
);


/* LUT functions -----------------------------------------------------------*/

extern XieLut XieCreateLUT (
#if NeedFunctionPrototypes
    Display *		/* display */
#endif
);

extern void XieDestroyLUT (
#if NeedFunctionPrototypes
    Display *		/* display */,
    XieLut		/* lut */
#endif
);


/* Photomap functions ------------------------------------------------------*/

extern XiePhotomap XieCreatePhotomap (
#if NeedFunctionPrototypes
    Display *		/* display */
#endif
);

extern void XieDestroyPhotomap (
#if NeedFunctionPrototypes
    Display *		/* display */,
    XiePhotomap		/* photomap */
#endif
);

extern Status XieQueryPhotomap (
#if NeedFunctionPrototypes
    Display *			/* display */,
    XiePhotomap			/* photomap */,
    Bool *			/* populated_ret */,
    XieDataType *		/* datatype_ret */,
    XieDataClass *		/* class_ret */,
    XieDecodeTechnique *	/* decode_technique_ret */,
    XieLTriplet			/* width_ret */,
    XieLTriplet			/* height_ret */,
    XieLTriplet			/* levels_ret */
#endif
);


/* ROI functions -----------------------------------------------------------*/

extern XieRoi XieCreateROI (
#if NeedFunctionPrototypes
    Display *		/* display */
#endif
);

extern void XieDestroyROI (
#if NeedFunctionPrototypes
    Display *		/* display */,
    XieRoi		/* roi */
#endif
);


/* Photospace functions ----------------------------------------------------*/

extern XiePhotospace XieCreatePhotospace (
#if NeedFunctionPrototypes
    Display *		/* display */
#endif
);

extern void XieDestroyPhotospace (
#if NeedFunctionPrototypes
    Display *		/* display */,
    XiePhotospace	/* photospace */
#endif
);

extern void XieExecuteImmediate (
#if NeedFunctionPrototypes
    Display *		/* display */,
    XiePhotospace	/* photospace */,
    unsigned long	/* flo_id */,
    Bool		/* notify */,
    XiePhotoElement *	/* elem_list */,
    int			/* elem_count */
#endif
);


/* Photoflo functions ------------------------------------------------------*/

extern XiePhotoElement *XieAllocatePhotofloGraph (
#if NeedFunctionPrototypes
    unsigned int	/* count */
#endif
);

extern void XieFreePhotofloGraph (
#if NeedFunctionPrototypes
    XiePhotoElement *	/* elements */,
    unsigned int	/* count */
#endif
);

extern XiePhotoflo XieCreatePhotoflo (
#if NeedFunctionPrototypes
    Display *		/* display */,
    XiePhotoElement *	/* elem_list */,
    int			/* elem_count */
#endif
);

extern void XieDestroyPhotoflo (
#if NeedFunctionPrototypes
    Display *		/* display */,
    XiePhotoflo		/* photoflo */
#endif
);

extern void XieExecutePhotoflo (
#if NeedFunctionPrototypes
    Display *		/* display */,
    XiePhotoflo		/* photoflo */,
    Bool		/* notify */
#endif
);

extern void XieModifyPhotoflo (
#if NeedFunctionPrototypes
    Display *		/* display */,
    XiePhotoflo		/* photoflo */,
    int			/* start */,
    XiePhotoElement *	/* elem_list */,
    int			/* elem_count */
#endif
);

extern void XieRedefinePhotoflo (
#if NeedFunctionPrototypes
    Display *		/* display */,
    XiePhotoflo		/* photoflo */,
    XiePhotoElement *	/* elem_list */,
    int			/* elem_count */
#endif
);

extern Status XieQueryPhotoflo (
#if NeedFunctionPrototypes
    Display *		/* display */,
    unsigned long	/* name_space */,
    unsigned long	/* flo_id */,
    XiePhotofloState *	/* state_ret */,
    XiePhototag **	/* data_expected_ret */,
    unsigned int *	/* nexpected_ret */,
    XiePhototag **	/* data_available_ret */,
    unsigned int *	/* navailable_ret */
#endif
);


/* Client Data functions ---------------------------------------------------*/

extern void XiePutClientData (
#if NeedFunctionPrototypes
    Display *     	/* display */,
    unsigned long  	/* name_space */,
    unsigned long  	/* flo_id */,
    XiePhototag		/* element */,
    Bool         	/* final */,
    unsigned     	/* band_number */,
    unsigned char *     /* data */,
    unsigned     	/* nbytes */
#endif
);

extern Status XieGetClientData (
#if NeedFunctionPrototypes
    Display *      	/* display */,
    unsigned long  	/* name_space */,
    unsigned long  	/* flo_id */,
    XiePhototag		/* element */,
    unsigned  		/* max_bytes */,
    Bool		/* terminate */,
    unsigned     	/* band_number */,
    XieExportState * 	/* new_state_ret */,
    unsigned char **    /* data_ret */,
    unsigned *     	/* nbytes_ret */
#endif
);


/* Abort and Await functions -----------------------------------------------*/

extern void XieAbort (
#if NeedFunctionPrototypes
    Display *		/* display */,
    unsigned long	/* name_space */,
    unsigned long	/* flo_id */
#endif
);

extern void XieAwait (
#if NeedFunctionPrototypes
   Display *		/* display */,
   unsigned long	/* name_space */,
   unsigned long	/* flo_id */
#endif
);


/* Photoflo element functions ----------------------------------------------*/

extern void XieFloImportClientLUT (
#if NeedFunctionPrototypes
    XiePhotoElement *	/* element */,
    XieDataClass 	/* data_class */,
    XieOrientation	/* band_order */,
    XieLTriplet		/* length */,
    XieLevels		/* levels */
#endif
);

extern void XieFloImportClientPhoto (
#if NeedFunctionPrototypes
    XiePhotoElement *	/* element */,
    XieDataClass 	/* data_class */,
    XieLTriplet		/* width */,
    XieLTriplet		/* height */,
    XieLevels		/* levels */,
    Bool		/* notify */,
    XieDecodeTechnique	/* decode_tech */,
    XiePointer		/* decode_param */
#endif
);

extern void XieFloImportClientROI (
#if NeedFunctionPrototypes
    XiePhotoElement *	/* element */,
    unsigned int	/* rectangles */
#endif
);

extern void XieFloImportDrawable (
#if NeedFunctionPrototypes
    XiePhotoElement *	/* element */,
    Drawable		/* drawable */,
    int			/* src_x */,
    int			/* src_y */,
    unsigned int	/* width */,
    unsigned int	/* height */,
    unsigned long	/* fill */,
    Bool		/* notify */
#endif
);

extern void XieFloImportDrawablePlane (
#if NeedFunctionPrototypes
    XiePhotoElement *	/* element */,
    Drawable		/* drawable */,
    int			/* src_x */,
    int			/* src_y */,
    unsigned int	/* width */,
    unsigned int	/* height */,
    unsigned long	/* fill */,
    unsigned long	/* bit_plane */,
    Bool		/* notify */
#endif
);

extern void XieFloImportLUT (
#if NeedFunctionPrototypes
    XiePhotoElement *	/* element */,
    XieLut		/* lut */
#endif
);

extern void XieFloImportPhotomap (
#if NeedFunctionPrototypes
    XiePhotoElement *	/* element */,
    XiePhotomap		/* photomap */,
    Bool		/* notify */
#endif
);

extern void XieFloImportROI (
#if NeedFunctionPrototypes
    XiePhotoElement *	/* element */,
    XieRoi		/* roi */
#endif
);

extern void XieFloArithmetic (
#if NeedFunctionPrototypes
    XiePhotoElement *	/* element */,
    XiePhototag		/* src1 */,
    XiePhototag		/* src2 */,
    XieProcessDomain *	/* domain */,
    XieConstant		/* constant */,
    XieArithmeticOp	/* operator */,
    unsigned int	/* band_mask */
#endif
);

extern void XieFloBandCombine (
#if NeedFunctionPrototypes
    XiePhotoElement *	/* element */,
    XiePhototag		/* src1 */,
    XiePhototag		/* src2 */,
    XiePhototag		/* src3 */
#endif
);

extern void XieFloBandExtract (
#if NeedFunctionPrototypes
    XiePhotoElement *	/* element */,
    XiePhototag		/* src */,
    unsigned int	/* levels */,
    double		/* bias */,
    XieConstant		/* coefficients */
#endif
);

extern void XieFloBandSelect (
#if NeedFunctionPrototypes
    XiePhotoElement *	/* element */,
    XiePhototag		/* src */,
    unsigned int	/* band_number */
#endif
);

extern void XieFloBlend (
#if NeedFunctionPrototypes
    XiePhotoElement *	/* element */,
    XiePhototag		/* src1 */,
    XiePhototag		/* src2 */,
    XieConstant		/* src_constant */,
    XiePhototag		/* alpha */,
    double		/* alpha_const */,
    XieProcessDomain *	/* domain */,
    unsigned int	/* band_mask */
#endif
);

extern void XieFloCompare (
#if NeedFunctionPrototypes
    XiePhotoElement *	/* element */,
    XiePhototag		/* src1 */,
    XiePhototag		/* src2 */,
    XieProcessDomain *	/* domain */,
    XieConstant		/* constant */,
    XieCompareOp	/* operator */,
    Bool		/* combine */,
    unsigned int	/* band_mask */
#endif
);

extern void XieFloConstrain (
#if NeedFunctionPrototypes
    XiePhotoElement *		/* element */,
    XiePhototag			/* src */,
    XieLevels			/* levels */,
    XieConstrainTechnique	/* constrain_tech */,
    XiePointer			/* constrain_param */
#endif
);

extern void XieFloConvertFromIndex (
#if NeedFunctionPrototypes
    XiePhotoElement *	/* element */,
    XiePhototag		/* src */,
    Colormap		/* colormap */,
    XieDataClass	/* data_class */,
    unsigned int	/* precision */
#endif
);

extern void XieFloConvertFromRGB (
#if NeedFunctionPrototypes
    XiePhotoElement *	/* element */,
    XiePhototag		/* src */,
    XieColorspace	/* color_space */,
    XiePointer		/* color_param */
#endif
);

extern void XieFloConvertToIndex (
#if NeedFunctionPrototypes
    XiePhotoElement *		/* element */,
    XiePhototag			/* src */,
    Colormap			/* colormap */,
    XieColorList		/* color_list */,
    Bool			/* notify */,
    XieColorAllocTechnique 	/* color_alloc_tech */,
    XiePointer			/* color_alloc_param */
#endif
);

extern void XieFloConvertToRGB (
#if NeedFunctionPrototypes
    XiePhotoElement *	/* element */,
    XiePhototag		/* src */,
    XieColorspace	/* color_space */,
    XiePointer		/* color_param */
#endif
);

extern void XieFloConvolve (
#if NeedFunctionPrototypes
    XiePhotoElement *		/* element */,
    XiePhototag			/* src */,
    XieProcessDomain *		/* domain */,
    float *			/* kernel */,
    int				/* kernel_size */,
    unsigned int		/* band_mask */,
    XieConvolveTechnique	/* convolve_tech */,
    XiePointer			/* convolve_param */
#endif
);

extern void XieFloDither (
#if NeedFunctionPrototypes
    XiePhotoElement *		/* element */,
    XiePhototag			/* src */,
    unsigned int		/* band_mask */,
    XieLevels			/* levels */,
    XieDitherTechnique		/* dither_tech */,
    XiePointer			/* dither_param */
#endif
);

extern void XieFloGeometry (
#if NeedFunctionPrototypes
    XiePhotoElement *		/* element */,
    XiePhototag			/* src */,
    unsigned int		/* width */,
    unsigned int		/* height */,
    float[6]			/* coefficients[6] */,
    XieConstant			/* constant */,
    unsigned int		/* band_mask */,
    XieGeometryTechnique	/* sample_tech */,
    XiePointer			/* sample_param */
#endif
);

extern void XieFloLogical (
#if NeedFunctionPrototypes
    XiePhotoElement *	/* element */,
    XiePhototag		/* src1 */,
    XiePhototag		/* src2 */,
    XieProcessDomain *	/* domain */,
    XieConstant		/* constant */,
    unsigned long	/* operator */,
    unsigned int	/* band_mask */
#endif
);

extern void XieFloMatchHistogram (
#if NeedFunctionPrototypes
    XiePhotoElement *	/* element */,
    XiePhototag		/* src */,
    XieProcessDomain *	/* domain */,
    XieHistogramShape	/* shape */,
    XiePointer		/* shape_param */
#endif
);

extern void XieFloMath (
#if NeedFunctionPrototypes
    XiePhotoElement *	/* element */,
    XiePhototag		/* src */,
    XieProcessDomain *	/* domain */,
    XieMathOp		/* operator */,
    unsigned int	/* band_mask */
#endif
);

extern void XieFloPasteUp (
#if NeedFunctionPrototypes
    XiePhotoElement *	/* element */,
    unsigned int	/* width */,
    unsigned int	/* height */,
    XieConstant		/* constant */,
    XieTile *		/* tiles */,
    unsigned int	/* tile_count */
#endif
);

extern void XieFloPoint (
#if NeedFunctionPrototypes
    XiePhotoElement *	/* element */,
    XiePhototag		/* src */,
    XieProcessDomain *	/* domain */,
    XiePhototag		/* lut */,
    unsigned int	/* band_mask */
#endif
);

extern void XieFloUnconstrain (
#if NeedFunctionPrototypes
    XiePhotoElement *	/* element */,
    XiePhototag		/* src */
#endif
);

extern void XieFloExportClientHistogram (
#if NeedFunctionPrototypes
    XiePhotoElement *	/* element */,
    XiePhototag		/* src */,
    XieProcessDomain *	/* domain */,
    XieExportNotify	/* notify */
#endif
);

extern void XieFloExportClientLUT (
#if NeedFunctionPrototypes
    XiePhotoElement *	/* element */,
    XiePhototag		/* src */,
    XieOrientation	/* band_order */,
    XieExportNotify	/* notify */,
    XieLTriplet 	/* start */,
    XieLTriplet 	/* length */
#endif
);

extern void XieFloExportClientPhoto (
#if NeedFunctionPrototypes
    XiePhotoElement *	/* element */,
    XiePhototag		/* src */,
    XieExportNotify	/* notify */,
    XieEncodeTechnique	/* encode_tech */,
    XiePointer		/* encode_param */
#endif
);

extern void XieFloExportClientROI (
#if NeedFunctionPrototypes
    XiePhotoElement *	/* element */,
    XiePhototag		/* src */,
    XieExportNotify	/* notify */
#endif
);

extern void XieFloExportDrawable (
#if NeedFunctionPrototypes
    XiePhotoElement *	/* element */,
    XiePhototag		/* src */,
    Drawable		/* drawable */,
    GC			/* gc */,
    int			/* dst_x */,
    int			/* dst_y */
#endif
);

extern void XieFloExportDrawablePlane (
#if NeedFunctionPrototypes
    XiePhotoElement *	/* element */,
    XiePhototag		/* src */,
    Drawable		/* drawable */,
    GC			/* gc */,
    int			/* dst_x */,
    int			/* dst_y */
#endif
);

extern void XieFloExportLUT (
#if NeedFunctionPrototypes
    XiePhotoElement *	/* element */,
    XiePhototag		/* src */,
    XieLut		/* lut */,
    Bool		/* merge */,
    XieLTriplet 	/* start */
#endif
);

extern void XieFloExportPhotomap (
#if NeedFunctionPrototypes
    XiePhotoElement *	/* element */,
    XiePhototag		/* src */,
    XiePhotomap		/* photomap */,
    XieEncodeTechnique	/* encode_tech */,
    XiePointer		/* encode_param */
#endif
);

extern void XieFloExportROI (
#if NeedFunctionPrototypes
    XiePhotoElement *	/* element */,
    XiePhototag		/* src */,
    XieRoi		/* roi */
#endif
);


/* Technique functions -----------------------------------------------------*/

extern XieColorAllocAllParam *XieTecColorAllocAll (
#if NeedFunctionPrototypes
    unsigned long	/* fill */
#endif
);

extern XieColorAllocMatchParam *XieTecColorAllocMatch (
#if NeedFunctionPrototypes
    double		/* match_limit */,
    double		/* gray_limit */
#endif
);

extern XieColorAllocRequantizeParam *XieTecColorAllocRequantize (
#if NeedFunctionPrototypes
    unsigned long	/* max_cells */
#endif
);

extern XieRGBToCIELabParam *XieTecRGBToCIELab (
#if NeedFunctionPrototypes
    XieMatrix			/* matrix */,
    XieWhiteAdjustTechnique	/* white_adjust_tech */,
    XiePointer			/* white_adjust_param */
#endif
);

extern XieRGBToCIEXYZParam *XieTecRGBToCIEXYZ (
#if NeedFunctionPrototypes
    XieMatrix			/* matrix */,
    XieWhiteAdjustTechnique	/* white_adjust_tech */,
    XiePointer			/* white_adjust_param */
#endif
);

extern XieRGBToYCbCrParam *XieTecRGBToYCbCr (
#if NeedFunctionPrototypes
    XieLevels		/* levels */,
    double		/* luma_red */,
    double		/* luma_green */,
    double		/* luma_blue */,
    XieConstant		/* bias */
#endif
);

extern XieRGBToYCCParam *XieTecRGBToYCC (
#if NeedFunctionPrototypes
    XieLevels		/* levels */,
    double		/* luma_red */,
    double		/* luma_green */,
    double		/* luma_blue */,
    double		/* scale */
#endif
);

extern XieCIELabToRGBParam *XieTecCIELabToRGB (
#if NeedFunctionPrototypes
    XieMatrix			/* matrix */,
    XieWhiteAdjustTechnique	/* white_adjust_tech */,
    XiePointer			/* white_adjust_param */,
    XieGamutTechnique		/* gamut_tech */,
    XiePointer			/* gamut_param */
#endif
);

extern XieCIEXYZToRGBParam *XieTecCIEXYZToRGB (
#if NeedFunctionPrototypes
    XieMatrix			/* matrix */,
    XieWhiteAdjustTechnique	/* white_adjust_tech */,
    XiePointer			/* white_adjust_param */,
    XieGamutTechnique		/* gamut_tech */,
    XiePointer			/* gamut_param */
#endif
);

extern XieYCbCrToRGBParam *XieTecYCbCrToRGB (
#if NeedFunctionPrototypes
    XieLevels		/* levels */,
    double		/* luma_red */,
    double		/* luma_green */,
    double		/* luma_blue */,
    XieConstant		/* bias */,
    XieGamutTechnique	/* gamut_tech */,
    XiePointer		/* gamut_param */
#endif
);

extern XieYCCToRGBParam *XieTecYCCToRGB (
#if NeedFunctionPrototypes
    XieLevels		/* levels */,
    double		/* luma_red */,
    double		/* luma_green */,
    double		/* luma_blue */,
    double		/* scale */,
    XieGamutTechnique	/* gamut_tech */,
    XiePointer		/* gamut_param */
#endif
);

extern XieClipScaleParam *XieTecClipScale (
#if NeedFunctionPrototypes
    XieConstant		/* in_low */,
    XieConstant		/* in_high */,
    XieLTriplet		/* out_low */,
    XieLTriplet		/* out_high */
#endif
);

extern XieConvolveConstantParam *XieTecConvolveConstant (
#if NeedFunctionPrototypes
    XieConstant		/* constant */
#endif
);

extern XieDecodeUncompressedSingleParam *XieTecDecodeUncompressedSingle (
#if NeedFunctionPrototypes
    XieOrientation	/* fill_order */,
    XieOrientation	/* pixel_order */,
    unsigned int	/* pixel_stride */,
    unsigned int	/* left_pad */,
    unsigned int	/* scanline_pad */
#endif
);

extern XieDecodeUncompressedTripleParam *XieTecDecodeUncompressedTriple (
#if NeedFunctionPrototypes
    XieOrientation	/* fill_order */,
    XieOrientation	/* pixel_order */,
    XieOrientation	/* band_order */,
    XieInterleave	/* interleave */,
    unsigned char[3]	/* pixel_stride[3] */,
    unsigned char[3]	/* left_pad[3] */,
    unsigned char[3]	/* scanline_pad[3] */
#endif
);

extern XieDecodeG31DParam *XieTecDecodeG31D (
#if NeedFunctionPrototypes
    XieOrientation	/* encoded_order */,
    Bool		/* normal */,
    Bool		/* radiometric */
#endif
);

extern XieDecodeG32DParam *XieTecDecodeG32D (
#if NeedFunctionPrototypes
    XieOrientation	/* encoded_order */,
    Bool		/* normal */,
    Bool		/* radiometric */
#endif
);

extern XieDecodeG42DParam *XieTecDecodeG42D (
#if NeedFunctionPrototypes
    XieOrientation	/* encoded_order */,
    Bool		/* normal */,
    Bool		/* radiometric */
#endif
);

extern XieDecodeTIFF2Param *XieTecDecodeTIFF2 (
#if NeedFunctionPrototypes
    XieOrientation	/* encoded_order */,
    Bool		/* normal */,
    Bool		/* radiometric */
#endif
);

extern XieDecodeTIFFPackBitsParam *XieTecDecodeTIFFPackBits (
#if NeedFunctionPrototypes
    XieOrientation	/* encoded_order */,
    Bool		/* normal */
#endif
);

extern XieDecodeJPEGBaselineParam *XieTecDecodeJPEGBaseline (
#if NeedFunctionPrototypes
    XieInterleave	/* interleave */,
    XieOrientation	/* band_order */,
    Bool		/* up_sample  */
#endif
);

extern XieDecodeJPEGLosslessParam *XieTecDecodeJPEGLossless (
#if NeedFunctionPrototypes
    XieInterleave	/* interleave */,
    XieOrientation	/* band_order */
#endif
);

extern XieDitherOrderedParam *XieTecDitherOrderedParam (
#if NeedFunctionPrototypes
    unsigned int	/* threshold_order */
#endif
);

extern XieEncodeUncompressedSingleParam *XieTecEncodeUncompressedSingle (
#if NeedFunctionPrototypes
    XieOrientation	/* fill_order */,
    XieOrientation	/* pixel_order */,
    unsigned int	/* pixel_stride */,
    unsigned int	/* scanline_pad */
#endif
);

extern XieEncodeUncompressedTripleParam *XieTecEncodeUncompressedTriple (
#if NeedFunctionPrototypes
    XieOrientation	/* fill_order */,
    XieOrientation	/* pixel_order */,
    XieOrientation	/* band_order */,
    XieInterleave	/* interleave */,
    unsigned char[3]	/* pixel_stride[3] */,
    unsigned char[3]	/* scanline_pad[3] */
#endif
);

extern XieEncodeG31DParam *XieTecEncodeG31D (
#if NeedFunctionPrototypes
    Bool		/* align_eol */,
    Bool		/* radiometric */,
    XieOrientation	/* encoded_order */
#endif
);

extern XieEncodeG32DParam *XieTecEncodeG32D (
#if NeedFunctionPrototypes
    Bool		/* uncompressed */,
    Bool		/* align_eol */,
    Bool		/* radiometric */,
    XieOrientation	/* encoded_order */,
    unsigned long	/* k_factor */
#endif
);

extern XieEncodeG42DParam *XieTecEncodeG42D (
#if NeedFunctionPrototypes
    Bool		/* uncompressed */,
    Bool		/* radiometric */,
    XieOrientation	/* encoded_order */
#endif
);

extern XieEncodeServerChoiceParam *XieTecEncodeServerChoice (
#if NeedFunctionPrototypes
    unsigned int	/* preference */
#endif
);

extern XieEncodeJPEGBaselineParam *XieTecEncodeJPEGBaseline (
#if NeedFunctionPrototypes
    XieInterleave	/* interleave */,
    XieOrientation	/* band_order */,
    unsigned char[3]	/* horizontal_samples[3] */,
    unsigned char[3]	/* vertical_samples[3] */,
    char *		/* q_table */,
    unsigned int	/* q_size */,
    char *		/* ac_table */,
    unsigned int	/* ac_size */,
    char *		/* dc_table */,
    unsigned int	/* dc_size */
#endif
);

extern void XieFreeEncodeJPEGBaseline (
#if NeedFunctionPrototypes
    XieEncodeJPEGBaselineParam *	/* param */
#endif
);

extern XieEncodeJPEGLosslessParam *XieTecEncodeJPEGLossless (
#if NeedFunctionPrototypes
    XieInterleave	/* interleave */,
    XieOrientation	/* band_order */,
    unsigned char[3]	/* predictor[3] */,
    char *		/* table */,
    unsigned int	/* table_size */
#endif
);

extern void XieFreeEncodeJPEGLossless (
#if NeedFunctionPrototypes
    XieEncodeJPEGLosslessParam *	/* param */
#endif
);

extern void XieFreePasteUpTiles (
#if NeedFunctionPrototypes
    XiePhotoElement *	/* element */
#endif
);

extern XieEncodeTIFF2Param *XieTecEncodeTIFF2 (
#if NeedFunctionPrototypes
    XieOrientation	/* encoded_order */,
    Bool		/* radiometric */
#endif
);

extern XieEncodeTIFFPackBitsParam *XieTecEncodeTIFFPackBits (
#if NeedFunctionPrototypes
    XieOrientation	/* encoded_order */
#endif
);

extern XieGeomAntialiasByAreaParam *XieTecGeomAntialiasByArea (
#if NeedFunctionPrototypes
    int			/* simple */
#endif
);

extern XieGeomAntialiasByLowpassParam *XieTecGeomAntialiasByLowpass (
#if NeedFunctionPrototypes
    int			/* kernel_size */
#endif
);

extern XieGeomGaussianParam *XieTecGeomGaussian (
#if NeedFunctionPrototypes
    double		/* sigma */,
    double		/* normalize */,
    unsigned int	/* radius */,
    Bool		/* simple */
#endif
);

extern XieGeomNearestNeighborParam *XieTecGeomNearestNeighbor (
#if NeedFunctionPrototypes
    unsigned int	/* modify */
#endif
);

extern XieHistogramGaussianParam *XieTecHistogramGaussian (
#if NeedFunctionPrototypes
    double		/* mean */,
    double		/* sigma */
#endif
);

extern XieHistogramHyperbolicParam *XieTecHistogramHyperbolic (
#if NeedFunctionPrototypes
    double		/* constant */,
    Bool		/* shape_factor */
#endif
);

extern XieWhiteAdjustCIELabShiftParam *XieTecWhiteAdjustCIELabShift (
#if NeedFunctionPrototypes
    XieConstant		/* white_point */
#endif
);


#endif /* _XIELIB_H_ */
