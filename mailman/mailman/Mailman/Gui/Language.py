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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

"""MailList mixin class managing the language options.
"""

import codecs

from Mailman import mm_cfg
from Mailman import Utils
from Mailman import i18n
from Mailman.Logging.Syslog import syslog
from Mailman.Gui.GUIBase import GUIBase

_ = i18n._



class Language(GUIBase):
    def GetConfigCategory(self):
        return 'language', _('Language&nbsp;options')

    def GetConfigInfo(self, mlist, category, subcat=None):
        if category <> 'language':
            return None

        # Set things up for the language choices
        langs = mlist.GetAvailableLanguages()
        langnames = [_(Utils.GetLanguageDescr(L)) for L in langs]
        try:
            langi = langs.index(mlist.preferred_language)
        except ValueError:
            # Someone must have deleted the list's preferred language.  Could
            # be other trouble lurking!
            langi = 0

        # Only allow the admin to choose a language if the system has a
        # charset for it.  I think this is the best way to test for that.
        def checkcodec(charset):
            try:
                codecs.lookup(charset)
                return 1
            except LookupError:
                return 0

        all = [key for key in mm_cfg.LC_DESCRIPTIONS.keys()
               if checkcodec(Utils.GetCharSet(key))]
        all.sort()
        checked = [L in langs for L in all]
        allnames = [_(Utils.GetLanguageDescr(L)) for L in all]

        return [
            _('Natural language (internationalization) options.'),

            ('preferred_language', mm_cfg.Select,
             (langs, langnames, langi),
             0,
             _('Default language for this list.'),
             _('''This is the default natural language for this mailing list.
             If <a href="?VARHELP=language/available_languages">more than one
             language</a> is supported then users will be able to select their
             own preferences for when they interact with the list.  All other
             interactions will be conducted in the default language.  This
             applies to both web-based and email-based messages, but not to
             email posted by list members.''')),

            ('available_languages', mm_cfg.Checkbox,
             (allnames, checked, 0, all), 0,
             _('Languages supported by this list.'),

             _('''These are all the natural languages supported by this list.
             Note that the
             <a href="?VARHELP=language/preferred_language">default
             language</a> must be included.''')),

            ('encode_ascii_prefixes', mm_cfg.Radio,
             (_('Never'), _('Always'), _('As needed')), 0,
             _("""Encode the
             <a href="?VARHELP=general/subject_prefix">subject
             prefix</a> even when it consists of only ASCII characters?"""),

             _("""If your mailing list's default language uses a non-ASCII
             character set and the prefix contains non-ASCII characters, the
             prefix will always be encoded according to the relevant
             standards.  However, if your prefix contains only ASCII
             characters, you may want to set this option to <em>Never</em> to
             disable prefix encoding.  This can make the subject headers
             slightly more readable for users with mail readers that don't
             properly handle non-ASCII encodings.

             <p>Note however, that if your mailing list receives both encoded
             and unencoded subject headers, you might want to choose <em>As
             needed</em>.  Using this setting, Mailman will not encode ASCII
             prefixes when the rest of the header contains only ASCII
             characters, but if the original header contains non-ASCII
             characters, it will encode the prefix.  This avoids an ambiguity
             in the standards which could cause some mail readers to display
             extra, or missing spaces between the prefix and the original
             header.""")),

            ]

    def _setValue(self, mlist, property, val, doc):
        # If we're changing the list's preferred language, change the I18N
        # context as well
        if property == 'preferred_language':
            i18n.set_language(val)
            doc.set_language(val)
        GUIBase._setValue(self, mlist, property, val, doc)
