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
package org.blojsom.plugin.admin;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.blog.Blog;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.util.BlojsomUtils;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Collections;
import java.util.Map;
import java.util.Properties;
import java.util.TreeMap;

/**
 * EditBlogAuthorizationPlugin
 *
 * @author czarnecki
 * @version $Id: EditBlogAuthorizationPlugin.java,v 1.2.2.1 2005/07/21 04:30:23 johnan Exp $
 * @since blojsom 2.06
 */
public class EditBlogAuthorizationPlugin extends BaseAdminPlugin {

    private Log _logger = LogFactory.getLog(EditBlogAuthorizationPlugin.class);

    // Pages
    private static final String EDIT_BLOG_AUTHORIZATION_PAGE = "/org/blojsom/plugin/admin/templates/admin-edit-blog-authorization";

    // Constants
    private static final String BLOJSOM_PLUGIN_EDIT_BLOG_AUTHORIZATION_MAP = "BLOJSOM_PLUGIN_EDIT_BLOG_AUTHORIZATION_MAP";

    // Actions
    private static final String ADD_BLOG_AUTHORIZATION_ACTION = "add-blog-authorization";
    private static final String MODIFY_BLOG_AUTHORIZATION_ACTION = "modify-blog-authorization";
    private static final String DELETE_BLOG_AUTHORIZATION_ACTION = "delete-blog-authorization";

    // Form elements
    private static final String BLOG_USER_ID = "blog-user-id";
    private static final String BLOG_USER_PASSWORD = "blog-user-password";
    private static final String BLOG_USER_PASSWORD_CHECK = "blog-user-password-check";
    private static final String BLOG_USER_EMAIL = "blog-user-email";

    // Permissions
    private static final String EDIT_BLOG_AUTHORIZATION_PERMISSION = "edit_blog_authorization";

    private String _authorizationConfiguration;

