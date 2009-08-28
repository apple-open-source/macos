# Copyright (C) 1998-2007 by the Free Software Foundation, Inc.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
# USA.


"""Library for program-based construction of an HTML documents.

Encapsulate HTML formatting directives in classes that act as containers
for python and, recursively, for nested HTML formatting objects.
"""


# Eventually could abstract down to HtmlItem, which outputs an arbitrary html
# object given start / end tags, valid options, and a value.  Ug, objects
# shouldn't be adding their own newlines.  The next object should.


import types

from Mailman import mm_cfg
from Mailman import Utils
from Mailman.i18n import _

SPACE = ' '
EMPTYSTRING = ''
NL = '\n'



# Format an arbitrary object.
def HTMLFormatObject(item, indent):
    "Return a presentation of an object, invoking their Format method if any."
    if type(item) == type(''):
        return item
    elif not hasattr(item, "Format"):
        return `item`
    else:
        return item.Format(indent)

def CaseInsensitiveKeyedDict(d):
    result = {}
    for (k,v) in d.items():
        result[k.lower()] = v
    return result

# Given references to two dictionaries, copy the second dictionary into the
# first one.
def DictMerge(destination, fresh_dict):
    for (key, value) in fresh_dict.items():
        destination[key] = value

class Table:
    def __init__(self, **table_opts):
        self.cells = []
        self.cell_info = {}
        self.row_info = {}
        self.opts = table_opts

    def AddOptions(self, opts):
        DictMerge(self.opts, opts)

    # Sets all of the cells.  It writes over whatever cells you had there
    # previously.

    def SetAllCells(self, cells):
        self.cells = cells

    # Add a new blank row at the end
    def NewRow(self):
        self.cells.append([])

    # Add a new blank cell at the end
    def NewCell(self):
        self.cells[-1].append('')

    def AddRow(self, row):
        self.cells.append(row)

    def AddCell(self, cell):
        self.cells[-1].append(cell)

    def AddCellInfo(self, row, col, **kws):
        kws = CaseInsensitiveKeyedDict(kws)
        if not self.cell_info.has_key(row):
            self.cell_info[row] = { col : kws }
        elif self.cell_info[row].has_key(col):
            DictMerge(self.cell_info[row], kws)
        else:
            self.cell_info[row][col] = kws

    def AddRowInfo(self, row, **kws):
        kws = CaseInsensitiveKeyedDict(kws)
        if not self.row_info.has_key(row):
            self.row_info[row] = kws
        else:
            DictMerge(self.row_info[row], kws)

    # What's the index for the row we just put in?
    def GetCurrentRowIndex(self):
        return len(self.cells)-1

    # What's the index for the col we just put in?
    def GetCurrentCellIndex(self):
        return len(self.cells[-1])-1

    def ExtractCellInfo(self, info):
        valid_mods = ['align', 'valign', 'nowrap', 'rowspan', 'colspan',
                      'bgcolor']
        output = ''

        for (key, val) in info.items():
            if not key in valid_mods:
                continue
            if key == 'nowrap':
                output = output + ' NOWRAP'
                continue
            else:
                output = output + ' %s="%s"' % (key.upper(), val)

        return output

    def ExtractRowInfo(self, info):
        valid_mods = ['align', 'valign', 'bgcolor']
        output = ''

        for (key, val) in info.items():
            if not key in valid_mods:
                continue
            output = output + ' %s="%s"' % (key.upper(), val)

        return output

    def ExtractTableInfo(self, info):
        valid_mods = ['align', 'width', 'border', 'cellspacing', 'cellpadding',
                      'bgcolor']

        output = ''

        for (key, val) in info.items():
            if not key in valid_mods:
                continue
            if key == 'border' and val == None:
                output = output + ' BORDER'
                continue
            else:
                output = output + ' %s="%s"' % (key.upper(), val)

        return output

    def FormatCell(self, row, col, indent):
        try:
            my_info = self.cell_info[row][col]
        except:
            my_info = None

        output = '\n' + ' '*indent + '<td'
        if my_info:
            output = output + self.ExtractCellInfo(my_info)
        item = self.cells[row][col]
        item_format = HTMLFormatObject(item, indent+4)
        output = '%s>%s</td>' % (output, item_format)
        return output

    def FormatRow(self, row, indent):
        try:
            my_info = self.row_info[row]
        except:
            my_info = None

        output = '\n' + ' '*indent + '<tr'
        if my_info:
            output = output + self.ExtractRowInfo(my_info)
        output = output + '>'

        for i in range(len(self.cells[row])):
            output = output + self.FormatCell(row, i, indent + 2)

        output = output + '\n' + ' '*indent + '</tr>'

        return output

    def Format(self, indent=0):
        output = '\n' + ' '*indent + '<table'
        output = output + self.ExtractTableInfo(self.opts)
        output = output + '>'

        for i in range(len(self.cells)):
            output = output + self.FormatRow(i, indent + 2)

        output = output + '\n' + ' '*indent + '</table>\n'

        return output


