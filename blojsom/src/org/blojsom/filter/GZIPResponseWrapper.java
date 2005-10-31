/**
 * Copyright (c) 2003-2005 , David A. Czarnecki
 * All rights reserved.
 *
 * Portions Copyright (c) 2003-2005  by Mark Lussier
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
package org.blojsom.filter;

import org.blojsom.util.BlojsomConstants;

import javax.servlet.ServletOutputStream;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.http.HttpServletResponseWrapper;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;

/**
 * GZIPResponseWrapper
 * <p/>
 * Copyright 2003 Jayson Falkner (jayson@jspinsider.com)
 * This code is from "Servlets and JavaServer pages; the J2EE Web Tier",
 * http://www.jspbook.com. You may freely use the code both commercially
 * and non-commercially. If you like the code, please pick up a copy of
 * the book and help support the authors, development of more free code,
 * and the JSP/Servlet/J2EE community.
 *
 * @version $Id: GZIPResponseWrapper.java,v 1.2.2.1 2005/07/21 14:11:03 johnan Exp $
 * @since blojsom 2.10
 */
public class GZIPResponseWrapper extends HttpServletResponseWrapper {

    protected HttpServletResponse origResponse = null;
    protected ServletOutputStream stream = null;
    protected PrintWriter writer = null;

    /**
     * Create a new GZIPResponseWrapper
     *
     * @param response Original HTTP servlet response
     */
    public GZIPResponseWrapper(HttpServletResponse response) {
        super(response);
        origResponse = response;
    }

    /**
     * Create a new ServletOutputStream which returns a GZIPResponseStream
     *
     * @return GZIPResponseStream object
     * @throws IOException If there is an error creating the response stream
     */
    public ServletOutputStream createOutputStream() throws IOException {
        return (new GZIPResponseStream(origResponse));
    }

    /**
     * Finish the response
     */
    public void finishResponse() throws IOException {
        if (writer != null) {
            writer.close();
        } else {
            if (stream != null) {
                stream.close();
            }
        }
    }

    /**
     * Flush the output buffer
     *
     * @throws IOException If there is an error flushing the buffer
     */
    public void flushBuffer() throws IOException {
        stream.flush();
    }

    /**
     * Retrieve the output stream for this response wrapper
     *
     * @return {@link #createOutputStream()}
     * @throws IOException If there is an error retrieving the output stream
     */
    public ServletOutputStream getOutputStream() throws IOException {
        if (writer != null) {
            throw new IllegalStateException("getWriter() has already been called.");
        }

        if (stream == null)
            stream = createOutputStream();

        return (stream);
    }

    /**
     * Retrieve a writer for this response wrapper
     *
     * @return PrintWriter that wraps an OutputStreamWriter (using UTF-8 as encoding)
     * @throws IOException If there is an error retrieving the writer
     */
    public PrintWriter getWriter() throws IOException {
        if (writer != null) {
            return (writer);
        }

        if (stream != null) {
            throw new IllegalStateException("getOutputStream() has already been called.");
        }

        stream = createOutputStream();
        writer = new PrintWriter(new OutputStreamWriter(stream, BlojsomConstants.UTF8));

        return (writer);
    }

    /**
     * Set the content length for the response. Currently a no-op.
     *
     * @param length Content length
     */
    public void setContentLength(int length) {
    }
}
