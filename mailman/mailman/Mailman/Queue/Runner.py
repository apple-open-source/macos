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

"""Generic queue runner class.
"""

import time
import traceback
import weakref
from cStringIO import StringIO

from Mailman import mm_cfg
from Mailman import Utils
from Mailman import Errors
from Mailman import MailList
from Mailman import i18n

from Mailman.Queue.Switchboard import Switchboard
from Mailman.Logging.Syslog import syslog



class Runner:
    QDIR = None
    SLEEPTIME = mm_cfg.QRUNNER_SLEEP_TIME

    def __init__(self, slice=None, numslices=1):
        self._kids = {}
        # Create our own switchboard.  Don't use the switchboard cache because
        # we want to provide slice and numslice arguments.
        self._switchboard = Switchboard(self.QDIR, slice, numslices)
        # Create the shunt switchboard
        self._shunt = Switchboard(mm_cfg.SHUNTQUEUE_DIR)
        self._stop = 0

    def stop(self):
        self._stop = 1

    def run(self):
        # Start the main loop for this queue runner.
        try:
            try:
                while 1:
                    # Once through the loop that processes all the files in
                    # the queue directory.
                    filecnt = self._oneloop()
                    # Do the periodic work for the subclass.  BAW: this
                    # shouldn't be called here.  There should be one more
                    # _doperiodic() call at the end of the _oneloop() loop.
                    self._doperiodic()
                    # If the stop flag is set, we're done.
                    if self._stop:
                        break
                    # If there were no files to process, then we'll simply
                    # sleep for a little while and expect some to show up.
                    if not filecnt:
                        self._snooze()
            except KeyboardInterrupt:
                pass
        finally:
            # We've broken out of our main loop, so we want to reap all the
            # subprocesses we've created and do any other necessary cleanups.
            self._cleanup()

    def _oneloop(self):
        # First, list all the files in our queue directory.
        # Switchboard.files() is guaranteed to hand us the files in FIFO
        # order.  Return an integer count of the number of files that were
        # available for this qrunner to process.  A non-zero value tells run()
        # not to snooze for a while.
        files = self._switchboard.files()
        for filebase in files:
            # Ask the switchboard for the message and metadata objects
            # associated with this filebase.
            msg, msgdata = self._switchboard.dequeue(filebase)
            # It's possible one or both files got lost.  If so, just ignore
            # this filebase entry.  dequeue() will automatically unlink the
            # other file, but we should log an error message for diagnostics.
            if msg is None or msgdata is None:
                syslog('error', 'lost data files for filebase: %s', filebase)
            else:
                # Now that we've dequeued the message, we want to be
                # incredibly anal about making sure that no uncaught exception
                # could cause us to lose the message.  All runners that
                # implement _dispose() must guarantee that exceptions are
                # caught and dealt with properly.  Still, there may be a bug
                # in the infrastructure, and we do not want those to cause
                # messages to be lost.  Any uncaught exceptions will cause the
                # message to be stored in the shunt queue for human
                # intervention.
                try:
                    self._onefile(msg, msgdata)
                except Exception, e:
                    self._log(e)
                    syslog('error', 'SHUNTING: %s', filebase)
                    # Put a marker in the metadata for unshunting
                    msgdata['whichq'] = self._switchboard.whichq()
                    self._shunt.enqueue(msg, msgdata)
            # Other work we want to do each time through the loop
            Utils.reap(self._kids, once=1)
            self._doperiodic()
            if self._shortcircuit():
                break
        return len(files)

    def _onefile(self, msg, msgdata):
        # Do some common sanity checking on the message metadata.  It's got to
        # be destined for a particular mailing list.  This switchboard is used
        # to shunt off badly formatted messages.  We don't want to just trash
        # them because they may be fixable with human intervention.  Just get
        # them out of our site though.
        #
        # Find out which mailing list this message is destined for.
        listname = msgdata.get('listname')
        if not listname:
            listname = mm_cfg.MAILMAN_SITE_LIST
        mlist = self._open_list(listname)
        if not mlist:
            syslog('error',
                   'Dequeuing message destined for missing list: %s',
                   listname)
            self._shunt.enqueue(msg, msgdata)
            return
        # Now process this message, keeping track of any subprocesses that may
        # have been spawned.  We'll reap those later.
        #
        # We also want to set up the language context for this message.  The
        # context will be the preferred language for the user if a member of
        # the list, or the list's preferred language.  However, we must take
        # special care to reset the defaults, otherwise subsequent messages
        # may be translated incorrectly.  BAW: I'm not sure I like this
        # approach, but I can't think of anything better right now.
        otranslation = i18n.get_translation()
        sender = msg.get_sender()
        if mlist:
            lang = mlist.getMemberLanguage(sender)
        else:
            lang = mm_cfg.DEFAULT_SERVER_LANGUAGE
        i18n.set_language(lang)
        msgdata['lang'] = lang
        try:
            keepqueued = self._dispose(mlist, msg, msgdata)
        finally:
            i18n.set_translation(otranslation)
        # Keep tabs on any child processes that got spawned.
        kids = msgdata.get('_kids')
        if kids:
            self._kids.update(kids)
        if keepqueued:
            self._switchboard.enqueue(msg, msgdata)

    # Mapping of listnames to MailList instances as a weak value dictionary.
    _listcache = weakref.WeakValueDictionary()

    def _open_list(self, listname):
        # Cache the open list so that any use of the list within this process
        # uses the same object.  We use a WeakValueDictionary so that when the
        # list is no longer necessary, its memory is freed.
        mlist = self._listcache.get(listname)
        if not mlist:
            try:
                mlist = MailList.MailList(listname, lock=0)
            except Errors.MMListError, e:
                syslog('error', 'error opening list: %s\n%s', listname, e)
                return None
            else:
                self._listcache[listname] = mlist
        return mlist

    def _log(self, exc):
        syslog('error', 'Uncaught runner exception: %s', exc)
        s = StringIO()
        traceback.print_exc(file=s)
        syslog('error', s.getvalue())

    #
    # Subclasses can override these methods.
    #
    def _cleanup(self):
        """Clean up upon exit from the main processing loop.

        Called when the Runner's main loop is stopped, this should perform
        any necessary resource deallocation.  Its return value is irrelevant.
        """
        Utils.reap(self._kids)

    def _dispose(self, mlist, msg, msgdata):
        """Dispose of a single message destined for a mailing list.

        Called for each message that the Runner is responsible for, this is
        the primary overridable method for processing each message.
        Subclasses, must provide implementation for this method.

        mlist is the MailList instance this message is destined for.

        msg is the Message object representing the message.

        msgdata is a dictionary of message metadata.
        """
        raise NotImplementedError

    def _doperiodic(self):
        """Do some processing `every once in a while'.

        Called every once in a while both from the Runner's main loop, and
        from the Runner's hash slice processing loop.  You can do whatever
        special periodic processing you want here, and the return value is
        irrelevant.

        """
        pass

    def _snooze(self):
        """Sleep for a little while, because there was nothing to do.

        This is called from the Runner's main loop, but only when the last
        processing loop had no work to do (i.e. there were no messages in it's
        little slice of hash space).
        """
        if self.SLEEPTIME <= 0:
            return
        time.sleep(self.SLEEPTIME)

    def _shortcircuit(self):
        """Return a true value if the individual file processing loop should
        exit before it's finished processing each message in the current slice
        of hash space.  A false value tells _oneloop() to continue processing
        until the current snapshot of hash space is exhausted.

        You could, for example, implement a throttling algorithm here.
        """
        return self._stop
