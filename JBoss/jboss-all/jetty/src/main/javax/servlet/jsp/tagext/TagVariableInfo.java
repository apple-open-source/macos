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
 * Variable information for a tag in a Tag Library;
 * This class is instantiated from the Tag Library Descriptor file (TLD)
 * and is available only at translation time.
 *
 * This object should be immutable.
 *
 * This information is only available in JSP 1.2 format
*/

public class TagVariableInfo {

    /**
     * Constructor for TagVariableInfo
     *
     * @param nameGiven value of &lt;name-given&gt;
     * @param nameFromAttribute value of &lt;name-from-attribute&gt;
     * @param className value of &lt;variable-class&gt;
     * @param declare value of &lt;declare&gt;
     * @param scope value of &lt;scope&gt;
     */
    public TagVariableInfo(
	    String nameGiven,
	    String nameFromAttribute,
	    String className,
	    boolean declare,
	    int scope) {
	this.nameGiven         = nameGiven;
	this.nameFromAttribute = nameFromAttribute;
	this.className         = className;
	this.declare           = declare;
	this.scope             = scope;
    }
			 
    /**
     * The body of the &lt;name-given&gt; element
     *
     * @return The variable name as a constant
     */

    public String getNameGiven() {
	return nameGiven;
    }

    /**
     * The body of the &lt;name-from-attribute&gt; element.
     * This is the name of an attribute whose (translation-time)
     * value will give the name of the variable.  One of
     * &lt;name-given&gt; or &lt;name-from-attribute&gt; is required.
     *
     * @return The attribute whose value defines the variable name
     */

    public String getNameFromAttribute() {
	return nameFromAttribute;
    }

    /**
     * The body of the &lt;variable-class&gt; element.  
     *
     * @return The name of the class of the variable
     */

    public String getClassName() {
	return className;
    }

    /**
     * The body of the &lt;declare&gt; element
     *
     * @return Whether the variable is to be declared or not
     */

    public boolean getDeclare() {
	return declare;
    }

    /**
     * The body of the &lt;scope&gt; element
     *
     * @return The scope to give the variable.
     */

    public int getScope() {
	return scope;
    }


    /*
     * private fields
     */

    private String   nameGiven;         // <name-given>
    private String   nameFromAttribute; // <name-from-attribute>
    private String   className;         // <class>
    private boolean  declare;           // <declare>
    private int      scope;             // <scope>
}
