# Copyright (C) 2001-2009 by the Free Software Foundation, Inc.
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

"""Confirm a pending action via URL."""

import signal
import cgi
import time

from Mailman import mm_cfg
from Mailman import Errors
from Mailman import i18n
from Mailman import MailList
from Mailman import Pending
from Mailman.UserDesc import UserDesc
from Mailman.htmlformat import *
from Mailman.Logging.Syslog import syslog

# Set up i18n
_ = i18n._
i18n.set_language(mm_cfg.DEFAULT_SERVER_LANGUAGE)

try:
    True, False
except NameError:
    True = 1
    False = 0



def main():
    doc = Document()
    doc.set_language(mm_cfg.DEFAULT_SERVER_LANGUAGE)

    parts = Utils.GetPathPieces()
    if not parts or len(parts) < 1:
        bad_confirmation(doc)
        doc.AddItem(MailmanLogo())
        print doc.Format()
        return

    listname = parts[0].lower()
    try:
        mlist = MailList.MailList(listname, lock=0)
    except Errors.MMListError, e:
        # Avoid cross-site scripting attacks
        safelistname = Utils.websafe(listname)
        bad_confirmation(doc, _('No such list <em>%(safelistname)s</em>'))
        doc.AddItem(MailmanLogo())
        print doc.Format()
        syslog('error', 'No such list "%s": %s', listname, e)
        return

    # Set the language for the list
    i18n.set_language(mlist.preferred_language)
    doc.set_language(mlist.preferred_language)

    # Get the form data to see if this is a second-step confirmation
    cgidata = cgi.FieldStorage(keep_blank_values=1)
    cookie = cgidata.getvalue('cookie')
    if cookie == '':
        ask_for_cookie(mlist, doc, _('Confirmation string was empty.'))
        return

    if not cookie and len(parts) == 2:
        cookie = parts[1]

    if len(parts) > 2:
        bad_confirmation(doc)
        doc.AddItem(mlist.GetMailmanFooter())
        print doc.Format()
        return

    if not cookie:
        ask_for_cookie(mlist, doc)
        return

    days = int(mm_cfg.PENDING_REQUEST_LIFE / mm_cfg.days(1) + 0.5)
    confirmurl = mlist.GetScriptURL('confirm', absolute=1)
    # Avoid cross-site scripting attacks
    safecookie = Utils.websafe(cookie)
    badconfirmstr = _('''<b>Invalid confirmation string:</b>
    %(safecookie)s.

    <p>Note that confirmation strings expire approximately
    %(days)s days after the initial subscription request.  If your
    confirmation has expired, please try to re-submit your subscription.
    Otherwise, <a href="%(confirmurl)s">re-enter</a> your confirmation
    string.''')

    content = mlist.pend_confirm(cookie, expunge=False)
    if content is None:
        bad_confirmation(doc, badconfirmstr)
        doc.AddItem(mlist.GetMailmanFooter())
        print doc.Format()
        return

    try:
        if content[0] == Pending.SUBSCRIPTION:
            if cgidata.getvalue('cancel'):
                subscription_cancel(mlist, doc, cookie)
            elif cgidata.getvalue('submit'):
                subscription_confirm(mlist, doc, cookie, cgidata)
            else:
                subscription_prompt(mlist, doc, cookie, content[1])
        elif content[0] == Pending.UNSUBSCRIPTION:
            try:
                if cgidata.getvalue('cancel'):
                    unsubscription_cancel(mlist, doc, cookie)
                elif cgidata.getvalue('submit'):
                    unsubscription_confirm(mlist, doc, cookie)
                else:
                    unsubscription_prompt(mlist, doc, cookie, *content[1:])
            except Errors.NotAMemberError:
                doc.addError(_("""The address requesting unsubscription is not
                a member of the mailing list.  Perhaps you have already been
                unsubscribed, e.g. by the list administrator?"""))
                # Expunge this record from the pending database.
                expunge(mlist, cookie)
        elif content[0] == Pending.CHANGE_OF_ADDRESS:
            if cgidata.getvalue('cancel'):
                addrchange_cancel(mlist, doc, cookie)
            elif cgidata.getvalue('submit'):
                addrchange_confirm(mlist, doc, cookie)
            else:
                # Watch out for users who have unsubscribed themselves in the
                # meantime!
                try:
                    addrchange_prompt(mlist, doc, cookie, *content[1:])
                except Errors.NotAMemberError:
                    doc.addError(_("""The address requesting to be changed has
                    been subsequently unsubscribed.  This request has been
                    cancelled."""))
                    # Expunge this record from the pending database.
                    expunge(mlist, cookie)
        elif content[0] == Pending.HELD_MESSAGE:
            if cgidata.getvalue('cancel'):
                heldmsg_cancel(mlist, doc, cookie)
            elif cgidata.getvalue('submit'):
                heldmsg_confirm(mlist, doc, cookie)
            else:
                heldmsg_prompt(mlist, doc, cookie, *content[1:])
        elif content[0] == Pending.RE_ENABLE:
            if cgidata.getvalue('cancel'):
                reenable_cancel(mlist, doc, cookie)
            elif cgidata.getvalue('submit'):
                reenable_confirm(mlist, doc, cookie)
            else:
                reenable_prompt(mlist, doc, cookie, *content[1:])
        else:
            bad_confirmation(doc, _('System error, bad content: %(content)s'))
    except Errors.MMBadConfirmation:
        bad_confirmation(doc, badconfirmstr)

    doc.AddItem(mlist.GetMailmanFooter())
    print doc.Format()



