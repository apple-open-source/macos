#
# This file generated automatically from shape.xml by py_client.py.
# Edit at your peril.
#

import xcb
import cStringIO
from struct import pack, unpack_from
from array import array
import xproto

MAJOR_VERSION = 1
MINOR_VERSION = 1

key = xcb.ExtensionKey('SHAPE')

class SO:
    Set = 0
    Union = 1
    Intersect = 2
    Subtract = 3
    Invert = 4

class SK:
    Bounding = 0
    Clip = 1
    Input = 2

class NotifyEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.shape_kind, self.affected_window, self.extents_x, self.extents_y, self.extents_width, self.extents_height, self.server_time, self.shaped,) = unpack_from('xB2xIhhHHIB11x', self, count)

class QueryVersionCookie(xcb.Cookie):
    pass

class QueryVersionReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.major_version, self.minor_version,) = unpack_from('xx2x4xHH', self, count)

class QueryExtentsCookie(xcb.Cookie):
    pass

class QueryExtentsReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.bounding_shaped, self.clip_shaped, self.bounding_shape_extents_x, self.bounding_shape_extents_y, self.bounding_shape_extents_width, self.bounding_shape_extents_height, self.clip_shape_extents_x, self.clip_shape_extents_y, self.clip_shape_extents_width, self.clip_shape_extents_height,) = unpack_from('xx2x4xBB2xhhHHhhHH', self, count)

class InputSelectedCookie(xcb.Cookie):
    pass

class InputSelectedReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.enabled,) = unpack_from('xB2x4x', self, count)

class GetRectanglesCookie(xcb.Cookie):
    pass

class GetRectanglesReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.ordering, self.rectangles_len,) = unpack_from('xB2x4xI', self, count)
        count += 12
        self.rectangles = xcb.List(self, count, self.rectangles_len, RECTANGLE, 8)

class shapeExtension(xcb.Extension):

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

    def RectanglesChecked(self, operation, destination_kind, ordering, destination_window, x_offset, y_offset, rectangles_len, rectangles):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBBBxIhh', operation, destination_kind, ordering, destination_window, x_offset, y_offset))
        for elt in xcb.Iterator(rectangles, 4, 'rectangles', True):
            buf.write(pack('hhHH', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 1, True, True),
                                 xcb.VoidCookie())

    def Rectangles(self, operation, destination_kind, ordering, destination_window, x_offset, y_offset, rectangles_len, rectangles):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBBBxIhh', operation, destination_kind, ordering, destination_window, x_offset, y_offset))
        for elt in xcb.Iterator(rectangles, 4, 'rectangles', True):
            buf.write(pack('hhHH', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 1, True, False),
                                 xcb.VoidCookie())

    def MaskChecked(self, operation, destination_kind, destination_window, x_offset, y_offset, source_bitmap):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBB2xIhhI', operation, destination_kind, destination_window, x_offset, y_offset, source_bitmap))
        return self.send_request(xcb.Request(buf.getvalue(), 2, True, True),
                                 xcb.VoidCookie())

    def Mask(self, operation, destination_kind, destination_window, x_offset, y_offset, source_bitmap):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBB2xIhhI', operation, destination_kind, destination_window, x_offset, y_offset, source_bitmap))
        return self.send_request(xcb.Request(buf.getvalue(), 2, True, False),
                                 xcb.VoidCookie())

    def CombineChecked(self, operation, destination_kind, source_kind, destination_window, x_offset, y_offset, source_window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBBBxIhhI', operation, destination_kind, source_kind, destination_window, x_offset, y_offset, source_window))
        return self.send_request(xcb.Request(buf.getvalue(), 3, True, True),
                                 xcb.VoidCookie())

    def Combine(self, operation, destination_kind, source_kind, destination_window, x_offset, y_offset, source_window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBBBxIhhI', operation, destination_kind, source_kind, destination_window, x_offset, y_offset, source_window))
        return self.send_request(xcb.Request(buf.getvalue(), 3, True, False),
                                 xcb.VoidCookie())

    def OffsetChecked(self, destination_kind, destination_window, x_offset, y_offset):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xB3xIhh', destination_kind, destination_window, x_offset, y_offset))
        return self.send_request(xcb.Request(buf.getvalue(), 4, True, True),
                                 xcb.VoidCookie())

    def Offset(self, destination_kind, destination_window, x_offset, y_offset):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xB3xIhh', destination_kind, destination_window, x_offset, y_offset))
        return self.send_request(xcb.Request(buf.getvalue(), 4, True, False),
                                 xcb.VoidCookie())

    def QueryExtents(self, destination_window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', destination_window))
        return self.send_request(xcb.Request(buf.getvalue(), 5, False, True),
                                 QueryExtentsCookie(),
                                 QueryExtentsReply)

    def QueryExtentsUnchecked(self, destination_window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', destination_window))
        return self.send_request(xcb.Request(buf.getvalue(), 5, False, False),
                                 QueryExtentsCookie(),
                                 QueryExtentsReply)

    def SelectInputChecked(self, destination_window, enable):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIB3x', destination_window, enable))
        return self.send_request(xcb.Request(buf.getvalue(), 6, True, True),
                                 xcb.VoidCookie())

    def SelectInput(self, destination_window, enable):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIB3x', destination_window, enable))
        return self.send_request(xcb.Request(buf.getvalue(), 6, True, False),
                                 xcb.VoidCookie())

    def InputSelected(self, destination_window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', destination_window))
        return self.send_request(xcb.Request(buf.getvalue(), 7, False, True),
                                 InputSelectedCookie(),
                                 InputSelectedReply)

    def InputSelectedUnchecked(self, destination_window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', destination_window))
        return self.send_request(xcb.Request(buf.getvalue(), 7, False, False),
                                 InputSelectedCookie(),
                                 InputSelectedReply)

    def GetRectangles(self, window, source_kind):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIB3x', window, source_kind))
        return self.send_request(xcb.Request(buf.getvalue(), 8, False, True),
                                 GetRectanglesCookie(),
                                 GetRectanglesReply)

    def GetRectanglesUnchecked(self, window, source_kind):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIB3x', window, source_kind))
        return self.send_request(xcb.Request(buf.getvalue(), 8, False, False),
                                 GetRectanglesCookie(),
                                 GetRectanglesReply)

_events = {
    0 : NotifyEvent,
}

_errors = {
}

xcb._add_ext(key, shapeExtension, _events, _errors)
