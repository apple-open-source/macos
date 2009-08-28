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

"""Decorate a message by sticking the header and footer around it."""

import re

from types import ListType
from email.MIMEText import MIMEText

from Mailman import mm_cfg
from Mailman import Utils
from Mailman import Errors
from Mailman.Message import Message
from Mailman.i18n import _
from Mailman.SafeDict import SafeDict
from Mailman.Logging.Syslog import syslog

try:
    True, False
except:
    True = 1
    False = 0



def process(mlist, msg, msgdata):
    # Digests and Mailman-craft messages should not get additional headers
    if msgdata.get('isdigest') or msgdata.get('nodecorate'):
        return
    d = {}
    if msgdata.get('personalize'):
        # Calculate the extra personalization dictionary.  Note that the
        # length of the recips list better be exactly 1.
        recips = msgdata.get('recips')
        assert type(recips) == ListType and len(recips) == 1
        member = recips[0].lower()
        d['user_address'] = member
        try:
            d['user_delivered_to'] = mlist.getMemberCPAddress(member)
            # BAW: Hmm, should we allow this?
            d['user_password'] = mlist.getMemberPassword(member)
            d['user_language'] = mlist.getMemberLanguage(member)
            username = mlist.getMemberName(member) or None
            try:
                username = username.encode(Utils.GetCharSet(d['user_language']))
            except (AttributeError, UnicodeError):
                username = member
            d['user_name'] = username
            d['user_optionsurl'] = mlist.GetOptionsURL(member)
        except Errors.NotAMemberError:
            pass
    # These strings are descriptive for the log file and shouldn't be i18n'd
    d.update(msgdata.get('decoration-data', {}))
    header = decorate(mlist, mlist.msg_header, 'non-digest header', d)
    footer = decorate(mlist, mlist.msg_footer, 'non-digest footer', d)
    # Escape hatch if both the footer and header are empty
    if not header and not footer:
        return
    # Be MIME smart here.  We only attach the header and footer by
    # concatenation when the message is a non-multipart of type text/plain.
    # Otherwise, if it is not a multipart, we make it a multipart, and then we
    # add the header and footer as text/plain parts.
    #
    # BJG: In addition, only add the footer if the message's character set
    # matches the charset of the list's preferred language.  This is a
    # suboptimal solution, and should be solved by allowing a list to have
    # multiple headers/footers, for each language the list supports.
    #
    # Also, if the list's preferred charset is us-ascii, we can always
    # safely add the header/footer to a plain text message since all
    # charsets Mailman supports are strict supersets of us-ascii --
    # no, UTF-16 emails are not supported yet.
    #
    # TK: Message with 'charset=' cause trouble. So, instead of
    #     mgs.get_content_charset('us-ascii') ...
    mcset = msg.get_content_charset() or 'us-ascii'
    lcset = Utils.GetCharSet(mlist.preferred_language)
    msgtype = msg.get_content_type()
    # BAW: If the charsets don't match, should we add the header and footer by
    # MIME multipart chroming the message?
    wrap = True
    if not msg.is_multipart() and msgtype == 'text/plain':
        # TK: Try to keep the message plain by converting the header/
        # footer/oldpayload into unicode and encode with mcset/lcset.
        # Try to decode qp/base64 also.
        # It is possible header/footer is already unicode if it was
        # interpolated with a unicode.
        if isinstance(header, unicode):
            uheader = header
        else:
            uheader = unicode(header, lcset, 'ignore')
        if isinstance(footer, unicode):
            ufooter = footer
        else:
            ufooter = unicode(footer, lcset, 'ignore')
        try:
            oldpayload = unicode(msg.get_payload(decode=True), mcset)
            frontsep = endsep = u''
            if header and not header.endswith('\n'):
                frontsep = u'\n'
            if footer and not oldpayload.endswith('\n'):
                endsep = u'\n'
            payload = uheader + frontsep + oldpayload + endsep + ufooter
            try:
                # first, try encode with list charset
                payload = payload.encode(lcset)
                newcset = lcset
            except UnicodeError:
                if lcset != mcset:
                    # if fail, encode with message charset (if different)
                    payload = payload.encode(mcset)
                    newcset = mcset
                    # if this fails, fallback to outer try and wrap=true
            format = msg.get_param('format')
            delsp = msg.get_param('delsp')
            del msg['content-transfer-encoding']
            del msg['content-type']
            msg.set_payload(payload, newcset)
            if format:
                msg.set_param('Format', format)
            if delsp:
                msg.set_param('DelSp', delsp)
            wrap = False
        except (LookupError, UnicodeError):
            pass
    elif msg.get_content_type() == 'multipart/mixed':
        # The next easiest thing to do is just prepend the header and append
        # the footer as additional subparts
        payload = msg.get_payload()
        if not isinstance(payload, ListType):
            payload = [payload]
        if footer:
            mimeftr = MIMEText(footer, 'plain', lcset)
            mimeftr['Content-Disposition'] = 'inline'
            payload.append(mimeftr)
        if header:
            mimehdr = MIMEText(header, 'plain', lcset)
            mimehdr['Content-Disposition'] = 'inline'
            payload.insert(0, mimehdr)
        msg.set_payload(payload)
        wrap = False
    # If we couldn't add the header or footer in a less intrusive way, we can
    # at least do it by MIME encapsulation.  We want to keep as much of the
    # outer chrome as possible.
    if not wrap:
        return
    # Because of the way Message objects are passed around to process(), we
    # need to play tricks with the outer message -- i.e. the outer one must
    # remain the same instance.  So we're going to create a clone of the outer
    # message, with all the header chrome intact, then copy the payload to it.
    # This will give us a clone of the original message, and it will form the
    # basis of the interior, wrapped Message.
    inner = Message()
    # Which headers to copy?  Let's just do the Content-* headers
    copied = False
    for h, v in msg.items():
        if h.lower().startswith('content-'):
            inner[h] = v
            copied = True
    inner.set_payload(msg.get_payload())
    # For completeness
    inner.set_unixfrom(msg.get_unixfrom())
    inner.preamble = msg.preamble
    inner.epilogue = msg.epilogue
    # Don't copy get_charset, as this might be None, even if
    # get_content_charset isn't.  However, do make sure there is a default
    # content-type, even if the original message was not MIME.
    inner.set_default_type(msg.get_default_type())
    if not copied:
        inner['Content-Type'] = inner.get_content_type()
    if msg['mime-version'] == None:
        msg['MIME-Version'] = '1.0'
    # BAW: HACK ALERT.
    if hasattr(msg, '__version__'):
        inner.__version__ = msg.__version__
    # Now, play games with the outer message to make it contain three
    # subparts: the header (if any), the wrapped message, and the footer (if
    # any).
    payload = [inner]
    if header:
        mimehdr = MIMEText(header, 'plain', lcset)
        mimehdr['Content-Disposition'] = 'inline'
        payload.insert(0, mimehdr)
    if footer:
        mimeftr = MIMEText(footer, 'plain', lcset)
        mimeftr['Content-Disposition'] = 'inline'
        payload.append(mimeftr)
    msg.set_payload(payload)
    del msg['content-type']
    del msg['content-transfer-encoding']
    del msg['content-disposition']
    msg['Content-Type'] = 'multipart/mixed'



