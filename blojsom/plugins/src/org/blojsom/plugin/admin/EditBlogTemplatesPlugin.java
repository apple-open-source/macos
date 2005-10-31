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

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.util.BlojsomConstants;
import org.blojsom.util.BlojsomUtils;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.*;
import java.util.*;

/**
 * EditBlogTemplatesPlugin
 *
 * @author czarnecki
 * @version $Id: EditBlogTemplatesPlugin.java,v 1.2.2.1 2005/07/21 04:30:24 johnan Exp $
 * @since blojsom 2.04
 */
public class EditBlogTemplatesPlugin extends BaseAdminPlugin {

    private Log _logger = LogFactory.getLog(EditBlogTemplatesPlugin.class);

    private static final String DEFAULT_ACCEPTED_TEMPLATE_EXTENSIONS = "vm";
    private static final String ACCEPTED_TEMPLATE_EXTENSIONS_INIT_PARAM = "accepted-template-extensions";

    // Pages
    private static final String EDIT_BLOG_TEMPLATES_PAGE = "/org/blojsom/plugin/admin/templates/admin-edit-blog-templates";
    private static final String EDIT_BLOG_TEMPLATE_PAGE = "/org/blojsom/plugin/admin/templates/admin-edit-blog-template";

    // Constants
    private static final String BLOJSOM_PLUGIN_EDIT_BLOG_TEMPLATES_TEMPLATE_FILES = "BLOJSOM_PLUGIN_EDIT_BLOG_TEMPLATES_TEMPLATE_FILES";
    private static final String BLOJSOM_PLUGIN_EDIT_BLOG_TEMPLATES_TEMPLATE_FILE = "BLOJSOM_PLUGIN_EDIT_BLOG_TEMPLATES_TEMPLATE_FILE";
    private static final String BLOJSOM_PLUGIN_EDIT_BLOG_TEMPLATES_TEMPLATE = "BLOJSOM_PLUGIN_EDIT_BLOG_TEMPLATES_TEMPLATE";
    private static final String BLOJSOM_PLUGIN_EDIT_BLOG_TEMPLATES_DIRECTORIES = "BLOJSOM_PLUGIN_EDIT_BLOG_TEMPLATES_DIRECTORIES";

    // Actions
    private static final String ADD_BLOG_TEMPLATE_ACTION = "add-blog-template";
    private static final String DELETE_BLOG_TEMPLATE_ACTION = "delete-blog-template";
    private static final String EDIT_BLOG_TEMPLATES_ACTION = "edit-blog-template";
    private static final String UPDATE_BLOG_TEMPLATE_ACTION = "update-blog-template";
    private static final String ADD_TEMPLATE_DIRECTORY_ACTION = "add-template-directory";
    private static final String DELETE_TEMPLATE_DIRECTORY_ACTION = "delete-template-directory";

    // Form elements
    private static final String BLOG_TEMPLATE = "blog-template";
    private static final String BLOG_TEMPLATE_DATA = "blog-template-data";
    private static final String BLOG_TEMPLATE_DIRECTORY = "blog-template-directory";
    private static final String TEMPLATE_DIRECTORY_TO_ADD = "template-directory-to-add";

    // Permissions
    private static final String EDIT_BLOG_TEMPLATES_PERMISSION = "edit_blog_templates";

    private Map _acceptedTemplateExtensions;

    /**
     * Default constructor.
     */
    public EditBlogTemplatesPlugin() {
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

        String acceptedTemplateExtensions = servletConfig.getInitParameter(ACCEPTED_TEMPLATE_EXTENSIONS_INIT_PARAM);
        if (BlojsomUtils.checkNullOrBlank(acceptedTemplateExtensions)) {
            acceptedTemplateExtensions = DEFAULT_ACCEPTED_TEMPLATE_EXTENSIONS;
        }

        _acceptedTemplateExtensions = new HashMap();
        String[] templateExtensions = BlojsomUtils.parseCommaList(acceptedTemplateExtensions);
        for (int i = 0; i < templateExtensions.length; i++) {
            String templateExtension = templateExtensions[i];
            _acceptedTemplateExtensions.put(templateExtension, templateExtension);
        }
    }

