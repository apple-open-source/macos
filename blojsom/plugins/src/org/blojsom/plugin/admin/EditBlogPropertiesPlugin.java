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
import org.blojsom.blog.Blog;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.plugin.aggregator.InternalAggregatorPlugin;
import org.blojsom.plugin.weblogsping.WeblogsPingPlugin;
import org.blojsom.plugin.trackback.TrackbackPlugin;
import org.blojsom.plugin.comment.CommentPlugin;
import org.blojsom.util.BlojsomConstants;
import org.blojsom.util.BlojsomUtils;

import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.Properties;

/**
 * EditBlogPropertiesPlugin
 *
 * @author David Czarnecki
 * @since blojsom 2.04
 * @version $Id: EditBlogPropertiesPlugin.java,v 1.2 2004/08/27 01:06:35 whitmore Exp $
 */
public class EditBlogPropertiesPlugin extends BaseAdminPlugin {

    private static Log _logger = LogFactory.getLog(EditBlogPropertiesPlugin.class);

    private static final String EDIT_BLOG_PROPERTIES_PAGE = "/org/blojsom/plugin/admin/templates/admin-edit-blog-properties";
    private static final String EDIT_BLOG_PROPERTIES_ACTION = "edit-blog-properties";
    private static final String BLOJSOM_PLUGIN_EDIT_BLOG_PROPERTIES_CATEGORY_MAP = "BLOJSOM_PLUGIN_EDIT_BLOG_PROPERTIES_CATEGORY_MAP";

    /**
     * Default constructor.
     */
    public EditBlogPropertiesPlugin() {
    }

