// ===========================================================================
// Copyright (c) 1996 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: HttpOutputStream.java,v 1.1.2.7 2003/07/11 00:55:12 jules_gosnell Exp $
// ---------------------------------------------------------------------------

package org.mortbay.http;

import java.io.FilterOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.io.Writer;
import java.util.ArrayList;
import org.mortbay.util.ByteArrayPool;
import org.mortbay.util.Code;
import org.mortbay.util.IO;
import org.mortbay.util.OutputObserver;
import org.mortbay.util.StringUtil;


/* ---------------------------------------------------------------- */
/** HTTP Http OutputStream.
 * Acts as a BufferedOutputStream until setChunking() is called.
 * Once chunking is enabled, the raw stream is chunk encoded as per RFC2616.
 *
 * Implements the following HTTP and Servlet features: <UL>
 * <LI>Filters for content and transfer encodings.
 * <LI>Allows output to be reset if not committed (buffer never flushed).
 * <LI>Notification of significant output events for filter triggering,
 *     header flushing, etc.
 * </UL>
 *
 * This class is not synchronized and should be synchronized
 * explicitly if an instance is used by multiple threads.
 *
 * @version $Id: HttpOutputStream.java,v 1.1.2.7 2003/07/11 00:55:12 jules_gosnell Exp $
 * @author Greg Wilkins
*/
public class HttpOutputStream
    extends FilterOutputStream
    implements OutputObserver,
               HttpMessage.HeaderWriter
{
    /* ------------------------------------------------------------ */
    final static String
        __CRLF      = IO.CRLF;
    final static byte[]
        __CRLF_B    = IO.CRLF_BYTES;
    final static byte[]
        __CHUNK_EOF_B ={(byte)'0',(byte)'\015',(byte)'\012'};

    final static int __BUFFER_SIZE=4096;
    final static int __FIRST_RESERVE=512;
    
    public final static Class[] __filterArg = {java.io.OutputStream.class};
    
    /* ------------------------------------------------------------ */
    private OutputStream _realOut;
    private NullableOutputStream _nullableOut;
    private HttpMessage.HeaderWriter _headerOut;
    private BufferedOutputStream _bufferedOut;
    private ChunkingOutputStream _chunkingOut;    
    private boolean _written;
    private ArrayList _observers;
    private int _bytes;
    private int _bufferSize;
    private int _headerReserve;
    private boolean _bufferHeaders;
    private HttpWriter _iso8859writer;
    private HttpWriter _utf8writer;
    
    /* ------------------------------------------------------------ */
    /** Constructor. 
     * @param outputStream The outputStream to buffer or chunk to.
     */
    public HttpOutputStream(OutputStream outputStream)
    {
        this (outputStream,__BUFFER_SIZE,__FIRST_RESERVE);
    }
    
    /* ------------------------------------------------------------ */
    /** Constructor. 
     * @param outputStream The outputStream to buffer or chunk to.
     */
    public HttpOutputStream(OutputStream outputStream, int bufferSize)
    {
        this (outputStream,bufferSize,__FIRST_RESERVE);
    }
    
    /* ------------------------------------------------------------ */
    /** Constructor. 
     * @param outputStream The outputStream to buffer or chunk to.
     */
    public HttpOutputStream(OutputStream outputStream,
                            int bufferSize,
                            int headerReserve)
    {
        super(outputStream);
        _written=false;
        _bufferSize=bufferSize;
        _headerReserve=headerReserve;
        
        _realOut=outputStream;
        _nullableOut=new NullableOutputStream(_realOut,headerReserve);
        _headerOut=_nullableOut;
        out=_nullableOut;
    }
    
    /* ------------------------------------------------------------ */
    public void setBufferedOutputStream(BufferedOutputStream bos,
                                        boolean bufferHeaders)
    {
        _bufferedOut=bos;
        _bufferedOut.setCommitObserver(this);
        _bufferHeaders=bufferHeaders;
    }
    
    /* ------------------------------------------------------------ */
    /** Get the backing output stream.
     * A stream without filters or chunking is returned.
     * @return Raw OutputStream.
     */
    public OutputStream getOutputStream()
    {
        return _realOut;
    }
    
    /* ------------------------------------------------------------ */
    /** Get Filter OutputStream.
     * Get the current top of the OutputStream filter stack
     * @return OutputStream.
     */
    public OutputStream getFilterStream()
    {
        return out;
    }
    
    /* ------------------------------------------------------------ */
    /** Set Filter OutputStream.
     * Set output filter stream, which should be constructed to wrap
     * the stream returned from get FilterStream.
     */
    public void setFilterStream(OutputStream filter)
    {
	out=filter;
    }
    
    /* ------------------------------------------------------------ */
    /** Has any data been written to the stream.
     * @return True if write has been called.
     */
    public boolean isWritten()
    {
        return _written;
    }
        
    /* ------------------------------------------------------------ */
    /** Get the output buffer capacity.
     * @return Buffer capacity in bytes.
     */
    public int getBufferSize()
    {
        return _bufferSize;
    }
    
    /* ------------------------------------------------------------ */
    /** Set the output buffer size.
     * Note that this is the minimal buffer size and that installed
     * filters may perform their own buffering and are likely to change
     * the size of the output. Also the pre and post reserve buffers may be
     * allocated within the buffer for headers and chunking.
     * @param size Minimum buffer size in bytes
     * @exception IllegalStateException If output has been written.
     */
    public void setBufferSize(int size)
        throws IllegalStateException
    {
        if (size<=_bufferSize)
            return;
        
        if (_bufferedOut!=null && _bufferedOut.size()>0)
            throw new IllegalStateException("Not Reset");

        try
        {
            _bufferSize=size;
            if (_bufferedOut!=null)
            {
                boolean fixed=_bufferedOut.isFixed();
                _bufferedOut.setFixed(false);
                _bufferedOut.ensureSize(size);
                _bufferedOut.setFixed(fixed);
            }
            
            if (_chunkingOut!=null)
            {
                boolean fixed=_chunkingOut.isFixed();
                _chunkingOut.setFixed(false);
                _chunkingOut.ensureSize(size);
                _chunkingOut.setFixed(fixed);
            }
        }
        catch (IOException e){Code.warning(e);}
    }

    /* ------------------------------------------------------------ */
    public int getBytesWritten()
    {
        return _bytes;
    }
    
    /* ------------------------------------------------------------ */
    /** Reset Buffered output.
     * If no data has been committed, the buffer output is discarded and
     * the filters may be reinitialized.
     * @exception IllegalStateException
     */
    public void resetBuffer()
        throws IllegalStateException
    {
        // Shutdown filters without observation
        if (out!=null && out!=_headerOut)
        {
            ArrayList save_observers=_observers;
            _observers=null;
            try
            {
                _nullableOut.nullOutput();
                out.close();
            }
            catch(Exception e)
            {
                Code.ignore(e);
            }
            finally
            {
                _observers=save_observers;
                _nullableOut.resetStream();
            }
        }
        
        // discard current buffer and set it to output
        if (_bufferedOut!=null)
            _bufferedOut.resetStream();
        if (_chunkingOut!=null)
            _chunkingOut.resetStream();

        _headerOut=_nullableOut;
        out=(OutputStream)_headerOut;
	_bytes=0;
        _written=false;
        try
        {
            notify(OutputObserver.__RESET_BUFFER);
        }
        catch(IOException e)
        {
            Code.ignore(e);
        }
    }

    /* ------------------------------------------------------------ */
    /** Add an Output Observer.
     * Output Observers get notified of significant events on the
     * output stream. Observers are called in the reverse order they
     * were added.
     * They are removed when the stream is closed.
     * @param observer The observer. 
     */
    public void addObserver(OutputObserver observer)
    {
        if (_observers==null)
            _observers=new ArrayList(4);
        _observers.add(observer);
        _observers.add(null);
    }
    
    /* ------------------------------------------------------------ */
    /** Add an Output Observer.
     * Output Observers get notified of significant events on the
     * output stream. Observers are called in the reverse order they
     * were added.
     * They are removed when the stream is closed.
     * @param observer The observer. 
     * @param data Data to be passed wit notify calls. 
     */
    public void addObserver(OutputObserver observer, Object data)
    {
        if (_observers==null)
            _observers=new ArrayList(4);
        _observers.add(observer);
        _observers.add(data);
    }
    
    /* ------------------------------------------------------------ */
    /** Reset the observers.
     */
    public void resetObservers()
    {
        _observers=null;
    }
    
    /* ------------------------------------------------------------ */
    /** Null the output.
     * All output written is discarded until the stream is reset. Used
     * for HEAD requests.
     */
    public void nullOutput()
        throws IOException
    {
        _nullableOut.nullOutput();
    }
    
    /* ------------------------------------------------------------ */
    /** is the output Nulled?
     */
    public boolean isNullOutput()
        throws IOException
    {
        return _nullableOut.isNullOutput();
    }
    
    /* ------------------------------------------------------------ */
    /** Set chunking mode.
     */
    public void setChunking()
    {
        if (_chunkingOut==null)
        {
            _chunkingOut=new ChunkingOutputStream(_nullableOut,
                                                  _bufferSize,
                                                  _headerReserve);
            _chunkingOut.setCommitObserver(this);
        }
        _headerOut=_chunkingOut;
        out=_chunkingOut;
    }
    
    /* ------------------------------------------------------------ */
    /** Get chunking mode 
     */
    public boolean isChunking()
    {
        return _chunkingOut!=null && _headerOut==_chunkingOut;
    }
    
    /* ------------------------------------------------------------ */
    /** Reset the stream.
     * Turn disable all filters.
     * @exception IllegalStateException The stream cannot be
     * reset if chunking is enabled.
     */
    public void resetStream()
        throws IOException, IllegalStateException
    {
        if (isChunking())
            close();
        
        _written=false;
        if (_bufferedOut!=null)
            _bufferedOut.resetStream();
        if (_chunkingOut!=null)
            _chunkingOut.resetStream();
        _nullableOut.resetStream();
        
        _headerOut=_nullableOut;
        out=_nullableOut;

        _bytes=0;

        if (_observers!=null)
            _observers.clear();
    }

    /* ------------------------------------------------------------ */
    public void destroy()
    {
        if (_bufferedOut!=null)
            _bufferedOut.destroy();
        _bufferedOut=null;
        if (_chunkingOut!=null)
            _chunkingOut.destroy();
        _chunkingOut=null;
        if (_nullableOut!=null)
            _nullableOut.destroy();
        _nullableOut=null;
        if (_iso8859writer!=null)
            _iso8859writer.destroy();
        _iso8859writer=null;
        if (_utf8writer!=null)
            _utf8writer.destroy();
        _utf8writer=null;
    }
    
    /* ------------------------------------------------------------ */
    /** Set the trailer to send with a chunked close.
     * @param trailer 
     */
    public void setTrailer(HttpFields trailer)
    {
        if (!isChunking())
            throw new IllegalStateException("Not Chunking");
        _chunkingOut.setTrailer(trailer);
    }
    
    /* ------------------------------------------------------------ */
    public void writeHeader(HttpMessage httpMessage)
        throws IOException
    {
        if (isNullOutput())
            _nullableOut.writeHeader(httpMessage);
        else if (_bufferHeaders)
            _bufferedOut.writeHeader(httpMessage);
        else
            _headerOut.writeHeader(httpMessage);
    }
    
    /* ------------------------------------------------------------ */
    public void write(int b) throws IOException
    {
        prepareOutput();
        out.write(b);
        _bytes++;
    }

    /* ------------------------------------------------------------ */
    public void write(byte b[]) throws IOException
    {
        write(b,0,b.length);
    }

    /* ------------------------------------------------------------ */
    public void write(byte b[], int off, int len)
        throws IOException
    {     
        prepareOutput();
        out.write(b,off,len);
        _bytes+=len;
    }
    
    /* ------------------------------------------------------------ */
    protected void prepareOutput()
        throws IOException
    {   
	if (out==null)
	    throw new IOException("closed");

        if (!_written)
        {
            _written=true;

            if (out==_nullableOut)
            {
                if (_bufferedOut==null)
                {
                    _bufferedOut=new BufferedOutputStream(_nullableOut,
                                                          _bufferSize,
                                                          _headerReserve,0,0);
                    _bufferedOut.setCommitObserver(this);
                    _bufferedOut.setBypassBuffer(true);
                    _bufferedOut.setFixed(true);
                }
                out=_bufferedOut;
                _headerOut=_bufferedOut;
            }
            
            notify(OutputObserver.__FIRST_WRITE);
            
            if (out==_nullableOut)
                notify(OutputObserver.__COMMITING);
        }        
    }
    /* ------------------------------------------------------------ */
    public void flush()
        throws IOException
    {
        if (out!=null)
        {
            if (out==_nullableOut)
                notify(OutputObserver.__COMMITING);
            out.flush();
        }
    }
    
    /* ------------------------------------------------------------ */
    /** Close the stream.
     * @exception IOException 
     */
    public boolean isClosed()
        throws IOException
    {
        return out==null;
    }
    
    /* ------------------------------------------------------------ */
    /** Close the stream.
     * @exception IOException 
     */
    public void close()
        throws IOException
    {        
        // Are we already closed?
        if (out==null)
            return;
        
        // Close
        try {
            if (out==_nullableOut)
                notify(OutputObserver.__COMMITING);
            
            notify(OutputObserver.__CLOSING);
            if (_bufferHeaders)
                _bufferedOut.close();
            else if (out!=null)
                out.close();
            out=null;
            _headerOut=_nullableOut;
            notify(OutputObserver.__CLOSED);
        }
        catch (IOException e)
        {
            Code.ignore(e);
        }
    }

    /* ------------------------------------------------------------ */
    /** Output Notification.
     * Called by the internal Buffered Output and the event is passed on to
     * this streams observers.
     */
    public void outputNotify(OutputStream out, int action, Object ignoredData)
        throws IOException
    {
        notify(action);
    }

    /* ------------------------------------------------------------ */
    /* Notify observers of action.
     * @see OutputObserver
     * @param action the action.
     */
    private void notify(int action)
        throws IOException
    {
        if (_observers!=null)
            for (int i=_observers.size();i-->0;)
            {
                Object data=_observers.get(i--);
                ((OutputObserver)_observers.get(i)).outputNotify(this,action,data);
            }
    }

    /* ------------------------------------------------------------ */
    public void write(InputStream in, int len)
        throws IOException
    {
        IO.copy(in,this,len);
    }

    /* ------------------------------------------------------------ */
    private Writer getISO8859Writer()
        throws IOException
    {
        if (_iso8859writer==null)
            _iso8859writer=new HttpWriter(StringUtil.__ISO_8859_1);
        return _iso8859writer;
    }
    
    /* ------------------------------------------------------------ */
    private Writer getUTF8Writer()
        throws IOException
    {
        if (_utf8writer==null)
            _utf8writer=new HttpWriter("UTF-8");
        return _utf8writer;
    }
    
    /* ------------------------------------------------------------ */
    public Writer getWriter(String encoding)
        throws IOException
    {
        if (encoding==null ||
            StringUtil.__ISO_8859_1.equalsIgnoreCase(encoding)  ||
            "ISO8859_1".equalsIgnoreCase(encoding))
            return getISO8859Writer();

        if ("UTF-8".equalsIgnoreCase(encoding) ||
            "UTF8".equalsIgnoreCase(encoding))
            return getUTF8Writer();

        return new OutputStreamWriter(this,encoding);
    }
    
    /* ------------------------------------------------------------ */
    public String toString()
    {
        return super.toString() +
            "\nout="+out+
            "\nrealOut="+_realOut+
            "\nnullableOut="+_nullableOut+
            "\nheaderOut="+_headerOut+
            "\nbufferedOut="+_bufferedOut+
            "\nchunkingOut="+_chunkingOut;
    }
    
    
    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    private class HttpWriter extends Writer
    {
        private OutputStreamWriter _writer=null;
        private boolean _writting=false;
        private byte[] _buf = ByteArrayPool.getByteArray(4096);
        private String _encoding;
        
        /* -------------------------------------------------------- */
        HttpWriter(String encoding)
        {
            _encoding=encoding;
        }
        
        /* -------------------------------------------------------- */
        public Object getLock()
        {
            return lock;
        }
        
        /* -------------------------------------------------------- */
        public void write(char c)
            throws IOException
        {
            if (_writting)
                _writer.write(c);
            else if (c>=0&&c<=0x7f)
                HttpOutputStream.this.write((int)c);
            else
            {
                char[] ca ={c};
                writeEncoded(ca,0,1);
            }
        }
    
        /* ------------------------------------------------------------ */
        public void write(char[] ca)
            throws IOException
        {
            this.write(ca,0,ca.length);
        }
        
        /* ------------------------------------------------------------ */
        public void write(char[] ca,int offset, int length)
            throws IOException
        {
            if (_writting)
                _writer.write(ca,offset,length);
            else
            {
                HttpOutputStream.this.prepareOutput();
                int s=0;
                for (int i=0;i<length;i++)
                {
                    char c=ca[offset+i];
                    if (c>=0&&c<=0x7f)
                    {
                        _buf[s++]=(byte)c;
                        if (s==_buf.length)
                        {
                            HttpOutputStream.this.getFilterStream().write(_buf,0,s);
                            HttpOutputStream.this._bytes+=s;
                            s=0;
                        }
                    }
                    else
                    {
                        if (s>0)
                        {
                            HttpOutputStream.this.getFilterStream().write(_buf,0,s);
                            HttpOutputStream.this._bytes+=s;
                            s=0;
                        }
                        writeEncoded(ca,offset+i,length-i);
                        break;
                    }
                }
                
                if (s>0)
                {
                    HttpOutputStream.this.getFilterStream().write(_buf,0,s);
                    HttpOutputStream.this._bytes+=s;
                    s=0;
                }
            }
        }
    
        /* ------------------------------------------------------------ */
        public void write(String s)
            throws IOException
        {
            this.write(s,0,s.length());
        }
    
        /* ------------------------------------------------------------ */
        public void write(String str,int offset, int length)
            throws IOException
        {
            if (_writting)
                _writer.write(str,offset,length);
            else
            {
                int s=0;
                HttpOutputStream.this.prepareOutput();
                for (int i=0;i<length;i++)
                {
                    char c=str.charAt(offset+i);
                    if (c>=0&&c<=0x7f)
                    {
                        _buf[s++]=(byte)c;
                        if (s==_buf.length)
                        {
                            HttpOutputStream.this.getFilterStream().write(_buf,0,s);
                            HttpOutputStream.this._bytes+=s;
                            s=0;
                        }
                    }
                    else
                    {
                        if (s>0)
                        {
                            HttpOutputStream.this.getFilterStream().write(_buf,0,s);
                            HttpOutputStream.this._bytes+=s;
                            s=0;
                        }
                        char[] chars = str.toCharArray();
                        writeEncoded(chars,offset+i,chars.length-(offset+i));
                        break;
                    }
                }
                if (s>0)
                {
                    HttpOutputStream.this.getFilterStream().write(_buf,0,s);
                    HttpOutputStream.this._bytes+=s;
                    s=0;
                }
            }
        }

        /* ------------------------------------------------------------ */
        private void writeEncoded(char[] ca,int offset, int length)
            throws IOException
        {
            if (_writer==null)
                _writer = new OutputStreamWriter(HttpOutputStream.this,
                                                 _encoding);
            _writting=true;
            _writer.write(ca,offset,length);
        }
        
        /* ------------------------------------------------------------ */
        public void flush()
            throws IOException
        {
            if (_writting)
                _writer.flush();
            else
                HttpOutputStream.this.flush();
            _writting=false;
        }
        
        /* ------------------------------------------------------------ */
        public void close()
            throws IOException
        {
            if (_writting)
                _writer.flush();
            HttpOutputStream.this.close();
            _writting=false;
        }
        
        /* ------------------------------------------------------------ */
        public void destroy()
        {
            ByteArrayPool.returnByteArray(_buf);
            _buf=null;
            _writer=null;
            _encoding=null;
        }
    }
}
