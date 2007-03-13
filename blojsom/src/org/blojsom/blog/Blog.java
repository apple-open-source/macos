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
import org.blojsom.util.BlojsomConstants;
import org.blojsom.util.BlojsomUtils;

import com.apple.blojsom.util.BlojsomAppleUtils;

import java.io.File;
import java.util.*;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

/**
 * Blog
 * 
 * @author David Czarnecki
 * @author Mark Lussier
 * @author Dan Morrill
 * @version $Id: Blog.java,v 1.13.2.2 2006/10/19 21:14:32 johnan Exp $
 */
public class Blog implements BlojsomConstants {

    private Log _logger = LogFactory.getLog(Blog.class);

    private String _blogHome;
    private String _blogName;
    private String _blogDescription;
    private String _blogURL;
    private String _blogAdminURL;
    private String _blogBaseURL;
    private String _blogCountry;
    private String _blogLanguage;
    private String _blogAdministrationLocale;
    private String[] _blogFileExtensions;
    private String[] _blogPropertiesExtensions;
    private int _blogDepth;
    private int _blogDisplayEntries;
    private String[] _blogDefaultCategoryMappings;
    private String[] _blogDirectoryFilter;
    private String _blogOwner;
    private String _blogOwnerEmail;
    private String _blogCommentsDirectory;
    private Boolean _blogCommentsEnabled;
    private Boolean _blogEmailEnabled;
    private Boolean _blogTrackbacksEnabled;
    private Boolean _blogPingbacksEnabled;
    private String _blogTrackbackDirectory;
    private String _blogEntryMetaDataExtension;
    private String _blogFileEncoding;
    private String _blogDefaultFlavor;
    private Boolean _linearNavigationEnabled;
    private Boolean _xmlrpcEnabled;
    private String _blogPingbacksDirectory;
    private Boolean _useEncryptedPasswords;
    private String _digestAlgorithm;
	private String _blogDefaultStyleSheet;

    private Map _blogProperties;

    private Map _authorization = null;

