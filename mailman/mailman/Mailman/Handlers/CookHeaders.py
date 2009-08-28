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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
# USA.

"""Cook a message's Subject header."""

from __future__ import nested_scopes
import re
from types import UnicodeType

from email.Charset import Charset
from email.Header import Header, decode_header, make_header
from email.Utils import parseaddr, formataddr, getaddresses
from email.Errors import HeaderParseError

from Mailman import mm_cfg
from Mailman import Utils
from Mailman.i18n import _
from Mailman.Logging.Syslog import syslog

CONTINUATION = ',\n\t'
COMMASPACE = ', '
MAXLINELEN = 78

# True/False
try:
    True, False
except NameError:
    True = 1
    False = 0



def _isunicode(s):
    return isinstance(s, UnicodeType)

nonascii = re.compile('[^\s!-~]')

def uheader(mlist, s, header_name=None, continuation_ws='\t', maxlinelen=None):
    # Get the charset to encode the string in. Then search if there is any
    # non-ascii character is in the string. If there is and the charset is
    # us-ascii then we use iso-8859-1 instead. If the string is ascii only
    # we use 'us-ascii' if another charset is specified.
    charset = Utils.GetCharSet(mlist.preferred_language)
    if nonascii.search(s):
        # use list charset but ...
        if charset == 'us-ascii':
            charset = 'iso-8859-1'
    else:
        # there is no nonascii so ...
        charset = 'us-ascii'
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
        try:
            prefix_subject(mlist, msg, msgdata)
        except (UnicodeError, ValueError):
            # TK: Sometimes subject header is not MIME encoded for 8bit
            # simply abort prefixing.
            pass
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
            i18ndesc = uheader(mlist, mlist.description, 'Reply-To')
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
        # Also skip Cc if this is an anonymous list as list posting address
        # is already in From and Reply-To in this case.
        if mlist.personalize == 2 and mlist.reply_goes_to_list <> 1 \
           and not mlist.anonymous_list:
            # Watch out for existing Cc headers, merge, and remove dups.  Note
            # that RFC 2822 says only zero or one Cc header is allowed.
            new = []
            d = {}
            for pair in getaddresses(msg.get_all('cc', [])):
                add(pair)
            i18ndesc = uheader(mlist, mlist.description, 'Cc')
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
    cset = Utils.GetCharSet(mlist.preferred_language)
    if mlist.description:
        # Don't wrap the header since here we just want to get it properly RFC
        # 2047 encoded.
        i18ndesc = uheader(mlist, mlist.description, 'List-Id', maxlinelen=998)
        listid_h = formataddr((str(i18ndesc), listid))
    else:
        # without desc we need to ensure the MUST brackets
        listid_h = '<%s>' % listid
    # We always add a List-ID: header.
    del msg['list-id']
    msg['List-Id'] = listid_h
    # For internally crafted messages, we also add a (nonstandard),
    # "X-List-Administrivia: yes" header.  For all others (i.e. those coming
    # from list posts), we add a bunch of other RFC 2369 headers.
    requestaddr = mlist.GetRequestEmail()
    subfieldfmt = '<%s>, <mailto:%s?subject=%ssubscribe>'
    listinfo = mlist.GetScriptURL('listinfo', absolute=1)
    useropts = mlist.GetScriptURL('options', absolute=1)
    headers = {}
    if msgdata.get('reduced_list_headers'):
        headers['X-List-Administrivia'] = 'yes'
    else:
        headers.update({
            'List-Help'       : '<mailto:%s?subject=help>' % requestaddr,
            'List-Unsubscribe': subfieldfmt % (useropts, requestaddr, 'un'),
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
    prefix = mlist.subject_prefix.strip()
    if not prefix:
        return
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
    # The subject may be multilingual but we take the first charset as major
    # one and try to decode.  If it is decodable, returned subject is in one
    # line and cset is properly set.  If fail, subject is mime-encoded and
    # cset is set as us-ascii.  See detail for ch_oneline() (CookHeaders one
    # line function).
    subject, cset = ch_oneline(subject)
    # TK: Python interpreter has evolved to be strict on ascii charset code
    # range.  It is safe to use unicode string when manupilating header
    # contents with re module.  It would be best to return unicode in
    # ch_oneline() but here is temporary solution.
    subject = unicode(subject, cset)
    # If the subject_prefix contains '%d', it is replaced with the
    # mailing list sequential number.  Sequential number format allows
    # '%d' or '%05d' like pattern.
    prefix_pattern = re.escape(prefix)
    # unescape '%' :-<
    prefix_pattern = '%'.join(prefix_pattern.split(r'\%'))
    p = re.compile('%\d*d')
    if p.search(prefix, 1):
        # prefix have number, so we should search prefix w/number in subject.
        # Also, force new style.
        prefix_pattern = p.sub(r'\s*\d+\s*', prefix_pattern)
        old_style = False
    else:
        old_style = mm_cfg.OLD_STYLE_PREFIXING
    subject = re.sub(prefix_pattern, '', subject)
    rematch = re.match('((RE|AW|SV|VS)(\[\d+\])?:\s*)+', subject, re.I)
    if rematch:
        subject = subject[rematch.end():]
        recolon = 'Re:'
    else:
        recolon = ''
    # At this point, subject may become null if someone post mail with
    # subject: [subject prefix]
    if subject.strip() == '':
        subject = _('(no subject)')
        cset = Utils.GetCharSet(mlist.preferred_language)
        subject = unicode(subject, cset)
    # and substitute %d in prefix with post_id
    try:
        prefix = prefix % mlist.post_id
    except TypeError:
        pass
    # If charset is 'us-ascii', try to concatnate as string because there
    # is some weirdness in Header module (TK)
    if cset == 'us-ascii':
        try:
            if old_style:
                h = u' '.join([recolon, prefix, subject])
            else:
                if recolon:
                    h = u' '.join([prefix, recolon, subject])
                else:
                    h = u' '.join([prefix, subject])
            h = h.encode('us-ascii')
            h = uheader(mlist, h, 'Subject', continuation_ws=ws)
            del msg['subject']
            msg['Subject'] = h
            ss = u' '.join([recolon, subject])
            ss = ss.encode('us-ascii')
            ss = uheader(mlist, ss, 'Subject', continuation_ws=ws)
            msgdata['stripped_subject'] = ss
            return
        except UnicodeError:
            pass
    # Get the header as a Header instance, with proper unicode conversion
    if old_style:
        h = uheader(mlist, recolon, 'Subject', continuation_ws=ws)
        h.append(prefix)
    else:
        h = uheader(mlist, prefix, 'Subject', continuation_ws=ws)
        h.append(recolon)
    # TK: Subject is concatenated and unicode string.
    subject = subject.encode(cset, 'replace')
    h.append(subject, cset)
    del msg['subject']
    msg['Subject'] = h
    ss = uheader(mlist, recolon, 'Subject', continuation_ws=ws)
    ss.append(subject, cset)
    msgdata['stripped_subject'] = ss



def ch_oneline(headerstr):
    # Decode header string in one line and convert into single charset
    # copied and modified from ToDigest.py and Utils.py
    # return (string, cset) tuple as check for failure
    try:
        d = decode_header(headerstr)
        # at this point, we should rstrip() every string because some
        # MUA deliberately add trailing spaces when composing return
        # message.
        d = [(s.rstrip(), c) for (s,c) in d]
        cset = 'us-ascii'
        for x in d:
            # search for no-None charset
            if x[1]:
                cset = x[1]
                break
        h = make_header(d)
        ustr = h.__unicode__()
        oneline = u''.join(ustr.splitlines())
        return oneline.encode(cset, 'replace'), cset
    except (LookupError, UnicodeError, ValueError, HeaderParseError):
        # possibly charset problem. return with undecoded string in one line.
        return ''.join(headerstr.splitlines()), 'us-ascii'
