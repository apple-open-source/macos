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


/**
 * A validation message from a TagLibraryValidator.
 * <p>
 * A JSP container may (optionally) support a jsp:id attribute
 * to provide higher quality validation errors.
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

public class ValidationMessage {

    /**
     * Create a ValidationMessage.  The message String should be
     * non-null.  The value of id may be null, if the message
     * is not specific to any XML element, or if no jsp:id
     * attributes were passed on.  If non-null, the value of
     * id must be the value of a jsp:id attribute for the PageData
     * passed into the validate() method.
     *
     * @param id Either null, or the value of a jsp:id attribute.
     * @param message A localized validation message.
     */
    public ValidationMessage(String id, String message) {
	this.id = id;
	this.message = message;
    }


    /**
     * Get the jsp:id.
     * Null means that there is no information available.
     *
     * @return The jsp:id information.
     */
    public String getId() {
	return id;
    }

    /**
     * Get the localized validation message.
     *
     * @return A validation message
     */
    public String getMessage(){
	return message;
    }

    // Private data
    private String id;
    private String message;
}
