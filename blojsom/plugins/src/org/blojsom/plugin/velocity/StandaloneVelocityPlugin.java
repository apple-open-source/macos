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
package org.blojsom.plugin.velocity;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.apache.velocity.VelocityContext;
import org.apache.velocity.app.VelocityEngine;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.plugin.BlojsomPlugin;
import org.blojsom.util.BlojsomUtils;
import org.blojsom.util.BlojsomConstants;

import javax.servlet.ServletConfig;
import java.io.InputStream;
import java.io.StringWriter;
import java.util.Map;
import java.util.Properties;

/**
 * StandalongVelocityPlugin
 *
 * @author David Czarnecki
 * @since blojsom 2.25
 * @version $Id: StandaloneVelocityPlugin.java,v 1.1.2.1 2005/07/21 04:30:43 johnan Exp $
 */
public abstract class StandaloneVelocityPlugin implements BlojsomPlugin, BlojsomConstants {

    protected Log _logger = LogFactory.getLog(StandaloneVelocityPlugin.class);

    private final static String BLOG_VELOCITY_PROPERTIES_IP = "velocity-properties";

    protected String _installationDirectory;
    protected String _baseConfigurationDirectory;
    protected String _templatesDirectory;
    protected Properties _velocityProperties;

    /**
     * Initialize this plugin. This method only called when the plugin is instantiated.
     *
     * @param servletConfig        Servlet config object for the plugin to retrieve any initialization parameters
     * @param blojsomConfiguration {@link org.blojsom.blog.BlojsomConfiguration} information
     * @throws org.blojsom.plugin.BlojsomPluginException
     *          If there is an error initializing the plugin
     */
    public void init(ServletConfig servletConfig, BlojsomConfiguration blojsomConfiguration) throws BlojsomPluginException {
        _baseConfigurationDirectory = blojsomConfiguration.getBaseConfigurationDirectory();
        _installationDirectory = blojsomConfiguration.getInstallationDirectory();
        _templatesDirectory = blojsomConfiguration.getTemplatesDirectory();

        _logger.debug("Using templates directory: " + _templatesDirectory);

        String velocityConfiguration = servletConfig.getInitParameter(BLOG_VELOCITY_PROPERTIES_IP);
        _velocityProperties = new Properties();
        InputStream is = servletConfig.getServletContext().getResourceAsStream(velocityConfiguration);

        try {
            _velocityProperties.load(is);
            is.close();
        } catch (Exception e) {
            _logger.error(e);
        }

        _logger.debug("Initialized Standalone Velocity plugin");
    }

    /**
     * Return a path appropriate for the Velocity file resource loader
     *
     * @param userId User ID
     * @return blojsom installation directory + base configuration directory + user id + templates directory
     */
    protected String getVelocityFileLoaderPath(String userId) {
        StringBuffer fileLoaderPath = new StringBuffer();
        fileLoaderPath.append(_installationDirectory);
        fileLoaderPath.append(BlojsomUtils.removeInitialSlash(_baseConfigurationDirectory));
        fileLoaderPath.append(userId).append("/");
        fileLoaderPath.append(BlojsomUtils.removeInitialSlash(_templatesDirectory));
        fileLoaderPath.append(", ");
        fileLoaderPath.append(_installationDirectory);
        fileLoaderPath.append(BlojsomUtils.removeInitialSlash(_baseConfigurationDirectory));
        fileLoaderPath.append(BlojsomUtils.removeInitialSlash(_templatesDirectory));

        return fileLoaderPath.toString();
    }

    /**
     * Merge a given template for the user with the appropriate context
     *
     * @param template Template
     * @param user {@link BlogUser} information
     * @param context Context with objects for use in the template
     * @return Merged template or <code>null</code> if there was an error setting properties, loading the template, or merging
     * the template
     */
    protected String mergeTemplate(String template, BlogUser user, Map context) {
        // Create the Velocity Engine
        VelocityEngine velocityEngine = new VelocityEngine();

        try {
            Properties updatedVelocityProperties = (Properties) _velocityProperties.clone();
            updatedVelocityProperties.setProperty(VelocityEngine.FILE_RESOURCE_LOADER_PATH, getVelocityFileLoaderPath(user.getId()));
            velocityEngine.init(updatedVelocityProperties);
        } catch (Exception e) {
            _logger.error(e);

            return null;
        }

        StringWriter writer = new StringWriter();

        // Setup the VelocityContext
        VelocityContext velocityContext = new VelocityContext(context);

        if (!velocityEngine.templateExists(template)) {
            _logger.error("Could not find template for user: " + template);

            return null;
        } else {
            try {
                velocityEngine.mergeTemplate(template, UTF8, velocityContext, writer);
            } catch (Exception e) {
                _logger.error(e);

                return null;
            }
        }

        _logger.debug("Merged template: " + template);

        return writer.toString();
    }
}