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
package org.blojsom.fetcher;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.BlojsomException;
import org.blojsom.blog.*;
import org.blojsom.util.BlojsomConstants;
import org.blojsom.util.BlojsomUtils;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.File;
import java.util.*;

/**
 * StandardFetcher
 *
 * @author David Czarnecki
 * @version $Id: StandardFetcher.java,v 1.3 2005/01/29 02:39:22 johnan Exp $
 * @since blojsom 1.8
 */
public class StandardFetcher implements BlojsomFetcher, BlojsomConstants {

    protected Log _logger = LogFactory.getLog(StandardFetcher.class);

    protected static final String DEPTH_PARAM = "depth";

    protected static final String STANDARD_FETCHER_CATEGORY = "STANDARD_FETCHER_CATEGORY";
    protected static final String STANDARD_FETCHER_DEPTH = "STANDARD_FETCHER_DEPTH";

    /**
     * Default constructor
     */
    public StandardFetcher() {
    }

    /**
     * Initialize this fetcher. This method only called when the fetcher is instantiated.
     *
     * @param servletConfig        Servlet config object for the plugin to retrieve any initialization parameters
     * @param blojsomConfiguration blojsom configuration information
     * @throws BlojsomFetcherException If there is an error initializing the fetcher
     */
    public void init(ServletConfig servletConfig, BlojsomConfiguration blojsomConfiguration) throws BlojsomFetcherException {
        _logger.debug("Initialized standard fetcher");
    }

    /**
     * Return a new blog entry instance. This methods returns an instance of {@link FileBackedBlogEntry}.
     *
     * @return Blog entry instance
     * @since blojsom 1.9
     */
    public BlogEntry newBlogEntry() {
        return new FileBackedBlogEntry();
    }

    /**
     * Return a new blog category instance
     *
     * @return Blog category instance
     * @since blojsom 1.9.1
     */
    public BlogCategory newBlogCategory() {
        return new FileBackedBlogCategory();
    }

    /**
     * Retrieve a permalink entry from the entries for a given category
     *
     * @param blogUser          Blog information
     * @param requestedCategory Requested category
     * @param permalink         Permalink entry requested
     * @return Blog entry array containing the single requested permalink entry,
     *         or <code>BlogEntry[0]</code> if the permalink entry was not found
     */
    protected BlogEntry[] getPermalinkEntry(BlogUser blogUser, BlogCategory requestedCategory, String permalink) {
        Blog blog = blogUser.getBlog();
        String category = BlojsomUtils.removeInitialSlash(requestedCategory.getCategory());
        permalink = BlojsomUtils.urlDecode(permalink);
        if (!category.endsWith("/")) {
            category += "/";
        }

        String permalinkEntry = blog.getBlogHome() + category + permalink;
        File blogFile = new File(permalinkEntry);

        if (!blogFile.exists()) {
            return new BlogEntry[0];
        } else {
            BlogEntry[] entryArray = new BlogEntry[1];
            BlogEntry blogEntry = newBlogEntry();
            BlogCategory blogCategory;

            if ("/".equals(category)) {
                blogCategory = newBlogCategory();
                blogCategory.setCategory(category);
                blogCategory.setCategoryURL(blog.getBlogURL());

                try {
                    blogCategory.load(blog);
                } catch (BlojsomException e) {
                    _logger.error(e);
                }
            } else {
                blogCategory = newBlogCategory();
                blogCategory.setCategory(category);
                blogCategory.setCategoryURL(blog.getBlogURL() + category);

                try {
                    blogCategory.load(blog);
                } catch (BlojsomException e) {
                    _logger.error(e);
                }
            }

            Map entryAttributes = new HashMap();
            entryAttributes.put(FileBackedBlogEntry.SOURCE_ATTRIBUTE, blogFile);
            blogEntry.setAttributes(entryAttributes);
            blogEntry.setCategory(category);

            if ("/".equals(category)) {
                blogEntry.setLink(blog.getBlogURL() + '?' + PERMALINK_PARAM + '=' + BlojsomUtils.urlEncode(blogFile.getName()));
            } else {
                blogEntry.setLink(blog.getBlogURL() + BlojsomUtils.urlEncodeForLink(category.substring(0, category.length() - 1)) + "/?" + PERMALINK_PARAM + '=' + BlojsomUtils.urlEncode(blogFile.getName()));
            }

            blogEntry.setBlogCategory(blogCategory);

            try {
                blogEntry.load(blogUser);
            } catch (BlojsomException e) {
                return new BlogEntry[0];
            }

            entryArray[0] = blogEntry;

            return entryArray;
        }
    }

