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
package org.blojsom.plugin.moblog;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.util.BlojsomUtils;

import javax.servlet.ServletConfig;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.HashMap;
import java.util.Map;
import java.util.Properties;

/**
 * Moblog Plugin Utils
 *
 * @author David Czarnecki
 * @version $Id: MoblogPluginUtils.java,v 1.1 2004/08/27 01:06:39 whitmore Exp $
 * @since blojsom 2.16
 */
public class MoblogPluginUtils {

    private static Log _logger = LogFactory.getLog(MoblogPluginUtils.class);

    /**
     * Configuires the list of valid Moblog posters for each users blog
     *
     * @param servletConfig        Servlet configuration information
     * @param blojsomConfiguration {@link org.blojsom.blog.BlojsomConfiguration} Information
     * @param user                 the User Id for this Authorzation List
     * @param authFile             the file that contains this users authorization List;
     */
    private static Map configureAuthorizedAddresses(ServletConfig servletConfig, BlojsomConfiguration blojsomConfiguration, String user, String authFile) {
        Map authorizedAddresses = new HashMap();

        if (authFile != null) {
            String authorizationFile = blojsomConfiguration.getBaseConfigurationDirectory() + user + "/" + authFile;
            InputStream ais = servletConfig.getServletContext().getResourceAsStream(authorizationFile);
            if (ais == null) {
                _logger.info("No moblog-authorization configuration file found: " + authorizationFile);
            } else {
                Properties authorizationProperties = new Properties();
                try {
                    authorizationProperties.load(ais);
                    ais.close();

                    authorizedAddresses = BlojsomUtils.propertiesToMap(authorizationProperties);
                } catch (IOException e) {
                    _logger.error(e);
                }
            }
        }

        return authorizedAddresses;
    }

