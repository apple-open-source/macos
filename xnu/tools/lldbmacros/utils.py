#General Utility functions for debugging or introspection

""" Please make sure you read the README file COMPLETELY BEFORE reading anything below.
    It is very critical that you read coding guidelines in Section E in README file. 
"""
import sys, re, time, os, time
import lldb
import struct

from core.cvalue import *
from core.configuration import *
from core.lazytarget import *

#DONOTTOUCHME: exclusive use for lldb_run_command only. 
lldb_run_command_state = {'active':False}

def lldb_run_command(cmdstring):
    """ Run a lldb command and get the string output.
        params: cmdstring - str : lldb command string which could be executed at (lldb) prompt. (eg. "register read")
        returns: str - output of command. it may be "" in case if command did not return any output.
    """
    global lldb_run_command_state
    retval =""
    res = lldb.SBCommandReturnObject()
    # set special attribute to notify xnu framework to not print on stdout
    lldb_run_command_state['active'] = True
    lldb.debugger.GetCommandInterpreter().HandleCommand(cmdstring, res)
    lldb_run_command_state['active'] = False
    if res.Succeeded():
        retval = res.GetOutput()
    else:
        retval = "ERROR:" + res.GetError()
    return retval

def EnableLLDBAPILogging():
    """ Enable file based logging for lldb and also provide essential information about what information
        to include when filing a bug with lldb or xnu.
    """
    logfile_name = "/tmp/lldb.%d.log" % int(time.time())
    enable_log_base_cmd = "log enable --file %s " % logfile_name
    cmd_str = enable_log_base_cmd + ' lldb api'
    print(cmd_str)
    print(lldb_run_command(cmd_str))
    cmd_str = enable_log_base_cmd + ' gdb-remote packets'
    print(cmd_str)
    print(lldb_run_command(cmd_str))
    cmd_str = enable_log_base_cmd + ' kdp-remote packets'
    print(cmd_str)
    print(lldb_run_command(cmd_str))
    print(f"{lldb.SBDebugger.GetVersionString()}\n")
    print("Please collect the logs from %s for filing a radar. If you had encountered an exception in a lldbmacro command please re-run it." % logfile_name)
    print("Please make sure to provide the output of 'version', 'image list' and output of command that failed.")
    return

def GetConnectionProtocol():
    """ Returns a string representing what kind of connection is used for debugging the target.
        params: None
        returns:
            str - connection type. One of ("core","kdp","gdb", "unknown")
    """
    retval = "unknown"
    process_plugin_name = LazyTarget.GetProcess().GetPluginName().lower()
    if "kdp" in process_plugin_name:
        retval = "kdp"
    elif "gdb" in process_plugin_name:
        retval = "gdb"
    elif "mach-o" in process_plugin_name and "core" in process_plugin_name:
        retval = "core"
    return retval

def SBValueToPointer(sbval):
    """ Helper function for getting pointer value from an object of pointer type. 
        ex. void *astring = 0x12345
        use SBValueToPointer(astring_val) to get 0x12345
        params: sbval - value object of type '<type> *'
        returns: int - pointer value as an int. 
    """
    if type(sbval) == core.value:
        sbval = sbval.GetSBValue()
    if sbval.IsPointerType():
        return sbval.GetValueAsUnsigned()
    else:
        return int(sbval.GetAddress())

def ArgumentStringToInt(arg_string) -> int:
    """ converts an argument to an int
        params:
            arg_string: str - typically a string passed from the commandline.
                        Accepted inputs:
                        1. A base 2/8/10/16 literal representation, e.g. "0b101"/"0o5"/"5"/"0x5"
                        2. An LLDB expression, e.g. "((char*)foo_ptr + sizeof(bar_type))"
        returns:
            int - integer representation of the string
    """
    try:
        return int(arg_string, 0)
    except ValueError:
        val = LazyTarget.GetTarget().chkEvaluateExpression(arg_string)
        return val.signed

