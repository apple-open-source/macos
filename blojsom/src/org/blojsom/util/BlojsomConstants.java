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
package org.blojsom.util;

/**
 * BlojsomConstants
 *
 * @author David Czarnecki
 * @author Mark Lussier
 * @author Dan Morrill
 * @version $Id: BlojsomConstants.java,v 1.6.2.1 2005/07/21 14:11:04 johnan Exp $
 */
public interface BlojsomConstants {

    /**
     * blojsom version
     */
    public static final String BLOJSOM_VERSION_NUMBER = "blojsom v2.25";

    /**
     * Key under which blog information will be placed
     * (example: on the request for the JSPDispatcher)
     */
    public static final String BLOJSOM_BLOG = "BLOJSOM_BLOG";

    /**
     * Key under which the blog entries will be placed
     * (example: on the request for the JSPDispatcher)
     */
    public static final String BLOJSOM_ENTRIES = "BLOJSOM_ENTRIES";

    /**
     * Key under which the blog categories will be placed
     * (example: on the request for the JSPDispatcher)
     */
    public static final String BLOJSOM_CATEGORIES = "BLOJSOM_CATEGORIES";

    /**
     * Key under which all the blog categories will be placed
     * (example: on the request for the JSPDispatcher)
     */
    public static final String BLOJSOM_ALL_CATEGORIES = "BLOJSOM_ALL_CATEGORIES";

    /**
     * Key under which the date (RFC 822 format) of the blog will be placed
     * (example: on the request for the JSPDispatcher)
     */
    public static final String BLOJSOM_DATE = "BLOJSOM_DATE";

    /**
     * Key under which the date (ISO 8601 format) of the blog will be placed
     * (example: on the request for the JSPDispatcher)
     */
    public static final String BLOJSOM_DATE_ISO8601 = "BLOJSOM_DATE_ISO8601";

    /**
     * Key under which the date object of the blog will be placed
     * (example: on the request for the JSPDispatcher)
     */
    public static final String BLOJSOM_DATE_OBJECT = "BLOJSOM_DATE_OBJECT";

    /**
     * Key under which the date (UTC format) of the blog will be placed
     * (example: on the request for the JSPDispatcher)
     */
    public static final String BLOJSOM_DATE_UTC = "BLOJSOM_DATE_UTC";

    /**
     * Key under which the blog site will be placed
     * (example: on the request for the JSPDispatcher)
     */
    public static final String BLOJSOM_SITE_URL = "BLOJSOM_SITE_URL";

    /**
     * Key under which the permalink value will be placed. This is used to allow templates
     * to generate trackback auto-discovery fragments.
     * (example: on the request for the JSPDispatcher)
     */
    public static final String BLOJSOM_PERMALINK = "BLOJSOM_PERMALINK";

    /**
     * Key under which the next entry after the permalink value will be placed. This is used to allow templates
     * to generate linear post navigation links.
     * (example: on the request for the JSPDispatcher)
     */
    public static final String BLOJSOM_PERMALINK_NEXT_ENTRY = "BLOJSOM_PERMALINK_NEXT_ENTRY";

    /**
     * Key under which the previous entry after the permalink value will be placed. This is used to allow templates
     * to generate linear post navigation links.
     * (example: on the request for the JSPDispatcher)
     */
    public static final String BLOJSOM_PERMALINK_PREVIOUS_ENTRY = "BLOJSOM_PERMALINK_PREVIOUS_ENTRY";

    /**
     * Key under which the requested category will be placed
     * (example: on the request for the JSPDispatcher)
     */
    public static final String BLOJSOM_REQUESTED_CATEGORY = "BLOJSOM_REQUESTED_CATEGORY";

    /**
     * Key under which whether or not comments are enabled will be placed
     * (example: on the request for the JSP dispatcher)
     */
    public static final String BLOJSOM_COMMENTS_ENABLED = "BLOJSOM_COMMENTS_ENABLED";

    /**
     * Key under which whether or not email messages will be sent to the blog author
     */
    public static final String BLOJSOM_EMAIL_ENABLED = "BLOJSOM_EMAIL_ENABLED";

    /**
     * Key under which the lastmodified date of the blog will be placed
     * (example: on the request for the JSPDispatcher)
     */
    public static final String BLOJSOM_LAST_MODIFIED = "BLOJSOM_LAST_MODIFIED";

