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
package org.blojsom.extension.xmlrpc.handlers;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.apache.xmlrpc.XmlRpcException;
import org.blojsom.BlojsomException;
import org.blojsom.blog.BlogCategory;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.Trackback;
import org.blojsom.fetcher.BlojsomFetcher;
import org.blojsom.plugin.trackback.TrackbackPlugin;
import org.blojsom.util.BlojsomMetaDataConstants;
import org.blojsom.util.BlojsomUtils;

import java.io.File;
import java.util.HashMap;
import java.util.Hashtable;
import java.util.Map;
import java.util.Vector;

/**
 * <a href="http://www.movabletype.org/docs/mtmanual_programmatic.html">MovableType API</a> handler
 *
 * @author David Czarnecki
 * @version $Id: MovableTypeAPIHandler.java,v 1.1.2.1 2005/07/21 04:30:23 johnan Exp $
 * @since blojsom 2.20
 */
public class MovableTypeAPIHandler extends AbstractBlojsomAPIHandler {

    protected static final String MEMBER_DATECREATED = "dateCreated";
    protected static final String MEMBER_USERID = "userid";
    protected static final String MEMBER_POSTID = "postid";
    protected static final String MEMBER_TITLE = "title";
    protected static final String MEMBER_CATEGORYID = "categoryId";
    protected static final String MEMBER_CATEGORYNAME = "categoryName";
    protected static final String MEMBER_ISPRIMARY = "isPrimary";
    protected static final String MEMBER_KEY = "key";
    protected static final String MEMBER_LABEL = "label";
    protected static final String MEMBER_PING_TITLE = "pingTitle";
    protected static final String MEMBER_PING_URL = "pingURL";
    protected static final String MEMBER_PING_IP = "pingIP";

    private static final String MOVABLETYPE_API_PERMISSION = "post_via_movabletype_api";

    protected static final String API_PREFIX = "mt";

    protected Log _logger = LogFactory.getLog(MovableTypeAPIHandler.class);

    /**
     * Construct a new <a href="http://www.movabletype.org/docs/mtmanual_programmatic.html">MovableType API</a> handler
     */
    public MovableTypeAPIHandler() {
    }

    /**
     * Attach a blog instance to the API Handler so that it can interact with the blog
     *
     * @param blogUser an instance of BlogUser
     * @throws org.blojsom.BlojsomException If there is an error setting the blog user instance or properties for the handler
     * @see org.blojsom.blog.BlogUser
     */
    public void setBlogUser(BlogUser blogUser) throws BlojsomException {
        _blogUser = blogUser;
        _blog = _blogUser.getBlog();
        _blogEntryExtension = _blog.getBlogProperty(BLOG_XMLRPC_ENTRY_EXTENSION_IP);
        if (BlojsomUtils.checkNullOrBlank(_blogEntryExtension)) {
            _blogEntryExtension = DEFAULT_BLOG_XMLRPC_ENTRY_EXTENSION;
        }
    }

    /**
     * Gets the name of API Handler. Used to bind to XML-RPC
     *
     * @return The API Name (ie: blogger)
     */
    public String getName() {
        return API_PREFIX;
    }

