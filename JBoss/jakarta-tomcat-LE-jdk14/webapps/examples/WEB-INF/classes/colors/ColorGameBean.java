/*
 * ====================================================================
 *
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
 * [Additional notices, if required by prior licensing conditions]
 *
 */ 
package colors;

import javax.servlet.http.*;

public class ColorGameBean {

    private String background = "yellow";
    private String foreground = "red";
    private String color1 = foreground;
    private String color2 = background;
    private String hint = "no";
    private int attempts = 0;
	private int intval = 0;
    private boolean tookHints = false;

    public void processRequest(HttpServletRequest request) {

	// background = "yellow";
	// foreground = "red";

	if (! color1.equals(foreground)) {
	    if (color1.equalsIgnoreCase("black") ||
			color1.equalsIgnoreCase("cyan")) {
			background = color1;
		}
	}

	if (! color2.equals(background)) {
	    if (color2.equalsIgnoreCase("black") ||
			color2.equalsIgnoreCase("cyan")) {
			foreground = color2;
	    }
	}

	attempts++;
    }

    public void setColor2(String x) {
	color2 = x;
    }

    public void setColor1(String x) {
	color1 = x;
    }

    public void setAction(String x) {
	if (!tookHints)
	    tookHints = x.equalsIgnoreCase("Hint");
	hint = x;
    }

    public String getColor2() {
	 return background;
    }

    public String getColor1() {
	 return foreground;
    }

    public int getAttempts() {
	return attempts;
    }

    public boolean getHint() {
	return hint.equalsIgnoreCase("Hint");
    }

    public boolean getSuccess() {
	if (background.equalsIgnoreCase("black") ||
	    background.equalsIgnoreCase("cyan")) {
	
	    if (foreground.equalsIgnoreCase("black") ||
		foreground.equalsIgnoreCase("cyan"))
		return true;
	    else
		return false;
	}

	return false;
    }

    public boolean getHintTaken() {
	return tookHints;
    }

    public void reset() {
	foreground = "red";
	background = "yellow";
    }

    public void setIntval(int value) {
	intval = value;
	}

    public int getIntval() {
	return intval;
	}
}

