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

package javax.servlet;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.util.Enumeration;
import java.util.Locale;
import java.util.Map;



/**
 * 
 * Provides a convenient implementation of the ServletRequest interface that
 * can be subclassed by developers wishing to adapt the request to a Servlet.
 * This class implements the Wrapper or Decorator pattern. Methods default to
 * calling through to the wrapped request object.
  * @since	v 2.3
 * 
 * 
 *
 * @see 	javax.servlet.ServletRequest
 *
 */

public class ServletRequestWrapper implements ServletRequest {
    private ServletRequest request;

	/**
	* Creates a ServletRequest adaptor wrapping the given request object. 
	* @throws java.lang.IllegalArgumentException if the request is null
	*/

    public ServletRequestWrapper(ServletRequest request) {
	if (request == null) {
	    throw new IllegalArgumentException("Request cannot be null");   
	}
	this.request = request;
    }

	/**
	* Return the wrapped request object.
	*/
	public ServletRequest getRequest() {
		return this.request;
	}
	
	/**
	* Sets the request object being wrapped. 
	* @throws java.lang.IllegalArgumentException if the request is null.
	*/
	
	public void setRequest(ServletRequest request) {
	    if (request == null) {
		throw new IllegalArgumentException("Request cannot be null");
	    }
	    this.request = request;
	}

    /**
     *
     * The default behavior of this method is to call getAttribute(String name)
     * on the wrapped request object.
     */

    public Object getAttribute(String name) {
	return this.request.getAttribute(name);
	}
    
    

    /**
     * The default behavior of this method is to return getAttributeNames()
     * on the wrapped request object.
     */

    public Enumeration getAttributeNames() {
	return this.request.getAttributeNames();
	}    
    
    
    
    /**
      * The default behavior of this method is to return getCharacterEncoding()
     * on the wrapped request object.
     */

    public String getCharacterEncoding() {
	return this.request.getCharacterEncoding();
	}
	
    /**
      * The default behavior of this method is to set the character encoding
     * on the wrapped request object.
     */

    public void setCharacterEncoding(String enc) throws java.io.UnsupportedEncodingException {
	this.request.setCharacterEncoding(enc);
	}
    
    
    /**
      * The default behavior of this method is to return getContentLength()
     * on the wrapped request object.
     */

    public int getContentLength() {
	return this.request.getContentLength();
    }
    
    
    

       /**
      * The default behavior of this method is to return getContentType()
     * on the wrapped request object.
     */
    public String getContentType() {
	return this.request.getContentType();
    }
    
    
    

     /**
      * The default behavior of this method is to return getInputStream()
     * on the wrapped request object.
     */

    public ServletInputStream getInputStream() throws IOException {
	return this.request.getInputStream();
	}
     
    
    

    /**
      * The default behavior of this method is to return getParameter(String name)
     * on the wrapped request object.
     */

    public String getParameter(String name) {
	return this.request.getParameter(name);
    }
    
    /**
      * The default behavior of this method is to return getParameterMap()
     * on the wrapped request object.
     */
    public Map getParameterMap() {
	return this.request.getParameterMap();
    }
    
    
    

    /**
      * The default behavior of this method is to return getParameterNames()
     * on the wrapped request object.
     */
     
    public Enumeration getParameterNames() {
	return this.request.getParameterNames();
    }
    
    
    

       /**
      * The default behavior of this method is to return getParameterValues(String name)
     * on the wrapped request object.
     */
    public String[] getParameterValues(String name) {
	return this.request.getParameterValues(name);
	}
    
    
    

     /**
      * The default behavior of this method is to return getProtocol()
     * on the wrapped request object.
     */
    
    public String getProtocol() {
	return this.request.getProtocol();
	}
    
    
    

    /**
      * The default behavior of this method is to return getScheme()
     * on the wrapped request object.
     */
    

    public String getScheme() {
	return this.request.getScheme();
	}
    
    
    

    /**
      * The default behavior of this method is to return getServerName()
     * on the wrapped request object.
     */
    public String getServerName() {
	return this.request.getServerName();
	}
    
    
    

   /**
      * The default behavior of this method is to return getServerPort()
     * on the wrapped request object.
     */

    public int getServerPort() {
	return this.request.getServerPort();
	}
    
    
    
  /**
      * The default behavior of this method is to return getReader()
     * on the wrapped request object.
     */

    public BufferedReader getReader() throws IOException {
	return this.request.getReader();
	}
    
    
    

    /**
      * The default behavior of this method is to return getRemoteAddr()
     * on the wrapped request object.
     */
    
    public String getRemoteAddr() {
	return this.request.getRemoteAddr();
    }
    
    
    

      /**
      * The default behavior of this method is to return getRemoteHost()
     * on the wrapped request object.
     */

    public String getRemoteHost() {
	return this.request.getRemoteHost();
    }
    
    
    

    /**
      * The default behavior of this method is to return setAttribute(String name, Object o)
     * on the wrapped request object.
     */

    public void setAttribute(String name, Object o) {
	this.request.setAttribute(name, o);
    }
    
    
    

    /**
      * The default behavior of this method is to call removeAttribute(String name)
     * on the wrapped request object.
     */
    public void removeAttribute(String name) {
	this.request.removeAttribute(name);
    }
    
    
    

   /**
      * The default behavior of this method is to return getLocale()
     * on the wrapped request object.
     */

    public Locale getLocale() {
	return this.request.getLocale();
    }
    
    
    

     /**
      * The default behavior of this method is to return getLocales()
     * on the wrapped request object.
     */

    public Enumeration getLocales() {
	return this.request.getLocales();
    }
    
    
    

    /**
      * The default behavior of this method is to return isSecure()
     * on the wrapped request object.
     */

    public boolean isSecure() {
	return this.request.isSecure();
    }
    
    
    

    /**
      * The default behavior of this method is to return getRequestDispatcher(String path)
     * on the wrapped request object.
     */

    public RequestDispatcher getRequestDispatcher(String path) {
	return this.request.getRequestDispatcher(path);
    }
    
    
    

    /**
      * The default behavior of this method is to return getRealPath(String path)
     * on the wrapped request object.
     */

    public String getRealPath(String path) {
	return this.request.getRealPath(path);
    }
    
    
}

