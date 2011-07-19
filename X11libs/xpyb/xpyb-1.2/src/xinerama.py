#
# This file generated automatically from xinerama.xml by py_client.py.
# Edit at your peril.
#

import xcb
import cStringIO
from struct import pack, unpack_from
from array import array
import xproto

MAJOR_VERSION = 1
MINOR_VERSION = 1

key = xcb.ExtensionKey('XINERAMA')

class ScreenInfo(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.x_org, self.y_org, self.width, self.height,) = unpack_from('hhHH', self, count)

class QueryVersionCookie(xcb.Cookie):
    pass

class QueryVersionReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.major, self.minor,) = unpack_from('xx2x4xHH', self, count)

class GetStateCookie(xcb.Cookie):
    pass

class GetStateReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.state, self.window,) = unpack_from('xB2x4xI', self, count)

class GetScreenCountCookie(xcb.Cookie):
    pass

class GetScreenCountReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.screen_count, self.window,) = unpack_from('xB2x4xI', self, count)

class GetScreenSizeCookie(xcb.Cookie):
    pass

class GetScreenSizeReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.width, self.height, self.window, self.screen,) = unpack_from('xx2x4xIIII', self, count)

class IsActiveCookie(xcb.Cookie):
    pass

class IsActiveReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.state,) = unpack_from('xx2x4xI', self, count)

class QueryScreensCookie(xcb.Cookie):
    pass

class QueryScreensReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.number,) = unpack_from('xx2x4xI20x', self, count)
        count += 32
        self.screen_info = xcb.List(self, count, self.number, ScreenInfo, 8)

class xineramaExtension(xcb.Extension):

    def QueryVersion(self, major, minor):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBB', major, minor))
        return self.send_request(xcb.Request(buf.getvalue(), 0, False, True),
                                 QueryVersionCookie(),
                                 QueryVersionReply)

    def QueryVersionUnchecked(self, major, minor):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBB', major, minor))
        return self.send_request(xcb.Request(buf.getvalue(), 0, False, False),
                                 QueryVersionCookie(),
                                 QueryVersionReply)

    def GetState(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 1, False, True),
                                 GetStateCookie(),
                                 GetStateReply)

    def GetStateUnchecked(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 1, False, False),
                                 GetStateCookie(),
                                 GetStateReply)

    def GetScreenCount(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 2, False, True),
                                 GetScreenCountCookie(),
                                 GetScreenCountReply)

    def GetScreenCountUnchecked(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 2, False, False),
                                 GetScreenCountCookie(),
                                 GetScreenCountReply)

    def GetScreenSize(self, window, screen):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', window, screen))
        return self.send_request(xcb.Request(buf.getvalue(), 3, False, True),
                                 GetScreenSizeCookie(),
                                 GetScreenSizeReply)

    def GetScreenSizeUnchecked(self, window, screen):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', window, screen))
        return self.send_request(xcb.Request(buf.getvalue(), 3, False, False),
                                 GetScreenSizeCookie(),
                                 GetScreenSizeReply)

    def IsActive(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 4, False, True),
                                 IsActiveCookie(),
                                 IsActiveReply)

    def IsActiveUnchecked(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 4, False, False),
                                 IsActiveCookie(),
                                 IsActiveReply)

    def QueryScreens(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 5, False, True),
                                 QueryScreensCookie(),
                                 QueryScreensReply)

    def QueryScreensUnchecked(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 5, False, False),
                                 QueryScreensCookie(),
                                 QueryScreensReply)

_events = {
}

_errors = {
}

xcb._add_ext(key, xineramaExtension, _events, _errors)
