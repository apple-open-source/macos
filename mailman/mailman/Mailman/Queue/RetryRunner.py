# Copyright (C) 2003 by the Free Software Foundation, Inc.
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

import time

from Mailman import mm_cfg
from Mailman.Queue.Runner import Runner
from Mailman.Queue.Switchboard import Switchboard

try:
    True, False
except NameError:
    True = 1
    False = 0



class RetryRunner(Runner):
    QDIR = mm_cfg.RETRYQUEUE_DIR
    SLEEPTIME = mm_cfg.minutes(15)

    def __init__(self, slice=None, numslices=1):
        Runner.__init__(self, slice, numslices)
        self.__outq = Switchboard(mm_cfg.OUTQUEUE_DIR)

    def _dispose(self, mlist, msg, msgdata):
        # Move it to the out queue for another retry
        self.__outq.enqueue(msg, msgdata)
        return False

    def _snooze(self, filecnt):
        # We always want to snooze
        time.sleep(self.SLEEPTIME)
