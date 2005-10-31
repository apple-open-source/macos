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
import org.blojsom.plugin.admin.event.AddBlogEntryEvent;
import org.blojsom.plugin.admin.event.UpdatedBlogEntryEvent;
import org.blojsom.plugin.admin.event.DeletedBlogEntryEvent;
import org.blojsom.blog.BlogCategory;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.Blog;
import org.blojsom.fetcher.BlojsomFetcher;
import org.blojsom.util.BlojsomUtils;

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.*;
import java.text.SimpleDateFormat;

/**
 * Blojsom XML-RPC Handler for the MetaWeblog API
 * <p/>
 * MetaWeblog API pec can be found at http://www.xmlrpc.com/metaWeblogApi
 *
 * @author Mark Lussier
 * @version $Id: MetaWeblogAPIHandler.java,v 1.3.2.1 2005/07/21 04:30:23 johnan Exp $
 */
public class MetaWeblogAPIHandler extends AbstractBlojsomAPIHandler {

    private static final String METAWEBLOG_ACCEPTED_TYPES_IP = "blojsom-extension-metaweblog-accepted-types";

    /**
     * Blogger API "blogid" key
     */
    private static final String MEMBER_BLOGID = "blogid";

    /**
     * Blogger API "blogName" key
     */
    private static final String MEMBER_BLOGNAME = "blogName";

    /**
     * MetaWeblog API "description" key
     */
    private static final String MEMBER_DESCRIPTION = "description";

    /**
     * MetaWeblog API "htmlUrl" key
     */
    private static final String MEMBER_HTML_URL = "htmlUrl";

    /**
     * MetaWeblog API "rssUrl" key
     */
    private static final String MEMBER_RSS_URL = "rssUrl";

    /**
     * MetaWeblog API "title" key
     */
    private static final String MEMBER_TITLE = "title";

    /**
     * MetaWeblog API "link" key
     */
    private static final String MEMBER_LINK = "link";

    /**
     * MetaWeblog API "name" key
     */
    private static final String MEMBER_NAME = "name";

    /**
     * MetaWeblog API "type" key
     */
    private static final String MEMBER_TYPE = "type";

    /**
     * MetaWeblog API "bits" key
     */
    private static final String MEMBER_BITS = "bits";

    /**
     * MetaWeblog API "permaLink" key
     */
    private static final String MEMBER_PERMALINK = "permaLink";

    /**
     * MetaWeblog API "dateCreated" key
     */
    private static final String MEMBER_DATE_CREATED = "dateCreated";

    /**
     * MetaWeblog API "categories" key
     */
    private static final String MEMBER_CATEGORIES = "categories";

    /**
     * MetaWeblog API "postid" key
     */
    private static final String MEMBER_POSTID = "postid";

    /**
     * MetaWeblog API "url" key
     */
    private static final String MEMBER_URL = "url";

    private static final String METAWEBLOG_API_PERMISSION = "post_via_metaweblog_api";

    public static final String API_PREFIX = "metaWeblog";

    private String _uploadDirectory;
    private HashMap _acceptedMimeTypes;
    private String _staticURLPrefix;

    private Log _logger = LogFactory.getLog(MetaWeblogAPIHandler.class);

    /**
     * Default constructor
     */
    public MetaWeblogAPIHandler() {
    }

    /**
     * Gets the name of API Handler. Used to bind to XML-RPC
     *
     * @return The API Name (ie: metaWeblog)
     */
    public String getName() {
        return API_PREFIX;
    }

