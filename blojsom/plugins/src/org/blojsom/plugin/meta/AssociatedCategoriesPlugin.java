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
package org.blojsom.plugin.meta;

import org.blojsom.plugin.BlojsomPlugin;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.blog.*;
import org.blojsom.util.BlojsomUtils;
import org.blojsom.BlojsomException;
import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.util.Map;

/**
 * AssociatedCategoriesPlugin
 *
 * @author David Czarnecki
 * @version $Id: AssociatedCategoriesPlugin.java,v 1.2 2004/08/27 01:06:39 whitmore Exp $
 * @since blojsom 1.9.6
 */
public class AssociatedCategoriesPlugin implements BlojsomPlugin {

    private Log _logger = LogFactory.getLog(AssociatedCategoriesPlugin.class);

    private static final String META_DATA_CATEGORIES_KEY = "categories";

    public static final String BLOJSOM_PLUGIN_ASSOCIATED_CATEGORIES = "BLOJSOM_PLUGIN_ASSOCIATED_CATEGORIES";

    /**
     * Default constructor
     */
    public AssociatedCategoriesPlugin() {
    }

    /**
     * Initialize this plugin. This method only called when the plugin is instantiated.
     *
     * @param servletConfig Servlet config object for the plugin to retrieve any initialization parameters
     * @param blojsomConfiguration {@link org.blojsom.blog.BlojsomConfiguration} information
     * @throws BlojsomPluginException If there is an error initializing the plugin
     */
    public void init(ServletConfig servletConfig, BlojsomConfiguration blojsomConfiguration) throws BlojsomPluginException {
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
     * @throws BlojsomPluginException If there is an error processing the blog entries
     */
    public BlogEntry[] process(HttpServletRequest httpServletRequest,
                               HttpServletResponse httpServletResponse,
                               BlogUser user,
                               Map context,
                               BlogEntry[] entries) throws BlojsomPluginException {
        BlogEntry entry;
        Map metadata;
        Blog blog = user.getBlog();

        for (int i = 0; i < entries.length; i++) {
            entry = entries[i];
            metadata = entry.getMetaData();
            if (metadata != null) {
                String[] associatedCategoriesList;
                if (metadata.containsKey(META_DATA_CATEGORIES_KEY)) {
                    associatedCategoriesList = BlojsomUtils.parseCommaList((String) metadata.get(META_DATA_CATEGORIES_KEY));
                    BlogCategory[] associatedCategories = new BlogCategory[associatedCategoriesList.length];
                    for (int j = 0; j < associatedCategoriesList.length; j++) {
                        String associatedCategory = associatedCategoriesList[j];
                        associatedCategory = BlojsomUtils.normalize(associatedCategory);
                        associatedCategory = BlojsomUtils.removeInitialSlash(associatedCategory);
                        if (!associatedCategory.endsWith("/")) {
                            associatedCategory += "/";
                        }
                        FileBackedBlogCategory fbbc = new FileBackedBlogCategory(associatedCategory, blog.getBlogURL() + associatedCategory);
                        try {
                            fbbc.load(blog);
                        } catch (BlojsomException e) {
                            _logger.error(e);
                        }
                        associatedCategories[j] = fbbc;
                    }
                    metadata.put(BLOJSOM_PLUGIN_ASSOCIATED_CATEGORIES, associatedCategories);
                    entry.setMetaData(metadata);
                }
            }
        }

        return entries;
    }

    /**
     * Perform any cleanup for the plugin. Called after {@link #process}.
     *
     * @throws BlojsomPluginException If there is an error performing cleanup for this plugin
     */
    public void cleanup() throws BlojsomPluginException {
    }

    /**
     * Called when BlojsomServlet is taken out of service
     *
     * @throws BlojsomPluginException If there is an error in finalizing this plugin
     */
    public void destroy() throws BlojsomPluginException {
    }
}
