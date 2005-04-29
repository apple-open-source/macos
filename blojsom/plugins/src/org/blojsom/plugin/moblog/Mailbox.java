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

import org.blojsom.blog.BlogUser;
import org.blojsom.util.BlojsomUtils;

import java.util.Map;
import java.util.HashMap;

/**
 * Mailbox.
 * <p/>
 * This file is a container for everything the thread will need to connect to and to store into.
 *
 * @author David Czarnecki
 * @author Mark Lussier
 * @version $Id: Mailbox.java,v 1.1 2004/08/27 01:06:39 whitmore Exp $
 * @since blojsom 2.14
 */
public class Mailbox {

    private static final String DEFAULT_FOLDER = "INBOX";

    private String _hostName;
    private String _userId;
    private String _password;
    private String _folder = DEFAULT_FOLDER;
    private String _outputDirectory;
    private String _entriesDirectory;
    private String _categoryName;
    private String _urlPrefix;
    private String _secretWord;
    private boolean _enabled;
    private BlogUser _user;
    private Map _imageMimeTypes;
    private Map _attachmentMimeTypes;
    private Map _textMimeTypes;
    private Map _authorizedAddresses;

    /**
     * Default constructor.
     */
    public Mailbox() {
        _enabled = false;
        _imageMimeTypes = new HashMap();
        _attachmentMimeTypes = new HashMap();
        _textMimeTypes = new HashMap();
        _authorizedAddresses = new HashMap();
    }

    /**
     * Construct a new mailbox for a given hostname, user, and password.
     *
     * @param hostname Mailbox hostname
     * @param userid Mailbox user id
     * @param password Mailbox user password
     */
    public Mailbox(String hostname, String userid, String password) {
        _hostName = hostname;
        _userId = userid;
        _password = password;
    }

    /**
     * Retrieve the mailbox hostname
     *
     * @return Mailbox hostname (e.g. mail.domain.com)
     */
    public String getHostName() {
        return _hostName;
    }

    /**
     * Set the mailbox hostname
     *
     * @param hostName Mailbox hostname (e.g. mail.domain.com)
     */
    public void setHostName(String hostName) {
        _hostName = hostName;
    }

    /**
     * Retreive the mailbox user id
     *
     * @return Mailbox user id
     */
    public String getUserId() {
        return _userId;
    }

    /**
     * Set the mailbox user id
     *
     * @param userId Mailbox user id
     */
    public void setUserId(String userId) {
        _userId = userId;
    }

    /**
     * Retrieve the mailbox user password
     *
     * @return Mailbox user password
     */
    public String getPassword() {
        return _password;
    }

    /**
     * Set the mailbox user password
     *
     * @param password Mailbox user password
     */
    public void setPassword(String password) {
        _password = password;
    }

    /**
     * Retrieve the mail folder
     *
     * @return Mail folder
     */
    public String getFolder() {
        return _folder;
    }

    /**
     * Set the mail folder
     *
     * @param folder Mail folder
     */
    public void setFolder(String folder) {
        _folder = folder;
    }

    /**
     * Retrieve the output directory where attachments will be written
     *
     * @return Output directory
     */
    public String getOutputDirectory() {
        return _outputDirectory;
    }

    /**
     * Set the output directory where attachments will be written
     * @param outputDirectory Output directory
     */
    public void setOutputDirectory(String outputDirectory) {
        _outputDirectory = outputDirectory;
    }

    /**
     * Retrieve the URL prefix for linking to attachments
     *
     * @return URL prefix (e.g. http://www.blog.com/resources/)
     */
    public String getUrlPrefix() {
        return _urlPrefix;
    }

    /**
     * Set the URL prefix for linking to attachments
     *
     * @param urlPrefix (e.g. http://www.blog.com/resources/)
     */
    public void setUrlPrefix(String urlPrefix) {
        _urlPrefix = urlPrefix;
    }

    /**
     * Retrive the directory where new blog entries will be created
     *
     * @return Entries directory
     */
    public String getEntriesDirectory() {
        return _entriesDirectory;
    }

    /**
     * Set the directory where new blog entries will be created
     *
     * @param entriesDirectory Entries directory
     */
    public void setEntriesDirectory(String entriesDirectory) {
        _entriesDirectory = entriesDirectory;
    }

    /**
     * Retrieve the category name for new moblog entries
     *
     * @return Category name
     */
    public String getCategoryName() {
        return _categoryName;
    }

