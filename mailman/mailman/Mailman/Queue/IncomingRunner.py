# Copyright (C) 1998-2003 by the Free Software Foundation, Inc.
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

"""Incoming queue runner."""

# A typical Mailman list exposes nine aliases which point to seven different
# wrapped scripts.  E.g. for a list named `mylist', you'd have:
#
# mylist-bounces -> bounces (-admin is a deprecated alias)
# mylist-confirm -> confirm
# mylist-join    -> join    (-subscribe is an alias)
# mylist-leave   -> leave   (-unsubscribe is an alias)
# mylist-owner   -> owner
# mylist         -> post
# mylist-request -> request
#
# -request, -join, and -leave are a robot addresses; their sole purpose is to
# process emailed commands in a Majordomo-like fashion (although the latter
# two are hardcoded to subscription and unsubscription requests).  -bounces is
# the automated bounce processor, and all messages to list members have their
# return address set to -bounces.  If the bounce processor fails to extract a
# bouncing member address, it can optionally forward the message on to the
# list owners.
#
# -owner is for reaching a human operator with minimal list interaction
# (i.e. no bounce processing).  -confirm is another robot address which
# processes replies to VERP-like confirmation notices.
#
# So delivery flow of messages look like this:
#
# joerandom ---> mylist ---> list members
#    |                           |
#    |                           |[bounces]
#    |        mylist-bounces <---+ <-------------------------------+
#    |              |                                              |
#    |              +--->[internal bounce processing]              |
#    |              ^                |                             |
#    |              |                |    [bounce found]           |
#    |         [bounces *]           +--->[register and discard]   |
#    |              |                |                      |      |
#    |              |                |                      |[*]   |
#    |        [list owners]          |[no bounce found]     |      |
#    |              ^                |                      |      |
#    |              |                |                      |      |
#    +-------> mylist-owner <--------+                      |      |
#    |                                                      |      |
#    |           data/owner-bounces.mbox <--[site list] <---+      |
#    |                                                             |
#    +-------> mylist-join--+                                      |
#    |                      |                                      |
#    +------> mylist-leave--+                                      |
#    |                      |                                      |
#    |                      v                                      |
#    +-------> mylist-request                                      |
#    |              |                                              |
#    |              +---> [command processor]                      |
#    |                            |                                |
#    +-----> mylist-confirm ----> +---> joerandom                  |
#                                           |                      |
#                                           |[bounces]             |
#                                           +----------------------+
#
# A person can send an email to the list address (for posting), the -owner
# address (to reach the human operator), or the -confirm, -join, -leave, and
# -request mailbots.  Message to the list address are then forwarded on to the
# list membership, with bounces directed to the -bounces address.
#
# [*] Messages sent to the -owner address are forwarded on to the list
# owner/moderators.  All -owner destined messages have their bounces directed
# to the site list -bounces address, regardless of whether a human sent the
# message or the message was crafted internally.  The intention here is that
# the site owners want to be notified when one of their list owners' addresses
# starts bouncing (yes, the will be automated in a future release).
#
# Any messages to site owners has their bounces directed to a special
# "loop-killer" address, which just dumps the message into
# data/owners-bounces.mbox.
#
# Finally, message to any of the mailbots causes the requested action to be
# performed.  Results notifications are sent to the author of the message,
# which all bounces pointing back to the -bounces address.


import sys
import os
from cStringIO import StringIO

from Mailman import mm_cfg
from Mailman import Errors
from Mailman import LockFile
from Mailman.Queue.Runner import Runner
from Mailman.Logging.Syslog import syslog



class IncomingRunner(Runner):
    QDIR = mm_cfg.INQUEUE_DIR

    def _dispose(self, mlist, msg, msgdata):
        # Try to get the list lock.
        try:
            mlist.Lock(timeout=mm_cfg.LIST_LOCK_TIMEOUT)
        except LockFile.TimeOutError:
            # Oh well, try again later
            return 1
        # Process the message through a handler pipeline.  The handler
        # pipeline can actually come from one of three places: the message
        # metadata, the mlist, or the global pipeline.
        #
        # If a message was requeued due to an uncaught exception, its metadata
        # will contain the retry pipeline.  Use this above all else.
        # Otherwise, if the mlist has a `pipeline' attribute, it should be
        # used.  Final fallback is the global pipeline.
        try:
            pipeline = self._get_pipeline(mlist, msg, msgdata)
            msgdata['pipeline'] = pipeline
            more = self._dopipeline(mlist, msg, msgdata, pipeline)
            if not more:
                del msgdata['pipeline']
            mlist.Save()
            return more
        finally:
            mlist.Unlock()

    # Overridable
    def _get_pipeline(self, mlist, msg, msgdata):
        # We must return a copy of the list, otherwise, the first message that
        # flows through the pipeline will empty it out!
        return msgdata.get('pipeline',
                           getattr(mlist, 'pipeline',
                                   mm_cfg.GLOBAL_PIPELINE))[:]

    def _dopipeline(self, mlist, msg, msgdata, pipeline):
        while pipeline:
            handler = pipeline.pop(0)
            modname = 'Mailman.Handlers.' + handler
            __import__(modname)
            try:
                pid = os.getpid()
                sys.modules[modname].process(mlist, msg, msgdata)
                # Failsafe -- a child may have leaked through.
                if pid <> os.getpid():
                    syslog('error', 'child process leaked thru: %s', modname)
                    os._exit(1)
            except Errors.DiscardMessage:
                # Throw the message away; we need do nothing else with it.
                syslog('vette', 'Message discarded, msgid: %s',
                       msg.get('message-id', 'n/a'))
                return 0
            except Errors.HoldMessage:
                # Let the approval process take it from here.  The message no
                # longer needs to be queued.
                return 0
            except Errors.RejectMessage, e:
                mlist.BounceMessage(msg, msgdata, e)
                return 0
            except:
                # Push this pipeline module back on the stack, then re-raise
                # the exception.
                pipeline.insert(0, handler)
                raise
        # We've successfully completed handling of this message
        return 0
