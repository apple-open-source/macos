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

"""Test base class which handles creating and deleting a test list.
"""

import os
import shutil
import difflib
import unittest
from cStringIO import StringIO

from Mailman import MailList
from Mailman import Utils
from Mailman import mm_cfg

NL = '\n'



class TestBase(unittest.TestCase):
    if hasattr(difflib, 'ndiff'):
        # Python 2.2 and beyond
        def ndiffAssertEqual(self, first, second):
            """Like failUnlessEqual except use ndiff for readable output."""
            if first <> second:
                sfirst = str(first)
                ssecond = str(second)
                diff = difflib.ndiff(sfirst.splitlines(), ssecond.splitlines())
                fp = StringIO()
                print >> fp, NL, NL.join(diff)
                raise self.failureException, fp.getvalue()
    else:
        # Python 2.1
        ndiffAssertEqual = unittest.TestCase.assertEqual

    def setUp(self):
        mlist = MailList.MailList()
        mlist.Create('_xtest', 'test@dom.ain', 'xxxxx')
        mlist.host_name = 'dom.ain'
        mlist.web_page_url = 'http://www.dom.ain/mailman/'
        mlist.Save()
        # This leaves the list in a locked state
        self._mlist = mlist

    def tearDown(self):
        self._mlist.Unlock()
        listname = self._mlist.internal_name()
        for dirtmpl in ['lists/%s',
                        'archives/private/%s',
                        'archives/private/%s.mbox',
                        'archives/public/%s',
                        'archives/public/%s.mbox',
                        ]:
            dir = os.path.join(mm_cfg.VAR_PREFIX, dirtmpl % listname)
            if os.path.islink(dir):
                os.unlink(dir)
            elif os.path.isdir(dir):
                shutil.rmtree(dir)