    /**
     * Returns a bandwidth-friendly list of the most recent posts in the system.
     *
     * @param blogID        Blog ID
     * @param username      Username
     * @param password      Password
     * @param numberOfPosts Number of titles to retrieve
     * @return Bandwidth-friendly list of the most recent posts in the system
     * @throws Exception If there is an error retrieving post titles
     */
    public Object getRecentPostTitles(String blogID, String username, String password, int numberOfPosts) throws Exception {
        _logger.debug("getRecentPostTitles() Called ===========[ SUPPORTED ]=====");
        _logger.debug("     BlogId: " + blogID);
        _logger.debug("     UserId: " + username);
        _logger.debug("   Password: *********");
        _logger.debug("   Numposts: " + numberOfPosts);

        Vector recentPosts = new Vector();
        blogID = BlojsomUtils.normalize(blogID);

        try {
            _authorizationProvider.loadAuthenticationCredentials(_blogUser);
            _authorizationProvider.authorize(_blogUser, null, username, password);
            checkXMLRPCPermission(username, MOVABLETYPE_API_PERMISSION);

            // Quick verify that the category is valid
            File blogCategoryFile = new File(_blog.getBlogHome() + BlojsomUtils.removeInitialSlash(blogID));
            if (blogCategoryFile.exists() && blogCategoryFile.isDirectory()) {

                String requestedCategory = BlojsomUtils.removeInitialSlash(blogID);
                BlogCategory blogCategory = _fetcher.newBlogCategory();
                blogCategory.setCategory(blogID);
                blogCategory.setCategoryURL(_blog.getBlogURL() + requestedCategory);

                BlogEntry[] entries;
                Map fetchMap = new HashMap();

                if (BlojsomUtils.checkNullOrBlank(requestedCategory)) {
                    fetchMap.put(BlojsomFetcher.FETCHER_FLAVOR, DEFAULT_FLAVOR_HTML);
                    fetchMap.put(BlojsomFetcher.FETCHER_NUM_POSTS_INTEGER, new Integer(numberOfPosts));
                    entries = _fetcher.fetchEntries(fetchMap, _blogUser);
                } else {
                    fetchMap.put(BlojsomFetcher.FETCHER_CATEGORY, blogCategory);
                    fetchMap.put(BlojsomFetcher.FETCHER_NUM_POSTS_INTEGER, new Integer(numberOfPosts));
                    entries = _fetcher.fetchEntries(fetchMap, _blogUser);
                }

                if (entries != null && entries.length > 0) {
                    for (int x = 0; x < entries.length; x++) {
                        BlogEntry entry = entries[x];
                        Hashtable entrystruct = new Hashtable();
                        entrystruct.put(MEMBER_DATECREATED, entry.getDate());
                        if (BlojsomUtils.checkMapForKey(entry.getMetaData(), BlojsomMetaDataConstants.BLOG_ENTRY_METADATA_AUTHOR)) {
                            entrystruct.put(MEMBER_USERID, (String) entry.getMetaData().get(BlojsomMetaDataConstants.BLOG_ENTRY_METADATA_AUTHOR));
                        } else {
                            entrystruct.put(MEMBER_USERID, _blog.getBlogOwner());
                        }
                        entrystruct.put(MEMBER_POSTID, entry.getId());
                        entrystruct.put(MEMBER_TITLE, entry.getTitle());
                        recentPosts.add(entrystruct);
                    }
                }
            }

            return recentPosts;
        } catch (BlojsomException e) {
            _logger.error("Failed to authenticate user [" + username + "] with password [" + password + "]");
            throw new XmlRpcException(AUTHORIZATION_EXCEPTION, AUTHORIZATION_EXCEPTION_MSG);
        }
    }

    /**
     * Returns a list of all categories defined in the weblog.
     *
     * @param blogID   Blog ID
     * @param username Username
     * @param password Password
     * @return List of all categories defined in the weblog
     * @throws Exception If there is an error getting the category list
     */
    public Object getCategoryList(String blogID, String username, String password) throws Exception {
        _logger.debug("getCategories() Called =====[ SUPPORTED ]=====");
        _logger.debug("     BlogId: " + blogID);
        _logger.debug("     UserId: " + username);
        _logger.debug("   Password: *********");

        try {
            _authorizationProvider.loadAuthenticationCredentials(_blogUser);
            _authorizationProvider.authorize(_blogUser, null, username, password);
            checkXMLRPCPermission(username, MOVABLETYPE_API_PERMISSION);

            Vector result;

            BlogCategory[] categories = _fetcher.fetchCategories(null, _blogUser);

            if (categories != null) {
                result = new Vector(categories.length);

                for (int x = 0; x < categories.length; x++) {
                    Hashtable catlist = new Hashtable(3);
                    BlogCategory category = categories[x];

                    String categoryId = category.getCategory();
                    if (categoryId.length() > 1) {
                        categoryId = BlojsomUtils.removeInitialSlash(categoryId);
                    }

                    String description;
                    Map metadata = category.getMetaData();
                    if (metadata != null && metadata.containsKey(DESCRIPTION_KEY)) {
                        description = (String) metadata.get(DESCRIPTION_KEY);
                    } else {
                        description = category.getEncodedCategory();
                    }

                    catlist.put(MEMBER_CATEGORYID, categoryId);
                    catlist.put(MEMBER_CATEGORYNAME, description);

                    result.add(catlist);
                }
            } else {
                throw new XmlRpcException(NOBLOGS_EXCEPTION, NOBLOGS_EXCEPTION_MSG);
            }

            return result;
        } catch (BlojsomException e) {
            _logger.error("Failed to authenticate user [" + username + "] with password [" + password + "]");
            throw new XmlRpcException(AUTHORIZATION_EXCEPTION, AUTHORIZATION_EXCEPTION_MSG);
        }
    }

