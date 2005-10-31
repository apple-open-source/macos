/**
 * Copyright (c) 2003-2005, David A. Czarnecki
 * All rights reserved.
 *
 * Portions Copyright (c) 2003-2005 by Mark Lussier
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 * Neither the name of the "David A. Czarnecki" and "blojsom" nor the names of
 * its contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * Products derived from this software may not be called "blojsom",
 * nor may "blojsom" appear in their name, without prior written permission of
 * David A. Czarnecki.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
package org.blojsom.plugin.admin;

import org.apache.commons.fileupload.DiskFileUpload;
import org.apache.commons.fileupload.FileItem;
import org.apache.commons.fileupload.FileUploadException;
import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.BlojsomException;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.util.BlojsomUtils;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.File;
import java.util.*;

/**
 * FileUploadPlugin
 *
 * @author czarnecki
 * @version $Id: FileUploadPlugin.java,v 1.2.2.2 2005/07/29 22:13:29 johnan Exp $
 * @since blojsom 2.05
 */
public class FileUploadPlugin extends BaseAdminPlugin {

    private Log _logger = LogFactory.getLog(FileUploadPlugin.class);

    private static final String PLUGIN_ADMIN_UPLOAD_IP = "plugin-admin-upload";
    private static final String TEMPORARY_DIRECTORY_IP = "temporary-directory";
    private static final String DEFAULT_TEMPORARY_DIRECTORY = "/tmp";
    private static final String MAXIMUM_UPLOAD_SIZE_IP = "maximum-upload-size";
    private static final long DEFAULT_MAXIMUM_UPLOAD_SIZE = 100000;
    private static final String MAXIMUM_MEMORY_SIZE_IP = "maximum-memory-size";
    private static final int DEFAULT_MAXIMUM_MEMORY_SIZE = 50000;
    private static final String ACCEPTED_FILE_TYPES_IP = "accepted-file-types";
    private static final String[] DEFAULT_ACCEPTED_FILE_TYPES = {"image/jpeg", "image/gif", "image/png"};
    private static final String DEFAULT_RESOURCES_DIRECTORY = "/blojsom_resources/meta/";
    private static final String INVALID_FILE_EXTENSIONS_IP = "invalid-file-extensions";
    private static final String[] DEFAULT_INVALID_FILE_EXTENSIONS = {".jsp", ".jspf", ".jspi", ".jspx", ".php", ".cgi"};

    // Pages
    private static final String FILE_UPLOAD_PAGE = "/org/blojsom/plugin/admin/templates/admin-file-upload";

    // Constants
    private static final String PLUGIN_ADMIN_FILE_UPLOAD_FILES = "PLUGIN_ADMIN_FILE_UPLOAD_FILES";

    // Actions
    private static final String UPLOAD_FILE_ACTION = "upload-file";
    private static final String DELETE_UPLOAD_FILES = "delete-upload-files";

    // Form items
    private static final String FILE_TO_DELETE = "file-to-delete";

    // Permissions
    protected static final String FILE_UPLOAD_PERMISSION = "file_upload";

    protected String _temporaryDirectory;
    protected long _maximumUploadSize;
    protected int _maximumMemorySize;
    protected Map _acceptedFileTypes;
    protected String _resourcesDirectory;
    protected String[] _invalidFileExtensions;

    /**
     * Default constructor.
     */
    public FileUploadPlugin() {
    }

