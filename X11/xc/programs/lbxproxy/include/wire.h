/* $Xorg: wire.h,v 1.4 2001/02/09 02:05:32 xorgcvs Exp $ */
/*

Copyright 1998  The Open Group

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

*/
/*
 * Copyright 1992 Network Computing Devices
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of NCD. not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  NCD. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * NCD. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL NCD.
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */
/* $XFree86: xc/programs/lbxproxy/include/wire.h,v 1.7 2001/12/14 20:00:56 dawes Exp $ */

#ifdef DEBUG 		/* Need this here because lbx.h undef's DEBUG */
extern int lbxDebug;
#endif

#include "misc.h"
#include "lbx.h"
#include "lbxdeltastr.h"
#include "lbximage.h"
#include "proxyopts.h"
#include "colormap.h"
#include "atomcache.h"
#include "cache.h"
#include "resource.h"

typedef struct _extinfo *ExtensionInfoPtr;

/*
 * NOTES on the implementation of the multi-display lbxproxy.
 *
 * o Each display has its own listen port(s).
 *
 * o Depending on how the proxy is compiled, it may support multiple
 *   transports thus more than one fd may be associated with a display.
 *
 * o When a new client connects, the proxy must be able to determine
 *   which display the client is interested in.  To facilitate this, when
 *   a display's listen port(s) are created, a list of the display's
 *   fd's is cached in the field listen_fds.
 *
 * o When accept is called, the listen_fds in the servers array is 
 *   searched to find the client's display.
 */

/*
 * MAXTRANSPORTS is the maximum number of transports or listen
 * ports that a proxy can support.  The number is determine at compile
 * by the xtrans library but there is no public interface to this
 * number.  See lib/xtrans/Xtrans.c for more information.
 */
#define MAXTRANSPORTS	13

/*
 * The maximum number of options that is negotiable.
 */
#define MAX_NEG_OPTS	8

typedef struct _LbxOptionsListRec {
    CARD8       optcode;
    Bool        negotiate;
    int         (*req)();
    int         (*reply)();
} LbxOptionsListRec;

typedef struct _LbxDeltaOptionsRec {
    CARD8       minN;                   /* min cache size */
    CARD8       maxN;                   /* max cache size */
    CARD8       prefN;                  /* preferred cache size */
    CARD8       minMaxMsgLen;           /* min max-message-len / 4 */
    CARD8       maxMaxMsgLen;           /* max max-message-len / 4 */
    CARD8       prefMaxMsgLen;          /* preferred max-message-len / 4 */
} LbxDeltaOptionsRec;

typedef struct _LbxStreamComp {
    int         typelen;
    char        *type;
    int         (*req)();
    int         (*reply)();
} LbxStreamCompRec;

typedef struct _LbxCmapAllMethod {
    char *methodName;
    void (*resolver)(
#if NeedFunctionPrototypes
        LbxVisualPtr /* pVisual */,
        CARD16* /* red */,
        CARD16* /* green */,
        CARD16* /* blue */
#endif
    );
    Pixel (*findfree)(
#if NeedFunctionPrototypes
        ColormapPtr /* pmap */,
        CARD32  /* red */,
        CARD32  /* green */,
        CARD32  /* blue */
#endif
    );
    Entry * (* findbest)(
#if NeedFunctionPrototypes
        ColormapPtr /* pmap */,
        CARD32  /* red */,
        CARD32  /* green */,
        CARD32  /* blue */,
        int     /* channels */
#endif
        );

} LbxCmapAllMethod;

