/**
 * Contains:   Editor login plug-in for blojsom.
 * Written by: John Anderson (for addtl writers check CVS comments).
 * Copyright:  © 2004-2005 Apple Computer, Inc., all rights reserved.
 * Note:       When editing this file set PB to "Editor uses tabs/width=4".
 *
 * $Id: BlojsomAppleUtils.java,v 1.27.2.2 2005/08/27 18:23:03 johnan Exp $
 */ 
package com.apple.blojsom.util;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.util.BlojsomConstants;
import org.blojsom.BlojsomException;
import org.blojsom.blog.FileBackedBlogEntry;
import org.blojsom.blog.Blog;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.blog.BlojsomConfigurationException;
import org.blojsom.util.BlojsomUtils;
import org.blojsom.util.BlojsomProperties;

import javax.servlet.ServletConfig;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.BufferedReader;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.util.*;

/**
 * BlojsomUtils
 * 
 * @author John Anderson
 * @version $Id: BlojsomAppleUtils.java,v 1.27.2.2 2005/08/27 18:23:03 johnan Exp $
 */
public class BlojsomAppleUtils implements BlojsomConstants {
	
	// Constants
    private static final String BOOTSTRAP_DIRECTORY_IP = "bootstrap-directory";
    private static final String PLUGIN_ADMIN_EDIT_USERS_IP = "plugin-admin-edit-users";
    private static final String BLOG_HOME_BASE_DIRECTORY_IP = "blog-home-base-directory";
	private static final String DSCL_LEGACY_GROUP_PREFIX = "AAAABBBB-CCCC-DDDD-EEEE-FFFF";
    /**
     * Run a command-line tool and read the results.
	 * @param commandLineArgs An array of arguments, the first of which is the tool's path
	 * @param defaultValue The default value to return.
	 * @return the text sent to stdout.
     */
	public static String getResultFromCommandLineUtility(String [] commandLineArgs, String defaultValue) {
		Log logger = LogFactory.getLog(BlojsomAppleUtils.class);
		String resultString = defaultValue;
		int resultCode = (-1);
		
		try {
			Process commandLineProcess = Runtime.getRuntime().exec(commandLineArgs);
			InputStream commandLineStream = commandLineProcess.getInputStream();
			InputStreamReader commandLineReader = new InputStreamReader(commandLineStream, "UTF-8");
			BufferedReader commandLineBufferedReader = new BufferedReader(commandLineReader);
			String partialCommandLineResponse = commandLineBufferedReader.readLine();
			String commandLineResponse = null;
			
			while (partialCommandLineResponse != null) {
				if (!("".equals(partialCommandLineResponse))) {
					commandLineResponse = partialCommandLineResponse;
				}
				partialCommandLineResponse = commandLineBufferedReader.readLine();
			}
			
			resultCode = commandLineProcess.waitFor();
			if ((resultCode == 0) && (commandLineResponse != null)) {
				resultString = commandLineResponse;
			}
		} catch (java.io.IOException e) {
			logger.error(e);
		} catch (java.lang.InterruptedException e) {
			logger.error(e);
		}
		
		return resultString;
	}
	 
    /**
     * Get settings from Server Admin.
     */
	public static String getServerAdminSetting(String theSetting, String defaultValue) {
		String [] commandLineArgs = { "/usr/sbin/serveradmin", "settings", theSetting };
		String settingKeySearch = theSetting + " = \"";
		String resultString = getResultFromCommandLineUtility(commandLineArgs, defaultValue);
		
		// trim the key off of the beginning
		if (resultString.startsWith(settingKeySearch)) {
			resultString = resultString.substring(settingKeySearch.length());
			// trim the quote off the end
			resultString = resultString.substring(0, (resultString.length()-1));
		}
		
		return resultString;
	}
	 
    /**
     * Try and turn a full name into a short name.
	 * If a matching full name isn't found, returns the input.
     */
	public static String getShortNameFromFullName(String username, String netInfoLoc) {
		// dscl . -search Users realname "John Anderson"
		String [] getShortNameProcessArgs = { "/usr/bin/dscl", netInfoLoc, "-search", "Users", "realname", username };
		String result = getResultFromCommandLineUtility(getShortNameProcessArgs, username);
		
		// if the last line contains a tab...
		if (result.matches(".+\t.+")) {
			// username is before the tab
			String [] splitResult = result.split("\t");
			username = splitResult[0];
		}
		
		return username;
	}

