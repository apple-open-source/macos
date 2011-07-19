#
# This file generated automatically from xinput.xml by py_client.py.
# Edit at your peril.
#

import xcb
import cStringIO
from struct import pack, unpack_from
from array import array
import xproto

MAJOR_VERSION = 1
MINOR_VERSION = 4

key = xcb.ExtensionKey('XInputExtension')

class ValuatorMode:
    Relative = 0
    Absolute = 1

class PropagateMode:
    AddToList = 0
    DeleteFromList = 1

class GetExtensionVersionCookie(xcb.Cookie):
    pass

class GetExtensionVersionReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.server_major, self.server_minor, self.present,) = unpack_from('xx2x4xHHB19x', self, count)

class DeviceUse:
    IsXPointer = 0
    IsXKeyboard = 1
    IsXExtensionDevice = 2
    IsXExtensionKeyboard = 3
    IsXExtensionPointer = 4

class DeviceInfo(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.device_type, self.device_id, self.num_class_info, self.device_use,) = unpack_from('IBBBx', self, count)

class ListInputDevicesCookie(xcb.Cookie):
    pass

class ListInputDevicesReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.devices_len,) = unpack_from('xx2x4xB23x', self, count)
        count += 32
        self.devices = xcb.List(self, count, self.devices_len, DeviceInfo, 8)

class InputClass:
    Key = 0
    Button = 1
    Valuator = 2
    Feedback = 3
    Proximity = 4
    Focus = 5
    Other = 6

class InputInfo(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.class_id, self.len,) = unpack_from('BB', self, count)

class KeyInfo(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.class_id, self.len, self.min_keycode, self.max_keycode, self.num_keys,) = unpack_from('BBBBH2x', self, count)

class ButtonInfo(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.class_id, self.len, self.num_buttons,) = unpack_from('BBH', self, count)

class AxisInfo(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.resolution, self.minimum, self.maximum,) = unpack_from('Iii', self, count)

class ValuatorInfo(xcb.Struct):
    def __init__(self, parent, offset):
        xcb.Struct.__init__(self, parent, offset)
        count = 0
        (self.class_id, self.len, self.axes_len, self.mode, self.motion_size,) = unpack_from('BBBBI', self, count)
        count += 8
        self.axes = xcb.List(self, count, self.axes_len, AxisInfo, 12)
        count += len(self.axes.buf())
        xcb._resize_obj(self, count)

class InputClassInfo(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.class_id, self.event_type_base,) = unpack_from('BB', self, count)

class OpenDeviceCookie(xcb.Cookie):
    pass

class OpenDeviceReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.num_classes,) = unpack_from('xx2x4xB23x', self, count)
        count += 32
        self.class_info = xcb.List(self, count, self.num_classes, InputClassInfo, 2)

class SetDeviceModeCookie(xcb.Cookie):
    pass

class SetDeviceModeReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.status,) = unpack_from('xx2x4xB23x', self, count)

class GetSelectedExtensionEventsCookie(xcb.Cookie):
    pass

class GetSelectedExtensionEventsReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.num_this_classes, self.num_all_classes,) = unpack_from('xx2x4xHH20x', self, count)
        count += 32
        self.this_classes = xcb.List(self, count, self.num_this_classes, 'I', 4)
        count += len(self.this_classes.buf())
        count += xcb.type_pad(4, count)
        self.all_classes = xcb.List(self, count, self.num_all_classes, 'I', 4)

class GetDeviceDontPropagateListCookie(xcb.Cookie):
    pass

class GetDeviceDontPropagateListReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.num_classes,) = unpack_from('xx2x4xH22x', self, count)
        count += 32
        self.classes = xcb.List(self, count, self.num_classes, 'I', 4)

class GetDeviceMotionEventsCookie(xcb.Cookie):
    pass

class GetDeviceMotionEventsReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.num_coords, self.num_axes, self.device_mode,) = unpack_from('xx2x4xIBB18x', self, count)

class DeviceTimeCoord(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.time,) = unpack_from('I', self, count)

class ChangeKeyboardDeviceCookie(xcb.Cookie):
    pass

class ChangeKeyboardDeviceReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.status,) = unpack_from('xx2x4xB23x', self, count)

class ChangePointerDeviceCookie(xcb.Cookie):
    pass

class ChangePointerDeviceReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.status,) = unpack_from('xx2x4xB23x', self, count)

class GrabDeviceCookie(xcb.Cookie):
    pass

class GrabDeviceReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.status,) = unpack_from('xx2x4xB23x', self, count)

class DeviceInputMode:
    AsyncThisDevice = 0
    SyncThisDevice = 1
    ReplayThisDevice = 2
    AsyncOtherDevices = 3
    AsyncAll = 4
    SyncAll = 5