class Link:
    def __init__(self, href, text, target=None):
        self.href = href
        self.text = text
        self.target = target

    def Format(self, indent=0):
        texpr = ""
        if self.target != None:
            texpr = ' target="%s"' % self.target
        return '<a href="%s"%s>%s</a>' % (HTMLFormatObject(self.href, indent),
                                          texpr,
                                          HTMLFormatObject(self.text, indent))

class FontSize:
    """FontSize is being deprecated - use FontAttr(..., size="...") instead."""
    def __init__(self, size, *items):
        self.items = list(items)
        self.size = size

    def Format(self, indent=0):
        output = '<font size="%s">' % self.size
        for item in self.items:
            output = output + HTMLFormatObject(item, indent)
        output = output + '</font>'
        return output

class FontAttr:
    """Present arbitrary font attributes."""
    def __init__(self, *items, **kw):
        self.items = list(items)
        self.attrs = kw

    def Format(self, indent=0):
        seq = []
        for k, v in self.attrs.items():
            seq.append('%s="%s"' % (k, v))
        output = '<font %s>' % SPACE.join(seq)
        for item in self.items:
            output = output + HTMLFormatObject(item, indent)
        output = output + '</font>'
        return output


class Container:
    def __init__(self, *items):
        if not items:
            self.items = []
        else:
            self.items = items

    def AddItem(self, obj):
        self.items.append(obj)

    def Format(self, indent=0):
        output = []
        for item in self.items:
            output.append(HTMLFormatObject(item, indent))
        return EMPTYSTRING.join(output)


class Label(Container):
    align = 'right'

    def __init__(self, *items):
        Container.__init__(self, *items)

    def Format(self, indent=0):
        return ('<div align="%s">' % self.align) + \
               Container.Format(self, indent) + \
               '</div>'


# My own standard document template.  YMMV.
# something more abstract would be more work to use...

