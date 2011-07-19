#
# This file generated automatically from sync.xml by py_client.py.
# Edit at your peril.
#

import xcb
import cStringIO
from struct import pack, unpack_from
from array import array
import xproto

MAJOR_VERSION = 3
MINOR_VERSION = 0

key = xcb.ExtensionKey('SYNC')

class ALARMSTATE:
    Active = 0
    Inactive = 1
    Destroyed = 2

class TESTTYPE:
    PositiveTransition = 0
    NegativeTransition = 1
    PositiveComparison = 2
    NegativeComparison = 3

class VALUETYPE:
    Absolute = 0
    Relative = 1

class CA:
    Counter = 1
    ValueType = 2
    Value = 4
    TestType = 8
    Delta = 16
    Events = 32

class INT64(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.hi, self.lo,) = unpack_from('iI', self, count)

class SYSTEMCOUNTER(xcb.Struct):
    def __init__(self, parent, offset):
        xcb.Struct.__init__(self, parent, offset)
        count = 0
        (self.counter,) = unpack_from('I', self, count)
        count += 4
        self.resolution = INT64(self, count, 8)
        count += 8
        (self.name_len,) = unpack_from('H', self, count)
        count += 2
        count += xcb.type_pad(1, count)
        self.name = xcb.List(self, count, self.name_len, 'b', 1)
        count += len(self.name.buf())
        xcb._resize_obj(self, count)

class TRIGGER(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        (self.counter, self.wait_type,) = unpack_from('II', self, count)
        count += 8
        self.wait_value = INT64(self, count, 8)
        count += 8
        count += xcb.type_pad(4, count)
        (self.test_type,) = unpack_from('I', self, count)

class WAITCONDITION(xcb.Struct):
    def __init__(self, parent, offset, size):
        xcb.Struct.__init__(self, parent, offset, size)
        count = 0
        self.trigger = TRIGGER(self, count, 20)
        count += 20
        count += xcb.type_pad(8, count)
        self.event_threshold = INT64(self, count, 8)

class CounterError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)
        count = 0
        (self.bad_counter, self.minor_opcode, self.major_opcode,) = unpack_from('xx2xIHB', self, count)

class BadCounter(xcb.ProtocolException):
    pass

class AlarmError(xcb.Error):
    def __init__(self, parent):
        xcb.Error.__init__(self, parent)
        count = 0
        (self.bad_alarm, self.minor_opcode, self.major_opcode,) = unpack_from('xx2xIHB', self, count)

class BadAlarm(xcb.ProtocolException):
    pass

class InitializeCookie(xcb.Cookie):
    pass

class InitializeReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.major_version, self.minor_version,) = unpack_from('xx2x4xBB22x', self, count)

class ListSystemCountersCookie(xcb.Cookie):
    pass

class ListSystemCountersReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.counters_len,) = unpack_from('xx2x4xI20x', self, count)
        count += 32
        self.counters = xcb.List(self, count, self.counters_len, SYSTEMCOUNTER, -1)

class QueryCounterCookie(xcb.Cookie):
    pass

class QueryCounterReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        count += 8
        self.counter_value = INT64(self, count, 8)

class QueryAlarmCookie(xcb.Cookie):
    pass

class QueryAlarmReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        count += 8
        self.trigger = TRIGGER(self, count, 20)
        count += 20
        count += xcb.type_pad(8, count)
        self.delta = INT64(self, count, 8)
        count += 8
        count += xcb.type_pad(4, count)
        (self.events, self.state,) = unpack_from('BB2x', self, count)

class GetPriorityCookie(xcb.Cookie):
    pass

class GetPriorityReply(xcb.Reply):
    def __init__(self, parent):
        xcb.Reply.__init__(self, parent)
        count = 0
        (self.priority,) = unpack_from('xx2x4xi', self, count)

class CounterNotifyEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.kind, self.counter,) = unpack_from('xB2xI', self, count)
        count += 8
        self.wait_value = INT64(self, count, 8)
        count += 8
        count += xcb.type_pad(8, count)
        self.counter_value = INT64(self, count, 8)
        count += 8
        count += xcb.type_pad(4, count)
        (self.timestamp, self.count, self.destroyed,) = unpack_from('IHBx', self, count)

class AlarmNotifyEvent(xcb.Event):
    def __init__(self, parent):
        xcb.Event.__init__(self, parent)
        count = 0
        (self.kind, self.alarm,) = unpack_from('xB2xI', self, count)
        count += 8
        self.counter_value = INT64(self, count, 8)
        count += 8
        count += xcb.type_pad(8, count)
        self.alarm_value = INT64(self, count, 8)
        count += 8
        count += xcb.type_pad(4, count)
        (self.timestamp, self.state,) = unpack_from('IB3x', self, count)