    /**
     * Sanitize a filename
     *
     * @param blogTemplate Blog template filename
     * @return Sanitized filename or <code>null</code> if error in sanitizing
     * @since blojsom 2.23
     */
    protected String sanitizeFilename(String blogTemplate) {
        String templateFilename = new File(blogTemplate).getName();
        int lastSeparator;
        blogTemplate = BlojsomUtils.normalize(blogTemplate);
        lastSeparator = blogTemplate.lastIndexOf(File.separator);
        if (lastSeparator == -1) {
            if (templateFilename != null) {
                return templateFilename;
            } else {
                return null;
            }
        } else {
            blogTemplate = blogTemplate.substring(0, lastSeparator + 1) + templateFilename;
        }

        return blogTemplate;
    }

    /**
     * Put the list of template files in the context
     *
     * @param templatesDirectory Templates directory
     * @param context Context
     * @since blojsom 2.23
     */
    protected void putTemplatesInContext(File templatesDirectory, Map context) {
        List templateFiles = new ArrayList();
        BlojsomUtils.listFilesInSubdirectories(templatesDirectory, templatesDirectory.getAbsolutePath(), templateFiles);
        File[] templates = (File[]) templateFiles.toArray(new File[templateFiles.size()]);
        Arrays.sort(templates);

        context.put(BLOJSOM_PLUGIN_EDIT_BLOG_TEMPLATES_TEMPLATE_FILES, templates);
    }

    /**
     * Put the list of template directories in the context
     *
     * @param templatesDirectory Templates directory
     * @param context Context
     * @since blojsom 2.23
     */
    protected void putTemplateDirectoriesInContext(File templatesDirectory, Map context) {
        List templateDirectories = new ArrayList();
        BlojsomUtils.listDirectoriesInSubdirectories(templatesDirectory, templatesDirectory.getAbsolutePath(), templateDirectories);
        File[] directories = (File[]) templateDirectories.toArray(new File[templateDirectories.size()]);
        Arrays.sort(directories);

        context.put(BLOJSOM_PLUGIN_EDIT_BLOG_TEMPLATES_DIRECTORIES, directories);
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
        if (!checkPermission(user, null, username, EDIT_BLOG_TEMPLATES_PERMISSION)) {
            httpServletRequest.setAttribute(PAGE_PARAM, ADMIN_LOGIN_PAGE);
            addOperationResultMessage(context, "You are not allowed to edit blog templates");

            return entries;
        }

        // Add list of templates to context
        File templatesDirectory = new File(_blojsomConfiguration.getInstallationDirectory() +
                BlojsomUtils.removeInitialSlash(_blojsomConfiguration.getBaseConfigurationDirectory()) +
                user.getId() + _blojsomConfiguration.getTemplatesDirectory());
        _logger.debug("Looking for templates in directory: " + templatesDirectory.toString());

        putTemplatesInContext(templatesDirectory, context);
        putTemplateDirectoriesInContext(templatesDirectory, context);

        String action = BlojsomUtils.getRequestValue(ACTION_PARAM, httpServletRequest);
        if (BlojsomUtils.checkNullOrBlank(action)) {
            _logger.debug("User did not request edit action");
            httpServletRequest.setAttribute(PAGE_PARAM, ADMIN_ADMINISTRATION_PAGE);
        } else if (PAGE_ACTION.equals(action)) {
            _logger.debug("User requested edit blog templates page");

            httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_TEMPLATES_PAGE);
        } else if (EDIT_BLOG_TEMPLATES_ACTION.equals(action)) {
            _logger.debug("User requested edit blog templates action");

            String blogTemplate = BlojsomUtils.getRequestValue(BLOG_TEMPLATE, httpServletRequest);
            if (BlojsomUtils.checkNullOrBlank(blogTemplate)) {
                httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_TEMPLATES_PAGE);

                return entries;
            }

            blogTemplate = sanitizeFilename(blogTemplate);
            if (blogTemplate == null) {
                addOperationResultMessage(context, "Invalid path specified");
                httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_TEMPLATES_PAGE);

