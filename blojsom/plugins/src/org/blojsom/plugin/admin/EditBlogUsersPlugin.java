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
import org.blojsom.BlojsomException;
import org.blojsom.blog.Blog;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.util.BlojsomUtils;
import org.blojsom.util.BlojsomProperties;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.*;

/**
 * EditBlogUsersPlugin
 * 
 * @author czarnecki
 * @version $Id: EditBlogUsersPlugin.java,v 1.2.2.1 2005/07/21 04:30:24 johnan Exp $
 * @since blojsom 2.06
 */
public class EditBlogUsersPlugin extends BaseAdminPlugin {

    private Log _logger = LogFactory.getLog(EditBlogUsersPlugin.class);

    // Pages
    private static final String EDIT_BLOG_USERS_PAGE = "/org/blojsom/plugin/admin/templates/admin-edit-blog-users";

    // Constants
    private static final String PLUGIN_ADMIN_EDIT_USERS_IP = "plugin-admin-edit-users";
    private static final String BOOTSTRAP_DIRECTORY_IP = "bootstrap-directory";
    private static final String DEFAULT_BOOTSTRAP_DIRECTORY = "/bootstrap";
    private static final String BLOG_HOME_BASE_DIRECTORY_IP = "blog-home-base-directory";

    private static final String BLOJSOM_PLUGIN_EDIT_BLOG_USERS_MAP = "BLOJSOM_PLUGIN_EDIT_BLOG_USERS_MAP";

    // Actions
    private static final String DELETE_BLOG_USER_ACTION = "delete-blog-user";
    private static final String ADD_BLOG_USER_ACTION = "add-blog-user";

    // Form elements
    private static final String BLOG_USER_ID = "blog-user-id";
    private static final String BLOG_USER_PASSWORD = "blog-user-password";
    private static final String BLOG_USER_PASSWORD_CHECK = "blog-user-password-check";

    private String _bootstrapDirectory;
    private String _blogHomeBaseDirectory;
    private String _flavorConfiguration;
    private String _pluginConfiguration;
    private String _authorizationConfiguration;
    private String _permissionsConfiguration;
    private ServletConfig _servletConfig;
    private Map _administrators;

