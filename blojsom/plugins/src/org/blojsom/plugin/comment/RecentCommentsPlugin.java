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
package org.blojsom.plugin.comment;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.blog.BlogComment;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.plugin.BlojsomPlugin;
import org.blojsom.plugin.BlojsomPluginException;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.util.*;

/**
 * RecentCommentsPlugin
 *
 * @author Mark Lussier
 * @version $Id: RecentCommentsPlugin.java,v 1.1.2.1 2005/07/21 04:30:28 johnan Exp $
 * @since Blojsom 2.23
 */
public class RecentCommentsPlugin implements BlojsomPlugin {

    private Log _logger = LogFactory.getLog(RecentCommentsPlugin.class);

    /**
     *
     */
    private static final int DEFAULT_COMMENT_COUNT = 25;

    /**
     *
     */
    private static final String BLOJSOM_RECENT_COMMENTS = "BLOJSOM_RECENT_COMMENTS";

    /**
     *
     */
    private static final String PARAM_COMMEN_COUNT = "count";


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

        String countParam = httpServletRequest.getParameter(PARAM_COMMEN_COUNT);
        int count = DEFAULT_COMMENT_COUNT;
        if (countParam != null && !"".equals(countParam)) {
            try {
                count = Integer.parseInt(countParam);
            } catch (NumberFormatException e) {
                _logger.error("Invalid value for Recent Comments count param, using default");
                count = DEFAULT_COMMENT_COUNT;
            }
        }

        List commentsList = new ArrayList(100);
        for (int x = 0; x < entries.length; x++) {
            BlogEntry entry = entries[x];
            BlogComment[] comments = entry.getCommentsAsArray();

            if (comments != null && comments.length > 0) {
                for (int y = 0; y < comments.length; y++) {
                    BlogComment comment = comments[y];
                    commentsList.add(comment);
                }
            }
        }

        // Sort and Chop
        if (commentsList.size() > 0) {
            Collections.sort(commentsList, COMMENT_DATE_COMPARATOR);
            if (commentsList.size() > count) {
                commentsList = commentsList.subList(0, count - 1);
            }

            context.put(BLOJSOM_RECENT_COMMENTS, commentsList);
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


    public static final Comparator COMMENT_DATE_COMPARATOR = new Comparator() {
        public int compare(Object o1, Object o2) {
            BlogComment f1;
            BlogComment f2;

            f1 = (BlogComment) o1;
            f2 = (BlogComment) o2;

            if (f1.getCommentDate().before(f2.getCommentDate())) {
                return 1;
            } else {
                return -1;
            }
        }
    };
}