    /**
     * Create a blog with the supplied configuration properties
     * 
     * @param blogConfiguration Blog configuration properties
     * @throws BlojsomConfigurationException If there is an error configuring the blog
     */
    public Blog(Properties blogConfiguration) throws BlojsomConfigurationException {
        _blogProperties = new HashMap();

        // Load the blog properties with all the keys/values even though some will be overridden
        Iterator keyIterator = blogConfiguration.keySet().iterator();
        while (keyIterator.hasNext()) {
            String key = (String) keyIterator.next();
            String propertyValue = blogConfiguration.getProperty(key);
            _blogProperties.put(key, propertyValue);
        }

        _blogHome = blogConfiguration.getProperty(BLOG_HOME_IP);
        if (BlojsomUtils.checkNullOrBlank(_blogHome)) {
            _logger.error("No value supplied for blog-home");
            throw new BlojsomConfigurationException("No valued supplied for blog-home");
        }
        _blogHome = _blogHome.trim();
        if (!_blogHome.endsWith("/")) {
            _blogHome += "/";
        }
        _blogProperties.put(BLOG_HOME_IP, _blogHome);

        _blogLanguage = blogConfiguration.getProperty(BLOG_LANGUAGE_IP);
        if (_blogLanguage == null) {
            _logger.info("No value supplied for blog-language. Defaulting to: " + BLOG_LANGUAGE_DEFAULT);
            _blogLanguage = BLOG_LANGUAGE_DEFAULT;
        }
        _blogProperties.put(BLOG_LANGUAGE_IP, _blogLanguage);

        _blogCountry = blogConfiguration.getProperty(BLOG_COUNTRY_IP);
        if (_blogCountry == null) {
            _logger.info("No value supplied for blog-country. Defaulting to: " + BLOG_COUNTRY_DEFAULT);
            _blogCountry = BLOG_COUNTRY_DEFAULT;
        }
        _blogProperties.put(BLOG_COUNTRY_IP, _blogCountry);

        _blogDescription = blogConfiguration.getProperty(BLOG_DESCRIPTION_IP);
        if (_blogDescription == null) {
            _logger.info("No value supplied for blog-description");
            _blogDescription = "";
        }
        _blogProperties.put(BLOG_DESCRIPTION_IP, _blogDescription);

        _blogName = blogConfiguration.getProperty(BLOG_NAME_IP);
        if (_blogName == null) {
            _logger.info("No value supplied for blog-name");
            _blogName = "";
        }
		else if (COMPUTER_NAME.equals(_blogName)) {
			_blogName = BlojsomAppleUtils.getComputerName();
		}
        _blogProperties.put(BLOG_NAME_IP, _blogName);

        _blogDepth = Integer.parseInt(blogConfiguration.getProperty(BLOG_DEPTH_IP, Integer.toString(INFINITE_BLOG_DEPTH)));
        _blogProperties.put(BLOG_DEPTH_IP, new Integer(_blogDepth));

        _blogURL = blogConfiguration.getProperty(BLOG_URL_IP);
        if (BlojsomUtils.checkNullOrBlank(_blogURL)) {
            _logger.info("No value supplied for blog-url");
        } else {
            if (!_blogURL.endsWith("/")) {
                _blogURL += "/";
            }
        }
        _blogProperties.put(BLOG_URL_IP, _blogURL);

        _blogAdminURL = blogConfiguration.getProperty(BLOG_ADMIN_URL_IP);
        if (BlojsomUtils.checkNullOrBlank(_blogAdminURL)) {
            _logger.info("No value supplied for blog-admin-url");
            _blogAdminURL = _blogURL;
        } else {
            if (!_blogAdminURL.endsWith("/")) {
                _blogAdminURL += "/";
            }
        }
        _blogProperties.put(BLOG_ADMIN_URL_IP, _blogAdminURL);

        _blogBaseURL = blogConfiguration.getProperty(BLOG_BASE_URL_IP);
        if (BlojsomUtils.checkNullOrBlank(_blogBaseURL)) {
            _logger.info("No value supplied for blog-base-url");
        } else {
            if (_blogBaseURL.endsWith("/")) {
                _blogBaseURL = _blogBaseURL.substring(0, _blogBaseURL.length() - 1);
            }
        }
        _blogProperties.put(BLOG_BASE_URL_IP, _blogBaseURL);

        _blogFileExtensions = BlojsomUtils.parseCommaList(blogConfiguration.getProperty(BLOG_FILE_EXTENSIONS_IP));
        _blogProperties.put(BLOG_FILE_EXTENSIONS_IP, blogConfiguration.getProperty(BLOG_FILE_EXTENSIONS_IP));

        String blogPropertiesExtensions = blogConfiguration.getProperty(BLOG_PROPERTIES_EXTENSIONS_IP);
        if (BlojsomUtils.checkNullOrBlank(blogPropertiesExtensions)) {
            _blogPropertiesExtensions = DEFAULT_PROPERTIES_EXTENSIONS;
        }
        _blogPropertiesExtensions = BlojsomUtils.parseCommaList(blogPropertiesExtensions);
        _blogProperties.put(BLOG_PROPERTIES_EXTENSIONS_IP, _blogPropertiesExtensions);

        _blogEntryMetaDataExtension = blogConfiguration.getProperty(BLOG_ENTRY_META_DATA_EXTENSION_IP);
        _blogProperties.put(BLOG_ENTRY_META_DATA_EXTENSION_IP, _blogEntryMetaDataExtension);

        _blogDisplayEntries = Integer.parseInt(blogConfiguration.getProperty(BLOG_ENTRIES_DISPLAY_IP, Integer.toString(BLOG_ENTRIES_DISPLAY_DEFAULT)));
        _blogProperties.put(BLOG_ENTRIES_DISPLAY_IP, new Integer(_blogDisplayEntries));

        String blogDefaultCategoryMapping = blogConfiguration.getProperty(BLOG_DEFAULT_CATEGORY_MAPPING_IP);
        if (BlojsomUtils.checkNullOrBlank(blogDefaultCategoryMapping)) {
            _blogDefaultCategoryMappings = null;
            _logger.debug("No mapping supplied for the default category '/'");
        } else {
            _blogDefaultCategoryMappings = BlojsomUtils.parseCommaList(blogDefaultCategoryMapping);
            _logger.debug(_blogDefaultCategoryMappings.length + " directories mapped to the default category '/'");
            if (_blogDefaultCategoryMappings.length == 0) {
                _blogDefaultCategoryMappings = null;
            }
        }
        _blogProperties.put(BLOG_DEFAULT_CATEGORY_MAPPING_IP, blogDefaultCategoryMapping);

        _blogCommentsDirectory = blogConfiguration.getProperty(BLOG_COMMENTS_DIRECTORY_IP);
        if ((_blogCommentsDirectory == null) || ("".equals(_blogCommentsDirectory))) {
            _blogCommentsDirectory = DEFAULT_COMMENTS_DIRECTORY;
        }
        _logger.debug("blojsom comments directory: " + _blogCommentsDirectory);
        _blogProperties.put(BLOG_COMMENTS_DIRECTORY_IP, _blogCommentsDirectory);

        String commentsDirectoryRegex;

        commentsDirectoryRegex = ".*" + File.separator + _blogCommentsDirectory;

        _blogTrackbackDirectory = blogConfiguration.getProperty(BLOG_TRACKBACK_DIRECTORY_IP);
        if ((_blogTrackbackDirectory == null) || ("".equals(_blogTrackbackDirectory))) {
            _blogTrackbackDirectory = DEFAULT_TRACKBACK_DIRECTORY;
        }
        _logger.debug("blojsom trackback directory: " + _blogTrackbackDirectory);
        _blogProperties.put(BLOG_TRACKBACK_DIRECTORY_IP, _blogTrackbackDirectory);

        String trackbackDirectoryRegex;

        trackbackDirectoryRegex = ".*" + File.separator + _blogTrackbackDirectory;

        _blogPingbacksDirectory = blogConfiguration.getProperty(BLOG_PINGBACKS_DIRECTORY_IP);
        if (BlojsomUtils.checkNullOrBlank(_blogPingbacksDirectory)) {
            _blogPingbacksDirectory = DEFAULT_PINGBACKS_DIRECTORY;
        }
        _logger.debug("blojsom pingbacks directory: " + _blogPingbacksDirectory);
        _blogProperties.put(BLOG_PINGBACKS_DIRECTORY_IP, _blogPingbacksDirectory);

        String pingbacksDirectoryRegex;

        pingbacksDirectoryRegex = ".*" + File.separator + _blogPingbacksDirectory;

        String blogDirectoryFilter = blogConfiguration.getProperty(BLOG_DIRECTORY_FILTER_IP);
        // Add the blog comments and trackback directories to the blog directory filter
        if (BlojsomUtils.checkNullOrBlank(blogDirectoryFilter)) {
            blogDirectoryFilter = commentsDirectoryRegex + ", " + trackbackDirectoryRegex + ", " + pingbacksDirectoryRegex;
        } else {
            if (blogDirectoryFilter.indexOf(commentsDirectoryRegex) == -1) {
                blogDirectoryFilter += ", " + commentsDirectoryRegex;
            }

            if (blogDirectoryFilter.indexOf(trackbackDirectoryRegex) == -1) {
                blogDirectoryFilter += ", " + trackbackDirectoryRegex;
            }

            if (blogDirectoryFilter.indexOf(pingbacksDirectoryRegex) == -1) {
                blogDirectoryFilter += ", " + pingbacksDirectoryRegex;
            }
        }
        _logger.debug("Comments directory regex: " + commentsDirectoryRegex);
        _logger.debug("Trackbacks directory regex: " + trackbackDirectoryRegex);
        _logger.debug("Pingbacks directory regex: " + pingbacksDirectoryRegex);

        _blogDirectoryFilter = BlojsomUtils.parseCommaList(blogDirectoryFilter);
        for (int i = 0; i < _blogDirectoryFilter.length; i++) {
            _logger.debug("blojsom to filter: " + _blogDirectoryFilter[i]);
        }
        _logger.debug("blojsom filtering " + _blogDirectoryFilter.length + " directories");
        _blogProperties.put(BLOG_DIRECTORY_FILTER_IP, _blogDirectoryFilter);

        _blogOwner = blogConfiguration.getProperty(BLOG_OWNER);
        _blogProperties.put(BLOG_OWNER, _blogOwner);
                                                                                                 
        _blogOwnerEmail = blogConfiguration.getProperty(BLOG_OWNER_EMAIL);
        _blogProperties.put(BLOG_OWNER_EMAIL, _blogOwnerEmail);

        String blogCommentsEnabled = blogConfiguration.getProperty(BLOG_COMMENTS_ENABLED_IP);
        _blogCommentsEnabled = Boolean.valueOf(blogCommentsEnabled);
        _blogProperties.put(BLOG_COMMENTS_ENABLED_IP, _blogCommentsEnabled);

        String blogTrackbacksEnabled = blogConfiguration.getProperty(BLOG_TRACKBACKS_ENABLED_IP);
        _blogTrackbacksEnabled = Boolean.valueOf(blogTrackbacksEnabled);
        _blogProperties.put(BLOG_TRACKBACKS_ENABLED_IP, _blogTrackbacksEnabled);

        String blogPingbacksEnabled = blogConfiguration.getProperty(BLOG_PINGBACKS_ENABLED_IP);
        _blogPingbacksEnabled = Boolean.valueOf(blogPingbacksEnabled);
        _blogProperties.put(BLOG_PINGBACKS_ENABLED_IP, _blogPingbacksEnabled);

        String blogEmailEnabled = blogConfiguration.getProperty(BLOG_EMAIL_ENABLED_IP);
        if ("true".equalsIgnoreCase(blogEmailEnabled)) {
            _blogEmailEnabled = Boolean.valueOf(true);
        } else {
            _blogEmailEnabled = Boolean.valueOf(false);
        }
        _blogProperties.put(BLOG_EMAIL_ENABLED_IP, _blogEmailEnabled);

        String blogFileEncoding = blogConfiguration.getProperty(BLOG_FILE_ENCODING_IP);
        if (BlojsomUtils.checkNullOrBlank(blogFileEncoding)) {
            blogFileEncoding = UTF8;
        }
        _blogFileEncoding = blogFileEncoding;
        _blogProperties.put(BLOG_FILE_ENCODING_IP, blogFileEncoding);

        String blogDefaultFlavor = blogConfiguration.getProperty(BLOG_DEFAULT_FLAVOR_IP);
        if (BlojsomUtils.checkNullOrBlank(blogDefaultFlavor)) {
            blogDefaultFlavor = DEFAULT_FLAVOR_HTML;
        }
        _blogDefaultFlavor = blogDefaultFlavor;
        _blogProperties.put(BLOG_DEFAULT_FLAVOR_IP, _blogDefaultFlavor);

        String linearNavigationEnabled = blogConfiguration.getProperty(LINEAR_NAVIGATION_ENABLED_IP);
        _linearNavigationEnabled = Boolean.valueOf(linearNavigationEnabled);
        _blogProperties.put(LINEAR_NAVIGATION_ENABLED_IP, _linearNavigationEnabled);

        String xmlrpcEnabled = blogConfiguration.getProperty(XMLRPC_ENABLED_IP);
        if (BlojsomUtils.checkNullOrBlank(xmlrpcEnabled)) {
            xmlrpcEnabled = "true";
        }
        _xmlrpcEnabled = Boolean.valueOf(xmlrpcEnabled);
        _blogProperties.put(XMLRPC_ENABLED_IP, _xmlrpcEnabled);

        String blogAdministrationLocale = blogConfiguration.getProperty(BLOG_ADMINISTRATION_LOCALE_IP);
        if (BlojsomUtils.checkNullOrBlank(blogAdministrationLocale)) {
            blogAdministrationLocale = BLOG_LANGUAGE_DEFAULT + "_" + BLOG_COUNTRY_DEFAULT;
        }
        _blogAdministrationLocale = blogAdministrationLocale;
        _blogProperties.put(BLOG_ADMINISTRATION_LOCALE_IP, _blogAdministrationLocale);

        String useEncryptedPasswords = blogConfiguration.getProperty(USE_ENCRYPTED_PASSWORDS);
        _useEncryptedPasswords = Boolean.valueOf(useEncryptedPasswords);
        _blogProperties.put(USE_ENCRYPTED_PASSWORDS, _useEncryptedPasswords);

        String digestAlgorithm = blogConfiguration.getProperty(DIGEST_ALGORITHM);
        if (BlojsomUtils.checkNullOrBlank(digestAlgorithm)) {
            digestAlgorithm = DEFAULT_DIGEST_ALGORITHM;
        }
        try {
            MessageDigest messageDigest = MessageDigest.getInstance(digestAlgorithm);
        } catch (NoSuchAlgorithmException e) {
            digestAlgorithm = DEFAULT_DIGEST_ALGORITHM;
        }
        _digestAlgorithm = digestAlgorithm;
        _blogProperties.put(DIGEST_ALGORITHM, _digestAlgorithm);

		String blogDefaultStyleSheet = blogConfiguration.getProperty(BLOG_DEFAULT_STYLESHEET_IP);
		if (blogDefaultStyleSheet == null) {
			blogDefaultStyleSheet = "";
		}
		_blogDefaultStyleSheet = blogDefaultStyleSheet;

		String blogExistsString = blogConfiguration.getProperty(BLOG_EXISTS);
		if (blogExistsString == null) {
			blogExistsString = "false";
		}
		_blogProperties.put(BLOG_EXISTS, blogExistsString);
		
        _logger.info("blojsom home: " + _blogHome);
    }

