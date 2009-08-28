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

"""Unit tests for the various Mailman/Handlers/*.py modules.
"""

import os
import time
import email
import errno
import cPickle
import unittest
from types import ListType
from email.Generator import Generator

from Mailman import mm_cfg
from Mailman.MailList import MailList
from Mailman import Message
from Mailman import Errors
from Mailman import Pending
from Mailman.Queue.Switchboard import Switchboard

from Mailman.Handlers import Acknowledge
from Mailman.Handlers import AfterDelivery
from Mailman.Handlers import Approve
from Mailman.Handlers import CalcRecips
from Mailman.Handlers import Cleanse
from Mailman.Handlers import CookHeaders
from Mailman.Handlers import Decorate
from Mailman.Handlers import FileRecips
from Mailman.Handlers import Hold
from Mailman.Handlers import MimeDel
from Mailman.Handlers import Moderate
from Mailman.Handlers import Replybot
# Don't test handlers such as SMTPDirect and Sendmail here
from Mailman.Handlers import SpamDetect
from Mailman.Handlers import Tagger
from Mailman.Handlers import ToArchive
from Mailman.Handlers import ToDigest
from Mailman.Handlers import ToOutgoing
from Mailman.Handlers import ToUsenet
from Mailman.Utils import sha_new

from TestBase import TestBase



def password(plaintext):
    return sha_new(plaintext).hexdigest()



class TestAcknowledge(TestBase):
    def setUp(self):
        TestBase.setUp(self)
        # We're going to want to inspect this queue directory
        self._sb = Switchboard(mm_cfg.VIRGINQUEUE_DIR)
        # Add a member
        self._mlist.addNewMember('aperson@dom.ain')
        self._mlist.personalize = False

    def tearDown(self):
        for f in os.listdir(mm_cfg.VIRGINQUEUE_DIR):
            os.unlink(os.path.join(mm_cfg.VIRGINQUEUE_DIR, f))
        TestBase.tearDown(self)

    def test_no_ack_msgdata(self):
        eq = self.assertEqual
        # Make sure there are no files in the virgin queue already
        eq(len(self._sb.files()), 0)
        msg = email.message_from_string("""\
From: aperson@dom.ain

""", Message.Message)
        Acknowledge.process(self._mlist, msg,
                            {'original_sender': 'aperson@dom.ain'})
        eq(len(self._sb.files()), 0)

    def test_no_ack_not_a_member(self):
        eq = self.assertEqual
        # Make sure there are no files in the virgin queue already
        eq(len(self._sb.files()), 0)
        msg = email.message_from_string("""\
From: bperson@dom.ain

""", Message.Message)
        Acknowledge.process(self._mlist, msg,
                            {'original_sender': 'bperson@dom.ain'})
        eq(len(self._sb.files()), 0)

    def test_no_ack_sender(self):
        eq = self.assertEqual
        eq(len(self._sb.files()), 0)
        msg = email.message_from_string("""\
From: aperson@dom.ain

""", Message.Message)
        Acknowledge.process(self._mlist, msg, {})
        eq(len(self._sb.files()), 0)

    def test_ack_no_subject(self):
        eq = self.assertEqual
        self._mlist.setMemberOption(
            'aperson@dom.ain', mm_cfg.AcknowledgePosts, 1)
        eq(len(self._sb.files()), 0)
        msg = email.message_from_string("""\
From: aperson@dom.ain

""", Message.Message)
        Acknowledge.process(self._mlist, msg, {})
        files = self._sb.files()
        eq(len(files), 1)
        qmsg, qdata = self._sb.dequeue(files[0])
        # Check the .db file
        eq(qdata.get('listname'), '_xtest')
        eq(qdata.get('recips'), ['aperson@dom.ain'])
        eq(qdata.get('version'), 3)
        # Check the .pck
        eq(str(str(qmsg['subject'])), '_xtest post acknowledgement')
        eq(qmsg['to'], 'aperson@dom.ain')
        eq(qmsg['from'], '_xtest-bounces@dom.ain')
        eq(qmsg.get_content_type(), 'text/plain')
        eq(qmsg.get_param('charset'), 'us-ascii')
        msgid = qmsg['message-id']
        self.failUnless(msgid.startswith('<mailman.'))
        self.failUnless(msgid.endswith('._xtest@dom.ain>'))
        eq(qmsg.get_payload(), """\
Your message entitled

    (no subject)

was successfully received by the _xtest mailing list.

List info page: http://www.dom.ain/mailman/listinfo/_xtest
Your preferences: http://www.dom.ain/mailman/options/_xtest/aperson%40dom.ain
""")
        # Make sure we dequeued the only message
        eq(len(self._sb.files()), 0)

    def test_ack_with_subject(self):
        eq = self.assertEqual
        self._mlist.setMemberOption(
            'aperson@dom.ain', mm_cfg.AcknowledgePosts, 1)
        eq(len(self._sb.files()), 0)
        msg = email.message_from_string("""\
From: aperson@dom.ain
Subject: Wish you were here

""", Message.Message)
        Acknowledge.process(self._mlist, msg, {})
        files = self._sb.files()
        eq(len(files), 1)
        qmsg, qdata = self._sb.dequeue(files[0])
        # Check the .db file
        eq(qdata.get('listname'), '_xtest')
        eq(qdata.get('recips'), ['aperson@dom.ain'])
        eq(qdata.get('version'), 3)
        # Check the .pck
        eq(str(qmsg['subject']), '_xtest post acknowledgement')
        eq(qmsg['to'], 'aperson@dom.ain')
        eq(qmsg['from'], '_xtest-bounces@dom.ain')
        eq(qmsg.get_content_type(), 'text/plain')
        eq(qmsg.get_param('charset'), 'us-ascii')
        msgid = qmsg['message-id']
        self.failUnless(msgid.startswith('<mailman.'))
        self.failUnless(msgid.endswith('._xtest@dom.ain>'))
        eq(qmsg.get_payload(), """\
Your message entitled

    Wish you were here

was successfully received by the _xtest mailing list.

List info page: http://www.dom.ain/mailman/listinfo/_xtest
Your preferences: http://www.dom.ain/mailman/options/_xtest/aperson%40dom.ain
""")
        # Make sure we dequeued the only message
        eq(len(self._sb.files()), 0)



class TestAfterDelivery(TestBase):
    # Both msg and msgdata are ignored
    def test_process(self):
        mlist = self._mlist
        last_post_time = mlist.last_post_time
        post_id = mlist.post_id
        AfterDelivery.process(mlist, None, None)
        self.failUnless(mlist.last_post_time > last_post_time)
        self.assertEqual(mlist.post_id, post_id + 1)



