# Copyright (C) 2001 by the Free Software Foundation, Inc.
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

"""Unit tests for the various Mailman/Queue/*Runner.py modules
"""

import unittest
import email

from Mailman.Queue.NewsRunner import prepare_message

from TestBase import TestBase



class TestPrepMessage(TestBase):
    def test_remove_unacceptables(self):
        eq = self.assertEqual
        msg = email.message_from_string("""\
From: aperson@dom.ain
To: _xtest@dom.ain
NNTP-Posting-Host: news.dom.ain
NNTP-Posting-Date: today
X-Trace: blah blah
X-Complaints-To: abuse@dom.ain
Xref: blah blah
Xref: blah blah
Date-Received: yesterday
Posted: tomorrow
Posting-Version: 99.99
Relay-Version: 88.88
Received: blah blah

A message
""")
        msgdata = {}
        prepare_message(self._mlist, msg, msgdata)
        eq(msgdata.get('prepped'), 1)
        eq(msg['from'], 'aperson@dom.ain')
        eq(msg['to'], '_xtest@dom.ain')
        eq(msg['nntp-posting-host'], None)
        eq(msg['nntp-posting-date'], None)
        eq(msg['x-trace'], None)
        eq(msg['x-complaints-to'], None)
        eq(msg['xref'], None)
        eq(msg['date-received'], None)
        eq(msg['posted'], None)
        eq(msg['posting-version'], None)
        eq(msg['relay-version'], None)
        eq(msg['received'], None)

    def test_munge_duplicates_no_duplicates(self):
        eq = self.assertEqual
        msg = email.message_from_string("""\
From: aperson@dom.ain
To: _xtest@dom.ain
Cc: someother@dom.ain
Content-Transfer-Encoding: yes

A message
""")
        msgdata = {}
        prepare_message(self._mlist, msg, msgdata)
        eq(msgdata.get('prepped'), 1)
        eq(msg['from'], 'aperson@dom.ain')
        eq(msg['to'], '_xtest@dom.ain')
        eq(msg['cc'], 'someother@dom.ain')
        eq(msg['content-transfer-encoding'], 'yes')

    def test_munge_duplicates(self):
        eq = self.assertEqual
        msg = email.message_from_string("""\
From: aperson@dom.ain
To: _xtest@dom.ain
To: two@dom.ain
Cc: three@dom.ain
Cc: four@dom.ain
Cc: five@dom.ain
Content-Transfer-Encoding: yes
Content-Transfer-Encoding: no
Content-Transfer-Encoding: maybe

A message
""")
        msgdata = {}
        prepare_message(self._mlist, msg, msgdata)
        eq(msgdata.get('prepped'), 1)
        eq(msg.get_all('from'), ['aperson@dom.ain'])
        eq(msg.get_all('to'), ['_xtest@dom.ain'])
        eq(msg.get_all('cc'), ['three@dom.ain'])
        eq(msg.get_all('content-transfer-encoding'), ['yes'])
        eq(msg.get_all('x-original-to'), ['two@dom.ain'])
        eq(msg.get_all('x-original-cc'), ['four@dom.ain', 'five@dom.ain'])
        eq(msg.get_all('x-original-content-transfer-encoding'),
           ['no', 'maybe'])



def suite():
    suite = unittest.TestSuite()
    suite.addTest(unittest.makeSuite(TestPrepMessage))
    return suite



if __name__ == '__main__':
    unittest.main(defaultTest='suite')
