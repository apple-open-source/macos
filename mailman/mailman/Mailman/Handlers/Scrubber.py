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

"""Cleanse a message for archiving.
"""

from __future__ import nested_scopes

import os
import re
import sha
import time
import errno
import binascii
import tempfile
from cStringIO import StringIO
from types import IntType

from email.Utils import parsedate
from email.Parser import HeaderParser
from email.Generator import Generator

from Mailman import mm_cfg
from Mailman import Utils
from Mailman import LockFile
from Mailman import Message
from Mailman.Errors import DiscardMessage
from Mailman.i18n import _
from Mailman.Logging.Syslog import syslog

# Path characters for common platforms
pre = re.compile(r'[/\\:]')
# All other characters to strip out of Content-Disposition: filenames
# (essentially anything that isn't an alphanum, dot, slash, or underscore.
sre = re.compile(r'[^-\w.]')
# Regexp to strip out leading dots
dre = re.compile(r'^\.*')

BR = '<br>\n'
SPACE = ' '

try:
    True, False
except NameError:
    True = 1
    False = 0


try:
    from mimetypes import guess_all_extensions
except ImportError:
    import mimetypes
    def guess_all_extensions(ctype, strict=True):
        # BAW: sigh, guess_all_extensions() is new in Python 2.3
        all = []
        def check(map):
            for e, t in map.items():
                if t == ctype:
                    all.append(e)
        check(mimetypes.types_map)
        # Python 2.1 doesn't have common_types.  Sigh, sigh.
        if not strict and hasattr(mimetypes, 'common_types'):
            check(mimetypes.common_types)
        return all



def guess_extension(ctype, ext):
    # mimetypes maps multiple extensions to the same type, e.g. .doc, .dot,
    # and .wiz are all mapped to application/msword.  This sucks for finding
    # the best reverse mapping.  If the extension is one of the giving
    # mappings, we'll trust that, otherwise we'll just guess. :/
    all = guess_all_extensions(ctype, strict=False)
    if ext in all:
        return ext
    return all and all[0]



# We're using a subclass of the standard Generator because we want to suppress
# headers in the subparts of multiparts.  We use a hack -- the ctor argument
# skipheaders to accomplish this.  It's set to true for the outer Message
# object, but false for all internal objects.  We recognize that
# sub-Generators will get created passing only mangle_from_ and maxheaderlen
# to the ctors.
#
# This isn't perfect because we still get stuff like the multipart boundaries,
# but see below for how we corrupt that to our nefarious goals.
class ScrubberGenerator(Generator):
    def __init__(self, outfp, mangle_from_=True,
                 maxheaderlen=78, skipheaders=True):
        Generator.__init__(self, outfp, mangle_from_=False)
        self.__skipheaders = skipheaders

    def _write_headers(self, msg):
        if not self.__skipheaders:
            Generator._write_headers(self, msg)


def safe_strftime(fmt, floatsecs):
    try:
        return time.strftime(fmt, floatsecs)
    except (TypeError, ValueError):
        return None


def calculate_attachments_dir(mlist, msg, msgdata):
    # Calculate the directory that attachments for this message will go
    # under.  To avoid inode limitations, the scheme will be:
    # archives/private/<listname>/attachments/YYYYMMDD/<msgid-hash>/<files>
    # Start by calculating the date-based and msgid-hash components.
    fmt = '%Y%m%d'
    datestr = msg.get('Date')
    if datestr:
        now = parsedate(datestr)
    else:
        now = time.gmtime(msgdata.get('received_time', time.time()))
    datedir = safe_strftime(fmt, now)
    if not datedir:
        datestr = msgdata.get('X-List-Received-Date')
        if datestr:
            datedir = safe_strftime(fmt, datestr)
    if not datedir:
        # What next?  Unixfrom, I guess.
        parts = msg.get_unixfrom().split()
        try:
            month = {'Jan':1, 'Feb':2, 'Mar':3, 'Apr':4, 'May':5, 'Jun':6,
                     'Jul':7, 'Aug':8, 'Sep':9, 'Oct':10, 'Nov':11, 'Dec':12,
                     }.get(parts[3], 0)
            day = int(parts[4])
            year = int(parts[6])
        except (IndexError, ValueError):
            # Best we can do I think
            month = day = year = 0
        datedir = '%04d%02d%02d' % (year, month, day)
    assert datedir
    # As for the msgid hash, we'll base this part on the Message-ID: so that
    # all attachments for the same message end up in the same directory (we'll
    # uniquify the filenames in that directory as needed).  We use the first 2
    # and last 2 bytes of the SHA1 hash of the message id as the basis of the
    # directory name.  Clashes here don't really matter too much, and that
    # still gives us a 32-bit space to work with.
    msgid = msg['message-id']
    if msgid is None:
        msgid = msg['Message-ID'] = Utils.unique_message_id(mlist)
    # We assume that the message id actually /is/ unique!
    digest = sha.new(msgid).hexdigest()
    return os.path.join('attachments', datedir, digest[:4] + digest[-4:])



