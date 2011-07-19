#
# This file generated automatically from xprint.xml by py_client.py.
# Edit at your peril.
#

import xcb
import cStringIO
from struct import pack, unpack_from
from array import array
import xproto

MAJOR_VERSION = 1
MINOR_VERSION = 0

key = xcb.ExtensionKey('XpExtension')

class PRINTER(xcb.Struct):
    def __init__(self, parent, offset):
        xcb.Struct.__init__(self, parent, offset)
        count = 0
        (self.nameLen,) = unpack_from('I', self, count)
        count += 4
        self.name = xcb.List(self, count, self.nameLen, 'b', 1)
        count += len(self.name.buf())
        (self.descLen,) = unpack_from('I', self, count)
        count += 4
        count += xcb.type_pad(1, count)
        self.description = xcb.List(self, count, self.descLen, 'b', 1)
        count += len(self.description.buf())
        xcb._resize_obj(self, count)

class GetDoc:
    Finished = 0
    SecondConsumer = 1

class EvMask:
    NoEventMask = 0
    PrintMask = 1
    AttributeMask = 2

class Detail:
    StartJobNotify = 1
    EndJobNotify = 2
    StartDocNotify = 3
    EndDocNotify = 4
    StartPageNotify = 5
    EndPageNotify = 6

class Attr:
    JobAttr = 1
    DocAttr = 2
    PageAttr = 3
    PrinterAttr = 4
    ServerAttr = 5
    MediumAttr = 6
    SpoolerAttr = 7

class PrintQueryVersionCookie(xcb.Cookie):
    pass

class PrintQueryVersionReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.major_version, self.minor_version,) = unpack_from('xx2x4xHH', self, count)

class PrintGetPrinterListCookie(xcb.Cookie):
    pass

class PrintGetPrinterListReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.listCount,) = unpack_from('xx2x4xI20x', self, count)
        count += 32
        self.printers = xcb.List(self, count, self.listCount, PRINTER, -1)

class PrintGetContextCookie(xcb.Cookie):
    pass

class PrintGetContextReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.context,) = unpack_from('xx2x4xI', self, count)

class PrintGetScreenOfContextCookie(xcb.Cookie):
    pass

class PrintGetScreenOfContextReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.root,) = unpack_from('xx2x4xI', self, count)

class PrintGetDocumentDataCookie(xcb.Cookie):
    pass

class PrintGetDocumentDataReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.status_code, self.finished_flag, self.dataLen,) = unpack_from('xx2x4xIII12x', self, count)
        count += 32
        self.data = xcb.List(self, count, self.dataLen, 'B', 1)

class PrintInputSelectedCookie(xcb.Cookie):
    pass

class PrintInputSelectedReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.event_mask,) = unpack_from('xx2x4xI', self, count)
        count += 12
        self.event_list = xcb.List(self, count, xcb.popcount(self.event_mask), 'I', 4)
        count += len(self.event_list.buf())
        (self.all_events_mask,) = unpack_from('I', self, count)
        count += 4
        count += xcb.type_pad(4, count)
        self.all_events_list = xcb.List(self, count, xcb.popcount(self.all_events_mask), 'I', 4)

class PrintGetAttributesCookie(xcb.Cookie):
    pass

class PrintGetAttributesReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.stringLen, self.attributes,) = unpack_from('xx2x4xI20xb', self, count)

class PrintGetOneAttributesCookie(xcb.Cookie):
    pass

class PrintGetOneAttributesReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.valueLen,) = unpack_from('xx2x4xI20x', self, count)
        count += 32
        self.value = xcb.List(self, count, self.valueLen, 'b', 1)

class PrintGetPageDimensionsCookie(xcb.Cookie):
    pass

class PrintGetPageDimensionsReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.width, self.height, self.offset_x, self.offset_y, self.reproducible_width, self.reproducible_height,) = unpack_from('xx2x4xHHHHHH', self, count)

class PrintQueryScreensCookie(xcb.Cookie):
    pass

class PrintQueryScreensReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.listCount,) = unpack_from('xx2x4xI20x', self, count)
        count += 32
        self.roots = xcb.List(self, count, self.listCount, 'I', 4)