    /**
     * Try and resolve short name aliases.
	 * Returns null if there's no match.
	 * Returns the short name if the username is valid and not an alias.
     */
	public static String validateShortNameAndResolveAliases(String username, String netInfoLoc) {
		// dscl . -search /Users RecordName "aliasedshortname"
		String [] validateShortNameProcessArgs = { "/usr/bin/dscl", netInfoLoc, "-search", "/Users", "RecordName", username };
		String result = getResultFromCommandLineUtility(validateShortNameProcessArgs, "");
		
		// if the last line contains a tab...
		if (result.matches(".+\t.+")) {
			// username is before the tab
			String [] splitResult = result.split("\t");
			return splitResult[0];
		}
		
		return null;
	}
	
    /**
     * Try and turn a full name into a short name.
	 * If a matching full name isn't found, returns the input.
     */
	public static boolean doesUserExistInDS(String username, String netInfoLoc) {
		// dscl . -read /Users/johnan name
		Log logger = LogFactory.getLog(BlojsomAppleUtils.class);
		String nameSearchString = "/dsRecTypeStandard:Users/" + username;
		String [] findNameProcessArgs = { "/usr/bin/dscl", netInfoLoc, "-read", nameSearchString, "dsAttrTypeStandard:RecordName" };
		logger.debug("exec: /usr/bin/dscl " + netInfoLoc + " -read " + nameSearchString + " dsAttrTypeStandard:RecordName");
		String foundName = getResultFromCommandLineUtility(findNameProcessArgs, "");
		logger.debug("got " + foundName);
		return (foundName.matches("dsAttrTypeStandard:RecordName:.+"));
	}
	
	public static boolean doesGroupExistInDS(String groupname, String netInfoLoc) {
		// dscl . -read /Groups/admin PrimaryGroupID
		Log logger = LogFactory.getLog(BlojsomAppleUtils.class);
		String nameSearchString = "/Groups/" + groupname;
		String [] findNameProcessArgs = { "/usr/bin/dscl", netInfoLoc, "-read", nameSearchString, "PrimaryGroupID" };
		logger.debug("exec: /usr/bin/dscl " + netInfoLoc + " -read " + nameSearchString + " PrimaryGroupID");
		String foundName = getResultFromCommandLineUtility(findNameProcessArgs, "");
		logger.debug("got " + foundName);
		return (foundName.matches("PrimaryGroupID:.+"));
	}
	
	public static String getFullNameFromGroupName(String groupname, String netInfoLoc, String stringIfFail) {
		Log logger = LogFactory.getLog(BlojsomAppleUtils.class);
		String nameSearchString = "/Groups/" + groupname;
		String [] findNameProcessArgs = { "/usr/bin/dscl", netInfoLoc, "-read", nameSearchString, "RealName" };
		logger.debug("exec: /usr/bin/dscl " + netInfoLoc + " -read " + nameSearchString + " RealName");
		String foundName = getResultFromCommandLineUtility(findNameProcessArgs, stringIfFail);
		logger.debug("got " + foundName);
		
		if (foundName.matches("RealName: .+")) {
			return foundName.substring(10);
		}

		return stringIfFail;
	}
	
	public static String getFullNameFromShortName(String username, String netInfoLoc) {
		String nameSearchString = "/Users/" + username;
		String [] findNameProcessArgs = { "/usr/bin/dscl", netInfoLoc, "-read", nameSearchString, "RealName" };
		String foundName = getResultFromCommandLineUtility(findNameProcessArgs, username);
		
		if (foundName.matches("RealName: .+")) {
			foundName = foundName.substring(10);
		} else {
			foundName = username;
		}
		
		return foundName;
	}
	 
    /**
     * Find out if a short name is a member of a group.
	 * If a matching full name isn't found, returns the input.
     */
	public static boolean checkGroupMembershipForUser(String shortName, String authGroupName) {
		Log logger = LogFactory.getLog(BlojsomAppleUtils.class);
		
		logger.debug("Checking group " + authGroupName + " for user " + shortName);
		logger.debug("will execute /usr/libexec/blojsom/bin/ismember_blojsom_helper " + shortName + " -g " + authGroupName);
		
		String [] isMemberProcessArgs = { "/usr/libexec/blojsom/bin/ismember_blojsom_helper", shortName, "-g", authGroupName };
		int resultCode = 1;
		
		try {
			Process isMemberProcess = Runtime.getRuntime().exec(isMemberProcessArgs);
			resultCode = isMemberProcess.waitFor();
		} catch (java.io.IOException e) {
			logger.error(e);
		} catch (java.lang.InterruptedException e) {
			logger.error(e);
		}
		
		if (resultCode == 0)
			return true;
		
		return false;
	}
	
