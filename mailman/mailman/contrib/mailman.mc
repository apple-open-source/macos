dnl
dnl *** EXAMPLE *** sendmail.mc file for a Mailman list server using
dnl mm-handler to deal with list operations (in place of aliases).
dnl This is what I actually use on my site.
dnl
dnl $Id: mailman.mc,v 1.1.1.1 2001/10/27 02:30:51 jalbert Exp $
dnl


dnl
dnl First you need to define your general characteristics.  You
dnl should know what these settings should be at your site -- I
dnl only know what they should be at mine.
dnl
OSTYPE(solaris2)dnl
DOMAIN(generic)dnl

dnl
dnl You can keep the old alias files for back-compatibility, but it's
dnl probably better not to as this can become a point of confusion
dnl later.
dnl
define(`ALIAS_FILE', `/etc/mail/aliases,/etc/mail/lists')

dnl
dnl I use procmail for local delivery, because it's smart to have a
dnl local delivery mailer, even if you don't (ordinarily) do any local
dnl delivery. The Solaris local delivery mailer is part of its sendmail
dnl package. I pkgrmed the sendmail packages so that system upgrades
dnl don't kill my sendmail.com sendmail, so mail.local is unavailable,
dnl so I throw procmail in here even though it never gets used.
dnl
define(`PROCMAIL_MAILER_PATH', `/opt/bin/procmail')
FEATURE(`local_procmail')
MAILER(`local')

dnl
dnl Miscellaneous tuning.  Not relevant to Mailman.
dnl
define(`confCONNECTION_RATE_THROTTLE', 5)
define(`confMAX_MESSAGE_SIZE', `5000000')
define(`confNO_RCPT_ACTION', `add-to-undisclosed')
define(`confME_TOO', `True')
define(`confDOUBLE_BOUNCE_ADDRESS', `mailer-daemon')

dnl
dnl Privacy options.  Also not relevant.
dnl
define(`confPRIVACY_FLAGS', `authwarnings,needvrfyhelo,noexpn,noreceipts,restrictmailq')


dnl
dnl Mm-handler works by mailertabling all addresses on your list
dnl server hostname(s) through the mm-handler mailer. Mailertable
dnl maps mail domains to mailer types. I want a mailertable to map
dnl listtest.uchicago.edu to the mm-handler mailer, but we need to
dnl specifically request this functionality in the .mc file.
dnl
FEATURE(`mailertable', `hash -o /etc/mail/mailertable')

dnl
dnl This leads to an immediate and important side-effect: "local"
dnl addresses, and notably RFC-specified addresses such as postmaster,
dnl are assumed by sendmail to be lists! Since aliases are not processed
dnl for domaintabled domains, we must use a virtusertable to reroute
dnl such addresses.
dnl
FEATURE(`virtusertable', `hash -o /etc/mail/virtusertable')

dnl
dnl By default, sendmail applies virtusertable mapping, if at all, for
dnl all interfaces for which it accepts mail -- i.e., all domains in
dnl $=w. Mm-handler relies on your having a single domain (hostname)
dnl that serves only lists, with no users. To avoid potential namespace
dnl conflicts, you need not to have this list domain included in $=w.
dnl As a result, virtuser mapping does not apply for the Mailman
dnl list domain. However, you can pre-empt this rule by defining
dnl $={VirtHost}: if there are domains in this class, they will be
dnl mapped before $=w is mapped.
dnl
dnl VIRTUSER_DOMAIN() defines this class.
dnl
VIRTUSER_DOMAIN(`nospam.uchicago.edu listtest.uchicago.edu listhost.uchicago.edu')

dnl
dnl On a related point: by default, Sendmail probes for open IP
dnl interfaces, and adds their hostnames to $=w. Although Sendmail does
dnl virtusertable mapping for members of $=w, it doesn't do mailertable
dnl mapping for them, because they're considered "local". This tells
dnl Sendmail not to probe interfaces for local hosts, and it's critical
dnl if your Mailman domain is actually an IP address (with an A record,
dnl not just CNAME or MX) on your server.
dnl
define(`confDONT_PROBE_INTERFACES', `True')


dnl
dnl Even though my actual hostname is foobar, tell the world that I'm
dnl listtest.uchicago.edu.
dnl
FEATURE(`limited_masquerade')
MASQUERADE_AS(`listtest.uchicago.edu')


dnl
dnl Access control is a useful feature for blocking abusers and relays
dnl and such.
dnl
FEATURE(`access_db')


dnl
dnl This allows you to block access for individual recipents through
dnl the same access database as is used for blocking sender hosts and
dnl addresses.
dnl
FEATURE(`blacklist_recipients')


dnl
dnl Other local mailers...
dnl
MAILER(`smtp')
MAILER(`procmail')


dnl
dnl Our Mailman-specific local mailer.
dnl
MAILER_DEFINITIONS
####################################
###   New Mailer specifications  ###
####################################

## Special flags! See
##	http://www.sendmail.org/~ca/email/doc8.10/op-sh-5.html#sh-5.4
## Note especially the absence of the "m" and "n" flags. THIS IS
## IMPORTANT: mm-handler assumes this behavior to avoid having to know
## too much about address parsing and other RFC-2822 mail details.

Mmailman,	P=/etc/mail/mm-handler, F=rDFMhlqSu, U=mailman:other,
		S=EnvFromL, R=EnvToL/HdrToL,
		A=mm-handler $h $u
