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

import sys
import traceback


def _logexc(logger=None, msg=''):
    sys.__stderr__.write('Logging error: %s\n' % logger)
    traceback.print_exc(file=sys.__stderr__)
    sys.__stderr__.write('Original log message:\n%s\n' % msg)


def LogStdErr(category, label, manual_reprime=1, tee_to_real_stderr=1):
    """Establish a StampedLogger on sys.stderr if possible.

    If tee_to_real_stderr is true, then the real standard error also gets
    output, via a MultiLogger.

    Returns the MultiLogger if successful, None otherwise.
    """
    from StampedLogger import StampedLogger
    from MultiLogger import MultiLogger
    try:
        logger = StampedLogger(category,
                               label=label,
                               manual_reprime=manual_reprime,
                               nofail=0)
        if tee_to_real_stderr:
            if hasattr(sys, '__stderr__'):
                stderr = sys.__stderr__
            else:
                stderr = sys.stderr
            logger = MultiLogger(stderr, logger)
        sys.stderr = logger
        return sys.stderr
    except IOError:
        return None

