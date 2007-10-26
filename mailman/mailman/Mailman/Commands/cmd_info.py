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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

"""
    info
        Get information about this mailing list.
"""

from Mailman.i18n import _

STOP = 1



def gethelp(mlist):
    return _(__doc__)



def process(res, args):
    mlist = res.mlist
    if args:
        res.results.append(gethelp(mlist))
        return STOP
    listname = mlist.real_name
    description = mlist.description or _('n/a')
    postaddr = mlist.GetListEmail()
    requestaddr = mlist.GetRequestEmail()
    owneraddr = mlist.GetOwnerEmail()
    listurl = mlist.GetScriptURL('listinfo', absolute=1)
    res.results.append(_('List name:    %(listname)s'))
    res.results.append(_('Description:  %(description)s'))
    res.results.append(_('Postings to:  %(postaddr)s'))
    res.results.append(_('List Helpbot: %(requestaddr)s'))
    res.results.append(_('List Owners:  %(owneraddr)s'))
    res.results.append(_('More information: %(listurl)s'))
