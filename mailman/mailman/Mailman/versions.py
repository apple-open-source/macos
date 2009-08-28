# Copyright (C) 1998-2007 by the Free Software Foundation, Inc.
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


"""Routines which rectify an old mailing list with current structure.

The MailList.CheckVersion() method looks for an old .data_version setting in
the loaded structure, and if found calls the Update() routine from this
module, supplying the list and the state last loaded from storage.  The state
is necessary to distinguish from default assignments done in the .InitVars()
methods, before .CheckVersion() is called.

For new versions you should add sections to the UpdateOldVars() and the
UpdateOldUsers() sections, to preserve the sense of settings across structural
changes.  Note that the routines have only one pass - when .CheckVersions()
finds a version change it runs this routine and then updates the data_version
number of the list, and then does a .Save(), so the transformations won't be
run again until another version change is detected.
"""


import email

from types import ListType, StringType

from Mailman import mm_cfg
from Mailman import Utils
from Mailman import Message
from Mailman.Bouncer import _BounceInfo
from Mailman.MemberAdaptor import UNKNOWN
from Mailman.Logging.Syslog import syslog



def Update(l, stored_state):
    "Dispose of old vars and user options, mapping to new ones when suitable."
    ZapOldVars(l)
    UpdateOldUsers(l)
    NewVars(l)
    UpdateOldVars(l, stored_state)
    CanonicalizeUserOptions(l)
    NewRequestsDatabase(l)



def ZapOldVars(mlist):
    for name in ('num_spawns', 'filter_prog', 'clobber_date',
                 'public_archive_file_dir', 'private_archive_file_dir',
                 'archive_directory',
                 # Pre-2.1a4 bounce data
                 'minimum_removal_date',
                 'minimum_post_count_before_bounce_action',
                 'automatic_bounce_action',
                 'max_posts_between_bounces',
                 ):
        if hasattr(mlist, name):
            delattr(mlist, name)