class GetDeviceFocusCookie(xcb.Cookie):
    pass

class GetDeviceFocusReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.focus, self.time, self.revert_to,) = unpack_from('xx2x4xIIB15x', self, count)

class GetFeedbackControlCookie(xcb.Cookie):
    pass

class GetFeedbackControlReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.num_feedback,) = unpack_from('xx2x4xH22x', self, count)

class FeedbackClass:
    Keyboard = 0
    Pointer = 1
    String = 2
    Integer = 3
    Led = 4
    Bell = 5

class FeedbackState(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.class_id, self.id, self.len,) = unpack_from('BBH', self, count)

class KbdFeedbackState(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.class_id, self.id, self.len, self.pitch, self.duration, self.led_mask, self.led_values, self.global_auto_repeat, self.click, self.percent,) = unpack_from('BBHHHIIBBBx', self, count)
        count += 20
        self.auto_repeats = xcb.List(self, count, 32, 'B', 1)

class PtrFeedbackState(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.class_id, self.id, self.len, self.accel_num, self.accel_denom, self.threshold,) = unpack_from('BBH2xHHH', self, count)

class IntegerFeedbackState(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.class_id, self.id, self.len, self.resolution, self.min_value, self.max_value,) = unpack_from('BBHIii', self, count)

class StringFeedbackState(xcb.Struct):
    def __init__(self, parent, offset):
        xcb.Struct.__init__(self, parent, offset)
        count = 0
        (self.class_id, self.id, self.len, self.max_symbols, self.num_keysyms,) = unpack_from('BBHHH', self, count)
        count += 8
        self.keysyms = xcb.List(self, count, self.num_keysyms, 'I', 4)
        count += len(self.keysyms.buf())
        xcb._resize_obj(self, count)

class BellFeedbackState(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.class_id, self.id, self.len, self.percent, self.pitch, self.duration,) = unpack_from('BBHB3xHH', self, count)

class LedFeedbackState(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.class_id, self.id, self.len, self.led_mask, self.led_values,) = unpack_from('BBHII', self, count)

class FeedbackCtl(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.class_id, self.id, self.len,) = unpack_from('BBH', self, count)

class KbdFeedbackCtl(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.class_id, self.id, self.len, self.key, self.auto_repeat_mode, self.key_click_percent, self.bell_percent, self.bell_pitch, self.bell_duration, self.led_mask, self.led_values,) = unpack_from('BBHBBbbhhII', self, count)

class PtrFeedbackCtl(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.class_id, self.id, self.len, self.num, self.denom, self.threshold,) = unpack_from('BBH2xhhh', self, count)

class IntegerFeedbackCtl(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.class_id, self.id, self.len, self.int_to_display,) = unpack_from('BBHi', self, count)

class StringFeedbackCtl(xcb.Struct):
    def __init__(self, parent, offset):
        xcb.Struct.__init__(self, parent, offset)
        count = 0
        (self.class_id, self.id, self.len, self.num_keysyms,) = unpack_from('BBH2xH', self, count)
        count += 8
        self.keysyms = xcb.List(self, count, self.num_keysyms, 'I', 4)
        count += len(self.keysyms.buf())
        xcb._resize_obj(self, count)

class BellFeedbackCtl(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.class_id, self.id, self.len, self.percent, self.pitch, self.duration,) = unpack_from('BBHb3xhh', self, count)

class LedFeedbackCtl(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.class_id, self.id, self.len, self.led_mask, self.led_values,) = unpack_from('BBHII', self, count)

class GetDeviceKeyMappingCookie(xcb.Cookie):
    pass

class GetDeviceKeyMappingReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.keysyms_per_keycode,) = unpack_from('xx2x4xB23x', self, count)
        count += 32
        self.keysyms = xcb.List(self, count, self.length, 'I', 4)

class GetDeviceModifierMappingCookie(xcb.Cookie):
    pass

class GetDeviceModifierMappingReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.keycodes_per_modifier,) = unpack_from('xx2x4xB23x', self, count)
        count += 32
        self.keymaps = xcb.List(self, count, (self.keycodes_per_modifier * 8), 'B', 1)

class SetDeviceModifierMappingCookie(xcb.Cookie):
    pass

class SetDeviceModifierMappingReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.status,) = unpack_from('xx2x4xB23x', self, count)

class GetDeviceButtonMappingCookie(xcb.Cookie):
    pass

class GetDeviceButtonMappingReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.map_size,) = unpack_from('xx2x4xB23x', self, count)
        count += 32
        self.map = xcb.List(self, count, self.map_size, 'B', 1)

class SetDeviceButtonMappingCookie(xcb.Cookie):
    pass

class SetDeviceButtonMappingReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.status,) = unpack_from('xx2x4xB23x', self, count)

