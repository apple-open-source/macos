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
 * The auxiliary interface of a Tag, IterationTag or BodyTag tag
 * handler that wants additional hooks for managing resources.
 *
 * <p>This interface provides two new methods: doCatch(Throwable)
 * and doFinally().  The prototypical invocation is as follows:
 *
 * <pre>
 * h = get a Tag();  // get a tag handler, perhaps from pool
 *
 * h.setPageContext(pc);  // initialize as desired
 * h.setParent(null);
 * h.setFoo("foo");
 * 
 * // tag invocation protocol; see Tag.java
 * try {
 *   doStartTag()...
 *   ....
 *   doEndTag()...
 * } catch (Throwable t) {
 *   // react to exceptional condition
 *   h.doCatch(t);
 * } finally {
 *   // restore data invariants and release per-invocation resources
 *   h.doFinally();
 * }
 * 
 * ... other invocations perhaps with some new setters
 * ...
 * h.release();  // release long-term resources
 * </pre>
 */

public interface TryCatchFinally {

    /**
     * Invoked if a Throwable occurs while evaluating the BODY
     * inside a tag or in any of the following methods:
     * Tag.doStartTag(), Tag.doEndTag(),
     * IterationTag.doAfterBody() and BodyTag.doInitBody().
     *
     * <p>This method is not invoked if the Throwable occurs during
     * one of the setter methods.
     *
     * <p>This method may throw an exception (the same or a new one)
     * that will be propagated further the nest chain.  If an exception
     * is thrown, doFinally() will be invoked.
     *
     * <p>This method is intended to be used to respond to an exceptional
     * condition.
     *
     * @param t The throwable exception navigating through this tag.
     */
 
    void doCatch(Throwable t) throws Throwable;

    /**
     * Invoked in all cases after doEndTag() for any class implementing
     * Tag, IterationTag or BodyTag.  This method is invoked even if
     * an exception has occurred in the BODY of the tag,
     * or in any of the following methods:
     * Tag.doStartTag(), Tag.doEndTag(),
     * IterationTag.doAfterBody() and BodyTag.doInitBody().
     *
     * <p>This method is not invoked if the Throwable occurs during
     * one of the setter methods.
     *
     * <p>This method should not throw an Exception.
     *
     * <p>This method is intended to maintain per-invocation data
     * integrity and resource management actions.
     */

    void doFinally();
}
