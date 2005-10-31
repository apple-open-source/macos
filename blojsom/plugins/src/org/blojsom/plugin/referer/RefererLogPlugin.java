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
package org.blojsom.plugin.referer;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.BlojsomException;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.plugin.BlojsomPlugin;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.util.BlojsomConstants;
import org.blojsom.util.BlojsomProperties;
import org.blojsom.util.BlojsomUtils;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.*;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.*;
import java.util.regex.Pattern;

/**
 * Generic Referer Plugin
 *
 * @author Mark Lussier
 * @author David Czarnecki
 * @version $Id: RefererLogPlugin.java,v 1.2.2.1 2005/07/21 04:30:38 johnan Exp $
 */
public class RefererLogPlugin implements BlojsomPlugin {

    /**
     * HTTP Header for Referer Information
     */
    private static final String HEADER_REFERER = "referer";

    /**
     * web.xml init-param name
     */
    private static final String REFERER_CONFIG_IP = "plugin-referer";

    /**
     * Header written to the refer log file
     */
    private static final String REFERER_LOG_HEADER = "blojsom referer log";

    /**
     * Flavor field location
     */
    private static final int FIELD_FLAVOR = 0;

    /**
     * Date field location
     */
    private static final int FIELD_DATE = 1;

    /**
     * Hit counter field location
     */
    private static final int FIELD_COUNT = 2;

    /**
     * Hit counter key
     */
    private static final String HITCOUNTER_KEY = ".hitcounter";

    /**
     * Line Comment
     */
    private static final String COMMENTED_LINE = "#";

    /**
     * Default max length for storing referer URLs
     */
    private static final int REFERER_MAX_LENGTH_DEFAULT = 40;

    /**
     * Referer log filename initialization parameter
     */
    private static final String REFERER_LOG_FILE_IP = "referer-filename";

    /**
     * Max length for referer URLs initialization parameter
     */
    private static final String REFERER_MAX_LENGTH_IP = "referer-display-size";

    /**
     * Hit count flavors initialization parameter
     */
    private static final String REFERER_HIT_COUNTS_IP = "hit-count-flavors";

    /**
     * Format used to store last refer date for a given url
     */
    public static final String REFERER_DATE_FORMAT = "yyyy-MM-dd";

    /**
     * Key under which the "REFERER_HISTORY" groups will be place into the context
     * (example: on the request for the JSPDispatcher)
     */
    public static final String REFERER_CONTEXT_NAME = "REFERER_HISTORY";

    /**
     * Key under which the "REFERER_MAX_LENGTH" will be placed into the context
     * (example: on the request for the JSPDispatcher)
     */
    public static final String REFERER_CONTEXT_MAX_LENGTH = "REFERER_MAX_LENGTH";

    /**
     * Logger instance
     */
    private Log _logger = LogFactory.getLog(RefererLogPlugin.class);

    private Map _refererUsers;

    /**
     * Process the blojsom blacklist file to be able to filter referer's
     *
     * @param blacklistFile The blacklist file
     */
    private List populateBlacklistPatterns(ServletConfig servletConfig, String blacklistFile) {
        ArrayList blacklistPatterns = new ArrayList(5);
        InputStream is = servletConfig.getServletContext().getResourceAsStream(blacklistFile);

        if (is != null) {
            try {
                _logger.info("Processing blacklist filter [" + blacklistFile + "]");
                BufferedReader br = new BufferedReader(new InputStreamReader(is));
                String regexp = null;
                while (((regexp = br.readLine()) != null)) {
                    if (!regexp.startsWith(COMMENTED_LINE) && !"".equals(regexp)) {
                        blacklistPatterns.add(Pattern.compile(regexp));
                    }
                }
                br.close();
            } catch (IOException e) {
                _logger.error(e);
            }
        }

        return blacklistPatterns;
    }

    /**
     * Checks to see if a referer is blacklisted or not
     *
     * @param blacklistPatterns Compiled regular expressions to match referers
     * @param referer           The referer to check
     * @return A boolean indicating if this referer is blacklisted
     */
    private boolean isBlacklisted(List blacklistPatterns, String referer) {
        boolean result = false;
        if (blacklistPatterns != null) {
            int count = blacklistPatterns.size();
            if (referer != null && count > 0) {
                for (int x = 0; x < count; x++) {
                    result = ((Pattern) blacklistPatterns.get(x)).matcher(referer).find();
                    if (result) {
                        break;
                    }
                }
            }
        }

        return result;
    }

