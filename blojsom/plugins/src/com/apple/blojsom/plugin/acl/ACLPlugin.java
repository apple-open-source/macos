/**
 * Contains:   ACL plug-in for blojsom.
 * Written by: John Anderson (for addtl writers check CVS comments).
 * Copyright:  © 2004 Apple Computer, Inc., all rights reserved.
 * Note:       When editing this file set PB to "Editor uses tabs/width=4".
 *
 * $Id: ACLPlugin.java,v 1.9 2005/03/02 22:58:02 johnan Exp $
 */ 
package com.apple.blojsom.plugin.acl;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.apache.soap.encoding.soapenc.Base64;
import org.blojsom.blog.Blog;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.plugin.BlojsomPlugin;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.util.BlojsomUtils;
import com.apple.blojsom.util.BlojsomAppleUtils;
import org.blojsom.util.BlojsomConstants;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpSession;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.util.Map;
import java.io.*;
import java.util.*;

/**
 * ACL plug-in
 *
 * @author John Anderson
 * @version $Id: ACLPlugin.java,v 1.9 2005/03/02 22:58:02 johnan Exp $
 */

public class ACLPlugin implements BlojsomPlugin, BlojsomConstants {

    protected static final Log _logger = LogFactory.getLog(ACLPlugin.class);

    // Constants from BaseAdminPlugin
    protected static final String BLOJSOM_ADMIN_PLUGIN_AUTHENTICATED_KEY = "org.blojsom.plugin.admin.Authenticated";
    protected static final String BLOJSOM_ADMIN_PLUGIN_USERNAME_KEY = "org.blojsom.plugin.admin.Username";
    protected static final String BLOJSOM_ADMIN_PLUGIN_USERNAME_PARAM = "username";
    protected static final String BLOJSOM_ADMIN_PLUGIN_PASSWORD_PARAM = "password";
    protected static final String ACTION_PARAM = "action";
    protected static final String BLOJSOM_ADMIN_PLUGIN_OPERATION_RESULT = "BLOJSOM_ADMIN_PLUGIN_OPERATION_RESULT";
    
	// Actions
    protected static final String LOGOUT_ACTION = "logout";
	
    // Other constants
    protected static final String BLOJSOM_ACL_PLUGIN_AUTHENTICATED_KEY = "org.blojsom.plugin.admin.acl.Authenticated";
    protected static final String ACL_LOGGED_IN_PROPERTY = "acl_logged_in";
    protected static final String ACL_USERS_FORM_PROPERTY = "acl_users";
    protected static final String ACL_DENIED_PROPERTY = "acl_denied";
    
	// Instance variables
    protected BlojsomConfiguration _blojsomConfiguration;
    protected ServletConfig _servletConfig;
    protected String [] _aclUsers;
    protected String [] _aclGroups;

    /**
     * Default constructor.
     */
    public ACLPlugin() {
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
        _servletConfig = servletConfig;
    }

    /**
     * Adds a message to the context under the <code>BLOJSOM_ADMIN_PLUGIN_OPERATION_RESULT</code> key
     *
     * @param context Context
     * @param message Message to add
     */
    protected void addOperationResultMessage(Map context, String message) {
        context.put(BLOJSOM_ADMIN_PLUGIN_OPERATION_RESULT, message);
    }

    /**
     * Mark a user as authenticated against the ACL.
     *
     * @param blog Blog
     * @param httpSession Session
     * @param username Username
     */
    protected void markUserAsAuthenticated(Blog blog, HttpSession httpSession, String username) {
		httpSession.setAttribute(blog.getBlogURL() + "_" + BLOJSOM_ACL_PLUGIN_AUTHENTICATED_KEY, Boolean.TRUE);
		httpSession.setAttribute(blog.getBlogURL() + "_" + BLOJSOM_ADMIN_PLUGIN_USERNAME_KEY, username);
		_logger.debug("Passed authentication for username: " + username);
    }

