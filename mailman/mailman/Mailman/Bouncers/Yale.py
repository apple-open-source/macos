# Copyright (C) 2000,2001,2002 by the Free Software Foundation, Inc.
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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

"""Yale's mail server is pretty dumb.

Its reports include the end user's name, but not the full domain.  I think we
can usually guess it right anyway.  This is completely based on examination of
the corpse, and is subject to failure whenever Yale even slightly changes
their MTA. :(

"""

import re
from cStringIO import StringIO
from email.Utils import getaddresses

scre = re.compile(r'Message not delivered to the following', re.IGNORECASE)
ecre = re.compile(r'Error Detail', re.IGNORECASE)
acre = re.compile(r'\s+(?P<addr>\S+)\s+')



def process(msg):
    if msg.is_multipart():
        return None
    try:
        whofrom = getaddresses([msg.get('from', '')])[0][1]
        if not whofrom:
            return None
        username, domain = whofrom.split('@', 1)
    except (IndexError, ValueError):
        return None
    if username.lower() <> 'mailer-daemon':
        return None
    parts = domain.split('.')
    parts.reverse()
    for part1, part2 in zip(parts, ('edu', 'yale')):
        if part1 <> part2:
            return None
    # Okay, we've established that the bounce came from the mailer-daemon at
    # yale.edu.  Let's look for a name, and then guess the relevant domains.
    names = {}
    body = StringIO(msg.get_payload())
    state = 0
    # simple state machine
    #     0 == init
    #     1 == intro found
    while 1:
        line = body.readline()
        if not line:
            break
        if state == 0 and scre.search(line):
            state = 1
        elif state == 1 and ecre.search(line):
            break
        elif state == 1:
            mo = acre.search(line)
            if mo:
                names[mo.group('addr')] = 1
    # Now we have a bunch of names, these are either @yale.edu or
    # @cs.yale.edu.  Add them both.
    addrs = []
    for name in names.keys():
        addrs.append(name + '@yale.edu')
        addrs.append(name + '@cs.yale.edu')
    return addrs