    /**
     * Returns an array of nested groups 
     */
	public static String [] getNestedGroupsForGroup(String shortName, String netInfoLoc) {
		Log logger = LogFactory.getLog(BlojsomAppleUtils.class);
		String groupSearchString = "/Groups/" + shortName;
		
		logger.debug("Getting nested groups for " + shortName + " in " + netInfoLoc);
		logger.debug("will execute dscl . read " + groupSearchString + " nestedgroups");
		String [] dsclProcessArgs = { "/usr/bin/dscl", netInfoLoc, "read", groupSearchString, "nestedgroups" };
		String nestedGroupsResult = getResultFromCommandLineUtility(dsclProcessArgs, "");
		
		if (nestedGroupsResult.matches("nestedgroups: .+")) {
			nestedGroupsResult = nestedGroupsResult.substring(14);
			return nestedGroupsResult.split(" ");
		}
		
		return new String[0];
	}

    /**
     * Find out whether a short groupname is a member of the weblog SACL.
     */
	public static boolean checkSACLMembershipForGroup(String shortName) {
		Log logger = LogFactory.getLog(BlojsomAppleUtils.class);
		String aclGroupName = null;
		
		logger.debug("Checking weblog SACL for group " + shortName);
		
		// if the SACL group doesn't exist, then allow all
		if (doesGroupExistInDS("com.apple.access_weblog", ".")) {
			aclGroupName = "com.apple.access_weblog";
		} else if (doesGroupExistInDS("com.apple.access_all_services", ".")) {
			aclGroupName = "com.apple.access_all_services";
		} else {
			return true;
		}
		
		// loop through the nested groups in the weblog SACL group
		String [] nestedGroups = getNestedGroupsForGroup(aclGroupName, ".");
		String [] getGroupNameArgs;
		for (int i = 0; i < nestedGroups.length; i++) {
			// first decide whether it is a legacy group
			if (nestedGroups[i].startsWith(DSCL_LEGACY_GROUP_PREFIX)) {
				String hexNumberString = "0x"+nestedGroups[i].substring(DSCL_LEGACY_GROUP_PREFIX.length());
				//ugly conversion
				Integer groupNumber = Integer.decode(hexNumberString);
				logger.debug("will execute /usr/bin/dscl /Search -search /Groups PrimaryGroupID " + groupNumber.toString());
				getGroupNameArgs = new String [] { "/usr/bin/dscl", "/Search", "-search", "/Groups", "PrimaryGroupID", groupNumber.toString() };
			}
			else {
				logger.debug("will execute /usr/bin/dscl /Search -search /Groups GeneratedUID " + nestedGroups[i]);
				getGroupNameArgs = new String [] { "/usr/bin/dscl", "/Search", "-search", "/Groups", "GeneratedUID", nestedGroups[i] };
			}
			String groupName = "";
			String result = getResultFromCommandLineUtility(getGroupNameArgs, groupName);
			
			// if the last line contains a tab...
			if (result.matches(".+\t.+")) {
				// group name is before the tab
				String [] splitResult = result.split("\t");
				groupName = splitResult[0];
			}
			
			// if this group name matches then they're in the SACL
			if (groupName.equals(shortName)) {
				return true;
			}
		}
		
		// if we got out of the for loop then there were no matches
		return false;
	}

    /**
     * Find out if a short name is a member of a SACL.
     */
	public static boolean checkSACLMembershipForUser(String shortName) {
		Log logger = LogFactory.getLog(BlojsomAppleUtils.class);
		
		logger.debug("Checking weblog SACL for user " + shortName);
		logger.debug("will execute /usr/libexec/blojsom/bin/ismember_blojsom_helper " + shortName + " weblog");
		
		String [] isMemberProcessArgs = { "/usr/libexec/blojsom/bin/ismember_blojsom_helper", shortName, "-s", "weblog" };
		int resultCode = 1;
		
		try {
			Process isMemberProcess = Runtime.getRuntime().exec(isMemberProcessArgs);
			resultCode = isMemberProcess.waitFor();
		} catch (java.io.IOException e) {
			logger.error(e);
		} catch (java.lang.InterruptedException e) {
			logger.error(e);
		}
		
		if (resultCode == 0)
			return true;
		
		return false;
	}
	
