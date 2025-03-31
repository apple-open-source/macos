"""LLDB macros and functions for accessing IOSF data structures"""

from __future__ import print_function
import json
from pprint import pprint, pformat

from six import PY3

from builtins import hex
from builtins import zip
from builtins import map
from builtins import range
from builtins import object
from collections import OrderedDict, deque
from random import choice
from string import ascii_lowercase

if PY3:
    from io import StringIO
else:
    from cStringIO import StringIO

from functools import wraps
import re
import sys
import copy
import time

import lldb
from xnu import dereference, sizeof, Cast, LazyTarget, lldb_command, kern, ShowCurrentAbsTime, unsigned, CastIOKitClass, GetConnectionProtocol
from utils import GetEnumValue


#####################################
# Globals
#####################################

registered_iosf_commands = OrderedDict()

#####################################
# Constants / Class Defn
#####################################

class InvalidTypeError(ValueError):
    """Raise when an SBType cannot be found (i.e. if the symbol is not loaded)"""

class ServicePlaneKeys(object):
    PARENT_LINKS = 0
    CHILD_LINKS = 1

class BSDDiscs(object):
    def __init__(self):
        self.physical_interconnect =""
        self.physical_interconnect_location =""
        self.media_objs = OrderedDict()


#####################################
# Utility functions
#####################################

def IterateTAILQ_HEAD(headval, element_name, list_prefix=''):
    """ iterate over a TAILQ_HEAD in kernel. refer to bsd/sys/queue.h
        params:
            headval      - value : value object representing the head of the list
            element_name - str   :  string name of the field which holds the list links.
            list_prefix  - str   : use 's' here to iterate STAILQ_HEAD instead
        returns:
            A generator does not return. It is used for iterating.
            value : an object that is of type as headval->tqh_first. Always a pointer object
        example usage:
          list_head = kern.GetGlobalVariable('mountlist')
          for entryobj in IterateTAILQ_HEAD(list_head, 'mnt_list'):
            print GetEntrySummary(entryobj)
    """
    iter_val = headval.__getattr__(list_prefix + 'tqh_first')
    while unsigned(iter_val) != 0 :
        yield iter_val
        iter_val = iter_val.__getattr__(element_name).__getattr__(list_prefix + 'tqe_next')
    #end of yield loop

def iosf_command(command_name):
    def iosf_command_decorator(function):
        registered_iosf_commands[command_name] = function.__doc__
        @lldb_command(command_name)
        @wraps(function)
        def iosf_command_wrapper(*args, **kwargs):
            return function(*args, **kwargs)
        return iosf_command_wrapper
    return iosf_command_decorator

def selectRegistryObject(registry_search_type_name):
    """A decorator which grabs all addresses in the cmd_args kwarg
    and calls the decorated function which each.  If no addresses were
    passed in, it searches the registry for the registry_search_type_name
    and calls the function for each matching object"""
    def selectRegistryObjectInnerDecorator(function):
        function.__doc__ += '''

Accepts one or more pointers to controllers as arguments, otherwise
searches the registry and executes the command for all objects of the
relevant type.'''
        @wraps(function)
        def selectionWrapper(*args, **kwargs):
            return_vals = []
            if 'cmd_args' in kwargs:
                cmd_args = kwargs['cmd_args']
                del kwargs['cmd_args']
            else:
                cmd_args = None
            registry_search_type = SBTypeFromName(registry_search_type_name)
            if not registry_search_type.IsValid():
                print('LLDB could not find any symbols for the {} type.'.format(registry_search_type_name))
                print('Make sure the machine is expected to have this object type')
                print('and that all kext symbols are loaded.')
                return

            if args:
                # python function was called directly, pass along the arguments
                return_vals.append(function(*args, **kwargs))
            elif cmd_args:
                # called as a lldb macro with arguments
                for address in cmd_args:
                    cvalue_obj = kern.GetValueFromAddress(address)
                    if IsInstanceOfBaseClass(cvalue_obj, registry_search_type):
                        print('Object at {}'.format(getCValueSignature(cvalue_obj)))
                        return_vals.append(function(cvalue_obj, *args, **kwargs))
                    elif 'void' in cvalue_obj.GetSBValue().type.name.lower():
                        print('LLDB could not determine the type of the object at {}.'.format(address))
                        print('Make sure the address is correct and that all kext symbols')
                        print('are loaded and slid to the correct offset address.')
                        return
                    else:
                        print('Object at {} is not the expected type.'.format(address))
                        print('({} expected, {} found)'.format(registry_search_type_name,
                                                               cvalue_obj.GetSBValue().type.name))
                        return
            else:
                # otherwise, search the registry
                try:
                    registry_objects = FindRegistryObjectsByTypeName(registry_search_type_name)
                except InvalidTypeError:
                    registry_objects = None
                if registry_objects:
                    for idx, cvalue_obj in enumerate(registry_objects):
                        if idx > 0:
                            print(' ')
                        print('Object at {}'.format(getCValueSignature(cvalue_obj)))
                        return_vals.append(function(cvalue_obj, *args, **kwargs))
                else:
                    print('LLDB could not find any {} objects in the registry.'.format(registry_search_type_name))
                    print('Make sure the machine is expected to have this object type')
                    print('and that all kext symbols are loaded and slid to the correct')
                    print('offset address.')
                    return

            # only return the list if there is something to return
            for rv in return_vals:
                if rv is not None:
                    return return_vals
            return
        return selectionWrapper
    return selectRegistryObjectInnerDecorator

def SBTypeFromName(name):
    """ Given the name of a type as a string, requests the SBType from the SBTarget"""
    return LazyTarget.GetTarget().FindFirstType(name)

def IsInstanceOfBaseClass(obj, base_type):
    """Given an cvalue object, determines if the object's type,
    or any of it's supertypes, matches the given SBType"""
    if not base_type.IsValid():
        raise InvalidTypeError('Supplied base type is not valid')
    inner_obj = obj.GetSBValue()
    if inner_obj.TypeIsPointerType():
        inner_obj = inner_obj.Dereference()
    obj_type = inner_obj.type
    while True:
        if obj_type.name == base_type.name:
            return True
        bases = obj_type.bases
        if not bases:
            return False
        # assuming IOKit types are limited to single-inheritance
        obj_type = bases[0].type

def LookupKeyInOSDict(osdict, key, searchFromBack=False):
    """ Returns the value corresponding to a given key in a OSDictionary
        Returns None if the key was not found
    """
    if not osdict:
        return None
    count = unsigned(osdict.count)
    dict_array = osdict.dictionary
    idx_range = reversed(list(range(count))) if searchFromBack else list(range(count))
    for idx in idx_range:
        if key == dict_array[idx].key:
            return dict_array[idx].value
    return None

def LookupKeyInPropTable(propertyTable, key_str):
    """ Returns the value corresponding to a given key from a registry entry's property table
        Returns None if the key was not found
        The property that is being searched for is specified as a string in key_str
    """
    if not propertyTable:
        return None
    count = unsigned(propertyTable.count)
    dict_array = propertyTable.dictionary
    for idx in range(count):
        if key_str == str(dict_array[idx].key.string):
            return dict_array[idx].value
    return None

