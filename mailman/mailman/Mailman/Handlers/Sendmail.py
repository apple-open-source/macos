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

"""Deliver a message via command-line drop-off.

WARNING WARNING WARNING: This module is provided for example purposes only.
It should not be used in a production environment for reasons described
below.  Because of this, you must explicitly enable it with by editing the
code.  See the WARN section in the process() function.

This module delivers the message via the command line interface to the
sendmail program.  It should work for sendmail clones like Postfix.  It is
expected that sendmail handles final delivery, message queueing, etc.  The
recipient list is only trivially split so that the command line is less than
about 3k in size.

SECURITY WARNING: Because this module uses os.popen(), it goes through the
shell.  This module does not scan the arguments for potential exploits and so
it should be considered unsafe for production use.  For performance reasons,
it's not recommended either -- use the SMTPDirect delivery module instead,
even if you're using the sendmail MTA.

DUPLICATES WARNING: Using this module can cause duplicates to be delivered to
your membership, depending on your MTA!  E.g. It is known that if you're using
the sendmail MTA, and if a message contains a single dot on a line by itself,
your list members will receive many duplicates.
"""

import string
import os

from Mailman import mm_cfg
from Mailman import Errors
from Mailman.Logging.Syslog import syslog

MAX_CMDLINE = 3000



def process(mlist, msg, msgdata):
    """Process the message object for the given list.

    The message object is an instance of Mailman.Message and must be fully
    prepared for delivery (i.e. all the appropriate headers must be set).  The
    message object can have the following attributes:

    recips - the list of recipients for the message (required)

    This function processes the message by handing off the delivery of the
    message to a sendmail (or sendmail clone) program.  It can raise a
    SendmailHandlerError if an error status was returned by the sendmail
    program.
    
    """
    # WARN: If you've read the warnings above and /still/ insist on using this
    # module, you must comment out the following line.  I still recommend you
    # don't do this!
    assert 0, 'Use of the Sendmail.py delivery module is highly discouraged'
    recips = msgdata.get('recips')
    if not recips:
        # Nobody to deliver to!
        return
    # Use -f to set the envelope sender
    cmd = mm_cfg.SENDMAIL_CMD + ' -f ' + mlist.GetBouncesEmail() + ' '
    # make sure the command line is of a manageable size
    recipchunks = []
    currentchunk = []
    chunklen = 0
    for r in recips:
        currentchunk.append(r)
        chunklen = chunklen + len(r) + 1
        if chunklen > MAX_CMDLINE:
            recipchunks.append(string.join(currentchunk))
            currentchunk = []
            chunklen = 0
    # pick up the last one
    if chunklen:
        recipchunks.append(string.join(currentchunk))
    # get all the lines of the message, since we're going to do this over and
    # over again
    msgtext = str(msg)
    msglen = len(msgtext)
    # cycle through all chunks
    failedrecips = []
    for chunk in recipchunks:
        # TBD: SECURITY ALERT.  This invokes the shell!
        fp = os.popen(cmd + chunk, 'w')
        fp.write(msgtext)
        status = fp.close()
        if status:
            errcode = (status & 0xff00) >> 8
            syslog('post', 'post to %s from %s, size=%d, failure=%d',
                   mlist.internal_name(), msg.get_sender(),
                   msglen, errcode)
            # TBD: can we do better than this?  What if only one recipient out
            # of the entire chunk failed?
            failedrecips.append(chunk)
        # Log the successful post
        syslog('post', 'post to %s from %s, size=%d, success',
               mlist.internal_name(), msg.get_sender(), msglen)
    if failedrecips:
        msgdata['recips'] = failedrecips
        raise Errors.SomeRecipientsFailed
