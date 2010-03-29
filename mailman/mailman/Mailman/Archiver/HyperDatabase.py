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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

#
# site modules
#
import os
import marshal
import time
import errno

#
# package/project modules
#
import pipermail
from Mailman import LockFile

CACHESIZE = pipermail.CACHESIZE

try:
    import cPickle
    pickle = cPickle
except ImportError:
    import pickle

#
# we're using a python dict in place of
# of bsddb.btree database.  only defining
# the parts of the interface used by class HyperDatabase
# only one thing can access this at a time.
#
class DumbBTree:
    """Stores pickles of Article objects

    This dictionary-like object stores pickles of all the Article
    objects.  The object itself is stored using marshal.  It would be
    much simpler, and probably faster, to store the actual objects in
    the DumbBTree and pickle it.

    TBD: Also needs a more sensible name, like IteratableDictionary or
    SortedDictionary.
    """

    def __init__(self, path):
        self.current_index = 0
        self.path = path
        self.lockfile = LockFile.LockFile(self.path + ".lock")
        self.lock()
        self.__dirty = 0
        self.dict = {}
        self.sorted = []
        self.load()

    def __repr__(self):
        return "DumbBTree(%s)" % self.path

    def __sort(self, dirty=None):
        if self.__dirty == 1 or dirty:
            self.sorted = self.dict.keys()
            self.sorted.sort()
            self.__dirty = 0

    def lock(self):
        self.lockfile.lock()

    def unlock(self):
        try:
            self.lockfile.unlock()
        except LockFile.NotLockedError:
            pass

    def __delitem__(self, item):
        # if first hasn't been called, we can skip the sort
        if self.current_index == 0:
            del self.dict[item]
            self.__dirty = 1
            return
        try:
            ci = self.sorted[self.current_index]
        except IndexError:
            ci = None
        if ci == item:
            try:
                ci = self.sorted[self.current_index + 1]
            except IndexError:
                ci = None
        del self.dict[item]
        self.__sort(dirty=1)
        if ci is not None:
            self.current_index = self.sorted.index(ci)
        else:
            self.current_index = self.current_index + 1

    def clear(self):
        # bulk clearing much faster than deleting each item, esp. with the
        # implementation of __delitem__() above :(
        self.dict = {}

    def first(self):
        self.__sort() # guarantee that the list is sorted
        if not self.sorted:
            raise KeyError
        else:
            key = self.sorted[0]
            self.current_index = 1
            return key, self.dict[key]

    def last(self):
        if not self.sorted:
            raise KeyError
        else:
            key = self.sorted[-1]
            self.current_index = len(self.sorted) - 1
            return key, self.dict[key]

    def next(self):
        try:
            key = self.sorted[self.current_index]
        except IndexError:
            raise KeyError
        self.current_index = self.current_index + 1
        return key, self.dict[key]

    def has_key(self, key):
        return self.dict.has_key(key)

    def set_location(self, loc):
        if not self.dict.has_key(loc):
            raise KeyError
        self.current_index = self.sorted.index(loc)

    def __getitem__(self, item):
        return self.dict[item]

    def __setitem__(self, item, val):
        # if first hasn't been called, then we don't need to worry
        # about sorting again
        if self.current_index == 0:
            self.dict[item] = val
            self.__dirty = 1
            return
        try:
            current_item = self.sorted[self.current_index]
        except IndexError:
            current_item = item
        self.dict[item] = val
        self.__sort(dirty=1)
        self.current_index = self.sorted.index(current_item)

    def __len__(self):
        return len(self.sorted)

    def load(self):
        try:
            fp = open(self.path)
            try:
                self.dict = marshal.load(fp)
            finally:
                fp.close()
        except IOError, e:
            if e.errno <> errno.ENOENT: raise
            pass
        except EOFError:
            pass
        else:
            self.__sort(dirty=1)

    def close(self):
        omask = os.umask(007)
        try:
            fp = open(self.path, 'w')
        finally:
            os.umask(omask)
        fp.write(marshal.dumps(self.dict))
        fp.close()
        self.unlock()