class PrintSetImageResolutionCookie(xcb.Cookie):
    pass

class PrintSetImageResolutionReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.status, self.previous_resolutions,) = unpack_from('xB2x4xH', self, count)

class PrintGetImageResolutionCookie(xcb.Cookie):
    pass

class PrintGetImageResolutionReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.image_resolution,) = unpack_from('xx2x4xH', self, count)

class NotifyEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.detail, self.context, self.cancel,) = unpack_from('xB2xIB', self, count)

class AttributNotifyEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.detail, self.context,) = unpack_from('xB2xI', self, count)

class ContextError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)

class BadContext(xcb.ProtocolException):
    pass

class SequenceError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)

class BadSequence(xcb.ProtocolException):
    pass

class xprintExtension(xcb.Extension):

    def PrintQueryVersion(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 0, False, True),
                                 PrintQueryVersionCookie(),
                                 PrintQueryVersionReply)

    def PrintQueryVersionUnchecked(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 0, False, False),
                                 PrintQueryVersionCookie(),
                                 PrintQueryVersionReply)

    def PrintGetPrinterList(self, printerNameLen, localeLen, printer_name, locale):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', printerNameLen, localeLen))
        buf.write(str(buffer(array('b', printer_name))))
        buf.write(str(buffer(array('b', locale))))
        return self.send_request(xcb.Request(buf.getvalue(), 1, False, True),
                                 PrintGetPrinterListCookie(),
                                 PrintGetPrinterListReply)

    def PrintGetPrinterListUnchecked(self, printerNameLen, localeLen, printer_name, locale):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', printerNameLen, localeLen))
        buf.write(str(buffer(array('b', printer_name))))
        buf.write(str(buffer(array('b', locale))))
        return self.send_request(xcb.Request(buf.getvalue(), 1, False, False),
                                 PrintGetPrinterListCookie(),
                                 PrintGetPrinterListReply)

    def PrintRehashPrinterListChecked(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 20, True, True),
                                 xcb.VoidCookie())

    def PrintRehashPrinterList(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 20, True, False),
                                 xcb.VoidCookie())

    def CreateContextChecked(self, context_id, printerNameLen, localeLen, printerName, locale):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_id, printerNameLen, localeLen))
        buf.write(str(buffer(array('b', printerName))))
        buf.write(str(buffer(array('b', locale))))
        return self.send_request(xcb.Request(buf.getvalue(), 2, True, True),
                                 xcb.VoidCookie())

    def CreateContext(self, context_id, printerNameLen, localeLen, printerName, locale):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_id, printerNameLen, localeLen))
        buf.write(str(buffer(array('b', printerName))))
        buf.write(str(buffer(array('b', locale))))
        return self.send_request(xcb.Request(buf.getvalue(), 2, True, False),
                                 xcb.VoidCookie())

    def PrintSetContextChecked(self, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context))
        return self.send_request(xcb.Request(buf.getvalue(), 3, True, True),
                                 xcb.VoidCookie())

    def PrintSetContext(self, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context))
        return self.send_request(xcb.Request(buf.getvalue(), 3, True, False),
                                 xcb.VoidCookie())

    def PrintGetContext(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 4, False, True),
                                 PrintGetContextCookie(),
                                 PrintGetContextReply)

    def PrintGetContextUnchecked(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 4, False, False),
                                 PrintGetContextCookie(),
                                 PrintGetContextReply)

    def PrintDestroyContextChecked(self, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context))
        return self.send_request(xcb.Request(buf.getvalue(), 5, True, True),
                                 xcb.VoidCookie())

    def PrintDestroyContext(self, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context))
        return self.send_request(xcb.Request(buf.getvalue(), 5, True, False),
                                 xcb.VoidCookie())

    def PrintGetScreenOfContext(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 6, False, True),
                                 PrintGetScreenOfContextCookie(),
                                 PrintGetScreenOfContextReply)

    def PrintGetScreenOfContextUnchecked(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 6, False, False),
                                 PrintGetScreenOfContextCookie(),
                                 PrintGetScreenOfContextReply)

    def PrintStartJobChecked(self, output_mode):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xB', output_mode))
        return self.send_request(xcb.Request(buf.getvalue(), 7, True, True),
                                 xcb.VoidCookie())

    def PrintStartJob(self, output_mode):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xB', output_mode))
        return self.send_request(xcb.Request(buf.getvalue(), 7, True, False),
                                 xcb.VoidCookie())

    def PrintEndJobChecked(self, cancel):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xB', cancel))
        return self.send_request(xcb.Request(buf.getvalue(), 8, True, True),
                                 xcb.VoidCookie())

    def PrintEndJob(self, cancel):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xB', cancel))
        return self.send_request(xcb.Request(buf.getvalue(), 8, True, False),
                                 xcb.VoidCookie())

    def PrintStartDocChecked(self, driver_mode):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xB', driver_mode))
        return self.send_request(xcb.Request(buf.getvalue(), 9, True, True),
                                 xcb.VoidCookie())

    def PrintStartDoc(self, driver_mode):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xB', driver_mode))
        return self.send_request(xcb.Request(buf.getvalue(), 9, True, False),
                                 xcb.VoidCookie())

    def PrintEndDocChecked(self, cancel):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xB', cancel))
        return self.send_request(xcb.Request(buf.getvalue(), 10, True, True),
                                 xcb.VoidCookie())

    def PrintEndDoc(self, cancel):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xB', cancel))
        return self.send_request(xcb.Request(buf.getvalue(), 10, True, False),
                                 xcb.VoidCookie())

    def PrintPutDocumentDataChecked(self, drawable, len_data, len_fmt, len_options, data, doc_format_len, doc_format, options_len, options):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIHH', drawable, len_data, len_fmt, len_options))
        buf.write(str(buffer(array('B', data))))
        buf.write(str(buffer(array('b', doc_format))))
        buf.write(str(buffer(array('b', options))))
        return self.send_request(xcb.Request(buf.getvalue(), 11, True, True),
                                 xcb.VoidCookie())

    def PrintPutDocumentData(self, drawable, len_data, len_fmt, len_options, data, doc_format_len, doc_format, options_len, options):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIHH', drawable, len_data, len_fmt, len_options))
        buf.write(str(buffer(array('B', data))))
        buf.write(str(buffer(array('b', doc_format))))
        buf.write(str(buffer(array('b', options))))
        return self.send_request(xcb.Request(buf.getvalue(), 11, True, False),
                                 xcb.VoidCookie())

    def PrintGetDocumentData(self, context, max_bytes):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', context, max_bytes))
        return self.send_request(xcb.Request(buf.getvalue(), 12, False, True),
                                 PrintGetDocumentDataCookie(),
                                 PrintGetDocumentDataReply)

    def PrintGetDocumentDataUnchecked(self, context, max_bytes):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', context, max_bytes))
        return self.send_request(xcb.Request(buf.getvalue(), 12, False, False),
                                 PrintGetDocumentDataCookie(),
                                 PrintGetDocumentDataReply)

    def PrintStartPageChecked(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 13, True, True),
                                 xcb.VoidCookie())

    def PrintStartPage(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 13, True, False),
                                 xcb.VoidCookie())

    def PrintEndPageChecked(self, cancel):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xB3x', cancel))
        return self.send_request(xcb.Request(buf.getvalue(), 14, True, True),
                                 xcb.VoidCookie())

    def PrintEndPage(self, cancel):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xB3x', cancel))
        return self.send_request(xcb.Request(buf.getvalue(), 14, True, False),
                                 xcb.VoidCookie())

    def PrintSelectInputChecked(self, context, event_mask, event_list):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', context, event_mask))
        buf.write(str(buffer(array('I', event_list))))
        return self.send_request(xcb.Request(buf.getvalue(), 15, True, True),
                                 xcb.VoidCookie())

    def PrintSelectInput(self, context, event_mask, event_list):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', context, event_mask))
        buf.write(str(buffer(array('I', event_list))))
        return self.send_request(xcb.Request(buf.getvalue(), 15, True, False),
                                 xcb.VoidCookie())

    def PrintInputSelected(self, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context))
        return self.send_request(xcb.Request(buf.getvalue(), 16, False, True),
                                 PrintInputSelectedCookie(),
                                 PrintInputSelectedReply)

    def PrintInputSelectedUnchecked(self, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context))
        return self.send_request(xcb.Request(buf.getvalue(), 16, False, False),
                                 PrintInputSelectedCookie(),
                                 PrintInputSelectedReply)

    def PrintGetAttributes(self, context, pool):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIB3x', context, pool))
        return self.send_request(xcb.Request(buf.getvalue(), 17, False, True),
                                 PrintGetAttributesCookie(),
                                 PrintGetAttributesReply)

    def PrintGetAttributesUnchecked(self, context, pool):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIB3x', context, pool))
        return self.send_request(xcb.Request(buf.getvalue(), 17, False, False),
                                 PrintGetAttributesCookie(),
                                 PrintGetAttributesReply)

    def PrintGetOneAttributes(self, context, nameLen, pool, name):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIB3x', context, nameLen, pool))
        buf.write(str(buffer(array('b', name))))
        return self.send_request(xcb.Request(buf.getvalue(), 19, False, True),
                                 PrintGetOneAttributesCookie(),
                                 PrintGetOneAttributesReply)

    def PrintGetOneAttributesUnchecked(self, context, nameLen, pool, name):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIB3x', context, nameLen, pool))
        buf.write(str(buffer(array('b', name))))
        return self.send_request(xcb.Request(buf.getvalue(), 19, False, False),
                                 PrintGetOneAttributesCookie(),
                                 PrintGetOneAttributesReply)

    def PrintSetAttributesChecked(self, context, stringLen, pool, rule, attributes_len, attributes):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIBB2x', context, stringLen, pool, rule))
        buf.write(str(buffer(array('b', attributes))))
        return self.send_request(xcb.Request(buf.getvalue(), 18, True, True),
                                 xcb.VoidCookie())

    def PrintSetAttributes(self, context, stringLen, pool, rule, attributes_len, attributes):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIBB2x', context, stringLen, pool, rule))
        buf.write(str(buffer(array('b', attributes))))
        return self.send_request(xcb.Request(buf.getvalue(), 18, True, False),
                                 xcb.VoidCookie())

    def PrintGetPageDimensions(self, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context))
        return self.send_request(xcb.Request(buf.getvalue(), 21, False, True),
                                 PrintGetPageDimensionsCookie(),
                                 PrintGetPageDimensionsReply)

    def PrintGetPageDimensionsUnchecked(self, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context))
        return self.send_request(xcb.Request(buf.getvalue(), 21, False, False),
                                 PrintGetPageDimensionsCookie(),
                                 PrintGetPageDimensionsReply)

    def PrintQueryScreens(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 22, False, True),
                                 PrintQueryScreensCookie(),
                                 PrintQueryScreensReply)

    def PrintQueryScreensUnchecked(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 22, False, False),
                                 PrintQueryScreensCookie(),
                                 PrintQueryScreensReply)

    def PrintSetImageResolution(self, context, image_resolution):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIH', context, image_resolution))
        return self.send_request(xcb.Request(buf.getvalue(), 23, False, True),
                                 PrintSetImageResolutionCookie(),
                                 PrintSetImageResolutionReply)

    def PrintSetImageResolutionUnchecked(self, context, image_resolution):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIH', context, image_resolution))
        return self.send_request(xcb.Request(buf.getvalue(), 23, False, False),
                                 PrintSetImageResolutionCookie(),
                                 PrintSetImageResolutionReply)

    def PrintGetImageResolution(self, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context))
        return self.send_request(xcb.Request(buf.getvalue(), 24, False, True),
                                 PrintGetImageResolutionCookie(),
                                 PrintGetImageResolutionReply)

    def PrintGetImageResolutionUnchecked(self, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context))
        return self.send_request(xcb.Request(buf.getvalue(), 24, False, False),
                                 PrintGetImageResolutionCookie(),
                                 PrintGetImageResolutionReply)

_events = {
    0 : NotifyEvent,
    1 : AttributNotifyEvent,
}

_errors = {
    0 : (ContextError, BadContext),
    1 : (SequenceError, BadSequence),
}

xcb._add_ext(key, xprintExtension, _events, _errors)