    /**
     * Authenticate the user if their authentication session variable is not present
     *
     * @param httpServletRequest Request
     * @param httpServletResponse Response
     * @param context Context
     * @param blog Blog information
     * @return <code>true</code> if the user is authenticated, <code>false</code> otherwise
     */
    protected boolean authenticateUser(HttpServletRequest httpServletRequest, HttpServletResponse httpServletResponse, Map context, Blog blog, String [] aclUsers, String [] aclGroups) {
        BlojsomUtils.setNoCacheControlHeaders(httpServletResponse);
        HttpSession httpSession = httpServletRequest.getSession();

        // Check first to see if someone has requested to logout
        String action = BlojsomUtils.getRequestValue(ACTION_PARAM, httpServletRequest);
        if (action != null && LOGOUT_ACTION.equals(action)) {
            httpSession.removeAttribute(blog.getBlogURL() + "_" + BLOJSOM_ACL_PLUGIN_AUTHENTICATED_KEY);
        }
        
        // if they're authenticated against the admin plug-in then we're good too
        String adminAttributeKey = blog.getBlogURL() + "_" + BLOJSOM_ADMIN_PLUGIN_AUTHENTICATED_KEY;
        if ((httpSession.getAttribute(adminAttributeKey) != null) && (((Boolean)httpSession.getAttribute(adminAttributeKey)).booleanValue())) {
        	return true;
        }

        // Otherwise, check for the authenticated key and if not authenticated, look for a "username" and "password" parameter
        if (httpSession.getAttribute(blog.getBlogURL() + "_" + BLOJSOM_ACL_PLUGIN_AUTHENTICATED_KEY) == null) {
            String username = httpServletRequest.getParameter(BLOJSOM_ADMIN_PLUGIN_USERNAME_PARAM);
            String password = httpServletRequest.getParameter(BLOJSOM_ADMIN_PLUGIN_PASSWORD_PARAM);
            
            // if there's no ACL rules then everybody is allowed
            if (aclUsers.length == 0 && aclGroups.length == 0) {
            	_logger.debug("No ACLs for blog at " + blog.getBlogURL());
            	return true;
            }
            
            // check for HTTP authentication
            String auth = httpServletRequest.getHeader("Authorization");
            
            if (auth != null && auth.toUpperCase().startsWith("BASIC ")) {
            	// get what's after BASIC
            	String userpassEncoded = auth.substring(6);
            	
            	// Decode the password
            	byte [] decodedBytes = Base64.decode(userpassEncoded);
            	
            	if (decodedBytes != null) {
            		String userpassDecoded = new String(decodedBytes);
            		username = userpassDecoded.replaceFirst(":.*$", "");
            		password = userpassDecoded.replaceFirst("^.*:", "");
            	}
            }

            if (username == null || password == null || "".equals(username) || "".equals(password)) {
                return false;
            }
            
            // first, make sure they typed the correct password
            if (!BlojsomAppleUtils.checkUserPassword(username, password)) {
                _logger.debug("Failed authentication for username: " + username);
            	return false;
            }
            
            // now see if they exist in the list of users
            int i;
            for (i = 0; i < aclUsers.length; i++) {
            	if (username.equals(aclUsers[i])) {
            		markUserAsAuthenticated(blog, httpSession, username);
					context.put(ACL_LOGGED_IN_PROPERTY, "true");
            		addOperationResultMessage(context, null);
            		return true;
            	}
            }
            
            for (i = 0; i < aclGroups.length; i++) {
            	if (BlojsomAppleUtils.checkGroupMembershipForUser(username, aclGroups[i])) {
            		markUserAsAuthenticated(blog, httpSession, username);
					context.put(ACL_LOGGED_IN_PROPERTY, "true");
             		addOperationResultMessage(context, null);
           			return true;
            	}
            }
            
            // if we've gotten to this point, they don't belong here
            return false;
            
        } else {
        	context.put(ACL_LOGGED_IN_PROPERTY, "true");
            return ((Boolean) httpSession.getAttribute(blog.getBlogURL() + "_" + BLOJSOM_ACL_PLUGIN_AUTHENTICATED_KEY)).booleanValue();
        }
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
        HttpSession httpSession = httpServletRequest.getSession();
		Blog blog = user.getBlog();
    	
        // Check first to see if someone has requested to logout
        String action = BlojsomUtils.getRequestValue(ACTION_PARAM, httpServletRequest);
        if (action != null && LOGOUT_ACTION.equals(action)) {
            httpSession.removeAttribute(blog.getBlogURL() + "_" + BLOJSOM_ACL_PLUGIN_AUTHENTICATED_KEY);
        }

    	// load the properties (if any)
    	Properties aclProperties = new Properties();
    	String aclConfigurationFile = _blojsomConfiguration.getBaseConfigurationDirectory() + user.getId() + "/acl.properties";
    	InputStream is = _servletConfig.getServletContext().getResourceAsStream(aclConfigurationFile);
    	
    	if (is != null) {
    		try {
				aclProperties.load(is);
				is.close();
			} catch (IOException e) {
				_logger.error(e);
			}
    	}
    	
    	String [] aclUsers = new String[0];
    	String [] aclGroups = new String[0];
    	
    	String aclUsersProperty = aclProperties.getProperty("users");
    	if ((aclUsersProperty != null) && (!"".equals(aclUsersProperty))) {
    		_logger.debug("aclUsersProperty = " + aclUsersProperty);
    		aclUsers = aclUsersProperty.split(", *");
    	}
    	
    	String aclGroupsProperty = aclProperties.getProperty("groups");
    	if ((aclGroupsProperty != null) && (!"".equals(aclGroupsProperty))) {
    		_logger.debug("aclGroupsProperty = " +  aclGroupsProperty);
    		aclGroups = aclGroupsProperty.split(", *");
    	}

        String aclUsersRequestValue = BlojsomUtils.getRequestValue(ACL_USERS_FORM_PROPERTY, httpServletRequest);

        // figure out whether they're authenticated as an admin
        String adminAttributeKey = blog.getBlogURL() + "_" + BLOJSOM_ADMIN_PLUGIN_AUTHENTICATED_KEY;
        if ((httpSession.getAttribute(adminAttributeKey) != null) && (((Boolean)httpSession.getAttribute(adminAttributeKey)).booleanValue())) {
        	if (aclUsersRequestValue != null) {
				String [] newUsersAndGroups = new String[0];
				aclUsers = new String[0];
				aclGroups = new String[0];
				
				//_logger.debug("got new ACL users and groups string: " + aclUsersRequestValue);
				
				if (!"".equals(aclUsersRequestValue)) {
					newUsersAndGroups = aclUsersRequestValue.split("[\r\n]+");
				}
				
				for (int i = 0; i < newUsersAndGroups.length; i++) {
					String currentUserOrGroup = newUsersAndGroups[i];
					if (BlojsomAppleUtils.doesUserExistInDS(currentUserOrGroup, ".") || BlojsomAppleUtils.doesUserExistInDS(currentUserOrGroup, "/Search")) {
						aclUsers = BlojsomAppleUtils.addSlotToStringArray(aclUsers);
						aclUsers[aclUsers.length-1] = currentUserOrGroup;
					}
					else if (BlojsomAppleUtils.doesGroupExistInDS(currentUserOrGroup, ".") || BlojsomAppleUtils.doesGroupExistInDS(currentUserOrGroup, "/Search")) {
						aclGroups = BlojsomAppleUtils.addSlotToStringArray(aclGroups);
						aclGroups[aclGroups.length-1] = currentUserOrGroup;
					}
				}
				
				aclProperties.setProperty("users", BlojsomUtils.arrayOfStringsToString(aclUsers));
				aclProperties.setProperty("groups", BlojsomUtils.arrayOfStringsToString(aclGroups));
				
				File propertiesFile = new File(_blojsomConfiguration.getInstallationDirectory()
						+ BlojsomUtils.removeInitialSlash(_blojsomConfiguration.getBaseConfigurationDirectory()) +
						"/" + user.getId() + "/acl.properties");
						
				_logger.debug("Writing ACL properties to: " + propertiesFile.toString());
				
				try {
					FileOutputStream fos = new FileOutputStream(propertiesFile);
					aclProperties.store(fos, null);
					fos.close();
				} catch (IOException e) {
					_logger.error(e);
				}
			}
			
			String [] usersAndGroups = new String [aclUsers.length + aclGroups.length];
			System.arraycopy(aclGroups, 0, usersAndGroups, 0, aclGroups.length);
			System.arraycopy(aclUsers, 0, usersAndGroups, aclGroups.length, aclUsers.length);
			
			StringBuffer result = new StringBuffer();
			String element;
			for (int i = 0; i < usersAndGroups.length; i++) {
				element = usersAndGroups[i];
				result.append(element);
				if (i < usersAndGroups.length - 1) {
					result.append("\n");
				}
			}
		
			context.put(ACL_USERS_FORM_PROPERTY, result.toString());
        }
        
        if (!authenticateUser(httpServletRequest, httpServletResponse, context, blog, aclUsers, aclGroups)) {
        	entries = new BlogEntry[0];
        	context.put(ACL_DENIED_PROPERTY, "true");
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
