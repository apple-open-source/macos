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

import javax.servlet.jsp.tagext.TagInfo;

import java.net.URL;

import java.io.InputStream;

/**
 * Translation-time information associated with a taglib directive, and its
 * underlying TLD file.
 *
 * Most of the information is directly from the TLD, except for
 * the prefix and the uri values used in the taglib directive
 *
 *
 */

abstract public class TagLibraryInfo {

    /**
     * Constructor.
     *
     * This will invoke the constructors for TagInfo, and TagAttributeInfo
     * after parsing the TLD file.
     *
     * @param prefix the prefix actually used by the taglib directive
     * @param uri the URI actually used by the taglib directive
     */

    protected TagLibraryInfo(String prefix, String uri) {
	this.prefix = prefix;
	this.uri    = uri;
    }


    // ==== methods accessing taglib information =======

    /**
     * The value of the uri attribute from the <%@ taglib directive for this library.
     *
     * @returns the value of the uri attribute
     */
   
    public String getURI() {
        return uri;
    }

    /**
     * The prefix assigned to this taglib from the <%taglib directive
     *
     * @returns the prefix assigned to this taglib from the <%taglib directive
     */

    public String getPrefixString() {
	return prefix;
    }

    // ==== methods using the TLD data =======

    /**
     * The preferred short name (prefix) as indicated in the TLD.
     * This may be used by authoring tools as the preferred prefix
     * to use when creating an include directive for this library.
     *
     * @returns the preferred short name for the library
     */
    public String getShortName() {
        return shortname;
    }

    /**
     * The "reliable" URN indicated in the TLD.
     * This may be used by authoring tools as a global identifier
     * (the uri attribute) to use when creating a taglib directive
     * for this library.
     *
     * @returns a reliable URN to a TLD like this
     */
    public String getReliableURN() {
        return urn;
    }


    /**
     * Information (documentation) for this TLD.
     *
     * @returns the info string for this tag lib
     */
   
    public String getInfoString() {
        return info;
    }


    /**
     * A string describing the required version of the JSP container.
     * 
     * @returns the (minimal) required version of the JSP container.
     * @seealso JspEngineInfo.
     */
   
    public String getRequiredVersion() {
        return jspversion;
    }


    /**
     * An array describing the tags that are defined in this tag library.
     *
     * @returns the tags defined in this tag lib
     */
   
    public TagInfo[] getTags() {
        return tags;
    }


    /**
     * Get the TagInfo for a given tag name, looking through all the
     * tags in this tag library.
     *
     * @param shortname The short name (no prefix) of the tag
     * @returns the TagInfo for that tag. 
     */

    public TagInfo getTag(String shortname) {
        TagInfo tags[] = getTags();

        if (tags == null || tags.length == 0) {
            System.err.println("No tags");
            return null;
        }

        for (int i=0; i < tags.length; i++) {
            if (tags[i].getTagName().equals(shortname)) {
                return tags[i];
            }
        }
        return null;
    }


    // Protected fields

    protected String        prefix;
    protected String        uri;

    protected TagInfo[]     tags;

    // Tag Library Data
    protected String tlibversion; // required
    protected String jspversion;  // optional
    protected String shortname;   // required
    protected String urn;         // required
    protected String info;        // optional
}
