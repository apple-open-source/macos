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


/**
 * The interface of a simple tag handler that does not want to manipulate its body.
 * The Tag interface defines the basic protocol between a Tag handler and
 * JSP page implementation class.  It defines the life cycle and the
 * methods to be invoked at start and end tag.
 *
 * <p><B>Properties</B>
 *
 * <p>
 * The  Tag interface specifies the setter and getter methods for the core
 * pageContext and parent properties.
 *
 * <p> The JSP page implementation object invokes setPageContext and
 * setParent, in that order, before invoking doStartTag() or doEndTag().
 *
 * <p><B>Methods</B>
 *
 * <p>
 * There are two main actions: doStartTag and doEndTag.  Once all
 * appropriate properties have been initialized, the doStartTag and
 * doEndTag methods can be invoked on the tag handler.  Between these
 * invocations, the tag handler is assumed to hold a state that must
 * be preserved.  After the doEndTag invocation, the tag handler is
 * available for further invocations (and it is expected to have
 * retained its properties).
 *
 * <p><B>Lifecycle</B>
 * <p> Lifecycle details are described by the transition diagram below,
 * with the following comments:
 * <ul>
 * <li> [1] This transition is intended to be for releasing long-term data.
 * no guarantees are assumed on whether any properties have been retained
 * or not.
 * <li> [2] This transition happens if and only if the tag ends normally
 * without raising an exception
 * <li> [3] Note that since there are no guarantees on the state of the
 * properties, a tag handler that had some optional properties set can only be
 * reused if those properties are set to a new (known) value.  This means
 * that tag handlers can only be reused within the same "AttSet" (set of
 * attributes that have been set).
 * <li> Check the TryCatchFinally interface for additional details related
 * to exception handling and resource management.
 * </ul>
 *
 * <IMG src="doc-files/TagProtocol.gif"/>
 * 
 * <p> Once all invocations on the tag handler
 * are completed, the release method is invoked on it.  Once a release
 * method is invoked <em>all</em> properties, including parent and
 * pageContext, are assumed to have been reset to an unspecified value.
 * The page compiler guarantees that release() will be invoked on the Tag
 * handler before the handler is released to the GC.
 *
 * <p><B>Empty and Non-Empty Action</B>
 * <p> If the TagLibraryDescriptor file indicates that the action must
 * always have an empty action, by an &lt;body-content&gt; entry of "empty",
 * then the doStartTag() method must return SKIP_BODY.
 *
 * Otherwise, the doStartTag() method may return SKIP_BODY or
 * EVAL_BODY_INCLUDE.
 *
 * <p>
 * If SKIP_BODY is returned the body, if present, is not evaluated.
 * 
 * <p>
 * If EVAL_BODY_INCLUDE is returned, the body is evaluated and
 * "passed through" to the current out.
*/

public interface Tag {

    /**
     * Skip body evaluation.
     * Valid return value for doStartTag and doAfterBody.
     */
 
    public final static int SKIP_BODY = 0;
 
    /**
     * Evaluate body into existing out stream.
     * Valid return value for doStartTag.
     */
 
    public final static int EVAL_BODY_INCLUDE = 1;

    /**
     * Skip the rest of the page.
     * Valid return value for doEndTag.
     */

    public final static int SKIP_PAGE = 5;

    /**
     * Continue evaluating the page.
     * Valid return value for doEndTag().
     */

    public final static int EVAL_PAGE = 6;

    // Setters for Tag handler data


    /**
     * Set the current page context.
     * This method is invoked by the JSP page implementation object
     * prior to doStartTag().
     * <p>
     * This value is *not* reset by doEndTag() and must be explicitly reset
     * by a page implementation if it changes between calls to doStartTag().
     *
     * @param pc The page context for this tag handler.
     */

    void setPageContext(PageContext pc);


    /**
     * Set the parent (closest enclosing tag handler) of this tag handler.
     * Invoked by the JSP page implementation object prior to doStartTag().
     * <p>
     * This value is *not* reset by doEndTag() and must be explicitly reset
     * by a page implementation.
     *
     * @param t The parent tag, or null.
     */


    void setParent(Tag t);


    /**
     * Get the parent (closest enclosing tag handler) for this tag handler.
     *
     * <p>
     * The getParent() method can be used to navigate the nested tag
     * handler structure at runtime for cooperation among custom actions;
     * for example, the findAncestorWithClass() method in TagSupport
     * provides a convenient way of doing this.
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
     * @returns the current parent, or null if none.
     * @seealso TagSupport.findAncestorWithClass().
     */

    Tag getParent();


    // Actions for basic start/end processing.


    /**
     * Process the start tag for this instance.
     * This method is invoked by the JSP page implementation object.
     *
     * <p>
     * The doStartTag method assumes that the properties pageContext and
     * parent have been set. It also assumes that any properties exposed as
     * attributes have been set too.  When this method is invoked, the body
     * has not yet been evaluated.
     *
     * <p>
     * This method returns Tag.EVAL_BODY_INCLUDE or
     * BodyTag.EVAL_BODY_BUFFERED to indicate
     * that the body of the action should be evaluated or SKIP_BODY to
     * indicate otherwise.
     *
     * <p>
     * When a Tag returns EVAL_BODY_INCLUDE the result of evaluating
     * the body (if any) is included into the current "out" JspWriter as it
     * happens and then doEndTag() is invoked.
     *
     * <p>
     * BodyTag.EVAL_BODY_BUFFERED is only valid  if the tag handler
     * implements BodyTag.
     *
     * <p>
     * The JSP container will resynchronize
     * any variable values that are indicated as so in TagExtraInfo after the
     * invocation of doStartTag().
     *
     * @returns EVAL_BODY_INCLUDE if the tag wants to process body, SKIP_BODY if it
     * does not want to process it.
     * @throws JspException.
     * @see BodyTag
     */
 
    int doStartTag() throws JspException;
 

    /**
     * Process the end tag for this instance.
     * This method is invoked by the JSP page implementation object
     * on all Tag handlers.
     *
     * <p>
     * This method will be called after returning from doStartTag. The
     * body of the action may or not have been evaluated, depending on
     * the return value of doStartTag.
     *
     * <p>
     * If this method returns EVAL_PAGE, the rest of the page continues
     * to be evaluated.  If this method returns SKIP_PAGE, the rest of
     * the page is not evaluated and the request is completed.  If this
     * request was forwarded or included from another page (or Servlet),
     * only the current page evaluation is completed.
     *
     * <p>
     * The JSP container will resynchronize
     * any variable values that are indicated as so in TagExtraInfo after the
     * invocation of doEndTag().
     *
     * @returns indication of whether to continue evaluating the JSP page.
     * @throws JspException.
     */

    int doEndTag() throws JspException;

    /**
     * Called on a Tag handler to release state.
     * The page compiler guarantees that JSP page implementation
     * objects will invoke this method on all tag handlers,
     * but there may be multiple invocations on doStartTag and doEndTag in between.
     */

    void release();

}
