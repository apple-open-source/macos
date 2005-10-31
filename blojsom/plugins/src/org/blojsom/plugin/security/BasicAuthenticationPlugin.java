/**
 * Copyright (c) 2003-2005 , David A. Czarnecki
 * All rights reserved.
 *
 * Portions Copyright (c) 2003-2005  by Mark Lussier
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
package org.blojsom.plugin.security;

import org.apache.commons.codec.binary.Base64;
import org.blojsom.BlojsomException;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.plugin.admin.BaseAdminPlugin;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.UnsupportedEncodingException;
import java.text.MessageFormat;
import java.util.Map;

/**
 * Basic Authentication plugin performs a BASIC authorization check so that users much authenticate
 * before they are able to see any blog entries.
 *
 * @author David Czarnecki
 * @version $Id: BasicAuthenticationPlugin.java,v 1.1.2.1 2005/07/21 04:30:39 johnan Exp $
 * @since blojsom 2.23
 */
public class BasicAuthenticationPlugin extends BaseAdminPlugin {

    private static final String AUTHORIZATION_HEADER = "Authorization";
    private static final String WWW_AUTHENTICATE_HEADER = "WWW-Authenticate";
    private static final String BASIC_REALM_HEADER = "Basic realm=\"{0}\"";
    private static final String FAILED_AUTHORIZATION_PAGE = "/org/blojsom/plugin/security/templates/failed-authorization";

    /**
     * Construct a new instance of the Basic Authentication plugin
     */
    public BasicAuthenticationPlugin() {
    }

    /**
     * Initialize this plugin. This method only called when the plugin is instantiated.
     *
     * @param servletConfig        Servlet config object for the plugin to retrieve any initialization parameters
     * @param blojsomConfiguration {@link org.blojsom.blog.BlojsomConfiguration} information
     * @throws org.blojsom.plugin.BlojsomPluginException
     *          If there is an error initializing the plugin
     */
    public void init(ServletConfig servletConfig, BlojsomConfiguration blojsomConfiguration) throws BlojsomPluginException {
        super.init(servletConfig, blojsomConfiguration);
    }

    /**
     * Set the appropriate headers for BASIC authentication
     *
     * @param httpServletResponse Response
     * @param blogUser            {@link BlogUser}
     */
    protected void setAuthenticationRequired(HttpServletResponse httpServletResponse, BlogUser blogUser) {
        httpServletResponse.setStatus(HttpServletResponse.SC_UNAUTHORIZED);
        httpServletResponse.setHeader(WWW_AUTHENTICATE_HEADER, MessageFormat.format(BASIC_REALM_HEADER, new String[]{blogUser.getBlog().getBlogName()}));
    }

    /**
     * Decode the BASIC authentication credentials and check the username/password against
     * the authorized users for the blog.
     *
     * @param httpServletRequest Request
     * @param blogUser           {@link BlogUser}
     * @return <code>true</code> if the BASIC authentication credentials are available and pass authentication,
     *         <code>false</code> otherwise
     */
    protected boolean decodeCredentialsAndAuthenticate(HttpServletRequest httpServletRequest, BlogUser blogUser) {
        String authorization = httpServletRequest.getHeader(AUTHORIZATION_HEADER);
        if (authorization != null) {
            String encodedCredentials = authorization.substring(6).trim();

            try {
                String usernameAndPassword = new String(Base64.decodeBase64(encodedCredentials.getBytes(UTF8)));
                int colonIndex = usernameAndPassword.indexOf(":");
                if (colonIndex > 0) {
                    String username = usernameAndPassword.substring(0, colonIndex);
                    String password = usernameAndPassword.substring(colonIndex + 1);

                    try {
                        _authorizationProvider.loadAuthenticationCredentials(blogUser);
                        _authorizationProvider.authorize(blogUser, null, username, password);

                        return true;
                    } catch (BlojsomException e) {
                        _logger.error(e);

                        return false;
                    }
                }
            } catch (UnsupportedEncodingException e) {
                _logger.error(e);

                return false;
            }
        }

        return false;
    }

    /**
     * Process the blog entries
     *
     * @param httpServletRequest  Request
     * @param httpServletResponse Response
     * @param user                {@link org.blojsom.blog.BlogUser} instance
     * @param context             Context
     * @param entries             Blog entries retrieved for the particular request
     * @return Modified set of blog entries
     * @throws org.blojsom.plugin.BlojsomPluginException
     *          If there is an error processing the blog entries
     */
    public BlogEntry[] process(HttpServletRequest httpServletRequest, HttpServletResponse httpServletResponse, BlogUser user, Map context, BlogEntry[] entries) throws BlojsomPluginException {
        if (!decodeCredentialsAndAuthenticate(httpServletRequest, user)) {
            setAuthenticationRequired(httpServletResponse, user);

            httpServletRequest.setAttribute(PAGE_PARAM, FAILED_AUTHORIZATION_PAGE);
            return new BlogEntry[0];
        }

        return entries;
    }

    /**
     * Perform any cleanup for the plugin. Called after {@link #process}.
     *
     * @throws org.blojsom.plugin.BlojsomPluginException
     *          If there is an error performing cleanup for this plugin
     */
    public void cleanup() throws BlojsomPluginException {
    }

    /**
     * Called when BlojsomServlet is taken out of service
     *
     * @throws org.blojsom.plugin.BlojsomPluginException
     *          If there is an error in finalizing this plugin
     */
    public void destroy() throws BlojsomPluginException {
    }
}