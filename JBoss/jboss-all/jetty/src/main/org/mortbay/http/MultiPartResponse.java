// ========================================================================
// Copyright (c) 1996 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: MultiPartResponse.java,v 1.15.2.5 2003/06/04 04:47:42 starksm Exp $
// ------------------------------------------------------------------------

package org.mortbay.http;

import java.io.IOException;
import java.io.OutputStream;
import org.mortbay.util.Code;
import org.mortbay.util.StringUtil;

/* ================================================================ */
/** Handle a multipart MIME response.
 *
 * @version $Id: MultiPartResponse.java,v 1.15.2.5 2003/06/04 04:47:42 starksm Exp $
 * @author Greg Wilkins
 * @author Jim Crossley
*/
public class MultiPartResponse
{
    /* ------------------------------------------------------------ */
    private static byte[] __CRLF;
    private static byte[] __DASHDASH;
    static
    {
        try
        {
            __CRLF="\015\012".getBytes(StringUtil.__ISO_8859_1);
            __DASHDASH="--".getBytes(StringUtil.__ISO_8859_1);
        }
        catch (Exception e) {Code.fail(e);}
    }
    
    /* ------------------------------------------------------------ */
    private String boundary;
    private byte[] boundaryBytes;

    /* ------------------------------------------------------------ */
    private MultiPartResponse()
    {
        try
        {
            boundary = "org.mortbay.http.MultiPartResponse.boundary."+
                Long.toString(System.currentTimeMillis(),36);
            boundaryBytes=boundary.getBytes(StringUtil.__ISO_8859_1);
        }
        catch (Exception e)
        {
            Code.fail(e);
        }
    }    
    
    /* ------------------------------------------------------------ */
    public String getBoundary()
    {
        return boundary;
    }
    
    /* ------------------------------------------------------------ */    
    /** PrintWriter to write content too.
     */
    private OutputStream out = null; 
    public OutputStream getOut() {return out;}

    /* ------------------------------------------------------------ */
    private boolean inPart=false;
    
    /* ------------------------------------------------------------ */
    public MultiPartResponse(OutputStream out)
         throws IOException
    {
        this();
        this.out=out;
        inPart=false;
    }
    
    /* ------------------------------------------------------------ */
    /** MultiPartResponse constructor.
     */
    public MultiPartResponse(HttpResponse response)
         throws IOException
    {
        this();
        response.setField(HttpFields.__ContentType,"multipart/mixed;boundary="+boundary);
        out=response.getOutputStream();
        inPart=false;
    }    

    /* ------------------------------------------------------------ */
    /** Start creation of the next Content.
     */
    public void startPart(String contentType)
         throws IOException
    {
        if (inPart)
            out.write(__CRLF);
        inPart=true;
        out.write(__DASHDASH);
        out.write(boundaryBytes);
        out.write(__CRLF);
        out.write(("Content-type: "+contentType).getBytes(StringUtil.__ISO_8859_1));
        out.write(__CRLF);
        out.write(__CRLF);
    }
    
    /* ------------------------------------------------------------ */
    /** Start creation of the next Content.
     */
    public void startPart(String contentType, String[] headers)
         throws IOException
    {
        if (inPart)
            out.write(__CRLF);
        inPart=true;
        out.write(__DASHDASH);
        out.write(boundaryBytes);
        out.write(__CRLF);
        out.write(("Content-type: "+contentType).getBytes(StringUtil.__ISO_8859_1));
        out.write(__CRLF);
        for (int i=0;headers!=null && i<headers.length;i++)
        {
            out.write(headers[i].getBytes(StringUtil.__ISO_8859_1));
            out.write(__CRLF);
        }
        out.write(__CRLF);
    }
        
    /* ------------------------------------------------------------ */
    /** End the current part.
     * @exception IOException IOException
     */
    public void close()
         throws IOException
    {
        if (inPart)
            out.write(__CRLF);
        out.write(__DASHDASH);
        out.write(boundaryBytes);
        out.write(__DASHDASH);
        out.write(__CRLF);
        inPart=false;
    }
    
};




