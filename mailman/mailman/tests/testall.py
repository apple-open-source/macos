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

"""PyUnit-based test harness for Mailman."""

import unittest

MODULES = ('bounces', 'handlers', 'membership', 'safedict',
           'security_mgr', 'runners', 'lockfile', 'smtp',
           )

# test_message.py can only be run when mailmanctl is running, but mailmanctl
# can't be run when the other tests are run, 'cause it'll clobber qfiles.
# This means you have to run test_message.py manually. :(


def suite():
    alltests = unittest.TestSuite()
    for modname in MODULES:
        mod = __import__('test_'+modname)
        alltests.addTest(mod.suite())
    return alltests



if __name__ == '__main__':
    unittest.main(defaultTest='suite')
