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

"""Cleanse certain headers from all messages."""


def process(mlist, msg, msgdata):
    # Always remove this header from any outgoing messages.  Be sure to do
    # this after the information on the header is actually used, but before a
    # permanent record of the header is saved.
    del msg['approved']
    # Also remove this header since it can contain a password
    del msg['urgent']
    # We remove other headers from anonymous lists
    if mlist.anonymous_list:
        del msg['from']
        del msg['reply-to']
        del msg['sender']
        # Hotmail sets this one
        del msg['x-originating-email']
        msg['From'] = mlist.GetListEmail()
        msg['Reply-To'] = mlist.GetListEmail()
    # Some headers can be used to fish for membership
    del msg['return-receipt-to']
    del msg['disposition-notification-to']
    del msg['x-confirm-reading-to']
    # Pegasus mail uses this one... sigh
    del msg['x-pmrqc']
