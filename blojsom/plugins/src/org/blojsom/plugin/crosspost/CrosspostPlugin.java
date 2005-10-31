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
package org.blojsom.plugin.crosspost;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.apache.xmlrpc.XmlRpcClient;
import org.apache.xmlrpc.XmlRpcException;
import org.blojsom.blog.BlogCategory;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.event.BlojsomEvent;
import org.blojsom.event.BlojsomListener;
import org.blojsom.plugin.BlojsomPlugin;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.plugin.admin.event.AddBlogEntryEvent;
import org.blojsom.plugin.admin.event.BlogEntryEvent;
import org.blojsom.plugin.crosspost.beans.Destination;
import org.blojsom.util.BlojsomConstants;
import org.blojsom.util.BlojsomProperties;
import org.blojsom.util.BlojsomUtils;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.IOException;
import java.io.InputStream;
import java.util.HashMap;
import java.util.Map;
import java.util.Properties;
import java.util.Vector;

/**
 * CrosspostPlugin
 *
 * @author Mark Lussier
 * @version $Id: CrosspostPlugin.java,v 1.1.2.1 2005/07/21 04:30:29 johnan Exp $
 * @since blojsom 2.23
 */
public class CrosspostPlugin implements BlojsomPlugin, BlojsomListener, BlojsomConstants {

    private Log _logger = LogFactory.getLog(CrosspostPlugin.class);

    /**
     * Crosspost Plugin configuration parameter for web.xml
     */
    public static final String PLUGIN_CROSSPOST_CONFIGURATION_IP = "plugin-crosspost";

    /**
     * Crosspost Category Meta Data Key
     */
    private static final String PLUGIN_CROSSPOST_METADATA_KEY = "blojsom.crosspost";

    private ServletConfig _servletConfig;
    private BlojsomConfiguration _blojsomConfiguration;

    /**
     * Initialize this plugin. This method only called when the plugin is instantiated.
     *
     * @param servletConfig        Servlet config object for the plugin to retrieve any initialization parameters
     * @param blojsomConfiguration {@link org.blojsom.blog.BlojsomConfiguration} information
     * @throws org.blojsom.plugin.BlojsomPluginException
     *          If there is an error initializing the plugin
     */
    public void init(ServletConfig servletConfig, BlojsomConfiguration blojsomConfiguration) throws BlojsomPluginException {
        _servletConfig = servletConfig;
        _blojsomConfiguration = blojsomConfiguration;
        _blojsomConfiguration.getEventBroadcaster().addListener(this);

        _logger.debug("Initialized crosspost plugin.");
    }

    /**
     * Process the blog entries
     *
     * @param httpServletRequest  Request
     * @param httpServletResponse Response
     * @param user                {@link org.blojsom.blog.BlogUser} instance
     * @param context             Context
     * @param entries             Blog entries retrieved for the particular request
     * @return Modified set of blog entries
     * @throws org.blojsom.plugin.BlojsomPluginException
     *          If there is an error processing the blog entries
     */
    public BlogEntry[] process(HttpServletRequest httpServletRequest, HttpServletResponse httpServletResponse, BlogUser user, Map context, BlogEntry[] entries) throws BlojsomPluginException {
        return entries;
    }

    /**
     * Perform any cleanup for the plugin. Called after {@link #process}.
     *
     * @throws org.blojsom.plugin.BlojsomPluginException
     *          If there is an error performing cleanup for this plugin
     */
    public void cleanup() throws BlojsomPluginException {
    }

    /**
     * Called when BlojsomServlet is taken out of service
     *
     * @throws org.blojsom.plugin.BlojsomPluginException
     *          If there is an error in finalizing this plugin
     */
    public void destroy() throws BlojsomPluginException {
    }

