#
# This file generated automatically from xv.xml by py_client.py.
# Edit at your peril.
#

import xcb
import cStringIO
from struct import pack, unpack_from
from array import array
import xproto
import shm

MAJOR_VERSION = 2
MINOR_VERSION = 2

key = xcb.ExtensionKey('XVideo')

class Type:
    InputMask = 1
    OutputMask = 2
    VideoMask = 4
    StillMask = 8
    ImageMask = 16

class ImageFormatInfoType:
    RGB = 0
    YUV = 1

class ImageFormatInfoFormat:
    Packed = 0
    Planar = 1

class AttributeFlag:
    Gettable = 1
    Settable = 2

class Rational(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.numerator, self.denominator,) = unpack_from('ii', self, count)

class Format(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.visual, self.depth,) = unpack_from('IB3x', self, count)

class AdaptorInfo(xcb.Struct):
    def __init__(self, parent, offset):
        xcb.Struct.__init__(self, parent, offset)
        count = 0
        (self.base_id, self.name_size, self.num_ports, self.num_formats, self.type,) = unpack_from('IHHHBx', self, count)
        count += 12
        self.name = xcb.List(self, count, self.name_size, 'b', 1)
        count += len(self.name.buf())
        count += xcb.type_pad(8, count)
        self.formats = xcb.List(self, count, self.num_formats, Format, 8)
        count += len(self.formats.buf())
        xcb._resize_obj(self, count)

class EncodingInfo(xcb.Struct):
    def __init__(self, parent, offset):
        xcb.Struct.__init__(self, parent, offset)
        count = 0
        (self.encoding, self.name_size, self.width, self.height,) = unpack_from('IHHH2x', self, count)
        count += 12
        self.rate = Rational(self, count, 8)
        count += 8
        count += xcb.type_pad(1, count)
        self.name = xcb.List(self, count, self.name_size, 'b', 1)
        count += len(self.name.buf())
        xcb._resize_obj(self, count)

class Image(xcb.Struct):
    def __init__(self, parent, offset):
        xcb.Struct.__init__(self, parent, offset)
        count = 0
        (self.id, self.width, self.height, self.data_size, self.num_planes,) = unpack_from('IHHII', self, count)
        count += 16
        self.pitches = xcb.List(self, count, self.num_planes, 'I', 4)
        count += len(self.pitches.buf())
        count += xcb.type_pad(4, count)
        self.offsets = xcb.List(self, count, self.num_planes, 'I', 4)
        count += len(self.offsets.buf())
        count += xcb.type_pad(1, count)
        self.data = xcb.List(self, count, self.data_size, 'B', 1)
        count += len(self.data.buf())
        xcb._resize_obj(self, count)

class AttributeInfo(xcb.Struct):
    def __init__(self, parent, offset):
        xcb.Struct.__init__(self, parent, offset)
        count = 0
        (self.flags, self.min, self.max, self.size,) = unpack_from('IiiI', self, count)
        count += 16
        self.name = xcb.List(self, count, self.size, 'b', 1)
        count += len(self.name.buf())
        xcb._resize_obj(self, count)

class ImageFormatInfo(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.id, self.type, self.byte_order,) = unpack_from('IBB2x', self, count)
        count += 8
        self.guid = xcb.List(self, count, 16, 'B', 1)
        count += len(self.guid.buf())
        (self.bpp, self.num_planes, self.depth, self.red_mask, self.green_mask, self.blue_mask, self.format, self.y_sample_bits, self.u_sample_bits, self.v_sample_bits, self.vhorz_y_period, self.vhorz_u_period, self.vhorz_v_period, self.vvert_y_period, self.vvert_u_period, self.vvert_v_period,) = unpack_from('BB2xB3xIIIB3xIIIIIIIII', self, count)
        count += 60
        count += xcb.type_pad(1, count)
        self.vcomp_order = xcb.List(self, count, 32, 'B', 1)
        count += len(self.vcomp_order.buf())
        count += xcb.type_pad(4, count)
        (self.vscanline_order,) = unpack_from('B11x', self, count)

class PortError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)

class BadPort(xcb.ProtocolException):
    pass

class EncodingError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)

class BadEncoding(xcb.ProtocolException):
    pass

class ControlError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)

class BadControl(xcb.ProtocolException):
    pass

class VideoNotifyEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.reason, self.time, self.drawable, self.port,) = unpack_from('xB2xIII', self, count)

class PortNotifyEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.time, self.port, self.attribute, self.value,) = unpack_from('xx2xIIIi', self, count)

class QueryExtensionCookie(xcb.Cookie):
    pass

class QueryExtensionReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.major, self.minor,) = unpack_from('xx2x4xHH', self, count)

class QueryAdaptorsCookie(xcb.Cookie):
    pass

class QueryAdaptorsReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.num_adaptors,) = unpack_from('xx2x4xH22x', self, count)
        count += 32
        self.info = xcb.List(self, count, self.num_adaptors, AdaptorInfo, -1)

class QueryEncodingsCookie(xcb.Cookie):
    pass

class QueryEncodingsReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.num_encodings,) = unpack_from('xx2x4xH22x', self, count)
        count += 32
        self.info = xcb.List(self, count, self.num_encodings, EncodingInfo, -1)

class GrabPortCookie(xcb.Cookie):
    pass

class GrabPortReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.result,) = unpack_from('xB2x4x', self, count)

class QueryBestSizeCookie(xcb.Cookie):
    pass

class QueryBestSizeReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.actual_width, self.actual_height,) = unpack_from('xx2x4xHH', self, count)

class GetPortAttributeCookie(xcb.Cookie):
    pass

class GetPortAttributeReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.value,) = unpack_from('xx2x4xi', self, count)

class QueryPortAttributesCookie(xcb.Cookie):
    pass

class QueryPortAttributesReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.num_attributes, self.text_size,) = unpack_from('xx2x4xII16x', self, count)
        count += 32
        self.attributes = xcb.List(self, count, self.num_attributes, AttributeInfo, -1)

class ListImageFormatsCookie(xcb.Cookie):
    pass

class ListImageFormatsReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.num_formats,) = unpack_from('xx2x4xI20x', self, count)
        count += 32
        self.format = xcb.List(self, count, self.num_formats, ImageFormatInfo, 128)

class QueryImageAttributesCookie(xcb.Cookie):
    pass

class QueryImageAttributesReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.num_planes, self.data_size, self.width, self.height,) = unpack_from('xx2x4xIIHH12x', self, count)
        count += 32
        self.pitches = xcb.List(self, count, self.num_planes, 'I', 4)
        count += len(self.pitches.buf())
        count += xcb.type_pad(4, count)
        self.offsets = xcb.List(self, count, self.num_planes, 'I', 4)