def FindRegistryObjectsBySBType(search_type):
    """Builds a list of objects in the registry whose type is,
    or descends from, the search_type."""
    # Setup
    queue = deque()
    queue.append(kern.globals.gRegistryRoot)
    found_objects = []
    scanned_objects = -1

    while queue:
        scanned_objects += 1
        if scanned_objects % 10 == 0:
            # clear to begining of line and then carriage return
            print('\x1b[1K\rScanned {} registry entries'.format(scanned_objects), end='', file=sys.stderr)
            sys.stderr.flush()
        entry = queue.popleft()

        # Compare
        if IsInstanceOfBaseClass(entry, search_type):
            found_objects.append(entry)

        # Add children
        child_key = kern.globals.gIOServicePlane.keys[ServicePlaneKeys.CHILD_LINKS]

        # For efficiency's sake, try grabbing the table without doing a slow cast
        try:
            registry_table = entry.fRegistryTable
        except AttributeError:
            # print('backup')
            registry_entry = CastIOKitClass(entry, 'IORegistryEntry *')
            registry_table = registry_entry.fRegistryTable

        # the child array tends to be the last element in the table, so we search from the back
        # to minimize the expensive key/value lookups
        child_array = LookupKeyInOSDict(registry_table, child_key, searchFromBack=True)
        if child_array:
            # again, try without casting first
            try:
                for idx in range(child_array.count):
                    queue.append(child_array.array[idx])
            except AttributeError:
                child_os_array = Cast(child_array, 'OSArray *')
                for idx in range(child_os_array.count):
                    queue.append(child_os_array.array[idx])

    print('\x1b[1K\r', end='', file=sys.stderr)
    return found_objects

registry_object_cache = {}
def FindRegistryObjectsByTypeName(type_name):
    """Builds a list of objects in the registry whose type is,
    or descends from, the type with the provided name."""
    # If this is a core file, we can safely memoize the search results
    if GetConnectionProtocol() == 'core' and type_name in registry_object_cache:
        return registry_object_cache[type_name]
    print('Searching registry for {} objects...'.format(type_name))
    search_results = FindRegistryObjectsBySBType(SBTypeFromName(type_name))
    registry_object_cache[type_name] = search_results
    return search_results

def PrintCValueObject(val, hex_format=False):
    """ Print an object with LLDB's expression command. If the CValue represents a
    pointer type, it is automatically dereferenced before printing"""
    if val.GetSBValue().TypeIsPointerType():
        ptr = ''
        address = val.GetSBValue().unsigned
    else:
        ptr = '*'
        address = val.GetSBValue().address_of.unsigned
    lldb.debugger.HandleCommand('expression {fmt} -T -- *(({type_str}{ptr}) {address:#x})'.format(
        fmt='-f hex' if hex_format  else '',
        type_str=val.GetSBValue().type.name,
        ptr=ptr,
        address=address
    ))

def getCValueAddress(val):
    "Returns address of a cvalue as a string"
    if val.GetSBValue().TypeIsPointerType():
        address = val.GetSBValue().unsigned
    else:
        address = val.GetSBValue().address_of.unsigned
    return '{address:#x}'.format(
        address=address
    )
def getCValueSignature(val):
    """Returns the type and address as a string"""
    if val.GetSBValue().TypeIsPointerType():
        address = val.GetSBValue().unsigned
    else:
        address = val.GetSBValue().address_of.unsigned
    return '({type_str}) {address:#x}'.format(
        type_str=val.GetSBValue().type.name,
        address=address
    )

def prettyPrintTable(in_table, justify='right'):
    """Prints a table (a list of row lists) where each column is the width of its
    largest member"""
    if len(set(map(len, in_table))) != 1:
        raise ValueError('in_table does not contain all equal-length lists')
    stringified_table = [list(map(str, row)) for row in in_table]
    columns = list(zip(*stringified_table))
    column_widths = [max(len(str(value)) for value in col) for col in columns]
    justify_symbol = '<' if justify.lower() == 'left' else '>'
    fmt_str = '  '.join(['{:' + justify_symbol + str(width) + '}' for width in column_widths])
    for row in stringified_table:
        print(fmt_str.format(*row))

def propTablePrintHelper(in_table, justify='right', prefix='', hasChild= False):
    """Prints a table (a list of row lists) where each column is the width of its
    largest member"""
    if len(set(map(len, in_table))) != 1:
        raise ValueError('in_table does not contain all equal-length lists')
    stringified_table = [list(map(str, row)) for row in in_table]
    columns = list(zip(*stringified_table))
    column_widths = [max(len(str(value)) for value in col) for col in columns]
    repStr = "\n" + " "*column_widths[0] + "    "
    stringified_table = [list(map(lambda e : e.replace('\n', repStr), row)) for row in stringified_table]
    justify_symbol = '<' if justify.lower() == 'left' else '>'
    fmt_str = '  '.join(['{:' + justify_symbol + str(width if width < 40 else 40) + '}' for width in column_widths])
    fmt_str = "{}" + fmt_str
    addStr = "-o "
    for row in stringified_table:
        print(fmt_str.format(prefix + addStr, *row))
        addStr = " " + " " if not hasChild else " |" + " "


def intToBoolStr(value, terms=('True', 'False')):
    """Transforms any truth value (e.g. an integer) into neat strings for True/False"""
    return terms[0] if value else terms[1]

def getMATUsTime():
    """Gets the current absolute system time in MATUs"""
    # capture stdout since ShowCurrentAbsTime only prints its result
    _stdout = sys.stdout
    sys.stdout = _stringio = StringIO()
    ShowCurrentAbsTime()
    sys.stdout = _stdout
    return int(re.search(r'(\d+) MATUs', _stringio.getvalue()).group(1))

def getNanoTime():
    """Gets the current absolute system time in nanoseconds"""
    return kern.GetNanotimeFromAbstime(getMATUsTime())

def nsToReadableString(ns_time):
    """Transforms an integer number of nanoseconds to a human-readable string"""
    str_pieces = []

    negative = ns_time < 0
    if negative:
        ns_time *= -1

    secs   = ns_time//(1000*1000*1000)
    millis = ns_time//(1000*1000)%(1000)
    micros = ns_time//(1000)%(1000)
    nanos  = ns_time%(1000)

    print_all_from_here = False
    if secs:
        str_pieces.append('{}s'.format(secs))
        print_all_from_here = True
    if millis or print_all_from_here:
        str_pieces.append('{:3}ms'.format(millis))
        print_all_from_here = True
    if micros or print_all_from_here:
        str_pieces.append('{:3}us'.format(micros))
        print_all_from_here = True
    if nanos or print_all_from_here:
        str_pieces.append('{:3}ns'.format(nanos))

    assembled_str = ' '.join(str_pieces).strip()
    if negative:
        return '-' + assembled_str
    else:
        return assembled_str

def getStringForSBValue(obj):
    """
    Returns the string form of SBValue.
    returns the char * string of the SBValue 
    else retuns the hex of unsigned int of the value
    """
    if obj.type.name == "char *":
        return "{}".format(obj.GetSummary().strip('"'))
    else:
        return "{:#x}".format(obj.unsigned)

def getSBTypeFields(obj):
    """
    Given a cvalue to SBValue object of pointer or concrete type
    the function returns a list of member fields. of the concrete type.
    """
    tmp_obj = obj.GetSBValue()
    while tmp_obj.type.IsPointerType():
        tmp_obj = tmp_obj.Dereference()

    fields = []
    for idx in range(tmp_obj.type.GetNumberOfFields()):
        fields.append(tmp_obj.type.GetFieldAtIndex(idx).GetName())
    if len(fields) == 0:
        return None
    return fields

def isRowFiltered(row, filter_dict):
    """ Given a filterdict cehcks if a row needs to be filtered off """
    filter_out = False
    for idx in range(len(row)):
        if idx in filter_dict and row[idx] not in filter_dict[idx]:
            filter_out = True
            break
    return filter_out

