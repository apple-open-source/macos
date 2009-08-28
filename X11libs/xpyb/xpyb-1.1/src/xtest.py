#
# This file generated automatically from xtest.xml by py_client.py.
# Edit at your peril.
#

import xcb
import cStringIO
from struct import pack, unpack_from
from array import array
import xproto

MAJOR_VERSION = 2
MINOR_VERSION = 1

key = xcb.ExtensionKey('XTEST')

class GetVersionCookie(xcb.Cookie):
    pass

class GetVersionReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.major_version, self.minor_version,) = unpack_from('xB2x4xH', self, count)

class Cursor:
    _None = 0
    Current = 1

class CompareCursorCookie(xcb.Cookie):
    pass

class CompareCursorReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.same,) = unpack_from('xB2x4x', self, count)

class xtestExtension(xcb.Extension):

    def GetVersion(self, major_version, minor_version):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBxH', major_version, minor_version))
        return self.send_request(xcb.Request(buf.getvalue(), 0, False, True),
                                 GetVersionCookie(),
                                 GetVersionReply)

    def GetVersionUnchecked(self, major_version, minor_version):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBxH', major_version, minor_version))
        return self.send_request(xcb.Request(buf.getvalue(), 0, False, False),
                                 GetVersionCookie(),
                                 GetVersionReply)

    def CompareCursor(self, window, cursor):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', window, cursor))
        return self.send_request(xcb.Request(buf.getvalue(), 1, False, True),
                                 CompareCursorCookie(),
                                 CompareCursorReply)

    def CompareCursorUnchecked(self, window, cursor):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', window, cursor))
        return self.send_request(xcb.Request(buf.getvalue(), 1, False, False),
                                 CompareCursorCookie(),
                                 CompareCursorReply)

    def FakeInputChecked(self, type, detail, time, window, rootX, rootY, deviceid):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBB2xII8xHH7xB', type, detail, time, window, rootX, rootY, deviceid))
        return self.send_request(xcb.Request(buf.getvalue(), 2, True, True),
                                 xcb.VoidCookie())

    def FakeInput(self, type, detail, time, window, rootX, rootY, deviceid):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBB2xII8xHH7xB', type, detail, time, window, rootX, rootY, deviceid))
        return self.send_request(xcb.Request(buf.getvalue(), 2, True, False),
                                 xcb.VoidCookie())

    def GrabControlChecked(self, impervious):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xB3x', impervious))
        return self.send_request(xcb.Request(buf.getvalue(), 3, True, True),
                                 xcb.VoidCookie())

    def GrabControl(self, impervious):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xB3x', impervious))
        return self.send_request(xcb.Request(buf.getvalue(), 3, True, False),
                                 xcb.VoidCookie())

_events = {
}

_errors = {
}

xcb._add_ext(key, xtestExtension, _events, _errors)
