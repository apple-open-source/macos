// ===========================================================================
// Copyright (c) 1996 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: ChunkingOutputStream.java,v 1.1.2.5 2003/06/04 04:47:40 starksm Exp $
// ---------------------------------------------------------------------------

package org.mortbay.http;

import java.io.IOException;
import java.io.OutputStream;

/* ---------------------------------------------------------------- */
/** HTTP Chunking OutputStream.
 * @version $Id: ChunkingOutputStream.java,v 1.1.2.5 2003/06/04 04:47:40 starksm Exp $
 * @author Greg Wilkins
*/
public class ChunkingOutputStream
    extends BufferedOutputStream
    implements HttpMessage.HeaderWriter
{
    /* ------------------------------------------------------------ */
    final static byte[]
        __CRLF   =   {(byte)'\015',(byte)'\012'};
    final static byte[]
        __CHUNK_EOF ={(byte)'0',(byte)'\015',(byte)'\012'};

    final static int __CHUNK_RESERVE=8;
    final static int __EOF_RESERVE=8;
    
    /* ------------------------------------------------------------ */
    private HttpFields _trailer;
    private boolean _complete;
    private boolean _completed;
    
    /* ------------------------------------------------------------ */
    /** Constructor. 
     * @param outputStream The outputStream to buffer or chunk to.
     */
    public ChunkingOutputStream(OutputStream outputStream,
                                int bufferSize,
                                int headerReserve)
    {
        super(outputStream,
              bufferSize,
              headerReserve,
              __CHUNK_RESERVE,
              __EOF_RESERVE);
        setBypassBuffer(true);
        setFixed(true);
    }
    
    /* ------------------------------------------------------------ */
    /** Set the trailer to send with a chunked close.
     * @param trailer 
     */
    public void setTrailer(HttpFields trailer)
    {
        _trailer=trailer;
    }
    
    /* ------------------------------------------------------------ */
    /** Flush.
     * @exception IOException 
     */
    public void flush()
        throws IOException
    {        
        super.flush();
        
        // Handle any trailers
        if (_trailer!=null && _completed)
        {
            _trailer.write(_httpMessageWriter);
            _httpMessageWriter.writeTo(_out);
            _httpMessageWriter.resetWriter();
        }
    }
    
    /* ------------------------------------------------------------ */
    public void close()
        throws IOException
    {
        _complete=true;
        flush();
    }

    /* ------------------------------------------------------------ */
    public void resetStream()
    {
        _complete=false;
        _completed=false;
        super.resetStream();
    }
    
    /* ------------------------------------------------------------ */
    protected void wrapBuffer()
        throws IOException
    {
        // Handle chunking
        int size=size();
        if (size()>0)
        {
            prewrite(__CRLF,0,__CRLF.length);
            while (size>0)
            {
                int d=size%16;
                if (d<=9)
                    prewrite('0'+d);
                else
                    prewrite('a'-10+d);
                size=size/16;
            }
            postwrite(__CRLF,0,__CRLF.length);
        }
        
        // Complete it if we must.
        if (_complete & !_completed)
        {
            _completed=true;
            postwrite(__CHUNK_EOF,0,__CHUNK_EOF.length);
            if (_trailer==null)
                postwrite(__CRLF,0,__CRLF.length);
        }
    }
    
    /* ------------------------------------------------------------ */
    protected void bypassWrite(byte[] b, int offset, int length)
        throws IOException
    {
        int i=9;                    
        int chunk=length;
        _buf[10]=(byte)'\012';
        _buf[9]=(byte)'\015';
        while (chunk>0)
        {
            int d=chunk%16;
            if (d<=9)
                _buf[--i]=(byte)('0'+d);
            else
                _buf[--i]=(byte)('a'-10+d);
            chunk=chunk/16;
        }
        _out.write(_buf,i,10-i+1);
        _out.write(b,offset,length);
        _out.write(__CRLF,0,__CRLF.length);
        _out.flush();
    }
    
}

    
