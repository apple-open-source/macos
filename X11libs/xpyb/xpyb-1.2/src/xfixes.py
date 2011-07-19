#
# This file generated automatically from xfixes.xml by py_client.py.
# Edit at your peril.
#

import xcb
import cStringIO
from struct import pack, unpack_from
from array import array
import xproto
import render
import shape

MAJOR_VERSION = 4
MINOR_VERSION = 0

key = xcb.ExtensionKey('XFIXES')

class QueryVersionCookie(xcb.Cookie):
    pass

class QueryVersionReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.major_version, self.minor_version,) = unpack_from('xx2x4xII16x', self, count)

class SaveSetMode:
    Insert = 0
    Delete = 1

class SaveSetTarget:
    Nearest = 0
    Root = 1

class SaveSetMapping:
    Map = 0
    Unmap = 1

class SelectionEvent:
    SetSelectionOwner = 0
    SelectionWindowDestroy = 1
    SelectionClientClose = 2

class SelectionEventMask:
    SetSelectionOwner = 1
    SelectionWindowDestroy = 2
    SelectionClientClose = 4

class SelectionNotifyEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.subtype, self.window, self.owner, self.selection, self.timestamp, self.selection_timestamp,) = unpack_from('xB2xIIIII8x', self, count)

class CursorNotify:
    DisplayCursor = 0

class CursorNotifyMask:
    DisplayCursor = 1

class CursorNotifyEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.subtype, self.window, self.cursor_serial, self.timestamp, self.name,) = unpack_from('xB2xIIII12x', self, count)

class GetCursorImageCookie(xcb.Cookie):
    pass

class GetCursorImageReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.x, self.y, self.width, self.height, self.xhot, self.yhot, self.cursor_serial,) = unpack_from('xx2x4xhhHHHHI8x', self, count)
        count += 32
        self.cursor_image = xcb.List(self, count, (self.width * self.height), 'I', 4)

class RegionError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)

class BadRegion(xcb.ProtocolException):
    pass

class Region:
    _None = 0

class FetchRegionCookie(xcb.Cookie):
    pass

class FetchRegionReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        count += 8
        self.extents = RECTANGLE(self, count, 8)
        count += 8
        count += 16
        count += xcb.type_pad(8, count)
        self.rectangles = xcb.List(self, count, (self.length / 2), RECTANGLE, 8)

class GetCursorNameCookie(xcb.Cookie):
    pass

class GetCursorNameReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.atom, self.nbytes,) = unpack_from('xx2x4xIH18x', self, count)
        count += 32
        self.name = xcb.List(self, count, self.nbytes, 'b', 1)

class GetCursorImageAndNameCookie(xcb.Cookie):
    pass

class GetCursorImageAndNameReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.x, self.y, self.width, self.height, self.xhot, self.yhot, self.cursor_serial, self.cursor_atom, self.nbytes,) = unpack_from('xx2x4xhhHHHHIIH2x', self, count)
        count += 32
        self.name = xcb.List(self, count, self.nbytes, 'b', 1)
        count += len(self.name.buf())
        count += xcb.type_pad(4, count)
        self.cursor_image = xcb.List(self, count, (self.width * self.height), 'I', 4)

