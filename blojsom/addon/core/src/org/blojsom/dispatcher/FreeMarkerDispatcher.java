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
package org.blojsom.dispatcher;

import freemarker.ext.beans.BeansWrapper;
import freemarker.template.Configuration;
import freemarker.template.Template;
import freemarker.template.TemplateException;
import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.BlojsomException;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.util.BlojsomUtils;

import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.File;
import java.io.IOException;
import java.io.Writer;
import java.util.Map;
import java.util.Properties;

/**
 * FreeMarkerDispatcher
 * 
 * @author czarneckid
 * @version $Id: FreeMarkerDispatcher.java,v 1.1 2004/11/18 22:22:34 johnan Exp $
 * @since blojsom 2.05
 */
public class FreeMarkerDispatcher implements BlojsomDispatcher {

    private Log _logger = LogFactory.getLog(FreeMarkerDispatcher.class);

    private final static String FREEMARKER_PROPERTIES_IP = "freemarker-properties";

    private String _installationDirectory;
    private String _baseConfigurationDirectory;
    private String _templatesDirectory;
    private Properties _freemarkerProperties;

    /**
     * Default constructor.
     */
    public FreeMarkerDispatcher() {
    }

    /**
     * Initialization method for blojsom dispatchers
     * 
     * @param servletConfig        ServletConfig for obtaining any initialization parameters
     * @param blojsomConfiguration BlojsomConfiguration for blojsom-specific configuration information
     * @throws org.blojsom.BlojsomException If there is an error initializing the dispatcher
     */
    public void init(ServletConfig servletConfig, BlojsomConfiguration blojsomConfiguration) throws BlojsomException {
        _baseConfigurationDirectory = blojsomConfiguration.getBaseConfigurationDirectory();
        _installationDirectory = blojsomConfiguration.getInstallationDirectory();
        _templatesDirectory = blojsomConfiguration.getTemplatesDirectory();

        _freemarkerProperties = BlojsomUtils.loadProperties(servletConfig, FREEMARKER_PROPERTIES_IP, true);
        if (_freemarkerProperties.isEmpty()) {
            _freemarkerProperties = null;
        }

        _logger.debug("Initialized FreeMarker dispatcher");
    }

    /**
     * Return a path appropriate for loading FreeMarker templates
     * 
     * @param userId User ID
     * @return blojsom installation directory + base configuration directory + user id + templates directory
     */
    private String getTemplatePath(String userId) {
        StringBuffer templatePath = new StringBuffer();
        templatePath.append(_installationDirectory);
        templatePath.append(BlojsomUtils.removeInitialSlash(_baseConfigurationDirectory));
        templatePath.append(userId).append("/");
        templatePath.append(BlojsomUtils.removeInitialSlash(_templatesDirectory));

        return templatePath.toString();
    }

    /**
     * Dispatch a request and response. A context map is provided for the BlojsomServlet to pass
     * any required information for use by the dispatcher. The dispatcher is also
     * provided with the template for the requested flavor along with the content type for the
     * specific flavor.
     * 
     * @param httpServletRequest  Request
     * @param httpServletResponse Response
     * @param user                {@link org.blojsom.blog.BlogUser} instance
     * @param context             Context map
     * @param flavorTemplate      Template to dispatch to for the requested flavor
     * @param flavorContentType   Content type for the requested flavor
     * @throws java.io.IOException            If there is an exception during IO
     * @throws javax.servlet.ServletException If there is an exception in dispatching the request
     */
    public void dispatch(HttpServletRequest httpServletRequest, HttpServletResponse httpServletResponse, BlogUser user, Map context, String flavorTemplate, String flavorContentType) throws IOException, ServletException {
        httpServletResponse.setContentType(flavorContentType);

        String templatePath = getTemplatePath(user.getId());

        // Configure FreeMarker with the loaded properties
        Configuration freemarkerConfiguration = Configuration.getDefaultConfiguration();
        if (_freemarkerProperties != null) {
            try {
                freemarkerConfiguration.setSettings(_freemarkerProperties);
            } catch (TemplateException e) {
                _logger.error(e);
            }
        }
        freemarkerConfiguration.setDirectoryForTemplateLoading(new File(templatePath));

        BeansWrapper wrapper = new BeansWrapper();
        wrapper.setExposureLevel(BeansWrapper.EXPOSE_PROPERTIES_ONLY);
        wrapper.setSimpleMapWrapper(true);
        freemarkerConfiguration.setObjectWrapper(wrapper);

        Writer responseWriter = httpServletResponse.getWriter();
        String flavorTemplateForPage = null;

        if (BlojsomUtils.getRequestValue(PAGE_PARAM, httpServletRequest) != null) {
            flavorTemplateForPage = BlojsomUtils.getTemplateForPage(flavorTemplate, BlojsomUtils.getRequestValue(PAGE_PARAM, httpServletRequest));
            _logger.debug("Retrieved template for page: " + flavorTemplateForPage);
        }

        if (flavorTemplateForPage != null) {
            // Try and look for the flavor page template for the individual user
            try {
                Template template = freemarkerConfiguration.getTemplate(flavorTemplateForPage);
                template.setEncoding(UTF8);
                template.process(context, responseWriter);
            } catch (Exception e) {
                _logger.error(e);
                return;
            }

            _logger.debug("Dispatched to flavor page template: " + flavorTemplateForPage);
        } else {
            // Otherwise, fallback and look for the flavor template for the individual user
            try {
                Template template = freemarkerConfiguration.getTemplate(flavorTemplate);
                template.setEncoding(UTF8);
                template.process(context, responseWriter);
            } catch (Exception e) {
                _logger.error(e);
                return;
            }

            _logger.debug("Dispatched to flavor template: " + flavorTemplate);
        }

        responseWriter.flush();
    }
}
