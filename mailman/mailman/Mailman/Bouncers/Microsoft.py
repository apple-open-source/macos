# Copyright (C) 1998-2008 by the Free Software Foundation, Inc.
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

"""Microsoft's `SMTPSVC' nears I kin tell."""

import re
from cStringIO import StringIO
from types import ListType

scre = re.compile(r'transcript of session follows', re.IGNORECASE)



def process(msg):
    if msg.get_content_type() <> 'multipart/mixed':
        return None
    # Find the first subpart, which has no MIME type
    try:
        subpart = msg.get_payload(0)
    except IndexError:
        # The message *looked* like a multipart but wasn't
        return None
    data = subpart.get_payload()
    if isinstance(data, ListType):
        # The message is a multi-multipart, so not a matching bounce
        return None
    body = StringIO(data)
    state = 0
    addrs = []
    while 1:
        line = body.readline()
        if not line:
            break
        if state == 0:
            if scre.search(line):
                state = 1
        if state == 1:
            if '@' in line:
                addrs.append(line)
    return addrs
