#!/usr/bin/env python
#from xml.etree.cElementTree import *
#from os.path import basename
import getopt
import sys
import re

# Jump to the bottom of this file for the main routine

# Some hacks to make the API more readable, and to keep backwards compability
_pyname_re = re.compile('^\d')
_pyname_except_re = re.compile('^Bad')

_py_reserved_words = [ 'None', 'def', 'class', 'and', 'or' ]

_cardinal_types = {'CARD8':  'B', 'uint8_t': 'B',
                   'CARD16': 'H','uint16_t': 'H',
                   'CARD32': 'I','uint32_t': 'I',
                   'INT8':   'b', 'int8_t':  'b',
                   'INT16':  'h', 'int16_t': 'h',
                   'INT32':  'i', 'int32_t': 'i',
                   'BYTE': 'B',
                   'BOOL': 'B',
                   'char': 'b',
                   'void': 'B',
                   'float': 'f',
                   'double' : 'd'}
_pylines = []
_pylevel = 0
_ns = None

_py_fmt_fmt = ''
_py_fmt_size = 0
_py_fmt_list = []

def _py(fmt, *args):
    '''
    Writes the given line to the header file.
    '''
    _pylines[_pylevel].append(fmt % args)

def _py_popline():
    _pylines[_pylevel][-1:] = ()

def _py_setlevel(idx):
    '''
    Changes the array that source lines are written to.
    Supports writing different sections of the source file.
    '''
    global _pylevel
    while len(_pylines) <= idx:
        _pylines.append([])
    _pylevel = idx
    
def _t(str):
    '''
    Does Python-name conversion on a type tuple of strings.
    '''
    return str[-1]

def _n(str):
    '''
    Does Python-name conversion on a single string fragment.
    Handles some number-only names and reserved words.
    '''
    if _pyname_re.match(str) or str in _py_reserved_words:
        return '_' + str
    return str

def _b(bool):
    '''
    Boolean to string
    '''

    return 'True' if bool else 'False'


def _py_push_format(field, prefix=''):
    global _py_fmt_fmt, _py_fmt_size, _py_fmt_list

    _py_fmt_fmt += field.type.py_format_str
    _py_fmt_size += field.type.size
    _py_fmt_list.append(prefix + _n(field.field_name))

def _py_push_pad(nmemb):
    global _py_fmt_fmt, _py_fmt_size, _py_fmt_list

    num = '' if nmemb == 1 else str(nmemb)
    _py_fmt_fmt += num + 'x'
    _py_fmt_size += nmemb

def _py_flush_format():
    global _py_fmt_fmt, _py_fmt_size, _py_fmt_list

    joined = ', '.join(_py_fmt_list)
    retval = [ _py_fmt_fmt, _py_fmt_size, joined ]
    _py_fmt_fmt = ''
    _py_fmt_size = 0
    _py_fmt_list = []
    return retval
    
def py_open(self):
    '''
    Exported function that handles module open.
    Opens the files and writes out the auto-generated comment, header file includes, etc.
    '''
    global _ns
    _ns = self.namespace

    _py_setlevel(0)
    _py('#')
    _py('# This file generated automatically from %s by py_client.py.', _ns.file)
    _py('# Edit at your peril.')
    _py('#')
    _py('')

    _py('import xcb')
    _py('import cStringIO')
    _py('from struct import pack, unpack_from')
    _py('from array import array')
        
    if _ns.is_ext:
        for (n, h) in self.imports:
            _py('import %s', h)

        _py('')
        _py('MAJOR_VERSION = %s', _ns.major_version)
        _py('MINOR_VERSION = %s', _ns.minor_version)
        _py('')
        _py('key = xcb.ExtensionKey(\'%s\')', _ns.ext_xname)

    _py_setlevel(1)
    _py('')
    _py('class %sExtension(xcb.Extension):', _ns.header)

    _py_setlevel(2)
    _py('')
    _py('_events = {')

    _py_setlevel(3)
    _py('}')
    _py('')
    _py('_errors = {')

    _py_setlevel(4)
    _py('}')
    _py('')
    if _ns.is_ext:
        _py('xcb._add_ext(key, %sExtension, _events, _errors)', _ns.header)
    else:
        _py('xcb._add_core(%sExtension, Setup, _events, _errors)', _ns.header)
    