class TestApprove(TestBase):
    def test_short_circuit(self):
        msgdata = {'approved': 1}
        rtn = Approve.process(self._mlist, None, msgdata)
        # Not really a great test, but there's little else to assert
        self.assertEqual(rtn, None)

    def test_approved_moderator(self):
        mlist = self._mlist
        mlist.mod_password = password('wazoo')
        msg = email.message_from_string("""\
Approved: wazoo

""")
        msgdata = {}
        Approve.process(mlist, msg, msgdata)
        self.failUnless(msgdata.has_key('approved'))
        self.assertEqual(msgdata['approved'], 1)

    def test_approve_moderator(self):
        mlist = self._mlist
        mlist.mod_password = password('wazoo')
        msg = email.message_from_string("""\
Approve: wazoo

""")
        msgdata = {}
        Approve.process(mlist, msg, msgdata)
        self.failUnless(msgdata.has_key('approved'))
        self.assertEqual(msgdata['approved'], 1)

    def test_approved_admin(self):
        mlist = self._mlist
        mlist.password = password('wazoo')
        msg = email.message_from_string("""\
Approved: wazoo

""")
        msgdata = {}
        Approve.process(mlist, msg, msgdata)
        self.failUnless(msgdata.has_key('approved'))
        self.assertEqual(msgdata['approved'], 1)

    def test_approve_admin(self):
        mlist = self._mlist
        mlist.password = password('wazoo')
        msg = email.message_from_string("""\
Approve: wazoo

""")
        msgdata = {}
        Approve.process(mlist, msg, msgdata)
        self.failUnless(msgdata.has_key('approved'))
        self.assertEqual(msgdata['approved'], 1)

    def test_unapproved(self):
        mlist = self._mlist
        mlist.password = password('zoowa')
        msg = email.message_from_string("""\
Approve: wazoo

""")
        msgdata = {}
        Approve.process(mlist, msg, msgdata)
        self.assertEqual(msgdata.get('approved'), None)

    def test_trip_beentheres(self):
        mlist = self._mlist
        msg = email.message_from_string("""\
X-BeenThere: %s

""" % mlist.GetListEmail())
        self.assertRaises(Errors.LoopError, Approve.process, mlist, msg, {})



class TestCalcRecips(TestBase):
    def setUp(self):
        TestBase.setUp(self)
        # Add a bunch of regular members
        mlist = self._mlist
        mlist.addNewMember('aperson@dom.ain')
        mlist.addNewMember('bperson@dom.ain')
        mlist.addNewMember('cperson@dom.ain')
        # And a bunch of digest members
        mlist.addNewMember('dperson@dom.ain', digest=1)
        mlist.addNewMember('eperson@dom.ain', digest=1)
        mlist.addNewMember('fperson@dom.ain', digest=1)

    def test_short_circuit(self):
        msgdata = {'recips': 1}
        rtn = CalcRecips.process(self._mlist, None, msgdata)
        # Not really a great test, but there's little else to assert
        self.assertEqual(rtn, None)

    def test_simple_path(self):
        msgdata = {}
        msg = email.message_from_string("""\
From: dperson@dom.ain

""", Message.Message)
        CalcRecips.process(self._mlist, msg, msgdata)
        self.failUnless(msgdata.has_key('recips'))
        recips = msgdata['recips']
        recips.sort()
        self.assertEqual(recips, ['aperson@dom.ain', 'bperson@dom.ain',
                                  'cperson@dom.ain'])

    def test_exclude_sender(self):
        msgdata = {}
        msg = email.message_from_string("""\
From: cperson@dom.ain

""", Message.Message)
        self._mlist.setMemberOption('cperson@dom.ain',
                                    mm_cfg.DontReceiveOwnPosts, 1)
        CalcRecips.process(self._mlist, msg, msgdata)
        self.failUnless(msgdata.has_key('recips'))
        recips = msgdata['recips']
        recips.sort()
        self.assertEqual(recips, ['aperson@dom.ain', 'bperson@dom.ain'])

    def test_urgent_moderator(self):
        self._mlist.mod_password = password('xxXXxx')
        msgdata = {}
        msg = email.message_from_string("""\
From: dperson@dom.ain
Urgent: xxXXxx

""", Message.Message)
        CalcRecips.process(self._mlist, msg, msgdata)
        self.failUnless(msgdata.has_key('recips'))
        recips = msgdata['recips']
        recips.sort()
        self.assertEqual(recips, ['aperson@dom.ain', 'bperson@dom.ain',
                                  'cperson@dom.ain', 'dperson@dom.ain',
                                  'eperson@dom.ain', 'fperson@dom.ain'])

    def test_urgent_admin(self):
        self._mlist.mod_password = password('yyYYyy')
        self._mlist.password = password('xxXXxx')
        msgdata = {}
        msg = email.message_from_string("""\
From: dperson@dom.ain
Urgent: xxXXxx

""", Message.Message)
        CalcRecips.process(self._mlist, msg, msgdata)
        self.failUnless(msgdata.has_key('recips'))
        recips = msgdata['recips']
        recips.sort()
        self.assertEqual(recips, ['aperson@dom.ain', 'bperson@dom.ain',
                                  'cperson@dom.ain', 'dperson@dom.ain',
                                  'eperson@dom.ain', 'fperson@dom.ain'])

    def test_urgent_reject(self):
        self._mlist.mod_password = password('yyYYyy')
        self._mlist.password = password('xxXXxx')
        msgdata = {}
        msg = email.message_from_string("""\
From: dperson@dom.ain
Urgent: zzZZzz

""", Message.Message)
        self.assertRaises(Errors.RejectMessage,
                          CalcRecips.process,
                          self._mlist, msg, msgdata)

    # BAW: must test the do_topic_filters() path...



class TestCleanse(TestBase):
    def setUp(self):
        TestBase.setUp(self)

    def test_simple_cleanse(self):
        eq = self.assertEqual
        msg = email.message_from_string("""\
From: aperson@dom.ain
Approved: yes
Urgent: indeed
Reply-To: bperson@dom.ain
Sender: asystem@dom.ain
Return-Receipt-To: another@dom.ain
Disposition-Notification-To: athird@dom.ain
X-Confirm-Reading-To: afourth@dom.ain
X-PMRQC: afifth@dom.ain
Subject: a message to you

""", Message.Message)
        Cleanse.process(self._mlist, msg, {})
        eq(msg['approved'], None)
        eq(msg['urgent'], None)
        eq(msg['return-receipt-to'], None)
        eq(msg['disposition-notification-to'], None)
        eq(msg['x-confirm-reading-to'], None)
        eq(msg['x-pmrqc'], None)
        eq(msg['from'], 'aperson@dom.ain')
        eq(msg['reply-to'], 'bperson@dom.ain')
        eq(msg['sender'], 'asystem@dom.ain')
        eq(msg['subject'], 'a message to you')

    def test_anon_cleanse(self):
        eq = self.assertEqual
        msg = email.message_from_string("""\
From: aperson@dom.ain
Approved: yes
Urgent: indeed
Reply-To: bperson@dom.ain
Sender: asystem@dom.ain
Return-Receipt-To: another@dom.ain
Disposition-Notification-To: athird@dom.ain
X-Confirm-Reading-To: afourth@dom.ain
X-PMRQC: afifth@dom.ain
Subject: a message to you

""", Message.Message)
        self._mlist.anonymous_list = 1
        Cleanse.process(self._mlist, msg, {})
        eq(msg['approved'], None)
        eq(msg['urgent'], None)
        eq(msg['return-receipt-to'], None)
        eq(msg['disposition-notification-to'], None)
        eq(msg['x-confirm-reading-to'], None)
        eq(msg['x-pmrqc'], None)
        eq(len(msg.get_all('from')), 1)
        eq(len(msg.get_all('reply-to')), 1)
        eq(msg['from'], '_xtest@dom.ain')
        eq(msg['reply-to'], '_xtest@dom.ain')
        eq(msg['sender'], None)
        eq(msg['subject'], 'a message to you')