def GetLongestMatchOption(searchstr, options=[], ignore_case=True):
    """ Get longest matched string from set of options. 
        params:
            searchstr : string of chars to be matched
            options : array of strings that are to be matched
        returns:
            [] - array of matched options. The order of options is same as the arguments.
                 empty array is returned if searchstr does not match any option.
        example:
            subcommand = LongestMatch('Rel', ['decode', 'enable', 'reload'], ignore_case=True)
            print subcommand # prints ['reload']
    """
    if ignore_case:
        searchstr = searchstr.lower()
    found_options = []
    for o in options:
        so = o
        if ignore_case:
            so = o.lower()
        if so == searchstr:
            return [o]
        if so.find(searchstr) >=0 :
            found_options.append(o)
    return found_options

def GetType(target_type):
    """ type cast an object to new type.
        params:
            target_type - str, ex. 'char', 'uint32_t' etc
        returns:
            lldb.SBType - a new Type that can be used as param to  lldb.SBValue.Cast()
        raises:
            NameError  - Incase the type is not identified
    """
    return gettype(target_type)

    
def Cast(obj, target_type):
    """ Type cast an object to another C type.
        params:
            obj - core.value  object representing some C construct in lldb
            target_type - str : ex 'char *'
                        - lldb.SBType :
    """
    return cast(obj, target_type)

def ContainerOf(obj, target_type, field_name):
    """ Type cast an object to another C type from a pointer to a field.
        params:
            obj - core.value  object representing some C construct in lldb
            target_type - str : ex 'struct thread'
                        - lldb.SBType :
            field_name - the field name within the target_type obj is a pointer to
    """
    return containerof(obj, target_type, field_name)

def loadLLDB():
    """ Util function to load lldb python framework in case not available in common include paths.
    """
    try:
        import lldb
        print('Found LLDB on path')
    except:
        platdir = subprocess.check_output('xcodebuild -version -sdk iphoneos PlatformPath'.split())
        offset = platdir.find("Contents/Developer")
        if offset == -1:
            lldb_py = os.path.join(os.path.dirname(os.path.dirname(platdir)), 'Library/PrivateFrameworks/LLDB.framework/Versions/A/Resources/Python')
        else:
            lldb_py = os.path.join(platdir[0:offset+8], 'SharedFrameworks/LLDB.framework/Versions/A/Resources/Python')
        if os.path.isdir(lldb_py):
            sys.path.append(lldb_py)
            global lldb
            lldb = __import__('lldb')
            print('Found LLDB in SDK')
        else:
            print('Failed to locate lldb.py from', lldb_py)
            sys.exit(-1)
    return True

class Logger(object):
    """ A logging utility """
    def __init__(self, log_file_path="/tmp/xnu.log"):
        self.log_file_handle = open(log_file_path, "w+")
        self.redirect_to_stdout = False
        
    def log_debug(self, *args):
        current_timestamp = time.time()
        debug_line_str = "DEBUG:" + str(current_timestamp) + ":"
        for arg in args:
            debug_line_str += " " + str(arg).replace("\n", " ") + ", "
        
        self.log_file_handle.write(debug_line_str + "\n")
        if self.redirect_to_stdout :
            print(debug_line_str)
    
    def write(self, line):
        self.log_debug(line)


def sizeof_fmt(num, unit_str='B'):
    """ format large number into human readable values.
        convert any number into Kilo, Mega, Giga, Tera format for human understanding.
        params:
            num - int : number to be converted
            unit_str - str : a suffix for unit. defaults to 'B' for bytes.
        returns:
            str - formatted string for printing.
    """
    for x in ['','K','M','G','T']:
        if num < 1024.0:
            return "%3.1f%s%s" % (num, x,unit_str)
        num /= 1024.0
    return "%3.1f%s%s" % (num, 'P', unit_str)

def WriteStringToMemoryAddress(stringval, addr):
    """ write a null terminated string to address. 
        params:
            stringval: str- string to be written to memory. a '\0' will be added at the end
            addr : int - address where data is to be written
        returns:
            bool - True if successfully written
    """
    serr = lldb.SBError()
    length = len(stringval) + 1
    format_string = "%ds" % length
    sdata = struct.pack(format_string,stringval.encode())
    numbytes = LazyTarget.GetProcess().WriteMemory(addr, sdata, serr)
    if numbytes == length and serr.Success():
        return True
    return False

