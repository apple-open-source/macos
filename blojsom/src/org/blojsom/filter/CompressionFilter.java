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
package org.blojsom.filter;

import org.blojsom.util.BlojsomConstants;
import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;

import javax.servlet.*;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.IOException;

/**
 * CompressionFilter
 * <p/>
 * Copyright 2003 Jayson Falkner (jayson@jspinsider.com)
 * This code is from "Servlets and JavaServer pages; the J2EE Web Tier",
 * http://www.jspbook.com. You may freely use the code both commercially
 * and non-commercially. If you like the code, please pick up a copy of
 * the book and help support the authors, development of more free code,
 * and the JSP/Servlet/J2EE community.
 *
 * @version $Id: CompressionFilter.java,v 1.2.2.1 2005/07/21 14:11:03 johnan Exp $
 * @since blojsom 2.10
 */
public class CompressionFilter implements Filter, BlojsomConstants {

    private Log _logger = LogFactory.getLog(CompressionFilter.class);

    /**
     * Initialize the CompressionFilter
     *
     * @param filterConfig FilterConfig object
     * @throws ServletException If there is an error initializing the filter
     */
    public void init(FilterConfig filterConfig) throws ServletException {
    }

    /**
     * Filter a request looking for the "accept-encoding" HTTP header and if available,
     * send compressed content.
     *
     * @param req   Request
     * @param res   Response
     * @param chain Filter chain
     * @throws IOException      If there is an error writing the response
     * @throws ServletException If there is an error processing the request
     */
    public void doFilter(ServletRequest req, ServletResponse res,
                         FilterChain chain) throws IOException, ServletException {
        if (req instanceof HttpServletRequest) {
            HttpServletRequest request = (HttpServletRequest) req;
            HttpServletResponse response = (HttpServletResponse) res;
            String ae = request.getHeader("accept-encoding");
            if (ae != null && ae.indexOf("gzip") != -1) {
                _logger.debug("GZIP supported, compressing.");
                GZIPResponseWrapper wrappedResponse = new GZIPResponseWrapper(response);
                try {
                    chain.doFilter(req, wrappedResponse);
                } catch (IOException e) {
                    _logger.error(e);
                    if (e.getCause() != null) {
                        _logger.error(e.getCause());
                    }
                } catch (ServletException e) {
                    _logger.error(e);
                    if (e.getRootCause() != null) {
                        _logger.error(e.getRootCause());
                    }
                } finally {
                    wrappedResponse.finishResponse();
                }
            } else {
                chain.doFilter(req, res);
            }
        }
    }

    /**
     * Called when the filter is taken out of service
     */
    public void destroy() {
    }
}