    /**
     * Default constructor.
     */
    public EditBlogUsersPlugin() {
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

        _servletConfig = servletConfig;
        _blojsomConfiguration = blojsomConfiguration;
        _flavorConfiguration = servletConfig.getInitParameter(BLOJSOM_FLAVOR_CONFIGURATION_IP);
        _pluginConfiguration = servletConfig.getInitParameter(BLOJSOM_PLUGIN_CONFIGURATION_IP);
        _authorizationConfiguration = servletConfig.getInitParameter(BLOG_AUTHORIZATION_IP);
        _permissionsConfiguration = servletConfig.getInitParameter(BLOG_PERMISSIONS_IP);
        if (BlojsomUtils.checkNullOrBlank(_permissionsConfiguration)) {
            _permissionsConfiguration = DEFAULT_PERMISSIONS_CONFIGURATION_FILE;
        }

        try {
            Properties configurationProperties = BlojsomUtils.loadProperties(servletConfig, PLUGIN_ADMIN_EDIT_USERS_IP, true);

            _bootstrapDirectory = configurationProperties.getProperty(BOOTSTRAP_DIRECTORY_IP);
            if (BlojsomUtils.checkNullOrBlank(_bootstrapDirectory)) {
                _bootstrapDirectory = DEFAULT_BOOTSTRAP_DIRECTORY;
            }

            _blogHomeBaseDirectory = configurationProperties.getProperty(BLOG_HOME_BASE_DIRECTORY_IP);
            if (BlojsomUtils.checkNullOrBlank(_blogHomeBaseDirectory)) {
                _blogHomeBaseDirectory = _blojsomConfiguration.getGlobalBlogHome();
                if (BlojsomUtils.checkNullOrBlank(_blogHomeBaseDirectory)) {
                    throw new BlojsomPluginException("No blog base home directory specified.");
                }
            } else {
                if (!_blogHomeBaseDirectory.endsWith("/")) {
                    _blogHomeBaseDirectory += "/";
                }
            }

            String administratorProperty = configurationProperties.getProperty(ADMINISTRATORS_IP);
            String[] administrators = BlojsomUtils.parseCommaList(administratorProperty);
            _administrators = BlojsomUtils.arrayOfStringsToMap(administrators);
        } catch (BlojsomException e) {
            _logger.error(e);
            throw new BlojsomPluginException(e);
        }
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

        // Check to see the requesting user is an administrator
        if (!_administrators.containsKey(user.getId())) {
            _logger.debug("User: " + user.getId() + " is not a valid administrator");

            httpServletRequest.setAttribute(PAGE_PARAM, ADMIN_ADMINISTRATION_PAGE);
            return entries;
        }

        context.put(BLOJSOM_PLUGIN_EDIT_BLOG_USERS_MAP, Collections.unmodifiableMap(BlojsomUtils.listToMap(BlojsomUtils.arrayToList(_blojsomConfiguration.getBlojsomUsers()))));
        String action = BlojsomUtils.getRequestValue(ACTION_PARAM, httpServletRequest);
        if (BlojsomUtils.checkNullOrBlank(action)) {
            _logger.debug("User did not request edit action");
            httpServletRequest.setAttribute(PAGE_PARAM, ADMIN_ADMINISTRATION_PAGE);
        } else if (PAGE_ACTION.equals(action)) {
            _logger.debug("User requested edit blog users page");

            httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_USERS_PAGE);
        } else if (DELETE_BLOG_USER_ACTION.equals(action)) {
            _logger.debug("User requested delete blog user action");

            String blogUserID = BlojsomUtils.getRequestValue(BLOG_USER_ID, httpServletRequest);
            if (BlojsomUtils.checkNullOrBlank(blogUserID)) {
                httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_USERS_PAGE);
                return entries;
            } else {
                _logger.debug("Deleting user: " + blogUserID);

                // Delete the user from the in-memory list
                _blojsomConfiguration.removeBlogID(blogUserID);

                File blogConfigurationDirectory = new File(_blojsomConfiguration.getInstallationDirectory() + _blojsomConfiguration.getBaseConfigurationDirectory() + blogUserID + "/");
                if (!BlojsomUtils.deleteDirectory(blogConfigurationDirectory)) {
                    _logger.error("Unable to remove blog configuration directory: " + blogConfigurationDirectory.toString());
                    addOperationResultMessage(context, "Unable to remove blog configuration for user: " + blogUserID);
                } else {
                    _logger.debug("Removed blog configuration directory: " + blogConfigurationDirectory.toString());
                }

                File blogDirectory = new File(_blogHomeBaseDirectory + blogUserID + "/");
                if (!BlojsomUtils.deleteDirectory(blogDirectory)) {
                    _logger.error("Unable to remove blog directory for user: " + blogDirectory.toString());
                    addOperationResultMessage(context, "Unable to remove blog directory for user: " + blogUserID);
                } else {
                    _logger.debug("Removed blog directory: " + blogDirectory.toString());
                }

                File blogResourcesDirectory = new File(_blojsomConfiguration.getInstallationDirectory() +
                        _blojsomConfiguration.getResourceDirectory() + blogUserID + "/");
                if (!BlojsomUtils.deleteDirectory(blogResourcesDirectory)) {
                    _logger.error("Unable to remove blog resource directory: " + blogResourcesDirectory.toString());
                    addOperationResultMessage(context, "Unable to remove resources directory for user: " + blogUserID);
                } else {
                    _logger.debug("Removed blog resource directory: " + blogResourcesDirectory.toString());
                }

                writeBlojsomConfiguration();
                _logger.debug("Wrote new blojsom configuration after deleting user: " + blogUserID);
                addOperationResultMessage(context, "Deleted user: " + blogUserID);
            }