class Document(Container):
    title = None
    language = None
    bgcolor = mm_cfg.WEB_BG_COLOR
    suppress_head = 0

    def set_language(self, lang=None):
        self.language = lang

    def set_bgcolor(self, color):
        self.bgcolor = color

    def SetTitle(self, title):
        self.title = title

    def Format(self, indent=0, **kws):
        charset = 'us-ascii'
        if self.language:
            charset = Utils.GetCharSet(self.language)
        output = ['Content-Type: text/html; charset=%s\n' % charset]
        if not self.suppress_head:
            kws.setdefault('bgcolor', self.bgcolor)
            tab = ' ' * indent
            output.extend([tab,
                           '<HTML>',
                           '<HEAD>'
                           ])
            if mm_cfg.IMAGE_LOGOS:
                output.append('<LINK REL="SHORTCUT ICON" HREF="%s">' %
                              (mm_cfg.IMAGE_LOGOS + mm_cfg.SHORTCUT_ICON))
            # Hit all the bases
            output.append('<META http-equiv="Content-Type" '
                          'content="text/html; charset=%s">' % charset)
            if self.title:
                output.append('%s<TITLE>%s</TITLE>' % (tab, self.title))
            output.append('%s</HEAD>' % tab)
            quals = []
            # Default link colors
            if mm_cfg.WEB_VLINK_COLOR:
                kws.setdefault('vlink', mm_cfg.WEB_VLINK_COLOR)
            if mm_cfg.WEB_ALINK_COLOR:
                kws.setdefault('alink', mm_cfg.WEB_ALINK_COLOR)
            if mm_cfg.WEB_LINK_COLOR:
                kws.setdefault('link', mm_cfg.WEB_LINK_COLOR)
            for k, v in kws.items():
                quals.append('%s="%s"' % (k, v))
            output.append('%s<BODY %s' % (tab, SPACE.join(quals)))
            # Language direction
            direction = Utils.GetDirection(self.language)
            output.append('dir="%s">' % direction)
        # Always do this...
        output.append(Container.Format(self, indent))
        if not self.suppress_head:
            output.append('%s</BODY>' % tab)
            output.append('%s</HTML>' % tab)
        return NL.join(output)

    def addError(self, errmsg, tag=None):
        if tag is None:
            tag = _('Error: ')
        self.AddItem(Header(3, Bold(FontAttr(
            _(tag), color=mm_cfg.WEB_ERROR_COLOR, size='+2')).Format() +
                            Italic(errmsg).Format()))


class HeadlessDocument(Document):
    """Document without head section, for templates that provide their own."""
    suppress_head = 1


class StdContainer(Container):
    def Format(self, indent=0):
        # If I don't start a new I ignore indent
        output = '<%s>' % self.tag
        output = output + Container.Format(self, indent)
        output = '%s</%s>' % (output, self.tag)
        return output


class QuotedContainer(Container):
    def Format(self, indent=0):
        # If I don't start a new I ignore indent
        output = '<%s>%s</%s>' % (
            self.tag,
            Utils.websafe(Container.Format(self, indent)),
            self.tag)
        return output

class Header(StdContainer):
    def __init__(self, num, *items):
        self.items = items
        self.tag = 'h%d' % num

class Address(StdContainer):
    tag = 'address'

class Underline(StdContainer):
    tag = 'u'

class Bold(StdContainer):
    tag = 'strong'

class Italic(StdContainer):
    tag = 'em'

class Preformatted(QuotedContainer):
    tag = 'pre'

class Subscript(StdContainer):
    tag = 'sub'

class Superscript(StdContainer):
    tag = 'sup'

class Strikeout(StdContainer):
    tag = 'strike'

class Center(StdContainer):
    tag = 'center'

class Form(Container):
    def __init__(self, action='', method='POST', encoding=None, *items):
        apply(Container.__init__, (self,) +  items)
        self.action = action
        self.method = method
        self.encoding = encoding

    def set_action(self, action):
        self.action = action

    def Format(self, indent=0):
        spaces = ' ' * indent
        encoding = ''
        if self.encoding:
            encoding = 'enctype="%s"' % self.encoding
        output = '\n%s<FORM action="%s" method="%s" %s>\n' % (
            spaces, self.action, self.method, encoding)
        output = output + Container.Format(self, indent+2)
        output = '%s\n%s</FORM>\n' % (output, spaces)
        return output


class InputObj:
    def __init__(self, name, ty, value, checked, **kws):
        self.name = name
        self.type = ty
        self.value = value
        self.checked = checked
        self.kws = kws

    def Format(self, indent=0):
        output = ['<INPUT name="%s" type="%s" value="%s"' %
                  (self.name, self.type, self.value)]
        for item in self.kws.items():
            output.append('%s="%s"' % item)
        if self.checked:
            output.append('CHECKED')
        output.append('>')
        return SPACE.join(output)


class SubmitButton(InputObj):
    def __init__(self, name, button_text):
        InputObj.__init__(self, name, "SUBMIT", button_text, checked=0)

