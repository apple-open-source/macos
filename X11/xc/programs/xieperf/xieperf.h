/* $Xorg: xieperf.h,v 1.4 2001/02/09 02:05:49 xorgcvs Exp $ */

/**** module xieperf.h ****/
/******************************************************************************

Copyright 1993, 1994, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.


				NOTICE
                              
This software is being provided by AGE Logic, Inc. under the
following license.  By obtaining, using and/or copying this software,
you agree that you have read, understood, and will comply with these
terms and conditions:

     Permission to use, copy, modify, distribute and sell this
     software and its documentation for any purpose and without
     fee or royalty and to grant others any or all rights granted
     herein is hereby granted, provided that you agree to comply
     with the following copyright notice and statements, including
     the disclaimer, and that the same appears on all copies and
     derivative works of the software and documentation you make.
     
     "Copyright 1993, 1994 by AGE Logic, Inc."
     
     THIS SOFTWARE IS PROVIDED "AS IS".  AGE LOGIC MAKES NO
     REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED.  By way of
     example, but not limitation, AGE LOGIC MAKE NO
     REPRESENTATIONS OR WARRANTIES OF MERCHANTABILITY OR FITNESS
     FOR ANY PARTICULAR PURPOSE OR THAT THE SOFTWARE DOES NOT
     INFRINGE THIRD-PARTY PROPRIETARY RIGHTS.  AGE LOGIC 
     SHALL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE.  IN NO
     EVENT SHALL EITHER PARTY BE LIABLE FOR ANY INDIRECT,
     INCIDENTAL, SPECIAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOSS
     OF PROFITS, REVENUE, DATA OR USE, INCURRED BY EITHER PARTY OR
     ANY THIRD PARTY, WHETHER IN AN ACTION IN CONTRACT OR TORT OR
     BASED ON A WARRANTY, EVEN IF AGE LOGIC LICENSEES
     HEREUNDER HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH
     DAMAGES.
    
     The name of AGE Logic, Inc. may not be used in
     advertising or publicity pertaining to this software without
     specific, written prior permission from AGE Logic.

     Title to this software shall at all times remain with AGE
     Logic, Inc.
*****************************************************************************
  
	xieperf.h -- xieperf header file

	Syd Logan -- AGE Logic, Inc.
  
*****************************************************************************/
/* $XFree86: xc/programs/xieperf/xieperf.h,v 1.6 2001/12/14 20:01:53 dawes Exp $ */

#include <stdio.h>
#ifndef VMS
#include <X11/Xatom.h>
#include <X11/Xos.h>
#else
#include <decw$include/Xatom.h>
#endif

#ifndef VMS
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#else
#include <decw$include/Xlib.h>
#include <decw$include/Xutil.h>
#endif
#include <X11/Xfuncs.h>
#include <X11/extensions/XIElib.h>

#include <stddef.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#ifdef SIGNALRETURNSINT
#define SIGNAL_T int
#else
#define SIGNAL_T void
#endif

#define WIDTH         600	/* Size of large window to work within  */
#define HEIGHT        600

#define	MONWIDTH      350 
#define	MONHEIGHT     200 


#define VALL	   	 1		/* future use - see x11perf.h */

typedef unsigned char Version;		/* ditto */

#define	ClampInputs	1
#define	ClampOutputs	2

#define	Drawable	1
#define	DrawablePlane 	2

#define NoObscure 	0
#define Obscured	1
#define	Obscuring	2

#define Overlap		1
#define	NoOverlap	2

/* geometry stuff */

#define	GEO_TYPE_CROP		1
#define	GEO_TYPE_SCALE		2
#define	GEO_TYPE_MIRRORX	3
#define	GEO_TYPE_MIRRORY	4
#define	GEO_TYPE_MIRRORXY	5
#define GEO_TYPE_ROTATE		6
#define	GEO_TYPE_DEFAULT	7
#define GEO_TYPE_SCALEDROTATE   8

/* for the tests which can have ROIs and control planes, these are the  
   options. Cannot support ROIs and Control Planes at same time, so these
   are mutually exclusive choices */ 

#define DomainNone      0
#define DomainROI       1
#define DomainCtlPlane  2

/* capabilities masks */

/* the low 8 bits of the short are for test requirements. Bit positions not
   defined below are reserved for future expansion */

#define	CAPA_EVENT	( 1 << 0 )	
#define	CAPA_ERROR	( 1 << 1 )	

#define	CAPA_MASK	0x00ff

#define IsEvent(x)	( x & CAPA_EVENT ? 1 : 0 ) 
#define IsError(x)	( x & CAPA_ERROR ? 1 : 0 ) 

#define IsFaxImage( x ) ( x == xieValDecodeG42D   	||		\
			  x == xieValDecodeG32D   	||		\
			  x == xieValDecodeTIFFPackBits ||		\
			  x == xieValDecodeTIFF2   	||		\
			  x == xieValDecodeG31D ) 

#define IsColorVisual( visclass ) ( visclass == StaticColor || visclass == \
	PseudoColor || visclass == DirectColor || visclass == TrueColor ? 1 : 0 )

#define IsTrueColorOrDirectColor( visclass ) ( visclass == TrueColor || \
	visclass == DirectColor )

#define IsGrayVisual( visclass ) ( !IsColorVisual( visclass ) )

