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
package org.blojsom.plugin.showmore;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.plugin.BlojsomPlugin;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.util.BlojsomUtils;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.IOException;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * ShowMeMorePlugin
 *
 * @author David Czarnecki
 * @version $Id: ShowMeMorePlugin.java,v 1.2.2.1 2005/07/21 04:30:40 johnan Exp $
 */
public class ShowMeMorePlugin implements BlojsomPlugin {

    private Log _logger = LogFactory.getLog(ShowMeMorePlugin.class);

    private static final String SHOW_ME_MORE_PARAM = "smm";

    public static final String SHOW_ME_MORE_CONFIG_IP = "plugin-showmemore";
    public static final String ENTRY_LENGTH_CUTOFF = "entry-length-cutoff";
    public static final String ENTRY_TEXT_CUTOFF = "entry-text-cutoff";
    public static final String SHOW_ME_MORE_TEXT = "show-me-more-text";
    public static final String ENTRY_TEXT_CUTOFF_START = "entry-text-cutoff-start";
    public static final String ENTRY_TEXT_CUTOFF_END = "entry-text-cutoff-end";
    public static final int ENTRY_TEXT_CUTOFF_DEFAULT = 400;

    private String _showMeMoreConfigurationFile;
    private BlojsomConfiguration _blojsomConfiguration;
    private ServletConfig _servletConfig;

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
        _servletConfig = servletConfig;
        _blojsomConfiguration = blojsomConfiguration;

        _showMeMoreConfigurationFile = servletConfig.getInitParameter(SHOW_ME_MORE_CONFIG_IP);
        if (BlojsomUtils.checkNullOrBlank(_showMeMoreConfigurationFile)) {
            throw new BlojsomPluginException("No value given for: " + SHOW_ME_MORE_CONFIG_IP + " configuration parameter");
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
            ShowMeMoreConfiguration showMeMoreConfiguration;
            try {
                showMeMoreConfiguration = ShowMeMoreUtilities.loadConfiguration(user.getId(), _showMeMoreConfigurationFile, _blojsomConfiguration, _servletConfig);
            } catch (IOException e) {
                _logger.error(e);

                return entries;
            }

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