class TestCookHeaders(TestBase):
    def test_transform_noack_to_xack(self):
        eq = self.assertEqual
        msg = email.message_from_string("""\
X-Ack: yes

""", Message.Message)
        CookHeaders.process(self._mlist, msg, {'noack': 1})
        eq(len(msg.get_all('x-ack')), 1)
        eq(msg['x-ack'], 'no')

    def test_original_sender(self):
        msg = email.message_from_string("""\
From: aperson@dom.ain

""", Message.Message)
        msgdata = {}
        CookHeaders.process(self._mlist, msg, msgdata)
        self.assertEqual(msgdata.get('original_sender'), 'aperson@dom.ain')

    def test_no_original_sender(self):
        msg = email.message_from_string("""\
Subject: about this message

""", Message.Message)
        msgdata = {}
        CookHeaders.process(self._mlist, msg, msgdata)
        self.assertEqual(msgdata.get('original_sender'), '')

    def test_xbeenthere(self):
        msg = email.message_from_string("""\
From: aperson@dom.ain

""", Message.Message)
        CookHeaders.process(self._mlist, msg, {})
        self.assertEqual(msg['x-beenthere'], '_xtest@dom.ain')

    def test_multiple_xbeentheres(self):
        eq = self.assertEqual
        msg = email.message_from_string("""\
From: aperson@dom.ain
X-BeenThere: alist@another.dom.ain

""", Message.Message)
        CookHeaders.process(self._mlist, msg, {})
        eq(len(msg.get_all('x-beenthere')), 2)
        beentheres = msg.get_all('x-beenthere')
        beentheres.sort()
        eq(beentheres, ['_xtest@dom.ain', 'alist@another.dom.ain'])

    def test_nonexisting_mmversion(self):
        eq = self.assertEqual
        msg = email.message_from_string("""\
From: aperson@dom.ain

""", Message.Message)
        CookHeaders.process(self._mlist, msg, {})
        eq(msg['x-mailman-version'], mm_cfg.VERSION)

    def test_existing_mmversion(self):
        eq = self.assertEqual
        msg = email.message_from_string("""\
From: aperson@dom.ain
X-Mailman-Version: 3000

""", Message.Message)
        CookHeaders.process(self._mlist, msg, {})
        eq(len(msg.get_all('x-mailman-version')), 1)
        eq(msg['x-mailman-version'], '3000')

    def test_nonexisting_precedence(self):
        eq = self.assertEqual
        msg = email.message_from_string("""\
From: aperson@dom.ain

""", Message.Message)
        CookHeaders.process(self._mlist, msg, {})
        eq(msg['precedence'], 'list')

    def test_existing_precedence(self):
        eq = self.assertEqual
        msg = email.message_from_string("""\
From: aperson@dom.ain
Precedence: junk

""", Message.Message)
        CookHeaders.process(self._mlist, msg, {})
        eq(len(msg.get_all('precedence')), 1)
        eq(msg['precedence'], 'junk')

    def test_subject_munging_no_subject(self):
        self._mlist.subject_prefix = '[XTEST] '
        msg = email.message_from_string("""\
From: aperson@dom.ain

""", Message.Message)
        msgdata = {}
        CookHeaders.process(self._mlist, msg, msgdata)
        self.assertEqual(msgdata.get('origsubj'), '')
        self.assertEqual(str(msg['subject']), '[XTEST] (no subject)')

    def test_subject_munging(self):
        self._mlist.subject_prefix = '[XTEST] '
        msg = email.message_from_string("""\
From: aperson@dom.ain
Subject: About Mailman...

""", Message.Message)
        CookHeaders.process(self._mlist, msg, {})
        self.assertEqual(msg['subject'], '[XTEST] About Mailman...')

    def test_no_subject_munging_for_digests(self):
        self._mlist.subject_prefix = '[XTEST] '
        msg = email.message_from_string("""\
From: aperson@dom.ain
Subject: About Mailman...

""", Message.Message)
        CookHeaders.process(self._mlist, msg, {'isdigest': 1})
        self.assertEqual(msg['subject'], 'About Mailman...')

    def test_no_subject_munging_for_fasttrack(self):
        self._mlist.subject_prefix = '[XTEST] '
        msg = email.message_from_string("""\
From: aperson@dom.ain
Subject: About Mailman...

""", Message.Message)
        CookHeaders.process(self._mlist, msg, {'_fasttrack': 1})
        self.assertEqual(msg['subject'], 'About Mailman...')

    def test_no_subject_munging_has_prefix(self):
        self._mlist.subject_prefix = '[XTEST] '
        msg = email.message_from_string("""\
From: aperson@dom.ain
Subject: Re: [XTEST] About Mailman...

""", Message.Message)
        CookHeaders.process(self._mlist, msg, {})
        self.assertEqual(msg['subject'], 'Re: [XTEST] About Mailman...')

    def test_reply_to_list(self):
        eq = self.assertEqual
        mlist = self._mlist
        mlist.reply_goes_to_list = 1
        msg = email.message_from_string("""\
From: aperson@dom.ain

""", Message.Message)
        CookHeaders.process(mlist, msg, {})
        eq(msg['reply-to'], '_xtest@dom.ain')
        eq(msg.get_all('reply-to'), ['_xtest@dom.ain'])

    def test_reply_to_list_with_strip(self):
        eq = self.assertEqual
        mlist = self._mlist
        mlist.reply_goes_to_list = 1
        mlist.first_strip_reply_to = 1
        msg = email.message_from_string("""\
From: aperson@dom.ain
Reply-To: bperson@dom.ain

""", Message.Message)
        CookHeaders.process(mlist, msg, {})
        eq(msg['reply-to'], '_xtest@dom.ain')
        eq(msg.get_all('reply-to'), ['_xtest@dom.ain'])

    def test_reply_to_explicit(self):
        eq = self.assertEqual
        mlist = self._mlist
        mlist.reply_goes_to_list = 2
        mlist.reply_to_address = 'mlist@dom.ain'
        msg = email.message_from_string("""\
From: aperson@dom.ain

""", Message.Message)
        CookHeaders.process(mlist, msg, {})
        eq(msg['reply-to'], 'mlist@dom.ain')
        eq(msg.get_all('reply-to'), ['mlist@dom.ain'])

    def test_reply_to_explicit_with_strip(self):
        eq = self.assertEqual
        mlist = self._mlist
        mlist.reply_goes_to_list = 2
        mlist.first_strip_reply_to = 1
        mlist.reply_to_address = 'mlist@dom.ain'
        msg = email.message_from_string("""\
From: aperson@dom.ain
Reply-To: bperson@dom.ain

""", Message.Message)
        CookHeaders.process(self._mlist, msg, {})
        eq(msg['reply-to'], 'mlist@dom.ain')
        eq(msg.get_all('reply-to'), ['mlist@dom.ain'])

    def test_reply_to_extends_to_list(self):
        eq = self.assertEqual
        mlist = self._mlist
        mlist.reply_goes_to_list = 1
        mlist.first_strip_reply_to = 0
        msg = email.message_from_string("""\
From: aperson@dom.ain
Reply-To: bperson@dom.ain

""", Message.Message)
        CookHeaders.process(mlist, msg, {})
        eq(msg['reply-to'], 'bperson@dom.ain, _xtest@dom.ain')

    def test_reply_to_extends_to_explicit(self):
        eq = self.assertEqual
        mlist = self._mlist
        mlist.reply_goes_to_list = 2
        mlist.first_strip_reply_to = 0
        mlist.reply_to_address = 'mlist@dom.ain'
        msg = email.message_from_string("""\
From: aperson@dom.ain
Reply-To: bperson@dom.ain

""", Message.Message)
        CookHeaders.process(mlist, msg, {})
        eq(msg['reply-to'], 'mlist@dom.ain, bperson@dom.ain')

    def test_list_headers_nolist(self):
        eq = self.assertEqual
        msg = email.message_from_string("""\
From: aperson@dom.ain

""", Message.Message)
        CookHeaders.process(self._mlist, msg, {'_nolist': 1})
        eq(msg['list-id'], None)
        eq(msg['list-help'], None)
        eq(msg['list-unsubscribe'], None)
        eq(msg['list-subscribe'], None)
        eq(msg['list-post'], None)
        eq(msg['list-archive'], None)

    def test_list_headers(self):
        eq = self.assertEqual
        self._mlist.archive = 1
        msg = email.message_from_string("""\
From: aperson@dom.ain

""", Message.Message)
        oldval = mm_cfg.DEFAULT_URL_HOST
        mm_cfg.DEFAULT_URL_HOST = 'www.dom.ain'
        try:
            CookHeaders.process(self._mlist, msg, {})
        finally:
            mm_cfg.DEFAULT_URL_HOST = oldval
        eq(msg['list-id'], '<_xtest.dom.ain>')
        eq(msg['list-help'], '<mailto:_xtest-request@dom.ain?subject=help>')
        eq(msg['list-unsubscribe'],
           '<http://www.dom.ain/mailman/listinfo/_xtest>,'
           '\n\t<mailto:_xtest-request@dom.ain?subject=unsubscribe>')
        eq(msg['list-subscribe'],
           '<http://www.dom.ain/mailman/listinfo/_xtest>,'
           '\n\t<mailto:_xtest-request@dom.ain?subject=subscribe>')
        eq(msg['list-post'], '<mailto:_xtest@dom.ain>')
        eq(msg['list-archive'], '<http://www.dom.ain/pipermail/_xtest>')

    def test_list_headers_with_description(self):
        eq = self.assertEqual
        self._mlist.archive = 1
        self._mlist.description = 'A Test List'
        msg = email.message_from_string("""\
From: aperson@dom.ain

""", Message.Message)
        CookHeaders.process(self._mlist, msg, {})
        eq(msg['list-id'].__unicode__(), 'A Test List <_xtest.dom.ain>')
        eq(msg['list-help'], '<mailto:_xtest-request@dom.ain?subject=help>')
        eq(msg['list-unsubscribe'],
           '<http://www.dom.ain/mailman/listinfo/_xtest>,'
           '\n\t<mailto:_xtest-request@dom.ain?subject=unsubscribe>')
        eq(msg['list-subscribe'],
           '<http://www.dom.ain/mailman/listinfo/_xtest>,'
           '\n\t<mailto:_xtest-request@dom.ain?subject=subscribe>')
        eq(msg['list-post'], '<mailto:_xtest@dom.ain>')



