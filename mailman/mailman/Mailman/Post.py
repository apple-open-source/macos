#! /usr/bin/env python
#
# Copyright (C) 2001,2002 by the Free Software Foundation, Inc.
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

import sys

from Mailman import mm_cfg
from Mailman.Queue.sbcache import get_switchboard



def inject(listname, msg, recips=None, qdir=None):
    if qdir is None:
        qdir = mm_cfg.INQUEUE_DIR
    queue = get_switchboard(qdir)
    kws = {'listname'  : listname,
           'tolist'    : 1,
           '_plaintext': 1,
           }
    if recips:
        kws['recips'] = recips
    queue.enqueue(msg, **kws)



if __name__ == '__main__':
    # When called as a command line script, standard input is read to get the
    # list that this message is destined to, the list of explicit recipients,
    # and the message to send (in its entirety).  stdin must have the
    # following format:
    #
    # line 1: the internal name of the mailing list
    # line 2: the number of explicit recipients to follow.  0 means to use the
    #         list's membership to calculate recipients.
    # line 3 - 3+recipnum: explicit recipients, one per line
    # line 4+recipnum - end of file: the message in RFC 822 format (may
    #         include an initial Unix-from header)
    listname = sys.stdin.readline().strip()
    numrecips = int(sys.stdin.readline())
    if numrecips == 0:
        recips = None
    else:
        recips = []
        for i in range(numrecips):
            recips.append(sys.stdin.readline().strip())
    # If the message isn't parsable, we won't get an error here
    inject(listname, sys.stdin.read(), recips)