    /**
     * Attach a Blog instance to the API Handler so that it can interact with the blog
     *
     * @param blogUser an instance of BlogUser
     * @throws BlojsomException If there is an error setting the blog instance or properties for the handler
     * @see org.blojsom.blog.BlogUser
     */
    public void setBlogUser(BlogUser blogUser) throws BlojsomException {
        _blogUser = blogUser;
        _blog = _blogUser.getBlog();
        _blogEntryExtension = _blog.getBlogProperty(BLOG_XMLRPC_ENTRY_EXTENSION_IP);
        if (BlojsomUtils.checkNullOrBlank(_blogEntryExtension)) {
            _blogEntryExtension = DEFAULT_BLOG_XMLRPC_ENTRY_EXTENSION;
        }

        _uploadDirectory = _configuration.getQualifiedResourceDirectory();
        if (BlojsomUtils.checkNullOrBlank(_uploadDirectory)) {
            throw new BlojsomException("No upload directory specified in blog configuration");
        }

        if (!_uploadDirectory.endsWith("/")) {
            _uploadDirectory += "/";
        }

        _logger.debug("Upload directory for user [" + _blogUser.getId() + "] is " + _uploadDirectory);

        _acceptedMimeTypes = new HashMap(3);
        String acceptedMimeTypes = _blog.getBlogProperty(METAWEBLOG_ACCEPTED_TYPES_IP);
        if (acceptedMimeTypes != null && !"".equals(acceptedMimeTypes)) {
            String[] types = BlojsomUtils.parseCommaList(acceptedMimeTypes);
            for (int i = 0; i < types.length; i++) {
                String type = types[i];
                type = type.toLowerCase();
                _acceptedMimeTypes.put(type, type);
            }
        }

        _staticURLPrefix = _configuration.getResourceDirectory();
        if (!_staticURLPrefix.endsWith("/")) {
            _staticURLPrefix += "/";
        }
    }

    /**
     * Returns information on all the blogs a given user is a member of
     *
     * @param appkey Unique identifier/passcode of the application sending the post
     * @param userid Login for a Blogger user who has permission to post to the blog
     * @param password Password for said username
     * @throws XmlRpcException If there are no categories or the user was not authenticated correctly
     * @return Blog category list
     */
    public Object getUsersBlogs(String appkey, String userid, String password) throws Exception {
        _logger.debug("getUsersBlogs() Called ===[ SUPPORTED ]=======");
        _logger.debug("     Appkey: " + appkey);
        _logger.debug("     UserId: " + userid);
        _logger.debug("   Password: *********");

        try {
            _authorizationProvider.loadAuthenticationCredentials(_blogUser);
            _authorizationProvider.authorize(_blogUser, null, userid, password);
            checkXMLRPCPermission(userid, METAWEBLOG_API_PERMISSION);

            Vector result = new Vector();
            BlogCategory[] _categories = _fetcher.fetchCategories(null, _blogUser);

            if (_categories != null) {
                for (int x = 0; x < _categories.length; x++) {
                    Hashtable _bloglist = new Hashtable(3);
                    BlogCategory _category = _categories[x];

                    String _blogid = _category.getCategory();
                    if (_blogid.length() > 1) {
                        _blogid = BlojsomUtils.removeInitialSlash(_blogid);
                    }

                    String _description = "";
                    Map _metadata = _category.getMetaData();
                    if (_metadata != null && _metadata.containsKey(NAME_KEY)) {
                        _description = (String) _metadata.get(NAME_KEY);
                    } else {
                        _description = _category.getEncodedCategory();
                    }

                    _bloglist.put(MEMBER_URL, _category.getCategoryURL());
                    _bloglist.put(MEMBER_BLOGID, _blogid);
                    _bloglist.put(MEMBER_BLOGNAME, _description);

                    result.add(_bloglist);
                }
            }

            return result;
        } catch (BlojsomException e) {
            _logger.error("Failed to authenticate user [" + userid + "] with password [" + password + "]");
            throw new XmlRpcException(AUTHORIZATION_EXCEPTION, AUTHORIZATION_EXCEPTION_MSG);
        }
    }

