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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

"""This class mixes in topic feature configuration for mailing lists.
"""

import re

from Mailman import mm_cfg
from Mailman.i18n import _



class TopicMgr:
    def InitVars(self):
        # Configurable
        #
        # `topics' is a list of 4-tuples of the following form:
        #
        #     (name, pattern, description, emptyflag)
        #
        # name is a required arbitrary string displayed to the user when they
        # get to select their topics of interest
        #
        # pattern is a required verbose regular expression pattern which is
        # used as IGNORECASE.
        #
        # description is an optional description of what this topic is
        # supposed to match
        #
        # emptyflag is a boolean used internally in the admin interface to
        # signal whether a topic entry is new or not (new ones which do not
        # have a name or pattern are not saved when the submit button is
        # pressed).
        self.topics = []
        self.topics_enabled = 0
        self.topics_bodylines_limit = 5
        # Non-configurable
        #
        # This is a mapping between user "names" (i.e. addresses) and
        # information about which topics that user is interested in.  The
        # values are a list of topic names that the user is interested in,
        # which should match the topic names in self.topics above.
        #
        # If the user has not selected any topics of interest, then the rule
        # is that they will get all messages, and they will not have an entry
        # in this dictionary.
        self.topics_userinterest = {}