#define IsStaticVisual( visclass ) ( visclass == StaticColor || visclass == TrueColor\
	 || visclass == StaticGray )

#define IsDynamicVisual( visclass ) ( !IsStaticVisual( visclass ) )

/* protocol subset masks */

/* the high 8 bits of the short are for XIE subsets. Again, those bits not
   defined are reserved for future use */

#define SUBSET_MASK	0xff00
#define	SUBSET_FULL	( xieValFull << 8 )
#define SUBSET_DIS	( xieValDIS << 8 )

#ifndef MAX
#define MAX( a, b ) ( a > b ? a : b )
#endif
#ifndef MIN
#define MIN( a, b ) ( a < b ? a : b )
#endif

#define	IsFull( x ) ( x & SUBSET_FULL ? 1 : 0 )
#define IsDIS( x ) ( x & SUBSET_DIS ? 1 : 0 )
#define IsDISAndFull( x ) ( IsFullTest( x ) && IsDISTest( x ) )

/*
 * configuration shared by all tests 
 */
 
/* image configuration - could be nicer, but put info for all
   decode techniques in this one struct */

typedef struct _Image {
    char	*fname;		/* filename */
    int		fsize;	      	/* size in bytes. set in init function */
    int		bandclass;     	/* singleband or tripleband */
    int		width[ 3 ];	/* width of image */
    int         height[ 3 ];    /* height of image */
    int         depth[ 3 ];     /* pixel depth */
    unsigned long levels[ 3 ];	/* 1 << depth */
    int		decode;	     	/* decode method */
    int         fill_order;	
    int         pixel_order;	
    int         pixel_stride[ 3 ];	
    int         scanline_pad[ 3 ];	
    int         left_pad[ 3 ];	
    int		interleave;	
    int		band_order;	
    unsigned int chksum;
    char        *data;       /* image data */
} XIEimage;

/* a file represents an image. 4 files per test are supported */

typedef struct _XIEfile {
    XIEimage	*image1;	
    XIEimage	*image2;	
    XIEimage	*image3;	
    XIEimage	*image4;	
} XIEifile;

/* test parameters */

typedef struct _Parms {
    /* Required fields */
    int  	objects;    /* Number of objects to process in one X call */
    /* Optional fields */
    int		description; /* server requirements flags */
    int         buffer_size; /* size when sending/reading data from xie */
    XIEifile 	finfo;      /* image file info */	
    XPointer	ts;	    /* test specifics */		
} ParmRec, *Parms;

/*
 * test specific configuration. One structure per C source file.
 */

typedef struct _abortParms {
    int 	lutSize; 
    int		lutLevels;
} AbortParms;

typedef struct _encodeParms {
	int	encode;		/* encode technique */
	char	*parms;		/* encode technique structs */
	Bool	exportClient;	/* if True, ECP. Else, EP */
	unsigned char scale;	/* For JPEG q_table scaling */
} EncodeParms;

typedef struct _eventParms {
    int		event;
} EventParms;

typedef struct _rgbParms {
	int			colorspace;
	int			which;
#define RGB_FF 1
#define RGB_IF 2
#define RGB_II 3
	XieMatrix		toMatrix;
	XieMatrix		fromMatrix;
	XieWhiteAdjustTechnique whiteAdjust;
	XieConstant		whitePoint;
	XieGamutTechnique	gamut;
        XieLTriplet		RGBLevels;
	XieConstant 		luma;
	XieConstant		bias;
	XieFloat		scale;
} RGBParms;

typedef struct _errorParms {
    int		error;
} ErrorParms;

typedef struct _awaitParms {
    int		lutSize;
    int		lutLevels;
} AwaitParms;

#define	BandExtract 1
#define BandSelect  2

typedef struct _bandParms {
    XieConstant c1;
    XieConstant c2;
    XieConstant c3;
    int which;		/* BandExtract or BandSelect */
    float bias;
    Atom atom;
    Bool useStdCmap;
} BandParms;

typedef struct _blendParms {
    XieConstant constant;
    XieFloat 	alphaConstant;
    int		bandMask;
    int         useDomain;
    int         numROIs;
} BlendParms;

typedef struct _constrainParms {
    Bool 	photoDest;
    int		constrain;
    int		clamp;
} ConstrainParms;

typedef struct _creatDstryParms {
    int		dummy;
} CreateDestroyParms;

typedef struct _cvtToIndexParms {
    int		dither;
    Bool	useDefaultCmap;
    Bool	flo_notify;
    Bool	addCvtFromIndex;
} CvtToIndexParms;

typedef struct _ditherParms {
    int		dither;
    int		drawable;
    float	threshold;
    int		bandMask;
} DitherParms;

typedef struct _exportClParms {
    int		numROIs;	/* ExportClientROI */
	
    /* following are for ExportClientHistogram with ROIs */

    int			useDomain;
    short		x;
    short		y;
    short		width;
    short		height;
} ExportClParms;

typedef struct _geometryParms {
    int		geoType;
    int		geoHeight;
    int		geoWidth;
    int		geoXOffset;
    int		geoYOffset;
    XieGeometryTechnique geoTech;
    int		geoAngle;
    Bool 	radiometric;
} GeometryParms;

