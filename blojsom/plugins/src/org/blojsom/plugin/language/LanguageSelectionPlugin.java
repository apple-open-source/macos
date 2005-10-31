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
package org.blojsom.plugin.language;

import org.blojsom.event.BlojsomListener;
import org.blojsom.event.BlojsomEvent;
import org.blojsom.plugin.BlojsomPlugin;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.plugin.admin.event.ProcessBlogEntryEvent;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.util.BlojsomUtils;
import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.util.Map;
import java.util.TreeMap;

/**
 * Language selection plugin allows you to attach a language attribute to a blog entry.
 *
 * @author David Czarnecki
 * @since blojsom 2.24
 * @version $Id: LanguageSelectionPlugin.java,v 1.1.2.1 2005/07/21 04:30:32 johnan Exp $
 */
public class LanguageSelectionPlugin implements BlojsomPlugin, BlojsomListener {

    private Log _logger = LogFactory.getLog(LanguageSelectionPlugin.class);

    private static final String LANGUAGE_SELECTION_TEMPLATE = "org/blojsom/plugin/language/templates/admin-language-selection.vm";

    private static final String BLOJSOM_JVM_LANGUAGES = "BLOJSOM_JVM_LANGUAGES";
    private static final String BLOJSOM_PLUGIN_CURRENT_LANGUAGE_SELECTION = "BLOJSOM_PLUGIN_CURRENT_LANGUAGE_SELECTION";
    private static final String METADATA_LANGUAGE = "language";

    /**
     * Create a new instance of the language selection plugin
     */
    public LanguageSelectionPlugin() {
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
        blojsomConfiguration.getEventBroadcaster().addListener(this);
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

    /**
     * Handle an event broadcast from another component
     *
     * @param event {@link org.blojsom.event.BlojsomEvent} to be handled
     */
    public void handleEvent(BlojsomEvent event) {
    }

    /**
     * Process an event from another component
     *
     * @param event {@link org.blojsom.event.BlojsomEvent} to be handled
     * @since blojsom 2.24
     */
    public void processEvent(BlojsomEvent event) {
        if (event instanceof ProcessBlogEntryEvent) {
            _logger.debug("Handling process blog entry event");
            ProcessBlogEntryEvent processBlogEntryEvent = (ProcessBlogEntryEvent) event;

            String language = BlojsomUtils.getRequestValue(METADATA_LANGUAGE, processBlogEntryEvent.getHttpServletRequest());
            Map context = processBlogEntryEvent.getContext();

            Map templateAdditions = (Map) processBlogEntryEvent.getContext().get("BLOJSOM_TEMPLATE_ADDITIONS");
            if (templateAdditions == null) {
                templateAdditions = new TreeMap();
            }

            templateAdditions.put(getClass().getName(), "#parse('" + LANGUAGE_SELECTION_TEMPLATE + "')");
            processBlogEntryEvent.getContext().put("BLOJSOM_TEMPLATE_ADDITIONS", templateAdditions);

            context.put(BLOJSOM_JVM_LANGUAGES, BlojsomUtils.getLanguagesForSystem(processBlogEntryEvent.getBlogUser().getBlog().getBlogAdministrationLocale()));

            // Preserve the current language selection if none submitted
            if (processBlogEntryEvent.getBlogEntry() != null) {
                String currentLanguage = (String) processBlogEntryEvent.getBlogEntry().getMetaData().get(METADATA_LANGUAGE);
                _logger.debug("Current language: " + currentLanguage);
                processBlogEntryEvent.getContext().put(BLOJSOM_PLUGIN_CURRENT_LANGUAGE_SELECTION, currentLanguage);
            }

            if (!BlojsomUtils.checkNullOrBlank(language)) {
                processBlogEntryEvent.getBlogEntry().getMetaData().put(METADATA_LANGUAGE, language);
                processBlogEntryEvent.getContext().put(BLOJSOM_PLUGIN_CURRENT_LANGUAGE_SELECTION, language);
                _logger.debug("Added/updated language: " + language);
            }
        }
    }
}