def bad_confirmation(doc, extra=''):
    title = _('Bad confirmation string')
    doc.SetTitle(title)
    doc.AddItem(Header(3, Bold(FontAttr(title, color='#ff0000', size='+2'))))
    doc.AddItem(extra)


def expunge(mlist, cookie):
    # Expunge this record from the list's pending database.  This requires
    # that the list lock be acquired, however the list doesn't need to be
    # saved because this operation doesn't touch the config.pck file.
    mlist.Lock()
    try:
        mlist.pend_confirm(cookie, expunge=True)
    finally:
        mlist.Unlock()



def ask_for_cookie(mlist, doc, extra=''):
    title = _('Enter confirmation cookie')
    doc.SetTitle(title)
    form = Form(mlist.GetScriptURL('confirm', 1))
    table = Table(border=0, width='100%')
    table.AddRow([Center(Bold(FontAttr(title, size='+1')))])
    table.AddCellInfo(table.GetCurrentRowIndex(), 0,
                      colspan=2, bgcolor=mm_cfg.WEB_HEADER_COLOR)

    if extra:
        table.AddRow([Bold(FontAttr(extra, size='+1'))])
        table.AddCellInfo(table.GetCurrentRowIndex(), 0, colspan=2)

    # Add cookie entry box
    table.AddRow([_("""Please enter the confirmation string
    (i.e. <em>cookie</em>) that you received in your email message, in the box
    below.  Then hit the <em>Submit</em> button to proceed to the next
    confirmation step.""")])
    table.AddCellInfo(table.GetCurrentRowIndex(), 0, colspan=2)
    table.AddRow([Label(_('Confirmation string:')),
                  TextBox('cookie')])
    table.AddRow([Center(SubmitButton('submit_cookie', _('Submit')))])
    table.AddCellInfo(table.GetCurrentRowIndex(), 0, colspan=2)
    form.AddItem(table)
    doc.AddItem(form)
    doc.AddItem(mlist.GetMailmanFooter())
    print doc.Format()



