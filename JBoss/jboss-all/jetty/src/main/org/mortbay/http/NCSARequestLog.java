// ========================================================================
// Copyright (c) 2000 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: NCSARequestLog.java,v 1.17.2.11 2003/06/04 04:47:42 starksm Exp $
// ========================================================================

package org.mortbay.http;

import java.io.BufferedOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.io.Writer;
import java.util.Locale;
import java.util.TimeZone;
import org.mortbay.util.Code;
import org.mortbay.util.DateCache;
import org.mortbay.util.RolloverFileOutputStream;
import org.mortbay.util.StringUtil;


/* ------------------------------------------------------------ */
/** NCSA HTTP Request Log.
 * NCSA common or NCSA extended (combined) request log.
 * @version $Id: NCSARequestLog.java,v 1.17.2.11 2003/06/04 04:47:42 starksm Exp $
 * @author Tony Thompson
 * @author Greg Wilkins
 */
public class NCSARequestLog implements RequestLog
{
    private String _filename;
    private boolean _extended;
    private boolean _append;
    private boolean _buffered;
    private int _retainDays;
    private boolean _closeOut;
    private String _logDateFormat="dd/MMM/yyyy:HH:mm:ss ZZZ";
    private Locale _logLocale=Locale.US;
    private String _logTimeZone=TimeZone.getDefault().getID();
    private String[] _ignorePaths;
    
    private transient OutputStream _out;
    private transient OutputStream _fileOut;
    private transient DateCache _logDateCache;
    private transient PathMap _ignorePathMap;
    private transient Writer _writer;
    
    /* ------------------------------------------------------------ */
    /** Constructor.
     */
    public NCSARequestLog()
    {
        _extended=true;
        _append=true;
        _retainDays=31;
        _buffered=false;
    }
    
    /* ------------------------------------------------------------ */
    /** Constructor. 
     * @param filename Filename, which can be in
     * rolloverFileOutputStream format
     * @see org.mortbay.util.RolloverFileOutputStream
     * @exception IOException 
     */
    public NCSARequestLog(String filename)
        throws IOException
    {
        _extended=true;
        _append=true;
        _retainDays=31;
        setFilename(filename);
        _buffered=(filename!=null);
    }

    /* ------------------------------------------------------------ */
    /** Set the log filename.
     * @see NCSARequestLog#setRetainDays(int)
     * @param filename The filename to use. If the filename contains the
     * string "yyyy_mm_dd", then a RolloverFileOutputStream is used and the
     * log is rolled over nightly and aged according setRetainDays. If no
     * filename is set or a null filename
     * passed, then requests are logged to System.err.
     */
    public void setFilename(String filename)
    {
        if (filename!=null)
        {
            filename=filename.trim();
            if (filename.length()==0)
                filename=null;
        }
        _filename=filename;        
    }

    /* ------------------------------------------------------------ */
    /** Get the log filename.
     * @see NCSARequestLog#getDatedFilename()
     * @return The log filename without any date expansion.
     */
    public String getFilename()
    {
        return _filename;
    }

    /* ------------------------------------------------------------ */
    /** Get the dated log filename.
     * @see NCSARequestLog#getFilename()
     * @return The log filename with any date encoding expanded.
     */
    public String getDatedFilename()
    {
        if (_fileOut instanceof RolloverFileOutputStream)
            return ((RolloverFileOutputStream)_fileOut).getDatedFilename();
        return null;
    }
    
    /* ------------------------------------------------------------ */
    /** Is output buffered.
     * @return True if the log output is buffered (which can increase performance).
     */
    public boolean isBuffered()
    {
        return _buffered;
    }
    
    /* ------------------------------------------------------------ */
    /** Set output buffering.
     * @param buffered True if the log output is buffered (which can increase performance).
     */
    public void setBuffered(boolean buffered)
    {
        _buffered = buffered;
    }
    
    
    /* ------------------------------------------------------------ */
    /** 
     * @param format The date format to use within the log file.
     */
    public void setLogDateFormat(String format)
    {
        _logDateFormat=format;
    }

    /* ------------------------------------------------------------ */
    /** 
     * @return The date format to use within the log file.
     */
    public String getLogDateFormat()
    {
        return _logDateFormat;
    }
    
    /* ------------------------------------------------------------ */
    /** 
     * @param tz The date format timezone to use within the log file.
     */
    public void setLogTimeZone(String tz)
    {
        _logTimeZone=tz;
    }

    /* ------------------------------------------------------------ */
    /** 
     * @return The date format timezone to use within the log file.
     */
    public String getLogTimeZone()
    {
        return _logTimeZone;
    }
    
    /* ------------------------------------------------------------ */
    /** 
     * @return The number of days to retain rollovered log files.
     */
    public int getRetainDays()
    {
        return _retainDays;
    }

    /* ------------------------------------------------------------ */
    /** 
     * @param retainDays The number of days to retain rollovered log files.
     */
    public void setRetainDays(int retainDays)
    {
        _retainDays = retainDays;
    }

    /* ------------------------------------------------------------ */
    /** 
     * @return True if NCSA extended format is to be used.
     */
    public boolean isExtended()
    {
        return _extended;
    }

    /* ------------------------------------------------------------ */
    /** 
     * @param e True if NCSA extended format is to be used.
     */
    public void setExtended(boolean e)
    {
        _extended=e;
    }
    
