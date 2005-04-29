/**
 * Contains:   Inline administration plug-in for blojsom.
 * Written by: John Anderson (for addtl writers check CVS comments).
 * Copyright:  © 2004 Apple Computer, Inc., all rights reserved.
 * Note:       When editing this file set PB to "Editor uses tabs/width=4".
 *
 * $Id: InlineAdminPlugin.java,v 1.40 2005/02/24 21:33:01 johnan Exp $
 */ 
package com.apple.blojsom.plugin.inlineadmin;

import com.apple.blojsom.util.AppleBlojsomVersion;
import com.apple.blojsom.util.BlojsomAppleUtils;
import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.BlojsomException;
import org.blojsom.blog.*;
import org.blojsom.plugin.BlojsomPlugin;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.util.BlojsomConstants;
import org.blojsom.util.BlojsomUtils;
import org.blojsom.util.BlojsomMetaDataConstants;
import org.blojsom.plugin.admin.EditBlogEntriesPlugin;
import org.blojsom.fetcher.BlojsomFetcher;
import org.blojsom.fetcher.BlojsomFetcherException;

import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.ServletConfig;
import java.io.*;
import java.util.*;

/**
 * InlineAdminPlugin
 *
 * @author John Anderson
 * @since blojsom 2.14
 * @version $Id: InlineAdminPlugin.java,v 1.40 2005/02/24 21:33:01 johnan Exp $
 */
public class InlineAdminPlugin extends EditBlogEntriesPlugin {

    protected static final Log _logger = LogFactory.getLog(InlineAdminPlugin.class);

    // Constants
	protected static final String BLOJSOM_INLINE_ADMIN_PLUGIN_AUTHENTICATED = "BLOJSOM_INLINE_ADMIN_PLUGIN_AUTHENTICATED";
	protected static final String INLINE_ADMIN_PLUGIN = "INLINE_ADMIN_PLUGIN";
	protected static final String HAS_ADDITIONAL_CATEGORY = "HAS_ADDITIONAL_CATEGORY";
	protected static final String ADDITIONAL_CATEGORY = "ADDITIONAL_CATEGORY";
	protected static final String PERMALINK_SEARCH_STR = "[^A-Za-z0-9-_]";
	protected static final String PERMALINK_REPLACE_STR = "";
	protected static final String SHOW_MESSAGE_PARAM = "show_message";
	private static final String EDITING_CATEGORY_NAME = "editing_category_name";
    private static final String EDITING_ENTRY_ID = "editing_entry_id";
	private static final String EDITING_ENTRY_TITLE = "editing_entry_title";
	private static final String EDITING_ENTRY_DESC = "editing_entry_desc";
    private static final String ADDING_CATEGORY_NAME = "adding_category_name";
	private static final String ADDING_ENTRY_TITLE = "adding_entry_title";
	private static final String ADDING_ENTRY_DESC = "adding_entry_desc";
	private static final String NEWCAT_SUPER = "newcat_super";
	private static final String NEWCAT_PARENT_LABEL = "new_category_parent_label";
	private static final String NEWCAT_CATEGORY_NAME = "newcat_category_name";
	private static final String ALL_STYLESHEETS = "all_stylesheets";
	private static final String BLOG_SETTING_STYLESHEET = "blog_setting_stylesheet";
	private static final String INLINE_BLOG_ENTRY_EXTENSION = ".html";
	private static final String PREV_ADDING_CATEGORY_NAME = "prev_adding_category_name";
	private static final String PREV_ADDING_ENTRY_TITLE = "prev_adding_entry_title";
	private static final String PREV_ADDING_ENTRY_DESC = "prev_adding_entry_desc";
    private static final String BLOG_TRACKBACK_URLS = "blog-trackback-urls";
    private static final String DELETING_CATEGORY_NAME = "deleting_category_name";
	private static final String CATEGORY_PROPERTIES_FILE = "blojsom.properties";
    private static final int MAXIMUM_FILENAME_LENGTH = 28;

