#
# This file generated automatically from glx.xml by py_client.py.
# Edit at your peril.
#

import xcb
import cStringIO
from struct import pack, unpack_from
from array import array
import xproto

MAJOR_VERSION = 1
MINOR_VERSION = 3

key = xcb.ExtensionKey('GLX')

class GenericError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)
        count = 0
        (self.bad_value, self.minor_opcode, self.major_opcode,) = unpack_from('xx2xIHB21x', self, count)

class BadGeneric(xcb.ProtocolException):
    pass

class ContextError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)
        count = 0
        (self.bad_value, self.minor_opcode, self.major_opcode,) = unpack_from('xx2xIHB21x', self, count)

class BadContext(xcb.ProtocolException):
    pass

class ContextStateError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)
        count = 0
        (self.bad_value, self.minor_opcode, self.major_opcode,) = unpack_from('xx2xIHB21x', self, count)

class BadContextState(xcb.ProtocolException):
    pass

class DrawableError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)
        count = 0
        (self.bad_value, self.minor_opcode, self.major_opcode,) = unpack_from('xx2xIHB21x', self, count)

class BadDrawable(xcb.ProtocolException):
    pass

class PixmapError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)
        count = 0
        (self.bad_value, self.minor_opcode, self.major_opcode,) = unpack_from('xx2xIHB21x', self, count)

class BadPixmap(xcb.ProtocolException):
    pass

class ContextTagError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)
        count = 0
        (self.bad_value, self.minor_opcode, self.major_opcode,) = unpack_from('xx2xIHB21x', self, count)

class BadContextTag(xcb.ProtocolException):
    pass

class CurrentWindowError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)
        count = 0
        (self.bad_value, self.minor_opcode, self.major_opcode,) = unpack_from('xx2xIHB21x', self, count)

class BadCurrentWindow(xcb.ProtocolException):
    pass

class RenderRequestError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)
        count = 0
        (self.bad_value, self.minor_opcode, self.major_opcode,) = unpack_from('xx2xIHB21x', self, count)

class BadRenderRequest(xcb.ProtocolException):
    pass

class LargeRequestError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)
        count = 0
        (self.bad_value, self.minor_opcode, self.major_opcode,) = unpack_from('xx2xIHB21x', self, count)

class BadLargeRequest(xcb.ProtocolException):
    pass

class UnsupportedPrivateRequestError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)
        count = 0
        (self.bad_value, self.minor_opcode, self.major_opcode,) = unpack_from('xx2xIHB21x', self, count)

class BadUnsupportedPrivateRequest(xcb.ProtocolException):
    pass

class FBConfigError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)
        count = 0
        (self.bad_value, self.minor_opcode, self.major_opcode,) = unpack_from('xx2xIHB21x', self, count)

class BadFBConfig(xcb.ProtocolException):
    pass

class PbufferError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)
        count = 0
        (self.bad_value, self.minor_opcode, self.major_opcode,) = unpack_from('xx2xIHB21x', self, count)

class BadPbuffer(xcb.ProtocolException):
    pass

class CurrentDrawableError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)
        count = 0
        (self.bad_value, self.minor_opcode, self.major_opcode,) = unpack_from('xx2xIHB21x', self, count)

class BadCurrentDrawable(xcb.ProtocolException):
    pass

class WindowError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)
        count = 0
        (self.bad_value, self.minor_opcode, self.major_opcode,) = unpack_from('xx2xIHB21x', self, count)

class BadWindow(xcb.ProtocolException):
    pass

class PbufferClobberEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.event_type, self.draw_type, self.drawable, self.b_mask, self.aux_buffer, self.x, self.y, self.width, self.height, self.count,) = unpack_from('xx2xHHIIHHHHHH4x', self, count)

class PBCET:
    Damaged = 32791
    Saved = 32792

class PBCDT:
    Window = 32793
    Pbuffer = 32794

class MakeCurrentCookie(xcb.Cookie):
    pass

class MakeCurrentReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.context_tag,) = unpack_from('xx2x4xI20x', self, count)

class IsDirectCookie(xcb.Cookie):
    pass

class IsDirectReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.is_direct,) = unpack_from('xx2x4xB23x', self, count)

class QueryVersionCookie(xcb.Cookie):
    pass

class QueryVersionReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.major_version, self.minor_version,) = unpack_from('xx2x4xII16x', self, count)

class GC:
    GL_CURRENT_BIT = 1
    GL_POINT_BIT = 2
    GL_LINE_BIT = 4
    GL_POLYGON_BIT = 8
    GL_POLYGON_STIPPLE_BIT = 16
    GL_PIXEL_MODE_BIT = 32
    GL_LIGHTING_BIT = 64
    GL_FOG_BIT = 128
    GL_DEPTH_BUFFER_BIT = 256
    GL_ACCUM_BUFFER_BIT = 512
    GL_STENCIL_BUFFER_BIT = 1024
    GL_VIEWPORT_BIT = 2048
    GL_TRANSFORM_BIT = 4096
    GL_ENABLE_BIT = 8192
    GL_COLOR_BUFFER_BIT = 16384
    GL_HINT_BIT = 32768
    GL_EVAL_BIT = 65536
    GL_LIST_BIT = 131072
    GL_TEXTURE_BIT = 262144
    GL_SCISSOR_BIT = 524288
    GL_ALL_ATTRIB_BITS = 16777215

class GetVisualConfigsCookie(xcb.Cookie):
    pass

class GetVisualConfigsReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.num_visuals, self.num_properties,) = unpack_from('xx2x4xII16x', self, count)
        count += 32
        self.property_list = xcb.List(self, count, self.length, 'I', 4)

class VendorPrivateWithReplyCookie(xcb.Cookie):
    pass

class VendorPrivateWithReplyReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.retval,) = unpack_from('xx2x4xI', self, count)
        count += 12
        self.data1 = xcb.List(self, count, 24, 'B', 1)
        count += len(self.data1.buf())
        count += xcb.type_pad(1, count)
        self.data2 = xcb.List(self, count, (self.length * 4), 'B', 1)

class QueryExtensionsStringCookie(xcb.Cookie):
    pass

class QueryExtensionsStringReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.n,) = unpack_from('xx2x4x4xI16x', self, count)

class QueryServerStringCookie(xcb.Cookie):
    pass

class QueryServerStringReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.str_len,) = unpack_from('xx2x4x4xI16x', self, count)
        count += 32
        self.string = xcb.List(self, count, self.str_len, 'b', 1)

class GetFBConfigsCookie(xcb.Cookie):
    pass

class GetFBConfigsReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.num_FB_configs, self.num_properties,) = unpack_from('xx2x4xII16x', self, count)
        count += 32
        self.property_list = xcb.List(self, count, self.length, 'I', 4)

class QueryContextCookie(xcb.Cookie):
    pass

class QueryContextReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.num_attribs,) = unpack_from('xx2x4xI20x', self, count)
        count += 32
        self.attribs = xcb.List(self, count, (self.num_attribs * 2), 'I', 4)

class MakeContextCurrentCookie(xcb.Cookie):
    pass

class MakeContextCurrentReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.context_tag,) = unpack_from('xx2x4xI20x', self, count)

class GetDrawableAttributesCookie(xcb.Cookie):
    pass

class GetDrawableAttributesReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.num_attribs,) = unpack_from('xx2x4xI20x', self, count)
        count += 32
        self.attribs = xcb.List(self, count, (self.num_attribs * 2), 'I', 4)

class GenListsCookie(xcb.Cookie):
    pass

class GenListsReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.ret_val,) = unpack_from('xx2x4xI', self, count)

class RenderModeCookie(xcb.Cookie):
    pass

class RenderModeReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.ret_val, self.n, self.new_mode,) = unpack_from('xx2x4xIII12x', self, count)
        count += 32
        self.data = xcb.List(self, count, self.n, 'I', 4)

class RM:
    GL_RENDER = 7168
    GL_FEEDBACK = 7169
    GL_SELECT = 7170

class FinishCookie(xcb.Cookie):
    pass

class FinishReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)

class ReadPixelsCookie(xcb.Cookie):
    pass

class ReadPixelsReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        count += 32
        self.data = xcb.List(self, count, (self.length * 4), 'B', 1)

