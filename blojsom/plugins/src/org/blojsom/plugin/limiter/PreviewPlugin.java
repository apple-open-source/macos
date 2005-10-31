/**
 * Copyright (c) 2003-2005 , David A. Czarnecki
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
package org.blojsom.plugin.limiter;

import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.plugin.BlojsomPlugin;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.util.BlojsomUtils;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/**
 * Hide preview entries from normal display.
 * Preview entires have the word PREVIEW (by default) at the start of the title.
 * To see the preview items along with normal entries add the parameter
 * preview="true"
 *
 * @author <a href="http://nick.chalko.com"/>Nick Chalko</a>
 * @author David Czarnecki
 * @version $Id: PreviewPlugin.java,v 1.2.2.1 2005/07/21 04:30:32 johnan Exp $
 * @since blojsom 1.9
 */
public class PreviewPlugin implements BlojsomPlugin {

    private static final String PLUGIN_PREVIEW_TITLE_PREFIX_IP = "plugin-preview-title-prefix";
    private static final String PLUGIN_PREVIEW_PREVIEW_PASSWORD_IP = "plugin-preview-preview-password";

    private static final String DEFAULT_TITLE_PREFIX = "PREVIEW";
    private static final String DEFAULT_PREVIEW_PASSWORD = "true";

    /**
     * Request parameter for the "preview"
     */
    private static final String PREVIEW_PARAM = "preview";

    /**
     * Default constructor
     */
    public PreviewPlugin() {
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
     * @throws BlojsomPluginException If there is an error processing the blog entries
     */
    public BlogEntry[] process(HttpServletRequest httpServletRequest,
                               HttpServletResponse httpServletResponse,
                               BlogUser user,
                               Map context,
                               BlogEntry[] entries)
            throws BlojsomPluginException {
        String titlePrefix = user.getBlog().getBlogProperty(PLUGIN_PREVIEW_TITLE_PREFIX_IP);
        if (BlojsomUtils.checkNullOrBlank(titlePrefix)) {
            titlePrefix = DEFAULT_TITLE_PREFIX;
        }

        String previewPassword = user.getBlog().getBlogProperty(PLUGIN_PREVIEW_PREVIEW_PASSWORD_IP);
        if (BlojsomUtils.checkNullOrBlank(previewPassword)) {
            previewPassword = DEFAULT_PREVIEW_PASSWORD;
        }

        // Determine if the user wants to preview posts
        String previewParam = httpServletRequest.getParameter(PREVIEW_PARAM);
        if (previewParam != null && previewParam.equals(previewPassword)) {
            return entries;
        } else {
            List postedEntries = new ArrayList(entries.length);
            for (int i = 0; i < entries.length; i++) {
                BlogEntry entry = entries[i];
                if (entry != null && entry.getTitle() != null && !entry.getTitle().startsWith(titlePrefix)) {
                    postedEntries.add(entry);
                }
            }

            return (BlogEntry[]) postedEntries.toArray(new BlogEntry[postedEntries.size()]);
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
