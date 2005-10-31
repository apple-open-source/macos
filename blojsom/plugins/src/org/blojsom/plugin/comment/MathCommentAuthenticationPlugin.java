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
package org.blojsom.plugin.comment;

import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.Blog;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.util.BlojsomUtils;
import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;

import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.http.HttpSession;
import java.util.Map;
import java.util.HashMap;

/**
 * Math comment authenticator plugin
 *
 * @author David Czarnecki
 * @version $Id: MathCommentAuthenticationPlugin.java,v 1.1.2.1 2005/07/21 04:30:28 johnan Exp $
 * @since blojsom 2.22
 */
public class MathCommentAuthenticationPlugin extends CommentModerationPlugin {

    private Log _logger = LogFactory.getLog(MathCommentAuthenticationPlugin.class);

    public static final String BLOJSOM_MATH_AUTHENTICATOR_PLUGIN_ANSWER = "BLOJSOM_MATH_AUTHENTICATOR_PLUGIN_ANSWER";
    public static final String BLOJSOM_MATH_AUTHENTICATOR_PLUGIN_VALUE1 = "BLOJSOM_MATH_AUTHENTICATOR_PLUGIN_VALUE1";
    public static final String BLOJSOM_MATH_AUTHENTICATOR_PLUGIN_VALUE2 = "BLOJSOM_MATH_AUTHENTICATOR_PLUGIN_VALUE2";
    public static final String BLOJSOM_MATH_AUTHENTICATOR_PLUGIN_OPERATION = "BLOJSOM_MATH_AUTHENTICATOR_PLUGIN_OPERATION";
    public static final String BLOJSOM_MATH_AUTHENTICATOR_PLUGIN_ANSWER_CHECK_PARAM = "mathAnswerCheck";
    public static final String BLOJSOM_MATH_AUTHENTICATOR_PLUGIN_STATUS_MESSAGE = "BLOJSOM_MATH_AUTHENTICATOR_PLUGIN_STATUS_MESSAGE";

    private static final int AVAILABLE_OPERATIONS = 3;

    /**
     * Math comment authenticator plugin
     */
    public MathCommentAuthenticationPlugin() {
    }

    /**
     * Simple check to see if comment moderation is enabled
     * <p/>
     * @param httpServletRequest  Request
     * @param httpServletResponse Response
     * @param user                {@link org.blojsom.blog.BlogUser} instance
     * @param context             Context
     * @param entries             Blog entries retrieved for the particular request
     * @throws org.blojsom.plugin.BlojsomPluginException
     *          If there is an error in moderating a comment
     */
    protected void moderateComment(HttpServletRequest httpServletRequest, HttpServletResponse httpServletResponse, BlogUser user, Map context, BlogEntry[] entries) throws BlojsomPluginException {
        Blog blog = user.getBlog();

        if ("true".equalsIgnoreCase(blog.getBlogProperty(COMMENT_MODERATION_ENABLED))) {
            HttpSession httpSession = httpServletRequest.getSession();

            if ("y".equalsIgnoreCase(httpServletRequest.getParameter(CommentPlugin.COMMENT_PARAM)) && user.getBlog().getBlogCommentsEnabled().booleanValue()) {
                String mathAnswerCheck = BlojsomUtils.getRequestValue(BLOJSOM_MATH_AUTHENTICATOR_PLUGIN_ANSWER_CHECK_PARAM, httpServletRequest);

                boolean passedCheck = false;
                if (httpSession.getAttribute(BLOJSOM_MATH_AUTHENTICATOR_PLUGIN_ANSWER) != null) {
                    Integer mathAnswer = (Integer) httpSession.getAttribute(BLOJSOM_MATH_AUTHENTICATOR_PLUGIN_ANSWER);

                    try {
                        int mathAnswerCheckValue = Integer.parseInt(mathAnswerCheck);
                        int originalMathAnswerValue = mathAnswer.intValue();

                        if (mathAnswerCheckValue == originalMathAnswerValue) {
                            passedCheck = true;
                        }
                    } catch (NumberFormatException e) {
                    }
                }

                if (!passedCheck) {
                    Map commentMetaData;
                    if (context.containsKey(CommentPlugin.BLOJSOM_PLUGIN_COMMENT_METADATA)) {
                        commentMetaData = (Map) context.get(CommentPlugin.BLOJSOM_PLUGIN_COMMENT_METADATA);
                    } else {
                        commentMetaData = new HashMap();
                    }
                    
                    commentMetaData.put(CommentPlugin.BLOJSOM_PLUGIN_COMMENT_METADATA_DESTROY, Boolean.TRUE);
                    context.put(CommentPlugin.BLOJSOM_PLUGIN_COMMENT_METADATA, commentMetaData);

                    _logger.info("Failed math comment authentication check.");
                    context.put(BLOJSOM_MATH_AUTHENTICATOR_PLUGIN_STATUS_MESSAGE, "Failed math comment authentication check.");
                }
            }

            int operation = (int) (Math.random() * AVAILABLE_OPERATIONS);
            int value1 = (int) (Math.random() * 50.0);
            int value2 = (int) (Math.random() * 50.0);
            int answer;

            answer = getAnswerForOperation(value1, value2, operation);

            httpSession.setAttribute(BLOJSOM_MATH_AUTHENTICATOR_PLUGIN_VALUE1, new Integer(value1));
            httpSession.setAttribute(BLOJSOM_MATH_AUTHENTICATOR_PLUGIN_VALUE2, new Integer(value2));
            httpSession.setAttribute(BLOJSOM_MATH_AUTHENTICATOR_PLUGIN_ANSWER, new Integer(answer));
            httpSession.setAttribute(BLOJSOM_MATH_AUTHENTICATOR_PLUGIN_OPERATION, getOperatorForOperation(operation));
        }
    }

    /**
     * Return the result from the specified operation where 0 = addition, 1 = subtraction, 2 = multiplication
     *
     * @param value1 Value 1
     * @param value2 Value 2
     * @param operation Operation where 0 = addition, 1 = subtraction, 2 = multiplication
     * @return Value of operation
     */
    protected int getAnswerForOperation(int value1, int value2, int operation) {
        int answer;

        switch (operation) {
            case 0:
                {
                    answer = value1 + value2;
                    break;
                }
            case 1:
                {
                    answer = value1 - value2;
                    break;
                }
            case 2:
                {
                    answer = value1 * value2;
                    break;
                }
            default:
                {
                    answer = value1 + value2;
                }
        }

        return answer;
    }

    /**
     * Return the appropriate operator for the operation
     *
     * @param operation Operation where 0 = addition, 1 = subtraction, 2 = multiplication
     * @return + for addition, - for subtraction, and * for multiplication
     */
    protected String getOperatorForOperation(int operation) {
        switch (operation) {
            case 0:
                {
                    return "+";
                }
            case 1:
                {
                    return "-";
                }
            case 2:
                {
                    return "*";
                }
            default:
                {
                    return "+";
                }
        }
    }
}