class xvExtension(xcb.Extension):

    def QueryExtension(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 0, False, True),
                                 QueryExtensionCookie(),
                                 QueryExtensionReply)

    def QueryExtensionUnchecked(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 0, False, False),
                                 QueryExtensionCookie(),
                                 QueryExtensionReply)

    def QueryAdaptors(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 1, False, True),
                                 QueryAdaptorsCookie(),
                                 QueryAdaptorsReply)

    def QueryAdaptorsUnchecked(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 1, False, False),
                                 QueryAdaptorsCookie(),
                                 QueryAdaptorsReply)

    def QueryEncodings(self, port):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', port))
        return self.send_request(xcb.Request(buf.getvalue(), 2, False, True),
                                 QueryEncodingsCookie(),
                                 QueryEncodingsReply)

    def QueryEncodingsUnchecked(self, port):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', port))
        return self.send_request(xcb.Request(buf.getvalue(), 2, False, False),
                                 QueryEncodingsCookie(),
                                 QueryEncodingsReply)

    def GrabPort(self, port, time):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', port, time))
        return self.send_request(xcb.Request(buf.getvalue(), 3, False, True),
                                 GrabPortCookie(),
                                 GrabPortReply)

    def GrabPortUnchecked(self, port, time):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', port, time))
        return self.send_request(xcb.Request(buf.getvalue(), 3, False, False),
                                 GrabPortCookie(),
                                 GrabPortReply)

    def UngrabPortChecked(self, port, time):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', port, time))
        return self.send_request(xcb.Request(buf.getvalue(), 4, True, True),
                                 xcb.VoidCookie())

    def UngrabPort(self, port, time):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', port, time))
        return self.send_request(xcb.Request(buf.getvalue(), 4, True, False),
                                 xcb.VoidCookie())

    def PutVideoChecked(self, port, drawable, gc, vid_x, vid_y, vid_w, vid_h, drw_x, drw_y, drw_w, drw_h):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIhhHHhhHH', port, drawable, gc, vid_x, vid_y, vid_w, vid_h, drw_x, drw_y, drw_w, drw_h))
        return self.send_request(xcb.Request(buf.getvalue(), 5, True, True),
                                 xcb.VoidCookie())

    def PutVideo(self, port, drawable, gc, vid_x, vid_y, vid_w, vid_h, drw_x, drw_y, drw_w, drw_h):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIhhHHhhHH', port, drawable, gc, vid_x, vid_y, vid_w, vid_h, drw_x, drw_y, drw_w, drw_h))
        return self.send_request(xcb.Request(buf.getvalue(), 5, True, False),
                                 xcb.VoidCookie())

    def PutStillChecked(self, port, drawable, gc, vid_x, vid_y, vid_w, vid_h, drw_x, drw_y, drw_w, drw_h):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIhhHHhhHH', port, drawable, gc, vid_x, vid_y, vid_w, vid_h, drw_x, drw_y, drw_w, drw_h))
        return self.send_request(xcb.Request(buf.getvalue(), 6, True, True),
                                 xcb.VoidCookie())

    def PutStill(self, port, drawable, gc, vid_x, vid_y, vid_w, vid_h, drw_x, drw_y, drw_w, drw_h):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIhhHHhhHH', port, drawable, gc, vid_x, vid_y, vid_w, vid_h, drw_x, drw_y, drw_w, drw_h))
        return self.send_request(xcb.Request(buf.getvalue(), 6, True, False),
                                 xcb.VoidCookie())

    def GetVideoChecked(self, port, drawable, gc, vid_x, vid_y, vid_w, vid_h, drw_x, drw_y, drw_w, drw_h):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIhhHHhhHH', port, drawable, gc, vid_x, vid_y, vid_w, vid_h, drw_x, drw_y, drw_w, drw_h))
        return self.send_request(xcb.Request(buf.getvalue(), 7, True, True),
                                 xcb.VoidCookie())

    def GetVideo(self, port, drawable, gc, vid_x, vid_y, vid_w, vid_h, drw_x, drw_y, drw_w, drw_h):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIhhHHhhHH', port, drawable, gc, vid_x, vid_y, vid_w, vid_h, drw_x, drw_y, drw_w, drw_h))
        return self.send_request(xcb.Request(buf.getvalue(), 7, True, False),
                                 xcb.VoidCookie())

    def GetStillChecked(self, port, drawable, gc, vid_x, vid_y, vid_w, vid_h, drw_x, drw_y, drw_w, drw_h):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIhhHHhhHH', port, drawable, gc, vid_x, vid_y, vid_w, vid_h, drw_x, drw_y, drw_w, drw_h))
        return self.send_request(xcb.Request(buf.getvalue(), 8, True, True),
                                 xcb.VoidCookie())

    def GetStill(self, port, drawable, gc, vid_x, vid_y, vid_w, vid_h, drw_x, drw_y, drw_w, drw_h):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIhhHHhhHH', port, drawable, gc, vid_x, vid_y, vid_w, vid_h, drw_x, drw_y, drw_w, drw_h))
        return self.send_request(xcb.Request(buf.getvalue(), 8, True, False),
                                 xcb.VoidCookie())

    def StopVideoChecked(self, port, drawable):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', port, drawable))
        return self.send_request(xcb.Request(buf.getvalue(), 9, True, True),
                                 xcb.VoidCookie())

    def StopVideo(self, port, drawable):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', port, drawable))
        return self.send_request(xcb.Request(buf.getvalue(), 9, True, False),
                                 xcb.VoidCookie())

    def SelectVideoNotifyChecked(self, drawable, onoff):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIB3x', drawable, onoff))
        return self.send_request(xcb.Request(buf.getvalue(), 10, True, True),
                                 xcb.VoidCookie())

    def SelectVideoNotify(self, drawable, onoff):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIB3x', drawable, onoff))
        return self.send_request(xcb.Request(buf.getvalue(), 10, True, False),
                                 xcb.VoidCookie())

    def SelectPortNotifyChecked(self, port, onoff):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIB3x', port, onoff))
        return self.send_request(xcb.Request(buf.getvalue(), 11, True, True),
                                 xcb.VoidCookie())

    def SelectPortNotify(self, port, onoff):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIB3x', port, onoff))
        return self.send_request(xcb.Request(buf.getvalue(), 11, True, False),
                                 xcb.VoidCookie())

    def QueryBestSize(self, port, vid_w, vid_h, drw_w, drw_h, motion):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIHHHHB3x', port, vid_w, vid_h, drw_w, drw_h, motion))
        return self.send_request(xcb.Request(buf.getvalue(), 12, False, True),
                                 QueryBestSizeCookie(),
                                 QueryBestSizeReply)

    def QueryBestSizeUnchecked(self, port, vid_w, vid_h, drw_w, drw_h, motion):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIHHHHB3x', port, vid_w, vid_h, drw_w, drw_h, motion))
        return self.send_request(xcb.Request(buf.getvalue(), 12, False, False),
                                 QueryBestSizeCookie(),
                                 QueryBestSizeReply)

    def SetPortAttributeChecked(self, port, attribute, value):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIi', port, attribute, value))
        return self.send_request(xcb.Request(buf.getvalue(), 13, True, True),
                                 xcb.VoidCookie())

    def SetPortAttribute(self, port, attribute, value):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIi', port, attribute, value))
        return self.send_request(xcb.Request(buf.getvalue(), 13, True, False),
                                 xcb.VoidCookie())

    def GetPortAttribute(self, port, attribute):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', port, attribute))
        return self.send_request(xcb.Request(buf.getvalue(), 14, False, True),
                                 GetPortAttributeCookie(),
                                 GetPortAttributeReply)

    def GetPortAttributeUnchecked(self, port, attribute):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', port, attribute))
        return self.send_request(xcb.Request(buf.getvalue(), 14, False, False),
                                 GetPortAttributeCookie(),
                                 GetPortAttributeReply)

    def QueryPortAttributes(self, port):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', port))
        return self.send_request(xcb.Request(buf.getvalue(), 15, False, True),
                                 QueryPortAttributesCookie(),
                                 QueryPortAttributesReply)

    def QueryPortAttributesUnchecked(self, port):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', port))
        return self.send_request(xcb.Request(buf.getvalue(), 15, False, False),
                                 QueryPortAttributesCookie(),
                                 QueryPortAttributesReply)

    def ListImageFormats(self, port):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', port))
        return self.send_request(xcb.Request(buf.getvalue(), 16, False, True),
                                 ListImageFormatsCookie(),
                                 ListImageFormatsReply)

    def ListImageFormatsUnchecked(self, port):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', port))
        return self.send_request(xcb.Request(buf.getvalue(), 16, False, False),
                                 ListImageFormatsCookie(),
                                 ListImageFormatsReply)

    def QueryImageAttributes(self, port, id, width, height):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIHH', port, id, width, height))
        return self.send_request(xcb.Request(buf.getvalue(), 17, False, True),
                                 QueryImageAttributesCookie(),
                                 QueryImageAttributesReply)

    def QueryImageAttributesUnchecked(self, port, id, width, height):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIHH', port, id, width, height))
        return self.send_request(xcb.Request(buf.getvalue(), 17, False, False),
                                 QueryImageAttributesCookie(),
                                 QueryImageAttributesReply)

    def PutImageChecked(self, port, drawable, gc, id, src_x, src_y, src_w, src_h, drw_x, drw_y, drw_w, drw_h, width, height, data_len, data):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIIhhHHhhHHHH', port, drawable, gc, id, src_x, src_y, src_w, src_h, drw_x, drw_y, drw_w, drw_h, width, height))
        buf.write(str(buffer(array('B', data))))
        return self.send_request(xcb.Request(buf.getvalue(), 18, True, True),
                                 xcb.VoidCookie())

    def PutImage(self, port, drawable, gc, id, src_x, src_y, src_w, src_h, drw_x, drw_y, drw_w, drw_h, width, height, data_len, data):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIIhhHHhhHHHH', port, drawable, gc, id, src_x, src_y, src_w, src_h, drw_x, drw_y, drw_w, drw_h, width, height))
        buf.write(str(buffer(array('B', data))))
        return self.send_request(xcb.Request(buf.getvalue(), 18, True, False),
                                 xcb.VoidCookie())

    def ShmPutImageChecked(self, port, drawable, gc, shmseg, id, offset, src_x, src_y, src_w, src_h, drw_x, drw_y, drw_w, drw_h, width, height, send_event):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIIIIhhHHhhHHHHB3x', port, drawable, gc, shmseg, id, offset, src_x, src_y, src_w, src_h, drw_x, drw_y, drw_w, drw_h, width, height, send_event))
        return self.send_request(xcb.Request(buf.getvalue(), 19, True, True),
                                 xcb.VoidCookie())

    def ShmPutImage(self, port, drawable, gc, shmseg, id, offset, src_x, src_y, src_w, src_h, drw_x, drw_y, drw_w, drw_h, width, height, send_event):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIIIIhhHHhhHHHHB3x', port, drawable, gc, shmseg, id, offset, src_x, src_y, src_w, src_h, drw_x, drw_y, drw_w, drw_h, width, height, send_event))
        return self.send_request(xcb.Request(buf.getvalue(), 19, True, False),
                                 xcb.VoidCookie())

_events = {
    0 : VideoNotifyEvent,
    1 : PortNotifyEvent,
}

_errors = {
    0 : (PortError, BadPort),
    1 : (EncodingError, BadEncoding),
    2 : (ControlError, BadControl),
}

xcb._add_ext(key, xvExtension, _events, _errors)
