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

"""Create mailing lists through the web."""

import sys
import os
import signal
import cgi
from types import ListType

from Mailman import mm_cfg
from Mailman import MailList
from Mailman import Message
from Mailman import Errors
from Mailman import i18n
from Mailman.htmlformat import *
from Mailman.Logging.Syslog import syslog
from Mailman.Utils import sha_new

# Set up i18n
_ = i18n._
i18n.set_language(mm_cfg.DEFAULT_SERVER_LANGUAGE)



def main():
    doc = Document()
    doc.set_language(mm_cfg.DEFAULT_SERVER_LANGUAGE)

    cgidata = cgi.FieldStorage()
    parts = Utils.GetPathPieces()
    if parts:
        # Bad URL specification
        title = _('Bad URL specification')
        doc.SetTitle(title)
        doc.AddItem(
            Header(3, Bold(FontAttr(title, color='#ff0000', size='+2'))))
        syslog('error', 'Bad URL specification: %s', parts)
    elif cgidata.has_key('doit'):
        # We must be processing the list creation request
        process_request(doc, cgidata)
    elif cgidata.has_key('clear'):
        request_creation(doc)
    else:
        # Put up the list creation request form
        request_creation(doc)
    doc.AddItem('<hr>')
    # Always add the footer and print the document
    doc.AddItem(_('Return to the ') +
                Link(Utils.ScriptURL('listinfo'),
                     _('general list overview')).Format())
    doc.AddItem(_('<br>Return to the ') +
                Link(Utils.ScriptURL('admin'),
                     _('administrative list overview')).Format())
    doc.AddItem(MailmanLogo())
    print doc.Format()