def IsInstanceOfBaseClassStr(obj, registry_search_type_name):
    """Given a cvalue obj and type name in string returns if the
    object is of the given type """
    registry_search_type = SBTypeFromName(registry_search_type_name)
    if not registry_search_type.IsValid():
        print('LLDB could not find any symbols for the {} type.'.format(registry_search_type_name))
        print('Make sure the machine is expected to have this object type')
        print('and that all kext symbols are loaded.')
        return False
    if not IsInstanceOfBaseClass(obj, registry_search_type):
        return False
    return True


def GetString(string):
    """ Returns the python string representation of a given OSString
    """
    # do not use CastIOKitClass instead try to directly cast it.
    try:
        out_string = "{0:s}".format(string.string.GetSBValue().GetSummary().strip('"'))
    except:
        out_string = "{0:s}".format(CastIOKitClass(string, 'OSString *').string)
    return out_string, out_string

def GetNumber(num):
    """Returns python int representation of a given OSNumber"""
    # do not use CastIOKitClass instead try to directly cast it.
    try:
        try:
            out_string = "{0:d}".format(num.value.GetSBValue().unsigned)
            out_value = num.value.GetSBValue().unsigned
        except:
            out_string = "{0:d}".format(num.GetSBValue().Dereference().GetChildAtIndex(2).GetChildMemberWithName('value').unsigned)
            out_value = num.GetSBValue().Dereference().GetChildAtIndex(2).GetChildMemberWithName('value').unsigned
    except:
        out_string = "{0:d}".format(CastIOKitClass(num, 'OSNumber *').value)
        out_value = CastIOKitClass(num, 'OSNumber *').value.unsinged
    return out_string, out_value

def GetBoolean(b):
    """ Shows info about a given OSBoolean
    """
    out_val = False
    out_string = ""
    if b == kern.globals.gOSBooleanFalse:
        out_string += "No"
    else:
        out_string += "Yes"
        out_val = True
    return out_string, out_val


def GetArray(arr):
    """ Returns a string containing info about a given OSArray
    """
    out_val = []
    out_string = ""
    idx = 0
    count = unsigned(arr.count)
    
    while idx < count:
        obj = arr.array[idx]
        idx += 1
        s, v = GetObjectSummary(obj)
        out_string += s
        out_val.append(v)
        if idx < unsigned(arr.count):
            out_string += ","
    return out_string, out_val

def GetDictionary(d, attrFilterList=None):
    """ Returns a string containing info about a given OSDictionary
    """
    out_val = OrderedDict()
    if d is None:
        return "", out_val
    out_string = "{\n"
    idx = 0
    num_items_to_get = -1 if attrFilterList is None else len(attrFilterList)
    count = unsigned(d.count)

    while idx < count:
        if num_items_to_get == 0:
            break
        key = d.dictionary[idx].key
        value = d.dictionary[idx].value
        s, v = GetString(key)
        if attrFilterList and s not in  attrFilterList:
            idx += 1
            continue
        s1, v1 = GetObjectSummary(value)
        out_val[v] = v1
        out_string += "    \"{}\" = {}\n".format(s, s1)
        idx += 1
        if num_items_to_get >= 0:
            num_items_to_get -= 1
    out_string += "}"
    return out_string, out_val

def GetSet(se):
    """ Returns a string containing info about a given OSSet
    """
    s, v = GetArray(se.members)
    out_val = v
    out_string = "[" + s + "]"
    return out_string, out_val

def GetObjectSummary(obj):
    """ Show info about an OSObject - its vtable ptr and retain count, & more info for simple container classes.
    """
    if obj is None:
        return "", ""
    v = None
    vt = dereference(Cast(obj, 'uintptr_t *')) - 2 * sizeof('uintptr_t')
    vt = kern.StripKernelPAC(vt)
    vtype = kern.SymbolicateFromAddress(vt)
    out_string = ""
    ztvAddr = kern.GetLoadAddressForSymbol('_ZTV8OSString')
    if vt == ztvAddr:
        s, out_val = GetString(obj)
        out_string += s
        return out_string, out_val
    
    ztvAddr = kern.GetLoadAddressForSymbol('_ZTV8OSSymbol')
    if vt == ztvAddr:
        s, out_val = GetString(obj)
        out_string += s
        return out_string, out_val
    
    ztvAddr = kern.GetLoadAddressForSymbol('_ZTV8OSNumber')
    if vt == ztvAddr:
        s, out_val = GetNumber(obj)
        out_string += s
        return out_string, out_val
    
    ztvAddr = kern.GetLoadAddressForSymbol('_ZTV9OSBoolean')
    if vt == ztvAddr:
        s, out_val = GetBoolean(obj)
        out_string += s
        return out_string, out_val
    
    ztvAddr = kern.GetLoadAddressForSymbol('_ZTV7OSArray')
    if vt == ztvAddr:
        try:
            s, out_val = GetArray(obj)
        except:
            s, out_val = GetArray(CastIOKitClass(obj, 'OSArray *'))
        out_string += "(" + s + ")"
        return out_string, out_val
    
    ztvAddr = kern.GetLoadAddressForSymbol('_ZTV5OSSet')
    if vt == ztvAddr:
        try:
            s, out_val = GetSet(obj)
        except:
            s, out_val = GetSet(CastIOKitClass(obj, 'OSSet *'))
        out_string += s
        return out_string, out_val
    
    ztvAddr = kern.GetLoadAddressForSymbol('_ZTV12OSDictionary')
    if vt == ztvAddr:
        try:
            s, out_val = GetDictionary(obj)
        except:
            s, out_val = GetDictionary(CastIOKitClass(obj, 'OSDictionary *'))
        
        out_string += s
        return out_string, out_val
    out_string += getCValueSignature(obj)
    return out_string, out_string

def ParseDeviceTreeEntry(obj,dump):
    if IOBlockStorageDeviceParser.isIOBlockStorageDeviceCV(obj):
        return IOBlockStorageDeviceParser.parseObject(obj, dump)
    elif IOBlockStorageDriverParser.isIOBlockStorageDriverCV(obj):
        return IOBlockStorageDriverParser.parseObject(obj, dump)
    elif IOMediaParser.isIOMediaCV(obj):
        return IOMediaParser.parseObject(obj, dump)
    elif IOPartitionSchemeParser.isIOPartitionSchemeCV(obj):
        return IOPartitionSchemeParser.parseObject(obj, dump)
    elif IOMediaBSDClientParser.isIOMediaBSDClientSchemeCV(obj):
        return IOMediaBSDClientParser.parseObject(obj, dump)
    else:
        return None

def checkIOBlockDeviceBusyAndInactive(root, timeNowNS):
    """Builds a list of objects in the registry whose type is,
    or descends from, the search_type.
    Params:
        root - cvalue
            IOService object from where to start traversing
        timeNowNS - int
            current time in NS
    Return:
         Tupe of max busy count in the hierarchy, if an object is inactive in hierarchy, 
         max busy since time in hierarchy.
    """

    # Setup
    entry = root


    busySinceNS = kern.GetNanotimeFromAbstime(entry.__timeBusy.GetSBValue().unsigned)
    state0 = entry.__state[0].GetSBValue().unsigned
    state1 = entry.__state[1].GetSBValue().unsigned
    
    isInactive = state0 & IOServiceState0.kIOServiceInactiveState
    busyCount = IOServiceState1.getBusyCount(state1)
    busyTimeNS = 0
    if busyCount > 0:
        busyTimeNS = timeNowNS - busySinceNS

    # Add children
    child_key = kern.globals.gIOServicePlane.keys[ServicePlaneKeys.CHILD_LINKS]

    # For efficiency's sake, try grabbing the table without doing a slow cast
    try:
        registry_table = entry.fRegistryTable
    except AttributeError:
        # print('backup')
        registry_entry = CastIOKitClass(entry, 'IORegistryEntry *')
        registry_table = registry_entry.fRegistryTable

    # the child array tends to be the last element in the table, so we search from the back
    # to minimize the expensive key/value lookups
    child_array = LookupKeyInOSDict(registry_table, child_key, searchFromBack=True)
    childBusyCount = False
    isChildInactive = False
    maxChildBusySinceNS = 0
    if child_array:
        numChildCount = 0
        # again, try without casting first
        try:
            numChildCount = child_array.count
            child_os_array = child_array
        except AttributeError:
            child_os_array = Cast(child_array, 'OSArray *')
            numChildCount = child_os_array.count
            
        for idx in range(numChildCount):
                retVal = checkIOBlockDeviceBusyAndInactive(child_os_array.array[idx], timeNowNS)
                childBusyCount = max(childBusyCount, retVal[0])
                isChildInactive |= retVal[1]
                maxChildBusySinceNS = max(maxChildBusySinceNS, retVal[2])
    
    return (max(busyCount, childBusyCount), isInactive | isChildInactive, max(busyTimeNS, maxChildBusySinceNS))

