# Copyright (C) 2002 by the Free Software Foundation, Inc.
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

"""
    unsubscribe [password] [address=<address>]
        Unsubscribe from the mailing list.  If given, your password must match
        your current password.  If omitted, a confirmation email will be sent
        to the unsubscribing address. If you wish to unsubscribe an address
        other than the address you sent this request from, you may specify
        `address=<address>' (no brackets around the email address, and no
        quotes!)
"""

from email.Utils import parseaddr

from Mailman import Errors
from Mailman.i18n import _

STOP = 1



def gethelp(mlist):
    return _(__doc__)



def process(res, args):
    mlist = res.mlist
    password = None
    address = None
    argnum = 0
    for arg in args:
        if arg.startswith('address='):
            address = arg[8:]
        elif argnum == 0:
            password = arg
        else:
            res.results.append(_('Usage:'))
            res.results.append(gethelp(mlist))
            return STOP
        argnum += 1
    # Fill in empty defaults
    if address is None:
        realname, address = parseaddr(res.msg['from'])
    if not mlist.isMember(address):
        listname = mlist.real_name
        res.results.append(
            _('%(address)s is not a member of the %(listname)s mailing list'))
        return STOP
    # If we're doing admin-approved unsubs, don't worry about the password
    if mlist.unsubscribe_policy:
        try:
            mlist.DeleteMember(address, 'mailcmd')
        except Errors.MMNeedApproval:
            res.results.append(_("""\
Your unsubscription request has been forwarded to the list administrator for
approval."""))
    elif password is None:
        # No password was given, so we need to do a mailback confirmation
        # instead of unsubscribing them here.
        cpaddr = mlist.getMemberCPAddress(address)
        mlist.ConfirmUnsubscription(cpaddr)
        # We don't also need to send a confirmation to this command
        res.respond = 0
    else:
        # No admin approval is necessary, so we can just delete them if the
        # passwords match.
        oldpw = mlist.getMemberPassword(address)
        if oldpw <> password:
            res.results.append(_('You gave the wrong password'))
            return STOP
        mlist.ApprovedDeleteMember(address, 'mailcmd')
        res.results.append(_('Unsubscription request succeeded.'))