    /**
     * Retrieve all of the entries for a requested category
     *
     * @param requestedCategory Requested category
     * @param maxBlogEntries    Maximum number of blog entries to retrieve from a blog category
     * @return Blog entry array containing the list of blog entries for the requested category,
     *         or <code>BlogEntry[0]</code> if there are no entries for the category
     */
    protected BlogEntry[] getEntriesForCategory(BlogUser user, BlogCategory requestedCategory, int maxBlogEntries) {
        BlogEntry[] entryArray;
        Blog blog = user.getBlog();
        String category = BlojsomUtils.removeInitialSlash(requestedCategory.getCategory());
        if (!category.endsWith("/")) {
            category += "/";
        }

        File blogCategory = new File(blog.getBlogHome() + category);
        File[] entries = blogCategory.listFiles(BlojsomUtils.getRegularExpressionFilter(blog.getBlogFileExtensions()));
        if (entries == null) {
            _logger.debug("No blog entries in blog directory: " + blogCategory);

            return new BlogEntry[0];
        } else {
            Arrays.sort(entries, BlojsomUtils.FILE_TIME_COMPARATOR);
            BlogEntry blogEntry;
            int entryCounter;

            if (maxBlogEntries == -1) {
                entryCounter = entries.length;
            } else {
                entryCounter = (maxBlogEntries > entries.length) ? entries.length : maxBlogEntries;
            }

            entryArray = new BlogEntry[entryCounter];

            for (int i = 0; i < entryCounter; i++) {
                File entry = entries[i];
                blogEntry = newBlogEntry();
                BlogCategory blogCategoryForEntry;

                if ("/".equals(category)) {
                    blogCategoryForEntry = newBlogCategory();
                    blogCategoryForEntry.setCategory(category);
                    blogCategoryForEntry.setCategoryURL(blog.getBlogURL());

                    try {
                        blogCategoryForEntry.load(blog);
                    } catch (BlojsomException e) {
                        _logger.error(e);
                    }
                } else {
                    blogCategoryForEntry = newBlogCategory();
                    blogCategoryForEntry.setCategory(category);
                    blogCategoryForEntry.setCategoryURL(blog.getBlogURL() + category);

                    try {
                        blogCategoryForEntry.load(blog);
                    } catch (BlojsomException e) {
                        _logger.error(e);
                    }
                }

                Map entryAttributes = new HashMap();
                entryAttributes.put(FileBackedBlogEntry.SOURCE_ATTRIBUTE, entry);
                blogEntry.setAttributes(entryAttributes);
                blogEntry.setCategory(category);

                if ("/".equals(category)) {
                    blogEntry.setLink(blog.getBlogURL() + '?' + PERMALINK_PARAM + '=' + BlojsomUtils.urlEncode(entry.getName()));
                } else {
                    blogEntry.setLink(blog.getBlogURL() + BlojsomUtils.urlEncodeForLink(category.substring(0, category.length() - 1)) + "/?" + PERMALINK_PARAM + '=' + BlojsomUtils.urlEncode(entry.getName()));
                }

                blogEntry.setBlogCategory(blogCategoryForEntry);

                try {
                    blogEntry.load(user);
                } catch (BlojsomException e) {
                    _logger.error(e);
                }

                entryArray[i] = blogEntry;
            }

            return entryArray;
        }
    }

    /**
     * Retrive entries for the categories, using the values set for
     * the default category mapping and the configured number of blog entries to retrieve
     * from each category
     *
     * @param flavor             Requested flavor
     * @param maxBlogEntries     Maximum number of entries to retrieve per category
     * @param blogDirectoryDepth Depth to which the fetcher should stop looking for categories
     * @return Blog entry array containing the list of blog entries for the categories
     *         or <code>BlogEntry[0]</code> if there are no entries
     */
    protected BlogEntry[] getEntriesAllCategories(BlogUser user, String flavor, int maxBlogEntries, int blogDirectoryDepth) {
        Blog blog = user.getBlog();
        String flavorMappingKey = flavor + '.' + BLOG_DEFAULT_CATEGORY_MAPPING_IP;
        String categoryMappingForFlavor = (String) blog.getBlogProperty(flavorMappingKey);
        String[] categoryMappingsForFlavor = null;
        if (!BlojsomUtils.checkNullOrBlank(categoryMappingForFlavor)) {
            _logger.debug("Using category mappings for flavor: " + flavor);
            categoryMappingsForFlavor = BlojsomUtils.parseCommaList(categoryMappingForFlavor);
        } else if (blog.getBlogDefaultCategoryMappings() != null && blog.getBlogDefaultCategoryMappings().length > 0) {
            _logger.debug("Using default category mapping");
            categoryMappingsForFlavor = blog.getBlogDefaultCategoryMappings();
        } else {
            categoryMappingsForFlavor = null;
        }

        return getEntriesAllCategories(user, categoryMappingsForFlavor, maxBlogEntries, blogDirectoryDepth);
    }