    // Actions
    private static final String EDIT_BLOG_PROPERTIES_ACTION = "edit-blog-properties";
    private static final String ADD_BLOG_CATEGORY_ACTION = "add-blog-category";
    private static final String DELETE_BLOG_CATEGORY_ACTION = "delete-blog-category";
    private static final String EDIT_BLOG_ENTRY_ACTION = "edit-blog-entry";
    private static final String UPDATE_BLOG_ENTRY_ACTION = "update-blog-entry";
    private static final String ADD_BLOG_ENTRY_ACTION = "add-blog-entry";
    private static final String DELETE_BLOG_ENTRY_ACTION = "delete-blog-entry";
	
	public static final String BLOG_FEED_URL = "BLOG_FEED_URL";

    /**
     * Default constructor.
     */
    public InlineAdminPlugin() {
    }

    /**
     * Redirects and adds a message to the context under the <code>BLOJSOM_ADMIN_PLUGIN_OPERATION_RESULT</code> key
     *
	 * @param httpServletResponse Response
     * @param message Message to add
     */
	protected void redirectWithMessage(HttpServletResponse httpServletResponse, String message) {
		String encodedMessage = BlojsomUtils.urlEncode(message);
		
		try {
			httpServletResponse.sendRedirect("./?" + SHOW_MESSAGE_PARAM + "=" + encodedMessage);
		} catch (IOException e) {
			_logger.error(e);
		}
	}
	
    /**
     * Redirects to base cateogry and adds a message to the context
     * under the <code>BLOJSOM_ADMIN_PLUGIN_OPERATION_RESULT</code> key
     *
     * @param user {@link org.blojsom.blog.BlogUser} instance
     * @param httpServletResponse Response
     * @param message Message to add
     */
	protected void redirectToBaseCategoryWithMessage(BlogUser user, HttpServletResponse httpServletResponse, String message) {
		String encodedMessage = BlojsomUtils.urlEncode(message);
		
		try {
			httpServletResponse.sendRedirect(user.getBlog().getBlogURL() + "?" + SHOW_MESSAGE_PARAM + "=" + encodedMessage);
		} catch (IOException e) {
			_logger.error(e);
		}
	}
	
