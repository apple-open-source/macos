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
package org.blojsom.plugin.admin;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.util.BlojsomProperties;
import org.blojsom.util.BlojsomUtils;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.*;

/**
 * ThemeSwitcherPlugin
 *
 * @author David Czarnecki
 * @version $Id: ThemeSwitcherPlugin.java,v 1.1.2.1 2005/07/21 04:30:24 johnan Exp $
 * @since blojsom 2.19
 */
public class ThemeSwitcherPlugin extends WebAdminPlugin {

    private Log _logger = LogFactory.getLog(ThemeSwitcherPlugin.class);

    // Pages
    private static final String THEME_SWITCHER_SETTINGS_PAGE = "/org/blojsom/plugin/admin/templates/admin-theme-switcher-settings";

    // Context variables
    private static final String THEME_SWITCHER_PLUGIN_AVAILABLE_THEMES = "THEME_SWITCHER_PLUGIN_AVAILABLE_THEMES";
    private static final String THEME_SWITCHER_PLUGIN_FLAVORS = "THEME_SWITCHER_PLUGIN_FLAVORS";
    private static final String THEME_SWITCHER_PLUGIN_DEFAULT_FLAVOR = "THEME_SWITCHER_PLUGIN_DEFAULT_FLAVOR";
    private static final String CURRENT_HTML_THEME = "CURRENT_HTML_THEME";

    // Actions
    private static final String SWITCH_THEME_ACTION = "switch-theme";

    // Form items
    private static final String THEME = "theme";
    private static final String FLAVOR = "flavor-name";

    // Permissions
    private static final String SWITCH_THEME_PERMISSION = "switch_theme";

    private static final String DEFAULT_THEMES_DIRECTORY = "/themes/";
    private static final String THEMES_DIRECTORY_IP = "themes-directory";
    private String _themesDirectory;
    private String _flavorConfiguration;

    /**
     * Default constructor
     */
    public ThemeSwitcherPlugin() {
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

        _themesDirectory = servletConfig.getInitParameter(THEMES_DIRECTORY_IP);
        if (BlojsomUtils.checkNullOrBlank(_themesDirectory)) {
            _themesDirectory = DEFAULT_THEMES_DIRECTORY;
        }

        _themesDirectory = BlojsomUtils.checkStartingAndEndingSlash(_themesDirectory);
        _flavorConfiguration = servletConfig.getInitParameter(BLOJSOM_FLAVOR_CONFIGURATION_IP);
    }

    /**
     * Return the display name for the plugin
     *
     * @return Display name for the plugin
     */
    public String getDisplayName() {
        return "Theme Switcher plugin";
    }

    /**
     * Return the name of the initial editing page for the plugin
     *
     * @return Name of the initial editing page for the plugin
     */
    public String getInitialPage() {
        return THEME_SWITCHER_SETTINGS_PAGE;
    }

    /**
     * Retrieve the list of directories (theme names) from the themes installation directory
     *
     * @return List of theme names available
     */
    protected String[] getAvailableThemes() {
        ArrayList themes = new ArrayList(0);

        File themesDirectory = new File(_blojsomConfiguration.getInstallationDirectory() +
                _blojsomConfiguration.getBaseConfigurationDirectory() + _themesDirectory);
        if (themesDirectory.exists() && themesDirectory.isDirectory()) {
            File[] themesInstalled = themesDirectory.listFiles(BlojsomUtils.getDirectoryFilter());
            if (themesInstalled != null && themesInstalled.length > 0) {
                for (int i = 0; i < themesInstalled.length; i++) {
                    File installedTheme = themesInstalled[i];
                    themes.add(installedTheme.getName());
                }
            }
        }

        String[] availableThemes = (String[]) themes.toArray(new String[themes.size()]);
        Arrays.sort(availableThemes);
        return availableThemes;
    }