class TestDecorate(TestBase):
    def test_short_circuit(self):
        msgdata = {'isdigest': 1}
        rtn = Decorate.process(self._mlist, None, msgdata)
        # Not really a great test, but there's little else to assert
        self.assertEqual(rtn, None)

    def test_no_multipart(self):
        mlist = self._mlist
        mlist.msg_header = 'header\n'
        mlist.msg_footer = 'footer'
        msg = email.message_from_string("""\
From: aperson@dom.ain

Here is a message.
""")
        Decorate.process(self._mlist, msg, {})
        self.assertEqual(msg.get_payload(), """\
header
Here is a message.
footer""")

    def test_no_multipart_template(self):
        mlist = self._mlist
        mlist.msg_header = '%(real_name)s header\n'
        mlist.msg_footer = '%(real_name)s footer'
        mlist.real_name = 'XTest'
        msg = email.message_from_string("""\
From: aperson@dom.ain

Here is a message.
""")
        Decorate.process(self._mlist, msg, {})
        self.assertEqual(msg.get_payload(), """\
XTest header
Here is a message.
XTest footer""")

    def test_no_multipart_type_error(self):
        mlist = self._mlist
        mlist.msg_header = '%(real_name) header\n'
        mlist.msg_footer = '%(real_name) footer'
        mlist.real_name = 'XTest'
        msg = email.message_from_string("""\
From: aperson@dom.ain

Here is a message.
""")
        Decorate.process(self._mlist, msg, {})
        self.assertEqual(msg.get_payload(), """\
%(real_name) header
Here is a message.
%(real_name) footer""")

    def test_no_multipart_value_error(self):
        mlist = self._mlist
        # These will generate warnings in logs/error
        mlist.msg_header = '%(real_name)p header\n'
        mlist.msg_footer = '%(real_name)p footer'
        mlist.real_name = 'XTest'
        msg = email.message_from_string("""\
From: aperson@dom.ain

Here is a message.
""")
        Decorate.process(self._mlist, msg, {})
        self.assertEqual(msg.get_payload(), """\
%(real_name)p header
Here is a message.
%(real_name)p footer""")

    def test_no_multipart_missing_key(self):
        mlist = self._mlist
        mlist.msg_header = '%(spooge)s header\n'
        mlist.msg_footer = '%(spooge)s footer'
        msg = email.message_from_string("""\
From: aperson@dom.ain

Here is a message.
""")
        Decorate.process(self._mlist, msg, {})
        self.assertEqual(msg.get_payload(), """\
%(spooge)s header
Here is a message.
%(spooge)s footer""")

    def test_multipart(self):
        eq = self.ndiffAssertEqual
        mlist = self._mlist
        mlist.msg_header = 'header'
        mlist.msg_footer = 'footer'
        msg1 = email.message_from_string("""\
From: aperson@dom.ain

Here is the first message.
""")
        msg2 = email.message_from_string("""\
From: bperson@dom.ain

Here is the second message.
""")
        msg = Message.Message()
        msg.set_type('multipart/mixed')
        msg.set_boundary('BOUNDARY')
        msg.attach(msg1)
        msg.attach(msg2)
        Decorate.process(self._mlist, msg, {})
        eq(msg.as_string(unixfrom=0), """\
MIME-Version: 1.0
Content-Type: multipart/mixed; boundary="BOUNDARY"

--BOUNDARY
Content-Type: text/plain; charset="us-ascii"
MIME-Version: 1.0
Content-Transfer-Encoding: 7bit
Content-Disposition: inline

header
--BOUNDARY
From: aperson@dom.ain

Here is the first message.

--BOUNDARY
From: bperson@dom.ain

Here is the second message.

--BOUNDARY
Content-Type: text/plain; charset="us-ascii"
MIME-Version: 1.0
Content-Transfer-Encoding: 7bit
Content-Disposition: inline

footer
--BOUNDARY--""")

    def test_image(self):
        eq = self.assertEqual
        mlist = self._mlist
        mlist.msg_header = 'header\n'
        mlist.msg_footer = 'footer'
        msg = email.message_from_string("""\
From: aperson@dom.ain
Content-type: image/x-spooge

IMAGEDATAIMAGEDATAIMAGEDATA
""")
        Decorate.process(self._mlist, msg, {})
        eq(len(msg.get_payload()), 3)
        self.assertEqual(msg.get_payload(1).get_payload(), """\
IMAGEDATAIMAGEDATAIMAGEDATA
""")

    def test_personalize_assert(self):
        raises = self.assertRaises
        raises(AssertionError, Decorate.process,
               self._mlist, None, {'personalize': 1})
        raises(AssertionError, Decorate.process,
               self._mlist, None, {'personalize': 1,
                                   'recips': [1, 2, 3]})



