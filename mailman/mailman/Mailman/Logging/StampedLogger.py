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

import os
import time

from Mailman.Logging.Logger import Logger



class StampedLogger(Logger):
    """Record messages in log files, including date stamp and optional label.

    If manual_reprime is on (off by default), then timestamp prefix will
    included only on first .write() and on any write immediately following a
    call to the .reprime() method.  This is useful for when StampedLogger is
    substituting for sys.stderr, where you'd like to see the grouping of
    multiple writes under a single timestamp (and there is often is one group,
    for uncaught exceptions where a script is bombing).

    In any case, the identifying prefix will only follow writes that start on
    a new line.

    Nofail (by default) says to fallback to sys.stderr if write fails to
    category file.  A message is emitted, but the IOError is caught.
    Initialize with nofail=0 if you want to handle the error in your code,
    instead.

    """
    def __init__(self, category, label=None, manual_reprime=0, nofail=1,
                 immediate=1):
	"""If specified, optional label is included after timestamp.
        Other options are passed to the Logger class initializer.
        """
	self.__label = label
        self.__manual_reprime = manual_reprime
        self.__primed = 1
        self.__bol = 1
	Logger.__init__(self, category, nofail, immediate)

    def reprime(self):
        """Reset so timestamp will be included with next write."""
        self.__primed = 1

    def write(self, msg):
        if not self.__bol:
            prefix = ""
        else:
            if not self.__manual_reprime or self.__primed:
                stamp = time.strftime("%b %d %H:%M:%S %Y ",
                                      time.localtime(time.time()))
                self.__primed = 0
            else:
                stamp = ""
            if self.__label is None:
                label = "(%d)" % os.getpid()
            else:
                label = "%s(%d):" % (self.__label, os.getpid())
            prefix = stamp + label
        Logger.write(self, "%s %s" % (prefix, msg))
        if msg and msg[-1] == '\n':
            self.__bol = 1
        else:
            self.__bol = 0

    def writelines(self, lines):
	first = 1
	for l in lines:
	    if first:
		self.write(l)
		first = 0
	    else:
		if l and l[0] not in [' ', '\t', '\n']:
		    Logger.write(self, ' ' + l)
		else:
		    Logger.write(self, l)
