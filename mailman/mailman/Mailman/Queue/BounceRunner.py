# Copyright (C) 2001-2008 by the Free Software Foundation, Inc.
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

"""Bounce queue runner."""

import os
import re
import time
import cPickle

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

try:
    True, False
except NameError:
    True = 1
    False = 0



class BounceMixin:
    def __init__(self):
        # Registering a bounce means acquiring the list lock, and it would be
        # too expensive to do this for each message.  Instead, each bounce
        # runner maintains an event log which is essentially a file with
        # multiple pickles.  Each bounce we receive gets appended to this file
        # as a 4-tuple record: (listname, addr, today, msg)
        #
        # today is itself a 3-tuple of (year, month, day)
        #
        # Every once in a while (see _doperiodic()), the bounce runner cracks
        # open the file, reads all the records and registers all the bounces.
        # Then it truncates the file and continues on.  We don't need to lock
        # the bounce event file because bounce qrunners are single threaded
        # and each creates a uniquely named file to contain the events.
        #
        # XXX When Python 2.3 is minimal require, we can use the new
        # tempfile.TemporaryFile() function.
        #
        # XXX We used to classify bounces to the site list as bounce events
        # for every list, but this caused severe problems.  Here's the
        # scenario: aperson@example.com is a member of 4 lists, and a list
        # owner of the foo list.  example.com has an aggressive spam filter
        # which rejects any message that is spam or contains spam as an
        # attachment.  Now, a spambot sends a piece of spam to the foo list,
        # but since that spambot is not a member, the list holds the message
        # for approval, and sends a notification to aperson@example.com as
        # list owner.  That notification contains a copy of the spam.  Now
        # example.com rejects the message, causing a bounce to be sent to the
        # site list's bounce address.  The bounce runner would then dutifully
        # register a bounce for all 4 lists that aperson@example.com was a
        # member of, and eventually that person would get disabled on all
        # their lists.  So now we ignore site list bounces.  Ce La Vie for
        # password reminder bounces.
        self._bounce_events_file = os.path.join(
            mm_cfg.DATA_DIR, 'bounce-events-%05d.pck' % os.getpid())
        self._bounce_events_fp = None
        self._bouncecnt = 0
        self._nextaction = time.time() + mm_cfg.REGISTER_BOUNCES_EVERY

    def _queue_bounces(self, listname, addrs, msg):
        today = time.localtime()[:3]
        if self._bounce_events_fp is None:
            omask = os.umask(006)
            try:
                self._bounce_events_fp = open(self._bounce_events_file, 'a+b')
            finally:
                os.umask(omask)
        for addr in addrs:
            cPickle.dump((listname, addr, today, msg),
                         self._bounce_events_fp, 1)
        self._bounce_events_fp.flush()
        os.fsync(self._bounce_events_fp.fileno())
        self._bouncecnt += len(addrs)

    def _register_bounces(self):
        syslog('bounce', '%s processing %s queued bounces',
               self, self._bouncecnt)
        # Read all the records from the bounce file, then unlink it.  Sort the
        # records by listname for more efficient processing.
        events = {}
        self._bounce_events_fp.seek(0)
        while True:
            try:
                listname, addr, day, msg = cPickle.load(self._bounce_events_fp)
            except ValueError, e:
                syslog('bounce', 'Error reading bounce events: %s', e)
            except EOFError:
                break
            events.setdefault(listname, []).append((addr, day, msg))
        # Now register all events sorted by list
        for listname in events.keys():
            mlist = self._open_list(listname)
            mlist.Lock()
            try:
                for addr, day, msg in events[listname]:
                    mlist.registerBounce(addr, msg, day=day)
                mlist.Save()
            finally:
                mlist.Unlock()
        # Reset and free all the cached memory
        self._bounce_events_fp.close()
        self._bounce_events_fp = None
        os.unlink(self._bounce_events_file)
        self._bouncecnt = 0

    def _cleanup(self):
        if self._bouncecnt > 0:
            self._register_bounces()

    def _doperiodic(self):
        now = time.time()
        if self._nextaction > now or self._bouncecnt == 0:
            return
        # Let's go ahead and register the bounces we've got stored up
        self._nextaction = now + mm_cfg.REGISTER_BOUNCES_EVERY
        self._register_bounces()

    def _probe_bounce(self, mlist, token):
        locked = mlist.Locked()
        if not locked:
            mlist.Lock()
        try:
            op, addr, bmsg = mlist.pend_confirm(token)
            info = mlist.getBounceInfo(addr)
            mlist.disableBouncingMember(addr, info, bmsg)
            # Only save the list if we're unlocking it
            if not locked:
                mlist.Save()
        finally:
            if not locked:
                mlist.Unlock()



