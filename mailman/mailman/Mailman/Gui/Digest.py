# Copyright (C) 1998,1999,2000,2001,2002 by the Free Software Foundation, Inc.
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

"""Administrative GUI for digest deliveries."""

from Mailman import mm_cfg
from Mailman import Utils
from Mailman.i18n import _

# Intra-package import
from Mailman.Gui.GUIBase import GUIBase

# Common b/w nondigest and digest headers & footers.  Personalizations may add
# to this.
ALLOWEDS = ('real_name', 'list_name', 'host_name', 'web_page_url',
            'description', 'info', 'cgiext', '_internal_name',
            )



class Digest(GUIBase):
    def GetConfigCategory(self):
        return 'digest', _('Digest options')

    def GetConfigInfo(self, mlist, category, subcat=None):
        if category <> 'digest':
            return None
        WIDTH = mm_cfg.TEXTFIELDWIDTH

	info = [
            _("Batched-delivery digest characteristics."),

	    ('digestable', mm_cfg.Toggle, (_('No'), _('Yes')), 1,
	     _('Can list members choose to receive list traffic '
	       'bunched in digests?')),

	    ('digest_is_default', mm_cfg.Radio, 
	     (_('Regular'), _('Digest')), 0,
	     _('Which delivery mode is the default for new users?')),

	    ('mime_is_default_digest', mm_cfg.Radio, 
	     (_('Plain'), _('MIME')), 0,
	     _('When receiving digests, which format is default?')),

	    ('digest_size_threshhold', mm_cfg.Number, 3, 0,
	     _('How big in Kb should a digest be before it gets sent out?')),
            # Should offer a 'set to 0' for no size threshhold.

 	    ('digest_send_periodic', mm_cfg.Radio, (_('No'), _('Yes')), 1,
	     _('Should a digest be dispatched daily when the size threshold '
	       "isn't reached?")),

            ('digest_header', mm_cfg.Text, (4, WIDTH), 0,
	     _('Header added to every digest'),
             _("Text attached (as an initial message, before the table"
               " of contents) to the top of digests. ")
             + Utils.maketext('headfoot.html', raw=1, mlist=mlist)),

	    ('digest_footer', mm_cfg.Text, (4, WIDTH), 0,
	     _('Footer added to every digest'),
             _("Text attached (as a final message) to the bottom of digests. ")
             + Utils.maketext('headfoot.html', raw=1, mlist=mlist)),

            ('digest_volume_frequency', mm_cfg.Radio,
             (_('Yearly'), _('Monthly'), _('Quarterly'),
              _('Weekly'), _('Daily')), 0,
             _('How often should a new digest volume be started?'),
             _('''When a new digest volume is started, the volume number is
             incremented and the issue number is reset to 1.''')),

            ('_new_volume', mm_cfg.Toggle, (_('No'), _('Yes')), 0,
             _('Should Mailman start a new digest volume?'),
             _('''Setting this option instructs Mailman to start a new volume
             with the next digest sent out.''')),

            ('_send_digest_now', mm_cfg.Toggle, (_('No'), _('Yes')), 0,
             _('''Should Mailman send the next digest right now, if it is not
             empty?''')),
	    ]

##        if mm_cfg.OWNERS_CAN_ENABLE_PERSONALIZATION:
##            info.extend([
##                ('digest_personalize', mm_cfg.Toggle, (_('No'), _('Yes')), 1,

##                 _('''Should Mailman personalize each digest delivery?
##                 This is often useful for announce-only lists, but <a
##                 href="?VARHELP=digest/digest_personalize">read the details</a>
##                 section for a discussion of important performance
##                 issues.'''),

##                 _("""Normally, Mailman sends the digest messages to
##                 the mail server in batches.  This is much more efficent
##                 because it reduces the amount of traffic between Mailman and
##                 the mail server.

##                 <p>However, some lists can benefit from a more personalized
##                 approach.  In this case, Mailman crafts a new message for
##                 each member on the digest delivery list.  Turning this on
##                 adds a few more expansion variables that can be included in
##                 the <a href="?VARHELP=digest/digest_header">message header</a>
##                 and <a href="?VARHELP=digest/digest_footer">message footer</a>
##                 but it may degrade the performance of your site as
##                 a whole.

##                 <p>You need to carefully consider whether the trade-off is
##                 worth it, or whether there are other ways to accomplish what
##                 you want.  You should also carefully monitor your system load
##                 to make sure it is acceptable.

##                 <p>These additional substitution variables will be available
##                 for your headers and footers, when this feature is enabled:

##                 <ul><li><b>user_address</b> - The address of the user,
##                         coerced to lower case.
##                     <li><b>user_delivered_to</b> - The case-preserved address
##                         that the user is subscribed with.
##                     <li><b>user_password</b> - The user's password.
##                     <li><b>user_name</b> - The user's full name.
##                     <li><b>user_optionsurl</b> - The url to the user's option
##                         page.
##                 """))
##                ])

        return info

    def _setValue(self, mlist, property, val, doc):
        # Watch for the special, immediate action attributes
        if property == '_new_volume' and val:
            mlist.bump_digest_volume()
            volume = mlist.volume
            number = mlist.next_digest_number
            doc.AddItem(_("""The next digest will be sent as volume
            %(volume)s, number %(number)s"""))
        elif property == '_send_digest_now' and val:
            status = mlist.send_digest_now()
            if status:
                doc.AddItem(_("""A digest has been sent."""))
            else:
                doc.AddItem(_("""There was no digest to send."""))
        else:
            # Everything else...
            if property in ('digest_header', 'digest_footer'):
                val = self._convertString(mlist, property, ALLOWEDS, val, doc)
                if val is None:
                    # There was a problem, so don't set it
                    return
            GUIBase._setValue(self, mlist, property, val, doc)
