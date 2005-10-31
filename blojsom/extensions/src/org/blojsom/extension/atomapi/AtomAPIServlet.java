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
package org.blojsom.extension.atomapi;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.BlojsomException;
import org.blojsom.plugin.admin.event.AddBlogEntryEvent;
import org.blojsom.plugin.admin.event.DeletedBlogEntryEvent;
import org.blojsom.plugin.admin.event.UpdatedBlogEntryEvent;
import org.blojsom.authorization.AuthorizationProvider;
import org.blojsom.blog.*;
import org.blojsom.fetcher.BlojsomFetcher;
import org.blojsom.fetcher.BlojsomFetcherException;
import org.blojsom.servlet.BlojsomBaseServlet;
import org.blojsom.util.BlojsomConstants;
import org.blojsom.util.BlojsomMetaDataConstants;
import org.blojsom.util.BlojsomProperties;
import org.blojsom.util.BlojsomUtils;
import org.intabulas.sandler.AtomConstants;
import org.intabulas.sandler.Sandler;
import org.intabulas.sandler.SyndicationFactory;
import org.intabulas.sandler.api.SearchResults;
import org.intabulas.sandler.api.impl.SearchResultsImpl;
import org.intabulas.sandler.authentication.AtomAuthentication;
import org.intabulas.sandler.authentication.AuthenticationException;
import org.intabulas.sandler.builders.XPPBuilder;
import org.intabulas.sandler.elements.Entry;
import org.intabulas.sandler.elements.Feed;
import org.intabulas.sandler.elements.Link;
import org.intabulas.sandler.elements.impl.LinkImpl;
import org.intabulas.sandler.exceptions.MarshallException;
import org.intabulas.sandler.serialization.SerializationException;

import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.*;
import java.util.*;

/**
 * AtomAPIServlet
 * <p/>
 * Implementation of J.C. Gregorio's <a href="http://bitworking.org/projects/atom/draft-gregorio-09.html">Atom API</a>.
 *
 * @author Mark Lussier
 * @version $Id: AtomAPIServlet.java,v 1.2.2.1 2005/07/21 04:30:22 johnan Exp $
 * @since blojsom 2.0
 */
public class AtomAPIServlet extends BlojsomBaseServlet implements BlojsomConstants, BlojsomMetaDataConstants, AtomAPIConstants {

    /**
     * Logger instance
     */
    private Log _logger = LogFactory.getLog(AtomAPIServlet.class);

    private static final String ATOM_API_PERMISSION = "post_via_atom_api";

    private AuthorizationProvider _authorizationProvider;
    private ServletConfig _servletConfig;

    /**
     * Default constructor
     */
    public AtomAPIServlet() {
    }

    /**
     * Configure the authorization provider
     *
     * @throws ServletException If there is an error instantiating and/or initializing the authorization provider
     */
    protected void configureAuthorization(ServletConfig servletConfig) throws ServletException {
        try {
            Class authorizationProviderClass = Class.forName(_blojsomConfiguration.getAuthorizationProvider());
            _authorizationProvider = (AuthorizationProvider) authorizationProviderClass.newInstance();
            _authorizationProvider.init(servletConfig, _blojsomConfiguration);
        } catch (ClassNotFoundException e) {
            _logger.error(e);
            throw new ServletException(e);
        } catch (InstantiationException e) {
            _logger.error(e);
            throw new ServletException(e);
        } catch (IllegalAccessException e) {
            _logger.error(e);
            throw new ServletException(e);
        } catch (BlojsomConfigurationException e) {
            _logger.error(e);
            throw new ServletException(e);
        }
    }

    /**
     * Initialize the blojsom AtomAPI servlet
     *
     * @param servletConfig Servlet configuration information
     * @throws ServletException If there is an error initializing the servlet
     */
    public void init(ServletConfig servletConfig) throws ServletException {
        super.init(servletConfig);
        _servletConfig = servletConfig;

        configureBlojsom(servletConfig);
        configureAuthorization(servletConfig);

        _logger.info("AtomAPI initialized");
    }

    /**
     * Configure the flavors for the blog which map flavor values like "html" and "rss" to
     * the proper template and content type
     *
     * @param servletConfig Servlet configuration information
     * @param blogUser {@link BlogUser} information
     * @since blojsom 2.22
     */
    protected void configureFlavorsForUser(ServletConfig servletConfig, BlogUser blogUser) throws ServletException {
        String flavorConfiguration = servletConfig.getInitParameter(BLOJSOM_FLAVOR_CONFIGURATION_IP);
        if (BlojsomUtils.checkNullOrBlank(flavorConfiguration)) {
            flavorConfiguration = DEFAULT_FLAVOR_CONFIGURATION_FILE;
        }

        Map flavors = new HashMap();
        Map flavorToTemplateMap = new HashMap();
        Map flavorToContentTypeMap = new HashMap();
        String user = blogUser.getId();

        Properties flavorProperties = new Properties();
        InputStream is = servletConfig.getServletContext().getResourceAsStream(_baseConfigurationDirectory + user + '/' + flavorConfiguration);
        try {
            flavorProperties.load(is);
            is.close();
            _logger.debug("Loaded flavor information for user: " + user);

            Iterator flavorIterator = flavorProperties.keySet().iterator();
            while (flavorIterator.hasNext()) {
                String flavor = (String) flavorIterator.next();
                String[] flavorMapping = BlojsomUtils.parseCommaList(flavorProperties.getProperty(flavor));
                flavors.put(flavor, flavor);
                flavorToTemplateMap.put(flavor, flavorMapping[0]);
                flavorToContentTypeMap.put(flavor, flavorMapping[1]);

            }

            blogUser.setFlavors(flavors);
            blogUser.setFlavorToTemplate(flavorToTemplateMap);
            blogUser.setFlavorToContentType(flavorToContentTypeMap);
        } catch (IOException e) {
            _logger.error(e);
            throw new ServletException(e);
        }
    }