def subscription_prompt(mlist, doc, cookie, userdesc):
    email = userdesc.address
    password = userdesc.password
    digest = userdesc.digest
    lang = userdesc.language
    name = Utils.uncanonstr(userdesc.fullname, lang)
    i18n.set_language(lang)
    doc.set_language(lang)
    title = _('Confirm subscription request')
    doc.SetTitle(title)

    form = Form(mlist.GetScriptURL('confirm', 1))
    table = Table(border=0, width='100%')
    table.AddRow([Center(Bold(FontAttr(title, size='+1')))])
    table.AddCellInfo(table.GetCurrentRowIndex(), 0,
                      colspan=2, bgcolor=mm_cfg.WEB_HEADER_COLOR)

    listname = mlist.real_name
    # This is the normal, no-confirmation required results text.
    #
    # We do things this way so we don't have to reformat this paragraph, which
    # would mess up translations.  If you modify this text for other reasons,
    # please refill the paragraph, and clean up the logic.
    result = _("""Your confirmation is required in order to complete the
    subscription request to the mailing list <em>%(listname)s</em>.  Your
    subscription settings are shown below; make any necessary changes and hit
    <em>Subscribe</em> to complete the confirmation process.  Once you've
    confirmed your subscription request, you will be shown your account
    options page which you can use to further customize your membership
    options.

    <p>Note: your password will be emailed to you once your subscription is
    confirmed.  You can change it by visiting your personal options page.

    <p>Or hit <em>Cancel my subscription request</em> if you no longer want to
    subscribe to this list.""") + '<p><hr>'
    if mlist.subscribe_policy in (2, 3):
        # Confirmation is required
        result = _("""Your confirmation is required in order to continue with
        the subscription request to the mailing list <em>%(listname)s</em>.
        Your subscription settings are shown below; make any necessary changes
        and hit <em>Subscribe to list ...</em> to complete the confirmation
        process.  Once you've confirmed your subscription request, the
        moderator must approve or reject your membership request.  You will
        receive notice of their decision.

        <p>Note: your password will be emailed to you once your subscription
        is confirmed.  You can change it by visiting your personal options
        page.

        <p>Or, if you've changed your mind and do not want to subscribe to
        this mailing list, you can hit <em>Cancel my subscription
        request</em>.""") + '<p><hr>'
    table.AddRow([result])
    table.AddCellInfo(table.GetCurrentRowIndex(), 0, colspan=2)

    table.AddRow([Label(_('Your email address:')), email])
    table.AddRow([Label(_('Your real name:')),
                  TextBox('realname', name)])
##    table.AddRow([Label(_('Password:')),
##                  PasswordBox('password', password)])
##    table.AddRow([Label(_('Password (confirm):')),
##                  PasswordBox('pwconfirm', password)])
    # Only give them a choice to receive digests if they actually have a
    # choice <wink>.
    if mlist.nondigestable and mlist.digestable:
        table.AddRow([Label(_('Receive digests?')),
                      RadioButtonArray('digests', (_('No'), _('Yes')),
                                       checked=digest, values=(0, 1))])
    langs = mlist.GetAvailableLanguages()
    values = [_(Utils.GetLanguageDescr(l)) for l in langs]
    try:
        selected = langs.index(lang)
    except ValueError:
        selected = lang.index(mlist.preferred_language)
    table.AddRow([Label(_('Preferred language:')),
                  SelectOptions('language', langs, values, selected)])
    table.AddRow([Hidden('cookie', cookie)])
    table.AddCellInfo(table.GetCurrentRowIndex(), 0, colspan=2)
    table.AddRow([
        Label(SubmitButton('cancel', _('Cancel my subscription request'))),
        SubmitButton('submit', _('Subscribe to list %(listname)s'))
        ])
    form.AddItem(table)
    doc.AddItem(form)



def subscription_cancel(mlist, doc, cookie):
    mlist.Lock()
    try:
        # Discard this cookie
        userdesc = mlist.pend_confirm(cookie)[1]
    finally:
        mlist.Unlock()
    lang = userdesc.language
    i18n.set_language(lang)
    doc.set_language(lang)
    doc.AddItem(_('You have canceled your subscription request.'))



