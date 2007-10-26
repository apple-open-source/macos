# Copyright (C) 2001-2004 by the Free Software Foundation, Inc.
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

"""User description class/structure, for ApprovedAddMember and friends."""


from types import UnicodeType



class UserDesc:
    def __init__(self, address=None, fullname=None, password=None,
                 digest=None, lang=None):
        if address is not None:
            self.address = address
        if fullname is not None:
            self.fullname = fullname
        if password is not None:
            self.password = password
        if digest is not None:
            self.digest = digest
        if lang is not None:
            self.language = lang

    def __iadd__(self, other):
        if getattr(other, 'address', None) is not None:
            self.address = other.address
        if getattr(other, 'fullname', None) is not None:
            self.fullname = other.fullname
        if getattr(other, 'password', None) is not None:
            self.password = other.password
        if getattr(other, 'digest', None) is not None:
            self.digest = other.digest
        if getattr(other, 'language', None) is not None:
            self.language = other.language
        return self

    def __repr__(self):
        address = getattr(self, 'address', 'n/a')
        fullname = getattr(self, 'fullname', 'n/a')
        password = getattr(self, 'password', 'n/a')
        digest = getattr(self, 'digest', 'n/a')
        if digest == 0:
            digest = 'no'
        elif digest == 1:
            digest = 'yes'
        language = getattr(self, 'language', 'n/a')
        # Make sure fullname and password are encoded if they're strings
        if isinstance(fullname, UnicodeType):
            fullname = fullname.encode('ascii', 'replace')
        if isinstance(password, UnicodeType):
            password = password.encode('ascii', 'replace')
        return '<UserDesc %s (%s) [%s] [digest? %s] [%s]>' % (
            address, fullname, password, digest, language)
