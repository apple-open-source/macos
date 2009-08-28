# Copyright (C) 2002-2007 by the Free Software Foundation, Inc.
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

from email.Utils import parseaddr

from Mailman import mm_cfg
from Mailman import i18n

STOP = 1

def _(s): return s

PUBLICHELP = _("""
    who
        See the non-hidden members of this mailing list.
    who password
        See everyone who is on this mailing list. The password is the
        list's admin or moderator password.
""")

MEMBERSONLYHELP = _("""
    who password [address=<address>]
        See the non-hidden members of this mailing list.  The roster is
        limited to list members only, and you must supply your membership
        password to retrieve it.  If you're posting from an address other
        than your membership address, specify your membership address with
        `address=<address>' (no brackets around the email address, and no
        quotes!). If you provide the list's admin or moderator password,
        hidden members will be included.
""")

ADMINONLYHELP = _("""
    who password
        See everyone who is on this mailing list.  The roster is limited to
        list administrators and moderators only; you must supply the list
        admin or moderator password to retrieve the roster.
""")

_ = i18n._



def gethelp(mlist):
    if mlist.private_roster == 0:
        return _(PUBLICHELP)
    elif mlist.private_roster == 1:
        return _(MEMBERSONLYHELP)
    elif mlist.private_roster == 2:
        return _(ADMINONLYHELP)


def usage(res):
    res.results.append(_('Usage:'))
    res.results.append(gethelp(res.mlist))



def process(res, args):
    mlist = res.mlist
    address = None
    password = None
    ok = False
    full = False
    if mlist.private_roster == 0:
        # Public rosters
        if args:
            if len(args) == 1:
                if mlist.Authenticate((mm_cfg.AuthListModerator,
                                       mm_cfg.AuthListAdmin),
                                      args[0]):
                    full = True
                else:
                    usage(res)
                    return STOP
            else:
                usage(res)
                return STOP
        ok = True
    elif mlist.private_roster == 1:
        # List members only
        if len(args) == 1:
            password = args[0]
            realname, address = parseaddr(res.msg['from'])
        elif len(args) == 2 and args[1].startswith('address='):
            password = args[0]
            address = args[1][8:]
        else:
            usage(res)
            return STOP
        if mlist.isMember(address) and mlist.Authenticate(
            (mm_cfg.AuthUser,
             mm_cfg.AuthListModerator,
             mm_cfg.AuthListAdmin),
            password, address):
            # Then
            ok = True
        if mlist.Authenticate(
            (mm_cfg.AuthListModerator,
             mm_cfg.AuthListAdmin),
            password):
            # Then
            ok = full = True
    else:
        # Admin only
        if len(args) <> 1:
            usage(res)
            return STOP
        if mlist.Authenticate((mm_cfg.AuthListModerator,
                               mm_cfg.AuthListAdmin),
                              args[0]):
            ok = full = True
    if not ok:
        res.results.append(
            _('You are not allowed to retrieve the list membership.'))
        return STOP
    # It's okay for this person to see the list membership
    dmembers = mlist.getDigestMemberKeys()
    rmembers = mlist.getRegularMemberKeys()
    if not dmembers and not rmembers:
        res.results.append(_('This list has no members.'))
        return
    # Convenience function
    def addmembers(members):
        for member in members:
            if not full and mlist.getMemberOption(member,
                                           mm_cfg.ConcealSubscription):
                continue
            realname = mlist.getMemberName(member)
            if realname:
                res.results.append('    %s (%s)' % (member, realname))
            else:
                res.results.append('    %s' % member)
    if rmembers:
        res.results.append(_('Non-digest (regular) members:'))
        addmembers(rmembers)
    if dmembers:
        res.results.append(_('Digest members:'))
        addmembers(dmembers)
