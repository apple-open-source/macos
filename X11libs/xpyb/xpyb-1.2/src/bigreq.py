#
# This file generated automatically from bigreq.xml by py_client.py.
# Edit at your peril.
#

import xcb
import cStringIO
from struct import pack, unpack_from
from array import array

MAJOR_VERSION = 0
MINOR_VERSION = 0

key = xcb.ExtensionKey('BIG-REQUESTS')

class EnableCookie(xcb.Cookie):
    pass

class EnableReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.maximum_request_length,) = unpack_from('xx2x4xI', self, count)

class bigreqExtension(xcb.Extension):

    def Enable(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 0, False, True),
                                 EnableCookie(),
                                 EnableReply)

    def EnableUnchecked(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 0, False, False),
                                 EnableCookie(),
                                 EnableReply)

_events = {
}

_errors = {
}

xcb._add_ext(key, bigreqExtension, _events, _errors)
