// ===========================================================================
// Copyright (c) 2001 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: BufferedOutputStream.java,v 1.1.2.6 2003/06/04 04:47:40 starksm Exp $
// ---------------------------------------------------------------------------

package org.mortbay.http;
import java.io.IOException;
import java.io.OutputStream;
import org.mortbay.util.ByteArrayISO8859Writer;
import org.mortbay.util.ByteBufferOutputStream;
import org.mortbay.util.OutputObserver;

/* ------------------------------------------------------------ */
/** Buffered Output Stream.
 * Uses ByteBufferOutputStream to allow pre and post writes.
 * @version $Revision: 1.1.2.6 $
 * @author Greg Wilkins (gregw)
 */
public class BufferedOutputStream
    extends ByteBufferOutputStream
    implements HttpMessage.HeaderWriter
{
    protected OutputStream _out;
    protected ByteArrayISO8859Writer _httpMessageWriter;
    private OutputObserver _commitObserver;
    private boolean _commited ;
    private int _preReserve;
    private boolean _bypassBuffer ;

    /* ------------------------------------------------------------ */
    /** Constructor. 
     * @param out the OutputStream to buffer to.
     * @param capacity Buffer capacity.
     * @param headerReserve The reserve of bytes for prepending to be used
     * for the first buffer after reset
     * @param preReserve The reserve of bytes for prepending
     * @param postReserve The reserve of bytes for appending
     */
    public BufferedOutputStream(OutputStream out,
                                int capacity,
                                int headerReserve,
                                int preReserve,
                                int postReserve)
    {
        super(capacity,headerReserve,postReserve);
        _out=out;
        _preReserve=preReserve;
        _httpMessageWriter = new ByteArrayISO8859Writer(headerReserve);
    }
    
    /* ------------------------------------------------------------ */
    public OutputStream getOutputStream()
    {
        return _out;
    }
    
    /* ------------------------------------------------------------ */
    /** 
     * @return OutputObserver to receives commit events from this stream.
     */
    public OutputObserver getCommitObserver()
    {
        return _commitObserver;
    }
    
    /* ------------------------------------------------------------ */
    /** 
     * @param commitObserver  OutputObserver to receives commit events from this stream.
     */
    public void setCommitObserver(OutputObserver commitObserver)
    {
        _commitObserver = commitObserver;
    }
    
    /* ------------------------------------------------------------ */
    public boolean isCommitted()
    {
        return _commited;
    }
    
    /* ------------------------------------------------------------ */
    /** 
     * @return If true, the buffer is bypassed for large writes
     * to a committed stream.
     */
    public boolean getBypassBuffer()
    {
        return _bypassBuffer;
    }
    
    /* ------------------------------------------------------------ */
    /** 
     * @param bypassBuffer If true, the buffer is bypassed for large writes
     * to a committed stream.
     */
    public void setBypassBuffer(boolean bypassBuffer)
    {
        _bypassBuffer = bypassBuffer;
    }
    
    /* ------------------------------------------------------------ */
    public void writeHeader(HttpMessage httpMessage)
        throws IOException
    {
        httpMessage.writeHeader(_httpMessageWriter);
    }
    
    /* ------------------------------------------------------------ */
    public void write(byte[] b)
        throws IOException
    {
        write(b,0,b.length);
    }
    
    /* ------------------------------------------------------------ */
    public void write(byte[] b, int offset, int length)
        throws IOException
    {
        int o=offset;
        int l=length;
        while (l>0)
        {
            int c=capacity();
            
            if (_bypassBuffer && isCommitted() && size()==0 && l>c)
            {
                // Bypass buffer
                bypassWrite(b,o,l);
                break;
            }

            if (l<c || !isFixed())
            {
                // Write all
                super.write(b,o,l);
                break;
            }
            else
            {
                // Write a block
                super.write(b,o,c);
                flush();
                l-=c;
                o+=c;
            }
        }
    }

    /* ------------------------------------------------------------ */
    protected void bypassWrite(byte[] b, int offset, int length)
        throws IOException
    {
        try
        {
            _out.write(b,offset,length);
            _out.flush();
        }
        catch (IOException e)
        {
            throw new EOFException(e);
        }
    }             
    
    /* ------------------------------------------------------------ */
    /**
     * This implementation calls the commitObserver on the first flush since
     * construction or reset.
     */
    public void flush()
        throws IOException
    {
        try
        {
            if (!_commited)
            {
                _commited=true;
                if (_commitObserver!=null)
                    _commitObserver.outputNotify(this,OutputObserver.__COMMITING,null);
            }
            
            wrapBuffer();
            
            // Add headers
            if (_httpMessageWriter.size()>0)
            {
                prewrite(_httpMessageWriter.getBuf(),0,_httpMessageWriter.size());
                _httpMessageWriter.resetWriter();
            }
            
            if (size()>0)
                writeTo(_out);
        }
        catch (IOException e)
        {
            throw new EOFException(e);
        }
        finally
        {
            reset(_preReserve);
        }
    }
    
    
    /* ------------------------------------------------------------ */
    /** Wrap Buffer.
     * Called by flush() to allow the data in the buffer to be pre and post
     * written for any protocol wrapping.  The default implementation does
     * nothing.
     * @exception IOException 
     */
    protected void wrapBuffer()
        throws IOException
    {
    }
    
    /* ------------------------------------------------------------ */
    public void close()
        throws IOException
    {
        flush();
        _out.close();
    }
    
    /* ------------------------------------------------------------ */
    public void resetStream()
    {
        super.reset(_httpMessageWriter.capacity());
        _commited=false;
    }
    
    /* ------------------------------------------------------------ */
    public void destroy()
    {
        super.destroy();
        if (_httpMessageWriter!=null)
            _httpMessageWriter.destroy();
        _httpMessageWriter=null;
        _out=null;
    }
    
}
