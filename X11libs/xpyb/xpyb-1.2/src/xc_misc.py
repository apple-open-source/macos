#
# This file generated automatically from xc_misc.xml by py_client.py.
# Edit at your peril.
#

import xcb
import cStringIO
from struct import pack, unpack_from
from array import array

MAJOR_VERSION = 1
MINOR_VERSION = 1

key = xcb.ExtensionKey('XC-MISC')

class GetVersionCookie(xcb.Cookie):
    pass

class GetVersionReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.server_major_version, self.server_minor_version,) = unpack_from('xx2x4xHH', self, count)

class GetXIDRangeCookie(xcb.Cookie):
    pass

class GetXIDRangeReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.start_id, self.count,) = unpack_from('xx2x4xII', self, count)

class GetXIDListCookie(xcb.Cookie):
    pass

class GetXIDListReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.ids_len,) = unpack_from('xx2x4xI20x', self, count)
        count += 32
        self.ids = xcb.List(self, count, self.ids_len, 'I', 4)

class xc_miscExtension(xcb.Extension):

    def GetVersion(self, client_major_version, client_minor_version):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xHH', client_major_version, client_minor_version))
        return self.send_request(xcb.Request(buf.getvalue(), 0, False, True),
                                 GetVersionCookie(),
                                 GetVersionReply)

    def GetVersionUnchecked(self, client_major_version, client_minor_version):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xHH', client_major_version, client_minor_version))
        return self.send_request(xcb.Request(buf.getvalue(), 0, False, False),
                                 GetVersionCookie(),
                                 GetVersionReply)

    def GetXIDRange(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 1, False, True),
                                 GetXIDRangeCookie(),
                                 GetXIDRangeReply)

    def GetXIDRangeUnchecked(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 1, False, False),
                                 GetXIDRangeCookie(),
                                 GetXIDRangeReply)

    def GetXIDList(self, count):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', count))
        return self.send_request(xcb.Request(buf.getvalue(), 2, False, True),
                                 GetXIDListCookie(),
                                 GetXIDListReply)

    def GetXIDListUnchecked(self, count):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', count))
        return self.send_request(xcb.Request(buf.getvalue(), 2, False, False),
                                 GetXIDListCookie(),
                                 GetXIDListReply)

_events = {
}

_errors = {
}

xcb._add_ext(key, xc_miscExtension, _events, _errors)
