/**
 * Contains:   Podcast upload plug-in for blojsom.
 * Written by: John Anderson (for addtl writers check CVS comments).
 * Copyright:  © 2005 Apple Computer, Inc., all rights reserved.
 * Note:       When editing this file set PB to "Editor uses tabs/width=4".
 *
 */ 
package com.apple.blojsom.plugin.podcastupload;

import org.apache.commons.fileupload.DiskFileUpload;
import org.apache.commons.fileupload.FileItem;
import org.apache.commons.fileupload.FileUploadException;
import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.util.BlojsomConstants;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogComment;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.plugin.BlojsomPlugin;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.plugin.admin.FileUploadPlugin;
import org.blojsom.plugin.common.RSSEnclosurePlugin;
import org.blojsom.util.BlojsomUtils;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.File;
import java.io.FileOutputStream;
import java.io.UnsupportedEncodingException;
import java.util.*;

/**
 * Podcast Upload plug-in
 *
 * @author John Anderson
 * @version $Id: PodcastUploadPlugin.java,v 1.7.2.4 2005/09/23 20:57:10 johnan Exp $
 */

public class PodcastUploadPlugin extends FileUploadPlugin implements BlojsomConstants {

    private Log _logger = LogFactory.getLog(PodcastUploadPlugin.class);

    private static final String ADD_BLOG_ENTRY_ACTION = "add-blog-entry";
    private static final String DELETE_BLOG_ENTRY_ACTION = "delete-blog-entry";
	private static final String RSS_ENCLOSURE = "rss_enclosure";
	private static final String STOP_ADMIN_PARAM = "stop-admin";
	private static final String PODCAST_BLOG_PROPERTY = "podcast-blog";

    private String _baseConfigurationDirectory;
	private String _installationDirectory;

    /**
     * Default constructor.
     */
    public PodcastUploadPlugin() {
    }
	
