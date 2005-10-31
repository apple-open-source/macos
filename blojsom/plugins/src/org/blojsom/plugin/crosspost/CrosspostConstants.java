/**
 * Copyright (c) 2003-2005, David A. Czarnecki
 * All rights reserved.
 *
 * Portions Copyright (c) 2003-2005 by Mark Lussier
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
package org.blojsom.plugin.crosspost;

/**
 * CrosspostConstants
 *
 * @author Mark Lussier
 * @version $Id: CrosspostConstants.java,v 1.1.2.1 2005/07/21 04:30:29 johnan Exp $
 * @since blojsom 2.23
 */
public class CrosspostConstants {

    /**
     * Blogger API type
     */
    public static final int API_BLOGGER = 0;

    /**
     * MetaWeblog API type
     */
    public static final int API_METAWEBLOG = 1;

    /**
     * Blogger API
     */
    public static final String TYPE_BLOGGER = "blogger";

    /**
     * MetaWeblog API
     */
    public static final String TYPE_METAWEBLOG = "metaweblog";

    /**
     * Destinations configuration parameter
     */
    public static final String XPOST_TAG_DESTINATIONS = "destinations";
    
    /**
     * Destination title
     */
    public static final String XPOST_TAG_TITLE = ".title";

    /**
     * Destination type
     */
    public static final String XPOST_TAG_TYPE = ".type";


    /**
     * Destination XML-RPC URL
     */
    public static final String XPOST_TAG_URL = ".url";

    /**
     * Destination user ID
     */
    public static final String XPOST_TAG_USERID = ".userid";

    /**
     * Destination password for user ID
     */
    public static final String XPOST_TAG_PASSWORD = ".password";

    /**
     * Destination category
     */
    public static final String XPOST_TAG_CATEGORY = ".category";

    /**
     * Destination blog ID
     */
    public static final String XPOST_TAG_BLOGID  =".blogid";
}
