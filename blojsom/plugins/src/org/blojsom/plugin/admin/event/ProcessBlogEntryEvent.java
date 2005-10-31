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
package org.blojsom.plugin.admin.event;

import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;

import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.util.Date;
import java.util.Map;

/**
 * Process blog entry event contains information about a blog entry with hooks for retrieving the servlet request,
 * response, and the current plugin execution context.
 *
 * @author David Czarnecki
 * @since blojsom 2.24
 * @version $Id: ProcessBlogEntryEvent.java,v 1.1.2.1 2005/07/21 04:30:24 johnan Exp $
 */
public class ProcessBlogEntryEvent extends BlogEntryEvent {
    
    private Log _logger = LogFactory.getLog(ProcessBlogEntryEvent.class);

    protected HttpServletRequest _httpServletRequest;
    protected HttpServletResponse _httpServletResponse;
    protected Map _context;

    /**
     * Create a new event indicating something happened with an entry in the system.
     *
     * @param source Source of the event
     * @param timestamp Event timestamp
     * @param blogEntry {@link BlogEntry}
     * @param blogUser {@link BlogUser}
     */
    public ProcessBlogEntryEvent(Object source, Date timestamp, BlogEntry blogEntry, BlogUser blogUser, 
                                 HttpServletRequest httpServletRequest, HttpServletResponse httpServletResponse,
                                 Map context) {
        super(source, timestamp, blogEntry, blogUser);

        _httpServletRequest = httpServletRequest;
        _httpServletResponse = httpServletResponse;
        _context = context;
    }

    /**
     * Retrieve the servlet request
     *
     * @return {@link HttpServletRequest}
     */
    public HttpServletRequest getHttpServletRequest() {
        return _httpServletRequest;
    }

    /**
     * Retrieve the servlet response
     *
     * @return {@link HttpServletResponse}
     */
    public HttpServletResponse getHttpServletResponse() {
        return _httpServletResponse;
    }

    /**
     * Retrieve the plugin execution context
     *
     * @return Context map
     */
    public Map getContext() {
        return _context;
    }
}