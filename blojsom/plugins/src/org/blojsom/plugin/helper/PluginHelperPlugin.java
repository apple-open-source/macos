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
package org.blojsom.plugin.helper;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.plugin.BlojsomPlugin;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.util.BlojsomConstants;
import org.blojsom.util.BlojsomUtils;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.util.Map;

/**
 * Plugin helper plugin allows you to execute plugins per name or flavor from your templates.
 *
 * @author David Czarnecki
 * @since blojsom 2.24
 * @version $Id: PluginHelperPlugin.java,v 1.1.2.1 2005/07/21 04:30:31 johnan Exp $
 */
public class PluginHelperPlugin implements BlojsomPlugin, BlojsomConstants {

    private Log _logger = LogFactory.getLog(PluginHelperPlugin.class);

    private static final String BLOJSOM_PLUGINS = "BLOJSOM_PLUGINS";
    private static final String BLOJSOM_PLUGIN_PLUGIN_HELPER = "BLOJSOM_PLUGIN_PLUGIN_HELPER";

    /**
     * Create a new instance of the plugin helper plugin
     */
    public PluginHelperPlugin() {
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
        context.put(BLOJSOM_PLUGIN_PLUGIN_HELPER, new PluginHelper(httpServletRequest, httpServletResponse, context, user));

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
    }

    /**
     * Plugin helper defines methods to execute plugins by name or by flavor
     */
    public class PluginHelper {

        HttpServletRequest _request;
        HttpServletResponse _response;
        Map _context;
        BlogUser _blog;

        /**
         * Create a new plugin helper instance
         *
         * @param request {@link HttpServletRequest}
         * @param response {@link HttpServletResponse}
         * @param context Context information
         * @param blog {@link BlogUser} information
         */
        public PluginHelper(HttpServletRequest request, HttpServletResponse response, Map context, BlogUser blog) {
            _request = request;
            _response = response;
            _context = context;
            _blog = blog;
        }

        /**
         * Process a single entry for a named plugin
         *
         * @param blogEntry {@link BlogEntry}
         * @param plugin Name of plugin
         */
        public void processEntryForPlugin(BlogEntry blogEntry, String plugin) {
            processEntriesForPlugin(new BlogEntry[]{blogEntry}, plugin);
        }

        /**
         * Process a single entry for the plugins in the plugin chain for the specified flavor
         *
         * @param blogEntry {@link BlogEntry}
         * @param flavor Flavor
         */
        public void processEntryForFlavor(BlogEntry blogEntry, String flavor) {
            processEntriesForFlavor(new BlogEntry[]{blogEntry}, flavor);
        }

        /**
         * Process a set of entries for a named plugin
         *
         * @param entries Array of {@link BlogEntry}
         * @param plugin Name of plguin
         */
        public void processEntriesForPlugin(BlogEntry[] entries, String plugin) {
            process(entries, plugin, null);
        }

        /**
         * Process a set of entries for the plugins in the plugin chain for the specified flavor
         *
         * @param entries Array of {@link BlogEntry}
         * @param flavor Flavor
         */
        public void processEntriesForFlavor(BlogEntry[] entries, String flavor) {
            process(entries, null, flavor);
        }

        /**
         * Executes the plugin(s) on the entries
         *
         * @param entries Array of {@link BlogEntry}
         * @param plugin Plugin name, <code>null</code> if using flavor
         * @param flavor Flavor name, <code>null</code> if using specific plugin name
         */
        protected void process(BlogEntry[] entries, String plugin, String flavor) {
            Map plugins = (Map) _context.get(BLOJSOM_PLUGINS);
            if (plugins == null) {
                return;
            }

            String[] pluginChain = null;
            Map pluginChainMap = _blog.getPluginChain();

            if (!BlojsomUtils.checkNullOrBlank(plugin)) {
                pluginChain = new String[]{plugin};
            } else {
                String pluginChainMapKey = flavor + '.' + BLOJSOM_PLUGIN_CHAIN;
                String[] pluginChainValue = (String[]) pluginChainMap.get(pluginChainMapKey);
                if (pluginChainValue != null && pluginChainValue.length > 0) {
                    pluginChain = (String[]) pluginChainMap.get(pluginChainMapKey);
                } else {
                    pluginChain = (String[]) pluginChainMap.get(BLOJSOM_PLUGIN_CHAIN);
                }
            }

            // Invoke the plugins in the order in which they were specified
            if ((entries != null) && (pluginChain != null) && (!"".equals(pluginChain))) {
                for (int i = 0; i < pluginChain.length; i++) {
                    String pluginToExecute = pluginChain[i];
                    if (plugins.containsKey(pluginToExecute)) {
                        BlojsomPlugin blojsomPlugin = (BlojsomPlugin) plugins.get(pluginToExecute);
                        _logger.debug("blojsom plugin execution: " + blojsomPlugin.getClass().getName());
                        try {
                            entries = blojsomPlugin.process(_request, _response, _blog, _context, entries);
                            blojsomPlugin.cleanup();
                        } catch (BlojsomPluginException e) {
                            _logger.error(e);
                        }
                    } else {
                        _logger.error("No plugin loaded for: " + pluginToExecute);
                    }
                }
            }
        }
    }
}