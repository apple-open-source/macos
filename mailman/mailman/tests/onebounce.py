#! /usr/bin/env python

# Copyright (C) 2002 by the Free Software Foundation, Inc.
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

"""Test the bounce detection for files containing bounces.

Usage: %(PROGRAM)s [options] file1 ...

Options:
    -h / --help
        Print this text and exit.

    -v / --verbose
        Verbose output.

    -a / --all
        Run the message through all the bounce modules.  Normally this script
        stops at the first one that finds a match.
"""

import sys
import email
import getopt

import paths
from Mailman.Bouncers import BouncerAPI

PROGRAM = sys.argv[0]
COMMASPACE = ', '



def usage(code, msg=''):
    print __doc__ % globals()
    if msg:
        print msg
    sys.exit(code)



def main():
    try:
        opts, args = getopt.getopt(sys.argv[1:], 'hva',
                                   ['help', 'verbose', 'all'])
    except getopt.error, msg:
        usage(1, msg)

    all = verbose = 0
    for opt, arg in opts:
        if opt in ('-h', '--help'):
            usage(0)
        elif opt in ('-v', '--verbose'):
            verbose = 1
        elif opt in ('-a', '--all'):
            all = 1

    for file in args:
        fp = open(file)
        msg = email.message_from_file(fp)
        fp.close()
        for module in BouncerAPI.BOUNCE_PIPELINE:
            modname = 'Mailman.Bouncers.' + module
            __import__(modname)
            addrs = sys.modules[modname].process(msg)
            if addrs is BouncerAPI.Stop:
                print module, 'got a Stop'
                if not all:
                    break
            if not addrs:
                if verbose:
                    print module, 'found no matches'
            else:
                print module, 'found', COMMASPACE.join(addrs)
                if not all:
                    break



if __name__ == '__main__':
    main()
