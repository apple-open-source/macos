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
package org.blojsom.dispatcher;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.apache.velocity.VelocityContext;
import org.apache.velocity.app.Velocity;
import org.apache.velocity.app.VelocityEngine;
import org.apache.velocity.exception.MethodInvocationException;
import org.apache.velocity.exception.ParseErrorException;
import org.apache.velocity.exception.ResourceNotFoundException;
import org.apache.velocity.util.EnumerationIterator;
import org.blojsom.BlojsomException;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.util.BlojsomUtils;

import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.http.HttpSession;
import java.io.IOException;
import java.io.InputStream;
import java.io.StringWriter;
import java.io.Writer;
import java.util.Map;
import java.util.Properties;

/**
 * VelocityDispatcher
 *
 * @author David Czarnecki
 * @version $Id: VelocityDispatcher.java,v 1.3.2.1 2005/07/21 14:11:02 johnan Exp $
 */
public class VelocityDispatcher implements BlojsomDispatcher {

    private static final String BLOG_VELOCITY_PROPERTIES_IP = "velocity-properties";
    private static final String BLOJSOM_RENDER_TOOL = "BLOJSOM_RENDER_TOOL";

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
     * Populate the Velocity context with the request and session attributes
     *
     * @param httpServletRequest Request
     * @param context            Context
     */
    protected void populateVelocityContext(HttpServletRequest httpServletRequest, Map context) {
        EnumerationIterator iterator = new EnumerationIterator(httpServletRequest.getAttributeNames());
        while (iterator.hasNext()) {
            Object key = iterator.next();
            Object value = httpServletRequest.getAttribute(key.toString());
            context.put(key, value);
        }

        HttpSession httpSession = httpServletRequest.getSession();
        if (httpSession != null) {
            iterator = new EnumerationIterator(httpSession.getAttributeNames());
            while (iterator.hasNext()) {
                Object key = iterator.next();
                Object value = httpSession.getAttribute(key.toString());
                context.put(key, value);
            }
        }
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
            updatedVelocityProperties = null;
        } catch (Exception e) {
            _logger.error(e);
            return;
        }

        Writer responseWriter = httpServletResponse.getWriter();
        String flavorTemplateForPage = null;
        String pageParameter = BlojsomUtils.getRequestValue(PAGE_PARAM, httpServletRequest, true);

        if (pageParameter != null) {
            flavorTemplateForPage = BlojsomUtils.getTemplateForPage(flavorTemplate, pageParameter);
            _logger.debug("Retrieved template for page: " + flavorTemplateForPage);
        }

        // Setup the VelocityContext
        populateVelocityContext(httpServletRequest, context);
        VelocityContext velocityContext = new VelocityContext(context);
        velocityContext.put(BLOJSOM_RENDER_TOOL, new BlojsomRenderTool(velocityEngine, velocityContext));

        if (flavorTemplateForPage != null) {
            // Try and look for the flavor page template for the individual user
            if (!velocityEngine.templateExists(flavorTemplateForPage)) {
                _logger.error("Could not find flavor page template for user: " + flavorTemplateForPage);
                responseWriter.flush();
                velocityEngine = null;

                return;
            } else {
                try {
                    velocityEngine.mergeTemplate(flavorTemplateForPage, UTF8, velocityContext, responseWriter);
                } catch (Exception e) {
                    _logger.error(e);
                    responseWriter.flush();
                    velocityEngine = null;

                    return;
                }
            }

            _logger.debug("Dispatched to flavor page template: " + flavorTemplateForPage);
        } else {
            // Otherwise, fallback and look for the flavor template for the individual user
            if (!velocityEngine.templateExists(flavorTemplate)) {
                _logger.error("Could not find flavor template for user: " + flavorTemplate);
                responseWriter.flush();
                velocityEngine = null;

                return;
            } else {
                try {
                    velocityEngine.mergeTemplate(flavorTemplate, UTF8, velocityContext, responseWriter);
                } catch (Exception e) {
                    _logger.error(e);
                    responseWriter.flush();
                    velocityEngine = null;

                    return;
                }
            }

            _logger.debug("Dispatched to flavor template: " + flavorTemplate);
        }

        responseWriter.flush();
        velocityEngine = null;
    }

    /**
     * Blojsom render tool mimics the functionality of the Velocity render tool to parse VTL markup added to a
     * template
     *
     * @since blojsom 2.24
     */
    public class BlojsomRenderTool {

        private static final String LOG_TAG = "BlojsomRenderTool";

        private VelocityEngine _velocityEngine;
        private VelocityContext _velocityContext;

        /**
         * Create a new instance of the render tool
         *
         * @param velocityEngine  {@link VelocityEngine}
         * @param velocityContext {@link VelocityContext}
         */
        public BlojsomRenderTool(VelocityEngine velocityEngine, VelocityContext velocityContext) {
            _velocityEngine = velocityEngine;
            _velocityContext = velocityContext;
        }

        /**
         * Evaluate a string containing VTL markup
         *
         * @param template VTL markup
         * @return Processed VTL or <code>null</code> if an error in evaluation
         */
        public String evaluate(String template) {
            if (BlojsomUtils.checkNullOrBlank(template)) {
                return null;
            }

            StringWriter sw = new StringWriter();
            boolean success = false;

            try {
                if (_velocityEngine == null) {
                    success = Velocity.evaluate(_velocityContext, sw, LOG_TAG, template);
                } else {
                    success = _velocityEngine.evaluate(_velocityContext, sw, LOG_TAG, template);
                }
            } catch (ParseErrorException e) {
                _logger.error(e);
            } catch (MethodInvocationException e) {
                _logger.error(e);
            } catch (ResourceNotFoundException e) {
                _logger.error(e);
            } catch (IOException e) {
                _logger.error(e);
            }

            if (success) {
                return sw.toString();
            }

            return null;
        }
    }
}