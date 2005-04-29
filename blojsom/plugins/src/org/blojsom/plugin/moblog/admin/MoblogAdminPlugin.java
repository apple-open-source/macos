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
package org.blojsom.plugin.moblog.admin;

import org.blojsom.plugin.admin.WebAdminPlugin;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.plugin.moblog.Mailbox;
import org.blojsom.plugin.moblog.MoblogPluginUtils;
import org.blojsom.plugin.moblog.MoblogPlugin;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.util.BlojsomUtils;
import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;

import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.util.Map;
import java.util.Properties;
import java.io.*;

/**
 * Moblog Admin Plugin
 *
 * @author David Czarnecki
 * @version $Id: MoblogAdminPlugin.java,v 1.1 2004/08/27 01:06:39 whitmore Exp $
 * @since blojsom 2.16
 */
public class MoblogAdminPlugin extends WebAdminPlugin {

    private Log _logger = LogFactory.getLog(MoblogAdminPlugin.class);

    private static final String EDIT_MOBLOG_SETTINGS_PAGE = "/org/blojsom/plugin/moblog/admin/templates/admin-edit-moblog-settings";

    // Form itmes
    private static final String MOBLOG_ENABLED = "moblog-enabled";
    private static final String MOBLOG_HOSTNAME = "moblog-hostname";
    private static final String MOBLOG_USERID = "moblog-userid";
    private static final String MOBLOG_PASSWORD = "moblog-password";
    private static final String MOBLOG_CATEGORY = "moblog-category";
    private static final String MOBLOG_SECRET_WORD = "moblog-secret-word";
    private static final String MOBLOG_IMAGE_MIME_TYPES = "moblog-image-mime-types";
    private static final String MOBLOG_ATTACHMENT_MIME_TYPES = "moblog-attachment-mime-types";
    private static final String MOBLOG_TEXT_MIME_TYPES = "moblog-text-mime-types";
    private static final String MOBLOG_AUTHORIZED_ADDRESS = "moblog-authorized-address";

    // Actions
    private static final String UPDATE_MOBLOG_SETTINGS_ACTIONS = "update-moblog-settings";
    private static final String ADD_AUTHORIZED_ADDRESS_ACTION = "add-authorized-address";
    private static final String DELETE_AUTHORIZED_ADDRESS_ACTION = "delete-authorized-address";

    private static final String BLOJSOM_PLUGIN_MOBLOG_MAILBOX = "BLOJSOM_PLUGIN_MOBLOG_MAILBOX";

    /**
     * Default constructor
     */
    public MoblogAdminPlugin() {
    }

    /**
     * Return the display name for the plugin
     *
     * @return Display name for the plugin
     */
    public String getDisplayName() {
        return "Moblog plugin";
    }