class PasswordBox(InputObj):
    def __init__(self, name, value='', size=mm_cfg.TEXTFIELDWIDTH):
        InputObj.__init__(self, name, "PASSWORD", value, checked=0, size=size)

class TextBox(InputObj):
    def __init__(self, name, value='', size=mm_cfg.TEXTFIELDWIDTH):
        if isinstance(value, str):
            safevalue = Utils.websafe(value)
        else:
            safevalue = value
        InputObj.__init__(self, name, "TEXT", safevalue, checked=0, size=size)

class Hidden(InputObj):
    def __init__(self, name, value=''):
        InputObj.__init__(self, name, 'HIDDEN', value, checked=0)

class TextArea:
    def __init__(self, name, text='', rows=None, cols=None, wrap='soft',
                 readonly=0):
        if isinstance(text, str):
            safetext = Utils.websafe(text)
        else:
            safetext = text
        self.name = name
        self.text = safetext
        self.rows = rows
        self.cols = cols
        self.wrap = wrap
        self.readonly = readonly

    def Format(self, indent=0):
        output = '<TEXTAREA NAME=%s' % self.name
        if self.rows:
            output += ' ROWS=%s' % self.rows
        if self.cols:
            output += ' COLS=%s' % self.cols
        if self.wrap:
            output += ' WRAP=%s' % self.wrap
        if self.readonly:
            output += ' READONLY'
        output += '>%s</TEXTAREA>' % self.text
        return output

class FileUpload(InputObj):
    def __init__(self, name, rows=None, cols=None, **kws):
        apply(InputObj.__init__, (self, name, 'FILE', '', 0), kws)

class RadioButton(InputObj):
    def __init__(self, name, value, checked=0, **kws):
        apply(InputObj.__init__, (self, name, 'RADIO', value, checked), kws)

class CheckBox(InputObj):
    def __init__(self, name, value, checked=0, **kws):
        apply(InputObj.__init__, (self, name, "CHECKBOX", value, checked), kws)

class VerticalSpacer:
    def __init__(self, size=10):
        self.size = size
    def Format(self, indent=0):
        output = '<spacer type="vertical" height="%d">' % self.size
        return output

class WidgetArray:
    Widget = None

    def __init__(self, name, button_names, checked, horizontal, values):
        self.name = name
        self.button_names = button_names
        self.checked = checked
        self.horizontal = horizontal
        self.values = values
        assert len(values) == len(button_names)
        # Don't assert `checked' because for RadioButtons it is a scalar while
        # for CheckedBoxes it is a vector.  Subclasses will assert length.

    def ischecked(self, i):
        raise NotImplemented

    def Format(self, indent=0):
        t = Table(cellspacing=5)
        items = []
        for i, name, value in zip(range(len(self.button_names)),
                                  self.button_names,
                                  self.values):
            ischecked = (self.ischecked(i))
            item = self.Widget(self.name, value, ischecked).Format() + name
            items.append(item)
            if not self.horizontal:
                t.AddRow(items)
                items = []
        if self.horizontal:
            t.AddRow(items)
        return t.Format(indent)

class RadioButtonArray(WidgetArray):
    Widget = RadioButton

    def __init__(self, name, button_names, checked=None, horizontal=1,
                 values=None):
        if values is None:
            values = range(len(button_names))
        # BAW: assert checked is a scalar...
        WidgetArray.__init__(self, name, button_names, checked, horizontal,
                             values)

    def ischecked(self, i):
        return self.checked == i

class CheckBoxArray(WidgetArray):
    Widget = CheckBox

    def __init__(self, name, button_names, checked=None, horizontal=0,
                 values=None):
        if checked is None:
            checked = [0] * len(button_names)
        else:
            assert len(checked) == len(button_names)
        if values is None:
            values = range(len(button_names))
        WidgetArray.__init__(self, name, button_names, checked, horizontal,
                             values)

    def ischecked(self, i):
        return self.checked[i]