def py_close(self):
    '''
    Exported function that handles module close.
    Writes out all the stored content lines, then closes the file.
    '''
    pyfile = open('%s.py' % _ns.header, 'w')
    for list in _pylines:
        for line in list:
            pyfile.write(line)
            pyfile.write('\n')
    pyfile.close()

def py_enum(self, name):
    '''
    Exported function that handles enum declarations.
    '''
    _py_setlevel(0)
    _py('')
    _py('class %s:', _t(name))

    count = 0

    for (enam, eval) in self.values:
        _py('    %s = %s', _n(enam), eval if eval != '' else count)
        count += 1

def _py_type_setup(self, name, postfix=''):
    '''
    Sets up all the C-related state by adding additional data fields to
    all Field and Type objects.  Here is where we figure out most of our
    variable and function names.

    Recurses into child fields and list member types.
    '''
    # Do all the various names in advance
    self.py_type = _t(name) + postfix

    self.py_request_name = _t(name)
    self.py_checked_name = _t(name) + 'Checked'
    self.py_unchecked_name = _t(name) + 'Unchecked'
    self.py_reply_name = _t(name) + 'Reply'
    self.py_event_name = _t(name) + 'Event'
    self.py_cookie_name = _t(name) + 'Cookie'

    if _pyname_except_re.match(_t(name)):
        self.py_error_name = re.sub('Bad', '', _t(name), 1) + 'Error'
        self.py_except_name = _t(name)
    else:
        self.py_error_name = _t(name) + 'Error'
        self.py_except_name = 'Bad' + _t(name)

    if self.is_pad:
        self.py_format_str = ('' if self.nmemb == 1 else str(self.nmemb)) + 'x'
        self.py_format_len = 0

    elif self.is_simple or self.is_expr:
        self.py_format_str = _cardinal_types[_t(self.name)]
        self.py_format_len = 1

    elif self.is_list:
        if self.fixed_size():
            self.py_format_str = str(self.nmemb) + _cardinal_types[_t(self.member.name)]
            self.py_format_len = self.nmemb
        else:
            self.py_format_str = None
            self.py_format_len = -1

    elif self.is_container:

        self.py_format_str = ''
        self.py_format_len = 0
        self.py_fixed_size = 0

        for field in self.fields:
            _py_type_setup(field.type, field.field_type)

            field.py_type = _t(field.field_type)

            if field.type.py_format_len < 0:
                self.py_format_str = None
                self.py_format_len = -1
            elif self.py_format_len >= 0:
                self.py_format_str += field.type.py_format_str
                self.py_format_len += field.type.py_format_len

            if field.type.is_list:
                _py_type_setup(field.type.member, field.field_type)

                field.py_listtype = _t(field.type.member.name)
                if field.type.member.is_simple:
                    field.py_listtype = "'" + field.type.member.py_format_str + "'"

                field.py_listsize = -1
                if field.type.member.fixed_size():
                    field.py_listsize = field.type.member.size

            if field.type.fixed_size():
                self.py_fixed_size += field.type.size

def _py_get_length_field(expr):
    '''
    Figures out what C code is needed to get a length field.
    For fields that follow a variable-length field, use the accessor.
    Otherwise, just reference the structure field directly.
    '''
    if expr.lenfield_name != None:
        # This would be nicer if Request had an is_request attribute...
        if hasattr(expr.parent.parent, "opcode"):
            return expr.lenfield_name
        else:
            return 'self.%s' % expr.lenfield_name
    else:
        return str(expr.nmemb)