typedef struct _logicalParms {
    XieConstant	logicalConstant;
    unsigned long logicalOp;
    int		logicalBandMask;
    int		useDomain;
    int		numROIs;
} LogicalParms;

typedef struct _importParms {
    int		obscure;
    int		numROIs;
} ImportParms;

typedef struct _importClParms {
    int		numROIs;
} ImportClParms;

typedef struct _pasteUpParms {
    int		overlap;
} PasteUpParms;

typedef struct _redefineParms {
    XieConstant	constant;
    int		bandMask;
    unsigned long op1;
    unsigned long op2;
} RedefineParms;

typedef struct _modifyParms {
    XieConstant	constant;
    int		bandMask;
    unsigned long op;
} ModifyParms;

typedef struct _pointParms {
    Bool	photoDest;
    int		levelsIn;
    int		levelsOut;
    int 	useDomain;
    short	x;
    short	y;
    short	width;
    short	height;
    int		bandMask;
} PointParms;

typedef struct _funnyEncodeParms {
    int			floElements;
    XieOrientation	*fillOrder;
    XieOrientation	*pixelOrder;
    unsigned char 	*pixelStride;	
    unsigned char	*scanlinePad;
    XieOrientation	*bandOrder;
    XieInterleave	*interleave;
    Bool		useMyLevelsPlease;
    XieLTriplet		myBits;
} FunnyEncodeParms;
    
typedef struct _triplePointParms {
    int 	useDomain;
    short	x;
    short	y;
    short	width;
    short	height;
    Atom	atom;
    int		bandMask;
    int 	ditherTech;
    int		threshold;
} TriplePointParms;

typedef struct _unconstrainParms {
    int		constrain;
} UnconstrainParms;

typedef struct _purgeColStParms {
    int		dummy;
} PurgeColStParms;

typedef struct _compareParms {
    XieCompareOp	op;
    XieConstant		constant;
    Bool		combine;
    unsigned int	bandMask;
    int			useDomain;
    short		x;
    short		y;
    short		width;
    short		height;
} CompareParms;

typedef struct _arithmeticParms {
    XieArithmeticOp	op;
    XieConstant		constant;
    unsigned int	bandMask;
    int			useDomain;
    short		x;
    short		y;
    short		width;
    short		height;
    Bool		constrain;
    XieConstrainTechnique constrainTech;	
    float		inLow;			
    float		inHigh;			
} ArithmeticParms;

typedef struct _mathParms {
    XieMathOp		*ops;
    int			nops; 
    unsigned int	bandMask;
    int			useDomain;
    short		x;
    short		y;
    short		width;
    short		height;
    Bool		constrain;
    XieConstrainTechnique constrainTech;
    float		inLow;
    float		inHigh;
} MathParms;

typedef struct _convolveParms {
    Bool		photoDest;
    unsigned int	bandMask;
    int			useDomain;
    short		x;
    short		y;
    short		width;
    short		height;
    XieConvolveTechnique tech;
    XieConstant		constant;
    int			( * kfunc )(float **);
} ConvolveParms;

typedef struct _queryParms {
    int		lutSize;
    int		lutLevels;
    XieTechniqueGroup techGroup;
} QueryParms;

typedef struct _matchHistogramParms {
    XieHistogramShape	shape;
    double		mean;
    double		sigma;
    double		constant;
    Bool		shape_factor;
    int			useDomain;
    short		x;
    short		y;
    short		width;
    short		height;
} MatchHistogramParms;

typedef struct _XParms {
    Display	    *d;
    char	    *displayName;	/* see do_await.c */
    Window	    w;
    Window	    p;
    GC		    fggc;
    GC		    bggc;
    unsigned long   foreground;
    unsigned long   background;
    XVisualInfo     vinfo;
    Bool	    pack;
    Version	    version;
    int		    screenDepth;	/* effective depth of drawables */
} XParmRec, *XParms;

typedef Bool (*InitProc)    (XParms, Parms, int);
typedef void (*Proc)	    (XParms, Parms, int);
typedef void (*CleanupProc) (XParms xp, Parms p);

typedef int TestType;

typedef struct _Test {
    char	*option;    /* Name to use in prompt line		    */
    char	*label;     /* Fuller description of test		    */
    InitProc    init;       /* Initialization procedure			    */
    Proc	proc;       /* Timed benchmark procedure		    */
    CleanupProc	passCleanup;/* Cleanup between repetitions of same test     */
    CleanupProc	cleanup;    /* Cleanup after test			    */
    Version     versions;   /* future expansion	    */
    TestType    testType;   /* future expansion     */
    int		clips;      /* Number of obscuring windows to force clipping*/
    ParmRec     parms;      /* Parameters passed to test procedures	    */
} Test;

extern Test test[];

#define ForEachTest(x) for (x = 0; test[x].option != NULL; x++)

/* abort.c */
extern int InitAbort ( XParms xp, Parms p, int reps );
extern void DoAbort ( XParms xp, Parms p, int reps );
extern void EndAbort ( XParms xp, Parms p );
extern void FreeAbortStuff ( XParms xp, Parms p );

/* arith.c */
extern int InitArithmetic ( XParms xp, Parms p, int reps );
extern void DoArithmetic ( XParms xp, Parms p, int reps );
extern void EndArithmetic ( XParms xp, Parms p );
extern void FreeArithStuff ( XParms xp, Parms p );

