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
import org.blojsom.BlojsomException;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.util.BlojsomUtils;

import javax.servlet.ServletConfig;
import javax.servlet.ServletContext;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.IOException;
import java.util.Iterator;
import java.util.Map;

/**
 * JSPDispatcher
 *
 * @author David Czarnecki
 * @version $Id: JSPDispatcher.java,v 1.2 2004/08/27 01:13:55 whitmore Exp $
 */
public class JSPDispatcher implements BlojsomDispatcher {

    private Log _logger = LogFactory.getLog(JSPDispatcher.class);

    private ServletContext _context;
    private String _templatesDirectory;
    private String _baseConfigurationDirectory;

    /**
     * Create a new JSPDispatcher
     */
    public JSPDispatcher() {
    }

    /**
     * Initialization method for blojsom dispatchers
     *
     * @param servletConfig ServletConfig for obtaining any initialization parameters
     * @param blojsomConfiguration BlojsomConfiguration for blojsom-specific configuration information
     * @throws BlojsomException If there is an error initializing the dispatcher
     */
    public void init(ServletConfig servletConfig, BlojsomConfiguration blojsomConfiguration) throws BlojsomException {
        _context = servletConfig.getServletContext();
        _baseConfigurationDirectory = blojsomConfiguration.getBaseConfigurationDirectory();
        _templatesDirectory = blojsomConfiguration.getTemplatesDirectory();

        _logger.debug("Using templates directory: " + _templatesDirectory);

        _logger.debug("Initialized JSP dispatcher");
    }

    /**
     * Dispatch a request and response. A context map is provided for the BlojsomServlet to pass
     * any required information for use by the dispatcher. The dispatcher is also
     * provided with the template for the requested flavor along with the content type for the
     * specific flavor.
     *
     * @param httpServletRequest Request
     * @param httpServletResponse Response
     * @param user {@link BlogUser} instance
     * @param context Context map
     * @param flavorTemplate Template to dispatch to for the requested flavor
     * @param flavorContentType Content type for the requested flavor
     * @throws IOException If there is an exception during IO
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

        if (!flavorTemplate.startsWith("/")) {
            flavorTemplate = '/' + flavorTemplate;
        }

        String flavorTemplateForPage = null;
        if (BlojsomUtils.getRequestValue(PAGE_PARAM, httpServletRequest) != null) {
            flavorTemplateForPage = BlojsomUtils.getTemplateForPage(flavorTemplate, BlojsomUtils.getRequestValue(PAGE_PARAM, httpServletRequest));
            _logger.debug("Retrieved template for page: " + flavorTemplateForPage);
        }

        // Populate the request with context attributes from the blog
        Iterator contextIterator = context.keySet().iterator();
        while (contextIterator.hasNext()) {
            String contextKey = (String) contextIterator.next();
            httpServletRequest.setAttribute(contextKey, context.get(contextKey));
        }

        // Try and look for the original flavor template with page for the individual user
        if (flavorTemplateForPage != null) {
            String templateToLoad = _baseConfigurationDirectory + user.getId() + _templatesDirectory + BlojsomUtils.removeInitialSlash(flavorTemplateForPage);
            if (_context.getResource(templateToLoad) != null) {
                httpServletRequest.getRequestDispatcher(templateToLoad).forward(httpServletRequest, httpServletResponse);
                httpServletResponse.getWriter().flush();
                _logger.debug("Dispatched to flavor page template for user: " + templateToLoad);
                return;
            } else {
                templateToLoad = _baseConfigurationDirectory + BlojsomUtils.removeInitialSlash(_templatesDirectory) + BlojsomUtils.removeInitialSlash(flavorTemplateForPage);
                if (_context.getResource(templateToLoad) != null) {
                    // Otherwise, fallback and look for the flavor template with page without including any user information
                    httpServletRequest.getRequestDispatcher(templateToLoad).forward(httpServletRequest, httpServletResponse);
                    httpServletResponse.getWriter().flush();
                    _logger.debug("Dispatched to flavor page template: " + templateToLoad);
                    return;
                } else {
                    _logger.error("Unable to dispatch to flavor page template: " + templateToLoad);
                }
            }
        } else {
            // Otherwise, fallback and look for the flavor template for the individual user
            String templateToLoad = _baseConfigurationDirectory + user.getId() + _templatesDirectory + BlojsomUtils.removeInitialSlash(flavorTemplate);
            if (_context.getResource(templateToLoad) != null) {
                httpServletRequest.getRequestDispatcher(templateToLoad).forward(httpServletRequest, httpServletResponse);
                httpServletResponse.getWriter().flush();
                _logger.debug("Dispatched to flavor template for user: " + templateToLoad);
                return;
            } else {
                templateToLoad = _baseConfigurationDirectory + BlojsomUtils.removeInitialSlash(_templatesDirectory) + BlojsomUtils.removeInitialSlash(flavorTemplate);
                if (_context.getResource(templateToLoad) != null) {
                    // Otherwise, fallback and look for the flavor template without including any user information
                    httpServletRequest.getRequestDispatcher(templateToLoad).forward(httpServletRequest, httpServletResponse);
                    httpServletResponse.getWriter().flush();
                    _logger.debug("Dispatched to flavor template: " + templateToLoad);
                    return;
                } else {
                    _logger.error("Unable to dispatch to flavor template: " + templateToLoad);
                }
            }
        }
    }
}