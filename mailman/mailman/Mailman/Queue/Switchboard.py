# Copyright (C) 2001-2003 by the Free Software Foundation, Inc.
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

"""Reading and writing message objects and message metadata.
"""

# enqueue() and dequeue() are not symmetric.  enqueue() takes a Message
# object.  dequeue() returns a email.Message object tree.
#
# Message metadata is represented internally as a Python dictionary.  Keys and
# values must be strings.  When written to a queue directory, the metadata is
# written into an externally represented format, as defined here.  Because
# components of the Mailman system may be written in something other than
# Python, the external interchange format should be chosen based on what those
# other components can read and write.
#
# Most efficient, and recommended if everything is Python, is Python marshal
# format.  Also supported by default is Berkeley db format (using the default
# bsddb module compiled into your Python executable -- usually Berkeley db
# 2), and rfc822 style plain text.  You can write your own if you have other
# needs.

import os
import time
import sha
import marshal
import errno
import cPickle

import email

from Mailman import mm_cfg
from Mailman import Utils
from Mailman import Message
from Mailman.Logging.Syslog import syslog

# 20 bytes of all bits set, maximum sha.digest() value
shamax = 0xffffffffffffffffffffffffffffffffffffffffL

try:
    True, False
except NameError:
    True = 1
    False = 0

# This flag causes messages to be written as pickles (when True) or text files
# (when False).  Pickles are more efficient because the message doesn't need
# to be re-parsed every time it's unqueued, but pickles are not human readable.
SAVE_MSGS_AS_PICKLES = True



