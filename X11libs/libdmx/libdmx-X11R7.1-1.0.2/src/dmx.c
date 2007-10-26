/* $XFree86$ */
/*
 * Copyright 2002-2004 Red Hat Inc., Durham, North Carolina.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation on the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT.  IN NO EVENT SHALL RED HAT AND/OR THEIR SUPPLIERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * Authors:
 *   Rickard E. (Rik) Faith <faith@redhat.com>
 *
 */

/* THIS IS NOT AN X CONSORTIUM STANDARD */

/** \file
 * This file implements the client-side part of the DMX protocol.  It
 * can be included in client applications by linking with the libdmx.a
 * library. */

#define NEED_REPLIES
#include <X11/Xlibint.h>
#include <X11/extensions/Xext.h>
#define EXTENSION_PROC_ARGS void *
#include <X11/extensions/extutil.h>
#include <X11/extensions/dmxproto.h>
#include <X11/extensions/dmxext.h>

static XExtensionInfo dmx_extension_info_data;
static XExtensionInfo *dmx_extension_info = &dmx_extension_info_data;
static const char     *dmx_extension_name = DMX_EXTENSION_NAME;

#define DMXCheckExtension(dpy,i,val) \
  XextCheckExtension(dpy, i, dmx_extension_name, val)
#define DMXSimpleCheckExtension(dpy,i) \
  XextSimpleCheckExtension(dpy, i, dmx_extension_name)

/*****************************************************************************
 *                                                                           *
 *                         private utility routines                          *
 *                                                                           *
 *****************************************************************************/

