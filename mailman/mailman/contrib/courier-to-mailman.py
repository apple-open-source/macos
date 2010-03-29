#! /usr/bin/python

# Configuration variables - Change these for your site if necessary.
#
MailmanHome = "@prefix@"; # Mailman home directory.
MailmanVar = "@VAR_PREFIX@"; # Mailman directory for mutable data.
MailmanOwner = "postmaster@localhost"; # Postmaster and abuse mail recepient.
#
# End of configuration variables.

# courier-to-mailman.py
#
# Interface mailman to a Courier virtual domain. Does not require the creation
# of _any_ list-specific aliases to connect lists to your mail system.
#
# Adapted March 29, 2004 by Lindsay Haisley, fmouse@fmp.com from
# qmail-to-mailman.py by Bruce Perens, bruce@perens.com, March 1999.
#
# This is free software under the GNU General Public License.
#
# This script is meant to be called from
# <domain_home>/alias/.courier-default. It catches all mail to any address
# at a virtual domain not otherwise handled by an explicit account or a
# .courier file.  It looks at the recepient for each mail message not
# otherwise handled and decides if the mail is addressed to a valid list or
# not, and bounces the message with a helpful suggestion if it's not
# addressed to a list.  It decides if it is a posting, a list command, or
# mail to the list administrator by checking for the various address tags as
# defined in manual Mailman list creation output (~mailman/bin/newlist).  It
# will recognize a list as soon as the list is created.  Above and beyond
# setting up a proper locally-hosted domain in Courier (the use of webadmin
# is highly recommended!), no other configuration should be required. This
# program recognizes mail to postmaster, mailman-owner, abuse,
# mailer-daemon, root, and owner, and routes those mails to MailmanOwner as
# defined in the configuration variables, above.
#
# INSTALLATION:
#
# Install this file as @prefix@/bin/courier-to-mailman.py
#
# To configure a virtual domain to connect to mailman, create these files:
#
# <domain_home>/alias/.courier-listname
# ... containing ...
# |/usr/bin/preline @prefix@/bin/courier-to-mailman.py
#
# Symlink <domain_home>/alias/.courier-listname-default to this file
#
# "listname" is the name of your list.
#
# Paths must, of course, be set correctly for the Courier and Mailman
# installations on your system.
#
# Note: "preline" is a Courier program which ensures a Unix "From " header
# is on the message.  Archiving will break without this.

import sys, os, re, string

def main():
	os.nice(5)  # Handle mailing lists at non-interactive priority.

	os.chdir(MailmanVar + "/lists")

	try:
		local = string.lower(os.environ["LOCAL"])
	except:
		# This might happen if we're not using qmail.
		sys.stderr.write("LOCAL not set in environment?\n")
		sys.exit(112)

	names = ("root", "postmaster", "mailer-daemon", "mailman-owner", "owner",
			 "abuse")
	for i in names:
		if i == local:
			os.execv("/usr/bin/sendmail",
					 ("/usr/bin/sendmail", MailmanOwner))
			sys.exit(0)

	type = "post"
	listname = string.lower(local)
	types = (("-admin$", "admin"),
			 ("-bounces$", "bounces"),
			 ("-bounces\+.*$", "bounces"),		# for VERP
			 ("-confirm$", "confirm"),
			 ("-confirm\+.*$", "confirm"),
			 ("-join$", "join"),
			 ("-leave$", "leave"),
			 ("-owner$", "owner"),
			 ("-request$", "request"),
			 ("-subscribe$", "subscribe"),
			 ("-unsubscribe$", "unsubscribe"))

	for i in types:
		if re.search(i[0],local):
			type = i[1]
			listname = re.sub(i[0],"",local)

	if os.path.exists(listname):
		os.execv(MailmanHome + "/mail/mailman",
				 (MailmanHome + "/mail/mailman", type, listname))
	else:
		bounce()

	sys.exit(111)

def bounce():
	bounce_message = """\
TO ACCESS THE MAILING LIST SYSTEM: Start your web browser on
http://%s/
That web page will help you subscribe or unsubscribe, and will
give you directions on how to post to each mailing list.\n"""
	sys.stderr.write(bounce_message % (os.environ["HOST"]))
	sys.exit(100)

try:
	sys.exit(main())
except SystemExit, argument:
	sys.exit(argument)

except Exception, argument:
	info = sys.exc_info()
	trace = info[2]
	sys.stderr.write("%s %s\n" % (sys.exc_type, argument))
	sys.stderr.write("LINE %d\n" % (trace.tb_lineno))
	sys.exit(111) # Soft failure, try again later.