class UnorderedList(Container):
    def Format(self, indent=0):
        spaces = ' ' * indent
        output = '\n%s<ul>\n' % spaces
        for item in self.items:
            output = output + '%s<li>%s\n' % \
                     (spaces, HTMLFormatObject(item, indent + 2))
        output = output + '%s</ul>\n' % spaces
        return output

class OrderedList(Container):
    def Format(self, indent=0):
        spaces = ' ' * indent
        output = '\n%s<ol>\n' % spaces
        for item in self.items:
            output = output + '%s<li>%s\n' % \
                     (spaces, HTMLFormatObject(item, indent + 2))
        output = output + '%s</ol>\n' % spaces
        return output

class DefinitionList(Container):
    def Format(self, indent=0):
        spaces = ' ' * indent
        output = '\n%s<dl>\n' % spaces
        for dt, dd in self.items:
            output = output + '%s<dt>%s\n<dd>%s\n' % \
                     (spaces, HTMLFormatObject(dt, indent+2),
                      HTMLFormatObject(dd, indent+2))
        output = output + '%s</dl>\n' % spaces
        return output



# Logo constants
#
# These are the URLs which the image logos link to.  The Mailman home page now
# points at the gnu.org site instead of the www.list.org mirror.
#
from mm_cfg import MAILMAN_URL
PYTHON_URL  = 'http://www.python.org/'
GNU_URL     = 'http://www.gnu.org/'

# The names of the image logo files.  These are concatentated onto
# mm_cfg.IMAGE_LOGOS (not urljoined).
DELIVERED_BY = 'mailman.jpg'
PYTHON_POWERED = 'PythonPowered.png'
GNU_HEAD = 'gnu-head-tiny.jpg'


def MailmanLogo():
    t = Table(border=0, width='100%')
    if mm_cfg.IMAGE_LOGOS:
        def logo(file):
            return mm_cfg.IMAGE_LOGOS + file
        mmlink = '<img src="%s" alt="Delivered by Mailman" border=0>' \
                 '<br>version %s' % (logo(DELIVERED_BY), mm_cfg.VERSION)
        pylink = '<img src="%s" alt="Python Powered" border=0>' % \
                 logo(PYTHON_POWERED)
        gnulink = '<img src="%s" alt="GNU\'s Not Unix" border=0>' % \
                  logo(GNU_HEAD)
        t.AddRow([mmlink, pylink, gnulink])
    else:
        # use only textual links
        version = mm_cfg.VERSION
        mmlink = Link(MAILMAN_URL,
                      _('Delivered by Mailman<br>version %(version)s'))
        pylink = Link(PYTHON_URL, _('Python Powered'))
        gnulink = Link(GNU_URL, _("Gnu's Not Unix"))
        t.AddRow([mmlink, pylink, gnulink])
    return t


class SelectOptions:
   def __init__(self, varname, values, legend,
                selected=0, size=1, multiple=None):
      self.varname  = varname
      self.values   = values
      self.legend   = legend
      self.size     = size
      self.multiple = multiple
      # we convert any type to tuple, commas are needed
      if not multiple:
         if type(selected) == types.IntType:
             self.selected = (selected,)
         elif type(selected) == types.TupleType:
             self.selected = (selected[0],)
         elif type(selected) == types.ListType:
             self.selected = (selected[0],)
         else:
             self.selected = (0,)

   def Format(self, indent=0):
      spaces = " " * indent
      items  = min( len(self.values), len(self.legend) )

      # jcrey: If there is no argument, we return nothing to avoid errors
      if items == 0:
          return ""

      text = "\n" + spaces + "<Select name=\"%s\"" % self.varname
      if self.size > 1:
          text = text + " size=%d" % self.size
      if self.multiple:
          text = text + " multiple"
      text = text + ">\n"

      for i in range(items):
          if i in self.selected:
              checked = " Selected"
          else:
              checked = ""

          opt = " <option value=\"%s\"%s> %s </option>" % (
              self.values[i], checked, self.legend[i])
          text = text + spaces + opt + "\n"

      return text + spaces + '</Select>'
