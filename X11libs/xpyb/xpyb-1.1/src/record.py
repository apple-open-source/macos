#
# This file generated automatically from record.xml by py_client.py.
# Edit at your peril.
#

import xcb
import cStringIO
from struct import pack, unpack_from
from array import array

MAJOR_VERSION = 1
MINOR_VERSION = 13

key = xcb.ExtensionKey('RECORD')

class Range8(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.first, self.last,) = unpack_from('BB', self, count)

class Range16(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.first, self.last,) = unpack_from('HH', self, count)

class ExtRange(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        self.major = Range8(self, count, 2)
        count += 2
        count += xcb.type_pad(4, count)
        self.minor = Range16(self, count, 4)

class Range(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        self.core_requests = Range8(self, count, 2)
        count += 2
        count += xcb.type_pad(2, count)
        self.core_replies = Range8(self, count, 2)
        count += 2
        count += xcb.type_pad(6, count)
        self.ext_requests = ExtRange(self, count, 6)
        count += 6
        count += xcb.type_pad(6, count)
        self.ext_replies = ExtRange(self, count, 6)
        count += 6
        count += xcb.type_pad(2, count)
        self.delivered_events = Range8(self, count, 2)
        count += 2
        count += xcb.type_pad(2, count)
        self.device_events = Range8(self, count, 2)
        count += 2
        count += xcb.type_pad(2, count)
        self.errors = Range8(self, count, 2)
        count += 2
        count += xcb.type_pad(4, count)
        (self.client_started, self.client_died,) = unpack_from('BB', self, count)

class HType:
    FromServerTime = 1
    FromClientTime = 2
    FromClientSequence = 4

class CS:
    CurrentClients = 1
    FutureClients = 2
    AllClients = 3

class ClientInfo(xcb.Struct):
    def __init__(self, parent, offset):
        xcb.Struct.__init__(self, parent, offset)
        count = 0
        (self.client_resource, self.num_ranges,) = unpack_from('II', self, count)
        count += 8
        self.ranges = xcb.List(self, count, self.num_ranges, Range, 24)
        count += len(self.ranges.buf())
        xcb._resize_obj(self, count)

class ContextError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)
        count = 0
        (self.invalid_record,) = unpack_from('xx2xI', self, count)

class BadContext(xcb.ProtocolException):
    pass

class QueryVersionCookie(xcb.Cookie):
    pass

class QueryVersionReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.major_version, self.minor_version,) = unpack_from('xx2x4xHH', self, count)

class GetContextCookie(xcb.Cookie):
    pass

class GetContextReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.enabled, self.element_header, self.num_intercepted_clients,) = unpack_from('xB2x4xB3xI16x', self, count)
        count += 32
        self.intercepted_clients = xcb.List(self, count, self.num_intercepted_clients, ClientInfo, -1)

class EnableContextCookie(xcb.Cookie):
    pass

class EnableContextReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.category, self.element_header, self.client_swapped, self.xid_base, self.server_time, self.rec_sequence_num,) = unpack_from('xB2x4xBB2xIII8x', self, count)
        count += 32
        self.data = xcb.List(self, count, (self.length * 4), 'B', 1)

class recordExtension(xcb.Extension):

    def QueryVersion(self, major_version, minor_version):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xHH', major_version, minor_version))
        return self.send_request(xcb.Request(buf.getvalue(), 0, False, True),
                                 QueryVersionCookie(),
                                 QueryVersionReply)

    def QueryVersionUnchecked(self, major_version, minor_version):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xHH', major_version, minor_version))
        return self.send_request(xcb.Request(buf.getvalue(), 0, False, False),
                                 QueryVersionCookie(),
                                 QueryVersionReply)

    def CreateContextChecked(self, context, element_header, num_client_specs, num_ranges, client_specs, ranges):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIB3xII', context, element_header, num_client_specs, num_ranges))
        buf.write(str(buffer(array('I', client_specs))))
        for elt in xcb.Iterator(ranges, 20, 'ranges', True):
            buf.write(pack('BBBBBBHHBBHHBBBBBBBB', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 1, True, True),
                                 xcb.VoidCookie())

    def CreateContext(self, context, element_header, num_client_specs, num_ranges, client_specs, ranges):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIB3xII', context, element_header, num_client_specs, num_ranges))
        buf.write(str(buffer(array('I', client_specs))))
        for elt in xcb.Iterator(ranges, 20, 'ranges', True):
            buf.write(pack('BBBBBBHHBBHHBBBBBBBB', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 1, True, False),
                                 xcb.VoidCookie())

    def RegisterClientsChecked(self, context, element_header, num_client_specs, num_ranges, client_specs, ranges):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIB3xII', context, element_header, num_client_specs, num_ranges))
        buf.write(str(buffer(array('I', client_specs))))
        for elt in xcb.Iterator(ranges, 20, 'ranges', True):
            buf.write(pack('BBBBBBHHBBHHBBBBBBBB', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 2, True, True),
                                 xcb.VoidCookie())

    def RegisterClients(self, context, element_header, num_client_specs, num_ranges, client_specs, ranges):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIB3xII', context, element_header, num_client_specs, num_ranges))
        buf.write(str(buffer(array('I', client_specs))))
        for elt in xcb.Iterator(ranges, 20, 'ranges', True):
            buf.write(pack('BBBBBBHHBBHHBBBBBBBB', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 2, True, False),
                                 xcb.VoidCookie())

    def UnregisterClientsChecked(self, context, num_client_specs, client_specs):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', context, num_client_specs))
        buf.write(str(buffer(array('I', client_specs))))
        return self.send_request(xcb.Request(buf.getvalue(), 3, True, True),
                                 xcb.VoidCookie())

    def UnregisterClients(self, context, num_client_specs, client_specs):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', context, num_client_specs))
        buf.write(str(buffer(array('I', client_specs))))
        return self.send_request(xcb.Request(buf.getvalue(), 3, True, False),
                                 xcb.VoidCookie())

    def GetContext(self, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context))
        return self.send_request(xcb.Request(buf.getvalue(), 4, False, True),
                                 GetContextCookie(),
                                 GetContextReply)

    def GetContextUnchecked(self, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context))
        return self.send_request(xcb.Request(buf.getvalue(), 4, False, False),
                                 GetContextCookie(),
                                 GetContextReply)

    def EnableContext(self, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context))
        return self.send_request(xcb.Request(buf.getvalue(), 5, False, True),
                                 EnableContextCookie(),
                                 EnableContextReply)

    def EnableContextUnchecked(self, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context))
        return self.send_request(xcb.Request(buf.getvalue(), 5, False, False),
                                 EnableContextCookie(),
                                 EnableContextReply)

    def DisableContextChecked(self, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context))
        return self.send_request(xcb.Request(buf.getvalue(), 6, True, True),
                                 xcb.VoidCookie())

    def DisableContext(self, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context))
        return self.send_request(xcb.Request(buf.getvalue(), 6, True, False),
                                 xcb.VoidCookie())

    def FreeContextChecked(self, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context))
        return self.send_request(xcb.Request(buf.getvalue(), 7, True, True),
                                 xcb.VoidCookie())

    def FreeContext(self, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context))
        return self.send_request(xcb.Request(buf.getvalue(), 7, True, False),
                                 xcb.VoidCookie())

_events = {
}

_errors = {
    0 : (ContextError, BadContext),
}

xcb._add_ext(key, recordExtension, _events, _errors)
