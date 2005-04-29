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
package org.blojsom.plugin.showmore;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.plugin.BlojsomPlugin;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.util.BlojsomUtils;
import org.blojsom.util.BlojsomProperties;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.IOException;
import java.io.InputStream;
import java.util.HashMap;
import java.util.Map;
import java.util.Properties;
import java.util.regex.Pattern;
import java.util.regex.Matcher;

/**
 * ShowMeMorePlugin
 *
 * @author David Czarnecki
 * @version $Id: ShowMeMorePlugin.java,v 1.2 2004/08/27 01:06:40 whitmore Exp $
 */
public class ShowMeMorePlugin implements BlojsomPlugin {

    private Log _logger = LogFactory.getLog(ShowMeMorePlugin.class);

    private static final String SHOW_ME_MORE_CONFIG_IP = "plugin-showmemore";
    private static final String ENTRY_LENGTH_CUTOFF = "entry-length-cutoff";
    private static final String ENTRY_TEXT_CUTOFF = "entry-text-cutoff";
    private static final String SHOW_ME_MORE_TEXT = "show-me-more-text";
    private static final String ENTRY_TEXT_CUTOFF_START = "entry-text-cutoff-start";
    private static final String ENTRY_TEXT_CUTOFF_END = "entry-text-cutoff-end";
    private static final String SHOW_ME_MORE_PARAM = "smm";
    private static final int ENTRY_TEXT_CUTOFF_DEFAULT = 400;

    private Map _showMeMoreConfiguration;

    /**
     * Default constructor
     */
    public ShowMeMorePlugin() {
    }

