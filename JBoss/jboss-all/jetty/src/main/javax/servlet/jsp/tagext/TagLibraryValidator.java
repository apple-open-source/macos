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

package javax.servlet.jsp.tagext;

import java.util.Map;

/**
 * Translation-time validator class for a JSP page. 
 * A validator operates on the XML document associated with the JSP page.
 *
 * <p>
 * The TLD file associates a TagLibraryValidator class and some init
 * arguments with a tag library.
 *
 * <p>
 * The JSP container is reponsible for locating an appropriate
 * instance of the appropriate subclass by
 *
 * <ul>
 * <li> new a fresh instance, or reuse an available one
 * <li> invoke the setInitParams(Map) method on the instance
 * </ul>
 *
 * once initialized, the validate(String, String, PageData) method will
 * be invoked, where the first two arguments are the prefix
 * and uri arguments used in the taglib directive.
 *
 * <p>
 * A TagLibraryValidator instance
 * may create auxiliary objects internally to perform
 * the validation (e.g. an XSchema validator) and may reuse it for all
 * the pages in a given translation run.
 *
 * <p>
 * The JSP container is not guaranteed to serialize invocations of
 * validate() method, and TagLibraryValidators should perform any
 * synchronization they may require.
 *
 * <p>
 * A JSP container may optionally support a jsp:id attribute to
 * provide higher quality validation errors.
 * When supported, the container will track the JSP pages
 * as passed to the container, and will assign to each element
 * a unique "id", which is passed as the value of the jsp:id
 * attribute.  Each XML element in the XML view available will
 * be extended with this attribute.  The TagLibraryValidator
 * can then use the attribute in one or more ValidationMessage
 * objects.  The container then, in turn, can use these
 * values to provide more precise information on the location
 * of an error.
 */

abstract public class TagLibraryValidator {

    /**
     * Set the init data in the TLD for this validator.
     * Parameter names are keys, and parameter values are the values.
     *
     * @param initMap A Map describing the init parameters
     */
    public void setInitParameters(Map map) {
	initParameters = map;
    }


    /**
     * Get the init parameters data as an immutable Map.
     * Parameter names are keys, and parameter values are the values.
     *
     * @return The init parameters as an immutable map.
     */
    public Map getInitParameters() {
	return initParameters;
    }

    /**
     * Validate a JSP page.
     * This will get invoked once per directive in the JSP page.
     * This method will return null if the page is valid; otherwise
     * the method should return an array of ValidationMessage objects.
     * An array of length zero is also interpreted as no errors.
     *
     * @param prefix the value of the prefix argument in the directive
     * @param uri the value of the uri argument in the directive
     * @param thePage the JspData page object
     * @return A null object, or zero length array if no errors, an array
     * of ValidationMessages otherwise.
     */
    public ValidationMessage[] validate(String prefix, String uri, PageData page) {
	return null;
    }

    /**
     * Release any data kept by this instance for validation purposes
     */
    public void release() {
	initParameters = null;
    };

    // Private data
    private Map initParameters;

}