    /**
     * Retrieve entries for all the categories in the blog. This method will the parameter
     * <code>maxBlogEntries</code> to limit the entries it retrieves from each of the categories.
     * Entries from the categories are sorted based on file time.
     *
     * @param categoryFilter     If <code>null</code>, a list of all the categories is retrieved, otherwise only
     *                           the categories in the list will be used to search for entries
     * @param maxBlogEntries     Maximum number of blog entries to retrieve from each category
     * @param blogDirectoryDepth Depth to which the fetcher should stop looking for categories
     * @return Blog entry array containing the list of blog entries for the categories
     *         or <code>BlogEntry[0]</code> if there are no entries
     */
    protected BlogEntry[] getEntriesAllCategories(BlogUser user, String[] categoryFilter, int maxBlogEntries, int blogDirectoryDepth) {
        BlogCategory[] blogCategories = null;
        Blog blog = user.getBlog();

        if (categoryFilter == null) {
            blogCategories = getBlogCategories(blog, blogDirectoryDepth);
        } else {
            blogCategories = new BlogCategory[categoryFilter.length];
            for (int i = 0; i < categoryFilter.length; i++) {
                String category = BlojsomUtils.removeInitialSlash(categoryFilter[i]);
                BlogCategory blogCategory = newBlogCategory();
                blogCategory.setCategoryURL(blog.getBlogURL() + category);
                if ("".equals(category)) {
                    blogCategory.setCategory("/");
                } else {
                    blogCategory.setCategory(category);
                }
                blogCategories[i] = blogCategory;
            }
        }

        if (blogCategories == null) {
            return new BlogEntry[0];
        } else {
            ArrayList blogEntries = new ArrayList();
            for (int i = 0; i < blogCategories.length; i++) {
                BlogCategory blogCategory = blogCategories[i];
                BlogEntry[] entriesForCategory = getEntriesForCategory(user, blogCategory, -1);
                if (entriesForCategory != null) {
                    Arrays.sort(entriesForCategory, BlojsomUtils.FILE_TIME_COMPARATOR);
                    if (maxBlogEntries != -1) {
                        int entryCounter = (maxBlogEntries >= entriesForCategory.length) ? entriesForCategory.length : maxBlogEntries;
                        for (int j = 0; j < entryCounter; j++) {
                            BlogEntry blogEntry = entriesForCategory[j];
                            blogEntries.add(blogEntry);
                        }
                    } else {
                        for (int j = 0; j < entriesForCategory.length; j++) {
                            BlogEntry blogEntry = entriesForCategory[j];
                            blogEntries.add(blogEntry);
                        }
                    }
                }
            }

            BlogEntry[] entries = (BlogEntry[]) blogEntries.toArray(new BlogEntry[blogEntries.size()]);
            Arrays.sort(entries, BlojsomUtils.FILE_TIME_COMPARATOR);
            return entries;
        }
    }

