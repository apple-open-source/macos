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

"""Unit tests for SMTPDirect and (eventually perhaps) Sendmail.
"""

import email
import unittest
import thread

from Mailman import mm_cfg
from Mailman.Handlers import SMTPDirect

from EmailBase import EmailBase

TESTPORT = 3925



class TestSMTPDirect(EmailBase):
    def setUp(self):
        self._origport = mm_cfg.SMTPPORT
        self._sessions = mm_cfg.SMTP_MAX_SESSIONS_PER_CONNECTION
        mm_cfg.SMTPPORT = TESTPORT
        mm_cfg.SMTP_MAX_SESSIONS_PER_CONNECTION = 1
        EmailBase.setUp(self)

    def tearDown(self):
        mm_cfg.SMTPPORT = self._origport
        mm_cfg.SMTP_MAX_SESSIONS_PER_CONNECTION = self._sessions
        EmailBase.tearDown(self)

    def test_disconnect_midsession(self):
        msgdata = {'recips': ['aperson@dom.ain', 'bperson@dom.ain'],
                   'personalize': 1,
                   }
        self._mlist.personalize = 1
        msg = email.message_from_string("""
From: cperson@dom.ain
To: _xtest@dom.ain
Subject: testing

testing
""")
        id = thread.start_new_thread(self._readmsg, ())
        SMTPDirect.process(self._mlist, msg, msgdata)



def suite():
    suite = unittest.TestSuite()
    #suite.addTest(unittest.makeSuite(TestSMTPDirect))
    return suite


if __name__ == '__main__':
    unittest.main(defaultTest='suite')