# this is lifted straight out of pipermail with
# the bsddb.btree replaced with above class.
# didn't use inheritance because of all the
# __internal stuff that needs to be here -scott
#
class HyperDatabase(pipermail.Database):
    __super_addArticle = pipermail.Database.addArticle

    def __init__(self, basedir, mlist):
        self.__cache = {}
        self.__currentOpenArchive = None   # The currently open indices
        self._mlist = mlist
        self.basedir = os.path.expanduser(basedir)
        # Recently added articles, indexed only by message ID
        self.changed={}

    def firstdate(self, archive):
        self.__openIndices(archive)
        date = 'None'
        try:
            datekey, msgid = self.dateIndex.first()
            date = time.asctime(time.localtime(float(datekey[0])))
        except KeyError:
            pass
        return date

    def lastdate(self, archive):
        self.__openIndices(archive)
        date = 'None'
        try:
            datekey, msgid = self.dateIndex.last()
            date = time.asctime(time.localtime(float(datekey[0])))
        except KeyError:
            pass
        return date

    def numArticles(self, archive):
        self.__openIndices(archive)
        return len(self.dateIndex)

    def addArticle(self, archive, article, subject=None, author=None,
                   date=None):
        self.__openIndices(archive)
        self.__super_addArticle(archive, article, subject, author, date)

    def __openIndices(self, archive):
        if self.__currentOpenArchive == archive:
            return
        self.__closeIndices()
        arcdir = os.path.join(self.basedir, 'database')
        omask = os.umask(0)
        try:
            try:
                os.mkdir(arcdir, 02770)
            except OSError, e:
                if e.errno <> errno.EEXIST: raise
        finally:
            os.umask(omask)
        for i in ('date', 'author', 'subject', 'article', 'thread'):
            t = DumbBTree(os.path.join(arcdir, archive + '-' + i))
            setattr(self, i + 'Index', t)
        self.__currentOpenArchive = archive

    def __closeIndices(self):
        for i in ('date', 'author', 'subject', 'thread', 'article'):
            attr = i + 'Index'
            if hasattr(self, attr):
                index = getattr(self, attr)
                if i == 'article':
                    if not hasattr(self, 'archive_length'):
                        self.archive_length = {}
                    l = len(index)
                    self.archive_length[self.__currentOpenArchive] = l
                index.close()
                delattr(self, attr)
        self.__currentOpenArchive = None

    def close(self):
        self.__closeIndices()

    def hasArticle(self, archive, msgid):
        self.__openIndices(archive)
        return self.articleIndex.has_key(msgid)

    def setThreadKey(self, archive, key, msgid):
        self.__openIndices(archive)
        self.threadIndex[key]=msgid

    def getArticle(self, archive, msgid):
        self.__openIndices(archive)
        if not self.__cache.has_key(msgid):
            # get the pickled object out of the DumbBTree
            buf = self.articleIndex[msgid]
            article = self.__cache[msgid] = pickle.loads(buf)
            # For upgrading older archives
            article.setListIfUnset(self._mlist)
        else:
            article = self.__cache[msgid]
        return article

    def first(self, archive, index):
        self.__openIndices(archive)
        index = getattr(self, index + 'Index')
        try:
            key, msgid = index.first()
            return msgid
        except KeyError:
            return None

    def next(self, archive, index):
        self.__openIndices(archive)
        index = getattr(self, index + 'Index')
        try:
            key, msgid = index.next()
            return msgid
        except KeyError:
            return None

    def getOldestArticle(self, archive, subject):
        self.__openIndices(archive)
        subject = subject.lower()
        try:
            key, tempid=self.subjectIndex.set_location(subject)
            self.subjectIndex.next()
            [subject2, date]= key.split('\0')
            if subject!=subject2: return None
            return tempid
        except KeyError:
            return None

    def newArchive(self, archive):
        pass

    def clearIndex(self, archive, index):
        self.__openIndices(archive)
        if hasattr(self.threadIndex, 'clear'):
            self.threadIndex.clear()
            return
        finished=0
        try:
            key, msgid=self.threadIndex.first()
        except KeyError: finished=1
        while not finished:
            del self.threadIndex[key]
            try:
                key, msgid=self.threadIndex.next()
            except KeyError: finished=1
