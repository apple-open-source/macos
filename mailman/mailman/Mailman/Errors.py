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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.


"""Shared mailman errors and messages."""


# exceptions for problems related to opening a list
class MMListError(Exception): pass
class MMUnknownListError(MMListError): pass
class MMCorruptListDatabaseError(MMListError): pass
class MMListNotReadyError(MMListError): pass
class MMListAlreadyExistsError(MMListError): pass
class BadListNameError(MMListError): pass

# Membership exceptions
class MMMemberError(Exception): pass
class MMBadUserError(MMMemberError): pass
class MMAlreadyAMember(MMMemberError): pass

# "New" style membership exceptions (new w/ MM2.1)
class MemberError(Exception): pass
class NotAMemberError(MemberError): pass
class AlreadyReceivingDigests(MemberError): pass
class AlreadyReceivingRegularDeliveries(MemberError): pass
class CantDigestError(MemberError): pass
class MustDigestError(MemberError): pass
class MembershipIsBanned(MemberError): pass

# Exception hierarchy for various authentication failures, can be
# raised from functions in SecurityManager.py
class MMAuthenticationError(Exception): pass
class MMBadPasswordError(MMAuthenticationError): pass
class MMPasswordsMustMatch(MMAuthenticationError): pass
class MMCookieError(MMAuthenticationError): pass
class MMExpiredCookieError(MMCookieError): pass
class MMInvalidCookieError(MMCookieError): pass

class MMMustDigestError: pass
class MMCantDigestError: pass
class MMNeedApproval: pass
class MMSubscribeNeedsConfirmation: pass
class MMBadConfirmation: pass
class MMAlreadyDigested: pass
class MMAlreadyUndigested: pass

MODERATED_LIST_MSG    = "Moderated list"
IMPLICIT_DEST_MSG     = "Implicit destination"
SUSPICIOUS_HEADER_MSG = "Suspicious header"
FORBIDDEN_SENDER_MSG  = "Forbidden sender"



# New style class based exceptions.  All the above errors should eventually be
# converted.

class MailmanError(Exception):
    """Base class for all Mailman exceptions."""
    pass


class MMLoopingPost(MailmanError):
    """Post already went through this list!"""
    pass


# Exception hierarchy for bad email address errors that can be raised from
# Utils.ValidateEmail()
class EmailAddressError(MailmanError):
    """Base class for email address validation errors."""
    pass

class MMBadEmailError(EmailAddressError):
    """Email address is invalid (empty string or not fully qualified)."""
    pass

class MMHostileAddress(EmailAddressError):
    """Email address has potentially hostile characters in it."""
    pass


# Exceptions for admin request database
class LostHeldMessage(MailmanError):
    """Held message was lost."""
    pass



def _(s):
    return s

# Exceptions for the Handler subsystem
class HandlerError(MailmanError):
    """Base class for all handler errors."""

class HoldMessage(HandlerError):
    """Base class for all message-being-held short circuits."""

    # funky spelling is necessary to break import loops
    reason = _('For some unknown reason')

    def reason_notice(self):
        return self.reason

    # funky spelling is necessary to break import loops
    rejection = _('Your message was rejected')

    def rejection_notice(self, mlist):
        return self.rejection

class DiscardMessage(HandlerError):
    """The message can be discarded with no further action"""

class SomeRecipientsFailed(HandlerError):
    """Delivery to some or all recipients failed"""
    def __init__(self, tempfailures, permfailures):
        HandlerError.__init__(self)
        self.tempfailures = tempfailures
        self.permfailures = permfailures

# multiple inheritance for backwards compatibility
class LoopError(DiscardMessage, MMLoopingPost):
    """We've seen this message before"""

class RejectMessage(HandlerError):
    """The message will be bounced back to the sender"""
    def __init__(self, notice=None):
        if notice is None:
            notice = _('Your message was rejected')
        if notice.endswith('\n\n'):
            pass
        elif notice.endswith('\n'):
            notice += '\n'
        else:
            notice += '\n\n'
        self.__notice = notice

    def notice(self):
        return self.__notice


# Additional exceptions
class HostileSubscriptionError(MailmanError):
    """A cross-subscription attempt was made."""
    # This exception gets raised when an invitee attempts to use the
    # invitation to cross-subscribe to some other mailing list.
