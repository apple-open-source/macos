// ========================================================================
// Copyright (c) 1997 MortBay Consulting, Sydney
// $Id: LoggerLogSink.java,v 1.1.2.2 2003/06/04 04:47:58 starksm Exp $
// ========================================================================

package org.mortbay.util;

import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.logging.LogRecord;

/** JDK 1.4 Logger LogSink
 * This implementation of LogSink can be used to direct messages to
 * the JDK 1.4 log mechanism.
 * @version $Id: LoggerLogSink.java,v 1.1.2.2 2003/06/04 04:47:58 starksm Exp $
 * @author Greg Wilkins (gregw)
 */
public class LoggerLogSink
    implements LogSink
{
    /* ------------------------------------------------------------ */
    private static StringMap __tagMap = new StringMap();
    static
    {
        __tagMap.put(Log.DEBUG,Level.FINE);
        __tagMap.put(Log.EVENT,Level.INFO);
        __tagMap.put(Log.WARN,Level.WARNING);
        __tagMap.put(Log.ASSERT,Level.WARNING);
        __tagMap.put(Log.FAIL,Level.SEVERE);
    }
    
    /* ------------------------------------------------------------ */
    private String _name="org.mortbay";
    private Logger _logger;
    private boolean _ownLogger;
    private boolean _started;
    
    /* ------------------------------------------------------------ */
    /** Constructor. 
     */
    public LoggerLogSink()
    {}
    
    /* ------------------------------------------------------------ */
    /** Constructor. 
     */
    public LoggerLogSink(String name)
    {
        _name=name;
    }

    /* ------------------------------------------------------------ */
    public String getName()
    {
        return _name;
    }
    
    /* ------------------------------------------------------------ */
    public void setName(String name)
    {
        _name=name;
    }
    
    /* ------------------------------------------------------------ */
    public Logger getLogger()
    {
        return _logger;
    }
    
    /* ------------------------------------------------------------ */
    public void setLogger(Logger logger)
    {
        _ownLogger=false;
        _logger=logger;
    }
    
    /* ------------------------------------------------------------ */
    public void start() throws Exception
    {
        if (_logger==null)
        {
            _logger=Logger.getLogger(_name);
            _ownLogger=true;
        }
        _started=true;
    }
    
    /* ------------------------------------------------------------ */
    public void stop() throws InterruptedException
    {
        _started=false;
        if (_ownLogger)
        {
            _logger=null;
            _ownLogger=false;
        }
    }
    
    /* ------------------------------------------------------------ */
    public boolean isStarted()
    {
        return _started;
    }
    
    /*-------------------------------------------------------------------*/
    public void setOptions(String options) {}
    
    /* ------------------------------------------------------------ */
    public String getOptions() {return null;}
    
    /* ------------------------------------------------------------ */
    public void log(String tag,
                    Object msg,
                    Frame frame,
                    long time)
    {
        Level level=(Level)__tagMap.get(tag);
        LogRecord lr = new LogRecord(level,msg.toString());
        lr.setMillis(time);
        if (frame!=null)
        {
            StackTraceElement ste = frame.getStackTraceElement();
            lr.setSourceMethodName(ste.getMethodName());
            lr.setSourceClassName(ste.getClassName());
        }
        _logger.log(lr);
    }
    
    /* ------------------------------------------------------------ */
    public void log(String formattedLog)
    {
        _logger.log(Level.INFO,formattedLog);
    }
}

