// ===========================================================================
// Copyright (c) 1996 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: ChunkingInputStream.java,v 1.1.2.5 2003/06/04 04:47:40 starksm Exp $
// ---------------------------------------------------------------------------

package org.mortbay.http;

import java.io.IOException;
import java.io.InputStream;
import org.mortbay.util.Code;
import org.mortbay.util.LineInput;


/* ------------------------------------------------------------ */
/** Dechunk input.
 * Or limit content length.
 */
public class ChunkingInputStream extends InputStream
{
    /* ------------------------------------------------------------ */
    int _chunkSize=0;
    HttpFields _trailer=null;
    LineInput _in;
    
    /* ------------------------------------------------------------ */
    /** Constructor.
     */
    public ChunkingInputStream(LineInput in)
    {
        _in=in;
    }
    
    /* ------------------------------------------------------------ */
    public void resetStream()
    {
        _chunkSize=0;
        _trailer=null;
    }
    
    /* ------------------------------------------------------------ */
    public int read()
        throws IOException
    {
        int b=-1;
        if (_chunkSize<=0 && getChunkSize()<=0)
            return -1;
        b=_in.read();
        _chunkSize=(b<0)?-1:(_chunkSize-1);
        return b;
    }
    
    /* ------------------------------------------------------------ */
    public int read(byte b[]) throws IOException
    {
        int len = b.length;
        if (_chunkSize<=0 && getChunkSize()<=0)
            return -1;
        if (len > _chunkSize)
            len=_chunkSize;
        len=_in.read(b,0,len);
        _chunkSize=(len<0)?-1:(_chunkSize-len);
        return len;
    }
    
    /* ------------------------------------------------------------ */
    public int read(byte b[], int off, int len) throws IOException
    {  
        if (_chunkSize<=0 && getChunkSize()<=0)
            return -1;
        if (len > _chunkSize)
            len=_chunkSize;
        len=_in.read(b,off,len);
        _chunkSize=(len<0)?-1:(_chunkSize-len);
        return len;
    }
    
    /* ------------------------------------------------------------ */
    public long skip(long len) throws IOException
    { 
        if (_chunkSize<=0 && getChunkSize()<=0)
                return -1;
        if (len > _chunkSize)
            len=_chunkSize;
        len=_in.skip(len);
        _chunkSize=(len<0)?-1:(_chunkSize-(int)len);
        return len;
    }
    
    /* ------------------------------------------------------------ */
    public int available()
        throws IOException
    {
        int len = _in.available();
        if (len<=_chunkSize || _chunkSize==0)
            return len;
        return _chunkSize;
    }
    
    /* ------------------------------------------------------------ */
    public void close()
        throws IOException
    {
        _chunkSize=-1;
    }
    
    /* ------------------------------------------------------------ */
    /** Mark is not supported.
     * @return false
     */
    public boolean markSupported()
    {
        return false;
    }
    
    /* ------------------------------------------------------------ */
    /** Not Implemented.
     */
    public void reset()
    {
        Code.notImplemented();
    }
    
    /* ------------------------------------------------------------ */
    /** Not Implemented.
     * @param readlimit 
     */
    public void mark(int readlimit)
    {
        Code.notImplemented();
    }
    
    /* ------------------------------------------------------------ */
    /* Get the size of the next chunk.
     * @return size of the next chunk or -1 for EOF.
     * @exception IOException 
     */
    private int getChunkSize()
        throws IOException
    {
        if (_chunkSize<0)
            return -1;
        
        _trailer=null;
        _chunkSize=-1;
        
        // Get next non blank line
        org.mortbay.util.LineInput.LineBuffer line_buffer
            =_in.readLineBuffer();
        while(line_buffer!=null && line_buffer.size==0)
            line_buffer=_in.readLineBuffer();
        
        // Handle early EOF or error in format
        if (line_buffer==null)
        {
            Code.warning("EOF");
            return -1;
        }
        String line= new String(line_buffer.buffer,0,line_buffer.size);
        
        
        // Get chunksize
        int i=line.indexOf(';');
        if (i>0)
            line=line.substring(0,i).trim();
        try
        {
            _chunkSize = Integer.parseInt(line,16);
        }
        catch (NumberFormatException e)
        {
            _chunkSize=-1;
            Code.warning("Bad Chunk:"+line);
            Code.debug(e);
        }
                 
        // check for EOF
        if (_chunkSize==0)
        {
            _chunkSize=-1;
            // Look for trailers
            _trailer = new HttpFields();
            _trailer.read(_in);
        }
        
        return _chunkSize;
    }
}
