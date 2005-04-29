/**
 * Copyright (c) 2003-2004 , David A. Czarnecki
 * All rights reserved.
 *
 * Portions Copyright (c) 2003-2004  by Mark Lussier
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

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.BlojsomException;
import org.blojsom.util.BlojsomConstants;
import org.blojsom.util.BlojsomMetaDataConstants;
import org.blojsom.util.BlojsomUtils;

import java.text.SimpleDateFormat;
import java.util.*;

/**
 * BlogEntry
 *
 * @author David Czarnecki
 * @version $Id: BlogEntry.java,v 1.3 2004/08/27 01:13:55 whitmore Exp $
 */
public abstract class BlogEntry implements BlojsomConstants, BlojsomMetaDataConstants {

    protected transient Log _logger = LogFactory.getLog(BlogEntry.class);

    protected String _title;
    protected String _link;
    protected String _description;
    protected String _category;
    protected Date _entryDate;
    protected long _lastModified;
    protected List _comments;
    protected List _trackbacks;
    protected BlogCategory _blogCategory;
    protected Map _metaData;

    /**
     * Create a new blog entry with no data
     */
    public BlogEntry() {
    }

    /**
     * Date of the blog entry
     * <p/>
     * This value is constructed from the lastModified value of the file
     *
     * @return Date of the blog entry
     */
    public Date getDate() {
        return _entryDate;
    }

    /**
     * Date of this blog entry
     *
     * @param entryDate Date of the blog entry
     */
    public void setDate(Date entryDate) {
        _entryDate = entryDate;
    }

    /**
     * Return an RFC 822 style date
     *
     * @return Date formatted in RFC 822 format
     */
    public String getRFC822Date() {
        return BlojsomUtils.getRFC822Date(_entryDate);
    }


    /**
     * Return an UTC style date
     *
     * @return Date formatted in UTC format
     */
    public String getUTCDate() {
        return BlojsomUtils.getUTCDate(_entryDate);
    }


    /**
     * Return an ISO 8601 style date
     * http://www.w3.org/TR/NOTE-datetime
     *
     * @return Date formatted in ISO 8601 format
     */
    public String getISO8601Date() {
        return BlojsomUtils.getISO8601Date(_entryDate);
    }

    /**
     * Return the blog entry date formatted with a specified date format and time zone
     *
     * @since blojsom 2.1.4 (blojsom-6 Apple)
     * @param format Date format
	 * @param timeZoneID ID of the time zone you want to retrieve, or null for the current one.
     * @return <code>null</code> if the entry date or format is null, otherwise returns the entry date formatted to the specified format. If the format is invalid, returns <tt>entryDate.toString()</tt>
     */
	public String getDateAsFormat(String format, String timeZoneID) {
        if (_entryDate == null || format == null) {
            return null;
        }

        SimpleDateFormat sdf = null;
        try {
            sdf = new SimpleDateFormat(format);
			if (timeZoneID != null) {
				sdf.setTimeZone(java.util.TimeZone.getTimeZone(timeZoneID));
			}
            return sdf.format(_entryDate);
        } catch (IllegalArgumentException e) {
            return _entryDate.toString();
        }
    }
    
    /**
     * Return the blog entry date formatted with a specified date format
     *
     * @since blojsom 1.9.3
     * @param format Date format
     * @return <code>null</code> if the entry date or format is null, otherwise returns the entry date formatted to the specified format. If the format is invalid, returns <tt>entryDate.toString()</tt>
     */
    public String getDateAsFormat(String format) {
        return getDateAsFormat(format, null);
    }

    /**
     * Title of the blog entry
     *
     * @return Blog title
     */
    public String getTitle() {
        return _title;
    }

    /**
     * Set the title of the blog entry
     *
     * @param title Title for the blog entry
     */
    public void setTitle(String title) {
        _title = title;
    }

    /**
     * Title for the entry where the &lt;, &gt;, and &amp; characters are escaped
     *
     * @return Escaped entry title
     */
    public String getEscapedTitle() {
        return BlojsomUtils.escapeString(_title);
    }

    /**
     * Permalink for the blog entry
     *
     * @return Blog entry permalink
     */
    public String getLink() {
        return _link;
    }

    /**
     * Permalink for the blog entry where the &lt;, &gt;, and &amp; characters are escaped
     *
     * @return Blog entry permalink which has been escaped
     */
    public String getEscapedLink() {
        return BlojsomUtils.escapeString(_link);
    }

    /**
     * Set the permalink for the blog entry
     *
     * @param link Permalink for the blog entry
     */
    public void setLink(String link) {
        _link = link;
    }

    /**
     * Description of the blog entry
     *
     * @return Blog entry description
     */
    public String getDescription() {
        return _description;
    }

    /**
     * Escaped description of the blog entry
     * This method would be used for generating RSS feeds where the &lt;, &gt;, and &amp; characters are escaped
     *
     * @return Blog entry description where &amp;, &lt;, and &gt; have been escaped
     */
    public String getEscapedDescription() {
        return BlojsomUtils.escapeString(_description);
    }

    /**
     * Set the description for the blog entry
     *
     * @param description Description for the blog entry
     */
    public void setDescription(String description) {
        _description = description;
    }

    /**
     * Last modified time for the blog entry
     *
     * @return Blog entry last modified time
     */
    public long getLastModified() {
        return _lastModified;
    }

    /**
     * Returns the BlogId  for this entry
     *
     * @return Blog Id
     */
    public abstract String getId();

    /**
     * Return the permalink name for this blog entry
     *
     * @return Permalink name
     */
    public abstract String getPermalink();

    /**
     * Category for the blog entry. This corresponds to the category directory name.
     *
     * @return Blog entry category
     */
    public String getCategory() {
        return _category;
    }

