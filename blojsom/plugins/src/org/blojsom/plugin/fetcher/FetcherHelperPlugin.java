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
package org.blojsom.plugin.fetcher;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.blog.BlogCategory;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.fetcher.BlojsomFetcher;
import org.blojsom.fetcher.BlojsomFetcherException;
import org.blojsom.plugin.BlojsomPlugin;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.util.BlojsomUtils;
import org.blojsom.util.BlojsomConstants;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.UnsupportedEncodingException;
import java.net.URLDecoder;
import java.util.HashMap;
import java.util.Map;

/**
 * Fetcher helper plugin allows you to retrieve blog entries in your templates.
 *
 * @author David Czarnecki
 * @version $Id :$
 * @since blojsom 2.24
 */
public class FetcherHelperPlugin implements BlojsomPlugin {

    private Log _logger = LogFactory.getLog(FetcherHelperPlugin.class);

    private static final String BLOJSOM_PLUGIN_FETCHER_HELPER = "BLOJSOM_PLUGIN_FETCHER_HELPER";

    protected BlojsomFetcher _fetcher;

    /**
     * Create a new instance of the fetcher helper plugin.
     */
    public FetcherHelperPlugin() {
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
     * @throws org.blojsom.plugin.BlojsomPluginException
     *          If there is an error processing the blog entries
     */
    public BlogEntry[] process(HttpServletRequest httpServletRequest, HttpServletResponse httpServletResponse, BlogUser user, Map context, BlogEntry[] entries) throws BlojsomPluginException {
        context.put(BLOJSOM_PLUGIN_FETCHER_HELPER, new FetcherHelper(_fetcher, user));

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

    /**
     * Fetcher helper is the actual class with methods for retrieving blog entries.
     *
     * @author David Czarnecki
     * @since blojsom 2.24
     */
    public class FetcherHelper {

        private BlojsomFetcher _fetcher;
        private BlogUser _blog;

        /**
         * Create a new instance of the fetcher helper.
         *
         * @param fetcher {@link BlojsomFetcher} used to retrieve entries
         * @param blog {@link BlogUser} information
         */
        public FetcherHelper(BlojsomFetcher fetcher, BlogUser blog) {
            _fetcher = fetcher;
            _blog = blog;
        }

        /**
         * Fetch entries for a given category allowing for a limit on number of entries returned.
         *
         * @param categoryName Category name
         * @param entriesLimit Limit on number of entries to return. Use -1 for all entries from category.
         * @return Entries from specified category
         */
        public BlogEntry[] fetchEntriesForCategory(String categoryName, int entriesLimit) {
            BlogEntry[] entries = new BlogEntry[0];
            categoryName = BlojsomUtils.normalize(categoryName);

            BlogCategory category;
            category = _fetcher.newBlogCategory();
            category.setCategory(categoryName);
            category.setCategoryURL(_blog.getBlog().getBlogURL() + BlojsomUtils.removeInitialSlash(categoryName));

            Map fetchMap = new HashMap();
            fetchMap.put(BlojsomFetcher.FETCHER_CATEGORY, category);
            fetchMap.put(BlojsomFetcher.FETCHER_NUM_POSTS_INTEGER, new Integer(entriesLimit));

            try {
                entries = _fetcher.fetchEntries(fetchMap, _blog);
                if (entries != null) {
                    _logger.debug("Retrieved " + entries.length + " entries from category: " + categoryName);
                } else {
                    _logger.debug("No entries found in category: " + categoryName);
                }
            } catch (BlojsomFetcherException e) {
                _logger.error(e);
            }

            return entries;
        }

        /**
         * Fetch entries for a given flavor allowing for a limit on number of entries returned.
         *
         * @param flavor Flavor name
         * @param entriesLimit Limit on number of entries to return. Use -1 for all entries from category.
         * @return Entries for specified flavor
         */
        public BlogEntry[] fetchEntriesForFlavor(String flavor, int entriesLimit) {
            BlogEntry[] entries = new BlogEntry[0];

            Map fetchMap = new HashMap();
            fetchMap.put(BlojsomFetcher.FETCHER_FLAVOR, flavor);
            fetchMap.put(BlojsomFetcher.FETCHER_NUM_POSTS_INTEGER, new Integer(entriesLimit));

            try {
                entries = _fetcher.fetchEntries(fetchMap, _blog);
                if (entries != null) {
                    _logger.debug("Retrieved " + entries.length + " entries for flavor: " + flavor);
                } else {
                    _logger.debug("No entries found for flavor: " + flavor);
                }
            } catch (BlojsomFetcherException e) {
                _logger.error(e);
            }

            return entries;
        }

        /**
         * Fetch a specific entry.
         *
         * @param categoryName Category name
         * @param permalink Permalink name of entry.
         * @return Permalink entry
         */
        public BlogEntry[] fetchPermalink(String categoryName, String permalink) {
            BlogEntry[] entries = new BlogEntry[0];

            try {
                permalink = URLDecoder.decode(permalink, BlojsomConstants.UTF8);
            } catch (UnsupportedEncodingException e) {
                _logger.error(e);
            }

            categoryName = BlojsomUtils.normalize(categoryName);

            BlogCategory category;
            category = _fetcher.newBlogCategory();
            category.setCategory(categoryName);
            category.setCategoryURL(_blog.getBlog().getBlogURL() + BlojsomUtils.removeInitialSlash(categoryName));

            Map fetchMap = new HashMap();
            fetchMap.put(BlojsomFetcher.FETCHER_CATEGORY, category);
            fetchMap.put(BlojsomFetcher.FETCHER_PERMALINK, permalink);

            try {
                entries = _fetcher.fetchEntries(fetchMap, _blog);
                if (entries != null) {
                    _logger.debug("Retrieved " + entries.length + " entries from category: " + categoryName);
                } else {
                    _logger.debug("No entries found in category: " + categoryName);
                }
            } catch (BlojsomFetcherException e) {
                _logger.error(e);
            }

            return entries;
        }
    }
}