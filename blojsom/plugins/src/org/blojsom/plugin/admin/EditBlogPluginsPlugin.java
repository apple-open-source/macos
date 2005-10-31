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
package org.blojsom.plugin.admin;

import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.util.BlojsomUtils;
import org.blojsom.BlojsomException;
import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;

import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.ServletConfig;
import java.util.*;
import java.io.IOException;
import java.io.File;
import java.io.FileOutputStream;

/**
 * EditBlogPluginsPlugin
 *
 * @since blojsom 2.06
 * @author czarnecki
 * @version $Id: EditBlogPluginsPlugin.java,v 1.2.2.1 2005/07/21 04:30:24 johnan Exp $
 */
public class EditBlogPluginsPlugin extends BaseAdminPlugin {

    private Log _logger = LogFactory.getLog(EditBlogPluginsPlugin.class);

    // Pages
    private static final String EDIT_BLOG_PLUGINS_PAGE = "/org/blojsom/plugin/admin/templates/admin-edit-blog-plugins";

    // Constants
    private static final String BLOJSOM_PLUGIN_EDIT_BLOG_PLUGINS_MAP = "BLOJSOM_PLUGIN_EDIT_BLOG_PLUGINS_MAP";
    private static final String BLOJSOM_PLUGIN_EDIT_BLOG_PLUGINS_AVAILABLE_PLUGINS = "BLOJSOM_PLUGIN_EDIT_BLOG_PLUGINS_AVAILABLE_PLUGINS";

    // Actions
    private static final String MODIFY_PLUGIN_CHAINS = "modify-plugin-chains";

    // Permissions
    private static final String EDIT_BLOG_PLUGINS_PERMISSION = "edit_blog_plugins";

    private String _pluginConfiguration;
    private Map _plugins;

