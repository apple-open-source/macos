/* $Xorg: globals.h,v 1.4 2001/02/09 02:03:41 xorgcvs Exp $ */

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
/* $XFree86: xc/lib/XIE/globals.h,v 1.6 2002/05/31 18:45:43 dawes Exp $ */

extern void _XieElemImportClientLUT		(char **, XiePhotoElement *);
extern void _XieElemImportClientPhoto		(char **, XiePhotoElement *);
extern void _XieElemImportClientROI		(char **, XiePhotoElement *);
extern void _XieElemImportDrawable		(char **, XiePhotoElement *);
extern void _XieElemImportDrawablePlane		(char **, XiePhotoElement *);
extern void _XieElemImportLUT			(char **, XiePhotoElement *);
extern void _XieElemImportPhotomap		(char **, XiePhotoElement *);
extern void _XieElemImportROI			(char **, XiePhotoElement *);
extern void _XieElemArithmetic			(char **, XiePhotoElement *);
extern void _XieElemBandCombine			(char **, XiePhotoElement *);
extern void _XieElemBandExtract			(char **, XiePhotoElement *);
extern void _XieElemBandSelect			(char **, XiePhotoElement *);
extern void _XieElemBlend			(char **, XiePhotoElement *);
extern void _XieElemCompare			(char **, XiePhotoElement *);
extern void _XieElemConstrain			(char **, XiePhotoElement *);
extern void _XieElemConvertFromIndex		(char **, XiePhotoElement *);
extern void _XieElemConvertFromRGB		(char **, XiePhotoElement *);
extern void _XieElemConvertToIndex		(char **, XiePhotoElement *);
extern void _XieElemConvertToRGB		(char **, XiePhotoElement *);
extern void _XieElemConvolve			(char **, XiePhotoElement *);
extern void _XieElemDither			(char **, XiePhotoElement *);
extern void _XieElemGeometry			(char **, XiePhotoElement *);
extern void _XieElemLogical			(char **, XiePhotoElement *);
extern void _XieElemMatchHistogram		(char **, XiePhotoElement *);
extern void _XieElemMath			(char **, XiePhotoElement *);
extern void _XieElemPasteUp			(char **, XiePhotoElement *);
extern void _XieElemPoint			(char **, XiePhotoElement *);
extern void _XieElemUnconstrain			(char **, XiePhotoElement *);
extern void _XieElemExportClientHistogram	(char **, XiePhotoElement *);
extern void _XieElemExportClientLUT		(char **, XiePhotoElement *);
extern void _XieElemExportClientPhoto		(char **, XiePhotoElement *);
extern void _XieElemExportClientROI		(char **, XiePhotoElement *);
extern void _XieElemExportDrawable		(char **, XiePhotoElement *);
extern void _XieElemExportDrawablePlane		(char **, XiePhotoElement *);
extern void _XieElemExportLUT			(char **, XiePhotoElement *);
extern void _XieElemExportPhotomap		(char **, XiePhotoElement *);
extern void _XieElemExportROI			(char **, XiePhotoElement *);

#ifdef NEED_XIE_GLOBALS

XieExtInfo *_XieExtInfoHeader = NULL;

void (*(_XieElemFuncs[]))(char **, XiePhotoElement *) =
{
    _XieElemImportClientLUT,
    _XieElemImportClientPhoto,
    _XieElemImportClientROI,
    _XieElemImportDrawable,
    _XieElemImportDrawablePlane,
    _XieElemImportLUT,
    _XieElemImportPhotomap,
    _XieElemImportROI,
    _XieElemArithmetic,
    _XieElemBandCombine,
    _XieElemBandExtract,
    _XieElemBandSelect,
    _XieElemBlend,
    _XieElemCompare,
    _XieElemConstrain,
    _XieElemConvertFromIndex,
    _XieElemConvertFromRGB,
    _XieElemConvertToIndex,
    _XieElemConvertToRGB,
    _XieElemConvolve,
    _XieElemDither,
    _XieElemGeometry,
    _XieElemLogical,
    _XieElemMatchHistogram,
    _XieElemMath,
    _XieElemPasteUp,
    _XieElemPoint,
    _XieElemUnconstrain,
    _XieElemExportClientHistogram,
    _XieElemExportClientLUT,
    _XieElemExportClientPhoto,
    _XieElemExportClientROI,
    _XieElemExportDrawable,
    _XieElemExportDrawablePlane,
    _XieElemExportLUT,
    _XieElemExportPhotomap,
    _XieElemExportROI
};


#ifndef __UNIXOS2__
XieTechFuncRec *_XieTechFuncs[xieValMaxTechGroup];
#else
XieTechFuncRec *_XieTechFuncs[xieValMaxTechGroup] = {0};
#endif

Bool _XieTechFuncsInitialized = 0;

#endif /* NEED_XIE_GLOBALS */