    /**
     * Key under which the blog user id will be placed
     * (example: on the request for the JSPDispatcher)
     */
    public static final String BLOJSOM_USER = "BLOJSOM_USER";

    /**
     * Key under which the blojsom version string will be placed
     * (example: on the request for the JSP dispatcher)
     */
    public static final String BLOJSOM_VERSION = "BLOJSOM_VERSION";

    /**
     * Key under which the blojsom requested flavor string will be placed
     * (example: in the context for the Velocity dispatcher)
     */
    public static final String BLOJSOM_REQUESTED_FLAVOR = "BLOJSOM_REQUESTED_FLAVOR";

    /**
     * Key under which the plugins will be places
     * (example: in the context for the Velocity dispatcher)
     */
    public static final String BLOJSOM_PLUGINS = "BLOJSOM_PLUGINS";

    /**
     * Default class for fetching blog entries
     */
    public static final String BLOG_DEFAULT_FETCHER = "org.blojsom.fetcher.StandardFetcher";

    /**
     * UTF-8 encoding
     */
    public static final String UTF8 = "UTF-8";

    /**
     * Default flavor for blojsom if none is requested or the flavor requested is invalid
     */
    public static final String DEFAULT_FLAVOR_HTML = "html";

    /**
     * Default directory for adding comments
     */
    public static final String DEFAULT_COMMENTS_DIRECTORY = ".comments";

    /**
     * Reserved file extension for blojsom comments
     */
    public static final String COMMENT_EXTENSION = ".cmt";

    /**
     * Request parameter for the requested "flavor"
     */
    public static final String FLAVOR_PARAM = "flavor";

    /**
     * Request parameter for a "permalink"
     */
    public static final String PERMALINK_PARAM = "permalink";

    /**
     * Request parameter for the "plugins"
     */
    public static final String PLUGINS_PARAM = "plugins";

    /**
     * Request parameter for the "page"
     */
    public static final String PAGE_PARAM = "page";

    /**
     * Request parameter value for the archive page
     */
    public static final String PAGE_PARAM_ARCHIVE = "archive";

    /**
     * Request parameter value for not toggling LastModfied and ETag fromgetting generated
     */
    public static final String OVERRIDE_LASTMODIFIED_PARAM = "lastmodified";

    /**
     * Request parameter for the "category"
     */
    public static final String CATEGORY_PARAM = "category";

    /**
     * Default directory for adding trackbacks
     */
    public static final String DEFAULT_TRACKBACK_DIRECTORY = ".trackbacks";

    /**
     * Reserved file extension for blojsom trackbacks
     */
    public static final String TRACKBACK_EXTENSION = ".tb";

    /**
     * Default directory for adding pingbacks
     */
    public static final String DEFAULT_PINGBACKS_DIRECTORY = ".pingbacks";

    /**
     * Reserved file extension for blojsom pingbacks
     */
    public static final String PINGBACK_EXTENSION = ".pb";

    /**
     * Value indicating all subdirectories under the blog home should be searched
     */
    public static final int INFINITE_BLOG_DEPTH = -1;

    /**
     * The properties file key that denotes a blog category description
     */
    public static final String DESCRIPTION_KEY = "blojsom.description";

    /**
     * The properties file key that denotes a blog category name (different from the directory name)
     */
    public static final String NAME_KEY = "blojsom.name";

    /**
     * Default language for blog if none supplied (en)
     */
    public static final String BLOG_LANGUAGE_DEFAULT = "en";

    /**
     * Default country for blog if none supplied (US)
     */
    public static final String BLOG_COUNTRY_DEFAULT = "US";

    /**
     * Default number of blog entries to display (-1 indicates all entries will be displayed)
     */
    public static final int BLOG_ENTRIES_DISPLAY_DEFAULT = -1;

    /**
     * HTTP Header Name representing the Last Modified Timstamp of the blog (GMT Based)
     */
    public static final String HTTP_LASTMODIFIED = "Last-Modified";

    /**
     * HTTP Header Name representing the ETag of the blog
     */
    public static final String HTTP_ETAG = "ETag";

    /**
     * RFC 822 style date format
     */
    public static final String RFC_822_DATE_FORMAT = "EEE, d MMM yyyy HH:mm:ss Z";

    /**
     * ISO 8601 style date format
     * ISO 8601 [W3CDTF] date format (used in rdf flavor)
     */
    public static final String ISO_8601_DATE_FORMAT = "yyyy-MM-dd'T'HH:mm:ssz";

