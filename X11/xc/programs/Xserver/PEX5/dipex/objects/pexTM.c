/* $Xorg: pexTM.c,v 1.4 2001/02/09 02:04:14 xorgcvs Exp $ */
/*

Copyright 1994, 1998  The Open Group

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


#include "X.h"
#include "Xproto.h"
#include "pexError.h"
#include "PEXproto.h"
#include "dipex.h"
#include "pexLookup.h"

#ifdef min
#undef min
#endif
 
#ifdef max
#undef max
#endif



/*++	PEXCreateColorMipMapTM
 --*/
ErrorCode
PEXCreateColorMipMapTM (cntxtPtr, strmPtr)
pexContext              *cntxtPtr;
pexCreateColorMipMapTMReq     *strmPtr;
{
    ErrorCode err = Success;
    ErrorCode FreeTM (), CreateColorMipMapTM ();
    ddPointer   Texel_Array;
    ddTextureStr *ptext = 0;

    CHECK_FP_FORMAT (strmPtr->fpFormat);

    if (!LegalNewID(strmPtr->TMid, cntxtPtr->client))
	PEX_ERR_EXIT(BadIDChoice,strmPtr->TMid,cntxtPtr);

    ptext = (ddTextureStr *) Xalloc ((unsigned long)(sizeof(ddTextureStr)));
    if (!ptext) PEX_ERR_EXIT(BadAlloc,0,cntxtPtr);

    ptext->rendId = strmPtr->TMid;

    Texel_Array = (ddPointer *)(strmPtr + 1);

    err = CreateColorMipMapTM(strmPtr->TMid, strmPtr->TMDimension,
    strmPtr->numLevels, strmPtr->texelType, Texel_Array, ptext);

    if (err) Xfree((pointer)ptext);
    ADDRESOURCE(strmPtr->TMid, PEXTextureType, ptext);

    return( err );

} /* end-PEXCreateColorMipMapTM() */


/*++	PEXCreateColorMipMapTM
 --*/
ErrorCode
PEXCreateColorMipMapfromRes (cntxtPtr, strmPtr)
pexContext              *cntxtPtr;
pexCreateColorMipMapfromResReq     *strmPtr;
{
    ErrorCode err = Success;
    ErrorCode FreeTM (), CreateColorMipMapfromRes ();
    ddPointer   Texel_Array_Counts, resource_IDs;
    ddTextureStr *ptext = 0;

    CHECK_FP_FORMAT (strmPtr->fpFormat);

    if (!LegalNewID(strmPtr->TMid, cntxtPtr->client))
	PEX_ERR_EXIT(BadIDChoice,strmPtr->TMid,cntxtPtr);

    ptext = (ddTextureStr *) Xalloc ((unsigned long)(sizeof(ddTextureStr)));
    if (!ptext) PEX_ERR_EXIT(BadAlloc,0,cntxtPtr);

    ptext->rendId = strmPtr->TMid;

    Texel_Array_Counts = (ddPointer *)(strmPtr + 1);
    if (strmPtr->TMDimension == TMDimension3D)
       /* if dimension is 3D Texel_Array_Counts are each 8 bytes long
          and there are numLevels of them                             */
	resource_IDs = (ddPointer *)(strmPtr + (strmPtr->numLevels*8));
    else 
      /* Texel_Array_Counts are each 4 bytes long, still numLevels of them */
	resource_IDs = (ddPointer *)(strmPtr + (strmPtr->numLevels*4));

    /* leave it to the ddpex layer to decipher the color and alpha 
       resource IDs lists after it processes the Texel_Array_Counts   */
    err = CreateColorMipMapfromRes(strmPtr->TMid, strmPtr->TMDimension,
	  strmPtr->numLevels, strmPtr->texelType, strmPtr->drawable, 
	  Texel_Array_Counts, resource_IDs, ptext);

    if (err) Xfree((pointer)ptext);
    ADDRESOURCE(strmPtr->TMid, PEXTextureType, ptext);

    return( err );

} /* end-PEXCreateColorMipMapfromRes() */

/*++	PEXFreeTM
 --*/
ErrorCode
PEXFreeTM (cntxtPtr, strmPtr)
pexContext              *cntxtPtr;
pexFreeTMReq       *strmPtr;
{
    ErrorCode err = Success;

    LU_NAMESET(strmPtr->id, pns);

    FreeResource(strmPtr->id, RT_NONE);

    return(err);

} /* end-PEXFreeTM() */

/*++
 *
 * 	End of File
 --*/
