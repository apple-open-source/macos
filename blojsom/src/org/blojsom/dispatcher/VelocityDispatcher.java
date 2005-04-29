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
package org.blojsom.dispatcher;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.apache.velocity.VelocityContext;
import org.apache.velocity.app.VelocityEngine;
import org.blojsom.BlojsomException;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.util.BlojsomUtils;

import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.IOException;
import java.io.InputStream;
import java.io.Writer;
import java.io.File;
import java.util.Map;
import java.util.Properties;

/**
 * VelocityDispatcher
 *
 * @author David Czarnecki
 * @version $Id: VelocityDispatcher.java,v 1.3 2004/08/27 01:13:55 whitmore Exp $
 */
public class VelocityDispatcher implements BlojsomDispatcher {

    private static final String BLOG_VELOCITY_PROPERTIES_IP = "velocity-properties";

    private Log _logger = LogFactory.getLog(VelocityDispatcher.class);
    private String _installationDirectory;
    private String _baseConfigurationDirectory;
    private String _templatesDirectory;
    private Properties _velocityProperties;

    /**
     * Create a new VelocityDispatcher
     */
    public VelocityDispatcher() {
    }

    /**
     * Initialization method for blojsom dispatchers
     *
     * @param servletConfig        ServletConfig for obtaining any initialization parameters
     * @param blojsomConfiguration BlojsomConfiguration for blojsom-specific configuration information
     * @throws BlojsomException If there is an error initializing the dispatcher
     */
    public void init(ServletConfig servletConfig, BlojsomConfiguration blojsomConfiguration) throws BlojsomException {
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

        _logger.debug("Initialized Velocity dispatcher");
    }

    /**
     * Return a path appropriate for the Velocity file resource loader
     *
     * @param userId User ID
     * @return blojsom installation directory + base configuration directory + user id + templates directory
     */
    private String getVelocityFileLoaderPath(String userId) {
        StringBuffer fileLoaderPath = new StringBuffer();
        fileLoaderPath.append(_installationDirectory);
        fileLoaderPath.append(BlojsomUtils.removeInitialSlash(_baseConfigurationDirectory));
        fileLoaderPath.append(userId).append("/");
        fileLoaderPath.append(BlojsomUtils.removeInitialSlash(_templatesDirectory));
		
		// check for the existence of this templates folder
		File fileLoaderDir = new File(fileLoaderPath.toString());
		
		_logger.debug("looking for " + fileLoaderPath.toString());
		
		// if it doesn't exist, use the base config dir's templates folder
		if ( ! fileLoaderDir.exists() )
		{
			fileLoaderPath = new StringBuffer();
			fileLoaderPath.append(_installationDirectory);
			fileLoaderPath.append(BlojsomUtils.removeInitialSlash(_templatesDirectory));
			_logger.debug("looking for " + fileLoaderPath.toString());
		}

        return fileLoaderPath.toString();
    }

    /**
     * Dispatch a request and response. A context map is provided for the BlojsomServlet to pass
     * any required information for use by the dispatcher. The dispatcher is also
     * provided with the template for the requested flavor along with the content type for the
     * specific flavor.
     *
     * @param httpServletRequest  Request
     * @param httpServletResponse Response
     * @param user                {@link BlogUser} instance
     * @param context             Context map
     * @param flavorTemplate      Template to dispatch to for the requested flavor
     * @param flavorContentType   Content type for the requested flavor
     * @throws IOException      If there is an exception during IO
     * @throws ServletException If there is an exception in dispatching the request
     */
    public void dispatch(HttpServletRequest httpServletRequest,
                         HttpServletResponse httpServletResponse,
                         BlogUser user,
                         Map context,
                         String flavorTemplate,
                         String flavorContentType)
            throws IOException, ServletException {
        httpServletResponse.setContentType(flavorContentType);

        // Create the Velocity Engine
        VelocityEngine velocityEngine = new VelocityEngine();

        try {
            Properties updatedVelocityProperties = (Properties) _velocityProperties.clone();
            updatedVelocityProperties.setProperty(VelocityEngine.FILE_RESOURCE_LOADER_PATH, getVelocityFileLoaderPath(user.getId()));
            velocityEngine.init(updatedVelocityProperties);
        } catch (Exception e) {
            _logger.error(e);
            return;
        }

        Writer responseWriter = httpServletResponse.getWriter();
        String flavorTemplateForPage = null;

        if (BlojsomUtils.getRequestValue(PAGE_PARAM, httpServletRequest) != null) {
            flavorTemplateForPage = BlojsomUtils.getTemplateForPage(flavorTemplate, BlojsomUtils.getRequestValue(PAGE_PARAM, httpServletRequest));
            _logger.debug("Retrieved template for page: " + flavorTemplateForPage);
        }

        // Setup the VelocityContext
        VelocityContext velocityContext = new VelocityContext(context);

        if (flavorTemplateForPage != null) {
            // Try and look for the flavor page template for the individual user
            if (!velocityEngine.templateExists(flavorTemplateForPage)) {
                _logger.error("Could not find flavor page template for user: " + flavorTemplateForPage);
                return;
            } else {
                try {
                    velocityEngine.mergeTemplate(flavorTemplateForPage, UTF8, velocityContext, responseWriter);
                } catch (Exception e) {
                    _logger.error(e);
                    return;
                }
            }
            _logger.debug("Dispatched to flavor page template: " + flavorTemplateForPage);
        } else {
            // Otherwise, fallback and look for the flavor template for the individual user
            if (!velocityEngine.templateExists(flavorTemplate)) {
                _logger.error("Could not find flavor template for user: " + flavorTemplate);
                return;
            } else {
                try {
                    velocityEngine.mergeTemplate(flavorTemplate, UTF8, velocityContext, responseWriter);
                } catch (Exception e) {
                    _logger.error(e);
                    return;
                }
            }
            _logger.debug("Dispatched to flavor template: " + flavorTemplate);
        }

        responseWriter.flush();
    }
}