def process(mlist, msg, msgdata=None):
    sanitize = mm_cfg.ARCHIVE_HTML_SANITIZER
    outer = True
    if msgdata is None:
        msgdata = {}
    dir = calculate_attachments_dir(mlist, msg, msgdata)
    charset = None
    lcset = Utils.GetCharSet(mlist.preferred_language)
    # Now walk over all subparts of this message and scrub out various types
    for part in msg.walk():
        ctype = part.get_type(part.get_default_type())
        # If the part is text/plain, we leave it alone
        if ctype == 'text/plain':
            # We need to choose a charset for the scrubbed message, so we'll
            # arbitrarily pick the charset of the first text/plain part in the
            # message.
            if charset is None:
                charset = part.get_content_charset(lcset)
        elif ctype == 'text/html' and isinstance(sanitize, IntType):
            if sanitize == 0:
                if outer:
                    raise DiscardMessage
                del part['content-type']
                part.set_payload(_('HTML attachment scrubbed and removed'),
                                 # Adding charset arg and removing content-tpe
                                 # sets content-type to text/plain
                                 lcset)
            elif sanitize == 2:
                # By leaving it alone, Pipermail will automatically escape it
                pass
            elif sanitize == 3:
                # Pull it out as an attachment but leave it unescaped.  This
                # is dangerous, but perhaps useful for heavily moderated
                # lists.
                omask = os.umask(002)
                try:
                    url = save_attachment(mlist, part, dir, filter_html=False)
                finally:
                    os.umask(omask)
                del part['content-type']
                part.set_payload(_("""\
An HTML attachment was scrubbed...
URL: %(url)s
"""), lcset)
            else:
                # HTML-escape it and store it as an attachment, but make it
                # look a /little/ bit prettier. :(
                payload = Utils.websafe(part.get_payload(decode=True))
                # For whitespace in the margin, change spaces into
                # non-breaking spaces, and tabs into 8 of those.  Then use a
                # mono-space font.  Still looks hideous to me, but then I'd
                # just as soon discard them.
                def doreplace(s):
                    return s.replace(' ', '&nbsp;').replace('\t', '&nbsp'*8)
                lines = [doreplace(s) for s in payload.split('\n')]
                payload = '<tt>\n' + BR.join(lines) + '\n</tt>\n'
                part.set_payload(payload)
                # We're replacing the payload with the decoded payload so this
                # will just get in the way.
                del part['content-transfer-encoding']
                omask = os.umask(002)
                try:
                    url = save_attachment(mlist, part, dir, filter_html=False)
                finally:
                    os.umask(omask)
                del part['content-type']
                part.set_payload(_("""\
An HTML attachment was scrubbed...
URL: %(url)s
"""), lcset)
        elif ctype == 'message/rfc822':
            # This part contains a submessage, so it too needs scrubbing
            submsg = part.get_payload(0)
            omask = os.umask(002)
            try:
                url = save_attachment(mlist, part, dir)
            finally:
                os.umask(omask)
            subject = submsg.get('subject', _('no subject'))
            date = submsg.get('date', _('no date'))
            who = submsg.get('from', _('unknown sender'))
            size = len(str(submsg))
            del part['content-type']
            part.set_payload(_("""\
An embedded message was scrubbed...
From: %(who)s
Subject: %(subject)s
Date: %(date)s
Size: %(size)s
Url: %(url)s
"""), lcset)
        # If the message isn't a multipart, then we'll strip it out as an
        # attachment that would have to be separately downloaded.  Pipermail
        # will transform the url into a hyperlink.
        elif not part.is_multipart():
            payload = part.get_payload(decode=True)
            ctype = part.get_type()
            size = len(payload)
            omask = os.umask(002)
            try:
                url = save_attachment(mlist, part, dir)
            finally:
                os.umask(omask)
            desc = part.get('content-description', _('not available'))
            filename = part.get_filename(_('not available'))
            del part['content-type']
            del part['content-transfer-encoding']
            part.set_payload(_("""\
A non-text attachment was scrubbed...
Name: %(filename)s
Type: %(ctype)s
Size: %(size)d bytes
Desc: %(desc)s
Url : %(url)s
"""), lcset)
        outer = False
    # We still have to sanitize multipart messages to flat text because
    # Pipermail can't handle messages with list payloads.  This is a kludge;
    # def (n) clever hack ;).
    if msg.is_multipart():
        # By default we take the charset of the first text/plain part in the
        # message, but if there was none, we'll use the list's preferred
        # language's charset.
        if charset is None or charset == 'us-ascii':
            charset = lcset
        # We now want to concatenate all the parts which have been scrubbed to
        # text/plain, into a single text/plain payload.  We need to make sure
        # all the characters in the concatenated string are in the same
        # encoding, so we'll use the 'replace' key in the coercion call.
        # BAW: Martin's original patch suggested we might want to try
        # generalizing to utf-8, and that's probably a good idea (eventually).
        text = []
        for part in msg.get_payload():
            # All parts should be scrubbed to text/plain by now.
            partctype = part.get_content_type()
            if partctype <> 'text/plain':
                text.append(_('Skipped content of type %(partctype)s'))
                continue
            try:
                t = part.get_payload(decode=True)
            except binascii.Error:
                t = part.get_payload()
            partcharset = part.get_content_charset()
            if partcharset and partcharset <> charset:
                try:
                    t = unicode(t, partcharset, 'replace')
                except (UnicodeError, LookupError, ValueError):
                    # Replace funny characters.  We use errors='replace' for
                    # both calls since the first replace will leave U+FFFD,
                    # which isn't ASCII encodeable.
                    u = unicode(t, 'ascii', 'replace')
                    t = u.encode('ascii', 'replace')
                try:
                    # Should use HTML-Escape, or try generalizing to UTF-8
                    t = t.encode(charset, 'replace')
                except (UnicodeError, LookupError, ValueError):
                    t = t.encode(lcset, 'replace')
            # Separation is useful
            if not t.endswith('\n'):
                t += '\n'
            text.append(t)
        # Now join the text and set the payload
        sep = _('-------------- next part --------------\n')
        del msg['content-type']
        msg.set_payload(sep.join(text), charset)
        del msg['content-transfer-encoding']
        msg.add_header('Content-Transfer-Encoding', '8bit')
    return msg