def subscription_confirm(mlist, doc, cookie, cgidata):
    # See the comment in admin.py about the need for the signal
    # handler.
    def sigterm_handler(signum, frame, mlist=mlist):
        mlist.Unlock()
        sys.exit(0)

    listname = mlist.real_name
    mlist.Lock()
    try:
        try:
            # Some pending values may be overridden in the form.  email of
            # course is hardcoded. ;)
            lang = cgidata.getvalue('language')
            if not Utils.IsLanguage(lang):
                lang = mlist.preferred_language
            i18n.set_language(lang)
            doc.set_language(lang)
            if cgidata.has_key('digests'):
                try:
                    digest = int(cgidata.getvalue('digests'))
                except ValueError:
                    digest = None
            else:
                digest = None
            userdesc = mlist.pend_confirm(cookie, expunge=False)[1]
            fullname = cgidata.getvalue('realname', None)
            if fullname is not None:
                fullname = Utils.canonstr(fullname, lang)
            overrides = UserDesc(fullname=fullname, digest=digest, lang=lang)
            userdesc += overrides
            op, addr, pw, digest, lang = mlist.ProcessConfirmation(
                cookie, userdesc)
        except Errors.MMNeedApproval:
            title = _('Awaiting moderator approval')
            doc.SetTitle(title)
            doc.AddItem(Header(3, Bold(FontAttr(title, size='+2'))))
            doc.AddItem(_("""\
            You have successfully confirmed your subscription request to the
            mailing list %(listname)s, however final approval is required from
            the list moderator before you will be subscribed.  Your request
            has been forwarded to the list moderator, and you will be notified
            of the moderator's decision."""))
        except Errors.NotAMemberError:
            bad_confirmation(doc, _('''Invalid confirmation string.  It is
            possible that you are attempting to confirm a request for an
            address that has already been unsubscribed.'''))
        except Errors.MMAlreadyAMember:
            doc.addError(_("You are already a member of this mailing list!"))
        except Errors.MembershipIsBanned:
            owneraddr = mlist.GetOwnerEmail()
            doc.addError(_("""You are currently banned from subscribing to
            this list.  If you think this restriction is erroneous, please
            contact the list owners at %(owneraddr)s."""))
        except Errors.HostileSubscriptionError:
            doc.addError(_("""\
            You were not invited to this mailing list.  The invitation has
            been discarded, and both list administrators have been
            alerted."""))
        else:
            # Use the user's preferred language
            i18n.set_language(lang)
            doc.set_language(lang)
            # The response
            listname = mlist.real_name
            title = _('Subscription request confirmed')
            optionsurl = mlist.GetOptionsURL(addr, absolute=1)
            doc.SetTitle(title)
            doc.AddItem(Header(3, Bold(FontAttr(title, size='+2'))))
            doc.AddItem(_('''\
            You have successfully confirmed your subscription request for
            "%(addr)s" to the %(listname)s mailing list.  A separate
            confirmation message will be sent to your email address, along
            with your password, and other useful information and links.

            <p>You can now
            <a href="%(optionsurl)s">proceed to your membership login
            page</a>.'''))
        mlist.Save()
    finally:
        mlist.Unlock()



def unsubscription_cancel(mlist, doc, cookie):
    # Expunge this record from the pending database
    expunge(mlist, cookie)
    doc.AddItem(_('You have canceled your unsubscription request.'))



def unsubscription_confirm(mlist, doc, cookie):
    # See the comment in admin.py about the need for the signal
    # handler.
    def sigterm_handler(signum, frame, mlist=mlist):
        mlist.Unlock()
        sys.exit(0)

    mlist.Lock()
    try:
        try:
            # Do this in two steps so we can get the preferred language for
            # the user who is unsubscribing.
            op, addr = mlist.pend_confirm(cookie, expunge=False)
            lang = mlist.getMemberLanguage(addr)
            i18n.set_language(lang)
            doc.set_language(lang)
            op, addr = mlist.ProcessConfirmation(cookie)
        except Errors.NotAMemberError:
            bad_confirmation(doc, _('''Invalid confirmation string.  It is
            possible that you are attempting to confirm a request for an
            address that has already been unsubscribed.'''))
        else:
            # The response
            listname = mlist.real_name
            title = _('Unsubscription request confirmed')
            listinfourl = mlist.GetScriptURL('listinfo', absolute=1)
            doc.SetTitle(title)
            doc.AddItem(Header(3, Bold(FontAttr(title, size='+2'))))
            doc.AddItem(_("""\
            You have successfully unsubscribed from the %(listname)s mailing
            list.  You can now <a href="%(listinfourl)s">visit the list's main
            information page</a>."""))
        mlist.Save()
    finally:
        mlist.Unlock()