class xfixesExtension(xcb.Extension):

    def QueryVersion(self, client_major_version, client_minor_version):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', client_major_version, client_minor_version))
        return self.send_request(xcb.Request(buf.getvalue(), 0, False, True),
                                 QueryVersionCookie(),
                                 QueryVersionReply)

    def QueryVersionUnchecked(self, client_major_version, client_minor_version):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', client_major_version, client_minor_version))
        return self.send_request(xcb.Request(buf.getvalue(), 0, False, False),
                                 QueryVersionCookie(),
                                 QueryVersionReply)

    def ChangeSaveSetChecked(self, mode, target, map, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBBBxI', mode, target, map, window))
        return self.send_request(xcb.Request(buf.getvalue(), 1, True, True),
                                 xcb.VoidCookie())

    def ChangeSaveSet(self, mode, target, map, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBBBxI', mode, target, map, window))
        return self.send_request(xcb.Request(buf.getvalue(), 1, True, False),
                                 xcb.VoidCookie())

    def SelectSelectionInputChecked(self, window, selection, event_mask):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', window, selection, event_mask))
        return self.send_request(xcb.Request(buf.getvalue(), 2, True, True),
                                 xcb.VoidCookie())

    def SelectSelectionInput(self, window, selection, event_mask):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', window, selection, event_mask))
        return self.send_request(xcb.Request(buf.getvalue(), 2, True, False),
                                 xcb.VoidCookie())

    def SelectCursorInputChecked(self, window, event_mask):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', window, event_mask))
        return self.send_request(xcb.Request(buf.getvalue(), 3, True, True),
                                 xcb.VoidCookie())

    def SelectCursorInput(self, window, event_mask):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', window, event_mask))
        return self.send_request(xcb.Request(buf.getvalue(), 3, True, False),
                                 xcb.VoidCookie())

    def GetCursorImage(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 4, False, True),
                                 GetCursorImageCookie(),
                                 GetCursorImageReply)

    def GetCursorImageUnchecked(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 4, False, False),
                                 GetCursorImageCookie(),
                                 GetCursorImageReply)

    def CreateRegionChecked(self, region, rectangles_len, rectangles):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', region))
        for elt in xcb.Iterator(rectangles, 4, 'rectangles', True):
            buf.write(pack('hhHH', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 5, True, True),
                                 xcb.VoidCookie())

    def CreateRegion(self, region, rectangles_len, rectangles):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', region))
        for elt in xcb.Iterator(rectangles, 4, 'rectangles', True):
            buf.write(pack('hhHH', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 5, True, False),
                                 xcb.VoidCookie())

    def CreateRegionFromBitmapChecked(self, region, bitmap):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', region, bitmap))
        return self.send_request(xcb.Request(buf.getvalue(), 6, True, True),
                                 xcb.VoidCookie())

    def CreateRegionFromBitmap(self, region, bitmap):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', region, bitmap))
        return self.send_request(xcb.Request(buf.getvalue(), 6, True, False),
                                 xcb.VoidCookie())

    def CreateRegionFromWindowChecked(self, region, window, kind):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIB3x', region, window, kind))
        return self.send_request(xcb.Request(buf.getvalue(), 7, True, True),
                                 xcb.VoidCookie())

    def CreateRegionFromWindow(self, region, window, kind):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIB3x', region, window, kind))
        return self.send_request(xcb.Request(buf.getvalue(), 7, True, False),
                                 xcb.VoidCookie())

    def CreateRegionFromGCChecked(self, region, gc):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', region, gc))
        return self.send_request(xcb.Request(buf.getvalue(), 8, True, True),
                                 xcb.VoidCookie())

    def CreateRegionFromGC(self, region, gc):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', region, gc))
        return self.send_request(xcb.Request(buf.getvalue(), 8, True, False),
                                 xcb.VoidCookie())

    def CreateRegionFromPictureChecked(self, region, picture):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', region, picture))
        return self.send_request(xcb.Request(buf.getvalue(), 9, True, True),
                                 xcb.VoidCookie())

    def CreateRegionFromPicture(self, region, picture):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', region, picture))
        return self.send_request(xcb.Request(buf.getvalue(), 9, True, False),
                                 xcb.VoidCookie())

    def DestroyRegionChecked(self, region):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', region))
        return self.send_request(xcb.Request(buf.getvalue(), 10, True, True),
                                 xcb.VoidCookie())

    def DestroyRegion(self, region):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', region))
        return self.send_request(xcb.Request(buf.getvalue(), 10, True, False),
                                 xcb.VoidCookie())

    def SetRegionChecked(self, region, rectangles_len, rectangles):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', region))
        for elt in xcb.Iterator(rectangles, 4, 'rectangles', True):
            buf.write(pack('hhHH', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 11, True, True),
                                 xcb.VoidCookie())

    def SetRegion(self, region, rectangles_len, rectangles):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', region))
        for elt in xcb.Iterator(rectangles, 4, 'rectangles', True):
            buf.write(pack('hhHH', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 11, True, False),
                                 xcb.VoidCookie())

    def CopyRegionChecked(self, source, destination):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', source, destination))
        return self.send_request(xcb.Request(buf.getvalue(), 12, True, True),
                                 xcb.VoidCookie())

    def CopyRegion(self, source, destination):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', source, destination))
        return self.send_request(xcb.Request(buf.getvalue(), 12, True, False),
                                 xcb.VoidCookie())

    def UnionRegionChecked(self, source1, source2, destination):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', source1, source2, destination))
        return self.send_request(xcb.Request(buf.getvalue(), 13, True, True),
                                 xcb.VoidCookie())

    def UnionRegion(self, source1, source2, destination):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', source1, source2, destination))
        return self.send_request(xcb.Request(buf.getvalue(), 13, True, False),
                                 xcb.VoidCookie())

    def IntersectRegionChecked(self, source1, source2, destination):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', source1, source2, destination))
        return self.send_request(xcb.Request(buf.getvalue(), 14, True, True),
                                 xcb.VoidCookie())

    def IntersectRegion(self, source1, source2, destination):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', source1, source2, destination))
        return self.send_request(xcb.Request(buf.getvalue(), 14, True, False),
                                 xcb.VoidCookie())

    def SubtractRegionChecked(self, source1, source2, destination):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', source1, source2, destination))
        return self.send_request(xcb.Request(buf.getvalue(), 15, True, True),
                                 xcb.VoidCookie())

    def SubtractRegion(self, source1, source2, destination):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', source1, source2, destination))
        return self.send_request(xcb.Request(buf.getvalue(), 15, True, False),
                                 xcb.VoidCookie())

    def InvertRegionChecked(self, source, bounds, destination):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', source))
        for elt in xcb.Iterator(bounds, 4, 'bounds', False):
            buf.write(pack('hhHH', *elt))
        buf.write(pack('I', destination))
        return self.send_request(xcb.Request(buf.getvalue(), 16, True, True),
                                 xcb.VoidCookie())

    def InvertRegion(self, source, bounds, destination):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', source))
        for elt in xcb.Iterator(bounds, 4, 'bounds', False):
            buf.write(pack('hhHH', *elt))
        buf.write(pack('I', destination))
        return self.send_request(xcb.Request(buf.getvalue(), 16, True, False),
                                 xcb.VoidCookie())

    def TranslateRegionChecked(self, region, dx, dy):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIhh', region, dx, dy))
        return self.send_request(xcb.Request(buf.getvalue(), 17, True, True),
                                 xcb.VoidCookie())

    def TranslateRegion(self, region, dx, dy):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIhh', region, dx, dy))
        return self.send_request(xcb.Request(buf.getvalue(), 17, True, False),
                                 xcb.VoidCookie())

    def RegionExtentsChecked(self, source, destination):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', source, destination))
        return self.send_request(xcb.Request(buf.getvalue(), 18, True, True),
                                 xcb.VoidCookie())

    def RegionExtents(self, source, destination):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', source, destination))
        return self.send_request(xcb.Request(buf.getvalue(), 18, True, False),
                                 xcb.VoidCookie())

    def FetchRegion(self, region):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', region))
        return self.send_request(xcb.Request(buf.getvalue(), 19, False, True),
                                 FetchRegionCookie(),
                                 FetchRegionReply)

    def FetchRegionUnchecked(self, region):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', region))
        return self.send_request(xcb.Request(buf.getvalue(), 19, False, False),
                                 FetchRegionCookie(),
                                 FetchRegionReply)

    def SetGCClipRegionChecked(self, gc, region, x_origin, y_origin):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIhh', gc, region, x_origin, y_origin))
        return self.send_request(xcb.Request(buf.getvalue(), 20, True, True),
                                 xcb.VoidCookie())

    def SetGCClipRegion(self, gc, region, x_origin, y_origin):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIhh', gc, region, x_origin, y_origin))
        return self.send_request(xcb.Request(buf.getvalue(), 20, True, False),
                                 xcb.VoidCookie())

    def SetWindowShapeRegionChecked(self, dest, dest_kind, x_offset, y_offset, region):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIB3xhhI', dest, dest_kind, x_offset, y_offset, region))
        return self.send_request(xcb.Request(buf.getvalue(), 21, True, True),
                                 xcb.VoidCookie())

    def SetWindowShapeRegion(self, dest, dest_kind, x_offset, y_offset, region):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIB3xhhI', dest, dest_kind, x_offset, y_offset, region))
        return self.send_request(xcb.Request(buf.getvalue(), 21, True, False),
                                 xcb.VoidCookie())

    def SetPictureClipRegionChecked(self, picture, region, x_origin, y_origin):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIhh', picture, region, x_origin, y_origin))
        return self.send_request(xcb.Request(buf.getvalue(), 22, True, True),
                                 xcb.VoidCookie())

    def SetPictureClipRegion(self, picture, region, x_origin, y_origin):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIhh', picture, region, x_origin, y_origin))
        return self.send_request(xcb.Request(buf.getvalue(), 22, True, False),
                                 xcb.VoidCookie())

    def SetCursorNameChecked(self, cursor, nbytes, name):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIH2x', cursor, nbytes))
        buf.write(str(buffer(array('b', name))))
        return self.send_request(xcb.Request(buf.getvalue(), 23, True, True),
                                 xcb.VoidCookie())

    def SetCursorName(self, cursor, nbytes, name):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIH2x', cursor, nbytes))
        buf.write(str(buffer(array('b', name))))
        return self.send_request(xcb.Request(buf.getvalue(), 23, True, False),
                                 xcb.VoidCookie())

    def GetCursorName(self, cursor):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', cursor))
        return self.send_request(xcb.Request(buf.getvalue(), 24, False, True),
                                 GetCursorNameCookie(),
                                 GetCursorNameReply)

    def GetCursorNameUnchecked(self, cursor):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', cursor))
        return self.send_request(xcb.Request(buf.getvalue(), 24, False, False),
                                 GetCursorNameCookie(),
                                 GetCursorNameReply)

    def GetCursorImageAndName(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 25, False, True),
                                 GetCursorImageAndNameCookie(),
                                 GetCursorImageAndNameReply)

    def GetCursorImageAndNameUnchecked(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 25, False, False),
                                 GetCursorImageAndNameCookie(),
                                 GetCursorImageAndNameReply)

    def ChangeCursorChecked(self, source, destination):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', source, destination))
        return self.send_request(xcb.Request(buf.getvalue(), 26, True, True),
                                 xcb.VoidCookie())

    def ChangeCursor(self, source, destination):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', source, destination))
        return self.send_request(xcb.Request(buf.getvalue(), 26, True, False),
                                 xcb.VoidCookie())

    def ChangeCursorByNameChecked(self, src, nbytes, name):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIH2x', src, nbytes))
        buf.write(str(buffer(array('b', name))))
        return self.send_request(xcb.Request(buf.getvalue(), 27, True, True),
                                 xcb.VoidCookie())

    def ChangeCursorByName(self, src, nbytes, name):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIH2x', src, nbytes))
        buf.write(str(buffer(array('b', name))))
        return self.send_request(xcb.Request(buf.getvalue(), 27, True, False),
                                 xcb.VoidCookie())

    def ExpandRegionChecked(self, source, destination, left, right, top, bottom):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIHHHH', source, destination, left, right, top, bottom))
        return self.send_request(xcb.Request(buf.getvalue(), 28, True, True),
                                 xcb.VoidCookie())

    def ExpandRegion(self, source, destination, left, right, top, bottom):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIHHHH', source, destination, left, right, top, bottom))
        return self.send_request(xcb.Request(buf.getvalue(), 28, True, False),
                                 xcb.VoidCookie())

    def HideCursorChecked(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 29, True, True),
                                 xcb.VoidCookie())

    def HideCursor(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 29, True, False),
                                 xcb.VoidCookie())

    def ShowCursorChecked(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 30, True, True),
                                 xcb.VoidCookie())

    def ShowCursor(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 30, True, False),
                                 xcb.VoidCookie())

_events = {
    0 : SelectionNotifyEvent,
    1 : CursorNotifyEvent,
}

_errors = {
    0 : (RegionError, BadRegion),
}

xcb._add_ext(key, xfixesExtension, _events, _errors)
