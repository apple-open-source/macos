# Copyright (C) 2000-2003 by the Free Software Foundation, Inc.
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

"""NNTP queue runner."""

import re
import socket
import nntplib
from cStringIO import StringIO

import email
from email.Utils import getaddresses

COMMASPACE = ', '

from Mailman import mm_cfg
from Mailman import Utils
from Mailman.Queue.Runner import Runner
from Mailman.Logging.Syslog import syslog


# Matches our Mailman crafted Message-IDs.  See Utils.unique_message_id()
mcre = re.compile(r"""
    <mailman.                                     # match the prefix
    \d+.                                          # serial number
    \d+.                                          # time in seconds since epoch
    \d+.                                          # pid
    (?P<listname>[^@]+)                           # list's internal_name()
    @                                             # localpart@dom.ain
    (?P<hostname>[^>]+)                           # list's host_name
    >                                             # trailer
    """, re.VERBOSE)


try:
    True, False
except NameError:
    True = 1
    False = 0



class NewsRunner(Runner):
    QDIR = mm_cfg.NEWSQUEUE_DIR

    def _dispose(self, mlist, msg, msgdata):
        # Make sure we have the most up-to-date state
        mlist.Load()
        if not msgdata.get('prepped'):
            prepare_message(mlist, msg, msgdata)
        try:
            # Flatten the message object, sticking it in a StringIO object
            fp = StringIO(msg.as_string())
            conn = None
            try:
                try:
                    nntp_host, nntp_port = Utils.nntpsplit(mlist.nntp_host)
                    conn = nntplib.NNTP(nntp_host, nntp_port,
                                        readermode=True,
                                        user=mm_cfg.NNTP_USERNAME,
                                        password=mm_cfg.NNTP_PASSWORD)
                    conn.post(fp)
                except nntplib.error_temp, e:
                    syslog('error',
                           '(NNTPDirect) NNTP error for list "%s": %s',
                           mlist.internal_name(), e)
                except socket.error, e:
                    syslog('error',
                           '(NNTPDirect) socket error for list "%s": %s',
                           mlist.internal_name(), e)
            finally:
                if conn:
                    conn.quit()
        except Exception, e:
            # Some other exception occurred, which we definitely did not
            # expect, so set this message up for requeuing.
            self._log(e)
            return True
        return False



def prepare_message(mlist, msg, msgdata):
    # If the newsgroup is moderated, we need to add this header for the Usenet
    # software to accept the posting, and not forward it on to the n.g.'s
    # moderation address.  The posting would not have gotten here if it hadn't
    # already been approved.  1 == open list, mod n.g., 2 == moderated
    if mlist.news_moderation in (1, 2):
        del msg['approved']
        msg['Approved'] = mlist.GetListEmail()
    # Should we restore the original, non-prefixed subject for gatewayed
    # messages?
    origsubj = msgdata.get('origsubj')
    if not mlist.news_prefix_subject_too and origsubj is not None:
        del msg['subject']
        msg['subject'] = origsubj
    # Add the appropriate Newsgroups: header
    ngheader = msg['newsgroups']
    if ngheader is not None:
        # See if the Newsgroups: header already contains our linked_newsgroup.
        # If so, don't add it again.  If not, append our linked_newsgroup to
        # the end of the header list
        ngroups = [s.strip() for s in ngheader.split(',')]
        if mlist.linked_newsgroup not in ngroups:
            ngroups.append(mlist.linked_newsgroup)
            # Subtitute our new header for the old one.
            del msg['newsgroups']
            msg['Newsgroups'] = COMMASPACE.join(ngroups)
    else:
        # Newsgroups: isn't in the message
        msg['Newsgroups'] = mlist.linked_newsgroup
    # Note: We need to be sure two messages aren't ever sent to the same list
    # in the same process, since message ids need to be unique.  Further, if
    # messages are crossposted to two Usenet-gated mailing lists, they each
    # need to have unique message ids or the nntpd will only accept one of
    # them.  The solution here is to substitute any existing message-id that
    # isn't ours with one of ours, so we need to parse it to be sure we're not
    # looping.
    #
    # Our Message-ID format is <mailman.secs.pid.listname@hostname>
    msgid = msg['message-id']
    hackmsgid = True
    if msgid:
        mo = mcre.search(msgid)
        if mo:
            lname, hname = mo.group('listname', 'hostname')
            if lname == mlist.internal_name() and hname == mlist.host_name:
                hackmsgid = False
    if hackmsgid:
        del msg['message-id']
        msg['Message-ID'] = Utils.unique_message_id(mlist)
    # Lines: is useful
    if msg['Lines'] is None:
        # BAW: is there a better way?
        count = len(list(email.Iterators.body_line_iterator(msg)))
        msg['Lines'] = str(count)
    # Massage the message headers by remove some and rewriting others.  This
    # woon't completely sanitize the message, but it will eliminate the bulk
    # of the rejections based on message headers.  The NNTP server may still
    # reject the message because of other problems.
    for header in mm_cfg.NNTP_REMOVE_HEADERS:
        del msg[header]
    for header, rewrite in mm_cfg.NNTP_REWRITE_DUPLICATE_HEADERS:
        values = msg.get_all(header, [])
        if len(values) < 2:
            # We only care about duplicates
            continue
        del msg[header]
        # But keep the first one...
        msg[header] = values[0]
        for v in values[1:]:
            msg[rewrite] = v
    # Mark this message as prepared in case it has to be requeued
    msgdata['prepped'] = True