    /**
     * Set the category name for new moblog entries
     *
     * @param categoryName Category name
     */
    public void setCategoryName(String categoryName) {
        _categoryName = categoryName;
    }

    /**
     * Retrieve whether or not this mailbox is enabled
     *
     * @return <code>true</code> if the mailbox is enabled, <code>false</code> otherwise
     */
    public boolean isEnabled() {
        return _enabled;
    }

    /**
     * Set whether or not this mailbox is enabled
     *
     * @param enabled <code>true</code> if the mailbox is enabled, <code>false</code> otherwise
     */
    public void setEnabled(boolean enabled) {
        _enabled = enabled;
    }

    /**
     * Retrive the {@link BlogUser} for this mailbox
     *
     * @return {@link BlogUser}
     */
    public BlogUser getBlogUser() {
        return _user;
    }

    /**
     * Set the {@link BlogUser} for this mailbox
     *
     * @param user {@link BlogUser}
     */
    public void setBlogUser(BlogUser user) {
        this._user = user;
    }

    /**
     * Retrieve the {@link Map} of image mime-types
     *
     * @return {@link Map} of image mime-types
     */
    public Map getImageMimeTypes() {
        return _imageMimeTypes;
    }

    /**
     * Retrieve the accepted image MIME types as a string list
     *
     * @return String list of accepted image mime types
     * @since blojsom 2.16
     */
    public String getImageMimeTypesAsStringList() {
        return BlojsomUtils.getKeysAsStringList(_imageMimeTypes);
    }

    /**
     * Set the {@link Map} of image mime-types
     *
     * @param imageMimeTypes {@link Map} of image mime-types
     */
    public void setImageMimeTypes(Map imageMimeTypes) {
        _imageMimeTypes = imageMimeTypes;
    }

    /**
     * Retrieve the {@link Map} of attachment mime-types
     *
     * @return {@link Map} of image mime-types
     */
    public Map getAttachmentMimeTypes() {
        return _attachmentMimeTypes;
    }

    /**
     * Retrieve the accepted attachment MIME types as a string list
     *
     * @return String list of accepted attachment mime types
     * @since blojsom 2.16
     */
    public String getAttachmentMimeTypesAsStringList() {
        return BlojsomUtils.getKeysAsStringList(_attachmentMimeTypes);
    }

    /**
     * Set the {@link Map} of attachment mime-types
     *
     * @param attachmentMimeTypes {@link Map} of attachment mime-types
     */
    public void setAttachmentMimeTypes(Map attachmentMimeTypes) {
        _attachmentMimeTypes = attachmentMimeTypes;
    }

    /**
     * Retrieve the {@link Map} of text mime-types
     *
     * @return {@link Map} of image mime-types
     */
    public Map getTextMimeTypes() {
        return _textMimeTypes;
    }

    /**
     * Retrieve the accepted text MIME types as a string list
     *
     * @return String list of accepted text mime types
     * @since blojsom 2.16
     */
    public String getTextMimeTypesAsStringList() {
        return BlojsomUtils.getKeysAsStringList(_textMimeTypes);
    }

    /**
     * Set the {@link Map} of text mime-types
     *
     * @param textMimeTypes {@link Map} of text mime-types
     */
    public void setTextMimeTypes(Map textMimeTypes) {
        _textMimeTypes = textMimeTypes;
    }

    /**
     * Retrieve the secret word for this mailbox
     *
     * @since blojsom 2.15
     * @return Secret word which must be present at the start of the subject of the e-mail
     */
    public String getSecretWord() {
        return _secretWord;
    }

    /**
     * Set the secret word for this mailbox.
     *
     * @since blojsom 2.15
     * @param secretWord Secret word which must be present at the start of the subject of the e-mail
     */
    public void setSecretWord(String secretWord) {
        _secretWord = secretWord;
    }

    /**
     * Retrieve the authorized e-mail from addresses for this mailbox
     *
     * @return Authorized e-mail from addresses for this mailbox
     * @since blojsom 2.16
     */
    public Map getAuthorizedAddresses() {
        return _authorizedAddresses;
    }

    /**
     * Set the authorized e-mail from addresses for this mailbox
     *
     * @param authorizedAddresses Authorized e-mail from addresses for this mailbox
     * @since blojsom 2.16
     */
    public void setAuthorizedAddresses(Map authorizedAddresses) {
        _authorizedAddresses = authorizedAddresses;
    }
}
