/*
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
 * ====================================================================
 *
 * This source code implements specifications defined by the Java
 * Community Process. In order to remain compliant with the specification
 * DO NOT add / change / or delete method signatures!
 */

package javax.servlet.http;

import javax.servlet.ServletRequestWrapper;
import java.util.Enumeration;

/**
 * 
 * Provides a convenient implementation of the HttpServletRequest interface that
 * can be subclassed by developers wishing to adapt the request to a Servlet.
 * This class implements the Wrapper or Decorator pattern. Methods default to
 * calling through to the wrapped request object.
 * 
 *
 * @see 	javax.servlet.http.HttpServletRequest
  * @since	v 2.3
 *
 */


public class HttpServletRequestWrapper extends ServletRequestWrapper implements HttpServletRequest {

	/** 
	* Constructs a request object wrapping the given request.
	* @throws java.lang.IllegalArgumentException if the request is null
	*/
    public HttpServletRequestWrapper(HttpServletRequest request) {
	    super(request);
    }
    
    private HttpServletRequest _getHttpServletRequest() {
	return (HttpServletRequest) super.getRequest();
    }

    /**
     * The default behavior of this method is to return getAuthType()
     * on the wrapped request object.
     */

    public String getAuthType() {
	return this._getHttpServletRequest().getAuthType();
    }
   
    /**
     * The default behavior of this method is to return getCookies()
     * on the wrapped request object.
     */
    public Cookie[] getCookies() {
	return this._getHttpServletRequest().getCookies();
    }

    /**
     * The default behavior of this method is to return getDateHeader(String name)
     * on the wrapped request object.
     */
    public long getDateHeader(String name) {
	return this._getHttpServletRequest().getDateHeader(name);
    }
        	
    /**
     * The default behavior of this method is to return getHeader(String name)
     * on the wrapped request object.
     */
    public String getHeader(String name) {
	return this._getHttpServletRequest().getHeader(name);
    }
    
    /**
     * The default behavior of this method is to return getHeaders(String name)
     * on the wrapped request object.
     */
    public Enumeration getHeaders(String name) {
	return this._getHttpServletRequest().getHeaders(name);
    }  

    /**
     * The default behavior of this method is to return getHeaderNames()
     * on the wrapped request object.
     */
  
    public Enumeration getHeaderNames() {
	return this._getHttpServletRequest().getHeaderNames();
    }
    
    /**
     * The default behavior of this method is to return getIntHeader(String name)
     * on the wrapped request object.
     */

     public int getIntHeader(String name) {
	return this._getHttpServletRequest().getIntHeader(name);
    }
    
    /**
     * The default behavior of this method is to return getMethod()
     * on the wrapped request object.
     */
    public String getMethod() {
	return this._getHttpServletRequest().getMethod();
    }
    
    /**
     * The default behavior of this method is to return getPathInfo()
     * on the wrapped request object.
     */
    public String getPathInfo() {
	return this._getHttpServletRequest().getPathInfo();
    }

    /**
     * The default behavior of this method is to return getPathTranslated()
     * on the wrapped request object.
     */

     public String getPathTranslated() {
	return this._getHttpServletRequest().getPathTranslated();
    }

    /**
     * The default behavior of this method is to return getContextPath()
     * on the wrapped request object.
     */
    public String getContextPath() {
	return this._getHttpServletRequest().getContextPath();
    }
    
    /**
     * The default behavior of this method is to return getQueryString()
     * on the wrapped request object.
     */
    public String getQueryString() {
	return this._getHttpServletRequest().getQueryString();
    }
    
    /**
     * The default behavior of this method is to return getRemoteUser()
     * on the wrapped request object.
     */
    public String getRemoteUser() {
	return this._getHttpServletRequest().getRemoteUser();
    }
    
 
    /**
     * The default behavior of this method is to return isUserInRole(String role)
     * on the wrapped request object.
     */
    public boolean isUserInRole(String role) {
	return this._getHttpServletRequest().isUserInRole(role);
    }
    
    
    
    /**
     * The default behavior of this method is to return getUserPrincipal()
     * on the wrapped request object.
     */
    public java.security.Principal getUserPrincipal() {
	return this._getHttpServletRequest().getUserPrincipal();
    }
    
   
    /**
     * The default behavior of this method is to return getRequestedSessionId()
     * on the wrapped request object.
     */
    public String getRequestedSessionId() {
	return this._getHttpServletRequest().getRequestedSessionId();
    }
    
    /**
     * The default behavior of this method is to return getRequestURI()
     * on the wrapped request object.
     */
    public String getRequestURI() {
	return this._getHttpServletRequest().getRequestURI();
    }
	/**
     * The default behavior of this method is to return getRequestURL()
     * on the wrapped request object.
     */
    public StringBuffer getRequestURL() {
	return this._getHttpServletRequest().getRequestURL();
    }
	
    
    /**
     * The default behavior of this method is to return getServletPath()
     * on the wrapped request object.
     */
    public String getServletPath() {
	return this._getHttpServletRequest().getServletPath();
    }
    
    
    /**
     * The default behavior of this method is to return getSession(boolean create)
     * on the wrapped request object.
     */
    public HttpSession getSession(boolean create) {
	return this._getHttpServletRequest().getSession(create);
    }
    
    /**
     * The default behavior of this method is to return getSession()
     * on the wrapped request object.
     */
    public HttpSession getSession() {
	return this._getHttpServletRequest().getSession();
    }
    
    /**
     * The default behavior of this method is to return isRequestedSessionIdValid()
     * on the wrapped request object.
     */ 

    public boolean isRequestedSessionIdValid() {
	return this._getHttpServletRequest().isRequestedSessionIdValid();
    }
     
    
    /**
     * The default behavior of this method is to return isRequestedSessionIdFromCookie()
     * on the wrapped request object.
     */
    public boolean isRequestedSessionIdFromCookie() {
	return this._getHttpServletRequest().isRequestedSessionIdFromCookie();
    }
    
    	  /**
     * The default behavior of this method is to return isRequestedSessionIdFromURL()
     * on the wrapped request object.
     */ 
    public boolean isRequestedSessionIdFromURL() {
	return this._getHttpServletRequest().isRequestedSessionIdFromURL();
    }
    
    /**
     * The default behavior of this method is to return isRequestedSessionIdFromUrl()
     * on the wrapped request object.
     */
    public boolean isRequestedSessionIdFromUrl() {
	return this._getHttpServletRequest().isRequestedSessionIdFromUrl();
    }


    
}