def process_request(doc, cgidata):
    # Lowercase the listname since this is treated as the "internal" name.
    listname = cgidata.getvalue('listname', '').strip().lower()
    owner    = cgidata.getvalue('owner', '').strip()
    try:
        autogen  = int(cgidata.getvalue('autogen', '0'))
    except ValueError:
        autogen = 0
    try:
        notify  = int(cgidata.getvalue('notify', '0'))
    except ValueError:
        notify = 0
    try:
        moderate = int(cgidata.getvalue('moderate',
                       mm_cfg.DEFAULT_DEFAULT_MEMBER_MODERATION))
    except ValueError:
        moderate = mm_cfg.DEFAULT_DEFAULT_MEMBER_MODERATION

    password = cgidata.getvalue('password', '').strip()
    confirm  = cgidata.getvalue('confirm', '').strip()
    auth     = cgidata.getvalue('auth', '').strip()
    langs    = cgidata.getvalue('langs', [mm_cfg.DEFAULT_SERVER_LANGUAGE])

    if not isinstance(langs, ListType):
        langs = [langs]
    # Sanity check
    safelistname = Utils.websafe(listname)
    if '@' in listname:
        request_creation(doc, cgidata,
                         _('List name must not include "@": %(safelistname)s'))
        return
    if Utils.list_exists(listname):
        # BAW: should we tell them the list already exists?  This could be
        # used to mine/guess the existance of non-advertised lists.  Then
        # again, that can be done in other ways already, so oh well.
        request_creation(doc, cgidata,
                         _('List already exists: %(safelistname)s'))
        return
    if not listname:
        request_creation(doc, cgidata,
                         _('You forgot to enter the list name'))
        return
    if not owner:
        request_creation(doc, cgidata,
                         _('You forgot to specify the list owner'))
        return

    if autogen:
        if password or confirm:
            request_creation(
                doc, cgidata,
                _('''Leave the initial password (and confirmation) fields
                blank if you want Mailman to autogenerate the list
                passwords.'''))
            return
        password = confirm = Utils.MakeRandomPassword(
            mm_cfg.ADMIN_PASSWORD_LENGTH)
    else:
        if password <> confirm:
            request_creation(doc, cgidata,
                             _('Initial list passwords do not match'))
            return
        if not password:
            request_creation(
                doc, cgidata,
                # The little <!-- ignore --> tag is used so that this string
                # differs from the one in bin/newlist.  The former is destined
                # for the web while the latter is destined for email, so they
                # must be different entries in the message catalog.
                _('The list password cannot be empty<!-- ignore -->'))
            return
    # The authorization password must be non-empty, and it must match either
    # the list creation password or the site admin password
    ok = 0
    if auth:
        ok = Utils.check_global_password(auth, 0)
        if not ok:
            ok = Utils.check_global_password(auth)
    if not ok:
        request_creation(
            doc, cgidata,
            _('You are not authorized to create new mailing lists'))
        return
    # Make sure the web hostname matches one of our virtual domains
    hostname = Utils.get_domain()
    if mm_cfg.VIRTUAL_HOST_OVERVIEW and \
           not mm_cfg.VIRTUAL_HOSTS.has_key(hostname):
        safehostname = Utils.websafe(hostname)
        request_creation(doc, cgidata,
                         _('Unknown virtual host: %(safehostname)s'))
        return
    emailhost = mm_cfg.VIRTUAL_HOSTS.get(hostname, mm_cfg.DEFAULT_EMAIL_HOST)
    # We've got all the data we need, so go ahead and try to create the list
    # See admin.py for why we need to set up the signal handler.
    mlist = MailList.MailList()

    def sigterm_handler(signum, frame, mlist=mlist):
        # Make sure the list gets unlocked...
        mlist.Unlock()
        # ...and ensure we exit, otherwise race conditions could cause us to
        # enter MailList.Save() while we're in the unlocked state, and that
        # could be bad!
        sys.exit(0)

    try:
        # Install the emergency shutdown signal handler
        signal.signal(signal.SIGTERM, sigterm_handler)

        pw = sha_new(password).hexdigest()
        # Guarantee that all newly created files have the proper permission.
        # proper group ownership should be assured by the autoconf script
        # enforcing that all directories have the group sticky bit set
        oldmask = os.umask(002)
        try:
            try:
                mlist.Create(listname, owner, pw, langs, emailhost)
            finally:
                os.umask(oldmask)
        except Errors.EmailAddressError, e:
            if e.args:
                s = Utils.websafe(e.args[0])
            else:
                s = Utils.websafe(owner)
            request_creation(doc, cgidata,
                             _('Bad owner email address: %(s)s'))
            return
        except Errors.MMListAlreadyExistsError:
            # MAS: List already exists so we don't need to websafe it.
            request_creation(doc, cgidata,
                             _('List already exists: %(listname)s'))
            return
        except Errors.BadListNameError, e:
            if e.args:
                s = Utils.websafe(e.args[0])
            else:
                s = Utils.websafe(listname)
            request_creation(doc, cgidata,
                             _('Illegal list name: %(s)s'))
            return
        except Errors.MMListError:
            request_creation(
                doc, cgidata,
                _('''Some unknown error occurred while creating the list.
                Please contact the site administrator for assistance.'''))
            return

        # Initialize the host_name and web_page_url attributes, based on
        # virtual hosting settings and the request environment variables.
        mlist.default_member_moderation = moderate
        mlist.web_page_url = mm_cfg.DEFAULT_URL_PATTERN % hostname
        mlist.host_name = emailhost
        mlist.Save()
    finally:
        # Now be sure to unlock the list.  It's okay if we get a signal here
        # because essentially, the signal handler will do the same thing.  And
        # unlocking is unconditional, so it's not an error if we unlock while
        # we're already unlocked.
        mlist.Unlock()

    # Now do the MTA-specific list creation tasks
    if mm_cfg.MTA:
        modname = 'Mailman.MTA.' + mm_cfg.MTA
        __import__(modname)
        sys.modules[modname].create(mlist, cgi=1)

    # And send the notice to the list owner.
    if notify:
        siteowner = Utils.get_site_email(mlist.host_name, 'owner')
        text = Utils.maketext(
            'newlist.txt',
            {'listname'    : listname,
             'password'    : password,
             'admin_url'   : mlist.GetScriptURL('admin', absolute=1),
             'listinfo_url': mlist.GetScriptURL('listinfo', absolute=1),
             'requestaddr' : mlist.GetRequestEmail(),
             'siteowner'   : siteowner,
             }, mlist=mlist)
        msg = Message.UserNotification(
            owner, siteowner,
            _('Your new mailing list: %(listname)s'),
            text, mlist.preferred_language)
        msg.send(mlist)

    # Success!
    listinfo_url = mlist.GetScriptURL('listinfo', absolute=1)
    admin_url = mlist.GetScriptURL('admin', absolute=1)
    create_url = Utils.ScriptURL('create')

    title = _('Mailing list creation results')
    doc.SetTitle(title)
    table = Table(border=0, width='100%')
    table.AddRow([Center(Bold(FontAttr(title, size='+1')))])
    table.AddCellInfo(table.GetCurrentRowIndex(), 0,
                      bgcolor=mm_cfg.WEB_HEADER_COLOR)
    table.AddRow([_('''You have successfully created the mailing list
    <b>%(listname)s</b> and notification has been sent to the list owner
    <b>%(owner)s</b>.  You can now:''')])
    ullist = UnorderedList()
    ullist.AddItem(Link(listinfo_url, _("Visit the list's info page")))
    ullist.AddItem(Link(admin_url, _("Visit the list's admin page")))
    ullist.AddItem(Link(create_url, _('Create another list')))
    table.AddRow([ullist])
    doc.AddItem(table)



