# Copyright (C) 1998,1999,2000,2001,2002 by the Free Software Foundation, Inc.
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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

"""A `safe' dictionary for string interpolation."""

from types import StringType
from UserDict import UserDict

COMMASPACE = ', '



class SafeDict(UserDict):
    """Dictionary which returns a default value for unknown keys.

    This is used in maketext so that editing templates is a bit more robust.
    """
    def __getitem__(self, key):
        try:
            return self.data[key]
        except KeyError:
            if isinstance(key, StringType):
                return '%('+key+')s'
            else:
                return '<Missing key: %s>' % `key`

    def interpolate(self, template):
        return template % self



class MsgSafeDict(SafeDict):
    def __init__(self, msg, dict=None):
        self.__msg = msg
        SafeDict.__init__(self, dict)

    def __getitem__(self, key):
        if key.startswith('msg_'):
            return self.__msg.get(key[4:], 'n/a')
        elif key.startswith('allmsg_'):
            missing = []
            all = self.__msg.get_all(key[7:], missing)
            if all is missing:
                return 'n/a'
            return COMMASPACE.join(all)
        else:
            return SafeDict.__getitem__(self, key)

    def copy(self):
        d = self.data.copy()
        for k in self.__msg.keys():
            vals = self.__msg.get_all(k)
            if len(vals) == 1:
                d['msg_'+k.lower()] = vals[0]
            else:
                d['allmsg_'+k.lower()] = COMMASPACE.join(vals)
        return d
