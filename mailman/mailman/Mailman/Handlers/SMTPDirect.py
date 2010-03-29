# Copyright (C) 1998-2009 by the Free Software Foundation, Inc.
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

"""Local SMTP direct drop-off.

This module delivers messages via SMTP to a locally specified daemon.  This
should be compatible with any modern SMTP server.  It is expected that the MTA
handles all final delivery.  We have to play tricks so that the list object
isn't locked while delivery occurs synchronously.

Note: This file only handles single threaded delivery.  See SMTPThreaded.py
for a threaded implementation.
"""

import copy
import time
import socket
import smtplib
from types import UnicodeType

from Mailman import mm_cfg
from Mailman import Utils
from Mailman import Errors
from Mailman.Handlers import Decorate
from Mailman.Logging.Syslog import syslog
from Mailman.SafeDict import MsgSafeDict

import email
from email.Utils import formataddr
from email.Header import Header
from email.Charset import Charset

DOT = '.'

try:
    True, False
except NameError:
    True = 1
    False = 0



# Manage a connection to the SMTP server
class Connection:
    def __init__(self):
        self.__conn = None

    def __connect(self):
        self.__conn = smtplib.SMTP()
        self.__conn.connect(mm_cfg.SMTPHOST, mm_cfg.SMTPPORT)
        self.__numsessions = mm_cfg.SMTP_MAX_SESSIONS_PER_CONNECTION

    def sendmail(self, envsender, recips, msgtext):
        if self.__conn is None:
            self.__connect()
        try:
            results = self.__conn.sendmail(envsender, recips, msgtext)
        except smtplib.SMTPException:
            # For safety, close this connection.  The next send attempt will
            # automatically re-open it.  Pass the exception on up.
            self.quit()
            raise
        # This session has been successfully completed.
        self.__numsessions -= 1
        # By testing exactly for equality to 0, we automatically handle the
        # case for SMTP_MAX_SESSIONS_PER_CONNECTION <= 0 meaning never close
        # the connection.  We won't worry about wraparound <wink>.
        if self.__numsessions == 0:
            self.quit()
        return results

    def quit(self):
        if self.__conn is None:
            return
        try:
            self.__conn.quit()
        except smtplib.SMTPException:
            pass
        self.__conn = None



