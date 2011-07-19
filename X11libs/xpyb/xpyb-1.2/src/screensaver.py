#
# This file generated automatically from screensaver.xml by py_client.py.
# Edit at your peril.
#

import xcb
import cStringIO
from struct import pack, unpack_from
from array import array
import xproto

MAJOR_VERSION = 1
MINOR_VERSION = 1

key = xcb.ExtensionKey('MIT-SCREEN-SAVER')

class Kind:
    Blanked = 0
    Internal = 1
    External = 2

class Event:
    NotifyMask = 1
    CycleMask = 2

class State:
    Off = 0
    On = 1
    Cycle = 2
    Disabled = 3

class QueryVersionCookie(xcb.Cookie):
    pass

class QueryVersionReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.server_major_version, self.server_minor_version,) = unpack_from('xx2x4xHH20x', self, count)

class QueryInfoCookie(xcb.Cookie):
    pass

class QueryInfoReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.state, self.saver_window, self.ms_until_server, self.ms_since_user_input, self.event_mask, self.kind,) = unpack_from('xB2x4xIIIIB7x', self, count)

class NotifyEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.code, self.state, self.sequence_number, self.time, self.root, self.window, self.kind, self.forced,) = unpack_from('xB2xBxHIIIBB14x', self, count)

class screensaverExtension(xcb.Extension):

    def QueryVersion(self, client_major_version, client_minor_version):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBB2x', client_major_version, client_minor_version))
        return self.send_request(xcb.Request(buf.getvalue(), 0, False, True),
                                 QueryVersionCookie(),
                                 QueryVersionReply)

    def QueryVersionUnchecked(self, client_major_version, client_minor_version):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBB2x', client_major_version, client_minor_version))
        return self.send_request(xcb.Request(buf.getvalue(), 0, False, False),
                                 QueryVersionCookie(),
                                 QueryVersionReply)

    def QueryInfo(self, drawable):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', drawable))
        return self.send_request(xcb.Request(buf.getvalue(), 1, False, True),
                                 QueryInfoCookie(),
                                 QueryInfoReply)

    def QueryInfoUnchecked(self, drawable):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', drawable))
        return self.send_request(xcb.Request(buf.getvalue(), 1, False, False),
                                 QueryInfoCookie(),
                                 QueryInfoReply)

    def SelectInputChecked(self, drawable, event_mask):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', drawable, event_mask))
        return self.send_request(xcb.Request(buf.getvalue(), 2, True, True),
                                 xcb.VoidCookie())

    def SelectInput(self, drawable, event_mask):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', drawable, event_mask))
        return self.send_request(xcb.Request(buf.getvalue(), 2, True, False),
                                 xcb.VoidCookie())

    def SetAttributesChecked(self, drawable, x, y, width, height, border_width, _class, depth, visual, value_mask, value_list):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIhhHHHBBII', drawable, x, y, width, height, border_width, _class, depth, visual, value_mask))
        buf.write(str(buffer(array('I', value_list))))
        return self.send_request(xcb.Request(buf.getvalue(), 3, True, True),
                                 xcb.VoidCookie())

    def SetAttributes(self, drawable, x, y, width, height, border_width, _class, depth, visual, value_mask, value_list):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIhhHHHBBII', drawable, x, y, width, height, border_width, _class, depth, visual, value_mask))
        buf.write(str(buffer(array('I', value_list))))
        return self.send_request(xcb.Request(buf.getvalue(), 3, True, False),
                                 xcb.VoidCookie())

    def UnsetAttributesChecked(self, drawable):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', drawable))
        return self.send_request(xcb.Request(buf.getvalue(), 4, True, True),
                                 xcb.VoidCookie())

    def UnsetAttributes(self, drawable):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', drawable))
        return self.send_request(xcb.Request(buf.getvalue(), 4, True, False),
                                 xcb.VoidCookie())

    def SuspendChecked(self, suspend):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xB3x', suspend))
        return self.send_request(xcb.Request(buf.getvalue(), 5, True, True),
                                 xcb.VoidCookie())

    def Suspend(self, suspend):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xB3x', suspend))
        return self.send_request(xcb.Request(buf.getvalue(), 5, True, False),
                                 xcb.VoidCookie())

_events = {
    0 : NotifyEvent,
}

_errors = {
}

xcb._add_ext(key, screensaverExtension, _events, _errors)
