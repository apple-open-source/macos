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
package org.blojsom.plugin.comment;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;

/**
 * Comment Support Methods.
 *
 * @author Mark Lussier
 * @version $Id: CommentUtils.java,v 1.2 2004/08/27 01:06:37 whitmore Exp $
 */
public class CommentUtils {

    /**
     * Logger Instance
     */
    private static Log _logger = LogFactory.getLog(CommentUtils.class);

    /**
     * Format the comment email
     *
     * @param permalink Blog entry permalink
     * @param author Comment entry author
     * @param authorEmail Comment author email
     * @param authorURL Comment author URL
     * @param userComment Comment
     * @param url Entry URL
     * @return the comment message as a string
     */
    public static String constructCommentEmail(String permalink, String author, String authorEmail, String authorURL,
                                               String userComment, String url) {

        StringBuffer emailcomment = new StringBuffer();
        emailcomment.append("Comment on: ").append(url);
        emailcomment.append("?permalink=").append(permalink).append("&page=comments").append("\n");

        if (author != null && !author.equals("")) {
            emailcomment.append("Comment by: ").append(author).append("\n");
        }
        if (authorEmail != null && !authorEmail.equals("")) {
            emailcomment.append("            ").append(authorEmail).append("\n");
        }
        if (authorURL != null && !authorURL.equals("")) {
            emailcomment.append("            ").append(authorURL).append("\n");
        }

        emailcomment.append("\n==[ Comment ]==========================================================").append("\n\n");
        emailcomment.append(userComment);

        return emailcomment.toString();

    }


}
