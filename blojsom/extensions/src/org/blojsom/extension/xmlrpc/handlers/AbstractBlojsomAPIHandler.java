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
import org.apache.xmlrpc.XmlRpcException;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.File;
import java.util.HashMap;


/**
 * Abstract blojsom API handler
 *
 * @author Mark Lussier
 * @version $Id: AbstractBlojsomAPIHandler.java,v 1.2.2.1 2005/07/21 04:30:23 johnan Exp $
 */
public abstract class AbstractBlojsomAPIHandler implements BlojsomConstants, BlojsomMetaDataConstants, BlojsomXMLRPCConstants {

    public static final int AUTHORIZATION_EXCEPTION = 0001;
    public static final String AUTHORIZATION_EXCEPTION_MSG = "Invalid username and/or password";

    public static final int UNKNOWN_EXCEPTION = 1000;
    public static final String UNKNOWN_EXCEPTION_MSG = "An error occured processing your request";

    public static final int UNSUPPORTED_EXCEPTION = 1001;
    public static final String UNSUPPORTED_EXCEPTION_MSG = "Unsupported method";

    public static final int INVALID_POSTID = 2000;
    public static final String INVALID_POSTID_MSG = "The entry postid you submitted is invalid";

    public static final int NOBLOGS_EXCEPTION = 3000;
    public static final String NOBLOGS_EXCEPTION_MSG = "There are no categories defined";

    public static final int PERMISSION_EXCEPTION = 4000;
    public static final String PERMISSION_EXCEPTION_MSG = "User does not have permission to use this XML-RPC method";

    protected Blog _blog;
    protected BlogUser _blogUser;
    protected BlojsomFetcher _fetcher;
    protected BlojsomConfiguration _configuration;
    protected String _blogEntryExtension;
    protected AuthorizationProvider _authorizationProvider;
    protected ServletConfig _servletConfig;
    protected HttpServletRequest _httpServletRequest;
    protected HttpServletResponse _httpServletResponse;

    /**
     * Attach a blog instance to the API Handler so that it can interact with the blog
     *
     * @param blogUser an instance of BlogUser
     * @throws BlojsomException If there is an error setting the blog user instance or properties for the handler
     * @see org.blojsom.blog.BlogUser
     */
    public abstract void setBlogUser(BlogUser blogUser) throws BlojsomException;

    /**
     * Sets the {@link AuthorizationProvider} for the XML-RPC handler
     *
     * @param authorizationProvider {@link AuthorizationProvider}
     */
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
     * Set the {@link ServletConfig} instance that can be used to retrieve servlet parameters
     *
     * @param servletConfig {@link ServletConfig} instance
     */
    public void setServletConfig(ServletConfig servletConfig) {
        _servletConfig = servletConfig;
    }

    /**
     * Set the {@link HttpServletRequest} instance or the handler
     *
     * @param httpServletRequest {@link HttpServletRequest} instance
     * @since blojsom 2.23
     */
    public void setHttpServletRequest(HttpServletRequest httpServletRequest) {
        _httpServletRequest = httpServletRequest;
    }

    /**
     * Set the {@link HttpServletResponse} instance or the handler
     *
     * @param httpServletResponse {@link HttpServletResponse} instance
     * @since blojsom 2.23
     */
    public void setHttpServletResponse(HttpServletResponse httpServletResponse) {
        _httpServletResponse = httpServletResponse;
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

    /**
     * Check XML-RPC permissions for a given username
     *
     * @param username Username
     * @param permission Permisison to check
     * @throws XmlRpcException If the username does not have the required permission
     * @since blojsom 2.23
     */
    protected void checkXMLRPCPermission(String username, String permission) throws XmlRpcException {
        try {
            _authorizationProvider.checkPermission(_blogUser, new HashMap(), username, permission);
        } catch (BlojsomException e) {
            throw new XmlRpcException(PERMISSION_EXCEPTION, PERMISSION_EXCEPTION_MSG);
        }
    }
}

