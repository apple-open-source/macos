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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

"""Virgin message queue runner.

This qrunner handles messages that the Mailman system gives virgin birth to.
E.g. acknowledgement responses to user posts or Replybot messages.  They need
to go through some minimal processing before they can be sent out to the
recipient.
"""

from Mailman import mm_cfg
from Mailman.Queue.Runner import Runner
from Mailman.Queue.IncomingRunner import IncomingRunner



class VirginRunner(IncomingRunner):
    QDIR = mm_cfg.VIRGINQUEUE_DIR

    def _dispose(self, mlist, msg, msgdata):
        # We need to fasttrack this message through any handlers that touch
        # it.  E.g. especially CookHeaders.
        msgdata['_fasttrack'] = 1
        return IncomingRunner._dispose(self, mlist, msg, msgdata)

    def _get_pipeline(self, mlist, msg, msgdata):
        # It's okay to hardcode this, since it'll be the same for all
        # internally crafted messages.
        return ['CookHeaders', 'ToOutgoing']
