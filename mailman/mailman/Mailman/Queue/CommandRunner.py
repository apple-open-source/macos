# Copyright (C) 1998-2007 by the Free Software Foundation, Inc.
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

"""-request robot command queue runner."""

# See the delivery diagram in IncomingRunner.py.  This module handles all
# email destined for mylist-request, -join, and -leave.  It no longer handles
# bounce messages (i.e. -admin or -bounces), nor does it handle mail to
# -owner.



# BAW: get rid of this when we Python 2.2 is a minimum requirement.
from __future__ import nested_scopes

import re
import sys
from types import StringType, UnicodeType

from Mailman import mm_cfg
from Mailman import Utils
from Mailman import Message
from Mailman.Handlers import Replybot
from Mailman.i18n import _
from Mailman.Queue.Runner import Runner
from Mailman.Logging.Syslog import syslog
from Mailman import LockFile

from email.Header import decode_header, make_header, Header
from email.Errors import HeaderParseError
from email.Iterators import typed_subpart_iterator
from email.MIMEText import MIMEText
from email.MIMEMessage import MIMEMessage

NL = '\n'

try:
    True, False
except NameError:
    True = 1
    False = 0



class Results:
    def __init__(self, mlist, msg, msgdata):
        self.mlist = mlist
        self.msg = msg
        self.msgdata = msgdata
        # Only set returnaddr if the response is to go to someone other than
        # the address specified in the From: header (e.g. for the password
        # command).
        self.returnaddr = None
        self.commands = []
        self.results = []
        self.ignored = []
        self.lineno = 0
        self.subjcmdretried = 0
        self.respond = True
        # Extract the subject header and do RFC 2047 decoding.  Note that
        # Python 2.1's unicode() builtin doesn't call obj.__unicode__().
        subj = msg.get('subject', '')
        try:
            subj = make_header(decode_header(subj)).__unicode__()
            # TK: Currently we don't allow 8bit or multibyte in mail command.
            subj = subj.encode('us-ascii')
            # Always process the Subject: header first
            self.commands.append(subj)
        except (HeaderParseError, UnicodeError, LookupError):
            # We couldn't parse it so ignore the Subject header
            pass
        # Find the first text/plain part
        part = None
        for part in typed_subpart_iterator(msg, 'text', 'plain'):
            break
        if part is None or part is not msg:
            # Either there was no text/plain part or we ignored some
            # non-text/plain parts.
            self.results.append(_('Ignoring non-text/plain MIME parts'))
        if part is None:
            # E.g the outer Content-Type: was text/html
            return
        body = part.get_payload(decode=True)
        # text/plain parts better have string payloads
        assert isinstance(body, StringType) or isinstance(body, UnicodeType)
        lines = body.splitlines()
        # Use no more lines than specified
        self.commands.extend(lines[:mm_cfg.DEFAULT_MAIL_COMMANDS_MAX_LINES])
        self.ignored.extend(lines[mm_cfg.DEFAULT_MAIL_COMMANDS_MAX_LINES:])

    def process(self):
        # Now, process each line until we find an error.  The first
        # non-command line found stops processing.
        stop = False
        for line in self.commands:
            if line and line.strip():
                args = line.split()
                cmd = args.pop(0).lower()
                stop = self.do_command(cmd, args)
            self.lineno += 1
            if stop:
                break

    def do_command(self, cmd, args=None):
        if args is None:
            args = ()
        # Try to import a command handler module for this command
        modname = 'Mailman.Commands.cmd_' + cmd
        try:
            __import__(modname)
            handler = sys.modules[modname]
        # ValueError can be raised if cmd has dots in it.
        except (ImportError, ValueError):
            # If we're on line zero, it was the Subject: header that didn't
            # contain a command.  It's possible there's a Re: prefix (or
            # localized version thereof) on the Subject: line that's messing
            # things up.  Pop the prefix off and try again... once.
            #
            # If that still didn't work it isn't enough to stop processing.
            # BAW: should we include a message that the Subject: was ignored?
            if not self.subjcmdretried and args:
                self.subjcmdretried += 1
                cmd = args.pop(0)
                return self.do_command(cmd, args)
            return self.lineno <> 0
        return handler.process(self, args)

    def send_response(self):
        # Helper
        def indent(lines):
            return ['    ' + line for line in lines]
        # Quick exit for some commands which don't need a response
        if not self.respond:
            return
        resp = [Utils.wrap(_("""\
The results of your email command are provided below.
Attached is your original message.
"""))]
        if self.results:
            resp.append(_('- Results:'))
            resp.extend(indent(self.results))
        # Ignore empty lines
        unprocessed = [line for line in self.commands[self.lineno:]
                       if line and line.strip()]
        if unprocessed:
            resp.append(_('\n- Unprocessed:'))
            resp.extend(indent(unprocessed))
        if not unprocessed and not self.results:
            # The user sent an empty message; return a helpful one.
            resp.append(Utils.wrap(_("""\
No commands were found in this message.
To obtain instructions, send a message containing just the word "help".
""")))
        if self.ignored:
            resp.append(_('\n- Ignored:'))
            resp.extend(indent(self.ignored))
        resp.append(_('\n- Done.\n\n'))
        # Encode any unicode strings into the list charset, so we don't try to
        # join unicode strings and invalid ASCII.
        charset = Utils.GetCharSet(self.msgdata['lang'])
        encoded_resp = []
        for item in resp:
            if isinstance(item, UnicodeType):
                item = item.encode(charset, 'replace')
            encoded_resp.append(item)
        results = MIMEText(NL.join(encoded_resp), _charset=charset)
        # Safety valve for mail loops with misconfigured email 'bots.  We
        # don't respond to commands sent with "Precedence: bulk|junk|list"
        # unless they explicitly "X-Ack: yes", but not all mail 'bots are
        # correctly configured, so we max out the number of responses we'll
        # give to an address in a single day.
        #
        # BAW: We wait until now to make this decision since our sender may
        # not be self.msg.get_sender(), but I'm not sure this is right.
        recip = self.returnaddr or self.msg.get_sender()
        if not self.mlist.autorespondToSender(recip, self.msgdata['lang']):
            return
        msg = Message.UserNotification(
            recip,
            self.mlist.GetBouncesEmail(),
            _('The results of your email commands'),
            lang=self.msgdata['lang'])
        msg.set_type('multipart/mixed')
        msg.attach(results)
        orig = MIMEMessage(self.msg)
        msg.attach(orig)
        msg.send(self.mlist)



