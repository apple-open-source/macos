# Copyright (C) 2000-2007 by the Free Software Foundation, Inc.
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
from Mailman.Queue.BounceRunner import BounceMixin
from Mailman.Logging.Syslog import syslog

# This controls how often _doperiodic() will try to deal with deferred
# permanent failures.  It is a count of calls to _doperiodic()
DEAL_WITH_PERMFAILURES_EVERY = 10

try:
    True, False
except NameError:
    True = 1
    False = 0



class OutgoingRunner(Runner, BounceMixin):
    QDIR = mm_cfg.OUTQUEUE_DIR

    def __init__(self, slice=None, numslices=1):
        Runner.__init__(self, slice, numslices)
        BounceMixin.__init__(self)
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
            # Handle local rejects of probe messages differently.
            if msgdata.get('probe_token') and e.permfailures:
                self._probe_bounce(mlist, msgdata['probe_token'])
            else:
                # Delivery failed at SMTP time for some or all of the
                # recipients.  Permanent failures are registered as bounces,
                # but temporary failures are retried for later.
                #
                # BAW: msg is going to be the original message that failed
                # delivery, not a bounce message.  This may be confusing if
                # this is what's sent to the user in the probe message.  Maybe
                # we should craft a bounce-like message containing information
                # about the permanent SMTP failure?
                if e.permfailures:
                    self._queue_bounces(mlist.internal_name(), e.permfailures,
                                        msg)
                # Move temporary failures to the qfiles/retry queue which will
                # occasionally move them back here for another shot at
                # delivery.
                if e.tempfailures:
                    now = time.time()
                    recips = e.tempfailures
                    last_recip_count = msgdata.get('last_recip_count', 0)
                    deliver_until = msgdata.get('deliver_until', now)
                    if len(recips) == last_recip_count:
                        # We didn't make any progress, so don't attempt
                        # delivery any longer.  BAW: is this the best
                        # disposition?
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

    _doperiodic = BounceMixin._doperiodic

    def _cleanup(self):
        BounceMixin._cleanup(self)
        Runner._cleanup(self)
