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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.


"""Mixin class with list-digest handling methods and settings."""

import os
from stat import ST_SIZE
import errno

from Mailman import mm_cfg
from Mailman import Utils
from Mailman import Errors
from Mailman.Handlers import ToDigest
from Mailman.i18n import _



class Digester:
    def InitVars(self):
	# Configurable
	self.digestable = mm_cfg.DEFAULT_DIGESTABLE
	self.digest_is_default = mm_cfg.DEFAULT_DIGEST_IS_DEFAULT
	self.mime_is_default_digest = mm_cfg.DEFAULT_MIME_IS_DEFAULT_DIGEST
	self.digest_size_threshhold = mm_cfg.DEFAULT_DIGEST_SIZE_THRESHHOLD
	self.digest_send_periodic = mm_cfg.DEFAULT_DIGEST_SEND_PERIODIC
	self.next_post_number = 1
	self.digest_header = mm_cfg.DEFAULT_DIGEST_HEADER
	self.digest_footer = mm_cfg.DEFAULT_DIGEST_FOOTER
        self.digest_volume_frequency = mm_cfg.DEFAULT_DIGEST_VOLUME_FREQUENCY
	# Non-configurable.
        self.one_last_digest = {}
	self.digest_members = {}
	self.next_digest_number = 1
        self.digest_last_sent_at = 0

    def send_digest_now(self):
        # Note: Handler.ToDigest.send_digests() handles bumping the digest
        # volume and issue number.
        digestmbox = os.path.join(self.fullpath(), 'digest.mbox')
        try:
            try:
                mboxfp = None
                # See if there's a digest pending for this mailing list
                if os.stat(digestmbox)[ST_SIZE] > 0:
                    mboxfp = open(digestmbox)
                    ToDigest.send_digests(self, mboxfp)
                    os.unlink(digestmbox)
            finally:
                if mboxfp:
                    mboxfp.close()
        except OSError, e:
            if e.errno <> errno.ENOENT: raise
            # List has no outstanding digests
            return 0
        return 1

    def bump_digest_volume(self):
        self.volume += 1
        self.next_digest_number = 1
