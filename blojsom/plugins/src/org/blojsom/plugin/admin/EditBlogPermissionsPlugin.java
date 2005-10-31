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
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.util.BlojsomUtils;
import org.blojsom.util.BlojsomProperties;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.util.*;
import java.io.IOException;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;

/**
 * Edit Blog Permissions plugin handles the adding and deleting of permissions for users of a given blog.
 *
 * @author David Czarnecki
 * @version $Id: EditBlogPermissionsPlugin.java,v 1.1.2.1 2005/07/21 04:30:24 johnan Exp $
 * @since blojsom 2.23
 */
public class EditBlogPermissionsPlugin extends BaseAdminPlugin {

    private Log _logger = LogFactory.getLog(EditBlogPermissionsPlugin.class);

    // Pages
    private static final String EDIT_BLOG_PERMISSIONS_PAGE = "/org/blojsom/plugin/admin/templates/admin-edit-blog-permissions";

    // Constants
    private static final String BLOJSOM_PLUGIN_EDIT_BLOG_PERMISSIONS_USER_MAP = "BLOJSOM_PLUGIN_EDIT_BLOG_PERMISSIONS_USER_MAP";
    private static final String BLOJSOM_PLUGIN_EDIT_BLOG_PERMISSIONS_MAP = "BLOJSOM_PLUGIN_EDIT_BLOG_PERMISSIONS_MAP";

    // Actions
    private static final String ADD_BLOG_PERMISSION_ACTION = "add-blog-permission";
    private static final String DELETE_BLOG_PERMISSION_ACTION = "delete-blog-permission";

    // Form elements
    private static final String BLOG_USER_ID = "blog-user-id";
    private static final String BLOG_PERMISSION = "blog-permission";

    // Permissions
    private static final String EDIT_BLOG_PERMISSIONS_PERMISSION = "edit_blog_permissions";

    private String _permissionConfiguration;

    /**
     * Construct a new instance of the Edit Blog Permissions plugin
     */
    public EditBlogPermissionsPlugin() {
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

        _permissionConfiguration = servletConfig.getInitParameter(BLOG_PERMISSIONS_IP);
        if (BlojsomUtils.checkNullOrBlank(_permissionConfiguration)) {
            _logger.error("No permissions configuration file specified. Using default: " + DEFAULT_PERMISSIONS_CONFIGURATION_FILE);
            _permissionConfiguration = DEFAULT_PERMISSIONS_CONFIGURATION_FILE;
        }
    }

    /**
     * Read the permissions configuration file for a given blog
     *
     * @param blogID Blog ID
     * @throws IOException If there is an error reading the permissions configuration file
     */
    private Properties readPermissionsConfiguration(String blogID) throws IOException {
        File permissionsFile = new File(_blojsomConfiguration.getInstallationDirectory() + _blojsomConfiguration.getBaseConfigurationDirectory()
                + blogID + "/" + _permissionConfiguration);
        Properties permissionsProperties = new BlojsomProperties(true);
        FileInputStream fis = new FileInputStream(permissionsFile);
        permissionsProperties.load(fis);
        fis.close();

        return permissionsProperties;
    }

    /**
     * Write the permissions configuration file for a given blog
     *
     * @param blogID Blog ID
     * @param permissions Permissions
     * @throws IOException If there is an error writing the permissions configuration file
     */
    private void writePermissionsConfiguration(String blogID, Properties permissions) throws IOException {
        File permissionsFile = new File(_blojsomConfiguration.getInstallationDirectory() + _blojsomConfiguration.getBaseConfigurationDirectory()
                + blogID + "/" + _permissionConfiguration);
        FileOutputStream fos = new FileOutputStream(permissionsFile);
        permissions.store(fos, null);
        fos.close();
    }