    /**
     * This method verify a username's password
	 * Returns true if the password is correct, false if it isn't.
     */
	public static boolean checkUserPassword(String username, String password) {
		Log logger = LogFactory.getLog(BlojsomAppleUtils.class);
		
		try {
			String [] processArgs = { "/usr/libexec/chkpasswd", username };
			Process checkPasswordProcess = Runtime.getRuntime().exec(processArgs);
			OutputStream passwdStream = checkPasswordProcess.getOutputStream();
			OutputStreamWriter passwdStreamWriter = new OutputStreamWriter(passwdStream);
			passwdStreamWriter.write(password);
			passwdStreamWriter.write("\n");
			passwdStreamWriter.close();
			int resultCode = checkPasswordProcess.waitFor();
			
			if (resultCode == 0) {
				return true;
			}
		} catch (java.io.IOException e) {
			logger.error(e);
		} catch (java.lang.InterruptedException e) {
			logger.error(e);
		}
		
		return false;
	}

    /**
     * This method attempts to create a user blog if it doesn't exist.
	 * True means it either could or didn't need to. False means it couldn't.
     */
	public static boolean attemptUserBlogCreation(BlojsomConfiguration blojsomConfiguration, ServletConfig servletConfig, String blogUserID) {
		Log logger = LogFactory.getLog(BlojsomAppleUtils.class);
		String newUserFullName = blogUserID;
		
		logger.debug("attemptUserBlogCreation called");
		
		if (blogUserID == null) {
			logger.debug("no blogUserID!");
		} else {
			logger.debug("blogUserID = " + blogUserID);
		}
	
		// if user already exists then no need to go further
		if (Arrays.binarySearch(blojsomConfiguration.getBlojsomUsers(), blogUserID) >= 0) {
			return true;
		}
		
		logger.debug("Checking to see if '" + blogUserID + "' exists in DS");
		
		// if the user isn't defined in directory services, then bail
		String resolvedBlogUserID = validateShortNameAndResolveAliases(blogUserID, "/Search");
		blogUserID = (resolvedBlogUserID == null) ? blogUserID : resolvedBlogUserID;
		boolean foundUsername = (resolvedBlogUserID != null);
		boolean foundGroupName = ((doesGroupExistInDS(blogUserID, ".") || doesGroupExistInDS(blogUserID, "/Search"))); 
		
		if (foundUsername && checkSACLMembershipForUser(blogUserID)) {
			logger.debug("Found user '" + blogUserID + "'.");
			newUserFullName = getFullNameFromShortName(blogUserID, ".");
			
			if (newUserFullName.equals(blogUserID)) {
				newUserFullName = getFullNameFromShortName(blogUserID, "/Search");
			}
		}
		else if (foundGroupName && checkSACLMembershipForGroup(blogUserID)) {
			logger.debug("Found group '" + blogUserID + "'.");
			newUserFullName = getFullNameFromGroupName(blogUserID, ".", blogUserID);
			
			if (newUserFullName.equals(blogUserID)) {
				newUserFullName = getFullNameFromGroupName(blogUserID, "/Search", blogUserID);
			}
		}
		else {
			return false;
		}
		
		logger.debug("Adding new user id: " + blogUserID);

		BlogUser blogUser = new BlogUser();
		blogUser.setId(blogUserID);
		
		File blogUserDirectory = new File(blojsomConfiguration.getInstallationDirectory() + blojsomConfiguration.getBaseConfigurationDirectory() + blogUserID);
		
		// Make sure that the blog user ID does not conflict with a directory underneath the installation directory
		if (blogUserDirectory.exists()) {
			logger.debug("User directory already exists for blog user: " + blogUserID);
			return true;
		}
		
		try {
			Properties configurationProperties = BlojsomUtils.loadProperties(servletConfig, PLUGIN_ADMIN_EDIT_USERS_IP, true);
			String authorizationConfiguration = servletConfig.getInitParameter(BLOG_AUTHORIZATION_IP);
			String flavorConfiguration = servletConfig.getInitParameter(BLOJSOM_FLAVOR_CONFIGURATION_IP);
			String pluginConfiguration = servletConfig.getInitParameter(BLOJSOM_PLUGIN_CONFIGURATION_IP);
			String bootstrapDirectoryLoc = configurationProperties.getProperty(BOOTSTRAP_DIRECTORY_IP);
			String blogHomeBaseDirectory = configurationProperties.getProperty(BLOG_HOME_BASE_DIRECTORY_IP);
			File bootstrapDirectory = new File(blojsomConfiguration.getInstallationDirectory() + blojsomConfiguration.getBaseConfigurationDirectory() + bootstrapDirectoryLoc);
			File newUserDirectory = new File(blojsomConfiguration.getInstallationDirectory() + blojsomConfiguration.getBaseConfigurationDirectory() + blogUserID);
			
			// make sure the blog home base directory is valid
			blogHomeBaseDirectory = BlojsomUtils.checkStartingAndEndingSlash(blogHomeBaseDirectory);
			
			logger.debug("Copying bootstrap directory: " + bootstrapDirectory.toString() + " to target user directory: " + newUserDirectory.toString());
			BlojsomUtils.copyDirectory(bootstrapDirectory, newUserDirectory);
			File blogHomeDirectory = new File(blogHomeBaseDirectory + blogUserID);
			blogHomeDirectory.mkdir();

			// Configure blog
			logger.debug("loading copied properties file at " + blojsomConfiguration.getBaseConfigurationDirectory() + blogUserID + '/' + BLOG_DEFAULT_PROPERTIES);
			Properties blogProperties = null;
			for (int i = 0; i < 20; i++) {
				try {
					blogProperties = BlojsomUtils.loadProperties(servletConfig, blojsomConfiguration.getBaseConfigurationDirectory() + blogUserID + '/' + BLOG_DEFAULT_PROPERTIES);
					break;
				} catch (BlojsomException e) {
					try {
						java.lang.Thread.currentThread().sleep(1000);
					} catch (java.lang.InterruptedException e2) {
						logger.error(e);
					}
				}
			}
			logger.debug("loaded properties file");
			blogProperties.put(BLOG_HOME_IP, blogHomeBaseDirectory + blogUserID);
			blogProperties.put(BLOG_OWNER, newUserFullName);
			
			if (foundUsername) {
				blogProperties.put(BLOG_NAME_IP, newUserFullName + "'s Weblog");
			}
			else if (foundGroupName) {
				blogProperties.put(BLOG_NAME_IP, newUserFullName);
			}
			
			String blogOwnerEmail = (String)blogProperties.get(BLOG_OWNER_EMAIL);
			
			if ((blogOwnerEmail != null) && (!("".equals(blogOwnerEmail)))) {
				blogOwnerEmail = blogUserID + blogOwnerEmail;
				blogProperties.put(BLOG_OWNER_EMAIL, blogOwnerEmail);
			}
			
			blogProperties.put(BLOG_URL_IP, "/weblog/" + blogUserID + "/");

			// Write out the blog configuration
			File blogConfigurationFile = new File(blojsomConfiguration.getInstallationDirectory() + blojsomConfiguration.getBaseConfigurationDirectory() + blogUserID + '/' + BLOG_DEFAULT_PROPERTIES);
			FileOutputStream fos = new FileOutputStream(blogConfigurationFile);
			blogProperties.store(fos, null);
			fos.close();
			logger.debug("Wrote blog configuration information for new user: " + blogConfigurationFile.toString());

			// Set the blog information for the user
			Blog blog = new Blog(blogProperties);
			blogUser.setBlog(blog);

			// Configure authorization
			Map authorizationMap = new HashMap();
			
			if (foundUsername) {
				authorizationMap.put(blogUserID, "_USE_DIRECTORY_SERVICES_");
			}
			else if (foundGroupName) {
				authorizationMap.put("_DIRECTORY_SERVICES_GROUP_", blogUserID);
			}
			blogUser.getBlog().setAuthorization(authorizationMap);
			logger.debug("Set authorization information for new user: " + blogUserID);

			// Write out the authorization
			File blogAuthorizationFile = new File(blojsomConfiguration.getInstallationDirectory() + blojsomConfiguration.getBaseConfigurationDirectory() + blogUserID + '/' + authorizationConfiguration);
			fos = new FileOutputStream(blogAuthorizationFile);
			Properties authorizationProperties = BlojsomUtils.mapToProperties(blogUser.getBlog().getAuthorization());
			authorizationProperties.store(fos, null);
			fos.close();
			logger.debug("Wrote blog authorization information for new user: " + blogAuthorizationFile.toString());

			// Configure flavors
			Map flavors = new HashMap();
			Map flavorToTemplateMap = new HashMap();
			Map flavorToContentTypeMap = new HashMap();

			Properties flavorProperties = BlojsomUtils.loadProperties(servletConfig, blojsomConfiguration.getBaseConfigurationDirectory() + blogUserID + '/' + flavorConfiguration);

			Iterator flavorIterator = flavorProperties.keySet().iterator();
			while (flavorIterator.hasNext()) {
				String flavor = (String) flavorIterator.next();
				String[] flavorMapping = BlojsomUtils.parseCommaList(flavorProperties.getProperty(flavor));
				flavors.put(flavor, flavor);
				flavorToTemplateMap.put(flavor, flavorMapping[0]);
				flavorToContentTypeMap.put(flavor, flavorMapping[1]);

			}

			blogUser.setFlavors(flavors);
			blogUser.setFlavorToTemplate(flavorToTemplateMap);
			blogUser.setFlavorToContentType(flavorToContentTypeMap);
			logger.debug("Loaded flavor information for new user: " + blogUserID);

			// Configure plugins
			Map pluginChainMap = new HashMap();

			Properties pluginProperties = BlojsomUtils.loadProperties(servletConfig, blojsomConfiguration.getBaseConfigurationDirectory() + blogUserID + '/' + pluginConfiguration);
			Iterator pluginIterator = pluginProperties.keySet().iterator();
			while (pluginIterator.hasNext()) {
				String plugin = (String) pluginIterator.next();
				if (plugin.indexOf(BLOJSOM_PLUGIN_CHAIN) != -1) {
					pluginChainMap.put(plugin, BlojsomUtils.parseCommaList(pluginProperties.getProperty(plugin)));
					logger.debug("Added plugin chain: " + plugin + '=' + pluginProperties.getProperty(plugin) + " for user: " + blogUserID);
				}
			}

			blogUser.setPluginChain(pluginChainMap);
			logger.debug("Loaded plugin chain map for new user: " + blogUserID);
		
			// Add the user to the global list of users
			blojsomConfiguration.addBlogID(blogUserID);
			writeBlojsomConfiguration(blojsomConfiguration);
			logger.debug("Wrote new blojsom configuration after adding new user: " + blogUserID);
		} catch (BlojsomException e) {
			logger.error(e);
			return false;
		} catch (IOException e) {
			logger.error(e);
			return false;
		}
		
		return true;
	}
	
