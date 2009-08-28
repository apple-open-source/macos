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

"""Determine whether the message is approved for delivery.

This module only tests for definitive approvals.  IOW, this module only
determines whether the message is definitively approved or definitively
denied.  Situations that could hold a message for approval or confirmation are
not tested by this module.
"""

import re

from email.Iterators import typed_subpart_iterator

from Mailman import mm_cfg
from Mailman import Errors

# True/False
try:
    True, False
except NameError:
    True = 1
    False = 0

NL = '\n'



def process(mlist, msg, msgdata):
    # Short circuits
    if msgdata.get('approved'):
        # Digests, Usenet postings, and some other messages come pre-approved.
        # TBD: we may want to further filter Usenet messages, so the test
        # above may not be entirely correct.
        return
    # See if the message has an Approved or Approve header with a valid
    # list-moderator, list-admin.  Also look at the first non-whitespace line
    # in the file to see if it looks like an Approved header.  We are
    # specifically /not/ allowing the site admins password to work here
    # because we want to discourage the practice of sending the site admin
    # password through email in the clear.
    missing = []
    passwd = msg.get('approved', msg.get('approve', missing))
    if passwd is missing:
        # Find the first text/plain part in the message
        part = None
        stripped = False
        for part in typed_subpart_iterator(msg, 'text', 'plain'):
            break
        # XXX I'm not entirely sure why, but it is possible for the payload of
        # the part to be None, and you can't splitlines() on None.
        if part is not None and part.get_payload() is not None:
            lines = part.get_payload(decode=True).splitlines()
            line = ''
            for lineno, line in zip(range(len(lines)), lines):
                if line.strip():
                    break
            i = line.find(':')
            if i >= 0:
                name = line[:i]
                value = line[i+1:]
                if name.lower() in ('approve', 'approved'):
                    passwd = value.lstrip()
                    # Now strip the first line from the payload so the
                    # password doesn't leak.
                    del lines[lineno]
                    reset_payload(part, NL.join(lines))
                    stripped = True
        if stripped:
            # MAS: Bug 1181161 - Now try all the text parts in case it's
            # multipart/alternative with the approved line in HTML or other
            # text part.  We make a pattern from the Approved line and delete
            # it from all text/* parts in which we find it.  It would be
            # better to just iterate forward, but email compatability for pre
            # Python 2.2 returns a list, not a true iterator.
            #
            # This will process all the multipart/alternative parts in the
            # message as well as all other text parts.  We shouldn't find the
            # pattern outside the mp/a parts, but if we do, it is probably
            # best to delete it anyway as it does contain the password.
            #
            # Make a pattern to delete.  We can't just delete a line because
            # line of HTML or other fancy text may include additional message
            # text.  This pattern works with HTML.  It may not work with rtf
            # or whatever else is possible.
            pattern = name + ':(\s|&nbsp;)*' + re.escape(passwd)
            for part in typed_subpart_iterator(msg, 'text'):
                if part is not None and part.get_payload() is not None:
                    lines = part.get_payload(decode=True)
                    if re.search(pattern, lines):
                        reset_payload(part, re.sub(pattern, '', lines))
    if passwd is not missing and mlist.Authenticate((mm_cfg.AuthListModerator,
                                                     mm_cfg.AuthListAdmin),
                                                    passwd):
        # BAW: should we definitely deny if the password exists but does not
        # match?  For now we'll let it percolate up for further determination.
        msgdata['approved'] = 1
        # Used by the Emergency module
        msgdata['adminapproved'] = 1
    # has this message already been posted to this list?
    beentheres = [s.strip().lower() for s in msg.get_all('x-beenthere', [])]
    if mlist.GetListEmail().lower() in beentheres:
        raise Errors.LoopError

def reset_payload(part, payload):
    # Set decoded payload maintaining content-type, format and delsp.
    # TK: Message with 'charset=' cause trouble. So, instead of
    #     part.get_content_charset('us-ascii') ...
    cset = part.get_content_charset() or 'us-ascii'
    ctype = part.get_content_type()
    format = part.get_param('format')
    delsp = part.get_param('delsp')
    del part['content-transfer-encoding']
    del part['content-type']
    part.set_payload(payload, cset)
    part.set_type(ctype)
    if format:
        part.set_param('Format', format)
    if delsp:
        part.set_param('DelSp', delsp)
