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
package org.blojsom.plugin.aggregator;

import org.blojsom.plugin.BlojsomPlugin;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.Blog;
import org.blojsom.util.BlojsomUtils;
import org.blojsom.fetcher.BlojsomFetcher;
import org.blojsom.fetcher.BlojsomFetcherException;
import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.util.*;

/**
 * Internal Aggregator Plugin
 *
 * @author David Czarnecki
 * @version $Id: InternalAggregatorPlugin.java,v 1.1 2004/08/27 01:06:36 whitmore Exp $
 * @since blojsom 2.17
 */
public class InternalAggregatorPlugin implements BlojsomPlugin {

    private Log _logger = LogFactory.getLog(InternalAggregatorPlugin.class);

    private static final int DEFAULT_MOST_RECENT_ENTRIES_SIZE = 3;

    private static final String BLOJSOM_PLUGIN_INTERNAL_AGGREGATOR_ENTRIES = "BLOJSOM_PLUGIN_INTERNAL_AGGREGATOR_ENTRIES";
    private static final String BLOJSOM_PLUGIN_INTERNAL_AGGREGATOR_BLOG_NAME = "BLOJSOM_PLUGIN_INTERNAL_AGGREGATOR_BLOG_NAME";
    private static final String BLOJSOM_PLUGIN_INTERNAL_AGGREGATOR_BLOG_URL = "BLOJSOM_PLUGIN_INTERNAL_AGGREGATOR_BLOG_URL";

    public static final String BLOJSOM_PLUGIN_INTERNAL_AGGREGATOR_OPT_OUT = "blojsom-plugin-internal-aggegator-opt-out";

    private BlojsomConfiguration _blojsomConfiguration;
    private BlojsomFetcher _fetcher;

    /**
     * Default constructor.
     */
    public InternalAggregatorPlugin() {
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
        _blojsomConfiguration = blojsomConfiguration;
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

        _logger.debug("Initialized internal aggregator plugin");
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
        Map users = _blojsomConfiguration.getBlogUsers();
        Iterator userIterator = users.keySet().iterator();
        String userID;
        Blog blogInformation;
        ArrayList aggregatedEntries = new ArrayList(50);
        BlogEntry[] entriesFromBlog = null;
        BlogEntry[] entriesToSort = null;

        while (userIterator.hasNext()) {
            userID = (String) userIterator.next();
            BlogUser blogUser = (BlogUser) users.get(userID);

            blogInformation = blogUser.getBlog();
            Boolean optOut = Boolean.valueOf(blogInformation.getBlogProperty(BLOJSOM_PLUGIN_INTERNAL_AGGREGATOR_OPT_OUT));
            if (!optOut.booleanValue()) {
                Map fetchParameters = new HashMap();
                fetchParameters.put(BlojsomFetcher.FETCHER_FLAVOR, blogInformation.getBlogDefaultFlavor());
                fetchParameters.put(BlojsomFetcher.FETCHER_NUM_POSTS_INTEGER, new Integer(blogInformation.getBlogDisplayEntries()));

                try {
                    entriesFromBlog = _fetcher.fetchEntries(fetchParameters, blogUser);
                } catch (BlojsomFetcherException e) {
                    _logger.error(e);
                }

                if (entriesFromBlog != null && entriesFromBlog.length > 0) {
                    String blogName = blogInformation.getBlogName();
                    String blogURL = blogInformation.getBlogURL();

                    int counter = (entriesFromBlog.length > DEFAULT_MOST_RECENT_ENTRIES_SIZE) ? DEFAULT_MOST_RECENT_ENTRIES_SIZE : entriesFromBlog.length;
                    for (int i = 0; i < counter; i++) {
                        BlogEntry blogEntry = entriesFromBlog[i];
                        Map blogEntryMetaData = blogEntry.getMetaData();

                        if (blogEntryMetaData == null) {
                            blogEntryMetaData = new HashMap();
                        }

                        blogEntryMetaData.put(BLOJSOM_PLUGIN_INTERNAL_AGGREGATOR_BLOG_NAME, blogName);
                        blogEntryMetaData.put(BLOJSOM_PLUGIN_INTERNAL_AGGREGATOR_BLOG_URL, blogURL);
                        blogEntry.setMetaData(blogEntryMetaData);
                        aggregatedEntries.add(blogEntry);
                    }
                }
            }
        }

        if (aggregatedEntries.size() > 0) {
            entriesToSort = (BlogEntry[]) aggregatedEntries.toArray(new BlogEntry[aggregatedEntries.size()]);
            Arrays.sort(entriesToSort, BlojsomUtils.FILE_TIME_COMPARATOR);
            context.put(BLOJSOM_PLUGIN_INTERNAL_AGGREGATOR_ENTRIES, entriesToSort);
        }
        
        return entries;
    }

    /**
     * Perform any cleanup for the plugin. Called after {@link #process}.
     *
     * @throws org.blojsom.plugin.BlojsomPluginException
     *          If there is an error performing cleanup for this plugin
     */
    public void cleanup() throws BlojsomPluginException {
    }

    /**
     * Called when BlojsomServlet is taken out of service
     *
     * @throws org.blojsom.plugin.BlojsomPluginException
     *          If there is an error in finalizing this plugin
     */
    public void destroy() throws BlojsomPluginException {
    }
}