def process(mlist, msg, msgdata):
    recips = msgdata.get('recips')
    if not recips:
        # Nobody to deliver to!
        return
    # Calculate the non-VERP envelope sender.
    envsender = msgdata.get('envsender')
    if envsender is None:
        if mlist:
            envsender = mlist.GetBouncesEmail()
        else:
            envsender = Utils.get_site_email(extra='bounces')
    # Time to split up the recipient list.  If we're personalizing or VERPing
    # then each chunk will have exactly one recipient.  We'll then hand craft
    # an envelope sender and stitch a message together in memory for each one
    # separately.  If we're not VERPing, then we'll chunkify based on
    # SMTP_MAX_RCPTS.  Note that most MTAs have a limit on the number of
    # recipients they'll swallow in a single transaction.
    deliveryfunc = None
    if (not msgdata.has_key('personalize') or msgdata['personalize']) and (
           msgdata.get('verp') or mlist.personalize):
        chunks = [[recip] for recip in recips]
        msgdata['personalize'] = 1
        deliveryfunc = verpdeliver
    elif mm_cfg.SMTP_MAX_RCPTS <= 0:
        chunks = [recips]
    else:
        chunks = chunkify(recips, mm_cfg.SMTP_MAX_RCPTS)
    # See if this is an unshunted message for which some were undelivered
    if msgdata.has_key('undelivered'):
        chunks = msgdata['undelivered']
    # If we're doing bulk delivery, then we can stitch up the message now.
    if deliveryfunc is None:
        # Be sure never to decorate the message more than once!
        if not msgdata.get('decorated'):
            Decorate.process(mlist, msg, msgdata)
            msgdata['decorated'] = True
        deliveryfunc = bulkdeliver
    refused = {}
    t0 = time.time()
    # Open the initial connection
    origrecips = msgdata['recips']
    # MAS: get the message sender now for logging.  If we're using 'sender'
    # and not 'from', bulkdeliver changes it for bounce processing.  If we're
    # VERPing, it doesn't matter because bulkdeliver is working on a copy, but
    # otherwise msg gets changed.  If the list is anonymous, the original
    # sender is long gone, but Cleanse.py has logged it.
    origsender = msgdata.get('original_sender', msg.get_sender())
    # `undelivered' is a copy of chunks that we pop from to do deliveries.
    # This seems like a good tradeoff between robustness and resource
    # utilization.  If delivery really fails (i.e. qfiles/shunt type
    # failures), then we'll pick up where we left off with `undelivered'.
    # This means at worst, the last chunk for which delivery was attempted
    # could get duplicates but not every one, and no recips should miss the
    # message.
    conn = Connection()
    try:
        msgdata['undelivered'] = chunks
        while chunks:
            chunk = chunks.pop()
            msgdata['recips'] = chunk
            try:
                deliveryfunc(mlist, msg, msgdata, envsender, refused, conn)
            except Exception:
                # If /anything/ goes wrong, push the last chunk back on the
                # undelivered list and re-raise the exception.  We don't know
                # how many of the last chunk might receive the message, so at
                # worst, everyone in this chunk will get a duplicate.  Sigh.
                chunks.append(chunk)
                raise
        del msgdata['undelivered']
    finally:
        conn.quit()
        msgdata['recips'] = origrecips
    # Log the successful post
    t1 = time.time()
    d = MsgSafeDict(msg, {'time'    : t1-t0,
                          # BAW: Urg.  This seems inefficient.
                          'size'    : len(msg.as_string()),
                          '#recips' : len(recips),
                          '#refused': len(refused),
                          'listname': mlist.internal_name(),
                          'sender'  : origsender,
                          })
    # We have to use the copy() method because extended call syntax requires a
    # concrete dictionary object; it does not allow a generic mapping.  It's
    # still worthwhile doing the interpolation in syslog() because it'll catch
    # any catastrophic exceptions due to bogus format strings.
    if mm_cfg.SMTP_LOG_EVERY_MESSAGE:
        syslog.write_ex(mm_cfg.SMTP_LOG_EVERY_MESSAGE[0],
                        mm_cfg.SMTP_LOG_EVERY_MESSAGE[1], kws=d)

    if refused:
        if mm_cfg.SMTP_LOG_REFUSED:
            syslog.write_ex(mm_cfg.SMTP_LOG_REFUSED[0],
                            mm_cfg.SMTP_LOG_REFUSED[1], kws=d)

    elif msgdata.get('tolist'):
        # Log the successful post, but only if it really was a post to the
        # mailing list.  Don't log sends to the -owner, or -admin addrs.
        # -request addrs should never get here.  BAW: it may be useful to log
        # the other messages, but in that case, we should probably have a
        # separate configuration variable to control that.
        if mm_cfg.SMTP_LOG_SUCCESS:
            syslog.write_ex(mm_cfg.SMTP_LOG_SUCCESS[0],
                            mm_cfg.SMTP_LOG_SUCCESS[1], kws=d)

    # Process any failed deliveries.
    tempfailures = []
    permfailures = []
    for recip, (code, smtpmsg) in refused.items():
        # DRUMS is an internet draft, but it says:
        #
        #    [RFC-821] incorrectly listed the error where an SMTP server
        #    exhausts its implementation limit on the number of RCPT commands
        #    ("too many recipients") as having reply code 552.  The correct
        #    reply code for this condition is 452. Clients SHOULD treat a 552
        #    code in this case as a temporary, rather than permanent failure
        #    so the logic below works.
        #
        if code >= 500 and code <> 552:
            # A permanent failure
            permfailures.append(recip)
        else:
            # Deal with persistent transient failures by queuing them up for
            # future delivery.  TBD: this could generate lots of log entries!
            tempfailures.append(recip)
        if mm_cfg.SMTP_LOG_EACH_FAILURE:
            d.update({'recipient': recip,
                      'failcode' : code,
                      'failmsg'  : smtpmsg})
            syslog.write_ex(mm_cfg.SMTP_LOG_EACH_FAILURE[0],
                            mm_cfg.SMTP_LOG_EACH_FAILURE[1], kws=d)
    # Return the results
    if tempfailures or permfailures:
        raise Errors.SomeRecipientsFailed(tempfailures, permfailures)



def chunkify(recips, chunksize):
    # First do a simple sort on top level domain.  It probably doesn't buy us
    # much to try to sort on MX record -- that's the MTA's job.  We're just
    # trying to avoid getting a max recips error.  Split the chunks along
    # these lines (as suggested originally by Chuq Von Rospach and slightly
    # elaborated by BAW).
    chunkmap = {'com': 1,
                'net': 2,
                'org': 2,
                'edu': 3,
                'us' : 3,
                'ca' : 3,
                }
    buckets = {}
    for r in recips:
        tld = None
        i = r.rfind('.')
        if i >= 0:
            tld = r[i+1:]
        bin = chunkmap.get(tld, 0)
        bucket = buckets.get(bin, [])
        bucket.append(r)
        buckets[bin] = bucket
    # Now start filling the chunks
    chunks = []
    currentchunk = []
    chunklen = 0
    for bin in buckets.values():
        for r in bin:
            currentchunk.append(r)
            chunklen = chunklen + 1
            if chunklen >= chunksize:
                chunks.append(currentchunk)
                currentchunk = []
                chunklen = 0
        if currentchunk:
            chunks.append(currentchunk)
            currentchunk = []
            chunklen = 0
    return chunks



