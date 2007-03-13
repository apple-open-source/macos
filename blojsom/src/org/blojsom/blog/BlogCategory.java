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

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.util.BlojsomConstants;
import org.blojsom.util.BlojsomUtils;
import org.blojsom.BlojsomException;

import java.util.*;
import java.io.Serializable;

/**
 * BlogCategory
 *
 * @author David Czarnecki
 * @author Mark Lussier
 * @version $Id: BlogCategory.java,v 1.2.2.2 2006/10/19 21:14:32 johnan Exp $
 */
public abstract class BlogCategory implements Comparable, Serializable {

    protected Log _logger = LogFactory.getLog(BlogCategory.class);

    protected String _categoryURL;
    protected String _category;
    protected Map _metadata = null;
    protected String _description = null;
    protected String _name = null;
    protected List _subcategories;
    protected BlogEntry _blogEntry;
    protected BlogCategory _parentCategory = null;

    /**
     * Create a new BlogCategory.
     */
    public BlogCategory() {
        _subcategories = new ArrayList();
    }

    /**
     * Create a new BlogCategory.
     *
     * @param category    Category name
     * @param categoryURL Category URL
     */
    public BlogCategory(String category, String categoryURL) {
        super();
        _category = BlojsomUtils.normalize(category);
        _categoryURL = categoryURL;
    }

    /**
     * Return the URL for this category
     *
     * @return Category URL
     */
    public String getCategoryURL() {
        return _categoryURL;
    }

    /**
     * Set a new URL for this category
     *
     * @param categoryURL Category URL
     */
    public void setCategoryURL(String categoryURL) {
        _categoryURL = categoryURL;
    }

    /**
     * Return the category name
     *
     * @return Category name
     */
    public String getCategory() {
        return _category;
    }

    /**
     * Return the category name encoded for a link
     *
     * @return Category name encoded as UTF-8 with preserved "/" and "+" characters
     * @since blojsom 2.08
     */
    public String getEncodedCategory() {
        return BlojsomUtils.urlEncodeForLink(_category);
    }

    /**
     * Return the category URL encoded for a link
     *
     * @return Category URL encoded as UTF-8 with preserved "/" characters and "+" characters
     * @see {@link BlojsomUtils#urlEncodeForLink(java.lang.String)}
     * @since blojsom 2.14
     */
    public String getEncodedCategoryURL() {
        return BlojsomUtils.urlEncodeForLink(_categoryURL);
    }

    /**
     * Set a new name for this category
     *
     * @param category Category name
     */
    public void setCategory(String category) {
        _category = BlojsomUtils.normalize(category);
    }

    /**
     * Checks to see if this category is equal to the input category
     *
     * @param obj Input category
     * @return <code>true</code> if the category name and category URL are equal, <code>false</code> otherwise
     */
    public boolean equals(Object obj) {
        BlogCategory otherCategory = (BlogCategory) obj;
        return ((_category.equals(otherCategory._category)) && (_categoryURL.equals(otherCategory._categoryURL)));
    }

    /**
     * Compare the current category to the input category
     *
     * @param o Input category
     * @return
     */
    public int compareTo(Object o) {
        BlogCategory category = (BlogCategory) o;
        return _category.compareTo(category._category);
    }

    /**
     * Returns the category name
     *
     * @return Category name
     * @see #getCategory()
     */
    public String toString() {
        return _category;
    }

    /**
     * Sets the description of this category
     *
     * @param desc The new description of the category
     */
    public void setDescription(String desc) {
        _description = desc;
        if (_metadata == null) {
            _metadata = new HashMap(5);
        }
        _metadata.put(BlojsomConstants.DESCRIPTION_KEY, _description);
    }

    /**
     * Retrieves the description of this category
     *
     * @return The description of the category
     */
    public String getDescription() {
        return _description;
    }

    /**
     * Sets the name of this category
     *
     * @param name The new name of the category
     */
    public void setName(String name) {
        _name = name;
        if (_metadata == null) {
            _metadata = new HashMap(5);
        }
        _metadata.put(BlojsomConstants.NAME_KEY, _name);
    }

    /**
     * Retrieves the name of this category
     *
     * @return The name of the category
     */
    public String getName() {
        return _name;
    }

    /**
     * Retrieves the HTML escaped name of this category
     *
     * @return The name of the category
     */
    public String getEscapedName() {
        return BlojsomUtils.escapeString(_name);
    }

    /**
     * Set the meta-data associated with this category
     *
     * @param metadata The map to be associated with the category as meta-data
     */
    public void setMetaData(Map metadata) {
        _metadata = metadata;
    }