    /**
     * Check to see if a username and password is valid for this blog. This method will always return false since it
     * has been deprecated.
     *
     * @param username Username of the user
     * @param password Password for the Username
     * @return This method will always return false since it has been deprecated
     * @deprecated
     * @see org.blojsom.authorization.AuthorizationProvider#authorize(org.blojsom.blog.BlogUser, java.util.Map, java.lang.String, java.lang.String)
     */
    public boolean checkAuthorization(String username, String password) {
        return false;
    }

    /**
     * Return the e-mail address of an authorized user from this blog. If the username is not in this user's
     * authorized list, a value of <code>null</code> is returned.
     *
     * @param username Authorized username
     * @return E-mail address of authorized user or <code>null</code> is username is not available
     * @since blojsom 2.14
     */
    public String getAuthorizedUserEmail(String username) {
        if (_authorization.containsKey(username)) {
            String[] parsedPasswordAndEmail = BlojsomUtils.parseLastComma((String)_authorization.get(username));
            if (parsedPasswordAndEmail.length < 2) {
                return getBlogOwnerEmail();
            } else {
                return parsedPasswordAndEmail[1];
            }
        }

        return null;
    }

    /**
     * Set a new e-mail address for an authorized user for this blog.
     *
     * @param username Username
     * @param email E-mail address
     * @since blojsom 2.14
     */
    public void setAuthorizedUserEmail(String username, String email) {
        if (_authorization.containsKey(username)) {
            String[] parsedPasswordAndEmail = BlojsomUtils.parseLastComma((String)_authorization.get(username));
            StringBuffer updatedPasswordAndEmail = new StringBuffer();
            updatedPasswordAndEmail.append(parsedPasswordAndEmail[0]);
            updatedPasswordAndEmail.append(",");
            updatedPasswordAndEmail.append(email);
            _authorization.put(username, updatedPasswordAndEmail.toString());
            _logger.debug("Set authorized user: " + username + " with e-mail address: " + email);
        }
    }

