// ========================================================================
// Copyright (c) 2000 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: ServletWriter.java,v 1.15.2.5 2003/06/04 04:47:52 starksm Exp $
// ========================================================================

package org.mortbay.jetty.servlet;

import java.io.IOException;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.io.UnsupportedEncodingException;
import org.mortbay.http.HttpOutputStream;
import org.mortbay.util.Code;
import org.mortbay.util.IO;


/* ------------------------------------------------------------ */
/** Servlet PrintWriter.
 * This writer can be disabled.
 * It is crying out for optimization.
 *
 * @version $Revision: 1.15.2.5 $
 * @author Greg Wilkins (gregw)
 */
class ServletWriter extends PrintWriter
{
    String encoding=null;
    OutputStream os=null;
    boolean written=false;
    
    /* ------------------------------------------------------------ */
    ServletWriter(OutputStream os)
        throws IOException
    {
        super((os instanceof HttpOutputStream) 
              ?((HttpOutputStream)os).getWriter(null)
              :new OutputStreamWriter(os));
        this.os=os;
    }
    
    /* ------------------------------------------------------------ */
    ServletWriter(OutputStream os, String encoding)
        throws IOException
    {
        super((os instanceof HttpOutputStream)
              ?((HttpOutputStream)os).getWriter(encoding)
              :new OutputStreamWriter(os,encoding));
        this.os=os;
        this.encoding=encoding;
    }

    /* ------------------------------------------------------------ */
    public void disable()
    {
        out=IO.getNullWriter();
    }
    
    /* ------------------------------------------------------------ */
    public void reset()
    {
        try{
            out=IO.getNullWriter();
            super.flush();
            out=new OutputStreamWriter(os,encoding);
            written=false;
        }
        catch(UnsupportedEncodingException e)
        {
            Code.fail(e);
        }
    }
    

    /* ------------------------------------------------------------ */
    public boolean isWritten()
    {
        return written;
    }

    
    /* ------------------------------------------------------------ */
    public void print(boolean p)  {written=true;super.print(p);}
    public void print(char p)     {written=true;super.print(p);}
    public void print(char[] p)   {written=true;super.print(p);}
    public void print(double p)   {written=true;super.print(p);}
    public void print(float p)    {written=true;super.print(p);}
    public void print(int p)      {written=true;super.print(p);}
    public void print(long p)     {written=true;super.print(p);}
    public void print(Object p)   {written=true;super.print(p);}
    public void print(String p)   {written=true;super.print(p);}
    public void println()         {written=true;super.println();}
    public void println(boolean p){written=true;super.println(p);}
    public void println(char p)   {written=true;super.println(p);}
    public void println(char[] p) {written=true;super.println(p);}
    public void println(double p) {written=true;super.println(p);}
    public void println(float p)  {written=true;super.println(p);}
    public void println(int p)    {written=true;super.println(p);}
    public void println(long p)   {written=true;super.println(p);}
    public void println(Object p) {written=true;super.println(p);}
    public void println(String p) {written=true;super.println(p);}

    
    public void write(int c)
    {
        try
        {
            if (out==null)
                throw new IOException("closed");
            written=true;
            out.write(c);
        }
        catch (IOException e){Code.ignore(e);setError();}
    }
    
    public void write(char[] cbuf, int off, int len)
    {
        try
        {
            if (out==null)
                throw new IOException("closed");
            written=true;
            out.write(cbuf,off,len);
        }
        catch (IOException e){Code.ignore(e);setError();}
    }
    
    public void write(char[] cbuf)
    {
        try
        {
            if (out==null)
                throw new IOException("closed");
            written=true;
            out.write(cbuf,0,cbuf.length);
        }
        catch (IOException e){Code.ignore(e);setError();}
    }

    public void write(String s, int off, int len)
    {
        try
        {
            if (out==null)
                throw new IOException("closed");
            written=true;
            out.write(s,off,len);
        }
        catch (IOException e){Code.ignore(e);setError();}
    }

    public void write(String s)
    {
        try
        {
            if (out==null)
                throw new IOException("closed");
            written=true;
            out.write(s,0,s.length());
        }
        catch (IOException e){Code.ignore(e);setError();}
    }
}
