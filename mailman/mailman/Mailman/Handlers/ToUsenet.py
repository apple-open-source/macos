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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

"""Move the message to the mail->news queue."""

from Mailman import mm_cfg
from Mailman.Queue.sbcache import get_switchboard
from Mailman.Logging.Syslog import syslog

COMMASPACE = ', '


def process(mlist, msg, msgdata):
    # short circuits
    if not mlist.gateway_to_news or \
           msgdata.get('isdigest') or \
           msgdata.get('fromusenet'):
        return
    # sanity checks
    error = []
    if not mlist.linked_newsgroup:
        error.append('no newsgroup')
    if not mlist.nntp_host:
        error.append('no NNTP host')
    if error:
        syslog('error', 'NNTP gateway improperly configured: %s',
               COMMASPACE.join(error))
        return
    # Put the message in the news runner's queue
    newsq = get_switchboard(mm_cfg.NEWSQUEUE_DIR)
    newsq.enqueue(msg, msgdata, listname=mlist.internal_name())
