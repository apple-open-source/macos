/*
 * jBoss, the OpenSource EJB server
 *
 * Distributable under GPL license.
 * See terms of license at gnu.org.
 */

// $Id: JBossLogSink.java,v 1.3 2002/04/11 08:57:27 starksm Exp $

package org.jboss.jetty.log;

//------------------------------------------------------------------------------

import java.util.HashMap;
import org.jboss.logging.Logger;
import org.mortbay.util.Code;
import org.mortbay.util.Frame;
import org.mortbay.util.Log;
import org.mortbay.util.LogSink;

//------------------------------------------------------------------------------

/* ------------------------------------------------------------ */

/**
 * This class bidges the API between Jetty and Log4J.
 *
 * @author <a href="mailto:">Jules Gosnell</a>
 * @version $Id: JBossLogSink.java,v 1.3 2002/04/11 08:57:27 starksm Exp $
 * @since 1.0
 * @see org.mortbay.util.LogSink
 */
public class JBossLogSink
  implements LogSink
{
  Logger   _log;
  boolean  _started  = false;
  HashMap  _dispatch = new HashMap();

  interface MyLogger {void log(String s);}

  public
    JBossLogSink()
  {
      // populate the dispatch map...
      
      // don't necessarily use just  _log.debug() for Jetty debug:
      // Jetty guards it's debug output to it's log sinks, so it will
      // only get here if Jetty has debugging enabled. However,
      // enabling debugging in Jetty (eg via the CodeMBean) may not
      // result in log output because JBoss has it's log level set to
      // INFO.To reduce confusion, if Jetty debug is enabled, but not
      // on JBoss, we will still output it to the JBoss log sink as
      // category INFO.
      
      _dispatch.put(Log.DEBUG,
                    new MyLogger()
                    {
                        public void log(String s)
                        {
                            if (_log.isDebugEnabled())
                                _log.debug(s);
                            else
                                _log.info("DEBUG: "+s);
                        }
                    });
    _dispatch.put(Log.EVENT,  new MyLogger(){public void log(String s){_log.info(s);}});
    _dispatch.put(Log.WARN,   new MyLogger(){public void log(String s){_log.warn("WARNING: "+s);}});
    _dispatch.put(Log.ASSERT, new MyLogger(){public void log(String s){_log.error(s);}});
    _dispatch.put(Log.FAIL,   new MyLogger(){public void log(String s){_log.error(s);}});
  }

  // 'LifeCycle' interface
  public void
    initialize(Object log)
    throws InterruptedException
  {
    _log = (Logger) log;
  }

  public void
    start()
  {
    _started = true;
  }

  public void
    stop()
    throws InterruptedException
  {
    _started = false;
    //_log=null;
  }

  public void
    destroy()
  {
    _log = null;
  }

  public boolean
    isStarted()
  {
    return _started;
  }

  public boolean
    isDestroyed()
  {
    return (_log==null);
  }

  //----------------------------------------------------------------------
  // Options interface - NYI - probably never will be...
  //----------------------------------------------------------------------

  public void
    setOptions(String dateFormat,
	       String timezone,
	       boolean logTimeStamps,
	       boolean logLabels,
	       boolean logTags,
	       boolean logStackSize,
	       boolean logStackTrace,
	       boolean logOneLine)
  {
    // is it possible to translate these into JBoss logging options...?
  }

  public void
    setOptions(String logOptions)
  {
    //     setOptions((logOptions.indexOf(OPT_TIMESTAMP) >= 0),
    // 	       (logOptions.indexOf(OPT_LABEL) >= 0),
    // 	       (logOptions.indexOf(OPT_TAG) >= 0),
    // 	       (logOptions.indexOf(OPT_STACKSIZE) >= 0),
    // 	       (logOptions.indexOf(OPT_STACKTRACE) >= 0),
    // 	       (logOptions.indexOf(OPT_ONELINE) >= 0));
  }

  public String
    getOptions()
  {
    //     return
    //       (_logTimeStamps?"t":"")+
    //       (_logLabels?"L":"")+
    //       (_logTags?"T":"")+
    //       (_logStackSize?"s":"")+
    //       (_logStackTrace?"S":"")+
    //       (_logOneLine?"O":"");
    return "";
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
  public void
    log(String tag, Object msg, Frame frame, long time)
  {
    boolean debugging=Code.debug();

    MyLogger logger=(MyLogger)_dispatch.get(tag);
    if (logger!=null)
    {
      logger.log(msg+(debugging?", "+frame:""));
    }
    else
    {
      log(msg+" - "+tag+(debugging?", "+frame:""));
      _log.warn("JBossLogSink doesn't understand tag: '"+tag+"'");
    }
  }

  /* ------------------------------------------------------------ */
  /** Log a message.
   * The formatted log string is written to the log sink. The default
   * implementation writes the message to a PrintWriter.
   * @param formattedLog
   */
  public synchronized void
    log(String formattedLog)
  {
    _log.info(formattedLog);
  }
}
