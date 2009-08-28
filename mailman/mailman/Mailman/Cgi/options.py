# Copyright (C) 1998-2008 by the Free Software Foundation, Inc.
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

"""Produce and handle the member options."""

import sys
import os
import cgi
import signal
import urllib
from types import ListType

from Mailman import mm_cfg
from Mailman import Utils
from Mailman import MailList
from Mailman import Errors
from Mailman import MemberAdaptor
from Mailman import i18n
from Mailman.htmlformat import *
from Mailman.Logging.Syslog import syslog

SLASH = '/'
SETLANGUAGE = -1

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
    lenparts = parts and len(parts)
    if not parts or lenparts < 1:
        title = _('CGI script error')
        doc.SetTitle(title)
        doc.AddItem(Header(2, title))
        doc.addError(_('Invalid options to CGI script.'))
        doc.AddItem('<hr>')
        doc.AddItem(MailmanLogo())
        print doc.Format()
        return

    # get the list and user's name
    listname = parts[0].lower()
    # open list
    try:
        mlist = MailList.MailList(listname, lock=0)
    except Errors.MMListError, e:
        # Avoid cross-site scripting attacks
        safelistname = Utils.websafe(listname)
        title = _('CGI script error')
        doc.SetTitle(title)
        doc.AddItem(Header(2, title))
        doc.addError(_('No such list <em>%(safelistname)s</em>'))
        doc.AddItem('<hr>')
        doc.AddItem(MailmanLogo())
        print doc.Format()
        syslog('error', 'No such list "%s": %s\n', listname, e)
        return

    # The total contents of the user's response
    cgidata = cgi.FieldStorage(keep_blank_values=1)

    # Set the language for the page.  If we're coming from the listinfo cgi,
    # we might have a 'language' key in the cgi data.  That was an explicit
    # preference to view the page in, so we should honor that here.  If that's
    # not available, use the list's default language.
    language = cgidata.getvalue('language')
    if not Utils.IsLanguage(language):
        language = mlist.preferred_language
    i18n.set_language(language)
    doc.set_language(language)

    if lenparts < 2:
        user = cgidata.getvalue('email')
        if not user:
            # If we're coming from the listinfo page and we left the email
            # address field blank, it's not an error.  Likewise if we're
            # coming from anywhere else. Only issue the error if we came
            # via one of our buttons.
            if (cgidata.getvalue('login') or cgidata.getvalue('login-unsub')
                    or cgidata.getvalue('login-remind')):
                doc.addError(_('No address given'))
            loginpage(mlist, doc, None, language)
            print doc.Format()
            return
    else:
        user = Utils.LCDomain(Utils.UnobscureEmail(SLASH.join(parts[1:])))

    # Avoid cross-site scripting attacks
    safeuser = Utils.websafe(user)
    try:
        Utils.ValidateEmail(user)
    except Errors.EmailAddressError:
        doc.addError(_('Illegal Email Address: %(safeuser)s'))
        loginpage(mlist, doc, None, language)
        print doc.Format()
        return
    # Sanity check the user, but only give the "no such member" error when
    # using public rosters, otherwise, we'll leak membership information.
    if not mlist.isMember(user) and mlist.private_roster == 0:
        doc.addError(_('No such member: %(safeuser)s.'))
        loginpage(mlist, doc, None, language)
        print doc.Format()
        return

    # Find the case preserved email address (the one the user subscribed with)
    lcuser = user.lower()
    try:
        cpuser = mlist.getMemberCPAddress(lcuser)
    except Errors.NotAMemberError:
        # This happens if the user isn't a member but we've got private rosters
        cpuser = None
    if lcuser == cpuser:
        cpuser = None

    # And now we know the user making the request, so set things up to for the
    # user's stored preferred language, overridden by any form settings for
    # their new language preference.
    userlang = cgidata.getvalue('language')
    if not Utils.IsLanguage(userlang):
        userlang = mlist.getMemberLanguage(user)
    doc.set_language(userlang)
    i18n.set_language(userlang)

    # See if this is VARHELP on topics.
    varhelp = None
    if cgidata.has_key('VARHELP'):
        varhelp = cgidata['VARHELP'].value
    elif os.environ.get('QUERY_STRING'):
        # POST methods, even if their actions have a query string, don't get
        # put into FieldStorage's keys :-(
        qs = cgi.parse_qs(os.environ['QUERY_STRING']).get('VARHELP')
        if qs and type(qs) == types.ListType:
            varhelp = qs[0]
    if varhelp:
        topic_details(mlist, doc, user, cpuser, userlang, varhelp)
        return

    # Are we processing an unsubscription request from the login screen?
    if cgidata.has_key('login-unsub'):
        # Because they can't supply a password for unsubscribing, we'll need
        # to do the confirmation dance.
        if mlist.isMember(user):
            # We must acquire the list lock in order to pend a request.
            try:
                mlist.Lock()
                # If unsubs require admin approval, then this request has to
                # be held.  Otherwise, send a confirmation.
                if mlist.unsubscribe_policy:
                    mlist.HoldUnsubscription(user)
                    doc.addError(_("""Your unsubscription request has been
                    forwarded to the list administrator for approval."""),
                                 tag='')
                else:
                    mlist.ConfirmUnsubscription(user, userlang)
                    doc.addError(_('The confirmation email has been sent.'),
                                 tag='')
                mlist.Save()
            finally:
                mlist.Unlock()
        else:
            # Not a member
            if mlist.private_roster == 0:
                # Public rosters
                doc.addError(_('No such member: %(safeuser)s.'))
            else:
                syslog('mischief',
                       'Unsub attempt of non-member w/ private rosters: %s',
                       user)
                doc.addError(_('The confirmation email has been sent.'),
                             tag='')
        loginpage(mlist, doc, user, language)
        print doc.Format()
        return

    # Are we processing a password reminder from the login screen?
    if cgidata.has_key('login-remind'):
        if mlist.isMember(user):
            mlist.MailUserPassword(user)
            doc.addError(
                _('A reminder of your password has been emailed to you.'),
                tag='')
        else:
            # Not a member
            if mlist.private_roster == 0:
                # Public rosters
                doc.addError(_('No such member: %(safeuser)s.'))
            else:
                syslog('mischief',
                       'Reminder attempt of non-member w/ private rosters: %s',
                       user)
                doc.addError(
                    _('A reminder of your password has been emailed to you.'),
                    tag='')
        loginpage(mlist, doc, user, language)
        print doc.Format()
        return

    # Get the password from the form.
    password = cgidata.getvalue('password', '').strip()
    # Check authentication.  We need to know if the credentials match the user
    # or the site admin, because they are the only ones who are allowed to
    # change things globally.  Specifically, the list admin may not change
    # values globally.
    if mm_cfg.ALLOW_SITE_ADMIN_COOKIES:
        user_or_siteadmin_context = (mm_cfg.AuthUser, mm_cfg.AuthSiteAdmin)
    else:
        # Site and list admins are treated equal so that list admin can pass
        # site admin test. :-(
        user_or_siteadmin_context = (mm_cfg.AuthUser,)
    is_user_or_siteadmin = mlist.WebAuthenticate(
        user_or_siteadmin_context, password, user)
    # Authenticate, possibly using the password supplied in the login page
    if not is_user_or_siteadmin and \
       not mlist.WebAuthenticate((mm_cfg.AuthListAdmin,
                                  mm_cfg.AuthSiteAdmin),
                                 password, user):
        # Not authenticated, so throw up the login page again.  If they tried
        # to authenticate via cgi (instead of cookie), then print an error
        # message.
        if cgidata.has_key('password'):
            doc.addError(_('Authentication failed.'))
            # So as not to allow membership leakage, prompt for the email
            # address and the password here.
            if mlist.private_roster <> 0:
                syslog('mischief',
                       'Login failure with private rosters: %s',
                       user)
                user = None
        loginpage(mlist, doc, user, language)
        print doc.Format()
        return

    # From here on out, the user is okay to view and modify their membership
    # options.  The first set of checks does not require the list to be
    # locked.

    if cgidata.has_key('logout'):
        print mlist.ZapCookie(mm_cfg.AuthUser, user)
        loginpage(mlist, doc, user, language)
        print doc.Format()
        return

    if cgidata.has_key('emailpw'):
        mlist.MailUserPassword(user)
        options_page(
            mlist, doc, user, cpuser, userlang,
            _('A reminder of your password has been emailed to you.'))
        print doc.Format()
        return

    if cgidata.has_key('othersubs'):
        # Only the user or site administrator can view all subscriptions.
        if not is_user_or_siteadmin:
            doc.addError(_("""The list administrator may not view the other
            subscriptions for this user."""), _('Note: '))
            options_page(mlist, doc, user, cpuser, userlang)
            print doc.Format()
            return
        hostname = mlist.host_name
        title = _('List subscriptions for %(safeuser)s on %(hostname)s')
        doc.SetTitle(title)
        doc.AddItem(Header(2, title))
        doc.AddItem(_('''Click on a link to visit your options page for the
        requested mailing list.'''))

        # Troll through all the mailing lists that match host_name and see if
        # the user is a member.  If so, add it to the list.
        onlists = []
        for gmlist in lists_of_member(mlist, user) + [mlist]:
            url = gmlist.GetOptionsURL(user)
            link = Link(url, gmlist.real_name)
            onlists.append((gmlist.real_name, link))
        onlists.sort()
        items = OrderedList(*[link for name, link in onlists])
        doc.AddItem(items)
        print doc.Format()
        return

    if cgidata.has_key('change-of-address'):
        # We could be changing the user's full name, email address, or both.
        # Watch out for non-ASCII characters in the member's name.
        membername = cgidata.getvalue('fullname')
        # Canonicalize the member's name
        membername = Utils.canonstr(membername, language)
        newaddr = cgidata.getvalue('new-address')
        confirmaddr = cgidata.getvalue('confirm-address')

        oldname = mlist.getMemberName(user)
        set_address = set_membername = 0

        # See if the user wants to change their email address globally.  The
        # list admin is /not/ allowed to make global changes.
        globally = cgidata.getvalue('changeaddr-globally')
        if globally and not is_user_or_siteadmin:
            doc.addError(_("""The list administrator may not change the names
            or addresses for this user's other subscriptions.  However, the
            subscription for this mailing list has been changed."""),
                         _('Note: '))
            globally = False
        # We will change the member's name under the following conditions:
        # - membername has a value
        # - membername has no value, but they /used/ to have a membername
        if membername and membername <> oldname:
            # Setting it to a new value
            set_membername = 1
        if not membername and oldname:
            # Unsetting it
            set_membername = 1
        # We will change the user's address if both newaddr and confirmaddr
        # are non-blank, have the same value, and aren't the currently
        # subscribed email address (when compared case-sensitively).  If both
        # are blank, but membername is set, we ignore it, otherwise we print
        # an error.
        msg = ''
        if newaddr and confirmaddr:
            if newaddr <> confirmaddr:
                options_page(mlist, doc, user, cpuser, userlang,
                             _('Addresses did not match!'))
                print doc.Format()
                return
            if newaddr == cpuser:
                options_page(mlist, doc, user, cpuser, userlang,
                             _('You are already using that email address'))
                print doc.Format()
                return
            # If they're requesting to subscribe an address which is already a
            # member, and they're /not/ doing it globally, then refuse.
            # Otherwise, we'll agree to do it globally (with a warning
            # message) and let ApprovedChangeMemberAddress() handle already a
            # member issues.
            if mlist.isMember(newaddr):
                safenewaddr = Utils.websafe(newaddr)
                if globally:
                    listname = mlist.real_name
                    msg += _("""\
The new address you requested %(newaddr)s is already a member of the
%(listname)s mailing list, however you have also requested a global change of
address.  Upon confirmation, any other mailing list containing the address
%(safeuser)s will be changed. """)
                    # Don't return
                else:
                    options_page(
                        mlist, doc, user, cpuser, userlang,
                        _('The new address is already a member: %(newaddr)s'))
                    print doc.Format()
                    return
            set_address = 1
        elif (newaddr or confirmaddr) and not set_membername:
            options_page(mlist, doc, user, cpuser, userlang,
                         _('Addresses may not be blank'))
            print doc.Format()
            return

        # Standard sigterm handler.
        def sigterm_handler(signum, frame, mlist=mlist):
            mlist.Unlock()
            sys.exit(0)

        signal.signal(signal.SIGTERM, sigterm_handler)
        if set_address:
            if cpuser is None:
                cpuser = user
            # Register the pending change after the list is locked
            msg += _('A confirmation message has been sent to %(newaddr)s. ')
            mlist.Lock()
            try:
                try:
                    mlist.ChangeMemberAddress(cpuser, newaddr, globally)
                    mlist.Save()
                finally:
                    mlist.Unlock()
            except Errors.MMBadEmailError:
                msg = _('Bad email address provided')
            except Errors.MMHostileAddress:
                msg = _('Illegal email address provided')
            except Errors.MMAlreadyAMember:
                msg = _('%(newaddr)s is already a member of the list.')
            except Errors.MembershipIsBanned:
                owneraddr = mlist.GetOwnerEmail()
                msg = _("""%(newaddr)s is banned from this list.  If you
                      think this restriction is erroneous, please contact
                      the list owners at %(owneraddr)s.""")

        if set_membername:
            mlist.Lock()
            try:
                mlist.ChangeMemberName(user, membername, globally)
                mlist.Save()
            finally:
                mlist.Unlock()
            msg += _('Member name successfully changed. ')

        options_page(mlist, doc, user, cpuser, userlang, msg)
        print doc.Format()
        return

    if cgidata.has_key('changepw'):
        # Is this list admin and is list admin allowed to change passwords.
        if not (is_user_or_siteadmin
                or mm_cfg.OWNERS_CAN_CHANGE_MEMBER_PASSWORDS):
            doc.addError(_("""The list administrator may not change the
                    password for a user."""))
            options_page(mlist, doc, user, cpuser, userlang)
            print doc.Format()
            return
        newpw = cgidata.getvalue('newpw')
        confirmpw = cgidata.getvalue('confpw')
        if not newpw or not confirmpw:
            options_page(mlist, doc, user, cpuser, userlang,
                         _('Passwords may not be blank'))
            print doc.Format()
            return
        if newpw <> confirmpw:
            options_page(mlist, doc, user, cpuser, userlang,
                         _('Passwords did not match!'))
            print doc.Format()
            return

        # See if the user wants to change their passwords globally, however
        # the list admin is /not/ allowed to change passwords globally.
        pw_globally = cgidata.getvalue('pw-globally')
        if pw_globally and not is_user_or_siteadmin:
            doc.addError(_("""The list administrator may not change the
            password for this user's other subscriptions.  However, the
            password for this mailing list has been changed."""),
                         _('Note: '))
            pw_globally = False

        mlists = [mlist]

        if pw_globally:
            mlists.extend(lists_of_member(mlist, user))

        for gmlist in mlists:
            change_password(gmlist, user, newpw, confirmpw)

        # Regenerate the cookie so a re-authorization isn't necessary
        print mlist.MakeCookie(mm_cfg.AuthUser, user)
        options_page(mlist, doc, user, cpuser, userlang,
                     _('Password successfully changed.'))
        print doc.Format()
        return

    if cgidata.has_key('unsub'):
        # Was the confirming check box turned on?
        if not cgidata.getvalue('unsubconfirm'):
            options_page(
                mlist, doc, user, cpuser, userlang,
                _('''You must confirm your unsubscription request by turning
                on the checkbox below the <em>Unsubscribe</em> button.  You
                have not been unsubscribed!'''))
            print doc.Format()
            return

        # Standard signal handler
        def sigterm_handler(signum, frame, mlist=mlist):
            mlist.Unlock()
            sys.exit(0)

        # Okay, zap them.  Leave them sitting at the list's listinfo page.  We
        # must own the list lock, and we want to make sure the user (BAW: and
        # list admin?) is informed of the removal.
        signal.signal(signal.SIGTERM, sigterm_handler)
        mlist.Lock()
        needapproval = False
        try:
            try:
                mlist.DeleteMember(
                    user, 'via the member options page', userack=1)
            except Errors.MMNeedApproval:
                needapproval = True
            mlist.Save()
        finally:
            mlist.Unlock()
        # Now throw up some results page, with appropriate links.  We can't
        # drop them back into their options page, because that's gone now!
        fqdn_listname = mlist.GetListEmail()
        owneraddr = mlist.GetOwnerEmail()
        url = mlist.GetScriptURL('listinfo', absolute=1)

        title = _('Unsubscription results')
        doc.SetTitle(title)
        doc.AddItem(Header(2, title))
        if needapproval:
            doc.AddItem(_("""Your unsubscription request has been received and
            forwarded on to the list moderators for approval.  You will
            receive notification once the list moderators have made their
            decision."""))
        else:
            doc.AddItem(_("""You have been successfully unsubscribed from the
            mailing list %(fqdn_listname)s.  If you were receiving digest
            deliveries you may get one more digest.  If you have any questions
            about your unsubscription, please contact the list owners at
            %(owneraddr)s."""))
        doc.AddItem(mlist.GetMailmanFooter())
        print doc.Format()
        return

    if cgidata.has_key('options-submit'):
        # Digest action flags
        digestwarn = 0
        cantdigest = 0
        mustdigest = 0

        newvals = []
        # First figure out which options have changed.  The item names come
        # from FormatOptionButton() in HTMLFormatter.py
        for item, flag in (('digest',      mm_cfg.Digests),
                           ('mime',        mm_cfg.DisableMime),
                           ('dontreceive', mm_cfg.DontReceiveOwnPosts),
                           ('ackposts',    mm_cfg.AcknowledgePosts),
                           ('disablemail', mm_cfg.DisableDelivery),
                           ('conceal',     mm_cfg.ConcealSubscription),
                           ('remind',      mm_cfg.SuppressPasswordReminder),
                           ('rcvtopic',    mm_cfg.ReceiveNonmatchingTopics),
                           ('nodupes',     mm_cfg.DontReceiveDuplicates),
                           ):
            try:
                newval = int(cgidata.getvalue(item))
            except (TypeError, ValueError):
                newval = None

            # Skip this option if there was a problem or it wasn't changed.
            # Note that delivery status is handled separate from the options
            # flags.
            if newval is None:
                continue
            elif flag == mm_cfg.DisableDelivery:
                status = mlist.getDeliveryStatus(user)
                # Here, newval == 0 means enable, newval == 1 means disable
                if not newval and status <> MemberAdaptor.ENABLED:
                    newval = MemberAdaptor.ENABLED
                elif newval and status == MemberAdaptor.ENABLED:
                    newval = MemberAdaptor.BYUSER
                else:
                    continue
            elif newval == mlist.getMemberOption(user, flag):
                continue
            # Should we warn about one more digest?
            if flag == mm_cfg.Digests and \
                   newval == 0 and mlist.getMemberOption(user, flag):
                digestwarn = 1

            newvals.append((flag, newval))

        # The user language is handled a little differently
        if userlang not in mlist.GetAvailableLanguages():
            newvals.append((SETLANGUAGE, mlist.preferred_language))
        else:
            newvals.append((SETLANGUAGE, userlang))

        # Process user selected topics, but don't make the changes to the
        # MailList object; we must do that down below when the list is
        # locked.
        topicnames = cgidata.getvalue('usertopic')
        if topicnames:
            # Some topics were selected.  topicnames can actually be a string
            # or a list of strings depending on whether more than one topic
            # was selected or not.
            if not isinstance(topicnames, ListType):
                # Assume it was a bare string, so listify it
                topicnames = [topicnames]
            # unquote the topic names
            topicnames = [urllib.unquote_plus(n) for n in topicnames]

        # The standard sigterm handler (see above)
        def sigterm_handler(signum, frame, mlist=mlist):
            mlist.Unlock()
            sys.exit(0)

        # Now, lock the list and perform the changes
        mlist.Lock()
        try:
            signal.signal(signal.SIGTERM, sigterm_handler)
            # `values' is a tuple of flags and the web values
            for flag, newval in newvals:
                # Handle language settings differently
                if flag == SETLANGUAGE:
                    mlist.setMemberLanguage(user, newval)
                # Handle delivery status separately
                elif flag == mm_cfg.DisableDelivery:
                    mlist.setDeliveryStatus(user, newval)
                else:
                    try:
                        mlist.setMemberOption(user, flag, newval)
                    except Errors.CantDigestError:
                        cantdigest = 1
                    except Errors.MustDigestError:
                        mustdigest = 1
            # Set the topics information.
            mlist.setMemberTopics(user, topicnames)
            mlist.Save()
        finally:
            mlist.Unlock()

        # A bag of attributes for the global options
        class Global:
            enable = None
            remind = None
            nodupes = None
            mime = None
            def __nonzero__(self):
                 return len(self.__dict__.keys()) > 0

        globalopts = Global()

        # The enable/disable option and the password remind option may have
        # their global flags sets.
        if cgidata.getvalue('deliver-globally'):
            # Yes, this is inefficient, but the list is so small it shouldn't
            # make much of a difference.
            for flag, newval in newvals:
                if flag == mm_cfg.DisableDelivery:
                    globalopts.enable = newval
                    break

        if cgidata.getvalue('remind-globally'):
            for flag, newval in newvals:
                if flag == mm_cfg.SuppressPasswordReminder:
                    globalopts.remind = newval
                    break

        if cgidata.getvalue('nodupes-globally'):
            for flag, newval in newvals:
                if flag == mm_cfg.DontReceiveDuplicates:
                    globalopts.nodupes = newval
                    break

        if cgidata.getvalue('mime-globally'):
            for flag, newval in newvals:
                if flag == mm_cfg.DisableMime:
                    globalopts.mime = newval
                    break

        # Change options globally, but only if this is the user or site admin,
        # /not/ if this is the list admin.
        if globalopts:
            if not is_user_or_siteadmin:
                doc.addError(_("""The list administrator may not change the
                options for this user's other subscriptions.  However the
                options for this mailing list subscription has been
                changed."""), _('Note: '))
            else:
                for gmlist in lists_of_member(mlist, user):
                    global_options(gmlist, user, globalopts)

        # Now print the results
        if cantdigest:
            msg = _('''The list administrator has disabled digest delivery for
            this list, so your delivery option has not been set.  However your
            other options have been set successfully.''')
        elif mustdigest:
            msg = _('''The list administrator has disabled non-digest delivery
            for this list, so your delivery option has not been set.  However
            your other options have been set successfully.''')
        else:
            msg = _('You have successfully set your options.')

        if digestwarn:
            msg += _('You may get one last digest.')

        options_page(mlist, doc, user, cpuser, userlang, msg)
        print doc.Format()
        return

    if mlist.isMember(user):
        options_page(mlist, doc, user, cpuser, userlang)
    else:
        loginpage(mlist, doc, user, userlang)
    print doc.Format()



