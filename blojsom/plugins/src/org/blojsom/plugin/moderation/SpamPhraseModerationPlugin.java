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
package org.blojsom.plugin.moderation;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.event.BlojsomEvent;
import org.blojsom.event.BlojsomListener;
import org.blojsom.plugin.BlojsomPlugin;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.plugin.comment.CommentModerationPlugin;
import org.blojsom.plugin.comment.CommentPlugin;
import org.blojsom.plugin.comment.event.CommentResponseSubmissionEvent;
import org.blojsom.plugin.response.event.ResponseSubmissionEvent;
import org.blojsom.plugin.trackback.TrackbackModerationPlugin;
import org.blojsom.plugin.trackback.TrackbackPlugin;
import org.blojsom.plugin.trackback.event.TrackbackResponseSubmissionEvent;
import org.blojsom.util.BlojsomConstants;
import org.blojsom.util.BlojsomUtils;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.*;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.Map;

/**
 * Spam phrase moderation plugin
 *
 * @author David Czarnecki
 * @since blojsom 2.25
 * @version $Id: SpamPhraseModerationPlugin.java,v 1.1.2.1 2005/07/21 04:30:35 johnan Exp $
 */
public class SpamPhraseModerationPlugin implements BlojsomPlugin, BlojsomListener {

    private Log _logger = LogFactory.getLog(SpamPhraseModerationPlugin.class);

    private static final String SPAM_PHRASE_BLACKLIST_IP = "spam-phrase-blacklist";
    private static final String DEFAULT_SPAM_PHRASE_BLACKLIST_FILE = "spam-phrase-blacklist.properties";
    private static final String DELETE_PHRASESPAM = "delete-phrasespam";
    private static final boolean DEFAULT_DELETE_PHRASESPAM = false;

    private BlojsomConfiguration _blojsomConfiguration;
    private String _spamPhraseBlacklist = DEFAULT_SPAM_PHRASE_BLACKLIST_FILE;

    /**
     * Create a new instance of the spam phrase moderation plugin
     */
    public SpamPhraseModerationPlugin() {
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

        _blojsomConfiguration = blojsomConfiguration;

        String spamPhraseBlacklist = servletConfig.getInitParameter(SPAM_PHRASE_BLACKLIST_IP);
        if (!BlojsomUtils.checkNullOrBlank(spamPhraseBlacklist)) {
            _spamPhraseBlacklist = spamPhraseBlacklist;
        }

        _logger.debug("Initialized spam phrase blacklist plugin");
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
        if (event instanceof ResponseSubmissionEvent) {
            ResponseSubmissionEvent responseSubmissionEvent = (ResponseSubmissionEvent) event;
            String content = responseSubmissionEvent.getContent();

            if (!BlojsomUtils.checkNullOrBlank(content)) {
                File blacklistFile = new File(_blojsomConfiguration.getInstallationDirectory() + _blojsomConfiguration.getBaseConfigurationDirectory() + "/" +
                        responseSubmissionEvent.getBlog().getId() + "/" + _spamPhraseBlacklist);
                if (blacklistFile.exists()) {
                    try {
                        FileInputStream fis = new FileInputStream(blacklistFile);
                        BufferedReader br = new BufferedReader(new InputStreamReader(fis,  BlojsomConstants.UTF8));
                        ArrayList spamPhrases = new ArrayList(25);
                        String phrase;

                        while ((phrase = br.readLine()) != null) {
                            spamPhrases.add(phrase);
                        }

                        br.close();

                        boolean deletePhraseSpam = DEFAULT_DELETE_PHRASESPAM;
                        Map metaData = responseSubmissionEvent.getMetaData();

                        String deletePhraseSpamValue = responseSubmissionEvent.getBlog().getBlog().getBlogProperty(DELETE_PHRASESPAM);
                        if (!BlojsomUtils.checkNullOrBlank(deletePhraseSpamValue)) {
                            deletePhraseSpam = Boolean.valueOf(deletePhraseSpamValue).booleanValue();
                        }

                        Iterator phraseIterator = spamPhrases.iterator();
                        boolean phraseSpamFound = false;
                        while (phraseIterator.hasNext()) {
                            phrase = (String) phraseIterator.next();
                            if (content.matches(phrase) || content.indexOf(phrase) != -1) {
                                phraseSpamFound = true;
                                break;
                            }
                        }

                        if (phraseSpamFound) {
                            if (!deletePhraseSpam) {
                                _logger.debug("Marking response for moderation");
                            } else{
                                _logger.debug("Marking response for automatic deletion");
                            }

                            if (responseSubmissionEvent instanceof CommentResponseSubmissionEvent) {
                                if (!deletePhraseSpam) {
                                    metaData.put(CommentModerationPlugin.BLOJSOM_COMMENT_MODERATION_PLUGIN_APPROVED, Boolean.FALSE.toString());
                                } else {
                                    metaData.put(CommentPlugin.BLOJSOM_PLUGIN_COMMENT_METADATA_DESTROY, Boolean.TRUE);
                                }
                            } else if (responseSubmissionEvent instanceof TrackbackResponseSubmissionEvent) {
                                if (!deletePhraseSpam) {
                                    metaData.put(TrackbackModerationPlugin.BLOJSOM_TRACKBACK_MODERATION_PLUGIN_APPROVED, Boolean.FALSE.toString());
                                } else {
                                    metaData.put(TrackbackPlugin.BLOJSOM_PLUGIN_TRACKBACK_METADATA_DESTROY, Boolean.TRUE);
                                }
                            }
                        }
                    } catch (IOException e) {
                        _logger.error(e);
                    }
                } else {
                    _logger.debug("Spam phrase blacklist not found: " + _spamPhraseBlacklist);
                }
            } else {
                _logger.debug("No content to evaluate for response");
            }
        }
    }
}