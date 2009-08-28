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

"""This appears to be the format for Novell GroupWise and NTMail

X-Mailer: Novell GroupWise Internet Agent 5.5.3.1
X-Mailer: NTMail v4.30.0012
X-Mailer: Internet Mail Service (5.5.2653.19)
"""

import re
from email.Message import Message
from cStringIO import StringIO

acre = re.compile(r'<(?P<addr>[^>]*)>')



def find_textplain(msg):
    if msg.get_content_type() == 'text/plain':
        return msg
    if msg.is_multipart:
        for part in msg.get_payload():
            if not isinstance(part, Message):
                continue
            ret = find_textplain(part)
            if ret:
                return ret
    return None



def process(msg):
    if msg.get_content_type() <> 'multipart/mixed' or not msg['x-mailer']:
        return None
    if msg['x-mailer'][:3].lower() not in ('nov', 'ntm', 'int'):
        return None
    addrs = {}
    # find the first text/plain part in the message
    textplain = find_textplain(msg)
    if not textplain:
        return None
    body = StringIO(textplain.get_payload())
    while 1:
        line = body.readline()
        if not line:
            break
        mo = acre.search(line)
        if mo:
            addrs[mo.group('addr')] = 1
        elif '@' in line:
            i = line.find(' ')
            if i == 0:
                continue
            if i < 0:
                addrs[line] = 1
            else:
                addrs[line[:i]] = 1
    return addrs.keys()
