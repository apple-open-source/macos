/*
 * $Header: /home/cvs/jakarta-tomcat-4.0/webapps/examples/WEB-INF/classes/filters/RequestDumperFilter.java,v 1.5 2001/05/23 22:26:17 craigmcc Exp $
 * $Revision: 1.5 $
 * $Date: 2001/05/23 22:26:17 $
 *
 * ====================================================================
 *
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 1999 The Apache Software Foundation.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution, if
 *    any, must include the following acknowlegement:
 *       "This product includes software developed by the
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowlegement may appear in the software itself,
 *    if and wherever such third-party acknowlegements normally appear.
 *
 * 4. The names "The Jakarta Project", "Tomcat", and "Apache Software
 *    Foundation" must not be used to endorse or promote products derived
 *    from this software without prior written permission. For written
 *    permission, please contact apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache"
 *    nor may "Apache" appear in their names without prior written
 *    permission of the Apache Group.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 * [Additional notices, if required by prior licensing conditions]
 *
 */


package filters;


import java.io.IOException;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.sql.Timestamp;
import java.util.Enumeration;
import java.util.Locale;
import javax.servlet.Filter;
import javax.servlet.FilterChain;
import javax.servlet.FilterConfig;
import javax.servlet.ServletContext;
import javax.servlet.ServletException;
import javax.servlet.ServletRequest;
import javax.servlet.ServletResponse;
import javax.servlet.http.Cookie;
import javax.servlet.http.HttpServletRequest;


/**
 * Example filter that dumps interesting state information about a request
 * to the associated servlet context log file, before allowing the servlet
 * to process the request in the usual way.  This can be installed as needed
 * to assist in debugging problems.
 *
 * @author Craig McClanahan
 * @version $Revision: 1.5 $ $Date: 2001/05/23 22:26:17 $
 */

public final class RequestDumperFilter implements Filter {


    // ----------------------------------------------------- Instance Variables


    /**
     * The filter configuration object we are associated with.  If this value
     * is null, this filter instance is not currently configured.
     */
    private FilterConfig filterConfig = null;


    // --------------------------------------------------------- Public Methods


    /**
     * Take this filter out of service.
     */
    public void destroy() {

        this.filterConfig = null;

    }


    /**
     * Time the processing that is performed by all subsequent filters in the
     * current filter stack, including the ultimately invoked servlet.
     *
     * @param request The servlet request we are processing
     * @param result The servlet response we are creating
     * @param chain The filter chain we are processing
     *
     * @exception IOException if an input/output error occurs
     * @exception ServletException if a servlet error occurs
     */
    public void doFilter(ServletRequest request, ServletResponse response,
                         FilterChain chain)
	throws IOException, ServletException {

        if (filterConfig == null)
	    return;

	// Render the generic servlet request properties
	StringWriter sw = new StringWriter();
	PrintWriter writer = new PrintWriter(sw);
	writer.println("Request Received at " +
		       (new Timestamp(System.currentTimeMillis())));
	writer.println(" characterEncoding=" + request.getCharacterEncoding());
	writer.println("     contentLength=" + request.getContentLength());
	writer.println("       contentType=" + request.getContentType());
	writer.println("            locale=" + request.getLocale());
	writer.print("           locales=");
	Enumeration locales = request.getLocales();
	boolean first = true;
	while (locales.hasMoreElements()) {
	    Locale locale = (Locale) locales.nextElement();
	    if (first)
	        first = false;
	    else
	        writer.print(", ");
	    writer.print(locale.toString());
	}
	writer.println();
	Enumeration names = request.getParameterNames();
	while (names.hasMoreElements()) {
	    String name = (String) names.nextElement();
	    writer.print("         parameter=" + name + "=");
	    String values[] = request.getParameterValues(name);
	    for (int i = 0; i < values.length; i++) {
	        if (i > 0)
		    writer.print(", ");
		writer.print(values[i]);
	    }
	    writer.println();
	}
	writer.println("          protocol=" + request.getProtocol());
	writer.println("        remoteAddr=" + request.getRemoteAddr());
	writer.println("        remoteHost=" + request.getRemoteHost());
	writer.println("            scheme=" + request.getScheme());
	writer.println("        serverName=" + request.getServerName());
	writer.println("        serverPort=" + request.getServerPort());
	writer.println("          isSecure=" + request.isSecure());

	// Render the HTTP servlet request properties
	if (request instanceof HttpServletRequest) {
	    writer.println("---------------------------------------------");
	    HttpServletRequest hrequest = (HttpServletRequest) request;
	    writer.println("       contextPath=" + hrequest.getContextPath());
	    Cookie cookies[] = hrequest.getCookies();
            if (cookies == null)
                cookies = new Cookie[0];
	    for (int i = 0; i < cookies.length; i++) {
	        writer.println("            cookie=" + cookies[i].getName() +
			       "=" + cookies[i].getValue());
	    }
	    names = hrequest.getHeaderNames();
	    while (names.hasMoreElements()) {
	        String name = (String) names.nextElement();
		String value = hrequest.getHeader(name);
	        writer.println("            header=" + name + "=" + value);
	    }
	    writer.println("            method=" + hrequest.getMethod());
	    writer.println("          pathInfo=" + hrequest.getPathInfo());
	    writer.println("       queryString=" + hrequest.getQueryString());
	    writer.println("        remoteUser=" + hrequest.getRemoteUser());
	    writer.println("requestedSessionId=" +
			   hrequest.getRequestedSessionId());
	    writer.println("        requestURI=" + hrequest.getRequestURI());
	    writer.println("       servletPath=" + hrequest.getServletPath());
	}
	writer.println("=============================================");

	// Log the resulting string
	writer.flush();
	filterConfig.getServletContext().log(sw.getBuffer().toString());

	// Pass control on to the next filter
        chain.doFilter(request, response);

    }


    /**
     * Place this filter into service.
     *
     * @param filterConfig The filter configuration object
     */
    public void init(FilterConfig filterConfig) throws ServletException {

	this.filterConfig = filterConfig;

    }


    /**
     * Return a String representation of this object.
     */
    public String toString() {

	if (filterConfig == null)
	    return ("RequestDumperFilter()");
	StringBuffer sb = new StringBuffer("RequestDumperFilter(");
	sb.append(filterConfig);
	sb.append(")");
	return (sb.toString());

    }


}

