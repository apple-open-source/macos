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
import org.blojsom.util.BlojsomProperties;
import org.blojsom.util.BlojsomConstants;
import org.blojsom.BlojsomException;
import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.FileOutputStream;
import java.util.Properties;
import java.util.List;
import java.util.ArrayList;

/**
 * FileBackedBlogCategory
 *
 * @author David Czarnecki
 * @version $Id: FileBackedBlogCategory.java,v 1.2.2.1 2005/07/21 14:11:02 johnan Exp $
 */
public class FileBackedBlogCategory extends BlogCategory {

    private Log _logger = LogFactory.getLog(FileBackedBlogCategory.class);

    protected BlogUser _blogUser;

    /**
     * Create a new FileBackedBlogCategory.
     */
    public FileBackedBlogCategory() {
        super();
    }

    /**
     * Create a new FileBackedBlogCategory.
     *
     * @param category    Category name
     * @param categoryURL Category URL
     */
    public FileBackedBlogCategory(String category, String categoryURL) {
        super(category, categoryURL);
    }

    /**
     * Load the meta data for the category
     *
     * @param blogHome             Directory where blog entries are stored
     * @param propertiesExtensions List of file extensions to use when looking for category properties
     */
    protected void loadMetaData(String blogHome, String[] propertiesExtensions) {
        File blog = new File(blogHome + BlojsomUtils.removeInitialSlash(_category));

        // Load properties file for category (if present)
        File[] categoryPropertyFiles = blog.listFiles(BlojsomUtils.getExtensionsFilter(propertiesExtensions));
        if ((categoryPropertyFiles != null) && (categoryPropertyFiles.length > 0)) {
            Properties dirProps = new BlojsomProperties();
            for (int i = 0; i < categoryPropertyFiles.length; i++) {
                try {
                    FileInputStream _fis = new FileInputStream(categoryPropertyFiles[i]);
                    dirProps.load(_fis);
                    _fis.close();
                } catch (IOException ex) {
                    _logger.warn("Failed loading properties from: " + categoryPropertyFiles[i].toString());
                    continue;
                }
            }

            setMetaData(dirProps);
        }
    }

    /**
     * Load a blog category.
     *
     * @param blog {@link Blog}
     * @throws BlojsomException If there is an error loading the category
     * @since blojsom 1.9.1
     * @deprecated
     */
    public void load(Blog blog) throws BlojsomException {
    }

    /**
     * Save the blog category.
     *
     * @param blog {@link Blog}
     * @throws BlojsomException If there is an error saving the category
     * @since blojsom 1.9.1
     * @deprecated
     */
    public void save(Blog blog) throws BlojsomException {
    }

    /**
     * Delete the blog category.
     *
     * @param blog {@link Blog}
     * @throws BlojsomException If there is an error deleting the category
     * @since blojsom 1.9.1
     * @deprecated
     */
    public void delete(Blog blog) throws BlojsomException {
    }

    /**
     * Load a blog category. Currently only loads the blog meta-data from disk.
     *
     * @param blogUser {@link BlogUser}
     * @throws BlojsomException If there is an error loading the category
     * @since blojsom 2.22
     */
    public void load(BlogUser blogUser) throws BlojsomException {
        _blogUser = blogUser;
        Blog blog = blogUser.getBlog();
        loadMetaData(blog.getBlogHome(), blog.getBlogPropertiesExtensions());
    }

    /**
     * Save the blog category.
     *
     * @param blogUser {@link BlogUser}
     * @throws BlojsomException If there is an error saving the category
     * @since blojsom 2.22
     */
    public void save(BlogUser blogUser) throws BlojsomException {
        _blogUser = blogUser;
        Blog blog = blogUser.getBlog();
        File blogCategory = new File(blog.getBlogHome() + BlojsomUtils.removeInitialSlash(_category));

        // If the category does not exist, try and create it
        if (!blogCategory.exists()) {
            if (!blogCategory.mkdirs()) {
                _logger.error("Could not create new blog category at: " + blogCategory.toString());

                return;
            }
        }

        // We know the category exists so try and save its meta-data
        String propertiesExtension = blog.getBlogPropertiesExtensions()[0];
        File categoryMetaDataFile = new File(blogCategory, "blojsom" + propertiesExtension);
        Properties categoryMetaData = BlojsomUtils.mapToProperties(_metadata, BlojsomConstants.UTF8);
        try {
            FileOutputStream fos = new FileOutputStream(categoryMetaDataFile);
            categoryMetaData.store(fos, null);
            fos.close();
        } catch (IOException e) {
            _logger.error(e);
            throw new BlojsomException("Unable to save blog category", e);
        }

        _logger.debug("Saved blog category: " + blogCategory.toString());
    }

