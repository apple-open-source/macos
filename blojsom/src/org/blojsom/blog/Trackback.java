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

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.HashMap;
import java.util.Map;
import java.io.Serializable;

/**
 * Trackback
 *
 * @author David Czarnecki
 * @version $Id: Trackback.java,v 1.2.2.1 2005/07/21 14:11:02 johnan Exp $
 */
public class Trackback implements Serializable {

    protected String _title;
    protected String _excerpt;
    protected String _url;
    protected String _blogName;
    protected long _trackbackDateLong;
    protected String _id;
    protected Map _metaData;
    protected BlogEntry _blogEntry;

    /**
     * Default constructor
     */
    public Trackback() {
    }

    /**
     * Trackback constructor to take a title, excerpt, url, and blog name
     *
     * @param title    Title of the trackback
     * @param excerpt  Excerpt from the trackback
     * @param url      Url for the trackback
     * @param blogName Blog name of the trackback
     */
    public Trackback(String title, String excerpt, String url, String blogName) {
        _title = title;
        _excerpt = excerpt;
        _url = url;
        _blogName = blogName;
    }

    /**
     * Get the title of the trackback
     *
     * @return Trackback title
     */
    public String getTitle() {
        return _title;
    }

    /**
     * Set the title of the trackback
     *
     * @param title Trackback title
     */
    public void setTitle(String title) {
        _title = title;
    }

    /**
     * Get the excerpt of the trackback
     *
     * @return Trackback excerpt
     */
    public String getExcerpt() {
        return _excerpt;
    }

    /**
     * Set the excerpt of the trackback
     *
     * @param excerpt Trackback excerpt
     */
    public void setExcerpt(String excerpt) {
        _excerpt = excerpt;
    }

    /**
     * Get the url of the trackback
     *
     * @return Trackback url
     */
    public String getUrl() {
        return _url;
    }

    /**
     * Set the url of the trackback
     *
     * @param url Trackback url
     */
    public void setUrl(String url) {
        _url = url;
    }

    /**
     * Get the blog name of the trackback
     *
     * @return Trackback blog name
     */
    public String getBlogName() {
        return _blogName;
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
     * Set the blog name of the trackback
     *
     * @param blogName Trackback blog name
     */
    public void setBlogName(String blogName) {
        _blogName = blogName;
    }

    /**
     * Set the date for the trackback
     *
     * @param trackbackDateLong Trackback date as a <code>long</code> value
     */
    public void setTrackbackDateLong(long trackbackDateLong) {
        _trackbackDateLong = trackbackDateLong;
    }

    /**
     * Get the date of the trackback
     *
     * @return Date of the trackback as a <code>long</code>
     */
    public long getTrackbackDateLong() {
        return _trackbackDateLong;
    }

    /**
     * Get the date of the trackback
     *
     * @return Date of the trackback as a <code>java.util.Date</code>
     */
    public Date getTrackbackDate() {
        return new Date(_trackbackDateLong);
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
     * @param metaData {@link Map} containing meta-data for this trackback
     * @since blojsom 2.14
     */
    public void setMetaData(Map metaData) {
        _metaData = metaData;
    }

    /**
     * Return the trackback date formatted with a specified date format
     *
     * @param format Date format
     * @return <code>null</code> if the format is null, otherwise returns the trackback date formatted to
     *         the specified format. If the format is invalid, returns <tt>trackbackDate.toString()</tt>
     * @since blojsom 2.19
     */
    public String getDateAsFormat(String format) {
        if (format == null) {
            return null;
        }

        SimpleDateFormat sdf = null;
        Date trackbackDate = new Date(_trackbackDateLong);
        try {
            sdf = new SimpleDateFormat(format);
            return sdf.format(trackbackDate);
        } catch (IllegalArgumentException e) {
            return trackbackDate.toString();
        }
    }

    /**
     * Retrieve the {@link BlogEntry} associated with this trackback
     *
     * @return {@link BlogEntry}
     * @since blojsom 2.23
     */
    public BlogEntry getBlogEntry() {
        return _blogEntry;
    }

    /**
     * Set the {@link BlogEntry} associated with this trackback
     *
     * @param blogEntry {@link BlogEntry}
     * @since blojsom 2.23
     */
    public void setBlogEntry(BlogEntry blogEntry) {
        _blogEntry = blogEntry;
    }
}