class _Switchboard:
    def __init__(self, whichq, slice=None, numslices=1):
        self.__whichq = whichq
        # Create the directory if it doesn't yet exist.
        # FIXME
        omask = os.umask(0)                       # rwxrws---
        try:
            try:
                os.mkdir(self.__whichq, 0770)
            except OSError, e:
                if e.errno <> errno.EEXIST: raise
        finally:
            os.umask(omask)
        # Fast track for no slices
        self.__lower = None
        self.__upper = None
        # BAW: test performance and end-cases of this algorithm
        if numslices <> 1:
            self.__lower = ((shamax+1) * slice) / numslices
            self.__upper = (((shamax+1) * (slice+1)) / numslices) - 1

    def whichq(self):
        return self.__whichq

    def enqueue(self, _msg, _metadata={}, **_kws):
        # Calculate the SHA hexdigest of the message to get a unique base
        # filename.  We're also going to use the digest as a hash into the set
        # of parallel qrunner processes.
        data = _metadata.copy()
        data.update(_kws)
        listname = data.get('listname', '--nolist--')
        # Get some data for the input to the sha hash
        now = time.time()
        if SAVE_MSGS_AS_PICKLES and not data.get('_plaintext'):
            msgsave = cPickle.dumps(_msg, 1)
            ext = '.pck'
        else:
            msgsave = str(_msg)
            ext = '.msg'
        hashfood = msgsave + listname + `now`
        # Encode the current time into the file name for FIFO sorting in
        # files().  The file name consists of two parts separated by a `+':
        # the received time for this message (i.e. when it first showed up on
        # this system) and the sha hex digest.
        #rcvtime = data.setdefault('received_time', now)
        rcvtime = data.setdefault('received_time', now)
        filebase = `rcvtime` + '+' + sha.new(hashfood).hexdigest()
        # Figure out which queue files the message is to be written to.
        msgfile = os.path.join(self.__whichq, filebase + ext)
        dbfile = os.path.join(self.__whichq, filebase + '.db')
        # Always add the metadata schema version number
        data['version'] = mm_cfg.QFILE_SCHEMA_VERSION
        # Filter out volatile entries
        for k in data.keys():
            if k.startswith('_'):
                del data[k]
        # Now write the message text to one file and the metadata to another
        # file.  The metadata is always written second to avoid race
        # conditions with the various queue runners (which key off of the .db
        # filename).
        omask = os.umask(007)                     # -rw-rw----
        try:
            msgfp = open(msgfile, 'w')
        finally:
            os.umask(omask)
        msgfp.write(msgsave)
        msgfp.flush()
        os.fsync(msgfp.fileno())
        msgfp.close()
        # Now write the metadata using the appropriate external metadata
        # format.  We play rename-switcheroo here to further plug the race
        # condition holes.
        tmpfile = dbfile + '.tmp'
        self._ext_write(tmpfile, data)
        os.rename(tmpfile, dbfile)
        return filebase

    def dequeue(self, filebase):
        # Calculate the .db and .msg filenames from the given filebase.
        msgfile = os.path.join(self.__whichq, filebase + '.msg')
        pckfile = os.path.join(self.__whichq, filebase + '.pck')
        dbfile = os.path.join(self.__whichq, filebase + '.db')
        # Now we are going to read the message and metadata for the given
        # filebase.  We want to read things in this order: first, the metadata
        # file to find out whether the message is stored as a pickle or as
        # plain text.  Second, the actual message file.  However, we want to
        # first unlink the message file and then the .db file, because the
        # qrunner only cues off of the .db file
        msg = None
        try:
            data = self._ext_read(dbfile)
            os.unlink(dbfile)
        except EnvironmentError, e:
            if e.errno <> errno.ENOENT: raise
            data = {}
        # Between 2.1b4 and 2.1b5, the `rejection-notice' key in the metadata
        # was renamed to `rejection_notice', since dashes in the keys are not
        # supported in METAFMT_ASCII.
        if data.has_key('rejection-notice'):
            data['rejection_notice'] = data['rejection-notice']
            del data['rejection-notice']
        msgfp = None
        try:
            try:
                msgfp = open(pckfile)
                msg = cPickle.load(msgfp)
                os.unlink(pckfile)
            except EnvironmentError, e:
                if e.errno <> errno.ENOENT: raise
                msgfp = None
                try:
                    msgfp = open(msgfile)
                    msg = email.message_from_file(msgfp, Message.Message)
                    os.unlink(msgfile)
                except EnvironmentError, e:
                    if e.errno <> errno.ENOENT: raise
                except email.Errors.MessageParseError, e:
                    # This message was unparsable, most likely because its
                    # MIME encapsulation was broken.  For now, there's not
                    # much we can do about it.
                    syslog('error', 'message is unparsable: %s', filebase)
                    msgfp.close()
                    msgfp = None
                    if mm_cfg.QRUNNER_SAVE_BAD_MESSAGES:
                        # Cheapo way to ensure the directory exists w/ the
                        # proper permissions.
                        sb = Switchboard(mm_cfg.BADQUEUE_DIR)
                        os.rename(msgfile, os.path.join(
                            mm_cfg.BADQUEUE_DIR, filebase + '.txt'))
                    else:
                        os.unlink(msgfile)
                    msg = data = None
        finally:
            if msgfp:
                msgfp.close()
        return msg, data

    def files(self):
        times = {}
        lower = self.__lower
        upper = self.__upper
        for f in os.listdir(self.__whichq):
            # We only care about the file's base name (i.e. no extension).
            # Thus we'll ignore anything that doesn't end in .db.
            if not f.endswith('.db'):
                continue
            filebase = os.path.splitext(f)[0]
            when, digest = filebase.split('+')
            # Throw out any files which don't match our bitrange.  BAW: test
            # performance and end-cases of this algorithm.
            if not lower or (lower <= long(digest, 16) < upper):
                times[float(when)] = filebase
        # FIFO sort
        keys = times.keys()
        keys.sort()
        return [times[k] for k in keys]

    def _ext_write(self, tmpfile, data):
        raise NotImplementedError

    def _ext_read(self, dbfile):
        raise NotImplementedError



