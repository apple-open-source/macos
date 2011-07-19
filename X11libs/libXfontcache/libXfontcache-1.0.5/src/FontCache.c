/*-
 * Copyright (c) 1998-1999 Shunsuke Akiyama <akiyama@jp.FreeBSD.org>.
 * All rights reserved.
 * Copyright (c) 1998-1999 X-TrueType Server Project, All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	Id: FontCache.c,v 1.8 1999/01/31 12:52:49 akiyama Exp $
 */
/* $XFree86: FontCache.c,v 1.3 2002/10/16 00:37:28 dawes Exp $ */

/* THIS IS NOT AN X CONSORTIUM STANDARD */

#define NEED_EVENTS
#define NEED_REPLIES
#include <X11/Xlibint.h>
#include <X11/extensions/fontcachstr.h>
#include <X11/extensions/Xext.h>
#include <X11/extensions/extutil.h>

static XExtensionInfo _fontcache_info_data;
static XExtensionInfo *fontcache_info = &_fontcache_info_data;
static char *fontcache_extension_name = FONTCACHENAME;

#define FontCacheCheckExtension(dpy,i,val) \
  XextCheckExtension (dpy, i, fontcache_extension_name, val)

/*****************************************************************************
 *                                                                           *
 *			   private utility routines                          *
 *                                                                           *
 *****************************************************************************/

static int close_display(Display *, XExtCodes *);

static /* const */ XExtensionHooks fontcache_extension_hooks = {
    NULL,				/* create_gc */
    NULL,				/* copy_gc */
    NULL,				/* flush_gc */
    NULL,				/* free_gc */
    NULL,				/* create_font */
    NULL,				/* free_font */
    close_display,			/* close_display */
    NULL,				/* wire_to_event */
    NULL,				/* event_to_wire */
    NULL,				/* error */
    NULL,				/* error_string */
};

static XEXT_GENERATE_FIND_DISPLAY (find_display, fontcache_info, 
				   fontcache_extension_name, 
				   &fontcache_extension_hooks, 
				   0, NULL)

static XEXT_GENERATE_CLOSE_DISPLAY (close_display, fontcache_info)


/*****************************************************************************
 *                                                                           *
 *		    public Font-Misc Extension routines                      *
 *                                                                           *
 *****************************************************************************/

Bool
FontCacheQueryExtension(Display *dpy,
                        int *event_base_return,
                        int *error_base_return)
{
    XExtDisplayInfo *info = find_display (dpy);

    if (XextHasExtension(info)) {
	*event_base_return = info->codes->first_event;
	*error_base_return = info->codes->first_error;
	return True;
    } else {
	return False;
    }
}

Bool
FontCacheQueryVersion(Display *dpy,
                      int *major_version_return,
                      int *minor_version_return)
{
    XExtDisplayInfo *info = find_display (dpy);
    xFontCacheQueryVersionReply rep;
    xFontCacheQueryVersionReq *req;

    FontCacheCheckExtension (dpy, info, False);

    LockDisplay(dpy);
    GetReq(FontCacheQueryVersion, req);
    req->reqType = info->codes->major_opcode;
    req->fontcacheReqType = X_FontCacheQueryVersion;
    if (!_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
	UnlockDisplay(dpy);
	SyncHandle();
	return False;
    }
    *major_version_return = rep.majorVersion;
    *minor_version_return = rep.minorVersion;
    UnlockDisplay(dpy);
    SyncHandle();
    return True;
}

Bool
FontCacheGetCacheSettings(Display *dpy, FontCacheSettings *cacheinfo)
{
    XExtDisplayInfo *info = find_display (dpy);
    xFontCacheGetCacheSettingsReply rep;
    xFontCacheGetCacheSettingsReq *req;

    FontCacheCheckExtension (dpy, info, False);

    LockDisplay(dpy);
    GetReq(FontCacheGetCacheSettings, req);
    req->reqType = info->codes->major_opcode;
    req->fontcacheReqType = X_FontCacheGetCacheSettings;
    if (!_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
	UnlockDisplay(dpy);
	SyncHandle();
	return False;
    }
    /* XXX */
    cacheinfo->himark = rep.himark;
    cacheinfo->lowmark = rep.lowmark;
    cacheinfo->balance = rep.balance;
    /* XXX */
    UnlockDisplay(dpy);
    SyncHandle();
    return True;
}

Bool
FontCacheChangeCacheSettings(Display *dpy, FontCacheSettings *cacheinfo)
{
    XExtDisplayInfo *info = find_display (dpy);
    xFontCacheChangeCacheSettingsReq *req;

    FontCacheCheckExtension (dpy, info, False);

    LockDisplay(dpy);
    GetReq(FontCacheChangeCacheSettings, req);
    req->reqType = info->codes->major_opcode;
    req->fontcacheReqType = X_FontCacheChangeCacheSettings;
    /* XXX */
    req->himark = cacheinfo->himark;
    req->lowmark = cacheinfo->lowmark;
    req->balance = cacheinfo->balance;
    /* XXX */
    UnlockDisplay(dpy);
    SyncHandle();
    return True;
}

Bool
FontCacheGetCacheStatistics(Display *dpy, FontCacheStatistics *cachestats)
{
    XExtDisplayInfo *info = find_display (dpy);
    xFontCacheGetCacheStatisticsReply rep;
    xFontCacheGetCacheStatisticsReq *req;

    FontCacheCheckExtension (dpy, info, False);

    LockDisplay(dpy);
    GetReq(FontCacheGetCacheStatistics, req);
    req->reqType = info->codes->major_opcode;
    req->fontcacheReqType = X_FontCacheGetCacheStatistics;
    if (!_XReply(dpy, (xReply *)&rep,
		(SIZEOF(xFontCacheGetCacheStatisticsReply)-SIZEOF(xReply))>>2,
		 xFalse)) {
	UnlockDisplay(dpy);
	SyncHandle();
	return False;
    }
    /* XXX */
    cachestats->purge_runs = rep.purge_runs;
    cachestats->purge_stat = rep.purge_stat;
    cachestats->balance = rep.balance;
    cachestats->f.hits = rep.f_hits;
    cachestats->f.misshits = rep.f_misshits;
    cachestats->f.purged = rep.f_purged;
    cachestats->f.usage = rep.f_usage;
    cachestats->v.hits = rep.v_hits;
    cachestats->v.misshits = rep.v_misshits;
    cachestats->v.purged = rep.v_purged;
    cachestats->v.usage = rep.v_usage;
    /* XXX */
    UnlockDisplay(dpy);
    SyncHandle();
    return True;
}
