#
# This file generated automatically from composite.xml by py_client.py.
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

MAJOR_VERSION = 0
MINOR_VERSION = 3

key = xcb.ExtensionKey('Composite')

class Redirect:
    Automatic = 0
    Manual = 1

class QueryVersionCookie(xcb.Cookie):
    pass

class QueryVersionReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.major_version, self.minor_version,) = unpack_from('xx2x4xII16x', self, count)

class GetOverlayWindowCookie(xcb.Cookie):
    pass

class GetOverlayWindowReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.overlay_win,) = unpack_from('xx2x4xI20x', self, count)

class compositeExtension(xcb.Extension):

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

    def RedirectWindowChecked(self, window, update):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIB3x', window, update))
        return self.send_request(xcb.Request(buf.getvalue(), 1, True, True),
                                 xcb.VoidCookie())

    def RedirectWindow(self, window, update):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIB3x', window, update))
        return self.send_request(xcb.Request(buf.getvalue(), 1, True, False),
                                 xcb.VoidCookie())

    def RedirectSubwindowsChecked(self, window, update):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIB3x', window, update))
        return self.send_request(xcb.Request(buf.getvalue(), 2, True, True),
                                 xcb.VoidCookie())

    def RedirectSubwindows(self, window, update):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIB3x', window, update))
        return self.send_request(xcb.Request(buf.getvalue(), 2, True, False),
                                 xcb.VoidCookie())

    def UnredirectWindowChecked(self, window, update):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIB3x', window, update))
        return self.send_request(xcb.Request(buf.getvalue(), 3, True, True),
                                 xcb.VoidCookie())

    def UnredirectWindow(self, window, update):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIB3x', window, update))
        return self.send_request(xcb.Request(buf.getvalue(), 3, True, False),
                                 xcb.VoidCookie())

    def UnredirectSubwindowsChecked(self, window, update):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIB3x', window, update))
        return self.send_request(xcb.Request(buf.getvalue(), 4, True, True),
                                 xcb.VoidCookie())

    def UnredirectSubwindows(self, window, update):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIB3x', window, update))
        return self.send_request(xcb.Request(buf.getvalue(), 4, True, False),
                                 xcb.VoidCookie())

    def CreateRegionFromBorderClipChecked(self, region, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', region, window))
        return self.send_request(xcb.Request(buf.getvalue(), 5, True, True),
                                 xcb.VoidCookie())

    def CreateRegionFromBorderClip(self, region, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', region, window))
        return self.send_request(xcb.Request(buf.getvalue(), 5, True, False),
                                 xcb.VoidCookie())

    def NameWindowPixmapChecked(self, window, pixmap):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', window, pixmap))
        return self.send_request(xcb.Request(buf.getvalue(), 6, True, True),
                                 xcb.VoidCookie())

    def NameWindowPixmap(self, window, pixmap):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', window, pixmap))
        return self.send_request(xcb.Request(buf.getvalue(), 6, True, False),
                                 xcb.VoidCookie())

    def GetOverlayWindow(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 7, False, True),
                                 GetOverlayWindowCookie(),
                                 GetOverlayWindowReply)

    def GetOverlayWindowUnchecked(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 7, False, False),
                                 GetOverlayWindowCookie(),
                                 GetOverlayWindowReply)

    def ReleaseOverlayWindowChecked(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 8, True, True),
                                 xcb.VoidCookie())

    def ReleaseOverlayWindow(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 8, True, False),
                                 xcb.VoidCookie())

_events = {
}

_errors = {
}

xcb._add_ext(key, compositeExtension, _events, _errors)
