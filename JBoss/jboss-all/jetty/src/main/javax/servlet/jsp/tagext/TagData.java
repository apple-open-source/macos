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

import java.util.Hashtable;

/**
 * The (translation-time only) attribute/value information for a tag instance.
 *
 * <p>
 * TagData is only used as an argument to the isValid and getVariableInfo
 * methods of TagExtraInfo, which are invoked at translation time.
 */

public class TagData implements Cloneable {

    /**
     * Distinguished value for an attribute to indicate its value
     * is a request-time expression (which is not yet available because
     * TagData instances are used at translation-time).
     */

    public static final Object REQUEST_TIME_VALUE = new Object();


    /**
     * Constructor for TagData.
     *
     * <p>
     * A typical constructor may be
     * <pre>
     * static final Object[][] att = {{"connection", "conn0"}, {"id", "query0"}};
     * static final TagData td = new TagData(att);
     * </pre>
     *
     * All values must be Strings except for those holding the
     * distinguished object REQUEST_TIME_VALUE.

     * @param atts the static attribute and values.  May be null.
     */
    public TagData(Object[] atts[]) {
	if (atts == null) {
	    attributes = new Hashtable();
	} else {
	    attributes = new Hashtable(atts.length);
	}

	if (atts != null) {
	    for (int i = 0; i < atts.length; i++) {
		attributes.put(atts[i][0], atts[i][1]);
	    }
	}
    }

    /**
     * Constructor for a TagData.
     *
     * If you already have the attributes in a hashtable, use this
     * constructor. 
     *
     * @param attrs A hashtable to get the values from.
     */
    public TagData(Hashtable attrs) {
        this.attributes = attrs;
    }

    /**
     * The value of the id attribute, if available.
     *
     * @return the value of the id attribute or null
     */

    public String getId() {
	return getAttributeString(TagAttributeInfo.ID);
    }

    /**
     * The value of the attribute.
     * Returns the distinguished object REQUEST_TIME_VALUE if
     * the value is request time. Returns null if the attribute is not set.
     *
     * @return the attribute's value object
     */

    public Object getAttribute(String attName) {
	return attributes.get(attName);
    }

    /**
     * Set the value of an attribute.
     *
     * @param attName the name of the attribute
     * @param value the value.
     */
    public void setAttribute(String attName,
			     Object value) {
	attributes.put(attName, value);
    }

    /**
     * Get the value for a given attribute.
     *
     * @return the attribute value string
     * @throw ClassCastException if attribute value is not a String
     */

    public String getAttributeString(String attName) {
	Object o = attributes.get(attName);
	if (o == null) {
	    return null;
	} else {
	    return (String) o;
	}	
    }

    /**
     * Enumerates the attributes.
     *
     *@return An enumeration of the attributes in a TagData
     */
    public java.util.Enumeration getAttributes() {
        return attributes.keys();
    };

    // private data

    private Hashtable attributes;	// the tagname/value map
}
