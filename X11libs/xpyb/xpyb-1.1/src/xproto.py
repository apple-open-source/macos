#
# This file generated automatically from xproto.xml by py_client.py.
# Edit at your peril.
#

import xcb
import cStringIO
from struct import pack, unpack_from
from array import array

class CHAR2B(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.byte1, self.byte2,) = unpack_from('BB', self, count)

class POINT(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.x, self.y,) = unpack_from('hh', self, count)

class RECTANGLE(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.x, self.y, self.width, self.height,) = unpack_from('hhHH', self, count)

class ARC(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.x, self.y, self.width, self.height, self.angle1, self.angle2,) = unpack_from('hhHHhh', self, count)

class FORMAT(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.depth, self.bits_per_pixel, self.scanline_pad,) = unpack_from('BBB5x', self, count)

class VisualClass:
    StaticGray = 0
    GrayScale = 1
    StaticColor = 2
    PseudoColor = 3
    TrueColor = 4
    DirectColor = 5

class VISUALTYPE(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.visual_id, self._class, self.bits_per_rgb_value, self.colormap_entries, self.red_mask, self.green_mask, self.blue_mask,) = unpack_from('IBBHIII4x', self, count)

class DEPTH(xcb.Struct):
    def __init__(self, parent, offset):
        xcb.Struct.__init__(self, parent, offset)
        count = 0
        (self.depth, self.visuals_len,) = unpack_from('BxH4x', self, count)
        count += 8
        self.visuals = xcb.List(self, count, self.visuals_len, VISUALTYPE, 24)
        count += len(self.visuals.buf())
        xcb._resize_obj(self, count)

class EventMask:
    NoEvent = 0
    KeyPress = 1
    KeyRelease = 2
    ButtonPress = 4
    ButtonRelease = 8
    EnterWindow = 16
    LeaveWindow = 32
    PointerMotion = 64
    PointerMotionHint = 128
    Button1Motion = 256
    Button2Motion = 512
    Button3Motion = 1024
    Button4Motion = 2048
    Button5Motion = 4096
    ButtonMotion = 8192
    KeymapState = 16384
    Exposure = 32768
    VisibilityChange = 65536
    StructureNotify = 131072
    ResizeRedirect = 262144
    SubstructureNotify = 524288
    SubstructureRedirect = 1048576
    FocusChange = 2097152
    PropertyChange = 4194304
    ColorMapChange = 8388608
    OwnerGrabButton = 16777216

class BackingStore:
    NotUseful = 0
    WhenMapped = 1
    Always = 2

class SCREEN(xcb.Struct):
    def __init__(self, parent, offset):
        xcb.Struct.__init__(self, parent, offset)
        count = 0
        (self.root, self.default_colormap, self.white_pixel, self.black_pixel, self.current_input_masks, self.width_in_pixels, self.height_in_pixels, self.width_in_millimeters, self.height_in_millimeters, self.min_installed_maps, self.max_installed_maps, self.root_visual, self.backing_stores, self.save_unders, self.root_depth, self.allowed_depths_len,) = unpack_from('IIIIIHHHHHHIBBBB', self, count)
        count += 40
        self.allowed_depths = xcb.List(self, count, self.allowed_depths_len, DEPTH, -1)
        count += len(self.allowed_depths.buf())
        xcb._resize_obj(self, count)

class SetupRequest(xcb.Struct):
    def __init__(self, parent, offset):
        xcb.Struct.__init__(self, parent, offset)
        count = 0
        (self.byte_order, self.protocol_major_version, self.protocol_minor_version, self.authorization_protocol_name_len, self.authorization_protocol_data_len,) = unpack_from('BxHHHH2x', self, count)
        count += 12
        self.authorization_protocol_name = xcb.List(self, count, self.authorization_protocol_name_len, 'b', 1)
        count += len(self.authorization_protocol_name.buf())
        count += xcb.type_pad(1, count)
        self.authorization_protocol_data = xcb.List(self, count, self.authorization_protocol_data_len, 'b', 1)
        count += len(self.authorization_protocol_data.buf())
        xcb._resize_obj(self, count)

class SetupFailed(xcb.Struct):
    def __init__(self, parent, offset):
        xcb.Struct.__init__(self, parent, offset)
        count = 0
        (self.status, self.reason_len, self.protocol_major_version, self.protocol_minor_version, self.length,) = unpack_from('BBHHH', self, count)
        count += 8
        self.reason = xcb.List(self, count, self.reason_len, 'b', 1)
        count += len(self.reason.buf())
        xcb._resize_obj(self, count)

class SetupAuthenticate(xcb.Struct):
    def __init__(self, parent, offset):
        xcb.Struct.__init__(self, parent, offset)
        count = 0
        (self.status, self.length,) = unpack_from('B5xH', self, count)
        count += 8
        self.reason = xcb.List(self, count, (self.length * 4), 'b', 1)
        count += len(self.reason.buf())
        xcb._resize_obj(self, count)

class ImageOrder:
    LSBFirst = 0
    MSBFirst = 1

class Setup(xcb.Struct):
    def __init__(self, parent, offset):
        xcb.Struct.__init__(self, parent, offset)
        count = 0
        (self.status, self.protocol_major_version, self.protocol_minor_version, self.length, self.release_number, self.resource_id_base, self.resource_id_mask, self.motion_buffer_size, self.vendor_len, self.maximum_request_length, self.roots_len, self.pixmap_formats_len, self.image_byte_order, self.bitmap_format_bit_order, self.bitmap_format_scanline_unit, self.bitmap_format_scanline_pad, self.min_keycode, self.max_keycode,) = unpack_from('BxHHHIIIIHHBBBBBBBB4x', self, count)
        count += 40
        self.vendor = xcb.List(self, count, self.vendor_len, 'b', 1)
        count += len(self.vendor.buf())
        count += xcb.type_pad(8, count)
        self.pixmap_formats = xcb.List(self, count, self.pixmap_formats_len, FORMAT, 8)
        count += len(self.pixmap_formats.buf())
        count += xcb.type_pad(4, count)
        self.roots = xcb.List(self, count, self.roots_len, SCREEN, -1)
        count += len(self.roots.buf())
        xcb._resize_obj(self, count)

class ModMask:
    Shift = 1
    Lock = 2
    Control = 4
    _1 = 8
    _2 = 16
    _3 = 32
    _4 = 64
    _5 = 128
    Any = 32768

class KeyButMask:
    Shift = 1
    Lock = 2
    Control = 4
    Mod1 = 8
    Mod2 = 16
    Mod3 = 32
    Mod4 = 64
    Mod5 = 128
    Button1 = 256
    Button2 = 512
    Button3 = 1024
    Button4 = 2048
    Button5 = 4096

class Window:
    _None = 0

class KeyPressEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.detail, self.time, self.root, self.event, self.child, self.root_x, self.root_y, self.event_x, self.event_y, self.state, self.same_screen,) = unpack_from('xB2xIIIIhhhhHBx', self, count)

class KeyReleaseEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.detail, self.time, self.root, self.event, self.child, self.root_x, self.root_y, self.event_x, self.event_y, self.state, self.same_screen,) = unpack_from('xB2xIIIIhhhhHBx', self, count)

class ButtonMask:
    _1 = 256
    _2 = 512
    _3 = 1024
    _4 = 2048
    _5 = 4096
    Any = 32768

class ButtonPressEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.detail, self.time, self.root, self.event, self.child, self.root_x, self.root_y, self.event_x, self.event_y, self.state, self.same_screen,) = unpack_from('xB2xIIIIhhhhHBx', self, count)

class ButtonReleaseEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.detail, self.time, self.root, self.event, self.child, self.root_x, self.root_y, self.event_x, self.event_y, self.state, self.same_screen,) = unpack_from('xB2xIIIIhhhhHBx', self, count)

class Motion:
    Normal = 0
    Hint = 1

class MotionNotifyEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.detail, self.time, self.root, self.event, self.child, self.root_x, self.root_y, self.event_x, self.event_y, self.state, self.same_screen,) = unpack_from('xB2xIIIIhhhhHBx', self, count)

class NotifyDetail:
    Ancestor = 0
    Virtual = 1
    Inferior = 2
    Nonlinear = 3
    NonlinearVirtual = 4
    Pointer = 5
    PointerRoot = 6
    _None = 7

class NotifyMode:
    Normal = 0
    Grab = 1
    Ungrab = 2
    WhileGrabbed = 3

class EnterNotifyEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.detail, self.time, self.root, self.event, self.child, self.root_x, self.root_y, self.event_x, self.event_y, self.state, self.mode, self.same_screen_focus,) = unpack_from('xB2xIIIIhhhhHBB', self, count)

class LeaveNotifyEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.detail, self.time, self.root, self.event, self.child, self.root_x, self.root_y, self.event_x, self.event_y, self.state, self.mode, self.same_screen_focus,) = unpack_from('xB2xIIIIhhhhHBB', self, count)

class FocusInEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.detail, self.event, self.mode,) = unpack_from('xB2xIB3x', self, count)

class FocusOutEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.detail, self.event, self.mode,) = unpack_from('xB2xIB3x', self, count)

class KeymapNotifyEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        count += 1
        self.keys = xcb.List(self, count, 31, 'B', 1)

class ExposeEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.window, self.x, self.y, self.width, self.height, self.count,) = unpack_from('xx2xIHHHHH2x', self, count)

class GraphicsExposureEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.drawable, self.x, self.y, self.width, self.height, self.minor_opcode, self.count, self.major_opcode,) = unpack_from('xx2xIHHHHHHB3x', self, count)

class NoExposureEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.drawable, self.minor_opcode, self.major_opcode,) = unpack_from('xx2xIHBx', self, count)

class Visibility:
    Unobscured = 0
    PartiallyObscured = 1
    FullyObscured = 2

class VisibilityNotifyEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.window, self.state,) = unpack_from('xx2xIB3x', self, count)

class CreateNotifyEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.parent, self.window, self.x, self.y, self.width, self.height, self.border_width, self.override_redirect,) = unpack_from('xx2xIIhhHHHBx', self, count)

class DestroyNotifyEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.event, self.window,) = unpack_from('xx2xII', self, count)

class UnmapNotifyEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.event, self.window, self.from_configure,) = unpack_from('xx2xIIB3x', self, count)

class MapNotifyEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.event, self.window, self.override_redirect,) = unpack_from('xx2xIIB3x', self, count)

class MapRequestEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.parent, self.window,) = unpack_from('xx2xII', self, count)

class ReparentNotifyEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.event, self.window, self.parent, self.x, self.y, self.override_redirect,) = unpack_from('xx2xIIIhhB3x', self, count)

class ConfigureNotifyEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.event, self.window, self.above_sibling, self.x, self.y, self.width, self.height, self.border_width, self.override_redirect,) = unpack_from('xx2xIIIhhHHHBx', self, count)

class ConfigureRequestEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.stack_mode, self.parent, self.window, self.sibling, self.x, self.y, self.width, self.height, self.border_width, self.value_mask,) = unpack_from('xB2xIIIhhHHHH', self, count)

class GravityNotifyEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.event, self.window, self.x, self.y,) = unpack_from('xx2xIIhh', self, count)

class ResizeRequestEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.window, self.width, self.height,) = unpack_from('xx2xIHH', self, count)

class Place:
    OnTop = 0
    OnBottom = 1

class CirculateNotifyEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.event, self.window, self.place,) = unpack_from('xx2xII4xB3x', self, count)

class CirculateRequestEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.event, self.window, self.place,) = unpack_from('xx2xII4xB3x', self, count)

class Property:
    NewValue = 0
    Delete = 1

class PropertyNotifyEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.window, self.atom, self.time, self.state,) = unpack_from('xx2xIIIB3x', self, count)

class SelectionClearEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.time, self.owner, self.selection,) = unpack_from('xx2xIII', self, count)

