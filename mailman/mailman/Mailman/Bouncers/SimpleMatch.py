# Copyright (C) 1998-2008 by the Free Software Foundation, Inc.
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

"""Recognizes simple heuristically delimited bounces."""

import re
import email.Iterators



def _c(pattern):
    return re.compile(pattern, re.IGNORECASE)

# This is a list of tuples of the form
#
#     (start cre, end cre, address cre)
#
# where `cre' means compiled regular expression, start is the line just before
# the bouncing address block, end is the line just after the bouncing address
# block, and address cre is the regexp that will recognize the addresses.  It
# must have a group called `addr' which will contain exactly and only the
# address that bounced.
PATTERNS = [
    # sdm.de
    (_c('here is your list of failed recipients'),
     _c('here is your returned mail'),
     _c(r'<(?P<addr>[^>]*)>')),
    # sz-sb.de, corridor.com, nfg.nl
    (_c('the following addresses had'),
     _c('transcript of session follows'),
     _c(r'<(?P<fulladdr>[^>]*)>|\(expanded from: <?(?P<addr>[^>)]*)>?\)')),
    # robanal.demon.co.uk
    (_c('this message was created automatically by mail delivery software'),
     _c('original message follows'),
     _c('rcpt to:\s*<(?P<addr>[^>]*)>')),
    # s1.com (InterScan E-Mail VirusWall NT ???)
    (_c('message from interscan e-mail viruswall nt'),
     _c('end of message'),
     _c('rcpt to:\s*<(?P<addr>[^>]*)>')),
    # Smail
    (_c('failed addresses follow:'),
     _c('message text follows:'),
     _c(r'\s*(?P<addr>\S+@\S+)')),
    # newmail.ru
    (_c('This is the machine generated message from mail service.'),
     _c('--- Below the next line is a copy of the message.'),
     _c('<(?P<addr>[^>]*)>')),
    # turbosport.com runs something called `MDaemon 3.5.2' ???
    (_c('The following addresses did NOT receive a copy of your message:'),
     _c('--- Session Transcript ---'),
     _c('[>]\s*(?P<addr>.*)$')),
    # usa.net
    (_c('Intended recipient:\s*(?P<addr>.*)$'),
     _c('--------RETURNED MAIL FOLLOWS--------'),
     _c('Intended recipient:\s*(?P<addr>.*)$')),
    # hotpop.com
    (_c('Undeliverable Address:\s*(?P<addr>.*)$'),
     _c('Original message attached'),
     _c('Undeliverable Address:\s*(?P<addr>.*)$')),
    # Another demon.co.uk format
    (_c('This message was created automatically by mail delivery'),
     _c('^---- START OF RETURNED MESSAGE ----'),
     _c("addressed to '(?P<addr>[^']*)'")),
    # Prodigy.net full mailbox
    (_c("User's mailbox is full:"),
     _c('Unable to deliver mail.'),
     _c("User's mailbox is full:\s*<(?P<addr>[^>]*)>")),
    # Microsoft SMTPSVC
    (_c('The email below could not be delivered to the following user:'),
     _c('Old message:'),
     _c('<(?P<addr>[^>]*)>')),
    # Yahoo on behalf of other domains like sbcglobal.net
    (_c('Unable to deliver message to the following address\(es\)\.'),
     _c('--- Original message follows\.'),
     _c('<(?P<addr>[^>]*)>:')),
    # googlemail.com
    (_c('Delivery to the following recipient(s)? failed'),
     _c('----- Original message -----'),
     _c('^\s*(?P<addr>[^\s@]+@[^\s@]+)\s*$')),
    # kundenserver.de, mxlogic.net
    (_c('A message that you( have)? sent could not be delivered'),
     _c('^---'),
     _c('<(?P<addr>[^>]*)>')),
    # another kundenserver.de
    (_c('A message that you( have)? sent could not be delivered'),
     _c('^---'),
     _c('^(?P<addr>[^\s@]+@[^\s@:]+):')),
    # thehartford.com and amenworld.com
    (_c('Del(i|e)very to the following recipient(s)? (failed|was aborted)'),
     # this one may or may not have the original message, but there's nothing
     # unique to stop on, so stop on the first line of at least 3 characters
     # that doesn't start with 'D' (to not stop immediately) and has no '@'.
     _c('^[^D][^@]{2,}$'),
     _c('^\s*(. )?(?P<addr>[^\s@]+@[^\s@]+)\s*$')),
    # and another thehartfod.com/hartfordlife.com
    (_c('^Your message\s*$'),
     _c('^because:'),
     _c('^\s*(?P<addr>[^\s@]+@[^\s@]+)\s*$')),
    # kviv.be (InterScan NT)
    (_c('^Unable to deliver message to'),
     _c(r'\*+\s+End of message\s+\*+'),
     _c('<(?P<addr>[^>]*)>')),
    # earthlink.net supported domains
    (_c('^Sorry, unable to deliver your message to'),
     _c('^A copy of the original message'),
     _c('\s*(?P<addr>[^\s@]+@[^\s@]+)\s+')),
    # ademe.fr
    (_c('^A message could not be delivered to:'),
     _c('^Subject:'),
     _c('^\s*(?P<addr>[^\s@]+@[^\s@]+)\s*$')),
    # andrew.ac.jp
    (_c('^Invalid final delivery userid:'),
     _c('^Original message follows.'),
     _c('\s*(?P<addr>[^\s@]+@[^\s@]+)\s*$')),
    # E500_SMTP_Mail_Service@lerctr.org and similar
    (_c('---- Failed Recipients ----'),
     _c(' Mail ----'),
     _c('<(?P<addr>[^>]*)>')),
    # cynergycom.net
    (_c('A message that you sent could not be delivered'),
     _c('^---'),
     _c('(?P<addr>[^\s@]+@[^\s@)]+)')),
    # LSMTP for Windows
    (_c('^--> Error description:\s*$'),
     _c('^Error-End:'),
     _c('^Error-for:\s+(?P<addr>[^\s@]+@[^\s@]+)')),
    # Qmail with a tri-language intro beginning in spanish
    (_c('Your message could not be delivered'),
     _c('^-'),
     _c('<(?P<addr>[^>]*)>:')),
    # socgen.com
    (_c('Your message could not be delivered to'),
     _c('^\s*$'),
     _c('(?P<addr>[^\s@]+@[^\s@]+)')),
    # dadoservice.it
    (_c('Your message has encountered delivery problems'),
     _c('Your message reads'),
     _c('addressed to\s*(?P<addr>[^\s@]+@[^\s@)]+)')),
    # gomaps.com
    (_c('Did not reach the following recipient'),
     _c('^\s*$'),
     _c('\s(?P<addr>[^\s@]+@[^\s@]+)')),
    # EYOU MTA SYSTEM
    (_c('This is the deliver program at'),
     _c('^-'),
     _c('^(?P<addr>[^\s@]+@[^\s@<>]+)')),
    # A non-standard qmail at ieo.it
    (_c('this is the email server at'),
     _c('^-'),
     _c('\s(?P<addr>[^\s@]+@[^\s@]+)[\s,]')),
    # pla.net.py (MDaemon.PRO ?)
    (_c('- no such user here'),
     _c('There is no user'),
     _c('^(?P<addr>[^\s@]+@[^\s@]+)\s')),
    # Next one goes here...
    ]



def process(msg, patterns=None):
    if patterns is None:
        patterns = PATTERNS
    # simple state machine
    #     0 = nothing seen yet
    #     1 = intro seen
    addrs = {}
    # MAS: This is a mess. The outer loop used to be over the message
    # so we only looped through the message once.  Looping through the
    # message for each set of patterns is obviously way more work, but
    # if we don't do it, problems arise because scre from the wrong
    # pattern set matches first and then acre doesn't match.  The
    # alternative is to split things into separate modules, but then
    # we process the message multiple times anyway.
    for scre, ecre, acre in patterns:
        state = 0
        for line in email.Iterators.body_line_iterator(msg):
            if state == 0:
                if scre.search(line):
                    state = 1
            if state == 1:
                mo = acre.search(line)
                if mo:
                    addr = mo.group('addr')
                    if addr:
                        addrs[addr.strip('<>')] = 1
                elif ecre.search(line):
                    break
        if addrs:
            break
    return addrs.keys()
