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

import javax.servlet.http.HttpServletResponse;
import javax.servlet.http.HttpServletResponseWrapper;
import java.io.CharArrayWriter;
import java.io.PrintWriter;

/**
 * A response wrapper that takes everything the client
 * would normally output and saves it in one big
 * character array.
 * <p></p>
 * Taken from More Servlets and JavaServer Pages
 * from Prentice Hall and Sun Microsystems Press,
 * <a href="http://www.moreservlets.com/">http://www.moreservlets.com/</a>.
 * &copy; 2002 Marty Hall; may be freely used or adapted.
 *
 * @author Marty Hall
 * @author David Czarnecki
 * @version $Id: CharArrayWrapper.java,v 1.1.2.1 2005/07/21 14:11:03 johnan Exp $
 * @since blojsom 2.24  
 */
public class CharArrayWrapper extends HttpServletResponseWrapper {

    private CharArrayWriter charWriter;

    /**
     * Initializes wrapper.
     * <p></p>
     * First, this constructor calls the parent
     * constructor. That call is crucial so that the response
     * is stored and thus setHeader, setStatus, addCookie,
     * and so forth work normally.
     * <p></p>
     * Second, this constructor creates a CharArrayWriter
     * that will be used to accumulate the response.
     */
    public CharArrayWrapper(HttpServletResponse response) {
        super(response);
        charWriter = new CharArrayWriter();
    }

    /**
     * When servlets or JSP pages ask for the Writer,
     * don't give them the real one. Instead, give them
     * a version that writes into the character array.
     * The filter needs to send the contents of the
     * array to the client (perhaps after modifying it).
     */
    public PrintWriter getWriter() {
        return (new PrintWriter(charWriter));
    }

    /**
     * Get a String representation of the entire buffer.
     * <p></p>
     * Be sure <b>not</b> to call this method multiple times
     * on the same wrapper. The API for CharArrayWriter
     * does not guarantee that it "remembers" the previous
     * value, so the call is likely to make a new String
     * every time.
     */
    public String toString() {
        return (charWriter.toString());
    }

    /**
     * Get the underlying character array.
     */
    public char[] toCharArray() {
        return (charWriter.toCharArray());
    }
}