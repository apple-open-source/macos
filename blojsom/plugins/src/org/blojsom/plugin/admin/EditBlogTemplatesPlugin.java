/**
 * Copyright (c) 2003-2004, David A. Czarnecki
 * All rights reserved.
 *
 * Portions Copyright (c) 2003-2004 by Mark Lussier
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
import java.util.ArrayList;
import java.util.Map;

/**
 * EditBlogTemplatesPlugin
 * 
 * @author czarnecki
 * @version $Id: EditBlogTemplatesPlugin.java,v 1.2 2004/08/27 01:06:35 whitmore Exp $
 * @since blojsom 2.04
 */
public class EditBlogTemplatesPlugin extends BaseAdminPlugin {

    private Log _logger = LogFactory.getLog(EditBlogTemplatesPlugin.class);

    // Pages
    private static final String EDIT_BLOG_TEMPLATES_PAGE = "/org/blojsom/plugin/admin/templates/admin-edit-blog-templates";
    private static final String EDIT_BLOG_TEMPLATE_PAGE = "/org/blojsom/plugin/admin/templates/admin-edit-blog-template";

    // Constants
    private static final String BLOJSOM_PLUGIN_EDIT_BLOG_TEMPLATES_TEMPLATE_FILES = "BLOJSOM_PLUGIN_EDIT_BLOG_TEMPLATES_TEMPLATE_FILES";
    private static final String BLOJSOM_PLUGIN_EDIT_BLOG_TEMPLATES_TEMPLATE_FILE = "BLOJSOM_PLUGIN_EDIT_BLOG_TEMPLATES_TEMPLATE_FILE";
    private static final String BLOJSOM_PLUGIN_EDIT_BLOG_TEMPLATES_TEMPLATE = "BLOJSOM_PLUGIN_EDIT_BLOG_TEMPLATES_TEMPLATE";

    // Actions
    private static final String EDIT_BLOG_TEMPLATES_ACTION = "edit-blog-template";
    private static final String UPDATE_BLOG_TEMPLATE_ACTION = "update-blog-template";

    // Form elements
    private static final String BLOG_TEMPLATE = "blog-template";
    private static final String BLOG_TEMPLATE_DATA = "blog-template-data";

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

        // Add list of templates to context
        File templatesDirectory = new File(_blojsomConfiguration.getInstallationDirectory() +
                BlojsomUtils.removeInitialSlash(_blojsomConfiguration.getBaseConfigurationDirectory()) +
                user.getId() + _blojsomConfiguration.getTemplatesDirectory());
        _logger.debug("Looking for templates in directory: " + templatesDirectory.toString());

        File[] templates = templatesDirectory.listFiles();
        ArrayList templatesList = new ArrayList(templates.length);
        for (int i = 0; i < templates.length; i++) {
            File template = templates[i];
            if (template.isFile()) {
                templatesList.add(template.getName());
                _logger.debug("Added template: " + template.getName());
            }
        }

        context.put(BLOJSOM_PLUGIN_EDIT_BLOG_TEMPLATES_TEMPLATE_FILES, templatesList);

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
                context.put(BLOJSOM_PLUGIN_EDIT_BLOG_TEMPLATES_TEMPLATE, template.toString());
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
            httpServletRequest.setAttribute(PAGE_PARAM, EDIT_BLOG_TEMPLATES_PAGE);
        }

        return entries;
    }
}