    /**
     * Process the blog entries
     *
     * @param httpServletRequest Request
     * @param httpServletResponse Response
     * @param user {@link org.blojsom.blog.BlogUser} instance
     * @param context Context
     * @param entries Blog entries retrieved for the particular request
     * @return Modified set of blog entries
     * @throws BlojsomPluginException If there is an error processing the blog entries
     */
    public BlogEntry[] process(HttpServletRequest httpServletRequest, HttpServletResponse httpServletResponse, BlogUser user, Map context, BlogEntry[] entries) throws BlojsomPluginException {
        String action = BlojsomUtils.getRequestValue(ACTION_PARAM, httpServletRequest);		
		String messageToShow = BlojsomUtils.getRequestValue(SHOW_MESSAGE_PARAM, httpServletRequest);
		
		// if there's a message to show, show it
		if (messageToShow != null) {
			addOperationResultMessage(context, messageToShow);
		}
		
		// add ourselves to the context (for converting user name)
		context.put(INLINE_ADMIN_PLUGIN, this);
		
		context.put("appleBlojsomRevision", AppleBlojsomVersion.appleBlojsomRevision);
		
		// get the blog reference
		Blog blog = user.getBlog();
		
		// make sure the blog home folder exists (in case it's been moved)
		File blogHome = new File(blog.getBlogHome());
		
		if (! blogHome.exists()) {
			blogHome.mkdir();
		}
		
		// feed-based URL
		context.put(BLOG_FEED_URL, blog.getBlogURL().replaceFirst("http:", "feed:"));
		
		// if we're not authenticated, set the value and do nothing else
        if (!authenticateUser(httpServletRequest, httpServletResponse, context, user)) {
			context.put(BLOJSOM_INLINE_ADMIN_PLUGIN_AUTHENTICATED, "false");
			
			if (LOGOUT_ACTION.equals(action)) {
				redirectWithMessage(httpServletResponse, "$message.logout");
			}
			else if (ADD_BLOG_ENTRY_ACTION.equals(action)) {
				context.put(PREV_ADDING_CATEGORY_NAME, BlojsomUtils.escapeString(BlojsomUtils.getRequestValue(ADDING_CATEGORY_NAME, httpServletRequest)));
				context.put(PREV_ADDING_ENTRY_TITLE, BlojsomUtils.escapeString(BlojsomUtils.getRequestValue(ADDING_ENTRY_TITLE, httpServletRequest)));
				context.put(PREV_ADDING_ENTRY_DESC, BlojsomUtils.escapeString(BlojsomUtils.getRequestValue(ADDING_ENTRY_DESC, httpServletRequest)));
				redirectWithMessage(httpServletResponse, " message.nosession.entry");
			}
			else if ((action != null) && (!("".equals(action)))) {
				redirectWithMessage(httpServletResponse, " message.nosession");
			}
			
			return entries;
		}
		
		_logger.debug("User is logged in.");
		
		// say we're authenticated
		context.put(BLOJSOM_INLINE_ADMIN_PLUGIN_AUTHENTICATED, "true");
		
		// add the list of stylesheets for the popup
        String installationDirectory = _blojsomConfiguration.getInstallationDirectory();
        StringBuffer stylesheetsFolderPath = new StringBuffer();
        stylesheetsFolderPath.append(installationDirectory);
		stylesheetsFolderPath.append("blojsom_resources/stylesheets");
		File stylesheetsFolder = new File(stylesheetsFolderPath.toString());
		FileFilter stylesheetsFilter = BlojsomUtils.getExtensionFilter(".css");
		File[] stylesheetFiles = stylesheetsFolder.listFiles(stylesheetsFilter);
		context.put(ALL_STYLESHEETS, stylesheetFiles);
		
		// if we're submitting form values...
		if (EDIT_BLOG_PROPERTIES_ACTION.equals(action)) {
            _logger.debug("User requested edit action");
			
			String blogPropertyValue = BlojsomUtils.getRequestValue("blog-name", httpServletRequest);
            blog.setBlogName(blogPropertyValue);
            blogPropertyValue = BlojsomUtils.getRequestValue("blog-description", httpServletRequest);
            blog.setBlogDescription(blogPropertyValue);
            blogPropertyValue = BlojsomUtils.getRequestValue("blog-owner", httpServletRequest);
            blog.setBlogOwner(blogPropertyValue);
            blogPropertyValue = BlojsomUtils.getRequestValue("blog-owner-email", httpServletRequest);
            blog.setBlogOwnerEmail(blogPropertyValue);
			blogPropertyValue = BlojsomUtils.getRequestValue("blog_setting_stylesheet", httpServletRequest);
			blog.setBlogDefaultStyleSheet(blogPropertyValue);
			
			// use the new properties
            user.setBlog(blog);
			
            // get the blog properties
            Properties blogProperties = BlojsomUtils.mapToProperties(blog.getBlogProperties(), UTF8);
            File propertiesFile = new File(_blojsomConfiguration.getInstallationDirectory()
                    + BlojsomUtils.removeInitialSlash(_blojsomConfiguration.getBaseConfigurationDirectory()) +
                    "/" + user.getId() + "/" + BlojsomConstants.BLOG_DEFAULT_PROPERTIES);

            _logger.debug("Writing blog properties to: " + propertiesFile.toString());
			
			// write out the blog properties again
            try {
                FileOutputStream fos = new FileOutputStream(propertiesFile);
                blogProperties.store(fos, null);
                fos.close();
				addOperationResultMessage(context, "$message.weblogsettingssaved");
            } catch (IOException e) {
                _logger.error(e);
				addOperationResultMessage(context, " message.weblogsettingsnotsaved");
            }
			
		} else if (ADD_BLOG_CATEGORY_ACTION.equals(action)) {
            _logger.debug("User requested new category action");
            String blogCategoryName = BlojsomUtils.getRequestValue(NEWCAT_CATEGORY_NAME, httpServletRequest);
			String blogParentCategoryName = BlojsomUtils.getRequestValue(NEWCAT_SUPER, httpServletRequest);
			String blogParentCategoryLabel = BlojsomUtils.getRequestValue(NEWCAT_PARENT_LABEL, httpServletRequest);
			
            // Check for blank or null category
            if (BlojsomUtils.checkNullOrBlank(blogCategoryName)) {
				redirectWithMessage(httpServletResponse, " message.nocategory");
                return entries;
            }
			
			// Check for blank or null parent
			if (BlojsomUtils.checkNullOrBlank(blogParentCategoryName)) {
				redirectWithMessage(httpServletResponse, " message.noparentcategory");
				return entries;
			}
			
			// Check for blank or null parent label
			if (BlojsomUtils.checkNullOrBlank(blogParentCategoryLabel)) {
				blogParentCategoryLabel = blogParentCategoryName;
			}
				
            blogCategoryName = blogCategoryName.replaceAll("/", "-");
			blogParentCategoryName = BlojsomUtils.normalize(blogParentCategoryName);
			blogParentCategoryLabel = BlojsomUtils.normalize(blogParentCategoryLabel);
			_logger.debug("Adding blog category: " + blogCategoryName);
			
			// let's not write out non-ASCII folder names
			String blogCategoryFilename = new String(blogCategoryName);
			blogCategoryFilename = blogCategoryFilename.replaceAll(PERMALINK_SEARCH_STR, PERMALINK_REPLACE_STR);
			boolean writeMetaDataFile = ((!blogParentCategoryLabel.equals(blogParentCategoryName)) || (!blogCategoryName.equals(blogCategoryFilename)));
						
			if (BlojsomUtils.checkNullOrBlank(blogCategoryFilename)) {
				writeMetaDataFile = true;
				blogCategoryFilename = getBlogEntryFilename(blogCategoryName, "");
			} else {
				if (blogCategoryFilename.length() > MAXIMUM_FILENAME_LENGTH) {
					blogCategoryFilename = blogCategoryFilename.substring(0, MAXIMUM_FILENAME_LENGTH);
				}
				File proposedBlogCategoryFile = new File(blog.getBlogHome() + "/" + BlojsomUtils.removeInitialSlash(blogParentCategoryName) + "/" + BlojsomUtils.removeInitialSlash(blogCategoryFilename));
				if (proposedBlogCategoryFile.exists()) {
					writeMetaDataFile = true;
					blogCategoryFilename = getBlogEntryFilename(blogCategoryName, "");
				}
			}
			
			blogCategoryFilename = blog.getBlogHome() + "/" + BlojsomUtils.removeInitialSlash(blogParentCategoryName) + "/" + BlojsomUtils.removeInitialSlash(blogCategoryFilename);
            File newBlogCategory = new File(blogCategoryFilename);

			if (!newBlogCategory.mkdirs()) {
				_logger.error("Unable to add new blog category: " + blogCategoryName);
				redirectWithMessage(httpServletResponse, " message.unabletoaddcategory");
				return entries;
			} else {
				_logger.debug("Created blog directory: " + newBlogCategory.toString());
				
				if (writeMetaDataFile) {
					blogCategoryName = BlojsomUtils.removeTrailingSlash(blogParentCategoryLabel) + "/" + blogCategoryName;
					Map categoryMetaData = new HashMap();
					categoryMetaData.put(NAME_KEY, blogCategoryName);
					
					try {
						File propertiesFile = new File(blogCategoryFilename + "/" + CATEGORY_PROPERTIES_FILE);
						propertiesFile.createNewFile();
						FileOutputStream fos = new FileOutputStream(propertiesFile);
						BlojsomUtils.mapToProperties(categoryMetaData, UTF8).store(fos, null);
						fos.close();
					} catch (IOException e) {
						_logger.error(e);
					}
				}
			}
			
			context.put(HAS_ADDITIONAL_CATEGORY, "true");
			context.put(ADDITIONAL_CATEGORY, "/" + BlojsomUtils.removeInitialSlash(blogParentCategoryName) + blogCategoryName + "/");
			_logger.debug("Successfully updated blog category: " + blogCategoryName);
			redirectToBaseCategoryWithMessage(user, httpServletResponse, "$message.addedcategory");
			
        } else if (DELETE_BLOG_CATEGORY_ACTION.equals(action)) {
            _logger.debug("User request blog category delete action");
            String blogCategoryName = BlojsomUtils.getRequestValue(DELETING_CATEGORY_NAME, httpServletRequest);
            blogCategoryName = BlojsomUtils.normalize(blogCategoryName);

            File existingBlogCategory = new File(blog.getBlogHome() + "/" + BlojsomUtils.removeInitialSlash(blogCategoryName));
            String [] categoryContents = existingBlogCategory.list();
            for (int i = 0; i < categoryContents.length; i++) {
				if (!CATEGORY_PROPERTIES_FILE.equals(categoryContents[i])) {
					File oldLocation = new File(existingBlogCategory.toString() + "/" + categoryContents[i]);
					File newLocation = new File(blog.getBlogHome() + "/" + categoryContents[i]);
					if (!newLocation.exists()) {
						oldLocation.renameTo(newLocation);
					}
				}
            }
            if (!BlojsomUtils.deleteDirectory(existingBlogCategory)) {
                _logger.debug("Unable to delete blog category: " + existingBlogCategory.toString());
                redirectToBaseCategoryWithMessage(user, httpServletResponse, " message.unabletodeletecategory");
            } else {
                _logger.debug("Deleted blog category: " + existingBlogCategory.toString());
                redirectToBaseCategoryWithMessage(user, httpServletResponse, "$message.categorydeleted");
            }
		} else if (EDIT_BLOG_ENTRY_ACTION.equals(action)) {
            String blogCategoryName = BlojsomUtils.getRequestValue(EDITING_CATEGORY_NAME, httpServletRequest);
            blogCategoryName = BlojsomUtils.normalize(blogCategoryName);
            String blogEntryId = BlojsomUtils.getRequestValue(EDITING_ENTRY_ID, httpServletRequest);
			
			context.put(EDITING_CATEGORY_NAME, blogCategoryName);
			context.put(EDITING_ENTRY_ID, blogEntryId);

			for (int i = 0; i < entries.length; i++) {
				BlogEntry entry = entries[i];
				
				if (blogCategoryName.equals(entry.getBlogCategory().toString()) && blogEntryId.equals(entry.getPermalink())) {
					context.put(EDITING_ENTRY_TITLE, BlojsomUtils.escapeString(entry.getTitle()));
					context.put(EDITING_ENTRY_DESC, BlojsomUtils.escapeString(entry.getDescription()));
				}
			}
			
		} else if (UPDATE_BLOG_ENTRY_ACTION.equals(action)) {
            _logger.debug("User requested update blog entry action");

            String blogCategoryName = BlojsomUtils.getRequestValue(EDITING_CATEGORY_NAME, httpServletRequest);
            blogCategoryName = BlojsomUtils.normalize(blogCategoryName);
            String blogEntryId = BlojsomUtils.getRequestValue(EDITING_ENTRY_ID, httpServletRequest);
            String blogEntryTitle = BlojsomUtils.getRequestValue(EDITING_ENTRY_TITLE, httpServletRequest);
            String blogEntryDescription = BlojsomUtils.getRequestValue(EDITING_ENTRY_DESC, httpServletRequest);
            String blogTrackbackURLs = BlojsomUtils.getRequestValue(BLOG_TRACKBACK_URLS, httpServletRequest);
			
            _logger.debug("Blog entry id: " + blogEntryId);
			
            BlogCategory category;
            category = _fetcher.newBlogCategory();
            category.setCategory(blogCategoryName);
            category.setCategoryURL(user.getBlog().getBlogURL() + BlojsomUtils.removeInitialSlash(blogCategoryName));

            Map fetchMap = new HashMap();
            fetchMap.put(BlojsomFetcher.FETCHER_CATEGORY, category);
            fetchMap.put(BlojsomFetcher.FETCHER_PERMALINK, blogEntryId);
            try {
                BlogEntry [] editingEntries = _fetcher.fetchEntries(fetchMap, user);
                if (editingEntries != null) {
                    _logger.debug("Retrieved " + editingEntries.length + " editingEntries from category: " + blogCategoryName);
                    BlogEntry entryToUpdate = editingEntries[0];
                    entryToUpdate.setTitle(blogEntryTitle);
                    entryToUpdate.setDescription(blogEntryDescription);
                    entryToUpdate.save(user);
                    entryToUpdate.load(user);
					StringBuffer entryLink = new StringBuffer();
					entryToUpdate.setLink(user.getBlog().getBlogURL() + BlojsomUtils.removeInitialSlash(entryToUpdate.getCategory()) + "?" + PERMALINK_PARAM + "=" + entryToUpdate.getPermalink());
					entryLink.append("<a href=\"").append(entryToUpdate.getLink()).append("\">").append(entryToUpdate.getTitle()).append("</a>");
                   _logger.debug("Updated blog entry: " + entryToUpdate.getLink());
					redirectWithMessage(httpServletResponse, "$message.updatedentry");
					for (int i = 0; i < entries.length; i++) {
						BlogEntry entry = entries[i];
						
						if (blogCategoryName.equals(entry.getBlogCategory().toString()) && blogEntryId.equals(entry.getPermalink())) {
							entry.setTitle(blogEntryTitle);
							entry.setDescription(blogEntryDescription);
							
							// Send trackback pings
							if (!BlojsomUtils.checkNullOrBlank(blogTrackbackURLs)) {
								sendTrackbackPings(blog, entryToUpdate, blogTrackbackURLs);
							}
						}
					}
                } else {
                    _logger.debug("No editingEntries found in category: " + blogCategoryName);
                }
            } catch (BlojsomFetcherException e) {
                _logger.error(e);
            } catch (BlojsomException e) {
                _logger.error(e);
			}
			
		} else if (ADD_BLOG_ENTRY_ACTION.equals(action)) {
            _logger.debug("User requested add blog entry action");
			
            String blogCategoryName = BlojsomUtils.getRequestValue(ADDING_CATEGORY_NAME, httpServletRequest);
            String blogTrackbackURLs = BlojsomUtils.getRequestValue(BLOG_TRACKBACK_URLS, httpServletRequest);
            blogCategoryName = BlojsomUtils.normalize(blogCategoryName);
            if (!blogCategoryName.endsWith("/")) {
                blogCategoryName += "/";
            }
            String blogEntryTitle = BlojsomUtils.getRequestValue(ADDING_ENTRY_TITLE, httpServletRequest);
            String blogEntryDescription = BlojsomUtils.getRequestValue(ADDING_ENTRY_DESC, httpServletRequest);
			
			if (BlojsomUtils.checkNullOrBlank(blogEntryTitle) && BlojsomUtils.checkNullOrBlank(blogEntryDescription)) {
				redirectWithMessage(httpServletResponse, " message.noentrydescription");
				return entries;
			}

            BlogCategory category;
            category = _fetcher.newBlogCategory();
            category.setCategory(blogCategoryName);
            category.setCategoryURL(user.getBlog().getBlogURL() + BlojsomUtils.removeInitialSlash(blogCategoryName));

            BlogEntry entryToAdd;
            entryToAdd = _fetcher.newBlogEntry();
            entryToAdd.setTitle(blogEntryTitle);
            entryToAdd.setCategory(blogCategoryName);
            entryToAdd.setDescription(blogEntryDescription);

            Map entryMetaData = new HashMap();
            String username = (String) httpServletRequest.getSession().getAttribute(user.getBlog().getBlogURL() + "_" + BLOJSOM_ADMIN_PLUGIN_USERNAME_KEY);
            entryMetaData.put(BlojsomMetaDataConstants.BLOG_ENTRY_METADATA_AUTHOR, username);
			entryMetaData.put(BlojsomMetaDataConstants.BLOG_ENTRY_METADATA_TIMESTAMP, new Long(new Date().getTime()).toString());
			entryToAdd.setMetaData(entryMetaData);
			
			String filename = blogEntryTitle;
			if ("".equals(blogEntryTitle)) {
				filename = blogEntryDescription;
			}
			filename = filename.replaceAll(PERMALINK_SEARCH_STR, PERMALINK_REPLACE_STR);
			
			if (BlojsomUtils.checkNullOrBlank(filename)) {
				filename = getBlogEntryFilename(blogEntryDescription, INLINE_BLOG_ENTRY_EXTENSION);
			} else {
				if (filename.length() > MAXIMUM_FILENAME_LENGTH) {
					filename = filename.substring(0, MAXIMUM_FILENAME_LENGTH);
				}
				filename += INLINE_BLOG_ENTRY_EXTENSION;
				File proposedBlogFile = new File(user.getBlog().getBlogHome() + BlojsomUtils.removeInitialSlash(blogCategoryName) + filename);
				if (proposedBlogFile.exists()) {
					filename = getBlogEntryFilename(blogEntryDescription, INLINE_BLOG_ENTRY_EXTENSION);
				}				
			}
			_logger.debug("Using proposed blog entry filename: " + filename);

            File blogFilename = new File(user.getBlog().getBlogHome() + BlojsomUtils.removeInitialSlash(blogCategoryName) + filename);
            _logger.debug("New blog entry file: " + blogFilename.toString());

            Map attributeMap = new HashMap();
            attributeMap.put(BlojsomMetaDataConstants.SOURCE_ATTRIBUTE, blogFilename);
            entryToAdd.setAttributes(attributeMap);

            try {
                entryToAdd.save(user);
                entryToAdd.load(user);
                StringBuffer entryLink = new StringBuffer();
                entryToAdd.setLink(user.getBlog().getBlogURL() + BlojsomUtils.removeInitialSlash(entryToAdd.getCategory()) + "?" + PERMALINK_PARAM + "=" + entryToAdd.getPermalink());
                entryLink.append("<a href=\"").append(entryToAdd.getLink()).append("\">").append(entryToAdd.getTitle()).append("</a>");
				
				Map fetchMap = new HashMap();
				fetchMap.put(BlojsomFetcher.FETCHER_FLAVOR, "html");
				fetchMap.put(BlojsomFetcher.FETCHER_NUM_POSTS_INTEGER, new Integer(blog.getBlogDisplayEntries()));
				entries = _fetcher.fetchEntries(fetchMap, user);
				
				// Send trackback pings
				if (!BlojsomUtils.checkNullOrBlank(blogTrackbackURLs)) {
					sendTrackbackPings(blog, entryToAdd, blogTrackbackURLs);
				}

				redirectToBaseCategoryWithMessage(user, httpServletResponse, "$message.addedentry");
            } catch (BlojsomException e) {
                _logger.error(e);
				redirectWithMessage(httpServletResponse, " message.unabletoaddentry");
            }
			
        } else if (DELETE_BLOG_ENTRY_ACTION.equals(action)) {
            _logger.debug("User requested delete blog entry action");
			
            String blogCategoryName = BlojsomUtils.getRequestValue(EDITING_CATEGORY_NAME, httpServletRequest);
            blogCategoryName = BlojsomUtils.normalize(blogCategoryName);
            String blogEntryId = BlojsomUtils.getRequestValue(EDITING_ENTRY_ID, httpServletRequest);
            _logger.debug("Blog entry id: " + blogEntryId);

            BlogCategory category;
            category = _fetcher.newBlogCategory();
            category.setCategory(blogCategoryName);
            category.setCategoryURL(user.getBlog().getBlogURL() + BlojsomUtils.removeInitialSlash(blogCategoryName));

            Map fetchMap = new HashMap();
            fetchMap.put(BlojsomFetcher.FETCHER_CATEGORY, category);
            fetchMap.put(BlojsomFetcher.FETCHER_PERMALINK, blogEntryId);
            try {
                entries = _fetcher.fetchEntries(fetchMap, user);
                if (entries != null) {
                    _logger.debug("Retrieved " + entries.length + " entries from category: " + blogCategoryName);
                    String deletedEntryTitle = entries[0].getTitle();
					entries[0].delete(user);

					fetchMap = new HashMap();
					fetchMap.put(BlojsomFetcher.FETCHER_FLAVOR, "html");
					fetchMap.put(BlojsomFetcher.FETCHER_NUM_POSTS_INTEGER, new Integer(blog.getBlogDisplayEntries()));
					entries = _fetcher.fetchEntries(fetchMap, user);
					
                    redirectWithMessage(httpServletResponse, "$message.deletedentry");
                } else {
                    _logger.debug("No entries found in category: " + blogCategoryName);
                }
            } catch (BlojsomFetcherException e) {
                _logger.error(e);
				redirectWithMessage(httpServletResponse, " message.unabletodeleteentry");
            } catch (BlojsomException e) {
                _logger.error(e);
				redirectWithMessage(httpServletResponse, " message.unabletodeleteentry");
            }
		}
		
		// and pick the correct stylesheet
		context.put(BLOG_SETTING_STYLESHEET, blog.getBlogDefaultStyleSheet());
		
        return entries;
    }
}