    /**
     * Loads a {@link BlogUser} object for a given user ID
     *
     * @param userID User ID
     * @return {@link BlogUser} configured for the given user ID or <code>null</code> if there is an error loading the user
     */
    protected BlogUser loadBlogUser(String userID) {
        BlogUser blogUser = new BlogUser();
        blogUser.setId(userID);

        try {
            Properties userProperties = new BlojsomProperties();
            InputStream is = _servletConfig.getServletContext().getResourceAsStream(_baseConfigurationDirectory + userID + '/' + BLOG_DEFAULT_PROPERTIES);

            if (is == null) {
                return null;
            }
            
            userProperties.load(is);
            is.close();
            Blog userBlog = null;

            // If a global blog-home directory has been defined, use it for each user
            if (!BlojsomUtils.checkNullOrBlank(_blojsomConfiguration.getGlobalBlogHome()) &&
                    !userProperties.containsKey(BLOG_HOME_IP)) {
                String usersBlogHome = _blojsomConfiguration.getGlobalBlogHome() + userID + "/";
                File blogHomeDirectory = new File(usersBlogHome);
                if (!blogHomeDirectory.exists()) {
                    _logger.error("Unable to use blog-home directory for user: " + blogHomeDirectory.toString());
                    throw new BlojsomConfigurationException("Unable to use blog-home directory for user: " + blogHomeDirectory.toString());
                }

                userProperties.setProperty(BLOG_HOME_IP, usersBlogHome);
                _logger.debug("Setting user blog-home directory: " + usersBlogHome);
            }

            userBlog = new Blog(userProperties);
            blogUser.setBlog(userBlog);
            configureFlavorsForUser(_servletConfig, blogUser);
            _logger.debug("Configured blojsom user: " + blogUser.getId());
        } catch (BlojsomConfigurationException e) {
            _logger.error(e);
            return null;
        } catch (IOException e) {
            _logger.error(e);
            return null;
        } catch (ServletException e) {
            _logger.error(e);
            return null;
        }

        return blogUser;
    }

    /**
     * Is the request from an authorized poster to this blog?
     *
     * @param blogUser
     * @param httpServletRequest Request
     * @return a boolean indicating if the user was authorized or not
     */
    private boolean isAuthorized(BlogUser blogUser, HttpServletRequest httpServletRequest) {
        Blog blog = blogUser.getBlog();
        try {
            _authorizationProvider.loadAuthenticationCredentials(blogUser);
        } catch (BlojsomException e) {
            _logger.error(e);

            return false;
        }

        boolean result = false;

        if (httpServletRequest.getHeader(ATOMHEADER_WSSE_AUTHORIZATION) != null) {
            AtomAuthentication auth = new AtomAuthentication(httpServletRequest.getHeader(ATOMHEADER_WSSE_AUTHORIZATION));
            Map authMap = blog.getAuthorization();
            if (authMap.containsKey(auth.getUsername())) {
                try {
                    _authorizationProvider.checkPermission(blogUser, new HashMap(), auth.getUsername(), ATOM_API_PERMISSION);
                    result = auth.authenticate(BlojsomUtils.parseCommaList((String) authMap.get(auth.getUsername()))[0]);
                } catch (AuthenticationException e) {
                    _logger.error(e.getMessage(), e);
                } catch (BlojsomException e) {
                    _logger.error(e);
                }
            } else {
                _logger.info("Unable to locate user [" + auth.getUsername() + "] in authorization table");
            }

            if (!result) {
                _logger.info("Unable to authenticate user [" + auth.getUsername() + "]");
            }
        }
        return result;
    }


    /**
     * Send back failed authorization response
     *
     * @param httpServletResponse Response
     * @param user                BlogUser instance
     */
    private void sendAuthenticationRequired(HttpServletResponse httpServletResponse, BlogUser user) {
        httpServletResponse.setContentType(CONTENTTYPE_HTML);

        // Send the NextNonce as part of a WWW-Authenticate header
        httpServletResponse.setHeader(HEADER_WWWAUTHENTICATE, AUTHENTICATION_REALM);
        httpServletResponse.setStatus(401);
    }


