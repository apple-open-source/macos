// ========================================================================
// Copyright (c) 2000 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: ServletOut.java,v 1.15.2.4 2003/07/11 00:55:03 jules_gosnell Exp $
// ========================================================================

package org.mortbay.jetty.servlet;

import java.io.IOException;
import java.io.OutputStream;
import javax.servlet.ServletOutputStream;
import org.mortbay.util.IO;


class ServletOut extends ServletOutputStream
{
    OutputStream _out;

    /* ------------------------------------------------------------ */
    ServletOut(OutputStream out)
    {
        _out=out;
    }
    
    /* ------------------------------------------------------------ */
    public void write(int ch)
        throws IOException
    {
        _out.write(ch);
    }
    
    /* ------------------------------------------------------------ */
    public void write(byte[]b)
        throws IOException
    {
        _out.write(b);
    }
    
    /* ------------------------------------------------------------ */
    public void write(byte[]b,int o,int l)
        throws IOException
    {
        _out.write(b,o,l);
    }

    /* ------------------------------------------------------------ */
    public void flush()
        throws IOException
    {
        _out.flush();
    }
    
    /* ------------------------------------------------------------ */
    public void close()
        throws IOException
    {
        super.close();
        _out.close();
    }
    
    /* ------------------------------------------------------------ */
    public void disable()
        throws IOException
    {
        _out=IO.getNullStream();
    }

    /* ------------------------------------------------------------ */
    public void print(String s) throws IOException 
    {
         if (s!=null) write(s.getBytes());
    }

    /* ------------------------------------------------------------ */
    public void println(String s) throws IOException 
    {
         if (s!=null) write(s.getBytes());     
         write(IO.CRLF_BYTES);
    }
}
