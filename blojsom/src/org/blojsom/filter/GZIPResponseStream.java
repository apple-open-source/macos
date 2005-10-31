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

import javax.servlet.ServletOutputStream;
import javax.servlet.http.HttpServletResponse;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.zip.GZIPOutputStream;

/**
 * GZIPResponseStream
 * <p/>
 * Copyright 2003 Jayson Falkner (jayson@jspinsider.com)
 * This code is from "Servlets and JavaServer pages; the J2EE Web Tier",
 * http://www.jspbook.com. You may freely use the code both commercially
 * and non-commercially. If you like the code, please pick up a copy of
 * the book and help support the authors, development of more free code,
 * and the JSP/Servlet/J2EE community.
 *
 * @version $Id: GZIPResponseStream.java,v 1.2.2.1 2005/07/21 14:11:03 johnan Exp $
 * @since blojsom 2.10
 */
public class GZIPResponseStream extends ServletOutputStream {

    protected ByteArrayOutputStream baos = null;
    protected GZIPOutputStream gzipstream = null;
    protected boolean closed = false;
    protected HttpServletResponse response = null;
    protected ServletOutputStream output = null;

    /**
     * Create a new GZIPResponseStream
     *
     * @param response Original HTTP servlet response
     * @throws IOException If there is an error creating the response stream
     */
    public GZIPResponseStream(HttpServletResponse response) throws IOException {
        super();

        closed = false;
        this.response = response;
        this.output = response.getOutputStream();
        baos = new ByteArrayOutputStream();
        gzipstream = new GZIPOutputStream(baos);
    }

    /**
     * Close this response stream
     *
     * @throws IOException If the stream is already closed or there is an error closing the stream
     */
    public void close() throws IOException {
        if (closed) {
            throw new IOException("This output stream has already been closed.");
        }

        gzipstream.finish();
        gzipstream.flush();
        gzipstream.close();

        byte[] bytes = baos.toByteArray();

        response.addHeader("Content-Length", Integer.toString(bytes.length));
        response.addHeader("Content-Encoding", "gzip");

        baos.flush();
        baos.close();

        output.write(bytes);
        output.flush();
        output.close();

        closed = true;
    }

    /**
     * Flush the response stream
     *
     * @throws IOException If the stream is already closed or there is an error flushing the stream
     */
    public void flush() throws IOException {
        if (closed) {
            throw new IOException("Cannot flush a closed output stream.");
        }

        gzipstream.flush();
    }

    /**
     * Write a byte to the stream
     *
     * @param b Byte to write
     * @throws IOException If the stream is closed or there is an error in writing
     */
    public void write(int b) throws IOException {
        if (closed) {
            throw new IOException("Cannot write to a closed output stream.");
        }

        gzipstream.write((byte) b);
    }

    /**
     * Write a byte array to the stream
     *
     * @param b Byte array to write
     * @throws IOException If the stream is closed or there is an error in writing
     */
    public void write(byte[] b) throws IOException {
        write(b, 0, b.length);
    }

    /**
     * Write a byte array to the stream
     *
     * @param b   Byte array to write
     * @param off Offset of starting point in byte array to start writing
     * @param len Length of bytes to write
     * @throws IOException If the stream is closed or there is an error in writing
     */
    public void write(byte[] b, int off, int len) throws IOException {
        if (closed) {
            throw new IOException("Cannot write to a closed output stream.");
        }

        gzipstream.write(b, off, len);
    }

    /**
     * Returns <code>true</code> if the stream is closed, <code>false</code> otherwise
     *
     * @return <code>true</code> if the stream is closed, <code>false</code> otherwise
     */
    public boolean closed() {
        return (this.closed);
    }

    /**
     * Reset the stream. Currently a no-op.
     */
    public void reset() {
    }
}
