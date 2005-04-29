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
package org.blojsom.plugin.markdown;

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
import java.io.*;
import java.util.Map;

/**
 * MarkdownPlugin
 * <p/>
 * To use the Markdown plugin, you will need to download the Markdown tool from
 * <a href="http://daringfireball.net/projects/markdown/">John Gruber's Markdown site</a>.
 *
 * @author David Czarnecki
 * @version $Id: MarkdownPlugin.java,v 1.2 2004/08/27 01:06:38 whitmore Exp $
 * @since blojsom 2.14
 */
public class MarkdownPlugin implements BlojsomPlugin, BlojsomConstants {

    private transient Log _logger = LogFactory.getLog(MarkdownPlugin.class);

    /**
     * Metadata key to identify a Markdown post
     */
    private static final String METADATA_RUN_MARKDOWN = "run-markdown";

    /**
     * Extension of Markdown post
     */
    private static final String MARKDOWN_EXTENSION = ".markdown";

    /**
     * Initialization parameter for the command to start a Markdown session
     */
    private static final String PLUGIN_MARKDOWN_EXECUTION_IP = "plugin-markdown-execution";

    private String _markdownExecution;

    /**
     * Initialize this plugin. This method only called when the plugin is instantiated.
     *
     * @param servletConfig        Servlet config object for the plugin to retrieve any initialization parameters
     * @param blojsomConfiguration {@link org.blojsom.blog.BlojsomConfiguration} information
     * @throws org.blojsom.plugin.BlojsomPluginException
     *          If there is an error initializing the plugin
     */
    public void init(ServletConfig servletConfig, BlojsomConfiguration blojsomConfiguration) throws BlojsomPluginException {
        _markdownExecution = servletConfig.getInitParameter(PLUGIN_MARKDOWN_EXECUTION_IP);

        if (BlojsomUtils.checkNullOrBlank(_markdownExecution)) {
            _logger.error("No Markdown execution string provided. Use initialization parameter: " + PLUGIN_MARKDOWN_EXECUTION_IP);
        }
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
        if (!BlojsomUtils.checkNullOrBlank(_markdownExecution)) {
            for (int i = 0; i < entries.length; i++) {
                BlogEntry entry = entries[i];
                String markdownExecution = user.getBlog().getBlogProperty(PLUGIN_MARKDOWN_EXECUTION_IP);

                if ((entry.getPermalink().endsWith(MARKDOWN_EXTENSION) || BlojsomUtils.checkMapForKey(entry.getMetaData(), METADATA_RUN_MARKDOWN)) && !BlojsomUtils.checkNullOrBlank(markdownExecution)) {
                    _logger.debug("Markdown processing: " + entry.getTitle());
                    try {
                        Process process = Runtime.getRuntime().exec(markdownExecution);
                        BufferedWriter bw = new BufferedWriter(new OutputStreamWriter(process.getOutputStream(), UTF8));
                        BufferedReader br = new BufferedReader(new InputStreamReader(process.getInputStream(), UTF8));
                        bw.write(entry.getDescription());
                        bw.close();
                        String input;
                        StringBuffer collectedDescription = new StringBuffer();
                        while ((input = br.readLine()) != null) {
                            collectedDescription.append(input).append(BlojsomConstants.LINE_SEPARATOR);
                        }
                        entry.setDescription(collectedDescription.toString());
                        br.close();
                    } catch (IOException e) {
                        _logger.error(e);
                    }
                }
            }
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