class TestFileRecips(TestBase):
    def test_short_circuit(self):
        msgdata = {'recips': 1}
        rtn = FileRecips.process(self._mlist, None, msgdata)
        # Not really a great test, but there's little else to assert
        self.assertEqual(rtn, None)

    def test_file_nonexistant(self):
        msgdata = {}
        FileRecips.process(self._mlist, None, msgdata)
        self.assertEqual(msgdata.get('recips'), [])

    def test_file_exists_no_sender(self):
        msg = email.message_from_string("""\
To: yall@dom.ain

""", Message.Message)
        msgdata = {}
        file = os.path.join(self._mlist.fullpath(), 'members.txt')
        addrs = ['aperson@dom.ain', 'bperson@dom.ain',
                 'cperson@dom.ain', 'dperson@dom.ain']
        fp = open(file, 'w')
        try:
            for addr in addrs:
                print >> fp, addr
            fp.close()
            FileRecips.process(self._mlist, msg, msgdata)
            self.assertEqual(msgdata.get('recips'), addrs)
        finally:
            try:
                os.unlink(file)
            except OSError, e:
                if e.errno <> e.ENOENT: raise

    def test_file_exists_no_member(self):
        msg = email.message_from_string("""\
From: eperson@dom.ain
To: yall@dom.ain

""", Message.Message)
        msgdata = {}
        file = os.path.join(self._mlist.fullpath(), 'members.txt')
        addrs = ['aperson@dom.ain', 'bperson@dom.ain',
                 'cperson@dom.ain', 'dperson@dom.ain']
        fp = open(file, 'w')
        try:
            for addr in addrs:
                print >> fp, addr
            fp.close()
            FileRecips.process(self._mlist, msg, msgdata)
            self.assertEqual(msgdata.get('recips'), addrs)
        finally:
            try:
                os.unlink(file)
            except OSError, e:
                if e.errno <> e.ENOENT: raise

    def test_file_exists_is_member(self):
        msg = email.message_from_string("""\
From: aperson@dom.ain
To: yall@dom.ain

""", Message.Message)
        msgdata = {}
        file = os.path.join(self._mlist.fullpath(), 'members.txt')
        addrs = ['aperson@dom.ain', 'bperson@dom.ain',
                 'cperson@dom.ain', 'dperson@dom.ain']
        fp = open(file, 'w')
        try:
            for addr in addrs:
                print >> fp, addr
                self._mlist.addNewMember(addr)
            fp.close()
            FileRecips.process(self._mlist, msg, msgdata)
            self.assertEqual(msgdata.get('recips'), addrs[1:])
        finally:
            try:
                os.unlink(file)
            except OSError, e:
                if e.errno <> e.ENOENT: raise