def WriteInt64ToMemoryAddress(intval, addr):
    """ write a 64 bit integer at an address.
        params:
          intval - int - an integer value to be saved
          addr - int - address where int is to be written
        returns:
          bool - True if successfully written.
    """
    serr = lldb.SBError()
    sdata = struct.pack('Q', intval)
    addr = int(hex(addr).rstrip('L'), 16)
    numbytes = LazyTarget.GetProcess().WriteMemory(addr,sdata, serr)
    if numbytes == 8 and serr.Success():
        return True
    return False 

def WritePtrDataToMemoryAddress(intval, addr):
    """ Write data to pointer size memory. 
        This is equivalent of doing *(&((struct pmap *)addr)) = intval
        It will identify 32/64 bit kernel and write memory accordingly.
        params:
          intval - int - an integer value to be saved
          addr - int - address where int is to be written
        returns:
          bool - True if successfully written.
    """
    if kern.ptrsize == 8:
        return WriteInt64ToMemoryAddress(intval, addr)
    else:
        return WriteInt32ToMemoryAddress(intval, addr)

def WriteInt32ToMemoryAddress(intval, addr):
    """ write a 32 bit integer at an address.
        params:
          intval - int - an integer value to be saved
          addr - int - address where int is to be written
        returns:
          bool - True if successfully written.
    """
    serr = lldb.SBError()
    sdata = struct.pack('I', intval)
    addr = int(hex(addr).rstrip('L'), 16)
    numbytes = LazyTarget.GetProcess().WriteMemory(addr,sdata, serr)
    if numbytes == 4 and serr.Success():
        return True
    return False 

def WriteInt16ToMemoryAddress(intval, addr):
    """ write a 16 bit integer at an address.
        params:
          intval - int - an integer value to be saved
          addr - int - address where int is to be written
        returns:
          bool - True if successfully written.
    """
    serr = lldb.SBError()
    sdata = struct.pack('H', intval)
    addr = int(hex(addr).rstrip('L'), 16)
    numbytes = LazyTarget.GetProcess().WriteMemory(addr,sdata, serr)
    if numbytes == 2 and serr.Success():
        return True
    return False 

def WriteInt8ToMemoryAddress(intval, addr):
    """ write a 8 bit integer at an address.
        params:
          intval - int - an integer value to be saved
          addr - int - address where int is to be written
        returns:
          bool - True if successfully written.
    """
    serr = lldb.SBError()
    sdata = struct.pack('B', intval)
    addr = int(hex(addr).rstrip('L'), 16)
    numbytes = LazyTarget.GetProcess().WriteMemory(addr,sdata, serr)
    if numbytes == 1 and serr.Success():
        return True
    return False 

_enum_cache = {}
def GetEnumValue(enum_name_or_combined, member_name = None):
    """ Finds the value of a particular enum define. Ex kdp_req_t::KDP_VERSION  => 0x3
        params:
            enum_name_or_combined: str
                name of an enum of the format type::name (legacy)
                name of an enum type
            member_name: None, or the name of an enum member
                   (then enum_name_or_combined is a type name).
        returns:
            int - value of the particular enum.
        raises:
            TypeError - if the enum is not found
    """
    global _enum_cache
    if member_name is None:
        enum_name, member_name = enum_name_or_combined.strip().split("::")
    else:
        enum_name = enum_name_or_combined

    if enum_name not in _enum_cache:
        ty = GetType(enum_name)
        d  = {}

        for e in ty.get_enum_members_array():
            if ty.GetTypeFlags() & lldb.eTypeIsSigned:
                d[e.GetName()] = e.GetValueAsSigned()
            else:
                d[e.GetName()] = e.GetValueAsUnsigned()

        _enum_cache[enum_name] = d

    return _enum_cache[enum_name][member_name]

