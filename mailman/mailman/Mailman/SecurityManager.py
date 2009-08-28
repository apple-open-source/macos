# Copyright (C) 1998-2008 by the Free Software Foundation, Inc.
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


"""Handle passwords and sanitize approved messages."""

# There are current 5 roles defined in Mailman, as codified in Defaults.py:
# user, list-creator, list-moderator, list-admin, site-admin.
#
# Here's how we do cookie based authentication.
#
# Each role (see above) has an associated password, which is currently the
# only way to authenticate a role (in the future, we'll authenticate a
# user and assign users to roles).
#
# Each cookie has the following ingredients: the authorization context's
# secret (i.e. the password, and a timestamp.  We generate an SHA1 hex
# digest of these ingredients, which we call the `mac'.  We then marshal
# up a tuple of the timestamp and the mac, hexlify that and return that as
# a cookie keyed off the authcontext.  Note that authenticating the user
# also requires the user's email address to be included in the cookie.
#
# The verification process is done in CheckCookie() below.  It extracts
# the cookie, unhexlifies and unmarshals the tuple, extracting the
# timestamp.  Using this, and the shared secret, the mac is calculated,
# and it must match the mac passed in the cookie.  If so, they're golden,
# otherwise, access is denied.
#
# It is still possible for an adversary to attempt to brute force crack
# the password if they obtain the cookie, since they can extract the
# timestamp and create macs based on password guesses.  They never get a
# cleartext version of the password though, so security rests on the
# difficulty and expense of retrying the cgi dialog for each attempt.  It
# also relies on the security of SHA1.

import os
import re
import time
import Cookie
import marshal
import binascii
import urllib
from types import StringType, TupleType
from urlparse import urlparse

try:
    import crypt
except ImportError:
    crypt = None

from Mailman import mm_cfg
from Mailman import Utils
from Mailman import Errors
from Mailman.Logging.Syslog import syslog
from Mailman.Utils import md5_new, sha_new

try:
    True, False
except NameError:
    True = 1
    False = 0



