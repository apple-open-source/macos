#! @PYTHON@

# Configuration variables - Change these for your site if necessary.
MailmanHome = "@prefix@"; # Mailman home directory.
MailmanVar = "@VAR_PREFIX@"; # Mailman directory for mutable data.
MailmanOwner = "postmaster@localhost"; # Postmaster and abuse mail recepient.
# End of configuration variables.

# qmail-to-mailman.py
#
# Interface mailman to a qmail virtual domain. Does not require the creation
# of _any_ aliases to connect lists to your mail system.
#
# Bruce Perens, bruce@perens.com, March 1999.
# This is free software under the GNU General Public License.
#
# This script is meant to be called from ~mailman/.qmail-default . It catches
# all mail to a virtual domain, in my case "lists.hams.com". It looks at the
# recepient for each mail message and decides if the mail is addressed to a
# valid list or not, and bounces the message with a helpful suggestion if it's
# not addressed to a list. It decides if it is a posting, a list command, or
# mail to the list administrator, by checking for the -admin, -owner, and
# -request addresses. It will recognize a list as soon as the list is created,
# there is no need to add _any_ aliases for any list. It recognizes mail to
# postmaster, mailman-owner, abuse, mailer-daemon, root, and owner, and
# routes those mails to MailmanOwner as defined in the configuration
# variables, above.
#
# INSTALLATION:
#
# Install this file as ~mailman/qmail-to-mailman.py
#
# To configure a virtual domain to connect to mailman, create these files:
#
# ~mailman/.qmail-default
# |preline @PYTHON@ @prefix@/mail-in.py
#
# /var/qmail/control/virtualdomains:
# DOMAIN.COM:mailman
#
# Note: "preline" is a QMail program which ensures a Unix "From " header is
# on the message.  Archiving will break without this.
#
# Replace DOMAIN.COM above with the name of the domain to be connected to
# Mailman. Note that _all_ mail to that domain will go to Mailman, so you
# don't want to put the name of your main domain here. In my case, I created 
# lists.hams.com for Mailman, and I use hams.com as my regular domain.
#
# After you edit /var/qmail/control/virtualdomains, kill and restart qmail.
#

import sys, os, re, string

def main():
    os.nice(5)  # Handle mailing lists at non-interactive priority.

    os.chdir(MailmanVar + "/lists")

    try:
        local = os.environ["LOCAL"]
    except:
        # This might happen if we're not using qmail.
        sys.stderr.write("LOCAL not set in environment?\n")
        sys.exit(100)

    local = string.lower(local)
    local = re.sub("^mailman-","",local)

    names = ("root", "postmaster", "mailer-daemon", "mailman-owner", "owner",
             "abuse")
    for i in names:
        if i == local:
            os.execv("/var/qmail/bin/qmail-inject",
                     ("/var/qmail/bin/qmail-inject", MailmanOwner))
            sys.exit(0)

    type = "post"
    types = (("-admin$", "bounces"),
             ("-bounces$", "bounces"),
             ("-join$", "join"),
             ("-leave$", "leave"),
             ("-owner$", "owner"),
             ("-request$", "request"))

    for i in types:
        if re.search(i[0],local):
            type = i[1]
            local = re.sub(i[0],"",local)

    if os.path.exists(local):
        os.execv(MailmanHome + "/mail/mailman",
                 (MailmanHome + "/mail/mailman", type, local))
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
    sys.stderr.write("Line %d\n" % (trace.tb_lineno))
    sys.exit(111)       # Soft failure, try again later.
