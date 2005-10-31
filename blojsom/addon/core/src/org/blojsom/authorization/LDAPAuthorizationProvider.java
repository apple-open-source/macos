/**
 * Copyright (c) 2003-2005, David A. Czarnecki
 * All rights reserved.
 *
 * Portions Copyright (c) 2003-2005 by Mark Lussier
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 * Neither the name of the "David A. Czarnecki" and "blojsom" nor the names of
 * its contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * Products derived from this software may not be called "blojsom",
 * nor may "blojsom" appear in their name, without prior written permission of
 * David A. Czarnecki.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
package org.blojsom.authorization;

import netscape.ldap.LDAPConnection;
import netscape.ldap.LDAPException;
import netscape.ldap.LDAPSearchResults;
import netscape.ldap.LDAPv2;
import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.BlojsomException;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.blog.BlojsomConfigurationException;
import org.blojsom.util.BlojsomConstants;
import org.blojsom.util.BlojsomUtils;

import javax.servlet.ServletConfig;
import java.util.Map;

/**
 * LDAPAuthorizationProvider
 * <p></p>
 * This implementation authenticates a user against an LDAP server.  The user
 * name must be the same as that of their LDAP user (uid).  There are two ways
 * to configure this in terms of the accepted users.  The first is where only
 * the blog owner can edit the blog.  To use this technique, delete the
 * authorization.properties file from the user's blog directory.  The lack of
 * this file tells the authorization logic to use the blog owner as the UID for
 * LDAP authentication.  The second way provides multiple user editing of a
 * blog.  This second way utilizes the authorization.properties file's user
 * names (it ignores passwords and other data).  Incoming authorization requests
 * have the user name checked to see if it is listed in the
 * authorization.properties file (indicating a user who is allowed to edit this
 * blog).  If it is in the list, this username is used as the LDAP UID.  This
 * class/implementation requires LDAP protocol version 3.  You must set the
 * configuration values defined by the BlojsomConstants:
 * BLOG_LDAP_AUTHORIZATION_SERVER_IP, BLOG_LDAP_AUTHORIZATION_DN_IP, and
 * BLOG_LDAP_AUTHORIZATION_PORT_IP (optional).
 * <p></p>
 * Note, this implementation currently requires the Mozilla LDAP Java SDK.  See
 * http://www.mozilla.org/directory/.
 *
 * @author Christopher Bailey
 * @version $Id: LDAPAuthorizationProvider.java,v 1.1.2.1 2005/07/21 04:30:20 johnan Exp $
 * @since blojsom 2.22
 */
public class LDAPAuthorizationProvider extends PropertiesAuthorizationProvider implements BlojsomConstants {

    private static final String BLOG_LDAP_AUTHORIZATION_SERVER_IP = "blog-ldap-authorization-server";
    private static final String BLOG_LDAP_AUTHORIZATION_PORT_IP = "blog-ldap-authorization-port";
    private static final String BLOG_LDAP_AUTHORIZATION_DN_IP = "blog-ldap-authorization-dn";

    private Log _logger = LogFactory.getLog(LDAPAuthorizationProvider.class);
    private String _ldapServer;
    private int _ldapPort = 389;
    private String _ldapDN;

    /**
     * Default constructor
     */
    public LDAPAuthorizationProvider() {
    }

    /**
     * Initialization method for the authorization provider
     *
     * @param servletConfig        ServletConfig for obtaining any initialization parameters
     * @param blojsomConfiguration BlojsomConfiguration for blojsom-specific configuration information
     * @throws org.blojsom.blog.BlojsomConfigurationException
     *          If there is an error initializing the provider
     */
    public void init(ServletConfig servletConfig, BlojsomConfiguration blojsomConfiguration) throws BlojsomConfigurationException {
        super.init(servletConfig, blojsomConfiguration);

        _logger.debug("Initialized LDAP authorization provider");
    }

    /**
     * Loads/configures the authentication credentials for a given blog.
     *
     * @param blogUser {@link BlogUser}
     * @throws BlojsomException If there is an error loading the user's authentication credentials
     */
    public void loadAuthenticationCredentials(BlogUser blogUser) throws BlojsomException {
        _ldapServer = _servletConfig.getInitParameter(BLOG_LDAP_AUTHORIZATION_SERVER_IP);
        _ldapDN = _servletConfig.getInitParameter(BLOG_LDAP_AUTHORIZATION_DN_IP);
        String port = _servletConfig.getInitParameter(BLOG_LDAP_AUTHORIZATION_PORT_IP);

        // We don't setup a credentions map here, because with LDAP, you can't
        // obtain the user's passwords, you can only check/authenticate against
        // the LDAP server.  Instead, check each time in the authorize method.

        _logger.debug("LDAPAuthorizationProvider server: " + _ldapServer);
        _logger.debug("LDAPAuthorizationProvider port: " + port);
        _logger.debug("LDAPAuthorizationProvider DN: " + _ldapDN);

        if (BlojsomUtils.checkNullOrBlank(_ldapServer)) {
            String msg = "No LDAP authorization server specified.";
            _logger.error(msg);
            throw new BlojsomException(msg);
        }

        if (BlojsomUtils.checkNullOrBlank(_ldapDN)) {
            String msg = "No LDAP authorization DN specified.";
            _logger.error(msg);
            throw new BlojsomException(msg);
        }

        if (!BlojsomUtils.checkNullOrBlank(port)) {
            try {
                _ldapPort = Integer.valueOf(port).intValue();
                if ((0 > _ldapPort) || (_ldapPort > 65535)) {
                    _logger.error("LDAP port is not in valid range [0,65535].");
                    throw new NumberFormatException();
                }
            } catch (NumberFormatException nfe) {
                String msg = "Invalid LDAP port '" + port + "' specified.";
                _logger.error(msg);
                throw new BlojsomException(msg);
            }
        }

        // Now load the list of acceptible LDAP user ID's from the
        // authorization properties file, if available.
        try {
            super.loadAuthenticationCredentials(blogUser);
        } catch (Exception be) {
            // Do nothing, as we don't have to use the auth file if not there,
            // or if bad, authorize will indicate error.
        }
    }