    /**
     * Determine the blog category based on the request
     *
     * @param httpServletRequest Request
     * @return {@link BlogCategory} of the requested category
     */
    protected BlogCategory getBlogCategory(BlogUser user, HttpServletRequest httpServletRequest) {
        Blog blog = user.getBlog();

        // Determine the user requested category
        String requestedCategory = httpServletRequest.getPathInfo();
        String userFromPath = BlojsomUtils.getUserFromPath(httpServletRequest.getPathInfo());
        if (userFromPath == null) {
            requestedCategory = httpServletRequest.getPathInfo();
        } else {
            if (userFromPath.equals(user.getId())) {
                requestedCategory = BlojsomUtils.getCategoryFromPath(httpServletRequest.getPathInfo());
            } else {
                requestedCategory = httpServletRequest.getPathInfo();
            }
        }

        requestedCategory = BlojsomUtils.normalize(requestedCategory);
        _logger.debug("blojsom path info: " + requestedCategory);

        String categoryParameter = httpServletRequest.getParameter(CATEGORY_PARAM);
        if (!(categoryParameter == null) && !("".equals(categoryParameter))) {
            categoryParameter = BlojsomUtils.normalize(categoryParameter);
            _logger.debug("category parameter override: " + categoryParameter);
            requestedCategory = categoryParameter;
        }

        if (requestedCategory == null) {
            requestedCategory = "/";
        } else if (!requestedCategory.endsWith("/")) {
            requestedCategory += "/";
        }

        requestedCategory = BlojsomUtils.urlDecode(requestedCategory);
        _logger.debug("User requested category: " + requestedCategory);
        BlogCategory category = newBlogCategory();
        category.setCategory(requestedCategory);
        category.setCategoryURL(blog.getBlogURL() + BlojsomUtils.removeInitialSlash(requestedCategory));

        // We might also want to pass the flavor so that we can also have flavor-based category meta-data
        try {
            category.load(blog);
        } catch (BlojsomException e) {
            _logger.error(e);
        }

        return category;
    }

    /**
     * Fetch a set of {@link BlogEntry} objects.
     *
     * @param httpServletRequest  Request
     * @param httpServletResponse Response
     * @param user                {@link BlogUser} instance
     * @param flavor              Flavor
     * @param context             Context
     * @return Blog entries retrieved for the particular request
     * @throws BlojsomFetcherException If there is an error retrieving the blog entries for the request
     */
    public BlogEntry[] fetchEntries(HttpServletRequest httpServletRequest,
                                    HttpServletResponse httpServletResponse,
                                    BlogUser user,
                                    String flavor,
                                    Map context) throws BlojsomFetcherException {
        BlogCategory category = (BlogCategory) context.get(STANDARD_FETCHER_CATEGORY);
        context.remove(STANDARD_FETCHER_CATEGORY);
        int blogDirectoryDepth = ((Integer) context.get(STANDARD_FETCHER_DEPTH)).intValue();
        context.remove(STANDARD_FETCHER_DEPTH);
        Blog blog = user.getBlog();

        // Determine if a permalink has been requested
        String permalink = httpServletRequest.getParameter(PERMALINK_PARAM);
        if (permalink != null) {
            permalink = BlojsomUtils.getFilenameForPermalink(permalink, blog.getBlogFileExtensions());
            permalink = BlojsomUtils.urlDecode(permalink);
            if (permalink == null) {
                _logger.error("Permalink request for invalid permalink: " + httpServletRequest.getParameter(PERMALINK_PARAM));
            } else {
                _logger.debug("Permalink request for: " + permalink);
            }
        }

        // Check for a permalink entry request
        if (permalink != null) {
            context.put(BLOJSOM_PERMALINK, permalink);
            BlogEntry[] permalinkEntry = getPermalinkEntry(user, category, permalink);

            if (blog.getLinearNavigationEnabled().booleanValue()) {
                BlogEntry[] allEntries = getEntriesAllCategories(user, flavor, -1, blogDirectoryDepth);

                if (permalinkEntry.length > 0 && allEntries.length > 0) {
                    String permalinkId = permalinkEntry[0].getId();
                    for (int i = 0; i < allEntries.length; i++) {
                        BlogEntry blogEntry = allEntries[i];
                        String blogEntryId = blogEntry.getId();
                        if (blogEntryId != null && blogEntryId.equals(permalinkId)) {
                            if ((i - 1) >= 0) {
                                context.put(BLOJSOM_PERMALINK_NEXT_ENTRY, allEntries[i - 1]);
                            } else {
                                context.put(BLOJSOM_PERMALINK_NEXT_ENTRY, null);
                            }

                            if ((i + 1) < allEntries.length) {
                                context.put(BLOJSOM_PERMALINK_PREVIOUS_ENTRY, allEntries[i + 1]);
                            } else {
                                context.put(BLOJSOM_PERMALINK_PREVIOUS_ENTRY, null);
                            }

                            break;
                        }
                    }
                }
            }

            return permalinkEntry;
        } else {
            if ("/".equals(category.getCategory())) {
                return getEntriesAllCategories(user, flavor, -1, blogDirectoryDepth);
            } else {
				String categoryString = category.getCategory();
				ArrayList blogEntries = new ArrayList();
				BlogCategory[] categoryHierarchy = getBlogCategoryHierarchy(blog, category, blog.getBlogDepth());
				for (int i = 0; i < categoryHierarchy.length; i++) {
					String currentCategoryString = categoryHierarchy[i].getCategory();
					if (currentCategoryString.startsWith(categoryString)) {
						BlogEntry[] currentCategoryEntries = getEntriesForCategory(user, categoryHierarchy[i], -1);
						for (int j = 0; j < currentCategoryEntries.length; j++) {
							blogEntries.add(currentCategoryEntries[j]);
						}
					}
				}
				BlogEntry[] currentCategoryEntries = getEntriesForCategory(user, category, -1);
				for (int j = 0; j < currentCategoryEntries.length; j++) {
					blogEntries.add(currentCategoryEntries[j]);
				}
				BlogEntry[] entries = (BlogEntry[]) blogEntries.toArray(new BlogEntry[blogEntries.size()]);
				Arrays.sort(entries, BlojsomUtils.FILE_TIME_COMPARATOR);
				return entries;
            }
        }
    }