# Because the cgi module blows
class Dummy:
    def getvalue(self, name, default):
        return default
dummy = Dummy()



def request_creation(doc, cgidata=dummy, errmsg=None):
    # What virtual domain are we using?
    hostname = Utils.get_domain()
    # Set up the document
    title = _('Create a %(hostname)s Mailing List')
    doc.SetTitle(title)
    table = Table(border=0, width='100%')
    table.AddRow([Center(Bold(FontAttr(title, size='+1')))])
    table.AddCellInfo(table.GetCurrentRowIndex(), 0,
                      bgcolor=mm_cfg.WEB_HEADER_COLOR)
    # Add any error message
    if errmsg:
        table.AddRow([Header(3, Bold(
            FontAttr(_('Error: '), color='#ff0000', size='+2').Format() +
            Italic(errmsg).Format()))])
    table.AddRow([_("""You can create a new mailing list by entering the
    relevant information into the form below.  The name of the mailing list
    will be used as the primary address for posting messages to the list, so
    it should be lowercased.  You will not be able to change this once the
    list is created.

    <p>You also need to enter the email address of the initial list owner.
    Once the list is created, the list owner will be given notification, along
    with the initial list password.  The list owner will then be able to
    modify the password and add or remove additional list owners.

    <p>If you want Mailman to automatically generate the initial list admin
    password, click on `Yes' in the autogenerate field below, and leave the
    initial list password fields empty.

    <p>You must have the proper authorization to create new mailing lists.
    Each site should have a <em>list creator's</em> password, which you can
    enter in the field at the bottom.  Note that the site administrator's
    password can also be used for authentication.
    """)])
    # Build the form for the necessary input
    GREY = mm_cfg.WEB_ADMINITEM_COLOR
    form = Form(Utils.ScriptURL('create'))
    ftable = Table(border=0, cols='2', width='100%',
                   cellspacing=3, cellpadding=4)

    ftable.AddRow([Center(Italic(_('List Identity')))])
    ftable.AddCellInfo(ftable.GetCurrentRowIndex(), 0, colspan=2)

    listname = cgidata.getvalue('listname', '')
    # MAS: Don't websafe twice.  TextBox does it.
    ftable.AddRow([Label(_('Name of list:')),
                   TextBox('listname', listname)])
    ftable.AddCellInfo(ftable.GetCurrentRowIndex(), 0, bgcolor=GREY)
    ftable.AddCellInfo(ftable.GetCurrentRowIndex(), 1, bgcolor=GREY)

    owner = cgidata.getvalue('owner', '')
    # MAS: Don't websafe twice.  TextBox does it.
    ftable.AddRow([Label(_('Initial list owner address:')),
                   TextBox('owner', owner)])
    ftable.AddCellInfo(ftable.GetCurrentRowIndex(), 0, bgcolor=GREY)
    ftable.AddCellInfo(ftable.GetCurrentRowIndex(), 1, bgcolor=GREY)

    try:
        autogen = int(cgidata.getvalue('autogen', '0'))
    except ValueError:
        autogen = 0
    ftable.AddRow([Label(_('Auto-generate initial list password?')),
                   RadioButtonArray('autogen', (_('No'), _('Yes')),
                                    checked=autogen,
                                    values=(0, 1))])
    ftable.AddCellInfo(ftable.GetCurrentRowIndex(), 0, bgcolor=GREY)
    ftable.AddCellInfo(ftable.GetCurrentRowIndex(), 1, bgcolor=GREY)

    safepasswd = Utils.websafe(cgidata.getvalue('password', ''))
    ftable.AddRow([Label(_('Initial list password:')),
                   PasswordBox('password', safepasswd)])
    ftable.AddCellInfo(ftable.GetCurrentRowIndex(), 0, bgcolor=GREY)
    ftable.AddCellInfo(ftable.GetCurrentRowIndex(), 1, bgcolor=GREY)

    safeconfirm = Utils.websafe(cgidata.getvalue('confirm', ''))
    ftable.AddRow([Label(_('Confirm initial password:')),
                   PasswordBox('confirm', safeconfirm)])
    ftable.AddCellInfo(ftable.GetCurrentRowIndex(), 0, bgcolor=GREY)
    ftable.AddCellInfo(ftable.GetCurrentRowIndex(), 1, bgcolor=GREY)

    try:
        notify = int(cgidata.getvalue('notify', '1'))
    except ValueError:
        notify = 1
    try:
        moderate = int(cgidata.getvalue('moderate',
                       mm_cfg.DEFAULT_DEFAULT_MEMBER_MODERATION))
    except ValueError:
        moderate = mm_cfg.DEFAULT_DEFAULT_MEMBER_MODERATION

    ftable.AddRow([Center(Italic(_('List Characteristics')))])
    ftable.AddCellInfo(ftable.GetCurrentRowIndex(), 0, colspan=2)

    ftable.AddRow([
        Label(_("""Should new members be quarantined before they
    are allowed to post unmoderated to this list?  Answer <em>Yes</em> to hold
    new member postings for moderator approval by default.""")),
        RadioButtonArray('moderate', (_('No'), _('Yes')),
                         checked=moderate,
                         values=(0,1))])
    ftable.AddCellInfo(ftable.GetCurrentRowIndex(), 0, bgcolor=GREY)
    ftable.AddCellInfo(ftable.GetCurrentRowIndex(), 1, bgcolor=GREY)
    # Create the table of initially supported languages, sorted on the long
    # name of the language.
    revmap = {}
    for key, (name, charset, direction) in mm_cfg.LC_DESCRIPTIONS.items():
        revmap[_(name)] = key
    langnames = revmap.keys()
    langnames.sort()
    langs = []
    for name in langnames:
        langs.append(revmap[name])
    try:
        langi = langs.index(mm_cfg.DEFAULT_SERVER_LANGUAGE)
    except ValueError:
        # Someone must have deleted the servers's preferred language.  Could
        # be other trouble lurking!
        langi = 0
    # BAW: we should preserve the list of checked languages across form
    # invocations.
    checked = [0] * len(langs)
    checked[langi] = 1
    deflang = _(Utils.GetLanguageDescr(mm_cfg.DEFAULT_SERVER_LANGUAGE))
    ftable.AddRow([Label(_(
        '''Initial list of supported languages.  <p>Note that if you do not
        select at least one initial language, the list will use the server
        default language of %(deflang)s''')),
                   CheckBoxArray('langs',
                                 [_(Utils.GetLanguageDescr(L)) for L in langs],
                                 checked=checked,
                                 values=langs)])
    ftable.AddCellInfo(ftable.GetCurrentRowIndex(), 0, bgcolor=GREY)
    ftable.AddCellInfo(ftable.GetCurrentRowIndex(), 1, bgcolor=GREY)

    ftable.AddRow([Label(_('Send "list created" email to list owner?')),
                   RadioButtonArray('notify', (_('No'), _('Yes')),
                                    checked=notify,
                                    values=(0, 1))])
    ftable.AddCellInfo(ftable.GetCurrentRowIndex(), 0, bgcolor=GREY)
    ftable.AddCellInfo(ftable.GetCurrentRowIndex(), 1, bgcolor=GREY)

    ftable.AddRow(['<hr>'])
    ftable.AddCellInfo(ftable.GetCurrentRowIndex(), 0, colspan=2)
    ftable.AddRow([Label(_("List creator's (authentication) password:")),
                   PasswordBox('auth')])
    ftable.AddCellInfo(ftable.GetCurrentRowIndex(), 0, bgcolor=GREY)
    ftable.AddCellInfo(ftable.GetCurrentRowIndex(), 1, bgcolor=GREY)

    ftable.AddRow([Center(SubmitButton('doit', _('Create List'))),
                   Center(SubmitButton('clear', _('Clear Form')))])
    form.AddItem(ftable)
    table.AddRow([form])
    doc.AddItem(table)
