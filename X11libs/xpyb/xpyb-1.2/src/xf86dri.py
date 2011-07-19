#
# This file generated automatically from xf86dri.xml by py_client.py.
# Edit at your peril.
#

import xcb
import cStringIO
from struct import pack, unpack_from
from array import array

MAJOR_VERSION = 4
MINOR_VERSION = 1

key = xcb.ExtensionKey('XFree86-DRI')

class DrmClipRect(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.x1, self.y1, self.x2, self.x3,) = unpack_from('hhhh', self, count)

class QueryVersionCookie(xcb.Cookie):
    pass

class QueryVersionReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.dri_major_version, self.dri_minor_version, self.dri_minor_patch,) = unpack_from('xx2x4xHHI', self, count)

class QueryDirectRenderingCapableCookie(xcb.Cookie):
    pass

class QueryDirectRenderingCapableReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.is_capable,) = unpack_from('xx2x4xB', self, count)

class OpenConnectionCookie(xcb.Cookie):
    pass

class OpenConnectionReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.sarea_handle_low, self.sarea_handle_high, self.bus_id_len,) = unpack_from('xx2x4xIII12x', self, count)
        count += 32
        self.bus_id = xcb.List(self, count, self.bus_id_len, 'b', 1)

class GetClientDriverNameCookie(xcb.Cookie):
    pass

class GetClientDriverNameReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.client_driver_major_version, self.client_driver_minor_version, self.client_driver_patch_version, self.client_driver_name_len,) = unpack_from('xx2x4xIIII8x', self, count)
        count += 32
        self.client_driver_name = xcb.List(self, count, self.client_driver_name_len, 'b', 1)

class CreateContextCookie(xcb.Cookie):
    pass

class CreateContextReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.hw_context,) = unpack_from('xx2x4xI', self, count)

class CreateDrawableCookie(xcb.Cookie):
    pass

class CreateDrawableReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.hw_drawable_handle,) = unpack_from('xx2x4xI', self, count)

class GetDrawableInfoCookie(xcb.Cookie):
    pass

class GetDrawableInfoReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.drawable_table_index, self.drawable_table_stamp, self.drawable_origin_X, self.drawable_origin_Y, self.drawable_size_W, self.drawable_size_H, self.num_clip_rects,) = unpack_from('xx2x4xIIhhhhI4x', self, count)
        count += 32
        self.clip_rects = xcb.List(self, count, self.num_clip_rects, DrmClipRect, 8)

class GetDeviceInfoCookie(xcb.Cookie):
    pass

class GetDeviceInfoReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.framebuffer_handle_low, self.framebuffer_handle_high, self.framebuffer_origin_offset, self.framebuffer_size, self.framebuffer_stride, self.device_private_size,) = unpack_from('xx2x4xIIIIII', self, count)
        count += 32
        self.device_private = xcb.List(self, count, self.device_private_size, 'I', 4)

class AuthConnectionCookie(xcb.Cookie):
    pass

class AuthConnectionReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.authenticated,) = unpack_from('xx2x4xI', self, count)