    /**
     * UTC style date format
     * @since blojsom 1.9.4
     */
    public static final String UTC_DATE_FORMAT = "yyyy-MM-dd'T'kk:mm:ss'Z'";

    /**
     * If a entry is longer that this length, then when any content hashing is performed, it is
     * truncated to this size. NOTE: This only truncates for hash.
     */
    public static final int MAX_HASHABLE_LENGTH = 300;

    /**
     * Default extension for metadata
     */
    public static final String DEFAULT_METADATA_EXTENSION = ".meta";

    /**
     * Default extension for entries
     */
    public static final String DEFAULT_ENTRY_EXTENSION = ".html"; 

    /**
     * Line separator for the system
     */
    public static final String LINE_SEPARATOR = System.getProperty("line.separator");

    public static final String DEFAULT_DIGEST_ALGORITHM = "MD5";

    public static final String WHITESPACE = " \t\n\f\r";

    /**
     * Default authorization provider
     */
    public static final String DEFAULT_AUTHORIZATION_PROVIDER = "org.blojsom.authorization.PropertiesAuthorizationProvider";

    /**
     * Various HTTP caching headers
     */
    public static final String PRAGMA_HTTP_HEADER = "Pragma";
    public static final String CACHE_CONTROL_HTTP_HEADER = "Cache-Control";
    public static final String NO_CACHE_HTTP_HEADER_VALUE = "no-cache";

    public static final String BLOJSOM_CONFIGURATION_IP = "blojsom-configuration";

    // Initialization parameters from web.xml
    public final static String BLOJSOM_FLAVOR_CONFIGURATION_IP = "flavor-configuration";
    public final static String BLOJSOM_PLUGIN_CONFIGURATION_IP = "plugin-configuration";

    // blojsom initialization parameters from blojsom.properties
    public static final String BLOJSOM_USERS_IP = "blojsom-users";
    public static final String BLOJSOM_DEFAULT_USER_IP = "blojsom-default-user";
    public static final String BLOJSOM_FETCHER_IP = "blojsom-fetcher";
    public static final String BLOJSOM_CONFIGURATION_BASE_DIRECTORY_IP = "blojsom-configuration-base-directory";
    public static final String BLOJSOM_INSTALLATION_DIRECTORY_IP = "blojsom-installation-directory";
    public static final String BLOJSOM_TEMPLATES_DIRECTORY_IP = "blojsom-templates-directory";
    public static final String BLOJSOM_RESOURCE_DIRECTORY_IP = "blojsom-resource-directory";
    public static final String BLOJSOM_DEFAULT_CONFIGURATION_BASE_DIRECTORY = "/WEB-INF/";
    public static final String BLOJSOM_DEFAULT_TEMPLATES_DIRECTORY = "/templates";
    public static final String BLOJSOM_DEFAULT_RESOURCE_DIRECTORY = "/blojsom_resources/meta/";
    public static final String BLOJSOM_RESOURCE_MANAGER_IP = "blojsom-resource-manager";
    public static final String BLOJSOM_DEFAULT_RESOURCE_MANAGER = "org.blojsom.util.resources.ResourceBundleResourceManager";
    public static final String BLOJSOM_RESOURCE_MANAGER_BUNDLES_IP = "blojsom-resource-manager-bundles";
    public static final String BLOJSOM_RESOURCE_MANAGER_CONTEXT_KEY = "BLOJSOM_RESOURCE_MANAGER";
    public static final String BLOG_DEFAULT_PROPERTIES = "blog.properties";
    public static final String BLOJSOM_AUTHORIZATION_PROVIDER_IP = "blojsom-authorization-provider";
    public static final String BLOJSOM_BLOG_HOME_IP = "blojsom-blog-home";
    public static final String BLOJSOM_INSTALLED_LOCALES_IP = "blojsom-installed-locales";

    // blojsom listener and event configuration parameters
    public static final String BLOJSOM_LISTENER_CONFIGURATION_IP = "listener-configuration";
    public static final String BLOJSOM_BROADCASTER_IP = "blojsom-broadcaster";
    public static final String BLOJSOM_DEFAULT_BROADCASTER = "org.blojsom.event.SimpleBlojsomEventBroadcaster";