    /**
     * Read in the mailbox settings for a given user
     *
     * @param blojsomConfiguration {@link BlojsomConfiguration}
     * @param servletConfig {@link ServletConfig}
     * @param blogUser {@link BlogUser}
     * @return {@link Mailbox} populated with settings and authorized e-mail addresses or <code>null</code> if there
     * was an error reading any configuration information
     */
    public static Mailbox readMailboxSettingsForUser(BlojsomConfiguration blojsomConfiguration, ServletConfig servletConfig,
                                                     BlogUser blogUser) {
        String moblogConfiguration = servletConfig.getInitParameter(MoblogPlugin.PLUGIN_MOBLOG_CONFIGURATION_IP);

        Mailbox mailbox = new Mailbox();
        mailbox.setEnabled(false);

        String user = blogUser.getId();

        Properties moblogProperties = new Properties();
        String configurationFile = blojsomConfiguration.getBaseConfigurationDirectory() + user + "/" + moblogConfiguration;

        InputStream is = servletConfig.getServletContext().getResourceAsStream(configurationFile);
        if (is == null) {
            _logger.info("No moblog configuration file found: " + configurationFile);
        } else {
            try {
                moblogProperties.load(is);
                is.close();

                if (moblogProperties.size() > 0) {
                    mailbox.setBlogUser(blogUser);
                    String authFile = moblogProperties.getProperty(MoblogPlugin.PROPERTY_AUTHORIZATION, MoblogPlugin.DEFAULT_MOBLOG_AUTHORIZATION_FILE);

                    String hostname = moblogProperties.getProperty(MoblogPlugin.PROPERTY_HOSTNAME);
                    if (!BlojsomUtils.checkNullOrBlank(hostname)) {
                        mailbox.setHostName(hostname);
                    } else {
                        mailbox.setEnabled(false);
                        _logger.info("Marked moblog mailbox as disabled for user: " + user + ". No " + MoblogPlugin.PROPERTY_HOSTNAME + " property.");

                        return mailbox;
                    }

                    String userid = moblogProperties.getProperty(MoblogPlugin.PROPERTY_USERID);
                    if (!BlojsomUtils.checkNullOrBlank(userid)) {
                        mailbox.setUserId(userid);
                    } else {
                        mailbox.setEnabled(false);
                        _logger.info("Marked moblog mailbox as disabled for user: " + user + ". No " + MoblogPlugin.PROPERTY_USERID + " property.");

                        return mailbox;
                    }

                    String password = moblogProperties.getProperty(MoblogPlugin.PROPERTY_PASSWORD);
                    if (!BlojsomUtils.checkNullOrBlank(password)) {
                        mailbox.setPassword(password);
                    } else {
                        mailbox.setEnabled(false);
                        _logger.info("Marked moblog mailbox as disabled for user: " + user + ". No " + MoblogPlugin.PROPERTY_PASSWORD + " property.");

                        return mailbox;
                    }

                    mailbox.setUrlPrefix(BlojsomUtils.removeTrailingSlash(blojsomConfiguration.getResourceDirectory()) + "/" + user + "/");
                    String resourceUrl = blojsomConfiguration.getQualifiedResourceDirectory();
                    mailbox.setOutputDirectory(resourceUrl + File.separator + user);

                    String blogCategoryName = moblogProperties.getProperty(MoblogPlugin.PROPERTY_CATEGORY);
                    blogCategoryName = BlojsomUtils.normalize(blogCategoryName);
                    if (!blogCategoryName.endsWith("/")) {
                        blogCategoryName += "/";
                    }

                    mailbox.setCategoryName(blogCategoryName);
                    mailbox.setEntriesDirectory(blogUser.getBlog().getBlogURL() + BlojsomUtils.removeInitialSlash(blogCategoryName));

                    Boolean enabled = Boolean.valueOf(moblogProperties.getProperty(MoblogPlugin.PROPERTY_ENABLED, "false"));
                    mailbox.setEnabled(enabled.booleanValue());

                    String[] types;

                    // Extract the image mime types
                    String imageMimeTypes = moblogProperties.getProperty(MoblogPlugin.PLUGIN_MOBLOG_IMAGE_MIME_TYPES, MoblogPlugin.DEFAULT_IMAGE_MIME_TYPES);
                    if (!BlojsomUtils.checkNullOrBlank(imageMimeTypes)) {
                        types = BlojsomUtils.parseCommaList(imageMimeTypes);
                        if (types.length > 0) {
                            Map imageTypesMap = new HashMap();
                            for (int i = 0; i < types.length; i++) {
                                String type = types[i];
                                imageTypesMap.put(type, type);
                            }
                            mailbox.setImageMimeTypes(imageTypesMap);
                        }
                    } else {
                        mailbox.setImageMimeTypes(new HashMap());
                    }

                    // Extract the attachment mime types
                    String attachmentMimeTypes = moblogProperties.getProperty(MoblogPlugin.PLUGIN_MOBLOG_ATTACHMENT_MIME_TYPES);
                    if (!BlojsomUtils.checkNullOrBlank(attachmentMimeTypes)) {
                        types = BlojsomUtils.parseCommaList(attachmentMimeTypes);
                        if (types.length > 0) {
                            Map attachmentTypesMap = new HashMap();
                            for (int i = 0; i < types.length; i++) {
                                String type = types[i];
                                attachmentTypesMap.put(type, type);
                            }
                            mailbox.setAttachmentMimeTypes(attachmentTypesMap);
                        }
                    } else {
                        mailbox.setAttachmentMimeTypes(new HashMap());
                    }

                    // Extract the text mime types
                    String textMimeTypes = moblogProperties.getProperty(MoblogPlugin.PLUGIN_MOBLOG_TEXT_MIME_TYPES, MoblogPlugin.DEFAULT_TEXT_MIME_TYPES);
                    if (!BlojsomUtils.checkNullOrBlank(textMimeTypes)) {
                        types = BlojsomUtils.parseCommaList(textMimeTypes);
                        if (types.length > 0) {
                            Map textTypesMap = new HashMap();
                            for (int i = 0; i < types.length; i++) {
                                String type = types[i];
                                textTypesMap.put(type, type);
                            }
                            mailbox.setTextMimeTypes(textTypesMap);
                        }
                    } else {
                        mailbox.setTextMimeTypes(new HashMap());
                    }

                    // Extract the secret word
                    String secretWord = moblogProperties.getProperty(MoblogPlugin.PLUGIN_MOBLOG_SECRET_WORD);
                    if (BlojsomUtils.checkNullOrBlank(secretWord)) {
                        mailbox.setSecretWord(null);
                    } else {
                        mailbox.setSecretWord(secretWord);
                    }

                    // Configure authorized email addresses for moblog posting
                    mailbox.setAuthorizedAddresses(configureAuthorizedAddresses(servletConfig, blojsomConfiguration, user, authFile));
                }
            } catch (IOException e) {
                _logger.error(e);

                return null;
            }
        }

        return mailbox;
    }
}