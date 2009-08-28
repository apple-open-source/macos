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

"""Netscape Messaging Server bounce formats.

I've seen at least one NMS server version 3.6 (envy.gmp.usyd.edu.au) bounce
messages of this format.  Bounces come in DSN MIME format, but don't include
any -Recipient: headers.  Gotta just parse the text :(

NMS 4.1 (dfw-smtpin1.email.verio.net) seems even worse, but we'll try to
decipher the format here too.

"""

import re
from cStringIO import StringIO

pcre = re.compile(
    r'This Message was undeliverable due to the following reason:',
    re.IGNORECASE)

acre = re.compile(
    r'(?P<reply>please reply to)?.*<(?P<addr>[^>]*)>',
    re.IGNORECASE)



def flatten(msg, leaves):
    # give us all the leaf (non-multipart) subparts
    if msg.is_multipart():
        for part in msg.get_payload():
            flatten(part, leaves)
    else:
        leaves.append(msg)



def process(msg):
    # Sigh.  Some show NMS 3.6's show
    #     multipart/report; report-type=delivery-status
    # and some show
    #     multipart/mixed;
    if not msg.is_multipart():
        return None
    # We're looking for a text/plain subpart occuring before a
    # message/delivery-status subpart.
    plainmsg = None
    leaves = []
    flatten(msg, leaves)
    for i, subpart in zip(range(len(leaves)-1), leaves):
        if subpart.get_content_type() == 'text/plain':
            plainmsg = subpart
            break
    if not plainmsg:
        return None
    # Total guesswork, based on captured examples...
    body = StringIO(plainmsg.get_payload())
    addrs = []
    while 1:
        line = body.readline()
        if not line:
            break
        mo = pcre.search(line)
        if mo:
            # We found a bounce section, but I have no idea what the official
            # format inside here is.  :(  We'll just search for <addr>
            # strings.
            while 1:
                line = body.readline()
                if not line:
                    break
                mo = acre.search(line)
                if mo and not mo.group('reply'):
                    addrs.append(mo.group('addr'))
    return addrs
