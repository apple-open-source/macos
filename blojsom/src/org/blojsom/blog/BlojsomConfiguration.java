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
package org.blojsom.blog;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.BlojsomException;
import org.blojsom.authorization.AuthorizationProvider;
import org.blojsom.event.BlojsomEventBroadcaster;
import org.blojsom.event.BlojsomListener;
import org.blojsom.util.BlojsomConstants;
import org.blojsom.util.BlojsomProperties;
import org.blojsom.util.BlojsomUtils;

import javax.servlet.ServletConfig;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.*;

/**
 * BlojsomConfiguration
 *
 * @author David Czarnecki
 * @version $Id: BlojsomConfiguration.java,v 1.2.2.1 2005/07/21 14:11:02 johnan Exp $
 * @since blojsom 2.0
 */
public class BlojsomConfiguration implements BlojsomConstants {

    private Log _logger = LogFactory.getLog(BlojsomConfiguration.class);

    private String _blojsomUsers;
    private String _defaultUser;
    private String _baseConfigurationDirectory;
    private String _fetcherClass;
    private String _installationDirectory;
    private String _templatesDirectory;
    private String _resourceDirectory;
    private String _qualifiedResourceDirectory;
    private String _resourceManager;
    private String _authorizationProviderClass;
    private static AuthorizationProvider _authorizationProvider = null;
    private String _globalBlogHome;
    private static BlojsomEventBroadcaster _eventBroadcaster = null;
    private String _installedLocales;
    private ServletConfig _servletConfig;

    private Map _blogUsers;
    private Map _blojsomConfiguration;
    private Map _blogIDs;