    /**
     * Is this an AtomAPI search request?
     *
     * @param request Request
     * @return <code>true</code> if the request is a search request, <code>false</code> otherwise
     */
    private boolean isSearchRequest(HttpServletRequest request) {
        Map paramMap = request.getParameterMap();

        // Looks for the existence of specific params and also checks the QueryString for a name only param
        return (paramMap.containsKey(KEY_ATOMALL) || paramMap.containsKey(KEY_ATOMLAST) || paramMap.containsKey("start-range")
                || (request.getQueryString().indexOf(KEY_ATOMALL) != -1));
    }


    /**
     * Process the Search request
     *
     * @param request  Request
     * @param category Blog category
     * @param blog     Blog instance
     * @param blogUser BlogUser instance
     * @return the search result as a String
     */
    private String processSearchRequest(HttpServletRequest request, String category, Blog blog, BlogUser blogUser) {
        String result = null;
        Map paramMap = request.getParameterMap();
        int numPosts = -1;
        // Did they specify how many entries?
        if (paramMap.containsKey(KEY_ATOMLAST)) {
            try {
                numPosts = Integer.parseInt(((String[]) paramMap.get(KEY_ATOMLAST))[0]);
                if (numPosts < -1 || numPosts == 0) {
                    numPosts = -1;
                }
            } catch (NumberFormatException e) {
                numPosts = -1;
            }
        }

        Map fetchMap = new HashMap();
        BlogCategory blogCategory = _fetcher.newBlogCategory();
        blogCategory.setCategory(category);
        blogCategory.setCategoryURL(blog.getBlogURL() + category);
        fetchMap.put(BlojsomFetcher.FETCHER_CATEGORY, blogCategory);
        fetchMap.put(BlojsomFetcher.FETCHER_NUM_POSTS_INTEGER, new Integer(numPosts));
        try {
            BlogEntry[] entries = _fetcher.fetchEntries(fetchMap, blogUser);

            if (entries != null && entries.length > 0) {
                SearchResults searchResult = new SearchResultsImpl();
                for (int x = 0; x < entries.length; x++) {
                    BlogEntry entry = entries[x];
                    Entry atomentry = AtomUtils.fromBlogEntrySearch(blog, blogUser, entry, request.getServletPath());
                    searchResult.addEntry(atomentry);
                }

                result = searchResult.toString();
            }
        } catch (BlojsomFetcherException e) {
            _logger.error(e.getLocalizedMessage(), e);
        }

        return result;
    }


    /**
     * Handle a Delete Entry message
     *
     * @param httpServletRequest  Request
     * @param httpServletResponse Response
     * @throws ServletException If there is an error processing the request
     * @throws IOException      If there is an error during I/O
     */
    protected void doDelete(HttpServletRequest httpServletRequest, HttpServletResponse httpServletResponse) throws ServletException, IOException {
        httpServletRequest.setCharacterEncoding(UTF8);

        Blog blog = null;
        BlogUser blogUser = null;

        String permalink = BlojsomUtils.getRequestValue(PERMALINK_PARAM, httpServletRequest);
        String category = BlojsomUtils.getCategoryFromPath(httpServletRequest.getPathInfo());
        category = BlojsomUtils.urlDecode(category);
        String user = BlojsomUtils.getUserFromPath(httpServletRequest.getPathInfo());

        _logger.info("AtomAPI Delete Called ================================================");
        _logger.info("       Path: " + httpServletRequest.getPathInfo());
        _logger.info("       User: " + user);
        _logger.info("   Category: " + category);
        _logger.info("  Permalink: " + permalink);

        if (BlojsomUtils.checkNullOrBlank(user)) {
            user = _blojsomConfiguration.getDefaultUser();
        }

        blogUser = loadBlogUser(user);
        if (blogUser == null) {
            _logger.error("Unable to configure user: " + user);
            httpServletResponse.setStatus(404);

            return;
        }

        blog = blogUser.getBlog();

        if (isAuthorized(blogUser, httpServletRequest)) {
            _logger.info("Fetching " + permalink);
            Map fetchMap = new HashMap();
            BlogCategory blogCategory = _fetcher.newBlogCategory();
            blogCategory.setCategory(category);
            blogCategory.setCategoryURL(blog.getBlogURL() + category);
            fetchMap.put(BlojsomFetcher.FETCHER_CATEGORY, blogCategory);
            fetchMap.put(BlojsomFetcher.FETCHER_PERMALINK, permalink);
            try {
                BlogEntry[] entries = _fetcher.fetchEntries(fetchMap, blogUser);
                if (entries != null && entries.length > 0) {
                    BlogEntry entry = entries[0];
                    entry.delete(blogUser);
                }

                // Okay now we generate a new NextOnce value, just for saftey sake and shove in into the response
                //String nonce = AtomUtils.generateNextNonce(blogUser);
                //httpServletResponse.setHeader(ATOMHEADER_AUTHENTICATION_INFO, ATOM_TOKEN_NEXTNONCE + nonce + "\"");
                httpServletResponse.setStatus(200);

                // Send out a deleted blog entry event
                _blojsomConfiguration.getEventBroadcaster().broadcastEvent(new DeletedBlogEntryEvent(this, new Date(), entries[0], blogUser));
            } catch (BlojsomFetcherException e) {
                _logger.error(e.getLocalizedMessage(), e);
                httpServletResponse.setStatus(404);
            } catch (BlojsomException e) {
                _logger.error(e.getLocalizedMessage(), e);
                httpServletResponse.setStatus(404);
            }
        } else {
            sendAuthenticationRequired(httpServletResponse, blogUser);
        }
    }