    /**
     * Set a new password for an authorized user for this blog.
     *
     * @param username Username
     * @param password Password
     * @since blojsom 2.14
     */
    public void setAuthorizedUserPassword(String username, String password) {
        if (_authorization.containsKey(username)) {
            String[] parsedPasswordAndEmail = BlojsomUtils.parseLastComma((String)_authorization.get(username));
            StringBuffer updatedPasswordAndEmail = new StringBuffer();
            updatedPasswordAndEmail.append(password);
            if (parsedPasswordAndEmail.length == 2) {
                updatedPasswordAndEmail.append(",");
                updatedPasswordAndEmail.append(parsedPasswordAndEmail[1]);
            }
            _authorization.put(username, updatedPasswordAndEmail.toString());
        } else {
            _authorization.put(username, password + ",");
        }
    }

    /**
     * Return the directory where blog entries are stored
     * 
     * @return Blog home directory
     */
    public String getBlogHome() {
        return _blogHome;
    }

    /**
     * Return the list of blog file extensions
     * 
     * @return Blog file extensions
     */
    public String[] getBlogFileExtensions() {
        return _blogFileExtensions;
    }

    /**
     * Return the list of blog properties file extensions
     * 
     * @return Blog proprties extensions
     */
    public String[] getBlogPropertiesExtensions() {
        return _blogPropertiesExtensions;
    }

