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

"""Put an emergency hold on all messages otherwise approved.

No notices are sent to either the sender or the list owner for emergency
holds.  I think they'd be too obnoxious.
"""

from Mailman import Errors
from Mailman.i18n import _



class EmergencyHold(Errors.HoldMessage):
    reason = _('Emergency hold on all list traffic is in effect')
    rejection = _('Your message was deemed inappropriate by the moderator.')



def process(mlist, msg, msgdata):
    if mlist.emergency and not msgdata.get('adminapproved'):
        mlist.HoldMessage(msg, _(EmergencyHold.reason), msgdata)
        raise EmergencyHold