def _py_get_expr(expr):
    '''
    Figures out what C code is needed to get the length of a list field.
    Recurses for math operations.
    Returns bitcount for value-mask fields.
    Otherwise, uses the value of the length field.
    '''
    lenexp = _py_get_length_field(expr)

    if expr.op != None:
        return '(' + _py_get_expr(expr.lhs) + ' ' + expr.op + ' ' + _py_get_expr(expr.rhs) + ')'
    elif expr.bitfield:
        return 'xcb.popcount(' + lenexp + ')'
    else:
        return lenexp

def py_simple(self, name):
    '''
    Exported function that handles cardinal declarations.
    These are types which are typedef'd to one of the CARDx's char, float, etc.
    '''
    _py_type_setup(self, name, '')

def _py_type_alignsize(field):
    if field.type.is_list:
        return field.type.member.size if field.type.member.fixed_size() else 4
    if field.type.is_container:
        return field.type.size if field.type.fixed_size() else 4
    return field.type.size
        
def _py_complex(self, name):
    need_alignment = False
    _py('        count = 0')

    for field in self.fields:
        if field.auto:
            _py_push_pad(field.type.size)
            continue
        if field.type.is_simple:
            _py_push_format(field, 'self.')
            continue
        if field.type.is_pad:
            _py_push_pad(field.type.nmemb)
            continue

        (format, size, list) = _py_flush_format()
        if len(list) > 0:
            _py('        (%s,) = unpack_from(\'%s\', self, count)', list, format)
        if size > 0:
            _py('        count += %d', size)

        if need_alignment:
            _py('        count += xcb.type_pad(%d, count)', _py_type_alignsize(field))
        need_alignment = True

        if field.type.is_list:
            _py('        self.%s = xcb.List(self, count, %s, %s, %d)', _n(field.field_name), _py_get_expr(field.type.expr), field.py_listtype, field.py_listsize)
            _py('        count += len(self.%s.buf())', _n(field.field_name))
        elif field.type.is_container and field.type.fixed_size():
            _py('        self.%s = %s(self, count, %s)', _n(field.field_name), field.py_type, field.type.size)
            _py('        count += %s', field.type.size)
        else:
            _py('        self.%s = %s(self, count)', _n(field.field_name), field.py_type)
            _py('        count += len(self.%s)', _n(field.field_name))

    (format, size, list) = _py_flush_format()
    if len(list) > 0:
        if need_alignment:
            _py('        count += xcb.type_pad(4, count)')
        _py('        (%s,) = unpack_from(\'%s\', self, count)', list, format)
        _py('        count += %d', size)

    if self.fixed_size() or self.is_reply:
        if len(self.fields) > 0:
            _py_popline()

def py_struct(self, name):
    '''
    Exported function that handles structure declarations.
    '''
    _py_type_setup(self, name)

    _py_setlevel(0)
    _py('')
    _py('class %s(xcb.Struct):', self.py_type)
    if self.fixed_size():
        _py('    def __init__(self, parent, offset, size):')
        _py('        xcb.Struct.__init__(self, parent, offset, size)')
    else:
        _py('    def __init__(self, parent, offset):')
        _py('        xcb.Struct.__init__(self, parent, offset)')

    _py_complex(self, name)

    if not self.fixed_size():
        _py('        xcb._resize_obj(self, count)')