    /**
     * Process the blog entries
     *
     * @param httpServletRequest Request
     * @param httpServletResponse Response
     * @param user {@link org.blojsom.blog.BlogUser} instance
     * @param context Context
     * @param entries Blog entries retrieved for the particular request
     * @return Modified set of blog entries
     * @throws BlojsomPluginException If there is an error processing the blog entries
     */
    public BlogEntry[] process(HttpServletRequest httpServletRequest, HttpServletResponse httpServletResponse, BlogUser user, Map context, BlogEntry[] entries) throws BlojsomPluginException {
        if (!authenticateUser(httpServletRequest, httpServletResponse, context, user)) {
            httpServletRequest.setAttribute(PAGE_PARAM, ADMIN_LOGIN_PAGE);

            return entries;
        }

        Blog blog = user.getBlog();
        Map flavorMap;
        Iterator flavorKeys;

        String action = BlojsomUtils.getRequestValue(ACTION_PARAM, httpServletRequest);
        if (BlojsomUtils.checkNullOrBlank(action)) {
            _logger.debug("User did not request edit action");
            httpServletRequest.setAttribute(PAGE_PARAM, ADMIN_ADMINISTRATION_PAGE);
        } else if (PAGE_ACTION.equals(action)) {
            _logger.debug("User requested edit page");
            httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_PROPERTIES_PAGE);
        } else if (EDIT_BLOG_PROPERTIES_ACTION.equals(action)) {
            _logger.debug("User requested edit action");

            String blogPropertyValue = BlojsomUtils.getRequestValue("blog-name", httpServletRequest);
            blog.setBlogName(blogPropertyValue);
            blogPropertyValue = BlojsomUtils.getRequestValue("blog-description", httpServletRequest);
            blog.setBlogDescription(blogPropertyValue);
            blogPropertyValue = BlojsomUtils.getRequestValue("blog-country", httpServletRequest);
            blog.setBlogCountry(blogPropertyValue);
            blogPropertyValue = BlojsomUtils.getRequestValue("blog-language", httpServletRequest);
            blog.setBlogLanguage(blogPropertyValue);
            blogPropertyValue = BlojsomUtils.getRequestValue("blog-depth", httpServletRequest);
            try {
                int blogDepth = Integer.parseInt(blogPropertyValue);
                blog.setBlogDepth(blogDepth);
            } catch (NumberFormatException e) {
                _logger.error("Blog depth parameter invalid.", e);
            }
            blogPropertyValue = BlojsomUtils.getRequestValue("blog-display-entries", httpServletRequest);
            try {
                int blogDisplayEntries = Integer.parseInt(blogPropertyValue);
                blog.setBlogDisplayEntries(blogDisplayEntries);
            } catch (NumberFormatException e) {
                _logger.error("Blog display entries parameter invalid.", e);
            }
            blogPropertyValue = BlojsomUtils.getRequestValue("blog-owner", httpServletRequest);
            blog.setBlogOwner(blogPropertyValue);
            blogPropertyValue = BlojsomUtils.getRequestValue("blog-owner-email", httpServletRequest);
            blog.setBlogOwnerEmail(blogPropertyValue);
            blogPropertyValue = BlojsomUtils.getRequestValue("blog-comments-enabled", httpServletRequest);
            blog.setBlogCommentsEnabled(Boolean.valueOf(blogPropertyValue));
            blogPropertyValue = BlojsomUtils.getRequestValue("blog-trackbacks-enabled", httpServletRequest);
            blog.setBlogTrackbacksEnabled(Boolean.valueOf(blogPropertyValue));
            blogPropertyValue = BlojsomUtils.getRequestValue("blog-email-enabled", httpServletRequest);
            blog.setBlogEmailEnabled(Boolean.valueOf(blogPropertyValue));
            blogPropertyValue = BlojsomUtils.getRequestValue("blog-file-encoding", httpServletRequest);
            blog.setBlogFileEncoding(blogPropertyValue);
            blogPropertyValue = BlojsomUtils.getRequestValue("blog-file-extensions", httpServletRequest);
            blog.setBlogFileExtensions(blogPropertyValue);
            blogPropertyValue = BlojsomUtils.getRequestValue("blog-default-flavor", httpServletRequest);
            blog.setBlogDefaultFlavor(blogPropertyValue);
            blogPropertyValue = BlojsomUtils.getRequestValue("linear-navigation-enabled", httpServletRequest);
            blog.setLinearNavigationEnabled(Boolean.valueOf(blogPropertyValue));

            // Aggregator properties
            blogPropertyValue = BlojsomUtils.getRequestValue(InternalAggregatorPlugin.BLOJSOM_PLUGIN_INTERNAL_AGGREGATOR_OPT_OUT, httpServletRequest);
            blog.setBlogProperty(InternalAggregatorPlugin.BLOJSOM_PLUGIN_INTERNAL_AGGREGATOR_OPT_OUT, blogPropertyValue);

            // Comment plugin properties
            blogPropertyValue = BlojsomUtils.getRequestValue(CommentPlugin.COMMENT_AUTOFORMAT_IP, httpServletRequest);
            blog.setBlogProperty(CommentPlugin.COMMENT_AUTOFORMAT_IP, blogPropertyValue);
            blogPropertyValue = BlojsomUtils.getRequestValue(CommentPlugin.COMMENT_PREFIX_IP, httpServletRequest);
            blog.setBlogProperty(CommentPlugin.COMMENT_PREFIX_IP, blogPropertyValue);
            blogPropertyValue = BlojsomUtils.getRequestValue(CommentPlugin.COMMENT_COOKIE_EXPIRATION_DURATION_IP, httpServletRequest);
            blog.setBlogProperty(CommentPlugin.COMMENT_COOKIE_EXPIRATION_DURATION_IP, blogPropertyValue);
            blogPropertyValue = BlojsomUtils.getRequestValue(CommentPlugin.COMMENT_THROTTLE_MINUTES_IP, httpServletRequest);
            blog.setBlogProperty(CommentPlugin.COMMENT_THROTTLE_MINUTES_IP, blogPropertyValue);
            blogPropertyValue = BlojsomUtils.getRequestValue(CommentPlugin.COMMENT_DAYS_EXPIRATION_IP, httpServletRequest);
            blog.setBlogProperty(CommentPlugin.COMMENT_DAYS_EXPIRATION_IP, blogPropertyValue);

            // Trackback plugin properties
            blogPropertyValue = BlojsomUtils.getRequestValue(TrackbackPlugin.TRACKBACK_THROTTLE_MINUTES_IP, httpServletRequest);
            blog.setBlogProperty(TrackbackPlugin.TRACKBACK_THROTTLE_MINUTES_IP, blogPropertyValue);
            blogPropertyValue = BlojsomUtils.getRequestValue(TrackbackPlugin.TRACKBACK_PREFIX_IP, httpServletRequest);
            blog.setBlogProperty(TrackbackPlugin.TRACKBACK_PREFIX_IP, blogPropertyValue);
            blogPropertyValue = BlojsomUtils.getRequestValue(TrackbackPlugin.TRACKBACK_DAYS_EXPIRATION_IP, httpServletRequest);
            blog.setBlogProperty(TrackbackPlugin.TRACKBACK_DAYS_EXPIRATION_IP, blogPropertyValue);

            // Weblogs Ping plugin properties
            blogPropertyValue = BlojsomUtils.getRequestValue(WeblogsPingPlugin.BLOG_PING_URLS_IP, httpServletRequest);
            String[] pingURLs = BlojsomUtils.parseDelimitedList(blogPropertyValue, BlojsomUtils.WHITESPACE);
            if (pingURLs != null && pingURLs.length > 0) {
                blog.setBlogProperty(WeblogsPingPlugin.BLOG_PING_URLS_IP, BlojsomUtils.arrayOfStringsToString(pingURLs, " "));
            } else {
                blog.setBlogProperty(WeblogsPingPlugin.BLOG_PING_URLS_IP, "");
            }

            // Set the blog default category mappings
            flavorMap = user.getFlavors();
            flavorKeys = flavorMap.keySet().iterator();
            String key;
            String flavorCategoryMapping;
            while (flavorKeys.hasNext()) {
                key = (String) flavorKeys.next();
                flavorCategoryMapping = BlojsomUtils.getRequestValue(key + "." + BLOG_DEFAULT_CATEGORY_MAPPING_IP, httpServletRequest);
                if (flavorCategoryMapping != null) {
                    flavorCategoryMapping = BlojsomUtils.normalize(flavorCategoryMapping);
                    blog.setBlogDefaultCategoryMappingForFlavor(key + "." + BLOG_DEFAULT_CATEGORY_MAPPING_IP, flavorCategoryMapping);
                }
            }

            String categoryMapping = BlojsomUtils.getRequestValue(BLOG_DEFAULT_CATEGORY_MAPPING_IP, httpServletRequest);
            categoryMapping = BlojsomUtils.normalize(categoryMapping);
            blog.setBlogDefaultCategoryMappings(BlojsomUtils.parseCommaList(categoryMapping));

            user.setBlog(blog);

            // Write out new blog properties
            Properties blogProperties = BlojsomUtils.mapToProperties(blog.getBlogProperties(), UTF8);
            File propertiesFile = new File(_blojsomConfiguration.getInstallationDirectory()
                    + BlojsomUtils.removeInitialSlash(_blojsomConfiguration.getBaseConfigurationDirectory()) +
                    "/" + user.getId() + "/" + BlojsomConstants.BLOG_DEFAULT_PROPERTIES);

            _logger.debug("Writing blog properties to: " + propertiesFile.toString());

            try {
                FileOutputStream fos = new FileOutputStream(propertiesFile);
                blogProperties.store(fos, null);
                fos.close();
                addOperationResultMessage(context, "Updated blog properties");
            } catch (IOException e) {
                _logger.error(e);
                addOperationResultMessage(context, "Unable to save blog properties");
            }

            // Request that we go back to the edit blog properties page
            httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_PROPERTIES_PAGE);
        }

        // Populate the context with the flavor/category mapping
        flavorMap = user.getFlavors();
        flavorKeys = flavorMap.keySet().iterator();
        Map categoryMapping = new HashMap();
        while (flavorKeys.hasNext()) {
            String key = (String) flavorKeys.next();
            if (blog.getBlogProperty(key + "." + BLOG_DEFAULT_CATEGORY_MAPPING_IP) != null) {
                categoryMapping.put(key, blog.getBlogProperty(key + "." + BLOG_DEFAULT_CATEGORY_MAPPING_IP));
            } else {
                categoryMapping.put(key, "");
            }
        }

        context.put(BLOJSOM_PLUGIN_EDIT_BLOG_PROPERTIES_CATEGORY_MAP, categoryMapping);

        return entries;
    }
}