    /**
     * Initialize this plugin. This method only called when the plugin is instantiated.
     *
     * @param servletConfig Servlet config object for the plugin to retrieve any initialization parameters
     * @param blojsomConfiguration {@link org.blojsom.blog.BlojsomConfiguration} information
     * @throws BlojsomPluginException If there is an error initializing the plugin
     */
    public void init(ServletConfig servletConfig, BlojsomConfiguration blojsomConfiguration) throws BlojsomPluginException {
		super.init(servletConfig, blojsomConfiguration);
        _baseConfigurationDirectory = blojsomConfiguration.getBaseConfigurationDirectory();
		_installationDirectory = blojsomConfiguration.getInstallationDirectory();
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
		// bail if this isn't multipart content
		if (!DiskFileUpload.isMultipartContent(httpServletRequest)) {
			return entries;
		}
	
		// make sure they can upload files (with the Apple auth provider, the answer is always yes)
        String username = getUsernameFromSession(httpServletRequest, user.getBlog());
        if (!checkPermission(user, null, username, FILE_UPLOAD_PERMISSION)) {
            addOperationResultMessage(context, " message.nouploadperms");
			context.put(STOP_ADMIN_PARAM, "true");
            return entries;
        }
				
		// find out what they're up to
        String action = BlojsomUtils.getRequestValue(ACTION_PARAM, httpServletRequest);
        if (BlojsomUtils.checkNullOrBlank(action)) {
			return entries;
		} else if (ADD_BLOG_ENTRY_ACTION.equals(action)) {
			_logger.debug("User requested file upload action");
			File resourceDirectory = new File(_blojsomConfiguration.getInstallationDirectory() + _resourcesDirectory + user.getId() + "/");
			_logger.debug("resourceDirectory = " + resourceDirectory);
			
			// Create a new servlet file upload and set its parameters
			DiskFileUpload diskFileUpload = new DiskFileUpload();
			diskFileUpload.setRepositoryPath(_temporaryDirectory);
			diskFileUpload.setSizeThreshold(_maximumMemorySize);
			diskFileUpload.setSizeMax(_maximumUploadSize);
			
			_logger.debug("created diskFileUpload");
			
			try {
				List items = diskFileUpload.parseRequest(httpServletRequest);
				Integer itemCount = new Integer(items.size());
				Iterator itemsIterator = items.iterator();
				_logger.debug("got list with " + itemCount.toString() + " items");
				while (itemsIterator.hasNext()) {
					FileItem item = (FileItem) itemsIterator.next();
					
					// Check for the file upload form item
					if (!item.isFormField() && authenticateUser(httpServletRequest, httpServletResponse, context, user) && !BlojsomUtils.checkNullOrBlank(item.getName())) {
						_logger.debug("found item that's not a form field");
						// the string was imported as Unicode; re-import as UTF-8
						String convItemName = item.getName();
						try {
							convItemName = new String(convItemName.getBytes(), "UTF-8");
						} catch (java.io.UnsupportedEncodingException e) {
							_logger.error(e);
						}
						String itemNameWithoutPath = BlojsomUtils.getFilenameFromPath(convItemName);
						
						_logger.debug("Found file item: " + itemNameWithoutPath + " of type: " + item.getContentType());
						
						// Is it one of the accepted file types?
						String extension = BlojsomUtils.getFileExtension(itemNameWithoutPath);
						boolean isAcceptedFileExtension = true;

						String fileType = item.getContentType();
						boolean isAcceptedFileType = _acceptedFileTypes.containsKey(fileType);
						
						// special-case m4a files since Safari doesn't define a MIME type for them
						if (extension.equals("m4a") && fileType.equals("application/octet-stream")) {
							fileType = "audio/x-m4a";
							isAcceptedFileType = _acceptedFileTypes.containsKey(fileType);
						}
						
                        for (int i = 0; i < _invalidFileExtensions.length; i++) {
                            String invalidFileExtension = _invalidFileExtensions[i];
                            if (itemNameWithoutPath.indexOf(invalidFileExtension) != -1) {
                                isAcceptedFileExtension = false;
                                break;
                            }
                        }

                        // If so, upload the file to the resources directory
                        if (isAcceptedFileType && isAcceptedFileExtension) {
                            if (!resourceDirectory.exists()) {
                                if (!resourceDirectory.mkdirs()) {
                                    _logger.error("Unable to create resource directory for user: " + resourceDirectory.toString());
                                    addOperationResultMessage(context, " message.unabletocreateresourcedir");
									context.put(STOP_ADMIN_PARAM, "true");
									return entries;
                                }
                            }

                            File resourceFile = new File(_blojsomConfiguration.getInstallationDirectory() +
                                    _resourcesDirectory + user.getId() + "/" + itemNameWithoutPath);
                            try {
                                item.write(resourceFile);
                            } catch (Exception e) {
                                _logger.error(e);
                                addOperationResultMessage(context, "Unknown error in file upload: " + e.getMessage());
								context.put(STOP_ADMIN_PARAM, "true");
								return entries;
                            }
                            _logger.debug("Successfully uploaded resource file: " + resourceFile.toString());
							context.put(RSSEnclosurePlugin.METADATA_RSS_ENCLOSURE, convItemName);
							
							// flag the blog as a podcast
							try {
								if ((user.getBlog().getBlogProperty(PODCAST_BLOG_PROPERTY) == null) || (!user.getBlog().getBlogProperty(PODCAST_BLOG_PROPERTY).equals("true"))) {
									// mark this user's blog as a podcast...
									user.getBlog().setBlogProperty(PODCAST_BLOG_PROPERTY, "true");
									
									// Write out new blog properties
									Properties blogProperties = BlojsomUtils.mapToProperties(user.getBlog().getBlogProperties(), UTF8);
									File propertiesFile = new File(_installationDirectory 
											+ BlojsomUtils.removeInitialSlash(_baseConfigurationDirectory) +
											"/" + user.getId() + "/" + BlojsomConstants.BLOG_DEFAULT_PROPERTIES);

									_logger.debug("Writing blog properties to: " + propertiesFile.toString());
								
									FileOutputStream fos = new FileOutputStream(propertiesFile);
									blogProperties.store(fos, null);
									fos.close();
								}
							} catch (java.io.IOException e) {
								_logger.error(e);
							}
                        } else {
                            if (!isAcceptedFileExtension) {
                                _logger.error("Upload file does not have an accepted extension: " + extension);
                                addOperationResultMessage(context, " message.badextension");
                            } else {
                                _logger.error("Upload file is not an accepted type: " + item.getName() + " of type: " + item.getContentType());
                                addOperationResultMessage(context, " message.badmimetype");
                            }
                        }
					} else {
						String attributeValue = item.getString();
						try {
							attributeValue = item.getString(UTF8);
						} catch (UnsupportedEncodingException e) {
							_logger.error(e);
						}
						httpServletRequest.setAttribute(item.getFieldName(), attributeValue);
					}
				}
            } catch (FileUploadException e) {
                _logger.error(e);
                addOperationResultMessage(context, "Unknown error in file upload: " + e.getMessage());
				context.put(STOP_ADMIN_PARAM, "true");
				return entries;
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