def dfsRecurseIOBlockStorageDevice(root, dump, timeNow,  prefix = '', bsdDevices=None, bsdDiscs=None):
    """Builds a list of objects in the registry whose type is,
    or descends from, the search_type."""
    # Setup
    entry = root
    

    busyTimeNS = kern.GetNanotimeFromAbstime(entry.__timeBusy.GetSBValue().unsigned)

    state0 = entry.__state[0].GetSBValue().unsigned
    state1 = entry.__state[1].GetSBValue().unsigned

    d = OrderedDict()
    d['Object'] = getCValueSignature(entry)
    d["state[0]"] = IOServiceState0.getSetStatesStr(state0)
    d["state[1]"] = IOServiceState1.getSetStatesStr(state1)
    isBusy = False
    if IOServiceState1.getBusyCount(state1) != 0:
        d["Busy Count"] = IOServiceState1.getBusyCount(state1)
        d["Time Busy"] = timeNow - busyTimeNS
        isBusy = True
    parseDict = ParseDeviceTreeEntry(entry, dump)

    if parseDict is not None:
        d.update(parseDict)

    if bsdDiscs is not None and IOBlockStorageDeviceParser.isIOBlockStorageDeviceCV(entry):
        if 'Physical Interconnect' in parseDict:
            bsdDiscs.physical_interconnect = parseDict['Physical Interconnect']
        if 'Physical Interconnect Location' in parseDict:
            bsdDiscs.physical_interconnect_location = parseDict['Physical Interconnect Location']

    if bsdDevices is not None and IOMediaParser.isIOMediaCV(entry):
        attrDict = OrderedDict()
        if 'BSD Name' not in parseDict:
            parseDict['BSD Name'] = 'diskNA'
        attrDict[parseDict['BSD Name']] = OrderedDict()
        isInactive = state0 & IOServiceState0.kIOServiceInactiveState
        attrDict[parseDict['BSD Name']]['inactive'] = 'Yes' if isInactive else 'No'
        attrDict[parseDict['BSD Name']]["IOName"] = parseDict['IOName'] if 'IOName' in parseDict else '-'
        if "BSD Minor" in parseDict:
            bsdDevices.append(int(parseDict["BSD Minor"]))
            attrDict[parseDict['BSD Name']]["BSD Minor"] = str(hex(int(parseDict["BSD Minor"])))
        else:
            attrDict[parseDict['BSD Name']]["BSD Minor"] = str(hex(-1))
        reqAttr = ['Content', 'Open', 'Busy Count']
        for attr in reqAttr:
            if attr in d:
                attrDict[parseDict['BSD Name']][attr] = d[attr]
            else:
                attrDict[parseDict['BSD Name']][attr] = '-'
        bsdDiscs.media_objs.update(attrDict)


    # Add children
    child_key = kern.globals.gIOServicePlane.keys[ServicePlaneKeys.CHILD_LINKS]

    # For efficiency's sake, try grabbing the table without doing a slow cast
    try:
        registry_table = entry.fRegistryTable
    except AttributeError:
        registry_entry = CastIOKitClass(entry, 'IORegistryEntry *')
        registry_table = registry_entry.fRegistryTable

    # the child array tends to be the last element in the table, so we search from the back
    # to minimize the expensive key/value lookups
    child_array = LookupKeyInOSDict(registry_table, child_key, searchFromBack=True)
    child_dicts = list()

    if child_array:
        # again, try without casting first
        numChilds = 0
        try:
            numChilds = child_array.count
            child_os_array = child_array
        except AttributeError:
            child_os_array = Cast(child_array, 'OSArray *')
            numChilds = child_os_array.count
        
        for idx in range(numChilds):
            retVal = dfsRecurseIOBlockStorageDevice(child_os_array.array[idx], dump, timeNow, prefix + " ", bsdDevices, bsdDiscs)
            child_dicts.append(retVal[0])
            isBusy |= retVal[1]

    d['childs_dict'] = child_dicts
    return (d, isBusy)

def print_dfs(node, prefix=""):
    if node is None:
        return
    childs_dict = node['childs_dict']
    chlen = len(childs_dict)
    del(node['childs_dict'])
    vertTable = list(zip(node.keys(), node.values()))
    propTablePrintHelper(vertTable, "left",  prefix, True if chlen > 0 else False)
    count = 0
    if chlen < 2:
        prefix += "  "
    else:
        prefix += " |"
    for child in childs_dict:
        if count == chlen-1:
            prefix = prefix[:-1]
            prefix += " "
        print_dfs(child, prefix)
        count += 1

def print_hierarchy(node):
    if node is None:
        return
    node = copy.deepcopy(node)
    print_dfs(node, "")

parsedIOBlockStorageDeviceCache = [OrderedDict(), OrderedDict()]
def parseIOBlockStorageDevice(obj, dump=False):
    """ Given IOBlockStorageDevice cvalue obj it prints the entire ioreg stack 
    Params:
        obj - cvalue
            The cvalue IOBlockStorageDevice to parse
        dump = Bool
            whether to dump all the property table attributes
    Return:
        A tuple of parsed stack dict, bool of is any object was busy,  
        list of minors in the stack and BSDDic object which has info
        of all the BSD discs in the stack.
    """
    key = getCValueAddress(obj)
    cacheIdx = 0 if dump else 1
    if  GetConnectionProtocol() == 'core' and key in parsedIOBlockStorageDeviceCache[cacheIdx]:
        return parsedIOBlockStorageDeviceCache[cacheIdx][key]
    registry_search_type_name = "IOBlockStorageDevice"
    registry_search_type = SBTypeFromName(registry_search_type_name)
    if not registry_search_type.IsValid():
        print('LLDB could not find any symbols for the {} type.'.format(registry_search_type_name))
        print('Make sure the machine is expected to have this object type')
        print('and that all kext symbols are loaded.')
        return ({}, False, [], BSDDiscs())
    if not IsInstanceOfBaseClass(obj, registry_search_type):
        return None
    out_dict = OrderedDict()
    level = 0
    bsdDevices = list()
    timeNowNs = getNanoTime()
    bsdDiscs = BSDDiscs()
    new_dict, isBusy = dfsRecurseIOBlockStorageDevice(obj, dump, timeNowNs, "", bsdDevices, bsdDiscs)
    parsedIOBlockStorageDeviceCache[cacheIdx][key] = (new_dict, isBusy, bsdDevices, bsdDiscs)
    return parsedIOBlockStorageDeviceCache[cacheIdx][key]

