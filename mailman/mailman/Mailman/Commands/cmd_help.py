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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

"""
    help
        Print this help message.
"""

import sys
import os

from Mailman import mm_cfg
from Mailman import Utils
from Mailman.i18n import _

EMPTYSTRING = ''



def gethelp(mlist):
    return _(__doc__)



def process(res, args):
    # Get the help text introduction
    mlist = res.mlist
    # Since this message is personalized, add some useful information if the
    # address requesting help is a member of the list.
    msg = res.msg
    for sender in  msg.get_senders():
        if mlist.isMember(sender):
            memberurl = mlist.GetOptionsURL(sender, absolute=1)
            urlhelp = _(
                'You can access your personal options via the following url:')
            res.results.append(urlhelp)
            res.results.append(memberurl)
            # Get a blank line in the output.
            res.results.append('')
            break
    # build the specific command helps from the module docstrings
    modhelps = {}
    import Mailman.Commands
    path = os.path.dirname(os.path.abspath(Mailman.Commands.__file__))
    for file in os.listdir(path):
        if not file.startswith('cmd_') or not file.endswith('.py'):
            continue
        module = os.path.splitext(file)[0]
        modname = 'Mailman.Commands.' + module
        try:
            __import__(modname)
        except ImportError:
            continue
        cmdname = module[4:]
        help = None
        if hasattr(sys.modules[modname], 'gethelp'):
            help = sys.modules[modname].gethelp(mlist)
        if help:
            modhelps[cmdname] = help
    # Now sort the command helps
    helptext = []
    keys = modhelps.keys()
    keys.sort()
    for cmd in keys:
        helptext.append(modhelps[cmd])
    commands = EMPTYSTRING.join(helptext)
    # Now craft the response
    helptext = Utils.maketext(
        'help.txt',
        {'listname'    : mlist.real_name,
         'version'     : mm_cfg.VERSION,
         'listinfo_url': mlist.GetScriptURL('listinfo', absolute=1),
         'requestaddr' : mlist.GetRequestEmail(),
         'adminaddr'   : mlist.GetOwnerEmail(),
         'commands'    : commands,
         }, mlist=mlist, lang=res.msgdata['lang'], raw=1)
    # Now add to the response
    res.results.append('help')
    res.results.append(helptext)