    /**
     * Initialize this plugin. This method only called when the plugin is instantiated.
     *
     * @param servletConfig        Servlet config object for the plugin to retrieve any initialization parameters
     * @param blojsomConfiguration {@link org.blojsom.blog.BlojsomConfiguration} information
     * @throws org.blojsom.plugin.BlojsomPluginException
     *          If there is an error initializing the plugin
     */
    public void init(ServletConfig servletConfig, BlojsomConfiguration blojsomConfiguration) throws BlojsomPluginException {
        super.init(servletConfig, blojsomConfiguration);

        try {
            Properties configurationProperties = BlojsomUtils.loadProperties(servletConfig, PLUGIN_ADMIN_UPLOAD_IP, true);
            _temporaryDirectory = configurationProperties.getProperty(TEMPORARY_DIRECTORY_IP);
            if (BlojsomUtils.checkNullOrBlank(_temporaryDirectory)) {
                _temporaryDirectory = DEFAULT_TEMPORARY_DIRECTORY;
            }
            _logger.debug("Using temporary directory: " + _temporaryDirectory);

            try {
                _maximumUploadSize = Long.parseLong(configurationProperties.getProperty(MAXIMUM_UPLOAD_SIZE_IP));
            } catch (NumberFormatException e) {
                _maximumUploadSize = DEFAULT_MAXIMUM_UPLOAD_SIZE;
            }
            _logger.debug("Using maximum upload size: " + _maximumUploadSize);

            try {
                _maximumMemorySize = Integer.parseInt(configurationProperties.getProperty(MAXIMUM_MEMORY_SIZE_IP));
            } catch (NumberFormatException e) {
                _maximumMemorySize = DEFAULT_MAXIMUM_MEMORY_SIZE;
            }
            _logger.debug("Using maximum memory size: " + _maximumMemorySize);

            String acceptedFileTypes = configurationProperties.getProperty(ACCEPTED_FILE_TYPES_IP);
            String[] parsedListOfTypes;
            if (BlojsomUtils.checkNullOrBlank(acceptedFileTypes)) {
                parsedListOfTypes = DEFAULT_ACCEPTED_FILE_TYPES;
            } else {
                parsedListOfTypes = BlojsomUtils.parseCommaList(acceptedFileTypes);
            }
            _acceptedFileTypes = new HashMap(parsedListOfTypes.length);
            for (int i = 0; i < parsedListOfTypes.length; i++) {
                String type = parsedListOfTypes[i];
                _acceptedFileTypes.put(type, type);
            }
            _logger.debug("Using accepted file types: " + BlojsomUtils.arrayOfStringsToString(parsedListOfTypes));

            _resourcesDirectory = _blojsomConfiguration.getResourceDirectory();
            if (BlojsomUtils.checkNullOrBlank(_resourcesDirectory)) {
                _resourcesDirectory = DEFAULT_RESOURCES_DIRECTORY;
            }

            _resourcesDirectory = BlojsomUtils.checkStartingAndEndingSlash(_resourcesDirectory);
            _logger.debug("Using resources directory: " + _resourcesDirectory);

            String invalidFileExtensionsProperty = configurationProperties.getProperty(INVALID_FILE_EXTENSIONS_IP);
            if (BlojsomUtils.checkNullOrBlank(invalidFileExtensionsProperty)) {
                _invalidFileExtensions = DEFAULT_INVALID_FILE_EXTENSIONS;
            } else {
                _invalidFileExtensions = BlojsomUtils.parseCommaList(invalidFileExtensionsProperty);
            }
            _logger.debug("Using invalid file extensions: " + invalidFileExtensionsProperty);
        } catch (BlojsomException e) {
            _logger.error(e);
            throw new BlojsomPluginException(e);
        }
    }