def unsubscription_prompt(mlist, doc, cookie, addr):
    title = _('Confirm unsubscription request')
    doc.SetTitle(title)
    lang = mlist.getMemberLanguage(addr)
    i18n.set_language(lang)
    doc.set_language(lang)

    form = Form(mlist.GetScriptURL('confirm', 1))
    table = Table(border=0, width='100%')
    table.AddRow([Center(Bold(FontAttr(title, size='+1')))])
    table.AddCellInfo(table.GetCurrentRowIndex(), 0,
                      colspan=2, bgcolor=mm_cfg.WEB_HEADER_COLOR)

    listname = mlist.real_name
    fullname = mlist.getMemberName(addr)
    if fullname is None:
        fullname = _('<em>Not available</em>')
    else:
        fullname = Utils.uncanonstr(fullname, lang)
    table.AddRow([_("""Your confirmation is required in order to complete the
    unsubscription request from the mailing list <em>%(listname)s</em>.  You
    are currently subscribed with

    <ul><li><b>Real name:</b> %(fullname)s
        <li><b>Email address:</b> %(addr)s
    </ul>

    Hit the <em>Unsubscribe</em> button below to complete the confirmation
    process.

    <p>Or hit <em>Cancel and discard</em> to cancel this unsubscription
    request.""") + '<p><hr>'])
    table.AddCellInfo(table.GetCurrentRowIndex(), 0, colspan=2)
    table.AddRow([Hidden('cookie', cookie)])
    table.AddCellInfo(table.GetCurrentRowIndex(), 0, colspan=2)
    table.AddRow([SubmitButton('submit', _('Unsubscribe')),
                  SubmitButton('cancel', _('Cancel and discard'))])

    form.AddItem(table)
    doc.AddItem(form)



def addrchange_cancel(mlist, doc, cookie):
    # Expunge this record from the pending database
    expunge(mlist, cookie)
    doc.AddItem(_('You have canceled your change of address request.'))



def addrchange_confirm(mlist, doc, cookie):
    # See the comment in admin.py about the need for the signal
    # handler.
    def sigterm_handler(signum, frame, mlist=mlist):
        mlist.Unlock()
        sys.exit(0)

    mlist.Lock()
    try:
        try:
            # Do this in two steps so we can get the preferred language for
            # the user who is unsubscribing.
            op, oldaddr, newaddr, globally = mlist.pend_confirm(
                cookie, expunge=False)
            lang = mlist.getMemberLanguage(oldaddr)
            i18n.set_language(lang)
            doc.set_language(lang)
            op, oldaddr, newaddr = mlist.ProcessConfirmation(cookie)
        except Errors.NotAMemberError:
            bad_confirmation(doc, _('''Invalid confirmation string.  It is
            possible that you are attempting to confirm a request for an
            address that has already been unsubscribed.'''))
        except Errors.MembershipIsBanned:
            owneraddr = mlist.GetOwnerEmail()
            realname = mlist.real_name
            doc.addError(_("""%(newaddr)s is banned from subscribing to the
            %(realname)s list.  If you think this restriction is erroneous,
            please contact the list owners at %(owneraddr)s."""))
        except Errors.MMAlreadyAMember:
            realname = mlist.real_name
            bad_confirmation(doc, _("""%(newaddr)s is already a member of
            the %(realname)s list.  It is possible that you are attempting
            to confirm a request for an address that has already been
            subscribed."""))
        else:
            # The response
            listname = mlist.real_name
            title = _('Change of address request confirmed')
            optionsurl = mlist.GetOptionsURL(newaddr, absolute=1)
            doc.SetTitle(title)
            doc.AddItem(Header(3, Bold(FontAttr(title, size='+2'))))
            doc.AddItem(_("""\
            You have successfully changed your address on the %(listname)s
            mailing list from <b>%(oldaddr)s</b> to <b>%(newaddr)s</b>.  You
            can now <a href="%(optionsurl)s">proceed to your membership
            login page</a>."""))
        mlist.Save()
    finally:
        mlist.Unlock()



