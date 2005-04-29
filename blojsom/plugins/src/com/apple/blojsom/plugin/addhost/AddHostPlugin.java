/**
 * Contains:   Inline administration plug-in for blojsom.
 * Written by: John Anderson (for addtl writers check CVS comments).
 * Copyright:  © 2004 Apple Computer, Inc., all rights reserved.
 * Note:       When editing this file set PB to "Editor uses tabs/width=4".
 *
 * $Id: AddHostPlugin.java,v 1.4 2005/01/30 18:53:35 johnan Exp $
 */ 
package com.apple.blojsom.plugin.addhost;

import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.blog.Blog;
import org.blojsom.plugin.BlojsomPlugin;
import org.blojsom.plugin.BlojsomPluginException;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.util.Map;
import java.net.URL;
import java.net.MalformedURLException;

/**
 * Add Host plug-in
 *
 * @author John Anderson
 * @version $Id: AddHostPlugin.java,v 1.4 2005/01/30 18:53:35 johnan Exp $
 */

public class AddHostPlugin implements BlojsomPlugin {

	// constants
	private static final String BLOG_BASE_PATH_IP = "blog-base-path";
	private static final String BLOG_SUBPATH = "weblog/";

    /**
     * Default constructor.
     */
    public AddHostPlugin() {
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
	
		// get the blog reference
		Blog blog = user.getBlog();

		// change the base URL to (host)/blojsom
		if (!("".equals(blog.getBlogProperty(BLOG_BASE_PATH_IP)))) {
		
			try {
				URL requestURL = new URL(httpServletRequest.getRequestURL().toString());
				String requestHost = requestURL.getHost();
				int requestPort = requestURL.getPort();
				String requestProtocol = requestURL.getProtocol();
				String blogBaseURL = requestProtocol + "://" + requestHost;
				if (!(((requestProtocol.equals("http") || requestProtocol.equals("feed")) && requestPort == 80)
						|| (requestProtocol.equals("https") && requestPort == 443)
						|| (requestPort <= 0))) {
					blogBaseURL += ":" + requestPort;
				}
				blogBaseURL += blog.getBlogProperty(BLOG_BASE_PATH_IP);
				blog.setBlogBaseURL(blogBaseURL);
				blog.setBlogURL(blogBaseURL + BLOG_SUBPATH + user.getId() + "/");
			} catch (MalformedURLException e) {
			}
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