uniqueval = []
def UpdateOldVars(l, stored_state):
    """Transform old variable values into new ones, deleting old ones.
    stored_state is last snapshot from file, as opposed to from InitVars()."""

    def PreferStored(oldname, newname, newdefault=uniqueval,
                     l=l, state=stored_state):
        """Use specified old value if new value is not in stored state.

        If the old attr does not exist, and no newdefault is specified, the
        new attr is *not* created - so either specify a default or be positive
        that the old attr exists - or don't depend on the new attr.

        """
        if hasattr(l, oldname):
            if not state.has_key(newname):
                setattr(l, newname, getattr(l, oldname))
            delattr(l, oldname)
        if not hasattr(l, newname) and newdefault is not uniqueval:
                setattr(l, newname, newdefault)

    # Migrate to 2.1b3, baw 17-Aug-2001
    if hasattr(l, 'dont_respond_to_post_requests'):
        oldval = getattr(l, 'dont_respond_to_post_requests')
        if not hasattr(l, 'respond_to_post_requests'):
            l.respond_to_post_requests = not oldval
        del l.dont_respond_to_post_requests

    # Migrate to 2.1b3, baw 13-Oct-2001
    # Basic defaults for new variables
    if not hasattr(l, 'default_member_moderation'):
        l.default_member_moderation = mm_cfg.DEFAULT_DEFAULT_MEMBER_MODERATION
    if not hasattr(l, 'accept_these_nonmembers'):
        l.accept_these_nonmembers = []
    if not hasattr(l, 'hold_these_nonmembers'):
        l.hold_these_nonmembers = []
    if not hasattr(l, 'reject_these_nonmembers'):
        l.reject_these_nonmembers = []
    if not hasattr(l, 'discard_these_nonmembers'):
        l.discard_these_nonmembers = []
    if not hasattr(l, 'forward_auto_discards'):
        l.forward_auto_discards = mm_cfg.DEFAULT_FORWARD_AUTO_DISCARDS
    if not hasattr(l, 'generic_nonmember_action'):
        l.generic_nonmember_action = mm_cfg.DEFAULT_GENERIC_NONMEMBER_ACTION
    # Now convert what we can...  Note that the interaction between the
    # MM2.0.x attributes `moderated', `member_posting_only', and `posters' is
    # so confusing, it makes my brain really ache.  Which is why they go away
    # in MM2.1.  I think the best we can do semantically is the following:
    #
    # - If moderated == yes, then any sender who's address is not on the
    #   posters attribute would get held for approval.  If the sender was on
    #   the posters list, then we'd defer judgement to a later step
    # - If member_posting_only == yes, then members could post without holds,
    #   and if there were any addresses added to posters, they could also post
    #   without holds.
    # - If member_posting_only == no, then what happens depends on the value
    #   of the posters attribute:
    #       o If posters was empty, then anybody can post without their
    #         message being held for approval
    #       o If posters was non-empty, then /only/ those addresses could post
    #         without approval, i.e. members not on posters would have their
    #         messages held for approval.
    #
    # How to translate this mess to MM2.1 values?  I'm sure I got this wrong
    # before, but here's how we're going to do it, as of MM2.1b3.
    #
    # - We'll control member moderation through their Moderate flag, and
    #   non-member moderation through the generic_nonmember_action,
    #   hold_these_nonmembers, and accept_these_nonmembers.
    # - If moderated == yes then we need to troll through the addresses on
    #   posters, and any non-members would get added to
    #   accept_these_nonmembers.  /Then/ we need to troll through the
    #   membership and any member on posters would get their Moderate flag
    #   unset, while members not on posters would get their Moderate flag set.
    #   Then generic_nonmember_action gets set to 1 (hold) so nonmembers get
    #   moderated, and default_member_moderation will be set to 1 (hold) so
    #   new members will also get held for moderation.  We'll stop here.
    # - We only get to here if moderated == no.
    # - If member_posting_only == yes, then we'll turn off the Moderate flag
    #   for members.  We troll through the posters attribute and add all those
    #   addresses to accept_these_nonmembers.  We'll also set
    #   generic_nonmember_action to 1 and default_member_moderation to 0.
    #   We'll stop here.
    # - We only get to here if member_posting_only == no
    # - If posters is empty, then anybody could post without being held for
    #   approval, so we'll set generic_nonmember_action to 0 (accept), and
    #   we'll turn off the Moderate flag for all members.  We'll also turn off
    #   default_member_moderation so new members can post without approval.
    #   We'll stop here.
    # - We only get here if posters is non-empty.
    # - This means that /only/ the addresses on posters got to post without
    #   being held for approval.  So first, we troll through posters and add
    #   all non-members to accept_these_nonmembers.  Then we troll through the
    #   membership and if their address is on posters, we'll clear their
    #   Moderate flag, otherwise we'll set it.  We'll turn on
    #   default_member_moderation so new members get moderated.  We'll set
    #   generic_nonmember_action to 1 (hold) so all other non-members will get
    #   moderated.  And I think we're finally done.
    #
    # SIGH.
    if hasattr(l, 'moderated'):
        # We'll assume we're converting all these attributes at once
        if l.moderated:
            #syslog('debug', 'Case 1')
            for addr in l.posters:
                if not l.isMember(addr):
                    l.accept_these_nonmembers.append(addr)
            for member in l.getMembers():
                l.setMemberOption(member, mm_cfg.Moderate,
                                  # reset for explicitly named members
                                  member not in l.posters)
            l.generic_nonmember_action = 1
            l.default_member_moderation = 1
        elif l.member_posting_only:
            #syslog('debug', 'Case 2')
            for addr in l.posters:
                if not l.isMember(addr):
                    l.accept_these_nonmembers.append(addr)
            for member in l.getMembers():
                l.setMemberOption(member, mm_cfg.Moderate, 0)
            l.generic_nonmember_action = 1
            l.default_member_moderation = 0
        elif not l.posters:
            #syslog('debug', 'Case 3')
            for member in l.getMembers():
                l.setMemberOption(member, mm_cfg.Moderate, 0)
            l.generic_nonmember_action = 0
            l.default_member_moderation = 0
        else:
            #syslog('debug', 'Case 4')
            for addr in l.posters:
                if not l.isMember(addr):
                    l.accept_these_nonmembers.append(addr)
            for member in l.getMembers():
                l.setMemberOption(member, mm_cfg.Moderate,
                                  # reset for explicitly named members
                                  member not in l.posters)
            l.generic_nonmember_action = 1
            l.default_member_moderation = 1
        # Now get rid of the old attributes
        del l.moderated
        del l.posters
        del l.member_posting_only
    if hasattr(l, 'forbidden_posters'):
        # For each of the posters on this list, if they are members, toggle on
        # their moderation flag.  If they are not members, then add them to
        # hold_these_nonmembers.
        forbiddens = l.forbidden_posters
        for addr in forbiddens:
            if l.isMember(addr):
                l.setMemberOption(addr, mm_cfg.Moderate, 1)
            else:
                l.hold_these_nonmembers.append(addr)
        del l.forbidden_posters

    # Migrate to 1.0b6, klm 10/22/1998:
    PreferStored('reminders_to_admins', 'umbrella_list',
                 mm_cfg.DEFAULT_UMBRELLA_LIST)

    # Migrate up to 1.0b5:
    PreferStored('auto_subscribe', 'open_subscribe')
    PreferStored('closed', 'private_roster')
    PreferStored('mimimum_post_count_before_removal',
                 'mimimum_post_count_before_bounce_action')
    PreferStored('bad_posters', 'forbidden_posters')
    PreferStored('automatically_remove', 'automatic_bounce_action')
    if hasattr(l, "open_subscribe"):
        if l.open_subscribe:
            if mm_cfg.ALLOW_OPEN_SUBSCRIBE:
                l.subscribe_policy = 0
            else:
                l.subscribe_policy = 1
        else:
            l.subscribe_policy = 2      # admin approval
        delattr(l, "open_subscribe")
    if not hasattr(l, "administrivia"):
        setattr(l, "administrivia", mm_cfg.DEFAULT_ADMINISTRIVIA)
    if not hasattr(l, "admin_member_chunksize"):
        setattr(l, "admin_member_chunksize",
                mm_cfg.DEFAULT_ADMIN_MEMBER_CHUNKSIZE)
    #
    # this attribute was added then deleted, so there are a number of
    # cases to take care of
    #
    if hasattr(l, "posters_includes_members"):
        if l.posters_includes_members:
            if l.posters:
                l.member_posting_only = 1
        else:
            if l.posters:
                l.member_posting_only = 0
        delattr(l, "posters_includes_members")
    elif l.data_version <= 10 and l.posters:
        # make sure everyone gets the behavior the list used to have, but only
        # for really old versions of Mailman (1.0b5 or before).  Any newer
        # version of Mailman should not get this attribute whacked.
        l.member_posting_only = 0
    #
    # transfer the list data type for holding members and digest members
    # to the dict data type starting file format version 11
    #
    if type(l.members) is ListType:
        members = {}
        for m in l.members:
            members[m] = 1
        l.members = members
    if type(l.digest_members) is ListType:
        dmembers = {}
        for dm in l.digest_members:
            dmembers[dm] = 1
        l.digest_members = dmembers
    #
    # set admin_notify_mchanges
    #
    if not hasattr(l, "admin_notify_mchanges"):
        setattr(l, "admin_notify_mchanges",
                mm_cfg.DEFAULT_ADMIN_NOTIFY_MCHANGES)
    #
    # Convert the members and digest_members addresses so that the keys of
    # both these are always lowercased, but if there is a case difference, the
    # value contains the case preserved value
    #
    for k in l.members.keys():
        if k.lower() <> k:
            l.members[k.lower()] = Utils.LCDomain(k)
            del l.members[k]
        elif type(l.members[k]) == StringType and k == l.members[k].lower():
            # already converted
            pass
        else:
            l.members[k] = 0
    for k in l.digest_members.keys():
        if k.lower() <> k:
            l.digest_members[k.lower()] = Utils.LCDomain(k)
            del l.digest_members[k]
        elif type(l.digest_members[k]) == StringType and \
                 k == l.digest_members[k].lower():
            # already converted
            pass
        else:
            l.digest_members[k] = 0



