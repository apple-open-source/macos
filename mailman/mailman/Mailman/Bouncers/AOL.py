# Copyright (C) 2009 by the Free Software Foundation, Inc.
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

"""Recognizes a class of messages from AOL that report only Screen Name."""

import re
from email.Utils import parseaddr

scre = re.compile('mail to the following recipients could not be delivered')

def process(msg):
    if msg.get_content_type() <> 'text/plain':
        return
    if not parseaddr(msg.get('from', ''))[1].lower().endswith('@aol.com'):
        return
    addrs = []
    found = False
    for line in msg.get_payload(decode=True).splitlines():
        if scre.search(line):
            found = True
            continue
        if found:
            local = line.strip()
            if local:
                if re.search(r'\s', local):
                    break
                if re.search('@', local):
                    addrs.append(local)
                else:
                    addrs.append('%s@aol.com' % local)
    return addrs