def time_command_run(func):
    """ Decorator for timing function Runs. """
    @wraps(func)
    def wrapper(*args, **kwargs):
        tic = time.perf_counter()
        func(*args, **kwargs)
        toc = time.perf_counter()
        print("Command took {:0.4f} seconds. ".format(toc - tic))
    return wrapper



######################################
#  Mounts static class 
######################################

class Mounts(object):
    """ This class helps parse mount list and caches the results 
    when running with corefile for fater access
    Class Attributes:
    mp_cache_by_bsdname - OrderedDict
        dict for caching bsdname to parsed mount point dict
    mp_cache_by_minor - OrderedDict
        dict for caching minor ID to parsed mount point dict
    mp_cache_by_mp - OrderedDict
        dict for caching mount point address to parsed mount point dict
    vblk_type - SBType
        the Block Device Type enum
    vchr_type = SBType
        the Char Device Type enum
    """
    mp_cache_by_bsdname = OrderedDict()
    mp_cache_by_minor = OrderedDict()
    mp_cache_by_mp = OrderedDict()
    vblk_type = GetEnumValue('vtype::VBLK')
    vchr_type = GetEnumValue('vtype::VCHR')

    @classmethod
    def cacheMP(cls, mp_dict):
        """Given a mount point dictionary it caches the dict against
        Bsd Name, Minor and address of mount point
        Param:
            mp_dict - OrderedDict
                the parsed mount point dict to be cached
        """
        cls.mp_cache_by_bsdname[mp_dict['bsdName']] = mp_dict
        cls.mp_cache_by_minor[int(mp_dict['minor'], base=16)] = mp_dict
        cls.mp_cache_by_mp[int(mp_dict['mp'], base=16)] = mp_dict

    @classmethod
    def parse_mp(cls, mp):
        """Parses a given mount point and returns a dict
        containing the parsed key, vals.
        Params:
            mp - cvalue
                the mount point cvalue to be parsed
        Return:
            An OrderedDict of parsed key, vals of the mount point.
        """
        out_dict = OrderedDict()
        if mp.mnt_devvp.GetSBValue().unsigned != 0:
            out_dict['mp'] = "{:#x}".format(mp.GetSBValue().unsigned)
            out_dict['mnt_devvp'] = "{:#x}".format(mp.mnt_devvp.GetSBValue().unsigned)
            vnode = mp.mnt_devvp
            if ((vnode.v_type == cls.vblk_type) or (vnode.v_type == cls.vchr_type)) and (vnode.v_data.GetSBValue().unsigned != 0):
                devnode = Cast(vnode.v_data, 'devnode_t *')
                devnode_dev = devnode.dn_typeinfo.dev
                devnode_major = (devnode_dev >> 24) & 0xff
                devnode_minor = devnode_dev & 0x00ffffff
                bsdNameRaw = mp.mnt_devvp.v_name.GetSBValue().GetSummary()
                if bsdNameRaw is not None:
                    bsdName = mp.mnt_devvp.v_name.GetSBValue().GetSummary().strip('"')
                else:
                    s = ''.join([choice(ascii_lowercase) for _ in range(5)])
                    bsdName = "NULL_" + s

                out_dict['major'] = str(hex(devnode_major))
                out_dict['minor'] = str(hex(devnode_minor))
                out_dict['bsdName'] = bsdName
                out_dict['f_fstypename'] = str(mp.mnt_vfsstat.f_fstypename)
                out_dict['f_mntonname'] = str(mp.mnt_vfsstat.f_mntonname)
                out_dict['f_mntfromname'] = str(mp.mnt_vfsstat.f_mntfromname)
            else:
                return None
            return out_dict
        return None
        

    @classmethod
    def getMounts(cls, minorID=None, bsdName=None):
        """ Gets all the mounts from mountlist. optionally filters for a 
        given minorID or BSD Name
        Params:
            minorID - int 
                minor ID whose corresponding parsed Mount Point dict is desired
            bsdName - str
                bsdName for which mount point parsed Mount Point dict is desired
        Return:
            Parsed mount point dict corrresponding to given minorId or bsdName if
            neither are given list of all parsed mount points si returned
        """
        mounts = []
        mntlist = kern.globals.mountlist
        for mnt in IterateTAILQ_HEAD(mntlist, 'mnt_list'):
            if GetConnectionProtocol() == 'core' and mnt.GetSBValue().unsigned in cls.mp_cache_by_mp:
                mounts.append(cls.mp_cache_by_mp[mnt.GetSBValue().unsigned])
                continue
            d = cls.parse_mp(mnt)
            if d is not None:
                cls.cacheMP(d)
                mounts.append(d)
        if minorID is not None:
            for mount in mounts:
                if mount['minor'] == minorID:
                    return mount
            return None
        elif bsdName is not None:
            for mount in mounts:
                if mount['bsdName'] == bsdName:
                    return mount
            return None
        return mounts

    
    @classmethod
    def getMountsforMinorID(cls, minorId):
        """ returns the mount point parsed dict for a given minor ID
        Param:
            minorId - int
                the minor ID for which parsed mp dict is to be returned
        Return:
            mp dict if the mount exist else None
        """
        if GetConnectionProtocol() == 'core' and minorId in cls.mp_cache_by_minor:
            return cls.mp_cache_by_minor[minorId]
        return cls.getMounts(minorID=minorId)


    @classmethod
    def getMountsforBsdName(cls, bsd_name):
        """ returns the mount point parsed dict for a given BSD name
        Param:
            bsd_name - str
                the bsd name for which parsed mp dict is to be returned
        Return:
            mp dict if the mount exist else None
        """
        if GetConnectionProtocol() == 'core' and bsd_name in cls.mp_cache_by_bsdname:
            return cls.mp_cache_by_minor[bsd_name]
        return cls.getMounts(bsdName=bsd_name)

    @classmethod
    def printMountsForMinorIDs(cls, minorIds=None):
        """ Prints the mount dict for given minorIDs
        Params:
            minorIds - list
                list of all minor Id for which mp dict is to be printed
        Return
            None
        """
        title = ["mp", "mnt_devvp", "major", "minor", "bsdName", "f_fstypename", "f_mntonname", "f_mntfromname"]
        rows = []
        table = []
        for minorId in minorIds:
            mp = cls.getMountsforMinorID(minorId)
            if mp is not None:
                rows.append(list(mp.values()))
        table = [title, *rows]
        prettyPrintTable(table)

    @classmethod
    def printAllMounts(cls):
        """ prints all the mount dicts
        Params:
            None
        Return
            None
        """
        title = ["mp", "mnt_devvp", "major", "minor", "bsdName", "f_fstypename", "f_mntonname", "f_mntfromname"]
        rows = []
        table = []
        for mp in cls.getMounts():
            rows.append(list(mp.values()))
        table = [title, *rows]
        prettyPrintTable(table)

    @classmethod
    def exportAll(cls):
        cls.getMounts()
        return {'bsdname':cls.mp_cache_by_bsdname, 'minor':cls.mp_cache_by_minor, 'mountPoint':cls.mp_cache_by_mp}

######################################
#  IOMediaBSDClientGlobals static class
######################################