    /**
     * Sets the meta-data associated with this category. This method will also try to set the
     * description and name properties from the meta-data by looking for the
     * DESCRIPTION_KEY and NAME_KEY, respectively
     *
     * @param data The properties to be associated with the category as meta-data
     */
    public void setMetaData(Properties data) {
        String s = null;
        Enumeration keys = data.keys();
        if (_metadata == null) {
            _metadata = new HashMap(5);
        }
        Object key;
        while (keys.hasMoreElements()) {
            key = keys.nextElement();
            _metadata.put(key, data.get(key));
        }

        s = (String) _metadata.get(BlojsomConstants.DESCRIPTION_KEY);
        if ((s != null) && (!"".equals(s))) {
            _description = s;
        }

        s = (String) _metadata.get(BlojsomConstants.NAME_KEY);
        if ((s != null) && (!"".equals(s))) {
            _name = s;
        }
    }

    /**
     * Retrieves the meta-data associated with this category
     *
     * @return The properties associated with the category as meta-data, or null if no metadata exists
     */
    public Map getMetaData() {
        if (_metadata == null) {
            return new HashMap();
        }

        return _metadata;
    }

    /**
     * Set any attributes of the blog category using data from the map.
     *
     * @param attributeMap Attributes
     * @since blojsom 1.9.1
     */
    public void setAttributes(Map attributeMap) {
    }

    /**
     * Load a blog category.
     *
     * @param blog {@link Blog}
     * @throws BlojsomException If there is an error loading the category
     * @since blojsom 1.9.1
     * @deprecated
     */
    public abstract void load(Blog blog) throws BlojsomException;

    /**
     * Save the blog category.
     *
     * @param blog {@link Blog}
     * @throws BlojsomException If there is an error saving the category
     * @since blojsom 1.9.1
     * @deprecated
     */
    public abstract void save(Blog blog) throws BlojsomException;

    /**
     * Delete the blog category.
     *
     * @param blog {@link Blog}
     * @throws BlojsomException If there is an error deleting the category
     * @since blojsom 1.9.1
     * @deprecated
     */
    public abstract void delete(Blog blog) throws BlojsomException;

    /**
     * Load a blog category.
     *
     * @param blogUser {@link BlogUser}
     * @throws BlojsomException If there is an error loading the category
     * @since blojsom 2.22
     */
    public abstract void load(BlogUser blogUser) throws BlojsomException;

    /**
     * Save the blog category.
     *
     * @param blogUser {@link BlogUser}
     * @throws BlojsomException If there is an error saving the category
     * @since blojsom 2.22
     */
    public abstract void save(BlogUser blogUser) throws BlojsomException;

    /**
     * Delete the blog category.
     *
     * @param blogUser {@link BlogUser}
     * @throws BlojsomException If there is an error deleting the category
     * @since blojsom 2.22
     */
    public abstract void delete(BlogUser blogUser) throws BlojsomException;

    /**
     * Count the number of blog entries in this category. If <code>recursive</code> is true, the method
     * will also count entries in all sub-categories.
     *
     * @param blog      {@link Blog}
     * @param recursive Set to <code>true</code> if entries in sub-categories should be counted
     * @return Number of blog entries in this category
     * @since blojsom 2.22
     */
    public abstract int countBlogEntries(Blog blog, boolean recursive);

    /**
     * Return a list of sub-categories under the current category
     *
     * @return {@link List} of sub-categories as {@link BlogCategory} objects
     * @since blojsom 2.22
     */
    public List getSubcategories() {
        return _subcategories;
    }

    /**
     * Set the sub-categories for this category
     *
     * @param subcategories {@link List} of {@link BlogCategory} objects
     * @since blojsom 2.22
     */
    public void setSubcategories(List subcategories) {
        _subcategories = subcategories;
    }


    /**
     * Returns the parent category of this category. Defaults to "/"
     *
     * @return {@link BlogCategory} containing the parent category
     * @since  blojsom 2.25
     */
    public BlogCategory getParentCategory() {
        return _parentCategory;
    }


    /**
     * Sets the parent category of this category
     *
     * @param cateogory {@link BlogCategory} that represents the parent category
     * @since blojsom 2.25
     */
    public void setParentCategory(BlogCategory cateogory) {
        _parentCategory = cateogory;
    }

    /**
     * Retrieve the {@link BlogEntry} associated with this category
     *
     * @return {@link BlogEntry}
     * @since blojsom 2.23
     */
    public BlogEntry getBlogEntry() {
        return _blogEntry;
    }

    /**
     * Set the {@link BlogEntry} associated with this category
     *
     * @param blogEntry {@link BlogEntry}
     * @since blojsom 2.23
     */
    public void setBlogEntry(BlogEntry blogEntry) {
        _blogEntry = blogEntry;
    }
}
