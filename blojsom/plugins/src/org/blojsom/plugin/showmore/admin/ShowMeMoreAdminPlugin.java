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
package org.blojsom.plugin.showmore.admin;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.plugin.admin.WebAdminPlugin;
import org.blojsom.plugin.showmore.ShowMeMoreConfiguration;
import org.blojsom.plugin.showmore.ShowMeMorePlugin;
import org.blojsom.plugin.showmore.ShowMeMoreUtilities;
import org.blojsom.util.BlojsomUtils;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.IOException;
import java.util.Map;

/**
 * Show Me More administration plugin
 *
 * @author David Czarnecki
 * @version $Id: ShowMeMoreAdminPlugin.java,v 1.1.2.1 2005/07/21 04:30:40 johnan Exp $
 * @since blojsom 2.20
 */
public class ShowMeMoreAdminPlugin extends WebAdminPlugin {

    private Log _logger = LogFactory.getLog(ShowMeMoreAdminPlugin.class);

    private String _showMeMoreConfigurationFile;

    // Pages
    private static final String EDIT_SHOWMEMORE_SETTINGS_PAGE = "/org/blojsom/plugin/showmore/admin/templates/admin-edit-showmemore-settings";

    // Actions
    private static final String UPDATE_SHOWMEMORE_SETTINGS = "update-showmemore-settings";

    // Context attributes
    private static final String SHOWMEMORE_CONFIGURATION = "SHOWMEMORE_CONFIGURATION";

    // Permissions
    private static final String SHOWMEMORE_ADMIN_PERMISSION = "showmemore_admin";

    /**
     * Default constructor
     */
    public ShowMeMoreAdminPlugin() {
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

        _showMeMoreConfigurationFile = servletConfig.getInitParameter(ShowMeMorePlugin.SHOW_ME_MORE_CONFIG_IP);
        if (BlojsomUtils.checkNullOrBlank(_showMeMoreConfigurationFile)) {
            throw new BlojsomPluginException("No value given for: " + ShowMeMorePlugin.SHOW_ME_MORE_CONFIG_IP + " configuration parameter");
        }
    }

    /**
     * Return the display name for the plugin
     *
     * @return Display name for the plugin
     */
    public String getDisplayName() {
        return "Show Me More plugin";
    }

    /**
     * Return the name of the initial editing page for the plugin
     *
     * @return Name of the initial editing page for the plugin
     */
    public String getInitialPage() {
        return EDIT_SHOWMEMORE_SETTINGS_PAGE;
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
        entries = super.process(httpServletRequest, httpServletResponse, user, context, entries);
        String page = BlojsomUtils.getRequestValue(PAGE_PARAM, httpServletRequest);

        String username = getUsernameFromSession(httpServletRequest, user.getBlog());
        if (!checkPermission(user, null, username, SHOWMEMORE_ADMIN_PERMISSION)) {
            httpServletRequest.setAttribute(PAGE_PARAM, ADMIN_LOGIN_PAGE);
            addOperationResultMessage(context, "You are not allowed to edit show-me-more settings");

            return entries;
        }

        if (ADMIN_LOGIN_PAGE.equals(page)) {
            return entries;
        } else {
            String action = BlojsomUtils.getRequestValue(ACTION_PARAM, httpServletRequest);

            ShowMeMoreConfiguration showMeMoreConfiguration = new ShowMeMoreConfiguration();
            if (UPDATE_SHOWMEMORE_SETTINGS.equals(action)) {
                String cutoffLength = BlojsomUtils.getRequestValue(ShowMeMorePlugin.ENTRY_LENGTH_CUTOFF, httpServletRequest);
                int entryLengthCutoff = ShowMeMorePlugin.ENTRY_TEXT_CUTOFF_DEFAULT;
                try {
                    entryLengthCutoff = Integer.parseInt(cutoffLength);
                } catch (NumberFormatException e) {
                }
                String entryTextCutoff = BlojsomUtils.getRequestValue(ShowMeMorePlugin.ENTRY_TEXT_CUTOFF, httpServletRequest);
                String showMoreText = BlojsomUtils.getRequestValue(ShowMeMorePlugin.SHOW_ME_MORE_TEXT, httpServletRequest);
                String textCutoffStart = BlojsomUtils.getRequestValue(ShowMeMorePlugin.ENTRY_TEXT_CUTOFF_START, httpServletRequest);
                String textCutoffEnd = BlojsomUtils.getRequestValue(ShowMeMorePlugin.ENTRY_TEXT_CUTOFF_END, httpServletRequest);
                showMeMoreConfiguration = new ShowMeMoreConfiguration(entryLengthCutoff, entryTextCutoff, showMoreText, textCutoffStart, textCutoffEnd);

                try {
                    ShowMeMoreUtilities.saveConfiguration(user.getId(), _showMeMoreConfigurationFile, _blojsomConfiguration, showMeMoreConfiguration);
                } catch (IOException e) {
                    _logger.error(e);
                    addOperationResultMessage(context, "Unable to save Show Me More plugin settings");
                }
            }

            try {
                showMeMoreConfiguration = ShowMeMoreUtilities.loadConfiguration(user.getId(), _showMeMoreConfigurationFile, _blojsomConfiguration, _servletConfig);
            } catch (IOException e) {
                _logger.error(e);
            }
            context.put(SHOWMEMORE_CONFIGURATION, showMeMoreConfiguration);
        }

        return entries;
    }
}