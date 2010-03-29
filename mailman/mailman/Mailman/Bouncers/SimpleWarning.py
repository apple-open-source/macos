# Copyright (C) 2001-2009 by the Free Software Foundation, Inc.
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

"""Recognizes simple heuristically delimited warnings."""

from Mailman.Bouncers.BouncerAPI import Stop
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
    # This is from MessageSwitch.  It is a kludge because the text that
    # identifies it as a warning only comes after the address.  We can't
    # use ecre, because it really isn't significant, so we fake it.  Once
    # we see the start, we know it's a warning, and we're going to return
    # Stop anyway, so we match anything for the address and end.
    (_c('This is just a warning, you do not need to take any action'),
     _c('.+'),
     _c('(?P<addr>.+)')),
    # Symantec_AntiVirus_for_SMTP_Gateways - see comments for MessageSwitch
    (_c('Delivery attempts will continue to be made'),
     _c('.+'),
     _c('(?P<addr>.+)')),
    # Googlemail
    (_c('THIS IS A WARNING MESSAGE ONLY'),
     _c('Message will be retried'),
     _c(r'\s*(?P<addr>\S+@\S+)\s*')),
    # RS ver 1.0.95vs ? - see comments for MessageSwitch
    (_c('We will continue to try to deliver'),
     _c('.+'),
     _c('(?P<addr>.+)')),
    # Next one goes here...
    ]



def process(msg):
    if _process(msg, patterns):
        # It's a recognized warning so stop now
        return Stop
    else:
        return []
