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

"""Track pending actions which require confirmation."""

import os
import time
import errno
import random
import cPickle

from Mailman import mm_cfg
from Mailman.Utils import sha_new

# Types of pending records
SUBSCRIPTION = 'S'
UNSUBSCRIPTION = 'U'
CHANGE_OF_ADDRESS = 'C'
HELD_MESSAGE = 'H'
RE_ENABLE = 'E'
PROBE_BOUNCE = 'P'

_ALLKEYS = (SUBSCRIPTION, UNSUBSCRIPTION,
            CHANGE_OF_ADDRESS, HELD_MESSAGE,
            RE_ENABLE, PROBE_BOUNCE,
            )

try:
    True, False
except NameError:
    True = 1
    False = 0


_missing = []



class Pending:
    def InitTempVars(self):
        self.__pendfile = os.path.join(self.fullpath(), 'pending.pck')

    def pend_new(self, op, *content, **kws):
        """Create a new entry in the pending database, returning cookie for it.
        """
        assert op in _ALLKEYS, 'op: %s' % op
        lifetime = kws.get('lifetime', mm_cfg.PENDING_REQUEST_LIFE)
        # We try the main loop several times. If we get a lock error somewhere
        # (for instance because someone broke the lock) we simply try again.
        assert self.Locked()
        # Load the database
        db = self.__load()
        # Calculate a unique cookie.  Algorithm vetted by the Timbot.  time()
        # has high resolution on Linux, clock() on Windows.  random gives us
        # about 45 bits in Python 2.2, 53 bits on Python 2.3.  The time and
        # clock values basically help obscure the random number generator, as
        # does the hash calculation.  The integral parts of the time values
        # are discarded because they're the most predictable bits.
        while True:
            now = time.time()
            x = random.random() + now % 1.0 + time.clock() % 1.0
            cookie = sha_new(repr(x)).hexdigest()
            # We'll never get a duplicate, but we'll be anal about checking
            # anyway.
            if not db.has_key(cookie):
                break
        # Store the content, plus the time in the future when this entry will
        # be evicted from the database, due to staleness.
        db[cookie] = (op,) + content
        evictions = db.setdefault('evictions', {})
        evictions[cookie] = now + lifetime
        self.__save(db)
        return cookie

    def __load(self):
        try:
            fp = open(self.__pendfile)
        except IOError, e:
            if e.errno <> errno.ENOENT: raise
            return {'evictions': {}}
        try:
            return cPickle.load(fp)
        finally:
            fp.close()

    def __save(self, db):
        evictions = db['evictions']
        now = time.time()
        for cookie, data in db.items():
            if cookie in ('evictions', 'version'):
                continue
            timestamp = evictions[cookie]
            if now > timestamp:
                # The entry is stale, so remove it.
                del db[cookie]
                del evictions[cookie]
        # Clean out any bogus eviction entries.
        for cookie in evictions.keys():
            if not db.has_key(cookie):
                del evictions[cookie]
        db['version'] = mm_cfg.PENDING_FILE_SCHEMA_VERSION
        tmpfile = '%s.tmp.%d.%d' % (self.__pendfile, os.getpid(), now)
        omask = os.umask(007)
        try:
            fp = open(tmpfile, 'w')
            try:
                cPickle.dump(db, fp)
                fp.flush()
                os.fsync(fp.fileno())
            finally:
                fp.close()
            os.rename(tmpfile, self.__pendfile)
        finally:
            os.umask(omask)

    def pend_confirm(self, cookie, expunge=True):
        """Return data for cookie, or None if not found.

        If optional expunge is True (the default), the record is also removed
        from the database.
        """
        db = self.__load()
        # If we're not expunging, the database is read-only.
        if not expunge:
            return db.get(cookie)
        # Since we're going to modify the database, we must make sure the list
        # is locked, since it's the list lock that protects pending.pck.
        assert self.Locked()
        content = db.get(cookie, _missing)
        if content is _missing:
            return None
        # Do the expunge
        del db[cookie]
        del db['evictions'][cookie]
        self.__save(db)
        return content

    def pend_repend(self, cookie, data, lifetime=mm_cfg.PENDING_REQUEST_LIFE):
        assert self.Locked()
        db = self.__load()
        db[cookie] = data
        db['evictions'][cookie] = time.time() + lifetime
        self.__save(db)



def _update(olddb):
    db = {}
    # We don't need this entry anymore
    if olddb.has_key('lastculltime'):
        del olddb['lastculltime']
    evictions = db.setdefault('evictions', {})
    for cookie, data in olddb.items():
        # The cookies used to be kept as a 6 digit integer.  We now keep the
        # cookies as a string (sha in our case, but it doesn't matter for
        # cookie matching).
        cookie = str(cookie)
        # The old format kept the content as a tuple and tacked the timestamp
        # on as the last element of the tuple.  We keep the timestamps
        # separate, but require the prepending of a record type indicator.  We
        # know that the only things that were kept in the old format were
        # subscription requests.  Also, the old request format didn't have the
        # subscription language.  Best we can do here is use the server
        # default.
        db[cookie] = (SUBSCRIPTION,) + data[:-1] + \
                     (mm_cfg.DEFAULT_SERVER_LANGUAGE,)
        # The old database format kept the timestamp as the time the request
        # was made.  The new format keeps it as the time the request should be
        # evicted.
        evictions[cookie] = data[-1] + mm_cfg.PENDING_REQUEST_LIFE
    return db
