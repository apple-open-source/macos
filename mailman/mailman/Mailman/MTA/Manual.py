# Copyright (C) 2001-2003 by the Free Software Foundation, Inc.
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

"""Creation/deletion hooks for manual /etc/aliases files."""

import sys
from cStringIO import StringIO

from Mailman import mm_cfg
from Mailman import Message
from Mailman import Utils
from Mailman.Queue.sbcache import get_switchboard
from Mailman.i18n import _
from Mailman.MTA.Utils import makealiases

try:
    True, False
except NameError:
    True = 1
    False = 0



# no-ops for interface compliance
def makelock():
    class Dummy:
        def lock(self):
            pass
        def unlock(self, unconditionally=False):
            pass
    return Dummy()


def clear():
    pass



# nolock argument is ignored, but exists for interface compliance
def create(mlist, cgi=False, nolock=False, quiet=False):
    if mlist is None:
        return
    listname = mlist.internal_name()
    fieldsz = len(listname) + len('-unsubscribe')
    if cgi:
        # If a list is being created via the CGI, the best we can do is send
        # an email message to mailman-owner requesting that the proper aliases
        # be installed.
        sfp = StringIO()
        if not quiet:
            print >> sfp, _("""\
The mailing list `%(listname)s' has been created via the through-the-web
interface.  In order to complete the activation of this mailing list, the
proper /etc/aliases (or equivalent) file must be updated.  The program
`newaliases' may also have to be run.

Here are the entries for the /etc/aliases file:
""")
        outfp = sfp
    else:
        if not quiet:
            print _("""\
To finish creating your mailing list, you must edit your /etc/aliases (or
equivalent) file by adding the following lines, and possibly running the
`newaliases' program:
""")
        print _("""\
## %(listname)s mailing list""")
        outfp = sys.stdout
    # Common path
    for k, v in makealiases(listname):
        print >> outfp, k + ':', ((fieldsz - len(k)) * ' '), v
    # If we're using the command line interface, we're done.  For ttw, we need
    # to actually send the message to mailman-owner now.
    if not cgi:
        print >> outfp
        return
    # Send the message to the site -owner so someone can do something about
    # this request.
    siteowner = Utils.get_site_email(extra='owner')
    # Should this be sent in the site list's preferred language?
    msg = Message.UserNotification(
        siteowner, siteowner,
        _('Mailing list creation request for list %(listname)s'),
        sfp.getvalue(), mm_cfg.DEFAULT_SERVER_LANGUAGE)
    outq = get_switchboard(mm_cfg.OUTQUEUE_DIR)
    outq.enqueue(msg, recips=[siteowner])



def remove(mlist, cgi=False):
    listname = mlist.internal_name()
    fieldsz = len(listname) + len('-unsubscribe')
    if cgi:
        # If a list is being removed via the CGI, the best we can do is send
        # an email message to mailman-owner requesting that the appropriate
        # aliases be deleted.
        sfp = StringIO()
        print >> sfp, _("""\
The mailing list `%(listname)s' has been removed via the through-the-web
interface.  In order to complete the de-activation of this mailing list, the
appropriate /etc/aliases (or equivalent) file must be updated.  The program
`newaliases' may also have to be run.

Here are the entries in the /etc/aliases file that should be removed:
""")
        outfp = sfp
    else:
        print _("""
To finish removing your mailing list, you must edit your /etc/aliases (or
equivalent) file by removing the following lines, and possibly running the
`newaliases' program:

## %(listname)s mailing list""")
        outfp = sys.stdout
    # Common path
    for k, v in makealiases(listname):
        print >> outfp, k + ':', ((fieldsz - len(k)) * ' '), v
    # If we're using the command line interface, we're done.  For ttw, we need
    # to actually send the message to mailman-owner now.
    if not cgi:
        print >> outfp
        return
    siteowner = Utils.get_site_email(extra='owner')
    # Should this be sent in the site list's preferred language?
    msg = Message.UserNotification(
        siteowner, siteowner,
        _('Mailing list removal request for list %(listname)s'),
        sfp.getvalue(), mm_cfg.DEFAULT_SERVER_LANGUAGE)
    outq = get_switchboard(mm_cfg.OUTQUEUE_DIR)
    outq.enqueue(msg, recips=[siteowner])