class TestHold(TestBase):
    def setUp(self):
        TestBase.setUp(self)
        self._mlist.administrivia = 1
        self._mlist.respond_to_post_requests = 0
        self._mlist.admin_immed_notify = 0
        # We're going to want to inspect this queue directory
        self._sb = Switchboard(mm_cfg.VIRGINQUEUE_DIR)

    def tearDown(self):
        for f in os.listdir(mm_cfg.VIRGINQUEUE_DIR):
            os.unlink(os.path.join(mm_cfg.VIRGINQUEUE_DIR, f))
        TestBase.tearDown(self)
        try:
            os.unlink(os.path.join(mm_cfg.DATA_DIR, 'pending.db'))
        except OSError, e:
            if e.errno <> errno.ENOENT: raise
        for f in [holdfile for holdfile in os.listdir(mm_cfg.DATA_DIR)
                  if holdfile.startswith('heldmsg-')]:
            os.unlink(os.path.join(mm_cfg.DATA_DIR, f))

    def test_short_circuit(self):
        msgdata = {'approved': 1}
        rtn = Hold.process(self._mlist, None, msgdata)
        # Not really a great test, but there's little else to assert
        self.assertEqual(rtn, None)

    def test_administrivia(self):
        msg = email.message_from_string("""\
From: aperson@dom.ain
Subject: unsubscribe

""", Message.Message)
        self.assertRaises(Hold.Administrivia, Hold.process,
                          self._mlist, msg, {})

    def test_max_recips(self):
        self._mlist.max_num_recipients = 5
        msg = email.message_from_string("""\
From: aperson@dom.ain
To: _xtest@dom.ain, bperson@dom.ain
Cc: cperson@dom.ain
Cc: dperson@dom.ain (Jimmy D. Person)
To: Billy E. Person <eperson@dom.ain>

Hey folks!
""", Message.Message)
        self.assertRaises(Hold.TooManyRecipients, Hold.process,
                          self._mlist, msg, {})

    def test_implicit_destination(self):
        self._mlist.require_explicit_destination = 1
        msg = email.message_from_string("""\
From: aperson@dom.ain
Subject: An implicit message

""", Message.Message)
        self.assertRaises(Hold.ImplicitDestination, Hold.process,
                          self._mlist, msg, {})

    def test_implicit_destination_fromusenet(self):
        self._mlist.require_explicit_destination = 1
        msg = email.message_from_string("""\
From: aperson@dom.ain
Subject: An implicit message

""", Message.Message)
        rtn = Hold.process(self._mlist, msg, {'fromusenet': 1})
        self.assertEqual(rtn, None)

    def test_suspicious_header(self):
        self._mlist.bounce_matching_headers = 'From: .*person@(blah.)?dom.ain'
        msg = email.message_from_string("""\
From: aperson@dom.ain
To: _xtest@dom.ain
Subject: An implicit message

""", Message.Message)
        self.assertRaises(Hold.SuspiciousHeaders, Hold.process,
                          self._mlist, msg, {})

    def test_suspicious_header_ok(self):
        self._mlist.bounce_matching_headers = 'From: .*person@blah.dom.ain'
        msg = email.message_from_string("""\
From: aperson@dom.ain
To: _xtest@dom.ain
Subject: An implicit message

""", Message.Message)
        rtn = Hold.process(self._mlist, msg, {})
        self.assertEqual(rtn, None)

    def test_max_message_size(self):
        self._mlist.max_message_size = 1
        msg = email.message_from_string("""\
From: aperson@dom.ain
To: _xtest@dom.ain

xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
""", Message.Message)
        self.assertRaises(Hold.MessageTooBig, Hold.process,
                          self._mlist, msg, {})

    def test_hold_notifications(self):
        eq = self.assertEqual
        self._mlist.respond_to_post_requests = 1
        self._mlist.admin_immed_notify = 1
        # Now cause an implicit destination hold
        msg = email.message_from_string("""\
From: aperson@dom.ain

""", Message.Message)
        self.assertRaises(Hold.ImplicitDestination, Hold.process,
                          self._mlist, msg, {})
        # Now we have to make sure there are two messages in the virgin queue,
        # one to the sender and one to the list owners.
        qfiles = {}
        files = self._sb.files()
        eq(len(files), 2)
        for filebase in files:
            qmsg, qdata = self._sb.dequeue(filebase)
            to = qmsg['to']
            qfiles[to] = qmsg, qdata
        # BAW: We could be testing many other attributes of either the
        # messages or the metadata files...
        keys = qfiles.keys()
        keys.sort()
        eq(keys, ['_xtest-owner@dom.ain', 'aperson@dom.ain'])
        # Get the pending cookie from the message to the sender
        pmsg, pdata = qfiles['aperson@dom.ain']
        confirmlines = pmsg.get_payload().split('\n')
        cookie = confirmlines[-3].split('/')[-1]
        # We also need to make sure there's an entry in the Pending database
        # for the heold message.
        data = Pending.confirm(cookie)
        eq(data, ('H', 1))
        heldmsg = os.path.join(mm_cfg.DATA_DIR, 'heldmsg-_xtest-1.pck')
        self.failUnless(os.path.exists(heldmsg))
        os.unlink(heldmsg)
        holdfiles = [f for f in os.listdir(mm_cfg.DATA_DIR)
                     if f.startswith('heldmsg-')]
        eq(len(holdfiles), 0)



class TestMimeDel(TestBase):
    def setUp(self):
        TestBase.setUp(self)
        self._mlist.filter_content = 1
        self._mlist.filter_mime_types = ['image/jpeg']
        self._mlist.pass_mime_types = []
        self._mlist.convert_html_to_plaintext = 1

    def test_outer_matches(self):
        msg = email.message_from_string("""\
From: aperson@dom.ain
Content-Type: image/jpeg
MIME-Version: 1.0

xxxxx
""")
        self.assertRaises(Errors.DiscardMessage, MimeDel.process,
                          self._mlist, msg, {})

    def test_strain_multipart(self):
        eq = self.assertEqual
        msg = email.message_from_string("""\
From: aperson@dom.ain
Content-Type: multipart/mixed; boundary=BOUNDARY
MIME-Version: 1.0

--BOUNDARY
Content-Type: image/jpeg
MIME-Version: 1.0

xxx

--BOUNDARY
Content-Type: image/gif
MIME-Version: 1.0

yyy
--BOUNDARY--
""")
        MimeDel.process(self._mlist, msg, {})
        eq(len(msg.get_payload()), 1)
        subpart = msg.get_payload(0)
        eq(subpart.get_content_type(), 'image/gif')
        eq(subpart.get_payload(), 'yyy')

    def test_collapse_multipart_alternative(self):
        eq = self.assertEqual
        msg = email.message_from_string("""\
From: aperson@dom.ain
Content-Type: multipart/mixed; boundary=BOUNDARY
MIME-Version: 1.0

--BOUNDARY
Content-Type: multipart/alternative; boundary=BOUND2
MIME-Version: 1.0

--BOUND2
Content-Type: image/jpeg
MIME-Version: 1.0

xxx

--BOUND2
Content-Type: image/gif
MIME-Version: 1.0

yyy
--BOUND2--

--BOUNDARY--
""")
        MimeDel.process(self._mlist, msg, {})
        eq(len(msg.get_payload()), 1)
        eq(msg.get_content_type(), 'multipart/mixed')
        subpart = msg.get_payload(0)
        eq(subpart.get_content_type(), 'image/gif')
        eq(subpart.get_payload(), 'yyy')

    def test_convert_to_plaintext(self):
        # BAW: This test is dependent on your particular lynx version
        eq = self.assertEqual
        msg = email.message_from_string("""\
From: aperson@dom.ain
Content-Type: text/html
MIME-Version: 1.0

<html><head></head>
<body></body></html>
""")
        MimeDel.process(self._mlist, msg, {})
        eq(msg.get_content_type(), 'text/plain')
        eq(msg.get_payload(), '\n\n\n')

    def test_deep_structure(self):
        eq = self.assertEqual
        self._mlist.filter_mime_types.append('text/html')
        msg = email.message_from_string("""\
From: aperson@dom.ain
Content-Type: multipart/mixed; boundary=AAA

--AAA
Content-Type: multipart/mixed; boundary=BBB

--BBB
Content-Type: image/jpeg

xxx
--BBB
Content-Type: image/jpeg

yyy
--BBB---
--AAA
Content-Type: multipart/alternative; boundary=CCC

--CCC
Content-Type: text/html

<h2>This is a header</h2>

--CCC
Content-Type: text/plain

A different message
--CCC--
--AAA
Content-Type: image/gif

zzz
--AAA
Content-Type: image/gif

aaa
--AAA--
""")
        MimeDel.process(self._mlist, msg, {})
        payload = msg.get_payload()
        eq(len(payload), 3)
        part1 = msg.get_payload(0)
        eq(part1.get_content_type(), 'text/plain')
        eq(part1.get_payload(), 'A different message')
        part2 = msg.get_payload(1)
        eq(part2.get_content_type(), 'image/gif')
        eq(part2.get_payload(), 'zzz')
        part3 = msg.get_payload(2)
        eq(part3.get_content_type(), 'image/gif')
        eq(part3.get_payload(), 'aaa')

    def test_top_multipart_alternative(self):
        eq = self.assertEqual
        self._mlist.filter_mime_types.append('text/html')
        msg = email.message_from_string("""\
From: aperson@dom.ain
Content-Type: multipart/alternative; boundary=AAA

--AAA
Content-Type: text/html

<b>This is some html</b>
--AAA
Content-Type: text/plain

This is plain text
--AAA--
""")
        MimeDel.process(self._mlist, msg, {})
        eq(msg.get_content_type(), 'text/plain')
        eq(msg.get_payload(), 'This is plain text')