            httpServletRequest.setAttribute(PAGE_PARAM, ADMIN_ADMINISTRATION_PAGE);
        } else if (ADD_BLOG_USER_ACTION.equals(action)) {
            _logger.debug("User requested add blog user action");

            Map blogUsers = _blojsomConfiguration.getBlogIDs();
            String blogUserID = BlojsomUtils.getRequestValue(BLOG_USER_ID, httpServletRequest);

            if (BlojsomUtils.checkNullOrBlank(blogUserID)) { // Check that we got a blog user ID
                addOperationResultMessage(context, "No blog ID specified for adding a blog");
                httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_USERS_PAGE);

                return entries;
            } else if (blogUsers.containsKey(blogUserID)) { // Check that the user does not already exist
                _logger.debug("User: " + blogUserID + " already exists");
                addOperationResultMessage(context, "Blog ID: " + blogUserID + " already exists");
                httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_USERS_PAGE);

                return entries;
            } else { // Begin the process of adding a new user
                _logger.debug("Adding new user id: " + blogUserID);

                BlogUser blogUser = new BlogUser();
                blogUser.setId(blogUserID);

                File blogUserDirectory = new File(_blojsomConfiguration.getInstallationDirectory() + _blojsomConfiguration.getBaseConfigurationDirectory() + blogUserID);
                if (blogUserDirectory.exists()) { // Make sure that the blog user ID does not conflict with a directory underneath the installation directory
                    _logger.debug("User directory already exists for blog user: " + blogUserID);
                    addOperationResultMessage(context, "User directory already exists for blog ID: " + blogUserID);
                    httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_USERS_PAGE);

                    return entries;
                } else { // Otherwise, check the authorization passwords match
                    String blogUserPassword = BlojsomUtils.getRequestValue(BLOG_USER_PASSWORD, httpServletRequest);
                    String blogUserPasswordCheck = BlojsomUtils.getRequestValue(BLOG_USER_PASSWORD_CHECK, httpServletRequest);
                    String blogBaseURL = BlojsomUtils.getRequestValue(BLOG_BASE_URL_IP, httpServletRequest);
                    String blogURL = BlojsomUtils.getRequestValue(BLOG_URL_IP, httpServletRequest);

                    // Check for the blog and blog base URLs
                    if (BlojsomUtils.checkNullOrBlank(blogURL) || BlojsomUtils.checkNullOrBlank(blogBaseURL)) {
                        _logger.debug("No blog URL or base URL supplied");
                        addOperationResultMessage(context, "No blog URL or base URL supplied");
                        httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_USERS_PAGE);

                        return entries;
                    } else {
                        if (!blogURL.endsWith("/")) {
                            blogURL += "/";
                        }
                        if (!blogBaseURL.endsWith("/")) {
                            blogBaseURL += "/";
                        }
                    }

                    // Check to see that the password and password check are equal
                    if (!blogUserPassword.equals(blogUserPasswordCheck)) {
                        _logger.debug("User password does not equal password check");
                        addOperationResultMessage(context, "User password does not equal user password check");
                        httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_USERS_PAGE);

                        return entries;
                    } else { // And if they do, copy the bootstrap directory and initialize the user
                        String bootstrap = _blojsomConfiguration.getInstallationDirectory() + _blojsomConfiguration.getBaseConfigurationDirectory() + _bootstrapDirectory + "/";
                        _logger.debug("Bootstrap directory: " + bootstrap);
                        File bootstrapDirectory = new File(bootstrap);
                        String userDirectory = _blojsomConfiguration.getInstallationDirectory() + _blojsomConfiguration.getBaseConfigurationDirectory() + blogUserID + "/";
                        _logger.debug("User directory: " + userDirectory);
                        File newUserDirectory = new File(userDirectory);

                        _logger.debug("Copying bootstrap directory: " + bootstrapDirectory.toString() + " to target user directory: " + newUserDirectory.toString());
                        try {
                            BlojsomUtils.copyDirectory(bootstrapDirectory, newUserDirectory);
                        } catch (IOException e) {
                            addOperationResultMessage(context, "Unable to copy bootstrap directory. Check log files for error");
                            httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_USERS_PAGE);
                            _logger.error(e);

                            return entries;
                        }

                        try {
                            // Configure blog
                            Properties blogProperties = BlojsomUtils.loadProperties(_servletConfig, _blojsomConfiguration.getBaseConfigurationDirectory() + blogUserID + '/' + BLOG_DEFAULT_PROPERTIES);
                            blogProperties.put(BLOG_HOME_IP, _blogHomeBaseDirectory + blogUserID);
                            File blogHomeDirectory = new File(_blogHomeBaseDirectory + blogUserID);
                            if (!blogHomeDirectory.mkdirs()) {
                                _logger.error("Unable to create blog home directory: " + blogHomeDirectory.toString());
                                addOperationResultMessage(context, "Unable to create blog home directory for blog ID: " + blogUserID);
                                httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_USERS_PAGE);

                                return entries;
                            }
                            blogProperties.put(BLOG_BASE_URL_IP, blogBaseURL);
                            blogProperties.put(BLOG_URL_IP, blogURL);

                            // Write out the blog configuration
                            File blogConfigurationFile = new File(_blojsomConfiguration.getInstallationDirectory() + _blojsomConfiguration.getBaseConfigurationDirectory() + blogUserID + '/' + BLOG_DEFAULT_PROPERTIES);
                            FileOutputStream fos = new FileOutputStream(blogConfigurationFile);
                            blogProperties.store(fos, null);
                            fos.close();
                            _logger.debug("Wrote blog configuration information for new user: " + blogConfigurationFile.toString());

                            // Set the blog information for the user
                            Blog blog = new Blog(blogProperties);
                            blogUser.setBlog(blog);

                            // Configure authorization
                            Map authorizationMap = new HashMap();
                            authorizationMap.put(blogUserID, blogUserPassword);
                            blogUser.getBlog().setAuthorization(authorizationMap);
                            _logger.debug("Set authorization information for new user: " + blogUserID);

                            // Write out the authorization
                            File blogAuthorizationFile = new File(_blojsomConfiguration.getInstallationDirectory() + _blojsomConfiguration.getBaseConfigurationDirectory() + blogUserID + '/' + _authorizationConfiguration);
                            fos = new FileOutputStream(blogAuthorizationFile);
                            Properties authorizationProperties = BlojsomUtils.mapToProperties(blogUser.getBlog().getAuthorization());
                            authorizationProperties.store(fos, null);
                            fos.close();
                            _logger.debug("Wrote blog authorization information for new user: " + blogAuthorizationFile.toString());

                            // Write out the permissions
                            File blogPermissionsFile = new File(_blojsomConfiguration.getInstallationDirectory() + _blojsomConfiguration.getBaseConfigurationDirectory() +
                                    blogUserID + "/" + _permissionsConfiguration);
                            fos = new FileOutputStream(blogPermissionsFile);
                            Properties permissionsProperties = new BlojsomProperties(true);
                            permissionsProperties.setProperty(blogUserID, "*");
                            permissionsProperties.store(fos, null);
                            fos.close();
                            _logger.debug("Wrote blog permissions information for new user: " + blogPermissionsFile.toString());

                            // Configure flavors
                            Map flavors = new HashMap();
                            Map flavorToTemplateMap = new HashMap();
                            Map flavorToContentTypeMap = new HashMap();

                            Properties flavorProperties = BlojsomUtils.loadProperties(_servletConfig, _blojsomConfiguration.getBaseConfigurationDirectory() + blogUserID + '/' + _flavorConfiguration);

                            Iterator flavorIterator = flavorProperties.keySet().iterator();
                            while (flavorIterator.hasNext()) {
                                String flavor = (String) flavorIterator.next();
                                String[] flavorMapping = BlojsomUtils.parseCommaList(flavorProperties.getProperty(flavor));
                                flavors.put(flavor, flavor);
                                flavorToTemplateMap.put(flavor, flavorMapping[0]);
                                flavorToContentTypeMap.put(flavor, flavorMapping[1]);

                            }

                            blogUser.setFlavors(flavors);
                            blogUser.setFlavorToTemplate(flavorToTemplateMap);
                            blogUser.setFlavorToContentType(flavorToContentTypeMap);
                            _logger.debug("Loaded flavor information for new user: " + blogUserID);

                            // Configure plugins
                            Map pluginChainMap = new HashMap();

                            Properties pluginProperties = BlojsomUtils.loadProperties(_servletConfig, _blojsomConfiguration.getBaseConfigurationDirectory() + blogUserID + '/' + _pluginConfiguration);
                            Iterator pluginIterator = pluginProperties.keySet().iterator();
                            while (pluginIterator.hasNext()) {
                                String plugin = (String) pluginIterator.next();
                                if (plugin.indexOf(BLOJSOM_PLUGIN_CHAIN) != -1) {
                                    pluginChainMap.put(plugin, BlojsomUtils.parseCommaList(pluginProperties.getProperty(plugin)));
                                    _logger.debug("Added plugin chain: " + plugin + '=' + pluginProperties.getProperty(plugin) + " for user: " + blogUserID);
                                }
                            }

                            blogUser.setPluginChain(pluginChainMap);
                            _logger.debug("Loaded plugin chain map for new user: " + blogUserID);
                        } catch (BlojsomException e) {
                            _logger.error(e);
                        } catch (IOException e) {
                            _logger.error(e);
                        }

                        // Add the resources directory
                        File blogResourcesDirectory = new File(_blojsomConfiguration.getInstallationDirectory() +
                            _blojsomConfiguration.getResourceDirectory() + blogUserID + "/");
                        File bootstrapResourcesDirectory = new File(bootstrapDirectory, _blojsomConfiguration.getResourceDirectory());
                        if (!blogResourcesDirectory.mkdirs()) {
                            _logger.error("Unable to create blog resource directory: " + blogResourcesDirectory.toString());
                        } else {
                            _logger.debug("Added blog resource directory: " + blogResourcesDirectory.toString());
                        }
                        try {
                            if (bootstrapResourcesDirectory.exists()) {
                                BlojsomUtils.copyDirectory(bootstrapResourcesDirectory, blogResourcesDirectory);
                            }

                            // Cleanup the bootstrap resources directory
                            File resourcesDirectoryToDelete = new File(_blojsomConfiguration.getInstallationDirectory() +
                                    _blojsomConfiguration.getBaseConfigurationDirectory() + blogUserID +
                                    _blojsomConfiguration.getResourceDirectory());
                            if (resourcesDirectoryToDelete.exists()) {
                                BlojsomUtils.deleteDirectory(resourcesDirectoryToDelete);
                            }
                        } catch (IOException e) {
                            _logger.error(e);
                        }

                        // Add the user to the global list of users
                        _blojsomConfiguration.addBlogID(blogUserID);
                        writeBlojsomConfiguration();
                        _logger.debug("Wrote new blojsom configuration after adding new user: " + blogUserID);

                        addOperationResultMessage(context, "Added new blog: " + blogUserID);
                    }
                }
            }
        }

        return entries;
    }

    /**
     * Write out the update blojsom configuration file. This is done after adding or deleting a new user.
     */
    private void writeBlojsomConfiguration() {
        File blojsomConfigurationFile = new File(_blojsomConfiguration.getInstallationDirectory() + _blojsomConfiguration.getBaseConfigurationDirectory() + "blojsom.properties");
        Properties configurationProperties = new BlojsomProperties(true);

        configurationProperties.put(BLOJSOM_USERS_IP, BlojsomUtils.arrayToList(_blojsomConfiguration.getBlojsomUsers()));
        configurationProperties.put(BLOJSOM_FETCHER_IP, _blojsomConfiguration.getFetcherClass());
        configurationProperties.put(BLOJSOM_DEFAULT_USER_IP, _blojsomConfiguration.getDefaultUser());
        configurationProperties.put(BLOJSOM_INSTALLATION_DIRECTORY_IP, _blojsomConfiguration.getInstallationDirectory());
        configurationProperties.put(BLOJSOM_CONFIGURATION_BASE_DIRECTORY_IP, _blojsomConfiguration.getBaseConfigurationDirectory());
        configurationProperties.put(BLOJSOM_BLOG_HOME_IP, _blojsomConfiguration.getGlobalBlogHome());
        configurationProperties.put(BLOJSOM_TEMPLATES_DIRECTORY_IP, _blojsomConfiguration.getTemplatesDirectory());
        configurationProperties.put(BLOJSOM_RESOURCE_DIRECTORY_IP, _blojsomConfiguration.getResourceDirectory());
        configurationProperties.put(BLOJSOM_RESOURCE_MANAGER_IP, _blojsomConfiguration.getResourceManager());
        configurationProperties.put(BLOJSOM_RESOURCE_MANAGER_BUNDLES_IP, "org.blojsom.plugin.admin.resources.messages");
        configurationProperties.put(BLOJSOM_BROADCASTER_IP, _blojsomConfiguration.getEventBroadcaster().getClass().getName());

        try {
            FileOutputStream fos = new FileOutputStream(blojsomConfigurationFile);
            configurationProperties.store(fos, null);
            fos.close();
        } catch (IOException e) {
            _logger.error(e);
        }
    }
}
