#! @PYTHON@
#
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

"""Reset a list's web_page_url attribute to the default setting.

This script is intended to be run as a bin/withlist script, i.e.

% bin/withlist -l -r fix_url listname [options]

Options:
    -u urlhost
    --urlhost=urlhost
        Look up urlhost in the virtual host table and set the web_page_url and
        host_name attributes of the list to the values found.  This
        essentially moves the list from one virtual domain to another.

        Without this option, the default web_page_url and host_name values are
        used.

    -v / --verbose
        Print what the script is doing.

If run standalone, it prints this help text and exits.
"""

import sys
import getopt

import paths
from Mailman import mm_cfg
from Mailman.i18n import _



def usage(code, msg=''):
    print _(__doc__.replace('%', '%%'))
    if msg:
        print msg
    sys.exit(code)



def fix_url(mlist, *args):
    try:
        opts, args = getopt.getopt(args, 'u:v', ['urlhost=', 'verbose'])
    except getopt.error, msg:
        usage(1, msg)

    verbose = 0
    urlhost = mailhost = None
    for opt, arg in opts:
        if opt in ('-u', '--urlhost'):
            urlhost = arg
        elif opt in ('-v', '--verbose'):
            verbose = 1

    if urlhost:
        web_page_url = mm_cfg.DEFAULT_URL_PATTERN % urlhost
        mailhost = mm_cfg.VIRTUAL_HOSTS.get(urlhost.lower(), urlhost)
    else:
        web_page_url = mm_cfg.DEFAULT_URL_PATTERN % mm_cfg.DEFAULT_URL_HOST
        mailhost = mm_cfg.DEFAULT_EMAIL_HOST

    if verbose:
        print _('Setting web_page_url to: %(web_page_url)s')
    mlist.web_page_url = web_page_url
    if verbose:
        print _('Setting host_name to: %(mailhost)s')
    mlist.host_name = mailhost
    print _('Saving list')
    mlist.Save()
    mlist.Unlock()



if __name__ == '__main__':
    usage(0)