    /**
     * Initialize the BlojsomConfiguration object
     *
     * @param servletConfig        Servlet configuration information
     * @param blojsomConfiguration Map of loaded blojsom properties
     */
    public BlojsomConfiguration(ServletConfig servletConfig, Map blojsomConfiguration) throws BlojsomConfigurationException {
        _blojsomConfiguration = blojsomConfiguration;
        _servletConfig = servletConfig;

        _installationDirectory = servletConfig.getServletContext().getRealPath("/");
        if (BlojsomUtils.checkNullOrBlank(_installationDirectory)) {
            _logger.error("No installation directory set for blojsom");
            throw new BlojsomConfigurationException("No installation directory set for blojsom");
        } else {
            if (!_installationDirectory.endsWith("/")) {
                _installationDirectory += "/";
            }
        }
        try {
            System.setProperty("blojsom.installation.directory", _installationDirectory);
        } catch (Exception e) {
            _logger.error(e);
        }
        _logger.debug("Using installation directory: " + _installationDirectory);

        _baseConfigurationDirectory = getBlojsomPropertyAsString(BLOJSOM_CONFIGURATION_BASE_DIRECTORY_IP);
        if (BlojsomUtils.checkNullOrBlank(_baseConfigurationDirectory)) {
            _baseConfigurationDirectory = BLOJSOM_DEFAULT_CONFIGURATION_BASE_DIRECTORY;
        } else {
            _baseConfigurationDirectory = BlojsomUtils.checkStartingAndEndingSlash(_baseConfigurationDirectory);
        }
        _baseConfigurationDirectory = _baseConfigurationDirectory.trim();
        _logger.debug("Using base configuration directory: " + _baseConfigurationDirectory);

        _templatesDirectory = getBlojsomPropertyAsString(BLOJSOM_TEMPLATES_DIRECTORY_IP);
        if (BlojsomUtils.checkNullOrBlank(_templatesDirectory)) {
            _templatesDirectory = BLOJSOM_DEFAULT_TEMPLATES_DIRECTORY;
        } else {
            if (!_templatesDirectory.startsWith("/")) {
                _templatesDirectory = '/' + _templatesDirectory;
            }

        }
        _templatesDirectory = _templatesDirectory.trim();
        _logger.debug("Using templates directory: " + _templatesDirectory);

        _resourceDirectory = getBlojsomPropertyAsString(BLOJSOM_RESOURCE_DIRECTORY_IP);
        if (BlojsomUtils.checkNullOrBlank(_resourceDirectory)) {
            _resourceDirectory = BLOJSOM_DEFAULT_RESOURCE_DIRECTORY;
        } else {
            if (!_resourceDirectory.startsWith("/")) {
                _resourceDirectory = '/' + _resourceDirectory;
            }

        }
        _resourceDirectory = _resourceDirectory.trim();
        _logger.debug("Using resources directory: " + _resourceDirectory);

        String eventBroadcaster = getBlojsomPropertyAsString(BLOJSOM_BROADCASTER_IP);
        if (BlojsomUtils.checkNullOrBlank(eventBroadcaster)) {
            eventBroadcaster = BLOJSOM_DEFAULT_BROADCASTER;
        }

        try {
            Class broadcasterClass = Class.forName(eventBroadcaster);
            if (_eventBroadcaster == null) {
                _eventBroadcaster = (BlojsomEventBroadcaster) broadcasterClass.newInstance();
            }
            _logger.debug("Using event broadcaster: " + eventBroadcaster);

            String listenerConfiguration = servletConfig.getInitParameter(BLOJSOM_LISTENER_CONFIGURATION_IP);
            if (!BlojsomUtils.checkNullOrBlank(listenerConfiguration)) {
                Properties listenerProperties = BlojsomUtils.loadProperties(servletConfig, BLOJSOM_LISTENER_CONFIGURATION_IP, true);
                Iterator listenerIterator = listenerProperties.keySet().iterator();
                while (listenerIterator.hasNext()) {
                    Object key = listenerIterator.next();
                    String listenerClassname = listenerProperties.getProperty(key.toString());
                    Class listenerClass = Class.forName(listenerClassname);
                    BlojsomListener listener = (BlojsomListener) listenerClass.newInstance();
                    _eventBroadcaster.addListener(listener);
                }
            }
        } catch (BlojsomException e) {
            _logger.error(e);
            throw new BlojsomConfigurationException(e);
        } catch (ClassNotFoundException e) {
            _logger.error(e);
            throw new BlojsomConfigurationException("Unable to instantiate event broadcaster: " + eventBroadcaster, e);
        } catch (InstantiationException e) {
            _logger.error(e);
            throw new BlojsomConfigurationException("Unable to instantiate event broadcaster: " + eventBroadcaster, e);
        } catch (IllegalAccessException e) {
            _logger.error(e);
            throw new BlojsomConfigurationException("Unable to instantiate event broadcaster: " + eventBroadcaster, e);
        }

        // Ensure the resource directory physically exists
        _qualifiedResourceDirectory = servletConfig.getServletContext().getRealPath(_resourceDirectory);
        _logger.debug("Using qualified resource directory: " + _qualifiedResourceDirectory);

        // Configure a global blog home directory
        _globalBlogHome = getBlojsomPropertyAsString(BLOJSOM_BLOG_HOME_IP);
        if (!BlojsomUtils.checkNullOrBlank(_globalBlogHome)) {
            if (_globalBlogHome.startsWith("{")) {
                int closingBraceIndex = _globalBlogHome.indexOf("}");
                String property = _globalBlogHome.substring(1, closingBraceIndex);
                property = System.getProperty(property);
                if (BlojsomUtils.checkNullOrBlank(property)) {
                    _logger.error("Global blog home directory property not found: " + property);
                    _globalBlogHome = null;
                } else {
                    String afterProperty = _globalBlogHome.substring(closingBraceIndex + 1);
                    _globalBlogHome = property + afterProperty;
                    // Normalize the blog-home path
                    _globalBlogHome = BlojsomUtils.replace(_globalBlogHome, "\\", "/");
                }
            }

            if (!BlojsomUtils.checkNullOrBlank(_globalBlogHome)) {
                _globalBlogHome = _globalBlogHome.trim();
                if (!_globalBlogHome.endsWith("/")) {
                    _globalBlogHome += "/";
                }

                File blogHomeDirectory = new File(_globalBlogHome);
                if (!blogHomeDirectory.exists()) {
                    if (!blogHomeDirectory.mkdirs()) {
                        _logger.error("Unable to create global blog home directory: " + _globalBlogHome);
                        _globalBlogHome = null;
                    }
                }
            }

            if (!BlojsomUtils.checkNullOrBlank(_globalBlogHome)) {
                _logger.debug("Using global blog-home directory: " + _globalBlogHome);
            }
        } else {
            _globalBlogHome = "";
        }

        String[] users;
        Object listOfUsers = getBlojsomProperty(BLOJSOM_USERS_IP);
        if (listOfUsers instanceof List) {
            List blojsomUsers = getBlojsomPropertyAsList(BLOJSOM_USERS_IP);
            users = (String[]) blojsomUsers.toArray(new String[blojsomUsers.size()]);
            _blojsomUsers = BlojsomUtils.arrayOfStringsToString(users);
        } else {
            _blojsomUsers = getBlojsomPropertyAsString(BLOJSOM_USERS_IP);
            users = BlojsomUtils.parseCommaList(_blojsomUsers);
        }

        if (users.length == 0) {
            _logger.error("No users defined for this blojsom blog");
            throw new BlojsomConfigurationException("No users defined for this blojsom blog");
        } else {
            _blogUsers = new HashMap();
            _blogIDs = new HashMap();
            for (int i = 0; i < users.length; i++) {
                String user = users[i];

                _blogIDs.put(user, user);
            }

            // Determine and set the default user
            String defaultUser = getBlojsomPropertyAsString(BLOJSOM_DEFAULT_USER_IP);
            if (BlojsomUtils.checkNullOrBlank(defaultUser)) {
                _logger.error("No default user defined in configuration property: " + BLOJSOM_DEFAULT_USER_IP);
                throw new BlojsomConfigurationException("No default user defined in configuration property: " + BLOJSOM_DEFAULT_USER_IP);
            }

            if (!_blogIDs.containsKey(defaultUser)) {
                _logger.error("Default user does not match any of the registered blojsom users: " + defaultUser);
                throw new BlojsomConfigurationException("Default user does not match any of the registered blojsom users: " + defaultUser);
            }

            _defaultUser = defaultUser;
            _logger.debug("blojsom default user: " + _defaultUser);

            _fetcherClass = getBlojsomPropertyAsString(BLOJSOM_FETCHER_IP);
            if ((_fetcherClass == null) || "".equals(_fetcherClass)) {
                _fetcherClass = BLOG_DEFAULT_FETCHER;
            }
        }

        _resourceManager = getBlojsomPropertyAsString(BLOJSOM_RESOURCE_MANAGER_IP);
        if (BlojsomUtils.checkNullOrBlank(_resourceManager)) {
            _resourceManager = BLOJSOM_DEFAULT_RESOURCE_MANAGER;
        }
        _logger.debug("Using resource manager: " + _resourceManager);

        _installedLocales = getBlojsomPropertyAsString(BLOJSOM_INSTALLED_LOCALES_IP);
        if (BlojsomUtils.checkNullOrBlank(_installedLocales)) {
            _installedLocales = BLOG_LANGUAGE_DEFAULT + "_" + BLOG_COUNTRY_DEFAULT;
        }
        _logger.debug("Using installed locales: " + _installedLocales);

        _authorizationProviderClass = getBlojsomPropertyAsString(BLOJSOM_AUTHORIZATION_PROVIDER_IP);
        if (BlojsomUtils.checkNullOrBlank(_authorizationProviderClass)) {
            _authorizationProviderClass = DEFAULT_AUTHORIZATION_PROVIDER;
        }
        _logger.debug("Using authorization provider: " + _authorizationProviderClass);

         try {
            Class authorizationProviderClass = Class.forName(_authorizationProviderClass);
            _authorizationProvider = (AuthorizationProvider) authorizationProviderClass.newInstance();
            _authorizationProvider.init(_servletConfig, this);
        } catch (ClassNotFoundException e) {
            throw new BlojsomConfigurationException(e);
        } catch (InstantiationException e) {
            throw new BlojsomConfigurationException(e);
        } catch (IllegalAccessException e) {
            throw new BlojsomConfigurationException(e);
        } catch (BlojsomConfigurationException e) {
            throw new BlojsomConfigurationException(e);
        }
    }

