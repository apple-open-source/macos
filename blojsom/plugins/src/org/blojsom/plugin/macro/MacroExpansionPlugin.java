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
package org.blojsom.plugin.macro;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.plugin.BlojsomPlugin;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.util.BlojsomProperties;
import org.blojsom.util.BlojsomUtils;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.IOException;
import java.io.InputStream;
import java.util.HashMap;
import java.util.Map;
import java.util.Properties;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Macro Expansion Plugin
 *
 * @author Mark Lussier
 * @version $Id: MacroExpansionPlugin.java,v 1.2 2004/08/27 01:06:38 whitmore Exp $
 */
public class MacroExpansionPlugin implements BlojsomPlugin {

    public static final String BLOG_MACRO_CONFIGURATION_IP = "plugin-macros-expansion";

    private Log _logger = LogFactory.getLog(MacroExpansionPlugin.class);
    private BlojsomConfiguration _blojsomConfiguration;
    private ServletConfig _servletConfig;
    private String _macroConfiguration;

    /**
     * Regular expression to identify macros as $MACRO$ and DOES NOT ignore escaped $'s
     */
    private static final String MACRO_REGEX = "(\\$[^\\$]*\\$)";

    /**
     * Default constructor. Compiles the macro regular expression pattern, $MACRO$
     */
    public MacroExpansionPlugin() {
    }

    /**
     * Initialize this plugin. This method only called when the plugin is instantiated.
     *
     * @param servletConfig        Servlet config object for the plugin to retrieve any initialization parameters
     * @param blojsomConfiguration {@link BlojsomConfiguration} information
     * @throws BlojsomPluginException If there is an error initializing the plugin
     */
    public void init(ServletConfig servletConfig, BlojsomConfiguration blojsomConfiguration) throws BlojsomPluginException {
        _blojsomConfiguration = blojsomConfiguration;
        _servletConfig = servletConfig;
        _macroConfiguration = servletConfig.getInitParameter(BLOG_MACRO_CONFIGURATION_IP);
        if (BlojsomUtils.checkNullOrBlank(_macroConfiguration)) {
            throw new BlojsomPluginException("No value given for: " + BLOG_MACRO_CONFIGURATION_IP + " configuration parameter");
        }
    }

    /**
     * Expand macro tokens in an entry
     *
     * @param content Entry to process
     * @param macros  Macros to expand in the content
     * @return The macro expanded string
     */
    private String replaceMacros(String content, Map macros) {
        if (BlojsomUtils.checkNullOrBlank(content)) {
            return content;
        }

        Pattern macroPattern = Pattern.compile(MACRO_REGEX);
        Matcher matcher = macroPattern.matcher(content);

        while (matcher.find()) {
            String token = matcher.group();
            String macro = token.substring(1, token.length() - 1);
            if (macros.containsKey(macro)) {
                content = BlojsomUtils.replace(content, token, (String) macros.get(macro));
            }
        }

        return content;
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
        Map macros = new HashMap();
        Properties macroProperties = new BlojsomProperties();
        String configurationFile = _blojsomConfiguration.getBaseConfigurationDirectory() + userId + '/' + macroConfiguration;
        InputStream is = _servletConfig.getServletContext().getResourceAsStream(configurationFile);
        if (is == null) {
            _logger.info("No macro configuration file found: " + configurationFile);
        } else {
            try {
                macroProperties.load(is);
                is.close();
                macros = BlojsomUtils.propertiesToMap(macroProperties);
            } catch (IOException e) {
                _logger.error(e);
                throw new BlojsomPluginException(e);
            }
        }

        return macros;
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
        String userId = user.getId();
        Map macros = readMacros(userId, _macroConfiguration);

        for (int i = 0; i < entries.length; i++) {
            BlogEntry entry = entries[i];
            entry.setTitle(replaceMacros(entry.getTitle(), macros));
            entry.setDescription(replaceMacros(entry.getDescription(), macros));
        }

        return entries;
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


