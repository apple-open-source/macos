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
    lists
        See a list of the public mailing lists on this GNU Mailman server.
"""

from Mailman import mm_cfg
from Mailman import Utils
from Mailman.MailList import MailList
from Mailman.i18n import _


STOP = 1



def gethelp(mlist):
    return _(__doc__)



def process(res, args):
    mlist = res.mlist
    if args:
        res.results.append(_('Usage:'))
        res.results.append(gethelp(mlist))
        return STOP
    hostname = mlist.host_name
    res.results.append(_('Public mailing lists at %(hostname)s:'))
    lists = Utils.list_names()
    lists.sort()
    i = 1
    for listname in lists:
        if listname == mlist.internal_name():
            xlist = mlist
        else:
            xlist = MailList(listname, lock=0)
        # We can mention this list if you already know about it
        if not xlist.advertised and xlist is not mlist:
            continue
        # Skip the list if it isn't in the same virtual domain.  BAW: should a
        # message to the site list include everything regardless of domain?
        if mm_cfg.VIRTUAL_HOST_OVERVIEW and \
               xlist.host_name <> mlist.host_name:
            continue
        realname = xlist.real_name
        description = xlist.description or _('n/a')
        requestaddr = xlist.GetRequestEmail()
        if i > 1:
            res.results.append('')
        res.results.append(_('%(i)3d. List name:   %(realname)s'))
        res.results.append(_('     Description: %(description)s'))
        res.results.append(_('     Requests to: %(requestaddr)s'))
        i += 1