class Time:
    CurrentTime = 0

class Atom:
    _None = 0

class SelectionRequestEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.time, self.owner, self.requestor, self.selection, self.target, self.property,) = unpack_from('xx2xIIIIII', self, count)

class SelectionNotifyEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.time, self.requestor, self.selection, self.target, self.property,) = unpack_from('xx2xIIIII', self, count)

class ColormapState:
    Uninstalled = 0
    Installed = 1

class Colormap:
    _None = 0

class ColormapNotifyEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.window, self.colormap, self.new, self.state,) = unpack_from('xx2xIIBB2x', self, count)

class ClientMessageData(xcb.Union):
    def __init__(self, parent, offset, size):
        xcb.Union.__init__(self, parent, offset, size)
        count = 0
        self.data8 = xcb.List(self, 0, 20, 'B', 1)
        count = max(count, len(self.data8.buf()))
        self.data16 = xcb.List(self, 0, 10, 'H', 2)
        count = max(count, len(self.data16.buf()))
        self.data32 = xcb.List(self, 0, 5, 'I', 4)
        count = max(count, len(self.data32.buf()))

class ClientMessageEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.format, self.window, self.type,) = unpack_from('xB2xII', self, count)
        count += 12
        self.data = ClientMessageData(self, count, 60)

class Mapping:
    Modifier = 0
    Keyboard = 1
    Pointer = 2

class MappingNotifyEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.request, self.first_keycode, self.count,) = unpack_from('xx2xBBBx', self, count)

class RequestError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)
        count = 0
        (self.bad_value, self.minor_opcode, self.major_opcode,) = unpack_from('xx2xIHBx', self, count)

class BadRequest(xcb.ProtocolException):
    pass

class ValueError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)
        count = 0
        (self.bad_value, self.minor_opcode, self.major_opcode,) = unpack_from('xx2xIHBx', self, count)

class BadValue(xcb.ProtocolException):
    pass

class WindowError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)
        count = 0
        (self.bad_value, self.minor_opcode, self.major_opcode,) = unpack_from('xx2xIHBx', self, count)

class BadWindow(xcb.ProtocolException):
    pass

class PixmapError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)
        count = 0
        (self.bad_value, self.minor_opcode, self.major_opcode,) = unpack_from('xx2xIHBx', self, count)

class BadPixmap(xcb.ProtocolException):
    pass

class AtomError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)
        count = 0
        (self.bad_value, self.minor_opcode, self.major_opcode,) = unpack_from('xx2xIHBx', self, count)

class BadAtom(xcb.ProtocolException):
    pass

class CursorError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)
        count = 0
        (self.bad_value, self.minor_opcode, self.major_opcode,) = unpack_from('xx2xIHBx', self, count)

class BadCursor(xcb.ProtocolException):
    pass

class FontError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)
        count = 0
        (self.bad_value, self.minor_opcode, self.major_opcode,) = unpack_from('xx2xIHBx', self, count)

class BadFont(xcb.ProtocolException):
    pass

class MatchError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)
        count = 0
        (self.bad_value, self.minor_opcode, self.major_opcode,) = unpack_from('xx2xIHBx', self, count)

class BadMatch(xcb.ProtocolException):
    pass

class DrawableError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)
        count = 0
        (self.bad_value, self.minor_opcode, self.major_opcode,) = unpack_from('xx2xIHBx', self, count)

class BadDrawable(xcb.ProtocolException):
    pass

class AccessError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)
        count = 0
        (self.bad_value, self.minor_opcode, self.major_opcode,) = unpack_from('xx2xIHBx', self, count)

class BadAccess(xcb.ProtocolException):
    pass

class AllocError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)
        count = 0
        (self.bad_value, self.minor_opcode, self.major_opcode,) = unpack_from('xx2xIHBx', self, count)

class BadAlloc(xcb.ProtocolException):
    pass

class ColormapError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)
        count = 0
        (self.bad_value, self.minor_opcode, self.major_opcode,) = unpack_from('xx2xIHBx', self, count)

class BadColormap(xcb.ProtocolException):
    pass

class GContextError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)
        count = 0
        (self.bad_value, self.minor_opcode, self.major_opcode,) = unpack_from('xx2xIHBx', self, count)

class BadGContext(xcb.ProtocolException):
    pass

class IDChoiceError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)
        count = 0
        (self.bad_value, self.minor_opcode, self.major_opcode,) = unpack_from('xx2xIHBx', self, count)

class BadIDChoice(xcb.ProtocolException):
    pass

class NameError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)
        count = 0
        (self.bad_value, self.minor_opcode, self.major_opcode,) = unpack_from('xx2xIHBx', self, count)

class BadName(xcb.ProtocolException):
    pass

class LengthError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)
        count = 0
        (self.bad_value, self.minor_opcode, self.major_opcode,) = unpack_from('xx2xIHBx', self, count)

class BadLength(xcb.ProtocolException):
    pass

class ImplementationError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)
        count = 0
        (self.bad_value, self.minor_opcode, self.major_opcode,) = unpack_from('xx2xIHBx', self, count)

class BadImplementation(xcb.ProtocolException):
    pass

class WindowClass:
    CopyFromParent = 0
    InputOutput = 1
    InputOnly = 2

class CW:
    BackPixmap = 1
    BackPixel = 2
    BorderPixmap = 4
    BorderPixel = 8
    BitGravity = 16
    WinGravity = 32
    BackingStore = 64
    BackingPlanes = 128
    BackingPixel = 256
    OverrideRedirect = 512
    SaveUnder = 1024
    EventMask = 2048
    DontPropagate = 4096
    Colormap = 8192
    Cursor = 16384

class BackPixmap:
    _None = 0
    ParentRelative = 1

class Gravity:
    BitForget = 0
    WinUnmap = 0
    NorthWest = 1
    North = 2
    NorthEast = 3
    West = 4
    Center = 5
    East = 6
    SouthWest = 7
    South = 8
    SouthEast = 9
    Static = 10

class MapState:
    Unmapped = 0
    Unviewable = 1
    Viewable = 2

class GetWindowAttributesCookie(xcb.Cookie):
    pass

class GetWindowAttributesReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.backing_store, self.visual, self._class, self.bit_gravity, self.win_gravity, self.backing_planes, self.backing_pixel, self.save_under, self.map_is_installed, self.map_state, self.override_redirect, self.colormap, self.all_event_masks, self.your_event_mask, self.do_not_propagate_mask,) = unpack_from('xB2x4xIHBBIIBBBBIIIH2x', self, count)

class SetMode:
    Insert = 0
    Delete = 1

class ConfigWindow:
    X = 1
    Y = 2
    Width = 4
    Height = 8
    BorderWidth = 16
    Sibling = 32
    StackMode = 64

class StackMode:
    Above = 0
    Below = 1
    TopIf = 2
    BottomIf = 3
    Opposite = 4

class Circulate:
    RaiseLowest = 0
    LowerHighest = 1

class GetGeometryCookie(xcb.Cookie):
    pass

class GetGeometryReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.depth, self.root, self.x, self.y, self.width, self.height, self.border_width,) = unpack_from('xB2x4xIhhHHH2x', self, count)

class QueryTreeCookie(xcb.Cookie):
    pass

class QueryTreeReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.root, self.parent, self.children_len,) = unpack_from('xx2x4xIIH14x', self, count)
        count += 32
        self.children = xcb.List(self, count, self.children_len, 'I', 4)

class InternAtomCookie(xcb.Cookie):
    pass

class InternAtomReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.atom,) = unpack_from('xx2x4xI', self, count)

class GetAtomNameCookie(xcb.Cookie):
    pass

class GetAtomNameReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.name_len,) = unpack_from('xx2x4xH22x', self, count)
        count += 32
        self.name = xcb.List(self, count, self.name_len, 'b', 1)

class PropMode:
    Replace = 0
    Prepend = 1
    Append = 2

class GetPropertyType:
    Any = 0

class GetPropertyCookie(xcb.Cookie):
    pass

class GetPropertyReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.format, self.type, self.bytes_after, self.value_len,) = unpack_from('xB2x4xIII12x', self, count)
        count += 32
        self.value = xcb.List(self, count, (self.value_len * (self.format / 8)), 'B', 1)

class ListPropertiesCookie(xcb.Cookie):
    pass

class ListPropertiesReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.atoms_len,) = unpack_from('xx2x4xH22x', self, count)
        count += 32
        self.atoms = xcb.List(self, count, self.atoms_len, 'I', 4)

class GetSelectionOwnerCookie(xcb.Cookie):
    pass

class GetSelectionOwnerReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.owner,) = unpack_from('xx2x4xI', self, count)

class SendEventDest:
    PointerWindow = 0
    ItemFocus = 1

class GrabMode:
    Sync = 0
    Async = 1

class GrabStatus:
    Success = 0
    AlreadyGrabbed = 1
    InvalidTime = 2
    NotViewable = 3
    Frozen = 4

class Cursor:
    _None = 0

class GrabPointerCookie(xcb.Cookie):
    pass

class GrabPointerReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.status,) = unpack_from('xB2x4x', self, count)

class ButtonIndex:
    Any = 0
    _1 = 1
    _2 = 2
    _3 = 3
    _4 = 4
    _5 = 5

class GrabKeyboardCookie(xcb.Cookie):
    pass

class GrabKeyboardReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.status,) = unpack_from('xB2x4x', self, count)

class Grab:
    Any = 0

class Allow:
    AsyncPointer = 0
    SyncPointer = 1
    ReplayPointer = 2
    AsyncKeyboard = 3
    SyncKeyboard = 4
    ReplayKeyboard = 5
    AsyncBoth = 6
    SyncBoth = 7

class QueryPointerCookie(xcb.Cookie):
    pass

class QueryPointerReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.same_screen, self.root, self.child, self.root_x, self.root_y, self.win_x, self.win_y, self.mask,) = unpack_from('xB2x4xIIhhhhH2x', self, count)

class TIMECOORD(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.time, self.x, self.y,) = unpack_from('Ihh', self, count)

class GetMotionEventsCookie(xcb.Cookie):
    pass

class GetMotionEventsReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.events_len,) = unpack_from('xx2x4xI20x', self, count)
        count += 32
        self.events = xcb.List(self, count, self.events_len, TIMECOORD, 8)

class TranslateCoordinatesCookie(xcb.Cookie):
    pass

class TranslateCoordinatesReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.same_screen, self.child, self.dst_x, self.dst_y,) = unpack_from('xB2x4xIHH', self, count)

class InputFocus:
    _None = 0
    PointerRoot = 1
    Parent = 2

class GetInputFocusCookie(xcb.Cookie):
    pass

class GetInputFocusReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.revert_to, self.focus,) = unpack_from('xB2x4xI', self, count)

class QueryKeymapCookie(xcb.Cookie):
    pass

class QueryKeymapReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        count += 8
        self.keys = xcb.List(self, count, 32, 'B', 1)

class FontDraw:
    LeftToRight = 0
    RightToLeft = 1

class FONTPROP(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.name, self.value,) = unpack_from('II', self, count)

class CHARINFO(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.left_side_bearing, self.right_side_bearing, self.character_width, self.ascent, self.descent, self.attributes,) = unpack_from('hhhhhH', self, count)

class QueryFontCookie(xcb.Cookie):
    pass

class QueryFontReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        count += 8
        self.min_bounds = CHARINFO(self, count, 12)
        count += 12
        count += 4
        count += xcb.type_pad(12, count)
        self.max_bounds = CHARINFO(self, count, 12)
        count += 12
        (self.min_char_or_byte2, self.max_char_or_byte2, self.default_char, self.properties_len, self.draw_direction, self.min_byte1, self.max_byte1, self.all_chars_exist, self.font_ascent, self.font_descent, self.char_infos_len,) = unpack_from('4xHHHHBBBBhhI', self, count)
        count += 24
        count += xcb.type_pad(8, count)
        self.properties = xcb.List(self, count, self.properties_len, FONTPROP, 8)
        count += len(self.properties.buf())
        count += xcb.type_pad(12, count)
        self.char_infos = xcb.List(self, count, self.char_infos_len, CHARINFO, 12)

