# Copyright (C) 1998-2003 by the Free Software Foundation, Inc.
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

"""Provide a password-interface wrapper around private archives.
"""

import os
import sys
import cgi
import mimetypes

from Mailman import mm_cfg
from Mailman import Utils
from Mailman import MailList
from Mailman import Errors
from Mailman import i18n
from Mailman.htmlformat import *
from Mailman.Logging.Syslog import syslog

# Set up i18n.  Until we know which list is being requested, we use the
# server's default.
_ = i18n._
i18n.set_language(mm_cfg.DEFAULT_SERVER_LANGUAGE)

SLASH = '/'



def true_path(path):
    "Ensure that the path is safe by removing .."
    parts = path.split(SLASH)
    safe = [x for x in parts if x not in ('.', '..')]
    if parts <> safe:
        syslog('mischief', 'Directory traversal attack thwarted')
    return SLASH.join(safe)[1:]



def guess_type(url, strict):
    if hasattr(mimetypes, 'common_types'):
        return mimetypes.guess_type(url, strict)
    return mimetypes.guess_type(url)



def main():
    doc = Document()
    doc.set_language(mm_cfg.DEFAULT_SERVER_LANGUAGE)

    parts = Utils.GetPathPieces()
    if not parts:
        doc.SetTitle(_("Private Archive Error"))
        doc.AddItem(Header(3, _("You must specify a list.")))
        print doc.Format()
        return

    path = os.environ.get('PATH_INFO')
    # BAW: This needs to be converted to the Site module abstraction
    true_filename = os.path.join(
        mm_cfg.PRIVATE_ARCHIVE_FILE_DIR,
        true_path(path))

    listname = parts[0].lower()
    mboxfile = ''
    if len(parts) > 1:
        mboxfile = parts[1]

    # See if it's the list's mbox file is being requested
    if listname.endswith('.mbox') and mboxfile.endswith('.mbox') and \
           listname[:-5] == mboxfile[:-5]:
        listname = listname[:-5]
    else:
        mboxfile = ''

    # If it's a directory, we have to append index.html in this script.  We
    # must also check for a gzipped file, because the text archives are
    # usually stored in compressed form.
    if os.path.isdir(true_filename):
        true_filename = true_filename + '/index.html'
    if not os.path.exists(true_filename) and \
           os.path.exists(true_filename + '.gz'):
        true_filename = true_filename + '.gz'

    try:
        mlist = MailList.MailList(listname, lock=0)
    except Errors.MMListError, e:
        # Avoid cross-site scripting attacks
        safelistname = Utils.websafe(listname)
        msg = _('No such list <em>%(safelistname)s</em>')
        doc.SetTitle(_("Private Archive Error - %(msg)s"))
        doc.AddItem(Header(2, msg))
        print doc.Format()
        syslog('error', 'No such list "%s": %s\n', listname, e)
        return

    i18n.set_language(mlist.preferred_language)
    doc.set_language(mlist.preferred_language)

    cgidata = cgi.FieldStorage()
    username = cgidata.getvalue('username', '')
    password = cgidata.getvalue('password', '')

    is_auth = 0
    realname = mlist.real_name
    message = ''

    if not mlist.WebAuthenticate((mm_cfg.AuthUser,
                                  mm_cfg.AuthListModerator,
                                  mm_cfg.AuthListAdmin,
                                  mm_cfg.AuthSiteAdmin),
                                 password, username):
        if cgidata.has_key('submit'):
            # This is a re-authorization attempt
            message = Bold(FontSize('+1', _('Authorization failed.'))).Format()
        # Output the password form
        charset = Utils.GetCharSet(mlist.preferred_language)
        print 'Content-type: text/html; charset=' + charset + '\n\n'
        while path and path[0] == '/':
            path=path[1:]  # Remove leading /'s
        print Utils.maketext(
            'private.html',
            {'action'  : mlist.GetScriptURL('private', absolute=1),
             'realname': mlist.real_name,
             'message' : message,
             }, mlist=mlist)
        return

    lang = mlist.getMemberLanguage(username)
    i18n.set_language(lang)
    doc.set_language(lang)

    # Authorization confirmed... output the desired file
    try:
        ctype, enc = guess_type(path, strict=0)
        if ctype is None:
            ctype = 'text/html'
        if mboxfile:
            f = open(os.path.join(mlist.archive_dir() + '.mbox',
                                  mlist.internal_name() + '.mbox'))
            ctype = 'text/plain'
        elif true_filename.endswith('.gz'):
            import gzip
            f = gzip.open(true_filename, 'r')
        else:
            f = open(true_filename, 'r')
    except IOError:
        msg = _('Private archive file not found')
        doc.SetTitle(msg)
        doc.AddItem(Header(2, msg))
        print doc.Format()
        syslog('error', 'Private archive file not found: %s', true_filename)
    else:
        print 'Content-type: %s\n' % ctype
        sys.stdout.write(f.read())
        f.close()
