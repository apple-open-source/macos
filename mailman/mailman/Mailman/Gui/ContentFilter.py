# Copyright (C) 2002-2005 by the Free Software Foundation, Inc.
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

"""GUI component managing the content filtering options."""

from Mailman import mm_cfg
from Mailman.i18n import _
from Mailman.Gui.GUIBase import GUIBase

NL = '\n'



class ContentFilter(GUIBase):
    def GetConfigCategory(self):
        return 'contentfilter', _('Content&nbsp;filtering')

    def GetConfigInfo(self, mlist, category, subcat=None):
        if category <> 'contentfilter':
            return None
        WIDTH = mm_cfg.TEXTFIELDWIDTH

        actions = [_('Discard'), _('Reject'), _('Forward to List Owner')]
        if mm_cfg.OWNERS_CAN_PRESERVE_FILTERED_MESSAGES:
            actions.append(_('Preserve'))

        return [
            _("""Policies concerning the content of list traffic.

            <p>Content filtering works like this: when a message is
            received by the list and you have enabled content filtering, the
            individual attachments are first compared to the
            <a href="?VARHELP=contentfilter/filter_mime_types">filter
            types</a>.  If the attachment type matches an entry in the filter
            types, it is discarded.

            <p>Then, if there are <a
            href="?VARHELP=contentfilter/pass_mime_types">pass types</a>
            defined, any attachment type that does <em>not</em> match a
            pass type is also discarded.  If there are no pass types defined,
            this check is skipped.

            <p>After this initial filtering, any <tt>multipart</tt>
            attachments that are empty are removed.  If the outer message is
            left empty after this filtering, then the whole message is
            discarded.

            <p> Then, each <tt>multipart/alternative</tt> section will
            be replaced by just the first alternative that is non-empty after
            filtering if
            <a href="?VARHELP=contentfilter/collapse_alternatives"
            >collapse_alternatives</a> is enabled.

            <p>Finally, any <tt>text/html</tt> parts that are left in the
            message may be converted to <tt>text/plain</tt> if
            <a href="?VARHELP=contentfilter/convert_html_to_plaintext"
            >convert_html_to_plaintext</a> is enabled and the site is
            configured to allow these conversions."""),

            ('filter_content', mm_cfg.Radio, (_('No'), _('Yes')), 0,
             _("""Should Mailman filter the content of list traffic according
             to the settings below?""")),

            ('filter_mime_types', mm_cfg.Text, (10, WIDTH), 0,
             _("""Remove message attachments that have a matching content
             type."""),

             _("""Use this option to remove each message attachment that
             matches one of these content types.  Each line should contain a
             string naming a MIME <tt>type/subtype</tt>,
             e.g. <tt>image/gif</tt>.  Leave off the subtype to remove all
             parts with a matching major content type, e.g. <tt>image</tt>.

             <p>Blank lines are ignored.

             <p>See also <a href="?VARHELP=contentfilter/pass_mime_types"
             >pass_mime_types</a> for a content type whitelist.""")),

            ('pass_mime_types', mm_cfg.Text, (10, WIDTH), 0,
             _("""Remove message attachments that don't have a matching
             content type.  Leave this field blank to skip this filter
             test."""),

             _("""Use this option to remove each message attachment that does
             not have a matching content type.  Requirements and formats are
             exactly like <a href="?VARHELP=contentfilter/filter_mime_types"
             >filter_mime_types</a>.

             <p><b>Note:</b> if you add entries to this list but don't add
             <tt>multipart</tt> to this list, any messages with attachments
             will be rejected by the pass filter.""")),

            ('filter_filename_extensions', mm_cfg.Text, (10, WIDTH), 0,
             _("""Remove message attachments that have a matching filename
             extension."""),),

            ('pass_filename_extensions', mm_cfg.Text, (10, WIDTH), 0,
             _("""Remove message attachments that don't have a matching
             filename extension.  Leave this field blank to skip this filter
             test."""),),

            ('collapse_alternatives', mm_cfg.Radio, (_('No'), _('Yes')), 0,
             _("""Should Mailman collapse multipart/alternative to its
             first part content?""")),

            ('convert_html_to_plaintext', mm_cfg.Radio, (_('No'), _('Yes')), 0,
             _("""Should Mailman convert <tt>text/html</tt> parts to plain
             text?  This conversion happens after MIME attachments have been
             stripped.""")),

            ('filter_action', mm_cfg.Radio, tuple(actions), 0,

             _("""Action to take when a message matches the content filtering
             rules."""),

             _("""One of these actions is taken when the message matches one of
             the content filtering rules, meaning, the top-level
             content type matches one of the <a
             href="?VARHELP=contentfilter/filter_mime_types"
             >filter_mime_types</a>, or the top-level content type does
             <strong>not</strong> match one of the
             <a href="?VARHELP=contentfilter/pass_mime_types"
             >pass_mime_types</a>, or if after filtering the subparts of the
             message, the message ends up empty.

             <p>Note this action is not taken if after filtering the message
             still contains content.  In that case the message is always
             forwarded on to the list membership.

             <p>When messages are discarded, a log entry is written
             containing the Message-ID of the discarded message.  When
             messages are rejected or forwarded to the list owner, a reason
             for the rejection is included in the bounce message to the
             original author.  When messages are preserved, they are saved in
             a special queue directory on disk for the site administrator to
             view (and possibly rescue) but otherwise discarded.  This last
             option is only available if enabled by the site
             administrator.""")),
            ]

    def _setValue(self, mlist, property, val, doc):
        if property in ('filter_mime_types', 'pass_mime_types'):
            types = []
            for spectype in [s.strip() for s in val.splitlines()]:
                ok = 1
                slashes = spectype.count('/')
                if slashes == 0 and not spectype:
                    ok = 0
                elif slashes == 1:
                    maintype, subtype = [s.strip().lower()
                                         for s in spectype.split('/')]
                    if not maintype or not subtype:
                        ok = 0
                elif slashes > 1:
                    ok = 0
                if not ok:
                    doc.addError(_('Bad MIME type ignored: %(spectype)s'))
                else:
                    types.append(spectype.strip().lower())
            if property == 'filter_mime_types':
                mlist.filter_mime_types = types
            elif property == 'pass_mime_types':
                mlist.pass_mime_types = types
        elif property in ('filter_filename_extensions',
                          'pass_filename_extensions'):
            fexts = []
            for ext in [s.strip() for s in val.splitlines()]:
                fexts.append(ext.lower())
            if property == 'filter_filename_extensions':
                mlist.filter_filename_extensions = fexts
            elif property == 'pass_filename_extensions':
                mlist.pass_filename_extensions = fexts
        else:
            GUIBase._setValue(self, mlist, property, val, doc)

    def getValue(self, mlist, kind, property, params):
        if property == 'filter_mime_types':
            return NL.join(mlist.filter_mime_types)
        if property == 'pass_mime_types':
            return NL.join(mlist.pass_mime_types)
        if property == 'filter_filename_extensions':
            return NL.join(mlist.filter_filename_extensions)
        if property == 'pass_filename_extensions':
            return NL.join(mlist.pass_filename_extensions)
        return None
