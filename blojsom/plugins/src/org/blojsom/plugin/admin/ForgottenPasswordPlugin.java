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
package org.blojsom.plugin.admin;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.blog.Blog;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.plugin.email.EmailMessage;
import org.blojsom.plugin.email.EmailUtils;
import org.blojsom.util.BlojsomConstants;
import org.blojsom.util.BlojsomUtils;
import org.blojsom.BlojsomException;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.util.ArrayList;
import java.util.Map;

/**
 * Forgotten password plugin.
 *
 * @author David Czarnecki
 * @since blojsom 2.14
 * @version $Id: ForgottenPasswordPlugin.java,v 1.1 2004/08/27 01:06:35 whitmore Exp $
 */
public class ForgottenPasswordPlugin extends BaseAdminPlugin implements BlojsomConstants {

    private Log _logger = LogFactory.getLog(ForgottenPasswordPlugin.class);

    private static final String FORGOTTEN_USERNAME_PARAM = "forgotten-username";
    private static final String FORGOTTEN_PASSWORD_PAGE = "forgotten-password";

    /**
     * Default constructor.
     */
    public ForgottenPasswordPlugin() {
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
        try {
            _authorizationProvider.loadAuthenticationCredentials(user);
        } catch (BlojsomException e) {
            addOperationResultMessage(context, "Error loading authorization credentials for user: " + user.getId());
            _logger.error(e);

            return entries;
        }

        String username = BlojsomUtils.getRequestValue(FORGOTTEN_USERNAME_PARAM, httpServletRequest);
        if (!BlojsomUtils.checkNullOrBlank(username)) {
            Blog blog = user.getBlog();
            String authorizedUserEmail = blog.getAuthorizedUserEmail(username);

            if (!BlojsomUtils.checkNullOrBlank(authorizedUserEmail)) {
                EmailMessage emailMessage = new EmailMessage(blog.getBlogOwnerEmail(), authorizedUserEmail, "Forgotten password", "Here is your password: " + blog.getAuthorization().get(username));
                ArrayList emailMessages = new ArrayList();
                emailMessages.add(emailMessage);
                context.put(EmailUtils.BLOJSOM_OUTBOUNDMAIL, emailMessages);
                _logger.debug("Constructed forgotten password e-mail message to user: " + username);
                addOperationResultMessage(context, "Constructed forgotten password e-mail message to username: " + username);
                httpServletRequest.setAttribute(PAGE_PARAM, ADMIN_LOGIN_PAGE);
            } else {
                _logger.debug("Authorized e-mail address was blank for user: " + username);
                addOperationResultMessage(context, "No authorized user found with username: " + username);
                httpServletRequest.setAttribute(PAGE_PARAM, FORGOTTEN_PASSWORD_PAGE);
            }
        } else {
            addOperationResultMessage(context, "No username provided");
            httpServletRequest.setAttribute(PAGE_PARAM, FORGOTTEN_PASSWORD_PAGE);
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