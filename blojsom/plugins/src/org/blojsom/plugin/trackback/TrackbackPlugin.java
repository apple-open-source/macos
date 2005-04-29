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
package org.blojsom.plugin.trackback;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.blog.*;
import org.blojsom.fetcher.BlojsomFetcher;
import org.blojsom.fetcher.BlojsomFetcherException;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.plugin.common.VelocityPlugin;
import org.blojsom.plugin.email.EmailUtils;
import org.blojsom.util.BlojsomMetaDataConstants;
import org.blojsom.util.BlojsomUtils;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.*;
import java.util.*;

/**
 * TrackbackPlugin
 *
 * @author David Czarnecki
 * @version $Id: TrackbackPlugin.java,v 1.4 2005/01/15 20:39:08 johnan Exp $
 */
public class TrackbackPlugin extends VelocityPlugin implements BlojsomMetaDataConstants {

    private Log _logger = LogFactory.getLog(TrackbackPlugin.class);

    /**
     * Template for comment e-mails
     */
    private static final String TRACKBACK_PLUGIN_EMAIL_TEMPLATE = "org/blojsom/plugin/trackback/trackback-plugin-email-template.vm";

    /**
     * Default prefix for trackback e-mail notification
     */
    private static final String DEFAULT_TRACKBACK_PREFIX = "[blojsom] Trackback on: ";

    /**
     * Initialization parameter for e-mail prefix
     */
    public static final String TRACKBACK_PREFIX_IP = "plugin-trackback-email-prefix";

    /**
     * Initialization parameter for the throttling of trackbacks from IP addresses
     */
    public static final String TRACKBACK_THROTTLE_MINUTES_IP = "plugin-trackback-throttle";

    /**
     * Initialization parameter for disabling trackbacks on entries after a certain number of days
     */
    public static final String TRACKBACK_DAYS_EXPIRATION_IP = "plugin-trackback-days-expiration";

    /**
     * Default throttle value for trackbacks from a particular IP address
     */
    private static final int TRACKBACK_THROTTLE_DEFAULT_MINUTES = 5;

    /**
     * Request parameter to indicate a trackback "tb"
     */
    private static final String TRACKBACK_PARAM = "tb";

    /**
     * Request parameter for the trackback "title"
     */
    public static final String TRACKBACK_TITLE_PARAM = "title";

    /**
     * Request parameter for the trackback "excerpt"
     */
    public static final String TRACKBACK_EXCERPT_PARAM = "excerpt";

    /**
     * Request parameter for the trackback "url"
     */
    public static final String TRACKBACK_URL_PARAM = "url";

    /**
     * Request parameter for the trackback "blog_name"
     */
    public static final String TRACKBACK_BLOG_NAME_PARAM = "blog_name";

    /**
     * Key under which the indicator this plugin is "live" will be placed
     * (example: on the request for the JSPDispatcher)
     */
    public static final String BLOJSOM_TRACKBACK_PLUGIN_ENABLED = "BLOJSOM_TRACKBACK_PLUGIN_ENABLED";

    /**
     * Key under which the trackback return code will be placed
     * (example: on the request for the JSPDispatcher)
     */
    public static final String BLOJSOM_TRACKBACK_RETURN_CODE = "BLOJSOM_TRACKBACK_RETURN_CODE";

    /**
     * Key under which the trackback error message will be placed
     * (example: on the request for the JSPDispatcher)
     */
    public static final String BLOJSOM_TRACKBACK_MESSAGE = "BLOJSOM_TRACKBACK_MESSAGE";

    /**
     * IP address meta-data
     */
    public static final String BLOJSOM_TRACKBACK_PLUGIN_METADATA_IP = "BLOJSOM_TRACKBACK_PLUGIN_METADATA_IP";

    /**
     * Trackback success page
     */
    private static final String TRACKBACK_SUCCESS_PAGE = "/trackback-success";

    /**
     * Trackback failure page
     */
    private static final String TRACKBACK_FAILURE_PAGE = "/trackback-failure";

    /**
     * Key under which the blog entry will be placed for merging the trackback e-mail
     */
    public static final String BLOJSOM_TRACKBACK_PLUGIN_BLOG_ENTRY = "BLOJSOM_TRACKBACK_PLUGIN_BLOG_ENTRY";

