/**
 * Contains:   Inline administration plug-in for blojsom.
 * Written by: John Anderson (for addtl writers check CVS comments).
 * Copyright:  © 2004 Apple Computer, Inc., all rights reserved.
 * Note:       When editing this file set PB to "Editor uses tabs/width=4".
 *
 * $Id: UserDataPlugin.java,v 1.1 2005/01/27 01:46:49 johnan Exp $
 */ 
package com.apple.blojsom.plugin.userdata;

import com.apple.blojsom.util.BlojsomAppleUtils;
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
 * @version $Id: UserDataPlugin.java,v 1.1 2005/01/27 01:46:49 johnan Exp $
 */

public class UserDataPlugin implements BlojsomPlugin {

	protected static final String USER_DATA_PLUGIN = "USER_DATA_PLUGIN";
    protected BlojsomConfiguration _blojsomConfiguration;

    /**
     * Default constructor.
     */
    public UserDataPlugin() {
    }
	
    /**
     * Initialize this plugin. This method only called when the plugin is instantiated.
     *
     * @param servletConfig Servlet config object for the plugin to retrieve any initialization parameters
     * @param blojsomConfiguration {@link org.blojsom.blog.BlojsomConfiguration} information
     * @throws BlojsomPluginException If there is an error initializing the plugin
     */
    public void init(ServletConfig servletConfig, BlojsomConfiguration blojsomConfiguration) throws BlojsomPluginException {
        _blojsomConfiguration = blojsomConfiguration;
    }

    /**
     * Convert a short username to a long one.
     *
     * @param username The short username.
     */
 	public String getFullNameFromShortName(String username) {
 		String fullName = null;
 		BlogUser user = (BlogUser)_blojsomConfiguration.getBlogUsers().get(username);
 		
 		if (user != null) {
 			fullName = user.getBlog().getBlogOwner();
 			
 			if ("".equals(fullName)) {
 				fullName = null;
 			}
 		}
 		if (fullName == null) {
 			fullName = BlojsomAppleUtils.getFullNameFromShortName(username, ".");
 			
 			if (fullName.equals(username)) {
 				fullName = BlojsomAppleUtils.getFullNameFromShortName(username, "/Search");
 			}
 		}
 		
 		return fullName;
 	}
 	
    /**
     * Given a short username, return their email address.
     *
     * @param username The short username.
     */
	public String getEmailAddressFromShortName(String username) {
		String emailAddress = "";
 		BlogUser user = (BlogUser)_blojsomConfiguration.getBlogUsers().get(username);
		
		if (user != null) {
			emailAddress = user.getBlog().getBlogOwnerEmail();
		}
		
		return emailAddress;
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
	
		// add ourselves to the context (for converting user name)
		context.put(USER_DATA_PLUGIN, this);
		
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
