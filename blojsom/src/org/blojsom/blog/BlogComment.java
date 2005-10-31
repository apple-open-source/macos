/**
 * Copyright (c) 2003-2005 , David A. Czarnecki
 * All rights reserved.
 *
 * Portions Copyright (c) 2003-2005  by Mark Lussier
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
package org.blojsom.blog;

import org.blojsom.util.BlojsomUtils;

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.HashMap;
import java.util.Map;
import java.io.Serializable;

/**
 * BlogComment
 *
 * @author David Czarnecki
 * @version $Id: BlogComment.java,v 1.2.2.1 2005/07/21 14:11:02 johnan Exp $
 */
public class BlogComment implements Serializable{

    protected String _author;
    protected String _authorEmail;
    protected String _authorURL;
    protected String _comment;
    protected Date _commentDate;
    protected long _commentDateLong;
    protected String _id;
    protected Map _metaData;
    protected BlogEntry _blogEntry;

    /**
     * Default constructor
     */
    public BlogComment() {
    }

    /**
     * Get the author of the comment
     *
     * @return Comment author
     */
    public String getAuthor() {
        return _author;
    }

    /**
     * Set the author of the comment
     *
     * @param author Comment's new author
     */
    public void setAuthor(String author) {
        _author = author;
    }

    /**
     * Get the e-mail of the author of the comment
     *
     * @return Author's e-mail
     */
    public String getAuthorEmail() {
        return _authorEmail;
    }

    /**
     * Set the e-mail of the author of the comment
     *
     * @param authorEmail Author's new e-mail
     */
    public void setAuthorEmail(String authorEmail) {
        _authorEmail = authorEmail;
    }

    /**
     * Get the URL of the author
     *
     * @return Author's URL
     */
    public String getAuthorURL() {
        return _authorURL;
    }

    /**
     * Set the URL for the author
     *
     * @param authorURL New URL for the author
     */
    public void setAuthorURL(String authorURL) {
        _authorURL = authorURL;
    }

    /**
     * Get the comment as a escaped string 
     * @return Escaped Comment
     */
    public String getEscapedComment() {
        return BlojsomUtils.escapeString(_comment);
    }

    /**
     * Get the comment
     *
     * @return Comment
     */
    public String getComment() {
        return _comment;
    }

    /**
     * Set the new comment
     *
     * @param comment New comment
     */
    public void setComment(String comment) {
        _comment = comment;
    }

    /**
     * Get the date the comment was entered
     *
     * @return Comment date
     */
    public Date getCommentDate() {
        return _commentDate;
    }

    /**
     * Return an ISO 8601 style date
     * http://www.w3.org/TR/NOTE-datetime
     *
     * @return Date formatted in ISO 8601 format
     */
    public String getISO8601Date() {
        return BlojsomUtils.getISO8601Date(_commentDate);
    }

    /**
     * Return an RFC 822 style date
     *
     * @return Date formatted in RFC 822 format
     */
    public String getRFC822Date() {
        return BlojsomUtils.getRFC822Date(_commentDate);
    }

    /**
     * Get the trackback meta-data
     *
     * @return Meta-data as a {@link Map}
     * @since blojsom 2.14
     */
    public Map getMetaData() {
        if (_metaData == null) {
            return new HashMap();
        }

        return _metaData;
    }

    /**
     * Set the date for the comment
     *
     * @param commentDate Comment date
     */
    public void setCommentDate(Date commentDate) {
        _commentDate = commentDate;
    }

    /**
     * Get the date of this comment as a Long.
     * Used for Last-Modified
     *
     * @return the comment date as a Long
     */
    public long getCommentDateLong() {
        return _commentDateLong;
    }

    /**
     * Set the Comment Date as a Long
     *
     * @param commentDateLong the comment file's lastModified()
     */
    public void setCommentDateLong(long commentDateLong) {
        _commentDateLong = commentDateLong;
    }

    /**
     * Get the id of this blog comments
     *
     * @return Id
     * @since blojsom 2.07
     */
    public String getId() {
        return _id;
    }

    /**
     * Set the id of this blog comment. This method can only be called if the id has not been set.
     *
     * @param id New id
     * @since blojsom 2.07
     */
    public void setId(String id) {
        if (_id == null) {
            _id = id;
        }
    }

    /**
     * Set the trackback meta-data
     *
     * @param metaData {@link Map} containing meta-data for this comment
     * @since blojsom 2.14
     */
    public void setMetaData(Map metaData) {
        _metaData = metaData;
    }

    /**
     * Return the comment date formatted with a specified date format
     *
     * @param format Date format
     * @return <code>null</code> if the comment date or format is null, otherwise returns the comment date
     *         formatted to the specified format. If the format is invalid, returns <tt>commentDate.toString()</tt>
     * @since blojsom 2.19
     */
    public String getDateAsFormat(String format) {
        if (_commentDate == null || format == null) {
            return null;
        }

        SimpleDateFormat sdf = null;
        try {
            sdf = new SimpleDateFormat(format);
            return sdf.format(_commentDate);
        } catch (IllegalArgumentException e) {
            return _commentDate.toString();
        }
    }

    /**
     * Retrieve the {@link BlogEntry} associated with this comment
     *
     * @return {@link BlogEntry}
     * @since blojsom 2.23
     */
    public BlogEntry getBlogEntry() {
        return _blogEntry;
    }

    /**
     * Set the {@link BlogEntry} associated with this comment
     *
     * @param blogEntry {@link BlogEntry}
     * @since blojsom 2.23
     */
    public void setBlogEntry(BlogEntry blogEntry) {
        _blogEntry = blogEntry;
    }
}
