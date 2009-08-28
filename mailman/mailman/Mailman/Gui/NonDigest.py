# Copyright (C) 2001-2007 by the Free Software Foundation, Inc.
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

"""GUI component for managing the non-digest delivery options.
"""

from Mailman import mm_cfg
from Mailman import Utils
from Mailman.i18n import _
from Mailman.Gui.GUIBase import GUIBase

from Mailman.Gui.Digest import ALLOWEDS
PERSONALIZED_ALLOWEDS = ('user_address', 'user_delivered_to', 'user_password',
                         'user_name', 'user_optionsurl',
                         )



class NonDigest(GUIBase):
    def GetConfigCategory(self):
        return 'nondigest', _('Non-digest&nbsp;options')

    def GetConfigInfo(self, mlist, category, subcat=None):
        if category <> 'nondigest':
            return None
        WIDTH = mm_cfg.TEXTFIELDWIDTH

        info = [
            _("Policies concerning immediately delivered list traffic."),

            ('nondigestable', mm_cfg.Toggle, (_('No'), _('Yes')), 1,
             _("""Can subscribers choose to receive mail immediately, rather
             than in batched digests?""")),
            ]

        if mm_cfg.OWNERS_CAN_ENABLE_PERSONALIZATION:
            info.extend([
                ('personalize', mm_cfg.Radio,
                 (_('No'), _('Yes'), _('Full Personalization')), 1,

                 _('''Should Mailman personalize each non-digest delivery?
                 This is often useful for announce-only lists, but <a
                 href="?VARHELP=nondigest/personalize">read the details</a>
                 section for a discussion of important performance
                 issues.'''),

                 _("""Normally, Mailman sends the regular delivery messages to
                 the mail server in batches.  This is much more efficent
                 because it reduces the amount of traffic between Mailman and
                 the mail server.

                 <p>However, some lists can benefit from a more personalized
                 approach.  In this case, Mailman crafts a new message for
                 each member on the regular delivery list.  Turning this
                 feature on may degrade the performance of your site, so you
                 need to carefully consider whether the trade-off is worth it,
                 or whether there are other ways to accomplish what you want.
                 You should also carefully monitor your system load to make
                 sure it is acceptable.

                 <p>Select <em>No</em> to disable personalization and send
                 messages to the members in batches.  Select <em>Yes</em> to
                 personalize deliveries and allow additional substitution
                 variables in message headers and footers (see below).  In
                 addition, by selecting <em>Full Personalization</em>, the
                 <code>To</code> header of posted messages will be modified to
                 include the member's address instead of the list's posting
                 address.

                 <p>When personalization is enabled, a few more expansion
                 variables that can be included in the <a
                 href="?VARHELP=nondigest/msg_header">message header</a> and
                 <a href="?VARHELP=nondigest/msg_footer">message footer</a>.

                 <p>These additional substitution variables will be available
                 for your headers and footers, when this feature is enabled:

                 <ul><li><b>user_address</b> - The address of the user,
                         coerced to lower case.
                     <li><b>user_delivered_to</b> - The case-preserved address
                         that the user is subscribed with.
                     <li><b>user_password</b> - The user's password.
                     <li><b>user_name</b> - The user's full name.
                     <li><b>user_optionsurl</b> - The url to the user's option
                         page.
                 </ul>
                 """))
                ])
        # BAW: for very dumb reasons, we want the `personalize' attribute to
        # show up before the msg_header and msg_footer attrs, otherwise we'll
        # get a bogus warning if the header/footer contains a personalization
        # substitution variable, and we're transitioning from no
        # personalization to personalization enabled.
        headfoot = Utils.maketext('headfoot.html', mlist=mlist, raw=1)
        if mm_cfg.OWNERS_CAN_ENABLE_PERSONALIZATION:
            extra = _("""\
When <a href="?VARHELP=nondigest/personalize">personalization</a> is enabled
for this list, additional substitution variables are allowed in your headers
and footers:

<ul><li><b>user_address</b> - The address of the user,
        coerced to lower case.
    <li><b>user_delivered_to</b> - The case-preserved address
        that the user is subscribed with.
    <li><b>user_password</b> - The user's password.
    <li><b>user_name</b> - The user's full name.
    <li><b>user_optionsurl</b> - The url to the user's option
        page.
</ul>
""")
        else:
            extra = ''

        info.extend([('msg_header', mm_cfg.Text, (10, WIDTH), 0,
             _('Header added to mail sent to regular list members'),
             _('''Text prepended to the top of every immediately-delivery
             message. ''') + headfoot + extra),

            ('msg_footer', mm_cfg.Text, (10, WIDTH), 0,
             _('Footer added to mail sent to regular list members'),
             _('''Text appended to the bottom of every immediately-delivery
             message. ''') + headfoot + extra),
            ])

        info.extend([
            ('scrub_nondigest', mm_cfg.Toggle, (_('No'), _('Yes')), 0,
             _('Scrub attachments of regular delivery message?'),
             _('''When you scrub attachments, they are stored in archive
             area and links are made in the message so that the member can
             access via web browser. If you want the attachments totally
             disappear, you can use content filter options.''')),
            ])

        info.extend([
            _('Sibling lists'),

            ('regular_exclude_lists', mm_cfg.EmailList, (3, WIDTH), 0,
             _("""Other mailing lists on this site whose members are
             excluded from the regular (non-digest) delivery if those
             list addresses appear in a To: or Cc: header."""),
             _("""The list addresses should be written in full mail address
             format (e.g. mailman@example.com). Do not specify this list
             address mutually in the exclude list configuration page of the
             other list, or members of both lists won't get any message.
             Note also that the site administrator may prohibit cross domain
             siblings.""")),

            ('regular_include_lists', mm_cfg.EmailList, (3, WIDTH), 0,
             _("""Other mailing lists on this site whose members are
             included in the regular (non-digest) delivery if those
             list addresses don't appear in a To: or Cc: header."""),
             _("""The list addresses should be written in full mail address
             format (e.g. mailman@example.com). Note also that the site
             administrator may prohibit cross domain siblings.""")),
            ])


        return info

    def _setValue(self, mlist, property, val, doc):
        alloweds = list(ALLOWEDS)
        if mlist.personalize:
            alloweds.extend(PERSONALIZED_ALLOWEDS)
        if property in ('msg_header', 'msg_footer'):
            val = self._convertString(mlist, property, alloweds, val, doc)
            if val is None:
                # There was a problem, so don't set it
                return
        GUIBase._setValue(self, mlist, property, val, doc)
