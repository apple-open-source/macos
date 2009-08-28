# Copyright (C) 2002-2007 by the Free Software Foundation, Inc.
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

"""Maildir pre-queue runner.

Most MTAs can be configured to deliver messages to a `Maildir'[1].  This
runner will read messages from a maildir's new/ directory and inject them into
Mailman's qfiles/in directory for processing in the normal pipeline.  This
delivery mechanism contrasts with mail program delivery, where incoming
messages end up in qfiles/in via the MTA executing the scripts/post script
(and likewise for the other -aliases for each mailing list).

The advantage to Maildir delivery is that it is more efficient; there's no
need to fork an intervening program just to take the message from the MTA's
standard output, to the qfiles/in directory.

[1] http://cr.yp.to/proto/maildir.html

We're going to use the :info flag == 1, experimental status flag for our own
purposes.  The :1 can be followed by one of these letters:

- P means that MaildirRunner's in the process of parsing and enqueuing the
  message.  If successful, it will delete the file.

- X means something failed during the parse/enqueue phase.  An error message
  will be logged to log/error and the file will be renamed <filename>:1,X.
  MaildirRunner will never automatically return to this file, but once the
  problem is fixed, you can manually move the file back to the new/ directory
  and MaildirRunner will attempt to re-process it.  At some point we may do
  this automatically.

See the variable USE_MAILDIR in Defaults.py.in for enabling this delivery
mechanism.
"""

# NOTE: Maildir delivery is experimental in Mailman 2.1.

import os
import re
import errno

from email.Parser import Parser
from email.Utils import parseaddr

from Mailman import mm_cfg
from Mailman import Utils
from Mailman.Message import Message
from Mailman.Queue.Runner import Runner
from Mailman.Queue.sbcache import get_switchboard
from Mailman.Logging.Syslog import syslog

# We only care about the listname and the subq as in listname@ or
# listname-request@
lre = re.compile(r"""
 ^                        # start of string
 (?P<listname>[^+@]+?)    # listname@ or listname-subq@ (non-greedy)
 (?:                      # non-grouping
   -                      # dash separator
   (?P<subq>              # any known suffix
     admin|
     bounces|
     confirm|
     join|
     leave|
     owner|
     request|
     subscribe|
     unsubscribe
   )
 )?                       # if it exists
 [+@]                     # followed by + or @
 """, re.VERBOSE | re.IGNORECASE)



class MaildirRunner(Runner):
    # This class is much different than most runners because it pulls files
    # of a different format than what scripts/post and friends leaves.  The
    # files this runner reads are just single message files as dropped into
    # the directory by the MTA.  This runner will read the file, and enqueue
    # it in the expected qfiles directory for normal processing.
    def __init__(self, slice=None, numslices=1):
        # Don't call the base class constructor, but build enough of the
        # underlying attributes to use the base class's implementation.
        self._stop = 0
        self._dir = os.path.join(mm_cfg.MAILDIR_DIR, 'new')
        self._cur = os.path.join(mm_cfg.MAILDIR_DIR, 'cur')
        self._parser = Parser(Message)

    def _oneloop(self):
        # Refresh this each time through the list.  BAW: could be too
        # expensive.
        listnames = Utils.list_names()
        # Cruise through all the files currently in the new/ directory
        try:
            files = os.listdir(self._dir)
        except OSError, e:
            if e.errno <> errno.ENOENT: raise
            # Nothing's been delivered yet
            return 0
        for file in files:
            srcname = os.path.join(self._dir, file)
            dstname = os.path.join(self._cur, file + ':1,P')
            xdstname = os.path.join(self._cur, file + ':1,X')
            try:
                os.rename(srcname, dstname)
            except OSError, e:
                if e.errno == errno.ENOENT:
                    # Some other MaildirRunner beat us to it
                    continue
                syslog('error', 'Could not rename maildir file: %s', srcname)
                raise
            # Now open, read, parse, and enqueue this message
            try:
                fp = open(dstname)
                try:
                    msg = self._parser.parse(fp)
                finally:
                    fp.close()
                # Now we need to figure out which queue of which list this
                # message was destined for.  See verp_bounce() in
                # BounceRunner.py for why we do things this way.
                vals = []
                for header in ('delivered-to', 'envelope-to', 'apparently-to'):
                    vals.extend(msg.get_all(header, []))
                for field in vals:
                    to = parseaddr(field)[1]
                    if not to:
                        continue
                    mo = lre.match(to)
                    if not mo:
                        # This isn't an address we care about
                        continue
                    listname, subq = mo.group('listname', 'subq')
                    if listname in listnames:
                        break
                else:
                    # As far as we can tell, this message isn't destined for
                    # any list on the system.  What to do?
                    syslog('error', 'Message apparently not for any list: %s',
                           xdstname)
                    os.rename(dstname, xdstname)
                    continue
                # BAW: blech, hardcoded
                msgdata = {'listname': listname}
                # -admin is deprecated
                if subq in ('bounces', 'admin'):
                    queue = get_switchboard(mm_cfg.BOUNCEQUEUE_DIR)
                elif subq == 'confirm':
                    msgdata['toconfirm'] = 1
                    queue = get_switchboard(mm_cfg.CMDQUEUE_DIR)
                elif subq in ('join', 'subscribe'):
                    msgdata['tojoin'] = 1
                    queue = get_switchboard(mm_cfg.CMDQUEUE_DIR)
                elif subq in ('leave', 'unsubscribe'):
                    msgdata['toleave'] = 1
                    queue = get_switchboard(mm_cfg.CMDQUEUE_DIR)
                elif subq == 'owner':
                    msgdata.update({
                        'toowner': 1,
                        'envsender': Utils.get_site_email(extra='bounces'),
                        'pipeline': mm_cfg.OWNER_PIPELINE,
                        })
                    queue = get_switchboard(mm_cfg.INQUEUE_DIR)
                elif subq is None:
                    msgdata['tolist'] = 1
                    queue = get_switchboard(mm_cfg.INQUEUE_DIR)
                elif subq == 'request':
                    msgdata['torequest'] = 1
                    queue = get_switchboard(mm_cfg.CMDQUEUE_DIR)
                else:
                    syslog('error', 'Unknown sub-queue: %s', subq)
                    os.rename(dstname, xdstname)
                    continue
                queue.enqueue(msg, msgdata)
                os.unlink(dstname)
            except Exception, e:
                os.rename(dstname, xdstname)
                syslog('error', str(e))

    def _cleanup(self):
        pass