def verpdeliver(mlist, msg, msgdata, envsender, failures, conn):
    for recip in msgdata['recips']:
        # We now need to stitch together the message with its header and
        # footer.  If we're VERPIng, we have to calculate the envelope sender
        # for each recipient.  Note that the list of recipients must be of
        # length 1.
        #
        # BAW: ezmlm includes the message number in the envelope, used when
        # sending a notification to the user telling her how many messages
        # they missed due to bouncing.  Neat idea.
        msgdata['recips'] = [recip]
        # Make a copy of the message and decorate + delivery that
        msgcopy = copy.deepcopy(msg)
        Decorate.process(mlist, msgcopy, msgdata)
        # Calculate the envelope sender, which we may be VERPing
        if msgdata.get('verp'):
            bmailbox, bdomain = Utils.ParseEmail(envsender)
            rmailbox, rdomain = Utils.ParseEmail(recip)
            if rdomain is None:
                # The recipient address is not fully-qualified.  We can't
                # deliver it to this person, nor can we craft a valid verp
                # header.  I don't think there's much we can do except ignore
                # this recipient.
                syslog('smtp', 'Skipping VERP delivery to unqual recip: %s',
                       recip)
                continue
            d = {'bounces': bmailbox,
                 'mailbox': rmailbox,
                 'host'   : DOT.join(rdomain),
                 }
            envsender = '%s@%s' % ((mm_cfg.VERP_FORMAT % d), DOT.join(bdomain))
        if mlist.personalize == 2:
            # When fully personalizing, we want the To address to point to the
            # recipient, not to the mailing list
            del msgcopy['to']
            name = None
            if mlist.isMember(recip):
                name = mlist.getMemberName(recip)
            if name:
                # Convert the name to an email-safe representation.  If the
                # name is a byte string, convert it first to Unicode, given
                # the character set of the member's language, replacing bad
                # characters for which we can do nothing about.  Once we have
                # the name as Unicode, we can create a Header instance for it
                # so that it's properly encoded for email transport.
                charset = Utils.GetCharSet(mlist.getMemberLanguage(recip))
                if charset == 'us-ascii':
                    # Since Header already tries both us-ascii and utf-8,
                    # let's add something a bit more useful.
                    charset = 'iso-8859-1'
                charset = Charset(charset)
                codec = charset.input_codec or 'ascii'
                if not isinstance(name, UnicodeType):
                    name = unicode(name, codec, 'replace')
                name = Header(name, charset).encode()
                msgcopy['To'] = formataddr((name, recip))
            else:
                msgcopy['To'] = recip
        # We can flag the mail as a duplicate for each member, if they've
        # already received this message, as calculated by Message-ID.  See
        # AvoidDuplicates.py for details.
        del msgcopy['x-mailman-copy']
        if msgdata.get('add-dup-header', {}).has_key(recip):
            msgcopy['X-Mailman-Copy'] = 'yes'
        # For the final delivery stage, we can just bulk deliver to a party of
        # one. ;)
        bulkdeliver(mlist, msgcopy, msgdata, envsender, failures, conn)



def bulkdeliver(mlist, msg, msgdata, envsender, failures, conn):
    # Do some final cleanup of the message header.  Start by blowing away
    # any the Sender: and Errors-To: headers so remote MTAs won't be
    # tempted to delivery bounces there instead of our envelope sender
    #
    # BAW An interpretation of RFCs 2822 and 2076 could argue for not touching
    # the Sender header at all.  Brad Knowles points out that MTAs tend to
    # wipe existing Return-Path headers, and old MTAs may still honor
    # Errors-To while new ones will at worst ignore the header.
    del msg['sender']
    del msg['errors-to']
    msg['Sender'] = envsender
    msg['Errors-To'] = envsender
    # Get the plain, flattened text of the message, sans unixfrom
    # using our as_string() method to not mangle From_ and not fold
    # sub-part headers possibly breaking signatures.
    msgtext = msg.as_string(mangle_from_=False)
    refused = {}
    recips = msgdata['recips']
    msgid = msg['message-id']
    try:
        # Send the message
        refused = conn.sendmail(envsender, recips, msgtext)
    except smtplib.SMTPRecipientsRefused, e:
        syslog('smtp-failure', 'All recipients refused: %s, msgid: %s',
               e, msgid)
        refused = e.recipients
    except smtplib.SMTPResponseException, e:
        syslog('smtp-failure', 'SMTP session failure: %s, %s, msgid: %s',
               e.smtp_code, e.smtp_error, msgid)
        # If this was a permanent failure, don't add the recipients to the
        # refused, because we don't want them to be added to failures.
        # Otherwise, if the MTA rejects the message because of the message
        # content (e.g. it's spam, virii, or has syntactic problems), then
        # this will end up registering a bounce score for every recipient.
        # Definitely /not/ what we want.
        if e.smtp_code < 500 or e.smtp_code == 552:
            # It's a temporary failure
            for r in recips:
                refused[r] = (e.smtp_code, e.smtp_error)
    except (socket.error, IOError, smtplib.SMTPException), e:
        # MTA not responding, or other socket problems, or any other kind of
        # SMTPException.  In that case, nothing got delivered, so treat this
        # as a temporary failure.
        syslog('smtp-failure', 'Low level smtp error: %s, msgid: %s', e, msgid)
        error = str(e)
        for r in recips:
            refused[r] = (-1, error)
    failures.update(refused)
