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

"""Cook a message's Subject header.
"""

from __future__ import nested_scopes
import re
from types import UnicodeType

from email.Charset import Charset
from email.Header import Header, decode_header
from email.Utils import parseaddr, formataddr, getaddresses

from Mailman import mm_cfg
from Mailman import Utils
from Mailman.i18n import _
from Mailman.Logging.Syslog import syslog

CONTINUATION = ',\n\t'
COMMASPACE = ', '
MAXLINELEN = 78



def _isunicode(s):
    return isinstance(s, UnicodeType)

def uheader(mlist, s, header_name=None, continuation_ws='\t', maxlinelen=None):
    # Get the charset to encode the string in.  If this is us-ascii, we'll use
    # iso-8859-1 instead, just to get a little extra coverage, and because the
    # Header class tries us-ascii first anyway.
    charset = Utils.GetCharSet(mlist.preferred_language)
    if charset == 'us-ascii':
        charset = 'iso-8859-1'
    charset = Charset(charset)
    # Convert the string to unicode so Header will do the 3-charset encoding.
    # If s is a byte string and there are funky characters in it that don't
    # match the charset, we might as well replace them now.
    if not _isunicode(s):
        codec = charset.input_codec or 'ascii'
        s = unicode(s, codec, 'replace')
    # We purposefully leave no space b/w prefix and subject!
    return Header(s, charset, maxlinelen, header_name, continuation_ws)



def process(mlist, msg, msgdata):
    # Set the "X-Ack: no" header if noack flag is set.
    if msgdata.get('noack'):
        del msg['x-ack']
        msg['X-Ack'] = 'no'
    # Because we're going to modify various important headers in the email
    # message, we want to save some of the information in the msgdata
    # dictionary for later.  Specifically, the sender header will get waxed,
    # but we need it for the Acknowledge module later.
    msgdata['original_sender'] = msg.get_sender()
    # VirginRunner sets _fasttrack for internally crafted messages.
    fasttrack = msgdata.get('_fasttrack')
    if not msgdata.get('isdigest') and not fasttrack:
        prefix_subject(mlist, msg, msgdata)
    # Mark message so we know we've been here, but leave any existing
    # X-BeenThere's intact.
    msg['X-BeenThere'] = mlist.GetListEmail()
    # Add Precedence: and other useful headers.  None of these are standard
    # and finding information on some of them are fairly difficult.  Some are
    # just common practice, and we'll add more here as they become necessary.
    # Good places to look are:
    #
    # http://www.dsv.su.se/~jpalme/ietf/jp-ietf-home.html
    # http://www.faqs.org/rfcs/rfc2076.html
    #
    # None of these headers are added if they already exist.  BAW: some
    # consider the advertising of this a security breach.  I.e. if there are
    # known exploits in a particular version of Mailman and we know a site is
    # using such an old version, they may be vulnerable.  It's too easy to
    # edit the code to add a configuration variable to handle this.
    if not msg.has_key('x-mailman-version'):
        msg['X-Mailman-Version'] = mm_cfg.VERSION
    # We set "Precedence: list" because this is the recommendation from the
    # sendmail docs, the most authoritative source of this header's semantics.
    if not msg.has_key('precedence'):
        msg['Precedence'] = 'list'
    # Reply-To: munging.  Do not do this if the message is "fast tracked",
    # meaning it is internally crafted and delivered to a specific user.  BAW:
    # Yuck, I really hate this feature but I've caved under the sheer pressure
    # of the (very vocal) folks want it.  OTOH, RFC 2822 allows Reply-To: to
    # be a list of addresses, so instead of replacing the original, simply
    # augment it.  RFC 2822 allows max one Reply-To: header so collapse them
    # if we're adding a value, otherwise don't touch it.  (Should we collapse
    # in all cases?)
    if not fasttrack:
        # A convenience function, requires nested scopes.  pair is (name, addr)
        new = []
        d = {}
        def add(pair):
            lcaddr = pair[1].lower()
            if d.has_key(lcaddr):
                return
            d[lcaddr] = pair
            new.append(pair)
        # List admin wants an explicit Reply-To: added
        if mlist.reply_goes_to_list == 2:
            add(parseaddr(mlist.reply_to_address))
        # If we're not first stripping existing Reply-To: then we need to add
        # the original Reply-To:'s to the list we're building up.  In both
        # cases we'll zap the existing field because RFC 2822 says max one is
        # allowed.
        if not mlist.first_strip_reply_to:
            orig = msg.get_all('reply-to', [])
            for pair in getaddresses(orig):
                add(pair)
        # Set Reply-To: header to point back to this list.  Add this last
        # because some folks think that some MUAs make it easier to delete
        # addresses from the right than from the left.
        if mlist.reply_goes_to_list == 1:
            i18ndesc = uheader(mlist, mlist.description)
            add((str(i18ndesc), mlist.GetListEmail()))
        del msg['reply-to']
        # Don't put Reply-To: back if there's nothing to add!
        if new:
            # Preserve order
            msg['Reply-To'] = COMMASPACE.join(
                [formataddr(pair) for pair in new])
        # The To field normally contains the list posting address.  However
        # when messages are fully personalized, that header will get
        # overwritten with the address of the recipient.  We need to get the
        # posting address in one of the recipient headers or they won't be
        # able to reply back to the list.  It's possible the posting address
        # was munged into the Reply-To header, but if not, we'll add it to a
        # Cc header.  BAW: should we force it into a Reply-To header in the
        # above code?
        if mlist.personalize == 2 and mlist.reply_goes_to_list <> 1:
            # Watch out for existing Cc headers, merge, and remove dups.  Note
            # that RFC 2822 says only zero or one Cc header is allowed.
            new = []
            d = {}
            for pair in getaddresses(msg.get_all('cc', [])):
                add(pair)
            i18ndesc = uheader(mlist, mlist.description)
            add((str(i18ndesc), mlist.GetListEmail()))
            del msg['Cc']
            msg['Cc'] = COMMASPACE.join([formataddr(pair) for pair in new])
    # Add list-specific headers as defined in RFC 2369 and RFC 2919, but only
    # if the message is being crafted for a specific list (e.g. not for the
    # password reminders).
    #
    # BAW: Some people really hate the List-* headers.  It seems that the free
    # version of Eudora (possibly on for some platforms) does not hide these
    # headers by default, pissing off their users.  Too bad.  Fix the MUAs.
    if msgdata.get('_nolist') or not mlist.include_rfc2369_headers:
        return
    # This will act like an email address for purposes of formataddr()
    listid = '%s.%s' % (mlist.internal_name(), mlist.host_name)
    if mlist.description:
        # Don't wrap the header since here we just want to get it properly RFC
        # 2047 encoded.
        h = uheader(mlist, mlist.description, 'List-Id', maxlinelen=10000)
        desc = str(h)
    else:
        desc = ''
    listid_h = formataddr((desc, listid))
    # BAW: I think the message object should handle any necessary wrapping.
    del msg['list-id']
    msg['List-Id'] = listid_h
    # For internally crafted messages, we
    # also add a (nonstandard), "X-List-Administrivia: yes" header.  For all
    # others (i.e. those coming from list posts), we adda a bunch of other RFC
    # 2369 headers.
    requestaddr = mlist.GetRequestEmail()
    subfieldfmt = '<%s>, <mailto:%s?subject=%ssubscribe>'
    listinfo = mlist.GetScriptURL('listinfo', absolute=1)
    headers = {}
    if msgdata.get('reduced_list_headers'):
        headers['X-List-Administrivia'] = 'yes'
    else:
        headers.update({
            'List-Help'       : '<mailto:%s?subject=help>' % requestaddr,
            'List-Unsubscribe': subfieldfmt % (listinfo, requestaddr, 'un'),
            'List-Subscribe'  : subfieldfmt % (listinfo, requestaddr, ''),
            })
        # List-Post: is controlled by a separate attribute
        if mlist.include_list_post_header:
            headers['List-Post'] = '<mailto:%s>' % mlist.GetListEmail()
        # Add this header if we're archiving
        if mlist.archive:
            archiveurl = mlist.GetBaseArchiveURL()
            if archiveurl.endswith('/'):
                archiveurl = archiveurl[:-1]
            headers['List-Archive'] = '<%s>' % archiveurl
    # First we delete any pre-existing headers because the RFC permits only
    # one copy of each, and we want to be sure it's ours.
    for h, v in headers.items():
        del msg[h]
        # Wrap these lines if they are too long.  78 character width probably
        # shouldn't be hardcoded, but is at least text-MUA friendly.  The
        # adding of 2 is for the colon-space separator.
        if len(h) + 2 + len(v) > 78:
            v = CONTINUATION.join(v.split(', '))
        msg[h] = v