    /**
     * Returns a list of all categories to which the post is assigned. Since we only support
     * single categories at the moment, just return a single structure.
     *
     * @param postID   Post ID
     * @param username Username
     * @param password Password
     * @return An array of structs containing String categoryName, String categoryId, and boolean isPrimary
     */
    public Object getPostCategories(String postID, String username, String password) throws Exception {
        _logger.debug("getPost() Called =========[ SUPPORTED ]=====");
        _logger.debug("     PostId: " + postID);
        _logger.debug("     UserId: " + username);
        _logger.debug("   Password: *********");

        Vector result = new Vector();

        try {
            _authorizationProvider.loadAuthenticationCredentials(_blogUser);
            _authorizationProvider.authorize(_blogUser, null, username, password);
            checkXMLRPCPermission(username, MOVABLETYPE_API_PERMISSION);
                   
            String category;
            String permalink;
            String match = "?" + PERMALINK_PARAM + "=";

            int pos = postID.indexOf(match);
            if (pos != -1) {
                category = postID.substring(0, pos);
                category = BlojsomUtils.normalize(category);
                category = BlojsomUtils.urlDecode(category);
                permalink = postID.substring(pos + match.length());

                BlogCategory blogCategory = _fetcher.newBlogCategory();
                blogCategory.setCategory(category);
                blogCategory.setCategoryURL(_blog.getBlogURL() + category);

                Map fetchMap = new HashMap();
                fetchMap.put(BlojsomFetcher.FETCHER_CATEGORY, blogCategory);
                fetchMap.put(BlojsomFetcher.FETCHER_PERMALINK, permalink);
                BlogEntry[] entries = _fetcher.fetchEntries(fetchMap, _blogUser);

                if (entries != null && entries.length > 0) {
                    BlogEntry entry = entries[0];

                    Hashtable categoryContent = new Hashtable(3);

                    String categoryId = entry.getBlogCategory().getCategory();
                    if (categoryId.length() > 1) {
                        categoryId = BlojsomUtils.removeInitialSlash(categoryId);
                    }

                    String description;
                    Map metadata = entry.getBlogCategory().getMetaData();
                    if (metadata != null && metadata.containsKey(DESCRIPTION_KEY)) {
                        description = (String) metadata.get(DESCRIPTION_KEY);
                    } else {
                        description = entry.getBlogCategory().getEncodedCategory();
                    }

                    categoryContent.put(MEMBER_CATEGORYID, categoryId);
                    categoryContent.put(MEMBER_CATEGORYNAME, description);
                    categoryContent.put(MEMBER_ISPRIMARY, Boolean.TRUE);

                    result.add(categoryContent);

                    return result;
                } else {
                    throw new XmlRpcException(INVALID_POSTID, INVALID_POSTID_MSG);
                }
            } else {
                throw new XmlRpcException(INVALID_POSTID, INVALID_POSTID_MSG);
            }
        } catch (BlojsomException e) {
            _logger.error("Failed to authenticate user [" + username + "] with password [" + password + "]");
            throw new XmlRpcException(AUTHORIZATION_EXCEPTION, AUTHORIZATION_EXCEPTION_MSG);
        }
    }

    /**
     * Sets the categories for a post.
     *
     * @param postID     Post ID
     * @param username   Username
     * @param password   Password
     * @param categories Array of structs containing String categoryId and boolean isPrimary
     * @return <code>true</code> if categories set for a post
     * @throws Exception If there is an error setting the categories for a post
     */
    public boolean setPostCategories(String postID, String username, String password, Vector categories) throws Exception {
        throw new XmlRpcException(UNSUPPORTED_EXCEPTION, UNSUPPORTED_EXCEPTION_MSG);
    }

    /**
     * Retrieve information about the XML-RPC methods supported by the server.
     *
     * @return Array of method names supported by the server
     * @throws Exception If there is an error retrieving the list of supported XML-RPC methods.
     */
    public Object supportedMethods() throws Exception {
        Vector result = new Vector();

        result.add("blogger.newPost");
        result.add("blogger.editPost");
        result.add("blogger.getPost");
        result.add("blogger.deletePost");
        result.add("blogger.getRecentPosts");
        result.add("blogger.getUsersBlogs");
        result.add("blogger.getUserInfo");
        result.add("metaWeblog.getUsersBlogs");
        result.add("metaWeblog.getCategories");
        result.add("metaWeblog.newPost");
        result.add("metaWeblog.editPost");
        result.add("metaWeblog.getPost");
        result.add("metaWeblog.deletePost");
        result.add("metaWeblog.getRecentPosts");
        result.add("metaWeblog.newMediaObject");
        result.add("mt.getRecentPostTitles");
        result.add("mt.getCategoryList");
        result.add("mt.getPostCategories");
        result.add("mt.supportedMethods");
        result.add("mt.supportedTextFilters");
        result.add("mt.getTrackbackPings");

        return result;
    }

