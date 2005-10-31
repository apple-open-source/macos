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
import org.blojsom.fetcher.BlojsomFetcher;
import org.blojsom.util.BlojsomUtils;

import java.io.File;
import java.util.*;

/**
 * Blojsom XML-RPC Handler for the Blogger v1.0 API
 *
 * Blogger API spec can be found at http://plant.blogger.com/api/index.html
 *
 * @author Mark Lussier
 * @version $Id: BloggerAPIHandler.java,v 1.2.2.1 2005/07/21 04:30:23 johnan Exp $
 */
public class BloggerAPIHandler extends AbstractBlojsomAPIHandler {

    /**
     * Blogger API "url" key
     */
    private static final String MEMBER_URL = "url";

    /**
     * Blogger API "blogid" key
     */
    private static final String MEMBER_BLOGID = "blogid";

    /**
     * Blogger APU "postid" key
     */
    private static final String MEMBER_POSTID = "postid";

    /**
     * Blogger API "blogName" key
     */
    private static final String MEMBER_BLOGNAME = "blogName";

    /**
     * Blogger API "title" key
     */
    private static final String MEMBER_TITLE = "title";

    /**
     * Blogger API "content" key
     */
    private static final String MEMBER_CONTENT = "content";

    /**
     * Blogger API "dateCreated" key
     */
    private static final String MEMBER_DATECREATED = "dateCreated";

    /**
     * Blogger API "authorName" key
     */
    private static final String MEMBER_AUTHORNAME = "authorName";

    /**
     * Blogger API "authorEmail" key
     */
    private static final String MEMBER_AUTHOREMAIL = "authorEmail";

    /**
     * Blogger API "nickname" key
     */
    private static final String MEMBER_NICKNAME = "nickname";

    /**
     * Blogger API "userid" key
     */
    private static final String MEMBER_USERID = "userid";

    /**
     * Blogger API "email" key
     */
    private static final String MEMBER_EMAIL = "email";

    /**
     * Blogger API "firstname" key
     */
    private static final String MEMBER_FIRSTNAME = "firstname";

    /**
     * Blogger API "lastname" key
     */
    private static final String MEMBER_LASTNAME = "lastname";

    private static final String TITLE_TAG_START = "<title>";
    private static final String TITLE_TAG_END = "</title>";

    public static final String API_PREFIX = "blogger";

    private static final String BLOGGER_API_PERMISSION = "post_via_blogger_api";

    private Log _logger = LogFactory.getLog(BloggerAPIHandler.class);

    /**
     * Default constructor
     */
    public BloggerAPIHandler() {
    }

    /**
     * Gets the Name of API Handler. Used to Bind to XML-RPC
     *
     * @return The API Name (ie: blogger)
     */
    public String getName() {
        return API_PREFIX;
    }

