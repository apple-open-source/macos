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
 */ 
 
package javax.servlet.jsp;

import javax.servlet.Servlet;
import javax.servlet.ServletRequest;
import javax.servlet.ServletResponse;
import javax.servlet.jsp.PageContext;

/**
 * <p>
 * The JspFactory is an abstract class that defines a number of factory
 * methods available to a JSP page at runtime for the purposes of creating
 * instances of various interfaces and classes used to support the JSP 
 * implementation.
 * <p>
 * A conformant JSP Engine implementation will, during it's initialization
 * instantiate an implementation dependent subclass of this class, and make 
 * it globally available for use by JSP implementation classes by registering
 * the instance created with this class via the
 * static <code> setDefaultFactory() </code> method.
 * <p>
 * The PageContext and the JspEngineInfo classes are the only implementation-dependent
 * classes that can be created from the factory.
 * <p>
 * JspFactory objects should not be used by JSP page authors.
 */

public abstract class JspFactory {

    private static JspFactory deflt = null;

    /**
     * <p>
     * set the default factory for this implementation. It is illegal for
     * any principal other than the JSP Engine runtime to call this method.
     * </p>
     *
     * @param default	The default factory implementation
     */

    public static synchronized void setDefaultFactory(JspFactory deflt) {
	JspFactory.deflt = deflt;
    }

    /**
     * @return the default factory for this implementation
     */

    public static synchronized JspFactory getDefaultFactory() {
	return deflt;
    }

    /**
     * <p>
     * obtains an instance of an implementation dependent 
     * javax.servlet.jsp.PageContext abstract class for the calling Servlet
     * and currently pending request and response.
     * </p>
     *
     * <p>
     * This method is typically called early in the processing of the 
     * _jspService() method of a JSP implementation class in order to 
     * obtain a PageContext object for the request being processed.
     * </p>
     * <p>
     * Invoking this method shall result in the PageContext.initialize()
     * method being invoked. The PageContext returned is properly initialized.
     * </p>
     * <p>
     * All PageContext objects obtained via this method shall be released
     * by invoking releasePageContext().
     * </p>
     *
     * @param servlet   the requesting servlet
     * @param config    the ServletConfig for the requesting Servlet
     * @param request	the current request pending on the servlet
     * @param response	the current response pending on the servlet
     * @param errorPageURL the URL of the error page for the requesting JSP, or null
     * @param needsSession true if the JSP participates in a session
     * @param buffer	size of buffer in bytes, PageContext.NO_BUFFER if no buffer,
     *			PageContext.DEFAULT_BUFFER if implementation default.
     * @param autoflush	should the buffer autoflush to the output stream on buffer
     *			overflow, or throw an IOException?
     *
     * @return the page context
     *
     * @see javax.servlet.jsp.PageContext
     */

    public abstract PageContext getPageContext(Servlet	       servlet,
				    	       ServletRequest  request,
				    	       ServletResponse response,
				    	       String	       errorPageURL,
				    	       boolean         needsSession,
				    	       int             buffer,
				    	       boolean         autoflush);

    /**
     * <p>
     * called to release a previously allocated PageContext object.
     * Results in PageContext.release() being invoked.
     * This method should be invoked prior to returning from the _jspService() method of a JSP implementation
     * class.
     * </p>
     *
     * @param pc A PageContext previously obtained by getPageContext()
     */

    public abstract void releasePageContext(PageContext pc);

    /**
     * <p>
     * called to get implementation-specific information on the current JSP engine
     * </p>
     *
     * @return a JspEngineInfo object describing the current JSP engine
     */
    
    public abstract JspEngineInfo getEngineInfo();
}