    /**
     * Creates an AtomAPI Introspection response
     *
     * @param blog        Blog Instance
     * @param user        BlogUser Instance
     * @param servletPath URL path to Atom API servlet
     * @return URL appropriate for introspection
     */
    private String createIntrospectionResponse(Blog blog, BlogUser user, String servletPath) {
        String atomuri = blog.getBlogBaseURL() + servletPath + "/" + user.getId() + "/";
        String atomuri2 = blog.getBlogURL() + "?flavor=atom";

        Feed feed = SyndicationFactory.newSyndicationFeed();

        LinkImpl link = new LinkImpl();
        link.setHref(atomuri);
        link.setRelationship(AtomConstants.Rel.SERVICE_POST);
        link.setType(AtomConstants.Type.ATOM_XML);
        link.setTitle(blog.getBlogDescription());
        feed.addLink(link);

        LinkImpl link3 = new LinkImpl();
        link3.setHref(atomuri);
        link3.setRelationship(AtomConstants.Rel.SERVICE_EDIT);
        link3.setType(AtomConstants.Type.ATOM_XML);
        link3.setTitle(blog.getBlogDescription());
        feed.addLink(link3);

        LinkImpl link2 = new LinkImpl();
        link2.setHref(atomuri2);
        link2.setRelationship(AtomConstants.Rel.SERVICE_FEED);
        link2.setType(AtomConstants.Type.ATOM_XML);
        link2.setTitle(blog.getBlogDescription());
        feed.addLink(link2);

        String result = "";
        try {
            result = Sandler.marshallFeed(feed);
        } catch (MarshallException e) {
            _logger.error(e);
        } catch (SerializationException e) {
            _logger.error(e);
        }

        return result;
    }


    /**
     * Process a Get Entry message
     *
     * @param httpServletRequest  Request
     * @param httpServletResponse Response
     * @throws ServletException If there is an error processing the request
     * @throws IOException      If there is an error during I/O
     */
    protected void doGet(HttpServletRequest httpServletRequest, HttpServletResponse httpServletResponse) throws ServletException, IOException {
        httpServletRequest.setCharacterEncoding(UTF8);

        Blog blog = null;
        BlogUser blogUser = null;
        String blogEntryExtension;

        String permalink = BlojsomUtils.getRequestValue(PERMALINK_PARAM, httpServletRequest);
        String category = BlojsomUtils.getCategoryFromPath(httpServletRequest.getPathInfo());
        category = BlojsomUtils.urlDecode(category);
        String user = BlojsomUtils.getUserFromPath(httpServletRequest.getPathInfo());

        if (BlojsomUtils.checkNullOrBlank(user)) {
            user = _blojsomConfiguration.getDefaultUser();
        }

        blogUser = loadBlogUser(user);
        if (blogUser == null) {
            _logger.error("Unable to configure user: " + user);
            httpServletResponse.setStatus(404);

            return;
        }

        blog = blogUser.getBlog();

        // Check to see if we need to dynamically determine blog-base-url and blog-url?
        BlojsomUtils.resolveDynamicBaseAndBlogURL(httpServletRequest, blog, user);

        blogEntryExtension = blog.getBlogProperty(BLOG_ATOMAPI_ENTRY_EXTENSION_IP);
        if (BlojsomUtils.checkNullOrBlank(blogEntryExtension)) {
            blogEntryExtension = DEFAULT_BLOG_ATOMAPI_ENTRY_EXTENSION;
        }

        _logger.info("AtomAPI GET Called ==================================================");
        _logger.info("       Path: " + httpServletRequest.getPathInfo());
        _logger.info("       User: " + user);
        _logger.info("   Category: " + category);
        _logger.info("  Permalink: " + permalink);
        _logger.info("      Query: " + httpServletRequest.getQueryString());
        _logger.info(" Params Cnt: " + httpServletRequest.getParameterMap().size());

        boolean hasParams = ((httpServletRequest.getParameterMap().size() > 0) || httpServletRequest.getQueryString() != null);

        // NOTE: Assumes that the getPathInfo() returns only category data
        String content = null;

        if (isAuthorized(blogUser, httpServletRequest)) {
            if (!hasParams) {
                content = createIntrospectionResponse(blog, blogUser, httpServletRequest.getServletPath());
                httpServletResponse.setContentType(CONTENTTYPE_ATOM);

            } else if (isSearchRequest(httpServletRequest)) {
                httpServletResponse.setContentType(CONTENTTYPE_XML);
                if (isSearchRequest(httpServletRequest)) {
                    content = processSearchRequest(httpServletRequest, category, blog, blogUser);
                }
            } else {
                _logger.info("Fetching " + permalink);
                Map fetchMap = new HashMap();
                BlogCategory blogCategory = _fetcher.newBlogCategory();
                blogCategory.setCategory(category);
                blogCategory.setCategoryURL(blog.getBlogURL() + category);
                fetchMap.put(BlojsomFetcher.FETCHER_CATEGORY, blogCategory);

                if (permalink != null) {
                    fetchMap.put(BlojsomFetcher.FETCHER_PERMALINK, permalink);
                }

                try {
                    BlogEntry[] entries = _fetcher.fetchEntries(fetchMap, blogUser);

                    if (entries != null && entries.length > 0) {
                        BlogEntry entry = entries[0];
                        Entry atomentry = AtomUtils.fromBlogEntry(blog, blogUser, entry, httpServletRequest.getServletPath());

                        String edituri = blog.getBlogBaseURL() + httpServletRequest.getServletPath() + "/" + blogUser.getId() + entry.getId();
                        LinkImpl link = new LinkImpl();
                        link.setHref(edituri);
                        link.setRelationship(AtomConstants.Rel.SERVICE_EDIT);
                        link.setType(AtomConstants.Type.ATOM_XML);
                        atomentry.addLink(link);

                        content = Sandler.marshallEntry(atomentry);
                        httpServletResponse.setContentType(CONTENTTYPE_ATOM);
                    }
                } catch (MarshallException e) {
                    _logger.error(e);
                    httpServletResponse.setStatus(404);
                } catch (SerializationException e) {
                    _logger.error(e);
                    httpServletResponse.setStatus(404);
                } catch (BlojsomFetcherException e) {
                    _logger.error(e);
                    httpServletResponse.setStatus(404);
                }
            }

            if (content != null) {
                httpServletResponse.setStatus(200);
                httpServletResponse.setContentLength(content.length());
                OutputStreamWriter osw = new OutputStreamWriter(httpServletResponse.getOutputStream(), UTF8);
                osw.write(content);
                osw.flush();
            } else {
                httpServletResponse.setStatus(404);
            }

        } else {
            sendAuthenticationRequired(httpServletResponse, blogUser);
        }
    }