class SecurityManager:
    def InitVars(self):
        # We used to set self.password here, from a crypted_password argument,
        # but that's been removed when we generalized the mixin architecture.
        # self.password is really a SecurityManager attribute, but it's set in
        # MailList.InitVars().
        self.mod_password = None
        # Non configurable
        self.passwords = {}

    def AuthContextInfo(self, authcontext, user=None):
        # authcontext may be one of AuthUser, AuthListModerator,
        # AuthListAdmin, AuthSiteAdmin.  Not supported is the AuthCreator
        # context.
        #
        # user is ignored unless authcontext is AuthUser
        #
        # Return the authcontext's secret and cookie key.  If the authcontext
        # doesn't exist, return the tuple (None, None).  If authcontext is
        # AuthUser, but the user isn't a member of this mailing list, a
        # NotAMemberError will be raised.  If the user's secret is None, raise
        # a MMBadUserError.
        key = self.internal_name() + '+'
        if authcontext == mm_cfg.AuthUser:
            if user is None:
                # A bad system error
                raise TypeError, 'No user supplied for AuthUser context'
            secret = self.getMemberPassword(user)
            userdata = urllib.quote(Utils.ObscureEmail(user), safe='')
            key += 'user+%s' % userdata
        elif authcontext == mm_cfg.AuthListModerator:
            secret = self.mod_password
            key += 'moderator'
        elif authcontext == mm_cfg.AuthListAdmin:
            secret = self.password
            key += 'admin'
        # BAW: AuthCreator
        elif authcontext == mm_cfg.AuthSiteAdmin:
            sitepass = Utils.get_global_password()
            if mm_cfg.ALLOW_SITE_ADMIN_COOKIES and sitepass:
                secret = sitepass
                key = 'site'
            else:
                # BAW: this should probably hand out a site password based
                # cookie, but that makes me a bit nervous, so just treat site
                # admin as a list admin since there is currently no site
                # admin-only functionality.
                secret = self.password
                key += 'admin'
        else:
            return None, None
        return key, secret

    def Authenticate(self, authcontexts, response, user=None):
        # Given a list of authentication contexts, check to see if the
        # response matches one of the passwords.  authcontexts must be a
        # sequence, and if it contains the context AuthUser, then the user
        # argument must not be None.
        #
        # Return the authcontext from the argument sequence that matches the
        # response, or UnAuthorized.
        for ac in authcontexts:
            if ac == mm_cfg.AuthCreator:
                ok = Utils.check_global_password(response, siteadmin=0)
                if ok:
                    return mm_cfg.AuthCreator
            elif ac == mm_cfg.AuthSiteAdmin:
                ok = Utils.check_global_password(response)
                if ok:
                    return mm_cfg.AuthSiteAdmin
            elif ac == mm_cfg.AuthListAdmin:
                def cryptmatchp(response, secret):
                    try:
                        salt = secret[:2]
                        if crypt and crypt.crypt(response, salt) == secret:
                            return True
                        return False
                    except TypeError:
                        # BAW: Hard to say why we can get a TypeError here.
                        # SF bug report #585776 says crypt.crypt() can raise
                        # this if salt contains null bytes, although I don't
                        # know how that can happen (perhaps if a MM2.0 list
                        # with USE_CRYPT = 0 has been updated?  Doubtful.
                        return False
                # The password for the list admin and list moderator are not
                # kept as plain text, but instead as an sha hexdigest.  The
                # response being passed in is plain text, so we need to
                # digestify it first.  Note however, that for backwards
                # compatibility reasons, we'll also check the admin response
                # against the crypted and md5'd passwords, and if they match,
                # we'll auto-migrate the passwords to sha.
                key, secret = self.AuthContextInfo(ac)
                if secret is None:
                    continue
                sharesponse = sha_new(response).hexdigest()
                upgrade = ok = False
                if sharesponse == secret:
                    ok = True
                elif md5_new(response).digest() == secret:
                    ok = upgrade = True
                elif cryptmatchp(response, secret):
                    ok = upgrade = True
                if upgrade:
                    save_and_unlock = False
                    if not self.Locked():
                        self.Lock()
                        save_and_unlock = True
                    try:
                        self.password = sharesponse
                        if save_and_unlock:
                            self.Save()
                    finally:
                        if save_and_unlock:
                            self.Unlock()
                if ok:
                    return ac
            elif ac == mm_cfg.AuthListModerator:
                # The list moderator password must be sha'd
                key, secret = self.AuthContextInfo(ac)
                if secret and sha_new(response).hexdigest() == secret:
                    return ac
            elif ac == mm_cfg.AuthUser:
                if user is not None:
                    try:
                        if self.authenticateMember(user, response):
                            return ac
                    except Errors.NotAMemberError:
                        pass
            else:
                # What is this context???
                syslog('error', 'Bad authcontext: %s', ac)
                raise ValueError, 'Bad authcontext: %s' % ac
        return mm_cfg.UnAuthorized

    def WebAuthenticate(self, authcontexts, response, user=None):
        # Given a list of authentication contexts, check to see if the cookie
        # contains a matching authorization, falling back to checking whether
        # the response matches one of the passwords.  authcontexts must be a
        # sequence, and if it contains the context AuthUser, then the user
        # argument should not be None.
        #
        # Returns a flag indicating whether authentication succeeded or not.
        for ac in authcontexts:
            ok = self.CheckCookie(ac, user)
            if ok:
                return True
        # Check passwords
        ac = self.Authenticate(authcontexts, response, user)
        if ac:
            print self.MakeCookie(ac, user)
            return True
        return False

    def MakeCookie(self, authcontext, user=None):
        key, secret = self.AuthContextInfo(authcontext, user)
        if key is None or secret is None or not isinstance(secret, StringType):
            raise ValueError
        # Timestamp
        issued = int(time.time())
        # Get a digest of the secret, plus other information.
        mac = sha_new(secret + `issued`).hexdigest()
        # Create the cookie object.
        c = Cookie.SimpleCookie()
        c[key] = binascii.hexlify(marshal.dumps((issued, mac)))
        # The path to all Mailman stuff, minus the scheme and host,
        # i.e. usually the string `/mailman'
        path = urlparse(self.web_page_url)[2]
        c[key]['path'] = path
        # We use session cookies, so don't set `expires' or `max-age' keys.
        # Set the RFC 2109 required header.
        c[key]['version'] = 1
        return c

    def ZapCookie(self, authcontext, user=None):
        # We can throw away the secret.
        key, secret = self.AuthContextInfo(authcontext, user)
        # Logout of the session by zapping the cookie.  For safety both set
        # max-age=0 (as per RFC2109) and set the cookie data to the empty
        # string.
        c = Cookie.SimpleCookie()
        c[key] = ''
        # The path to all Mailman stuff, minus the scheme and host,
        # i.e. usually the string `/mailman'
        path = urlparse(self.web_page_url)[2]
        c[key]['path'] = path
        c[key]['max-age'] = 0
        # Don't set expires=0 here otherwise it'll force a persistent cookie
        c[key]['version'] = 1
        return c

    def CheckCookie(self, authcontext, user=None):
        # Two results can occur: we return 1 meaning the cookie authentication
        # succeeded for the authorization context, we return 0 meaning the
        # authentication failed.
        #
        # Dig out the cookie data, which better be passed on this cgi
        # environment variable.  If there's no cookie data, we reject the
        # authentication.
        cookiedata = os.environ.get('HTTP_COOKIE')
        if not cookiedata:
            return False
        # We can't use the Cookie module here because it isn't liberal in what
        # it accepts.  Feed it a MM2.0 cookie along with a MM2.1 cookie and
        # you get a CookieError. :(.  All we care about is accessing the
        # cookie data via getitem, so we'll use our own parser, which returns
        # a dictionary.
        c = parsecookie(cookiedata)
        # If the user was not supplied, but the authcontext is AuthUser, we
        # can try to glean the user address from the cookie key.  There may be
        # more than one matching key (if the user has multiple accounts
        # subscribed to this list), but any are okay.
        if authcontext == mm_cfg.AuthUser:
            if user:
                usernames = [user]
            else:
                usernames = []
                prefix = self.internal_name() + '+user+'
                for k in c.keys():
                    if k.startswith(prefix):
                        usernames.append(k[len(prefix):])
            # If any check out, we're golden.  Note: `@'s are no longer legal
            # values in cookie keys.
            for user in [Utils.UnobscureEmail(urllib.unquote(u))
                         for u in usernames]:
                ok = self.__checkone(c, authcontext, user)
                if ok:
                    return True
            return False
        else:
            return self.__checkone(c, authcontext, user)

    def __checkone(self, c, authcontext, user):
        # Do the guts of the cookie check, for one authcontext/user
        # combination.
        try:
            key, secret = self.AuthContextInfo(authcontext, user)
        except Errors.NotAMemberError:
            return False
        if not c.has_key(key) or not isinstance(secret, StringType):
            return False
        # Undo the encoding we performed in MakeCookie() above.  BAW: I
        # believe this is safe from exploit because marshal can't be forced to
        # load recursive data structures, and it can't be forced to execute
        # any unexpected code.  The worst that can happen is that either the
        # client will have provided us bogus data, in which case we'll get one
        # of the caught exceptions, or marshal format will have changed, in
        # which case, the cookie decoding will fail.  In either case, we'll
        # simply request reauthorization, resulting in a new cookie being
        # returned to the client.
        try:
            data = marshal.loads(binascii.unhexlify(c[key]))
            issued, received_mac = data
        except (EOFError, ValueError, TypeError, KeyError):
            return False
        # Make sure the issued timestamp makes sense
        now = time.time()
        if now < issued:
            return False
        # Calculate what the mac ought to be based on the cookie's timestamp
        # and the shared secret.
        mac = sha_new(secret + `issued`).hexdigest()
        if mac <> received_mac:
            return False
        # Authenticated!
        return True



splitter = re.compile(';\s*')

def parsecookie(s):
    c = {}
    for line in s.splitlines():
        for p in splitter.split(line):
            try:
                k, v = p.split('=', 1)
            except ValueError:
                pass
            else:
                c[k] = v
    return c