class syncExtension(xcb.Extension):

    def Initialize(self, desired_major_version, desired_minor_version):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBB', desired_major_version, desired_minor_version))
        return self.send_request(xcb.Request(buf.getvalue(), 0, False, True),
                                 InitializeCookie(),
                                 InitializeReply)

    def InitializeUnchecked(self, desired_major_version, desired_minor_version):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xBB', desired_major_version, desired_minor_version))
        return self.send_request(xcb.Request(buf.getvalue(), 0, False, False),
                                 InitializeCookie(),
                                 InitializeReply)

    def ListSystemCounters(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 1, False, True),
                                 ListSystemCountersCookie(),
                                 ListSystemCountersReply)

    def ListSystemCountersUnchecked(self, ):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        return self.send_request(xcb.Request(buf.getvalue(), 1, False, False),
                                 ListSystemCountersCookie(),
                                 ListSystemCountersReply)

    def CreateCounterChecked(self, id, initial_value):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', id))
        for elt in xcb.Iterator(initial_value, 2, 'initial_value', False):
            buf.write(pack('iI', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 2, True, True),
                                 xcb.VoidCookie())

    def CreateCounter(self, id, initial_value):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', id))
        for elt in xcb.Iterator(initial_value, 2, 'initial_value', False):
            buf.write(pack('iI', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 2, True, False),
                                 xcb.VoidCookie())

    def DestroyCounterChecked(self, counter):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', counter))
        return self.send_request(xcb.Request(buf.getvalue(), 6, True, True),
                                 xcb.VoidCookie())

    def DestroyCounter(self, counter):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', counter))
        return self.send_request(xcb.Request(buf.getvalue(), 6, True, False),
                                 xcb.VoidCookie())

    def QueryCounter(self, counter):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', counter))
        return self.send_request(xcb.Request(buf.getvalue(), 5, False, True),
                                 QueryCounterCookie(),
                                 QueryCounterReply)

    def QueryCounterUnchecked(self, counter):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', counter))
        return self.send_request(xcb.Request(buf.getvalue(), 5, False, False),
                                 QueryCounterCookie(),
                                 QueryCounterReply)

    def AwaitChecked(self, wait_list_len, wait_list):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        for elt in xcb.Iterator(wait_list, 7, 'wait_list', True):
            buf.write(pack('IIiIIiI', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 7, True, True),
                                 xcb.VoidCookie())

    def Await(self, wait_list_len, wait_list):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2x', ))
        for elt in xcb.Iterator(wait_list, 7, 'wait_list', True):
            buf.write(pack('IIiIIiI', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 7, True, False),
                                 xcb.VoidCookie())

    def ChangeCounterChecked(self, counter, amount):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', counter))
        for elt in xcb.Iterator(amount, 2, 'amount', False):
            buf.write(pack('iI', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 4, True, True),
                                 xcb.VoidCookie())

    def ChangeCounter(self, counter, amount):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', counter))
        for elt in xcb.Iterator(amount, 2, 'amount', False):
            buf.write(pack('iI', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 4, True, False),
                                 xcb.VoidCookie())

    def SetCounterChecked(self, counter, value):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', counter))
        for elt in xcb.Iterator(value, 2, 'value', False):
            buf.write(pack('iI', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 3, True, True),
                                 xcb.VoidCookie())

    def SetCounter(self, counter, value):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', counter))
        for elt in xcb.Iterator(value, 2, 'value', False):
            buf.write(pack('iI', *elt))
        return self.send_request(xcb.Request(buf.getvalue(), 3, True, False),
                                 xcb.VoidCookie())

    def CreateAlarmChecked(self, id, value_mask, value_list):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', id, value_mask))
        buf.write(str(buffer(array('I', value_list))))
        return self.send_request(xcb.Request(buf.getvalue(), 8, True, True),
                                 xcb.VoidCookie())

    def CreateAlarm(self, id, value_mask, value_list):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', id, value_mask))
        buf.write(str(buffer(array('I', value_list))))
        return self.send_request(xcb.Request(buf.getvalue(), 8, True, False),
                                 xcb.VoidCookie())

    def ChangeAlarmChecked(self, id, value_mask, value_list):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', id, value_mask))
        buf.write(str(buffer(array('I', value_list))))
        return self.send_request(xcb.Request(buf.getvalue(), 9, True, True),
                                 xcb.VoidCookie())

    def ChangeAlarm(self, id, value_mask, value_list):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xII', id, value_mask))
        buf.write(str(buffer(array('I', value_list))))
        return self.send_request(xcb.Request(buf.getvalue(), 9, True, False),
                                 xcb.VoidCookie())

    def DestroyAlarmChecked(self, alarm):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', alarm))
        return self.send_request(xcb.Request(buf.getvalue(), 11, True, True),
                                 xcb.VoidCookie())

    def DestroyAlarm(self, alarm):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', alarm))
        return self.send_request(xcb.Request(buf.getvalue(), 11, True, False),
                                 xcb.VoidCookie())

    def QueryAlarm(self, alarm):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', alarm))
        return self.send_request(xcb.Request(buf.getvalue(), 10, False, True),
                                 QueryAlarmCookie(),
                                 QueryAlarmReply)

    def QueryAlarmUnchecked(self, alarm):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', alarm))
        return self.send_request(xcb.Request(buf.getvalue(), 10, False, False),
                                 QueryAlarmCookie(),
                                 QueryAlarmReply)

    def SetPriorityChecked(self, id, priority):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIi', id, priority))
        return self.send_request(xcb.Request(buf.getvalue(), 12, True, True),
                                 xcb.VoidCookie())

    def SetPriority(self, id, priority):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xIi', id, priority))
        return self.send_request(xcb.Request(buf.getvalue(), 12, True, False),
                                 xcb.VoidCookie())

    def GetPriority(self, id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', id))
        return self.send_request(xcb.Request(buf.getvalue(), 13, False, True),
                                 GetPriorityCookie(),
                                 GetPriorityReply)

    def GetPriorityUnchecked(self, id):
        buf = cStringIO.StringIO()
        buf.write(pack('xx2xI', id))
        return self.send_request(xcb.Request(buf.getvalue(), 13, False, False),
                                 GetPriorityCookie(),
                                 GetPriorityReply)

_events = {
    0 : CounterNotifyEvent,
    1 : AlarmNotifyEvent,
}

_errors = {
    0 : (CounterError, BadCounter),
    1 : (AlarmError, BadAlarm),
}

xcb._add_ext(key, syncExtension, _events, _errors)
