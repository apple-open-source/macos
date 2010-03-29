# Copyright (C) 1998-2009 by the Free Software Foundation, Inc.
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

"""Cleanse certain headers from all messages."""

import re

from email.Utils import formataddr

from Mailman.Utils import unique_message_id
from Mailman.Logging.Syslog import syslog
from Mailman.Handlers.CookHeaders import uheader


def process(mlist, msg, msgdata):
    # Always remove this header from any outgoing messages.  Be sure to do
    # this after the information on the header is actually used, but before a
    # permanent record of the header is saved.
    del msg['approved']
    # Remove this one too.
    del msg['approve']
    # Also remove this header since it can contain a password
    del msg['urgent']
    # We remove other headers from anonymous lists
    if mlist.anonymous_list:
        syslog('post', 'post to %s from %s anonymized',
               mlist.internal_name(), msg.get('from'))
        del msg['from']
        del msg['reply-to']
        del msg['sender']
        del msg['return-path']
        # Hotmail sets this one
        del msg['x-originating-email']
        # And these can reveal the sender too
        del msg['received']
        # And so can the message-id so replace it.
        del msg['message-id']
        msg['Message-ID'] = unique_message_id(mlist)
        i18ndesc = str(uheader(mlist, mlist.description, 'From'))
        msg['From'] = formataddr((i18ndesc, mlist.GetListEmail()))
        msg['Reply-To'] = mlist.GetListEmail()
        uf = msg.get_unixfrom()
        if uf:
            uf = re.sub(r'\S*@\S*', mlist.GetListEmail(), uf)
            msg.set_unixfrom(uf)
    # Some headers can be used to fish for membership
    del msg['return-receipt-to']
    del msg['disposition-notification-to']
    del msg['x-confirm-reading-to']
    # Pegasus mail uses this one... sigh
    del msg['x-pmrqc']
