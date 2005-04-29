/**
 * Copyright (c) 2003-2004, David A. Czarnecki
 * All rights reserved.
 *
 * Portions Copyright (c) 2003-2004 by Mark Lussier
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 * Neither the name of the "David A. Czarnecki" and "blojsom" nor the names of
 * its contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * Products derived from this software may not be called "blojsom",
 * nor may "blojsom" appear in their name, without prior written permission of
 * David A. Czarnecki.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
package org.blojsom.extension.atomapi;

/**
 * AtomAPIConstants
 *
 * @author Mark Lussier
 * @version $Id: AtomAPIConstants.java,v 1.2 2004/08/27 00:49:41 whitmore Exp $
 * @since blojsom 2.0
 */
public interface AtomAPIConstants {

    /**
     * Default file extension for blog entries written via AtomAPI
     */
    static final String DEFAULT_BLOG_ATOMAPI_ENTRY_EXTENSION = ".txt";

    /**
     * Initialization parameter for setting blog entries written via AtomAPI
     */
    static final String BLOG_ATOMAPI_ENTRY_EXTENSION_IP = "blog-atomapi-entry-extension";

    /**
     * Header value prefix for Atom authentication realm
     */
    static final String ATOM_AUTH_PREFIX = "Atom ";

    /**
     * Authentication realm
     */
    //static final String AUTHENTICATION_REALM = "Atom realm=\"blojsom\", qop=\"atom-auth\", algorith=\"SHA\", nonce=\"{0}\"";
    static final String AUTHENTICATION_REALM = "WSSE realm=\"blojsom\", profile=\"UsernameToken\"";

    /**
     * Response header for authentication challenge
     */
    static final String HEADER_WWWAUTHENTICATE = "WWW-Authenticate";

    /**
     * Response header for path
     */
    static final String HEADER_LOCATION = "Location";

    /**
     * Inbound request header with authentication credentials
     */
    static final String HEADER_AUTHORIZATION = "Authorization";

    /**
     * Atom namespace
     */
    static final String ATOM_NAMESPACE = "\"http://purl.org/atom/ns#\"";

    /**
     * Atom authorization header
     */
    static final String ATOMHEADER_AUTHORIZATION = "X-Atom-Authorization";

    /**
     * Atom WSSE authorization header
     */
    static final String ATOMHEADER_WSSE_AUTHORIZATION = "X-WSSE";

    /**
     * Atom authentication info header
     */
    static final String ATOMHEADER_AUTHENTICATION_INFO = "X-Atom-Authentication-Info";

    /**
     * Atom nextnonce token
     */
    static final String ATOM_TOKEN_NEXTNONCE = "nextnonce=\"";

    /**
     * Atom content-type
     */
    static final String CONTENTTYPE_ATOM = "application/x.atom+xml";

    /**
     * XML content-type
     */
    static final String CONTENTTYPE_XML = "application/xml";

    /**
     * HTML content-type
     */
    static final String CONTENTTYPE_HTML = "text/html";

    /**
     * Key for atom-all
     */
    static final String KEY_ATOMALL = "atom-all";

    /**
     * Key for atom-last
     */
    static final String KEY_ATOMLAST = "atom-last";

    /**
     * Default Atom API servlet mapping
     */
    static final String ATOM_SERVLETMAPPING = "/atomapi/";

    /**
     * SOAPAction header for SOAP AtomAPI client
     */
    static final String HEADER_SOAPACTION = "SOAPAction";
}
