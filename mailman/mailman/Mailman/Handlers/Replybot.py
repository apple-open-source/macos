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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

"""Handler for auto-responses.
"""

import time

from Mailman import Utils
from Mailman import Message
from Mailman.i18n import _
from Mailman.SafeDict import SafeDict
from Mailman.Logging.Syslog import syslog



def process(mlist, msg, msgdata):
    # Normally, the replybot should get a shot at this message, but there are
    # some important short-circuits, mostly to suppress 'bot storms, at least
    # for well behaved email bots (there are other governors for misbehaving
    # 'bots).  First, if the original message has an "X-Ack: No" header, we
    # skip the replybot.  Then, if the message has a Precedence header with
    # values bulk, junk, or list, and there's no explicit "X-Ack: yes" header,
    # we short-circuit.  Finally, if the message metadata has a true 'noack'
    # key, then we skip the replybot too.
    ack = msg.get('x-ack', '').lower()
    if ack == 'no' or msgdata.get('noack'):
        return
    precedence = msg.get('precedence', '').lower()
    if ack <> 'yes' and precedence in ('bulk', 'junk', 'list'):
        return
    # Check to see if the list is even configured to autorespond to this email
    # message.  Note: the owner script sets the `toowner' key, and the various
    # confirm, join, leave, request, subscribe and unsubscribe scripts set the
    # keys we use for `torequest'.
    toadmin = msgdata.get('toowner')
    torequest = msgdata.get('torequest') or msgdata.get('toconfirm') or \
                    msgdata.get('tojoin') or msgdata.get('toleave')
    if ((toadmin and not mlist.autorespond_admin) or
           (torequest and not mlist.autorespond_requests) or \
           (not toadmin and not torequest and not mlist.autorespond_postings)):
        return
    # Now see if we're in the grace period for this sender.  graceperiod <= 0
    # means always autorespond, as does an "X-Ack: yes" header (useful for
    # debugging).
    sender = msg.get_sender()
    now = time.time()
    graceperiod = mlist.autoresponse_graceperiod
    if graceperiod > 0 and ack <> 'yes':
        if toadmin:
            quiet_until = mlist.admin_responses.get(sender, 0)
        elif torequest:
            quiet_until = mlist.request_responses.get(sender, 0)
        else:
            quiet_until = mlist.postings_responses.get(sender, 0)
        if quiet_until > now:
            return
    #
    # Okay, we know we're going to auto-respond to this sender, craft the
    # message, send it, and update the database.
    realname = mlist.real_name
    subject = _(
        'Auto-response for your message to the "%(realname)s" mailing list')
    # Do string interpolation
    d = SafeDict({'listname'    : realname,
                  'listurl'     : mlist.GetScriptURL('listinfo'),
                  'requestemail': mlist.GetRequestEmail(),
                  # BAW: Deprecate adminemail; it's not advertised but still
                  # supported for backwards compatibility.
                  'adminemail'  : mlist.GetBouncesEmail(),
                  'owneremail'  : mlist.GetOwnerEmail(),
                  })
    # Just because we're using a SafeDict doesn't mean we can't get all sorts
    # of other exceptions from the string interpolation.  Let's be ultra
    # conservative here.
    if toadmin:
        rtext = mlist.autoresponse_admin_text
    elif torequest:
        rtext = mlist.autoresponse_request_text
    else:
        rtext = mlist.autoresponse_postings_text
    # Using $-strings?
    if getattr(mlist, 'use_dollar_strings', 0):
        rtext = Utils.to_percent(rtext)
    try:
        text = rtext % d
    except Exception:
        syslog('error', 'Bad autoreply text for list: %s\n%s',
               mlist.internal_name(), rtext)
        text = rtext
    # Wrap the response.
    text = Utils.wrap(text)
    outmsg = Message.UserNotification(sender, mlist.GetBouncesEmail(),
                                      subject, text, mlist.preferred_language)
    outmsg['X-Mailer'] = _('The Mailman Replybot')
    # prevent recursions and mail loops!
    outmsg['X-Ack'] = 'No'
    outmsg.send(mlist)
    # update the grace period database
    if graceperiod > 0:
        # graceperiod is in days, we need # of seconds
        quiet_until = now + graceperiod * 24 * 60 * 60
        if toadmin:
            mlist.admin_responses[sender] = quiet_until
        elif torequest:
            mlist.request_responses[sender] = quiet_until
        else:
            mlist.postings_responses[sender] = quiet_until
