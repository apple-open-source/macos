/**
 * Contains:   Apple Authorization plug-in for blojsom.
 * Written by: Doug Whitmore (for addtl writers check CVS comments).
 * Copyright:  © 2004 Apple Computer, Inc., all rights reserved.
 * Note:       When editing this file set XCode to "Editor uses tabs/width=4".
 *
 */ 
 
package com.apple.blojsom.util;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.BlojsomException;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.Blog;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.blog.BlojsomConfigurationException;
import org.blojsom.util.BlojsomConstants;
import org.blojsom.util.BlojsomUtils;
import org.blojsom.authorization.AuthorizationProvider;
import com.apple.blojsom.util.BlojsomAppleUtils;

import javax.servlet.ServletConfig;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStreamWriter;
import java.util.Map;
import java.util.Properties;


/**
 * AppleBlojsomAuthorizationProvider
 *
 * @author Doug Whitmore
 * 
 */

public class AppleBlojsomAuthorizationProvider implements AuthorizationProvider, BlojsomConstants
{
	
	private Log _logger = LogFactory.getLog(AppleBlojsomAuthorizationProvider.class);

    private ServletConfig _servletConfig;
    private String _baseConfigurationDirectory;
	private String _installationDirectory;

    /**
     * Default constructor
     */
    public AppleBlojsomAuthorizationProvider() 
    {
    }
    
    /**
     * Initialization method for the authorization provider
     *
     * @param servletConfig        ServletConfig for obtaining any initialization parameters
     * @param blojsomConfiguration BlojsomConfiguration for blojsom-specific configuration information
     * @throws org.blojsom.blog.BlojsomConfigurationException
     *          If there is an error initializing the provider
     */
    public void init(ServletConfig servletConfig, BlojsomConfiguration blojsomConfiguration) throws BlojsomConfigurationException 
    {
        _servletConfig = servletConfig;
        _baseConfigurationDirectory = blojsomConfiguration.getBaseConfigurationDirectory();
		_installationDirectory = blojsomConfiguration.getInstallationDirectory();
        _logger.debug("Initialized apple directory services authorization provider");
    }
	
	/**
     * Loads the authentication credentials for a given user
     *
     * @param blogUser {@link BlogUser}
     * @throws BlojsomException If there is an error loading the user's authentication credentials
     */
    public void loadAuthenticationCredentials(BlogUser blogUser) throws BlojsomException 
    {
        String authorizationConfiguration = _servletConfig.getInitParameter(BLOG_AUTHORIZATION_IP);
        if (BlojsomUtils.checkNullOrBlank(authorizationConfiguration)) 
        {
            _logger.error("No authorization configuration file specified");
            throw new BlojsomException("No authorization configuration file specified");
        }

        Properties authorizationProperties;
        InputStream is = _servletConfig.getServletContext().getResourceAsStream(_baseConfigurationDirectory + blogUser.getId() + '/' + authorizationConfiguration);
        authorizationProperties = new Properties();
        try 
        {
            authorizationProperties.load(is);
            Map authorizationMap = BlojsomUtils.propertiesToMap(authorizationProperties);
            blogUser.getBlog().setAuthorization(authorizationMap);
        } catch (IOException e) 
        {
            _logger.error(e);
            throw new BlojsomException(e);
        }

    }

    /**
     * Authorize a username and password for the given {@link BlogUser}
     *
     * @param blogUser             {@link BlogUser}
     * @param authorizationContext {@link Map} to be used to provide other information for authorization. This will
     *                             change depending on the authorization provider. This parameter is not used in this implementation.
     * @param username             Username
     * @param password             Password
     * @throws BlojsomException If there is an error authorizing the username and password
     */
    public void authorize(BlogUser blogUser, Map authorizationContext, String username, String password) throws BlojsomException 
    {
        boolean result = false;
		boolean groupsContainUser = false;
		
        // find out the user's short name (if applicable)
		String shortName = BlojsomAppleUtils.getShortNameFromFullName(username, ".");
		
		// if we didn't find it in the local machine record, look in the search path
		if (shortName.equals(username)) 
		{
			shortName = BlojsomAppleUtils.getShortNameFromFullName(username, "/Search");
		}
		
		if (!(shortName.equals(username))) 
		{
			username = shortName;
		}
		
		_logger.debug("attempting to authorize " + username);
		
		Map aAuthorization = blogUser.getBlog().getAuthorization();
		
		// if we found a _DIRECTORY_SERVICES_GROUP_ key...
		if (aAuthorization.containsKey(DIRECTORY_SERVICES_GROUP)) 
		{
			_logger.debug("attempting to authorize a group for" + username);
			String authGroupName = aAuthorization.get(DIRECTORY_SERVICES_GROUP).toString();
			groupsContainUser = BlojsomAppleUtils.checkGroupMembershipForUser(shortName, authGroupName);
		}
	
		if (BlojsomAppleUtils.checkSACLMembershipForUser(username) && (groupsContainUser || aAuthorization.containsKey(username))) 
		{
			try 
			{
				String [] processArgs = { "/usr/libexec/chkpasswd", username };
				Process checkPasswordProcess = Runtime.getRuntime().exec(processArgs);
				OutputStream passwdStream = checkPasswordProcess.getOutputStream();
				OutputStreamWriter passwdStreamWriter = new OutputStreamWriter(passwdStream);
				passwdStreamWriter.write(password);
				passwdStreamWriter.write("\n");
				passwdStreamWriter.close();
				int resultCode = checkPasswordProcess.waitFor();
				
				_logger.debug("result of checking the password: " + resultCode);
				
				if (resultCode == 0) 
				{
					result = true;
					if (!blogUser.getBlog().getBlogProperty(BLOG_EXISTS).equals("true")) {
						// mark this user's blog as existing...
						blogUser.getBlog().setBlogProperty(BLOG_EXISTS, "true");
						
						// Write out new blog properties
						Properties blogProperties = BlojsomUtils.mapToProperties(blogUser.getBlog().getBlogProperties(), UTF8);
						File propertiesFile = new File(_installationDirectory 
								+ BlojsomUtils.removeInitialSlash(_baseConfigurationDirectory) +
								"/" + blogUser.getId() + "/" + BlojsomConstants.BLOG_DEFAULT_PROPERTIES);

						_logger.debug("Writing blog properties to: " + propertiesFile.toString());
					
						FileOutputStream fos = new FileOutputStream(propertiesFile);
						blogProperties.store(fos, null);
						fos.close();
					}
				}
			} catch (java.io.IOException e) 
			{
				_logger.error(e);
			} catch (java.lang.InterruptedException e) 
			{
				_logger.error(e);
			}
		}
        if (!result) 
        {
            throw new BlojsomException("Authorization failed for blog user: " + blogUser.getId() + " for username: " + username);
        }
    }

}