class QueryDeviceStateCookie(xcb.Cookie):
    pass

class QueryDeviceStateReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.num_classes,) = unpack_from('xx2x4xB23x', self, count)

class InputState(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.class_id, self.len, self.num_items,) = unpack_from('BBB', self, count)

class KeyState(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.class_id, self.len, self.num_keys,) = unpack_from('BBBx', self, count)
        count += 4
        self.keys = xcb.List(self, count, 32, 'B', 1)

class ButtonState(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.class_id, self.len, self.num_buttons,) = unpack_from('BBBx', self, count)
        count += 4
        self.buttons = xcb.List(self, count, 32, 'B', 1)

class ValuatorState(xcb.Struct):
    def __init__(self, parent, offset):
        xcb.Struct.__init__(self, parent, offset)
        count = 0
        (self.class_id, self.len, self.num_valuators, self.mode,) = unpack_from('BBBB', self, count)
        count += 4
        self.valuators = xcb.List(self, count, self.num_valuators, 'I', 4)
        count += len(self.valuators.buf())
        xcb._resize_obj(self, count)

class SetDeviceValuatorsCookie(xcb.Cookie):
    pass

class SetDeviceValuatorsReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.status,) = unpack_from('xx2x4xB23x', self, count)

class GetDeviceControlCookie(xcb.Cookie):
    pass

class GetDeviceControlReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.status,) = unpack_from('xx2x4xB23x', self, count)

class DeviceState(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.control_id, self.len,) = unpack_from('HH', self, count)

class DeviceResolutionState(xcb.Struct):
    def __init__(self, parent, offset):
        xcb.Struct.__init__(self, parent, offset)
        count = 0
        (self.control_id, self.len, self.num_valuators,) = unpack_from('HHI', self, count)
        count += 8
        self.resolution_values = xcb.List(self, count, self.num_valuators, 'I', 4)
        count += len(self.resolution_values.buf())
        count += xcb.type_pad(4, count)
        self.resolution_min = xcb.List(self, count, self.num_valuators, 'I', 4)
        count += len(self.resolution_min.buf())
        count += xcb.type_pad(4, count)
        self.resolution_max = xcb.List(self, count, self.num_valuators, 'I', 4)
        count += len(self.resolution_max.buf())
        xcb._resize_obj(self, count)

class DeviceAbsCalibState(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.control_id, self.len, self.min_x, self.max_x, self.min_y, self.max_y, self.flip_x, self.flip_y, self.rotation, self.button_threshold,) = unpack_from('HHiiiiIIII', self, count)

class DeviceAbsAreaState(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.control_id, self.len, self.offset_x, self.offset_y, self.width, self.height, self.screen, self.following,) = unpack_from('HHIIIIII', self, count)

class DeviceCoreState(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.control_id, self.len, self.status, self.iscore,) = unpack_from('HHBB2x', self, count)

class DeviceEnableState(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.control_id, self.len, self.enable,) = unpack_from('HHB3x', self, count)

class DeviceCtl(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.control_id, self.len,) = unpack_from('HH', self, count)

class DeviceResolutionCtl(xcb.Struct):
    def __init__(self, parent, offset):
        xcb.Struct.__init__(self, parent, offset)
        count = 0
        (self.control_id, self.len, self.first_valuator, self.num_valuators,) = unpack_from('HHBB', self, count)
        count += 6
        self.resolution_values = xcb.List(self, count, self.num_valuators, 'I', 4)
        count += len(self.resolution_values.buf())
        xcb._resize_obj(self, count)

class DeviceAbsCalibCtl(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.control_id, self.len, self.min_x, self.max_x, self.min_y, self.max_y, self.flip_x, self.flip_y, self.rotation, self.button_threshold,) = unpack_from('HHiiiiIIII', self, count)

class DeviceAbsAreaCtrl(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.control_id, self.len, self.offset_x, self.offset_y, self.width, self.height, self.screen, self.following,) = unpack_from('HHIIiiiI', self, count)

class DeviceCoreCtrl(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.control_id, self.len, self.status,) = unpack_from('HHB3x', self, count)

class DeviceEnableCtrl(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.control_id, self.len, self.enable,) = unpack_from('HHB3x', self, count)

class DeviceValuatorEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.device_id, self.device_state, self.num_valuators, self.first_valuator,) = unpack_from('xB2xHBB', self, count)
        count += 8
        self.valuators = xcb.List(self, count, 6, 'i', 4)

class DeviceKeyPressEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.detail, self.time, self.root, self.event, self.child, self.root_x, self.root_y, self.event_x, self.event_y, self.state, self.same_screen, self.device_id,) = unpack_from('xB2xIIIIhhhhHBB', self, count)

class DeviceKeyReleaseEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.detail, self.time, self.root, self.event, self.child, self.root_x, self.root_y, self.event_x, self.event_y, self.state, self.same_screen, self.device_id,) = unpack_from('xB2xIIIIhhhhHBB', self, count)

class DeviceButtonPressEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.detail, self.time, self.root, self.event, self.child, self.root_x, self.root_y, self.event_x, self.event_y, self.state, self.same_screen, self.device_id,) = unpack_from('xB2xIIIIhhhhHBB', self, count)

class DeviceButtonReleaseEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.detail, self.time, self.root, self.event, self.child, self.root_x, self.root_y, self.event_x, self.event_y, self.state, self.same_screen, self.device_id,) = unpack_from('xB2xIIIIhhhhHBB', self, count)

class DeviceMotionNotifyEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.detail, self.time, self.root, self.event, self.child, self.root_x, self.root_y, self.event_x, self.event_y, self.state, self.same_screen, self.device_id,) = unpack_from('xB2xIIIIhhhhHBB', self, count)

class ProximityInEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.detail, self.time, self.root, self.event, self.child, self.root_x, self.root_y, self.event_x, self.event_y, self.state, self.same_screen, self.device_id,) = unpack_from('xB2xIIIIhhhhHBB', self, count)

class ProximityOutEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.detail, self.time, self.root, self.event, self.child, self.root_x, self.root_y, self.event_x, self.event_y, self.state, self.same_screen, self.device_id,) = unpack_from('xB2xIIIIhhhhHBB', self, count)

class FocusInEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.detail, self.time, self.window, self.mode, self.device_id,) = unpack_from('xB2xIIBB18x', self, count)

class FocusOutEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.detail, self.time, self.window, self.mode, self.device_id,) = unpack_from('xB2xIIBB18x', self, count)

class DeviceStateNotifyEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.device_id, self.time, self.num_keys, self.num_buttons, self.num_valuators, self.classes_reported,) = unpack_from('xB2xIBBBB', self, count)
        count += 12
        self.buttons = xcb.List(self, count, 4, 'B', 1)
        count += len(self.buttons.buf())
        count += xcb.type_pad(1, count)
        self.keys = xcb.List(self, count, 4, 'B', 1)
        count += len(self.keys.buf())
        count += xcb.type_pad(4, count)
        self.valuators = xcb.List(self, count, 3, 'I', 4)

class DeviceMappingNotifyEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.device_id, self.request, self.first_keycode, self.count, self.time,) = unpack_from('xB2xBBBxI20x', self, count)

class ChangeDeviceNotifyEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.device_id, self.time, self.request,) = unpack_from('xB2xIB23x', self, count)

class DeviceKeyStateNotifyEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.device_id,) = unpack_from('xB2x', self, count)
        count += 4
        self.keys = xcb.List(self, count, 28, 'B', 1)

class DeviceButtonStateNotifyEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.device_id,) = unpack_from('xB2x', self, count)
        count += 4
        self.buttons = xcb.List(self, count, 28, 'B', 1)

class DevicePresenceNotifyEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.time, self.devchange, self.device_id, self.control,) = unpack_from('xx2xIBBH20x', self, count)

class DeviceError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)

class BadDevice(xcb.ProtocolException):
    pass

class EventError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)

class BadEvent(xcb.ProtocolException):
    pass

class ModeError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)

class BadMode(xcb.ProtocolException):
    pass

class DeviceBusyError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)

class BadDeviceBusy(xcb.ProtocolException):
    pass

class ClassError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)

class BadClass(xcb.ProtocolException):
    pass

