// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: RootNotFoundHandler.java,v 1.1.2.6 2003/06/04 04:47:47 starksm Exp $
// ========================================================================

package org.mortbay.http.handler;

import java.io.IOException;
import java.io.OutputStream;
import org.mortbay.http.HttpContext;
import org.mortbay.http.HttpException;
import org.mortbay.http.HttpFields;
import org.mortbay.http.HttpRequest;
import org.mortbay.http.HttpResponse;
import org.mortbay.util.ByteArrayISO8859Writer;
import org.mortbay.util.Code;
import org.mortbay.util.StringUtil;

/* ------------------------------------------------------------ */
/** 
 * @version $Id: RootNotFoundHandler.java,v 1.1.2.6 2003/06/04 04:47:47 starksm Exp $
 * @author Greg Wilkins (gregw)
 */
public class RootNotFoundHandler extends NotFoundHandler
{
    
    /* ------------------------------------------------------------ */
    public void handle(String pathInContext,
                       String pathParams,
                       HttpRequest request,
                       HttpResponse response)
        throws HttpException, IOException
    {
        Code.debug("Root Not Found");
        String method=request.getMethod();
        
        if (!method.equals(HttpRequest.__GET) ||
            !request.getPath().equals("/"))
        {
            // don't bother with fancy format.
            super.handle(pathInContext,pathParams,request,response);
            return;
        }

        response.setStatus(404);
        request.setHandled(true);
        response.setReason("Not Found");
        response.setContentType(HttpFields.__TextHtml);
        
        ByteArrayISO8859Writer writer = new ByteArrayISO8859Writer(1500);

        String uri=request.getPath();
        uri=StringUtil.replace(uri,"<","&lt;");
        uri=StringUtil.replace(uri,">","&gt;");
        
        writer.write("<HTML>\n<HEAD>\n<TITLE>Error 404 - Not Found");
        writer.write("</TITLE>\n<BODY>\n<H2>Error 404 - Not Found.</H2>\n");
        writer.write("No context on this server matched or handled this request.<BR>");
        writer.write("Contexts known to this server are: <UL>");

        HttpContext[] contexts = getHttpContext().getHttpServer().getContexts();
        
        for (int i=0;i<contexts.length;i++)
        {
            HttpContext context = contexts[i];
            writer.write("<LI><A HREF=");
            writer.write(context.getContextPath());
            writer.write(">");
            writer.write(context.toString());
            writer.write("</A><BR>\n");
        }
        
        writer.write("</UL><small><I>The links above may not work if a virtual host is configured</I></small>");

        writer.write("\n</BODY>\n</HTML>\n");
        writer.flush();
        response.setContentLength(writer.size());
        OutputStream out=response.getOutputStream();
        writer.writeTo(out);
        out.close();
    }
}