class IOMediaBSDClientGlobals(object):
    """ Static class that manages the IOMediaBSDClientGlobals 
    Class Attributes:
        kMinorsAddCountBits - int
            minor table slot index scheme
        kMinorsAddCountMask - int
            minor table slot index scheme
        minorSlotCache - OrderedDict
            dict which stores minor slot index (comma separated) to MinorSlot cvalue
        anchorSlotCache - OrderedDict
            dict which stores minor slot index (comma separated) to AnchorSlot cvalue
    """
    gIOMediaBSDClientGlobals = None
    kMinorsAddCountBits = 6
    kMinorsAddCountMask = (1 << kMinorsAddCountBits) - 1
    minorSlotCache = OrderedDict()
    anchorSlotCache = OrderedDict()

    @classmethod
    def getMinorSlot(cls, minor) :
        """ Gets the minorslot for the given minor ID
        Param:
            minor - int
                the minor number for whic to get the MinorSlot
        Return:
            MinorSlot if valid minor number is given else None
        """
        if minor >= kern.globals.gIOMediaBSDClientGlobals._minors._tableCount:
            return None
        key = "{},{}".format(minor >> cls.kMinorsAddCountBits, minor & cls.kMinorsAddCountMask)
        if GetConnectionProtocol() == 'core' and key in cls.minorSlotCache:
            return cls.minorSlotCache[key]
        cls.minorSlotCache[key] = kern.globals.gIOMediaBSDClientGlobals._minors._table.buckets[minor >> cls.kMinorsAddCountBits][minor & cls.kMinorsAddCountMask]
        return cls.minorSlotCache[key]


    @classmethod
    def isMinorExist(cls, minor):
        """ checks if the minor slot exist for minor ID
        Param:
            minor - int
                the minor number for whic to get the MinorSlot
        Return:
            True if valid minor number and it is valid else False
        """
        if minor < kern.globals.gIOMediaBSDClientGlobals._minors._tableCount:
            minor = IOMediaBSDClientGlobals.getMinorSlot(minor)
            if bool(minor.isAssigned.GetSBValue().unsigned):
                return True
        return False

    @classmethod
    def getMinorSlotTable(cls, filter_args=None):
        """ Returs MinorSlots. optionally filters rows based on filter_args dict
        Params:
            filter_args - list
                list of minorSlot attr on which to filter minorslots
                it is taken as [sttr=val,val1, attr1=val1]
        Return:
            An OrderedDict with single 'values' having all the MinorSlots
            which match the filter criteria
        """
        out_dict = OrderedDict()
        rows = []
        filter_dict = OrderedDict()
        fieldList = getSBTypeFields(kern.globals.gIOMediaBSDClientGlobals._minors._table.buckets)
        fieldList = ["minorID", 'IOName', *fieldList]
        out_dict['title'] = fieldList
        if filter_args:
            filter_dict = IOMediaBSDClientGlobals.getFilterDictWithCValue(fieldList, filter_args)
        for idx in range(kern.globals.gIOMediaBSDClientGlobals._minors._tableCount):
            minor = IOMediaBSDClientGlobals.getMinorSlot(idx)
            if minor.isAssigned.GetSBValue().unsigned != 1:
                continue
            dataRow = []
            numchilds = minor.GetSBValue().GetNumChildren()
            dataRow.append(str(hex(idx)))
            s,v = GetDictionary( minor.media.fRegistryTable, 'IOName')
            dataRow.append('{}'.format(v['IOName'] if 'IOName' in v else '-'))
            for idx in range(numchilds):
                dataRow.append("{}".format(getStringForSBValue(minor.GetSBValue().GetChildAtIndex(idx))))
            
            if isRowFiltered(dataRow, filter_dict):
                continue
            rows.append(dataRow)
        out_dict['values'] = rows
        return out_dict

    @classmethod
    def printMinorSlotTable(cls, filter_args=None):
        """ Prints the MinorSlotTable. optionally filters rows from filter_args
        Params:
            filter_args - list
                list of minorSlot attr on which to filter minorslots
                it is taken as [sttr=val,val1, attr1=val1]
        Return:
            None
        """
        d = IOMediaBSDClientGlobals.getMinorSlotTable(filter_args)
        table = [d['title'], *d['values']]
        prettyPrintTable(table)

    @classmethod
    def getAnchorSlot(cls, anchorID):
        """ Gets all the anchorSlots. optionally filters slots based on filterDict
        Param:
            anchorID - int
                the anchor number for which to get the AnchorSlot
        Return:
            AnchorSlot if valid minor number is given else None

        """
        if anchorID >= kern.globals.gIOMediaBSDClientGlobals._anchors._tableCount:
            return None
        if GetConnectionProtocol() == 'core' and anchorID in cls.anchorSlotCache:
            return cls.anchorSlotCache[anchorID]
        cls.anchorSlotCache[anchorID] = kern.globals.gIOMediaBSDClientGlobals._anchors._table[anchorID]
        return cls.anchorSlotCache[anchorID]

    @classmethod
    def getFilterDictWithCValue(cls, fieldList, filter_args):
        """ Given a field list and field=value[,value] type filter args constructs filter dict
        Params:
            fieldList - list
                list of ordered list of field members
            filter_args - list
                list of minorSlot attr on which to filter minorslots
                it is taken as [attr=val,val1, attr1=val1]
        Return:
            Returns filterDict where keys are index values of attr values in filter_args
            with list of filter values from filter_args
        """
        filter_dict = OrderedDict()
        filter_dict = {fieldList.index(arg.split("=")[0]):arg.split("=")[1].split(",") for arg in filter_args if arg.split("=")[0] in fieldList}
        return filter_dict

    @classmethod
    def getAnchorSlotTable(cls, filter_args = None):
        """ Gets AnchorTable and optionally filters rows based on filter_args
        Params:
            filter_args - list
                list of minorSlot attr on which to filter minorslots
                it is taken as [sttr=val,val1, attr1=val1]
        Return:
            An OrderedDict with single 'values' having all the AnchorSlots
            which match the filter criteria
        """
        out_dict = OrderedDict()
        rows = []
        filter_dict = OrderedDict()
        fieldList = getSBTypeFields(kern.globals.gIOMediaBSDClientGlobals._anchors._table)
        fieldList = ["anchorID", *fieldList]
        out_dict['title'] = fieldList
        if filter_args:
            filter_dict = IOMediaBSDClientGlobals.getFilterDictWithCValue(fieldList, filter_args)
        for idx in range(kern.globals.gIOMediaBSDClientGlobals._anchors._tableCount):
            minor = IOMediaBSDClientGlobals.getAnchorSlot(idx)
            if minor.isAssigned.GetSBValue().unsigned != 1:
                continue
            dataRow = []
            numchilds = minor.GetSBValue().GetNumChildren()
            dataRow.append(str(hex(idx)))
            for idx in range(numchilds):
                dataRow.append("{}".format(getStringForSBValue(minor.GetSBValue().GetChildAtIndex(idx))))
            if isRowFiltered(dataRow, filter_dict):
                continue
            rows.append(dataRow)
        out_dict['values'] = rows
        return out_dict

    @classmethod
    def printAnchorSlotTable(cls, filter_args = None):
        """ Prints AnchorTable and optionally filters rows based on filter_args
        Params:
            filter_args - list
                list of minorSlot attr on which to filter minorslots
                it is taken as [sttr=val,val1, attr1=val1]
        Return:
            None
        """
        d = IOMediaBSDClientGlobals.getAnchorSlotTable(filter_args)
        table = [d["title"], *d["values"]]
        prettyPrintTable(table)

