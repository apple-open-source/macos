// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: AJP13OutputStream.java,v 1.3.2.9 2003/06/04 04:47:45 starksm Exp $
// ========================================================================

package org.mortbay.http.ajp;


import java.io.IOException;
import java.io.OutputStream;
import java.util.Enumeration;
import org.mortbay.http.BufferedOutputStream;
import org.mortbay.http.HttpMessage;
import org.mortbay.http.HttpResponse;
import org.mortbay.util.Code;
import org.mortbay.http.ajp.AJP13ResponsePacket;
import org.mortbay.http.ajp.AJP13Packet;

/** OutputStream for AJP13 protocol.
 * 
 *
 * @version $Revision: 1.3.2.9 $
 * @author Greg Wilkins (gregw)
 */
public class AJP13OutputStream extends BufferedOutputStream
{
    private AJP13ResponsePacket _packet;
    private boolean _complete;
    private boolean _completed;
    private boolean _persistent=true;
    private AJP13ResponsePacket _ajpResponse;

    /* ------------------------------------------------------------ */
    AJP13OutputStream(OutputStream out,int bufferSize)
    {
        super(out,
              bufferSize,
              AJP13ResponsePacket.__DATA_HDR,
              AJP13ResponsePacket.__DATA_HDR,
              0);
        setFixed(true);
        _packet=new AJP13ResponsePacket(_buf);
        _packet.prepare();
        
        setBypassBuffer(false);
        setFixed(true);
        
        _ajpResponse=new AJP13ResponsePacket(bufferSize);
        _ajpResponse.prepare();
    }

    
    /* ------------------------------------------------------------ */
    public void writeHeader(HttpMessage httpMessage)
        throws IOException
    {
        HttpResponse response= (HttpResponse)httpMessage; 
        response.setState(HttpMessage.__MSG_SENDING);
        
        _ajpResponse.resetData();
        _ajpResponse.addByte(AJP13ResponsePacket.__SEND_HEADERS);
        _ajpResponse.addInt(response.getStatus());
        _ajpResponse.addString(response.getReason());
        
        int mark=_ajpResponse.getMark();
        _ajpResponse.addInt(0);        int nh=0;
        Enumeration e1=response.getFieldNames();
        while(e1.hasMoreElements())
        {
            String h=(String)e1.nextElement();
            Enumeration e2=response.getFieldValues(h);
            while(e2.hasMoreElements())
            {
                _ajpResponse.addHeader(h);
                _ajpResponse.addString((String)e2.nextElement());
                nh++;
            }
        }

        if (nh>0)
            _ajpResponse.setInt(mark,nh);
        _ajpResponse.setDataSize();

        write(_ajpResponse);
        
        _ajpResponse.resetData();
    }
    
    /* ------------------------------------------------------------ */
    public void write(AJP13Packet packet)
        throws IOException
    {
        packet.write(_out);
    }
    
    /* ------------------------------------------------------------ */
    public void flush()
        throws IOException
    {
        super.flush();
        if (_complete && !_completed)
        {
            _completed=true;
            
            _packet.resetData();
            _packet.addByte(AJP13ResponsePacket.__END_RESPONSE);
            _packet.addBoolean(_persistent);
            _packet.setDataSize();
            write(_packet);
            _packet.resetData();
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
    public void destroy()
    {
        if (_packet!=null)_packet.destroy();
        _packet=null;
        if (_ajpResponse!=null)_ajpResponse.destroy();
        _ajpResponse=null;
        _out=null;
    }
    
    /* ------------------------------------------------------------ */
    public void end()
        throws IOException
    {
        _persistent=false;
    }
    
    /* ------------------------------------------------------------ */
    protected void wrapBuffer()
        throws IOException
    {
        if (size()==0)
            return;

        if (_buf!=_packet.getBuffer())
        {
            _packet=new AJP13ResponsePacket(_buf);
            _packet.prepare();
        }

        prewrite(_buf,0,AJP13ResponsePacket.__DATA_HDR);
        _packet.resetData();
        _packet.addByte(AJP13ResponsePacket.__SEND_BODY_CHUNK);
        _packet.setDataSize(size()-AJP13ResponsePacket.__HDR_SIZE);
    }
    
    /* ------------------------------------------------------------ */
    protected void bypassWrite(byte[] b, int offset, int length)
        throws IOException
    {
        Code.notImplemented();
    }
   
    /* ------------------------------------------------------------ */
    public void writeTo(OutputStream out)
        throws IOException
    {
        int sz = size();
        
        if (sz<=AJP13ResponsePacket.__MAX_BUF)
            super.writeTo(out);
        else
        {
            int offset=preReserve();
            int data=sz-AJP13ResponsePacket.__DATA_HDR;
            
            while (data>AJP13ResponsePacket.__MAX_DATA)
            {   
                _packet.setDataSize(AJP13ResponsePacket.__MAX_BUF-AJP13ResponsePacket.__HDR_SIZE);
                if (offset>0)
                    System.arraycopy(_buf,0,_buf,offset,AJP13ResponsePacket.__DATA_HDR);
                out.write(_buf,offset,AJP13ResponsePacket.__MAX_BUF);
                
                data-=AJP13ResponsePacket.__MAX_DATA;
                offset+=AJP13ResponsePacket.__MAX_DATA;
            }
            
            int len=data+AJP13ResponsePacket.__DATA_HDR;
            _packet.setDataSize(len-AJP13ResponsePacket.__HDR_SIZE);
            if (offset>0)
                System.arraycopy(_buf,0,_buf,offset,AJP13ResponsePacket.__DATA_HDR);
            out.write(_buf,offset,len);
        }
    }
}
