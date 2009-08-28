# Copyright (C) 2001-2008 by the Free Software Foundation, Inc.
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

"""Unit tests for Mailman/SecurityManager.py
"""

import os
import unittest
import errno
import Cookie
try:
    import crypt
except ImportError:
    crypt = None
# Don't use cStringIO because we're going to inherit
from StringIO import StringIO

from Mailman import mm_cfg
from Mailman import Utils
from Mailman import Errors
from Mailman.Utils import md5_new, sha_new

from TestBase import TestBase



def password(plaintext):
    return sha_new(plaintext).hexdigest()



class TestSecurityManager(TestBase):
    def test_init_vars(self):
        eq = self.assertEqual
        eq(self._mlist.mod_password, None)
        eq(self._mlist.passwords, {})

    def test_auth_context_info_authuser(self):
        mlist = self._mlist
        self.assertRaises(TypeError, mlist.AuthContextInfo, mm_cfg.AuthUser)
        # Add a member
        mlist.addNewMember('aperson@dom.ain', password='xxXXxx')
        self.assertEqual(
            mlist.AuthContextInfo(mm_cfg.AuthUser, 'aperson@dom.ain'),
            ('_xtest+user+aperson--at--dom.ain', 'xxXXxx'))

    def test_auth_context_moderator(self):
        mlist = self._mlist
        mlist.mod_password = 'yyYYyy'
        self.assertEqual(
            mlist.AuthContextInfo(mm_cfg.AuthListModerator),
            ('_xtest+moderator', 'yyYYyy'))

    def test_auth_context_admin(self):
        mlist = self._mlist
        mlist.password = 'zzZZzz'
        self.assertEqual(
            mlist.AuthContextInfo(mm_cfg.AuthListAdmin),
            ('_xtest+admin', 'zzZZzz'))

    def test_auth_context_site(self):
        mlist = self._mlist
        mlist.password = 'aaAAaa'
        self.assertEqual(
            mlist.AuthContextInfo(mm_cfg.AuthSiteAdmin),
            ('_xtest+admin', 'aaAAaa'))

    def test_auth_context_huh(self):
        self.assertEqual(
            self._mlist.AuthContextInfo('foo'),
            (None, None))



class TestAuthenticate(TestBase):
    def setUp(self):
        TestBase.setUp(self)
        Utils.set_global_password('bbBBbb', siteadmin=1)
        Utils.set_global_password('ccCCcc', siteadmin=0)

    def tearDown(self):
        try:
            os.unlink(mm_cfg.SITE_PW_FILE)
        except OSError, e:
            if e.errno <> errno.ENOENT: raise
        try:
            os.unlink(mm_cfg.LISTCREATOR_PW_FILE)
        except OSError, e:
            if e.errno <> errno.ENOENT: raise
        TestBase.tearDown(self)

    def test_auth_creator(self):
        self.assertEqual(self._mlist.Authenticate(
            [mm_cfg.AuthCreator], 'ccCCcc'), mm_cfg.AuthCreator)

    def test_auth_creator_unauth(self):
        self.assertEqual(self._mlist.Authenticate(
            [mm_cfg.AuthCreator], 'xxxxxx'), mm_cfg.UnAuthorized)

    def test_auth_site_admin(self):
        self.assertEqual(self._mlist.Authenticate(
            [mm_cfg.AuthSiteAdmin], 'bbBBbb'), mm_cfg.AuthSiteAdmin)

    def test_auth_site_admin_unauth(self):
        self.assertEqual(self._mlist.Authenticate(
            [mm_cfg.AuthSiteAdmin], 'xxxxxx'), mm_cfg.UnAuthorized)

    def test_list_admin(self):
        self._mlist.password = password('ttTTtt')
        self.assertEqual(self._mlist.Authenticate(
            [mm_cfg.AuthListAdmin], 'ttTTtt'), mm_cfg.AuthListAdmin)

    def test_list_admin_unauth(self):
        self._mlist.password = password('ttTTtt')
        self.assertEqual(self._mlist.Authenticate(
            [mm_cfg.AuthListAdmin], 'xxxxxx'), mm_cfg.UnAuthorized)

    def test_list_admin_upgrade(self):
        eq = self.assertEqual
        mlist = self._mlist
        mlist.password = md5_new('ssSSss').digest()
        eq(mlist.Authenticate(
            [mm_cfg.AuthListAdmin], 'ssSSss'), mm_cfg.AuthListAdmin)
        eq(mlist.password, password('ssSSss'))
        # Test crypt upgrades if crypt is supported
        if crypt:
            mlist.password = crypt.crypt('rrRRrr', 'zc')
            eq(self._mlist.Authenticate(
                [mm_cfg.AuthListAdmin], 'rrRRrr'), mm_cfg.AuthListAdmin)
            eq(mlist.password, password('rrRRrr'))

    def test_list_admin_oldstyle_unauth(self):
        eq = self.assertEqual
        mlist = self._mlist
        mlist.password = md5_new('ssSSss').digest()
        eq(mlist.Authenticate(
            [mm_cfg.AuthListAdmin], 'xxxxxx'), mm_cfg.UnAuthorized)
        eq(mlist.password, md5_new('ssSSss').digest())
        # Test crypt upgrades if crypt is supported
        if crypt:
            mlist.password = crypted = crypt.crypt('rrRRrr', 'zc')
            eq(self._mlist.Authenticate(
                [mm_cfg.AuthListAdmin], 'xxxxxx'), mm_cfg.UnAuthorized)
            eq(mlist.password, crypted)

    def test_list_moderator(self):
        self._mlist.mod_password = password('mmMMmm')
        self.assertEqual(self._mlist.Authenticate(
            [mm_cfg.AuthListModerator], 'mmMMmm'), mm_cfg.AuthListModerator)

    def test_user(self):
        mlist = self._mlist
        mlist.addNewMember('aperson@dom.ain', password='nosrepa')
        self.assertEqual(mlist.Authenticate(
            [mm_cfg.AuthUser], 'nosrepa', 'aperson@dom.ain'), mm_cfg.AuthUser)

    def test_wrong_user(self):
        mlist = self._mlist
        mlist.addNewMember('aperson@dom.ain', password='nosrepa')
        self.assertRaises(Errors.NotAMemberError, mlist.Authenticate,
                          [mm_cfg.AuthUser], 'nosrepa', 'bperson@dom.ain')

    def test_no_user(self):
        mlist = self._mlist
        mlist.addNewMember('aperson@dom.ain', password='nosrepa')
        self.assertRaises(AttributeError, mlist.Authenticate,
                          [mm_cfg.AuthUser], 'nosrepa')

    def test_user_unauth(self):
        mlist = self._mlist
        mlist.addNewMember('aperson@dom.ain', password='nosrepa')
        self.assertEqual(mlist.Authenticate(
            [mm_cfg.AuthUser], 'xxxxxx', 'aperson@dom.ain'),
                         mm_cfg.UnAuthorized)

    def test_value_error(self):
        self.assertRaises(ValueError, self._mlist.Authenticate,
                          ['spooge'], 'xxxxxx', 'zperson@dom.ain')



