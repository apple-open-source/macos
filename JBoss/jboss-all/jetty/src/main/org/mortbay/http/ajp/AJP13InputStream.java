// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: AJP13InputStream.java,v 1.3.2.7 2003/06/04 04:47:45 starksm Exp $
// ========================================================================

package org.mortbay.http.ajp;


import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import org.mortbay.http.ajp.AJP13RequestPacket;

public class AJP13InputStream extends InputStream
{   
    /* ------------------------------------------------------------ */
    private AJP13RequestPacket _packet;
    private AJP13RequestPacket _getBodyChunk;
    private InputStream _in;
    private OutputStream _out;
    private boolean _gotFirst=false;
    private boolean _closed;
    
    /* ------------------------------------------------------------ */
    AJP13InputStream(InputStream in, OutputStream out, int bufferSize)
    {
        _in=in;
        _out=out;
        _packet=new AJP13RequestPacket(bufferSize);
        _getBodyChunk=new AJP13RequestPacket(8);
        _getBodyChunk.addByte((byte)'A');
        _getBodyChunk.addByte((byte)'B');
        _getBodyChunk.addInt(3);
        _getBodyChunk.addByte(AJP13RequestPacket.__GET_BODY_CHUNK);
        _getBodyChunk.addInt(bufferSize);
    }

    /* ------------------------------------------------------------ */
    public void resetStream()
    {
        _gotFirst=false;
        _closed=false;
        _packet.reset();
    }
    
    /* ------------------------------------------------------------ */
    public void destroy()
    {
        if (_packet!=null)_packet.destroy();
        _packet=null;
        if (_getBodyChunk!=null)_getBodyChunk.destroy();
        _getBodyChunk=null;
        _in=null;
        _out=null;
    }

    /* ------------------------------------------------------------ */
    public int available()
        throws IOException
    {
        if (_closed)
            return 0;
        if (_packet.unconsumedData()==0)
            fillPacket();
        return _packet.unconsumedData();
    }

    /* ------------------------------------------------------------ */
    public void close()
        throws IOException
    {
        _closed=true;
    }

    /* ------------------------------------------------------------ */
    public void mark(int readLimit)
    {}

    /* ------------------------------------------------------------ */
    public boolean markSupported()
    {
        return false;
    }

    /* ------------------------------------------------------------ */
    public void reset()
        throws IOException
    {
        throw new IOException("reset() not supported");
    }

    /* ------------------------------------------------------------ */
    public int read()
        throws IOException
    {
        if (_closed)
            return -1;
        
        if (_packet.unconsumedData()<=0)
        {
            fillPacket();
            if (_packet.unconsumedData()<=0)
            {
                _closed=true;
                return -1;
            }
        }
        return _packet.getByte();
    }

    /* ------------------------------------------------------------ */
    public int read(byte[] b, int off, int len)
        throws IOException
    {
        if (_closed)
            return -1;
        
        if (_packet.unconsumedData()==0)
        {
            fillPacket();
            if (_packet.unconsumedData()==0)
            {
                _closed=true;
                return -1;
            }
        }
        
        return _packet.getBytes(b,off,len);
    }

    /* ------------------------------------------------------------ */
    /** 
     * @return The next packet from the stream. The packet is recycled and is
     * only valid until the next call to nextPacket or read().
     * @exception IOException 
     */
    public AJP13RequestPacket nextPacket()
        throws IOException
    {
        if (_packet.read(_in))
            return _packet;
        return null;
    }
    
    /* ------------------------------------------------------------ */
    private void fillPacket()
        throws IOException
    {
        if (_closed)
            return;
        
        if (_gotFirst || _in.available()==0) 
            _getBodyChunk.write(_out);
        _gotFirst=true;

        // read packet
        if (!_packet.read(_in))
            throw new IOException("EOF");
        
        if (_packet.unconsumedData()<=0)
            _closed=true;
        else if(_packet.getInt()>_packet.getBufferSize())
            throw new IOException("AJP Protocol error");
    }
    
    /* ------------------------------------------------------------ */
    public long skip(long n)
        throws IOException
    {
        if (_closed)
            return -1;
        
        for (int i=0;i<n;i++)
            if (read()<0)
                return i==0?-1:i;
        return n;
    }
}
