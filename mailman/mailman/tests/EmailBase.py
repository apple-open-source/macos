# Copyright (C) 2001-2003 by the Free Software Foundation, Inc.
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

"""Base class for tests that email things.
"""

import socket
import asyncore
import smtpd

from Mailman import mm_cfg

from TestBase import TestBase



MSGTEXT = None

class OneShotChannel(smtpd.SMTPChannel):
    def smtp_QUIT(self, arg):
        smtpd.SMTPChannel.smtp_QUIT(self, arg)
        raise asyncore.ExitNow


class SinkServer(smtpd.SMTPServer):
    def handle_accept(self):
        conn, addr = self.accept()
        channel = OneShotChannel(self, conn, addr)

    def process_message(self, peer, mailfrom, rcpttos, data):
        global MSGTEXT
        MSGTEXT = data



class EmailBase(TestBase):
    def setUp(self):
        TestBase.setUp(self)
        # Second argument tuple is ignored.
        self._server = SinkServer(('localhost', mm_cfg.SMTPPORT),
                                  ('localhost', 25))

    def tearDown(self):
        self._server.close()
        TestBase.tearDown(self)

    def _readmsg(self):
        global MSGTEXT
        # Save and unlock the list so that the qrunner process can open it and
        # lock it if necessary.  We'll re-lock the list in our finally clause
        # since that if an invariant of the test harness.
        self._mlist.Unlock()
        try:
            try:
                # timeout is in milliseconds, see asyncore.py poll3()
                asyncore.loop(timeout=30.0)
                MSGTEXT = None
            except asyncore.ExitNow:
                pass
            return MSGTEXT
        finally:
            self._mlist.Lock()