/* await.c */
extern int InitAwait ( XParms xp, Parms p, int reps );
extern void AbortFlo ( XParms xp );
extern SIGNAL_T AwaitHandler ( int sig );
extern void DoAwait ( XParms xp, Parms p, int reps );
extern void EndAwait ( XParms xp, Parms p );
extern void FreeAwaitStuff ( XParms xp, Parms p );

/* band.c */
extern int InitBandSelectExtract ( XParms xp, Parms p, int reps );
extern int InitBandColormap ( XParms xp, Parms p, int reps );
extern int InitBandCombine ( XParms xp, Parms p, int reps );
extern int CreateColorBandSelectExtractFlo ( XParms xp, Parms p );
extern void DoBand ( XParms xp, Parms p, int reps );
extern void EndBandCombine ( XParms xp, Parms p );
extern void EndBandColormap ( XParms xp, Parms p );
extern void EndBandSelectExtract ( XParms xp, Parms p );
extern void FreeBandStuff ( XParms xp, Parms p );

/* blend.c */
extern int InitBlend ( XParms xp, Parms p, int reps );
extern void DoBlend ( XParms xp, Parms p, int reps );
extern void EndBlend ( XParms xp, Parms p );
extern void FreeBlendStuff ( XParms xp, Parms p );

/* cache.c */
extern void CacheInit ( void );
extern void FlushCache ( void );
extern int SetImageActiveState ( XIEimage *image, Bool active );
extern int SetPhotomapActiveState ( XiePhotomap pId, Bool active );
extern int AddToCache ( XIEimage *image, XiePhotomap pId );
extern Bool IsImageInCache ( XIEimage *image );
extern Bool IsPhotomapInCache ( XiePhotomap pId );
extern XiePhotomap PhotomapOfImage ( XIEimage *image );
extern int RemoveImageFromCache ( XIEimage *image );
extern int RemovePhotomapFromCache ( XiePhotomap pId );
extern int TouchImage ( XIEimage *image );
extern int TouchPhotomap ( XiePhotomap pId );
extern XIEimage * GetLRUImage ( void );
extern XiePhotomap GetLRUPhotomap ( void );
extern void DumpCache ( void );

/* compare.c */
extern int InitCompare ( XParms xp, Parms p, int reps );
extern void DoCompare ( XParms xp, Parms p, int reps );
extern void EndCompare ( XParms xp, Parms p );
extern void FreeCompareStuff ( XParms xp, Parms p );

/* complex.c */
extern int InitComplex ( XParms xp, Parms p, int reps );
extern int CreateComplexFlo ( XParms xp, Parms p );
extern void DoComplex ( XParms xp, Parms p, int reps );
extern void EndComplex ( XParms xp, Parms p );
extern void FreeComplexStuff ( XParms xp, Parms p );

/* constrain.c */
extern int InitConstrain ( XParms xp, Parms p, int reps );
extern void DoConstrain ( XParms xp, Parms p, int reps );
extern void EndConstrain ( XParms xp, Parms p );
extern void FreeConstrainStuff ( XParms xp, Parms p );

/* convolve.c */
extern int InitConvolve ( XParms xp, Parms p, int reps );
extern void DoConvolve ( XParms xp, Parms p, int reps );
extern void EndConvolve ( XParms xp, Parms p );
extern void FreeConvolveStuff ( XParms xp, Parms p );
extern int Boxcar3 ( float **data );
extern int Boxcar5 ( float **data );
extern int LaPlacian3 ( float **data );
extern int LaPlacian5 ( float **data );

/* creatdstry.c */
extern int InitCreateDestroyPhotoflo ( XParms xp, Parms p, int reps );
extern int InitCreateDestroy ( XParms xp, Parms p, int reps );
extern void DoCreateDestroyColorList ( XParms xp, Parms p, int reps );
extern void DoCreateDestroyLUT ( XParms xp, Parms p, int reps );
extern void DoCreateDestroyPhotomap ( XParms xp, Parms p, int reps );
extern void DoCreateDestroyROI ( XParms xp, Parms p, int reps );
extern void DoCreateDestroyPhotospace ( XParms xp, Parms p, int reps );
extern void DoCreateDestroyPhotoflo ( XParms xp, Parms p, int reps );
extern void EndCreateDestroy ( XParms xp, Parms p );
extern void EndCreateDestroyPhotoflo ( XParms xp, Parms p );
extern void FreeCreateDestroyPhotofloStuff ( XParms xp, Parms p );

/* cvttoindex.c */
extern int InitConvertToIndex ( XParms xp, Parms p, int reps );
extern void DoConvertToIndex ( XParms xp, Parms p, int reps );
extern void DoColorAllocEvent ( XParms xp, Parms p, int reps );
extern void EndConvertToIndex ( XParms xp, Parms p );
extern void FreeCvtToIndexStuff ( XParms xp, Parms p );

/* dither.c */
extern int InitDither ( XParms xp, Parms p, int reps );
extern void DoDither ( XParms xp, Parms p, int reps );
extern void EndDither ( XParms xp, Parms p );
extern void FreeDitherStuff ( XParms xp, Parms p );