class StripperIO(StringIO):
    HEAD = 'Set-Cookie: '
    def write(self, s):
        if s.startswith(self.HEAD):
            s = s[len(self.HEAD):]
        StringIO.write(self, s)


class TestWebAuthenticate(TestBase):
    def setUp(self):
        TestBase.setUp(self)
        Utils.set_global_password('bbBBbb', siteadmin=1)
        Utils.set_global_password('ccCCcc', siteadmin=0)
        mlist = self._mlist
        mlist.mod_password = password('abcdefg')
        mlist.addNewMember('aperson@dom.ain', password='qqQQqq')
        # Set up the cookie data
        sfp = StripperIO()
        print >> sfp, mlist.MakeCookie(mm_cfg.AuthSiteAdmin)
        # AuthCreator isn't handled in AuthContextInfo()
        print >> sfp, mlist.MakeCookie(mm_cfg.AuthListAdmin)
        print >> sfp, mlist.MakeCookie(mm_cfg.AuthListModerator)
        print >> sfp, mlist.MakeCookie(mm_cfg.AuthUser, 'aperson@dom.ain')
        # Strip off the "Set-Cookie: " prefix
        cookie = sfp.getvalue()
        os.environ['HTTP_COOKIE'] = cookie

    def tearDown(self):
        try:
            os.unlink(mm_cfg.SITE_PW_FILE)
        except OSError, e:
            if e.errno <> errno.ENOENT: raise
        try:
            os.unlink(mm_cfg.LISTCREATOR_PW_FILE)
        except OSError, e:
            if e.errno <> errno.ENOENT: raise
        del os.environ['HTTP_COOKIE']
        TestBase.tearDown(self)

    def test_auth_site_admin(self):
        self.assertEqual(self._mlist.WebAuthenticate(
            [mm_cfg.AuthSiteAdmin], 'xxxxxx'), 1)

    def test_list_admin(self):
        self.assertEqual(self._mlist.WebAuthenticate(
            [mm_cfg.AuthListAdmin], 'xxxxxx'), 1)

    def test_list_moderator(self):
        self.assertEqual(self._mlist.WebAuthenticate(
            [mm_cfg.AuthListModerator], 'xxxxxx'), 1)

    def test_user(self):
        self.assertEqual(self._mlist.WebAuthenticate(
            [mm_cfg.AuthUser], 'xxxxxx'), 1)

    def test_not_a_user(self):
        self._mlist.removeMember('aperson@dom.ain')
        self.assertEqual(self._mlist.WebAuthenticate(
            [mm_cfg.AuthUser], 'xxxxxx', 'aperson@dom.ain'), 0)



# TBD: Tests for MakeCookie(), ZapCookie(), CheckCookie() -- although the
# latter is implicitly tested by testing WebAuthenticate() above.



def suite():
    suite = unittest.TestSuite()
    suite.addTest(unittest.makeSuite(TestSecurityManager))
    suite.addTest(unittest.makeSuite(TestAuthenticate))
    suite.addTest(unittest.makeSuite(TestWebAuthenticate))
    return suite



if __name__ == '__main__':
    unittest.main(defaultTest='suite')
