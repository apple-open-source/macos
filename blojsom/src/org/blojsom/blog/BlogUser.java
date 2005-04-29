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

import java.util.Map;
import org.blojsom.blog.Blog;

/**
 * BlogUser
 *
 * @author David Czarnecki
 * @since blojsom 2.0
 * @version $Id: BlogUser.java,v 1.3 2005/01/18 19:18:38 johnan Exp $
 */
public class BlogUser implements Comparable {

    protected String _id;
    protected Blog _blog;
    protected Map _pluginChain;
    protected Map _flavors;
    protected Map _flavorToTemplate;
    protected Map _flavorToContentType;

    /**
     * Default constructor
     */
    public BlogUser() {
    }

    /**
     * Set the blog user's id
     *
     * @param id Blog user id, more commonly known as their username
     */
    public void setId(String id) {
        _id = id;
    }

    /**
     * Get the blog user's id
     *
     * @return Blog user id
     */
    public String getId() {
        return _id;
    }

    /**
     * Get the blog user's blog information
     *
     * @return {@link Blog} information
     */
    public Blog getBlog() {
        return _blog;
    }

    /**
     * Set the blog user's blog information
     *
     * @param blog {@link Blog} information
     */
    public void setBlog(Blog blog) {
        _blog = blog;
    }

    /**
     * Return the plugin chains for the blog user
     *
     * @return Map of the plugin chains for the blog user. The map will contain entries keyed on the
     * particular flavor with values corresponding to a <code>String[]</code> for the plugins to be
     * executed for that flavor
     */
    public Map getPluginChain() {
        return _pluginChain;
    }

    /**
     * Set the flavor-based plugin chains for the blog user
     *
     * @param pluginChain Map of the plugin chains for the blog user. The map will contain entries keyed on the
     * particular flavor with values corresponding to a <code>String[]</code> for the plugins to be
     * executed for that flavor
     */
    public void setPluginChain(Map pluginChain) {
        _pluginChain = pluginChain;
    }

    /**
     * Get the supported flavors for the blog user
     *
     * @return Map of the flavors available for this user. The map will contain entries keyed on the
     * particular flavor with a corresponding value set to the same key
     */
    public Map getFlavors() {
        return _flavors;
    }

    /**
     * Set the supported flavors for the blog user
     *
     * @param flavors Map of the flavors available for this user. The map will contain entries keyed on the
     * particular flavor with a corresponding value set to the same key
     */
    public void setFlavors(Map flavors) {
        _flavors = flavors;
    }

    /**
     * Get the flavor to template mapping for the blog user
     *
     * @return Map of the flavors available for this user. The map will contain entries keyed on the
     * particular flavor with a corresponding value set to the template that should be used in
     * rendering the flavor
     */
    public Map getFlavorToTemplate() {
        return _flavorToTemplate;
    }

    /**
     * Set the flavor to template mapping for the blog user
     *
     * @param flavorToTemplate Map of the flavors available for this user. The map will contain entries keyed on the
     * particular flavor with a corresponding value set to the template that should be used in
     * rendering the flavor
     */
    public void setFlavorToTemplate(Map flavorToTemplate) {
        _flavorToTemplate = flavorToTemplate;
    }

    /**
     * Get the flavor to content-type mapping for the blog user
     *
     * @return Map of the flavors available for this user. The map will contain entries keyed on the
     * particular flavor with a corresponding value set to the content-type that should be used in
     * rendering the flavor
     */
    public Map getFlavorToContentType() {
        return _flavorToContentType;
    }

    /**
     * Set the flavor to content-type mapping for the blog user
     *
     * @param flavorToContentType Map of the flavors available for this user. The map will contain entries keyed on the
     * particular flavor with a corresponding value set to the content-type that should be used in
     * rendering the flavor
     */
    public void setFlavorToContentType(Map flavorToContentType) {
        _flavorToContentType = flavorToContentType;
    }
	
    /**
     * Compare this user to another user.
     *
     * @param anotherUser Other user to compare to.
     */
	public int compareTo(Object anotherUser) {
		if (!(anotherUser instanceof BlogUser)) {
			throw new ClassCastException("A BlogUser object expected.");
		}
		
		String thisEscapedBlogName = this.getBlog().getEscapedBlogName();
		String otherEscapedBlogName = ((BlogUser)anotherUser).getBlog().getEscapedBlogName();
		
		return thisEscapedBlogName.compareTo(otherEscapedBlogName);
	}
}