typedef struct _XServer {
    int			index;
    int			fd;
    int			lbxReq;
    int			lbxEvent;
    int			lbxError;
    Bool		initialized;
    char		*display_name;  /* The name as requested by the client*/
    char		*proxy_name;    /* The actual name used by the proxy */
    int			listen_fds[MAXTRANSPORTS];
    ClientPtr		prev_exec;
    ClientPtr		send, recv;
    ClientPtr		serverClient;
    int			recv_expect;
    int			motion_allowed;
    Bool		wm_running;
    pointer		compHandle;
    lbxMotionCache	motionCache;
    ExtensionInfoPtr	extensions;
    LBXDeltasRec	indeltas;
    LBXDeltasRec	outdeltas;

    /* 
     * The following fields are needed to handle the option
     * negotiation for a multi-display proxy.
     */
    LbxOptionsListRec	LbxOptions[MAX_NEG_OPTS]; 
    int			optcount;    /* Number of options actually negotiated */
    int			optlist[MAX_NEG_OPTS];
    LbxNegOptsRec       lbxNegOpt;
    LbxDeltaOptionsRec	proxyDeltaOpt;
    LbxDeltaOptionsRec	serverDeltaOpt;
    LbxStreamCompRec    LbxStreamComp[1];
    LbxBitmapCompMethod LbxBitmapCompMethods[1];
    LbxPixmapCompMethod LbxPixmapCompMethods[1];
    LbxCmapAllMethod	LbxCmapAllMethods[1]; 

    /*
     * requestVector is the function table to be used by the
     * "real" clients associated with this server.
     */
    int			(**requestVector) (
#if NeedNestedPrototypes
	ClientPtr /* pClient */
#endif
    );

    /*
     * The following fields are needed to handle the atom
     * cache for a multi-display proxy.
     */
    AtomControlPtr	atom_control;
    int			atom_control_count;
    AtomListPtr		*hashTable;
    int			hashSize;
    int			hashUsed;
    int			hashMask;
    int			rehash;
    AtomListPtr		*reverseMap;
    int			reverseMapSize;
    Atom		lastAtom;

    /*
     * The following fields are needed for each server's
     * global and property caches.
     */
    Cache		global_cache;
    Cache		prop_cache;
    CachePtr		caches[MAX_NUM_CACHES + 1];
    int			num_caches;
    unsigned long	seed;

    /*
     * The following fields are needed for each server's
     * colormap caches.
     */
    RGBCacheEntryPtr	rgb_cache[NBUCKETS];
    RGBEntryRec 	rgb_buf;
    Entry		rgb_ent;

    /*
     * Server-specific temporary delta buffer
     */
    unsigned char 	tempdeltabuf[256];

    /*
     * The following fields are used to mange a server's resources
     */
    ClientResourceRec	clientTable[MAXCLIENTS];
    ClientPtr		lastLbxClientIndexLookup;

    /*
     * The following fields maintain a server's state for grab's
     */
    int			lbxIgnoringAll;
    int			lbxGrabInProgress;

    struct _XDisplay*	dpy;

} XServerRec;


/*
 * External declarations for global variables defined in main.c
 */
extern XServerPtr	* servers;
extern char 		* display;
extern int 		lbxMaxServers;

extern void WriteReqToServer(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    int /*len*/,
    char * /*buf*/,
    Bool /*checkLargeRequest*/
#endif
);

extern void _write_to_server(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    Bool /*compressed*/,
    int /*len*/,
    char* /*buf*/,
    Bool /*checkLarge*/,
    Bool /*startOfRequest*/
#endif
);

extern void WriteToServer(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    int /*len*/,
    char * /*buf*/,
    Bool /*startOfRequest*/,
    Bool /*checkLargeRequest*/
#endif
);

extern void WriteToServerUncompressed(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    int /*len*/,
    char * /*buf*/,
    Bool /* startOfRequest */
#endif
);

extern Bool NewClient(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    int /*setuplen*/
#endif
);

extern void CloseClient(
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);

extern void ModifySequence(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    int /*num*/
#endif
);

extern void AllowMotion(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    int /*num*/
#endif
);

extern void SendIncrementPixel(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    XID /*cmap*/,
    unsigned long /*pixel*/
#endif
);

extern void SendAllocColor(
#if NeedFunctionPrototypes
    ClientPtr /* pClient */,
    XID       /* cmap */,
    CARD32    /* pixel */,
    CARD16    /* red */,
    CARD16    /* green */,
    CARD16    /* blue */
#endif
);

extern void SendGetModifierMapping(
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);

extern void SendGetKeyboardMapping(
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);

extern void SendQueryFont(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    XID /*fid*/
#endif
);

extern void SendChangeProperty(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    Window /*win*/,
    Atom /*prop*/,
    Atom /*type*/,
    int /*format*/,
    int /*mode*/,
    unsigned long /*num*/
#endif
);

extern void SendGetProperty(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    Window /*win*/,
    Atom /*prop*/,
    Atom /*type*/,
    Bool /*delete*/,
    unsigned long /*off*/,
    unsigned long /*len*/
#endif
);

extern void SendInvalidateTag(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    XID /*tag*/
#endif
);

extern void SendTagData(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    XID /*tag*/,
    unsigned long /*len*/,
    pointer /*data*/
#endif
);

extern void SendGetImage(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    Drawable /*drawable*/,
    int /*x*/,
    int /*y*/,
    unsigned int /*width*/,
    unsigned int /*height*/,
    unsigned long /*planeMask*/,
    int /*format*/
#endif
);

extern int ServerProcStandardEvent(
#if NeedFunctionPrototypes
    ClientPtr /*sc*/
#endif
);

extern void CloseServer(
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);

extern Bool ConnectToServer(
#if NeedFunctionPrototypes
    char * /*dpy_name*/
#endif
);
