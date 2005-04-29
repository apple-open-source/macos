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
package org.blojsom.servlet;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.BlojsomException;
import org.blojsom.blog.*;
import org.blojsom.dispatcher.BlojsomDispatcher;
import org.blojsom.fetcher.BlojsomFetcher;
import org.blojsom.fetcher.BlojsomFetcherException;
import org.blojsom.plugin.BlojsomPlugin;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.util.BlojsomUtils;
import org.blojsom.util.resources.ResourceManager;

import com.apple.blojsom.util.BlojsomAppleUtils;

import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.IOException;
import java.io.InputStream;
import java.io.UnsupportedEncodingException;
import java.util.*;

/**
 * BlojsomServlet
 *
 * @author David Czarnecki
 * @author Mark Lussier
 * @version $Id: BlojsomServlet.java,v 1.5 2005/03/01 00:29:03 johnan Exp $
 */
public class BlojsomServlet extends BlojsomBaseServlet {

    // BlojsomServlet initialization properties from web.xml
    private static final String BLOJSOM_DISPATCHER_MAP_CONFIGURATION_IP = "dispatcher-configuration";

    private Log _logger = LogFactory.getLog(BlojsomServlet.class);

    private Map _plugins;
    private Map _dispatchers;
    private ResourceManager _resourceManager;
	private ServletConfig _servletConfig;

    /**
     * Create a new blojsom servlet instance
     */
    public BlojsomServlet() {
    }

    /**
     * Configure the dispatchers that blojsom will use when passing a request/response on to a
     * particular template
     *
     * @param servletConfig Servlet configuration information
     */
    protected void configureDispatchers(ServletConfig servletConfig) throws ServletException {
        String templateConfiguration = servletConfig.getInitParameter(BLOJSOM_DISPATCHER_MAP_CONFIGURATION_IP);
        _dispatchers = new HashMap();
        Properties templateMapProperties = new Properties();
        InputStream is = servletConfig.getServletContext().getResourceAsStream(templateConfiguration);
        try {
            templateMapProperties.load(is);
            is.close();
            Iterator templateIterator = templateMapProperties.keySet().iterator();
            while (templateIterator.hasNext()) {
                String templateExtension = (String) templateIterator.next();
                String templateDispatcherClass = templateMapProperties.getProperty(templateExtension);
                Class dispatcherClass = Class.forName(templateDispatcherClass);
                BlojsomDispatcher dispatcher = (BlojsomDispatcher) dispatcherClass.newInstance();
                dispatcher.init(servletConfig, _blojsomConfiguration);
                _dispatchers.put(templateExtension, dispatcher);
                _logger.debug("Added template dispatcher: " + templateDispatcherClass);
            }
        } catch (InstantiationException e) {
            _logger.error(e);
        } catch (IllegalAccessException e) {
            _logger.error(e);
        } catch (ClassNotFoundException e) {
            _logger.error(e);
        } catch (IOException e) {
            _logger.error(e);
            throw new ServletException(e);
        } catch (BlojsomException e) {
            _logger.error(e);
            throw new ServletException(e);
        }
    }