    /**
     * Default constructor
     */
    public EditBlogAuthorizationPlugin() {
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

        _authorizationConfiguration = servletConfig.getInitParameter(BLOG_AUTHORIZATION_IP);
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
        if (!authenticateUser(httpServletRequest, httpServletResponse, context, user)) {
            httpServletRequest.setAttribute(PAGE_PARAM, ADMIN_LOGIN_PAGE);

            return entries;
        }

        String username = getUsernameFromSession(httpServletRequest, user.getBlog());
        if (!checkPermission(user, null, username, EDIT_BLOG_AUTHORIZATION_PERMISSION)) {
            httpServletRequest.setAttribute(PAGE_PARAM, ADMIN_LOGIN_PAGE);
            addOperationResultMessage(context, "You are not allowed to edit blog authorizations");

            return entries;
        }

        String action = BlojsomUtils.getRequestValue(ACTION_PARAM, httpServletRequest);
        if (BlojsomUtils.checkNullOrBlank(action)) {
            _logger.debug("User did not request edit authorization action");
            httpServletRequest.setAttribute(PAGE_PARAM, ADMIN_ADMINISTRATION_PAGE);
        } else if (PAGE_ACTION.equals(action)) {
            _logger.debug("User requested edit blog authorization page");
        } else if (ADD_BLOG_AUTHORIZATION_ACTION.equals(action) || MODIFY_BLOG_AUTHORIZATION_ACTION.equals(action)) {
            if (ADD_BLOG_AUTHORIZATION_ACTION.equals(action)) {
                _logger.debug("User requested add authorization action");
            } else {
                _logger.debug("User requested modify authorization action");
            }

            String blogUserID = BlojsomUtils.getRequestValue(BLOG_USER_ID, httpServletRequest);
            String blogUserPassword = BlojsomUtils.getRequestValue(BLOG_USER_PASSWORD, httpServletRequest);
            String blogUserPasswordCheck = BlojsomUtils.getRequestValue(BLOG_USER_PASSWORD_CHECK, httpServletRequest);
            String blogUserEmail = BlojsomUtils.getRequestValue(BLOG_USER_EMAIL, httpServletRequest);

            if (!BlojsomUtils.checkNullOrBlank(blogUserID) && !BlojsomUtils.checkNullOrBlank(blogUserPassword)
                    && !BlojsomUtils.checkNullOrBlank(blogUserPasswordCheck)) {
                if (blogUserPassword.equals(blogUserPasswordCheck)) {
                    if (BlojsomUtils.checkNullOrBlank(blogUserEmail)) {
                        blogUserEmail = "";
                    }

                    if (user.getBlog().getUseEncryptedPasswords().booleanValue()) {
                        blogUserPassword = BlojsomUtils.digestString(blogUserPassword, user.getBlog().getDigestAlgorithm());
                    }

                    Blog blog = user.getBlog();
                    blog.setAuthorizedUserPassword(blogUserID, blogUserPassword);
                    blog.setAuthorizedUserEmail(blogUserID, blogUserEmail);
                    user.setBlog(blog);

                    Map authorizationMap = user.getBlog().getAuthorization();

                    if (ADD_BLOG_AUTHORIZATION_ACTION.equals(action)) {
                        _logger.debug("Added user: " + blogUserID + " to authorization map");
                    } else {
                        _logger.debug("Modified user: " + blogUserID + " to authorization map");
                    }

                    try {
                        writeAuthorizationConfiguration(authorizationMap, user.getId());
                        addOperationResultMessage(context, "Successfully updated authorization configuration adding user: " + blogUserID);
                        _logger.debug("Wrote new authorization configuration for user: " + user.getId());
                    } catch (IOException e) {
                        addOperationResultMessage(context, "Unable to update authorization configuration adding user: " + blogUserID);
                        _logger.error(e);
                    }
                } else {
                    addOperationResultMessage(context, "Password and password check not equal");
                    _logger.debug("Password and password check not equal for add/modify authorization action");
                }
            } else {
                addOperationResultMessage(context, "Missing parameters from the request to complete action");
                _logger.debug("Missing parameters from the request to complete add/modify authorization action");
            }
        } else if (DELETE_BLOG_AUTHORIZATION_ACTION.equals(action)) {
            _logger.debug("User requested delete authorization action");

            String blogUserID = BlojsomUtils.getRequestValue(BLOG_USER_ID, httpServletRequest);
            if (!BlojsomUtils.checkNullOrBlank(blogUserID)) {
                Map authorizationMap = user.getBlog().getAuthorization();
                authorizationMap.remove(blogUserID);
                user.getBlog().setAuthorization(authorizationMap);
                _logger.debug("Removed user: " + blogUserID + " from authorization map");

                try {
                    writeAuthorizationConfiguration(authorizationMap, user.getId());
                    addOperationResultMessage(context, "Successfully updated authorization configuration deleting user: " + blogUserID);
                    _logger.debug("Wrote new authorization configuration for user: " + user.getId());
                } catch (IOException e) {
                    // @todo In the event we have an error writing the configuration, do we want to restore the user? We would need to save their information first.
                    addOperationResultMessage(context, "Unable to update authorization configuration deleting user: " + blogUserID);
                    _logger.error(e);
                }
            } else {
                addOperationResultMessage(context, "No blog user id to delete from authorization");
                _logger.debug("No blog user id to delete from authorization");
            }
        }

        context.put(BLOJSOM_PLUGIN_EDIT_BLOG_AUTHORIZATION_MAP, Collections.unmodifiableMap(new TreeMap(user.getBlog().getAuthorization())));
        httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_AUTHORIZATION_PAGE);

        return entries;
    }

    /**
     * Write out the authorization configuration information for a particular user
     *
     * @param authorizationMap Authorization usernames/passwords
     * @param user             User id
     * @throws IOException If there is an error writing the authorization file
     */
    private void writeAuthorizationConfiguration(Map authorizationMap, String user) throws IOException {
        File authorizationFile = new File(_blojsomConfiguration.getInstallationDirectory() + _blojsomConfiguration.getBaseConfigurationDirectory() + user + "/" + _authorizationConfiguration);
        _logger.debug("Writing authorization file: " + authorizationFile.toString());
        Properties authorizationProperties = BlojsomUtils.mapToProperties(authorizationMap);
        FileOutputStream fos = new FileOutputStream(authorizationFile);
        authorizationProperties.store(fos, null);
        fos.close();
    }
}