/* encode.c */
extern int InitEncodePhotomap ( XParms xp, Parms p, int reps );
extern void DoEncodePhotomap ( XParms xp, Parms p, int reps );
extern void DoEncodeClientPhotomap ( XParms xp, Parms p, int reps );
extern void EndEncodePhotomap ( XParms xp, Parms p );
extern void FreeEncodePhotomapStuff ( XParms xp, Parms p );

/* errors.c */
extern int InitErrors ( XParms xp, Parms p, int reps );
extern int InitCoreErrors ( XParms xp, Parms p, int reps );
extern int InitFloAccessError ( XParms xp, Parms p, int reps );
extern int InitBadValue ( XParms xp, Parms p, int reps );
extern int InitFloErrors ( XParms xp, Parms p, int reps );
extern int InitPhotofloError ( XParms xp, Parms p, int reps );
extern int InitFloMatchError ( XParms xp, Parms p, int reps );
extern int InitFloTechniqueError ( XParms xp, Parms p, int reps );
extern int InitFloPhotomapError ( XParms xp, Parms p, int reps );
extern int InitFloSourceError ( XParms xp, Parms p, int reps );
extern int InitFloElementError ( XParms xp, Parms p, int reps );
extern int InitFloDrawableError ( XParms xp, Parms p, int reps );
extern int InitFloColorListError ( XParms xp, Parms p, int reps );
extern int InitFloColormapError ( XParms xp, Parms p, int reps );
extern int InitFloGCError ( XParms xp, Parms p, int reps );
extern int InitFloOperatorError ( XParms xp, Parms p, int reps );
extern int InitFloDomainError ( XParms xp, Parms p, int reps );
extern int InitFloIDError ( XParms xp, Parms p, int reps );
extern int InitFloValueError ( XParms xp, Parms p, int reps );
extern int InitFloROIError ( XParms xp, Parms p, int reps );
extern int InitFloLUTError ( XParms xp, Parms p, int reps );
extern void DoBadValueError ( XParms xp, Parms p, int reps );
extern void DoColorListError ( XParms xp, Parms p, int reps );
extern void DoFloErrorImmediate ( XParms xp, Parms p, int reps );
extern void DoPhotospaceError ( XParms xp, Parms p, int reps );
extern void DoROIError ( XParms xp, Parms p, int reps );
extern void DoFloElementError ( XParms xp, Parms p, int reps );
extern void DoLUTError ( XParms xp, Parms p, int reps );
extern void DoFloIDError ( XParms xp, Parms p, int reps );
extern void DoPhotomapError ( XParms xp, Parms p, int reps );
extern void DoErrors ( XParms xp, Parms p, int reps );
extern void EndErrors ( XParms xp, Parms p );
extern void EndCoreErrors ( XParms xp, Parms p );
extern void EndFloErrors ( XParms xp, Parms p );
extern void FreeErrorWithPhotomapStuff ( XParms xp, Parms p );
extern void FreeErrorWithROIStuff ( XParms xp, Parms p );
extern void FreeErrorWithLUTStuff ( XParms xp, Parms p );
extern void FreeFloErrorStuff ( XParms xp, Parms p );

/* events.c */
extern int GetTimeout ( void );
extern void SetTimeout ( int time );
extern void InitEventInfo ( Display *display, XieExtensionInfo *info );
extern void GetExtensionInfo ( XieExtensionInfo **info );
extern Bool event_check ( Display *display, XEvent *event, char *arg );
extern int WaitForXIEEvent ( XParms xp, int which, XiePhotoflo flo_id, XiePhototag tag, Bool verbose );
extern int InitEvents ( XParms xp, Parms p, int reps );
extern int InitPhotofloDoneEvent ( XParms xp, Parms p, int reps );
extern int InitColorAllocEvent ( XParms xp, Parms p, int reps );
extern int InitDecodeNotifyEvent ( XParms xp, Parms p, int reps );
extern int InitImportObscuredEvent ( XParms xp, Parms p, int reps );
extern int InitExportAvailableEvent ( XParms xp, Parms p, int reps );
extern void DoPhotofloDoneEvent ( XParms xp, Parms p, int reps );
extern void DoDecodeNotifyEvent ( XParms xp, Parms p, int reps );
extern void DoImportObscuredEvent ( XParms xp, Parms p, int reps );
extern void DoExportAvailableEvent ( XParms xp, Parms p, int reps );
extern void EndEvents ( XParms xp, Parms p );
extern void FreePhotofloDoneEventStuff ( XParms xp, Parms p );
extern void FreeImportObscuredEventStuff ( XParms xp, Parms p );
extern void FreeExportAvailableEventStuff ( XParms xp, Parms p );
extern void FreeDecodeNotifyEventStuff ( XParms xp, Parms p );

/* exportcl.c */
extern int InitExportClientPhoto ( XParms xp, Parms p, int reps );
extern int InitExportClientLUT ( XParms xp, Parms p, int reps );
extern int InitExportClientROI ( XParms xp, Parms p, int reps );
extern int InitExportClientHistogram ( XParms xp, Parms p, int reps );
extern void DoExportClientPhotoCSum ( XParms xp, Parms p, int reps );
extern void DoExportClientPhoto ( XParms xp, Parms p, int reps );
extern void DoExportClientHistogram ( XParms xp, Parms p, int reps );
extern void DoExportClientLUT ( XParms xp, Parms p, int reps );
extern void DoExportClientROI ( XParms xp, Parms p, int reps );
extern void EndExportClientLUT ( XParms xp, Parms p );
extern void EndExportClientPhoto ( XParms xp, Parms p );
extern void EndExportClientHistogram ( XParms xp, Parms p );
extern void EndExportClientROI ( XParms xp, Parms p );
extern void FreeExportClientPhotoStuff ( XParms xp, Parms p );
extern void FreeExportClientROIStuff ( XParms xp, Parms p );
extern void FreeExportClientLUTStuff ( XParms xp, Parms p );
extern void FreeExportClientHistogramStuff ( XParms xp, Parms p );

