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
package org.blojsom.plugin.crosspost.beans;

/**
 * Destination POJO
 *
 * @author Mark Lussier
 * @version $Id: Destination.java,v 1.1.2.1 2005/07/21 04:30:29 johnan Exp $
 * @since blojsom 2.23
 */
public class Destination {

    private String _title;
    private String _url;
    private int _apiType;
    private String _userId;
    private String _password;
    private String _blogId;
    private String _category;

    /**
     * Default constructor
     */
    public Destination() {
    }

    /**
     * Title of the destination
     *
     * @return Title
     */
    public String getTitle() {
        return _title;
    }

    /**
     * Set the title of the destination
     *
     * @param title Title
     */
    public void setTitle(String title) {
        this._title = title;
    }

    /**
     * XML-RPC URL for the destination
     *
     * @return XML-RPC URL
     */
    public String getUrl() {
        return _url;
    }

    /**
     * Set the XML-RPC URL for the destination
     *
     * @param url XML-RPC URL
     */
    public void setUrl(String url) {
        this._url = url;
    }

    /**
     * Get the API type for the destination. Currently 0 = Blogger, 1 = MetaWeblog
     *
     * @return API type for destination
     */
    public int getApiType() {
        return _apiType;
    }

    /**
     * Set the API type for the destination. Currently 0 = Blogger, 1 = MetaWeblog
     *
     * @param apiType API type for destination
     */
    public void setApiType(int apiType) {
        this._apiType = apiType;
    }

    /**
     * Get the user ID posting to the destination
     *
     * @return User ID
     */
    public String getUserId() {
        return _userId;
    }

    /**
     * Set the user ID posting to the destination
     *
     * @param userId User ID posting to the destination
     */
    public void setUserId(String userId) {
        this._userId = userId;
    }

    /**
     * Get the password for the user ID posting to the destination
     *
     * @return Password for the user ID posting to the destination
     */
    public String getPassword() {
        return _password;
    }

    /**
     * Set the password for the user ID posting to the destination
     *
     * @param password Password for the user ID posting to the destination
     */
    public void setPassword(String password) {
        this._password = password;
    }

    /**
     * Get the blog ID of the destination
     *
     * @return Blog ID of the destination
     */
    public String getBlogId() {
        return _blogId;
    }

    /**
     * Set the blog ID of the destination
     *
     * @param blogId Blog ID of the destination
     */
    public void setBlogId(String blogId) {
        this._blogId = blogId;
    }

    /**
     * Get the category of the destination
     *
     * @return category of the destination
     */
    public String getCategory() {
        return _category;
    }

    /**
     * Set the category of the destination
     *
     * @param category Category of the destination
     */
    public void setCategory(String category) {
        this._category = category;
    }
}
