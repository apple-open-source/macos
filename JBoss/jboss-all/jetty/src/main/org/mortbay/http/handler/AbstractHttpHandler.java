// ========================================================================
// Copyright (c) 1999 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: AbstractHttpHandler.java,v 1.2.2.6 2003/06/04 04:47:46 starksm Exp $
// ========================================================================

package org.mortbay.http.handler;

import java.io.IOException;
import java.io.OutputStream;
import org.mortbay.http.HttpContext;
import org.mortbay.http.HttpFields;
import org.mortbay.http.HttpHandler;
import org.mortbay.http.HttpRequest;
import org.mortbay.http.HttpResponse;
import org.mortbay.util.ByteArrayISO8859Writer;
import org.mortbay.util.Code;

/* ------------------------------------------------------------ */
/** Base HTTP Handler.
 * This No-op handler is a good base for other handlers
 *
 * @version $Id: AbstractHttpHandler.java,v 1.2.2.6 2003/06/04 04:47:46 starksm Exp $
 * @author Greg Wilkins (gregw)
 */
abstract public class AbstractHttpHandler implements HttpHandler
{
    /* ----------------------------------------------------------------- */
    private String _name;
    
    private transient HttpContext _context;
    private transient boolean _started=false;

    
    /* ------------------------------------------------------------ */
    public void setName(String name)
    {
        _name=name;
    }
    
    /* ------------------------------------------------------------ */
    public String getName()
    {
        if (_name==null)
        {
            _name=this.getClass().getName();
            if (!Code.debug())
                _name=_name.substring(_name.lastIndexOf('.')+1);
        }
        return _name;
    }
    
    /* ------------------------------------------------------------ */
    public HttpContext getHttpContext()
    {
        return _context;
    }
    
    /* ------------------------------------------------------------ */
    /** Initialize with a HttpContext.
     * Called by addHandler methods of HttpContext.
     * @param context Must be the HttpContext of the handler
     */
    public void initialize(HttpContext context)
    {
        if (_context==null)
            _context=context;
        else if (_context!=context)
            throw new IllegalStateException("Can't initialize handler for different context");
    }
    
    /* ----------------------------------------------------------------- */
    public void start()
        throws Exception
    {
        if (_context==null)
            throw new IllegalStateException("No context for "+this);        
        _started=true;
        Code.debug("Started "+this);
    }
    
    /* ----------------------------------------------------------------- */
    public void stop()
        throws InterruptedException
    {
        _started=false;
        Code.debug("Stopped "+this);
    }

    /* ----------------------------------------------------------------- */
    public boolean isStarted()
    {
        return _started;
    }
    
    /* ------------------------------------------------------------ */
    public String toString()
    {
        return getName()+" in "+_context;
    }    

    /* ----------------------------------------------------------------- */
    public void handleTrace(HttpRequest request,
                            HttpResponse response)
        throws IOException
    {
        boolean trace=getHttpContext().getHttpServer().getTrace();
        
        // Handle TRACE by returning request header
        response.setField(HttpFields.__ContentType,
                          HttpFields.__MessageHttp);
        if (trace)
        {
            OutputStream out = response.getOutputStream();
            ByteArrayISO8859Writer writer = new ByteArrayISO8859Writer();
            writer.write(request.toString());
            writer.flush();
            response.setIntField(HttpFields.__ContentLength,writer.size());
            writer.writeTo(out);
            out.flush();
        }
        request.setHandled(true);
    }
}