    /**
     * Write out the flavor configuration file for a particular user
     *
     * @param user Blog user information
     * @throws java.io.IOException If there is an error writing the flavor configuration file
     */
    protected void writeFlavorConfiguration(BlogUser user) throws IOException {
        Iterator flavorIterator = user.getFlavors().keySet().iterator();
        Properties flavorProperties = new BlojsomProperties();
        Map flavorToTemplateMap = user.getFlavorToTemplate();
        Map flavorToContentTypeMap = user.getFlavorToContentType();

        while (flavorIterator.hasNext()) {
            String flavor = (String) flavorIterator.next();
            flavorProperties.setProperty(flavor, flavorToTemplateMap.get(flavor) + ", " + flavorToContentTypeMap.get(flavor));
        }

        File flavorFile = new File(_blojsomConfiguration.getInstallationDirectory() + _blojsomConfiguration.getBaseConfigurationDirectory() + user.getId() + "/" + _flavorConfiguration);
        FileOutputStream fos = new FileOutputStream(flavorFile);
        flavorProperties.store(fos, null);
        fos.close();
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
        if (!checkPermission(user, null, username, SWITCH_THEME_PERMISSION)) {
            httpServletRequest.setAttribute(PAGE_PARAM, ADMIN_LOGIN_PAGE);
            addOperationResultMessage(context, "You are not allowed to switch themes");

            return entries;
        }

        if (ADMIN_LOGIN_PAGE.equals(page)) {
            return entries;
        } else {
            String action = BlojsomUtils.getRequestValue(ACTION_PARAM, httpServletRequest);

            context.put(THEME_SWITCHER_PLUGIN_AVAILABLE_THEMES, getAvailableThemes());
            context.put(THEME_SWITCHER_PLUGIN_FLAVORS, new TreeMap(user.getFlavors()));
            context.put(THEME_SWITCHER_PLUGIN_DEFAULT_FLAVOR, user.getBlog().getBlogDefaultFlavor());
            String currentHtmlFlavor = (String) user.getFlavorToTemplate().get("html");
            currentHtmlFlavor = currentHtmlFlavor.substring(0, currentHtmlFlavor.indexOf('.'));
            context.put(CURRENT_HTML_THEME, currentHtmlFlavor);
            
            if (SWITCH_THEME_ACTION.equals(action)) {
                String theme = BlojsomUtils.getRequestValue(THEME, httpServletRequest);
                String flavor = BlojsomUtils.getRequestValue(FLAVOR, httpServletRequest);

                if (BlojsomUtils.checkNullOrBlank(theme) || BlojsomUtils.checkNullOrBlank(flavor)) {
                    addOperationResultMessage(context, "No theme or flavor selected");
                    return entries;
                }

                if ("admin".equalsIgnoreCase(flavor)) {
                    addOperationResultMessage(context, "Theme cannot be changed for admin flavor");
                    return entries;
                }

                File copyFromTemplatesDirectory = new File(_blojsomConfiguration.getInstallationDirectory() +
                        _blojsomConfiguration.getBaseConfigurationDirectory() + _themesDirectory + theme + "/" +
                        _blojsomConfiguration.getTemplatesDirectory());

                File[] templateFiles = copyFromTemplatesDirectory.listFiles();
                String mainTemplate = null;
                if (templateFiles != null && templateFiles.length > 0) {
                    for (int i = 0; i < templateFiles.length; i++) {
                        File templateFile = templateFiles[i];
                        if (!templateFile.isDirectory()) {
                            if (templateFile.getName().startsWith(theme + ".")) {
                                mainTemplate = templateFile.getName();
                            }
                        }
                    }
                }
                if (mainTemplate == null) {
                    mainTemplate = (String) user.getFlavorToTemplate().get(flavor);
                    _logger.debug("No main template supplied for " + theme + " theme. Using existing template for flavor: " + mainTemplate);
                }

                File copyToTemplatesDirectory = new File(_blojsomConfiguration.getInstallationDirectory() +
                        _blojsomConfiguration.getBaseConfigurationDirectory() + user.getId() + "/" +
                        _blojsomConfiguration.getTemplatesDirectory());

                try {
                    BlojsomUtils.copyDirectory(copyFromTemplatesDirectory, copyToTemplatesDirectory);
                } catch (IOException e) {
                    _logger.error(e);
                    addOperationResultMessage(context, "Unable to copy theme templates to user's template directory");
                }

                File copyFromResourcesDirectory = new File(_blojsomConfiguration.getInstallationDirectory() +
                        _blojsomConfiguration.getBaseConfigurationDirectory() + _themesDirectory + theme + "/" +
                        _blojsomConfiguration.getResourceDirectory());
                File copyToResourcesDirectory = new File(_blojsomConfiguration.getInstallationDirectory() +
                        _blojsomConfiguration.getResourceDirectory() + user.getId() + "/");

                try {
                    BlojsomUtils.copyDirectory(copyFromResourcesDirectory, copyToResourcesDirectory);
                } catch (IOException e) {
                    _logger.error(e);
                    addOperationResultMessage(context, "Unable to copy theme resources to user's resource directory");
                }

                Map flavorTemplatesForUser = user.getFlavorToTemplate();
                flavorTemplatesForUser.put(flavor, mainTemplate);
                try {
                    writeFlavorConfiguration(user);
                } catch (IOException e) {
                    _logger.error(e);
                    addOperationResultMessage(context, "Unable to write flavor configuration for user");

                    return entries;
                }

                currentHtmlFlavor = (String) user.getFlavorToTemplate().get("html");
                currentHtmlFlavor = currentHtmlFlavor.substring(0, currentHtmlFlavor.indexOf('.'));
                context.put(CURRENT_HTML_THEME, currentHtmlFlavor);

                addOperationResultMessage(context, "Theme switched to: " + theme + " for flavor: " + flavor);
            }
        }

        return entries;
    }
}