    /**
     * Return the depth to which blog entries will be searched
     * 
     * @return Blog depth
     */
    public int getBlogDepth() {
        return _blogDepth;
    }

    /**
     * Name of the blog
     * 
     * @return Blog name
     */
    public String getBlogName() {
        return _blogName;
    }

    /**
     * Returns the HTML escaped name of the blog
     * 
     * @return Name of the blog that has been escaped
     * @since blojsom 1.9.6
     */
    public String getEscapedBlogName() {
        return BlojsomUtils.escapeString(_blogName);
    }

    /**
     * Description of the blog
     * 
     * @return Blog description
     */
    public String getBlogDescription() {
        return _blogDescription;
    }

    /**
     * Returns the HTML escaped description of the blog
     * 
     * @return Description of the blog that has been escaped
     * @since blojsom 1.9.6
     */
    public String getEscapedBlogDescription() {
        return BlojsomUtils.escapeString(_blogDescription);
    }

    /**
     * URL for the blog
     * 
     * @return Blog URL
     */
    public String getBlogURL() {
        return _blogURL;
    }

    /**
     * Admin URL for the blog
     *
     * @return Blog admin URL
     * @since blosjom 2.21
     */
    public String getBlogAdminURL() {
        return _blogAdminURL;
    }

    /**
     * Base URL for the blog
     * 
     * @return Blog base URL
     */
    public String getBlogBaseURL() {
        return _blogBaseURL;
    }

    /**
     * Language of the blog
     * 
     * @return Blog language
     */
    public String getBlogLanguage() {
        return _blogLanguage;
    }

    /**
     * Country of the blog
     * 
     * @return Country for the blog
     * @since blojsom 1.9.5
     */
    public String getBlogCountry() {
        return _blogCountry;
    }

    /**
     * Return the number of blog entries to retrieve from the individual categories
     * 
     * @return Blog entries to retrieve from the individual categories
     */
    public int getBlogDisplayEntries() {
        return _blogDisplayEntries;
    }

    /**
     * Return the list of categories that should be mapped to the default category '/'
     * 
     * @return List of categories
     */
    public String[] getBlogDefaultCategoryMappings() {
        return _blogDefaultCategoryMappings;
    }

    /**
     * Return the list of categories that should be mapped to the default category '/' as a String
     *
     * @since blojsom 2.12
     * @return List of categories
     */
    public String getBlogDefaultCategoryMappingsAsString() {
        if (_blogDefaultCategoryMappings == null) {
            return "";
        }

        return BlojsomUtils.arrayOfStringsToString(_blogDefaultCategoryMappings);
    }

    /**
     * Set the Username/Password table used for blog authorization.
     * <p/>
     * As of blojsom 2.16, this method can be called more than once.
     * 
     * @param authorization HashMap of Usernames and Passwords
     * @return True is the authorization table was assigned, otherwise false
     */
    public boolean setAuthorization(Map authorization) {
        boolean result = false;
        _authorization = authorization;

        return result;
    }

    /**
     * Returns the authorization map for this blog
     * 
     * @return Map of authorization usernames/passwords

     */
    public Map getAuthorization() {
        return _authorization;
    }


    /**
     * Return the blog owner's e-mail address
     * 
     * @return Blog owner's e-mail
     */
    public String getBlogOwnerEmail() {
        return _blogOwnerEmail;
    }

    /**
     * Return the blog owner's HTML escaped e-mail address
     * 
     * @return Blog owner's e-mail
     */
    public String getEscapedBlogOwnerEmail() {
        return BlojsomUtils.escapeString(_blogOwnerEmail);
    }

    /**
     * Return the blog owner's name
     * 
     * @return Blog owner's name
     */
    public String getBlogOwner() {
        return _blogOwner;
    }

    /**
     * Return the blog owner's HTML escaped name
     * 
     * @return Blog owner's name
     */
    public String getEscapedBlogOwner() {
        return BlojsomUtils.escapeString(_blogOwner);
    }

    /**
     * Returns a read-only view of the properties for this blog
     * 
     * @return Map of blog properties
     */
    public Map getBlogProperties() {
        return Collections.unmodifiableMap(_blogProperties);
    }

    /**
     * Get the directory where blog comments will be written to under the individual blog
     * category directories
     * 
     * @return Blog comments directory
     */
    public String getBlogCommentsDirectory() {
        return _blogCommentsDirectory;
    }

    /**
     * Get the list of directories that should be filtered when looking for categories
     * 
     * @return Blog directory filter list
     */
    public String[] getBlogDirectoryFilter() {
        return _blogDirectoryFilter;
    }

    /**
     * Get the directory where blog trackbacks will be written to under the individual blog
     * category directories
     * 
     * @return Blog trackbacks directory
     */
    public String getBlogTrackbackDirectory() {
        return _blogTrackbackDirectory;
    }

