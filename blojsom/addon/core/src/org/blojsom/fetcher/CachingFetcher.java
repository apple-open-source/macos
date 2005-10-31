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
package org.blojsom.fetcher;

import com.opensymphony.oscache.base.NeedsRefreshException;
import com.opensymphony.oscache.general.GeneralCacheAdministrator;
import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.BlojsomException;
import org.blojsom.blog.*;
import org.blojsom.event.BlojsomEvent;
import org.blojsom.event.BlojsomListener;
import org.blojsom.plugin.admin.event.BlogEntryEvent;
import org.blojsom.util.BlojsomUtils;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.util.*;

/**
 * CachingFetcher
 *
 * @author David Czarnecki
 * @version $Id: CachingFetcher.java,v 1.1.2.1 2005/07/21 04:30:21 johnan Exp $
 * @since blojsom 2.01
 */
public class CachingFetcher extends StandardFetcher implements BlojsomListener {

    private Log _logger = LogFactory.getLog(CachingFetcher.class);

    /**
     * Default refresh period for refreshing the cache (30 minutes)
     */
    private static final int DEFAULT_CACHE_REFRESH = 1800;

    /**
     * Initialization parameter for web.xml
     */
    private static final String OSCACHE_PROPERTIES_IP = "oscache-properties";

    /**
     * Parameter for blog.properties for user to control cache refresh period
     */
    private static final String CACHING_FETCHER_REFRESH = "caching-fetcher-refresh";

    /**
     * Default location for oscache.properties
     */
    private static final String OSCACHE_PROPERTIES_DEFAULT = "/WEB-INF/oscache.properties";

    protected static GeneralCacheAdministrator _cache;

    /**
     * Default constructor
     */
    public CachingFetcher() {
    }

    /**
     * Initialize this fetcher. This method only called when the fetcher is instantiated.
     *
     * @param servletConfig        Servlet config object for the plugin to retrieve any initialization parameters
     * @param blojsomConfiguration blojsom configuration information
     * @throws BlojsomFetcherException If there is an error initializing the fetcher
     */
    public void init(ServletConfig servletConfig, BlojsomConfiguration blojsomConfiguration) throws BlojsomFetcherException {
        super.init(servletConfig, blojsomConfiguration);

        String oscachePropertiesIP = servletConfig.getInitParameter(OSCACHE_PROPERTIES_IP);
        if (BlojsomUtils.checkNullOrBlank(oscachePropertiesIP)) {
            oscachePropertiesIP = OSCACHE_PROPERTIES_DEFAULT;
        }

        try {
            Properties oscacheProperties = BlojsomUtils.loadProperties(servletConfig, oscachePropertiesIP);
            if (_cache == null) {
                _cache = new GeneralCacheAdministrator(oscacheProperties);
            }
            _logger.debug("Initialized caching fetcher");
        } catch (BlojsomException e) {
            _logger.error(e);
            throw new BlojsomFetcherException(e);
        }

        blojsomConfiguration.getEventBroadcaster().addListener(this);
    }

