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

from Mailman import mm_cfg
from Mailman.i18n import _
from Mailman.Gui.GUIBase import GUIBase



class Usenet(GUIBase):
    def GetConfigCategory(self):
        return 'gateway', _('Mail&lt;-&gt;News&nbsp;gateways')

    def GetConfigInfo(self, mlist, category, subcat=None):
        if category <> 'gateway':
            return None

        WIDTH = mm_cfg.TEXTFIELDWIDTH
        VERTICAL = 1

        return [
            _('Mail-to-News and News-to-Mail gateway services.'),

            _('News server settings'),

            ('nntp_host', mm_cfg.String, WIDTH, 0,
             _('The hostname of the machine your news server is running on.'),
             _('''This value may be either the name of your news server, or
             optionally of the format name:port, where port is a port number.

             The news server is not part of Mailman proper.  You have to
             already have access to an NNTP server, and that NNTP server must
             recognize the machine this mailing list runs on as a machine
             capable of reading and posting news.''')),

            ('linked_newsgroup', mm_cfg.String, WIDTH, 0,
              _('The name of the Usenet group to gateway to and/or from.')),

            ('gateway_to_news',  mm_cfg.Toggle, (_('No'), _('Yes')), 0,
             _('''Should new posts to the mailing list be sent to the
             newsgroup?''')),

            ('gateway_to_mail',  mm_cfg.Toggle, (_('No'), _('Yes')), 0,
             _('''Should new posts to the newsgroup be sent to the mailing
             list?''')),

            _('Forwarding options'),

            ('news_moderation', mm_cfg.Radio,
             (_('None'), _('Open list, moderated group'), _('Moderated')),
             VERTICAL,

             _("""The moderation policy of the newsgroup."""),

             _("""This setting determines the moderation policy of the
             newsgroup and its interaction with the moderation policy of the
             mailing list.  This only applies to the newsgroup that you are
             gatewaying <em>to</em>, so if you are only gatewaying from
             Usenet, or the newsgroup you are gatewaying to is not moderated,
             set this option to <em>None</em>.

             <p>If the newsgroup is moderated, you can set this mailing list
             up to be the moderation address for the newsgroup.  By selecting
             <em>Moderated</em>, an additional posting hold will be placed in
             the approval process.  All messages posted to the mailing list
             will have to be approved before being sent on to the newsgroup,
             or to the mailing list membership.

             <p><em>Note that if the message has an <tt>Approved</tt> header
             with the list's administrative password in it, this hold test
             will be bypassed, allowing privileged posters to send messages
             directly to the list and the newsgroup.</em>

             <p>Finally, if the newsgroup is moderated, but you want to have
             an open posting policy anyway, you should select <em>Open list,
             moderated group</em>.  The effect of this is to use the normal
             Mailman moderation facilities, but to add an <tt>Approved</tt>
             header to all messages that are gatewayed to Usenet.""")),

            ('news_prefix_subject_too', mm_cfg.Toggle, (_('No'), _('Yes')), 0,
             _('Prefix <tt>Subject:</tt> headers on postings gated to news?'),
             _("""Mailman prefixes <tt>Subject:</tt> headers with
             <a href="?VARHELP=general/subject_prefix">text you can
             customize</a> and normally, this prefix shows up in messages
             gatewayed to Usenet.  You can set this option to <em>No</em> to
             disable the prefix on gated messages.  Of course, if you turn off
             normal <tt>Subject:</tt> prefixes, they won't be prefixed for
             gated messages either.""")),

            _('Mass catch up'),

            ('_mass_catchup', mm_cfg.Toggle, (_('No'), _('Yes')), 0,
             _('Should Mailman perform a <em>catchup</em> on the newsgroup?'),
             _('''When you tell Mailman to perform a catchup on the newsgroup,
             this means that you want to start gating messages to the mailing
             list with the next new message found.  All earlier messages on
             the newsgroup will be ignored.  This is as if you were reading
             the newsgroup yourself, and you marked all current messages as
             <em>read</em>.  By catching up, your mailing list members will
             not see any of the earlier messages.''')),

            ]

    def _setValue(self, mlist, property, val, doc):
        # Watch for the special, immediate action attributes
        if property == '_mass_catchup' and val:
            mlist.usenet_watermark = None
            doc.AddItem(_('Mass catchup completed'))
        else:
            GUIBase._setValue(self, mlist, property, val, doc)

    def _postValidate(self, mlist, doc):
        # Make sure that if we're gating, that the newsgroups and host
        # information are not blank.
        if mlist.gateway_to_news or mlist.gateway_to_mail:
            # BAW: It's too expensive and annoying to ensure that both the
            # host is valid and that the newsgroup is a valid n.g. on the
            # server.  This should be good enough.
            if not mlist.nntp_host or not mlist.linked_newsgroup:
                doc.addError(_("""You cannot enable gatewaying unless both the
                <a href="?VARHELP=gateway/nntp_host">news server field</a> and
                the <a href="?VARHELP=gateway/linked_newsgroup">linked
                newsgroup</a> fields are filled in."""))
                # And reset these values
                mlist.gateway_to_news = 0
                mlist.gateway_to_mail = 0