    /**
     * Returns an unmodifiable map of the blojsom configuration properties
     *
     * @return Unmodifiable map of the blojsom configuration properties
     */
    public Map getBlojsomConfiguration() {
        return Collections.unmodifiableMap(_blojsomConfiguration);
    }

    /**
     * Retrieve a blojsom property as a string
     *
     * @param propertyKey Property key
     * @return Value of blojsom property as a string or <code>null</code> if no property key is found
     */
    public String getBlojsomPropertyAsString(String propertyKey) {
        if (_blojsomConfiguration.containsKey(propertyKey)) {
            return (String) _blojsomConfiguration.get(propertyKey);
        }

        return null;
    }

    /**
     * Retrieve a property from <code>blojsom.properties</code> as a {@link List}
     *
     * @param propertyKey Key
     * @return {@link List} for property or <code>null</code> if property does not exist
     */
    public List getBlojsomPropertyAsList(String propertyKey) {
        if (_blojsomConfiguration.containsKey(propertyKey)) {
            Object value = _blojsomConfiguration.get(propertyKey);
            if (value instanceof List) {
                return (List) value;
            } else {
                ArrayList values = new ArrayList();
                values.add(value);

                return values;
            }
        }

        return null;
    }

    /**
     * Return a blojsom configuration property
     *
     * @param propertyKey Property key
     * @return Value of blojsom property
     */
    public Object getBlojsomProperty(String propertyKey) {
        return _blojsomConfiguration.get(propertyKey);
    }