    /**
     * Handle a Post Entry request
     *
     * @param httpServletRequest  Request
     * @param httpServletResponse Response
     * @throws ServletException If there is an error processing the request
     * @throws IOException      If there is an error during I/O
     */
    protected void doPost(HttpServletRequest httpServletRequest, HttpServletResponse httpServletResponse) throws ServletException, IOException {
        httpServletRequest.setCharacterEncoding(UTF8);

        // Check for SOAP request
        if (httpServletRequest.getHeader(HEADER_SOAPACTION) != null) {
            handleSOAPRequest(httpServletRequest, httpServletResponse);

            return;
        }

        Blog blog = null;
        BlogUser blogUser = null;
        String blogEntryExtension = DEFAULT_BLOG_ATOMAPI_ENTRY_EXTENSION;

        String permalink = BlojsomUtils.getRequestValue(PERMALINK_PARAM, httpServletRequest);
        String category = BlojsomUtils.getCategoryFromPath(httpServletRequest.getPathInfo());
        category = BlojsomUtils.urlDecode(category);
        String user = BlojsomUtils.getUserFromPath(httpServletRequest.getPathInfo());

        _logger.info("AtomAPI POST Called =================================================");
        _logger.info("       Path: " + httpServletRequest.getPathInfo());
        _logger.info("       User: " + user);
        _logger.info("   Category: " + category);
        _logger.info("  Permalink: " + permalink);

        if (BlojsomUtils.checkNullOrBlank(user)) {
            user = _blojsomConfiguration.getDefaultUser();
        }

        blogUser = loadBlogUser(user);
        if (blogUser == null) {
            _logger.error("Unable to configure user: " + user);
            httpServletResponse.setStatus(404);

            return;
        }

        blog = blogUser.getBlog();

        // Check to see if we need to dynamically determine blog-base-url and blog-url?
        BlojsomUtils.resolveDynamicBaseAndBlogURL(httpServletRequest, blog, user);

        blogEntryExtension = blog.getBlogProperty(BLOG_ATOMAPI_ENTRY_EXTENSION_IP);
        if (BlojsomUtils.checkNullOrBlank(blogEntryExtension)) {
            blogEntryExtension = DEFAULT_BLOG_ATOMAPI_ENTRY_EXTENSION;
        }

        if (isAuthorized(blogUser, httpServletRequest)) {

            // Quick verify that the category is valid
            File blogCategory = getBlogCategoryDirectory(blog, category);
            if (blogCategory.exists() && blogCategory.isDirectory()) {

                try {
                    Entry atomEntry = Sandler.unmarshallEntry(httpServletRequest.getInputStream(), new XPPBuilder());

                    String filename = BlojsomUtils.getBlogEntryFilename(atomEntry.getTitle().getBody(), atomEntry.getContent(0).getBody());
                    String outputfile = blogCategory.getAbsolutePath() + File.separator + filename;

                    File sourceFile = new File(outputfile + blogEntryExtension);
                    int fileTag = 1;
                    while (sourceFile.exists()) {
                        sourceFile = new File(outputfile + "-" + fileTag + blogEntryExtension);
                        fileTag++;
                    }

                    BlogEntry entry = _fetcher.newBlogEntry();
                    Map attributeMap = new HashMap();
                    Map blogEntryMetaData = new HashMap();
                    attributeMap.put(SOURCE_ATTRIBUTE, sourceFile);
                    entry.setAttributes(attributeMap);
                    entry.setCategory(category);
                    entry.setDescription(atomEntry.getContent(0).getBody());
                    entry.setDate(atomEntry.getCreated());
                    entry.setTitle(atomEntry.getTitle().getBody());

                    if (atomEntry.getAuthor() != null) {
                        blogEntryMetaData.put(BLOG_ENTRY_METADATA_AUTHOR, atomEntry.getAuthor().getName());
                    } else {
                        blogEntryMetaData.put(BLOG_ENTRY_METADATA_AUTHOR, blog.getBlogOwner());
                    }

                    blogEntryMetaData.put(BLOG_ENTRY_METADATA_TIMESTAMP, new Long(new Date().getTime()).toString());
                    entry.setMetaData(blogEntryMetaData);

                    // Insert an escaped Link into the Blog Entry
                    entry.setLink(blog.getBlogURL() + BlojsomUtils.removeInitialSlash(entry.getId()));

                    entry.save(blogUser);
                    entry.load(blogUser);

                    // Send out an add blog entry event
                    _blojsomConfiguration.getEventBroadcaster().broadcastEvent(new AddBlogEntryEvent(this, new Date(), entry, blogUser));

                    httpServletResponse.setContentType(CONTENTTYPE_ATOM);
                    httpServletResponse.setStatus(201);

                    atomEntry = AtomUtils.fromBlogEntry(blog, blogUser, entry, httpServletRequest.getServletPath());

                    // Extract the service.edit link to send for the Location: header
                    Collection links = atomEntry.getLinks();
                    Iterator linksIterator = links.iterator();
                    while (linksIterator.hasNext()) {
                        Link link = (Link) linksIterator.next();
                        if (AtomConstants.Rel.SERVICE_EDIT.equals(link.getRelationship())) {
                            httpServletResponse.setHeader(HEADER_LOCATION, link.getEscapedHref());
                            break;
                        }
                    }

                    OutputStreamWriter osw = new OutputStreamWriter(httpServletResponse.getOutputStream(), UTF8);
                    osw.write(Sandler.marshallEntry(atomEntry, true));
                    osw.flush();
                } catch (SerializationException e) {
                    _logger.error(e.getLocalizedMessage(), e);
                    httpServletResponse.setStatus(404);
                } catch (MarshallException e) {
                    _logger.error(e.getLocalizedMessage(), e);
                    httpServletResponse.setStatus(404);
                } catch (BlojsomException e) {
                    _logger.error(e);
                    httpServletResponse.setStatus(404);
                }
            }
        } else {
            sendAuthenticationRequired(httpServletResponse, blogUser);
        }
    }

