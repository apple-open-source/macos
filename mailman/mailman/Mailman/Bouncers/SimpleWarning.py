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

"""Recognizes simple heuristically delimited warnings."""

from Mailman.Bouncers.SimpleMatch import _c
from Mailman.Bouncers.SimpleMatch import process as _process



# This is a list of tuples of the form
#
#     (start cre, end cre, address cre)
#
# where `cre' means compiled regular expression, start is the line just before
# the bouncing address block, end is the line just after the bouncing address
# block, and address cre is the regexp that will recognize the addresses.  It
# must have a group called `addr' which will contain exactly and only the
# address that bounced.
patterns = [
    # pop3.pta.lia.net
    (_c('The address to which the message has not yet been delivered is'),
     _c('No action is required on your part'),
     _c(r'\s*(?P<addr>\S+@\S+)\s*')),
    # Next one goes here...
    ]



def process(msg):
    return _process(msg, patterns)
