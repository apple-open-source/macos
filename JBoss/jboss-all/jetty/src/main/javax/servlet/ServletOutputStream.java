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
 * ====================================================================
 *
 * This source code implements specifications defined by the Java
 * Community Process. In order to remain compliant with the specification
 * DO NOT add / change / or delete method signatures!
 */

package javax.servlet;

import java.io.OutputStream;
import java.io.IOException;
import java.io.CharConversionException;
import java.text.MessageFormat;
import java.util.ResourceBundle;

/**
 * Provides an output stream for sending binary data to the
 * client. A <code>ServletOutputStream</code> object is normally retrieved 
 * via the {@link ServletResponse#getOutputStream} method.
 *
 * <p>This is an abstract class that the servlet container implements.
 * Subclasses of this class
 * must implement the <code>java.io.OutputStream.write(int)</code>
 * method.
 *
 * 
 * @author 	Various
 * @version 	$Version$
 *
 * @see 	ServletResponse
 *
 */

public abstract class ServletOutputStream extends OutputStream {

    private static final String LSTRING_FILE = "javax.servlet.LocalStrings";
    private static ResourceBundle lStrings =
	ResourceBundle.getBundle(LSTRING_FILE);


    
    /**
     *
     * Does nothing, because this is an abstract class.
     *
     */

    protected ServletOutputStream() { }


    /**
     * Writes a <code>String</code> to the client, 
     * without a carriage return-line feed (CRLF) 
     * character at the end.
     *
     *
     * @param s			the <code>String</code to send to the client
     *
     * @exception IOException 	if an input or output exception occurred
     *
     */

    public void print(String s) throws IOException {
	if (s==null) s="null";
	int len = s.length();
	for (int i = 0; i < len; i++) {
	    char c = s.charAt (i);

	    //
	    // XXX NOTE:  This is clearly incorrect for many strings,
	    // but is the only consistent approach within the current
	    // servlet framework.  It must suffice until servlet output
	    // streams properly encode their output.
	    //
	    if ((c & 0xff00) != 0) {	// high order byte must be zero
		String errMsg = lStrings.getString("err.not_iso8859_1");
		Object[] errArgs = new Object[1];
		errArgs[0] = new Character(c);
		errMsg = MessageFormat.format(errMsg, errArgs);
		throw new CharConversionException(errMsg);
	    }
	    write (c);
	}
    }



    /**
     * Writes a <code>boolean</code> value to the client,
     * with no carriage return-line feed (CRLF) 
     * character at the end.
     *
     * @param b			the <code>boolean</code> value 
     *				to send to the client
     *
     * @exception IOException 	if an input or output exception occurred
     *
     */

    public void print(boolean b) throws IOException {
	String msg;
	if (b) {
	    msg = lStrings.getString("value.true");
	} else {
	    msg = lStrings.getString("value.false");
	}
	print(msg);
    }



    /**
     * Writes a character to the client,
     * with no carriage return-line feed (CRLF) 
     * at the end.
     *
     * @param c			the character to send to the client
     *
     * @exception IOException 	if an input or output exception occurred
     *
     */

    public void print(char c) throws IOException {
	print(String.valueOf(c));
    }




    /**
     *
     * Writes an int to the client,
     * with no carriage return-line feed (CRLF) 
     * at the end.
     *
     * @param i			the int to send to the client
     *
     * @exception IOException 	if an input or output exception occurred
     *
     */  

    public void print(int i) throws IOException {
	print(String.valueOf(i));
    }



 
    /**
     * 
     * Writes a <code>long</code> value to the client,
     * with no carriage return-line feed (CRLF) at the end.
     *
     * @param l			the <code>long</code> value 
     *				to send to the client
     *
     * @exception IOException 	if an input or output exception 
     *				occurred
     * 
     */

    public void print(long l) throws IOException {
	print(String.valueOf(l));
    }



    /**
     *
     * Writes a <code>float</code> value to the client,
     * with no carriage return-line feed (CRLF) at the end.
     *
     * @param f			the <code>float</code> value
     *				to send to the client
     *
     * @exception IOException	if an input or output exception occurred
     *
     *
     */

    public void print(float f) throws IOException {
	print(String.valueOf(f));
    }



    /**
     *
     * Writes a <code>double</code> value to the client,
     * with no carriage return-line feed (CRLF) at the end.
     * 
     * @param d			the <code>double</code> value
     *				to send to the client
     *
     * @exception IOException 	if an input or output exception occurred
     *
     */

    public void print(double d) throws IOException {
	print(String.valueOf(d));
    }



    /**
     * Writes a carriage return-line feed (CRLF)
     * to the client.
     *
     *
     *
     * @exception IOException 	if an input or output exception occurred
     *
     */

    public void println() throws IOException {
	print("\r\n");
    }



    /**
     * Writes a <code>String</code> to the client, 
     * followed by a carriage return-line feed (CRLF).
     *
     *
     * @param s			the </code>String</code> to write to the client
     *
     * @exception IOException 	if an input or output exception occurred
     *
     */

    public void println(String s) throws IOException {
	print(s);
	println();
    }




    /**
     *
     * Writes a <code>boolean</code> value to the client, 
     * followed by a 
     * carriage return-line feed (CRLF).
     *
     *
     * @param b			the <code>boolean</code> value 
     *				to write to the client
     *
     * @exception IOException 	if an input or output exception occurred
     *
     */

    public void println(boolean b) throws IOException {
	print(b);
	println();
    }



    /**
     *
     * Writes a character to the client, followed by a carriage
     * return-line feed (CRLF).
     *
     * @param c			the character to write to the client
     *
     * @exception IOException 	if an input or output exception occurred
     *
     */

    public void println(char c) throws IOException {
	print(c);
	println();
    }



    /**
     *
     * Writes an int to the client, followed by a 
     * carriage return-line feed (CRLF) character.
     *
     *
     * @param i			the int to write to the client
     *
     * @exception IOException 	if an input or output exception occurred
     *
     */

    public void println(int i) throws IOException {
	print(i);
	println();
    }



    /**  
     *
     * Writes a <code>long</code> value to the client, followed by a 
     * carriage return-line feed (CRLF).
     *
     *
     * @param l			the <code>long</code> value to write to the client
     *
     * @exception IOException 	if an input or output exception occurred
     *
     */  

    public void println(long l) throws IOException {
	print(l);
	println();
    }



    /**
     *
     * Writes a <code>float</code> value to the client, 
     * followed by a carriage return-line feed (CRLF).
     *
     * @param f			the <code>float</code> value 
     *				to write to the client
     *
     *
     * @exception IOException 	if an input or output exception 
     *				occurred
     *
     */

    public void println(float f) throws IOException {
	print(f);
	println();
    }



    /**
     *
     * Writes a <code>double</code> value to the client, 
     * followed by a carriage return-line feed (CRLF).
     *
     *
     * @param d			the <code>double</code> value
     *				to write to the client
     *
     * @exception IOException 	if an input or output exception occurred
     *
     */

    public void println(double d) throws IOException {
	print(d);
	println();
    }
}
