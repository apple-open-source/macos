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
package org.blojsom.plugin.trackback;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.blog.Blog;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.plugin.BlojsomPlugin;
import org.blojsom.plugin.BlojsomPluginException;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.util.HashMap;
import java.util.Map;

/**
 * Trackback moderation plugin
 *
 * @author David Czarnecki
 * @version $Id: TrackbackModerationPlugin.java,v 1.1.2.1 2005/07/21 04:30:43 johnan Exp $
 * @since blojsom 2.20
 */
public class TrackbackModerationPlugin implements BlojsomPlugin {

    private Log _logger = LogFactory.getLog(TrackbackModerationPlugin.class);

    // blog.properties property
    public static final String TRACKBACK_MODERATION_ENABLED = "trackback-moderation-enabled";

    // Trackback meta-data
    public static final String BLOJSOM_TRACKBACK_MODERATION_PLUGIN_APPROVED = "BLOJSOM_TRACKBACK_MODERATION_PLUGIN_APPROVED";

    /**
     * Construct a new trackback moderation plugin
     */
    public TrackbackModerationPlugin() {
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
        _logger.debug("Initialized trackback moderation plugin");
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
        moderateTrackback(httpServletRequest, httpServletResponse, user, context, entries);

        return entries;
    }

    /**
     * Simple check to see if trackback moderation is enabled
     * <p/>
     * param httpServletRequest  Request
     *
     * @param httpServletResponse Response
     * @param user                {@link org.blojsom.blog.BlogUser} instance
     * @param context             Context
     * @param entries             Blog entries retrieved for the particular request
     * @throws BlojsomPluginException If there is an error in moderating a trackback
     */
    protected void moderateTrackback(HttpServletRequest httpServletRequest, HttpServletResponse httpServletResponse, BlogUser user, Map context, BlogEntry[] entries) throws BlojsomPluginException {
        Blog blog = user.getBlog();

        if ("true".equalsIgnoreCase(blog.getBlogProperty(TRACKBACK_MODERATION_ENABLED))) {
            if ("y".equalsIgnoreCase(httpServletRequest.getParameter(TrackbackPlugin.TRACKBACK_PARAM)) && user.getBlog().getBlogTrackbacksEnabled().booleanValue()) {
                Map trackbackMetaData;
                if (context.containsKey(TrackbackPlugin.BLOJSOM_PLUGIN_TRACKBACK_METADATA)) {
                    trackbackMetaData = (Map) context.get(TrackbackPlugin.BLOJSOM_PLUGIN_TRACKBACK_METADATA);
                } else {
                    trackbackMetaData = new HashMap();
                }
                
                trackbackMetaData.put(BLOJSOM_TRACKBACK_MODERATION_PLUGIN_APPROVED, Boolean.FALSE.toString());
                context.put(TrackbackPlugin.BLOJSOM_PLUGIN_TRACKBACK_METADATA, trackbackMetaData);

                _logger.debug("Marking trackback as requiring approval");
            }
        }
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