def decorate(mlist, template, what, extradict=None):
    # `what' is just a descriptive phrase used in the log message
    #
    # BAW: We've found too many situations where Python can be fooled into
    # interpolating too much revealing data into a format string.  For
    # example, a footer of "% silly %(real_name)s" would give a header
    # containing all list attributes.  While we've previously removed such
    # really bad ones like `password' and `passwords', it's much better to
    # provide a whitelist of known good attributes, then to try to remove a
    # blacklist of known bad ones.
    d = SafeDict({'real_name'     : mlist.real_name,
                  'list_name'     : mlist.internal_name(),
                  # For backwards compatibility
                  '_internal_name': mlist.internal_name(),
                  'host_name'     : mlist.host_name,
                  'web_page_url'  : mlist.web_page_url,
                  'description'   : mlist.description,
                  'info'          : mlist.info,
                  'cgiext'        : mm_cfg.CGIEXT,
                  })
    if extradict is not None:
        d.update(extradict)
    # Using $-strings?
    if getattr(mlist, 'use_dollar_strings', 0):
        template = Utils.to_percent(template)
    # Interpolate into the template
    try:
        text = re.sub(r'(?m)(?<!^--) +(?=\n)', '',
                      re.sub(r'\r\n', r'\n', template % d))
    except (ValueError, TypeError), e:
        syslog('error', 'Exception while calculating %s:\n%s', what, e)
        what = what.upper()
        text = template
    return text