class CommandRunner(Runner):
    QDIR = mm_cfg.CMDQUEUE_DIR

    def _dispose(self, mlist, msg, msgdata):
        # The policy here is similar to the Replybot policy.  If a message has
        # "Precedence: bulk|junk|list" and no "X-Ack: yes" header, we discard
        # it to prevent replybot response storms.
        precedence = msg.get('precedence', '').lower()
        ack = msg.get('x-ack', '').lower()
        if ack <> 'yes' and precedence in ('bulk', 'junk', 'list'):
            syslog('vette', 'Precedence: %s message discarded by: %s',
                   precedence, mlist.GetRequestEmail())
            return False
        # Do replybot for commands
        mlist.Load()
        Replybot.process(mlist, msg, msgdata)
        if mlist.autorespond_requests == 1:
            syslog('vette', 'replied and discard')
            # w/discard
            return False
        # Now craft the response
        res = Results(mlist, msg, msgdata)
        # BAW: Not all the functions of this qrunner require the list to be
        # locked.  Still, it's more convenient to lock it here and now and
        # deal with lock failures in one place.
        try:
            mlist.Lock(timeout=mm_cfg.LIST_LOCK_TIMEOUT)
        except LockFile.TimeOutError:
            # Oh well, try again later
            return True
        # This message will have been delivered to one of mylist-request,
        # mylist-join, or mylist-leave, and the message metadata will contain
        # a key to which one was used.
        try:
            if msgdata.get('torequest'):
                res.process()
            elif msgdata.get('tojoin'):
                res.do_command('join')
            elif msgdata.get('toleave'):
                res.do_command('leave')
            elif msgdata.get('toconfirm'):
                mo = re.match(mm_cfg.VERP_CONFIRM_REGEXP, msg.get('to', ''))
                if mo:
                    res.do_command('confirm', (mo.group('cookie'),))
            res.send_response()
            mlist.Save()
        finally:
            mlist.Unlock()