def NewVars(l):
    """Add defaults for these new variables if they don't exist."""
    def add_only_if_missing(attr, initval, l=l):
        if not hasattr(l, attr):
            setattr(l, attr, initval)
    # 1.2 beta 1, baw 18-Feb-2000
    # Autoresponder mixin class attributes
    add_only_if_missing('autorespond_postings', 0)
    add_only_if_missing('autorespond_admin', 0)
    add_only_if_missing('autorespond_requests', 0)
    add_only_if_missing('autoresponse_postings_text', '')
    add_only_if_missing('autoresponse_admin_text', '')
    add_only_if_missing('autoresponse_request_text', '')
    add_only_if_missing('autoresponse_graceperiod', 90)
    add_only_if_missing('postings_responses', {})
    add_only_if_missing('admin_responses', {})
    add_only_if_missing('reply_goes_to_list', '')
    add_only_if_missing('preferred_language', mm_cfg.DEFAULT_SERVER_LANGUAGE)
    add_only_if_missing('available_languages', [])
    add_only_if_missing('digest_volume_frequency',
                        mm_cfg.DEFAULT_DIGEST_VOLUME_FREQUENCY)
    add_only_if_missing('digest_last_sent_at', 0)
    add_only_if_missing('mod_password', None)
    add_only_if_missing('moderator', [])
    add_only_if_missing('topics', [])
    add_only_if_missing('topics_enabled', 0)
    add_only_if_missing('topics_bodylines_limit', 5)
    add_only_if_missing('one_last_digest', {})
    add_only_if_missing('usernames', {})
    add_only_if_missing('personalize', 0)
    add_only_if_missing('first_strip_reply_to',
                        mm_cfg.DEFAULT_FIRST_STRIP_REPLY_TO)
    add_only_if_missing('unsubscribe_policy',
                        mm_cfg.DEFAULT_UNSUBSCRIBE_POLICY)
    add_only_if_missing('send_goodbye_msg', mm_cfg.DEFAULT_SEND_GOODBYE_MSG)
    add_only_if_missing('include_rfc2369_headers', 1)
    add_only_if_missing('include_list_post_header', 1)
    add_only_if_missing('bounce_score_threshold',
                        mm_cfg.DEFAULT_BOUNCE_SCORE_THRESHOLD)
    add_only_if_missing('bounce_info_stale_after',
                        mm_cfg.DEFAULT_BOUNCE_INFO_STALE_AFTER)
    add_only_if_missing('bounce_you_are_disabled_warnings',
                        mm_cfg.DEFAULT_BOUNCE_YOU_ARE_DISABLED_WARNINGS)
    add_only_if_missing(
        'bounce_you_are_disabled_warnings_interval',
        mm_cfg.DEFAULT_BOUNCE_YOU_ARE_DISABLED_WARNINGS_INTERVAL)
    add_only_if_missing(
        'bounce_unrecognized_goes_to_list_owner',
        mm_cfg.DEFAULT_BOUNCE_UNRECOGNIZED_GOES_TO_LIST_OWNER)
    add_only_if_missing(
        'bounce_notify_owner_on_disable',
        mm_cfg.DEFAULT_BOUNCE_NOTIFY_OWNER_ON_DISABLE)
    add_only_if_missing(
        'bounce_notify_owner_on_removal',
        mm_cfg.DEFAULT_BOUNCE_NOTIFY_OWNER_ON_REMOVAL)
    add_only_if_missing('ban_list', [])
    add_only_if_missing('filter_mime_types', mm_cfg.DEFAULT_FILTER_MIME_TYPES)
    add_only_if_missing('pass_mime_types', mm_cfg.DEFAULT_PASS_MIME_TYPES)
    add_only_if_missing('filter_content', mm_cfg.DEFAULT_FILTER_CONTENT)
    add_only_if_missing('convert_html_to_plaintext',
                        mm_cfg.DEFAULT_CONVERT_HTML_TO_PLAINTEXT)
    add_only_if_missing('filter_action', mm_cfg.DEFAULT_FILTER_ACTION)
    add_only_if_missing('delivery_status', {})
    # This really ought to default to mm_cfg.HOLD, but that doesn't work with
    # the current GUI description model.  So, 0==Hold, 1==Reject, 2==Discard
    add_only_if_missing('member_moderation_action', 0)
    add_only_if_missing('member_moderation_notice', '')
    add_only_if_missing('new_member_options',
                        mm_cfg.DEFAULT_NEW_MEMBER_OPTIONS)
    # Emergency moderation flag
    add_only_if_missing('emergency', 0)
    add_only_if_missing('hold_and_cmd_autoresponses', {})
    add_only_if_missing('news_prefix_subject_too', 1)
    # Should prefixes be encoded?
    if Utils.GetCharSet(l.preferred_language) == 'us-ascii':
        encode = 0
    else:
        encode = 2
    add_only_if_missing('encode_ascii_prefixes', encode)
    add_only_if_missing('news_moderation', 0)
    add_only_if_missing('header_filter_rules', [])
    # Scrubber in regular delivery
    add_only_if_missing('scrub_nondigest', 0)
    # ContentFilter by file extensions
    add_only_if_missing('filter_filename_extensions',
                        mm_cfg.DEFAULT_FILTER_FILENAME_EXTENSIONS)
    add_only_if_missing('pass_filename_extensions', [])
    # automatic discard
    add_only_if_missing('max_days_to_hold', 0)
    add_only_if_missing('nonmember_rejection_notice', '')
    # multipart/alternative collapse
    add_only_if_missing('collapse_alternatives',
                        mm_cfg.DEFAULT_COLLAPSE_ALTERNATIVES)
    # exclude/include lists
    add_only_if_missing('regular_exclude_lists',
                        mm_cfg.DEFAULT_REGULAR_EXCLUDE_LISTS)
    add_only_if_missing('regular_include_lists',
                        mm_cfg.DEFAULT_REGULAR_INCLUDE_LISTS)