    /**
     * Loads the saved referer log from disk after a blojsom restart.
     *
     * @param refererlog        Fully qualified path to the refer log file
     * @param blacklistPatterns
     * @param refererGroups
     * @param hitCountFlavors
     */
    private void loadRefererLog(String refererlog, List blacklistPatterns, Map refererGroups, List hitCountFlavors) {
        File _refererfile = new File(refererlog);

        if (_refererfile.exists()) {
            Properties _refererproperties = new BlojsomProperties();

            try {
                InputStream is = new FileInputStream(_refererfile);
                _refererproperties.load(is);
                is.close();

                Enumeration _refererenum = _refererproperties.keys();
                while (_refererenum.hasMoreElements()) {
                    String _key = (String) _refererenum.nextElement();
                    String[] _details = BlojsomUtils.parseDelimitedList(_key, ".");

                    String _flavor = _details[FIELD_FLAVOR];
                    String _url = (String) _refererproperties.get(_key);
                    if (!isBlacklisted(blacklistPatterns, _url)) {
                        BlogRefererGroup _group;
                        if (refererGroups.containsKey(_flavor)) {
                            _group = (BlogRefererGroup) refererGroups.get(_flavor);
                        } else {
                            _group = new BlogRefererGroup(hitCountFlavors.contains(_flavor));
                        }

                        if (hitCountFlavors.contains(_flavor)) {
                            _group.addHitCount(getDateFromReferer(_details[FIELD_DATE]), Integer.parseInt(_details[FIELD_COUNT]));
                        } else {
                            _group.addReferer(_flavor, _url, getDateFromReferer(_details[FIELD_DATE]), Integer.parseInt(_details[FIELD_COUNT]));
                        }
                        refererGroups.put(_flavor, _group);
                    }
                }
            } catch (IOException e) {
                _logger.error(e);
            }
        }
    }