    /**
     * Handle a Put Entry request
     *
     * @param httpServletRequest  Request
     * @param httpServletResponse Response
     * @throws ServletException If there is an error processing the request
     * @throws IOException      If there is an error during I/O
     */
    protected void doPut(HttpServletRequest httpServletRequest, HttpServletResponse httpServletResponse) throws ServletException, IOException {
        httpServletRequest.setCharacterEncoding(UTF8);

        Blog blog = null;
        BlogUser blogUser = null;

        String permalink = BlojsomUtils.getRequestValue(PERMALINK_PARAM, httpServletRequest);
        String category = BlojsomUtils.getCategoryFromPath(httpServletRequest.getPathInfo());
        category = BlojsomUtils.urlDecode(category);
        String user = BlojsomUtils.getUserFromPath(httpServletRequest.getPathInfo());

        _logger.info("AtomAPI PUT Called ==================================================");
        _logger.info("       Path: " + httpServletRequest.getPathInfo());
        _logger.info("       User: " + user);
        _logger.info("   Category: " + category);
        _logger.info("  Permalink: " + permalink);

        if (BlojsomUtils.checkNullOrBlank(user)) {
            user = _blojsomConfiguration.getDefaultUser();
        }

        blogUser = loadBlogUser(user);
        if (blogUser == null) {
            _logger.error("Unable to configure user: " + user);
            httpServletResponse.setStatus(404);

            return;
        }

        blog = blogUser.getBlog();

        // Check to see if we need to dynamically determine blog-base-url and blog-url?
        BlojsomUtils.resolveDynamicBaseAndBlogURL(httpServletRequest, blog, user);

        if (isAuthorized(blogUser, httpServletRequest)) {

            Map fetchMap = new HashMap();
            BlogCategory blogCategory = _fetcher.newBlogCategory();
            blogCategory.setCategory(category);
            blogCategory.setCategoryURL(blog.getBlogURL() + category);
            fetchMap.put(BlojsomFetcher.FETCHER_CATEGORY, blogCategory);
            fetchMap.put(BlojsomFetcher.FETCHER_PERMALINK, permalink);
            try {
                BlogEntry[] entries = _fetcher.fetchEntries(fetchMap, blogUser);

                if (entries != null && entries.length > 0) {

                    Entry atomEntry = Sandler.unmarshallEntry(httpServletRequest.getInputStream(), new XPPBuilder());

                    BlogEntry entry = entries[0];
                    Map blogEntryMetaData = entry.getMetaData();
                    entry.setCategory(category);
                    entry.setDescription(atomEntry.getContent(0).getBody());
                    entry.setTitle(atomEntry.getTitle().getBody());
                    if (atomEntry.getAuthor() != null) {
                        blogEntryMetaData.put(BLOG_ENTRY_METADATA_AUTHOR, atomEntry.getAuthor().getName());
                    }
                    entry.setMetaData(blogEntryMetaData);
                    entry.save(blogUser);

                    //String nonce = AtomUtils.generateNextNonce(blogUser);
                    //httpServletResponse.setHeader(x, ATOM_TOKEN_NEXTNONCE + nonce + "\"");

                    httpServletResponse.setStatus(204);

                    // Send out an updated blog entry event
                    _blojsomConfiguration.getEventBroadcaster().broadcastEvent(new UpdatedBlogEntryEvent(this, new Date(), entry, blogUser));
                } else {
                    _logger.info("Unable to fetch " + permalink);
                }
            } catch (BlojsomFetcherException e) {
                _logger.error(e);
                httpServletResponse.setStatus(404);
            } catch (BlojsomException e) {
                _logger.error(e);
                httpServletResponse.setStatus(404);
            }
        } else {
            sendAuthenticationRequired(httpServletResponse, blogUser);
        }
    }

