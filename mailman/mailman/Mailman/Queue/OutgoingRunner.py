# Copyright (C) 2000-2003 by the Free Software Foundation, Inc.
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

"""Outgoing queue runner."""

import os
import sys
import copy
import time
import socket

import email

from Mailman import mm_cfg
from Mailman import Message
from Mailman import Errors
from Mailman import LockFile
from Mailman.Queue.Runner import Runner
from Mailman.Queue.Switchboard import Switchboard
from Mailman.Logging.Syslog import syslog

# This controls how often _doperiodic() will try to deal with deferred
# permanent failures.  It is a count of calls to _doperiodic()
DEAL_WITH_PERMFAILURES_EVERY = 10

try:
    True, False
except NameError:
    True = 1
    False = 0



class OutgoingRunner(Runner):
    QDIR = mm_cfg.OUTQUEUE_DIR

    def __init__(self, slice=None, numslices=1):
        Runner.__init__(self, slice, numslices)
        # Maps mailing lists to (recip, msg) tuples
        self._permfailures = {}
        self._permfail_counter = 0
        # We look this function up only at startup time
        modname = 'Mailman.Handlers.' + mm_cfg.DELIVERY_MODULE
        mod = __import__(modname)
        self._func = getattr(sys.modules[modname], 'process')
        # This prevents smtp server connection problems from filling up the
        # error log.  It gets reset if the message was successfully sent, and
        # set if there was a socket.error.
        self.__logged = False
        self.__retryq = Switchboard(mm_cfg.RETRYQUEUE_DIR)

    def _dispose(self, mlist, msg, msgdata):
        # See if we should retry delivery of this message again.
        deliver_after = msgdata.get('deliver_after', 0)
        if time.time() < deliver_after:
            return True
        # Make sure we have the most up-to-date state
        mlist.Load()
        try:
            pid = os.getpid()
            self._func(mlist, msg, msgdata)
            # Failsafe -- a child may have leaked through.
            if pid <> os.getpid():
                syslog('error', 'child process leaked thru: %s', modname)
                os._exit(1)
            self.__logged = False
        except socket.error:
            # There was a problem connecting to the SMTP server.  Log this
            # once, but crank up our sleep time so we don't fill the error
            # log.
            port = mm_cfg.SMTPPORT
            if port == 0:
                port = 'smtp'
            # Log this just once.
            if not self.__logged:
                syslog('error', 'Cannot connect to SMTP server %s on port %s',
                       mm_cfg.SMTPHOST, port)
                self.__logged = True
            return True
        except Errors.SomeRecipientsFailed, e:
            # The delivery module being used (SMTPDirect or Sendmail) failed
            # to deliver the message to one or all of the recipients.
            # Permanent failures should be registered (but registration
            # requires the list lock), and temporary failures should be
            # retried later.
            #
            # For permanent failures, make a copy of the message for bounce
            # handling.  I'm not sure this is necessary, or the right thing to
            # do.
            if e.permfailures:
                pcnt = len(e.permfailures)
                msgcopy = copy.deepcopy(msg)
                self._permfailures.setdefault(mlist, []).extend(
                    zip(e.permfailures, [msgcopy] * pcnt))
            # Move temporary failures to the qfiles/retry queue which will
            # occasionally move them back here for another shot at delivery.
            if e.tempfailures:
                now = time.time()
                recips = e.tempfailures
                last_recip_count = msgdata.get('last_recip_count', 0)
                deliver_until = msgdata.get('deliver_until', now)
                if len(recips) == last_recip_count:
                    # We didn't make any progress, so don't attempt delivery
                    # any longer.  BAW: is this the best disposition?
                    if now > deliver_until:
                        return False
                else:
                    # Keep trying to delivery this message for a while
                    deliver_until = now + mm_cfg.DELIVERY_RETRY_PERIOD
                msgdata['last_recip_count'] = len(recips)
                msgdata['deliver_until'] = deliver_until
                msgdata['recips'] = recips
                self.__retryq.enqueue(msg, msgdata)
        # We've successfully completed handling of this message
        return False

    def _doperiodic(self):
        # Periodically try to acquire the list lock and clear out the
        # permanent failures.
        self._permfail_counter += 1
        if self._permfail_counter < DEAL_WITH_PERMFAILURES_EVERY:
            return
        self._handle_permfailures()

    def _handle_permfailures(self):
        # Reset the counter
        self._permfail_counter = 0
        # And deal with the deferred permanent failures.
        for mlist in self._permfailures.keys():
            try:
                mlist.Lock(timeout=mm_cfg.LIST_LOCK_TIMEOUT)
            except LockFile.TimeOutError:
                return
            try:
                for recip, msg in self._permfailures[mlist]:
                    mlist.registerBounce(recip, msg)
                del self._permfailures[mlist]
                mlist.Save()
            finally:
                mlist.Unlock()

    def _cleanup(self):
        self._handle_permfailures()
        Runner._cleanup(self)