class xf86driExtension(xcb.Extension):

    def QueryVersion(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 0, False, True),
                                 QueryVersionCookie(),
                                 QueryVersionReply)

    def QueryVersionUnchecked(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 0, False, False),
                                 QueryVersionCookie(),
                                 QueryVersionReply)

    def QueryDirectRenderingCapable(self, screen):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', screen))
        return self.send_request(xcb.Request(buf.getvalue(), 1, False, True),
                                 QueryDirectRenderingCapableCookie(),
                                 QueryDirectRenderingCapableReply)

    def QueryDirectRenderingCapableUnchecked(self, screen):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', screen))
        return self.send_request(xcb.Request(buf.getvalue(), 1, False, False),
                                 QueryDirectRenderingCapableCookie(),
                                 QueryDirectRenderingCapableReply)

    def OpenConnection(self, screen):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', screen))
        return self.send_request(xcb.Request(buf.getvalue(), 2, False, True),
                                 OpenConnectionCookie(),
                                 OpenConnectionReply)

    def OpenConnectionUnchecked(self, screen):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', screen))
        return self.send_request(xcb.Request(buf.getvalue(), 2, False, False),
                                 OpenConnectionCookie(),
                                 OpenConnectionReply)

    def CloseConnectionChecked(self, screen):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', screen))
        return self.send_request(xcb.Request(buf.getvalue(), 3, True, True),
                                 xcb.VoidCookie())

    def CloseConnection(self, screen):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', screen))
        return self.send_request(xcb.Request(buf.getvalue(), 3, True, False),
                                 xcb.VoidCookie())

    def GetClientDriverName(self, screen):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', screen))
        return self.send_request(xcb.Request(buf.getvalue(), 4, False, True),
                                 GetClientDriverNameCookie(),
                                 GetClientDriverNameReply)

    def GetClientDriverNameUnchecked(self, screen):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', screen))
        return self.send_request(xcb.Request(buf.getvalue(), 4, False, False),
                                 GetClientDriverNameCookie(),
                                 GetClientDriverNameReply)

    def CreateContext(self, screen, visual, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', screen, visual, context))
        return self.send_request(xcb.Request(buf.getvalue(), 5, False, True),
                                 CreateContextCookie(),
                                 CreateContextReply)

    def CreateContextUnchecked(self, screen, visual, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', screen, visual, context))
        return self.send_request(xcb.Request(buf.getvalue(), 5, False, False),
                                 CreateContextCookie(),
                                 CreateContextReply)

    def DestroyContextChecked(self, screen, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', screen, context))
        return self.send_request(xcb.Request(buf.getvalue(), 6, True, True),
                                 xcb.VoidCookie())

    def DestroyContext(self, screen, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', screen, context))
        return self.send_request(xcb.Request(buf.getvalue(), 6, True, False),
                                 xcb.VoidCookie())

    def CreateDrawable(self, screen, drawable):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', screen, drawable))
        return self.send_request(xcb.Request(buf.getvalue(), 7, False, True),
                                 CreateDrawableCookie(),
                                 CreateDrawableReply)

    def CreateDrawableUnchecked(self, screen, drawable):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', screen, drawable))
        return self.send_request(xcb.Request(buf.getvalue(), 7, False, False),
                                 CreateDrawableCookie(),
                                 CreateDrawableReply)

    def DestroyDrawableChecked(self, screen, drawable):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', screen, drawable))
        return self.send_request(xcb.Request(buf.getvalue(), 8, True, True),
                                 xcb.VoidCookie())

    def DestroyDrawable(self, screen, drawable):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', screen, drawable))
        return self.send_request(xcb.Request(buf.getvalue(), 8, True, False),
                                 xcb.VoidCookie())

    def GetDrawableInfo(self, screen, drawable):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', screen, drawable))
        return self.send_request(xcb.Request(buf.getvalue(), 9, False, True),
                                 GetDrawableInfoCookie(),
                                 GetDrawableInfoReply)

    def GetDrawableInfoUnchecked(self, screen, drawable):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', screen, drawable))
        return self.send_request(xcb.Request(buf.getvalue(), 9, False, False),
                                 GetDrawableInfoCookie(),
                                 GetDrawableInfoReply)

    def GetDeviceInfo(self, screen):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', screen))
        return self.send_request(xcb.Request(buf.getvalue(), 10, False, True),
                                 GetDeviceInfoCookie(),
                                 GetDeviceInfoReply)

    def GetDeviceInfoUnchecked(self, screen):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', screen))
        return self.send_request(xcb.Request(buf.getvalue(), 10, False, False),
                                 GetDeviceInfoCookie(),
                                 GetDeviceInfoReply)

    def AuthConnection(self, screen, magic):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', screen, magic))
        return self.send_request(xcb.Request(buf.getvalue(), 11, False, True),
                                 AuthConnectionCookie(),
                                 AuthConnectionReply)

    def AuthConnectionUnchecked(self, screen, magic):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', screen, magic))
        return self.send_request(xcb.Request(buf.getvalue(), 11, False, False),
                                 AuthConnectionCookie(),
                                 AuthConnectionReply)

_events = {
}

_errors = {
}

xcb._add_ext(key, xf86driExtension, _events, _errors)
