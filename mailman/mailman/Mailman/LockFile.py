# Copyright (C) 1998-2008 by the Free Software Foundation, Inc.
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

"""Portable, NFS-safe file locking with timeouts.

This code implements an NFS-safe file-based locking algorithm influenced by
the GNU/Linux open(2) manpage, under the description of the O_EXCL option.
From RH6.1:

        [...] O_EXCL is broken on NFS file systems, programs which rely on it
        for performing locking tasks will contain a race condition.  The
        solution for performing atomic file locking using a lockfile is to
        create a unique file on the same fs (e.g., incorporating hostname and
        pid), use link(2) to make a link to the lockfile.  If link() returns
        0, the lock is successful.  Otherwise, use stat(2) on the unique file
        to check if its link count has increased to 2, in which case the lock
        is also successful.

The assumption made here is that there will be no `outside interference',
e.g. no agent external to this code will have access to link() to the affected
lock files.

LockFile objects support lock-breaking so that you can't wedge a process
forever.  This is especially helpful in a web environment, but may not be
appropriate for all applications.

Locks have a `lifetime', which is the maximum length of time the process
expects to retain the lock.  It is important to pick a good number here
because other processes will not break an existing lock until the expected
lifetime has expired.  Too long and other processes will hang; too short and
you'll end up trampling on existing process locks -- and possibly corrupting
data.  In a distributed (NFS) environment, you also need to make sure that
your clocks are properly synchronized.

Locks can also log their state to a log file.  When running under Mailman, the
log file is placed in a Mailman-specific location, otherwise, the log file is
called `LockFile.log' and placed in the temp directory (calculated from
tempfile.mktemp()).

"""

# This code has undergone several revisions, with contributions from Barry
# Warsaw, Thomas Wouters, Harald Meland, and John Viega.  It should also work
# well outside of Mailman so it could be used for other Python projects
# requiring file locking.  See the __main__ section at the bottom of the file
# for unit testing.

import os
import socket
import time
import errno
import random
import traceback
from stat import ST_NLINK, ST_MTIME

# Units are floating-point seconds.
DEFAULT_LOCK_LIFETIME  = 15
# Allowable a bit of clock skew
CLOCK_SLOP = 10

try:
    True, False
except NameError:
    True = 1
    False = 0



# Figure out what logfile to use.  This is different depending on whether
# we're running in a Mailman context or not.
_logfile = None

def _get_logfile():
    global _logfile
    if _logfile is None:
        try:
            from Mailman.Logging.StampedLogger import StampedLogger
            _logfile = StampedLogger('locks')
        except ImportError:
            # not running inside Mailman
            import tempfile
            dir = os.path.split(tempfile.mktemp())[0]
            path = os.path.join(dir, 'LockFile.log')
            # open in line-buffered mode
            class SimpleUserFile:
                def __init__(self, path):
                    self.__fp = open(path, 'a', 1)
                    self.__prefix = '(%d) ' % os.getpid()
                def write(self, msg):
                    now = '%.3f' % time.time()
                    self.__fp.write(self.__prefix + now + ' ' + msg)
            _logfile = SimpleUserFile(path)
    return _logfile



# Exceptions that can be raised by this module
class LockError(Exception):
    """Base class for all exceptions in this module."""

class AlreadyLockedError(LockError):
    """An attempt is made to lock an already locked object."""

class NotLockedError(LockError):
    """An attempt is made to unlock an object that isn't locked."""

class TimeOutError(LockError):
    """The timeout interval elapsed before the lock succeeded."""