######################################
#  IOService Base Parser
######################################
class IOServiceParser(object):
    """ Serves as Base class parser for all IOService derived objects.
    has common parsing routines for IOService based objects."""
    @classmethod
    def parseDict(cls, osdict, attrFilterList=None):
        """ Parses property dict and returns it on python dict. optionally parses
        attrFilterList attributs only from property dict to save parsing time.
        Params:
            osdict - OrderedDict
                the propertytable cvalue of IOService objt to be parsed
            attrFilterList - list
                it is the list of keys in osdict that we want to parse and return
        Return:
            Tuple of string rep of the key value pair and also a python DS representation
            of the property table
        """
        if not osdict:
            return "", OrderedDict()
        if attrFilterList is not None and len(attrFilterList) == 0:
            return "", OrderedDict()
        s, v = GetDictionary(osdict, attrFilterList)
        return s, v
    
    @classmethod
    def copyDictAttr(cls, dstDict, srcDict, attrList = None):
        """ Copies attrList attributes from srcDict to dstDict if attrList is not None else 
        copies the pprint.pformat value from srcDict for each key. val pair in srcDict
        Params:
            dstDict - OrderedDict
                the destination dict to copy the attributes into
            srcDict - OrderedDict
                the source dict from which to copy attibutes from
            attrList - list
                the list of attr from src to dst dict to be copied
        Return:
            None
        """
        if attrList is not None:
            for attr in attrList if attrList != None else srcDict.keys():
                if attr in srcDict:
                    dstDict[attr] = srcDict[attr]
        else:
            for k, v in srcDict.items():
                dstDict[k] = pformat(v)

######################################
#  IOSF Parser classes
######################################
class IOBlockStorageDeviceParser(IOServiceParser):
    """ Parses IOBlockStorageDeviceParser """
    @classmethod
    def parseObject(cls, objCV, dump = False):
        #print(objCV.GetSBValue().Dereference())
        attrList = ['Medium Type', 'Product Name', 'Serial Number', 'Vendor Name', 'Protocol Characteristics']
        s, v = super(IOBlockStorageDeviceParser, cls).parseDict(objCV.fPropertyTable, attrList if not dump else None)
        attrList.pop()
        propDict = OrderedDict()
        if not dump:
            IOBlockStorageDriverParser.copyDictAttr(propDict, v, attrList if not dump else None)
            if 'Protocol Characteristics' in v:
                IOBlockStorageDriverParser.copyDictAttr(propDict, v['Protocol Characteristics'], ['Physical Interconnect', 'Physical Interconnect Location'])
            else:  
                propDict['Physical Interconnect'] = 'N/A'
                propDict['Physical Interconnect Location'] = 'N/A'
        else:
            propDict["PropertyTable"] = s
        return propDict

    @classmethod
    def isIOBlockStorageDeviceCV(cls, obj):
        return IsInstanceOfBaseClassStr(obj, "IOBlockStorageDevice")

class IOBlockStorageDriverParser(IOServiceParser):
    """ Parses IOBlockStorageDriverParser """
    @classmethod
    def parseObject(cls, objCV, dump=False):
        #print(objCV.GetSBValue().Dereference())
        attrList = ['Statistics']
        s, v = super(IOBlockStorageDriverParser, cls).parseDict(objCV.fPropertyTable, attrList if not dump else None)
        propDict = OrderedDict()
        IOBlockStorageDriverParser.copyDictAttr(propDict, v['Statistics'] if not dump else v)
        return propDict
    @classmethod
    def isIOBlockStorageDriverCV(cls, obj):
        return IsInstanceOfBaseClassStr(obj, "IOBlockStorageDriver")

class IOMediaParser(IOServiceParser):
    """ Parses IOMediaParser """
    @classmethod
    def parseObject(cls, objCV, dump=False):
        #print(objCV.GetSBValue().Dereference())
        attrList = ['BSD Major', 'BSD Minor', 'BSD Name', 'Open', 'Content', 'Content Hint', 'Whole']
        s, v = super(IOMediaParser, cls).parseDict(objCV.fPropertyTable, attrList if not dump else None)
        propDict = OrderedDict()

        s1,v1 = GetDictionary( objCV.fRegistryTable, 'IOName')
        propDict.update(v1)

        IOBlockStorageDriverParser.copyDictAttr(propDict, v, attrList if not dump else None)
        
        return propDict
    @classmethod
    def isIOMediaCV(cls, obj):
        return IsInstanceOfBaseClassStr(obj, "IOMedia")

class IOPartitionSchemeParser(IOServiceParser):
    """ Parses IOPartitionSchemeParser """
    @classmethod
    def parseObject(cls, objCV, dump=False):
        #print(objCV.GetSBValue().Dereference())
        attrList = ['Content Mask']
        s, v = super(IOPartitionSchemeParser, cls).parseDict(objCV.fPropertyTable, attrList if not dump else None)
        propDict = OrderedDict()
        IOPartitionSchemeParser.copyDictAttr(propDict, v, attrList if not dump else None)
        return propDict
    @classmethod
    def isIOPartitionSchemeCV(cls, obj):
        return IsInstanceOfBaseClassStr(obj, "IOPartitionScheme")
    
class IOMediaBSDClientParser(IOServiceParser):
    """ Parses IOMediaBSDClientParser """
    @classmethod
    def parseObject(cls, objCV, dump=False):
        # attrList = ['IOPersonalityPublisher']
        attrList = []
        s, v = super(IOMediaBSDClientParser, cls).parseDict(objCV.fPropertyTable, attrList if not dump else None)
        propDict = OrderedDict()
        IOMediaBSDClientParser.copyDictAttr(propDict, v, attrList if not dump else None)
        return propDict
    @classmethod
    def isIOMediaBSDClientSchemeCV(cls, obj):
        return IsInstanceOfBaseClassStr(obj, "IOMediaBSDClient")

######################################
#  IOService __state[0] & __state[1] parsers
######################################

class IOServiceState0(object):
    """ Parses the IOService __state[0] of the object """
    kIOServiceInactiveState = 0x00000001
    kIOServiceRegisteredState   = 0x00000002
    kIOServiceMatchedState  = 0x00000004
    kIOServiceFirstPublishState = 0x00000008
    kIOServiceFirstMatchState   = 0x00000010
    enum_vals = { 
        0x00000001:"kIOServiceInactiveState",
        0x00000002:"kIOServiceRegisteredState",
        0x00000004:"kIOServiceMatchedState" ,
        0x00000008:"kIOServiceFirstPublishState",
        0x00000010:"kIOServiceFirstMatchState"
    }

    @classmethod
    def getNameStr(cls, val):
        """ Gets the string name for __state[0] enum """
        if val in cls.enum_vals:
            return cls.enum_vals[val]
        return "UNKNOWN"
    @classmethod
    def getSetStatesStr(cls, val):
        """ Given val gets all the different states set in it in 
        string representation. """
        out_str = ""
        for enum_val in cls.enum_vals:
            if (val & enum_val) != 0:
                out_str += cls.enum_vals[enum_val] + " "
        return out_str

class IOServiceState1(object):
    """ Parses the IOService __state[1] of the object """
    kIOServiceBusyMax           = 1023
    kIOServiceBusyStateMask     = 0x000003ff
    enum_vals = { 
        0x80000000 : "kIOServiceNeedConfigState",
        0x40000000 : "kIOServiceSynchronousState",
        0x20000000 : "kIOServiceModuleStallState",
        0x10000000 : "kIOServiceBusyWaiterState",

        0x08000000 : "kIOServiceSyncPubState",
        0x04000000 : "kIOServiceConfigState",
        0x02000000 : "kIOServiceStartState",
        0x01000000 : "kIOServiceTermPhase2State",
        0x00800000 : "kIOServiceTermPhase3State",
        0x00400000 : "kIOServiceTermPhase1State",
        0x00200000 : "kIOServiceTerm1WaiterState",
        0x00100000 : "kIOServiceRecursing",
        0x00080000 : "kIOServiceNeedWillTerminate",
        0x00040000 : "kIOServiceWaitDetachState",
        0x00020000 : "kIOServiceConfigRunning",
        0x00010000 : "kIOServiceFinalized"
    }

    @classmethod
    def getNameStr(cls, val):
        """ Gets the string name for __state[1] enum """
        val = val & ~cls.kIOServiceBusyStateMask
        if val in cls.enum_vals:
            return cls.enum_vals[val]
        return "UNKNOWN"
    @classmethod
    def getSetStatesStr(cls, val):
        """ Given val gets all the different states set in it in 
        string representation. """
        out_str = ""
        for enum_val in cls.enum_vals:
            if (val & enum_val) != 0:
                out_str += cls.enum_vals[enum_val] + " "
        return out_str
    @classmethod
    def getBusyCount(cls, val):
        """ Gets the busy count of the object given its __state[1] val"""
        val = val & cls.kIOServiceBusyStateMask
        return val



