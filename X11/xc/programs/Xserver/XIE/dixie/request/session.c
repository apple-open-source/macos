/* $Xorg: session.c,v 1.4 2001/02/09 02:04:22 xorgcvs Exp $ */
/**** session.c ****/
/****************************************************************************

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

	session.c: Initialization code for the XIE server. 

	Dean Verheiden -- AGE Logic, Inc  March, 1993

*****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/dixie/request/session.c,v 3.14 2001/12/14 19:58:10 dawes Exp $ */

#define _XIEC_SESSION

#define NEED_EVENTS
#define NEED_REPLIES
#include "X.h"			/* Needed for just about anything	*/
#include "Xproto.h"		/* defines protocol-related stuff	*/
#include "misc.h"		/* includes os.h, which type FatalError	*/
#include "dixstruct.h" 		/* this picks up ClientPtr definition	*/
#include "extnsionst.h"		/* defines things like ExtensionEntry	*/

#include "XIE.h"		
#include "XIEproto.h"		/* Xie protocol specification		*/
#include <corex.h>		/* core X interface routine definitions */ 
#include <tables.h>
#include <macro.h>		/* server internal macros		*/

#include <memory.h>
#include <technq.h>		/* extern def for technique_init	*/

/* function declarations */
extern	int	SProcQueryImageExtension(ClientPtr client);
extern	int	ProcQueryImageExtension(ClientPtr client);

static	int	XieDispatch(ClientPtr client),
		SXieDispatch(ClientPtr client),
		DeleteXieClient(pointer data, XID id);
static void	XieReset (ExtensionEntry *extEntry);
static int	DdxInit(void);

#define REPORT_MEMORY_LEAKS
#ifdef  REPORT_MEMORY_LEAKS
extern int ALLOCS;
extern int STRIPS;
extern int BYTES;
#endif

ExtensionEntry	*extEntry;
RESTYPE		RC_XIE;			/* XIE Resource Class		*/
#if XIE_FULL
RESTYPE		RT_COLORLIST;		/* ColorList resource type	*/
#endif
RESTYPE		RT_LUT;			/* Lookup table resource type	*/
RESTYPE		RT_PHOTOFLO;		/* Photoflo   resource type	*/
RESTYPE		RT_PHOTOMAP;		/* Photomap   resource type	*/
RESTYPE		RT_PHOTOSPACE;		/* Photospace resource type	*/
#if XIE_FULL
RESTYPE		RT_ROI;			/* Region Of Interest type	*/
#endif
RESTYPE		RT_XIE_CLIENT;		/* XIE Type for Shutdown Notice */

struct _client_table {
  XID	   Shutdown_id;
  int 	(**proc_table)(ClientPtr);	/* Table of version specific procedures */
  int 	(**sproc_table)(ClientPtr);	
  CARD16   minorVersion;
  CARD16   pad;		
} client_table[MAXCLIENTS];

void XieInit(void)
{
  ExtensionEntry *AddExtension();
  
  /* Initialize XIE Resources */
  RC_XIE	= CreateNewResourceClass();
#if XIE_FULL
  RT_COLORLIST	= RC_XIE | CreateNewResourceType((DeleteType)DeleteColorList);
#endif
  RT_LUT	= RC_XIE | CreateNewResourceType((DeleteType)DeleteLUT);
  RT_PHOTOFLO	= RC_XIE | CreateNewResourceType((DeleteType)DeletePhotoflo);
  RT_PHOTOMAP	= RC_XIE | CreateNewResourceType((DeleteType)DeletePhotomap);
  RT_PHOTOSPACE	= RC_XIE | CreateNewResourceType((DeleteType)DeletePhotospace);
#if XIE_FULL
  RT_ROI	= RC_XIE | CreateNewResourceType((DeleteType)DeleteROI);
#endif
  RT_XIE_CLIENT	= RC_XIE | CreateNewResourceType((DeleteType)DeleteXieClient);
  
  
  extEntry = AddExtension(xieExtName, 		/* extension name   */
			  xieNumEvents, 	/* number of events */
			  xieNumErrors,		/* number of errors */
			  XieDispatch,		/* Xie's dispatcher */
			  SXieDispatch,		/* Swapped dispatch */
			  XieReset, 		/* Reset XIE stuff  */
			  StandardMinorOpcode	/* choose opcode dynamically */
			  );
  
  if (extEntry == NULL)
    FatalError(" could not add Xie as an extension\n");
  
  /* Initialize client table */
  bzero((char *)client_table, sizeof(client_table));

  if (!technique_init() || DdxInit() != Success)
    FatalError(" could not add Xie as an extension\n");

}

/**********************************************************************/
/* Register client with core X under a FakeClientID */
static int RegisterXieClient(ClientPtr client, CARD16 minor)
{
    client_table[client->index].Shutdown_id  = FakeClientID(client->index);
    client_table[client->index].minorVersion = minor;
    
    init_proc_tables(minor,
		     &(client_table[client->index].proc_table),
		     &(client_table[client->index].sproc_table));
    
    /* Register the client with Core X for shutdown */
    return (AddResource(client_table[client->index].Shutdown_id,
		    RT_XIE_CLIENT,
		    &(client_table[client->index])));
}

