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
 * Information on the attributes of a Tag, available at translation time.
 * This class is instantiated from the Tag Library Descriptor file (TLD).
 *
 * <p>
 * Only the information needed to generate code is included here.  Other information
 * like SCHEMA for validation belongs elsewhere.
 */

public class TagAttributeInfo {
    /**
     * "id" is wired in to be ID.  There is no real benefit in having it be something else
     * IDREFs are not handled any differently.
     */

    public static final String ID = "id";

    /**
     * Constructor for TagAttributeInfo.
     * This class is to be instantiated only from the
     * TagLibrary code under request from some JSP code that is parsing a
     * TLD (Tag Library Descriptor).
     *
     * @param name The name of the attribute.
     * @param required If this attribute is required in tag instances.
     * @param type The name of the type of the attribute.
     * @param reqTime Whether this attribute holds a request-time Attribute.
     */

    public TagAttributeInfo(String name, boolean required,
                            String type, boolean reqTime)
    {
	this.name = name;
        this.required = required;
        this.type = type;
	this.reqTime = reqTime;
    }

    /**
     * The name of this attribute.
     *
     * @return the name of the attribute
     */

    public String getName() {
	return name;
    }

    /**
     * The type (as a String) of this attribute.
     *
     * @return the type of the attribute
     */

    public String getTypeName() {
	return type;
    }

    /**
     * Whether this attribute can hold a request-time value.
     *
     * @return if the attribute can hold a request-time value.
     */

    public boolean canBeRequestTime() {
	return reqTime;
    }

    /**
     * Whether this attribute is required.
     *
     * @return if the attribute is required.
     */
    public boolean isRequired() {
        return required;
    }

    /**
     * Convenience static method that goes through an array of TagAttributeInfo
     * objects and looks for "id".
     *
     * @param a An array of TagAttributeInfo
     * @return The TagAttributeInfo reference with name "id"
     */
    public static TagAttributeInfo getIdAttribute(TagAttributeInfo a[]) {
	for (int i=0; i<a.length; i++) {
	    if (a[i].getName().equals(ID)) {
		return a[i];
	    }
	}
	return null;		// no such attribute
    }

    public String toString() {
        StringBuffer b = new StringBuffer();
        b.append("name = "+name+" ");
        b.append("type = "+type+" ");
	b.append("reqTime = "+reqTime+" ");
        b.append("required = "+required+" ");
        return b.toString();
    }

    /*
     * fields
     */

    private String name;
    private String type;
    private boolean reqTime;
    private boolean required;
}