def addrchange_prompt(mlist, doc, cookie, oldaddr, newaddr, globally):
    title = _('Confirm change of address request')
    doc.SetTitle(title)
    lang = mlist.getMemberLanguage(oldaddr)
    i18n.set_language(lang)
    doc.set_language(lang)

    form = Form(mlist.GetScriptURL('confirm', 1))
    table = Table(border=0, width='100%')
    table.AddRow([Center(Bold(FontAttr(title, size='+1')))])
    table.AddCellInfo(table.GetCurrentRowIndex(), 0,
                      colspan=2, bgcolor=mm_cfg.WEB_HEADER_COLOR)

    listname = mlist.real_name
    fullname = mlist.getMemberName(oldaddr)
    if fullname is None:
        fullname = _('<em>Not available</em>')
    else:
        fullname = Utils.uncanonstr(fullname, lang)
    if globally:
        globallys = _('globally')
    else:
        globallys = ''
    table.AddRow([_("""Your confirmation is required in order to complete the
    change of address request for the mailing list <em>%(listname)s</em>.  You
    are currently subscribed with

    <ul><li><b>Real name:</b> %(fullname)s
        <li><b>Old email address:</b> %(oldaddr)s
    </ul>

    and you have requested to %(globallys)s change your email address to

    <ul><li><b>New email address:</b> %(newaddr)s
    </ul>

    Hit the <em>Change address</em> button below to complete the confirmation
    process.

    <p>Or hit <em>Cancel and discard</em> to cancel this change of address
    request.""") + '<p><hr>'])
    table.AddCellInfo(table.GetCurrentRowIndex(), 0, colspan=2)
    table.AddRow([Hidden('cookie', cookie)])
    table.AddCellInfo(table.GetCurrentRowIndex(), 0, colspan=2)
    table.AddRow([SubmitButton('submit', _('Change address')),
                  SubmitButton('cancel', _('Cancel and discard'))])

    form.AddItem(table)
    doc.AddItem(form)



def heldmsg_cancel(mlist, doc, cookie):
    title = _('Continue awaiting approval')
    doc.SetTitle(title)
    table = Table(border=0, width='100%')
    table.AddRow([Center(Bold(FontAttr(title, size='+1')))])
    table.AddCellInfo(table.GetCurrentRowIndex(), 0,
                      bgcolor=mm_cfg.WEB_HEADER_COLOR)
    # Expunge this record from the pending database.
    expunge(mlist, cookie)
    table.AddRow([_('''Okay, the list moderator will still have the
    opportunity to approve or reject this message.''')])
    doc.AddItem(table)



