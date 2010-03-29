# Copyright (C) 2002-2003 by the Free Software Foundation, Inc.
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

"""Provide some customization for site-wide behavior.

This should be considered experimental for Mailman 2.1.  The default
implementation should work for standard Mailman.
"""

import os
import errno

from Mailman import mm_cfg

try:
    True, False
except NameError:
    True = 1
    False = 0



def _makedir(path):
    try:
        omask = os.umask(0)
        try:
            os.makedirs(path, 02775)
        finally:
            os.umask(omask)
    except OSError, e:
        # Ignore the exceptions if the directory already exists
        if e.errno <> errno.EEXIST:
            raise



# BAW: We don't really support domain<>None yet.  This will be added in a
# future version.  By default, Mailman will never pass in a domain argument.
def get_listpath(listname, domain=None, create=0):
    """Return the file system path to the list directory for the named list.

    If domain is given, it is the virtual domain for the named list.  The
    default is to not distinguish list paths on the basis of virtual domains.

    If the create flag is true, then this method should create the path
    hierarchy if necessary.  If the create flag is false, then this function
    should not attempt to create the path heirarchy (and in fact the absence
    of the path might be significant).
    """
    path = os.path.join(mm_cfg.LIST_DATA_DIR, listname)
    if create:
        _makedir(path)
    return path



# BAW: We don't really support domain<>None yet.  This will be added in a
# future version.  By default, Mailman will never pass in a domain argument.
def get_archpath(listname, domain=None, create=False, public=False):
    """Return the file system path to the list's archive directory for the
    named list in the named virtual domain.

    If domain is given, it is the virtual domain for the named list.  The
    default is to not distinguish list paths on the basis of virtual domains.

    If the create flag is true, then this method should create the path
    hierarchy if necessary.  If the create flag is false, then this function
    should not attempt to create the path heirarchy (and in fact the absence
    of the path might be significant).

    If public is true, then the path points to the public archive path (which
    is usually a symlink instead of a directory).
    """
    if public:
        subdir = mm_cfg.PUBLIC_ARCHIVE_FILE_DIR
    else:
        subdir = mm_cfg.PRIVATE_ARCHIVE_FILE_DIR
    path = os.path.join(subdir, listname)
    if create:
        _makedir(path)
    return path



# BAW: We don't really support domain<>None yet.  This will be added in a
# future version.  By default, Mailman will never pass in a domain argument.
def get_listnames(domain=None):
    """Return the names of all the known lists for the given domain.

    If domain is given, it is the virtual domain for the named list.  The
    default is to not distinguish list paths on the basis of virtual domains.
    """
    # Import this here to avoid circular imports
    from Mailman.Utils import list_exists
    # We don't currently support separate virtual domain directories
    got = []
    for fn in os.listdir(mm_cfg.LIST_DATA_DIR):
        if list_exists(fn):
            got.append(fn)
    return got
