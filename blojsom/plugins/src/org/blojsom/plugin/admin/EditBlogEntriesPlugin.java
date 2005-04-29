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
import org.apache.xmlrpc.AsyncCallback;
import org.apache.xmlrpc.XmlRpcClient;
import org.blojsom.BlojsomException;
import org.blojsom.blog.*;
import org.blojsom.fetcher.BlojsomFetcher;
import org.blojsom.fetcher.BlojsomFetcherException;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.plugin.trackback.TrackbackPlugin;
import org.blojsom.util.BlojsomMetaDataConstants;
import org.blojsom.util.BlojsomUtils;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.*;
import java.util.*;
import java.net.*;
import java.text.SimpleDateFormat;
import java.text.ParseException;

/**
 * EditBlogEntriesPlugin
 *
 * @author czarnecki
 * @version $Id: EditBlogEntriesPlugin.java,v 1.5 2005/02/18 23:09:27 johnan Exp $
 * @since blojsom 2.05
 */
public class EditBlogEntriesPlugin extends BaseAdminPlugin {

    private Log _logger = LogFactory.getLog(EditBlogEntriesPlugin.class);

    // XML-RPC constants
    public static final String BLOG_XMLRPC_ENTRY_EXTENSION_IP = "blog-xmlrpc-entry-extension";

    private static final String WEBLOGS_PING_METHOD = "weblogUpdates.ping";
    private static final int MAXIMUM_FILENAME_LENGTH = 32;

    /**
     * Default file extension for blog entries written via XML-RPC
     */
    public static final String DEFAULT_BLOG_XMLRPC_ENTRY_EXTENSION = ".txt";

    // Pages
    private static final String EDIT_BLOG_ENTRIES_PAGE = "/org/blojsom/plugin/admin/templates/admin-edit-blog-entries";
    private static final String EDIT_BLOG_ENTRIES_LIST_PAGE = "/org/blojsom/plugin/admin/templates/admin-edit-blog-entries-list";
    private static final String EDIT_BLOG_ENTRY_PAGE = "/org/blojsom/plugin/admin/templates/admin-edit-blog-entry";
    private static final String ADD_BLOG_ENTRY_PAGE = "/org/blojsom/plugin/admin/templates/admin-add-blog-entry";

    // Constants
    private static final String BLOJSOM_PLUGIN_EDIT_BLOG_ENTRIES_LIST = "BLOJSOM_PLUGIN_EDIT_BLOG_ENTRIES_LIST";
    private static final String BLOJSOM_PLUGIN_EDIT_BLOG_ENTRIES_CATEGORY = "BLOJSOM_PLUGIN_EDIT_BLOG_ENTRIES_CATEGORY";
    private static final String BLOJSOM_PLUGIN_EDIT_BLOG_ENTRIES_ENTRY = "BLOJSOM_PLUGIN_EDIT_BLOG_ENTRIES_ENTRY";

    // Actions
    private static final String EDIT_BLOG_ENTRIES_ACTION = "edit-blog-entries";
    private static final String EDIT_BLOG_ENTRY_ACTION = "edit-blog-entry";
    private static final String UPDATE_BLOG_ENTRY_ACTION = "update-blog-entry";
    private static final String DELETE_BLOG_ENTRY_ACTION = "delete-blog-entry";
    private static final String NEW_BLOG_ENTRY_ACTION = "new-blog-entry";
    private static final String ADD_BLOG_ENTRY_ACTION = "add-blog-entry";
    private static final String DELETE_BLOG_COMMENTS = "delete-blog-comments";
    private static final String DELETE_BLOG_TRACKBACKS = "delete-blog-trackbacks";

    // Form elements
    private static final String BLOG_CATEGORY_NAME = "blog-category-name";
    private static final String BLOG_ENTRY_ID = "blog-entry-id";
    private static final String BLOG_ENTRY_TITLE = "blog-entry-title";
    private static final String BLOG_ENTRY_DESCRIPTION = "blog-entry-description";
    private static final String BLOG_COMMENT_ID = "blog-comment-id";
    private static final String BLOG_TRACKBACK_ID = "blog-trackback-id";
    private static final String BLOG_ENTRY_PUBLISH_DATETIME = "blog-entry-publish-datetime";
    private static final String BLOG_TRACKBACK_URLS = "blog-trackback-urls";
    private static final String BLOG_PING_URLS = "blog-ping-urls";
    private static final String BLOG_ENTRY_PROPOSED_NAME = "blog-entry-proposed-name";