/* funcode.c */
extern int InitFunnyEncode ( XParms xp, Parms p, int reps );
extern void DoFunnyEncode ( XParms xp, Parms p, int reps );
extern void EndFunnyEncode ( XParms xp, Parms p );
extern void FreeFunnyEncodeStuff ( XParms xp, Parms p );

/*  geometry.c */
extern int InitGeometry ( XParms xp, Parms p, int reps );
extern void DoGeometry ( XParms xp, Parms p, int reps );
extern int InitGeometryFAX ( XParms xp, Parms p, int reps );
extern void DoGeometryFAX ( XParms xp, Parms p, int reps );
extern int SetCoefficients ( XParms xp, Parms p, int which, GeometryParms *gp, float coeffs[] );
extern void EndGeometry ( XParms xp, Parms p );
extern void FreeGeometryStuff ( XParms xp, Parms p );
extern void EndGeometryFAX ( XParms xp, Parms p );
extern void FreeGeometryFAXStuff ( XParms xp, Parms p );

/* getnext.c */
extern int GetNextTest ( FILE *fp, int *repeat, int *reps );

/* import.c */
extern int InitImportDrawablePixmap ( XParms xp, Parms p, int reps );
extern int InitImportDrawableWindow ( XParms xp, Parms p, int reps );
extern int InitImportDrawablePlanePixmap ( XParms xp, Parms p, int reps );
extern int InitImportDrawablePlaneWindow ( XParms xp, Parms p, int reps );
extern int InitImportPhoto ( XParms xp, Parms p, int reps );
extern int InitImportPhotoExportDrawable ( XParms xp, Parms p, int reps );
extern int InitImportLUT ( XParms xp, Parms p, int reps );
extern int InitImportROI ( XParms xp, Parms p, int reps );
extern void DoImportPhoto ( XParms xp, Parms p, int reps );
extern void DoImportDrawablePixmap ( XParms xp, Parms p, int reps );
extern void DoImportDrawableWindow ( XParms xp, Parms p, int reps );
extern void DoImportDrawablePlanePixmap ( XParms xp, Parms p, int reps );
extern void DoImportDrawablePlaneWindow ( XParms xp, Parms p, int reps );
extern void DoImportLUT ( XParms xp, Parms p, int reps );
extern void DoImportROI ( XParms xp, Parms p, int reps );
extern void EndImportLUT ( XParms xp, Parms p );
extern void EndImportPhoto ( XParms xp, Parms p );
extern void EndImportROI ( XParms xp, Parms p );
extern void EndImportDrawableWindow ( XParms xp, Parms p );
extern void EndImportDrawablePixmap ( XParms xp, Parms p );
extern void FreeImportPhotoStuff ( XParms xp, Parms p );
extern void FreeImportDrawableWindowStuff ( XParms xp, Parms p );
extern void FreeImportDrawablePixmapStuff ( XParms xp, Parms p );
extern void FreeImportLUTStuff ( XParms xp, Parms p );
extern void FreeImportROIStuff ( XParms xp, Parms p );

/* importctl.c */
extern int InitImportClientPhoto ( XParms xp, Parms p, int reps );
extern int InitImportClientPhotoExportDrawable ( XParms xp, Parms p, 
						 int reps );
extern int InitImportClientLUT ( XParms xp, Parms p, int reps );
extern int InitImportClientROI ( XParms xp, Parms p, int reps );
extern void DoImportClientPhoto ( XParms xp, Parms p, int reps );
extern void DoImportClientLUT ( XParms xp, Parms p, int reps );
extern void DoImportClientROI ( XParms xp, Parms p, int reps );
extern void EndImportClientLUT ( XParms xp, Parms p );
extern void FreeImportClientLUTStuff ( XParms xp, Parms p );
extern void EndImportClientPhoto ( XParms xp, Parms p );
extern void FreeImportClientPhotoStuff ( XParms xp, Parms p );
extern void EndImportClientROI ( XParms xp, Parms p );
extern void FreeImportClientROIStuff ( XParms xp, Parms p );

/* logical.c */
extern int InitLogical ( XParms xp, Parms p, int reps );
extern void DoLogical ( XParms xp, Parms p, int reps );
extern void EndLogical ( XParms xp, Parms p );
extern void FreeLogicalStuff ( XParms xp, Parms p );

/* math.c */
extern int InitMath ( XParms xp, Parms p, int reps );
extern void DoMath ( XParms xp, Parms p, int reps );
extern void EndMath ( XParms xp, Parms p );
extern void FreeMathStuff ( XParms xp, Parms p );

