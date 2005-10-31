/**
 * Copyright (c) 2003-2005 , David A. Czarnecki
 * All rights reserved.
 *
 * Portions Copyright (c) 2003-2005  by Mark Lussier
 * Adapted code from Chris Nokleberg (http://sixlegs.com/)
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

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.util.BlojsomUtils;
import org.blojsom.util.BlojsomConstants;

import javax.servlet.*;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletRequestWrapper;
import java.io.IOException;
import java.util.*;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * PermalinkFilter
 *
 * @author David Czarnecki
 * @version $Id: PermalinkFilter.java,v 1.1.2.1 2005/07/21 14:11:03 johnan Exp $
 * @since blojsom 2.17
 */
public class PermalinkFilter implements Filter {

    private static final Log _logger = LogFactory.getLog(PermalinkFilter.class);

    private static final String YMD_PERMALINK_REGEX = "/(\\d\\d\\d\\d)/(\\d{1,2}+)/(\\d{1,2}+)/(.+)";
    private static final Pattern YMD_PERMALINK_PATTERN = Pattern.compile(YMD_PERMALINK_REGEX, Pattern.UNICODE_CASE);
    private static final String YMD_REGEX = "/(\\d\\d\\d\\d)/(\\d{1,2}+)/(\\d{1,2}+)/";
    private static final Pattern YMD_PATTERN = Pattern.compile(YMD_REGEX, Pattern.UNICODE_CASE);
    private static final String YM_REGEX = "/(\\d\\d\\d\\d)/(\\d{1,2}+)/";
    private static final Pattern YM_PATTERN = Pattern.compile(YM_REGEX, Pattern.UNICODE_CASE);
    private static final String Y_REGEX = "/(\\d\\d\\d\\d)/";
    private static final Pattern Y_PATTERN = Pattern.compile(Y_REGEX, Pattern.UNICODE_CASE);

    /**
     * Default constructor.
     */
    public PermalinkFilter() {
    }

    /**
     * Initialize the filter
     *
     * @param filterConfig {@link FilterConfig}
     * @throws ServletException If there is an error initializing the filter
     */
    public void init(FilterConfig filterConfig) throws ServletException {
    }

    /**
     * Remove the filter from service
     */
    public void destroy() {
    }

    /**
     * Process the request.
     * <p/>
     * Processes requests of the form
     * <ul>
     * <li>/YYYY/MM/DD/permalink</li>
     * <li>/YYYY/MM/DD/</li>
     * <li>/YYYY/MM/</li>
     * <li>/YYYY/</li>
     * </ul>
     *
     * @param request {@link ServletRequest}
     * @param response {@link ServletResponse}
     * @param chain {@link FilterChain} to execute
     * @throws IOException If there is an error executing the filter
     * @throws ServletException If there is an error executing the filter
     */
    public void doFilter(ServletRequest request, ServletResponse response, FilterChain chain)
            throws IOException, ServletException {
        request.setCharacterEncoding(BlojsomConstants.UTF8);
        
        HttpServletRequest hreq = (HttpServletRequest) request;
        String uri = hreq.getRequestURI();
        StringBuffer url = hreq.getRequestURL();
        String pathInfo = hreq.getPathInfo();
        if (BlojsomUtils.checkNullOrBlank(pathInfo)) {
            pathInfo = "/";
        }

        Matcher ymdpMatcher = YMD_PERMALINK_PATTERN.matcher(pathInfo);
        Matcher ymdMatcher = YMD_PATTERN.matcher(pathInfo);
        Matcher ymMatcher = YM_PATTERN.matcher(pathInfo);
        Matcher yMatcher = Y_PATTERN.matcher(pathInfo);
        Map extraParameters;

        if (ymdpMatcher.find()) {
            String year = ymdpMatcher.group(1);
            String month = ymdpMatcher.group(2);
            String day = ymdpMatcher.group(3);
            String permalink = ymdpMatcher.group(4);
            extraParameters = new HashMap();
            extraParameters.put("year", new String[]{year});
            extraParameters.put("month", new String[]{month});
            extraParameters.put("day", new String[]{day});
            extraParameters.put("permalink", new String[]{permalink});
            String yearSubstring = year + "/";
            int yearIndex = pathInfo.lastIndexOf(yearSubstring);
            String pathinfo = pathInfo.substring(0, yearIndex);
            yearIndex = uri.lastIndexOf(yearSubstring);
            String URI = uri.substring(0, yearIndex);
            yearIndex = url.lastIndexOf(yearSubstring);
            String URL = url.substring(0, yearIndex);
            _logger.debug("Handling YYYY/MM/DD/permalink request: " + pathinfo);
            hreq = new PermalinkRequest(hreq, extraParameters, URI, URL, pathinfo);
        } else if (ymdMatcher.find()) {
            String year = ymdMatcher.group(1);
            String month = ymdMatcher.group(2);
            String day = ymdMatcher.group(3);
            extraParameters = new HashMap();
            extraParameters.put("year", new String[]{year});
            extraParameters.put("month", new String[]{month});
            extraParameters.put("day", new String[]{day});
            String yearSubstring = year + "/";
            int yearIndex = pathInfo.lastIndexOf(yearSubstring);
            String pathinfo = pathInfo.substring(0, yearIndex);
            yearIndex = uri.lastIndexOf(yearSubstring);
            String URI = uri.substring(0, yearIndex);
            yearIndex = url.lastIndexOf(yearSubstring);
            String URL = url.substring(0, yearIndex);
            hreq = new PermalinkRequest(hreq, extraParameters, URI, URL, pathinfo);
            _logger.debug("Handling YYYY/MM/DD/ request: " + pathinfo);
        } else if (ymMatcher.find()) {
            String year = ymMatcher.group(1);
            String month = ymMatcher.group(2);
            extraParameters = new HashMap();
            extraParameters.put("year", new String[]{year});
            extraParameters.put("month", new String[]{month});
            String yearSubstring = year + "/";
            int yearIndex = pathInfo.lastIndexOf(yearSubstring);
            String pathinfo = pathInfo.substring(0, yearIndex);
            yearIndex = uri.lastIndexOf(yearSubstring);
            String URI = uri.substring(0, yearIndex);
            yearIndex = url.lastIndexOf(yearSubstring);
            String URL = url.substring(0, yearIndex);
            hreq = new PermalinkRequest(hreq, extraParameters, URI, URL, pathinfo);
            _logger.debug("Handling YYYY/MM request: " + pathinfo);
        } else if (yMatcher.find()) {
            String year = yMatcher.group(1);
            extraParameters = new HashMap();
            extraParameters.put("year", new String[]{year});
            String yearSubstring = year + "/";
            int yearIndex = pathInfo.lastIndexOf(yearSubstring);
            String pathinfo = pathInfo.substring(0, yearIndex);
            yearIndex = uri.lastIndexOf(yearSubstring);
            String URI = uri.substring(0, yearIndex);
            yearIndex = url.lastIndexOf(yearSubstring);
            String URL = url.substring(0, yearIndex);
            hreq = new PermalinkRequest(hreq, extraParameters, URI, URL, pathinfo);
            _logger.debug("Handling YYYY request: " + pathinfo);
        } else {
            // Check for a /category/permalink.html post
            String permalinkSubstring = "/";
            int permalinkIndex = pathInfo.lastIndexOf(permalinkSubstring);
            if (permalinkIndex < pathInfo.length() - 1) {
                extraParameters = new HashMap();
                extraParameters.put("permalink", new String[]{pathInfo.substring(permalinkIndex + 1)});
                String pathinfo = pathInfo.substring(0, permalinkIndex + 1);
                permalinkIndex = uri.lastIndexOf(permalinkSubstring);
                String URI = uri.substring(0, permalinkIndex + 1);
                permalinkIndex = url.lastIndexOf(permalinkSubstring);
                String URL = url.substring(0, permalinkIndex + 1);
                _logger.debug("Handling permalink request: " + pathinfo);
                hreq = new PermalinkRequest(hreq, extraParameters, URI, URL, pathinfo);
            }
        }

        chain.doFilter(hreq, response);
    }