                return entries;
            }

            File blogTemplateFile = new File(_blojsomConfiguration.getInstallationDirectory() +
                    BlojsomUtils.removeInitialSlash(_blojsomConfiguration.getBaseConfigurationDirectory()) +
                    user.getId() + _blojsomConfiguration.getTemplatesDirectory() + blogTemplate);
            _logger.debug("Reading template file: " + blogTemplateFile.toString());

            try {
                BufferedReader br = new BufferedReader(new InputStreamReader(new FileInputStream(blogTemplateFile), UTF8));
                String input;
                StringBuffer template = new StringBuffer();

                while ((input = br.readLine()) != null) {
                    template.append(input);
                    template.append(BlojsomConstants.LINE_SEPARATOR);
                }

                br.close();

                context.put(BLOJSOM_PLUGIN_EDIT_BLOG_TEMPLATES_TEMPLATE_FILE, blogTemplate);
                context.put(BLOJSOM_PLUGIN_EDIT_BLOG_TEMPLATES_TEMPLATE, BlojsomUtils.escapeString(template.toString()));
                httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_TEMPLATE_PAGE);
            } catch (UnsupportedEncodingException e) {
                _logger.error(e);
                addOperationResultMessage(context, "Unable to load blog template: " + blogTemplate);
                httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_TEMPLATES_PAGE);
            } catch (IOException e) {
                _logger.error(e);
                addOperationResultMessage(context, "Unable to load blog template: " + blogTemplate);
                httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_TEMPLATES_PAGE);
            }
        } else if (UPDATE_BLOG_TEMPLATE_ACTION.equals(action)) {
            _logger.debug("User requested update blog template action");

            String blogTemplate = BlojsomUtils.getRequestValue(BLOG_TEMPLATE, httpServletRequest);
            if (BlojsomUtils.checkNullOrBlank(blogTemplate)) {
                httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_TEMPLATES_PAGE);

                return entries;
            }

            blogTemplate = sanitizeFilename(blogTemplate);
            if (blogTemplate == null) {
                addOperationResultMessage(context, "Invalid path specified");
                httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_TEMPLATES_PAGE);

                return entries;
            }

            String blogTemplateData = BlojsomUtils.getRequestValue(BLOG_TEMPLATE_DATA, httpServletRequest);
            File blogTemplateFile = new File(_blojsomConfiguration.getInstallationDirectory() +
                    BlojsomUtils.removeInitialSlash(_blojsomConfiguration.getBaseConfigurationDirectory()) +
                    user.getId() + _blojsomConfiguration.getTemplatesDirectory() + blogTemplate);

            _logger.debug("Writing template file: " + blogTemplateFile.toString());

            try {
                BufferedWriter bw = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(blogTemplateFile), UTF8));
                bw.write(blogTemplateData);
                bw.close();
            } catch (UnsupportedEncodingException e) {
                _logger.error(e);
                addOperationResultMessage(context, "Unable to update blog template: " + blogTemplate);
            } catch (IOException e) {
                _logger.error(e);
                addOperationResultMessage(context, "Unable to update blog template: " + blogTemplate);
            }

            addOperationResultMessage(context, "Updated blog template: " + blogTemplate);

            context.put(BLOJSOM_PLUGIN_EDIT_BLOG_TEMPLATES_TEMPLATE_FILE, blogTemplate);
            context.put(BLOJSOM_PLUGIN_EDIT_BLOG_TEMPLATES_TEMPLATE, BlojsomUtils.escapeString(blogTemplateData));
            httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_TEMPLATE_PAGE);
        } else if (ADD_BLOG_TEMPLATE_ACTION.equals(action)) {
            _logger.debug("User requested add blog template action");

            String blogTemplate = BlojsomUtils.getRequestValue(BLOG_TEMPLATE, httpServletRequest);
            String blogTemplateDirectory = BlojsomUtils.getRequestValue(BLOG_TEMPLATE_DIRECTORY, httpServletRequest);

            if (BlojsomUtils.checkNullOrBlank(blogTemplate)) {
                addOperationResultMessage(context, "No template name specified");
                httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_TEMPLATES_PAGE);

                return entries;
            }

            String templateName = BlojsomUtils.getFilenameFromPath(blogTemplate);
            String templateExtension = BlojsomUtils.getFileExtension(templateName);

            if (!_acceptedTemplateExtensions.containsKey(templateExtension)) {
                addOperationResultMessage(context, "Invalid template extension: " + templateExtension);
                httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_TEMPLATES_PAGE);

                return entries;
            } else {
                blogTemplateDirectory = BlojsomUtils.normalize(blogTemplateDirectory);
                File addedTemplateDirectory = new File(templatesDirectory, blogTemplateDirectory);
                if (addedTemplateDirectory.exists()) {
                    context.put(BLOJSOM_PLUGIN_EDIT_BLOG_TEMPLATES_TEMPLATE_FILE, blogTemplateDirectory + File.separator + templateName);
                    context.put(BLOJSOM_PLUGIN_EDIT_BLOG_TEMPLATES_TEMPLATE, "");

                    httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_TEMPLATE_PAGE);
                } else {
                    addOperationResultMessage(context, "Specified template directory does not exist");
                }
            }
        } else if (ADD_TEMPLATE_DIRECTORY_ACTION.equals(action)) {
            _logger.debug("User requested add blog template directory action");

            String templateDirectoryToAdd = BlojsomUtils.getRequestValue(TEMPLATE_DIRECTORY_TO_ADD, httpServletRequest);
            String blogTemplateDirectory = BlojsomUtils.getRequestValue(BLOG_TEMPLATE_DIRECTORY, httpServletRequest);
            if (BlojsomUtils.checkNullOrBlank(templateDirectoryToAdd)) {
                addOperationResultMessage(context, "New template directory not specified");
            } else {
                blogTemplateDirectory = BlojsomUtils.normalize(blogTemplateDirectory);
                templateDirectoryToAdd = BlojsomUtils.normalize(templateDirectoryToAdd);

                File newTemplateDirectory = new File(templatesDirectory, blogTemplateDirectory + File.separator + templateDirectoryToAdd);
                _logger.debug("Adding blog template directory: " + newTemplateDirectory.toString());

                if (!newTemplateDirectory.mkdir()) {
                    addOperationResultMessage(context, "Unable to add new template directory: " + templateDirectoryToAdd);
                } else {
                    addOperationResultMessage(context, "Added template directory: " + templateDirectoryToAdd);

                    putTemplateDirectoriesInContext(templatesDirectory, context);
                }
            }

            httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_TEMPLATES_PAGE);
        } else if (DELETE_TEMPLATE_DIRECTORY_ACTION.equals(action)) {
            _logger.debug("User requested delete blog template directory action");

            String blogTemplateDirectory = BlojsomUtils.getRequestValue(BLOG_TEMPLATE_DIRECTORY, httpServletRequest);
            if (BlojsomUtils.checkNullOrBlank(blogTemplateDirectory)) {
                addOperationResultMessage(context, "You cannot remove root the top-level template directory");
            } else {
                blogTemplateDirectory = BlojsomUtils.normalize(blogTemplateDirectory);
                _logger.debug("Sanitized template directory: " + blogTemplateDirectory);
                File templateDirectoryToDelete = new File(templatesDirectory, blogTemplateDirectory);
                _logger.debug("Removing blog template directory: " + templateDirectoryToDelete);

                if (!BlojsomUtils.deleteDirectory(templateDirectoryToDelete, true)) {
                    addOperationResultMessage(context, "Unable to remove template directory: " + blogTemplateDirectory);
                } else {
                    addOperationResultMessage(context, "Removed template directory: " + blogTemplateDirectory);

                    putTemplateDirectoriesInContext(templatesDirectory, context);
                }
            }

            httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_TEMPLATES_PAGE);
        } else if (DELETE_BLOG_TEMPLATE_ACTION.equals(action)) {
             _logger.debug("User requested delete blog template directory action");

            String blogTemplate = BlojsomUtils.getRequestValue(BLOG_TEMPLATE, httpServletRequest);
            if (BlojsomUtils.checkNullOrBlank(blogTemplate)) {
                addOperationResultMessage(context, "Template to delete not specified");
            }

            blogTemplate = sanitizeFilename(blogTemplate);
            File templateToDelete = new File(templatesDirectory, blogTemplate);
            _logger.debug("Deleting blog template: " + templateToDelete.toString());

            if (!templateToDelete.delete()) {
                addOperationResultMessage(context, "Unable to delete template: " + blogTemplate);
            } else {
                addOperationResultMessage(context, "Deleted blog template: " + blogTemplate);

                putTemplatesInContext(templatesDirectory, context);
            }

            httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_TEMPLATES_PAGE);
        }

        return entries;
    }
}