class GetBooleanvCookie(xcb.Cookie):
    pass

class GetBooleanvReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.n, self.datum,) = unpack_from('xx2x4x4xIB15x', self, count)
        count += 32
        self.data = xcb.List(self, count, self.n, 'B', 1)

class GetClipPlaneCookie(xcb.Cookie):
    pass

class GetClipPlaneReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        count += 32
        self.data = xcb.List(self, count, (self.length / 2), 'd', 8)

class GetDoublevCookie(xcb.Cookie):
    pass

class GetDoublevReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.n, self.datum,) = unpack_from('xx2x4x4xId8x', self, count)
        count += 32
        self.data = xcb.List(self, count, self.n, 'd', 8)

class GetErrorCookie(xcb.Cookie):
    pass

class GetErrorReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.error,) = unpack_from('xx2x4xi', self, count)

class GetFloatvCookie(xcb.Cookie):
    pass

class GetFloatvReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.n, self.datum,) = unpack_from('xx2x4x4xIf12x', self, count)
        count += 32
        self.data = xcb.List(self, count, self.n, 'f', 4)

class GetIntegervCookie(xcb.Cookie):
    pass

class GetIntegervReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.n, self.datum,) = unpack_from('xx2x4x4xIi12x', self, count)
        count += 32
        self.data = xcb.List(self, count, self.n, 'i', 4)

class GetLightfvCookie(xcb.Cookie):
    pass

class GetLightfvReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.n, self.datum,) = unpack_from('xx2x4x4xIf12x', self, count)
        count += 32
        self.data = xcb.List(self, count, self.n, 'f', 4)

class GetLightivCookie(xcb.Cookie):
    pass

class GetLightivReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.n, self.datum,) = unpack_from('xx2x4x4xIi12x', self, count)
        count += 32
        self.data = xcb.List(self, count, self.n, 'i', 4)

class GetMapdvCookie(xcb.Cookie):
    pass

class GetMapdvReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.n, self.datum,) = unpack_from('xx2x4x4xId8x', self, count)
        count += 32
        self.data = xcb.List(self, count, self.n, 'd', 8)

class GetMapfvCookie(xcb.Cookie):
    pass

class GetMapfvReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.n, self.datum,) = unpack_from('xx2x4x4xIf12x', self, count)
        count += 32
        self.data = xcb.List(self, count, self.n, 'f', 4)

class GetMapivCookie(xcb.Cookie):
    pass

class GetMapivReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.n, self.datum,) = unpack_from('xx2x4x4xIi12x', self, count)
        count += 32
        self.data = xcb.List(self, count, self.n, 'i', 4)

class GetMaterialfvCookie(xcb.Cookie):
    pass

class GetMaterialfvReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.n, self.datum,) = unpack_from('xx2x4x4xIf12x', self, count)
        count += 32
        self.data = xcb.List(self, count, self.n, 'f', 4)

class GetMaterialivCookie(xcb.Cookie):
    pass

class GetMaterialivReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.n, self.datum,) = unpack_from('xx2x4x4xIi12x', self, count)
        count += 32
        self.data = xcb.List(self, count, self.n, 'i', 4)

class GetPixelMapfvCookie(xcb.Cookie):
    pass

class GetPixelMapfvReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.n, self.datum,) = unpack_from('xx2x4x4xIf12x', self, count)
        count += 32
        self.data = xcb.List(self, count, self.n, 'f', 4)

class GetPixelMapuivCookie(xcb.Cookie):
    pass

class GetPixelMapuivReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.n, self.datum,) = unpack_from('xx2x4x4xII12x', self, count)
        count += 32
        self.data = xcb.List(self, count, self.n, 'I', 4)

class GetPixelMapusvCookie(xcb.Cookie):
    pass

class GetPixelMapusvReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.n, self.datum,) = unpack_from('xx2x4x4xIH16x', self, count)
        count += 34
        self.data = xcb.List(self, count, self.n, 'H', 2)

class GetPolygonStippleCookie(xcb.Cookie):
    pass

class GetPolygonStippleReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        count += 32
        self.data = xcb.List(self, count, (self.length * 4), 'B', 1)

class GetStringCookie(xcb.Cookie):
    pass

class GetStringReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.n,) = unpack_from('xx2x4x4xI16x', self, count)
        count += 32
        self.string = xcb.List(self, count, self.n, 'b', 1)

class GetTexEnvfvCookie(xcb.Cookie):
    pass

class GetTexEnvfvReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.n, self.datum,) = unpack_from('xx2x4x4xIf12x', self, count)
        count += 32
        self.data = xcb.List(self, count, self.n, 'f', 4)

class GetTexEnvivCookie(xcb.Cookie):
    pass

class GetTexEnvivReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.n, self.datum,) = unpack_from('xx2x4x4xIi12x', self, count)
        count += 32
        self.data = xcb.List(self, count, self.n, 'i', 4)

class GetTexGendvCookie(xcb.Cookie):
    pass

class GetTexGendvReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.n, self.datum,) = unpack_from('xx2x4x4xId8x', self, count)
        count += 32
        self.data = xcb.List(self, count, self.n, 'd', 8)

class GetTexGenfvCookie(xcb.Cookie):
    pass

class GetTexGenfvReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.n, self.datum,) = unpack_from('xx2x4x4xIf12x', self, count)
        count += 32
        self.data = xcb.List(self, count, self.n, 'f', 4)

class GetTexGenivCookie(xcb.Cookie):
    pass

class GetTexGenivReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.n, self.datum,) = unpack_from('xx2x4x4xIi12x', self, count)
        count += 32
        self.data = xcb.List(self, count, self.n, 'i', 4)

class GetTexImageCookie(xcb.Cookie):
    pass

class GetTexImageReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.width, self.height, self.depth,) = unpack_from('xx2x4x8xiii4x', self, count)
        count += 32
        self.data = xcb.List(self, count, (self.length * 4), 'B', 1)

class GetTexParameterfvCookie(xcb.Cookie):
    pass

class GetTexParameterfvReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.n, self.datum,) = unpack_from('xx2x4x4xIf12x', self, count)
        count += 32
        self.data = xcb.List(self, count, self.n, 'f', 4)

class GetTexParameterivCookie(xcb.Cookie):
    pass

class GetTexParameterivReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.n, self.datum,) = unpack_from('xx2x4x4xIi12x', self, count)
        count += 32
        self.data = xcb.List(self, count, self.n, 'i', 4)

class GetTexLevelParameterfvCookie(xcb.Cookie):
    pass

class GetTexLevelParameterfvReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.n, self.datum,) = unpack_from('xx2x4x4xIf12x', self, count)
        count += 32
        self.data = xcb.List(self, count, self.n, 'f', 4)

class GetTexLevelParameterivCookie(xcb.Cookie):
    pass

class GetTexLevelParameterivReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.n, self.datum,) = unpack_from('xx2x4x4xIi12x', self, count)
        count += 32
        self.data = xcb.List(self, count, self.n, 'i', 4)

class IsListCookie(xcb.Cookie):
    pass

class IsListReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.ret_val,) = unpack_from('xx2x4xI', self, count)

class AreTexturesResidentCookie(xcb.Cookie):
    pass

class AreTexturesResidentReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.ret_val,) = unpack_from('xx2x4xI20x', self, count)
        count += 32
        self.data = xcb.List(self, count, (self.length * 4), 'B', 1)

class GenTexturesCookie(xcb.Cookie):
    pass

class GenTexturesReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        count += 32
        self.data = xcb.List(self, count, self.length, 'I', 4)

class IsTextureCookie(xcb.Cookie):
    pass

class IsTextureReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.ret_val,) = unpack_from('xx2x4xI', self, count)

class GetColorTableCookie(xcb.Cookie):
    pass

class GetColorTableReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.width,) = unpack_from('xx2x4x8xi12x', self, count)
        count += 32
        self.data = xcb.List(self, count, (self.length * 4), 'B', 1)

class GetColorTableParameterfvCookie(xcb.Cookie):
    pass

class GetColorTableParameterfvReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.n, self.datum,) = unpack_from('xx2x4x4xIf12x', self, count)
        count += 32
        self.data = xcb.List(self, count, self.n, 'f', 4)

class GetColorTableParameterivCookie(xcb.Cookie):
    pass

class GetColorTableParameterivReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.n, self.datum,) = unpack_from('xx2x4x4xIi12x', self, count)
        count += 32
        self.data = xcb.List(self, count, self.n, 'i', 4)

class GetConvolutionFilterCookie(xcb.Cookie):
    pass

class GetConvolutionFilterReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.width, self.height,) = unpack_from('xx2x4x8xii8x', self, count)
        count += 32
        self.data = xcb.List(self, count, (self.length * 4), 'B', 1)

class GetConvolutionParameterfvCookie(xcb.Cookie):
    pass

class GetConvolutionParameterfvReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.n, self.datum,) = unpack_from('xx2x4x4xIf12x', self, count)
        count += 32
        self.data = xcb.List(self, count, self.n, 'f', 4)

class GetConvolutionParameterivCookie(xcb.Cookie):
    pass

class GetConvolutionParameterivReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.n, self.datum,) = unpack_from('xx2x4x4xIi12x', self, count)
        count += 32
        self.data = xcb.List(self, count, self.n, 'i', 4)

class GetSeparableFilterCookie(xcb.Cookie):
    pass

class GetSeparableFilterReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.row_w, self.col_h,) = unpack_from('xx2x4x8xii8x', self, count)
        count += 32
        self.rows_and_cols = xcb.List(self, count, (self.length * 4), 'B', 1)

class GetHistogramCookie(xcb.Cookie):
    pass

class GetHistogramReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.width,) = unpack_from('xx2x4x8xi12x', self, count)
        count += 32
        self.data = xcb.List(self, count, (self.length * 4), 'B', 1)

class GetHistogramParameterfvCookie(xcb.Cookie):
    pass

class GetHistogramParameterfvReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.n, self.datum,) = unpack_from('xx2x4x4xIf12x', self, count)
        count += 32
        self.data = xcb.List(self, count, self.n, 'f', 4)

class GetHistogramParameterivCookie(xcb.Cookie):
    pass

class GetHistogramParameterivReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.n, self.datum,) = unpack_from('xx2x4x4xIi12x', self, count)
        count += 32
        self.data = xcb.List(self, count, self.n, 'i', 4)

class GetMinmaxCookie(xcb.Cookie):
    pass

class GetMinmaxReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        count += 32
        self.data = xcb.List(self, count, (self.length * 4), 'B', 1)

class GetMinmaxParameterfvCookie(xcb.Cookie):
    pass

class GetMinmaxParameterfvReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.n, self.datum,) = unpack_from('xx2x4x4xIf12x', self, count)
        count += 32
        self.data = xcb.List(self, count, self.n, 'f', 4)

class GetMinmaxParameterivCookie(xcb.Cookie):
    pass

class GetMinmaxParameterivReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.n, self.datum,) = unpack_from('xx2x4x4xIi12x', self, count)
        count += 32
        self.data = xcb.List(self, count, self.n, 'i', 4)

class GetCompressedTexImageARBCookie(xcb.Cookie):
    pass

class GetCompressedTexImageARBReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.size,) = unpack_from('xx2x4x8xi12x', self, count)
        count += 32
        self.data = xcb.List(self, count, (self.length * 4), 'B', 1)

class GenQueriesARBCookie(xcb.Cookie):
    pass

class GenQueriesARBReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        count += 32
        self.data = xcb.List(self, count, self.length, 'I', 4)

class IsQueryARBCookie(xcb.Cookie):
    pass

class IsQueryARBReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.ret_val,) = unpack_from('xx2x4xI', self, count)

class GetQueryivARBCookie(xcb.Cookie):
    pass

class GetQueryivARBReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.n, self.datum,) = unpack_from('xx2x4x4xIi12x', self, count)
        count += 32
        self.data = xcb.List(self, count, self.n, 'i', 4)

class GetQueryObjectivARBCookie(xcb.Cookie):
    pass

class GetQueryObjectivARBReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.n, self.datum,) = unpack_from('xx2x4x4xIi12x', self, count)
        count += 32
        self.data = xcb.List(self, count, self.n, 'i', 4)

class GetQueryObjectuivARBCookie(xcb.Cookie):
    pass

class GetQueryObjectuivARBReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.n, self.datum,) = unpack_from('xx2x4x4xII12x', self, count)
        count += 32
        self.data = xcb.List(self, count, self.n, 'I', 4)

