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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

"""Determine whether the message is approved for delivery.

This module only tests for definitive approvals.  IOW, this module only
determines whether the message is definitively approved or definitively
denied.  Situations that could hold a message for approval or confirmation are
not tested by this module.

"""

from email.Iterators import typed_subpart_iterator

from Mailman import mm_cfg
from Mailman import Errors

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
        for part in typed_subpart_iterator(msg, 'text', 'plain'):
            break
        if part is not None:
            lines = part.get_payload().splitlines()
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
                    part.set_payload(NL.join(lines[1:]))
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