######################################
#  General command functions
######################################

@iosf_command('iosfhelp')
def PrintIOSFHelp(cmd_args=None):
    """Lists all available IOSF commands registered with LLDB"""
    print('List of commands provided by {} for IOSF debugging:'.format(__name__))
    max_cmd_len = max([len(key) for key in list(registered_iosf_commands.keys())])
    for command, doc in list(registered_iosf_commands.items()):
        print('{:{width}} - {}'.format(command, doc.splitlines()[0], width=max_cmd_len))


@iosf_command('iosfdotriage')
@time_command_run
def IOSFDoTriage(cmd_args):
    """Prints appropriate information for IOSF triage."""
    objs = FindRegistryObjectsByTypeName('IOBlockStorageDevice')
    timeNowNs = getNanoTime()
    isProblem = False
    for obj in objs:
        maxBusyCount, isInactive, maxBusySince = checkIOBlockDeviceBusyAndInactive(obj, timeNowNs)
        if maxBusyCount > 0 or isInactive:
            isProblem = True
            IOSFShowBlockDevices(obj)
            print(" ")
    if not isProblem:
        IOSFShowDisks()
        print(" ")
        IOMediaBSDClientGlobals.printMinorSlotTable()
        print(" ")
        Mounts.printAllMounts()
    else:
        IOSFExportAll()
    return

@iosf_command('iosfshowdisks')
@time_command_run
def IOSFShowDisks(cmd_args = None):
    """Shows all BSD disks in Registry"""
    title = ['BSD Name', 'Media Name', 'Minor ID', 'Content', 'Is Open', 'Busy Count', 'Inactive', 'Physical Interconnect', 'Physical Interconnect Location']
    rows = [title]
    try:
        registry_objects = FindRegistryObjectsByTypeName('IOBlockStorageDevice')
    except InvalidTypeError:
        registry_objects = None
    count = 0
    media_objs_dict = dict()
    if registry_objects:
        for idx, cvalue_obj in enumerate(registry_objects):
            out, isBusy, bsdDevices, bsdDicsc = parseIOBlockStorageDevice(cvalue_obj)
            keys = list(bsdDicsc.media_objs.keys())
            keys.sort()
            for key in keys:
                row = []
                row.append(key)
                row.append(bsdDicsc.media_objs[key]['IOName'])
                row.append(bsdDicsc.media_objs[key]['BSD Minor'])
                row.append(bsdDicsc.media_objs[key]['Content'])
                row.append(bsdDicsc.media_objs[key]['Open'])
                row.append(bsdDicsc.media_objs[key]['Busy Count'])
                row.append(bsdDicsc.media_objs[key]['inactive'])
                row.append(bsdDicsc.physical_interconnect)
                row.append(bsdDicsc.physical_interconnect_location)
                rows.append(row)
    prettyPrintTable(rows)
    return


@iosf_command('iosfshowminortable')
@time_command_run
def ShowMinorTable(cmd_args):
    "shows the Minor Table"
    IOMediaBSDClientGlobals.printMinorSlotTable(cmd_args)

@iosf_command('iosfshowanchortable')
def ShowAnchorTable(cmd_args):
    "shows the Anchor Table"
    IOMediaBSDClientGlobals.printAnchorSlotTable(cmd_args)

    
@iosf_command('iosfshowbusyblockdevices')
@time_command_run
@selectRegistryObject('IOBlockStorageDevice')
@time_command_run
def IOSFShowBusyBlockDevices(controller):
    """Prints ioreg stack for IOBlockStorageDevice that are busy """
    timeNowNs = getNanoTime()
    maxBusyCount, isInactive, maxBusySince = checkIOBlockDeviceBusyAndInactive(controller, timeNowNs)
    if maxBusyCount > 0 or isInactive:
        IOSFShowBlockDevices(controller)
        print(" ")
    return


@iosf_command('iosfshowblockdevices')
@time_command_run
@selectRegistryObject('IOBlockStorageDevice')
@time_command_run
def IOSFShowBlockDevices(controller):
    """Prints ioreg stack for IOBlockStorageDevice(s) """
    d, isBusy, bsdDevices, bsdDics = parseIOBlockStorageDevice(controller)
    print_hierarchy(d)
    print(" ")
    if len(bsdDevices) > 0:
        cmd_args = ["minorID="+",".join(map(lambda a : "{:#x}".format(a),bsdDevices))]
        IOMediaBSDClientGlobals.printMinorSlotTable(cmd_args)
        print(" ")
        Mounts.printMountsForMinorIDs(bsdDevices)
    return

@iosf_command('iosfdumpbusyblockdevices')
@time_command_run
@selectRegistryObject('IOBlockStorageDevice')
@time_command_run
def IOSFDumpBusyBlockDevices(controller):
    """Prints all properties of ioreg stack for IOBlockStorageDevice that are busy """
    timeNowNs = getNanoTime()
    maxBusyCount, isInactive, maxBusySince = checkIOBlockDeviceBusyAndInactive(controller, timeNowNs)
    if maxBusyCount > 0 or isInactive:
        IOSFDumpBlockDevices(controller)
    return


@iosf_command('iosfdumpblockdevices')
@time_command_run
@selectRegistryObject('IOBlockStorageDevice')
@time_command_run
def IOSFDumpBlockDevices(controller):
    """Prints All Properties of ioreg stack for IOBlockStorageDevice(s) """
    d, isBusy, bsdDevices, bsdDiscs = parseIOBlockStorageDevice(controller, True)
    print_hierarchy(d)
    print(" ")
    if len(bsdDevices) > 0:
        cmd_args = ["minorID="+",".join(map(lambda a : "{:#x}".format(a),bsdDevices))]
        IOMediaBSDClientGlobals.printMinorSlotTable(cmd_args)
        print(" ")
        Mounts.printMountsForMinorIDs(bsdDevices)
    return



@iosf_command('iosfexportall')
@time_command_run
def IOSFExportAll(cmd_args = None):
    """Dumps the entire state as json for itriage script"""
    out_dict = OrderedDict()
    out_dict['ioregStacks'] = list()
    out_dict['ioregBusyStacks'] = list()
    try:
        registry_objects = FindRegistryObjectsByTypeName('IOBlockStorageDevice')
    except InvalidTypeError:
        registry_objects = None
    if registry_objects:
        for idx, cvalue_obj in enumerate(registry_objects):
            out, isBusy, bsdDevices, bsdDiscs = parseIOBlockStorageDevice(cvalue_obj)
            if isBusy:
                out_dict['ioregBusyStacks'].append(out)
            else:
                out_dict['ioregStacks'].append(out)
    anchorTable = IOMediaBSDClientGlobals.getAnchorSlotTable()
    minorTable = IOMediaBSDClientGlobals.getMinorSlotTable()
    mounts = Mounts.exportAll()
    out_dict['anchorSlot'] = anchorTable
    out_dict['minorSlot'] = minorTable
    out_dict['mounts'] = mounts
    print(json.dumps(out_dict))




            