def heldmsg_confirm(mlist, doc, cookie):
    # See the comment in admin.py about the need for the signal
    # handler.
    def sigterm_handler(signum, frame, mlist=mlist):
        mlist.Unlock()
        sys.exit(0)

    mlist.Lock()
    try:
        try:
            # Do this in two steps so we can get the preferred language for
            # the user who posted the message.
            op, id = mlist.pend_confirm(cookie)
            ign, sender, msgsubject, ign, ign, ign = mlist.GetRecord(id)
            lang = mlist.getMemberLanguage(sender)
            subject = Utils.websafe(Utils.oneline(msgsubject,
                                                  Utils.GetCharSet(lang)))
            i18n.set_language(lang)
            doc.set_language(lang)
            # Discard the message
            mlist.HandleRequest(id, mm_cfg.DISCARD,
                                _('Sender discarded message via web.'))
        except Errors.LostHeldMessage:
            bad_confirmation(doc, _('''The held message with the Subject:
            header <em>%(subject)s</em> could not be found.  The most likely
            reason for this is that the list moderator has already approved or
            rejected the message.  You were not able to cancel it in
            time.'''))
        else:
            # The response
            listname = mlist.real_name
            title = _('Posted message canceled')
            doc.SetTitle(title)
            doc.AddItem(Header(3, Bold(FontAttr(title, size='+2'))))
            doc.AddItem(_('''\
            You have successfully canceled the posting of your message with
            the Subject: header <em>%(subject)s</em> to the mailing list
            %(listname)s.'''))
        mlist.Save()
    finally:
        mlist.Unlock()



def heldmsg_prompt(mlist, doc, cookie, id):
    title = _('Cancel held message posting')
    doc.SetTitle(title)
    form = Form(mlist.GetScriptURL('confirm', 1))
    table = Table(border=0, width='100%')
    table.AddRow([Center(Bold(FontAttr(title, size='+1')))])
    table.AddCellInfo(table.GetCurrentRowIndex(), 0,
                      colspan=2, bgcolor=mm_cfg.WEB_HEADER_COLOR)
    # Blarg.  The list must be locked in order to interact with the ListAdmin
    # database, even for read-only.  See the comment in admin.py about the
    # need for the signal handler.
    def sigterm_handler(signum, frame, mlist=mlist):
        mlist.Unlock()
        sys.exit(0)
    # Get the record, but watch for KeyErrors which mean the admin has already
    # disposed of this message.
    mlist.Lock()
    try:
        try:
            data = mlist.GetRecord(id)
        except KeyError:
            data = None
    finally:
        mlist.Unlock()

    if data is None:
        bad_confirmation(doc, _("""The held message you were referred to has
        already been handled by the list administrator."""))
        return

    # Unpack the data and present the confirmation message
    ign, sender, msgsubject, givenreason, ign, ign = data
    # Now set the language to the sender's preferred.
    lang = mlist.getMemberLanguage(sender)
    i18n.set_language(lang)
    doc.set_language(lang)

    subject = Utils.websafe(Utils.oneline(msgsubject, Utils.GetCharSet(lang)))
    reason = Utils.websafe(_(givenreason))
    listname = mlist.real_name
    table.AddRow([_('''Your confirmation is required in order to cancel the
    posting of your message to the mailing list <em>%(listname)s</em>:

    <ul><li><b>Sender:</b> %(sender)s
        <li><b>Subject:</b> %(subject)s
        <li><b>Reason:</b> %(reason)s
    </ul>

    Hit the <em>Cancel posting</em> button to discard the posting.

    <p>Or hit the <em>Continue awaiting approval</em> button to continue to
    allow the list moderator to approve or reject the message.''')
                    + '<p><hr>'])
    table.AddCellInfo(table.GetCurrentRowIndex(), 0, colspan=2)
    table.AddRow([Hidden('cookie', cookie)])
    table.AddCellInfo(table.GetCurrentRowIndex(), 0, colspan=2)
    table.AddRow([SubmitButton('submit', _('Cancel posting')),
                  SubmitButton('cancel', _('Continue awaiting approval'))])

    form.AddItem(table)
    doc.AddItem(form)



def reenable_cancel(mlist, doc, cookie):
    # Don't actually discard this cookie, since the user may decide to
    # re-enable their membership at a future time, and we may be sending out
    # future notifications with this cookie value.
    doc.AddItem(_("""You have canceled the re-enabling of your membership.  If
    we continue to receive bounces from your address, it could be deleted from
    this mailing list."""))



