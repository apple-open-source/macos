# Copyright (C) 2002 by the Free Software Foundation, Inc.
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

"""sina.com bounces"""

import re
from email import Iterators

acre = re.compile(r'<(?P<addr>[^>]*)>')



def process(msg):
    if msg.get('from', '').lower() <> 'mailer-daemon@sina.com':
        print 'out 1'
        return []
    if not msg.is_multipart():
        print 'out 2'
        return []
    # The interesting bits are in the first text/plain multipart
    part = None
    try:
        part = msg.get_payload(0)
    except IndexError:
        pass
    if not part:
        print 'out 3'
        return []
    addrs = {}
    for line in Iterators.body_line_iterator(part):
        mo = acre.match(line)
        if mo:
            addrs[mo.group('addr')] = 1
    return addrs.keys()
