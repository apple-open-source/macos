/*
   Copyright (c) 2002  XFree86 Inc
*/
/* $XFree86: xc/lib/XRes/XRes.c,v 1.4 2002/10/16 00:37:26 dawes Exp $ */

#define NEED_EVENTS
#define NEED_REPLIES
#include <stdlib.h>
#include <X11/Xlibint.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xext.h>
#include <X11/extensions/extutil.h>
#include <X11/extensions/XResproto.h>
#include <X11/extensions/XRes.h>


static XExtensionInfo _xres_ext_info_data;
static XExtensionInfo *xres_ext_info = &_xres_ext_info_data;
static char *xres_extension_name = XRES_NAME;

#define XResCheckExtension(dpy,i,val) \
  XextCheckExtension (dpy, i, xres_extension_name, val)

static XEXT_GENERATE_CLOSE_DISPLAY (close_display, xres_ext_info)

static XExtensionHooks xres_extension_hooks = {
    NULL,                               /* create_gc */
    NULL,                               /* copy_gc */
    NULL,                               /* flush_gc */
    NULL,                               /* free_gc */
    NULL,                               /* create_font */
    NULL,                               /* free_font */
    close_display,                      /* close_display */
    NULL,                               /* wire_to_event */
    NULL,                               /* event_to_wire */
    NULL,                               /* error */
    NULL,                               /* error_string */
};

static XEXT_GENERATE_FIND_DISPLAY (find_display, xres_ext_info,
                                   xres_extension_name, 
                                   &xres_extension_hooks,
                                   0, NULL)

Bool XResQueryExtension (
    Display *dpy,
    int *event_basep,
    int *error_basep
)
{
    XExtDisplayInfo *info = find_display (dpy);

    if (XextHasExtension(info)) {
        *event_basep = info->codes->first_event;
        *error_basep = info->codes->first_error;
        return True;
    } else {
        return False;
    }
}

Status XResQueryVersion(
    Display *dpy,
    int *major_versionp, 
    int *minor_versionp
)
{
    XExtDisplayInfo *info = find_display (dpy);
    xXResQueryVersionReply rep;
    xXResQueryVersionReq *req;

    XResCheckExtension (dpy, info, 0);

    LockDisplay (dpy);
    GetReq (XResQueryVersion, req);
    req->reqType = info->codes->major_opcode;
    req->XResReqType = X_XResQueryVersion;
    req->client_major = XRES_MAJOR_VERSION;
    req->client_minor = XRES_MINOR_VERSION;
    if (!_XReply (dpy, (xReply *) &rep, 0, xTrue)) {
        UnlockDisplay (dpy);
        SyncHandle ();
        return 0;
    }
    *major_versionp = rep.server_major;
    *minor_versionp = rep.server_minor;
    UnlockDisplay (dpy);
    SyncHandle ();
    return 1;
}


Status XResQueryClients (
    Display *dpy,
    int *num_clients,
    XResClient **clients
)
{
    XExtDisplayInfo *info = find_display (dpy);
    xXResQueryClientsReq *req;
    xXResQueryClientsReply rep;
    XResClient *clnts;
    int result = 0;

    *num_clients = 0;
    *clients = NULL;

    XResCheckExtension (dpy, info, 0);

    LockDisplay (dpy);
    GetReq (XResQueryClients, req);
    req->reqType = info->codes->major_opcode;
    req->XResReqType = X_XResQueryClients;
    if (!_XReply (dpy, (xReply *) &rep, 0, xFalse)) {
        UnlockDisplay (dpy);
        SyncHandle ();
        return 0;
    }

    if(rep.num_clients) {
        if((clnts = Xmalloc(sizeof(XResClient) * rep.num_clients))) {
            xXResClient scratch;
            int i;

            for(i = 0; i < rep.num_clients; i++) {
                _XRead(dpy, (char*)&scratch, sz_xXResClient);
                clnts[i].resource_base = scratch.resource_base;
                clnts[i].resource_mask = scratch.resource_mask;
            }
            *clients = clnts;
            *num_clients = rep.num_clients;
            result = 1;
        } else {
            _XEatData(dpy, rep.length << 2);
        }
    }

    UnlockDisplay (dpy);
    SyncHandle ();
    return result;
}

Status XResQueryClientResources (
    Display *dpy,
    XID xid,
    int *num_types,
    XResType **types
)
{
    XExtDisplayInfo *info = find_display (dpy);
    xXResQueryClientResourcesReq *req;
    xXResQueryClientResourcesReply rep;
    XResType *typs;
    int result = 0;

    *num_types = 0;
    *types = NULL;

    XResCheckExtension (dpy, info, 0);

    LockDisplay (dpy);
    GetReq (XResQueryClientResources, req);
    req->reqType = info->codes->major_opcode;
    req->XResReqType = X_XResQueryClientResources;
    req->xid = xid;
    if (!_XReply (dpy, (xReply *) &rep, 0, xFalse)) {
        UnlockDisplay (dpy);
        SyncHandle ();
        return 0;
    }

    if(rep.num_types) {
        if((typs = Xmalloc(sizeof(XResType) * rep.num_types))) {
            xXResType scratch;
            int i;

            for(i = 0; i < rep.num_types; i++) {
                _XRead(dpy, (char*)&scratch, sz_xXResType);
                typs[i].resource_type = scratch.resource_type;
                typs[i].count = scratch.count;
            }
            *types = typs;
            *num_types = rep.num_types;
            result = 1;
        } else {
            _XEatData(dpy, rep.length << 2);
        }
    }
    
    UnlockDisplay (dpy);
    SyncHandle ();
    return result;
}

Status XResQueryClientPixmapBytes (
    Display *dpy,
    XID xid,
    unsigned long *bytes
)
{
    XExtDisplayInfo *info = find_display (dpy);
    xXResQueryClientPixmapBytesReq *req;
    xXResQueryClientPixmapBytesReply rep;

    *bytes = 0;

    XResCheckExtension (dpy, info, 0);

    LockDisplay (dpy);
    GetReq (XResQueryClientPixmapBytes, req);
    req->reqType = info->codes->major_opcode;
    req->XResReqType = X_XResQueryClientPixmapBytes;
    req->xid = xid;
    if (!_XReply (dpy, (xReply *) &rep, 0, xTrue)) {
        UnlockDisplay (dpy);
        SyncHandle ();
        return 0;
    }

#ifdef LONG64
    *bytes = (rep.bytes_overflow * 4294967295) + rep.bytes;
#else
    *bytes = rep.bytes_overflow ? 0xffffffff : rep.bytes;
#endif

    UnlockDisplay (dpy);
    SyncHandle ();
    return 1;
}

