// ===========================================================================
// Copyright (c) 2001 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: NullableOutputStream.java,v 1.1.2.4 2003/06/04 04:47:42 starksm Exp $
// ---------------------------------------------------------------------------

package org.mortbay.http;
import java.io.FilterOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import org.mortbay.util.ByteArrayISO8859Writer;
import org.mortbay.util.Code;

/* ------------------------------------------------------------ */
/** Buffered Output Stream.
 * Uses ByteBufferOutputStream to allow pre and post writes.
 * @version $Revision: 1.1.2.4 $
 * @author Greg Wilkins (gregw)
 */
public class NullableOutputStream
    extends FilterOutputStream
    implements HttpMessage.HeaderWriter
{
    private ByteArrayISO8859Writer _httpMessageWriter;
    private boolean _nulled=false;
    private boolean _closed=false;
    private int _headerReserve;
    
    /* ------------------------------------------------------------ */
    public NullableOutputStream(OutputStream outputStream, int headerReserve)
    {
        super(outputStream);
        _headerReserve = headerReserve;
    }
    
    /* ------------------------------------------------------------ */
    /** Null the output.
     * All output written is discarded until the stream is reset. Used
     * for HEAD requests.
     */
    public void nullOutput()
        throws IOException
    {
        _nulled=true;
    }
    
    /* ------------------------------------------------------------ */
    /** is the output Nulled?
     */
    public boolean isNullOutput()
        throws IOException
    {
        return _nulled;
    }
    
    /* ------------------------------------------------------------ */
    public void writeHeader(HttpMessage httpMessage)
        throws IOException
    {
        if (_httpMessageWriter==null)
            _httpMessageWriter=new ByteArrayISO8859Writer(_headerReserve);
        httpMessage.writeHeader(_httpMessageWriter);
        _httpMessageWriter.writeTo(out);
        _httpMessageWriter.resetWriter();
    }
    
    /* ------------------------------------------------------------ */
    public void resetStream()
    {
        _closed=false;
        _nulled=false;
        if (_httpMessageWriter!=null)
            _httpMessageWriter.resetWriter();
    }
    
    /* ------------------------------------------------------------ */
    public void destroy()
    {
        if (_httpMessageWriter!=null)
            _httpMessageWriter.destroy();
        _httpMessageWriter=null;
        try{out.close();} catch (Exception e){Code.warning(e);}
    }
    
    /* ------------------------------------------------------------ */
    public void write(int b) throws IOException
    {
        if (!_nulled)
        {
            if (_closed)
                throw new IOException("closed");
            out.write(b);
        }
    }

    /* ------------------------------------------------------------ */
    public void write(byte b[]) throws IOException
    {
        if (!_nulled)
        {
            if (_closed)
                throw new IOException("closed");
            out.write(b,0,b.length);
        }
    }

    /* ------------------------------------------------------------ */
    public void write(byte b[], int off, int len)
        throws IOException
    {     
        if (!_nulled)
        {
            if (_closed)
                throw new IOException("closed");
//             byte[] b2 = new byte[8192];
//             System.arraycopy(b,off,b2,0,len);
//             org.mortbay.http.ajp.AJP13Packet p =
//                 new org.mortbay.http.ajp.AJP13Packet(b2,len);
//             System.err.println(Thread.currentThread()+" Nullable  "+p.toString());
            out.write(b,off,len);
        }
    }
    
    /* ------------------------------------------------------------ */
    public void flush()
        throws IOException
    {
        if (!_nulled && !_closed)
            out.flush();
    }
    
    /* ------------------------------------------------------------ */
    /** Close the stream.
     * @exception IOException 
     */
    public void close()
        throws IOException
    {
        _closed=true;
    }
    
}