def reenable_confirm(mlist, doc, cookie):
    # See the comment in admin.py about the need for the signal
    # handler.
    def sigterm_handler(signum, frame, mlist=mlist):
        mlist.Unlock()
        sys.exit(0)

    mlist.Lock()
    try:
        try:
            # Do this in two steps so we can get the preferred language for
            # the user who is unsubscribing.
            op, listname, addr = mlist.pend_confirm(cookie, expunge=False)
            lang = mlist.getMemberLanguage(addr)
            i18n.set_language(lang)
            doc.set_language(lang)
            op, addr = mlist.ProcessConfirmation(cookie)
        except Errors.NotAMemberError:
            bad_confirmation(doc, _('''Invalid confirmation string.  It is
            possible that you are attempting to confirm a request for an
            address that has already been unsubscribed.'''))
        else:
            # The response
            listname = mlist.real_name
            title = _('Membership re-enabled.')
            optionsurl = mlist.GetOptionsURL(addr, absolute=1)
            doc.SetTitle(title)
            doc.AddItem(Header(3, Bold(FontAttr(title, size='+2'))))
            doc.AddItem(_("""\
            You have successfully re-enabled your membership in the
            %(listname)s mailing list.  You can now <a
            href="%(optionsurl)s">visit your member options page</a>.
            """))
        mlist.Save()
    finally:
        mlist.Unlock()



def reenable_prompt(mlist, doc, cookie, list, member):
    title = _('Re-enable mailing list membership')
    doc.SetTitle(title)
    form = Form(mlist.GetScriptURL('confirm', 1))
    table = Table(border=0, width='100%')
    table.AddRow([Center(Bold(FontAttr(title, size='+1')))])
    table.AddCellInfo(table.GetCurrentRowIndex(), 0,
                      colspan=2, bgcolor=mm_cfg.WEB_HEADER_COLOR)

    lang = mlist.getMemberLanguage(member)
    i18n.set_language(lang)
    doc.set_language(lang)

    realname = mlist.real_name
    info = mlist.getBounceInfo(member)
    if not info:
        listinfourl = mlist.GetScriptURL('listinfo', absolute=1)
        # They've already be unsubscribed
        table.AddRow([_("""We're sorry, but you have already been unsubscribed
        from this mailing list.  To re-subscribe, please visit the
        <a href="%(listinfourl)s">list information page</a>.""")])
        return

    date = time.strftime('%A, %B %d, %Y',
                         time.localtime(time.mktime(info.date + (0,)*6)))
    daysleft = int(info.noticesleft *
                   mlist.bounce_you_are_disabled_warnings_interval /
                   mm_cfg.days(1))
    # BAW: for consistency this should be changed to 'fullname' or the above
    # 'fullname's should be changed to 'username'.  Don't want to muck with
    # the i18n catalogs though.
    username = mlist.getMemberName(member)
    if username is None:
        username = _('<em>not available</em>')
    else:
        username = Utils.uncanonstr(username, lang)

    table.AddRow([_("""Your membership in the %(realname)s mailing list is
    currently disabled due to excessive bounces.  Your confirmation is
    required in order to re-enable delivery to your address.  We have the
    following information on file:

    <ul><li><b>Member address:</b> %(member)s
        <li><b>Member name:</b> %(username)s
        <li><b>Last bounce received on:</b> %(date)s
        <li><b>Approximate number of days before you are permanently removed
               from this list:</b> %(daysleft)s
    </ul>

    Hit the <em>Re-enable membership</em> button to resume receiving postings
    from the mailing list.  Or hit the <em>Cancel</em> button to defer
    re-enabling your membership.
    """)])

    table.AddCellInfo(table.GetCurrentRowIndex(), 0, colspan=2)
    table.AddRow([Hidden('cookie', cookie)])
    table.AddCellInfo(table.GetCurrentRowIndex(), 0, colspan=2)
    table.AddRow([SubmitButton('submit', _('Re-enable membership')),
                  SubmitButton('cancel', _('Cancel'))])

    form.AddItem(table)
    doc.AddItem(form)
