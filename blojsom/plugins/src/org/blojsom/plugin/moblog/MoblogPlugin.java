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
import org.blojsom.BlojsomException;
import org.blojsom.blog.*;
import org.blojsom.fetcher.BlojsomFetcher;
import org.blojsom.fetcher.BlojsomFetcherException;
import org.blojsom.plugin.BlojsomPlugin;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.util.BlojsomConstants;
import org.blojsom.util.BlojsomMetaDataConstants;
import org.blojsom.util.BlojsomUtils;

import javax.mail.*;
import javax.mail.internet.InternetAddress;
import javax.mail.internet.MimeMultipart;
import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.*;
import java.net.ConnectException;
import java.util.*;

/**
 * Moblog Plugin
 *
 * @author David Czarnecki
 * @author Mark Lussier
 * @version $Id: MoblogPlugin.java,v 1.1 2004/08/27 01:06:39 whitmore Exp $
 * @since blojsom 2.14
 */
public class MoblogPlugin implements BlojsomPlugin, BlojsomConstants {

    private Log _logger = LogFactory.getLog(MoblogPlugin.class);

    /**
     * Multipart/alternative mime-type
     */
    private static final String MULTIPART_ALTERNATIVE_MIME_TYPE = "multipart/alternative";

    /**
     * Text/html mime-type
     */
    private static final String TEXT_HTML_MIME_TYPE = "text/html";

    /**
     * Default mime-types for text
     */
    public static final String DEFAULT_TEXT_MIME_TYPES = "text/plain, text/html";

    /**
     * Default mime-types for images
     */
    public static final String DEFAULT_IMAGE_MIME_TYPES = "image/jpg, image/jpeg, image/gif, image/png";

    /**
     * Multipart mime-type
     */
    private static final String MULTIPART_TYPE = "multipart/*";

    /**
     * Default store
     */
    private static final String POP3_STORE = "pop3";

    /**
     * Default poll time (5 minutes)
     */
    private static final int DEFAULT_POLL_TIME = 360;

    /**
     * Moblog confifguration parameter for web.xml
     */
    public static final String PLUGIN_MOBLOG_CONFIGURATION_IP = "plugin-moblog";

    /**
     * Moblog configuration parameter for mailbox polling time (5 minutes)
     */
    public static final String PLUGIN_MOBLOG_POLL_TIME = "plugin-moblog-poll-time";

    /**
     * Default moblog authorization properties file which lists valid e-mail addresses who can moblog entries
     */
    public static final String DEFAULT_MOBLOG_AUTHORIZATION_FILE = "moblog-authorization.properties";

    /**
     * Configuration property for moblog authorization properties file to use
     */
    public static final String PROPERTY_AUTHORIZATION = "moblog-authorization";

    /**
     * Configuration property for mailhost
     */
    public static final String PROPERTY_HOSTNAME = "moblog-hostname";

    /**
     * Configuration property for mailbox user ID
     */
    public static final String PROPERTY_USERID = "moblog-userid";

    /**
     * Configuration property for mailbox user password
     */
    public static final String PROPERTY_PASSWORD = "moblog-password";

    /**
     * Configuration property for moblog category
     */
    public static final String PROPERTY_CATEGORY = "moblog-category";

    /**
     * Configuration property for whether or not moblog is enabled for this blog
     */
    public static final String PROPERTY_ENABLED = "moblog-enabled";

    /**
     * Configuration property for the secret word that must be present at the beginning of the subject
     */
    public static final String PLUGIN_MOBLOG_SECRET_WORD = "moblog-secret-word";

    /**
     * Configuration property for image mime-types
     */
    public static final String PLUGIN_MOBLOG_IMAGE_MIME_TYPES = "moblog-image-mime-types";

    /**
     * Configuration property for attachment mime-types
     */
    public static final String PLUGIN_MOBLOG_ATTACHMENT_MIME_TYPES = "moblog-attachment-mime-types";