    /**
     * Default constructor
     */
    public EditBlogPluginsPlugin() {
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

        _pluginConfiguration = servletConfig.getInitParameter(BLOJSOM_PLUGIN_CONFIGURATION_IP);
        try {
            Properties pluginProperties = BlojsomUtils.loadProperties(servletConfig, _blojsomConfiguration.getBaseConfigurationDirectory() + _pluginConfiguration);
            _plugins = BlojsomUtils.propertiesToMap(pluginProperties);
            Map plugins = BlojsomUtils.propertiesToMap(pluginProperties);
            Iterator pluginsIterator = plugins.keySet().iterator();
            while (pluginsIterator.hasNext()) {
                String pluginName = (String) pluginsIterator.next();
                if (pluginName.indexOf(BLOJSOM_PLUGIN_CHAIN) != -1) {
                    _plugins.remove(pluginName);
                }
                if (((String)plugins.get(pluginName)).indexOf("admin") != -1) {
                    _plugins.remove(pluginName);
                }
            }
        } catch (BlojsomException e) {
            _logger.error(e);
            throw new BlojsomPluginException(e);
        }

        _plugins = new TreeMap(_plugins);
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
        if (!authenticateUser(httpServletRequest, httpServletResponse, context, user)) {
            httpServletRequest.setAttribute(PAGE_PARAM, ADMIN_LOGIN_PAGE);

            return entries;
        }

        String username = getUsernameFromSession(httpServletRequest, user.getBlog());
        if (!checkPermission(user, null, username, EDIT_BLOG_PLUGINS_PERMISSION)) {
            httpServletRequest.setAttribute(PAGE_PARAM, ADMIN_LOGIN_PAGE);
            addOperationResultMessage(context, "You are not allowed to edit blog plugins");

            return entries;
        }

        // Create the plugin chain map
        Iterator flavorIterator = user.getFlavors().keySet().iterator();
        Map userPluginChain = user.getPluginChain();
        Map updatedPluginChain = new TreeMap();

        while (flavorIterator.hasNext()) {
            String flavor = (String) flavorIterator.next();
            if (userPluginChain.containsKey(flavor + "." + BLOJSOM_PLUGIN_CHAIN)) {
                updatedPluginChain.put(flavor, BlojsomUtils.arrayOfStringsToString((String[]) userPluginChain.get(flavor + "." + BLOJSOM_PLUGIN_CHAIN)));
            } else {
                updatedPluginChain.put(flavor, "");
            }
        }
        if (userPluginChain.containsKey(BLOJSOM_PLUGIN_CHAIN)) {
            updatedPluginChain.put("", BlojsomUtils.arrayOfStringsToString((String[]) userPluginChain.get(BLOJSOM_PLUGIN_CHAIN)));
        }
        context.put(BLOJSOM_PLUGIN_EDIT_BLOG_PLUGINS_MAP, updatedPluginChain);

        // Add the list of available plugins
        context.put(BLOJSOM_PLUGIN_EDIT_BLOG_PLUGINS_AVAILABLE_PLUGINS, _plugins);

        String action = BlojsomUtils.getRequestValue(ACTION_PARAM, httpServletRequest);
        if (BlojsomUtils.checkNullOrBlank(action)) {
            _logger.debug("User did not request edit action");

            httpServletRequest.setAttribute(PAGE_PARAM, ADMIN_ADMINISTRATION_PAGE);
        } else if (PAGE_ACTION.equals(action)) {
            _logger.debug("User requested edit blog plugins page");

            httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_PLUGINS_PAGE);
        } else if (MODIFY_PLUGIN_CHAINS.equals(action)) {
            _logger.debug("User requested modify blog plugins action");

            Map pluginChain = user.getPluginChain();
            Map pluginChainForWriting = new HashMap();
            Map pluginChainForContext = new HashMap();

            // Iterate over the user's flavors and update the plugin chains
            flavorIterator = user.getFlavors().keySet().iterator();
            String updatedFlavor;
            String[] updatedFlavorArray;
            while (flavorIterator.hasNext()) {
                String flavor = (String) flavorIterator.next();
                updatedFlavor = BlojsomUtils.getRequestValue(flavor + "." + BLOJSOM_PLUGIN_CHAIN, httpServletRequest);
                updatedFlavorArray = BlojsomUtils.parseCommaList(updatedFlavor);
                pluginChain.put(flavor + "." + BLOJSOM_PLUGIN_CHAIN, updatedFlavorArray);
                pluginChainForWriting.put(flavor + "." + BLOJSOM_PLUGIN_CHAIN, updatedFlavor);
                pluginChainForContext.put(flavor, updatedFlavor);
            }

            // Check for the default flavor
            updatedFlavor = BlojsomUtils.getRequestValue("." + BLOJSOM_PLUGIN_CHAIN, httpServletRequest);
            updatedFlavorArray = BlojsomUtils.parseCommaList(updatedFlavor);
            pluginChain.put(BLOJSOM_PLUGIN_CHAIN, updatedFlavorArray);
            pluginChainForWriting.put(BLOJSOM_PLUGIN_CHAIN, updatedFlavor);
            pluginChainForContext.put("", updatedFlavor);

            // Update the internal plugin chain map for the user
            user.setPluginChain(pluginChain);

            // Write out the updated plugin configuration file
            try {
                writePluginsConfiguration(user.getId(), pluginChainForWriting);
                addOperationResultMessage(context, "Successfully updated plugin configuration");
            } catch (IOException e) {
                _logger.error(e);
                addOperationResultMessage(context, "Unable to update plugin configuration");
            }

            context.put(BLOJSOM_PLUGIN_EDIT_BLOG_PLUGINS_MAP, new TreeMap(pluginChainForContext));
            httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_PLUGINS_PAGE);
        }

        return entries;
    }

    /**
     * Write out the plugin configuration file for a particular user
     *
     * @param userId User ID
     * @param pluginChain Map of the plugin chains for the various flavors
     * @throws java.io.IOException If there is an error writing the plugin configuration file
     */
    private void writePluginsConfiguration(String userId, Map pluginChain) throws IOException {
        File pluginFile = new File(_blojsomConfiguration.getInstallationDirectory() + _blojsomConfiguration.getBaseConfigurationDirectory() + userId + "/" + _pluginConfiguration);
        FileOutputStream fos = new FileOutputStream(pluginFile);
        Properties pluginChainProperties = BlojsomUtils.mapToProperties(pluginChain);
        pluginChainProperties.store(fos, null);
        fos.close();
    }
}