    /**
     * Key under which the blog comment will be placed for merging the trackback e-mail
     */
    public static final String BLOJSOM_TRACKBACK_PLUGIN_TRACKBACK = "BLOJSOM_TRACKBACK_PLUGIN_TRACKBACK";

    private Map _ipAddressTrackbackTimes;
    private BlojsomFetcher _fetcher;

    /**
     * Default constructor
     */
    public TrackbackPlugin() {
    }

    /**
     * Initialize this plugin. This method only called when the plugin is instantiated.
     *
     * @param servletConfig        Servlet config object for the plugin to retrieve any initialization parameters
     * @param blojsomConfiguration {@link org.blojsom.blog.BlojsomConfiguration} information
     * @throws BlojsomPluginException If there is an error initializing the plugin
     */
    public void init(ServletConfig servletConfig, BlojsomConfiguration blojsomConfiguration) throws BlojsomPluginException {
        super.init(servletConfig, blojsomConfiguration);

        _ipAddressTrackbackTimes = new HashMap(10);
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
     * @param user                {@link BlogUser} instance
     * @param context             Context
     * @param entries             Blog entries retrieved for the particular request
     * @return Modified set of blog entries
     * @throws BlojsomPluginException If there is an error processing the blog entries
     */
    public BlogEntry[] process(HttpServletRequest httpServletRequest,
                               HttpServletResponse httpServletResponse,
                               BlogUser user,
                               Map context,
                               BlogEntry[] entries) throws BlojsomPluginException {
        Blog blog = user.getBlog();
        context.put(BLOJSOM_TRACKBACK_PLUGIN_ENABLED, blog.getBlogTrackbacksEnabled());
        if (!blog.getBlogTrackbacksEnabled().booleanValue()) {
            _logger.debug("blog trackbacks not enabled for user: " + user.getId());
            return entries;
        }

        String[] _blogFileExtensions;
        String _blogHome;
        String _blogTrackbackDirectory;
        Boolean _blogEmailEnabled;
        Boolean _blogTrackbacksEnabled;
        String _blogFileEncoding;
        String _emailPrefix;

        _blogFileExtensions = blog.getBlogFileExtensions();
        _blogHome = blog.getBlogHome();
        _blogTrackbackDirectory = blog.getBlogTrackbackDirectory();
        _blogEmailEnabled = blog.getBlogEmailEnabled();
        _blogTrackbacksEnabled = blog.getBlogTrackbacksEnabled();
        _blogFileEncoding = blog.getBlogFileEncoding();
        _emailPrefix = blog.getBlogProperty(TRACKBACK_PREFIX_IP);
        if (_emailPrefix == null) {
            _emailPrefix = DEFAULT_TRACKBACK_PREFIX;
        }

        if (entries.length == 0) {
            return entries;
        }

        if (!_blogTrackbacksEnabled.booleanValue()) {
            return entries;
        }

        String bannedIPListParam = blog.getBlogProperty(BANNED_IP_ADDRESSES_IP);
        String[] bannedIPList;
        if (bannedIPListParam == null) {
            bannedIPList = null;
            _logger.info("Blog configuration parameter not supplied for: " + BANNED_IP_ADDRESSES_IP);
        } else {
            bannedIPList = BlojsomUtils.parseCommaList(bannedIPListParam);
        }

        // Check for a trackback from a banned IP address
        String remoteIPAddress = httpServletRequest.getRemoteAddr();
        if (isIPBanned(bannedIPList, remoteIPAddress)) {
            _logger.debug("Attempted trackback from banned IP address: " + remoteIPAddress);
            return entries;
        }

        String category = httpServletRequest.getPathInfo();
        if (category == null) {
            category = "/";
        } else if (!category.endsWith("/")) {
            category += "/";
        }

        if (category.startsWith("/" + user.getId() + "/")) {
            category = BlojsomUtils.getCategoryFromPath(category);
        }

        category = BlojsomUtils.urlDecode(category);

        String url = httpServletRequest.getParameter(TRACKBACK_URL_PARAM);
        String permalink = httpServletRequest.getParameter(PERMALINK_PARAM);
        String title = httpServletRequest.getParameter(TRACKBACK_TITLE_PARAM);
        String excerpt = httpServletRequest.getParameter(TRACKBACK_EXCERPT_PARAM);
        String blogName = httpServletRequest.getParameter(TRACKBACK_BLOG_NAME_PARAM);
        String tb = httpServletRequest.getParameter(TRACKBACK_PARAM);

        if ((permalink != null) && (!"".equals(permalink)) && (tb != null) && ("y".equalsIgnoreCase(tb))) {
            if ((url == null) || ("".equals(url.trim()))) {
                context.put(BLOJSOM_TRACKBACK_RETURN_CODE, new Integer(1));
                context.put(BLOJSOM_TRACKBACK_MESSAGE, "No url parameter for trackback. url must be specified.");
                httpServletRequest.setAttribute(PAGE_PARAM, TRACKBACK_FAILURE_PAGE);

                return entries;
            }

            // Check for trackback throttling
            String trackbackThrottleValue = blog.getBlogProperty(TRACKBACK_THROTTLE_MINUTES_IP);
            if (!BlojsomUtils.checkNullOrBlank(trackbackThrottleValue)) {
                int trackbackThrottleMinutes;

                try {
                    trackbackThrottleMinutes = Integer.parseInt(trackbackThrottleValue);
                } catch (NumberFormatException e) {
                    trackbackThrottleMinutes = TRACKBACK_THROTTLE_DEFAULT_MINUTES;
                }
                _logger.debug("Trackback throttling enabled at: " + trackbackThrottleMinutes + " minutes");

                remoteIPAddress = httpServletRequest.getRemoteAddr();
                if (_ipAddressTrackbackTimes.containsKey(remoteIPAddress)) {
                    Calendar currentTime = Calendar.getInstance();
                    Calendar timeOfLastTrackback = (Calendar) _ipAddressTrackbackTimes.get(remoteIPAddress);
                    long timeDifference = currentTime.getTimeInMillis() - timeOfLastTrackback.getTimeInMillis();

                    long differenceInMinutes = timeDifference / (60 * 1000);
                    if (differenceInMinutes < trackbackThrottleMinutes) {
                        _logger.debug("Trackback throttle enabled. Comment from IP address: " + remoteIPAddress + " in less than " + trackbackThrottleMinutes + " minutes");

                        context.put(BLOJSOM_TRACKBACK_RETURN_CODE, new Integer(1));
                        context.put(BLOJSOM_TRACKBACK_MESSAGE, "Trackback throttling enabled.");
                        httpServletRequest.setAttribute(PAGE_PARAM, TRACKBACK_FAILURE_PAGE);

                        return entries;
                    } else {
                        _logger.debug("Trackback throttle enabled. Resetting date of last comment to current time");
                        _ipAddressTrackbackTimes.put(remoteIPAddress, currentTime);
                    }
                } else {
                    Calendar calendar = Calendar.getInstance();
                    _ipAddressTrackbackTimes.put(remoteIPAddress, calendar);
                }
            }

            url = url.trim();
            if (!url.toLowerCase().startsWith("http://")) {
                url = "http://" + url;
            }

            if (BlojsomUtils.checkNullOrBlank(title)) {
                title = url;
            } else {
                title = title.trim();
            }

            if (excerpt == null) {
                excerpt = "";
            } else {
                if (excerpt.length() >= 255) {
                    excerpt = excerpt.substring(0, 252);
                    excerpt += "...";
                }

                excerpt = BlojsomUtils.stripLineTerminators(excerpt);
            }

            if (blogName == null) {
                blogName = "";
            } else {
                blogName = blogName.trim();
                blogName = BlojsomUtils.stripLineTerminators(blogName);
            }

            if (!category.endsWith("/")) {
                category += "/";
            }

            // Check to see if comments have been disabled for this blog entry
            BlogCategory blogCategory = _fetcher.newBlogCategory();
            blogCategory.setCategory(category);
            blogCategory.setCategoryURL(user.getBlog().getBlogURL() + BlojsomUtils.removeInitialSlash(category));

            Map fetchMap = new HashMap();
            fetchMap.put(BlojsomFetcher.FETCHER_CATEGORY, blogCategory);
            fetchMap.put(BlojsomFetcher.FETCHER_PERMALINK, permalink);

            try {
                BlogEntry[] fetchedEntries = _fetcher.fetchEntries(fetchMap, user);
                if (fetchedEntries.length > 0) {
                    BlogEntry entry = fetchedEntries[0];
                    if (BlojsomUtils.checkMapForKey(entry.getMetaData(), BLOG_METADATA_TRACKBACKS_DISABLED)) {
                        _logger.debug("Trackbacks have been disabled for blog entry: " + entry.getId());

                        context.put(BLOJSOM_TRACKBACK_MESSAGE, "Trackbacks have been disabled for this blog entry");
                        context.put(BLOJSOM_TRACKBACK_RETURN_CODE, new Integer(1));
                        httpServletRequest.setAttribute(PAGE_PARAM, TRACKBACK_FAILURE_PAGE);

                        return entries;
                    }

                    // Check for a trackback where the number of days between trackback auto-expiration has passed
                    String trackbackDaysExpiration = blog.getBlogProperty(TRACKBACK_DAYS_EXPIRATION_IP);
                    if (!BlojsomUtils.checkNullOrBlank(trackbackDaysExpiration)) {
                        try {
                            int daysExpiration = Integer.parseInt(trackbackDaysExpiration);
                            int daysBetweenDates = BlojsomUtils.daysBetweenDates(entry.getDate(), new Date());
                            if ((daysExpiration > 0) && (daysBetweenDates >= daysExpiration)) {
                                _logger.debug("Trackback period for this entry has expired. Expiration period set at " + daysExpiration + " days. Difference in days: " + daysBetweenDates);

                                return entries;
                            }
                        } catch (NumberFormatException e) {
                            _logger.error("Error in parameter " + TRACKBACK_DAYS_EXPIRATION_IP + ": " + trackbackDaysExpiration);
                        }
                    }
                }
            } catch (BlojsomFetcherException e) {
                _logger.error(e);
            }

            Map trackbackMetaData = new HashMap();
            trackbackMetaData.put(BLOJSOM_TRACKBACK_PLUGIN_METADATA_IP, remoteIPAddress);

            Trackback trackback = new Trackback();
            Integer code = addTrackback(context, category, permalink, title, excerpt, url, blogName,
                    _blogFileExtensions, _blogHome, _blogTrackbackDirectory,
                    _blogFileEncoding, trackbackMetaData, trackback);

            // For persisting the Last-Modified time
            context.put(BLOJSOM_LAST_MODIFIED, new Long(new Date().getTime()));

            // Merge the template e-mail
            Map emailTemplateContext = new HashMap();
            emailTemplateContext.put(BLOJSOM_BLOG, blog);
            emailTemplateContext.put(BLOJSOM_USER, user);
            emailTemplateContext.put(BLOJSOM_TRACKBACK_PLUGIN_BLOG_ENTRY, entries[0]);
            emailTemplateContext.put(BLOJSOM_TRACKBACK_PLUGIN_TRACKBACK, trackback);

            String emailTrackback = mergeTemplate(TRACKBACK_PLUGIN_EMAIL_TEMPLATE, user, emailTemplateContext);

            if (_blogEmailEnabled.booleanValue()) {
                sendTrackbackEmail(_emailPrefix, entries[0].getTitle(), emailTrackback, context);
            }

            context.put(BLOJSOM_TRACKBACK_RETURN_CODE, code);
            if (code.intValue() == 0) {
                httpServletRequest.setAttribute(PAGE_PARAM, TRACKBACK_SUCCESS_PAGE);

                // Add the trackback to the list of trackbacks for the entry
                List trackbacks = entries[0].getTrackbacks();
                if (trackbacks == null) {
                    trackbacks = new ArrayList();
                }
                trackbacks.add(trackback);
                entries[0].setTrackbacks(trackbacks);
            } else {
                httpServletRequest.setAttribute(PAGE_PARAM, TRACKBACK_FAILURE_PAGE);
            }
        }

        return entries;
    }

    /**
     * Add a trackback to the permalink entry
     *
     * @param category  Category where the permalink exists
     * @param permalink Permalink
     * @param title     Trackback title
     * @param excerpt   Excerpt for the trackback (not more than 255 characters in length)
     * @param url       URL for the trackback
     * @param blogName  Name of the blog making the trackback
     */
    private Integer addTrackback(Map context, String category, String permalink, String title,
                                 String excerpt, String url, String blogName,
                                 String[] blogFileExtensions, String blogHome,
                                 String blogTrackbackDirectory, String blogFileEncoding, Map trackbackMetaData, Trackback trackback) {
        excerpt = BlojsomUtils.escapeMetaAndLink(excerpt);
        trackback.setTitle(title);
        trackback.setExcerpt(excerpt);
        trackback.setUrl(url);
        trackback.setBlogName(blogName);
        trackback.setTrackbackDateLong(new Date().getTime());
        trackback.setMetaData(trackbackMetaData);

        StringBuffer trackbackDirectory = new StringBuffer();
        String permalinkFilename = BlojsomUtils.getFilenameForPermalink(permalink, blogFileExtensions);
        permalinkFilename = BlojsomUtils.urlDecode(permalinkFilename);
        if (permalinkFilename == null) {
            _logger.debug("Invalid permalink trackback for: " + permalink);
            context.put(BLOJSOM_TRACKBACK_MESSAGE, "Invalid permalink trackback for: " + permalink);
            return new Integer(1);
        }

        trackbackDirectory.append(blogHome);
        trackbackDirectory.append(BlojsomUtils.removeInitialSlash(category));
        File blogEntry = new File(trackbackDirectory.toString() + File.separator + permalinkFilename);
        if (!blogEntry.exists()) {
            _logger.error("Trying to create trackback for invalid blog entry: " + permalink);
            context.put(BLOJSOM_TRACKBACK_MESSAGE, "Trying to create trackback for invalid permalink: " + category + permalinkFilename);
            return new Integer(1);
        }
        trackbackDirectory.append(blogTrackbackDirectory);
        trackbackDirectory.append(File.separator);
        trackbackDirectory.append(permalinkFilename);
        trackbackDirectory.append(File.separator);
        String trackbackFilename = trackbackDirectory.toString() + trackback.getTrackbackDateLong() + TRACKBACK_EXTENSION;
        trackback.setId(trackback.getTrackbackDateLong() + TRACKBACK_EXTENSION);
        File trackbackDir = new File(trackbackDirectory.toString());
        if (!trackbackDir.exists()) {
            if (!trackbackDir.mkdirs()) {
                _logger.error("Could not create directory for trackbacks: " + trackbackDirectory);
                context.put(BLOJSOM_TRACKBACK_MESSAGE, "Could not create directory for trackbacks");
                return new Integer(1);
            }
        }

        File trackbackEntry = new File(trackbackFilename);
        try {
            BufferedWriter bw = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(trackbackEntry), blogFileEncoding));
            bw.write(BlojsomUtils.nullToBlank(trackback.getTitle()).trim());
            bw.newLine();
            bw.write(BlojsomUtils.nullToBlank(trackback.getExcerpt()).trim());
            bw.newLine();
            bw.write(BlojsomUtils.nullToBlank(trackback.getUrl()).trim());
            bw.newLine();
            bw.write(BlojsomUtils.nullToBlank(trackback.getBlogName()).trim());
            bw.newLine();
            bw.close();
            _logger.debug("Added trackback: " + trackbackFilename);

            Properties trackbackMetaDataProperties = BlojsomUtils.mapToProperties(trackbackMetaData, UTF8);
            String trackbackMetaDataFilename = BlojsomUtils.getFilename(trackbackEntry.toString()) + DEFAULT_METADATA_EXTENSION;
            FileOutputStream fos = new FileOutputStream(new File(trackbackMetaDataFilename));
            trackbackMetaDataProperties.store(fos, null);
            fos.close();
            _logger.debug("Wrote trackback meta-data: " + trackbackMetaDataFilename);
        } catch (IOException e) {
            _logger.error(e);
            context.put(BLOJSOM_TRACKBACK_MESSAGE, "I/O error on trackback write.");
            return new Integer(1);
        }

        return new Integer(0);
    }


    /**
     * Send the trackback e-mail to the blog author
     *
     * @param emailPrefix E-mail prefix
     * @param title       Entry title
     * @param trackback   Trackback text
     * @param context     Context
     */
    public void sendTrackbackEmail(String emailPrefix, String title, String trackback, Map context) {
        EmailUtils.notifyBlogAuthor(emailPrefix + title, trackback, context);
    }


    /**
     * Perform any cleanup for the plugin. Called after {@link #process}.
     *
     * @throws BlojsomPluginException If there is an error performing cleanup for this plugin
     */
    public void cleanup() throws BlojsomPluginException {
    }

    /**
     * Called when BlojsomServlet is taken out of service
     *
     * @throws BlojsomPluginException If there is an error in finalizing this plugin
     */
    public void destroy() throws BlojsomPluginException {
    }
}
