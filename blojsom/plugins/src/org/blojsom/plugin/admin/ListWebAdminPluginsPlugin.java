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

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.BlojsomException;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.plugin.BlojsomPlugin;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.util.BlojsomUtils;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.util.Iterator;
import java.util.Map;
import java.util.Properties;
import java.util.TreeMap;

/**
 * List Web Admin Plugins Plugin
 *
 * @author David Czarnecki
 * @version $Id: ListWebAdminPluginsPlugin.java,v 1.1.2.1 2005/07/21 04:30:24 johnan Exp $
 * @since blojsom 2.16
 */
public class ListWebAdminPluginsPlugin extends BaseAdminPlugin {

    private static Log _logger = LogFactory.getLog(ListWebAdminPluginsPlugin.class);

    private static final String BLOJSOM_PLUGIN_WEB_ADMIN_PLUGINS_LIST = "BLOJSOM_PLUGIN_WEB_ADMIN_PLUGINS_LIST";
    private static final String LIST_WEB_ADMIN_PLUGINS_PAGE = "/org/blojsom/plugin/admin/templates/admin-list-web-admin-plugins";

    private Map _plugins;

    /**
     * Default constructor
     */
    public ListWebAdminPluginsPlugin() {
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
        _plugins = new TreeMap();

        String pluginConfiguration = servletConfig.getInitParameter(BLOJSOM_PLUGIN_CONFIGURATION_IP);
        try {
            Properties pluginProperties = BlojsomUtils.loadProperties(servletConfig, _blojsomConfiguration.getBaseConfigurationDirectory() + pluginConfiguration);
            Map plugins = BlojsomUtils.propertiesToMap(pluginProperties);
            Iterator pluginsIterator = plugins.keySet().iterator();
            while (pluginsIterator.hasNext()) {
                String pluginName = (String) pluginsIterator.next();
                if (pluginName.indexOf(BLOJSOM_PLUGIN_CHAIN) != -1) {
                    _plugins.remove(pluginName);
                } else {
                    Class pluginClass = Class.forName((String) plugins.get(pluginName));
                    BlojsomPlugin blojsomPlugin = (BlojsomPlugin) pluginClass.newInstance();
                    if (blojsomPlugin instanceof WebAdminPlugin) {
                        _plugins.put(pluginName, ((WebAdminPlugin) blojsomPlugin).getDisplayName());
                        _logger.debug("Added web admin plugin: " + pluginName);
                    }
                }
            }
        } catch (BlojsomException e) {
            _logger.error(e);
        } catch (ClassNotFoundException e) {
            _logger.error(e);
        } catch (InstantiationException e) {
            _logger.error(e);
        } catch (IllegalAccessException e) {
            _logger.error(e);
        }

        _logger.debug("Initialized list web admin plugins plugin");
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

        if (ADMIN_LOGIN_PAGE.equals(page)) {
            return entries;
        } else {
            httpServletRequest.setAttribute(PAGE_PARAM, LIST_WEB_ADMIN_PLUGINS_PAGE);
            context.put(BLOJSOM_PLUGIN_WEB_ADMIN_PLUGINS_LIST, _plugins);
        }

        return entries;
    }
}