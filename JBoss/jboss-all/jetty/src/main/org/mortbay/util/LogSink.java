// ========================================================================
// Copyright (c) 1997 MortBay Consulting, Sydney
// $Id: LogSink.java,v 1.15.2.4 2003/06/04 04:47:58 starksm Exp $
// ========================================================================

package org.mortbay.util;

import java.io.Serializable;

/* ------------------------------------------------------------ */
/** A Log sink.
 * This class represents both a concrete or abstract sink of
 * Log data.  The default implementation logs to a PrintWriter, but
 * derived implementations may log to files, syslog, or other
 * logging APIs.
 *
 * 
 * @version $Id: LogSink.java,v 1.15.2.4 2003/06/04 04:47:58 starksm Exp $
 * @author Greg Wilkins (gregw)
 */
public interface LogSink extends LifeCycle, Serializable
{
    /*-------------------------------------------------------------------*/
    /** Set the log options.
     *
     * @param options A string of characters as defined for the
     * LOG_OPTIONS system parameter.
     */
    public void setOptions(String options);
    
    /* ------------------------------------------------------------ */
    public String getOptions();
    
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
    public void log(String tag,
                    Object msg,
                    Frame frame,
                    long time);
    
    /* ------------------------------------------------------------ */
    /** Log a message.
     * The formatted log string is written to the log sink. The default
     * implementation writes the message to a PrintWriter.
     * @param formattedLog 
     */
    public void log(String formattedLog);

    
};








