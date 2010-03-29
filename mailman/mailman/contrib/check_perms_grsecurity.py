#! @PYTHON@
#
# Copyright (C) 1998-2009 by the Free Software Foundation, Inc.
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

"""Fixes for running Mailman under the `secure-linux' patch or grsecurity.

Run check_perms -f and only then check_perms_grsecurity.py -f
Note that you  will have to re-run  this script after a  mailman upgrade and
that check_perms will undo part of what this script does

If you use  Solar Designer's secure-linux patch, it prevents  a process from
linking (hard link) to a file it doesn't own.
Grsecurity (http://grsecurity.net/) can have  the same restriction depending
on how it was built, including other restrictions like preventing you to run
a program if it is located in a directory writable by a non root user.

As a  result Mailman has to  be changed so that  the whole tree is  owned by
Mailman, and  the CGIs and some  of the programs  in the bin tree  (the ones
that lock config.pck  files) are SUID Mailman.  The idea  is that config.pck
files have to be owned by the  mailman UID and only touched by programs that
are UID mailman.
At the  same time, We have  to make sure  that at least 3  directories under
~mailman aren't writable by mailman: mail, cgi-bin, and bin

Binary commands that are changed to be SUID mailman are also made unreadable
and unrunnable  by people who aren't  in the mailman group.   This shouldn't
affect much since most of those commands would fail work if you weren't part
of the mailman group anyway.
Scripts in ~mailman/bin/ are  not made suid or sgid, they need  to be run by
user mailman or root to work.

Marc <marc_soft@merlins.org>/<marc_bts@vasoftware.com>
2000/10/27 - Initial version for secure_linux/openwall and mailman 2.0
2001/12/09 - Updated version for grsecurity and mailman 2.1
"""

import sys
import os
import paths
import re
import glob
import pwd
import grp
from Mailman import mm_cfg
from Mailman.mm_cfg import MAILMAN_USER, MAILMAN_GROUP
from stat import *

# Directories that we don't want writable by mailman.
dirstochownroot= ( 'mail', 'cgi-bin', 'bin' )

# Those are the programs that we patch so that they insist being run under the
# mailman uid or as root.
binfilestopatch= ( 'add_members', 'change_pw', 'check_db', 'clone_member',
        'config_list', 'newlist', 'qrunner', 'remove_members',
        'rmlist', 'sync_members', 'update', 'withlist' )

def main(argv):
    binpath = paths.prefix + '/bin/'
    droplib = binpath + 'CheckFixUid.py'

    if len(argv) < 2 or argv[1] != "-f":
        print __doc__
        sys.exit(1)

    print "Making select directories owned and writable by root only"
    gid = grp.getgrnam(MAILMAN_GROUP)[2]
    for dir in dirstochownroot:
        dirpath = paths.prefix + '/' + dir
        os.chown(dirpath, 0, gid)
        os.chmod(dirpath, 02755)
        print dirpath

    print

    file = mm_cfg.VAR_PREFIX + '/data/last_mailman_version'
    print "Making" + file + "owned by mailman (not root)"
    uid = pwd.getpwnam(MAILMAN_USER)[2]
    gid = grp.getgrnam(MAILMAN_GROUP)[2]
    os.chown(file, uid, gid)
    print

    if not os.path.exists(droplib):
        print "Creating " + droplib
        fp = open(droplib, 'w', 0644)
        fp.write("""import sys
import os
import grp, pwd
from Mailman.mm_cfg import MAILMAN_USER, MAILMAN_GROUP

class CheckFixUid:
    uid = pwd.getpwnam(MAILMAN_USER)[2]
    gid = grp.getgrnam(MAILMAN_GROUP)[2]
    if os.geteuid() == 0:
        os.setgid(gid)
        os.setuid(uid)
    if os.geteuid() != uid:
        print "You need to run this script as root or mailman because it was configured to run"
        print "on a linux system with a security patch which restricts hard links"
        sys.exit()
""")
        fp.close()
    else:
        print "Skipping creation of " + droplib


    print "\nMaking cgis setuid mailman"
    cgis = glob.glob(paths.prefix + '/cgi-bin/*')

    for file in cgis:
        print file
        os.chown(file, uid, gid)
        os.chmod(file, 06755)

    print "\nMaking mail wrapper setuid mailman"
    file= paths.prefix + '/mail/mailman'
    os.chown(file, uid, gid)
    os.chmod(file, 06755)
    print file

    print "\nEnsuring that all config.db/pck files are owned by Mailman"
    cdbs = glob.glob(mm_cfg.VAR_PREFIX + '/lists/*/config.db*')
    cpcks = glob.glob(mm_cfg.VAR_PREFIX + '/lists/*/config.pck*')

    for file in cdbs + cpcks:
        stat = os.stat(file)
        if (stat[ST_UID] != uid or stat[ST_GID] != gid):
            print file
            os.chown(file, uid, gid)

    print "\nPatching mailman scripts to change the uid to mailman"

    for script in binfilestopatch:
        filefd = open(script, "r")
        file = filefd.readlines()
        filefd.close()

        patched = 0
        try:
            file.index("import CheckFixUid\n")
            print "Not patching " + script + ", already patched"
        except ValueError:
            file.insert(file.index("import paths\n")+1, "import CheckFixUid\n")
            for i in range(len(file)-1, 0, -1):
                object=re.compile("^([   ]*)main\(").search(file[i])
                # Special hack to support patching of update
                object2=re.compile("^([     ]*).*=[      ]*main\(").search(file[i])
                if object:
                    print "Patching " + script
                    file.insert(i,
                        object.group(1) + "CheckFixUid.CheckFixUid()\n")
                    patched=1
                    break
                if object2:
                    print "Patching " + script
                    file.insert(i,
                        object2.group(1) + "CheckFixUid.CheckFixUid()\n")
                    patched=1
                    break

            if patched==0:
                print "Warning, file "+script+" couldn't be patched."
                print "If you use it, mailman may not function properly"
            else:
                filefd=open(script, "w")
                filefd.writelines(file)

main(sys.argv)