class BounceRunner(Runner, BounceMixin):
    QDIR = mm_cfg.BOUNCEQUEUE_DIR

    def __init__(self, slice=None, numslices=1):
        Runner.__init__(self, slice, numslices)
        BounceMixin.__init__(self)

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
        #   we'll simply attempt to deliver the message to the site list
        #   owner.
        #   Note that this means that automated bounce processing doesn't work
        #   for the site list.  Because we can't reliably tell to what address
        #   a non-VERP'd bounce was originally sent, we have to treat all
        #   bounces sent to the site list as potential list owner bounces.
        # - the list owner could have set list-bounces (or list-admin) as the
        #   owner address.  That's really bad as it results in a loop of ever
        #   growing unrecognized bounce messages.  We detect this based on the
        #   fact that this message itself will be from the site bounces
        #   address.  We then send this to the site list owner instead.
        # Notices to list-owner have their envelope sender and From: set to
        # the site-bounces address.  Check if this is this a bounce for a
        # message to a list owner, coming to site-bounces, or a looping
        # message sent directly to the -bounces address.  We have to do these
        # cases separately, because sending to site-owner will reset the
        # envelope sender.
        # Is this a site list bounce?
        if (mlist.internal_name().lower() ==
                mm_cfg.MAILMAN_SITE_LIST.lower()):
            # Send it on to the site owners, but craft the envelope sender to
            # be the -loop detection address, so if /they/ bounce, we won't
            # get stuck in a bounce loop.
            outq.enqueue(msg, msgdata,
                         recips=mlist.owner,
                         envsender=Utils.get_site_email(extra='loop'),
                         nodecorate=1,
                         )
            return
        # Is this a possible looping message sent directly to a list-bounces
        # address other than the site list?
        # Check From: because unix_from might be VERP'd.
        # Also, check the From: that Message.OwnerNotification uses.
        if (msg.get('from') ==
                Utils.get_site_email(mlist.host_name, 'bounces')):
            # Just send it to the sitelist-owner address.  If that bounces
            # we'll handle it above.
            outq.enqueue(msg, msgdata,
                         recips=[Utils.get_site_email(extra='owner')],
                         envsender=Utils.get_site_email(extra='loop'),
                         nodecorate=1,
                         )
            return
        # List isn't doing bounce processing?
        if not mlist.bounce_processing:
            return
        # Try VERP detection first, since it's quick and easy
        addrs = verp_bounce(mlist, msg)
        if addrs:
            # We have an address, but check if the message is non-fatal.
            if BouncerAPI.ScanMessages(mlist, msg) is BouncerAPI.Stop:
                return
        else:
            # See if this was a probe message.
            token = verp_probe(mlist, msg)
            if token:
                self._probe_bounce(mlist, token)
                return
            # That didn't give us anything useful, so try the old fashion
            # bounce matching modules.
            addrs = BouncerAPI.ScanMessages(mlist, msg)
            if addrs is BouncerAPI.Stop:
                # This is a recognized, non-fatal notice. Ignore it.
                return
        # If that still didn't return us any useful addresses, then send it on
        # or discard it.
        if not addrs:
            syslog('bounce',
                   '%s: bounce message w/no discernable addresses: %s',
                   mlist.internal_name(),
                   msg.get('message-id', 'n/a'))
            maybe_forward(mlist, msg)
            return
        # BAW: It's possible that there are None's in the list of addresses,
        # although I'm unsure how that could happen.  Possibly ScanMessages()
        # can let None's sneak through.  In any event, this will kill them.
        addrs = filter(None, addrs)
        self._queue_bounces(mlist.internal_name(), addrs, msg)

    _doperiodic = BounceMixin._doperiodic

    def _cleanup(self):
        BounceMixin._cleanup(self)
        Runner._cleanup(self)



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



def verp_probe(mlist, msg):
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
        mo = re.search(mm_cfg.VERP_PROBE_REGEXP, to)
        if not mo:
            continue                          # no match of regexp
        try:
            if bmailbox <> mo.group('bounces'):
                continue                      # not a bounce to our list
            # Extract the token and see if there's an entry
            token = mo.group('token')
            data = mlist.pend_confirm(token, expunge=False)
            if data is not None:
                return token
        except IndexError:
            syslog(
                'error',
                "VERP_PROBE_REGEXP doesn't yield the right match groups: %s",
                mm_cfg.VERP_PROBE_REGEXP)
    return None



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
        syslog('bounce',
               '%s: forwarding unrecognized, message-id: %s',
               mlist.internal_name(),
               msg.get('message-id', 'n/a'))
    else:
        syslog('bounce',
               '%s: discarding unrecognized, message-id: %s',
               mlist.internal_name(),
               msg.get('message-id', 'n/a'))
