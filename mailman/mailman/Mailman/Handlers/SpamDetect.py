# Copyright (C) 1998-2003 by the Free Software Foundation, Inc.
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
from cStringIO import StringIO

from email.Generator import Generator

from Mailman import mm_cfg
from Mailman import Errors
from Mailman import i18n
from Mailman.Handlers.Hold import hold_for_approval

try:
    True, False
except NameError:
    True = 1
    False = 0

# First, play footsie with _ so that the following are marked as translated,
# but aren't actually translated until we need the text later on.
def _(s):
    return s



class SpamDetected(Errors.DiscardMessage):
    """The message contains known spam"""

class HeaderMatchHold(Errors.HoldMessage):
    reason = _('The message headers matched a filter rule')


# And reset the translator
_ = i18n._



class Tee:
    def __init__(self, outfp_a, outfp_b):
        self._outfp_a = outfp_a
        self._outfp_b = outfp_b

    def write(self, s):
        self._outfp_a.write(s)
        self._outfp_b.write(s)
        

# Class to capture the headers separate from the message body
class HeaderGenerator(Generator):
    def __init__(self, outfp, mangle_from_=True, maxheaderlen=78):
        Generator.__init__(self, outfp, mangle_from_, maxheaderlen)
        self._headertxt = ''

    def _write_headers(self, msg):
        sfp = StringIO()
        oldfp = self._fp
        self._fp = Tee(oldfp, sfp)
        try:
            Generator._write_headers(self, msg)
        finally:
            self._fp = oldfp
        self._headertxt = sfp.getvalue()

    def header_text(self):
        return self._headertxt



def process(mlist, msg, msgdata):
    if msgdata.get('approved'):
        return
    # First do site hard coded header spam checks
    for header, regex in mm_cfg.KNOWN_SPAMMERS:
        cre = re.compile(regex, re.IGNORECASE)
        value = msg[header]
        if not value:
            continue
        mo = cre.search(value)
        if mo:
            # we've detected spam, so throw the message away
            raise SpamDetected
    # Now do header_filter_rules
    g = HeaderGenerator(StringIO())
    g.flatten(msg)
    headers = g.header_text()
    for patterns, action, empty in mlist.header_filter_rules:
        if action == mm_cfg.DEFER:
            continue
        for pattern in patterns.splitlines():
            if pattern.startswith('#'):
                continue
            if re.search(pattern, headers, re.IGNORECASE):
                if action == mm_cfg.DISCARD:
                    raise Errors.DiscardMessage
                if action == mm_cfg.REJECT:
                    raise Errors.RejectMessage(
                        _('Message rejected by filter rule match'))
                if action == mm_cfg.HOLD:
                    hold_for_approval(mlist, msg, msgdata, HeaderMatchHold)
                if action == mm_cfg.ACCEPT:
                    return