    /* ------------------------------------------------------------ */
    /** 
     * @return True if logs are appended to existing log files.
     */
    public boolean isAppend()
    {
        return _append;
    }

    /* ------------------------------------------------------------ */
    /** 
     * @param a True if logs are appended to existing log files.
     */
    public void setAppend(boolean a)
    {
        _append=a;
    }
    
    /* ------------------------------------------------------------ */
    /** Set which paths to ignore.
     *
     * @param ignorePaths Array of path specifications to ignore
     */
    public void setIgnorePaths(String[] ignorePaths)
    {
        // Contributed by  Martin Vilcans (martin@jadestone.se)
        _ignorePaths = ignorePaths;
    }

    /* ------------------------------------------------------------ */
    public String[] getIgnorePaths()
    {
        return _ignorePaths;
    }
    
    /* ------------------------------------------------------------ */
    public void start()
        throws Exception
    {
        _logDateCache=new DateCache(_logDateFormat,_logLocale);
        _logDateCache.setTimeZoneID(_logTimeZone);
        
        if (_filename != null)
        {
            _fileOut=new RolloverFileOutputStream(_filename,_append,_retainDays);
            _closeOut=true;
        }
        else
            _fileOut=System.err;

        if (_buffered)
            _out=new BufferedOutputStream(_fileOut);
        else
            _out=_fileOut;

        if (_ignorePaths!=null && _ignorePaths.length>0)
        {
            _ignorePathMap=new PathMap();
            for (int i=0;i<_ignorePaths.length;i++)
                _ignorePathMap.put(_ignorePaths[i],_ignorePaths[i]);
        }
        else
            _ignorePathMap=null;

        _writer=new OutputStreamWriter(_out);
    }

    /* ------------------------------------------------------------ */
    public boolean isStarted()
    {
        return _fileOut!=null;
    }
    
    /* ------------------------------------------------------------ */
    public void stop()
    {
        try{if (_writer!=null)_writer.flush();} catch (IOException e){Code.ignore(e);}
        if (_out!=null && _closeOut)
            try{_out.close();}catch(IOException e){Code.ignore(e);}
        _out=null;
        _fileOut=null;
        _closeOut=false;
        _logDateCache=null;
        _writer=null;
    }
    
    /* ------------------------------------------------------------ */
    /** Log a request.
     * @param request The request
     * @param response The response to this request.
     * @param responseLength The bytes written to the response.
     */
    public void log(HttpRequest request,
                    HttpResponse response,
                    int responseLength)
    {
        try{
            // ignore ignorables
            if (_ignorePathMap != null &&
                _ignorePathMap.getMatch(request.getPath()) != null)
                return;

            // log the rest
            if (_fileOut==null)
                return;

            StringBuffer buf = new StringBuffer(160);
            
            buf.append(request.getRemoteAddr());
            buf.append(" - ");
            String user = request.getAuthUser();
            buf.append((user==null)?"-":user);
            buf.append(" [");
            buf.append(_logDateCache.format(request.getTimeStamp()));
            buf.append("] \"");
            buf.append(request.getMethod());
            buf.append(' ');
            buf.append(request.getURI());
            buf.append(' ');
            buf.append(request.getVersion());
            buf.append("\" ");
            int status=response.getStatus();    
            buf.append((char)('0'+((status/100)%10)));
            buf.append((char)('0'+((status/10)%10)));
            buf.append((char)('0'+(status%10)));
            if (responseLength>=0)
            {
                buf.append(' ');
                if (responseLength>99999)
                    buf.append(Integer.toString(responseLength));
                else
                {
                    if (responseLength>9999) 
                        buf.append((char)('0'+((responseLength/10000)%10)));
                    if (responseLength>999) 
                        buf.append((char)('0'+((responseLength/1000)%10)));
                    if (responseLength>99) 
                        buf.append((char)('0'+((responseLength/100)%10)));
                    if (responseLength>9) 
                        buf.append((char)('0'+((responseLength/10)%10)));
                    buf.append((char)('0'+(responseLength%10)));
                }
                buf.append(' ');
            }
            else
                buf.append(" - ");

            String log =buf.toString();
            synchronized(_writer)
            {
                _writer.write(log);
                if (_extended)
                    logExtended(request,response,_writer);
                _writer.write(StringUtil.__LINE_SEPARATOR);
                if (!_buffered)
                     _writer.flush();
            }
        }
        catch(IOException e)
        {
            Code.warning(e);
        }
    }
    
    /* ------------------------------------------------------------ */
    /** Log Extended fields.
     * This method can be extended by a derived class to add extened fields to
     * each log entry.  It is called by the log method after all standard
     * fields have been added, but before the line terminator.
     * Derived implementations should write extra fields to the Writer
     * provided.
     * The default implementation writes the referer and user agent.
     * @param request The request to log.
     * @param response The response to log.
     * @param log The writer to write the extra fields to.
     * @exception IOException Problem writing log
     */
    protected void logExtended(HttpRequest request,
                               HttpResponse response,
                               Writer log)
        throws IOException
    {
        String referer = request.getField(HttpFields.__Referer);
        if(referer==null)
            log.write("\"-\" ");
        else
        {
            log.write('"');
            log.write(referer);
            log.write("\" ");
        }
        
        String agent = request.getField(HttpFields.__UserAgent);
        if(agent==null)
            log.write("\"-\"");
        else
        {
            log.write('"');
            log.write(agent);
            log.write('"');
        }
    }
}

