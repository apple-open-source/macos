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

"""Calculate the regular (i.e. non-digest) recipients of the message.

This module calculates the non-digest recipients for the message based on the
list's membership and configuration options.  It places the list of recipients
on the `recips' attribute of the message.  This attribute is used by the
SendmailDeliver and BulkDeliver modules.
"""

import email.Utils
from Mailman import mm_cfg
from Mailman import Utils
from Mailman import Message
from Mailman import Errors
from Mailman.MemberAdaptor import ENABLED
from Mailman.MailList import MailList
from Mailman.i18n import _
from Mailman.Logging.Syslog import syslog
from Mailman.Errors import MMUnknownListError

# Use set for sibling list recipient calculation
try:
    set
except NameError: # Python2.3
    from sets import Set as set



def process(mlist, msg, msgdata):
    # Short circuit if we've already calculated the recipients list,
    # regardless of whether the list is empty or not.
    if msgdata.has_key('recips'):
        return
    # Should the original sender should be included in the recipients list?
    include_sender = 1
    sender = msg.get_sender()
    try:
        if mlist.getMemberOption(sender, mm_cfg.DontReceiveOwnPosts):
            include_sender = 0
    except Errors.NotAMemberError:
        pass
    # Support for urgent messages, which bypasses digests and disabled
    # delivery and forces an immediate delivery to all members Right Now.  We
    # are specifically /not/ allowing the site admins password to work here
    # because we want to discourage the practice of sending the site admin
    # password through email in the clear. (see also Approve.py)
    missing = []
    password = msg.get('urgent', missing)
    if password is not missing:
        if mlist.Authenticate((mm_cfg.AuthListModerator,
                               mm_cfg.AuthListAdmin),
                              password):
            recips = mlist.getMemberCPAddresses(mlist.getRegularMemberKeys() +
                                                mlist.getDigestMemberKeys())
            msgdata['recips'] = recips
            return
        else:
            # Bad Urgent: password, so reject it instead of passing it on.  I
            # think it's better that the sender know they screwed up than to
            # deliver it normally.
            realname = mlist.real_name
            text = _("""\
Your urgent message to the %(realname)s mailing list was not authorized for
delivery.  The original message as received by Mailman is attached.
""")
            raise Errors.RejectMessage, Utils.wrap(text)
    # Calculate the regular recipients of the message
    recips = [mlist.getMemberCPAddress(m)
              for m in mlist.getRegularMemberKeys()
              if mlist.getDeliveryStatus(m) == ENABLED]
    # Remove the sender if they don't want to receive their own posts
    if not include_sender:
        try:
            recips.remove(mlist.getMemberCPAddress(sender))
        except (Errors.NotAMemberError, ValueError):
            # Sender does not want to get copies of their own messages (not
            # metoo), but delivery to their address is disabled (nomail).  Or
            # the sender is not a member of the mailing list.
            pass
    # Handle topic classifications
    do_topic_filters(mlist, msg, msgdata, recips)
    # Regular delivery exclude/include (if in/not_in To: or Cc:) lists
    recips = do_exclude(mlist, msg, msgdata, recips)
    recips = do_include(mlist, msg, msgdata, recips)
    # Bookkeeping
    msgdata['recips'] = recips



def do_topic_filters(mlist, msg, msgdata, recips):
    if not mlist.topics_enabled:
        # MAS: if topics are currently disabled for the list, send to all
        # regardless of ReceiveNonmatchingTopics
        return
    hits = msgdata.get('topichits')
    zaprecips = []
    if hits:
        # The message hit some topics, so only deliver this message to those
        # who are interested in one of the hit topics.
        for user in recips:
            utopics = mlist.getMemberTopics(user)
            if not utopics:
                # This user is not interested in any topics, so they get all
                # postings.
                continue
            # BAW: Slow, first-match, set intersection!
            for topic in utopics:
                if topic in hits:
                    # The user wants this message
                    break
            else:
                # The user was interested in topics, but not any of the ones
                # this message matched, so zap him.
                zaprecips.append(user)
    else:
        # The semantics for a message that did not hit any of the pre-canned
        # topics is to troll through the membership list, looking for users
        # who selected at least one topic of interest, but turned on
        # ReceiveNonmatchingTopics.
        for user in recips:
            if not mlist.getMemberTopics(user):
                # The user did not select any topics of interest, so he gets
                # this message by default.
                continue
            if not mlist.getMemberOption(user,
                                         mm_cfg.ReceiveNonmatchingTopics):
                # The user has interest in some topics, but elects not to
                # receive message that match no topics, so zap him.
                zaprecips.append(user)
            # Otherwise, the user wants non-matching messages.
    # Prune out the non-receiving users
    for user in zaprecips:
        recips.remove(user)


def do_exclude(mlist, msg, msgdata, recips):
    # regular_exclude_lists are the other mailing lists on this mailman
    # installation whose members are excluded from the regular (non-digest)
    # delivery of this list if those list addresses appear in To: or Cc:
    # headers.
    if not mlist.regular_exclude_lists:
        return recips
    recips = set(recips)
    destinations = email.Utils.getaddresses(msg.get_all('to', []) +
                                            msg.get_all('cc', []))
    destinations = [y.lower() for x,y in destinations]
    for listname in mlist.regular_exclude_lists:
        listname = listname.lower()
        if listname not in destinations:
            continue
        listlhs, hostname = listname.split('@')
        if listlhs == mlist.internal_name():
            syslog('error', 'Exclude list %s is a self reference.',
                    listname)
            continue
        try:
            slist = MailList(listlhs, lock=False)
        except MMUnknownListError:
            syslog('error', 'Exclude list %s not found.', listname)
            continue
        if not mm_cfg.ALLOW_CROSS_DOMAIN_SIBLING \
           and slist.host_name != hostname:
            syslog('error', 'Exclude list %s is not in the same domain.',
                    listname)
            continue
        srecips = set([slist.getMemberCPAddress(m)
                   for m in slist.getRegularMemberKeys()
                   if slist.getDeliveryStatus(m) == ENABLED])
        recips -= srecips
    return list(recips)


def do_include(mlist, msg, msgdata, recips):
    # regular_include_lists are the other mailing lists on this mailman
    # installation whose members are included in the regular (non-digest)
    # delivery if those list addresses don't appear in To: or Cc: headers.
    if not mlist.regular_include_lists:
        return recips
    recips = set(recips)
    destinations = email.Utils.getaddresses(msg.get_all('to', []) +
                                            msg.get_all('cc', []))
    destinations = [y.lower() for x,y in destinations]
    for listname in mlist.regular_include_lists:
        listname = listname.lower()
        if listname in destinations:
            continue
        listlhs, hostname = listname.split('@')
        if listlhs == mlist.internal_name():
            syslog('error', 'Include list %s is a self reference.',
                    listname)
            continue
        try:
            slist = MailList(listlhs, lock=False)
        except MMUnknownListError:
            syslog('error', 'Include list %s not found.', listname)
            continue
        if not mm_cfg.ALLOW_CROSS_DOMAIN_SIBLING \
           and slist.host_name != hostname:
            syslog('error', 'Include list %s is not in the same domain.',
                    listname)
            continue
        srecips = set([slist.getMemberCPAddress(m)
                   for m in slist.getRegularMemberKeys()
                   if slist.getDeliveryStatus(m) == ENABLED])
        recips |= srecips
    return list(recips)