    /**
     * Fetch a set of {@link BlogEntry} objects. This method is intended to be used for other
     * components such as the XML-RPC handlers that cannot generate servlet request and
     * response objects, but still need to be able to fetch entries. Implementations of this
     * method <b>must</b> be explicit about the exact parameter names and types that are
     * expected to return an appropriate set of {@link BlogEntry} objects. The following
     * table describes the parameters accepted by this method and their return value. The value
     * for <code>fetchParameters</code> may represent the keys and data types that should be
     * present in the <code>fetchParameters</code> map to return the proper data.
     * <p />
     * <table border="1">
     * <th><code>fetchParameters</code> value</th> <th>Return value</th>
     * <tr>
     * <td>"FETCHER_CATEGORY" (<code>BlogCategory</code>) and "FETCHER_PERMALINK" (<code>String</code>)</td> <td>return a single <code>BlogEntry</code> for the requested permalink</td>
     * </tr>
     * <tr>
     * <td>"FETCHER_CATEGORY" (<code>BlogCategory</code>) and "FETCHER_NUM_POSTS_INTEGER" (<code>Integer</code>)</td> <td>return entries for the requested category up to the value indicated by the number of entries</td>
     * </tr>
     * <tr>
     * <td>"FETCHER_FLAVOR" (<code>String</code>) and "FETCHER_NUM_POSTS_INTEGER" (<code>Integer</code>)</td> <td>return all entries for the default category ("/") for the requested flavor up to the value indicated by the number of entries</td>
     * </tr>
     * </table>
     *
     * @param fetchParameters Parameters which will be used to retrieve blog entries
     * @param user            {@link BlogUser} instance
     * @return Blog entries retrieved for the particular request
     * @throws BlojsomFetcherException If there is an error retrieving the blog entries for the request
     */
    public BlogEntry[] fetchEntries(Map fetchParameters, BlogUser user) throws BlojsomFetcherException {
        Blog blog = user.getBlog();
        if (fetchParameters.containsKey(FETCHER_CATEGORY) && fetchParameters.containsKey(FETCHER_PERMALINK)) {
            return getPermalinkEntry(user, (BlogCategory) fetchParameters.get(FETCHER_CATEGORY), (String) fetchParameters.get(FETCHER_PERMALINK));
        } else if (fetchParameters.containsKey(FETCHER_FLAVOR) && fetchParameters.containsKey(FETCHER_NUM_POSTS_INTEGER)) {
            return getEntriesAllCategories(user, (String) fetchParameters.get(FETCHER_FLAVOR), ((Integer) fetchParameters.get(FETCHER_NUM_POSTS_INTEGER)).intValue(), blog.getBlogDepth());
        } else if (fetchParameters.containsKey(FETCHER_CATEGORY) && fetchParameters.containsKey(FETCHER_NUM_POSTS_INTEGER)) {
            return getEntriesForCategory(user, (BlogCategory) fetchParameters.get(FETCHER_CATEGORY), ((Integer) fetchParameters.get(FETCHER_NUM_POSTS_INTEGER)).intValue());
        }

        return new BlogEntry[0];
    }