    /**
     * Get the directory where blog pingbacks will be written to under the individual blog
     * category directories
     *
     * @return Blog pingbacks directory
     * @since blojsom 2.23
     */
    public String getBlogPingbacksDirectory() {
        return _blogPingbacksDirectory;
    }

    /**
     * Return whether or not comments are enabled
     * 
     * @return Whether or not comments are enabled
     */
    public Boolean getBlogCommentsEnabled() {
        return _blogCommentsEnabled;
    }

    /**
     * Return whether or not trackbacks are enabled
     * 
     * @return <code>true</code> if trackbacks are enabled, <code>false</code> otherwise
     * @since blojsom 1.9.5
     */
    public Boolean getBlogTrackbacksEnabled() {
        return _blogTrackbacksEnabled;
    }

    /**
     * Return whether or not pingbacks are enabled
     *
     * @return <code>true</code> if pingbacks are enabled, <code>false</code> otherwise
     * @since blojsom 2.23
     */
    public Boolean getBlogPingbacksEnabled() {
        return _blogPingbacksEnabled;
    }

    /**
     * Get whether or not email is enabled
     * 
     * @return Whether or not email is enabled
     */
    public Boolean getBlogEmailEnabled() {
        return _blogEmailEnabled;
    }

    /**
     * Get the file extension for blog entry meta-data
     * 
     * @return Meta-data file extension
     * @since blojsom 1.9
     */
    public String getBlogEntryMetaDataExtension() {
        return _blogEntryMetaDataExtension;
    }

    /**
     * Get the file encoding for blog entries
     * 
     * @return File encoding
     * @since blojsom 1.9
     */
    public String getBlogFileEncoding() {
        return _blogFileEncoding;
    }

    /**
     * Return a named property from the blog properties
     * 
     * @param propertyName Name of the property to retrieve
     * @return Property value as a string or <code>null</code> if the property is not found
     * @since blojsom 1.9
     */
    public String getBlogProperty(String propertyName) {
        if (_blogProperties.containsKey(propertyName)) {
            return _blogProperties.get(propertyName).toString();
        }

        return null;
    }

    /**
     * Get the default flavor for this blog
     * 
     * @return Default blog flavor
     * @since blojsom 2.05
     */
    public String getBlogDefaultFlavor() {
        return _blogDefaultFlavor;
    }

    /**
     * Get the style sheet for this blog
     * 
     * @return Default blog flavor
     * @since blojsom 2.05
     */
    public String getBlogDefaultStyleSheet() {
        return _blogDefaultStyleSheet;
    }

    /**
     * Set the new name for the blog
     * 
     * @param blogName Blog name
     */
    public void setBlogName(String blogName) {
        _blogName = blogName;
        _blogProperties.put(BLOG_NAME_IP, blogName);
    }

    /**
     * Set the new description for the blog
     * 
     * @param blogDescription Blog description
     */
    public void setBlogDescription(String blogDescription) {
        _blogDescription = blogDescription;
        _blogProperties.put(BLOG_DESCRIPTION_IP, blogDescription);
    }

    /**
     * Set the new URL for the blog
     * 
     * @param blogURL Blog URL
     */
    public void setBlogURL(String blogURL) {
        _blogURL = blogURL;
        _blogProperties.put(BLOG_URL_IP, blogURL);
    }

    /**
     * Set the new admin URL for the blog
     *
     * @param blogAdminURL Blog admin URL
     * @since blojsom 2.21
     */
    public void setAdminBlogURL(String blogAdminURL) {
        _blogAdminURL = blogAdminURL;
        _blogProperties.put(BLOG_ADMIN_URL_IP, blogAdminURL);
    }

    /**
     * Set the new base URL for the blog
     * 
     * @param blogBaseURL Blog base URL
     */
    public void setBlogBaseURL(String blogBaseURL) {
        _blogBaseURL = blogBaseURL;
        _blogProperties.put(BLOG_BASE_URL_IP, blogBaseURL);
    }

    /**
     * Set the new 2 letter country code for the blog
     * 
     * @param blogCountry Blog country code
     */
    public void setBlogCountry(String blogCountry) {
        _blogCountry = blogCountry;
        _blogProperties.put(BLOG_COUNTRY_IP, blogCountry);
    }

    /**
     * Set the new 2 letter language code for the blog
     * 
     * @param blogLanguage Blog language code
     */
    public void setBlogLanguage(String blogLanguage) {
        _blogLanguage = blogLanguage;
        _blogProperties.put(BLOG_LANGUAGE_IP, blogLanguage);
    }

    /**
     * Set the depth to which blojsom should look for directories, where -1 indicates infinite depth search
     * 
     * @param blogDepth Blog directory depth
     */
    public void setBlogDepth(int blogDepth) {
        _blogDepth = blogDepth;
        _blogProperties.put(BLOG_DEPTH_IP, new Integer(blogDepth));
    }

    /**
     * Set the number of entries to display at one time, where -1 indicates to display all entries
     * 
     * @param blogDisplayEntries Blog display entries
     */
    public void setBlogDisplayEntries(int blogDisplayEntries) {
        _blogDisplayEntries = blogDisplayEntries;
        _blogProperties.put(BLOG_ENTRIES_DISPLAY_IP, new Integer(blogDisplayEntries));
    }

