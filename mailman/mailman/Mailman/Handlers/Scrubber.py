# Copyright (C) 2001-2008 by the Free Software Foundation, Inc.
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

"""Cleanse a message for archiving."""

from __future__ import nested_scopes

import os
import re
import time
import errno
import binascii
import tempfile
from cStringIO import StringIO
from types import IntType, StringType

from email.Utils import parsedate
from email.Parser import HeaderParser
from email.Generator import Generator
from email.Charset import Charset

from Mailman import mm_cfg
from Mailman import Utils
from Mailman import LockFile
from Mailman import Message
from Mailman.Errors import DiscardMessage
from Mailman.i18n import _
from Mailman.Logging.Syslog import syslog
from Mailman.Utils import sha_new

# Path characters for common platforms
pre = re.compile(r'[/\\:]')
# All other characters to strip out of Content-Disposition: filenames
# (essentially anything that isn't an alphanum, dot, dash, or underscore).
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


def safe_strftime(fmt, t):
    try:
        return time.strftime(fmt, t)
    except (TypeError, ValueError, OverflowError):
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
    digest = sha_new(msgid).hexdigest()
    return os.path.join('attachments', datedir, digest[:4] + digest[-4:])


def replace_payload_by_text(msg, text, charset):
    # TK: This is a common function in replacing the attachment and the main
    # message by a text (scrubbing).
    del msg['content-type']
    del msg['content-transfer-encoding']
    msg.set_payload(text, charset)