class LockFile:
    """A portable way to lock resources by way of the file system.

    This class supports the following methods:

    __init__(lockfile[, lifetime[, withlogging]]):
        Create the resource lock using lockfile as the global lock file.  Each
        process laying claim to this resource lock will create their own
        temporary lock files based on the path specified by lockfile.
        Optional lifetime is the number of seconds the process expects to hold
        the lock.  Optional withlogging, when true, turns on lockfile logging
        (see the module docstring for details).

    set_lifetime(lifetime):
        Set a new lock lifetime.  This takes affect the next time the file is
        locked, but does not refresh a locked file.

    get_lifetime():
        Return the lock's lifetime.

    refresh([newlifetime[, unconditionally]]):
        Refreshes the lifetime of a locked file.  Use this if you realize that
        you need to keep a resource locked longer than you thought.  With
        optional newlifetime, set the lock's lifetime.   Raises NotLockedError
        if the lock is not set, unless optional unconditionally flag is set to
        true.

    lock([timeout]):
        Acquire the lock.  This blocks until the lock is acquired unless
        optional timeout is greater than 0, in which case, a TimeOutError is
        raised when timeout number of seconds (or possibly more) expires
        without lock acquisition.  Raises AlreadyLockedError if the lock is
        already set.

    unlock([unconditionally]):
        Relinquishes the lock.  Raises a NotLockedError if the lock is not
        set, unless optional unconditionally is true.

    locked():
        Return true if the lock is set, otherwise false.  To avoid race
        conditions, this refreshes the lock (on set locks).

    """
    # BAW: We need to watch out for two lock objects in the same process
    # pointing to the same lock file.  Without this, if you lock lf1 and do
    # not lock lf2, lf2.locked() will still return true.  NOTE: this gimmick
    # probably does /not/ work in a multithreaded world, but we don't have to
    # worry about that, do we? <1 wink>.
    COUNTER = 0

    def __init__(self, lockfile,
                 lifetime=DEFAULT_LOCK_LIFETIME,
                 withlogging=False):
        """Create the resource lock using lockfile as the global lock file.

        Each process laying claim to this resource lock will create their own
        temporary lock files based on the path specified by lockfile.
        Optional lifetime is the number of seconds the process expects to hold
        the lock.  Optional withlogging, when true, turns on lockfile logging
        (see the module docstring for details).

        """
        self.__lockfile = lockfile
        self.__lifetime = lifetime
        # This works because we know we're single threaded
        self.__counter = LockFile.COUNTER
        LockFile.COUNTER += 1
        self.__tmpfname = '%s.%s.%d.%d' % (
            lockfile, socket.gethostname(), os.getpid(), self.__counter)
        self.__withlogging = withlogging
        self.__logprefix = os.path.split(self.__lockfile)[1]
        # For transferring ownership across a fork.
        self.__owned = True
	
    def __repr__(self):
        return '<LockFile %s: %s [%s: %ssec] pid=%s>' % (
            id(self), self.__lockfile,
            self.locked() and 'locked' or 'unlocked',
            self.__lifetime, os.getpid())

    def set_lifetime(self, lifetime):
        """Set a new lock lifetime.

        This takes affect the next time the file is locked, but does not
        refresh a locked file.
        """
        self.__lifetime = lifetime

    def get_lifetime(self):
        """Return the lock's lifetime."""
        return self.__lifetime

    def refresh(self, newlifetime=None, unconditionally=False):
        """Refreshes the lifetime of a locked file.

        Use this if you realize that you need to keep a resource locked longer
        than you thought.  With optional newlifetime, set the lock's lifetime.
        Raises NotLockedError if the lock is not set, unless optional
        unconditionally flag is set to true.
        """
        if newlifetime is not None:
            self.set_lifetime(newlifetime)
        # Do we have the lock?  As a side effect, this refreshes the lock!
        if not self.locked() and not unconditionally:
            raise NotLockedError, '%s: %s' % (repr(self), self.__read())

    def lock(self, timeout=0):
        """Acquire the lock.

        This blocks until the lock is acquired unless optional timeout is
        greater than 0, in which case, a TimeOutError is raised when timeout
        number of seconds (or possibly more) expires without lock acquisition.
        Raises AlreadyLockedError if the lock is already set.
        """
        if timeout:
            timeout_time = time.time() + timeout
        # Make sure my temp lockfile exists, and that its contents are
        # up-to-date (e.g. the temp file name, and the lock lifetime).
        self.__write()
        # TBD: This next call can fail with an EPERM.  I have no idea why, but
        # I'm nervous about wrapping this in a try/except.  It seems to be a
        # very rare occurence, only happens from cron, and (only?) on Solaris
        # 2.6.
        self.__touch()
        self.__writelog('laying claim')
        # for quieting the logging output
        loopcount = -1
        while True:
            loopcount += 1
            # Create the hard link and test for exactly 2 links to the file
            try:
                os.link(self.__tmpfname, self.__lockfile)
                # If we got here, we know we know we got the lock, and never
                # had it before, so we're done.  Just touch it again for the
                # fun of it.
                self.__writelog('got the lock')
                self.__touch()
                break
            except OSError, e:
                # The link failed for some reason, possibly because someone
                # else already has the lock (i.e. we got an EEXIST), or for
                # some other bizarre reason.
                if e.errno == errno.ENOENT:
                    # TBD: in some Linux environments, it is possible to get
                    # an ENOENT, which is truly strange, because this means
                    # that self.__tmpfname doesn't exist at the time of the
                    # os.link(), but self.__write() is supposed to guarantee
                    # that this happens!  I don't honestly know why this
                    # happens, but for now we just say we didn't acquire the
                    # lock, and try again next time.
                    pass
                elif e.errno <> errno.EEXIST:
                    # Something very bizarre happened.  Clean up our state and
                    # pass the error on up.
                    self.__writelog('unexpected link error: %s' % e,
                                    important=True)
                    os.unlink(self.__tmpfname)
                    raise
                elif self.__linkcount() <> 2:
                    # Somebody's messin' with us!  Log this, and try again
                    # later.  TBD: should we raise an exception?
                    self.__writelog('unexpected linkcount: %d' %
                                    self.__linkcount(), important=True)
                elif self.__read() == self.__tmpfname:
                    # It was us that already had the link.
                    self.__writelog('already locked')
                    raise AlreadyLockedError
                # otherwise, someone else has the lock
                pass
            # We did not acquire the lock, because someone else already has
            # it.  Have we timed out in our quest for the lock?
            if timeout and timeout_time < time.time():
                os.unlink(self.__tmpfname)
                self.__writelog('timed out')
                raise TimeOutError
            # Okay, we haven't timed out, but we didn't get the lock.  Let's
            # find if the lock lifetime has expired.
            if time.time() > self.__releasetime() + CLOCK_SLOP:
                # Yes, so break the lock.
                self.__break()
                self.__writelog('lifetime has expired, breaking',
                                important=True)
            # Okay, someone else has the lock, our claim hasn't timed out yet,
            # and the expected lock lifetime hasn't expired yet.  So let's
            # wait a while for the owner of the lock to give it up.
            elif not loopcount % 100:
                self.__writelog('waiting for claim')
            self.__sleep()

    def unlock(self, unconditionally=False):
        """Unlock the lock.

        If we don't already own the lock (either because of unbalanced unlock
        calls, or because the lock was stolen out from under us), raise a
        NotLockedError, unless optional `unconditionally' is true.
        """
        islocked = self.locked()
        if not islocked and not unconditionally:
            raise NotLockedError
        # If we owned the lock, remove the global file, relinquishing it.
        if islocked:
            try:
                os.unlink(self.__lockfile)
            except OSError, e:
                if e.errno <> errno.ENOENT: raise
        # Remove our tempfile
        try:
            os.unlink(self.__tmpfname)
        except OSError, e:
            if e.errno <> errno.ENOENT: raise
        self.__writelog('unlocked')

    def locked(self):
        """Return true if we own the lock, false if we do not.

        Checking the status of the lock resets the lock's lifetime, which
        helps avoid race conditions during the lock status test.
        """
        # Discourage breaking the lock for a while.
        try:
            self.__touch()
        except OSError, e:
            if e.errno == errno.EPERM:
                # We can't touch the file because we're not the owner.  I
                # don't see how we can own the lock if we're not the owner.
                return False
            else:
                raise
        # TBD: can the link count ever be > 2?
        if self.__linkcount() <> 2:
            return False
        return self.__read() == self.__tmpfname

    def finalize(self):
        self.unlock(unconditionally=True)

    def __del__(self):
        if self.__owned:
            self.finalize()

    # Use these only if you're transfering ownership to a child process across
    # a fork.  Use at your own risk, but it should be race-condition safe.
    # _transfer_to() is called in the parent, passing in the pid of the child.
    # _take_possession() is called in the child, and blocks until the parent
    # has transferred possession to the child.  _disown() is used to set the
    # __owned flag to false, and it is a disgusting wart necessary to make
    # forced lock acquisition work in mailmanctl. :(
    def _transfer_to(self, pid):
        # First touch it so it won't get broken while we're fiddling about.
        self.__touch()
        # Find out current claim's temp filename
        winner = self.__read()
        # Now twiddle ours to the given pid
        self.__tmpfname = '%s.%s.%d' % (
            self.__lockfile, socket.gethostname(), pid)
        # Create a hard link from the global lock file to the temp file.  This
        # actually does things in reverse order of normal operation because we
        # know that lockfile exists, and tmpfname better not!
        os.link(self.__lockfile, self.__tmpfname)
        # Now update the lock file to contain a reference to the new owner
        self.__write()
        # Toggle off our ownership of the file so we don't try to finalize it
        # in our __del__()
        self.__owned = False
        # Unlink the old winner, completing the transfer
        os.unlink(winner)
        # And do some sanity checks
        assert self.__linkcount() == 2
        assert self.locked()
        self.__writelog('transferred the lock')

    def _take_possession(self):
        self.__tmpfname = tmpfname = '%s.%s.%d' % (
            self.__lockfile, socket.gethostname(), os.getpid())
        # Wait until the linkcount is 2, indicating the parent has completed
        # the transfer.
        while self.__linkcount() <> 2 or self.__read() <> tmpfname:
            time.sleep(0.25)
        self.__writelog('took possession of the lock')

    def _disown(self):
        self.__owned = False

    #
    # Private interface
    #

    def __writelog(self, msg, important=0):
        if self.__withlogging or important:
            logf = _get_logfile()
            logf.write('%s %s\n' % (self.__logprefix, msg))
            traceback.print_stack(file=logf)

    def __write(self):
        # Make sure it's group writable
        oldmask = os.umask(002)
        try:
            fp = open(self.__tmpfname, 'w')
            fp.write(self.__tmpfname)
            fp.close()
        finally:
            os.umask(oldmask)

    def __read(self):
        try:
            fp = open(self.__lockfile)
            filename = fp.read()
            fp.close()
            return filename
        except EnvironmentError, e:
            if e.errno <> errno.ENOENT: raise
            return None

    def __touch(self, filename=None):
        t = time.time() + self.__lifetime
        try:
            # TBD: We probably don't need to modify atime, but this is easier.
            os.utime(filename or self.__tmpfname, (t, t))
        except OSError, e:
            if e.errno <> errno.ENOENT: raise

    def __releasetime(self):
        try:
            return os.stat(self.__lockfile)[ST_MTIME]
        except OSError, e:
            if e.errno <> errno.ENOENT: raise
            return -1

    def __linkcount(self):
        try:
            return os.stat(self.__lockfile)[ST_NLINK]
        except OSError, e:
            if e.errno <> errno.ENOENT: raise
            return -1

    def __break(self):
        # First, touch the global lock file.  This reduces but does not
        # eliminate the chance for a race condition during breaking.  Two
        # processes could both pass the test for lock expiry in lock() before
        # one of them gets to touch the global lockfile.  This shouldn't be
        # too bad because all they'll do in this function is wax the lock
        # files, not claim the lock, and we can be defensive for ENOENTs
        # here.
        #
        # Touching the lock could fail if the process breaking the lock and
        # the process that claimed the lock have different owners.  We could
        # solve this by set-uid'ing the CGI and mail wrappers, but I don't
        # think it's that big a problem.
        try:
            self.__touch(self.__lockfile)
        except OSError, e:
            if e.errno <> errno.EPERM: raise
        # Get the name of the old winner's temp file.
        winner = self.__read()
        # Remove the global lockfile, which actually breaks the lock.
        try:
            os.unlink(self.__lockfile)
        except OSError, e:
            if e.errno <> errno.ENOENT: raise
        # Try to remove the old winner's temp file, since we're assuming the
        # winner process has hung or died.  Don't worry too much if we can't
        # unlink their temp file -- this doesn't wreck the locking algorithm,
        # but will leave temp file turds laying around, a minor inconvenience.
        try:
            if winner:
                os.unlink(winner)
        except OSError, e:
            if e.errno <> errno.ENOENT: raise

    def __sleep(self):
        interval = random.random() * 2.0 + 0.01
        time.sleep(interval)