class TestModerate(TestBase):
    pass



class TestReplybot(TestBase):
    pass



class TestSpamDetect(TestBase):
    def test_short_circuit(self):
        msgdata = {'approved': 1}
        rtn = SpamDetect.process(self._mlist, None, msgdata)
        # Not really a great test, but there's little else to assert
        self.assertEqual(rtn, None)

    def test_spam_detect(self):
        msg1 = email.message_from_string("""\
From: aperson@dom.ain

A message.
""")
        msg2 = email.message_from_string("""\
To: xlist@dom.ain

A message.
""")
        spammers = mm_cfg.KNOWN_SPAMMERS[:]
        try:
            mm_cfg.KNOWN_SPAMMERS.append(('from', '.?person'))
            self.assertRaises(SpamDetect.SpamDetected,
                              SpamDetect.process, self._mlist, msg1, {})
            rtn = SpamDetect.process(self._mlist, msg2, {})
            self.assertEqual(rtn, None)
        finally:
            mm_cfg.KNOWN_SPAMMERS = spammers



class TestTagger(TestBase):
    def setUp(self):
        TestBase.setUp(self)
        self._mlist.topics = [('bar fight', '.*bar.*', 'catch any bars', 1)]
        self._mlist.topics_enabled = 1

    def test_short_circuit(self):
        self._mlist.topics_enabled = 0
        rtn = Tagger.process(self._mlist, None, {})
        # Not really a great test, but there's little else to assert
        self.assertEqual(rtn, None)

    def test_simple(self):
        eq = self.assertEqual
        mlist = self._mlist
        mlist.topics_bodylines_limit = 0
        msg = email.message_from_string("""\
Subject: foobar
Keywords: barbaz

""")
        msgdata = {}
        Tagger.process(mlist, msg, msgdata)
        eq(msg['x-topics'], 'bar fight')
        eq(msgdata.get('topichits'), ['bar fight'])

    def test_all_body_lines_plain_text(self):
        eq = self.assertEqual
        mlist = self._mlist
        mlist.topics_bodylines_limit = -1
        msg = email.message_from_string("""\
Subject: Was
Keywords: Raw

Subject: farbaw
Keywords: barbaz
""")
        msgdata = {}
        Tagger.process(mlist, msg, msgdata)
        eq(msg['x-topics'], 'bar fight')
        eq(msgdata.get('topichits'), ['bar fight'])

    def test_no_body_lines(self):
        eq = self.assertEqual
        mlist = self._mlist
        mlist.topics_bodylines_limit = 0
        msg = email.message_from_string("""\
Subject: Was
Keywords: Raw

Subject: farbaw
Keywords: barbaz
""")
        msgdata = {}
        Tagger.process(mlist, msg, msgdata)
        eq(msg['x-topics'], None)
        eq(msgdata.get('topichits'), None)

    def test_body_lines_in_multipart(self):
        eq = self.assertEqual
        mlist = self._mlist
        mlist.topics_bodylines_limit = -1
        msg = email.message_from_string("""\
Subject: Was
Keywords: Raw
Content-Type: multipart/alternative; boundary="BOUNDARY"

--BOUNDARY
From: sabo
To: obas

Subject: farbaw
Keywords: barbaz

--BOUNDARY--
""")
        msgdata = {}
        Tagger.process(mlist, msg, msgdata)
        eq(msg['x-topics'], 'bar fight')
        eq(msgdata.get('topichits'), ['bar fight'])

    def test_body_lines_no_part(self):
        eq = self.assertEqual
        mlist = self._mlist
        mlist.topics_bodylines_limit = -1
        msg = email.message_from_string("""\
Subject: Was
Keywords: Raw
Content-Type: multipart/alternative; boundary=BOUNDARY

--BOUNDARY
From: sabo
To: obas
Content-Type: message/rfc822

Subject: farbaw
Keywords: barbaz

--BOUNDARY
From: sabo
To: obas
Content-Type: message/rfc822

Subject: farbaw
Keywords: barbaz

--BOUNDARY--
""")
        msgdata = {}
        Tagger.process(mlist, msg, msgdata)
        eq(msg['x-topics'], None)
        eq(msgdata.get('topichits'), None)



class TestToArchive(TestBase):
    def setUp(self):
        TestBase.setUp(self)
        # We're going to want to inspect this queue directory
        self._sb = Switchboard(mm_cfg.ARCHQUEUE_DIR)

    def tearDown(self):
        for f in os.listdir(mm_cfg.ARCHQUEUE_DIR):
            os.unlink(os.path.join(mm_cfg.ARCHQUEUE_DIR, f))
        TestBase.tearDown(self)

    def test_short_circuit(self):
        eq = self.assertEqual
        msgdata = {'isdigest': 1}
        ToArchive.process(self._mlist, None, msgdata)
        eq(len(self._sb.files()), 0)
        # Try the other half of the or...
        self._mlist.archive = 0
        ToArchive.process(self._mlist, None, msgdata)
        eq(len(self._sb.files()), 0)
        # Now try the various message header shortcuts
        msg = email.message_from_string("""\
X-No-Archive: YES

""")
        self._mlist.archive = 1
        ToArchive.process(self._mlist, msg, {})
        eq(len(self._sb.files()), 0)
        # And for backwards compatibility
        msg = email.message_from_string("""\
X-Archive: NO

""")
        ToArchive.process(self._mlist, msg, {})
        eq(len(self._sb.files()), 0)

    def test_normal_archiving(self):
        eq = self.assertEqual
        msg = email.message_from_string("""\
Subject: About Mailman

It rocks!
""")
        ToArchive.process(self._mlist, msg, {})
        files = self._sb.files()
        eq(len(files), 1)
        msg2, data = self._sb.dequeue(files[0])
        eq(len(data), 2)
        eq(data['version'], 3)
        # Clock skew makes this unreliable
        #self.failUnless(data['received_time'] <= time.time())
        eq(msg.as_string(unixfrom=0), msg2.as_string(unixfrom=0))