    /**
     * Fetch a set of {@link org.blojsom.blog.BlogEntry} objects.
     *
     * @param httpServletRequest  Request
     * @param httpServletResponse Response
     * @param user                {@link BlogUser} instance
     * @param flavor              Flavor
     * @param context             Context
     * @return Blog entries retrieved for the particular request
     * @throws BlojsomFetcherException If there is an error retrieving the blog entries for the request
     */
    public BlogEntry[] fetchEntries(HttpServletRequest httpServletRequest, HttpServletResponse httpServletResponse, BlogUser user, String flavor, Map context) throws BlojsomFetcherException {
        BlogCategory category = (BlogCategory) context.get(STANDARD_FETCHER_CATEGORY);
        context.remove(STANDARD_FETCHER_CATEGORY);
        context.remove(STANDARD_FETCHER_DEPTH);
        Blog blog = user.getBlog();

        // Check to see if the requested flavor should be ignored
        if (_ignoreFlavors.indexOf(flavor) != -1) {
            return new BlogEntry[0];
        }

        // Determine if a permalink has been requested
        String permalink = httpServletRequest.getParameter(PERMALINK_PARAM);
        if (permalink != null) {
            permalink = BlojsomUtils.getFilenameForPermalink(permalink, blog.getBlogFileExtensions());
            permalink = BlojsomUtils.urlDecode(permalink);

            if (permalink == null) {
                _logger.error("Permalink request for invalid permalink: " + httpServletRequest.getParameter(PERMALINK_PARAM));
            } else {
                _logger.debug("Permalink request for: " + permalink);
            }
        }

        // Check for a permalink entry request
        if (permalink != null) {
            context.put(BLOJSOM_PERMALINK, permalink);

            BlogEntry[] permalinkEntry = getPermalinkEntry(user, category, permalink);

            if (blog.getLinearNavigationEnabled().booleanValue()) {
                BlogEntry[] allEntries;

                allEntries = getEntriesFromCache(user);

                if (permalinkEntry.length > 0 && allEntries.length > 0) {
                    String permalinkId = permalinkEntry[0].getId();
                    BlogEntry blogEntry;

                    for (int i = allEntries.length - 1; i >= 0; i--) {
                        blogEntry = allEntries[i];
                        String blogEntryId = blogEntry.getId();

                        if (blogEntryId != null && blogEntryId.equals(permalinkId)) {
                            if ((i - 1) >= 0) {
                                context.put(BLOJSOM_PERMALINK_NEXT_ENTRY, allEntries[i - 1]);
                            } else {
                                context.put(BLOJSOM_PERMALINK_NEXT_ENTRY, null);
                            }

                            if ((i + 1) < allEntries.length) {
                                context.put(BLOJSOM_PERMALINK_PREVIOUS_ENTRY, allEntries[i + 1]);
                            } else {
                                context.put(BLOJSOM_PERMALINK_PREVIOUS_ENTRY, null);
                            }

                            break;
                        }
                    }
                }
            }

            return permalinkEntry;
        } else {
            BlogEntry[] entries;

            entries = getEntriesFromCache(user);

            if (entries == null || entries.length == 0) {
                return new BlogEntry[0];
            }

            if (category.getCategory().equals("/")) {
                entries = filterEntriesForFlavor(user, flavor);

                return entries;
            } else {
                if (!category.getCategory().startsWith("/")) {
                    category.setCategory("/" + category.getCategory());
                }

                ArrayList entriesList = new ArrayList(entries.length);
                String entryCategory;
                BlogEntry entry;

                for (int i = 0; i < entries.length; i++) {
                    entry = entries[i];

                    if (!entry.getCategory().startsWith("/")) {
                        entryCategory = "/" + entry.getCategory();
                    } else {
                        entryCategory = entry.getCategory();
                    }

                    if (entryCategory.equals(category.getCategory())) {
                        entriesList.add(entry);
                    }
                }

                return (BlogEntry[]) entriesList.toArray(new BlogEntry[entriesList.size()]);
            }
        }
    }

    /**
     * Retrieve cached blog entries for a given blog
     *
     * @param blog {@link BlogUser}
     * @return Entries for the blog
     * @since blojsom 2.24
     */
    protected BlogEntry[] getEntriesFromCache(BlogUser blog) {
        BlogEntry[] entries;

        String cacheRefresh = blog.getBlog().getBlogProperty(CACHING_FETCHER_REFRESH);
        int refreshPeriod;
        if (BlojsomUtils.checkNullOrBlank(cacheRefresh)) {
            refreshPeriod = DEFAULT_CACHE_REFRESH;
        }
        try {
            refreshPeriod = Integer.parseInt(cacheRefresh);
        } catch (NumberFormatException e) {
            refreshPeriod = DEFAULT_CACHE_REFRESH;
        }

        // Get the entries for this blog from the cache
        try {
            entries = (BlogEntry[]) _cache.getFromCache(blog.getId(), refreshPeriod);
            _logger.debug("Returned entries from cache for blog: " + blog.getId());
        } catch (NeedsRefreshException e) {
            entries = (BlogEntry[]) e.getCacheContent();

            if (entries == null) {
                String[] filter = null;
                entries = getEntriesAllCategories(blog, filter, -1, blog.getBlog().getBlogDepth());
                _cache.putInCache(blog.getId(), entries);
            } else {
                _cache.cancelUpdate(blog.getId());
                Thread allCategoriesFetcherThread = new Thread(new AllCategoriesFetcherThread(blog, blog.getBlog().getBlogDepth()));
                allCategoriesFetcherThread.setDaemon(true);
                allCategoriesFetcherThread.start();

                _logger.debug("Returning from all categories fetcher thread for key: " + blog.getId());
            }
        }

        return entries;
    }

