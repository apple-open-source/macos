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

"""Do more detailed spam detection.

This module hard codes site wide spam detection.  By hacking the
KNOWN_SPAMMERS variable, you can set up more regular expression matches
against message headers.  If spam is detected the message is discarded
immediately.

TBD: This needs to be made more configurable and robust.
"""

import re

from Mailman import mm_cfg
from Mailman import Errors



class SpamDetected(Errors.DiscardMessage):
    """The message contains known spam"""



def process(mlist, msg, msgdata):
    if msgdata.get('approved'):
        return
    for header, regex in mm_cfg.KNOWN_SPAMMERS:
        cre = re.compile(regex, re.IGNORECASE)
        value = msg[header]
        if not value:
            continue
        mo = cre.search(value)
        if mo:
            # we've detected spam, so throw the message away
            raise SpamDetected
