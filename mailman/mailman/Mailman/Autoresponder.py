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

"""MailList mixin class managing the autoresponder.
"""

from Mailman import mm_cfg
from Mailman.i18n import _



class Autoresponder:
    def InitVars(self):
        # configurable
        self.autorespond_postings = 0
        self.autorespond_admin = 0
        # this value can be
        #  0 - no autoresponse on the -request line
        #  1 - autorespond, but discard the original message
        #  2 - autorespond, and forward the message on to be processed
        self.autorespond_requests = 0
        self.autoresponse_postings_text = ''
        self.autoresponse_admin_text = ''
        self.autoresponse_request_text = ''
        self.autoresponse_graceperiod = 90 # days
        # non-configurable
        self.postings_responses = {}
        self.admin_responses = {}
        self.request_responses = {}