class QueryTextExtentsCookie(xcb.Cookie):
    pass

class QueryTextExtentsReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.draw_direction, self.font_ascent, self.font_descent, self.overall_ascent, self.overall_descent, self.overall_width, self.overall_left, self.overall_right,) = unpack_from('xB2x4xhhhhiii', self, count)

class STR(xcb.Struct):
    def __init__(self, parent, offset):
        xcb.Struct.__init__(self, parent, offset)
        count = 0
        (self.name_len,) = unpack_from('B', self, count)
        count += 1
        self.name = xcb.List(self, count, self.name_len, 'b', 1)
        count += len(self.name.buf())
        xcb._resize_obj(self, count)

class ListFontsCookie(xcb.Cookie):
    pass

class ListFontsReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.names_len,) = unpack_from('xx2x4xH22x', self, count)
        count += 32
        self.names = xcb.List(self, count, self.names_len, STR, -1)

class ListFontsWithInfoCookie(xcb.Cookie):
    pass

class ListFontsWithInfoReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.name_len,) = unpack_from('xB2x4x', self, count)
        count += 8
        self.min_bounds = CHARINFO(self, count, 12)
        count += 12
        count += 4
        count += xcb.type_pad(12, count)
        self.max_bounds = CHARINFO(self, count, 12)
        count += 12
        (self.min_char_or_byte2, self.max_char_or_byte2, self.default_char, self.properties_len, self.draw_direction, self.min_byte1, self.max_byte1, self.all_chars_exist, self.font_ascent, self.font_descent, self.replies_hint,) = unpack_from('4xHHHHBBBBhhI', self, count)
        count += 24
        count += xcb.type_pad(8, count)
        self.properties = xcb.List(self, count, self.properties_len, FONTPROP, 8)
        count += len(self.properties.buf())
        count += xcb.type_pad(1, count)
        self.name = xcb.List(self, count, self.name_len, 'b', 1)

class GetFontPathCookie(xcb.Cookie):
    pass

class GetFontPathReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.path_len,) = unpack_from('xx2x4xH22x', self, count)
        count += 32
        self.path = xcb.List(self, count, self.path_len, STR, -1)

class GC:
    Function = 1
    PlaneMask = 2
    Foreground = 4
    Background = 8
    LineWidth = 16
    LineStyle = 32
    CapStyle = 64
    JoinStyle = 128
    FillStyle = 256
    FillRule = 512
    Tile = 1024
    Stipple = 2048
    TileStippleOriginX = 4096
    TileStippleOriginY = 8192
    Font = 16384
    SubwindowMode = 32768
    GraphicsExposures = 65536
    ClipOriginX = 131072
    ClipOriginY = 262144
    ClipMask = 524288
    DashOffset = 1048576
    DashList = 2097152
    ArcMode = 4194304

class GX:
    clear = 0
    _and = 1
    andReverse = 2
    copy = 3
    andInverted = 4
    noop = 5
    xor = 6
    _or = 7
    nor = 8
    equiv = 9
    invert = 10
    orReverse = 11
    copyInverted = 12
    orInverted = 13
    nand = 14
    set = 15

class LineStyle:
    Solid = 0
    OnOffDash = 1
    DoubleDash = 2

class CapStyle:
    NotLast = 0
    Butt = 1
    Round = 2
    Projecting = 3

class JoinStyle:
    Miter = 0
    Round = 1
    Bevel = 2

class FillStyle:
    Solid = 0
    Tiled = 1
    Stippled = 2
    OpaqueStippled = 3

class FillRule:
    EvenOdd = 0
    Winding = 1

class SubwindowMode:
    ClipByChildren = 0
    IncludeInferiors = 1

class ArcMode:
    Chord = 0
    PieSlice = 1

class ClipOrdering:
    Unsorted = 0
    YSorted = 1
    YXSorted = 2
    YXBanded = 3

class CoordMode:
    Origin = 0
    Previous = 1

class SEGMENT(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.x1, self.y1, self.x2, self.y2,) = unpack_from('hhhh', self, count)

class PolyShape:
    Complex = 0
    Nonconvex = 1
    Convex = 2

class ImageFormat:
    XYBitmap = 0
    XYPixmap = 1
    ZPixmap = 2

class GetImageCookie(xcb.Cookie):
    pass

class GetImageReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.depth, self.visual,) = unpack_from('xB2x4xI20x', self, count)
        count += 32
        self.data = xcb.List(self, count, (self.length * 4), 'B', 1)

class ColormapAlloc:
    _None = 0
    All = 1

class ListInstalledColormapsCookie(xcb.Cookie):
    pass

class ListInstalledColormapsReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.cmaps_len,) = unpack_from('xx2x4xH22x', self, count)
        count += 32
        self.cmaps = xcb.List(self, count, self.cmaps_len, 'I', 4)

class AllocColorCookie(xcb.Cookie):
    pass

class AllocColorReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.red, self.green, self.blue, self.pixel,) = unpack_from('xx2x4xHHH2xI', self, count)

class AllocNamedColorCookie(xcb.Cookie):
    pass

class AllocNamedColorReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.pixel, self.exact_red, self.exact_green, self.exact_blue, self.visual_red, self.visual_green, self.visual_blue,) = unpack_from('xx2x4xIHHHHHH', self, count)

class AllocColorCellsCookie(xcb.Cookie):
    pass

class AllocColorCellsReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.pixels_len, self.masks_len,) = unpack_from('xx2x4xHH20x', self, count)
        count += 32
        self.pixels = xcb.List(self, count, self.pixels_len, 'I', 4)
        count += len(self.pixels.buf())
        count += xcb.type_pad(4, count)
        self.masks = xcb.List(self, count, self.masks_len, 'I', 4)

class AllocColorPlanesCookie(xcb.Cookie):
    pass

class AllocColorPlanesReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.pixels_len, self.red_mask, self.green_mask, self.blue_mask,) = unpack_from('xx2x4xH2xIII8x', self, count)
        count += 32
        self.pixels = xcb.List(self, count, self.pixels_len, 'I', 4)

class ColorFlag:
    Red = 1
    Green = 2
    Blue = 4

class COLORITEM(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.pixel, self.red, self.green, self.blue, self.flags,) = unpack_from('IHHHBx', self, count)

class RGB(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.red, self.green, self.blue,) = unpack_from('HHH2x', self, count)

class QueryColorsCookie(xcb.Cookie):
    pass

class QueryColorsReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.colors_len,) = unpack_from('xx2x4xH22x', self, count)
        count += 32
        self.colors = xcb.List(self, count, self.colors_len, RGB, 8)

class LookupColorCookie(xcb.Cookie):
    pass

class LookupColorReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.exact_red, self.exact_green, self.exact_blue, self.visual_red, self.visual_green, self.visual_blue,) = unpack_from('xx2x4xHHHHHH', self, count)

class Pixmap:
    _None = 0

class Font:
    _None = 0

class QueryShapeOf:
    LargestCursor = 0
    FastestTile = 1
    FastestStipple = 2

class QueryBestSizeCookie(xcb.Cookie):
    pass

class QueryBestSizeReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.width, self.height,) = unpack_from('xx2x4xHH', self, count)

class QueryExtensionCookie(xcb.Cookie):
    pass

class QueryExtensionReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.present, self.major_opcode, self.first_event, self.first_error,) = unpack_from('xx2x4xBBBB', self, count)

class ListExtensionsCookie(xcb.Cookie):
    pass

class ListExtensionsReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.names_len,) = unpack_from('xB2x4x24x', self, count)
        count += 32
        self.names = xcb.List(self, count, self.names_len, STR, -1)

class GetKeyboardMappingCookie(xcb.Cookie):
    pass

class GetKeyboardMappingReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.keysyms_per_keycode,) = unpack_from('xB2x4x24x', self, count)
        count += 32
        self.keysyms = xcb.List(self, count, self.length, 'I', 4)

class KB:
    KeyClickPercent = 1
    BellPercent = 2
    BellPitch = 4
    BellDuration = 8
    Led = 16
    LedMode = 32
    Key = 64
    AutoRepeatMode = 128

class LedMode:
    Off = 0
    On = 1

class AutoRepeatMode:
    Off = 0
    On = 1
    Default = 2

class GetKeyboardControlCookie(xcb.Cookie):
    pass

class GetKeyboardControlReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.global_auto_repeat, self.led_mask, self.key_click_percent, self.bell_percent, self.bell_pitch, self.bell_duration,) = unpack_from('xB2x4xIBBHH2x', self, count)
        count += 20
        self.auto_repeats = xcb.List(self, count, 32, 'B', 1)

class GetPointerControlCookie(xcb.Cookie):
    pass

class GetPointerControlReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.acceleration_numerator, self.acceleration_denominator, self.threshold,) = unpack_from('xx2x4xHHH18x', self, count)

class Blanking:
    NotPreferred = 0
    Preferred = 1
    Default = 2

class Exposures:
    NotAllowed = 0
    Allowed = 1
    Default = 2

class GetScreenSaverCookie(xcb.Cookie):
    pass

class GetScreenSaverReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.timeout, self.interval, self.prefer_blanking, self.allow_exposures,) = unpack_from('xx2x4xHHBB18x', self, count)

class HostMode:
    Insert = 0
    Delete = 1

class Family:
    Internet = 0
    DECnet = 1
    Chaos = 2
    ServerInterpreted = 5
    Internet6 = 6

class HOST(xcb.Struct):
    def __init__(self, parent, offset):
        xcb.Struct.__init__(self, parent, offset)
        count = 0
        (self.family, self.address_len,) = unpack_from('BxH', self, count)
        count += 4
        self.address = xcb.List(self, count, self.address_len, 'B', 1)
        count += len(self.address.buf())
        xcb._resize_obj(self, count)

class ListHostsCookie(xcb.Cookie):
    pass

class ListHostsReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.mode, self.hosts_len,) = unpack_from('xB2x4xH22x', self, count)
        count += 32
        self.hosts = xcb.List(self, count, self.hosts_len, HOST, -1)

class AccessControl:
    Disable = 0
    Enable = 1

class CloseDown:
    DestroyAll = 0
    RetainPermanent = 1
    RetainTemporary = 2

class Kill:
    AllTemporary = 0

class ScreenSaver:
    Reset = 0
    Active = 1

class MappingStatus:
    Success = 0
    Busy = 1
    Failure = 2

class SetPointerMappingCookie(xcb.Cookie):
    pass

class SetPointerMappingReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.status,) = unpack_from('xB2x4x', self, count)

class GetPointerMappingCookie(xcb.Cookie):
    pass

class GetPointerMappingReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.map_len,) = unpack_from('xB2x4x24x', self, count)
        count += 32
        self.map = xcb.List(self, count, self.map_len, 'B', 1)

class MapIndex:
    Shift = 0
    Lock = 1
    Control = 2
    _1 = 3
    _2 = 4
    _3 = 5
    _4 = 6
    _5 = 7

class SetModifierMappingCookie(xcb.Cookie):
    pass

class SetModifierMappingReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.status,) = unpack_from('xB2x4x', self, count)

class GetModifierMappingCookie(xcb.Cookie):
    pass

class GetModifierMappingReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.keycodes_per_modifier,) = unpack_from('xB2x4x24x', self, count)
        count += 32
        self.keycodes = xcb.List(self, count, (self.keycodes_per_modifier * 8), 'B', 1)