class TestToDigest(TestBase):
    def _makemsg(self, i=0):
        msg = email.message_from_string("""From: aperson@dom.ain
To: _xtest@dom.ain
Subject: message number %(i)d

Here is message %(i)d
""" % {'i' : i})
        return msg

    def setUp(self):
        TestBase.setUp(self)
        self._path = os.path.join(self._mlist.fullpath(), 'digest.mbox')
        fp = open(self._path, 'w')
        g = Generator(fp)
        for i in range(5):
            g.flatten(self._makemsg(i), unixfrom=1)
        fp.close()
        self._sb = Switchboard(mm_cfg.VIRGINQUEUE_DIR)

    def tearDown(self):
        try:
            os.unlink(self._path)
        except OSError, e:
            if e.errno <> errno.ENOENT: raise
        for f in os.listdir(mm_cfg.VIRGINQUEUE_DIR):
            os.unlink(os.path.join(mm_cfg.VIRGINQUEUE_DIR, f))
        TestBase.tearDown(self)

    def test_short_circuit(self):
        eq = self.assertEqual
        mlist = self._mlist
        mlist.digestable = 0
        eq(ToDigest.process(mlist, None, {}), None)
        mlist.digestable = 1
        eq(ToDigest.process(mlist, None, {'isdigest': 1}), None)
        eq(self._sb.files(), [])

    def test_undersized(self):
        msg = self._makemsg(99)
        size = os.path.getsize(self._path) + len(str(msg))
        self._mlist.digest_size_threshhold = (size + 1) * 1024
        ToDigest.process(self._mlist, msg, {})
        self.assertEqual(self._sb.files(), [])

    def test_send_a_digest(self):
        eq = self.assertEqual
        mlist = self._mlist
        msg = self._makemsg(99)
        size = os.path.getsize(self._path) + len(str(msg))
        mlist.digest_size_threshhold = 0
        ToDigest.process(mlist, msg, {})
        files = self._sb.files()
        # There should be two files in the queue, one for the MIME digest and
        # one for the RFC 1153 digest.
        eq(len(files), 2)
        # Now figure out which of the two files is the MIME digest and which
        # is the RFC 1153 digest.
        for filebase in files:
            qmsg, qdata = self._sb.dequeue(filebase)
            if qmsg.get_main_type() == 'multipart':
                mimemsg = qmsg
                mimedata = qdata
            else:
                rfc1153msg = qmsg
                rfc1153data = qdata
        eq(rfc1153msg.get_content_type(), 'text/plain')
        eq(mimemsg.get_content_type(), 'multipart/mixed')
        eq(mimemsg['from'], mlist.GetRequestEmail())
        eq(mimemsg['subject'],
           '%(realname)s Digest, Vol %(volume)d, Issue %(issue)d' % {
            'realname': mlist.real_name,
            'volume'  : mlist.volume,
            'issue'   : mlist.next_digest_number - 1,
            })
        eq(mimemsg['to'], mlist.GetListEmail())
        # BAW: this test is incomplete...



class TestToOutgoing(TestBase):
    def setUp(self):
        TestBase.setUp(self)
        # We're going to want to inspect this queue directory
        self._sb = Switchboard(mm_cfg.OUTQUEUE_DIR)

    def tearDown(self):
        for f in os.listdir(mm_cfg.OUTQUEUE_DIR):
            os.unlink(os.path.join(mm_cfg.OUTQUEUE_DIR, f))
        TestBase.tearDown(self)

    def test_outgoing(self):
        eq = self.assertEqual
        msg = email.message_from_string("""\
Subject: About Mailman

It rocks!
""")
        msgdata = {'foo': 1, 'bar': 2}
        ToOutgoing.process(self._mlist, msg, msgdata)
        files = self._sb.files()
        eq(len(files), 1)
        msg2, data = self._sb.dequeue(files[0])
        eq(msg.as_string(unixfrom=0), msg2.as_string(unixfrom=0))
        eq(len(data), 6)
        eq(data['foo'], 1)
        eq(data['bar'], 2)
        eq(data['version'], 3)
        eq(data['listname'], '_xtest')
        eq(data['verp'], 1)
        # Clock skew makes this unreliable
        #self.failUnless(data['received_time'] <= time.time())



class TestToUsenet(TestBase):
    def setUp(self):
        TestBase.setUp(self)
        # We're going to want to inspect this queue directory
        self._sb = Switchboard(mm_cfg.NEWSQUEUE_DIR)

    def tearDown(self):
        for f in os.listdir(mm_cfg.NEWSQUEUE_DIR):
            os.unlink(os.path.join(mm_cfg.NEWSQUEUE_DIR, f))
        TestBase.tearDown(self)

    def test_short_circuit(self):
        eq = self.assertEqual
        mlist = self._mlist
        mlist.gateway_to_news = 0
        ToUsenet.process(mlist, None, {})
        eq(len(self._sb.files()), 0)
        mlist.gateway_to_news = 1
        ToUsenet.process(mlist, None, {'isdigest': 1})
        eq(len(self._sb.files()), 0)
        ToUsenet.process(mlist, None, {'fromusenet': 1})
        eq(len(self._sb.files()), 0)

    def test_to_usenet(self):
        # BAW: Should we, can we, test the error conditions that only log to a
        # file instead of raising an exception?
        eq = self.assertEqual
        mlist = self._mlist
        mlist.gateway_to_news = 1
        mlist.linked_newsgroup = 'foo'
        mlist.nntp_host = 'bar'
        msg = email.message_from_string("""\
Subject: About Mailman

Mailman rocks!
""")
        ToUsenet.process(mlist, msg, {})
        files = self._sb.files()
        eq(len(files), 1)
        msg2, data = self._sb.dequeue(files[0])
        eq(msg.as_string(unixfrom=0), msg2.as_string(unixfrom=0))
        eq(data['version'], 3)
        eq(data['listname'], '_xtest')
        # Clock skew makes this unreliable
        #self.failUnless(data['received_time'] <= time.time())



def suite():
    suite = unittest.TestSuite()
    suite.addTest(unittest.makeSuite(TestAcknowledge))
    suite.addTest(unittest.makeSuite(TestAfterDelivery))
    suite.addTest(unittest.makeSuite(TestApprove))
    suite.addTest(unittest.makeSuite(TestCalcRecips))
    suite.addTest(unittest.makeSuite(TestCleanse))
    suite.addTest(unittest.makeSuite(TestCookHeaders))
    suite.addTest(unittest.makeSuite(TestDecorate))
    suite.addTest(unittest.makeSuite(TestFileRecips))
    suite.addTest(unittest.makeSuite(TestHold))
    suite.addTest(unittest.makeSuite(TestMimeDel))
    suite.addTest(unittest.makeSuite(TestModerate))
    suite.addTest(unittest.makeSuite(TestReplybot))
    suite.addTest(unittest.makeSuite(TestSpamDetect))
    suite.addTest(unittest.makeSuite(TestTagger))
    suite.addTest(unittest.makeSuite(TestToArchive))
    suite.addTest(unittest.makeSuite(TestToDigest))
    suite.addTest(unittest.makeSuite(TestToOutgoing))
    suite.addTest(unittest.makeSuite(TestToUsenet))
    return suite



if __name__ == '__main__':
    unittest.main(defaultTest='suite')
