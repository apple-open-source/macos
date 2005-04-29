/**
 * Contains:   ACL plug-in for blojsom.
 * Written by: John Anderson (for addtl writers check CVS comments).
 * Copyright:  © 2004 Apple Computer, Inc., all rights reserved.
 * Note:       When editing this file set PB to "Editor uses tabs/width=4".
 *
 * $Id: HTTPACLPlugin.java,v 1.2 2004/10/19 19:42:30 johnan Exp $
 */ 
package com.apple.blojsom.plugin.acl;

import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.plugin.BlojsomPlugin;
import org.blojsom.plugin.BlojsomPluginException;
import com.apple.blojsom.plugin.acl.ACLPlugin;

import javax.servlet.ServletConfig;
import javax.servlet.*;
import javax.servlet.http.*;
import java.util.Map;

/**
 * HTTP-based ACL plug-in
 *
 * @author John Anderson
 * @version $Id: HTTPACLPlugin.java,v 1.2 2004/10/19 19:42:30 johnan Exp $
 */

public class HTTPACLPlugin extends ACLPlugin {

    /**
     * Default constructor.
     */
    public HTTPACLPlugin() {
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
    	entries = super.process(httpServletRequest, httpServletResponse, user, context, entries);
    	
    	if (context.get(ACL_DENIED_PROPERTY) != null && "true".equals(context.get(ACL_DENIED_PROPERTY))) {
    		httpServletResponse.setHeader("WWW-Authenticate", "BASIC realm=\"/weblog/" + user.getId() + "\"");
    		try {
    			httpServletResponse.sendError(httpServletResponse.SC_UNAUTHORIZED);
    		} catch (java.io.IOException e) {
    			
    		}
    	}
    	
    	return entries;
    }
}