    /**
     * Handle a given SOAP request looking for the "SOAPAction" header to decide on which method to execute
     *
     * @param httpServletRequest  Request
     * @param httpServletResponse Response
     */
    protected void handleSOAPRequest(HttpServletRequest httpServletRequest, HttpServletResponse httpServletResponse) {
        String soapAction = httpServletRequest.getHeader(HEADER_SOAPACTION);

        if (SOAPACTION_PUT.equalsIgnoreCase(soapAction)) {
            handleSOAPPut(httpServletRequest, httpServletResponse);
        } else {
            try {
                httpServletResponse.sendError(404, "Unable to process SOAP request for unknown action: " + soapAction);
            } catch (IOException e) {
                _logger.error(e);

                httpServletResponse.setStatus(404);
            }
        }
    }

    /**
     * Retrieve an entry body (&lt;entry&gt;...&lt;/entry&gt;) from arbitrary content
     *
     * @param content Content
     * @return Entry body with entry tags included or <code>null</code> if the entry body could not be found
     */
    protected String retrieveEntryBody(String content) {
        String entryStart = "<entry";
        String entryEnd = "</entry>";

        int entryIndexStart = content.indexOf(entryStart);
        int entryIndexEnd = content.indexOf(entryEnd);
        if (entryIndexStart != -1 && entryIndexEnd != -1 && (entryIndexEnd > entryIndexStart)) {
            return content.substring(entryIndexStart, entryIndexEnd + entryEnd.length());
        }

        return null;
    }

    /**
     * Read all the content from a given input stream for a specified length. Content will be read in
     * using UTF-8 as the character encoding. If an exception in reading occurs, a <code>null</code> value
     * is returned.
     *
     * @param is     {@link InputStream}
     * @param length Length of input stream to read
     * @return Content from input stream up to specified length
     */
    protected String readContentFromInputStream(InputStream is, int length) {
        char[] buffer = new char[0];
        try {
            InputStreamReader inputStreamReader = new InputStreamReader(is, UTF8);
            BufferedReader bufferedReader = new BufferedReader(inputStreamReader);
            buffer = new char[length];
            bufferedReader.read(buffer, 0, length);
            bufferedReader.close();
        } catch (IOException e) {
            _logger.error(e);

            return null;
        }

        return new String(buffer);
    }