    protected BlojsomFetcher _fetcher;

    /**
     * Default constructor.
     */
    public EditBlogEntriesPlugin() {
    }

    /**
     * Initialize this plugin. This method only called when the plugin is instantiated.
     *
     * @param servletConfig        Servlet config object for the plugin to retrieve any initialization parameters
     * @param blojsomConfiguration {@link BlojsomConfiguration} information
     * @throws org.blojsom.plugin.BlojsomPluginException
     *          If there is an error initializing the plugin
     */
    public void init(ServletConfig servletConfig, BlojsomConfiguration blojsomConfiguration) throws BlojsomPluginException {
        super.init(servletConfig, blojsomConfiguration);

        String fetcherClassName = blojsomConfiguration.getFetcherClass();
        try {
            Class fetcherClass = Class.forName(fetcherClassName);
            _fetcher = (BlojsomFetcher) fetcherClass.newInstance();
            _fetcher.init(servletConfig, blojsomConfiguration);
            _logger.info("Added blojsom fetcher: " + fetcherClassName);
        } catch (ClassNotFoundException e) {
            _logger.error(e);
            throw new BlojsomPluginException(e);
        } catch (InstantiationException e) {
            _logger.error(e);
            throw new BlojsomPluginException(e);
        } catch (IllegalAccessException e) {
            _logger.error(e);
            throw new BlojsomPluginException(e);
        } catch (BlojsomFetcherException e) {
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
     * @throws BlojsomPluginException If there is an error processing the blog entries
     */
    public BlogEntry[] process(HttpServletRequest httpServletRequest, HttpServletResponse httpServletResponse, BlogUser user, Map context, BlogEntry[] entries) throws BlojsomPluginException {
        if (!authenticateUser(httpServletRequest, httpServletResponse, context, user)) {
            httpServletRequest.setAttribute(PAGE_PARAM, ADMIN_LOGIN_PAGE);

            return entries;
        }

        String action = BlojsomUtils.getRequestValue(ACTION_PARAM, httpServletRequest);
        if (BlojsomUtils.checkNullOrBlank(action)) {
            _logger.debug("User did not request edit action");
            httpServletRequest.setAttribute(PAGE_PARAM, ADMIN_ADMINISTRATION_PAGE);
        } else if (PAGE_ACTION.equals(action)) {
            _logger.debug("User requested edit blog entries page");

            httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_ENTRIES_PAGE);
        } else if (EDIT_BLOG_ENTRIES_ACTION.equals(action)) {
            _logger.debug("User requested edit blog entries list page");

            String blogCategoryName = BlojsomUtils.getRequestValue(BLOG_CATEGORY_NAME, httpServletRequest);
            blogCategoryName = BlojsomUtils.normalize(blogCategoryName);

            BlogCategory category;
            category = _fetcher.newBlogCategory();
            category.setCategory(blogCategoryName);
            category.setCategoryURL(user.getBlog().getBlogURL() + BlojsomUtils.removeInitialSlash(blogCategoryName));

            Map fetchMap = new HashMap();
            fetchMap.put(BlojsomFetcher.FETCHER_CATEGORY, category);
            fetchMap.put(BlojsomFetcher.FETCHER_NUM_POSTS_INTEGER, new Integer(-1));
            try {
                entries = _fetcher.fetchEntries(fetchMap, user);
                if (entries != null) {
                    _logger.debug("Retrieved " + entries.length + " entries from category: " + blogCategoryName);
                    Arrays.sort(entries, BlojsomUtils.FILE_TIME_COMPARATOR);
                } else {
                    _logger.debug("No entries found in category: " + blogCategoryName);
                }
            } catch (BlojsomFetcherException e) {
                _logger.error(e);
                entries = new BlogEntry[0];
            }

            context.put(BLOJSOM_PLUGIN_EDIT_BLOG_ENTRIES_CATEGORY, blogCategoryName);
            context.put(BLOJSOM_PLUGIN_EDIT_BLOG_ENTRIES_LIST, entries);
            httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_ENTRIES_LIST_PAGE);
        } else if (EDIT_BLOG_ENTRY_ACTION.equals(action)) {
            _logger.debug("User requested edit blog entry action");

            String blogCategoryName = BlojsomUtils.getRequestValue(BLOG_CATEGORY_NAME, httpServletRequest);
            blogCategoryName = BlojsomUtils.normalize(blogCategoryName);
            String blogEntryId = BlojsomUtils.getRequestValue(BLOG_ENTRY_ID, httpServletRequest);
            _logger.debug("Blog entry id: " + blogEntryId);

            BlogCategory category;
            category = _fetcher.newBlogCategory();
            category.setCategory(blogCategoryName);
            category.setCategoryURL(user.getBlog().getBlogURL() + BlojsomUtils.removeInitialSlash(blogCategoryName));

            Map fetchMap = new HashMap();
            fetchMap.put(BlojsomFetcher.FETCHER_CATEGORY, category);
            fetchMap.put(BlojsomFetcher.FETCHER_PERMALINK, blogEntryId);
            try {
                entries = _fetcher.fetchEntries(fetchMap, user);
                if (entries != null) {
                    _logger.debug("Retrieved " + entries.length + " entries from category: " + blogCategoryName);
                    context.put(BLOJSOM_PLUGIN_EDIT_BLOG_ENTRIES_ENTRY, entries[0]);
                } else {
                    _logger.debug("No entries found in category: " + blogCategoryName);
                }
            } catch (BlojsomFetcherException e) {
                _logger.error(e);
                addOperationResultMessage(context, "Unable to retrieve blog entry: " + blogEntryId);
                entries = new BlogEntry[0];
            }

            context.put(BLOJSOM_PLUGIN_EDIT_BLOG_ENTRIES_CATEGORY, blogCategoryName);
            httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_ENTRY_PAGE);
        } else if (UPDATE_BLOG_ENTRY_ACTION.equals(action)) {
            _logger.debug("User requested update blog entry action");

            Blog blog = user.getBlog();
            String blogCategoryName = BlojsomUtils.getRequestValue(BLOG_CATEGORY_NAME, httpServletRequest);
            blogCategoryName = BlojsomUtils.normalize(blogCategoryName);
            String blogEntryId = BlojsomUtils.getRequestValue(BLOG_ENTRY_ID, httpServletRequest);
            String blogEntryDescription = BlojsomUtils.getRequestValue(BLOG_ENTRY_DESCRIPTION, httpServletRequest);
            String blogEntryTitle = BlojsomUtils.getRequestValue(BLOG_ENTRY_TITLE, httpServletRequest);
            if (BlojsomUtils.checkNullOrBlank(blogEntryTitle)) {
                blogEntryDescription = BlojsomUtils.LINE_SEPARATOR + blogEntryDescription;
            }
            String allowComments = BlojsomUtils.getRequestValue(BLOG_METADATA_COMMENTS_DISABLED, httpServletRequest);
            String allowTrackbacks = BlojsomUtils.getRequestValue(BLOG_METADATA_TRACKBACKS_DISABLED, httpServletRequest);
            String blogTrackbackURLs = BlojsomUtils.getRequestValue(BLOG_TRACKBACK_URLS, httpServletRequest);
            String blogPingURLs = BlojsomUtils.getRequestValue(BLOG_PING_URLS, httpServletRequest);

            _logger.debug("Blog entry id: " + blogEntryId);

            BlogCategory category;
            category = _fetcher.newBlogCategory();
            category.setCategory(blogCategoryName);
            category.setCategoryURL(user.getBlog().getBlogURL() + BlojsomUtils.removeInitialSlash(blogCategoryName));

            Map fetchMap = new HashMap();
            fetchMap.put(BlojsomFetcher.FETCHER_CATEGORY, category);
            fetchMap.put(BlojsomFetcher.FETCHER_PERMALINK, blogEntryId);
            try {
                entries = _fetcher.fetchEntries(fetchMap, user);
                if (entries != null) {
                    _logger.debug("Retrieved " + entries.length + " entries from category: " + blogCategoryName);
                    BlogEntry entryToUpdate = entries[0];
                    entryToUpdate.setTitle(blogEntryTitle);
                    entryToUpdate.setDescription(blogEntryDescription);

                    Map entryMetaData = entryToUpdate.getMetaData();
                    if (entryMetaData == null) {
                        entryMetaData = new HashMap();
                    }

                    if (!BlojsomUtils.checkNullOrBlank(allowComments)) {
                        entryMetaData.put(BLOG_METADATA_COMMENTS_DISABLED, "y");
                    } else {
                        entryMetaData.remove(BLOG_METADATA_COMMENTS_DISABLED);
                    }

                    if (!BlojsomUtils.checkNullOrBlank(allowTrackbacks)) {
                        entryMetaData.put(BLOG_METADATA_TRACKBACKS_DISABLED, "y");
                    } else {
                        entryMetaData.remove(BLOG_METADATA_TRACKBACKS_DISABLED);
                    }

                    entryToUpdate.setMetaData(entryMetaData);
                    entryToUpdate.save(user);
                    entryToUpdate.load(user);
                    _logger.debug("Updated blog entry: " + entryToUpdate.getLink());
                    StringBuffer entryLink = new StringBuffer();
                    entryLink.append("<a href=\"").append(user.getBlog().getBlogURL()).append(BlojsomUtils.removeInitialSlash(entryToUpdate.getCategory())).append("?").append(PERMALINK_PARAM).append("=").append(entryToUpdate.getPermalink()).append("\">").append(entryToUpdate.getTitle()).append("</a>");
                    addOperationResultMessage(context, "Updated blog entry: " + entryLink.toString());

                    // Send trackback pings
                    if (!BlojsomUtils.checkNullOrBlank(blogTrackbackURLs)) {
                        sendTrackbackPings(blog, entryToUpdate, blogTrackbackURLs);
                    }

                    // Send update pings to the ping URLs
                    if (!BlojsomUtils.checkNullOrBlank(blogPingURLs)) {
                        sendBlogPings(blog, blogPingURLs);
                    }

                    httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_ENTRY_PAGE);
                    context.put(BLOJSOM_PLUGIN_EDIT_BLOG_ENTRIES_ENTRY, entryToUpdate);
                    context.put(BLOJSOM_PLUGIN_EDIT_BLOG_ENTRIES_CATEGORY, blogCategoryName);
                } else {
                    _logger.debug("No entries found in category: " + blogCategoryName);
                }
            } catch (BlojsomFetcherException e) {
                _logger.error(e);
                addOperationResultMessage(context, "Unable to retrieve blog entry: " + blogEntryId);
                httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_ENTRIES_PAGE);
                entries = new BlogEntry[0];
            } catch (BlojsomException e) {
                _logger.error(e);
                addOperationResultMessage(context, "Unable to retrieve blog entry: " + blogEntryId);
                entries = new BlogEntry[0];
                httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_ENTRIES_PAGE);
            }
        } else if (DELETE_BLOG_ENTRY_ACTION.equals(action)) {
            _logger.debug("User requested delete blog entry action");

            String blogCategoryName = BlojsomUtils.getRequestValue(BLOG_CATEGORY_NAME, httpServletRequest);
            blogCategoryName = BlojsomUtils.normalize(blogCategoryName);
            String blogEntryId = BlojsomUtils.getRequestValue(BLOG_ENTRY_ID, httpServletRequest);
            _logger.debug("Blog entry id: " + blogEntryId);

            BlogCategory category;
            category = _fetcher.newBlogCategory();
            category.setCategory(blogCategoryName);
            category.setCategoryURL(user.getBlog().getBlogURL() + BlojsomUtils.removeInitialSlash(blogCategoryName));

            Map fetchMap = new HashMap();
            fetchMap.put(BlojsomFetcher.FETCHER_CATEGORY, category);
            fetchMap.put(BlojsomFetcher.FETCHER_PERMALINK, blogEntryId);
            try {
                entries = _fetcher.fetchEntries(fetchMap, user);
                if (entries != null) {
                    _logger.debug("Retrieved " + entries.length + " entries from category: " + blogCategoryName);
                    String title = entries[0].getTitle();
                    entries[0].delete(user);
                    addOperationResultMessage(context, "Deleted blog entry: " + title);
                } else {
                    _logger.debug("No entries found in category: " + blogCategoryName);
                }
            } catch (BlojsomFetcherException e) {
                _logger.error(e);
                addOperationResultMessage(context, "Unable to delete blog entry: " + blogEntryId);
                entries = new BlogEntry[0];
            } catch (BlojsomException e) {
                _logger.error(e);
                addOperationResultMessage(context, "Unable to delete blog entry: " + blogEntryId);
                entries = new BlogEntry[0];
            }

            httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_ENTRIES_PAGE);
        } else if (NEW_BLOG_ENTRY_ACTION.equals(action)) {
            _logger.debug("User requested new blog entry action");

            String blogCategoryName = BlojsomUtils.getRequestValue(BLOG_CATEGORY_NAME, httpServletRequest);
            blogCategoryName = BlojsomUtils.normalize(blogCategoryName);

            context.put(BLOJSOM_PLUGIN_EDIT_BLOG_ENTRIES_CATEGORY, blogCategoryName);
            httpServletRequest.setAttribute(PAGE_PARAM, ADD_BLOG_ENTRY_PAGE);
        } else if (ADD_BLOG_ENTRY_ACTION.equals(action)) {
            _logger.debug("User requested add blog entry action");
            Blog blog = user.getBlog();

            String blogCategoryName = BlojsomUtils.getRequestValue(BLOG_CATEGORY_NAME, httpServletRequest);
            blogCategoryName = BlojsomUtils.normalize(blogCategoryName);
            if (!blogCategoryName.endsWith("/")) {
                blogCategoryName += "/";
            }
            String blogEntryDescription = BlojsomUtils.getRequestValue(BLOG_ENTRY_DESCRIPTION, httpServletRequest);
            String blogEntryTitle = BlojsomUtils.getRequestValue(BLOG_ENTRY_TITLE, httpServletRequest);
            if (BlojsomUtils.checkNullOrBlank(blogEntryTitle)) {
                blogEntryDescription = BlojsomUtils.LINE_SEPARATOR + blogEntryDescription;
            }
            String allowComments = BlojsomUtils.getRequestValue(BLOG_METADATA_COMMENTS_DISABLED, httpServletRequest);
            String allowTrackbacks = BlojsomUtils.getRequestValue(BLOG_METADATA_TRACKBACKS_DISABLED, httpServletRequest);
            String blogTrackbackURLs = BlojsomUtils.getRequestValue(BLOG_TRACKBACK_URLS, httpServletRequest);
            String blogPingURLs = BlojsomUtils.getRequestValue(BLOG_PING_URLS, httpServletRequest);
            String proposedBlogFilename = BlojsomUtils.getRequestValue(BLOG_ENTRY_PROPOSED_NAME, httpServletRequest);

            BlogCategory category;
            category = _fetcher.newBlogCategory();
            category.setCategory(blogCategoryName);
            category.setCategoryURL(user.getBlog().getBlogURL() + BlojsomUtils.removeInitialSlash(blogCategoryName));

            BlogEntry entry;
            entry = _fetcher.newBlogEntry();
            entry.setTitle(blogEntryTitle);
            entry.setCategory(blogCategoryName);
            entry.setDescription(blogEntryDescription);

            Map entryMetaData = new HashMap();
            String username = (String) httpServletRequest.getSession().getAttribute(user.getBlog().getBlogURL() + "_" + BLOJSOM_ADMIN_PLUGIN_USERNAME_KEY);
            entryMetaData.put(BlojsomMetaDataConstants.BLOG_ENTRY_METADATA_AUTHOR, username);

            String entryPublishDateTime = httpServletRequest.getParameter(BLOG_ENTRY_PUBLISH_DATETIME);
            if (!BlojsomUtils.checkNullOrBlank(entryPublishDateTime)) {
                SimpleDateFormat simpleDateFormat = new SimpleDateFormat("MM/dd/yyyy HH:mm:ss");
                try {
                    Date publishDateTime = simpleDateFormat.parse(entryPublishDateTime);
                    _logger.debug("Publishing blog entry at: " + publishDateTime.toString());
                    entryMetaData.put(BlojsomMetaDataConstants.BLOG_ENTRY_METADATA_TIMESTAMP, new Long(publishDateTime.getTime()).toString());
                } catch (ParseException e) {
                    _logger.error(e);
                    entryMetaData.put(BlojsomMetaDataConstants.BLOG_ENTRY_METADATA_TIMESTAMP, new Long(new Date().getTime()).toString());
                }
            } else {
                entryMetaData.put(BlojsomMetaDataConstants.BLOG_ENTRY_METADATA_TIMESTAMP, new Long(new Date().getTime()).toString());
            }

            if (!BlojsomUtils.checkNullOrBlank(allowComments)) {
                entryMetaData.put(BLOG_METADATA_COMMENTS_DISABLED, "y");
            }

            if (!BlojsomUtils.checkNullOrBlank(allowTrackbacks)) {
                entryMetaData.put(BLOG_METADATA_TRACKBACKS_DISABLED, "y");
            }

            entry.setMetaData(entryMetaData);

            String blogEntryExtension = user.getBlog().getBlogProperty(BLOG_XMLRPC_ENTRY_EXTENSION_IP);
            if (BlojsomUtils.checkNullOrBlank(blogEntryExtension)) {
                blogEntryExtension = DEFAULT_BLOG_XMLRPC_ENTRY_EXTENSION;
            }

            String filename;
            if (BlojsomUtils.checkNullOrBlank(proposedBlogFilename)) {
                filename = getBlogEntryFilename(blogEntryDescription, blogEntryExtension);
            } else {
                if (proposedBlogFilename.length() > MAXIMUM_FILENAME_LENGTH) {
                    proposedBlogFilename = proposedBlogFilename.substring(0, MAXIMUM_FILENAME_LENGTH);
                }
                
                proposedBlogFilename = BlojsomUtils.normalize(proposedBlogFilename);
                proposedBlogFilename += blogEntryExtension;
                filename = proposedBlogFilename;
                _logger.debug("Using proposed blog entry filename: " + filename);
            }

            File blogFilename = new File(user.getBlog().getBlogHome() + BlojsomUtils.removeInitialSlash(blogCategoryName) + filename);
            _logger.debug("New blog entry file: " + blogFilename.toString());

            Map attributeMap = new HashMap();
            attributeMap.put(BlojsomMetaDataConstants.SOURCE_ATTRIBUTE, blogFilename);
            entry.setAttributes(attributeMap);

            try {
                entry.save(user);
                entry.load(user);
                StringBuffer entryLink = new StringBuffer();
                entry.setLink(user.getBlog().getBlogURL() + BlojsomUtils.removeInitialSlash(entry.getCategory()) + "?" + PERMALINK_PARAM + "=" + entry.getPermalink());
                entryLink.append("<a href=\"").append(entry.getLink()).append("\">").append(entry.getTitle()).append("</a>");
                addOperationResultMessage(context, "Added blog entry: " + entryLink.toString());
            } catch (BlojsomException e) {
                _logger.error(e);
                addOperationResultMessage(context, "Unable to add blog entry to category: " + blogCategoryName);
            }

            // Send trackback pings
            if (!BlojsomUtils.checkNullOrBlank(blogTrackbackURLs)) {
                sendTrackbackPings(blog, entry, blogTrackbackURLs);
            }

            // Send update pings to the ping URLs
            if (!BlojsomUtils.checkNullOrBlank(blogPingURLs)) {
                sendBlogPings(blog, blogPingURLs);
            }

            httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_ENTRY_ACTION);
            context.put(BLOJSOM_PLUGIN_EDIT_BLOG_ENTRIES_ENTRY, entry);
            context.put(BLOJSOM_PLUGIN_EDIT_BLOG_ENTRIES_CATEGORY, blogCategoryName);
        } else if (DELETE_BLOG_COMMENTS.equals(action)) {
            _logger.debug("User requested delete blog comments action");

            Blog blog = user.getBlog();
            String blogCategoryName = BlojsomUtils.getRequestValue(BLOG_CATEGORY_NAME, httpServletRequest);
            blogCategoryName = BlojsomUtils.normalize(blogCategoryName);
            String blogEntryId = BlojsomUtils.getRequestValue(BLOG_ENTRY_ID, httpServletRequest);
            try {
                blogEntryId = URLDecoder.decode(blogEntryId, UTF8);
            } catch (UnsupportedEncodingException e) {
                _logger.error(e);
            }
            _logger.debug("Blog entry id: " + blogEntryId);

            String[] blogCommentIDs = httpServletRequest.getParameterValues(BLOG_COMMENT_ID);
            if (blogCommentIDs != null && blogCommentIDs.length > 0) {
                File commentsDirectory = new File(blog.getBlogHome() + blogCategoryName + blog.getBlogCommentsDirectory()
                        + File.separatorChar + blogEntryId + File.separatorChar);
                File blogCommentToDelete;
                for (int i = 0; i < blogCommentIDs.length; i++) {
                    String blogCommentID = blogCommentIDs[i];
                    blogCommentToDelete = new File(commentsDirectory, blogCommentID);
                    if (!blogCommentToDelete.delete()) {
                        _logger.error("Unable to delete blog comment: " + blogCommentToDelete.toString());
                    } else {
                        _logger.debug("Deleted blog comment: " + blogCommentToDelete.toString());
                    }
                }

                addOperationResultMessage(context, "Deleted " + blogCommentIDs.length + " comments");
            }

            httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_ENTRIES_PAGE);
        } else if (DELETE_BLOG_TRACKBACKS.equals(action)) {
            _logger.debug("User requested delete blog trackbacks action");

            Blog blog = user.getBlog();
            String blogCategoryName = BlojsomUtils.getRequestValue(BLOG_CATEGORY_NAME, httpServletRequest);
            blogCategoryName = BlojsomUtils.normalize(blogCategoryName);
            String blogEntryId = BlojsomUtils.getRequestValue(BLOG_ENTRY_ID, httpServletRequest);
            try {
                blogEntryId = URLDecoder.decode(blogEntryId, UTF8);
            } catch (UnsupportedEncodingException e) {
                _logger.error(e);
            }
            _logger.debug("Blog entry id: " + blogEntryId);

            String[] blogTrackbackIDs = httpServletRequest.getParameterValues(BLOG_TRACKBACK_ID);
            if (blogTrackbackIDs != null && blogTrackbackIDs.length > 0) {
                File trackbacksDirectory = new File(blog.getBlogHome() + blogCategoryName + blog.getBlogTrackbackDirectory()
                        + File.separatorChar + blogEntryId + File.separatorChar);
                File blogTrackbackToDelete;
                for (int i = 0; i < blogTrackbackIDs.length; i++) {
                    String blogTrackbackID = blogTrackbackIDs[i];
                    blogTrackbackToDelete = new File(trackbacksDirectory, blogTrackbackID);
                    if (!blogTrackbackToDelete.delete()) {
                        _logger.error("Unable to delete blog trackback: " + blogTrackbackToDelete.toString());
                    } else {
                        _logger.debug("Deleted blog trackback: " + blogTrackbackToDelete.toString());
                    }
                }

                addOperationResultMessage(context, "Deleted " + blogTrackbackIDs.length + " trackbacks");
            }

            httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_ENTRIES_PAGE);
        }

        return entries;
    }

    /**
     * Send trackback pings to a comma-separated list of trackback URLs
     *
     * @param blog Blog information
     * @param entry Blog entry
     * @param blogTrackbackURLs Trackback URLs
     */
    protected void sendTrackbackPings(Blog blog, BlogEntry entry, String blogTrackbackURLs) {
        // Build the URL parameters for the trackback ping URL
        StringBuffer trackbackPingURLParameters = new StringBuffer();
        try {
            trackbackPingURLParameters.append(TrackbackPlugin.TRACKBACK_URL_PARAM).append("=").append(URLEncoder.encode(entry.getLink(), UTF8));
            trackbackPingURLParameters.append("&").append(TrackbackPlugin.TRACKBACK_TITLE_PARAM).append("=").append(URLEncoder.encode(entry.getTitle(), UTF8));
            trackbackPingURLParameters.append("&").append(TrackbackPlugin.TRACKBACK_BLOG_NAME_PARAM).append("=").append(URLEncoder.encode(blog.getBlogName(), UTF8));

            String excerpt = entry.getDescription().replaceAll("<.*?>", "");
            if (excerpt.length() > 255) {
                excerpt = excerpt.substring(0, 251);
                excerpt += "...";
            }
            trackbackPingURLParameters.append("&").append(TrackbackPlugin.TRACKBACK_EXCERPT_PARAM).append("=").append(URLEncoder.encode(excerpt, UTF8));
        } catch (UnsupportedEncodingException e) {
            _logger.error(e);
        }

        String[] trackbackURLs = BlojsomUtils.parseDelimitedList(blogTrackbackURLs, WHITESPACE);
        if (trackbackURLs != null && trackbackURLs.length > 0) {
            for (int i = 0; i < trackbackURLs.length; i++) {
                String trackbackURL = trackbackURLs[i].trim();
                StringBuffer trackbackPingURL = new StringBuffer(BlojsomUtils.replace(trackbackURL, "&amp;", "&"));
                _logger.debug("Automatically sending trackback ping to URL: " + trackbackPingURL.toString());

                try {
                    URL trackbackUrl = new URL(trackbackPingURL.toString());

                    // Open a connection to the trackback URL and read its input
                    HttpURLConnection trackbackUrlConnection = (HttpURLConnection) trackbackUrl.openConnection();
                    trackbackUrlConnection.setRequestMethod("POST");
                    trackbackUrlConnection.setRequestProperty("Content-Encoding", UTF8);
                    trackbackUrlConnection.setRequestProperty("Content-Type", "application/x-www-form-urlencoded");
                    trackbackUrlConnection.setDoOutput(true);
                    trackbackUrlConnection.connect();
					OutputStream trackbackPostOutputStream = trackbackUrlConnection.getOutputStream();
					trackbackPostOutputStream.write(trackbackPingURLParameters.toString().getBytes());
					trackbackPostOutputStream.flush();
                    BufferedReader trackbackStatus = new BufferedReader(new InputStreamReader(trackbackUrlConnection.getInputStream()));
                    String line;
                    StringBuffer status = new StringBuffer();
                    while ((line = trackbackStatus.readLine()) != null) {
                        status.append(line).append("\n");
                    }
                    trackbackUrlConnection.disconnect();

                    _logger.debug("Trackback status for ping to " + trackbackURL + ": " + status.toString());
                } catch (IOException e) {
                    _logger.error(e);
                }
            }
        }
    }

    /**
     * Send "pings" to a blog service such as weblogs.com
     *
     * @param blog Blog information
     * @param blogPingURLs Ping URLs
     */
    private void sendBlogPings(Blog blog, String blogPingURLs) {
        String[] pingURLs = BlojsomUtils.parseDelimitedList(blogPingURLs, WHITESPACE);
        EditBlogEntriesPluginAsyncCallback editBlogEntriesPluginAsyncCallback = new EditBlogEntriesPluginAsyncCallback();
        Vector params = new Vector();
        params.add(blog.getBlogName());
        params.add(blog.getBlogURL());

        if (pingURLs != null && pingURLs.length > 0) {
            for (int i = 0; i < pingURLs.length; i++) {
                String pingURL = pingURLs[i].trim();

                _logger.debug("Sending ping to: " + pingURL);
                try {
                    XmlRpcClient xmlRpcClient = new XmlRpcClient(pingURL);
                    xmlRpcClient.executeAsync(WEBLOGS_PING_METHOD, params, editBlogEntriesPluginAsyncCallback);
                } catch (MalformedURLException e) {
                    _logger.error(e);
                }
            }
        }
    }

    /**
     * Return a filename appropriate for the blog entry content
     *
     * @param content            Blog entry content
     * @param blogEntryExtension Extension to be used for the blog entry filename
     * @return Filename for the new blog entry
     */
    protected String getBlogEntryFilename(String content, String blogEntryExtension) {
        String hashable = content;

        if (content.length() > MAX_HASHABLE_LENGTH) {
            hashable = hashable.substring(0, MAX_HASHABLE_LENGTH);
        }

        String baseFilename = BlojsomUtils.digestString(hashable).toUpperCase();
        String filename = baseFilename + blogEntryExtension;
        return filename;
    }

    /**
     * Asynchronous callback handler for the weblogs.com ping
     */
    private class EditBlogEntriesPluginAsyncCallback implements AsyncCallback {

        /**
         * Default constructor
         */
        public EditBlogEntriesPluginAsyncCallback() {
        }

        /**
         * Call went ok, handle result.
         *
         * @param o   Return object
         * @param url URL
         * @param s   String
         */
        public void handleResult(Object o, URL url, String s) {
            _logger.debug(o.toString());
        }

        /**
         * Something went wrong, handle error.
         *
         * @param e   Exception containing error from XML-RPC call
         * @param url URL
         * @param s   String
         */
        public void handleError(Exception e, URL url, String s) {
            _logger.error(e);
        }
    }
}