    /**
     * Write out the update blojsom configuration file. This is done after adding or deleting a new user.
     */
    public static void writeBlojsomConfiguration(BlojsomConfiguration blojsomConfiguration) {
		Log logger = LogFactory.getLog(BlojsomAppleUtils.class);
        File blojsomConfigurationFile = new File(blojsomConfiguration.getInstallationDirectory() + blojsomConfiguration.getBaseConfigurationDirectory() + "blojsom.properties");
        Properties configurationProperties = new BlojsomProperties();
		String [] blojsomUsers = blojsomConfiguration.getBlojsomUsers();
		StringBuffer usersString = new StringBuffer();
		for (int i = 0; i < blojsomUsers.length; i++) {
			usersString.append(blojsomUsers[i]).append(",");
		}
        configurationProperties.put(BLOJSOM_USERS_IP, usersString);
        configurationProperties.put(BLOJSOM_AUTHORIZATION_PROVIDER_IP, "com.apple.blojsom.util.AppleBlojsomAuthorizationProvider"); // change this!
        configurationProperties.put(BLOJSOM_FETCHER_IP, blojsomConfiguration.getFetcherClass());
        configurationProperties.put(BLOJSOM_DEFAULT_USER_IP, blojsomConfiguration.getDefaultUser());
        configurationProperties.put(BLOJSOM_INSTALLATION_DIRECTORY_IP, blojsomConfiguration.getInstallationDirectory());
        configurationProperties.put(BLOJSOM_CONFIGURATION_BASE_DIRECTORY_IP, blojsomConfiguration.getBaseConfigurationDirectory());

		// write config
        try {
            FileOutputStream fos = new FileOutputStream(blojsomConfigurationFile);
            configurationProperties.store(fos, null);
            fos.close();
        } catch (IOException e) {
            logger.error(e);
        }
   }
	
    /**
     * Get computer name: Get the user-defined computer name for this system.
     */
	public static String getComputerName() {
		return getServerAdminSetting("info:computerName", "Mac OS X Server");
	}

    /**
     * Increment the size of a string array by 1. Returns the new array.
     */
    public static String [] addSlotToStringArray(String [] theArray) {
		String [] tmpVar = new String [theArray.length + 1];
		System.arraycopy(theArray, 0, tmpVar, 0, theArray.length);
		return tmpVar;
    }
}