/**********************************************************************/
/* dispatcher for XIE opcodes */
static int XieDispatch (ClientPtr client)
{
  REQUEST(xieReq); 	/* make "stuff" point to client's request buffer */
  
  /* QueryImageExtension establishes a communication version */
  if (stuff->opcode == X_ieQueryImageExtension)
    return (ProcQueryImageExtension(client));		

  /* First make sure that a communication version has been established  */
  if (client_table[client->index].Shutdown_id == 0 && /* Pick a favorite */
      !RegisterXieClient(client, xieMinorVersion))    /* minor version   */
	return ( BadAlloc );
  if (stuff->opcode > 0 && stuff->opcode <= xieNumProtoReq) 
    /* Index into version specific routines */
    return (CallProc(client));
  else 
    return ( BadRequest);
}


/**********************************************************************/
/* dispatcher for swapped code */
static int SXieDispatch (ClientPtr client)
{
  REQUEST(xieReq);	/* make "stuff" point to client's request buffer */
  
  /* QueryImageExtension establishes a communication version */
  if (stuff->opcode == X_ieQueryImageExtension)
    return (SProcQueryImageExtension(client));		

  /* First make sure that a communication version has been established  */
  if (client_table[client->index].Shutdown_id == 0 && /* Pick a favorite */
      !RegisterXieClient(client, xieMinorVersion))    /* minor version   */
	return ( BadAlloc );
  if (stuff->opcode > 0 && stuff->opcode <= xieNumProtoReq) 
    /* Index into version specific routines */
    return (CallSProc(client));

  else 
    return ( BadRequest);
}

/**********************************************************************/
int ProcQueryImageExtension(ClientPtr client)
{
  xieQueryImageExtensionReply reply;
  XID FakeClientID();
  REQUEST(xieQueryImageExtensionReq);
  REQUEST_SIZE_MATCH( xieQueryImageExtensionReq );
  
  reply.type = X_Reply;
  reply.sequenceNum = client->sequence;
  
  reply.majorVersion = xieMajorVersion;
  
  if (stuff->majorVersion != xieMajorVersion || 
#if xieEarliestMinorVersion > 0
      stuff->minorVersion < xieEarliestMinorVersion ||
#endif
      stuff->minorVersion > xieLatestMinorVersion) 
    reply.minorVersion = xieMinorVersion;
  else 
    reply.minorVersion = stuff->minorVersion;

  reply.length = sizeof(Preferred_levels)>>2;
  
#if XIE_FULL
  reply.serviceClass          = xieValFull;
#else
  reply.serviceClass          = xieValDIS;
#endif
  reply.alignment             = ALIGNMENT;
  reply.unconstrainedMantissa = UNCONSTRAINED_MANTISSA;
  reply.unconstrainedMaxExp   = UNCONSTRAINED_MAX_EXPONENT;
  reply.unconstrainedMinExp   = UNCONSTRAINED_MIN_EXPONENT;
  
  /* 
    If this is the first QueryImageExtension for this client, register fake_id 
    with Core X to get a closedown notification later 
  */
  if (client_table[client->index].Shutdown_id == 0) 
  	if (!RegisterXieClient(client, reply.minorVersion)) 
        	return ( BadAlloc );
  
  /***	Take care of swapping bytes if necessary	***/
  if (client->swapped) {
    register int n;
    
    swaps(&reply.sequenceNum,n);
    swapl(&reply.length,n);
    swaps(&reply.majorVersion,n);
    swaps(&reply.minorVersion,n);
    swaps(&reply.unconstrainedMantissa,n);
    swapl(&reply.unconstrainedMaxExp,n);
    swapl(&reply.unconstrainedMinExp,n);
  }
  WriteToClient(client,sz_xieQueryImageExtensionReply,(char*)&reply);

  /*
   * Send the list of preferred levels (swapped as necessary)
   */
  if(reply.length)
    if(client->swapped)
      CopySwap32Write(client,sizeof(Preferred_levels),Preferred_levels);
    else
      WriteToClient(client,sizeof(Preferred_levels),(char*)Preferred_levels);
  
  return(Success);
}


/**********************************************************************/
int SProcQueryImageExtension(ClientPtr client)
{
  REQUEST(xieQueryImageExtensionReq);
  register int n;
  swaps(&stuff->length,n);
  swaps(&stuff->majorVersion, n);
  swaps(&stuff->minorVersion, n);
  return( ProcQueryImageExtension(client) );
}

/************************************************************************/

static int DdxInit(void)
{
  return Success;
}

/**********************************************************************/
/* Clean up routine */
static int DeleteXieClient(pointer data, XID id)
{
  bzero((char *)&(client_table[CLIENT_ID(id)]), sizeof(struct _client_table));
  return 0;
}

/**********************************************************************/
/* reset the XIE code, eg, on reboot */
static void XieReset (ExtensionEntry *extEntry)
{
#ifdef REPORT_MEMORY_LEAKS

  /* memory leak debug code
   */
  /* Initialize client table */
  bzero((char *)client_table, sizeof(client_table));
  if(ALLOCS)		/* check on outstanding mallocs and callocs */
    ErrorF("XieReset: %d allocs still outstanding.\n", ALLOCS);

  if(STRIPS || BYTES)	/* check on outstanding data manager allocs */
    ErrorF("XieReset: %d strips with %d data bytes still outstanding.\n",
	   STRIPS, BYTES);
#endif  
}

/**** End of session.c ****/
