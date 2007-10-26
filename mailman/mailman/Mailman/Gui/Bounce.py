# Copyright (C) 2001-2004 by the Free Software Foundation, Inc.
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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

from Mailman import mm_cfg
from Mailman.i18n import _
from Mailman.mm_cfg import days
from Mailman.Gui.GUIBase import GUIBase



class Bounce(GUIBase):
    def GetConfigCategory(self):
        return 'bounce', _('Bounce processing')

    def GetConfigInfo(self, mlist, category, subcat=None):
        if category <> 'bounce':
            return None
        return [
            _("""These policies control the automatic bounce processing system
            in Mailman.  Here's an overview of how it works.

            <p>When a bounce is received, Mailman tries to extract two pieces
            of information from the message: the address of the member the
            message was intended for, and the severity of the problem causing
            the bounce.  The severity can be either <em>hard</em> or
            <em>soft</em> meaning either a fatal error occurred, or a
            transient error occurred.  When in doubt, a hard severity is used.

            <p>If no member address can be extracted from the bounce, then the
            bounce is usually discarded.  Otherwise, each member is assigned a
            <em>bounce score</em> and every time we encounter a bounce from
            this member we increment the score.  Hard bounces increment by 1
            while soft bounces increment by 0.5.  We only increment the bounce
            score once per day, so even if we receive ten hard bounces from a
            member per day, their score will increase by only 1 for that day.

            <p>When a member's bounce score is greater than the
            <a href="?VARHELP=bounce/bounce_score_threshold">bounce score
            threshold</a>, the subscription is disabled.  Once disabled, the
            member will not receive any postings from the list until their
            membership is explicitly re-enabled (either by the list
            administrator or the user).  However, they will receive occasional
            reminders that their membership has been disabled, and these
            reminders will include information about how to re-enable their
            membership.

            <p>You can control both the
            <a href="?VARHELP=bounce/bounce_you_are_disabled_warnings">number
            of reminders</a> the member will receive and the
            <a href="?VARHELP=bounce/bounce_you_are_disabled_warnings_interval"
            >frequency</a> with which these reminders are sent.

            <p>There is one other important configuration variable; after a
            certain period of time -- during which no bounces from the member
            are received -- the bounce information is
            <a href="?VARHELP=bounce/bounce_info_stale_after">considered
            stale</a> and discarded.  Thus by adjusting this value, and the
            score threshold, you can control how quickly bouncing members are
            disabled.  You should tune both of these to the frequency and
            traffic volume of your list."""),

            _('Bounce detection sensitivity'),

            ('bounce_processing', mm_cfg.Toggle, (_('No'), _('Yes')), 0,
             _('Should Mailman perform automatic bounce processing?'),
             _("""By setting this value to <em>No</em>, you disable all
             automatic bounce processing for this list, however bounce
             messages will still be discarded so that the list administrator
             isn't inundated with them.""")),

            ('bounce_score_threshold', mm_cfg.Number, 5, 0,
             _("""The maximum member bounce score before the member's
             subscription is disabled.  This value can be a floating point
             number."""),
             _("""Each subscriber is assigned a bounce score, as a floating
             point number.  Whenever Mailman receives a bounce from a list
             member, that member's score is incremented.  Hard bounces (fatal
             errors) increase the score by 1, while soft bounces (temporary
             errors) increase the score by 0.5.  Only one bounce per day
             counts against a member's score, so even if 10 bounces are
             received for a member on the same day, their score will increase
             by just 1.

             This variable describes the upper limit for a member's bounce
             score, above which they are automatically disabled, but not
             removed from the mailing list.""")),

            ('bounce_info_stale_after', mm_cfg.Number, 5, 0,
             _("""The number of days after which a member's bounce information
             is discarded, if no new bounces have been received in the
             interim.  This value must be an integer.""")),

            ('bounce_you_are_disabled_warnings', mm_cfg.Number, 5, 0,
             _("""How many <em>Your Membership Is Disabled</em> warnings a
             disabled member should get before their address is removed from
             the mailing list.  Set to 0 to immediately remove an address from
             the list once their bounce score exceeds the threshold.  This
             value must be an integer.""")),

            ('bounce_you_are_disabled_warnings_interval', mm_cfg.Number, 5, 0,
             _("""The number of days between sending the <em>Your Membership
             Is Disabled</em> warnings.  This value must be an integer.""")),

            _('Notifications'),

            ('bounce_unrecognized_goes_to_list_owner', mm_cfg.Toggle,
             (_('No'), _('Yes')), 0,
             _('''Should Mailman send you, the list owner, any bounce messages
             that failed to be detected by the bounce processor?  <em>Yes</em>
             is recommended.'''),
             _("""While Mailman's bounce detector is fairly robust, it's
             impossible to detect every bounce format in the world.  You
             should keep this variable set to <em>Yes</em> for two reasons: 1)
             If this really is a permanent bounce from one of your members,
             you should probably manually remove them from your list, and 2)
             you might want to send the message on to the Mailman developers
             so that this new format can be added to its known set.

             <p>If you really can't be bothered, then set this variable to
             <em>No</em> and all non-detected bounces will be discarded
             without further processing.

             <p><b>Note:</b> This setting will also affect all messages sent
             to your list's -admin address.  This address is deprecated and
             should never be used, but some people may still send mail to this
             address.  If this happens, and this variable is set to
             <em>No</em> those messages too will get discarded.  You may want
             to set up an
             <a href="?VARHELP=autoreply/autoresponse_admin_text">autoresponse
             message</a> for email to the -owner and -admin address.""")),

            ('bounce_notify_owner_on_disable', mm_cfg.Toggle,
             (_('No'), _('Yes')), 0,
             _("""Should Mailman notify you, the list owner, when bounces
             cause a member's subscription to be disabled?"""),
             _("""By setting this value to <em>No</em>, you turn off
             notification messages that are normally sent to the list owners
             when a member's delivery is disabled due to excessive bounces.
             An attempt to notify the member will always be made.""")),

            ('bounce_notify_owner_on_removal', mm_cfg.Toggle,
             (_('No'), _('Yes')), 0,
             _("""Should Mailman notify you, the list owner, when bounces
             cause a member to be unsubscribed?"""),
             _("""By setting this value to <em>No</em>, you turn off
             notification messages that are normally sent to the list owners
             when a member is unsubscribed due to excessive bounces.  An
             attempt to notify the member will always be made.""")),

            ]

    def _setValue(self, mlist, property, val, doc):
        # Do value conversion from web representation to internal
        # representation.
        try:
            if property == 'bounce_processing':
                val = int(val)
            elif property == 'bounce_score_threshold':
                val = float(val)
            elif property == 'bounce_info_stale_after':
                val = days(int(val))
            elif property == 'bounce_you_are_disabled_warnings':
                val = int(val)
            elif property == 'bounce_you_are_disabled_warnings_interval':
                val = days(int(val))
            elif property == 'bounce_notify_owner_on_disable':
                val = int(val)
            elif property == 'bounce_notify_owner_on_removal':
                val = int(val)
        except ValueError:
            doc.addError(
                _("""Bad value for <a href="?VARHELP=bounce/%(property)s"
                >%(property)s</a>: %(val)s"""),
                tag = _('Error: '))
            return
        GUIBase._setValue(self, mlist, property, val, doc)

    def getValue(self, mlist, kind, varname, params):
        if varname not in ('bounce_info_stale_after',
                           'bounce_you_are_disabled_warnings_interval'):
            return None
        return int(getattr(mlist, varname) / days(1))