    /**
     * Configure the flavors for the blog which map flavor values like "html" and "rss" to
     * the proper template and content type
     *
     * @param servletConfig Servlet configuration information
     */
    protected void configureFlavors(ServletConfig servletConfig) throws ServletException {
        String flavorConfiguration = servletConfig.getInitParameter(BLOJSOM_FLAVOR_CONFIGURATION_IP);
        Iterator usersIterator = _blojsomConfiguration.getBlogUsers().keySet().iterator();
        BlogUser blogUser;
        while (usersIterator.hasNext()) {
            Map flavors = new HashMap();
            Map flavorToTemplateMap = new HashMap();
            Map flavorToContentTypeMap = new HashMap();
            String user = (String) usersIterator.next();
            blogUser = (BlogUser) _blojsomConfiguration.getBlogUsers().get(user);

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
                _blojsomConfiguration.getBlogUsers().put(user, blogUser);
            } catch (IOException e) {
                _logger.error(e);
                throw new ServletException(e);
            }
        }
    }

    /**
     * Configure the plugins that blojsom will use
     *
     * @param servletConfig Servlet configuration information
     */
    protected void configurePlugins(ServletConfig servletConfig) throws ServletException {
        // Instantiate the plugins
        String pluginConfiguration = servletConfig.getInitParameter(BLOJSOM_PLUGIN_CONFIGURATION_IP);
        if (BlojsomUtils.checkNullOrBlank(pluginConfiguration)) {
            _logger.error("No plugin configuration file specified");
            throw new ServletException("No plugin configuration file specified");
        }

        // Load the plugin chains for the individual users
        Iterator usersIterator = _blojsomConfiguration.getBlogUsers().keySet().iterator();
        Iterator pluginIterator;
        BlogUser blogUser;
        Properties pluginProperties;

        while (usersIterator.hasNext()) {
            Map pluginChainMap = new HashMap();
            String user = (String) usersIterator.next();
            blogUser = (BlogUser) _blojsomConfiguration.getBlogUsers().get(user);

            InputStream is = servletConfig.getServletContext().getResourceAsStream(_baseConfigurationDirectory + user + '/' + pluginConfiguration);
            pluginProperties = new Properties();
            try {
                pluginProperties.load(is);
                is.close();
                pluginIterator = pluginProperties.keySet().iterator();
                while (pluginIterator.hasNext()) {
                    String plugin = (String) pluginIterator.next();
                    if (plugin.indexOf(BLOJSOM_PLUGIN_CHAIN) != -1) {
                        pluginChainMap.put(plugin, BlojsomUtils.parseCommaList(pluginProperties.getProperty(plugin)));
                        _logger.debug("Added plugin chain: " + plugin + '=' + pluginProperties.getProperty(plugin) + " for user: " + user);
                    }
                }
                blogUser.setPluginChain(pluginChainMap);
                _blojsomConfiguration.getBlogUsers().put(user, blogUser);
            } catch (IOException e) {
                _logger.error(e);
                throw new ServletException(e);
            }
        }

        _plugins = new HashMap();
        String pluginConfigurationLocation = _baseConfigurationDirectory + pluginConfiguration;
        pluginProperties = new Properties();
        try {
            pluginProperties = BlojsomUtils.loadProperties(servletConfig, pluginConfigurationLocation);
        } catch (BlojsomException e) {
            _logger.error(e);
            throw new ServletException(e);
        }

        // Instantiate the plugin classes
        pluginIterator = pluginProperties.keySet().iterator();
        while (pluginIterator.hasNext()) {
            String plugin = (String) pluginIterator.next();
            if (plugin.indexOf(BLOJSOM_PLUGIN_CHAIN) != -1) {
                // Just in case we are migrating from a blojsom 1.x installation
                _logger.debug("Skipping blojsom plugin chain in global plugin configuration file");
            } else {
                String pluginClassName = pluginProperties.getProperty(plugin);
                try {
                    Class pluginClass = Class.forName(pluginClassName);
                    BlojsomPlugin blojsomPlugin = (BlojsomPlugin) pluginClass.newInstance();
                    blojsomPlugin.init(servletConfig, _blojsomConfiguration);
                    _plugins.put(plugin, blojsomPlugin);
                    _logger.info("Added blojsom plugin: " + pluginClassName);
                } catch (BlojsomPluginException e) {
                    _logger.error(e);
                } catch (InstantiationException e) {
                    _logger.error(e);
                } catch (IllegalAccessException e) {
                    _logger.error(e);
                } catch (ClassNotFoundException e) {
                    _logger.error(e);
                }
            }
        }
    }

    /**
     * Instantiate the resource manager
     *
     * @throws ServletException If there is an error instantiating the resource manager class
     */
    protected void configureResourceManager() throws ServletException {
        String resourceManagerClass = _blojsomConfiguration.getResourceManager();

        try {
            Class resourceManagerClazz = Class.forName(resourceManagerClass);
            _resourceManager = (ResourceManager) resourceManagerClazz.newInstance();
            _resourceManager.init(_blojsomConfiguration);
        } catch (InstantiationException e) {
            _logger.error(e);
            throw new ServletException(e);
        } catch (IllegalAccessException e) {
            _logger.error(e);
            throw new ServletException(e);
        } catch (ClassNotFoundException e) {
            _logger.error(e);
            throw new ServletException(e);
        } catch (BlojsomException e) {
            _logger.error(e);
            throw new ServletException(e);
        }
    }

    /**
     * Initialize blojsom: configure blog, configure flavors, configure dispatchers
     *
     * @param servletConfig Servlet configuration information
     * @throws ServletException If there is an error initializing blojsom
     */
    public void init(ServletConfig servletConfig) throws ServletException {
        super.init(servletConfig);
		_servletConfig = servletConfig;
        configureBlojsom(servletConfig);
        configureDispatchers(servletConfig);
        configureFlavors(servletConfig);
        configurePlugins(servletConfig);
        configureResourceManager();

        _logger.debug("blojsom: All Your Blog Are Belong To Us");
    }

    /**
     * Service a request to blojsom
     *
     * @param httpServletRequest  Request
     * @param httpServletResponse Response
     * @throws ServletException If there is an error processing the request
     * @throws IOException      If there is an error in IO
     */
    protected void service(HttpServletRequest httpServletRequest, HttpServletResponse httpServletResponse) throws ServletException, IOException {
        try {
            httpServletRequest.setCharacterEncoding(UTF8);
        } catch (UnsupportedEncodingException e) {
            _logger.error(e);
        }

        // Make sure that we have a request URI ending with a / otherwise we need to
        // redirect so that the browser can handle relative link generation
        if (!httpServletRequest.getRequestURI().endsWith("/")) {
            StringBuffer redirectURL = new StringBuffer();
            redirectURL.append(httpServletRequest.getRequestURI());
            redirectURL.append("/");
            if (httpServletRequest.getParameterMap().size() > 0) {
                redirectURL.append("?");
                redirectURL.append(BlojsomUtils.convertRequestParams(httpServletRequest));
            }
            _logger.debug("Redirecting the user to: " + redirectURL.toString());
            httpServletResponse.sendRedirect(redirectURL.toString());
            return;
        }

        // Setup the initial context for the fetcher, plugins, and finally the dispatcher
        HashMap context = new HashMap();

        // Check for an overriding id
        String user = httpServletRequest.getParameter("id");
        if (BlojsomUtils.checkNullOrBlank(user)) {
            String userFromPath = BlojsomUtils.getUserFromPath(httpServletRequest.getPathInfo());
            if (userFromPath == null) {
                user = _blojsomConfiguration.getDefaultUser();
            } else {
				if (!BlojsomAppleUtils.attemptUserBlogCreation(_blojsomConfiguration, _servletConfig, userFromPath)) {
                    user = _blojsomConfiguration.getDefaultUser();
					context.put("BLOJSOM_ADMIN_PLUGIN_OPERATION_RESULT", " message.loginfailed");
                } else {
                    user = userFromPath;
                }
            }
        }

        // Make sure the user exists and if not, return a 404
        if (!_blojsomConfiguration.getBlogUsers().containsKey(user)) {
            httpServletResponse.sendError(HttpServletResponse.SC_NOT_FOUND, "Requested user not found: " + user);
            return;
        }

        BlogUser blogUser = (BlogUser) _blojsomConfiguration.getBlogUsers().get(user);
        Blog blog = blogUser.getBlog();

        // Determine the requested flavor
        String flavor = httpServletRequest.getParameter(FLAVOR_PARAM);
        if (BlojsomUtils.checkNullOrBlank(flavor)) {
            flavor = blog.getBlogDefaultFlavor();
            if (blogUser.getFlavors().get(flavor) == null) {
                flavor = DEFAULT_FLAVOR_HTML;
            }
        } else {
            if (blogUser.getFlavors().get(flavor) == null) {
                flavor = blog.getBlogDefaultFlavor();
                if (blogUser.getFlavors().get(flavor) == null) {
                    flavor = DEFAULT_FLAVOR_HTML;
                }
            }
        }

        // Setup the resource manager in the context
        context.put(BLOJSOM_RESOURCE_MANAGER_CONTEXT_KEY, _resourceManager);

        BlogEntry[] entries = null;
        BlogCategory[] categories = null;

        // Fetch the categories and entries for the request
        try {
            categories = _fetcher.fetchCategories(httpServletRequest, httpServletResponse, blogUser, flavor, context);
            entries = _fetcher.fetchEntries(httpServletRequest, httpServletResponse, blogUser, flavor, context);
        } catch (BlojsomFetcherException e) {
            _logger.error(e);
        }

        String[] pluginChain = null;
        Map pluginChainMap = blogUser.getPluginChain();

        // Check to see if the user would like to override the plugin chain
        if (httpServletRequest.getParameter(PLUGINS_PARAM) != null) {
            pluginChain = BlojsomUtils.parseCommaList(httpServletRequest.getParameter(PLUGINS_PARAM));
        } else {
            String pluginChainMapKey = flavor + '.' + BLOJSOM_PLUGIN_CHAIN;
            String[] pluginChainValue = (String[]) pluginChainMap.get(pluginChainMapKey);
            if (pluginChainValue != null && pluginChainValue.length > 0) {
                pluginChain = (String[]) pluginChainMap.get(pluginChainMapKey);
            } else {
                pluginChain = (String[]) pluginChainMap.get(BLOJSOM_PLUGIN_CHAIN);
            }
        }

        // Invoke the plugins in the order in which they were specified
        if ((entries != null) && (pluginChain != null) && (!"".equals(pluginChain))) {
            for (int i = 0; i < pluginChain.length; i++) {
                String plugin = pluginChain[i];
                if (_plugins.containsKey(plugin)) {
                    BlojsomPlugin blojsomPlugin = (BlojsomPlugin) _plugins.get(plugin);
                    _logger.debug("blojsom plugin execution: " + blojsomPlugin.getClass().getName());
                    try {
                        entries = blojsomPlugin.process(httpServletRequest, httpServletResponse, blogUser, context, entries);
                        blojsomPlugin.cleanup();
                    } catch (BlojsomPluginException e) {
                        _logger.error(e);
                    }
                } else {
                    _logger.error("No plugin loaded for: " + plugin);
                }
            }
        }

        String blogdate = null;
        String blogISO8601Date = null;
        String blogUTCDate = null;
        Date blogDateObject = null;

        boolean sendLastModified = true;
        if (httpServletRequest.getParameter(OVERRIDE_LASTMODIFIED_PARAM) != null) {
            sendLastModified = Boolean.getBoolean(httpServletRequest.getParameter(OVERRIDE_LASTMODIFIED_PARAM));
        }

        // If we have entries, construct a last modified on the most recent entry
        // Additionally, set the blog date
        if (sendLastModified) {
            if ((entries != null) && (entries.length > 0)) {
                BlogEntry _entry = entries[0];
                long _lastmodified;

                if (_entry.getNumComments() > 0) {
                    BlogComment _comment = _entry.getCommentsAsArray()[_entry.getNumComments() - 1];
                    _lastmodified = _comment.getCommentDateLong();
                    _logger.debug("Adding last-modified header for most recent entry comment");
                } else {
                    _lastmodified = _entry.getLastModified();
                    _logger.debug("Adding last-modified header for most recent blog entry");
                }

                // Check for the Last-Modified object from one of the plugins
                if (context.containsKey(BLOJSOM_LAST_MODIFIED)) {
                    Long lastModified = (Long) context.get(BLOJSOM_LAST_MODIFIED);
                    if (lastModified.longValue() > _lastmodified) {
                        _lastmodified = lastModified.longValue();
                    }
                }

                // Generates an ETag header based on the string value of LastModified as an ISO8601 Format
                String etagLastModified = BlojsomUtils.getISO8601Date(new Date(_lastmodified));
                httpServletResponse.addHeader(HTTP_ETAG, "\"" + BlojsomUtils.digestString(etagLastModified) + "\"");

                httpServletResponse.addDateHeader(HTTP_LASTMODIFIED, _lastmodified);
                blogdate = entries[0].getRFC822Date();
                blogISO8601Date = entries[0].getISO8601Date();
                blogDateObject = entries[0].getDate();
                blogUTCDate = BlojsomUtils.getUTCDate(entries[0].getDate());
            } else {
                _logger.debug("Adding last-modified header for current date");
                Date today = new Date();
                blogdate = BlojsomUtils.getRFC822Date(today);
                blogISO8601Date = BlojsomUtils.getISO8601Date(today);
                blogUTCDate = BlojsomUtils.getUTCDate(today);
                blogDateObject = today;
                httpServletResponse.addDateHeader(HTTP_LASTMODIFIED, today.getTime());
                // Generates an ETag header based on the string value of LastModified as an ISO8601 Format
                httpServletResponse.addHeader(HTTP_ETAG, "\"" + BlojsomUtils.digestString(blogISO8601Date) + "\"");
            }
        }

        context.put(BLOJSOM_DATE, blogdate);
        context.put(BLOJSOM_DATE_ISO8601, blogISO8601Date);
        context.put(BLOJSOM_DATE_OBJECT, blogDateObject);
        context.put(BLOJSOM_DATE_UTC, blogUTCDate);

        // Finish setting up the context for the dispatcher
        context.put(BLOJSOM_BLOG, blog);
        context.put(BLOJSOM_SITE_URL, blog.getBlogBaseURL());
        context.put(BLOJSOM_ENTRIES, entries);
        context.put(BLOJSOM_CATEGORIES, categories);
        context.put(BLOJSOM_COMMENTS_ENABLED, blog.getBlogCommentsEnabled());
        context.put(BLOJSOM_VERSION, BLOJSOM_VERSION_NUMBER);
        context.put(BLOJSOM_USER, blogUser.getId());

        // Forward the request on to the template for the requested flavor
        String flavorTemplate;
        Map flavorToTemplate = blogUser.getFlavorToTemplate();
        if (flavorToTemplate.get(flavor) == null) {
            flavorTemplate = (String) flavorToTemplate.get(DEFAULT_FLAVOR_HTML);
        } else {
            flavorTemplate = (String) flavorToTemplate.get(flavor);
        }

        // Get the content type for the requested flavor
        Map flavorToContentType = blogUser.getFlavorToContentType();
        String flavorContentType = (String) flavorToContentType.get(flavor);

        String templateExtension = BlojsomUtils.getFileExtension(flavorTemplate);
        _logger.debug("Template extension: " + templateExtension);

        // Retrieve the appropriate dispatcher for the template
        BlojsomDispatcher dispatcher = (BlojsomDispatcher) _dispatchers.get(templateExtension);
        dispatcher.dispatch(httpServletRequest, httpServletResponse, blogUser, context, flavorTemplate, flavorContentType);
    }

    /**
     * Called when removing the servlet from the servlet container. Also calls the
     * {@link BlojsomPlugin#destroy} method for each of the plugins loaded by
     * blojsom. This method also calls the {@link BlojsomFetcher#destroy} method for the fetcher loaded
     * by blojsom.
     */
    public void destroy() {
        super.destroy();

        // Destroy the fetcher
        try {
            _fetcher.destroy();
        } catch (BlojsomFetcherException e) {
            _logger.error(e);
        }

        // Destroy the plugins
        Iterator pluginIteratorIterator = _plugins.keySet().iterator();
        while (pluginIteratorIterator.hasNext()) {
            String pluginName = (String) pluginIteratorIterator.next();
            BlojsomPlugin plugin = (BlojsomPlugin) _plugins.get(pluginName);
            try {
                plugin.destroy();
                _logger.debug("Removed blojsom plugin: " + plugin.getClass().getName());
            } catch (BlojsomPluginException e) {
                _logger.error(e);
            }
        }

        _logger.debug("blojsom destroyed");
    }
}
