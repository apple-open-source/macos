/**
 * Copyright (c) 2003-2005, David A. Czarnecki
 * All rights reserved.
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
package org.blojsom.plugin.delicious;

import del.icio.us.Delicious;
import del.icio.us.DeliciousUtils;
import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.BlojsomException;
import org.blojsom.blog.Blog;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.fetcher.BlojsomFetcher;
import org.blojsom.fetcher.BlojsomFetcherException;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.plugin.admin.event.AddBlogEntryEvent;
import org.blojsom.plugin.velocity.StandaloneVelocityPlugin;
import org.blojsom.util.BlojsomMetaDataConstants;
import org.blojsom.util.BlojsomUtils;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.File;
import java.text.MessageFormat;
import java.util.*;

/**
 * Plugin to post links from your <a href="http://del.icio.us">del.icio.us</a> account to your blog.
 *
 * @author David Czarnecki
 * @version $Id: DailyPostingPlugin.java,v 1.1.2.1 2005/07/21 04:30:21 johnan Exp $
 * @since blojsom 2.25
 */
public class DailyPostingPlugin extends StandaloneVelocityPlugin {

    private Log _logger = LogFactory.getLog(DailyPostingPlugin.class);

    // Plugin configuration parameters
    private static final String DAILY_POSTING_POLL_TIME_IP = "daily-posting-poll-time";
    private static final int DAILY_POSTING_POLL_TIME_DEFAULT = (1000 * 60 * 60);
    private int _pollTime = DAILY_POSTING_POLL_TIME_DEFAULT;

    // Template
    private static final String DAILY_POSTING_TEMPLATE = "org/blojsom/plugin/delicious/daily-posting-template.vm";

    // Context variables
    private static final String DAILY_POSTING_USERNAME = "DAILY_POSTING_USERNAME";
    private static final String DAILY_POSTING_POSTS = "DAILY_POSTING_POSTS";

    // Individual configuration parameters
    private static final String DAILY_POSTING_USERNAME_IP = "daily-posting-username";
    private static final String DAILY_POSTING_PASSWORD_IP = "daily-posting-password";
    private static final String DAILY_POSTING_CATEGORY_IP = "daily-posting-category";
    private static final String DAILY_POSTING_HOUR_IP = "daily-posting-hour";
    private static final String DAILY_POSTING_TITLE_IP = "daily-posting-title";
    private static final String DAILY_POSTING_TITLE_DEFAULT = "del.icio.us links for {0}";

    private BlojsomFetcher _fetcher;
    private boolean _finished = false;
    private DeliciousChecker _checker;
    private BlojsomConfiguration _blojsomConfiguration;

    private String _proxyHost = null;
    private String _proxyPort = null;

    /**
     * Create a new instance of the daily posting plugin
     */
    public DailyPostingPlugin() {
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
        super.init(servletConfig, blojsomConfiguration);

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

        String pollTime = servletConfig.getInitParameter(DAILY_POSTING_POLL_TIME_IP);
        if (BlojsomUtils.checkNullOrBlank(pollTime)) {
            _pollTime = DAILY_POSTING_POLL_TIME_DEFAULT;
        } else {
            try {
                _pollTime = Integer.parseInt(pollTime);
                if (_pollTime < DAILY_POSTING_POLL_TIME_DEFAULT) {
                    _pollTime = DAILY_POSTING_POLL_TIME_DEFAULT;
                    _logger.debug("Minimum poll time allowed at 1 hour. Setting to 1 hour");
                }
            } catch (NumberFormatException e) {
                _logger.error(e);
            }
        }

        try {
            _proxyHost = System.getProperty("http.proxyHost");
            _proxyPort = System.getProperty("http.proxyPort");
        } catch (Exception e) {
            _logger.error(e);
        }

        _checker = new DeliciousChecker();
        _checker.setDaemon(true);
        _checker.start();

        _logger.debug("Initialized del.icio.us daily posting plugin");
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
        _finished = true;
    }

    /**
     * Checker thread for posting to del.icio.us
     */
    private class DeliciousChecker extends Thread {

