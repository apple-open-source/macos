/**
 * Copyright (c) 2003-2004, David A. Czarnecki
 * All rights reserved.
 *
 * Portions Copyright (c) 2003-2004 by Mark Lussier
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

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.BlojsomException;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.blog.BlojsomConfigurationException;
import org.blojsom.util.BlojsomConstants;
import org.blojsom.util.BlojsomUtils;

import javax.servlet.ServletConfig;
import java.io.IOException;
import java.io.InputStream;
import java.util.Map;
import java.util.Properties;

/**
 * PropertiesAuthorizationProvider
 *
 * @author David Czarnecki
 * @version $Id: PropertiesAuthorizationProvider.java,v 1.2 2005/01/15 20:39:08 johnan Exp $
 * @since blojsom 2.16
 */
public class PropertiesAuthorizationProvider implements AuthorizationProvider, BlojsomConstants {

    private Log _logger = LogFactory.getLog(PropertiesAuthorizationProvider.class);

    private ServletConfig _servletConfig;
    private String _baseConfigurationDirectory;

    /**
     * Default constructor
     */
    public PropertiesAuthorizationProvider() {
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
        _servletConfig = servletConfig;
        _baseConfigurationDirectory = blojsomConfiguration.getBaseConfigurationDirectory();

        _logger.debug("Initialized properties authorization provider");
    }

    /**
     * Loads the authentication credentials for a given user
     *
     * @param blogUser {@link BlogUser}
     * @throws BlojsomException If there is an error loading the user's authentication credentials
     */
    public void loadAuthenticationCredentials(BlogUser blogUser) throws BlojsomException {
        String authorizationConfiguration = _servletConfig.getInitParameter(BLOG_AUTHORIZATION_IP);
        if (BlojsomUtils.checkNullOrBlank(authorizationConfiguration)) {
            _logger.error("No authorization configuration file specified");
            throw new BlojsomException("No authorization configuration file specified");
        }

        Properties authorizationProperties;
        InputStream is = _servletConfig.getServletContext().getResourceAsStream(_baseConfigurationDirectory + blogUser.getId() + '/' + authorizationConfiguration);
        authorizationProperties = new Properties();
        try {
            authorizationProperties.load(is);
            Map authorizationMap = BlojsomUtils.propertiesToMap(authorizationProperties);
            is.close();
            blogUser.getBlog().setAuthorization(authorizationMap);
        } catch (IOException e) {
            _logger.error(e);
            throw new BlojsomException(e);
        }

    }

    /**
     * Authorize a username and password for the given {@link BlogUser}
     *
     * @param blogUser             {@link BlogUser}
     * @param authorizationContext {@link Map} to be used to provide other information for authorization. This will
     *                             change depending on the authorization provider. This parameter is not used in this implementation.
     * @param username             Username
     * @param password             Password
     * @throws BlojsomException If there is an error authorizing the username and password
     */
    public void authorize(BlogUser blogUser, Map authorizationContext, String username, String password) throws BlojsomException {
        Map authorizationMap = blogUser.getBlog().getAuthorization();
        boolean result = false;

        if (authorizationMap != null) {
            if (authorizationMap.containsKey(username)) {
                String parsedPassword = BlojsomUtils.parseCommaList((String) authorizationMap.get(username))[0];
                if (password.equals(parsedPassword)) {
                    result = true;
                }
            }
        }

        if (!result) {
            throw new BlojsomException("Authorization failed for blog user: " + blogUser.getId() + " for username: " + username);
        }
    }
}