    /**
     * Return the category name encoded. 
     *
     * @return Category name encoded as UTF-8
     * @since blojsom 2.08
     */
    public String getEncodedCategory() {
        if (_category == null) {
            return null;
        }

        if (!_category.startsWith("/")) {
            return "/" + BlojsomUtils.urlEncodeForLink(_category);
        }

        return BlojsomUtils.urlEncodeForLink(_category);
    }

    /**
     * Set the category for the blog entry. This corresponds to the category directory name.
     *
     * @param category Category for the blog entry
     */
    public void setCategory(String category) {
        _category = category;
    }

    /**
     * Checks to see if the link to this entry is the same as the input entry
     *
     * @see java.lang.Object#equals(java.lang.Object)
     */
    public boolean equals(Object o) {
        if (!(o instanceof BlogEntry)) {
            return false;
        }

        BlogEntry entry = (BlogEntry) o;
        if (_link.equals(entry.getLink())) {
            return true;
        }
        return false;
    }

    /**
     * Determines whether or not this blog entry supports comments.
     *
     * @return <code>true</code> if the blog entry supports comments, <code>false</code> otherwise
     */
    public abstract boolean supportsComments();

    /**
     * Get the comments
     *
     * @return List of comments
     */
    public List getComments() {
        if (_comments == null) {
            return new ArrayList();
        }

        return _comments;
    }

    /**
     * Set the comments for this blog entry. The comments must be an <code>List</code>
     * of {@link BlogComment}. This method will not writeback or change the comments on disk.
     *
     * @param comments Comments for this entry
     */
    public void setComments(List comments) {
        _comments = comments;
    }

    /**
     * Get the comments as an array of BlogComment objects
     *
     * @return BlogComment[] array
     */
    public BlogComment[] getCommentsAsArray() {
        if (_comments == null) {
            return new BlogComment[0];
        } else {
            return (BlogComment[]) _comments.toArray(new BlogComment[_comments.size()]);
        }
    }

    /**
     * Get the number of comments for this entry
     *
     * @return 0 if comments is <code>null</code>, or the number of comments otherwise, which could be 0
     */
    public int getNumComments() {
        if (_comments == null) {
            return 0;
        } else {
            return _comments.size();
        }
    }

    /**
     * Determines whether or not this blog entry supports trackbacks.
     *
     * @return <code>true</code> if the blog entry supports trackbacks, <code>false</code> otherwise
     * @since blojsom 2.05
     */
    public abstract boolean supportsTrackbacks();

    /**
     * Get the trackbacks
     *
     * @return List of trackbacks
     */
    public List getTrackbacks() {
        if (_trackbacks == null) {
            return new ArrayList();
        }

        return _trackbacks;
    }

    /**
     * Set the trackbacks for this blog entry. The trackbacks must be an <code>List</code>
     * of {@link Trackback}. This method will not writeback or change the trackbacks to disk.
     *
     * @param trackbacks Trackbacks for this entry
     */
    public void setTrackbacks(List trackbacks) {
        _trackbacks = trackbacks;
    }

    /**
     * Get the trackbacks as an array of Trackback objects
     *
     * @return Trackback[] array
     */
    public Trackback[] getTrackbacksAsArray() {
        if (_trackbacks == null) {
            return new Trackback[0];
        } else {
            return (Trackback[]) _trackbacks.toArray(new Trackback[_trackbacks.size()]);
        }
    }

    /**
     * Get the number of trackbacks for this entry
     *
     * @return 0 if trackbacks is <code>null</code>, or the number of trackbacks otherwise, which could be 0
     */
    public int getNumTrackbacks() {
        if (_trackbacks == null) {
            return 0;
        } else {
            return _trackbacks.size();
        }
    }

    /**
     * Get the {@link BlogCategory} object for this blog entry
     *
     * @return {@link BlogCategory} object
     */
    public BlogCategory getBlogCategory() {
        return _blogCategory;
    }

    /**
     * Set the {@link BlogCategory} object for this blog entry
     *
     * @param blogCategory New {@link BlogCategory} object
     */
    public void setBlogCategory(BlogCategory blogCategory) {
        _blogCategory = blogCategory;
    }

    /**
     * Return meta data for this blog entry. This method may return <code>null</code>.
     *
     * @return Meta data
     * @since blojsom 1.8
     */
    public Map getMetaData() {
        if (_metaData == null) {
            return new HashMap();
        }
        
        return _metaData;
    }

    /**
     * Set the meta-data associated with this blog entry
     *
     * @param metaData Meta-data
     * @since blojsom 1.8
     */
    public void setMetaData(Map metaData) {
        _metaData = metaData;
    }

    /**
     * Return a string representation of the entry. The default implementation is to return
     * the blog entry title.
     *
     * @return String representation of this entry
     * @since blojsom 1.9
     */
    public String toString() {
        return _title;
    }

    /**
     * Set any attributes of the blog entry using data from the map.
     *
     * @param attributeMap Attributes
     * @since blojsom 1.9
     */
    public void setAttributes(Map attributeMap) {
    }

    /**
     * Load a blog entry.
     *
     * @param blogUser User information
     * @throws BlojsomException If there is an error loading the entry
     * @since blojsom 1.9
     */
    public abstract void load(BlogUser blogUser) throws BlojsomException;

    /**
     * Save the blog entry.
     *
     * @param blogUser User information
     * @throws BlojsomException If there is an error saving the entry
     * @since blojsom 1.9
     */
    public abstract void save(BlogUser blogUser) throws BlojsomException;

    /**
     * Delete the blog entry.
     *
     * @param blogUser User information
     * @throws BlojsomException If there is an error deleting the entry
     * @since blojsom 1.9
     */
    public abstract void delete(BlogUser blogUser) throws BlojsomException;
}