class xprotoExtension(xcb.Extension):

    def CreateWindowChecked(self, depth, wid, parent, x, y, width, height, border_width, _class, visual, value_mask, value_list):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIIhhHHHHII', depth, wid, parent, x, y, width, height, border_width, _class, visual, value_mask))
        buf.write(str(buffer(array('I', value_list))))
        return self.send_request(xcb.Request(buf.getvalue(), 1, True, True),
                                 xcb.VoidCookie())

    def CreateWindow(self, depth, wid, parent, x, y, width, height, border_width, _class, visual, value_mask, value_list):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIIhhHHHHII', depth, wid, parent, x, y, width, height, border_width, _class, visual, value_mask))
        buf.write(str(buffer(array('I', value_list))))
        return self.send_request(xcb.Request(buf.getvalue(), 1, True, False),
                                 xcb.VoidCookie())

    def ChangeWindowAttributesChecked(self, window, value_mask, value_list):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', window, value_mask))
        buf.write(str(buffer(array('I', value_list))))
        return self.send_request(xcb.Request(buf.getvalue(), 2, True, True),
                                 xcb.VoidCookie())

    def ChangeWindowAttributes(self, window, value_mask, value_list):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', window, value_mask))
        buf.write(str(buffer(array('I', value_list))))
        return self.send_request(xcb.Request(buf.getvalue(), 2, True, False),
                                 xcb.VoidCookie())

    def GetWindowAttributes(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 3, False, True),
                                 GetWindowAttributesCookie(),
                                 GetWindowAttributesReply)

    def GetWindowAttributesUnchecked(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 3, False, False),
                                 GetWindowAttributesCookie(),
                                 GetWindowAttributesReply)

    def DestroyWindowChecked(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 4, True, True),
                                 xcb.VoidCookie())

    def DestroyWindow(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 4, True, False),
                                 xcb.VoidCookie())

    def DestroySubwindowsChecked(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 5, True, True),
                                 xcb.VoidCookie())

    def DestroySubwindows(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 5, True, False),
                                 xcb.VoidCookie())

    def ChangeSaveSetChecked(self, mode, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xI', mode, window))
        return self.send_request(xcb.Request(buf.getvalue(), 6, True, True),
                                 xcb.VoidCookie())

    def ChangeSaveSet(self, mode, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xI', mode, window))
        return self.send_request(xcb.Request(buf.getvalue(), 6, True, False),
                                 xcb.VoidCookie())

    def ReparentWindowChecked(self, window, parent, x, y):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIhh', window, parent, x, y))
        return self.send_request(xcb.Request(buf.getvalue(), 7, True, True),
                                 xcb.VoidCookie())

    def ReparentWindow(self, window, parent, x, y):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIhh', window, parent, x, y))
        return self.send_request(xcb.Request(buf.getvalue(), 7, True, False),
                                 xcb.VoidCookie())

    def MapWindowChecked(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 8, True, True),
                                 xcb.VoidCookie())

    def MapWindow(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 8, True, False),
                                 xcb.VoidCookie())

    def MapSubwindowsChecked(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 9, True, True),
                                 xcb.VoidCookie())

    def MapSubwindows(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 9, True, False),
                                 xcb.VoidCookie())

    def UnmapWindowChecked(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 10, True, True),
                                 xcb.VoidCookie())

    def UnmapWindow(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 10, True, False),
                                 xcb.VoidCookie())

    def UnmapSubwindowsChecked(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 11, True, True),
                                 xcb.VoidCookie())

    def UnmapSubwindows(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 11, True, False),
                                 xcb.VoidCookie())

    def ConfigureWindowChecked(self, window, value_mask, value_list):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIH', window, value_mask))
        buf.write(str(buffer(array('I', value_list))))
        buf.write(pack('2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 12, True, True),
                                 xcb.VoidCookie())

    def ConfigureWindow(self, window, value_mask, value_list):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIH', window, value_mask))
        buf.write(str(buffer(array('I', value_list))))
        buf.write(pack('2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 12, True, False),
                                 xcb.VoidCookie())

    def CirculateWindowChecked(self, direction, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xI', direction, window))
        return self.send_request(xcb.Request(buf.getvalue(), 13, True, True),
                                 xcb.VoidCookie())

    def CirculateWindow(self, direction, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xI', direction, window))
        return self.send_request(xcb.Request(buf.getvalue(), 13, True, False),
                                 xcb.VoidCookie())

    def GetGeometry(self, drawable):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', drawable))
        return self.send_request(xcb.Request(buf.getvalue(), 14, False, True),
                                 GetGeometryCookie(),
                                 GetGeometryReply)

    def GetGeometryUnchecked(self, drawable):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', drawable))
        return self.send_request(xcb.Request(buf.getvalue(), 14, False, False),
                                 GetGeometryCookie(),
                                 GetGeometryReply)

    def QueryTree(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 15, False, True),
                                 QueryTreeCookie(),
                                 QueryTreeReply)

    def QueryTreeUnchecked(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 15, False, False),
                                 QueryTreeCookie(),
                                 QueryTreeReply)

    def InternAtom(self, only_if_exists, name_len, name):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xH2x', only_if_exists, name_len))
        buf.write(str(buffer(array('b', name))))
        return self.send_request(xcb.Request(buf.getvalue(), 16, False, True),
                                 InternAtomCookie(),
                                 InternAtomReply)

    def InternAtomUnchecked(self, only_if_exists, name_len, name):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xH2x', only_if_exists, name_len))
        buf.write(str(buffer(array('b', name))))
        return self.send_request(xcb.Request(buf.getvalue(), 16, False, False),
                                 InternAtomCookie(),
                                 InternAtomReply)

    def GetAtomName(self, atom):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', atom))
        return self.send_request(xcb.Request(buf.getvalue(), 17, False, True),
                                 GetAtomNameCookie(),
                                 GetAtomNameReply)

    def GetAtomNameUnchecked(self, atom):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', atom))
        return self.send_request(xcb.Request(buf.getvalue(), 17, False, False),
                                 GetAtomNameCookie(),
                                 GetAtomNameReply)

    def ChangePropertyChecked(self, mode, window, property, type, format, data_len, data):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIIIB3xI', mode, window, property, type, format, data_len))
        buf.write(str(buffer(array('B', data))))
        return self.send_request(xcb.Request(buf.getvalue(), 18, True, True),
                                 xcb.VoidCookie())

    def ChangeProperty(self, mode, window, property, type, format, data_len, data):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIIIB3xI', mode, window, property, type, format, data_len))
        buf.write(str(buffer(array('B', data))))
        return self.send_request(xcb.Request(buf.getvalue(), 18, True, False),
                                 xcb.VoidCookie())

    def DeletePropertyChecked(self, window, property):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', window, property))
        return self.send_request(xcb.Request(buf.getvalue(), 19, True, True),
                                 xcb.VoidCookie())

    def DeleteProperty(self, window, property):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', window, property))
        return self.send_request(xcb.Request(buf.getvalue(), 19, True, False),
                                 xcb.VoidCookie())

    def GetProperty(self, delete, window, property, type, long_offset, long_length):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIIIII', delete, window, property, type, long_offset, long_length))
        return self.send_request(xcb.Request(buf.getvalue(), 20, False, True),
                                 GetPropertyCookie(),
                                 GetPropertyReply)

    def GetPropertyUnchecked(self, delete, window, property, type, long_offset, long_length):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIIIII', delete, window, property, type, long_offset, long_length))
        return self.send_request(xcb.Request(buf.getvalue(), 20, False, False),
                                 GetPropertyCookie(),
                                 GetPropertyReply)

    def ListProperties(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 21, False, True),
                                 ListPropertiesCookie(),
                                 ListPropertiesReply)

    def ListPropertiesUnchecked(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 21, False, False),
                                 ListPropertiesCookie(),
                                 ListPropertiesReply)

    def SetSelectionOwnerChecked(self, owner, selection, time):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', owner, selection, time))
        return self.send_request(xcb.Request(buf.getvalue(), 22, True, True),
                                 xcb.VoidCookie())

    def SetSelectionOwner(self, owner, selection, time):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', owner, selection, time))
        return self.send_request(xcb.Request(buf.getvalue(), 22, True, False),
                                 xcb.VoidCookie())

    def GetSelectionOwner(self, selection):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', selection))
        return self.send_request(xcb.Request(buf.getvalue(), 23, False, True),
                                 GetSelectionOwnerCookie(),
                                 GetSelectionOwnerReply)

    def GetSelectionOwnerUnchecked(self, selection):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', selection))
        return self.send_request(xcb.Request(buf.getvalue(), 23, False, False),
                                 GetSelectionOwnerCookie(),
                                 GetSelectionOwnerReply)

    def ConvertSelectionChecked(self, requestor, selection, target, property, time):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIII', requestor, selection, target, property, time))
        return self.send_request(xcb.Request(buf.getvalue(), 24, True, True),
                                 xcb.VoidCookie())

    def ConvertSelection(self, requestor, selection, target, property, time):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIII', requestor, selection, target, property, time))
        return self.send_request(xcb.Request(buf.getvalue(), 24, True, False),
                                 xcb.VoidCookie())

    def SendEventChecked(self, propagate, destination, event_mask, event):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xII', propagate, destination, event_mask))
        buf.write(str(buffer(array('b', event))))
        return self.send_request(xcb.Request(buf.getvalue(), 25, True, True),
                                 xcb.VoidCookie())

    def SendEvent(self, propagate, destination, event_mask, event):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xII', propagate, destination, event_mask))
        buf.write(str(buffer(array('b', event))))
        return self.send_request(xcb.Request(buf.getvalue(), 25, True, False),
                                 xcb.VoidCookie())

    def GrabPointer(self, owner_events, grab_window, event_mask, pointer_mode, keyboard_mode, confine_to, cursor, time):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIHBBIII', owner_events, grab_window, event_mask, pointer_mode, keyboard_mode, confine_to, cursor, time))
        return self.send_request(xcb.Request(buf.getvalue(), 26, False, True),
                                 GrabPointerCookie(),
                                 GrabPointerReply)

    def GrabPointerUnchecked(self, owner_events, grab_window, event_mask, pointer_mode, keyboard_mode, confine_to, cursor, time):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIHBBIII', owner_events, grab_window, event_mask, pointer_mode, keyboard_mode, confine_to, cursor, time))
        return self.send_request(xcb.Request(buf.getvalue(), 26, False, False),
                                 GrabPointerCookie(),
                                 GrabPointerReply)

    def UngrabPointerChecked(self, time):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', time))
        return self.send_request(xcb.Request(buf.getvalue(), 27, True, True),
                                 xcb.VoidCookie())

    def UngrabPointer(self, time):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', time))
        return self.send_request(xcb.Request(buf.getvalue(), 27, True, False),
                                 xcb.VoidCookie())

    def GrabButtonChecked(self, owner_events, grab_window, event_mask, pointer_mode, keyboard_mode, confine_to, cursor, button, modifiers):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIHBBIIBxH', owner_events, grab_window, event_mask, pointer_mode, keyboard_mode, confine_to, cursor, button, modifiers))
        return self.send_request(xcb.Request(buf.getvalue(), 28, True, True),
                                 xcb.VoidCookie())

    def GrabButton(self, owner_events, grab_window, event_mask, pointer_mode, keyboard_mode, confine_to, cursor, button, modifiers):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIHBBIIBxH', owner_events, grab_window, event_mask, pointer_mode, keyboard_mode, confine_to, cursor, button, modifiers))
        return self.send_request(xcb.Request(buf.getvalue(), 28, True, False),
                                 xcb.VoidCookie())

    def UngrabButtonChecked(self, button, grab_window, modifiers):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIH2x', button, grab_window, modifiers))
        return self.send_request(xcb.Request(buf.getvalue(), 29, True, True),
                                 xcb.VoidCookie())

    def UngrabButton(self, button, grab_window, modifiers):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIH2x', button, grab_window, modifiers))
        return self.send_request(xcb.Request(buf.getvalue(), 29, True, False),
                                 xcb.VoidCookie())

    def ChangeActivePointerGrabChecked(self, cursor, time, event_mask):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIH2x', cursor, time, event_mask))
        return self.send_request(xcb.Request(buf.getvalue(), 30, True, True),
                                 xcb.VoidCookie())

    def ChangeActivePointerGrab(self, cursor, time, event_mask):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIH2x', cursor, time, event_mask))
        return self.send_request(xcb.Request(buf.getvalue(), 30, True, False),
                                 xcb.VoidCookie())

    def GrabKeyboard(self, owner_events, grab_window, time, pointer_mode, keyboard_mode):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIIBB2x', owner_events, grab_window, time, pointer_mode, keyboard_mode))
        return self.send_request(xcb.Request(buf.getvalue(), 31, False, True),
                                 GrabKeyboardCookie(),
                                 GrabKeyboardReply)

    def GrabKeyboardUnchecked(self, owner_events, grab_window, time, pointer_mode, keyboard_mode):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIIBB2x', owner_events, grab_window, time, pointer_mode, keyboard_mode))
        return self.send_request(xcb.Request(buf.getvalue(), 31, False, False),
                                 GrabKeyboardCookie(),
                                 GrabKeyboardReply)

    def UngrabKeyboardChecked(self, time):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', time))
        return self.send_request(xcb.Request(buf.getvalue(), 32, True, True),
                                 xcb.VoidCookie())

    def UngrabKeyboard(self, time):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', time))
        return self.send_request(xcb.Request(buf.getvalue(), 32, True, False),
                                 xcb.VoidCookie())

    def GrabKeyChecked(self, owner_events, grab_window, modifiers, key, pointer_mode, keyboard_mode):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIHBBB3x', owner_events, grab_window, modifiers, key, pointer_mode, keyboard_mode))
        return self.send_request(xcb.Request(buf.getvalue(), 33, True, True),
                                 xcb.VoidCookie())

    def GrabKey(self, owner_events, grab_window, modifiers, key, pointer_mode, keyboard_mode):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIHBBB3x', owner_events, grab_window, modifiers, key, pointer_mode, keyboard_mode))
        return self.send_request(xcb.Request(buf.getvalue(), 33, True, False),
                                 xcb.VoidCookie())

    def UngrabKeyChecked(self, key, grab_window, modifiers):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIH2x', key, grab_window, modifiers))
        return self.send_request(xcb.Request(buf.getvalue(), 34, True, True),
                                 xcb.VoidCookie())

    def UngrabKey(self, key, grab_window, modifiers):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIH2x', key, grab_window, modifiers))
        return self.send_request(xcb.Request(buf.getvalue(), 34, True, False),
                                 xcb.VoidCookie())

    def AllowEventsChecked(self, mode, time):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xI', mode, time))
        return self.send_request(xcb.Request(buf.getvalue(), 35, True, True),
                                 xcb.VoidCookie())

    def AllowEvents(self, mode, time):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xI', mode, time))
        return self.send_request(xcb.Request(buf.getvalue(), 35, True, False),
                                 xcb.VoidCookie())

    def GrabServerChecked(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 36, True, True),
                                 xcb.VoidCookie())

    def GrabServer(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 36, True, False),
                                 xcb.VoidCookie())

    def UngrabServerChecked(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 37, True, True),
                                 xcb.VoidCookie())

    def UngrabServer(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 37, True, False),
                                 xcb.VoidCookie())

    def QueryPointer(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 38, False, True),
                                 QueryPointerCookie(),
                                 QueryPointerReply)

    def QueryPointerUnchecked(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 38, False, False),
                                 QueryPointerCookie(),
                                 QueryPointerReply)

    def GetMotionEvents(self, window, start, stop):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', window, start, stop))
        return self.send_request(xcb.Request(buf.getvalue(), 39, False, True),
                                 GetMotionEventsCookie(),
                                 GetMotionEventsReply)

    def GetMotionEventsUnchecked(self, window, start, stop):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', window, start, stop))
        return self.send_request(xcb.Request(buf.getvalue(), 39, False, False),
                                 GetMotionEventsCookie(),
                                 GetMotionEventsReply)

    def TranslateCoordinates(self, src_window, dst_window, src_x, src_y):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIhh', src_window, dst_window, src_x, src_y))
        return self.send_request(xcb.Request(buf.getvalue(), 40, False, True),
                                 TranslateCoordinatesCookie(),
                                 TranslateCoordinatesReply)

    def TranslateCoordinatesUnchecked(self, src_window, dst_window, src_x, src_y):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIhh', src_window, dst_window, src_x, src_y))
        return self.send_request(xcb.Request(buf.getvalue(), 40, False, False),
                                 TranslateCoordinatesCookie(),
                                 TranslateCoordinatesReply)

    def WarpPointerChecked(self, src_window, dst_window, src_x, src_y, src_width, src_height, dst_x, dst_y):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIhhHHhh', src_window, dst_window, src_x, src_y, src_width, src_height, dst_x, dst_y))
        return self.send_request(xcb.Request(buf.getvalue(), 41, True, True),
                                 xcb.VoidCookie())

    def WarpPointer(self, src_window, dst_window, src_x, src_y, src_width, src_height, dst_x, dst_y):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIhhHHhh', src_window, dst_window, src_x, src_y, src_width, src_height, dst_x, dst_y))
        return self.send_request(xcb.Request(buf.getvalue(), 41, True, False),
                                 xcb.VoidCookie())

    def SetInputFocusChecked(self, revert_to, focus, time):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xII', revert_to, focus, time))
        return self.send_request(xcb.Request(buf.getvalue(), 42, True, True),
                                 xcb.VoidCookie())

    def SetInputFocus(self, revert_to, focus, time):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xII', revert_to, focus, time))
        return self.send_request(xcb.Request(buf.getvalue(), 42, True, False),
                                 xcb.VoidCookie())

    def GetInputFocus(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 43, False, True),
                                 GetInputFocusCookie(),
                                 GetInputFocusReply)

    def GetInputFocusUnchecked(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 43, False, False),
                                 GetInputFocusCookie(),
                                 GetInputFocusReply)

    def QueryKeymap(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 44, False, True),
                                 QueryKeymapCookie(),
                                 QueryKeymapReply)

    def QueryKeymapUnchecked(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 44, False, False),
                                 QueryKeymapCookie(),
                                 QueryKeymapReply)

    def OpenFontChecked(self, fid, name_len, name):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIH2x', fid, name_len))
        buf.write(str(buffer(array('b', name))))
        return self.send_request(xcb.Request(buf.getvalue(), 45, True, True),
                                 xcb.VoidCookie())

    def OpenFont(self, fid, name_len, name):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIH2x', fid, name_len))
        buf.write(str(buffer(array('b', name))))
        return self.send_request(xcb.Request(buf.getvalue(), 45, True, False),
                                 xcb.VoidCookie())

    def CloseFontChecked(self, font):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', font))
        return self.send_request(xcb.Request(buf.getvalue(), 46, True, True),
                                 xcb.VoidCookie())

    def CloseFont(self, font):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', font))
        return self.send_request(xcb.Request(buf.getvalue(), 46, True, False),
                                 xcb.VoidCookie())

    def QueryFont(self, font):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', font))
        return self.send_request(xcb.Request(buf.getvalue(), 47, False, True),
                                 QueryFontCookie(),
                                 QueryFontReply)

    def QueryFontUnchecked(self, font):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', font))
        return self.send_request(xcb.Request(buf.getvalue(), 47, False, False),
                                 QueryFontCookie(),
                                 QueryFontReply)

    def QueryTextExtents(self, font, string_len, string):
        buf = cStringIO.StringIO()
        buf.write(pack('x', ))
        buf.write(pack('B', (self.string_len & 1)))
        buf.write(pack('2xI', font))
        for elt in xcb.Iterator(string, 2, 'string', True):
            buf.write(pack('BB', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 48, False, True),
                                 QueryTextExtentsCookie(),
                                 QueryTextExtentsReply)

    def QueryTextExtentsUnchecked(self, font, string_len, string):
        buf = cStringIO.StringIO()
        buf.write(pack('x', ))
        buf.write(pack('B', (self.string_len & 1)))
        buf.write(pack('2xI', font))
        for elt in xcb.Iterator(string, 2, 'string', True):
            buf.write(pack('BB', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 48, False, False),
                                 QueryTextExtentsCookie(),
                                 QueryTextExtentsReply)

    def ListFonts(self, max_names, pattern_len, pattern):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xHH', max_names, pattern_len))
        buf.write(str(buffer(array('b', pattern))))
        return self.send_request(xcb.Request(buf.getvalue(), 49, False, True),
                                 ListFontsCookie(),
                                 ListFontsReply)

    def ListFontsUnchecked(self, max_names, pattern_len, pattern):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xHH', max_names, pattern_len))
        buf.write(str(buffer(array('b', pattern))))
        return self.send_request(xcb.Request(buf.getvalue(), 49, False, False),
                                 ListFontsCookie(),
                                 ListFontsReply)

    def ListFontsWithInfo(self, max_names, pattern_len, pattern):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xHH', max_names, pattern_len))
        buf.write(str(buffer(array('b', pattern))))
        return self.send_request(xcb.Request(buf.getvalue(), 50, False, True),
                                 ListFontsWithInfoCookie(),
                                 ListFontsWithInfoReply)

    def ListFontsWithInfoUnchecked(self, max_names, pattern_len, pattern):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xHH', max_names, pattern_len))
        buf.write(str(buffer(array('b', pattern))))
        return self.send_request(xcb.Request(buf.getvalue(), 50, False, False),
                                 ListFontsWithInfoCookie(),
                                 ListFontsWithInfoReply)

    def SetFontPathChecked(self, font_qty, path_len, path):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xH', font_qty))
        buf.write(str(buffer(array('b', path))))
        return self.send_request(xcb.Request(buf.getvalue(), 51, True, True),
                                 xcb.VoidCookie())

    def SetFontPath(self, font_qty, path_len, path):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xH', font_qty))
        buf.write(str(buffer(array('b', path))))
        return self.send_request(xcb.Request(buf.getvalue(), 51, True, False),
                                 xcb.VoidCookie())

    def GetFontPath(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 52, False, True),
                                 GetFontPathCookie(),
                                 GetFontPathReply)

    def GetFontPathUnchecked(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 52, False, False),
                                 GetFontPathCookie(),
                                 GetFontPathReply)

    def CreatePixmapChecked(self, depth, pid, drawable, width, height):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIIHH', depth, pid, drawable, width, height))
        return self.send_request(xcb.Request(buf.getvalue(), 53, True, True),
                                 xcb.VoidCookie())

    def CreatePixmap(self, depth, pid, drawable, width, height):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIIHH', depth, pid, drawable, width, height))
        return self.send_request(xcb.Request(buf.getvalue(), 53, True, False),
                                 xcb.VoidCookie())

    def FreePixmapChecked(self, pixmap):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', pixmap))
        return self.send_request(xcb.Request(buf.getvalue(), 54, True, True),
                                 xcb.VoidCookie())

    def FreePixmap(self, pixmap):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', pixmap))
        return self.send_request(xcb.Request(buf.getvalue(), 54, True, False),
                                 xcb.VoidCookie())

    def CreateGCChecked(self, cid, drawable, value_mask, value_list):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', cid, drawable, value_mask))
        buf.write(str(buffer(array('I', value_list))))
        return self.send_request(xcb.Request(buf.getvalue(), 55, True, True),
                                 xcb.VoidCookie())

    def CreateGC(self, cid, drawable, value_mask, value_list):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', cid, drawable, value_mask))
        buf.write(str(buffer(array('I', value_list))))
        return self.send_request(xcb.Request(buf.getvalue(), 55, True, False),
                                 xcb.VoidCookie())

    def ChangeGCChecked(self, gc, value_mask, value_list):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', gc, value_mask))
        buf.write(str(buffer(array('I', value_list))))
        return self.send_request(xcb.Request(buf.getvalue(), 56, True, True),
                                 xcb.VoidCookie())

    def ChangeGC(self, gc, value_mask, value_list):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', gc, value_mask))
        buf.write(str(buffer(array('I', value_list))))
        return self.send_request(xcb.Request(buf.getvalue(), 56, True, False),
                                 xcb.VoidCookie())

    def CopyGCChecked(self, src_gc, dst_gc, value_mask):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', src_gc, dst_gc, value_mask))
        return self.send_request(xcb.Request(buf.getvalue(), 57, True, True),
                                 xcb.VoidCookie())

    def CopyGC(self, src_gc, dst_gc, value_mask):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIII', src_gc, dst_gc, value_mask))
        return self.send_request(xcb.Request(buf.getvalue(), 57, True, False),
                                 xcb.VoidCookie())

    def SetDashesChecked(self, gc, dash_offset, dashes_len, dashes):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIHH', gc, dash_offset, dashes_len))
        buf.write(str(buffer(array('B', dashes))))
        return self.send_request(xcb.Request(buf.getvalue(), 58, True, True),
                                 xcb.VoidCookie())

    def SetDashes(self, gc, dash_offset, dashes_len, dashes):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIHH', gc, dash_offset, dashes_len))
        buf.write(str(buffer(array('B', dashes))))
        return self.send_request(xcb.Request(buf.getvalue(), 58, True, False),
                                 xcb.VoidCookie())

    def SetClipRectanglesChecked(self, ordering, gc, clip_x_origin, clip_y_origin, rectangles_len, rectangles):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIhh', ordering, gc, clip_x_origin, clip_y_origin))
        for elt in xcb.Iterator(rectangles, 4, 'rectangles', True):
            buf.write(pack('hhHH', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 59, True, True),
                                 xcb.VoidCookie())

    def SetClipRectangles(self, ordering, gc, clip_x_origin, clip_y_origin, rectangles_len, rectangles):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIhh', ordering, gc, clip_x_origin, clip_y_origin))
        for elt in xcb.Iterator(rectangles, 4, 'rectangles', True):
            buf.write(pack('hhHH', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 59, True, False),
                                 xcb.VoidCookie())

    def FreeGCChecked(self, gc):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', gc))
        return self.send_request(xcb.Request(buf.getvalue(), 60, True, True),
                                 xcb.VoidCookie())

    def FreeGC(self, gc):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', gc))
        return self.send_request(xcb.Request(buf.getvalue(), 60, True, False),
                                 xcb.VoidCookie())

    def ClearAreaChecked(self, exposures, window, x, y, width, height):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIhhHH', exposures, window, x, y, width, height))
        return self.send_request(xcb.Request(buf.getvalue(), 61, True, True),
                                 xcb.VoidCookie())

    def ClearArea(self, exposures, window, x, y, width, height):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIhhHH', exposures, window, x, y, width, height))
        return self.send_request(xcb.Request(buf.getvalue(), 61, True, False),
                                 xcb.VoidCookie())

    def CopyAreaChecked(self, src_drawable, dst_drawable, gc, src_x, src_y, dst_x, dst_y, width, height):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIhhhhHH', src_drawable, dst_drawable, gc, src_x, src_y, dst_x, dst_y, width, height))
        return self.send_request(xcb.Request(buf.getvalue(), 62, True, True),
                                 xcb.VoidCookie())

    def CopyArea(self, src_drawable, dst_drawable, gc, src_x, src_y, dst_x, dst_y, width, height):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIhhhhHH', src_drawable, dst_drawable, gc, src_x, src_y, dst_x, dst_y, width, height))
        return self.send_request(xcb.Request(buf.getvalue(), 62, True, False),
                                 xcb.VoidCookie())

    def CopyPlaneChecked(self, src_drawable, dst_drawable, gc, src_x, src_y, dst_x, dst_y, width, height, bit_plane):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIhhhhHHI', src_drawable, dst_drawable, gc, src_x, src_y, dst_x, dst_y, width, height, bit_plane))
        return self.send_request(xcb.Request(buf.getvalue(), 63, True, True),
                                 xcb.VoidCookie())

    def CopyPlane(self, src_drawable, dst_drawable, gc, src_x, src_y, dst_x, dst_y, width, height, bit_plane):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIhhhhHHI', src_drawable, dst_drawable, gc, src_x, src_y, dst_x, dst_y, width, height, bit_plane))
        return self.send_request(xcb.Request(buf.getvalue(), 63, True, False),
                                 xcb.VoidCookie())

    def PolyPointChecked(self, coordinate_mode, drawable, gc, points_len, points):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xII', coordinate_mode, drawable, gc))
        for elt in xcb.Iterator(points, 2, 'points', True):
            buf.write(pack('hh', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 64, True, True),
                                 xcb.VoidCookie())

    def PolyPoint(self, coordinate_mode, drawable, gc, points_len, points):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xII', coordinate_mode, drawable, gc))
        for elt in xcb.Iterator(points, 2, 'points', True):
            buf.write(pack('hh', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 64, True, False),
                                 xcb.VoidCookie())

    def PolyLineChecked(self, coordinate_mode, drawable, gc, points_len, points):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xII', coordinate_mode, drawable, gc))
        for elt in xcb.Iterator(points, 2, 'points', True):
            buf.write(pack('hh', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 65, True, True),
                                 xcb.VoidCookie())

    def PolyLine(self, coordinate_mode, drawable, gc, points_len, points):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xII', coordinate_mode, drawable, gc))
        for elt in xcb.Iterator(points, 2, 'points', True):
            buf.write(pack('hh', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 65, True, False),
                                 xcb.VoidCookie())

    def PolySegmentChecked(self, drawable, gc, segments_len, segments):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', drawable, gc))
        for elt in xcb.Iterator(segments, 4, 'segments', True):
            buf.write(pack('hhhh', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 66, True, True),
                                 xcb.VoidCookie())

    def PolySegment(self, drawable, gc, segments_len, segments):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', drawable, gc))
        for elt in xcb.Iterator(segments, 4, 'segments', True):
            buf.write(pack('hhhh', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 66, True, False),
                                 xcb.VoidCookie())

    def PolyRectangleChecked(self, drawable, gc, rectangles_len, rectangles):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', drawable, gc))
        for elt in xcb.Iterator(rectangles, 4, 'rectangles', True):
            buf.write(pack('hhHH', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 67, True, True),
                                 xcb.VoidCookie())

    def PolyRectangle(self, drawable, gc, rectangles_len, rectangles):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', drawable, gc))
        for elt in xcb.Iterator(rectangles, 4, 'rectangles', True):
            buf.write(pack('hhHH', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 67, True, False),
                                 xcb.VoidCookie())

    def PolyArcChecked(self, drawable, gc, arcs_len, arcs):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', drawable, gc))
        for elt in xcb.Iterator(arcs, 6, 'arcs', True):
            buf.write(pack('hhHHhh', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 68, True, True),
                                 xcb.VoidCookie())

    def PolyArc(self, drawable, gc, arcs_len, arcs):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', drawable, gc))
        for elt in xcb.Iterator(arcs, 6, 'arcs', True):
            buf.write(pack('hhHHhh', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 68, True, False),
                                 xcb.VoidCookie())

    def FillPolyChecked(self, drawable, gc, shape, coordinate_mode, points_len, points):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIBB2x', drawable, gc, shape, coordinate_mode))
        for elt in xcb.Iterator(points, 2, 'points', True):
            buf.write(pack('hh', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 69, True, True),
                                 xcb.VoidCookie())

    def FillPoly(self, drawable, gc, shape, coordinate_mode, points_len, points):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIBB2x', drawable, gc, shape, coordinate_mode))
        for elt in xcb.Iterator(points, 2, 'points', True):
            buf.write(pack('hh', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 69, True, False),
                                 xcb.VoidCookie())

    def PolyFillRectangleChecked(self, drawable, gc, rectangles_len, rectangles):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', drawable, gc))
        for elt in xcb.Iterator(rectangles, 4, 'rectangles', True):
            buf.write(pack('hhHH', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 70, True, True),
                                 xcb.VoidCookie())

    def PolyFillRectangle(self, drawable, gc, rectangles_len, rectangles):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', drawable, gc))
        for elt in xcb.Iterator(rectangles, 4, 'rectangles', True):
            buf.write(pack('hhHH', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 70, True, False),
                                 xcb.VoidCookie())

    def PolyFillArcChecked(self, drawable, gc, arcs_len, arcs):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', drawable, gc))
        for elt in xcb.Iterator(arcs, 6, 'arcs', True):
            buf.write(pack('hhHHhh', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 71, True, True),
                                 xcb.VoidCookie())

    def PolyFillArc(self, drawable, gc, arcs_len, arcs):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', drawable, gc))
        for elt in xcb.Iterator(arcs, 6, 'arcs', True):
            buf.write(pack('hhHHhh', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 71, True, False),
                                 xcb.VoidCookie())

    def PutImageChecked(self, format, drawable, gc, width, height, dst_x, dst_y, left_pad, depth, data_len, data):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIIHHhhBB2x', format, drawable, gc, width, height, dst_x, dst_y, left_pad, depth))
        buf.write(str(buffer(array('B', data))))
        return self.send_request(xcb.Request(buf.getvalue(), 72, True, True),
                                 xcb.VoidCookie())

    def PutImage(self, format, drawable, gc, width, height, dst_x, dst_y, left_pad, depth, data_len, data):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIIHHhhBB2x', format, drawable, gc, width, height, dst_x, dst_y, left_pad, depth))
        buf.write(str(buffer(array('B', data))))
        return self.send_request(xcb.Request(buf.getvalue(), 72, True, False),
                                 xcb.VoidCookie())

    def GetImage(self, format, drawable, x, y, width, height, plane_mask):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIhhHHI', format, drawable, x, y, width, height, plane_mask))
        return self.send_request(xcb.Request(buf.getvalue(), 73, False, True),
                                 GetImageCookie(),
                                 GetImageReply)

    def GetImageUnchecked(self, format, drawable, x, y, width, height, plane_mask):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIhhHHI', format, drawable, x, y, width, height, plane_mask))
        return self.send_request(xcb.Request(buf.getvalue(), 73, False, False),
                                 GetImageCookie(),
                                 GetImageReply)

    def PolyText8Checked(self, drawable, gc, x, y, items_len, items):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIhh', drawable, gc, x, y))
        buf.write(str(buffer(array('B', items))))
        return self.send_request(xcb.Request(buf.getvalue(), 74, True, True),
                                 xcb.VoidCookie())

    def PolyText8(self, drawable, gc, x, y, items_len, items):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIhh', drawable, gc, x, y))
        buf.write(str(buffer(array('B', items))))
        return self.send_request(xcb.Request(buf.getvalue(), 74, True, False),
                                 xcb.VoidCookie())

    def PolyText16Checked(self, drawable, gc, x, y, items_len, items):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIhh', drawable, gc, x, y))
        buf.write(str(buffer(array('B', items))))
        return self.send_request(xcb.Request(buf.getvalue(), 75, True, True),
                                 xcb.VoidCookie())

    def PolyText16(self, drawable, gc, x, y, items_len, items):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIhh', drawable, gc, x, y))
        buf.write(str(buffer(array('B', items))))
        return self.send_request(xcb.Request(buf.getvalue(), 75, True, False),
                                 xcb.VoidCookie())

    def ImageText8Checked(self, string_len, drawable, gc, x, y, string):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIIhh', string_len, drawable, gc, x, y))
        buf.write(str(buffer(array('b', string))))
        return self.send_request(xcb.Request(buf.getvalue(), 76, True, True),
                                 xcb.VoidCookie())

    def ImageText8(self, string_len, drawable, gc, x, y, string):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIIhh', string_len, drawable, gc, x, y))
        buf.write(str(buffer(array('b', string))))
        return self.send_request(xcb.Request(buf.getvalue(), 76, True, False),
                                 xcb.VoidCookie())

    def ImageText16Checked(self, string_len, drawable, gc, x, y, string):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIIhh', string_len, drawable, gc, x, y))
        for elt in xcb.Iterator(string, 2, 'string', True):
            buf.write(pack('BB', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 77, True, True),
                                 xcb.VoidCookie())

    def ImageText16(self, string_len, drawable, gc, x, y, string):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIIhh', string_len, drawable, gc, x, y))
        for elt in xcb.Iterator(string, 2, 'string', True):
            buf.write(pack('BB', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 77, True, False),
                                 xcb.VoidCookie())

    def CreateColormapChecked(self, alloc, mid, window, visual):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIII', alloc, mid, window, visual))
        return self.send_request(xcb.Request(buf.getvalue(), 78, True, True),
                                 xcb.VoidCookie())

    def CreateColormap(self, alloc, mid, window, visual):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIII', alloc, mid, window, visual))
        return self.send_request(xcb.Request(buf.getvalue(), 78, True, False),
                                 xcb.VoidCookie())

    def FreeColormapChecked(self, cmap):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', cmap))
        return self.send_request(xcb.Request(buf.getvalue(), 79, True, True),
                                 xcb.VoidCookie())

    def FreeColormap(self, cmap):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', cmap))
        return self.send_request(xcb.Request(buf.getvalue(), 79, True, False),
                                 xcb.VoidCookie())

    def CopyColormapAndFreeChecked(self, mid, src_cmap):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', mid, src_cmap))
        return self.send_request(xcb.Request(buf.getvalue(), 80, True, True),
                                 xcb.VoidCookie())

    def CopyColormapAndFree(self, mid, src_cmap):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', mid, src_cmap))
        return self.send_request(xcb.Request(buf.getvalue(), 80, True, False),
                                 xcb.VoidCookie())

    def InstallColormapChecked(self, cmap):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', cmap))
        return self.send_request(xcb.Request(buf.getvalue(), 81, True, True),
                                 xcb.VoidCookie())

    def InstallColormap(self, cmap):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', cmap))
        return self.send_request(xcb.Request(buf.getvalue(), 81, True, False),
                                 xcb.VoidCookie())

    def UninstallColormapChecked(self, cmap):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', cmap))
        return self.send_request(xcb.Request(buf.getvalue(), 82, True, True),
                                 xcb.VoidCookie())

    def UninstallColormap(self, cmap):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', cmap))
        return self.send_request(xcb.Request(buf.getvalue(), 82, True, False),
                                 xcb.VoidCookie())

    def ListInstalledColormaps(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 83, False, True),
                                 ListInstalledColormapsCookie(),
                                 ListInstalledColormapsReply)

    def ListInstalledColormapsUnchecked(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 83, False, False),
                                 ListInstalledColormapsCookie(),
                                 ListInstalledColormapsReply)

    def AllocColor(self, cmap, red, green, blue):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIHHH2x', cmap, red, green, blue))
        return self.send_request(xcb.Request(buf.getvalue(), 84, False, True),
                                 AllocColorCookie(),
                                 AllocColorReply)

    def AllocColorUnchecked(self, cmap, red, green, blue):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIHHH2x', cmap, red, green, blue))
        return self.send_request(xcb.Request(buf.getvalue(), 84, False, False),
                                 AllocColorCookie(),
                                 AllocColorReply)

    def AllocNamedColor(self, cmap, name_len, name):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIH2x', cmap, name_len))
        buf.write(str(buffer(array('b', name))))
        return self.send_request(xcb.Request(buf.getvalue(), 85, False, True),
                                 AllocNamedColorCookie(),
                                 AllocNamedColorReply)

    def AllocNamedColorUnchecked(self, cmap, name_len, name):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIH2x', cmap, name_len))
        buf.write(str(buffer(array('b', name))))
        return self.send_request(xcb.Request(buf.getvalue(), 85, False, False),
                                 AllocNamedColorCookie(),
                                 AllocNamedColorReply)

    def AllocColorCells(self, contiguous, cmap, colors, planes):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIHH', contiguous, cmap, colors, planes))
        return self.send_request(xcb.Request(buf.getvalue(), 86, False, True),
                                 AllocColorCellsCookie(),
                                 AllocColorCellsReply)

    def AllocColorCellsUnchecked(self, contiguous, cmap, colors, planes):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIHH', contiguous, cmap, colors, planes))
        return self.send_request(xcb.Request(buf.getvalue(), 86, False, False),
                                 AllocColorCellsCookie(),
                                 AllocColorCellsReply)

    def AllocColorPlanes(self, contiguous, cmap, colors, reds, greens, blues):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIHHHH', contiguous, cmap, colors, reds, greens, blues))
        return self.send_request(xcb.Request(buf.getvalue(), 87, False, True),
                                 AllocColorPlanesCookie(),
                                 AllocColorPlanesReply)

    def AllocColorPlanesUnchecked(self, contiguous, cmap, colors, reds, greens, blues):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIHHHH', contiguous, cmap, colors, reds, greens, blues))
        return self.send_request(xcb.Request(buf.getvalue(), 87, False, False),
                                 AllocColorPlanesCookie(),
                                 AllocColorPlanesReply)

    def FreeColorsChecked(self, cmap, plane_mask, pixels_len, pixels):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', cmap, plane_mask))
        buf.write(str(buffer(array('I', pixels))))
        return self.send_request(xcb.Request(buf.getvalue(), 88, True, True),
                                 xcb.VoidCookie())

    def FreeColors(self, cmap, plane_mask, pixels_len, pixels):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', cmap, plane_mask))
        buf.write(str(buffer(array('I', pixels))))
        return self.send_request(xcb.Request(buf.getvalue(), 88, True, False),
                                 xcb.VoidCookie())

    def StoreColorsChecked(self, cmap, items_len, items):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', cmap))
        for elt in xcb.Iterator(items, 5, 'items', True):
            buf.write(pack('IHHHBx', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 89, True, True),
                                 xcb.VoidCookie())

    def StoreColors(self, cmap, items_len, items):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', cmap))
        for elt in xcb.Iterator(items, 5, 'items', True):
            buf.write(pack('IHHHBx', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 89, True, False),
                                 xcb.VoidCookie())

    def StoreNamedColorChecked(self, flags, cmap, pixel, name_len, name):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIIH2x', flags, cmap, pixel, name_len))
        buf.write(str(buffer(array('b', name))))
        return self.send_request(xcb.Request(buf.getvalue(), 90, True, True),
                                 xcb.VoidCookie())

    def StoreNamedColor(self, flags, cmap, pixel, name_len, name):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIIH2x', flags, cmap, pixel, name_len))
        buf.write(str(buffer(array('b', name))))
        return self.send_request(xcb.Request(buf.getvalue(), 90, True, False),
                                 xcb.VoidCookie())

    def QueryColors(self, cmap, pixels_len, pixels):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', cmap))
        buf.write(str(buffer(array('I', pixels))))
        return self.send_request(xcb.Request(buf.getvalue(), 91, False, True),
                                 QueryColorsCookie(),
                                 QueryColorsReply)

    def QueryColorsUnchecked(self, cmap, pixels_len, pixels):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', cmap))
        buf.write(str(buffer(array('I', pixels))))
        return self.send_request(xcb.Request(buf.getvalue(), 91, False, False),
                                 QueryColorsCookie(),
                                 QueryColorsReply)

    def LookupColor(self, cmap, name_len, name):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIH2x', cmap, name_len))
        buf.write(str(buffer(array('b', name))))
        return self.send_request(xcb.Request(buf.getvalue(), 92, False, True),
                                 LookupColorCookie(),
                                 LookupColorReply)

    def LookupColorUnchecked(self, cmap, name_len, name):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIH2x', cmap, name_len))
        buf.write(str(buffer(array('b', name))))
        return self.send_request(xcb.Request(buf.getvalue(), 92, False, False),
                                 LookupColorCookie(),
                                 LookupColorReply)

    def CreateCursorChecked(self, cid, source, mask, fore_red, fore_green, fore_blue, back_red, back_green, back_blue, x, y):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIHHHHHHHH', cid, source, mask, fore_red, fore_green, fore_blue, back_red, back_green, back_blue, x, y))
        return self.send_request(xcb.Request(buf.getvalue(), 93, True, True),
                                 xcb.VoidCookie())

    def CreateCursor(self, cid, source, mask, fore_red, fore_green, fore_blue, back_red, back_green, back_blue, x, y):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIHHHHHHHH', cid, source, mask, fore_red, fore_green, fore_blue, back_red, back_green, back_blue, x, y))
        return self.send_request(xcb.Request(buf.getvalue(), 93, True, False),
                                 xcb.VoidCookie())

    def CreateGlyphCursorChecked(self, cid, source_font, mask_font, source_char, mask_char, fore_red, fore_green, fore_blue, back_red, back_green, back_blue):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIHHHHHHHH', cid, source_font, mask_font, source_char, mask_char, fore_red, fore_green, fore_blue, back_red, back_green, back_blue))
        return self.send_request(xcb.Request(buf.getvalue(), 94, True, True),
                                 xcb.VoidCookie())

    def CreateGlyphCursor(self, cid, source_font, mask_font, source_char, mask_char, fore_red, fore_green, fore_blue, back_red, back_green, back_blue):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIIHHHHHHHH', cid, source_font, mask_font, source_char, mask_char, fore_red, fore_green, fore_blue, back_red, back_green, back_blue))
        return self.send_request(xcb.Request(buf.getvalue(), 94, True, False),
                                 xcb.VoidCookie())

    def FreeCursorChecked(self, cursor):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', cursor))
        return self.send_request(xcb.Request(buf.getvalue(), 95, True, True),
                                 xcb.VoidCookie())

    def FreeCursor(self, cursor):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', cursor))
        return self.send_request(xcb.Request(buf.getvalue(), 95, True, False),
                                 xcb.VoidCookie())

    def RecolorCursorChecked(self, cursor, fore_red, fore_green, fore_blue, back_red, back_green, back_blue):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIHHHHHH', cursor, fore_red, fore_green, fore_blue, back_red, back_green, back_blue))
        return self.send_request(xcb.Request(buf.getvalue(), 96, True, True),
                                 xcb.VoidCookie())

    def RecolorCursor(self, cursor, fore_red, fore_green, fore_blue, back_red, back_green, back_blue):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIHHHHHH', cursor, fore_red, fore_green, fore_blue, back_red, back_green, back_blue))
        return self.send_request(xcb.Request(buf.getvalue(), 96, True, False),
                                 xcb.VoidCookie())

    def QueryBestSize(self, _class, drawable, width, height):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIHH', _class, drawable, width, height))
        return self.send_request(xcb.Request(buf.getvalue(), 97, False, True),
                                 QueryBestSizeCookie(),
                                 QueryBestSizeReply)

    def QueryBestSizeUnchecked(self, _class, drawable, width, height):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xIHH', _class, drawable, width, height))
        return self.send_request(xcb.Request(buf.getvalue(), 97, False, False),
                                 QueryBestSizeCookie(),
                                 QueryBestSizeReply)

    def QueryExtension(self, name_len, name):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xH2x', name_len))
        buf.write(str(buffer(array('b', name))))
        return self.send_request(xcb.Request(buf.getvalue(), 98, False, True),
                                 QueryExtensionCookie(),
                                 QueryExtensionReply)

    def QueryExtensionUnchecked(self, name_len, name):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xH2x', name_len))
        buf.write(str(buffer(array('b', name))))
        return self.send_request(xcb.Request(buf.getvalue(), 98, False, False),
                                 QueryExtensionCookie(),
                                 QueryExtensionReply)

    def ListExtensions(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 99, False, True),
                                 ListExtensionsCookie(),
                                 ListExtensionsReply)

    def ListExtensionsUnchecked(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 99, False, False),
                                 ListExtensionsCookie(),
                                 ListExtensionsReply)

    def ChangeKeyboardMappingChecked(self, keycode_count, first_keycode, keysyms_per_keycode, keysyms):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xBB', keycode_count, first_keycode, keysyms_per_keycode))
        buf.write(str(buffer(array('I', keysyms))))
        return self.send_request(xcb.Request(buf.getvalue(), 100, True, True),
                                 xcb.VoidCookie())

    def ChangeKeyboardMapping(self, keycode_count, first_keycode, keysyms_per_keycode, keysyms):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xBB', keycode_count, first_keycode, keysyms_per_keycode))
        buf.write(str(buffer(array('I', keysyms))))
        return self.send_request(xcb.Request(buf.getvalue(), 100, True, False),
                                 xcb.VoidCookie())

    def GetKeyboardMapping(self, first_keycode, count):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBB', first_keycode, count))
        return self.send_request(xcb.Request(buf.getvalue(), 101, False, True),
                                 GetKeyboardMappingCookie(),
                                 GetKeyboardMappingReply)

    def GetKeyboardMappingUnchecked(self, first_keycode, count):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBB', first_keycode, count))
        return self.send_request(xcb.Request(buf.getvalue(), 101, False, False),
                                 GetKeyboardMappingCookie(),
                                 GetKeyboardMappingReply)

    def ChangeKeyboardControlChecked(self, value_mask, value_list):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', value_mask))
        buf.write(str(buffer(array('I', value_list))))
        return self.send_request(xcb.Request(buf.getvalue(), 102, True, True),
                                 xcb.VoidCookie())

    def ChangeKeyboardControl(self, value_mask, value_list):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', value_mask))
        buf.write(str(buffer(array('I', value_list))))
        return self.send_request(xcb.Request(buf.getvalue(), 102, True, False),
                                 xcb.VoidCookie())

    def GetKeyboardControl(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 103, False, True),
                                 GetKeyboardControlCookie(),
                                 GetKeyboardControlReply)

    def GetKeyboardControlUnchecked(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 103, False, False),
                                 GetKeyboardControlCookie(),
                                 GetKeyboardControlReply)

    def BellChecked(self, percent):
        buf = cStringIO.StringIO()
        buf.write(pack('xb2x', percent))
        return self.send_request(xcb.Request(buf.getvalue(), 104, True, True),
                                 xcb.VoidCookie())

    def Bell(self, percent):
        buf = cStringIO.StringIO()
        buf.write(pack('xb2x', percent))
        return self.send_request(xcb.Request(buf.getvalue(), 104, True, False),
                                 xcb.VoidCookie())

    def ChangePointerControlChecked(self, acceleration_numerator, acceleration_denominator, threshold, do_acceleration, do_threshold):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xhhhBB', acceleration_numerator, acceleration_denominator, threshold, do_acceleration, do_threshold))
        return self.send_request(xcb.Request(buf.getvalue(), 105, True, True),
                                 xcb.VoidCookie())

    def ChangePointerControl(self, acceleration_numerator, acceleration_denominator, threshold, do_acceleration, do_threshold):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xhhhBB', acceleration_numerator, acceleration_denominator, threshold, do_acceleration, do_threshold))
        return self.send_request(xcb.Request(buf.getvalue(), 105, True, False),
                                 xcb.VoidCookie())

    def GetPointerControl(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 106, False, True),
                                 GetPointerControlCookie(),
                                 GetPointerControlReply)

    def GetPointerControlUnchecked(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 106, False, False),
                                 GetPointerControlCookie(),
                                 GetPointerControlReply)

    def SetScreenSaverChecked(self, timeout, interval, prefer_blanking, allow_exposures):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xhhBB', timeout, interval, prefer_blanking, allow_exposures))
        return self.send_request(xcb.Request(buf.getvalue(), 107, True, True),
                                 xcb.VoidCookie())

    def SetScreenSaver(self, timeout, interval, prefer_blanking, allow_exposures):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xhhBB', timeout, interval, prefer_blanking, allow_exposures))
        return self.send_request(xcb.Request(buf.getvalue(), 107, True, False),
                                 xcb.VoidCookie())

    def GetScreenSaver(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 108, False, True),
                                 GetScreenSaverCookie(),
                                 GetScreenSaverReply)

    def GetScreenSaverUnchecked(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 108, False, False),
                                 GetScreenSaverCookie(),
                                 GetScreenSaverReply)

    def ChangeHostsChecked(self, mode, family, address_len, address):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xBxH', mode, family, address_len))
        buf.write(str(buffer(array('b', address))))
        return self.send_request(xcb.Request(buf.getvalue(), 109, True, True),
                                 xcb.VoidCookie())

    def ChangeHosts(self, mode, family, address_len, address):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2xBxH', mode, family, address_len))
        buf.write(str(buffer(array('b', address))))
        return self.send_request(xcb.Request(buf.getvalue(), 109, True, False),
                                 xcb.VoidCookie())

    def ListHosts(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 110, False, True),
                                 ListHostsCookie(),
                                 ListHostsReply)

    def ListHostsUnchecked(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 110, False, False),
                                 ListHostsCookie(),
                                 ListHostsReply)

    def SetAccessControlChecked(self, mode):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2x', mode))
        return self.send_request(xcb.Request(buf.getvalue(), 111, True, True),
                                 xcb.VoidCookie())

    def SetAccessControl(self, mode):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2x', mode))
        return self.send_request(xcb.Request(buf.getvalue(), 111, True, False),
                                 xcb.VoidCookie())

    def SetCloseDownModeChecked(self, mode):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2x', mode))
        return self.send_request(xcb.Request(buf.getvalue(), 112, True, True),
                                 xcb.VoidCookie())

    def SetCloseDownMode(self, mode):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2x', mode))
        return self.send_request(xcb.Request(buf.getvalue(), 112, True, False),
                                 xcb.VoidCookie())

    def KillClientChecked(self, resource):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', resource))
        return self.send_request(xcb.Request(buf.getvalue(), 113, True, True),
                                 xcb.VoidCookie())

    def KillClient(self, resource):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', resource))
        return self.send_request(xcb.Request(buf.getvalue(), 113, True, False),
                                 xcb.VoidCookie())

    def RotatePropertiesChecked(self, window, atoms_len, delta, atoms):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIHh', window, atoms_len, delta))
        buf.write(str(buffer(array('I', atoms))))
        return self.send_request(xcb.Request(buf.getvalue(), 114, True, True),
                                 xcb.VoidCookie())

    def RotateProperties(self, window, atoms_len, delta, atoms):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIHh', window, atoms_len, delta))
        buf.write(str(buffer(array('I', atoms))))
        return self.send_request(xcb.Request(buf.getvalue(), 114, True, False),
                                 xcb.VoidCookie())

    def ForceScreenSaverChecked(self, mode):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2x', mode))
        return self.send_request(xcb.Request(buf.getvalue(), 115, True, True),
                                 xcb.VoidCookie())

    def ForceScreenSaver(self, mode):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2x', mode))
        return self.send_request(xcb.Request(buf.getvalue(), 115, True, False),
                                 xcb.VoidCookie())

    def SetPointerMapping(self, map_len, map):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2x', map_len))
        buf.write(str(buffer(array('B', map))))
        return self.send_request(xcb.Request(buf.getvalue(), 116, False, True),
                                 SetPointerMappingCookie(),
                                 SetPointerMappingReply)

    def SetPointerMappingUnchecked(self, map_len, map):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2x', map_len))
        buf.write(str(buffer(array('B', map))))
        return self.send_request(xcb.Request(buf.getvalue(), 116, False, False),
                                 SetPointerMappingCookie(),
                                 SetPointerMappingReply)

    def GetPointerMapping(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 117, False, True),
                                 GetPointerMappingCookie(),
                                 GetPointerMappingReply)

    def GetPointerMappingUnchecked(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 117, False, False),
                                 GetPointerMappingCookie(),
                                 GetPointerMappingReply)

    def SetModifierMapping(self, keycodes_per_modifier, keycodes):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2x', keycodes_per_modifier))
        buf.write(str(buffer(array('B', keycodes))))
        return self.send_request(xcb.Request(buf.getvalue(), 118, False, True),
                                 SetModifierMappingCookie(),
                                 SetModifierMappingReply)

    def SetModifierMappingUnchecked(self, keycodes_per_modifier, keycodes):
        buf = cStringIO.StringIO()
        buf.write(pack('xB2x', keycodes_per_modifier))
        buf.write(str(buffer(array('B', keycodes))))
        return self.send_request(xcb.Request(buf.getvalue(), 118, False, False),
                                 SetModifierMappingCookie(),
                                 SetModifierMappingReply)

    def GetModifierMapping(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 119, False, True),
                                 GetModifierMappingCookie(),
                                 GetModifierMappingReply)

    def GetModifierMappingUnchecked(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 119, False, False),
                                 GetModifierMappingCookie(),
                                 GetModifierMappingReply)

    def NoOperationChecked(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 127, True, True),
                                 xcb.VoidCookie())

    def NoOperation(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 127, True, False),
                                 xcb.VoidCookie())