    /**
     * Build a list of blog categories recursively
     *
     * @param blogDepth          Depth at which the current iteration is running
     * @param blogDirectoryDepth Depth to which the fetcher should stop looking for categories
     * @param blogDirectory      Directory in which the current iteration is running
     * @param categoryList       Dynamic list of categories that gets added to as it explores directories
     */
    protected void recursiveCategoryBuilder(Blog blog, int blogDepth, int blogDirectoryDepth, String blogDirectory, ArrayList categoryList) {
        blogDepth++;
        if (blogDirectoryDepth != INFINITE_BLOG_DEPTH) {
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

        BlogCategory blogCategory = newBlogCategory();
        blogCategory.setCategory(categoryKey);
        blogCategory.setCategoryURL(blog.getBlogURL() + BlojsomUtils.removeInitialSlash(categoryKey));
        try {
            blogCategory.load(blog);
        } catch (BlojsomException e) {
            _logger.error(e);
        }
        categoryList.add(blogCategory);

        if (directories == null) {
            return;
        } else {
            for (int i = 0; i < directories.length; i++) {
                File directory = directories[i];
                recursiveCategoryBuilder(blog, blogDepth, blogDirectoryDepth, directory.toString(), categoryList);
            }
        }
    }

    /**
     * Return a list of categories for the blog that are appropriate in a hyperlink
     *
     * @param blogDirectoryDepth Depth to which the fetcher should stop looking for categories
     * @return List of BlogCategory objects
     */
    protected BlogCategory[] getBlogCategories(Blog blog, int blogDirectoryDepth) {
        ArrayList categoryList = new ArrayList();
        recursiveCategoryBuilder(blog, -1, blogDirectoryDepth, blog.getBlogHome(), categoryList);
        return (BlogCategory[]) (categoryList.toArray(new BlogCategory[categoryList.size()]));
    }

    /**
     * Return a list of categories up the category hierarchy from the current category. If
     * the "/" category is requested, <code>null</code> is returned. Up the hierarchy, only
     * the parent categories are returned. Down the hierarchy from the current category, all
     * children are returned while obeying the <code>blog-directory-depth</code> parameter.
     *
     * @param currentCategory    Current category in the blog category hierarchy
     * @param blogDirectoryDepth Depth to which the fetcher should stop looking for categories
     * @return List of blog categories or <code>null</code> if "/" category is requested or there
     *         are no sub-categories
     */
    protected BlogCategory[] getBlogCategoryHierarchy(Blog blog, BlogCategory currentCategory, int blogDirectoryDepth) {
        if ("/".equals(currentCategory.getCategory())) {
            return null;
        }

        StringTokenizer slashTokenizer = new StringTokenizer(currentCategory.getCategory(), "/");
        String previousCategoryName = "/";
        ArrayList categoryList = new ArrayList();
        ArrayList sanitizedCategoryList = new ArrayList();
        BlogCategory category;
        BlogCategory updatedBackedCategory;

        while (slashTokenizer.hasMoreTokens()) {
            previousCategoryName += slashTokenizer.nextToken() + '/';
            if (!previousCategoryName.equals(currentCategory.getCategory())) {
                updatedBackedCategory = newBlogCategory();
                updatedBackedCategory.setCategory(previousCategoryName);
                updatedBackedCategory.setCategoryURL(blog.getBlogURL() + BlojsomUtils.removeInitialSlash(previousCategoryName));
                try {
                    updatedBackedCategory.load(blog);
                } catch (BlojsomException e) {
                    _logger.error(e);
                }
                categoryList.add(updatedBackedCategory);
            }
        }

        recursiveCategoryBuilder(blog, -1, blogDirectoryDepth, blog.getBlogHome() + BlojsomUtils.removeInitialSlash(currentCategory.getCategory()), categoryList);
        for (int i = 0; i < categoryList.size(); i++) {
            category = (BlogCategory) categoryList.get(i);
            if (!category.getCategory().equals(currentCategory.getCategory())) {
                _logger.debug(category.getCategory());
                sanitizedCategoryList.add(category);
            }
        }

        BlogCategory rootCategory = newBlogCategory();
        rootCategory.setCategory("/");
        rootCategory.setCategoryURL(blog.getBlogURL());
        try {
            rootCategory.load(blog);
        } catch (BlojsomException e) {
            _logger.error(e);
        }
        sanitizedCategoryList.add(0, rootCategory);

        if (sanitizedCategoryList.size() > 0) {
            return (BlogCategory[]) sanitizedCategoryList.toArray(new BlogCategory[sanitizedCategoryList.size()]);
        } else {
            return null;
        }
    }

    /**
     * Fetch a set of {@link BlogCategory} objects
     *
     * @param httpServletRequest  Request
     * @param httpServletResponse Response
     * @param user                {@link BlogUser} instance
     * @param flavor              Flavor
     * @param context             Context
     * @return Blog categories retrieved for the particular request
     * @throws BlojsomFetcherException If there is an error retrieving the blog categories for the request
     */
    public BlogCategory[] fetchCategories(HttpServletRequest httpServletRequest,
                                          HttpServletResponse httpServletResponse,
                                          BlogUser user,
                                          String flavor,
                                          Map context) throws BlojsomFetcherException {
        BlogCategory[] categories;
        Blog blog = user.getBlog();
        BlogCategory category = getBlogCategory(user, httpServletRequest);

        context.put(STANDARD_FETCHER_CATEGORY, category);
        context.put(BLOJSOM_REQUESTED_CATEGORY, category);

        String depthParam;
        int blogDirectoryDepth = blog.getBlogDepth();
        depthParam = httpServletRequest.getParameter(DEPTH_PARAM);
        if (depthParam != null || !"".equals(depthParam)) {
            try {
                blogDirectoryDepth = Integer.parseInt(depthParam);
            } catch (NumberFormatException e) {
                blogDirectoryDepth = blog.getBlogDepth();
            }
        }

        context.put(STANDARD_FETCHER_DEPTH, new Integer(blogDirectoryDepth));
        BlogCategory[] allCategories = getBlogCategories(blog, blogDirectoryDepth);
        context.put(BLOJSOM_ALL_CATEGORIES, allCategories);

        if ("/".equals(category.getCategory())) {
            categories = allCategories;
        } else {
            categories = getBlogCategoryHierarchy(blog, category, blogDirectoryDepth);
        }

        return categories;
    }

    /**
     * Fetch a set of {@link BlogCategory} objects. This method is intended to be used for other
     * components such as the XML-RPC handlers that cannot generate servlet request and
     * response objects, but still need to be able to fetch categories. Implementations of this
     * method <b>must</b> be explicit about the exact parameter names and types that are
     * expected to return an appropriate set of {@link BlogCategory} objects. The following
     * table describes the parameters accepted by this method and their return value. The value
     * for <code>fetchParameters</code> may represent the keys and data types that should be
     * present in the <code>fetchParameters</code> map to return the proper data.
     * <p />
     * <table border="1">
     * <th><code>fetchParameters</code> value</th> <th>Return value</th>
     * <tr>
     * <td><code>null</code></td> <td>return all categories</td>
     * </tr>
     * <tr>
     * <td>"FETCHER_CATEGORY" (<code>BlogCategory</code>)</td> <td>Up the hierarchy, only
     * the parent categories are returned. Down the hierarchy from the current category, all
     * children are returned while obeying the <code>blog-directory-depth</code> parameter.</td>
     * </tr>
     * </table>
     *
     * @param fetchParameters Parameters which will be used to retrieve blog entries
     * @param user            {@link BlogUser} instance
     * @return Blog categories retrieved for the particular request
     * @throws BlojsomFetcherException If there is an error retrieving the blog categories for the request
     */
    public BlogCategory[] fetchCategories(Map fetchParameters, BlogUser user) throws BlojsomFetcherException {
        Blog blog = user.getBlog();
        if (fetchParameters == null) {
            return getBlogCategories(blog, blog.getBlogDepth());
        } else if (fetchParameters.containsKey(FETCHER_CATEGORY)) {
            BlogCategory category = (BlogCategory) fetchParameters.get(FETCHER_CATEGORY);
            if ("/".equals(category.getCategory())) {
                return getBlogCategories(blog, blog.getBlogDepth());
            } else {
                return getBlogCategoryHierarchy(blog, category, blog.getBlogDepth());
            }
        }

        return new BlogCategory[0];
    }

    /**
     * Called when {@link org.blojsom.servlet.BlojsomServlet} is taken out of service
     *
     * @throws BlojsomFetcherException If there is an error in finalizing this fetcher
     */
    public void destroy() throws BlojsomFetcherException {
    }
}
