# Copyright (C) 2001,2002 by the Free Software Foundation, Inc.
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

"""Test the bounce detection modules."""

import sys
import os
import unittest
import email



class BounceTest(unittest.TestCase):
    DATA = (
        # Postfix bounces
        ('Postfix', 'postfix_01.txt', ['xxxxx@local.ie']),
        ('Postfix', 'postfix_02.txt', ['yyyyy@digicool.com']),
        ('Postfix', 'postfix_03.txt', ['ttttt@ggggg.com']),
        ('Postfix', 'postfix_04.txt', ['davidlowie@mail1.keftamail.com']),
        ('Postfix', 'postfix_05.txt', ['bjelf@detectit.net']),
        # Exim bounces
        ('Exim', 'exim_01.txt', ['delangen@its.tudelft.nl']),
        # SimpleMatch bounces
        ('SimpleMatch', 'sendmail_01.txt', ['zzzzz@nfg.nl']),
        ('SimpleMatch', 'simple_01.txt', ['bbbsss@turbosport.com']),
        ('SimpleMatch', 'simple_02.txt', ['chris.ggggmmmm@usa.net']),
        ('SimpleMatch', 'simple_04.txt', ['claird@starbase.neosoft.com']),
        ('SimpleMatch', 'newmailru_01.txt', ['zzzzz@newmail.ru']),
        ('SimpleMatch', 'hotpop_01.txt', ['allensmithee@hotpop.com']),
        # SimpleWarning
        ('SimpleWarning', 'simple_03.txt', ['jacobus@geo.co.za']),
        # GroupWise
        ('GroupWise', 'groupwise_01.txt', ['thoff@MAINEX1.ASU.EDU']),
        # This one really sucks 'cause it's text/html.  Just make sure it
        # doesn't throw an exception, but we won't get any meaningful
        # addresses back from it.
        ('GroupWise', 'groupwise_02.txt', []),
        # Yale's own
        ('Yale', 'yale_01.txt', ['thomas.dtankengine@cs.yale.edu',
                                 'thomas.dtankengine@yale.edu']),
        # DSN, i.e. RFC 1894
        ('DSN', 'dsn_01.txt', ['JimmyMcEgypt@go.com']),
        ('DSN', 'dsn_02.txt', ['zzzzz@zeus.hud.ac.uk']),
        ('DSN', 'dsn_03.txt', ['ddd.kkk@advalvas.be']),
        ('DSN', 'dsn_04.txt', ['max.haas@unibas.ch']),
        ('DSN', 'dsn_05.txt', ['pkocmid@atlas.cz']),
        ('DSN', 'dsn_06.txt', ['hao-nghi.au@fr.thalesgroup.com']),
        ('DSN', 'dsn_07.txt', ['david.farrar@parliament.govt.nz']),
        ('DSN', 'dsn_08.txt', ['news-list.zope@localhost.bln.innominate.de']),
        ('DSN', 'dsn_09.txt', ['pr@allen-heath.com']),
        ('DSN', 'dsn_10.txt', ['anne.person@dom.ain']),
        ('DSN', 'dsn_11.txt', ['joem@example.com']),
        # Microsoft Exchange
        ('Exchange', 'microsoft_01.txt', ['DJBENNETT@IKON.COM']),
        ('Exchange', 'microsoft_02.txt', ['MDMOORE@BALL.COM']),
        # SMTP32
        ('SMTP32', 'smtp32_01.txt', ['oliver@pcworld.com.ph']),
        ('SMTP32', 'smtp32_02.txt', ['lists@mail.spicynoodles.com']),
        ('SMTP32', 'smtp32_03.txt', ['borisk@gw.xraymedia.com']),
        # Qmail
        ('Qmail', 'qmail_01.txt', ['psadisc@wwwmail.n-h.de']),
        # LLNL's custom Sendmail
        ('LLNL', 'llnl_01.txt', ['trotts1@llnl.gov']),
        # Netscape's server...
        ('Netscape', 'netscape_01.txt', ['aaaaa@corel.com',
                                         'bbbbb@corel.com']),
        # Yahoo's proprietary format
        ('Yahoo', 'yahoo_01.txt', ['subscribe.motorcycles@listsociety.com']),
        ('Yahoo', 'yahoo_02.txt', ['agarciamartiartu@yahoo.es']),
        ('Yahoo', 'yahoo_03.txt', ['cresus22@yahoo.com']),
        ('Yahoo', 'yahoo_04.txt', ['agarciamartiartu@yahoo.es',
                                   'open00now@yahoo.co.uk']),
        ('Yahoo', 'yahoo_05.txt', ['cresus22@yahoo.com',
                                   'jjb700@yahoo.com']),
        ('Yahoo', 'yahoo_06.txt', ['andrew_polevoy@yahoo.com',
                                   'baruch_sterin@yahoo.com',
                                   'rjhoeks@yahoo.com',
                                   'tritonrugger91@yahoo.com']),
        ('Yahoo', 'yahoo_07.txt', ['mark1960_1998@yahoo.com',
                                   'ovchenkov@yahoo.com',
                                   'tsa412@yahoo.com',
                                   'vaxheadroom@yahoo.com']),
        ('Yahoo', 'yahoo_08.txt', ['chatrathis@yahoo.com',
                                   'crownjules01@yahoo.com',
                                   'cwl_999@yahoo.com',
                                   'eichaiwiu@yahoo.com',
                                   'rjhoeks@yahoo.com',
                                   'yuli_kolesnikov@yahoo.com']),
        ('Yahoo', 'yahoo_09.txt', ['hankel_o_fung@yahoo.com',
                                   'ultravirus2001@yahoo.com']),
        # sina.com appears to use their own weird SINAEMAIL MTA
        ('Sina', 'sina_01.txt', ['boboman76@sina.com', 'alan_t18@sina.com']),
        # No address can be detected in these...
        # dumbass_01.txt - We love Microsoft. :(
        # Done
        )

    def test_bounce(self):
        for modname, file, addrs in self.DATA:
            module = 'Mailman.Bouncers.' + modname
            __import__(module)
            fp = open(os.path.join('tests', 'bounces', file))
            try:
                msg = email.message_from_file(fp)
            finally:
                fp.close()
            foundaddrs = sys.modules[module].process(msg)
            # Some modules return None instead of [] for failure
            if foundaddrs is None:
                foundaddrs = []
            addrs.sort()
            foundaddrs.sort()
            self.assertEqual(addrs, foundaddrs)

    def test_SMTP32_failure(self):
        from Mailman.Bouncers import SMTP32
        # This file has no X-Mailer: header
        fp = open(os.path.join('tests', 'bounces', 'postfix_01.txt'))
        try:
            msg = email.message_from_file(fp)
        finally:
            fp.close()
        self.failIf(msg['x-mailer'] is not None)
        self.failIf(SMTP32.process(msg))

    def test_caiwireless(self):
        from Mailman.Bouncers import Caiwireless
        # BAW: this is a mostly bogus test; I lost the samples. :(
        msg = email.message_from_string("""\
Content-Type: multipart/report; boundary=BOUNDARY

--BOUNDARY

--BOUNDARY--

""")
        self.assertEqual(None, Caiwireless.process(msg))

    def test_microsoft(self):
        from Mailman.Bouncers import Microsoft
        # BAW: similarly as above, I lost the samples. :(
        msg = email.message_from_string("""\
Content-Type: multipart/report; boundary=BOUNDARY

--BOUNDARY

--BOUNDARY--

""")
        self.assertEqual(None, Microsoft.process(msg))



def suite():
    suite = unittest.TestSuite()
    suite.addTest(unittest.makeSuite(BounceTest))
    return suite



if __name__ == '__main__':
    unittest.main(defaultTest='suite')
