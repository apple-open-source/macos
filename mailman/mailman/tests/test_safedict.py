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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

"""Unit tests for the SafeDict.py module
"""

import email
import unittest

from Mailman import SafeDict



class TestSafeDict(unittest.TestCase):
    def test_okay(self):
        sd = SafeDict.SafeDict({'foo': 'bar'})
        si = '%(foo)s' % sd
        self.assertEqual(si, 'bar')

    def test_key_error(self):
        sd = SafeDict.SafeDict({'foo': 'bar'})
        si = '%(baz)s' % sd
        self.assertEqual(si, '%(baz)s')

    def test_key_error_not_string(self):
        key = ()
        sd = SafeDict.SafeDict({})
        self.assertEqual(sd[key], '<Missing key: ()>')



class TestMsgSafeDict(unittest.TestCase):
    def setUp(self):
        self._msg = email.message_from_string("""To: foo
From: bar
Subject: baz
Cc: aperson@dom.ain
Cc: bperson@dom.ain

""")

    def test_normal_key(self):
        sd = SafeDict.MsgSafeDict(self._msg, {'key': 'value'})
        si = '%(key)s' % sd
        self.assertEqual(si, 'value')

    def test_msg_key(self):
        sd = SafeDict.MsgSafeDict(self._msg, {'to': 'value'})
        si = '%(msg_to)s' % sd
        self.assertEqual(si, 'foo')

    def test_allmsg_key(self):
        sd = SafeDict.MsgSafeDict(self._msg, {'cc': 'value'})
        si = '%(allmsg_cc)s' % sd
        self.assertEqual(si, 'aperson@dom.ain, bperson@dom.ain')

    def test_msg_no_key(self):
        sd = SafeDict.MsgSafeDict(self._msg)
        si = '%(msg_date)s' % sd
        self.assertEqual(si, 'n/a')

    def test_allmsg_no_key(self):
        sd = SafeDict.MsgSafeDict(self._msg)
        si = '%(allmsg_date)s' % sd
        self.assertEqual(si, 'n/a')

    def test_copy(self):
        sd = SafeDict.MsgSafeDict(self._msg, {'foo': 'bar'})
        copy = sd.copy()
        items = copy.items()
        items.sort()
        self.assertEqual(items, [
            ('allmsg_cc', 'aperson@dom.ain, bperson@dom.ain'),
            ('foo', 'bar'),
            ('msg_from', 'bar'),
            ('msg_subject', 'baz'),
            ('msg_to', 'foo'),
            ])


def suite():
    suite = unittest.TestSuite()
    suite.addTest(unittest.makeSuite(TestSafeDict))
    suite.addTest(unittest.makeSuite(TestMsgSafeDict))
    return suite



if __name__ == '__main__':
    unittest.main(defaultTest='suite')