def UpdateOldUsers(mlist):
    """Transform sense of changed user options."""
    # pre-1.0b11 to 1.0b11.  Force all keys in l.passwords to be lowercase
    passwords = {}
    for k, v in mlist.passwords.items():
        passwords[k.lower()] = v
    mlist.passwords = passwords
    # Go through all the keys in bounce_info.  If the key is not a member, or
    # if the data is not a _BounceInfo instance, chuck the bounce info.  We're
    # doing things differently now.
    for m in mlist.bounce_info.keys():
        if not mlist.isMember(m) or not isinstance(mlist.getBounceInfo(m),
                                                   _BounceInfo):
            del mlist.bounce_info[m]



def CanonicalizeUserOptions(l):
    """Fix up the user options."""
    # I want to put a flag in the list database which tells this routine to
    # never try to canonicalize the user options again.
    if getattr(l, 'useropts_version', 0) > 0:
        return
    # pre 1.0rc2 to 1.0rc3.  For all keys in l.user_options to be lowercase,
    # but merge options for both cases
    options = {}
    for k, v in l.user_options.items():
        if k is None:
            continue
        lcuser = k.lower()
        flags = 0
        if options.has_key(lcuser):
            flags = options[lcuser]
        flags |= v
        options[lcuser] = flags
    l.user_options = options
    # 2.1alpha3 -> 2.1alpha4.  The DisableDelivery flag is now moved into
    # get/setDeilveryStatus().  This must be done after the addresses are
    # canonicalized.
    for k, v in l.user_options.items():
        if not l.isMember(k):
            # There's a key in user_options that isn't associated with a real
            # member address.  This is likely caused by an earlier bug.
            del l.user_options[k]
            continue
        if l.getMemberOption(k, mm_cfg.DisableDelivery):
            # Convert this flag into a legacy disable
            l.setDeliveryStatus(k, UNKNOWN)
            l.setMemberOption(k, mm_cfg.DisableDelivery, 0)
    l.useropts_version = 1



