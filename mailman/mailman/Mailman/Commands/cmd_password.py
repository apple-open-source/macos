# Copyright (C) 2002-2004 by the Free Software Foundation, Inc.
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
    password [<oldpassword> <newpassword>] [address=<address>]
        Retrieve or change your password.  With no arguments, this returns
        your current password.  With arguments <oldpassword> and <newpassword>
        you can change your password.

        If you're posting from an address other than your membership address,
        specify your membership address with `address=<address>' (no brackets
        around the email address, and no quotes!).  Note that in this case the
        response is always sent to the subscribed address.
"""

from email.Utils import parseaddr

from Mailman import mm_cfg
from Mailman.i18n import _

STOP = 1



def gethelp(mlist):
    return _(__doc__)



def process(res, args):
    mlist = res.mlist
    address = None
    if not args:
        # They just want to get their existing password
        realname, address = parseaddr(res.msg['from'])
        if mlist.isMember(address):
            password = mlist.getMemberPassword(address)
            res.results.append(_('Your password is: %(password)s'))
            # Prohibit multiple password retrievals.
            return STOP
        else:
            listname = mlist.real_name
            res.results.append(
                _('You are not a member of the %(listname)s mailing list'))
            return STOP
    elif len(args) == 1 and args[0].startswith('address='):
        # They want their password, but they're posting from a different
        # address.  We /must/ return the password to the subscribed address.
        address = args[0][8:]
        res.returnaddr = address
        if mlist.isMember(address):
            password = mlist.getMemberPassword(address)
            res.results.append(_('Your password is: %(password)s'))
            # Prohibit multiple password retrievals.
            return STOP
        else:
            listname = mlist.real_name
            res.results.append(
                _('You are not a member of the %(listname)s mailing list'))
            return STOP
    elif len(args) == 2:
        # They are changing their password
        oldpasswd = args[0]
        newpasswd = args[1]
        realname, address = parseaddr(res.msg['from'])
        if mlist.isMember(address):
            if mlist.Authenticate((mm_cfg.AuthUser, mm_cfg.AuthListAdmin),
                                  oldpasswd, address):
                mlist.setMemberPassword(address, newpasswd)
                res.results.append(_('Password successfully changed.'))
            else:
                res.results.append(_("""\
You did not give the correct old password, so your password has not been
changed.  Use the no argument version of the password command to retrieve your
current password, then try again."""))
                res.results.append(_('\nUsage:'))
                res.results.append(gethelp(mlist))
                return STOP
        else:
            listname = mlist.real_name
            res.results.append(
                _('You are not a member of the %(listname)s mailing list'))
            return STOP
    elif len(args) == 3 and args[2].startswith('address='):
        # They want to change their password, and they're sending this from a
        # different address than what they're subscribed with.  Be sure the
        # response goes to the subscribed address.
        oldpasswd = args[0]
        newpasswd = args[1]
        address = args[2][8:]
        res.returnaddr = address
        if mlist.isMember(address):
            if mlist.Authenticate((mm_cfg.AuthUser, mm_cfg.AuthListAdmin),
                                  oldpasswd, address):
                mlist.setMemberPassword(address, newpasswd)
                res.results.append(_('Password successfully changed.'))
            else:
                res.results.append(_("""\
You did not give the correct old password, so your password has not been
changed.  Use the no argument version of the password command to retrieve your
current password, then try again."""))
                res.results.append(_('\nUsage:'))
                res.results.append(gethelp(mlist))
                return STOP
        else:
            listname = mlist.real_name
            res.results.append(
                _('You are not a member of the %(listname)s mailing list'))
            return STOP