def options_page(mlist, doc, user, cpuser, userlang, message=''):
    # The bulk of the document will come from the options.html template, which
    # includes it's own html armor (head tags, etc.).  Suppress the head that
    # Document() derived pages get automatically.
    doc.suppress_head = 1

    if mlist.obscure_addresses:
        presentable_user = Utils.ObscureEmail(user, for_text=1)
        if cpuser is not None:
            cpuser = Utils.ObscureEmail(cpuser, for_text=1)
    else:
        presentable_user = user

    fullname = Utils.uncanonstr(mlist.getMemberName(user), userlang)
    if fullname:
        presentable_user += ', %s' % Utils.websafe(fullname)

    # Do replacements
    replacements = mlist.GetStandardReplacements(userlang)
    replacements['<mm-results>'] = Bold(FontSize('+1', message)).Format()
    replacements['<mm-digest-radio-button>'] = mlist.FormatOptionButton(
        mm_cfg.Digests, 1, user)
    replacements['<mm-undigest-radio-button>'] = mlist.FormatOptionButton(
        mm_cfg.Digests, 0, user)
    replacements['<mm-plain-digests-button>'] = mlist.FormatOptionButton(
        mm_cfg.DisableMime, 1, user)
    replacements['<mm-mime-digests-button>'] = mlist.FormatOptionButton(
        mm_cfg.DisableMime, 0, user)
    replacements['<mm-global-mime-button>'] = (
        CheckBox('mime-globally', 1, checked=0).Format())
    replacements['<mm-delivery-enable-button>'] = mlist.FormatOptionButton(
        mm_cfg.DisableDelivery, 0, user)
    replacements['<mm-delivery-disable-button>'] = mlist.FormatOptionButton(
        mm_cfg.DisableDelivery, 1, user)
    replacements['<mm-disabled-notice>'] = mlist.FormatDisabledNotice(user)
    replacements['<mm-dont-ack-posts-button>'] = mlist.FormatOptionButton(
        mm_cfg.AcknowledgePosts, 0, user)
    replacements['<mm-ack-posts-button>'] = mlist.FormatOptionButton(
        mm_cfg.AcknowledgePosts, 1, user)
    replacements['<mm-receive-own-mail-button>'] = mlist.FormatOptionButton(
        mm_cfg.DontReceiveOwnPosts, 0, user)
    replacements['<mm-dont-receive-own-mail-button>'] = (
        mlist.FormatOptionButton(mm_cfg.DontReceiveOwnPosts, 1, user))
    replacements['<mm-dont-get-password-reminder-button>'] = (
        mlist.FormatOptionButton(mm_cfg.SuppressPasswordReminder, 1, user))
    replacements['<mm-get-password-reminder-button>'] = (
        mlist.FormatOptionButton(mm_cfg.SuppressPasswordReminder, 0, user))
    replacements['<mm-public-subscription-button>'] = (
        mlist.FormatOptionButton(mm_cfg.ConcealSubscription, 0, user))
    replacements['<mm-hide-subscription-button>'] = mlist.FormatOptionButton(
        mm_cfg.ConcealSubscription, 1, user)
    replacements['<mm-dont-receive-duplicates-button>'] = (
        mlist.FormatOptionButton(mm_cfg.DontReceiveDuplicates, 1, user))
    replacements['<mm-receive-duplicates-button>'] = (
        mlist.FormatOptionButton(mm_cfg.DontReceiveDuplicates, 0, user))
    replacements['<mm-unsubscribe-button>'] = (
        mlist.FormatButton('unsub', _('Unsubscribe')) + '<br>' +
        CheckBox('unsubconfirm', 1, checked=0).Format() +
        _('<em>Yes, I really want to unsubscribe</em>'))
    replacements['<mm-new-pass-box>'] = mlist.FormatSecureBox('newpw')
    replacements['<mm-confirm-pass-box>'] = mlist.FormatSecureBox('confpw')
    replacements['<mm-change-pass-button>'] = (
        mlist.FormatButton('changepw', _("Change My Password")))
    replacements['<mm-other-subscriptions-submit>'] = (
        mlist.FormatButton('othersubs',
                           _('List my other subscriptions')))
    replacements['<mm-form-start>'] = (
        mlist.FormatFormStart('options', user))
    replacements['<mm-user>'] = user
    replacements['<mm-presentable-user>'] = presentable_user
    replacements['<mm-email-my-pw>'] = mlist.FormatButton(
        'emailpw', (_('Email My Password To Me')))
    replacements['<mm-umbrella-notice>'] = (
        mlist.FormatUmbrellaNotice(user, _("password")))
    replacements['<mm-logout-button>'] = (
        mlist.FormatButton('logout', _('Log out')))
    replacements['<mm-options-submit-button>'] = mlist.FormatButton(
        'options-submit', _('Submit My Changes'))
    replacements['<mm-global-pw-changes-button>'] = (
        CheckBox('pw-globally', 1, checked=0).Format())
    replacements['<mm-global-deliver-button>'] = (
        CheckBox('deliver-globally', 1, checked=0).Format())
    replacements['<mm-global-remind-button>'] = (
        CheckBox('remind-globally', 1, checked=0).Format())
    replacements['<mm-global-nodupes-button>'] = (
        CheckBox('nodupes-globally', 1, checked=0).Format())

    days = int(mm_cfg.PENDING_REQUEST_LIFE / mm_cfg.days(1))
    if days > 1:
        units = _('days')
    else:
        units = _('day')
    replacements['<mm-pending-days>'] = _('%(days)d %(units)s')

    replacements['<mm-new-address-box>'] = mlist.FormatBox('new-address')
    replacements['<mm-confirm-address-box>'] = mlist.FormatBox(
        'confirm-address')
    replacements['<mm-change-address-button>'] = mlist.FormatButton(
        'change-of-address', _('Change My Address and Name'))
    replacements['<mm-global-change-of-address>'] = CheckBox(
        'changeaddr-globally', 1, checked=0).Format()
    replacements['<mm-fullname-box>'] = mlist.FormatBox(
        'fullname', value=fullname)

    # Create the topics radios.  BAW: what if the list admin deletes a topic,
    # but the user still wants to get that topic message?
    usertopics = mlist.getMemberTopics(user)
    if mlist.topics:
        table = Table(border="0")
        for name, pattern, description, emptyflag in mlist.topics:
            if emptyflag:
                continue
            quotedname = urllib.quote_plus(name)
            details = Link(mlist.GetScriptURL('options') +
                           '/%s/?VARHELP=%s' % (user, quotedname),
                           ' (Details)')
            if name in usertopics:
                checked = 1
            else:
                checked = 0
            table.AddRow([CheckBox('usertopic', quotedname, checked=checked),
                          name + details.Format()])
        topicsfield = table.Format()
    else:
        topicsfield = _('<em>No topics defined</em>')
    replacements['<mm-topics>'] = topicsfield
    replacements['<mm-suppress-nonmatching-topics>'] = (
        mlist.FormatOptionButton(mm_cfg.ReceiveNonmatchingTopics, 0, user))
    replacements['<mm-receive-nonmatching-topics>'] = (
        mlist.FormatOptionButton(mm_cfg.ReceiveNonmatchingTopics, 1, user))

    if cpuser is not None:
        replacements['<mm-case-preserved-user>'] = _('''
You are subscribed to this list with the case-preserved address
<em>%(cpuser)s</em>.''')
    else:
        replacements['<mm-case-preserved-user>'] = ''

    doc.AddItem(mlist.ParseTags('options.html', replacements, userlang))



