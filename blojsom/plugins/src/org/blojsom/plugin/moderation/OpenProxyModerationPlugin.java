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
package org.blojsom.plugin.moderation;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.plugin.BlojsomPlugin;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.plugin.comment.CommentPlugin;
import org.blojsom.plugin.comment.CommentModerationPlugin;
import org.blojsom.plugin.trackback.TrackbackModerationPlugin;
import org.blojsom.plugin.trackback.TrackbackPlugin;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.util.HashMap;
import java.util.Map;

/**
 * Open proxy check plugin for comments and trackbacks. This plugin queries the <a href="http://dsbl.org/main">Distributed
 * Sender Blackhole List</a> if a comment or trackback is submitted. If the IP address of the requesting
 * host is on the blacklist, the comment or trackback is marked for moderation if moderation is enabled,
 * otherwise it is destroyed.
 * <p/>
 * This plugin can work in conjunction with other moderation plugins as it looks for the comment or
 * trackback metadata.
 *
 * @author David Czarnecki
 * @version $Id: OpenProxyModerationPlugin.java,v 1.1.2.1 2005/07/21 04:30:35 johnan Exp $
 * @since blojsom 2.22
 */
public class OpenProxyModerationPlugin implements BlojsomPlugin {

    private Log _logger = LogFactory.getLog(OpenProxyModerationPlugin.class);
    public static final String BLOJSOM_OPEN_PROXY_PLUGIN_MESSAGE = "BLOJSOM_OPEN_PROXY_PLUGIN_MESSAGE";

    /**
     * Create a new instance of the open proxy comment check plugin
     */
    public OpenProxyModerationPlugin() {
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
        _logger.debug("Initialized open proxy moderation plugin");
    }

    /**
     * Simple check to see if comment moderation is enabled
     * <p/>
     *
     * @param httpServletRequest  Request
     * @param httpServletResponse Response
     * @param user                {@link org.blojsom.blog.BlogUser} instance
     * @param context             Context
     * @param entries             Blog entries retrieved for the particular request
     * @throws org.blojsom.plugin.BlojsomPluginException
     *          If there is an error in moderating a comment
     */
    protected void checkOpenProxy(HttpServletRequest httpServletRequest, HttpServletResponse httpServletResponse, BlogUser user, Map context, BlogEntry[] entries) throws BlojsomPluginException {
        String remoteIP = httpServletRequest.getRemoteAddr();
        String[] ipAddress = remoteIP.split("\\.");
        StringBuffer reversedAddress = new StringBuffer();
        reversedAddress.append(ipAddress[3]).append(".").append(ipAddress[2]).append(".").append(ipAddress[1]).append(".").append(ipAddress[0]);

        try {
            InetAddress inetAddress = InetAddress.getByName(reversedAddress + ".list.dsbl.org");

            // Check for a comment submission
            if ("y".equalsIgnoreCase(httpServletRequest.getParameter(CommentPlugin.COMMENT_PARAM)) && user.getBlog().getBlogCommentsEnabled().booleanValue()) {
                Map commentMetaData;
                if (context.containsKey(CommentPlugin.BLOJSOM_PLUGIN_COMMENT_METADATA)) {
                    commentMetaData = (Map) context.get(CommentPlugin.BLOJSOM_PLUGIN_COMMENT_METADATA);
                } else {
                    commentMetaData = new HashMap();
                }

                // If comment moderation is enabled, comment is marked for moderation, otherwise it's destroyed
                if ("true".equalsIgnoreCase(user.getBlog().getBlogProperty(CommentModerationPlugin.COMMENT_MODERATION_ENABLED))) {
                    commentMetaData.put(CommentModerationPlugin.BLOJSOM_COMMENT_MODERATION_PLUGIN_APPROVED, Boolean.FALSE.toString());
                    context.put(CommentPlugin.BLOJSOM_PLUGIN_COMMENT_METADATA, commentMetaData);
                    _logger.debug("Marking comment as requiring approval");
                } else {
                    commentMetaData.put(CommentPlugin.BLOJSOM_PLUGIN_COMMENT_METADATA_DESTROY, Boolean.TRUE.toString());
                    _logger.debug("Marking comment to be destroyed");
                }

                context.put(CommentPlugin.BLOJSOM_PLUGIN_COMMENT_METADATA, commentMetaData);
            }

            // Check for a trackback submission
            if ("y".equalsIgnoreCase(httpServletRequest.getParameter(TrackbackPlugin.TRACKBACK_PARAM)) && user.getBlog().getBlogTrackbacksEnabled().booleanValue()) {
                Map trackbackMetaData;
                if (context.containsKey(TrackbackPlugin.BLOJSOM_PLUGIN_TRACKBACK_METADATA)) {
                    trackbackMetaData = (Map) context.get(TrackbackPlugin.BLOJSOM_PLUGIN_TRACKBACK_METADATA);
                } else {
                    trackbackMetaData = new HashMap();
                }

                // If trackback moderation is enabled, trackback is marked for moderation, otherwise it's destroyed
                if ("true".equalsIgnoreCase(user.getBlog().getBlogProperty(TrackbackModerationPlugin.TRACKBACK_MODERATION_ENABLED))) {
                    trackbackMetaData.put(TrackbackModerationPlugin.BLOJSOM_TRACKBACK_MODERATION_PLUGIN_APPROVED, Boolean.FALSE.toString());
                    _logger.debug("Marking trackback as requiring approval");
                } else {
                    trackbackMetaData.put(TrackbackPlugin.BLOJSOM_PLUGIN_TRACKBACK_METADATA_DESTROY, Boolean.TRUE.toString());
                    _logger.debug("Marking trackback to be destroyed");
                }

                context.put(TrackbackPlugin.BLOJSOM_PLUGIN_TRACKBACK_METADATA, trackbackMetaData);
            }

            _logger.debug("Failed open proxy check for comment/trackback submission for IP: " + inetAddress.getHostAddress() + "/" + inetAddress.getHostName());
            context.put(BLOJSOM_OPEN_PROXY_PLUGIN_MESSAGE, "Failed open proxy authentication check.");
        } catch (UnknownHostException e) {
            // The IP address is unknown to the DSBL server
            return;
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
        checkOpenProxy(httpServletRequest, httpServletResponse, user, context, entries);

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