    /**
     * Process the blog entries
     *
     * @param httpServletRequest  Request
     * @param httpServletResponse Response
     * @param user                {@link org.blojsom.blog.BlogUser} instance
     * @param context             Context
     * @param entries             Blog entries retrieved for the particular request
     * @return Modified set of blog entries
     * @throws BlojsomPluginException If there is an error processing the blog entries
     */
    public BlogEntry[] process(HttpServletRequest httpServletRequest, HttpServletResponse httpServletResponse, BlogUser user, Map context, BlogEntry[] entries) throws BlojsomPluginException {
        if (!authenticateUser(httpServletRequest, httpServletResponse, context, user)) {
            httpServletRequest.setAttribute(PAGE_PARAM, ADMIN_LOGIN_PAGE);

            return entries;
        }

        String username = getUsernameFromSession(httpServletRequest, user.getBlog());
        if (!checkPermission(user, null, username, FILE_UPLOAD_PERMISSION)) {
            httpServletRequest.setAttribute(PAGE_PARAM, ADMIN_LOGIN_PAGE);
            addOperationResultMessage(context, "You are not allowed to upload files");

            return entries;
        }

        File resourceDirectory = new File(_blojsomConfiguration.getInstallationDirectory() + _resourcesDirectory + user.getId() + "/");

        String action = BlojsomUtils.getRequestValue(ACTION_PARAM, httpServletRequest);
        if (BlojsomUtils.checkNullOrBlank(action)) {
            _logger.debug("User did not request edit action");

            httpServletRequest.setAttribute(PAGE_PARAM, ADMIN_ADMINISTRATION_PAGE);
        } else if (PAGE_ACTION.equals(action)) {
            _logger.debug("User requested file upload page");

            httpServletRequest.setAttribute(PAGE_PARAM, FILE_UPLOAD_PAGE);
        } else if (UPLOAD_FILE_ACTION.equals(action)) {
            _logger.debug("User requested file upload action");

            // Create a new disk file upload and set its parameters
            DiskFileUpload diskFileUpload = new DiskFileUpload();
            diskFileUpload.setRepositoryPath(_temporaryDirectory);
            diskFileUpload.setSizeThreshold(_maximumMemorySize);
            diskFileUpload.setSizeMax(_maximumUploadSize);

            try {
                List items = diskFileUpload.parseRequest(httpServletRequest);
                Iterator itemsIterator = items.iterator();
                while (itemsIterator.hasNext()) {
                    FileItem item = (FileItem) itemsIterator.next();

                    // Check for the file upload form item
                    if (!item.isFormField()) {
                        String itemNameWithoutPath = BlojsomUtils.getFilenameFromPath(item.getName());

                        _logger.debug("Found file item: " + itemNameWithoutPath + " of type: " + item.getContentType());

                        // Is it one of the accepted file types?
                        String fileType = item.getContentType();
                        boolean isAcceptedFileType = _acceptedFileTypes.containsKey(fileType);

                        String extension = BlojsomUtils.getFileExtension(itemNameWithoutPath);
                        boolean isAcceptedFileExtension = true;
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
                                    addOperationResultMessage(context, "Unable to create resource directory");
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
                            }
                            _logger.debug("Successfully uploaded resource file: " + resourceFile.toString());
                            addOperationResultMessage(context, "Successfully upload resource file: " + item.getName());
                        } else {
                            if (!isAcceptedFileExtension) {
                                _logger.error("Upload file does not have an accepted extension: " + extension);
                                addOperationResultMessage(context, "Upload file does not have an accepted extension: " + extension);
                            } else {
                                _logger.error("Upload file is not an accepted type: " + item.getName() + " of type: " + item.getContentType());
                                addOperationResultMessage(context, "Upload file is not an accepted type: " + item.getName() + " of type: " + item.getContentType());
                            }
                        }
                    }
                }
            } catch (FileUploadException e) {
                _logger.error(e);
                addOperationResultMessage(context, "Unknown error in file upload: " + e.getMessage());
            }

            httpServletRequest.setAttribute(PAGE_PARAM, FILE_UPLOAD_PAGE);
        } else if (DELETE_UPLOAD_FILES.equals(action)) {
            String[] filesToDelete = httpServletRequest.getParameterValues(FILE_TO_DELETE);
            if (filesToDelete != null && filesToDelete.length > 0) {
                File deletedFile;
                for (int i = 0; i < filesToDelete.length; i++) {
                    String fileToDelete = filesToDelete[i];
                    deletedFile = new File(resourceDirectory, fileToDelete);
                    if (!deletedFile.delete()) {
                        _logger.debug("Unable to delete resource file: " + deletedFile.toString());
                    }
                }

                addOperationResultMessage(context, "Deleted " + filesToDelete.length + " file(s) from resources directory");
            }

            httpServletRequest.setAttribute(PAGE_PARAM, FILE_UPLOAD_PAGE);
        }

        // Create a list of files in the user's resource directory
        Map resourceFilesMap = null;
        if (resourceDirectory.exists()) {
            File[] resourceFiles = resourceDirectory.listFiles();

            if (resourceFiles != null) {
                resourceFilesMap = new HashMap(resourceFiles.length);
                for (int i = 0; i < resourceFiles.length; i++) {
                    File resourceFile = resourceFiles[i];
                    resourceFilesMap.put(resourceFile.getName(), resourceFile.getName());
                }
            }
        } else {
            resourceFilesMap = new HashMap();
        }

        resourceFilesMap = new TreeMap(resourceFilesMap);
        context.put(PLUGIN_ADMIN_FILE_UPLOAD_FILES, resourceFilesMap);

        return entries;
    }
}