    /**
     * Filter blog entries for the flavor
     *
     * @param blog   {@link BlogUser}
     * @param flavor Flavor
     * @return <code>BlogEntry[]</code> for flavor
     * @since blojsom 2.24
     */
    protected BlogEntry[] filterEntriesForFlavor(BlogUser blog, String flavor) {
        Blog blogInformation = blog.getBlog();

        String flavorMappingKey = flavor + '.' + BLOG_DEFAULT_CATEGORY_MAPPING_IP;
        String categoryMappingForFlavor = (String) blogInformation.getBlogProperty(flavorMappingKey);
        String[] categoryMappingsForFlavor = null;

        if (!BlojsomUtils.checkNullOrBlank(categoryMappingForFlavor)) {
            _logger.debug("Using category mappings for flavor: " + flavor);
            categoryMappingsForFlavor = BlojsomUtils.parseCommaList(categoryMappingForFlavor);
        } else if (blogInformation.getBlogDefaultCategoryMappings() != null && blogInformation.getBlogDefaultCategoryMappings().length > 0) {
            _logger.debug("Using default category mapping");
            categoryMappingsForFlavor = blogInformation.getBlogDefaultCategoryMappings();
        } else {
            categoryMappingsForFlavor = null;
        }

        BlogEntry[] entries = getEntriesFromCache(blog);
        if (entries != null && entries.length > 0) {
            ArrayList entriesList = new ArrayList(entries.length);
            Map categoryMap = new HashMap();

            if (categoryMappingsForFlavor != null) {
                // Setup map to check categories
                for (int i = 0; i < categoryMappingsForFlavor.length; i++) {
                    String category = categoryMappingsForFlavor[i];

                    if (!category.startsWith("/")) {
                        category = "/" + category;
                    }

                    if (!category.endsWith("/")) {
                        category = category + "/";
                    }

                    categoryMap.put(category, category);
                }

                String entryCategory;
                for (int i = 0; i < entries.length; i++) {
                    BlogEntry entry = entries[i];
                    entryCategory = entry.getCategory();

                    if (!entryCategory.startsWith("/")) {
                        entryCategory = "/" + entryCategory;
                    }

                    if (!entryCategory.endsWith("/")) {
                        entryCategory = entryCategory + "/";
                    }

                    if (categoryMap.containsKey(entryCategory)) {
                        entriesList.add(entry);
                    }
                }

                return (BlogEntry[]) entriesList.toArray(new BlogEntry[entriesList.size()]);
            } else {
                return entries;
            }
        }

        return new BlogEntry[0];
    }

    /**
     * Handle an event broadcast from another component
     *
     * @param event {@link org.blojsom.event.BlojsomEvent} to be handled
     */
    public void handleEvent(BlojsomEvent event) {
        if (event instanceof BlogEntryEvent && !event.isEventHandled()) {
            BlogEntryEvent blogEntryEvent = (BlogEntryEvent) event;

            Thread allCategoriesFetcherThread = new Thread(new AllCategoriesFetcherThread(blogEntryEvent.getBlogUser(), blogEntryEvent.getBlogUser().getBlog().getBlogDepth()));
            allCategoriesFetcherThread.setDaemon(true);
            allCategoriesFetcherThread.start();
            
            event.setEventHandled(true);
        }
    }

    /**
     * Process an event from another component
     *
     * @param event {@link BlojsomEvent} to be handled
     * @since blojsom 2.24
     */
    public void processEvent(BlojsomEvent event) {
    }

    /**
     * AllCategoriesFetcherThread
     *
     * @since blojsom 2.05
     */
    private class AllCategoriesFetcherThread implements Runnable {

        private BlogUser _user;
        private int _blogDirectoryDepth;

        /**
         * Default constructor.
         *
         * @param user               Blog user
         * @param blogDirectoryDepth Blog directory depth
         */
        public AllCategoriesFetcherThread(BlogUser user, int blogDirectoryDepth) {
            _user = user;
            _blogDirectoryDepth = blogDirectoryDepth;
        }

        /**
         * If this thread was constructed using a separate
         * <code>Runnable</code> run object, then that
         * <code>Runnable</code> object's <code>run</code> method is called;
         * otherwise, this method does nothing and returns.
         * <p/>
         * Subclasses of <code>Thread</code> should override this method.
         *
         * @see Thread#start()
         * @see Thread#stop()
         * @see Thread#Thread(ThreadGroup,
                *      Runnable, String)
         * @see Runnable#run()
         */
        public void run() {
            synchronized (_cache) {
                String[] filter = null;
                BlogEntry[] entries = getEntriesAllCategories(_user, filter, -1, _blogDirectoryDepth);
                _cache.flushEntry(_user.getId());
                _cache.putInCache(_user.getId(), entries);
            }

            return;            
        }
    }
}
