/*
 * $Header: /home/cvs/jakarta-tomcat-4.0/webapps/examples/WEB-INF/classes/validators/DebugValidator.java,v 1.1 2001/09/05 05:14:01 craigmcc Exp $
 * $Revision: 1.1 $
 * $Date: 2001/09/05 05:14:01 $
 *
 * ====================================================================
 *
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 1999-2001 The Apache Software Foundation.  All rights
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


package validators;


import java.io.InputStream;
import java.io.IOException;
import javax.servlet.jsp.tagext.PageData;
import javax.servlet.jsp.tagext.TagLibraryValidator;
import javax.servlet.jsp.tagext.ValidationMessage;


/**
 * Example tag library validator that simply dumps the XML version of each
 * page to standard output (which will typically be sent to the file
 * <code>$CATALINA_HOME/logs/catalina.out</code>).  To utilize it, simply
 * include a <code>taglib</code> directive for this tag library at the top
 * of your JSP page.
 *
 * @author Craig McClanahan
 * @version $Revision: 1.1 $ $Date: 2001/09/05 05:14:01 $
 */

public class DebugValidator extends TagLibraryValidator {


    // ----------------------------------------------------- Instance Variables


    // --------------------------------------------------------- Public Methods


    /**
     * Validate a JSP page.  This will get invoked once per directive in the
     * JSP page.  This method will return <code>null</code> if the page is
     * valid; otherwise the method should return an array of
     * <code>ValidationMessage</code> objects.  An array of length zero is
     * also interpreted as no errors.
     *
     * @param prefix The value of the prefix argument in this directive
     * @param uri The value of the URI argument in this directive
     * @param page The page data for this page
     */
    public ValidationMessage[] validate(String prefix, String uri,
                                        PageData page) {

        System.out.println("---------- Prefix=" + prefix + " URI=" + uri +
                           "----------");

        InputStream is = page.getInputStream();
        while (true) {
            try {
                int ch = is.read();
                if (ch < 0)
                    break;
                System.out.print((char) ch);
            } catch (IOException e) {
                break;
            }
        }
        System.out.println();
        System.out.println("-----------------------------------------------");
        return (null);

    }


}
