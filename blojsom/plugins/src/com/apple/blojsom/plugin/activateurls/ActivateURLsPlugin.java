/**
 * Contains:   Inline administration plug-in for blojsom.
 * Written by: John Anderson (for addtl writers check CVS comments).
 * Copyright:  © 2004 Apple Computer, Inc., all rights reserved.
 * Note:       When editing this file set PB to "Editor uses tabs/width=4".
 *
 * $Id: ActivateURLsPlugin.java,v 1.4 2005/03/01 00:44:22 johnan Exp $
 */ 
package com.apple.blojsom.plugin.activateurls;

import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.plugin.BlojsomPlugin;
import org.blojsom.plugin.BlojsomPluginException;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.util.Map;

/**
 * Convert Line Breaks plug-in
 *
 * @author John Anderson
 * @version $Id: ActivateURLsPlugin.java,v 1.4 2005/03/01 00:44:22 johnan Exp $
 */

public class ActivateURLsPlugin implements BlojsomPlugin {

	// constants
	private static final String SEARCH_REGEX = "(^|[^'\"])((http|https)://[^> \t\r\n\"]+)";
	private static final String REPLACE_REGEX = "$1<a href=\"$2\" target=\"_blank\">$2</a>";
	private static final String NEW_WIN_SEARCH_REGEX = "(^|[^'\"])((feed|rdar|ftp|mailto|afp|goaim)://[^> \t\r\n\"]+)";
	private static final String NEW_WIN_REPLACE_REGEX = "$1<a href=\"$2\">$2</a>";

    /**
     * Default constructor.
     */
    public ActivateURLsPlugin() {
    }
	
    /**
     * Initialize this plugin. This method only called when the plugin is instantiated.
     *
     * @param servletConfig Servlet config object for the plugin to retrieve any initialization parameters
     * @param blojsomConfiguration {@link org.blojsom.blog.BlojsomConfiguration} information
     * @throws BlojsomPluginException If there is an error initializing the plugin
     */
    public void init(ServletConfig servletConfig, BlojsomConfiguration blojsomConfiguration) throws BlojsomPluginException {
    }

    /**
     * Process the blog entries
     *
     * @param httpServletRequest Request
     * @param httpServletResponse Response
     * @param user {@link BlogUser} instance
     * @param context Context
     * @param entries Blog entries retrieved for the particular request
     * @return Modified set of blog entries
     * @throws org.blojsom.plugin.BlojsomPluginException If there is an error processing the blog entries
     */
    public BlogEntry[] process(HttpServletRequest httpServletRequest, HttpServletResponse httpServletResponse, BlogUser user, Map context, BlogEntry[] entries) throws BlojsomPluginException {
        for (int i = 0; i < entries.length; i++) {
            BlogEntry entry = entries[i];

            entry.setDescription(entry.getDescription().replaceAll(SEARCH_REGEX, REPLACE_REGEX));
            entry.setDescription(entry.getDescription().replaceAll(NEW_WIN_SEARCH_REGEX, NEW_WIN_REPLACE_REGEX));
        }
		
		return entries;
	}
	
    /**
     * Perform any cleanup for the plugin. Called after {@link #process}.
     *
     * @throws org.blojsom.plugin.BlojsomPluginException If there is an error performing cleanup for this plugin
     */
    public void cleanup() throws BlojsomPluginException {
    }

    /**
     * Called when BlojsomServlet is taken out of service
     *
     * @throws org.blojsom.plugin.BlojsomPluginException If there is an error in finalizing this plugin
     */
    public void destroy() throws BlojsomPluginException {
    }
}
