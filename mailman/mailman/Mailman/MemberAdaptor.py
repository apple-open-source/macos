# Copyright (C) 2001-2003 by the Free Software Foundation, Inc.
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

"""This is an interface to list-specific membership information.

This class should not be instantiated directly, but instead, it should be
subclassed for specific adaptation to membership databases.  The default
MM2.0.x style adaptor is in OldStyleMemberships.py.  Through the extend.py
mechanism, you can instantiate different membership information adaptors to
get info out of LDAP, Zope, other, or any combination of the above.

Members have three pieces of identifying information: a unique identifying
opaque key (KEY), a lower-cased email address (LCE), and a case-preserved
email (CPE) address.  Adaptors must ensure that both member keys and lces can
uniquely identify a member, and that they can (usually) convert freely between
keys and lces.  Most methods must accept either a key or an lce, unless
specifically documented otherwise.

The CPE is always used to calculate the recipient address for a message.  Some
remote MTAs make a distinction based on localpart case, so we always send
messages to the case-preserved address.  Note that DNS is case insensitive so
it doesn't matter what the case is for the domain part of an email address,
although by default, we case-preserve that too.

The adaptors must support the readable interface for getting information about
memberships, and may optionally support the writeable interface.  If they do
not, then members cannot change their list attributes via Mailman's web or
email interfaces.  Updating membership information in that case is the
backend's responsibility.  Adaptors are allowed to support parts of the
writeable interface.

For any writeable method not supported, a NotImplementedError exception should
be raised.
"""

# Delivery statuses
ENABLED  = 0                                      # enabled
UNKNOWN  = 1                                      # legacy disabled
BYUSER   = 2                                      # disabled by user choice
BYADMIN  = 3                                      # disabled by admin choice
BYBOUNCE = 4                                      # disabled by bounces



