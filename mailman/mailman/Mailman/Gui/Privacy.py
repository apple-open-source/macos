# Copyright (C) 2001,2002 by the Free Software Foundation, Inc.
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

"""MailList mixin class managing the privacy options.
"""

from Mailman import mm_cfg
from Mailman.i18n import _
from Mailman.Gui.GUIBase import GUIBase



class Privacy(GUIBase):
    def GetConfigCategory(self):
        return 'privacy', _('Privacy options')

    def GetConfigSubCategories(self, category):
        if category == 'privacy':
            return [('subscribing', _('Subscription&nbsp;rules')),
                    ('sender',      _('Sender&nbsp;filters')),
                    ('recipient',   _('Recipient&nbsp;filters')),
                    ('spam',        _('Spam&nbsp;filters')),
                    ]
        return None

    def GetConfigInfo(self, mlist, category, subcat=None):
        if category <> 'privacy':
            return None
        # Pre-calculate some stuff.  Technically, we shouldn't do the
        # sub_cfentry calculation here, but it's too ugly to indent it any
        # further, and besides, that'll mess up i18n catalogs.
        WIDTH = mm_cfg.TEXTFIELDWIDTH
        if mm_cfg.ALLOW_OPEN_SUBSCRIBE:
            sub_cfentry = ('subscribe_policy', mm_cfg.Radio,
                           # choices
                           (_('None'),
                            _('Confirm'),
                            _('Require approval'),
                            _('Confirm and approve')),
                           0, 
                           _('What steps are required for subscription?<br>'),
                           _('''None - no verification steps (<em>Not
                           Recommended </em>)<br>
                           Confirm (*) - email confirmation step required <br>
                           Require approval - require list administrator
                           Approval for subscriptions <br>
                           Confirm and approve - both confirm and approve
                           
                           <p>(*) when someone requests a subscription,
                           Mailman sends them a notice with a unique
                           subscription request number that they must reply to
                           in order to subscribe.<br>

                           This prevents mischievous (or malicious) people
                           from creating subscriptions for others without
                           their consent.'''))
        else:
            sub_cfentry = ('subscribe_policy', mm_cfg.Radio,
                           # choices
                           (_('Confirm'),
                            _('Require approval'),
                            _('Confirm and approve')),
                           1,
                           _('What steps are required for subscription?<br>'),
                           _('''Confirm (*) - email confirmation required <br>
                           Require approval - require list administrator
                           approval for subscriptions <br>
                           Confirm and approve - both confirm and approve
                           
                           <p>(*) when someone requests a subscription,
                           Mailman sends them a notice with a unique
                           subscription request number that they must reply to
                           in order to subscribe.<br> This prevents
                           mischievous (or malicious) people from creating
                           subscriptions for others without their consent.'''))

        # some helpful values
        admin = mlist.GetScriptURL('admin')

        subscribing_rtn = [
            _("""This section allows you to configure subscription and
            membership exposure policy.  You can also control whether this
            list is public or not.  See also the
            <a href="%(admin)s/archive">Archival Options</a> section for
            separate archive-related privacy settings."""),

            _('Subscribing'),
            ('advertised', mm_cfg.Radio, (_('No'), _('Yes')), 0,
             _('''Advertise this list when people ask what lists are on this
             machine?''')),

            sub_cfentry,
            
            ('unsubscribe_policy', mm_cfg.Radio, (_('No'), _('Yes')), 0,
             _("""Is the list moderator's approval required for unsubscription
             requests?  (<em>No</em> is recommended)"""),

             _("""When members want to leave a list, they will make an
             unsubscription request, either via the web or via email.
             Normally it is best for you to allow open unsubscriptions so that
             users can easily remove themselves from mailing lists (they get
             really upset if they can't get off lists!).

             <p>For some lists though, you may want to impose moderator
             approval before an unsubscription request is processed.  Examples
             of such lists include a corporate mailing list that all employees
             are required to be members of.""")),

            _('Ban list'),
            ('ban_list', mm_cfg.EmailListEx, (10, WIDTH), 1,
             _("""List of addresses which are banned from membership in this
             mailing list."""),

             _("""Addresses in this list are banned outright from subscribing
             to this mailing list, with no further moderation required.  Add
             addresses one per line; start the line with a ^ character to
             designate a regular expression match.""")),

            _("Membership exposure"),
            ('private_roster', mm_cfg.Radio,
             (_('Anyone'), _('List members'), _('List admin only')), 0,
             _('Who can view subscription list?'),

             _('''When set, the list of subscribers is protected by member or
             admin password authentication.''')),

            ('obscure_addresses', mm_cfg.Radio, (_('No'), _('Yes')), 0,
             _("""Show member addresses so they're not directly recognizable
             as email addresses?"""),
             _("""Setting this option causes member email addresses to be
             transformed when they are presented on list web pages (both in
             text and as links), so they're not trivially recognizable as
             email addresses.  The intention is to prevent the addresses
             from being snarfed up by automated web scanners for use by
             spammers.""")),
            ]

        adminurl = mlist.GetScriptURL('admin', absolute=1)
        sender_rtn = [
            _("""When a message is posted to the list, a series of
            moderation steps are take to decide whether the a moderator must
            first approve the message or not.  This section contains the
            controls for moderation of both member and non-member postings.

            <p>Member postings are held for moderation if their
            <b>moderation flag</b> is turned on.  You can control whether
            member postings are moderated by default or not.

            <p>Non-member postings can be automatically
            <a href="?VARHELP=privacy/sender/accept_these_nonmembers"
            >accepted</a>,
            <a href="?VARHELP=privacy/sender/hold_these_nonmembers">held for
            moderation</a>,
            <a href="?VARHELP=privacy/sender/reject_these_nonmembers"
            >rejected</a> (bounced), or
            <a href="?VARHELP=privacy/sender/discard_these_nonmembers"
            >discarded</a>,
            either individually or as a group.  Any
            posting from a non-member who is not explicitly accepted,
            rejected, or discarded, will have their posting filtered by the
            <a href="?VARHELP=privacy/sender/generic_nonmember_action">general
            non-member rules</a>.

            <p>In the text boxes below, add one address per line; start the
            line with a ^ character to designate a <a href=
            "http://www.python.org/doc/current/lib/module-re.html"
            >Python regular expression</a>.  When entering backslashes, do so
            as if you were using Python raw strings (i.e. you generally just
            use a single backslash).

            <p>Note that non-regexp matches are always done first."""),

            _('Member filters'),

            ('default_member_moderation', mm_cfg.Radio, (_('No'), _('Yes')),
             0, _('By default, should new list member postings be moderated?'),

             _("""Each list member has a <em>moderation flag</em> which says
             whether messages from the list member can be posted directly to
             the list, or must first be approved by the list moderator.  When
             the moderation flag is turned on, list member postings must be
             approved first.  You, the list administrator can decide whether a
             specific individual's postings will be moderated or not.

             <p>When a new member is subscribed, their initial moderation flag
             takes its value from this option.  Turn this option off to accept
             member postings by default.  Turn this option on to, by default,
             moderate member postings first.  You can always manually set an
             individual member's moderation bit by using the
             <a href="%(adminurl)s/members">membership management
             screens</a>.""")),

            ('member_moderation_action', mm_cfg.Radio,
             (_('Hold'), _('Reject'), _('Discard')), 0,
             _("""Action to take when a moderated member posts to the
             list."""),
             _("""<ul><li><b>Hold</b> -- this holds the message for approval
             by the list moderators.

             <p><li><b>Reject</b> -- this automatically rejects the message by
             sending a bounce notice to the post's author.  The text of the
             bounce notice can be <a
             href="?VARHELP=privacy/sender/member_moderation_notice"
             >configured by you</a>.

             <p><li><b>Discard</b> -- this simply discards the message, with
             no notice sent to the post's author.
             </ul>""")),

            ('member_moderation_notice', mm_cfg.Text, (10, WIDTH), 1,
             _("""Text to include in any
             <a href="?VARHELP/privacy/sender/member_moderation_action"
             >rejection notice</a> to
             be sent to moderated members who post to this list.""")),

            _('Non-member filters'),

            ('accept_these_nonmembers', mm_cfg.EmailListEx, (10, WIDTH), 1,
             _("""List of non-member addresses whose postings should be
             automatically accepted."""),

             _("""Postings from any of these non-members will be automatically
             accepted with no further moderation applied.  Add member
             addresses one per line; start the line with a ^ character to
             designate a regular expression match.""")),

            ('hold_these_nonmembers', mm_cfg.EmailListEx, (10, WIDTH), 1,
             _("""List of non-member addresses whose postings will be
             immediately held for moderation."""),

             _("""Postings from any of these non-members will be immediately
             and automatically held for moderation by the list moderators.
             The sender will receive a notification message which will allow
             them to cancel their held message.  Add member addresses one per
             line; start the line with a ^ character to designate a regular
             expression match.""")),

            ('reject_these_nonmembers', mm_cfg.EmailListEx, (10, WIDTH), 1,
             _("""List of non-member addresses whose postings will be
             automatically rejected."""),

             _("""Postings from any of these non-members will be automatically
             rejected.  In other words, their messages will be bounced back to
             the sender with a notification of automatic rejection.  This
             option is not appropriate for known spam senders; their messages
             should be
             <a href="?VARHELP=privacy/sender/discard_these_nonmembers"
             >automatically discarded</a>.

             <p>Add member addresses one per line; start the line with a ^
             character to designate a regular expression match.""")),

            ('discard_these_nonmembers', mm_cfg.EmailListEx, (10, WIDTH), 1,
             _("""List of non-member addresses whose postings will be
             automatically discarded."""),

             _("""Postings from any of these non-members will be automatically
             discarded.  That is, the message will be thrown away with no
             further processing or notification.  The sender will not receive
             a notification or a bounce, however the list moderators can
             optionally <a href="?VARHELP=privacy/sender/forward_auto_discards"
             >receive copies of auto-discarded messages.</a>.

             <p>Add member addresses one per line; start the line with a ^
             character to designate a regular expression match.""")),

            ('generic_nonmember_action', mm_cfg.Radio,
             (_('Accept'), _('Hold'), _('Reject'), _('Discard')), 0,
             _("""Action to take for postings from non-members for which no
             explicit action is defined."""),

             _("""When a post from a non-member is received, the message's
             sender is matched against the list of explicitly
             <a href="?VARHELP=privacy/sender/accept_these_nonmembers"
             >accepted</a>,
             <a href="?VARHELP=privacy/sender/hold_these_nonmembers">held</a>,
             <a href="?VARHELP=privacy/sender/reject_these_nonmembers"
             >rejected</a> (bounced), and
             <a href="?VARHELP=privacy/sender/discard_these_nonmembers"
             >discarded</a> addresses.  If no match is found, then this action
             is taken.""")),

            ('forward_auto_discards', mm_cfg.Radio, (_('No'), _('Yes')), 0,
             _("""Should messages from non-members, which are automatically
             discarded, be forwarded to the list moderator?""")),

            ]

        recip_rtn = [
            _("""This section allows you to configure various filters based on
            the recipient of the message."""),

            _('Recipient filters'),

            ('require_explicit_destination', mm_cfg.Radio,
             (_('No'), _('Yes')), 0,
             _("""Must posts have list named in destination (to, cc) field
             (or be among the acceptable alias names, specified below)?"""),

             _("""Many (in fact, most) spams do not explicitly name their
             myriad destinations in the explicit destination addresses - in
             fact often the To: field has a totally bogus address for
             obfuscation.  The constraint applies only to the stuff in the
             address before the '@' sign, but still catches all such spams.

             <p>The cost is that the list will not accept unhindered any
             postings relayed from other addresses, unless

             <ol>
                 <li>The relaying address has the same name, or

                 <li>The relaying address name is included on the options that
                 specifies acceptable aliases for the list.

             </ol>""")),

            ('acceptable_aliases', mm_cfg.Text, (4, WIDTH), 0,
             _("""Alias names (regexps) which qualify as explicit to or cc
             destination names for this list."""),

             _("""Alternate addresses that are acceptable when
             `require_explicit_destination' is enabled.  This option takes a
             list of regular expressions, one per line, which is matched
             against every recipient address in the message.  The matching is
             performed with Python's re.match() function, meaning they are
             anchored to the start of the string.
             
             <p>For backwards compatibility with Mailman 1.1, if the regexp
             does not contain an `@', then the pattern is matched against just
             the local part of the recipient address.  If that match fails, or
             if the pattern does contain an `@', then the pattern is matched
             against the entire recipient address.
             
             <p>Matching against the local part is deprecated; in a future
             release, the pattern will always be matched against the entire
             recipient address.""")),

            ('max_num_recipients', mm_cfg.Number, 5, 0, 
             _('Ceiling on acceptable number of recipients for a posting.'),

             _('''If a posting has this number, or more, of recipients, it is
             held for admin approval.  Use 0 for no ceiling.''')),
            ]

        spam_rtn = [
            _("""This section allows you to configure various anti-spam
            filters posting filters, which can help reduce the amount of spam
            your list members end up receiving.
            """),

            _("Anti-Spam filters"),

            ('bounce_matching_headers', mm_cfg.Text, (6, WIDTH), 0,
             _('Hold posts with header value matching a specified regexp.'),
             _("""Use this option to prohibit posts according to specific
             header values.  The target value is a regular-expression for
             matching against the specified header.  The match is done
             disregarding letter case.  Lines beginning with '#' are ignored
             as comments.

             <p>For example:<pre>to: .*@public.com </pre> says to hold all
             postings with a <em>To:</em> mail header containing '@public.com'
             anywhere among the addresses.

             <p>Note that leading whitespace is trimmed from the regexp.  This
             can be circumvented in a number of ways, e.g. by escaping or
             bracketing it.""")),
          ]

        if subcat == 'sender':
            return sender_rtn
        elif subcat == 'recipient':
            return recip_rtn
        elif subcat == 'spam':
            return spam_rtn
        else:
            return subscribing_rtn

    def _setValue(self, mlist, property, val, doc):
        # For subscribe_policy when ALLOW_OPEN_SUBSCRIBE is true, we need to
        # add one to the value because the page didn't present an open list as
        # an option.
        if property == 'subscribe_policy' and not mm_cfg.ALLOW_OPEN_SUBSCRIBE:
            val += 1
        setattr(mlist, property, val)
