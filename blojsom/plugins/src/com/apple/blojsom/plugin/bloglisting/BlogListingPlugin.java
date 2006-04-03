/**
 * Contains:   Default page (blog listing) plug-in for blojsom.
 * Written by: John Anderson (for addtl writers check CVS comments).
 * Copyright:  © 2004-2005 Apple Computer, Inc., all rights reserved.
 * Note:       When editing this file set PB to "Editor uses tabs/width=4".
 *
 * $Id: BlogListingPlugin.java,v 1.6.2.4 2006/01/19 23:08:50 johnan Exp $
 */ 
package com.apple.blojsom.plugin.bloglisting;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.BlojsomException;
import org.blojsom.blog.*;
import org.blojsom.plugin.BlojsomPlugin;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.util.BlojsomUtils;
import org.blojsom.util.BlojsomConstants;
import org.blojsom.fetcher.BlojsomFetcher;
import org.blojsom.fetcher.BlojsomFetcherException;

import com.apple.blojsom.util.BlojsomAppleUtils;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.util.*;

/**
 * Convert Line Breaks plug-in
 *
 * @author John Anderson
 * @version $Id: BlogListingPlugin.java,v 1.6.2.4 2006/01/19 23:08:50 johnan Exp $
 */

public class BlogListingPlugin implements BlojsomPlugin, BlojsomConstants {

    protected static final String ALL_BLOG_USERS_PROPERTY = "ALL_BLOG_USERS";
	protected static final String CREATE_USER_ID_PROPERTY = "createUserID";
    protected static final Log _logger = LogFactory.getLog(BlogListingPlugin.class);

    protected BlojsomConfiguration _blojsomConfiguration;
	protected ServletConfig _servletConfig;

    private BlojsomFetcher _fetcher;
	
    /**
     * Default constructor.
     */
    public BlogListingPlugin() {
    }
	