# Unit test framework
def _dochild():
    prefix = '[%d]' % os.getpid()
    # Create somewhere between 1 and 1000 locks
    lockfile = LockFile('/tmp/LockTest', withlogging=True, lifetime=120)
    # Use a lock lifetime of between 1 and 15 seconds.  Under normal
    # situations, Mailman's usage patterns (untested) shouldn't be much longer
    # than this.
    workinterval = 5 * random.random()
    hitwait = 20 * random.random()
    print prefix, 'workinterval:', workinterval
    islocked = False
    t0 = 0
    t1 = 0
    t2 = 0
    try:
        try:
            t0 = time.time()
            print prefix, 'acquiring...'
            lockfile.lock()
            print prefix, 'acquired...'
            islocked = True
        except TimeOutError:
            print prefix, 'timed out'
        else:
            t1 = time.time()
            print prefix, 'acquisition time:', t1-t0, 'seconds'
            time.sleep(workinterval)
    finally:
        if islocked:
            try:
                lockfile.unlock()
                t2 = time.time()
                print prefix, 'lock hold time:', t2-t1, 'seconds'
            except NotLockedError:
                print prefix, 'lock was broken'
    # wait for next web hit
    print prefix, 'webhit sleep:', hitwait
    time.sleep(hitwait)


def _seed():
    try:
        fp = open('/dev/random')
        d = fp.read(40)
        fp.close()
    except EnvironmentError, e:
        if e.errno <> errno.ENOENT:
            raise
        from Mailman.Utils import sha_new
        d = sha_new(`os.getpid()`+`time.time()`).hexdigest()
    random.seed(d)


def _onetest():
    loopcount = random.randint(1, 100)
    for i in range(loopcount):
        print 'Loop %d of %d' % (i+1, loopcount)
        pid = os.fork()
        if pid:
            # parent, wait for child to exit
            pid, status = os.waitpid(pid, 0)
        else:
            # child
            _seed()
            try:
                _dochild()
            except KeyboardInterrupt:
                pass
            os._exit(0)


def _reap(kids):
    if not kids:
        return
    pid, status = os.waitpid(-1, os.WNOHANG)
    if pid <> 0:
        del kids[pid]


def _test(numtests):
    kids = {}
    for i in range(numtests):
        pid = os.fork()
        if pid:
            # parent
            kids[pid] = pid
        else:
            # child
            _seed()
            try:
                _onetest()
            except KeyboardInterrupt:
                pass
            os._exit(0)
        # slightly randomize each kid's seed
    while kids:
        _reap(kids)


if __name__ == '__main__':
    import sys
    import random
    _test(int(sys.argv[1]))
