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


/**
 * Defines an exception that a servlet or filter throws to indicate
 * that it is permanently or temporarily unavailable. 
 *
 * <p>When a servlet or filter is permanently unavailable, something is wrong
 * with the it, and it cannot handle
 * requests until some action is taken. For example, a servlet
 * might be configured incorrectly, or a filter's state may be corrupted.
 * The component should log both the error and the corrective action
 * that is needed.
 *
 * <p>A servlet or filter is temporarily unavailable if it cannot handle
 * requests momentarily due to some system-wide problem. For example,
 * a third-tier server might not be accessible, or there may be 
 * insufficient memory or disk storage to handle requests. A system
 * administrator may need to take corrective action.
 *
 * <p>Servlet containers can safely treat both types of unavailable
 * exceptions in the same way. However, treating temporary unavailability
 * effectively makes the servlet container more robust. Specifically,
 * the servlet container might block requests to the servlet or filter for a period
 * of time suggested by the exception, rather than rejecting them until
 * the servlet container restarts.
 *
 *
 * @author 	Various
 * @version 	$Version$
 *
 */

public class UnavailableException
extends ServletException {

    private Servlet     servlet;           // what's unavailable
    private boolean     permanent;         // needs admin action?
    private int         seconds;           // unavailability estimate

    /**
     * 
     * @deprecated	As of Java Servlet API 2.2, use {@link
     * 			#UnavailableException(String)} instead.
     *
     * @param servlet 	the <code>Servlet</code> instance that is
     *                  unavailable
     *
     * @param msg 	a <code>String</code> specifying the
     *                  descriptive message
     *
     */

    public UnavailableException(Servlet servlet, String msg) {
	super(msg);
	this.servlet = servlet;
	permanent = true;
    }
 
    /**
     * @deprecated	As of Java Servlet API 2.2, use {@link
     *			#UnavailableException(String, int)} instead.
     *
     * @param seconds	an integer specifying the number of seconds
     * 			the servlet expects to be unavailable; if
     *			zero or negative, indicates that the servlet
     *			can't make an estimate
     *
     * @param servlet	the <code>Servlet</code> that is unavailable
     * 
     * @param msg	a <code>String</code> specifying the descriptive 
     *			message, which can be written to a log file or 
     *			displayed for the user.
     *
     */
    
    public UnavailableException(int seconds, Servlet servlet, String msg) {
	super(msg);
	this.servlet = servlet;
	if (seconds <= 0)
	    this.seconds = -1;
	else
	    this.seconds = seconds;
	permanent = false;
    }

    /**
     * 
     * Constructs a new exception with a descriptive
     * message indicating that the servlet is permanently
     * unavailable.
     *
     * @param msg 	a <code>String</code> specifying the
     *                  descriptive message
     *
     */

    public UnavailableException(String msg) {
	super(msg);

	permanent = true;
    }

    /**
     * Constructs a new exception with a descriptive message
     * indicating that the servlet is temporarily unavailable
     * and giving an estimate of how long it will be unavailable.
     * 
     * <p>In some cases, the servlet cannot make an estimate. For
     * example, the servlet might know that a server it needs is
     * not running, but not be able to report how long it will take
     * to be restored to functionality. This can be indicated with
     * a negative or zero value for the <code>seconds</code> argument.
     *
     * @param msg	a <code>String</code> specifying the
     *                  descriptive message, which can be written
     *                  to a log file or displayed for the user.
     *
     * @param seconds	an integer specifying the number of seconds
     * 			the servlet expects to be unavailable; if
     *			zero or negative, indicates that the servlet
     *			can't make an estimate
     *
     */
    
    public UnavailableException(String msg, int seconds) {
	super(msg);

	if (seconds <= 0)
	    this.seconds = -1;
	else
	    this.seconds = seconds;

	permanent = false;
    }

    /**
     *
     * Returns a <code>boolean</code> indicating
     * whether the servlet is permanently unavailable.
     * If so, something is wrong with the servlet, and the
     * system administrator must take some corrective action.
     *
     * @return		<code>true</code> if the servlet is
     *			permanently unavailable; <code>false</code>
     *			if the servlet is available or temporarily
     *			unavailable
     *
     */
     
    public boolean isPermanent() {
	return permanent;
    }
  
    /**
     * @deprecated	As of Java Servlet API 2.2, with no replacement.
     *
     * Returns the servlet that is reporting its unavailability.
     * 
     * @return		the <code>Servlet</code> object that is 
     *			throwing the <code>UnavailableException</code>
     *
     */
     
    public Servlet getServlet() {
	return servlet;
    }

    /**
     * Returns the number of seconds the servlet expects to 
     * be temporarily unavailable.  
     *
     * <p>If this method returns a negative number, the servlet
     * is permanently unavailable or cannot provide an estimate of
     * how long it will be unavailable. No effort is
     * made to correct for the time elapsed since the exception was
     * first reported.
     *
     * @return		an integer specifying the number of seconds
     *			the servlet will be temporarily unavailable,
     *			or a negative number if the servlet is permanently
     *			unavailable or cannot make an estimate
     *
     */
     
    public int getUnavailableSeconds() {
	return permanent ? -1 : seconds;
    }
}
