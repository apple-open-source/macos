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
package org.blojsom.servlet;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.BlojsomException;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.blog.BlojsomConfigurationException;
import org.blojsom.fetcher.BlojsomFetcher;
import org.blojsom.fetcher.BlojsomFetcherException;
import org.blojsom.util.BlojsomConstants;
import org.blojsom.util.BlojsomUtils;

import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import java.util.Properties;

/**
 * BlojsomBaseServlet
 *
 * @author David Czarnecki
 * @since blojsom 2.0
 * @version $Id: BlojsomBaseServlet.java,v 1.2.2.1 2005/07/21 14:11:04 johnan Exp $
 */
public class BlojsomBaseServlet extends HttpServlet implements BlojsomConstants {

    private Log _logger = LogFactory.getLog(BlojsomBaseServlet.class);

    protected String _baseConfigurationDirectory;
    protected BlojsomFetcher _fetcher;
    protected BlojsomConfiguration _blojsomConfiguration;
    protected ServletConfig _servletConfig;

    /**
     * Servlet initialization
     *
     * @param servletConfig {@link ServletConfig}
     * @throws ServletException If there is an error during initialization
     */
    public void init(ServletConfig servletConfig) throws ServletException {
        super.init(servletConfig);

        _servletConfig = servletConfig;
    }

    /**
     * Configure the {@link BlojsomFetcher} that will be used to fetch categories and
     * entries
     *
     * @param servletConfig Servlet configuration information
     * @param blojsomConfiguration blojsom properties
     * @throws javax.servlet.ServletException If the {@link BlojsomFetcher} class could not be loaded and/or initialized
     */
    protected void configureFetcher(ServletConfig servletConfig, BlojsomConfiguration blojsomConfiguration) throws ServletException {
        String fetcherClassName = blojsomConfiguration.getFetcherClass();
        try {
            Class fetcherClass = Class.forName(fetcherClassName);
            _fetcher = (BlojsomFetcher) fetcherClass.newInstance();
            _fetcher.init(servletConfig, blojsomConfiguration);
            _logger.info("Added blojsom fetcher: " + fetcherClassName);
        } catch (ClassNotFoundException e) {
            _logger.error(e);
            throw new ServletException(e);
        } catch (InstantiationException e) {
            _logger.error(e);
            throw new ServletException(e);
        } catch (IllegalAccessException e) {
            _logger.error(e);
            throw new ServletException(e);
        } catch (BlojsomFetcherException e) {
            _logger.error(e);
            throw new ServletException(e);
        }
    }

    /**
     * Configure the global configuration information and initialize the users for this blog
     *
     * @param servletConfig Servlet configuration information
     */
    protected void configureBlojsom(ServletConfig servletConfig) throws ServletException {
        try {
            // Configure blojsom's properties
            Properties configurationProperties = BlojsomUtils.loadProperties(servletConfig, BLOJSOM_CONFIGURATION_IP, true, true);

            _blojsomConfiguration = new BlojsomConfiguration(servletConfig, BlojsomUtils.propertiesToMap(configurationProperties));
            _baseConfigurationDirectory = _blojsomConfiguration.getBaseConfigurationDirectory();

            // Configure the fetcher for use by this blog
            configureFetcher(servletConfig, _blojsomConfiguration);
        } catch (BlojsomConfigurationException e) {
            _logger.error(e);
            throw new ServletException(e);
        } catch (BlojsomException e) {
            _logger.error(e);
            throw new ServletException(e);
        }
    }
}