    /**
     * Get the default user for this blojsom instance
     *
     * @return Default user
     */
    public String getDefaultUser() {
        return _defaultUser;
    }

    /**
     * Get the base directory for obtaining configuration information
     *
     * @return Configuration base directory (e.g. /WEB-INF)
     */
    public String getBaseConfigurationDirectory() {
        return _baseConfigurationDirectory;
    }

    /**
     * Get the classname of the fetcher used for this blojsom instance
     *
     * @return Fetcher classname
     */
    public String getFetcherClass() {
        return _fetcherClass;
    }

    /**
     * Get the installation directory for blojsom. This is the directory where the blojsom WAR file will
     * be unpacked.
     *
     * @return Installation directory
     * @since blojsom 2.01
     */
    public String getInstallationDirectory() {
        return _installationDirectory;
    }

    /**
     * Get the directory where templates will be located off the user's directory.
     *
     * @return Templates directory
     * @since blojsom 2.04
     */
    public String getTemplatesDirectory() {
        return _templatesDirectory;
    }

    /**
     * Get the directory where resources will be located off the installed directory.
     *
     * @return Resources directory
     * @since blojsom 2.14
     */
    public String getResourceDirectory() {
        return _resourceDirectory;
    }

    /**
     * Get the fully qualified directory where resources will be located off the installed directory.
     *
     * @return Resources directory
     * @since blojsom 2.14
     */
    public String getQualifiedResourceDirectory() {
        return _qualifiedResourceDirectory;
    }

    /**
     * Get the list of users for this blojsom instance returned as a String[]
     *
     * @return List of users as a String[]
     */
    public String[] getBlojsomUsers() {
        Iterator blogs = _blogIDs.keySet().iterator();
        ArrayList blogsList = new ArrayList();
        while (blogs.hasNext()) {
            String blogID = (String) blogs.next();
            blogsList.add(blogID);
        }

        return (String[]) blogsList.toArray(new String[blogsList.size()]);
    }

    /**
     * Get a map of the {@link BlogUser} objects
     *
     * @return Map of {@link BlogUser} objects
     * @deprecated
     * @see {@link #getBlojsomUsers()}  
     */
    public Map getBlogUsers() {
        return _blogUsers;
    }

    /**
     * Get the name of the resource manager class
     *
     * @return Classname of the resource manager
     * @since blojsom 2.13
     */
    public String getResourceManager() {
        return _resourceManager;
    }

    /**
     * Get the name of the authorization provider class
     *
     * @return Classname of the authorization provider
     * @since blojsom 2.16
     */
    public String getAuthorizationProvider() {
        return _authorizationProviderClass;
    }

    /**
     * Get the name of the global blog home directory
     *
     * @return Global blog home directory
     * @since blojsom 2.18
     */
    public String getGlobalBlogHome() {
        return _globalBlogHome;
    }

    /**
     * Get the {@link BlojsomEventBroadcaster} in use to broadcast events
     *
     * @return {@link BlojsomEventBroadcaster}
     * @since blojsom 2.18
     */
    public BlojsomEventBroadcaster getEventBroadcaster() {
        return _eventBroadcaster;
    }

    /**
     * Get the installed locales for this blojsom installation
     *
     * @return Array of locale strings installed for this blojsom installation
     * @since blojsom 2.21
     */
    public String[] getInstalledLocalesAsStrings() {
        return BlojsomUtils.parseCommaList(_installedLocales);
    }