    /**
     * Delete the blog category.
     *
     * @param blogUser {@link BlogUser}
     * @throws BlojsomException If there is an error deleting the category
     * @since blojsom 2.22
     */
    public void delete(BlogUser blogUser) throws BlojsomException {
        _blogUser = blogUser;
        Blog blog = blogUser.getBlog();
        File blogCategory = new File(blog.getBlogHome() + BlojsomUtils.removeInitialSlash(_category));
        if (blogCategory.equals(blog.getBlogHome())) {
            if (!BlojsomUtils.deleteDirectory(blogCategory, false)) {
                throw new BlojsomException("Unable to delete blog category directory: " + _category);
            }
        } else {
            if (!BlojsomUtils.deleteDirectory(blogCategory)) {
                throw new BlojsomException("Unable to delete blog category directory: " + _category);
            }
        }

        _logger.debug("Deleted blog category: " + blogCategory.toString());
    }

    /**
     * Recursively count the blog entries in a blog directory
     *
     * @param blog          {@link Blog}
     * @param recursive     Set to <code>true</code> if entries in sub-categories should be counted
     * @param rootDirectory
     * @return
     */
    protected int recursiveBlogEntriesCounter(Blog blog, boolean recursive, String rootDirectory) {
        File blogDirectory = new File(rootDirectory);
        File[] directories;
        int totalEntries = 0;

        if (blog.getBlogDirectoryFilter() == null) {
            directories = blogDirectory.listFiles(BlojsomUtils.getDirectoryFilter());
        } else {
            directories = blogDirectory.listFiles(BlojsomUtils.getDirectoryFilter(blog.getBlogDirectoryFilter()));
        }

        File[] entries = blogDirectory.listFiles(BlojsomUtils.getRegularExpressionFilter(blog.getBlogFileExtensions()));
        if (entries == null) {
            totalEntries = 0;
        } else {
            totalEntries = entries.length;
        }

        if (directories != null && recursive) {
            for (int i = 0; i < directories.length; i++) {
                File directory = directories[i];
                return totalEntries + recursiveBlogEntriesCounter(blog, recursive, directory.toString());
            }
        }

        return totalEntries;
    }

    /**
     * Count the number of blog entries in this category. If <code>recursive</code> is true, the method
     * will also count entries in all sub-categories.
     *
     * @param blog      {@link Blog}
     * @param recursive Set to <code>true</code> if entries in sub-categories should be counted
     * @return Number of blog entries in this category
     * @since blojsom 2.22
     */
    public int countBlogEntries(Blog blog, boolean recursive) {
        int totalEntries = recursiveBlogEntriesCounter(blog, recursive, blog.getBlogHome() + _category);

        return totalEntries;
    }

    /**
     * Build a list of blog categories recursively
     *
     * @param blogUser           {@link BlogUser}
     * @param blogDepth          Depth at which the current iteration is running
     * @param blogDirectoryDepth Depth to which the fetcher should stop looking for categories
     * @param blogDirectory      Directory in which the current iteration is running
     * @param categoryList       Dynamic list of categories that gets added to as it explores directories
     */
    protected void recursiveCategoryBuilder(BlogUser blogUser, int blogDepth, int blogDirectoryDepth, String blogDirectory, List categoryList) {
        Blog blog = blogUser.getBlog();
        blogDepth++;
        if (blogDirectoryDepth != BlojsomConstants.INFINITE_BLOG_DEPTH) {
            if (blogDepth == blogDirectoryDepth) {
                return;
            }
        }

        File blogDir = new File(blogDirectory);
        File[] directories;
        if (blog.getBlogDirectoryFilter() == null) {
            directories = blogDir.listFiles(BlojsomUtils.getDirectoryFilter());
        } else {
            directories = blogDir.listFiles(BlojsomUtils.getDirectoryFilter(blog.getBlogDirectoryFilter()));
        }

        String categoryKey = BlojsomUtils.getBlogCategory(blog.getBlogHome(), blogDirectory);
        if (!categoryKey.endsWith("/")) {
            categoryKey += "/";
        }

        BlogCategory blogCategory = new FileBackedBlogCategory();
        blogCategory.setParentCategory(this);        
        blogCategory.setCategory(categoryKey);
        blogCategory.setCategoryURL(blog.getBlogURL() + BlojsomUtils.removeInitialSlash(categoryKey));


        try {
            blogCategory.load(blogUser);
        } catch (BlojsomException e) {
            _logger.error(e);
        }

        if (blogDepth != 0) {
            categoryList.add(blogCategory);
        }

        if (directories == null) {
            return;
        } else {
            for (int i = 0; i < directories.length; i++) {
                File directory = directories[i];
                recursiveCategoryBuilder(blogUser, blogDepth, blogDirectoryDepth, directory.toString(), categoryList);
            }
        }
    }

    /**
     * Return a list of sub-categories under the current category
     *
     * @return {@link java.util.List} of sub-categories as {@link BlogCategory} objects
     * @since blojsom 2.22
     */
    public List getSubcategories() {
        _subcategories = new ArrayList();
        recursiveCategoryBuilder(_blogUser, -1, -1, _blogUser.getBlog().getBlogHome() + BlojsomUtils.removeInitialSlash(_category), _subcategories);

        return _subcategories;
    }
}
