#! @PYTHON@
#
# Copyright (C) 2000,2001,2002 by the Free Software Foundation, Inc.
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

"""Check the error logs and send any which have information in them.

If any log entries exist, a message is sent to the mailman owner address
and the logs are rotated.
"""

# GETTING STARTED
#
#  Run this program as root from cron, preferably at least daily.  Running
#  as root is optional, but will preserve the various modes and ownerships
#  of log files in "~mailman/logs".  If any entries are in "errors" or
#  "smtp-errors", they will be mailed to the mailman owner address.
#
#  Set COMPRESS_LOGFILES_WITH in mm_cfg.py to "gzip" to get rotated logfiles
#  to be compressed.
#
#  Hacked from some existing Mailman code by
#  Sean Reifschneider <jafo-mailman@tummy.com>
#  Please direct questions on this to the above address.
#

showLines = 100		#  lines of log messages to display before truncating

import sys, os, string, time, errno
import paths
from Mailman import mm_cfg, Utils
import fileinput, socket, time, stat

# Work around known problems with some RedHat cron daemons
import signal
signal.signal(signal.SIGCHLD, signal.SIG_DFL)


newLogfiles = []
text = []
text.append('Mailman Log Report')
text.append('Generated: %s' % time.ctime(time.time()))
text.append('Host: %s' % socket.gethostname())
text.append('')

logDate = time.strftime('%Y%m%d-%H%M%S', time.localtime(time.time()))
textSend = 0
for log in ( 'error', 'smtp-failures' ):
	fileName = os.path.join(mm_cfg.LOG_DIR, log)

	#  rotate file if it contains any data
	stats = os.stat(fileName)
	if stats[stat.ST_SIZE] < 1: continue
	fileNameNew = '%s.%s' % ( fileName, logDate )
	newLogfiles.append(fileNameNew)
	os.rename(fileName, fileNameNew)
	open(fileName, 'w')
	os.chmod(fileName, stat.S_IMODE(stats[stat.ST_MODE]))
	try: os.chown(fileName, stats[stat.ST_UID], stats[stat.ST_GID])
	except OSError: pass    #  permission denied, DOH!

	textSend = 1
	tmp = '#  FILE: %s  #' % fileNameNew
	text.append('#' * len(tmp))
	text.append(tmp)
	text.append('#' * len(tmp))
	text.append('')

	linesLeft = showLines	#  e-mail first linesLeft of log files
	for line in fileinput.input(fileNameNew):
		if linesLeft == 0:
			text.append('[... truncated ...]')
			break
		linesLeft = linesLeft - 1
		line = string.rstrip(line)
		text.append(line)
	text.append('')

#  send message if we've actually found anything
if textSend:
	text = string.join(text, '\n') + '\n'
	siteowner = Utils.get_site_email()
	Utils.SendTextToUser(
		'Mailman Log Report -- %s' % time.ctime(time.time()),
		text, siteowner, siteowner)

#  compress any log-files we made
if hasattr(mm_cfg, 'COMPRESS_LOGFILES_WITH') and mm_cfg.COMPRESS_LOGFILES_WITH:
	for file in newLogfiles:
		os.system(mm_cfg.COMPRESS_LOGFILES_WITH % file)