    /**
     * Retrieve information about the text formatting plugins supported by the server.
     *
     * @return An array of structs containing String key and String label. key is the
     *         unique string identifying a text formatting plugin, and label is the readable
     *         description to be displayed to a user
     * @throws Exception If there is an error retrieving the list of plugins
     */
    public Object supportedTextFilters() throws Exception {
        Vector result = new Vector();

        // Return an empty list as we need to figure out a way to determine supported formatting plugins
        return result;
    }

    /**
     * Retrieve the list of TrackBack pings posted to a particular entry
     *
     * @param postID Post ID
     * @return An array of structs containing String pingTitle (the title of the entry sent
     *         in the ping), String pingURL (the URL of the entry), and String pingIP (the IP address
     *         of the host that sent the ping)
     * @throws Exception If there is an error retrieving trackbacks for an entry
     */
    public Object getTrackbackPings(String postID) throws Exception {
        _logger.debug("getTrackbackPings() Called =========[ SUPPORTED ]=====");
        _logger.debug("     PostId: " + postID);

        Vector trackbackPings = new Vector();

        try {
            String category;
            String permalink;
            String match = "?" + PERMALINK_PARAM + "=";

            int pos = postID.indexOf(match);
            if (pos != -1) {
                category = postID.substring(0, pos);
                category = BlojsomUtils.normalize(category);
                category = BlojsomUtils.urlDecode(category);
                permalink = postID.substring(pos + match.length());

                BlogCategory blogCategory = _fetcher.newBlogCategory();
                blogCategory.setCategory(category);
                blogCategory.setCategoryURL(_blog.getBlogURL() + category);

                Map fetchMap = new HashMap();
                fetchMap.put(BlojsomFetcher.FETCHER_CATEGORY, blogCategory);
                fetchMap.put(BlojsomFetcher.FETCHER_PERMALINK, permalink);
                BlogEntry[] entries = _fetcher.fetchEntries(fetchMap, _blogUser);

                if (entries != null && entries.length > 0) {
                    BlogEntry entry = entries[0];

                    Trackback[] trackbacks = entry.getTrackbacksAsArray();
                    for (int i = 0; i < trackbacks.length; i++) {
                        Hashtable trackbackInformation = new Hashtable(3);

                        trackbackInformation.put(MEMBER_PING_TITLE, trackbacks[i].getTitle());
                        trackbackInformation.put(MEMBER_PING_URL, trackbacks[i].getUrl());
                        if (BlojsomUtils.checkMapForKey(trackbacks[i].getMetaData(), TrackbackPlugin.BLOJSOM_TRACKBACK_PLUGIN_METADATA_IP)) {
                            trackbackInformation.put(MEMBER_PING_IP, trackbacks[i].getMetaData().get(TrackbackPlugin.BLOJSOM_TRACKBACK_PLUGIN_METADATA_IP));
                        } else {
                            trackbackInformation.put(MEMBER_PING_IP, "");
                        }

                        trackbackPings.add(trackbackInformation);
                    }

                    return trackbackPings;
                } else {
                    throw new XmlRpcException(INVALID_POSTID, INVALID_POSTID_MSG);
                }
            } else {
                throw new XmlRpcException(INVALID_POSTID, INVALID_POSTID_MSG);
            }
        } catch (BlojsomException e) {
            _logger.error(UNKNOWN_EXCEPTION_MSG, e);
            throw new XmlRpcException(UNKNOWN_EXCEPTION, UNKNOWN_EXCEPTION_MSG);
        }
    }

    /**
     * Publish (rebuild) all of the static files related to an entry from your weblog. Equivalent to saving an entry in the system (but without the ping)
     *
     * @param postID Post ID
     * @param username Username
     * @param password Password
     * @return <code>true</code> if post published
     * @throws Exception If there is an error publishing the post
     */
    public boolean publishPost(String postID, String username, String password) throws Exception {
        throw new XmlRpcException(UNSUPPORTED_EXCEPTION, UNSUPPORTED_EXCEPTION_MSG);
    }
}