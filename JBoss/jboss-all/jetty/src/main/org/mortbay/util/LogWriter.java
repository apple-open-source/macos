// ===========================================================================
// LogWriter.java
// ===========================================================================

package org.mortbay.util;

import java.io.CharArrayWriter;
import java.io.IOException;
import java.io.Writer;

/* ------------------------------------------------------------ */
/** 
*  A Writer that writes to the Log when it is flushed. For best results
*  wrap with a PrintWriter configured to flush on println() for example
*  <code>
*  PrintWriter out = new PrintWriter(new LogWriter(), true);
*  </code>
*  @author Kent Johnson
*/
public class LogWriter extends Writer
{

    private CharArrayWriter buf = new CharArrayWriter();
    private static final String lineEnd = System.getProperty("line.separator");
    
    /* ------------------------------------------------------------ */
    public LogWriter()
    {
        this.lock = buf;
    }
    
    /* ------------------------------------------------------------ */
    public void write(int c)
        throws IOException
    {
        buf.write(c);
    }

    /* ------------------------------------------------------------ */
    public void write(char cbuf[])
        throws IOException
    {
        buf.write(cbuf, 0, cbuf.length);
    }

    /* ------------------------------------------------------------ */
    public void write(char cbuf[], int off, int len)
        throws IOException
    {
        buf.write(cbuf, off, len);
    }

    /* ------------------------------------------------------------ */
    public void write(String str)
        throws IOException
    {
        buf.write(str, 0, str.length());
    }

    /* ------------------------------------------------------------ */
    public void write(String str, int off, int len)
        throws IOException
    {
        buf.write(str, off, len);
    }

    /* ------------------------------------------------------------ */
    public void flush()
        throws IOException
    {
        synchronized (lock)
        {
            String line = buf.toString();
            if (line.endsWith(lineEnd))
                line = line.substring(0, line.length() - lineEnd.length());
            Log.event(line);
            buf.reset();
        }
    }
    
    /* ------------------------------------------------------------ */
    public void close()
        throws IOException
    {
        synchronized (lock)
        {
            flush();
            buf = null;
        }
    }

}   // LogWriter