class glxExtension(xcb.Extension):

    def RenderChecked(self, context_tag, data_len, data):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context_tag))
        buf.write(str(buffer(array('B', data))))
        return self.send_request(xcb.Request(buf.getvalue(), 1, True, True),
                                 xcb.VoidCookie())

    def Render(self, context_tag, data_len, data):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context_tag))
        buf.write(str(buffer(array('B', data))))
        return self.send_request(xcb.Request(buf.getvalue(), 1, True, False),
                                 xcb.VoidCookie())

    def RenderLargeChecked(self, context_tag, request_num, request_total, data_len, data):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIHHI', context_tag, request_num, request_total, data_len))
        buf.write(str(buffer(array('B', data))))
        return self.send_request(xcb.Request(buf.getvalue(), 2, True, True),
                                 xcb.VoidCookie())

    def RenderLarge(self, context_tag, request_num, request_total, data_len, data):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIHHI', context_tag, request_num, request_total, data_len))
        buf.write(str(buffer(array('B', data))))
        return self.send_request(xcb.Request(buf.getvalue(), 2, True, False),
                                 xcb.VoidCookie())

    def CreateContextChecked(self, context, visual, screen, share_list, is_direct):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIIB3x', context, visual, screen, share_list, is_direct))
        return self.send_request(xcb.Request(buf.getvalue(), 3, True, True),
                                 xcb.VoidCookie())

    def CreateContext(self, context, visual, screen, share_list, is_direct):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIIB3x', context, visual, screen, share_list, is_direct))
        return self.send_request(xcb.Request(buf.getvalue(), 3, True, False),
                                 xcb.VoidCookie())

    def DestroyContextChecked(self, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context))
        return self.send_request(xcb.Request(buf.getvalue(), 4, True, True),
                                 xcb.VoidCookie())

    def DestroyContext(self, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context))
        return self.send_request(xcb.Request(buf.getvalue(), 4, True, False),
                                 xcb.VoidCookie())

    def MakeCurrent(self, drawable, context, old_context_tag):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', drawable, context, old_context_tag))
        return self.send_request(xcb.Request(buf.getvalue(), 5, False, True),
                                 MakeCurrentCookie(),
                                 MakeCurrentReply)

    def MakeCurrentUnchecked(self, drawable, context, old_context_tag):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', drawable, context, old_context_tag))
        return self.send_request(xcb.Request(buf.getvalue(), 5, False, False),
                                 MakeCurrentCookie(),
                                 MakeCurrentReply)

    def IsDirect(self, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context))
        return self.send_request(xcb.Request(buf.getvalue(), 6, False, True),
                                 IsDirectCookie(),
                                 IsDirectReply)

    def IsDirectUnchecked(self, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context))
        return self.send_request(xcb.Request(buf.getvalue(), 6, False, False),
                                 IsDirectCookie(),
                                 IsDirectReply)

    def QueryVersion(self, major_version, minor_version):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', major_version, minor_version))
        return self.send_request(xcb.Request(buf.getvalue(), 7, False, True),
                                 QueryVersionCookie(),
                                 QueryVersionReply)

    def QueryVersionUnchecked(self, major_version, minor_version):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', major_version, minor_version))
        return self.send_request(xcb.Request(buf.getvalue(), 7, False, False),
                                 QueryVersionCookie(),
                                 QueryVersionReply)

    def WaitGLChecked(self, context_tag):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context_tag))
        return self.send_request(xcb.Request(buf.getvalue(), 8, True, True),
                                 xcb.VoidCookie())

    def WaitGL(self, context_tag):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context_tag))
        return self.send_request(xcb.Request(buf.getvalue(), 8, True, False),
                                 xcb.VoidCookie())

    def WaitXChecked(self, context_tag):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context_tag))
        return self.send_request(xcb.Request(buf.getvalue(), 9, True, True),
                                 xcb.VoidCookie())

    def WaitX(self, context_tag):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context_tag))
        return self.send_request(xcb.Request(buf.getvalue(), 9, True, False),
                                 xcb.VoidCookie())

    def CopyContextChecked(self, src, dest, mask, src_context_tag):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIII', src, dest, mask, src_context_tag))
        return self.send_request(xcb.Request(buf.getvalue(), 10, True, True),
                                 xcb.VoidCookie())

    def CopyContext(self, src, dest, mask, src_context_tag):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIII', src, dest, mask, src_context_tag))
        return self.send_request(xcb.Request(buf.getvalue(), 10, True, False),
                                 xcb.VoidCookie())

    def SwapBuffersChecked(self, context_tag, drawable):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', context_tag, drawable))
        return self.send_request(xcb.Request(buf.getvalue(), 11, True, True),
                                 xcb.VoidCookie())

    def SwapBuffers(self, context_tag, drawable):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', context_tag, drawable))
        return self.send_request(xcb.Request(buf.getvalue(), 11, True, False),
                                 xcb.VoidCookie())

    def UseXFontChecked(self, context_tag, font, first, count, list_base):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIII', context_tag, font, first, count, list_base))
        return self.send_request(xcb.Request(buf.getvalue(), 12, True, True),
                                 xcb.VoidCookie())

    def UseXFont(self, context_tag, font, first, count, list_base):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIII', context_tag, font, first, count, list_base))
        return self.send_request(xcb.Request(buf.getvalue(), 12, True, False),
                                 xcb.VoidCookie())

    def CreateGLXPixmapChecked(self, screen, visual, pixmap, glx_pixmap):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIII', screen, visual, pixmap, glx_pixmap))
        return self.send_request(xcb.Request(buf.getvalue(), 13, True, True),
                                 xcb.VoidCookie())

    def CreateGLXPixmap(self, screen, visual, pixmap, glx_pixmap):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIII', screen, visual, pixmap, glx_pixmap))
        return self.send_request(xcb.Request(buf.getvalue(), 13, True, False),
                                 xcb.VoidCookie())

    def GetVisualConfigs(self, screen):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', screen))
        return self.send_request(xcb.Request(buf.getvalue(), 14, False, True),
                                 GetVisualConfigsCookie(),
                                 GetVisualConfigsReply)

    def GetVisualConfigsUnchecked(self, screen):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', screen))
        return self.send_request(xcb.Request(buf.getvalue(), 14, False, False),
                                 GetVisualConfigsCookie(),
                                 GetVisualConfigsReply)

    def DestroyGLXPixmapChecked(self, glx_pixmap):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', glx_pixmap))
        return self.send_request(xcb.Request(buf.getvalue(), 15, True, True),
                                 xcb.VoidCookie())

    def DestroyGLXPixmap(self, glx_pixmap):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', glx_pixmap))
        return self.send_request(xcb.Request(buf.getvalue(), 15, True, False),
                                 xcb.VoidCookie())

    def VendorPrivateChecked(self, vendor_code, context_tag, data_len, data):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', vendor_code, context_tag))
        buf.write(str(buffer(array('B', data))))
        return self.send_request(xcb.Request(buf.getvalue(), 16, True, True),
                                 xcb.VoidCookie())

    def VendorPrivate(self, vendor_code, context_tag, data_len, data):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', vendor_code, context_tag))
        buf.write(str(buffer(array('B', data))))
        return self.send_request(xcb.Request(buf.getvalue(), 16, True, False),
                                 xcb.VoidCookie())

    def VendorPrivateWithReply(self, vendor_code, context_tag, data_len, data):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', vendor_code, context_tag))
        buf.write(str(buffer(array('B', data))))
        return self.send_request(xcb.Request(buf.getvalue(), 17, False, True),
                                 VendorPrivateWithReplyCookie(),
                                 VendorPrivateWithReplyReply)

    def VendorPrivateWithReplyUnchecked(self, vendor_code, context_tag, data_len, data):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', vendor_code, context_tag))
        buf.write(str(buffer(array('B', data))))
        return self.send_request(xcb.Request(buf.getvalue(), 17, False, False),
                                 VendorPrivateWithReplyCookie(),
                                 VendorPrivateWithReplyReply)

    def QueryExtensionsString(self, screen):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', screen))
        return self.send_request(xcb.Request(buf.getvalue(), 18, False, True),
                                 QueryExtensionsStringCookie(),
                                 QueryExtensionsStringReply)

    def QueryExtensionsStringUnchecked(self, screen):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', screen))
        return self.send_request(xcb.Request(buf.getvalue(), 18, False, False),
                                 QueryExtensionsStringCookie(),
                                 QueryExtensionsStringReply)

    def QueryServerString(self, screen, name):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', screen, name))
        return self.send_request(xcb.Request(buf.getvalue(), 19, False, True),
                                 QueryServerStringCookie(),
                                 QueryServerStringReply)

    def QueryServerStringUnchecked(self, screen, name):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', screen, name))
        return self.send_request(xcb.Request(buf.getvalue(), 19, False, False),
                                 QueryServerStringCookie(),
                                 QueryServerStringReply)

    def ClientInfoChecked(self, major_version, minor_version, str_len, string):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', major_version, minor_version, str_len))
        buf.write(str(buffer(array('b', string))))
        return self.send_request(xcb.Request(buf.getvalue(), 20, True, True),
                                 xcb.VoidCookie())

    def ClientInfo(self, major_version, minor_version, str_len, string):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', major_version, minor_version, str_len))
        buf.write(str(buffer(array('b', string))))
        return self.send_request(xcb.Request(buf.getvalue(), 20, True, False),
                                 xcb.VoidCookie())

    def GetFBConfigs(self, screen):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', screen))
        return self.send_request(xcb.Request(buf.getvalue(), 21, False, True),
                                 GetFBConfigsCookie(),
                                 GetFBConfigsReply)

    def GetFBConfigsUnchecked(self, screen):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', screen))
        return self.send_request(xcb.Request(buf.getvalue(), 21, False, False),
                                 GetFBConfigsCookie(),
                                 GetFBConfigsReply)

    def CreatePixmapChecked(self, screen, fbconfig, pixmap, glx_pixmap, num_attribs, attribs):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIII', screen, fbconfig, pixmap, glx_pixmap, num_attribs))
        buf.write(str(buffer(array('I', attribs))))
        return self.send_request(xcb.Request(buf.getvalue(), 22, True, True),
                                 xcb.VoidCookie())

    def CreatePixmap(self, screen, fbconfig, pixmap, glx_pixmap, num_attribs, attribs):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIII', screen, fbconfig, pixmap, glx_pixmap, num_attribs))
        buf.write(str(buffer(array('I', attribs))))
        return self.send_request(xcb.Request(buf.getvalue(), 22, True, False),
                                 xcb.VoidCookie())

    def DestroyPixmapChecked(self, glx_pixmap):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', glx_pixmap))
        return self.send_request(xcb.Request(buf.getvalue(), 23, True, True),
                                 xcb.VoidCookie())

    def DestroyPixmap(self, glx_pixmap):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', glx_pixmap))
        return self.send_request(xcb.Request(buf.getvalue(), 23, True, False),
                                 xcb.VoidCookie())

    def CreateNewContextChecked(self, context, fbconfig, screen, render_type, share_list, is_direct, reserved1, reserved2):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIIIBBH', context, fbconfig, screen, render_type, share_list, is_direct, reserved1, reserved2))
        return self.send_request(xcb.Request(buf.getvalue(), 24, True, True),
                                 xcb.VoidCookie())

    def CreateNewContext(self, context, fbconfig, screen, render_type, share_list, is_direct, reserved1, reserved2):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIIIBBH', context, fbconfig, screen, render_type, share_list, is_direct, reserved1, reserved2))
        return self.send_request(xcb.Request(buf.getvalue(), 24, True, False),
                                 xcb.VoidCookie())

    def QueryContext(self, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context))
        return self.send_request(xcb.Request(buf.getvalue(), 25, False, True),
                                 QueryContextCookie(),
                                 QueryContextReply)

    def QueryContextUnchecked(self, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context))
        return self.send_request(xcb.Request(buf.getvalue(), 25, False, False),
                                 QueryContextCookie(),
                                 QueryContextReply)

    def MakeContextCurrent(self, old_context_tag, drawable, read_drawable, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIII', old_context_tag, drawable, read_drawable, context))
        return self.send_request(xcb.Request(buf.getvalue(), 26, False, True),
                                 MakeContextCurrentCookie(),
                                 MakeContextCurrentReply)

    def MakeContextCurrentUnchecked(self, old_context_tag, drawable, read_drawable, context):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIII', old_context_tag, drawable, read_drawable, context))
        return self.send_request(xcb.Request(buf.getvalue(), 26, False, False),
                                 MakeContextCurrentCookie(),
                                 MakeContextCurrentReply)

    def CreatePbufferChecked(self, screen, fbconfig, pbuffer, num_attribs, attribs):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIII', screen, fbconfig, pbuffer, num_attribs))
        buf.write(str(buffer(array('I', attribs))))
        return self.send_request(xcb.Request(buf.getvalue(), 27, True, True),
                                 xcb.VoidCookie())

    def CreatePbuffer(self, screen, fbconfig, pbuffer, num_attribs, attribs):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIII', screen, fbconfig, pbuffer, num_attribs))
        buf.write(str(buffer(array('I', attribs))))
        return self.send_request(xcb.Request(buf.getvalue(), 27, True, False),
                                 xcb.VoidCookie())

    def DestroyPbufferChecked(self, pbuffer):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', pbuffer))
        return self.send_request(xcb.Request(buf.getvalue(), 28, True, True),
                                 xcb.VoidCookie())

    def DestroyPbuffer(self, pbuffer):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', pbuffer))
        return self.send_request(xcb.Request(buf.getvalue(), 28, True, False),
                                 xcb.VoidCookie())

    def GetDrawableAttributes(self, drawable):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', drawable))
        return self.send_request(xcb.Request(buf.getvalue(), 29, False, True),
                                 GetDrawableAttributesCookie(),
                                 GetDrawableAttributesReply)

    def GetDrawableAttributesUnchecked(self, drawable):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', drawable))
        return self.send_request(xcb.Request(buf.getvalue(), 29, False, False),
                                 GetDrawableAttributesCookie(),
                                 GetDrawableAttributesReply)

    def ChangeDrawableAttributesChecked(self, drawable, num_attribs, attribs):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', drawable, num_attribs))
        buf.write(str(buffer(array('I', attribs))))
        return self.send_request(xcb.Request(buf.getvalue(), 30, True, True),
                                 xcb.VoidCookie())

    def ChangeDrawableAttributes(self, drawable, num_attribs, attribs):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', drawable, num_attribs))
        buf.write(str(buffer(array('I', attribs))))
        return self.send_request(xcb.Request(buf.getvalue(), 30, True, False),
                                 xcb.VoidCookie())

    def CreateWindowChecked(self, screen, fbconfig, window, glx_window, num_attribs, attribs):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIII', screen, fbconfig, window, glx_window, num_attribs))
        buf.write(str(buffer(array('I', attribs))))
        return self.send_request(xcb.Request(buf.getvalue(), 31, True, True),
                                 xcb.VoidCookie())

    def CreateWindow(self, screen, fbconfig, window, glx_window, num_attribs, attribs):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIII', screen, fbconfig, window, glx_window, num_attribs))
        buf.write(str(buffer(array('I', attribs))))
        return self.send_request(xcb.Request(buf.getvalue(), 31, True, False),
                                 xcb.VoidCookie())

    def DeleteWindowChecked(self, glxwindow):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', glxwindow))
        return self.send_request(xcb.Request(buf.getvalue(), 32, True, True),
                                 xcb.VoidCookie())

    def DeleteWindow(self, glxwindow):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', glxwindow))
        return self.send_request(xcb.Request(buf.getvalue(), 32, True, False),
                                 xcb.VoidCookie())

    def NewListChecked(self, context_tag, list, mode):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, list, mode))
        return self.send_request(xcb.Request(buf.getvalue(), 101, True, True),
                                 xcb.VoidCookie())

    def NewList(self, context_tag, list, mode):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, list, mode))
        return self.send_request(xcb.Request(buf.getvalue(), 101, True, False),
                                 xcb.VoidCookie())

    def EndListChecked(self, context_tag):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context_tag))
        return self.send_request(xcb.Request(buf.getvalue(), 102, True, True),
                                 xcb.VoidCookie())

    def EndList(self, context_tag):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context_tag))
        return self.send_request(xcb.Request(buf.getvalue(), 102, True, False),
                                 xcb.VoidCookie())

    def DeleteListsChecked(self, context_tag, list, range):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIi', context_tag, list, range))
        return self.send_request(xcb.Request(buf.getvalue(), 103, True, True),
                                 xcb.VoidCookie())

    def DeleteLists(self, context_tag, list, range):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIi', context_tag, list, range))
        return self.send_request(xcb.Request(buf.getvalue(), 103, True, False),
                                 xcb.VoidCookie())

    def GenLists(self, context_tag, range):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIi', context_tag, range))
        return self.send_request(xcb.Request(buf.getvalue(), 104, False, True),
                                 GenListsCookie(),
                                 GenListsReply)

    def GenListsUnchecked(self, context_tag, range):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIi', context_tag, range))
        return self.send_request(xcb.Request(buf.getvalue(), 104, False, False),
                                 GenListsCookie(),
                                 GenListsReply)

    def FeedbackBufferChecked(self, context_tag, size, type):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIii', context_tag, size, type))
        return self.send_request(xcb.Request(buf.getvalue(), 105, True, True),
                                 xcb.VoidCookie())

    def FeedbackBuffer(self, context_tag, size, type):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIii', context_tag, size, type))
        return self.send_request(xcb.Request(buf.getvalue(), 105, True, False),
                                 xcb.VoidCookie())

    def SelectBufferChecked(self, context_tag, size):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIi', context_tag, size))
        return self.send_request(xcb.Request(buf.getvalue(), 106, True, True),
                                 xcb.VoidCookie())

    def SelectBuffer(self, context_tag, size):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIi', context_tag, size))
        return self.send_request(xcb.Request(buf.getvalue(), 106, True, False),
                                 xcb.VoidCookie())

    def RenderMode(self, context_tag, mode):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', context_tag, mode))
        return self.send_request(xcb.Request(buf.getvalue(), 107, False, True),
                                 RenderModeCookie(),
                                 RenderModeReply)

    def RenderModeUnchecked(self, context_tag, mode):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', context_tag, mode))
        return self.send_request(xcb.Request(buf.getvalue(), 107, False, False),
                                 RenderModeCookie(),
                                 RenderModeReply)

    def Finish(self, context_tag):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context_tag))
        return self.send_request(xcb.Request(buf.getvalue(), 108, False, True),
                                 FinishCookie(),
                                 FinishReply)

    def FinishUnchecked(self, context_tag):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context_tag))
        return self.send_request(xcb.Request(buf.getvalue(), 108, False, False),
                                 FinishCookie(),
                                 FinishReply)

    def PixelStorefChecked(self, context_tag, pname, datum):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIf', context_tag, pname, datum))
        return self.send_request(xcb.Request(buf.getvalue(), 109, True, True),
                                 xcb.VoidCookie())

    def PixelStoref(self, context_tag, pname, datum):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIf', context_tag, pname, datum))
        return self.send_request(xcb.Request(buf.getvalue(), 109, True, False),
                                 xcb.VoidCookie())

    def PixelStoreiChecked(self, context_tag, pname, datum):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIi', context_tag, pname, datum))
        return self.send_request(xcb.Request(buf.getvalue(), 110, True, True),
                                 xcb.VoidCookie())

    def PixelStorei(self, context_tag, pname, datum):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIi', context_tag, pname, datum))
        return self.send_request(xcb.Request(buf.getvalue(), 110, True, False),
                                 xcb.VoidCookie())

    def ReadPixels(self, context_tag, x, y, width, height, format, type, swap_bytes, lsb_first):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIiiiiIIBB', context_tag, x, y, width, height, format, type, swap_bytes, lsb_first))
        return self.send_request(xcb.Request(buf.getvalue(), 111, False, True),
                                 ReadPixelsCookie(),
                                 ReadPixelsReply)

    def ReadPixelsUnchecked(self, context_tag, x, y, width, height, format, type, swap_bytes, lsb_first):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIiiiiIIBB', context_tag, x, y, width, height, format, type, swap_bytes, lsb_first))
        return self.send_request(xcb.Request(buf.getvalue(), 111, False, False),
                                 ReadPixelsCookie(),
                                 ReadPixelsReply)

    def GetBooleanv(self, context_tag, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIi', context_tag, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 112, False, True),
                                 GetBooleanvCookie(),
                                 GetBooleanvReply)

    def GetBooleanvUnchecked(self, context_tag, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIi', context_tag, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 112, False, False),
                                 GetBooleanvCookie(),
                                 GetBooleanvReply)

    def GetClipPlane(self, context_tag, plane):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIi', context_tag, plane))
        return self.send_request(xcb.Request(buf.getvalue(), 113, False, True),
                                 GetClipPlaneCookie(),
                                 GetClipPlaneReply)

    def GetClipPlaneUnchecked(self, context_tag, plane):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIi', context_tag, plane))
        return self.send_request(xcb.Request(buf.getvalue(), 113, False, False),
                                 GetClipPlaneCookie(),
                                 GetClipPlaneReply)

    def GetDoublev(self, context_tag, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', context_tag, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 114, False, True),
                                 GetDoublevCookie(),
                                 GetDoublevReply)

    def GetDoublevUnchecked(self, context_tag, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', context_tag, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 114, False, False),
                                 GetDoublevCookie(),
                                 GetDoublevReply)

    def GetError(self, context_tag):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context_tag))
        return self.send_request(xcb.Request(buf.getvalue(), 115, False, True),
                                 GetErrorCookie(),
                                 GetErrorReply)

    def GetErrorUnchecked(self, context_tag):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context_tag))
        return self.send_request(xcb.Request(buf.getvalue(), 115, False, False),
                                 GetErrorCookie(),
                                 GetErrorReply)

    def GetFloatv(self, context_tag, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', context_tag, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 116, False, True),
                                 GetFloatvCookie(),
                                 GetFloatvReply)

    def GetFloatvUnchecked(self, context_tag, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', context_tag, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 116, False, False),
                                 GetFloatvCookie(),
                                 GetFloatvReply)

    def GetIntegerv(self, context_tag, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', context_tag, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 117, False, True),
                                 GetIntegervCookie(),
                                 GetIntegervReply)

    def GetIntegervUnchecked(self, context_tag, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', context_tag, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 117, False, False),
                                 GetIntegervCookie(),
                                 GetIntegervReply)

    def GetLightfv(self, context_tag, light, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, light, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 118, False, True),
                                 GetLightfvCookie(),
                                 GetLightfvReply)

    def GetLightfvUnchecked(self, context_tag, light, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, light, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 118, False, False),
                                 GetLightfvCookie(),
                                 GetLightfvReply)

    def GetLightiv(self, context_tag, light, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, light, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 119, False, True),
                                 GetLightivCookie(),
                                 GetLightivReply)

    def GetLightivUnchecked(self, context_tag, light, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, light, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 119, False, False),
                                 GetLightivCookie(),
                                 GetLightivReply)

    def GetMapdv(self, context_tag, target, query):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, target, query))
        return self.send_request(xcb.Request(buf.getvalue(), 120, False, True),
                                 GetMapdvCookie(),
                                 GetMapdvReply)

    def GetMapdvUnchecked(self, context_tag, target, query):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, target, query))
        return self.send_request(xcb.Request(buf.getvalue(), 120, False, False),
                                 GetMapdvCookie(),
                                 GetMapdvReply)

    def GetMapfv(self, context_tag, target, query):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, target, query))
        return self.send_request(xcb.Request(buf.getvalue(), 121, False, True),
                                 GetMapfvCookie(),
                                 GetMapfvReply)

    def GetMapfvUnchecked(self, context_tag, target, query):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, target, query))
        return self.send_request(xcb.Request(buf.getvalue(), 121, False, False),
                                 GetMapfvCookie(),
                                 GetMapfvReply)

    def GetMapiv(self, context_tag, target, query):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, target, query))
        return self.send_request(xcb.Request(buf.getvalue(), 122, False, True),
                                 GetMapivCookie(),
                                 GetMapivReply)

    def GetMapivUnchecked(self, context_tag, target, query):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, target, query))
        return self.send_request(xcb.Request(buf.getvalue(), 122, False, False),
                                 GetMapivCookie(),
                                 GetMapivReply)

    def GetMaterialfv(self, context_tag, face, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, face, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 123, False, True),
                                 GetMaterialfvCookie(),
                                 GetMaterialfvReply)

    def GetMaterialfvUnchecked(self, context_tag, face, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, face, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 123, False, False),
                                 GetMaterialfvCookie(),
                                 GetMaterialfvReply)

    def GetMaterialiv(self, context_tag, face, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, face, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 124, False, True),
                                 GetMaterialivCookie(),
                                 GetMaterialivReply)

    def GetMaterialivUnchecked(self, context_tag, face, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, face, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 124, False, False),
                                 GetMaterialivCookie(),
                                 GetMaterialivReply)

    def GetPixelMapfv(self, context_tag, map):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', context_tag, map))
        return self.send_request(xcb.Request(buf.getvalue(), 125, False, True),
                                 GetPixelMapfvCookie(),
                                 GetPixelMapfvReply)

    def GetPixelMapfvUnchecked(self, context_tag, map):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', context_tag, map))
        return self.send_request(xcb.Request(buf.getvalue(), 125, False, False),
                                 GetPixelMapfvCookie(),
                                 GetPixelMapfvReply)

    def GetPixelMapuiv(self, context_tag, map):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', context_tag, map))
        return self.send_request(xcb.Request(buf.getvalue(), 126, False, True),
                                 GetPixelMapuivCookie(),
                                 GetPixelMapuivReply)

    def GetPixelMapuivUnchecked(self, context_tag, map):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', context_tag, map))
        return self.send_request(xcb.Request(buf.getvalue(), 126, False, False),
                                 GetPixelMapuivCookie(),
                                 GetPixelMapuivReply)

    def GetPixelMapusv(self, context_tag, map):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', context_tag, map))
        return self.send_request(xcb.Request(buf.getvalue(), 127, False, True),
                                 GetPixelMapusvCookie(),
                                 GetPixelMapusvReply)

    def GetPixelMapusvUnchecked(self, context_tag, map):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', context_tag, map))
        return self.send_request(xcb.Request(buf.getvalue(), 127, False, False),
                                 GetPixelMapusvCookie(),
                                 GetPixelMapusvReply)

    def GetPolygonStipple(self, context_tag, lsb_first):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIB', context_tag, lsb_first))
        return self.send_request(xcb.Request(buf.getvalue(), 128, False, True),
                                 GetPolygonStippleCookie(),
                                 GetPolygonStippleReply)

    def GetPolygonStippleUnchecked(self, context_tag, lsb_first):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIB', context_tag, lsb_first))
        return self.send_request(xcb.Request(buf.getvalue(), 128, False, False),
                                 GetPolygonStippleCookie(),
                                 GetPolygonStippleReply)

    def GetString(self, context_tag, name):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', context_tag, name))
        return self.send_request(xcb.Request(buf.getvalue(), 129, False, True),
                                 GetStringCookie(),
                                 GetStringReply)

    def GetStringUnchecked(self, context_tag, name):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', context_tag, name))
        return self.send_request(xcb.Request(buf.getvalue(), 129, False, False),
                                 GetStringCookie(),
                                 GetStringReply)

    def GetTexEnvfv(self, context_tag, target, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, target, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 130, False, True),
                                 GetTexEnvfvCookie(),
                                 GetTexEnvfvReply)

    def GetTexEnvfvUnchecked(self, context_tag, target, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, target, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 130, False, False),
                                 GetTexEnvfvCookie(),
                                 GetTexEnvfvReply)

    def GetTexEnviv(self, context_tag, target, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, target, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 131, False, True),
                                 GetTexEnvivCookie(),
                                 GetTexEnvivReply)

    def GetTexEnvivUnchecked(self, context_tag, target, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, target, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 131, False, False),
                                 GetTexEnvivCookie(),
                                 GetTexEnvivReply)

    def GetTexGendv(self, context_tag, coord, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, coord, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 132, False, True),
                                 GetTexGendvCookie(),
                                 GetTexGendvReply)

    def GetTexGendvUnchecked(self, context_tag, coord, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, coord, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 132, False, False),
                                 GetTexGendvCookie(),
                                 GetTexGendvReply)

    def GetTexGenfv(self, context_tag, coord, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, coord, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 133, False, True),
                                 GetTexGenfvCookie(),
                                 GetTexGenfvReply)

    def GetTexGenfvUnchecked(self, context_tag, coord, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, coord, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 133, False, False),
                                 GetTexGenfvCookie(),
                                 GetTexGenfvReply)

    def GetTexGeniv(self, context_tag, coord, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, coord, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 134, False, True),
                                 GetTexGenivCookie(),
                                 GetTexGenivReply)

    def GetTexGenivUnchecked(self, context_tag, coord, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, coord, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 134, False, False),
                                 GetTexGenivCookie(),
                                 GetTexGenivReply)

    def GetTexImage(self, context_tag, target, level, format, type, swap_bytes):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIiIIB', context_tag, target, level, format, type, swap_bytes))
        return self.send_request(xcb.Request(buf.getvalue(), 135, False, True),
                                 GetTexImageCookie(),
                                 GetTexImageReply)

    def GetTexImageUnchecked(self, context_tag, target, level, format, type, swap_bytes):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIiIIB', context_tag, target, level, format, type, swap_bytes))
        return self.send_request(xcb.Request(buf.getvalue(), 135, False, False),
                                 GetTexImageCookie(),
                                 GetTexImageReply)

    def GetTexParameterfv(self, context_tag, target, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, target, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 136, False, True),
                                 GetTexParameterfvCookie(),
                                 GetTexParameterfvReply)

    def GetTexParameterfvUnchecked(self, context_tag, target, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, target, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 136, False, False),
                                 GetTexParameterfvCookie(),
                                 GetTexParameterfvReply)

    def GetTexParameteriv(self, context_tag, target, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, target, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 137, False, True),
                                 GetTexParameterivCookie(),
                                 GetTexParameterivReply)

    def GetTexParameterivUnchecked(self, context_tag, target, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, target, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 137, False, False),
                                 GetTexParameterivCookie(),
                                 GetTexParameterivReply)

    def GetTexLevelParameterfv(self, context_tag, target, level, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIiI', context_tag, target, level, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 138, False, True),
                                 GetTexLevelParameterfvCookie(),
                                 GetTexLevelParameterfvReply)

    def GetTexLevelParameterfvUnchecked(self, context_tag, target, level, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIiI', context_tag, target, level, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 138, False, False),
                                 GetTexLevelParameterfvCookie(),
                                 GetTexLevelParameterfvReply)

    def GetTexLevelParameteriv(self, context_tag, target, level, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIiI', context_tag, target, level, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 139, False, True),
                                 GetTexLevelParameterivCookie(),
                                 GetTexLevelParameterivReply)

    def GetTexLevelParameterivUnchecked(self, context_tag, target, level, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIiI', context_tag, target, level, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 139, False, False),
                                 GetTexLevelParameterivCookie(),
                                 GetTexLevelParameterivReply)

    def IsList(self, context_tag, list):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', context_tag, list))
        return self.send_request(xcb.Request(buf.getvalue(), 141, False, True),
                                 IsListCookie(),
                                 IsListReply)

    def IsListUnchecked(self, context_tag, list):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', context_tag, list))
        return self.send_request(xcb.Request(buf.getvalue(), 141, False, False),
                                 IsListCookie(),
                                 IsListReply)

    def FlushChecked(self, context_tag):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context_tag))
        return self.send_request(xcb.Request(buf.getvalue(), 142, True, True),
                                 xcb.VoidCookie())

    def Flush(self, context_tag):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', context_tag))
        return self.send_request(xcb.Request(buf.getvalue(), 142, True, False),
                                 xcb.VoidCookie())

    def AreTexturesResident(self, context_tag, n, textures):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIi', context_tag, n))
        buf.write(str(buffer(array('I', textures))))
        return self.send_request(xcb.Request(buf.getvalue(), 143, False, True),
                                 AreTexturesResidentCookie(),
                                 AreTexturesResidentReply)

    def AreTexturesResidentUnchecked(self, context_tag, n, textures):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIi', context_tag, n))
        buf.write(str(buffer(array('I', textures))))
        return self.send_request(xcb.Request(buf.getvalue(), 143, False, False),
                                 AreTexturesResidentCookie(),
                                 AreTexturesResidentReply)

    def DeleteTexturesChecked(self, context_tag, n, textures):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIi', context_tag, n))
        buf.write(str(buffer(array('I', textures))))
        return self.send_request(xcb.Request(buf.getvalue(), 144, True, True),
                                 xcb.VoidCookie())

    def DeleteTextures(self, context_tag, n, textures):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIi', context_tag, n))
        buf.write(str(buffer(array('I', textures))))
        return self.send_request(xcb.Request(buf.getvalue(), 144, True, False),
                                 xcb.VoidCookie())

    def GenTextures(self, context_tag, n):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIi', context_tag, n))
        return self.send_request(xcb.Request(buf.getvalue(), 145, False, True),
                                 GenTexturesCookie(),
                                 GenTexturesReply)

    def GenTexturesUnchecked(self, context_tag, n):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIi', context_tag, n))
        return self.send_request(xcb.Request(buf.getvalue(), 145, False, False),
                                 GenTexturesCookie(),
                                 GenTexturesReply)

    def IsTexture(self, context_tag, texture):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', context_tag, texture))
        return self.send_request(xcb.Request(buf.getvalue(), 146, False, True),
                                 IsTextureCookie(),
                                 IsTextureReply)

    def IsTextureUnchecked(self, context_tag, texture):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', context_tag, texture))
        return self.send_request(xcb.Request(buf.getvalue(), 146, False, False),
                                 IsTextureCookie(),
                                 IsTextureReply)

    def GetColorTable(self, context_tag, target, format, type, swap_bytes):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIIB', context_tag, target, format, type, swap_bytes))
        return self.send_request(xcb.Request(buf.getvalue(), 147, False, True),
                                 GetColorTableCookie(),
                                 GetColorTableReply)

    def GetColorTableUnchecked(self, context_tag, target, format, type, swap_bytes):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIIB', context_tag, target, format, type, swap_bytes))
        return self.send_request(xcb.Request(buf.getvalue(), 147, False, False),
                                 GetColorTableCookie(),
                                 GetColorTableReply)

    def GetColorTableParameterfv(self, context_tag, target, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, target, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 148, False, True),
                                 GetColorTableParameterfvCookie(),
                                 GetColorTableParameterfvReply)

    def GetColorTableParameterfvUnchecked(self, context_tag, target, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, target, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 148, False, False),
                                 GetColorTableParameterfvCookie(),
                                 GetColorTableParameterfvReply)

    def GetColorTableParameteriv(self, context_tag, target, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, target, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 149, False, True),
                                 GetColorTableParameterivCookie(),
                                 GetColorTableParameterivReply)

    def GetColorTableParameterivUnchecked(self, context_tag, target, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, target, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 149, False, False),
                                 GetColorTableParameterivCookie(),
                                 GetColorTableParameterivReply)

    def GetConvolutionFilter(self, context_tag, target, format, type, swap_bytes):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIIB', context_tag, target, format, type, swap_bytes))
        return self.send_request(xcb.Request(buf.getvalue(), 150, False, True),
                                 GetConvolutionFilterCookie(),
                                 GetConvolutionFilterReply)

    def GetConvolutionFilterUnchecked(self, context_tag, target, format, type, swap_bytes):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIIB', context_tag, target, format, type, swap_bytes))
        return self.send_request(xcb.Request(buf.getvalue(), 150, False, False),
                                 GetConvolutionFilterCookie(),
                                 GetConvolutionFilterReply)

    def GetConvolutionParameterfv(self, context_tag, target, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, target, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 151, False, True),
                                 GetConvolutionParameterfvCookie(),
                                 GetConvolutionParameterfvReply)

    def GetConvolutionParameterfvUnchecked(self, context_tag, target, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, target, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 151, False, False),
                                 GetConvolutionParameterfvCookie(),
                                 GetConvolutionParameterfvReply)

    def GetConvolutionParameteriv(self, context_tag, target, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, target, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 152, False, True),
                                 GetConvolutionParameterivCookie(),
                                 GetConvolutionParameterivReply)

    def GetConvolutionParameterivUnchecked(self, context_tag, target, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, target, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 152, False, False),
                                 GetConvolutionParameterivCookie(),
                                 GetConvolutionParameterivReply)

    def GetSeparableFilter(self, context_tag, target, format, type, swap_bytes):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIIB', context_tag, target, format, type, swap_bytes))
        return self.send_request(xcb.Request(buf.getvalue(), 153, False, True),
                                 GetSeparableFilterCookie(),
                                 GetSeparableFilterReply)

    def GetSeparableFilterUnchecked(self, context_tag, target, format, type, swap_bytes):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIIB', context_tag, target, format, type, swap_bytes))
        return self.send_request(xcb.Request(buf.getvalue(), 153, False, False),
                                 GetSeparableFilterCookie(),
                                 GetSeparableFilterReply)

    def GetHistogram(self, context_tag, target, format, type, swap_bytes, reset):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIIBB', context_tag, target, format, type, swap_bytes, reset))
        return self.send_request(xcb.Request(buf.getvalue(), 154, False, True),
                                 GetHistogramCookie(),
                                 GetHistogramReply)

    def GetHistogramUnchecked(self, context_tag, target, format, type, swap_bytes, reset):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIIBB', context_tag, target, format, type, swap_bytes, reset))
        return self.send_request(xcb.Request(buf.getvalue(), 154, False, False),
                                 GetHistogramCookie(),
                                 GetHistogramReply)

    def GetHistogramParameterfv(self, context_tag, target, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, target, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 155, False, True),
                                 GetHistogramParameterfvCookie(),
                                 GetHistogramParameterfvReply)

    def GetHistogramParameterfvUnchecked(self, context_tag, target, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, target, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 155, False, False),
                                 GetHistogramParameterfvCookie(),
                                 GetHistogramParameterfvReply)

    def GetHistogramParameteriv(self, context_tag, target, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, target, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 156, False, True),
                                 GetHistogramParameterivCookie(),
                                 GetHistogramParameterivReply)

    def GetHistogramParameterivUnchecked(self, context_tag, target, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, target, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 156, False, False),
                                 GetHistogramParameterivCookie(),
                                 GetHistogramParameterivReply)

    def GetMinmax(self, context_tag, target, format, type, swap_bytes, reset):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIIBB', context_tag, target, format, type, swap_bytes, reset))
        return self.send_request(xcb.Request(buf.getvalue(), 157, False, True),
                                 GetMinmaxCookie(),
                                 GetMinmaxReply)

    def GetMinmaxUnchecked(self, context_tag, target, format, type, swap_bytes, reset):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIIBB', context_tag, target, format, type, swap_bytes, reset))
        return self.send_request(xcb.Request(buf.getvalue(), 157, False, False),
                                 GetMinmaxCookie(),
                                 GetMinmaxReply)

    def GetMinmaxParameterfv(self, context_tag, target, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, target, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 158, False, True),
                                 GetMinmaxParameterfvCookie(),
                                 GetMinmaxParameterfvReply)

    def GetMinmaxParameterfvUnchecked(self, context_tag, target, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, target, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 158, False, False),
                                 GetMinmaxParameterfvCookie(),
                                 GetMinmaxParameterfvReply)

    def GetMinmaxParameteriv(self, context_tag, target, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, target, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 159, False, True),
                                 GetMinmaxParameterivCookie(),
                                 GetMinmaxParameterivReply)

    def GetMinmaxParameterivUnchecked(self, context_tag, target, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, target, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 159, False, False),
                                 GetMinmaxParameterivCookie(),
                                 GetMinmaxParameterivReply)

    def GetCompressedTexImageARB(self, context_tag, target, level):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIi', context_tag, target, level))
        return self.send_request(xcb.Request(buf.getvalue(), 160, False, True),
                                 GetCompressedTexImageARBCookie(),
                                 GetCompressedTexImageARBReply)

    def GetCompressedTexImageARBUnchecked(self, context_tag, target, level):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIi', context_tag, target, level))
        return self.send_request(xcb.Request(buf.getvalue(), 160, False, False),
                                 GetCompressedTexImageARBCookie(),
                                 GetCompressedTexImageARBReply)

    def DeleteQueriesARBChecked(self, context_tag, n, ids):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIi', context_tag, n))
        buf.write(str(buffer(array('I', ids))))
        return self.send_request(xcb.Request(buf.getvalue(), 161, True, True),
                                 xcb.VoidCookie())

    def DeleteQueriesARB(self, context_tag, n, ids):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIi', context_tag, n))
        buf.write(str(buffer(array('I', ids))))
        return self.send_request(xcb.Request(buf.getvalue(), 161, True, False),
                                 xcb.VoidCookie())

    def GenQueriesARB(self, context_tag, n):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIi', context_tag, n))
        return self.send_request(xcb.Request(buf.getvalue(), 162, False, True),
                                 GenQueriesARBCookie(),
                                 GenQueriesARBReply)

    def GenQueriesARBUnchecked(self, context_tag, n):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIi', context_tag, n))
        return self.send_request(xcb.Request(buf.getvalue(), 162, False, False),
                                 GenQueriesARBCookie(),
                                 GenQueriesARBReply)

    def IsQueryARB(self, context_tag, id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', context_tag, id))
        return self.send_request(xcb.Request(buf.getvalue(), 163, False, True),
                                 IsQueryARBCookie(),
                                 IsQueryARBReply)

    def IsQueryARBUnchecked(self, context_tag, id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', context_tag, id))
        return self.send_request(xcb.Request(buf.getvalue(), 163, False, False),
                                 IsQueryARBCookie(),
                                 IsQueryARBReply)

    def GetQueryivARB(self, context_tag, target, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, target, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 164, False, True),
                                 GetQueryivARBCookie(),
                                 GetQueryivARBReply)

    def GetQueryivARBUnchecked(self, context_tag, target, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, target, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 164, False, False),
                                 GetQueryivARBCookie(),
                                 GetQueryivARBReply)

    def GetQueryObjectivARB(self, context_tag, id, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, id, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 165, False, True),
                                 GetQueryObjectivARBCookie(),
                                 GetQueryObjectivARBReply)

    def GetQueryObjectivARBUnchecked(self, context_tag, id, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, id, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 165, False, False),
                                 GetQueryObjectivARBCookie(),
                                 GetQueryObjectivARBReply)

    def GetQueryObjectuivARB(self, context_tag, id, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, id, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 166, False, True),
                                 GetQueryObjectuivARBCookie(),
                                 GetQueryObjectuivARBReply)

    def GetQueryObjectuivARBUnchecked(self, context_tag, id, pname):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', context_tag, id, pname))
        return self.send_request(xcb.Request(buf.getvalue(), 166, False, False),
                                 GetQueryObjectuivARBCookie(),
                                 GetQueryObjectuivARBReply)

_events = {
    0 : PbufferClobberEvent,
}

_errors = {
    -1 : (GenericError, BadGeneric),
    0 : (ContextError, BadContext),
    1 : (ContextStateError, BadContextState),
    2 : (DrawableError, BadDrawable),
    3 : (PixmapError, BadPixmap),
    4 : (ContextTagError, BadContextTag),
    5 : (CurrentWindowError, BadCurrentWindow),
    6 : (RenderRequestError, BadRenderRequest),
    7 : (LargeRequestError, BadLargeRequest),
    8 : (UnsupportedPrivateRequestError, BadUnsupportedPrivateRequest),
    9 : (FBConfigError, BadFBConfig),
    10 : (PbufferError, BadPbuffer),
    11 : (CurrentDrawableError, BadCurrentDrawable),
    12 : (WindowError, BadWindow),
}

xcb._add_ext(key, glxExtension, _events, _errors)