def process(mlist, msg, msgdata=None):
    sanitize = mm_cfg.ARCHIVE_HTML_SANITIZER
    outer = True
    if msgdata is None:
        msgdata = {}
    if msgdata:
        # msgdata is available if it is in GLOBAL_PIPELINE
        # ie. not in digest or archiver
        # check if the list owner want to scrub regular delivery
        if not mlist.scrub_nondigest:
            return
    dir = calculate_attachments_dir(mlist, msg, msgdata)
    charset = None
    lcset = Utils.GetCharSet(mlist.preferred_language)
    lcset_out = Charset(lcset).output_charset or lcset
    # Now walk over all subparts of this message and scrub out various types
    format = delsp = None
    for part in msg.walk():
        ctype = part.get_content_type()
        # If the part is text/plain, we leave it alone
        if ctype == 'text/plain':
            # We need to choose a charset for the scrubbed message, so we'll
            # arbitrarily pick the charset of the first text/plain part in the
            # message.
            # MAS: Also get the RFC 3676 stuff from this part. This seems to
            # work OK for scrub_nondigest.  It will also work as far as
            # scrubbing messages for the archive is concerned, but pipermail
            # doesn't pay any attention to the RFC 3676 parameters.  The plain
            # format digest is going to be a disaster in any case as some of
            # messages will be format="flowed" and some not.  ToDigest creates
            # its own Content-Type: header for the plain digest which won't
            # have RFC 3676 parameters. If the message Content-Type: headers
            # are retained for display in the digest, the parameters will be
            # there for information, but not for the MUA. This is the best we
            # can do without having get_payload() process the parameters.
            if charset is None:
                charset = part.get_content_charset(lcset)
                format = part.get_param('format')
                delsp = part.get_param('delsp')
            # TK: if part is attached then check charset and scrub if none
            if part.get('content-disposition') and \
               not part.get_content_charset():
                omask = os.umask(002)
                try:
                    url = save_attachment(mlist, part, dir)
                finally:
                    os.umask(omask)
                filename = part.get_filename(_('not available'))
                filename = Utils.oneline(filename, lcset)
                replace_payload_by_text(part, _("""\
An embedded and charset-unspecified text was scrubbed...
Name: %(filename)s
URL: %(url)s
"""), lcset)
        elif ctype == 'text/html' and isinstance(sanitize, IntType):
            if sanitize == 0:
                if outer:
                    raise DiscardMessage
                replace_payload_by_text(part,
                                 _('HTML attachment scrubbed and removed'),
                                 # Adding charset arg and removing content-type
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
                replace_payload_by_text(part, _("""\
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
                replace_payload_by_text(part, _("""\
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
            replace_payload_by_text(part, _("""\
An embedded message was scrubbed...
From: %(who)s
Subject: %(subject)s
Date: %(date)s
Size: %(size)s
URL: %(url)s
"""), lcset)
        # If the message isn't a multipart, then we'll strip it out as an
        # attachment that would have to be separately downloaded.  Pipermail
        # will transform the url into a hyperlink.
        elif part.get_payload() and not part.is_multipart():
            payload = part.get_payload(decode=True)
            ctype = part.get_content_type()
            # XXX Under email 2.5, it is possible that payload will be None.
            # This can happen when you have a Content-Type: multipart/* with
            # only one part and that part has two blank lines between the
            # first boundary and the end boundary.  In email 3.0 you end up
            # with a string in the payload.  I think in this case it's safe to
            # ignore the part.
            if payload is None:
                continue
            size = len(payload)
            omask = os.umask(002)
            try:
                url = save_attachment(mlist, part, dir)
            finally:
                os.umask(omask)
            desc = part.get('content-description', _('not available'))
            desc = Utils.oneline(desc, lcset)
            filename = part.get_filename(_('not available'))
            filename = Utils.oneline(filename, lcset)
            replace_payload_by_text(part, _("""\
A non-text attachment was scrubbed...
Name: %(filename)s
Type: %(ctype)s
Size: %(size)d bytes
Desc: %(desc)s
URL: %(url)s
"""), lcset)
        outer = False
    # We still have to sanitize multipart messages to flat text because
    # Pipermail can't handle messages with list payloads.  This is a kludge;
    # def (n) clever hack ;).
    if msg.is_multipart() and sanitize <> 2:
        # By default we take the charset of the first text/plain part in the
        # message, but if there was none, we'll use the list's preferred
        # language's charset.
        if not charset or charset == 'us-ascii':
            charset = lcset_out
        else:
            # normalize to the output charset if input/output are different
            charset = Charset(charset).output_charset or charset
        # We now want to concatenate all the parts which have been scrubbed to
        # text/plain, into a single text/plain payload.  We need to make sure
        # all the characters in the concatenated string are in the same
        # encoding, so we'll use the 'replace' key in the coercion call.
        # BAW: Martin's original patch suggested we might want to try
        # generalizing to utf-8, and that's probably a good idea (eventually).
        text = []
        for part in msg.walk():
            # TK: bug-id 1099138 and multipart
            # MAS test payload - if part may fail if there are no headers.
            if not part.get_payload() or part.is_multipart():
                continue
            # All parts should be scrubbed to text/plain by now.
            partctype = part.get_content_type()
            if partctype <> 'text/plain':
                text.append(_('Skipped content of type %(partctype)s\n'))
                continue
            try:
                t = part.get_payload(decode=True) or ''
            # MAS: TypeError exception can occur if payload is None. This
            # was observed with a message that contained an attached
            # message/delivery-status part. Because of the special parsing
            # of this type, this resulted in a text/plain sub-part with a
            # null body. See bug 1430236.
            except (binascii.Error, TypeError):
                t = part.get_payload() or ''
            # TK: get_content_charset() returns 'iso-2022-jp' for internally
            # crafted (scrubbed) 'euc-jp' text part. So, first try
            # get_charset(), then get_content_charset() for the parts
            # which are already embeded in the incoming message.
            partcharset = part.get_charset()
            if partcharset:
                partcharset = str(partcharset)
            else:
                partcharset = part.get_content_charset()
            if partcharset and partcharset <> charset:
                try:
                    t = unicode(t, partcharset, 'replace')
                except (UnicodeError, LookupError, ValueError,
                        AssertionError):
                    # We can get here if partcharset is bogus in come way.
                    # Replace funny characters.  We use errors='replace'
                    t = unicode(t, 'ascii', 'replace')
                try:
                    # Should use HTML-Escape, or try generalizing to UTF-8
                    t = t.encode(charset, 'replace')
                except (UnicodeError, LookupError, ValueError,
                        AssertionError):
                    # if the message charset is bogus, use the list's.
                    t = t.encode(lcset, 'replace')
            # Separation is useful
            if isinstance(t, StringType):
                if not t.endswith('\n'):
                    t += '\n'
                text.append(t)
        # Now join the text and set the payload
        sep = _('-------------- next part --------------\n')
        # The i18n separator is in the list's charset. Coerce it to the
        # message charset.
        try:
            s = unicode(sep, lcset, 'replace')
            sep = s.encode(charset, 'replace')
        except (UnicodeError, LookupError, ValueError,
                AssertionError):
            pass
        replace_payload_by_text(msg, sep.join(text), charset)
        if format:
            msg.set_param('Format', format)
        if delsp:
            msg.set_param('DelSp', delsp)
    return msg



def makedirs(dir):
    # Create all the directories to store this attachment in
    try:
        os.makedirs(dir, 0775)
        # Unfortunately, FreeBSD seems to be broken in that it doesn't honor
        # the mode arg of mkdir().
        def twiddle(arg, dirname, names):
            os.chmod(dirname, 0775)
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
    # i18n file name is encoded
    lcset = Utils.GetCharSet(mlist.preferred_language)
    filename = Utils.oneline(msg.get_filename(''), lcset)
    filename, fnext = os.path.splitext(filename)
    # For safety, we should confirm this is valid ext for content-type
    # but we can use fnext if we introduce fnext filtering
    if mm_cfg.SCRUBBER_USE_ATTACHMENT_FILENAME_EXTENSION:
        # HTML message doesn't have filename :-(
        ext = fnext or guess_extension(ctype, fnext)
    else:
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
    # Allow only alphanumerics, dash, underscore, and dot
    ext = sre.sub('', ext)
    path = None
    # We need a lock to calculate the next attachment number
    lockfile = os.path.join(fsdir, 'attachments.lock')
    lock = LockFile.LockFile(lockfile)
    lock.lock()
    try:
        # Now base the filename on what's in the attachment, uniquifying it if
        # necessary.
        if not filename or mm_cfg.SCRUBBER_DONT_USE_ATTACHMENT_FILENAME:
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
            # The extension was removed from the name above.
            filebase = filename
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
    # A trailing space in url string may save users who are using
    # RFC-1738 compliant MUA (Not Mozilla).
    # Trailing space will definitely be a problem with format=flowed.
    # Bracket the URL instead.
    url = '<' + baseurl + '%s/%s%s%s>' % (dir, filebase, extra, ext)
    return url