    /**
     * Set the new default blog category mappings
     * 
     * @param blogDefaultCategoryMappings Blog default category mappings
     */
    public void setBlogDefaultCategoryMappings(String[] blogDefaultCategoryMappings) {
        _blogDefaultCategoryMappings = blogDefaultCategoryMappings;
        _blogProperties.put(BLOG_DEFAULT_CATEGORY_MAPPING_IP, blogDefaultCategoryMappings);
    }

    /**
     * Set the new blog owner name
     * 
     * @param blogOwner Blog owner
     */
    public void setBlogOwner(String blogOwner) {
        _blogOwner = blogOwner;
        _blogProperties.put(BLOG_OWNER, blogOwner);
    }

    /**
     * Set the new blog owner e-mail address
     * 
     * @param blogOwnerEmail Blog owner e-mail
     */
    public void setBlogOwnerEmail(String blogOwnerEmail) {
        _blogOwnerEmail = blogOwnerEmail;
        _blogProperties.put(BLOG_OWNER_EMAIL, blogOwnerEmail);
    }

    /**
     * Set whether blog comments are enabled
     * 
     * @param blogCommentsEnabled <code>true</code> if comments are enabled, <code>false</code> otherwise
     */
    public void setBlogCommentsEnabled(Boolean blogCommentsEnabled) {
        _blogCommentsEnabled = blogCommentsEnabled;
        _blogProperties.put(BLOG_COMMENTS_ENABLED_IP, blogCommentsEnabled);
    }

    /**
     * Set whether emails are sent on blog comments and trackbacks
     * 
     * @param blogEmailEnabled <code>true</code> if email of comments and trackbacks is enabled, <code>false</code> otherwise
     */
    public void setBlogEmailEnabled(Boolean blogEmailEnabled) {
        _blogEmailEnabled = blogEmailEnabled;
        _blogProperties.put(BLOG_EMAIL_ENABLED_IP, blogEmailEnabled);
    }

    /**
     * Set whether blog trackbacks are enabled
     * 
     * @param blogTrackbacksEnabled <code>true</code> if trackbacks are enabled, <code>false</code> otherwise
     */
    public void setBlogTrackbacksEnabled(Boolean blogTrackbacksEnabled) {
        _blogTrackbacksEnabled = blogTrackbacksEnabled;
        _blogProperties.put(BLOG_TRACKBACKS_ENABLED_IP, blogTrackbacksEnabled);
    }

    /**
     * Set whether blog pingbacks are enabled
     *
     * @param blogPingbacksEnabled <code>true</code> if pingbacks are enabled, <code>false</code> otherwise
     * @since blojsom 2.23
     */
    public void setBlogPingbacksEnabled(Boolean blogPingbacksEnabled) {
        _blogPingbacksEnabled = blogPingbacksEnabled;
        _blogProperties.put(BLOG_PINGBACKS_ENABLED_IP, blogPingbacksEnabled);
    }

    /**
     * Set the new blog file encoding
     * 
     * @param blogFileEncoding Blog file encoding
     */
    public void setBlogFileEncoding(String blogFileEncoding) {
        _blogFileEncoding = blogFileEncoding;
        _blogProperties.put(BLOG_FILE_ENCODING_IP, blogFileEncoding);
    }

    /**
     * Set the new blog default category mapping for a particular flavor
     * 
     * @param flavorKey                  Flavor key (must end in blog-default-category-mapping)
     * @param blogDefaultCategoryMapping New blog category mapping
     */
    public void setBlogDefaultCategoryMappingForFlavor(String flavorKey, String blogDefaultCategoryMapping) {
        if (flavorKey.endsWith(BLOG_DEFAULT_CATEGORY_MAPPING_IP)) {
            _blogProperties.put(flavorKey, blogDefaultCategoryMapping);
        }
    }

    /**
     * Set the new set of blog file extensions
     * 
     * @param blogFileExtensions Comma-separated list of blog file extensions
     */
    public void setBlogFileExtensions(String blogFileExtensions) {
        _blogFileExtensions = BlojsomUtils.parseCommaList(blogFileExtensions);
        _blogProperties.put(BLOG_FILE_EXTENSIONS_IP, blogFileExtensions);
    }

    /**
     * Set the new default flavor for this blog
     *
     * @param blogDefaultFlavor New default blog flavor
     * @since blojsom 2.05
     */
    public void setBlogDefaultFlavor(String blogDefaultFlavor) {
        _blogDefaultFlavor = blogDefaultFlavor;
        _blogProperties.put(BLOG_DEFAULT_FLAVOR_IP, _blogDefaultFlavor);
    }

    /**
     * Set a blog property. Properties not allowed to be set are <code>blog-home</code>, <code>blog-comments-directory</code>,
     * and <code>blog-trackbacks-directory</code>.
     *
     * @param key Blog property key
     * @param value Blog property value
     * @since blojsom 2.14
     */
    public void setBlogProperty(String key, String value) {
        if (key != null && value != null) {
            if (!key.equals(BLOG_HOME_IP)  && !key.equals(BLOG_COMMENTS_DIRECTORY_IP)  && !key.equals(BLOG_TRACKBACK_DIRECTORY_IP) && !key.equals(BLOG_PINGBACKS_DIRECTORY_IP)) {
                _blogProperties.put(key, value);
            }
        }
    }

