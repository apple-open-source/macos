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

	/** 
	** A filter is an object than perform filtering tasks
	** on either the request to a resource (a servlet or static content), or on the response from 
	** a resource, or both.<br><br>
	** Filters perform filtering in the <code>doFilter</code> method. Every Filter has access to 
	** a FilterConfig object from which it can obtain its initialization parameters, a
	** reference to the ServletContext which it can use, for example, to load resources
	** needed for filtering tasks.
	** <p>
	** Filters are configured in the deployment descriptor of a web application
	** <p>
	** Examples that have been identified for this design are<br>
	** 1) Authentication Filters <br>
	** 2) Logging and Auditing Filters <br>
	** 3) Image conversion Filters <br>
    	** 4) Data compression Filters <br>
	** 5) Encryption Filters <br>
	** 6) Tokenizing Filters <br>
	** 7) Filters that trigger resource access events <br>
	** 8) XSL/T filters <br>
	** 9) Mime-type chain Filter <br>
	 * @since	Servlet 2.3
	*/

public interface Filter {

	/** 
	* Called by the web container to indicate to a filter that it is being placed into
	* service. The servlet container calls the init method exactly once after instantiating the
	* filter. The init method must complete successfully before the filter is asked to do any
	* filtering work. <br><br>

     	* The web container cannot place the filter into service if the init method either<br>
        * 1.Throws a ServletException <br>
        * 2.Does not return within a time period defined by the web container 
	*/
	public void init(FilterConfig filterConfig) throws ServletException;
	
	
	/**
	* The <code>doFilter</code> method of the Filter is called by the container
	* each time a request/response pair is passed through the chain due
	* to a client request for a resource at the end of the chain. The FilterChain passed in to this
	* method allows the Filter to pass on the request and response to the next entity in the
	* chain.<p>
	* A typical implementation of this method would follow the following pattern:- <br>
	* 1. Examine the request<br>
	* 2. Optionally wrap the request object with a custom implementation to
	* filter content or headers for input filtering <br>
	* 3. Optionally wrap the response object with a custom implementation to
	* filter content or headers for output filtering <br>
	* 4. a) <strong>Either</strong> invoke the next entity in the chain using the FilterChain object (<code>chain.doFilter()</code>), <br>   
	** 4. b) <strong>or</strong> not pass on the request/response pair to the next entity in the filter chain to block the request processing<br>
	** 5. Directly set headers on the response after invokation of the next entity in ther filter chain.
	**/
    public void doFilter ( ServletRequest request, ServletResponse response, FilterChain chain ) throws IOException, ServletException;

	/**
	* Called by the web container to indicate to a filter that it is being taken out of service. This 
	* method is only called once all threads within the filter's doFilter method have exited or after
	* a timeout period has passed. After the web container calls this method, it will not call the
	* doFilter method again on this instance of the filter. <br><br>
	* 
     	* This method gives the filter an opportunity to clean up any resources that are being held (for
	* example, memory, file handles, threads) and make sure that any persistent state is synchronized
	* with the filter's current state in memory.
	*/

	public void destroy();


}

