# Copyright (C) 2002-2008 by the Free Software Foundation, Inc.
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

"""
    subscribe [password] [digest|nodigest] [address=<address>]
        Subscribe to this mailing list.  Your password must be given to
        unsubscribe or change your options, but if you omit the password, one
        will be generated for you.  You may be periodically reminded of your
        password.

        The next argument may be either: `nodigest' or `digest' (no quotes!).
        If you wish to subscribe an address other than the address you sent
        this request from, you may specify `address=<address>' (no brackets
        around the email address, and no quotes!)
"""

from email.Utils import parseaddr
from email.Header import decode_header, make_header

from Mailman import Utils
from Mailman import Errors
from Mailman.UserDesc import UserDesc
from Mailman.i18n import _

STOP = 1



def gethelp(mlist):
    return _(__doc__)



def process(res, args):
    mlist = res.mlist
    digest = None
    password = None
    address = None
    realname = None
    # Parse the args
    argnum = 0
    for arg in args:
        if arg.lower().startswith('address='):
            address = arg[8:]
        elif argnum == 0:
            password = arg
        elif argnum == 1:
            if arg.lower() not in ('digest', 'nodigest'):
                res.results.append(_('Bad digest specifier: %(arg)s'))
                return STOP
            if arg.lower() == 'digest':
                digest = 1
            else:
                digest = 0
        else:
            res.results.append(_('Usage:'))
            res.results.append(gethelp(mlist))
            return STOP
        argnum += 1
    # Fix the password/digest issue
    if (digest is None
            and password and password.lower() in ('digest', 'nodigest')):
        if password.lower() == 'digest':
            digest = 1
        else:
            digest = 0
        password = None
    # Fill in empty defaults
    if digest is None:
        digest = mlist.digest_is_default
    if password is None:
        password = Utils.MakeRandomPassword()
    if address is None:
        realname, address = parseaddr(res.msg['from'])
        if not address:
            # Fall back to the sender address
            address = res.msg.get_sender()
        if not address:
            res.results.append(_('No valid address found to subscribe'))
            return STOP
        # Watch for encoded names
        try:
            h = make_header(decode_header(realname))
            # BAW: in Python 2.2, use just unicode(h)
            realname = h.__unicode__()
        except UnicodeError:
            realname = u''
        # Coerce to byte string if uh contains only ascii
        try:
            realname = realname.encode('us-ascii')
        except UnicodeError:
            pass
    # Create the UserDesc record and do a non-approved subscription
    listowner = mlist.GetOwnerEmail()
    userdesc = UserDesc(address, realname, password, digest)
    remote = res.msg.get_sender()
    try:
        mlist.AddMember(userdesc, remote)
    except Errors.MembershipIsBanned:
        res.results.append(_("""\
The email address you supplied is banned from this mailing list.
If you think this restriction is erroneous, please contact the list
owners at %(listowner)s."""))
        return STOP
    except Errors.MMBadEmailError:
        res.results.append(_("""\
Mailman won't accept the given email address as a valid address.
(E.g. it must have an @ in it.)"""))
        return STOP
    except Errors.MMHostileAddress:
        res.results.append(_("""\
Your subscription is not allowed because
the email address you gave is insecure."""))
        return STOP
    except Errors.MMAlreadyAMember:
        res.results.append(_('You are already subscribed!'))
        return STOP
    except Errors.MMCantDigestError:
        res.results.append(
            _('No one can subscribe to the digest of this list!'))
        return STOP
    except Errors.MMMustDigestError:
        res.results.append(_('This list only supports digest subscriptions!'))
        return STOP
    except Errors.MMSubscribeNeedsConfirmation:
        # We don't need to respond /and/ send a confirmation message.
        res.respond = 0
    except Errors.MMNeedApproval:
        res.results.append(_("""\
Your subscription request has been forwarded to the list administrator
at %(listowner)s for review."""))
    else:
        # Everything is a-ok
        res.results.append(_('Subscription request succeeded.'))