class MarshalSwitchboard(_Switchboard):
    """Python marshal format."""
    FLOAT_ATTRIBUTES = ['received_time']

    def _ext_write(self, filename, dict):
        omask = os.umask(007)                     # -rw-rw----
        try:
            fp = open(filename, 'w')
        finally:
            os.umask(omask)
        # Python's marshal, up to and including in Python 2.1, has a bug where
        # the full precision of floats was not stored.  We work around this
        # bug by hardcoding a list of float values we know about, repr()-izing
        # them ourselves, and doing the reverse conversion on _ext_read().
        for attr in self.FLOAT_ATTRIBUTES:
            # We use try/except because we expect a hitrate of nearly 100%
            try:
                fval = dict[attr]
            except KeyError:
                pass
            else:
                dict[attr] = repr(fval)
        marshal.dump(dict, fp)
        # Make damn sure that the data we just wrote gets flushed to disk
        fp.flush()
        if mm_cfg.SYNC_AFTER_WRITE:
            os.fsync(fp.fileno())
        fp.close()

    def _ext_read(self, filename):
        fp = open(filename)
        dict = marshal.load(fp)
        # Update from version 2 files
        if dict.get('version', 0) == 2:
            del dict['filebase']
        # Do the reverse conversion (repr -> float)
        for attr in self.FLOAT_ATTRIBUTES:
            try:
                sval = dict[attr]
            except KeyError:
                pass
            else:
                # Do a safe eval by setting up a restricted execution
                # environment.  This may not be strictly necessary since we
                # know they are floats, but it can't hurt.
                dict[attr] = eval(sval, {'__builtins__': {}})
        fp.close()
        return dict



class BSDDBSwitchboard(_Switchboard):
    """Native (i.e. compiled-in) Berkeley db format."""
    def _ext_write(self, filename, dict):
        import bsddb
        omask = os.umask(0)
        try:
            hashfile = bsddb.hashopen(filename, 'n', 0660)
        finally:
            os.umask(omask)
        # values must be strings
        for k, v in dict.items():
            hashfile[k] = marshal.dumps(v)
        hashfile.sync()
        hashfile.close()

    def _ext_read(self, filename):
        import bsddb
        dict = {}
        hashfile = bsddb.hashopen(filename, 'r')
        for k in hashfile.keys():
            dict[k] = marshal.loads(hashfile[k])
        hashfile.close()
        return dict



class ASCIISwitchboard(_Switchboard):
    """Human readable .db file format.

    key/value pairs are written as

        key = value

    as real Python code which can be execfile'd.
    """

    def _ext_write(self, filename, dict):
        omask = os.umask(007)                     # -rw-rw----
        try:
            fp = open(filename, 'w')
        finally:
            os.umask(omask)
        for k, v in dict.items():
            print >> fp, '%s = %s' % (k, repr(v))
        # Make damn sure that the data we just wrote gets flushed to disk
        fp.flush()
        if mm_cfg.SYNC_AFTER_WRITE:
            os.fsync(fp.fileno())
        fp.close()

    def _ext_read(self, filename):
        dict = {'__builtins__': {}}
        execfile(filename, dict)
        del dict['__builtins__']
        return dict



# Here are the various types of external file formats available.  The format
# chosen is given defined in the mm_cfg.py configuration file.
if mm_cfg.METADATA_FORMAT == mm_cfg.METAFMT_MARSHAL:
    Switchboard = MarshalSwitchboard
elif mm_cfg.METADATA_FORMAT == mm_cfg.METAFMT_BSDDB_NATIVE:
    Switchboard = BSDDBSwitchboard
elif mm_cfg.METADATA_FORMAT == mm_cfg.METAFMT_ASCII:
    Switchboard = ASCIISwitchboard
else:
    syslog('error', 'Undefined metadata format: %d (using marshals)',
           mm_cfg.METADATA_FORMAT)
    Switchboard = MarshalSwitchboard



# For bin/dumpdb
class DumperSwitchboard(Switchboard):
    def __init__(self):
        pass

    def read(self, filename):
        return self._ext_read(filename)
