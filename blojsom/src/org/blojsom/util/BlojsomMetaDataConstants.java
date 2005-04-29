/**
 * Copyright (c) 2003-2004 , David A. Czarnecki
 * All rights reserved.
 *
 * Portions Copyright (c) 2003-2004  by Mark Lussier
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
package org.blojsom.util;

/**
 * BlojsomMetaDataConstants
 *
 * @author David Czarnecki
 * @version $Id: BlojsomMetaDataConstants.java,v 1.2 2004/08/27 01:13:56 whitmore Exp $
 * @since blojsom 2.04
 */
public interface BlojsomMetaDataConstants {

    /**
     * Entry Meta Data Key for Poster
     */
    public static final String BLOG_ENTRY_METADATA_AUTHOR = "blog-entry-author";

    /**
     * Extended Entry Meta Data Key for Poster
     * Used to store private meta-data so that the default templates wont render it, like an email address
     */
    public static final String BLOG_ENTRY_METADATA_AUTHOR_EXT = "blog-entry-author-ext";


    /**
     * Entry MetaData File Header
     */
    public static final String BLOG_METADATA_HEADER = "blojsom entry metadata";

    /**
     * Entry Attribute for File()
     */
    public static final String SOURCE_ATTRIBUTE = "blog-entry-source";

    /**
     * Entry meta-data key for entry time
     */
    public static final String BLOG_ENTRY_METADATA_TIMESTAMP = "blog-entry-metadata-timestamp";

    /**
     * Entry metadata key for disabling comments
     */
    public static final String BLOG_METADATA_COMMENTS_DISABLED = "blog-entry-comments-disabled";

    /**
     * Entry metadata key for disabling trackbacks
     */
    public static final String BLOG_METADATA_TRACKBACKS_DISABLED = "blog-entry-trackbacks-disabled";
}
