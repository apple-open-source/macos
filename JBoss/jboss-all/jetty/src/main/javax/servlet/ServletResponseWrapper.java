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

import java.io.IOException;
import java.io.PrintWriter;
import java.io.UnsupportedEncodingException;
import java.util.Locale;

/**
 * 
 * Provides a convenient implementation of the ServletResponse interface that
 * can be subclassed by developers wishing to adapt the response from a Servlet.
 * This class implements the Wrapper or Decorator pattern. Methods default to
 * calling through to the wrapped response object.
 * 
 * @author 	Various
 * @version 	$Version$
  * @since	v 2.3
 *
 * @see 	javax.servlet.ServletResponse
 *
 */

 
public class ServletResponseWrapper implements ServletResponse {
	private ServletResponse response;
	/**
	* Creates a ServletResponse adaptor wrapping the given response object.
	* @throws java.lang.IllegalArgumentException if the response is null.
	*/


	public ServletResponseWrapper(ServletResponse response) {
	    if (response == null) {
		throw new IllegalArgumentException("Response cannot be null");
	    }
	    this.response = response;
	}

	/**
	* Return the wrapped ServletResponse object.
	*/

	public ServletResponse getResponse() {
		return this.response;
	}	
	
	
	/**
	* Sets the response being wrapped. 
	* @throws java.lang.IllegalArgumentException if the response is null.
	*/
	
	public void setResponse(ServletResponse response) {
	    if (response == null) {
		throw new IllegalArgumentException("Response cannot be null");
	    }
	    this.response = response;
	}

	  /**
     * The default behavior of this method is to return getCharacterEncoding()
     * on the wrapped response object.
     */


    public String getCharacterEncoding() {
	return this.response.getCharacterEncoding();
	}
    
	  /**
     * The default behavior of this method is to return getOutputStream()
     * on the wrapped response object.
     */

    public ServletOutputStream getOutputStream() throws IOException {
	return this.response.getOutputStream();
    }  
      
     /**
     * The default behavior of this method is to return getWriter()
     * on the wrapped response object.
     */


    public PrintWriter getWriter() throws IOException {
	return this.response.getWriter();
	}
    
    /**
     * The default behavior of this method is to call setContentLength(int len)
     * on the wrapped response object.
     */

    public void setContentLength(int len) {
	this.response.setContentLength(len);
    }
    
    /**
     * The default behavior of this method is to call setContentType(String type)
     * on the wrapped response object.
     */

    public void setContentType(String type) {
	this.response.setContentType(type);
    }
    
    /**
     * The default behavior of this method is to call setBufferSize(int size)
     * on the wrapped response object.
     */
    public void setBufferSize(int size) {
	this.response.setBufferSize(size);
    }
    
    /**
     * The default behavior of this method is to return getBufferSize()
     * on the wrapped response object.
     */
    public int getBufferSize() {
	return this.response.getBufferSize();
    }

    /**
     * The default behavior of this method is to call flushBuffer()
     * on the wrapped response object.
     */

    public void flushBuffer() throws IOException {
	this.response.flushBuffer();
    }
    
    /**
     * The default behavior of this method is to return isCommitted()
     * on the wrapped response object.
     */
    public boolean isCommitted() {
	return this.response.isCommitted();
    }

    /**
     * The default behavior of this method is to call reset()
     * on the wrapped response object.
     */

    public void reset() {
	this.response.reset();
    }
    
    /**
     * The default behavior of this method is to call resetBuffer()
     * on the wrapped response object.
     */
     
    public void resetBuffer() {
	this.response.resetBuffer();
    }
    
    /**
     * The default behavior of this method is to call setLocale(Locale loc)
     * on the wrapped response object.
     */

    public void setLocale(Locale loc) {
	this.response.setLocale(loc);
    }
    
    /**
     * The default behavior of this method is to return getLocale()
     * on the wrapped response object.
     */
    public Locale getLocale() {
	return this.response.getLocale();
    }


}





