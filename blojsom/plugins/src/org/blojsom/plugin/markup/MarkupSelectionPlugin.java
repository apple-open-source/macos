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
package org.blojsom.plugin.markup;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.event.BlojsomEvent;
import org.blojsom.event.BlojsomListener;
import org.blojsom.plugin.BlojsomPlugin;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.plugin.admin.event.ProcessBlogEntryEvent;
import org.blojsom.util.BlojsomUtils;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.util.Collections;
import java.util.Iterator;
import java.util.Map;
import java.util.TreeMap;

/**
 * Markup selection plugin allows an individual to select a markup filter to apply
 * to their blog entry.
 *
 * @author David Czarnecki
 * @version $Id: MarkupSelectionPlugin.java,v 1.1.2.1 2005/07/21 04:30:33 johnan Exp $
 * @since blojsom 2.24
 */
public class MarkupSelectionPlugin implements BlojsomPlugin, BlojsomListener {

    private Log _logger = LogFactory.getLog(MarkupSelectionPlugin.class);

    private static final String PLUGIN_MARKUP_SELECTION_IP = "plugin-markup-selection";
    private static final String BLOJSOM_PLUGIN_MARKUP_SELECTIONS = "BLOJSOM_PLUGIN_MARKUP_SELECTIONS";
    private static final String MARKUP_SELECTION_TEMPLATE = "org/blojsom/plugin/markup/templates/admin-markup-selection-attachment.vm";
    private static final String MARKUP_SELECTIONS = "markup-selections";

    private Map _markupSelections;

    /**
     * Create a new instance of the markup selection plugin
     */
    public MarkupSelectionPlugin() {
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

        String markupSelection = servletConfig.getInitParameter(PLUGIN_MARKUP_SELECTION_IP);
        _markupSelections = new TreeMap();
        if (!BlojsomUtils.checkNullOrBlank(markupSelection)) {
            String[] markupTypes = BlojsomUtils.parseCommaList(markupSelection);
            for (int i = 0; i < markupTypes.length; i++) {
                String markupType = markupTypes[i];
                String[] markupNameAndKey = BlojsomUtils.parseDelimitedList(markupType, ":");
                if (markupNameAndKey != null && markupNameAndKey.length == 2) {
                    _markupSelections.put(markupNameAndKey[0], markupNameAndKey[1]);
                    _logger.debug("Added markup type and key: " + markupNameAndKey[0] + ":" + markupNameAndKey[1]);
                }
            }
        }

        _logger.debug("Initialized markup selection plugin");
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

            if (!_markupSelections.isEmpty()) {
                ProcessBlogEntryEvent processBlogEntryEvent = (ProcessBlogEntryEvent) event;
                Map templateAdditions = (Map) processBlogEntryEvent.getContext().get("BLOJSOM_TEMPLATE_ADDITIONS");
                if (templateAdditions == null) {
                    templateAdditions = new TreeMap();
                }

                templateAdditions.put(getClass().getName(), "#parse('" + MARKUP_SELECTION_TEMPLATE + "')");
                processBlogEntryEvent.getContext().put("BLOJSOM_TEMPLATE_ADDITIONS", templateAdditions);

                processBlogEntryEvent.getContext().put(BLOJSOM_PLUGIN_MARKUP_SELECTIONS, Collections.unmodifiableMap(_markupSelections));

                String[] markupSelections = processBlogEntryEvent.getHttpServletRequest().getParameterValues(MARKUP_SELECTIONS);
                BlogEntry entry = processBlogEntryEvent.getBlogEntry();

                if (markupSelections != null && markupSelections.length > 0) {
                    // Remove the markup selections if the user selections the blank option
                    if (markupSelections.length == 1 && "".equals(markupSelections[0])) {
                        Iterator markupSelectionsIterator = _markupSelections.values().iterator();
                        while (markupSelectionsIterator.hasNext()) {
                            entry.getMetaData().remove(markupSelectionsIterator.next().toString());
                        }
                    } else {
                        // Otherwise, set the new markup selections
                        for (int i = 0; i < markupSelections.length; i++) {
                            String markupSelection = markupSelections[i];
                            entry.getMetaData().put(markupSelection, Boolean.TRUE.toString());
                        }
                    }
                }
            } else {
                _logger.debug("No markup selections available");
            }
        }
    }
}