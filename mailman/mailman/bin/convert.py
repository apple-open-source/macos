#! @PYTHON@
#
# Copyright (C) 2002 by the Free Software Foundation, Inc.
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

"""Convert a list's interpolation strings from %-strings to $-strings.

This script is intended to be run as a bin/withlist script, i.e.

% bin/withlist -l -r convert <mylist>
"""

import paths
from Mailman import Utils
from Mailman.i18n import _

def convert(mlist):
    for attr in ('msg_header', 'msg_footer', 'digest_header', 'digest_footer',
                 'autoresponse_postings_text', 'autoresponse_admin_text',
                 'autoresponse_request_text'):
        s = getattr(mlist, attr)
        t = Utils.to_dollar(s)
        setattr(mlist, attr, t)
    mlist.use_dollar_strings = 1
    print _('Saving list')
    mlist.Save()



if __name__ == '__main__':
    print _(__doc__.replace('%', '%%'))