    /**
     * Configuration property for text mime-types
     */
    public static final String PLUGIN_MOBLOG_TEXT_MIME_TYPES = "moblog-text-mime-types";

    private int _pollTime;

    private Session _popSession;
    private boolean _finished = false;
    private MailboxChecker _checker;
    private ServletConfig _servletConfig;
    private BlojsomConfiguration _blojsomConfiguration;

    private BlojsomFetcher _fetcher;


    /**
     * Initialize this plugin. This method only called when the plugin is
     * instantiated.
     *
     * @param servletConfig        Servlet config object for the plugin to retrieve any
     *                             initialization parameters
     * @param blojsomConfiguration {@link org.blojsom.blog.BlojsomConfiguration}
     *                             information
     * @throws org.blojsom.plugin.BlojsomPluginException
     *          If there is an error
     *          initializing the plugin
     */
    public void init(ServletConfig servletConfig, BlojsomConfiguration
            blojsomConfiguration) throws BlojsomPluginException {

        String fetcherClassName = blojsomConfiguration.getFetcherClass();
        try {
            Class fetcherClass = Class.forName(fetcherClassName);
            _fetcher = (BlojsomFetcher) fetcherClass.newInstance();
            _fetcher.init(servletConfig, blojsomConfiguration);
            _logger.info("Added blojsom fetcher: " + fetcherClassName);
        } catch (ClassNotFoundException e) {
            _logger.error(e);
            throw new BlojsomPluginException(e);
        } catch (InstantiationException e) {
            _logger.error(e);
            throw new BlojsomPluginException(e);
        } catch (IllegalAccessException e) {
            _logger.error(e);
            throw new BlojsomPluginException(e);
        } catch (BlojsomFetcherException e) {
            _logger.error(e);
            throw new BlojsomPluginException(e);
        }

        String moblogPollTime = servletConfig.getInitParameter(PLUGIN_MOBLOG_POLL_TIME);
        if (BlojsomUtils.checkNullOrBlank(moblogPollTime)) {
            _pollTime = DEFAULT_POLL_TIME;
        } else {
            try {
                _pollTime = Integer.parseInt(moblogPollTime);
            } catch (NumberFormatException e) {
                _logger.error("Invalid time specified for: " + PLUGIN_MOBLOG_POLL_TIME);
                _pollTime = DEFAULT_POLL_TIME;
            }
        }

        _servletConfig = servletConfig;
        _blojsomConfiguration = blojsomConfiguration;
        _checker = new MailboxChecker();
        _popSession = Session.getDefaultInstance(System.getProperties(), null);
        _checker.start();

        _logger.debug("Initialized moblog plugin.");
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
     * @throws BlojsomPluginException If there is an error processing the blog
     *                                entries
     */
    public BlogEntry[] process(HttpServletRequest httpServletRequest,
                               HttpServletResponse httpServletResponse, BlogUser user, Map context,
                               BlogEntry[] entries) throws BlojsomPluginException {
        return entries;
    }

    /**
     * Perform any cleanup for the plugin. Called after {@link #process}.
     *
     * @throws BlojsomPluginException If there is an error performing cleanup
     *                                for this plugin
     */
    public void cleanup() throws BlojsomPluginException {
    }

    /**
     * Called when BlojsomServlet is taken out of service
     *
     * @throws BlojsomPluginException If there is an error in finalizing this
     *                                plugin
     */
    public void destroy() throws BlojsomPluginException {
        _finished = true;
    }

    /**
     * Thread that polls the mailboxes
     */
    private class MailboxChecker extends Thread {

        /**
         * Allocates a new <code>Thread</code> object. This constructor has
         * the same effect as <code>Thread(null, null,</code>
         * <i>gname</i><code>)</code>, where <b><i>gname</i></b> is
         * a newly generated name. Automatically generated names are of the
         * form <code>"Thread-"+</code><i>n</i>, where <i>n</i> is an integer.
         *
         * @see Thread#Thread(ThreadGroup,
                *      Runnable, String)
         */
        public MailboxChecker() {
            super();
        }

        /**
         * Perform the actual work of checking the POP3 mailbox configured for the blog user.
         *
         * @param mailbox Mailbox to be processed
         */
        private void processMailbox(Mailbox mailbox) {
            Folder folder = null;
            Store store = null;
            String subject = null;
            try {
                store = _popSession.getStore(POP3_STORE);
                store.connect(mailbox.getHostName(), mailbox.getUserId(), mailbox.getPassword());

                // -- Try to get hold of the default folder --
                folder = store.getDefaultFolder();
                if (folder == null) {
                    _logger.error("Default folder is null.");
                    _finished = true;
                }

                // -- ...and its INBOX --
                folder = folder.getFolder(mailbox.getFolder());
                if (folder == null) {
                    _logger.error("No POP3 folder called " + mailbox.getFolder());
                    _finished = true;
                }

                // -- Open the folder for read only --
                folder.open(Folder.READ_WRITE);

                // -- Get the message wrappers and process them --
                Message[] msgs = folder.getMessages();

                _logger.debug("Found [" + msgs.length + "] messages");

                for (int msgNum = 0; msgNum < msgs.length; msgNum++) {
                    String from = ((InternetAddress)
                            msgs[msgNum].getFrom()[0]).getAddress();
                    _logger.debug("Processing message: " + msgNum);

                    if (!checkSender(mailbox, from)) {
                        _logger.debug("Unauthorized sender address: " + from);
                        _logger.debug("Deleting message: " + msgNum);
                        msgs[msgNum].setFlag(Flags.Flag.DELETED, true);
                    } else {
                        Message email = msgs[msgNum];
                        subject = email.getSubject();

                        StringBuffer entry = new StringBuffer();
                        StringBuffer description = new StringBuffer();
                        Part messagePart = email;
                        if (subject == null) {
                            subject = "";
                        } else {
                            subject = subject.trim();
                        }

                        String secretWord = mailbox.getSecretWord();
                        if (secretWord != null) {
                            if (!subject.startsWith(secretWord)) {
                                _logger.error("Message does not begin with secret word for user id: " + mailbox.getUserId());
                                msgs[msgNum].setFlag(Flags.Flag.DELETED, true);

                                continue;
                            } else {
                                subject = subject.substring(secretWord.length());
                            }
                        }

                        if (email.isMimeType(MULTIPART_TYPE)) {
                            // Check for multipart/alternative
                            String overallType = email.getContentType();
                            overallType = sanitizeContentType(overallType);
                            boolean isMultipartAlternative = false;
                            if (MULTIPART_ALTERNATIVE_MIME_TYPE.equals(overallType)) {
                                isMultipartAlternative = true;
                            }

                            Multipart mp = (Multipart)
                                    messagePart.getContent();
                            int count = mp.getCount();

                            for (int i = 0; i < count; i++) {
                                BodyPart bp = mp.getBodyPart(i);
                                String type = bp.getContentType();
                                if (type != null) {
                                    type = sanitizeContentType(type);

                                    Map imageMimeTypes = mailbox.getImageMimeTypes();
                                    Map attachmentMimeTypes = mailbox.getAttachmentMimeTypes();
                                    Map textMimeTypes = mailbox.getTextMimeTypes();

                                    // Check for multipart alternative as part of a larger e-mail block
                                    if (MULTIPART_ALTERNATIVE_MIME_TYPE.equals(type)) {
                                        Object mimeMultipartContent = bp.getContent();
                                        if (mimeMultipartContent instanceof MimeMultipart) {
                                            MimeMultipart mimeMultipart = (MimeMultipart) mimeMultipartContent;
                                            int mimeMultipartCount = mimeMultipart.getCount();
                                            for (int j = 0; j < mimeMultipartCount; j++) {
                                                BodyPart mimeMultipartBodyPart = mimeMultipart.getBodyPart(j);
                                                String mmpbpType = mimeMultipartBodyPart.getContentType();
                                                if (mmpbpType != null) {
                                                    mmpbpType = sanitizeContentType(mmpbpType);
                                                    if (TEXT_HTML_MIME_TYPE.equals(mmpbpType)) {
                                                        _logger.debug("Using HTML part of multipart/alternative: " + type);
                                                        InputStream is = bp.getInputStream();

                                                        BufferedReader reader = new
                                                                BufferedReader(new InputStreamReader(is, UTF8));
                                                        String thisLine;

                                                        while ((thisLine = reader.readLine()) !=
                                                                null) {
                                                            description.append(thisLine);
                                                            description.append(BlojsomConstants.LINE_SEPARATOR);
                                                        }

                                                        reader.close();
                                                        entry.append(description);
                                                    } else {
                                                        _logger.debug("Skipping non-HTML part of multipart/alternative block");
                                                    }
                                                } else {
                                                    _logger.info("Unknown mimetype for multipart/alternative block");
                                                }
                                            }
                                        } else {
                                            _logger.debug("Multipart alternative block not instance of MimeMultipart");
                                        }
                                    } else {

                                        if (imageMimeTypes.containsKey(type)) {
                                            _logger.debug("Creating image of type: " + type);
                                            InputStream is = bp.getInputStream();
                                            byte[] imageFile = new byte[is.available()];
                                            is.read(imageFile, 0, is.available());
                                            is.close();
                                            String outputFilename =
                                                    BlojsomUtils.digestString(bp.getFileName() + "-" + new Date().getTime());
                                            String extension = BlojsomUtils.getFileExtension(bp.getFileName());
                                            if (BlojsomUtils.checkNullOrBlank(extension)) {
                                                extension = "";
                                            }

                                            _logger.debug("Writing to: " + mailbox.getOutputDirectory() + File.separator +
                                                    outputFilename + "." + extension);
                                            FileOutputStream fos = new
                                                    FileOutputStream(new File(mailbox.getOutputDirectory() + File.separator + outputFilename + "." + extension));
                                            fos.write(imageFile);
                                            fos.close();
                                            String baseurl = mailbox.getBlogUser().getBlog().getBlogBaseURL();
                                            entry.append("<p /><img src=\"").append(baseurl).append(mailbox.getUrlPrefix()).append(outputFilename + "." + extension).append("\" border=\"0\" />");
                                        } else if (attachmentMimeTypes.containsKey(type)) {
                                            _logger.debug("Creating attachment of type: " + type);
                                            InputStream is = bp.getInputStream();
                                            byte[] attachmentFile = new byte[is.available()];
                                            is.read(attachmentFile, 0, is.available());
                                            is.close();
                                            String outputFilename =
                                                    BlojsomUtils.digestString(bp.getFileName() + "-" + new Date().getTime());
                                            String extension = BlojsomUtils.getFileExtension(bp.getFileName());
                                            if (BlojsomUtils.checkNullOrBlank(extension)) {
                                                extension = "";
                                            }

                                            _logger.debug("Writing to: " + mailbox.getOutputDirectory() + File.separator +
                                                    outputFilename + "." + extension);
                                            FileOutputStream fos = new
                                                    FileOutputStream(new File(mailbox.getOutputDirectory() + File.separator + outputFilename + "." + extension));
                                            fos.write(attachmentFile);
                                            fos.close();
                                            String baseurl = mailbox.getBlogUser().getBlog().getBlogBaseURL();
                                            entry.append("<p /><a href=\"").append(baseurl).append(mailbox.getUrlPrefix()).append(outputFilename + "." + extension).append("\">").append(bp.getFileName()).append("</a>");
                                        } else if (textMimeTypes.containsKey(type)) {
                                            if ((isMultipartAlternative && (TEXT_HTML_MIME_TYPE.equals(type))) || !isMultipartAlternative) {
                                                _logger.debug("Using text part of type: " + type);
                                                InputStream is = bp.getInputStream();

                                                BufferedReader reader = new
                                                        BufferedReader(new InputStreamReader(is, UTF8));
                                                String thisLine;

                                                while ((thisLine = reader.readLine()) !=
                                                        null) {
                                                    description.append(thisLine);
                                                    description.append(BlojsomConstants.LINE_SEPARATOR);
                                                }

                                                reader.close();
                                                entry.append(description);
                                            }
                                        } else {
                                            _logger.info("Unknown mimetype for multipart: " + type);
                                        }
                                    }
                                } else {
                                    _logger.debug("Body part has no defined mime type. Skipping.");
                                }
                            }
                        } else {
                            // Check for the message being one of the defined text mime types if it's not a multipart
                            Map textMimeTypes = mailbox.getTextMimeTypes();
                            String mimeType = email.getContentType();
                            if (mimeType != null) {
                                mimeType = sanitizeContentType(mimeType);
                            }

                            if ((mimeType != null) && (textMimeTypes.containsKey(mimeType))) {
                                InputStream is = email.getInputStream();

                                BufferedReader reader = new BufferedReader(new InputStreamReader(is, UTF8));
                                String thisLine;

                                while ((thisLine = reader.readLine()) != null) {
                                    description.append(thisLine);
                                    description.append(BlojsomConstants.LINE_SEPARATOR);
                                }

                                reader.close();
                                entry.append(description);
                            } else {
                                _logger.info("Unknown mimetype: " + mimeType);
                            }
                        }

                        String filename = BlojsomUtils.digestString(entry.toString());
                        filename += ".txt";

                        // Process subject to change category for moblog post
                        boolean categoryInSubject = false;
                        String categoryFromSubject = null;
                        if (subject.startsWith("[")) {
                            int startIndex = subject.indexOf("[");
                            if (startIndex != -1) {
                                int closingIndex = subject.indexOf("]", startIndex);
                                if (closingIndex != -1) {
                                    categoryFromSubject = subject.substring(startIndex + 1, closingIndex);
                                    subject = subject.substring(closingIndex + 1);
                                    categoryFromSubject = BlojsomUtils.normalize(categoryFromSubject);
                                    if (!categoryFromSubject.startsWith("/")) {
                                        categoryFromSubject = "/" + categoryFromSubject;
                                    }
                                    if (!categoryFromSubject.endsWith("/")) {
                                        categoryFromSubject += "/";
                                    }
                                    categoryInSubject = true;
                                    _logger.info("Using category [" + categoryFromSubject + "] for entry: " + subject);
                                }
                            }
                        }


                        BlogUser blogUser = mailbox.getBlogUser();
                        String categoryName = categoryInSubject ? categoryFromSubject : mailbox.getCategoryName();
                        File blogCategory = getBlogCategoryDirectory(blogUser.getBlog(), categoryName);

                        if (blogCategory.exists() && blogCategory.isDirectory()) {
                            String outputfile = blogCategory.getAbsolutePath() + File.separator + filename;

                            try {
                                File sourceFile = new File(outputfile);
                                BlogEntry blogEntry;
                                blogEntry = _fetcher.newBlogEntry();

                                Map attributeMap = new HashMap();
                                Map blogEntryMetaData = new HashMap();
                                attributeMap.put(BlojsomMetaDataConstants.SOURCE_ATTRIBUTE, sourceFile);
                                blogEntry.setAttributes(attributeMap);

                                blogEntry.setTitle(subject);
                                blogEntry.setCategory(categoryName);
                                blogEntry.setDescription(entry.toString());
                                blogEntryMetaData.put(BlojsomMetaDataConstants.BLOG_ENTRY_METADATA_TIMESTAMP, new Long(new Date().getTime()).toString());
                                blogEntryMetaData.put(BlojsomMetaDataConstants.BLOG_ENTRY_METADATA_AUTHOR_EXT, from);
                                blogEntry.setMetaData(blogEntryMetaData);
                                blogEntry.save(mailbox.getBlogUser());

                                msgs[msgNum].setFlag(Flags.Flag.DELETED, true);
                            } catch (BlojsomException e) {
                                _logger.error(e);
                            }
                        }
                    }
                }

                // Delete the messages
                try {
                    if (folder != null) {
                        folder.close(true);
                    }

                    if (store != null) {
                        store.close();
                    }
                } catch (MessagingException e) {
                    _logger.error(e);
                }
            } catch (ConnectException e) {
                _logger.error(e);
            } catch (NoSuchProviderException e) {
                _logger.error(e);
            } catch (MessagingException e) {
                _logger.error(e);
            } catch (IOException e) {
                _logger.error(e);
            } finally {
                try {
                    if (folder != null && folder.isOpen()) {
                        folder.close(true);
                    }

                    if (store != null) {
                        store.close();
                    }
                } catch (MessagingException e) {
                    _logger.error(e);
                }
            }
        }

        /**
         * Process the moblog mailboxes for each user
         */
        public void run() {
            try {
                while (!_finished) {
                    _logger.debug("Moblog plugin waking up and looking for new messages");

                    Iterator userIterator = _blojsomConfiguration.getBlogUsers().keySet().iterator();
                    while (userIterator.hasNext()) {
                        String user = (String) userIterator.next();
                        BlogUser blogUser = (BlogUser) _blojsomConfiguration.getBlogUsers().get(user);

                        Mailbox mailbox = MoblogPluginUtils.readMailboxSettingsForUser(_blojsomConfiguration, _servletConfig, blogUser);
                        if (mailbox != null) {
                            if (mailbox.isEnabled()) {
                                _logger.debug("Checking mailbox: " + mailbox.getUserId() + " for user: " + mailbox.getBlogUser().getId());
                                processMailbox(mailbox);
                            }
                        }
                    }

                    _logger.debug("Moblog plugin off to take a nap");
                    sleep(_pollTime * 1000);
                }
            } catch (InterruptedException e) {
                _logger.error(e);
            }
        }

        /**
         * Check to see that the sender is an authorized user to moblog
         *
         * @param mailbox     Mailbox for user
         * @param fromAddress E-mail address of sender
         * @return <code>true</code> if the from address is specified as a valid poster to the moblog,
         *         <code>false</code> otherwise
         */
        private boolean checkSender(Mailbox mailbox, String fromAddress) {
            boolean result = false;
            Map authorizedAddresses = mailbox.getAuthorizedAddresses();

            if (authorizedAddresses.containsKey(fromAddress)) {
                result = true;
            }

            return result;
        }
    }

    /**
     * Get the blog category. If the category exists, return the
     * appropriate directory, otherwise return the "root" of this blog.
     *
     * @param categoryName Category name
     * @return A directory into which a blog entry can be placed
     * @since blojsom 2.14
     */
    protected File getBlogCategoryDirectory(Blog blog, String categoryName) {
        File blogCategory = new File(blog.getBlogHome() + BlojsomUtils.removeInitialSlash(categoryName));
        if (blogCategory.exists() && blogCategory.isDirectory()) {
            return blogCategory;
        } else {
            return new File(blog.getBlogHome() + "/");
        }
    }

    /**
     * Return a content type up to the first ; character
     *
     * @param contentType Content type
     * @return Content type without any trailing information after a ;
     */
    protected String sanitizeContentType(String contentType) {
        int semicolonIndex = contentType.indexOf(";");
        if (semicolonIndex != -1) {
            return contentType.substring(0, semicolonIndex);
        }

        return contentType;
    }
}
