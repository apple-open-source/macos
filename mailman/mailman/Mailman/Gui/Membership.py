# Copyright (C) 2001,2002 by the Free Software Foundation, Inc.
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

"""MailList mixin class managing the membership pseudo-options.
"""

from Mailman.i18n import _



class Membership:
    def GetConfigCategory(self):
        return 'members', _('Membership&nbsp;Management')

    def GetConfigSubCategories(self, category):
        if category == 'members':
            return [('list',   _('Membership&nbsp;List')),
                    ('add',    _('Mass&nbsp;Subscription')),
                    ('remove', _('Mass&nbsp;Removal')),
                    ]
        return None
