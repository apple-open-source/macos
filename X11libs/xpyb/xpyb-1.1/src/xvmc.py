#
# This file generated automatically from xvmc.xml by py_client.py.
# Edit at your peril.
#

import xcb
import cStringIO
from struct import pack, unpack_from
from array import array
import xproto
import shm
import xv

MAJOR_VERSION = 1
MINOR_VERSION = 1

key = xcb.ExtensionKey('XVideo-MotionCompensation')

class SurfaceInfo(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.id, self.chroma_format, self.pad0, self.max_width, self.max_height, self.subpicture_max_width, self.subpicture_max_height, self.mc_type, self.flags,) = unpack_from('IHHHHHHII', self, count)

class QueryVersionCookie(xcb.Cookie):
    pass

class QueryVersionReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.major, self.minor,) = unpack_from('xx2x4xII', self, count)

class ListSurfaceTypesCookie(xcb.Cookie):
    pass

class ListSurfaceTypesReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.num,) = unpack_from('xx2x4xI20x', self, count)
        count += 32
        self.surfaces = xcb.List(self, count, self.num, SurfaceInfo, 24)

class CreateContextCookie(xcb.Cookie):
    pass

class CreateContextReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.width_actual, self.height_actual, self.flags_return,) = unpack_from('xx2x4xHHI20x', self, count)
        count += 36
        self.priv_data = xcb.List(self, count, self.length, 'I', 4)

class CreateSurfaceCookie(xcb.Cookie):
    pass

class CreateSurfaceReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        count += 32
        self.priv_data = xcb.List(self, count, self.length, 'I', 4)

class CreateSubpictureCookie(xcb.Cookie):
    pass

class CreateSubpictureReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.width_actual, self.height_actual, self.num_palette_entries, self.entry_bytes,) = unpack_from('xx2x4xHHHH', self, count)
        count += 16
        self.component_order = xcb.List(self, count, 4, 'B', 1)
        count += len(self.component_order.buf())
        count += 12
        count += xcb.type_pad(4, count)
        self.priv_data = xcb.List(self, count, self.length, 'I', 4)

class ListSubpictureTypesCookie(xcb.Cookie):
    pass

class ListSubpictureTypesReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.num,) = unpack_from('xx2x4xI20x', self, count)
        count += 32
        self.types = xcb.List(self, count, self.num, ImageFormatInfo, 128)

class xvmcExtension(xcb.Extension):

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

    def ListSurfaceTypes(self, port_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', port_id))
        return self.send_request(xcb.Request(buf.getvalue(), 1, False, True),
                                 ListSurfaceTypesCookie(),
                                 ListSurfaceTypesReply)

    def ListSurfaceTypesUnchecked(self, port_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', port_id))
        return self.send_request(xcb.Request(buf.getvalue(), 1, False, False),
                                 ListSurfaceTypesCookie(),
                                 ListSurfaceTypesReply)

    def CreateContext(self, context_id, port_id, surface_id, width, height, flags):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIHHI', context_id, port_id, surface_id, width, height, flags))
        return self.send_request(xcb.Request(buf.getvalue(), 2, False, True),
                                 CreateContextCookie(),
                                 CreateContextReply)

    def CreateContextUnchecked(self, context_id, port_id, surface_id, width, height, flags):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIHHI', context_id, port_id, surface_id, width, height, flags))
        return self.send_request(xcb.Request(buf.getvalue(), 2, False, False),
                                 CreateContextCookie(),
                                 CreateContextReply)

    def DestroyContextChecked(self, context_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context_id))
        return self.send_request(xcb.Request(buf.getvalue(), 3, True, True),
                                 xcb.VoidCookie())

    def DestroyContext(self, context_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context_id))
        return self.send_request(xcb.Request(buf.getvalue(), 3, True, False),
                                 xcb.VoidCookie())

    def CreateSurface(self, surface_id, context_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', surface_id, context_id))
        return self.send_request(xcb.Request(buf.getvalue(), 4, False, True),
                                 CreateSurfaceCookie(),
                                 CreateSurfaceReply)

    def CreateSurfaceUnchecked(self, surface_id, context_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', surface_id, context_id))
        return self.send_request(xcb.Request(buf.getvalue(), 4, False, False),
                                 CreateSurfaceCookie(),
                                 CreateSurfaceReply)

    def DestroySurfaceChecked(self, surface_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', surface_id))
        return self.send_request(xcb.Request(buf.getvalue(), 5, True, True),
                                 xcb.VoidCookie())

    def DestroySurface(self, surface_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', surface_id))
        return self.send_request(xcb.Request(buf.getvalue(), 5, True, False),
                                 xcb.VoidCookie())

    def CreateSubpicture(self, subpicture_id, context, xvimage_id, width, height):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIHH', subpicture_id, context, xvimage_id, width, height))
        return self.send_request(xcb.Request(buf.getvalue(), 6, False, True),
                                 CreateSubpictureCookie(),
                                 CreateSubpictureReply)

    def CreateSubpictureUnchecked(self, subpicture_id, context, xvimage_id, width, height):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIHH', subpicture_id, context, xvimage_id, width, height))
        return self.send_request(xcb.Request(buf.getvalue(), 6, False, False),
                                 CreateSubpictureCookie(),
                                 CreateSubpictureReply)

    def DestroySubpictureChecked(self, subpicture_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', subpicture_id))
        return self.send_request(xcb.Request(buf.getvalue(), 7, True, True),
                                 xcb.VoidCookie())

    def DestroySubpicture(self, subpicture_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', subpicture_id))
        return self.send_request(xcb.Request(buf.getvalue(), 7, True, False),
                                 xcb.VoidCookie())

    def ListSubpictureTypes(self, port_id, surface_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', port_id, surface_id))
        return self.send_request(xcb.Request(buf.getvalue(), 8, False, True),
                                 ListSubpictureTypesCookie(),
                                 ListSubpictureTypesReply)

    def ListSubpictureTypesUnchecked(self, port_id, surface_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', port_id, surface_id))
        return self.send_request(xcb.Request(buf.getvalue(), 8, False, False),
                                 ListSubpictureTypesCookie(),
                                 ListSubpictureTypesReply)

_events = {
}

_errors = {
}

xcb._add_ext(key, xvmcExtension, _events, _errors)