    /**
     * Authorize a username and password for the given {@link BlogUser}
     *
     * @param blogUser             {@link BlogUser}
     * @param authorizationContext {@link Map} to be used to provide other information for authorization. This will
     *                             change depending on the authorization provider. This parameter is not used in this implementation.
     * @param username             Username.  In this implementation, this value must match that of the blog user's ID.
     * @param password             Password
     * @throws BlojsomException If there is an error authorizing the username and password
     */
    public void authorize(BlogUser blogUser, Map authorizationContext, String username, String password) throws BlojsomException {
        String dn = getDN(username);
        Map authorizationMap = blogUser.getBlog().getAuthorization();

        if (BlojsomUtils.checkNullOrBlank(getServer()) || BlojsomUtils.checkNullOrBlank(dn)) {
            String msg = "Authorization failed for blog user: " + blogUser.getId() + " for username: " + username + "; " + "LDAP not properly configured";
            _logger.error(msg);
            throw new BlojsomException(msg);
        }

        // See if we have a list of possible user's
        if ((authorizationMap != null) && (!authorizationMap.containsKey(username))) {
            String msg = "Username '" + username + "' is not an authorized user of this blog.";
            _logger.error(msg);
            throw new BlojsomException(msg);
        }
        // otherwise, only the blog owner is allowed to login
        else if (!blogUser.getId().equals(username)) {
            String msg = username + " is not the owner of this blog, and only the owner (" + blogUser.getId() + ") is authorized to use this blog.";
            _logger.error(msg);
            throw new BlojsomException(msg);
        }

        try {
            LDAPConnection ldapConnection = new LDAPConnection();

            // Connect to the directory server
            ldapConnection.connect(getServer(), getPort());

            if (blogUser.getBlog().getUseEncryptedPasswords().booleanValue()) {
                password = BlojsomUtils.digestString(password, blogUser.getBlog().getDigestAlgorithm());
            }

            // Use simple authentication. The first argument
            // specifies the version of the LDAP protocol used.
            ldapConnection.authenticate(3, dn, password);

            _logger.debug("Successfully authenticated user '" + username + "' via LDAP.");
        } catch (LDAPException e) {
            String reason;
            switch (e.getLDAPResultCode()) {
                // The DN does not correspond to any existing entry
                case LDAPException.NO_SUCH_OBJECT:
                    reason = "The specified user does not exist: " + dn;
                    break;
                    // The password is incorrect
                case LDAPException.INVALID_CREDENTIALS:
                    reason = "Invalid password";
                    break;
                    // Some other error occurred
                default:
                    reason = "Failed to authenticate as " + dn + ", " + e;
                    break;
            }

            String msg = "Authorization failed for blog user: " + blogUser.getId() + " for username: " + username + "; " + reason;
            _logger.error(msg);
            throw new BlojsomException(msg);
        }
    }

    protected String getDN(String username) {
        try {
            LDAPConnection ldapConnection = new LDAPConnection();

            // Connect to the directory server
            ldapConnection.connect(getServer(), getPort());

            // Search for the dn of the user given the username (uid).
            String[] attrs = {};
            LDAPSearchResults res = ldapConnection.search(getBaseDN(),
                    LDAPv2.SCOPE_SUB, "(uid=" + username + ")", attrs, true);

            if (!res.hasMoreElements()) {
                // No such user.
                _logger.debug("User '" + username + "' does not exist in LDAP directory.");
                return null;
            }

            String dn = res.next().getDN();
            ldapConnection.disconnect();
            _logger.debug("Successfully got user DN '" + dn + "' via LDAP.");

            return dn;
        } catch (LDAPException e) {
            // Some exception occurred above; the search for the dn failed.
            return null;
        }
    }

    protected String getServer() {
        return _ldapServer;
    }

    protected int getPort() {
        return _ldapPort;
    }

    protected String getBaseDN() {
        return _ldapDN;
    }
}