/* modify.c */
extern int InitModifyROI ( XParms xp, Parms p, int reps );
extern void DoModifyROI ( XParms xp, Parms p, int reps );
extern void EndModifyROI ( XParms xp, Parms p );
extern void FreeModifyROIStuff ( XParms xp, Parms p );
extern int InitModifyPoint ( XParms xp, Parms p, int reps );
extern void DoModifyPoint ( XParms xp, Parms p, int reps );
extern void EndModifyPoint ( XParms xp, Parms p );
extern void FreeModifyPointStuff ( XParms xp, Parms p );
extern int InitModifySimple ( XParms xp, Parms p, int reps );
extern void DoModifySimple ( XParms xp, Parms p, int reps );
extern void EndModifySimple ( XParms xp, Parms p );
extern void FreeModifySimpleStuff ( XParms xp, Parms p );
extern int InitModifyLong1 ( XParms xp, Parms p, int reps );
extern void DoModifyLong1 ( XParms xp, Parms p, int reps );
extern void EndModifyLong ( XParms xp, Parms p );
extern void FreeModifyLongStuff ( XParms xp, Parms p );
extern int InitModifyLong2 ( XParms xp, Parms p, int reps );
extern void DoModifyLong2 ( XParms xp, Parms p, int reps );

/* mtchhist.c */
extern int InitMatchHistogram ( XParms xp, Parms p, int reps );
extern void DoMatchHistogram ( XParms xp, Parms p, int reps );
extern void EndMatchHistogram ( XParms xp, Parms p );
extern void FreeMatchHistogramStuff ( XParms xp, Parms p );

/* pasteup.c */
extern int InitPasteUp ( XParms xp, Parms p, int reps );
extern void DoPasteUp ( XParms xp, Parms p, int reps );
extern void EndPasteUp ( XParms xp, Parms p );
extern void FreePasteUpStuff ( XParms xp, Parms p );

/* point.c */
extern int InitPoint ( XParms xp, Parms p, int reps );
extern void DoPoint ( XParms xp, Parms p, int reps );
extern void EndPoint ( XParms xp, Parms p );
extern void FreePointStuff ( XParms xp, Parms p );
extern int InitTriplePoint ( XParms xp, Parms p, int reps );
extern void DoTriplePoint ( XParms xp, Parms p, int reps );
extern void EndTriplePoint ( XParms xp, Parms p );
extern void FreeTriplePointStuff ( XParms xp, Parms p );

/* purgecolst.c */
extern int InitPurgeColorList ( XParms xp, Parms p, int reps );
extern void DoPurgeColorList ( XParms xp, Parms p, int reps );
extern void EndPurgeColorList ( XParms xp, Parms p );

/* query.c */
extern int InitQueryTechniques ( XParms xp, Parms p, int reps );
extern int InitQueryColorList ( XParms xp, Parms p, int reps );
extern int InitQueryPhotomap ( XParms xp, Parms p, int reps );
extern int InitQueryPhotoflo ( XParms xp, Parms p, int reps );
extern void DoQueryTechniques ( XParms xp, Parms p, int reps );
extern void DoQueryColorList ( XParms xp, Parms p, int reps );
extern void DoQueryPhotomap ( XParms xp, Parms p, int reps );
extern void DoQueryPhotoflo ( XParms xp, Parms p, int reps );
extern void EndQueryTechniques ( XParms xp, Parms p );
extern void EndQueryColorList ( XParms xp, Parms p );
extern void FreeQueryColorListStuff ( XParms xp, Parms p );
extern void EndQueryPhotomap ( XParms xp, Parms p );
extern void FreeQueryPhotomapStuff ( XParms xp, Parms p );
extern void EndQueryPhotoflo ( XParms xp, Parms p );
extern void FreeQueryPhotofloStuff ( XParms xp, Parms p );

/* redefine.c */
extern int InitRedefine ( XParms xp, Parms p, int reps );
extern void DoRedefine ( XParms xp, Parms p, int reps );
extern void EndRedefine ( XParms xp, Parms p );
extern void FreeRedefineStuff ( XParms xp, Parms p );

/* rgb.c */
extern int InitRGB ( XParms xp, Parms p, int reps );
extern void DoRGB ( XParms xp, Parms p, int reps );
extern void EndRGB ( XParms xp, Parms p );
extern void FreeRGBStuff ( XParms xp, Parms p );

/* tests.c */
extern XIEimage * GetImageStruct ( int which );
extern void ReclaimPhotomapMemory ( void );

/* uconstrain.c */
extern int InitUnconstrain ( XParms xp, Parms p, int reps );
extern void DoUnconstrain ( XParms xp, Parms p, int reps );
extern void EndUnconstrain ( XParms xp, Parms p );
extern void FreeUnconstrainStuff ( XParms xp, Parms p );

/* xieperf.c */
extern Display * GetDisplay ( void );
extern Display *Open_Display ( char *display_name );
extern SIGNAL_T Cleanup ( int sig );
extern void NullProc ( XParms xp, Parms p );
extern Bool NullInitProc ( XParms xp, Parms p, int reps );
extern void InstallThisColormap ( XParms xp, Colormap newcmap );
extern void InstallDefaultColormap ( XParms xp );
extern void InstallGrayColormap ( XParms xp );
extern void InstallEmptyColormap ( XParms xp );
extern void InstallColorColormap ( XParms xp );
extern int TestIndex ( char *testname );
extern void PumpTheClientData ( XParms xp, Parms p, int flo_id, 
			       XiePhotospace photospace, int element, 
			       char *data, int size, int band_number );