def py_union(self, name):
    '''
    Exported function that handles union declarations.
    '''
    _py_type_setup(self, name)

    _py_setlevel(0)
    _py('')
    _py('class %s(xcb.Union):', self.py_type)
    if self.fixed_size():
        _py('    def __init__(self, parent, offset, size):')
        _py('        xcb.Union.__init__(self, parent, offset, size)')
    else:
        _py('    def __init__(self, parent, offset):')
        _py('        xcb.Union.__init__(self, parent, offset)')

    _py('        count = 0')

    for field in self.fields:
        if field.type.is_simple:
            _py('        self.%s = unpack_from(\'%s\', self)', _n(field.field_name), field.type.py_format_str)
            _py('        count = max(count, %s)', field.type.size)
        elif field.type.is_list:
            _py('        self.%s = xcb.List(self, 0, %s, %s, %s)', _n(field.field_name), _py_get_expr(field.type.expr), field.py_listtype, field.py_listsize)
            _py('        count = max(count, len(self.%s.buf()))', _n(field.field_name))
        elif field.type.is_container and field.type.fixed_size():
            _py('        self.%s = %s(self, 0, %s)', _n(field.field_name), field.py_type, field.type.size)
            _py('        count = max(count, %s)', field.type.size)
        else:
            _py('        self.%s = %s(self, 0)', _n(field.field_name), field.py_type)
            _py('        count = max(count, len(self.%s))', _n(field.field_name))

    if not self.fixed_size():
        _py('        xcb._resize_obj(self, count)')

def _py_reply(self, name):
    '''
    Handles reply declarations.
    '''
    _py_type_setup(self, name, 'Reply')

    _py_setlevel(0)
    _py('')
    _py('class %s(xcb.Reply):', self.py_reply_name)
    _py('    def __init__(self, parent):')
    _py('        xcb.Reply.__init__(self, parent)')

    _py_complex(self, name)
    
def _py_request_helper(self, name, void, regular):
    '''
    Declares a request function.
    '''

    # Four stunningly confusing possibilities here:
    #
    #   Void            Non-void
    # ------------------------------
    # "req"            "req"
    # 0 flag           CHECKED flag   Normal Mode
    # void_cookie      req_cookie
    # ------------------------------
    # "req_checked"    "req_unchecked"
    # CHECKED flag     0 flag         Abnormal Mode
    # void_cookie      req_cookie
    # ------------------------------


    # Whether we are _checked or _unchecked
    checked = void and not regular
    unchecked = not void and not regular

    # What kind of cookie we return
    func_cookie = 'xcb.VoidCookie' if void else self.py_cookie_name

    # What flag is passed to xcb_request
    func_flags = checked or (not void and regular)

    # What our function name is
    func_name = self.py_request_name
    if checked:
        func_name = self.py_checked_name
    if unchecked:
        func_name = self.py_unchecked_name

    param_fields = []
    wire_fields = []

    for field in self.fields:
        if field.visible:
            # The field should appear as a call parameter
            param_fields.append(field)
        if field.wire:
            # We need to set the field up in the structure
            wire_fields.append(field)

    _py_setlevel(1)
    _py('')
    _py('    def %s(self, %s):', func_name, ', '.join([_n(x.field_name) for x in param_fields]))
    _py('        buf = cStringIO.StringIO()')

    for field in wire_fields:
        if field.auto:
            _py_push_pad(field.type.size)
            continue
        if field.type.is_simple:
            _py_push_format(field)
            continue
        if field.type.is_pad:
            _py_push_pad(field.type.nmemb)
            continue

        (format, size, list) = _py_flush_format()
        if size > 0:
            _py('        buf.write(pack(\'%s\', %s))', format, list)

        if field.type.is_expr:
            _py('        buf.write(pack(\'%s\', %s))', field.type.py_format_str, _py_get_expr(field.type.expr))
        elif field.type.is_pad:
            _py('        buf.write(pack(\'%sx\'))', field.type.nmemb)
        elif field.type.is_container:
            _py('        for elt in xcb.Iterator(%s, %d, \'%s\', False):', _n(field.field_name), field.type.py_format_len, _n(field.field_name))
            _py('            buf.write(pack(\'%s\', *elt))', field.type.py_format_str)
        elif field.type.is_list and field.type.member.is_simple:
            _py('        buf.write(str(buffer(array(\'%s\', %s))))', field.type.member.py_format_str, _n(field.field_name))
        else:
            _py('        for elt in xcb.Iterator(%s, %d, \'%s\', True):', _n(field.field_name), field.type.member.py_format_len, _n(field.field_name))
            _py('            buf.write(pack(\'%s\', *elt))', field.type.member.py_format_str)

    (format, size, list) = _py_flush_format()
    if size > 0:
        _py('        buf.write(pack(\'%s\', %s))', format, list)

    _py('        return self.send_request(xcb.Request(buf.getvalue(), %s, %s, %s),', self.opcode, _b(void), _b(func_flags))
    _py('                                 %s()%s', func_cookie, ')' if void else ',')
    if not void:
        _py('                                 %s)', self.py_reply_name)