    /**
     * Return the name of the initial editing page for the plugin
     *
     * @return Name of the initial editing page for the plugin
     */
    public String getInitialPage() {
        return EDIT_MOBLOG_SETTINGS_PAGE;
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

        if (ADMIN_LOGIN_PAGE.equals(page)) {
            return entries;
        } else {
            String action = BlojsomUtils.getRequestValue(ACTION_PARAM, httpServletRequest);
            Mailbox mailbox = MoblogPluginUtils.readMailboxSettingsForUser(_blojsomConfiguration, _servletConfig, user);

            if (mailbox == null) {
                mailbox = new Mailbox();
            }

            if (UPDATE_MOBLOG_SETTINGS_ACTIONS.equals(action)) {
                String moblogConfigurationFile = _servletConfig.getInitParameter(MoblogPlugin.PLUGIN_MOBLOG_CONFIGURATION_IP);

                boolean mailboxEnabled = Boolean.valueOf(BlojsomUtils.getRequestValue(MOBLOG_ENABLED, httpServletRequest)).booleanValue();
                mailbox.setEnabled(mailboxEnabled);
                String hostname = BlojsomUtils.getRequestValue(MOBLOG_HOSTNAME, httpServletRequest);
                mailbox.setHostName(hostname);
                String userID = BlojsomUtils.getRequestValue(MOBLOG_USERID, httpServletRequest);
                mailbox.setUserId(userID);
                String password = BlojsomUtils.getRequestValue(MOBLOG_PASSWORD, httpServletRequest);
                mailbox.setPassword(password);
                String category = BlojsomUtils.getRequestValue(MOBLOG_CATEGORY, httpServletRequest);
                category = BlojsomUtils.normalize(category);
                mailbox.setCategoryName(category);
                String textMimeTypeValue = BlojsomUtils.getRequestValue(MOBLOG_TEXT_MIME_TYPES, httpServletRequest);
                String[] textMimeTypes = BlojsomUtils.parseCommaList(textMimeTypeValue);
                Map textMimeMap = mailbox.getTextMimeTypes();
                for (int i = 0; i < textMimeTypes.length; i++) {
                    String textMimeType = textMimeTypes[i];
                    textMimeMap.put(textMimeType, textMimeType);
                }
                String attachmentMimeTypeValue = BlojsomUtils.getRequestValue(MOBLOG_ATTACHMENT_MIME_TYPES, httpServletRequest);
                String[] attachmentMimeTypes = BlojsomUtils.parseCommaList(attachmentMimeTypeValue);
                Map attachmentMimeMap = mailbox.getAttachmentMimeTypes();
                for (int i = 0; i < attachmentMimeTypes.length; i++) {
                    String attachmentMimeType = attachmentMimeTypes[i];
                    attachmentMimeMap.put(attachmentMimeType, attachmentMimeType);
                }
                String imageMimeTypeValue = BlojsomUtils.getRequestValue(MOBLOG_IMAGE_MIME_TYPES, httpServletRequest);
                String[] imageMimeTypes = BlojsomUtils.parseCommaList(imageMimeTypeValue);
                Map imageMimeMap = mailbox.getImageMimeTypes();
                for (int i = 0; i < imageMimeTypes.length; i++) {
                    String imageMimeType = imageMimeTypes[i];
                    imageMimeMap.put(imageMimeType, imageMimeType);
                }
                String secretWord = BlojsomUtils.getRequestValue(MOBLOG_SECRET_WORD, httpServletRequest);
                mailbox.setSecretWord(secretWord);

                try {
                    writeMoblogConfiguration(user.getId(), moblogConfigurationFile, mailbox);
                    addOperationResultMessage(context, "Updated moblog configuration");
                } catch (IOException e) {
                    _logger.error(e);
                    addOperationResultMessage(context, "Unable to write moblog configuration");
                }
            } else if (ADD_AUTHORIZED_ADDRESS_ACTION.equals(action)) {
                String addressToAdd = BlojsomUtils.getRequestValue(MOBLOG_AUTHORIZED_ADDRESS, httpServletRequest);
                if (!BlojsomUtils.checkNullOrBlank(addressToAdd)) {
                    Map authorizedEmailAddresses = mailbox.getAuthorizedAddresses();
                    authorizedEmailAddresses.put(addressToAdd, addressToAdd);

                    try {
                        writeAuthorizedEmailAddresssConfiguration(user.getId(), MoblogPlugin.DEFAULT_MOBLOG_AUTHORIZATION_FILE, authorizedEmailAddresses);
                        addOperationResultMessage(context, "Added e-mail address to moblog authorized addresses: " + addressToAdd);
                    } catch (IOException e) {
                        _logger.error(e);
                        addOperationResultMessage(context, "Unable to add e-mail address");
                    }
                } else {
                    addOperationResultMessage(context, "No e-mail address to add");
                }
            } else if (DELETE_AUTHORIZED_ADDRESS_ACTION.equals(action)) {
                String addressToDelete = BlojsomUtils.getRequestValue(MOBLOG_AUTHORIZED_ADDRESS, httpServletRequest);
                if (!BlojsomUtils.checkNullOrBlank(addressToDelete)) {
                    Map authorizedEmailAddresses = mailbox.getAuthorizedAddresses();
                    authorizedEmailAddresses.remove(addressToDelete);

                    try {
                        writeAuthorizedEmailAddresssConfiguration(user.getId(), MoblogPlugin.DEFAULT_MOBLOG_AUTHORIZATION_FILE, authorizedEmailAddresses);
                        addOperationResultMessage(context, "Removed e-mail address from moblog authorized addresses: " + addressToDelete);
                    } catch (IOException e) {
                        _logger.error(e);
                        addOperationResultMessage(context, "Unable to delete e-mail address");
                    }
                } else {
                    addOperationResultMessage(context, "No e-mail address to delete");
                }
            }

            context.put(BLOJSOM_PLUGIN_MOBLOG_MAILBOX, mailbox);
        }

        return entries;
    }