class MemberAdaptor:
    #
    # The readable interface
    #
    def getMembers(self):
        """Get the LCE for all the members of the mailing list."""
        raise NotImplementedError

    def getRegularMemberKeys(self):
        """Get the LCE for all regular delivery members (i.e. non-digest)."""
        raise NotImplementedError

    def getDigestMemberKeys(self):
        """Get the LCE for all digest delivery members."""
        raise NotImplementedError

    def isMember(self, member):
        """Return 1 if member KEY/LCE is a valid member, otherwise 0."""

    def getMemberKey(self, member):
        """Return the KEY for the member KEY/LCE.

        If member does not refer to a valid member, raise NotAMemberError.
        """
        raise NotImplementedError

    def getMemberCPAddress(self, member):
        """Return the CPE for the member KEY/LCE.

        If member does not refer to a valid member, raise NotAMemberError.
        """
        raise NotImplementedError

    def getMemberCPAddresses(self, members):
        """Return a sequence of CPEs for the given sequence of members.

        The returned sequence will be the same length as members.  If any of
        the KEY/LCEs in members does not refer to a valid member, that entry
        in the returned sequence will be None (i.e. NotAMemberError is never
        raised).
        """
        raise NotImplementedError

    def authenticateMember(self, member, response):
        """Authenticate the member KEY/LCE with the given response.

        If the response authenticates the member, return a secret that is
        known only to the authenticated member.  This need not be the member's
        password, but it will be used to craft a session cookie, so it should
        be persistent for the life of the session.

        If the authentication failed return False.  If member did not refer to
        a valid member, raise NotAMemberError.

        Normally, the response will be the password typed into a web form or
        given in an email command, but it needn't be.  It is up to the adaptor
        to compare the typed response to the user's authentication token.
        """
        raise NotImplementedError

    def getMemberPassword(self, member):
        """Return the member's password.

        If the member KEY/LCE is not a member of the list, raise
        NotAMemberError.
        """
        raise NotImplementedError

    def getMemberLanguage(self, member):
        """Return the preferred language for the member KEY/LCE.

        The language returned must be a key in mm_cfg.LC_DESCRIPTIONS and the
        mailing list must support that language.

        If member does not refer to a valid member, the list's default
        language is returned instead of raising a NotAMemberError error.
        """
        raise NotImplementedError

    def getMemberOption(self, member, flag):
        """Return the boolean state of the member option for member KEY/LCE.

        Option flags are defined in Defaults.py.

        If member does not refer to a valid member, raise NotAMemberError.
        """
        raise NotImplementedError

    def getMemberName(self, member):
        """Return the full name of the member KEY/LCE.

        None is returned if the member has no registered full name.  The
        returned value may be a Unicode string if there are non-ASCII
        characters in the name.  NotAMemberError is raised if member does not
        refer to a valid member.
        """
        raise NotImplementedError

    def getMemberTopics(self, member):
        """Return the list of topics this member is interested in.

        The return value is a list of strings which name the topics.
        """
        raise NotImplementedError

    def getDeliveryStatus(self, member):
        """Return the delivery status of this member.

        Value is one of the module constants:

            ENABLED  - The deliveries to the user are not disabled
            UNKNOWN  - Deliveries are disabled for unknown reasons.  The
                       primary reason for this to happen is that we've copied
                       their delivery status from a legacy version which didn't
                       keep track of disable reasons
            BYUSER   - The user explicitly disable deliveries
            BYADMIN  - The list administrator explicitly disabled deliveries
            BYBOUNCE - The system disabled deliveries due to bouncing

        If member is not a member of the list, raise NotAMemberError.
        """
        raise NotImplementedError

    def getDeliveryStatusChangeTime(self, member):
        """Return the time of the last disabled delivery status change.

        If the current delivery status is ENABLED, the status change time will
        be zero.  If member is not a member of the list, raise
        NotAMemberError.
        """
        raise NotImplementedError

    def getDeliveryStatusMembers(self,
                                 status=(UNKNOWN, BYUSER, BYADMIN, BYBOUNCE)):
        """Return the list of members with a matching delivery status.

        Optional `status' if given, must be a sequence containing one or more
        of ENABLED, UNKNOWN, BYUSER, BYADMIN, or BYBOUNCE.  The members whose
        delivery status is in this sequence are returned.
        """
        raise NotImplementedError

    def getBouncingMembers(self):
        """Return the list of members who have outstanding bounce information.

        This list of members doesn't necessarily overlap with
        getDeliveryStatusMembers() since getBouncingMembers() will return
        member who have bounced but not yet reached the disable threshold.
        """
        raise NotImplementedError

    def getBounceInfo(self, member):
        """Return the member's bounce information.

        A value of None means there is no bounce information registered for
        the member.

        Bounce info is opaque to the MemberAdaptor.  It is set by
        setBounceInfo() and returned by this method without modification.

        If member is not a member of the list, raise NotAMemberError.
        """
        raise NotImplementedError


    #
    # The writeable interface
    #
    def addNewMember(self, member, **kws):
        """Subscribes a new member to the mailing list.

        member is the case-preserved address to subscribe.  The LCE is
        calculated from this argument.  Return the new member KEY.

        This method also takes a keyword dictionary which can be used to set
        additional attributes on the member.  The actual set of supported
        keywords is adaptor specific, but should at least include:

        - digest == subscribing to digests instead of regular delivery
        - password == user's password
        - language == user's preferred language
        - realname == user's full name (should be Unicode if there are
          non-ASCII characters in the name)

        Any values not passed to **kws is set to the adaptor-specific
        defaults.

        Raise AlreadyAMemberError it the member is already subscribed to the
        list.  Raises ValueError if **kws contains an invalid option.
        """
        raise NotImplementedError

    def removeMember(self, memberkey):
        """Unsubscribes the member from the mailing list.

        Raise NotAMemberError if member is not subscribed to the list.
        """
        raise NotImplementedError

    def changeMemberAddress(self, memberkey, newaddress, nodelete=0):
        """Change the address for the member KEY.

        memberkey will be a KEY, not an LCE.  newaddress should be the
        new case-preserved address for the member; the LCE will be calculated
        from newaddress.

        If memberkey does not refer to a valid member, raise NotAMemberError.
        No verification on the new address is done here (such assertions
        should be performed by the caller).

        If nodelete flag is true, then the old membership is not removed.
        """
        raise NotImplementedError

    def setMemberPassword(self, member, password):
        """Set the password for member LCE/KEY.

        If member does not refer to a valid member, raise NotAMemberError.
        Also raise BadPasswordError if the password is illegal (e.g. too
        short or easily guessed via a dictionary attack).
        """
        raise NotImplementedError

    def setMemberLanguage(self, member, language):
        """Set the language for the member LCE/KEY.

        If member does not refer to a valid member, raise NotAMemberError.
        Also raise BadLanguageError if the language is invalid (e.g. the list
        is not configured to support the given language).
        """
        raise NotImplementedError

    def setMemberOption(self, member, flag, value):
        """Set the option for the given member to value.

        member is an LCE/KEY, flag is one of the option flags defined in
        Default.py, and value is a boolean.

        If member does not refer to a valid member, raise NotAMemberError.
        Also raise BadOptionError if the flag does not refer to a valid
        option.
        """
        raise NotImplementedError

    def setMemberName(self, member, realname):
        """Set the member's full name.

        member is an LCE/KEY and realname is an arbitrary string.  It should
        be a Unicode string if there are non-ASCII characters in the name.
        NotAMemberError is raised if member does not refer to a valid member.
        """
        raise NotImplementedError

    def setMemberTopics(self, member, topics):
        """Add list of topics to member's interest.

        member is an LCE/KEY and realname is an arbitrary string.
        NotAMemberError is raised if member does not refer to a valid member.
        topics must be a sequence of strings.
        """
        raise NotImplementedError

    def setDeliveryStatus(self, member, status):
        """Set the delivery status of the member's address.

        Status must be one of the module constants:

            ENABLED  - The deliveries to the user are not disabled
            UNKNOWN  - Deliveries are disabled for unknown reasons.  The
                       primary reason for this to happen is that we've copied
                       their delivery status from a legacy version which didn't
                       keep track of disable reasons
            BYUSER   - The user explicitly disable deliveries
            BYADMIN  - The list administrator explicitly disabled deliveries
            BYBOUNCE - The system disabled deliveries due to bouncing

        This method also records the time (in seconds since epoch) at which
        the last status change was made.  If the delivery status is changed to
        ENABLED, then the change time information will be deleted.  This value
        is retrievable via getDeliveryStatusChangeTime().
        """
        raise NotImplementedError

    def setBounceInfo(self, member, info):
        """Set the member's bounce information.

        When info is None, any bounce info for the member is cleared.

        Bounce info is opaque to the MemberAdaptor.  It is set by this method
        and returned by getBounceInfo() without modification.
        """
        raise NotImplementedError