def loginpage(mlist, doc, user, lang):
    realname = mlist.real_name
    actionurl = mlist.GetScriptURL('options')
    if user is None:
        title = _('%(realname)s list: member options login page')
        extra = _('email address and ')
    else:
        safeuser = Utils.websafe(user)
        title = _('%(realname)s list: member options for user %(safeuser)s')
        obuser = Utils.ObscureEmail(user)
        extra = ''
    # Set up the title
    doc.SetTitle(title)
    # We use a subtable here so we can put a language selection box in
    table = Table(width='100%', border=0, cellspacing=4, cellpadding=5)
    # If only one language is enabled for this mailing list, omit the choice
    # buttons.
    table.AddRow([Center(Header(2, title))])
    table.AddCellInfo(table.GetCurrentRowIndex(), 0,
                      bgcolor=mm_cfg.WEB_HEADER_COLOR)
    if len(mlist.GetAvailableLanguages()) > 1:
        langform = Form(actionurl)
        langform.AddItem(SubmitButton('displang-button',
                                      _('View this page in')))
        langform.AddItem(mlist.GetLangSelectBox(lang))
        if user:
            langform.AddItem(Hidden('email', user))
        table.AddRow([Center(langform)])
    doc.AddItem(table)
    # Preamble
    # Set up the login page
    form = Form(actionurl)
    form.AddItem(Hidden('language', lang))
    table = Table(width='100%', border=0, cellspacing=4, cellpadding=5)
    table.AddRow([_("""In order to change your membership option, you must
    first log in by giving your %(extra)smembership password in the section
    below.  If you don't remember your membership password, you can have it
    emailed to you by clicking on the button below.  If you just want to
    unsubscribe from this list, click on the <em>Unsubscribe</em> button and a
    confirmation message will be sent to you.

    <p><strong><em>Important:</em></strong> From this point on, you must have
    cookies enabled in your browser, otherwise none of your changes will take
    effect.
    """)])
    # Password and login button
    ptable = Table(width='50%', border=0, cellspacing=4, cellpadding=5)
    if user is None:
        ptable.AddRow([Label(_('Email address:')),
                       TextBox('email', size=20)])
    else:
        ptable.AddRow([Hidden('email', user)])
    ptable.AddRow([Label(_('Password:')),
                   PasswordBox('password', size=20)])
    ptable.AddRow([Center(SubmitButton('login', _('Log in')))])
    ptable.AddCellInfo(ptable.GetCurrentRowIndex(), 0, colspan=2)
    table.AddRow([Center(ptable)])
    # Unsubscribe section
    table.AddRow([Center(Header(2, _('Unsubscribe')))])
    table.AddCellInfo(table.GetCurrentRowIndex(), 0,
                      bgcolor=mm_cfg.WEB_HEADER_COLOR)

    table.AddRow([_("""By clicking on the <em>Unsubscribe</em> button, a
    confirmation message will be emailed to you.  This message will have a
    link that you should click on to complete the removal process (you can
    also confirm by email; see the instructions in the confirmation
    message).""")])

    table.AddRow([Center(SubmitButton('login-unsub', _('Unsubscribe')))])
    # Password reminder section
    table.AddRow([Center(Header(2, _('Password reminder')))])
    table.AddCellInfo(table.GetCurrentRowIndex(), 0,
                      bgcolor=mm_cfg.WEB_HEADER_COLOR)

    table.AddRow([_("""By clicking on the <em>Remind</em> button, your
    password will be emailed to you.""")])

    table.AddRow([Center(SubmitButton('login-remind', _('Remind')))])
    # Finish up glomming together the login page
    form.AddItem(table)
    doc.AddItem(form)
    doc.AddItem(mlist.GetMailmanFooter())



