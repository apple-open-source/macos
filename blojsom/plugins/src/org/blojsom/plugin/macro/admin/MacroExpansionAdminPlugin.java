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
package org.blojsom.plugin.macro.admin;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.plugin.admin.WebAdminPlugin;
import org.blojsom.plugin.macro.MacroExpansionPlugin;
import org.blojsom.util.BlojsomProperties;
import org.blojsom.util.BlojsomUtils;

import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Map;
import java.util.Properties;
import java.util.TreeMap;

/**
 * Macro Expansion Admin Plugin
 *
 * @author David Czarnecki
 * @version $Id: MacroExpansionAdminPlugin.java,v 1.1.2.1 2005/07/21 04:30:33 johnan Exp $
 * @since blojsom 2.16
 */
public class MacroExpansionAdminPlugin extends WebAdminPlugin {

    private Log _logger = LogFactory.getLog(MacroExpansionAdminPlugin.class);

    private static final String EDIT_MACRO_EXPANSION_SETTINGS_PAGE = "/org/blojsom/plugin/macro/admin/templates/admin-edit-macro-expansion-settings";

    private static final String BLOJSOM_PLUGIN_EDIT_MACRO_EXPANSION_MACROS = "BLOJSOM_PLUGIN_EDIT_MACRO_EXPANSION_MACROS";

    // Form items
    private static final String MACRO_SHORT_NAME = "macro-short-name";
    private static final String MACRO_EXPANSION = "macro-expansion";
    private static final String MACROS = "macros";

    // Actions
    private static final String ADD_MACRO_ACTION = "add-macro";
    private static final String DELETE_SELECTED_MACROS_ACTION = "delete-selected-macros";

    // Permissions
    private static final String MACRO_EXPANSION_ADMIN_PERMISSION = "macro_expansion_admin";

    /**
     * Default constructor
     */
    public MacroExpansionAdminPlugin() {
    }

    /**
     * Return the display name for the plugin
     *
     * @return Display name for the plugin
     */
    public String getDisplayName() {
        return "Macro Expansion plugin";
    }

    /**
     * Return the name of the initial editing page for the plugin
     *
     * @return Name of the initial editing page for the plugin
     */
    public String getInitialPage() {
        return EDIT_MACRO_EXPANSION_SETTINGS_PAGE;
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
        if (!checkPermission(user, null, username, MACRO_EXPANSION_ADMIN_PERMISSION)) {
            httpServletRequest.setAttribute(PAGE_PARAM, ADMIN_LOGIN_PAGE);
            addOperationResultMessage(context, "You are not allowed to edit macros");

            return entries;
        }

        if (ADMIN_LOGIN_PAGE.equals(page)) {
            return entries;
        } else {
            String action = BlojsomUtils.getRequestValue(ACTION_PARAM, httpServletRequest);

            String macroConfiguration = _servletConfig.getInitParameter(MacroExpansionPlugin.BLOG_MACRO_CONFIGURATION_IP);
            if (BlojsomUtils.checkNullOrBlank(macroConfiguration)) {
                httpServletRequest.setAttribute(PAGE_PARAM, ADMIN_ADMINISTRATION_PAGE);
                addOperationResultMessage(context, "No configuration file for macro expansion plugin specified in " + MacroExpansionPlugin.BLOG_MACRO_CONFIGURATION_IP + " configuration parameter");

                return entries;
            } else {
                Map macros = readMacros(user.getId(), macroConfiguration);

                if (DELETE_SELECTED_MACROS_ACTION.equals(action)) {
                    String[] macrosToDelete = httpServletRequest.getParameterValues(MACROS);
                    if (macrosToDelete != null && macrosToDelete.length > 0) {
                        for (int i = 0; i < macrosToDelete.length; i++) {
                            String macro = macrosToDelete[i];
                            macros.remove(macro);
                        }

                        try {
                            writeMacroConfiguration(user.getId(), macroConfiguration, macros);
                        } catch (IOException e) {
                            _logger.error(e);
                        }
                        addOperationResultMessage(context, "Deleted " + macrosToDelete.length + " macros");
                    } else {
                        addOperationResultMessage(context, "No macros selected for deletion");
                    }
                } else if (ADD_MACRO_ACTION.equals(action)) {
                    String macroShortName = BlojsomUtils.getRequestValue(MACRO_SHORT_NAME, httpServletRequest);
                    String macroExpansion = BlojsomUtils.getRequestValue(MACRO_EXPANSION, httpServletRequest);

                    if (BlojsomUtils.checkNullOrBlank(macroShortName) || BlojsomUtils.checkNullOrBlank(macroExpansion)) {
                        addOperationResultMessage(context, "No macro short name or macro expansion provided");
                    } else {
                        if (!macros.containsKey(macroShortName)) {
                            macros.put(macroShortName, macroExpansion);

                            try {
                                writeMacroConfiguration(user.getId(), macroConfiguration, macros);
                            } catch (IOException e) {
                                _logger.error(e);
                            }
                            addOperationResultMessage(context, "Added macro: " + macroShortName);
                        } else {
                            addOperationResultMessage(context, "Macro short name: " + macroShortName + " already exists");
                        }
                    }
                }

                readMacros(user.getId(), macroConfiguration);
                context.put(BLOJSOM_PLUGIN_EDIT_MACRO_EXPANSION_MACROS, macros);
            }
        }

        return entries;
    }

    /**
     * Read in the macros from the specified configuration file in the user's directory
     *
     * @param userId             User ID
     * @param macroConfiguration Macro configuration file
     * @return Macros as a {@link Map}
     * @throws BlojsomPluginException If there is an error reading in the macros
     */
    private Map readMacros(String userId, String macroConfiguration) throws BlojsomPluginException {
        Map macros = new TreeMap();
        Properties macroProperties = new BlojsomProperties();
        String configurationFile = _blojsomConfiguration.getBaseConfigurationDirectory() + userId + '/' + macroConfiguration;
        InputStream is = _servletConfig.getServletContext().getResourceAsStream(configurationFile);
        if (is == null) {
            _logger.info("No macro configuration file found: " + configurationFile);
        } else {
            try {
                macroProperties.load(is);
                is.close();
                macros = new TreeMap(BlojsomUtils.propertiesToMap(macroProperties));
            } catch (IOException e) {
                _logger.error(e);
                throw new BlojsomPluginException(e);
            }
        }

        return macros;
    }

    /**
     * Write out the macros configuration file for a particular user
     *
     * @param userId             User ID
     * @param macroConfiguration Macro configuration filename
     * @param macros             Map of the macros
     * @throws java.io.IOException If there is an error writing the plugin configuration file
     */
    private void writeMacroConfiguration(String userId, String macroConfiguration, Map macros) throws IOException {
        File macroConfigurationFile = new File(_blojsomConfiguration.getInstallationDirectory() + _blojsomConfiguration.getBaseConfigurationDirectory() + userId + "/" + macroConfiguration);
        FileOutputStream fos = new FileOutputStream(macroConfigurationFile);
        Properties macroProperties = BlojsomUtils.mapToProperties(macros);
        macroProperties.store(fos, null);
        fos.close();
    }
}