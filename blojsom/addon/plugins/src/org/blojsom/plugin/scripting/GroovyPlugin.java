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
package org.blojsom.plugin.scripting;

import groovy.lang.GroovyClassLoader;
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
import java.io.File;
import java.util.Map;

/**
 * GroovyPlugin
 *
 * @author David Czarnecki
 * @version $Id: GroovyPlugin.java,v 1.1.2.1 2005/07/21 04:30:21 johnan Exp $
 * @since blojsom 2.14
 */
public class GroovyPlugin implements BlojsomPlugin {

    private Log _logger = LogFactory.getLog(GroovyPlugin.class);

    private static final String GROOVY_SCRIPTS_PARAM = "groovy-scripts";
    private static final String GROOVY_SCRIPTS_COUNTER = "BLOJSOM_GROOVY_PLUGIN_SCRIPTS_COUNTER";
    private static final String GROOVY_SCRIPTS_LIST = "BLOJSOM_GROOVY_PLUGIN_SCRIPTS_LIST";

    private ServletConfig _servletConfig;
    private BlojsomConfiguration _blojsomConfiguration;

    /**
     * Default constructor.
     */
    public GroovyPlugin() {
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
        _servletConfig = servletConfig;
        _blojsomConfiguration = blojsomConfiguration;
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
        String[] scripts = null;

        // Parse the scripts to execute
        if (context.containsKey(GROOVY_SCRIPTS_LIST)) {
            scripts = (String[]) context.get(GROOVY_SCRIPTS_LIST);
        } else {
            String scriptsParam = BlojsomUtils.getRequestValue(GROOVY_SCRIPTS_PARAM, httpServletRequest);
            if (scriptsParam != null) {
                scripts = BlojsomUtils.parseCommaList(scriptsParam);
                context.put(GROOVY_SCRIPTS_LIST, scripts);
            }
        }


        if (scripts == null) {
            _logger.info("No scripts to process");

            return entries;
        }

        // Check for which script we should process if there was more than one
        Integer scriptToProcess;
        if (context.containsKey(GROOVY_SCRIPTS_COUNTER)) {
            scriptToProcess = (Integer) context.get(GROOVY_SCRIPTS_COUNTER);
        } else {
            scriptToProcess = new Integer(0);
        }

        if (scriptToProcess == null || scriptToProcess.intValue() < 0 || scriptToProcess.intValue() > scripts.length) {
            _logger.error("Groovy scripts counter is null or value is out of range");

            return entries;
        }

        File scriptFile = new File(_blojsomConfiguration.getQualifiedResourceDirectory() + user.getId() + "/" + BlojsomUtils.normalize(scripts[scriptToProcess.intValue()]));
        _logger.debug("Processing script file: " + scriptFile.toString());

        GroovyClassLoader groovyClassLoader = new GroovyClassLoader(this.getClass().getClassLoader());
        BlojsomPlugin plugin;
        try {
            Class scriptPluginClazz = groovyClassLoader.parseClass(scriptFile);
            plugin = (BlojsomPlugin) scriptPluginClazz.newInstance();
            plugin.init(_servletConfig, _blojsomConfiguration);
            entries = plugin.process(httpServletRequest, httpServletResponse, user, context, entries);
            plugin.cleanup();
            plugin.destroy();

            // Increment the script to process counter
            scriptToProcess = new Integer(scriptToProcess.intValue() + 1);
            context.put(GROOVY_SCRIPTS_COUNTER, scriptToProcess);
        } catch (Exception e) {
            _logger.error(e);
        }

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
}