    /**
     * Authenticates a user and returns the categories available in the blojsom
     *
     * @param blogid   Dummy Value for Blojsom
     * @param userid   Login for a MetaWeblog user who has permission to post to the blog
     * @param password Password for said username
     * @return Blog category list
     * @throws XmlRpcException If there are no categories or the user was not authenticated correctly
     */
    public Object getCategories(String blogid, String userid, String password) throws Exception {
        _logger.debug("getCategories() Called =====[ SUPPORTED ]=====");
        _logger.debug("     BlogId: " + blogid);
        _logger.debug("     UserId: " + userid);
        _logger.debug("   Password: *********");

        try {
            _authorizationProvider.loadAuthenticationCredentials(_blogUser);
            _authorizationProvider.authorize(_blogUser, null, userid, password);
            checkXMLRPCPermission(userid, METAWEBLOG_API_PERMISSION);

            Hashtable result;

            BlogCategory[] categories = _fetcher.fetchCategories(null, _blogUser);

            if (categories != null) {
                result = new Hashtable(categories.length);

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

                    catlist.put(MEMBER_DESCRIPTION, description);
                    catlist.put(MEMBER_HTML_URL, category.getCategoryURL());
                    catlist.put(MEMBER_RSS_URL, category.getCategoryURL() + "?flavor=rss2");

                    result.put(categoryId, catlist);
                }
            } else {
                throw new XmlRpcException(NOBLOGS_EXCEPTION, NOBLOGS_EXCEPTION_MSG);
            }

            return result;
        } catch (BlojsomException e) {
            _logger.error("Failed to authenticate user [" + userid + "] with password [" + password + "]");
            throw new XmlRpcException(AUTHORIZATION_EXCEPTION, AUTHORIZATION_EXCEPTION_MSG);
        }
    }

    /**
     * Makes a new post to a designated blog. Optionally, will publish the blog after making the post
     *
     * @param blogid   Unique identifier of the blog the post will be added to
     * @param userid   Login for a MetaWeblog user who has permission to post to the blog
     * @param password Password for said username
     * @param struct   Contents of the post
     * @param publish  If true, the blog will be published immediately after the post is made
     * @return Post ID of the added entry
     * @throws XmlRpcException If the user was not authenticated correctly or if there was an I/O exception
     */
    public String newPost(String blogid, String userid, String password, Hashtable struct, boolean publish) throws Exception {
        _logger.debug("newPost() Called ===========[ SUPPORTED ]=====");
        _logger.debug("     BlogId: " + blogid);
        _logger.debug("     UserId: " + userid);
        _logger.debug("   Password: *********");
        _logger.debug("    Publish: " + publish);

        blogid = BlojsomUtils.normalize(blogid);
        if (struct.containsKey(MEMBER_CATEGORIES)) {
            Vector categories = (Vector) struct.get(MEMBER_CATEGORIES);
            if (categories.size() > 0) {
                String categoryForPost = (String) categories.get(0);
                blogid = BlojsomUtils.normalize(categoryForPost);
            }
        }

        try {
            _authorizationProvider.loadAuthenticationCredentials(_blogUser);
            _authorizationProvider.authorize(_blogUser, null, userid, password);
            checkXMLRPCPermission(userid, METAWEBLOG_API_PERMISSION);

            String result = null;

            //Quick verify that the categories are valid
            File blogCategory = getBlogCategoryDirectory(blogid);
            if (blogCategory.exists() && blogCategory.isDirectory()) {
                Hashtable postcontent = struct;

                String title = (String) postcontent.get(MEMBER_TITLE);
                String description = (String) postcontent.get(MEMBER_DESCRIPTION);
                String filename = BlojsomUtils.getBlogEntryFilename(title, description);
                String outputfile = blogCategory.getAbsolutePath() + File.separator + filename;
                Date dateCreated = (Date) postcontent.get(MEMBER_DATE_CREATED);

                try {
                    File sourceFile = new File(outputfile + _blogEntryExtension);
                    int fileTag = 1;
                    while (sourceFile.exists()) {
                        sourceFile = new File(outputfile + "-" + fileTag + _blogEntryExtension);
                        fileTag++;
                    }
                    String postid = blogid + "?" + PERMALINK_PARAM + "=" + BlojsomUtils.urlEncode(sourceFile.getName());

                    BlogEntry entry = _fetcher.newBlogEntry();
                    HashMap attributeMap = new HashMap();
                    HashMap blogEntryMetaData = new HashMap();

                    attributeMap.put(SOURCE_ATTRIBUTE, sourceFile);
                    entry.setAttributes(attributeMap);
                    entry.setCategory(blogid);
                    if (BlojsomUtils.checkNullOrBlank(title)) {
                        title = null;
                    }
                    entry.setTitle(title);
                    entry.setDescription(description);
                    blogEntryMetaData.put(BLOG_ENTRY_METADATA_AUTHOR, userid);
                    if (dateCreated == null) {
                        blogEntryMetaData.put(BLOG_ENTRY_METADATA_TIMESTAMP, new Long(new Date().getTime()).toString());
                    } else {
                        blogEntryMetaData.put(BLOG_ENTRY_METADATA_TIMESTAMP, convertDateCreated(dateCreated, _blog));
                    }
                    entry.setMetaData(blogEntryMetaData);
                    entry.save(_blogUser);
                    entry.load(_blogUser);
                    result = postid;

                    // Send out an add blog entry event
                    _configuration.getEventBroadcaster().broadcastEvent(new AddBlogEntryEvent(this, new Date(), entry, _blogUser));
                } catch (BlojsomException e) {
                    _logger.error(e);
                    throw new XmlRpcException(UNKNOWN_EXCEPTION, UNKNOWN_EXCEPTION_MSG);
                }
            }

            return result;
        } catch (BlojsomException e) {
            _logger.error("Failed to authenticate user [" + userid + "] with password [" + password + "]");
            throw new XmlRpcException(AUTHORIZATION_EXCEPTION, AUTHORIZATION_EXCEPTION_MSG);
        }
    }

    /**
     * Edits a given post. Optionally, will publish the blog after making the edit
     *
     * @param postid   Unique identifier of the post to be changed
     * @param userid   Login for a MetaWeblog user who has permission to post to the blog
     * @param password Password for said username
     * @param struct   Contents of the post
     * @param publish  If true, the blog will be published immediately after the post is made
     * @return <code>true</code> if the entry was edited, <code>false</code> otherwise
     * @throws XmlRpcException If the user was not authenticated correctly, if there was an I/O exception,
     *                         or if the entry permalink ID is invalid
     */
    public boolean editPost(String postid, String userid, String password, Hashtable struct, boolean publish) throws Exception {
        _logger.debug("editPost() Called ========[ SUPPORTED ]=====");
        _logger.debug("     PostId: " + postid);
        _logger.debug("     UserId: " + userid);
        _logger.debug("   Password: *********");
        _logger.debug("    Publish: " + publish);

        try {
            _authorizationProvider.loadAuthenticationCredentials(_blogUser);
            _authorizationProvider.authorize(_blogUser, null, userid, password);
            checkXMLRPCPermission(userid, METAWEBLOG_API_PERMISSION);

            boolean result = false;

            String category = null;
            String permalink = null;
            String match = "?" + PERMALINK_PARAM + "=";

            int pos = postid.indexOf(match);

            // Look for categories in struct
            if (pos == -1) {
                Vector categories = (Vector) struct.get(MEMBER_CATEGORIES);
                if (categories == null) {
                    throw new XmlRpcException(INVALID_POSTID, INVALID_POSTID_MSG);
                } else {
                    category = (String) categories.get(0);
                    permalink = postid;
                }
            } else if (pos != -1) {
                category = postid.substring(0, pos);
                category = BlojsomUtils.normalize(category);
                category = BlojsomUtils.urlDecode(category);
                permalink = postid.substring(pos + match.length());
            }

            BlogCategory blogCategory = _fetcher.newBlogCategory();
            blogCategory.setCategory(category);
            blogCategory.setCategoryURL(_blog.getBlogURL() + category);

            Map fetchMap = new HashMap();
            fetchMap.put(BlojsomFetcher.FETCHER_CATEGORY, blogCategory);
            fetchMap.put(BlojsomFetcher.FETCHER_PERMALINK, permalink);
            BlogEntry[] entries = _fetcher.fetchEntries(fetchMap, _blogUser);

            if (entries != null && entries.length > 0) {
                BlogEntry entry = entries[0];

                try {
                    Hashtable postcontent = struct;

                    String title = (String) postcontent.get(MEMBER_TITLE);
                    String description = (String) postcontent.get(MEMBER_DESCRIPTION);
                    Date dateCreated = (Date) postcontent.get(MEMBER_DATE_CREATED);

                    if (title == null) {
                        title = "No Title";
                    }

                    String hashable = description;

                    if (description.length() > MAX_HASHABLE_LENGTH) {
                        hashable = hashable.substring(0, MAX_HASHABLE_LENGTH);
                    }

                    Map blogEntryMetaData = entry.getMetaData();
                    entry.setTitle(title);
                    entry.setDescription(description);
                    if (dateCreated != null) {
                        blogEntryMetaData.put(BLOG_ENTRY_METADATA_TIMESTAMP, convertDateCreated(dateCreated, _blog));
                    }

                    entry.save(_blogUser);
                    result = true;

                    // Send out an updated blog entry event
                    _configuration.getEventBroadcaster().broadcastEvent(new UpdatedBlogEntryEvent(this, new Date(), entry, _blogUser));
                } catch (BlojsomException e) {
                    _logger.error(e);
                    throw new XmlRpcException(UNKNOWN_EXCEPTION, UNKNOWN_EXCEPTION_MSG);
                }
            } else {
                throw new XmlRpcException(INVALID_POSTID, INVALID_POSTID_MSG);
            }

            return result;
        } catch (BlojsomException e) {
            _logger.error("Failed to authenticate user [" + userid + "] with password [" + password + "]");
            throw new XmlRpcException(AUTHORIZATION_EXCEPTION, AUTHORIZATION_EXCEPTION_MSG);
        }
    }

    /**
     * Retrieves a given post from the blog
     *
     * @param postid   Unique identifier of the post to be changed
     * @param userid   Login for a MetaWeblog user who has permission to post to the blog
     * @param password Password for said username
     * @return Structure containing the minimal attributes for the MetaWeblog API getPost() method: title, link, and description
     * @throws XmlRpcException If the user was not authenticated correctly, if there was an I/O exception,
     *                         or if the entry permalink ID is invalid
     * @since blojsom 1.9.4
     */
    public Object getPost(String postid, String userid, String password) throws Exception {
        _logger.debug("getPost() Called =========[ SUPPORTED ]=====");
        _logger.debug("     PostId: " + postid);
        _logger.debug("     UserId: " + userid);
        _logger.debug("   Password: *********");

        try {
            _authorizationProvider.loadAuthenticationCredentials(_blogUser);
            _authorizationProvider.authorize(_blogUser, null, userid, password);
            checkXMLRPCPermission(userid, METAWEBLOG_API_PERMISSION);

            String category;
            String permalink;
            String match = "?" + PERMALINK_PARAM + "=";

            int pos = postid.indexOf(match);
            if (pos != -1) {
                category = postid.substring(0, pos);
                category = BlojsomUtils.normalize(category);
                category = BlojsomUtils.urlDecode(category);
                permalink = postid.substring(pos + match.length());

                BlogCategory blogCategory = _fetcher.newBlogCategory();
                blogCategory.setCategory(category);
                blogCategory.setCategoryURL(_blog.getBlogURL() + category);

                Map fetchMap = new HashMap();
                fetchMap.put(BlojsomFetcher.FETCHER_CATEGORY, blogCategory);
                fetchMap.put(BlojsomFetcher.FETCHER_PERMALINK, permalink);
                BlogEntry[] entries = _fetcher.fetchEntries(fetchMap, _blogUser);

                if (entries != null && entries.length > 0) {
                    BlogEntry entry = entries[0];

                    Hashtable postcontent = new Hashtable(3);
                    postcontent.put(MEMBER_TITLE, entry.getTitle());
                    postcontent.put(MEMBER_LINK, entry.getPermalink());
                    postcontent.put(MEMBER_DESCRIPTION, entry.getDescription());
                    postcontent.put(MEMBER_DATE_CREATED, entry.getDate());
                    postcontent.put(MEMBER_PERMALINK, entry.getLink());
                    postcontent.put(MEMBER_POSTID, entry.getId());

                    Vector postCategories = new Vector(1);
                    postCategories.add(entry.getCategory());
                    postcontent.put(MEMBER_CATEGORIES, postCategories);

                    return postcontent;
                } else {
                    throw new XmlRpcException(INVALID_POSTID, INVALID_POSTID_MSG);
                }
            } else {
                throw new XmlRpcException(INVALID_POSTID, INVALID_POSTID_MSG);
            }
        } catch (BlojsomException e) {
            _logger.error("Failed to authenticate user [" + userid + "] with password [" + password + "]");
            throw new XmlRpcException(AUTHORIZATION_EXCEPTION, AUTHORIZATION_EXCEPTION_MSG);
        }
    }

    /**
     * Delete a Post
     *
     * @param appkey Unique identifier/passcode of the application sending the post
     * @param postid Unique identifier of the post to be changed
     * @param userid Login for a Blogger user who has permission to post to the blog
     * @param password Password for said username
     * @param publish Ignored
     * @throws XmlRpcException
     * @return <code>true</code> if the entry was delete, <code>false</code> otherwise
     */
    public boolean deletePost(String appkey, String postid, String userid, String password, boolean publish) throws Exception {
        _logger.debug("deletePost() Called =====[ SUPPORTED ]=====");
        _logger.debug("     Appkey: " + appkey);
        _logger.debug("     PostId: " + postid);
        _logger.debug("     UserId: " + userid);
        _logger.debug("   Password: *********");

        boolean result = false;

        try {
            _authorizationProvider.loadAuthenticationCredentials(_blogUser);
            _authorizationProvider.authorize(_blogUser, null, userid, password);
            checkXMLRPCPermission(userid, METAWEBLOG_API_PERMISSION);

            String category;
            String permalink;
            String match = "?" + PERMALINK_PARAM + "=";

            int pos = postid.indexOf(match);
            if (pos != -1) {
                category = postid.substring(0, pos);
                category = BlojsomUtils.normalize(category);
                category = BlojsomUtils.urlDecode(category);
                permalink = postid.substring(pos + match.length());

                Map fetchMap = new HashMap();
                BlogCategory blogCategory = _fetcher.newBlogCategory();
                blogCategory.setCategory(category);
                blogCategory.setCategoryURL(_blog.getBlogURL() + category);
                fetchMap.put(BlojsomFetcher.FETCHER_CATEGORY, blogCategory);
                fetchMap.put(BlojsomFetcher.FETCHER_PERMALINK, permalink);
                BlogEntry[] _entries = _fetcher.fetchEntries(fetchMap, _blogUser);

                if (_entries != null && _entries.length > 0) {
                    try {
                        _entries[0].delete(_blogUser);
                    } catch (BlojsomException e) {
                        _logger.error(e);
                        throw new XmlRpcException(UNKNOWN_EXCEPTION, UNKNOWN_EXCEPTION_MSG);
                    }
                    result = true;

                    // Send out a deleted blog entry event
                    _configuration.getEventBroadcaster().broadcastEvent(new DeletedBlogEntryEvent(this, new Date(), _entries[0], _blogUser));                    
                } else {
                    throw new XmlRpcException(INVALID_POSTID, INVALID_POSTID_MSG);
                }
            }
        } catch (BlojsomException e) {
            _logger.error("Failed to authenticate user [" + userid + "] with password [" + password + "]");
            throw new XmlRpcException(AUTHORIZATION_EXCEPTION, AUTHORIZATION_EXCEPTION_MSG);
        }

        return result;
    }

    /**
     * Retrieves a set of recent posts to the blog
     *
     * @param blogid        Unique identifier of the blog the post will be added to
     * @param userid        Login for a MetaWeblog user who has permission to post to the blog
     * @param password      Password for said username
     * @param numberOfPosts Number of posts to be retrieved from the blog
     * @return Array of structures containing the minimal attributes for the MetaWeblog API getPost() method: title, link, and description
     * @throws Exception If the user was not authenticated correctly
     * @since blojsom 1.9.5
     */
    public Object getRecentPosts(String blogid, String userid, String password, int numberOfPosts) throws Exception {
        _logger.debug("getRecentPosts() Called =========[ SUPPORTED ]=====");
        _logger.debug("     BlogId: " + blogid);
        _logger.debug("     UserId: " + userid);
        _logger.debug("   Password: *********");

        try {
            _authorizationProvider.loadAuthenticationCredentials(_blogUser);
            _authorizationProvider.authorize(_blogUser, null, userid, password);
            checkXMLRPCPermission(userid, METAWEBLOG_API_PERMISSION);

            blogid = BlojsomUtils.normalize(blogid);
            BlogCategory category = _fetcher.newBlogCategory();
            category.setCategory(blogid);
            category.setCategoryURL(_blog.getBlogURL() + blogid);

            Map fetchMap = new HashMap();
            fetchMap.put(BlojsomFetcher.FETCHER_CATEGORY, category);
            fetchMap.put(BlojsomFetcher.FETCHER_NUM_POSTS_INTEGER, new Integer(numberOfPosts));

            BlogEntry[] entries = _fetcher.fetchEntries(fetchMap, _blogUser);
            Vector blogEntries = new Vector();
            Hashtable postcontent;
            if (entries != null && entries.length > 0) {
                blogEntries = new Vector(entries.length);
                BlogEntry entry;
                for (int i = 0; i < entries.length; i++) {
                    entry = entries[i];
                    postcontent = new Hashtable(7);
                    postcontent.put(MEMBER_TITLE, entry.getTitle());
                    postcontent.put(MEMBER_LINK, entry.getPermalink());
                    postcontent.put(MEMBER_DESCRIPTION, entry.getDescription());
                    postcontent.put(MEMBER_DATE_CREATED, entry.getDate());
                    postcontent.put(MEMBER_PERMALINK, entry.getLink());
                    postcontent.put(MEMBER_POSTID, entry.getId());

                    Vector postCategories = new Vector(1);
                    postCategories.add(entry.getCategory());
                    postcontent.put(MEMBER_CATEGORIES, postCategories);
                    blogEntries.add(postcontent);
                }
            }

            return blogEntries;
        } catch (BlojsomException e) {
            _logger.error("Failed to authenticate user [" + userid + "] with password [" + password + "]");
            throw new XmlRpcException(AUTHORIZATION_EXCEPTION, AUTHORIZATION_EXCEPTION_MSG);
        }
    }

    /**
     * Uploads an object to the blog to a specified directory
     *
     * @param blogid   Unique identifier of the blog the post will be added to
     * @param userid   Login for a MetaWeblog user who has permission to post to the blog
     * @param password Password for said username
     * @param struct   Upload structure defined by the MetaWeblog API
     * @return Structure containing a link to the uploaded media object
     * @throws XmlRpcException If the user was not authenticated correctly, if there was an I/O exception,
     *                         or if the MIME type of the upload object is not accepted
     * @since blojsom 1.9.4
     */
    public Object newMediaObject(String blogid, String userid, String password, Hashtable struct) throws Exception {
        _logger.debug("newMediaObject() Called =[ SUPPORTED ]=====");
        _logger.debug("     BlogId: " + blogid);
        _logger.debug("     UserId: " + userid);
        _logger.debug("   Password: *********");

        try {
            _authorizationProvider.loadAuthenticationCredentials(_blogUser);
            _authorizationProvider.authorize(_blogUser, null, userid, password);
            checkXMLRPCPermission(userid, METAWEBLOG_API_PERMISSION);
                   
            String name = (String) struct.get(MEMBER_NAME);
            name = BlojsomUtils.getFilenameFromPath(name);
            _logger.debug("newMediaObject name: " + name);
            String type = (String) struct.get(MEMBER_TYPE);
            _logger.debug("newMediaObject type: " + type);
            byte[] bits = (byte[]) struct.get(MEMBER_BITS);

            File uploadDirectory = new File(_uploadDirectory);
            if (!uploadDirectory.exists()) {
                _logger.error("Upload directory does not exist: " + uploadDirectory.toString());
                throw new XmlRpcException(UNKNOWN_EXCEPTION, "Upload directory does not exist: " + uploadDirectory.toString());
            }

            if (_acceptedMimeTypes.containsKey(type.toLowerCase())) {
                try {
                    File uploadDirectoryForUser = new File(uploadDirectory, _blogUser.getId());
                    if (!uploadDirectoryForUser.exists()) {
                        if (!uploadDirectoryForUser.mkdir()) {
                            _logger.error("Could not create upload directory for user: " + uploadDirectoryForUser.toString());
                            throw new XmlRpcException(UNKNOWN_EXCEPTION, "Could not create upload directory for user: " + _blogUser.getId());
                        }
                    }
					// create any intermediate dirs...
					File mediaObjectFile = new File(name); // not absolute
					File parentDirs = mediaObjectFile.getParentFile();
					if (parentDirs != null) {
						String parentDirsString = parentDirs.toString();
						if (parentDirsString.startsWith("/")) {
							parentDirsString = parentDirsString.substring(1);
						}
						File uploadMediaDirectory = new File(uploadDirectoryForUser, parentDirsString);
						_logger.debug("upload directory for this media object: " + uploadMediaDirectory.toString());
						if (!uploadMediaDirectory.exists()) {
							if (!uploadMediaDirectory.mkdirs()) {
								_logger.error("Could not create upload directory: " + uploadMediaDirectory.toString());
								throw new XmlRpcException(UNKNOWN_EXCEPTION, "Could not create upload directory: " + uploadMediaDirectory.toString());
							}
						}
					}
					
					String slashOrNot = "/";
					if (_blogUser.getId().endsWith("/")) {
						slashOrNot = "";
					}
                    BufferedOutputStream bos = new BufferedOutputStream(new FileOutputStream(new File(uploadDirectoryForUser, name)));
                    bos.write(bits);
                    bos.close();

                    Hashtable returnStruct = new Hashtable(1);
                    String mediaURL = _blog.getBlogBaseURL() + _staticURLPrefix + BlojsomUtils.removeTrailingSlash(_blogUser.getId()) + "/" + name;
                    _logger.debug("newMediaObject URL: " + mediaURL);
                    returnStruct.put(MEMBER_URL, mediaURL);

                    return returnStruct;
                } catch (IOException e) {
                    _logger.error(e);
                    throw new XmlRpcException(UNKNOWN_EXCEPTION, UNKNOWN_EXCEPTION_MSG);
                }
            } else {
                _logger.error("MIME type not accepted. Received MIME type: " + type);
                throw new XmlRpcException(UNKNOWN_EXCEPTION, "MIME type not accepted. Received MIME type: " + type);
            }
        } catch (BlojsomException e) {
            _logger.error("Failed to authenticate user [" + userid + "] with password [" + password + "]");
            throw new XmlRpcException(AUTHORIZATION_EXCEPTION, AUTHORIZATION_EXCEPTION_MSG);
        }
    }

    /**
     * Edits the main or archive index template of a given blog (NOT IMPLEMENTED)
     *
     * @param appkey Unique identifier/passcode of the application sending the post
     * @param blogid Unique identifier of the blog the post will be added to
     * @param userid Login for a Blogger user who has permission to post to the blog
     * @param password Password for said username
     * @param template The text for the new template (usually mostly HTML). Must contain opening and closing <Blogger> tags, since they're needed to publish
     * @param templateType Determines which of the blog's templates will be returned. Currently, either "main" or "archiveIndex"
     * @throws XmlRpcException
     * @return
     */
    public boolean setTemplate(String appkey, String blogid, String userid, String password, String template, String templateType) throws Exception {
        _logger.debug("setTemplate() Called =====[ UNSUPPORTED ]=====");
        _logger.debug("     Appkey: " + appkey);
        _logger.debug("     BlogId: " + blogid);
        _logger.debug("     UserId: " + userid);
        _logger.debug("   Password: *********");
        _logger.debug("   Template: " + template);
        _logger.debug("       Type: " + templateType);

        throw new XmlRpcException(UNSUPPORTED_EXCEPTION, UNSUPPORTED_EXCEPTION_MSG);
    }

    /**
     * Returns the main or archive index template of a given blog (NOT IMPLEMENTED)
     *
     * @param appkey Unique identifier/passcode of the application sending the post
     * @param blogid Unique identifier of the blog the post will be added to
     * @param userid Login for a Blogger user who has permission to post to the blog
     * @param password Password for said username
     * @param templateType Determines which of the blog's templates will be returned. Currently, either "main" or "archiveIndex"
     * @throws XmlRpcException
     * @return
     */
    public String getTemplate(String appkey, String blogid, String userid, String password, String templateType) throws Exception {
        _logger.debug("getTemplate() Called =====[ UNSUPPORTED ]=====");
        _logger.debug("     Appkey: " + appkey);
        _logger.debug("     BlogId: " + blogid);
        _logger.debug("     UserId: " + userid);
        _logger.debug("   Password: *********");
        _logger.debug("       Type: " + templateType);

        throw new XmlRpcException(UNSUPPORTED_EXCEPTION, UNSUPPORTED_EXCEPTION_MSG);
    }

    /**
     * Convert the dateCreated attribute to the local timezone
     *
     * @param dateCreated Date indicating the date and time created for the entry
     * @param blog {@link Blog} infomation
     * @return Date converted to a long (as String)
     */
    private String convertDateCreated(Date dateCreated, Blog blog) {
        if (dateCreated == null) {
            return Long.toString(new Date().getTime());
        } else {
            String timezoneID = blog.getBlogProperty("blog-timezone-id");
            TimeZone timezone;

            if (BlojsomUtils.checkNullOrBlank(timezoneID)) {
                timezone = TimeZone.getDefault();
            } else {
                timezone = TimeZone.getTimeZone(timezoneID);
            }

            SimpleDateFormat simpleDateFormat = new SimpleDateFormat(ISO_8601_DATE_FORMAT);
            simpleDateFormat.setTimeZone(timezone);

            long convertedDateTime = dateCreated.getTime();
            try {
                convertedDateTime = Long.parseLong(simpleDateFormat.format(dateCreated));
            } catch (NumberFormatException e) {
            }

            return Long.toString(convertedDateTime);
        }
    }
}