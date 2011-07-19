#
# This file generated automatically from xselinux.xml by py_client.py.
# Edit at your peril.
#

import xcb
import cStringIO
from struct import pack, unpack_from
from array import array
import xproto

MAJOR_VERSION = 1
MINOR_VERSION = 0

key = xcb.ExtensionKey('SELinux')

class QueryVersionCookie(xcb.Cookie):
    pass

class QueryVersionReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.server_major, self.server_minor,) = unpack_from('xx2x4xHH', self, count)

class GetDeviceCreateContextCookie(xcb.Cookie):
    pass

class GetDeviceCreateContextReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.context_len,) = unpack_from('xx2x4xI20x', self, count)
        count += 32
        self.context = xcb.List(self, count, self.context_len, 'b', 1)

class GetDeviceContextCookie(xcb.Cookie):
    pass

class GetDeviceContextReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.context_len,) = unpack_from('xx2x4xI20x', self, count)
        count += 32
        self.context = xcb.List(self, count, self.context_len, 'b', 1)

class GetWindowCreateContextCookie(xcb.Cookie):
    pass

class GetWindowCreateContextReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.context_len,) = unpack_from('xx2x4xI20x', self, count)
        count += 32
        self.context = xcb.List(self, count, self.context_len, 'b', 1)

class GetWindowContextCookie(xcb.Cookie):
    pass

class GetWindowContextReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.context_len,) = unpack_from('xx2x4xI20x', self, count)
        count += 32
        self.context = xcb.List(self, count, self.context_len, 'b', 1)

class ListItem(xcb.Struct):
    def __init__(self, parent, offset):
        xcb.Struct.__init__(self, parent, offset)
        count = 0
        (self.name, self.object_context_len, self.data_context_len,) = unpack_from('III', self, count)
        count += 12
        self.object_context = xcb.List(self, count, self.object_context_len, 'b', 1)
        count += len(self.object_context.buf())
        count += xcb.type_pad(1, count)
        self.data_context = xcb.List(self, count, self.data_context_len, 'b', 1)
        count += len(self.data_context.buf())
        xcb._resize_obj(self, count)

class GetPropertyCreateContextCookie(xcb.Cookie):
    pass

class GetPropertyCreateContextReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.context_len,) = unpack_from('xx2x4xI20x', self, count)
        count += 32
        self.context = xcb.List(self, count, self.context_len, 'b', 1)

class GetPropertyUseContextCookie(xcb.Cookie):
    pass

class GetPropertyUseContextReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.context_len,) = unpack_from('xx2x4xI20x', self, count)
        count += 32
        self.context = xcb.List(self, count, self.context_len, 'b', 1)

class GetPropertyContextCookie(xcb.Cookie):
    pass

class GetPropertyContextReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.context_len,) = unpack_from('xx2x4xI20x', self, count)
        count += 32
        self.context = xcb.List(self, count, self.context_len, 'b', 1)

class GetPropertyDataContextCookie(xcb.Cookie):
    pass

class GetPropertyDataContextReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.context_len,) = unpack_from('xx2x4xI20x', self, count)
        count += 32
        self.context = xcb.List(self, count, self.context_len, 'b', 1)

class ListPropertiesCookie(xcb.Cookie):
    pass

class ListPropertiesReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.properties_len,) = unpack_from('xx2x4xI20x', self, count)
        count += 32
        self.properties = xcb.List(self, count, self.properties_len, ListItem, -1)

class GetSelectionCreateContextCookie(xcb.Cookie):
    pass

class GetSelectionCreateContextReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.context_len,) = unpack_from('xx2x4xI20x', self, count)
        count += 32
        self.context = xcb.List(self, count, self.context_len, 'b', 1)

class GetSelectionUseContextCookie(xcb.Cookie):
    pass

class GetSelectionUseContextReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.context_len,) = unpack_from('xx2x4xI20x', self, count)
        count += 32
        self.context = xcb.List(self, count, self.context_len, 'b', 1)

class GetSelectionContextCookie(xcb.Cookie):
    pass

class GetSelectionContextReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.context_len,) = unpack_from('xx2x4xI20x', self, count)
        count += 32
        self.context = xcb.List(self, count, self.context_len, 'b', 1)

class GetSelectionDataContextCookie(xcb.Cookie):
    pass

class GetSelectionDataContextReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.context_len,) = unpack_from('xx2x4xI20x', self, count)
        count += 32
        self.context = xcb.List(self, count, self.context_len, 'b', 1)

class ListSelectionsCookie(xcb.Cookie):
    pass

class ListSelectionsReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.selections_len,) = unpack_from('xx2x4xI20x', self, count)
        count += 32
        self.selections = xcb.List(self, count, self.selections_len, ListItem, -1)

class GetClientContextCookie(xcb.Cookie):
    pass

class GetClientContextReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.context_len,) = unpack_from('xx2x4xI20x', self, count)
        count += 32
        self.context = xcb.List(self, count, self.context_len, 'b', 1)

class xselinuxExtension(xcb.Extension):

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

    def SetDeviceCreateContextChecked(self, context_len, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context_len))
        buf.write(str(buffer(array('b', context))))
        return self.send_request(xcb.Request(buf.getvalue(), 1, True, True),
                                 xcb.VoidCookie())

    def SetDeviceCreateContext(self, context_len, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context_len))
        buf.write(str(buffer(array('b', context))))
        return self.send_request(xcb.Request(buf.getvalue(), 1, True, False),
                                 xcb.VoidCookie())

    def GetDeviceCreateContext(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 2, False, True),
                                 GetDeviceCreateContextCookie(),
                                 GetDeviceCreateContextReply)

    def GetDeviceCreateContextUnchecked(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 2, False, False),
                                 GetDeviceCreateContextCookie(),
                                 GetDeviceCreateContextReply)

    def SetDeviceContextChecked(self, device, context_len, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', device, context_len))
        buf.write(str(buffer(array('b', context))))
        return self.send_request(xcb.Request(buf.getvalue(), 3, True, True),
                                 xcb.VoidCookie())

    def SetDeviceContext(self, device, context_len, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', device, context_len))
        buf.write(str(buffer(array('b', context))))
        return self.send_request(xcb.Request(buf.getvalue(), 3, True, False),
                                 xcb.VoidCookie())

    def GetDeviceContext(self, device):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', device))
        return self.send_request(xcb.Request(buf.getvalue(), 4, False, True),
                                 GetDeviceContextCookie(),
                                 GetDeviceContextReply)

    def GetDeviceContextUnchecked(self, device):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', device))
        return self.send_request(xcb.Request(buf.getvalue(), 4, False, False),
                                 GetDeviceContextCookie(),
                                 GetDeviceContextReply)

    def SetWindowCreateContextChecked(self, context_len, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context_len))
        buf.write(str(buffer(array('b', context))))
        return self.send_request(xcb.Request(buf.getvalue(), 5, True, True),
                                 xcb.VoidCookie())

    def SetWindowCreateContext(self, context_len, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context_len))
        buf.write(str(buffer(array('b', context))))
        return self.send_request(xcb.Request(buf.getvalue(), 5, True, False),
                                 xcb.VoidCookie())

    def GetWindowCreateContext(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 6, False, True),
                                 GetWindowCreateContextCookie(),
                                 GetWindowCreateContextReply)

    def GetWindowCreateContextUnchecked(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 6, False, False),
                                 GetWindowCreateContextCookie(),
                                 GetWindowCreateContextReply)

    def GetWindowContext(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 7, False, True),
                                 GetWindowContextCookie(),
                                 GetWindowContextReply)

    def GetWindowContextUnchecked(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 7, False, False),
                                 GetWindowContextCookie(),
                                 GetWindowContextReply)

    def SetPropertyCreateContextChecked(self, context_len, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context_len))
        buf.write(str(buffer(array('b', context))))
        return self.send_request(xcb.Request(buf.getvalue(), 8, True, True),
                                 xcb.VoidCookie())

    def SetPropertyCreateContext(self, context_len, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context_len))
        buf.write(str(buffer(array('b', context))))
        return self.send_request(xcb.Request(buf.getvalue(), 8, True, False),
                                 xcb.VoidCookie())

    def GetPropertyCreateContext(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 9, False, True),
                                 GetPropertyCreateContextCookie(),
                                 GetPropertyCreateContextReply)

    def GetPropertyCreateContextUnchecked(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 9, False, False),
                                 GetPropertyCreateContextCookie(),
                                 GetPropertyCreateContextReply)

    def SetPropertyUseContextChecked(self, context_len, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context_len))
        buf.write(str(buffer(array('b', context))))
        return self.send_request(xcb.Request(buf.getvalue(), 10, True, True),
                                 xcb.VoidCookie())

    def SetPropertyUseContext(self, context_len, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context_len))
        buf.write(str(buffer(array('b', context))))
        return self.send_request(xcb.Request(buf.getvalue(), 10, True, False),
                                 xcb.VoidCookie())

    def GetPropertyUseContext(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 11, False, True),
                                 GetPropertyUseContextCookie(),
                                 GetPropertyUseContextReply)

    def GetPropertyUseContextUnchecked(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 11, False, False),
                                 GetPropertyUseContextCookie(),
                                 GetPropertyUseContextReply)

    def GetPropertyContext(self, window, property):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', window, property))
        return self.send_request(xcb.Request(buf.getvalue(), 12, False, True),
                                 GetPropertyContextCookie(),
                                 GetPropertyContextReply)

    def GetPropertyContextUnchecked(self, window, property):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', window, property))
        return self.send_request(xcb.Request(buf.getvalue(), 12, False, False),
                                 GetPropertyContextCookie(),
                                 GetPropertyContextReply)

    def GetPropertyDataContext(self, window, property):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', window, property))
        return self.send_request(xcb.Request(buf.getvalue(), 13, False, True),
                                 GetPropertyDataContextCookie(),
                                 GetPropertyDataContextReply)

    def GetPropertyDataContextUnchecked(self, window, property):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', window, property))
        return self.send_request(xcb.Request(buf.getvalue(), 13, False, False),
                                 GetPropertyDataContextCookie(),
                                 GetPropertyDataContextReply)

    def ListProperties(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 14, False, True),
                                 ListPropertiesCookie(),
                                 ListPropertiesReply)

    def ListPropertiesUnchecked(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 14, False, False),
                                 ListPropertiesCookie(),
                                 ListPropertiesReply)

    def SetSelectionCreateContextChecked(self, context_len, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context_len))
        buf.write(str(buffer(array('b', context))))
        return self.send_request(xcb.Request(buf.getvalue(), 15, True, True),
                                 xcb.VoidCookie())

    def SetSelectionCreateContext(self, context_len, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context_len))
        buf.write(str(buffer(array('b', context))))
        return self.send_request(xcb.Request(buf.getvalue(), 15, True, False),
                                 xcb.VoidCookie())

    def GetSelectionCreateContext(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 16, False, True),
                                 GetSelectionCreateContextCookie(),
                                 GetSelectionCreateContextReply)

    def GetSelectionCreateContextUnchecked(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 16, False, False),
                                 GetSelectionCreateContextCookie(),
                                 GetSelectionCreateContextReply)

    def SetSelectionUseContextChecked(self, context_len, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context_len))
        buf.write(str(buffer(array('b', context))))
        return self.send_request(xcb.Request(buf.getvalue(), 17, True, True),
                                 xcb.VoidCookie())

    def SetSelectionUseContext(self, context_len, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context_len))
        buf.write(str(buffer(array('b', context))))
        return self.send_request(xcb.Request(buf.getvalue(), 17, True, False),
                                 xcb.VoidCookie())

    def GetSelectionUseContext(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 18, False, True),
                                 GetSelectionUseContextCookie(),
                                 GetSelectionUseContextReply)

    def GetSelectionUseContextUnchecked(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 18, False, False),
                                 GetSelectionUseContextCookie(),
                                 GetSelectionUseContextReply)

    def GetSelectionContext(self, selection):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', selection))
        return self.send_request(xcb.Request(buf.getvalue(), 19, False, True),
                                 GetSelectionContextCookie(),
                                 GetSelectionContextReply)

    def GetSelectionContextUnchecked(self, selection):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', selection))
        return self.send_request(xcb.Request(buf.getvalue(), 19, False, False),
                                 GetSelectionContextCookie(),
                                 GetSelectionContextReply)

    def GetSelectionDataContext(self, selection):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', selection))
        return self.send_request(xcb.Request(buf.getvalue(), 20, False, True),
                                 GetSelectionDataContextCookie(),
                                 GetSelectionDataContextReply)

    def GetSelectionDataContextUnchecked(self, selection):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', selection))
        return self.send_request(xcb.Request(buf.getvalue(), 20, False, False),
                                 GetSelectionDataContextCookie(),
                                 GetSelectionDataContextReply)

    def ListSelections(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 21, False, True),
                                 ListSelectionsCookie(),
                                 ListSelectionsReply)

    def ListSelectionsUnchecked(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 21, False, False),
                                 ListSelectionsCookie(),
                                 ListSelectionsReply)

    def GetClientContext(self, resource):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', resource))
        return self.send_request(xcb.Request(buf.getvalue(), 22, False, True),
                                 GetClientContextCookie(),
                                 GetClientContextReply)

    def GetClientContextUnchecked(self, resource):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', resource))
        return self.send_request(xcb.Request(buf.getvalue(), 22, False, False),
                                 GetClientContextCookie(),
                                 GetClientContextReply)

_events = {
}

_errors = {
}

xcb._add_ext(key, xselinuxExtension, _events, _errors)