_events = {
    2 : KeyPressEvent,
    3 : KeyReleaseEvent,
    4 : ButtonPressEvent,
    5 : ButtonReleaseEvent,
    6 : MotionNotifyEvent,
    7 : EnterNotifyEvent,
    8 : LeaveNotifyEvent,
    9 : FocusInEvent,
    10 : FocusOutEvent,
    11 : KeymapNotifyEvent,
    12 : ExposeEvent,
    13 : GraphicsExposureEvent,
    14 : NoExposureEvent,
    15 : VisibilityNotifyEvent,
    16 : CreateNotifyEvent,
    17 : DestroyNotifyEvent,
    18 : UnmapNotifyEvent,
    19 : MapNotifyEvent,
    20 : MapRequestEvent,
    21 : ReparentNotifyEvent,
    22 : ConfigureNotifyEvent,
    23 : ConfigureRequestEvent,
    24 : GravityNotifyEvent,
    25 : ResizeRequestEvent,
    26 : CirculateNotifyEvent,
    27 : CirculateRequestEvent,
    28 : PropertyNotifyEvent,
    29 : SelectionClearEvent,
    30 : SelectionRequestEvent,
    31 : SelectionNotifyEvent,
    32 : ColormapNotifyEvent,
    33 : ClientMessageEvent,
    34 : MappingNotifyEvent,
}

_errors = {
    1 : (RequestError, BadRequest),
    2 : (ValueError, BadValue),
    3 : (WindowError, BadWindow),
    4 : (PixmapError, BadPixmap),
    5 : (AtomError, BadAtom),
    6 : (CursorError, BadCursor),
    7 : (FontError, BadFont),
    8 : (MatchError, BadMatch),
    9 : (DrawableError, BadDrawable),
    10 : (AccessError, BadAccess),
    11 : (AllocError, BadAlloc),
    12 : (ColormapError, BadColormap),
    13 : (GContextError, BadGContext),
    14 : (IDChoiceError, BadIDChoice),
    15 : (NameError, BadName),
    16 : (LengthError, BadLength),
    17 : (ImplementationError, BadImplementation),
}

xcb._add_core(xprotoExtension, Setup, _events, _errors)