    /**
     * Initialize this plugin. This method only called when the plugin is instantiated.
     *
     * @param servletConfig        Servlet config object for the plugin to retrieve any initialization parameters
     * @param blojsomConfiguration {@link BlojsomConfiguration} information
     * @throws BlojsomPluginException If there is an error initializing the plugin
     */
    public void init(ServletConfig servletConfig, BlojsomConfiguration blojsomConfiguration) throws BlojsomPluginException {
        String showMeMoreConfiguration = servletConfig.getInitParameter(SHOW_ME_MORE_CONFIG_IP);
        if (BlojsomUtils.checkNullOrBlank(showMeMoreConfiguration)) {
            throw new BlojsomPluginException("No value given for: " + SHOW_ME_MORE_CONFIG_IP + " configuration parameter");
        }

        String[] users = blojsomConfiguration.getBlojsomUsers();
        _showMeMoreConfiguration = new HashMap(users.length);
        for (int i = 0; i < users.length; i++) {
            String user = users[i];
            Properties showMeMoreProperties = new BlojsomProperties();
            String configurationFile = blojsomConfiguration.getBaseConfigurationDirectory() + user + '/' + showMeMoreConfiguration;
            InputStream is = servletConfig.getServletContext().getResourceAsStream(configurationFile);
            if (is == null) {
                _logger.info("No show me more configuration file found: " + configurationFile);
            } else {
                try {
                    showMeMoreProperties.load(is);
                    is.close();
                    String moreText = showMeMoreProperties.getProperty(SHOW_ME_MORE_TEXT);
                    String textCutoff = showMeMoreProperties.getProperty(ENTRY_TEXT_CUTOFF);
                    String textCutoffStart = showMeMoreProperties.getProperty(ENTRY_TEXT_CUTOFF_START);
                    String textCutoffEnd = showMeMoreProperties.getProperty(ENTRY_TEXT_CUTOFF_END);
                    int cutoff;
                    try {
                        cutoff = Integer.parseInt(showMeMoreProperties.getProperty(ENTRY_LENGTH_CUTOFF));
                    } catch (NumberFormatException e) {
                        cutoff = ENTRY_TEXT_CUTOFF_DEFAULT;
                    }
                    ShowMeMoreConfiguration showMeMore = new ShowMeMoreConfiguration(cutoff, textCutoff, moreText, textCutoffStart, textCutoffEnd);
                    _showMeMoreConfiguration.put(user, showMeMore);
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
        String wantsToSeeMore = httpServletRequest.getParameter(SHOW_ME_MORE_PARAM);
        if ("y".equalsIgnoreCase(wantsToSeeMore)) {
            return entries;
        } else {
            String userId = user.getId();
            if (!_showMeMoreConfiguration.containsKey(userId)) {
                return entries;
            } else {
                ShowMeMoreConfiguration showMeMoreConfiguration = (ShowMeMoreConfiguration) _showMeMoreConfiguration.get(userId);
                int cutoff = showMeMoreConfiguration.getCutoff();
                String textCutoff = showMeMoreConfiguration.getTextCutoff();
                String moreText = showMeMoreConfiguration.getMoreText();
                String textCutoffStart = showMeMoreConfiguration.getTextCutoffStart();
                String textCutoffEnd = showMeMoreConfiguration.getTextCutoffEnd();

                for (int i = 0; i < entries.length; i++) {
                    BlogEntry entry = entries[i];
                    String description = entry.getDescription();
                    StringBuffer partialDescription = new StringBuffer();
                    int indexOfCutoffText;

                    if (!BlojsomUtils.checkNullOrBlank(textCutoffStart) && !BlojsomUtils.checkNullOrBlank(textCutoffEnd)) {
                        StringBuffer showMeMoreText = new StringBuffer("<a href=\"");
                        showMeMoreText.append(entry.getLink());
                        showMeMoreText.append("&amp;");
                        showMeMoreText.append(SHOW_ME_MORE_PARAM);
                        showMeMoreText.append("=y\">");
                        showMeMoreText.append(moreText);
                        showMeMoreText.append("</a>");
                        Pattern cutoffPattern = Pattern.compile("(" + textCutoffStart + ".*?" + textCutoffEnd + ").*?", Pattern.DOTALL | Pattern.MULTILINE | Pattern.CASE_INSENSITIVE);
                        Matcher cutoffMatcher = cutoffPattern.matcher(description);
                        if (cutoffMatcher.find()) {
                            description = cutoffMatcher.replaceAll(showMeMoreText.toString());
                            entry.setDescription(description);
                        }
                    }

                    if (!BlojsomUtils.checkNullOrBlank(textCutoff)) {
                        indexOfCutoffText = description.indexOf(textCutoff);
                        if (indexOfCutoffText != -1) {
                            partialDescription.append(description.substring(0, indexOfCutoffText));
                            partialDescription.append("&nbsp; <a href=\"");
                            partialDescription.append(entry.getLink());
                            partialDescription.append("&amp;");
                            partialDescription.append(SHOW_ME_MORE_PARAM);
                            partialDescription.append("=y\">");
                            partialDescription.append(moreText);
                            partialDescription.append("</a>");
                            entry.setDescription(partialDescription.toString());
                        } else if ((cutoff > 0) && (description.length() > cutoff)) {
                            partialDescription.append(description.substring(0, cutoff));
                            partialDescription.append("&nbsp; <a href=\"");
                            partialDescription.append(entry.getLink());
                            partialDescription.append("&amp;");
                            partialDescription.append(SHOW_ME_MORE_PARAM);
                            partialDescription.append("=y\">");
                            partialDescription.append(moreText);
                            partialDescription.append("</a>");
                            entry.setDescription(partialDescription.toString());
                        }
                    } else if ((cutoff > 0) && (description.length() > cutoff)) {
                        partialDescription.append(description.substring(0, cutoff));
                        partialDescription.append("&nbsp; <a href=\"");
                        partialDescription.append(entry.getLink());
                        partialDescription.append("&amp;");
                        partialDescription.append(SHOW_ME_MORE_PARAM);
                        partialDescription.append("=y\">");
                        partialDescription.append(moreText);
                        partialDescription.append("</a>");
                        entry.setDescription(partialDescription.toString());
                    }
                }

                return entries;
            }
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
    }

    /**
     * Internal class to hold configuration properties
     */
    private static class ShowMeMoreConfiguration {

        private int _cutoff;
        private String _textCutoff;
        private String _moreText;
        private String _textCutoffStart;
        private String _textCutoffEnd;

        /**
         * Default constructor
         *
         * @param cutoff          Cutoff length
         * @param textCutoff      Cutoff string
         * @param moreText        Text to insert when making a cut
         * @param textCutoffStart Start tag for cutting parts of entries
         * @param textCutoffEnd   End tag for cutting parts of entries
         */
        public ShowMeMoreConfiguration(int cutoff, String textCutoff, String moreText, String textCutoffStart, String textCutoffEnd) {
            _cutoff = cutoff;
            _textCutoff = textCutoff;
            _moreText = moreText;
            _textCutoffStart = textCutoffStart;
            _textCutoffEnd = textCutoffEnd;
        }

        /**
         * Cutoff length
         *
         * @return Cutoff length
         */
        public int getCutoff() {
            return _cutoff;
        }

        /**
         * Cutoff string
         *
         * @return Cutoff string
         */
        public String getTextCutoff() {
            return _textCutoff;
        }

        /**
         * Text to insert when making a cut
         *
         * @return Text to insert when making a cut
         */
        public String getMoreText() {
            return _moreText;
        }

        /**
         * Start tag for cutting parts of entries
         *
         * @return Start tag for cutting parts of entries
         */
        public String getTextCutoffStart() {
            return _textCutoffStart;
        }

        /**
         * End tag for cutting parts of entries
         *
         * @return End tag for cutting parts of entries
         */
        public String getTextCutoffEnd() {
            return _textCutoffEnd;
        }
    }
}