    // Blog initialization parameters from blog.properties
    public static final String BLOG_HOME_IP = "blog-home";
    public static final String BLOG_NAME_IP = "blog-name";
    public static final String BLOG_DEPTH_IP = "blog-directory-depth";
    public static final String BLOG_LANGUAGE_IP = "blog-language";
    public static final String BLOG_COUNTRY_IP = "blog-country";
    public static final String BLOG_DESCRIPTION_IP = "blog-description";
    public static final String BLOG_URL_IP = "blog-url";
    public static final String BLOG_ADMIN_URL_IP = "blog-admin-url"; 
    public static final String BLOG_BASE_URL_IP = "blog-base-url";
    public static final String BLOG_FILE_EXTENSIONS_IP = "blog-file-extensions";
    public static final String BLOG_PROPERTIES_EXTENSIONS_IP = "blog-properties-extensions";
    public static final String[] DEFAULT_PROPERTIES_EXTENSIONS = {".properties"};
    public static final String BLOG_ENTRIES_DISPLAY_IP = "blog-entries-display";
    public static final String BLOG_DEFAULT_CATEGORY_MAPPING_IP = "blog-default-category-mapping";
    public static final String BLOG_DIRECTORY_FILTER_IP = "blog-directory-filter";
    public static final String BLOG_AUTHORIZATION_IP = "blog-authorization";
    public static final String BLOG_OWNER = "blog-owner";
    public static final String BLOG_OWNER_EMAIL = "blog-owner-email";
    public static final String BLOG_COMMENTS_DIRECTORY_IP = "blog-comments-directory";
    public static final String BLOG_TRACKBACK_DIRECTORY_IP = "blog-trackbacks-directory";
    public static final String BLOG_PINGBACKS_DIRECTORY_IP = "blog-pingbacks-directory";
    public static final String BLOG_COMMENTS_ENABLED_IP = "blog-comments-enabled";
    public static final String BLOG_TRACKBACKS_ENABLED_IP = "blog-trackbacks-enabled";
    public static final String BLOG_PINGBACKS_ENABLED_IP = "blog-pingbacks-enabled"; 
    public static final String BLOG_EMAIL_ENABLED_IP = "blog-email-enabled";
    public static final String BLOJSOM_PLUGIN_CHAIN = "blojsom-plugin-chain";
    public static final String BLOG_ENTRY_META_DATA_EXTENSION_IP = "blog-entry-meta-data-extension";
    public static final String BLOG_FILE_ENCODING_IP = "blog-file-encoding";
    public static final String BLOG_BLACKLIST_FILE_IP = "blog-blacklist-file";
    public static final String BLOG_DEFAULT_FLAVOR_IP = "blog-default-flavor";
    public static final String LINEAR_NAVIGATION_ENABLED_IP = "linear-navigation-enabled";
    public static final String XMLRPC_ENABLED_IP = "xmlrpc-enabled";
    public static final String BLOG_ADMINISTRATION_LOCALE_IP = "blog-administration-locale";
    public static final String USE_ENCRYPTED_PASSWORDS = "use-encrypted-passwords";
    public static final String DIGEST_ALGORITHM = "digest-algorithm";
    public static final String RECURSIVE_CATEGORIES = "recursive-categories";
    public static final String PREFERRED_SYNDICATION_FLAVOR = "preferred-syndication-flavor";
	public static final String BLOG_DEFAULT_STYLESHEET_IP = "blog-stylesheet";
	public static final String BLOG_EXISTS = "blog-exists";

    // Other generic properties
    public static final String ADMINISTRATORS_IP = "administrators";

    // Configuration filename defaults
    public static final String DEFAULT_FLAVOR_CONFIGURATION_FILE = "flavor.properties";
    public static final String DEFAULT_PLUGIN_CONFIGURATION_FILE = "plugin.properties";
    public static final String DEFAULT_DISPATCHER_CONFIGURATION_FILE = "dispatcher.properties";

    public static final String BLOG_PERMISSIONS_IP = "blog-permissions";
    public static final String DEFAULT_PERMISSIONS_CONFIGURATION_FILE = "permissions.properties";

	// Directory Service authentication
	public static final String USE_DIRECTORY_SERVICES = "_USE_DIRECTORY_SERVICES_";
	public static final String DIRECTORY_SERVICES_GROUP = "_DIRECTORY_SERVICES_GROUP_";
	public static final String COMPUTER_NAME = "_COMPUTER_NAME_";
}
