#!/usr/bin/env python
#
# lsmgen.py -- generate current LSM for fetchmail
#
# FIXME: The FTP location doesn't actually work.
import sys, os, time
version = sys.argv[1];
size = int(sys.argv[2].split()[0])/1000
wwwhost = os.environ['WWWVIRTUAL'];
date = time.strftime("%Y%b%0d", time.localtime()).upper()

fmt = """Begin3
Title:		fetchmail
Version:	%s
Entered-date:	%s
Description:	Full-featured IMAP/POP2/POP3/APOP/RPOP/KPOP/ETRN/ODMR client 
	        with GUI configuration, daemon mode, forwarding via SMTP or 
		local MDA, reply address rewrites, support for multidrop
		mailboxes. Supports SASL authentication, Kerbereros, GSSAPI, 
		OTP. Not a mail user agent, rather a pipe-fitting that 
		forwards fetched mail to your local delivery system. 
		Your one-stop solution for intermittent email connections.
		Home page and FAQ at http://%s/~esr/fetchmail.
Keywords:	mail, client, POP, POP2, POP3, APOP, RPOP, KPOP, IMAP, ETRN,
    		ODMR, SMTP, ESMTP, GSSAPI, RPA, NTLM, CRAM-MD5, SASL.
Author: 	esr@snark.thyrsus.com (Eric S. Raymond)
Primary-site:	locke.ccil.org /pub/esr/fetchmail
		%sK fetchmail-%s.tar.gz
Platforms:	All
Copying-policy:	GPL
End
"""
print fmt % (version, date, wwwhost, size, version)