def prefix_subject(mlist, msg, msgdata):
    # Add the subject prefix unless the message is a digest or is being fast
    # tracked (e.g. internally crafted, delivered to a single user such as the
    # list admin).
    prefix = mlist.subject_prefix
    subject = msg.get('subject', '')
    # Try to figure out what the continuation_ws is for the header
    if isinstance(subject, Header):
        lines = str(subject).splitlines()
    else:
        lines = subject.splitlines()
    ws = '\t'
    if len(lines) > 1 and lines[1] and lines[1][0] in ' \t':
        ws = lines[1][0]
    msgdata['origsubj'] = subject
    if not subject:
        subject = _('(no subject)')
    # The header may be multilingual; decode it from base64/quopri and search
    # each chunk for the prefix.  BAW: Note that if the prefix contains spaces
    # and each word of the prefix is encoded in a different chunk in the
    # header, we won't find it.  I think in practice that's unlikely though.
    headerbits = decode_header(subject)
    if prefix and subject:
        pattern = re.escape(prefix.strip())
        for decodedsubj, charset in headerbits:
            if re.search(pattern, decodedsubj, re.IGNORECASE):
                # The subject's already got the prefix, so don't change it
                return
    del msg['subject']
    # Get the header as a Header instance, with proper unicode conversion
    h = uheader(mlist, prefix, 'Subject', continuation_ws=ws)
    for s, c in headerbits:
        # Once again, convert the string to unicode.
        if c is None:
            c = Charset('iso-8859-1')
        if not isinstance(c, Charset):
            c = Charset(c)
        if not _isunicode(s):
            codec = c.input_codec or 'ascii'
            try:
                s = unicode(s, codec, 'replace')
            except LookupError:
                # Unknown codec, is this default reasonable?
                s = unicode(s, Utils.GetCharSet(mlist.preferred_language),
                            'replace')
        h.append(s, c)
    msg['Subject'] = h