    /**
     * Handle a SOAP PUT request
     *
     * @param httpServletRequest  Request
     * @param httpServletResponse Response
     */
    protected void handleSOAPPut(HttpServletRequest httpServletRequest, HttpServletResponse httpServletResponse) {
        Blog blog = null;
        BlogUser blogUser = null;
        String blogEntryExtension = DEFAULT_BLOG_ATOMAPI_ENTRY_EXTENSION;

        String permalink = BlojsomUtils.getRequestValue(PERMALINK_PARAM, httpServletRequest);
        String category = BlojsomUtils.getCategoryFromPath(httpServletRequest.getPathInfo());
        category = BlojsomUtils.urlDecode(category);
        String user = BlojsomUtils.getUserFromPath(httpServletRequest.getPathInfo());

        _logger.info("AtomAPI SOAP PUT Called =================================================");
        _logger.info("       Path: " + httpServletRequest.getPathInfo());
        _logger.info("       User: " + user);
        _logger.info("   Category: " + category);
        _logger.info("  Permalink: " + permalink);

        if (BlojsomUtils.checkNullOrBlank(user)) {
            user = _blojsomConfiguration.getDefaultUser();
        }

        blogUser = loadBlogUser(user);
        if (blogUser == null) {
            _logger.error("Unable to configure user: " + user);
            httpServletResponse.setStatus(404);

            return;
        }

        blog = blogUser.getBlog();

        // Check to see if we need to dynamically determine blog-base-url and blog-url?
        BlojsomUtils.resolveDynamicBaseAndBlogURL(httpServletRequest, blog, user);

        blogEntryExtension = blog.getBlogProperty(BLOG_ATOMAPI_ENTRY_EXTENSION_IP);
        if (BlojsomUtils.checkNullOrBlank(blogEntryExtension)) {
            blogEntryExtension = DEFAULT_BLOG_ATOMAPI_ENTRY_EXTENSION;
        }

        if (isAuthorized(blogUser, httpServletRequest)) {
            try {
                String content = readContentFromInputStream(httpServletRequest.getInputStream(), httpServletRequest.getContentLength());
                if (content != null) {
                    String entryContent = retrieveEntryBody(content);
                    if (entryContent != null && entryContent.length() > 0) {
                        Map fetchMap = new HashMap();
                        BlogCategory blogCategory = _fetcher.newBlogCategory();
                        blogCategory.setCategory(category);
                        blogCategory.setCategoryURL(blog.getBlogURL() + category);
                        fetchMap.put(BlojsomFetcher.FETCHER_CATEGORY, blogCategory);
                        fetchMap.put(BlojsomFetcher.FETCHER_PERMALINK, permalink);
                        try {
                            BlogEntry[] entries = _fetcher.fetchEntries(fetchMap, blogUser);

                            if (entries != null && entries.length > 0) {

                                Entry atomEntry = Sandler.unmarshallEntry(entryContent, new XPPBuilder());

                                BlogEntry entry = entries[0];
                                Map blogEntryMetaData = entry.getMetaData();
                                entry.setCategory(category);
                                entry.setDescription(atomEntry.getContent(0).getBody());
                                entry.setTitle(atomEntry.getTitle().getBody());
                                if (atomEntry.getAuthor() != null) {
                                    blogEntryMetaData.put(BLOG_ENTRY_METADATA_AUTHOR, atomEntry.getAuthor().getName());
                                }
                                entry.setMetaData(blogEntryMetaData);

                                // Insert an escaped Link into the Blog Entry
                                entry.setLink(blog.getBlogURL() + BlojsomUtils.removeInitialSlash(entry.getId()));

                                entry.save(blogUser);
                                entry.load(blogUser);

                                httpServletResponse.setContentType(CONTENTTYPE_ATOM);
                                httpServletResponse.setStatus(201);

                                // Send out an add blog entry event
                                _blojsomConfiguration.getEventBroadcaster().broadcastEvent(new AddBlogEntryEvent(this, new Date(), entry, blogUser));

                                atomEntry = AtomUtils.fromBlogEntry(blog, blogUser, entry, httpServletRequest.getServletPath());

                                // Extract the service.edit link to send for the Location: header
                                Collection links = atomEntry.getLinks();
                                Iterator linksIterator = links.iterator();
                                while (linksIterator.hasNext()) {
                                    Link link = (Link) linksIterator.next();
                                    if (AtomConstants.Rel.SERVICE_EDIT.equals(link.getRelationship())) {
                                        httpServletResponse.setHeader(HEADER_LOCATION, link.getEscapedHref());
                                        break;
                                    }
                                }

                                OutputStreamWriter osw = new OutputStreamWriter(httpServletResponse.getOutputStream(), UTF8);
                                osw.write(Sandler.marshallEntry(atomEntry, true));
                                osw.flush();
                            } else {
                                _logger.info("Unable to fetch " + permalink);
                            }
                        } catch (BlojsomFetcherException e) {
                            _logger.error(e);
                            httpServletResponse.setStatus(404);
                        } catch (BlojsomException e) {
                            _logger.error(e);
                            httpServletResponse.setStatus(404);
                        } catch (SerializationException e) {
                            _logger.error(e);
                            httpServletResponse.setStatus(404);
                        } catch (MarshallException e) {
                            _logger.error(e);
                            httpServletResponse.setStatus(404);
                        }
                    } else {
                        httpServletResponse.setStatus(404);
                    }
                } else {
                    httpServletResponse.setStatus(404);
                }
            } catch (IOException e) {
                _logger.error(e);
                httpServletResponse.setStatus(404);
            }
        } else {
            sendAuthenticationRequired(httpServletResponse, blogUser);
        }
    }


    /**
     * Called when removing the servlet from the servlet container
     */
    public void destroy() {
        try {
            _fetcher.destroy();
        } catch (BlojsomFetcherException e) {
            _logger.error(e);
        }
    }


    /**
     * Get the blog category. If the category exists, return the
     * appropriate directory, otherwise return the "root" of this blog.
     *
     * @param categoryName Category name
     * @return A directory into which a blog entry can be placed
     * @since blojsom 1.9
     */
    protected File getBlogCategoryDirectory(Blog blog, String categoryName) {
        File blogCategory = new File(blog.getBlogHome() + BlojsomUtils.removeInitialSlash(categoryName));
        if (blogCategory.exists() && blogCategory.isDirectory()) {
            return blogCategory;
        } else {
            return new File(blog.getBlogHome() + "/");
        }
    }
}

