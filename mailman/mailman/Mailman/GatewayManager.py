# Copyright (C) 1998,1999,2000,2001,2002 by the Free Software Foundation, Inc.
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

"""Mixin class for configuring Usenet gateway.

All the actual functionality is in Handlers/ToUsenet.py for the mail->news
gateway and cron/gate_news for the news->mail gateway.

"""

from Mailman import mm_cfg
from Mailman.i18n import _


class GatewayManager:
    def InitVars(self):
        # Configurable
        self.nntp_host = mm_cfg.DEFAULT_NNTP_HOST
        self.linked_newsgroup = ''
        self.gateway_to_news = 0
        self.gateway_to_mail = 0
        self.news_prefix_subject_too = 1
        # In patch #401270, this was called newsgroup_is_moderated, but the
        # semantics weren't quite the same.
        self.news_moderation = 0