def py_request(self, name):
    '''
    Exported function that handles request declarations.
    '''
    _py_type_setup(self, name, 'Request')
    _py_setlevel(0)

    if self.reply:
        # Cookie class declaration
        _py('')
        _py('class %s(xcb.Cookie):', self.py_cookie_name)
        _py('    pass')

    if self.reply:
        # Reply class definition
        _py_reply(self.reply, name)
        # Request prototypes
        _py_request_helper(self, name, False, True)
        _py_request_helper(self, name, False, False)
    else:
        # Request prototypes
        _py_request_helper(self, name, True, False)
        _py_request_helper(self, name, True, True)

def py_event(self, name):
    '''
    Exported function that handles event declarations.
    '''
    _py_type_setup(self, name, 'Event')

    # Structure definition
    _py_setlevel(0)
    _py('')
    _py('class %s(xcb.Event):', self.py_event_name)
    _py('    def __init__(self, parent):')
    _py('        xcb.Event.__init__(self, parent)')

    _py_complex(self, name)

    # Opcode define
    _py_setlevel(2)
    _py('    %s : %s,', self.opcodes[name], self.py_event_name)

def py_error(self, name):
    '''
    Exported function that handles error declarations.
    '''
    _py_type_setup(self, name, 'Error')

    # Structure definition
    _py_setlevel(0)
    _py('')
    _py('class %s(xcb.Error):', self.py_error_name)
    _py('    def __init__(self, parent):')
    _py('        xcb.Error.__init__(self, parent)')

    _py_complex(self, name)

    # Exception definition
    _py('')
    _py('class %s(xcb.ProtocolException):', self.py_except_name)
    _py('    pass')

    # Opcode define
    _py_setlevel(3)
    _py('    %s : (%s, %s),', self.opcodes[name], self.py_error_name, self.py_except_name)


# Main routine starts here

# Must create an "output" dictionary before any xcbgen imports.
output = {'open'    : py_open,
          'close'   : py_close,
          'simple'  : py_simple,
          'enum'    : py_enum,
          'struct'  : py_struct,
          'union'   : py_union,
          'request' : py_request,
          'event'   : py_event,
          'error'   : py_error
          }

# Boilerplate below this point

# Check for the argument that specifies path to the xcbgen python package.
try:
    opts, args = getopt.getopt(sys.argv[1:], 'p:')
except getopt.GetoptError, err:
    print str(err)
    print 'Usage: py_client.py [-p path] file.xml'
    sys.exit(1)

for (opt, arg) in opts:
    if opt == '-p':
        sys.path.append(arg)

# Import the module class
try:
    from xcbgen.state import Module
except ImportError:
    print ''
    print 'Failed to load the xcbgen Python package!'
    print 'Make sure that xcb/proto installed it on your Python path.'
    print 'If not, you will need to create a .pth file or define $PYTHONPATH'
    print 'to extend the path.'
    print 'Refer to the README file in xcb/proto for more info.'
    print ''
    raise

# Parse the xml header
module = Module(args[0], output)

# Build type-registry and resolve type dependencies
module.register()
module.resolve()

# Output the code
module.generate()