def GetEnumValues(enum_name, names):
    """ Finds the values of a particular set of enum defines.
        params:
            enum_name: str
                name of an enum type
            member_name: str list
                list of fields to resolve
        returns:
            int list - value of the particular enum.
        raises:
            TypeError - if the enum is not found
    """
    return [GetEnumValue(enum_name, x) for x in names]

_enum_name_cache = {}
def GetEnumName(enum_name, value, prefix = ''):
    """ Finds symbolic name for a particular enum integer value
        params:
            enum_name - str:   name of an enum type
            value     - value: the value to decode
            prefix    - str:   a prefix to strip from the tag
        returns:
            str - the symbolic name or UNKNOWN(value)
        raises:
            TypeError - if the enum is not found
    """
    global _enum_name_cache

    ty = GetType(enum_name)

    if enum_name not in _enum_name_cache:
        ty_dict  = {}

        for e in ty.get_enum_members_array():
            if ty.GetTypeFlags() & lldb.eTypeIsSigned:
                ty_dict[e.GetValueAsSigned()] = e.GetName()
            else:
                ty_dict[e.GetValueAsUnsigned()] = e.GetName()

        _enum_name_cache[enum_name] = ty_dict
    else:
        ty_dict = _enum_name_cache[enum_name]

    if ty.GetTypeFlags() & lldb.eTypeIsSigned:
        key = int(value)
    else:
        key = unsigned(value)

    name = ty_dict.get(key, "UNKNOWN({:d})".format(key))
    if name.startswith(prefix):
        return name[len(prefix):]
    return name

def GetOptionString(enum_name, value, prefix = ''):
    """ Tries to format a given value as a combination of options
        params:
            enum_name - str:   name of an enum type
            value     - value: the value to decode
            prefix    - str:   a prefix to strip from the tag
        raises:
            TypeError - if the enum is not found
    """
    ty = GetType(enum_name)

    if enum_name not in _enum_name_cache:
        ty_dict  = {}

        for e in ty.get_enum_members_array():
            if ty.GetTypeFlags() & lldb.eTypeIsSigned:
                ty_dict[e.GetValueAsSigned()] = e.GetName()
            else:
                ty_dict[e.GetValueAsUnsigned()] = e.GetName()

        _enum_name_cache[enum_name] = ty_dict
    else:
        ty_dict = _enum_name_cache[enum_name]

    if ty.GetTypeFlags() & lldb.eTypeIsSigned:
        v = int(value)
    else:
        v = unsigned(value)

    flags = []
    for bit in range(0, 64):
        mask = 1 << bit
        if not v & mask: continue
        if mask not in ty_dict: continue
        name = ty_dict[mask]
        if name.startswith(prefix):
            name = name[len(prefix):]
        flags.append(name)
        v &= ~mask
    if v:
        flags.append("UNKNOWN({:d})".format(v))
    return " ".join(flags)

def ResolveFSPath(path):
    """ expand ~user directories and return absolute path.
        params: path - str - eg "~rc/Software"
        returns:
                str - abs path with user directories and symlinks expanded.
                str - if path resolution fails then returns the same string back
    """
    expanded_path = os.path.expanduser(path)
    norm_path = os.path.normpath(expanded_path)
    return norm_path

_dsymlist = {}
uuid_regex = re.compile("[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}",re.IGNORECASE|re.DOTALL)
def addDSYM(uuid, info):
    """ add a module by dsym into the target modules. 
        params: uuid - str - uuid string eg. 4DD2344C0-4A81-3EAB-BDCF-FEAFED9EB73E
                info - dict - info dictionary passed from dsymForUUID
    """
    global _dsymlist
    if "DBGSymbolRichExecutable" not in info:
        print("Error: Unable to find syms for %s" % uuid)
        return False
    if not uuid in _dsymlist:
        # add the dsym itself
        cmd_str = "target modules add --uuid %s" % uuid
        debuglog(cmd_str)
        lldb.debugger.HandleCommand(cmd_str)
        # set up source path
        #lldb.debugger.HandleCommand("settings append target.source-map %s %s" % (info["DBGBuildSourcePath"], info["DBGSourcePath"]))
        # modify the list to show we loaded this
        _dsymlist[uuid] = True