extern int ReadNotifyExportData ( XParms xp, Parms p, unsigned long namespace, 
				  int flo_id, XiePhototag element, 
				  unsigned int elementsz, unsigned int numels, 
				  char **data, int *done );
extern int ReadNotifyExportTripleData ( XParms xp, Parms p, 
					unsigned long namespace, int flo_id, 
					XiePhototag element, 
					unsigned int elementsz, 
					unsigned int numels, char **data, 
					int *done );
extern unsigned int CheckSum ( char *data, unsigned int size );
extern XiePhotomap GetXIEFAXPhotomap ( XParms xp, Parms p, int which, 
				       Bool radiometric );
extern XiePhotomap GetXIETriplePhotomap ( XParms xp, Parms p, int which );
extern XiePhotomap GetXIEPhotomap ( XParms xp, Parms p, int which );
extern XiePhotomap GetXIEPointPhotomap ( XParms xp, Parms p, int which, 
					 int inlevels, Bool useLevels );
extern XiePhotomap GetXIEGeometryPhotomap ( XParms xp, Parms p, 
					    GeometryParms *geo, int which );
extern int GetXIEGeometryWindow ( XParms xp, Parms p, Window w, 
				  GeometryParms *geo, int which );
extern int GetXIEPixmap ( XParms xp, Parms p, Pixmap pixmap, int which );
extern int GetXIEWindow ( XParms xp, Parms p, Window window, int which );
extern int GetXIEDitheredWindow ( XParms xp, Parms p, Window window, 
				  int which, int level );
extern XiePhotomap GetXIEDitheredPhotomap ( XParms xp, Parms p, int which, 
					    int level );
extern XiePhotomap GetXIEDitheredTriplePhotomap ( XParms xp, Parms p, int 
						  which, int ditherTech, 
						  int threshold, 
						  XieLTriplet levels );
extern XiePhotomap GetXIEConstrainedPhotomap ( XParms xp, Parms p, int which, 
					       XieLTriplet cliplevels, 
					       int cliptype, 
					       XieConstant in_low, 
					       XieConstant in_high, 
					       XieLTriplet out_low, 
					       XieLTriplet out_high );
extern XiePhotomap GetXIEConstrainedTriplePhotomap ( XParms xp, Parms p, 
						     int which, 
						     XieLTriplet cliplevels, 
						     int cliptype, 
						     XieConstant in_low, 
						     XieConstant in_high, 
						     XieLTriplet out_low, 
						     XieLTriplet out_high );
extern XiePhotomap GetXIEConstrainedGeometryTriplePhotomap ( XParms xp, 
							     Parms p, 
							     int which, 
							     XieLTriplet cliplevels, 
							     int cliptype, 
							     XieConstant in_low, 
							     XieConstant in_high, 
							     XieLTriplet out_low, 
							     XieLTriplet out_high, 
							     GeometryParms *geo );
extern XiePhotomap GetXIEGeometryTriplePhotomap ( XParms xp, Parms p, 
						  int which, 
						  GeometryParms *geo );
extern int GetXIEDitheredTripleWindow ( XParms xp, Parms p, Window w, 
					int which, int ditherTech, 
					int threshold, XieLTriplet levels );
extern int GetXIEDitheredStdTripleWindow ( XParms xp, Parms p, Window w, 
					   int which, int ditherTech, 
					   int threshold, XieLTriplet levels, 
					   XStandardColormap *stdCmap );
extern int GetFileSize ( char *path );
extern int GetImageData ( XParms xp, Parms p, int which );
extern XieLut GetXIELut ( XParms xp, Parms p, unsigned char *lut, 
			  int lutSize, int lutLevels );
extern XieRoi GetXIERoi ( XParms xp, Parms p, XieRectangle *rects, 
			  int rectsSize );
extern int IsDISServer ( void );
extern Bool TechniqueSupported ( XParms xp, XieTechniqueGroup group, 
				 unsigned int tech );
extern void DrawHistogram ( XParms xp, Window w, XieHistogramData histos[], 
			    int size, unsigned long levels );
extern int GetStandardColormap ( XParms xp, XStandardColormap *stdColormap, 
				 Atom atom );
extern int DepthFromLevels ( int levels );
extern XieLut CreatePointLut ( XParms xp, Parms p, int inlevels, 
			       int outlevels, Bool computeLutFromLevels );
extern int TrueOrDirectLevels ( XParms xp );
extern int TripleTrueOrDirectLevels ( XParms xp );
extern int CreateStandardColormap ( XParms xp, XStandardColormap *stdCmap, 
				    int atom );
extern XiePhotomap GetControlPlane ( XParms xp, int which );
extern int icbrt ( int a );

/*****************************************************************************

For repeatable results, XIEperf should be run using a local connection on a
freshly-started server.  The default configuration runs each test 5 times, in
order to see if each trial takes approximately the same amount of time.
Strange glitches should be examined; if non-repeatable I chalk them up to
daemons and network traffic.  Each trial is run for 5 seconds, in order to
reduce random time differences.  The number of objects processed per second is
displayed to 3 significant digits, but you'll be lucky on most UNIX system if
the numbers are actually consistent to 2 digits.

******************************************************************************/
