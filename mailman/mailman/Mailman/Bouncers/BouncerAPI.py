# Copyright (C) 1998-2009 by the Free Software Foundation, Inc.
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

"""Contains all the common functionality for msg bounce scanning API.

This module can also be used as the basis for a bounce detection testing
framework.  When run as a script, it expects two arguments, the listname and
the filename containing the bounce message.

"""

import sys

from Mailman.Logging.Syslog import syslog

# If a bounce detector returns Stop, that means to just discard the message.
# An example is warning messages for temporary delivery problems.  These
# shouldn't trigger a bounce notification, but we also don't want to send them
# on to the list administrator.
class _Stop:
    pass
Stop = _Stop()


BOUNCE_PIPELINE = [
    'DSN',
    'Qmail',
    'Postfix',
    'Yahoo',
    'Caiwireless',
    'Exchange',
    'Exim',
    'Netscape',
    'Compuserve',
    'Microsoft',
    'GroupWise',
    'SMTP32',
    'SimpleMatch',
    'SimpleWarning',
    'Yale',
    'LLNL',
    'AOL',
    ]



# msg must be a mimetools.Message
def ScanMessages(mlist, msg):
    for module in BOUNCE_PIPELINE:
        modname = 'Mailman.Bouncers.' + module
        __import__(modname)
        addrs = sys.modules[modname].process(msg)
        if addrs:
            # Return addrs even if it is Stop. BounceRunner needs this info.
            return addrs
    return []
