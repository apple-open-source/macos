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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

"""Archive queue runner."""

import time
from email.Utils import parsedate_tz, mktime_tz, formatdate

from Mailman import mm_cfg
from Mailman import LockFile
from Mailman.Queue.Runner import Runner



class ArchRunner(Runner):
    QDIR = mm_cfg.ARCHQUEUE_DIR

    def _dispose(self, mlist, msg, msgdata):
        # Support clobber_date, i.e. setting the date in the archive to the
        # received date, not the (potentially bogus) Date: header of the
        # original message.
        clobber = 0
        originaldate = msg.get('date')
        receivedtime = formatdate(msgdata['received_time'])
        if not originaldate:
            clobber = 1
        elif mm_cfg.ARCHIVER_CLOBBER_DATE_POLICY == 1:
            clobber = 1
        elif mm_cfg.ARCHIVER_CLOBBER_DATE_POLICY == 2:
            # what's the timestamp on the original message?
            tup = parsedate_tz(originaldate)
            now = time.time()
            try:
                if not tup:
                    clobber = 1
                elif abs(now - mktime_tz(tup)) > \
                         mm_cfg.ARCHIVER_ALLOWABLE_SANE_DATE_SKEW:
                    clobber = 1
            except (ValueError, OverflowError):
                # The likely cause of this is that the year in the Date: field
                # is horribly incorrect, e.g. (from SF bug # 571634):
                # Date: Tue, 18 Jun 0102 05:12:09 +0500
                # Obviously clobber such dates.
                clobber = 1
        if clobber:
            del msg['date']
            del msg['x-original-date']
            msg['Date'] = receivedtime
            if originaldate:
                msg['X-Original-Date'] = originaldate
        # Always put an indication of when we received the message.
        msg['X-List-Received-Date'] = receivedtime
        # Now try to get the list lock
        try:
            mlist.Lock(timeout=mm_cfg.LIST_LOCK_TIMEOUT)
        except LockFile.TimeOutError:
            # oh well, try again later
            return 1
        try:
            mlist.ArchiveMail(msg)
            mlist.Save()
        finally:
            mlist.Unlock()