def loadDSYM(uuid, load_address, sections=[]):
    """ Load an already added symbols to a particular load address
        params: uuid - str - uuid string
                load_address - int - address where to load the symbols
        returns bool:
            True - if successful
            False - if failed. possible because uuid is not presently loaded.
    """
    if uuid not in _dsymlist:
        return False
    if not sections:
        cmd_str = "target modules load --uuid %s --slide %d" % ( uuid, load_address)
        debuglog(cmd_str)
    else:
        cmd_str = "target modules load --uuid {}   ".format(uuid)
        sections_str = ""
        for s in sections:
            sections_str += " {} {:#0x} ".format(s.name, s.vmaddr)
        cmd_str += sections_str
        debuglog(cmd_str)

    lldb.debugger.HandleCommand(cmd_str)
    return True


def RunShellCommand(command):
    """ Run a shell command in subprocess.
        params: command with arguments to run (a list is preferred, but a string is also supported)
        returns: (exit_code, stdout, stderr)
    """
    import subprocess

    if not isinstance(command, list):
        import shlex
        command = shlex.split(command)

    result = subprocess.run(command, capture_output=True, encoding="utf-8")
    returncode =  result.returncode
    stdout = result.stdout
    stderr = result.stderr

    if returncode != 0:
        print("Failed to run command. Command: {}, "
              "exit code: {}, stdout: '{}', stderr: '{}'".format(command, returncode, stdout, stderr))

    return (returncode, stdout, stderr)

def dsymForUUID(uuid):
    """ Get dsym informaiton by calling dsymForUUID 
        params: uuid - str - uuid string from executable. eg. 4DD2344C0-4A81-3EAB-BDCF-FEAFED9EB73E
        returns:
            {} - a dictionary holding dsym information printed by dsymForUUID. 
            None - if failed to find information
    """
    import plistlib
    rc, output, _ = RunShellCommand(["/usr/local/bin/dsymForUUID", "--copyExecutable", uuid])
    if rc != 0:
        return None

    if output:
        # because of <rdar://12713712>
        #plist = plistlib.readPlistFromString(output)
        #beginworkaround
        keyvalue_extract_re = re.compile("<key>(.*?)</key>\s*<string>(.*?)</string>",re.IGNORECASE|re.MULTILINE|re.DOTALL)
        plist={}
        plist[uuid] = {}
        for item in keyvalue_extract_re.findall(output):
            plist[uuid][item[0]] = item[1]
        #endworkaround
        if plist and plist[uuid]:
            return plist[uuid]
    return None

def debuglog(s):
    """ Print a object in the debug stream
    """
    global config
    if config['debug']:
      print("DEBUG:",s)
    return None

def IsAppleInternal():
    """ check if apple_internal modules are available
        returns: True if apple_internal module is present
    """
    import imp
    try:
        imp.find_module("apple_internal")
        retval = True
    except ImportError:
        retval = False
    return retval

def print_hex_data(data, start=0, desc="", marks={}, prefix=" "):
    """ print on stdout "hexdump -C < data" like output
        params:
            data - bytearray or array of int where each int < 255
            start - int offset that should be printed in left column
            desc - str optional description to print on the first line to describe data
            mark - dictionary of markers
    """

    if desc:
        print("{}:".format(desc))

    end = start + len(data)

    for row in range(start & -16, end, 16):
        line  = ""
        chars = ""

        for col in range(16):
            addr = row + col

            if col == 8:
                line += " "
            if start <= addr < end:
                b      = data[addr - start]
                line  += "{}{:02x}".format(marks.get(addr, ' '), b)
                chars += chr(b) if 0x20 <= b < 0x80 else '.'
            else:
                line  += "   "
                chars += ' '

        print("{}{:#016x} {}  |{}|".format(prefix, row, line, chars))

def Ones(x):
    return (1 << x)-1

def StripPAC(x, TySz):
    sign_mask = 1 << 55
    ptr_mask = Ones(64-TySz)
    pac_mask = ~ptr_mask
    sign = x & sign_mask
    if sign:
        return (x | pac_mask) + 2**64
    else:
        return x & ptr_mask
