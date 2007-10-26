# Copyright (C) 2001,2002 by the Free Software Foundation, Inc.
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

"""Get the normal delivery recipients from a Sendmail style :include: file.
"""

import os
import errno

from Mailman import Errors



def process(mlist, msg, msgdata):
    if msgdata.has_key('recips'):
        return
    filename = os.path.join(mlist.fullpath(), 'members.txt')
    try:
        fp = open(filename)
    except IOError, e:
        if e.errno <> errno.ENOENT:
            raise
        # If the file didn't exist, just set an empty recipients list
        msgdata['recips'] = []
        return
    # Read all the lines out of the file, and strip them of the trailing nl
    addrs = [line.strip() for line in fp.readlines()]
    # If the sender is in that list, remove him
    sender = msg.get_sender()
    if mlist.isMember(sender):
        try:
            addrs.remove(mlist.getMemberCPAddress(sender))
        except ValueError:
            # Don't worry if the sender isn't in the list
            pass
    msgdata['recips'] = addrs
