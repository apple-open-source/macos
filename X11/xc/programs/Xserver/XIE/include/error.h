/* $Xorg: error.h,v 1.4 2001/02/09 02:04:23 xorgcvs Exp $ */
/**** module error.h ****/
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
*******************************************************************************

	error.h -- error specific definitions

	Robert NC Shelley -- AGE Logic, Inc. April, 1993

******************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/include/error.h,v 3.5 2001/12/14 19:58:13 dawes Exp $ */

#ifndef _XIEH_ERROR
#define _XIEH_ERROR

#include <flo.h>
#include <XIE.h>	
#include <XIEproto.h>		/* declares xieFloEvn */
#include <XIEprotost.h>		/* declares xieTypPhototag */
#include <flostr.h>		/* declares floDefPtr */

extern	int	 SendFloError(ClientPtr client, floDefPtr flo);
extern	int	 SendFloIDError(ClientPtr client, XID spaceID, XID floID);
extern	int	 SendResourceError(ClientPtr client, CARD8 code, XID id);
extern	void	 ErrDomain(floDefPtr flo, peDefPtr ped, xieTypPhototag domain);
extern	void	 ErrGeneric(floDefPtr flo, peDefPtr ped, CARD8 code);
extern	void	 ErrOperator(floDefPtr flo, peDefPtr ped, CARD8 operator);
extern	void	 ErrResource(floDefPtr flo, peDefPtr ped, CARD8 code, CARD32 id);
extern	void	 ErrTechnique(floDefPtr flo, peDefPtr ped, CARD8 group, CARD16 tech, CARD16 lenParams);
extern	void	 ErrValue(floDefPtr flo, peDefPtr ped, CARD32 value);
extern	void	 FloError(floDefPtr flo, xieTypPhototag tag, CARD16 type, CARD8 code);

/*
 *  Convenience macros for dealing with the floDef generic error packet
 */
#define ferrTag(flo)          ((flo)->error.phototag)
#define ferrType(flo)         ((flo)->error.type)
#define ferrCode(flo)         ((flo)->error.floErrorCode)
#define	ferrError(flo,tag,type,code) \
		(ferrTag(flo)=(tag),ferrType(flo)=(type),ferrCode(flo)=(code))

/*
 * convenience macros for general flo errors
 */
#define xieFloError(flo,tag,type,code) \
	FloError(flo,(xieTypPhototag)tag,(CARD16)(long)type,(CARD8)code)

#define	FloAccessError(flo,tag,type,xfer) \
		{xieFloError(flo,tag,type,xieErrNoFloAccess); xfer;}
#define	FloAllocError(flo,tag,type,xfer) \
		{xieFloError(flo,tag,type,xieErrNoFloAlloc); xfer;}
#define	FloElementError(flo,tag,type,xfer) \
		{xieFloError(flo,tag,type,xieErrNoFloElement); xfer;}
#define	FloImplementationError(flo,tag,type,xfer) \
		{xieFloError(flo,tag,type,xieErrNoFloImplementation); xfer;}
#define	FloLengthError(flo,tag,type,xfer) \
		{xieFloError(flo,tag,type,xieErrNoFloLength); xfer;}
#define	FloSourceError(flo,tag,type,xfer) \
		{xieFloError(flo,tag,type,xieErrNoFloSource); xfer;}

/*
 * convenience macros for element-specific flo errors
 */
#define	AccessError(flo,ped,xfer) \
		{ErrGeneric(flo,ped,(CARD8)xieErrNoFloAccess); xfer;}
#define	AllocError(flo,ped,xfer) \
		{ErrGeneric(flo,ped,(CARD8)xieErrNoFloAlloc); xfer;}
#define	ColorListError(flo,ped,id,xfer) \
		{ErrResource(flo,ped,(CARD8)xieErrNoFloColorList,id); xfer;}
#define	ColormapError(flo,ped,id,xfer) \
		{ErrResource(flo,ped,(CARD8)xieErrNoFloColormap,id); xfer;}
#define	DomainError(flo,ped,dom,xfer) \
		{ErrDomain(flo,ped,dom); xfer;}
#define	DrawableError(flo,ped,id,xfer) \
		{ErrResource(flo,ped,(CARD8)xieErrNoFloDrawable,id); xfer;}
#define	ElementError(flo,ped,xfer) \
		{ErrGeneric(flo,ped,(CARD8)xieErrNoFloElement); xfer;}
#define	GCError(flo,ped,id,xfer) \
		{ErrResource(flo,ped,(CARD8)xieErrNoFloGC,id); xfer;}
#define	ImplementationError(flo,ped,xfer) \
		{ErrGeneric(flo,ped,(CARD8)xieErrNoFloImplementation); xfer;}
#define	LUTError(flo,ped,id,xfer) \
		{ErrResource(flo,ped,(CARD8)xieErrNoFloLUT,id); xfer;}
#define	MatchError(flo,ped,xfer) \
		{ErrGeneric(flo,ped,(CARD8)xieErrNoFloMatch); xfer;}
#define	OperatorError(flo,ped,op,xfer) \
		{ErrOperator(flo,ped,(CARD8)op); xfer;}
#define	PhotomapError(flo,ped,id,xfer) \
		{ErrResource(flo,ped,(CARD8)xieErrNoFloPhotomap,id); xfer;}
#define	ROIError(flo,ped,id,xfer) \
		{ErrResource(flo,ped,(CARD8)xieErrNoFloROI,id); xfer;}
#define	SourceError(flo,ped,xfer) \
		{ErrGeneric(flo,ped,(CARD8)xieErrNoFloSource); xfer;}
#define	TechniqueError(flo,ped,group,tech,len,xfer) \
		{ErrTechnique(flo,ped,(CARD8)group,(CARD16)tech,(CARD16)len); \
		   xfer;}
#define	ValueError(flo,ped,value,xfer) \
		{ErrValue(flo,ped,value); xfer;}

#endif /* end _XIEH_ERROR */
