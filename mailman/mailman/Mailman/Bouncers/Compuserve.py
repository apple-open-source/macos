# Copyright (C) 1998,1999,2000,2001,2002 by the Free Software Foundation, Inc.
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

"""Compuserve has its own weird format for bounces."""

import re
import email

dcre = re.compile(r'your message could not be delivered', re.IGNORECASE)
acre = re.compile(r'Invalid receiver address: (?P<addr>.*)')



def process(msg):
    # simple state machine
    #    0 = nothing seen yet
    #    1 = intro line seen
    state = 0
    addrs = []
    for line in email.Iterators.body_line_iterator(msg):
        if state == 0:
            mo = dcre.search(line)
            if mo:
                state = 1
        elif state == 1:
            mo = dcre.search(line)
            if mo:
                break
            mo = acre.search(line)
            if mo:
                addrs.append(mo.group('addr'))
    return addrs
