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
package org.blojsom.plugin.moderation.admin;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.plugin.admin.WebAdminPlugin;
import org.blojsom.util.BlojsomConstants;
import org.blojsom.util.BlojsomUtils;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.*;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

/**
 * Spam phrase moderation administration plugin
 *
 * @author David Czarnecki
 * @version $Id: SpamPhraseModerationAdminPlugin.java,v 1.1.2.1 2005/07/21 04:30:36 johnan Exp $
 * @since blojsom 2.25
 */
public class SpamPhraseModerationAdminPlugin extends WebAdminPlugin {

    private Log _logger = LogFactory.getLog(SpamPhraseModerationAdminPlugin.class);

    private static final String SPAM_PHRASE_BLACKLIST_IP = "spam-phrase-blacklist";
    private static final String DEFAULT_SPAM_PHRASE_BLACKLIST_FILE = "spam-phrase-blacklist.properties";
    private String _spamPhraseBlacklist = DEFAULT_SPAM_PHRASE_BLACKLIST_FILE;

    // Context
    private static final String BLOJSOM_PLUGIN_SPAM_PHRASES = "BLOJSOM_PLUGIN_SPAM_PHRASES";

    // Pages
    private static final String EDIT_SPAM_PHRASE_MODERATION_SETTINGS_PAGE = "/org/blojsom/plugin/moderation/admin/templates/admin-edit-spam-phrase-moderation-settings";

    // Form itmes
    private static final String SPAM_PHRASE = "spam-phrase";

    // Actions
    private static final String ADD_SPAM_PHRASE_ACTION = "add-spam-phrase";
    private static final String DELETE_SPAM_PHRASE_ACTION = "delete-spam-phrase";

    // Permissions
    private static final String SPAM_PHRASE_MODERATION_PERMISSION = "spam_phrase_moderation";

    /**
     * Create a new instance of the spam phrase moderation administration plugin
     */
    public SpamPhraseModerationAdminPlugin() {
    }

    /**
     * Return the display name for the plugin
     *
     * @return Display name for the plugin
     */
    public String getDisplayName() {
        return "Spam Phrase Moderation plugin";
    }

    /**
     * Return the name of the initial editing page for the plugin
     *
     * @return Name of the initial editing page for the plugin
     */
    public String getInitialPage() {
        return EDIT_SPAM_PHRASE_MODERATION_SETTINGS_PAGE;
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

        String spamPhraseBlacklist = servletConfig.getInitParameter(SPAM_PHRASE_BLACKLIST_IP);
        if (!BlojsomUtils.checkNullOrBlank(spamPhraseBlacklist)) {
            _spamPhraseBlacklist = spamPhraseBlacklist;
        }

        _logger.debug("Initialized spam phrase blacklist administration plugin");
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
        entries = super.process(httpServletRequest, httpServletResponse, user, context, entries);

        String page = BlojsomUtils.getRequestValue(PAGE_PARAM, httpServletRequest);

        String username = getUsernameFromSession(httpServletRequest, user.getBlog());
        if (!checkPermission(user, null, username, SPAM_PHRASE_MODERATION_PERMISSION)) {
            httpServletRequest.setAttribute(PAGE_PARAM, ADMIN_LOGIN_PAGE);
            addOperationResultMessage(context, "You are not allowed to edit spam phrase moderation settings");

            return entries;
        }

        if (ADMIN_LOGIN_PAGE.equals(page)) {
            return entries;
        } else {
            String action = BlojsomUtils.getRequestValue(ACTION_PARAM, httpServletRequest);
            List spamPhrases = loadSpamPhrases(user);
            String spamPhrase = BlojsomUtils.getRequestValue(SPAM_PHRASE, httpServletRequest);

            if (ADD_SPAM_PHRASE_ACTION.equals(action)) {
                if (!spamPhrases.contains(spamPhrase)) {
                    spamPhrases.add(spamPhrase);
                    writeSpamPhrases(user, spamPhrases);
                    addOperationResultMessage(context, "Added spam phrase: " + spamPhrase);
                } else {
                    addOperationResultMessage(context, "Spam phrase " + spamPhrase + " has already been added");
                }
            } else if (DELETE_SPAM_PHRASE_ACTION.equals(action)) {
                String[] spamPhrasesToDelete = httpServletRequest.getParameterValues(SPAM_PHRASE);
                if (spamPhrasesToDelete != null && spamPhrasesToDelete.length > 0) {
                    for (int i = 0; i < spamPhrasesToDelete.length; i++) {
                        spamPhrases.set(Integer.parseInt(spamPhrasesToDelete[i]), null);
                    }

                    spamPhrases = BlojsomUtils.removeNullValues(spamPhrases);
                    writeSpamPhrases(user, spamPhrases);
                    addOperationResultMessage(context, "Deleted " + spamPhrasesToDelete.length + " spam phrases");
                } else {
                    addOperationResultMessage(context, "No spam phrases selected for deletion");
                }
            }

            context.put(BLOJSOM_PLUGIN_SPAM_PHRASES, spamPhrases);
        }

        return entries;
    }

    /**
     * Load the list of spam phrases from the blog's configuration directory
     *
     * @param blogUser {@link BlogUser}
     * @return List of spam phrases
     */
    protected List loadSpamPhrases(BlogUser blogUser) {
        File blacklistFile = new File(_blojsomConfiguration.getInstallationDirectory() + _blojsomConfiguration.getBaseConfigurationDirectory() + "/" +
                blogUser.getId() + "/" + _spamPhraseBlacklist);
        ArrayList spamPhrases = new ArrayList(25);

        if (blacklistFile.exists()) {
            try {
                FileInputStream fis = new FileInputStream(blacklistFile);
                BufferedReader br = new BufferedReader(new InputStreamReader(fis, BlojsomConstants.UTF8));
                String phrase;

                while ((phrase = br.readLine()) != null) {
                    spamPhrases.add(phrase);
                }

                br.close();
            } catch (IOException e) {
                _logger.error(e);
            }
        }

        return spamPhrases;
    }

    /**
     * Write out the spam phrases to the blog's configuration directory
     *
     * @param blogUser    {@link BlogUser}
     * @param spamPhrases List of spam phrases
     */
    protected void writeSpamPhrases(BlogUser blogUser, List spamPhrases) {
        File blacklistFile = new File(_blojsomConfiguration.getInstallationDirectory() + _blojsomConfiguration.getBaseConfigurationDirectory() + "/" +
                blogUser.getId() + "/" + _spamPhraseBlacklist);

        try {
            FileOutputStream fos = new FileOutputStream(blacklistFile);
            BufferedWriter bw = new BufferedWriter(new OutputStreamWriter(fos, BlojsomConstants.UTF8));

            Iterator phrasesIterator = spamPhrases.iterator();
            while (phrasesIterator.hasNext()) {
                String phrase = (String) phrasesIterator.next();
                bw.write(phrase);
                bw.newLine();
            }

            bw.close();
        } catch (IOException e) {
            _logger.error(e);
        }
    }
}