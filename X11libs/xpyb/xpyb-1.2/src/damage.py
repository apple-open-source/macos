#
# This file generated automatically from damage.xml by py_client.py.
# Edit at your peril.
#

import xcb
import cStringIO
from struct import pack, unpack_from
from array import array
import xproto
import render
import shape
import xfixes

MAJOR_VERSION = 1
MINOR_VERSION = 1

key = xcb.ExtensionKey('DAMAGE')

class ReportLevel:
    RawRectangles = 0
    DeltaRectangles = 1
    BoundingBox = 2
    NonEmpty = 3

class DamageError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)

class BadDamage(xcb.ProtocolException):
    pass

class QueryVersionCookie(xcb.Cookie):
    pass

class QueryVersionReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.major_version, self.minor_version,) = unpack_from('xx2x4xII16x', self, count)

class NotifyEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.level, self.drawable, self.damage, self.timestamp,) = unpack_from('xB2xIII', self, count)
        count += 16
        self.area = RECTANGLE(self, count, 8)
        count += 8
        count += xcb.type_pad(8, count)
        self.geometry = RECTANGLE(self, count, 8)

class damageExtension(xcb.Extension):

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

    def CreateChecked(self, damage, drawable, level):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIB3x', damage, drawable, level))
        return self.send_request(xcb.Request(buf.getvalue(), 1, True, True),
                                 xcb.VoidCookie())

    def Create(self, damage, drawable, level):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIB3x', damage, drawable, level))
        return self.send_request(xcb.Request(buf.getvalue(), 1, True, False),
                                 xcb.VoidCookie())

    def DestroyChecked(self, damage):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', damage))
        return self.send_request(xcb.Request(buf.getvalue(), 2, True, True),
                                 xcb.VoidCookie())

    def Destroy(self, damage):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', damage))
        return self.send_request(xcb.Request(buf.getvalue(), 2, True, False),
                                 xcb.VoidCookie())

    def SubtractChecked(self, damage, repair, parts):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', damage, repair, parts))
        return self.send_request(xcb.Request(buf.getvalue(), 3, True, True),
                                 xcb.VoidCookie())

    def Subtract(self, damage, repair, parts):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', damage, repair, parts))
        return self.send_request(xcb.Request(buf.getvalue(), 3, True, False),
                                 xcb.VoidCookie())

    def AddChecked(self, drawable, region):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', drawable, region))
        return self.send_request(xcb.Request(buf.getvalue(), 4, True, True),
                                 xcb.VoidCookie())

    def Add(self, drawable, region):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', drawable, region))
        return self.send_request(xcb.Request(buf.getvalue(), 4, True, False),
                                 xcb.VoidCookie())

_events = {
    0 : NotifyEvent,
}

_errors = {
    0 : (DamageError, BadDamage),
}

xcb._add_ext(key, damageExtension, _events, _errors)
