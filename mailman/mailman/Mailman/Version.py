# Copyright (C) 1998-2006 by the Free Software Foundation, Inc.
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

# Mailman version
VERSION = '2.1.12rc1'

# And as a hex number in the manner of PY_VERSION_HEX
ALPHA = 0xa
BETA  = 0xb
GAMMA = 0xc
# release candidates
RC    = GAMMA
FINAL = 0xf

MAJOR_REV = 2
MINOR_REV = 1
MICRO_REV = 12
REL_LEVEL = GAMMA
# at most 15 beta releases!
REL_SERIAL = 1

HEX_VERSION = ((MAJOR_REV << 24) | (MINOR_REV << 16) | (MICRO_REV << 8) |
               (REL_LEVEL << 4)  | (REL_SERIAL << 0))

# config.pck schema version number
DATA_FILE_VERSION = 97

# qfile/*.db schema version number
QFILE_SCHEMA_VERSION = 3

# version number for the lists/<listname>/pending.db file schema
PENDING_FILE_SCHEMA_VERSION = 2

# version number for the lists/<listname>/request.db file schema
REQUESTS_FILE_SCHEMA_VERSION = 1
