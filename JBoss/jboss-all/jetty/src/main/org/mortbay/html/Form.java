// ===========================================================================
// Copyright (c) 1996 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: Form.java,v 1.15.2.3 2003/06/04 04:47:37 starksm Exp $
// ---------------------------------------------------------------------------

package org.mortbay.html;
import java.io.IOException;
import java.io.Writer;
import org.mortbay.http.HttpFields;

/* -------------------------------------------------------------------- */
/** HTML Form.
 * The specialized Block can contain HTML Form elements as well as
 * any other HTML elements
 */
public class Form extends Block
{
    public static final String encodingWWWURL = HttpFields.__WwwFormUrlEncode;
    public static final String encodingMultipartForm = "multipart/form-data";
    private String method="POST";
    
    /* ----------------------------------------------------------------- */
    /** Constructor.
     */
    public Form()
    {
        super("form");
    }

    /* ----------------------------------------------------------------- */
    /** Constructor.
     * @param submitURL The URL to submit the form to
     */
    public Form(String submitURL)
    {
        super("form");
        action(submitURL);
    }

    /* ----------------------------------------------------------------- */
    /** Constructor.
     * @param submitURL The URL to submit the form to
     */
    public Form action(String submitURL)
    {
        attribute("action",submitURL);
        return this;
    }
    
    /* ----------------------------------------------------------------- */
    /** Set the form target.
     */
    public Form target(String t)
    {
        attribute("target",t);
        return this;
    }
    
    /* ----------------------------------------------------------------- */
    /** Set the form method.
     */
    public Form method(String m)
    {
        method=m;
        return this;
    }
    
    /* ------------------------------------------------------------ */
    /** Set the form encoding type.
     */
    public Form encoding(String encoding){
        attribute("enctype", encoding);
        return this;
    }
    /* ----------------------------------------------------------------- */
    public void write(Writer out)
         throws IOException
    {
        attribute("method",method);
        super.write(out);
    }
}