def lists_of_member(mlist, user):
    hostname = mlist.host_name
    onlists = []
    for listname in Utils.list_names():
        # The current list will always handle things in the mainline
        if listname == mlist.internal_name():
            continue
        glist = MailList.MailList(listname, lock=0)
        if glist.host_name <> hostname:
            continue
        if not glist.isMember(user):
            continue
        onlists.append(glist)
    return onlists



def change_password(mlist, user, newpw, confirmpw):
    # This operation requires the list lock, so let's set up the signal
    # handling so the list lock will get released when the user hits the
    # browser stop button.
    def sigterm_handler(signum, frame, mlist=mlist):
        # Make sure the list gets unlocked...
        mlist.Unlock()
        # ...and ensure we exit, otherwise race conditions could cause us to
        # enter MailList.Save() while we're in the unlocked state, and that
        # could be bad!
        sys.exit(0)

    # Must own the list lock!
    mlist.Lock()
    try:
        # Install the emergency shutdown signal handler
        signal.signal(signal.SIGTERM, sigterm_handler)
        # change the user's password.  The password must already have been
        # compared to the confirmpw and otherwise been vetted for
        # acceptability.
        mlist.setMemberPassword(user, newpw)
        mlist.Save()
    finally:
        mlist.Unlock()



def global_options(mlist, user, globalopts):
    # Is there anything to do?
    for attr in dir(globalopts):
        if attr.startswith('_'):
            continue
        if getattr(globalopts, attr) is not None:
            break
    else:
        return

    def sigterm_handler(signum, frame, mlist=mlist):
        # Make sure the list gets unlocked...
        mlist.Unlock()
        # ...and ensure we exit, otherwise race conditions could cause us to
        # enter MailList.Save() while we're in the unlocked state, and that
        # could be bad!
        sys.exit(0)

    # Must own the list lock!
    mlist.Lock()
    try:
        # Install the emergency shutdown signal handler
        signal.signal(signal.SIGTERM, sigterm_handler)

        if globalopts.enable is not None:
            mlist.setDeliveryStatus(user, globalopts.enable)

        if globalopts.remind is not None:
            mlist.setMemberOption(user, mm_cfg.SuppressPasswordReminder,
                                  globalopts.remind)

        if globalopts.nodupes is not None:
            mlist.setMemberOption(user, mm_cfg.DontReceiveDuplicates,
                                  globalopts.nodupes)

        if globalopts.mime is not None:
            mlist.setMemberOption(user, mm_cfg.DisableMime, globalopts.mime)

        mlist.Save()
    finally:
        mlist.Unlock()