    /**
     * Initialize this plugin. This method only called when the plugin is instantiated.
     *
     * @param servletConfig Servlet config object for the plugin to retrieve any initialization parameters
     * @param blojsomConfiguration {@link org.blojsom.blog.BlojsomConfiguration} information
     * @throws BlojsomPluginException If there is an error initializing the plugin
     */
    public void init(ServletConfig servletConfig, BlojsomConfiguration blojsomConfiguration) throws BlojsomPluginException {
        String fetcherClassName = blojsomConfiguration.getFetcherClass();
        try {
            Class fetcherClass = Class.forName(fetcherClassName);
            _fetcher = (BlojsomFetcher) fetcherClass.newInstance();
            _fetcher.init(servletConfig, blojsomConfiguration);
            _logger.info("Added blojsom fetcher: " + fetcherClassName);
        } catch (ClassNotFoundException e) {
            _logger.error(e);
            throw new BlojsomPluginException(e);
        } catch (InstantiationException e) {
            _logger.error(e);
            throw new BlojsomPluginException(e);
        } catch (IllegalAccessException e) {
            _logger.error(e);
            throw new BlojsomPluginException(e);
        } catch (BlojsomFetcherException e) {
            _logger.error(e);
            throw new BlojsomPluginException(e);
        }
        _blojsomConfiguration = blojsomConfiguration;
		_servletConfig = servletConfig;
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
    	// if a user is present, try to create a blog for that user
		String userFromPath = BlojsomUtils.getRequestValue(CREATE_USER_ID_PROPERTY, httpServletRequest);
		if (userFromPath != null) {
			_logger.debug("found user in parameter:" + userFromPath);
		} else {
			_logger.debug("getting user from path: " + httpServletRequest.getPathInfo());
			userFromPath = BlojsomUtils.getUserFromPath(httpServletRequest.getPathInfo());
		}
		String defaultUser = (String)_blojsomConfiguration.getBlojsomProperty(BLOJSOM_DEFAULT_USER_IP);
		if (userFromPath != null && !userFromPath.equals(defaultUser)) {
			userFromPath = userFromPath.replaceAll("\\\\", "\\\\\\\\");
			_logger.debug("userFromPath = " + userFromPath);
			String resolvedUserFromPath = BlojsomAppleUtils.validateShortNameAndResolveAliases(userFromPath, "/Search");
			// if this resolves to a different username, try to redirect to it
			if (resolvedUserFromPath != null && userFromPath != null && !userFromPath.equals(resolvedUserFromPath)) {
				_logger.debug("userFromPath resolves to " + resolvedUserFromPath);
				try {
					String redirectURL = _blojsomConfiguration.loadBlog(resolvedUserFromPath).getBlog().getBlogURL();
					_logger.debug("redirecting to " + redirectURL);
					httpServletResponse.sendRedirect(redirectURL);
					return entries;
				} catch (BlojsomException e) { // aliased user doesn't exist
					userFromPath = resolvedUserFromPath;
				} catch (java.io.IOException e) {
					_logger.error(e);
				}
			}
			if (BlojsomAppleUtils.attemptUserBlogCreation(_blojsomConfiguration, _servletConfig, userFromPath)) {
				// created a new blog... redirect to it
				try {
					_logger.debug("attempting to load blog");
					userFromPath = userFromPath.replaceAll("\\\\", "_").replaceAll(" ", "_");
					_logger.debug("userFromPath = " +  userFromPath);
					String redirectURL = _blojsomConfiguration.loadBlog(userFromPath).getBlog().getBlogURL();
					for (int i = 0; i < 20; i++) {
						if (!redirectURL.endsWith("/default/")) {
							_logger.debug("matched other than default in " + redirectURL);
							break;
						}
						try {
							java.lang.Thread.currentThread().sleep(1000);
						} catch (java.lang.InterruptedException e2) {
							_logger.error(e2);
						}
						redirectURL = _blojsomConfiguration.loadBlog(userFromPath).getBlog().getBlogURL();
					}
					_logger.debug("redirecting to " + redirectURL);
					httpServletResponse.sendRedirect(redirectURL);
					return entries;
				} catch (BlojsomException e) {
					_logger.error(e);
				} catch (java.io.IOException e) {
					_logger.error(e);
				}
			}
			context.put("BLOJSOM_ADMIN_PLUGIN_OPERATION_RESULT", " message.loginfailed");
		}
    
        BlojsomUtils.setNoCacheControlHeaders(httpServletResponse);
		Blog blog = user.getBlog();
		String [] blogUserIDs = _blojsomConfiguration.getBlojsomUsers();
		BlogUser [] blogUsers = new BlogUser[blogUserIDs.length-1];
		int i = 0;
		int j = 0;
		
		for (j = 0; j < blogUserIDs.length; j++) {
			String currentKey = blogUserIDs[j];
			try {
				BlogUser currentBlogUser = _blojsomConfiguration.loadBlog(currentKey);
				String blogExistsString = currentBlogUser.getBlog().getBlogProperty(BLOG_EXISTS);
				boolean blogExists = false;
				if (blogExistsString != null)
				{
					blogExists = blogExistsString.equals("true");
				}
					
				if (blogExists && (!"default".equals(currentKey))) {
					blogUsers[i++] = currentBlogUser;
				}
			} catch (BlojsomException e) {
				_logger.error(e);
			}
		}
		
		// make a shrunken array
		BlogUser [] displayBlogUsers = new BlogUser[i];
		System.arraycopy(blogUsers, 0, displayBlogUsers, 0, i);
		
		// sort the array
		java.util.Arrays.sort(displayBlogUsers);
		
		// read in the template every time
		try {
			Properties blogProperties = BlojsomUtils.loadProperties(_servletConfig, _blojsomConfiguration.getBaseConfigurationDirectory() + "default/" + BLOG_DEFAULT_PROPERTIES);
			blog.setBlogProperty(BLOG_DEFAULT_STYLESHEET_IP, blogProperties.getProperty(BLOG_DEFAULT_STYLESHEET_IP));
		} catch (org.blojsom.BlojsomException e) {
			// don't store the hash if we couldn't read the file
		}

		context.put(ALL_BLOG_USERS_PROPERTY, displayBlogUsers);
		entries = new BlogEntry[0];
		
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
