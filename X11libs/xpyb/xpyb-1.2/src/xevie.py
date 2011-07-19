#
# This file generated automatically from xevie.xml by py_client.py.
# Edit at your peril.
#

import xcb
import cStringIO
from struct import pack, unpack_from
from array import array

MAJOR_VERSION = 1
MINOR_VERSION = 0

key = xcb.ExtensionKey('XEVIE')

class QueryVersionCookie(xcb.Cookie):
    pass

class QueryVersionReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.server_major_version, self.server_minor_version,) = unpack_from('xx2x4xHH20x', self, count)

class StartCookie(xcb.Cookie):
    pass

class StartReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)

class EndCookie(xcb.Cookie):
    pass

class EndReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)

class Datatype:
    Unmodified = 0
    Modified = 1

class Event(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)

class SendCookie(xcb.Cookie):
    pass

class SendReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)

class SelectInputCookie(xcb.Cookie):
    pass

class SelectInputReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)

class xevieExtension(xcb.Extension):

    def QueryVersion(self, client_major_version, client_minor_version):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xHH', client_major_version, client_minor_version))
        return self.send_request(xcb.Request(buf.getvalue(), 0, False, True),
                                 QueryVersionCookie(),
                                 QueryVersionReply)

    def QueryVersionUnchecked(self, client_major_version, client_minor_version):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xHH', client_major_version, client_minor_version))
        return self.send_request(xcb.Request(buf.getvalue(), 0, False, False),
                                 QueryVersionCookie(),
                                 QueryVersionReply)

    def Start(self, screen):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', screen))
        return self.send_request(xcb.Request(buf.getvalue(), 1, False, True),
                                 StartCookie(),
                                 StartReply)

    def StartUnchecked(self, screen):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', screen))
        return self.send_request(xcb.Request(buf.getvalue(), 1, False, False),
                                 StartCookie(),
                                 StartReply)

    def End(self, cmap):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', cmap))
        return self.send_request(xcb.Request(buf.getvalue(), 2, False, True),
                                 EndCookie(),
                                 EndReply)

    def EndUnchecked(self, cmap):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', cmap))
        return self.send_request(xcb.Request(buf.getvalue(), 2, False, False),
                                 EndCookie(),
                                 EndReply)

    def Send(self, event, data_type):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        for elt in xcb.Iterator(event, 0, 'event', False):
            buf.write(pack('32x', *elt))
        buf.write(pack('I64x', data_type))
        return self.send_request(xcb.Request(buf.getvalue(), 3, False, True),
                                 SendCookie(),
                                 SendReply)

    def SendUnchecked(self, event, data_type):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        for elt in xcb.Iterator(event, 0, 'event', False):
            buf.write(pack('32x', *elt))
        buf.write(pack('I64x', data_type))
        return self.send_request(xcb.Request(buf.getvalue(), 3, False, False),
                                 SendCookie(),
                                 SendReply)

    def SelectInput(self, event_mask):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', event_mask))
        return self.send_request(xcb.Request(buf.getvalue(), 4, False, True),
                                 SelectInputCookie(),
                                 SelectInputReply)

    def SelectInputUnchecked(self, event_mask):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', event_mask))
        return self.send_request(xcb.Request(buf.getvalue(), 4, False, False),
                                 SelectInputCookie(),
                                 SelectInputReply)

_events = {
}

_errors = {
}

xcb._add_ext(key, xevieExtension, _events, _errors)