    /**
     * Retrieve a map of {@link Destination} objects key'd by destination ID
     *
     * @param blogUser {@link BlogUser}
     */
    private Map processConfiguration(BlogUser blogUser) {
        Map destinationMap = new HashMap();
        String xpostConfiguration = _servletConfig.getInitParameter(PLUGIN_CROSSPOST_CONFIGURATION_IP);
        String configurationFile = _blojsomConfiguration.getBaseConfigurationDirectory() + blogUser.getId() + "/" + xpostConfiguration;
        Properties userXpost = new BlojsomProperties();

        try {
            InputStream is = _servletConfig.getServletContext().getResourceAsStream(configurationFile);

            if (is != null) {
                userXpost.load(is);
                is.close();
            } else {
                _logger.info("No Crosspost configuration file for blog: " + blogUser.getId());

                return destinationMap;
            }
        } catch (IOException e) {
            _logger.error(e);

            return destinationMap;
        }

        Map configurationMap = BlojsomUtils.propertiesToMap(userXpost);
        String destinationChain = (String) configurationMap.get(CrosspostConstants.XPOST_TAG_DESTINATIONS);
        if (destinationChain != null) {
            String[] destinations = BlojsomUtils.parseCommaList(destinationChain);

            if (destinations != null) {
                destinationMap = new HashMap(destinations.length);

                for (int x = 0; x < destinations.length; x++) {
                    String id = destinations[x];
                    Destination destination = new Destination();
                    String title = (String) configurationMap.get(id + CrosspostConstants.XPOST_TAG_TITLE);
                    destination.setTitle(title);

                    String url = (String) configurationMap.get(id + CrosspostConstants.XPOST_TAG_URL);
                    destination.setUrl(url);

                    String userid = (String) configurationMap.get(id + CrosspostConstants.XPOST_TAG_USERID);
                    destination.setUserId(userid);

                    String password = (String) configurationMap.get(id + CrosspostConstants.XPOST_TAG_PASSWORD);
                    destination.setPassword(password);

                    String category = (String) configurationMap.get(id + CrosspostConstants.XPOST_TAG_CATEGORY);
                    destination.setCategory(category);

                    String blogid = (String) configurationMap.get(id + CrosspostConstants.XPOST_TAG_BLOGID);
                    destination.setBlogId(blogid);

                    int apiType = CrosspostConstants.API_BLOGGER;
                    String type = (String) configurationMap.get(id + CrosspostConstants.XPOST_TAG_TYPE);

                    if (type.toLowerCase().equalsIgnoreCase(CrosspostConstants.TYPE_METAWEBLOG)) {
                        apiType = CrosspostConstants.API_METAWEBLOG;
                    }

                    destination.setApiType(apiType);

                    _logger.info("Adding CrossPost Destination [" + destination.getTitle() + "] @ [" + destination.getUrl() + "]");

                    destinationMap.put(id, destination);
                }
            }
        }

        return destinationMap;
    }

    /**
     * Handle an event broadcast from another component
     *
     * @param event {@link org.blojsom.event.BlojsomEvent} to be handled
     */
    public void handleEvent(BlojsomEvent event) {
        if (event instanceof BlogEntryEvent) {
            BlogEntryEvent entryEvent = (BlogEntryEvent) event;

            if (entryEvent instanceof AddBlogEntryEvent) {
                AddBlogEntryEvent addEntryEvent = (AddBlogEntryEvent) entryEvent;
                BlogCategory blogCategory = addEntryEvent.getBlogEntry().getBlogCategory();
                Map categoryMetaData = blogCategory.getMetaData();

                if (categoryMetaData != null) {
                    String xpost = (String) categoryMetaData.get(PLUGIN_CROSSPOST_METADATA_KEY);

                    if (xpost != null) {
                        String[] destinations = BlojsomUtils.parseCommaList(xpost);

                        if (destinations != null) {
                            Map destinationsMap = processConfiguration(entryEvent.getBlogUser());

                            for (int x = 0; x < destinations.length; x++) {
                                String destination = destinations[x];
                                _logger.info("Crossposting to " + destination);
                                if (destinationsMap.containsKey(destination)) {
                                    Destination d = (Destination) destinationsMap.get(destination);
                                    publishCrosspost(addEntryEvent.getBlogEntry(), d);
                                }
                            }
                        }
                    }
                } else {
                    _logger.info("No category metadata for " + blogCategory.getCategory());
                }
            }
        }
    }

    /**
     * Process an event from another component
     *
     * @param event {@link BlojsomEvent} to be handled
     * @since blojsom 2.24
     */
    public void processEvent(BlojsomEvent event) {
    }

    /**
     * Perform the crosspost operation
     *
     * @param entry {@link BlogEntry}
     * @param destination {@link Destination} to post entry to
     */
    private void publishCrosspost(BlogEntry entry, Destination destination) {
        String method = null;
        Vector param = new Vector();
        param.add("0");
        param.add(destination.getCategory());
        param.add(destination.getUserId());
        param.add(destination.getPassword());
        if (destination.getApiType() == CrosspostConstants.API_BLOGGER) {
            method = "blogger.newPost";
            param.add(entry.getTitle() + "\n" + entry.getDescription());
        } else {
            method = "metaWeblog.newPost";
            param.add(entry.getTitle() + "\n" + entry.getDescription());
        }
        param.add(Boolean.TRUE);

        String url = destination.getUrl();
        try {
            XmlRpcClient xpostClient = new XmlRpcClient(url);
            xpostClient.execute(method, param);
        } catch (XmlRpcException e) {
            _logger.error(e);
        } catch (IOException e) {
            _logger.error(e.getMessage(), e);
        }
    }
}

