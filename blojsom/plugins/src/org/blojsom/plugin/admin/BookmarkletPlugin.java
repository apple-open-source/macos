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

import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.Blog;
import org.blojsom.blog.BlogCategory;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.plugin.admin.event.AddBlogEntryEvent;
import org.blojsom.plugin.weblogsping.WeblogsPingPlugin;
import org.blojsom.util.BlojsomUtils;
import org.blojsom.util.BlojsomMetaDataConstants;
import org.blojsom.BlojsomException;
import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;

import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.util.Map;
import java.util.HashMap;
import java.util.Date;
import java.text.SimpleDateFormat;
import java.text.ParseException;
import java.io.File;

/**
 * Bookmarklet Plugin
 *
 * @author David Czarnecki
 * @version $Id: BookmarkletPlugin.java,v 1.1.2.1 2005/07/21 04:30:23 johnan Exp $
 * @since blojsom 2.20
 */
public class BookmarkletPlugin extends EditBlogEntriesPlugin {

    private Log _logger = LogFactory.getLog(BookmarkletPlugin.class);

    // Pages
    protected static final String BOOKMARKLET_PAGE = "/org/blojsom/plugin/admin/templates/admin-bookmarklet-entry";

    // Constants
    protected static final String BOOKMARKLET_PLUGIN_SELECTION = "BOOKMARKLET_PLUGIN_SELECTION";

    // Actions
    protected static final String BOOKMARKLET_BLOG_ENTRY_ACTION = "bookmarklet-blog-entry";

    // Form items
    protected static final String SELECTION_PARAM = "selection";

    // Permissions
    protected static final String USE_BOOKMARKLET_PERMISSION = "use_bookmarklet";

    /**
     * Default constructor
     */
    public BookmarkletPlugin() {
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
        String action = BlojsomUtils.getRequestValue(ACTION_PARAM, httpServletRequest);
        String selection = BlojsomUtils.getRequestValue(SELECTION_PARAM, httpServletRequest);
        context.put(BOOKMARKLET_PLUGIN_SELECTION, selection);

        if (BlojsomUtils.checkNullOrBlank(action)) {
            _logger.debug("User did not request action");
            httpServletRequest.setAttribute(PAGE_PARAM, BOOKMARKLET_PAGE);
        } else if (PAGE_ACTION.equals(action)) {
            _logger.debug("User requested bookmarklet blog entry page");

            httpServletRequest.setAttribute(PAGE_PARAM, BOOKMARKLET_PAGE);
        } else if (BOOKMARKLET_BLOG_ENTRY_ACTION.equals(action)) {
            if (!authenticateUser(httpServletRequest, httpServletResponse, context, user)) {
                httpServletRequest.setAttribute(PAGE_PARAM, BOOKMARKLET_PAGE);
                addOperationResultMessage(context, "Unable to authenticate user");
            } else {
                _logger.debug("User requested bookmarklet add blog entry action");

                String username = getUsernameFromSession(httpServletRequest, user.getBlog());
                if (!checkPermission(user, null, username, USE_BOOKMARKLET_PERMISSION)) {
                    httpServletRequest.setAttribute(PAGE_PARAM, ADMIN_LOGIN_PAGE);
                    addOperationResultMessage(context, "You are not allowed to use the bookmarklet");

                    return entries;
                }
                
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
                String proposedBlogFilename = BlojsomUtils.getRequestValue(BLOG_ENTRY_PROPOSED_NAME, httpServletRequest);
                String pingBlogURLS = BlojsomUtils.getRequestValue(PING_BLOG_URLS, httpServletRequest);

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
                username = (String) httpServletRequest.getSession().getAttribute(user.getBlog().getBlogURL() + "_" + BLOJSOM_ADMIN_PLUGIN_USERNAME_KEY);
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

                if (BlojsomUtils.checkNullOrBlank(pingBlogURLS)) {
                    entryMetaData.put(WeblogsPingPlugin.NO_PING_WEBLOGS_METADATA, "true");
                }

                entry.setMetaData(entryMetaData);

                String blogEntryExtension = user.getBlog().getBlogProperty(BLOG_XMLRPC_ENTRY_EXTENSION_IP);
                if (BlojsomUtils.checkNullOrBlank(blogEntryExtension)) {
                    blogEntryExtension = DEFAULT_BLOG_XMLRPC_ENTRY_EXTENSION;
                }

                String filename;
                if (BlojsomUtils.checkNullOrBlank(proposedBlogFilename)) {
                    filename = BlojsomUtils.getBlogEntryFilename(blogEntryTitle, blogEntryDescription);
                } else {
                    if (proposedBlogFilename.length() > MAXIMUM_FILENAME_LENGTH) {
                        proposedBlogFilename = proposedBlogFilename.substring(0, MAXIMUM_FILENAME_LENGTH);
                    }

                    proposedBlogFilename = BlojsomUtils.normalize(proposedBlogFilename);
                    filename = proposedBlogFilename;
                    _logger.debug("Using proposed blog entry filename: " + filename);
                }

                File blogFilename = new File(user.getBlog().getBlogHome() + BlojsomUtils.removeInitialSlash(blogCategoryName) + filename + blogEntryExtension);
                int fileTag = 1;
                while (blogFilename.exists()) {
                    blogFilename = new File(user.getBlog().getBlogHome() + BlojsomUtils.removeInitialSlash(blogCategoryName) + filename + "-" + fileTag + blogEntryExtension);
                    fileTag++;
                }
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
                    AddBlogEntryEvent addEvent = new AddBlogEntryEvent(this, new Date(), entry, user);
                    _blojsomConfiguration.getEventBroadcaster().broadcastEvent(addEvent);
                } catch (BlojsomException e) {
                    _logger.error(e);
                    addOperationResultMessage(context, "Unable to add blog entry to category: " + blogCategoryName);
                }

                // Send trackback pings
                if (!BlojsomUtils.checkNullOrBlank(blogTrackbackURLs)) {
                    sendTrackbackPings(blog, entry, blogTrackbackURLs);
                }

                httpServletRequest.setAttribute(PAGE_PARAM, BOOKMARKLET_PAGE);
                context.put(BLOJSOM_PLUGIN_EDIT_BLOG_ENTRIES_ENTRY, entry);
                context.put(BLOJSOM_PLUGIN_EDIT_BLOG_ENTRIES_CATEGORY, blogCategoryName);
            }
        }

        return entries;
    }
}