def topic_details(mlist, doc, user, cpuser, userlang, varhelp):
    # Find out which topic the user wants to get details of
    reflist = varhelp.split('/')
    name = None
    topicname = _('<missing>')
    if len(reflist) == 1:
        topicname = urllib.unquote_plus(reflist[0])
        for name, pattern, description, emptyflag in mlist.topics:
            if name == topicname:
                break
        else:
            name = None

    if not name:
        options_page(mlist, doc, user, cpuser, userlang,
                     _('Requested topic is not valid: %(topicname)s'))
        print doc.Format()
        return

    table = Table(border=3, width='100%')
    table.AddRow([Center(Bold(_('Topic filter details')))])
    table.AddCellInfo(table.GetCurrentRowIndex(), 0, colspan=2,
                      bgcolor=mm_cfg.WEB_SUBHEADER_COLOR)
    table.AddRow([Bold(Label(_('Name:'))),
                  Utils.websafe(name)])
    table.AddRow([Bold(Label(_('Pattern (as regexp):'))),
                  '<pre>' + Utils.websafe(pattern) + '</pre>'])
    table.AddRow([Bold(Label(_('Description:'))),
                  Utils.websafe(description)])
    # Make colors look nice
    for row in range(1, 4):
        table.AddCellInfo(row, 0, bgcolor=mm_cfg.WEB_ADMINITEM_COLOR)

    options_page(mlist, doc, user, cpuser, userlang, table.Format())
    print doc.Format()
