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

"""Bounce queue runner."""

import re
from email.MIMEText import MIMEText
from email.MIMEMessage import MIMEMessage
from email.Utils import parseaddr

from Mailman import mm_cfg
from Mailman import Utils
from Mailman import LockFile
from Mailman.Message import UserNotification
from Mailman.Bouncers import BouncerAPI
from Mailman.Queue.Runner import Runner
from Mailman.Queue.sbcache import get_switchboard
from Mailman.Logging.Syslog import syslog
from Mailman.i18n import _

COMMASPACE = ', '



class BounceRunner(Runner):
    QDIR = mm_cfg.BOUNCEQUEUE_DIR
    # We only do bounce processing once per minute.
    SLEEPTIME = mm_cfg.minutes(1)

    def _dispose(self, mlist, msg, msgdata):
        # Make sure we have the most up-to-date state
        mlist.Load()
        outq = get_switchboard(mm_cfg.OUTQUEUE_DIR)
        # There are a few possibilities here:
        #
        # - the message could have been VERP'd in which case, we know exactly
        #   who the message was destined for.  That make our job easy.
        # - the message could have been originally destined for a list owner,
        #   but a list owner address itself bounced.  That's bad, and for now
        #   we'll simply log the problem and attempt to deliver the message to
        #   the site owner.
        #
        # All messages to list-owner@vdom.ain have their envelope sender set
        # to site-owner@dom.ain (no virtual domain).  Is this a bounce for a
        # message to a list owner, coming to the site owner?
        if msg.get('to', '') == Utils.get_site_email(extra='-owner'):
            # Send it on to the site owners, but craft the envelope sender to
            # be the -loop detection address, so if /they/ bounce, we won't
            # get stuck in a bounce loop.
            outq.enqueue(msg, msgdata,
                         recips=[Utils.get_site_email()],
                         envsender=Utils.get_site_email(extra='loop'),
                         )
        # List isn't doing bounce processing?
        if not mlist.bounce_processing:
            return
        # Try VERP detection first, since it's quick and easy
        addrs = verp_bounce(mlist, msg)
        if not addrs:
            # That didn't give us anything useful, so try the old fashion
            # bounce matching modules
            addrs = BouncerAPI.ScanMessages(mlist, msg)
        # If that still didn't return us any useful addresses, then send it on
        # or discard it.
        if not addrs:
            syslog('bounce', 'bounce message w/no discernable addresses: %s',
                   msg.get('message-id'))
            maybe_forward(mlist, msg)
            return
        # BAW: It's possible that there are None's in the list of addresses,
        # although I'm unsure how that could happen.  Possibly ScanMessages()
        # can let None's sneak through.  In any event, this will kill them.
        addrs = filter(None, addrs)
        # Okay, we have some recognized addresses.  We now need to register
        # the bounces for each of these.  If the bounce came to the site list,
        # then we'll register the address on every list in the system, but
        # note: this could be VERY resource intensive!
        foundp = 0
        listname = mlist.internal_name()
        if listname == mm_cfg.MAILMAN_SITE_LIST:
            foundp = 1
            for listname in Utils.list_names():
                xlist = self._open_list(listname)
                xlist.Load()
                for addr in addrs:
                    if xlist.isMember(addr):
                        unlockp = 0
                        if not xlist.Locked():
                            try:
                                xlist.Lock(timeout=mm_cfg.LIST_LOCK_TIMEOUT)
                            except LockFile.TimeOutError:
                                # Oh well, forget aboutf this list
                                continue
                            unlockp = 1
                        try:
                            xlist.registerBounce(addr, msg)
                            foundp = 1
                            xlist.Save()
                        finally:
                            if unlockp:
                                xlist.Unlock()
        else:
            try:
                mlist.Lock(timeout=mm_cfg.LIST_LOCK_TIMEOUT)
            except LockFile.TimeOutError:
                # Try again later
                syslog('bounce', "%s: couldn't get list lock", listname)
                return 1
            else:
                try:
                    for addr in addrs:
                        if mlist.isMember(addr):
                            mlist.registerBounce(addr, msg)
                            foundp = 1
                    mlist.Save()
                finally:
                    mlist.Unlock()
        if not foundp:
            # It means an address was recognized but it wasn't an address
            # that's on any mailing list at this site.  BAW: don't forward
            # these, but do log it.
            syslog('bounce', 'bounce message with non-members of %s: %s',
                   listname, COMMASPACE.join(addrs))
            #maybe_forward(mlist, msg)



def verp_bounce(mlist, msg):
    bmailbox, bdomain = Utils.ParseEmail(mlist.GetBouncesEmail())
    # Sadly not every MTA bounces VERP messages correctly, or consistently.
    # Fall back to Delivered-To: (Postfix), Envelope-To: (Exim) and
    # Apparently-To:, and then short-circuit if we still don't have anything
    # to work with.  Note that there can be multiple Delivered-To: headers so
    # we need to search them all (and we don't worry about false positives for
    # forwarded email, because only one should match VERP_REGEXP).
    vals = []
    for header in ('to', 'delivered-to', 'envelope-to', 'apparently-to'):
        vals.extend(msg.get_all(header, []))
    for field in vals:
        to = parseaddr(field)[1]
        if not to:
            continue                          # empty header
        mo = re.search(mm_cfg.VERP_REGEXP, to)
        if not mo:
            continue                          # no match of regexp
        try:
            if bmailbox <> mo.group('bounces'):
                continue                      # not a bounce to our list
            # All is good
            addr = '%s@%s' % mo.group('mailbox', 'host')
        except IndexError:
            syslog('error',
                   "VERP_REGEXP doesn't yield the right match groups: %s",
                   mm_cfg.VERP_REGEXP)
            return []
        return [addr]



def maybe_forward(mlist, msg):
    # Does the list owner want to get non-matching bounce messages?
    # If not, simply discard it.
    if mlist.bounce_unrecognized_goes_to_list_owner:
        adminurl = mlist.GetScriptURL('admin', absolute=1) + '/bounce'
        mlist.ForwardMessage(msg,
                             text=_("""\
The attached message was received as a bounce, but either the bounce format
was not recognized, or no member addresses could be extracted from it.  This
mailing list has been configured to send all unrecognized bounce messages to
the list administrator(s).

For more information see:
%(adminurl)s

"""),
                             subject=_('Uncaught bounce notification'),
                             tomoderators=0)
        syslog('bounce', 'forwarding unrecognized, message-id: %s',
               msg.get('message-id', 'n/a'))
    else:
        syslog('bounce', 'discarding unrecognized, message-id: %s',
               msg.get('message-id', 'n/a'))
