#
# This file generated automatically from res.xml by py_client.py.
# Edit at your peril.
#

import xcb
import cStringIO
from struct import pack, unpack_from
from array import array
import xproto

MAJOR_VERSION = 1
MINOR_VERSION = 0

key = xcb.ExtensionKey('X-Resource')

class Client(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.resource_base, self.resource_mask,) = unpack_from('II', self, count)

class Type(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.resource_type, self.count,) = unpack_from('II', self, count)

class QueryVersionCookie(xcb.Cookie):
    pass

class QueryVersionReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.server_major, self.server_minor,) = unpack_from('xx2x4xHH', self, count)

class QueryClientsCookie(xcb.Cookie):
    pass

class QueryClientsReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.num_clients,) = unpack_from('xx2x4xI20x', self, count)
        count += 32
        self.clients = xcb.List(self, count, self.num_clients, Client, 8)

class QueryClientResourcesCookie(xcb.Cookie):
    pass

class QueryClientResourcesReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.num_types,) = unpack_from('xx2x4xI20x', self, count)
        count += 32
        self.types = xcb.List(self, count, self.num_types, Type, 8)

class QueryClientPixmapBytesCookie(xcb.Cookie):
    pass

class QueryClientPixmapBytesReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.bytes, self.bytes_overflow,) = unpack_from('xx2x4xII', self, count)

class resExtension(xcb.Extension):

    def QueryVersion(self, client_major, client_minor):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBB', client_major, client_minor))
        return self.send_request(xcb.Request(buf.getvalue(), 0, False, True),
                                 QueryVersionCookie(),
                                 QueryVersionReply)

    def QueryVersionUnchecked(self, client_major, client_minor):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBB', client_major, client_minor))
        return self.send_request(xcb.Request(buf.getvalue(), 0, False, False),
                                 QueryVersionCookie(),
                                 QueryVersionReply)

    def QueryClients(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 1, False, True),
                                 QueryClientsCookie(),
                                 QueryClientsReply)

    def QueryClientsUnchecked(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 1, False, False),
                                 QueryClientsCookie(),
                                 QueryClientsReply)

    def QueryClientResources(self, xid):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', xid))
        return self.send_request(xcb.Request(buf.getvalue(), 2, False, True),
                                 QueryClientResourcesCookie(),
                                 QueryClientResourcesReply)

    def QueryClientResourcesUnchecked(self, xid):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', xid))
        return self.send_request(xcb.Request(buf.getvalue(), 2, False, False),
                                 QueryClientResourcesCookie(),
                                 QueryClientResourcesReply)

    def QueryClientPixmapBytes(self, xid):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', xid))
        return self.send_request(xcb.Request(buf.getvalue(), 3, False, True),
                                 QueryClientPixmapBytesCookie(),
                                 QueryClientPixmapBytesReply)

    def QueryClientPixmapBytesUnchecked(self, xid):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', xid))
        return self.send_request(xcb.Request(buf.getvalue(), 3, False, False),
                                 QueryClientPixmapBytesCookie(),
                                 QueryClientPixmapBytesReply)

_events = {
}

_errors = {
}

xcb._add_ext(key, resExtension, _events, _errors)