    /**
     * Permalink request
     */
    public class PermalinkRequest extends HttpServletRequestWrapper {

        private Map params;
        private String uri;
        private String url;
        private String pathInfo;

        /**
         * Construct a new permalink request
         *
         * @param httpServletRequest {@link HttpServletRequest}
         * @param params Parameters pulled from the URL
         * @param uri URI
         * @param url URL
         * @param pathInfo Path information
         */
        public PermalinkRequest(HttpServletRequest httpServletRequest, Map params, String uri, String url, String pathInfo) {
            super(httpServletRequest);
            Map updatedParams = new HashMap(httpServletRequest.getParameterMap());
            Iterator keys = params.keySet().iterator();
            while (keys.hasNext()) {
                Object o = keys.next();
                updatedParams.put(o, params.get(o));
            }

            this.params = Collections.unmodifiableMap(updatedParams);
            this.uri = uri;
            this.url = url;
            this.pathInfo = pathInfo;
        }

        /**
         * Return the request URI
         *
         * @return Request URI
         */
        public String getRequestURI() {
            return uri;
        }

        /**
         * Return the request URL
         *
         * @return Request URL
         */
        public StringBuffer getRequestURL() {
            return new StringBuffer(url);
        }

        /**
         * Return the path information
         *
         * @return Path information
         */
        public String getPathInfo() {
            return pathInfo;
        }

        /**
         * Retrieve a named parameter
         *
         * @param name Parameter to retrieve
         * @return Parameter value or <code>null</code> if the parameter is not found
         */
        public String getParameter(String name) {
            String[] values = getParameterValues(name);
            return (values != null) ? values[0] : null;
        }

        /**
         * Retrieve the map of parameters
         *
         * @return Parameter map
         */
        public Map getParameterMap() {
            return params;
        }

        /**
         * Retrieve the parameter names
         *
         * @return {@link Enumeration} of parameter names
         */
        public Enumeration getParameterNames() {
            return Collections.enumeration(params.keySet());
        }

        /**
         * Retrieve a parameter value as a <code>String[]</code>
         *
         * @param name Parameter name
         * @return Parameter value as <code>String[]</code> or <code>null</code> if the parameter is not found
         */
        public String[] getParameterValues(String name) {
            return (String[]) params.get(name);
        }

        /**
         * Set the path information for the request
         *
         * @param pathInfo New path information
         * @since blojsom 2.25
         */
        public void setPathInfo(String pathInfo) {
            this.pathInfo = pathInfo;
        }
    }
}