def makedirs(dir):
    # Create all the directories to store this attachment in
    try:
        os.makedirs(dir, 02775)
        # Unfortunately, FreeBSD seems to be broken in that it doesn't honor
        # the mode arg of mkdir().
        def twiddle(arg, dirname, names):
            os.chmod(dirname, 02775)
        os.path.walk(dir, twiddle, None)
    except OSError, e:
        if e.errno <> errno.EEXIST: raise



def save_attachment(mlist, msg, dir, filter_html=True):
    fsdir = os.path.join(mlist.archive_dir(), dir)
    makedirs(fsdir)
    # Figure out the attachment type and get the decoded data
    decodedpayload = msg.get_payload(decode=True)
    # BAW: mimetypes ought to handle non-standard, but commonly found types,
    # e.g. image/jpg (should be image/jpeg).  For now we just store such
    # things as application/octet-streams since that seems the safest.
    ctype = msg.get_content_type()
    fnext = os.path.splitext(msg.get_filename(''))[1]
    ext = guess_extension(ctype, fnext)
    if not ext:
        # We don't know what it is, so assume it's just a shapeless
        # application/octet-stream, unless the Content-Type: is
        # message/rfc822, in which case we know we'll coerce the type to
        # text/plain below.
        if ctype == 'message/rfc822':
            ext = '.txt'
        else:
            ext = '.bin'
    path = None
    # We need a lock to calculate the next attachment number
    lockfile = os.path.join(fsdir, 'attachments.lock')
    lock = LockFile.LockFile(lockfile)
    lock.lock()
    try:
        # Now base the filename on what's in the attachment, uniquifying it if
        # necessary.
        filename = msg.get_filename()
        if not filename:
            filebase = 'attachment'
        else:
            # Sanitize the filename given in the message headers
            parts = pre.split(filename)
            filename = parts[-1]
            # Strip off leading dots
            filename = dre.sub('', filename)
            # Allow only alphanumerics, dash, underscore, and dot
            filename = sre.sub('', filename)
            # If the filename's extension doesn't match the type we guessed,
            # which one should we go with?  For now, let's go with the one we
            # guessed so attachments can't lie about their type.  Also, if the
            # filename /has/ no extension, then tack on the one we guessed.
            filebase, ignore = os.path.splitext(filename)
        # Now we're looking for a unique name for this file on the file
        # system.  If msgdir/filebase.ext isn't unique, we'll add a counter
        # after filebase, e.g. msgdir/filebase-cnt.ext
        counter = 0
        extra = ''
        while True:
            path = os.path.join(fsdir, filebase + extra + ext)
            # Generally it is not a good idea to test for file existance
            # before just trying to create it, but the alternatives aren't
            # wonderful (i.e. os.open(..., O_CREAT | O_EXCL) isn't
            # NFS-safe).  Besides, we have an exclusive lock now, so we're
            # guaranteed that no other process will be racing with us.
            if os.path.exists(path):
                counter += 1
                extra = '-%04d' % counter
            else:
                break
    finally:
        lock.unlock()
    # `path' now contains the unique filename for the attachment.  There's
    # just one more step we need to do.  If the part is text/html and
    # ARCHIVE_HTML_SANITIZER is a string (which it must be or we wouldn't be
    # here), then send the attachment through the filter program for
    # sanitization
    if filter_html and ctype == 'text/html':
        base, ext = os.path.splitext(path)
        tmppath = base + '-tmp' + ext
        fp = open(tmppath, 'w')
        try:
            fp.write(decodedpayload)
            fp.close()
            cmd = mm_cfg.ARCHIVE_HTML_SANITIZER % {'filename' : tmppath}
            progfp = os.popen(cmd, 'r')
            decodedpayload = progfp.read()
            status = progfp.close()
            if status:
                syslog('error',
                       'HTML sanitizer exited with non-zero status: %s',
                       status)
        finally:
            os.unlink(tmppath)
        # BAW: Since we've now sanitized the document, it should be plain
        # text.  Blarg, we really want the sanitizer to tell us what the type
        # if the return data is. :(
        ext = '.txt'
        path = base + '.txt'
    # Is it a message/rfc822 attachment?
    elif ctype == 'message/rfc822':
        submsg = msg.get_payload()
        # BAW: I'm sure we can eventually do better than this. :(
        decodedpayload = Utils.websafe(str(submsg))
    fp = open(path, 'w')
    fp.write(decodedpayload)
    fp.close()
    # Now calculate the url
    baseurl = mlist.GetBaseArchiveURL()
    # Private archives will likely have a trailing slash.  Normalize.
    if baseurl[-1] <> '/':
        baseurl += '/'
    url = baseurl + '%s/%s%s%s' % (dir, filebase, extra, ext)
    return url
