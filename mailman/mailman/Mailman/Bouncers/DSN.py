# Copyright (C) 1998-2003 by the Free Software Foundation, Inc.
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

"""Parse RFC 3464 (i.e. DSN) bounce formats.

RFC 3464 obsoletes 1894 which was the old DSN standard.  This module has not
been audited for differences between the two.
"""

from email.Iterators import typed_subpart_iterator
from email.Utils import parseaddr
from cStringIO import StringIO



def check(msg):
    # Iterate over each message/delivery-status subpart
    addrs = []
    for part in typed_subpart_iterator(msg, 'message', 'delivery-status'):
        if not part.is_multipart():
            # Huh?
            continue
        # Each message/delivery-status contains a list of Message objects
        # which are the header blocks.  Iterate over those too.
        for msgblock in part.get_payload():
            # We try to dig out the Original-Recipient (which is optional) and
            # Final-Recipient (which is mandatory, but may not exactly match
            # an address on our list).  Some MTA's also use X-Actual-Recipient
            # as a synonym for Original-Recipient, but some apparently use
            # that for other purposes :(
            #
            # Also grok out Action so we can do something with that too.
            action = msgblock.get('action', '')
            # BAW: Should we treat delayed bounces the same?  Yes, because if
            # the transient problem clears up, they should get unbounced.  The
            # other problem is what to do about a DSN that has both delayed
            # and failed actions in multiple header blocks?  We're not
            # architected to handle that. ;/
            if action.lower() not in ('failed', 'failure', 'delayed'):
                # Some non-permanent failure, so ignore this block
                continue
            params = []
            foundp = 0
            for header in ('original-recipient', 'final-recipient'):
                for k, v in msgblock.get_params([], header):
                    if k.lower() == 'rfc822':
                        foundp = 1
                    else:
                        params.append(k)
                if foundp:
                    # Note that params should already be unquoted.
                    addrs.extend(params)
                    break
    # Uniquify
    rtnaddrs = {}
    for a in addrs:
        if a is not None:
            realname, a = parseaddr(a)
            rtnaddrs[a] = 1
    return rtnaddrs.keys()



def process(msg):
    # The report-type parameter should be "delivery-status", but it seems that
    # some DSN generating MTAs don't include this on the Content-Type: header,
    # so let's relax the test a bit.
    if not msg.is_multipart() or msg.get_subtype() <> 'report':
        return None
    return check(msg)
