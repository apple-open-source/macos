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
package org.blojsom.plugin.categories;

import org.blojsom.plugin.BlojsomPlugin;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlogCategory;
import org.blojsom.util.BlojsomConstants;
import org.blojsom.util.BlojsomUtils;
import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.util.Map;

/**
 * Category template plugin allows you to define a separate template for each category in your blog using
 * the <code>category.template</code> meta-data. In your category meta-data, include a
 * property called <code>category.template</code> which is set to the name of a template. If the template name starts with
 * a '/', blojsom will try to load the name of template using the extension of the template name for the
 * current flavor. If the template name does not start with a '/', blojsom will try to load the name of the
 * template for the current flavor + '-' + category page template name + extension of the template name
 * for the current flavor.
 * <p/>
 * For example, if your flavor.properties contains the following:
 * <p/>
 * <code>html=asual.vm, text/html;charset=UTF-8</code>
 * <p/>
 * If your category meta-data contains the following:
 * <p/>
 * <code>category.template=/another-page</code>
 * <p/>
 * blojsom will try to load <code>another-page.vm</code> located in the blog's <code>templates</code> directory.
 * <p/>
 * If your category meta-data contains the following:
 * <p/>
 * <code>category.template=another-page</code>
 * <p/>
 * blojsom will try to load <code>asual-another-page.vm</code> located in the blog's <code>templates</code> directory.  
 *
 * @author David Czarnecki
 * @version $Id: CategoryTemplatePlugin.java,v 1.1.2.1 2005/07/21 04:30:26 johnan Exp $
 * @since blojsom 2.23
 */
public class CategoryTemplatePlugin implements BlojsomPlugin, BlojsomConstants {

    private static final String CATEGORY_TEMPLATE_METADATA = "category.template";

    protected static Log _logger = LogFactory.getLog(CategoryTemplatePlugin.class);

    /**
     * Construct a new category template plugin
     */
    public CategoryTemplatePlugin() {
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
        _logger.debug("Initialized category template plugin");
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
     * @throws org.blojsom.plugin.BlojsomPluginException
     *          If there is an error processing the blog entries
     */
    public BlogEntry[] process(HttpServletRequest httpServletRequest, HttpServletResponse httpServletResponse, BlogUser user, Map context, BlogEntry[] entries) throws BlojsomPluginException {
        BlogCategory blogCategory = (BlogCategory) context.get(BLOJSOM_REQUESTED_CATEGORY);
        if (BlojsomUtils.checkMapForKey(blogCategory.getMetaData(), CATEGORY_TEMPLATE_METADATA)) {
            String pageForCategory = (String) blogCategory.getMetaData().get(CATEGORY_TEMPLATE_METADATA);
            pageForCategory = BlojsomUtils.normalize(pageForCategory);
            String currentPageParam = BlojsomUtils.getRequestValue(PAGE_PARAM, httpServletRequest);
            if (!BlojsomUtils.checkNullOrBlank(currentPageParam)) {
                pageForCategory += "-" + currentPageParam;
            }

            _logger.debug("Using template for category: " + pageForCategory);
            httpServletRequest.setAttribute(PAGE_PARAM, pageForCategory);
        }

        return entries;
    }

    /**
     * Perform any cleanup for the plugin. Called after {@link #process}.
     *
     * @throws org.blojsom.plugin.BlojsomPluginException
     *          If there is an error performing cleanup for this plugin
     */
    public void cleanup() throws BlojsomPluginException {
    }

    /**
     * Called when BlojsomServlet is taken out of service
     *
     * @throws org.blojsom.plugin.BlojsomPluginException
     *          If there is an error in finalizing this plugin
     */
    public void destroy() throws BlojsomPluginException {
    }
}