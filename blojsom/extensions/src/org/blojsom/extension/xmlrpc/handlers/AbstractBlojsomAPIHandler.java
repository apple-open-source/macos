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
package org.blojsom.extension.xmlrpc.handlers;

import org.blojsom.BlojsomException;
import org.blojsom.authorization.AuthorizationProvider;
import org.blojsom.blog.Blog;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.extension.xmlrpc.BlojsomXMLRPCConstants;
import org.blojsom.fetcher.BlojsomFetcher;
import org.blojsom.util.BlojsomConstants;
import org.blojsom.util.BlojsomMetaDataConstants;
import org.blojsom.util.BlojsomUtils;

import java.io.File;


/**
 * Abstract blojsom API handler
 *
 * @author Mark Lussier
 * @version $Id: AbstractBlojsomAPIHandler.java,v 1.2 2004/08/27 00:49:41 whitmore Exp $
 */
public abstract class AbstractBlojsomAPIHandler implements BlojsomConstants, BlojsomMetaDataConstants, BlojsomXMLRPCConstants {

    public static final int AUTHORIZATION_EXCEPTION = 0001;
    public static final String AUTHORIZATION_EXCEPTION_MSG = "Invalid Username and/or Password";

    public static final int UNKNOWN_EXCEPTION = 1000;
    public static final String UNKNOWN_EXCEPTION_MSG = "An error occured processing your request";

    public static final int UNSUPPORTED_EXCEPTION = 1001;
    public static final String UNSUPPORTED_EXCEPTION_MSG = "Unsupported method - blojsom does not support this blogger concept";

    public static final int INVALID_POSTID = 2000;
    public static final String INVALID_POSTID_MSG = "The entry postid you submitted is invalid";

    public static final int NOBLOGS_EXCEPTION = 3000;
    public static final String NOBLOGS_EXCEPTION_MSG = "There are no categories defined for this blojsom";

    protected Blog _blog;
    protected BlogUser _blogUser;
    protected BlojsomFetcher _fetcher;
    protected BlojsomConfiguration _configuration;
    protected String _blogEntryExtension;
    protected AuthorizationProvider _authorizationProvider;

    /**
     * Attach a blog instance to the API Handler so that it can interact with the blog
     *
     * @param blogUser an instance of BlogUser
     * @throws BlojsomException If there is an error setting the blog user instance or properties for the handler
     * @see org.blojsom.blog.BlogUser
     */
    public abstract void setBlogUser(BlogUser blogUser) throws BlojsomException;

    public void setAuthorizationProvider(AuthorizationProvider authorizationProvider) {
        _authorizationProvider = authorizationProvider;
    }

    /**
     * Gets the name of API Handler. Used to bind to XML-RPC
     *
     * @return The API Name (ie: blogger)
     */
    public abstract String getName();

    /**
     * Set the {@link BlojsomFetcher} instance that will be used to fetch categories and entries
     *
     * @param fetcher {@link BlojsomFetcher} instance
     * @throws BlojsomException If there is an error in setting the fetcher
     */
    public void setFetcher(BlojsomFetcher fetcher) throws BlojsomException {
        _fetcher = fetcher;
    }


    /**
     * Set the {@link BlojsomConfiguration} instance that will be used to configure the handlers
     *
     * @param configuration {@link BlojsomConfiguration} instance
     * @throws BlojsomException If there is an error in setting the fetcher
     */
    public void setConfiguration(BlojsomConfiguration configuration) throws BlojsomException {
        _configuration = configuration;
    }

    /**
     * Return a filename appropriate for the blog entry content
     *
     * @param content Blog entry content
     * @return Filename for the new blog entry
     */
    protected String getBlogEntryFilename(String content) {
        String hashable = content;

        if (content.length() > MAX_HASHABLE_LENGTH) {
            hashable = hashable.substring(0, MAX_HASHABLE_LENGTH);
        }

        String baseFilename = BlojsomUtils.digestString(hashable).toUpperCase();
        String filename = baseFilename + _blogEntryExtension;
        return filename;
    }

    /**
     * Get the blog category. If the category exists, return the
     * appropriate directory, otherwise return the "root" of this blog.
     *
     * @param categoryName Category name
     * @return A directory into which a blog entry can be placed
     * @since blojsom 1.9
     */
    protected File getBlogCategoryDirectory(String categoryName) {
        File blogCategory = new File(_blog.getBlogHome() + BlojsomUtils.removeInitialSlash(categoryName));
        if (blogCategory.exists() && blogCategory.isDirectory()) {
            return blogCategory;
        } else {
            return new File(_blog.getBlogHome() + "/");
        }
    }
}