    /**
     * Get the installed locales as {@link Locale} objects
     *
     * @return Array of {@link Locale} objects
     * @since blojsom 2.21
     */
    public Locale[] getInstalledLocales() {
        String[] localeStrings = getInstalledLocalesAsStrings();
        Locale[] locales = new Locale[localeStrings.length];

        for (int i = 0; i < localeStrings.length; i++) {
            String localeString = localeStrings[i];
            locales[i] = BlojsomUtils.getLocaleFromString(localeString);
        }

        return locales;
    }

    /**
     * Check to see if a blog ID exists in the known blog IDs
     *
     * @param blogID Blog ID
     * @return <code>true</code> if the blog ID exists, <code>false</code> otherwise
     * @since blojsom 2.24
     */
    public boolean checkBlogIDExists(String blogID) {
        return _blogIDs.containsKey(blogID);
    }

    /**
     * Load a {@link BlogUser} for a given blog ID
     *
     * @param blogID Blog ID
     * @return {@link BlogUser} object
     * @throws BlojsomException If there is an exception loading the {@link BlogUser object}
     * @since blojsom 2.24
     */
    public BlogUser loadBlog(String blogID) throws BlojsomException {
        InputStream is;

        BlogUser blogUser = new BlogUser();
        blogUser.setId(blogID);

        Properties userProperties = new BlojsomProperties();
        _logger.info("Attemping to load " + _baseConfigurationDirectory + blogID + '/' + BLOG_DEFAULT_PROPERTIES);
        is = _servletConfig.getServletContext().getResourceAsStream(_baseConfigurationDirectory + blogID + '/' + BLOG_DEFAULT_PROPERTIES);
        if (is != null) {
            try {
                userProperties.load(is);
                is.close();
            } catch (IOException e) {
                _logger.error(e);
                throw new BlojsomConfigurationException(e);
            }

            Blog userBlog = null;
            try {
                // If a global blog-home directory has been defined, use it for each user
                if (!BlojsomUtils.checkNullOrBlank(_globalBlogHome) &&
                        !userProperties.containsKey(BLOG_HOME_IP)) {
                    String usersBlogHome = _globalBlogHome + blogID + "/";
                    File blogHomeDirectory = new File(usersBlogHome);
                    if (!blogHomeDirectory.exists()) {
                        if (!blogHomeDirectory.mkdirs()) {
                            _logger.error("Unable to create blog-home directory for blog: " + blogHomeDirectory.toString());
                            throw new BlojsomConfigurationException("Unable to create blog-home directory for blog: " + blogHomeDirectory.toString());
                        }
                    }

                    userProperties.setProperty(BLOG_HOME_IP, usersBlogHome);
                    _logger.debug("Setting blog blog-home directory: " + usersBlogHome);
                }

                userBlog = new Blog(userProperties);
                blogUser.setBlog(userBlog);

                _authorizationProvider.loadAuthenticationCredentials(blogUser);
                
                _logger.debug("Added blojsom blog: " + blogUser.getId());

                // Ensure the resource directory for the user physically exists
                File resourceDirectory = new File(_qualifiedResourceDirectory + File.separator + blogID);
                if (!resourceDirectory.exists()) {
                    _logger.debug("Creating resource directory for blog " + blogID);
                    resourceDirectory.mkdirs();
                }
            } catch (BlojsomConfigurationException e) {
                _logger.error(e);
                _logger.error("Marking blog as invalid: " + blogUser.getId());

                throw new BlojsomConfigurationException(e);
            }
        } else {
            throw new BlojsomConfigurationException("Unable to load blog configuration for blog: " + blogUser.getId());
        }

        return blogUser;
    }

    /**
     * Retrieve a {@link Map} of the known blog IDs
     *
     * @return {@link Map} of the known blog IDs
     * @since blojsom 2.24
     */
    public Map getBlogIDs() {
        return Collections.unmodifiableMap(_blogIDs);
    }

    /**
     * Add a blog ID to the known blog IDs
     *
     * @param blogID Blog ID
     * @since blojsom 2.24
     */
    public void addBlogID(String blogID) {
        _blogIDs.put(blogID, blogID);
    }

    /**
     * Remove a blog ID from the known blog IDs
     *
     * @param blogID Blog ID
     * @since blojsom 2.24
     */
    public void removeBlogID(String blogID) {
        _blogIDs.remove(blogID);
    }
}