    /**
     * Attach a Blog instance to the API Handler so that it can interact with the blog
     *
     * @param blogUser an instance of BlogUser
     * @see org.blojsom.blog.BlogUser
     * @throws BlojsomException If there is an error setting the blog instance or properties for the handler
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
     * Authenticates a user and returns basic user info (name, email, userid, etc.).
     *
     * @param appkey Unique identifier/passcode of the application sending the post
     * @param userid Login for a Blogger user who has permission to post to the blog
     * @param password Password for said username
     * @throws XmlRpcException
     * @return
     */
    public Object getUserInfo(String appkey, String userid, String password) throws Exception {
        _logger.debug("getUserInfo() Called =====[ SUPPORTED ]=======");
        _logger.debug("     Appkey: " + appkey);
        _logger.debug("     UserId: " + userid);
        _logger.debug("   Password: *********");

        try {
            _authorizationProvider.loadAuthenticationCredentials(_blogUser);
            _authorizationProvider.authorize(_blogUser, null, userid, password);
            checkXMLRPCPermission(userid, BLOGGER_API_PERMISSION);

            Hashtable userinfo = new Hashtable();
            userinfo.put(MEMBER_EMAIL, _blog.getBlogOwnerEmail());
            userinfo.put(MEMBER_NICKNAME, userid);
            userinfo.put(MEMBER_USERID, "1");
            userinfo.put(MEMBER_URL, _blog.getBlogURL());

            String _ownerName = _blog.getBlogOwner();
            int _split = _ownerName.indexOf(" ");
            if (_split > 0) {
                userinfo.put(MEMBER_FIRSTNAME, _ownerName.substring(0, _split));
                userinfo.put(MEMBER_LASTNAME, _ownerName.substring(_split + 1));
            } else {
                userinfo.put(MEMBER_FIRSTNAME, "blojsom");
                userinfo.put(MEMBER_LASTNAME, _ownerName);
            }

            return userinfo;

        } catch (BlojsomException e) {
            _logger.error("Failed to authenticate user [" + userid + "] with password [" + password + "]");
            throw new XmlRpcException(AUTHORIZATION_EXCEPTION, AUTHORIZATION_EXCEPTION_MSG);
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
            checkXMLRPCPermission(userid, BLOGGER_API_PERMISSION);

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
     * Find a title in a content string delimited by &lt;title&gt;...&lt;/title&gt;
     *
     * @param content Content
     * @return Title found in content or <code>null</code> if the title was not present in &lt;title&gt; tags
     */
    private String findTitleInContent(String content) {
        String titleFromContent = null;
        int titleTagStartIndex = content.indexOf(TITLE_TAG_START);

        if (titleTagStartIndex != -1) {
            int titleTagEndIndex = content.indexOf(TITLE_TAG_END);

            if (titleTagEndIndex != -1 && (titleTagEndIndex > titleTagStartIndex)) {
                titleFromContent = content.substring(titleTagStartIndex + TITLE_TAG_START.length(), titleTagEndIndex);
            }
        }

        return titleFromContent;
    }

    /**
     * Makes a new post to a designated blog. Optionally, will publish the blog after making the post
     *
     * @param appkey Unique identifier/passcode of the application sending the post
     * @param blogid Unique identifier of the blog the post will be added to
     * @param userid Login for a Blogger user who has permission to post to the blog
     * @param password Password for said username
     * @param content Contents of the post
     * @param publish If true, the blog will be published immediately after the post is made
     * @throws XmlRpcException If the user was not authenticated correctly or if there was an I/O exception
     * @return Post ID of the added entry
     */
    public String newPost(String appkey, String blogid, String userid, String password, String content, boolean publish) throws Exception {
        _logger.debug("newPost() Called ===========[ SUPPORTED ]=====");
        _logger.debug("     Appkey: " + appkey);
        _logger.debug("     BlogId: " + blogid);
        _logger.debug("     UserId: " + userid);
        _logger.debug("   Password: *********");
        _logger.debug("    Publish: " + publish);
        _logger.debug("     Content:\n " + content);

        blogid = BlojsomUtils.normalize(blogid);

        try {
            _authorizationProvider.loadAuthenticationCredentials(_blogUser);
            _authorizationProvider.authorize(_blogUser, null, userid, password);
            checkXMLRPCPermission(userid, BLOGGER_API_PERMISSION);

            String result = null;

            // Quick verify that the category is valid
            File blogCategory = getBlogCategoryDirectory(blogid);
            if (blogCategory.exists() && blogCategory.isDirectory()) {
                String title = findTitleInContent(content);
                String filename = BlojsomUtils.getBlogEntryFilename(title, content);
                String outputfile = blogCategory.getAbsolutePath() + File.separator + filename;

                try {
                    File sourceFile = new File(outputfile + _blogEntryExtension);
                    int fileTag = 1;
                    while (sourceFile.exists()) {
                        sourceFile = new File(outputfile + "-" + fileTag + _blogEntryExtension);
                        fileTag++;
                    }
                    String postid = blogid + "?" + PERMALINK_PARAM + "=" + BlojsomUtils.urlEncode(sourceFile.getName());
                    BlogEntry entry = _fetcher.newBlogEntry();
                    Map attributeMap = new HashMap();
                    Map blogEntryMetaData = new HashMap();

                    attributeMap.put(SOURCE_ATTRIBUTE, sourceFile);
                    entry.setAttributes(attributeMap);
                    entry.setCategory(blogid);
                    if (title != null) {
                        content = BlojsomUtils.replace(content, TITLE_TAG_START + title + TITLE_TAG_END, "");
                        entry.setTitle(title);
                    }
                    entry.setDescription(content);
                    blogEntryMetaData.put(BLOG_ENTRY_METADATA_AUTHOR, userid);
                    blogEntryMetaData.put(BLOG_ENTRY_METADATA_TIMESTAMP, new Long(new Date().getTime()).toString());
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
     * @param appkey Unique identifier/passcode of the application sending the post
     * @param postid Unique identifier of the post to be changed
     * @param userid Login for a Blogger user who has permission to post to the blog
     * @param password Password for said username
     * @param content Contents of the post
     * @param publish If true, the blog will be published immediately after the post is made
     * @throws XmlRpcException If the user was not authenticated correctly, if there was an I/O exception,
     * or if the entry permalink ID is invalid
     * @return <code>true</code> if the entry was edited, <code>false</code> otherwise
     */
    public boolean editPost(String appkey, String postid, String userid, String password, String content, boolean publish) throws Exception {
        _logger.debug("editPost() Called ========[ SUPPORTED ]=====");
        _logger.debug("     Appkey: " + appkey);
        _logger.debug("     PostId: " + postid);
        _logger.debug("     UserId: " + userid);
        _logger.debug("   Password: *********");
        _logger.debug("    Publish: " + publish);
        _logger.debug("     Content:\n " + content);

        try {
            _authorizationProvider.loadAuthenticationCredentials(_blogUser);
            _authorizationProvider.authorize(_blogUser, null, userid, password);
            checkXMLRPCPermission(userid, BLOGGER_API_PERMISSION);

            boolean result = false;

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
                    BlogEntry _entry = _entries[0];
                    try {
                        String title = findTitleInContent(content);
                        if (title != null) {
                            content = BlojsomUtils.replace(content, TITLE_TAG_START + title + TITLE_TAG_END, "");
                            _entry.setTitle(title);
                        } else {
                            _entry.setTitle(null);
                        }
                        _entry.setDescription(content);
                        _entry.save(_blogUser);


                        result = true;

                        // Send out an updated blog entry event
                        _configuration.getEventBroadcaster().broadcastEvent(new UpdatedBlogEntryEvent(this, new Date(), _entry, _blogUser));
                    } catch (BlojsomException e) {
                        _logger.error(e);
                        throw new XmlRpcException(UNKNOWN_EXCEPTION, UNKNOWN_EXCEPTION_MSG);
                    }
                } else {
                    throw new XmlRpcException(INVALID_POSTID, INVALID_POSTID_MSG);
                }
            }

            return result;
        } catch (BlojsomException e) {
            _logger.error("Failed to authenticate user [" + userid + "] with password [" + password + "]");
            throw new XmlRpcException(AUTHORIZATION_EXCEPTION, AUTHORIZATION_EXCEPTION_MSG);
        }
    }

    /**
     * Get a particular post for a blojsom category
     *
     * @since blojsom 1.9.3
     * @param appkey Unique identifier/passcode of the application sending the post
     * @param blogid Unique identifier of the blog the post will be added to
     * @param userid Login for a Blogger user who has permission to post to the blog
     * @param password Password for said username
     * @throws XmlRpcException If the user was not authenticated correctly
     * @return Post to the blog
     */
    public Object getPost(String appkey, String blogid, String userid, String password) throws Exception {
        _logger.debug("getPost() Called ===========[ SUPPORTED ]=====");
        _logger.debug("     Appkey: " + appkey);
        _logger.debug("     BlogId: " + blogid);
        _logger.debug("     UserId: " + userid);
        _logger.debug("   Password: *********");

        try {
            _authorizationProvider.loadAuthenticationCredentials(_blogUser);
            _authorizationProvider.authorize(_blogUser, null, userid, password);
            checkXMLRPCPermission(userid, BLOGGER_API_PERMISSION);

            String category;
            String permalink;
            String match = "?" + PERMALINK_PARAM + "=";

            int pos = blogid.indexOf(match);
            if (pos != -1) {
                category = blogid.substring(0, pos);
                category = BlojsomUtils.normalize(category);
                category = BlojsomUtils.urlDecode(category);
                permalink = blogid.substring(pos + match.length());

                Map fetchMap = new HashMap();
                BlogCategory blogCategory = _fetcher.newBlogCategory();
                blogCategory.setCategory(category);
                blogCategory.setCategoryURL(_blog.getBlogURL() + category);
                fetchMap.put(BlojsomFetcher.FETCHER_CATEGORY, blogCategory);
                fetchMap.put(BlojsomFetcher.FETCHER_PERMALINK, permalink);
                BlogEntry[] _entries = _fetcher.fetchEntries(fetchMap, _blogUser);

                if (_entries != null && _entries.length > 0) {
                    BlogEntry entry = _entries[0];
                    Hashtable entrystruct = new Hashtable();
                    entrystruct.put(MEMBER_POSTID, entry.getId());
                    entrystruct.put(MEMBER_BLOGID, entry.getCategory());
                    entrystruct.put(MEMBER_TITLE, entry.getTitle());
                    entrystruct.put(MEMBER_URL, entry.getEscapedLink());
                    entrystruct.put(MEMBER_CONTENT, entry.getTitle() + LINE_SEPARATOR + entry.getDescription());
                    entrystruct.put(MEMBER_DATECREATED, entry.getDate());
                    entrystruct.put(MEMBER_AUTHORNAME, _blog.getBlogOwner());
                    entrystruct.put(MEMBER_AUTHOREMAIL, _blog.getBlogOwnerEmail());

                    return entrystruct;
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
            checkXMLRPCPermission(userid, BLOGGER_API_PERMISSION);

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
     * Get a list of recent posts for a blojsom category
     *
     * @param appkey Unique identifier/passcode of the application sending the post
     * @param blogid Unique identifier of the blog the post will be added to
     * @param userid Login for a Blogger user who has permission to post to the blog
     * @param password Password for said username
     * @param numposts Number of Posts to Retrieve
     * @throws XmlRpcException If the user was not authenticated correctly
     * @return Recent posts to the blog
     */
    public Object getRecentPosts(String appkey, String blogid, String userid, String password, int numposts) throws Exception {
        _logger.debug("getRecentPosts() Called ===========[ SUPPORTED ]=====");
        _logger.debug("     Appkey: " + appkey);
        _logger.debug("     BlogId: " + blogid);
        _logger.debug("     UserId: " + userid);
        _logger.debug("   Password: *********");
        _logger.debug("   Numposts: " + numposts);

        Vector recentPosts = new Vector();
        blogid = BlojsomUtils.normalize(blogid);

        try {
            _authorizationProvider.loadAuthenticationCredentials(_blogUser);
            _authorizationProvider.authorize(_blogUser, null, userid, password);
            checkXMLRPCPermission(userid, BLOGGER_API_PERMISSION);

            // Quick verify that the categories are valid
            File blogCategoryFile = new File(_blog.getBlogHome() + BlojsomUtils.removeInitialSlash(blogid));
            if (blogCategoryFile.exists() && blogCategoryFile.isDirectory()) {

                String requestedCategory = BlojsomUtils.removeInitialSlash(blogid);
                BlogCategory blogCategory = _fetcher.newBlogCategory();
                blogCategory.setCategory(blogid);
                blogCategory.setCategoryURL(_blog.getBlogURL() + requestedCategory);

                BlogEntry[] entries;
                Map fetchMap = new HashMap();

                if (BlojsomUtils.checkNullOrBlank(requestedCategory)) {
                    fetchMap.put(BlojsomFetcher.FETCHER_FLAVOR, DEFAULT_FLAVOR_HTML);
                    fetchMap.put(BlojsomFetcher.FETCHER_NUM_POSTS_INTEGER, new Integer(numposts));
                    entries = _fetcher.fetchEntries(fetchMap, _blogUser);
                } else {
                    fetchMap.put(BlojsomFetcher.FETCHER_CATEGORY, blogCategory);
                    fetchMap.put(BlojsomFetcher.FETCHER_NUM_POSTS_INTEGER, new Integer(numposts));
                    entries = _fetcher.fetchEntries(fetchMap, _blogUser);
                }

                if (entries != null && entries.length > 0) {
                    for (int x = 0; x < entries.length; x++) {
                        BlogEntry entry = entries[x];
                        Hashtable entrystruct = new Hashtable();
                        entrystruct.put(MEMBER_POSTID, entry.getId());
                        entrystruct.put(MEMBER_BLOGID, entry.getCategory());
                        entrystruct.put(MEMBER_TITLE, entry.getTitle());
                        entrystruct.put(MEMBER_URL, entry.getEscapedLink());
                        entrystruct.put(MEMBER_CONTENT, entry.getTitle() + LINE_SEPARATOR + entry.getDescription());
                        entrystruct.put(MEMBER_DATECREATED, entry.getDate());
                        entrystruct.put(MEMBER_AUTHORNAME, _blog.getBlogOwner());
                        entrystruct.put(MEMBER_AUTHOREMAIL, _blog.getBlogOwnerEmail());
                        recentPosts.add(entrystruct);
                    }
                }
            }

            return recentPosts;
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
}