def NewRequestsDatabase(l):
    """With version 1.2, we use a new pending request database schema."""
    r = getattr(l, 'requests', {})
    if not r:
        # no old-style requests
        return
    for k, v in r.items():
        if k == 'post':
            # This is a list of tuples with the following format
            #
            # a sequential request id integer
            # a timestamp float
            # a message tuple: (author-email-str, message-text-str)
            # a reason string
            # the subject string
            #
            # We'll re-submit this as a new HoldMessage request, but we'll
            # blow away the original timestamp and request id.  This means the
            # request will live a little longer than it possibly should have,
            # but that's no big deal.
            for p in v:
                author, text = p[2]
                reason = p[3]
                msg = email.message_from_string(text, Message.Message)
                l.HoldMessage(msg, reason)
            del r[k]
        elif k == 'add_member':
            # This is a list of tuples with the following format
            #
            # a sequential request id integer
            # a timestamp float
            # a digest flag (0 == nodigest, 1 == digest)
            # author-email-str
            # password
            #
            # See the note above; the same holds true.
            for ign, ign, digest, addr, password in v:
                l.HoldSubscription(addr, '', password, digest,
                                   mm_cfg.DEFAULT_SERVER_LANGUAGE)
            del r[k]
        else:
            syslog('error', """\
VERY BAD NEWS.  Unknown pending request type `%s' found for list: %s""",
                   k, l.internal_name())
