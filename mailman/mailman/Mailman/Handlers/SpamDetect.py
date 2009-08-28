# Copyright (C) 1998-2007 by the Free Software Foundation, Inc.
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
        for value in msg.get_all(header, []):
            mo = cre.search(value)
            if mo:
                # we've detected spam, so throw the message away
                raise SpamDetected
    # Now do header_filter_rules
    # TK: Collect headers in sub-parts because attachment filename
    # extension may be a clue to possible virus/spam.
    headers = ''
    for p in msg.walk():
        g = HeaderGenerator(StringIO())
        g.flatten(p)
        headers += g.header_text()
    # Now reshape headers (remove extra CR and connect multiline).
    headers = re.sub('\n+', '\n', headers)
    headers = re.sub('\n\s', ' ', headers)
    for patterns, action, empty in mlist.header_filter_rules:
        if action == mm_cfg.DEFER:
            continue
        for pattern in patterns.splitlines():
            if pattern.startswith('#'):
                continue
            # ignore 'empty' patterns
            if not pattern.strip():
                continue
            if re.search(pattern, headers, re.IGNORECASE|re.MULTILINE):
                if action == mm_cfg.DISCARD:
                    raise Errors.DiscardMessage
                if action == mm_cfg.REJECT:
                    if msgdata.get('toowner'):
                        # Don't send rejection notice if addressed to '-owner'
                        # because it may trigger a loop of notices if the
                        # sender address is forged.  We just discard it here.
                        raise Errors.DiscardMessage
                    raise Errors.RejectMessage(
                        _('Message rejected by filter rule match'))
                if action == mm_cfg.HOLD:
                    if msgdata.get('toowner'):
                        # Don't hold '-owner' addressed message.  We just
                        # pass it here but list-owner can set this to be
                        # discarded on the GUI if he wants.
                        return
                    hold_for_approval(mlist, msg, msgdata, HeaderMatchHold)
                if action == mm_cfg.ACCEPT:
                    return