    /**
     * Is linear navigation enabled?
     *
     * @return <code>true</code> if linear navigation is enabled, <code>false</code> otherwise
     * @since blojsom 2.16
     */
    public Boolean getLinearNavigationEnabled() {
        return _linearNavigationEnabled;
    }

    /**
     * Set whether or not linear navigation should be enabled
     *
     * @param linearNavigationEnabled <code>true</code> if linear navigation is enabled, <code>false</code> otherwise
     * @since blojsom 2.16
     */
    public void setLinearNavigationEnabled(Boolean linearNavigationEnabled) {
        _linearNavigationEnabled = linearNavigationEnabled;
        _blogProperties.put(LINEAR_NAVIGATION_ENABLED_IP, _linearNavigationEnabled);
    }

    /**
     * Is XML-RPC enabled for this blog?
     *
     * @return <code>true</code> if XML-RPC is enabled, <code>false</code> otherwise
     * @since blojsom 2.19
     */
    public Boolean getXmlrpcEnabled() {
        return _xmlrpcEnabled;
    }

    /**
     * Set whether or not XML-RPC is enabled
     *
     * @param xmlrpcEnabled <code>true</code> if XML-RPC is enabled, <code>false</code> otherwise
     * @since blojsom 2.19
     */
    public void setXmlrpcEnabled(Boolean xmlrpcEnabled) {
        _xmlrpcEnabled = xmlrpcEnabled;
        _blogProperties.put(XMLRPC_ENABLED_IP, _xmlrpcEnabled);
    }

    /**
     * Retrieve the blog administration locale as a String
     *
     * @return String of blog administration locale
     * @since blojsom 2.21
     */
    public String getBlogAdministrationLocaleAsString() {
        return _blogAdministrationLocale;
    }

    /**
     * Retrieve the blog administration locale as a {@link Locale} object
     *
     * @return {@link Locale} object for blog administration locale
     * @since blojsom 2.21
     */
    public Locale getBlogAdministrationLocale() {
        return BlojsomUtils.getLocaleFromString(_blogAdministrationLocale);
    }

    /**
     * Set the locale used in the administration console
     *
     * @param blogAdministrationLocale Locale string of form <code>language_country_variant</code>
     * @since blojsom 2.21
     */
    public void setBlogAdministrationLocale(String blogAdministrationLocale) {
        _blogAdministrationLocale = blogAdministrationLocale;
        _blogProperties.put(BLOG_ADMINISTRATION_LOCALE_IP, _blogAdministrationLocale);
    }

    /**
     * Retrive a {@link Locale} object from the blog's language and country settings
     *
     * @return {@link Locale} object from the blog's language and country settings
     * @since blojsom 2.21
     */
    public Locale getBlogLocale() {
        return new Locale(_blogLanguage, _blogCountry);
    }

    /**
     * Retrieve whether or not MD5 encrypted passwords are used
     *
     * @return <code>true</code> if encrypted passwords are used, <code>false</code> otherwise
     * @since blojsom 2.24
     */
    public Boolean getUseEncryptedPasswords() {
        return _useEncryptedPasswords;
    }

    /**
     * Set whether or not MD5 encrypted passwords are used
     *
     * @param useEncryptedPasswords <code>true</code> if MD5 passwords are used, <code>false</code> otherwise
     * @since blojsom 2.24
     */
    public void setUseEncryptedPasswords(Boolean useEncryptedPasswords) {
        _useEncryptedPasswords = useEncryptedPasswords;
    }

    /**
     * Set the new admin URL for the blog
     *
     * @param blogAdminURL Blog admin URL
     * @since blojsom 2.24
     */
    public void setBlogAdminURL(String blogAdminURL) {
        _blogAdminURL = blogAdminURL;
        _blogProperties.put(BLOG_ADMIN_URL_IP, blogAdminURL);
    }

    /**
     * Retrieve the in-use password digest algorithm
     *
     * @return Password digest algorithm
     */
    public String getDigestAlgorithm() {
        return _digestAlgorithm;
    }

    /**
     * Set the in-use password digest algorithm
     *
     * @param digestAlgorithm Digest algorithm
     */
    public void setDigestAlgorithm(String digestAlgorithm) {
        if (BlojsomUtils.checkNullOrBlank(digestAlgorithm)) {
            digestAlgorithm = DEFAULT_DIGEST_ALGORITHM;
        }
        try {
            MessageDigest messageDigest = MessageDigest.getInstance(digestAlgorithm);
        } catch (NoSuchAlgorithmException e) {
            digestAlgorithm = DEFAULT_DIGEST_ALGORITHM;
        }

        _digestAlgorithm = digestAlgorithm;
        _blogProperties.put(DIGEST_ALGORITHM, _digestAlgorithm);
    }

    /**
     * Set the new default style sheet for this blog
     *
     * @param blogDefaultStyleSheet New default blog flavor
     * @since blojsom 2.05
     */
    public void setBlogDefaultStyleSheet(String blogDefaultStyleSheet) {
        _blogDefaultStyleSheet = blogDefaultStyleSheet;
        _blogProperties.put(BLOG_DEFAULT_STYLESHEET_IP, _blogDefaultStyleSheet);
    }
}