static int close_display(Display *dpy, XExtCodes *extCodes);
static /* const */ XExtensionHooks dmx_extension_hooks = {
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

static XEXT_GENERATE_FIND_DISPLAY(find_display, dmx_extension_info,
                                  (char *)dmx_extension_name,
                                  &dmx_extension_hooks,
                                  0, NULL)

static XEXT_GENERATE_CLOSE_DISPLAY(close_display, dmx_extension_info)


/*****************************************************************************
 *                                                                           *
 *                  public DMX Extension routines                            *
 *                                                                           *
 *****************************************************************************/

/** If the server has the DMX extension, the event and error bases will
 * be placed in \a event_basep and \a error_basep, and True will be
 * returned.  Otherwise, False will be returned.
 *
 * Available in DMX Protocol Version 1.0 */
Bool DMXQueryExtension(Display *dpy, int *event_basep, int *error_basep)
{
    XExtDisplayInfo *info = find_display(dpy);

    if (XextHasExtension(info)) {
	*event_basep = info->codes->first_event;
	*error_basep = info->codes->first_error;
	return True;
    } else {
	return False;
    }
}

/** If the DMXQueryVersion protocol request returns version information
 * from the server, \a majorVersion, \a minorVersion, and \a
 * patchVersion are filled in with the appropriate information and True
 * is returned.  Otherwise, False will be returned.
 *
 * Available in DMX Protocol Version 1.0 */
Bool DMXQueryVersion(Display *dpy,
                     int *majorVersion, int *minorVersion, int *patchVersion)
{
    XExtDisplayInfo       *info = find_display(dpy);
    xDMXQueryVersionReply rep;
    xDMXQueryVersionReq   *req;

    DMXCheckExtension(dpy, info, False);

    LockDisplay(dpy);
    GetReq(DMXQueryVersion, req);
    req->reqType     = info->codes->major_opcode;
    req->dmxReqType  = X_DMXQueryVersion;
    if (!_XReply(dpy, (xReply *)&rep, 0, xTrue)) {
	UnlockDisplay(dpy);
	SyncHandle();
	return False;
    }
    *majorVersion = rep.majorVersion;
    *minorVersion = rep.minorVersion;
    *patchVersion = rep.patchVersion;
    UnlockDisplay(dpy);
    SyncHandle();
    return True;
}

/** Flush all pending dmxSync requests in DMX server.
 *
 * Available in DMX Protocol Version 1.5 */
Bool DMXSync(Display *dpy)
{
    XExtDisplayInfo *info = find_display(dpy);
    xDMXSyncReply   rep;
    xDMXSyncReq     *req;

    DMXCheckExtension(dpy, info, False);

    LockDisplay(dpy);
    GetReq(DMXSync, req);
    req->reqType    = info->codes->major_opcode;
    req->dmxReqType = X_DMXSync;
    if (!_XReply(dpy, (xReply *)&rep, 0, xTrue)) {
        UnlockDisplay(dpy);
        SyncHandle();
        return False;
    }

    UnlockDisplay(dpy);
    SyncHandle();
    return rep.status == Success ? True : False;
}

/** The creation of the specified \a window will be forced.
 *
 * Available in DMX Protocol Version 1.2
 * Reply added in DMX Protocol Version 2.0 */
Bool DMXForceWindowCreation(Display *dpy, Window window)
{
    XExtDisplayInfo              *info = find_display(dpy);
    xDMXForceWindowCreationReq   *req;
    xDMXForceWindowCreationReply rep;

    DMXCheckExtension(dpy, info, False);

    LockDisplay(dpy);
    GetReq(DMXForceWindowCreation, req);
    req->reqType    = info->codes->major_opcode;
    req->dmxReqType = X_DMXForceWindowCreation;
    req->window     = window;
    if (!_XReply(dpy, (xReply *)&rep, 0, xTrue)) {
        UnlockDisplay(dpy);
        SyncHandle();
        return False;
    }

    UnlockDisplay(dpy);
    SyncHandle();
    return rep.status == Success ? True : False;
}

/** If the DMXGetScreenCount protocol request returns the screen count,
 * the value will be placed in \a screen_count, and True will be
 * returned.  Otherwise, False will be returned.
 *
 * Available in DMX Protocol Version 1.0 */
Bool DMXGetScreenCount(Display *dpy, int *screen_count)
{
    XExtDisplayInfo         *info = find_display(dpy);
    xDMXGetScreenCountReply rep;
    xDMXGetScreenCountReq   *req;

    DMXCheckExtension(dpy, info, False);

    LockDisplay(dpy);
    GetReq(DMXGetScreenCount, req);
    req->reqType    = info->codes->major_opcode;
    req->dmxReqType = X_DMXGetScreenCount;
    if (!_XReply(dpy, (xReply *)&rep, 0, xTrue)) {
        UnlockDisplay(dpy);
        SyncHandle();
        return False;
    }
    *screen_count = rep.screenCount;
    UnlockDisplay(dpy);
    SyncHandle();
    return True;
}

/** If the DMXGetScreenAttributes protocol request returns information
 * for the specified \a physical_screen, information about the screen
 * will be placed in \a attr, and True will be returned.  Otherwise,
 * False will be returned.
 *
 * Available in DMX Protocol Version 1.0; Modified in Version 2.0 */
Bool DMXGetScreenAttributes(Display *dpy, int physical_screen,
                            DMXScreenAttributes *attr)
{
    XExtDisplayInfo              *info = find_display(dpy);
    xDMXGetScreenAttributesReply rep;
    xDMXGetScreenAttributesReq   *req;

    DMXCheckExtension(dpy, info, False);

    LockDisplay(dpy);
    GetReq(DMXGetScreenAttributes, req);
    req->reqType        = info->codes->major_opcode;
    req->dmxReqType     = X_DMXGetScreenAttributes;
    req->physicalScreen = physical_screen;
    if (!_XReply(dpy, (xReply *)&rep,
                 (SIZEOF(xDMXGetScreenAttributesReply) - 32) >> 2, xFalse)) {
        UnlockDisplay(dpy);
        SyncHandle();
        return False;
    }
    attr->displayName  = Xmalloc(rep.displayNameLength + 1 + 4 /* for pad */);
    _XReadPad(dpy, attr->displayName, rep.displayNameLength);
    attr->displayName[rep.displayNameLength] = '\0';
    attr->logicalScreen       = rep.logicalScreen;
    
    attr->screenWindowWidth   = rep.screenWindowWidth;
    attr->screenWindowHeight  = rep.screenWindowHeight;
    attr->screenWindowXoffset = rep.screenWindowXoffset;
    attr->screenWindowYoffset = rep.screenWindowYoffset;

    attr->rootWindowWidth     = rep.rootWindowWidth;
    attr->rootWindowHeight    = rep.rootWindowHeight;
    attr->rootWindowXoffset   = rep.rootWindowXoffset;
    attr->rootWindowYoffset   = rep.rootWindowYoffset;
    attr->rootWindowXorigin   = rep.rootWindowXorigin;
    attr->rootWindowYorigin   = rep.rootWindowYorigin;
    UnlockDisplay(dpy);
    SyncHandle();
    return True;
}

static CARD32 _DMXGetScreenAttribute(int bit, DMXScreenAttributes *attr)
{
    switch (1 << bit) {
    case DMXScreenWindowWidth:   return attr->screenWindowWidth;
    case DMXScreenWindowHeight:  return attr->screenWindowHeight;
    case DMXScreenWindowXoffset: return attr->screenWindowXoffset;
    case DMXScreenWindowYoffset: return attr->screenWindowYoffset;
    case DMXRootWindowWidth:     return attr->rootWindowWidth;
    case DMXRootWindowHeight:    return attr->rootWindowHeight;
    case DMXRootWindowXoffset:   return attr->rootWindowXoffset;
    case DMXRootWindowYoffset:   return attr->rootWindowYoffset;
    case DMXRootWindowXorigin:   return attr->rootWindowXorigin;
    case DMXRootWindowYorigin:   return attr->rootWindowYorigin;
    default:                     return 0;
    }
}

static int _DMXDumpScreenAttributes(Display *dpy,
                                    unsigned long mask,
                                    DMXScreenAttributes *attr)
{
    int           i;
    unsigned long value_list[32];
    unsigned long *value = value_list;
    int           count  = 0;
        
    for (i = 0; i < 32; i++) {
        if (mask & (1 << i)) {
            *value++ = _DMXGetScreenAttribute(i, attr);
            ++count;
        }
    }
    Data32(dpy, value_list, count * sizeof(CARD32));
    return count;
}

static CARD32 _DMXGetInputAttribute(int bit, DMXInputAttributes *attr)
{
    switch (1 << bit) {
    case DMXInputType:
        switch (attr->inputType) {
        case DMXLocalInputType:   return 0;
        case DMXConsoleInputType: return 1;
        case DMXBackendInputType: return 2;
        }
        return attr->inputType;
    case DMXInputPhysicalScreen: return attr->physicalScreen;
    case DMXInputSendsCore:      return attr->sendsCore;
    default:                     return 0;
    }
}

static int _DMXDumpInputAttributes(Display *dpy,
                                   unsigned long mask,
                                   DMXInputAttributes *attr)
{
    int           i;
    unsigned long value_list[32];
    unsigned long *value = value_list;
    int           count = 0;

    for (i = 0; i < 32; i++) {
        if (mask & (1 << i)) {
            *value++ = _DMXGetInputAttribute(i, attr);
            ++count;
        }
    }
    Data32(dpy, value_list, count * sizeof(CARD32));
    return count;
}

/** Change geometries and positions of the DMX screen and root windows
 * on the back-end X server. */
int DMXChangeScreensAttributes(Display *dpy,
                               int screen_count,
                               int *screens,
                               int mask_count,
                               unsigned int *masks,
                               DMXScreenAttributes *attrs, /* vector */
                               int *error_screen)
{
    XExtDisplayInfo                  *info = find_display(dpy);
    xDMXChangeScreensAttributesReply rep;
    xDMXChangeScreensAttributesReq   *req;
    int                              i;
    unsigned int                     mask  = 0;
    CARD32                           *screen_list;
    CARD32                           *mask_list;

    DMXCheckExtension(dpy, info, False);

    if (screen_count < 1 || mask_count < 1) return DmxBadValue;

    LockDisplay(dpy);
    GetReq(DMXChangeScreensAttributes, req);
    req->reqType      = info->codes->major_opcode;
    req->dmxReqType   = X_DMXChangeScreensAttributes;
    req->screenCount  = screen_count;
    req->maskCount    = mask_count;
    req->length      += screen_count + mask_count;

    screen_list = (CARD32 *)Xmalloc(sizeof(*screen_list) * screen_count);
    for (i = 0; i < screen_count; i++) screen_list[i] = screens[i];
    Data32(dpy, screen_list, screen_count * sizeof(CARD32));
    Xfree(screen_list);

    mask_list = (CARD32 *)Xmalloc(sizeof(*mask_list) * mask_count);
    for (i = 0; i < mask_count;   i++) mask_list[i]   = masks[i];
    Data32(dpy, mask_list, mask_count * sizeof(CARD32));
    Xfree(mask_list);

    for (i = 0; i < screen_count; i++) {
        if (i < mask_count) mask = masks[i];
        req->length += _DMXDumpScreenAttributes(dpy, mask, attrs + i);
    }
    
    if (!_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
        UnlockDisplay(dpy);
        SyncHandle();
        return DmxBadReply;
    }
    if (error_screen) *error_screen = rep.errorScreen;
    UnlockDisplay(dpy);
    SyncHandle();
    return rep.status;
}

/** Add a screen. */
Bool DMXAddScreen(Display *dpy, const char *displayName, unsigned int mask,
                  DMXScreenAttributes *attr, int *screen)
{
    XExtDisplayInfo    *info = find_display(dpy);
    xDMXAddScreenReply rep;
    xDMXAddScreenReq   *req;
    int                length;
    int                paddedLength;

    if (!screen)
	return False;

    DMXCheckExtension(dpy, info, False);

    LockDisplay(dpy);
    GetReq(DMXAddScreen, req);
    length                 = displayName ? strlen(displayName) : 0;
    paddedLength           = (length + 3) & ~3;
    req->reqType           = info->codes->major_opcode;
    req->dmxReqType        = X_DMXAddScreen;
    req->displayNameLength = length;
    req->physicalScreen    = *screen;
    req->valueMask         = mask;
    req->length           += paddedLength/4;
    req->length           += _DMXDumpScreenAttributes(dpy, mask, attr);

    if (length) {
        char *buffer       = Xmalloc(paddedLength);
        memset(buffer, 0, paddedLength);
        memcpy(buffer, displayName, length);
        Data32(dpy, buffer, paddedLength);
        Xfree(buffer);
    }

    if (!_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
        UnlockDisplay(dpy);
        SyncHandle();
        return False;
    }
    if (screen) *screen = rep.physicalScreen;
    UnlockDisplay(dpy);
    SyncHandle();
    return rep.status == Success ? True : False;
}

/** Remove a screen. */
Bool DMXRemoveScreen(Display *dpy, int screen)
{
    XExtDisplayInfo       *info = find_display(dpy);
    xDMXRemoveScreenReply rep;
    xDMXRemoveScreenReq   *req;

    DMXCheckExtension(dpy, info, False);

    LockDisplay(dpy);
    GetReq(DMXRemoveScreen, req);
    req->reqType           = info->codes->major_opcode;
    req->dmxReqType        = X_DMXRemoveScreen;
    req->physicalScreen    = screen;

    if (!_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
        UnlockDisplay(dpy);
        SyncHandle();
        return False;
    }
    UnlockDisplay(dpy);
    SyncHandle();
    return rep.status == Success ? True : False;
}

/** If the DMXGetWindowAttributes protocol request returns information
 * about the specified \a window, the number of screens for which
 * information is available will be returned in \a screen_count and
 * information about the first \a available_count of those screens will
 * be placed in \a inf.  Because this call transports a great deal of
 * information over the wire, please call #DMXGetScreenCount first, and
 * make sure \a inf is that large.
 *
 * Note that if the specified \a window has not yet been mapped when
 * #DMXGetWindowAttributes is called, then a subsequent XMapWindow call
 * might be buffered in xlib while requests directly to the back-end X
 * servers are processed.  This race condition can be solved by calling
 * #DMXSync before talking directly to the back-end X servers.
 *
 * Available in DMX Protocol Version 1.0, but not working correctly
 * until DMX Protocol Version 1.4 */
Bool DMXGetWindowAttributes(Display *dpy, Window window,
                            int *screen_count, int available_count,
                            DMXWindowAttributes *inf)
{
    XExtDisplayInfo              *info = find_display(dpy);
    xDMXGetWindowAttributesReply rep;
    xDMXGetWindowAttributesReq   *req;
    unsigned long                current;
    CARD32                       *screens; /* Must match protocol size */
    CARD32                       *windows; /* Must match protocol size */
    XRectangle                   *pos;     /* Must match protocol size */
    XRectangle                   *vis;     /* Must match protocol size */

    DMXCheckExtension(dpy, info, False);

    LockDisplay(dpy);
    GetReq(DMXGetWindowAttributes, req);
    req->reqType    = info->codes->major_opcode;
    req->dmxReqType = X_DMXGetWindowAttributes;
    req->window     = window;
    if (!_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
        UnlockDisplay(dpy);
        SyncHandle();
        return False;
    }

                                /* FIXME: check for NULL? */
    screens    = Xmalloc(rep.screenCount * sizeof(*screens));
    windows    = Xmalloc(rep.screenCount * sizeof(*windows));
    pos        = Xmalloc(rep.screenCount * sizeof(*pos));
    vis        = Xmalloc(rep.screenCount * sizeof(*vis));

    _XRead(dpy, (char *)screens, rep.screenCount * sizeof(*screens));
    _XRead(dpy, (char *)windows, rep.screenCount * sizeof(*windows));
    _XRead(dpy, (char *)pos,     rep.screenCount * sizeof(*pos));
    _XRead(dpy, (char *)vis,     rep.screenCount * sizeof(*vis));
    
    *screen_count = rep.screenCount;
    for (current = 0;
         current < rep.screenCount && current < (unsigned)available_count;
         current++, inf++) {
        inf->screen    = screens[current];
        inf->window    = windows[current];
        inf->pos       = pos[current];
        inf->vis       = vis[current];
    }

    Xfree(vis);
    Xfree(pos);
    Xfree(windows);
    Xfree(screens);

    UnlockDisplay(dpy);
    SyncHandle();
    return True;
}

/** If the DMXGetDesktopAttributes protocol request returns information
 * correctly, the information will be placed in \a attr, and True will
 * be returned.  Otherwise, False will be returned.
 *
 * Available in DMX Protocol Version 2.0 */
Bool DMXGetDesktopAttributes(Display *dpy, DMXDesktopAttributes *attr)
{
    XExtDisplayInfo               *info = find_display(dpy);
    xDMXGetDesktopAttributesReply rep;
    xDMXGetDesktopAttributesReq   *req;

    DMXCheckExtension(dpy, info, False);

    LockDisplay(dpy);
    GetReq(DMXGetDesktopAttributes, req);
    req->reqType        = info->codes->major_opcode;
    req->dmxReqType     = X_DMXGetDesktopAttributes;
    if (!_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
        UnlockDisplay(dpy);
        SyncHandle();
        return False;
    }
    attr->width  = rep.width;
    attr->height = rep.height;
    attr->shiftX = rep.shiftX;
    attr->shiftY = rep.shiftY;

    UnlockDisplay(dpy);
    SyncHandle();
    return True;
}

static CARD32 _DMXGetDesktopAttribute(int bit, DMXDesktopAttributes *attr)
{
    switch (1 << bit) {
    case DMXDesktopWidth:  return attr->width;
    case DMXDesktopHeight: return attr->height;
    case DMXDesktopShiftX: return attr->shiftX;
    case DMXDesktopShiftY: return attr->shiftY;
    default:               return 0;
    }
}

static int _DMXDumpDesktopAttributes(Display *dpy,
                                     unsigned long mask,
                                     DMXDesktopAttributes *attr)
{
    int           i;
    unsigned long value_list[32];
    unsigned long *value = value_list;
    int           count  = 0;
        
    for (i = 0; i < 32; i++) {
        if (mask & (1 << i)) {
            *value++ = _DMXGetDesktopAttribute(i, attr);
            ++count;
        }
    }
    Data32(dpy, value_list, count * sizeof(CARD32));
    return count;
}

/** Change the global bounding box and origin offset.
 *
 * Available in DMX Protocol Version 2.0 */
int DMXChangeDesktopAttributes(Display *dpy,
                               unsigned int mask,
                               DMXDesktopAttributes *attr) 
{
    XExtDisplayInfo                  *info = find_display(dpy);
    xDMXChangeDesktopAttributesReply rep;
    xDMXChangeDesktopAttributesReq   *req;

    DMXCheckExtension(dpy, info, False);

    LockDisplay(dpy);
    GetReq(DMXChangeDesktopAttributes, req);
    req->reqType      = info->codes->major_opcode;
    req->dmxReqType   = X_DMXChangeDesktopAttributes;
    req->valueMask    = mask;
    req->length      +=_DMXDumpDesktopAttributes(dpy, mask, attr);
    
    if (!_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
        UnlockDisplay(dpy);
        SyncHandle();
        return DmxBadReply;
    }
    UnlockDisplay(dpy);
    SyncHandle();
    return rep.status;
}

/** If the DMXGetInputCount protocol request returns the input count,
 * the value will be placed in \a input_count, and True will be
 * returned.  Otherwise, False will be returned.
 *
 * Available in DMX Protocol Version 1.1 */
Bool DMXGetInputCount(Display *dpy, int *input_count)
{
    XExtDisplayInfo         *info = find_display(dpy);
    xDMXGetInputCountReply  rep;
    xDMXGetInputCountReq    *req;

    DMXCheckExtension(dpy, info, False);

    LockDisplay(dpy);
    GetReq(DMXGetInputCount, req);
    req->reqType    = info->codes->major_opcode;
    req->dmxReqType = X_DMXGetInputCount;
    if (!_XReply(dpy, (xReply *)&rep, 0, xTrue)) {
        UnlockDisplay(dpy);
        SyncHandle();
        return False;
    }
    *input_count = rep.inputCount;
    UnlockDisplay(dpy);
    SyncHandle();
    return True;
}

/** If the DMXGetInputAttributes protocol request returns information
 * about the input device with the specified \a id, information about
 * the input device will be placed in \a inf, and True will be returned.
 * Otherwise, False will be returned.
 *
 * Available in DMX Protocol Version 1.1 */
Bool DMXGetInputAttributes(Display *dpy, int id, DMXInputAttributes *inf)
{
    XExtDisplayInfo             *info = find_display(dpy);
    xDMXGetInputAttributesReply rep;
    xDMXGetInputAttributesReq   *req;
    char                        *buffer;

    DMXCheckExtension(dpy, info, False);

    LockDisplay(dpy);
    GetReq(DMXGetInputAttributes, req);
    req->reqType    = info->codes->major_opcode;
    req->dmxReqType = X_DMXGetInputAttributes;
    req->deviceId   = id;
    if (!_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
        UnlockDisplay(dpy);
        SyncHandle();
        return False;
    }

    switch (rep.inputType) {
    case 0: inf->inputType = DMXLocalInputType;   break;
    case 1: inf->inputType = DMXConsoleInputType; break;
    case 2: inf->inputType = DMXBackendInputType; break;
    }

    inf->physicalScreen = rep.physicalScreen;
    inf->physicalId     = rep.physicalId;
    inf->isCore         = rep.isCore;
    inf->sendsCore      = rep.sendsCore;
    inf->detached       = rep.detached;
    buffer              = Xmalloc(rep.nameLength + 1 + 4 /* for pad */);
    _XReadPad(dpy, buffer, rep.nameLength);
    buffer[rep.nameLength] = '\0';
    inf->name           = buffer;
    UnlockDisplay(dpy);
    SyncHandle();
    return True;
}

/** Add input. */
Bool DMXAddInput(Display *dpy, unsigned int mask, DMXInputAttributes *attr,
                 int *id)
{
    XExtDisplayInfo         *info = find_display(dpy);
    xDMXAddInputReply       rep;
    xDMXAddInputReq         *req;
    int                     length;
    int                     paddedLength;

    DMXCheckExtension(dpy, info, False);

    LockDisplay(dpy);
    GetReq(DMXAddInput, req);
    length                 = attr->name ? strlen(attr->name) : 0;
    paddedLength           = (length + 3) & ~3;
    req->reqType           = info->codes->major_opcode;
    req->dmxReqType        = X_DMXAddInput;
    req->displayNameLength = length;
    req->valueMask         = mask;
    req->length           += paddedLength/4;
    req->length           += _DMXDumpInputAttributes(dpy, mask, attr);

    if (length) {
        char *buffer       = Xmalloc(paddedLength);
        memset(buffer, 0, paddedLength);
        memcpy(buffer, attr->name, paddedLength);
        Data32(dpy, buffer, paddedLength);
        Xfree(buffer);
    }

    if (!_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
        UnlockDisplay(dpy);
        SyncHandle();
        return False;
    }
    if (id) *id = rep.physicalId;
    UnlockDisplay(dpy);
    SyncHandle();
    return rep.status == Success ? True : False;
}

/** Add backend input (a helper function that calls #DMXAddInput). */
Bool DMXAddBackendInput(Display *dpy, int screen, int sendsCore, int *newId)
{
    DMXInputAttributes attr;
    unsigned int       mask = (DMXInputType
                               | DMXInputPhysicalScreen
                               | DMXInputSendsCore);
        
    attr.inputType        = DMXBackendInputType;
    attr.physicalScreen   = screen;
    attr.sendsCore        = sendsCore;
    attr.name             = NULL;
    return DMXAddInput(dpy, mask, &attr, newId);
}

/** Add console input (a helper function that calls #DMXAddInput). */
Bool DMXAddConsoleInput(Display *dpy, const char *name, int sendsCore,
                        int *newId)
{
    DMXInputAttributes attr;
    unsigned int       mask = (DMXInputType
                               | DMXInputSendsCore);
        
    attr.inputType        = DMXConsoleInputType;
    attr.physicalScreen   = 0;
    attr.sendsCore        = sendsCore;
    attr.name             = name;
    return DMXAddInput(dpy, mask, &attr, newId);
}

/** Remove an input. */
Bool DMXRemoveInput(Display *dpy, int id)
{
    XExtDisplayInfo      *info = find_display(dpy);
    xDMXRemoveInputReply rep;
    xDMXRemoveInputReq   *req;

    DMXCheckExtension(dpy, info, False);

    LockDisplay(dpy);
    GetReq(DMXRemoveInput, req);
    req->reqType          = info->codes->major_opcode;
    req->dmxReqType       = X_DMXRemoveInput;
    req->physicalId       = id;

    if (!_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
        UnlockDisplay(dpy);
        SyncHandle();
        return False;
    }
    UnlockDisplay(dpy);
    SyncHandle();
    return rep.status == Success ? True : False;
}