        /**
         * Allocates a new <code>Thread</code> object. This constructor has
         * the same effect as <code>Thread(null, null,</code>
         * <i>gname</i><code>)</code>, where <b><i>gname</i></b> is
         * a newly generated name. Automatically generated names are of the
         * form <code>"Thread-"+</code><i>n</i>, where <i>n</i> is an integer.
         *
         * @see Thread#Thread(ThreadGroup,
                *      Runnable, String)
         */
        public DeliciousChecker() {
            super();
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
            try {
                while (!_finished) {
                    String[] users = _blojsomConfiguration.getBlojsomUsers();
                    BlogUser blogUser = null;
                    Blog blog = null;
                    String user = null;

                    for (int i = 0; i < users.length; i++) {
                        user = users[i];
                        try {
                            blogUser = (BlogUser) _blojsomConfiguration.loadBlog(user);
                            blog = blogUser.getBlog();

                            String postingCategory = blog.getBlogProperty(DAILY_POSTING_CATEGORY_IP);
                            String deliciousUsername = blog.getBlogProperty(DAILY_POSTING_USERNAME_IP);
                            String deliciousPassword = blog.getBlogProperty(DAILY_POSTING_PASSWORD_IP);
                            String postingHour = blog.getBlogProperty(DAILY_POSTING_HOUR_IP);
                            String postTitle = blog.getBlogProperty(DAILY_POSTING_TITLE_IP);
                            if (BlojsomUtils.checkNullOrBlank(postTitle)) {
                                postTitle = DAILY_POSTING_TITLE_DEFAULT;
                            }

                            if (BlojsomUtils.checkNullOrBlank(postingCategory) ||
                                    BlojsomUtils.checkNullOrBlank(deliciousPassword) ||
                                    BlojsomUtils.checkNullOrBlank(deliciousUsername) ||
                                    BlojsomUtils.checkNullOrBlank(postingHour)) {
                                _logger.debug("Incomplete configuration information. Skipping user: " + blogUser.getId());
                            } else {
                                Date now = new Date();
                                Calendar calendar = Calendar.getInstance();
                                calendar.setTime(now);
                                int currentHour = calendar.get(Calendar.HOUR_OF_DAY);

                                try {
                                    int hourToPost = Integer.parseInt(postingHour);
                                    if (hourToPost == currentHour) {
                                        Delicious delicious = new Delicious(deliciousUsername, deliciousPassword);
                                        if (_proxyHost != null && _proxyPort != null) {
                                            delicious.setProxyConfiguration(_proxyHost, Integer.parseInt(_proxyPort));
                                        }

                                        List posts = delicious.getPostsForDate(null, now);
                                        if (posts.size() > 0) {
                                            HashMap deliciousContext = new HashMap();
                                            deliciousContext.put(DAILY_POSTING_USERNAME, deliciousUsername);
                                            deliciousContext.put(DAILY_POSTING_POSTS, posts);

                                            String renderedLinkTemplate = mergeTemplate(DAILY_POSTING_TEMPLATE, blogUser, deliciousContext);

                                            // Create the blog entry
                                            String nowAsString = DeliciousUtils.getDeliciousDate(now);
                                            postingCategory = BlojsomUtils.normalize(postingCategory);

                                            File sourceFile = new File(blog.getBlogHome() + postingCategory + File.separator + "delicious-links-" + nowAsString + DEFAULT_ENTRY_EXTENSION);
                                            BlogEntry blogEntry;
                                            blogEntry = _fetcher.newBlogEntry();

                                            Map attributeMap = new HashMap();
                                            Map blogEntryMetaData = new HashMap();

                                            attributeMap.put(BlojsomMetaDataConstants.SOURCE_ATTRIBUTE, sourceFile);
                                            blogEntry.setAttributes(attributeMap);

                                            String title = MessageFormat.format(postTitle, new Object[]{nowAsString, deliciousUsername});
                                            blogEntry.setTitle(title);
                                            blogEntry.setCategory(postingCategory);
                                            blogEntry.setDescription(renderedLinkTemplate);
                                            blogEntryMetaData.put(BlojsomMetaDataConstants.BLOG_ENTRY_METADATA_TIMESTAMP, new Long(new Date().getTime()).toString());
                                            blogEntry.setMetaData(blogEntryMetaData);
                                            blogEntry.save(blogUser);
                                            blogEntry.load(blogUser);

                                            _blojsomConfiguration.getEventBroadcaster().broadcastEvent(new AddBlogEntryEvent(this, new Date(), blogEntry, blogUser));
                                            _logger.debug("Posted del.icio.us links for: " + blogUser.getId() + " using: " + deliciousUsername);
                                        }
                                    }
                                } catch (NumberFormatException e) {
                                    _logger.error(e);
                                }
                            }
                        } catch (BlojsomException e) {
                            _logger.error(e);
                        }
                    }

                    _logger.debug("Daily posting plugin off to take a nap");
                    sleep(_pollTime);
                }
            } catch (InterruptedException e) {
                _logger.error(e);
            }
        }
    }
}