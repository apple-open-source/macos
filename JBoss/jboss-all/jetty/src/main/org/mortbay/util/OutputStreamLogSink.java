// ========================================================================
// Copyright (c) 1997 MortBay Consulting, Sydney
// $Id: OutputStreamLogSink.java,v 1.15.2.9 2003/06/04 04:47:58 starksm Exp $
// ========================================================================

package org.mortbay.util;

import java.io.IOException;
import java.io.OutputStream;
import java.util.TimeZone;


/* ------------------------------------------------------------ */
/** A Log sink.
 * This class represents both a concrete or abstract sink of
 * Log data.  The default implementation logs to System.err, but
 * other output stream or files may be specified.
 *
 * Currently this Stream only writes in ISO8859_1 encoding.  For
 * Other encodings use the less efficient WriterLogSink.
 *
 * If a logFilename is specified, output is sent to that file.
 * If the filename contains "yyyy_mm_dd", the log file date format
 * is used to create the actual filename and the log file is rolled
 * over at local midnight.
 * If append is set, existing logfiles are appended to, otherwise
 * a backup is created with a timestamp.
 * Dated log files are deleted after retain days.
 * 
 * <p> If the property LOG_DATE_FORMAT is set, then it is interpreted
 * as a format string for java.text.SimpleDateFormat and used to
 * format the log timestamps. Default value: HH:mm:ss.SSS
 *
 * <p> If LOG_TIMEZONE is set, it is used to set the timezone of the log date
 * format, otherwise GMT is used.
 *
 * @see org.mortbay.util.Log
 * @version $Id: OutputStreamLogSink.java,v 1.15.2.9 2003/06/04 04:47:58 starksm Exp $
 * @author Greg Wilkins (gregw)
 */