class xinputExtension(xcb.Extension):

    def GetExtensionVersion(self, name_len, name):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xH2x', name_len))
        buf.write(str(buffer(array('b', name))))
        return self.send_request(xcb.Request(buf.getvalue(), 1, False, True),
                                 GetExtensionVersionCookie(),
                                 GetExtensionVersionReply)

    def GetExtensionVersionUnchecked(self, name_len, name):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xH2x', name_len))
        buf.write(str(buffer(array('b', name))))
        return self.send_request(xcb.Request(buf.getvalue(), 1, False, False),
                                 GetExtensionVersionCookie(),
                                 GetExtensionVersionReply)

    def ListInputDevices(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 2, False, True),
                                 ListInputDevicesCookie(),
                                 ListInputDevicesReply)

    def ListInputDevicesUnchecked(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 2, False, False),
                                 ListInputDevicesCookie(),
                                 ListInputDevicesReply)

    def OpenDevice(self, device_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xB3x', device_id))
        return self.send_request(xcb.Request(buf.getvalue(), 3, False, True),
                                 OpenDeviceCookie(),
                                 OpenDeviceReply)

    def OpenDeviceUnchecked(self, device_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xB3x', device_id))
        return self.send_request(xcb.Request(buf.getvalue(), 3, False, False),
                                 OpenDeviceCookie(),
                                 OpenDeviceReply)

    def CloseDeviceChecked(self, device_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xB3x', device_id))
        return self.send_request(xcb.Request(buf.getvalue(), 4, True, True),
                                 xcb.VoidCookie())

    def CloseDevice(self, device_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xB3x', device_id))
        return self.send_request(xcb.Request(buf.getvalue(), 4, True, False),
                                 xcb.VoidCookie())

    def SetDeviceMode(self, device_id, mode):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBB2x', device_id, mode))
        return self.send_request(xcb.Request(buf.getvalue(), 5, False, True),
                                 SetDeviceModeCookie(),
                                 SetDeviceModeReply)

    def SetDeviceModeUnchecked(self, device_id, mode):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBB2x', device_id, mode))
        return self.send_request(xcb.Request(buf.getvalue(), 5, False, False),
                                 SetDeviceModeCookie(),
                                 SetDeviceModeReply)

    def SelectExtensionEventChecked(self, window, num_classes, classes):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIH2x', window, num_classes))
        buf.write(str(buffer(array('I', classes))))
        return self.send_request(xcb.Request(buf.getvalue(), 6, True, True),
                                 xcb.VoidCookie())

    def SelectExtensionEvent(self, window, num_classes, classes):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIH2x', window, num_classes))
        buf.write(str(buffer(array('I', classes))))
        return self.send_request(xcb.Request(buf.getvalue(), 6, True, False),
                                 xcb.VoidCookie())

    def GetSelectedExtensionEvents(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 7, False, True),
                                 GetSelectedExtensionEventsCookie(),
                                 GetSelectedExtensionEventsReply)

    def GetSelectedExtensionEventsUnchecked(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 7, False, False),
                                 GetSelectedExtensionEventsCookie(),
                                 GetSelectedExtensionEventsReply)

    def ChangeDeviceDontPropagateListChecked(self, window, num_classes, mode, classes):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIHBx', window, num_classes, mode))
        buf.write(str(buffer(array('I', classes))))
        return self.send_request(xcb.Request(buf.getvalue(), 8, True, True),
                                 xcb.VoidCookie())

    def ChangeDeviceDontPropagateList(self, window, num_classes, mode, classes):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIHBx', window, num_classes, mode))
        buf.write(str(buffer(array('I', classes))))
        return self.send_request(xcb.Request(buf.getvalue(), 8, True, False),
                                 xcb.VoidCookie())

    def GetDeviceDontPropagateList(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 9, False, True),
                                 GetDeviceDontPropagateListCookie(),
                                 GetDeviceDontPropagateListReply)

    def GetDeviceDontPropagateListUnchecked(self, window):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', window))
        return self.send_request(xcb.Request(buf.getvalue(), 9, False, False),
                                 GetDeviceDontPropagateListCookie(),
                                 GetDeviceDontPropagateListReply)

    def GetDeviceMotionEvents(self, start, stop, device_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIB', start, stop, device_id))
        return self.send_request(xcb.Request(buf.getvalue(), 10, False, True),
                                 GetDeviceMotionEventsCookie(),
                                 GetDeviceMotionEventsReply)

    def GetDeviceMotionEventsUnchecked(self, start, stop, device_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIB', start, stop, device_id))
        return self.send_request(xcb.Request(buf.getvalue(), 10, False, False),
                                 GetDeviceMotionEventsCookie(),
                                 GetDeviceMotionEventsReply)

    def ChangeKeyboardDevice(self, device_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xB3x', device_id))
        return self.send_request(xcb.Request(buf.getvalue(), 11, False, True),
                                 ChangeKeyboardDeviceCookie(),
                                 ChangeKeyboardDeviceReply)

    def ChangeKeyboardDeviceUnchecked(self, device_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xB3x', device_id))
        return self.send_request(xcb.Request(buf.getvalue(), 11, False, False),
                                 ChangeKeyboardDeviceCookie(),
                                 ChangeKeyboardDeviceReply)

    def ChangePointerDevice(self, x_axis, y_axis, device_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBBBx', x_axis, y_axis, device_id))
        return self.send_request(xcb.Request(buf.getvalue(), 12, False, True),
                                 ChangePointerDeviceCookie(),
                                 ChangePointerDeviceReply)

    def ChangePointerDeviceUnchecked(self, x_axis, y_axis, device_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBBBx', x_axis, y_axis, device_id))
        return self.send_request(xcb.Request(buf.getvalue(), 12, False, False),
                                 ChangePointerDeviceCookie(),
                                 ChangePointerDeviceReply)

    def GrabDevice(self, grab_window, time, num_classes, this_device_mode, other_device_mode, owner_events, device_id, classes):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIHBBBB2x', grab_window, time, num_classes, this_device_mode, other_device_mode, owner_events, device_id))
        buf.write(str(buffer(array('I', classes))))
        return self.send_request(xcb.Request(buf.getvalue(), 13, False, True),
                                 GrabDeviceCookie(),
                                 GrabDeviceReply)

    def GrabDeviceUnchecked(self, grab_window, time, num_classes, this_device_mode, other_device_mode, owner_events, device_id, classes):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIHBBBB2x', grab_window, time, num_classes, this_device_mode, other_device_mode, owner_events, device_id))
        buf.write(str(buffer(array('I', classes))))
        return self.send_request(xcb.Request(buf.getvalue(), 13, False, False),
                                 GrabDeviceCookie(),
                                 GrabDeviceReply)

    def UngrabDeviceChecked(self, time, device_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIB', time, device_id))
        return self.send_request(xcb.Request(buf.getvalue(), 14, True, True),
                                 xcb.VoidCookie())

    def UngrabDevice(self, time, device_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIB', time, device_id))
        return self.send_request(xcb.Request(buf.getvalue(), 14, True, False),
                                 xcb.VoidCookie())

    def GrabDeviceKeyChecked(self, grab_window, num_classes, modifiers, modifier_device, grabbed_device, key, this_device_mode, other_device_mode, owner_events, classes):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIHHBBBBBB2x', grab_window, num_classes, modifiers, modifier_device, grabbed_device, key, this_device_mode, other_device_mode, owner_events))
        buf.write(str(buffer(array('I', classes))))
        return self.send_request(xcb.Request(buf.getvalue(), 15, True, True),
                                 xcb.VoidCookie())

    def GrabDeviceKey(self, grab_window, num_classes, modifiers, modifier_device, grabbed_device, key, this_device_mode, other_device_mode, owner_events, classes):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIHHBBBBBB2x', grab_window, num_classes, modifiers, modifier_device, grabbed_device, key, this_device_mode, other_device_mode, owner_events))
        buf.write(str(buffer(array('I', classes))))
        return self.send_request(xcb.Request(buf.getvalue(), 15, True, False),
                                 xcb.VoidCookie())

    def UngrabDeviceKeyChecked(self, grabWindow, modifiers, modifier_device, key, grabbed_device):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIHBBB', grabWindow, modifiers, modifier_device, key, grabbed_device))
        return self.send_request(xcb.Request(buf.getvalue(), 16, True, True),
                                 xcb.VoidCookie())

    def UngrabDeviceKey(self, grabWindow, modifiers, modifier_device, key, grabbed_device):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIHBBB', grabWindow, modifiers, modifier_device, key, grabbed_device))
        return self.send_request(xcb.Request(buf.getvalue(), 16, True, False),
                                 xcb.VoidCookie())

    def GrabDeviceButtonChecked(self, grab_window, grabbed_device, modifier_device, num_classes, modifiers, this_device_mode, other_device_mode, button, owner_events, classes):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIBBHHBBBB2x', grab_window, grabbed_device, modifier_device, num_classes, modifiers, this_device_mode, other_device_mode, button, owner_events))
        buf.write(str(buffer(array('I', classes))))
        return self.send_request(xcb.Request(buf.getvalue(), 17, True, True),
                                 xcb.VoidCookie())

    def GrabDeviceButton(self, grab_window, grabbed_device, modifier_device, num_classes, modifiers, this_device_mode, other_device_mode, button, owner_events, classes):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIBBHHBBBB2x', grab_window, grabbed_device, modifier_device, num_classes, modifiers, this_device_mode, other_device_mode, button, owner_events))
        buf.write(str(buffer(array('I', classes))))
        return self.send_request(xcb.Request(buf.getvalue(), 17, True, False),
                                 xcb.VoidCookie())

    def UngrabDeviceButtonChecked(self, grab_window, modifiers, modifier_device, button, grabbed_device):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIHBBB', grab_window, modifiers, modifier_device, button, grabbed_device))
        return self.send_request(xcb.Request(buf.getvalue(), 18, True, True),
                                 xcb.VoidCookie())

    def UngrabDeviceButton(self, grab_window, modifiers, modifier_device, button, grabbed_device):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIHBBB', grab_window, modifiers, modifier_device, button, grabbed_device))
        return self.send_request(xcb.Request(buf.getvalue(), 18, True, False),
                                 xcb.VoidCookie())

    def AllowDeviceEventsChecked(self, time, mode, device_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIBB', time, mode, device_id))
        return self.send_request(xcb.Request(buf.getvalue(), 19, True, True),
                                 xcb.VoidCookie())

    def AllowDeviceEvents(self, time, mode, device_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIBB', time, mode, device_id))
        return self.send_request(xcb.Request(buf.getvalue(), 19, True, False),
                                 xcb.VoidCookie())

    def GetDeviceFocus(self, device_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xB3x', device_id))
        return self.send_request(xcb.Request(buf.getvalue(), 20, False, True),
                                 GetDeviceFocusCookie(),
                                 GetDeviceFocusReply)

    def GetDeviceFocusUnchecked(self, device_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xB3x', device_id))
        return self.send_request(xcb.Request(buf.getvalue(), 20, False, False),
                                 GetDeviceFocusCookie(),
                                 GetDeviceFocusReply)

    def SetDeviceFocusChecked(self, focus, time, revert_to, device_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIBB', focus, time, revert_to, device_id))
        return self.send_request(xcb.Request(buf.getvalue(), 21, True, True),
                                 xcb.VoidCookie())

    def SetDeviceFocus(self, focus, time, revert_to, device_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIIBB', focus, time, revert_to, device_id))
        return self.send_request(xcb.Request(buf.getvalue(), 21, True, False),
                                 xcb.VoidCookie())

    def GetFeedbackControl(self, device_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xB3x', device_id))
        return self.send_request(xcb.Request(buf.getvalue(), 22, False, True),
                                 GetFeedbackControlCookie(),
                                 GetFeedbackControlReply)

    def GetFeedbackControlUnchecked(self, device_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xB3x', device_id))
        return self.send_request(xcb.Request(buf.getvalue(), 22, False, False),
                                 GetFeedbackControlCookie(),
                                 GetFeedbackControlReply)

    def GetDeviceKeyMapping(self, device_id, first_keycode, count):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBBB', device_id, first_keycode, count))
        return self.send_request(xcb.Request(buf.getvalue(), 24, False, True),
                                 GetDeviceKeyMappingCookie(),
                                 GetDeviceKeyMappingReply)

    def GetDeviceKeyMappingUnchecked(self, device_id, first_keycode, count):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBBB', device_id, first_keycode, count))
        return self.send_request(xcb.Request(buf.getvalue(), 24, False, False),
                                 GetDeviceKeyMappingCookie(),
                                 GetDeviceKeyMappingReply)

    def ChangeDeviceKeyMappingChecked(self, device_id, first_keycode, keysyms_per_keycode, keycode_count, keysyms):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBBBB', device_id, first_keycode, keysyms_per_keycode, keycode_count))
        buf.write(str(buffer(array('I', keysyms))))
        return self.send_request(xcb.Request(buf.getvalue(), 25, True, True),
                                 xcb.VoidCookie())

    def ChangeDeviceKeyMapping(self, device_id, first_keycode, keysyms_per_keycode, keycode_count, keysyms):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBBBB', device_id, first_keycode, keysyms_per_keycode, keycode_count))
        buf.write(str(buffer(array('I', keysyms))))
        return self.send_request(xcb.Request(buf.getvalue(), 25, True, False),
                                 xcb.VoidCookie())

    def GetDeviceModifierMapping(self, device_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xB3x', device_id))
        return self.send_request(xcb.Request(buf.getvalue(), 26, False, True),
                                 GetDeviceModifierMappingCookie(),
                                 GetDeviceModifierMappingReply)

    def GetDeviceModifierMappingUnchecked(self, device_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xB3x', device_id))
        return self.send_request(xcb.Request(buf.getvalue(), 26, False, False),
                                 GetDeviceModifierMappingCookie(),
                                 GetDeviceModifierMappingReply)

    def SetDeviceModifierMapping(self, device_id, keycodes_per_modifier, keymaps):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBBx', device_id, keycodes_per_modifier))
        buf.write(str(buffer(array('B', keymaps))))
        return self.send_request(xcb.Request(buf.getvalue(), 27, False, True),
                                 SetDeviceModifierMappingCookie(),
                                 SetDeviceModifierMappingReply)

    def SetDeviceModifierMappingUnchecked(self, device_id, keycodes_per_modifier, keymaps):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBBx', device_id, keycodes_per_modifier))
        buf.write(str(buffer(array('B', keymaps))))
        return self.send_request(xcb.Request(buf.getvalue(), 27, False, False),
                                 SetDeviceModifierMappingCookie(),
                                 SetDeviceModifierMappingReply)

    def GetDeviceButtonMapping(self, device_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xB3x', device_id))
        return self.send_request(xcb.Request(buf.getvalue(), 28, False, True),
                                 GetDeviceButtonMappingCookie(),
                                 GetDeviceButtonMappingReply)

    def GetDeviceButtonMappingUnchecked(self, device_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xB3x', device_id))
        return self.send_request(xcb.Request(buf.getvalue(), 28, False, False),
                                 GetDeviceButtonMappingCookie(),
                                 GetDeviceButtonMappingReply)

    def SetDeviceButtonMapping(self, device_id, map_size, map):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBB2x', device_id, map_size))
        buf.write(str(buffer(array('B', map))))
        return self.send_request(xcb.Request(buf.getvalue(), 29, False, True),
                                 SetDeviceButtonMappingCookie(),
                                 SetDeviceButtonMappingReply)

    def SetDeviceButtonMappingUnchecked(self, device_id, map_size, map):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBB2x', device_id, map_size))
        buf.write(str(buffer(array('B', map))))
        return self.send_request(xcb.Request(buf.getvalue(), 29, False, False),
                                 SetDeviceButtonMappingCookie(),
                                 SetDeviceButtonMappingReply)

    def QueryDeviceState(self, device_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xB3x', device_id))
        return self.send_request(xcb.Request(buf.getvalue(), 30, False, True),
                                 QueryDeviceStateCookie(),
                                 QueryDeviceStateReply)

    def QueryDeviceStateUnchecked(self, device_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xB3x', device_id))
        return self.send_request(xcb.Request(buf.getvalue(), 30, False, False),
                                 QueryDeviceStateCookie(),
                                 QueryDeviceStateReply)

    def SendExtensionEventChecked(self, destination, device_id, propagate, num_classes, num_events, events, classes):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIBBHB3x', destination, device_id, propagate, num_classes, num_events))
        buf.write(str(buffer(array('b', events))))
        buf.write(str(buffer(array('I', classes))))
        return self.send_request(xcb.Request(buf.getvalue(), 31, True, True),
                                 xcb.VoidCookie())

    def SendExtensionEvent(self, destination, device_id, propagate, num_classes, num_events, events, classes):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIBBHB3x', destination, device_id, propagate, num_classes, num_events))
        buf.write(str(buffer(array('b', events))))
        buf.write(str(buffer(array('I', classes))))
        return self.send_request(xcb.Request(buf.getvalue(), 31, True, False),
                                 xcb.VoidCookie())

    def DeviceBellChecked(self, device_id, feedback_id, feedback_class, percent):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBBBb', device_id, feedback_id, feedback_class, percent))
        return self.send_request(xcb.Request(buf.getvalue(), 32, True, True),
                                 xcb.VoidCookie())

    def DeviceBell(self, device_id, feedback_id, feedback_class, percent):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBBBb', device_id, feedback_id, feedback_class, percent))
        return self.send_request(xcb.Request(buf.getvalue(), 32, True, False),
                                 xcb.VoidCookie())

    def SetDeviceValuators(self, device_id, first_valuator, num_valuators, valuators):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBBBx', device_id, first_valuator, num_valuators))
        buf.write(str(buffer(array('i', valuators))))
        return self.send_request(xcb.Request(buf.getvalue(), 33, False, True),
                                 SetDeviceValuatorsCookie(),
                                 SetDeviceValuatorsReply)

    def SetDeviceValuatorsUnchecked(self, device_id, first_valuator, num_valuators, valuators):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBBBx', device_id, first_valuator, num_valuators))
        buf.write(str(buffer(array('i', valuators))))
        return self.send_request(xcb.Request(buf.getvalue(), 33, False, False),
                                 SetDeviceValuatorsCookie(),
                                 SetDeviceValuatorsReply)

    def GetDeviceControl(self, control_id, device_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xHBx', control_id, device_id))
        return self.send_request(xcb.Request(buf.getvalue(), 34, False, True),
                                 GetDeviceControlCookie(),
                                 GetDeviceControlReply)

    def GetDeviceControlUnchecked(self, control_id, device_id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xHBx', control_id, device_id))
        return self.send_request(xcb.Request(buf.getvalue(), 34, False, False),
                                 GetDeviceControlCookie(),
                                 GetDeviceControlReply)

_events = {
    0 : DeviceValuatorEvent,
    1 : DeviceKeyPressEvent,
    2 : DeviceKeyReleaseEvent,
    3 : DeviceButtonPressEvent,
    4 : DeviceButtonReleaseEvent,
    5 : DeviceMotionNotifyEvent,
    8 : ProximityInEvent,
    9 : ProximityOutEvent,
    6 : FocusInEvent,
    7 : FocusOutEvent,
    10 : DeviceStateNotifyEvent,
    11 : DeviceMappingNotifyEvent,
    12 : ChangeDeviceNotifyEvent,
    13 : DeviceKeyStateNotifyEvent,
    14 : DeviceButtonStateNotifyEvent,
    15 : DevicePresenceNotifyEvent,
}

_errors = {
    0 : (DeviceError, BadDevice),
    1 : (EventError, BadEvent),
    2 : (ModeError, BadMode),
    3 : (DeviceBusyError, BadDeviceBusy),
    4 : (ClassError, BadClass),
}

xcb._add_ext(key, xinputExtension, _events, _errors)