    /**
     * Write out the moblog e-mail address authorization
     *
     * @param userID User ID
     * @param authorizationConfiguration Authorization configuration file
     * @param authorizedEmailAddresses Addresses
     * @throws IOException If there is an error writing the configuration
     */
    private void writeAuthorizedEmailAddresssConfiguration(String userID, String authorizationConfiguration, Map authorizedEmailAddresses) throws IOException {
        File emailConfigurationFile = new File(_blojsomConfiguration.getInstallationDirectory() + _blojsomConfiguration.getBaseConfigurationDirectory() + userID + "/" + authorizationConfiguration);
        FileOutputStream fos = new FileOutputStream(emailConfigurationFile);
        Properties emailProperties = BlojsomUtils.mapToProperties(authorizedEmailAddresses);
        emailProperties.store(fos, null);
        fos.close();
    }

    /**
     * Write out the moblog configuration
     *
     * @param userID User ID
     * @param moblogConfiguration Moblog configuration file
     * @param mailbox {@link Mailbox} information
     * @throws IOException If there is an error writing the configuration                        
     */
    private void writeMoblogConfiguration(String userID, String moblogConfiguration, Mailbox mailbox) throws IOException {
        File moblogConfigurationFile = new File(_blojsomConfiguration.getInstallationDirectory() + _blojsomConfiguration.getBaseConfigurationDirectory() + userID + "/" + moblogConfiguration);
        BufferedWriter bw = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(moblogConfigurationFile), UTF8));
        bw.write(MoblogPlugin.PROPERTY_HOSTNAME);
        bw.write("=");
        bw.write(BlojsomUtils.nullToBlank(mailbox.getHostName()));
        bw.write(BlojsomUtils.LINE_SEPARATOR);
        bw.write(MoblogPlugin.PROPERTY_USERID);
        bw.write("=");
        bw.write(BlojsomUtils.nullToBlank(mailbox.getUserId()));
        bw.write(BlojsomUtils.LINE_SEPARATOR);
        bw.write(MoblogPlugin.PROPERTY_PASSWORD);
        bw.write("=");
        bw.write(BlojsomUtils.nullToBlank(mailbox.getPassword()));
        bw.write(BlojsomUtils.LINE_SEPARATOR);
        bw.write(MoblogPlugin.PROPERTY_CATEGORY);
        bw.write("=");
        bw.write(BlojsomUtils.nullToBlank(mailbox.getCategoryName()));
        bw.write(BlojsomUtils.LINE_SEPARATOR);
        bw.write(MoblogPlugin.PROPERTY_ENABLED);
        bw.write("=");
        bw.write(Boolean.toString(mailbox.isEnabled()));
        bw.write(BlojsomUtils.LINE_SEPARATOR);
        bw.write(MoblogPlugin.PROPERTY_AUTHORIZATION);
        bw.write("=");
        bw.write(MoblogPlugin.DEFAULT_MOBLOG_AUTHORIZATION_FILE);
        bw.write(BlojsomUtils.LINE_SEPARATOR);
        bw.write(MoblogPlugin.PLUGIN_MOBLOG_TEXT_MIME_TYPES);
        bw.write("=");
        bw.write(mailbox.getTextMimeTypesAsStringList());
        bw.write(BlojsomUtils.LINE_SEPARATOR);
        bw.write(MoblogPlugin.PLUGIN_MOBLOG_ATTACHMENT_MIME_TYPES);
        bw.write("=");
        bw.write(mailbox.getAttachmentMimeTypesAsStringList());
        bw.write(BlojsomUtils.LINE_SEPARATOR);
        bw.write(MoblogPlugin.PLUGIN_MOBLOG_IMAGE_MIME_TYPES);
        bw.write("=");
        bw.write(mailbox.getImageMimeTypesAsStringList());
        bw.write(BlojsomUtils.LINE_SEPARATOR);
        bw.write(MoblogPlugin.PLUGIN_MOBLOG_SECRET_WORD);
        bw.write("=");
        bw.write(BlojsomUtils.nullToBlank(mailbox.getSecretWord()));
        bw.write(BlojsomUtils.LINE_SEPARATOR);
        bw.close();
    }
}