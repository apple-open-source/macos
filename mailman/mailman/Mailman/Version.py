# Copyright (C) 1998-2003 by the Free Software Foundation, Inc.
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

# Mailman version
VERSION = "2.1.2"

# And as a hex number in the manner of PY_VERSION_HEX
ALPHA = 0xa
BETA  = 0xb
GAMMA = 0xc
# release candidates
RC    = GAMMA
FINAL = 0xf

MAJOR_REV = 2
MINOR_REV = 1
MICRO_REV = 2
REL_LEVEL = FINAL
# at most 15 beta releases!
REL_SERIAL = 0

HEX_VERSION = ((MAJOR_REV << 24) | (MINOR_REV << 16) | (MICRO_REV << 8) |
               (REL_LEVEL << 4)  | (REL_SERIAL << 0))

# config.pck schema version number
DATA_FILE_VERSION = 88

# qfile/*.db schema version number
QFILE_SCHEMA_VERSION = 3

# version number for the data/pending.db file schema
PENDING_FILE_SCHEMA_VERSION = 1

# version number for the lists/<listname>/request.db file schema
REQUESTS_FILE_SCHEMA_VERSION = 1
