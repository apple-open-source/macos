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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
# USA.

"""Script which implements admin editing of the list's html templates."""

import os
import cgi
import errno
import re

from Mailman import Utils
from Mailman import MailList
from Mailman.htmlformat import *
from Mailman.HTMLFormatter import HTMLFormatter
from Mailman import Errors
from Mailman.Cgi import Auth
from Mailman.Logging.Syslog import syslog
from Mailman import i18n

_ = i18n._



def main():
    # Trick out pygettext since we want to mark template_data as translatable,
    # but we don't want to actually translate it here.
    def _(s):
        return s

    template_data = (
        ('listinfo.html',    _('General list information page')),
        ('subscribe.html',   _('Subscribe results page')),
        ('options.html',     _('User specific options page')),
        ('subscribeack.txt', _('Welcome email text file')),
        )

    _ = i18n._
    doc = Document()

    # Set up the system default language
    i18n.set_language(mm_cfg.DEFAULT_SERVER_LANGUAGE)
    doc.set_language(mm_cfg.DEFAULT_SERVER_LANGUAGE)

    parts = Utils.GetPathPieces()
    if not parts:
        doc.AddItem(Header(2, _("List name is required.")))
        print doc.Format()
        return

    listname = parts[0].lower()
    try:
        mlist = MailList.MailList(listname, lock=0)
    except Errors.MMListError, e:
        # Avoid cross-site scripting attacks
        safelistname = Utils.websafe(listname)
        doc.AddItem(Header(2, _('No such list <em>%(safelistname)s</em>')))
        print doc.Format()
        syslog('error', 'No such list "%s": %s', listname, e)
        return

    # Now that we have a valid list, set the language to its default
    i18n.set_language(mlist.preferred_language)
    doc.set_language(mlist.preferred_language)

    # Must be authenticated to get any farther
    cgidata = cgi.FieldStorage()

    # Editing the html for a list is limited to the list admin and site admin.
    if not mlist.WebAuthenticate((mm_cfg.AuthListAdmin,
                                  mm_cfg.AuthSiteAdmin),
                                 cgidata.getvalue('adminpw', '')):
        if cgidata.has_key('admlogin'):
            # This is a re-authorization attempt
            msg = Bold(FontSize('+1', _('Authorization failed.'))).Format()
        else:
            msg = ''
        Auth.loginpage(mlist, 'admin', msg=msg)
        return

    realname = mlist.real_name
    if len(parts) > 1:
        template_name = parts[1]
        for (template, info) in template_data:
            if template == template_name:
                template_info = _(info)
                doc.SetTitle(_(
                    '%(realname)s -- Edit html for %(template_info)s'))
                break
        else:
            # Avoid cross-site scripting attacks
            safetemplatename = Utils.websafe(template_name)
            doc.SetTitle(_('Edit HTML : Error'))
            doc.AddItem(Header(2, _("%(safetemplatename)s: Invalid template")))
            doc.AddItem(mlist.GetMailmanFooter())
            print doc.Format()
            return
    else:
        doc.SetTitle(_('%(realname)s -- HTML Page Editing'))
        doc.AddItem(Header(1, _('%(realname)s -- HTML Page Editing')))
        doc.AddItem(Header(2, _('Select page to edit:')))
        template_list = UnorderedList()
        for (template, info) in template_data:
            l = Link(mlist.GetScriptURL('edithtml') + '/' + template, _(info))
            template_list.AddItem(l)
        doc.AddItem(FontSize("+2", template_list))
        doc.AddItem(mlist.GetMailmanFooter())
        print doc.Format()
        return

    try:
        if cgidata.keys():
            ChangeHTML(mlist, cgidata, template_name, doc)
        FormatHTML(mlist, doc, template_name, template_info)
    finally:
        doc.AddItem(mlist.GetMailmanFooter())
        print doc.Format()



def FormatHTML(mlist, doc, template_name, template_info):
    doc.AddItem(Header(1,'%s:' % mlist.real_name))
    doc.AddItem(Header(1, template_info))
    doc.AddItem('<hr>')

    link = Link(mlist.GetScriptURL('admin'),
                _('View or edit the list configuration information.'))

    doc.AddItem(FontSize("+1", link))
    doc.AddItem('<p>')
    doc.AddItem('<hr>')
    form = Form(mlist.GetScriptURL('edithtml') + '/' + template_name)
    text = Utils.maketext(template_name, raw=1, mlist=mlist)
    # MAS: Don't websafe twice.  TextArea does it.
    form.AddItem(TextArea('html_code', text, rows=40, cols=75))
    form.AddItem('<p>' + _('When you are done making changes...'))
    form.AddItem(SubmitButton('submit', _('Submit Changes')))
    doc.AddItem(form)



def ChangeHTML(mlist, cgi_info, template_name, doc):
    if not cgi_info.has_key('html_code'):
        doc.AddItem(Header(3,_("Can't have empty html page.")))
        doc.AddItem(Header(3,_("HTML Unchanged.")))
        doc.AddItem('<hr>')
        return
    code = cgi_info['html_code'].value
    if Utils.suspiciousHTML(code):
        doc.AddItem(Header(3,
           _("""The page you saved contains suspicious HTML that could
potentially expose your users to cross-site scripting attacks.  This change
has therefore been rejected.  If you still want to make these changes, you
must have shell access to your Mailman server.
             """)))
        doc.AddItem(_('See '))
        doc.AddItem(Link(
'http://wiki.list.org/x/jYA9',
                _('FAQ 4.48.')))
        doc.AddItem(Header(3,_("Page Unchanged.")))
        doc.AddItem('<hr>')
        return
    langdir = os.path.join(mlist.fullpath(), mlist.preferred_language)
    # Make sure the directory exists
    omask = os.umask(0)
    try:
        try:
            os.mkdir(langdir, 02775)
        except OSError, e:
            if e.errno <> errno.EEXIST: raise
    finally:
        os.umask(omask)
    fp = open(os.path.join(langdir, template_name), 'w')
    try:
        fp.write(code)
    finally:
        fp.close()
    doc.AddItem(Header(3, _('HTML successfully updated.')))
    doc.AddItem('<hr>')
