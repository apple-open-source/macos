#! @PYTHON@
#
# Copyright (C) 2004 by the Free Software Foundation, Inc.
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

# Inspired by Florian Weimer.

"""Reset the passwords for members of a mailing list.

This script resets all the passwords of a mailing list's members.  It can also
be used to reset the lists of all members of all mailing lists, but it is your
responsibility to let the users know that their passwords have been changed.

This script is intended to be run as a bin/withlist script, i.e.

% bin/withlist -l -r reset_pw listname [options]

Options:
    -v / --verbose
        Print what the script is doing.
"""

import sys
import getopt

import paths
from Mailman import Utils
from Mailman.i18n import _


try:
    True, False
except NameError:
    True = 1
    False = 0



def usage(code, msg=''):
    if code:
        fd = sys.stderr
    else:
        fd = sys.stdout
    print >> fd, _(__doc__.replace('%', '%%'))
    if msg:
        print >> fd, msg
    sys.exit(code)



def reset_pw(mlist, *args):
    try:
        opts, args = getopt.getopt(args, 'v', ['verbose'])
    except getopt.error, msg:
        usage(1, msg)

    verbose = False
    for opt, args in opts:
        if opt in ('-v', '--verbose'):
            verbose = True

    listname = mlist.internal_name()
    if verbose:
        print _('Changing passwords for list: %(listname)s')

    for member in mlist.getMembers():
        randompw = Utils.MakeRandomPassword()
        mlist.setMemberPassword(member, randompw)
        if verbose:
            print _('New password for member %(member)40s: %(randompw)s')

    mlist.Save()



if __name__ == '__main__':
    usage(0)