    /**
     * Read the permissions file for a given blog
     *
     * @param context Context for messages
     * @param blogID Blog ID
     * @return Permissions for the given blog
     */
    private Map readPermissionsForBlog(Map context, String blogID) {
        Map permissions = new TreeMap();
        try {
            Properties permissionsProperties = readPermissionsConfiguration(blogID);
            permissions = new TreeMap(BlojsomUtils.blojsomPropertiesToMap(permissionsProperties));
        } catch (IOException e) {
            _logger.error(e);
            addOperationResultMessage(context, "Unable to read permissions for blog: " + blogID);
        }

        return permissions;
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
        if (!checkPermission(user, null, username, EDIT_BLOG_PERMISSIONS_PERMISSION)) {
            httpServletRequest.setAttribute(PAGE_PARAM, ADMIN_LOGIN_PAGE);
            addOperationResultMessage(context, "You are not allowed to edit blog permissions");

            return entries;
        }

        Map permissions = readPermissionsForBlog(context, user.getId());

        // Sanitize permissions if a user has been deleted
        Map authorizedUsers = user.getBlog().getAuthorization();
        Iterator users = permissions.keySet().iterator();
        List usersToRemove = new ArrayList();
        while (users.hasNext()) {
            String userID = (String) users.next();
            if (!authorizedUsers.containsKey(userID)) {
                usersToRemove.add(userID);
            }
        }

        for (int i = 0; i < usersToRemove.size(); i++) {
            String userID = (String) usersToRemove.get(i);

            permissions.remove(userID);
        }

        String action = BlojsomUtils.getRequestValue(ACTION_PARAM, httpServletRequest);
        if (BlojsomUtils.checkNullOrBlank(action)) {
            _logger.debug("User did not request edit permission action");
            httpServletRequest.setAttribute(PAGE_PARAM, ADMIN_ADMINISTRATION_PAGE);
        } else if (PAGE_ACTION.equals(action)) {
            _logger.debug("User requested edit blog permissions page");
        } else if (ADD_BLOG_PERMISSION_ACTION.equals(action)) {
            _logger.debug("User requested add permission action");

            String blogUserID = BlojsomUtils.getRequestValue(BLOG_USER_ID, httpServletRequest);
            if (!BlojsomUtils.checkNullOrBlank(blogUserID)) {
                String permissionToAdd = BlojsomUtils.getRequestValue(BLOG_PERMISSION, httpServletRequest);
                if (!BlojsomUtils.checkNullOrBlank(permissionToAdd)) {
                    Properties updatedPermissions = BlojsomUtils.mapToBlojsomProperties(permissions);
                    if (updatedPermissions.containsKey(blogUserID)) {
                        List permissionList = (List) updatedPermissions.get(blogUserID);
                        if (!permissionList.contains(permissionToAdd)) {
                            permissionList.add(permissionToAdd);
                        }
                    } else {
                        List permissionList = new ArrayList();
                        permissionList.add(permissionToAdd);
                        updatedPermissions.put(blogUserID, permissionList);
                    }

                    try {
                        writePermissionsConfiguration(user.getId(), updatedPermissions);
                        permissions = readPermissionsForBlog(context, user.getId());
                        addOperationResultMessage(context, "Permissions saved");
                    } catch (IOException e) {
                        _logger.error(e);
                        addOperationResultMessage(context, "Error saving permissions");
                    }
                } else {
                    addOperationResultMessage(context, "No permission given to add");
                }
            } else {
                addOperationResultMessage(context, "No blog user id specified");
                _logger.debug("No blog user id specified");
            }
        } else if (DELETE_BLOG_PERMISSION_ACTION.equals(action)) {
            _logger.debug("User requested delete permission action");

            String blogUserID = BlojsomUtils.getRequestValue(BLOG_USER_ID, httpServletRequest);
            if (!BlojsomUtils.checkNullOrBlank(blogUserID)) {
                String permissionToDelete = BlojsomUtils.getRequestValue(BLOG_PERMISSION, httpServletRequest);
                if (!BlojsomUtils.checkNullOrBlank(permissionToDelete)) {
                    Properties updatedPermissions = BlojsomUtils.mapToBlojsomProperties(permissions);
                    if (updatedPermissions.containsKey(blogUserID)) {
                        List permissionList = (List) updatedPermissions.get(blogUserID);
                        if (permissionList.contains(permissionToDelete)) {
                            permissionList.remove(permissionToDelete);
                        }
                    }

                    try {
                        writePermissionsConfiguration(user.getId(), updatedPermissions);
                        permissions = readPermissionsForBlog(context, user.getId());
                        addOperationResultMessage(context, "Permission deleted");
                    } catch (IOException e) {
                        _logger.error(e);
                        addOperationResultMessage(context, "Error saving permissions");
                    }
                } else {
                    addOperationResultMessage(context, "No permission given to delete");
                }
            } else {
                addOperationResultMessage(context, "No blog user id to delete from authorization");
                _logger.debug("No blog user id to delete from authorization");
            }
        }


        context.put(BLOJSOM_PLUGIN_EDIT_BLOG_PERMISSIONS_USER_MAP, Collections.unmodifiableMap(new TreeMap(user.getBlog().getAuthorization())));
        context.put(BLOJSOM_PLUGIN_EDIT_BLOG_PERMISSIONS_MAP, Collections.unmodifiableMap(permissions));
        httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_PERMISSIONS_PAGE);

        return entries;
    }
}