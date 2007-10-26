# Copyright (C) 1998-2006 by the Free Software Foundation, Inc.
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

"""Add the message to the archives."""

import time
from cStringIO import StringIO

from Mailman import mm_cfg
from Mailman.Queue.sbcache import get_switchboard



def process(mlist, msg, msgdata):
    # short circuits
    if msgdata.get('isdigest') or not mlist.archive:
        return
    # Common practice seems to favor "X-No-Archive: yes".  No other value for
    # this header seems to make sense, so we'll just test for it's presence.
    # I'm keeping "X-Archive: no" for backwards compatibility.
    if msg.has_key('x-no-archive') or msg.get('x-archive', '').lower() == 'no':
        return
    # Send the message to the archiver queue
    archq = get_switchboard(mm_cfg.ARCHQUEUE_DIR)
    # Send the message to the queue
    archq.enqueue(msg, msgdata)
