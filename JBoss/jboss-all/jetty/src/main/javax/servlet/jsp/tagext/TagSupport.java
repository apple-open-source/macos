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

import javax.servlet.jsp.*;
import javax.servlet.jsp.tagext.*;

import javax.servlet.*;

import java.io.Writer;
import java.io.Serializable;

import java.util.Hashtable;
import java.util.Enumeration;

/**
 * A base class for defining new tag handlers implementing Tag.
 *
 * <p> The TagSupport class is a utility class intended to be used as
 * the base class for new tag handlers.  The TagSupport class
 * implements the Tag and IterationTag interfaces and adds additional
 * convenience methods including getter methods for the properties in
 * Tag.  TagSupport has one static method that is included to
 * facilitate coordination among cooperating tags.
 *
 * <p> Many tag handlers will extend TagSupport and only redefine a
 * few methods. 
 */

public class TagSupport implements IterationTag, Serializable {

    /**
     * Find the instance of a given class type that is closest to a given
     * instance.
     * This method uses the getParent method from the Tag
     * interface.
     * This method is used for coordination among cooperating tags.
     *
     * <p>
     * The current version of the specification only provides one formal
     * way of indicating the observable type of a tag handler: its
     * tag handler implementation class, described in the tag-class
     * subelement of the tag element.  This is extended in an
     * informal manner by allowing the tag library author to
     * indicate in the description subelement an observable type.
     * The type should be a subtype of the tag handler implementation
     * class or void.
     * This addititional constraint can be exploited by a
     * specialized container that knows about that specific tag library,
     * as in the case of the JSP standard tag library.
     *
     * <p>
     * When a tag library author provides information on the
     * observable type of a tag handler, client programmatic code
     * should adhere to that constraint.  Specifically, the Class
     * passed to findAncestorWithClass should be a subtype of the
     * observable type.
     * 
     *
     * @param from The instance from where to start looking.
     * @param klass The subclass of Tag or interface to be matched
     * @returns the nearest ancestor that implements the interface
     * or is an instance of the class specified
     */

    public static final Tag findAncestorWithClass(Tag from, Class klass) {
	boolean isInterface = false;

	if (from == null ||
	    klass == null ||
	    (!Tag.class.isAssignableFrom(klass) &&
	     !(isInterface = klass.isInterface()))) {
	    return null;
	}

	for (;;) {
	    Tag tag = from.getParent();

	    if (tag == null) {
		return null;
	    }

	    if ((isInterface && klass.isInstance(tag)) ||
	        klass.isAssignableFrom(tag.getClass()))
		return tag;
	    else
		from = tag;
	}
    }

    /**
     * Default constructor, all subclasses are required to define only
     * a public constructor with the same signature, and to call the
     * superclass constructor.
     *
     * This constructor is called by the code generated by the JSP
     * translator.
     */

    public TagSupport() { }

    /**
     * Default processing of the start tag, returning SKIP_BODY.
     *
     * @returns SKIP_BODY
     *
     * @see Tag#doStartTag()
     */
 
    public int doStartTag() throws JspException {
        return SKIP_BODY;
    }

    /**
     * Default processing of the end tag returning EVAL_PAGE.
     *
     * @returns EVAL_PAGE
     *
     * @see Tag#doEndTag()
     */

    public int doEndTag() throws JspException {
	return EVAL_PAGE;
    }


    /**
     * Default processing for a body
     *
     * @return SKIP_BODY
     *
     * @see IterationTag#doAfterBody()
     */
    
    public int doAfterBody() throws JspException {
	return SKIP_BODY;
    }

    // Actions related to body evaluation


    /**
     * Release state.
     *
     * @see Tag#release()
     */

    public void release() {
	parent          = null;
    }

    /**
     * Set the nesting tag of this tag.
     *
     * @param t The parent Tag.
     * @see Tag#setParent(Tag)
     */

    public void setParent(Tag t) {
	parent = t;
    }

    /**
     * The Tag instance most closely enclosing this tag instance.
     * @see Tag#getParent()
     *
     * @returns the parent tag instance or null
     */

    public Tag getParent() {
	return parent;
    }

    /**
     * Set the id attribute for this tag.
     *
     * @param id The String for the id.
     */

    public void setId(String id) {
	this.id = id;
    }

    /**
     * The value of the id attribute of this tag; or null.
     *
     * @returns the value of the id attribute, or null
     */
    
    public String getId() {
	return id;
    }

    /**
     * Set the page context.
     *
     * @param pageContenxt The PageContext.
     * @see Tag#setPageContext
     */

    public void setPageContext(PageContext pageContext) {
	this.pageContext = pageContext;
    }

    /**
     * Associate a value with a String key.
     *
     * @param k The key String.
     * @param o The value to associate.
     */

    public void setValue(String k, Object o) {
	if (values == null) {
	    values = new Hashtable();
	}
	values.put(k, o);
    }

    /**
     * Get a the value associated with a key.
     *
     * @param k The string key.
     * @returns The value associated with the key, or null.
     */

    public Object getValue(String k) {
	if (values == null) {
	    return null;
	} else {
	    return values.get(k);
	}
    }

    /**
     * Remove a value associated with a key.
     *
     * @param k The string key.
     */

    public void removeValue(String k) {
	if (values != null) {
	    values.remove(k);
	}
    }

    /**
     * Enumerate the values kept by this tag handler.
     *
     * @returns An enumeration of all the values set.
     */

    public Enumeration getValues() {
	if (values == null) {
	    return null;
	}
	return values.keys();
    }

    // private fields

    private   Tag         parent;
    private   Hashtable   values;
    protected String	  id;

    // protected fields

    protected PageContext pageContext;
}

