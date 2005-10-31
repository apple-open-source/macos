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
package org.blojsom.plugin.response.event;

import org.blojsom.blog.BlogUser;
import org.blojsom.event.BlojsomEvent;

import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.util.Date;
import java.util.Map;

/**
 * Response submission event
 *
 * @author David Czarnecki
 * @since blojsom 2.25
 * @version $Id: ResponseSubmissionEvent.java,v 1.1.2.1 2005/07/21 04:30:38 johnan Exp $
 */
public class ResponseSubmissionEvent extends BlojsomEvent {

    protected HttpServletRequest _httpServletRequest;
    protected HttpServletResponse _httpServletResponse;
    protected String _submitter;
    protected String _submitterItem1;
    protected String _submitterItem2;
    protected String _content;
    protected Map _metaData;
    protected BlogUser _blog;

    /**
     * Create a new instance of the response submission event
     *
     * @param source Source of event
     * @param timestamp Time of event
     * @param blog {@link BlogUser}
     * @param httpServletRequest {@link HttpServletRequest}
     * @param httpServletResponse {@link HttpServletResponse}
     * @param submitter Submitter
     * @param submitterItem1 Submitter data item 1
     * @param submitterItem2 Submitter data item 2
     * @param content Content to be evaluated
     * @param metaData Meta-data
     */
    public ResponseSubmissionEvent(Object source, Date timestamp, BlogUser blog, HttpServletRequest httpServletRequest, HttpServletResponse httpServletResponse, String submitter, String submitterItem1, String submitterItem2, String content, Map metaData) {
        super(source, timestamp);

        _blog = blog;
        _httpServletRequest = httpServletRequest;
        _httpServletResponse = httpServletResponse;
        _submitter = submitter;
        _submitterItem1 = submitterItem1;
        _submitterItem2 = submitterItem2;
        _content = content;
        _metaData = metaData;
    }

    /**
     * Retrieve the submitter
     *
     * @return Submitter
     */
    public String getSubmitter() {
        return _submitter;
    }

    /**
     * Retrieve the submitter item #1
     *
     * @return Submitter item #1
     */
    public String getSubmitterItem1() {
        return _submitterItem1;
    }

    /**
     * Retrieve the submitter item #2
     *
     * @return Submitter item #2
     */
    public String getSubmitterItem2() {
        return _submitterItem2;
    }

    /**
     * Retrieve the submission content
     *
     * @return Submission content
     */
    public String getContent() {
        return _content;
    }

    /**
     * Retrieve the meta-data associated with the submission
     *
     * @return Meta-data associated with the submission
     */
    public Map getMetaData() {
        return _metaData;
    }

    /**
     * Retrieve the {@link BlogUser}
     *
     * @return {@link BlogUser}
     */
    public BlogUser getBlog() {
        return _blog;
    }

    /**
     * Retrieve the {@link HttpServletRequest}
     *
     * @return {@link HttpServletRequest}
     */
    public HttpServletRequest getHttpServletRequest() {
        return _httpServletRequest;
    }

    /**
     * Retrieve the {@link HttpServletResponse}
     *
     * @return {@link HttpServletResponse}
     */
    public HttpServletResponse getHttpServletResponse() {
        return _httpServletResponse;
    }
}