    /**
     * Initialize this plugin. This method only called when the plugin is instantiated.
     *
     * @param servletConfig        Servlet config object for the plugin to retrieve any initialization parameters
     * @param blojsomConfiguration {@link org.blojsom.blog.BlojsomConfiguration} information
     * @throws BlojsomPluginException If there is an error initializing the plugin
     */
    public void init(ServletConfig servletConfig, BlojsomConfiguration blojsomConfiguration) throws BlojsomPluginException {
        String refererConfiguration = servletConfig.getInitParameter(REFERER_CONFIG_IP);
        if (BlojsomUtils.checkNullOrBlank(refererConfiguration)) {
            throw new BlojsomPluginException("No value given for: " + REFERER_CONFIG_IP + " configuration parameter");
        }

        String[] users = blojsomConfiguration.getBlojsomUsers();
        _refererUsers = new HashMap(users.length);

        // For each user, try to load the referer properties file
        for (int i = 0; i < users.length; i++) {
            String user = users[i];
            Properties refererProperties = new BlojsomProperties();
            String configurationFile = blojsomConfiguration.getBaseConfigurationDirectory() + user + '/' + refererConfiguration;
            InputStream is = servletConfig.getServletContext().getResourceAsStream(configurationFile);
            if (is == null) {
                _logger.info("No referer log configuration file found: " + configurationFile);
            } else {
                try {
                    // Load the properties
                    refererProperties.load(is);
                    is.close();

                    int refererMaxLength = REFERER_MAX_LENGTH_DEFAULT;
                    List hitCountFlavors = new ArrayList();
                    String refererLog;

                    try {
                        BlogUser blog = blojsomConfiguration.loadBlog(user);
                        String blogUrlFilter = blog.getBlog().getBlogURL();

                        String maxlength = refererProperties.getProperty(REFERER_MAX_LENGTH_IP);
                        if (maxlength != null) {
                            try {
                                refererMaxLength = Integer.parseInt(maxlength);
                            } catch (NumberFormatException e) {
                                refererMaxLength = REFERER_MAX_LENGTH_DEFAULT;
                            }
                        }

                        String hitcounters = refererProperties.getProperty(REFERER_HIT_COUNTS_IP);
                        if (hitcounters != null) {
                            String[] _hitflavors = BlojsomUtils.parseCommaList(hitcounters);
                            for (int x = 0; x < _hitflavors.length; x++) {
                                hitCountFlavors.add(_hitflavors[x]);
                            }
                            _logger.info("Hit count flavors = " + hitCountFlavors.size());
                        }

                        refererLog = refererProperties.getProperty(REFERER_LOG_FILE_IP);

                        String blacklistFilename = refererProperties.getProperty(BlojsomConstants.BLOG_BLACKLIST_FILE_IP);
                        List blacklist = new ArrayList();
                        if (BlojsomUtils.checkNullOrBlank(blacklistFilename)) {
                            _logger.error("No value given for: " + BlojsomConstants.BLOG_BLACKLIST_FILE_IP + " configuration parameter for user: " + user);
                        } else {
                            blacklistFilename = blojsomConfiguration.getBaseConfigurationDirectory() + user + '/' + blacklistFilename;
                            blacklist = populateBlacklistPatterns(servletConfig, blacklistFilename);
                        }

                        Map refererGroups = new HashMap(5);
                        loadRefererLog(refererLog, blacklist, refererGroups, hitCountFlavors);

                        RefererLogConfiguration refererLogConfiguration = new RefererLogConfiguration(blacklist, refererLog, blogUrlFilter, refererGroups, hitCountFlavors, refererMaxLength);
                        _refererUsers.put(user, refererLogConfiguration);

                    } catch (BlojsomException e) {
                        _logger.error(e);
                    }
                } catch (IOException e) {
                    _logger.error(e);
                    throw new BlojsomPluginException(e);
                }
            }
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
    public BlogEntry[] process(HttpServletRequest httpServletRequest,
                               HttpServletResponse httpServletResponse,
                               BlogUser user,
                               Map context,
                               BlogEntry[] entries) throws BlojsomPluginException {
        String referer = httpServletRequest.getHeader(HEADER_REFERER);
        String flavor = httpServletRequest.getParameter(BlojsomConstants.FLAVOR_PARAM);
        String userId = user.getId();
        RefererLogConfiguration refererLogConfiguration = (RefererLogConfiguration) _refererUsers.get(userId);

        if (refererLogConfiguration == null) {
            // No referer properties loaded for user
            return entries;
        } else {

            List hitCountFlavors = refererLogConfiguration.getHitCountFlavors();
            Map refererGroups = refererLogConfiguration.getRefererGroups();

            if (!isBlacklisted(refererLogConfiguration.getBlacklistPatterns(), referer)) {
                if (flavor == null) {
                    flavor = BlojsomConstants.DEFAULT_FLAVOR_HTML;
                }

                if (hitCountFlavors.contains(flavor)) {
                    _logger.debug("[HitCounter] flavor=" + flavor + " - referer=" + referer);

                    BlogRefererGroup group;
                    if (refererGroups.containsKey(flavor)) {
                        group = (BlogRefererGroup) refererGroups.get(flavor);
                    } else {
                        group = new BlogRefererGroup(true);
                    }
                    group.addHitCount(new Date(), 1);
                    refererGroups.put(flavor, group);

                } else if ((referer != null) && (!referer.startsWith(refererLogConfiguration.getBlogUrlFilter()))) {
                    _logger.debug("[Referer] flavor=" + flavor + " - referer=" + referer);

                    BlogRefererGroup group;
                    if (refererGroups.containsKey(flavor)) {
                        group = (BlogRefererGroup) refererGroups.get(flavor);
                    } else {
                        group = new BlogRefererGroup(hitCountFlavors.contains(flavor));
                    }
                    group.addReferer(flavor, referer, new Date());
                    refererGroups.put(flavor, group);
                }
            }

            context.put(REFERER_CONTEXT_NAME, refererGroups);
            context.put(REFERER_CONTEXT_MAX_LENGTH, new Integer(refererLogConfiguration.getRefererMaxLength()));

            return entries;
        }
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
        Iterator refererUserIterator = _refererUsers.keySet().iterator();
        String user;
        while (refererUserIterator.hasNext()) {
            user = (String) refererUserIterator.next();
            RefererLogConfiguration refererLogConfiguration = (RefererLogConfiguration) _refererUsers.get(user);
            Map refererGroups = refererLogConfiguration.getRefererGroups();
            String refererlog = refererLogConfiguration.getRefererLog();

            // Writer referer cache out to disk
            Properties refererProperties = new BlojsomProperties();

            Iterator groupiterator = refererGroups.keySet().iterator();
            while (groupiterator.hasNext()) {
                String groupflavor = (String) groupiterator.next();
                BlogRefererGroup group = (BlogRefererGroup) refererGroups.get(groupflavor);
                if (group.isHitCounter()) {
                    refererProperties.put(groupflavor + "." + getRefererDate(group.getLastReferralDate()) + "." + group.getReferralCount(), HITCOUNTER_KEY);
                } else {
                    Iterator flavoriterator = group.keySet().iterator();
                    while (flavoriterator.hasNext()) {
                        String flavorkey = (String) flavoriterator.next();
                        BlogReferer referer = (BlogReferer) group.get(flavorkey);
                        refererProperties.put(groupflavor + "." + getRefererDate(referer.getLastReferral()) + "." + referer.getCount(),
                                referer.getUrl());
                    }
                }
            }

            try {
                FileOutputStream fos = new FileOutputStream(refererlog, false);
                refererProperties.store(fos, REFERER_LOG_HEADER);
                fos.close();
            } catch (IOException e) {
                _logger.error(e);
            }
        }
    }

    /**
     * Converts a string date in the form of yyyy-MM-dd to a Date
     *
     * @param rfcdate String in yyyy-MM-dd format
     * @return the Date
     */
    private static Date getDateFromReferer(String rfcdate) {
        Date result = null;
        SimpleDateFormat sdf = new SimpleDateFormat(REFERER_DATE_FORMAT);
        try {
            result = sdf.parse(rfcdate);
        } catch (ParseException e) {
            result = new Date();
        }
        return result;
    }

    /**
     * Converts a Date into a String for writing to the referer log
     *
     * @param date Date to write
     * @return String verion of date in the format of yyyy-MM-dd
     */
    public static String getRefererDate(Date date) {
        SimpleDateFormat sdf = new SimpleDateFormat(REFERER_DATE_FORMAT);
        return sdf.format(date);
    }

    /**
     * Internal class to hold referer log configuration information per user
     */
    private static class RefererLogConfiguration {

        /**
         * Contains compiled blacklist filter patterns
         */
        private List _blacklistPatterns;

        /**
         * Fully qualified filename to write referers to
         */
        private String _refererLog = null;

        /**
         * Contains the blog url to filter referes from sub-category entries
         */
        private String _blogUrlFilter = null;

        /**
         * Referer log groups
         */
        private Map _refererGroups;

        /**
         * Flavors that are hit counters and not referer loggers
         */
        private List _hitCountFlavors;

        /**
         * Max length for storing referer URLs
         */
        private int _refererMaxLength = REFERER_MAX_LENGTH_DEFAULT;

        /**
         * Default constructor
         *
         * @param blacklistPatterns Compiled regular expressions of blacklisted referers
         * @param refererLog        Referer log filename (absolute filename)
         * @param blogUrlFilter     URL of the blog to be filtered
         * @param refererGroups     Referer log groups
         * @param hitCountFlavors   Flavors that are hit counters and not referer loggers
         * @param refererMaxLength  Max length for storing referer URLs
         */
        public RefererLogConfiguration(List blacklistPatterns,
                                       String refererLog,
                                       String blogUrlFilter,
                                       Map refererGroups,
                                       List hitCountFlavors,
                                       int refererMaxLength) {
            _blacklistPatterns = blacklistPatterns;
            _refererLog = refererLog;
            _blogUrlFilter = blogUrlFilter;
            _refererGroups = refererGroups;
            _hitCountFlavors = hitCountFlavors;
            _refererMaxLength = refererMaxLength;
        }

        /**
         * Retrieve the list of blacklist patterns
         *
         * @return Blacklist patterns
         */
        public List getBlacklistPatterns() {
            return _blacklistPatterns;
        }

        /**
         * Retrieve the absolute log file filename
         *
         * @return Log file filename
         */
        public String getRefererLog() {
            return _refererLog;
        }

        /**
         * Retrieve the URL of the blog to be filtered
         *
         * @return URL of the blog to be filtered
         */
        public String getBlogUrlFilter() {
            return _blogUrlFilter;
        }

        /**
         * Retrieve the referer log groups
         *
         * @return Referer log groups
         */
        public Map getRefererGroups() {
            return _refererGroups;
        }

        /**
         * Retrieve the flavors that are hit counters and not referer loggers
         *
         * @return Flavors that are hit counters and not referer loggers
         */
        public List getHitCountFlavors() {
            return _hitCountFlavors;
        }

        /**
         * Retrieve the max length for storing referer URLs
         *
         * @return Max length for storing referer URLs
         */
        public int getRefererMaxLength() {
            return _refererMaxLength;
        }
    }
}