public class OutputStreamLogSink
    implements LogSink
{
    /*-------------------------------------------------------------------*/
    public final static char OPT_TIMESTAMP = 't';
    public final static char OPT_LABEL = 'L';
    public final static char OPT_TAG = 'T';
    public final static char OPT_STACKSIZE = 's';
    public final static char OPT_STACKTRACE = 'S';
    public final static char OPT_ONELINE = 'O';
    
    /* ------------------------------------------------------------ */
    private final static String __lineSeparator =
        System.getProperty("line.separator");

    /*-------------------------------------------------------------------*/
    private int _retainDays =Integer.getInteger("LOG_FILE_RETAIN_DAYS",31).intValue();
    
    protected DateCache _dateFormat=
        new DateCache(System.getProperty("LOG_DATE_FORMAT","HH:mm:ss.SSS"));
    protected String _logTimezone=
	System.getProperty("LOG_TIME_ZONE");    
    {
        if (_logTimezone!=null)
            _dateFormat.getFormat().setTimeZone(TimeZone.getTimeZone(_logTimezone));
    }

    /* ------------------------------------------------------------ */
    protected boolean _logTimeStamps=true;
    protected boolean _logLabels=true;
    protected boolean _logTags=true;
    protected boolean _logStackSize=true;
    protected boolean _logStackTrace=false;
    protected boolean _logOneLine=false;
    
    /*-------------------------------------------------------------------*/
    private String _filename;
    private boolean _append=true;
    protected boolean _flushOn=true;
    protected int _bufferSize=2048;
    protected boolean _reopen=false;

    protected transient boolean _started;
    protected transient OutputStream _out;
    protected transient ByteArrayISO8859Writer _buffer;

    /* ------------------------------------------------------------ */
    /** Constructor. 
     */
    public OutputStreamLogSink()
        throws IOException
    {
        _filename=System.getProperty("LOG_FILE");
        if (_filename==null)
            _out=System.err;
    }
        
    /* ------------------------------------------------------------ */
    public OutputStreamLogSink(String filename)
    {
        _filename=filename;
    }
    
    /* ------------------------------------------------------------ */
    public void setOptions(String logOptions)
    {  
        setOptions((logOptions.indexOf(OPT_TIMESTAMP) >= 0),
                   (logOptions.indexOf(OPT_LABEL) >= 0),
                   (logOptions.indexOf(OPT_TAG) >= 0),
                   (logOptions.indexOf(OPT_STACKSIZE) >= 0),
                   (logOptions.indexOf(OPT_STACKTRACE) >= 0),
                   (logOptions.indexOf(OPT_ONELINE) >= 0));
    }
    
    /* ------------------------------------------------------------ */
    public String getOptions()
    {
        return
            (_logTimeStamps?"t":"")+
            (_logLabels?"L":"")+
            (_logTags?"T":"")+
            (_logStackSize?"s":"")+
            (_logStackTrace?"S":"")+
            (_logOneLine?"O":"");
    }
    
    
    /*-------------------------------------------------------------------*/
    /** Set the log options.
     */
    public void setOptions(boolean logTimeStamps,
                           boolean logLabels,
                           boolean logTags,
                           boolean logStackSize,
                           boolean logStackTrace,
                           boolean logOneLine)
    {
        _logTimeStamps      = logTimeStamps;
        _logLabels          = logLabels;
        _logTags            = logTags;
        _logStackSize       = logStackSize;
        _logStackTrace      = logStackTrace;
        _logOneLine         = logOneLine;
    }
    
    /* ------------------------------------------------------------ */
    public String getLogDateFormat()
    {
        return _dateFormat.getFormatString();
    }
    
    /* ------------------------------------------------------------ */
    public void setLogDateFormat(String logDateFormat)
    {
        _dateFormat = new DateCache(logDateFormat);
        if (_logTimezone!=null)
            _dateFormat.getFormat().setTimeZone(TimeZone.getTimeZone(_logTimezone));
    }

    
    /* ------------------------------------------------------------ */
    /** 
     * @deprecated Use getLogTimeZone() 
     */
    public String getLogTimezone()
    {
        return _logTimezone;
    }
    
    /* ------------------------------------------------------------ */
    /** 
     * @deprecated Use setLogTimeZone(String) 
     */
    public void setLogTimezone(String logTimezone)
    {
        _logTimezone=logTimezone;
        if (_dateFormat!=null && _logTimezone!=null)
            _dateFormat.getFormat().setTimeZone(TimeZone.getTimeZone(_logTimezone));
    }
    
    /* ------------------------------------------------------------ */
    public String getLogTimeZone()
    {
        return _logTimezone;
    }
    
    /* ------------------------------------------------------------ */
    public void setLogTimeZone(String logTimezone)
    {
        _logTimezone=logTimezone;
        if (_dateFormat!=null && _logTimezone!=null)
            _dateFormat.getFormat().setTimeZone(TimeZone.getTimeZone(_logTimezone));
    }
    
    /* ------------------------------------------------------------ */
    public boolean isLogTimeStamps()
    {
        return _logTimeStamps;
    }
    /* ------------------------------------------------------------ */
    public void setLogTimeStamps(boolean logTimeStamps)
    {
        _logTimeStamps = logTimeStamps;
    }
    /* ------------------------------------------------------------ */
    public boolean isLogLabels()
    {
        return _logLabels;
    }
    /* ------------------------------------------------------------ */
    public void setLogLabels(boolean logLabels)
    {
        _logLabels = logLabels;
    }
    /* ------------------------------------------------------------ */
    public boolean isLogTags()
    {
        return _logTags;
    }
    /* ------------------------------------------------------------ */
    public void setLogTags(boolean logTags)
    {
        _logTags = logTags;
    }
    /* ------------------------------------------------------------ */
    public boolean isLogStackSize()
    {
        return _logStackSize;
    }
    /* ------------------------------------------------------------ */
    public void setLogStackSize(boolean logStackSize)
    {
        _logStackSize = logStackSize;
    }
    /* ------------------------------------------------------------ */
    public boolean isLogStackTrace()
    {
        return _logStackTrace;
    }
    /* ------------------------------------------------------------ */
    public void setLogStackTrace(boolean logStackTrace)
    {
        _logStackTrace = logStackTrace;
    }
    /* ------------------------------------------------------------ */
    public boolean isLogOneLine()
    {
        return _logOneLine;
    }
    /* ------------------------------------------------------------ */
    public void setLogOneLine(boolean logOneLine)
    {
        _logOneLine = logOneLine;
    }

    /* ------------------------------------------------------------ */
    public boolean isAppend()
    {
        return _append;
    }

    /* ------------------------------------------------------------ */
    public void setAppend(boolean a)
    {
        _append=a;
    }
    
    /* ------------------------------------------------------------ */
    public synchronized void setOutputStream(OutputStream out)
    {
        _reopen=isStarted() && out!=out;
        _filename=null;
        if (_buffer!=null)
            _buffer.resetWriter();
        _out=out;
    }

    /* ------------------------------------------------------------ */
    public OutputStream getOutputStream()
    {
        return _out;
    }

    /* ------------------------------------------------------------ */
    public  synchronized void setFilename(String filename)
    {
        if (filename!=null)
        {
            filename=filename.trim();
            if (filename.length()==0)
                filename=null;
        }
        _reopen=isStarted() &&
            ((_filename==null && filename!=null)||
             (_filename!=null && !_filename.equals(filename)));
        _filename=filename;

        if (!isStarted() && _filename!=null)
            _out=null;
    }

    /* ------------------------------------------------------------ */
    public String getFilename()
    {
        return _filename;
    }
    
    /* ------------------------------------------------------------ */
    public String getDatedFilename()
    {
        if (_filename==null)
            return null;

        if (_out==null || ! (_out instanceof RolloverFileOutputStream))
            return null;

        return ((RolloverFileOutputStream)_out).getDatedFilename();
    }
    
    /* ------------------------------------------------------------ */
    public int getRetainDays()
    {
        return _retainDays;
    }

    /* ------------------------------------------------------------ */
    public void setRetainDays(int retainDays)
    {
        _reopen=isStarted() && _retainDays!=retainDays;
        _retainDays = retainDays;
    }
    
    /* ------------------------------------------------------------ */
    /** 
     * @param on If true, log is flushed on every log. 
     */
    public void setFlushOn(boolean on)
    {
        _flushOn=on;
        if (on && _out!=null)
        {
            try{_out.flush();}
            catch(IOException e){e.printStackTrace();}
        }
        
    }
    
    /* ------------------------------------------------------------ */
    /** 
     * @return true, log is flushed on every log. 
     */
    public boolean getFlushOn()
    {
        return _flushOn;
    }
    
    /* ------------------------------------------------------------ */
    /** Log a message.
     * This method formats the log information as a string and calls
     * log(String).  It should only be specialized by a derived
     * implementation if the format of the logged messages is to be changed.
     *
     * @param tag Tag for type of log
     * @param msg The message
     * @param frame The frame that generated the message.
     * @param time The time stamp of the message.
     */
    public  synchronized void log(String tag,
                                  Object msg,
                                  Frame frame,
                                  long time)
    {
        StringBuffer buf = new StringBuffer(160);
        
        // Log the time stamp
        if (_logTimeStamps)
        {
            buf.append(_dateFormat.format(time));
            buf.append(' ');
        }
            
            
        // Log the tag
        if (_logTags)
            buf.append(tag);
        
        // Log the label
        if (_logLabels && frame != null)
        {
            buf.append(frame.toString());
        }
        
        // Log the stack depth.
        if (_logStackSize && frame != null)
        {
            if (frame.getDepth()<10)    
                buf.append('0');
            buf.append(Integer.toString(frame.getDepth()));  
            buf.append("> ");
        }
        
        // Determine the indent string for the message and append it
        // to the buffer. Only put a newline in the buffer if the first
        // line is not blank
        String nl=__lineSeparator;
        
        if (_logLabels && !_logOneLine && _buffer.size() > 0)
            buf.append(nl);
        
        // Log indented message
        String smsg=(msg==null)
            ?"???"
            :((msg instanceof String)?((String)msg):msg.toString());
        
        if (_logOneLine)
        {
            smsg=StringUtil.replace(smsg,"\015\012","<|");
            smsg=StringUtil.replace(smsg,"\015","<");
            smsg=StringUtil.replace(smsg,"\012","|");
        }
        else
        {
            smsg=StringUtil.replace(smsg,"\015\012","<|");
            smsg=StringUtil.replace(smsg,"\015","<|");
            smsg=StringUtil.replace(smsg,"\012","<|");
            smsg=StringUtil.replace(smsg,"<|",nl);
        }
        buf.append(smsg);
        
        // Add stack frame to message
        if (_logStackTrace && frame != null)
        {
            buf.append(nl);
            buf.append(frame.getStack());
        }
        
        log(buf.toString());
    }
    
    /* ------------------------------------------------------------ */
    /** Log a message.
     * The formatted log string is written to the log sink. The default
     * implementation writes the message to an outputstream.
     * @param formattedLog 
     */
    public synchronized void log(String formattedLog)
    {
        if (_reopen)
        {
            stop();
            start();
        }
        try
        {
            _buffer.write(formattedLog);
            _buffer.write(StringUtil.__LINE_SEPARATOR);
            if (_flushOn || _buffer.size()>_bufferSize)
            {
                _buffer.writeTo(_out);
                _buffer.resetWriter();
                _out.flush();
            }
        }
        catch(IOException e){e.printStackTrace();}
    }

    
    /* ------------------------------------------------------------ */
    /** Start a log sink.
     * The default implementation does nothing 
     */
    public synchronized void start()
    {
        _buffer=new ByteArrayISO8859Writer(_bufferSize);
        _reopen=false;
        if (_started)
            return;
        
        if (_out==null && _filename!=null)
        {
            try
            {
                RolloverFileOutputStream rfos=
                    new RolloverFileOutputStream(_filename,_append,_retainDays);
                _out=rfos;
            }
            catch(IOException e){e.printStackTrace();}   
        }

        if (_out==null)
            _out=System.err;
        
        _started=true;
    }
    
    
    /* ------------------------------------------------------------ */
    /** Stop a log sink.
     * An opportunity for subclasses to clean up. The default
     * implementation does nothing 
     */
    public synchronized void stop()
    {
        _started=false;

        if (_out!=null)
        {
            try
            {
                if (_buffer.size()>0)
                {
                    _buffer.writeTo(_out);
                }
                _out.flush();
                _buffer=null;
            }
            catch(Exception e){if (Code.debug())e.printStackTrace();}
            Thread.yield();
        }
        
        if (_out!=null && _out!=System.err)
        {
            try{_out.close();}
            catch(Exception e){if (Code.debug())e.printStackTrace();}
        }

        if (_filename!=null)
            _out=null;
    }

    /* ------------------------------------------------------------ */
    public boolean isStarted()
    {
        return _started;
    }    
};
